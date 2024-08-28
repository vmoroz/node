#ifndef SRC_NODE_API_EMBEDDING_H_
#define SRC_NODE_API_EMBEDDING_H_

#include "node_api.h"

EXTERN_C_START

typedef struct node_api_platform__* node_api_platform;

typedef void(NAPI_CDECL* node_api_get_strings_callback)(
    void* data, size_t str_count, const char* str_array[]);

typedef bool(NAPI_CDECL* node_api_run_predicate)(void* predicate_data);

// TODO(vmoroz): Add passing flags for InitializeOncePerProcess
NAPI_EXTERN napi_status NAPI_CDECL
node_api_create_platform(int argc,
                         char** argv,
                         int32_t* exit_code,
                         node_api_get_strings_callback get_errors_cb,
                         void* errors_data,
                         node_api_platform* result);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_destroy_platform(node_api_platform platform);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_get_platform_args(node_api_platform platform,
                           node_api_get_strings_callback get_strings_cb,
                           void* strings_data);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_get_platform_exec_args(node_api_platform platform,
                                node_api_get_strings_callback get_strings_cb,
                                void* strings_data);

// TODO(vmoroz): Consider creating opaque environment options type.
// TODO(vmoroz): Remove the main_script parameter.
// TODO(vmoroz): Add ABI-safe way to access internal module functionality.
// TODO(vmoroz): Add ability to create snapshots and to load them.
// TODO(vmoroz): Pass the parsed arguments.
// TODO(vmoroz): Pass EnvironmentFlags
// TODO(vmoroz): Allow setting the global inspector for a specific environment.
NAPI_EXTERN napi_status NAPI_CDECL
node_api_create_environment(node_api_platform platform,
                            node_api_get_strings_callback get_errors_cb,
                            void* errors_data,
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
node_api_run_environment_while(napi_env env,
                               node_api_run_predicate predicate,
                               void* predicate_data,
                               bool* has_more_work);

NAPI_EXTERN napi_status NAPI_CDECL node_api_await_promise(napi_env env,
                                                          napi_value promise,
                                                          napi_value* result);

EXTERN_C_END

#endif  // SRC_NODE_API_EMBEDDING_H_
