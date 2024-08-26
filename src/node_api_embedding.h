#ifndef SRC_NODE_API_EMBEDDING_H_
#define SRC_NODE_API_EMBEDDING_H_

#include "node_api.h"

EXTERN_C_START

typedef struct node_api_platform__* node_api_platform;

typedef void(NAPI_CDECL* node_api_error_handler)(void* data,
                                                 bool early_return,
                                                 int32_t exit_code,
                                                 size_t msg_count,
                                                 const char** msg_list);
typedef void(NAPI_CDECL* node_api_error_message_handler)(const char* msg);
typedef bool(NAPI_CDECL* node_api_run_predicate)(void* predicate_data);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_create_platform(int argc,
                         char** argv,
                         node_api_error_handler err_handler,
                         void* err_handler_data,
                         node_api_platform* result);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_destroy_platform(node_api_platform platform);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_create_environment(node_api_platform platform,
                            node_api_error_message_handler err_handler,
                            const char* main_script,
                            int32_t api_version,
                            napi_env* result);

NAPI_EXTERN napi_status NAPI_CDECL node_api_destroy_environment(napi_env env,
                                                                int* exit_code);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_open_environment_scope(napi_env env);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_close_environment_scope(napi_env env);

NAPI_EXTERN napi_status NAPI_CDECL node_api_run_environment(napi_env env);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_run_environment_if(napi_env env,
                            node_api_run_predicate predicate,
                            void* predicate_data,
                            bool* has_more_work);

NAPI_EXTERN napi_status NAPI_CDECL node_api_await_promise(napi_env env,
                                                          napi_value promise,
                                                          napi_value* result);

EXTERN_C_END

#endif  // SRC_NODE_API_EMBEDDING_H_
