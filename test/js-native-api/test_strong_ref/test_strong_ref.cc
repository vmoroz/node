#define NAPI_EXPERIMENTAL
#include <js_native_api.h>
#include <vector>
#include "../common.h"

static napi_value Create(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env,
                napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  NODE_API_ASSERT(env, argc == 1, "Wrong type of arguments. Expects one arg.");

  napi_ref strong_ref;
  NODE_API_CALL(env,
                node_api_create_reference(
                    env, args[0], node_api_reftype_any, 1, &strong_ref));

  std::vector<napi_ref>* strong_ref_values;
  NODE_API_CALL(env,
                napi_get_instance_data(
                    env, reinterpret_cast<void**>(&strong_ref_values)));

  if (strong_ref_values == nullptr) {
    strong_ref_values = new std::vector<napi_ref>();
    NODE_API_CALL(
        env,
        napi_set_instance_data(
            env,
            strong_ref_values,
            [](napi_env env, void* finalize_data, void* finalize_hint) {
              delete reinterpret_cast<std::vector<napi_ref>*>(finalize_data);
            },
            nullptr));
  }

  size_t index = strong_ref_values->size();
  strong_ref_values->push_back(strong_ref);

  napi_value result;
  NODE_API_CALL(env,
                napi_create_uint32(env, static_cast<uint32_t>(index), &result));

  return result;
}

static napi_value GetValue(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env,
                napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  NODE_API_ASSERT(env, argc == 1, "Wrong type of arguments. Expects one arg.");

  napi_valuetype value_type;
  NODE_API_CALL(env, napi_typeof(env, args[0], &value_type));
  NODE_API_ASSERT(env, value_type == napi_number, "Argument must be a number.");

  uint32_t index;
  NODE_API_CALL(env, napi_get_value_uint32(env, args[0], &index));

  std::vector<napi_ref>* strong_ref_values;
  NODE_API_CALL(env,
                napi_get_instance_data(
                    env, reinterpret_cast<void**>(&strong_ref_values)));
  NODE_API_ASSERT(
      env, strong_ref_values != nullptr, "Cannot get instance data.");

  napi_ref strong_ref = strong_ref_values->at(index);
  napi_value value;
  NODE_API_CALL(env, napi_get_reference_value(env, strong_ref, &value));

  return value;
}

static napi_value Ref(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env,
                napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  NODE_API_ASSERT(env, argc == 1, "Wrong type of arguments. Expects one arg.");

  napi_valuetype value_type;
  NODE_API_CALL(env, napi_typeof(env, args[0], &value_type));
  NODE_API_ASSERT(env, value_type == napi_number, "Argument must be a number.");

  uint32_t index;
  NODE_API_CALL(env, napi_get_value_uint32(env, args[0], &index));

  std::vector<napi_ref>* strong_ref_values;
  NODE_API_CALL(env,
                napi_get_instance_data(
                    env, reinterpret_cast<void**>(&strong_ref_values)));
  NODE_API_ASSERT(
      env, strong_ref_values != nullptr, "Cannot get instance data.");

  napi_ref strong_ref = strong_ref_values->at(index);
  NODE_API_CALL(env, napi_reference_ref(env, strong_ref, nullptr));

  napi_value undefined;
  NODE_API_CALL(env, napi_get_undefined(env, &undefined));
  return undefined;
}

static napi_value Unref(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env,
                napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  NODE_API_ASSERT(env, argc == 1, "Wrong type of arguments. Expects one arg.");

  napi_valuetype value_type;
  NODE_API_CALL(env, napi_typeof(env, args[0], &value_type));
  NODE_API_ASSERT(env, value_type == napi_number, "Argument must be a number.");

  uint32_t index;
  NODE_API_CALL(env, napi_get_value_uint32(env, args[0], &index));

  std::vector<napi_ref>* strong_ref_values;
  NODE_API_CALL(env,
                napi_get_instance_data(
                    env, reinterpret_cast<void**>(&strong_ref_values)));
  NODE_API_ASSERT(
      env, strong_ref_values != nullptr, "Cannot get instance data.");

  napi_ref strong_ref = strong_ref_values->at(index);
  uint32_t ref_count;
  NODE_API_CALL(env, napi_reference_unref(env, strong_ref, &ref_count));

  napi_value result;
  NODE_API_CALL(env, napi_get_boolean(env, ref_count == 0, &result));
  return result;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor properties[] = {
      DECLARE_NODE_API_PROPERTY("Create", Create),
      DECLARE_NODE_API_PROPERTY("GetValue", GetValue),
      DECLARE_NODE_API_PROPERTY("Ref", Ref),
      DECLARE_NODE_API_PROPERTY("Unref", Unref),
  };

  NODE_API_CALL(
      env,
      napi_define_properties(
          env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}
EXTERN_C_END
