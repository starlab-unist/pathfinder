#include "trace_pc.h"

#include <sstream>

#include "utils.h"

namespace pathfinder {

TracePC& TPC() {
  static TracePC* TPC_ = new TracePC();
  return *TPC_;
}

TracePC::TracePC(size_t max_significant_execpath_size_) {
  assert(max_significant_execpath_size_ >= 10);
  max_significant_execpath_size = max_significant_execpath_size_;
  max_tail_execpath_size = max_significant_execpath_size;
  check_diff_chunk_size = max_significant_execpath_size / 10;

  NumGuards = 0;
  PathLog = new PCID[max_significant_execpath_size + max_tail_execpath_size];
  PathLogSize = 0;
  trace = false;
}
TracePC::~TracePC() {
  if (PathLog != nullptr) delete[] PathLog;
}
void TracePC::HandleInit(uint32_t* Start, uint32_t* Stop) {
  if (Start == Stop || *Start) return;
  for (uint32_t* P = Start; P < Stop; P++) {
    NumGuards++;
    *P = NumGuards;
  }
}
size_t TracePC::ExecPathSignificantMax() const {
  return max_significant_execpath_size;
}

size_t TracePC::ExecPathTailMax() const { return max_tail_execpath_size; }

size_t TracePC::ExecPathMax() const {
  return ExecPathSignificantMax() + ExecPathTailMax();
}

size_t TracePC::CheckDiffChunkSize() const { return check_diff_chunk_size; }

ExecPath TracePC::significant(ExecPath epath) const {
  if (epath.size() <= ExecPathSignificantMax()) return epath;
  return subvec(epath, 0, ExecPathSignificantMax());
}

ExecPath TracePC::tail_of(ExecPath epath) const {
  if (epath.size() <= ExecPathSignificantMax()) return {};
  size_t len = epath.size() - ExecPathSignificantMax() <= ExecPathTailMax()
                   ? epath.size() - ExecPathSignificantMax()
                   : ExecPathTailMax();
  return subvec(epath, ExecPathSignificantMax(), len);
}

bool TracePC::eq_significant(const ExecPath& left,
                             const ExecPath& right) const {
  if (left.size() >= ExecPathSignificantMax() &&
      right.size() >= ExecPathSignificantMax()) {
    for (size_t i = 0; i < ExecPathSignificantMax(); i++)
      if (left[i] != right[i]) return false;
    return true;
  }

  if (left.size() != right.size()) return false;

  for (size_t i = 0; i < left.size(); i++)
    if (left[i] != right[i]) return false;
  return true;
}

bool TracePC::truncated(const ExecPath& epath) const {
  return epath.size() == ExecPathMax();
}

bool TracePC::considerably_longer(const ExecPath& left,
                                  const ExecPath& right) const {
  // Currently, this criteria is not a known solution
  // nor thoroughly demonstrated, but just a trial.

  if (left.size() <= right.size()) return false;

  if (left.size() <= CheckDiffChunkSize()) return false;

  if (left.size() > 2 * right.size() ||
      left.size() - right.size() >= 2 * CheckDiffChunkSize()) {
    return true;
  } else {
    return false;
  }
}

void TracePC::TraceOn() { trace = true; }
void TracePC::TraceOff() { trace = false; }

void TracePC::ClearPathLog() { PathLogSize = 0; }

void TracePC::InitCoveredBitMap() {
  if (!covered_pc_bitmap.is_available()) covered_pc_bitmap.init(NumGuards);
}

void TracePC::InitNDBitMap() {
  if (!nondeterministic_pc_bitmap.is_available())
    nondeterministic_pc_bitmap.init(NumGuards);
}

inline bool TracePC::IsND(PCID pcid) {
  InitNDBitMap();
  return nondeterministic_pc_bitmap.is_set(pcid - 1);
}

void TracePC::AppendPathLog(PCID pcid) {
  if (!trace) return;

  if (covered_pc_bitmap.is_available()) covered_pc_bitmap.set(pcid - 1);

  if (PathLogSize < max_significant_execpath_size + max_tail_execpath_size &&
      (!nondeterministic_pc_bitmap.is_available() || !IsND(pcid)))
    PathLog[PathLogSize++] = pcid;
}

ExecPath TracePC::Prune(PCID* epath, size_t size) {
  static PCID* pruned =
      new PCID[max_significant_execpath_size + max_tail_execpath_size];

  size_t pruned_size = 0;
  for (size_t i = 0; i < size; i++)
    if (!IsND(epath[i])) pruned[pruned_size++] = epath[i];

  return ExecPath(pruned, pruned + pruned_size);
}

ExecPath TracePC::Prune(ExecPath epath) {
  return Prune(epath.data(), epath.size());
}

ExecPath TracePC::GetPathLog() {
  // workaround to mitigate buffer overflow
  size_t size = std::min(
      PathLogSize, max_significant_execpath_size + max_tail_execpath_size);
  return ExecPath(PathLog, PathLog + size);
}

size_t TracePC::GetNumInstrumented() { return NumGuards; }

size_t TracePC::GetNumCovered() {
  assert(covered_pc_bitmap.is_available());
  return covered_pc_bitmap.num_set_bit();
}

size_t TracePC::GetNumND() {
  if (!nondeterministic_pc_bitmap.is_available()) return 0;
  return nondeterministic_pc_bitmap.num_set_bit();
}

class NegIndexable {
 public:
  NegIndexable(size_t size) : vec(ExecPath(size)) {}
  NegIndexable(ExecPath vec_) : vec(std::move(vec_)) {}
  PCID& operator[](int idx) { return vec[normal_idx(idx)]; }

 private:
  ExecPath vec;
  size_t normal_idx(int idx) { return idx = idx >= 0 ? idx : vec.size() + idx; }
};

std::tuple<size_t, size_t, size_t, size_t, size_t>
find_middle_snake_myers_original(ExecPath& left, size_t left_start,
                                 size_t left_size, ExecPath& right,
                                 size_t right_start, size_t right_size) {
  size_t max = left_size + right_size;
  assert(max > 0);
  int delta = left_size - right_size;

  NegIndexable vf(max * 2);
  NegIndexable vb(max * 2);

  vf[1] = 0;
  vb[1] = 0;

  int d_max = max % 2 == 0 ? max / 2 : max / 2 + 1;
  for (int d = 0; d <= d_max; d++) {
    for (int k = -d; k <= d; k += 2) {
      int x, y;
      if (k == -d || (k != d && vf[k - 1] < vf[k + 1])) {
        x = vf[k + 1];
      } else {
        x = vf[k - 1] + 1;
      }
      y = x - k;
      int x_i = x;
      int y_i = y;
      while (x < (int)left_size && y < (int)right_size &&
             left[left_start + x] == right[right_start + y]) {
        x++;
        y++;
      }
      vf[k] = x;
      if (delta % 2 != 0 && (-(k - delta)) >= -(d - 1) &&
          (-(k - delta)) <= (d - 1))
        if (vf[k] + vb[-(k - delta)] >= left_size)
          return std::make_tuple(2 * d - 1, x_i, y_i, x, y);
    }
    for (int k = -d; k <= d; k += 2) {
      int x, y;
      if (k == -d || (k != d && vb[k - 1] < vb[k + 1])) {
        x = vb[k + 1];
      } else {
        x = vb[k - 1] + 1;
      }
      y = x - k;
      int x_i = x;
      int y_i = y;
      while (x < (int)left_size && y < (int)right_size &&
             left[left_start + (left_size - x - 1)] ==
                 right[right_start + (right_size - y - 1)]) {
        x++;
        y++;
      }
      vb[k] = x;
      if (delta % 2 == 0 && (-(k - delta)) >= -d && (-(k - delta)) <= d)
        if (vb[k] + vf[-(k - delta)] >= left_size)
          return std::make_tuple(2 * d, left_size - x, right_size - y,
                                 left_size - x_i, right_size - y_i);
    }
  }
  throw Unreachable();
}

void TracePC::CheckDiff(ExecPath& left, size_t left_start, size_t left_size,
                        ExecPath& right, size_t right_start, size_t right_size,
                        std::vector<bool>& shadow_left,
                        std::vector<bool>& shadow_right) {
  if (left_size == 0) {
    for (size_t i = 0; i < right_size; i++)
      shadow_right[right_start + i] = true;
    return;
  }

  if (right_size == 0) {
    for (size_t i = 0; i < left_size; i++) shadow_left[left_start + i] = true;
    return;
  }

  size_t d, x, y, u, v;
  std::tie(d, x, y, u, v) = find_middle_snake_myers_original(
      left, left_start, left_size, right, right_start, right_size);
  if (d > 1) {
    size_t left_start_fist_half = left_start;
    size_t left_size_first_half = x;
    size_t right_start_first_half = right_start;
    size_t right_size_first_half = y;
    size_t left_start_second_half = left_start + u;
    size_t left_size_second_half = left_size - u;
    size_t right_start_second_half = right_start + v;
    size_t right_size_second_half = right_size - v;

    CheckDiff(left, left_start_fist_half, left_size_first_half, right,
              right_start_first_half, right_size_first_half, shadow_left,
              shadow_right);
    CheckDiff(left, left_start_second_half, left_size_second_half, right,
              right_start_second_half, right_size_second_half, shadow_left,
              shadow_right);
  } else if (d == 1) {
    if (left_size < right_size) {
      for (size_t i = 0; i < left_size; i++) {
        if (left[left_start + i] != right[right_start + i]) {
          shadow_right[right_start + i] = true;
          break;
        }
      }
      shadow_right[right_start + right_size - 1] = true;
    } else {
      for (size_t i = 0; i < right_size; i++) {
        if (right[right_start + i] != left[left_start + i]) {
          shadow_left[left_start + i] = true;
          break;
        }
      }
      shadow_left[left_start + left_size - 1] = true;
    }
  }
}

size_t remove_common_prefix(ExecPath& left, ExecPath& right) {
  size_t common_length = common_prefix_length(left, right);
  left = subvec(left, common_length);
  right = subvec(right, common_length);
  return common_length;
}

inline void TracePC::AddND(ExecPath& epath, std::vector<bool>& shadow_epath,
                           bool do_all) {
  assert(epath.size() == shadow_epath.size());

  if (do_all) {
    for (size_t i = 0; i < epath.size(); i++)
      if (shadow_epath[i] == true) nondeterministic_pc_bitmap.set(epath[i] - 1);
    return;
  }

  size_t common_length = 0;
  for (size_t i = 0; i < shadow_epath.size(); i++)
    if (shadow_epath[i] == false) common_length++;
  size_t common_half =
      common_length % 2 == 0 ? common_length / 2 : (common_length / 2) + 1;

  size_t common_seen = 0;
  for (size_t i = 0; i < epath.size(); i++) {
    if (common_seen > common_half) break;

    if (shadow_epath[i] == true) {
      nondeterministic_pc_bitmap.set(epath[i] - 1);
    } else {
      common_seen++;
    }
  }
}

void TracePC::AddND(const ExecPath& epath) {
  // Used for testing only

  InitNDBitMap();

  for (size_t i = 0; i < epath.size(); i++)
    nondeterministic_pc_bitmap.set(epath[i] - 1);
}

// referred to
// https://github.com/RobertElderSoftware/roberteldersoftwarediff/blob/master/myers_diff_and_variations.py
void TracePC::CheckDiff(ExecPath left, ExecPath right) {
  if (!nondeterministic_pc_bitmap.is_available()) InitNDBitMap();

  ExecPath left_pruned = left;
  ExecPath right_pruned = right;
  size_t common_length = remove_common_prefix(left_pruned, right_pruned);
  assert(common_length < ExecPathSignificantMax());

  bool is_last_iter = left_pruned.size() <= CheckDiffChunkSize() ||
                      right_pruned.size() <= CheckDiffChunkSize();

  size_t left_chunk_size, right_chunk_size;
  ExecPath left_chunk, right_chunk;
  if (is_last_iter) {
    left_chunk_size = left_pruned.size();
    left_chunk = left_pruned;
    right_chunk_size = right_pruned.size();
    right_chunk = right_pruned;
  } else {
    left_chunk_size = CheckDiffChunkSize();
    left_chunk = subvec(left_pruned, 0, left_chunk_size);
    right_chunk_size = CheckDiffChunkSize();
    right_chunk = subvec(right_pruned, 0, right_chunk_size);
  }

  std::vector<bool> shadow_left(left_chunk_size, false);
  std::vector<bool> shadow_right(right_chunk_size, false);

  while (true) {
    CheckDiff(left_chunk, 0, left_chunk_size, right_chunk, 0, right_chunk_size,
              shadow_left, shadow_right);
    AddND(left_chunk, shadow_left, is_last_iter);
    AddND(right_chunk, shadow_right, is_last_iter);

    left_pruned = Prune(left);
    right_pruned = Prune(right);
    common_length = remove_common_prefix(left_pruned, right_pruned);
    if (common_length >= ExecPathSignificantMax() ||
        (left_pruned.size() == 0 && right_pruned.size() == 0))
      break;

    is_last_iter = left_pruned.size() <= CheckDiffChunkSize() ||
                   right_pruned.size() <= CheckDiffChunkSize();

    if (is_last_iter) {
      left_chunk_size = left_pruned.size();
      left_chunk = left_pruned;
      right_chunk_size = right_pruned.size();
      right_chunk = right_pruned;
    } else {
      left_chunk_size = CheckDiffChunkSize();
      left_chunk = subvec(left_pruned, 0, left_chunk_size);
      right_chunk_size = CheckDiffChunkSize();
      right_chunk = subvec(right_pruned, 0, right_chunk_size);
    }

    shadow_left = std::vector<bool>(left_chunk_size, false);
    shadow_right = std::vector<bool>(right_chunk_size, false);
  }
}

}  // namespace pathfinder

#define ATTRIBUTE_INTERFACE __attribute__((visibility("default")))

#if __has_attribute(no_sanitize)
#define ATTRIBUTE_NO_SANITIZE_MEMORY __attribute__((no_sanitize("memory")))
#else
#define ATTRIBUTE_NO_SANITIZE_MEMORY
#endif

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define ATTRIBUTE_NO_SANITIZE_ALL __attribute__((no_sanitize_address))
#elif __has_feature(memory_sanitizer)
#define ATTRIBUTE_NO_SANITIZE_ALL ATTRIBUTE_NO_SANITIZE_MEMORY
#else
#define ATTRIBUTE_NO_SANITIZE_ALL
#endif
#else
#define ATTRIBUTE_NO_SANITIZE_ALL
#endif

extern "C" {

ATTRIBUTE_INTERFACE
ATTRIBUTE_NO_SANITIZE_ALL
void __sanitizer_cov_trace_pc_guard(uint32_t* Guard) {
  pathfinder::PCID pcid = *Guard;
  pathfinder::TPC().AppendPathLog(pcid);
}

ATTRIBUTE_INTERFACE
void __sanitizer_cov_trace_pc_guard_init(uint32_t* Start, uint32_t* Stop) {
  pathfinder::TPC().HandleInit(Start, Stop);
}

}  // extern "C"
