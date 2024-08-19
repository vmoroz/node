#include "node_api_embedtest.h"

#define NAPI_EXPERIMENTAL
#include <node_api.h>
#include <node_api_embedding.h>

#include <stdio.h>
#include <string.h>

int node_api_modules_test_main(int argc, char** argv) {
  node_api_platform platform;

  if (argc < 3) {
    fprintf(stderr, "node_api_modules <cjs.cjs> <es6.mjs>\n");
    return 2;
  }

  CHECK(node_api_create_platform(0, NULL, NULL, &platform));

  napi_env env;
  CHECK(node_api_create_environment(platform, NULL, NULL, NAPI_VERSION, &env));

  napi_handle_scope scope;
  CHECK(napi_open_handle_scope(env, &scope));

  napi_value global, import_name, require_name, import, require, cjs, es6,
      value;
  CHECK(napi_get_global(env, &global));
  CHECK(napi_create_string_utf8(env, "import", strlen("import"), &import_name));
  CHECK(napi_create_string_utf8(
      env, "require", strlen("require"), &require_name));
  CHECK(napi_get_property(env, global, import_name, &import));
  CHECK(napi_get_property(env, global, require_name, &require));

  CHECK(napi_create_string_utf8(env, argv[1], strlen(argv[1]), &cjs));
  CHECK(napi_create_string_utf8(env, argv[2], strlen(argv[2]), &es6));
  CHECK(napi_create_string_utf8(env, "value", strlen("value"), &value));

  napi_value es6_module, es6_promise, cjs_module, es6_result, cjs_result;
  char buffer[32];
  size_t bufferlen;

  CHECK(napi_call_function(env, global, import, 1, &es6, &es6_promise));
  CHECK(node_api_await_promise(env, es6_promise, &es6_module));

  CHECK(napi_get_property(env, es6_module, value, &es6_result));
  CHECK(napi_get_value_string_utf8(
      env, es6_result, buffer, sizeof(buffer), &bufferlen));
  if (strncmp(buffer, "genuine", bufferlen)) {
    fprintf(stderr, "Unexpected value: %s\n", buffer);
    return -1;
  }

  CHECK(napi_call_function(env, global, require, 1, &cjs, &cjs_module));
  CHECK(napi_get_property(env, cjs_module, value, &cjs_result));
  CHECK(napi_get_value_string_utf8(
      env, cjs_result, buffer, sizeof(buffer), &bufferlen));
  if (strncmp(buffer, "original", bufferlen)) {
    fprintf(stderr, "Unexpected value: %s\n", buffer);
    return -1;
  }

  CHECK(napi_close_handle_scope(env, scope));
  CHECK(node_api_destroy_environment(env, NULL));
  CHECK(node_api_destroy_platform(platform));
  return 0;
}
