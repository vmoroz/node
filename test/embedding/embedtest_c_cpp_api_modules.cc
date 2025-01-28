#include "embedtest_c_cpp_api_common.h"

#include <atomic>
#include <cstdio>
#include <cstring>

namespace node::embedding {

class GreeterModule {
 public:
  explicit GreeterModule(std::atomic<int32_t>* counter_ptr)
      : counter_ptr_(counter_ptr) {}

  napi_value operator()(const NodeRuntime& runtime,
                        napi_env env,
                        std::string_view module_name,
                        napi_value exports) {
    NodeApiErrorHandler<napi_value> error_handler(env);
    counter_ptr_->fetch_add(1);

    napi_value greet_func{};
    NODE_API_CALL(napi_create_function(
        env,
        "greet",
        NAPI_AUTO_LENGTH,
        [](napi_env env, napi_callback_info info) -> napi_value {
          NodeApiErrorHandler<napi_value> error_handler(env);
          std::string greeting = "Hello, ";
          napi_value arg{};
          size_t arg_count = 1;
          NODE_API_CALL(
              napi_get_cb_info(env, info, &arg_count, &arg, nullptr, nullptr));
          NODE_API_CALL(AddUtf8String(greeting, env, arg));
          napi_value result;
          NODE_API_CALL(napi_create_string_utf8(
              env, greeting.c_str(), greeting.size(), &result));
          return result;
        },
        nullptr,
        &greet_func));
    NODE_API_CALL(napi_set_named_property(env, exports, "greet", greet_func));
    return exports;
  }

 private:
  std::atomic<int32_t>* counter_ptr_;
};

class ReplicatorModule {
 public:
  explicit ReplicatorModule(std::atomic<int32_t>* counter_ptr)
      : counter_ptr_(counter_ptr) {}

  napi_value operator()(const NodeRuntime& runtime,
                        napi_env env,
                        std::string_view module_name,
                        napi_value exports) {
    NodeApiErrorHandler<napi_value> error_handler(env);
    counter_ptr_->fetch_add(1);

    napi_value greet_func{};
    NODE_API_CALL(napi_create_function(
        env,
        "replicate",
        NAPI_AUTO_LENGTH,
        [](napi_env env, napi_callback_info info) -> napi_value {
          NodeApiErrorHandler<napi_value> error_handler(env);
          std::string str;
          napi_value arg{};
          size_t arg_count = 1;
          NODE_API_CALL(
              napi_get_cb_info(env, info, &arg_count, &arg, nullptr, nullptr));
          NODE_API_CALL(AddUtf8String(str, env, arg));
          str += " " + str;
          napi_value result;
          NODE_API_CALL(
              napi_create_string_utf8(env, str.c_str(), str.size(), &result));
          return result;
        },
        nullptr,
        &greet_func));
    NODE_API_CALL(
        napi_set_named_property(env, exports, "replicate", greet_func));
    return exports;
  }

 private:
  std::atomic<int32_t>* counter_ptr_;
};

extern "C" int32_t test_main_c_cpp_api_linked_modules(int32_t argc,
                                                      char* argv[]) {
  TestExitCodeHandler error_handler(argv[0]);
  NODE_ASSERT(argc == 4);
  int32_t expectedGreeterModuleInitCallCount = atoi(argv[2]);
  int32_t expectedReplicatorModuleInitCallCount = atoi(argv[2]);

  std::atomic<int32_t> greeterModuleInitCallCount{0};
  std::atomic<int32_t> replicatorModuleInitCallCount{0};

  NODE_EMBEDDING_CALL(NodePlatform::RunMain(
      NodeArgs(argc, argv),
      nullptr,
      NodeConfigureRuntimeCallback(
          [&](const NodePlatform& platform,
              const NodeRuntimeConfig& runtime_config) {
            NodeEmbeddingErrorHandler error_handler;
            NODE_EMBEDDING_CALL(
                runtime_config.OnPreload([](const NodeRuntime& runtime,
                                            napi_env env,
                                            napi_value process,
                                            napi_value /*require*/
                                         ) {
                  napi_value global;
                  napi_get_global(env, &global);
                  napi_set_named_property(env, global, "process", process);
                }));

            NODE_EMBEDDING_CALL(runtime_config.AddModule(
                "greeter_module",
                GreeterModule(&greeterModuleInitCallCount),
                NAPI_VERSION));

            NODE_EMBEDDING_CALL(runtime_config.AddModule(
                "replicator_module",
                ReplicatorModule(&replicatorModuleInitCallCount),
                NAPI_VERSION));

            NODE_EMBEDDING_CALL(LoadUtf8Script(runtime_config, main_script));

            return error_handler.ReportResult();
          })));

  NODE_ASSERT(greeterModuleInitCallCount == expectedGreeterModuleInitCallCount);
  NODE_ASSERT(replicatorModuleInitCallCount ==
              expectedReplicatorModuleInitCallCount);

  return error_handler.ReportResult();
}

extern "C" int32_t test_main_modules_node_api(int32_t argc, char* argv[]) {
  /*
  if (argc < 3) {
    fprintf(stderr, "node_api_modules <cjs.cjs> <es6.mjs>\n");
    return 2;
  }

  CHECK(node_embedding_on_error(HandleTestError, argv[0]));

  node_embedding_platform platform;
  CHECK(node_embedding_platform_create(NODE_EMBEDDING_VERSION, &platform));
  CHECK(node_embedding_platform_set_args(platform, argc, argv));
  bool early_return = false;
  CHECK(node_embedding_platform_initialize(platform, &early_return));
  if (early_return) {
    return 0;
  }

  node_embedding_runtime runtime;
  CHECK(node_embedding_runtime_create(platform, &runtime));
  CHECK(node_embedding_runtime_initialize_from_script(runtime));
  int32_t exit_code = 0;
  CHECK(InvokeNodeApi(runtime, [&](napi_env env) {
    napi_value global, import_name, require_name, import, require, cjs, es6,
        value;
    NODE_API_CALL(napi_get_global(env, &global));
    NODE_API_CALL(
        napi_create_string_utf8(env, "import", strlen("import"), &import_name));
    NODE_API_CALL(napi_create_string_utf8(
        env, "require", strlen("require"), &require_name));
    NODE_API_CALL(napi_get_property(env, global, import_name, &import));
    NODE_API_CALL(napi_get_property(env, global, require_name, &require));

    NODE_API_CALL(napi_create_string_utf8(env, argv[1], strlen(argv[1]), &cjs));
    NODE_API_CALL(napi_create_string_utf8(env, argv[2], strlen(argv[2]), &es6));
    NODE_API_CALL(
        napi_create_string_utf8(env, "value", strlen("value"), &value));

    napi_value es6_module, es6_promise, cjs_module, es6_result, cjs_result;
    char buffer[32];
    size_t bufferlen;

    NODE_API_CALL(
        napi_call_function(env, global, import, 1, &es6, &es6_promise));
    node_embedding_promise_state es6_promise_state;
    CHECK_RETURN_VOID(node_embedding_runtime_await_promise(
        runtime, es6_promise, &es6_promise_state, &es6_module, nullptr));

    NODE_API_CALL(napi_get_property(env, es6_module, value, &es6_result));
    NODE_API_CALL(napi_get_value_string_utf8(
        env, es6_result, buffer, sizeof(buffer), &bufferlen));
    if (strncmp(buffer, "genuine", bufferlen) != 0) {
      FAIL_RETURN_VOID("Unexpected value: %s\n", buffer);
    }

    NODE_API_CALL(
        napi_call_function(env, global, require, 1, &cjs, &cjs_module));
    NODE_API_CALL(napi_get_property(env, cjs_module, value, &cjs_result));
    NODE_API_CALL(napi_get_value_string_utf8(
        env, cjs_result, buffer, sizeof(buffer), &bufferlen));
    if (strncmp(buffer, "original", bufferlen) != 0) {
      FAIL_RETURN_VOID("Unexpected value: %s\n", buffer);
    }
  }));
  CHECK(exit_code);
  CHECK(node_embedding_runtime_delete(runtime));
  CHECK(node_embedding_platform_delete(platform));
*/
  return 0;
}
}  // namespace node::embedding
