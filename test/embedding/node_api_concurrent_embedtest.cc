#include "node_api_embedtest.h"

#define NAPI_EXPERIMENTAL
#include <node_api_embedding.h>

#include <thread>

const char* main_script =
    "globalThis.require = require('module').createRequire(process.execPath);\n"
    "require('vm').runInThisContext(process.argv[1]);";

extern "C" int node_api_concurrent_test_main(int argc, char** argv) {
  std::atomic<int32_t> global_count{0};
  std::atomic<int32_t> global_exit_code{0};
  node_api_platform platform;
  CHECK(node_api_create_platform(argc, argv, nullptr, &platform));

  const size_t thread_count = 12;
  std::vector<std::thread> threads;
  threads.reserve(thread_count);
  for (size_t i = 0; i < thread_count; i++) {
    threads.emplace_back([platform, &global_count, &global_exit_code] {
      int exit_code = [&]() {
        napi_env env;
        CHECK(node_api_create_environment(
            platform, nullptr, main_script, NAPI_VERSION, &env));

        CHECK(node_api_open_environment_scope(env));

        napi_value global, my_count;
        CHECK(napi_get_global(env, &global));
        CHECK(napi_get_named_property(env, global, "my_count", &my_count));
        int32_t count;
        CHECK(napi_get_value_int32(env, my_count, &count));
        global_count.fetch_add(count);

        CHECK(node_api_close_environment_scope(env));
        CHECK(node_api_destroy_environment(env, nullptr));
        return 0;
      }();
      if (exit_code != 0) {
        global_exit_code = exit_code;
      }
    });
  } 

  for (size_t i = 0; i < thread_count; i++) {
    threads[i].join();
  }

  CHECK_EXIT_CODE(global_exit_code);

  CHECK(node_api_destroy_platform(platform));

  fprintf(stdout, "%d\n", global_count.load());  

  return 0;
}
