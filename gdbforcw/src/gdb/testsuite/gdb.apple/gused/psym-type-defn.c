#include <stdio.h>

/* An example program that defines a couple of times in the function parameters
   of one function that is stripped (dead code stripping, coalescing) and 
   another function which has parameters of those same types.  Can gdb see
   the types of the arguments of the second function?  */

void unused_function (int argc, const char **argv)
{
  printf ("%s\n", argv[0]);
}

int main (int argc, const char **argv)
{
  argc = 5;
  printf ("%s\n", argv[0]); /* a good place to put a breakpoint */
}
