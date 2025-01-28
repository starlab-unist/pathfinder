#include "enumarg_bitvec.h"

#include <cassert>
#include <memory>

#include "utils.h"

namespace pathfinder {

static const size_t ENUM_SIZE_MAX = 64;

EnumArgBitVecArray enum_bvs_template;

void register_enum_bv(std::string name, std::vector<std::string> entries) {
  enum_bvs_template.push(EnumArgBitVec(name, entries));
}
void register_enum_bv(std::string name, size_t start, size_t size) {
  enum_bvs_template.push(EnumArgBitVec(name, start, size));
}

EnumArgBitVecArray initial_enum_bvs(bool set_all) {
  EnumArgBitVecArray enum_bvs = EnumArgBitVecArray(enum_bvs_template);
  if (set_all) enum_bvs.set_all();
  return enum_bvs;
}

EnumArgBitVec::EnumArgBitVec() {}
EnumArgBitVec::EnumArgBitVec(std::string name_,
                             std::vector<std::string> entries_)
    : name(name_), entries(entries_) {
  assert(0 < entries.size() && entries.size() <= ENUM_SIZE_MAX);
  start = 0;
  size = entries.size();
  init_mask();
}
EnumArgBitVec::EnumArgBitVec(std::string name_, size_t start_, size_t size_)
    : name(name_), start(start_), size(size_) {
  assert(0 < size && size <= ENUM_SIZE_MAX);
  init_mask();
}
EnumArgBitVec::EnumArgBitVec(const EnumArgBitVec& other) {
  name = other.name;
  start = other.start;
  size = other.size;
  entries = other.entries;
  mask = other.mask;
  bitvec = other.bitvec;
}
void EnumArgBitVec::operator=(const EnumArgBitVec& other) {
  name = other.name;
  start = other.start;
  size = other.size;
  entries = other.entries;
  mask = other.mask;
  bitvec = other.bitvec;
}
bool EnumArgBitVec::operator==(const EnumArgBitVec& other) {
  return name == other.name && start == other.start && size == other.size &&
         mask == other.mask && bitvec == other.bitvec &&
         entries == other.entries;
}
std::string EnumArgBitVec::get_name() const { return name; }
void EnumArgBitVec::check_name() {
  for (size_t i = 0; i < entries.size() - 1; i++)
    for (size_t j = i + 1; j < entries.size(); j++)
      if (entries[i] == entries[j])
        PATHFINDER_CHECK(false, "Enum arg error: duplicate enum entry `" +
                                    entries[i] + "` in enum `" + name + "`");
}
bool EnumArgBitVec::eval(Args args) const {
  long val = args[get_name()];
  size_t bitfield = idx_to_bitfield(val);
  return (bitvec & bitfield) ^ bitfield;
}
bool EnumArgBitVec::eval(const std::set<Args>& args_set) const {
  for (auto& args : args_set)
    if (!eval(args)) return false;
  return true;
}
void EnumArgBitVec::init_mask() {
  // suppose size == 3
  mask = 0;             // 0000 0000
  mask = ~mask;         // 1111 1111
  mask = mask << size;  // 1111 1000
  mask = ~mask;         // 0000 0111
}
void EnumArgBitVec::set_all() { bitvec = mask; }
void EnumArgBitVec::unset_all() { bitvec = 0; }
std::optional<size_t> EnumArgBitVec::draw() const {
  if (empty()) return std::nullopt;

  auto candidate = bitfield_to_idx();
  assert(candidate.size() > 0);
  auto val = start + candidate[rand() % candidate.size()];
  return val;
}
void EnumArgBitVec::set(size_t value) {
  assert(start <= value && value < start + size);
  bitvec |= idx_to_bitfield(value - start);
}
bool EnumArgBitVec::empty() const { return bitvec == 0; }
bool EnumArgBitVec::full() const { return bitvec == mask; }
bool EnumArgBitVec::same(const EnumArgBitVec& other) const {
  return name == other.name && start == other.start && size == other.size;
}
bool EnumArgBitVec::exclusive(const EnumArgBitVec& other) const {
  assert(start == other.start && size == other.size);
  return (bitvec & other.bitvec) == 0;
}
bool EnumArgBitVec::complement(const EnumArgBitVec& other) const {
  assert(start == other.start && size == other.size);
  return (bitvec | other.bitvec) == mask;
}
bool EnumArgBitVec::in(const EnumArgBitVec& other) const {
  assert(start == other.start && size == other.size);
  return (bitvec | other.bitvec) == other.bitvec;
}
void EnumArgBitVec::bit_and(const EnumArgBitVec& other) {
  assert(start == other.start && size == other.size);
  bitvec = bitvec & other.bitvec;
}
void EnumArgBitVec::bit_or(const EnumArgBitVec& other) {
  assert(start == other.start && size == other.size);
  bitvec = bitvec | other.bitvec;
}
void EnumArgBitVec::exclude(const EnumArgBitVec& other) {
  assert(start == other.start && size == other.size);
  bitvec = bitvec & ~other.bitvec;
}
void EnumArgBitVec::negate() { bitvec = ~bitvec & mask; }
std::optional<EnumArgBitVec> EnumArgBitVec::extract_random_bit() const {
  EnumArgBitVec extracted = EnumArgBitVec(*this);
  auto value_opt = draw();
  if (!value_opt.has_value()) return std::nullopt;

  extracted.unset_all();
  extracted.set(value_opt.value());
  return extracted;
}
size_t EnumArgBitVec::num_set_bit() const { return bitfield_to_idx().size(); }
bool EnumArgBitVec::operator==(const EnumArgBitVec& other) const {
  assert(start == other.start && size == other.size);
  return bitvec == other.bitvec;
}
bool EnumArgBitVec::operator!=(const EnumArgBitVec& other) const {
  assert(start == other.start && size == other.size);
  return !(*this == other);
}
EnumArgBitVec EnumArgBitVec::operator&(const EnumArgBitVec& other) {
  assert(start == other.start && size == other.size);
  EnumArgBitVec result(*this);
  result.bit_and(other);
  return result;
}
EnumArgBitVec EnumArgBitVec::operator|(const EnumArgBitVec& other) {
  assert(start == other.start && size == other.size);
  EnumArgBitVec result(*this);
  result.bit_or(other);
  return result;
}
EnumArgBitVec EnumArgBitVec::operator~() const {
  EnumArgBitVec result(*this);
  result.negate();
  return result;
}
std::vector<size_t> EnumArgBitVec::bitfield_to_idx() const {
  std::vector<size_t> bitfield_idx;
  for (size_t i = 0; i < size; i++) {
    size_t shift_amt = size - 1 - i;
    if ((bitvec >> shift_amt) & 1) bitfield_idx.push_back(i);
  }
  return bitfield_idx;
}
size_t EnumArgBitVec::idx_to_bitfield(size_t idx) const {
  assert(idx < ENUM_SIZE_MAX);
  size_t shift_amt = size - 1 - idx;
  return 1 << shift_amt;
}
std::string EnumArgBitVec::to_string(bool negate) const {
  std::string relation = negate ? unicode_setnotin : unicode_setin;
  std::string str = name + " " + relation + " {";
  std::vector<size_t> bitfield_idx = bitfield_to_idx();
  for (size_t i = 0; i < bitfield_idx.size(); i++) {
    if (entries.size() > 0)
      str += entries[bitfield_idx[i]];
    else
      str += std::to_string(start + bitfield_idx[i]);

    if (i != bitfield_idx.size() - 1) str += ",";
  }
  str += "}";
  return str;
}

EnumArgBitVecArray::EnumArgBitVecArray() {}
EnumArgBitVecArray::EnumArgBitVecArray(
    std::vector<std::unique_ptr<EnumArgBitVec>> array_)
    : array(std::move(array_)) {}
EnumArgBitVecArray::EnumArgBitVecArray(const EnumArgBitVecArray& other) {
  for (auto&& bv : other.array) push(*bv);
}
EnumArgBitVecArray::EnumArgBitVecArray(EnumArgBitVecArray&& other) {
  for (auto&& bv : other.array) push(*bv);
  other.array.clear();
}
void EnumArgBitVecArray::operator=(const EnumArgBitVecArray& other) {
  array.clear();
  for (auto&& bv : other.array) push(*bv);
}
void EnumArgBitVecArray::check_name() {
  for (size_t i = 0; i < array.size() - 1; i++)
    for (size_t j = i + 1; j < array.size(); j++)
      if (array[i]->name == array[j]->name)
        PATHFINDER_CHECK(false, "Enum arg error: duplicate enum name `" +
                                    array[i]->name + "`");
  for (auto&& bv : array) bv->check_name();
}
void EnumArgBitVecArray::set_all() {
  for (auto&& bv : array) bv->set_all();
}
void EnumArgBitVecArray::set(const Args& enum_args) {
  for (auto& bv : array) bv->set((size_t)enum_args.at(bv->name));
}
void EnumArgBitVecArray::push(const EnumArgBitVec& bv) {
  array.push_back(std::make_unique<EnumArgBitVec>(bv));
}
size_t EnumArgBitVecArray::size() const { return array.size(); }
bool EnumArgBitVecArray::empty() const {
  for (auto& bv : array)
    if (!bv->empty()) return false;
  return true;
}
bool EnumArgBitVecArray::full() const {
  for (auto& bv : array)
    if (!bv->full()) return false;
  return true;
}
bool EnumArgBitVecArray::in(const EnumArgBitVecArray& other) const {
  assert(size() == other.size());
  for (size_t i = 0; i < array.size(); i++)
    if (!array[i]->in(*other.array[i])) return false;
  return true;
}
void EnumArgBitVecArray::bit_and(const EnumArgBitVecArray& other) {
  assert(size() == other.size());
  for (size_t i = 0; i < array.size(); i++) array[i]->bit_and(*other.array[i]);
}
void EnumArgBitVecArray::bit_or(const EnumArgBitVecArray& other) {
  assert(size() == other.size());
  for (size_t i = 0; i < array.size(); i++) array[i]->bit_or(*other.array[i]);
}
void EnumArgBitVecArray::bit_or(const EnumArgBitVec& other) {
  for (auto& bv : array) {
    if (bv->name == other.name) {
      bv->bit_or(other);
      break;
    }
  }
}
void EnumArgBitVecArray::negate() {
  for (size_t i = 0; i < array.size(); i++) array[i]->negate();
}
EnumArgBitVecArray EnumArgBitVecArray::distinct(
    const EnumArgBitVecArray& other) const {
  assert(array.size() == other.size());
  EnumArgBitVecArray distinct_bvs = initial_enum_bvs(false);
  for (size_t i = 0; i < array.size(); i++)
    if (array[i]->exclusive(*other.array[i]))
      distinct_bvs.array[i] = std::make_unique<EnumArgBitVec>(*other.array[i]);
  return distinct_bvs;
}
EnumArgBitVec EnumArgBitVecArray::export_non_empty_bv() const {
  for (auto& bv : array)
    if (!bv->empty()) return EnumArgBitVec(*bv);
  throw Unreachable();
}
bool EnumArgBitVecArray::operator==(const EnumArgBitVecArray& other) const {
  assert(array.size() == other.size());
  for (size_t i = 0; i < array.size(); i++)
    if (*array[i] != *other.array[i]) return false;
  return true;
}
bool EnumArgBitVecArray::operator!=(const EnumArgBitVecArray& other) const {
  assert(array.size() == other.size());
  return !(*this == other);
}
const EnumArgBitVec& EnumArgBitVecArray::operator[](size_t i) const {
  assert(i < array.size());
  return *array[i];
}
EnumArgBitVec* EnumArgBitVecArray::operator[](std::string name) {
  for (auto& bv : array)
    if (bv->get_name() == name) return bv.get();
  throw Unreachable();
}
std::vector<std::string> EnumArgBitVecArray::to_string() const {
  std::vector<std::string> strs;
  for (auto& bv : array) strs.push_back(bv->to_string());
  return strs;
}
std::map<std::string, std::vector<size_t>> EnumArgBitVecArray::get_idx_map()
    const {
  std::map<std::string, std::vector<size_t>> idx_map;
  for (auto&& bv : array) idx_map.insert({bv->name, bv->bitfield_to_idx()});
  return idx_map;
}

}  // namespace pathfinder
