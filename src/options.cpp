#include "options.h"

#include <getopt.h>

#include <cassert>
#include <climits>
#include <cstring>
#include <fstream>
#include <iostream>

#include "utils.h"

namespace pathfinder {

enum PATHFINDER_OPTION {
  OPT_DUET_OPT,
  OPT_SYNTHESIS_BUDGET,

  OPT_CORPUS,
  OPT_OUTPUT_UNIQUE,
  OPT_OUTPUT_COV,
  OPT_OUTPUT_STAT,
  OPT_COLORIZE_OUTPUT,

  OPT_RUN_ONLY,
  OPT_RUN_CORPUS_FROM_GEN,
  OPT_RUN_CORPUS_TO_GEN,
  OPT_RUN_CORPUS_FROM_TIME,
  OPT_RUN_CORPUS_TO_TIME,
  OPT_RUN_CMD_INPUT,
  OPT_CONSTRAINT,
  OPT_IGNORE_EXCEPTION,

  OPT_SCHEDULE,
  OPT_INT_MIN,
  OPT_INT_MAX,
  OPT_MUT_RATE,
  OPT_COND_ACCURACY_THRESHOLD,
  OPT_WO_NBP,
  OPT_MAX_TOTAL_TIME,
  OPT_MAX_TOTAL_GEN,
  OPT_COV_INTERVAL_TIME,
  OPT_COV_INTERVAL_GEN,

  OPT_MAX_ITER,
  OPT_VERBOSE_LEVEL,
  OPT_MAX_GEN_PER_ITER,
  OPT_MAX_TIME_PER_ITER,
  OPT_CALLBACK_TIMEOUT,

  OPT_HELP,
};

option longopts[] = {
    {"duet_opt", required_argument, NULL, OPT_DUET_OPT},
    {"synthesis_budget", required_argument, NULL, OPT_SYNTHESIS_BUDGET},

    {"corpus", required_argument, NULL, OPT_CORPUS},
    {"output_unique", no_argument, NULL, OPT_OUTPUT_UNIQUE},
    {"output_cov", required_argument, NULL, OPT_OUTPUT_COV},
    {"output_stat", required_argument, NULL, OPT_OUTPUT_STAT},
    {"colorize", required_argument, NULL, OPT_COLORIZE_OUTPUT},

    {"run_only", no_argument, NULL, OPT_RUN_ONLY},
    {"run_corpus_from_gen", required_argument, NULL, OPT_RUN_CORPUS_FROM_GEN},
    {"run_corpus_to_gen", required_argument, NULL, OPT_RUN_CORPUS_TO_GEN},
    {"run_corpus_from_time", required_argument, NULL, OPT_RUN_CORPUS_FROM_TIME},
    {"run_corpus_to_time", required_argument, NULL, OPT_RUN_CORPUS_TO_TIME},
    {"run_cmd_input", required_argument, NULL, OPT_RUN_CMD_INPUT},
    {"constraint", required_argument, NULL, OPT_CONSTRAINT},
    {"ignore_exception", no_argument, NULL, OPT_IGNORE_EXCEPTION},

    {"schedule", required_argument, NULL, OPT_SCHEDULE},
    {"min", required_argument, NULL, OPT_INT_MIN},
    {"max", required_argument, NULL, OPT_INT_MAX},
    {"mut_rate", required_argument, NULL, OPT_MUT_RATE},
    {"cond_accuracy_threshold", required_argument, NULL,
     OPT_COND_ACCURACY_THRESHOLD},
    {"wo_nbp", no_argument, NULL, OPT_WO_NBP},
    {"max_total_time", required_argument, NULL, OPT_MAX_TOTAL_TIME},
    {"max_total_gen", required_argument, NULL, OPT_MAX_TOTAL_GEN},
    {"cov_interval_time", required_argument, NULL, OPT_COV_INTERVAL_TIME},
    {"cov_interval_gen", required_argument, NULL, OPT_COV_INTERVAL_GEN},

    {"iter", required_argument, NULL, OPT_MAX_ITER},
    {"verbose", required_argument, NULL, OPT_VERBOSE_LEVEL},
    {"max_gen_per_iter", required_argument, NULL, OPT_MAX_GEN_PER_ITER},
    {"max_time_per_iter", required_argument, NULL, OPT_MAX_TIME_PER_ITER},
    {"callback_timeout", required_argument, NULL, OPT_CALLBACK_TIMEOUT},

    {"help", no_argument, NULL, OPT_HELP},
    {0}};

std::string DUET_OPT = "-all";
size_t SYNTHESIS_BUDGET = 4;

fs::path CORPUS;
bool OUTPUT_UNIQUE = true;
std::string COV_OUTPUT_FILENAME = "";
std::string STAT_OUTPUT_FILENAME = "";
bool COLORIZE_OUTPUT = true;

bool RUN_ONLY = false;
int RUN_CORPUS_FROM_GEN = -1;
int RUN_CORPUS_TO_GEN = INT_MAX;
int RUN_CORPUS_FROM_TIME = -1;
int RUN_CORPUS_TO_TIME = INT_MAX;
std::string CMD_LINE_INPUT;
std::string CMD_LINE_CONSTRAINT;
bool IGNORE_EXCEPTION = false;

SCHEDULE SCHEDULING_STRATEGY = SCHEDULE_RAND;
int ARG_INT_MIN = -64;
int ARG_INT_MAX = 64;
int MAX_GEN_PER_ITER = 10;
size_t MAX_TIME_PER_ITER = 10000;  // max time per iteration in milliseconds.
float MUT_RATE = 0.2f;
float COND_ACCURACY_THRESHOLD = 0.6f;
bool WO_NBP = false;

int MAX_ITER = INT_MAX;
unsigned CALLBACK_TIMEOUT = 1;
size_t MAX_TOTAL_TIME = INT_MAX;
size_t MAX_TOTAL_GEN = INT_MAX;
size_t COV_INTERVAL_TIME = 0;
size_t COV_INTERVAL_GEN = 0;

void print_usage(int exit_code, char* program_name) {
  printf("Usage : %s [...]\n", program_name);
  printf(
      "    --duet_opt                  Cmd options for a duet.\n"
      "    --synthesis_budget          Synthesis budget for each branch "
      "condition in seconds. (default=4)\n"

      "    --corpus                    Starting corpus directory. If not "
      "exists, make one.\n"
      "    --output_unique             Output unique(path) inputs only. "
      "(default=1)\n"
      "    --output_cov                Output coverage(csv) to given file "
      "name.\n"
      "    --output_stat               Output statistic summary(csv) to given "
      "file name.\n"
      "    --colorize                  Colorize output. (default=1)\n\n"

      "    --run_only                  Run inputs in corpus and exit. Useful "
      "when measure coverage of generated inputs.\n"
      "    --run_corpus_from_gen       Run inputs in corpus whose gen count is "
      "in [run_corpus_from_gen,run_corpus_to_gen). (default=-1)\n"
      "                                Negative `run_corpus_from_gen` means "
      "running corpus from initial seeds.\n"
      "    --run_corpus_to_gen         Run inputs in corpus whose gen count is "
      "in [run_corpus_from_gen,run_corpus_to_gen). (default=SIZE_MAX)\n"
      "    --run_corpus_from_time      Run inputs in corpus which is generated "
      "in [run_corpus_from_time,run_corpus_to_time). (default=-1)\n"
      "                                Negative `run_corpus_from_time` means "
      "running corpus from initial seeds.\n"
      "    --run_corpus_to_time        Run inputs in corpus which is generated "
      "in [run_corpus_from_time,run_corpus_to_time). (default=SIZE_MAX)\n"
      "    --run_cmd_input             Run fuzz target with an input provided "
      "by command-line.\n"
      "                                Should be quoted and space(or comma) "
      "separated(e.g., --run_cmd_input \"1 2 3\", --run_cmd_input \"1,2,3\").\n"
      "    --constraint                Add additional constraint(s). "
      "\"argN==10\" enforces Nth element to be 10.\n"
      "                                Should be quoted and comma "
      "separated(e.g., --constraint \"arg0 >= 0, arg5 == 10\").\n"
      "    --ignore_exception          Does not terminate on every exception. "
      "Useful when measure coverage.\n\n"

      "    --schedule                  Set scheduling strategy. Should be one "
      "of {random}. (default=random)\n"
      "    --min                       Minimum integer value of variables in "
      "synthesized function for searching CEs. (default=-64)\n"
      "    --max                       Maximum integer value of variables in "
      "synthesized function for searching CEs. (default=64)\n"
      "    --mut_rate                  Mutation rate of concrete input "
      "generation.\n"
      "    --cond_accuracy_threshold   If accuracy of a barnch condition is "
      "lower than this, try refinement. (default=0.6)\n"
      "    --wo_nbp                    Disable nondeterministic branch "
      "pruning.\n\n"

      "    --iter                      Max number of refining iteration. "
      "(default=INT_MAX).\n"
      "    --verbose                   Verbose level of logging. Should be one "
      "of {0,1,2}. (default=0).\n"
      "    --max_gen_per_iter          Max number of solver iteration per "
      "target branch.\n"
      "    --max_time_per_iter         Max time per iteration of target branch "
      "in milliseconds.\n"
      "    --callback_timeout          Timeout of each execution of target "
      "function in seconds. (default=1)\n"
      "    --max_total_time            Maximum total time in seconds.\n"
      "    --max_total_gen             Maximum total input generation.\n"
      "    --cov_interval_time         Time interval for checking coverage.\n"
      "    --cov_interval_gen          Gen interval for checking coverage.\n\n"

      "    --help                      Display this usage information.\n");
  exit(exit_code);
}

void parse_arg(int argc, char** argv) {
  while (1) {
    int opt = getopt_long(argc, argv, "", longopts, 0);
    if (opt == -1) break;
    switch (opt) {
      case OPT_DUET_OPT:
        DUET_OPT = optarg;
        break;
      case OPT_SYNTHESIS_BUDGET:
        SYNTHESIS_BUDGET = strtof(optarg, NULL);
        break;
      case OPT_SCHEDULE:
        if (strcmp(optarg, "rand") == 0) {
          SCHEDULING_STRATEGY = SCHEDULE_RAND;
          break;
        } else {
          std::cout << "PathFinder Error: Invalid scheduling option `" << optarg
                    << "`. Available scheduling options: {rand}.\n";
          exit(0);
        }
      case OPT_INT_MIN:
        ARG_INT_MIN = atoi(optarg);
        break;
      case OPT_INT_MAX:
        ARG_INT_MAX = atoi(optarg);
        break;
      case OPT_MUT_RATE:
        MUT_RATE = strtof(optarg, NULL);
        break;
      case OPT_COND_ACCURACY_THRESHOLD:
        COND_ACCURACY_THRESHOLD = strtof(optarg, NULL);
        break;
      case OPT_WO_NBP:
        WO_NBP = true;
        break;
      case OPT_CORPUS:
        CORPUS = fs::path(optarg);
        break;
      case OPT_OUTPUT_UNIQUE:
        OUTPUT_UNIQUE = true;
        break;
      case OPT_OUTPUT_COV:
        COV_OUTPUT_FILENAME = optarg;
        break;
      case OPT_OUTPUT_STAT:
        STAT_OUTPUT_FILENAME = optarg;
        break;
      case OPT_COLORIZE_OUTPUT:
        COLORIZE_OUTPUT = bool(optarg);
        break;
      case OPT_RUN_ONLY:
        RUN_ONLY = true;
        break;
      case OPT_RUN_CORPUS_FROM_GEN:
        RUN_CORPUS_FROM_GEN = atoi(optarg);
        break;
      case OPT_RUN_CORPUS_TO_GEN:
        RUN_CORPUS_TO_GEN = atoi(optarg);
        break;
      case OPT_RUN_CORPUS_FROM_TIME:
        RUN_CORPUS_FROM_TIME = atoi(optarg);
        break;
      case OPT_RUN_CORPUS_TO_TIME:
        RUN_CORPUS_TO_TIME = atoi(optarg);
        break;
      case OPT_RUN_CMD_INPUT:
        CMD_LINE_INPUT = optarg;
        break;
      case OPT_CONSTRAINT:
        CMD_LINE_CONSTRAINT = optarg;
        break;
      case OPT_IGNORE_EXCEPTION:
        IGNORE_EXCEPTION = true;
        break;
      case OPT_MAX_ITER:
        MAX_ITER = atoi(optarg);
        break;
      case OPT_VERBOSE_LEVEL:
        V_LEVEL = static_cast<VERBOSE_LEVEL>(atoi(optarg));
        break;
      case OPT_MAX_GEN_PER_ITER:
        MAX_GEN_PER_ITER = atoi(optarg);
        break;
      case OPT_MAX_TIME_PER_ITER:
        MAX_TIME_PER_ITER = (size_t)atoi(optarg);
        break;
      case OPT_CALLBACK_TIMEOUT:
        CALLBACK_TIMEOUT = (size_t)atoi(optarg);
        break;
      case OPT_MAX_TOTAL_TIME:
        MAX_TOTAL_TIME = (size_t)atoi(optarg);
        break;
      case OPT_MAX_TOTAL_GEN:
        MAX_TOTAL_GEN = (size_t)atoi(optarg);
        break;
      case OPT_COV_INTERVAL_TIME:
        COV_INTERVAL_TIME = (size_t)atoi(optarg);
        break;
      case OPT_COV_INTERVAL_GEN:
        COV_INTERVAL_GEN = (size_t)atoi(optarg);
        break;
      case OPT_HELP:
      case '?':
        print_usage(1, argv[0]);
        break;
      default:
        throw Unreachable();
    }
  }
}

}  // namespace pathfinder
