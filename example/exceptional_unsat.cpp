#include <stddef.h>
#include <stdint.h>

#include "example/utils.h"
#include "pathfinder.h"

int unsat(long a) { return 0; }

extern "C" {

void PathFinderSetup() {
  PathFinderIntArg("a");
  PathFinderAddHardConstraint({
      sym_int_arg["a"] < 0,
      sym_int_arg["a"] > 0,
  });
}

int PathFinderTestOneInput(const pathfinder::Input& x) {
  PathFinderExecuteTarget(unsat(x["a"]));

  return 0;
}

}  // extern "C"

int main(int argc, char** argv) {
  pathfinder::parse_arg(argc, argv);
  return pathfinder::driver(PathFinderTestOneInput);
}
