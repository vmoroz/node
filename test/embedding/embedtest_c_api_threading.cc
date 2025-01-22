#include "embedtest_c_api_common.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

using namespace node;
using namespace node::embedding;

// Tests that multiple runtimes can be run at the same time in their own
// threads. The test creates 12 threads and 12 runtimes. Each runtime runs in it
// own thread.
extern "C" int32_t test_main_c_cpp_api_threading_runtime_per_thread(
    int32_t argc, char* argv[]) {
  const size_t thread_count = 12;
  std::vector<std::thread> threads;
  threads.reserve(thread_count);
  std::atomic<int32_t> global_count{0};
  std::atomic<NodeStatus> global_status{NodeStatus::kOk};

  NodeScopedErrorHandler error_handler{};
  {
    NodeExpected<NodePlatform> expected_platform =
        NodePlatform::Create(NodeArgs(argc, argv), nullptr);
    CHECK_EXPECTED_OR_EXIT(argv[0], expected_platform);
    NodePlatform platform = std::move(expected_platform).value();
    if (!platform) {
      return 0;  // early return
    }

    for (size_t i = 0; i < thread_count; i++) {
      threads.emplace_back([&platform, &global_count, &global_status] {
        NodeExpected<void> result = NodeRuntime::Run(
            platform,
            [&](const NodePlatform& platform,
                const NodeRuntimeConfig& runtime_config) {
              // Inspector can be associated with only one
              // runtime in the process.
              NODE_EMBEDDED_CALL(runtime_config.SetFlags(
                  NodeRuntimeFlags::kDefault |
                  NodeRuntimeFlags::kNoCreateInspector));
              NODE_EMBEDDED_CALL(LoadUtf8Script(
                  runtime_config,
                  main_script,
                  [&](const NodeRuntime& runtime,
                      napi_env env,
                      napi_value /*value*/) {
                    napi_value global, my_count;
                    NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
                    NODE_API_CALL_RETURN_VOID(napi_get_named_property(
                        env, global, "myCount", &my_count));
                    int32_t count;
                    NODE_API_CALL_RETURN_VOID(
                        napi_get_value_int32(env, my_count, &count));
                    global_count.fetch_add(count);
                  }));
              return NodeExpected<void>{};
            });
        if (result.has_error()) {
          global_status.store(result.status());
        }
      });
    }

    for (size_t i = 0; i < thread_count; i++) {
      threads[i].join();
    }

    CHECK_EXPECTED_OR_EXIT(argv[0], NodeExpected<void>(global_status.load()));
  }

  fprintf(stdout, "%d\n", global_count.load());
  return 0;
}

// Tests that multiple runtimes can run in the same thread.
// The runtime scope must be opened and closed for each use.
// There are 12 runtimes that share the same main thread.
extern "C" int32_t test_main_c_cpp_api_threading_several_runtimes_per_thread(
    int32_t argc, char* argv[]) {
  const size_t runtime_count = 12;
  bool more_work = false;
  int32_t global_count = 0;

  NodeScopedErrorHandler error_handler{};
  {
    NodeExpected<NodePlatform> expected_platform =
        NodePlatform::Create(NodeArgs(argc, argv), nullptr);
    CHECK_EXPECTED_OR_EXIT(argv[0], expected_platform);
    NodePlatform platform = std::move(expected_platform).value();

    // We declared list of NodeRuntime after NodePlatform to ensure that they
    // are released before the platform.
    std::vector<NodeRuntime> runtimes;
    runtimes.reserve(runtime_count);

    for (size_t i = 0; i < runtime_count; i++) {
      NodeExpected<NodeRuntime> expected_runtime = NodeRuntime::Create(
          platform,
          [](const NodePlatform& platform,
             const NodeRuntimeConfig& runtime_config) {
            // Inspector can be associated with only one runtime in the process.
            NODE_EMBEDDED_CALL(
                runtime_config.SetFlags(NodeRuntimeFlags::kDefault |
                                        NodeRuntimeFlags::kNoCreateInspector));
            NODE_EMBEDDED_CALL(LoadUtf8Script(runtime_config, main_script));
            return NodeExpected<void>();
          });

      CHECK_EXPECTED_OR_EXIT(argv[0], expected_runtime);
      NodeRuntime runtime = std::move(expected_runtime).value();

      CHECK_EXPECTED_OR_EXIT(
          argv[0],
          runtime.RunNodeApi([&](const NodeRuntime& runtime, napi_env env) {
            napi_value undefined, global, func;
            NODE_API_CALL_RETURN_VOID(napi_get_undefined(env, &undefined));
            NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
            NODE_API_CALL_RETURN_VOID(
                napi_get_named_property(env, global, "incMyCount", &func));

            napi_valuetype func_type;
            NODE_API_CALL_RETURN_VOID(napi_typeof(env, func, &func_type));
            NODE_API_ASSERT_RETURN_VOID(func_type == napi_function);
            NODE_API_CALL_RETURN_VOID(
                napi_call_function(env, undefined, func, 0, nullptr, nullptr));
          }));

      runtimes.push_back(std::move(runtime));
    }

    do {
      more_work = false;
      for (const NodeRuntime& runtime : runtimes) {
        NodeExpected<bool> has_more_work = runtime.RunEventLoopNoWait();
        CHECK_EXPECTED_OR_EXIT(argv[0], has_more_work);
        more_work |= has_more_work.value();
      }
    } while (more_work);

    for (const NodeRuntime& runtime : runtimes) {
      CHECK_EXPECTED_OR_EXIT(
          argv[0],
          runtime.RunNodeApi([&](const NodeRuntime& runtime, napi_env env) {
            napi_value global, my_count;
            NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
            NODE_API_CALL_RETURN_VOID(
                napi_get_named_property(env, global, "myCount", &my_count));

            napi_valuetype my_count_type;
            NODE_API_CALL_RETURN_VOID(
                napi_typeof(env, my_count, &my_count_type));
            NODE_API_ASSERT_RETURN_VOID(my_count_type == napi_number);
            int32_t count;
            NODE_API_CALL_RETURN_VOID(
                napi_get_value_int32(env, my_count, &count));

            global_count += count;
          }));
      CHECK_EXPECTED_OR_EXIT(argv[0], runtime.RunEventLoop());
    }
  }

  fprintf(stdout, "%d\n", global_count);
  return 0;
}

// Tests that a runtime can be invoked from different threads as long as only
// one thread uses it at a time.
extern "C" int32_t test_main_c_cpp_api_threading_runtime_in_several_threads(
    int32_t argc, char* argv[]) {
  // Use mutex to synchronize access to the runtime.
  std::mutex mutex;
  std::atomic<int32_t> result_count{0};
  std::atomic<NodeStatus> result_status{NodeStatus::kOk};
  const size_t thread_count = 5;
  std::vector<std::thread> threads;
  threads.reserve(thread_count);

  NodeScopedErrorHandler error_handler{};
  {
    NodeExpected<NodePlatform> expected_platform =
        NodePlatform::Create(NodeArgs(argc, argv), nullptr);
    CHECK_EXPECTED_OR_EXIT(argv[0], expected_platform);
    NodePlatform platform = std::move(expected_platform).value();
    if (!platform) {
      return 0;  // early return
    }

    NodeExpected<NodeRuntime> expected_runtime = NodeRuntime::Create(
        platform,
        [](const NodePlatform& platform,
           const NodeRuntimeConfig& runtime_config) {
          return LoadUtf8Script(runtime_config, main_script);
        });

    CHECK_EXPECTED_OR_EXIT(argv[0], expected_runtime);
    NodeRuntime runtime = std::move(expected_runtime).value();

    for (size_t i = 0; i < thread_count; i++) {
      threads.emplace_back([&runtime, &result_count, &result_status, &mutex] {
        std::scoped_lock lock(mutex);
        NodeExpected<void> run_result =
            runtime.RunNodeApi([&](const NodeRuntime& runtime, napi_env env) {
              napi_value undefined, global, func, my_count;
              NODE_API_CALL_RETURN_VOID(napi_get_undefined(env, &undefined));
              NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
              NODE_API_CALL_RETURN_VOID(
                  napi_get_named_property(env, global, "incMyCount", &func));

              napi_valuetype func_type;
              NODE_API_CALL_RETURN_VOID(napi_typeof(env, func, &func_type));
              NODE_API_ASSERT_RETURN_VOID(func_type == napi_function);
              NODE_API_CALL_RETURN_VOID(napi_call_function(
                  env, undefined, func, 0, nullptr, nullptr));

              NODE_API_CALL_RETURN_VOID(
                  napi_get_named_property(env, global, "myCount", &my_count));
              napi_valuetype count_type;
              NODE_API_CALL_RETURN_VOID(
                  napi_typeof(env, my_count, &count_type));
              NODE_API_ASSERT_RETURN_VOID(count_type == napi_number);
              int32_t count;
              NODE_API_CALL_RETURN_VOID(
                  napi_get_value_int32(env, my_count, &count));
              result_count.store(count);
            });
        if (run_result.has_error()) {
          result_status.store(run_result.status());
        }
      });
    }

    for (size_t i = 0; i < thread_count; i++) {
      threads[i].join();
    }

    CHECK_EXPECTED_OR_EXIT(argv[0], NodeExpected<void>(result_status.load()));
    CHECK_EXPECTED_OR_EXIT(argv[0], runtime.RunEventLoop());
  }

  fprintf(stdout, "%d\n", result_count.load());
  return 0;
}

// Tests that a the runtime's event loop can be called from the UI thread
// event loop.
extern "C" int32_t test_main_c_cpp_api_threading_runtime_in_ui_thread(
    int32_t argc, char* argv[]) {
  // A simulation of the UI thread's event loop implemented as a dispatcher
  // queue. Note that it is a very simplistic implementation not suitable
  // for the real apps.
  class UIQueue {
   public:
    void PostTask(std::function<void()>&& task) {
      std::scoped_lock lock(mutex_);
      if (!is_finished_) {
        tasks_.push_back(std::move(task));
        wakeup_.notify_one();
      }
    }

    void Run() {
      for (;;) {
        std::function<void()> task;
        {
          std::unique_lock lock(mutex_);
          wakeup_.wait(lock, [&] { return is_finished_ || !tasks_.empty(); });
          if (is_finished_) break;
          task = std::move(tasks_.front());
          tasks_.pop_front();
        }
        task();
      }
    }

    void Stop() {
      std::scoped_lock lock(mutex_);
      if (!is_finished_) {
        is_finished_ = true;
        wakeup_.notify_one();
      }
    }

   private:
    std::mutex mutex_;
    std::condition_variable wakeup_;
    std::deque<std::function<void()>> tasks_;
    bool is_finished_{false};
  } ui_queue;

  NodeScopedErrorHandler error_handler{};
  {
    NodeExpected<NodePlatform> expected_platform =
        NodePlatform::Create(NodeArgs(argc, argv), nullptr);
    CHECK_EXPECTED_OR_EXIT(argv[0], expected_platform);
    NodePlatform platform = std::move(expected_platform).value();
    if (!platform) {
      return 0;  // early return
    }

    NodeRuntime runtime{nullptr};
    NodeExpected<NodeRuntime> expected_runtime = NodeRuntime::Create(
        platform,
        [&](const NodePlatform& platform,
            const NodeRuntimeConfig& runtime_config) {
          // The callback will be invoked from the runtime's event loop
          // observer thread. It must schedule the work to the UI thread's
          // event loop.
          NODE_EMBEDDED_CALL(runtime_config.SetTaskRunner(
              // We capture the ui_queue by reference here because we
              // guarantee it to be alive till the end of the test. In
              // real applications, you should use a safer way to
              // capture the dispatcher queue.
              [&ui_queue, &runtime](NodeRunTaskCallback run_task) {
                // TODO: figure out the termination scenario.
                ui_queue.PostTask([run_task =
                                       std::make_shared<NodeRunTaskCallback>(
                                           std::move(run_task)),
                                   &runtime,
                                   &ui_queue]() {
                  (*run_task)();  // TODO: handle result
                  // Check myCount and stop the processing when it reaches 5.
                  int32_t count{};
                  runtime.RunNodeApi([&](const NodeRuntime& runtime,
                                         napi_env env) {
                    napi_value global, my_count;
                    NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
                    NODE_API_CALL_RETURN_VOID(napi_get_named_property(
                        env, global, "myCount", &my_count));
                    napi_valuetype count_type;
                    NODE_API_CALL_RETURN_VOID(
                        napi_typeof(env, my_count, &count_type));
                    NODE_API_ASSERT_RETURN_VOID(count_type == napi_number);
                    NODE_API_CALL_RETURN_VOID(
                        napi_get_value_int32(env, my_count, &count));
                  });
                  if (count == 5) {
                    runtime.RunEventLoop();
                    fprintf(stdout, "%d\n", count);
                    ui_queue.Stop();
                  }
                });
                return NodeExpected<bool>(true);
              }));

          return LoadUtf8Script(runtime_config, main_script);
        });
    CHECK_EXPECTED_OR_EXIT(argv[0], expected_runtime);
    runtime = std::move(expected_runtime).value();

    // The initial task starts the JS code that then will do the timer
    // scheduling. The timer supposed to be handled by the runtime's event loop.
    ui_queue.PostTask([&runtime]() {
      runtime.RunNodeApi([&](const NodeRuntime& runtime, napi_env env) {
        napi_value undefined, global, func;
        NODE_API_CALL_RETURN_VOID(napi_get_undefined(env, &undefined));
        NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
        NODE_API_CALL_RETURN_VOID(
            napi_get_named_property(env, global, "incMyCount", &func));

        napi_valuetype func_type;
        NODE_API_CALL_RETURN_VOID(napi_typeof(env, func, &func_type));
        NODE_API_ASSERT_RETURN_VOID(func_type == napi_function);
        NODE_API_CALL_RETURN_VOID(
            napi_call_function(env, undefined, func, 0, nullptr, nullptr));
      });
    });

    ui_queue.Run();
  }

  return 0;
}
