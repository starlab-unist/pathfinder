#include <stddef.h>
#include <stdint.h>

#include "example/utils.h"
#include "pathfinder.h"

void TORCH_CHECK(bool b) {
  if (b) {
    do_something();
  } else {
    throw ExpectedException();
  }
}

void conv2d_check_shape(long i_rank, long i0, long i1, long i2, long i3,
                        long i4, long w_rank, long w0, long w1, long w2,
                        long w3, long w4, long p0, long p1, long d0, long d1,
                        long groups) {
  TORCH_CHECK(i_rank == 4);
  TORCH_CHECK(i_rank == w_rank);
  TORCH_CHECK(w0 >= groups);
  TORCH_CHECK(w0 % groups == 0);

  TORCH_CHECK(i1 == w1 * groups);

  bool kernel_size_correct =
      i2 + 2 * p0 >= d0 * (w2 - 1) + 1 && i3 + 2 * p1 >= d1 * (w3 - 1) + 1;
  TORCH_CHECK(kernel_size_correct);
}

extern "C" {

void PathFinderSetup() {
  PathFinderEnumArg("i_rank", 6);
  PathFinderIntArg("i0");
  PathFinderIntArg("i1");
  PathFinderIntArg("i2");
  PathFinderIntArg("i3");
  PathFinderIntArg("i4");
  PathFinderEnumArg("w_rank", 6);
  PathFinderIntArg("w0");
  PathFinderIntArg("w1");
  PathFinderIntArg("w2");
  PathFinderIntArg("w3");
  PathFinderIntArg("w4");
  PathFinderIntArg("p0");
  PathFinderIntArg("p1");
  PathFinderIntArg("d0");
  PathFinderIntArg("d1");
  PathFinderIntArg("groups");
  PathFinderAddHardConstraint({
      sym_int_arg["groups"] > 0,
  });
}

int PathFinderTestOneInput(const pathfinder::Input& x) {
  try {
    PathFinderExecuteTarget(conv2d_check_shape(
        x["i_rank"], x["i0"], x["i1"], x["i2"], x["i3"], x["i4"], x["w_rank"],
        x["w0"], x["w1"], x["w2"], x["w3"], x["w4"], x["p0"], x["p1"], x["d0"],
        x["d1"], x["groups"]));
  } catch (const ExpectedException& e) {
    return -2;
  }

  return 0;
}

}  // extern "C"

int main(int argc, char** argv) {
  pathfinder::parse_arg(argc, argv);
  return pathfinder::driver(PathFinderTestOneInput);
}
