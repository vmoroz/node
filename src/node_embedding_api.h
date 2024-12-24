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

#ifndef SRC_NODE_EMBEDDING_API_H_
#define SRC_NODE_EMBEDDING_API_H_

#include "node_api.h"

#define NODE_EMBEDDING_VERSION 1

#ifdef __cplusplus

#define NODE_OPTIONS(c_name, cpp_name)                                         \
  enum class cpp_name : int32_t cpp_name;                                      \
  inline constexpr cpp_name operator|(cpp_name lhs, cpp_name rhs) {            \
    return static_cast<cpp_name>(static_cast<int32_t>(lhs) |                   \
                                 static_cast<int32_t>(rhs));                   \
  }                                                                            \
  inline constexpr bool is_option_set(cpp_name flags, cpp_name flag) {         \
    return (static_cast<int32_t>(flags) & static_cast<int32_t>(flag)) != 0;    \
  }                                                                            \
  enum class cpp_name : int32_t

#define NODE_OPTION(c_name, cpp_name) cpp_name

#define NODE_ENUM(c_name, cpp_name)                                            \
  enum class cpp_name : int32_t cpp_name;                                      \
  enum class cpp_name : int32_t

#define NODE_ENUM_ITEM(c_name, cpp_name) cpp_name

#else

#define NODE_OPTIONS(c_name, cpp_name)                                         \
  enum c_name c_name;                                                          \
  enum c_name

#define NODE_OPTION(c_name, cpp_name) c_name

#define NODE_ENUM(c_name, cpp_name)                                            \
  enum c_name c_name;                                                          \
  enum c_name

#define NODE_ENUM_ITEM(c_name, cpp_name) c_name

#endif

//==============================================================================
// Data types
//==============================================================================

typedef struct node_embedding_platform_s* node_embedding_platform;
typedef struct node_embedding_runtime_s* node_embedding_runtime;
typedef struct node_embedding_platform_config_s* node_embedding_platform_config;
typedef struct node_embedding_runtime_config_s* node_embedding_runtime_config;
typedef struct node_embedding_node_api_scope_s* node_embedding_node_api_scope;

#ifdef __cplusplus
namespace node::embedding {
#endif

// The status returned by the Node.js embedding API functions.
typedef NODE_ENUM(node_embedding_status, NodeStatus){
  NODE_ENUM_ITEM(node_embedding_status_ok, kOk) = 0,
  NODE_ENUM_ITEM(node_embedding_status_generic_error, kGenericError) = 1,
  NODE_ENUM_ITEM(node_embedding_status_null_arg, kNullArg) = 2,
  NODE_ENUM_ITEM(node_embedding_status_bad_arg, kBadArg) = 3,
  // This value is added to the exit code in cases when Node.js API returns
  // an error exit code.
  NODE_ENUM_ITEM(node_embedding_status_error_exit_code, kErrorExitCode) = 512,
};

// The flags for the Node.js platform initialization.
// They match the internal ProcessInitializationFlags::Flags enum.
typedef NODE_OPTIONS(node_embedding_platform_flags, NodePlatformFlags){
  NODE_OPTION(node_embedding_platform_flags_none, kNone) = 0,
  // Enable stdio inheritance, which is disabled by default.
  // This flag is also implied by
  // node_embedding_platform_flags_no_stdio_initialization.
  NODE_OPTION(node_embedding_platform_flags_enable_stdio_inheritance,
              kEnableStdioInheritance) = 1 << 0,
  // Disable reading the NODE_OPTIONS environment variable.
  NODE_OPTION(node_embedding_platform_flags_disable_node_options_env,
              kDisableNodeOptionsEnv) = 1 << 1,
  // Do not parse CLI options.
  NODE_OPTION(node_embedding_platform_flags_disable_cli_options,
              kDisableCliOptions) = 1 << 2,
  // Do not initialize ICU.
  NODE_OPTION(node_embedding_platform_flags_no_icu, kNoICU) = 1 << 3,
  // Do not modify stdio file descriptor or TTY state.
  NODE_OPTION(node_embedding_platform_flags_no_stdio_initialization,
              kNoStdioInitialization) = 1 << 4,
  // Do not register Node.js-specific signal handlers
  // and reset other signal handlers to default state.
  NODE_OPTION(node_embedding_platform_flags_no_default_signal_handling,
              kNoDefaultSignalHandling) = 1 << 5,
  // Do not initialize OpenSSL config.
  NODE_OPTION(node_embedding_platform_flags_no_init_openssl,
              kNoInitOpenSSL) = 1 << 8,
  // Do not initialize Node.js debugging based on environment variables.
  NODE_OPTION(node_embedding_platform_flags_no_parse_global_debug_variables,
              kNoParseGlobalDebugVariables) = 1 << 9,
  // Do not adjust OS resource limits for this process.
  NODE_OPTION(node_embedding_platform_flags_no_adjust_resource_limits,
              kNoAdjustResourceLimits) = 1 << 10,
  // Do not map code segments into large pages for this process.
  NODE_OPTION(node_embedding_platform_flags_no_use_large_pages,
              kNoUseLargePages) = 1 << 11,
  // Skip printing output for --help, --version, --v8-options.
  NODE_OPTION(node_embedding_platform_flags_no_print_help_or_version_output,
              kNoPrintHelpOrVersionOutput) = 1 << 12,
  // Initialize the process for predictable snapshot generation.
  NODE_OPTION(node_embedding_platform_flags_generate_predictable_snapshot,
              kGeneratePredictableSnapshot) = 1 << 14,
};

// The flags for the Node.js runtime initialization.
// They match the internal EnvironmentFlags::Flags enum.
typedef NODE_OPTIONS(node_embedding_runtime_flags, NodeRuntimeFlags){
  NODE_OPTION(node_embedding_runtime_flags_none, kNone) = 0,
  // Use the default behavior for Node.js instances.
  NODE_OPTION(node_embedding_runtime_flags_default, kDefault) = 1 << 0,
  // Controls whether this Environment is allowed to affect per-process state
  // (e.g. cwd, process title, uid, etc.).
  // This is set when using node_embedding_runtime_flags_default.
  NODE_OPTION(node_embedding_runtime_flags_owns_process_state,
              kOwnsProcessState) = 1 << 1,
  // Set if this Environment instance is associated with the global inspector
  // handling code (i.e. listening on SIGUSR1).
  // This is set when using node_embedding_runtime_flags_default.
  NODE_OPTION(node_embedding_runtime_flags_owns_inspector,
              kOwnsInspector) = 1 << 2,
  // Set if Node.js should not run its own esm loader. This is needed by some
  // embedders, because it's possible for the Node.js esm loader to conflict
  // with another one in an embedder environment, e.g. Blink's in Chromium.
  NODE_OPTION(node_embedding_runtime_flags_no_register_esm_loader,
              kNoRegisterEsmLoader) = 1 << 3,
  // Set this flag to make Node.js track "raw" file descriptors, i.e. managed
  // by fs.open() and fs.close(), and close them during
  // node_embedding_delete_runtime().
  NODE_OPTION(node_embedding_runtime_flags_track_unmanaged_fds,
              kTrackUnmanagedFds) = 1 << 4,
  // Set this flag to force hiding console windows when spawning child
  // processes. This is usually used when embedding Node.js in GUI programs on
  // Windows.
  NODE_OPTION(node_embedding_runtime_flags_hide_console_windows,
              kHideConsoleWindows) = 1 << 5,
  // Set this flag to disable loading native addons via `process.dlopen`.
  // This environment flag is especially important for worker threads
  // so that a worker thread can't load a native addon even if `execArgv`
  // is overwritten and `--no-addons` is not specified but was specified
  // for this Environment instance.
  NODE_OPTION(node_embedding_runtime_flags_no_native_addons,
              kNoNativeAddons) = 1 << 6,
  // Set this flag to disable searching modules from global paths like
  // $HOME/.node_modules and $NODE_PATH. This is used by standalone apps that
  // do not expect to have their behaviors changed because of globally
  // installed modules.
  NODE_OPTION(node_embedding_runtime_flags_no_global_search_paths,
              kNoGlobalSearchPaths) = 1 << 7,
  // Do not export browser globals like setTimeout, console, etc.
  NODE_OPTION(node_embedding_runtime_flags_no_browser_globals,
              kNoBrowserGlobals) = 1 << 8,
  // Controls whether or not the Environment should call
  // V8Inspector::create(). This control is needed by embedders who may not
  // want to initialize the V8 inspector in situations where one has already
  // been created, e.g. Blink's in Chromium.
  NODE_OPTION(node_embedding_runtime_flags_no_create_inspector,
              kNoCreateInspector) = 1 << 9,
  // Controls whether or not the InspectorAgent for this Environment should
  // call StartDebugSignalHandler. This control is needed by embedders who may
  // not want to allow other processes to start the V8 inspector.
  NODE_OPTION(node_embedding_runtime_flags_no_start_debug_signal_handler,
              kNoStartDebugSignalHandler) = 1 << 10,
  // Controls whether the InspectorAgent created for this Environment waits
  // for Inspector frontend events during the Environment creation. It's used
  // to call node::Stop(env) on a Worker thread that is waiting for the
  // events.
  NODE_OPTION(node_embedding_runtime_flags_no_wait_for_inspector_frontend,
              kNoWaitForInspectorFrontend) = 1 << 11,
};

#ifdef __cplusplus
}  // namespace node::embedding
using node_embedding_status = node::embedding::NodeStatus;
using node_embedding_platform_flags = node::embedding::NodePlatformFlags;
using node_embedding_runtime_flags = node::embedding::NodeRuntimeFlags;
#endif

//==============================================================================
// Callbacks
//==============================================================================

typedef void(NAPI_CDECL* node_embedding_release_data_callback)(void* data);

typedef node_embedding_status(NAPI_CDECL* node_embedding_handle_error_callback)(
    void* cb_data,
    const char* messages[],
    size_t messages_size,
    node_embedding_status status);

typedef node_embedding_status(
    NAPI_CDECL* node_embedding_configure_platform_callback)(
    void* cb_data, node_embedding_platform_config platform_config);

typedef node_embedding_status(
    NAPI_CDECL* node_embedding_configure_runtime_callback)(
    void* cb_data,
    node_embedding_platform platform,
    node_embedding_runtime_config runtime_config);

typedef void(NAPI_CDECL* node_embedding_get_args_callback)(void* cb_data,
                                                           int32_t argc,
                                                           const char* argv[]);

typedef void(NAPI_CDECL* node_embedding_preload_callback)(
    void* cb_data,
    node_embedding_runtime runtime,
    napi_env env,
    napi_value process,
    napi_value require);

typedef napi_value(NAPI_CDECL* node_embedding_start_execution_callback)(
    void* cb_data,
    node_embedding_runtime runtime,
    napi_env env,
    napi_value process,
    napi_value require,
    napi_value run_cjs);

typedef void(NAPI_CDECL* node_embedding_handle_start_result_callback)(
    void* cb_data,
    node_embedding_runtime runtime,
    napi_env env,
    napi_value value);

typedef napi_value(NAPI_CDECL* node_embedding_initialize_module_callback)(
    void* cb_data,
    node_embedding_runtime runtime,
    napi_env env,
    const char* module_name,
    napi_value exports);

typedef napi_value(NAPI_CDECL* node_embedding_create_wrapper_callback)(
    void* cb_data, node_embedding_runtime runtime, void** result);

typedef void(NAPI_CDECL* node_embedding_run_task_callback)(void* cb_data);

typedef void(NAPI_CDECL* node_embedding_post_task_callback)(
    void* cb_data,
    node_embedding_run_task_callback run_task,
    void* task_data,
    node_embedding_release_data_callback release_task_data);

typedef void(NAPI_CDECL* node_embedding_run_node_api_callback)(
    void* cb_data, node_embedding_runtime runtime, napi_env env);

//==============================================================================
// API v-table
//==============================================================================

typedef struct {
  node_embedding_status(NAPI_CDECL* on_error)(
      node_embedding_handle_error_callback error_handler,
      void* error_handler_data,
      node_embedding_release_data_callback release_error_handler_data);

  node_embedding_status(NAPI_CDECL* set_api_version)(
      int32_t embedding_api_version, int32_t node_api_version);

  node_embedding_status(NAPI_CDECL* run_main)(
      int32_t argc,
      const char* argv[],
      node_embedding_configure_platform_callback configure_platform,
      void* configure_platform_data,
      node_embedding_configure_runtime_callback configure_runtime,
      void* configure_runtime_data);

  node_embedding_status(NAPI_CDECL* create_platform)(
      int32_t argc,
      const char* argv[],
      node_embedding_configure_platform_callback configure_platform,
      void* configure_platform_data,
      node_embedding_platform* result);

  node_embedding_status(NAPI_CDECL* delete_platform)(
      node_embedding_platform platform);

  node_embedding_status(NAPI_CDECL* set_platform_flags)(
      node_embedding_platform_config platform_config,
      node_embedding_platform_flags flags);

  node_embedding_status(NAPI_CDECL* get_platform_parsed_args)(
      node_embedding_platform platform,
      node_embedding_get_args_callback get_args,
      void* get_args_data,
      node_embedding_get_args_callback get_runtime_args,
      void* get_runtime_args_data);

  node_embedding_status(NAPI_CDECL* run_runtime)(
      node_embedding_platform platform,
      node_embedding_configure_runtime_callback configure_runtime,
      void* configure_runtime_data);

  node_embedding_status(NAPI_CDECL* create_runtime)(
      node_embedding_platform platform,
      node_embedding_configure_runtime_callback configure_runtime,
      node_embedding_runtime* result);

  node_embedding_status(NAPI_CDECL* delete_runtime)(
      node_embedding_runtime runtime);

  node_embedding_status(NAPI_CDECL* set_runtime_flags)(
      node_embedding_runtime_config runtime_config,
      node_embedding_runtime_flags flags);

  node_embedding_status(NAPI_CDECL* set_runtime_args)(
      node_embedding_runtime_config runtime_config,
      int32_t argc,
      const char* argv[],
      int32_t runtime_argc,
      const char* runtime_argv[]);

  node_embedding_status(NAPI_CDECL* on_runtime_preload)(
      node_embedding_runtime_config runtime_config,
      node_embedding_preload_callback run_preload,
      void* preload_data,
      node_embedding_release_data_callback release_preload_data);

  node_embedding_status(NAPI_CDECL* on_runtime_start_execution)(
      node_embedding_runtime_config runtime_config,
      node_embedding_start_execution_callback start_execution,
      void* start_execution_data,
      node_embedding_release_data_callback release_start_execution_data);

  node_embedding_status(NAPI_CDECL* on_handle_runtime_start_result)(
      node_embedding_runtime_config runtime_config,
      node_embedding_handle_start_result_callback handle_result,
      void* handle_result_data,
      node_embedding_release_data_callback release_handle_result_data);

  node_embedding_status(NAPI_CDECL* add_runtime_module)(
      node_embedding_runtime_config runtime_config,
      const char* module_name,
      node_embedding_initialize_module_callback init_module,
      void* init_module_data,
      node_embedding_release_data_callback release_init_module_data,
      int32_t module_node_api_version);

  node_embedding_status(NAPI_CDECL* on_create_runtime_wrapper)(
      node_embedding_runtime_config runtime_config,
      node_embedding_create_wrapper_callback create_wrapper,
      void* create_wrapper_data,
      node_embedding_release_data_callback release_create_wrapper_data);

  node_embedding_status(NAPI_CDECL* get_runtime_wrapper)(
      node_embedding_runtime runtime, void** result);

  node_embedding_status(NAPI_CDECL* set_runtime_task_runner)(
      node_embedding_runtime_config runtime_config,
      node_embedding_post_task_callback post_task,
      void* post_task_data,
      node_embedding_release_data_callback release_post_task_data);

  node_embedding_status(NAPI_CDECL* run_event_loop)(
      node_embedding_runtime runtime);

  node_embedding_status(NAPI_CDECL* terminate_event_loop)(
      node_embedding_runtime runtime);

  node_embedding_status(NAPI_CDECL* run_event_loop_once)(
      node_embedding_runtime runtime, bool* has_more_work);

  node_embedding_status(NAPI_CDECL* run_event_loop_no_wait)(
      node_embedding_runtime runtime, bool* has_more_work);

  node_embedding_status(NAPI_CDECL* run_node_api)(
      node_embedding_runtime runtime,
      node_embedding_run_node_api_callback run_node_api,
      void* run_node_api_data);

  node_embedding_status(NAPI_CDECL* open_node_api_scope)(
      node_embedding_runtime runtime,
      node_embedding_node_api_scope* node_api_scope,
      napi_env* env);

  node_embedding_status(NAPI_CDECL* close_node_api_scope)(
      node_embedding_runtime runtime,
      node_embedding_node_api_scope node_api_scope);
} node_embedding_api_vtable;

//==============================================================================
// Functions
//==============================================================================

EXTERN_C_START

//------------------------------------------------------------------------------
// API v-table functions.
//------------------------------------------------------------------------------

NAPI_EXTERN node_embedding_status NAPI_CDECL
node_embedding_get_api_vtable(node_embedding_api_vtable** api_vtable);

//------------------------------------------------------------------------------
// Error handling functions.
//------------------------------------------------------------------------------

// Sets the global error handing for the Node.js embedding API.
NAPI_EXTERN node_embedding_status NAPI_CDECL node_embedding_on_error(
    node_embedding_handle_error_callback error_handler,
    void* error_handler_data,
    node_embedding_release_data_callback release_error_handler_data);

//------------------------------------------------------------------------------
// Node.js global platform functions.
//------------------------------------------------------------------------------

// Sets the API version for the Node.js embedding API and the Node-API.
NAPI_EXTERN node_embedding_status NAPI_CDECL node_embedding_set_api_version(
    int32_t embedding_api_version, int32_t node_api_version);

// Runs Node.js main function as if it is invoked from Node.js CLI.
NAPI_EXTERN node_embedding_status NAPI_CDECL node_embedding_run_main(
    int32_t argc,
    const char* argv[],
    node_embedding_configure_platform_callback configure_platform,
    void* configure_platform_data,
    node_embedding_configure_runtime_callback configure_runtime,
    void* configure_runtime_data);

// Creates and configures a new Node.js platform instance.
NAPI_EXTERN node_embedding_status NAPI_CDECL node_embedding_create_platform(
    int32_t argc,
    const char* argv[],
    node_embedding_configure_platform_callback configure_platform,
    void* configure_platform_data,
    node_embedding_platform* result);

// Deletes the Node.js platform instance.
NAPI_EXTERN node_embedding_status NAPI_CDECL
node_embedding_delete_platform(node_embedding_platform platform);

// Sets the flags for the Node.js platform initialization.
NAPI_EXTERN node_embedding_status NAPI_CDECL node_embedding_set_platform_flags(
    node_embedding_platform_config platform_config,
    node_embedding_platform_flags flags);

// Gets the parsed list of non-Node.js and Node.js arguments.
NAPI_EXTERN node_embedding_status NAPI_CDECL
node_embedding_get_platform_parsed_args(
    node_embedding_platform platform,
    node_embedding_get_args_callback get_args,
    void* get_args_data,
    node_embedding_get_args_callback get_runtime_args,
    void* get_runtime_args_data);

//------------------------------------------------------------------------------
// Node.js runtime functions.
//------------------------------------------------------------------------------

// Runs the Node.js runtime with the provided configuration.
NAPI_EXTERN node_embedding_status NAPI_CDECL node_embedding_run_runtime(
    node_embedding_platform platform,
    node_embedding_configure_runtime_callback configure_runtime,
    void* configure_runtime_data);

// Creates a new Node.js runtime instance.
NAPI_EXTERN node_embedding_status NAPI_CDECL node_embedding_create_runtime(
    node_embedding_platform platform,
    node_embedding_configure_runtime_callback configure_runtime,
    node_embedding_runtime* result);

// Deletes the Node.js runtime instance.
NAPI_EXTERN node_embedding_status NAPI_CDECL
node_embedding_delete_runtime(node_embedding_runtime runtime);

// Sets the flags for the Node.js runtime initialization.
NAPI_EXTERN node_embedding_status NAPI_CDECL
node_embedding_set_runtime_flags(node_embedding_runtime_config runtime_config,
                                 node_embedding_runtime_flags flags);

// Sets the non-Node.js and Node.js CLI arguments for the Node.js runtime
// initialization.
NAPI_EXTERN node_embedding_status NAPI_CDECL
node_embedding_set_runtime_args(node_embedding_runtime_config runtime_config,
                                int32_t argc,
                                const char* argv[],
                                int32_t runtime_argc,
                                const char* runtime_argv[]);

// Sets the preload callback for the Node.js runtime initialization.
NAPI_EXTERN node_embedding_status NAPI_CDECL node_embedding_on_preload_runtime(
    node_embedding_runtime_config runtime_config,
    node_embedding_preload_callback run_preload,
    void* preload_data,
    node_embedding_release_data_callback release_preload_data);

// Sets the start execution callback for the Node.js runtime initialization.
NAPI_EXTERN node_embedding_status NAPI_CDECL
node_embedding_on_start_runtime_execution(
    node_embedding_runtime_config runtime_config,
    node_embedding_start_execution_callback start_execution,
    void* start_execution_data,
    node_embedding_release_data_callback release_start_execution_data);

NAPI_EXTERN node_embedding_status NAPI_CDECL
node_embedding_on_handle_runtime_start_result(
    node_embedding_runtime_config runtime_config,
    node_embedding_handle_start_result_callback handle_result,
    void* handle_result_data,
    node_embedding_release_data_callback release_handle_result_data);

// Adds a new module to the Node.js runtime.
// It is accessed as process._linkedBinding(module_name) in the main JS and in
// the related worker threads.
NAPI_EXTERN node_embedding_status NAPI_CDECL node_embedding_add_runtime_module(
    node_embedding_runtime_config runtime_config,
    const char* module_name,
    node_embedding_initialize_module_callback init_module,
    void* init_module_data,
    node_embedding_release_data_callback release_init_module_data,
    int32_t module_node_api_version);

NAPI_EXTERN node_embedding_status NAPI_CDECL
node_embedding_on_create_runtime_wrapper(
    node_embedding_runtime_config runtime_config,
    node_embedding_create_wrapper_callback create_wrapper,
    void* create_wrapper_data,
    node_embedding_release_data_callback release_create_wrapper_data);

NAPI_EXTERN node_embedding_status NAPI_CDECL node_embedding_get_runtime_wrapper(
    node_embedding_runtime runtime, void** result);

//------------------------------------------------------------------------------
// Node.js runtime functions for the event loop.
//------------------------------------------------------------------------------

// Sets the task runner for the Node.js runtime.
// This is an alternative way to run the Node.js runtime event loop that helps
// running it inside of an existing task scheduler.
// E.g. it enables running Node.js event loop inside of the application UI event
// loop or UI dispatcher.
NAPI_EXTERN node_embedding_status NAPI_CDECL
node_embedding_set_runtime_task_runner(
    node_embedding_runtime_config runtime_config,
    node_embedding_post_task_callback post_task,
    void* post_task_data,
    node_embedding_release_data_callback release_post_task_data);

// Runs the Node.js runtime event loop in UV_RUN_DEFAULT mode.
// It finishes it with emitting the beforeExit and exit process events.
NAPI_EXTERN node_embedding_status NAPI_CDECL
node_embedding_run_event_loop(node_embedding_runtime runtime);

// Stops the Node.js runtime event loop. It cannot be resumed after this call.
// It does not emit the beforeExit and exit process events if they were not
// emitted before.
NAPI_EXTERN node_embedding_status NAPI_CDECL
node_embedding_terminate_event_loop(node_embedding_runtime runtime);

// Runs the Node.js runtime event loop once. It may block the current thread.
// It matches the UV_RUN_ONCE behavior
NAPI_EXTERN node_embedding_status NAPI_CDECL node_embedding_run_event_loop_once(
    node_embedding_runtime runtime, bool* has_more_work);

// Runs the Node.js runtime event loop once. It does not block the thread.
// It matches the UV_RUN_NOWAIT behavior.
NAPI_EXTERN node_embedding_status NAPI_CDECL
node_embedding_run_event_loop_no_wait(node_embedding_runtime runtime,
                                      bool* has_more_work);

//------------------------------------------------------------------------------
// Node.js runtime functions for the Node-API interop.
//------------------------------------------------------------------------------

// Runs Node-API code in the Node-API scope.
NAPI_EXTERN node_embedding_status NAPI_CDECL
node_embedding_run_node_api(node_embedding_runtime runtime,
                            node_embedding_run_node_api_callback run_node_api,
                            void* run_node_api_data);

// Opens a new Node-API scope.
NAPI_EXTERN node_embedding_status NAPI_CDECL node_embedding_open_node_api_scope(
    node_embedding_runtime runtime,
    node_embedding_node_api_scope* node_api_scope,
    napi_env* env);

// Closes the Node-API invocation scope.
NAPI_EXTERN node_embedding_status NAPI_CDECL
node_embedding_close_node_api_scope(
    node_embedding_runtime runtime,
    node_embedding_node_api_scope node_api_scope);

EXTERN_C_END

#ifdef __cplusplus

namespace node::embedding {

//==============================================================================
// C++ convenience functions for the C API.
// These functions are not ABI safe and can be changed in future versions.
//==============================================================================

template <typename TPointer>
class NodePointer {
 public:
  NodePointer() = default;

  explicit NodePointer(TPointer ptr) : ptr_(ptr) {}

  NodePointer(const NodePointer&) = delete;
  NodePointer& operator=(const NodePointer&) = delete;

  NodePointer(NodePointer&& other) : ptr_(other.ptr_) { other.ptr_ = nullptr; }
  NodePointer& operator=(NodePointer&& other) {
    if (this != &other) {
      ptr_ = other.ptr_;
      other.ptr_ = nullptr;
    }
    return *this;
  }

  TPointer Get() const { return ptr_; }
  TPointer operator->() const { return ptr_; }

  explicit operator bool() const { return ptr_ != nullptr; }

 private:
  TPointer ptr_{};
};

template <typename T>
class NodeExpected {
 public:
  explicit NodeExpected(T value) : value_(std::move(value)) {}

  explicit NodeExpected(NodeStatus status) : status_(status) {}

  NodeExpected(const NodeExpected&) = delete;
  NodeExpected& operator=(const NodeExpected&) = delete;

  NodeExpected(NodeExpected&& other) : status_(other) {
    if (other.HasValue()) {
      new (std::addressof(value_)) T(std::move(other.value_));
    }
  }

  NodeExpected& operator=(NodeExpected&& other) {
    if (this != &other) {
      if (HasValue()) {
        value_.~T();
      }
      status_ = other.status_;
      if (other.HasValue()) {
        new (std::addressof(value_)) T(std::move(other.value_));
      }
    }
    return *this;
  }

  ~NodeExpected() {
    if (HasValue()) {
      value_.~T();
    }
  }

  bool HasValue() const { return status_ == NodeStatus::kOk; }

  T& Value() & { return value_; }
  const T& Value() const& { return value_; }
  T&& Value() && { return std::move(value_); }
  const T&& Value() const&& { return std::move(value_); }

  NodeStatus Status() const { return status_; }

 private:
  NodeStatus status_{NodeStatus::kOk};
  union {
    T value_;  // The value is uninitialized if status_ is not kOk.
    char padding[sizeof(T)];
  };
};

template <>
class NodeExpected<void> {
 public:
  NodeExpected() = default;

  explicit NodeExpected(NodeStatus status) : status_(status) {}

  NodeExpected(const NodeExpected&) = delete;
  NodeExpected& operator=(const NodeExpected&) = delete;

  NodeExpected(NodeExpected&& other) = default;
  NodeExpected& operator=(NodeExpected&& other) = default;

  bool HasValue() const { return status_ == NodeStatus::kOk; }

  NodeStatus Status() const { return status_; }

 private:
  NodeStatus status_{NodeStatus::kOk};
};

class NodePlatformConfig {
 public:
  explicit NodePlatformConfig(node_embedding_platform_config platform_config)
      : platform_config_(platform_config) {}

  NodePlatformConfig(const NodePlatformConfig&) = delete;
  NodePlatformConfig& operator=(const NodePlatformConfig&) = delete;

  NodePlatformConfig(NodePlatformConfig&& other) = default;
  NodePlatformConfig& operator=(NodePlatformConfig&& other) = default;

  operator node_embedding_platform_config() const {
    return platform_config_.Get();
  }

  void SetFlags(NodePlatformFlags flags) {
    node_embedding_set_platform_flags(platform_config_.Get(), flags);
  }

 private:
  NodePointer<node_embedding_platform_config> platform_config_{};
};

class NodeArgs {
 public:
  NodeArgs(int32_t argc, const char* argv[]) : argc_(argc), argv_(argv) {}

  int32_t GetArgc() const { return argc_; }
  const char** GetArgv() const { return argv_; }

 private:
  int32_t argc_{};
  const char** argv_{};
};

template <typename TCallback>
class NodeFunctorRef {
 public:
  NodeFunctorRef(TCallback callback, void* callback_data)
      : callback_(callback), callback_data_(callback_data) {}

  NodeFunctorRef(const NodeFunctorRef&) = delete;
  NodeFunctorRef& operator=(const NodeFunctorRef&) = delete;

  NodeFunctorRef(NodeFunctorRef&& other) = default;
  NodeFunctorRef& operator=(NodeFunctorRef&& other) = default;

  TCallback GetCallback() const { return callback_; }
  void* GetData() const { return callback_data_; }

 private:
  TCallback callback_{};
  void* callback_data_{};
};

template <typename TCallback>
class NodeFunctor {
 public:
  NodeFunctor(TCallback callback,
              void* callback_data,
              node_embedding_release_data_callback callback_release)
      : callback_(callback),
        callback_data_(callback_data),
        callback_release_(callback_release) {}

  NodeFunctor(const NodeFunctor&) = delete;
  NodeFunctor& operator=(const NodeFunctor&) = delete;

  NodeFunctor(NodeFunctor&& other) = default;
  NodeFunctor& operator=(NodeFunctor&& other) = default;

  TCallback GetCallback() const { return callback_; }

  void* GetData() const { return callback_data_; }

  node_embedding_release_data_callback GetRelease() const {
    return callback_release_;
  }

 private:
  TCallback callback_{};
  void* callback_data_{};
  node_embedding_release_data_callback callback_release_{};
};

class NodePlatform {
 public:
  static NodeStatus RunMain(
      NodeArgs args,
      NodeFunctorRef<node_embedding_configure_platform_callback>
          configure_platform,
      NodeFunctorRef<node_embedding_configure_runtime_callback>
          configure_runtime) {
    return node_embedding_run_main(args.GetArgc(),
                                   args.GetArgv(),
                                   configure_platform.GetCallback(),
                                   configure_platform.GetData(),
                                   configure_runtime.GetCallback(),
                                   configure_runtime.GetData());
  }

  static NodeExpected<NodePlatform> Create(
      NodeArgs args,
      NodeFunctorRef<node_embedding_configure_platform_callback>
          configure_platform) {
    node_embedding_platform result;
    NodeStatus status =
        node_embedding_create_platform(args.GetArgc(),
                                       args.GetArgv(),
                                       configure_platform.GetCallback(),
                                       configure_platform.GetData(),
                                       &result);
    if (status != NodeStatus::kOk) {
      return NodeExpected<NodePlatform>(status);
    }
    return NodeExpected<NodePlatform>(result);
  }

  NodePlatform(node_embedding_platform platform) : platform_(platform) {}

  NodePlatform(const NodePlatform&) = delete;
  NodePlatform& operator=(const NodePlatform&) = delete;
  NodePlatform(NodePlatform&& other) = default;
  NodePlatform& operator=(NodePlatform&& other) = default;

  ~NodePlatform() {
    if (platform_) {
      node_embedding_delete_platform(platform_.Get());
    }
  }

  NodeStatus GetParsedArgs(
      NodeFunctorRef<node_embedding_get_args_callback> get_args,
      NodeFunctorRef<node_embedding_get_args_callback> get_runtime_args) {
    return node_embedding_get_platform_parsed_args(
        platform_.Get(),
        get_args.GetCallback(),
        get_args.GetData(),
        get_runtime_args.GetCallback(),
        get_runtime_args.GetData());
  }

  operator node_embedding_platform() const { return platform_.Get(); }

 private:
  NodePointer<node_embedding_platform> platform_{};
};

class NodeRuntimeConfig {
 public:
  explicit NodeRuntimeConfig(node_embedding_runtime_config runtime_config)
      : runtime_config_(runtime_config) {}

  NodeRuntimeConfig(const NodeRuntimeConfig&) = delete;
  NodeRuntimeConfig& operator=(const NodeRuntimeConfig&) = delete;

  NodeRuntimeConfig(NodeRuntimeConfig&& other) = default;
  NodeRuntimeConfig& operator=(NodeRuntimeConfig&& other) = default;

  operator node_embedding_runtime_config() const {
    return runtime_config_.Get();
  }

  void SetFlags(NodeRuntimeFlags flags) {
    node_embedding_set_runtime_flags(runtime_config_.Get(), flags);
  }

  void SetArgs(NodeArgs args, NodeArgs runtime_args) {
    node_embedding_set_runtime_args(runtime_config_.Get(),
                                    args.GetArgc(),
                                    args.GetArgv(),
                                    runtime_args.GetArgc(),
                                    runtime_args.GetArgv());
  }

  void OnPreload(NodeFunctor<node_embedding_preload_callback> run_preload) {
    node_embedding_on_preload_runtime(runtime_config_.Get(),
                                      run_preload.GetCallback(),
                                      run_preload.GetData(),
                                      run_preload.GetRelease());
  }

  void OnStartExecution(
      NodeFunctor<node_embedding_start_execution_callback> start_execution) {
    node_embedding_on_start_runtime_execution(runtime_config_.Get(),
                                              start_execution.GetCallback(),
                                              start_execution.GetData(),
                                              start_execution.GetRelease());
  }

  void OnHandleStartResult(
      NodeFunctor<node_embedding_handle_start_result_callback>
          handle_start_result) {
    node_embedding_on_handle_runtime_start_result(
        runtime_config_.Get(),
        handle_start_result.GetCallback(),
        handle_start_result.GetData(),
        handle_start_result.GetRelease());
  }

  void AddModule(
      const char* moduleName,
      NodeFunctor<node_embedding_initialize_module_callback> init_module,
      int32_t moduleNodeApiVersion) {
    node_embedding_add_runtime_module(runtime_config_.Get(),
                                      moduleName,
                                      init_module.GetCallback(),
                                      init_module.GetData(),
                                      init_module.GetRelease(),
                                      moduleNodeApiVersion);
  }

  void SetTaskRunner(NodeFunctor<node_embedding_post_task_callback> post_task) {
    node_embedding_set_runtime_task_runner(runtime_config_.Get(),
                                           post_task.GetCallback(),
                                           post_task.GetData(),
                                           post_task.GetRelease());
  }

 private:
  NodePointer<node_embedding_runtime_config> runtime_config_{};
};

class NodeApiScope {
 public:
  NodeApiScope(node_embedding_runtime runtime) : runtime_(runtime) {
    node_embedding_open_node_api_scope(runtime, &node_api_scope_, &env_);
  }

  NodeApiScope(node_embedding_runtime runtime,
               node_embedding_node_api_scope node_api_scope,
               napi_env env)
      : runtime_(runtime), node_api_scope_(node_api_scope), env_(env) {}

  NodeApiScope(const NodeApiScope&) = delete;
  NodeApiScope& operator=(const NodeApiScope&) = delete;

  ~NodeApiScope() {
    node_embedding_close_node_api_scope(runtime_, node_api_scope_);
  }

 private:
  node_embedding_runtime runtime_{};
  node_embedding_node_api_scope node_api_scope_{};
  napi_env env_{};
};

class NodeRuntime {
 public:
  explicit NodeRuntime(node_embedding_runtime runtime) : runtime_(runtime) {}

  NodeRuntime(const NodeRuntime&) = delete;
  NodeRuntime& operator=(const NodeRuntime&) = delete;

  NodeRuntime(NodeRuntime&& other) = default;
  NodeRuntime& operator=(NodeRuntime&& other) = default;

  ~NodeRuntime() {
    if (runtime_) {
      node_embedding_delete_runtime(runtime_.Get());
    }
  }
  operator node_embedding_runtime() const { return runtime_.Get(); }

  NodeExpected<void> RunEventLoop() {
    return NodeExpected<void>(node_embedding_run_event_loop(runtime_.Get()));
  }

  NodeExpected<void> TerminateEventLoop() {
    return NodeExpected<void>(
        node_embedding_terminate_event_loop(runtime_.Get()));
  }

  NodeExpected<bool> RunEventLoopOnce() {
    bool has_more_work{};
    NodeStatus status =
        node_embedding_run_event_loop_once(runtime_.Get(), &has_more_work);
    if (status != NodeStatus::kOk) {
      return NodeExpected<bool>(status);
    }
    return NodeExpected<bool>(has_more_work);
  }

  NodeExpected<bool> RunEventLoopNoWait() {
    bool has_more_work{};
    NodeStatus status =
        node_embedding_run_event_loop_no_wait(runtime_.Get(), &has_more_work);
    if (status != NodeStatus::kOk) {
      return NodeExpected<bool>(status);
    }
    return NodeExpected<bool>(has_more_work);
  }

  void RunNodeApi(
      NodeFunctorRef<node_embedding_run_node_api_callback> runNodeApi) {
    node_embedding_run_node_api(
        runtime_.Get(), runNodeApi.GetCallback(), runNodeApi.GetData());
  }

  NodeApiScope OpenNodeApiScope() { return NodeApiScope(runtime_.Get()); }

 private:
  NodePointer<node_embedding_runtime> runtime_{};
};

}  // namespace node::embedding

#endif  // __cplusplus

#endif  // SRC_NODE_EMBEDDING_API_H_
