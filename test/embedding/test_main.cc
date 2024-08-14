#include <assert.h>
#include "executable_wrapper.h"
#include "node.h"

extern "C" int test_main(int argc, char** argv);
extern "C" int test_main_node_api(int argc, char** argv);
extern "C" int test_main_node_api_modules(int argc, char** argv);

NODE_MAIN(int argc, node::argv_type raw_argv[]) {
  char** argv = nullptr;
  node::FixupMain(argc, raw_argv, &argv);
  if (argc > 1) {
    if (strcmp(argv[1], "--node-api") == 0) {
      return test_main_node_api(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "--node-api-modules") == 0) {
      return test_main_node_api_modules(argc - 1, argv + 1);
    }
  }
  return test_main(argc, argv);
}