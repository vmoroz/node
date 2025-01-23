#include "embedtest_c_api_common.h"

static void OnPreload(void* cb_data,
                      node_embedding_runtime runtime,
                      napi_env env,
                      napi_value process,
                      napi_value require) {
  napi_value global, value;
  NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
  NODE_API_CALL_RETURN_VOID(napi_create_int32(env, 42, &value));
  NODE_API_CALL_RETURN_VOID(
      napi_set_named_property(env, global, "preloadValue", value));
}

static node_embedding_status ConfigureRuntime(
    void* cb_data,
    node_embedding_platform platform,
    node_embedding_runtime_config runtime_config) {
  NODE_EMBEDDED_CALL(
      node_embedding_on_preload_runtime(runtime_config, OnPreload, NULL, NULL));
  NODE_EMBEDDED_CALL(LoadUtf8Script(runtime_config, main_script));
  return node_embedding_status_ok;
}

// Tests that the same preload callback is called from the main thread and from
// the worker thread.
int32_t test_main_c_api_preload(int32_t argc, char* argv[]) {
  CHECK_EXPECTED_OR_EXIT(argv[0],
                         node_embedding_run_main(NODE_EMBEDDING_VERSION,
                                                 argc,
                                                 argv,
                                                 NULL,
                                                 NULL,
                                                 ConfigureRuntime,
                                                 NULL));
  return 0;
}
