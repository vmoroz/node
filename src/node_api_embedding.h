#ifndef SRC_NODE_API_EMBEDDING_H_
#define SRC_NODE_API_EMBEDDING_H_

#include "js_native_api.h"
#include "js_native_api_types.h"

typedef struct node_api_platform__* node_api_platform;

EXTERN_C_START

typedef void(NAPI_CDECL* node_api_error_message_handler)(const char* msg);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_create_platform(int argc,
                         char** argv,
                         node_api_error_message_handler err_handler,
                         node_api_platform* result);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_destroy_platform(node_api_platform platform);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_create_environment(node_api_platform platform,
                            node_api_error_message_handler err_handler,
                            const char* main_script,
                            int32_t api_version,
                            napi_env* result);

NAPI_EXTERN napi_status NAPI_CDECL node_api_run_environment(napi_env env);

NAPI_EXTERN napi_status NAPI_CDECL node_api_await_promise(napi_env env,
                                                          napi_value promise,
                                                          napi_value* result);

NAPI_EXTERN napi_status NAPI_CDECL node_api_destroy_environment(napi_env env,
                                                                int* exit_code);

EXTERN_C_END

#endif  // SRC_NODE_API_EMBEDDING_H_
