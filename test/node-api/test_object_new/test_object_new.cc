#define NAPI_EXPERIMENTAL
#include <node_api.h>
#include <v8.h>
#include "../../js-native-api/common.h"

static napi_value test(napi_env env, napi_callback_info info) {
  napi_value arr, foo, bar, hi, num, obj;
  NODE_API_CALL(env, napi_create_array(env, &arr));

  NODE_API_CALL(env,
                napi_create_string_utf8(env, "foo", NAPI_AUTO_LENGTH, &foo));
  NODE_API_CALL(env,
                napi_create_string_utf8(env, "bar", NAPI_AUTO_LENGTH, &bar));
  NODE_API_CALL(env, napi_create_string_utf8(env, "hi", NAPI_AUTO_LENGTH, &hi));
  for (int i = 0; i < 100000; i++) {
    NODE_API_CALL(env, napi_create_object(env, &obj));
    NODE_API_CALL(env, napi_create_int32(env, i, &num));
    NODE_API_CALL(env, napi_set_property(env, obj, foo, num));
    NODE_API_CALL(env, napi_set_property(env, obj, bar, hi));
    NODE_API_CALL(env, napi_set_element(env, arr, i, obj));
  }

  return nullptr;
}

static napi_value ctor(napi_env env, napi_callback_info info) {
  return nullptr;
}
static napi_value getter(napi_env env, napi_callback_info info) {
  return nullptr;
}
static napi_value setter(napi_env env, napi_callback_info info) {
  return nullptr;
}

static napi_value napi_class(napi_env env, napi_callback_info info) {
  napi_value arr, foo, bar, hi, num, obj, klass;
  NODE_API_CALL(env, napi_create_array(env, &arr));

  NODE_API_CALL(env,
                napi_create_string_utf8(env, "foo", NAPI_AUTO_LENGTH, &foo));
  NODE_API_CALL(env,
                napi_create_string_utf8(env, "bar", NAPI_AUTO_LENGTH, &bar));
  NODE_API_CALL(env, napi_create_string_utf8(env, "hi", NAPI_AUTO_LENGTH, &hi));

  napi_property_descriptor props[] = {{nullptr,
                                       foo,
                                       nullptr,
                                       getter,
                                       setter,
                                       nullptr,
                                       napi_default_jsproperty,
                                       (void*)0},
                                      {nullptr,
                                       bar,
                                       nullptr,
                                       getter,
                                       setter,
                                       nullptr,
                                       napi_default_jsproperty,
                                       (void*)1}};

  NODE_API_CALL(env,
                napi_define_class(
                    env, "AClass", NAPI_AUTO_LENGTH, ctor, nullptr, 2, props, &klass));

  for (int i = 0; i < 100000; i++) {
    napi_value obj;
    NODE_API_CALL(env, napi_new_instance(env, klass, 0,  nullptr, &obj));
    //NODE_API_CALL(env, napi_create_int32(env, i, &num));
    //NODE_API_CALL(env, napi_set_property(env, obj, foo, num));
    //NODE_API_CALL(env, napi_set_property(env, obj, bar, hi));
    NODE_API_CALL(env, napi_set_element(env, arr, i, obj));
  }

  return nullptr;
}

static napi_value tmpl(napi_env env, napi_callback_info info) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Array> arr = v8::Array::New(isolate, 0);

  v8::Local<v8::ObjectTemplate> tpl = v8::ObjectTemplate::New(isolate);
  tpl->Set(isolate, "foo", v8::Null(isolate));
  tpl->Set(isolate, "bar", v8::Null(isolate));

  auto foo = v8::String::NewFromUtf8(isolate, "foo").ToLocalChecked();
  auto bar = v8::String::NewFromUtf8(isolate, "bar").ToLocalChecked();
  auto hi = v8::String::NewFromUtf8(isolate, "hi").ToLocalChecked();

  for (int i = 0; i < 100000; i++) {
    auto obj = tpl->NewInstance(context).ToLocalChecked();
    obj->Set(context, foo, v8::Number::New(isolate, i));
    obj->Set(context, bar, hi);
    arr->Set(context, i, obj);
  }

  return nullptr;
}

static napi_value tmpl_intern(napi_env env, napi_callback_info info) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Array> arr = v8::Array::New(isolate, 0);

  v8::Local<v8::ObjectTemplate> tpl = v8::ObjectTemplate::New(isolate);
  tpl->Set(isolate, "foo", v8::Null(isolate));
  tpl->Set(isolate, "bar", v8::Null(isolate));

  auto foo =
      v8::String::NewFromUtf8(isolate, "foo", v8::NewStringType::kInternalized)
          .ToLocalChecked();
  auto bar =
      v8::String::NewFromUtf8(isolate, "bar", v8::NewStringType::kInternalized)
          .ToLocalChecked();
  auto hi =
      v8::String::NewFromUtf8(isolate, "hi", v8::NewStringType::kInternalized)
          .ToLocalChecked();

  for (int i = 0; i < 100000; i++) {
    auto obj = tpl->NewInstance(context).ToLocalChecked();
    obj->Set(context, foo, v8::Number::New(isolate, i));
    obj->Set(context, bar, hi);
    arr->Set(context, i, obj);
  }

  return nullptr;
}

static napi_value obj_data_prop(napi_env env, napi_callback_info info) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Array> arr = v8::Array::New(isolate, 0);

  auto foo =
      v8::String::NewFromUtf8(isolate, "foo", v8::NewStringType::kInternalized)
          .ToLocalChecked();
  auto bar =
      v8::String::NewFromUtf8(isolate, "bar", v8::NewStringType::kInternalized)
          .ToLocalChecked();
  auto hi =
      v8::String::NewFromUtf8(isolate, "hi", v8::NewStringType::kInternalized)
          .ToLocalChecked();

  for (int i = 0; i < 100000; i++) {
    auto obj = v8::Object::New(isolate);
    obj->CreateDataProperty(context, foo, v8::Number::New(isolate, i));
    obj->CreateDataProperty(context, bar, hi);
    arr->Set(context, i, obj);
  }

  return nullptr;
}

static napi_value obj_new_as_literal(napi_env env, napi_callback_info info) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Array> arr = v8::Array::New(isolate, 0);
  v8::Local<v8::Object> proto = v8::Object::New(isolate);

  auto foo =
      v8::String::NewFromUtf8(isolate, "foo", v8::NewStringType::kInternalized)
          .ToLocalChecked();
  auto bar =
      v8::String::NewFromUtf8(isolate, "bar", v8::NewStringType::kInternalized)
          .ToLocalChecked();
  auto hi =
      v8::String::NewFromUtf8(isolate, "hi", v8::NewStringType::kInternalized)
          .ToLocalChecked();

  v8::Local<v8::Name> names[2] = {foo, bar};
  v8::Local<v8::Value> values[2] = {};

  for (int i = 0; i < 100000; i++) {
    values[0] = v8::Number::New(isolate, i);
    values[1] = hi;
    auto obj = v8::Object::NewAsLiteral(isolate, proto, names, values, 2);
    arr->Set(context, i, obj);
  }

  return nullptr;
}

static napi_value obj_new_as_json(napi_env env, napi_callback_info info) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Array> arr = v8::Array::New(isolate, 0);
  v8::Local<v8::Object> proto = v8::Object::New(isolate);

  auto foo =
      v8::String::NewFromUtf8(isolate, "foo", v8::NewStringType::kInternalized)
          .ToLocalChecked();
  auto bar =
      v8::String::NewFromUtf8(isolate, "bar", v8::NewStringType::kInternalized)
          .ToLocalChecked();
  auto hi =
      v8::String::NewFromUtf8(isolate, "hi", v8::NewStringType::kInternalized)
          .ToLocalChecked();

  v8::Local<v8::Name> names[2] = {foo, bar};
  v8::Local<v8::Value> values[2] = {};

  for (int i = 0; i < 100000; i++) {
    values[0] = v8::Number::New(isolate, i);
    values[1] = hi;
    auto obj = v8::Object::NewAsJson(isolate, proto, names, values, 2);
    arr->Set(context, i, obj);
  }

  return nullptr;
}

static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
      DECLARE_NODE_API_PROPERTY("test", test),
      DECLARE_NODE_API_PROPERTY("tmpl", tmpl),
      DECLARE_NODE_API_PROPERTY("tmpl_intern", tmpl_intern),
      DECLARE_NODE_API_PROPERTY("obj_data_prop", obj_data_prop),
      DECLARE_NODE_API_PROPERTY("obj_new_as_literal", obj_new_as_literal),
      DECLARE_NODE_API_PROPERTY("obj_new_as_json", obj_new_as_json),
      DECLARE_NODE_API_PROPERTY("napi_class", napi_class),
  };

  NODE_API_CALL(
      env,
      napi_define_properties(env,
                             exports,
                             sizeof(descriptors) / sizeof(*descriptors),
                             descriptors));

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
