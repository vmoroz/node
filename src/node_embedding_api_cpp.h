//
// Description: C-based API for embedding Node.js.
//
// !!! WARNING !!! WARNING !!! WARNING !!!
// This is a new API and is subject to change.
// While it is C-based, it is not ABI safe yet.
// Consider all functions and data structures as experimental.
// !!! WARNING !!! WARNING !!! WARNING !!!
//
// This file contains the C-based API for embedding Node.js in a host
// application. The API is designed to be used by applications that want to
// embed Node.js as a shared library (.so or .dll) and can interop with
// C-based API.
//

#ifndef SRC_NODE_EMBEDDING_API_CPP_H_
#define SRC_NODE_EMBEDDING_API_CPP_H_

//==============================================================================
// The C++ wrappers for the Node.js embedding API.
//==============================================================================

#include "node_embedding_api.h"

#include <array>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace node::embedding {

//==============================================================================
// C++ convenience functions for the C API.
// These functions are not ABI safe and can be changed in future versions.
//==============================================================================

// Move-only pointer wrapper.
// The class does not own the pointer and does not delete it.
// It simplifies implementation of the C++ API classes that wrap pointers.
template <typename TPointer>
class NodePointer {
 public:
  NodePointer() = default;

  explicit NodePointer(TPointer ptr) : ptr_(ptr) {}

  NodePointer(const NodePointer&) = delete;
  NodePointer& operator=(const NodePointer&) = delete;

  NodePointer(NodePointer&& other) noexcept
      : ptr_(std::exchange(other.ptr_, nullptr)) {}

  NodePointer& operator=(NodePointer&& other) noexcept {
    if (this != &other) {
      ptr_ = std::exchange(other.ptr_, nullptr);
    }
    return *this;
  }

  NodePointer& operator=(std::nullptr_t) {
    ptr_ = nullptr;
    return *this;
  }

  TPointer ptr() const { return ptr_; }

  explicit operator bool() const { return ptr_ != nullptr; }

 private:
  TPointer ptr_{};
};

template <typename T>
class [[nodiscard]] NodeExpected {
 public:
  explicit NodeExpected(T value) : value_(std::move(value)) {}

  explicit NodeExpected(NodeStatus status) : status_(status) {}

  NodeExpected(const NodeExpected&) = delete;
  NodeExpected& operator=(const NodeExpected&) = delete;

  NodeExpected(NodeExpected&& other) noexcept : status_(other.status_) {
    if (other.has_value()) {
      new (std::addressof(value_)) T(std::move(other.value_));
    }
  }

  NodeExpected& operator=(NodeExpected&& other) noexcept {
    if (this != &other) {
      if (has_value()) {
        value_.~T();
      }
      status_ = other.status_;
      if (other.has_value()) {
        new (std::addressof(value_)) T(std::move(other.value_));
      }
    }
    return *this;
  }

  ~NodeExpected() {
    if (has_value()) {
      value_.~T();
    }
  }

  bool has_value() const { return status_ == NodeStatus::kOk; }
  bool has_error() const { return status_ != NodeStatus::kOk; }

  T& value() & { return value_; }
  const T& value() const& { return value_; }
  T&& value() && { return std::move(value_); }
  const T&& value() const&& { return std::move(value_); }

  NodeStatus status() const { return status_; }

  int32_t exit_code() const {
    if (status_ == NodeStatus::kOk) {
      return 0;
    } else if ((static_cast<int32_t>(status_) &
                static_cast<int32_t>(NodeStatus::kErrorExitCode)) != 0) {
      return static_cast<int32_t>(status_) &
             ~static_cast<int32_t>(NodeStatus::kErrorExitCode);
    }
    return 1;
  }

 private:
  NodeStatus status_{NodeStatus::kOk};
  union {
    T value_;  // The value is uninitialized if status_ is not kOk.
    char padding_[sizeof(T)];
  };
};

template <>
class [[nodiscard]] NodeExpected<void> {
 public:
  NodeExpected() = default;

  explicit NodeExpected(NodeStatus status) : status_(status) {}

  NodeExpected(const NodeExpected&) = delete;
  NodeExpected& operator=(const NodeExpected&) = delete;

  NodeExpected(NodeExpected&& other) = default;
  NodeExpected& operator=(NodeExpected&& other) = default;

  bool has_value() const { return status_ == NodeStatus::kOk; }
  bool has_error() const { return status_ != NodeStatus::kOk; }

  NodeStatus status() const { return status_; }

  int32_t exit_code() const {
    if (status_ == NodeStatus::kOk) {
      return 0;
    } else if ((static_cast<int32_t>(status_) &
                static_cast<int32_t>(NodeStatus::kErrorExitCode)) != 0) {
      return static_cast<int32_t>(status_) &
             ~static_cast<int32_t>(NodeStatus::kErrorExitCode);
    }
    return 1;
  }

  template <typename T>
  NodeExpected<void> AndThen(T&& lambda) && {
    return (status_ == NodeStatus::kOk) ? lambda() : std::move(*this);
  }

 private:
  NodeStatus status_{NodeStatus::kOk};
};

template <typename T>
NodeExpected<T> operator&&(NodeStatus status, NodeExpected<T> success_value) {
  if (status != NodeStatus::kOk) {
    return NodeExpected<T>(status);
  }
  return success_value;
}

template <typename T>
NodeExpected<T> operator&&(NodeExpected<void> expected,
                           NodeExpected<T> success_value) {
  if (expected.has_error()) {
    return NodeExpected<T>(expected.status());
  }
  return success_value;
}

// A helper class to convert std::vector<std::string> to an array of C strings.
// If the number of strings is less than kInplaceBufferSize, the strings are
// stored in the inplace_buffer_ array. Otherwise, the strings are stored in the
// allocated_buffer_ array.
// Ideally the class must be allocated on the stack.
// In any case it must not outlive the passed vector since it keeps only the
// string pointers returned by std::string::c_str() method.
template <size_t kInplaceBufferSize = 32>
class NodeCStringArray {
 public:
  explicit NodeCStringArray(const std::vector<std::string>& strings) noexcept
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

  NodeCStringArray(const NodeCStringArray&) = delete;
  NodeCStringArray& operator=(const NodeCStringArray&) = delete;

  int32_t size() const { return static_cast<int32_t>(size_); }
  const char** c_strs() const { return c_strs_; }

 private:
  size_t size_{};
  const char** c_strs_{};
  std::array<const char*, kInplaceBufferSize> inplace_buffer_;
  std::unique_ptr<const char*[]> allocated_buffer_;
};

// Wraps command line arguments.
class NodeArgs {
 public:
  NodeArgs(int32_t argc, const char* argv[]) : argc_(argc), argv_(argv) {}

  NodeArgs(int32_t argc, char* argv[])
      : argc_(argc), argv_(const_cast<const char**>(argv)) {}

  NodeArgs(const NodeCStringArray<>& string_array_view)
      : argc_(string_array_view.size()), argv_(string_array_view.c_strs()) {}

  int32_t argc() const { return argc_; }
  const char** argv() const { return argv_; }

 private:
  int32_t argc_{};
  const char** argv_{};
};

template <typename TCallback, typename TFunctor, typename TEnable = void>
class NodeFunctorInvoker;

template <typename TCallback>
class NodeFunctorRef;

template <typename TResult, typename... TArgs>
class NodeFunctorRef<TResult (*)(void*, TArgs...)> {
  using TCallback = TResult (*)(void*, TArgs...);

 public:
  NodeFunctorRef(std::nullptr_t) {}

  NodeFunctorRef(TCallback callback, void* data)
      : callback_(callback), data_(data) {}

  template <typename TFunctor>
  NodeFunctorRef(TFunctor&& functor)
      : callback_(&NodeFunctorInvoker<TCallback, TFunctor>::Invoke),
        data_(&functor) {}

  NodeFunctorRef(NodeFunctorRef&& other) = default;
  NodeFunctorRef& operator=(NodeFunctorRef&& other) = default;

  TCallback callback() const { return callback_.ptr(); }

  void* data() const { return data_.ptr(); }

  explicit operator bool() const { return static_cast<bool>(callback_); }

 private:
  NodePointer<TCallback> callback_;
  NodePointer<void*> data_;
};

template <typename TCallback>
class NodeFunctor;

template <typename TResult, typename... TArgs>
class NodeFunctor<TResult (*)(void*, TArgs...)> {
  using TCallback = TResult (*)(void*, TArgs...);

 public:
  NodeFunctor() = default;
  NodeFunctor(std::nullptr_t) {}

  NodeFunctor(TCallback callback,
              void* data,
              node_embedding_release_data_callback data_release)
      : callback_(callback), data_(data), data_release_(data_release) {}

  // TODO: add overload for stateless lambdas.
  template <typename TFunctor>
  NodeFunctor(TFunctor&& functor)
      : callback_(&NodeFunctorInvoker<TCallback, TFunctor>::Invoke),
        data_(new TFunctor(std::forward<TFunctor>(functor))),
        data_release_(&ReleaseFunctor<TFunctor>) {}

  NodeFunctor(NodeFunctor&& other) = default;
  NodeFunctor& operator=(NodeFunctor&& other) = default;

  TCallback callback() const { return callback_.ptr(); }

  void* data() const { return data_.ptr(); }

  node_embedding_release_data_callback data_release() const {
    return data_release_.ptr();
  }

  explicit operator bool() const { return static_cast<bool>(callback_); }

  TResult operator()(TArgs... args) const {
    return (*callback_.ptr())(data_.ptr(), args...);
  }

 private:
  template <typename TFunctor>
  static NodeStatus ReleaseFunctor(void* data) {
    // TODO: Handle exceptions.
    delete reinterpret_cast<TFunctor*>(data);
    return NodeStatus::kOk;
  }

 private:
  NodePointer<TCallback> callback_;
  NodePointer<void*> data_;
  NodePointer<node_embedding_release_data_callback> data_release_;
};

// NodeGetStringsCallback supported signatures:
// - NodeExpected<void>(int32_t strings_size, const char* strings[]);
// - NodeExpected<void>(std::vector<std::string> strings);
using NodeGetStringsCallback =
    NodeFunctorRef<node_embedding_get_strings_callback>;

// NodeConfigurePlatformCallback supported signatures:
// - NodeExpected<void>(const NodePlatformConfig& platform_config);
using NodeConfigurePlatformCallback =
    NodeFunctorRef<node_embedding_configure_platform_callback>;

// NodeGetStringsCallback supported signatures:
// - NodeExpected<void>(int32_t strings_size, const char* strings[]);
// - NodeExpected<void>(std::vector<std::string> strings);
using NodeEarlyReturnCallback =
    NodeFunctor<node_embedding_get_strings_callback>;

// NodeConfigureRuntimeCallback supported signatures:
// - NodeExpected<void>(const NodePlatform& platform,
//                      const NodeRuntimeConfig& runtime_config);
using NodeConfigureRuntimeCallback =
    NodeFunctorRef<node_embedding_configure_runtime_callback>;

// NodePreloadCallback supported signatures:
// - void(const NodeRuntime& runtime,
//        napi_env env,
//        napi_value process,
//        napi_value require);
using NodePreloadCallback = NodeFunctor<node_embedding_preload_callback>;

// NodeStartExecutionCallback supported signatures:
// - napi_value(const NodeRuntime& runtime,
//              napi_env env,
//              napi_value process,
//              napi_value require,
//              napi_value run_cjs);
using NodeStartExecutionCallback =
    NodeFunctor<node_embedding_start_execution_callback>;

// NodeHandleExecutionResultCallback supported signatures:
// - void(const NodeRuntime& runtime,
//        napi_env env,
//        napi_value execution_result);
using NodeHandleExecutionResultCallback =
    NodeFunctor<node_embedding_handle_execution_result_callback>;

// NodeInitializeModuleCallback supported signatures:
// - napi_value(const NodeRuntime& runtime,
//              napi_env env,
//              std::string_view module_name,
//              napi_value exports);
using NodeInitializeModuleCallback =
    NodeFunctor<node_embedding_initialize_module_callback>;

// NodeRunTaskCallback supported signatures:
// - NodeExpected<void>();
using NodeRunTaskCallback = NodeFunctor<node_embedding_run_task_callback>;

// NodePostTaskCallback supported signatures:
// - NodeExpected<bool>(NodeRunTaskCallback run_task);
using NodePostTaskCallback = NodeFunctor<node_embedding_post_task_callback>;

// NodeRunNodeApiCallback supported signatures:
// - void(const NodeRuntime& runtime, napi_env env);
using NodeRunNodeApiCallback =
    NodeFunctorRef<node_embedding_run_node_api_callback>;

inline std::string NodeFormatString(const char* format, ...) {
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

class NodeErrorInfo {
 public:
  static NodeExpected<void> GetLastErrorMessage(
      NodeGetStringsCallback get_message) {
    return NodeExpected<void>(node_embedding_get_last_error_message(
        get_message.callback(), get_message.data()));
  }

  static NodeExpected<std::vector<std::string>> GetLastErrorMessage() {
    std::vector<std::string> result_message;
    return GetLastErrorMessage(
               [&result_message](std::vector<std::string> message) {
                 result_message = std::move(message);
                 return NodeExpected<void>();
               }) &&
           NodeExpected<std::vector<std::string>>(std::move(result_message));
  }

  static NodeExpected<void> SetLastErrorMessage(int32_t message_strings_size,
                                                const char* message_strings[]) {
    return NodeExpected<void>(node_embedding_set_last_error_message(
        message_strings_size, message_strings));
  }

  static NodeExpected<void> SetLastErrorMessage(std::string_view message) {
    const char* message_data = message.data();
    return SetLastErrorMessage(1, &message_data);
  }

  static NodeExpected<void> SetLastErrorMessage(std::string_view message,
                                                std::string_view filename,
                                                int32_t line) {
    return SetLastErrorMessage(NodeFormatString(
        "Error: %s at %s:%d", message.data(), filename.data(), line));
  }

  static NodeExpected<void> SetLastErrorMessage(
      const std::vector<std::string>& message) {
    NodeCStringArray message_strings(message);
    return SetLastErrorMessage(message_strings.size(),
                               message_strings.c_strs());
  }

  static NodeExpected<void> ClearLastErrorMessage() {
    return NodeExpected<void>(node_embedding_clear_last_error_message());
  }

  static NodeExpected<std::vector<std::string>> GetAndClearLastErrorMessage() {
    auto expected_message = GetLastErrorMessage();
    if (expected_message.has_error()) {
      return expected_message;
    }
    auto expected_clear = ClearLastErrorMessage();
    if (expected_clear.has_error()) {
      return NodeExpected<std::vector<std::string>>(expected_clear.status());
    }
    return expected_message;
  }
};

class NodeScopedErrorHandler {
 public:
  static void SetStatus(NodeStatus status) {
    if (status == NodeStatus::kOk) return;
    NodeScopedErrorHandler* current_handler = Current();
    if (current_handler != nullptr) {
      napi_fatal_error("NodeScopedErrorHandler::SetStatus",
                       NAPI_AUTO_LENGTH,
                       "NodeScopedErrorHandler is not found om the stack.",
                       NAPI_AUTO_LENGTH);
    }
    current_handler->SetStatusInternal(status);
  }

  NodeScopedErrorHandler() {}

  ~NodeScopedErrorHandler() {
    if (status_.has_value()) {
      napi_fatal_error("NodeScopedErrorHandler::~NodeScopedErrorHandler",
                       NAPI_AUTO_LENGTH,
                       "NodeScopedErrorHandler status is not read and cleared.",
                       NAPI_AUTO_LENGTH);
    }
    Current() = previous_handler_;
  }

  NodeStatus GetAndClearStatus() {
    NodeStatus result_status = status_.value_or(NodeStatus::kOk);
    status_.reset();
    return result_status;
  }

 private:
  static NodeScopedErrorHandler*& Current() {
    static thread_local NodeScopedErrorHandler* current_handler = nullptr;
    return current_handler;
  }

  void SetStatusInternal(NodeStatus status) {
    if (status_ != NodeStatus::kOk) return;
    status_ = status;
    error_message_ = NodeErrorInfo::GetLastErrorMessage().value();
  }

 private:
  std::optional<NodeStatus> status_;
  std::vector<std::string> error_message_;
  NodeScopedErrorHandler* previous_handler_{Current()};
};

// Wraps the Node.js platform instance.
class NodePlatform {
 public:
  explicit NodePlatform(node_embedding_platform platform)
      : platform_(platform) {}

  NodePlatform(NodePlatform&& other) = default;
  NodePlatform& operator=(NodePlatform&& other) = default;

  ~NodePlatform() {
    if (platform_) {
      NodeScopedErrorHandler::SetStatus(
          node_embedding_delete_platform(platform_.ptr()));
    }
  }

  explicit operator bool() const { return static_cast<bool>(platform_); }

  operator node_embedding_platform() const { return platform_.ptr(); }

  node_embedding_platform Detach() {
    return std::exchange(platform_, nullptr).ptr();
  }

  static NodeExpected<void> RunMain(
      NodeArgs args,
      NodeConfigurePlatformCallback configure_platform,
      NodeConfigureRuntimeCallback configure_runtime) {
    return node_embedding_run_main(NODE_EMBEDDING_VERSION,
                                   args.argc(),
                                   args.argv(),
                                   configure_platform.callback(),
                                   configure_platform.data(),
                                   configure_runtime.callback(),
                                   configure_runtime.data()) &&
           NodeExpected<void>();
  }

  static NodeExpected<NodePlatform> Create(
      NodeArgs args, NodeConfigurePlatformCallback configure_platform) {
    node_embedding_platform platform;
    return node_embedding_create_platform(NODE_EMBEDDING_VERSION,
                                          args.argc(),
                                          args.argv(),
                                          configure_platform.callback(),
                                          configure_platform.data(),
                                          &platform) &&
           NodeExpected<NodePlatform>(NodePlatform(platform));
  }

  NodeExpected<void> GetParsedArgs(
      NodeGetStringsCallback get_args,
      NodeGetStringsCallback get_runtime_args) const {
    return node_embedding_get_platform_parsed_args(platform_.ptr(),
                                                   get_args.callback(),
                                                   get_args.data(),
                                                   get_runtime_args.callback(),
                                                   get_runtime_args.data()) &&
           NodeExpected<void>();
  }

  NodeExpected<std::vector<std::string>> GetArgs() const {
    std::vector<std::string> result_args;
    return GetParsedArgs(
               [&result_args](std::vector<std::string> args) {
                 result_args = std::move(args);
                 return NodeExpected<void>();
               },
               nullptr) &&
           NodeExpected<std::vector<std::string>>(std::move(result_args));
  }

  NodeExpected<std::vector<std::string>> GetRuntimeArgs() const {
    std::vector<std::string> result_args;
    return GetParsedArgs(nullptr,
                         [&result_args](std::vector<std::string> args) {
                           result_args = std::move(args);
                           return NodeExpected<void>();
                         }) &&
           NodeExpected<std::vector<std::string>>(std::move(result_args));
  }

 private:
  NodePointer<node_embedding_platform> platform_;
};

// The NodePlatform that does not delete the platform on destruction.
class NodeDetachedPlatform : public NodePlatform {
 public:
  explicit NodeDetachedPlatform(node_embedding_platform platform)
      : NodePlatform(platform) {}

  ~NodeDetachedPlatform() { Detach(); }
};

class NodePlatformConfig {
 public:
  explicit NodePlatformConfig(node_embedding_platform_config platform_config)
      : platform_config_(platform_config) {}

  NodePlatformConfig(NodePlatformConfig&& other) = default;
  NodePlatformConfig& operator=(NodePlatformConfig&& other) = default;

  operator node_embedding_platform_config() const {
    return platform_config_.ptr();
  }

  NodeExpected<void> SetFlags(NodePlatformFlags flags) const {
    return node_embedding_set_platform_flags(platform_config_.ptr(), flags) &&
           NodeExpected<void>();
  }

  NodeExpected<void> OnEarlyReturn(
      NodeEarlyReturnCallback early_return_handler) const {
    return node_embedding_on_early_return(
               platform_config_.ptr(),
               early_return_handler.callback(),
               early_return_handler.data(),
               early_return_handler.data_release()) &&
           NodeExpected<void>();
  }

 private:
  NodePointer<node_embedding_platform_config> platform_config_{};
};

class NodeApiScope {
 public:
  static NodeExpected<NodeApiScope> Open(node_embedding_runtime runtime) {
    node_embedding_node_api_scope node_api_scope{};
    napi_env env{};
    return node_embedding_open_node_api_scope(runtime, &node_api_scope, &env) &&
           NodeExpected<NodeApiScope>(
               NodeApiScope(runtime, node_api_scope, env));
  }

  explicit NodeApiScope(node_embedding_runtime runtime,
                        node_embedding_node_api_scope node_api_scope,
                        napi_env env)
      : runtime_(runtime), node_api_scope_(node_api_scope), env_(env) {}

  NodeApiScope(NodeApiScope&&) = default;
  NodeApiScope& operator=(NodeApiScope&&) = default;

  ~NodeApiScope() {
    if (runtime_) {
      NodeScopedErrorHandler::SetStatus(node_embedding_close_node_api_scope(
          runtime_.ptr(), node_api_scope_.ptr()));
    }
  }

  napi_env env() const { return env_.ptr(); }

 private:
  NodePointer<node_embedding_runtime> runtime_;
  NodePointer<node_embedding_node_api_scope> node_api_scope_;
  NodePointer<napi_env> env_;
};

class NodeRuntime {
 public:
  static NodeExpected<void> Run(
      const NodePlatform& platform,
      NodeConfigureRuntimeCallback configure_runtime) {
    return node_embedding_run_runtime(
               static_cast<node_embedding_platform>(platform),
               configure_runtime.callback(),
               configure_runtime.data()) &&
           NodeExpected<void>();
  }

  static NodeExpected<NodeRuntime> Create(
      NodePlatform platform, NodeConfigureRuntimeCallback configure_runtime) {
    node_embedding_runtime runtime;
    return node_embedding_create_runtime(platform,
                                         configure_runtime.callback(),
                                         configure_runtime.data(),
                                         &runtime) &&
           NodeExpected<NodeRuntime>(NodeRuntime(runtime));
  }

  explicit NodeRuntime(node_embedding_runtime runtime) : runtime_(runtime) {}

  NodeRuntime(NodeRuntime&& other) = default;
  NodeRuntime& operator=(NodeRuntime&& other) = default;

  ~NodeRuntime() {
    if (runtime_) {
      NodeScopedErrorHandler::SetStatus(
          node_embedding_delete_runtime(runtime_.ptr()));
      ;
    }
  }

  operator node_embedding_runtime() const { return runtime_.ptr(); }

  node_embedding_runtime Detach() {
    return std::exchange(runtime_, nullptr).ptr();
  }

  NodeExpected<void> RunEventLoop() const {
    return node_embedding_run_event_loop(runtime_.ptr()) &&
           NodeExpected<void>();
  }

  NodeExpected<void> TerminateEventLoop() const {
    return node_embedding_terminate_event_loop(runtime_.ptr()) &&
           NodeExpected<void>();
  }

  NodeExpected<bool> RunEventLoopOnce() const {
    bool has_more_work{};
    return node_embedding_run_event_loop_once(runtime_.ptr(), &has_more_work) &&
           NodeExpected<bool>(has_more_work);
  }

  NodeExpected<bool> RunEventLoopNoWait() const {
    bool has_more_work{};
    return node_embedding_run_event_loop_no_wait(runtime_.ptr(),
                                                 &has_more_work) &&
           NodeExpected<bool>(has_more_work);
  }

  NodeExpected<void> RunNodeApi(NodeRunNodeApiCallback run_node_api) const {
    return node_embedding_run_node_api(
               runtime_.ptr(), run_node_api.callback(), run_node_api.data()) &&
           NodeExpected<void>();
  }

  NodeExpected<NodeApiScope> OpenNodeApiScope() const {
    return NodeApiScope::Open(runtime_.ptr());
  }

 private:
  NodePointer<node_embedding_runtime> runtime_{};
};

class NodeDetachedRuntime : public NodeRuntime {
 public:
  explicit NodeDetachedRuntime(node_embedding_runtime runtime)
      : NodeRuntime(runtime) {}
  ~NodeDetachedRuntime() { Detach(); }
};

class NodeRuntimeConfig {
 public:
  explicit NodeRuntimeConfig(node_embedding_runtime_config runtime_config)
      : runtime_config_(runtime_config) {}

  NodeRuntimeConfig(NodeRuntimeConfig&& other) = default;
  NodeRuntimeConfig& operator=(NodeRuntimeConfig&& other) = default;

  operator node_embedding_runtime_config() const {
    return runtime_config_.ptr();
  }

  NodeExpected<void> SetNodeApiVersion(int32_t node_api_version) const {
    return node_embedding_set_runtime_node_api_version(runtime_config_.ptr(),
                                                       node_api_version) &&
           NodeExpected<void>();
  }

  NodeExpected<void> SetFlags(NodeRuntimeFlags flags) const {
    return node_embedding_set_runtime_flags(runtime_config_.ptr(), flags) &&
           NodeExpected<void>();
  }

  NodeExpected<void> SetArgs(NodeArgs args, NodeArgs runtime_args) const {
    return node_embedding_set_runtime_args(runtime_config_.ptr(),
                                           args.argc(),
                                           args.argv(),
                                           runtime_args.argc(),
                                           runtime_args.argv()) &&
           NodeExpected<void>();
  }

  NodeExpected<void> OnPreload(NodePreloadCallback preload) const {
    return node_embedding_on_preload_runtime(runtime_config_.ptr(),
                                             preload.callback(),
                                             preload.data(),
                                             preload.data_release()) &&
           NodeExpected<void>();
  }

  NodeExpected<void> OnStartExecution(
      NodeStartExecutionCallback start_execution) const {
    return node_embedding_on_start_runtime_execution(
               runtime_config_.ptr(),
               start_execution.callback(),
               start_execution.data(),
               start_execution.data_release()) &&
           NodeExpected<void>();
  }

  NodeExpected<void> OnHandleExecutionResult(
      NodeHandleExecutionResultCallback handle_start_result) const {
    return node_embedding_on_handle_runtime_execution_result(
               runtime_config_.ptr(),
               handle_start_result.callback(),
               handle_start_result.data(),
               handle_start_result.data_release()) &&
           NodeExpected<void>();
  }

  NodeExpected<void> AddModule(std::string_view module_name,
                               NodeInitializeModuleCallback init_module,
                               int32_t moduleNodeApiVersion) const {
    return node_embedding_add_runtime_module(runtime_config_.ptr(),
                                             module_name.data(),
                                             init_module.callback(),
                                             init_module.data(),
                                             init_module.data_release(),
                                             moduleNodeApiVersion) &&
           NodeExpected<void>();
  }

  NodeExpected<void> SetTaskRunner(NodePostTaskCallback post_task) const {
    return node_embedding_set_runtime_task_runner(runtime_config_.ptr(),
                                                  post_task.callback(),
                                                  post_task.data(),
                                                  post_task.data_release()) &&
           NodeExpected<void>();
  }

 private:
  NodePointer<node_embedding_runtime_config> runtime_config_{};
};

template <typename TFunctor>
class NodeFunctorInvoker<
    node_embedding_get_strings_callback,
    TFunctor,
    std::enable_if_t<std::is_invocable_r_v<NodeExpected<void>,
                                           TFunctor,
                                           int32_t,
                                           const char**>>> {
 public:
  static NodeStatus Invoke(void* cb_data,
                           int32_t strings_size,
                           const char* strings[]) {
    TFunctor* callback = reinterpret_cast<TFunctor*>(cb_data);
    NodeExpected<void> result_cpp = (*callback)(strings_size, strings);
    return result_cpp.status();
  }
};

template <typename TFunctor>
class NodeFunctorInvoker<
    node_embedding_get_strings_callback,
    TFunctor,
    std::enable_if_t<std::is_invocable_r_v<NodeExpected<void>,
                                           TFunctor,
                                           std::vector<std::string>>>> {
 public:
  static NodeStatus Invoke(void* cb_data,
                           int32_t strings_size,
                           const char* strings[]) {
    TFunctor* callback = reinterpret_cast<TFunctor*>(cb_data);
    std::vector<std::string> strings_cpp(strings, strings + strings_size);
    NodeExpected<void> result_cpp = (*callback)(std::move(strings_cpp));
    return result_cpp.status();
  }
};

template <typename TFunctor>
class NodeFunctorInvoker<
    node_embedding_configure_platform_callback,
    TFunctor,
    std::enable_if_t<std::is_invocable_r_v<NodeExpected<void>,
                                           TFunctor,
                                           const NodePlatformConfig&>>> {
 public:
  static NodeStatus Invoke(void* cb_data,
                           node_embedding_platform_config platform_config) {
    TFunctor* callback = reinterpret_cast<TFunctor*>(cb_data);
    NodePlatformConfig platform_config_cpp(platform_config);
    NodeExpected<void> result_cpp = (*callback)(platform_config_cpp);
    return result_cpp.status();
  }
};

template <typename TFunctor>
class NodeFunctorInvoker<
    node_embedding_configure_runtime_callback,
    TFunctor,
    std::enable_if_t<std::is_invocable_r_v<NodeExpected<void>,
                                           TFunctor,
                                           const NodePlatform&,
                                           const NodeRuntimeConfig&>>> {
 public:
  static NodeStatus Invoke(void* cb_data,
                           node_embedding_platform platform,
                           node_embedding_runtime_config runtime_config) {
    TFunctor* callback = reinterpret_cast<TFunctor*>(cb_data);
    NodeDetachedPlatform platform_cpp(platform);
    NodeRuntimeConfig runtime_config_cpp(runtime_config);
    NodeExpected<void> result_cpp =
        (*callback)(platform_cpp, runtime_config_cpp);
    return result_cpp.status();
  }
};

template <typename TFunctor>
class NodeFunctorInvoker<
    node_embedding_preload_callback,
    TFunctor,
    std::enable_if_t<std::is_invocable_r_v<void,
                                           TFunctor,
                                           const NodeRuntime&,
                                           napi_env,
                                           napi_value,
                                           napi_value>>> {
 public:
  static void Invoke(void* cb_data,
                     node_embedding_runtime runtime,
                     napi_env env,
                     napi_value process,
                     napi_value require) {
    TFunctor* callback = reinterpret_cast<TFunctor*>(cb_data);
    NodeDetachedRuntime runtime_cpp(runtime);
    (*callback)(runtime_cpp, env, process, require);
  }
};

template <typename TFunctor>
class NodeFunctorInvoker<
    node_embedding_start_execution_callback,
    TFunctor,
    std::enable_if_t<std::is_invocable_r_v<napi_value,
                                           TFunctor,
                                           const NodeRuntime&,
                                           napi_env,
                                           napi_value,
                                           napi_value,
                                           napi_value>>> {
 public:
  static napi_value Invoke(void* cb_data,
                           node_embedding_runtime runtime,
                           napi_env env,
                           napi_value process,
                           napi_value require,
                           napi_value run_cjs) {
    TFunctor* callback = reinterpret_cast<TFunctor*>(cb_data);
    NodeDetachedRuntime runtime_cpp(runtime);
    return (*callback)(runtime_cpp, env, process, require, run_cjs);
  }
};

template <typename TFunctor>
class NodeFunctorInvoker<
    node_embedding_handle_execution_result_callback,
    TFunctor,
    std::enable_if_t<std::is_invocable_r_v<void,
                                           TFunctor,
                                           const NodeRuntime&,
                                           napi_env,
                                           napi_value>>> {
 public:
  static void Invoke(void* cb_data,
                     node_embedding_runtime runtime,
                     napi_env env,
                     napi_value execution_result) {
    TFunctor* callback = reinterpret_cast<TFunctor*>(cb_data);
    NodeDetachedRuntime runtime_cpp(runtime);
    (*callback)(runtime_cpp, env, execution_result);
  }
};

template <typename TFunctor>
class NodeFunctorInvoker<
    node_embedding_initialize_module_callback,
    TFunctor,
    std::enable_if_t<std::is_invocable_r_v<napi_value,
                                           TFunctor,
                                           const NodeRuntime&,
                                           napi_env,
                                           std::string_view,
                                           napi_value>>> {
 public:
  static napi_value Invoke(void* cb_data,
                           node_embedding_runtime runtime,
                           napi_env env,
                           const char* module_name,
                           napi_value exports) {
    TFunctor* callback = reinterpret_cast<TFunctor*>(cb_data);
    NodeDetachedRuntime runtime_cpp(runtime);
    return (*callback)(runtime_cpp, env, module_name, exports);
  }
};

template <typename TFunctor>
class NodeFunctorInvoker<
    node_embedding_run_task_callback,
    TFunctor,
    std::enable_if_t<std::is_invocable_r_v<NodeExpected<void>, TFunctor>>> {
 public:
  static node_embedding_status Invoke(void* cb_data) {
    TFunctor* callback = reinterpret_cast<TFunctor*>(cb_data);
    return (*callback)().status();
  }
};

template <typename TFunctor>
class NodeFunctorInvoker<
    node_embedding_post_task_callback,
    TFunctor,
    std::enable_if_t<std::is_invocable_r_v<NodeExpected<bool>,
                                           TFunctor,
                                           NodeRunTaskCallback>>> {
 public:
  static node_embedding_status Invoke(
      void* cb_data,
      node_embedding_run_task_callback run_task,
      void* task_data,
      node_embedding_release_data_callback release_task_data,
      bool* succeeded) {
    TFunctor* callback = reinterpret_cast<TFunctor*>(cb_data);
    NodeExpected<bool> result_cpp = (*callback)(
        NodeRunTaskCallback(run_task, task_data, release_task_data));
    if (succeeded != nullptr) {
      *succeeded = result_cpp.value();
    }
    return result_cpp.status();
  }
};

template <typename TFunctor>
class NodeFunctorInvoker<
    node_embedding_run_node_api_callback,
    TFunctor,
    std::enable_if_t<
        std::is_invocable_r_v<void, TFunctor, const NodeRuntime&, napi_env>>> {
 public:
  static void Invoke(void* cb_data,
                     node_embedding_runtime runtime,
                     napi_env env) {
    TFunctor* callback = reinterpret_cast<TFunctor*>(cb_data);
    NodeDetachedRuntime runtime_cpp(runtime);
    (*callback)(runtime_cpp, env);
  }
};

}  // namespace node::embedding

#endif  // SRC_NODE_EMBEDDING_API_CPP_H_
