/* Test dead code strip support.  */
/* Contributed by Devang Patel  <dpatel@apple.com>  */

/* { dg-do compile { target *-*-darwin* } } */
/* APPLE LOCAL mainline 2006-03-26 */
/* { dg-options "-gstabs+ -fno-eliminate-unused-debug-symbols" } */

int
main ()
{
  return 0;
}

/* { dg-final { scan-assembler ".stabd.46,0,0" } } */
/* { dg-final { scan-assembler ".stabd.78,0,0" } } */

