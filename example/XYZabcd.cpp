#include <stddef.h>
#include <stdint.h>

#include "example/utils.h"
#include "pathfinder.h"

enum ENUM {
  EnumA,
  EnumB,
  EnumC,
};

int XYZabcd(ENUM X, ENUM Y, ENUM Z, long a, long b, long c, long d) {
  if (X != EnumA) {
    if (b > c) {
      if (a > b) {
        if (X == Y) {
          if (Y != Z) {
            do_something();
          }
        }
      }
    } else if (X == EnumB) {
      if (c > d) {
        do_something();
      }
    }
  }
  return 0;
}

extern "C" {

void PathFinderSetup() {
  PathFinderEnumArg("X", {"EnumA", "EnumB", "EnumC"});
  PathFinderEnumArg("Y", {"EnumA", "EnumB", "EnumC"});
  PathFinderEnumArg("Z", {"EnumA", "EnumB", "EnumC"});
  PathFinderIntArg("a");
  PathFinderIntArg("b");
  PathFinderIntArg("c");
  PathFinderIntArg("d");
}

int PathFinderTestOneInput(const pathfinder::Input& x) {
  PathFinderExecuteTarget(XYZabcd(ENUM(x["X"]), ENUM(x["Y"]), ENUM(x["Z"]),
                                  x["a"], x["b"], x["c"], x["d"]));
  return 0;
}

}  // extern "C"

int main(int argc, char** argv) {
  pathfinder::parse_arg(argc, argv);
  return pathfinder::driver(PathFinderTestOneInput);
}
