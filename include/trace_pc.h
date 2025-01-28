#ifndef PATHFINDER_TRACE_PC
#define PATHFINDER_TRACE_PC

#include <bitset>
#include <cassert>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

#include "pathfinder_defs.h"

namespace pathfinder {

class TracePC {
 public:
  class BitMap {
    static const size_t BITSIZE = 64;

   public:
    BitMap() {
      bitmap = nullptr;
      size = 0;
    }
    ~BitMap() {
      if (bitmap != nullptr) delete[] bitmap;
    }
    bool is_available() { return bitmap != nullptr; }
    void init(size_t size_) {
      size = size_;
      size_t bytesize = size % 8 == 0 ? size / 8 : size / 8 + 1;
      size_t alligned_size =
          bytesize % 8 == 0 ? bytesize : (bytesize / 8 + 1) * 8;
      bitmap = new uint8_t[alligned_size];
      memset(bitmap, 0, alligned_size);
    }
    inline void set(size_t idx) {
      size_t byteidx = idx / 8;
      size_t bitidx = idx % 8;
      bitmap[byteidx] |= bitmask(bitidx);
    }
    inline bool is_set(size_t idx) {
      size_t byteidx = idx / 8;
      size_t bitidx = idx % 8;
      return bitmap[byteidx] & bitmask(bitidx);
    }
    size_t get_size() const { return size; }
    size_t num_set_bit() const {
      size_t bytesize = size % 8 == 0 ? size / 8 : size / 8 + 1;
      size_t llsize = bytesize % 8 == 0 ? bytesize / 8 : bytesize / 8 + 1;
      size_t total = 0;
      unsigned long long* bitmap_ll_aligned = (unsigned long long*)bitmap;
      for (size_t i = 0; i < llsize; i++)
        total += __builtin_popcountll(bitmap_ll_aligned[i]);

      return total;
    }

   private:
    inline uint8_t bitmask(size_t bitidx) {
      uint8_t mask;
      switch (bitidx) {
        case 0:
          mask = 1;
          break;
        case 1:
          mask = 2;
          break;
        case 2:
          mask = 4;
          break;
        case 3:
          mask = 8;
          break;
        case 4:
          mask = 16;
          break;
        case 5:
          mask = 32;
          break;
        case 6:
          mask = 64;
          break;
        case 7:
          mask = 128;
          break;
      }
      return mask;
    }

    uint8_t* bitmap;
    size_t size;
  };

 public:
  TracePC(size_t max_significant_execpath_size_ = 1000000);
  ~TracePC();
  void HandleInit(uint32_t* Start, uint32_t* Stop);
  size_t ExecPathSignificantMax() const;
  ExecPath significant(ExecPath epath) const;
  ExecPath tail_of(ExecPath epath) const;
  bool eq_significant(const ExecPath& left, const ExecPath& right) const;
  bool truncated(const ExecPath& epath) const;
  bool considerably_longer(const ExecPath& left, const ExecPath& right) const;
  void TraceOn();
  void TraceOff();
  void ClearPathLog();
  void AppendPathLog(PCID pcid);
  void CheckDiff(ExecPath left, ExecPath right);
  ExecPath Prune(ExecPath epath);
  ExecPath GetPathLog();
  size_t GetNumInstrumented();
  void InitCoveredBitMap();
  size_t GetNumCovered();
  size_t GetNumND();
  void AddND(const ExecPath& epath);

 private:
  size_t ExecPathTailMax() const;
  size_t ExecPathMax() const;
  size_t CheckDiffChunkSize() const;
  void InitNDBitMap();
  bool IsND(PCID pcid);
  void CheckDiff(ExecPath& left, size_t left_start, size_t left_size,
                 ExecPath& right, size_t right_start, size_t right_size,
                 std::vector<bool>& shadow_left,
                 std::vector<bool>& shadow_right);
  void AddND(ExecPath& epath, std::vector<bool>& shadow_epath, bool do_all);
  ExecPath Prune(PCID* epath, size_t size);

  // parameterized for being used in test
  size_t max_significant_execpath_size;
  size_t max_tail_execpath_size;
  size_t check_diff_chunk_size;

  size_t NumGuards;
  BitMap covered_pc_bitmap;
  BitMap nondeterministic_pc_bitmap;

  PCID* PathLog;
  size_t PathLogSize;
  bool trace;
};

TracePC& TPC();

}  // namespace pathfinder

#endif
