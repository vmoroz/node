#define NAPI_EXPERIMENTAL
#include <node_api.h>
#include <v8.h>
#include "../../js-native-api/common.h"

template <typename T>
napi_value ToNapiValue(v8::Local<T> local) {
  return reinterpret_cast<napi_value>(*local);
}

template <typename T>
v8::Local<T> ToLocal(napi_value value) {
  return *reinterpret_cast<v8::Local<T>*>(&value);
}

struct ModuleData {
  static void Finalize(napi_env /*env*/, void* data, void* /*hint*/) {
    delete reinterpret_cast<ModuleData*>(data);
  }

  static ModuleData* FromEnv(napi_env env) {
    ModuleData* data{};
    napi_get_instance_data(env, reinterpret_cast<void**>(&data));
    return data;
  }

  napi_value GetFooBar(napi_env env, napi_value* foo, napi_value* bar) {
    if (fooRef == nullptr) {
      v8::Isolate* isolate = v8::Isolate::GetCurrent();
      v8::Local<v8::Name> fooName =
          v8::String::NewFromUtf8(
              isolate, "foo", v8::NewStringType::kInternalized)
              .ToLocalChecked();
      v8::Local<v8::Name> barName =
          v8::String::NewFromUtf8(
              isolate, "bar", v8::NewStringType::kInternalized)
              .ToLocalChecked();
      *foo = ToNapiValue(fooName);
      *bar = ToNapiValue(barName);

      NODE_API_CALL(env, napi_create_reference(env, *foo, 1, &fooRef));
      NODE_API_CALL(env, napi_create_reference(env, *bar, 1, &barRef));
    } else {
      NODE_API_CALL(env, napi_get_reference_value(env, fooRef, foo));
      NODE_API_CALL(env, napi_get_reference_value(env, barRef, bar));
    }
    return nullptr;
  }

  napi_value GetTemplate(napi_env env) {
    napi_value result;
    if (tmplRef == nullptr) {
      v8::Isolate* isolate = v8::Isolate::GetCurrent();
      v8::Local<v8::ObjectTemplate> tmpl = v8::ObjectTemplate::New(isolate);
      tmpl->Set(isolate, "foo", v8::Null(isolate));
      tmpl->Set(isolate, "bar", v8::Null(isolate));
      result = ToNapiValue(tmpl);
      NODE_API_CALL(env, napi_create_reference(env, result, 1, &tmplRef));
    } else {
      NODE_API_CALL(env, napi_get_reference_value(env, tmplRef, &result));
    }
    return result;
  }

 private:
  napi_ref fooRef{};
  napi_ref barRef{};
  napi_ref tmplRef{};
};

// A method to put a break point for perf measure
static napi_value js_perf_start(napi_env /*env*/, napi_callback_info /*info*/) {
  return nullptr;
}

// A method to put a break point for perf measure
static napi_value js_perf_end(napi_env /*env*/, napi_callback_info /*info*/) {
  return nullptr;
}

// Creating object using existing Node-API calls
static napi_value obj_napi(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value args[3];
  NODE_API_CALL(env,
                napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  napi_value obj;
  NODE_API_CALL(env, napi_create_object(env, &obj));
  NODE_API_CALL(env, napi_set_named_property(env, obj, "foo", args[1]));
  NODE_API_CALL(env, napi_set_named_property(env, obj, "bar", args[2]));
  return obj;
}

static napi_value obj_tmpl(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value args[3];
  NODE_API_CALL(env,
                napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  ModuleData* data = ModuleData::FromEnv(env);
  v8::Local<v8::ObjectTemplate> tmpl =
      ToLocal<v8::ObjectTemplate>(data->GetTemplate(env));

  napi_value foo, bar;
  data->GetFooBar(env, &foo, &bar);

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  auto obj = tmpl->NewInstance(context).ToLocalChecked();
  obj->Set(context, ToLocal<v8::Name>(foo), ToLocal<v8::Value>(args[1]));
  obj->Set(context, ToLocal<v8::Name>(bar), ToLocal<v8::Value>(args[2]));

  return ToNapiValue(obj);
}

static napi_value obj_new_data_prop(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value args[3];
  NODE_API_CALL(env,
                napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  ModuleData* data = ModuleData::FromEnv(env);
  v8::Local<v8::ObjectTemplate> tmpl =
      ToLocal<v8::ObjectTemplate>(data->GetTemplate(env));

  napi_value foo, bar;
  data->GetFooBar(env, &foo, &bar);

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  auto obj = v8::Object::New(isolate);
  obj->CreateDataProperty(
      context, ToLocal<v8::Name>(foo), ToLocal<v8::Value>(args[1]));
  obj->CreateDataProperty(
      context, ToLocal<v8::Name>(bar), ToLocal<v8::Value>(args[2]));

  return ToNapiValue(obj);
}

static napi_value obj_new(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value args[3];
  NODE_API_CALL(env,
                napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  ModuleData* data = ModuleData::FromEnv(env);
  v8::Local<v8::ObjectTemplate> tmpl =
      ToLocal<v8::ObjectTemplate>(data->GetTemplate(env));

  napi_value foo, bar;
  data->GetFooBar(env, &foo, &bar);

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Name> names[2] = {ToLocal<v8::Name>(foo),
                                  ToLocal<v8::Name>(bar)};
  v8::Local<v8::Value>* values =
      reinterpret_cast<v8::Local<v8::Value>*>(args + 1);

  v8::Local<v8::Object> obj =
      v8::Object::New(isolate, ToLocal<v8::Value>(args[0]), names, values, 2);

  return ToNapiValue(obj);
}

static napi_value obj_new_as_literal(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value args[3];
  NODE_API_CALL(env,
                napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  ModuleData* data = ModuleData::FromEnv(env);
  v8::Local<v8::ObjectTemplate> tmpl =
      ToLocal<v8::ObjectTemplate>(data->GetTemplate(env));

  napi_value foo, bar;
  data->GetFooBar(env, &foo, &bar);

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Name> names[2] = {ToLocal<v8::Name>(foo),
                                  ToLocal<v8::Name>(bar)};
  v8::Local<v8::Value>* values =
      reinterpret_cast<v8::Local<v8::Value>*>(args + 1);

  v8::Local<v8::Object> obj = v8::Object::NewAsLiteral(
      isolate, ToLocal<v8::Value>(args[0]), names, values, 2);

  return ToNapiValue(obj);
}

static napi_value obj_new_as_json(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value args[3];
  NODE_API_CALL(env,
                napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  ModuleData* data = ModuleData::FromEnv(env);
  v8::Local<v8::ObjectTemplate> tmpl =
      ToLocal<v8::ObjectTemplate>(data->GetTemplate(env));

  napi_value foo, bar;
  data->GetFooBar(env, &foo, &bar);

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Name> names[2] = {ToLocal<v8::Name>(foo),
                                  ToLocal<v8::Name>(bar)};
  v8::Local<v8::Value>* values =
      reinterpret_cast<v8::Local<v8::Value>*>(args + 1);

  v8::Local<v8::Object> obj = v8::Object::NewAsJson(
      isolate, ToLocal<v8::Value>(args[0]), names, values, 2);

  return ToNapiValue(obj);
}

static napi_value obj_new_from_js(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value result, args[3], undefined;
  NODE_API_CALL(env,
                napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));
  NODE_API_CALL(env, napi_get_undefined(env, &undefined));
  NODE_API_CALL(
      env, napi_call_function(env, undefined, args[0], 2, (args + 1), &result));
  return result;
}

static napi_value Init(napi_env env, napi_value exports) {
  NODE_API_CALL(env,
                napi_set_instance_data(
                    env, new ModuleData(), ModuleData::Finalize, nullptr));

  napi_property_descriptor descriptors[] = {
      DECLARE_NODE_API_PROPERTY("js_perf_start", js_perf_start),
      DECLARE_NODE_API_PROPERTY("js_perf_end", js_perf_end),
      DECLARE_NODE_API_PROPERTY("obj_napi", obj_napi),
      DECLARE_NODE_API_PROPERTY("obj_tmpl", obj_tmpl),
      DECLARE_NODE_API_PROPERTY("obj_new_data_prop", obj_new_data_prop),
      DECLARE_NODE_API_PROPERTY("obj_new", obj_new),
      DECLARE_NODE_API_PROPERTY("obj_new_as_literal", obj_new_as_literal),
      DECLARE_NODE_API_PROPERTY("obj_new_as_json", obj_new_as_json),
      DECLARE_NODE_API_PROPERTY("obj_new_from_js", obj_new_from_js),
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
