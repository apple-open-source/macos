/* APPLE LOCAL file mainline 4095526 */
/* { dg-do compile { target i?86*-*-* } } */
/* { dg-options "-O2 -march=pentium4 -mtune=prescott -mfpmath=sse" } */
double foo (double x) {
  return x + 1.75;
}
/* { dg-final { scan-assembler-not "cvtss2sd" } } */
/* { dg-final { scan-assembler-not "flds" } } */
/* { dg-final { scan-assembler-not "movss" } } */
