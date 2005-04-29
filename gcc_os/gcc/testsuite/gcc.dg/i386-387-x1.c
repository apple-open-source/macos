/* { dg-do run { target "i?86-*-*" } } */
/* { dg-options "-Os" } */
#include <stdlib.h>

static unsigned char x = 0;
static unsigned char y = 0;

float
width(void)
{
  return 0.0f;
}

int
foo(void)
{
  float w = width();
  int index;

  if (x) {
    index = y ? 2 : 1;
  } else {
    index = y ? 2 : 0;
  }

  return index + (int)w;
}

int
main (void)
{
  int x = foo() ;
  if (x)
    abort();
  exit (0);
}

