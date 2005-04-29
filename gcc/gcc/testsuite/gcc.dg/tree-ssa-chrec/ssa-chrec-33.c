/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details " } */

void bar (int);

#define N 100
#define NPad 10
#define M 32

void foo ()
{
  short coefs[2*M];
  short input[2*N];
  short output[2*N];
  
  int    sum_real, sum_imag;
  int    i,j,k;
  
  k = NPad;
  for (i = 0; i < N; i++) 
    {
      sum_real = 0;
      sum_imag = 0;
      for (j = 0; j < M; j++) 
	{
	  sum_real +=
	    input[2*k-2*j+1]*coefs[2*j+1] - input[2*k-2*j]*coefs[2*j];
	  
	  sum_imag +=
	    input[2*k-2*j]*coefs[2*j+1] + input[2*k-2*j+1]*coefs[2*j];
	}
      output[2*i+1] = sum_imag;
      output[2*i] = sum_real;
      k++; 
    }
  bar (sum_imag);
}

/* The following evolution functions have to be detected:
   
   i  ->  {0, +, 1}_1
   j  ->  {0, +, 1}_2

*/

/* { dg-final { diff-tree-dumps "ddall" } } */
