/* APPLE LOCAL 4760857 */
/* Check that loop optimizations are handled correctly by
   #pragma optimization_level.  */
/* { dg-do compile { target "i?86*-*-darwin*" } } */
/* { dg-options "-Os" } */
extern int a[];
extern float b[];
#pragma GCC optimization_level 2
int foo ()
{
  int i;
  for (i=0; i<100; i++)
    a[i] = b[i];
}
#pragma GCC optimization_level reset
/* { dg-final { scan-assembler "align" } } */
