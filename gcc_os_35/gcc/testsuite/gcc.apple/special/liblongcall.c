/* { dg-do compile } */
/* { dg-options "-mlongcall" } */
#include <stdio.h>

int
dy_foo (char *str, int i)
{
  printf ("dy_foo (\"%s\", %d)\n", str, i);
  return i + 1;
}
