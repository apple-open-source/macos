/* APPLE LOCAL file mainline 2005-04-25  */
/* Test Attribute Vector associated with vectory type stabs.  */
/* { dg-do compile { target powerpc*-*-darwin* } } */
/* { dg-options "-gstabs -fno-eliminate-unused-debug-types -faltivec" } */

int main ()
{
  vector int vi = { 6,7,8,9 };
  return 0;
}

/* { dg-final { scan-assembler ".stabs.*\@V" } } */
