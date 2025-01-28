#include "numeric_solver.h"

#include <cassert>

#include "utils.h"

std::map<std::string, pathfinder::IntExpr> sym_int_arg;

namespace pathfinder {

/*
 *  Integer type argument and constraints.
 *  User provided constraints.
 *
 */
std::vector<std::unique_ptr<BoolExpr>> hard_constraints;
std::vector<std::unique_ptr<BoolExpr>> soft_constraints;

void register_sym_int_arg(std::string name) {
  sym_int_arg.insert({name, IntExpr(name)});
}

std::unique_ptr<z3::expr> conjunction(std::vector<z3::expr> ctrs) {
  if (ctrs.empty()) return nullptr;

  z3::expr res = ctrs[0];
  for (size_t i = 1; i < ctrs.size(); i++) res = res && ctrs[i];
  return std::make_unique<z3::expr>(res);
}

SolverVar::SolverVar(z3::context* ctx_, std::string name_)
    : name(name_), ctx(ctx_) {
  symbolic = std::make_unique<IntExpr>(name);
}
z3::expr SolverVar::get_z3_expr() const { return symbolic->to_z3_expr(*ctx); }
z3::expr SolverVar::basic_constraint() const {
  z3::expr exp = get_z3_expr();
  return ARG_INT_MIN <= exp && exp <= ARG_INT_MAX;
}
void SolverVar::assign(const z3::model& m) {
  z3::expr exp = get_z3_expr();
  concrete = m.eval(exp, true).get_numeral_int();
}
z3::expr SolverVar::current() const { return get_z3_expr() == (int)concrete; }
long SolverVar::get_concrete() const { return concrete; }

Solver::Solver() {
  ctx = std::make_unique<z3::context>();
  s = std::make_unique<z3::solver>(*ctx);
}
z3::context* Solver::get_ctx() const {
  assert(ctx.get() != nullptr);
  return ctx.get();
}
z3::solver* Solver::get_solver() const {
  assert(s.get() != nullptr);
  return s.get();
}
void Solver::clear_history() { history.reset(); }
void Solver::reset() {
  s->reset();
  clear_history();
}
void Solver::assign(const z3::model& m) {
  for (auto&& solver_var : solver_vars) solver_var->assign(m);
}
std::unique_ptr<z3::expr> Solver::current_assignment() {
  std::vector<z3::expr> current_assignments;
  for (auto&& solver_var : solver_vars)
    current_assignments.push_back(solver_var->current());
  return conjunction(current_assignments);
}
std::optional<Args> Solver::draw() {
  std::optional<Args> args_opt;

  s->push();
  if (history != nullptr) s->add(*history);

  if (s->check() != z3::sat) {
    args_opt = std::nullopt;
  } else {
    assign(s->get_model());
    args_opt = get_args();
    auto cur_assign = current_assignment();
    if (cur_assign != nullptr)
      history = history == nullptr
                    ? std::make_unique<z3::expr>(!(*cur_assign))
                    : std::make_unique<z3::expr>(*history && !(*cur_assign));
  }
  s->pop();

  return args_opt;
}
bool Solver::is_satisfiable() { return s->check() == z3::sat; }
Args Solver::get_args() {
  Args args;
  for (auto&& solver_var : solver_vars)
    args.insert({solver_var->name, solver_var->get_concrete()});
  return args;
}

NumericSolver::NumericSolver() : Solver() {
  std::vector<z3::expr> basic_ctrs;
  for (auto param : get_numeric_params()) {
    auto solver_var = std::make_unique<SolverVar>(get_ctx(), param.get_name());
    basic_ctrs.push_back(solver_var->basic_constraint());
    solver_vars.push_back(std::move(solver_var));
  }
  basic_constraint = conjunction(basic_ctrs);

  std::vector<z3::expr> hard_ctrs;
  for (auto& hard_ctr : hard_constraints)
    hard_ctrs.push_back(hard_ctr->to_z3_expr(*get_ctx()));
  hard_constraint = conjunction(hard_ctrs);

  std::vector<z3::expr> soft_ctrs;
  for (auto& soft_ctr : soft_constraints)
    soft_ctrs.push_back(soft_ctr->to_z3_expr(*get_ctx()));
  soft_constraint = conjunction(soft_ctrs);
}
void NumericSolver::reset(bool conform_soft) {
  Solver::reset();

  bool violating = std::rand() % 2 == 0;

  if (basic_constraint != nullptr) get_solver()->add(*basic_constraint);
  if (hard_constraint != nullptr) get_solver()->add(*hard_constraint);
  if (soft_constraint != nullptr) {
    if (conform_soft)
      get_solver()->add(*soft_constraint);
    else
      get_solver()->add(!(*soft_constraint));
  }
}
void NumericSolver::set_condition(
    std::vector<NumericCondition*> numeric_conditions, bool conform_soft) {
  reset(conform_soft);

  std::unique_ptr<BoolExpr> numeric_cond =
      std::make_unique<BoolExpr>(BoolExpr::true_expr());
  for (auto& numeric_condition : numeric_conditions)
    if (!numeric_condition->invalid())
      numeric_cond = std::make_unique<BoolExpr>(*numeric_cond &&
                                                *(numeric_condition->cond));
  get_solver()->add(numeric_cond->to_z3_expr(*get_ctx()));
}
z3::expr NumericSolver::rand_constraint() {
  int num_args = solver_vars.size();
  int first = std::rand() % num_args;
  int second = (first + (std::rand() % (num_args - 1)) + 1) % num_args;
  int NUM_RELOP = MUTOP_LAST - MUTOP_FIRST;
  Mutation_Op op = static_cast<Mutation_Op>(std::rand() % NUM_RELOP + 1);
  z3::expr constraint = z3::expr(*get_ctx());
  switch (op) {
    case MUTOP_EQ:
      constraint = solver_vars[first]->get_z3_expr() ==
                   solver_vars[second]->get_z3_expr();
      break;
    case MUTOP_NEQ:
      constraint = solver_vars[first]->get_z3_expr() !=
                   solver_vars[second]->get_z3_expr();
      break;
    case MUTOP_LT:
      constraint = solver_vars[first]->get_z3_expr() <
                   solver_vars[second]->get_z3_expr();
      break;
    case MUTOP_LTE:
      constraint = solver_vars[first]->get_z3_expr() <=
                   solver_vars[second]->get_z3_expr();
      break;
    default:
      throw Unreachable();
  }
  return constraint;
}
std::optional<Args> NumericSolver::draw() {
  std::optional<Args> args_opt;

  if (rand_float() < MUT_RATE && solver_vars.size() > 1) {
    get_solver()->push();
    get_solver()->add(rand_constraint());
    args_opt = Solver::draw();
    get_solver()->pop();

    if (args_opt.has_value()) return args_opt.value();
  }

  args_opt = Solver::draw();

  if (!args_opt.has_value()) {
    clear_history();
    args_opt = Solver::draw();
  }

  return args_opt;
}

}  // namespace pathfinder
