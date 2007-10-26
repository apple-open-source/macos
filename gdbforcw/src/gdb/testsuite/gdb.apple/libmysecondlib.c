#include <stdio.h>

static int foo (int input)
{
  return input * 5;
}

int blubby (int input)
{
  printf ("I got input: %d\n", input);
  if (input > 5)
    return foo (input);
  else
    return 5;
}


