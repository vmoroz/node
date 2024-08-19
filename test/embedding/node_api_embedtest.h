#ifndef TEST_EMBEDDING_NODE_API_EMBEDTEST_H_
#define TEST_EMBEDDING_NODE_API_EMBEDTEST_H_

#define CHECK(expr)                                                            \
  if ((expr) != napi_ok) {                                                     \
    fprintf(stderr, "Failed: %s\n", #expr);                                    \
    fprintf(stderr, "File: %s\n", __FILE__);                                   \
    fprintf(stderr, "Line: %d\n", __LINE__);                                   \
    return 1;                                                                  \
  }

#endif  // TEST_EMBEDDING_NODE_API_EMBEDTEST_H_