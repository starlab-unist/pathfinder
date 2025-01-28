#ifndef PATHFINDER_INPUT_SIGNATURE
#define PATHFINDER_INPUT_SIGNATURE

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "pathfinder_defs.h"

namespace pathfinder {

class EnumParam {
 public:
  EnumParam(std::string name_, std::vector<std::string> entries_);
  EnumParam(std::string name_, size_t start_, size_t size_);
  std::string get_name() const;
  size_t get_start() const;
  size_t get_size() const;
  std::vector<std::string> get_entries() const;
  std::string to_string(long value) const;

 private:
  std::string name;
  size_t start;
  size_t size;
  std::vector<std::string> entries;

  friend class InputSignature;
};

class NumericParam {
 public:
  NumericParam(std::string name_);
  std::string get_name() const;
  std::string to_string(long value) const;

 private:
  std::string name;

  friend class InputSignature;
};

class InputSignature {
 public:
  InputSignature();

 private:
  void push(EnumParam enum_param);
  void push(NumericParam int_param);
  void check_duplicate_and_register(std::string name);

  std::vector<EnumParam> enum_params;
  std::vector<std::vector<EnumParam>> enum_param_groups;
  std::vector<NumericParam> numeric_params;
  std::set<std::string> name_set;

  friend void register_enum_param(std::string name,
                                  std::vector<std::string> entries);
  friend void register_enum_param(std::string name, size_t size, size_t start);
  friend void register_int_param(std::string name);
  friend size_t size();
  friend size_t enum_params_size();
  friend size_t int_params_size();
  friend const std::vector<EnumParam>& get_enum_params();
  friend const std::vector<std::vector<EnumParam>>& get_enum_param_groups();
  friend const std::vector<NumericParam>& get_numeric_params();
  friend long enum_value_at(Args enum_args, size_t idx);
  friend long numeric_value_at(Args numeric_args, size_t idx);
  friend std::string enum_args_to_string(const Args& enum_args);
  friend std::string numeric_args_to_string(const Args& numeric_args);
  friend std::string input_to_string(const Input& input);
  friend std::vector<long> serialize(const Input& input);
  friend std::optional<Input> deserialize(const std::vector<long>& data);
};

void register_enum_param(std::string name, std::vector<std::string> entries);
void register_enum_param(std::string name, size_t start, size_t size);
void register_int_param(std::string name);
size_t enum_params_size();
size_t int_params_size();
size_t params_size();
const std::vector<EnumParam>& get_enum_params();
const std::vector<std::vector<EnumParam>>& get_enum_param_groups();
const std::vector<NumericParam>& get_numeric_params();
std::vector<std::string> get_enum_param_names();
std::vector<std::string> get_numeric_param_names();
long enum_value_at(Args enum_args, size_t idx);
long numeric_value_at(Args numeric_args, size_t idx);
std::string enum_args_to_string(const Args& enum_args);
std::string numeric_args_to_string(const Args& numeric_args);
std::string input_to_string(const Input& input);
std::vector<long> serialize(const Input& input);
std::optional<Input> deserialize(const std::vector<long>& data);

}  // namespace pathfinder

#endif
