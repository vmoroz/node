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

##### `node_platform_options`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

This is an opaque pointer that represents a set of Node.js platform options.

##### `node_platform`

<!-- YAML
added: REPLACEME
-->

This is an opaque pointer that represents a Node.js platform.

> Stability: 1 - Experimental

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

##### `node_error_handler`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

```c
typedef void(*node_error_handler)(void* handler_data,
                                  const char* messages[],
                                  size_t size,
                                  int32_t exit_code);
```

Function pointer type for user-provided native functions that handles the list
of error messages and the exit code.

The callback parameters:

- `[in] handler_data`: The user data associated with this callback.
- `[in] messages`: Pointer to an array of zero terminating C-strings.
- `[in] size`: Size of the messages string array.
- `[in] exit_code`: The process exit code in case of error. It is not 0 if error
  happened. The callback can be used to report non-error output if the
  `exit_code` is 0.

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

##### `node_create_platform_options`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Creates new platform options.

```c
napi_status
node_create_platform_options(node_platform_options *result);
```

- `[out] result`: A pointer to the new platform options.

Returns `napi_ok` if there were no issues.

##### `node_delete_platform_options`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Deletes platform options.

```c
napi_status
node_delete_platform_options(node_platform_options options);
```

- `[in] options`: The platform options to delete.

Returns `napi_ok` if there were no issues.

##### `node_platform_options_frozen`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Checks if the platform options are frozen.

```c
napi_status
node_platform_options_frozen(node_platform_options options, bool* result);
```

- `[in] options`: The platform options to check.
- `[out] result`: `true` if the platform options are frozen.

Returns `napi_ok` if there were no issues.

The platform options can be changed until they are frozen.
They are frozen after they are passed to the `node_create_platform` function
to create a new platform. After that all changes to the platform options are
prohibited since they are frozen.

##### `node_platform_options_args`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Sets the CLI args for the Node.js platform.

```c
napi_status
node_platform_options_args(node_platform_options options,
                           int32_t argc,
                           const char* argv[]);
```

- `[in] options`: The platform options to configure.
- `[in] argc`: Number of items in the `argv` array.
- `[in] argv`: CLI arguments as an array of zero terminating C-strings.

Returns `napi_ok` if there were no issues.

##### `node_platform_options_flags`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Sets the CLI args for the Node.js platform.

```c
napi_status
node_platform_options_flags(node_platform_options options,
                            node_platform_flags flags);
```

- `[in] options`: The platform options to configure.
- `[in] flags`: The platform flags that control the platform behavior.

Returns `napi_ok` if there were no issues.

##### `node_platform_options_on_error`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Sets the custom Node.js platform error handler.

```c
napi_status
node_platform_options_flags(node_platform_options options,
                            node_platform_flags flags);
```

- `[in] options`: The platform options to configure.
- `[in] flags`: The platform flags that control the platform behavior.

Returns `napi_ok` if there were no issues.

##### `node_create_platform`

##### `node_initialize_platform`

##### `node_delete_platform`

##### `node_platform_get_args`

##### `node_platform_get_exec_args`

### Runtime instance APIs

#### Data types

##### `node_runtime_options`

##### `node_runtime`

##### `node_runtime_flags`

#### Callback types

##### `node_runtime_error_handler`

##### `node_runtime_store_blob_callback`

##### `node_runtime_preload_callback`

#### Functions

##### `node_create_runtime_options`

##### `node_delete_runtime_options`

##### `node_runtime_options_frozen`

##### `node_runtime_options_on_error`

##### `node_runtime_options_flags`

##### `node_runtime_options_args`

##### `node_runtime_options_exec_args`

##### `node_runtime_options_on_preload`

##### `node_runtime_options_snapshot`

##### `node_runtime_options_on_store_snapshot`

##### `node_create_runtime`

##### `node_delete_runtime`

### Event loop APIs

#### Functions

##### `node_runtime_run_event_loop`

##### `node_runtime_run_event_loop_while`

##### `node_runtime_await_promise`

### JavaScript/Native interop APIs

#### Functions

##### `node_runtime_run_task`

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
