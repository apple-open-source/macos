/* { dg-do run { target powerpc*-*-* } } */
/* { dg-options "-fast -lmx" } */

#include <signal.h>

#include <stdio.h>
#include <math.h>
#include <complex.h>

void
sig_ill_handler (int sig)
{
    exit(0);
}

int main(void)
{

  double complex iPi, a;

  /* Exit on systems without 64bit instructions.  */
  signal (SIGILL, sig_ill_handler);
#ifdef __MACH__
  asm volatile ("extsw r0,r0");
#else
  asm volatile ("extsw 0,0");
#endif
  signal (SIGILL, SIG_DFL);


  iPi = 4 * atan(1.) * I;
  a = cexp(iPi);  /* a = e^(i*pi) = -1 */
  if (creal(a) != -1 || floor(cimag(a)) != 0)
    abort();
  exit(0);
}

