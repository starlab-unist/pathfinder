#include <stddef.h>
#include <stdint.h>

#include "example/lib_external.h"
#include "example/utils.h"
#include "pathfinder.h"

int conflict(long a, long b, long c, long d) {
  if (random_bool()) {
    do_something();
  }

  if (a < b) {
    if (b < c) {
      if (c < d) {
        if (random_bool()) {
          do_something();
        }
      }
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
  PathFinderExecuteTarget(conflict(x["a"], x["b"], x["c"], x["d"]));

  return 0;
}

}  // extern "C"

int main(int argc, char** argv) {
  pathfinder::parse_arg(argc, argv);
  return pathfinder::driver(PathFinderTestOneInput);
}
