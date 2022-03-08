#include "../common.h"
#include "testobject.h"

static int finalize_count = 0;

TestObject::TestObject() : env_(nullptr), wrapper_(nullptr) {}

TestObject::~TestObject() {
  napi_delete_reference(env_, wrapper_);
}

void TestObject::Destructor(napi_env env,
                            void* nativeObject,
                            void* /*finalize_hint*/) {
  ++finalize_count;
  TestObject* obj = static_cast<TestObject*>(nativeObject);
  delete obj;
}

napi_value TestObject::GetFinalizeCount(napi_env env, napi_callback_info info) {
  napi_value result;
  NODE_API_CALL(env, napi_create_int32(env, finalize_count, &result));
  return result;
}

napi_ref TestObject::constructor;

napi_status TestObject::Init(napi_env env) {
  napi_status status;
  napi_value cons;
  status = napi_define_class(
      env, "TestObject", -1, New, nullptr, 0, nullptr, &cons);
  if (status != napi_ok) return status;

  status = napi_create_reference(env, cons, 1, &constructor);
  if (status != napi_ok) return status;

  return napi_ok;
}

napi_value TestObject::New(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_value _this;
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, &_this, nullptr));

  TestObject* obj = new TestObject();

  obj->env_ = env;
  NODE_API_CALL(env,
                napi_wrap(env,
                          _this,
                          obj,
                          TestObject::Destructor,
                          nullptr /* finalize_hint */,
                          &obj->wrapper_));

  return _this;
}

napi_status TestObject::NewInstance(napi_env env, napi_value* instance) {
  napi_status status;

  napi_value cons;
  status = napi_get_reference_value(env, constructor, &cons);
  if (status != napi_ok) return status;

  status = napi_new_instance(env, cons, 0, nullptr, instance);
  if (status != napi_ok) return status;

  return napi_ok;
}
