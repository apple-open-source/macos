/**************************************************************
 *
 *  giants.c
 *
 *  Library for large-integer arithmetic.
 * 
 *  The large-gcd implementation is due to J. P. Buhler.
 *  Special mod routines use ideas of R. McIntosh.
 *  Contributions from G. Woltman, A. Powell acknowledged.
 *
 *  Updates:
 *      18 Jul 99   REC  Added routine fer_mod(), for use when Fermat
                         giant itself is available.
 *      17 Jul 99   REC  Fixed sign bug in fermatmod()
 *      17 Apr 99   REC  Fixed various comment/line wraps
 *      25 Mar 99   REC  G. Woltman/A. Powell fixes Karat. routines
 *      05 Mar 99   REC  Moved invaux, binvaux giants to stack
 *      05 Mar 99   REC  Moved gread/gwrite giants to stack
 *      05 Mar 99   REC  No static on cur_den, cur_recip (A. Powell)
 *      28 Feb 99   REC  Error detection added atop newgiant().
 *      27 Feb 99   REC  Reduced wasted work in addsignal().
 *      27 Feb 99   REC  Reduced wasted work in FFTmulg().
 *      19 Feb 99   REC  Generalized iaddg() per R. Mcintosh.
 *       2 Feb 99   REC  Fixed comments.
 *       6 Dec 98   REC  Fixed yet another Karatsuba glitch.
 *       1 Dec 98   REC  Fixed errant case of addg().
 *      28 Nov 98   REC  Installed A. Powell's (correct) variant of
						 Karatsuba multiply.
 *      15 May 98   REC  Modified gwrite() to handle huge integers.
 *      13 May 98   REC  Changed to static stack declarations
 *      11 May 98   REC  Installed Karatsuba multiply, to handle
 *                       medregion 'twixt grammar- and FFT-multiply.
 *       1 May 98   JF   gshifts now handle bits < 0 correctly.
 *      30 Apr 98   JF   68k assembler code removed,
 *                       stack giant size now invariant and based
 *                           on first call of newgiant(),
 *                       stack memory leaks fixed.
 *      29 Apr 98   JF   function prototyes cleaned up,
 *                       GCD no longer uses personal stack,
 *                       optimized shifts for bits%16 == 0.
 *      27 Apr 98   JF   scratch giants now replaced with stack
 *      20 Apr 98   JF   grammarsquareg fixed for asize == 0.
 *                       scratch giants now static.
 *      29 Jan 98   JF   Corrected out-of-range errors in
 *                       mersennemod and fermatmod.
 *      23 Dec 97   REC  Sped up divide routines via split-shift.
 *      18 Nov 97   JF   Improved mersennemod, fermatmod.
 *       9 Nov 97   JF   Sped up grammarsquareg.
 *      20 May 97   RDW  Fixed Win32 compiler warnings.
 *      18 May 97   REC  Installed new, fast divide.
 *      17 May 97   REC  Reduced memory for FFT multiply.
 *      26 Apr 97   REC  Creation.
 *
 *  c. 1997,1998 Perfectly Scientific, Inc.
 *  All Rights Reserved.
 *
 **************************************************************/


/* Include Files. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "giants.h"


/* Compiler options. */

#ifdef _WIN32
#pragma warning( disable : 4127 4706 ) /* disable conditional is constant warning */
#endif


/* Global variables. */

int				error = 0;
int				mulmode = AUTO_MUL;
int				cur_prec = 0;
int				cur_shift = 0;
static int		cur_stack_size = 0;
static int		cur_stack_elem = 0;
static int		stack_glen = 0;
static giant	*stack;
giant			cur_den = NULL,
				cur_recip = NULL;
int				current_max_size = 0,
				cur_run = 0;
double *		sinCos=NULL;
int				checkFFTerror = 0;
double			maxFFTerror;
static giant	u0=NULL, u1=NULL, v0=NULL, v1=NULL;
static double	*z = NULL,
				*z2 = NULL;

/* stack handling functions. */
static giant	popg(void);
static void		pushg(int);


/* Private function prototypes. */

int 		gerr(void);
double		gfloor(double);
int			radixdiv(int, int, giant);
void		columnwrite(FILE *, short *, char *, short, int);

void		normal_addg(giant, giant);
void		normal_subg(giant, giant);
void		reverse_subg(giant, giant);
int			binvaux(giant, giant);
int 		invaux(giant, giant);
int 		allzeros(int, int, giant);
void 		auxmulg(giant a, giant b);
void 		karatmulg(giant a, giant b);
void 		karatsquareg(giant b);
void 		grammarmulg(giant a, giant b);
void		grammarsquareg(giant b);

int 		lpt(int, int *);
void 		addsignal(giant, double *, int);
void 		FFTsquareg(giant x);
void 		FFTmulg(giant y, giant x);
void 		scramble_real();
void 		fft_real_to_hermitian(double *z, int n);
void 		fftinv_hermitian_to_real(double *z, int n);
void 		mul_hermitian(double *a, double *b, int n);
void 		square_hermitian(double *b, int n);
void 		giant_to_double(giant x, int sizex, double *z, int L);
void 		gswap(giant *, giant *);
void 		onestep(giant, giant, gmatrix);
void 		mulvM(gmatrix, giant, giant);
void 		mulmM(gmatrix, gmatrix);
void 		writeM(gmatrix);
static void	punch(giant, gmatrix);
static void	dotproduct(giant, giant, giant, giant);
void		fix(giant *, giant *);
void		hgcd(int, giant, giant, gmatrix);
void		shgcd(int, int, gmatrix);



/**************************************************************
 *
 *	Functions
 *
 **************************************************************/


/**************************************************************
 *
 * Initialization and utility functions
 *
 **************************************************************/

double
gfloor(
	double 	f
)
{
	return floor(f);
}


void
init_sinCos(
	int 		n
)
{
	int 		j;
	double 	e = TWOPI/n;

	if (n<=cur_run)
		return;
	cur_run = n;
	if (sinCos)
		free(sinCos);
	sinCos = (double *)malloc(sizeof(double)*(1+(n>>2)));
	for (j=0;j<=(n>>2);j++)
	{
		sinCos[j] = sin(e*j);
	}
}


double
s_sin(
	int 	n
)
{
	int 	seg = n/(cur_run>>2);

	switch (seg)
	{
		case 0: return(sinCos[n]);
		case 1: return(sinCos[(cur_run>>1)-n]);
		case 2: return(-sinCos[n-(cur_run>>1)]);
		case 3: return(-sinCos[cur_run-n]);
	}
	return 0;
}


double
s_cos(
	int 	n
)
{
	int 	quart = (cur_run>>2);

	if (n < quart)
		return(s_sin(n+quart));
	return(-s_sin(n-quart));
}


int
gerr(void)
{
	return(error);
}


giant
popg (
	void
)
{
	int i;
	
	if (current_max_size <= 0) current_max_size = MAX_SHORTS;
	
	if (cur_stack_size == 0) {
/* Initialize the stack if we're just starting out.
 * Note that all stack giants will be whatever current_max_size is
 * when newgiant() is first called. */
		cur_stack_size = STACK_GROW;
		stack = (giant *) malloc (cur_stack_size * sizeof(giant));
		for(i = 0; i < STACK_GROW; i++)
			stack[i] = NULL;
		if (stack_glen == 0) stack_glen = current_max_size;
	} else if (cur_stack_elem >= cur_stack_size) {
/* Expand the stack if we need to. */
		i = cur_stack_size;
		cur_stack_size += STACK_GROW;
		stack = (giant *) realloc (stack,cur_stack_size * sizeof(giant));
		for (; i < cur_stack_size; i++)
			stack[i] = NULL;
	} else if (cur_stack_elem < cur_stack_size - 2*STACK_GROW) {
/* Prune the stack if it's too big. Disabled, so the stack can only expand */
		/* cur_stack_size -= STACK_GROW;
		for (i = cur_stack_size - STACK_GROW; i < cur_stack_size; i++)
			free(stack[i]);
		stack = (giant *) realloc (stack,cur_stack_size * sizeof(giant)); */
	}
	
/* Malloc our giant. */
	if (stack[cur_stack_elem] == NULL)
		stack[cur_stack_elem] = malloc(stack_glen*sizeof(short)+sizeof(int));
	stack[cur_stack_elem]->sign = 0;
	
	return(stack[cur_stack_elem++]);
}


void
pushg (
	int a
)
{
	if (a < 0) return;
	cur_stack_elem -= a;
	if (cur_stack_elem < 0) cur_stack_elem = 0;
}


giant
newgiant(
	int 		numshorts
)
{
	int 		size;
	giant 		thegiant;

    if (numshorts > MAX_SHORTS) {
		fprintf(stderr, "Requested giant too big.\n");
		fflush(stderr);
	}
	if (numshorts<=0)
		numshorts = MAX_SHORTS;
	size = numshorts*sizeof(short)+sizeof(int);
	thegiant = (giant)malloc(size);
	thegiant->sign = 0;
	
	if (newmin(2*numshorts,MAX_SHORTS) > current_max_size)
		current_max_size = newmin(2*numshorts,MAX_SHORTS);

/* If newgiant() is being called for the first time, set the
 * size of the stack giants. */
	if (stack_glen == 0) stack_glen = current_max_size;

	return(thegiant);
}


gmatrix
newgmatrix(
	void
)
/* Allocates space for a gmatrix struct, but does not
 * allocate space for the giants. */
{
	return((gmatrix) malloc (4*sizeof(giant)));
}

int
bitlen(
	giant		n
)
{
	int 		b = 16, c = 1<<15, w;

	if (isZero(n))
		return(0);
	w = n->n[abs(n->sign) - 1];
	while ((w&c) == 0)
	{
		b--;
		c >>= 1;
	}
	return (16*(abs(n->sign)-1) + b);
}


int
bitval(
	giant 	n,
	int 	pos
)
{
	int 	i = abs(pos)>>4, c = 1 << (pos&15);

	return ((n->n[i]) & c);
}


int
isone(
	giant	g
)
{
	return((g->sign==1)&&(g->n[0]==1));
}


int isZero(
	giant thegiant
)
/* Returns TR if thegiant == 0. */
{
	register int 				count;
	int			 				length = abs(thegiant->sign);
	register unsigned short	*	numpointer = thegiant->n;

	if (length)
	{
		for(count = 0; count<length; ++count,++numpointer)
		{
			if (*numpointer != 0 )
				return(FA);
		}
	}
	return(TR);
}


void
gtog(
	giant		srcgiant,
	giant		destgiant
)
/* destgiant becomes equal to srcgiant. */
{
	int 		numbytes = sizeof(int) + abs(srcgiant->sign)*sizeof(short);

	memcpy((char *)destgiant,(char *)srcgiant,numbytes);
}


void
itog(
	int				i,
	giant			g
)
/* The giant g becomes set to the integer value i. */
{
	unsigned int	j = abs(i);

	if (i==0)
	{
		g->sign = 0;
		g->n[0] = 0;
		return;
	}
	g->n[0] = (unsigned short)(j & 0xFFFF);
	j >>= 16;
	if (j)
	{
		g->n[1] = (unsigned short)j;
		g->sign = 2;
	}
	else
	{
		g->sign = 1;
	}
	if (i<0)
		g->sign = -(g->sign);
}


signed int
gtoi(
	giant 			x
)
/* Calculate the value of an int-sized giant NOT exceeding 31 bits. */
{
	register int 	size = abs(x->sign);
	register int 	sign = (x->sign < 0) ? -1 : 1;

	switch(size)
	{
		case 0:
			break;
		case 1:
			return sign * x->n[0];
		case 2:
			return sign * (x->n[0]+((x->n[1])<<16));
		default:
			fprintf(stderr,"Giant too large for gtoi\n");
			break;
	}
	return 0;
}


int
gsign(
	giant 	g
)
/* Returns the sign of g. */
{
	if (isZero(g))
		return(0);
	if (g->sign >0)
		return(1);
	return(-1);
}


#if 0
int gcompg(a,b)
/* Returns -1,0,1 if a<b, a=b, a>b, respectively. */
	giant a,b;
{
	int size = abs(a->sign);

	if(isZero(a)) size = 0;
	if (size == 0) {
		if (isZero(b)) return(0); else return(-gsign(b));
	}

	if (b->sign == 0) return(gsign(a));
	if (gsign(a)!=gsign(b)) return(gsign(a));
	if (size>abs(b->sign)) return(gsign(a));
	if (size<abs(b->sign)) return(-gsign(a));

	do {
		--size;
		if (a->n[size] > b->n[size]) return(gsign(a));
		if (a->n[size] < b->n[size]) return(-gsign(a));
	 } while(size>0);
	 
	return(0);
}
#else

int
gcompg(
	giant		a,
	giant		b
)
/* Returns -1,0,1 if a<b, a=b, a>b, respectively. */
{
	int 		sa = a->sign, j, sb = b->sign, va, vb, sgn;

	if(sa > sb)
		return(1);
	if(sa < sb)
		return(-1);
	if(sa < 0)
	{
		sa = -sa; /* Take absolute value of sa. */
		sgn = -1;
	}
	else
	{
		sgn = 1;
	}
	for(j = sa-1; j >= 0; j--)
	{
		va = a->n[j];
		vb = b->n[j];
		if (va > vb)
			return(sgn);
		if (va < vb)
			return(-sgn);
	}
	return(0);
}
#endif


void
setmulmode(
	int 	mode
)
{
	mulmode = mode;
}


/**************************************************************
 *
 * Private I/O Functions
 *
 **************************************************************/


int
radixdiv(
	int				base,
	int				divisor,
	giant			thegiant
)
/* Divides giant of arbitrary base by divisor.
 * Returns remainder. Used by idivg and gread. */
{
	int				first = TR;
	int				finalsize = abs(thegiant->sign);
	int				j = finalsize-1;
	unsigned short	*digitpointer=&thegiant->n[j];
	unsigned int 	num,rem=0;

	if (divisor == 0)
	{
		error = DIVIDEBYZERO;
		exit(error);
	}

	while (j>=0)
	{
		num=rem*base + *digitpointer;
		*digitpointer = (unsigned short)(num/divisor);
		if (first)
		{
			if (*digitpointer == 0)
				--finalsize;
			else
				first = FA;
		}
		rem = num % divisor;
		--digitpointer;
		--j;
	}

	if ((divisor<0) ^ (thegiant->sign<0))
		finalsize=-finalsize;
	thegiant->sign=finalsize;
	return(rem);
}


void
columnwrite(
	FILE 	*filepointer,
	short 	*column,
	char 	*format,
	short 	arg,
	int 	newlines
)
/* Used by gwriteln. */
{
	char 	outstring[10];
	short 	i;

	sprintf(outstring,format,arg);
	for (i=0; outstring[i]!=0; ++i)
	{
		if (newlines)
		{
			if (*column >= COLUMNWIDTH)
			{
				fputs("\\\n",filepointer);
				*column = 0;
			}
		}
		fputc(outstring[i],filepointer);
		++*column;
	}
}


void
gwrite(
	giant			thegiant,
	FILE			*filepointer,
	int				newlines
)
/* Outputs thegiant to filepointer. Output is terminated by a newline. */
{
	short			column;
	unsigned int 	i;
	unsigned short	*numpointer;
	giant	garbagegiant, basetengrand;

	basetengrand = popg();
    garbagegiant = popg();

	if (isZero(thegiant))
	{
		fputs("0",filepointer);
	}
	else
	{
		numpointer = basetengrand->n;
		gtog(thegiant,garbagegiant);

		basetengrand->sign = 0;
		do
		{
			*numpointer = (unsigned short)idivg(10000,garbagegiant);
			++numpointer;
			if (++basetengrand->sign >= current_max_size)
			{
				error = OVFLOW;
				exit(error);
			}
		} 	while (!isZero(garbagegiant));

		if (!error)
		{
			i = basetengrand->sign-1;
			column = 0;
			if (thegiant->sign<0 && basetengrand->n[i]!=0)
				columnwrite(filepointer,&column,"-",0, newlines);
			columnwrite(filepointer,&column,"%d",basetengrand->n[i],newlines);
			for( ; i>0; )
			{
				--i;
				columnwrite(filepointer,&column,"%04d",basetengrand->n[i],newlines);
			}
		}
	}
   pushg(2);
}


void
gwriteln(
	giant		theg,
	FILE		*filepointer
)
{
	gwrite(theg, filepointer, 1);
	fputc('\n',filepointer);
}


void
gread(
	giant 			theg,
	FILE 			*filepointer
)
{
	char 			currentchar;
	int 			isneg,size,backslash=FA,numdigits=0;
	unsigned short	*numpointer;
	giant	        basetenthousand;
	static char		*inbuf = NULL;

    basetenthousand = popg();
	if (inbuf == NULL)
		inbuf = (char*)malloc(MAX_DIGITS);

	currentchar = (char)fgetc(filepointer);
	if (currentchar=='-')
	{
		isneg=TR;
	}
	else
	{
		isneg=FA;
		if (currentchar!='+')
			ungetc(currentchar,filepointer);
	}

	do
	{
		currentchar = (char)fgetc(filepointer);
		if ((currentchar>='0') && (currentchar<='9'))
		{
			inbuf[numdigits]=currentchar;
			if(++numdigits==MAX_DIGITS)
				break;
			backslash=FA;
		}
		else
		{
			if (currentchar=='\\')
				backslash=TR;
		}
	} while(((currentchar!=' ') && (currentchar!='\n') &&
				(currentchar!='\t')) || (backslash) );
	if (numdigits)
	{
		size = 0;
		do
		{
			inbuf[numdigits] = 0;
			numdigits-=4;
			if (numdigits<0)
				numdigits=0;
			basetenthousand->n[size] = (unsigned short)strtol(&inbuf[numdigits],NULL,10);
			++size;
		} while (numdigits>0);

		basetenthousand->sign = size;
		theg->sign = 0;
		numpointer = theg->n;
		do
		{
			*numpointer = (unsigned short)
			radixdiv(10000,1<<(8*sizeof(short)),basetenthousand);
			++numpointer;
			if (++theg->sign >= current_max_size)
			{
				error = OVFLOW;
				exit(error);
			}
		} while (!isZero(basetenthousand));

		if (isneg)
			theg->sign = -theg->sign;
	}
    pushg(1);
}



/**************************************************************
 *
 * Private Math Functions
 *
 **************************************************************/


void
negg(
	giant	g
)
/* g becomes -g. */
{
	g->sign = -g->sign;
}


void
absg(
	giant g
)
{
	/* g becomes the absolute value of g. */
	if (g->sign <0)
		g->sign=-g->sign;
}


void
iaddg(
	int		i,
	giant	g
)
/* Giant g becomes g + (int)i. */
{
	int 	w,j=0,carry = 0, size = abs(g->sign);
    giant	tmp;

	if (isZero(g))
	{
		itog(i,g);
	}
	else if(g->sign < 0) {
		tmp = popg();
		itog(i, tmp);
	    addg(tmp, g);
		pushg(1);
		return;
    } 
	else
	{
		w = g->n[0]+i;
		do
		{
			g->n[j] = (unsigned short) (w & 65535L);
			carry = w >> 16;
			w = g->n[++j]+carry;
		} while ((carry!=0) && (j<size));
	}
	if (carry)
	{
		++g->sign;
		g->n[size] = (unsigned short)carry;
	}
}


/* New subtract routines.
	The basic subtract "subg()" uses the following logic table:

     a      b          if(b > a)           if(a > b)
     
     +      +          b := b - a          b := -(a - b)
     -      +          b := b + (-a)       N.A.
     +      -          N.A.                b := -((-b) + a)
	  -      -          b := (-a) - (-b)    b := -((-b) - (-a))

   The basic addition routine "addg()" uses:

	  a      b          if(b > -a)          if(-a > b)
     
     +      +          b := b + a          N.A. 
     -      +          b := b - (-a)       b := -((-a) - b)
     +      -          b := a - (-b)       b := -((-b) - a)
     -      -          N.A.                b := -((-b) + (-a))

   In this way, internal routines "normal_addg," "normal_subg," 
	and "reverse_subg;" each of which assumes non-negative
	operands and a non-negative result, are now used for greater
	efficiency.
 */

void
normal_addg(
	giant			a,
	giant			b
)
/* b := a + b, both a,b assumed non-negative. */
{
	int 			carry = 0;
	int 			asize = a->sign, bsize = b->sign;
	long 			k;
	int				j=0;
	unsigned short	*aptr = a->n, *bptr = b->n;

	if (asize < bsize)
	{
		for (j=0; j<asize; j++)
		{
			k = *aptr++ + *bptr + carry;
			carry = 0;
			if (k >= 65536L)
			{
				k -= 65536L;
				++carry;
			}
			*bptr++ = (unsigned short)k;
		}
		for (j=asize; j<bsize; j++)
		{
			k = *bptr + carry;
			carry = 0;
			if (k >= 65536L)
			{
				k -= 65536L;
				++carry;
			}
			*bptr++ = (unsigned short)k;
		}
	}
	else
	{
		for (j=0; j<bsize; j++)
		{
			k = *aptr++ + *bptr + carry;
			carry = 0;
			if (k >= 65536L)
			{
				k -= 65536L;
				++carry;
			}
			*bptr++ = (unsigned short)k;
		}
		for (j=bsize; j<asize; j++)
		{
			k = *aptr++ + carry;
			carry = 0;
			if (k >= 65536L)
			{
				k -= 65536L;
				++carry;
			}
			*bptr++ = (unsigned short)k;
		}
	}
	if (carry)
	{
		*bptr = 1; ++j;
	}
	b->sign = j;
}


void
normal_subg(
	giant			a,
	giant			b
)
/* b := b - a; requires b, a non-negative and b >= a. */
{
	int 			j, size = b->sign;
	unsigned int	k;

	if (a->sign == 0)
		return;

	k = 0;
	for (j=0; j<a->sign; ++j)
	{
		k += 0xffff - a->n[j] + b->n[j];
		b->n[j] = (unsigned short)(k & 0xffff);
		k >>= 16;
	}
	for (j=a->sign; j<size; ++j)
	{
		k += 0xffff + b->n[j];
		b->n[j] = (unsigned short)(k & 0xffff);
		k >>= 16;
	}

	if (b->n[0] == 0xffff)
		iaddg(1,b);
	else
		++b->n[0];

	while ((size-- > 0) && (b->n[size]==0));

	b->sign = (b->n[size]==0) ? 0 : size+1;
}


void
reverse_subg(
	giant			a,
	giant			b
)
/* b := a - b; requires b, a non-negative and a >= b. */
{
	int 			j, size = a->sign;
	unsigned int	k;

	k = 0;
	for (j=0; j<b->sign; ++j)
	{
		k += 0xffff - b->n[j] + a->n[j];
		b->n[j] = (unsigned short)(k & 0xffff);
		k >>= 16;
	}
	for (j=b->sign; j<size; ++j)
	{
		k += 0xffff + a->n[j];
		b->n[j] = (unsigned short)(k & 0xffff);
		k >>= 16;
	}

	b->sign = size; /* REC, 21 Apr 1996. */
	if (b->n[0] == 0xffff)
		iaddg(1,b);
	else
		++b->n[0];

	while (!b->n[--size]);

	b->sign = size+1;
}

void
addg(
	giant		a,
	giant		b
)
/* b := b + a, any signs any result. */
{
	int 		asgn = a->sign, bsgn = b->sign;

	if (asgn == 0)
		return;
	if (bsgn == 0)
	{
		gtog(a,b);
		return;
	}
	if ((asgn < 0) == (bsgn < 0))
	{
		if (bsgn > 0)
		{
			normal_addg(a,b);
			return;
		}
		absg(b);
		if(a != b) absg(a);
		normal_addg(a,b);
		negg(b);
		if(a != b) negg(a);
		return;
	}
	if(bsgn > 0)
	{
		negg(a);
		if (gcompg(b,a) >= 0)
		{
			normal_subg(a,b);
			negg(a);
			return;
		}
		reverse_subg(a,b);
		negg(a);
		negg(b);
		return;
	}
	negg(b);
	if(gcompg(b,a) < 0)
	{
		reverse_subg(a,b);
		return;
	}
	normal_subg(a,b);
	negg(b);
	return;
}

void
subg(
	giant		a,
	giant		b
)
/* b := b - a, any signs, any result. */
{
	int 		asgn = a->sign, bsgn = b->sign;

	if (asgn == 0)
		return;
	if (bsgn == 0)
	{
		gtog(a,b);
		negg(b);
		return;
	}
	if ((asgn < 0) != (bsgn < 0))
	{
		if (bsgn > 0)
		{
			negg(a);
			normal_addg(a,b);
			negg(a);
			return;
		}
		negg(b);
		normal_addg(a,b);
		negg(b);
		return;
	}
	if (bsgn > 0)
	{
		if (gcompg(b,a) >= 0)
		{
			normal_subg(a,b);
			return;
		}
		reverse_subg(a,b);
		negg(b);
		return;
	}
	negg(a);
	negg(b);
	if (gcompg(b,a) >= 0)
	{
		normal_subg(a,b);
		negg(a);
		negg(b);
		return;
	}
	reverse_subg(a,b);
	negg(a);
	return;
}


int
numtrailzeros(
	giant					g
)
/* Returns the number of trailing zero bits in g. */
{
	register int 			numshorts = abs(g->sign), j, bcount=0;
	register unsigned short gshort, c;

	for (j=0;j<numshorts;j++)
	{
		gshort = g->n[j];
		c = 1;
		for (bcount=0;bcount<16; bcount++)
		{
			if (c & gshort)
				break;
			c <<= 1;
		}
		if (bcount<16)
			break;
	}
	return(bcount + 16*j);
}


void
bdivg(
	giant		v,
	giant		u
)
/* u becomes greatest power of two not exceeding u/v. */
{
	int 		diff = bitlen(u) - bitlen(v);
	giant		scratch7;

	if (diff<0)
	{
		itog(0,u);
		return;
	}
	scratch7 = popg();
	gtog(v, scratch7);
	gshiftleft(diff,scratch7);
	if (gcompg(u,scratch7) < 0)
		diff--;
	if (diff<0)
	{
		itog(0,u);
		pushg(1);
		return;
	}
	itog(1,u);
	gshiftleft(diff,u);

	pushg(1);
}


int
binvaux(
	giant 	p,
	giant 	x
)
/* Binary inverse method. Returns zero if no inverse exists,
 * in which case x becomes GCD(x,p). */
{
	
	giant scratch7, u0, u1, v0, v1;

	if (isone(x))
		return(1);
	u0 = popg();
    u1 = popg();
    v0 = popg();
    v1 = popg();
	itog(1, v0);
	gtog(x, v1);
	itog(0,x);
	gtog(p, u1);

	scratch7 = popg();
	while(!isZero(v1))
	{
		gtog(u1, u0);
		bdivg(v1, u0);
		gtog(x, scratch7);
		gtog(v0, x);
		mulg(u0, v0);
		subg(v0,scratch7);
		gtog(scratch7, v0);

		gtog(u1, scratch7);
		gtog(v1, u1);
		mulg(u0, v1);
		subg(v1,scratch7);
		gtog(scratch7, v1);
	}
	
	pushg(1);

	if (!isone(u1))
	{
		gtog(u1,x);
		if(x->sign<0) addg(p, x);
		pushg(4);
	    return(0);
	}
	if(x->sign<0)
		addg(p, x);
    pushg(4);
	return(1);
}


int
binvg(
	giant 	p,
	giant 	x
)
{
	modg(p, x);
	return(binvaux(p,x));
}


int
invg(
	giant 	p,
	giant 	x
)
{
	modg(p, x);
	return(invaux(p,x));
}

int
invaux(
	giant 	p,
	giant 	x
)
/* Returns zero if no inverse exists, in which case x becomes
 * GCD(x,p). */
{

	giant scratch7, u0, u1, v0, v1;
	
	if ((x->sign==1)&&(x->n[0]==1))
		return(1);
	
    u0 = popg();
    u1 = popg();
    v0 = popg();
    v1 = popg();
    
	itog(1,u1);
	gtog(p, v0);
	gtog(x, v1);
	itog(0,x);

	scratch7 = popg();
	while (!isZero(v1))
	{
		gtog(v0, u0);
		divg(v1, u0);
		gtog(u0, scratch7);
		mulg(v1, scratch7);
		subg(v0, scratch7);
		negg(scratch7);
		gtog(v1, v0);
		gtog(scratch7, v1);
		gtog(u1, scratch7);
		mulg(u0, scratch7);
		subg(x, scratch7);
		negg(scratch7);
		gtog(u1,x);
		gtog(scratch7, u1);
	}
	pushg(1);
	
	if ((v0->sign!=1)||(v0->n[0]!=1))
	{
		gtog(v0,x);
        pushg(4);
		return(0);
	}
	if(x->sign<0)
		addg(p, x);
	pushg(4);
	return(1);
}


int
mersenneinvg(
	int		q,
	giant 	x
)
{
	int		k;
    giant u0, u1, v1;

    u0 = popg();
    u1 = popg();
    v1 = popg();

	itog(1, u0);
	itog(0, u1);
	itog(1, v1);
	gshiftleft(q, v1);
	subg(u0, v1);
	mersennemod(q, x);
	while (1)
	{
		k = -1;
		if (isZero(x))
		{
			gtog(v1, x);
            pushg(3);
			return(0);
		}
		while (bitval(x, ++k) == 0);

		gshiftright(k, x);
		if (k)
		{
			gshiftleft(q-k, u0);
			mersennemod(q, u0);
		}
		if (isone(x))
			break;
		addg(u1, u0);
		mersennemod(q, u0);
		negg(u1);
		addg(u0, u1);
		mersennemod(q, u1);
		if (!gcompg(v1,x)) {
			pushg(3);
			return(0);
        }
		addg(v1, x);
		negg(v1);
		addg(x, v1);
		mersennemod(q, v1);
	}
	gtog(u0, x);
	mersennemod(q,x);
    pushg(3);
	return(1);
}


void
cgcdg(
	giant 	a,
	giant 	v
)
/* Classical Euclid GCD. v becomes gcd(a, v). */
{
	giant 	u, r;

	v->sign = abs(v->sign);
	if (isZero(a))
		return;
	
	u = popg();
	r = popg();
	gtog(a, u);
	u->sign = abs(u->sign);
	while (!isZero(v))
	{
		gtog(u, r);
		modg(v, r);
		gtog(v, u);
		gtog(r, v);
	}
	gtog(u,v);
	pushg(2);
}


void
gcdg(
	giant		x,
	giant		y
)
{
	if (bitlen(y)<= GCDLIMIT)
		bgcdg(x,y);
	else
		ggcd(x,y);
}

void
bgcdg(
	giant 	a,
	giant 	b
)
/* Binary form of the gcd. b becomes the gcd of a,b. */
{
	int		k = isZero(b), m = isZero(a);
	giant 	u, v, t;

	if (k || m)
	{
		if (m)
		{
			if (k)
				itog(1,b);
			return;
		}
		if (k)
		{
			if (m)
				itog(1,b);
			else
				gtog(a,b);
			return;
		}
	}

	u = popg();
	v = popg();
	t = popg();

	/* Now neither a nor b is zero. */
	gtog(a, u);
	u->sign = abs(a->sign);
	gtog(b, v);
	v->sign = abs(b->sign);
	k = numtrailzeros(u);
	m = numtrailzeros(v);
	if (k>m)
		k = m;
	gshiftright(k,u);
	gshiftright(k,v);
	if (u->n[0] & 1)
	{
		gtog(v, t);
		negg(t);
	}
	else
	{
		gtog(u,t);
	}

	while (!isZero(t))
	{
		m = numtrailzeros(t);
		gshiftright(m, t);
		if (t->sign > 0)
		{
			gtog(t, u);
			subg(v,t);
		}
		else
		{
			gtog(t, v);
			negg(v);
			addg(u,t);
		}
	}
	gtog(u,b);
	gshiftleft(k, b);
	pushg(3);
}


void
powerg(
	int		m,
	int		n,
	giant 	g
)
/* g becomes m^n, NO mod performed. */
{
	giant scratch2 = popg();
	
	itog(1, g);
	itog(m, scratch2);
	while (n)
	{
		if (n & 1)
			mulg(scratch2, g);
		n >>= 1;
		if (n)
			squareg(scratch2);
	}
	pushg(1);
}

#if 0
void
jtest(
	giant 	n
)
{
	if (n->sign)
	{
		if (n->n[n->sign-1] == 0)
		{
			fprintf(stderr,"%d %d tilt",n->sign, (int)(n->n[n->sign-1]));
			exit(7);
		}
	}
}
#endif


void
make_recip(
	giant 	d, 
	giant 	r
)
/* r becomes the steady-state reciprocal
 * 2^(2b)/d, where b = bit-length of d-1. */
{
	int		b;
	giant 	tmp, tmp2;

	if (isZero(d) || (d->sign < 0))
	{
		exit(SIGN);
	}
	tmp = popg();
	tmp2 = popg();
	itog(1, r); 
	subg(r, d); 
	b = bitlen(d); 
	addg(r, d);
	gshiftleft(b, r); 
	gtog(r, tmp2);
	while (1) 
	{
		gtog(r, tmp);
		squareg(tmp);
		gshiftright(b, tmp);
		mulg(d, tmp);
		gshiftright(b, tmp);
		addg(r, r); 
		subg(tmp, r);
		if (gcompg(r, tmp2) <= 0) 
			break;
		gtog(r, tmp2);
	}
	itog(1, tmp);
	gshiftleft(2*b, tmp);
	gtog(r, tmp2); 
	mulg(d, tmp2);
	subg(tmp2, tmp);
	itog(1, tmp2);
	while (tmp->sign < 0) 
	{
		subg(tmp2, r);
		addg(d, tmp);
	}
	pushg(2);
}

void
divg_via_recip(
	giant 	d, 
	giant 	r, 
	giant 	n
)
/* n := n/d, where r is the precalculated
 * steady-state reciprocal of d. */
{
	int 	s = 2*(bitlen(r)-1), sign = gsign(n);
	giant 	tmp, tmp2;

	if (isZero(d) || (d->sign < 0))
	{
		exit(SIGN);
	}
	
	tmp = popg();
	tmp2 = popg();
	
	n->sign = abs(n->sign);
	itog(0, tmp2);
	while (1) 
	{
		gtog(n, tmp);	
		mulg(r, tmp);
		gshiftright(s, tmp);
		addg(tmp, tmp2);
		mulg(d, tmp);
		subg(tmp, n);
		if (gcompg(n,d) >= 0)
		{
			subg(d,n);
			iaddg(1, tmp2);
		}
		if (gcompg(n,d) < 0) 
			break;
	}
	gtog(tmp2, n);
	n->sign *= sign;
	pushg(2);
}

#if 1
void
modg_via_recip(
	giant 	d, 
	giant 	r,
	giant 	n
)
/* This is the fastest mod of the present collection.
 * n := n % d, where r is the precalculated
 * steady-state reciprocal of d. */

{
	int		s = (bitlen(r)-1), sign = n->sign;
	giant 	tmp, tmp2;

	if (isZero(d) || (d->sign < 0))
	{
		exit(SIGN);
	}
	
	tmp = popg();
	tmp2 = popg();
	
	n->sign = abs(n->sign);
	while (1) 
	{
		gtog(n, tmp); gshiftright(s-1, tmp);	
		mulg(r, tmp);
		gshiftright(s+1, tmp);
		mulg(d, tmp);
		subg(tmp, n);
		if (gcompg(n,d) >= 0) 
			subg(d,n);
		if (gcompg(n,d) < 0) 
			break;
	}
	if (sign >= 0)
		goto done;
	if (isZero(n))
		goto done; 
	negg(n);
	addg(d,n);
done:
	pushg(2);
	return;
}

#else
void
modg_via_recip(
	giant 	d, 
	giant 	r,
	giant 	n
)
{
	int		s = 2*(bitlen(r)-1), sign = n->sign;
	giant 	tmp, tmp2;

	if (isZero(d) || (d->sign < 0))
	{
		exit(SIGN);
	}

	tmp = popg();
	tmp2 = popg()

	n->sign = abs(n->sign);
	while (1) 
	{
		gtog(n, tmp);	
		mulg(r, tmp);
		gshiftright(s, tmp);
		mulg(d, tmp);
		subg(tmp, n);
		if (gcompg(n,d) >= 0) 
			subg(d,n);
		if (gcompg(n,d) < 0) 
			break;
	}
	if (sign >= 0) 
		goto done;
	if (isZero(n)) 
		goto done;
	negg(n);
	addg(d,n);
done:
	pushg(2);
	return;
}
#endif

void
modg(
	giant 	d,
	giant 	n
)
/* n becomes n%d. n is arbitrary, but the denominator d must be positive! */
{
	if (cur_recip == NULL) {
		cur_recip = newgiant(current_max_size);
		cur_den = newgiant(current_max_size);
		gtog(d, cur_den);
		make_recip(d, cur_recip);
	} else if (gcompg(d, cur_den)) {
		gtog(d, cur_den);
		make_recip(d, cur_recip);
	}
	modg_via_recip(d, cur_recip, n);
}


#if 0
int
feemulmod (
	giant a,
	giant b,
	int q,
	int k
)
/* a becomes (a*b) (mod 2^q-k) where q % 16 == 0 and k is "small" (0 < k < 65535).
 * Returns 0 if unsuccessful, otherwise 1. */
{
	giant			carry, kk, scratch;
	int				i, j;
	int 			asize = abs(a->sign), bsize = abs(b->sign);
	unsigned short 	*aptr,*bptr,*destptr;
	unsigned int	words;
	int				kpower, curk;

	if ((q % 16) || (k <= 0) || (k >= 65535)) {
		return (0);
	}
	
	carry = popg();
	kk = popg();
	scratch = popg();
	
	for (i=0; i<asize+bsize; i++) scratch->n[i]=0;

	words = q >> 4;
	
	bptr = b->n;
	for (i = 0; i < bsize; i++) {
		mult = *bptr++;
		if (mult) {
			kpower = i/words;
			
			if (kpower >= 1) itog (kpower,kk);
			for (j = 1; j < kpower; k++) smulg(kpower,kk);
			
			itog(0,carry);
			
			aptr = a->n;
			for (j = 0; j < bsize; b++) {
				gtog(kk,scratch);
				smulg(*aptr++,scratch);
				smulg(mult,scratch);
				iaddg(*destptr,scratch);
				addg(carry,scratch);
				*destptr++ = scratch->n[0];
				gshiftright(scratch,16);
				gtog(scratch,carry);
				if (destptr - scratch->n >= words) {
					smulg(k, carry);
					smulg(k, kk);
					destptr -= words;
				}
			}
		}
	}

	int 			i,j,m;
	unsigned int 	prod,carry=0;
	int 			asize = abs(a->sign), bsize = abs(b->sign);
	unsigned short 	*aptr,*bptr,*destptr;
	unsigned short	mult;
	int				words, excess;
	int				temp;
	giant			scratch = popg(), scratch2 = popg(), scratch3 = popg();
	short			*carryptr = scratch->n;
	int				kpower,kpowerlimit, curk;

	if ((q % 16) || (k <= 0) || (k >= 65535)) {
		return (0);
	}

	scratch

	for (i=0; i<asize+bsize; i++) scratch->n[i]=0;

	words = q >> 4;
	
	bptr = b->n;
	for (i=0; i<bsize; ++i)
	{
		mult = *bptr++;
		if (mult)
		{
			kpower = i/words;
			aptr = a->n;
			destptr = scratch->n + i;
			
			if (kpower == 0) {
				carry = 0;
			} else if (kpower <= kpowerlimit) {
				carry = 0;
				curk = k;
				for (j = 1; j < kpower; j++) curk *= k;
			} else {
				itog (k,scratch);
				for (j = 1; j < kpower; j++) smulg(k,scratch);
				itog(0,scratch2);
			}
			
			for (j = 0; j < asize; j++) {
				if(kpower == 0) {
					prod = *aptr++ * mult + *destptr + carry;
					*destptr++ = (unsigned short)(prod & 0xFFFF);
					carry = prod >> 16;					
				} else if (kpower < kpowerlimit) {
					prod = kcur * *aptr++;
					temp = prod >> 16;
					prod &= 0xFFFF;
					temp *= mult;
					prod *= mult;
					temp += prod >> 16;
					prod &= 0xFFFF;
					prod += *destptr + carry;
					carry = prod >> 16 + temp;
					*destptr++ = (unsigned short)(prod & 0xFFFF);			
				} else {
					gtog(scratch,scratch3);
					smulg(*aptr++,scratch3);
					smulg(mult,scratch3);
					iaddg(*destptr,scratch3);
					addg(scratch3,scratch2);
					*destptr++ = scratch2->n[0];
					memmove(scratch2->n,scratch2->n+1,2*(scratch2->size-1));
					scratch2->sign--;
				}				
				if (destptr - scratch->n > words) {
					if (kpower == 0) {
						curk = k;
						carry *= k;
					} else if (kpower < kpowerlimit) {
						curk *= k;
						carry *= curk;
					} else if (kpower == kpowerlimit) {
						itog (k,scratch);
						for (j = 1; j < kpower; j++) smulg(k,scratch);
						itog(carry,scratch2);
						smulg(k,scratch2);
					} else {
						smulg(k,scratch);
						smulg(k,scratch2);
					}
					kpower++;
					destptr -= words;
				}
			}
			
			/* Next, deal with the carry term. Needs to be improved to
			handle overflow carry cases. */
			if (kpower <= kpowerlimit) {
				iaddg(carry,scratch);
			} else {
				addg(scratch2,scratch);
			}
			while(scratch->sign > q)
				gtog(scratch,scratch2)
		}
	}
	scratch->sign = destptr - scratch->n;
	if (!carry)
		--(scratch->sign);
	scratch->sign *= gsign(a)*gsign(b);
	gtog(scratch,a);
	pushg(3);
	return (1);
}
#endif

int
idivg(
	int		divisor,
	giant 	theg
)
{
	/* theg becomes theg/divisor. Returns remainder. */
	int 	n;
	int 	base = 1<<(8*sizeof(short));

	n = radixdiv(base,divisor,theg);
	return(n);
}


void
divg(
	giant 	d,
	giant 	n
)
/* n becomes n/d. n is arbitrary, but the denominator d must be positive! */
{
	if (cur_recip == NULL) {
		cur_recip = newgiant(current_max_size);
		cur_den = newgiant(current_max_size);
		gtog(d, cur_den);
		make_recip(d, cur_recip);
	} else if (gcompg(d, cur_den)) {
		gtog(d, cur_den);
		make_recip(d, cur_recip);
	}
	divg_via_recip(d, cur_recip, n);
}


void
powermod(
	giant		x,
	int 		n,
	giant 		g
)
/* x becomes x^n (mod g). */
{
	giant scratch2 = popg();
	gtog(x, scratch2);
	itog(1, x);
	while (n)
	{
		if (n & 1)
		{
			mulg(scratch2, x);
			modg(g, x);
		}
		n >>= 1;
		if (n)
		{
			squareg(scratch2);
			modg(g, scratch2);
		}
	}
	pushg(1);
}


void
powermodg(
	giant		x,
	giant		n,
	giant		g
)
/* x becomes x^n (mod g). */
{
	int 		len, pos;
	giant		scratch2 = popg();

	gtog(x, scratch2);
	itog(1, x);
	len = bitlen(n);
	pos = 0;
	while (1)
	{
		if (bitval(n, pos++))
		{
			mulg(scratch2, x);
			modg(g, x);
		}
		if (pos>=len)
			break;
		squareg(scratch2);
		modg(g, scratch2);
	}
	pushg(1);
}


void
fermatpowermod(
	giant 	x,
	int		n,
	int		q
)
/* x becomes x^n (mod 2^q+1). */
{
	giant scratch2 = popg();
	
	gtog(x, scratch2);
	itog(1, x);
	while (n)
	{
		if (n & 1)
		{
			mulg(scratch2, x);
			fermatmod(q, x);
		}
		n >>= 1;
		if (n)
		{
			squareg(scratch2);
			fermatmod(q, scratch2);
		}
	}
	pushg(1);
}


void
fermatpowermodg(
	giant 	x,
	giant	n,
	int		q
)
/* x becomes x^n (mod 2^q+1). */
{
	int		len, pos;
	giant	scratch2 = popg();

	gtog(x, scratch2);
	itog(1, x);
	len = bitlen(n);
	pos = 0;
	while (1)
	{
		if (bitval(n, pos++))
		{
			mulg(scratch2, x);
			fermatmod(q, x);
		}
		if (pos>=len)
			break;
		squareg(scratch2);
		fermatmod(q, scratch2);
	}
	pushg(1);
}


void
mersennepowermod(
	giant 	x,
	int		n,
	int		q
)
/* x becomes x^n (mod 2^q-1). */
{
	giant scratch2 = popg();

	gtog(x, scratch2);
	itog(1, x);
	while (n)
	{
		if (n & 1)
		{
			mulg(scratch2, x);
			mersennemod(q, x);
		}
		n >>= 1;
		if (n)
		{
			squareg(scratch2);
			mersennemod(q, scratch2);
		}
	}
	pushg(1);
}


void
mersennepowermodg(
	giant 	x,
	giant	n,
	int		q
)
/* x becomes x^n (mod 2^q-1). */
{
	int		len, pos;
	giant	scratch2 = popg();

	gtog(x, scratch2);
	itog(1, x);
	len = bitlen(n);
	pos = 0;
	while (1)
	{
		if (bitval(n, pos++))
		{
			mulg(scratch2, x);
			mersennemod(q, x);
		}
		if (pos>=len)
			break;
		squareg(scratch2);
		mersennemod(q, scratch2);
	}
	pushg(1);
}


void
gshiftleft(
	int				bits,
	giant			g
)
/* shift g left bits bits. Equivalent to g = g*2^bits. */
{
	int 			rem = bits&15, crem = 16-rem, words = bits>>4;
	int 			size = abs(g->sign), j, k, sign = gsign(g);
	unsigned short 	carry, dat;

	if (!bits)
		return;
	if (!size)
		return;
	if (bits < 0) {
		gshiftright(-bits,g);
		return;
	}
	if (size+words+1 > current_max_size) {
		error = OVFLOW;
		exit(error);
	}
	if (rem == 0) {
		memmove(g->n + words, g->n, size * sizeof(short));
		for (j = 0; j < words; j++) g->n[j] = 0;
		g->sign += (g->sign < 0)?(-words):(words);
	} else {
		k = size+words;
		carry = 0;
		for (j=size-1; j>=0; j--) {
			dat = g->n[j];
			g->n[k--] = (unsigned short)((dat >> crem) | carry);
			carry = (unsigned short)(dat << rem);
		}
		do {
			g->n[k--] = carry;
			carry = 0;
		} while(k>=0);
	
		k = size+words;
		if (g->n[k] == 0)
			--k;
		g->sign = sign*(k+1);
	}
}


void
gshiftright(
	int						bits,
	giant					g
)
/* shift g right bits bits. Equivalent to g = g/2^bits. */
{
	register int 			j,size=abs(g->sign);
	register unsigned int 	carry;
	int 					words = bits >> 4;
	int 					remain = bits & 15, cremain = (16-remain);

	if (bits==0)
		return;
	if (isZero(g))
		return;
	if (bits < 0) {
		gshiftleft(-bits,g);
		return;
	}
	if (words >= size) {
		g->sign = 0;
		return;
	}
	if (remain == 0) {
		memmove(g->n,g->n + words,(size - words) * sizeof(short));
		g->sign += (g->sign < 0)?(words):(-words);
	} else {
		size -= words;
	
		if (size)
		{
			for(j=0;j<size-1;++j)
			{
				carry = g->n[j+words+1] << cremain;
				g->n[j] = (unsigned short)((g->n[j+words] >> remain ) | carry);
			}
			g->n[size-1] = (unsigned short)(g->n[size-1+words] >> remain);
		}
	
		if (g->n[size-1] == 0)
			--size;
	
		if (g->sign > 0)
			g->sign = size;
		else
			g->sign = -size;
	}
}


void
extractbits(
	int				n,
	giant			src,
	giant			dest
)
/* dest becomes lowermost n bits of src. Equivalent to dest = src % 2^n. */
{
	register int 	words = n >> 4, numbytes = words*sizeof(short);
	register int 	bits = n & 15;

	if (n<=0)
		return;
	if (words >= abs(src->sign))
		gtog(src,dest);
	else
	{
		memcpy((char *)(dest->n), (char *)(src->n), numbytes);
		if (bits)
		{
			dest->n[words] = (unsigned short)(src->n[words] & ((1<<bits)-1));
			++words;
		}
		while ((dest->n[words-1] == 0) && (words > 0))
		{
			--words;
		}
		if (src->sign<0)
			dest->sign = -words;
		else
			dest->sign = words;
	}
}


int
allzeros(
	int		shorts,
	int		bits,
	giant	g
)
{
	int		i=shorts;

	while (i>0)
	{
		if (g->n[--i])
			return(0);
	}
	return((int)(!(g->n[shorts] & ((1<<bits)-1))));
}


void
fermatnegate(
	int					n,
	giant	 			g
)
/* negate g. g is mod 2^n+1. */
{
	register int		shorts = n>>4,
			 			bits = n & 15,
			 			i = shorts,
			 			mask = 1<<bits;
	register unsigned	carry,temp;

	for (temp=(unsigned)shorts; (int)temp>g->sign-1; --temp)
	{
		g->n[temp] = 0;
	}
	if (g->n[shorts] & mask)
	{                 /* if high bit is set, -g = 1. */
		g->sign = 1;
		g->n[0] = 1;
		return;
	}
	if (allzeros(shorts,bits,g))
		return;       /* if g=0, -g = 0. */

	while (i>0)
	{   --i;
		g->n[i] = (unsigned short)(~(g->n[i+1]));
	}
	g->n[shorts] ^= mask-1;

	carry = 2;
	i = 0;
	while (carry)
	{
		temp = g->n[i]+carry;
		g->n[i++] = (unsigned short)(temp & 0xffff);
		carry = temp>>16;
	}
	while(!g->n[shorts])
	{
		--shorts;
	}
	g->sign = shorts+1;
}


void
mersennemod (
	int n,
	giant g
)
/* g := g (mod 2^n - 1) */
{
	int the_sign, s;
	giant scratch3 = popg(), scratch4 = popg();
	
	if ((the_sign = gsign(g)) < 0) absg(g);
	while (bitlen(g) > n) {
		gtog(g,scratch3);
		gshiftright(n,scratch3);
		addg(scratch3,g);
		gshiftleft(n,scratch3);
		subg(scratch3,g);
	}
	if(!isZero(g)) {
		if ((s = gsign(g)) < 0) absg(g);
		itog(1,scratch3);
		gshiftleft(n,scratch3);
		itog(1,scratch4);
		subg(scratch4,scratch3);
		if(gcompg(g,scratch3) >= 0) subg(scratch3,g);
		if (s < 0) {
			g->sign = -g->sign;
			addg(scratch3,g);
		}
		if (the_sign < 0) {
			g->sign = -g->sign;
			addg(scratch3,g);
		}
	}
	pushg(2);
}

void
fermatmod (
	int 			n,
	giant 			g
)
/* g := g (mod 2^n + 1), */
{
	int the_sign, s;
	giant scratch3 = popg();
	
	if ((the_sign = gsign(g)) < 0) absg(g);
	while (bitlen(g) > n) {
		gtog(g,scratch3);
		gshiftright(n,scratch3);
		subg(scratch3,g);
		gshiftleft(n,scratch3);
		subg(scratch3,g);
	}
        if((bitlen(g) < n) && (the_sign * (g->sign) >= 0)) goto leave;
	if(!isZero(g)) {
		if ((s = gsign(g)) < 0) absg(g);
		itog(1,scratch3);
		gshiftleft(n,scratch3);
		iaddg(1,scratch3);
		if(gcompg(g,scratch3) >= 0) subg(scratch3,g);
		if (s * the_sign < 0) { 
			g->sign = -g->sign;
			addg(scratch3,g);
		}
	}
leave:
	pushg(1);

}

void
fer_mod (
	int 			n,
	giant 			g,
	giant modulus
)
/* Same as fermatmod(), except modulus = 2^n should be passed
if available (i.e. if already allocated and set). */
{
	int the_sign, s;
	giant scratch3 = popg();
	
	if ((the_sign = gsign(g)) < 0) absg(g);
	while (bitlen(g) > n) {
		gtog(g,scratch3);
		gshiftright(n,scratch3);
		subg(scratch3,g);
		gshiftleft(n,scratch3);
		subg(scratch3,g);
	}
        if((bitlen(g) < n) && (the_sign * (g->sign) >= 0)) goto leave;
	if(!isZero(g)) {
		if ((s = gsign(g)) < 0) absg(g);
		if(gcompg(g,modulus) >= 0) subg(modulus,g);
		if (s * the_sign < 0) { 
			g->sign = -g->sign;
			addg(modulus,g);
		}
	}
leave:
	pushg(1);
}


void
smulg(
	unsigned short	i,
	giant 			g
)
/* g becomes g * i. */
{
	unsigned short	carry = 0;
	int				size = abs(g->sign);
	register int 	j,k,mul = abs(i);
	unsigned short 	*digit = g->n;

	for (j=0; j<size; ++j)
	{
		k = *digit * mul + carry;
		carry = (unsigned short)(k>>16);
		*digit = (unsigned short)(k & 0xffff);
		++digit;
	}
	if (carry)
	{
		if (++j >= current_max_size)
		{
			error = OVFLOW;
			exit(error);
		}
		*digit = carry;
	}

	if ((g->sign>0) ^ (i>0))
		g->sign = -j;
	else
		g->sign = j;
}


void
squareg(
	giant 	b
)
/* b becomes b^2. */
{
	auxmulg(b,b);
}


void
mulg(
	giant	a,
	giant	b
)
/* b becomes a*b. */
{
	auxmulg(a,b);
}


void
auxmulg(
	giant		a,
	giant		b
)
/* Optimized general multiply, b becomes a*b. Modes are:
 * AUTO_MUL: switch according to empirical speed criteria.
 * GRAMMAR_MUL: force grammar-school algorithm.
 * KARAT_MUL: force Karatsuba divide-conquer method.
 * FFT_MUL: force floating point FFT method. */
{
	float		grammartime;
	int 		square = (a==b);
	int 		sizea, sizeb;

	switch (mulmode)
	{
		case GRAMMAR_MUL:
			if (square) grammarsquareg(b);
			else grammarmulg(a,b);
			break;
		case FFT_MUL:
			if (square)
				FFTsquareg(b);
			else
				FFTmulg(a,b);
			break;
		case KARAT_MUL:
			if (square) karatsquareg(b);
				else karatmulg(a,b);
			break;
		case AUTO_MUL:
			sizea = abs(a->sign);
			sizeb = abs(b->sign);
		    if((sizea > KARAT_BREAK) && (sizea <= FFT_BREAK) &&
			   (sizeb > KARAT_BREAK) && (sizeb <= FFT_BREAK)){
				if(square) karatsquareg(b);
					else karatmulg(a,b);

			} else {
				grammartime  = (float)sizea; 
				grammartime *= (float)sizeb;
			    if (grammartime < FFT_BREAK * FFT_BREAK)
			    {
				   if (square) grammarsquareg(b);
						else grammarmulg(a,b);
				}
				else
				{
				if (square) FFTsquareg(b);
					else FFTmulg(a,b);
				}
			}
			break;
	}
}

void
justg(giant x) {
	int s = x->sign, sg = 1;
	
	if(s<0) {
		sg = -1;
		s = -s;
	}
	--s;
	while(x->n[s] == 0) {
			--s;
			if(s < 0) break;
	}
	x->sign = sg*(s+1);
}

/* Next, improved Karatsuba routines from A. Powell,
   improvements by G. Woltman. */

void
karatmulg(giant x, giant y)
/* y becomes x*y. */
{
	int s = abs(x->sign), t = abs(y->sign), w, bits,
		sg = gsign(x)*gsign(y);
	giant a, b, c, d, e, f;

	if((s <= KARAT_BREAK) || (t <= KARAT_BREAK)) {
		grammarmulg(x,y);
		return;
	}
	w = (s + t + 2)/4; bits = 16*w;
	a = popg(); b = popg(); c = popg(); 
    d = popg(); e = popg(); f = popg();
	gtog(x,a); absg(a); if (w <= s) {a->sign = w; justg(a);}
	gtog(x,b); absg(b);
	gshiftright(bits, b);
	gtog(y,c); absg(c); if (w <= t) {c->sign = w; justg(c);}
	gtog(y,d); absg(d);
	gshiftright(bits,d);
	gtog(a,e); normal_addg(b,e);	/* e := (a + b) */
	gtog(c,f); normal_addg(d,f);	/* f := (c + d) */
	karatmulg(e,f);			/* f := (a + b)(c + d) */
	karatmulg(c,a);			/* a := a c */
	karatmulg(d,b);			/* b := b d */
	normal_subg(a,f);			
         /* f := (a + b)(c + d) - a c */
	normal_subg(b,f);			
         /* f := (a + b)(c + d) - a c - b d */
	gshiftleft(bits, b);
	normal_addg(f, b);
	gshiftleft(bits, b);
	normal_addg(a, b);
	gtog(b, y); y->sign *= sg;
	pushg(6);
	
	return;
}

void
karatsquareg(giant x)
/* x becomes x^2. */
{
	int s = abs(x->sign), w, bits;
	giant a, b, c;

	if(s <= KARAT_BREAK) {
		grammarsquareg(x);
		return;
	}
	w = (s+1)/2; bits = 16*w;
	a = popg(); b = popg(); c = popg();
	gtog(x, a); a->sign = w; justg(a);
	gtog(x, b); absg(b);
	gshiftright(bits, b);
	gtog(a,c); normal_addg(b,c);
	karatsquareg(c);
	karatsquareg(a);
	karatsquareg(b);
	normal_subg(b, c);
	normal_subg(a, c);
	gshiftleft(bits, b);
	normal_addg(c,b);
	gshiftleft(bits, b);
	normal_addg(a, b);
	gtog(b, x);
	pushg(3);

	return;
}

void
grammarmulg(
	giant			a,
	giant			b
)
/* b becomes a*b. */
{
	int 			i,j;
	unsigned int 	prod,carry=0;
	int 			asize = abs(a->sign), bsize = abs(b->sign);
	unsigned short 	*aptr,*bptr,*destptr;
	unsigned short	mult;
	giant scratch = popg();

	for (i=0; i<asize+bsize; ++i)
	{
		scratch->n[i]=0;
	}

	bptr = &(b->n[0]);
	for (i=0; i<bsize; ++i)
	{
		mult = *(bptr++);
		if (mult)
		{
			carry = 0;
			aptr = &(a->n[0]);
			destptr = &(scratch->n[i]);
			for (j=0; j<asize; ++j)
			{
				prod = *(aptr++) * mult + *destptr + carry;
				*(destptr++) = (unsigned short)(prod & 0xffff);
				carry = prod >> 16;
			}
			*destptr = (unsigned short)carry;
		}
	}
	bsize+=asize;
	if (!carry)
		--bsize;
	scratch->sign = gsign(a)*gsign(b)*bsize;
	gtog(scratch,b);
	pushg(1);
}


void
grammarsquareg (
	giant a
)
/* a := a^2. */
{
	unsigned int	cur_term;
	unsigned int	prod, carry=0, temp;
	int	asize = abs(a->sign), max = asize * 2 - 1;
	unsigned short	*ptr = a->n, *ptr1, *ptr2;
	giant scratch;
	
	if(asize == 0) {
		itog(0,a);
		return;
	}

	scratch = popg();

	asize--;
	
	temp = *ptr;
	temp *= temp;
	scratch->n[0] = temp;
	carry = temp >> 16;
	
	for (cur_term = 1; cur_term < max; cur_term++) {
		ptr1 = ptr2 = ptr;
		if (cur_term <= asize) {
			ptr2 += cur_term;
		} else {
			ptr1 += cur_term - asize;
			ptr2 += asize;
		}
		prod = carry & 0xFFFF;
		carry >>= 16;
		while(ptr1 < ptr2) {
				temp = *ptr1++ * *ptr2--;
				prod += (temp << 1) & 0xFFFF;
				carry += (temp >> 15);
		}
		if (ptr1 == ptr2) {
				temp = *ptr1;
				temp *= temp;
				prod += temp & 0xFFFF;
				carry += (temp >> 16);
		}
		carry += prod >> 16;
		scratch->n[cur_term] = (unsigned short) (prod);
	}
	if (carry) {
		scratch->n[cur_term] = carry;
		scratch->sign = cur_term+1;
	} else scratch->sign = cur_term;
	
	gtog(scratch,a);
	pushg(1);
}


/**************************************************************
 *
 * FFT multiply Functions
 *
 **************************************************************/

int 		initL = 0;

int
lpt(
	int	  			n,
	int				*lambda
)
/* Returns least power of two greater than n. */
{
	register int	i = 1;

	*lambda = 0;
	while (i<n)
	{
		i<<=1;
		++(*lambda);
	}
	return(i);
}


void
addsignal(
	giant 				x,
	double 				*z,
	int 				n
)
{
	register int 		j, k, m, car, last;
	register double 	f, g,err;

	maxFFTerror = 0;
    last = 0;
	for (j=0;j<n;j++)
	{
		f = gfloor(z[j]+0.5);
        if(f != 0.0) last = j;
		if (checkFFTerror)
		{
			err = fabs(f - z[j]);
			if (err > maxFFTerror)
				maxFFTerror = err;
		}
		z[j] =0;
		k = 0;
		do
		{
			g = gfloor(f*TWOM16);
			z[j+k] += f-g*TWO16;
			++k;
			f=g;
		} while(f != 0.0);
	}
	car = 0;
	for(j=0;j < last + 1;j++)
	{
		m = (int)(z[j]+car);
		x->n[j] = (unsigned short)(m & 0xffff);
		car = (m>>16);
	}
	if (car)
		x->n[j] = (unsigned short)car;
	else
		--j;

	while(!(x->n[j])) --j;

	x->sign = j+1;
}


void
FFTsquareg(
	giant 			x
)
{
	int 			j,size = abs(x->sign);
	register int 	L;

	if (size<4)
	{
		grammarmulg(x,x);
		return;
	}
	L = lpt(size+size, &j);
	if(!z) z = (double *)malloc(MAX_SHORTS * sizeof(double));
	giant_to_double(x, size, z, L);
	fft_real_to_hermitian(z, L);
	square_hermitian(z, L);
	fftinv_hermitian_to_real(z, L);
	addsignal(x,z,L);
	x->sign = abs(x->sign);
}


void
FFTmulg(
	giant			y,
	giant			x
)
{
	/* x becomes y*x. */
	int 			lambda, sizex = abs(x->sign), sizey = abs(y->sign);
	int 			finalsign = gsign(x)*gsign(y);
	register int	L;

	if ((sizex<=4)||(sizey<=4))
	{
		grammarmulg(y,x);
		return;
	}
	L = lpt(sizex+sizey, &lambda);
	if(!z) z = (double *)malloc(MAX_SHORTS * sizeof(double));
	if(!z2) z2 = (double *)malloc(MAX_SHORTS * sizeof(double));

	giant_to_double(x, sizex, z, L);
	giant_to_double(y, sizey, z2, L);
	fft_real_to_hermitian(z, L);
	fft_real_to_hermitian(z2, L);
	mul_hermitian(z2, z, L);
	fftinv_hermitian_to_real(z, L);
	addsignal(x,z,L);
	x->sign = finalsign*abs(x->sign);
}


void
scramble_real(
	double 			*x,
	int 			n
)
{
	register int 	i,j,k;
	register double	tmp;

	for (i=0,j=0;i<n-1;i++)
	{
		if (i<j)
		{
			tmp = x[j];
			x[j]=x[i];
			x[i]=tmp;
		}
		k = n/2;
		while (k<=j)
		{
			j -= k;
			k>>=1;
		}
		j += k;
	}
}


void
fft_real_to_hermitian(
	double 			*z,
	int 			n
)
/* Output is {Re(z^[0]),...,Re(z^[n/2),Im(z^[n/2-1]),...,Im(z^[1]).
 *	This is a decimation-in-time, split-radix algorithm.
 */
{
	register double	cc1, ss1, cc3, ss3;
	register int 	is, id, i0, i1, i2, i3, i4, i5, i6, i7, i8,
					a, a3, b, b3, nminus = n-1, dil, expand;
	register double *x, e;
	int 			nn = n>>1;
	double 			t1, t2, t3, t4, t5, t6;
	register int 	n2, n4, n8, i, j;

	init_sinCos(n);
	expand = cur_run/n;
	scramble_real(z, n);
	x = z-1; /* FORTRAN compatibility. */
	is = 1;
	id = 4;
	do
	{
		for (i0=is;i0<=n;i0+=id)
		{
			i1 = i0+1;
			e = x[i0];
			x[i0] = e + x[i1];
			x[i1] = e - x[i1];
		}
		is = (id<<1)-1;
		id <<= 2;
	} while(is<n);

	n2 = 2;
	while(nn>>=1)
	{
		n2 <<= 1;
		n4 = n2>>2;
		n8 = n2>>3;
		is = 0;
		id = n2<<1;
		do
		{
			for (i=is;i<n;i+=id)
			{
				i1 = i+1;
				i2 = i1 + n4;
				i3 = i2 + n4;
				i4 = i3 + n4;
				t1 = x[i4]+x[i3];
				x[i4] -= x[i3];
				x[i3] = x[i1] - t1;
				x[i1] += t1;
				if (n4==1)
					continue;
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
			is = (id<<1) - n2;
			id <<= 2;
		} while(is<n);
		dil = n/n2;
		a = dil;
		for (j=2;j<=n8;j++)
		{
			a3 = (a+(a<<1))&nminus;
			b = a*expand;
			b3 = a3*expand;
			cc1 = s_cos(b);
			ss1 = s_sin(b);
			cc3 = s_cos(b3);
			ss3 = s_sin(b3);
			a = (a+dil)&nminus;
			is = 0;
			id = n2<<1;
			do
			{
				for(i=is;i<n;i+=id)
				{
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
				is = (id<<1) - n2;
				id <<= 2;
			} while(is<n);
		}
	}
}


void
fftinv_hermitian_to_real(
	double 				*z,
	int 				n
)
/* Input is {Re(z^[0]),...,Re(z^[n/2),Im(z^[n/2-1]),...,Im(z^[1]).
 * This is a decimation-in-frequency, split-radix algorithm.
 */
{
	register double 	cc1, ss1, cc3, ss3;
	register int 		is, id, i0, i1, i2, i3, i4, i5, i6, i7, i8,
						a, a3, b, b3, nminus = n-1, dil, expand;
	register double 	*x, e;
	int 				nn = n>>1;
	double 				t1, t2, t3, t4, t5;
	int 				n2, n4, n8, i, j;

	init_sinCos(n);
	expand = cur_run/n;
	x = z-1;
	n2 = n<<1;
	while(nn >>= 1)
	{
		is = 0;
		id = n2;
		n2 >>= 1;
		n4 = n2>>2;
		n8 = n4>>1;
		do
		{
			for(i=is;i<n;i+=id)
			{
				i1 = i+1;
				i2 = i1 + n4;
				i3 = i2 + n4;
				i4 = i3 + n4;
				t1 = x[i1] - x[i3];
				x[i1] += x[i3];
				x[i2] += x[i2];
				x[i3] = t1 - 2.0*x[i4];
				x[i4] = t1 + 2.0*x[i4];
				if (n4==1)
					continue;
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
			is = (id<<1) - n2;
			id <<= 2;
		} while (is<n-1);
		dil = n/n2;
		a = dil;
		for (j=2;j<=n8;j++)
		{
			a3 = (a+(a<<1))&nminus;
			b = a*expand;
			b3 = a3*expand;
			cc1 = s_cos(b);
			ss1 = s_sin(b);
			cc3 = s_cos(b3);
			ss3 = s_sin(b3);
			a = (a+dil)&nminus;
			is = 0;
			id = n2<<1;
			do
			{
				for(i=is;i<n;i+=id)
				{
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
				is = (id<<1) - n2;
				id <<= 2;
			} while(is<n-1);
		}
	}
	is = 1;
	id = 4;
	do
	{
		for (i0=is;i0<=n;i0+=id)
		{
			i1 = i0+1;
			e = x[i0];
			x[i0] = e + x[i1];
			x[i1] = e - x[i1];
		}
		is = (id<<1) - 1;
		id <<= 2;
	} while(is<n);
	scramble_real(z, n);
	e = 1/(double)n;
	for (i=0;i<n;i++)
	{
		z[i] *= e;
	}
}


void
mul_hermitian(
	double 				*a,
	double 				*b,
	int 				n
)
{
	register int 		k, half = n>>1;
	register double 	aa, bb, am, bm;

	b[0] *= a[0];
	b[half] *= a[half];
	for (k=1;k<half;k++)
	{
		aa = a[k];
		bb = b[k];
		am = a[n-k];
		bm = b[n-k];
		b[k] = aa*bb - am*bm;
		b[n-k] = aa*bm + am*bb;
	}
}


void
square_hermitian(
	double 				*b,
	int 				n
)
{
	register int 		k, half = n>>1;
	register double 	c, d;

	b[0] *= b[0];
	b[half] *= b[half];
	for (k=1;k<half;k++)
	{
		c = b[k];
		d = b[n-k];
		b[n-k] = 2.0*c*d;
		b[k] = (c+d)*(c-d);
	}
}


void
giant_to_double
(
	giant 			x,
	int 			sizex,
	double 			*z,
	int 			L
)
{
	register int 	j;

	for (j=sizex;j<L;j++)
	{
		z[j]=0.0;
	}
	for (j=0;j<sizex;j++)
	{
		 z[j] = x->n[j];
	}
}


void
gswap(
	giant 	*p,
	giant 	*q
)
{
	giant 	t;

	t = *p;
	*p = *q;
	*q = t;
}


void
onestep(
	giant 		x,
	giant 		y,
	gmatrix 	A
)
/* Do one step of the euclidean algorithm and modify
 * the matrix A accordingly. */
{
	giant s4 = popg();

	gtog(x,s4);
	gtog(y,x);
	gtog(s4,y);
	divg(x,s4);
	punch(s4,A);
	mulg(x,s4);
	subg(s4,y);
	
	pushg(1);
}


void
mulvM(
	gmatrix 	A,
	giant 		x,
	giant 		y
)
/* Multiply vector by Matrix; changes x,y. */
{
	giant s0 = popg(), s1 = popg();
	
	gtog(A->ur,s0);
	gtog( A->lr,s1);
	dotproduct(x,y,A->ul,s0);
	dotproduct(x,y,A->ll,s1);
	gtog(s0,x);
	gtog(s1,y);
	
	pushg(2);
}


void
mulmM(
	gmatrix 	A,
	gmatrix 	B
)
/* Multiply matrix by Matrix; changes second matrix. */
{
	giant s0 = popg();
	giant s1 = popg();
	giant s2 = popg();
	giant s3 = popg();
	
	gtog(B->ul,s0);
	gtog(B->ur,s1);
	gtog(B->ll,s2);
	gtog(B->lr,s3);
	dotproduct(A->ur,A->ul,B->ll,s0);
	dotproduct(A->ur,A->ul,B->lr,s1);
	dotproduct(A->ll,A->lr,B->ul,s2);
	dotproduct(A->ll,A->lr,B->ur,s3);
	gtog(s0,B->ul);
	gtog(s1,B->ur);
	gtog(s2,B->ll);
	gtog(s3,B->lr);
	
	pushg(4);
}


void
writeM(
	gmatrix 	A
)
{
	printf("    ul:");
	gout(A->ul);
	printf("    ur:");
	gout(A->ur);
	printf("    ll:");
	gout(A->ll);
	printf("    lr:");
	gout(A->lr);
}


void
punch(
	giant 		q,
	gmatrix 	A
)
/* Multiply the matrix A on the left by [0,1,1,-q]. */
{
	giant s0 = popg();
	
	gtog(A->ll,s0);
	mulg(q,A->ll);
	gswap(&A->ul,&A->ll);
	subg(A->ul,A->ll);
	gtog(s0,A->ul);
	gtog(A->lr,s0);
	mulg(q,A->lr);
	gswap(&A->ur,&A->lr);
	subg(A->ur,A->lr);
	gtog(s0,A->ur);

	pushg(1);
}


static void
dotproduct(
	giant 	a,
	giant 	b,
	giant 	c,
	giant 	d
)
/* Replace last argument with the dot product of two 2-vectors. */
{
	giant s4 = popg();

	gtog(c,s4);
	mulg(a, s4);
	mulg(b,d);
	addg(s4,d);
	
	pushg(1);
}


void
ggcd(
	giant 	xx,
	giant 	yy
)
/* A giant gcd.  Modifies its arguments. */
{
	giant 	x = popg(), y = popg();
	gmatrix 	A = newgmatrix();

	gtog(xx,x); gtog(yy,y);
	for(;;)
	{
		fix(&x,&y);
		if (bitlen(y) <= GCDLIMIT )
			break;
		A->ul = popg();
		A->ur = popg();
		A->ll = popg();
		A->lr = popg();
		itog(1,A->ul);
		itog(0,A->ur);
		itog(0,A->ll);
		itog(1,A->lr);
		hgcd(0,x,y,A);
		mulvM(A,x,y);
		pushg(4);
		fix(&x,&y);
		if (bitlen(y) <= GCDLIMIT )
			break;
		modg(y,x);
		gswap(&x,&y);
	}
	bgcdg(x,y);
	gtog(y,yy);
	pushg(2);
	free(A);
}


void
fix(
	giant 	*p,
	giant 	*q
)
/* Insure that x > y >= 0. */
{
	if( gsign(*p) < 0 )
		negg(*p);
	if( gsign(*q) < 0 )
		negg(*q);
	if( gcompg(*p,*q) < 0 )
		gswap(p,q);
}


void
hgcd(
	int 		n,
	giant	 	xx,
	giant 		yy,
	gmatrix 	A
)
/* hgcd(n,x,y,A) chops n bits off x and y and computes th
 * 2 by 2 matrix A such that A[x y] is the pair of terms
 * in the remainder sequence starting with x,y that is
 * half the size of x. Note that the argument A is modified
 * but that the arguments xx and yy are left unchanged.
 */
{
	giant 		x, y;

	if (isZero(yy))
		return;
	
	x = popg();
	y = popg();
	gtog(xx,x);
	gtog(yy,y);
	gshiftright(n,x);
	gshiftright(n,y);
	if (bitlen(x) <= INTLIMIT )
	{
		shgcd(gtoi(x),gtoi(y),A);
	}
	else
	{
		gmatrix 	B = newgmatrix();
		int 		m = bitlen(x)/2;

		hgcd(m,x,y,A);
		mulvM(A,x,y);
		if (gsign(x) < 0)
		{
			negg(x); negg(A->ul); negg(A->ur);
		}
		if (gsign(y) < 0)
		{
			negg(y); negg(A->ll); negg(A->lr);
		}
		if (gcompg(x,y) < 0)
		{
			gswap(&x,&y);
			gswap(&A->ul,&A->ll);
			gswap(&A->ur,&A->lr);
		}
		if (!isZero(y))
		{
			onestep(x,y,A);
			m /= 2;
			B->ul = popg();
			B->ur = popg();
			B->ll = popg();
			B->lr = popg();
			itog(1,B->ul);
			itog(0,B->ur);
			itog(0,B->ll);
			itog(1,B->lr);
			hgcd(m,x,y,B);
			mulmM(B,A);
			pushg(4);
		}
		free(B);
	}
	pushg(2);
}


void
shgcd(
	register int	x,
	register int 	y,
	gmatrix 		A
)
/*
 * Do a half gcd on the integers a and b, putting the result in A
 * It is fairly easy to use the 2 by 2 matrix description of the
 * extended Euclidean algorithm to prove that the quantity q*t
 * never overflows.
 */
{
	register int 	q, t, start = x;
	int 			Aul = 1, Aur = 0, All = 0, Alr = 1;

	while	(y != 0 && y > start/y)
	{
		q = x/y;
		t = y;
		y = x%y;
		x = t;
		t = All;
		All = Aul-q*t;
		Aul = t;
		t = Alr;
		Alr = Aur-q*t;
		Aur = t;
	}
	itog(Aul,A->ul);
	itog(Aur,A->ur);
	itog(All,A->ll);
	itog(Alr,A->lr);
}
