#define NAPI_EXPERIMENTAL

#include <assert.h>
#include <node_api.h>
#include <node_api_embedding.h>

#include <stdio.h>
#include <string.h>

// Note: This file is being referred to from doc/api/embedding.md, and excerpts
// from it are included in the documentation. Try to keep these in sync.

static int RunNodeInstance(node_api_platform platform);

const char* main_script =
    "globalThis.require = require('module').createRequire(process.execPath);\n"
    "globalThis.embedVars = { nön_ascıı: '🏳️‍🌈' };\n"
    "require('vm').runInThisContext(process.argv[1]);";

#define CHECK(status, msg)                                                     \
  if (status != napi_ok) {                                                     \
    exit_code = 1;                                                             \
    fprintf(stderr, "%s\n", msg);                                              \
    goto fail;                                                                 \
  }

#define CHECK_EXIT_CODE(code)                                                  \
  if (code != 0) {                                                             \
    exit_code = code;                                                          \
    goto fail;                                                                 \
  }

int node_api_test_main(int argc, char** argv) {
  int exit_code = 0;

  node_api_platform platform;
  CHECK(node_api_create_platform(argc, argv, NULL, &platform),
        "Failed creating the platform");

  CHECK_EXIT_CODE(RunNodeInstance(platform));

  CHECK(node_api_destroy_platform(platform), "Failed destroying the platform");

fail:
  return exit_code;
}

int callMe(napi_env env) {
  int exit_code = 0;
  napi_handle_scope scope;
  napi_value global;
  napi_value cb;
  napi_value key;

  napi_open_handle_scope(env, &scope);

  CHECK(napi_get_global(env, &global), "Failed accessing the global object");

  CHECK(napi_create_string_utf8(env, "callMe", strlen("callMe"), &key),
        "create string");

  CHECK(napi_get_property(env, global, key, &cb),
        "Failed accessing the global object");

  napi_valuetype cb_type;
  CHECK(napi_typeof(env, cb, &cb_type), "Failed accessing the global object");

  if (cb_type == napi_function) {
    napi_value undef;
    napi_get_undefined(env, &undef);
    napi_value arg;
    napi_create_string_utf8(env, "called", strlen("called"), &arg);
    napi_value result;
    napi_call_function(env, undef, cb, 1, &arg, &result);

    char buf[32];
    size_t len;
    napi_get_value_string_utf8(env, result, buf, 32, &len);
    if (strncmp(buf, "called you", strlen("called you"))) {
      fprintf(stderr, "Invalid value received: %s\n", buf);
      goto fail;
    }
    printf("%s", buf);
  } else if (cb_type != napi_undefined) {
    fprintf(stderr, "Invalid callMe value\n");
    goto fail;
  }

  napi_value object;
  CHECK(napi_create_object(env, &object), "Failed creating an object\n");

fail:
  napi_close_handle_scope(env, scope);
  return exit_code;
}

char callback_buf[32];
size_t callback_buf_len;
napi_value c_cb(napi_env env, napi_callback_info info) {
  napi_handle_scope scope;
  size_t argc = 1;
  napi_value arg;
  napi_value undef;

  napi_open_handle_scope(env, &scope);
  napi_get_cb_info(env, info, &argc, &arg, NULL, NULL);

  napi_get_value_string_utf8(env, arg, callback_buf, 32, &callback_buf_len);
  napi_get_undefined(env, &undef);
  napi_close_handle_scope(env, scope);
  return undef;
}

int waitMe(napi_env env) {
  int exit_code = 0;
  napi_handle_scope scope;
  napi_value global;
  napi_value cb;
  napi_value key;

  napi_open_handle_scope(env, &scope);

  CHECK(napi_get_global(env, &global), "Failed accessing the global object");

  napi_create_string_utf8(env, "waitMe", strlen("waitMe"), &key);

  CHECK(napi_get_property(env, global, key, &cb),
        "Failed accessing the global object");

  napi_valuetype cb_type;
  CHECK(napi_typeof(env, cb, &cb_type), "Failed accessing the global object");

  if (cb_type == napi_function) {
    napi_value undef;
    napi_get_undefined(env, &undef);
    napi_value args[2];
    napi_create_string_utf8(env, "waited", strlen("waited"), &args[0]);
    CHECK(napi_create_function(
              env, "wait_cb", strlen("wait_cb"), c_cb, NULL, &args[1]),
          "Failed creating function");

    napi_value result;
    memset(callback_buf, 0, 32);
    napi_call_function(env, undef, cb, 2, args, &result);
    if (!strncmp(callback_buf, "waited you", strlen("waited you"))) {
      fprintf(stderr, "Anachronism detected: %s\n", callback_buf);
      goto fail;
    }

    CHECK(node_api_run_environment(env), "Failed spinning the event loop");

    if (strncmp(callback_buf, "waited you", strlen("waited you"))) {
      fprintf(stderr, "Invalid value received: %s\n", callback_buf);
      goto fail;
    }
    printf("%s", callback_buf);
  } else if (cb_type != napi_undefined) {
    fprintf(stderr, "Invalid waitMe value\n");
    goto fail;
  }

fail:
  napi_close_handle_scope(env, scope);
  return exit_code;
}

int waitMeWithCheese(napi_env env) {
  int exit_code = 0;
  napi_handle_scope scope;
  napi_value global;
  napi_value cb;
  napi_value key;

  napi_open_handle_scope(env, &scope);

  CHECK(napi_get_global(env, &global), "Failed accessing the global object");

  napi_create_string_utf8(env, "waitPromise", strlen("waitPromise"), &key);

  CHECK(napi_get_property(env, global, key, &cb),
        "Failed accessing the global object");

  napi_valuetype cb_type;
  CHECK(napi_typeof(env, cb, &cb_type), "Failed accessing the global object");

  if (cb_type == napi_function) {
    napi_value undef;
    napi_get_undefined(env, &undef);
    napi_value arg;
    bool result_type;

    napi_create_string_utf8(env, "waited", strlen("waited"), &arg);

    memset(callback_buf, 0, 32);
    napi_value promise;
    napi_value result;
    CHECK(napi_call_function(env, undef, cb, 1, &arg, &promise),
          "Failed evaluating the function");

    if (!strncmp(
            callback_buf, "waited with cheese", strlen("waited with cheese"))) {
      fprintf(stderr, "Anachronism detected: %s\n", callback_buf);
      goto fail;
    }

    CHECK(napi_is_promise(env, promise, &result_type),
          "Failed evaluating the result");

    if (!result_type) {
      fprintf(stderr, "Result is not a Promise\n");
      goto fail;
    }

    napi_status r = node_api_await_promise(env, promise, &result);
    if (r != napi_ok && r != napi_pending_exception) {
      fprintf(stderr, "Failed awaiting promise: %d\n", r);
      goto fail;
    }

    const char* expected;
    if (r == napi_ok)
      expected = "waited with cheese";
    else
      expected = "waited without cheese";

    napi_get_value_string_utf8(
        env, result, callback_buf, 32, &callback_buf_len);
    if (strncmp(callback_buf, expected, strlen(expected))) {
      fprintf(stderr, "Invalid value received: %s\n", callback_buf);
      goto fail;
    }
    printf("%s", callback_buf);
  } else if (cb_type != napi_undefined) {
    fprintf(stderr, "Invalid waitPromise value\n");
    goto fail;
  }

fail:
  napi_close_handle_scope(env, scope);
  return exit_code;
}

int RunNodeInstance(node_api_platform platform) {
  int exit_code = 0;

  napi_env env;
  CHECK(node_api_create_environment(
            platform, NULL, main_script, NAPI_VERSION, &env),
        "Failed running JS");

  CHECK_EXIT_CODE(callMe(env));
  CHECK_EXIT_CODE(waitMe(env));
  CHECK_EXIT_CODE(waitMeWithCheese(env));

  CHECK(node_api_destroy_environment(env, &exit_code),
        "napi_destroy_environment");

fail:
  return exit_code;
}
