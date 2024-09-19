#ifndef TEST_EMBEDDING_EMBEDTEST_NODE_API_H_
#define TEST_EMBEDDING_EMBEDTEST_NODE_API_H_

#define NAPI_EXPERIMENTAL
#include <node_embedding_api.h>

#ifdef __cplusplus

#include <functional>
#include <string>
#include <vector>

extern "C" inline void NAPI_CDECL GetArgsVector(void* data,
                                                int32_t argc,
                                                const char* argv[]) {
  static_cast<std::vector<std::string>*>(data)->assign(argv, argv + argc);
}

extern "C" inline node_embedding_exit_code NAPI_CDECL
HandleTestError(void* handler_data,
                const char* messages[],
                size_t messages_size,
                node_embedding_exit_code exit_code) {
  auto exe_name = static_cast<const char*>(handler_data);
  if (exit_code != 0) {
    for (size_t i = 0; i < messages_size; ++i)
      fprintf(stderr, "%s: %s\n", exe_name, messages[i]);
    exit(static_cast<int32_t>(exit_code));
  } else {
    for (size_t i = 0; i < messages_size; ++i) printf("%s\n", messages[i]);
  }
  return node_embedding_exit_code_ok;
}

#endif

extern const char* main_script;

napi_status AddUtf8String(std::string& str, napi_env env, napi_value value);

void GetAndThrowLastErrorMessage(napi_env env);

void ThrowLastErrorMessage(napi_env env, const char* message);

std::string FormatString(const char* format, ...);

inline node_embedding_exit_code RunMain(
    int32_t argc,
    char* argv[],
    const std::function<node_embedding_exit_code(node_embedding_platform)>&
        configurePlatform,
    const std::function<node_embedding_exit_code(
        node_embedding_platform, node_embedding_runtime)>& configureRuntime,
    const std::function<void(node_embedding_runtime, napi_env)>& runNodeApi) {
  return node_embedding_run_main(
      argc,
      argv,
      configurePlatform ?
        [](void* cb_data, node_embedding_platform platform) {
          auto configurePlatform = static_cast<
              std::function<node_embedding_exit_code(node_embedding_platform)>*>(
              cb_data);
          return (*configurePlatform)(platform);
        } : nullptr,
      const_cast<
          std::function<node_embedding_exit_code(node_embedding_platform)>*>(
          &configurePlatform),
      configureRuntime ?
        [](void* cb_data,
           node_embedding_platform platform,
           node_embedding_runtime runtime) {
          auto configureRuntime =
              static_cast<std::function<node_embedding_exit_code(
                  node_embedding_platform, node_embedding_runtime)>*>(cb_data);
          return (*configureRuntime)(platform, runtime);
        } : nullptr,
      const_cast<std::function<node_embedding_exit_code(
          node_embedding_platform, node_embedding_runtime)>*>(
          &configureRuntime),
      runNodeApi ?
        [](void* cb_data, node_embedding_runtime runtime, napi_env env) {
          auto runNodeApi =
              static_cast<std::function<void(node_embedding_runtime, napi_env)>*>(
                  cb_data);
          (*runNodeApi)(runtime, env);
        } : nullptr,
      const_cast<std::function<void(node_embedding_runtime, napi_env)>*>(
          &runNodeApi));
}

inline node_embedding_exit_code RunRuntime(
    node_embedding_platform platform,
    const std::function<node_embedding_exit_code(
        node_embedding_platform, node_embedding_runtime)>& configureRuntime,
    const std::function<void(node_embedding_runtime, napi_env)>& runNodeApi) {
  return node_embedding_run_runtime(
      platform,
      configureRuntime ?
        [](void* cb_data,
           node_embedding_platform platform,
           node_embedding_runtime runtime) {
          auto configureRuntime =
              static_cast<std::function<node_embedding_exit_code(
                  node_embedding_platform, node_embedding_runtime)>*>(cb_data);
          return (*configureRuntime)(platform, runtime);
        } : nullptr,
      const_cast<std::function<node_embedding_exit_code(
          node_embedding_platform, node_embedding_runtime)>*>(
          &configureRuntime),
      runNodeApi ?
        [](void* cb_data, node_embedding_runtime runtime, napi_env env) {
          auto runNodeApi =
              static_cast<std::function<void(node_embedding_runtime, napi_env)>*>(
                  cb_data);
          (*runNodeApi)(runtime, env);
        } : nullptr,
      const_cast<std::function<void(node_embedding_runtime, napi_env)>*>(
          &runNodeApi));
}

inline node_embedding_exit_code CreateRuntime(
    node_embedding_platform platform,
    const std::function<node_embedding_exit_code(
        node_embedding_platform, node_embedding_runtime)>& configureRuntime,
    node_embedding_runtime* runtime) {
  return node_embedding_create_runtime(
      platform,
      configureRuntime ?
        [](void* cb_data,
           node_embedding_platform platform,
           node_embedding_runtime runtime) {
          auto configureRuntime =
              static_cast<std::function<node_embedding_exit_code(
                  node_embedding_platform, node_embedding_runtime)>*>(cb_data);
          return (*configureRuntime)(platform, runtime);
        } : nullptr,
      const_cast<std::function<node_embedding_exit_code(
          node_embedding_platform, node_embedding_runtime)>*>(
          &configureRuntime),
      runtime);
}

inline node_embedding_exit_code RunNodeApi(
    node_embedding_runtime runtime,
    const std::function<void(node_embedding_runtime, napi_env)>& func) {
  return node_embedding_run_node_api(
      runtime,
      [](void* cb_data, node_embedding_runtime runtime, napi_env env) {
        auto func =
            static_cast<std::function<void(node_embedding_runtime, napi_env)>*>(
                cb_data);
        (*func)(runtime, env);
      },
      const_cast<std::function<void(node_embedding_runtime, napi_env)>*>(
          &func));
}

#define NODE_API_CALL(expr)                                                    \
  do {                                                                         \
    if ((expr) != napi_ok) {                                                   \
      GetAndThrowLastErrorMessage(env);                                        \
      exit_code = 1;                                                           \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define CHECK_NAPI(expr)                                                       \
  do {                                                                         \
    if ((expr) != napi_ok) {                                                   \
      goto fail;                                                               \
    }                                                                          \
  } while (0)

#define CHECK_STATUS_NAPI(expr)                                                \
  do {                                                                         \
    if ((expr) != node_embedding_exit_code_ok) {                               \
      ThrowLastErrorMessage(env, "Embedding API failed");                      \
      goto fail;                                                               \
    }                                                                          \
  } while (0)

#define CHECK_STATUS(expr)                                                     \
  do {                                                                         \
    exit_code = (expr);                                                        \
    if (exit_code != node_embedding_exit_code_ok) {                            \
      goto fail;                                                               \
    }                                                                          \
  } while (0)

#define CHECK_RETURN_VOID(expr)                                                \
  do {                                                                         \
    if ((expr) != node_embedding_exit_code_ok) {                               \
      fprintf(stderr, "Failed: %s\n", #expr);                                  \
      fprintf(stderr, "File: %s\n", __FILE__);                                 \
      fprintf(stderr, "Line: %d\n", __LINE__);                                 \
      exit_code = node_embedding_exit_code_generic_user_error;                 \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define CHECK_TRUE(expr)                                                       \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "Failed: %s\n", #expr);                                  \
      fprintf(stderr, "File: %s\n", __FILE__);                                 \
      fprintf(stderr, "Line: %d\n", __LINE__);                                 \
      return 1;                                                                \
    }                                                                          \
  } while (0)

#define CHECK_TRUE_RETURN_VOID(expr)                                           \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "Failed: %s\n", #expr);                                  \
      fprintf(stderr, "File: %s\n", __FILE__);                                 \
      fprintf(stderr, "Line: %d\n", __LINE__);                                 \
      exit_code = node_embedding_exit_code_generic_user_error;                 \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define FAIL(...)                                                              \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    return 1;                                                                  \
  } while (0)

#define FAIL_NAPI(...)                                                         \
  do {                                                                         \
    ThrowLastErrorMessage(env, FormatString(__VA_ARGS__).c_str());             \
    goto fail;                                                                 \
  } while (0)

#define CHECK_EXIT_CODE(code)                                                  \
  do {                                                                         \
    int exit_code = (code);                                                    \
    if (exit_code != 0) {                                                      \
      return exit_code;                                                        \
    }                                                                          \
  } while (0)

#endif  // TEST_EMBEDDING_EMBEDTEST_NODE_API_H_
