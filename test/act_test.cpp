#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include "exectree.h"
#include "pathfinder.h"
#include "test_utils.h"

namespace pathfinder {

class ActTest : public testing::Test {
 protected:
  ActTest() {
    act_target = std::make_unique<ExecTree>(mock_tpc.get());
    act_correct = std::make_unique<ExecTree>(mock_tpc.get());
  }

  MockTracePC mock_tpc;
  std::unique_ptr<ExecTree> act_target;
  std::unique_ptr<ExecTree> act_correct;
};

TEST_F(ActTest, Init) { ASSERT_TRUE(act_target->is_empty()); }

TEST_F(ActTest, InsertionCase1) {
  act_target->insert({0x01}, Input(), true);
  EXPECT_EQ(act_target->get_leaves().size(), 1);
}

TEST_F(ActTest, InsertionCase2) {
  act_target->insert({0x01, 0x02}, Input(), true);
  act_target->insert({0x01, 0x03}, Input(), true);
  EXPECT_EQ(act_target->get_leaves().size(), 2);
}

TEST_F(ActTest, InsertionCase2Epsilon) {
  act_target->insert({0x01}, Input(), true);
  act_target->insert({0x02}, Input(), true);
  EXPECT_EQ(act_target->get_leaves().size(), 2);
  EXPECT_EQ(act_target->get_root()->get_prefix(), Node::EPSILON);
}

TEST_F(ActTest, InsertionCase2Epsilon2) {
  act_target->insert({0x01, 0x02}, Input(), true);
  act_target->insert({0x01}, Input(), true);

  act_correct->insert({0x01, 0x02}, Input(), true);
  act_correct->insert({0x01, 0x00}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct));
}

TEST_F(ActTest, InsertionCase3) {
  act_target->insert({0x01, 0x02}, Input(), true);
  act_target->insert({0x01, 0x03}, Input(), true);
  act_target->insert({0x01}, Input(), true);

  act_correct->insert({0x01, 0x02}, Input(), true);
  act_correct->insert({0x01, 0x03}, Input(), true);
  act_correct->insert({0x01, 0x00}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct));
}

TEST_F(ActTest, InsertionCase4) {
  act_target->insert({0x01}, Input(), true);
  act_target->insert({0x01}, Input(), true);
  EXPECT_EQ(act_target->get_leaves().size(), 1);
}

TEST_F(ActTest, InsertionCase5) {
  act_target->insert({0x01, 0x02}, Input(), true);
  act_target->insert({0x01, 0x03}, Input(), true);
  act_target->insert({0x01, 0x04}, Input(), true);

  EXPECT_EQ(act_target->get_leaves().size(), 3);
}

TEST_F(ActTest, InsertionCase6) {
  act_target->insert({0x01, 0x02}, Input(), true);
  act_target->insert({0x01, 0x03, 0x04}, Input(), true);
  act_target->insert({0x01, 0x03, 0x05}, Input(), true);

  EXPECT_EQ(act_target->get_leaves().size(), 3);
}

TEST_F(ActTest, InsertionCase6Epsilon) {
  act_target->insert({0x01, 0x02}, Input(), true);
  act_target->insert({0x01, 0x03, 0x04}, Input(), true);
  act_target->insert({0x01, 0x03}, Input(), true);

  EXPECT_EQ(act_target->get_leaves().size(), 3);
}

TEST_F(ActTest, InsertionCase7) {
  act_target->insert({0x01}, Input(), true);
  act_target->insert({0x01, 0x02}, Input(), true);

  act_correct->insert({0x01, 0x00}, Input(), true);
  act_correct->insert({0x01, 0x02}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct));
}

TEST_F(ActTest, Find) {
  act_target->insert({0x01}, Input(), true);
  Node* inserted = act_target->insert({0x02}, Input(), true);
  Node* found = act_target->find({0x02});
  EXPECT_EQ(inserted, found) << "====== target ======\n"
                             << act_target->to_string(true);
}

TEST_F(ActTest, PurgeAndReinsert) {
  act_target->insert({0x01, 0x02, 0x03}, Input(), true);
  act_target->insert({0x01, 0x02, 0x04}, Input(), true);
  act_target->insert({0x01, 0x05, 0x06}, Input(), true);
  act_target->purge_and_reinsert({0x01, 0x05, 0x06}, {0x01, 0x02, 0x07});

  act_correct->insert({0x01, 0x02, 0x03}, Input(), true);
  act_correct->insert({0x01, 0x02, 0x04}, Input(), true);
  act_correct->insert({0x01, 0x02, 0x07}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct));
}

class NdPruningTest : public testing::Test {
 protected:
  NdPruningTest() {
    act_target = std::make_unique<ExecTree>(mock_tpc.get());
    act_correct = std::make_unique<ExecTree>(mock_tpc.get());
  }

  MockTracePC mock_tpc;
  std::unique_ptr<ExecTree> act_target;
  std::unique_ptr<ExecTree> act_correct;
};

TEST_F(NdPruningTest, FilterND1) {
  Node* inserted = act_target->insert({0x01, 0x02}, Input(), true);
  mock_tpc->AddND({0x02});
  act_target->prune();

  act_correct->insert({0x01}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, FilterND2) {
  act_target->insert({0x01, 0x02}, Input(), true);
  act_target->insert({0x01, 0x03}, Input(), true);
  mock_tpc->AddND({0x02});
  act_target->prune();

  act_correct->insert({0x01, 0x00}, Input(), true);
  act_correct->insert({0x01, 0x03}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, FilterND3) {
  act_target->insert({0x01, 0x02}, Input(), true);
  act_target->insert({0x01, 0x03}, Input(), true);
  mock_tpc->AddND({0x01});
  act_target->prune();

  act_correct->insert({0x02}, Input(), true);
  act_correct->insert({0x03}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, RmEmptyPrefixedInternalNode1) {
  act_target->insert({0x01, 0x02}, Input(), true);
  act_target->insert({0x01, 0x03, 0x04}, Input(), true);
  act_target->insert({0x01, 0x03, 0x05}, Input(), true);
  mock_tpc->AddND({0x03});
  act_target->prune();

  act_correct->insert({0x01, 0x02}, Input(), true);
  act_correct->insert({0x01, 0x04}, Input(), true);
  act_correct->insert({0x01, 0x05}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, RmEmptyPrefixedInternalNode2) {
  act_target->insert({0x01, 0x02}, Input(), true);
  act_target->insert({0x01, 0x03, 0x04}, Input(), true);
  act_target->insert({0x01, 0x03, 0x05, 0x06}, Input(), true);
  act_target->insert({0x01, 0x03, 0x05, 0x07}, Input(), true);
  mock_tpc->AddND({0x03, 0x05});
  act_target->prune();

  act_correct->insert({0x01, 0x02}, Input(), true);
  act_correct->insert({0x01, 0x04}, Input(), true);
  act_correct->insert({0x01, 0x06}, Input(), true);
  act_correct->insert({0x01, 0x07}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, Merge1) {
  act_target->insert({0x01, 0x0A, 0x02}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02}, Input(), true);
  act_target->insert({0x01, 0xC}, Input(), true);
  mock_tpc->AddND({0x0A, 0x0B});
  act_target->prune();

  act_correct->insert({0x01, 0x02}, Input(), true);
  act_correct->insert({0x01, 0x0C}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, Merge2) {
  act_target->insert({0x01, 0x0A, 0x02}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02, 0x03}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02, 0x04}, Input(), true);
  act_target->insert({0x01, 0x0C}, Input(), true);
  mock_tpc->AddND({0x0A, 0x0B});
  act_target->prune();

  act_correct->insert({0x01, 0x02}, Input(), true);
  act_correct->insert({0x01, 0x02, 0x03}, Input(), true);
  act_correct->insert({0x01, 0x02, 0x04}, Input(), true);
  act_correct->insert({0x01, 0x0C}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, Merge2Epsilon) {
  act_target->insert({0x01, 0x0A, 0x02}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02, 0x03}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02}, Input(), true);
  act_target->insert({0x01, 0x0C}, Input(), true);
  mock_tpc->AddND({0x0A, 0x0B});
  act_target->prune();

  act_correct->insert({0x01, 0x02}, Input(), true);
  act_correct->insert({0x01, 0x02, 0x03}, Input(), true);
  act_correct->insert({0x01, 0x0C}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, Merge3) {
  act_target->insert({0x01, 0x0A, 0x02, 0x03}, Input(), true);
  act_target->insert({0x01, 0x0A, 0x02, 0x04}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02}, Input(), true);
  act_target->insert({0x01, 0x0C}, Input(), true);
  mock_tpc->AddND({0x0A, 0x0B});
  act_target->prune();

  act_correct->insert({0x01, 0x02}, Input(), true);
  act_correct->insert({0x01, 0x02, 0x03}, Input(), true);
  act_correct->insert({0x01, 0x02, 0x04}, Input(), true);
  act_correct->insert({0x01, 0x0C}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, Merge3Epsilon) {
  act_target->insert({0x01, 0x0A, 0x02, 0x03}, Input(), true);
  act_target->insert({0x01, 0x0A, 0x02}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02}, Input(), true);
  act_target->insert({0x01, 0x0C}, Input(), true);
  mock_tpc->AddND({0x0A, 0x0B});
  act_target->prune();

  act_correct->insert({0x01, 0x02}, Input(), true);
  act_correct->insert({0x01, 0x02, 0x03}, Input(), true);
  act_correct->insert({0x01, 0x0C}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, Merge4) {
  act_target->insert({0x01, 0x0A, 0x02, 0x03}, Input(), true);
  act_target->insert({0x01, 0x0A, 0x02, 0x04}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02, 0x05}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02, 0x06}, Input(), true);
  act_target->insert({0x01, 0x0C}, Input(), true);
  mock_tpc->AddND({0x0A, 0x0B});
  act_target->prune();

  act_correct->insert({0x01, 0x02, 0x03}, Input(), true);
  act_correct->insert({0x01, 0x02, 0x04}, Input(), true);
  act_correct->insert({0x01, 0x02, 0x05}, Input(), true);
  act_correct->insert({0x01, 0x02, 0x06}, Input(), true);
  act_correct->insert({0x01, 0x0C}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, Merge5) {
  act_target->insert({0x01, 0x0A, 0x02}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02, 0x03}, Input(), true);
  act_target->insert({0x01, 0x0C}, Input(), true);
  mock_tpc->AddND({0x0A, 0x0B});
  act_target->prune();

  act_correct->insert({0x01, 0x02}, Input(), true);
  act_correct->insert({0x01, 0x02, 0x03}, Input(), true);
  act_correct->insert({0x01, 0x0C}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, Merge6) {
  act_target->insert({0x01, 0x0A, 0x02, 0x03}, Input(), true);
  act_target->insert({0x01, 0x0A, 0x02, 0x04}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02, 0x05}, Input(), true);
  act_target->insert({0x01, 0x0C}, Input(), true);
  mock_tpc->AddND({0x0A, 0x0B});
  act_target->prune();

  act_correct->insert({0x01, 0x02, 0x03}, Input(), true);
  act_correct->insert({0x01, 0x02, 0x04}, Input(), true);
  act_correct->insert({0x01, 0x02, 0x05}, Input(), true);
  act_correct->insert({0x01, 0x0C}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, Merge7) {
  act_target->insert({0x01, 0x0A, 0x02, 0x03}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02}, Input(), true);
  act_target->insert({0x01, 0x0C}, Input(), true);
  mock_tpc->AddND({0x0A, 0x0B});
  act_target->prune();

  act_correct->insert({0x01, 0x02, 0x03}, Input(), true);
  act_correct->insert({0x01, 0x02}, Input(), true);
  act_correct->insert({0x01, 0x0C}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, Merge8) {
  act_target->insert({0x01, 0x0A, 0x02, 0x03}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02, 0x04}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02, 0x05}, Input(), true);
  act_target->insert({0x01, 0x0C}, Input(), true);
  mock_tpc->AddND({0x0A, 0x0B});
  act_target->prune();

  act_correct->insert({0x01, 0x02, 0x03}, Input(), true);
  act_correct->insert({0x01, 0x02, 0x04}, Input(), true);
  act_correct->insert({0x01, 0x02, 0x05}, Input(), true);
  act_correct->insert({0x01, 0x0C}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, Merge9) {
  act_target->insert({0x01, 0x0A, 0x02, 0x03}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02, 0x04}, Input(), true);
  act_target->insert({0x01, 0x0C}, Input(), true);
  mock_tpc->AddND({0x0A, 0x0B});
  act_target->prune();

  act_correct->insert({0x01, 0x02, 0x03}, Input(), true);
  act_correct->insert({0x01, 0x02, 0x04}, Input(), true);
  act_correct->insert({0x01, 0x0C}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, RmInternalWithOnlyChild1) {
  act_target->insert({0x0A, 0x01}, Input(), true);
  act_target->insert({0x0B, 0x01}, Input(), true);
  mock_tpc->AddND({0x0A, 0x0B});
  act_target->prune();

  act_correct->insert({0x01}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, RmInternalWithOnlyChild2) {
  act_target->insert({0x01, 0x0A, 0x02}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02}, Input(), true);
  mock_tpc->AddND({0x0A, 0x0B});
  act_target->prune();

  act_correct->insert({0x01, 0x02}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, RmInternalWithOnlyChild3) {
  act_target->insert({0x01, 0x0A, 0x02, 0x0C, 0x03}, Input(), true);
  act_target->insert({0x01, 0x0A, 0x02, 0x0D, 0x03}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02, 0x0C, 0x03}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02, 0x0D, 0x03}, Input(), true);
  mock_tpc->AddND({0x0A, 0x0B, 0x0C, 0x0D});
  act_target->prune();

  act_correct->insert({0x01, 0x02, 0x03}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, RmInternalWithOnlyChild4) {
  act_target->insert({0x01, 0x0A, 0x02, 0x0C, 0x03}, Input(), true);
  act_target->insert({0x01, 0x0A, 0x02, 0x0D, 0x03}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02, 0x0C, 0x03}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02, 0x0D, 0x03}, Input(), true);
  act_target->insert({0x0E}, Input(), true);
  mock_tpc->AddND({0x0A, 0x0B, 0x0C, 0x0D});
  act_target->prune();

  act_correct->insert({0x01, 0x02, 0x03}, Input(), true);
  act_correct->insert({0x0E}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, PullNode) {
  act_target->insert({0x01, 0x0A}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x02}, Input(), true);
  act_target->insert({0x01, 0x0B, 0x03}, Input(), true);
  mock_tpc->AddND({0x0A, 0x0B});
  act_target->prune();

  act_correct->insert({0x01}, Input(), true);
  act_correct->insert({0x01, 0x02}, Input(), true);
  act_correct->insert({0x01, 0x03}, Input(), true);

  EXPECT_TRUE(struct_eq(*act_target, *act_correct))
      << "====== target ======\n"
      << act_target->to_string(true) << "====== correct ======\n"
      << act_correct->to_string(true);
}

TEST_F(NdPruningTest, TailExecPath1) {
  ExecPath _1000_As = ExecPath(1000, 0x0A);
  ExecPath _1000_As_plus_B = vec_concat(_1000_As, {0x0B});
  act_target->insert(_1000_As, Input(), true);
  EXPECT_TRUE(act_target->has(_1000_As_plus_B));
}

TEST_F(NdPruningTest, TailExecPath2) {
  ExecPath _1000_As = ExecPath(1000, 0x0A);
  ExecPath _1000_As_plus_B = vec_concat(_1000_As, {0x0B});
  act_target->insert(_1000_As_plus_B, Input(), true);
  EXPECT_TRUE(act_target->has(_1000_As));
}

TEST_F(NdPruningTest, TailExecPath3) {
  ExecPath _3_Bs = {0x0B, 0x0B, 0x0B};
  ExecPath _1000_As = ExecPath(1000, 0x0A);
  ExecPath _3_Bs_plus_1000_As = vec_concat(_3_Bs, _1000_As);
  act_target->insert(_3_Bs_plus_1000_As, Input(), true);
  EXPECT_TRUE(act_target->has(_3_Bs_plus_1000_As));
  ExecPath _997_As = ExecPath(997, 0x0A);
  ExecPath _3_Bs_plus_997_As = vec_concat(_3_Bs, _997_As);
  EXPECT_TRUE(act_target->has(_3_Bs_plus_997_As));

  mock_tpc->AddND({0x0B});
  act_target->prune();
  EXPECT_TRUE(act_target->has(_1000_As));
  EXPECT_FALSE(act_target->has(_997_As));
}

}  // namespace pathfinder
