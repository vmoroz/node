#define NAPI_EXPERIMENTAL
#include "node_api_embedtest.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <optional>

static int RunNodeInstance(node_api_platform platform,
                           const std::vector<std::string>& args,
                           const std::vector<std::string>& exec_args);

extern "C" int test_main_snapshot_node_api(int argc, char** argv) {
  std::vector<std::string> args(argv, argv + argc);
  int exit_code = 0;

  node_api_platform platform;
  std::vector<std::string> errors;
  napi_status status = node_api_create_platform(
      argc, argv, &exit_code, GetStringVector, &errors, &platform);
  if (status != napi_ok) {
    for (const std::string& err : errors)
      fprintf(stderr, "%s: %s\n", args[0].c_str(), err.c_str());
    return exit_code;
  }

  std::vector<std::string> parsed_args, exec_args;
  CHECK(node_api_get_platform_args(platform, GetStringVector, &parsed_args));
  CHECK(node_api_get_platform_exec_args(platform, GetStringVector, &exec_args));
  CHECK_EXIT_CODE(RunNodeInstance(platform, parsed_args, exec_args));

  CHECK(node_api_destroy_platform(platform));
  return 0;
}

int RunNodeInstance(node_api_platform platform,
                    const std::vector<std::string>& args,
                    const std::vector<std::string>& exec_args) {
  //   napi_env env;
  //   CHECK(node_api_create_environment(
  //       platform, NULL, main_script, NAPI_VERSION, &env));

  //   CHECK_EXIT_CODE(callMe(env));
  //   CHECK_EXIT_CODE(waitMe(env));
  //   CHECK_EXIT_CODE(waitMeWithCheese(env));

  //   int exit_code;
  //   CHECK(node_api_destroy_environment(env, &exit_code));

  //   return exit_code;
  // }

  int exit_code = 0;

  // Format of the arguments of this binary:
  // Building snapshot:
  // embedtest js_code_to_eval arg1 arg2...
  //           --embedder-snapshot-blob blob-path
  //           --embedder-snapshot-create
  //           [--embedder-snapshot-as-file]
  //           [--without-code-cache]
  // Running snapshot:
  // embedtest --embedder-snapshot-blob blob-path
  //           [--embedder-snapshot-as-file]
  //           arg1 arg2...
  // No snapshot:
  // embedtest arg1 arg2...

  // TODO: node::EmbedderSnapshotData::Pointer snapshot;

  std::string binary_path = args[0];
  std::vector<std::string> filtered_args;
  bool is_building_snapshot = false;
  bool snapshot_as_file = false;
  // TODO: std::optional<node::SnapshotConfig> snapshot_config;
  std::string snapshot_blob_path;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (arg == "--embedder-snapshot-create") {
      is_building_snapshot = true;
    } else if (arg == "--embedder-snapshot-as-file") {
      snapshot_as_file = true;
    } else if (arg == "--without-code-cache") {
      // TODO: Add environment flag kSnapshotWithoutCodeCache
      // TODO: if (!snapshot_config.has_value()) {
      //   snapshot_config = node::SnapshotConfig{};
      // }
      // snapshot_config.value().flags = static_cast<node::SnapshotFlags>(
      //     static_cast<uint32_t>(snapshot_config.value().flags) |
      //     static_cast<uint32_t>(node::SnapshotFlags::kWithoutCodeCache));
    } else if (arg == "--embedder-snapshot-blob") {
      assert(i + 1 < args.size());
      snapshot_blob_path = args[i + 1];
      i++;
    } else {
      filtered_args.push_back(arg);
    }
  }

  if (!snapshot_blob_path.empty() && !is_building_snapshot) {
    // TODO: pass snapshot to env creation
    FILE* fp = fopen(snapshot_blob_path.c_str(), "rb");
    assert(fp != nullptr);
    if (snapshot_as_file) {
      // TODO: snapshot = node::EmbedderSnapshotData::FromFile(fp);
    } else {
      uv_fs_t req = uv_fs_t();
      int statret =
          uv_fs_stat(nullptr, &req, snapshot_blob_path.c_str(), nullptr);
      assert(statret == 0);
      size_t filesize = req.statbuf.st_size;
      uv_fs_req_cleanup(&req);

      std::vector<char> vec(filesize);
      size_t read = fread(vec.data(), filesize, 1, fp);
      assert(read == 1);
      // TODO: snapshot = node::EmbedderSnapshotData::FromBlob(vec);
    }
    assert(snapshot);
    int ret = fclose(fp);
    assert(ret == 0);
  }

  if (is_building_snapshot) {
    // It contains at least the binary path, the code to snapshot,
    // and --embedder-snapshot-create (which is filtered, so at least
    // 2 arguments should remain after filtering).
    assert(filtered_args.size() >= 2);
    // Insert an anonymous filename as process.argv[1].
    // TODO: filtered_args.insert(filtered_args.begin() + 1,
    //                     node::GetAnonymousMainPath());
  }

  // std::vector<std::string> errors;
  // std::unique_ptr<CommonEnvironmentSetup> setup;

  // if (snapshot) {
  //   setup = CommonEnvironmentSetup::CreateFromSnapshot(
  //       platform, &errors, snapshot.get(), filtered_args, exec_args);
  // } else if (is_building_snapshot) {
  //   if (snapshot_config.has_value()) {
  //     setup = CommonEnvironmentSetup::CreateForSnapshotting(
  //         platform, &errors, filtered_args, exec_args,
  //         snapshot_config.value());
  //   } else {
  //     setup = CommonEnvironmentSetup::CreateForSnapshotting(
  //         platform, &errors, filtered_args, exec_args);
  //   }
  // } else {
  //   setup = CommonEnvironmentSetup::Create(
  //       platform, &errors, filtered_args, exec_args);
  // }
  // if (!setup) {
  //   for (const std::string& err : errors)
  //     fprintf(stderr, "%s: %s\n", binary_path.c_str(), err.c_str());
  //   return 1;
  // }

  // Isolate* isolate = setup->isolate();
  // Environment* env = setup->env();

  // {
  //   Locker locker(isolate);
  //   Isolate::Scope isolate_scope(isolate);
  //   HandleScope handle_scope(isolate);
  //   Context::Scope context_scope(setup->context());

  //   MaybeLocal<Value> loadenv_ret;
  //   if (snapshot) {  // Deserializing snapshot
  //     loadenv_ret = node::LoadEnvironment(env,
  //     node::StartExecutionCallback{});
  //   } else if (is_building_snapshot) {
  //     // Environment created for snapshotting must set process.argv[1] to
  //     // the name of the main script, which was inserted above.
  //     loadenv_ret = node::LoadEnvironment(
  //         env,
  //         "const assert = require('assert');"
  //         "assert(require('v8').startupSnapshot.isBuildingSnapshot());"
  //         "globalThis.embedVars = { nön_ascıı: '🏳️‍🌈' };"
  //         "globalThis.require = require;"
  //         "require('vm').runInThisContext(process.argv[2]);");
  //   } else {
  //     loadenv_ret = node::LoadEnvironment(
  //         env,
  //         "const publicRequire =
  //         require('module').createRequire(process.cwd() "
  //         "+ '/');"
  //         "globalThis.require = publicRequire;"
  //         "globalThis.embedVars = { nön_ascıı: '🏳️‍🌈' };"
  //         "require('vm').runInThisContext(process.argv[1]);");
  //   }

  //   if (loadenv_ret.IsEmpty())  // There has been a JS exception.
  //     return 1;

  //   exit_code = node::SpinEventLoop(env).FromMaybe(1);
  // }

  // if (!snapshot_blob_path.empty() && is_building_snapshot) {
  //   snapshot = setup->CreateSnapshot();
  //   assert(snapshot);

  //   FILE* fp = fopen(snapshot_blob_path.c_str(), "wb");
  //   assert(fp != nullptr);
  //   if (snapshot_as_file) {
  //     snapshot->ToFile(fp);
  //   } else {
  //     const std::vector<char> vec = snapshot->ToBlob();
  //     size_t written = fwrite(vec.data(), vec.size(), 1, fp);
  //     assert(written == 1);
  //   }
  //   int ret = fclose(fp);
  //   assert(ret == 0);
  // }

  // node::Stop(env);

  return exit_code;
}
