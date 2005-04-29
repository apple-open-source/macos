/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest  " } */

extern int foo (float A[100]);

int bar ()
{
  int i, j;
  float A[100];
  
  for (i=0; i<5; i++)
    {
      A[i * 3] = i + 3;
      A[i + 7] = i;
    }
  
  foo (A);
  return A[1];
}

/* { dg-final { diff-tree-dumps "ddall" } } */
