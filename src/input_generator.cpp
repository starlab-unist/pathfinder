#include "input_generator.h"

namespace pathfinder {

InputGenerator::InputGenerator() {
  enum_solver = std::make_unique<EnumSolver>();
  numeric_solver = std::make_unique<NumericSolver>();
}
void InputGenerator::set_condition(
    std::vector<EnumCondition*> enum_conditions,
    std::vector<NumericCondition*> numeric_conditions) {
  bool conform_soft = std::rand() % 2 == 0;
  enum_solver->set_condition(enum_conditions);
  numeric_solver->set_condition(numeric_conditions, conform_soft);
}
std::optional<Input> InputGenerator::gen() {
  auto enum_args = enum_solver->draw();
  auto numeric_args = numeric_solver->draw();

  if (!enum_args.has_value() || !numeric_args.has_value()) return std::nullopt;

  return Input(enum_args.value(), numeric_args.value());
}

}  // namespace pathfinder
