#include <assert.h>
#include <stdlib.h>
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
  napi_value this_arg;
  NAPI_CALL(napi_get_cb_info(env, info, NULL, NULL, &this_arg, NULL));

  int* data = malloc(sizeof(int));
  *data = 42;
  NAPI_CALL(napi_wrap(
      env, this_arg, data, MyObjectDestructor, NULL /* finalize_hint */, NULL));

  return this_arg;
}

static napi_value MakeObject(napi_env env, napi_callback_info info) {
  napi_value this_arg;

  NAPI_CALL(napi_get_cb_info(env, info, NULL, NULL, &this_arg, NULL));

  napi_value constructor;
  NAPI_CALL(napi_define_class(env,
                              "MyObject",
                              NAPI_AUTO_LENGTH,
                              MyObjectConstructor,
                              NULL,
                              0,
                              NULL,
                              &constructor));

  napi_value obj;
  NAPI_CALL(napi_new_instance(env, constructor, 0, NULL, &obj));
  return obj;
}

static napi_value Unwrap(napi_env env, napi_callback_info info) {
  napi_value this_arg;

  NAPI_CALL(napi_get_cb_info(env, info, NULL, NULL, &this_arg, NULL));

  int* data;
  NAPI_CALL(napi_unwrap(env, this_arg, (void*)&data));
  assert(*data == 42);

  return NULL;
}

NAPI_MODULE_INIT() {
  napi_property_descriptor desc[] = {
      NODE_API_DECLARE_METHOD("unwrap", Unwrap),
      NODE_API_DECLARE_METHOD("makeObject", MakeObject),
  };
  NAPI_CALL(napi_define_properties(
      env, exports, sizeof(desc) / sizeof(napi_property_descriptor), desc));
  return exports;
}
