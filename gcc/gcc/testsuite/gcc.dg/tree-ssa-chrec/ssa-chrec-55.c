/* APPLE LOCAL file lno */
#include <stdio.h>
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */

int main(int argc)
{
  int I, J;
  const int N = 30;
  const int M = 40;
  for (J = argc; J < N; J += 3)
    {
      for (I = J; I < M; I++)
	{
	  printf ("%d %d\n", I, J);
	}
    }
}
