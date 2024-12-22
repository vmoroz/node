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

EXTERN_C_START

//==============================================================================
// Data types
//==============================================================================

typedef struct node_embedding_platform_s* node_embedding_platform;
typedef struct node_embedding_runtime_s* node_embedding_runtime;
typedef struct node_embedding_platform_config_s* node_embedding_platform_config;
typedef struct node_embedding_runtime_config_s* node_embedding_runtime_config;
typedef struct node_embedding_node_api_scope_s* node_embedding_node_api_scope;

// The status returned by the Node.js embedding API functions.
typedef enum {
  node_embedding_status_ok = 0,
  node_embedding_status_generic_error = 1,
  node_embedding_status_null_arg = 2,
  node_embedding_status_bad_arg = 3,
  // This value is added to the exit code in cases when Node.js API returns
  // an error exit code.
  node_embedding_status_error_exit_code = 512,
} node_embedding_status;

// The flags for the Node.js platform initialization.
// They match the internal ProcessInitializationFlags::Flags enum.
typedef enum {
  node_embedding_platform_flags_none = 0,
  // Enable stdio inheritance, which is disabled by default.
  // This flag is also implied by
  // node_embedding_platform_flags_no_stdio_initialization.
  node_embedding_platform_flags_enable_stdio_inheritance = 1 << 0,
  // Disable reading the NODE_OPTIONS environment variable.
  node_embedding_platform_flags_disable_node_options_env = 1 << 1,
  // Do not parse CLI options.
  node_embedding_platform_flags_disable_cli_options = 1 << 2,
  // Do not initialize ICU.
  node_embedding_platform_flags_no_icu = 1 << 3,
  // Do not modify stdio file descriptor or TTY state.
  node_embedding_platform_flags_no_stdio_initialization = 1 << 4,
  // Do not register Node.js-specific signal handlers
  // and reset other signal handlers to default state.
  node_embedding_platform_flags_no_default_signal_handling = 1 << 5,
  // Do not initialize OpenSSL config.
  node_embedding_platform_flags_no_init_openssl = 1 << 8,
  // Do not initialize Node.js debugging based on environment variables.
  node_embedding_platform_flags_no_parse_global_debug_variables = 1 << 9,
  // Do not adjust OS resource limits for this process.
  node_embedding_platform_flags_no_adjust_resource_limits = 1 << 10,
  // Do not map code segments into large pages for this process.
  node_embedding_platform_flags_no_use_large_pages = 1 << 11,
  // Skip printing output for --help, --version, --v8-options.
  node_embedding_platform_flags_no_print_help_or_version_output = 1 << 12,
  // Initialize the process for predictable snapshot generation.
  node_embedding_platform_flags_generate_predictable_snapshot = 1 << 14,
} node_embedding_platform_flags;

// The flags for the Node.js runtime initialization.
// They match the internal EnvironmentFlags::Flags enum.
typedef enum {
  node_embedding_runtime_flags_none = 0,
  // Use the default behavior for Node.js instances.
  node_embedding_runtime_flags_default = 1 << 0,
  // Controls whether this Environment is allowed to affect per-process state
  // (e.g. cwd, process title, uid, etc.).
  // This is set when using node_embedding_runtime_flags_default.
  node_embedding_runtime_flags_owns_process_state = 1 << 1,
  // Set if this Environment instance is associated with the global inspector
  // handling code (i.e. listening on SIGUSR1).
  // This is set when using node_embedding_runtime_flags_default.
  node_embedding_runtime_flags_owns_inspector = 1 << 2,
  // Set if Node.js should not run its own esm loader. This is needed by some
  // embedders, because it's possible for the Node.js esm loader to conflict
  // with another one in an embedder environment, e.g. Blink's in Chromium.
  node_embedding_runtime_flags_no_register_esm_loader = 1 << 3,
  // Set this flag to make Node.js track "raw" file descriptors, i.e. managed
  // by fs.open() and fs.close(), and close them during
  // node_embedding_delete_runtime().
  node_embedding_runtime_flags_track_unmanaged_fds = 1 << 4,
  // Set this flag to force hiding console windows when spawning child
  // processes. This is usually used when embedding Node.js in GUI programs on
  // Windows.
  node_embedding_runtime_flags_hide_console_windows = 1 << 5,
  // Set this flag to disable loading native addons via `process.dlopen`.
  // This environment flag is especially important for worker threads
  // so that a worker thread can't load a native addon even if `execArgv`
  // is overwritten and `--no-addons` is not specified but was specified
  // for this Environment instance.
  node_embedding_runtime_flags_no_native_addons = 1 << 6,
  // Set this flag to disable searching modules from global paths like
  // $HOME/.node_modules and $NODE_PATH. This is used by standalone apps that
  // do not expect to have their behaviors changed because of globally
  // installed modules.
  node_embedding_runtime_flags_no_global_search_paths = 1 << 7,
  // Do not export browser globals like setTimeout, console, etc.
  node_embedding_runtime_flags_no_browser_globals = 1 << 8,
  // Controls whether or not the Environment should call V8Inspector::create().
  // This control is needed by embedders who may not want to initialize the V8
  // inspector in situations where one has already been created,
  // e.g. Blink's in Chromium.
  node_embedding_runtime_flags_no_create_inspector = 1 << 9,
  // Controls whether or not the InspectorAgent for this Environment should
  // call StartDebugSignalHandler. This control is needed by embedders who may
  // not want to allow other processes to start the V8 inspector.
  node_embedding_runtime_flags_no_start_debug_signal_handler = 1 << 10,
  // Controls whether the InspectorAgent created for this Environment waits for
  // Inspector frontend events during the Environment creation. It's used to
  // call node::Stop(env) on a Worker thread that is waiting for the events.
  node_embedding_runtime_flags_no_wait_for_inspector_frontend = 1 << 11
} node_embedding_runtime_flags;

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
      char* argv[],
      node_embedding_configure_platform_callback configure_platform,
      void* configure_platform_data,
      node_embedding_configure_runtime_callback configure_runtime,
      void* configure_runtime_data);

  node_embedding_status(NAPI_CDECL* create_platform)(
      int32_t argc,
      char* argv[],
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

NAPI_EXTERN node_embedding_status NAPI_CDECL
node_embedding_get_api_vtable(node_embedding_api_vtable** api_vtable);

//==============================================================================
// Functions
//==============================================================================

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
    char* argv[],
    node_embedding_configure_platform_callback configure_platform,
    void* configure_platform_data,
    node_embedding_configure_runtime_callback configure_runtime,
    void* configure_runtime_data);

// Creates and configures a new Node.js platform instance.
NAPI_EXTERN node_embedding_status NAPI_CDECL node_embedding_create_platform(
    int32_t argc,
    char* argv[],
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

//==============================================================================
// C++ convenience functions for the C API.
// These functions are not ABI safe and can be changed in future versions.
//==============================================================================

//------------------------------------------------------------------------------
// Convenience union operator for the Node.js flags.
//------------------------------------------------------------------------------

inline constexpr node_embedding_platform_flags operator|(
    node_embedding_platform_flags lhs, node_embedding_platform_flags rhs) {
  return static_cast<node_embedding_platform_flags>(static_cast<int32_t>(lhs) |
                                                    static_cast<int32_t>(rhs));
}

inline constexpr node_embedding_runtime_flags operator|(
    node_embedding_runtime_flags lhs, node_embedding_runtime_flags rhs) {
  return static_cast<node_embedding_runtime_flags>(static_cast<int32_t>(lhs) |
                                                   static_cast<int32_t>(rhs));
}

namespace node::embedding {

enum class NodeStatus : int32_t {
  kOk = 0,
  kGenericError = 1,
  kNullArg = 2,
  kBadArg = 3,
  // This value is added to the exit code in cases when Node.js API returns
  // an error exit code.
  kErrorExitCode = 512,
};

enum class NodePlatformFlags : int32_t {
  None = 0,
  EnableStdioInheritance = 1 << 0,
  DisableNodeOptionsEnv = 1 << 1,
  DisableCliOptions = 1 << 2,
  NoIcu = 1 << 3,
  NoStdioInitialization = 1 << 4,
  NoDefaultSignalHandling = 1 << 5,
  NoInitOpenSsl = 1 << 8,
  NoParseGlobalDebugVariables = 1 << 9,
  NoAdjustResourceLimits = 1 << 10,
  NoUseLargePages = 1 << 11,
  NoPrintHelpOrVersionOutput = 1 << 12,
  GeneratePredictableSnapshot = 1 << 14,
};

inline constexpr NodePlatformFlags operator|(NodePlatformFlags lhs,
                                             NodePlatformFlags rhs) {
  return static_cast<NodePlatformFlags>(static_cast<int32_t>(lhs) |
                                        static_cast<int32_t>(rhs));
}

inline constexpr NodePlatformFlags operator&(NodePlatformFlags lhs,
                                             NodePlatformFlags rhs) {
  return static_cast<NodePlatformFlags>(static_cast<int32_t>(lhs) &
                                        static_cast<int32_t>(rhs));
}

enum class NodeRuntimeFlags : int32_t {
  None = 0,
  Default = 1 << 0,
  OwnsProcessState = 1 << 1,
  OwnsInspector = 1 << 2,
  NoRegisterEsmLoader = 1 << 3,
  TrackUnmanagedFds = 1 << 4,
  HideConsoleWindows = 1 << 5,
  NoNativeAddons = 1 << 6,
  NoGlobalSearchPaths = 1 << 7,
  NoBrowserGlobals = 1 << 8,
  NoCreateInspector = 1 << 9,
  NoStartDebugSignalHandler = 1 << 10,
  NoWaitForInspectorFrontend = 1 << 11
};

inline constexpr NodeRuntimeFlags operator|(NodeRuntimeFlags lhs,
                                            NodeRuntimeFlags rhs) {
  return static_cast<NodeRuntimeFlags>(static_cast<int32_t>(lhs) |
                                       static_cast<int32_t>(rhs));
}

inline constexpr NodeRuntimeFlags operator&(NodeRuntimeFlags lhs,
                                            NodeRuntimeFlags rhs) {
  return static_cast<NodeRuntimeFlags>(static_cast<int32_t>(lhs) &
                                       static_cast<int32_t>(rhs));
}
#if 0
class NodeRuntimeConfig {
 public:
  NodeRuntimeConfig() {
    node_embedding_runtime_config runtime_config{};
    //node_embedding_set_runtime_flags(runtime_config, NodeRuntimeFlags::Default);
    runtime_config_ = runtime_config;
  }

  NodeRuntimeConfig(const NodeRuntimeConfig&) = delete;
  NodeRuntimeConfig& operator=(const NodeRuntimeConfig&) = delete;

  NodeRuntimeConfig(NodeRuntimeConfig&& other) noexcept
      : runtime_config_(other.runtime_config_) {
    other.runtime_config_ = nullptr;
  }

  NodeRuntimeConfig& operator=(NodeRuntimeConfig&& other) noexcept {
    if (this != &other) {
      runtime_config_ = other.runtime_config_;
      other.runtime_config_ = nullptr;
    }
    return *this;
  }

  ~NodeRuntimeConfig() {
    if (runtime_config_) {
      node_embedding_delete_runtime_config(runtime_config_);
    }
  }

  node_embedding_runtime_config Get() const { return runtime_config_; }

  void SetArgs(int32_t argc,
               const char* argv[],
               int32_t runtime_argc,
               const char* runtime_argv[]) {
    node_embedding_set_runtime_args(
        runtime_config_, argc, argv, runtime_argc, runtime_argv);
  }

  using PreloadCallback = std::function<void(napi_env, napi_value, napi_value)>;

  void OnPreload(PreloadCallback preloadCallback) {
    node_embedding_on_preload_runtime(
        runtime_config_, run_preload, preload_data, release_preload_data);
  }

  template <typename TPreload>
  void OnPreload(TPreload&& preloadCallback) {
    node_embedding_on_preload_runtime(
        runtime_config_, run_preload, preload_data, release_preload_data);
  }

  //// Sets the start execution callback for the Node.js runtime initialization.
  // NAPI_EXTERN node_embedding_status NAPI_CDECL
  // node_embedding_on_start_runtime_execution(
  //     node_embedding_runtime_config runtime_config,
  //     node_embedding_start_execution_callback start_execution,
  //     void* start_execution_data,
  //     node_embedding_release_data_callback release_start_execution_data);

  // NAPI_EXTERN node_embedding_status NAPI_CDECL
  // node_embedding_on_handle_runtime_start_result(
  //     node_embedding_runtime_config runtime_config,
  //     node_embedding_handle_start_result_callback handle_result,
  //     void* handle_result_data,
  //     node_embedding_release_data_callback release_handle_result_data);

  //// Adds a new module to the Node.js runtime.
  //// It is accessed as process._linkedBinding(module_name) in the main JS and
  /// in / the related worker threads.
  // NAPI_EXTERN node_embedding_status NAPI_CDECL
  // node_embedding_add_runtime_module(
  //     node_embedding_runtime_config runtime_config,
  //     const char* module_name,
  //     node_embedding_initialize_module_callback init_module,
  //     void* init_module_data,
  //     node_embedding_release_data_callback release_init_module_data,
  //     int32_t module_node_api_version);

 private:
  node_embedding_runtime_config runtime_config_{};
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
  NodeStatus RunEventLoop() {
    return static_cast<NodeStatus>(node_embedding_run_event_loop(runtime_));
  }

  NodeStatus TerminateEventLoop() {
    return static_cast<NodeStatus>(
        node_embedding_terminate_event_loop(runtime_));
  }

  NodeStatus RunEventLoopOnce(bool* has_more_work) {
    return static_cast<NodeStatus>(
        node_embedding_run_event_loop_once(runtime_, has_more_work));
  }

  NodeStatus RunEventLoopNoWait(bool* has_more_work) {
    return static_cast<NodeStatus>(
        node_embedding_run_event_loop_no_wait(runtime_, has_more_work));
  }

  template <typename TRunNodeApi>
  void Run(TRunNodeApi&& runNodeApi) {
    node_embedding_run_node_api(
        runtime_,
        [](void* cb_data, node_embedding_runtime runtime, napi_env env) {
          TRunNodeApi* runNodeApi = static_cast<TRunNodeApi*>(cb_data);
          (*runNodeApi)(runtime, env);
        },
        &runNodeApi);
  }

 private:
  node_embedding_runtime runtime_{};
};
#endif
}  // namespace node::embedding

#endif

#endif  // SRC_NODE_EMBEDDING_API_H_
