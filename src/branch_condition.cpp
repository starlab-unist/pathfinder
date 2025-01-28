#include "branch_condition.h"

#include <unistd.h>

#include <cmath>
#include <iostream>

#include "input_signature.h"
#include "sygus_gen.h"
#include "sygus_parser.h"
#include "utils.h"

namespace pathfinder {

ConfusionMatrix::ConfusionMatrix() : tp(0), tn(0), fp(0), fn(0) {}
ConfusionMatrix::ConfusionMatrix(int64_t tp_, int64_t tn_, int64_t fp_,
                                 int64_t fn_)
    : tp(tp_), tn(tn_), fp(fp_), fn(fn_) {}
bool ConfusionMatrix::operator==(const ConfusionMatrix& other) const {
  return tp == other.tp && tn == other.tn && fp == other.fp && fn == other.fn;
}
ConfusionMatrix ConfusionMatrix::symmetry() const {
  return ConfusionMatrix(tn, tp, fn, fp);
}
void ConfusionMatrix::add_tp() { tp++; }
void ConfusionMatrix::add_tn() { tn++; }
void ConfusionMatrix::add_fp() { fp++; }
void ConfusionMatrix::add_fn() { fn++; }
bool ConfusionMatrix::perfect() const { return tp + tn > 0 && fp + fn == 0; }
void ConfusionMatrix::update(const ConfusionMatrix& other) {
  tp += other.tp;
  tn += other.tn;
  fp += other.fp;
  fn += other.fn;
}
double ConfusionMatrix::accuracy() const {
  int64_t tp_ = tp;
  int64_t tn_ = tn;
  int64_t fp_ = fp;
  int64_t fn_ = fn;
  while (std::max({tp_, tn_, fp_, fn_}) > 25000) {
    tp_ /= 2;
    tn_ /= 2;
    fp_ /= 2;
    fn_ /= 2;
  }
  int64_t numerator = tp_ * tn_ - fp_ * fn_;
  int64_t prod = (tp_ + fp_) * (tp_ + fn_) * (tn_ + fp_) * (tn_ + fn_);
  if (prod == 0) return 0.0;
  double denom = sqrt(prod);
  return (double)numerator / denom;
}

std::string run_synthesizer(std::string sygus_file, float timeout) {
  if (almost_zero(timeout)) return "";

  static std::string temp_sygus_file_name =
      std::string("temp_p_") + std::to_string(getpid()) + ".sl";
  write_to_file(temp_sygus_file_name, sygus_file);
  return exec(duet_cmd(temp_sygus_file_name, timeout));
}

const size_t BranchCondition::MAX_SAMPLE_SIZE = 50;
BranchCondition::BranchCondition(CondType condtype_) : condtype(condtype_) {
  synthesis_budget = synthesis_budget_max();
}
bool BranchCondition::operator==(const BranchCondition& other) const {
  return condtype == other.condtype &&
         synthesis_budget == other.synthesis_budget && cmat == other.cmat;
}
CondType BranchCondition::get_condtype() const { return condtype; }
bool BranchCondition::eval_and_update(const Input& input, bool ground_truth) {
  assert(!invalid());

  try {
    if (eval(input, ground_truth)) {
      if (ground_truth == true) {
        cmat.add_tp();
      } else {
        cmat.add_tn();
      }
      return true;
    } else {
      if (ground_truth == true) {
        cmat.add_fn();
      } else {
        cmat.add_fp();
      }
      return false;
    }
  } catch (const CondEvalException& e) {
    if (ground_truth == true) {
      cmat.add_fn();
    } else {
      cmat.add_fp();
    }
    return false;
  }
}
void BranchCondition::classify(const std::set<Input>& pos_examples,
                               const std::set<Input>& neg_examples) {
  assert(!invalid());

  int64_t tp = 0;  // true positives
  int64_t tn = 0;  // true negatives
  int64_t fp = 0;  // false positives
  int64_t fn = 0;  // false negatives

  for (auto pos_example : pos_examples) {
    try {
      if (eval(pos_example, true)) {
        tp++;
      } else {
        fn++;
      }
    } catch (const CondEvalException& e) {
      fn++;
    }
  }
  for (auto neg_example : neg_examples) {
    try {
      if (eval(neg_example, false)) {
        tn++;
      } else {
        fp++;
      }
    } catch (const CondEvalException& e) {
      fp++;
    }
  }
  cmat = ConfusionMatrix(tp, tn, fp, fn);
}
int64_t BranchCondition::synthesis_budget_max() {
  return SYNTHESIS_BUDGET * 1000000000;  // in nanoseconds
}
int64_t BranchCondition::get_synthesis_budget() const {
  return synthesis_budget;
}
void BranchCondition::set_synthesis_budget(int64_t budget) {
  synthesis_budget = budget;
}

void BranchCondition::deduct_synthesis_budget(int64_t used) {
  synthesis_budget = synthesis_budget > used ? synthesis_budget - used : 0;
  if (almost_zero(ns_to_s(synthesis_budget))) synthesis_budget = 0;
}
bool BranchCondition::insolvent() const { return synthesis_budget <= 0; }
SynthesisResult BranchCondition::synthesize(
    bool is_pair, const std::set<Input>& pos_examples,
    const std::set<Input>& neg_examples) {
  if (insolvent()) return std::make_tuple(GIVEUP, nullptr, nullptr, 0);

  SynthesisStatus status;
  std::unique_ptr<BranchCondition> cond_new;
  std::unique_ptr<BranchCondition> cond_new_sibling;
  int64_t synthesis_time;
  std::tie(status, cond_new, cond_new_sibling, synthesis_time) =
      synthesize_internal(is_pair, pos_examples, neg_examples);

  if (status == SUCCESS) {
    cond_new->classify(pos_examples, neg_examples);
    if (is_pair) {
      cond_new_sibling->classify(neg_examples, pos_examples);
    }
  }

  return std::make_tuple(status, std::move(cond_new),
                         std::move(cond_new_sibling), synthesis_time);
}

EnumCondition::EnumCondition() : BranchCondition(CT_ENUM) {
  inclusion_cond = EnumArgBitVec();
}
EnumCondition::EnumCondition(const EnumCondition& other)
    : BranchCondition(other) {
  inclusion_cond = other.inclusion_cond;
  equality_cond = other.equality_cond == nullptr
                      ? nullptr
                      : std::make_unique<BoolExpr>(*other.equality_cond);
}
bool EnumCondition::operator==(const EnumCondition& other) const {
  if (!BranchCondition::operator==(other)) return false;

  if (!(inclusion_cond == other.inclusion_cond)) return false;

  if (equality_cond == nullptr && other.equality_cond == nullptr) {
    return true;
  } else if (equality_cond == nullptr && other.equality_cond != nullptr) {
    return false;
  } else if (equality_cond != nullptr && other.equality_cond == nullptr) {
    return false;
  } else {
    return equality_cond->struct_eq(*other.equality_cond);
  }
}
bool EnumCondition::is_accurate() const { return cmat.perfect(); }
bool EnumCondition::invalid() const {
  return (inclusion_cond.has_value() && inclusion_cond.value().empty()) ||
         (!inclusion_cond.has_value() && equality_cond == nullptr);
}
bool EnumCondition::eval(const Input& input, bool ground_truth) const {
  const Args& args = input.get_enum_args();
  assert(!invalid());
  if (is_inclusion_cond()) {
    return inclusion_cond.value().eval(args) == ground_truth;
  } else {
    return equality_cond->eval(args) == ground_truth;
  }
};
SynthesisResult EnumCondition::synthesize_internal(
    bool is_pair, const std::set<Input>& pos_examples,
    const std::set<Input>& neg_examples) {
  std::chrono::steady_clock::time_point synthesis_start =
      std::chrono::steady_clock::now();

  std::unique_ptr<EnumCondition> cond_new, cond_new_sibling;
  cond_new = std::make_unique<EnumCondition>();
  cond_new->set_synthesis_budget(get_synthesis_budget());
  if (is_pair) {
    cond_new_sibling = std::make_unique<EnumCondition>();
    cond_new_sibling->set_synthesis_budget(get_synthesis_budget());
  }

  if (is_inclusion_cond()) {
    EnumArgBitVecArray enum_bvs_pos = initial_enum_bvs(false);
    for (auto pos_example : pos_examples)
      enum_bvs_pos.set(pos_example.get_enum_args());
    EnumArgBitVecArray enum_bvs_neg = initial_enum_bvs(false);
    for (auto neg_example : neg_examples)
      enum_bvs_neg.set(neg_example.get_enum_args());
    EnumArgBitVecArray distinct_bvs = enum_bvs_pos.distinct(enum_bvs_neg);
    if (!distinct_bvs.empty()) {
      cond_new->set_inclusion_cond(distinct_bvs.export_non_empty_bv());
      if (is_pair) {
        EnumArgBitVecArray distinct_bvs_opposite =
            enum_bvs_neg.distinct(enum_bvs_pos);
        assert(!distinct_bvs_opposite.empty());
        cond_new_sibling->set_inclusion_cond(
            distinct_bvs_opposite.export_non_empty_bv());
      }
      return std::make_tuple(SUCCESS, std::move(cond_new),
                             std::move(cond_new_sibling),
                             elapsed_from_ns(synthesis_start));
    } else {
      cond_new->disable_inclusion_cond();
      if (is_pair) cond_new_sibling->disable_inclusion_cond();
    }
  }

  std::vector<std::unique_ptr<Constraint>> ctrs;
  for (auto pos_example : pos_examples)
    ctrs.push_back(std::make_unique<Constraint>(
        "f", CT_ENUM, pos_example.get_enum_args(), true));
  for (auto neg_example : neg_examples)
    ctrs.push_back(std::make_unique<Constraint>(
        "f", CT_ENUM, neg_example.get_enum_args(), false));

  std::unique_ptr<SygusFile> sfile = gen_sygus_file(CT_ENUM, std::move(ctrs));
  auto synthesizer_result =
      run_synthesizer(sfile->to_string(), ns_to_s(get_synthesis_budget()));

  const std::string DUET_FAIL_MSG_PREFIX = "Fatal error: exception";
  if (is_prefix_of(DUET_FAIL_MSG_PREFIX, synthesizer_result) ||
      synthesizer_result == "")  // TIMEOUT
    return std::make_tuple(GIVEUP, nullptr, nullptr,
                           elapsed_from_ns(synthesis_start));

  auto synthesized_cond = std::make_unique<BoolExpr>(
      simplify(*parse_fun(synthesizer_result)->get_body()));

  cond_new->set_equality_cond(std::move(synthesized_cond));
  if (is_pair)
    cond_new_sibling->set_equality_cond(
        std::make_unique<BoolExpr>(!(*(cond_new->get_equality_cond()))));

  return std::make_tuple(SUCCESS, std::move(cond_new),
                         std::move(cond_new_sibling),
                         elapsed_from_ns(synthesis_start));
}
std::string EnumCondition::to_string() const {
  if (inclusion_cond.has_value() && !inclusion_cond.value().empty()) {
    return (inclusion_cond.value()).to_string(true);
  } else if (equality_cond != nullptr) {
    return equality_cond->to_string();
  }
  return "none";
}

bool EnumCondition::is_inclusion_cond() const {
  return inclusion_cond.has_value();
}
void EnumCondition::disable_inclusion_cond() { inclusion_cond = std::nullopt; }
void EnumCondition::set_inclusion_cond(EnumArgBitVec bv) {
  inclusion_cond = bv;
}
std::optional<EnumArgBitVec> EnumCondition::get_inclusion_cond() const {
  return inclusion_cond;
}
void EnumCondition::set_equality_cond(
    std::unique_ptr<BoolExpr> equality_cond_) {
  disable_inclusion_cond();
  equality_cond = std::move(equality_cond_);
}
BoolExpr* EnumCondition::get_equality_cond() const {
  assert(!is_inclusion_cond());
  return equality_cond.get();
}

NumericCondition::NumericCondition() : BranchCondition(CT_NUMERIC) {}
NumericCondition::NumericCondition(const NumericCondition& other)
    : BranchCondition(other) {
  cond =
      other.cond == nullptr ? nullptr : std::make_unique<BoolExpr>(*other.cond);
}
bool NumericCondition::operator==(const NumericCondition& other) const {
  if (!BranchCondition::operator==(other)) return false;

  if (cond == nullptr && other.cond == nullptr) {
    return true;
  } else if (cond == nullptr && other.cond != nullptr) {
    return false;
  } else if (cond != nullptr && other.cond == nullptr) {
    return false;
  } else {
    return cond->struct_eq(*other.cond);
  }
}
bool NumericCondition::is_accurate() const {
  return cmat.accuracy() >= dynamic_threshold();
}
bool NumericCondition::invalid() const { return cond == nullptr; }
bool NumericCondition::eval(const Input& input, bool ground_truth) const {
  assert(cond != nullptr);
  return cond->eval(input.get_numeric_args()) == ground_truth;
}
std::string NumericCondition::to_string() const {
  if (cond != nullptr)
    return cond->to_string() +
           " / accuracy: " + std::to_string(cmat.accuracy()) +
           " / budget: " + std::to_string(ns_to_s(get_synthesis_budget()));
  else
    return "none";
}
SynthesisResult NumericCondition::synthesize_internal(
    bool is_pair, const std::set<Input>& pos_examples,
    const std::set<Input>& neg_examples) {
  std::chrono::steady_clock::time_point synthesis_start =
      std::chrono::steady_clock::now();

  std::unique_ptr<NumericCondition> cond_new, cond_new_sibling;
  cond_new = std::make_unique<NumericCondition>();
  cond_new->set_synthesis_budget(get_synthesis_budget());
  if (is_pair) {
    cond_new_sibling = std::make_unique<NumericCondition>();
    cond_new_sibling->set_synthesis_budget(get_synthesis_budget());
  }

  size_t sample_size = std::min(
      std::max(pos_examples.size(), neg_examples.size()), MAX_SAMPLE_SIZE);
  std::set<Input> pos_examples_sampled =
      random_sample(pos_examples, sample_size);
  std::set<Input> neg_examples_sampled =
      random_sample(neg_examples, sample_size);

  std::vector<std::unique_ptr<Constraint>> ctrs;
  for (auto pos_example : pos_examples_sampled)
    ctrs.push_back(std::make_unique<Constraint>(
        "f", CT_NUMERIC, pos_example.get_numeric_args(), true));
  for (auto neg_example : neg_examples_sampled)
    ctrs.push_back(std::make_unique<Constraint>(
        "f", CT_NUMERIC, neg_example.get_numeric_args(), false));

  std::unique_ptr<SygusFile> sfile =
      gen_sygus_file(CT_NUMERIC, std::move(ctrs));
  auto synthesizer_result =
      run_synthesizer(sfile->to_string(), ns_to_s(get_synthesis_budget()));

  const std::string DUET_FAIL_MSG_PREFIX = "Fatal error: exception";
  if (is_prefix_of(DUET_FAIL_MSG_PREFIX, synthesizer_result) ||
      synthesizer_result == "")  // TIMEOUT
    return std::make_tuple(FAIL, nullptr, nullptr,
                           elapsed_from_ns(synthesis_start));

  auto synthesized_cond = std::make_unique<BoolExpr>(
      simplify(*parse_fun(synthesizer_result)->get_body()));

  cond_new->cond = std::move(synthesized_cond);
  if (is_pair)
    cond_new_sibling->cond = std::make_unique<BoolExpr>(!(*(cond_new->cond)));

  return std::make_tuple(SUCCESS, std::move(cond_new),
                         std::move(cond_new_sibling),
                         elapsed_from_ns(synthesis_start));
}
double NumericCondition::dynamic_threshold() const {
  double threshold_min = COND_ACCURACY_THRESHOLD;
  double threshold_variable = accuracy_max - COND_ACCURACY_THRESHOLD;
  double budget_residual_ratio =
      (double)(get_synthesis_budget()) / (double)(synthesis_budget_max());
  return threshold_min + threshold_variable * budget_residual_ratio;
}

NeglectCondition::NeglectCondition() : BranchCondition(CT_NEGLECT) {}
bool NeglectCondition::operator==(const NeglectCondition& other) const {
  return BranchCondition::operator==(other);
}
bool NeglectCondition::is_accurate() const { return true; }
bool NeglectCondition::invalid() const { return false; }
bool NeglectCondition::eval(const Input& input, bool ground_truth) const {
  return true;
}
std::string NeglectCondition::to_string() const { return "NEGLECT"; }
SynthesisResult NeglectCondition::synthesize_internal(
    bool is_pair, const std::set<Input>& pos_examples,
    const std::set<Input>& neg_examples) {
  std::unique_ptr<NeglectCondition> cond_new, cond_new_sibling;
  cond_new = std::make_unique<NeglectCondition>();
  cond_new->set_synthesis_budget(get_synthesis_budget());
  if (is_pair) {
    cond_new_sibling = std::make_unique<NeglectCondition>();
    cond_new_sibling->set_synthesis_budget(get_synthesis_budget());
  }
  return std::make_tuple(SUCCESS, std::move(cond_new),
                         std::move(cond_new_sibling), 0);
}

CondType default_condtype() {
  if (enum_params_size() > 0)
    return CT_ENUM;
  else
    return CT_NUMERIC;
}

std::unique_ptr<BranchCondition> default_branch_condition() {
  if (default_condtype() == CT_ENUM)
    return std::make_unique<EnumCondition>();
  else
    return std::make_unique<NumericCondition>();
}

std::unique_ptr<BranchCondition> copy(
    const std::unique_ptr<BranchCondition>& other) {
  std::unique_ptr<BranchCondition> copied;
  if (auto* enum_cond = dynamic_cast<EnumCondition*>(other.get())) {
    copied = std::make_unique<EnumCondition>(*enum_cond);
  } else if (auto* numeric_cond =
                 dynamic_cast<NumericCondition*>(other.get())) {
    copied = std::make_unique<NumericCondition>(*numeric_cond);
  } else if (auto* neglect_cond =
                 dynamic_cast<NeglectCondition*>(other.get())) {
    copied = std::make_unique<NeglectCondition>(*neglect_cond);
  } else {
    throw Unreachable();
  }
  return copied;
}

}  // namespace pathfinder
