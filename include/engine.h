#ifndef PATHFINDER_ENGINE
#define PATHFINDER_ENGINE

#include "exectree.h"
#include "input_generator.h"
#include "sygus_gen.h"

namespace pathfinder {

class Engine {
 public:
  Engine(UserCallback callback_, size_t num_args_,
         std::chrono::steady_clock::time_point started_at_,
         size_t total_time_budget_, size_t max_generation_cnt_, TracePC* tpc_);
  void run_cmd_input();
  int run_corpus();
  void run_corpus_and_output_cov();
  void synthesize_all();
  void warmingup(size_t cnt = 64);
  void run();
  void reset_counter();
  int get_gen_cnt() const;
  size_t get_num_path() const;
  std::string stats() const;
  void output_stat(const std::string& filename, size_t time) const;
  std::string to_string() const;

 private:
  void exit_if_time_up();
  InputGenerator* ig();
  LeafNode* schedule();
  int run_one_input();
  std::unique_ptr<FunSynthesized> trivial_enum();
  std::unique_ptr<FunSynthesized> trivial_numeric();
  std::unique_ptr<SygusFile> make_sygus();
  std::pair<int, ExecPath> run_callback(const Input& input,
                                        bool measure_covered_pc_before_running,
                                        bool is_initial_seed_);
  void check_run_result(int run_status);
  void set_generator(std::vector<EnumCondition*> enum_conditions,
                     std::vector<NumericCondition*> numeric_conditions);
  std::optional<Input> run_generator();
  void update_enum_bvs(Node* target);
  void refine(const std::set<Node*>& refinement_target);

  const std::string& potential_crash_prefix();
  fs::path output_file_path(const std::string& file_name);
  void write_to_output_corpus(Input arg);
  void commit_last_seed();
  void delete_last_seed();

  UserCallback callback;
  size_t num_args;
  std::chrono::steady_clock::time_point started_at;
  size_t total_time_budget;
  size_t max_generation_cnt;
  TracePC* tpc;
  size_t covered_pc = 0;
  int solver_arggen_max_iter;
  size_t solver_timeout;

  std::unique_ptr<ExecTree> exectree;
  std::unique_ptr<InputGenerator> input_generator;

  // timers(in ms)
  size_t time_warming_up = 0;

  size_t time_conflict_check() const;
  size_t time_conflict_check_internal = 0;
  size_t time_conflict_check_reconstruction = 0;
  size_t time_conflict_check_dump = 0;
  size_t time_conflict_check_synthesis = 0;

  size_t time_scheduling = 0;

  size_t time_generation_setting = 0;
  size_t time_generation_gen = 0;

  size_t time_running_callback = 0;
  size_t time_result_check = 0;

  size_t time_handling_duplicate() const;
  size_t num_conflict = 0;
  size_t time_handling_duplicate_run_callback = 0;
  size_t time_handling_duplicate_checkdiff = 0;
  size_t time_handling_duplicate_reconstruction = 0;
  size_t time_handling_duplicate_synthesis = 0;
  size_t time_handling_duplicate_dump = 0;

  size_t time_path_check() const;
  size_t time_path_check_duplicate = 0;
  size_t time_path_check_insert = 0;

  size_t time_condition_evaluation = 0;

  size_t time_synthesis = 0;

  size_t time_dump = 0;

  size_t iter = 0;

  int gen_remained = 0;
  int total_gen_cnt = 0;

  size_t output_stat_interval = 300;
  size_t next_time_to_output_stat;

  size_t num_pass = 0;
  size_t num_fail = 0;

  // flag to check current phase.
  // used for printing properly.
  enum Phase { RUNNIG_CORPUS, INITIALIZING_PATHTREE, FUZZ_RUNNING };
  Phase phase;

  std::string last_written_seed;
};

}  // namespace pathfinder

#endif
