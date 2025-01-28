#include <stddef.h>
#include <stdint.h>

#include "pathfinder.h"

#define DTYPE_INT   0
#define DTYPE_FLOAT 1

bool check_matmul(int a_dtype, int a_row, int a_col,
                  int b_dtype, int b_row, int b_col) {

  if (a_dtype != DTYPE_FLOAT)
    return false;

  if (a_dtype != b_dtype)
    return false;

  if (a_col != b_row)
    return false;

  return true;
}

extern "C" {

void PathFinderSetup() {
  PathFinderEnumArg("a_dtype", 0, 2);
  PathFinderEnumArg("b_dtype", 0, 2);
  PathFinderIntArg("a_row");
  PathFinderIntArg("a_col");
  PathFinderIntArg("b_row");
  PathFinderIntArg("b_col");

  PathFinderAddHardConstraint({
      sym_int_arg["a_row"] >= 1,
      sym_int_arg["a_col"] >= 1,
      sym_int_arg["b_row"] >= 1,
      sym_int_arg["b_col"] >= 1,
  });
}

int PathFinderTestOneInput(const pathfinder::Input& x) {
  
  PathFinderExecuteTarget(
    check_matmul(
      x["a_dtype"], x["a_row"], x["a_col"],
      x["b_dtype"], x["b_row"], x["b_col"]));

  return 0;
}

}  // extern "C"

int main(int argc, char** argv) {
  pathfinder::parse_arg(argc, argv);
  return pathfinder::driver(PathFinderTestOneInput);
}
