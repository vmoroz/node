#include "embedtest_c_api_common.h"

using namespace node::embedding;

// The simplest Node.js embedding scenario where the Node.js main function is
// invoked from the libnode shared library as it would be run from the Node.js
// CLI. No embedder customizations are available in this case.
extern "C" int32_t test_main_nodejs_main_c_cpp_api(int32_t argc, char* argv[]) {
  NodeExpected<void> result =
      NodePlatform::RunMain(NodeArgs(argc, argv), nullptr, nullptr);
  return PrintErrorMessage(argv[0], std::move(result)).exit_code();
}
