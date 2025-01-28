#ifndef PATHFINDER_SOLVER
#define PATHFINDER_SOLVER

#include <optional>

#include "branch_condition.h"
#include "input_signature.h"
#include "options.h"
#include "pathfinder_defs.h"
#include "sygus_ast.h"

extern std::map<std::string, pathfinder::IntExpr> sym_int_arg;

namespace pathfinder {

void register_sym_int_arg(std::string name);

extern std::vector<std::unique_ptr<pathfinder::BoolExpr>> hard_constraints;
extern std::vector<std::unique_ptr<pathfinder::BoolExpr>> soft_constraints;

class SolverVar {
 public:
  SolverVar(z3::context* ctx_, std::string name_);
  z3::expr get_z3_expr() const;
  z3::expr basic_constraint() const;
  void assign(const z3::model& m);
  z3::expr current() const;
  long get_concrete() const;

 private:
  std::string name;
  std::unique_ptr<IntExpr> symbolic;
  long concrete;

  z3::context* ctx;

  friend class Solver;
};

enum Mutation_Op {
  MUTOP_NONE,
  MUTOP_EQ,
  MUTOP_NEQ,
  MUTOP_LT,
  MUTOP_LTE,

  MUTOP_FIRST = MUTOP_NONE,
  MUTOP_LAST = MUTOP_LTE,
};

class Solver {
 public:
  Solver();
  bool is_satisfiable();

 protected:
  z3::context* get_ctx() const;
  z3::solver* get_solver() const;
  void clear_history();
  void reset();
  void assign(const z3::model& m);
  std::unique_ptr<z3::expr> current_assignment();
  std::optional<Args> draw();
  Args get_args();
  std::vector<std::unique_ptr<SolverVar>> solver_vars;
  std::unique_ptr<z3::expr> history;

 private:
  std::unique_ptr<z3::context> ctx;
  std::unique_ptr<z3::solver> s;
};

class NumericSolver : public Solver {
 public:
  NumericSolver();
  void set_condition(std::vector<NumericCondition*> numeric_conditions,
                     bool conform_soft);
  std::optional<Args> draw();

 private:
  void reset(bool conform_soft);
  z3::expr rand_constraint();

  std::unique_ptr<z3::expr> basic_constraint;
  std::unique_ptr<z3::expr> hard_constraint;
  std::unique_ptr<z3::expr> soft_constraint;
};

}  // namespace pathfinder

#endif
