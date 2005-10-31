#include <stdio.h>

int foo3 (arg)
{
  char *foo = (char *) 0x0;
  int i = 10;
  foo[0] = 5;
  return 5 * arg;
}

int foo2 (int arg)
{
  return 5 * foo3 (arg);
}

int foo1 (int arg)
{
  return 5 * foo2 (arg);
}

int main (int argc, char **argv)
{
  printf ("I got %d\n", foo1(argc));

  return 0;
}
