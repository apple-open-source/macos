/* APPLE LOCAL file 4231773 */
/* { dg-do compile { target i?86-*-darwin* } } */
/* { dg-options "-Os -static" } */
/* { dg-final { scan-assembler-not "\tincl?\[ 	\]*_?global_x" } } */

/* Insure that -Os does not generate INC on Darwin/x86 (see also opt-size-2.c).  */

int global_x;

int
main ()
{
  global_x++;
  return 0;
}
