# C++ embedder API

<!--introduced_in=v12.19.0-->

Node.js provides a number of C++ APIs that can be used to execute JavaScript
in a Node.js environment from other C++ software.

The documentation for these APIs can be found in [src/node.h][] in the Node.js
source tree. In addition to the APIs exposed by Node.js, some required concepts
are provided by the V8 embedder API.

Because using Node.js as an embedded library is different from writing code
that is executed by Node.js, breaking changes do not follow typical Node.js
[deprecation policy][] and may occur on each semver-major release without prior
warning.

## Example embedding application

The following sections will provide an overview over how to use these APIs
to create an application from scratch that will perform the equivalent of
`node -e <code>`, i.e. that will take a piece of JavaScript and run it in
a Node.js-specific environment.

The full code can be found [in the Node.js source tree][embedtest.cc].

### Setting up per-process state

Node.js requires some per-process state management in order to run:

- Arguments parsing for Node.js [CLI options][],
- V8 per-process requirements, such as a `v8::Platform` instance.

The following example shows how these can be set up. Some class names are from
the `node` and `v8` C++ namespaces, respectively.

```cpp
int main(int argc, char** argv) {
  argv = uv_setup_args(argc, argv);
  std::vector<std::string> args(argv, argv + argc);
  // Parse Node.js CLI options, and print any errors that have occurred while
  // trying to parse them.
  std::unique_ptr<node::InitializationResult> result =
      node::InitializeOncePerProcess(args, {
        node::ProcessInitializationFlags::kNoInitializeV8,
        node::ProcessInitializationFlags::kNoInitializeNodeV8Platform
      });

  for (const std::string& error : result->errors())
    fprintf(stderr, "%s: %s\n", args[0].c_str(), error.c_str());
  if (result->early_return() != 0) {
    return result->exit_code();
  }

  // Create a v8::Platform instance. `MultiIsolatePlatform::Create()` is a way
  // to create a v8::Platform instance that Node.js can use when creating
  // Worker threads. When no `MultiIsolatePlatform` instance is present,
  // Worker threads are disabled.
  std::unique_ptr<MultiIsolatePlatform> platform =
      MultiIsolatePlatform::Create(4);
  V8::InitializePlatform(platform.get());
  V8::Initialize();

  // See below for the contents of this function.
  int ret = RunNodeInstance(
      platform.get(), result->args(), result->exec_args());

  V8::Dispose();
  V8::DisposePlatform();

  node::TearDownOncePerProcess();
  return ret;
}
```

### Per-instance state

<!-- YAML
changes:
  - version: v15.0.0
    pr-url: https://github.com/nodejs/node/pull/35597
    description:
      The `CommonEnvironmentSetup` and `SpinEventLoop` utilities were added.
-->

Node.js has a concept of a “Node.js instance”, that is commonly being referred
to as `node::Environment`. Each `node::Environment` is associated with:

- Exactly one `v8::Isolate`, i.e. one JS Engine instance,
- Exactly one `uv_loop_t`, i.e. one event loop, and
- A number of `v8::Context`s, but exactly one main `v8::Context`.
- One `node::IsolateData` instance that contains information that could be
  shared by multiple `node::Environment`s that use the same `v8::Isolate`.
  Currently, no testing is performed for this scenario.

In order to set up a `v8::Isolate`, an `v8::ArrayBuffer::Allocator` needs
to be provided. One possible choice is the default Node.js allocator, which
can be created through `node::ArrayBufferAllocator::Create()`. Using the Node.js
allocator allows minor performance optimizations when addons use the Node.js
C++ `Buffer` API, and is required in order to track `ArrayBuffer` memory in
[`process.memoryUsage()`][].

Additionally, each `v8::Isolate` that is used for a Node.js instance needs to
be registered and unregistered with the `MultiIsolatePlatform` instance, if one
is being used, in order for the platform to know which event loop to use
for tasks scheduled by the `v8::Isolate`.

The `node::NewIsolate()` helper function creates a `v8::Isolate`,
sets it up with some Node.js-specific hooks (e.g. the Node.js error handler),
and registers it with the platform automatically.

```cpp
int RunNodeInstance(MultiIsolatePlatform* platform,
                    const std::vector<std::string>& args,
                    const std::vector<std::string>& exec_args) {
  int exit_code = 0;

  // Setup up a libuv event loop, v8::Isolate, and Node.js Environment.
  std::vector<std::string> errors;
  std::unique_ptr<CommonEnvironmentSetup> setup =
      CommonEnvironmentSetup::Create(platform, &errors, args, exec_args);
  if (!setup) {
    for (const std::string& err : errors)
      fprintf(stderr, "%s: %s\n", args[0].c_str(), err.c_str());
    return 1;
  }

  Isolate* isolate = setup->isolate();
  Environment* env = setup->env();

  {
    Locker locker(isolate);
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    // The v8::Context needs to be entered when node::CreateEnvironment() and
    // node::LoadEnvironment() are being called.
    Context::Scope context_scope(setup->context());

    // Set up the Node.js instance for execution, and run code inside of it.
    // There is also a variant that takes a callback and provides it with
    // the `require` and `process` objects, so that it can manually compile
    // and run scripts as needed.
    // The `require` function inside this script does *not* access the file
    // system, and can only load built-in Node.js modules.
    // `module.createRequire()` is being used to create one that is able to
    // load files from the disk, and uses the standard CommonJS file loader
    // instead of the internal-only `require` function.
    MaybeLocal<Value> loadenv_ret = node::LoadEnvironment(
        env,
        "const publicRequire ="
        "  require('node:module').createRequire(process.cwd() + '/');"
        "globalThis.require = publicRequire;"
        "require('node:vm').runInThisContext(process.argv[1]);");

    if (loadenv_ret.IsEmpty())  // There has been a JS exception.
      return 1;

    exit_code = node::SpinEventLoop(env).FromMaybe(1);

    // node::Stop() can be used to explicitly stop the event loop and keep
    // further JavaScript from running. It can be called from any thread,
    // and will act like worker.terminate() if called from another thread.
    node::Stop(env);
  }

  return exit_code;
}
```

# C embedder API

<!--introduced_in=REPLACEME-->

While Node.js provides an extensive C++ embedding API that can be used from C++
applications, the C-based API is useful when Node.js is embedded as a shared
libnode library into non-C++ applications.

The C embedding API is defined in [src/node_api_embedding.h][] in the Node.js
source tree.

## API design overview

One of the goals for the C based embedder API is to be ABI stable. It means that
applications must be able to use newer libnode versions without recompilation.
The following design principles are targeting to achieve that goal.

- Follow the best practices for the [node-api][] design and build on top of
  the [node-api][].
- Use the [Builder pattern][] as the way to configure the global platform and
  the instance environments. It enables us incrementally add new flags,
  settings, callbacks, and behavior without changing the existing
  functions.
- Use the API version as a way to add new or change existing behavior.
- Make the common scenarios simple and the complex scenarios possible. In some
  cases we provide some "shortcut" APIs that combine calls to multiple other
  APIs to simplify common scenarios.

The C embedder API has the four major API function groups:

- **Global platform APIs.** These are the global settings and initializations
  that are done once per process. They include parsing CLI arguments, setting
  the V8 platform, V8 thread pool, and initializing V8.
- **Runtime instance APIs.** This is the main Node.js working environment that
  combines V8 `Isolate`, `Context`, and a UV loop. It is used run the Node.js
  JavaScript code and modules. In a process we may have one or more runtime
  environments. Its behavior is based on the global platform API.
- **Event loop APIs.** The event loop is one of the key concepts of Node.js. It
  handles IO callbacks, timer jobs, and Promise continuations. These APIs are
  related to a specific Node.js runtime instance and control handling of the
  loop tasks. The loop tasks can be executed in the chosen thread. The API
  controls how many tasks executed at one: all, one-by-one, or until a predicate
  becomes false.
- **JavaScript/Native interop APIs.** Here we rely on the existing [node-api][]
  set of functions. The embedding APIs just provide access to functions that
  retrieve or create `napi_env` instances related to a runtime instance.

## API reference

The C embedder API is split up by the four major groups.

### Global platform APIs

#### Data types

##### `node_platform`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

This is an opaque pointer that represents a Node.js platform.

##### `node_platform_flags`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Flags used to initialize a Node.js platform.

```c
typedef enum {
  node_platform_no_flags = 0,
  node_platform_enable_stdio_inheritance = 1 << 0,
  node_platform_disable_node_options_env = 1 << 1,
  node_platform_disable_cli_options = 1 << 2,
  node_platform_no_icu = 1 << 3,
  node_platform_no_stdio_initialization = 1 << 4,
  node_platform_no_default_signal_handling = 1 << 5,
  node_platform_no_init_openssl = 1 << 8,
  node_platform_no_parse_global_debug_variables = 1 << 9,
  node_platform_no_adjust_resource_limits = 1 << 10,
  node_platform_no_use_large_pages = 1 << 11,
  node_platform_no_print_help_or_version_output = 1 << 12,
  node_platform_generate_predictable_snapshot = 1 << 14,
} node_platform_flags;
```

These flags match to the C++ `node::ProcessInitializationFlags` and control the
Node.js global platform initialization.

- `node_platform_no_flags` - The default flags.
- `node_platform_enable_stdio_inheritance` - Enable stdio inheritance, which is
  disabled by default. This flag is also implied by
  node_platform_no_stdio_initialization.
- `node_platform_disable_node_options_env` - Disable reading the `NODE_OPTIONS`
  environment variable.
- `node_platform_disable_cli_options` - Do not parse CLI options.
- `node_platform_no_icu` - Do not initialize ICU.
- `node_platform_no_stdio_initialization` - Do not modify stdio file descriptor
  or TTY state.
- `node_platform_no_default_signal_handling` - Do not register Node.js-specific
  signal handlers and reset other signal handlers to default state.
- `node_platform_no_init_openssl` - Do not initialize OpenSSL config.
- `node_platform_no_parse_global_debug_variables` - Do not initialize Node.js
  debugging based on environment variables.
- `node_platform_no_adjust_resource_limits` - Do not adjust OS resource limits
  for this process.
- `node_platform_no_use_large_pages` - Do not map code segments into large pages
  for this process.
- `node_platform_no_print_help_or_version_output` - Skip printing output for
  --help, --version, --v8-options.
- `node_platform_generate_predictable_snapshot` - Initialize the process for
  predictable snapshot generation.

#### Callback types

##### `node_platform_get_messages_callback`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

```c
typedef void(*node_platform_get_messages_callback)(void* cb_data,
                                                   const char* messages[],
                                                   size_t size);
```

Function pointer type for user-provided native functions that receives
a list of messages.

The callback parameters:

- `[in] cb_data`: The user data associated with this callback.
- `[in] messages`: An array of zero terminating C-strings.
- `[in] size`: Size of the `messages` array.

##### `node_platform_get_args_callback`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

```c
typedef void(*node_platform_get_args_callback)(void* cb_data,
                                               int32_t argc,
                                               const char* argv[]);
```

Function pointer type for user-provided native functions that receives list of
CLI arguments from the `node_platform`.

The callback parameters:

- `[in] cb_data`: The user data associated with this callback.
- `[in] argc`: Number of items in the `argv` array.
- `[in] argv`: CLI arguments as an array of zero terminating C-strings.

#### Functions

##### `node_create_platform`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Creates new Node.js platform instance.

```c
napi_status node_create_platform(node_platform* result);
```

- `[out] result`: Upon return has a new Node.js platform.

Returns `napi_ok` if there were no issues.

It is a simple object allocation. It does not do any initialization or any
other complex work that may fail besides checking the argument.

Note that Node.js typically allows only a single platform instance per process.

##### `node_delete_platform`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Deletes Node.js platform instance.

```c
napi_status node_delete_platform(node_platform platform);
```

- `[in] platform`: The Node.js platform instance to delete.

Returns `napi_ok` if there were no issues.

If the platform was initialized before the deletion, then the method
un-initializes the platform before deletion.

##### `node_platform_is_initialized`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Checks if the platform is initialized.

```c
napi_status
node_platform_is_initialized(node_platform platform, bool* result);
```

- `[in] platform`: The Node.js platform instance to check.
- `[out] result`: `true` if the platform is already initialized.

Returns `napi_ok` if there were no issues.

The platform settings can be changed until the platform is initialized.
After the `node_platform_initialize` function is called any attempt to change
platform settings will fail.

##### `node_platform_set_args`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Sets the CLI args for the Node.js platform instance.

```c
napi_status
node_platform_set_args(node_platform platform,
                       int32_t argc,
                       const char* argv[]);
```

- `[in] platform`: The Node.js platform instance to configure.
- `[in] argc`: Number of items in the `argv` array.
- `[in] argv`: CLI arguments as an array of zero terminating C-strings.

Returns `napi_ok` if there were no issues.

##### `node_platform_set_flags`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Sets the Node.js platform flags.

```c
napi_status
node_platform_set_flags(node_platform platform, node_platform_flags flags);
```

- `[in] platform`: The Node.js platform instance to configure.
- `[in] flags`: The platform flags that control the platform behavior.

Returns `napi_ok` if there were no issues.

##### `node_initialize_platform`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Initializes the Node.js platform instance.

```c
napi_status
node_initialize_platform(node_platform platform, bool* early_return);
```

- `[in] platform`: The Node.js platform instance to initialize.
- `[out] early_return`: Optional. `true` if the initialization result requires
  early return either because of an error, or if the Node.js completed the work
  based on the provided CLI args. For example, it had printed Node.js version
  or the help text.

Returns `napi_ok` if there were no issues.

The Node.js initialization parses CLI args, and initializes Node.js internals
and the V8 runtime. If the initial work such as printing the Node.js version
number is completed, then the `early_return` is set to `true`. The printed
message text can be accessed by calling the `node_platform_get_error_info`
function.

After the initialization is completed the Node.js platform settings cannot be
changed anymore. The parsed arguments can be accessed by calling the
`node_platform_get_args` and `node_platform_get_exec_args` functions.

##### `node_platform_get_args`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Gets the parsed list of non-Node.js arguments.

```c
napi_status node_platform_get_args(node_platform platform,
                                   node_platform_get_args_callback get_args,
                                   void* get_args_data);
```

- `[in] platform`: The Node.js platform instance to check.
- `[in] get_args`: The callback to receive non-Node.js arguments.
- `[in] get_args_data`: Optional. The callback data that will be passed to the
  callback. It can be deleted right after the function call.

Returns `napi_ok` if there were no issues.

##### `node_platform_get_exec_args`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Gets the parsed list of Node.js arguments.

```c
napi_status
node_platform_get_exec_args(node_platform platform,
                            node_platform_get_args_callback get_args,
                            void* get_args_data);
```

- `[in] platform`: The Node.js platform instance to check.
- `[in] get_args`: The callback to receive Node.js arguments.
- `[in] get_args_data`: Optional. The callback data that will be passed to the
  `get_args` callback. It can be deleted right after the function call.

Returns `napi_ok` if there were no issues.

##### `node_platform_get_error_info`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Gets error info for the last platform function call.

```c
napi_status node_platform_get_error_info(
    node_platform platform,
    node_platform_get_messages_callback get_messages,
    void* get_messages_data,
    int32_t* exit_code);
```

- `[in] platform`: The Node.js platform instance to check.
- `[in] get_messages`: The callback to receive messages.
- `[in] get_messages_data`: Optional. The callback data that will be passed
  to the `get_messages`, callback. It can be deleted right after the
  function call.
- `[out] exit_code`: Optional. A non-zero recommended process exit code if there
  was an error. Otherwise, it is zero.

Returns `napi_ok` if there were no issues.

In case if the `exit_code` is zero and we get some messages, then these are not
error messages, but rather some Node.js output or warnings. For example, it can
be Node.js help text returned in response to the `--help` CLI argument.

### Runtime instance APIs

#### Data types

##### `node_runtime`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

This is an opaque pointer that represents a Node.js runtime.
It wraps up the C++ `node::Environment`.

##### `node_runtime_flags`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Flags used to initialize a Node.js runtime.

```c
typedef enum : uint64_t {
  node_runtime_no_flags = 0,
  node_runtime_default_flags = 1 << 0,
  node_runtime_owns_process_state = 1 << 1,
  node_runtime_owns_inspector = 1 << 2,
  node_runtime_no_register_esm_loader = 1 << 3,
  node_runtime_track_unmanaged_fds = 1 << 4,
  node_runtime_hide_console_windows = 1 << 5,
  node_runtime_no_native_addons = 1 << 6,
  node_runtime_no_global_search_paths = 1 << 7,
  node_runtime_no_browser_globals = 1 << 8,
  node_runtime_no_create_inspector = 1 << 9,
  node_runtime_no_start_debug_signal_handler = 1 << 10,
  node_runtime_no_wait_for_inspector_frontend = 1 << 11
} node_runtime_flags;
```

These flags match to the C++ `node::EnvironmentFlags` and control the
Node.js runtime initialization.

- `node_runtime_no_flags` - No flags set.
- `node_runtime_default_flags` - Use the default behavior for Node.js instances.
- `node_runtime_owns_process_state` - Controls whether this runtime is allowed
  to affect per-process state (e.g. cwd, process title, uid, etc.).
  This is set when using `node_runtime_default_flags`.
- `node_runtime_owns_inspector` - Set if this runtime instance is associated
  with the global inspector handling code (i.e. listening on SIGUSR1).
  This is set when using `node_runtime_default_flags`.
- `node_runtime_no_register_esm_loader` - Set if Node.js should not run its own
  esm loader. This is needed by some embedders, because it's possible for the
  Node.js esm loader to conflict with another one in an embedder environment,
  e.g. Blink's in Chromium.
- `node_runtime_track_unmanaged_fds` - Set this flag to make Node.js track "raw"
  file descriptors, i.e. managed by fs.open() and fs.close(), and close them
  during `node_delete_runtime`.
- `node_runtime_hide_console_windows` - Set this flag to force hiding console
  windows when spawning child processes. This is usually used when embedding
  Node.js in GUI programs on Windows.
- `node_runtime_no_native_addons` - Set this flag to disable loading native
  addons via `process.dlopen`. This runtime flag is especially important for
  worker threads so that a worker thread can't load a native addon even if
  `execArgv` is overwritten and `--no-addons` is not specified but was specified
  for this runtime instance.
- `node_runtime_no_global_search_paths` - Set this flag to disable searching
  modules from global paths like `$HOME/.node_modules` and `$NODE_PATH`. This is
  used by standalone apps that do not expect to have their behaviors changed
  because of globally installed modules.
- `node_runtime_no_browser_globals` - Do not export browser globals like
  setTimeout, console, etc.
- `node_runtime_no_create_inspector` - Controls whether or not the runtime
  should call `V8Inspector::create()`. This control is needed by embedders who
  may not want to initialize the V8 inspector in situations where one has
  already been created, e.g. Blink's in Chromium.
- `node_runtime_no_start_debug_signal_handler` - Controls whether or not the
  `InspectorAgent` for this runtime should call `StartDebugSignalHandler`.
  This control is needed by embedders who may not want to allow other processes
  to start the V8 inspector.
- `node_runtime_no_wait_for_inspector_frontend` - Controls whether the
  `InspectorAgent` created for this runtime waits for Inspector frontend events
  during the runtime creation. It's used to call `node::Stop(env)` on a Worker
  thread that is waiting for the events.

#### Callback types

##### `node_runtime_store_blob_callback`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

```c
typedef void(*node_runtime_store_blob_callback)(void* cb_data,
                                                const uint8_t* blob,
                                                size_t size);
```

Function pointer type for user-provided native function that is called when the
runtime needs to store the snapshot blob.

The callback parameters:

- `[in] cb_data`: The user data associated with this callback.
- `[in] blob`: Start of the blob memory span.
- `[in] size`: Size of the blob memory span.


##### `node_runtime_preload_callback`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

```c
typedef void(*node_runtime_preload_callback)(void* cb_data,
                                             napi_env env,
                                             napi_value process,
                                             napi_value require);
```

Function pointer type for user-provided native function that is called when the
runtime initially loads the JavaScript code.

The callback parameters:

- `[in] cb_data`: The user data associated with this callback.
- `[in] env`: Node-API environmentStart of the blob memory span.
- `[in] process`: The Node.js `process` object.
- `[in] require`: The internal `require` function.

#### Functions

##### `node_create_runtime`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Creates new Node.js runtime instance.

```c
napi_status node_create_runtime(node_platform platform, node_runtime* result);
```

- `[in] platform`: Optional. An initialized Node.js platform instance.
- `[out] result`: Upon return has a new Node.js runtime instance.

Returns `napi_ok` if there were no issues.

Creates new Node.js runtime instance based on the provided platform instance.

It is a simple object allocation. It does not do any initialization or any
other complex work that may fail besides checking the arguments.

If the platform instance is `NULL` then a default platform instance will be
created internally when the `node_platform_initialize` is called. Since there
can be only one platform instance per process, only one runtime instance can be
created this way per process.

If it is planned to create more than one runtime or non-default platform
configuration required, then it is recommended to create the Node.js platform
explicitly.

##### `node_delete_runtime`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Deletes Node.js runtime instance.

```c
napi_status node_delete_runtime(node_runtime runtime);
```

- `[in] runtime`: The Node.js runtime instance to delete.

Returns `napi_ok` if there were no issues.

If the runtime was initialized before the deletion, then the method
un-initializes the runtime before deletion.

As a part of the un-initialization it can store created snapshot blob if the
`node_runtime_on_store_snapshot` set the callback to save the snapshot blob.

##### `node_runtime_is_initialized`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Checks if the Node.js runtime is initialized.

```c
napi_status
node_runtime_is_initialized(node_runtime runtime, bool* result);
```

- `[in] runtime`: The Node.js runtime instance to check.
- `[out] result`: `true` if the runtime is already initialized.

Returns `napi_ok` if there were no issues.

The runtime settings can be changed until the runtime is initialized.
After the `node_runtime_initialize` function is called any attempt to change
runtime settings will fail.

##### `node_runtime_on_error`

##### `node_runtime_set_flags`

##### `node_runtime_set_args`

##### `node_runtime_set_exec_args`

##### `node_runtime_on_preload`

##### `node_runtime_use_snapshot`

##### `node_runtime_on_store_snapshot`

##### `node_runtime_initialize`

### Event loop APIs

#### Functions

##### `node_runtime_run_event_loop`

##### `node_runtime_run_event_loop_while`

##### `node_runtime_await_promise`

### JavaScript/Native interop APIs

#### Functions

##### `node_runtime_run_in_scope`

##### `node_runtime_open_scope`

##### `node_runtime_close_scope`

## Examples

The examples listed here are part of the Node.js [embedding unit tests][test_embedding].

```c
  // TODO: add example here.
```

[Builder pattern]: https://en.wikipedia.org/wiki/Builder_pattern
[CLI options]: cli.md
[`process.memoryUsage()`]: process.md#processmemoryusage
[deprecation policy]: deprecations.md
[embedtest.cc]: https://github.com/nodejs/node/blob/HEAD/test/embedding/embedtest.cc
[test_embedding]: https://github.com/nodejs/node/blob/HEAD/test/embedding
[node-api]: n-api.md
[src/node.h]: https://github.com/nodejs/node/blob/HEAD/src/node.h
