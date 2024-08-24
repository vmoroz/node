#include <algorithm>
#include <climits>  // INT_MAX
#include <cmath>
#define NAPI_EXPERIMENTAL
#include "env-inl.h"
#include "js_native_api.h"
#include "js_native_api_v8.h"
#include "node_api_embedding.h"
#include "node_api_internals.h"
#include "simdutf.h"
#include "util-inl.h"

namespace node {

// Declare functions implemented in embed_helpers.cc
v8::Maybe<ExitCode> SpinEventLoopWithoutCleanup(Environment* env);
v8::Maybe<ExitCode> SpinEventLoopWithoutCleanup(
    Environment* env, const std::function<bool(void)>& shouldContinue);

}  // end of namespace node

namespace v8impl {
namespace {

struct IsolateLocker {
  IsolateLocker(node::CommonEnvironmentSetup* env_setup)
      : v8_locker_(env_setup->isolate()),
        isolate_scope_(env_setup->isolate()),
        handle_scope_(env_setup->isolate()),
        context_scope_(env_setup->context()) {}

 private:
  v8::Locker v8_locker_;
  v8::Isolate::Scope isolate_scope_;
  v8::HandleScope handle_scope_;
  v8::Context::Scope context_scope_;
};

class EmbeddedEnvironment : public node_napi_env__ {
 public:
  explicit EmbeddedEnvironment(
      std::unique_ptr<node::CommonEnvironmentSetup>&& env_setup,
      v8::Local<v8::Context> context,
      const std::string& module_filename,
      int32_t module_api_version)
      : node_napi_env__(context, module_filename, module_api_version),
        env_setup_(std::move(env_setup)) {}

  node::CommonEnvironmentSetup* env_setup() { return env_setup_.get(); }

  std::unique_ptr<node::CommonEnvironmentSetup> ResetEnvSetup() {
    return std::move(env_setup_);
  }

  napi_status OpenScope() {
    if (isolate_locker_.has_value()) return napi_generic_failure;
    isolate_locker_.emplace(env_setup_.get());
    return napi_ok;
  }

  napi_status CloseScope() {
    if (!isolate_locker_.has_value()) return napi_generic_failure;
    isolate_locker_.reset();
    return napi_ok;
  }

  bool IsScopeOpened() const { return isolate_locker_.has_value(); }

 private:
  std::unique_ptr<node::CommonEnvironmentSetup> env_setup_;
  std::optional<IsolateLocker> isolate_locker_;
};

}  // end of anonymous namespace
}  // end of namespace v8impl

napi_status NAPI_CDECL
node_api_create_platform(int argc,
                         char** argv,
                         node_api_error_message_handler err_handler,
                         node_api_platform* result) {
  argv = uv_setup_args(argc, argv);
  std::vector<std::string> args(argv, argv + argc);
  if (args.size() < 1) args.push_back("libnode");

  std::shared_ptr<node::InitializationResult> node_platform =
      node::InitializeOncePerProcess(
          args,
          {node::ProcessInitializationFlags::kDisableNodeOptionsEnv,
           node::ProcessInitializationFlags::kNoInitializeV8,
           node::ProcessInitializationFlags::kNoInitializeNodeV8Platform});

  if (err_handler != nullptr) {
    for (const std::string& error : node_platform->errors()) {
      err_handler(error.c_str());
    }
  } else {
    for (const std::string& error : node_platform->errors()) {
      fprintf(stderr, "%s\n", error.c_str());
    }
  }

  if (node_platform->early_return() != 0) {
    return napi_generic_failure;
  }

  int thread_pool_size =
      static_cast<int>(node::per_process::cli_options->v8_thread_pool_size);
  std::unique_ptr<node::MultiIsolatePlatform> v8_platform =
      node::MultiIsolatePlatform::Create(thread_pool_size);
  v8::V8::InitializePlatform(v8_platform.get());
  v8::V8::Initialize();
  static_cast<node::InitializationResultImpl*>(node_platform.get())->platform_ =
      v8_platform.release();
  *result = reinterpret_cast<node_api_platform>(
      new std::shared_ptr<node::InitializationResult>(
          std::move(node_platform)));
  return napi_ok;
}

napi_status NAPI_CDECL node_api_destroy_platform(node_api_platform platform) {
  std::unique_ptr<std::shared_ptr<node::InitializationResult>> wrapper{
      reinterpret_cast<std::shared_ptr<node::InitializationResult>*>(platform)};
  std::unique_ptr<node::MultiIsolatePlatform> v8_platform{
      static_cast<node::InitializationResultImpl*>(wrapper->get())->platform_};
  v8::V8::Dispose();
  v8::V8::DisposePlatform();
  node::TearDownOncePerProcess();
  return napi_ok;
}

napi_status NAPI_CDECL
node_api_create_environment(node_api_platform platform,
                            node_api_error_message_handler err_handler,
                            const char* main_script,
                            int32_t api_version,
                            napi_env* result) {
  CHECK_ENV(platform);
  CHECK_ENV(result);

  std::shared_ptr<node::InitializationResult> wrapper =
      *reinterpret_cast<std::shared_ptr<node::InitializationResult>*>(platform);
  std::vector<std::string> errors_vec;

  std::unique_ptr<node::CommonEnvironmentSetup> env_setup =
      node::CommonEnvironmentSetup::Create(
          wrapper->platform(),
          &errors_vec,
          wrapper->args(),
          wrapper->exec_args(),
          static_cast<node::EnvironmentFlags::Flags>(
              node::EnvironmentFlags::kDefaultFlags |
              node::EnvironmentFlags::kNoCreateInspector));

  for (const std::string& error : errors_vec) {
    if (err_handler != nullptr) {
      err_handler(error.c_str());
    } else {
      fprintf(stderr, "%s\n", error.c_str());
    }
  }
  if (env_setup == nullptr) {
    return napi_generic_failure;
  }

  std::string filename =
      wrapper->args().size() > 1 ? wrapper->args()[1] : "<internal>";
  node::CommonEnvironmentSetup* env_setup_ptr = env_setup.get();

  v8impl::IsolateLocker isolate_locker(env_setup_ptr);
  v8impl::EmbeddedEnvironment* embedded_env = new v8impl::EmbeddedEnvironment(
      std::move(env_setup), env_setup_ptr->context(), filename, api_version);

  embedded_env->node_env()->AddCleanupHook(
      [](void* arg) { static_cast<napi_env>(arg)->Unref(); },
      static_cast<void*>(embedded_env));
  *result = embedded_env;

  node::Environment* node_env = env_setup_ptr->env();

  v8::MaybeLocal<v8::Value> ret =
      node::LoadEnvironment(node_env, std::string_view(main_script));

  if (ret.IsEmpty()) return napi_pending_exception;

  return napi_ok;
}

napi_status NAPI_CDECL node_api_destroy_environment(napi_env env,
                                                    int* exit_code) {
  CHECK_ENV(env);
  v8impl::EmbeddedEnvironment* embedded_env =
      reinterpret_cast<v8impl::EmbeddedEnvironment*>(env);

  if (embedded_env->IsScopeOpened()) return napi_generic_failure;

  {
    v8impl::IsolateLocker isolate_locker(embedded_env->env_setup());

    int ret = node::SpinEventLoop(embedded_env->node_env()).FromMaybe(1);
    if (exit_code != nullptr) *exit_code = ret;
  }

  std::unique_ptr<node::CommonEnvironmentSetup> env_setup =
      embedded_env->ResetEnvSetup();
  node::Stop(embedded_env->node_env());

  return napi_ok;
}

napi_status NAPI_CDECL node_api_open_environment_scope(napi_env env) {
  CHECK_ENV(env);
  v8impl::EmbeddedEnvironment* embedded_env =
      reinterpret_cast<v8impl::EmbeddedEnvironment*>(env);

  return embedded_env->OpenScope();
}

napi_status NAPI_CDECL node_api_close_environment_scope(napi_env env) {
  CHECK_ENV(env);
  v8impl::EmbeddedEnvironment* embedded_env =
      reinterpret_cast<v8impl::EmbeddedEnvironment*>(env);

  return embedded_env->CloseScope();
}

napi_status NAPI_CDECL node_api_run_environment(napi_env env) {
  CHECK_ENV(env);
  v8impl::EmbeddedEnvironment* embedded_env =
      reinterpret_cast<v8impl::EmbeddedEnvironment*>(env);

  if (node::SpinEventLoopWithoutCleanup(embedded_env->node_env()).IsNothing()) {
    return napi_closing;
  }

  return napi_ok;
}

napi_status NAPI_CDECL
node_api_run_environment_if(napi_env env,
                            node_api_run_predicate predicate,
                            void* predicate_data,
                            bool* has_more_work) {
  CHECK_ENV(env);
  CHECK_ARG(env, predicate);
  v8impl::EmbeddedEnvironment* embedded_env =
      reinterpret_cast<v8impl::EmbeddedEnvironment*>(env);

  if (node::SpinEventLoopWithoutCleanup(
          embedded_env->node_env(),
          [predicate, predicate_data]() { return predicate(predicate_data); })
          .IsNothing()) {
    return napi_closing;
  }

  if (has_more_work != nullptr) {
    *has_more_work = uv_loop_alive(embedded_env->node_env()->event_loop());
  }

  return napi_ok;
}

static void node_api_promise_error_handler(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  return;
}

napi_status NAPI_CDECL node_api_await_promise(napi_env env,
                                              napi_value promise,
                                              napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8impl::EmbeddedEnvironment* embedded_env =
      reinterpret_cast<v8impl::EmbeddedEnvironment*>(env);

  v8::EscapableHandleScope scope(env->isolate);

  v8::Local<v8::Value> promise_value = v8impl::V8LocalValueFromJsValue(promise);
  if (promise_value.IsEmpty() || !promise_value->IsPromise())
    return napi_invalid_arg;
  v8::Local<v8::Promise> promise_object = promise_value.As<v8::Promise>();

  v8::Local<v8::Value> rejected = v8::Boolean::New(env->isolate, false);
  v8::Local<v8::Function> err_handler =
      v8::Function::New(
          env->context(), node_api_promise_error_handler, rejected)
          .ToLocalChecked();

  if (promise_object->Catch(env->context(), err_handler).IsEmpty())
    return napi_pending_exception;

  if (node::SpinEventLoopWithoutCleanup(
          embedded_env->node_env(),
          [&promise_object]() {
            return promise_object->State() ==
                   v8::Promise::PromiseState::kPending;
          })
          .IsNothing())
    return napi_closing;

  *result =
      v8impl::JsValueFromV8LocalValue(scope.Escape(promise_object->Result()));
  if (promise_object->State() == v8::Promise::PromiseState::kRejected)
    return napi_pending_exception;

  return napi_ok;
}
