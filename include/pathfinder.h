#ifndef PATHFINDER
#define PATHFINDER

#include "enum_solver.h"
#include "numeric_solver.h"
#include "trace_pc.h"

#define PathFinderExecuteTarget(...) \
  pathfinder::TPC().TraceOn();       \
  pathfinder::TPC().ClearPathLog();  \
  __VA_ARGS__;                       \
  pathfinder::TPC().TraceOff()

#define PathFinderPassIf(cond) \
  if (!is_initial_seed && cond) return -1

extern bool is_initial_seed;

void PathFinderEnumArg(std::string name, std::vector<std::string> entries);
void PathFinderEnumArg(std::string name, size_t start, size_t size);
void PathFinderEnumArg(std::string name, size_t size);
void PathFinderIntArg(std::string name);
void PathFinderAddHardConstraint(pathfinder::BoolExpr ctr);
void PathFinderAddHardConstraint(std::vector<pathfinder::BoolExpr> ctrs);
void PathFinderAddSoftConstraint(pathfinder::BoolExpr ctr);
void PathFinderAddSoftConstraint(std::vector<pathfinder::BoolExpr> ctrs);

int pathfinder::driver(UserCallback Callback);

#endif
