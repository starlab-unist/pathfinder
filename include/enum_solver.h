#ifndef PATHFINDER_ENUM_SOLVER
#define PATHFINDER_ENUM_SOLVER

#include <optional>

#include "branch_condition.h"
#include "input_signature.h"
#include "pathfinder_defs.h"
#include "sygus_ast.h"
#include "utils.h"

namespace pathfinder {

void register_sym_enum_arg(std::string name);

extern std::vector<std::unique_ptr<pathfinder::BoolExpr>> enum_constraints;

class EqualityGraph;

class EqualSet {
 public:
  EqualSet(std::string param, EnumArgBitVec candidates_);
  bool operator<(const EqualSet& other) const;
  void merge(EqualSet* other);
  void connect(EqualSet* other);
  bool has_sole_candidate() const;
  void exclude();
  void detach(EqualSet* other);
  void unset_assignment();
  bool pick();
  Args draw();

 private:
  std::set<std::string> params;
  EnumArgBitVec candidates;
  std::set<EqualSet*> inequal_sets;

  std::optional<EnumArgBitVec> assignment;
  std::vector<EqualSet*> traversed;

  friend EqualityGraph;
};

class EqualityGraph {
 public:
  EqualityGraph(std::vector<std::string> params,
                EnumArgBitVecArray const_equality_bvs,
                std::vector<EqualityCondition> param_equality_conds);
  std::optional<Args> draw();

 private:
  std::set<std::unique_ptr<EqualSet>> eqsets;
  std::map<std::string, EqualSet*> param_to_eqset;

  void merge(std::string param_l, std::string param_r);
  void connect(std::string param_l, std::string param_r);
  void simplify();
};

class EnumGroupSolver {
 public:
  EnumGroupSolver(std::vector<std::string> params_);
  void set_condition(EnumArgBitVecArray const_equality_bvs,
                     std::vector<EqualityCondition> param_equality_conds);
  std::optional<Args> draw();

 private:
  std::vector<std::string> params;
  std::unique_ptr<EqualityGraph> eqgraph;
};

class EnumSolver {
 public:
  EnumSolver();
  void set_condition(std::vector<EnumCondition*> enum_conditions);
  std::optional<Args> draw();

 private:
  std::map<std::string, size_t> param_to_group_idx;
  std::vector<std::unique_ptr<EnumGroupSolver>> solvers;
};

}  // namespace pathfinder

#endif
