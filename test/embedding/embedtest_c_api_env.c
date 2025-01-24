#include "embedtest_c_api_common.h"

static node_embedding_status ConfigureRuntimeNoBrowserGlobals(
    void* cb_data,
    node_embedding_platform platform,
    node_embedding_runtime_config runtime_config) {
  NODE_EMBEDDED_CALL(node_embedding_runtime_config_set_flags(
      runtime_config, node_embedding_runtime_flags_no_browser_globals));
  NODE_EMBEDDED_CALL(LoadUtf8Script(
      runtime_config,
      "const assert = require('assert');\n"
      "const path = require('path');\n"
      "const relativeRequire =\n"
      "  require('module').createRequire(path.join(process.cwd(), "
      "'stub.js'));\n"
      "const { intrinsics, nodeGlobals } =\n"
      "  relativeRequire('./test/common/globals');\n"
      "const items = Object.getOwnPropertyNames(globalThis);\n"
      "const leaks = [];\n"
      "for (const item of items) {\n "
      "  if (intrinsics.has(item)) {\n"
      "    continue;\n"
      "  }\n"
      "  if (nodeGlobals.has(item)) {\n"
      "    continue;\n"
      "  }\n"
      "  if (item === '$jsDebugIsRegistered') {\n"
      "    continue;\n"
      "  }\n"
      "  leaks.push(item);\n"
      "}\n"
      "assert.deepStrictEqual(leaks, []);\n"));

  return node_embedding_status_ok;
}

// Test the no_browser_globals option.
int32_t test_main_c_api_env_no_browser_globals(int32_t argc, char* argv[]) {
  CHECK_EXPECTED_OR_EXIT(
      argv[0],
      node_embedding_main_run(NODE_EMBEDDING_VERSION,
                              argc,
                              argv,
                              NULL,
                              NULL,
                              ConfigureRuntimeNoBrowserGlobals,
                              NULL));
  return 0;
}

static node_embedding_status ConfigureRuntimeWithEsmLoader(
    void* cb_data,
    node_embedding_platform platform,
    node_embedding_runtime_config runtime_config) {
  NODE_EMBEDDED_CALL(LoadUtf8Script(
      runtime_config,
      "globalThis.require = "
      "require('module').createRequire(process.execPath);\n"
      "const { SourceTextModule } = require('node:vm');\n"
      "(async () => {\n"
      "  const stmString = 'globalThis.importResult = import(\"\")';\n"
      "  const m = new SourceTextModule(stmString, {\n"
      "    importModuleDynamically: (async () => {\n"
      "      const m = new SourceTextModule('');\n"
      "      await m.link(() => 0);\n"
      "      await m.evaluate();\n"
      "      return m.namespace;\n"
      "    }),\n"
      "  });\n"
      "  await m.link(() => 0);\n"
      "  await m.evaluate();\n"
      "  delete globalThis.importResult;\n"
      "  process.exit(0);\n"
      "})();\n"));

  return node_embedding_status_ok;
}

// Test ESM loaded
int32_t test_main_c_api_env_with_esm_loader(int32_t argc, char* argv[]) {
  // We currently cannot pass argument to command line arguments to the runtime.
  // They must be parsed by the platform.
  char* argv2[64];
  for (int32_t i = 0; i < argc; ++i) {
    argv2[i] = argv[i];
  }
  argv2[argc] = "--experimental-vm-modules";
  CHECK_EXPECTED_OR_EXIT(argv[0],
                         node_embedding_main_run(NODE_EMBEDDING_VERSION,
                                                 argc,
                                                 argv2,
                                                 NULL,
                                                 NULL,
                                                 ConfigureRuntimeWithEsmLoader,
                                                 NULL));
  return 0;
}

static node_embedding_status ConfigureRuntimeWithNoEsmLoader(
    void* cb_data,
    node_embedding_platform platform,
    node_embedding_runtime_config runtime_config) {
  NODE_EMBEDDED_CALL(LoadUtf8Script(
      runtime_config,
      "globalThis.require = "
      "require('module').createRequire(process.execPath);\n"
      "const { SourceTextModule } = require('node:vm');\n"
      "(async () => {\n"
      "  const stmString = 'globalThis.importResult = import(\"\")';\n"
      "  const m = new SourceTextModule(stmString, {\n"
      "    importModuleDynamically: (async () => {\n"
      "      const m = new SourceTextModule('');\n"
      "      await m.link(() => 0);\n"
      "      await m.evaluate();\n"
      "      return m.namespace;\n"
      "    }),\n"
      "  });\n"
      "  await m.link(() => 0);\n"
      "  await m.evaluate();\n"
      "  delete globalThis.importResult;\n"
      "})();\n"));

  return node_embedding_status_ok;
}

// Test ESM loaded
int32_t test_main_c_api_env_with_no_esm_loader(int32_t argc, char* argv[]) {
  CHECK_EXPECTED_OR_EXIT(argv[0],
                         node_embedding_main_run(NODE_EMBEDDING_VERSION,
                                                 argc,
                                                 argv,
                                                 NULL,
                                                 NULL,
                                                 ConfigureRuntimeWithEsmLoader,
                                                 NULL));
  return 0;
}
