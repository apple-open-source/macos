/* PR 5967, PR 7114 */
/* { dg-do run } */
/* { dg-require-profiling "-pg" } */
/* APPLE LOCAL begin testsuite nested functions */
/* { dg-options "-O2 -pg -fnested-functions" } */
/* { dg-options "-O2 -pg -static -fnested-functions" { target hppa*-*-hpux* } } */
/* APPLE LOCAL end testsuite nested functions */
/* { dg-error "profiler" "No profiler support" { target xstormy16-*-* } 0 } */
/* { dg-error "-pg not supported" "Profiler support missing" { target *-*-sco3.2v5* } 0 } */
/* { dg-bogus "\[Uu\]nresolved symbol ._mcount" "Profiling unsupported" { xfail *-*-netware* } 0 } */

extern void abort (void);

long foo (long x)
{
  long i, sum = 0;
  long bar (long z) { return z * 2; }

  for (i = 0; i < x; i++)
    sum += bar (i);

  return sum;
}

int main (void)
{
  if (foo(10) != 90)
    abort ();
  return 0;
}
