#include "utils.h"

#include <sys/types.h>

#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <future>
#include <optional>
#include <sstream>

#include "duet.h"
#include "options.h"

#define LINE_LENGTH 175
#define INDENT 4

namespace pathfinder {

std::chrono::steady_clock::time_point start_time;
std::chrono::steady_clock::time_point timer_checkpoint;

VERBOSE_LEVEL V_LEVEL = VERBOSE_LOW;

// from https://stackoverflow.com/questions/478898
std::string exec(std::string cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"),
                                                pclose);
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

// from https://www.w3schools.com/cpp/cpp_files.asp
std::string read_from_file(std::string filename) {
  std::string content = "";
  std::string buffer;

  std::ifstream f(filename);
  while (getline(f, buffer)) {
    content += buffer + "\n";
  }
  f.close();
  return content;
}

void write_to_file(std::string filename, std::string contents) {
  std::ofstream out(filename);
  out << contents;
  out.close();
}

void append_to_file(std::string filename, std::string contents) {
  std::ofstream out(filename, std::ios_base::app);
  out << contents;
  out.close();
}

std::string singleline() { return std::string(LINE_LENGTH, '-') + "\n"; }

std::string doubleline() { return std::string(LINE_LENGTH, '=') + "\n"; }

std::string indent(int depth) { return std::string(INDENT * depth, ' '); }

std::string side_align(std::string left, std::string right, size_t width) {
  int space_len = left.size() + right.size() >= width
                      ? 1
                      : width - (left.size() + right.size());
  std::string space = std::string(space_len, ' ');
  return left + space + right;
}

std::string right_align(std::string str, size_t width) {
  int space_len = str.size() >= width ? 1 : width - str.size();
  std::string space = std::string(space_len, ' ');
  return space + str;
}

float rand_float() {
  return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

void prepare_random_seed() { std::srand(time(0)); }

void prepare_corpus() {
  if (RUN_ONLY) {
    assert(!CORPUS.empty());
    assert(fs::is_regular_file(CORPUS) || fs::is_directory(CORPUS));
    return;
  }

  if (CORPUS.empty()) {  // Make a new CORPUS name if not given.
    int CORPUS_id = 0;
    std::string corpus_dir_name =
        std::string("pathfinder_corpus") + std::to_string(CORPUS_id);
    do {
      corpus_dir_name =
          std::string("pathfinder_corpus") + std::to_string(CORPUS_id++);
    } while (fs::is_directory(fs::current_path() / corpus_dir_name));
    std::cout << "Corpus name is not given. Use a new corpus name `"
              << corpus_dir_name << "`.\n"
              << singleline();
    CORPUS = fs::current_path() / corpus_dir_name;
  }

  if (!fs::is_directory(CORPUS))  // Make an empty CORPUS if not exists
    PATHFINDER_CHECK(fs::create_directories(CORPUS),
                     "PathFinder Error: Failed to make output corpus `" +
                         CORPUS.string() + "`");
}

size_t elapsed_from_s(std::chrono::steady_clock::time_point from) {
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::seconds>(now - from).count();
}

size_t elapsed_from_ms(std::chrono::steady_clock::time_point from) {
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - from)
      .count();
}

size_t elapsed_from_us(std::chrono::steady_clock::time_point from) {
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(now - from)
      .count();
}

size_t elapsed_from_ns(std::chrono::steady_clock::time_point from) {
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(now - from)
      .count();
}

size_t ns_to_ms(size_t time_in_ns) { return time_in_ns / 1000000; }

float ns_to_s(size_t time_in_ns) { return (float)time_in_ns / 1000000000.0f; }

bool almost_zero(float seconds) {
  return std::stof(std::to_string(seconds)) == 0;
}

std::string rm_non_numeric(std::string str) {
  std::string ret;
  for (auto ch : str)
    if ('0' <= ch && ch <= '9') ret.push_back(ch);
  return ret;
}

bool is_number(std::string str) {
  for (auto& c : str)
    if (c < '0' || c > '9') return false;
  return true;
}

bool is_prefix_of(std::string pre, std::string base) {
  return base.compare(0, pre.size(), pre) == 0;
}

std::pair<std::string, std::string> split(std::string str, char sep) {
  size_t pos;
  for (pos = 0; pos < str.size(); pos++)
    if (str[pos] == sep) break;

  std::string first = str.substr(0, pos);
  std::string second = pos == str.size() ? "" : str.substr(pos + 1, str.size());
  return std::make_pair(first, second);
}

bool is_comp(char c) { return c == '=' || c == '!' || c == '>' || c == '<'; }

std::tuple<std::string, std::string, std::string> split_comp(
    std::string constraint) {
  size_t pos_comp;
  for (pos_comp = 0; pos_comp < constraint.size(); pos_comp++)
    if (is_comp(constraint[pos_comp])) break;

  if (is_prefix_of("==", constraint.substr(pos_comp, 2)) ||
      is_prefix_of("!=", constraint.substr(pos_comp, 2)) ||
      is_prefix_of(">=", constraint.substr(pos_comp, 2)) ||
      is_prefix_of("<=", constraint.substr(pos_comp, 2))) {
    PATHFINDER_CHECK(
        constraint.size() > pos_comp + 2,
        "PathFinder Error: Invalid constraint `" + constraint + "`");
    std::string lhs = constraint.substr(0, pos_comp);
    std::string comp = constraint.substr(pos_comp, 2);
    std::string rhs =
        constraint.substr(pos_comp + 2, constraint.size() - (pos_comp + 2));
    return std::make_tuple(strip(lhs), comp, strip(rhs));
  } else if (constraint[pos_comp] == '>' || constraint[pos_comp] == '<') {
    PATHFINDER_CHECK(
        constraint.size() > pos_comp + 1,
        "PathFinder Error: Invalid constraint `" + constraint + "`");
    std::string lhs = constraint.substr(0, pos_comp);
    std::string comp = constraint.substr(pos_comp, 1);
    std::string rhs =
        constraint.substr(pos_comp + 1, constraint.size() - (pos_comp + 1));
    return std::make_tuple(strip(lhs), comp, strip(rhs));
  } else {
    PATHFINDER_CHECK(
        false, "PathFinder Error: Invalid constraint `" + constraint + "`");
  }
}

std::vector<std::string> split_all(std::string str, char sep) {
  std::vector<std::string> chopped;
  std::string hd, tl;
  std::tie(hd, tl) = split(str, sep);
  chopped.push_back(hd);
  while (tl != "") {
    std::tie(hd, tl) = split(tl, sep);
    chopped.push_back(hd);
  }
  return chopped;
}

bool is_space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

std::string strip(std::string str) {
  if (str.size() == 0) return str;

  size_t pos_right = str.size() - 1;
  for (; pos_right > 0; pos_right--)
    if (!is_space(str[pos_right])) break;

  size_t pos_left = 0;
  for (; pos_left <= pos_right; pos_left++)
    if (!is_space(str[pos_left])) break;

  return str.substr(pos_left, pos_right - pos_left + 1);
}

std::string rm_leading_zeros(std::string numeric_str) {
  // Input is assumed to be composed of all numeric characters.
  std::string ret;
  size_t non_zero_pos = 0;
  for (size_t non_zero_pos = 0; non_zero_pos < numeric_str.size();
       non_zero_pos++)
    if ('0' < numeric_str[non_zero_pos]) break;

  return numeric_str.substr(non_zero_pos);
}

std::string epath_to_string(const std::vector<uint32_t>& epath) {
  std::string str;
  for (size_t i = 0; i < epath.size(); i++) {
    std::stringstream stream;
    stream << std::hex << epath[i];
    str += std::string(stream.str());
    if (i != epath.size() - 1) str += "->";
  }
  return str;
}

std::ostream& operator<<(std::ostream& os,
                         const std::vector<uintptr_t>& epath) {
  for (size_t i = 0; i < epath.size(); i++) {
    os << std::hex << epath[i];
    if (i != epath.size() - 1) os << "->";
  }
  return os;
}

std::vector<long> cmd_input_to_vec() {
  assert(CMD_LINE_INPUT.size() > 0);

  std::vector<std::string> space_separated = split_all(CMD_LINE_INPUT, ' ');
  std::vector<std::string> comma_separated = split_all(CMD_LINE_INPUT, ',');

  std::vector<std::string> num_strings;
  if (space_separated.size() >= comma_separated.size())
    num_strings = space_separated;
  else
    num_strings = comma_separated;

  std::vector<long> numbers;
  for (auto s : num_strings) numbers.push_back(stol(s));

  return numbers;
}

std::vector<uint8_t> file_to_vector(const fs::path& filepath) {
  // from
  // https://www.coniferproductions.com/posts/2022/10/25/reading-binary-files-cpp/

  std::ifstream inputFile(filepath, std::ios_base::binary);

  inputFile.seekg(0, std::ios_base::end);
  auto length = inputFile.tellg();
  inputFile.seekg(0, std::ios_base::beg);

  std::vector<uint8_t> buffer(length);
  inputFile.read(reinterpret_cast<char*>(buffer.data()), length);

  inputFile.close();
  return buffer;
}

std::vector<long> uint8_vec_to_long_vec(std::vector<uint8_t> uint8_vec) {
  size_t unit = sizeof(long) / sizeof(uint8_t);
  size_t size = uint8_vec.size() / unit;
  std::vector<long> long_vec(size);
  memcpy(long_vec.data(), uint8_vec.data(), sizeof(long) * size);
  assert(long_vec.size() == size);
  return long_vec;
}

std::vector<fs::path> list_files_in_dir(const fs::path& dirpath) {
  std::vector<fs::path> file_paths;
  for (const auto& entry : fs::directory_iterator(dirpath)) {
    auto path = entry.path();
    if (fs::is_regular_file(path)) file_paths.push_back(path);
  }
  return file_paths;
}

void check_duet() {
  fs::path duet_bin_path(DUET_BIN_PATH);
  if (duet_bin_path.empty() && RUN_ONLY) return;

  PATHFINDER_CHECK(fs::is_regular_file(duet_bin_path),
                   "PathFinder Error: Failed to find duet binary `" +
                       duet_bin_path.string() + "`.");

  auto duet_test_run = [](const std::string& cmd) -> bool {
    const std::string DUET_EMPTY_RUN_OUTPUT_PREFIX = "Usage: ";
    std::string duet_test_run_output = exec(cmd);
    return is_prefix_of(DUET_EMPTY_RUN_OUTPUT_PREFIX, duet_test_run_output);
  };

  std::string cmd = duet_bin_path.string() + " 2>&1";
  auto th = std::async(duet_test_run, cmd);
  PATHFINDER_CHECK(th.get(), "PathFinder Error: Duet binary `" +
                                 duet_bin_path.string() + "` run failed.");
}

std::string duet_cmd(std::string sygus_file_name, float timeout) {
#ifdef __linux__
  std::string TIMEOUT_CMD = "timeout";
  std::string ENVVAR = "";
#elif __APPLE__
  std::string TIMEOUT_CMD = "gtimeout";
  std::string ENVVAR = "DYLD_LIBRARY_PATH=" + std::string(getenv("HOME")) +
                       "/.opam/4.08.0/lib/z3 ";
#endif
  return ENVVAR + " " + TIMEOUT_CMD + " " + std::to_string(timeout) + " " +
         DUET_BIN_PATH + " " + DUET_OPT + " " + sygus_file_name +
         " 2>&1";  // duet prints to stderr
}

}  // namespace pathfinder
