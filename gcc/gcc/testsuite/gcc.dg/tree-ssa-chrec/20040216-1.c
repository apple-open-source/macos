/* APPLE LOCAL file lno */
/* Test dependence graph.  */

/* { dg-do compile } */
/* { dg-options "-O2 -floop-test -c -fdump-tree-all" } */

#define N 16
void bar(int *);
void foo()
{
  int i,j;
  int A[N];
  int X[N];
  int Y[N];
  int Z[N];

  for (i=2; i<9; i++)
    {
      X[i] = Y[i] + Z[i];
      A[i] = X[i-1] + 1;
    }

  bar (A);
}

/* Find 4 Dependence nodes */
/* { dg-final { scan-tree-dump-times "Dependence Node" 4 "lptest"} } */
