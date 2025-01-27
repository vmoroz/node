#ifndef TEST_EMBEDDING_EMBEDTEST_C_CPP_API_COMMON_H_
#define TEST_EMBEDDING_EMBEDTEST_C_CPP_API_COMMON_H_

#define NAPI_EXPERIMENTAL

#include <node_embedding_api_cpp.h>

namespace node::embedding {

extern const char* main_script;

napi_status AddUtf8String(std::string& str, napi_env env, napi_value value);

void GetAndThrowLastErrorMessage(napi_env env);

void ThrowLastErrorMessage(napi_env env, const char* message);

NodeExpected<void> LoadUtf8Script(const NodeRuntimeConfig& runtime_config,
                                  std::string_view script);

NodeExpected<void> PrintErrorMessage(std::string_view exe_name,
                                     NodeStatus status);

class NodeErrorHandler {
 public:
  void SetEmbeddingStatus(NodeStatus embedding_status) {
    embedding_status_ = embedding_status;
  }

  void SetEmbeddingError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::string message = NodeFormatString(format, args);
    va_end(args);
    node_embedding_last_error_message_set(message.c_str());
  }

  NodeStatus error_value() { return embedding_status_; }

  bool has_embedding_error() const {
    return embedding_status_ != NodeStatus::kOk;
  }

  NodeStatus embedding_status() const { return embedding_status_; }

 private:
  NodeStatus embedding_status_ = NodeStatus::kOk;
};

template <typename TValue>
class TestErrorHandler : public NodeErrorHandler {
 public:
  TestErrorHandler(napi_env env) : env_(env) {}

  void SetNodeApiStatus(napi_status node_api_status) {
    node_api_status_ = node_api_status;
  }

  void ThrowNodeApiError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::string message = NodeFormatString(format, args);
    va_end(args);
    ThrowLastErrorMessage(env_, message.c_str());
  }

  TValue error_value() { return TValue(); }

  bool has_node_api_error() const { return node_api_status_ != napi_ok; }

 private:
  napi_env env_ = nullptr;
  napi_status node_api_status_ = napi_ok;
};

template <typename TValue>
class TestErrorHandler<NodeExpected<TValue>> : public NodeErrorHandler {
 public:
  TestErrorHandler() {}

  void SetNodeApiStatus(napi_status node_api_status) {
    node_api_status_ = node_api_status;
  }

  void ThrowNodeApiError(const char* format, ...);

  NodeExpected<TValue> error_value() {
    return NodeExpected<TValue>(embedding_status());
  }

  bool has_node_api_error() const { return node_api_status_ != napi_ok; }

 private:
  napi_status node_api_status_ = napi_ok;
};

class TestExitCodeHandler : public NodeErrorHandler {
 public:
  TestExitCodeHandler(const char* exe_name) : exe_name_(exe_name) {}

  void SetNodeApiStatus(napi_status node_api_status) {
    node_api_status_ = node_api_status;
  }

  void ThrowNodeApiError(const char* format, ...);

  int32_t error_value() {
    return PrintErrorMessage(exe_name_, embedding_status()).exit_code();
  }

 private:
  const char* exe_name_ = nullptr;
  napi_status node_api_status_ = napi_ok;
};

}  // namespace node::embedding

//==============================================================================
// Error handing macros
//==============================================================================

#define NODE_API_CALL(expr)                                                    \
  do {                                                                         \
    error_handler.SetNodeApiStatus(expr);                                      \
    if (error_handler.has_node_api_error()) {                                  \
      return error_handler.error_value();                                      \
    }                                                                          \
  } while (0)

#define NODE_API_ASSERT(expr)                                                  \
  do {                                                                         \
    if (!(expr)) {                                                             \
      error_handler.ThrowNodeApiError(                                         \
          "Failed: %s\nFile: %s\nLine: %d\n", #expr, __FILE__, __LINE__);      \
      return error_handler.error_value();                                      \
    }                                                                          \
  } while (0)

#define NODE_API_FAIL(format, ...)                                             \
  do {                                                                         \
    error_handler.ThrowNodeApiError(format, __VA_ARGS__);                      \
    return error_handler.error_value();                                        \
  } while (0)

#define NODE_EMBEDDING_CALL(expr)                                              \
  do {                                                                         \
    error_handler.SetEmbeddingStatus((expr).status());                         \
    if (error_handler.has_embedding_error()) {                                 \
      return error_handler.error_value();                                      \
    }                                                                          \
  } while (0)

#define NODE_EMBEDDING_ASSERT(expr)                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      error_handler.SetEmbeddingError(                                         \
          "Failed: %s\nFile: %s\nLine: %d\n", #expr, __FILE__, __LINE__);      \
      error_handler.SetEmbeddingStatus(NodeStatus::kGenericError);             \
      return error_handler.error_value();                                      \
    }                                                                          \
  } while (0)

#endif  // TEST_EMBEDDING_EMBEDTEST_C_CPP_API_COMMON_H_
