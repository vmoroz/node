#include "embedtest_c_api_common.h"

using namespace node::embedding;

// Tests that the same preload callback is called from the main thread and from
// the worker thread.
extern "C" int32_t test_main_preload_c_cpp_api(int32_t argc, char* argv[]) {
  NodeExpected<void> result = NodePlatform::RunMain(
      NodeArgs(argc, argv),
      nullptr,
      [](const NodePlatform& platform,
         const NodeRuntimeConfig& runtime_config) {
        NODE_EMBEDDED_CALL(
            runtime_config.OnPreload([](const NodeRuntime& runtime,
                                        napi_env env,
                                        napi_value /*process*/,
                                        napi_value /*require*/
                                     ) {
              napi_value global, value;
              NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
              NODE_API_CALL_RETURN_VOID(napi_create_int32(env, 42, &value));
              NODE_API_CALL_RETURN_VOID(
                  napi_set_named_property(env, global, "preloadValue", value));
            }));

        NODE_EMBEDDED_CALL(LoadUtf8Script(runtime_config, main_script));

        return NodeExpected<void>();
      });
  return PrintErrorMessage(argv[0], std::move(result)).exit_code();
}
