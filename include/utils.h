#ifndef PATHFINDER_UTILS
#define PATHFINDER_UTILS

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <random>
#include <set>
#include <unordered_map>
#include <unordered_set>
namespace fs = std::filesystem;

namespace pathfinder {

class Unreachable : public std::exception {};

extern std::chrono::steady_clock::time_point start_time;
extern std::chrono::steady_clock::time_point timer_checkpoint;

#define log_msg(verbose_level, msg) \
  if (V_LEVEL >= verbose_level) std::cout << msg << std::flush;

#define log_msg_only(verbose_level, msg) \
  if (V_LEVEL == verbose_level) std::cout << msg << std::flush;

#define add_str(verbose_level, str, msg) \
  if (V_LEVEL >= verbose_level) str += msg;

#define add_str_only(verbose_level, str, msg) \
  if (V_LEVEL == verbose_level) str += msg;

#define PATHFINDER_CHECK(cond, msg) \
  if (!(cond)) {                    \
    std::cerr << msg << std::endl;  \
    exit(0);                        \
  }

#define PATHFINDER_TIMER(timer, ...)                   \
  timer_checkpoint = std::chrono::steady_clock::now(); \
  __VA_ARGS__;                                         \
  timer += elapsed_from_ns(timer_checkpoint)

const std::string unicode_setin = "\u2208";
const std::string unicode_setnotin = "\u2209";
const std::string unicode_neq = "\u2260";
const std::string unicode_lte = "\u2264";
const std::string unicode_gte = "\u2265";
const std::string unicode_and = "\u2227";
const std::string unicode_or = "\u2228";
const std::string unicode_not = "\u00AC";

std::string exec(std::string cmd);

std::string read_from_file(std::string filename);
void write_to_file(std::string filename, std::string contents);
void append_to_file(std::string filename, std::string contents);

std::string singleline();
std::string doubleline();

std::string indent(int depth);
std::string side_align(std::string left, std::string right, size_t width);
std::string right_align(std::string str, size_t width);

float rand_float();

void prepare_random_seed();
void prepare_corpus();

size_t elapsed_from_s(std::chrono::steady_clock::time_point from);
size_t elapsed_from_ms(std::chrono::steady_clock::time_point from);
size_t elapsed_from_us(std::chrono::steady_clock::time_point from);
size_t elapsed_from_ns(std::chrono::steady_clock::time_point from);
size_t ns_to_ms(size_t time_in_ns);
float ns_to_s(size_t time_in_ns);
bool almost_zero(float seconds);

bool is_number(std::string str);
bool is_prefix_of(std::string pre, std::string base);
std::pair<std::string, std::string> split(std::string str, char sep);
std::tuple<std::string, std::string, std::string> split_comp(
    std::string constraint);
std::vector<std::string> split_all(std::string str, char sep);
std::string strip(std::string str);
std::string rm_non_numeric(std::string str);
std::string rm_leading_zeros(std::string numeric_str);

std::string epath_to_string(const std::vector<uint32_t>& epath);
std::ostream& operator<<(std::ostream& os, const std::vector<uintptr_t>& epath);

std::vector<long> cmd_input_to_vec();

std::vector<uint8_t> file_to_vector(const fs::path& filepath);
std::vector<long> uint8_vec_to_long_vec(std::vector<uint8_t> uint8_vec);
std::vector<fs::path> list_files_in_dir(const fs::path& dirpath);

void check_duet();
std::string duet_cmd(std::string sygus_file_name, float timeout);

template <typename T>
T sum(std::vector<T> values) {
  assert(!values.empty());
  T total = values[0];
  for (size_t i = 1; i < values.size(); i++) total += values[i];
  return total;
}

template <typename T>
T avg(std::vector<T> values) {
  assert(!values.empty());
  T total = sum(values);
  return total / values.size();
}

template <typename T>
size_t common_prefix_length(const std::vector<T>& left,
                            const std::vector<T>& right) {
  // returns length of longest common prefix.
  size_t i = 0;
  while (i < left.size() && i < right.size()) {
    if (left[i] != right[i]) {
      break;
    }
    i++;
  }
  return i;
}
template <typename T>
bool is_prefix(std::vector<T> may_prefix, std::vector<T> target) {
  return common_prefix_length(may_prefix, target) == may_prefix.size();
}
template <typename T>
std::vector<T> subvec(std::vector<T> orig, int start) {
  typename std::vector<T>::const_iterator first = orig.begin() + start;
  typename std::vector<T>::const_iterator last = orig.end();
  return std::vector<T>(first, last);
}
template <typename T>
std::vector<T> subvec(std::vector<T> orig, int start, int len) {
  typename std::vector<T>::const_iterator first = orig.begin() + start;
  typename std::vector<T>::const_iterator last = first + len;
  return std::vector<T>(first, last);
}
template <typename T>
std::vector<T> vec_concat(std::vector<T> left, std::vector<T> right) {
  left.insert(left.end(), right.begin(), right.end());
  return left;
}
template <typename T>
std::string vec_to_string(std::vector<T> arg) {
  std::string str = "(";
  for (size_t i = 0; i < arg.size(); i++) {
    str += std::to_string(arg[i]);
    if (i != arg.size() - 1) str += ",";
  }
  str += ")";
  return str;
}

template <typename T>
T random_choice(std::set<T> from) {
  assert(!from.empty());
  size_t pos = (std::rand() % from.size());
  auto it = from.begin();
  std::advance(it, pos);
  return *it;
}
template <typename T>
T random_choice(std::unordered_set<T> from) {
  assert(!from.empty());
  size_t pos = (std::rand() % from.size());
  auto it = from.begin();
  std::advance(it, pos);
  return *it;
}
template <typename K, typename V>
const V& random_choice(std::unordered_map<K, V> from) {
  assert(!from.empty());
  size_t pos = (std::rand() % from.size());
  auto it = from.begin();
  std::advance(it, pos);
  return it->second;
}
template <typename T>
std::set<T> random_sample(std::set<T> orig, size_t sample_size) {
  if (orig.size() <= sample_size) return orig;

  // random indices
  std::vector<size_t> indices(orig.size());  // vector with 100 ints.
  std::iota(std::begin(indices), std::end(indices),
            0);  // Fill with 0, 1, ..., orig.size()-1.
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(indices.begin(), indices.end(), g);
  std::vector<size_t> indices_sampled(&indices[0], &indices[sample_size]);
  std::sort(indices.begin(), indices.end(), std::greater<size_t>());

  std::set<T> sampled;
  for (size_t i = 0; i < sample_size; i++) {
    auto it = orig.begin();
    std::advance(it, indices_sampled[i]);
    sampled.insert(*it);
  }
  return sampled;
}

template <typename T>
bool exclude_intersection(std::set<T>& left, std::set<T>& right) {
  // return whether there was intersection
  std::set<T> intersection;
  for (auto& l : left)
    if (right.find(l) != right.end()) intersection.insert(l);

  if (intersection.empty()) return false;

  for (auto& e : intersection) {
    left.erase(e);
    right.erase(e);
  }
  return true;
}

}  // namespace pathfinder

#endif
