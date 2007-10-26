/* APPLE LOCAL file mainline 5207358 */
/* { dg-do compile } */
/* { dg-options "" } */
/* { dg-options "-msse" { target i?86-*-* x86_64-*-* } } */
/* Verify that a vector divide operation works.  */
typedef float __attribute__((vector_size (16))) vFloat;

vFloat vFloat_recip(vFloat fp) {
  vFloat fpone = (vFloat) {1.0f, 1.0f, 1.0f, 1.0f};
  return fpone / fp;
}
