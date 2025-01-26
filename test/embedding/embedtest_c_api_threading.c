#include <uv.h>
#include "embedtest_c_api_common.h"

typedef struct {
  node_embedding_platform platform;
  uv_mutex_t mutex;
  int32_t global_count;
  node_embedding_status global_status;
} thread_data;

static void HandleExecutionResult(void* cb_data,
                                  node_embedding_runtime runtime,
                                  napi_env env,
                                  napi_value execution_result) {
  thread_data* data = (thread_data*)cb_data;
  napi_value global, my_count;
  NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
  NODE_API_CALL_RETURN_VOID(
      napi_get_named_property(env, global, "myCount", &my_count));
  int32_t count;
  NODE_API_CALL_RETURN_VOID(napi_get_value_int32(env, my_count, &count));
  uv_mutex_lock(&data->mutex);
  ++data->global_count;
  uv_mutex_unlock(&data->mutex);
}

static node_embedding_status ConfigureRuntime(
    void* cb_data,
    node_embedding_platform platform,
    node_embedding_runtime_config runtime_config) {
  // Inspector can be associated with only one
  // runtime in the process.
  NODE_EMBEDDED_CALL(node_embedding_runtime_config_set_flags(
      runtime_config,
      node_embedding_runtime_flags_default |
          node_embedding_runtime_flags_no_create_inspector));
  NODE_EMBEDDED_CALL(LoadUtf8Script(runtime_config, main_script));
  NODE_EMBEDDED_CALL(node_embedding_runtime_config_on_loaded(
      runtime_config, HandleExecutionResult, cb_data, NULL));
  return node_embedding_status_ok;
}

static void ThreadCallback(void* arg) {
  thread_data* data = (thread_data*)arg;
  node_embedding_status status =
      node_embedding_runtime_run(data->platform, ConfigureRuntime, arg);
  if (status != node_embedding_status_ok) {
    uv_mutex_lock(&data->mutex);
    data->global_status = status;
    uv_mutex_unlock(&data->mutex);
  }
}

// Tests that multiple runtimes can be run at the same time in their own
// threads. The test creates 12 threads and 12 runtimes. Each runtime runs in it
// own thread.
int32_t test_main_c_api_threading_runtime_per_thread(int32_t argc,
                                                     char* argv[]) {
  size_t thread_count = 12;
  uv_thread_t threads[12] = {0};
  thread_data data = {0};
  uv_mutex_init(&data.mutex);

  int32_t global_count = 0;
  node_embedding_status global_status = node_embedding_status_ok;

  node_embedding_platform platform;
  CHECK_EXPECTED_OR_EXIT(
      argv[0],
      node_embedding_platform_create(
          NODE_EMBEDDING_VERSION, argc, argv, NULL, NULL, &platform));
  if (platform == NULL) {
    return 0;  // early return
  }

  for (size_t i = 0; i < thread_count; i++) {
    uv_thread_create(&threads[i], ThreadCallback, &data);
  }

  for (size_t i = 0; i < thread_count; i++) {
    uv_thread_join(&threads[i]);
  }

  // TODO: Add passing error message
  CHECK_EXPECTED_OR_EXIT(argv[0], global_status);
  CHECK_EXPECTED_OR_EXIT(argv[0], node_embedding_platform_delete(platform));

  fprintf(stdout, "%d\n", global_count);
  return 0;
}

node_embedding_status ConfigureRuntime2(
    void* cb_data,
    node_embedding_platform platform,
    node_embedding_runtime_config runtime_config) {
  // Inspector can be associated with only one runtime in the process.
  NODE_EMBEDDED_CALL(node_embedding_runtime_config_set_flags(
      runtime_config,
      node_embedding_runtime_flags_default |
          node_embedding_runtime_flags_no_create_inspector));
  NODE_EMBEDDED_CALL(LoadUtf8Script(runtime_config, main_script));
  return node_embedding_status_ok;
}

void IncMyCount(void* cb_data, napi_env env) {
  napi_value undefined, global, func;
  NODE_API_CALL_RETURN_VOID(napi_get_undefined(env, &undefined));
  NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
  NODE_API_CALL_RETURN_VOID(
      napi_get_named_property(env, global, "incMyCount", &func));

  napi_valuetype func_type;
  NODE_API_CALL_RETURN_VOID(napi_typeof(env, func, &func_type));
  NODE_API_ASSERT_RETURN_VOID(func_type == napi_function);
  NODE_API_CALL_RETURN_VOID(
      napi_call_function(env, undefined, func, 0, NULL, NULL));
}

void SumMyCount(void* cb_data, napi_env env) {
  int32_t* global_count = (int32_t*)cb_data;
  napi_value global, my_count;
  NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
  NODE_API_CALL_RETURN_VOID(
      napi_get_named_property(env, global, "myCount", &my_count));

  napi_valuetype my_count_type;
  NODE_API_CALL_RETURN_VOID(napi_typeof(env, my_count, &my_count_type));
  NODE_API_ASSERT_RETURN_VOID(my_count_type == napi_number);
  int32_t count;
  NODE_API_CALL_RETURN_VOID(napi_get_value_int32(env, my_count, &count));

  *global_count += count;
}

// Tests that multiple runtimes can run in the same thread.
// The runtime scope must be opened and closed for each use.
// There are 12 runtimes that share the same main thread.
int32_t test_main_c_api_threading_several_runtimes_per_thread(int32_t argc,
                                                              char* argv[]) {
  const size_t runtime_count = 12;
  bool more_work = false;
  int32_t global_count = 0;
  node_embedding_runtime runtimes[12] = {0};

  node_embedding_platform platform;
  CHECK_EXPECTED_OR_EXIT(
      argv[0],
      node_embedding_platform_create(
          NODE_EMBEDDING_VERSION, argc, argv, NULL, NULL, &platform));
  if (platform == NULL) {
    return 0;  // early return
  }

  for (size_t i = 0; i < runtime_count; ++i) {
    CHECK_EXPECTED_OR_EXIT(
        argv[0],
        node_embedding_runtime_create(
            platform, ConfigureRuntime2, NULL, &runtimes[i]));

    CHECK_EXPECTED_OR_EXIT(
        argv[0],
        node_embedding_runtime_node_api_run(runtimes[i], IncMyCount, NULL));
  }

  do {
    more_work = false;
    // for (const NodeRuntime& runtime : runtimes) {
    //  TODO: implement
    //  NodeExpected<bool> has_more_work = runtime.RunEventLoopNoWait();
    //  CHECK_EXPECTED_OR_EXIT(argv[0], has_more_work);
    //  more_work |= has_more_work.value();
    //}
  } while (more_work);

  for (size_t i = 0; i < runtime_count; ++i) {
    CHECK_EXPECTED_OR_EXIT(argv[0],
                           node_embedding_runtime_node_api_run(
                               runtimes[i], SumMyCount, &global_count));
    CHECK_EXPECTED_OR_EXIT(argv[0],
                           node_embedding_runtime_event_loop_run(runtimes[i]));
    CHECK_EXPECTED_OR_EXIT(argv[0], node_embedding_runtime_delete(runtimes[i]));
  }

  CHECK_EXPECTED_OR_EXIT(argv[0], node_embedding_platform_delete(platform));

  fprintf(stdout, "%d\n", global_count);
  return 0;
}

typedef struct {
  node_embedding_runtime runtime;
  uv_mutex_t mutex;
  int32_t result_count;
  node_embedding_status result_status;
} thread_data3;

node_embedding_status ConfigureRuntime3(
    void* cb_data,
    node_embedding_platform platform,
    node_embedding_runtime_config runtime_config) {
  NODE_EMBEDDED_CALL(LoadUtf8Script(runtime_config, main_script));
  return node_embedding_status_ok;
}

void RunNodeApi3(void* cb_data, napi_env env) {
  thread_data3* data = (thread_data3*)cb_data;
  napi_value undefined, global, func, my_count;
  NODE_API_CALL_RETURN_VOID(napi_get_undefined(env, &undefined));
  NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
  NODE_API_CALL_RETURN_VOID(
      napi_get_named_property(env, global, "incMyCount", &func));

  napi_valuetype func_type;
  NODE_API_CALL_RETURN_VOID(napi_typeof(env, func, &func_type));
  NODE_API_ASSERT_RETURN_VOID(func_type == napi_function);
  NODE_API_CALL_RETURN_VOID(
      napi_call_function(env, undefined, func, 0, NULL, NULL));

  NODE_API_CALL_RETURN_VOID(
      napi_get_named_property(env, global, "myCount", &my_count));
  napi_valuetype count_type;
  NODE_API_CALL_RETURN_VOID(napi_typeof(env, my_count, &count_type));
  NODE_API_ASSERT_RETURN_VOID(count_type == napi_number);
  int32_t count;
  NODE_API_CALL_RETURN_VOID(napi_get_value_int32(env, my_count, &count));
  data->result_count = count;
}

void ThreadCallback3(void* arg) {
  thread_data3* data = (thread_data3*)arg;
  uv_mutex_lock(&data->mutex);
  node_embedding_status status =
      node_embedding_runtime_node_api_run(data->runtime, RunNodeApi3, arg);
  if (status != node_embedding_status_ok) {
    data->result_status = status;
  }
  uv_mutex_unlock(&data->mutex);
}

// Tests that a runtime can be invoked from different threads as long as only
// one thread uses it at a time.
int32_t test_main_c_api_threading_runtime_in_several_threads(int32_t argc,
                                                             char* argv[]) {
  // Use mutex to synchronize access to the runtime.
  thread_data3 data = {0};
  uv_mutex_init(&data.mutex);

  const size_t thread_count = 5;
  uv_thread_t threads[5] = {0};

  node_embedding_platform platform;
  CHECK_EXPECTED_OR_EXIT(
      argv[0],
      node_embedding_platform_create(
          NODE_EMBEDDING_VERSION, argc, argv, NULL, NULL, &platform));
  if (platform == NULL) {
    return 0;  // early return
  }

  node_embedding_runtime runtime;
  CHECK_EXPECTED_OR_EXIT(argv[0],
                         node_embedding_runtime_create(
                             platform, ConfigureRuntime3, NULL, &runtime));

  for (size_t i = 0; i < thread_count; ++i) {
    uv_thread_create(&threads[i], ThreadCallback3, &data);
  }

  for (size_t i = 0; i < thread_count; ++i) {
    uv_thread_join(&threads[i]);
  }

  CHECK_EXPECTED_OR_EXIT(argv[0], data.result_status);
  CHECK_EXPECTED_OR_EXIT(argv[0],
                         node_embedding_runtime_event_loop_run(runtime));

  fprintf(stdout, "%d\n", data.result_count);
  return 0;
}

struct task_t {
  struct task_t* prev;
  struct task_t* next;
  void* task_data;
  void (*run_task)(void*);
  void (*release_task_data)(void*);
};
typedef struct task_t task_t;

typedef struct {
  uv_mutex_t mutex;
  uv_cond_t wakeup;
  task_t* queue_in;
  task_t* queue_out;
  bool is_finished;
} ui_queue_t;

// TODO: Change implementation from doubly linked list to using read/write
// buffer.
static void ui_queue_init(ui_queue_t* queue) {
  uv_mutex_init(&queue->mutex);
  uv_cond_init(&queue->wakeup);
  queue->queue_in = NULL;
  queue->queue_out = NULL;
  queue->is_finished = false;
}

static void ui_queue_post_task(ui_queue_t* queue, task_t* task) {
  uv_mutex_lock(&queue->mutex);
  if (!queue->is_finished) {
    if (queue->queue_in == NULL) {
      queue->queue_out = task;
    } else {
      queue->queue_in->prev = task;
    }
    task->prev = NULL;
    task->next = queue->queue_in;
    queue->queue_in = task;
    uv_cond_signal(&queue->wakeup);
  }
  uv_mutex_unlock(&queue->mutex);
}

static void ui_queue_run(ui_queue_t* queue) {
  for (;;) {
    task_t* task;
    uv_mutex_lock(&queue->mutex);
    while (queue->queue_out == NULL && !queue->is_finished) {
      uv_cond_wait(&queue->wakeup, &queue->mutex);
    }
    if (queue->is_finished) {
      uv_mutex_unlock(&queue->mutex);
      break;
    }
    task = queue->queue_out;
    if (task != NULL) {
      queue->queue_out = task->prev;
      if (queue->queue_out == NULL) {
        queue->queue_in = NULL;
      } else {
        queue->queue_out->next = NULL;
      }
    }
    uv_mutex_unlock(&queue->mutex);
    if (task != NULL) {
      task->prev = NULL;
      task->next = NULL;
      if (task->run_task != NULL) {
        task->run_task(task->task_data);
      }
      if (task->release_task_data != NULL) {
        task->release_task_data(task->task_data);
      }
    }
  }
}

static void ui_queue_stop(ui_queue_t* queue) {
  uv_mutex_lock(&queue->mutex);
  if (!queue->is_finished) {
    queue->is_finished = true;
    uv_cond_signal(&queue->wakeup);
  }
  uv_mutex_unlock(&queue->mutex);
}

static void ui_queue_destroy(ui_queue_t* queue) {
  uv_mutex_destroy(&queue->mutex);
  uv_cond_destroy(&queue->wakeup);
}

typedef struct {
  ui_queue_t ui_queue;
  node_embedding_runtime runtime;
} test_data4_t;

typedef struct {
  task_t parent_task;
  node_embedding_task_run_callback run_task;
  void* task_data;
  node_embedding_data_release_callback release_task_data;
  test_data4_t* test_data;
} test_task_t;

void RunTestTask(void* cb_data) {
  test_task_t* test_task = (test_task_t*)cb_data;
  test_task->run_task(test_task->task_data);  // TODO: handle result

  // Check myCount and stop the processing when it reaches 5.
  int32_t count;
  node_embedding_runtime runtime = test_task->test_data->runtime;
  node_embedding_node_api_scope node_api_scope;
  napi_env env;
  NODE_EMBEDDED_CALL(node_embedding_runtime_node_api_scope_open(
      runtime, &node_api_scope, &env));
  napi_value global, my_count;
  NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
  NODE_API_CALL_RETURN_VOID(
      napi_get_named_property(env, global, "myCount", &my_count));
  napi_valuetype count_type;
  NODE_API_CALL_RETURN_VOID(napi_typeof(env, my_count, &count_type));
  NODE_API_ASSERT_RETURN_VOID(count_type == napi_number);
  NODE_API_CALL_RETURN_VOID(napi_get_value_int32(env, my_count, &count));
  NODE_EMBEDDED_CALL(
      node_embedding_runtime_node_api_scope_close(runtime, node_api_scope));
  if (count == 5) {
    NODE_EMBEDDED_CALL(node_embedding_runtime_event_loop_run(runtime));
    fprintf(stdout, "%d\n", count);
    ui_queue_stop(&test_task->test_data->ui_queue);
  }
}

void ReleaseTestTask(void* cb_data) {
  test_task_t* test_task = (test_task_t*)cb_data;
  if (test_task->release_task_data != NULL) {
    test_task->release_task_data(test_task->task_data);
  }
  free(test_task);
}

static node_embedding_status PostTask(
    void* cb_data,
    node_embedding_task_run_callback run_task,
    void* task_data,
    node_embedding_data_release_callback release_task_data,
    bool* succeeded) {
  test_data4_t* test_data = (test_data4_t*)cb_data;
  test_task_t* test_task = (test_task_t*)malloc(sizeof(test_task_t));
  if (test_task == NULL) {
    return node_embedding_status_out_of_memory;
  }
  memset(test_task, 0, sizeof(test_task_t));
  test_task->parent_task.run_task = RunTestTask;
  test_task->parent_task.task_data = test_task;
  test_task->parent_task.release_task_data = ReleaseTestTask;
  test_task->run_task = run_task;
  test_task->task_data = task_data;
  test_task->release_task_data = release_task_data;
  test_task->test_data = test_data;

  ui_queue_post_task(&test_data->ui_queue, test_task);
  return node_embedding_status_ok;
}

static node_embedding_status ConfigureRuntime4(
    void* cb_data,
    node_embedding_platform platform,
    node_embedding_runtime_config runtime_config) {
  // The callback will be invoked from the runtime's event loop
  // observer thread. It must schedule the work to the UI thread's
  // event loop.
  NODE_EMBEDDED_CALL(node_embedding_runtime_config_set_task_runner(
      runtime_config, PostTask, cb_data, NULL));

  NODE_EMBEDDED_CALL(LoadUtf8Script(runtime_config, main_script));
  return node_embedding_status_ok;
}

static void StartProcessing(void* cb_data) {
  test_data4_t* data = (test_data4_t*)cb_data;
  node_embedding_node_api_scope node_api_scope;
  napi_env env;
  node_embedding_runtime_node_api_scope_open(
      data->runtime, &node_api_scope, &env);
  napi_value undefined, global, func;
  NODE_API_CALL_RETURN_VOID(napi_get_undefined(env, &undefined));
  NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
  NODE_API_CALL_RETURN_VOID(
      napi_get_named_property(env, global, "incMyCount", &func));

  napi_valuetype func_type;
  NODE_API_CALL_RETURN_VOID(napi_typeof(env, func, &func_type));
  NODE_API_ASSERT_RETURN_VOID(func_type == napi_function);
  NODE_API_CALL_RETURN_VOID(
      napi_call_function(env, undefined, func, 0, NULL, NULL));
  node_embedding_runtime_node_api_scope_close(data->runtime, node_api_scope);
}

// Tests that a the runtime's event loop can be called from the UI thread
// event loop.
int32_t test_main_c_api_threading_runtime_in_ui_thread(int32_t argc,
                                                       char* argv[]) {
  // A simulation of the UI thread's event loop implemented as a dispatcher
  // queue. Note that it is a very simplistic implementation not suitable
  // for the real apps.
  test_data4_t data = {0};
  ui_queue_init(&data.ui_queue);

  node_embedding_platform platform;
  CHECK_EXPECTED_OR_EXIT(
      argv[0],
      node_embedding_platform_create(
          NODE_EMBEDDING_VERSION, argc, argv, NULL, NULL, &platform));
  if (platform == NULL) {
    return 0;  // early return
  }

  CHECK_EXPECTED_OR_EXIT(
      argv[0],
      node_embedding_runtime_create(
          platform, ConfigureRuntime4, &data, &data.runtime));

  // The initial task starts the JS code that then will do the timer
  // scheduling. The timer supposed to be handled by the runtime's event loop.
  task_t task = {0};
  task.run_task = StartProcessing;
  ui_queue_post_task(&data.ui_queue, &task);

  ui_queue_run(&data.ui_queue);
  ui_queue_destroy(&data.ui_queue);

  CHECK_EXPECTED_OR_EXIT(argv[0], node_embedding_runtime_delete(data.runtime));
  CHECK_EXPECTED_OR_EXIT(argv[0], node_embedding_platform_delete(platform));
  return 0;
}
