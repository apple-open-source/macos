/* { dg-do compile { target "powerpc*-*-darwin*" } } */
/* Test -falign-loops-max-skip */
/* { dg-options "-O3 -falign-loops=16 -falign-loops-max-skip=11" } */

void foo()
{
  int i;
  float j;
  for (i=0; i<1000; i++)
    j = 123*456;
}

/* { dg-final { scan-assembler "p2align 4,,11" } } */
