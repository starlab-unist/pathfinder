#include "sygus_parser.h"

#include <cstring>
#include <iostream>

#include "utils.h"

namespace pathfinder {

bool is_delimiter(char c) {
  return c == ' ' || c == '\n' || c == '\t' || c == '\\';
}

void strip(char** cursor) {
  while (is_delimiter(**cursor)) {
    (*cursor)++;
  }
}

void until_delimiter(char** cursor) {
  while (!(is_delimiter(**cursor))) {
    (*cursor)++;
  }
}

bool is_digit(char c) { return '0' <= c && c <= '9'; }

bool is_capital_alphabet(char c) { return 'A' <= c && c <= 'Z'; }

bool is_lower_alphabet(char c) { return 'a' <= c && c <= 'z'; }

bool is_alphabet(char c) {
  return is_capital_alphabet(c) || is_lower_alphabet(c);
}

bool is_alphanumeric(char c) { return is_alphabet(c) || is_digit(c); }

void consume(char** cursor, char c) {
  PATHFINDER_CHECK(**cursor != EOF, "PathFinder Error: cursor reached EOF");

  strip(cursor);
  if (**cursor == c) {
    (*cursor)++;
    strip(cursor);
    return;
  } else {
    PATHFINDER_CHECK(false, std::string("PathFinder Error: expected: ") + c +
                                ", cursor: " + *cursor);
  }
}

void consume(char** cursor, const char* str) {
  PATHFINDER_CHECK(**cursor != EOF, "PathFinder Error: cursor reached EOF");

  strip(cursor);
  if (strncmp(*cursor, str, strlen(str)) == 0) {
    (*cursor) += strlen(str);
    strip(cursor);
  } else {
    PATHFINDER_CHECK(false, "PathFinder Error: expected: " + std::string(str) +
                                ", cursor: " + *cursor);
  }
}

std::string parse_id(char** cursor) {
  PATHFINDER_CHECK(**cursor != EOF, "PathFinder Error: cursor reached EOF");

  strip(cursor);
  char* start = *cursor;
  PATHFINDER_CHECK(is_alphabet(*start) || *start == '_',
                   "PathFinder Error: not a valid identifier. received " +
                       std::string(start));

  int len = 1;

  while (is_alphanumeric(*(start + len)) || *(start + len) == '_') len++;

  *cursor += len;
  return std::string(start, len);
}

std::unique_ptr<Param> parse_param(char** cursor) {
  consume(cursor, '(');
  std::string param_name = parse_id(cursor);
  consume(cursor, "Int");
  consume(cursor, ')');
  return std::make_unique<Param>(param_name);
}

std::unique_ptr<BoolExpr> parse_boolexpr(char** cursor);

std::unique_ptr<IntExpr> parse_intexpr(char** cursor) {
  strip(cursor);
  if ('-' == **cursor && is_digit(*((*cursor)++))) {
    int value = atoi(*cursor);
    while (is_digit(**cursor)) (*cursor)++;
    return std::make_unique<IntExpr>(-value);
  } else if (is_digit(**cursor)) {
    int value = atoi(*cursor);
    while (is_digit(**cursor)) {
      (*cursor)++;
    }
    return std::make_unique<IntExpr>(value);
  } else if (is_alphabet(**cursor) || **cursor == '_') {
    return std::make_unique<IntExpr>(parse_id(cursor));
  } else if (**cursor == '(') {
    consume(cursor, '(');
    std::unique_ptr<IntExpr> ret;
    std::unique_ptr<BoolExpr> cond;
    std::unique_ptr<IntExpr> left;
    std::unique_ptr<IntExpr> right;
    switch (**cursor) {
      case '+':
        consume(cursor, '+');
        left = parse_intexpr(cursor);
        right = parse_intexpr(cursor);
        ret = std::make_unique<IntExpr>(INTEXPR_ADD, std::move(left),
                                        std::move(right));
        break;
      case '-':
        consume(cursor, '-');
        left = parse_intexpr(cursor);
        right = parse_intexpr(cursor);
        ret = std::make_unique<IntExpr>(INTEXPR_SUB, std::move(left),
                                        std::move(right));
        break;
      case '*':
        consume(cursor, '*');
        left = parse_intexpr(cursor);
        right = parse_intexpr(cursor);
        ret = std::make_unique<IntExpr>(INTEXPR_MULT, std::move(left),
                                        std::move(right));
        break;
      case '/':
        consume(cursor, '/');
        left = parse_intexpr(cursor);
        right = parse_intexpr(cursor);
        ret = std::make_unique<IntExpr>(INTEXPR_DIV, std::move(left),
                                        std::move(right));
        break;
      case '%':
        consume(cursor, '%');
        left = parse_intexpr(cursor);
        right = parse_intexpr(cursor);
        ret = std::make_unique<IntExpr>(INTEXPR_MOD, std::move(left),
                                        std::move(right));
        break;
      case 'i':
        consume(cursor, "ite");
        cond = parse_boolexpr(cursor);
        left = parse_intexpr(cursor);
        right = parse_intexpr(cursor);
        ret = std::make_unique<IntExpr>(std::move(cond), std::move(left),
                                        std::move(right));
        break;
      default:
        PATHFINDER_CHECK(false,
                         std::string("PathFinder Error: parse error while "
                                     "parsing an int expression. ") +
                             "expecting one of binary operator, but received " +
                             *cursor);
    }
    consume(cursor, ')');
    return ret;
  }
  PATHFINDER_CHECK(false, std::string("PathFinder Error: parse error while "
                                      "parsing an int expression. received ") +
                              *cursor);
}

std::unique_ptr<BoolExpr> parse_boolexpr(char** cursor) {
  consume(cursor, '(');
  std::unique_ptr<BoolExpr> ret;
  if (strncmp(*cursor, "=", strlen("=")) == 0) {
    consume(cursor, "=");
    std::unique_ptr<IntExpr> ileft = parse_intexpr(cursor);
    std::unique_ptr<IntExpr> iright = parse_intexpr(cursor);
    ret = std::make_unique<BoolExpr>(BOOLEXPR_EQ, std::move(ileft),
                                     std::move(iright));
  } else if (strncmp(*cursor, "<=", strlen("<=")) == 0) {
    consume(cursor, "<=");
    std::unique_ptr<IntExpr> ileft = parse_intexpr(cursor);
    std::unique_ptr<IntExpr> iright = parse_intexpr(cursor);
    ret = std::make_unique<BoolExpr>(BOOLEXPR_LTE, std::move(ileft),
                                     std::move(iright));
  } else if (strncmp(*cursor, ">=", strlen(">=")) == 0) {
    consume(cursor, ">=");
    std::unique_ptr<IntExpr> ileft = parse_intexpr(cursor);
    std::unique_ptr<IntExpr> iright = parse_intexpr(cursor);
    ret = std::make_unique<BoolExpr>(BOOLEXPR_GTE, std::move(ileft),
                                     std::move(iright));
  } else if (strncmp(*cursor, "<", strlen("<")) == 0) {
    consume(cursor, "<");
    std::unique_ptr<IntExpr> ileft = parse_intexpr(cursor);
    std::unique_ptr<IntExpr> iright = parse_intexpr(cursor);
    ret = std::make_unique<BoolExpr>(BOOLEXPR_LT, std::move(ileft),
                                     std::move(iright));
  } else if (strncmp(*cursor, ">", strlen(">")) == 0) {
    consume(cursor, ">");
    std::unique_ptr<IntExpr> ileft = parse_intexpr(cursor);
    std::unique_ptr<IntExpr> iright = parse_intexpr(cursor);
    ret = std::make_unique<BoolExpr>(BOOLEXPR_GT, std::move(ileft),
                                     std::move(iright));
  } else if (strncmp(*cursor, "and", strlen("and")) == 0) {
    consume(cursor, "and");
    std::unique_ptr<BoolExpr> bleft = parse_boolexpr(cursor);
    std::unique_ptr<BoolExpr> bright = parse_boolexpr(cursor);
    ret = std::make_unique<BoolExpr>(BOOLEXPR_AND, std::move(bleft),
                                     std::move(bright));
  } else if (strncmp(*cursor, "or", strlen("or")) == 0) {
    consume(cursor, "or");
    std::unique_ptr<BoolExpr> bleft = parse_boolexpr(cursor);
    std::unique_ptr<BoolExpr> bright = parse_boolexpr(cursor);
    ret = std::make_unique<BoolExpr>(BOOLEXPR_OR, std::move(bleft),
                                     std::move(bright));
  } else if (strncmp(*cursor, "not", strlen("not")) == 0) {
    consume(cursor, "not");
    std::unique_ptr<BoolExpr> b = parse_boolexpr(cursor);
    ret = std::make_unique<BoolExpr>(BOOLEXPR_NOT, std::move(b));
  } else {
    PATHFINDER_CHECK(false,
                     std::string("PathFinder Error: parse error while parsing "
                                 "an bool expression. ") +
                         "Expecting one of boolean operator, but received " +
                         *cursor);
  }
  consume(cursor, ')');
  return ret;
}

std::unique_ptr<FunSynthesized> parse_fun(std::string fun_str) {
  std::string fun_name;
  std::vector<std::unique_ptr<Param>> params;
  std::unique_ptr<BoolExpr> body;

  char* fun_str_ = strdup(fun_str.c_str());
  char* cursor = fun_str_;

  consume(&cursor, '(');

  consume(&cursor, "define-fun");
  fun_name = parse_id(&cursor);

  consume(&cursor, '(');
  while (*cursor != ')') {
    params.push_back(parse_param(&cursor));
  }
  consume(&cursor, ')');

  consume(&cursor, "Bool");

  body = parse_boolexpr(&cursor);

  free(fun_str_);

  return std::make_unique<FunSynthesized>(fun_name, std::move(params),
                                          std::move(body));
}

}  // namespace pathfinder
