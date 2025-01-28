#include <stddef.h>
#include <stdint.h>

#include "example/lib_external.h"
#include "example/utils.h"
#include "pathfinder.h"

int epsilon(long a, long b, long c, long d) {
  if (a < b) {
    if (b < c) {
      throw_if(c == d);
    }
  }
  return 0;
}

extern "C" {

void PathFinderSetup() {
  PathFinderIntArg("a");
  PathFinderIntArg("b");
  PathFinderIntArg("c");
  PathFinderIntArg("d");
}

int PathFinderTestOneInput(const pathfinder::Input& x) {
  try {
    PathFinderExecuteTarget(epsilon(x["a"], x["b"], x["c"], x["d"]));
  } catch (const InvisibleException& e) {
    return -2;
  }

  return 0;
}

}  // extern "C"

int main(int argc, char** argv) {
  pathfinder::parse_arg(argc, argv);
  return pathfinder::driver(PathFinderTestOneInput);
}
