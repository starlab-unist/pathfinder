#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include "exectree.h"
#include "pathfinder.h"
#include "test_utils.h"

namespace pathfinder {

class TracePCTest : public testing::Test {
 protected:
  TracePCTest() {}

  MockTracePC mock_tpc;
};

TEST_F(TracePCTest, Trace1) {
  mock_tpc->AppendPathLog(0x01);
  ExecPath epath = mock_tpc->GetPathLog();
  EXPECT_EQ(epath, ExecPath());
}

TEST_F(TracePCTest, Trace2) {
  mock_tpc->TraceOn();
  mock_tpc->AppendPathLog(0x01);
  ExecPath epath = mock_tpc->GetPathLog();
  EXPECT_EQ(epath, ExecPath({0x01}));

  EXPECT_EQ(mock_tpc->GetNumCovered(), 1);

  mock_tpc->ClearPathLog();
  epath = mock_tpc->GetPathLog();
  EXPECT_EQ(epath, ExecPath());
}

TEST_F(TracePCTest, Prune1) {
  ExecPath left = {0x01, 0x02};
  ExecPath right = {0x01, 0x03};

  mock_tpc->CheckDiff(left, right);
  EXPECT_EQ(mock_tpc->GetNumND(), 2);

  ExecPath left_pruned = mock_tpc->Prune(left);
  EXPECT_EQ(left_pruned, ExecPath({0x01}));

  ExecPath right_pruned = mock_tpc->Prune(right);
  EXPECT_EQ(right_pruned, ExecPath({0x01}));
}

TEST_F(TracePCTest, Prune2) {
  ExecPath left = {0x01, 0x02};
  ExecPath right = {0x01, 0x03};

  mock_tpc->CheckDiff(left, right);
  EXPECT_EQ(mock_tpc->GetNumND(), 2);

  mock_tpc->TraceOn();
  mock_tpc->AppendPathLog(0x01);
  mock_tpc->AppendPathLog(0x02);
  mock_tpc->AppendPathLog(0x03);
  mock_tpc->AppendPathLog(0x04);
  ExecPath epath = mock_tpc->GetPathLog();
  EXPECT_EQ(epath, ExecPath({0x01, 0x04}));
}

class ExecPathTest : public testing::Test {
 protected:
  ExecPathTest() {}

  MockTracePC mock_tpc;
};

TEST_F(ExecPathTest, Truncate) {
  ExecPath _1000_As = ExecPath(1000, 0x0A);
  ExecPath _1000_As_plus_B = vec_concat(_1000_As, {0x0B});
  EXPECT_EQ(mock_tpc->significant(_1000_As_plus_B), _1000_As);
  EXPECT_EQ(mock_tpc->tail_of(_1000_As_plus_B), ExecPath({0x0B}));
  mock_tpc->AppendPathLog(0x01);
  ExecPath epath = mock_tpc->GetPathLog();
  EXPECT_EQ(epath, ExecPath());
}

}  // namespace pathfinder
