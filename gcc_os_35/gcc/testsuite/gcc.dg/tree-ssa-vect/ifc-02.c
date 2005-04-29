/* APPLE LOCAL file  */
/* { dg-do compile { target powerpc*-*-* i?86-*-* } } */
/* { dg-options "-c -O2 -ftree-vectorize -fdump-tree-vect-details -maltivec" { target powerpc*-*-* } } */
/* { dg-options "-c -O2 -ftree-vectorize -fdump-tree-vect-details -msse" { target i?86-*-* } } */

#include <stdarg.h>
#include <signal.h>

extern int c;
struct A;
typedef struct A *A_def;
static A_def *data;

extern void abort (void);
extern void exit (int);

#define N 128

int main1 ()
{  

  unsigned int i;

  for (i = 0; i < N; i++)
    if (c)
      data[i] = 0;

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
