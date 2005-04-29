/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details " } */

#define L 100
#define M 100
#define N 100

int bar (float);

int foo (float A[][M][N])
{
  int i, j, k;
  
  for (i = 0; i < L; i++)
    for (j = 0; j < M; j++)
      for (k = 0; k < N; k++)
	A[i][j][j] = A[i][j][k];
  
  return bar (A[10][11][12]);
}

/* The following evolution functions have to be detected:
   
   i  ->  {0, +, 1}_1
   j  ->  {0, +, 1}_2
   k  ->  {0, +, 1}_3
   
   For the subscript [j] vs. [k], "{0, +, 1}_2"  vs.  "{0, +, 1}_3"
   the overlapping elements are respectively located at iterations:
   {0, +, 1}_3 and {0, +, 1}_2.
   
*/

/* { dg-final { diff-tree-dumps "ddall" } } */
