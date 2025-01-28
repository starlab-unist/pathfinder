#ifndef PATHFINDER_DEFS
#define PATHFINDER_DEFS

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace pathfinder {

typedef uint32_t PCID;
typedef std::vector<PCID> ExecPath;  // Execution path

typedef std::map<std::string, long> Args;

class Input {
 public:
  Input() {};
  Input(Args enum_args_, Args numeric_args_)
      : enum_args(enum_args_), numeric_args(numeric_args_) {}
  const Args& get_enum_args() const { return enum_args; }
  const Args& get_numeric_args() const { return numeric_args; }
  bool operator<(const Input& other) const {
    if (enum_args < other.enum_args)
      return true;
    else if (other.enum_args < enum_args)
      return false;
    else
      return numeric_args < other.numeric_args;
  }
  long operator[](std::string key) const {
    if (enum_args.find(key) != enum_args.end())
      return enum_args.at(key);
    else
      return numeric_args.at(key);
  }

 private:
  Args enum_args;
  Args numeric_args;
};

enum CondType {
  CT_ENUM,
  CT_NUMERIC,
  CT_NEGLECT,
};

typedef int (*UserCallback)(const Input& input);

int driver(UserCallback Callback);

// When fuzz target returns -1, pathfinder ignores it.
const int PATHFINDER_PASS = -1;
const int PATHFINDER_EXPECTED_EXCEPTION = -2;
const int PATHFINDER_UNEXPECTED_EXCEPTION = -3;

}  // namespace pathfinder

#endif
