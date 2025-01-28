#ifndef PATHFINDER_INPUT_GENERATOR
#define PATHFINDER_INPUT_GENERATOR

#include "enum_solver.h"
#include "numeric_solver.h"
#include "pathfinder_defs.h"

namespace pathfinder {

class InputGenerator {
 public:
  InputGenerator();
  void set_condition(std::vector<EnumCondition*> enum_conditions,
                     std::vector<NumericCondition*> numeric_conditions);
  std::optional<Input> gen();

 private:
  std::unique_ptr<EnumSolver> enum_solver;
  std::unique_ptr<NumericSolver> numeric_solver;
};

}  // namespace pathfinder

#endif
