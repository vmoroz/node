#include "test_std_string.h"
#include <js_native_api.h>
#include <string>
#include "../common.h"

// These tests show how to use the Node-API with C++ std::string class.

EXTERN_C_START

napi_value TestStdStringLatin1(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env,
                napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  napi_valuetype valuetype;
  NODE_API_CALL(env, napi_typeof(env, args[0], &valuetype));

  NODE_API_ASSERT(env,
                  valuetype == napi_string,
                  "Wrong type of argment. Expects a string.");

  // Measure the string length
  size_t str_length;
  NODE_API_CALL(
      env, napi_get_value_string_latin1(env, args[0], nullptr, 0, &str_length));

  // Create std::string with the required size.
  std::string latin1_str(str_length, '\0');

  // Copy the napi_value string content to std::string
  size_t copied;
  NODE_API_CALL(
      env,
      napi_get_value_string_latin1(
          env, args[0], &latin1_str[0], latin1_str.length() + 1, &copied));

  NODE_API_ASSERT(env, str_length == copied, "Must copy the whole string");

  napi_value output;
  NODE_API_CALL(env,
                napi_create_string_latin1(
                    env, latin1_str.data(), latin1_str.length(), &output));

  return output;
}

napi_value TestStdStringUtf8(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env,
                napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  napi_valuetype valuetype;
  NODE_API_CALL(env, napi_typeof(env, args[0], &valuetype));

  NODE_API_ASSERT(env,
                  valuetype == napi_string,
                  "Wrong type of argment. Expects a string.");

  // Measure the string length
  size_t str_length;
  NODE_API_CALL(
      env, napi_get_value_string_utf8(env, args[0], nullptr, 0, &str_length));

  // Create std::string with the required size.
  std::string utf8_str(str_length, '\0');

  // Copy the napi_value string content to std::string
  size_t copied;
  NODE_API_CALL(
      env,
      napi_get_value_string_utf8(
          env, args[0], &utf8_str[0], utf8_str.length() + 1, &copied));

  NODE_API_ASSERT(env, str_length == copied, "Must copy the whole string");

  napi_value output;
  NODE_API_CALL(env,
                napi_create_string_utf8(
                    env, utf8_str.data(), utf8_str.length(), &output));

  return output;
}

napi_value TestStdStringUtf16(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env,
                napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  napi_valuetype valuetype;
  NODE_API_CALL(env, napi_typeof(env, args[0], &valuetype));

  NODE_API_ASSERT(env,
                  valuetype == napi_string,
                  "Wrong type of argment. Expects a string.");

  // Measure the string length
  size_t str_length;
  NODE_API_CALL(
      env, napi_get_value_string_utf16(env, args[0], nullptr, 0, &str_length));

  // Create std::string with the required size.
  std::u16string utf16_str(str_length, '\0');

  // Copy the napi_value string content to std::string
  size_t copied;
  NODE_API_CALL(
      env,
      napi_get_value_string_utf16(
          env, args[0], &utf16_str[0], utf16_str.length() + 1, &copied));

  NODE_API_ASSERT(env, str_length == copied, "Must copy the whole string");

  napi_value output;
  NODE_API_CALL(env,
                napi_create_string_utf16(
                    env, utf16_str.data(), utf16_str.length(), &output));

  return output;
}

EXTERN_C_END
