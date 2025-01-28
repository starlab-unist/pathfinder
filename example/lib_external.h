#include <exception>

class InvisibleException : public std::exception {};

void throw_if(bool cond);

bool random_bool();
