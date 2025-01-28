#ifndef PATHFINDER_BRANCH_CONDITION
#define PATHFINDER_BRANCH_CONDITION

#include <optional>

#include "enumarg_bitvec.h"
#include "options.h"
#include "pathfinder_defs.h"
#include "sygus_ast.h"

namespace pathfinder {

class ConfusionMatrix {
 public:
  ConfusionMatrix();
  ConfusionMatrix(int64_t tp_, int64_t tn_, int64_t fp_, int64_t fn_);
  bool operator==(const ConfusionMatrix& other) const;
  ConfusionMatrix symmetry() const;
  void add_tp();
  void add_tn();
  void add_fp();
  void add_fn();
  bool perfect() const;
  void update(const ConfusionMatrix& other);
  double accuracy() const;

 private:
  int64_t tp;
  int64_t tn;
  int64_t fp;
  int64_t fn;
};

enum SynthesisStatus {
  SUCCESS,
  FAIL,
  GIVEUP,
};
class BranchCondition;
typedef std::tuple<SynthesisStatus, std::unique_ptr<BranchCondition>,
                   std::unique_ptr<BranchCondition>, int64_t>
    SynthesisResult;

std::string run_synthesizer(std::string sygus_file, float timeout);

class BranchCondition {
 public:
  static const size_t MAX_SAMPLE_SIZE;

  BranchCondition(CondType condtype_);
  BranchCondition(const BranchCondition& other) = default;
  virtual ~BranchCondition() = default;
  virtual bool operator==(const BranchCondition& other) const;

  bool eval_and_update(const Input& input, bool ground_truth);
  static int64_t synthesis_budget_max();
  int64_t get_synthesis_budget() const;
  void set_synthesis_budget(int64_t budget);
  void deduct_synthesis_budget(int64_t used);
  bool insolvent() const;
  SynthesisResult synthesize(bool is_pair, const std::set<Input>& pos_examples,
                             const std::set<Input>& neg_examples);

  virtual bool is_accurate() const = 0;
  virtual bool invalid() const = 0;
  virtual bool eval(const Input& input, bool ground_truth) const = 0;
  virtual std::string to_string() const = 0;

  CondType get_condtype() const;

 protected:
  ConfusionMatrix cmat;
  void classify(const std::set<Input>& pos_examples,
                const std::set<Input>& neg_examples);

 private:
  virtual SynthesisResult synthesize_internal(
      bool is_pair, const std::set<Input>& pos_examples,
      const std::set<Input>& neg_examples) = 0;

  CondType condtype;
  int64_t synthesis_budget;

  friend class Node;
};

class EnumCondition : public BranchCondition {
 public:
  EnumCondition();
  EnumCondition(const EnumCondition& other);
  virtual bool operator==(const EnumCondition& other) const;

  virtual bool is_accurate() const;
  virtual bool invalid() const;
  virtual bool eval(const Input& input, bool ground_truth) const;

  virtual std::string to_string() const;

 private:
  virtual SynthesisResult synthesize_internal(
      bool is_pair, const std::set<Input>& pos_examples,
      const std::set<Input>& neg_examples);
  bool is_inclusion_cond() const;
  void disable_inclusion_cond();
  void set_inclusion_cond(EnumArgBitVec bv);
  std::optional<EnumArgBitVec> get_inclusion_cond() const;
  void set_equality_cond(std::unique_ptr<BoolExpr> equality_cond_);
  BoolExpr* get_equality_cond() const;

  std::optional<EnumArgBitVec> inclusion_cond;
  std::unique_ptr<BoolExpr> equality_cond;

  friend class EnumSolver;
};

class NumericCondition : public BranchCondition {
 public:
  NumericCondition();
  NumericCondition(const NumericCondition& other);
  virtual bool operator==(const NumericCondition& other) const;

  virtual bool is_accurate() const;
  virtual bool invalid() const;
  virtual bool eval(const Input& input, bool ground_truth) const;
  virtual std::string to_string() const;

 private:
  virtual SynthesisResult synthesize_internal(
      bool is_pair, const std::set<Input>& pos_examples,
      const std::set<Input>& neg_examples);

  const double accuracy_min = -1.0;
  const double accuracy_max = 1.0;
  double dynamic_threshold() const;
  std::unique_ptr<BoolExpr> cond;

  friend class NumericSolver;
};

class NeglectCondition : public BranchCondition {
 public:
  NeglectCondition();
  NeglectCondition(const NeglectCondition& other) = default;
  virtual bool operator==(const NeglectCondition& other) const;

  virtual bool is_accurate() const;
  virtual bool invalid() const;
  virtual bool eval(const Input& input, bool ground_truth) const;
  virtual std::string to_string() const;

 private:
  virtual SynthesisResult synthesize_internal(
      bool is_pair, const std::set<Input>& pos_examples,
      const std::set<Input>& neg_examples);
};

CondType default_condtype();
std::unique_ptr<BranchCondition> default_branch_condition();
std::unique_ptr<BranchCondition> copy(
    const std::unique_ptr<BranchCondition>& other);

}  // namespace pathfinder

#endif
