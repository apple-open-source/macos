#include <stdio.h>

const int const_int = 5;

namespace Test {
  int myVar;
}

int main ()
{
  /* If you don't take it's address, gcc will just treat the const as a 
     macro and not emit any debug info for it.  */
  const int *foo = &const_int;
  Test::myVar = *foo;
  printf("Test got %d, const_int is %d\n", Test::myVar, const_int);

  return 0;
}
