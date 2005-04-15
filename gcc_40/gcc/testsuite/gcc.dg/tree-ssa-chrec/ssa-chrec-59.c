/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest  " } */

extern int foo (float A[100][200]);

int bar ()
{
  int i, j;
  float A[100][200];
  
  for (i=0; i<5; i++)
    for (j=0; j<5; j++)
      A[i][j] = A[i+1][j];
  foo (A);
  return A[1][2];
}

/* { dg-final { diff-tree-dumps "ddall" } } */
