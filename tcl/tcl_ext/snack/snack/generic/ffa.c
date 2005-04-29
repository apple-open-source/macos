/* ------------------------------------------------------------------        *
 *                                                                           * 
 *   Copyright (C) 1995 Roger Lindell                                        * 
 *                                                                           * 
 *   All rights reserved.                                                    * 
 *                                                                           * 
 *   Adapted by Roger Lindell at the Dept. of Speech, Music and Hearing,     * 
 *   KTH (Royal Institute of Technology), Sweden, from an original algorithm *
 *   described in "Programs for Digital Signal Processing", Bergland & Dolan *
 *   (1979) Ed. by the DSP committee IEEE Acoustics, Speech and Signal       *
 *   Processing Society. Wiley, New York.                                    *
 *   Roger Lindell, rog@speech.kth.se                                        * 
 *                                                                           * 
 *   KTH                                                                     * 
 *   Institutionen foer Tal, musik och hoersel                               * 
 *   Box 700 14                                                              * 
 *   100 44 STOCKHOLM                                                        * 
 *   SWEDEN                                                                  * 
 *                                                                           * 
 *---------------------------------------------------------------------------*
 * Function Snack_DBPowerSpectrum                                            *
 *    Fast Fourier Transform for N=2**M                                      *
 *    Complex Input                                                          *
 *---------------------------------------------------------------------------*
 *                                                                           *
 *   This program replaces the vector z=x+iy by its  finite                  *
 *   discrete, complex fourier transform if in=0.  The inverse               *
 *   transform is calculated for in=1.  It performs as many base             *
 *   8 iterations as possible and then finishes with a base                  *
 *   4 iteration or a base 2 iteration if needed.                            *
 *                                                                           *
 *---------------------------------------------------------------------------*
 *   The functions are called as                                             *
 *        Snack_InitFFT(int n) ;     / * n must be a power of 2 * /          *
 *        Snack_DBPowerSpectrum (float *x) ;                                 *
 *---------------------------------------------------------------------------*
 *                                                                           */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h> 
#include "snack.h"

#ifdef __cplusplus
extern "C" {
#endif
int  Snack_InitFFT(int n);
void Snack_DBPowerSpectrum(float *x);
#ifdef __cplusplus
}
#endif
static void r2tx(int nthpo, float *cr0, float *cr1, float *ci0, float *ci1);
static void r4tx(int nthpo, float *cr0, float *cr1, float *cr2, float *cr3,
		 float *ci0, float *ci1, float *ci2, float *ci3);
static void r8tx(int nxtlt, int nthpo, int lengt,
		 float *cr0, float *cr1, float *cr2, float *cr3, float *cr4,
		 float *cr5, float *cr6, float *cr7, float *ci0, float *ci1,
		 float *ci2, float *ci3, float *ci4, float *ci5, float *ci6,
		 float *ci7);

static unsigned int Pow2[17] =
  {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768,
   65536};
static float *sint, *cost, *x, *y;
static int sint_init = 0;
static int n2pow,nthpo;
static double theta, wpr, wpi;

#define  SNACK_PI2  6.283185307179586
#define  P7   0.707106781186548
#define  LN2  0.6931471805599453

int
Snack_InitFFT(int n)
{
  int k, l, m;
  double a, p, wtemp;

  n = n>>1;
  m = (int) (log ((float) (n)) / LN2 + .5);
  l = Pow2[m];
  p = SNACK_PI2 / l;
  if (sint_init != 0) {
    ckfree ((char *) sint);
    ckfree ((char *) cost);
    ckfree ((char *) x);
    ckfree ((char *) y);
  }
  sint = (float *) ckalloc (l * sizeof(float));
  cost = (float *) ckalloc (l * sizeof(float));
  x    = (float *) ckalloc (l * sizeof(float));
  y    = (float *) ckalloc (l * sizeof(float));
  memset(sint, 0, l * sizeof(float));
  memset(cost, 0, l * sizeof(float));
  memset(x, 0, l * sizeof(float));
  memset(y, 0, l * sizeof(float));

  sint_init = 1;
  for (k = 0; k < l; k++) {
    a = k * p;
    sint[k] = (float) sin (a);
    cost[k] = (float) cos (a);
  }
  nthpo = l;
  n2pow = m;

  theta = 3.141592653589793/(double) (nthpo);
  wtemp = sin(0.5*theta);
  wpr = -2.0*wtemp*wtemp;
  wpi = sin(theta);

  return (l<<1);
}

void
Snack_DBPowerSpectrum(float *z)
{
  int l[17];
  int i, in, n8pow, fn;
  int lengt;
  register int ij, ji, nxtlt;
  int i1, i2, i3, i4, i5, i6, i7, i8, i9, i10, i11, i12, i13, i14;
  float r, fi;
  float h1r, h1i, h2r, h2i;
  double wr, wi, wtemp;

  fn = nthpo;
  for (i = 0; i < nthpo; i++) {
    y[i] = -z[(i<<1)+1];
    x[i] = z[i<<1];
  }

  n8pow = n2pow / 3;
  if (n8pow != 0) {
    for (i = 0; i < n8pow; i++) {
      lengt = n2pow - 3 * i;
      nxtlt = Pow2[lengt - 3];
      r8tx (nxtlt, nthpo, lengt,
	    &x[0], &x[nxtlt], &x[2 * nxtlt], &x[3 * nxtlt],
	    &x[4 * nxtlt], &x[5 * nxtlt], &x[6 * nxtlt], &x[7 * nxtlt],
	    &y[0], &y[nxtlt], &y[2 * nxtlt], &y[3 * nxtlt],
	    &y[4 * nxtlt], &y[5 * nxtlt], &y[6 * nxtlt], &y[7 * nxtlt]);
    }
  }

  switch (n2pow - 3 * n8pow) {
  case 0:
    break;
  case 1:
    r2tx (nthpo, &x[0], &x[1], &y[0], &y[1]);
    break;
  case 2:
    r4tx (nthpo, &x[0], &x[1], &x[2], &x[3],
	  &y[0], &y[1], &y[2], &y[3]);
    break;
  default:
    /*    fprintf (stderr, "-- Algorithm Error Snack_DBPowerSpectrum\n");*/
    exit (1);
  }
  for (i = 0; i < 17; i++) {
    if (i < n2pow) {
      l[i] = Pow2[n2pow - i];
    } else {
      l[i] = 1;
    }
  }
  ij = 0;
  for (i1 = 0; i1 < l[14]; i1++) {
    for (i2 = i1; i2 < l[13]; i2 += l[14]) {
      for (i3 = i2; i3 < l[12]; i3 += l[13]) {
	for (i4 = i3; i4 < l[11]; i4 += l[12]) {
	  for (i5 = i4; i5 < l[10]; i5 += l[11]) {
	    for (i6 = i5; i6 < l[9]; i6 += l[10]) {
	      for (i7 = i6; i7 < l[8]; i7 += l[9]) {
		for (i8 = i7; i8 < l[7]; i8 += l[8]) {
		  for (i9 = i8; i9 < l[6]; i9 += l[7]) {
		    for (i10 = i9; i10 < l[5]; i10 += l[6]) {
		      for (i11 = i10; i11 < l[4]; i11 += l[5]) {
			for (i12 = i11; i12 < l[3]; i12 += l[4]) {
			  for (i13 = i12; i13 < l[2]; i13 += l[3]) {
			    for (i14 = i13; i14 < l[1]; i14 += l[2]) {
			      for (ji = i14; ji < l[0]; ji += l[1]) {
				if (ij < ji) {
				  r = x[ij];
				  x[ij] = x[ji];
				  x[ji] = r;
				  fi = y[ij];
				  y[ij] = y[ji];
				  y[ji] = fi;
				}
				ij++;
			      }
			    }
			  }
			}
		      }
		    }
		  }
		}
	      }
	    }
	  }
	}
      }
    }
  }

  wr = 1.0+wpr;
  wi = wpi;
  for (i = 1; i <= (nthpo>>1); i++) {
    in = nthpo-i;
    h1r = (x[i]+x[in]);
    h1i = (y[i]-y[in]);
    h2r = (y[i]+y[in]);
    h2i = (x[in]-x[i]);
    x[in] = (float) (h1r+wr*h2r-wi*h2i);
    y[in] = (float) (h1i+wr*h2i+wi*h2r);
    wtemp =  x[in]*x[in]+y[in]*y[in];
    if (wtemp < SNACK_INTLOGARGMIN)
      wtemp = SNACK_INTLOGARGMIN;
    z[in] = (float) (SNACK_DB * log(wtemp) - SNACK_CORRN);
 
    x[i]  = (float) (h1r-wr*h2r+wi*h2i);
    y[i]  = (float) (-h1i+wr*h2i+wi*h2r);
    wtemp = x[i]*x[i]+y[i]*y[i];
    if (wtemp < SNACK_INTLOGARGMIN)
      wtemp = SNACK_INTLOGARGMIN;
    z[i] = (float) (SNACK_DB * log(wtemp) - SNACK_CORRN);

    wtemp = wr;
    wr = wr*wpr-wi*wpi+wr;
    wi = wi*wpr+wtemp*wpi+wi;
  }
  wtemp = x[0]-y[0];
  wtemp = wtemp*wtemp;
  if (wtemp < SNACK_INTLOGARGMIN)
    wtemp = SNACK_INTLOGARGMIN;
  z[0] = (float) (SNACK_DB * log(wtemp) - SNACK_CORR0);
}

/*--------------------------------------------------------------------*
 * Function:  r2tx                                                    *
 *      RADIX 2 ITERATION SUBROUTINE                                  *
 *--------------------------------------------------------------------*/
static void
r2tx(int nthpo, float *cr0, float *cr1, float *ci0, float *ci1)
{
  register int k;
  register float r1, fi1;

  for (k = 0; k < nthpo; k += 2) {
    r1 = cr0[k] + cr1[k];
    cr1[k] = cr0[k] - cr1[k];
    cr0[k] = r1;
    fi1 = ci0[k] + ci1[k];
    ci1[k] = ci0[k] - ci1[k];
    ci0[k] = fi1;
  }
}

/*--------------------------------------------------------------------*
 * Function:  r4tx                                                    *
 *      RADIX 4 ITERATION SUBROUTINE                                  *
 *--------------------------------------------------------------------*/
static void
r4tx(int nthpo, float *cr0, float *cr1, float *cr2, float *cr3, float *ci0, float *ci1, float *ci2, float *ci3)
{
  register int k;
  register float r1, r2, r3, r4;
  register float fi1, fi2, fi3, fi4;

  for (k = 0; k < nthpo; k += 4) {
    r1 = cr0[k] + cr2[k];
    r2 = cr0[k] - cr2[k];
    r3 = cr1[k] + cr3[k];
    r4 = cr1[k] - cr3[k];
    fi1 = ci0[k] + ci2[k];
    fi2 = ci0[k] - ci2[k];
    fi3 = ci1[k] + ci3[k];
    fi4 = ci1[k] - ci3[k];
    cr0[k] = r1 + r3;
    ci0[k] = fi1 + fi3;
    cr1[k] = r1 - r3;
    ci1[k] = fi1 - fi3;
    cr2[k] = r2 - fi4;
    ci2[k] = fi2 + r4;
    cr3[k] = r2 + fi4;
    ci3[k] = fi2 - r4;
  }
}

/*--------------------------------------------------------------------*
 * Function:  r8tx                                                    *
 *      RADIX 8 ITERATION SUBROUTINE                                  *
 *--------------------------------------------------------------------*/
static void
r8tx(int nxtlt, int nthpo, int lengt,
     float *cr0, float *cr1, float *cr2, float *cr3,
     float *cr4, float *cr5, float *cr6, float *cr7,
     float *ci0, float *ci1, float *ci2, float *ci3,
     float *ci4, float *ci5, float *ci6, float *ci7)
{
  float c1, c2, c3, c4, c5, c6, c7;
  float s1, s2, s3, s4, s5, s6, s7;
  float ar0, ar1, ar2, ar3, ar4, ar5, ar6, ar7;
  float ai0, ai1, ai2, ai3, ai4, ai5, ai6, ai7;
  float br0, br1, br2, br3, br4, br5, br6, br7;
  float bi0, bi1, bi2, bi3, bi4, bi5, bi6, bi7;
  register float tr, ti;
  register int j, k;

  for (j = 0; j < nxtlt; j++) {
    c1 = cost[(j * nthpo) >> lengt];
    s1 = sint[(j * nthpo) >> lengt];
    c2 = c1 * c1 - s1 * s1;
    s2 = c1 * s1 + c1 * s1;
    c3 = c1 * c2 - s1 * s2;
    s3 = c2 * s1 + s2 * c1;
    c4 = c2 * c2 - s2 * s2;
    s4 = c2 * s2 + c2 * s2;
    c5 = c2 * c3 - s2 * s3;
    s5 = c3 * s2 + s3 * c2;
    c6 = c3 * c3 - s3 * s3;
    s6 = c3 * s3 + c3 * s3;
    c7 = c3 * c4 - s3 * s4;
    s7 = c4 * s3 + s4 * c3;
    for (k = j; k < nthpo; k += Pow2[lengt]) {
      ar0 = cr0[k] + cr4[k];
      ar1 = cr1[k] + cr5[k];
      ar2 = cr2[k] + cr6[k];
      ar3 = cr3[k] + cr7[k];
      ar4 = cr0[k] - cr4[k];
      ar5 = cr1[k] - cr5[k];
      ar6 = cr2[k] - cr6[k];
      ar7 = cr3[k] - cr7[k];
      ai0 = ci0[k] + ci4[k];
      ai1 = ci1[k] + ci5[k];
      ai2 = ci2[k] + ci6[k];
      ai3 = ci3[k] + ci7[k];
      ai4 = ci0[k] - ci4[k];
      ai5 = ci1[k] - ci5[k];
      ai6 = ci2[k] - ci6[k];
      ai7 = ci3[k] - ci7[k];
      br0 = ar0 + ar2;
      br1 = ar1 + ar3;
      br2 = ar0 - ar2;
      br3 = ar1 - ar3;
      br4 = ar4 - ai6;
      br5 = ar5 - ai7;
      br6 = ar4 + ai6;
      br7 = ar5 + ai7;
      bi0 = ai0 + ai2;
      bi1 = ai1 + ai3;
      bi2 = ai0 - ai2;
      bi3 = ai1 - ai3;
      bi4 = ai4 + ar6;
      bi5 = ai5 + ar7;
      bi6 = ai4 - ar6;
      bi7 = ai5 - ar7;
      cr0[k] = br0 + br1;
      ci0[k] = bi0 + bi1;
      if (j > 0) {
	cr1[k] = c4 * (br0 - br1) - s4 * (bi0 - bi1);
	ci1[k] = c4 * (bi0 - bi1) + s4 * (br0 - br1);
	cr2[k] = c2 * (br2 - bi3) - s2 * (bi2 + br3);
	ci2[k] = c2 * (bi2 + br3) + s2 * (br2 - bi3);
	cr3[k] = c6 * (br2 + bi3) - s6 * (bi2 - br3);
	ci3[k] = c6 * (bi2 - br3) + s6 * (br2 + bi3);
	tr = (float) (P7 * (br5 - bi5));
	ti = (float) (P7 * (br5 + bi5));
	cr4[k] = c1 * (br4 + tr) - s1 * (bi4 + ti);
	ci4[k] = c1 * (bi4 + ti) + s1 * (br4 + tr);
	cr5[k] = c5 * (br4 - tr) - s5 * (bi4 - ti);
	ci5[k] = c5 * (bi4 - ti) + s5 * (br4 - tr);
	tr = (float) (-P7 * (br7 + bi7));
	ti = (float) (P7 * (br7 - bi7));
	cr6[k] = c3 * (br6 + tr) - s3 * (bi6 + ti);
	ci6[k] = c3 * (bi6 + ti) + s3 * (br6 + tr);
	cr7[k] = c7 * (br6 - tr) - s7 * (bi6 - ti);
	ci7[k] = c7 * (bi6 - ti) + s7 * (br6 - tr);
      } else {
	cr1[k] = br0 - br1;
	ci1[k] = bi0 - bi1;
	cr2[k] = br2 - bi3;
	ci2[k] = bi2 + br3;
	cr3[k] = br2 + bi3;
	ci3[k] = bi2 - br3;
	tr = (float) (P7 * (br5 - bi5));
	ti = (float) (P7 * (br5 + bi5));
	cr4[k] = br4 + tr;
	ci4[k] = bi4 + ti;
	cr5[k] = br4 - tr;
	ci5[k] = bi4 - ti;
	tr = (float) (-P7 * (br7 + bi7));
	ti = (float) (P7 * (br7 - bi7));
	cr6[k] = br6 + tr;
	ci6[k] = bi6 + ti;
	cr7[k] = br6 - tr;
	ci7[k] = bi6 - ti;
      }
    }
  }
}

void
Snack_PowerSpectrum(float *z)
{
  int l[17];
  int i, in, n8pow, fn;
  int lengt;
  register int ij, ji, nxtlt;
  int i1, i2, i3, i4, i5, i6, i7, i8, i9, i10, i11, i12, i13, i14;
  float r, fi;
  float h1r, h1i, h2r, h2i;
  double wr, wi, wtemp;

  fn = nthpo;
  for (i = 0; i < nthpo; i++) {
    y[i] = -z[(i<<1)+1];
    x[i] = z[i<<1];
  }

  n8pow = n2pow / 3;
  if (n8pow != 0) {
    for (i = 0; i < n8pow; i++) {
      lengt = n2pow - 3 * i;
      nxtlt = Pow2[lengt - 3];
      r8tx (nxtlt, nthpo, lengt,
	    &x[0], &x[nxtlt], &x[2 * nxtlt], &x[3 * nxtlt],
	    &x[4 * nxtlt], &x[5 * nxtlt], &x[6 * nxtlt], &x[7 * nxtlt],
	    &y[0], &y[nxtlt], &y[2 * nxtlt], &y[3 * nxtlt],
	    &y[4 * nxtlt], &y[5 * nxtlt], &y[6 * nxtlt], &y[7 * nxtlt]);
    }
  }

  switch (n2pow - 3 * n8pow) {
  case 0:
    break;
  case 1:
    r2tx (nthpo, &x[0], &x[1], &y[0], &y[1]);
    break;
  case 2:
    r4tx (nthpo, &x[0], &x[1], &x[2], &x[3],
	  &y[0], &y[1], &y[2], &y[3]);
    break;
  default:
    /*    fprintf (stderr, "-- Algorithm Error Snack_DBPowerSpectrum\n");*/
    exit (1);
  }
  for (i = 0; i < 17; i++) {
    if (i < n2pow) {
      l[i] = Pow2[n2pow - i];
    } else {
      l[i] = 1;
    }
  }
  ij = 0;
  for (i1 = 0; i1 < l[14]; i1++) {
    for (i2 = i1; i2 < l[13]; i2 += l[14]) {
      for (i3 = i2; i3 < l[12]; i3 += l[13]) {
	for (i4 = i3; i4 < l[11]; i4 += l[12]) {
	  for (i5 = i4; i5 < l[10]; i5 += l[11]) {
	    for (i6 = i5; i6 < l[9]; i6 += l[10]) {
	      for (i7 = i6; i7 < l[8]; i7 += l[9]) {
		for (i8 = i7; i8 < l[7]; i8 += l[8]) {
		  for (i9 = i8; i9 < l[6]; i9 += l[7]) {
		    for (i10 = i9; i10 < l[5]; i10 += l[6]) {
		      for (i11 = i10; i11 < l[4]; i11 += l[5]) {
			for (i12 = i11; i12 < l[3]; i12 += l[4]) {
			  for (i13 = i12; i13 < l[2]; i13 += l[3]) {
			    for (i14 = i13; i14 < l[1]; i14 += l[2]) {
			      for (ji = i14; ji < l[0]; ji += l[1]) {
				if (ij < ji) {
				  r = x[ij];
				  x[ij] = x[ji];
				  x[ji] = r;
				  fi = y[ij];
				  y[ij] = y[ji];
				  y[ji] = fi;
				}
				ij++;
			      }
			    }
			  }
			}
		      }
		    }
		  }
		}
	      }
	    }
	  }
	}
      }
    }
  }

  wr = 1.0+wpr;
  wi = wpi;
  for (i = 1; i <= (nthpo>>1); i++) {
    in = nthpo-i;
    h1r = (x[i]+x[in]);
    h1i = (y[i]-y[in]);
    h2r = (y[i]+y[in]);
    h2i = (x[in]-x[i]);
    x[in] = (float) (h1r+wr*h2r-wi*h2i);
    y[in] = (float) (h1i+wr*h2i+wi*h2r);
    wtemp =  x[in]*x[in]+y[in]*y[in];
    z[in] = (float) wtemp;
 
    x[i]  = (float) (h1r-wr*h2r+wi*h2i);
    y[i]  = (float) (-h1i+wr*h2i+wi*h2r);
    wtemp = x[i]*x[i]+y[i]*y[i];
    z[i] = (float) wtemp;

    wtemp = wr;
    wr = wr*wpr-wi*wpi+wr;
    wi = wi*wpr+wtemp*wpi+wi;
  }
  wtemp = x[0]-y[0];
  wtemp = wtemp*wtemp;
  z[0] = (float) wtemp;
}
