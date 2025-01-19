#include "embedtest_c_api_common.h"

using namespace node::embedding;

// Tests that the same preload callback is called from the main thread and from
// the worker thread.
extern "C" int32_t test_main_preload_node_api(int32_t argc, char* argv[]) {
  return PrintErrorMessage(
             argv[0],
             NodePlatform::RunMain(
                 NodeArgs(argc, argv),
                 nullptr,
                 [](const NodePlatform& platform,
                    const NodeRuntimeConfig& runtime_config) {
                   CHECK_EXPECTED(
                       runtime_config.OnPreload([](const NodeRuntime& runtime,
                                                   napi_env env,
                                                   napi_value /*process*/,
                                                   napi_value /*require*/
                                                ) {
                         napi_value global, value;
                         NODE_API_CALL_RETURN_VOID(
                             napi_get_global(env, &global));
                         NODE_API_CALL_RETURN_VOID(
                             napi_create_int32(env, 42, &value));
                         NODE_API_CALL_RETURN_VOID(napi_set_named_property(
                             env, global, "preloadValue", value));
                         return NodeExpected<void>();
                       }));

                   CHECK_EXPECTED(LoadUtf8Script(runtime_config, main_script));

                   return NodeExpected<void>();
                 }))
      .exit_code();
}
