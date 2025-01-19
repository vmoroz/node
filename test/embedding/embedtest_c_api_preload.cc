#include "embedtest_c_api_common.h"

#include <mutex>
#include <thread>
#if 0
using namespace node::embedding;

// Tests that the same preload callback is called from the main thread and from
// the worker thread.
extern "C" int32_t test_main_preload_node_api(int32_t argc, char* argv[]) {
  CHECK_EXPECTED_OR_EXIT(
      NodePlatform::RunMain(NodeArgs(argc, argv),
                            nullptr,
                            [](node_embedding_platform platform,
                               node_embedding_runtime_config runtime_config) {
                              // CHECK_STATUS(runtime_config.OnPreload(
                              //        [](NodeRuntime& runtime,
                              //           napi_env env,
                              //           napi_value /*process*/,
                              //           napi_value /*require*/
                              //        ) {
                              //  napi_value global, value;
                              //  NODE_API_CALL_RETURN_VOID(napi_get_global(env,
                              //  &global));
                              //  NODE_API_CALL_RETURN_VOID(napi_create_int32(env,
                              //  42, &value)); NODE_API_CALL_RETURN_VOID(
                              //      napi_set_named_property(env, global,
                              //      "preloadValue", value));
                              //        })));

                              // CHECK_STATUS(LoadUtf8Script(runtime_config,
                              // main_script));

                              return NodeStatus::kOk;
                            }));

  return 0;
}
#endif
