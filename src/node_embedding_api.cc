#define NAPI_EXPERIMENTAL
#include "node_embedding_api.h"

#include "env-inl.h"
#include "js_native_api_v8.h"
#include "node_api_internals.h"
#include "util-inl.h"

#include <algorithm>
#include <climits>  // INT_MAX
#include <cmath>
#include <mutex>

#ifdef ASSERT
#undef ASSERT
#endif

#define EMBEDDED_PLATFORM(platform)                                            \
  return (platform) == nullptr                                                 \
             ? v8impl::EmbdeddedPlatform::ReportError(                         \
                   #platform " is null", __FILE__, __LINE__, napi_invalid_arg) \
             : reinterpret_cast<v8impl::EmbeddedPlatform*>(platform)

#define EMBEDDED_RUNTIME(runtime)                                              \
  return (runtime) == nullptr                                                  \
             ? v8impl::EmbdeddedPlatform::ReportError(                         \
                   #runtime " is null", __FILE__, __LINE__, napi_invalid_arg)  \
             : reinterpret_cast<v8impl::EmbeddedRuntime*>(runtime)

#define ARG_NOT_NULL(arg)                                                      \
  do {                                                                         \
    if ((arg) == nullptr) {                                                    \
      return v8impl::EmbeddedPlatform::ReportError(                            \
          #arg "is null", __FILE__, __LINE__, napi_invalid_arg);               \
    }                                                                          \
  } while (false)

#define ASSERT(expr)                                                           \
  do {                                                                         \
    if (!(expr)) {                                                             \
      return v8impl::EmbeddedPlatform::ReportError(                            \
          #expr " is not true", __FILE__, __LINE__, napi_generic_failure);     \
    }                                                                          \
  } while (false)

namespace node {
// Declare functions implemented in embed_helpers.cc
v8::Maybe<ExitCode> SpinEventLoopWithoutCleanup(Environment* env);
v8::Maybe<ExitCode> SpinEventLoopWithoutCleanup(
    Environment* env, const std::function<bool(void)>& shouldContinue);

}  // end of namespace node

namespace v8impl {
namespace {

// A helper class to convert std::vector<std::string> to an array of C strings.
// If the number of strings is less than kInplaceBufferSize, the strings are
// stored in the inplace_buffer_ array. Otherwise, the strings are stored in the
// allocated_buffer_ array.
// Ideally the class must be allocated on the stack.
// In any case it must not outlive the passed vector since it keeps only the
// string pointers returned by std::string::c_str() method.
class CStringArray {
  static constexpr size_t kInplaceBufferSize = 32;

 public:
  explicit CStringArray(const std::vector<std::string>& strings) noexcept
      : size_(strings.size()) {
    if (size_ <= inplace_buffer_.size()) {
      c_strs_ = inplace_buffer_.data();
    } else {
      allocated_buffer_ = std::make_unique<const char*[]>(size_);
      c_strs_ = allocated_buffer_.get();
    }
    for (size_t i = 0; i < size_; ++i) {
      c_strs_[i] = strings[i].c_str();
    }
  }

  CStringArray(const CStringArray&) = delete;
  CStringArray& operator=(const CStringArray&) = delete;

  const char** c_strs() const { return c_strs_; }
  size_t size() const { return size_; }

  const char** argv() const { return c_strs_; }
  int32_t argc() const { return static_cast<int>(size_); }

 private:
  const char** c_strs_{};
  size_t size_{};
  std::array<const char*, kInplaceBufferSize> inplace_buffer_;
  std::unique_ptr<const char*[]> allocated_buffer_;
};

class EmbeddedPlatform {
 public:
  static std::string FormatError(const char* format, ...) {
    va_list args1;
    va_start(args1, format);
    va_list args2;
    va_copy(args2, args1);  // Required for some compilers like GCC.
    std::string result(std::vsnprintf(nullptr, 0, format, args1), '\0');
    va_end(args1);
    std::vsnprintf(&result[0], result.size() + 1, format, args2);
    va_end(args2);
    return result;
  }

  static napi_status ReportError(const char* message,
                                 const char* filename,
                                 int32_t line,
                                 napi_status status = napi_generic_failure) {
    HandleError(FormatError("Error: %s at %s:%d\n", message, filename, line));
    return status;
  }

  static void HandleError(const std::string& message) {
    const char* message_cstr = message.c_str();
    if (custom_error_handler_ != nullptr) {
      custom_error_handler_(1, &message_cstr, 1, custom_error_handler_data_);
    } else {
      DefaultErrorHandler(1, &message_cstr, 1, nullptr);
    }
  }

  static void HandleError(int32_t exit_code,
                          const std::vector<std::string>& messages) {
    CStringArray message_arr(messages);
    if (custom_error_handler_ != nullptr) {
      custom_error_handler_(exit_code,
                            message_arr.c_strs(),
                            message_arr.size(),
                            custom_error_handler_data_);
    } else {
      DefaultErrorHandler(
          exit_code, message_arr.c_strs(), message_arr.size(), nullptr);
    }
  }

  static void DefaultErrorHandler(int32_t exit_code,
                                  const char* messages[],
                                  size_t size,
                                  void* /*handler_data*/) {
    if (exit_code != 0) {
      for (size_t i = 0; i < size; ++i) {
        fprintf(stderr, "%s", messages[i]);
      }
      fflush(stderr);
      exit(exit_code);
    } else {
      for (size_t i = 0; i < size; ++i) {
        fprintf(stdout, "%s", messages[i]);
      }
      fflush(stdout);
    }
  }

  explicit EmbeddedPlatform(int32_t api_version) noexcept
      : api_version_(api_version) {}

  napi_status Delete() {
    //   if (!v8impl::EmbeddedPlatform::UninitOncePerProcess())
    //     return napi_generic_failure;
    if (v8_is_initialized_ && !v8_is_uninitialized_) {
      v8::V8::Dispose();
      v8::V8::DisposePlatform();
      node::TearDownOncePerProcess();
      v8_is_uninitialized_ = false;
    }

    delete this;
    return napi_ok;
  }

  napi_status IsInitialized(bool* result) {
    ARG_NOT_NULL(resut);
    *result = is_initialized_;
    return napi_ok;
  }

  napi_status SetFlags(node_platform_flags flags) {
    ASSERT(!is_initialized_);
    flags_ = flags;
    optional_bits_.flags = true;
    return napi_ok;
  }

  napi_status SetArgs(int32_t argc, const char* argv[]) {
    ARG_NOT_NULL(argv);
    ASSERT(!is_initialized_);
    args_.assign(argv, argv + argc);
    optional_bits_.args = true;
    return napi_ok;
  }

  napi_status Initialize(bool* early_return) {
    ASSERT(!is_initialized_);

    is_initialized_ = true;

    // TODO: (vmoroz) default initialize args_.

    if (!optional_bits_.flags) {
      flags_ = node_platform_no_flags;
    }

    init_result_ = node::InitializeOncePerProcess(
        args_, GetProcessInitializationFlags(flags_));

    if (init_result_->exit_code() != 0 || !init_result_->errors().empty()) {
      HandleError(init_result_->exit_code(), init_result_->errors());
    }

    if (early_return != nullptr) {
      *early_return = init_result_->early_return();
    } else if (init_result_->early_return()) {
      exit(init_result_->exit_code());
    }

    if (init_result_->early_return()) {
      return init_result_->exit_code() == 0 ? napi_ok : napi_generic_failure;
    }

    int32_t thread_pool_size = static_cast<int32_t>(
        node::per_process::cli_options->v8_thread_pool_size);
    v8_platform_ = node::MultiIsolatePlatform::Create(thread_pool_size);
    v8::V8::InitializePlatform(v8_platform_.get());
    v8::V8::Initialize();

    v8_is_initialized_ = true;

    return napi_ok;
  }

  napi_status GetArgs(node_platform_get_args_callback get_args,
                      void* get_args_data) {
    ARG_NOT_NULL(get_args);
    ASSERT(is_initialized_);

    v8impl::CStringArray args(init_result_->args());
    get_args(args.argc(), args.argv(), get_args_data);

    return napi_ok;
  }

  napi_status GetExecArgs(node_platform_get_args_callback get_args,
                          void* get_args_data) {
    ARG_NOT_NULL(get_args);
    ASSERT(is_initialized_);

    v8impl::CStringArray args(init_result_->exec_args());
    get_args(args.argc(), args.argv(), get_args_data);

    return napi_ok;
  }

  napi_status CreateRuntime(node_runtime* result);

  // static bool InitOncePerProcess() noexcept {
  //   return !is_initialized_.test_and_set();
  // }

  // static bool UninitOncePerProcess() noexcept {
  //   return is_initialized_.test() && !is_uninitialized_.test_and_set();
  // }

  // static EmbeddedPlatform* GetInstance() noexcept { return platform_.get();
  // }

  // static EmbeddedPlatform* CreateInstance(
  //     std::shared_ptr<node::InitializationResult>&&
  //         platform_init_result) noexcept {
  //   platform_ =
  //       std::make_unique<EmbeddedPlatform>(std::move(platform_init_result));
  //   return platform_.get();
  // }

  // static void DeleteInstance() noexcept { platform_ = nullptr; }

  // static void set_v8_platform(
  //     std::unique_ptr<node::MultiIsolatePlatform>&& v8_platform) {
  //   platform_->v8_platform_ = std::move(v8_platform);
  // }

  // static node::MultiIsolatePlatform* get_v8_platform() noexcept {
  //   return platform_->v8_platform_.get();
  // }

  // const std::vector<std::string>& args() const {
  //   return platform_init_result_->args();
  // }

  // const std::vector<std::string>& exec_args() const {
  //   return platform_init_result_->exec_args();
  // }

  // explicit EmbeddedPlatform(int32_t api_version) noexcept
  //     : platform_init_result_(std::move(platform_init_result)) {}

  // EmbeddedPlatform(const EmbeddedPlatform&) = delete;
  // EmbeddedPlatform& operator=(const EmbeddedPlatform&) = delete;

  // napi_status NAPI_CDECL node_api_dispose_platform() {
  //   if (!v8impl::EmbeddedPlatform::UninitOncePerProcess())
  //     return napi_generic_failure;
  //   v8::V8::Dispose();
  //   v8::V8::DisposePlatform();
  //   node::TearDownOncePerProcess();
  //   v8impl::EmbeddedPlatform::DeleteInstance();
  //   return napi_ok;
  // }

  // napi_status NAPI_CDECL
  // node_api_create_env_options(node_api_env_options* result) {
  //   if (result == nullptr) return napi_invalid_arg;
  //   std::unique_ptr<v8impl::EmbeddedEnvironmentOptions> options =
  //       std::make_unique<v8impl::EmbeddedEnvironmentOptions>();
  //   // Transfer ownership of the options object to the caller.
  //   *result = reinterpret_cast<node_api_env_options>(options.release());
  //   return napi_ok;
  // }

  node::ProcessInitializationFlags::Flags GetProcessInitializationFlags(
      node_platform_flags flags) {
    uint32_t result = node::ProcessInitializationFlags::kNoFlags;
    if ((flags & node_platform_enable_stdio_inheritance) != 0) {
      result |= node::ProcessInitializationFlags::kEnableStdioInheritance;
    }
    if ((flags & node_platform_disable_node_options_env) != 0) {
      result |= node::ProcessInitializationFlags::kDisableNodeOptionsEnv;
    }
    if ((flags & node_platform_disable_cli_options) != 0) {
      result |= node::ProcessInitializationFlags::kDisableCLIOptions;
    }
    if ((flags & node_platform_no_icu) != 0) {
      result |= node::ProcessInitializationFlags::kNoICU;
    }
    if ((flags & node_platform_no_stdio_initialization) != 0) {
      result |= node::ProcessInitializationFlags::kNoStdioInitialization;
    }
    if ((flags & node_platform_no_default_signal_handling) != 0) {
      result |= node::ProcessInitializationFlags::kNoDefaultSignalHandling;
    }
    result |= node::ProcessInitializationFlags::kNoInitializeV8;
    result |= node::ProcessInitializationFlags::kNoInitializeNodeV8Platform;
    if ((flags & node_platform_no_init_openssl) != 0) {
      result |= node::ProcessInitializationFlags::kNoInitOpenSSL;
    }
    if ((flags & node_platform_no_parse_global_debug_variables) != 0) {
      result |= node::ProcessInitializationFlags::kNoParseGlobalDebugVariables;
    }
    if ((flags & node_platform_no_adjust_resource_limits) != 0) {
      result |= node::ProcessInitializationFlags::kNoAdjustResourceLimits;
    }
    if ((flags & node_platform_no_use_large_pages) != 0) {
      result |= node::ProcessInitializationFlags::kNoUseLargePages;
    }
    if ((flags & node_platform_no_print_help_or_version_output) != 0) {
      result |= node::ProcessInitializationFlags::kNoPrintHelpOrVersionOutput;
    }
    if ((flags & node_platform_generate_predictable_snapshot) != 0) {
      result |= node::ProcessInitializationFlags::kGeneratePredictableSnapshot;
    }
    return static_cast<node::ProcessInitializationFlags::Flags>(result);
  }

 private:
  int32_t api_version_{0};
  bool is_initialized_{false};
  bool v8_is_initialized_{false};
  bool v8_is_uninitialized_{false};
  node_platform_flags flags_;
  std::vector<std::string> args_;
  struct {
    bool flags : 1;
    bool args : 1;
  } optional_bits_;

  std::shared_ptr<node::InitializationResult> init_result_;
  std::unique_ptr<node::MultiIsolatePlatform> v8_platform_;

  // static std::atomic_flag is_initialized_;
  // static std::atomic_flag is_uninitialized_;
  static std::unique_ptr<EmbeddedPlatform> platform_;
  static node_platform_error_handler custom_error_handler_;
  static void* custom_error_handler_data_;
};

// std::atomic_flag EmbeddedPlatform::is_initialized_{};
// std::atomic_flag EmbeddedPlatform::is_uninitialized_{};
std::unique_ptr<EmbeddedPlatform> EmbeddedPlatform::platform_{};
node_platform_error_handler EmbeddedPlatform::custom_error_handler_{};
void* EmbeddedPlatform::custom_error_handler_data_{};

struct IsolateLocker {
  IsolateLocker(node::CommonEnvironmentSetup* env_setup)
      : v8_locker_(env_setup->isolate()),
        isolate_scope_(env_setup->isolate()),
        handle_scope_(env_setup->isolate()),
        context_scope_(env_setup->context()) {}

  bool IsLocked() const {
    return v8::Locker::IsLocked(v8::Isolate::GetCurrent());
  }

  void IncrementLockCount() { ++lock_count_; }

  bool DecrementLockCount() {
    --lock_count_;
    return lock_count_ == 0;
  }

 private:
  int32_t lock_count_ = 1;
  v8::Locker v8_locker_;
  v8::Isolate::Scope isolate_scope_;
  v8::HandleScope handle_scope_;
  v8::Context::Scope context_scope_;
};

class EmbeddedRuntime {
 public:
  explicit EmbeddedRuntime(
      std::unique_ptr<EmbeddedEnvironmentOptions>&& env_options,
      std::unique_ptr<node::CommonEnvironmentSetup>&& env_setup,
      v8::Local<v8::Context> context,
      const std::string& module_filename,
      int32_t module_api_version)
      : node_napi_env__(context, module_filename, module_api_version),
        env_options_(std::move(env_options)),
        env_setup_(std::move(env_setup)) {
    env_options_->is_frozen_ = true;

    std::scoped_lock<std::mutex> lock(shared_mutex_);
    node_env_to_node_api_env_.emplace(env_setup_->env(), this);
  }

  napi_status Delete() {}

  napi_status IsInitialized(bool* result) { ARG_NOT_NULL(result); }

  napi_status SetFlags(node_platform_flags flags) {}

  napi_status SetArgs(int32_t argc, const char* argv[]) {}

  napi_status SetExecArgs(int32_t argc, const char* argv[]) {}

  napi_status SetPreloadCallback(node_runtime_preload_callback preload_cb,
                                 void* preload_cb_data) {}

  napi_status SetSnapshotBlob(const uint8_t* snapshot, size_t size) {}

  napi_status OnCreateSnapshotBlob(
      node_runtime_store_blob_callback store_blob_cb,
      void* store_blob_cb_data,
      node_runtime_snapshot_flags snapshot_flags) {}

  napi_status Initialize() {}

  napi_status RunEventLoop() {}

  napi_status RunEventLoopWhile(node_runtime_event_loop_predicate predicate,
                                void* predicate_data,
                                bool is_thread_blocking,
                                bool* has_more_work) {}

  napi_status AwaitPromise(napi_value promise,
                           napi_value* result,
                           bool* has_more_work) {}

  napi_status SetNodeApiVersion(int32_t node_api_version) {}

  napi_status GetNodeApiEnv(napi_env* env) {}

  napi_status OpenScope() {}

  napi_status CloseScope() {}

  static node_napi_env GetOrCreateNodeApiEnv(node::Environment* node_env) {
    std::scoped_lock<std::mutex> lock(shared_mutex_);
    auto it = node_env_to_node_api_env_.find(node_env);
    if (it != node_env_to_node_api_env_.end()) return it->second;
    // TODO: (vmoroz) propagate API version from the root environment.
    node_napi_env env = new node_napi_env__(
        node_env->context(), "<worker_thread>", NAPI_VERSION_EXPERIMENTAL);
    node_env->AddCleanupHook(
        [](void* arg) { static_cast<node_napi_env>(arg)->Unref(); }, env);
    node_env_to_node_api_env_.try_emplace(node_env, env);
    return env;
  }

  static EmbeddedEnvironment* FromNapiEnv(napi_env env) {
    return static_cast<EmbeddedEnvironment*>(env);
  }

  node::CommonEnvironmentSetup* env_setup() { return env_setup_.get(); }

  std::unique_ptr<node::CommonEnvironmentSetup> ResetEnvSetup() {
    return std::move(env_setup_);
  }

  napi_status OpenScope() {
    if (isolate_locker_.has_value()) {
      if (!isolate_locker_->IsLocked()) return napi_generic_failure;
      isolate_locker_->IncrementLockCount();
    } else {
      isolate_locker_.emplace(env_setup_.get());
    }
    return napi_ok;
  }

  napi_status CloseScope() {
    if (!isolate_locker_.has_value()) return napi_generic_failure;
    if (!isolate_locker_->IsLocked()) return napi_generic_failure;
    if (isolate_locker_->DecrementLockCount()) isolate_locker_.reset();
    return napi_ok;
  }

  bool IsScopeOpened() const { return isolate_locker_.has_value(); }

  const EmbeddedEnvironmentOptions& options() const { return *env_options_; }

  const node::EmbedderSnapshotData::Pointer& snapshot() const {
    return env_options_->snapshot_;
  }

  const std::function<void(const node::EmbedderSnapshotData*)>&
  create_snapshot() {
    return env_options_->create_snapshot_;
  }

  napi_status NAPI_CDECL node_api_env_options_set_flags(
      node_api_env_options options, node_api_env_flags flags) {
    if (options == nullptr) return napi_invalid_arg;

    v8impl::EmbeddedEnvironmentOptions* env_options =
        reinterpret_cast<v8impl::EmbeddedEnvironmentOptions*>(options);
    if (env_options->is_frozen_) return napi_generic_failure;

    env_options->flags_ = flags;
    return napi_ok;
  }

  napi_status NAPI_CDECL node_api_env_options_set_args(
      node_api_env_options options, size_t argc, const char* argv[]) {
    if (options == nullptr) return napi_invalid_arg;
    if (argv == nullptr) return napi_invalid_arg;

    v8impl::EmbeddedEnvironmentOptions* env_options =
        reinterpret_cast<v8impl::EmbeddedEnvironmentOptions*>(options);
    if (env_options->is_frozen_) return napi_generic_failure;

    env_options->args_.assign(argv, argv + argc);
    return napi_ok;
  }

  napi_status NAPI_CDECL node_api_env_options_set_exec_args(
      node_api_env_options options, size_t argc, const char* argv[]) {
    if (options == nullptr) return napi_invalid_arg;
    if (argv == nullptr) return napi_invalid_arg;

    v8impl::EmbeddedEnvironmentOptions* env_options =
        reinterpret_cast<v8impl::EmbeddedEnvironmentOptions*>(options);
    if (env_options->is_frozen_) return napi_generic_failure;

    env_options->exec_args_.assign(argv, argv + argc);
    return napi_ok;
  }

  napi_status NAPI_CDECL node_api_env_options_set_preload_callback(
      node_api_env_options options,
      node_api_preload_callback preload_cb,
      void* cb_data) {
    if (options == nullptr) return napi_invalid_arg;

    v8impl::EmbeddedEnvironmentOptions* env_options =
        reinterpret_cast<v8impl::EmbeddedEnvironmentOptions*>(options);
    if (env_options->is_frozen_) return napi_generic_failure;

    // TODO: (vmoroz) use CallIntoModule to handle errors.
    if (preload_cb != nullptr) {
      env_options->preload_cb_ = node::EmbedderPreloadCallback(
          [preload_cb, cb_data](node::Environment* node_env,
                                v8::Local<v8::Value> process,
                                v8::Local<v8::Value> require) {
            node_napi_env env =
                v8impl::EmbeddedEnvironment::GetOrCreateNodeApiEnv(node_env);
            napi_value process_value = v8impl::JsValueFromV8LocalValue(process);
            napi_value require_value = v8impl::JsValueFromV8LocalValue(require);
            preload_cb(env, process_value, require_value, cb_data);
          });
    } else {
      env_options->preload_cb_ = {};
    }

    return napi_ok;
  }

  napi_status NAPI_CDECL
  node_api_env_options_use_snapshot(node_api_env_options options,
                                    const char* snapshot_data,
                                    size_t snapshot_size) {
    if (options == nullptr) return napi_invalid_arg;
    if (snapshot_data == nullptr) return napi_invalid_arg;

    v8impl::EmbeddedEnvironmentOptions* env_options =
        reinterpret_cast<v8impl::EmbeddedEnvironmentOptions*>(options);
    if (env_options->is_frozen_) return napi_generic_failure;

    env_options->snapshot_ = node::EmbedderSnapshotData::FromBlob(
        std::string_view(snapshot_data, snapshot_size));
    return napi_ok;
  }

  napi_status NAPI_CDECL node_api_env_options_create_snapshot(
      node_api_env_options options,
      node_api_store_blob_callback store_blob_cb,
      void* cb_data,
      node_api_snapshot_flags snapshot_flags) {
    if (options == nullptr) return napi_invalid_arg;
    if (store_blob_cb == nullptr) return napi_invalid_arg;

    v8impl::EmbeddedEnvironmentOptions* env_options =
        reinterpret_cast<v8impl::EmbeddedEnvironmentOptions*>(options);
    if (env_options->is_frozen_) return napi_generic_failure;

    env_options->create_snapshot_ =
        [store_blob_cb, cb_data](const node::EmbedderSnapshotData* snapshot) {
          std::vector<char> blob = snapshot->ToBlob();
          store_blob_cb(cb_data,
                        reinterpret_cast<const uint8_t*>(blob.data()),
                        blob.size());
        };

    if ((snapshot_flags & node_api_snapshot_no_code_cache) != 0) {
      env_options->snapshot_config_.flags = static_cast<node::SnapshotFlags>(
          static_cast<uint32_t>(env_options->snapshot_config_.flags) |
          static_cast<uint32_t>(node::SnapshotFlags::kWithoutCodeCache));
    }

    return napi_ok;
  }

  napi_status NAPI_CDECL
  node_api_create_env(node_api_env_options options,
                      node_api_error_message_handler error_handler,
                      void* error_handler_data,
                      const char* main_script,
                      int32_t api_version,
                      napi_env* result) {
    if (options == nullptr) return napi_invalid_arg;
    if (result == nullptr) return napi_invalid_arg;
    if (api_version == 0) api_version = NODE_API_DEFAULT_MODULE_API_VERSION;

    std::unique_ptr<v8impl::EmbeddedEnvironmentOptions> env_options{
        reinterpret_cast<v8impl::EmbeddedEnvironmentOptions*>(options)};
    std::vector<std::string> errors;

    std::unique_ptr<node::CommonEnvironmentSetup> env_setup;
    node::MultiIsolatePlatform* platform =
        v8impl::EmbeddedPlatform::GetInstance()->get_v8_platform();
    node::EnvironmentFlags::Flags flags =
        v8impl::GetEnvironmentFlags(env_options->flags_);
    if (env_options->snapshot_) {
      env_setup = node::CommonEnvironmentSetup::CreateFromSnapshot(
          platform,
          &errors,
          env_options->snapshot_.get(),
          env_options->args_,
          env_options->exec_args_,
          flags);
    } else if (env_options->create_snapshot_) {
      env_setup = node::CommonEnvironmentSetup::CreateForSnapshotting(
          platform,
          &errors,
          env_options->args_,
          env_options->exec_args_,
          env_options->snapshot_config_);
    } else {
      env_setup = node::CommonEnvironmentSetup::Create(platform,
                                                       &errors,
                                                       env_options->args_,
                                                       env_options->exec_args_,
                                                       flags);
    }

    if (error_handler != nullptr && !errors.empty()) {
      v8impl::CStringArray error_arr(errors);
      error_handler(error_handler_data, error_arr.c_strs(), error_arr.size());
    }

    if (env_setup == nullptr) {
      return napi_generic_failure;
    }

    std::string filename =
        env_options->args_.size() > 1 ? env_options->args_[1] : "<internal>";
    node::CommonEnvironmentSetup* env_setup_ptr = env_setup.get();

    v8impl::IsolateLocker isolate_locker(env_setup_ptr);
    std::unique_ptr<v8impl::EmbeddedEnvironment> embedded_env =
        std::make_unique<v8impl::EmbeddedEnvironment>(std::move(env_options),
                                                      std::move(env_setup),
                                                      env_setup_ptr->context(),
                                                      filename,
                                                      api_version);
    embedded_env->node_env()->AddCleanupHook(
        [](void* arg) { static_cast<napi_env>(arg)->Unref(); },
        static_cast<void*>(embedded_env.get()));
    *result = embedded_env.get();

    node::Environment* node_env = env_setup_ptr->env();

    // TODO(vmoroz): If we return an error here, then it is not clear if the
    // environment must be deleted after that or not.
    v8::MaybeLocal<v8::Value> ret =
        embedded_env->snapshot()
            ? node::LoadEnvironment(node_env, node::StartExecutionCallback{})
            : node::LoadEnvironment(node_env,
                                    std::string_view(main_script),
                                    embedded_env->options().preload_cb_);

    embedded_env.release();

    if (ret.IsEmpty()) return napi_pending_exception;

    return napi_ok;
  }

  napi_status NAPI_CDECL node_api_delete_env(napi_env env, int* exit_code) {
    CHECK_ENV(env);
    v8impl::EmbeddedEnvironment* embedded_env =
        v8impl::EmbeddedEnvironment::FromNapiEnv(env);

    if (embedded_env->IsScopeOpened()) return napi_generic_failure;

    {
      v8impl::IsolateLocker isolate_locker(embedded_env->env_setup());

      int ret = node::SpinEventLoop(embedded_env->node_env()).FromMaybe(1);
      if (exit_code != nullptr) *exit_code = ret;
    }

    std::unique_ptr<node::CommonEnvironmentSetup> env_setup =
        embedded_env->ResetEnvSetup();

    if (embedded_env->create_snapshot()) {
      node::EmbedderSnapshotData::Pointer snapshot =
          env_setup->CreateSnapshot();
      assert(snapshot);
      embedded_env->create_snapshot()(snapshot.get());
    }

    node::Stop(embedded_env->node_env());

    return napi_ok;
  }

  napi_status NAPI_CDECL node_api_open_env_scope(napi_env env) {
    CHECK_ENV(env);
    v8impl::EmbeddedEnvironment* embedded_env =
        v8impl::EmbeddedEnvironment::FromNapiEnv(env);

    return embedded_env->OpenScope();
  }

  napi_status NAPI_CDECL node_api_close_env_scope(napi_env env) {
    CHECK_ENV(env);
    v8impl::EmbeddedEnvironment* embedded_env =
        v8impl::EmbeddedEnvironment::FromNapiEnv(env);

    return embedded_env->CloseScope();
  }

  napi_status NAPI_CDECL node_api_run_env(napi_env env) {
    CHECK_ENV(env);
    v8impl::EmbeddedEnvironment* embedded_env =
        v8impl::EmbeddedEnvironment::FromNapiEnv(env);

    if (node::SpinEventLoopWithoutCleanup(embedded_env->node_env())
            .IsNothing()) {
      return napi_closing;
    }

    return napi_ok;
  }

  napi_status NAPI_CDECL
  node_api_run_env_while(napi_env env,
                         node_api_run_predicate predicate,
                         void* predicate_data,
                         bool* has_more_work) {
    CHECK_ENV(env);
    CHECK_ARG(env, predicate);
    v8impl::EmbeddedEnvironment* embedded_env =
        v8impl::EmbeddedEnvironment::FromNapiEnv(env);

    if (predicate(predicate_data)) {
      if (node::SpinEventLoopWithoutCleanup(embedded_env->node_env(),
                                            [predicate, predicate_data]() {
                                              return predicate(predicate_data);
                                            })
              .IsNothing()) {
        return napi_closing;
      }
    }

    if (has_more_work != nullptr) {
      *has_more_work = uv_loop_alive(embedded_env->node_env()->event_loop());
    }

    return napi_ok;
  }

  napi_status NAPI_CDECL node_api_await_promise(napi_env env,
                                                napi_value promise,
                                                napi_value* result,
                                                bool* has_more_work) {
    NAPI_PREAMBLE(env);
    CHECK_ARG(env, result);

    v8impl::EmbeddedEnvironment* embedded_env =
        v8impl::EmbeddedEnvironment::FromNapiEnv(env);

    v8::EscapableHandleScope scope(env->isolate);

    v8::Local<v8::Value> promise_value =
        v8impl::V8LocalValueFromJsValue(promise);
    if (promise_value.IsEmpty() || !promise_value->IsPromise())
      return napi_invalid_arg;
    v8::Local<v8::Promise> promise_object = promise_value.As<v8::Promise>();

    v8::Local<v8::Value> rejected = v8::Boolean::New(env->isolate, false);
    v8::Local<v8::Function> err_handler =
        v8::Function::New(
            env->context(),
            [](const v8::FunctionCallbackInfo<v8::Value>& info) { return; },
            rejected)
            .ToLocalChecked();

    if (promise_object->Catch(env->context(), err_handler).IsEmpty())
      return napi_pending_exception;

    if (node::SpinEventLoopWithoutCleanup(
            embedded_env->node_env(),
            [&promise_object]() {
              return promise_object->State() ==
                     v8::Promise::PromiseState::kPending;
            })
            .IsNothing())
      return napi_closing;

    *result =
        v8impl::JsValueFromV8LocalValue(scope.Escape(promise_object->Result()));

    if (has_more_work != nullptr) {
      *has_more_work = uv_loop_alive(embedded_env->node_env()->event_loop());
    }

    if (promise_object->State() == v8::Promise::PromiseState::kRejected)
      return napi_pending_exception;

    return napi_ok;
  }

  node::EnvironmentFlags::Flags GetEnvironmentFlags(node_runtime_flags flags) {
    uint64_t result = node::EnvironmentFlags::kNoFlags;
    if ((flags & node_runtime_default_flags) != 0) {
      result |= node::EnvironmentFlags::kDefaultFlags;
    }
    if ((flags & node_runtime_owns_process_state) != 0) {
      result |= node::EnvironmentFlags::kOwnsProcessState;
    }
    if ((flags & node_runtime_owns_inspector) != 0) {
      result |= node::EnvironmentFlags::kOwnsInspector;
    }
    if ((flags & node_runtime_no_register_esm_loader) != 0) {
      result |= node::EnvironmentFlags::kNoRegisterESMLoader;
    }
    if ((flags & node_runtime_track_unmanaged_fds) != 0) {
      result |= node::EnvironmentFlags::kTrackUnmanagedFds;
    }
    if ((flags & node_runtime_hide_console_windows) != 0) {
      result |= node::EnvironmentFlags::kHideConsoleWindows;
    }
    if ((flags & node_runtime_no_native_addons) != 0) {
      result |= node::EnvironmentFlags::kNoNativeAddons;
    }
    if ((flags & node_runtime_no_global_search_paths) != 0) {
      result |= node::EnvironmentFlags::kNoGlobalSearchPaths;
    }
    if ((flags & node_runtime_no_browser_globals) != 0) {
      result |= node::EnvironmentFlags::kNoBrowserGlobals;
    }
    if ((flags & node_runtime_no_create_inspector) != 0) {
      result |= node::EnvironmentFlags::kNoCreateInspector;
    }
    if ((flags & node_runtime_no_start_debug_signal_handler) != 0) {
      result |= node::EnvironmentFlags::kNoStartDebugSignalHandler;
    }
    if ((flags & node_runtime_no_wait_for_inspector_frontend) != 0) {
      result |= node::EnvironmentFlags::kNoWaitForInspectorFrontend;
    }
    return static_cast<node::EnvironmentFlags::Flags>(result);
  }

  // struct EmbeddedEnvironmentOptions {
  //   explicit EmbeddedEnvironmentOptions() noexcept
  //       : args_(EmbeddedPlatform::GetInstance()->args()),
  //         exec_args_(EmbeddedPlatform::GetInstance()->exec_args()) {}

  //   EmbeddedEnvironmentOptions(const EmbeddedEnvironmentOptions&) = delete;
  //   EmbeddedEnvironmentOptions& operator=(const
  //   EmbeddedEnvironmentOptions&)
  //   =
  //       delete;

  //   bool is_frozen_{false};
  //   node_api_env_flags flags_{node_api_env_default_flags};
  //   std::vector<std::string> args_;
  //   std::vector<std::string> exec_args_;
  //   node::EmbedderPreloadCallback preload_cb_{};
  //   node::EmbedderSnapshotData::Pointer snapshot_;
  //   std::function<void(const node::EmbedderSnapshotData*)>
  //   create_snapshot_; node::SnapshotConfig snapshot_config_{};
  // };

 private:
  std::unique_ptr<EmbeddedEnvironmentOptions> env_options_;
  std::unique_ptr<node::CommonEnvironmentSetup> env_setup_;
  std::optional<IsolateLocker> isolate_locker_;

  static std::mutex shared_mutex_;
  static std::unordered_map<node::Environment*, node_napi_env>
      node_env_to_node_api_env_;
};

std::mutex EmbeddedEnvironment::shared_mutex_{};
std::unordered_map<node::Environment*, node_napi_env>
    EmbeddedEnvironment::node_env_to_node_api_env_{};

napi_status EmbeddedPlatform::CreateRuntime(node_runtime* result) {
  ARG_NOT_NULL(result);
  ASSERT(is_initialized_);
  ASSERT(v8_is_initialized_);

  std::unique_ptr<EmbeddedRuntime> runtime =
      std::make_unique<EmbeddedRuntime>(this);

  *result = reinterpret_cast<node_runtime>(runtime.release());

  return napi_ok;
}

}  // end of anonymous namespace
}  // end of namespace v8impl

napi_status NAPI_CDECL node_platform_on_error(
    node_platform_error_handler error_handler, void* error_handler_data) {
  return v8impl::EmbeddedPlatform::SetErrorHandler(error_handler,
                                                   error_handler_data);
}

napi_status NAPI_CDECL node_create_platform(int32_t api_version,
                                            node_platform* result) {
  ARG_NOT_NULL(result);
  *result = reinterpret_cast<node_platform>(
      new v8impl::EmbeddedPlatform(api_version));
  return napi_ok;
}

napi_status NAPI_CDECL node_delete_platform(node_platform platform) {
  return EMBEDDED_PLATFORM(platform)->Delete();
}

napi_status NAPI_CDECL node_platform_is_initialized(node_platform platform,
                                                    bool* result) {
  return EMBEDDED_PLATFORM(platform)->IsInitialized(result);
}

napi_status NAPI_CDECL node_platform_set_flags(node_platform platform,
                                               node_platform_flags flags) {
  return EMBEDDED_PLATFORM(platform)->SetFlags(flags);
}

napi_status NAPI_CDECL node_platform_set_args(node_platform platform,
                                              int32_t argc,
                                              const char* argv[]) {
  return EMBEDDED_PLATFORM(platform)->SetArgs(argc, argv);
}

napi_status NAPI_CDECL node_platform_initialize(node_platform platform,
                                                bool* early_return) {
  return EMBEDDED_PLATFORM(platform)->Initialize(early_return);
}

napi_status NAPI_CDECL
node_platform_get_args(node_platform platform,
                       node_platform_get_args_callback get_args,
                       void* get_args_data) {
  return EMBEDDED_PLATFORM(platform)->GetArgs(get_args, get_args_data);
}

napi_status NAPI_CDECL
node_platform_get_exec_args(node_platform platform,
                            node_platform_get_args_callback get_args,
                            void* get_args_data) {
  return EMBEDDED_PLATFORM(platform)->GetExecArgs(get_args, get_args_data);
}

napi_status NAPI_CDECL node_create_runtime(node_platform platform,
                                           node_runtime* result) {
  return EMBEDDED_PLATFORM(platform)->CreateRuntime(result);
}

napi_status NAPI_CDECL node_delete_runtime(node_runtime runtime) {
  return EMBEDDED_RUNTIME(runtime)->Delete();
}

napi_status NAPI_CDECL node_runtime_is_initialized(node_runtime runtime,
                                                   bool* result) {
  return EMBEDDED_RUNTIME(runtime)->IsInitialized(result);
}

napi_status NAPI_CDECL node_runtime_set_flags(node_runtime runtime,
                                              node_platform_flags flags) {
  return EMBEDDED_RUNTIME(runtime)->SetFlags(flags);
}

napi_status NAPI_CDECL node_runtime_set_args(node_runtime runtime,
                                             int32_t argc,
                                             const char* argv[]) {
  return EMBEDDED_RUNTIME(runtime)->SetArgs(argc, argv);
}

napi_status NAPI_CDECL node_runtime_set_exec_args(node_runtime runtime,
                                                  int32_t argc,
                                                  const char* argv[]) {
  return EMBEDDED_RUNTIME(runtime)->SetExecArgs(argc, argv);
}

napi_status NAPI_CDECL
node_runtime_on_preload(node_runtime runtime,
                        node_runtime_preload_callback preload_cb,
                        void* preload_cb_data) {
  return EMBEDDED_RUNTIME(runtime)->SetPreloadCallback(preload_cb,
                                                       preload_cb_data);
}

napi_status NAPI_CDECL node_runtime_use_snapshot(node_runtime runtime,
                                                 const uint8_t* snapshot,
                                                 size_t size) {
  return EMBEDDED_RUNTIME(runtime)->SetSnapshotBlob(snapshot, size);
}

napi_status NAPI_CDECL
node_runtime_on_create_snapshot(node_runtime runtime,
                                node_runtime_store_blob_callback store_blob_cb,
                                void* store_blob_cb_data,
                                node_runtime_snapshot_flags snapshot_flags) {
  return EMBEDDED_RUNTIME(runtime)->OnCreateSnapshotBlob(
      store_blob_cb, store_blob_cb_data, snapshot_flags);
}

napi_status NAPI_CDECL node_runtime_initialize(node_runtime runtime) {
  return EMBEDDED_RUNTIME(runtime)->Initialize();
}

napi_status NAPI_CDECL node_runtime_run_event_loop(node_runtime runtime) {
  return EMBEDDED_RUNTIME(runtime)->RunEventLoop();
}

napi_status NAPI_CDECL
node_runtime_run_event_loop_while(node_runtime runtime,
                                  node_runtime_event_loop_predicate predicate,
                                  void* predicate_data,
                                  bool is_thread_blocking,
                                  bool* has_more_work) {
  return EMBEDDED_RUNTIME(runtime)->RunEventLoopWhile(
      predicate, predicate_data, is_thread_blocking, has_more_work);
}

napi_status NAPI_CDECL node_runtime_await_promise(node_runtime runtime,
                                                  napi_value promise,
                                                  napi_value* result,
                                                  bool* has_more_work) {
  return EMBEDDED_RUNTIME(runtime)->AwaitPromise(
      promise, result, has_more_work);
}

napi_status NAPI_CDECL node_runtime_set_node_api_version(
    node_runtime runtime, int32_t node_api_version) {
  return EMBEDDED_RUNTIME(runtime)->SetNodeApiVersion(node_api_version);
}

napi_status NAPI_CDECL node_runtime_get_node_api_env(node_runtime runtime,
                                                     napi_env* env) {
  return EMBEDDED_RUNTIME(runtime)->GetNodeApiEnv(env);
}

napi_status NAPI_CDECL node_runtime_open_scope(node_runtime runtime) {
  return EMBEDDED_RUNTIME(runtime)->OpenScope();
}

napi_status NAPI_CDECL node_runtime_close_scope(node_runtime runtime) {
  return EMBEDDED_RUNTIME(runtime)->CloseScope();
}
