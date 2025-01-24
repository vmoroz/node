#include "embedtest_c_api_common.h"

// #include <cassert>
#include <stdarg.h>
// #include <cstdio>
// #include <cstring>

const char* main_script =
    "globalThis.require = require('module').createRequire(process.execPath);\n"
    "globalThis.embedVars = { nön_ascıı: '🏳️‍🌈' };\n"
    "require('vm').runInThisContext(process.argv[1]);";

// napi_status AddUtf8String(std::string& str, napi_env env, napi_value value) {
//   size_t str_size = 0;
//   napi_status status =
//       napi_get_value_string_utf8(env, value, nullptr, 0, &str_size);
//   if (status != napi_ok) {
//     return status;
//   }
//   size_t offset = str.size();
//   str.resize(offset + str_size);
//   status = napi_get_value_string_utf8(
//       env, value, &str[0] + offset, str_size + 1, &str_size);
//   return status;
// }

void GetAndThrowLastErrorMessage(napi_env env) {
  const napi_extended_error_info* error_info;
  napi_get_last_error_info(env, &error_info);
  ThrowLastErrorMessage(env, error_info->error_message);
}

void ThrowLastErrorMessage(napi_env env, const char* format, ...) {
  bool is_pending;
  napi_is_exception_pending(env, &is_pending);
  /* If an exception is already pending, don't rethrow it */
  if (is_pending) {
    return;
  }
  char error_message_buf[1024];
  char* error_message = error_message_buf;
  const char* error_format = format != NULL ? format : "empty error message";

  va_list args1;
  va_start(args1, format);
  va_list args2;  // Required for some compilers like GCC since we go over the
                  // args twice.
  va_copy(args2, args1);
  int32_t error_message_size = vsnprintf(NULL, 0, error_format, args1);
  if (error_message_size > 1024 - 1) {
    error_message = (char*)malloc(error_message_size + 1);
  }
  va_end(args1);
  vsnprintf(error_message, error_message_size + 1, error_format, args2);
  va_end(args2);

  napi_throw_error(env, NULL, error_message);

  if (error_message_size > 1024 - 1) {
    free(error_message);
  }
}

// void ThrowLastErrorMessage(napi_env env, const char* message) {
//   bool is_pending;
//   napi_is_exception_pending(env, &is_pending);
//   /* If an exception is already pending, don't rethrow it */
//   if (!is_pending) {
//     const char* error_message =
//         message != nullptr ? message : "empty error message";
//     napi_throw_error(env, nullptr, error_message);
//   }
// }

napi_value OnStartExecution(void* cb_data,
                            node_embedding_runtime runtime,
                            napi_env env,
                            napi_value process,
                            napi_value require,
                            napi_value run_cjs) {
  napi_value script_value, null_value, result;
  const char* script = (const char*)cb_data;
  NODE_API_CALL_RETURN(
      napi_create_string_utf8(env, script, NAPI_AUTO_LENGTH, &script_value));
  NODE_API_CALL_RETURN(napi_get_null(env, &null_value));
  NODE_API_CALL_RETURN(
      napi_call_function(env, null_value, run_cjs, 1, &script_value, &result));
  return result;
}

node_embedding_status LoadUtf8Script(
    node_embedding_runtime_config runtime_config, const char* script) {
  NODE_EMBEDDED_CALL(node_embedding_runtime_config_on_load(
      runtime_config, OnStartExecution, (void*)script, NULL));
  return node_embedding_status_ok;
}

// NodeExpected<void> LoadUtf8Script(
//     const NodeRuntimeConfig& runtime_config,
//     std::string_view script,
//     NodeHandleExecutionResultCallback handle_result) {
//   NODE_EMBEDDED_CALL(runtime_config.OnStartExecution(
//       [script = std::string(script)](const NodeRuntime& /*runtime*/,
//                                      napi_env env,
//                                      napi_value /*process*/,
//                                      napi_value /*require*/,
//                                      napi_value run_cjs) -> napi_value {
//         napi_value script_value, null_value, result;
//         NODE_API_CALL_RETURN(napi_create_string_utf8(
//             env, script.c_str(), script.size(), &script_value));
//         NODE_API_CALL_RETURN(napi_get_null(env, &null_value));
//         NODE_API_CALL_RETURN(napi_call_function(
//             env, null_value, run_cjs, 1, &script_value, &result));
//         return result;
//       }));
//   NODE_EMBEDDED_CALL(
//       runtime_config.OnHandleExecutionResult(std::move(handle_result)));
//   return NodeExpected<void>();
// }

int32_t StatusToExitCode(node_embedding_status status) {
  if (status == node_embedding_status_ok) {
    return 0;
  } else if ((status & node_embedding_status_error_exit_code) != 0) {
    return status & ~node_embedding_status_error_exit_code;
  }
  return 1;
}

node_embedding_status PrintErrorMessage(const char* exe_name,
                                        node_embedding_status status) {
  if (status == node_embedding_status_ok) {
    return status;
  }
  // TODO:
  // auto expected_message = NodeErrorInfo::GetAndClearLastErrorMessage();
  // if (expected_message.has_error()) {
  //   return NodeExpected<void>(expected_message.status());
  // }
  // std::vector<std::string> messages = std::move(expected_message).value();
  // for (const std::string& message : messages) {
  //   fprintf(stderr, "%s: %s\n", exe_name.data(), message.c_str());
  // }
  // return NodeExpected<void>(expected.status());
  return status;
}
