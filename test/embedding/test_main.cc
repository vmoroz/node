#include <assert.h>
#include "executable_wrapper.h"
#include "node.h"

extern "C" int cpp_api_test_main(int argc, char** argv);
extern "C" int node_api_test_main(int argc, char** argv);
extern "C" int node_api_modules_test_main(int argc, char** argv);

NODE_MAIN(int argc, node::argv_type raw_argv[]) {
  char** argv = nullptr;
  node::FixupMain(argc, raw_argv, &argv);
  if (argc > 1) {
    const char* lastArg = argv[argc - 1];
    if (strcmp(lastArg, "--cpp-api") == 0) {
      return cpp_api_test_main(argc - 1, argv);
    } else if (strcmp(lastArg, "--node-api") == 0) {
      return node_api_test_main(argc - 1, argv);
    } else if (strcmp(lastArg, "--node-api-modules") == 0) {
      return node_api_modules_test_main(argc - 1, argv);
    }
  }
  return cpp_api_test_main(argc, argv);
}