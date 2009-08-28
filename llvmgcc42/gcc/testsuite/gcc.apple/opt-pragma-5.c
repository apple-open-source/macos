/* Radar 3124235 */
/* { dg-do compile { target "powerpc*-*-darwin*" } } */
/* { dg-options "-O3" } */
#pragma optimization_level 0
extern void f1a(int), f4a(int), f5a(int), f6a(int);
void f4(int); void f6(int);
void f1(int x) {
  f1a(x);
}
#pragma GCC optimization_level 2
void f5(int x) {
  f5a(x);
}
#pragma GCC optimization_level 3
void f6(int x) {
  f6a(x);
}
#pragma GCC optimization_level 1
void f4(int  x) {
  f4a(x);
}
/* { dg-final { scan-assembler "bl L?_f1a" } } */
/* { dg-final { scan-assembler "bl L?_f4a" } } */
/* { dg-final { scan-assembler "b L?_f5a" } } */
/* { dg-final { scan-assembler "b L?_f6a" } } */
