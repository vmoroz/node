#include <assert.h>
#define NAPI_EXPERIMENTAL
#include <node_api.h>

#define NAPI_CALL(call)                                                        \
  do {                                                                         \
    napi_status status = (call);                                               \
    assert(status == napi_ok && #call " failed");                              \
  } while (0);

#define NODE_API_DECLARE_METHOD(name, func)                                    \
  { (name), NULL, (func), NULL, NULL, NULL, napi_default, NULL }

static void MyObjectDestructor(napi_env env, void* data, void* finalize_hint) {
  free(data);
}

static napi_value MyObjectConstructor(napi_env env, napi_callback_info info) {
  napi_value instance;
  NODE_API_CALL(env, napi_get_cb_info(env, info, NULL, NULL, &instance, NULL));

  int* data = malloc(sizeof(int));
  NAPI_CALL(napi_wrap(
      env, instance, data, MyObjectDestructor, NULL /* finalize_hint */, NULL));

  return instance;
}

static napi_value Unwrap(napi_env env, napi_callback_info info) {
  size_t argc = 4;
  napi_value argv[4];
  uint32_t n;
  uint32_t index;
  napi_handle_scope scope;
  napi_value constructor;
  napi_value obj;

  NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
  NAPI_CALL(napi_get_value_uint32(env, argv[0], &n));
  NAPI_CALL(napi_open_handle_scope(env, &scope));

  NAPI_CALL(napi_define_class(env,
                              "MyObject",
                              NAPI_AUTO_LENGTH,
                              MyObjectConstructor,
                              NULL,
                              0,
                              NULL,
                              &constructor));

  NAPI_CALL(napi_new_instance(env, constructor, 0, NULL, &obj));

  // Time the object tag creation.
  NAPI_CALL(napi_call_function(env, argv[1], argv[2], 0, NULL, NULL));
  for (index = 0; index < n; index++) {
    int data;
    NAPI_CALL(napi_unwrap(env, obj, &data));
    assert(data == 42);
  }
  NAPI_CALL(napi_call_function(env, argv[1], argv[3], 1, &argv[0], NULL));

  NAPI_CALL(napi_close_handle_scope(env, scope));
  return NULL;
}

NAPI_MODULE_INIT() {
  napi_property_descriptor desc[] = {
      NODE_API_DECLARE_METHOD("unwrap", Unwrap),
  };
  NAPI_CALL(napi_define_properties(env, exports, 1, desc));
  return exports;
}
