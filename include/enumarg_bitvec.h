#ifndef PATHFINDER_ENUMARG_BITVEC
#define PATHFINDER_ENUMARG_BITVEC

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "pathfinder_defs.h"

namespace pathfinder {

void register_enum_bv(std::string name, std::vector<std::string> entries);
void register_enum_bv(std::string name, size_t start, size_t size);

class EnumArgBitVec {
 public:
  EnumArgBitVec();
  EnumArgBitVec(std::string name_, std::vector<std::string> entries_);
  EnumArgBitVec(std::string name_, size_t start_, size_t size_);
  EnumArgBitVec(const EnumArgBitVec& other);
  void operator=(const EnumArgBitVec& other);
  bool operator==(const EnumArgBitVec& other);
  std::string get_name() const;
  void check_name();
  bool eval(Args args) const;
  bool eval(const std::set<Args>& args_set) const;
  void set_all();
  void unset_all();
  std::optional<size_t> draw() const;
  void set(size_t value);
  bool empty() const;
  bool full() const;
  bool exclusive(const EnumArgBitVec& other) const;
  bool complement(const EnumArgBitVec& other) const;
  bool in(const EnumArgBitVec& other) const;
  void bit_and(const EnumArgBitVec& other);
  void bit_or(const EnumArgBitVec& other);
  void exclude(const EnumArgBitVec& other);
  void negate();
  std::optional<EnumArgBitVec> extract_random_bit() const;
  size_t num_set_bit() const;
  bool operator==(const EnumArgBitVec& other) const;
  bool operator!=(const EnumArgBitVec& other) const;
  EnumArgBitVec operator&(const EnumArgBitVec& other);
  EnumArgBitVec operator|(const EnumArgBitVec& other);
  EnumArgBitVec operator~() const;
  std::string to_string(bool negate = false) const;

 private:
  void init_mask();
  bool same(const EnumArgBitVec& other) const;
  std::vector<size_t> bitfield_to_idx() const;
  size_t idx_to_bitfield(size_t idx) const;

  std::string name;
  size_t start;
  size_t size;
  std::vector<std::string> entries;
  unsigned long mask;
  unsigned long bitvec = 0;

  friend class EnumArgBitVecArray;
};

class EnumArgBitVecArray {
 public:
  EnumArgBitVecArray();
  EnumArgBitVecArray(std::vector<std::unique_ptr<EnumArgBitVec>> array);
  EnumArgBitVecArray(const EnumArgBitVecArray& other);
  EnumArgBitVecArray(EnumArgBitVecArray&& other);
  void operator=(const EnumArgBitVecArray& other);
  void check_name();
  void set_all();
  void set(const Args& enum_args);
  void push(const EnumArgBitVec& bv);
  size_t size() const;
  bool empty() const;
  bool full() const;
  bool in(const EnumArgBitVecArray& other) const;
  void bit_and(const EnumArgBitVecArray& other);
  void bit_or(const EnumArgBitVecArray& other);
  void bit_or(const EnumArgBitVec& other);
  void negate();
  EnumArgBitVecArray distinct(const EnumArgBitVecArray& other) const;
  EnumArgBitVec export_non_empty_bv() const;
  bool operator==(const EnumArgBitVecArray& other) const;
  bool operator!=(const EnumArgBitVecArray& other) const;
  const EnumArgBitVec& operator[](size_t i) const;
  EnumArgBitVec* operator[](std::string name);
  std::vector<std::string> to_string() const;
  std::map<std::string, std::vector<size_t>> get_idx_map() const;

 private:
  std::vector<std::unique_ptr<EnumArgBitVec>> array;
};

EnumArgBitVecArray initial_enum_bvs(bool set_all);

}  // namespace pathfinder

#endif
