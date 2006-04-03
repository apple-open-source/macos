/* APPLE LOCAL begin testsuite nested functions */
// { dg-do compile }
/* { dg-xfail-if "" { *-*-darwin* } } */
/* APPLE LOCAL end testsuite nested functions */
extern int printf (const char *, ...);

int foo() {
  int yd;
  float in[1][yd];
 
  void bar() {
    printf("%p\n",in[0]);
  }
}
