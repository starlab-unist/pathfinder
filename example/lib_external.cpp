#include "example/lib_external.h"

#include <stdlib.h>

void throw_if(bool cond) {
  if (cond) throw InvisibleException();
}

bool random_bool() { return std::rand() % 2 == 0; }
