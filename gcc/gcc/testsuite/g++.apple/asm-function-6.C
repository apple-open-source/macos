/* APPLE LOCAL file CW asm blocks */
/* Test entry points in asm functions.  */

/* { dg-do run { target powerpc*-*-darwin* } } */
/* { dg-options "-fasm-blocks -O2" } */

#include <stdlib.h>

int entry1(int);
int entry2(int);
int entry3(int);

asm int foo(int x)
{
    addi x,x,45
  entry entry1
    addi x,x,1
  entry static entry2
    addi x,x,1
  entry extern entry3
    addi x,x,1
}

int main ()
{
  if (entry1(0) != 3)
    abort();
  if (entry2(89) != 91)
    abort();
  if (entry3(100) != 101)
    abort();
  return 0;
}
