#include <cstring>
#include "executable_wrapper.h"

extern "C" int32_t test_main_cpp_api(int32_t argc, char* argv[]);
extern "C" int32_t test_main_c_cpp_api(int32_t argc, char* argv[]);
extern "C" int32_t test_main_c_cpp_api_nodejs_main(int32_t argc, char* argv[]);
extern "C" int32_t test_main_c_cpp_api_threading_runtime_per_thread(
    int32_t argc, char* argv[]);
extern "C" int32_t test_main_c_cpp_api_threading_several_runtimes_per_thread(
    int32_t argc, char* argv[]);
extern "C" int32_t test_main_c_cpp_api_threading_runtime_in_several_threads(
    int32_t argc, char* argv[]);
extern "C" int32_t test_main_c_cpp_api_threading_runtime_in_ui_thread(
    int32_t argc, char* argv[]);
extern "C" int32_t test_main_c_cpp_api_preload(int32_t argc, char* argv[]);
extern "C" int32_t test_main_c_cpp_api_linked_modules(int32_t argc,
                                                      char* argv[]);
extern "C" int32_t test_main_c_cpp_api_env_no_browser_globals(int32_t argc,
                                                              char* argv[]);
extern "C" int32_t test_main_c_cpp_api_env_with_esm_loader(int32_t argc,
                                                           char* argv[]);
extern "C" int32_t test_main_c_cpp_api_env_with_no_esm_loader(int32_t argc,
                                                              char* argv[]);

// extern "C" int32_t test_main_modules_node_api(int32_t argc, char*
// argv[]);

typedef int32_t (*main_callback)(int32_t argc, char* argv[]);

int32_t CallWithoutArg1(main_callback main, int32_t argc, char** argv) {
  for (int32_t i = 2; i < argc; i++) {
    argv[i - 1] = argv[i];
  }
  argv[--argc] = nullptr;
  return main(argc, argv);
}

NODE_MAIN(int32_t argc, node::argv_type raw_argv[]) {
  char** argv = nullptr;
  node::FixupMain(argc, raw_argv, &argv);

  if (argc > 1) {
    const char* arg1 = argv[1];
    if (strcmp(arg1, "cpp-api") == 0) {
      return CallWithoutArg1(test_main_cpp_api, argc, argv);
    } else if (strcmp(arg1, "c-cpp-api") == 0) {
      return CallWithoutArg1(test_main_c_cpp_api, argc, argv);
    } else if (strcmp(arg1, "c-cpp-api-nodejs-main") == 0) {
      return CallWithoutArg1(test_main_c_cpp_api_nodejs_main, argc, argv);
    } else if (strcmp(arg1, "c-cpp-api-threading-runtime-per-thread") == 0) {
      return CallWithoutArg1(
          test_main_c_cpp_api_threading_runtime_per_thread, argc, argv);
    } else if (strcmp(arg1,
                      "c-cpp-api-threading-several-runtimes-per-thread") == 0) {
      return CallWithoutArg1(
          test_main_c_cpp_api_threading_several_runtimes_per_thread,
          argc,
          argv);
    } else if (strcmp(arg1, "c-cpp-api-threading-runtime-in-several-threads") ==
               0) {
      return CallWithoutArg1(
          test_main_c_cpp_api_threading_runtime_in_several_threads, argc, argv);
    } else if (strcmp(arg1, "c-cpp-api-threading-runtime-in-ui-thread") == 0) {
      return CallWithoutArg1(
          test_main_c_cpp_api_threading_runtime_in_ui_thread, argc, argv);
    } else if (strcmp(arg1, "c-cpp-api-preload") == 0) {
      return CallWithoutArg1(test_main_c_cpp_api_preload, argc, argv);
    } else if (strcmp(arg1, "c-cpp-api-linked-modules") == 0) {
      return CallWithoutArg1(test_main_c_cpp_api_linked_modules, argc, argv);
    } else if (strcmp(arg1, "c-cpp-api-env-no-browser-globals") == 0) {
      return CallWithoutArg1(
          test_main_c_cpp_api_env_no_browser_globals, argc, argv);
    } else if (strcmp(arg1, "c-cpp-api-env-with-esm-loader") == 0) {
      return CallWithoutArg1(
          test_main_c_cpp_api_env_with_esm_loader, argc, argv);
    } else if (strcmp(arg1, "c-cpp-api-env-with-no-esm-loader") == 0) {
      return CallWithoutArg1(
          test_main_c_cpp_api_env_with_no_esm_loader, argc, argv);
      //   } else if (strcmp(arg1, "modules-node-api") == 0) {
      //     return CallWithoutArg1(test_main_modules_node_api, argc,
      //     argv);
    }
  }
  return test_main_cpp_api(argc, argv);
}
