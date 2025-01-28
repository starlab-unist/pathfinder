#ifndef PATHFINDER_TEST_UTILS
#define PATHFINDER_TEST_UTILS

#include "exectree.h"
#include "pathfinder.h"

namespace pathfinder {

class MockTracePC {
 public:
  MockTracePC(size_t num_pc = 100,
              size_t max_significant_execpath_size = 1000) {
    tpc = std::make_unique<TracePC>(max_significant_execpath_size);
    guard = new uint32_t[num_pc];
    memset(guard, 0, num_pc * sizeof(uint32_t));
    tpc->HandleInit(guard, guard + num_pc);
    tpc->InitCoveredBitMap();
  }
  ~MockTracePC() {
    if (guard != nullptr) delete[] guard;
  }
  TracePC* operator->() { return tpc.get(); }
  TracePC* get() { return tpc.get(); }

 private:
  uint32_t* guard = nullptr;
  std::unique_ptr<TracePC> tpc;
};

}  // namespace pathfinder

extern "C" {

void (*PathFinderSetup)() = nullptr;
int (*PathFinderTestOneInput)(const pathfinder::Input& x) = nullptr;

}  // extern "C"

#endif
