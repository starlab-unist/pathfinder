#include "enum_solver.h"

namespace pathfinder {

void register_sym_enum_arg(std::string name);

extern std::vector<std::unique_ptr<pathfinder::BoolExpr>> enum_constraints;

EqualSet::EqualSet(std::string param, EnumArgBitVec candidates_)
    : params({param}), candidates(candidates_) {}
bool EqualSet::operator<(const EqualSet& other) const {
  assert(!params.empty() && !other.params.empty());
  return *params.begin() < *other.params.begin();
}
void EqualSet::merge(EqualSet* other) {
  if (inequal_sets.find(other) != inequal_sets.end() ||
      other->inequal_sets.find(this) != other->inequal_sets.end())
    PATHFINDER_CHECK(false, "Enum solver Unsat");

  for (auto inequal_set : other->inequal_sets) {
    inequal_set->inequal_sets.erase(other);
    inequal_set->inequal_sets.insert(this);
  }

  params.insert(other->params.begin(), other->params.end());
  candidates.bit_and(other->candidates);
  PATHFINDER_CHECK(!candidates.empty(), "Enum solver Unsat");
  inequal_sets.insert(other->inequal_sets.begin(), other->inequal_sets.end());
}
void EqualSet::connect(EqualSet* other) {
  for (auto& param : params)
    if (other->params.find(param) != other->params.end())
      PATHFINDER_CHECK(false, "Enum solver Unsat");
  for (auto& param : other->params)
    if (params.find(param) != params.end())
      PATHFINDER_CHECK(false, "Enum solver Unsat");

  inequal_sets.insert(other);
  other->inequal_sets.insert(this);
}
bool EqualSet::has_sole_candidate() const {
  return candidates.num_set_bit() == 1;
}
void EqualSet::exclude() {
  assert(candidates.num_set_bit() == 1);
  for (auto& inequal_set : inequal_sets) {
    inequal_set->candidates.exclude(candidates);
    inequal_set->inequal_sets.erase(this);
  }
  inequal_sets.clear();
}
void EqualSet::detach(EqualSet* other) {
  assert(candidates.exclusive(other->candidates));
  inequal_sets.erase(other);
  other->inequal_sets.erase(this);
}
void EqualSet::unset_assignment() {
  assignment = std::nullopt;
  for (auto& inequal_set : traversed) inequal_set->unset_assignment();
  traversed.clear();
}
bool EqualSet::pick() {
  if (assignment.has_value()) return true;

  std::vector<EqualSet*> fixed;
  std::vector<EqualSet*> to_be_fixed;
  for (auto& inequal_set : inequal_sets) {
    if (inequal_set->assignment.has_value())
      fixed.push_back(inequal_set);
    else
      to_be_fixed.push_back(inequal_set);
  }

  EnumArgBitVec to_be_excluded = candidates;
  to_be_excluded.unset_all();
  for (auto& inequal_set : fixed)
    to_be_excluded.bit_or(inequal_set->assignment.value());
  EnumArgBitVec excluded(candidates);
  excluded.exclude(to_be_excluded);

  EnumArgBitVec tried = candidates;
  tried.unset_all();
  auto picked_opt = excluded.extract_random_bit();

  while (picked_opt.has_value()) {
    assignment = picked_opt;
    tried.bit_or(assignment.value());

    bool success = true;
    for (auto& inequal_set : to_be_fixed) {
      if (!inequal_set->pick()) {
        success = false;
        break;
      } else {
        traversed.push_back(inequal_set);
      }
    }

    if (success) return true;

    unset_assignment();
    excluded.exclude(tried);
    picked_opt = excluded.extract_random_bit();
  }
  unset_assignment();
  return false;
}
Args EqualSet::draw() {
  assert(assignment.has_value());
  auto value_opt = assignment.value().draw();
  assert(value_opt.has_value());
  auto value = value_opt.value();

  Args args;
  for (auto& param : params) args[param] = value;

  return args;
}

EqualityGraph::EqualityGraph(
    std::vector<std::string> params, EnumArgBitVecArray const_equality_bvs,
    std::vector<EqualityCondition> param_equality_conds) {
  std::map<std::string, EnumArgBitVec> const_equality_bv_map;
  for (size_t i = 0; i < const_equality_bvs.size(); i++) {
    std::string param = const_equality_bvs[i].get_name();
    const_equality_bv_map[param] = const_equality_bvs[i];
  }
  for (auto& param : params) {
    auto eqset =
        std::make_unique<EqualSet>(param, const_equality_bv_map[param]);
    assert(param_to_eqset.find(param) == param_to_eqset.end());
    param_to_eqset[param] = eqset.get();
    eqsets.insert(std::move(eqset));
  }
  std::vector<EqualityCondition> equals;
  std::vector<EqualityCondition> inequals;
  for (auto& param_equality_cond : param_equality_conds) {
    if (param_equality_cond.get_eqtype() == ET_Equal)
      equals.push_back(param_equality_cond);
    else
      inequals.push_back(param_equality_cond);
  }
  for (auto& equal : equals) merge(equal.get_left(), equal.get_right());
  for (auto& inequal : inequals)
    connect(inequal.get_left(), inequal.get_right());
  simplify();
}
std::optional<Args> EqualityGraph::draw() {
  for (auto& eqset : eqsets) eqset->unset_assignment();
  for (auto& eqset : eqsets)
    if (!eqset->pick()) return std::nullopt;
  Args enum_args;
  for (auto& eqset : eqsets) {
    auto args = eqset->draw();
    enum_args.insert(args.begin(), args.end());
  }
  return enum_args;
}
void EqualityGraph::merge(std::string param_l, std::string param_r) {
  if (param_l == param_r) return;
  EqualSet* eqset_l = param_to_eqset[param_l];
  EqualSet* eqset_r = param_to_eqset[param_r];
  if (eqset_l == eqset_r) return;
  eqset_l->merge(eqset_r);
  auto it = eqsets.begin();
  for (; it != eqsets.end(); it++)
    if (it->get() == eqset_r) break;
  eqsets.erase(it);
  param_to_eqset[param_r] = eqset_l;
}
void EqualityGraph::connect(std::string param_l, std::string param_r) {
  PATHFINDER_CHECK(param_l != param_r, "Enum solver Unsat");

  EqualSet* eqset_l = param_to_eqset[param_l];
  EqualSet* eqset_r = param_to_eqset[param_r];
  eqset_l->connect(eqset_r);
}
void EqualityGraph::simplify() {
  for (auto& eqset : eqsets)
    if (eqset->has_sole_candidate()) eqset->exclude();
  for (auto& eqset : eqsets) {
    // separate detecting and detaching eqsets using `to_be_detached`,
    // to prevent modifying `eqset->inequal_sets` while iterating it.
    std::vector<EqualSet*> to_be_detached;
    for (auto& inequal_eqset : eqset->inequal_sets)
      if (eqset->candidates.exclusive(inequal_eqset->candidates))
        to_be_detached.push_back(inequal_eqset);
    for (auto& inequal_eqset : to_be_detached) eqset->detach(inequal_eqset);
  }
}

EnumGroupSolver::EnumGroupSolver(std::vector<std::string> params_)
    : params(params_) {}
void EnumGroupSolver::set_condition(
    EnumArgBitVecArray const_equality_bvs,
    std::vector<EqualityCondition> param_equality_conds) {
  eqgraph = std::make_unique<EqualityGraph>(params, const_equality_bvs,
                                            param_equality_conds);
}
std::optional<Args> EnumGroupSolver::draw() { return eqgraph->draw(); }

EnumSolver::EnumSolver() {
  auto enum_param_groups = get_enum_param_groups();
  for (size_t i = 0; i < enum_param_groups.size(); i++) {
    std::vector<std::string> params;
    for (auto& param : enum_param_groups[i]) {
      params.push_back(param.get_name());
      param_to_group_idx[param.get_name()] = i;
    }
    solvers.push_back(std::make_unique<EnumGroupSolver>(params));
  }
}
void EnumSolver::set_condition(std::vector<EnumCondition*> enum_conditions) {
  std::vector<std::vector<EqualityCondition>> param_equality_conds(
      solvers.size());
  EnumArgBitVecArray const_equality_bvs = initial_enum_bvs(false);
  for (auto& enum_condition : enum_conditions) {
    if (auto inclusion_cond = enum_condition->get_inclusion_cond()) {
      auto bv = inclusion_cond.value();
      if (!bv.empty()) const_equality_bvs.bit_or(bv);
    } else if (auto equality_cond = enum_condition->get_equality_cond()) {
      EqualityCondition eqcond = to_equality_condition(*equality_cond);
      assert(param_to_group_idx.find(eqcond.get_left()) !=
             param_to_group_idx.end());
      assert(param_to_group_idx.find(eqcond.get_right()) !=
             param_to_group_idx.end());
      size_t group_idx = param_to_group_idx.at(eqcond.get_left());
      assert(group_idx == param_to_group_idx.at(eqcond.get_right()));
      param_equality_conds[group_idx].push_back(eqcond);
    }
  }
  const_equality_bvs.negate();

  std::vector<std::vector<std::unique_ptr<EnumArgBitVec>>>
      const_equality_bv_arrays(solvers.size());
  for (size_t i = 0; i < const_equality_bvs.size(); i++) {
    auto bv = const_equality_bvs[i];
    assert(param_to_group_idx.find(bv.get_name()) != param_to_group_idx.end());
    size_t group_idx = param_to_group_idx.at(bv.get_name());
    const_equality_bv_arrays[group_idx].push_back(
        std::make_unique<EnumArgBitVec>(bv));
  }
  for (size_t group_idx = 0; group_idx < solvers.size(); group_idx++)
    solvers[group_idx]->set_condition(
        std::move(const_equality_bv_arrays[group_idx]),
        param_equality_conds[group_idx]);
}
std::optional<Args> EnumSolver::draw() {
  Args enum_args;
  for (auto& solver : solvers) {
    auto args_opt = solver->draw();
    if (!args_opt.has_value()) return std::nullopt;
    Args args = args_opt.value();
    enum_args.insert(args.begin(), args.end());
  }
  assert(enum_args.size() == enum_params_size());
  return enum_args;
}

}  // namespace pathfinder
