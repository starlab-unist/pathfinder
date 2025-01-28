#include "sygus_ast.h"

#include <cmath>

#include "utils.h"

namespace pathfinder {

EqualityCondition::EqualityCondition(EqualityType eqtype_, std::string left_,
                                     std::string right_)
    : eqtype(eqtype_), left(left_), right(right_) {}
EqualityType EqualityCondition::get_eqtype() const { return eqtype; }
std::string EqualityCondition::get_left() const { return left; }
std::string EqualityCondition::get_right() const { return right; }
EqualityCondition EqualityCondition::negate() const {
  EqualityType eqtype_ = eqtype == ET_Equal ? ET_Inequal : ET_Equal;
  return EqualityCondition(eqtype_, left, right);
}
EqualityCondition to_equality_condition(const BoolExpr &e) {
  switch (e.t) {
    case BOOLEXPR_NOT:
      return to_equality_condition(*(e.b)).negate();
    case BOOLEXPR_EQ:
      assert(e.ileft->t == INTEXPR_VAR && e.iright->t == INTEXPR_VAR);
      return EqualityCondition(ET_Equal, e.ileft->id, e.iright->id);
    case BOOLEXPR_NEQ:
      assert(e.ileft->t == INTEXPR_VAR && e.iright->t == INTEXPR_VAR);
      return EqualityCondition(ET_Inequal, e.ileft->id, e.iright->id);
    default:
      PATHFINDER_CHECK(false, "Only equality between parameters are expected");
  }
}

Param::Param(std::string id_) : id(id_) {}
Param::Param(const Param &p) : id(p.id) {}
std::string Param::get_id() const { return id; }
std::string Param::to_string() const { return get_id(); }
std::vector<std::unique_ptr<Param>> to_params(
    std::vector<std::string> &param_names) {
  std::vector<std::unique_ptr<Param>> params;
  for (auto &param_name : param_names)
    params.push_back(std::make_unique<Param>(param_name));
  return params;
}

IntExpr::IntExpr() : t(INTEXPR_CONST), value(0) {}
IntExpr::IntExpr(int value_) : t(INTEXPR_CONST), value(value_) {}
IntExpr::IntExpr(std::string id_) : t(INTEXPR_VAR), id(id_) {}
IntExpr::IntExpr(IntExprType t_, std::unique_ptr<IntExpr> left_,
                 std::unique_ptr<IntExpr> right_)
    : t(t_), left(std::move(left_)), right(std::move(right_)) {}
IntExpr::IntExpr(std::unique_ptr<BoolExpr> cond_,
                 std::unique_ptr<IntExpr> left_,
                 std::unique_ptr<IntExpr> right_)
    : t(INTEXPR_ITE),
      cond(std::move(cond_)),
      left(std::move(left_)),
      right(std::move(right_)) {}
IntExpr::IntExpr(const IntExpr &other) {
  t = other.t;
  value = other.value;
  id = other.id;
  if (other.cond.get() != nullptr) {
    cond = std::make_unique<BoolExpr>(*(other.cond));
  }
  if (other.left.get() != nullptr) {
    left = std::make_unique<IntExpr>(*(other.left));
  }
  if (other.right.get() != nullptr) {
    right = std::make_unique<IntExpr>(*(other.right));
  }
}
bool IntExpr::struct_eq(const IntExpr &other) const {
  if (t != other.t) return false;
  switch (t) {
    case INTEXPR_CONST:
      return value == other.value;
    case INTEXPR_VAR:
      return id == other.id;
    case INTEXPR_ITE:
      return cond->struct_eq(*(other.cond)) && left->struct_eq(*(other.left)) &&
             right->struct_eq(*(other.right));
    case INTEXPR_ADD:
    case INTEXPR_SUB:
    case INTEXPR_MULT:
    case INTEXPR_DIV:
    case INTEXPR_MOD:
      return left->struct_eq(*(other.left)) && right->struct_eq(*(other.right));
    default:
      throw Unreachable();
  }
}
z3::expr IntExpr::to_z3_expr(z3::context &ctx) const {
  switch (t) {
    case INTEXPR_CONST:
      return ctx.int_val(value);
    case INTEXPR_VAR:
      return ctx.int_const(id.c_str());
    case INTEXPR_ITE:
      return z3::ite(cond->to_z3_expr(ctx), left->to_z3_expr(ctx),
                     right->to_z3_expr(ctx));
    case INTEXPR_ADD:
      return left->to_z3_expr(ctx) + right->to_z3_expr(ctx);
    case INTEXPR_SUB:
      return left->to_z3_expr(ctx) - right->to_z3_expr(ctx);
    case INTEXPR_MULT:
      return left->to_z3_expr(ctx) * right->to_z3_expr(ctx);
    case INTEXPR_DIV:
      return left->to_z3_expr(ctx) / right->to_z3_expr(ctx);
    case INTEXPR_MOD:
      return left->to_z3_expr(ctx) % right->to_z3_expr(ctx);
    default:
      throw Unreachable();
  }
}
std::string IntExpr::to_string(bool readable) const {
  if (readable) {  // pretty print
    switch (t) {
      case INTEXPR_CONST:
        return std::to_string(value);
      case INTEXPR_VAR:
        return id;
      case INTEXPR_ITE:
        return "(ite " + cond->to_string(readable) + " " +
               left->to_string(readable) + " " + right->to_string(readable) +
               ")";
      case INTEXPR_ADD:
        return "(" + left->to_string(readable) + " + " +
               right->to_string(readable) + ")";
      case INTEXPR_SUB:
        return "(" + left->to_string(readable) + " - " +
               right->to_string(readable) + ")";
      case INTEXPR_MULT:
        return "(" + left->to_string(readable) + " * " +
               right->to_string(readable) + ")";
      case INTEXPR_DIV:
        return "(" + left->to_string(readable) + " / " +
               right->to_string(readable) + ")";
      case INTEXPR_MOD:
        return "(" + left->to_string(readable) + " % " +
               right->to_string(readable) + ")";
      default:
        throw Unreachable();
    }
  } else {  // for sygus spec generation
    switch (t) {
      case INTEXPR_CONST:
        return std::to_string(value);
      case INTEXPR_VAR:
        return id;
      case INTEXPR_ITE:
        return "(ite " + cond->to_string(readable) + " " +
               left->to_string(readable) + " " + right->to_string(readable) +
               ")";
      case INTEXPR_ADD:
        return "(+ " + left->to_string(readable) + " " +
               right->to_string(readable) + ")";
      case INTEXPR_SUB:
        return "(- " + left->to_string(readable) + " " +
               right->to_string(readable) + ")";
      case INTEXPR_MULT:
        return "(* " + left->to_string(readable) + " " +
               right->to_string(readable) + ")";
      case INTEXPR_DIV:
        return "(/ " + left->to_string(readable) + " " +
               right->to_string(readable) + ")";
      case INTEXPR_MOD:
        return "(% " + left->to_string(readable) + " " +
               right->to_string(readable) + ")";
      default:
        throw Unreachable();
    }
  }
};
long IntExpr::eval(Args numeric_args) const {
  int left_val, right_val;
  switch (t) {
    case INTEXPR_CONST:
      return value;
    case INTEXPR_VAR:
      return numeric_args.at(id);
    case INTEXPR_ITE:
      if (cond->eval(numeric_args)) {
        return left->eval(numeric_args);
      } else {
        return right->eval(numeric_args);
      }
    case INTEXPR_ADD:
      return left->eval(numeric_args) + right->eval(numeric_args);
    case INTEXPR_SUB:
      return left->eval(numeric_args) - right->eval(numeric_args);
    case INTEXPR_MULT:
      return left->eval(numeric_args) * right->eval(numeric_args);
    case INTEXPR_DIV:
      left_val = left->eval(numeric_args);
      right_val = right->eval(numeric_args);
      if (right_val == 0) throw CondEvalException();
      return left_val / right_val;
    case INTEXPR_MOD:
      left_val = left->eval(numeric_args);
      right_val = right->eval(numeric_args);
      if (right_val == 0) throw CondEvalException();
      return left_val % right_val;
    default:
      throw Unreachable();
  }
}
bool IntExpr::has(int literal) const {
  switch (t) {
    case INTEXPR_CONST:
      return literal == value;
    case INTEXPR_VAR:
      return false;
    case INTEXPR_ITE:
      return cond->has(literal) || left->has(literal) || right->has(literal);
    case INTEXPR_ADD:
    case INTEXPR_SUB:
    case INTEXPR_MULT:
    case INTEXPR_DIV:
    case INTEXPR_MOD:
      return left->has(literal) || right->has(literal);
    default:
      throw Unreachable();
  }
}
IntExpr IntExpr::operator+(const IntExpr &other) const {
  return IntExpr(INTEXPR_ADD, std::make_unique<IntExpr>(*this),
                 std::make_unique<IntExpr>(other));
}
IntExpr IntExpr::operator-(const IntExpr &other) const {
  return IntExpr(INTEXPR_SUB, std::make_unique<IntExpr>(*this),
                 std::make_unique<IntExpr>(other));
}
IntExpr IntExpr::operator*(const IntExpr &other) const {
  return IntExpr(INTEXPR_MULT, std::make_unique<IntExpr>(*this),
                 std::make_unique<IntExpr>(other));
}
IntExpr IntExpr::operator/(const IntExpr &other) const {
  return IntExpr(INTEXPR_DIV, std::make_unique<IntExpr>(*this),
                 std::make_unique<IntExpr>(other));
}
IntExpr IntExpr::operator%(const IntExpr &other) const {
  return IntExpr(INTEXPR_MOD, std::make_unique<IntExpr>(*this),
                 std::make_unique<IntExpr>(other));
}
BoolExpr IntExpr::operator==(const IntExpr &other) const {
  return BoolExpr(BOOLEXPR_EQ, std::make_unique<IntExpr>(*this),
                  std::make_unique<IntExpr>(other));
}
BoolExpr IntExpr::operator!=(const IntExpr &other) const {
  return BoolExpr(BOOLEXPR_NEQ, std::make_unique<IntExpr>(*this),
                  std::make_unique<IntExpr>(other));
}
BoolExpr IntExpr::operator<(const IntExpr &other) const {
  return BoolExpr(BOOLEXPR_LT, std::make_unique<IntExpr>(*this),
                  std::make_unique<IntExpr>(other));
}
BoolExpr IntExpr::operator>(const IntExpr &other) const {
  return BoolExpr(BOOLEXPR_GT, std::make_unique<IntExpr>(*this),
                  std::make_unique<IntExpr>(other));
}
BoolExpr IntExpr::operator<=(const IntExpr &other) const {
  return BoolExpr(BOOLEXPR_LTE, std::make_unique<IntExpr>(*this),
                  std::make_unique<IntExpr>(other));
}
BoolExpr IntExpr::operator>=(const IntExpr &other) const {
  return BoolExpr(BOOLEXPR_GTE, std::make_unique<IntExpr>(*this),
                  std::make_unique<IntExpr>(other));
}
IntExpr IntExpr::operator+(int val) const {
  switch (t) {
    case INTEXPR_CONST:
      return IntExpr(value + val);
    default:
      return IntExpr(INTEXPR_ADD, std::make_unique<IntExpr>(*this),
                     std::make_unique<IntExpr>(val));
  }
}
IntExpr IntExpr::operator-(int val) const {
  switch (t) {
    case INTEXPR_CONST:
      return IntExpr(value - val);
    default:
      return IntExpr(INTEXPR_SUB, std::make_unique<IntExpr>(*this),
                     std::make_unique<IntExpr>(val));
  }
}
IntExpr IntExpr::operator*(int val) const {
  switch (t) {
    case INTEXPR_CONST:
      return IntExpr(value * val);
    default:
      return IntExpr(INTEXPR_MULT, std::make_unique<IntExpr>(*this),
                     std::make_unique<IntExpr>(val));
  }
}
IntExpr IntExpr::operator/(int val) const {
  switch (t) {
    case INTEXPR_CONST:
      return IntExpr(value / val);
    default:
      return IntExpr(INTEXPR_DIV, std::make_unique<IntExpr>(*this),
                     std::make_unique<IntExpr>(val));
  }
}
IntExpr IntExpr::operator%(int val) const {
  switch (t) {
    case INTEXPR_CONST:
      return IntExpr(value % val);
    default:
      return IntExpr(INTEXPR_MOD, std::make_unique<IntExpr>(*this),
                     std::make_unique<IntExpr>(val));
  }
}
BoolExpr IntExpr::operator==(int val) const {
  return BoolExpr(BOOLEXPR_EQ, std::make_unique<IntExpr>(*this),
                  std::make_unique<IntExpr>(val));
}
BoolExpr IntExpr::operator!=(int val) const {
  return BoolExpr(BOOLEXPR_NEQ, std::make_unique<IntExpr>(*this),
                  std::make_unique<IntExpr>(val));
}
BoolExpr IntExpr::operator<(int val) const {
  return BoolExpr(BOOLEXPR_LT, std::make_unique<IntExpr>(*this),
                  std::make_unique<IntExpr>(val));
}
BoolExpr IntExpr::operator>(int val) const {
  return BoolExpr(BOOLEXPR_GT, std::make_unique<IntExpr>(*this),
                  std::make_unique<IntExpr>(val));
}
BoolExpr IntExpr::operator<=(int val) const {
  return BoolExpr(BOOLEXPR_LTE, std::make_unique<IntExpr>(*this),
                  std::make_unique<IntExpr>(val));
}
BoolExpr IntExpr::operator>=(int val) const {
  return BoolExpr(BOOLEXPR_GTE, std::make_unique<IntExpr>(*this),
                  std::make_unique<IntExpr>(val));
}
IntExpr operator+(int val, const IntExpr &e) {
  switch (e.t) {
    case INTEXPR_CONST:
      return IntExpr(val + e.value);
    default:
      return IntExpr(INTEXPR_ADD, std::make_unique<IntExpr>(val),
                     std::make_unique<IntExpr>(e));
  }
}
IntExpr operator-(int val, const IntExpr &e) {
  switch (e.t) {
    case INTEXPR_CONST:
      return IntExpr(val - e.value);
    default:
      return IntExpr(INTEXPR_SUB, std::make_unique<IntExpr>(val),
                     std::make_unique<IntExpr>(e));
  }
}
IntExpr operator*(int val, const IntExpr &e) {
  switch (e.t) {
    case INTEXPR_CONST:
      return IntExpr(val * e.value);
    default:
      return IntExpr(INTEXPR_MULT, std::make_unique<IntExpr>(val),
                     std::make_unique<IntExpr>(e));
  }
}
IntExpr operator/(int val, const IntExpr &e) {
  switch (e.t) {
    case INTEXPR_CONST:
      return IntExpr(val / e.value);
    default:
      return IntExpr(INTEXPR_DIV, std::make_unique<IntExpr>(val),
                     std::make_unique<IntExpr>(e));
  }
}
IntExpr operator%(int val, const IntExpr &e) {
  switch (e.t) {
    case INTEXPR_CONST:
      return IntExpr(val % e.value);
    default:
      return IntExpr(INTEXPR_MOD, std::make_unique<IntExpr>(val),
                     std::make_unique<IntExpr>(e));
  }
}
BoolExpr operator==(int val, const IntExpr &e) {
  return BoolExpr(BOOLEXPR_EQ, std::make_unique<IntExpr>(val),
                  std::make_unique<IntExpr>(e));
}
BoolExpr operator!=(int val, const IntExpr &e) {
  return BoolExpr(BOOLEXPR_NEQ, std::make_unique<IntExpr>(val),
                  std::make_unique<IntExpr>(e));
}
BoolExpr operator<(int val, const IntExpr &e) {
  return BoolExpr(BOOLEXPR_LT, std::make_unique<IntExpr>(val),
                  std::make_unique<IntExpr>(e));
}
BoolExpr operator>(int val, const IntExpr &e) {
  return BoolExpr(BOOLEXPR_GT, std::make_unique<IntExpr>(val),
                  std::make_unique<IntExpr>(e));
}
BoolExpr operator<=(int val, const IntExpr &e) {
  return BoolExpr(BOOLEXPR_LTE, std::make_unique<IntExpr>(val),
                  std::make_unique<IntExpr>(e));
}
BoolExpr operator>=(int val, const IntExpr &e) {
  return BoolExpr(BOOLEXPR_GTE, std::make_unique<IntExpr>(val),
                  std::make_unique<IntExpr>(e));
}

BoolExpr::BoolExpr(BoolExprType t_, std::unique_ptr<BoolExpr> bleft_,
                   std::unique_ptr<BoolExpr> bright_) {
  assert(t_ == BOOLEXPR_AND || t_ == BOOLEXPR_OR);
  if (t_ == BOOLEXPR_AND) {
    if (bleft_->struct_eq(true_expr())) {
      copy(*bright_);
    } else if (bright_->struct_eq(true_expr())) {
      copy(*bleft);
    } else {
      t = t_;
      bleft = std::move(bleft_);
      bright = std::move(bright_);
    }
  } else {  // t_ == BOOLEXPR_OR
    if (bleft_->struct_eq(false_expr())) {
      copy(*bright_);
    } else if (bright_->struct_eq(false_expr())) {
      copy(*bleft);
    } else {
      t = t_;
      bleft = std::move(bleft_);
      bright = std::move(bright_);
    }
  }
}
BoolExpr::BoolExpr(BoolExprType t_, std::unique_ptr<BoolExpr> b_)
    : t(t_), b(std::move(b_)) {
  assert(t_ == BOOLEXPR_NOT);
}
BoolExpr::BoolExpr(BoolExprType t_, std::unique_ptr<IntExpr> ileft_,
                   std::unique_ptr<IntExpr> iright_)
    : t(t_), ileft(std::move(ileft_)), iright(std::move(iright_)) {
  assert(t_ == BOOLEXPR_EQ || t_ == BOOLEXPR_NEQ || t_ == BOOLEXPR_LT ||
         t_ == BOOLEXPR_GT || t_ == BOOLEXPR_LTE || t_ == BOOLEXPR_GTE);
}
BoolExpr::BoolExpr(std::string id_) : t(BOOLEXPR_VAR), id(id_) {}
void BoolExpr::copy(const BoolExpr &other) {
  t = other.t;
  if (other.b.get() != nullptr) {
    b = std::make_unique<BoolExpr>(*(other.b));
  }
  if (other.bleft.get() != nullptr) {
    bleft = std::make_unique<BoolExpr>(*(other.bleft));
  }
  if (other.bright.get() != nullptr) {
    bright = std::make_unique<BoolExpr>(*(other.bright));
  }
  if (other.ileft.get() != nullptr) {
    ileft = std::make_unique<IntExpr>(*(other.ileft));
  }
  if (other.iright.get() != nullptr) {
    iright = std::make_unique<IntExpr>(*(other.iright));
  }
  id = other.id;
}
BoolExpr::BoolExpr(const BoolExpr &other) { copy(other); }
BoolExpr BoolExpr::true_expr() {
  return BoolExpr(BOOLEXPR_EQ, std::make_unique<IntExpr>(1),
                  std::make_unique<IntExpr>(1));
}
BoolExpr BoolExpr::false_expr() {
  return BoolExpr(BOOLEXPR_NEQ, std::make_unique<IntExpr>(1),
                  std::make_unique<IntExpr>(1));
}
std::unique_ptr<BoolExpr> BoolExpr::and_expr(
    std::vector<std::unique_ptr<BoolExpr>> &es) {
  if (es.empty()) return std::make_unique<BoolExpr>(true_expr());

  std::unique_ptr<BoolExpr> acc = es[0].get() == nullptr
                                      ? std::make_unique<BoolExpr>(true_expr())
                                      : std::make_unique<BoolExpr>(*es[0]);

  for (size_t i = 1; i < es.size(); i++) {
    if (es[i].get() != nullptr)
      acc = std::make_unique<BoolExpr>(BOOLEXPR_AND, std::move(acc),
                                       std::make_unique<BoolExpr>(*es[i]));
  }

  return acc;
}
bool BoolExpr::struct_eq(const BoolExpr &other) const {
  if (t != other.t) return false;
  switch (t) {
    case BOOLEXPR_VAR:
      return id == other.id;
    case BOOLEXPR_NOT:
      return b->struct_eq(*(other.b));
    case BOOLEXPR_AND:
    case BOOLEXPR_OR:
      return bleft->struct_eq(*(other.bleft)) &&
             bright->struct_eq(*(other.bright));
    case BOOLEXPR_EQ:
    case BOOLEXPR_NEQ:
    case BOOLEXPR_LT:
    case BOOLEXPR_GT:
    case BOOLEXPR_LTE:
    case BOOLEXPR_GTE:
      return ileft->struct_eq(*(other.ileft)) &&
             iright->struct_eq(*(other.iright));
    default:
      throw Unreachable();
  }
}
z3::expr BoolExpr::to_z3_expr(z3::context &ctx) const {
  switch (t) {
    case BOOLEXPR_AND:
      return bleft->to_z3_expr(ctx) && bright->to_z3_expr(ctx);
    case BOOLEXPR_OR:
      return bleft->to_z3_expr(ctx) || bright->to_z3_expr(ctx);
    case BOOLEXPR_NOT:
      return !(b->to_z3_expr(ctx));
    case BOOLEXPR_EQ:
      return ileft->to_z3_expr(ctx) == iright->to_z3_expr(ctx);
    case BOOLEXPR_NEQ:
      return ileft->to_z3_expr(ctx) != iright->to_z3_expr(ctx);
    case BOOLEXPR_LT:
      return ileft->to_z3_expr(ctx) < iright->to_z3_expr(ctx);
    case BOOLEXPR_GT:
      return ileft->to_z3_expr(ctx) > iright->to_z3_expr(ctx);
    case BOOLEXPR_LTE:
      return ileft->to_z3_expr(ctx) <= iright->to_z3_expr(ctx);
    case BOOLEXPR_GTE:
      return ileft->to_z3_expr(ctx) >= iright->to_z3_expr(ctx);
    // Bool expression var is not supposed to be used except secifying syntax.
    default:
      throw Unreachable();
  }
}
std::string BoolExpr::to_string(bool readable) const {
  if (readable) {  // pretty print
    switch (t) {
      case BOOLEXPR_AND:
        return "(" + bleft->to_string(readable) + " " + unicode_and + " " +
               bright->to_string(readable) + ")";
      case BOOLEXPR_OR:
        return "(" + bleft->to_string(readable) + " " + unicode_or + " " +
               bright->to_string(readable) + ")";
      case BOOLEXPR_NOT:
        return "(" + unicode_not + " " + b->to_string(readable) + ")";
      case BOOLEXPR_EQ:
        return "(" + ileft->to_string(readable) + " = " +
               iright->to_string(readable) + ")";
      case BOOLEXPR_NEQ:
        return "(" + ileft->to_string(readable) + " " + unicode_neq + " " +
               iright->to_string(readable) + ")";
      case BOOLEXPR_LT:
        return "(" + ileft->to_string(readable) + " < " +
               iright->to_string(readable) + ")";
      case BOOLEXPR_GT:
        return "(" + ileft->to_string(readable) + " > " +
               iright->to_string(readable) + ")";
      case BOOLEXPR_LTE:
        return "(" + ileft->to_string(readable) + " " + unicode_lte + " " +
               iright->to_string(readable) + ")";
      case BOOLEXPR_GTE:
        return "(" + ileft->to_string(readable) + " " + unicode_gte + " " +
               iright->to_string(readable) + ")";
      case BOOLEXPR_VAR:
        return id;
      default:
        throw Unreachable();
    }
  } else {  // for sygus spec generation
    switch (t) {
      case BOOLEXPR_AND:
        return "(and " + bleft->to_string(readable) + " " +
               bright->to_string(readable) + ")";
      case BOOLEXPR_OR:
        return "(or " + bleft->to_string(readable) + " " +
               bright->to_string(readable) + ")";
      case BOOLEXPR_NOT:
        return "(not " + b->to_string(readable) + ")";
      case BOOLEXPR_EQ:
        return "(= " + ileft->to_string(readable) + " " +
               iright->to_string(readable) + ")";
      case BOOLEXPR_NEQ:
        return "(!= " + ileft->to_string(readable) + " " +
               iright->to_string(readable) + ")";
      case BOOLEXPR_LT:
        return "(< " + ileft->to_string(readable) + " " +
               iright->to_string(readable) + ")";
      case BOOLEXPR_GT:
        return "(> " + ileft->to_string(readable) + " " +
               iright->to_string(readable) + ")";
      case BOOLEXPR_LTE:
        return "(<= " + ileft->to_string(readable) + " " +
               iright->to_string(readable) + ")";
      case BOOLEXPR_GTE:
        return "(>= " + ileft->to_string(readable) + " " +
               iright->to_string(readable) + ")";
      case BOOLEXPR_VAR:
        return id;
      default:
        throw Unreachable();
    }
  }
}
bool BoolExpr::eval(Args args) const {
  switch (t) {
    case BOOLEXPR_AND:
      return bleft->eval(args) && bright->eval(args);
    case BOOLEXPR_OR:
      return bleft->eval(args) || bright->eval(args);
    case BOOLEXPR_NOT:
      return !(b->eval(args));
    case BOOLEXPR_EQ:
      return ileft->eval(args) == iright->eval(args);
    case BOOLEXPR_NEQ:
      return ileft->eval(args) != iright->eval(args);
    case BOOLEXPR_LT:
      return ileft->eval(args) < iright->eval(args);
    case BOOLEXPR_GT:
      return ileft->eval(args) > iright->eval(args);
    case BOOLEXPR_LTE:
      return ileft->eval(args) <= iright->eval(args);
    case BOOLEXPR_GTE:
      return ileft->eval(args) >= iright->eval(args);

    // Bool expression var is not supposed to be used except secifying syntax.
    default:
      throw Unreachable();
  }
}
bool BoolExpr::has(int literal) const {
  switch (t) {
    case BOOLEXPR_AND:
    case BOOLEXPR_OR:
      return bleft->has(literal) || bright->has(literal);
    case BOOLEXPR_NOT:
      return b->has(literal);
    case BOOLEXPR_EQ:
    case BOOLEXPR_NEQ:
    case BOOLEXPR_LT:
    case BOOLEXPR_GT:
    case BOOLEXPR_LTE:
    case BOOLEXPR_GTE:
      return ileft->has(literal) || iright->has(literal);
    case BOOLEXPR_VAR:
      return false;
    default:
      throw Unreachable();
  }
}
BoolExpr BoolExpr::operator&&(const BoolExpr &other) const {
  return BoolExpr(BOOLEXPR_AND, std::make_unique<BoolExpr>(*this),
                  std::make_unique<BoolExpr>(other));
}
BoolExpr BoolExpr::operator||(const BoolExpr &other) const {
  return BoolExpr(BOOLEXPR_OR, std::make_unique<BoolExpr>(*this),
                  std::make_unique<BoolExpr>(other));
}
BoolExpr BoolExpr::operator!() const {
  switch (t) {
    case BOOLEXPR_NOT:
      return BoolExpr(*b);
    case BOOLEXPR_EQ:
      return BoolExpr(BOOLEXPR_NEQ, std::make_unique<IntExpr>(*ileft),
                      std::make_unique<IntExpr>(*iright));
    case BOOLEXPR_NEQ:
      return BoolExpr(BOOLEXPR_EQ, std::make_unique<IntExpr>(*ileft),
                      std::make_unique<IntExpr>(*iright));
    case BOOLEXPR_LT:
      return BoolExpr(BOOLEXPR_GTE, std::make_unique<IntExpr>(*ileft),
                      std::make_unique<IntExpr>(*iright));
    case BOOLEXPR_GT:
      return BoolExpr(BOOLEXPR_LTE, std::make_unique<IntExpr>(*ileft),
                      std::make_unique<IntExpr>(*iright));
    case BOOLEXPR_LTE:
      return BoolExpr(BOOLEXPR_GT, std::make_unique<IntExpr>(*ileft),
                      std::make_unique<IntExpr>(*iright));
    case BOOLEXPR_GTE:
      return BoolExpr(BOOLEXPR_LT, std::make_unique<IntExpr>(*ileft),
                      std::make_unique<IntExpr>(*iright));
    default:
      return BoolExpr(BOOLEXPR_NOT, std::make_unique<BoolExpr>(*this));
  }
}
BoolExpr BoolExpr::operator&&(bool val) const {
  if (val) {
    return BoolExpr(*this);
  } else {
    return false_expr();
  }
}
BoolExpr BoolExpr::operator||(bool val) const {
  if (val) {
    return true_expr();
  } else {
    return BoolExpr(*this);
  }
}
BoolExpr operator&&(bool val, const BoolExpr &e) {
  if (val) {
    return BoolExpr(e);
  } else {
    return BoolExpr::false_expr();
    ;
  }
}
BoolExpr operator||(bool val, const BoolExpr &e) {
  if (val) {
    return BoolExpr::true_expr();
  } else {
    return BoolExpr(e);
  }
}
bool eval(std::vector<std::unique_ptr<BoolExpr>> &conds, const Args &args) {
  bool result = true;
  for (auto &&cond : conds) {
    try {
      result = result && cond->eval(args);
    } catch (const CondEvalException &e) {
      result = false;
    }
  }

  return result;
}
bool check_correct(BoolExpr *cond, const std::set<Args> &pos_examples,
                   const std::set<Args> &neg_examples) {
  if (cond == nullptr) return false;

  for (auto pos_example : pos_examples) {
    try {
      if (!cond->eval(pos_example)) return false;
    } catch (const CondEvalException &e) {
      return false;
    }
  }
  for (auto neg_example : neg_examples) {
    try {
      if (cond->eval(neg_example)) return false;
    } catch (const CondEvalException &e) {
      return false;
    }
  }
  return true;
}
BoolExpr simplify(const BoolExpr &e) {
  // For now, it just simplifies negation
  if (e.t == BOOLEXPR_NOT) return !(*e.b);

  return BoolExpr(e);
}

FunSynthesized::FunSynthesized(std::string name_,
                               std::vector<std::unique_ptr<Param>> params_,
                               std::unique_ptr<BoolExpr> body_)
    : name(name_), params(std::move(params_)), body(std::move(body_)) {}
FunSynthesized::FunSynthesized(std::string name_,
                               std::vector<std::string> param_names,
                               std::unique_ptr<BoolExpr> body_)
    : name(name_), body(std::move(body_)) {
  std::vector<std::unique_ptr<Param>> params_;
  for (auto &param_name : param_names)
    params_.push_back(std::make_unique<Param>(param_name));
  params = std::move(params_);
}
std::string FunSynthesized::to_string() const {
  std::string str = "(define-fun ";
  str += name + " (";
  for (size_t i = 0; i < params.size(); i++) {
    str += "(";
    str += params[i]->to_string() + " Int)";
    if (i != params.size() - 1) {
      str += " ";
    }
  }
  str += ") Bool ";
  str += body->to_string() + ")";
  return str;
}
std::string FunSynthesized::get_name() const { return name; }
std::vector<std::unique_ptr<Param>> FunSynthesized::get_params() const {
  std::vector<std::unique_ptr<Param>> copied;
  for (auto &&param : params) {
    copied.push_back(std::make_unique<Param>(*param));
  }
  return copied;
}
BoolExpr *FunSynthesized::get_body() const { return body.get(); }
z3::expr FunSynthesized::get_z3_expr(z3::context &ctx) const {
  return body->to_z3_expr(ctx);
}
bool FunSynthesized::eval(Args args) const { return body->eval(args); }

}  // namespace pathfinder
