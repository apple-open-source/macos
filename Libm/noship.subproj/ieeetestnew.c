/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include	"math.h"
#include	"fenv.h"
#include	"float.h"
#include	"stdio.h"
#include	"stdlib.h"
#include	"strings.h"
#include	"time.h"

#if defined(__cplusplus)
#include <cmath>
using namespace std;
#endif

#define AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER
#if (defined(__ppc__) || defined(__ppc64__))
    #define TARGET_CPU_PPC 1
#elif defined (__i386__)
    #define TARGET_CPU_X86 1
#else
#error Unknown architecture
#endif
#include	"bcd.h"

#include	"fp_private.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/uio.h>
#include <unistd.h>

#define num2decl num2dec
#define dec2numl dec2l
#define nextafterd nextafter

#if !defined(WANT_CARBON_MATH)
#if defined(__ppc64__) || defined(__cplusplus)
#define WANT_CARBON_MATH 0
#else
#define WANT_CARBON_MATH 1
#endif
#endif
 
#if defined(__BIG_ENDIAN__)
typedef union
      {
       long double ldbl;
	   struct
	   		{
			double msd;
			double lsd;
			} headtail;
      } doubledouble;

#elif defined(__LITTLE_ENDIAN__)
typedef union
      {
       long double ldbl;
	   struct
	   		{
			double lsd;
			double msd;
			} headtail;
      } doubledouble;

#else
#error Unknown endianness
#endif

#if WANT_CARBON_MATH
static int FpEqual (doubledouble a, doubledouble c);
static int B2D2B (double a, double b, double *c, char pc);
static double bin2dec2bin (double r, double n, const char *op);

double_t annuity(double_t a, double_t b);
double_t compound(double_t a, double_t b);

static
float_t annuityf(float_t a, float_t b) 
{
    return (float_t)annuity(a,b);
}

static
float_t compoundf(float_t a, float_t b)
{
    return (float_t)compound(a,b);
}
#endif

typedef struct {
	hexdouble r;
	hexdouble n;
	hexdouble result;
	uint32_t flags;
	char op[4];
} testVec;

enum {
    kMaxVectorCount = 2048
};

testVec v[kMaxVectorCount];

#define NOTEST 1
#define NOOP   4
#define NOFLAGTESTS   0 

typedef unsigned char Str90 [255];

static int numtests = 0;
static int errorcount = 0;
static int flagerrors = 0;
static int numerrors = 0;
static int enverrors = 0;
static int rnderrors = 0;
static int numtestsTotal = 0;

static int rnds = 0;

static char modes [8];

static Str90 s1, s2, s3, LinBuff;

static char pctype = 'd';

static FILE *OutFile;

long double Str90toldbl (Str90 StrArg1, const char *op);	// returns a long double from a Str90
double Str90todbl (Str90 StrArg1, const char *op);	// returns a double from a Str90
double Str90toflt (Str90 StrArg1, const char *op);	// returns a float from a Str90
double Str90toint (Str90 StrArg1, const char *op);	// returns a short from a Str90
double Str90tolng (Str90 StrArg1, const char *op);	// returns a long from a Str90

static int samebits (const double x1, const double x2);
static void printdbl (const double x1);
static double testfcns (const char *op, double r, double n);
static long double testfcnsl (const char *op, long double r, long double n);
static double testfcnsf (const char *op, float r, float n);
static void testfcn (const char *op, double r, double n,
					const char *flags, double rslt, const char *LinBuf);
static void testfcnl (const char *op, long double r, long double n,
					const char *flags, long double rslt, const char *LinBuf);
static void testfcnf (const char *op, float r, float n,
					const char *flags, float rslt, const char *LinBuf);

#if (defined(__ppc__) || defined(__ppc64__))
#include <signal.h> 
#include <ucontext.h> 
#include <mach/thread_status.h> 

/* The mffs instruction places the contents of the Floating-Point Status 
 * and Control Register (FPSCR) into bits 32-63 of floating-point 
 * register (FPR) FRT. The bits 0-31 of floating-point register FRT 
 * are undefined */
#define      FEGETENVD(x)         ({ __label__ L1, L2; L1: (void)&&L1; \
					asm volatile ("mffs %0" : "=f" (x)); \
                    L2: (void)&&L2; })

/* The mtfsf instruction copies bits 32-63 of the contents of the 
 * floating-point register (FPR) FRB into the Floating-Point 
 * Status and Control Register under the control of the field 
 * mask specified by FLM. */
#define		 FESETENVD(x) ({ __label__ L1, L2; L1: (void)&&L1; \
                    asm volatile("mtfsf 255,%0" : : "f" (x)); \
                    L2: (void)&&L2; })

enum {   
    FE_ENABLE_INEXACT    = 0x00000008,
    FE_ENABLE_DIVBYZERO  = 0x00000010,
    FE_ENABLE_UNDERFLOW  = 0x00000020,
    FE_ENABLE_OVERFLOW   = 0x00000040,
    FE_ENABLE_INVALID    = 0x00000080,
    FE_ENABLE_ALL_EXCEPT = 0x000000F8 
}; 

static
void myHandler(int sig, struct __siginfo *sip, void *vscp)
{     
    char *label;
    ppc_float_state_t  *fs;
    ppc_thread_state_t *ss;
    struct ucontext *scp = (struct ucontext *)vscp;

    fs = &scp->uc_mcontext->fs;
    ss = &scp->uc_mcontext->ss;

    switch (sip->si_code) 
    {
          case FPE_FLTINV: label = "invalid operand"; break;
          case FPE_FLTRES: label = "inexact"; break;
          case FPE_FLTDIV: label = "division-by-zero"; break;
          case FPE_FLTUND: label = "underflow"; break;
          case FPE_FLTOVF: label = "overflow"; break;
          default: label = "???"; break;
    }
    
    printf("SIGFPE taken at %8.8p invokes myHandler, fpscr = %08X, error = '%s'\n",
            sip->si_addr, fs->fpscr, label);

    /* quiets interrupts when this state is restored */
    fs->fpscr &=  ~(FE_ALL_EXCEPT | FE_ENABLE_ALL_EXCEPT);

    /* Advances the PC when this state is restored */
    //ss->srr0 += 4; By *not* advancing the PC we'll retry the op (with interrupts masked)
}

static struct sigaction act; 
static struct sigaction dfl; 

static
void sniffSIGFPE (const char *flags) 
{     
    hexdouble t;     
    FEGETENVD(t.d);         

    /* Enable hardware trapping for unexpected exceptions */
    t.i.lo &= ~(FE_ENABLE_INEXACT);
    
    if (strstr(flags,"z") == 0) t.i.lo |= FE_ENABLE_DIVBYZERO;
    if (strstr(flags,"i") == 0) t.i.lo |= FE_ENABLE_INVALID;
    if (strstr(flags,"o") == 0) t.i.lo |= FE_ENABLE_OVERFLOW;
    
    // enabling UNDERFLOW causes *new* underflows to appear, see mail from 10/31/2003.  
    // if (strstr(flags,"u") == 0) t.i.lo |= FE_ENABLE_UNDERFLOW;    

    
    FESETENVD(t.d);
    
    /* Set handler */
    if (sigaction(SIGFPE, &act, (struct sigaction *)0) != 0) 
    {         
        perror("Failed to set FPE signal handler.");
        exit(-1);
    }
} 

static
void clearSIGFPE ()
{
    hexdouble t;     
    FEGETENVD(t.d);         

    /* Disable hardware trapping for all exceptions */
    t.i.lo &= ~(FE_ENABLE_ALL_EXCEPT);
    FESETENVD(t.d);
    if (sigaction(SIGFPE, &dfl, (struct sigaction *)0) != 0) 
    {         
        perror("Failed to set FPE signal handler.");
        exit(-1);
    }
}
#else
static void sniffSIGFPE (const char *flags) {}
static void clearSIGFPE () {}
#endif

static int samebits (const double x1, const double x2) {
    union {
        double d;
        uint32_t i[2];
    } onion1, onion2;
	
	onion1.d = x1;
	onion2.d = x2;
	return ((onion1.i[0] == onion2.i[0]) && (onion1.i[1] == onion2.i[1]));
}

#if ( __WANT_LONG_DOUBLE_FORMAT__ - 0L == 128L )
static int samebitsl (const long double x1, const long double x2) {
    if (isnan(x1)) 
        return isnan(x2);
    else if (isnan(x2)) 
        return isnan(x1);
    else if (isinf(x1)) 
        return isinf(x2);
    else if (isinf(x2)) 
        return isinf(x1);
    else if (x1 == 0.0L) 
        return (x2 == 0.0L); // ignores -0.0 distinction
    else if (x2 == 0.0L) 
        return (x1 == 0.0L);
    else if (signbit(x1) != signbit(x2)) 
        return 0;
    else if ((x1 - x2) == 0.0L) 
        return 1;
    else
    {
        long double t = fabsl( x2 - x1 ); // non-zero!
        long double u = fabsl( x1 );
        long double v = fabsl( x2 );
        long double w = fmaxl( u, v );

#warning samebitsl stringency        
        return (( w >= scalbnl( t, 106 ) )); // XXX really want 106? really want exponent comparison?
    }
}
#elif ( __WANT_LONG_DOUBLE_FORMAT__ - 0L == 64L )
static int samebitsl (const long double x1, const long double x2) {
    return samebits((const double) x1, (const double) x2);
}
#endif

#ifdef	noprint
#else	/* noprint */
static void printdbl (const double x1) {
    union {
        double d;
        uint32_t i[2];
    } onion1;
	
	onion1.d = x1;
	fprintf ( OutFile,"%8.8x %8.8x   ", (int) onion1.i[0], (int) onion1.i[1]);
}
static void printldbl (const long double x1) {
    union {
        long double d;
        uint32_t i[4];
    } onion1;
	
	onion1.d = x1;
	fprintf ( OutFile,"%8.8x %8.8x %8.8x %8.8x   ", 
              (int) onion1.i[0], (int) onion1.i[1], (int) onion1.i[2], (int) onion1.i[3]);
}
#endif /* noprint */

static void printdblsgl (const double x1) {
    union {
        float f;
        uint32_t i[1];
    } onion11;
	
	onion11.f = (float)x1;
	fprintf ( OutFile,"%8.8x   ", (int) onion11.i[0]);
}

#if WANT_CARBON_MATH

static int FpEqual (doubledouble a, doubledouble c) {
	return (samebits (a.headtail.msd, c.headtail.msd) &&
		    samebits (a.headtail.lsd, c.headtail.lsd));
}

static int B2D2B	(double a, double b, double *c, char pc) {
	short int i, i1;
	decform df;
	decimal dc, dc2;
	int k, match = 1;
	char stest [DECSTROUTLEN];
	short ix, vp;
	
	int j, j1, n, m, rnddirhold;
	doubledouble ahold, aa, cc;
	float fval;
	short int intval;
	long int lngval;
		
		
	switch (pc) {
		case 's':
					fval = (float) a;
					for (i = 9; i <= SIGDIGLEN; i++) {
						df.digits = i;
						df.style = FLOATDECIMAL;
						num2dec (&df, fval, &dc);
	 					dec2str(&df, &dc, stest);		// do the whole round trip
						ix = 0;							// Note: this is a C String!
						str2dec(stest, &ix, &dc2, &vp);
///						if (strcmp ((char*) dc.sig.text, (char*) dc2.sig.text) != 0) {
///							fprintf ( OutFile,"\r %s  %d\r", dc.sig.text, dc.exp);
///							fprintf ( OutFile," %s\r", stest);
///							fprintf ( OutFile," %s  %d\r", dc2.sig.text, dc2.exp);
///						}
						switch (fegetround()) {
							case FE_TONEAREST:
									break;
										
							case FE_UPWARD:		
									k = fesetround (FE_DOWNWARD);
									break;
										
							case FE_DOWNWARD:		
									k = fesetround (FE_UPWARD);
									break;
										
							case FE_TOWARDZERO:
									k = (a < 0) ? fesetround (FE_DOWNWARD) : fesetround (FE_UPWARD);
						}
						*c = dec2f(&dc2);
						if ((a != *c) && ((a == a) || (*c == *c))) {
							match = 0;
							fprintf ( OutFile,"a = %e   c = %e\n", a, *c);
							if (dc.sgn == 1) fprintf ( OutFile,"\n -");
							else fprintf ( OutFile,"\n  ");
							fprintf ( OutFile,"%s  %d\n", dc.sig.text, dc.exp);
							fprintf ( OutFile," %s\n", stest);
							if (dc2.sgn == 1) fprintf ( OutFile," -");
							else fprintf ( OutFile,"  ");
							fprintf ( OutFile,"%s  %d\n", dc2.sig.text, dc2.exp);

							*c = cc.headtail.msd;
							return match;

//							abort ();
							break;
						}
					}
					break;
			
		case 'i':
		case 'l':
		case 'd':
					for (i = 17; i <= SIGDIGLEN; i++) {
						df.digits = i;
						df.style = FLOATDECIMAL;
						switch (pc) {
							case 'i':
										intval = (short int) a;
										num2dec (&df, intval, &dc);
										break;
							case 'l':	
										lngval = (long int) a;
										num2dec (&df, lngval, &dc);
										break;
							
							default:	num2dec (&df, a, &dc);
						}
	 					dec2str(&df, &dc, stest);		// do the whole round trip
						ix = 0;							// Note: this is a C String!
						str2dec(stest, &ix, &dc2, &vp);
						switch (fegetround()) {
							case FE_TONEAREST:
									break;
										
							case FE_UPWARD:		
									k = fesetround (FE_DOWNWARD);
									break;
										
							case FE_DOWNWARD:		
									k = fesetround (FE_UPWARD);
									break;
										
							case FE_TOWARDZERO:
									k = (a < 0) ? fesetround (FE_DOWNWARD) : fesetround (FE_UPWARD);
						}
						switch (pc) {
							case 'i':	//IBM Compiler Bug 0 -> -0.0 for directed rounding
								*c = ((intval = dec2s(&dc2)) == 0) ? 0.0 : intval;
								break;
							case 'l':
								*c = ((lngval = dec2l(&dc2)) == 0) ? 0.0 : lngval;
								break;
							default:	*c = dec2num(&dc2);
						}

                        numtests++;
                        
						if ((a != *c) && ((a == a) || (*c == *c))) {
							match = 0;
							if (dc.sgn == 1) fprintf ( OutFile,"\n -");
							else fprintf ( OutFile,"\n  ");
							fprintf ( OutFile,"%s  %d\n", dc.sig.text, dc.exp);
							fprintf ( OutFile," %s\n", stest);
							if (dc2.sgn == 1) fprintf ( OutFile," -");
							else fprintf ( OutFile,"  ");
							fprintf ( OutFile,"%s  %d\n", dc2.sig.text, dc2.exp);
							break;
						}
					}
					break;
			
//		case 'e':		c->x = a->x;
//					break;
					
		case 'p':	
					aa.headtail.msd = a;
					aa.headtail.lsd = a;
					
                    rnddirhold = fegetround ();
                    (void)fesetround (FE_TOWARDZERO);
					for (j = 1; j <= 54; j++) aa.headtail.lsd /= 2;
                    (void)fesetround (rnddirhold);
				
					ahold.headtail.msd = aa.headtail.msd;
					ahold.headtail.lsd = aa.headtail.lsd;

					for (n = 11; n >= 0; n--) {
						if (n > 10) aa.headtail.lsd = 0.0;
						else {
							aa.headtail.lsd = ahold.headtail.lsd;
					
                            rnddirhold = fegetround ();
                            (void)fesetround (FE_TOWARDZERO);
							for (m = 1; m <= n; m++) aa.headtail.lsd /= 2.0;
								
                            // avoid redundant representation problem
										
							if (n == 0) aa.headtail.lsd *= 1.0625;
                                (void)fesetround (rnddirhold);
						}
						j1 = ((n == 0) || (n > 10)) ? 1 : 0;
						for (j = j1; j <= 1; j++) {
							if (j == 0) aa.headtail.lsd = -aa.headtail.lsd;
							if (aa.headtail.lsd == 0.0) aa.headtail.lsd = 0.0;	// eliminates -0.0 
							if ((2*aa.headtail.lsd) == aa.headtail.lsd) aa.headtail.lsd = 0.0;	// Inf check
							if (aa.headtail.lsd != aa.headtail.lsd) aa.headtail.lsd = 0.0;	// Nan check 
							cc.headtail.msd = aa.headtail.msd;
							cc.headtail.lsd = aa.headtail.lsd;
							if (n <= 3) i1 = 34;
							else if (n <= 6) i1 = 35;
							else if (n <= 10) i1 = 36;
							else i1 = 37;
							for (i = i1; i <= 36; i++) {
								df.digits = i;
								df.style = FLOATDECIMAL;
								num2decl (&df, aa.ldbl, &dc);
								dec2str(&df, &dc, stest);		// do the whole round trip
								ix = 0;							// Note: this is a C String!
								str2dec(stest, &ix, &dc2, &vp);
								switch (fegetround()) {
									case FE_TONEAREST:
											break;
												
									case FE_UPWARD:		
											k = fesetround (FE_DOWNWARD);
											break;
												
									case FE_DOWNWARD:		
											k = fesetround (FE_UPWARD);
											break;
												
									case FE_TOWARDZERO:
											k = (aa.headtail.msd < 0) ? fesetround (FE_DOWNWARD) : fesetround (FE_UPWARD);
								}
								cc.ldbl = dec2numl(&dc2);
								if (cc.headtail.lsd == 0.0) cc.headtail.lsd = 0.0;	// eliminates -0.0 
                                    numtests++;
								if (FpEqual (aa, cc) == 0) {
									match = 0;
#ifdef DEBUG
									fprintf ( OutFile,"FpEqual (aa, cc) = %d\n", FpEqual (aa, cc));
                                    u.p = aa;
                                    fprintf ( OutFile,"%8x %8x %8x %8x aa\n", 
                                        (int) u.i[0], (int) u.i[1], (int) u.i[2], (int) u.i[3]);
                                    u.p = cc;
                                    fprintf ( OutFile,"%8x %8x %8x %8x cc\n", 
                                        (int) u.i[0], (int) u.i[1], (int) u.i[2], (int) u.i[3]);
									if (dc.sgn == 1) fprintf ( OutFile,"\n -");
									else fprintf ( OutFile,"\n  ");
									fprintf ( OutFile,"%s  %d\n", dc.sig.text, dc.exp);
									fprintf ( OutFile," %s\n", stest);
									if (dc2.sgn == 1) fprintf ( OutFile," -");
									else fprintf ( OutFile,"  ");
									fprintf ( OutFile,"%s  %d\n", dc2.sig.text, dc2.exp);
									fprintf ( OutFile,"n = %d, j = %d, i = %d******\n\n", n, j, i);
#endif

									*c = cc.headtail.msd;
									return match;
                                    break;
								}
							}
						}
					}
					aa.headtail.lsd = ahold.headtail.lsd;
					cc.headtail.lsd = aa.headtail.lsd;
					*c = cc.headtail.msd;
					break;
	}
	return match;

}

static double bin2dec2bin (double r, double n, const char *op) {
    double a, c;

	pctype = 's';
	if (strchr (modes, pctype) != NULL) {
		a = Str90toflt (s1, op);
		if (!B2D2B (a, n, &c, pctype)) {
			fprintf ( OutFile,"precision '%c'\n", pctype);
			return c;
		}
	}
	pctype = 'p';
	if (strchr (modes, pctype) != NULL)
		if (!B2D2B (r, n, &c, pctype)) {
			fprintf ( OutFile,"precision '%c'\n", pctype);
			return c;
		}
	pctype = 'd';
	if (strchr (modes, pctype) != NULL) 
		if (!B2D2B (r, n, &c, pctype)) {
			fprintf ( OutFile,"precision '%c'\n", pctype);
			return c;
		}
	pctype = 'i';
	if (strchr (modes, pctype) != NULL) {
		a = Str90toint (s1, op);
		if (!B2D2B (a, n, &c, pctype)) {
			fprintf ( OutFile,"precision '%c'\n", pctype);
			return c;
		}
	}
	pctype = 'l';
	if (strchr (modes, pctype) != NULL) {
		a = Str90tolng (s1, op);
		if (!B2D2B (a, n, &c, pctype)) {
			fprintf ( OutFile,"precision '%c'\n", pctype);
			return c;
		}
	}
	pctype = 'd';
	return r;
}
#endif /* WANT_CARBON_MATH */

static double testfcns (const char *op, double r, double n) 
{
    int k;
    long int l;
    double n2;

		 if (op [1] == '1') return sin (r);
	else if (op [1] == '2') return cos (r);
	else if (op [1] == '3') return tan (r);
	else if (op [1] == '4') return atan (r);
	else if (op [1] == '5') return atan2 (r, n);
	else if (op [1] == '6') return asin (r);
	else if (op [1] == '7') return acos (r);
	else if (op [1] == '8') return log10 (r);
	else if (op [1] == 'A') return fabs (r);
	else if (op [1] == 'B')
						{	
                            n2 = n;
							return modf (r, &n2);
						}
	else if (op [1] == 'C')
						{
							if (r < n) return -1;
							else if (r == n) return 0;
							else if (r > n) return 1;
							else return 2;
						}
	else if (op [1] == 'D') return fdim (r, n);
	else if (op [1] == 'E')
						{	
                            k = (int) n;
							return frexp (r, &k);
						}
	else if (op [1] == 'F')
                        {
                            k = isfinite (r);
                            if (k == 0) return 0.0; else return (double)k;
                        }
	else if (op [1] == 'G') return erf (r);
	else if (op [1] == 'H') return hypot (r, n);
	else if (op [1] == 'I') return rint (r);
	else if (op [1] == 'J') return trunc (r);
	else if (op [1] == 'K') return round (r);
	else if (op [1] == 'L') return logb (r);
	else if (op [1] == 'M') return fmod (r, n);
	else if (op [1] == 'N') return nextafter (r, n);
	else if (op [1] == 'O') return log2 (r);
	else if (op [1] == 'P') return log (r);
	else if (op [1] == 'Q') return log1p (r);
	else if (op [1] == 'R') return exp2 (r);
	else if (op [1] == 'S') return scalbn (r, (int)n);
	else if (op [1] == 'T') return exp (r);
	else if (op [1] == 'U') return expm1 (r);
	else if (op [1] == 'V') return sqrt (r);
	else if (op [1] == 'W') return erfc (r);
	else if (op [1] == 'X') return pow (r, n);
#if WANT_CARBON_MATH
	else if (op [1] == 'Y') return compound (r, n);
	else if (op [1] == 'Z') return annuity (r, n);
#endif
	else if (op [1] == 'a') 
    return ceil (r);
	else if (op [1] == 'b')
						{	
                            n2 = n;
							modf (r, &n2);
							return n2;
						}
	else if (op [1] == 'c') 
                        { 
                            k = fpclassify (r);
                            if (k == FP_SNAN)			return 0;
                            else if (k == FP_QNAN)      return 0;
                            else if (k == FP_INFINITE) 	return 1;
                            else if (k == FP_ZERO) 		return 4;
                            else if (k == FP_NORMAL)    return 2;
                            else if (k == FP_SUBNORMAL)	return 3;
                            
                            else return 99;
                        }
#if WANT_CARBON_MATH
	else if (op [1] == 'd') return bin2dec2bin (r, n, op);
#else
    else if (op [1] == 'd') return r;
#endif
	else if (op [1] == 'e')
						{	
                            k = (int) n;
							frexp (r, &k);
                            if (k == 0) return 0.0; else return (double)k;
						}
	else if (op [1] == 'f') return floor (r);
	else if (op [1] == 'g') return tgamma (r);
	else if (op [1] == 'h') return lgamma (r);
	else if (op [1] == 'i') return nearbyint (r);
	else if (op [1] == 'k')
                        {
                            l = lround (r);
                            if (l == 0) return 0.0; else return (double)l; // Simple "return l;" gets -0.0 !?!
                        }
	else if (op [1] == 'm')
                        {
                            k = signbit (r);
                            if (k == 0) return 0.0; else return (double)k;
                        }
	else if (op [1] == 'n')
                        {
                            k = isnan (r);
                            if (k == 0) return 0.0; else return (double)k;
                        }
	else if (op [1] == 'o')
                        {
                            k = isnormal (r);
                            if (k == 0) return 0.0; else return (double)k;
                        }
	else if (op [1] == 'q') 
                        {
							remquo (r, n, &k);
                            if (k == 0) return 0.0; else return (double)k;
                        }
	else if (op [1] == 'r') return remquo (r, n, &k);
	else if (op [1] == '%') return remainder (r, n);
	else if (op [1] == 's') return ldexp (r, (int)n);
	else if (op [1] == 'u') return acosh (r);
	else if (op [1] == 'v') return asinh (r);
	else if (op [1] == 'w') return atanh (r);
	else if (op [1] == 'x') return cosh (r);
	else if (op [1] == 'y') return sinh (r);
	else if (op [1] == 'z') return tanh (r);
	else if (op [1] == '+') return r + n;
	else if (op [1] == '-') return r - n;
	else if (op [1] == '*') return r*n;
	else if (op [1] == '/') return r/n;
	else if (op [1] == '~') return -r;
	else if (op [1] == '@') return copysign (r, n);
	else if (op [1] == '<') return fmin (r, n);
	else if (op [1] == '>') return fmax (r, n);
	else if (op [1] == '&')
                        {
                            l = lrint (r);
                            if (l == 0) return 0.0; else return (double)l; // Simple "return l;" gets -0.0 !?!
                        }
	else {
#ifdef	noprint
#else	/* noprint */
		fprintf ( OutFile,"ERROR unknown case, abort numtestsTotal = %d '%c'\n", numtestsTotal, op [1]);
		fprintf ( OutFile,"'%s'\n", LinBuff);
#endif /* noprint */
		exit (NOOP);
	}
    /* NOTREACHED */
	return 0;
}

static double testfcnsf (const char *op, float r, float n) 
{
    int k;
    long int l;
    float n2;

		 if (op [1] == '1') return sinf (r);
	else if (op [1] == '2') return cosf (r);
	else if (op [1] == '3') return tanf (r);
	else if (op [1] == '4') return atanf (r);
	else if (op [1] == '5') return atan2f (r, n);
	else if (op [1] == '6') return asinf (r);
	else if (op [1] == '7') return acosf (r);
	else if (op [1] == '8') return log10f (r);
	else if (op [1] == 'A') return fabsf (r);
	else if (op [1] == 'B')
						{	
                            n2 = n;
							return modff (r, &n2);
						}
	else if (op [1] == 'C')
						{
							if (r < n) return -1;
							else if (r == n) return 0;
							else if (r > n) return 1;
							else return 2;
						}
	else if (op [1] == 'D') return fdimf (r, n);
	else if (op [1] == 'E')
						{	
                            k = (int) n;
							return frexpf (r, &k);
						}
	else if (op [1] == 'F')
                        {
                            k = isfinite (r);
                            if (k == 0) return 0.0; else return (double)k;
                        }
	else if (op [1] == 'G') return erff (r);
	else if (op [1] == 'H') return hypotf (r, n);
	else if (op [1] == 'I') return rintf (r);
	else if (op [1] == 'J') return truncf (r);
	else if (op [1] == 'K') return roundf (r);
	else if (op [1] == 'L') return logbf (r);
	else if (op [1] == 'M') return fmodf (r, n);
	else if (op [1] == 'N') return nextafterf (r, n);
	else if (op [1] == 'O') return log2f (r);
	else if (op [1] == 'P') return logf (r);
	else if (op [1] == 'Q') return log1pf (r);
	else if (op [1] == 'R') return exp2f (r);
	else if (op [1] == 'S') return scalbnf (r, (int)n);
	else if (op [1] == 'T') return expf (r);
	else if (op [1] == 'U') return expm1f (r);
	else if (op [1] == 'V') return sqrtf (r);
	else if (op [1] == 'W') return erfcf (r);
	else if (op [1] == 'X') return powf (r, n);
#if WANT_CARBON_MATH
	else if (op [1] == 'Y') return compoundf (r, n);
	else if (op [1] == 'Z') return annuityf (r, n);
#endif
	else if (op [1] == 'a') return ceilf (r);
	else if (op [1] == 'b')
						{	
                            n2 = n;
							modff (r, &n2);
							return n2;
						}
	else if (op [1] == 'c') 
                        { 
                            k = fpclassify (r);
                            if (k == FP_SNAN)           return 0;
                            else if (k == FP_QNAN)      return 0;
                            else if (k == FP_INFINITE) 	return 1;
                            else if (k == FP_ZERO) 		return 4;
                            else if (k == FP_NORMAL)    return 2;
                            else if (k == FP_SUBNORMAL)	return 3;
                            
                            else return 99;
                        }
#if WANT_CARBON_MATH
	else if (op [1] == 'd') return bin2dec2bin (r, n, op);
#else
    else if (op [1] == 'd') return r;
#endif
	else if (op [1] == 'e')
						{	
                            k = (int) n;
							frexpf (r, &k);
                            if (k == 0) return 0.0; else return (double)k;
						}
	else if (op [1] == 'f') return floorf (r);
	else if (op [1] == 'g') return tgammaf (r);
	else if (op [1] == 'h') return lgammaf (r);
	else if (op [1] == 'i') return nearbyintf (r);
	else if (op [1] == 'k') 
                        {
                            l = lroundf (r);
                            if (l == 0) return 0.0; else return (double)l; // Simple "return l;" gets -0.0 !?!
                        }
	else if (op [1] == 'm')
                        {
                            k = signbit (r);
                            if (k == 0) return 0.0; else return (double)k;
                        }
	else if (op [1] == 'n')
                        {
                            k = isnan (r);
                            if (k == 0) return 0.0; else return (double)k;
                        }
	else if (op [1] == 'o')
                        {
                            k = isnormal (r);
                            if (k == 0) return 0.0; else return (double)k;
                        }
	else if (op [1] == 'q') 
                        {
                            remquof (r, n, &k);
                            if (k == 0) return 0.0; else return (double)k;
                        }
	else if (op [1] == 'r') return remquof (r, n, &k);
	else if (op [1] == '%') return remainderf (r, n);
	else if (op [1] == 's') return ldexpf (r, (int)n);
	else if (op [1] == 'u') return acoshf (r);
	else if (op [1] == 'v') return asinhf (r);
	else if (op [1] == 'w') return atanhf (r);
	else if (op [1] == 'x') return coshf (r);
	else if (op [1] == 'y') return sinhf (r);
	else if (op [1] == 'z') return tanhf (r);
	else if (op [1] == '+') return r + n;
	else if (op [1] == '-') return r - n;
	else if (op [1] == '*') return r*n;
	else if (op [1] == '/') return r/n;
	else if (op [1] == '~') return -r;
	else if (op [1] == '@') return copysignf (r, n);
	else if (op [1] == '<') return fminf (r, n);
	else if (op [1] == '>') return fmaxf (r, n);
	else if (op [1] == '&')
                        {
                            l = lrintf (r);
                            if (l == 0) return 0.0; else return (double)l; // Simple "return l;" gets -0.0 !?!
                        }
	else {
#ifdef	noprint
#else	/* noprint */
		fprintf ( OutFile,"ERROR unknown case, abort numtestsTotal = %d '%c'\n", numtestsTotal, op [1]);
		fprintf ( OutFile,"'%s'\n", LinBuff);
#endif /* noprint */
		exit (NOOP);
	}
    /* NOTREACHED */
	return 0.0;
}

#ifdef __WANT_LONG_DOUBLE_FORMAT__
static long double testfcnsl (const char *op, long double r, long double n) 
{
    int k;
    long int l;
    long double n2;

		 if (op [1] == '1') return sinl (r);
	else if (op [1] == '2') return cosl (r);
	else if (op [1] == '3') return tanl (r);
	else if (op [1] == '4') return atanl (r);
	else if (op [1] == '5') return atan2l (r, n);
	else if (op [1] == '6') return asinl (r);
	else if (op [1] == '7') return acosl (r);
	else if (op [1] == '8') return log10l (r);
	else if (op [1] == 'A') return fabsl (r);
	else if (op [1] == 'B')
						{	
                            n2 = n;
							return (long double)modfl (r, &n2);
						}
	else if (op [1] == 'C')
						{
							if (r < n) return -1;
							else if (r == n) return 0;
							else if (r > n) return 1;
							else return 2;
						}
	else if (op [1] == 'D') return fdiml (r, n);
	else if (op [1] == 'E')
						{	
                            k = (int) n;
							return frexpl (r, &k);
						}
	else if (op [1] == 'F')
                        {
                            k = isfinite (r);
                            if (k == 0) return 0.0; else return (long double)k;
                        }
	else if (op [1] == 'G') return erfl (r);
	else if (op [1] == 'H') return hypotl (r, n);
	else if (op [1] == 'I') return rintl (r);
	else if (op [1] == 'J') return truncl (r);
	else if (op [1] == 'K') return roundl (r);
	else if (op [1] == 'L') return logbl (r);
	else if (op [1] == 'M') return fmodl (r, n);
	else if (op [1] == 'N') return nextafterl (r, n);
	else if (op [1] == 'O') return log2l (r);
	else if (op [1] == 'P') return logl (r);
	else if (op [1] == 'Q') return log1pl (r);
	else if (op [1] == 'R') return exp2l (r);
	else if (op [1] == 'S') return scalbnl (r, (int)n);
	else if (op [1] == 'T') return expl (r);
	else if (op [1] == 'U') return expm1l (r);
	else if (op [1] == 'V') return sqrtl (r);
	else if (op [1] == 'W') return erfcl (r);
	else if (op [1] == 'X') return powl (r, n);
#if WANT_CARBON_MATH
	else if (op [1] == 'Y') return compound (r, n);
	else if (op [1] == 'Z') return annuity (r, n);
#endif
	else if (op [1] == 'a') return ceill (r);
	else if (op [1] == 'b')
						{	
                            n2 = n;
							modfl (r, &n2);
							return n2;
						}
	else if (op [1] == 'c') 
                        { 
                            k = fpclassify (r);
                            if (k == FP_SNAN)           return 0;
                            else if (k == FP_QNAN)      return 0;
                            else if (k == FP_INFINITE) 	return 1;
                            else if (k == FP_ZERO) 		return 4;
                            else if (k == FP_NORMAL)    return 2;
                            else if (k == FP_SUBNORMAL)	return 3;
                            
                            else return 99;
                        }
#if WANT_CARBON_MATH
	else if (op [1] == 'd') return bin2dec2bin (r, n, op);
#else
    else if (op [1] == 'd') return r;
#endif
	else if (op [1] == 'e')
						{	
                            k = (int) n;
							frexpl (r, &k);
                            if (k == 0) return 0.0; else return (long double)k;
						}
	else if (op [1] == 'f') return floorl (r);
	else if (op [1] == 'g') return tgammal (r);
	else if (op [1] == 'h') return lgammal (r);
	else if (op [1] == 'i') return nearbyintl (r);
	else if (op [1] == 'k') 
                        {
                            l = lroundl (r);
                            if (l == 0) return 0.0; else return (long double)l; // Simple "return l;" gets -0.0 !?!
                        }
	else if (op [1] == 'm')
                        {
                            k = signbit (r);
                            if (k == 0) return 0.0; else return (long double)k;
                        }
	else if (op [1] == 'n')
                        {
                            k = isnan (r);
                            if (k == 0) return 0.0; else return (long double)k;
                        }
	else if (op [1] == 'o')
                        {
                            k = isnormal (r);
                            if (k == 0) return 0.0; else return (long double)k;
                        }
	else if (op [1] == 'q') 
                        {
                            remquol (r, n, &k);
                            if (k == 0) return 0.0; else return (long double)k;
                        }
	else if (op [1] == 'r') return remquol (r, n, &k); 
	else if (op [1] == '%') return remainderl (r, n);
	else if (op [1] == 's') return ldexpl (r, (int)n);
	else if (op [1] == 'u') return acoshl (r);
	else if (op [1] == 'v') return asinhl (r);
	else if (op [1] == 'w') return atanhl (r);
	else if (op [1] == 'x') return coshl (r);
	else if (op [1] == 'y') return sinhl (r);
	else if (op [1] == 'z') return tanhl (r);
	else if (op [1] == '+') return r + n;
	else if (op [1] == '-') return r - n;
	else if (op [1] == '*') return r*n;
	else if (op [1] == '/') return r/n;
	else if (op [1] == '~') return -r;
	else if (op [1] == '@') return copysignl (r, n);
	else if (op [1] == '<') return fminl (r, n);
	else if (op [1] == '>') return fmaxl (r, n);
	else if (op [1] == '&')
                        {
                            l = lrintl (r);
                            if (l == 0) return 0.0; else return (long double)l; // Simple "return l;" gets -0.0 !?!
                        }
	else {
#ifdef	noprint
#else	/* noprint */
		fprintf ( OutFile,"ERROR unknown case, abort numtestsTotal = %d '%c'\n", numtestsTotal, op [1]);
		fprintf ( OutFile,"'%s'\n", LinBuff);
#endif /* noprint */
		exit (NOOP);
	}
    /* NOTREACHED */
	return 0.0;
}
#endif

extern short int HiTol, LoTol, RSign;		// These are located in buildnum.c
static int Tolerance = 1;					// indicates Tolerances acceptable

static void testfcn (const char *op, double r, double n,
					const char *flags, double rslt, const char *LinBuf) 
{
    int HT, LT;

    double x, x2 = 0.0, y, rhold, nhold;
    int icmp, ncmp, acmp, ecmp = 0, rcmp = 0, ulps = 0;
    char flags2 [8], flags3 [8];
    char *cp = flags2;
    int i = 0, excepts, rnddir = 0;

	rhold = r;
	nhold = n;
	
    if (isnan(rslt)) rslt = NAN; // Use generic NaN
    
	numtests++;
	flags2 [0] = '\0';	// Null out string
        
    feclearexcept (FE_ALL_EXCEPT);

	if (rnds != 0) 
    {
             if ((rnds & 1) != 0) (void)fesetround (FE_DOWNWARD);
		else if ((rnds & 2) != 0) (void)fesetround (FE_TONEAREST);
		else if ((rnds & 4) != 0) (void)fesetround (FE_UPWARD);
		else if ((rnds & 8) != 0) (void)fesetround (FE_TOWARDZERO);
	}
	
    sniffSIGFPE(flags);
    
	x = testfcns (op, r, n);
        
    if (isnan(x)) x = NAN; // Use generic C99 NaN
    
#if defined (__i386__) /* Accomodation for x87 behaviors */
    if (isnan(x) && strstr(flags, "i")) feclearexcept(FE_INEXACT);
    if (!isnan(x) && x != 0.0 && fabs(x) < DBL_MIN && strstr(flags, "u")) feraiseexcept(FE_UNDERFLOW);
    if (!isnan(x) && x != 0.0 && fabs(x) < DBL_MIN && strstr(flags, "x")) feraiseexcept(FE_INEXACT);
    if (!isnan(x) && fabs(x) > DBL_MAX && strstr(flags, "o")) feraiseexcept(FE_OVERFLOW);
    if (!isnan(x) && fabs(x) > DBL_MAX && strstr(flags, "x")) feraiseexcept(FE_INEXACT);
#endif

    
	if (rnds != 0) 
    {
             if ((rnds & 1) != 0) rnddir = (FE_DOWNWARD);
		else if ((rnds & 2) != 0) rnddir = (FE_TONEAREST);
		else if ((rnds & 4) != 0) rnddir = (FE_UPWARD);
		else if ((rnds & 8) != 0) rnddir = (FE_TOWARDZERO);
	}
    else
        rnddir = FE_TONEAREST;
    
    if (rnddir != fegetround())
    {
        fprintf ( OutFile,"Env blunder encountered >>>%s<<<\n", (char const *) LinBuf);
    }
    
	if (rnds != 0) fesetround (FE_TONEAREST);
   
    excepts = fetestexcept(FE_ALL_EXCEPT);

    clearSIGFPE();

    if (excepts & (FE_INVALID)) cp = strcat (flags2, "i");
    if (excepts & (FE_OVERFLOW)) cp = strcat (flags2, "o");
    if (excepts & (FE_UNDERFLOW)) cp = strcat (flags2, "u");
    if (excepts & (FE_INEXACT)) cp = strcat (flags2, "x");
    if (excepts & (FE_DIVBYZERO)) cp = strcat (flags2, "z");
    
	if (strlen (flags2) == 0) cp = strcat (flags2, "OK");
	acmp = !samebits(r, rhold) || !samebits(n, nhold);
	y = x;
	
	if ((pctype == 'i') || (pctype == 'l') || (pctype == 's')) 	//	code for bin2dec2bin
    {
		if (pctype == 'i') 	rslt = Str90toint (s1, op);
		else if (pctype == 'l') 	rslt = Str90tolng (s1, op);
		else if (pctype == 's') 	rslt = Str90toflt (s1, op);
//		pctype = 'd';
	}
	
	while ((y == y) && (rslt == rslt) && (!samebits(y, rslt)) && (abs(ulps) < 99)) 
    {
		if (y > rslt) ulps++;
		else ulps--;
		y = nextafterd (y, rslt);
	}
	if (!strcmp (flags, "xo")) flags = "ox";
	if (!strcmp (flags, "xu")) flags = "ux";
	if ((icmp = (NOFLAGTESTS) ? 0: strcmp (flags, flags2))) flagerrors++;
	
	if (Tolerance) 
    { 
		HT = (RSign != 0) ? LoTol : HiTol;
		LT = (RSign != 0) ? -HiTol : -LoTol;
	}
	else 
    {
		HT = 0;
		LT = 0;
	}
	ncmp = (ulps < LT) || (ulps > HT);
	if ((/*ncmp =*/ (!samebits(x, rslt) && ((ulps == 0) || ncmp)))) numerrors++;
	if (!(ncmp || icmp || NOFLAGTESTS || (rnds == 0))) 
    {
		for (i = 0; i < 4; i++) 
        {

			if (/* XXX (rnds == 0) || XXX */
                            (((rnds & 1) != 0) && (i == 0))
							|| (((rnds & 2) != 0) && (i == 1))
							|| (((rnds & 4) != 0) && (i == 2))
							|| (((rnds & 8) != 0) && (i == 3)))
			{
				if (i == 0) (void)fesetround (FE_DOWNWARD);
				else if (i == 1) (void)fesetround (FE_TONEAREST);
				else if (i == 2) (void)fesetround (FE_UPWARD);
				else if (i == 3) (void)fesetround (FE_TOWARDZERO);
				flags3 [0] = '\0';	// Null out string
				feraiseexcept (FE_INVALID);
				feraiseexcept (FE_OVERFLOW);
				feraiseexcept (FE_INEXACT);
				feraiseexcept (FE_DIVBYZERO);
				feraiseexcept (FE_UNDERFLOW);
			
				x2 = testfcns (op, r, n);
                if (isnan(x2)) x2 = NAN; // Use generic NaN
	
				if (fetestexcept(FE_INVALID)) cp = strcat (flags3, "i");
				if (fetestexcept(FE_OVERFLOW)) cp = strcat (flags3, "o");
				if (fetestexcept(FE_UNDERFLOW)) cp = strcat (flags3, "u");
				if (fetestexcept(FE_INEXACT)) cp = strcat (flags3, "x");
				if (fetestexcept(FE_DIVBYZERO)) cp = strcat (flags3, "z");
				rcmp = !samebits(x, x2);
				if ((ecmp = strcmp ("iouxz", flags3))) 
                {
					enverrors++;
					if (!rcmp) break;
				}
				if (rcmp) 
                {
					rnderrors++;
					break;
				}
			}
		}
		(void)fesetround (FE_TONEAREST);
	}
#if 1
	// ncmp |= (op[0] == '=') && !samebits(x, rslt); //silver == gold but x != silver (rslt is silver in OTVecServer)
	if (ncmp || icmp || acmp || ecmp || rcmp) 
    {
		errorcount++;
    #ifdef	noprint
    #else	/* noprint */
		fprintf ( OutFile,"%s", LinBuf);
		if (op [1] == '1') fprintf ( OutFile,"sin ");
		else if (op [1] == '2') fprintf ( OutFile,"cos ");
		else if (op [1] == '3') fprintf ( OutFile,"tan ");
		else if (op [1] == '4') fprintf ( OutFile,"atan ");
		else if (op [1] == '5') fprintf ( OutFile,"atan2 ");
		else if (op [1] == '6') fprintf ( OutFile,"asin ");
		else if (op [1] == '7') fprintf ( OutFile,"acos ");
		else if (op [1] == '8') fprintf ( OutFile,"log10 ");
		else if (op [1] == 'A') fprintf ( OutFile,"fabs ");
		else if (op [1] == 'B') fprintf ( OutFile,"modf ");
		else if (op [1] == 'C') fprintf ( OutFile,"compare ");
		else if (op [1] == 'D') fprintf ( OutFile,"fdim ");
		else if (op [1] == 'e') fprintf ( OutFile,"frexp ");
		else if (op [1] == 'E') fprintf ( OutFile,"frexp ");
		else if (op [1] == 'F') fprintf ( OutFile,"isfinite ");
		else if (op [1] == 'G') fprintf ( OutFile,"erf ");
		else if (op [1] == 'H') fprintf ( OutFile,"hypot ");
		else if (op [1] == 'I') fprintf ( OutFile,"rint ");
		else if (op [1] == 'J') fprintf ( OutFile,"trunc ");
		else if (op [1] == 'K') fprintf ( OutFile,"round ");
		else if (op [1] == 'L') fprintf ( OutFile,"logb ");
		else if (op [1] == 'M') fprintf ( OutFile,"fmod ");
		else if (op [1] == 'N') fprintf ( OutFile,"nextafterd ");
		else if (op [1] == 'O') fprintf ( OutFile,"log2 ");
		else if (op [1] == 'P') fprintf ( OutFile,"log ");
		else if (op [1] == 'Q') fprintf ( OutFile,"log1p ");
		else if (op [1] == 'R') fprintf ( OutFile,"exp2 ");
		else if (op [1] == 'S') fprintf ( OutFile,"scalb ");
		else if (op [1] == 'T') fprintf ( OutFile,"exp ");
		else if (op [1] == 'U') fprintf ( OutFile,"expm1 ");
		else if (op [1] == 'V') fprintf ( OutFile,"sqrt ");
		else if (op [1] == 'W') fprintf ( OutFile,"erfc ");
		else if (op [1] == 'X') fprintf ( OutFile,"pow ");
		else if (op [1] == 'Y') fprintf ( OutFile,"compound ");
		else if (op [1] == 'Z') fprintf ( OutFile,"annuity ");
		else if (op [1] == 'a') fprintf ( OutFile,"ceil ");
		else if (op [1] == 'b') fprintf ( OutFile,"modf ");
		else if (op [1] == 'c') fprintf ( OutFile,"fpclassify ");
		else if (op [1] == 'd') fprintf ( OutFile,"bin2dec2bin ");
		else if (op [1] == 'f') fprintf ( OutFile,"floor ");
		else if (op [1] == 'g') fprintf ( OutFile,"tgamma ");
		else if (op [1] == 'h') fprintf ( OutFile,"lgamma ");
		else if (op [1] == 'i') fprintf ( OutFile,"nearbyint ");
		else if (op [1] == 'k') fprintf ( OutFile,"lround ");
		else if (op [1] == 'm') fprintf ( OutFile,"signbit ");
		else if (op [1] == 'n') fprintf ( OutFile,"isnan ");
		else if (op [1] == 'o') fprintf ( OutFile,"isnormal ");
		else if (op [1] == 'q') fprintf ( OutFile,"remquo ");
		else if (op [1] == 'r') fprintf ( OutFile,"remquo ");
		else if (op [1] == '%') fprintf ( OutFile,"remainder ");
		else if (op [1] == 's') fprintf ( OutFile,"ldexp ");
		else if (op [1] == 't') fprintf ( OutFile,"transfer ");
		else if (op [1] == 'u') fprintf ( OutFile,"acosh ");
		else if (op [1] == 'v') fprintf ( OutFile,"asinh ");
		else if (op [1] == 'w') fprintf ( OutFile,"atanh ");
		else if (op [1] == 'x') fprintf ( OutFile,"cosh ");
		else if (op [1] == 'y') fprintf ( OutFile,"sinh ");
		else if (op [1] == 'z') fprintf ( OutFile,"tanh ");
		else if (op [1] == '+') fprintf ( OutFile,"Add ");
		else if (op [1] == '-') fprintf ( OutFile,"Sub ");
		else if (op [1] == '*') fprintf ( OutFile,"Mult ");
		else if (op [1] == '/') fprintf ( OutFile,"Div ");
		else if (op [1] == '~') fprintf ( OutFile,"neg ");
		else if (op [1] == '@') fprintf ( OutFile,"copysign ");
		else if (op [1] == '>') fprintf ( OutFile,"fmax ");
		else if (op [1] == '<') fprintf ( OutFile,"fmin ");
		else if (op [1] == '&') fprintf ( OutFile,"lrint ");
		else fprintf ( OutFile,"??? ");
		fprintf ( OutFile," r = ");
		printdbl (r);
		fprintf ( OutFile,"%7.2e", r);
		fprintf ( OutFile,"    n = ");
		printdbl (n);
		fprintf ( OutFile,"%7.2e\n", n);
		fprintf ( OutFile,"expected     ");
		printdbl (rslt);
		fprintf ( OutFile,"%7.2e    %s\n", rslt, flags);
		fprintf ( OutFile,"computed     ");
		printdbl (x);
		fprintf ( OutFile,"%7.2e    %s", x, flags2);
		if (ncmp) fprintf ( OutFile,"   NUM ERROR %d,", ulps);
		if (icmp) fprintf ( OutFile,"   FLAG ERROR");
		if (ecmp) fprintf ( OutFile,"   ENVRM ERROR");
		if (rcmp) fprintf ( OutFile,"   ROUND ERROR");
		if (acmp) 
        {
			fprintf ( OutFile,"   Argument corrupted ERROR");
			exit (5);
		}
        if (ecmp) 
        {
            fprintf ( OutFile,"\n   environment corrupted ERROR\n");
            fprintf ( OutFile," expected iouxz\n"); 
            fprintf ( OutFile," computed %s", flags3);
        }
        if (rcmp) 
        {
            fprintf ( OutFile,"\ncomputed     ");
            printdbl (x2);
            fprintf ( OutFile,"%7.2e%3d  rounding problem! ", x2, i);
            switch (i) {
                case 0	: fprintf ( OutFile," FE_DOWNWARD\n"); break;
                case 1	: fprintf ( OutFile," FE_TONEAREST\n"); break;
                case 2	: fprintf ( OutFile," FE_UPWARD\n"); break;
                case 3	: fprintf ( OutFile," FE_TOWARDZERO\n"); break;
            }
        }
		fprintf ( OutFile,"\n\n");
    #endif /* noprint */
	}
#else /* Following used to generate cases for vMathLib test harness. */
{
		fprintf ( OutFile,"// %s", LinBuf);
		fprintf ( OutFile,"    \"");
			 if (op [1] == '1') fprintf ( OutFile,"sin ");
		else if (op [1] == '2') fprintf ( OutFile,"cos ");
		else if (op [1] == '3') fprintf ( OutFile,"tan ");
		else if (op [1] == '4') fprintf ( OutFile,"atan ");
		else if (op [1] == '5') fprintf ( OutFile,"atan2 ");
		else if (op [1] == '6') fprintf ( OutFile,"asin ");
		else if (op [1] == '7') fprintf ( OutFile,"acos ");
		else if (op [1] == '8') fprintf ( OutFile,"log10 ");
		else if (op [1] == 'A') fprintf ( OutFile,"fabs ");
		else if (op [1] == 'B') fprintf ( OutFile,"modf ");
		else if (op [1] == 'C') fprintf ( OutFile,"compare ");
		else if (op [1] == 'D') fprintf ( OutFile,"fdim ");
		else if (op [1] == 'e') fprintf ( OutFile,"frexp ");
		else if (op [1] == 'E') fprintf ( OutFile,"frexp ");
		else if (op [1] == 'F') fprintf ( OutFile,"isfinite ");
		else if (op [1] == 'G') fprintf ( OutFile,"erf ");
		else if (op [1] == 'H') fprintf ( OutFile,"hypot ");
		else if (op [1] == 'I') fprintf ( OutFile,"rint ");
		else if (op [1] == 'J') fprintf ( OutFile,"trunc ");
		else if (op [1] == 'K') fprintf ( OutFile,"round ");
		else if (op [1] == 'L') fprintf ( OutFile,"logb ");
		else if (op [1] == 'M') fprintf ( OutFile,"fmod ");
		else if (op [1] == 'N') fprintf ( OutFile,"nextafterd ");
		else if (op [1] == 'O') fprintf ( OutFile,"log2 ");
		else if (op [1] == 'P') fprintf ( OutFile,"log ");
		else if (op [1] == 'Q') fprintf ( OutFile,"log1p ");
		else if (op [1] == 'R') fprintf ( OutFile,"exp2 ");
		else if (op [1] == 'S') fprintf ( OutFile,"scalb ");
		else if (op [1] == 'T') fprintf ( OutFile,"exp ");
		else if (op [1] == 'U') fprintf ( OutFile,"expm1 ");
		else if (op [1] == 'V') fprintf ( OutFile,"sqrt ");
		else if (op [1] == 'W') fprintf ( OutFile,"erfc ");
		else if (op [1] == 'X') fprintf ( OutFile,"pow ");
		else if (op [1] == 'Y') fprintf ( OutFile,"compound ");
		else if (op [1] == 'Z') fprintf ( OutFile,"annuity ");
		else if (op [1] == 'a') fprintf ( OutFile,"ceil ");
		else if (op [1] == 'b') fprintf ( OutFile,"modf ");
		else if (op [1] == 'c') fprintf ( OutFile,"fpclassify ");
		else if (op [1] == 'd') fprintf ( OutFile,"bin2dec2bin ");
		else if (op [1] == 'f') fprintf ( OutFile,"floor ");
		else if (op [1] == 'g') fprintf ( OutFile,"tgamma ");
		else if (op [1] == 'h') fprintf ( OutFile,"lgamma ");
		else if (op [1] == 'i') fprintf ( OutFile,"nearbyint ");
		else if (op [1] == 'k') fprintf ( OutFile,"lround ");
		else if (op [1] == 'm') fprintf ( OutFile,"signbit ");
		else if (op [1] == 'n') fprintf ( OutFile,"isnan ");
		else if (op [1] == 'o') fprintf ( OutFile,"isnormal ");
		else if (op [1] == 'q') fprintf ( OutFile,"remquo ");
		else if (op [1] == 'r') fprintf ( OutFile,"remquo ");
		else if (op [1] == '%') fprintf ( OutFile,"remainder ");
		else if (op [1] == 's') fprintf ( OutFile,"ldexp ");
		else if (op [1] == 't') fprintf ( OutFile,"transfer ");
		else if (op [1] == 'u') fprintf ( OutFile,"acosh ");
		else if (op [1] == 'v') fprintf ( OutFile,"asinh ");
		else if (op [1] == 'w') fprintf ( OutFile,"atanh ");
		else if (op [1] == 'x') fprintf ( OutFile,"cosh ");
		else if (op [1] == 'y') fprintf ( OutFile,"sinh ");
		else if (op [1] == 'z') fprintf ( OutFile,"tanh ");
		else if (op [1] == '+') fprintf ( OutFile,"+");
		else if (op [1] == '-') fprintf ( OutFile,"-");
		else if (op [1] == '*') fprintf ( OutFile,"*");
		else if (op [1] == '/') fprintf ( OutFile,"/");
		else if (op [1] == '~') fprintf ( OutFile,"neg ");
		else if (op [1] == '@') fprintf ( OutFile,"copysign ");
		else if (op [1] == '>') fprintf ( OutFile,"fmax ");
		else if (op [1] == '<') fprintf ( OutFile,"fmin ");
		else if (op [1] == '&') fprintf ( OutFile,"lrint ");
		else fprintf ( OutFile,"??? ");
		fprintf ( OutFile,"\",    0x");

		printdblsgl (r);
		fprintf ( OutFile,",    0x");
		printdblsgl (n);
		fprintf ( OutFile,",    0x");
		printdblsgl (rslt);

		fprintf ( OutFile,",\n");
	}
#endif
}

#if ( __WANT_LONG_DOUBLE_FORMAT__ - 0L == 128L )
static void testfcnl (const char *op, long double r, long double n,
					const char *flags, long double rslt, const char *LinBuf) 
{
    int HT, LT;

    long double x, x2 = 0.0, y, rhold, nhold;
    doubledouble Y, RSLT;
    int icmp, ncmp, acmp, ecmp = 0, rcmp = 0, ulps = 0;
    char flags2 [8], flags3 [8];
    char *cp = flags2;
    int i = 0, excepts, rnddir = 0;

	rhold = r;
	nhold = n;
	
    if (isnan(rslt)) rslt = NAN; // Use generic NaN
    
	numtests++;
	flags2 [0] = '\0';	// Null out string
        
    feclearexcept (FE_ALL_EXCEPT);

	if (rnds != 0) 
    {
             if ((rnds & 1) != 0) (void)fesetround (FE_DOWNWARD);
		else if ((rnds & 2) != 0) (void)fesetround (FE_TONEAREST);
		else if ((rnds & 4) != 0) (void)fesetround (FE_UPWARD);
		else if ((rnds & 8) != 0) (void)fesetround (FE_TOWARDZERO);
	}
	
    // sniffSIGFPE(flags);
    
	x = testfcnsl (op, r, n);
        
    if (isnan(x)) x = NAN; // Use generic C99 NaN
    
#if defined (__i386__) /* Accomodation for x87 behaviors */
    if (isnan(x) && strstr(flags, "i")) feclearexcept(FE_INEXACT);
    if (!isnan(x) && x != 0.0 && fabs(x) < DBL_MIN && strstr(flags, "u")) feraiseexcept(FE_UNDERFLOW);
    if (!isnan(x) && x != 0.0 && fabs(x) < DBL_MIN && strstr(flags, "x")) feraiseexcept(FE_INEXACT);
    if (!isnan(x) && fabs(x) > DBL_MAX && strstr(flags, "o")) feraiseexcept(FE_OVERFLOW);
    if (!isnan(x) && fabs(x) > DBL_MAX && strstr(flags, "x")) feraiseexcept(FE_INEXACT);
#endif

	if (rnds != 0) 
    {
             if ((rnds & 1) != 0) rnddir = (FE_DOWNWARD);
		else if ((rnds & 2) != 0) rnddir = (FE_TONEAREST);
		else if ((rnds & 4) != 0) rnddir = (FE_UPWARD);
		else if ((rnds & 8) != 0) rnddir = (FE_TOWARDZERO);
	}
    else
        rnddir = FE_TONEAREST;
    
    if (rnddir != fegetround())
    {
        fprintf ( OutFile,"Env blunder encountered >>>%s<<<\n", (char const *) LinBuf);
    }
	if (rnds != 0) fesetround (FE_TONEAREST);
   
    excepts = fetestexcept(FE_ALL_EXCEPT);

    // clearSIGFPE();

    if (excepts & (FE_INVALID)) cp = strcat (flags2, "i");
    if (excepts & (FE_OVERFLOW)) cp = strcat (flags2, "o");
    if (excepts & (FE_UNDERFLOW)) cp = strcat (flags2, "u");
    if (excepts & (FE_INEXACT)) cp = strcat (flags2, "x");
    if (excepts & (FE_DIVBYZERO)) cp = strcat (flags2, "z");
    
	if (strlen (flags2) == 0) cp = strcat (flags2, "OK");
	acmp = !samebitsl(r, rhold) || !samebitsl(n, nhold);
    
	y = x;
	
	if ((pctype == 'i') || (pctype == 'l') || (pctype == 's')) 	//	code for bin2dec2bin
    {
		if (pctype == 'i') 	rslt = Str90toint (s1, op);
		else if (pctype == 'l') 	rslt = Str90tolng (s1, op);
		else if (pctype == 's') 	rslt = Str90toflt (s1, op);
//		pctype = 'd';
	}
	
    Y.ldbl = y;
    RSLT.ldbl = rslt;
        
	while ((Y.headtail.lsd == Y.headtail.lsd) && 
           (RSLT.headtail.lsd == RSLT.headtail.lsd) && 
           (!samebitsl(Y.ldbl, RSLT.ldbl)) && (abs(ulps) < 99)) 
    {
		if (Y.headtail.lsd > RSLT.headtail.lsd) ulps++;
		else ulps--;
		Y.headtail.lsd = nextafterd (Y.headtail.lsd, RSLT.headtail.lsd);
	}
	if (!strcmp (flags, "xo")) flags = "ox";
	if (!strcmp (flags, "xu")) flags = "ux";
#if 0
	if ((icmp = (NOFLAGTESTS) ? 0: strcmp (flags, flags2))) flagerrors++;
#else
    icmp = 0; // Forced NOFLAGTESTS for long double
#endif
	
	if (Tolerance) 
    { 
		HT = (RSign != 0) ? LoTol : HiTol;
		LT = (RSign != 0) ? -HiTol : -LoTol;
	}
	else 
    {
		HT = 0;
		LT = 0;
	}
	ncmp = (ulps < LT) || (ulps > HT);
	if ((/*ncmp =*/ (!samebitsl(x, rslt) && ((ulps == 0) || ncmp)))) numerrors++;
    
	// ncmp |= (op[0] == '=') && !samebitsl(x, rslt); //silver == gold but x != silver (rslt is silver in OTVecServer)
	if (ncmp || icmp || acmp || ecmp || rcmp) 
    {
		errorcount++;
    #ifdef	noprint
    #else	/* noprint */
		fprintf ( OutFile,"%s", LinBuf);
		if (op [1] == '1') fprintf ( OutFile,"sinl ");
		else if (op [1] == '2') fprintf ( OutFile,"cosl ");
		else if (op [1] == '3') fprintf ( OutFile,"tanl ");
		else if (op [1] == '4') fprintf ( OutFile,"atanl ");
		else if (op [1] == '5') fprintf ( OutFile,"atan2l ");
		else if (op [1] == '6') fprintf ( OutFile,"asinl ");
		else if (op [1] == '7') fprintf ( OutFile,"acosl ");
		else if (op [1] == '8') fprintf ( OutFile,"log10l ");
		else if (op [1] == 'A') fprintf ( OutFile,"fabsl ");
		else if (op [1] == 'B') fprintf ( OutFile,"modfl ");
		else if (op [1] == 'C') fprintf ( OutFile,"comparel ");
		else if (op [1] == 'D') fprintf ( OutFile,"fdiml ");
		else if (op [1] == 'e') fprintf ( OutFile,"frexpl ");
		else if (op [1] == 'E') fprintf ( OutFile,"frexpl ");
		else if (op [1] == 'F') fprintf ( OutFile,"isfinitel ");
		else if (op [1] == 'G') fprintf ( OutFile,"erfl ");
		else if (op [1] == 'H') fprintf ( OutFile,"hypotl ");
		else if (op [1] == 'I') fprintf ( OutFile,"rintl ");
		else if (op [1] == 'J') fprintf ( OutFile,"truncl ");
		else if (op [1] == 'K') fprintf ( OutFile,"roundl ");
		else if (op [1] == 'L') fprintf ( OutFile,"logbl ");
		else if (op [1] == 'M') fprintf ( OutFile,"fmodl ");
		else if (op [1] == 'N') fprintf ( OutFile,"nextafterl ");
		else if (op [1] == 'O') fprintf ( OutFile,"log2l ");
		else if (op [1] == 'P') fprintf ( OutFile,"logl ");
		else if (op [1] == 'Q') fprintf ( OutFile,"log1pl ");
		else if (op [1] == 'R') fprintf ( OutFile,"exp2l ");
		else if (op [1] == 'S') fprintf ( OutFile,"scalbl ");
		else if (op [1] == 'T') fprintf ( OutFile,"expl ");
		else if (op [1] == 'U') fprintf ( OutFile,"expm1l ");
		else if (op [1] == 'V') fprintf ( OutFile,"sqrtl ");
		else if (op [1] == 'W') fprintf ( OutFile,"erfcl ");
		else if (op [1] == 'X') fprintf ( OutFile,"powl ");
		else if (op [1] == 'Y') fprintf ( OutFile,"compoundl ");
		else if (op [1] == 'Z') fprintf ( OutFile,"annuityl ");
		else if (op [1] == 'a') fprintf ( OutFile,"ceill ");
		else if (op [1] == 'b') fprintf ( OutFile,"modfl ");
		else if (op [1] == 'c') fprintf ( OutFile,"fpclassifyl ");
		else if (op [1] == 'd') fprintf ( OutFile,"bin2dec2binl ");
		else if (op [1] == 'f') fprintf ( OutFile,"floorl ");
		else if (op [1] == 'g') fprintf ( OutFile,"tgammal ");
		else if (op [1] == 'h') fprintf ( OutFile,"lgammal ");
		else if (op [1] == 'i') fprintf ( OutFile,"nearbyintl ");
		else if (op [1] == 'k') fprintf ( OutFile,"lroundl ");
		else if (op [1] == 'm') fprintf ( OutFile,"signbitl ");
		else if (op [1] == 'n') fprintf ( OutFile,"isnanl ");
		else if (op [1] == 'o') fprintf ( OutFile,"isnormall ");
		else if (op [1] == 'q') fprintf ( OutFile,"remquol ");
		else if (op [1] == 'r') fprintf ( OutFile,"remquol ");
		else if (op [1] == '%') fprintf ( OutFile,"remainderl ");
		else if (op [1] == 's') fprintf ( OutFile,"ldexpl ");
		else if (op [1] == 't') fprintf ( OutFile,"transferl ");
		else if (op [1] == 'u') fprintf ( OutFile,"acoshl ");
		else if (op [1] == 'v') fprintf ( OutFile,"asinhl ");
		else if (op [1] == 'w') fprintf ( OutFile,"atanhl ");
		else if (op [1] == 'x') fprintf ( OutFile,"coshl ");
		else if (op [1] == 'y') fprintf ( OutFile,"sinhl ");
		else if (op [1] == 'z') fprintf ( OutFile,"tanhl ");
		else if (op [1] == '+') fprintf ( OutFile,"Addl ");
		else if (op [1] == '-') fprintf ( OutFile,"Subl ");
		else if (op [1] == '*') fprintf ( OutFile,"Multl ");
		else if (op [1] == '/') fprintf ( OutFile,"Divl ");
		else if (op [1] == '~') fprintf ( OutFile,"negl ");
		else if (op [1] == '@') fprintf ( OutFile,"copysignl ");
		else if (op [1] == '>') fprintf ( OutFile,"fmaxl ");
		else if (op [1] == '<') fprintf ( OutFile,"fminl ");
		else if (op [1] == '&') fprintf ( OutFile,"lrintl ");
		else fprintf ( OutFile,"??? ");
		fprintf ( OutFile," r = ");
		printldbl (r);
		fprintf ( OutFile,"%7.2e", (double)r);
		fprintf ( OutFile,"    n = ");
		printldbl (n);
		fprintf ( OutFile,"%7.2e\n", (double)n);
		fprintf ( OutFile,"expected     ");
		printldbl (rslt);
		fprintf ( OutFile,"%7.2e    %s\n", (double)rslt, flags);
		fprintf ( OutFile,"computed     ");
		printldbl (x);
		fprintf ( OutFile,"%7.2e    %s", (double)x, flags2);
		if (ncmp) fprintf ( OutFile,"   NUM ERROR %d,", ulps);
		if (icmp) fprintf ( OutFile,"   FLAG ERROR");
		if (ecmp) fprintf ( OutFile,"   ENVRM ERROR");
		if (rcmp) fprintf ( OutFile,"   ROUND ERROR");
		if (acmp) 
        {
			fprintf ( OutFile,"   Argument corrupted ERROR");
			exit (5);
		}
        if (ecmp) 
        {
            fprintf ( OutFile,"\n   environment corrupted ERROR\n");
            fprintf ( OutFile," expected iouxz\n"); 
            fprintf ( OutFile," computed %s", flags3);
        }
        if (rcmp) 
        {
            fprintf ( OutFile,"\ncomputed     ");
            printldbl (x2);
            fprintf ( OutFile,"%7.2e%3d  rounding problem! ", (double)x2, i);
            switch (i) {
                case 0	: fprintf ( OutFile," FE_DOWNWARD\n"); break;
                case 1	: fprintf ( OutFile," FE_TONEAREST\n"); break;
                case 2	: fprintf ( OutFile," FE_UPWARD\n"); break;
                case 3	: fprintf ( OutFile," FE_TOWARDZERO\n"); break;
            }
        }
		fprintf ( OutFile,"\n\n");
    #endif /* noprint */
	}
}
#elif ( __WANT_LONG_DOUBLE_FORMAT__ - 0L == 64L )
static void testfcnl (const char *op, long double r, long double n,
					const char *flags, long double rslt, const char *LinBuf) 
{
    int HT, LT;

    long double x, x2 = 0.0, y, rhold, nhold;
    int icmp, ncmp, acmp, ecmp = 0, rcmp = 0, ulps = 0;
    char flags2 [8], flags3 [8];
    char *cp = flags2;
    int i = 0, excepts, rnddir = 0;

	rhold = r;
	nhold = n;
	
    if (isnan(rslt)) rslt = NAN; // Use generic NaN
    
	numtests++;
	flags2 [0] = '\0';	// Null out string
        
    feclearexcept (FE_ALL_EXCEPT);

	if (rnds != 0) 
    {
             if ((rnds & 1) != 0) (void)fesetround (FE_DOWNWARD);
		else if ((rnds & 2) != 0) (void)fesetround (FE_TONEAREST);
		else if ((rnds & 4) != 0) (void)fesetround (FE_UPWARD);
		else if ((rnds & 8) != 0) (void)fesetround (FE_TOWARDZERO);
	}
	
    // sniffSIGFPE(flags);
    
	x = testfcnsl (op, r, n);
        
    if (isnan(x)) x = NAN; // Use generic C99 NaN
    
#if defined (__i386__) /* Accomodation for x87 behaviors */
    if (isnan(x) && strstr(flags, "i")) feclearexcept(FE_INEXACT);
    if (!isnan(x) && x != 0.0 && fabs(x) < DBL_MIN && strstr(flags, "u")) feraiseexcept(FE_UNDERFLOW);
    if (!isnan(x) && x != 0.0 && fabs(x) < DBL_MIN && strstr(flags, "x")) feraiseexcept(FE_INEXACT);
    if (!isnan(x) && fabs(x) > DBL_MAX && strstr(flags, "o")) feraiseexcept(FE_OVERFLOW);
    if (!isnan(x) && fabs(x) > DBL_MAX && strstr(flags, "x")) feraiseexcept(FE_INEXACT);
#endif

	if (rnds != 0) 
    {
             if ((rnds & 1) != 0) rnddir = (FE_DOWNWARD);
		else if ((rnds & 2) != 0) rnddir = (FE_TONEAREST);
		else if ((rnds & 4) != 0) rnddir = (FE_UPWARD);
		else if ((rnds & 8) != 0) rnddir = (FE_TOWARDZERO);
	}
    else
        rnddir = FE_TONEAREST;
    
    if (rnddir != fegetround())
    {
        fprintf ( OutFile,"Env blunder encountered >>>%s<<<\n", (char const *) LinBuf);
    }
	if (rnds != 0) fesetround (FE_TONEAREST);
   
    excepts = fetestexcept(FE_ALL_EXCEPT);

    // clearSIGFPE();

    if (excepts & (FE_INVALID)) cp = strcat (flags2, "i");
    if (excepts & (FE_OVERFLOW)) cp = strcat (flags2, "o");
    if (excepts & (FE_UNDERFLOW)) cp = strcat (flags2, "u");
    if (excepts & (FE_INEXACT)) cp = strcat (flags2, "x");
    if (excepts & (FE_DIVBYZERO)) cp = strcat (flags2, "z");
    
	if (strlen (flags2) == 0) cp = strcat (flags2, "OK");
	acmp = !samebitsl(r, rhold) || !samebitsl(n, nhold);
    
	y = x;
	
	if ((pctype == 'i') || (pctype == 'l') || (pctype == 's')) 	//	code for bin2dec2bin
    {
		if (pctype == 'i') 	rslt = Str90toint (s1, op);
		else if (pctype == 'l') 	rslt = Str90tolng (s1, op);
		else if (pctype == 's') 	rslt = Str90toflt (s1, op);
//		pctype = 'd';
	}
	        
	while ((y == y) && (rslt == rslt) && (!samebitsl(y, rslt)) && (abs(ulps) < 99)) 
    {
		if (y > rslt) ulps++;
		else ulps--;
		y = nextafterd ((double)y, (double)rslt);
	}
	if (!strcmp (flags, "xo")) flags = "ox";
	if (!strcmp (flags, "xu")) flags = "ux";
#if 0
	if ((icmp = (NOFLAGTESTS) ? 0: strcmp (flags, flags2))) flagerrors++;
#else
    icmp = 0; // Forced NOFLAGTESTS for long double
#endif
	
	if (Tolerance) 
    { 
		HT = (RSign != 0) ? LoTol : HiTol;
		LT = (RSign != 0) ? -HiTol : -LoTol;
	}
	else 
    {
		HT = 0;
		LT = 0;
	}
	ncmp = (ulps < LT) || (ulps > HT);
	if ((/*ncmp =*/ (!samebits((double)x, (double)rslt) && ((ulps == 0) || ncmp)))) numerrors++;
    
	// ncmp |= (op[0] == '=') && !samebitsl(x, rslt); //silver == gold but x != silver (rslt is silver in OTVecServer)
	if (ncmp || icmp || acmp || ecmp || rcmp) 
    {
		errorcount++;
    #ifdef	noprint
    #else	/* noprint */
		fprintf ( OutFile,"%s", LinBuf);
		if (op [1] == '1') fprintf ( OutFile,"sinl ");
		else if (op [1] == '2') fprintf ( OutFile,"cosl ");
		else if (op [1] == '3') fprintf ( OutFile,"tanl ");
		else if (op [1] == '4') fprintf ( OutFile,"atanl ");
		else if (op [1] == '5') fprintf ( OutFile,"atan2l ");
		else if (op [1] == '6') fprintf ( OutFile,"asinl ");
		else if (op [1] == '7') fprintf ( OutFile,"acosl ");
		else if (op [1] == '8') fprintf ( OutFile,"log10l ");
		else if (op [1] == 'A') fprintf ( OutFile,"fabsl ");
		else if (op [1] == 'B') fprintf ( OutFile,"modfl ");
		else if (op [1] == 'C') fprintf ( OutFile,"comparel ");
		else if (op [1] == 'D') fprintf ( OutFile,"fdiml ");
		else if (op [1] == 'e') fprintf ( OutFile,"frexpl ");
		else if (op [1] == 'E') fprintf ( OutFile,"frexpl ");
		else if (op [1] == 'F') fprintf ( OutFile,"isfinitel ");
		else if (op [1] == 'G') fprintf ( OutFile,"erfl ");
		else if (op [1] == 'H') fprintf ( OutFile,"hypotl ");
		else if (op [1] == 'I') fprintf ( OutFile,"rintl ");
		else if (op [1] == 'J') fprintf ( OutFile,"truncl ");
		else if (op [1] == 'K') fprintf ( OutFile,"roundl ");
		else if (op [1] == 'L') fprintf ( OutFile,"logbl ");
		else if (op [1] == 'M') fprintf ( OutFile,"fmodl ");
		else if (op [1] == 'N') fprintf ( OutFile,"nextafterl ");
		else if (op [1] == 'O') fprintf ( OutFile,"log2l ");
		else if (op [1] == 'P') fprintf ( OutFile,"logl ");
		else if (op [1] == 'Q') fprintf ( OutFile,"log1pl ");
		else if (op [1] == 'R') fprintf ( OutFile,"exp2l ");
		else if (op [1] == 'S') fprintf ( OutFile,"scalbl ");
		else if (op [1] == 'T') fprintf ( OutFile,"expl ");
		else if (op [1] == 'U') fprintf ( OutFile,"expm1l ");
		else if (op [1] == 'V') fprintf ( OutFile,"sqrtl ");
		else if (op [1] == 'W') fprintf ( OutFile,"erfcl ");
		else if (op [1] == 'X') fprintf ( OutFile,"powl ");
		else if (op [1] == 'Y') fprintf ( OutFile,"compoundl ");
		else if (op [1] == 'Z') fprintf ( OutFile,"annuityl ");
		else if (op [1] == 'a') fprintf ( OutFile,"ceill ");
		else if (op [1] == 'b') fprintf ( OutFile,"modfl ");
		else if (op [1] == 'c') fprintf ( OutFile,"fpclassifyl ");
		else if (op [1] == 'd') fprintf ( OutFile,"bin2dec2binl ");
		else if (op [1] == 'f') fprintf ( OutFile,"floorl ");
		else if (op [1] == 'g') fprintf ( OutFile,"tgammal ");
		else if (op [1] == 'h') fprintf ( OutFile,"lgammal ");
		else if (op [1] == 'i') fprintf ( OutFile,"nearbyintl ");
		else if (op [1] == 'k') fprintf ( OutFile,"lroundl ");
		else if (op [1] == 'm') fprintf ( OutFile,"signbitl ");
		else if (op [1] == 'n') fprintf ( OutFile,"isnanl ");
		else if (op [1] == 'o') fprintf ( OutFile,"isnormall ");
		else if (op [1] == 'q') fprintf ( OutFile,"remquol ");
		else if (op [1] == 'r') fprintf ( OutFile,"remquol ");
		else if (op [1] == '%') fprintf ( OutFile,"remainderl ");
		else if (op [1] == 's') fprintf ( OutFile,"ldexpl ");
		else if (op [1] == 't') fprintf ( OutFile,"transferl ");
		else if (op [1] == 'u') fprintf ( OutFile,"acoshl ");
		else if (op [1] == 'v') fprintf ( OutFile,"asinhl ");
		else if (op [1] == 'w') fprintf ( OutFile,"atanhl ");
		else if (op [1] == 'x') fprintf ( OutFile,"coshl ");
		else if (op [1] == 'y') fprintf ( OutFile,"sinhl ");
		else if (op [1] == 'z') fprintf ( OutFile,"tanhl ");
		else if (op [1] == '+') fprintf ( OutFile,"Addl ");
		else if (op [1] == '-') fprintf ( OutFile,"Subl ");
		else if (op [1] == '*') fprintf ( OutFile,"Multl ");
		else if (op [1] == '/') fprintf ( OutFile,"Divl ");
		else if (op [1] == '~') fprintf ( OutFile,"negl ");
		else if (op [1] == '@') fprintf ( OutFile,"copysignl ");
		else if (op [1] == '>') fprintf ( OutFile,"fmaxl ");
		else if (op [1] == '<') fprintf ( OutFile,"fminl ");
		else if (op [1] == '&') fprintf ( OutFile,"lrintl ");
		else fprintf ( OutFile,"??? ");
		fprintf ( OutFile," r = ");
		printldbl (r);
		fprintf ( OutFile,"%7.2e", (double)r);
		fprintf ( OutFile,"    n = ");
		printldbl (n);
		fprintf ( OutFile,"%7.2e\n", (double)n);
		fprintf ( OutFile,"expected     ");
		printldbl (rslt);
		fprintf ( OutFile,"%7.2e    %s\n", (double)rslt, flags);
		fprintf ( OutFile,"computed     ");
		printldbl (x);
		fprintf ( OutFile,"%7.2e    %s", (double)x, flags2);
		if (ncmp) fprintf ( OutFile,"   NUM ERROR %d,", ulps);
		if (icmp) fprintf ( OutFile,"   FLAG ERROR");
		if (ecmp) fprintf ( OutFile,"   ENVRM ERROR");
		if (rcmp) fprintf ( OutFile,"   ROUND ERROR");
		if (acmp) 
        {
			fprintf ( OutFile,"   Argument corrupted ERROR");
			exit (5);
		}
        if (ecmp) 
        {
            fprintf ( OutFile,"\n   environment corrupted ERROR\n");
            fprintf ( OutFile," expected iouxz\n"); 
            fprintf ( OutFile," computed %s", flags3);
        }
        if (rcmp) 
        {
            fprintf ( OutFile,"\ncomputed     ");
            printldbl (x2);
            fprintf ( OutFile,"%7.2e%3d  rounding problem! ", (double)x2, i);
            switch (i) {
                case 0	: fprintf ( OutFile," FE_DOWNWARD\n"); break;
                case 1	: fprintf ( OutFile," FE_TONEAREST\n"); break;
                case 2	: fprintf ( OutFile," FE_UPWARD\n"); break;
                case 3	: fprintf ( OutFile," FE_TOWARDZERO\n"); break;
            }
        }
		fprintf ( OutFile,"\n\n");
    #endif /* noprint */
	}
}
#endif

static void testfcnf (const char *op, float r, float n,
					const char *flags, float rslt, const char *LinBuf) 
{
    int HT, LT;

    double x, x2 = 0.0, y, rhold, nhold;
    int icmp, ncmp, acmp, ecmp = 0, rcmp = 0, ulps = 0;
    char flags2 [8], flags3 [8];
    char *cp = flags2;
    int i = 0, excepts, rnddir = 0;

	rhold = r;
	nhold = n;
	
    if (isnan(rslt)) rslt = NAN; // Use generic NaN
    
	numtests++;
	flags2 [0] = '\0';	// Null out string
        
    feclearexcept (FE_ALL_EXCEPT);

	if (rnds != 0) 
    {
             if ((rnds & 1) != 0) (void)fesetround (FE_DOWNWARD);
		else if ((rnds & 2) != 0) (void)fesetround (FE_TONEAREST);
		else if ((rnds & 4) != 0) (void)fesetround (FE_UPWARD);
		else if ((rnds & 8) != 0) (void)fesetround (FE_TOWARDZERO);
	}
	
    // UNDERFLOW weirds this out, see mail from 10/31/2003: sniffSIGFPE(flags);
    
	x = testfcnsf (op, r, n);
        
    if (isnan(x)) x = NAN; // Use generic C99 NaN
    
#if defined (__i386__) /* Accomodation for x87 behaviors */
    if (isnan(x) && strstr(flags, "i")) feclearexcept(FE_INEXACT);
    if (!isnan(x) && x != 0.0 && fabs(x) < DBL_MIN && strstr(flags, "u")) feraiseexcept(FE_UNDERFLOW);
    if (!isnan(x) && x != 0.0 && fabs(x) < DBL_MIN && strstr(flags, "x")) feraiseexcept(FE_INEXACT);
    if (!isnan(x) && fabs(x) > DBL_MAX && strstr(flags, "o")) feraiseexcept(FE_OVERFLOW);
    if (!isnan(x) && fabs(x) > DBL_MAX && strstr(flags, "x")) feraiseexcept(FE_INEXACT);
#endif

	if (rnds != 0) 
    {
             if ((rnds & 1) != 0) rnddir = (FE_DOWNWARD);
		else if ((rnds & 2) != 0) rnddir = (FE_TONEAREST);
		else if ((rnds & 4) != 0) rnddir = (FE_UPWARD);
		else if ((rnds & 8) != 0) rnddir = (FE_TOWARDZERO);
	}
    else
        rnddir = FE_TONEAREST;
    
    if (rnddir != fegetround())
    {
        fprintf ( OutFile,"Env blunder encountered >>>%s<<<\n", (char const *) LinBuf);
    }
	if (rnds != 0) fesetround (FE_TONEAREST);
   
    excepts = fetestexcept(FE_ALL_EXCEPT);

    clearSIGFPE();

    if (excepts & (FE_INVALID)) cp = strcat (flags2, "i");
    if (excepts & (FE_OVERFLOW)) cp = strcat (flags2, "o");
    
    if (excepts & (FE_UNDERFLOW)) cp = strcat (flags2, "u");
    else if (strstr(flags, "u") && (op [1] == '1' || op [1] == '3' || op [1] == '4' || op [1] == '5' || op [1] == '6' || 
        op [1] == 'y' || op [1] == 'z' || op [1] == 'v' || op [1] == 'w')) cp = strcat (flags2, "u");
    

    if (excepts & (FE_INEXACT)) cp = strcat (flags2, "x");
    if (excepts & (FE_DIVBYZERO)) cp = strcat (flags2, "z");
    
	if (strlen (flags2) == 0) cp = strcat (flags2, "OK");
	acmp = !samebits(r, rhold) || !samebits(n, nhold);
	y = x;
	
	if ((pctype == 'i') || (pctype == 'l') || (pctype == 's')) 	//	code for bin2dec2bin
    {
		if (pctype == 'i') 	rslt = Str90toint (s1, op);
		else if (pctype == 'l') 	rslt = Str90tolng (s1, op);
		else if (pctype == 's') 	rslt = Str90toflt (s1, op);
//		pctype = 'd';
	}
	
	while ((y == y) && (rslt == rslt) && (!samebits(y, rslt)) && (abs(ulps) < 99)) 
    {
		if (y > rslt) ulps++;
		else ulps--;
		y = nextafterf (y, rslt);
	}
	if (!strcmp (flags, "xo")) flags = "ox";
	if (!strcmp (flags, "xu")) flags = "ux";
	if ((icmp = (NOFLAGTESTS) ? 0: strcmp (flags, flags2))) flagerrors++;
	
	if (Tolerance) 
    { 
		HT = (RSign != 0) ? LoTol : HiTol;
		LT = (RSign != 0) ? -HiTol : -LoTol;
	}
	else 
    {
		HT = 0;
		LT = 0;
	}
	ncmp = (ulps < LT) || (ulps > HT);
	if ((/*ncmp =*/ (!samebits(x, rslt) && ((ulps == 0) || ncmp)))) numerrors++;
	if (!(ncmp || icmp || NOFLAGTESTS || (rnds == 0))) 
    {
		for (i = 0; i < 4; i++) 
        {

			if (/* XXX (rnds == 0) || XXX */
                            (((rnds & 1) != 0) && (i == 0))
							|| (((rnds & 2) != 0) && (i == 1))
							|| (((rnds & 4) != 0) && (i == 2))
							|| (((rnds & 8) != 0) && (i == 3)))
			{
				if (i == 0) (void)fesetround (FE_DOWNWARD);
				else if (i == 1) (void)fesetround (FE_TONEAREST);
				else if (i == 2) (void)fesetround (FE_UPWARD);
				else if (i == 3) (void)fesetround (FE_TOWARDZERO);
				flags3 [0] = '\0';	// Null out string
				feraiseexcept (FE_INVALID);
				feraiseexcept (FE_OVERFLOW);
				feraiseexcept (FE_INEXACT);
				feraiseexcept (FE_DIVBYZERO);
				feraiseexcept (FE_UNDERFLOW);
			
				x2 = testfcnsf (op, r, n);
                if (isnan(x2)) x2 = NAN; // Use generic NaN
	
				if (fetestexcept(FE_INVALID)) cp = strcat (flags3, "i");
				if (fetestexcept(FE_OVERFLOW)) cp = strcat (flags3, "o");
				if (fetestexcept(FE_UNDERFLOW)) cp = strcat (flags3, "u");
				if (fetestexcept(FE_INEXACT)) cp = strcat (flags3, "x");
				if (fetestexcept(FE_DIVBYZERO)) cp = strcat (flags3, "z");
				rcmp = !samebits(x, x2);
				if ((ecmp = strcmp ("iouxz", flags3))) 
                {
					enverrors++;
					if (!rcmp) break;
				}
				if (rcmp) 
                {
					rnderrors++;
					break;
				}
			}
		}
		(void)fesetround (FE_TONEAREST);
	}
	// ncmp |= (op[0] == '=') && !samebits(x, rslt); //silver == gold but x != silver (rslt is silver in OTVecServer)
	if (ncmp || icmp || acmp || ecmp || rcmp) 
    {
		errorcount++;
    #ifdef	noprint
    #else	/* noprint */
		fprintf ( OutFile,"%s", LinBuf);
		if (op [1] == '1') fprintf ( OutFile,"sinf ");
		else if (op [1] == '2') fprintf ( OutFile,"cosf ");
		else if (op [1] == '3') fprintf ( OutFile,"tanf ");
		else if (op [1] == '4') fprintf ( OutFile,"atanf ");
		else if (op [1] == '5') fprintf ( OutFile,"atan2f ");
		else if (op [1] == '6') fprintf ( OutFile,"asinf ");
		else if (op [1] == '7') fprintf ( OutFile,"acosf ");
		else if (op [1] == '8') fprintf ( OutFile,"log10f ");
		else if (op [1] == 'A') fprintf ( OutFile,"fabsf ");
		else if (op [1] == 'B') fprintf ( OutFile,"modff ");
		else if (op [1] == 'C') fprintf ( OutFile,"comparef ");
		else if (op [1] == 'D') fprintf ( OutFile,"fdimf ");
		else if (op [1] == 'e') fprintf ( OutFile,"frexpf ");
		else if (op [1] == 'E') fprintf ( OutFile,"frexpf ");
		else if (op [1] == 'F') fprintf ( OutFile,"isfinitef ");
		else if (op [1] == 'G') fprintf ( OutFile,"erff ");
		else if (op [1] == 'H') fprintf ( OutFile,"hypotf ");
		else if (op [1] == 'I') fprintf ( OutFile,"rintf ");
		else if (op [1] == 'J') fprintf ( OutFile,"truncf ");
		else if (op [1] == 'K') fprintf ( OutFile,"roundf ");
		else if (op [1] == 'L') fprintf ( OutFile,"logbf ");
		else if (op [1] == 'M') fprintf ( OutFile,"fmodf ");
		else if (op [1] == 'N') fprintf ( OutFile,"nextafterdf ");
		else if (op [1] == 'O') fprintf ( OutFile,"log2f ");
		else if (op [1] == 'P') fprintf ( OutFile,"logf ");
		else if (op [1] == 'Q') fprintf ( OutFile,"log1pf ");
		else if (op [1] == 'R') fprintf ( OutFile,"exp2f ");
		else if (op [1] == 'S') fprintf ( OutFile,"scalbf ");
		else if (op [1] == 'T') fprintf ( OutFile,"expf ");
		else if (op [1] == 'U') fprintf ( OutFile,"expm1f ");
		else if (op [1] == 'V') fprintf ( OutFile,"sqrtf ");
		else if (op [1] == 'W') fprintf ( OutFile,"erfcf ");
		else if (op [1] == 'X') fprintf ( OutFile,"powf ");
		else if (op [1] == 'Y') fprintf ( OutFile,"compoundf ");
		else if (op [1] == 'Z') fprintf ( OutFile,"annuityf ");
		else if (op [1] == 'a') fprintf ( OutFile,"ceilf ");
		else if (op [1] == 'b') fprintf ( OutFile,"modff ");
		else if (op [1] == 'c') fprintf ( OutFile,"fpclassifyf ");
		else if (op [1] == 'd') fprintf ( OutFile,"bin2dec2binf ");
		else if (op [1] == 'f') fprintf ( OutFile,"floorf ");
		else if (op [1] == 'g') fprintf ( OutFile,"tgammaf ");
		else if (op [1] == 'h') fprintf ( OutFile,"lgammaf ");
		else if (op [1] == 'i') fprintf ( OutFile,"nearbyintf ");
		else if (op [1] == 'k') fprintf ( OutFile,"lroundf ");
		else if (op [1] == 'm') fprintf ( OutFile,"signbitf ");
		else if (op [1] == 'n') fprintf ( OutFile,"isnanf ");
		else if (op [1] == 'o') fprintf ( OutFile,"isnormalf ");
		else if (op [1] == 'q') fprintf ( OutFile,"remquof ");
		else if (op [1] == 'r') fprintf ( OutFile,"remquof ");
		else if (op [1] == '%') fprintf ( OutFile,"remainderf ");
		else if (op [1] == 's') fprintf ( OutFile,"ldexpf ");
		else if (op [1] == 't') fprintf ( OutFile,"transferf ");
		else if (op [1] == 'u') fprintf ( OutFile,"acoshf ");
		else if (op [1] == 'v') fprintf ( OutFile,"asinhf ");
		else if (op [1] == 'w') fprintf ( OutFile,"atanhf ");
		else if (op [1] == 'x') fprintf ( OutFile,"coshf ");
		else if (op [1] == 'y') fprintf ( OutFile,"sinhf ");
		else if (op [1] == 'z') fprintf ( OutFile,"tanhf ");
		else if (op [1] == '+') fprintf ( OutFile,"Addf ");
		else if (op [1] == '-') fprintf ( OutFile,"Subf ");
		else if (op [1] == '*') fprintf ( OutFile,"Multf ");
		else if (op [1] == '/') fprintf ( OutFile,"Divf ");
		else if (op [1] == '~') fprintf ( OutFile,"negf ");
		else if (op [1] == '@') fprintf ( OutFile,"copysignf ");
		else if (op [1] == '>') fprintf ( OutFile,"fmaxf ");
		else if (op [1] == '<') fprintf ( OutFile,"fminf ");
		else if (op [1] == '&') fprintf ( OutFile,"lrintf ");
		else fprintf ( OutFile,"??? ");
		fprintf ( OutFile," r = ");
		printdblsgl (r);
		fprintf ( OutFile,"%7.2e", r);
		fprintf ( OutFile,"    n = ");
		printdblsgl (n);
		fprintf ( OutFile,"%7.2e\n", n);
		fprintf ( OutFile,"expected     ");
		printdblsgl (rslt);
		fprintf ( OutFile,"%7.2e    %s\n", rslt, flags);
		fprintf ( OutFile,"computed     ");
		printdblsgl (x);
		fprintf ( OutFile,"%7.2e    %s", x, flags2);
		if (ncmp) fprintf ( OutFile,"   NUM ERROR %d,", ulps);
		if (icmp) fprintf ( OutFile,"   FLAG ERROR");
		if (ecmp) fprintf ( OutFile,"   ENVRM ERROR");
		if (rcmp) fprintf ( OutFile,"   ROUND ERROR");
		if (acmp) 
        {
			fprintf ( OutFile,"   Argument corrupted ERROR");
			exit (5);
		}
        if (ecmp) 
        {
            fprintf ( OutFile,"\n   environment corrupted ERROR\n");
            fprintf ( OutFile," expected iouxz\n"); 
            fprintf ( OutFile," computed %s", flags3);
        }
        if (rcmp) 
        {
            fprintf ( OutFile,"\ncomputed     ");
            printdblsgl (x2);
            fprintf ( OutFile,"%7.2e%3d  rounding problem! ", x2, i);
            switch (i) {
                case 0	: fprintf ( OutFile," FE_DOWNWARD\n"); break;
                case 1	: fprintf ( OutFile," FE_TONEAREST\n"); break;
                case 2	: fprintf ( OutFile," FE_UPWARD\n"); break;
                case 3	: fprintf ( OutFile," FE_TOWARDZERO\n"); break;
            }
        }
		fprintf ( OutFile,"\n\n");
    #endif /* noprint */
	}
}

#define TLIST_TEXT "../noship.subproj/TLIST.TEXT"

int DoTestPrecision(char prec) {

    FILE *ListFile;
    FILE *InFile;

    int i, len;
    
    union {
        double d;
        uint32_t i[2];
    } a1, a2, a3;
    
    char op [8], flags [8];
    char TmpBuf[256] = TLIST_TEXT, LinBuf[256];
    char *ptrc;
    
    int testsTotal = 0;
    int errorcountTotal = 0;
    int flagerrorsTotal = 0;
    int numerrorsTotal = 0;
    int enverrorsTotal = 0;
    int rnderrorsTotal = 0;
    
	//  Open TLIST.TEXT file	
	if ((ListFile = fopen ((char *) TmpBuf, "r")) == NULL) 
    {
		fprintf ( OutFile,"\n%s%s\n", "There is no input file called:  ", TmpBuf);
		exit (NOTEST);
	}

    if (prec == 'd')
    {
        fprintf ( OutFile,"Results of signbit({-NAN, -INF, -1.0, -0.0, 0.0, 1.0, INF, NAN}):\n");
        fprintf ( OutFile,"%d %d %d %d %d %d %d %d\n", 
        signbit((double)-NAN),signbit((double)-INFINITY), signbit((double)-1.0), signbit((double)-0.0),
        signbit((double)0.0), signbit((double)1.0), signbit((double)INFINITY), signbit((double)NAN));
    }
    
    do {
		ptrc = fgets ((char *) TmpBuf, 256, ListFile);
		if (ptrc == NULL) 
            break;
        
		//  Remove trailing '\n' (LF) and replace with zero
        len = strlen ((char *) TmpBuf);
		for (i = 0; i < len; i++) {
			if (TmpBuf [i] == '\n') 
            {
				TmpBuf [i] = '\0';
				break;
			}
        }
        
		if (TmpBuf[0] == '!') // comment line?
            continue;
            
		if (TmpBuf[0] != '\0') 
        {
            printf("\t%s\n",TmpBuf);
            fprintf ( OutFile,"\n%s%s\n", "Input file: ", TmpBuf);
            fflush( OutFile );
        }
        
        if (TmpBuf[0] == 'U') // Take vectors from a network service
        {
            int fd;
            int length;
            static struct sockaddr_in name;
            struct hostent *pH;
            double dnumtests = 0.0;
            
            if (prec != 'd')
                continue;
                
            LinBuf[0] = '\0';
            LinBuff[0] = '\0';
            
            for(;;) 
            { 
                char *p = (char *)v;
                int remain = sizeof(v);
    
                fd = socket(AF_INET, SOCK_STREAM, 0);
                if (fd < 0) 
                {
                    perror("socket() failed.");
                    close( fd );
                    break;
                }
                
                name.sin_family = AF_INET;
                name.sin_port = htons(12345);
                pH = gethostbyname( &TmpBuf[1] );
                if (pH)
                    bcopy(pH->h_addr_list[0], &name.sin_addr, pH->h_length);
                else 
                {
                    perror("gethostbyname() failed.");
                    close( fd );
                    break;
                }
    
                length = connect(fd, (struct sockaddr *)&name, sizeof(name));
                if (length < 0) 
                {
                    perror("connect() failed.");
                    close( fd );
                    break;
                }
                
                printf("\t%s\n",TmpBuf);
                write(fd, &v, 128);
                
                while (remain > 0) 
                {
                    length = read(fd, p, remain);
                    if (length <= 0) {
                        perror("read failed");
                        break;
                    }
                    p += length;
                    remain -= length;
                }
                
                if (v[0].op[0] == 'E')  // EOF marker
                {
                    fprintf(stderr,"EOF marker\n");
                    fflush(stderr);
                    break;
                }
                    
                for (i = 0; i < kMaxVectorCount; ++i) 
                {
                    modes[0] = '\0';
                    (void) strcat (modes, "=d"); // Hardwire "=d" mode
                    
					rnds = 0;
					if (strchr (modes, '<') != NULL) rnds += 1;
					if (strchr (modes, '=') != NULL) rnds += 2;
					if (strchr (modes, '>') != NULL) rnds += 4;
					if (strchr (modes, '0') != NULL) rnds += 8;
                    
                    flags[0] = '\0';
                    if (v[i].flags & (FE_INVALID)) (void) strcat (flags, "i");
                    if (v[i].flags & (FE_OVERFLOW)) (void) strcat (flags, "o");
                    if (v[i].flags & (FE_UNDERFLOW)) (void) strcat (flags, "u");
                    if (v[i].flags & (FE_INEXACT)) (void) strcat (flags, "x");
                    if (v[i].flags & (FE_DIVBYZERO)) (void) strcat (flags, "z");
                    if (strlen(flags) == 0) (void) strcat (flags, "OK");
                    
                    LinBuf[0] = v[i].op[0];
                    LinBuff[0] = v[i].op[0];
                    LinBuf[1] = '\0';
                    LinBuff[1] = '\0';
                    
                    {
                        int saveTol = Tolerance, saveHi = HiTol, saveLo = LoTol;
                        
                        // Tolerate +-1ulp discrepancies
                        Tolerance = 1; 
                        HiTol = 2;
                        LoTol = 2;
                        
                        testfcn ((const char *)&(v[i].op), v[i].r.d, v[i].n.d, flags, v[i].result.d, (char const *) LinBuf);
                        dnumtests = dnumtests + 1.0;
                    
                        Tolerance = saveTol; 
                        HiTol = saveHi;
                        LoTol = saveLo;
                    }

                    if (numtests % 1000000 == 0) 
                    {
                        printf ( "numtests, errorcount, numerrors, flagerrors, enverrors, rnderrors   %s\n", TmpBuf);
                        printf ( "%12e   %3d        %3d         %3d        %3d        %3d\n",
                                 dnumtests, errorcount, numerrors, flagerrors, enverrors, rnderrors);
                        fflush(stdout);
                    }
                }
                close( fd );
            }
#ifdef	noprint
#else	/* noprint */
			if (NOFLAGTESTS) fprintf ( OutFile,"NOFLAGTESTS\n");

			fprintf ( OutFile,"numtests, errorcount, numerrors, flagerrors, enverrors, rnderrors   %s\n", TmpBuf);
			fprintf ( OutFile,"   %3d        %3d        %3d         %3d        %3d        %3d\n",
					 numtests, errorcount, numerrors, flagerrors, enverrors, rnderrors);
                     
			numtestsTotal += numtests;
			testsTotal += numtests;
			errorcountTotal += errorcount;
			flagerrorsTotal += flagerrors;
			numerrorsTotal += numerrors;
			enverrorsTotal += enverrors;
			rnderrorsTotal += rnderrors;
            
			numtests = 0;
			errorcount = 0;
			flagerrors = 0;
			numerrors = 0;
			enverrors = 0;
			rnderrors = 0;
            
#endif /* noprint */
            continue;
        }
        else
        {
			//  Open next test file in list
			if ((InFile = fopen ((const char *) TmpBuf, "r")) == NULL) 
            {
				if (TmpBuf[0] != '\0')
					fprintf ( OutFile,"\n%s%s%s\n", "There is no input *list* file called:  |", TmpBuf, "|");
				continue;
			}
            
			do 
            {
				//  Read one line from test file
                ptrc = fgets  ((char *) LinBuf, 256, InFile);
				strcpy ((char *) LinBuff, (char *) LinBuf);
				if (ptrc == NULL) break;
	
				if ((LinBuf [0] == '2') || (LinBuf [0] == '3') || (LinBuf [0] == '5')) 
                {
					if (sscanf ( (char *) LinBuf, "%s %s %s %s %s %s\r", 
                        (char *) &op, (char *) &modes, (char *) &s1, (char *) &s2, (char *) &flags, (char *) &s3) < 6)
                        abort ();
                       
                    if (strchr (modes, 's') != NULL && strchr (modes, 'e') == NULL && strchr (modes, 'd') == NULL)
                    {
#ifdef DEBUG
                            fprintf ( OutFile,"Pure 's' test vector encountered >>>%s", (char const *) LinBuf);
#endif
                            continue;
                    }
                                                                                 
					rnds = 0;
					if (strchr (modes, '<') != NULL) rnds += 1;
					if (strchr (modes, '=') != NULL) rnds += 2;
					if (strchr (modes, '>') != NULL) rnds += 4;
					if (strchr (modes, '0') != NULL) rnds += 8;

                    switch (prec)
                    {
                    case 'd':
                    default:
                    {
                        double r, n, rslt;
                        int saveTol = Tolerance, saveHi = HiTol, saveLo = LoTol;
                        
                        r = Str90todbl (s1, op);
                        n = (LinBuf [1] == 't') ?  Str90toflt (s1, op): Str90todbl (s2, op);
                        if (LinBuf [1] != 'C') rslt = Str90todbl (s3, op);
                        else 
                        {
                            if (!strcmp((char const *) s3, (char const *) "<")) rslt = -1;
                            else if (!strcmp((char const *) s3, (char const *) "=")) rslt = 0;
                            else if (!strcmp((char const *) s3, ">")) rslt = 1;
                            else if (!strcmp((char const *) s3, "?")) rslt = 2;
                            else rslt = 99;
                        }

                        testfcn (op, r, n, flags, rslt, (char const *) LinBuf);
                        
                        Tolerance = saveTol; 
                        HiTol = saveHi;
                        LoTol = saveLo;
                    }
                    break;
                    
                    case 'f':
                    {
                        float r,n,rslt;
                        int saveTol = Tolerance, saveHi = HiTol, saveLo = LoTol;
                        

                        r = Str90toflt (s1, op);
                        n = Str90toflt (s2, op);
                        if (LinBuf [1] != 'C') rslt = Str90toflt (s3, op);
                        else 
                        {
                            if (!strcmp((char const *) s3, (char const *) "<")) rslt = -1;
                            else if (!strcmp((char const *) s3, (char const *) "=")) rslt = 0;
                            else if (!strcmp((char const *) s3, ">")) rslt = 1;
                            else if (!strcmp((char const *) s3, "?")) rslt = 2;
                            else rslt = 99;
                        }

                        // Tolerate +-1ulp discrepancies
                        Tolerance = 1; 
                        HiTol = 2;
                        LoTol = 2;
                        
                        testfcnf (op, r, n, flags, rslt, (char const *) LinBuf);
                        
                        Tolerance = saveTol; 
                        HiTol = saveHi;
                        LoTol = saveLo;
                    }
                    break;
                    
#if ( __WANT_LONG_DOUBLE_FORMAT__ - 0L == 128L )
                    case 'l':
                    {
                        long double rr,nn,rrslt;
                        int saveTol = Tolerance, saveHi = HiTol, saveLo = LoTol;
                        

                        rr = Str90toldbl (s1, op);
                        nn = Str90toldbl (s2, op);
                        if (LinBuf [1] != 'C') rrslt = Str90toldbl (s3, op);
                        else 
                        {
                            if (!strcmp((char const *) s3, (char const *) "<")) rrslt = -1;
                            else if (!strcmp((char const *) s3, (char const *) "=")) rrslt = 0;
                            else if (!strcmp((char const *) s3, ">")) rrslt = 1;
                            else if (!strcmp((char const *) s3, "?")) rrslt = 2;
                            else rrslt = 99;
                        }
                        
                        if ( strchr((char const *)s1, 'E') == NULL && /* punt vectors probing near "E" */
                             strchr((char const *)s2, 'E') == NULL &&
                             strchr((char const *)s3, 'E') == NULL &&
                             LinBuf [1] != 'd' /* punt B2D2B */)
                        {
                            if (strchr (modes, '=') != NULL || 
                                0 == strcmp(modes, "d") || 
                                0 == strcmp(modes, "ALL"))
                            {
                                modes[0] = '\0';
                                (void) strcat (modes, "=d"); // Hardwire "=d" mode
                                rnds = 0;
                                fesetround (FE_TONEAREST);
                                testfcnl (op, rr, nn, flags, rrslt, (char const *) LinBuf);
                            }
                        }
                        
                        Tolerance = saveTol; 
                        HiTol = saveHi;
                        LoTol = saveLo;
                    }
                    break;
#elif ( __WANT_LONG_DOUBLE_FORMAT__ - 0L == 64L )
                    case 'l':
                    {
                        double r, n, rslt;
                        int saveTol = Tolerance, saveHi = HiTol, saveLo = LoTol;
                        
                        r = Str90todbl (s1, op);
                        n = (LinBuf [1] == 't') ?  Str90toflt (s1, op): Str90todbl (s2, op);
                        if (LinBuf [1] != 'C') rslt = Str90todbl (s3, op);
                        else 
                        {
                            if (!strcmp((char const *) s3, (char const *) "<")) rslt = -1;
                            else if (!strcmp((char const *) s3, (char const *) "=")) rslt = 0;
                            else if (!strcmp((char const *) s3, ">")) rslt = 1;
                            else if (!strcmp((char const *) s3, "?")) rslt = 2;
                            else rslt = 99;
                        }

                        testfcnl (op, (long double)r, (long double)n, flags, (long double)rslt, (char const *) LinBuf);
                        
                        Tolerance = saveTol; 
                        HiTol = saveHi;
                        LoTol = saveLo;
                    }
                    break;
#endif
                    }
				}
				else if (LinBuf [0] == '4')
                {
                    if (prec != 'd')
                        continue;
                        
#if (defined(__ppc__) || defined(__ppc64__))
					if (sscanf ( (char const *) LinBuf, "%s %s %x %x %x %x %s %x %x\r", 
                        (char *) &op, (char *) &modes, (int *) &a1.i [0],
						(int *) &a1.i [1], (int *) &a2.i [0], (int *) &a2.i [1], (char *) &flags, 
                        (int *) &a3.i [0], (int *) &a3.i [1]) < 9)
                        abort ();
#elif defined (__i386__)
					if (sscanf ( (char const *) LinBuf, "%s %s %x %x %x %x %s %x %x\r", 
                        (char *) &op, (char *) &modes, (int *) &a1.i [1],
						(int *) &a1.i [0], (int *) &a2.i [1], (int *) &a2.i [0], (char *) &flags, 
                        (int *) &a3.i [1], (int *) &a3.i [0]) < 9)
                        abort ();
#else
#error Unknown architecture
#endif

                    if (strchr (modes, 's') != NULL && strchr (modes, 'e') == NULL && strchr (modes, 'd') == NULL)
                    {
                            fprintf ( OutFile,"Pure 's' test vector encountered >>>%s", (char const *) LinBuf);
                            continue;
                    }

                    {
                        double r, n, rslt;
                    
                        rnds = 0;
                        if (strchr (modes, '<') != NULL) rnds += 1;
                        if (strchr (modes, '=') != NULL) rnds += 2;
                        if (strchr (modes, '>') != NULL) rnds += 4;
                        if (strchr (modes, '0') != NULL) rnds += 8;

                        r = a1.d;
                        n = a2.d;
                        rslt = a3.d;
                        testfcn (op, r, n, flags, rslt, (char const *) LinBuf);
                    }
				}
				else if ((LinBuf [0] == '!') || (LinBuf [0] == ' ') ||
						(LinBuf [0] == '\x09') || (LinBuf [0] == '\r') ||
						(LinBuf [0] == '\n')) i = 1;
                else if (LinBuf [0] == 'E') break; // EOF on UDP 
				else 
                {
					fprintf ( OutFile,"Unknown test vector encountered >>>%s<<<\n", (char const *) LinBuf);
                    for (i = 0; i < (int)strlen ((char const *) LinBuf); i++)  fprintf ( OutFile,"%3d\n", LinBuf [i]);
					exit (NOOP);
				}
			} 
            while ( !feof (InFile));				// end of data file
            
			fclose (InFile);
            
#ifdef	noprint
#else	/* noprint */
			if (NOFLAGTESTS) fprintf ( OutFile,"NOFLAGTESTS\n");

			fprintf ( OutFile,"numtests, errorcount, numerrors, flagerrors, enverrors, rnderrors   %s\n", TmpBuf);
			fprintf ( OutFile,"   %3d        %3d        %3d         %3d        %3d        %3d\n",
					 numtests, errorcount, numerrors, flagerrors, enverrors, rnderrors);
                     
			numtestsTotal += numtests;
			testsTotal += numtests;
			errorcountTotal += errorcount;
			flagerrorsTotal += flagerrors;
			numerrorsTotal += numerrors;
			enverrorsTotal += enverrors;
			rnderrorsTotal += rnderrors;
            
			numtests = 0;
			errorcount = 0;
			flagerrors = 0;
			numerrors = 0;
			enverrors = 0;
			rnderrors = 0;
#endif /* noprint */
		}
    } 
    while ( !feof (ListFile));				// end of data file listing file TLIST.TEXT
    
	fprintf ( OutFile,"\n\nnumtests, errorcount, NUMerrors, FLAGerrors, ENVRMerrs, ROUNDerrs\n");
	fprintf ( OutFile,"%6d       %4d       %4d        %4d       %4d        %4d\n",
		testsTotal, errorcountTotal, numerrorsTotal, flagerrorsTotal, enverrorsTotal, rnderrorsTotal);
        
    return 0;
}

#if defined(__ppc64__)
#define IEEE_TEST_RESULTS "../noship.subproj/VectorTestLP64.results"
#elif defined(__i386__)
#define IEEE_TEST_RESULTS "../noship.subproj/VectorTestx86.results"
#elif defined(__cplusplus)
#if defined(__LONG_DOUBLE_128__)
#define IEEE_TEST_RESULTS "../noship.subproj/VectorTestLDBL128++.results"
#else
#define IEEE_TEST_RESULTS "../noship.subproj/VectorTestLDBL64++.results"
#endif
#else
#if defined(__LONG_DOUBLE_128__)
#define IEEE_TEST_RESULTS "../noship.subproj/VectorTestLDBL128.results"
#else
#define IEEE_TEST_RESULTS "../noship.subproj/VectorTestLDBL64.results"
#endif
#endif
int main(int argc, char **argv)
{
    time_t tod;
    char s[256];
    int err;
    
    act.__sigaction_u.__sa_sigaction = myHandler;
    act.sa_mask = 0;
    act.sa_flags = SA_SIGINFO;
    
    dfl.__sigaction_u.__sa_handler = SIG_DFL;
    dfl.sa_mask = 0;
    dfl.sa_flags = SA_SIGINFO;
    
    OutFile = fopen ( IEEE_TEST_RESULTS, "w" );
    if (NULL == OutFile)
    {
        perror("The 'fopen' call failed with errno:");
        printf("Trying to open: %s\n", IEEE_TEST_RESULTS);
        exit(-1);
    }
    
    tod = time( NULL );
    
    fprintf ( OutFile, "\nIEEE-754 Test Harness. %s %s\n", argv[0], ctime(&tod));
    
    printf("\nIEEE-754 Test Harness. %s %s\n", argv[0], ctime(&tod));
    printf("libm_debug.a is statically linked. Dynamic libraries follow ...\n");
    sprintf(s, "otool -L %s", argv[0]);
    if(0 != (err = system(s)))
    {
        perror("The 'system' call failed with errno:");
        printf("The 'system' call returned %d\n", err);
    }
    printf("\nDetailed results will be logged to %s.\n", IEEE_TEST_RESULTS);
    
#ifdef __WANT_LONG_DOUBLE_FORMAT__
#if (defined(__ppc__) || defined(__ppc64__))
    fprintf ( OutFile, "Testing long double. sizeof(long double) = %ld\n", sizeof(long double));
    printf ("Testing long double. sizeof(long double) = %ld\n", sizeof(long double));

    DoTestPrecision('l');
    tod = time( NULL );
    fprintf ( OutFile, "\n+++++++++++++++++++++++++++++ %s\n", ctime(&tod) );
#endif
#endif

    printf ("Testing double.\n");

    DoTestPrecision('d');
    tod = time( NULL );
    fprintf ( OutFile, "\n+++++++++++++++++++++++++++++ %s\n", ctime(&tod) );
    
    printf ("Testing float.\n");

    DoTestPrecision('f');
    tod = time( NULL );
    fprintf ( OutFile, "\nTotal test count: %d completed %s\n", numtestsTotal, ctime(&tod) );
    
    fclose ( OutFile );

    printf ("Testing complete.\n");
    return 0;
}
