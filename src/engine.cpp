#include "engine.h"

#include <signal.h>
#include <unistd.h>

#include <fstream>
#include <iomanip>
#include <sstream>

bool is_initial_seed;

namespace pathfinder {

Engine::Engine(UserCallback callback_, size_t num_args_,
               std::chrono::steady_clock::time_point started_at_,
               size_t total_time_budget_, size_t max_generation_cnt_,
               TracePC* tpc_)
    : callback(callback_),
      num_args(num_args_),
      started_at(started_at_),
      total_time_budget(total_time_budget_),
      max_generation_cnt(max_generation_cnt_),
      tpc(tpc_) {
  assert(num_args > 0);

  exectree = std::make_unique<ExecTree>(tpc);
  input_generator = std::make_unique<InputGenerator>();

  next_time_to_output_stat = output_stat_interval;
}
void Engine::exit_if_time_up() {
  size_t elapsed = elapsed_from_s(started_at);

  if (STAT_OUTPUT_FILENAME != "") {
    if (elapsed >= next_time_to_output_stat) {
      output_stat(STAT_OUTPUT_FILENAME, next_time_to_output_stat);
      next_time_to_output_stat += output_stat_interval;
    }
  }

  bool time_up =
      elapsed > total_time_budget || total_gen_cnt > max_generation_cnt;
  if (!time_up) return;

  std::cout << "\n" << doubleline();
  if (V_LEVEL == VERBOSE_LOW) {
    std::cout << "Done. Generated " << std::to_string(get_gen_cnt())
              << " inputs in " << std::to_string(elapsed) << " seconds.\n\n"
              << stats();
  } else {
    std::cout << to_string();
  }

  exit(0);
}
InputGenerator* Engine::ig() { return input_generator.get(); }
LeafNode* Engine::schedule() {
  assert(!exectree->is_empty());

  assert(SCHEDULING_STRATEGY == SCHEDULE_RAND);
  return random_choice(exectree->get_leaves());
}
std::unique_ptr<FunSynthesized> Engine::trivial_enum() {
  return std::make_unique<FunSynthesized>(
      "trivial", get_enum_param_names(),
      std::make_unique<BoolExpr>(BoolExpr::true_expr()));
}
std::unique_ptr<FunSynthesized> Engine::trivial_numeric() {
  return std::make_unique<FunSynthesized>(
      "trivial", get_numeric_param_names(),
      std::make_unique<BoolExpr>(BoolExpr::true_expr()));
}
std::pair<int, ExecPath> Engine::run_callback(
    const Input& input, bool measure_covered_pc_before_running,
    bool is_initial_seed_) {
  int run_status;
  ExecPath epath;

  is_initial_seed = is_initial_seed_;

  if (tpc != nullptr) {
    tpc->TraceOff();
    tpc->ClearPathLog();
    tpc->InitCoveredBitMap();
    if (measure_covered_pc_before_running) covered_pc = tpc->GetNumCovered();
  }

  run_status = callback(input);
  if (tpc != nullptr) tpc->TraceOff();
  if (run_status == PATHFINDER_UNEXPECTED_EXCEPTION) {
    if (IGNORE_EXCEPTION) {
      run_status = PATHFINDER_EXPECTED_EXCEPTION;
    } else {
      std::cerr << "PATHFINDER ABORT: Terminated due to unexpected exception"
                << std::endl;
      abort();
    }
  }
  if (tpc != nullptr) epath = tpc->GetPathLog();

  return std::make_pair(run_status, epath);
}
void Engine::check_run_result(int run_status) {
  assert(tpc != nullptr);

  if (run_status == PATHFINDER_PASS) {
    delete_last_seed();
    return;
  } else if (run_status == 0) {
    num_pass++;
  } else {
    num_fail++;
  }

  size_t covered_pc_new = tpc->GetNumCovered();
  if (covered_pc_new > covered_pc) {
    covered_pc = covered_pc_new;
    commit_last_seed();
  } else {
    delete_last_seed();
  }
}
void Engine::set_generator(std::vector<EnumCondition*> enum_conditions,
                           std::vector<NumericCondition*> numeric_conditions) {
  ig()->set_condition(enum_conditions, numeric_conditions);
}
std::optional<Input> Engine::run_generator() {
  std::optional<Input> input = ig()->gen();
  if (input.has_value()) write_to_output_corpus(input.value());

  return input;
}
void Engine::refine(const std::set<Node*>& refinement_target) {
  for (auto& target : refinement_target) {
    assert(!target->is_root());

    std::set<Input> pos_examples, neg_examples;
    std::tie(pos_examples, neg_examples) = target->get_examples();
    auto sibling = target->get_sibling();
    bool is_pair = sibling.has_value();

    while (true) {
      exit_if_time_up();

      SynthesisResult synthesis_result =
          target->cond->synthesize(is_pair, pos_examples, neg_examples);
      SynthesisStatus synthesis_status = std::get<0>(synthesis_result);
      std::unique_ptr<BranchCondition> cond_new =
          std::move(std::get<1>(synthesis_result));
      std::unique_ptr<BranchCondition> cond_new_sibling =
          std::move(std::get<2>(synthesis_result));
      int64_t synthesis_time = std::get<3>(synthesis_result);

      if (synthesis_status == SUCCESS || synthesis_status == FAIL) {
        if (synthesis_status == SUCCESS) {
          assert(cond_new != nullptr);
          target->cond = std::move(cond_new);
          if (is_pair) {
            assert(cond_new_sibling != nullptr);
            sibling.value()->cond = std::move(cond_new_sibling);
          }
        }

        if (is_pair) {
          target->cond->deduct_synthesis_budget(synthesis_time / 2);
          sibling.value()->cond->deduct_synthesis_budget(synthesis_time / 2);
        } else {
          target->cond->deduct_synthesis_budget(synthesis_time);
        }

        break;
      }

      if (synthesis_status == GIVEUP) target->promote_cond();
    }
  }
}
void Engine::run_cmd_input() {
  if (auto cmd_input = deserialize(cmd_input_to_vec())) {
    log_msg(VERBOSE_LOW,
            "Running command-line input \"" + CMD_LINE_INPUT + "\"...\n");
    run_callback(cmd_input.value(), false, false);
  }
}
int Engine::run_corpus() {
  // run inputs in corpus and return the number of runned input.

  phase = RUNNIG_CORPUS;

  if (fs::is_regular_file(CORPUS)) return run_one_input();

  std::vector<fs::path> seeds;
  if (fs::is_directory(CORPUS)) {
    std::vector<fs::path> all_seeds = list_files_in_dir(CORPUS);

    std::vector<fs::path> filtered_seeds;
    for (auto path : all_seeds) {
      std::string first, second;
      std::tie(first, second) = split(path.stem().string(), '_');
      if (is_prefix_of("time", first) && is_prefix_of("gen", second)) {
        int time = stoi(rm_leading_zeros(rm_non_numeric(first)));
        int gen = stoi(rm_leading_zeros(rm_non_numeric(second)));
        if (RUN_CORPUS_FROM_TIME <= time && time < RUN_CORPUS_TO_TIME &&
            RUN_CORPUS_FROM_GEN <= gen && gen < RUN_CORPUS_TO_GEN)
          filtered_seeds.push_back(path);
      } else {  // is initial seed
        if (RUN_CORPUS_FROM_TIME < 0 && 0 <= RUN_CORPUS_TO_TIME &&
            RUN_CORPUS_FROM_GEN < 0 && 0 <= RUN_CORPUS_TO_GEN)
          filtered_seeds.push_back(path);
      }
    }

    std::sort(filtered_seeds.begin(), filtered_seeds.end());
    seeds = filtered_seeds;
  } else {
    assert(fs::is_regular_file(CORPUS));
    seeds.push_back(CORPUS);
  }

  log_msg(VERBOSE_LOW,
          "In corpus, " + std::to_string(seeds.size()) + " inputs to run.\n");
  int num_runned_input = 0;
  for (auto seed : seeds) {
    auto raw_input = uint8_vec_to_long_vec(file_to_vector(seed));
    auto input_opt = deserialize(raw_input);
    if (!input_opt.has_value()) continue;

    Input input = input_opt.value();
    if (!eval(hard_constraints, input.get_numeric_args())) {
      log_msg(VERBOSE_MID, indent(1) + "ignore input `" + seed.string() + "` " +
                               vec_to_string(raw_input) +
                               " which violates basic constraints\n");
      continue;
    }
    log_msg(VERBOSE_MID, indent(1) + "running input `" + seed.string() + "` " +
                             vec_to_string(raw_input) + " ...\n");
    int run_status;
    ExecPath epath;
    std::tie(run_status, epath) = run_callback(input, false, !RUN_ONLY);
    assert(RUN_ONLY);
    if (!RUN_ONLY) {
      // TODO: Remove it?
      if (epath.size() > 0 && exectree != nullptr)
        exectree->insert(epath, input, run_status);
    }
    num_runned_input++;
  }
  std::cout << std::endl;
  return num_runned_input;
}
void Engine::run_corpus_and_output_cov() {
  // run inputs in corpus and return the number of runned input.

  phase = RUNNIG_CORPUS;

  PATHFINDER_CHECK(RUN_ONLY, "PathFinder Error: Should be run only mode");
  PATHFINDER_CHECK(
      fs::is_directory(CORPUS),
      "PathFinder Error: Invalid corpus directory `" + CORPUS.string() + "`");
  PATHFINDER_CHECK(!COV_OUTPUT_FILENAME.empty(),
                   "PathFinder Error: `--output_cov` should be specified");
  bool itv_time = MAX_TOTAL_TIME < INT_MAX && COV_INTERVAL_TIME != 0;
  bool itv_gen = MAX_TOTAL_GEN < INT_MAX && COV_INTERVAL_GEN != 0;
  PATHFINDER_CHECK(
      itv_time || itv_gen,
      "PathFinder Error: You must specify"
      "                  both `--max_total_time` && `--cov_interval_time`,"
      "                  or `--max_total_gen` && `--cov_interval_gen`.");

  size_t TOTAL_BUDGET = itv_time ? MAX_TOTAL_TIME : MAX_TOTAL_GEN;
  size_t COV_INTERVAL = itv_time ? COV_INTERVAL_TIME : COV_INTERVAL_GEN;

  std::vector<fs::path> seeds;

  std::vector<fs::path> all_seeds = list_files_in_dir(CORPUS);

  size_t num_interval = TOTAL_BUDGET % COV_INTERVAL == 0
                            ? TOTAL_BUDGET / COV_INTERVAL
                            : TOTAL_BUDGET / COV_INTERVAL + 1;
  std::vector<std::vector<fs::path>> intervals =
      std::vector<std::vector<fs::path>>(num_interval, std::vector<fs::path>());

  for (auto path : all_seeds) {
    std::string first, second;
    std::tie(first, second) = split(path.stem().string(), '_');
    if (is_prefix_of("time", first) && is_prefix_of("gen", second)) {
      int itv = itv_time ? stoi(rm_leading_zeros(rm_non_numeric(first)))
                         : stoi(rm_leading_zeros(rm_non_numeric(second)));
      size_t interval_idx = itv / COV_INTERVAL;
      if (interval_idx < num_interval) intervals[interval_idx].push_back(path);
    }
  }

  tpc->InitCoveredBitMap();
  write_to_file(
      COV_OUTPUT_FILENAME,
      "Total Coverage," + std::to_string(tpc->GetNumInstrumented()) + "\n\n");
  if (itv_time) {
    append_to_file(COV_OUTPUT_FILENAME, "Time,Coverage\n");
  } else {
    append_to_file(COV_OUTPUT_FILENAME, "Gen,Coverage\n");
  }

  size_t t = COV_INTERVAL;
  for (auto& interval : intervals) {
    std::sort(interval.begin(), interval.end());
    for (auto seed : interval) {
      auto raw_input = uint8_vec_to_long_vec(file_to_vector(seed));
      auto input_opt = deserialize(raw_input);
      if (!input_opt.has_value()) continue;

      Input input = input_opt.value();
      if (!eval(hard_constraints, input.get_numeric_args())) {
        log_msg(VERBOSE_MID, indent(1) + "ignore input `" + seed.string() +
                                 "` " + vec_to_string(raw_input) +
                                 " which violates basic constraints\n");
        continue;
      }
      log_msg(VERBOSE_MID, indent(1) + "running input `" + seed.string() +
                               "` " + vec_to_string(raw_input) + " ...\n");
      int run_status;
      ExecPath epath;
      std::tie(run_status, epath) = run_callback(input, false, !RUN_ONLY);
    }
    append_to_file(
        COV_OUTPUT_FILENAME,
        std::to_string(t) + "," + std::to_string(tpc->GetNumCovered()) + "\n");
    t += COV_INTERVAL;
  }
}
int Engine::run_one_input() {
  auto raw_input = uint8_vec_to_long_vec(file_to_vector(CORPUS));
  auto input_opt = deserialize(raw_input);
  PATHFINDER_CHECK(
      input_opt.has_value(),
      "PathFinder Error: failed to parse input `" + CORPUS.string() + "`");

  Input input = input_opt.value();
  log_msg(VERBOSE_MID, indent(1) + "running input `" + CORPUS.string() + "` " +
                           input_to_string(input) + " ...\n");
  int run_status;
  ExecPath epath;
  std::tie(run_status, epath) = run_callback(input, false, !RUN_ONLY);
  assert(RUN_ONLY);
  if (!RUN_ONLY) {
    // TODO: Remove it?
    if (epath.size() > 0 && exectree != nullptr)
      exectree->insert(epath, input, run_status);
  }

  return 1;
}
void Engine::synthesize_all() {
  // TODO: Remove it?
  /* phase = INITIALIZING_PATHTREE;

  std::set<Node*> internals_conforming =
    et_conforming == nullptr ? std::set<Node*>() :
  et_conforming->get_internals(); std::set<Node*> internals_violating =
    et_violating == nullptr ? std::set<Node*>() : et_violating->get_internals();
  std::set<Node*> internals;
  internals.insert(internals_conforming.begin(), internals_conforming.end());
  internals.insert(internals_violating.begin(), internals_violating.end());

  std::vector<Node*> internals_vec(internals.size());
  std::copy(internals.begin(), internals.end(), internals_vec.begin());
  std::cout << "Synthesizing " << std::to_string(internals_vec.size()) << "
  conditions" << std::flush;

  for (size_t i = 0; i < internals_vec.size(); i++) {
    auto ft = synthesize_thread(internals_vec[i]);
    ft.wait();
    std::cout << "." << std::flush;
  }
  std::cout << std::endl; */
}
void Engine::warmingup(size_t cnt) {
  auto warming_up_start = std::chrono::steady_clock::now();
  log_msg(VERBOSE_MID, singleline() + "Warmingup Running\n")

      Input input;
  int run_status;
  ExecPath epath;
  bool epath_truncated;
  for (size_t i = 0; i < cnt; i++) {
    // Run callback with random input CNT times.
    // We expect it can mitigate confusion of execution path from initialization
    // step of target function.
    set_generator({}, {});
    while (true) {
      auto input_opt = run_generator();
      assert(input_opt.has_value());
      input = input_opt.value();
      std::tie(run_status, epath) = run_callback(input, true, false);
      check_run_result(run_status);
      epath_truncated = tpc->truncated(epath);

      if (run_status != PATHFINDER_PASS) break;
    }
    total_gen_cnt++;
    PATHFINDER_CHECK(
        epath.size() != 0,
        "Exited before `PathFinderExecuteTarget`.\n"
        "Make sure your fuzz driver does not terminate before it.");
  }

  if (WO_NBP) {
    log_msg(VERBOSE_MID, "\n" + singleline()) time_warming_up +=
        elapsed_from_ns(warming_up_start);
    return;
  }

  // Check if execution paths of one input conflicts
  for (size_t i = 0; i < cnt; i++) {
    ExecPath epath_from_same_input = run_callback(input, false, false).second;

    if (tpc->eq_significant(epath, epath_from_same_input) ||
        tpc->considerably_longer(epath_from_same_input, epath)) {
      continue;
    } else if (tpc->considerably_longer(epath, epath_from_same_input)) {
      exectree->purge_and_reinsert(epath, epath_from_same_input);
      continue;
    }

    // Conflict
    log_msg(VERBOSE_MID,
            "\nFound different execution path from same input(length: " +
                std::to_string(epath.size()) + ", " +
                std::to_string(epath_from_same_input.size()) +
                "). Check nondeterministic PCs");
    tpc->CheckDiff(epath, epath_from_same_input);
    if (!epath_truncated) {
      epath = tpc->Prune(std::move(epath));
    } else {
      epath = run_callback(input, false, false).second;
      epath_truncated = tpc->truncated(epath);
    }
    i = 0;
  }
  log_msg(VERBOSE_MID, "\n" + singleline()) time_warming_up +=
      elapsed_from_ns(warming_up_start);
}
void Engine::run() {
  exit_if_time_up();
  iter++;
  phase = FUZZ_RUNNING;
  PATHFINDER_TIMER(
      time_scheduling, std::vector<EnumCondition*> enum_conditions;
      std::vector<NumericCondition*> numeric_conditions;
      if (!exectree->is_empty()) {
        Node* target = schedule();
        std::tie(enum_conditions, numeric_conditions) = target->get_path_cond();
      });
  PATHFINDER_TIMER(time_generation_setting,
                   set_generator(enum_conditions, numeric_conditions));
  gen_remained = MAX_GEN_PER_ITER;
  size_t gen_time = 0;
  std::chrono::steady_clock::time_point before_iter =
      std::chrono::steady_clock::now();
  while (gen_remained > 0 && gen_time < MAX_TIME_PER_ITER) {
    exit_if_time_up();
    Input input;
    int run_status;
    ExecPath epath;
    while (true) {
      PATHFINDER_TIMER(time_generation_gen, auto input_opt = run_generator(););
      if (!input_opt.has_value()) return;
      input = input_opt.value();
      PATHFINDER_TIMER(
          time_running_callback,
          std::tie(run_status, epath) = run_callback(input, true, false););
      PATHFINDER_TIMER(time_result_check, check_run_result(run_status));

      if (run_status == 0 || run_status == PATHFINDER_EXPECTED_EXCEPTION) break;

      gen_time = elapsed_from_ms(before_iter);
      if (gen_time >= MAX_TIME_PER_ITER) return;
    }
    gen_remained--;
    total_gen_cnt++;

    PATHFINDER_CHECK(
        epath.size() != 0,
        "Exited before `PathFinderExecuteTarget`.\n"
        "Make sure your fuzz driver does not terminate before it.\n");

    if (exectree->has(input)) {
      // HANDLING DUPLICATE
      ExecPath epath_old;
      epath_old = exectree->get_path(input);
      assert(!epath_old.empty());

      if (tpc->eq_significant(epath_old, epath) ||
          tpc->considerably_longer(epath, epath_old)) {
        gen_time = elapsed_from_ms(before_iter);
        continue;
      } else if (tpc->considerably_longer(epath_old, epath)) {
        exectree->purge_and_reinsert(epath_old, epath);
        gen_time = elapsed_from_ms(before_iter);
        continue;
      }

      if (WO_NBP) {
        Node* leaf_old = exectree->get_leaf(input);
        Node* leaf_new = exectree->find(epath);
        assert(leaf_old != leaf_new);
        if (leaf_new == nullptr)
          leaf_new = exectree->insert(epath, input, run_status);
        Node* lca = leaf_old->lowest_common_ancestor(leaf_new);
        assert(lca->is_internal());
        for (auto& child : as_internal(lca)->children)
          child->cond = std::make_unique<NeglectCondition>();
        return;
      }

      // Execution path conflict
      log_msg(VERBOSE_MID, "\n" + singleline() +
                               "Found a conflicting input(length: " +
                               std::to_string(epath_old.size()) + ", " +
                               std::to_string(epath.size()) +
                               "). Checking difference of execution paths");
      num_conflict++;
      PATHFINDER_TIMER(time_handling_duplicate_checkdiff,
                       tpc->CheckDiff(epath_old, epath));
      PATHFINDER_TIMER(time_handling_duplicate_reconstruction,
                       exectree->prune(););
      PATHFINDER_TIMER(
          time_handling_duplicate_synthesis,
          std::set<Node*> invalid_nodes = exectree->invalid_condition_nodes();
          refine(invalid_nodes););

      PATHFINDER_TIMER(
          time_handling_duplicate_dump,
          log_msg(VERBOSE_MID, singleline() + "iter " + std::to_string(iter) +
                                   ": Execution Tree Reconstructed.\n" +
                                   singleline() + to_string()););
      return;
    }
    bool found_new_path = false;
    PATHFINDER_TIMER(time_path_check_duplicate,
                     bool is_existing_epath = exectree->has(epath););
    if (!is_existing_epath) {
      found_new_path = true;
      PATHFINDER_TIMER(time_path_check_insert,
                       exectree->insert(epath, input, run_status));
      assert(exectree->is_sorted());
    }

    PATHFINDER_TIMER(
        time_condition_evaluation,
        std::set<Node*> incorrect_nodes =
            exectree->evaluate_conditions(input, epath);
        bool found_counter_example = !incorrect_nodes.empty();
        if (!found_new_path && found_counter_example) {
          exectree->insert(epath, input, run_status);
          assert(exectree->is_sorted());
        } std::set<Node*>
            refinement_target;
        for (auto& node
             : incorrect_nodes) {
          if (auto sibling = node->get_sibling())
            if (refinement_target.find(sibling.value()) !=
                refinement_target.end())
              continue;

          if (!node->cond->is_accurate()) refinement_target.insert(node);
        });

    PATHFINDER_TIMER(time_synthesis, refine(refinement_target););

    assert(exectree->is_sorted());

    if (found_new_path || found_counter_example) {
      PATHFINDER_TIMER(time_dump,
                       log_msg(VERBOSE_MID, "\n" + singleline() + "iter " +
                                                std::to_string(iter) + "\n" +
                                                singleline() + to_string()););
    }

    if (found_new_path || found_counter_example) return;

    gen_time = elapsed_from_ms(before_iter);
  }
  log_msg(VERBOSE_LOW, ".");
}
void Engine::reset_counter() {
  // TODO: Remove it?
  /* time_gen = 0;
  time_syn = 0;
  time_run = 0;
  total_gen_cnt = 0; */
}
int Engine::get_gen_cnt() const { return total_gen_cnt; }
size_t Engine::get_num_path() const {
  size_t num_path = 0;
  if (exectree != nullptr) num_path += exectree->leaves.size();

  return num_path;
}
std::string Engine::stats() const {
  std::string str;
  str += "Number of instrumented PCs: " +
         std::to_string(tpc->GetNumInstrumented()) + "\n";
  str += "Number of covered PCs: " + std::to_string(covered_pc) + "\n";
  size_t num_nd_pc = tpc->GetNumND();
  if (num_nd_pc != 0)
    str +=
        "Number of nondeterministic PCs: " + std::to_string(num_nd_pc) + "\n";

  std::string str_time_detailed;
  str_time_detailed +=
      side_align("Time for warming up: ",
                 std::to_string(ns_to_ms(time_warming_up)) + " ms", 60) +
      "\n";
  str_time_detailed +=
      side_align("Time for conflict check: ",
                 std::to_string(ns_to_ms(time_conflict_check())) + " ms", 60) +
      "\n";
  str_time_detailed +=
      side_align("    conflict check internal: ",
                 std::to_string(ns_to_ms(time_conflict_check_internal)) + " ms",
                 60) +
      "\n";
  str_time_detailed +=
      side_align(
          "    reconstruction: ",
          std::to_string(ns_to_ms(time_conflict_check_reconstruction)) + " ms",
          60) +
      "\n";
  str_time_detailed +=
      side_align("    dump: ",
                 std::to_string(ns_to_ms(time_conflict_check_dump)) + " ms",
                 60) +
      "\n";
  str_time_detailed +=
      side_align(
          "    synthesis: ",
          std::to_string(ns_to_ms(time_conflict_check_synthesis)) + " ms", 60) +
      "\n";
  str_time_detailed +=
      side_align("Time for scheduling: ",
                 std::to_string(ns_to_ms(time_scheduling)) + " ms", 60) +
      "\n";
  str_time_detailed +=
      side_align("Time for generator setting: ",
                 std::to_string(ns_to_ms(time_generation_setting)) + " ms",
                 60) +
      "\n";
  str_time_detailed +=
      side_align("Time for generation: ",
                 std::to_string(ns_to_ms(time_generation_gen)) + " ms", 60) +
      "\n";
  str_time_detailed +=
      side_align("Time for running callback: ",
                 std::to_string(ns_to_ms(time_running_callback)) + " ms", 60) +
      "\n";
  str_time_detailed +=
      side_align("Time for result check: ",
                 std::to_string(ns_to_ms(time_result_check)) + " ms", 60) +
      "\n";
  str_time_detailed +=
      side_align("Time for handling duplicate: ",
                 std::to_string(ns_to_ms(time_handling_duplicate())) + " ms",
                 60) +
      "\n";
  str_time_detailed +=
      side_align(
          "    check diff: ",
          std::to_string(ns_to_ms(time_handling_duplicate_run_callback)) +
              " ms",
          60) +
      "\n";
  str_time_detailed +=
      side_align(
          "    check diff: ",
          std::to_string(ns_to_ms(time_handling_duplicate_checkdiff)) + " ms",
          60) +
      "\n";
  str_time_detailed +=
      side_align(
          "    reconstruction: ",
          std::to_string(ns_to_ms(time_handling_duplicate_reconstruction)) +
              " ms",
          60) +
      "\n";
  str_time_detailed +=
      side_align(
          "    reconstruction: ",
          std::to_string(ns_to_ms(time_handling_duplicate_synthesis)) + " ms",
          60) +
      "\n";
  str_time_detailed +=
      side_align("    dump: ",
                 std::to_string(ns_to_ms(time_handling_duplicate_dump)) + " ms",
                 60) +
      "\n";
  str_time_detailed +=
      side_align("Time for path check: ",
                 std::to_string(ns_to_ms(time_path_check())) + " ms", 60) +
      "\n";
  str_time_detailed +=
      side_align("    check duplicate: ",
                 std::to_string(ns_to_ms(time_path_check_duplicate)) + " ms",
                 60) +
      "\n";
  str_time_detailed +=
      side_align("    insert: ",
                 std::to_string(ns_to_ms(time_path_check_insert)) + " ms", 60) +
      "\n";
  str_time_detailed +=
      side_align("Time for condition evaluation: ",
                 std::to_string(ns_to_ms(time_condition_evaluation)) + " ms",
                 60) +
      "\n";
  str_time_detailed +=
      side_align("Time for synthesis: ",
                 std::to_string(ns_to_ms(time_synthesis)) + " ms", 60) +
      "\n";
  str_time_detailed +=
      side_align("Time for dump: ", std::to_string(ns_to_ms(time_dump)) + " ms",
                 60) +
      "\n";
  add_str(VERBOSE_HIGH, str, str_time_detailed);
  str += "Total elpased time: " + std::to_string(elapsed_from_ms(start_time)) +
         " ms \n\n";
  return str;
}
void Engine::output_stat(const std::string& filename, size_t time) const {
  const std::string comma = ",";
  std::string str;
  str += "============== " + std::to_string(time) + " ==============\n";
  str += "Number of instrumented PCs" + comma +
         std::to_string(tpc->GetNumInstrumented()) + "\n";
  str += "Number of covered PCs" + comma + std::to_string(covered_pc) + "\n";
  str += "Number of nondeterministic PCs" + comma +
         std::to_string(tpc->GetNumND()) + "\n";
  str += "Number of generation" + comma + std::to_string(total_gen_cnt) + "\n";
  str += "Number of nodes in ACT" + comma +
         std::to_string(exectree->get_leaves().size() +
                        exectree->get_internals().size()) +
         "\n";
  str += "    Internals" + comma +
         std::to_string(exectree->get_internals().size()) + "\n";
  str += "    Leaves" + comma + std::to_string(exectree->get_leaves().size()) +
         "\n";
  str += "Total prefix length of ACT" + comma +
         std::to_string(exectree->total_prefix_length()) + "\n";
  str += "Total argument size" + comma +
         std::to_string(params_size() * exectree->num_total_input()) + "\n";
  str +=
      "    Number of arguments" + comma + std::to_string(params_size()) + "\n";
  str += "    Total number of input in ACT" + comma +
         std::to_string(exectree->num_total_input()) + "\n\n";
  str += "Number of passed inputs" + comma + std::to_string(num_pass) + "\n";
  str += "Number of failed inputs" + comma + std::to_string(num_fail) + "\n\n";
  str += "Time for warming up(ms)" + comma +
         std::to_string(ns_to_ms(time_warming_up)) + "\n";
  str += "Time for conflict check(ms)" + comma +
         std::to_string(ns_to_ms(time_conflict_check())) + "\n";
  str += "    conflict check internal(ms)" + comma +
         std::to_string(ns_to_ms(time_conflict_check_internal)) + "\n";
  str += "    reconstruction(ms)" + comma +
         std::to_string(ns_to_ms(time_conflict_check_reconstruction)) + "\n";
  str += "    dump(ms)" + comma +
         std::to_string(ns_to_ms(time_conflict_check_dump)) + "\n";
  str += "    synthesis(ms)" + comma +
         std::to_string(ns_to_ms(time_conflict_check_synthesis)) + "\n";
  str += "Time for scheduling(ms)" + comma +
         std::to_string(ns_to_ms(time_scheduling)) + "\n";
  str += "Time for generator setting(ms)" + comma +
         std::to_string(ns_to_ms(time_generation_setting)) + "\n";
  str += "Time for generation(ms)" + comma +
         std::to_string(ns_to_ms(time_generation_gen)) + "\n";
  str += "Time for running callback(ms)" + comma +
         std::to_string(ns_to_ms(time_running_callback)) + "\n";
  str += "Time for result check(ms)" + comma +
         std::to_string(ns_to_ms(time_result_check)) + "\n";
  str += "Time for handling duplicate(ms)" + comma +
         std::to_string(ns_to_ms(time_handling_duplicate())) + "\n";
  str += "    num conflicts" + comma + std::to_string(num_conflict) + "\n";
  str += "    run callback(ms)" + comma +
         std::to_string(ns_to_ms(time_handling_duplicate_run_callback)) + "\n";
  str += "    check diff(ms)" + comma +
         std::to_string(ns_to_ms(time_handling_duplicate_checkdiff)) + "\n";
  str += "    reconstruction(ms)" + comma +
         std::to_string(ns_to_ms(time_handling_duplicate_reconstruction)) +
         "\n";
  str += "    synthesis(ms)" + comma +
         std::to_string(ns_to_ms(time_handling_duplicate_synthesis)) + "\n";
  str += "    dump(ms)" + comma +
         std::to_string(ns_to_ms(time_handling_duplicate_dump)) + "\n";
  str += "Time for path check(ms)" + comma +
         std::to_string(ns_to_ms(time_path_check())) + "\n";
  str += "    check duplicate(ms)" + comma +
         std::to_string(ns_to_ms(time_path_check_duplicate)) + "\n";
  str += "    insert(ms)" + comma +
         std::to_string(ns_to_ms(time_path_check_insert)) + "\n";
  str += "Time for condition evaluation(ms)" + comma +
         std::to_string(ns_to_ms(time_condition_evaluation)) + "\n";
  str += "Time for synthesis(ms)" + comma +
         std::to_string(ns_to_ms(time_synthesis)) + "\n";
  str +=
      "Time for dump(ms)" + comma + std::to_string(ns_to_ms(time_dump)) + "\n";
  str += "Total elpased time(ms)" + comma +
         std::to_string(elapsed_from_ms(start_time)) + "\n";

  std::ofstream file(filename, std::ios::app);
  if (!file.is_open()) {
    std::cerr << "Failed to open `" << filename << "` file" << std::endl;
    return;
  }
  file.write(str.data(), sizeof(str.data()[0]) * str.size());
  file.close();
}
std::string Engine::to_string() const {
  std::string str;
  add_str(VERBOSE_MID, str, exectree->to_string());
  add_str(
      VERBOSE_MID, str,
      std::string("\nFound paths: ") + std::to_string(get_num_path()) + "\n");
  if (phase == FUZZ_RUNNING)
    add_str(VERBOSE_MID, str,
            std::string("Total number of generation: ") +
                std::to_string(total_gen_cnt) + "\n");
  add_str(VERBOSE_MID, str, stats());

  return str;
}

const std::string& Engine::potential_crash_prefix() {
  static const std::string prefix = "CRASH_";
  return prefix;
}
fs::path Engine::output_file_path(const std::string& file_name) {
  return CORPUS / file_name;
}
void Engine::write_to_output_corpus(Input input) {
  std::stringstream ss_time;
  ss_time << std::setw(10) << std::setfill('0') << elapsed_from_s(start_time);
  std::string time = ss_time.str();

  std::stringstream ss_gen;
  ss_gen << std::setw(10) << std::setfill('0') << total_gen_cnt;
  std::string gen = ss_gen.str();

  std::string seed_name =
      potential_crash_prefix() + "time" + time + "_gen" + gen;
  assert(fs::is_directory(CORPUS));
  fs::path seed_path = output_file_path(seed_name);
  PATHFINDER_CHECK(
      !fs::is_regular_file(seed_path),
      "PathFinder Error: File name conflict `" + seed_path.string() + "`");
  FILE* out = fopen(seed_path.c_str(), "wb");
  if (!out) return;
  std::vector<long> serialized = serialize(input);
  fwrite(serialized.data(), sizeof(serialized.data()[0]), serialized.size(),
         out);
  fclose(out);

  last_written_seed = seed_name;
}
void Engine::commit_last_seed() {
  assert(is_prefix_of(potential_crash_prefix(), last_written_seed));
  std::string old_seed_name = last_written_seed;
  std::string new_seed_name =
      last_written_seed.substr(potential_crash_prefix().size());
  fs::rename(output_file_path(old_seed_name), output_file_path(new_seed_name));
}
void Engine::delete_last_seed() {
  fs::remove(output_file_path(last_written_seed));
}

size_t Engine::time_conflict_check() const {
  return time_conflict_check_internal + time_conflict_check_reconstruction +
         time_conflict_check_dump + time_conflict_check_synthesis;
}
size_t Engine::time_handling_duplicate() const {
  return time_handling_duplicate_run_callback +
         time_handling_duplicate_checkdiff +
         time_handling_duplicate_reconstruction +
         time_handling_duplicate_synthesis + time_handling_duplicate_dump;
}
size_t Engine::time_path_check() const {
  return time_path_check_duplicate + time_path_check_insert;
}

}  // namespace pathfinder
