#include "embedtest_c_api_common.h"

// #include <cassert>
#include <stdarg.h>
// #include <cstdio>
// #include <cstring>

const char* main_script =
    "globalThis.require = require('module').createRequire(process.execPath);\n"
    "globalThis.embedVars = { nön_ascıı: '🏳️‍🌈' };\n"
    "require('vm').runInThisContext(process.argv[1]);";

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
  NODE_EMBEDDED_CALL(node_embedding_runtime_config_on_loading(
      runtime_config, OnStartExecution, (void*)script, NULL));
  return node_embedding_status_ok;
}

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
  const char* error_message = node_embedding_last_error_message_get();
  fprintf(stderr, "%s: %s\n", exe_name, error_message);
  return status;
}

void dynamic_string_init(dynamic_string_t* str) {
  if (str == NULL) return;
  str->data = str->buffer;
  str->length = 0;
  str->buffer[0] = '\0';
}

void dynamic_string_destroy(dynamic_string_t* str) {
  if (str == NULL) return;
  if (str->data != str->buffer) {
    free(str->data);
  }
  dynamic_string_init(str);
}

void dynamic_string_set(dynamic_string_t* str, const char* value) {
  if (str == NULL) return;
  dynamic_string_destroy(str);
  dynamic_string_append(str, value);
}

void dynamic_string_append(dynamic_string_t* str, const char* value) {
  if (str == NULL) return;
  if (value == NULL) return;
  size_t new_length = str->length + strlen(value);
  char* new_data = (new_length + 1 > DYNAMIC_STRING_BUFFER_SIZE)
                       ? new_data = (char*)malloc(new_length + 1)
                       : str->buffer;
  if (new_data == NULL) return;
  strcpy(new_data, str->data);
  strcpy(new_data + str->length, value);
  if (str->data != str->buffer) {
    free(str->data);
  }
  str->data = new_data;
  str->length = new_length;
}
