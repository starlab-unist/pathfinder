#include "sygus_gen.h"

#include "input_signature.h"
#include "utils.h"

namespace pathfinder {

SetLogic::SetLogic(SupportedLogic logic_) : logic(logic_) {}
std::string SetLogic::to_string() const {
  std::string str = "(set-logic ";
  switch (logic) {
    case SYGUS_LOGIC_LIA:
      str += "LIA";
      break;
    default:
      throw Unreachable();
  }
  str += ")";
  return str;
}

ProductionRule::ProductionRule(std::string symbol_,
                               std::vector<std::unique_ptr<IntExpr>> irhs_)
    : symbol(symbol_), t(SYGUS_VALUE_TYPE_INT), irhs(std::move(irhs_)) {}
ProductionRule::ProductionRule(std::string symbol_,
                               std::vector<std::unique_ptr<BoolExpr>> brhs_)
    : symbol(symbol_), t(SYGUS_VALUE_TYPE_BOOL), brhs(std::move(brhs_)) {}
std::string ProductionRule::to_string(int depth) const {
  std::string str = indent(depth) + "(";
  str += symbol + " ";
  switch (t) {
    case SYGUS_VALUE_TYPE_INT:
      str += "Int (\n";
      str += indent(depth + 1);
      for (size_t i = 0; i < irhs.size(); i++) {
        str += irhs[i]->to_string();
        if (i != irhs.size() - 1) {
          str += " ";
        }
      }
      str += "))";
      break;
    case SYGUS_VALUE_TYPE_BOOL:
      str += "Bool (\n";
      str += indent(depth + 1);
      for (size_t i = 0; i < brhs.size(); i++) {
        str += brhs[i]->to_string();
        if (i != brhs.size() - 1) {
          str += " ";
        }
      }
      str += "))";
      break;
    default:
      throw Unreachable();
  }
  return str;
}

FunSpec::FunSpec(std::string name_, std::vector<std::unique_ptr<Param>> params_,
                 std::vector<std::unique_ptr<ProductionRule>> rules_)
    : name(name_), params(std::move(params_)), rules(std::move(rules_)) {}
std::string FunSpec::to_string() const {
  std::string str = "(synth-fun ";
  str += name + "\n\n";

  str += indent(1) + ";; Parameters and return type\n";
  str += indent(1) + "(";
  for (size_t i = 0; i < params.size(); i++) {
    str += "(";
    str += params[i]->to_string() +
           " Int)";  // For now, parameters are always integral.
    if (i != params.size() - 1) {
      str += " ";
    }
  }
  str += ") Bool\n\n";  // For now, return type is always boolean.

  str += indent(1) + ";; Define the syntax\n";
  str += indent(1) + "(\n";
  for (auto&& rule : rules) {
    str += rule->to_string(2) + "\n";
  }
  str += indent(1) + ")\n";
  str += ")\n";

  return str;
}

Constraint::Constraint(std::string fname_, CondType condtype_,
                       const Args& args_, bool result_)
    : fname(fname_), condtype(condtype_), args(args_), result(result_) {}
std::string Constraint::to_string() const {
  std::string str = "(constraint (= (";
  str += fname + " ";
  for (size_t i = 0; i < args.size(); i++) {
    str += condtype == CT_ENUM ? std::to_string(enum_value_at(args, i))
                               : std::to_string(numeric_value_at(args, i));
    if (i != args.size() - 1) str += " ";
  }
  str += ") ";
  if (result) {
    str += "true";
  } else {
    str += "false";
  }
  str += "))";
  return str;
}

SygusFile::SygusFile(std::unique_ptr<SetLogic> setlogic_,
                     std::unique_ptr<FunSpec> funspec_,
                     std::vector<std::unique_ptr<Constraint>> constraints_)
    : setlogic(std::move(setlogic_)),
      funspec(std::move(funspec_)),
      constraints(std::move(constraints_)) {}
std::string SygusFile::to_string() const {
  std::string str = ";; Background theory\n";
  str += setlogic->to_string() + "\n\n";

  str += ";; Spec of the function to be synthesized\n";
  str += funspec->to_string() + "\n";

  str += ";; Input-Output examples\n";
  for (auto&& constraint : constraints) {
    str += constraint->to_string() + "\n";
  }
  str += "\n(check-synth)\n";
  return str;
}

std::unique_ptr<SetLogic> default_setlogic() {
  return std::make_unique<SetLogic>(SYGUS_LOGIC_LIA);
}

const std::string start_symbol = "Start";
const std::string bool_symbol = "BoolExpr";
const std::string int_symbol = "IntExpr";
const std::string const_symbol = "ConstExpr";
const std::string var_symbol = "VarExpr";
const std::set<int> default_literals = {0, 1, 2, 3, 4, 5};

std::vector<std::unique_ptr<ProductionRule>> rule_enum() {
  std::vector<std::unique_ptr<ProductionRule>> rules;

  std::vector<std::unique_ptr<BoolExpr>> start_rule_rhs;
  start_rule_rhs.push_back(std::make_unique<BoolExpr>(bool_symbol));
  start_rule_rhs.push_back(std::make_unique<BoolExpr>(
      BOOLEXPR_NOT, std::make_unique<BoolExpr>(bool_symbol)));

  std::vector<std::unique_ptr<BoolExpr>> bool_rule_rhs;
  std::vector<std::unique_ptr<ProductionRule>> int_rules;
  auto enum_param_groups = get_enum_param_groups();
  for (size_t i = 0; i < enum_param_groups.size(); i++) {
    std::string enumtype_symbol = "EnumType_" + std::to_string(i);
    bool_rule_rhs.push_back(std::make_unique<BoolExpr>(
        BOOLEXPR_EQ, std::make_unique<IntExpr>(enumtype_symbol),
        std::make_unique<IntExpr>(enumtype_symbol)));
    std::vector<std::unique_ptr<IntExpr>> int_rule_rhs;
    for (auto& enum_param : enum_param_groups[i])
      int_rule_rhs.push_back(std::make_unique<IntExpr>(enum_param.get_name()));
    int_rules.push_back(std::make_unique<ProductionRule>(
        enumtype_symbol, std::move(int_rule_rhs)));
  }

  rules.push_back(std::make_unique<ProductionRule>(start_symbol,
                                                   std::move(start_rule_rhs)));
  rules.push_back(
      std::make_unique<ProductionRule>(bool_symbol, std::move(bool_rule_rhs)));
  for (auto& int_rule : int_rules) rules.push_back(std::move(int_rule));

  return rules;
}

const std::string bool_symbol0 = "BoolExpr0";
const std::string bool_symbol1 = "BoolExpr1";
const std::string int_symbol0 = "IntExpr0";
const std::string int_symbol1 = "IntExpr1";
const std::string int_symbol2 = "IntExpr2";

std::unique_ptr<ProductionRule> const_rule() {
  std::set<int> literals = default_literals;
  std::vector<int> literals_sorted(literals.begin(), literals.end());
  std::sort(literals_sorted.begin(), literals_sorted.end());

  std::vector<std::unique_ptr<IntExpr>> crhs;
  for (const auto& value : literals_sorted) {
    crhs.push_back(std::make_unique<IntExpr>(value));
  }

  return std::make_unique<ProductionRule>(const_symbol, std::move(crhs));
}
std::unique_ptr<ProductionRule> var_rule() {
  std::vector<std::unique_ptr<IntExpr>> rhs;
  for (auto& param : get_numeric_params())
    rhs.push_back(std::make_unique<IntExpr>(param.get_name()));

  return std::make_unique<ProductionRule>(var_symbol, std::move(rhs));
}

std::unique_ptr<ProductionRule> start_rule_numeric_linear() {
  std::vector<std::unique_ptr<BoolExpr>> brhs;
  brhs.push_back(std::make_unique<BoolExpr>(bool_symbol0));

  return std::make_unique<ProductionRule>(start_symbol, std::move(brhs));
}
std::unique_ptr<ProductionRule> bool_rule0_numeric_linear() {
  std::vector<std::unique_ptr<BoolExpr>> brhs;
  brhs.push_back(std::make_unique<BoolExpr>(bool_symbol1));
  brhs.push_back(std::make_unique<BoolExpr>(
      BOOLEXPR_AND, std::make_unique<BoolExpr>(bool_symbol1),
      std::make_unique<BoolExpr>(bool_symbol1)));
  brhs.push_back(std::make_unique<BoolExpr>(
      BOOLEXPR_OR, std::make_unique<BoolExpr>(bool_symbol1),
      std::make_unique<BoolExpr>(bool_symbol1)));
  brhs.push_back(std::make_unique<BoolExpr>(
      BOOLEXPR_NOT, std::make_unique<BoolExpr>(bool_symbol1)));

  return std::make_unique<ProductionRule>(bool_symbol0, std::move(brhs));
}
std::unique_ptr<ProductionRule> bool_rule1_numeric_linear() {
  std::vector<std::unique_ptr<BoolExpr>> brhs;
  brhs.push_back(std::make_unique<BoolExpr>(
      BOOLEXPR_EQ, std::make_unique<IntExpr>(int_symbol0),
      std::make_unique<IntExpr>(int_symbol0)));
  brhs.push_back(std::make_unique<BoolExpr>(
      BOOLEXPR_LT, std::make_unique<IntExpr>(int_symbol0),
      std::make_unique<IntExpr>(int_symbol0)));
  brhs.push_back(std::make_unique<BoolExpr>(
      BOOLEXPR_LTE, std::make_unique<IntExpr>(int_symbol0),
      std::make_unique<IntExpr>(int_symbol0)));

  return std::make_unique<ProductionRule>(bool_symbol1, std::move(brhs));
}
std::unique_ptr<ProductionRule> int_rule0_numeric_linear() {
  std::vector<std::unique_ptr<IntExpr>> irhs;
  irhs.push_back(std::make_unique<IntExpr>(int_symbol1));
  irhs.push_back(std::make_unique<IntExpr>(
      INTEXPR_ADD, std::make_unique<IntExpr>(int_symbol0),
      std::make_unique<IntExpr>(int_symbol0)));
  irhs.push_back(std::make_unique<IntExpr>(
      INTEXPR_SUB, std::make_unique<IntExpr>(int_symbol0),
      std::make_unique<IntExpr>(int_symbol0)));

  return std::make_unique<ProductionRule>(int_symbol0, std::move(irhs));
}
std::unique_ptr<ProductionRule> int_rule1_numeric_linear() {
  std::vector<std::unique_ptr<IntExpr>> irhs;
  irhs.push_back(std::make_unique<IntExpr>(const_symbol));
  irhs.push_back(std::make_unique<IntExpr>(var_symbol));
  irhs.push_back(std::make_unique<IntExpr>(
      INTEXPR_MULT, std::make_unique<IntExpr>(const_symbol),
      std::make_unique<IntExpr>(var_symbol)));
  irhs.push_back(std::make_unique<IntExpr>(
      INTEXPR_DIV, std::make_unique<IntExpr>(var_symbol),
      std::make_unique<IntExpr>(const_symbol)));
  irhs.push_back(std::make_unique<IntExpr>(
      INTEXPR_MOD, std::make_unique<IntExpr>(var_symbol),
      std::make_unique<IntExpr>(const_symbol)));

  return std::make_unique<ProductionRule>(int_symbol1, std::move(irhs));
}
std::vector<std::unique_ptr<ProductionRule>> rule_numeric_linear() {
  std::vector<std::unique_ptr<ProductionRule>> rules;
  rules.push_back(start_rule_numeric_linear());
  rules.push_back(bool_rule0_numeric_linear());
  rules.push_back(bool_rule1_numeric_linear());
  rules.push_back(int_rule0_numeric_linear());
  rules.push_back(int_rule1_numeric_linear());
  rules.push_back(const_rule());
  rules.push_back(var_rule());

  return rules;
}

std::unique_ptr<ProductionRule> start_rule_numeric_nonlinear_simple() {
  std::vector<std::unique_ptr<BoolExpr>> brhs;
  brhs.push_back(std::make_unique<BoolExpr>(bool_symbol0));

  return std::make_unique<ProductionRule>(start_symbol, std::move(brhs));
}
std::unique_ptr<ProductionRule> bool_rule0_numeric_nonlinear_simple() {
  std::vector<std::unique_ptr<BoolExpr>> brhs;
  brhs.push_back(std::make_unique<BoolExpr>(bool_symbol1));
  brhs.push_back(std::make_unique<BoolExpr>(
      BOOLEXPR_AND, std::make_unique<BoolExpr>(bool_symbol1),
      std::make_unique<BoolExpr>(bool_symbol1)));
  brhs.push_back(std::make_unique<BoolExpr>(
      BOOLEXPR_OR, std::make_unique<BoolExpr>(bool_symbol1),
      std::make_unique<BoolExpr>(bool_symbol1)));
  brhs.push_back(std::make_unique<BoolExpr>(
      BOOLEXPR_NOT, std::make_unique<BoolExpr>(bool_symbol1)));

  return std::make_unique<ProductionRule>(bool_symbol0, std::move(brhs));
}
std::unique_ptr<ProductionRule> bool_rule1_numeric_nonlinear_simple() {
  std::vector<std::unique_ptr<BoolExpr>> brhs;
  brhs.push_back(std::make_unique<BoolExpr>(
      BOOLEXPR_EQ, std::make_unique<IntExpr>(int_symbol0),
      std::make_unique<IntExpr>(int_symbol0)));
  brhs.push_back(std::make_unique<BoolExpr>(
      BOOLEXPR_LT, std::make_unique<IntExpr>(int_symbol0),
      std::make_unique<IntExpr>(int_symbol0)));
  brhs.push_back(std::make_unique<BoolExpr>(
      BOOLEXPR_LTE, std::make_unique<IntExpr>(int_symbol0),
      std::make_unique<IntExpr>(int_symbol0)));

  return std::make_unique<ProductionRule>(bool_symbol1, std::move(brhs));
}
std::unique_ptr<ProductionRule> int_rule0_numeric_nonlinear_simple() {
  std::vector<std::unique_ptr<IntExpr>> irhs;
  irhs.push_back(std::make_unique<IntExpr>(int_symbol1));
  irhs.push_back(std::make_unique<IntExpr>(
      INTEXPR_ADD, std::make_unique<IntExpr>(int_symbol0),
      std::make_unique<IntExpr>(int_symbol0)));
  irhs.push_back(std::make_unique<IntExpr>(
      INTEXPR_SUB, std::make_unique<IntExpr>(int_symbol0),
      std::make_unique<IntExpr>(int_symbol0)));

  return std::make_unique<ProductionRule>(int_symbol0, std::move(irhs));
}
std::unique_ptr<ProductionRule> int_rule1_numeric_nonlinear_simple() {
  std::vector<std::unique_ptr<IntExpr>> irhs;
  irhs.push_back(std::make_unique<IntExpr>(int_symbol2));
  irhs.push_back(std::make_unique<IntExpr>(
      INTEXPR_MULT, std::make_unique<IntExpr>(int_symbol2),
      std::make_unique<IntExpr>(int_symbol2)));
  irhs.push_back(std::make_unique<IntExpr>(
      INTEXPR_DIV, std::make_unique<IntExpr>(int_symbol2),
      std::make_unique<IntExpr>(int_symbol2)));
  irhs.push_back(std::make_unique<IntExpr>(
      INTEXPR_MOD, std::make_unique<IntExpr>(int_symbol2),
      std::make_unique<IntExpr>(int_symbol2)));

  return std::make_unique<ProductionRule>(int_symbol1, std::move(irhs));
}
std::unique_ptr<ProductionRule> int_rule2_numeric_nonlinear_simple() {
  std::set<int> literals = default_literals;
  std::vector<int> literals_sorted(literals.begin(), literals.end());
  std::sort(literals_sorted.begin(), literals_sorted.end());

  std::vector<std::unique_ptr<IntExpr>> irhs;
  for (auto& param : get_numeric_params())
    irhs.push_back(std::make_unique<IntExpr>(param.get_name()));
  for (const auto& value : literals_sorted)
    irhs.push_back(std::make_unique<IntExpr>(value));

  return std::make_unique<ProductionRule>(int_symbol2, std::move(irhs));
}
std::vector<std::unique_ptr<ProductionRule>> rule_numeric_nonlinear_simple() {
  std::vector<std::unique_ptr<ProductionRule>> rules;
  rules.push_back(start_rule_numeric_nonlinear_simple());
  rules.push_back(bool_rule0_numeric_nonlinear_simple());
  rules.push_back(bool_rule1_numeric_nonlinear_simple());
  rules.push_back(int_rule0_numeric_nonlinear_simple());
  rules.push_back(int_rule1_numeric_nonlinear_simple());
  rules.push_back(int_rule2_numeric_nonlinear_simple());

  return rules;
}

std::unique_ptr<ProductionRule> bool_rule_numeric_nonlinear_complex() {
  std::vector<std::unique_ptr<BoolExpr>> brhs;
  brhs.push_back(std::make_unique<BoolExpr>(
      BOOLEXPR_AND, std::make_unique<BoolExpr>(start_symbol),
      std::make_unique<BoolExpr>(start_symbol)));
  brhs.push_back(std::make_unique<BoolExpr>(
      BOOLEXPR_OR, std::make_unique<BoolExpr>(start_symbol),
      std::make_unique<BoolExpr>(start_symbol)));
  brhs.push_back(std::make_unique<BoolExpr>(
      BOOLEXPR_NOT, std::make_unique<BoolExpr>(start_symbol)));
  brhs.push_back(std::make_unique<BoolExpr>(
      BOOLEXPR_EQ, std::make_unique<IntExpr>(int_symbol),
      std::make_unique<IntExpr>(int_symbol)));
  brhs.push_back(std::make_unique<BoolExpr>(
      BOOLEXPR_LTE, std::make_unique<IntExpr>(int_symbol),
      std::make_unique<IntExpr>(int_symbol)));
  brhs.push_back(std::make_unique<BoolExpr>(
      BOOLEXPR_GTE, std::make_unique<IntExpr>(int_symbol),
      std::make_unique<IntExpr>(int_symbol)));

  return std::make_unique<ProductionRule>(start_symbol, std::move(brhs));
}
std::unique_ptr<ProductionRule> int_rule_numeric_nonlinear_complex() {
  std::set<int> literals = default_literals;
  std::vector<int> literals_sorted(literals.begin(), literals.end());
  std::sort(literals_sorted.begin(), literals_sorted.end());

  std::vector<std::unique_ptr<IntExpr>> irhs;
  for (auto& param : get_numeric_params())
    irhs.push_back(std::make_unique<IntExpr>(param.get_name()));
  for (const auto& value : literals_sorted)
    irhs.push_back(std::make_unique<IntExpr>(value));

  irhs.push_back(std::make_unique<IntExpr>(
      INTEXPR_ADD, std::make_unique<IntExpr>(int_symbol),
      std::make_unique<IntExpr>(int_symbol)));
  irhs.push_back(std::make_unique<IntExpr>(
      INTEXPR_SUB, std::make_unique<IntExpr>(int_symbol),
      std::make_unique<IntExpr>(int_symbol)));
  irhs.push_back(std::make_unique<IntExpr>(
      INTEXPR_MULT, std::make_unique<IntExpr>(int_symbol),
      std::make_unique<IntExpr>(int_symbol)));
  irhs.push_back(std::make_unique<IntExpr>(
      INTEXPR_DIV, std::make_unique<IntExpr>(int_symbol),
      std::make_unique<IntExpr>(int_symbol)));
  irhs.push_back(std::make_unique<IntExpr>(
      INTEXPR_MOD, std::make_unique<IntExpr>(int_symbol),
      std::make_unique<IntExpr>(int_symbol)));

  return std::make_unique<ProductionRule>(int_symbol, std::move(irhs));
}
std::vector<std::unique_ptr<ProductionRule>> rule_numeric_nonlinear_complex() {
  std::vector<std::unique_ptr<ProductionRule>> rules;
  rules.push_back(bool_rule_numeric_nonlinear_complex());
  rules.push_back(int_rule_numeric_nonlinear_complex());

  return rules;
}

std::unique_ptr<SygusFile> gen_sygus_file(
    CondType condtype, std::vector<std::unique_ptr<Constraint>> constraints) {
  static const std::string default_func_name = "f";

  std::vector<std::string> param_names =
      condtype == CT_ENUM ? get_enum_param_names() : get_numeric_param_names();
  std::vector<std::unique_ptr<Param>> params;
  for (auto& param_name : param_names)
    params.push_back(std::make_unique<Param>(param_name));

  std::vector<std::unique_ptr<ProductionRule>> rules;
  switch (condtype) {
    case CT_ENUM:
      rules = rule_enum();
      break;
    case CT_NUMERIC:
      rules = rule_numeric_linear();
      break;
    default:
      throw Unreachable();
  }

  std::unique_ptr<FunSpec> fspec = std::make_unique<FunSpec>(
      default_func_name, std::move(params), std::move(rules));
  return std::make_unique<SygusFile>(default_setlogic(), std::move(fspec),
                                     std::move(constraints));
}

}  // namespace pathfinder
