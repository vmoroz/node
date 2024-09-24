#include "embedtest_c_api_common.h"

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>

const char* main_script =
    "globalThis.require = require('module').createRequire(process.execPath);\n"
    "globalThis.embedVars = { nön_ascıı: '🏳️‍🌈' };\n"
    "require('vm').runInThisContext(process.argv[1]);";

napi_status AddUtf8String(std::string& str, napi_env env, napi_value value) {
  size_t str_size = 0;
  napi_status status =
      napi_get_value_string_utf8(env, value, nullptr, 0, &str_size);
  if (status != napi_ok) {
    return status;
  }
  size_t offset = str.size();
  str.resize(offset + str_size);
  status = napi_get_value_string_utf8(
      env, value, &str[0] + offset, str_size + 1, &str_size);
  return status;
}

void GetAndThrowLastErrorMessage(napi_env env) {
  const napi_extended_error_info* error_info;
  napi_get_last_error_info(env, &error_info);
  bool is_pending;
  const char* err_message = error_info->error_message;
  napi_is_exception_pending((env), &is_pending);
  /* If an exception is already pending, don't rethrow it */
  if (!is_pending) {
    const char* error_message =
        err_message != nullptr ? err_message : "empty error message";
    napi_throw_error((env), nullptr, error_message);
  }
}

void ThrowLastErrorMessage(napi_env env, const char* message) {
  bool is_pending;
  napi_is_exception_pending(env, &is_pending);
  /* If an exception is already pending, don't rethrow it */
  if (!is_pending) {
    const char* error_message =
        message != nullptr ? message : "empty error message";
    napi_throw_error(env, nullptr, error_message);
  }
}

std::string FormatString(const char* format, ...) {
  va_list args1;
  va_start(args1, format);
  va_list args2;
  va_copy(args2, args1);  // Required for some compilers like GCC.
  std::string result(std::vsnprintf(nullptr, 0, format, args1), '\0');
  va_end(args1);
  std::vsnprintf(&result[0], result.size() + 1, format, args2);
  va_end(args2);
  return result;
}
