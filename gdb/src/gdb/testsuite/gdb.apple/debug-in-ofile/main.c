#include "dbg-in-ofile.h"
#include <stdio.h>

int main (int argc, char **argv)
{
  int retval;

  my_one (argc, &retval);

  printf ("Return value: %d\n", retval);
  return 0;
  
}
