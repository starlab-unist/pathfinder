#include "example/utils.h"

/*
 *  `do_something()` is a dummy statement that force gcc to 'observe' behavior.
 *  An empty block without it is considered 'meaningless' so that gcc does not
 *  generate code for it, resuling malfunctioning of fuzzing process.
 * 
 *  FYI, clang works correctly with empty blocks.
 * 
 */
void do_something() {
  static long dummy = 0;
  dummy++;
}
