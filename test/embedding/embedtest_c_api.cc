#include "embedtest_c_api_common.h"

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>

using namespace node;
using namespace node::embedding;

napi_status CallMe(const NodeRuntime& runtime, napi_env env);
napi_status WaitMe(const NodeRuntime& runtime, napi_env env);
napi_status WaitMeWithCheese(const NodeRuntime& runtime, napi_env env);

extern "C" int32_t test_main_node_api(int32_t argc, char* argv[]) {
  return PrintErrorMessage(
             argv[0],
             NodePlatform::RunMain(
                 NodeArgs(argc, argv),
                 [](const NodePlatformConfig& platform_config) {
                   return platform_config.SetFlags(
                       NodePlatformFlags::kDisableNodeOptionsEnv);
                 },
                 [](const NodePlatform& platform,
                    const NodeRuntimeConfig& runtime_config) {
                   return LoadUtf8Script(
                       runtime_config,
                       main_script,
                       [](const NodeRuntime& runtime, napi_env env, napi_value
                          /*value*/) {
                         NODE_API_CALL_RETURN_VOID(CallMe(runtime, env));
                         // NODE_API_CALL_RETURN_VOID(WaitMe(runtime, env));
                         // NODE_API_CALL_RETURN_VOID(
                         //     WaitMeWithCheese(runtime, env));
                       });
                 }))
      .exit_code();
}

napi_status CallMe(const NodeRuntime& runtime, napi_env env) {
  napi_value global;
  napi_value cb;
  napi_value key;

  NODE_API_CALL(napi_get_global(env, &global));
  NODE_API_CALL(napi_create_string_utf8(env, "callMe", NAPI_AUTO_LENGTH, &key));
  NODE_API_CALL(napi_get_property(env, global, key, &cb));

  napi_valuetype cb_type;
  NODE_API_CALL(napi_typeof(env, cb, &cb_type));

  // Only evaluate callMe if it was registered as a function.
  if (cb_type == napi_function) {
    napi_value undef;
    NODE_API_CALL(napi_get_undefined(env, &undef));
    napi_value arg;
    NODE_API_CALL(
        napi_create_string_utf8(env, "called", NAPI_AUTO_LENGTH, &arg));
    napi_value result;
    NODE_API_CALL(napi_call_function(env, undef, cb, 1, &arg, &result));

    char buf[32];
    size_t len;
    NODE_API_CALL(napi_get_value_string_utf8(env, result, buf, 32, &len));
    if (strcmp(buf, "called you") != 0) {
      NODE_API_FAIL("Invalid value received: %s\n", buf);
    }
    printf("%s", buf);
  } else if (cb_type != napi_undefined) {
    NODE_API_FAIL("Invalid callMe value\n");
  }
  return napi_ok;
}
#if 0
char callback_buf[32];
size_t callback_buf_len;
napi_value c_cb(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value arg;
  NODE_API_CALL(napi_get_cb_info(env, info, &argc, &arg, nullptr, nullptr));
  NODE_API_CALL(napi_get_value_string_utf8(
      env, arg, callback_buf, 32, &callback_buf_len));
  return nullptr;
}

napi_status WaitMe(const NodeRuntime& runtime, napi_env env) {
  napi_value global;
  napi_value cb;
  napi_value key;

  NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
  NODE_API_CALL_RETURN_VOID(
      napi_create_string_utf8(env, "waitMe", NAPI_AUTO_LENGTH, &key));
  NODE_API_CALL_RETURN_VOID(napi_get_property(env, global, key, &cb));

  napi_valuetype cb_type;
  NODE_API_CALL_RETURN_VOID(napi_typeof(env, cb, &cb_type));

  // Only evaluate waitMe if it was registered as a function.
  if (cb_type == napi_function) {
    napi_value undef;
    NODE_API_CALL_RETURN_VOID(napi_get_undefined(env, &undef));
    napi_value args[2];
    NODE_API_CALL_RETURN_VOID(
        napi_create_string_utf8(env, "waited", NAPI_AUTO_LENGTH, &args[0]));
    NODE_API_CALL_RETURN_VOID(napi_create_function(
        env, "wait_cb", strlen("wait_cb"), c_cb, NULL, &args[1]));

    napi_value result;
    memset(callback_buf, 0, 32);
    NODE_API_CALL_RETURN_VOID(
        napi_call_function(env, undef, cb, 2, args, &result));
    if (strcmp(callback_buf, "waited you") == 0) {
      NODE_API_FAIL_RETURN_VOID("Anachronism detected: %s\n", callback_buf);
    }

    // TODO: Implement
    // node_embedding_run_event_loop(
    //    runtime, node_embedding_event_loop_run_mode_default, nullptr);

    if (strcmp(callback_buf, "waited you") != 0) {
      NODE_API_FAIL_RETURN_VOID("Invalid value received: %s\n", callback_buf);
    }
    printf("%s", callback_buf);
  } else if (cb_type != napi_undefined) {
    NODE_API_FAIL_RETURN_VOID("Invalid waitMe value\n");
  }
}

napi_status WaitMeWithCheese(const NodeRuntime& runtime, napi_env env) {
  enum class PromiseState {
    kPending,
    kFulfilled,
    kRejected,
  };

  PromiseState promise_state = PromiseState::kPending;
  napi_value global{}, wait_promise{}, undefined{};
  napi_value on_fulfilled{}, on_rejected{};
  napi_value then_args[2] = {};
  const char* expected{};

  NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
  NODE_API_CALL_RETURN_VOID(napi_get_undefined(env, &undefined));

  NODE_API_CALL_RETURN_VOID(
      napi_get_named_property(env, global, "waitPromise", &wait_promise));

  napi_valuetype wait_promise_type;
  NODE_API_CALL_RETURN_VOID(napi_typeof(env, wait_promise, &wait_promise_type));

  // Only evaluate waitPromise if it was registered as a function.
  if (wait_promise_type == napi_undefined) {
    return NodeExpected<void>();
  } else if (wait_promise_type != napi_function) {
    NODE_API_FAIL_RETURN_VOID("Invalid waitPromise value\n");
  }

  napi_value arg;
  NODE_API_CALL_RETURN_VOID(
      napi_create_string_utf8(env, "waited", NAPI_AUTO_LENGTH, &arg));

  memset(callback_buf, 0, 32);
  napi_value promise;
  NODE_API_CALL_RETURN_VOID(
      napi_call_function(env, undefined, wait_promise, 1, &arg, &promise));

  if (strcmp(callback_buf, "waited with cheese") == 0) {
    NODE_API_FAIL_RETURN_VOID("Anachronism detected: %s\n", callback_buf);
  }

  bool is_promise;
  NODE_API_CALL_RETURN_VOID(napi_is_promise(env, promise, &is_promise));
  if (!is_promise) {
    NODE_API_FAIL_RETURN_VOID("Result is not a Promise\n");
  }

  NODE_API_CALL_RETURN_VOID(napi_create_function(
      env,
      "onFulfilled",
      NAPI_AUTO_LENGTH,
      [](napi_env env, napi_callback_info info) -> napi_value {
        size_t argc = 1;
        napi_value result;
        void* data;
        napi_get_cb_info(env, info, &argc, &result, nullptr, &data);
        napi_get_value_string_utf8(
            env, result, callback_buf, 32, &callback_buf_len);
        *static_cast<PromiseState*>(data) = PromiseState::kFulfilled;
        return nullptr;
      },
      &promise_state,
      &on_fulfilled));
  NODE_API_CALL_RETURN_VOID(napi_create_function(
      env,
      "rejected",
      NAPI_AUTO_LENGTH,
      [](napi_env env, napi_callback_info info) -> napi_value {
        size_t argc = 1;
        napi_value result;
        void* data;
        napi_get_cb_info(env, info, &argc, &result, nullptr, &data);
        napi_get_value_string_utf8(
            env, result, callback_buf, 32, &callback_buf_len);
        *static_cast<PromiseState*>(data) = PromiseState::kRejected;
        return nullptr;
      },
      &promise_state,
      &on_rejected));
  napi_value then;
  NODE_API_CALL_RETURN_VOID(
      napi_get_named_property(env, promise, "then", &then));
  then_args[0] = on_fulfilled;
  then_args[1] = on_rejected;
  NODE_API_CALL_RETURN_VOID(
      napi_call_function(env, promise, then, 2, then_args, nullptr));

  // TODO: Implement
  // while (promise_state == PromiseState::kPending) {
  //  node_embedding_run_event_loop(
  //      runtime, node_embedding_event_loop_run_mode_nowait, nullptr);
  //}

  expected = (promise_state == PromiseState::kFulfilled)
                 ? "waited with cheese"
                 : "waited without cheese";

  if (strcmp(callback_buf, expected) != 0) {
    NODE_API_FAIL_RETURN_VOID("Invalid value received: %s\n", callback_buf);
  }
  printf("%s", callback_buf);
}
#endif
