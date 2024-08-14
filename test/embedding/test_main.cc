#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include "executable_wrapper.h"
#include "node.h"

extern "C" int test_main(int argc, char** argv);

NODE_MAIN(int argc, node::argv_type raw_argv[]) {
  char** argv = nullptr;
  node::FixupMain(argc, raw_argv, &argv);
  return test_main(argc, argv);
}