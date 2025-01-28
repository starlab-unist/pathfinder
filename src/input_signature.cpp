#include "input_signature.h"

#include <cassert>

#include "utils.h"

namespace pathfinder {

EnumParam::EnumParam(std::string name_, std::vector<std::string> entries_)
    : name(name_), entries(entries_) {
  start = 0;
  size = entries.size();
}
EnumParam::EnumParam(std::string name_, size_t start_, size_t size_)
    : name(name_), start(start_), size(size_) {}
std::string EnumParam::get_name() const { return name; }
size_t EnumParam::get_start() const { return start; }
size_t EnumParam::get_size() const { return size; }
std::vector<std::string> EnumParam::get_entries() const { return entries; }
std::string EnumParam::to_string(long value) const {
  size_t value_ = (size_t)value;
  assert(start <= value_ && value_ < start + size);
  if (entries.size() > 0) {
    size_t idx = value_ - start;
    return entries[idx];
  } else {
    return std::to_string(value_);
  }
}

NumericParam::NumericParam(std::string name_) : name(name_) {}
std::string NumericParam::get_name() const { return name; }
std::string NumericParam::to_string(long value) const {
  return std::to_string(value);
}

InputSignature::InputSignature() {}
void InputSignature::push(EnumParam enum_param) {
  check_duplicate_and_register(enum_param.name);
  enum_params.push_back(enum_param);

  for (auto& enum_param_group : enum_param_groups) {
    assert(!enum_param_group.empty());
    if (enum_param_group[0].start == enum_param.start &&
        enum_param_group[0].size == enum_param.size) {
      enum_param_group.push_back(enum_param);
      return;
    }
  }
  enum_param_groups.push_back(std::vector<EnumParam>({enum_param}));
}
void InputSignature::push(NumericParam int_param) {
  check_duplicate_and_register(int_param.name);
  numeric_params.push_back(int_param);
}
void InputSignature::check_duplicate_and_register(std::string name) {
  PATHFINDER_CHECK(
      name_set.find(name) == name_set.end(),
      "PathFinder Error: parameter name " + name + " is duplicated");
  name_set.insert(name);
}

InputSignature input_signature;

void register_enum_param(std::string name, std::vector<std::string> entries) {
  input_signature.push(EnumParam(name, entries));
}
void register_enum_param(std::string name, size_t start, size_t size) {
  input_signature.push(EnumParam(name, start, size));
}
void register_int_param(std::string name) {
  input_signature.push(NumericParam(name));
}
const std::vector<EnumParam>& get_enum_params() {
  return input_signature.enum_params;
}
const std::vector<std::vector<EnumParam>>& get_enum_param_groups() {
  return input_signature.enum_param_groups;
}
const std::vector<NumericParam>& get_numeric_params() {
  return input_signature.numeric_params;
}
std::vector<std::string> get_enum_param_names() {
  std::vector<std::string> enum_param_names;
  for (auto& enum_param : get_enum_params())
    enum_param_names.push_back(enum_param.get_name());
  return enum_param_names;
}
std::vector<std::string> get_numeric_param_names() {
  std::vector<std::string> numeric_param_names;
  for (auto& numeric_param : get_numeric_params())
    numeric_param_names.push_back(numeric_param.get_name());
  return numeric_param_names;
}
size_t enum_params_size() { return get_enum_params().size(); }
size_t int_params_size() { return get_numeric_params().size(); }
size_t params_size() { return enum_params_size() + int_params_size(); }
long enum_value_at(Args enum_args, size_t idx) {
  assert(idx < enum_params_size());
  std::string idx_th_name = get_enum_params()[idx].get_name();
  return enum_args[idx_th_name];
}
long numeric_value_at(Args numeric_args, size_t idx) {
  assert(idx < int_params_size());
  std::string idx_th_name = get_numeric_params()[idx].get_name();
  return numeric_args[idx_th_name];
}
std::string enum_args_to_string(const Args& enum_args) {
  std::string str = "(";

  auto enum_params = get_enum_params();
  for (size_t i = 0; i < enum_params.size(); i++) {
    str += enum_params[i].to_string(enum_args.at(enum_params[i].get_name()));
    if (i != enum_params.size() - 1) str += ",";
  }
  str += ")";

  return str;
}
std::string numeric_args_to_string(const Args& numeric_args) {
  std::string str = "(";

  auto numeric_params = get_numeric_params();
  for (size_t i = 0; i < numeric_params.size(); i++) {
    str += numeric_params[i].to_string(
        numeric_args.at(numeric_params[i].get_name()));
    if (i != numeric_params.size() - 1) str += ",";
  }
  str += ")";

  return str;
}
std::string input_to_string(const Input& input) {
  std::string str = "(";

  auto enum_params = get_enum_params();
  for (size_t i = 0; i < enum_params.size(); i++) {
    str += enum_params[i].to_string(
        input.get_enum_args().at(enum_params[i].get_name()));
    if (i != enum_params.size() - 1) str += ",";
  }
  auto numeric_params = get_numeric_params();
  if (enum_params.size() > 0 && numeric_params.size() > 0) str += ",";
  for (size_t i = 0; i < numeric_params.size(); i++) {
    str += numeric_params[i].to_string(
        input.get_numeric_args().at(numeric_params[i].get_name()));
    if (i != numeric_params.size() - 1) str += ",";
  }
  str += ")";

  return str;
}
std::vector<long> serialize(const Input& input) {
  std::vector<long> data;
  auto enum_args = input.get_enum_args();
  auto numeric_args = input.get_numeric_args();

  for (auto& enum_param : get_enum_params())
    data.push_back(enum_args[enum_param.get_name()]);
  for (auto& int_param : get_numeric_params())
    data.push_back(numeric_args[int_param.get_name()]);
  return data;
}
std::optional<Input> deserialize(const std::vector<long>& data) {
  Args enum_args;
  Args numeric_args;
  if (data.size() < params_size()) {
    std::cout << "PathFinder Warning: deserialization failed. expected "
              << params_size() << " args, but found " << data.size()
              << std::endl;
    return std::nullopt;
  }

  if (data.size() > params_size()) {
    std::cout << "PathFinder Warning: deserialization failed. expected "
              << params_size() << " args, but found " << data.size()
              << ". remainder is truncated";
  }

  for (size_t i = 0; i < enum_params_size(); i++)
    enum_args.insert({get_enum_params()[i].get_name(), data[i]});
  for (size_t i = 0; i < int_params_size(); i++)
    numeric_args.insert(
        {get_numeric_params()[i].get_name(), data[enum_params_size() + i]});

  return Input(enum_args, numeric_args);
}

}  // namespace pathfinder
