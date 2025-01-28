#ifndef PATHFINDER_OPTIONS
#define PATHFINDER_OPTIONS

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace pathfinder {

enum SCHEDULE {
  SCHEDULE_RAND,
};

enum VERBOSE_LEVEL {
  VERBOSE_LOW = 0,
  VERBOSE_MID = 1,
  VERBOSE_HIGH = 2,
};

extern std::string DUET_OPT;
extern size_t SYNTHESIS_BUDGET;

extern fs::path CORPUS;
extern bool OUTPUT_UNIQUE;
extern std::string COV_OUTPUT_FILENAME;
extern std::string STAT_OUTPUT_FILENAME;
extern bool COLORIZE_OUTPUT;

extern bool RUN_ONLY;
extern int RUN_CORPUS_FROM_GEN;
extern int RUN_CORPUS_TO_GEN;
extern int RUN_CORPUS_FROM_TIME;
extern int RUN_CORPUS_TO_TIME;
extern std::string CMD_LINE_INPUT;
extern std::string CMD_LINE_CONSTRAINT;
extern bool IGNORE_EXCEPTION;

extern SCHEDULE SCHEDULING_STRATEGY;
extern int ARG_INT_MIN;
extern int ARG_INT_MAX;
extern int MAX_GEN_PER_ITER;
extern size_t MAX_TIME_PER_ITER;
extern float MUT_RATE;
extern float COND_ACCURACY_THRESHOLD;
extern bool WO_NBP;

extern bool BLACKBOX;
extern int MAX_ITER;
extern VERBOSE_LEVEL V_LEVEL;
extern unsigned CALLBACK_TIMEOUT;
extern size_t MAX_TOTAL_TIME;
extern size_t MAX_TOTAL_GEN;
extern size_t COV_INTERVAL_TIME;
extern size_t COV_INTERVAL_GEN;

void parse_arg(int argc, char** argv);

}  // namespace pathfinder

#endif
