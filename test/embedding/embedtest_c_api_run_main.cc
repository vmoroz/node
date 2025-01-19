#include <node_embedding_api_cpp.h>
#if 0
using namespace node::embedding;

// The simplest Node.js embedding scenario where the Node.js main function is
// invoked from the libnode shared library as it would be run from the Node.js
// CLI. No embedder customizations are available in this case.
extern "C" int32_t test_main_nodejs_main_node_api(int32_t argc, char* argv[]) {
  // TODO: print errors to stderr
  return NodePlatform::RunMain(NodeArgs(argc, argv), nullptr, nullptr)
      .exit_code();
}
#endif
