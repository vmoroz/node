#ifndef TEST_EMBEDDING_EMBEDTEST_C_API_COMMON_H_
#define TEST_EMBEDDING_EMBEDTEST_C_API_COMMON_H_

#define NAPI_EXPERIMENTAL

#include <node_embedding_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const char* main_script;

int32_t StatusToExitCode(node_embedding_status status);

node_embedding_status PrintErrorMessage(const char* exe_name,
                                        node_embedding_status status);

node_embedding_status LoadUtf8Script(
    node_embedding_runtime_config runtime_config, const char* script);

void GetAndThrowLastErrorMessage(napi_env env);

void ThrowLastErrorMessage(napi_env env, const char* format, ...);

//==============================================================================
// Error handling macros copied from test/js_native_api/common.h
//==============================================================================

// Empty value so that macros here are able to return NULL or void
#define NODE_API_RETVAL_NOTHING

#define NODE_API_FAIL_BASE(ret_val, ...)                                       \
  do {                                                                         \
    ThrowLastErrorMessage(env, __VA_ARGS__);                                   \
    return ret_val;                                                            \
  } while (0)

// Returns NULL on failed assertion.
// This is meant to be used inside napi_callback methods.
#define NODE_API_FAIL(...) NODE_API_FAIL_BASE(napi_generic_failure, __VA_ARGS__)

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
#define NODE_API_CALL_RETURN(expr) NODE_API_CALL_BASE(expr, NULL)

// Returns empty if the_call doesn't return napi_ok.
#define NODE_API_CALL_RETURN_VOID(expr)                                        \
  NODE_API_CALL_BASE(expr, NODE_API_RETVAL_NOTHING)

#define NODE_API_CALL_EXPECTED(expr)                                           \
  NODE_API_CALL_BASE(expr, NodeExpected<napi_value>(nullptr))

#define NODE_API_CALL(expr)                                                    \
  do {                                                                         \
    napi_status status = (expr);                                               \
    if (status != napi_ok) {                                                   \
      return status;                                                           \
    }                                                                          \
  } while (0)

#define NODE_EMBEDDED_CALL(expr)                                               \
  do {                                                                         \
    node_embedding_status status = (expr);                                     \
    if (status != node_embedding_status_ok) {                                  \
      return status;                                                           \
    }                                                                          \
  } while (0)

#define CHECK_EXPECTED_OR_EXIT(exe_name, expr)                                 \
  do {                                                                         \
    node_embedding_status status_ = (expr);                                    \
    if (status_ != node_embedding_status_ok) {                                 \
      exit(StatusToExitCode(PrintErrorMessage(exe_name, status_)));            \
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

#endif  // TEST_EMBEDDING_EMBEDTEST_C_API_COMMON_H_
