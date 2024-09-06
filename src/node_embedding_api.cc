#define NAPI_EXPERIMENTAL
#include "node_embedding_api.h"

#include "env-inl.h"
#include "js_native_api_v8.h"
#include "node_api_internals.h"
#include "util-inl.h"

#include <mutex>

#define EMBEDDED_PLATFORM(platform)                                            \
  ((platform) == nullptr)                                                      \
      ? v8impl::EmbeddedPlatform::ReportError(                                 \
            #platform " is null", __FILE__, __LINE__, napi_invalid_arg)        \
      : reinterpret_cast<v8impl::EmbeddedPlatform*>(platform)

#define EMBEDDED_RUNTIME(runtime)                                              \
  (runtime) == nullptr                                                         \
      ? v8impl::EmbeddedPlatform::ReportError(                                 \
            #runtime " is null", __FILE__, __LINE__, napi_invalid_arg)         \
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

  // TODO: (vmoroz) implement this.
  static napi_status SetErrorHandler(node_embedding_error_handler error_handler,
                                     void* error_handler_data) {
    return napi_ok;
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
    ARG_NOT_NULL(result);
    *result = is_initialized_;
    return napi_ok;
  }

  napi_status SetFlags(node_embedding_platform_flags flags) {
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
      flags_ = node_embedding_platform_no_flags;
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

  napi_status GetArgs(node_embedding_get_args_callback get_args,
                      void* get_args_data) {
    ARG_NOT_NULL(get_args);
    ASSERT(is_initialized_);

    v8impl::CStringArray args(init_result_->args());
    get_args(args.argc(), args.argv(), get_args_data);

    return napi_ok;
  }

  napi_status GetExecArgs(node_embedding_get_args_callback get_args,
                          void* get_args_data) {
    ARG_NOT_NULL(get_args);
    ASSERT(is_initialized_);

    v8impl::CStringArray args(init_result_->exec_args());
    get_args(args.argc(), args.argv(), get_args_data);

    return napi_ok;
  }

  napi_status CreateRuntime(node_embedding_runtime* result);

  // TODO: (vmoroz) should we implement these once-per-process guards?
  // static bool InitOncePerProcess() noexcept {
  //   return !is_initialized_.test_and_set();
  // }

  // static bool UninitOncePerProcess() noexcept {
  //   return is_initialized_.test() && !is_uninitialized_.test_and_set();
  // }

  node::ProcessInitializationFlags::Flags GetProcessInitializationFlags(
      node_embedding_platform_flags flags) {
    uint32_t result = node::ProcessInitializationFlags::kNoFlags;
    if ((flags & node_embedding_platform_enable_stdio_inheritance) != 0) {
      result |= node::ProcessInitializationFlags::kEnableStdioInheritance;
    }
    if ((flags & node_embedding_platform_disable_node_options_env) != 0) {
      result |= node::ProcessInitializationFlags::kDisableNodeOptionsEnv;
    }
    if ((flags & node_embedding_platform_disable_cli_options) != 0) {
      result |= node::ProcessInitializationFlags::kDisableCLIOptions;
    }
    if ((flags & node_embedding_platform_no_icu) != 0) {
      result |= node::ProcessInitializationFlags::kNoICU;
    }
    if ((flags & node_embedding_platform_no_stdio_initialization) != 0) {
      result |= node::ProcessInitializationFlags::kNoStdioInitialization;
    }
    if ((flags & node_embedding_platform_no_default_signal_handling) != 0) {
      result |= node::ProcessInitializationFlags::kNoDefaultSignalHandling;
    }
    result |= node::ProcessInitializationFlags::kNoInitializeV8;
    result |= node::ProcessInitializationFlags::kNoInitializeNodeV8Platform;
    if ((flags & node_embedding_platform_no_init_openssl) != 0) {
      result |= node::ProcessInitializationFlags::kNoInitOpenSSL;
    }
    if ((flags & node_embedding_platform_no_parse_global_debug_variables) !=
        0) {
      result |= node::ProcessInitializationFlags::kNoParseGlobalDebugVariables;
    }
    if ((flags & node_embedding_platform_no_adjust_resource_limits) != 0) {
      result |= node::ProcessInitializationFlags::kNoAdjustResourceLimits;
    }
    if ((flags & node_embedding_platform_no_use_large_pages) != 0) {
      result |= node::ProcessInitializationFlags::kNoUseLargePages;
    }
    if ((flags & node_embedding_platform_no_print_help_or_version_output) !=
        0) {
      result |= node::ProcessInitializationFlags::kNoPrintHelpOrVersionOutput;
    }
    if ((flags & node_embedding_platform_generate_predictable_snapshot) != 0) {
      result |= node::ProcessInitializationFlags::kGeneratePredictableSnapshot;
    }
    return static_cast<node::ProcessInitializationFlags::Flags>(result);
  }

  EmbeddedPlatform(const EmbeddedPlatform&) = delete;
  EmbeddedPlatform& operator=(const EmbeddedPlatform&) = delete;

  node::MultiIsolatePlatform* get_v8_platform() { return v8_platform_.get(); }

 private:
  int32_t api_version_{0};
  bool is_initialized_{false};
  bool v8_is_initialized_{false};
  bool v8_is_uninitialized_{false};
  node_embedding_platform_flags flags_;
  std::vector<std::string> args_;
  struct {
    bool flags : 1;
    bool args : 1;
  } optional_bits_{};

  std::shared_ptr<node::InitializationResult> init_result_;
  std::unique_ptr<node::MultiIsolatePlatform> v8_platform_;

  // static std::atomic_flag is_initialized_;
  // static std::atomic_flag is_uninitialized_;
  // static std::unique_ptr<EmbeddedPlatform> platform_;
  static node_embedding_error_handler custom_error_handler_;
  static void* custom_error_handler_data_;
};

// std::atomic_flag EmbeddedPlatform::is_initialized_{};
// std::atomic_flag EmbeddedPlatform::is_uninitialized_{};
// std::unique_ptr<EmbeddedPlatform> EmbeddedPlatform::platform_{};
node_embedding_error_handler EmbeddedPlatform::custom_error_handler_{};
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
  explicit EmbeddedRuntime(EmbeddedPlatform* platform) : platform_(platform) {
    //    std::unique_ptr<node::CommonEnvironmentSetup>&& env_setup,
    //    v8::Local<v8::Context> context,
    //    const std::string& module_filename,
    //    int32_t module_api_version)
    //    : node_napi_env__(context, module_filename, module_api_version),
    //      env_setup_(std::move(env_setup)) {

    std::scoped_lock<std::mutex> lock(shared_mutex_);
    // TODO: (vmoroz) implement this.
    // node_env_to_node_api_env_.emplace(env_setup_->env(), this);
  }

  napi_status Delete() {
    ASSERT(!IsScopeOpened());

    {
      v8impl::IsolateLocker isolate_locker(env_setup_.get());

      int ret = node::SpinEventLoop(env_setup_->env()).FromMaybe(1);
      // TODO: (vmoroz) handle errors.
      // if (exit_code != nullptr) *exit_code = ret;
    }

    std::unique_ptr<node::CommonEnvironmentSetup> env_setup =
        std::move(env_setup_);

    // TODO: (vmoroz) implement.
    // if (embedded_env->create_snapshot()) {
    //  node::EmbedderSnapshotData::Pointer snapshot =
    //      env_setup->CreateSnapshot();
    //  assert(snapshot);
    //  embedded_env->create_snapshot()(snapshot.get());
    //}

    node::Stop(env_setup->env());

    return napi_ok;
  }

  napi_status IsInitialized(bool* result) {
    ARG_NOT_NULL(result);
    *result = is_initialized_;
    return napi_ok;
  }

  napi_status SetFlags(node_embedding_runtime_flags flags) {
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

  napi_status SetExecArgs(int32_t argc, const char* argv[]) {
    ARG_NOT_NULL(argv);
    ASSERT(!is_initialized_);
    exec_args_.assign(argv, argv + argc);
    optional_bits_.exec_args = true;
    return napi_ok;
  }

  napi_status SetPreloadCallback(
      node_embedding_runtime_preload_callback preload_cb,
      void* preload_cb_data) {
    ASSERT(!is_initialized_);

    // TODO: (vmoroz) use CallIntoModule to handle errors.
    if (preload_cb != nullptr) {
      preload_cb_ = node::EmbedderPreloadCallback(
          [preload_cb, preload_cb_data](node::Environment* node_env,
                                        v8::Local<v8::Value> process,
                                        v8::Local<v8::Value> require) {
            node_napi_env env = GetOrCreateNodeApiEnv(node_env);
            napi_value process_value = v8impl::JsValueFromV8LocalValue(process);
            napi_value require_value = v8impl::JsValueFromV8LocalValue(require);
            preload_cb(env, process_value, require_value, preload_cb_data);
          });
    } else {
      preload_cb_ = {};
    }

    return napi_ok;
  }

  napi_status SetSnapshotBlob(const uint8_t* snapshot, size_t size) {
    ARG_NOT_NULL(snapshot);
    ASSERT(!is_initialized_);

    snapshot_ = node::EmbedderSnapshotData::FromBlob(
        std::string_view(reinterpret_cast<const char*>(snapshot), size));
    return napi_ok;
  }

  napi_status OnCreateSnapshotBlob(
      node_embedding_runtime_store_blob_callback store_blob_cb,
      void* store_blob_cb_data,
      node_embedding_snapshot_flags snapshot_flags) {
    ARG_NOT_NULL(store_blob_cb);
    ASSERT(!is_initialized_);

    create_snapshot_ = [store_blob_cb, store_blob_cb_data](
                           const node::EmbedderSnapshotData* snapshot) {
      std::vector<char> blob = snapshot->ToBlob();
      store_blob_cb(reinterpret_cast<const uint8_t*>(blob.data()),
                    blob.size(),
                    store_blob_cb_data);
    };

    if ((snapshot_flags & node_embedding_snapshot_no_code_cache) != 0) {
      snapshot_config_.flags = static_cast<node::SnapshotFlags>(
          static_cast<uint32_t>(snapshot_config_.flags) |
          static_cast<uint32_t>(node::SnapshotFlags::kWithoutCodeCache));
    }

    return napi_ok;
  }

  napi_status Initialize() {
    ASSERT(!is_initialized_);
    // TODO: (vmoroz) implement this.
    // if (api_version_ == 0) api_version_ =
    // NODE_API_DEFAULT_MODULE_API_VERSION;

    std::vector<std::string> errors;
    std::unique_ptr<node::CommonEnvironmentSetup> env_setup;
    node::MultiIsolatePlatform* platform = platform_->get_v8_platform();
    node::EnvironmentFlags::Flags flags = GetEnvironmentFlags(flags_);
    if (snapshot_) {
      env_setup = node::CommonEnvironmentSetup::CreateFromSnapshot(
          platform, &errors, snapshot_.get(), args_, exec_args_, flags);
    } else if (create_snapshot_) {
      env_setup = node::CommonEnvironmentSetup::CreateForSnapshotting(
          platform, &errors, args_, exec_args_, snapshot_config_);
    } else {
      env_setup = node::CommonEnvironmentSetup::Create(
          platform, &errors, args_, exec_args_, flags);
    }

    if (!errors.empty()) {
      EmbeddedPlatform::HandleError(1, errors);
    }

    if (env_setup == nullptr) {
      return napi_generic_failure;
    }

    std::string filename = args_.size() > 1 ? args_[1] : "<internal>";
    node::CommonEnvironmentSetup* env_setup_ptr = env_setup.get();

    v8impl::IsolateLocker isolate_locker(env_setup_ptr);
    // TODO: (vmoroz) implement this.
    // env_setup_ptr->env()->AddCleanupHook(
    //    [](void* arg) { static_cast<napi_env>(arg)->Unref(); },
    //    static_cast<void*>(embedded_env.get()));
    //*result = embedded_env.get();

    node::Environment* node_env = env_setup_ptr->env();

    // TODO: (vmoroz) If we return an error here, then it is not clear if the
    // environment must be deleted after that or not.

    // TODO: (vmoroz) solve the main script issue.
    std::string main_script;
    v8::MaybeLocal<v8::Value> ret =
        snapshot_
            ? node::LoadEnvironment(node_env, node::StartExecutionCallback{})
            : node::LoadEnvironment(
                  node_env, std::string_view(main_script), preload_cb_);

    if (ret.IsEmpty()) return napi_pending_exception;

    return napi_ok;
  }

  napi_status RunEventLoop() {
    if (node::SpinEventLoopWithoutCleanup(env_setup_->env()).IsNothing()) {
      return napi_closing;
    }

    return napi_ok;
  }

  // TODO: (vmoroz) add support for is_thread_blocking.
  napi_status RunEventLoopWhile(
      node_embedding_runtime_event_loop_predicate predicate,
      void* predicate_data,
      bool /* is_thread_blocking*/,
      bool* has_more_work) {
    ARG_NOT_NULL(predicate);

    if (predicate(predicate_data)) {
      if (node::SpinEventLoopWithoutCleanup(env_setup_->env(),
                                            [predicate, predicate_data]() {
                                              return predicate(predicate_data);
                                            })
              .IsNothing()) {
        return napi_closing;
      }
    }

    if (has_more_work != nullptr) {
      *has_more_work = uv_loop_alive(env_setup_->env()->event_loop());
    }

    return napi_ok;
  }

  napi_status AwaitPromise(napi_value promise,
                           napi_value* result,
                           bool* has_more_work) {
    // TODO: (vmoroz) implement this.
    // NAPI_PREAMBLE(env);
    // CHECK_ARG(env, result);

    // v8::EscapableHandleScope scope(env->isolate);

    // v8::Local<v8::Value> promise_value =
    //     v8impl::V8LocalValueFromJsValue(promise);
    // if (promise_value.IsEmpty() || !promise_value->IsPromise())
    //   return napi_invalid_arg;
    // v8::Local<v8::Promise> promise_object = promise_value.As<v8::Promise>();

    // v8::Local<v8::Value> rejected = v8::Boolean::New(env->isolate, false);
    // v8::Local<v8::Function> err_handler =
    //     v8::Function::New(
    //         env->context(),
    //         [](const v8::FunctionCallbackInfo<v8::Value>& info) { return; },
    //         rejected)
    //         .ToLocalChecked();

    // if (promise_object->Catch(env->context(), err_handler).IsEmpty())
    //   return napi_pending_exception;

    // if (node::SpinEventLoopWithoutCleanup(
    //         embedded_env->node_env(),
    //         [&promise_object]() {
    //           return promise_object->State() ==
    //                  v8::Promise::PromiseState::kPending;
    //         })
    //         .IsNothing())
    //   return napi_closing;

    //*result =
    //    v8impl::JsValueFromV8LocalValue(scope.Escape(promise_object->Result()));

    // if (has_more_work != nullptr) {
    //   *has_more_work = uv_loop_alive(embedded_env->node_env()->event_loop());
    // }

    // if (promise_object->State() == v8::Promise::PromiseState::kRejected)
    //   return napi_pending_exception;

    return napi_ok;
  }

  napi_status SetNodeApiVersion(int32_t node_api_version) { return napi_ok; }

  napi_status GetNodeApiEnv(napi_env* env) { return napi_ok; }

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

  // TODO: (vmoroz) implement this.
  // static EmbeddedEnvironment* FromNapiEnv(napi_env env) {
  //  return static_cast<EmbeddedEnvironment*>(env);
  //}

  node::EnvironmentFlags::Flags GetEnvironmentFlags(
      node_embedding_runtime_flags flags) {
    uint64_t result = node::EnvironmentFlags::kNoFlags;
    if ((flags & node_embedding_runtime_default_flags) != 0) {
      result |= node::EnvironmentFlags::kDefaultFlags;
    }
    if ((flags & node_embedding_runtime_owns_process_state) != 0) {
      result |= node::EnvironmentFlags::kOwnsProcessState;
    }
    if ((flags & node_embedding_runtime_owns_inspector) != 0) {
      result |= node::EnvironmentFlags::kOwnsInspector;
    }
    if ((flags & node_embedding_runtime_no_register_esm_loader) != 0) {
      result |= node::EnvironmentFlags::kNoRegisterESMLoader;
    }
    if ((flags & node_embedding_runtime_track_unmanaged_fds) != 0) {
      result |= node::EnvironmentFlags::kTrackUnmanagedFds;
    }
    if ((flags & node_embedding_runtime_hide_console_windows) != 0) {
      result |= node::EnvironmentFlags::kHideConsoleWindows;
    }
    if ((flags & node_embedding_runtime_no_native_addons) != 0) {
      result |= node::EnvironmentFlags::kNoNativeAddons;
    }
    if ((flags & node_embedding_runtime_no_global_search_paths) != 0) {
      result |= node::EnvironmentFlags::kNoGlobalSearchPaths;
    }
    if ((flags & node_embedding_runtime_no_browser_globals) != 0) {
      result |= node::EnvironmentFlags::kNoBrowserGlobals;
    }
    if ((flags & node_embedding_runtime_no_create_inspector) != 0) {
      result |= node::EnvironmentFlags::kNoCreateInspector;
    }
    if ((flags & node_embedding_runtime_no_start_debug_signal_handler) != 0) {
      result |= node::EnvironmentFlags::kNoStartDebugSignalHandler;
    }
    if ((flags & node_embedding_runtime_no_wait_for_inspector_frontend) != 0) {
      result |= node::EnvironmentFlags::kNoWaitForInspectorFrontend;
    }
    return static_cast<node::EnvironmentFlags::Flags>(result);
  }

 private:
  EmbeddedPlatform* platform_;
  bool is_initialized_{false};
  node_embedding_runtime_flags flags_{node_embedding_runtime_default_flags};
  std::vector<std::string> args_;
  std::vector<std::string> exec_args_;
  node::EmbedderPreloadCallback preload_cb_{};
  node::EmbedderSnapshotData::Pointer snapshot_;
  std::function<void(const node::EmbedderSnapshotData*)> create_snapshot_;
  node::SnapshotConfig snapshot_config_{};
  std::unique_ptr<node::CommonEnvironmentSetup> env_setup_;
  std::optional<IsolateLocker> isolate_locker_;

  struct {
    bool flags : 1;
    bool args : 1;
    bool exec_args : 1;
  } optional_bits_{};

  static std::mutex shared_mutex_;
  static std::unordered_map<node::Environment*, node_napi_env>
      node_env_to_node_api_env_;
};

std::mutex EmbeddedRuntime::shared_mutex_{};
std::unordered_map<node::Environment*, node_napi_env>
    EmbeddedRuntime::node_env_to_node_api_env_{};

napi_status EmbeddedPlatform::CreateRuntime(node_embedding_runtime* result) {
  ARG_NOT_NULL(result);
  ASSERT(is_initialized_);
  ASSERT(v8_is_initialized_);

  std::unique_ptr<EmbeddedRuntime> runtime =
      std::make_unique<EmbeddedRuntime>(this);

  *result = reinterpret_cast<node_embedding_runtime>(runtime.release());

  return napi_ok;
}

}  // end of anonymous namespace
}  // end of namespace v8impl

napi_status NAPI_CDECL node_embedding_on_error(
    node_embedding_error_handler error_handler, void* error_handler_data) {
  return v8impl::EmbeddedPlatform::SetErrorHandler(error_handler,
                                                   error_handler_data);
}

napi_status NAPI_CDECL node_embedding_create_platform(
    int32_t api_version, node_embedding_platform* result) {
  ARG_NOT_NULL(result);
  *result = reinterpret_cast<node_embedding_platform>(
      new v8impl::EmbeddedPlatform(api_version));
  return napi_ok;
}

napi_status NAPI_CDECL
node_embedding_delete_platform(node_embedding_platform platform) {
  return EMBEDDED_PLATFORM(platform)->Delete();
}

napi_status NAPI_CDECL node_embedding_platform_is_initialized(
    node_embedding_platform platform, bool* result) {
  return EMBEDDED_PLATFORM(platform)->IsInitialized(result);
}

napi_status NAPI_CDECL node_embedding_platform_set_flags(
    node_embedding_platform platform, node_embedding_platform_flags flags) {
  return EMBEDDED_PLATFORM(platform)->SetFlags(flags);
}

napi_status NAPI_CDECL node_embedding_platform_set_args(
    node_embedding_platform platform, int32_t argc, const char* argv[]) {
  return EMBEDDED_PLATFORM(platform)->SetArgs(argc, argv);
}

napi_status NAPI_CDECL node_embedding_platform_initialize(
    node_embedding_platform platform, bool* early_return) {
  return EMBEDDED_PLATFORM(platform)->Initialize(early_return);
}

napi_status NAPI_CDECL
node_embedding_platform_get_args(node_embedding_platform platform,
                                 node_embedding_get_args_callback get_args,
                                 void* get_args_data) {
  return EMBEDDED_PLATFORM(platform)->GetArgs(get_args, get_args_data);
}

napi_status NAPI_CDECL
node_embedding_platform_get_exec_args(node_embedding_platform platform,
                                      node_embedding_get_args_callback get_args,
                                      void* get_args_data) {
  return EMBEDDED_PLATFORM(platform)->GetExecArgs(get_args, get_args_data);
}

napi_status NAPI_CDECL node_embedding_create_runtime(
    node_embedding_platform platform, node_embedding_runtime* result) {
  return EMBEDDED_PLATFORM(platform)->CreateRuntime(result);
}

napi_status NAPI_CDECL
node_embdedding_delete_runtime(node_embedding_runtime runtime) {
  return EMBEDDED_RUNTIME(runtime)->Delete();
}

napi_status NAPI_CDECL node_embedding_runtime_is_initialized(
    node_embedding_runtime runtime, bool* result) {
  return EMBEDDED_RUNTIME(runtime)->IsInitialized(result);
}

napi_status NAPI_CDECL node_embedding_runtime_set_flags(
    node_embedding_runtime runtime, node_embedding_runtime_flags flags) {
  return EMBEDDED_RUNTIME(runtime)->SetFlags(flags);
}

napi_status NAPI_CDECL node_embedding_runtime_set_args(
    node_embedding_runtime runtime, int32_t argc, const char* argv[]) {
  return EMBEDDED_RUNTIME(runtime)->SetArgs(argc, argv);
}

napi_status NAPI_CDECL node_embedding_runtime_set_exec_args(
    node_embedding_runtime runtime, int32_t argc, const char* argv[]) {
  return EMBEDDED_RUNTIME(runtime)->SetExecArgs(argc, argv);
}

napi_status NAPI_CDECL node_embedding_runtime_on_preload(
    node_embedding_runtime runtime,
    node_embedding_runtime_preload_callback preload_cb,
    void* preload_cb_data) {
  return EMBEDDED_RUNTIME(runtime)->SetPreloadCallback(preload_cb,
                                                       preload_cb_data);
}

napi_status NAPI_CDECL node_embedding_runtime_use_snapshot(
    node_embedding_runtime runtime, const uint8_t* snapshot, size_t size) {
  return EMBEDDED_RUNTIME(runtime)->SetSnapshotBlob(snapshot, size);
}

napi_status NAPI_CDECL node_embedding_runtime_on_create_snapshot(
    node_embedding_runtime runtime,
    node_embedding_runtime_store_blob_callback store_blob_cb,
    void* store_blob_cb_data,
    node_embedding_snapshot_flags snapshot_flags) {
  return EMBEDDED_RUNTIME(runtime)->OnCreateSnapshotBlob(
      store_blob_cb, store_blob_cb_data, snapshot_flags);
}

napi_status NAPI_CDECL
node_embedding_runtime_initialize(node_embedding_runtime runtime) {
  return EMBEDDED_RUNTIME(runtime)->Initialize();
}

napi_status NAPI_CDECL
node_embedding_runtime_run_event_loop(node_embedding_runtime runtime) {
  return EMBEDDED_RUNTIME(runtime)->RunEventLoop();
}

napi_status NAPI_CDECL node_embedding_runtime_run_event_loop_while(
    node_embedding_runtime runtime,
    node_embedding_runtime_event_loop_predicate predicate,
    void* predicate_data,
    bool is_thread_blocking,
    bool* has_more_work) {
  return EMBEDDED_RUNTIME(runtime)->RunEventLoopWhile(
      predicate, predicate_data, is_thread_blocking, has_more_work);
}

napi_status NAPI_CDECL
node_embedding_runtime_await_promise(node_embedding_runtime runtime,
                                     napi_value promise,
                                     napi_value* result,
                                     bool* has_more_work) {
  return EMBEDDED_RUNTIME(runtime)->AwaitPromise(
      promise, result, has_more_work);
}

napi_status NAPI_CDECL node_embedding_runtime_set_node_api_version(
    node_embedding_runtime runtime, int32_t node_api_version) {
  return EMBEDDED_RUNTIME(runtime)->SetNodeApiVersion(node_api_version);
}

napi_status NAPI_CDECL node_embedding_runtime_get_node_api_env(
    node_embedding_runtime runtime, napi_env* env) {
  return EMBEDDED_RUNTIME(runtime)->GetNodeApiEnv(env);
}

napi_status NAPI_CDECL
node_embedding_runtime_open_scope(node_embedding_runtime runtime) {
  return EMBEDDED_RUNTIME(runtime)->OpenScope();
}

napi_status NAPI_CDECL
node_embedding_runtime_close_scope(node_embedding_runtime runtime) {
  return EMBEDDED_RUNTIME(runtime)->CloseScope();
}
