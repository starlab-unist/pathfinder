#ifndef SYGUS_GEN
#define SYGUS_GEN

#include <set>

#include "pathfinder_defs.h"
#include "sygus_ast.h"

namespace pathfinder {

enum SupportedLogic {
  SYGUS_LOGIC_LIA,
};

class SetLogic {
 public:
  SetLogic(SupportedLogic logic_);
  std::string to_string() const;

 private:
  SupportedLogic logic;
};

class ProductionRule {
 public:
  ProductionRule(std::string symbol_,
                 std::vector<std::unique_ptr<IntExpr>> irhs_);
  ProductionRule(std::string symbol_,
                 std::vector<std::unique_ptr<BoolExpr>> brhs_);
  std::string to_string(int depth) const;

 private:
  std::string symbol;
  SygusValueType t;
  std::vector<std::unique_ptr<IntExpr>> irhs;
  std::vector<std::unique_ptr<BoolExpr>> brhs;
};

class FunSpec {
 public:
  FunSpec(std::string name_, std::vector<std::unique_ptr<Param>> params_,
          std::vector<std::unique_ptr<ProductionRule>> rules_);
  std::string to_string() const;

 private:
  std::string name;
  std::vector<std::unique_ptr<Param>> params;
  std::vector<std::unique_ptr<ProductionRule>> rules;
};

class Constraint {
 public:
  Constraint(std::string fname_, CondType condtype_, const Args& args_,
             bool result_);
  std::string to_string() const;

 private:
  std::string fname;
  CondType condtype;
  Args args;
  bool result;
};

class SygusFile {
 public:
  SygusFile(std::unique_ptr<SetLogic> setlogic_,
            std::unique_ptr<FunSpec> funspec_,
            std::vector<std::unique_ptr<Constraint>> constraints_);
  std::string to_string() const;

 private:
  std::unique_ptr<SetLogic> setlogic;
  std::unique_ptr<FunSpec> funspec;
  std::vector<std::unique_ptr<Constraint>> constraints;
};

extern const std::set<int> default_literals;

std::unique_ptr<SygusFile> gen_sygus_file(
    CondType condtype, std::vector<std::unique_ptr<Constraint>> constraints);

}  // namespace pathfinder

#endif
