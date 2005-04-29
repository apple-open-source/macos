/* APPLE LOCAL file AV */
/* { dg-do run { target powerpc*-*-* i?86-*-* } } */
/* { dg-options "-O2 -ftree-vectorize -fdump-tree-vect-stats -maltivec" { target powerpc*-*-* } } */
/* { dg-options "-O2 -ftree-vectorize -fdump-tree-vect-stats -msse" { target i?86-*-* } } */

#include <stdarg.h>
#include <signal.h>

extern void abort (void);
extern void exit (int);
#define N 16
#define MAX 42
 
int main1 ()
{  
  int A[N] = {36,39,42,45,43,32,21,12,23,34,45,56,67,78,89,11};

  int i, j;

  for (i = 0; i < N; i++)
    {
      j = A[i];
      A[i] = ( j >= MAX ? MAX : 0); 
    }

  /* check results:  */
  for (i = 0; i < N; i++)
    {
      if (A[i] > MAX)
	abort ();
    }

  return 0;
}

void
sig_ill_handler (int sig)
{
    exit(0);
}
  
int main (void)
{ 
  /* Exit on systems without altivec.  */
  signal (SIGILL, sig_ill_handler);
  /* Altivec instruction, 'vor %v0,%v0,%v0'.  */
  asm volatile (".long 0x10000484");
  signal (SIGILL, SIG_DFL);
  
  return main1 ();
} 


/* { dg-final { scan-tree-dump-times "Applying if-conversion" 1 "vect" { xfail *-*-* } } } */
/* { dg-final { scan-tree-dump-times "vectorized 1 loops" 1 "vect" { xfail *-*-* } } } */
