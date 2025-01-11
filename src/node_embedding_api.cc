#define NAPI_EXPERIMENTAL
#include "node_embedding_api.h"

#include "env-inl.h"
#include "js_native_api_v8.h"
#include "node_api_internals.h"
#include "util-inl.h"
#include "uv.h"

#include <mutex>
#include <string>
#include <string_view>

#if defined(__APPLE__)
#include <sys/select.h>
#elif defined(__linux)
#include <sys/epoll.h>
#endif

// Use macros to handle errors since they can record the failing argument name
// or expression and their location in the source code.

#define CAST_NOT_NULL_TO(value, type)                                          \
  (value) == nullptr ? node::EmbeddedErrorHandling::HandleError(               \
                           "Argument must not be null: " #value,               \
                           __FILE__,                                           \
                           __LINE__,                                           \
                           node_embedding_status::kNullArg)                    \
                     : reinterpret_cast<type*>(value)

#define EMBEDDED_PLATFORM(platform)                                            \
  CAST_NOT_NULL_TO(platform, node::EmbeddedPlatform)

#define EMBEDDED_RUNTIME(runtime)                                              \
  CAST_NOT_NULL_TO(runtime, node::EmbeddedRuntime)

#define CHECK_ARG_NOT_NULL(arg)                                                \
  do {                                                                         \
    if ((arg) == nullptr) {                                                    \
      return node::EmbeddedErrorHandling::HandleError(                         \
          "Argument must not be null: " #arg,                                  \
          __FILE__,                                                            \
          __LINE__,                                                            \
          node_embedding_status::kNullArg);                                    \
    }                                                                          \
  } while (false)

#define ASSERT_ARG(arg, expr)                                                  \
  do {                                                                         \
    if (!(expr)) {                                                             \
      return node::EmbeddedErrorHandling::HandleError(                         \
          "Arg: " #arg " failed: " #expr,                                      \
          __FILE__,                                                            \
          __LINE__,                                                            \
          node_embedding_status::kBadArg);                                     \
    }                                                                          \
  } while (false)

#define ASSERT(expr)                                                           \
  do {                                                                         \
    if (!(expr)) {                                                             \
      return node::EmbeddedErrorHandling::HandleError(                         \
          "Expression returned false: " #expr,                                 \
          __FILE__,                                                            \
          __LINE__,                                                            \
          node_embedding_status::kGenericError);                               \
    }                                                                          \
  } while (false)

#define CHECK_STATUS(expr)                                                     \
  do {                                                                         \
    node_embedding_status status = (expr);                                     \
    if (status != node_embedding_status::kOk) {                                \
      return status;                                                           \
    }                                                                          \
  } while (false)

namespace v8impl {

napi_env NewEnv(v8::Local<v8::Context> context,
                const std::string& module_filename,
                int32_t module_api_version);

}  // namespace v8impl

namespace node {

// Declare functions implemented in embed_helpers.cc
v8::Maybe<ExitCode> SpinEventLoopWithoutCleanup(Environment* env,
                                                uv_run_mode run_mode);

//------------------------------------------------------------------------------
// Convenience functor struct adapter for C++ function object or lambdas.
//------------------------------------------------------------------------------

template <typename TCallback>
struct functor_struct {
  void* data{};
  TCallback invoke{};
  node_embedding_release_data_callback release{};

  functor_struct() = default;

  functor_struct(void* data,
                 TCallback invoke,
                 node_embedding_release_data_callback release)
      : data(data), invoke(invoke), release(release) {}

  ~functor_struct() {
    if (release != nullptr) {
      release(data);
    }
  }
};

template <typename TCallback>
std::shared_ptr<functor_struct<TCallback>> MakeSharedFunctorPtr(
    TCallback callback,
    void* callback_data,
    node_embedding_release_data_callback release_callback_data) {
  return callback ? std::make_shared<functor_struct<TCallback>>(
                        callback_data, callback, release_callback_data)
                  : nullptr;
}

template <typename TCallback>
std::unique_ptr<functor_struct<TCallback>> MakeUniqueFunctorPtr(
    TCallback callback,
    void* callback_data,
    node_embedding_release_data_callback release_callback_data) {
  return callback ? std::make_unique_ptr<functor_struct<TCallback>>(
                        callback_data, callback, release_callback_data)
                  : nullptr;
}

// TODO: consider better name for this class.
template <typename TCallback>
class UniqueFunction;

template <typename TResult, typename... TArgs>
class UniqueFunction<TResult (*)(void*, TArgs...)> {
 public:
  using TCallback = TResult (*)(void*, TArgs...);

  UniqueFunction() = default;
  UniqueFunction(TCallback callback,
                 void* callback_data,
                 node_embedding_release_data_callback release_callback_data)
      : functor_{callback_data, callback, release_callback_data} {}

  ~UniqueFunction() {
    if (functor_.release != nullptr) {
      functor_.release(functor_.data);
    }
  }

  UniqueFunction(const UniqueFunction&) = delete;
  UniqueFunction& operator=(const UniqueFunction&) = delete;

  UniqueFunction(UniqueFunction&& other)
      : functor_{std::exchange(other.functor_, {})} {}

  UniqueFunction& operator=(UniqueFunction&& other) {
    if (this != &other) {
      UniqueFunction temp(std::move(other));
      functor_ = std::exchange(other.functor_, {});
    }
    return *this;
  }

  TResult operator()(TArgs... args) const {
    return functor_.invoke ? functor_.invoke(functor_.data, args...)
                           : TResult();
  }

  explicit operator bool() const { return functor_.invoke != nullptr; }

 private:
  functor_struct<TCallback> functor_{};
};

// A helper class to convert std::vector<std::string> to an array of C strings.
// If the number of strings is less than kInplaceBufferSize, the strings are
// stored in the inplace_buffer_ array. Otherwise, the strings are stored in the
// allocated_buffer_ array.
// Ideally the class must be allocated on the stack.
// In any case it must not outlive the passed vector since it keeps only the
// string pointers returned by std::string::c_str() method.
template <size_t kInplaceBufferSize = 32>
class CStringArray {
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
  int32_t argc() const { return static_cast<int32_t>(size_); }

 private:
  const char** c_strs_{};
  size_t size_{};
  std::array<const char*, kInplaceBufferSize> inplace_buffer_;
  std::unique_ptr<const char*[]> allocated_buffer_;
};

// Stack implementation that works only with trivially constructible,
// destructible, and copyable types. It uses the small value optimization where
// several elements are stored in the in-place array.
template <typename T, size_t kInplaceEntryCount = 8>
class SmallTrivialStack {
  static_assert(std::is_trivially_constructible_v<T>,
                "T must be trivially constructible");
  static_assert(std::is_trivially_destructible_v<T>,
                "T must be trivially destructible");
  static_assert(std::is_trivially_copyable_v<T>,
                "T must be trivially copyable");

 public:
  SmallTrivialStack() noexcept : stack_(this->inplace_entries_.data()) {}

  void Push(T&& value) {
    EnsureCapacity(size_ + 1);
    stack_[size_++] = std::move(value);
  }

  void Pop() {
    CHECK_GT(size_, 0);
    --size_;
  }

  size_t size() const { return size_; }

  const T& top() const {
    CHECK_GT(size_, 0);
    return stack_[size_ - 1];
  }

  SmallTrivialStack(const SmallTrivialStack&) = delete;
  SmallTrivialStack& operator=(const SmallTrivialStack&) = delete;

 private:
  void EnsureCapacity(size_t new_size) {
    if (new_size <= capacity_) {
      return;
    }

    size_t new_capacity = capacity_ + capacity_ / 2;
    std::unique_ptr<T[]> new_allocated_entries =
        std::make_unique<T[]>(new_capacity);
    std::memcpy(new_allocated_entries.get(), stack_, size_ * sizeof(T));
    allocated_entries_ = std::move(new_allocated_entries);
    stack_ = allocated_entries_.get();
    capacity_ = new_capacity;
  }

 private:
  std::array<T, kInplaceEntryCount> inplace_entries_;
  T* stack_{};     // Points to either inplace_entries_ or allocated_entries_.
  size_t size_{};  // Number of elements in the stack.
  size_t capacity_{kInplaceEntryCount};
  std::unique_ptr<T[]> allocated_entries_;
};

class EmbeddedErrorHandling {
 public:
  using ErrorHandlerCallback =
      UniqueFunction<node_embedding_handle_error_callback>;

  static node_embedding_status SetErrorHandler(
      ErrorHandlerCallback error_handler);

  static node_embedding_status HandleError(std::string_view message,
                                           node_embedding_status status);

  static node_embedding_status HandleError(
      const std::vector<std::string>& messages, node_embedding_status status);

  static node_embedding_status HandleError(const char* message,
                                           const char* filename,
                                           int32_t line,
                                           node_embedding_status status);

  static std::string FormatString(const char* format, ...);

 private:
  static ErrorHandlerCallback* ErrorHandler();
  static std::mutex& ErrorHandlerMutex();

  static node_embedding_status DefaultErrorHandler(
      void* handler_data,
      const char* messages[],
      size_t messages_size,
      node_embedding_status status);
};

class EmbeddedPlatform {
 public:
  EmbeddedPlatform(int32_t argc, const char* argv[]) noexcept
      : args_(argv, argv + argc) {}

  EmbeddedPlatform(const EmbeddedPlatform&) = delete;
  EmbeddedPlatform& operator=(const EmbeddedPlatform&) = delete;

  static node_embedding_status RunMain(
      int32_t argc,
      const char* argv[],
      const node_embedding_version_info* version_info,
      node_embedding_configure_platform_callback configure_platform,
      void* configure_platform_data,
      node_embedding_configure_runtime_callback configure_runtime,
      void* configure_runtime_data);

  static node_embedding_status Create(
      int32_t argc,
      const char* argv[],
      const node_embedding_version_info* version_info,
      node_embedding_configure_platform_callback configure_platform,
      void* configure_platform_data,
      node_embedding_platform* result);

  node_embedding_status DeleteMe();

  node_embedding_status SetFlags(node_embedding_platform_flags flags);

  node_embedding_status Initialize(
      node_embedding_configure_platform_callback configure_platform,
      void* configure_platform_data,
      bool* early_return);

  node_embedding_status GetParsedArgs(
      node_embedding_get_strings_callback get_args,
      void* get_args_data,
      node_embedding_get_strings_callback get_exec_args,
      void* get_exec_args_data);

  node::InitializationResult* init_result() { return init_result_.get(); }

  node::MultiIsolatePlatform* get_v8_platform() { return v8_platform_.get(); }

  static int32_t embedding_api_version() {
    return embedding_api_version_ == 0 ? NODE_EMBEDDING_VERSION
                                       : embedding_api_version_;
  }

  static int32_t node_api_version() {
    return node_api_version_ == 0 ? NAPI_VERSION : node_api_version_;
  }

 private:
  static node_embedding_status EmbeddedPlatform::SetApiVersion(
      int32_t embedding_api_version, int32_t node_api_version);

  static node::ProcessInitializationFlags::Flags GetProcessInitializationFlags(
      node_embedding_platform_flags flags);

 private:
  bool is_initialized_{false};
  bool v8_is_initialized_{false};
  bool v8_is_uninitialized_{false};
  node_embedding_platform_flags flags_;
  std::vector<std::string> args_;
  struct {
    bool flags : 1;
  } optional_bits_{};

  std::shared_ptr<node::InitializationResult> init_result_;
  std::unique_ptr<node::MultiIsolatePlatform> v8_platform_;

  static int32_t embedding_api_version_;
  static int32_t node_api_version_;
};

int32_t EmbeddedPlatform::embedding_api_version_{};
int32_t EmbeddedPlatform::node_api_version_{};

class EmbeddedRuntime {
 public:
  explicit EmbeddedRuntime(EmbeddedPlatform* platform);

  EmbeddedRuntime(const EmbeddedRuntime&) = delete;
  EmbeddedRuntime& operator=(const EmbeddedRuntime&) = delete;

  static node_embedding_status Run(
      node_embedding_platform platform,
      node_embedding_configure_runtime_callback configure_runtime,
      void* configure_runtime_data);

  static node_embedding_status Create(
      node_embedding_platform platform,
      node_embedding_configure_runtime_callback configure_runtime,
      void* configure_runtime_data,
      node_embedding_runtime* result);

  node_embedding_status DeleteMe();

  node_embedding_status SetFlags(node_embedding_runtime_flags flags);

  node_embedding_status SetArgs(int32_t argc,
                                const char* argv[],
                                int32_t exec_argc,
                                const char* exec_argv[]);

  node_embedding_status OnPreload(
      node_embedding_preload_callback run_preload,
      void* preload_data,
      node_embedding_release_data_callback release_preload_data);

  node_embedding_status OnStartExecution(
      node_embedding_start_execution_callback start_execution,
      void* start_execution_data,
      node_embedding_release_data_callback release_start_execution_data);

  node_embedding_status OnHandleStartResult(
      node_embedding_handle_execution_result_callback handle_result,
      void* handle_result_data,
      node_embedding_release_data_callback release_handle_result_data);

  node_embedding_status AddModule(
      const char* module_name,
      node_embedding_initialize_module_callback init_module,
      void* init_module_data,
      node_embedding_release_data_callback release_init_module_data,
      int32_t module_node_api_version);

  node_embedding_status OnCreateWrapper(
      node_embedding_create_runtime_wrapper_callback create_wrapper,
      void* create_wrapper_data,
      node_embedding_release_data_callback release_create_wrapper_data);

  node_embedding_status GetWrapper(void** result);

  node_embedding_status Initialize(
      node_embedding_configure_runtime_callback configure_runtime,
      void* configure_runtime_data);

  node_embedding_status SetTaskRunner(
      node_embedding_post_task_callback post_task,
      void* post_task_data,
      node_embedding_release_data_callback release_post_task_data);

  node_embedding_status RunEventLoop();

  node_embedding_status TerminateEventLoop();

  node_embedding_status RunEventLoopOnce(bool* has_more_work);

  node_embedding_status RunEventLoopNoWait(bool* has_more_work);

  node_embedding_status RunNodeApi(
      node_embedding_run_node_api_callback run_node_api,
      void* run_node_api_data);

  node_embedding_status OpenNodeApiScope(
      node_embedding_node_api_scope* node_api_scope, napi_env* env);
  node_embedding_status CloseNodeApiScope(
      node_embedding_node_api_scope node_api_scope);
  bool IsNodeApiScopeOpened() const;

  static napi_env GetOrCreateNodeApiEnv(node::Environment* node_env,
                                        const std::string& module_filename);

  size_t OpenV8Scope();
  void CloseV8Scope(size_t nest_level);

 private:
  static void TriggerFatalException(napi_env env,
                                    v8::Local<v8::Value> local_err);
  static node::EnvironmentFlags::Flags GetEnvironmentFlags(
      node_embedding_runtime_flags flags);

  void RegisterModules();

  static void RegisterModule(v8::Local<v8::Object> exports,
                             v8::Local<v8::Value> module,
                             v8::Local<v8::Context> context,
                             void* priv);

  uv_loop_t* EventLoop();
  void InitializePollingThread();
  void DestroyPollingThread();
  void WakeupPollingThread();
  static void RunPollingThread(void* data);
  void PollEvents();

 private:
  struct ModuleInfo {
    node_embedding_runtime runtime;
    std::string module_name;
    UniqueFunction<node_embedding_initialize_module_callback> init_module;
    int32_t module_node_api_version;
  };

  struct SharedData {
    std::mutex mutex;
    std::unordered_map<node::Environment*, napi_env> node_env_to_node_api_env;

    static SharedData& Get() {
      static SharedData shared_data;
      return shared_data;
    }
  };

  struct V8ScopeLocker {
    explicit V8ScopeLocker(EmbeddedRuntime& runtime)
        : runtime_(runtime), nest_level_(runtime_.OpenV8Scope()) {}

    ~V8ScopeLocker() { runtime_.CloseV8Scope(nest_level_); }

    V8ScopeLocker(const V8ScopeLocker&) = delete;
    V8ScopeLocker& operator=(const V8ScopeLocker&) = delete;

   private:
    EmbeddedRuntime& runtime_;
    size_t nest_level_;
  };

  struct V8ScopeData {
    V8ScopeData(node::CommonEnvironmentSetup* env_setup)
        : isolate_(env_setup->isolate()),
          v8_locker_(env_setup->isolate()),
          isolate_scope_(env_setup->isolate()),
          handle_scope_(env_setup->isolate()),
          context_scope_(env_setup->context()) {}

    bool IsLocked() const { return v8::Locker::IsLocked(isolate_); }

    size_t IncrementNestLevel() { return ++nest_level_; }

    bool DecrementNestLevel() { return --nest_level_ == 0; }

    size_t nest_level() const { return nest_level_; }

   private:
    int32_t nest_level_{1};
    v8::Isolate* isolate_;
    v8::Locker v8_locker_;  // TODO(vmoroz): can we remove it?
    v8::Isolate::Scope isolate_scope_;
    v8::HandleScope handle_scope_;
    v8::Context::Scope context_scope_;
  };

  struct NodeApiScopeData {
    napi_env__::CallModuleScopeData module_scope_data_;
    size_t v8_scope_nest_level_;
  };

 private:
  EmbeddedPlatform* platform_;
  bool is_initialized_{false};
  node_embedding_runtime_flags flags_{node_embedding_runtime_flags::kDefault};
  std::vector<std::string> args_;
  std::vector<std::string> exec_args_;
  node::EmbedderPreloadCallback preload_cb_{};
  node::StartExecutionCallback start_execution_cb_{};
  UniqueFunction<node_embedding_handle_execution_result_callback>
      handle_result_{};
  UniqueFunction<node_embedding_create_runtime_wrapper_callback>
      on_create_wrapper_{};
  void* wrapper_{};
  napi_env node_api_env_{};

  struct {
    bool flags : 1;
    bool args : 1;
    bool exec_args : 1;
  } optional_bits_{};

  std::unordered_map<std::string, ModuleInfo> modules_;

  std::unique_ptr<node::CommonEnvironmentSetup> env_setup_;
  std::optional<V8ScopeData> v8_scope_data_;

  UniqueFunction<node_embedding_post_task_callback> post_task_{};
  uv_async_t polling_async_handle_{};
  uv_sem_t polling_sem_{};
  uv_thread_t polling_thread_{};
  bool polling_thread_closed_{false};
#if defined(__linux)
  // Epoll to poll for uv's backend fd.
  int epoll_{epoll_create(1)};
#endif

  SmallTrivialStack<NodeApiScopeData> node_api_scope_data_{};
};

//-----------------------------------------------------------------------------
// EmbeddedErrorHandling implementation.
//-----------------------------------------------------------------------------

node_embedding_status EmbeddedErrorHandling::SetErrorHandler(
    ErrorHandlerCallback error_handler) {
  std::scoped_lock lock(ErrorHandlerMutex());
  *ErrorHandler() = std::move(error_handler);
  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedErrorHandling::HandleError(
    std::string_view message, node_embedding_status status) {
  const char* message_c_str = message.data();
  std::scoped_lock lock(ErrorHandlerMutex());
  return (*ErrorHandler())(1, &message_c_str, status);
}

node_embedding_status EmbeddedErrorHandling::HandleError(
    const std::vector<std::string>& messages, node_embedding_status status) {
  CStringArray message_arr(messages);
  std::scoped_lock lock(ErrorHandlerMutex());
  return (*ErrorHandler())(message_arr.size(), message_arr.c_strs(), status);
}

node_embedding_status EmbeddedErrorHandling::HandleError(
    const char* message,
    const char* filename,
    int32_t line,
    node_embedding_status status) {
  return HandleError(
      FormatString("Error: %s at %s:%d", message, filename, line), status);
}

node_embedding_status EmbeddedErrorHandling::DefaultErrorHandler(
    void* /*handler_data*/,
    const char* messages[],
    size_t messages_size,
    node_embedding_status status) {
  // TODO: see how the rest of Node.js reports to console.
  FILE* stream = status != node_embedding_status::kOk ? stderr : stdout;
  for (size_t i = 0; i < messages_size; ++i) {
    fprintf(stream, "%s\n", messages[i]);
  }
  fflush(stream);
  return status;
}

std::string EmbeddedErrorHandling::FormatString(const char* format, ...) {
  va_list args1;
  va_start(args1, format);
  va_list args2;  // Required for some compilers like GCC since we go over the
                  // args twice.
  va_copy(args2, args1);
  std::string result(std::vsnprintf(nullptr, 0, format, args1), '\0');
  va_end(args1);
  std::vsnprintf(&result[0], result.size() + 1, format, args2);
  va_end(args2);
  return result;
}

EmbeddedErrorHandling::ErrorHandlerCallback*
EmbeddedErrorHandling::ErrorHandler() {
  static ErrorHandlerCallback error_handler = {
      nullptr, &DefaultErrorHandler, nullptr};
  return &error_handler;
}

std::mutex& EmbeddedErrorHandling::ErrorHandlerMutex() {
  static std::mutex mutex;
  return mutex;
}

//-----------------------------------------------------------------------------
// EmbeddedPlatform implementation.
//-----------------------------------------------------------------------------

/*static*/ node_embedding_status EmbeddedPlatform::SetApiVersion(
    int32_t embedding_api_version, int32_t node_api_version) {
  ASSERT_ARG(embedding_api_version,
             embedding_api_version > 0 &&
                 embedding_api_version <= NODE_EMBEDDING_VERSION);
  ASSERT_ARG(node_api_version,
             node_api_version >= NODE_API_DEFAULT_MODULE_API_VERSION &&
                 (node_api_version <= NAPI_VERSION ||
                  node_api_version == NAPI_VERSION_EXPERIMENTAL));

  embedding_api_version_ = embedding_api_version;
  node_api_version_ = node_api_version;

  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedPlatform::RunMain(
    int32_t argc,
    const char* argv[],
    const node_embedding_version_info* version_info,
    node_embedding_configure_platform_callback configure_platform,
    void* configure_platform_data,
    node_embedding_configure_runtime_callback configure_runtime,
    void* configure_runtime_data) {
  node_embedding_platform platform{};
  CHECK_STATUS(EmbeddedPlatform::Create(argc,
                                        argv,
                                        version_info,
                                        configure_platform,
                                        configure_platform_data,
                                        &platform));
  if (platform == nullptr) {
    return node_embedding_status::kOk;  // early return
  }
  return EmbeddedRuntime::Run(
      platform, configure_runtime, configure_runtime_data);
}

/*static*/ node_embedding_status EmbeddedPlatform::Create(
    int32_t argc,
    const char* argv[],
    const node_embedding_version_info* version_info,
    node_embedding_configure_platform_callback configure_platform,
    void* configure_platform_data,
    node_embedding_platform* result) {
  CHECK_ARG_NOT_NULL(result);

  if (version_info != nullptr) {
    CHECK_STATUS(SetApiVersion(version_info->embedding_api_version,
                               version_info->node_api_version));
  }

  // Hack around with the argv pointer. Used for process.title = "blah".
  argv =
      const_cast<const char**>(uv_setup_args(argc, const_cast<char**>(argv)));

  auto platform_ptr = std::make_unique<EmbeddedPlatform>(argc, argv);
  bool early_return = false;
  CHECK_STATUS(platform_ptr->Initialize(
      configure_platform, configure_platform_data, &early_return));
  if (early_return) {
    return platform_ptr.release()->DeleteMe();
  }

  // The initialization was successful, the caller is responsible for deleting
  // the platform instance.
  *result = reinterpret_cast<node_embedding_platform>(platform_ptr.release());
  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedPlatform::DeleteMe() {
  if (v8_is_initialized_ && !v8_is_uninitialized_) {
    v8_is_uninitialized_ = true;
    v8::V8::Dispose();
    v8::V8::DisposePlatform();
    node::TearDownOncePerProcess();
  }

  delete this;
  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedPlatform::SetFlags(
    node_embedding_platform_flags flags) {
  ASSERT(!is_initialized_);
  flags_ = flags;
  optional_bits_.flags = true;
  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedPlatform::Initialize(
    node_embedding_configure_platform_callback configure_platform,
    void* configure_platform_data,
    bool* early_return) {
  ASSERT(!is_initialized_);

  node_embedding_platform_config platform_config =
      reinterpret_cast<node_embedding_platform_config>(this);
  if (configure_platform != nullptr) {
    CHECK_STATUS(configure_platform(configure_platform_data, platform_config));
  }

  is_initialized_ = true;

  if (!optional_bits_.flags) {
    flags_ = node_embedding_platform_flags::kNone;
  }

  init_result_ = node::InitializeOncePerProcess(
      args_, GetProcessInitializationFlags(flags_));
  int32_t exit_code = init_result_->exit_code();
  if (exit_code != 0 || !init_result_->errors().empty()) {
    CHECK_STATUS(EmbeddedErrorHandling::HandleError(
        init_result_->errors(),
        exit_code != 0
            ? static_cast<node_embedding_status>(
                  static_cast<int32_t>(node_embedding_status::kErrorExitCode) +
                  exit_code)
            : node_embedding_status::kOk));
  }

  if (init_result_->early_return()) {
    *early_return = true;
    return node_embedding_status::kOk;
  }

  int32_t thread_pool_size =
      static_cast<int32_t>(node::per_process::cli_options->v8_thread_pool_size);
  v8_platform_ = node::MultiIsolatePlatform::Create(thread_pool_size);
  v8::V8::InitializePlatform(v8_platform_.get());
  v8::V8::Initialize();

  v8_is_initialized_ = true;

  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedPlatform::GetParsedArgs(
    node_embedding_get_strings_callback get_args,
    void* get_args_data,
    node_embedding_get_strings_callback get_exec_args,
    void* get_exec_args_data) {
  ASSERT(is_initialized_);

  if (get_args != nullptr) {
    node::CStringArray args(init_result_->args());
    get_args(get_args_data, args.argc(), args.argv());
  }

  if (get_exec_args != nullptr) {
    node::CStringArray exec_args(init_result_->exec_args());
    get_exec_args(get_exec_args_data, exec_args.argc(), exec_args.argv());
  }

  return node_embedding_status::kOk;
}

node::ProcessInitializationFlags::Flags
EmbeddedPlatform::GetProcessInitializationFlags(
    node_embedding_platform_flags flags) {
  uint32_t result = node::ProcessInitializationFlags::kNoFlags;
  if (embedding::IsFlagSet(
          flags, node_embedding_platform_flags::kEnableStdioInheritance)) {
    result |= node::ProcessInitializationFlags::kEnableStdioInheritance;
  }
  if (embedding::IsFlagSet(
          flags, node_embedding_platform_flags::kDisableNodeOptionsEnv)) {
    result |= node::ProcessInitializationFlags::kDisableNodeOptionsEnv;
  }
  if (embedding::IsFlagSet(flags,
                           node_embedding_platform_flags::kDisableCliOptions)) {
    result |= node::ProcessInitializationFlags::kDisableCLIOptions;
  }
  if (embedding::IsFlagSet(flags, node_embedding_platform_flags::kNoICU)) {
    result |= node::ProcessInitializationFlags::kNoICU;
  }
  if (embedding::IsFlagSet(
          flags, node_embedding_platform_flags::kNoStdioInitialization)) {
    result |= node::ProcessInitializationFlags::kNoStdioInitialization;
  }
  if (embedding::IsFlagSet(
          flags, node_embedding_platform_flags::kNoDefaultSignalHandling)) {
    result |= node::ProcessInitializationFlags::kNoDefaultSignalHandling;
  }
  result |= node::ProcessInitializationFlags::kNoInitializeV8;
  result |= node::ProcessInitializationFlags::kNoInitializeNodeV8Platform;
  if (embedding::IsFlagSet(flags,
                           node_embedding_platform_flags::kNoInitOpenSSL)) {
    result |= node::ProcessInitializationFlags::kNoInitOpenSSL;
  }
  if (embedding::IsFlagSet(
          flags, node_embedding_platform_flags::kNoParseGlobalDebugVariables)) {
    result |= node::ProcessInitializationFlags::kNoParseGlobalDebugVariables;
  }
  if (embedding::IsFlagSet(
          flags, node_embedding_platform_flags::kNoAdjustResourceLimits)) {
    result |= node::ProcessInitializationFlags::kNoAdjustResourceLimits;
  }
  if (embedding::IsFlagSet(flags,
                           node_embedding_platform_flags::kNoUseLargePages)) {
    result |= node::ProcessInitializationFlags::kNoUseLargePages;
  }
  if (embedding::IsFlagSet(
          flags, node_embedding_platform_flags::kNoPrintHelpOrVersionOutput)) {
    result |= node::ProcessInitializationFlags::kNoPrintHelpOrVersionOutput;
  }
  if (embedding::IsFlagSet(
          flags, node_embedding_platform_flags::kGeneratePredictableSnapshot)) {
    result |= node::ProcessInitializationFlags::kGeneratePredictableSnapshot;
  }
  return static_cast<node::ProcessInitializationFlags::Flags>(result);
}

//-----------------------------------------------------------------------------
// EmbeddedRuntime implementation.
//-----------------------------------------------------------------------------

/*static*/ node_embedding_status EmbeddedRuntime::Run(
    node_embedding_platform platform,
    node_embedding_configure_runtime_callback configure_runtime,
    void* configure_runtime_data) {
  node_embedding_runtime runtime{};
  CHECK_STATUS(
      Create(platform, configure_runtime, configure_runtime_data, &runtime));
  CHECK_STATUS(node_embedding_run_event_loop(runtime));
  CHECK_STATUS(node_embedding_delete_runtime(runtime));
  return node_embedding_status::kOk;
}

/*static*/ node_embedding_status EmbeddedRuntime::Create(
    node_embedding_platform platform,
    node_embedding_configure_runtime_callback configure_runtime,
    void* configure_runtime_data,
    node_embedding_runtime* result) {
  CHECK_ARG_NOT_NULL(platform);
  CHECK_ARG_NOT_NULL(result);

  EmbeddedPlatform* platform_ptr =
      reinterpret_cast<EmbeddedPlatform*>(platform);
  std::unique_ptr<EmbeddedRuntime> runtime_ptr =
      std::make_unique<EmbeddedRuntime>(platform_ptr);

  CHECK_STATUS(
      runtime_ptr->Initialize(configure_runtime, configure_runtime_data));

  *result = reinterpret_cast<node_embedding_runtime>(runtime_ptr.release());

  return node_embedding_status::kOk;
}

EmbeddedRuntime::EmbeddedRuntime(EmbeddedPlatform* platform)
    : platform_(platform) {}

node_embedding_status EmbeddedRuntime::DeleteMe() {
  ASSERT(!IsNodeApiScopeOpened());

  std::unique_ptr<node::CommonEnvironmentSetup> env_setup =
      std::move(env_setup_);
  if (env_setup != nullptr) {
    node::Stop(env_setup->env());
  }

  delete this;
  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedRuntime::SetFlags(
    node_embedding_runtime_flags flags) {
  ASSERT(!is_initialized_);
  flags_ = flags;
  optional_bits_.flags = true;
  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedRuntime::SetArgs(int32_t argc,
                                               const char* argv[],
                                               int32_t exec_argc,
                                               const char* exec_argv[]) {
  ASSERT(!is_initialized_);
  if (argv != nullptr) {
    args_.assign(argv, argv + argc);
    optional_bits_.args = true;
  }
  if (exec_argv != nullptr) {
    exec_args_.assign(exec_argv, exec_argv + exec_argc);
    optional_bits_.exec_args = true;
  }
  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedRuntime::OnPreload(
    node_embedding_preload_callback run_preload,
    void* preload_data,
    node_embedding_release_data_callback release_preload_data) {
  ASSERT(!is_initialized_);

  if (run_preload != nullptr) {
    preload_cb_ = node::EmbedderPreloadCallback(
        [runtime = reinterpret_cast<node_embedding_runtime>(this),
         run_preload_ptr = MakeSharedFunctorPtr(
             run_preload, preload_data, release_preload_data)](
            node::Environment* node_env,
            v8::Local<v8::Value> process,
            v8::Local<v8::Value> require) {
          napi_env env = GetOrCreateNodeApiEnv(node_env, "<worker thread>");
          env->CallIntoModule(
              [&](napi_env env) {
                napi_value process_value =
                    v8impl::JsValueFromV8LocalValue(process);
                napi_value require_value =
                    v8impl::JsValueFromV8LocalValue(require);
                run_preload_ptr->invoke(run_preload_ptr->data,
                                        runtime,
                                        env,
                                        process_value,
                                        require_value);
              },
              TriggerFatalException);
        });
  } else {
    preload_cb_ = {};
  }

  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedRuntime::OnStartExecution(
    node_embedding_start_execution_callback start_execution,
    void* start_execution_data,
    node_embedding_release_data_callback release_start_execution_data) {
  ASSERT(!is_initialized_);

  if (start_execution != nullptr) {
    start_execution_cb_ = node::StartExecutionCallback(
        [this,
         start_execution_ptr =
             MakeSharedFunctorPtr(start_execution,
                                  start_execution_data,
                                  release_start_execution_data)](
            const node::StartExecutionCallbackInfo& info)
            -> v8::MaybeLocal<v8::Value> {
          napi_value result{};
          node_api_env_->CallIntoModule(
              [&](napi_env env) {
                napi_value process_value =
                    v8impl::JsValueFromV8LocalValue(info.process_object);
                napi_value require_value =
                    v8impl::JsValueFromV8LocalValue(info.native_require);
                napi_value run_cjs_value =
                    v8impl::JsValueFromV8LocalValue(info.run_cjs);
                return start_execution_ptr->invoke(
                    start_execution_ptr->data,
                    reinterpret_cast<node_embedding_runtime>(this),
                    env,
                    process_value,
                    require_value,
                    run_cjs_value,
                    &result);
              },
              TriggerFatalException);

          if (result == nullptr)
            return {};
          else
            return v8impl::V8LocalValueFromJsValue(result);
        });
  } else {
    start_execution_cb_ = {};
  }

  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedRuntime::OnHandleStartResult(
    node_embedding_handle_execution_result_callback handle_result,
    void* handle_result_data,
    node_embedding_release_data_callback release_handle_result_data) {
  ASSERT(!is_initialized_);

  handle_result_ =
      UniqueFunction<node_embedding_handle_execution_result_callback>{
          handle_result, handle_result_data, release_handle_result_data};

  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedRuntime::AddModule(
    const char* module_name,
    node_embedding_initialize_module_callback init_module,
    void* init_module_data,
    node_embedding_release_data_callback release_init_module_data,
    int32_t module_node_api_version) {
  CHECK_ARG_NOT_NULL(module_name);
  CHECK_ARG_NOT_NULL(init_module);
  ASSERT(!is_initialized_);

  auto insert_result = modules_.try_emplace(
      module_name,
      ModuleInfo{reinterpret_cast<node_embedding_runtime>(this),
                 module_name,
                 UniqueFunction<node_embedding_initialize_module_callback>{
                     init_module, init_module_data, release_init_module_data},
                 module_node_api_version});
  if (!insert_result.second) {
    return EmbeddedErrorHandling::HandleError(
        EmbeddedErrorHandling::FormatString(
            "Module with name '%s' is already added.", module_name),
        node_embedding_status::kBadArg);
  }

  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedRuntime::OnCreateWrapper(
    node_embedding_create_runtime_wrapper_callback create_wrapper,
    void* create_wrapper_data,
    node_embedding_release_data_callback release_create_wrapper_data) {
  CHECK_ARG_NOT_NULL(create_wrapper);
  ASSERT(!is_initialized_);

  on_create_wrapper_ =
      UniqueFunction<node_embedding_create_runtime_wrapper_callback>{
          create_wrapper, create_wrapper_data, release_create_wrapper_data};

  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedRuntime::GetWrapper(void** result) {
  CHECK_ARG_NOT_NULL(result);
  ASSERT(is_initialized_);

  *result = wrapper_;

  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedRuntime::Initialize(
    node_embedding_configure_runtime_callback configure_runtime,
    void* configure_runtime_data) {
  ASSERT(!is_initialized_);

  if (configure_runtime != nullptr) {
    CHECK_STATUS(configure_runtime(
        configure_runtime_data,
        reinterpret_cast<node_embedding_platform>(platform_),
        reinterpret_cast<node_embedding_runtime_config>(this)));
  }

  is_initialized_ = true;

  node::EnvironmentFlags::Flags flags = GetEnvironmentFlags(
      optional_bits_.flags ? flags_ : node_embedding_runtime_flags::kDefault);

  const std::vector<std::string>& args =
      optional_bits_.args ? args_ : platform_->init_result()->args();

  const std::vector<std::string>& exec_args =
      optional_bits_.exec_args ? exec_args_
                               : platform_->init_result()->exec_args();

  node::MultiIsolatePlatform* v8_platform = platform_->get_v8_platform();

  std::vector<std::string> errors;
  env_setup_ = node::CommonEnvironmentSetup::Create(
      v8_platform, &errors, args, exec_args, flags);

  if (env_setup_ == nullptr || !errors.empty()) {
    return EmbeddedErrorHandling::HandleError(
        errors, node_embedding_status::kGenericError);
  }

  V8ScopeLocker v8_scope_locker(*this);

  std::string filename = args_.size() > 1 ? args_[1] : "<internal>";
  node_api_env_ = GetOrCreateNodeApiEnv(env_setup_->env(), filename);

  node::Environment* node_env = env_setup_->env();

  RegisterModules();

  v8::MaybeLocal<v8::Value> ret =
      node::LoadEnvironment(node_env, start_execution_cb_, preload_cb_);

  if (ret.IsEmpty())
    return EmbeddedErrorHandling::HandleError(
        "Failed to load environment", node_embedding_status::kGenericError);

  if (handle_result_) {
    node_api_env_->CallIntoModule(
        [&](napi_env env) {
          handle_result_(reinterpret_cast<node_embedding_runtime>(this),
                         env,
                         v8impl::JsValueFromV8LocalValue(ret.ToLocalChecked()));
        },
        TriggerFatalException);
  }

  InitializePollingThread();
  WakeupPollingThread();

  return node_embedding_status::kOk;
}

uv_loop_t* EmbeddedRuntime::EventLoop() {
  return env_setup_->env()->event_loop();
}

void EmbeddedRuntime::InitializePollingThread() {
  if (!post_task_) return;

  uv_loop_t* event_loop = EventLoop();

  {
#if defined(_WIN32)

    SYSTEM_INFO system_info = {};
    ::GetNativeSystemInfo(&system_info);

    // on single-core the IO completion port NumberOfConcurrentThreads needs to
    // be 2 to avoid CPU pegging likely caused by a busy loop in PollEvents
    if (system_info.dwNumberOfProcessors == 1) {
      // the expectation is the event_loop has just been initialized
      // which makes IOCP replacement safe
      CHECK_EQ(0u, event_loop->active_handles);
      CHECK_EQ(0u, event_loop->active_reqs.count);

      if (event_loop->iocp && event_loop->iocp != INVALID_HANDLE_VALUE)
        ::CloseHandle(event_loop->iocp);
      event_loop->iocp =
          ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 2);
    }

#elif defined(__APPLE__)

    // Do nothing

#elif defined(__linux)

    int backend_fd = uv_backend_fd(event_loop);
    struct epoll_event ev = {};
    ev.events = EPOLLIN;
    ev.data.fd = backend_fd;
    epoll_ctl(epoll_, EPOLL_CTL_ADD, backend_fd, &ev);

#else
    ERROR_AND_ABORT("The platform is not supported yet.");
#endif
  }

  // keep the loop alive and allow waking up the polling thread
  uv_async_init(event_loop, &polling_async_handle_, nullptr);

  // Start worker thread that will post to the task runner when new uv events
  // arrive.
  polling_thread_closed_ = false;
  uv_sem_init(&polling_sem_, 0);
  uv_thread_create(&polling_thread_, RunPollingThread, this);
}

void EmbeddedRuntime::DestroyPollingThread() {
  if (!post_task_) return;
  if (polling_thread_closed_) return;

  polling_thread_closed_ = true;
  uv_sem_post(&polling_sem_);
  // Wake up polling thread.
  uv_async_send(&polling_async_handle_);
  // Wait for polling thread to complete.
  uv_thread_join(&polling_thread_);

  // Clear uv.
  uv_sem_destroy(&polling_sem_);
  uv_close(reinterpret_cast<uv_handle_t*>(&polling_async_handle_), nullptr);
}

void EmbeddedRuntime::WakeupPollingThread() {
  if (!post_task_) return;
  if (polling_thread_closed_) return;

  uv_sem_post(&polling_sem_);
}

void EmbeddedRuntime::RunPollingThread(void* data) {
  EmbeddedRuntime* runtime = static_cast<EmbeddedRuntime*>(data);
  for (;;) {
    // Wait for the task runner to deal with events.
    uv_sem_wait(&runtime->polling_sem_);
    if (runtime->polling_thread_closed_) break;

    // Wait for something to happen in uv loop.
    runtime->PollEvents();
    if (runtime->polling_thread_closed_) break;

    // Deal with event in the task runner thread.
    runtime->post_task_(
        [](void* task_data) {
          auto* runtime = static_cast<EmbeddedRuntime*>(task_data);
          return runtime->RunEventLoopNoWait(nullptr);
        },
        runtime,
        nullptr);
  }
}

void EmbeddedRuntime::PollEvents() {
  uv_loop_t* event_loop = EventLoop();

  // If there are other kinds of events pending, uv_backend_timeout will
  // instruct us not to wait.
  int timeout = uv_backend_timeout(event_loop);

#if defined(_WIN32)

  DWORD timeout_msec = static_cast<DWORD>(timeout);
  DWORD byte_count;
  ULONG_PTR completion_key;
  OVERLAPPED* overlapped;
  ::GetQueuedCompletionStatus(event_loop->iocp,
                              &byte_count,
                              &completion_key,
                              &overlapped,
                              timeout_msec);

  // Give the event back so libuv can deal with it.
  if (overlapped != nullptr)
    ::PostQueuedCompletionStatus(
        event_loop->iocp, byte_count, completion_key, overlapped);

#elif defined(__APPLE__)

  struct timeval tv;
  if (timeout != -1) {
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
  }

  fd_set readset;
  int fd = uv_backend_fd(event_loop);
  FD_ZERO(&readset);
  FD_SET(fd, &readset);

  // Wait for new libuv events.
  int r;
  do {
    r = select(
        fd + 1, &readset, nullptr, nullptr, timeout == -1 ? nullptr : &tv);
  } while (r == -1 && errno == EINTR);

#elif defined(__linux)

  // Wait for new libuv events.
  int r;
  do {
    struct epoll_event ev;
    r = epoll_wait(epoll_, &ev, 1, timeout);
  } while (r == -1 && errno == EINTR);

#else
  ERROR_AND_ABORT("The platform is not supported yet.");
#endif
}

node_embedding_status EmbeddedRuntime::SetTaskRunner(
    node_embedding_post_task_callback post_task,
    void* post_task_data,
    node_embedding_release_data_callback release_post_task_data) {
  ASSERT(!is_initialized_);
  post_task_ = UniqueFunction<node_embedding_post_task_callback>{
      post_task, post_task_data, release_post_task_data};
  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedRuntime::RunEventLoop() {
  ASSERT(is_initialized_);

  V8ScopeLocker v8_scope_locker(*this);

  DestroyPollingThread();

  int32_t exit_code = node::SpinEventLoop(env_setup_->env()).FromMaybe(1);
  if (exit_code != 0) {
    return EmbeddedErrorHandling::HandleError(
        "Failed while closing the runtime",
        static_cast<node_embedding_status>(
            static_cast<int32_t>(node_embedding_status::kErrorExitCode) +
            exit_code));
  }

  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedRuntime::TerminateEventLoop() {
  ASSERT(is_initialized_);

  V8ScopeLocker v8_scope_locker(*this);
  int32_t exit_code = node::Stop(env_setup_->env(), node::StopFlags::kNoFlags);
  if (exit_code != 0) {
    return EmbeddedErrorHandling::HandleError(
        "Failed while stopping the runtime",
        static_cast<node_embedding_status>(
            static_cast<int32_t>(node_embedding_status::kErrorExitCode) +
            exit_code));
  }
  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedRuntime::RunEventLoopOnce(bool* has_more_work) {
  ASSERT(is_initialized_);

  V8ScopeLocker v8_scope_locker(*this);

  node::ExitCode exit_code =
      node::SpinEventLoopWithoutCleanup(env_setup_->env(), UV_RUN_ONCE)
          .FromMaybe(node::ExitCode::kGenericUserError);
  if (exit_code != node::ExitCode::kNoFailure) {
    return EmbeddedErrorHandling::HandleError(
        "Failed running the event loop",
        static_cast<node_embedding_status>(
            static_cast<int32_t>(node_embedding_status::kErrorExitCode) +
            static_cast<int32_t>(exit_code)));
  }

  if (has_more_work != nullptr) {
    *has_more_work = uv_loop_alive(env_setup_->env()->event_loop());
  }

  WakeupPollingThread();

  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedRuntime::RunEventLoopNoWait(bool* has_more_work) {
  ASSERT(is_initialized_);

  V8ScopeLocker v8_scope_locker(*this);

  node::ExitCode exit_code =
      node::SpinEventLoopWithoutCleanup(env_setup_->env(), UV_RUN_ONCE)
          .FromMaybe(node::ExitCode::kGenericUserError);
  if (exit_code != node::ExitCode::kNoFailure) {
    return EmbeddedErrorHandling::HandleError(
        "Failed running the event loop",
        static_cast<node_embedding_status>(
            static_cast<int32_t>(node_embedding_status::kErrorExitCode) +
            static_cast<int32_t>(exit_code)));
  }

  if (has_more_work != nullptr) {
    *has_more_work = uv_loop_alive(env_setup_->env()->event_loop());
  }

  WakeupPollingThread();

  return node_embedding_status::kOk;
}

/*static*/ void EmbeddedRuntime::TriggerFatalException(
    napi_env env, v8::Local<v8::Value> local_err) {
  node_napi_env__* node_napi_env = static_cast<node_napi_env__*>(env);
  if (node_napi_env->terminatedOrTerminating()) {
    return;
  }
  // If there was an unhandled exception while calling Node-API,
  // report it as a fatal exception. (There is no JavaScript on the
  // call stack that can possibly handle it.)
  node_napi_env->trigger_fatal_exception(local_err);
}

size_t EmbeddedRuntime::OpenV8Scope() {
  if (v8_scope_data_.has_value()) {
    CHECK(v8_scope_data_->IsLocked());
    return v8_scope_data_->IncrementNestLevel();
  }

  v8_scope_data_.emplace(env_setup_.get());
  return 1;
}

void EmbeddedRuntime::CloseV8Scope(size_t nest_level) {
  CHECK(v8_scope_data_.has_value());
  CHECK_EQ(v8_scope_data_->nest_level(), nest_level);
  if (v8_scope_data_->DecrementNestLevel()) {
    v8_scope_data_.reset();
  }
}

node_embedding_status EmbeddedRuntime::RunNodeApi(
    node_embedding_run_node_api_callback run_node_api,
    void* run_node_api_data) {
  CHECK_ARG_NOT_NULL(run_node_api);

  node_embedding_node_api_scope node_api_scope{};
  napi_env env{};
  CHECK_STATUS(OpenNodeApiScope(&node_api_scope, &env));
  auto nodeApiScopeLeave =
      node::OnScopeLeave([&]() { CloseNodeApiScope(node_api_scope); });

  run_node_api(
      run_node_api_data, reinterpret_cast<node_embedding_runtime>(this), env);

  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedRuntime::OpenNodeApiScope(
    node_embedding_node_api_scope* node_api_scope, napi_env* env) {
  CHECK_ARG_NOT_NULL(node_api_scope);
  CHECK_ARG_NOT_NULL(env);

  size_t v8_scope_nest_level = OpenV8Scope();
  node_api_scope_data_.Push(
      {node_api_env_->OpenCallModuleScope(), v8_scope_nest_level});

  *node_api_scope = reinterpret_cast<node_embedding_node_api_scope>(
      node_api_scope_data_.size());
  *env = node_api_env_;
  return node_embedding_status::kOk;
}

node_embedding_status EmbeddedRuntime::CloseNodeApiScope(
    node_embedding_node_api_scope node_api_scope) {
  CHECK_EQ(node_api_scope_data_.size(),
           reinterpret_cast<size_t>(node_api_scope));
  size_t v8_scope_nest_level = node_api_scope_data_.top().v8_scope_nest_level_;

  node_api_env_->CloseCallModuleScope(
      node_api_scope_data_.top().module_scope_data_);
  node_api_scope_data_.Pop();
  CloseV8Scope(v8_scope_nest_level);

  return node_embedding_status::kOk;
}

bool EmbeddedRuntime::IsNodeApiScopeOpened() const {
  return node_api_scope_data_.size() > 0;
}

napi_env EmbeddedRuntime::GetOrCreateNodeApiEnv(
    node::Environment* node_env, const std::string& module_filename) {
  SharedData& shared_data = SharedData::Get();

  {
    std::scoped_lock<std::mutex> lock(shared_data.mutex);
    auto it = shared_data.node_env_to_node_api_env.find(node_env);
    if (it != shared_data.node_env_to_node_api_env.end()) return it->second;
  }

  // Avoid creating the environment under the lock.
  napi_env env = v8impl::NewEnv(node_env->context(),
                                module_filename,
                                EmbeddedPlatform::node_api_version());

  // In case if we cannot insert the new env, we are just going to have an
  // unused env which will be deleted in the end with other environments.
  std::scoped_lock<std::mutex> lock(shared_data.mutex);
  auto insert_result =
      shared_data.node_env_to_node_api_env.try_emplace(node_env, env);

  // Return either the inserted or the existing environment.
  return insert_result.first->second;
}

node::EnvironmentFlags::Flags EmbeddedRuntime::GetEnvironmentFlags(
    node_embedding_runtime_flags flags) {
  uint64_t result = node::EnvironmentFlags::kNoFlags;
  if (embedding::IsFlagSet(flags, node_embedding_runtime_flags::kDefault)) {
    result |= node::EnvironmentFlags::kDefaultFlags;
  }
  if (embedding::IsFlagSet(flags,
                           node_embedding_runtime_flags::kOwnsProcessState)) {
    result |= node::EnvironmentFlags::kOwnsProcessState;
  }
  if (embedding::IsFlagSet(flags,
                           node_embedding_runtime_flags::kOwnsInspector)) {
    result |= node::EnvironmentFlags::kOwnsInspector;
  }
  if (embedding::IsFlagSet(
          flags, node_embedding_runtime_flags::kNoRegisterEsmLoader)) {
    result |= node::EnvironmentFlags::kNoRegisterESMLoader;
  }
  if (embedding::IsFlagSet(flags,
                           node_embedding_runtime_flags::kTrackUnmanagedFds)) {
    result |= node::EnvironmentFlags::kTrackUnmanagedFds;
  }
  if (embedding::IsFlagSet(flags,
                           node_embedding_runtime_flags::kHideConsoleWindows)) {
    result |= node::EnvironmentFlags::kHideConsoleWindows;
  }
  if (embedding::IsFlagSet(flags,
                           node_embedding_runtime_flags::kNoNativeAddons)) {
    result |= node::EnvironmentFlags::kNoNativeAddons;
  }
  if (embedding::IsFlagSet(
          flags, node_embedding_runtime_flags::kNoGlobalSearchPaths)) {
    result |= node::EnvironmentFlags::kNoGlobalSearchPaths;
  }
  if (embedding::IsFlagSet(flags,
                           node_embedding_runtime_flags::kNoBrowserGlobals)) {
    result |= node::EnvironmentFlags::kNoBrowserGlobals;
  }
  if (embedding::IsFlagSet(flags,
                           node_embedding_runtime_flags::kNoCreateInspector)) {
    result |= node::EnvironmentFlags::kNoCreateInspector;
  }
  if (embedding::IsFlagSet(
          flags, node_embedding_runtime_flags::kNoStartDebugSignalHandler)) {
    result |= node::EnvironmentFlags::kNoStartDebugSignalHandler;
  }
  if (embedding::IsFlagSet(
          flags, node_embedding_runtime_flags::kNoWaitForInspectorFrontend)) {
    result |= node::EnvironmentFlags::kNoWaitForInspectorFrontend;
  }
  return static_cast<node::EnvironmentFlags::Flags>(result);
}

void EmbeddedRuntime::RegisterModules() {
  for (const auto& [module_name, module_info] : modules_) {
    node::node_module mod = {
        -1,                                     // nm_version for Node-API
        NM_F_LINKED,                            // nm_flags
        nullptr,                                // nm_dso_handle
        nullptr,                                // nm_filename
        nullptr,                                // nm_register_func
        RegisterModule,                         // nm_context_register_func
        module_name.c_str(),                    // nm_modname
        const_cast<ModuleInfo*>(&module_info),  // nm_priv
        nullptr                                 // nm_link
    };
    node::AddLinkedBinding(env_setup_->env(), mod);
  }
}

/*static*/ void EmbeddedRuntime::RegisterModule(v8::Local<v8::Object> exports,
                                                v8::Local<v8::Value> module,
                                                v8::Local<v8::Context> context,
                                                void* priv) {
  ModuleInfo* module_info = static_cast<ModuleInfo*>(priv);

  // Create a new napi_env for this specific module.
  napi_env env = v8impl::NewEnv(
      context, module_info->module_name, module_info->module_node_api_version);

  napi_value node_api_exports = nullptr;
  env->CallIntoModule([&](napi_env env) {
    return module_info->init_module(module_info->runtime,
                                    env,
                                    module_info->module_name.c_str(),
                                    v8impl::JsValueFromV8LocalValue(exports),
                                    &node_api_exports);
  });

  // If register function returned a non-null exports object different from
  // the exports object we passed it, set that as the "exports" property of
  // the module.
  if (node_api_exports != nullptr &&
      node_api_exports != v8impl::JsValueFromV8LocalValue(exports)) {
    napi_value node_api_module = v8impl::JsValueFromV8LocalValue(module);
    napi_set_named_property(env, node_api_module, "exports", node_api_exports);
  }
}

}  // namespace node

node_embedding_status NAPI_CDECL node_embedding_run_main(
    int32_t argc,
    const char* argv[],
    const node_embedding_version_info* version_info,
    node_embedding_configure_platform_callback configure_platform,
    void* configure_platform_data,
    node_embedding_configure_runtime_callback configure_runtime,
    void* configure_runtime_data) {
  return node::EmbeddedPlatform::RunMain(argc,
                                         argv,
                                         version_info,
                                         configure_platform,
                                         configure_platform_data,
                                         configure_runtime,
                                         configure_runtime_data);
}

node_embedding_status NAPI_CDECL node_embedding_create_platform(
    int32_t argc,
    const char* argv[],
    const node_embedding_version_info* version_info,
    node_embedding_configure_platform_callback configure_platform,
    void* configure_platform_data,
    node_embedding_platform* result) {
  return node::EmbeddedPlatform::Create(argc,
                                        argv,
                                        version_info,
                                        configure_platform,
                                        configure_platform_data,
                                        result);
}

node_embedding_status NAPI_CDECL
node_embedding_delete_platform(node_embedding_platform platform) {
  return EMBEDDED_PLATFORM(platform)->DeleteMe();
}

node_embedding_status NAPI_CDECL node_embedding_set_platform_flags(
    node_embedding_platform_config platform_config,
    node_embedding_platform_flags flags) {
  return EMBEDDED_PLATFORM(platform_config)->SetFlags(flags);
}

node_embedding_status NAPI_CDECL node_embedding_get_platform_parsed_args(
    node_embedding_platform platform,
    node_embedding_get_strings_callback get_args,
    void* get_args_data,
    node_embedding_get_strings_callback get_runtime_args,
    void* get_runtime_args_data) {
  return EMBEDDED_PLATFORM(platform)->GetParsedArgs(
      get_args, get_args_data, get_runtime_args, get_runtime_args_data);
}

node_embedding_status NAPI_CDECL node_embedding_run_runtime(
    node_embedding_platform platform,
    node_embedding_configure_runtime_callback configure_runtime,
    void* configure_runtime_data) {
  return node::EmbeddedRuntime::Run(
      platform, configure_runtime, configure_runtime_data);
}

node_embedding_status NAPI_CDECL node_embedding_create_runtime(
    node_embedding_platform platform,
    node_embedding_configure_runtime_callback configure_runtime,
    void* configure_runtime_data,
    node_embedding_runtime* result) {
  return node::EmbeddedRuntime::Create(
      platform, configure_runtime, configure_runtime_data, result);
}

node_embedding_status NAPI_CDECL
node_embedding_delete_runtime(node_embedding_runtime runtime) {
  return EMBEDDED_RUNTIME(runtime)->DeleteMe();
}

node_embedding_status NAPI_CDECL
node_embedding_set_runtime_flags(node_embedding_runtime_config runtime_config,
                                 node_embedding_runtime_flags flags) {
  return EMBEDDED_RUNTIME(runtime_config)->SetFlags(flags);
}

node_embedding_status NAPI_CDECL
node_embedding_set_runtime_args(node_embedding_runtime_config runtime_config,
                                int32_t argc,
                                const char* argv[],
                                int32_t runtime_argc,
                                const char* runtime_argv[]) {
  return EMBEDDED_RUNTIME(runtime_config)
      ->SetArgs(argc, argv, runtime_argc, runtime_argv);
}

node_embedding_status NAPI_CDECL node_embedding_on_preload_runtime(
    node_embedding_runtime_config runtime_config,
    node_embedding_preload_callback run_preload,
    void* preload_data,
    node_embedding_release_data_callback release_preload_data) {
  return EMBEDDED_RUNTIME(runtime_config)
      ->OnPreload(run_preload, preload_data, release_preload_data);
}

node_embedding_status NAPI_CDECL node_embedding_on_start_runtime_execution(
    node_embedding_runtime_config runtime_config,
    node_embedding_start_execution_callback start_execution,
    void* start_execution_data,
    node_embedding_release_data_callback release_start_execution_data) {
  return EMBEDDED_RUNTIME(runtime_config)
      ->OnStartExecution(
          start_execution, start_execution_data, release_start_execution_data);
}

node_embedding_status NAPI_CDECL node_embedding_on_handle_runtime_start_result(
    node_embedding_runtime_config runtime_config,
    node_embedding_handle_execution_result_callback handle_result,
    void* handle_result_data,
    node_embedding_release_data_callback release_handle_result_data) {
  return EMBEDDED_RUNTIME(runtime_config)
      ->OnHandleStartResult(
          handle_result, handle_result_data, release_handle_result_data);
}

node_embedding_status NAPI_CDECL node_embedding_add_runtime_module(
    node_embedding_runtime_config runtime_config,
    const char* module_name,
    node_embedding_initialize_module_callback init_module,
    void* init_module_data,
    node_embedding_release_data_callback release_init_module_data,
    int32_t module_node_api_version) {
  return EMBEDDED_RUNTIME(runtime_config)
      ->AddModule(module_name,
                  init_module,
                  init_module_data,
                  release_init_module_data,
                  module_node_api_version);
}

node_embedding_status NAPI_CDECL node_embedding_on_create_runtime_wrapper(
    node_embedding_runtime_config runtime_config,
    node_embedding_create_runtime_wrapper_callback create_wrapper,
    void* create_wrapper_data,
    node_embedding_release_data_callback release_create_wrapper_data) {
  return EMBEDDED_RUNTIME(runtime_config)
      ->OnCreateWrapper(
          create_wrapper, create_wrapper_data, release_create_wrapper_data);
}

node_embedding_status NAPI_CDECL node_embedding_get_runtime_wrapper(
    node_embedding_runtime runtime, void** result) {
  return EMBEDDED_RUNTIME(runtime)->GetWrapper(result);
}

node_embedding_status NAPI_CDECL node_embedding_set_runtime_task_runner(
    node_embedding_runtime_config runtime_config,
    node_embedding_post_task_callback post_task,
    void* post_task_data,
    node_embedding_release_data_callback release_post_task_data) {
  return EMBEDDED_RUNTIME(runtime_config)
      ->SetTaskRunner(post_task, post_task_data, release_post_task_data);
}

node_embedding_status NAPI_CDECL
node_embedding_run_event_loop(node_embedding_runtime runtime) {
  return EMBEDDED_RUNTIME(runtime)->RunEventLoop();
}

node_embedding_status NAPI_CDECL
node_embedding_terminate_event_loop(node_embedding_runtime runtime) {
  return EMBEDDED_RUNTIME(runtime)->TerminateEventLoop();
}

node_embedding_status NAPI_CDECL node_embedding_run_event_loop_once(
    node_embedding_runtime runtime, bool* has_more_work) {
  return EMBEDDED_RUNTIME(runtime)->RunEventLoopOnce(has_more_work);
}

node_embedding_status NAPI_CDECL node_embedding_run_event_loop_no_wait(
    node_embedding_runtime runtime, bool* has_more_work) {
  return EMBEDDED_RUNTIME(runtime)->RunEventLoopNoWait(has_more_work);
}

node_embedding_status NAPI_CDECL
node_embedding_run_node_api(node_embedding_runtime runtime,
                            node_embedding_run_node_api_callback run_node_api,
                            void* run_node_api_data) {
  return EMBEDDED_RUNTIME(runtime)->RunNodeApi(run_node_api, run_node_api_data);
}

node_embedding_status NAPI_CDECL node_embedding_open_node_api_scope(
    node_embedding_runtime runtime,
    node_embedding_node_api_scope* node_api_scope,
    napi_env* env) {
  return EMBEDDED_RUNTIME(runtime)->OpenNodeApiScope(node_api_scope, env);
}

node_embedding_status NAPI_CDECL node_embedding_close_node_api_scope(
    node_embedding_runtime runtime,
    node_embedding_node_api_scope node_api_scope) {
  return EMBEDDED_RUNTIME(runtime)->CloseNodeApiScope(node_api_scope);
}
