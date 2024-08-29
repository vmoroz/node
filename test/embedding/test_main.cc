#include <assert.h>
#include "executable_wrapper.h"
#include "node.h"

extern "C" int cpp_api_test_main(size_t argc, const char* argv[]);
extern "C" int node_api_test_main(size_t argc, const char* argv[]);
extern "C" int node_api_modules_test_main(size_t argc, const char* argv[]);
extern "C" int node_api_concurrent_test_main(size_t argc, const char* argv[]);
extern "C" int node_api_multi_env_test_main(size_t argc, const char* argv[]);
extern "C" int node_api_multi_thread_test_main(size_t argc, const char* argv[]);
extern "C" int test_main_snapshot_node_api(size_t argc, const char* argv[]);

typedef int (*main_callback)(size_t argc, const char* argv[]);

int CallWithoutArg1(main_callback main, int argc, char** argv) {
  for (int i = 2; i < argc; i++) {
    argv[i - 1] = argv[i];
  }
  argv[--argc] = nullptr;
  return main(static_cast<size_t>(argc), const_cast<const char**>(argv));
}

NODE_MAIN(int argc, node::argv_type raw_argv[]) {
  char** argv = nullptr;
  node::FixupMain(argc, raw_argv, &argv);

  if (argc > 1) {
    const char* arg1 = argv[1];
    if (strcmp(arg1, "cpp-api") == 0) {
      return CallWithoutArg1(cpp_api_test_main, argc, argv);
    } else if (strcmp(arg1, "node-api") == 0) {
      return CallWithoutArg1(node_api_test_main, argc, argv);
    } else if (strcmp(arg1, "node-api-modules") == 0) {
      return CallWithoutArg1(node_api_modules_test_main, argc, argv);
    } else if (strcmp(arg1, "node-api-concurrent") == 0) {
      return CallWithoutArg1(node_api_concurrent_test_main, argc, argv);
    } else if (strcmp(arg1, "node-api-multi-env") == 0) {
      return CallWithoutArg1(node_api_multi_env_test_main, argc, argv);
    } else if (strcmp(arg1, "node-api-multi-thread") == 0) {
      return CallWithoutArg1(node_api_multi_thread_test_main, argc, argv);
    } else if (strcmp(arg1, "snapshot-node-api") == 0) {
      return CallWithoutArg1(test_main_snapshot_node_api, argc, argv);
    }
  }
  return cpp_api_test_main(static_cast<size_t>(argc),
                           const_cast<const char**>(argv));
}
