#ifndef SYGUS_AST
#define SYGUS_AST

#include <map>
#include <set>
#include <string>
#include <vector>

#include "pathfinder_defs.h"
#include "z3++.h"

namespace pathfinder {

class CondEvalException : public std::exception {};

enum SygusValueType {
  // we only consider int and bool
  SYGUS_VALUE_TYPE_INT,
  SYGUS_VALUE_TYPE_BOOL,
};

/*
 *  EqualityType, EqualityCondition, and to_equality_condition()
 *  are used in EnumSolver.
 */
enum EqualityType { ET_Equal, ET_Inequal };

class EqualityCondition {
 public:
  EqualityCondition(EqualityType eqtype_, std::string left_,
                    std::string right_);
  EqualityType get_eqtype() const;
  std::string get_left() const;
  std::string get_right() const;
  EqualityCondition negate() const;

 private:
  EqualityType eqtype;
  std::string left;
  std::string right;
};

class Param {
 public:
  Param(std::string id_);
  Param(const Param &p);
  std::string get_id() const;
  std::string to_string() const;

 private:
  std::string id;
};
std::vector<std::unique_ptr<Param>> to_params(
    std::vector<std::string> &param_names);

enum IntExprType {
  INTEXPR_CONST,
  INTEXPR_VAR,
  INTEXPR_ITE,
  INTEXPR_ADD,
  INTEXPR_SUB,
  INTEXPR_MULT,
  INTEXPR_DIV,
  INTEXPR_MOD,
};

class BoolExpr;

class IntExpr {
 public:
  IntExpr();  // needed for std::map
  IntExpr(int value_);
  IntExpr(std::string id_);
  IntExpr(IntExprType t_, std::unique_ptr<IntExpr> left_,
          std::unique_ptr<IntExpr> right_);
  IntExpr(std::unique_ptr<BoolExpr> cond_, std::unique_ptr<IntExpr> left_,
          std::unique_ptr<IntExpr> right_);
  IntExpr(const IntExpr &other);
  bool struct_eq(const IntExpr &other) const;
  z3::expr to_z3_expr(z3::context &ctx) const;
  std::string to_string(bool readable = false) const;
  long eval(Args numeric_args) const;
  bool has(int literal) const;
  IntExpr operator+(const IntExpr &other) const;
  IntExpr operator-(const IntExpr &other) const;
  IntExpr operator*(const IntExpr &other) const;
  IntExpr operator/(const IntExpr &other) const;
  IntExpr operator%(const IntExpr &other) const;
  BoolExpr operator==(const IntExpr &other) const;
  BoolExpr operator!=(const IntExpr &other) const;
  BoolExpr operator<(const IntExpr &other) const;
  BoolExpr operator>(const IntExpr &other) const;
  BoolExpr operator<=(const IntExpr &other) const;
  BoolExpr operator>=(const IntExpr &other) const;
  IntExpr operator+(int val) const;
  IntExpr operator-(int val) const;
  IntExpr operator*(int val) const;
  IntExpr operator/(int val) const;
  IntExpr operator%(int val) const;
  BoolExpr operator==(int val) const;
  BoolExpr operator!=(int val) const;
  BoolExpr operator<(int val) const;
  BoolExpr operator>(int val) const;
  BoolExpr operator<=(int val) const;
  BoolExpr operator>=(int val) const;

 private:
  IntExprType t;
  int value;
  std::string id;
  std::unique_ptr<BoolExpr> cond;
  std::unique_ptr<IntExpr> left;
  std::unique_ptr<IntExpr> right;
  friend IntExpr operator+(int val, const IntExpr &e);
  friend IntExpr operator-(int val, const IntExpr &e);
  friend IntExpr operator*(int val, const IntExpr &e);
  friend IntExpr operator/(int val, const IntExpr &e);
  friend IntExpr operator%(int val, const IntExpr &e);
  friend BoolExpr operator==(int val, const IntExpr &e);
  friend BoolExpr operator!=(int val, const IntExpr &e);
  friend BoolExpr operator<(int val, const IntExpr &e);
  friend BoolExpr operator>(int val, const IntExpr &e);
  friend BoolExpr operator<=(int val, const IntExpr &e);
  friend BoolExpr operator>=(int val, const IntExpr &e);
  friend EqualityCondition to_equality_condition(const BoolExpr &e);
};
IntExpr operator+(int val, const IntExpr &e);
IntExpr operator-(int val, const IntExpr &e);
IntExpr operator*(int val, const IntExpr &e);
IntExpr operator/(int val, const IntExpr &e);
IntExpr operator%(int val, const IntExpr &e);
BoolExpr operator==(int val, const IntExpr &e);
BoolExpr operator!=(int val, const IntExpr &e);
BoolExpr operator<(int val, const IntExpr &e);
BoolExpr operator>(int val, const IntExpr &e);
BoolExpr operator<=(int val, const IntExpr &e);
BoolExpr operator>=(int val, const IntExpr &e);

enum BoolExprType {
  BOOLEXPR_AND,
  BOOLEXPR_OR,
  BOOLEXPR_NOT,
  BOOLEXPR_EQ,
  BOOLEXPR_NEQ,
  BOOLEXPR_LT,
  BOOLEXPR_GT,
  BOOLEXPR_LTE,
  BOOLEXPR_GTE,
  BOOLEXPR_VAR,
};

class BoolExpr {
 public:
  BoolExpr(BoolExprType t_, std::unique_ptr<BoolExpr> bleft_,
           std::unique_ptr<BoolExpr> bright_);
  BoolExpr(BoolExprType t_, std::unique_ptr<BoolExpr> b_);
  BoolExpr(BoolExprType t_, std::unique_ptr<IntExpr> ileft_,
           std::unique_ptr<IntExpr> iright_);
  BoolExpr(std::string id_);
  BoolExpr(const BoolExpr &other);
  static BoolExpr true_expr();
  static BoolExpr false_expr();
  static std::unique_ptr<BoolExpr> and_expr(
      std::vector<std::unique_ptr<BoolExpr>> &es);
  bool struct_eq(const BoolExpr &other) const;
  z3::expr to_z3_expr(z3::context &ctx) const;
  std::string to_string(bool readable = false) const;
  bool eval(Args args) const;
  bool has(int literal) const;
  BoolExpr operator&&(const BoolExpr &other) const;
  BoolExpr operator||(const BoolExpr &other) const;
  BoolExpr operator!() const;
  BoolExpr operator&&(bool val) const;
  BoolExpr operator||(bool val) const;

 private:
  void copy(const BoolExpr &other);
  BoolExprType t;
  std::unique_ptr<BoolExpr> b;
  std::unique_ptr<BoolExpr> bleft;
  std::unique_ptr<BoolExpr> bright;
  std::unique_ptr<IntExpr> ileft;
  std::unique_ptr<IntExpr> iright;
  std::string
      id;  // bool type variables are only used for specifying production rule.
  friend class Engine;
  friend BoolExpr operator&&(bool val, const BoolExpr &e);
  friend BoolExpr operator||(bool val, const BoolExpr &e);
  friend BoolExpr simplify(const BoolExpr &e);
  friend EqualityCondition to_equality_condition(const BoolExpr &e);
};
BoolExpr operator&&(bool val, const BoolExpr &e);
BoolExpr operator||(bool val, const BoolExpr &e);
BoolExpr simplify(const BoolExpr &e);
bool eval(std::vector<std::unique_ptr<BoolExpr>> &conds, const Args &args);
bool check_correct(BoolExpr *cond, const std::set<Args> &pos_examples,
                   const std::set<Args> &neg_examples);
EqualityCondition to_equality_condition(const BoolExpr &e);

class FunSynthesized {
 public:
  FunSynthesized(std::string name_, std::vector<std::unique_ptr<Param>> params_,
                 std::unique_ptr<BoolExpr> body_);
  FunSynthesized(std::string name_, std::vector<std::string> param_names,
                 std::unique_ptr<BoolExpr> body_);
  std::string to_string() const;
  std::string get_name() const;
  std::vector<std::unique_ptr<Param>> get_params() const;
  BoolExpr *get_body() const;

  z3::expr get_z3_expr(z3::context &ctx) const;
  bool eval(Args args) const;

 private:
  std::string name;
  std::vector<std::unique_ptr<Param>> params;
  std::unique_ptr<BoolExpr> body;
  friend class Engine;
};

}  // namespace pathfinder

#endif
