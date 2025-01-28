#include <stddef.h>
#include <stdint.h>

#include "example/utils.h"
#include "pathfinder.h"

int abcde(long a, long b, long c, long d, long e, long f) {
  if (a < b) {
    if (b < c) {
      if (c < d) {
        if (a * b % c > d * e % f) {
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
  PathFinderIntArg("e");
  PathFinderIntArg("f");
  PathFinderAddHardConstraint({
      sym_int_arg["c"] > 0,
      sym_int_arg["f"] > 0,
  });
}

int PathFinderTestOneInput(const pathfinder::Input& x) {
  PathFinderExecuteTarget(
      abcde(x["a"], x["b"], x["c"], x["d"], x["e"], x["f"]));

  return 0;
}

}  // extern "C"

int main(int argc, char** argv) {
  pathfinder::parse_arg(argc, argv);
  return pathfinder::driver(PathFinderTestOneInput);
}
