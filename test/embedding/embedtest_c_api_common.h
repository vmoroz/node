#ifndef TEST_EMBEDDING_EMBEDTEST_NODE_API_H_
#define TEST_EMBEDDING_EMBEDTEST_NODE_API_H_

#define NAPI_EXPERIMENTAL
#include <node_embedding_api_cpp.h>

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace node::embedding {

extern const char* main_script;

napi_status AddUtf8String(std::string& str, napi_env env, napi_value value);

void GetAndThrowLastErrorMessage(napi_env env);

void ThrowLastErrorMessage(napi_env env, const char* message);

NodeExpected<void> LoadUtf8Script(
    const NodeRuntimeConfig& runtime_config,
    std::string script,
    NodeHandleExecutionResultCallback handle_result = {});

NodeExpected<void> PrintErrorMessage(NodeExpected<void> expected,
                                     std::string_view exe_name);

}  // namespace node::embedding

//
// Error handling macros copied from test/js_native_api/common.h
//
#if 0
// Empty value so that macros here are able to return NULL or void
#define NODE_API_RETVAL_NOTHING  // Intentionally blank #define

#define NODE_API_FAIL_BASE(ret_val, ...)                                       \
  do {                                                                         \
    ThrowLastErrorMessage(env, FormatString(__VA_ARGS__).c_str());             \
    return ret_val;                                                            \
  } while (0)

// Returns NULL on failed assertion.
// This is meant to be used inside napi_callback methods.
#define NODE_API_FAIL(...) NODE_API_FAIL_BASE(NULL, __VA_ARGS__)

// Returns empty on failed assertion.
// This is meant to be used inside functions with void return type.
#define NODE_API_FAIL_RETURN_VOID(...)                                         \
  NODE_API_FAIL_BASE(NODE_API_RETVAL_NOTHING, __VA_ARGS__)

#define NODE_API_ASSERT_BASE(expr, ret_val)                                    \
  do {                                                                         \
    if (!(expr)) {                                                             \
      napi_throw_error(env, NULL, "Failed: (" #expr ")");                      \
      return ret_val;                                                          \
    }                                                                          \
  } while (0)

// Returns NULL on failed assertion.
// This is meant to be used inside napi_callback methods.
#define NODE_API_ASSERT(expr) NODE_API_ASSERT_BASE(expr, NULL)

// Returns empty on failed assertion.
// This is meant to be used inside functions with void return type.
#define NODE_API_ASSERT_RETURN_VOID(expr)                                      \
  NODE_API_ASSERT_BASE(expr, NODE_API_RETVAL_NOTHING)

#define NODE_API_CALL_BASE(expr, ret_val)                                      \
  do {                                                                         \
    if ((expr) != napi_ok) {                                                   \
      GetAndThrowLastErrorMessage(env);                                        \
      return ret_val;                                                          \
    }                                                                          \
  } while (0)

// Returns NULL if the_call doesn't return napi_ok.
#define NODE_API_CALL(expr) NODE_API_CALL_BASE(expr, NULL)

// Returns empty if the_call doesn't return napi_ok.
#define NODE_API_CALL_RETURN_VOID(expr)                                        \
  NODE_API_CALL_BASE(expr, NODE_API_RETVAL_NOTHING)

#define CHECK_STATUS(expr)                                                     \
  do {                                                                         \
    node_embedding_status status_ = (expr);                                    \
    if (status_ != node_embedding_status_ok) {                                 \
      return status_;                                                          \
    }                                                                          \
  } while (0)

#define CHECK_EXPECTED_OR_EXIT(expr)                                           \
  do {                                                                         \
    node::embedding::NodeExpected<void> expected_ = (expr);                    \
    if (expected_.HasError()) {                                                \
      exit(expected_.ExitCode());                                              \
    }                                                                          \
  } while (0)

#define ASSERT_OR_EXIT(expr)                                                   \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "Failed: %s\n", #expr);                                  \
      fprintf(stderr, "File: %s\n", __FILE__);                                 \
      fprintf(stderr, "Line: %d\n", __LINE__);                                 \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

#endif

#endif  // TEST_EMBEDDING_EMBEDTEST_NODE_API_H_
