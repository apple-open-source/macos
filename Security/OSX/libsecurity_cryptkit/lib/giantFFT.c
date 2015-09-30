/* Copyright (c) 1998,2011,2014 Apple Inc.  All Rights Reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************

   giantFFT.c
   Library for large-integer arithmetic via FFT. Currently unused
   in CryptKit.


 Revision History
 ----------------
 19 Jan 1998 at Apple
 	Split off from NSGiantIntegers.c.

*/

/*
 * FIXME - make sure platform-specific math lib has floor(), fmod(),
 *         sin(), pow()
 */
#include <math.h>
#include "NSGiantIntegers.h"

#define AUTO_MUL 	0
#define GRAMMAR_MUL 	1
#define FFT_MUL 	2

#define TWOPI 		(double)(2*3.1415926535897932384626433)
#define SQRT2 		(double)(1.414213562373095048801688724209)
#define SQRTHALF 	(double)(0.707106781186547524400844362104)
#define TWO16 		(double)(65536.0)
#define TWOM16 		(double)(0.0000152587890625)
#define BREAK_SHORTS 	400    // Number of shorts at which FFT breaks over.

static int lpt(int n, int *lambda);
static void mul_hermitian(double *a, double *b, int n) ;
static void square_hermitian(double *b, int n);
static void addsignal(giant x, double *zs, int n);
static void scramble_real(double *x, int n);
static void fft_real_to_hermitian(double *zs, int n);
static void fftinv_hermitian_to_real(double *zs, int n);
static void GiantFFTSquare(giant gx);
static void GiantFFTMul(giant,giant);
static void giant_to_double(giant x, int sizex, double *zs, int L);

static int mulmode = AUTO_MUL;

void mulg(giant a, giant b) { /* b becomes a*b. */
	PROF_START;
	INCR_MULGS;
	GiantAuxMul(a,b);
	#if	FEE_DEBUG
        (void)bitlen(b); // XXX
	#endif	FEE_DEBUG
        PROF_END(mulgTime);
	PROF_INCR(numMulg);
}

static void GiantAuxMul(giant a, giant b) {
/* Optimized general multiply, b becomes a*b. Modes are:
   AUTO_MUL: switch according to empirical speed criteria.
   GRAMMAR_MUL: force grammar-school algorithm.
   FFT_MUL: force floating point FFT method.
*/
    int square = (a==b);

    if (isZero(b)) return;
    if (isZero(a)) {
        gtog(a, b);
        return;
    }
    switch(mulmode) {
    case GRAMMAR_MUL:
        GiantGrammarMul(a,b);
        break;
    case FFT_MUL:
        if (square) {
            GiantFFTSquare(b);
        }
        else {
            GiantFFTMul(a,b);
        }
        break;
    case AUTO_MUL: {
        int sizea, sizeb;
        float grammartime;
        sizea = abs(a->sign);
        sizeb = abs(b->sign);
        grammartime = sizea; grammartime *= sizeb;
        if(grammartime < BREAK_SHORTS*BREAK_SHORTS) {
                GiantGrammarMul(a,b);
        }
        else {
            if (square) GiantFFTSquare(b);
            else GiantFFTMul(a,b);
        }
        break;
      }
   }
}

/***************** Commence FFT multiply routines ****************/

static int CurrentRun = 0;
double *sincos = NULL;
static void init_sincos(int n) {
    int j;
    double e = TWOPI/n;

    if (n <= CurrentRun) return;
    CurrentRun = n;
    if (sincos) free(sincos);
    sincos = (double *)malloc(sizeof(double)*(1+(n>>2)));
    for(j=0;j<=(n>>2);j++) {
        sincos[j] = sin(e*j);
    }
}

static double s_sin(int n) {
    int seg = n/(CurrentRun>>2);

    switch(seg) {
    case 0: return(sincos[n]);
    case 1: return(sincos[(CurrentRun>>1)-n]);
    case 2: return(-sincos[n-(CurrentRun>>1)]);
    case 3:
    default: return(-sincos[CurrentRun-n]);
    }
}

static double s_cos(int n) {
    int quart = (CurrentRun>>2);

    if (n < quart) return(s_sin(n+quart));
    return(-s_sin(n-quart));
}


static int lpt(int n, int *lambda) {
/* returns least power of two greater than n */
    register int i = 1;

    *lambda = 0;
    while(i<n) {
        i<<=1;
        ++(*lambda);
    }
    return(i);
}

static void addsignal(giant x, double *zs, int n) {
   register int j, k, m, car;
   register double f, g;
   /*double  err,  maxerr = 0.0;*/

   for(j=0;j<n;j++) {
   	f = floor(zs[j]+0.5);

	/* err = fabs(zs[j]-f);
	if(err>maxerr) maxerr = err;
	*/

	zs[j] =0;
	k = 0;
	do{
           g = floor(f*TWOM16);
	   zs[j+k] += f-g*TWO16;
	   ++k;
	   f=g;
	} while(f != 0.0);
   }
   car = 0;
   for(j=0;j<n;j++) {
   	m = zs[j]+car;
	x->n[j] = m & 0xffff;
	car = (m>>16);
   }
   if(car) x->n[j] = car;
      else --j;
   while(!(x->n[j])) --j;
   x->sign = j+1;
   if (abs(x->sign) > x->capacity) NSGiantRaise("addsignal overflow");
}

static void GiantFFTSquare(giant gx) {
    int j,size = abs(gx->sign);
    register int L;

    if(size<4) { GiantGrammarMul(gx,gx); return; }
    L = lpt(size+size, &j);
    {
        //was...double doubles[L];
	//is...
	double *doubles = malloc(sizeof(double) * L);
	// end
        giant_to_double(gx, size, doubles, L);
        fft_real_to_hermitian(doubles, L);
        square_hermitian(doubles, L);
        fftinv_hermitian_to_real(doubles, L);
        addsignal(gx, doubles, L);
	// new
	free(doubles);
    }
    gx->sign = abs(gx->sign);
    bitlen(gx); // XXX
    if (abs(gx->sign) > gx->capacity) NSGiantRaise("GiantFFTSquare overflow");
}

static void GiantFFTMul(giant y, giant x) { /* x becomes y*x. */
    int lambda, size, sizex = abs(x->sign), sizey = abs(y->sign);
    int finalsign = gsign(x)*gsign(y);
    register int L;

    if((sizex<=4)||(sizey<=4)) { GiantGrammarMul(y,x); return; }
    size = sizex; if(size<sizey) size=sizey;
    L = lpt(size+size, &lambda);
    {
        //double doubles1[L];
        //double doubles2[L];
       	double *doubles1 = malloc(sizeof(double) * L);
	double *doubles2 = malloc(sizeof(double) * L);

        giant_to_double(x, sizex, doubles1, L);
        giant_to_double(y, sizey, doubles2, L);
        fft_real_to_hermitian(doubles1, L);
        fft_real_to_hermitian(doubles2, L);
        mul_hermitian(doubles2, doubles1, L);
        fftinv_hermitian_to_real(doubles1, L);
        addsignal(x, doubles1, L);

	free(doubles1);
	free(doubles2);
    }
    x->sign = finalsign*abs(x->sign);
    bitlen(x); // XXX
    if (abs(x->sign) > x->capacity) NSGiantRaise("GiantFFTMul overflow");
}

static void scramble_real(double *x, int n) {
    register int i,j,k;
    register double tmp;

    for(i=0,j=0;i<n-1;i++) {
        if(i<j) {
            tmp = x[j];
            x[j]=x[i];
            x[i]=tmp;
        }
        k = n/2;
        while(k<=j) {
            j -= k;
            k>>=1;
        }
        j += k;
    }
}

static void fft_real_to_hermitian(double *zs, int n) {
/* Output is {Re(z^[0]),...,Re(z^[n/2),Im(z^[n/2-1]),...,Im(z^[1]).
   This is a decimation-in-time, split-radix algorithm.
 */
	register double cc1, ss1, cc3, ss3;
	register int is, iD, i0, i1, i2, i3, i4, i5, i6, i7, i8,
		     a, a3, b, b3, nminus = n-1, dil, expand;
	register double *x, e;
	int nn = n>>1;
	double t1, t2, t3, t4, t5, t6;
	register int n2, n4, n8, i, j;

        init_sincos(n);
	expand = CurrentRun/n;
	scramble_real(zs, n);
	x = zs-1;  /* FORTRAN compatibility. */
	is = 1;
	iD = 4;
	do{
	   for(i0=is;i0<=n;i0+=iD) {
		i1 = i0+1;
		e = x[i0];
		x[i0] = e + x[i1];
		x[i1] = e - x[i1];
	   }
	   is = (iD<<1)-1;
	   iD <<= 2;
	} while(is<n);
	n2 = 2;
	while(nn>>=1) {
		n2 <<= 1;
		n4 = n2>>2;
		n8 = n2>>3;
		is = 0;
		iD = n2<<1;
		do {
			for(i=is;i<n;i+=iD) {
				i1 = i+1;
				i2 = i1 + n4;
				i3 = i2 + n4;
				i4 = i3 + n4;
				t1 = x[i4]+x[i3];
				x[i4] -= x[i3];
				x[i3] = x[i1] - t1;
				x[i1] += t1;
				if(n4==1) continue;
				i1 += n8;
				i2 += n8;
				i3 += n8;
				i4 += n8;
				t1 = (x[i3]+x[i4])*SQRTHALF;
				t2 = (x[i3]-x[i4])*SQRTHALF;
				x[i4] = x[i2] - t1;
				x[i3] = -x[i2] - t1;
				x[i2] = x[i1] - t2;
				x[i1] += t2;
			}
			is = (iD<<1) - n2;
			iD <<= 2;
		} while(is<n);
		dil = n/n2;
		a = dil;
		for(j=2;j<=n8;j++) {
		    	a3 = (a+(a<<1))&nminus;
			b = a*expand;
			b3 = a3*expand;
			cc1 = s_cos(b);
			ss1 = s_sin(b);
			cc3 = s_cos(b3);
			ss3 = s_sin(b3);
			a = (a+dil)&nminus;
			is = 0;
			iD = n2<<1;
		        do {
				for(i=is;i<n;i+=iD) {
					i1 = i+j;
					i2 = i1 + n4;
					i3 = i2 + n4;
					i4 = i3 + n4;
					i5 = i + n4 - j + 2;
					i6 = i5 + n4;
					i7 = i6 + n4;
					i8 = i7 + n4;
					t1 = x[i3]*cc1 + x[i7]*ss1;
					t2 = x[i7]*cc1 - x[i3]*ss1;
					t3 = x[i4]*cc3 + x[i8]*ss3;
					t4 = x[i8]*cc3 - x[i4]*ss3;
					t5 = t1 + t3;
					t6 = t2 + t4;
					t3 = t1 - t3;
					t4 = t2 - t4;
					t2 = x[i6] + t6;
					x[i3] = t6 - x[i6];
					x[i8] = t2;
					t2 = x[i2] - t3;
					x[i7] = -x[i2] - t3;
					x[i4] = t2;
					t1 = x[i1] + t5;
					x[i6] = x[i1] - t5;
					x[i1] = t1;
					t1 = x[i5] + t4;
					x[i5] -= t4;
					x[i2] = t1;
				}
			        is = (iD<<1) - n2;
				iD <<= 2;
			} while(is<n);
		}
	}
}

static void fftinv_hermitian_to_real(double *zs, int n) {
/* Input is {Re(z^[0]),...,Re(z^[n/2),Im(z^[n/2-1]),...,Im(z^[1]).
   This is a decimation-in-frequency, split-radix algorithm.
 */
	register double cc1, ss1, cc3, ss3;
	register int is, iD, i0, i1, i2, i3, i4, i5, i6, i7, i8,
		 a, a3, b, b3, nminus = n-1, dil, expand;
	register double *x, e;
	int nn = n>>1;
	double t1, t2, t3, t4, t5;
	int n2, n4, n8, i, j;

        init_sincos(n);
	expand = CurrentRun/n;
	x = zs-1;
	n2 = n<<1;
	while(nn >>= 1) {
		is = 0;
		iD = n2;
		n2 >>= 1;
		n4 = n2>>2;
		n8 = n4>>1;
		do {
			for(i=is;i<n;i+=iD) {
				i1 = i+1;
				i2 = i1 + n4;
				i3 = i2 + n4;
				i4 = i3 + n4;
				t1 = x[i1] - x[i3];
				x[i1] += x[i3];
				x[i2] += x[i2];
				x[i3] = t1 - 2.0*x[i4];
				x[i4] = t1 + 2.0*x[i4];
				if(n4==1) continue;
				i1 += n8;
				i2 += n8;
				i3 += n8;
				i4 += n8;
				t1 = (x[i2]-x[i1])*SQRTHALF;
				t2 = (x[i4]+x[i3])*SQRTHALF;
				x[i1] += x[i2];
				x[i2] = x[i4]-x[i3];
				x[i3] = -2.0*(t2+t1);
				x[i4] = 2.0*(t1-t2);
			}
			is = (iD<<1) - n2;
			iD <<= 2;
		} while(is<n-1);
		dil = n/n2;
		a = dil;
		for(j=2;j<=n8;j++) {
		    	a3 = (a+(a<<1))&nminus;
			b = a*expand;
			b3 = a3*expand;
			cc1 = s_cos(b);
			ss1 = s_sin(b);
			cc3 = s_cos(b3);
			ss3 = s_sin(b3);
			a = (a+dil)&nminus;
			is = 0;
			iD = n2<<1;
			do {
			   for(i=is;i<n;i+=iD) {
				i1 = i+j;
				i2 = i1+n4;
				i3 = i2+n4;
				i4 = i3+n4;
				i5 = i+n4-j+2;
				i6 = i5+n4;
				i7 = i6+n4;
				i8 = i7+n4;
				t1 = x[i1] - x[i6];
				x[i1] += x[i6];
				t2 = x[i5] - x[i2];
				x[i5] += x[i2];
				t3 = x[i8] + x[i3];
				x[i6] = x[i8] - x[i3];
				t4 = x[i4] + x[i7];
				x[i2] = x[i4] - x[i7];
				t5 = t1 - t4;
				t1 += t4;
				t4 = t2 - t3;
				t2 += t3;
				x[i3] = t5*cc1 + t4*ss1;
				x[i7] = -t4*cc1 + t5*ss1;
				x[i4] = t1*cc3 - t2*ss3;
				x[i8] = t2*cc3 + t1*ss3;
			   }
			   is = (iD<<1) - n2;
			   iD <<= 2;
			} while(is<n-1);
		}
	}
	is = 1;
	iD = 4;
	do {
	  for(i0=is;i0<=n;i0+=iD){
		i1 = i0+1;
		e = x[i0];
		x[i0] = e + x[i1];
		x[i1] = e - x[i1];
	  }
	  is = (iD<<1) - 1;
	  iD <<= 2;
	} while(is<n);
	scramble_real(zs, n);
	e = 1/(double)n;
	for(i=0;i<n;i++) zs[i] *= e;
}


static void mul_hermitian(double *a, double *b, int n) {
	register int k, half = n>>1;
	register double aa, bb, am, bm;

	b[0] *= a[0];
	b[half] *= a[half];
	for(k=1;k<half;k++) {
	        aa = a[k]; bb = b[k];
		am = a[n-k]; bm = b[n-k];
		b[k] = aa*bb - am*bm;
		b[n-k] = aa*bm + am*bb;
	}
}

static void square_hermitian(double *b, int n) {
	register int k, half = n>>1;
	register double c, d;

	b[0] *= b[0];
	b[half] *= b[half];
	for(k=1;k<half;k++) {
	        c = b[k]; d = b[n-k];
		b[n-k] = 2.0*c*d;
		b[k] = (c+d)*(c-d);
	}
}

static void giant_to_double(giant x, int sizex, double *zs, int L) {
	register int j;
	for(j=sizex;j<L;j++) zs[j]=0.0;
	for(j=0;j<sizex;j++) {
		 zs[j] = x->n[j];
	}
}
