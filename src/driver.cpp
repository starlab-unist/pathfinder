#include <iostream>

#include "engine.h"
#include "enum_solver.h"
#include "enumarg_bitvec.h"
#include "numeric_solver.h"
#include "options.h"
#include "pathfinder_defs.h"
#include "trace_pc.h"
#include "utils.h"

extern "C" {
// Below functions should be defined by the user.
void PathFinderSetup();
int PathFinderTestOneInput(const pathfinder::Input& input);
}  // extern "C"

void PathFinderEnumArg(std::string name, std::vector<std::string> entries) {
  pathfinder::register_enum_param(name, entries);
  pathfinder::register_enum_bv(name, entries);
}
void PathFinderEnumArg(std::string name, size_t start, size_t size) {
  pathfinder::register_enum_param(name, start, size);
  pathfinder::register_enum_bv(name, start, size);
}
void PathFinderEnumArg(std::string name, size_t size) {
  pathfinder::register_enum_param(name, 0, size);
  pathfinder::register_enum_bv(name, 0, size);
}
void PathFinderIntArg(std::string name) {
  pathfinder::register_int_param(name);
  pathfinder::register_sym_int_arg(name);
}
void PathFinderAddHardConstraint(pathfinder::BoolExpr ctr) {
  pathfinder::hard_constraints.push_back(
      std::make_unique<pathfinder::BoolExpr>(ctr));
}
void PathFinderAddHardConstraint(std::vector<pathfinder::BoolExpr> ctrs) {
  for (auto ctr : ctrs)
    pathfinder::hard_constraints.push_back(
        std::make_unique<pathfinder::BoolExpr>(ctr));
}
void PathFinderAddSoftConstraint(pathfinder::BoolExpr ctr) {
  pathfinder::soft_constraints.push_back(
      std::make_unique<pathfinder::BoolExpr>(ctr));
}
void PathFinderAddSoftConstraint(std::vector<pathfinder::BoolExpr> ctrs) {
  for (auto ctr : ctrs)
    pathfinder::soft_constraints.push_back(
        std::make_unique<pathfinder::BoolExpr>(ctr));
}

namespace pathfinder {

void add_cmd_line_constraint() {
  if (CMD_LINE_CONSTRAINT == "") return;

  std::vector<std::string> constraints = split_all(CMD_LINE_CONSTRAINT, ',');
  for (auto constraint : constraints) {
    std::string lhs, comp, rhs;
    std::tie(lhs, comp, rhs) = split_comp(constraint);
    PATHFINDER_CHECK(is_prefix_of("arg", lhs),
                     "PathFinder Error: Invalid argument name `" + lhs +
                         "` in \'--constraint\'");

    size_t arg_idx = (size_t)stoi(lhs.substr(3));
    PATHFINDER_CHECK(arg_idx < params_size(),
                     "PathFinder Error: Invalid argument name `" + lhs +
                         "` in \'--constraint\'");

    if (!is_number(rhs)) {
      if (comp == "==")
        hard_constraints.push_back(std::make_unique<pathfinder::BoolExpr>(
            sym_int_arg[lhs] == sym_int_arg[rhs]));
      else if (comp == "!=")
        hard_constraints.push_back(std::make_unique<pathfinder::BoolExpr>(
            sym_int_arg[lhs] != sym_int_arg[rhs]));
      else if (comp == ">=")
        hard_constraints.push_back(std::make_unique<pathfinder::BoolExpr>(
            sym_int_arg[lhs] >= sym_int_arg[rhs]));
      else if (comp == "<=")
        hard_constraints.push_back(std::make_unique<pathfinder::BoolExpr>(
            sym_int_arg[lhs] <= sym_int_arg[rhs]));
      else if (comp == ">")
        hard_constraints.push_back(std::make_unique<pathfinder::BoolExpr>(
            sym_int_arg[lhs] > sym_int_arg[rhs]));
      else if (comp == "<")
        hard_constraints.push_back(std::make_unique<pathfinder::BoolExpr>(
            sym_int_arg[lhs] < sym_int_arg[rhs]));
      else
        PATHFINDER_CHECK(false, "PathFinder Error: Invalid comparator `" +
                                    comp + "` in \'--constraint\'");
    } else {
      size_t value = stoi(rhs);
      if (comp == "==")
        hard_constraints.push_back(
            std::make_unique<pathfinder::BoolExpr>(sym_int_arg[lhs] == value));
      else if (comp == "!=")
        hard_constraints.push_back(
            std::make_unique<pathfinder::BoolExpr>(sym_int_arg[lhs] != value));
      else if (comp == ">=")
        hard_constraints.push_back(
            std::make_unique<pathfinder::BoolExpr>(sym_int_arg[lhs] >= value));
      else if (comp == "<=")
        hard_constraints.push_back(
            std::make_unique<pathfinder::BoolExpr>(sym_int_arg[lhs] <= value));
      else if (comp == ">")
        hard_constraints.push_back(
            std::make_unique<pathfinder::BoolExpr>(sym_int_arg[lhs] > value));
      else if (comp == "<")
        hard_constraints.push_back(
            std::make_unique<pathfinder::BoolExpr>(sym_int_arg[lhs] < value));
      else
        PATHFINDER_CHECK(false, "PathFinder Error: Invalid comparator `" +
                                    comp + "` in \'--constraint\'");
    }
  }
}

void PathFinderInit() {
  check_duet();

  PATHFINDER_CHECK(params_size() >= 1,
                   "PathFinder Error: Arg size is not set up properly");

  NumericSolver solver;
  solver.set_condition({}, true);
  PATHFINDER_CHECK(
      solver.is_satisfiable(),
      "PathFinder Error: Provided initial constraint is not satisfiable");
}

int driver(UserCallback Callback) {
  start_time = std::chrono::steady_clock::now();

  PathFinderSetup();
  add_cmd_line_constraint();
  PathFinderInit();

  prepare_random_seed();
  if (CMD_LINE_INPUT.size() == 0) prepare_corpus();

  Engine engine(Callback, params_size(), start_time, MAX_TOTAL_TIME,
                MAX_TOTAL_GEN, &TPC());

  if (CMD_LINE_INPUT.size() != 0) {
    engine.run_cmd_input();
    std::cout << doubleline() << "Running command-line input \""
              << CMD_LINE_INPUT << "\" done.\n"
              << singleline() << engine.to_string() << doubleline();
    exit(0);
  }

  if (RUN_ONLY && !COV_OUTPUT_FILENAME.empty()) {
    engine.run_corpus_and_output_cov();
    exit(0);
  }
  int num_starting_intput = engine.run_corpus();
  if (RUN_ONLY) {
    std::cout << doubleline() << "Running corpus with "
              << std::to_string(num_starting_intput) << " inputs done in "
              << std::to_string(elapsed_from_s(start_time)) << " seconds.\n"
              << singleline() << engine.to_string() << doubleline();
    exit(0);
  }

  // synthesize branch conditions from runned inputs.
  if (num_starting_intput > 0) {
    engine.synthesize_all();
    std::cout << doubleline() << "Initialization with "
              << std::to_string(num_starting_intput) << " inputs done in "
              << std::to_string(elapsed_from_s(start_time)) << " seconds.\n"
              << singleline() << engine.to_string() << doubleline();
  }

  engine.reset_counter();
  engine.warmingup();

  int num_iter = 1;
  while (num_iter <= MAX_ITER) {
    engine.run();
    num_iter++;
  }

  return 0;
}

}  // namespace pathfinder
