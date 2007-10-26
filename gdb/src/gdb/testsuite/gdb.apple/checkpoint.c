#include <stdio.h>

int globa;

int
main()
{
  int a = 1, idx;

  ++a;

  globa = a;

  for (idx = 0; idx < 10; ++idx)
    {
      ++globa;
      subr(a, idx, globa);
    }

  return 0;
}

int
subr (int a1, int a2, int a3)
{
  int rslt;

  rslt = subsub (a1 * a2);
  return a1 + a2 + a3;
}

int
subsub (int arg)
{
  int rslt = 2;

  rslt *= arg;
  return rslt;
}
