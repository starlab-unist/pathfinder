#ifndef SYGUS_PARSER
#define SYGUS_PARSER

#include "sygus_ast.h"

namespace pathfinder {

std::unique_ptr<FunSynthesized> parse_fun(std::string fun_str);

}  // namespace pathfinder

#endif
