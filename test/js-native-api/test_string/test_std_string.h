#ifndef TEST_JS_NATIVE_API_TEST_STRING_TEST_STD_STRING_H_
#define TEST_JS_NATIVE_API_TEST_STRING_TEST_STD_STRING_H_

#include <js_native_api.h>

EXTERN_C_START

napi_value TestStdStringLatin1(napi_env env, napi_callback_info info);
napi_value TestStdStringUtf8(napi_env env, napi_callback_info info);
napi_value TestStdStringUtf16(napi_env env, napi_callback_info info);

EXTERN_C_END

#endif  // TEST_JS_NATIVE_API_TEST_STRING_TEST_STD_STRING_H_
