#include <assert.h>
#include "executable_wrapper.h"
#include "node.h"

extern "C" int cpp_api_test_main(int argc, char** argv);
extern "C" int node_api_test_main(int argc, char** argv);
extern "C" int node_api_modules_test_main(int argc, char** argv);

void RemoveArg1(int& argc, char** argv) {
  for (int i = 2; i < argc; i++) {
    argv[i - 1] = argv[i];
  }
  argc--;
  argv[argc] = nullptr;
}

NODE_MAIN(int argc, node::argv_type raw_argv[]) {
  char** argv = nullptr;
  node::FixupMain(argc, raw_argv, &argv);

  if (argc > 1) {
    const char* arg1 = argv[1];
    if (strcmp(arg1, "cpp-api") == 0) {
      RemoveArg1(argc, argv);
      return cpp_api_test_main(argc, argv);
    } else if (strcmp(arg1, "node-api") == 0) {
      RemoveArg1(argc, argv);
      return node_api_test_main(argc, argv);
    } else if (strcmp(arg1, "node-api-modules") == 0) {
      RemoveArg1(argc, argv);
      return node_api_modules_test_main(argc, argv);
    }
  }
  return cpp_api_test_main(argc, argv);
}
