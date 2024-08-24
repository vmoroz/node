#ifndef TEST_EMBEDDING_NODE_API_EMBEDTEST_H_
#define TEST_EMBEDDING_NODE_API_EMBEDTEST_H_

#define CHECK(expr)                                                            \
  do {                                                                         \
    if ((expr) != napi_ok) {                                                   \
      fprintf(stderr, "Failed: %s\n", #expr);                                  \
      fprintf(stderr, "File: %s\n", __FILE__);                                 \
      fprintf(stderr, "Line: %d\n", __LINE__);                                 \
      return 1;                                                                \
    }                                                                          \
  } while (0)

#define CHECK_TRUE(expr)                                                       \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "Failed: %s\n", #expr);                                  \
      fprintf(stderr, "File: %s\n", __FILE__);                                 \
      fprintf(stderr, "Line: %d\n", __LINE__);                                 \
      return 1;                                                                \
    }                                                                          \
  } while (0)

#define FAIL(...)                                                              \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    return 1;                                                                  \
  } while (0)

#define CHECK_EXIT_CODE(code)                                                  \
  do {                                                                         \
    int exit_code = (code);                                                    \
    if (exit_code != 0) {                                                      \
      return exit_code;                                                        \
    }                                                                          \
  } while (0)

#endif  // TEST_EMBEDDING_NODE_API_EMBEDTEST_H_