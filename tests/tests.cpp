
// a unit test made using the Catch framework in catch.hpp / tests.cpp.

#ifdef _WINDOWS

#define CATCH_CONFIG_RUNNER
#include "tests.h"
#include "catch.hpp"

// Windows hack to pause console output so we can read it.
#include <conio.h>
int main(int argc, char** argv)
{
  int result = Catch::Session().run(argc, argv);
  system("pause");
  return result;
}

#else

#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "tests.h"

#endif
