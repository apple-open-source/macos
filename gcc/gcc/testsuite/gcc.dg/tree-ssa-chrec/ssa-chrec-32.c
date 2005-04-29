/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details " } */

void bar (short);

#define N 100
#define NPad 10
#define M 32
void foo()
{
  short coef[M];
  short input[N];
  short output[N];
  
  int i,j,k;                              
  int sum;  
  
  for (i = 0; i < N; i++) {
    sum = 0;
    for (j = 0; j < M; j++) {
      sum += input[i+NPad-j] * coef[j];
    }
    output[i] = sum;
  }
  bar (sum);
}

/* The following evolution functions have to be detected:
   
   i  ->  {0, +, 1}_1
   j  ->  {0, +, 1}_2
   
*/

/* { dg-final { diff-tree-dumps "ddall" } } */
