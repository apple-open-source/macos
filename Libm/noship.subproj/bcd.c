/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/////////////////////////////////////////////////////////////////////////
// File: bcd.c
// void num2dec(const decform *f,double x,decimal *d);
// double dec2num(const decimal *d);
// void dec2str(const decform *f,const decimal *d,char *s);
// void str2dec(const char *s,short *ix,decimal *d,short *vp);
// Status: BETA
// Copyright Apple Computer, Inc. 1984, 1985, 1990, 1991, 1992, 1993, 1994, 1996
//
// Written by Kenton Hanson, 1991 - 1993
//
// Modification History:
//		22 Jun 92  KLH	Released to Danny Kubota for inclusion in the debugger
//						Needs tens tables hard coded and tens buffer removed.
//
//		10 Aug 92  KLH  Bug fixes for rounding logic and denormalization shifting.
//
//		25 Aug 92  KLH  Added tens table, and comments.
//
//		27 Aug 92  KLH  Rounding logic, adivb2c (index problem), and ++i bug fix.
//
//		28 Aug 92  JPO  Another ++i bug, eliminate unnormal zeros.
//				   KLH  Fix tens power table and generator.
//
//		02 Sep 92  JPO  Corrections to classextended & biggetsig.
//
//		24 Sep 92  JPO  Corrections to axb2c
//
//		29 Jan 93  KLH  started converison for powerpc
//
//		13 May 93  KLH  long double part split out into file "dec2bin2L.c"
//
//		31 Aug 93  KLH  fixed ext2big to handle denormal numbers
//
//		20 Oct 93  KLH  Fixed bug in num2dec, inexact flag not getting cleared.
//						Fixed a mask in big2short.
//
//      04 Nov 93  KLH  Set inexact correctly when overflow occurs
//
//		23 May 94  KLH  changed num2dec to match universal header fp.h.
//						Cast many (unsigned char*) to (char*) to conform to
//						IBM's latest RS6000 C++ compiler.
//
//      05 Mar 96  KLH	Changed SIGSIZEDIV from 4 to 5 to better handle directed
//						rounding cases for double double.
//
//		21 Aug 96  PAF  changed str2dec to allow more than SIGDIGLEN significant
//						digits in input string.
//
//		03 Sep 96  KLH  Added subroutine excessDigits (called from Str2Dec)
//						to handle extra digits as prescribed by NCEG spec.
//
//		24 Sep 96  KLH  Added code to Str2Dec to better handle overflow exponent
//						digits and excessive ( > 8192) leading & trailing zeros.
//
//		27 Sep 96  KLH  Lead ".." in str2dec fixed.
//
//		08 Oct 96  KLH  Corrected "(*expadjust)++" statement in excessDigits.
//						Error found by Scott Fraser.
//
/////////////////////////////////////////////////////////////////////////
#include	"MyStdLib.h"
#include	"MyString.h"
#include	"math.h"
#include 	"fp_private.h"
#include	"fenv.h"
#include	"bcd.h"

/*  Rounding Precisions */
// Special internal Rounding precision for rounding to integral values 
// and not flagging underflow or overflow

#define INTPRECISION ((roundpre)(3))




#define	SIGSIZEDIV	5	// deliver 160 good bits


#define	SIGDIVQOUT	SIGSIZE+SIGSIZEDIV

struct parts {			// parts of an double
	long highsig, lowsig;
};

typedef struct parts parts;

typedef union	{
		double extval;
		struct parts intval;
			} eparts;
			
typedef union	{
		unsigned long along;
		float flt;
			} fparts;
			
struct prod {			// useful struct for big arithmetic
	unsigned long high;
	unsigned long low;
};

typedef struct prod prod;




static void excessDigits (decimal *d, char rndChar, int excess, long *expadjust);
long apml ( double x );
double tenpower (const long n);
double dec2d ( const decimal *d );
double big2d ( int sgn, big *b );
float big2float ( int sgn, big *b );
long big2long ( int sgn, big *b );
short big2short ( int sgn, big *b );
void dec2big ( const decimal *d, big *b );

void getnan ( double x, decimal *d );
void bigb2d ( double x, long scale, decimal *d );
void ext2big ( double x, big *b );
unsigned long bigshifter ( big *a, big *d, int n);
void addbigs ( big *a, big *b, big *c );
void subbigs ( big *a, big *b, big *c );


void getsig ( double x, decimal *d );	// not used in debugger version 


// support functions for adivb2c
int divuL (unsigned long dvsr, unsigned long *dvdnd);
struct prod muluL (unsigned long a, unsigned long b);
int subDL (prod a, unsigned long *b);
void subL (unsigned long *a, unsigned long *b, int length);		// b -= a
int addL (unsigned long *a, unsigned long *b, int length);		// b += a
int firstdiff (unsigned long *a, unsigned long *b, int length);


/////////////////////////////////////////////////////////////////////////
// classic SANE num2dec except all in c and destination is
// wider decimal (see above)
/////////////////////////////////////////////////////////////////////////

void num2dec( const decform *f, double_t x, decimal *d ) {	//	<5/23/94 klh>
decform df;
long logx, len, scale;

long int rnd;
fenv_t env;
double y, tens;
int dummy;

	d->sgn = (char) __signbitd (x);
	switch ( __fpclassifyd ( x ) ) {
#if 0
		case FP_SNAN:
			feraiseexcept (FE_INVALID);

		case FP_QNAN:
			getnan (x, d);
			break;
#else
                case FP_NAN: // C99 doesn't provide signalling NAN's
                {
                    hexdouble z;
                    z.d = x;
                    if (!( z.i.hi & dQuietNan ))
			feraiseexcept (FE_INVALID);
                    getnan (x, d);
                    break;
               }
#endif
		case FP_INFINITE:
			d->sig.length = 1;			// pascal type length
			d->sig.text [0] = 'I';
			d->sig.text [1] = 0;		//  null byte
			break;
		case FP_ZERO:
			d->sig.length = 1;			// pascal type length
			d->sig.text [0] = '0';
			d->sig.text [1] = 0;		//  null byte
			break;
		case FP_NORMAL:
		case FP_SUBNORMAL:
			rnd = fegetround ();	// need this for rounding algorithm
			dummy = feholdexcept (&env);	//	Does not change the current 
											//	rounding direction mode.
			df = *f;
			logx = apml (x);
			if ((df.style == FLOATDECIMAL) && (df.digits < 1)) df.digits = 1;
			do {
				if (df.style == FLOATDECIMAL) len = df.digits;
				else len = df.digits + logx + 1;
				if (len > MAXDIGITS) len = MAXDIGITS;
				scale = len - logx - 1;
				if (len < 1) len = 1;
				feclearexcept (FE_INEXACT);
				if (labs (scale) > MAXTEN) bigb2d (x, scale, d); // use big arith
				else {											 // try double
					tens = tenpower (scale);
					dummy = fesetround (FE_TOWARDZERO);		// need chop rounding
					if (scale < 0) y = x / tens;
					else if (scale > 0) y = x * tens;
					else y = x;
					
//-----------------------------------------------------------------------------
//	If 	1) the scaling operation is exact, or 
//		2) the integer string embedded in y and the inexact flag contain enough 
//		   information to deliver a correctly rounded AASCI string
//	then call "getsig" to construct the "decimal" record
//	otherwise call "bigb2d" to redo the scaling and building of "decimal"
//	
//	Note, 	a) "bigb2d" uses big arithmetic
//			b)	(logb (y) <= 62) is equivalent to condition 2 above for extended
//				(logb (y) <= 51) is equivalent to condition 2 above	for double
//-----------------------------------------------------------------------------
		
					
					dummy = fesetround (rnd);	// need this for rounding algorithm
					if (fetestexcept (FE_INEXACT) && (logb (y) > 51)) {
						feclearexcept (FE_INEXACT);			//	<10/20/93 klh>
						bigb2d (x, scale, d);	//	need big arith
					}
					else getsig (y, d);		// 	usual case, extract sig from double
					}
				logx++;
			} while (d->sig.length > len);
			feupdateenv (&env);
			d->exp = (short) -scale;
			break;
	}
}

/////////////////////////////////////////////////////////////////////////
// classic SANE dec2num except all in c and input is
// wider decimal (see above)
//
// This routine handles all special cases, ZERO, NaNs, infinities, all
// other cases requiring conversions are passed onto dec2x 
//
// decimal is assumed to be pascal type string, i.e., byte length,
// followed by a character array (not necessarily NULL terminated.
/////////////////////////////////////////////////////////////////////////

double dec2num (const decimal *d) {
eparts s;
int i, i2, hex;

	if ((d->sig.text [0] == '0') || (d->sig.length <= 0)) {	//	return Zero
		s.intval.highsig = (d->sgn) ? 0x80000000 : 0 ;
		s.intval.lowsig = 0;
		return s.extval;
	}
	else if (d->sig.text [0] == 'I') {	//	return Infinity
		s.intval.highsig = (d->sgn) ? 0xfff00000 : 0x7ff00000 ;
		s.intval.lowsig = 0;
		return s.extval;
	}
	else if (d->sig.text [0] == 'N') {	//	return NAN
		i2 = (d->sig.length < 5) ? d->sig.length + 3 : 8;
		s.intval.highsig = 0;
		for (i = 1; i <= i2; i++) {
			s.intval.highsig <<= 4;
			hex = (i < d->sig.length) ? d->sig.text [i] - '0' : 0 ;
			if (hex > 9) hex += 9;		//	translate a..f, A..F to numerical value
			hex &= 0x000f;				//	true nibble value
			s.intval.highsig |= hex;
		}
			//	check for zero nan code ala 68k software style
		if (!(s.intval.highsig &= 0x3fffffff)) s.intval.highsig = 0x00150000;  // klh 8/27/92
		s.intval.highsig |= 0x40000000;	// set quiet bit
		
		s.intval.lowsig = s.intval.highsig << 23;
		s.intval.highsig = 0x7ff00000 | (s.intval.highsig >> 11);
		if (d->sgn) s.intval.highsig |= 0x80000000;
		return s.extval;
	}
	else  {
		return dec2d ( d );						// non zero real value
	}
}
 


/////////////////////////////////////////////////////////////////////////
// dec2f is similar to dec2num except a float is returned.
/////////////////////////////////////////////////////////////////////////

float dec2f ( const decimal *d ) {
int i, i2, hex;
fparts y;
big b;

	if ((d->sig.text [0] == '0') || (d->sig.length <= 0)) {	//	return Zero
		y.along = (d->sgn) ? 0x80000000 : 0 ;
		return y.flt;
	}
	else if (d->sig.text [0] == 'I') {	//	return Infinity
		y.along = (d->sgn) ? 0xff800000 : 0x7f800000 ;
		return y.flt;
	}
	else if (d->sig.text [0] == 'N') {	//	return NAN
		i2 = (d->sig.length < 3) ? d->sig.length + 3 : 6;
		y.along = 0;
		for (i = 1; i <= i2; i++) {
			y.along <<= 4;
			hex = (i < d->sig.length) ? d->sig.text [i] - '0' : 0 ;
			if (hex > 9) hex += 9;		//	translate a..f, A..F to numerical value
			hex &= 0x000f;				//	true nibble value
			y.along |= hex;
		}
		
			//	check for zero nan code ala 68k software style
		if (!(y.along &= 0x003fffff)) y.along = 0x00001500;
		y.along |= 0x7fc00000;	// set exponent and quiet bit
		if (d->sgn) y.along |= 0x80000000;
		return y.flt;
	}
	else  {
		dec2big (d, &b);
		return big2float ( (int) d->sgn, &b );
	}
}			//	dec2f

/////////////////////////////////////////////////////////////////////////
// dec2big  transforms a decimal record into a big
//	 assumes special cases have been filtered out
//
/////////////////////////////////////////////////////////////////////////

void dec2big ( const decimal *d, big *b ) {
big a;
long i, i2, j, j2;
int sticky;

	j2 = 0;						// initialize big digit string length
	a.sig.lng [j2] = 0;			// initialize big digit string	
	i2 = d->sig.length - 1;		// set i2 to AASCI string length
	for (i = 0; i <= i2; i++) {	// build the big significand
		a.sig.lng [0] += (d->sig.text [i] - '0');	// translate AASCI to binary
	// if not last chracter multiple by 10
		if (i < i2) for (j = 0; j <= j2; j++) a.sig.lng [j] *= 10;
		for (j = 0; j <= j2; j++) {
			if (a.sig.lng [j] & 0xffff0000) {	// test for carry(s)
				if (j == j2) {	// if last digit overflowed expand binary digit
					j2++;
					a.sig.lng [j2] = 0;
				}
				a.sig.lng [j + 1] += ((a.sig.lng [j] >> 16) & 0x0000ffff); // add carry(s)
				a.sig.lng [j] &= 0x0000ffff;		// mask out carry(s)
			}
		}
	}

	for (i = 0; i < SIGSIZE; i++) b->sig.lng [i] = 0;
	// Reverse the order
	for (j = 0; j <= j2; j++) b->sig.shrt [j] = (short) a.sig.lng [j2 - j];
	b->exp = 16*j2 + 15;
	// Normalize it
	j2 /= 2;
	while ((b->sig.lng [0] & 0x80000000) == 0) {		// normalizing loop
		b->exp--;
		for (j = 0; j < j2; j++) {
			b->sig.lng [j] <<= 1;
			if ((b->sig.lng [j + 1] & 0x80000000) != 0)
				b->sig.lng [j]++;
		}
		b->sig.lng [j2] <<= 1;
	}
		
			//	check to see if sig fits exactly into double
	
	sticky = (b->sig.lng [1] & 0x000007ff) != 0;		
	i = 2;
	while ((!sticky) && (i < SIGSIZE)) sticky = b->sig.lng [i++] != 0;	// JPO 8/28/92
	
	if (d->exp < 0) {						// scale by d->exp
		bigtenpower (d->exp, &a);
		adivb2c (b, &a, b); 
	}
	else if (d->exp > 0) {
		bigtenpower (d->exp, &a);
		axb2c ( b, &a, b, FALSE ); 
	}
}

/////////////////////////////////////////////////////////////////////////
// dec2s is similar to dec2num except a short is returned.
/////////////////////////////////////////////////////////////////////////

short dec2s ( const decimal *d ) {		//	short
big b;

	if ((d->sig.text [0] == '0') || (d->sig.length <= 0)) {	//	return Zero
		return 0;
	}
	else if ((d->sig.text [0] == 'I') || (d->sig.text [0] == 'N')) {
		feraiseexcept (FE_INVALID);
		return 0x8000;
	}
	else  {
		dec2big (d, &b);
		return big2short ( (int) d->sgn, &b );
	}
}

/////////////////////////////////////////////////////////////////////////
// dec2l is similar to dec2num except a long is returned.
/////////////////////////////////////////////////////////////////////////

long dec2l ( const decimal *d ) {		//	long
big b;

	if ((d->sig.text [0] == '0') || (d->sig.length <= 0)) {	//	return Zero
		return 0;
	}
	else if ((d->sig.text [0] == 'I') || (d->sig.text [0] == 'N')) {
		feraiseexcept (FE_INVALID);
		return 0x80000000;
	}
	else  {
		dec2big (d, &b);
		return big2long ( (int) d->sgn, &b );
	}
}


 
/////////////////////////////////////////////////////////////////////////
// classic SANE dec2str except all in c and input is
// wider decimal (see above)
/////////////////////////////////////////////////////////////////////////

void dec2str(const decform *f,const decimal *d,char *s) {
unsigned char ExpDigs [9];
int i, i0, n = 0, exp, k;
ldiv_t qr;

	n = strlen ((char*) d->sig.text);
	if (n > SIGDIGLEN) {
		s = strcpy (s, "NAN(017)");	// NaN code for invalid AASCI to binary
		return;
	}
	s = strncpy (s, (char*) d->sig.text, n);
	s [n] = '\0';
	exp = d->exp;			

	if (s [0] == '?') s = strcpy (s, "?");
	else {
		if (s [0] == '0') {		// Fix up d if 0.
			s = strcpy (s, "0");
			exp = 0;			// protects against garbage in exponent <klh 3/9/93>
		}
		if (s [0] == 'I') s = strcpy (s, "INF");
		else {
			if (s [0] == 'N') {
				i = strlen (s) - 1;
				if (i > 4) i = 4;
				i0 = i - 1;
				if ( i0 < 1) i0 = 1;
				qr.quot = 0;
				while (i >= i0) {
					qr.quot *= 16;
					qr.quot += (s [i0] - '0');
					if (s [i0] > '9') qr.quot += ('0' - 'A' + 10);
					i0++;
				};				
				s = strcpy (s, "NAN(XXX)");
				for (i = 6; i >= 4; i--) {
					qr = ldiv (qr.quot, 10);
					s [i] = (unsigned char) ('0' + qr.rem);
				}
			}
			else {
				if (f->style == FLOATDECIMAL) { 	//	Set n = # digits to display.
					if (f->digits > (DECSTROUTLEN - 8)) n = DECSTROUTLEN - 8;
					else {
						if (f->digits < d->sig.length) n = d->sig.length;
						else n = f->digits;
					}
							//       Pad with zeros to honor n.
					for (i = d->sig.length + 1; i <= n; i++) s = strcat (s, "0");
					if (n > 1) {
						k = strlen (s);
						memmove (&s[2], &s[1], k);
						s [1] = '.';
					}
					i = abs(exp + d->sig.length - 1);  // Get exponent digits
					ExpDigs [0] = 0;	// i.e., NULL string
					do {
						qr = ldiv (i, 10);
						k = strlen ((char*) ExpDigs) + 1;
						memmove ((char*) &ExpDigs [1], (char*) &ExpDigs [0], k);
						ExpDigs [0] = (unsigned char) ('0' + qr.rem);
						i = qr.quot;
					} while (i > 0);
					if ((exp + d->sig.length) < 1) s = strcat (s, "e-");
					else s = strcat (s, "e+");
					s = strcat (s, (char*) ExpDigs);
				}
				else {	//	BEGIN {FixedDecimal}
//	Set n = # digits to right of point to be displayed.
					n = (f->digits < (-exp)) ? - exp : f->digits;
// 'D---D0---0' case
					if (n < 1) {
                        if ((d->sig.length + exp + 1) <= DECSTROUTLEN) {
							  for (i = 1; i <= exp; i++) s = strcat(s, "0");
						}
                        else s = strcpy (s, "?");
					}
//	n >= 1 cases
					else {
						if ((d->sig.length + exp) > 0) {
//	'D---D.D---D0---0' case
							if ((d->sig.length + exp + n + 2) <= DECSTROUTLEN) {
								for (i = 1; i <= (exp + n); i++) s = strcat(s, "0");
								k = strlen (s);
								i = k - n;
								memmove (&s[i + 1], &s[i], n + 1);
								s [i] = '.';
							}
							else s = strcpy (s, "?");
						}
//	'0.0---0D---D0---0' case
						else {
							if ((n + 3) <= DECSTROUTLEN) {
							for (i = d->sig.length + 1; i <= (-exp); i++) {
								k = strlen (s) + 1;
								memmove (&s[1], &s[0], k);
								s [0] = '0';
							}
							for (i = - exp + 1; i <= n; i++) s = strcat(s, "0");
							k = strlen (s) + 1;
							memmove (&s[2], &s[0], k);
							s [0] = '0';
							s [1] = '.';
							}
							else s = strcpy (s, "?");
						}
					}
				} 			//	END; { FixedDecimal }
			}
		}
	}
	if (s [0] != '?') {
		if ((d->sgn == 1) || (f->style == FLOATDECIMAL)) {
			k = strlen (s) + 1;
			memmove (&s[1], &s[0], k);
			s [0] = (d->sgn) ? '-' : ' ';
		}
	}
}

/////////////////////////////////////////////////////////////////////////
// classic SANE str2dec except all in c and input is
// wider decimal (see above)
//
// SYNTAX ACCEPTED BY THE SCANNER
// ------------------------------
// Square brackets enclose optional items, braces enclose elements
// to be repeated at least once, and vertical bars separate
// alternative elements; letters that appear literally, like the 'E'
// marking the exponent field, may be either upper or lower case.
//
// <decimal number>   ::= [{space | tab}] <left decimal>
// <left decimal>	  ::= [+ | -] <unsigned decimal>
// <unsigned decimal> ::= <finite number> | <infinity> | <NaN>
// <finite number>	  ::= <significand> [<exponent>]
// <significand>	  ::= <integer> | <mixed>
// <integer>		  ::= {digits} [.]
// <digits> 		  ::= {0|1|2|3|4|5|6|7|8|9}
// <mixed>			  ::= [<digits>].<digits>
// <exponent>		  ::= E [+ | -] <digits>
// <infinity>		  ::= INF | °
// <NAN>			  ::= NAN[([<digits>])]
//
/////////////////////////////////////////////////////////////////////////

void str2dec(const char *s,short *ix,decimal *d,short *vp) {
				// 	blank, non-breaking space, tab //
const char white[] = {' ',       '\xca',      '\t', '\0'};	//	white space
const char zero[] = "0";
const char digits[] = "0123456789";
const char digitsdot[] = "0123456789.";
const char hex[] = "0123456789ABCDEF";
char rndChar = 0;						// round decimal digit, if necessary


int i;		// next character pointer
int iok;	// all characters up to this pointer have been used to build the number
int j;
int expsign, i1, i2, n1, n2;
int excess = 0;				// number of excessive digits that don't fit in sig.text
unsigned long nancode;
long expadjust, expdigits;

	*vp = d->sgn = d->exp = 0;	// initialize default return values
	d->sig.length = strlen (strcpy ((char*) d->sig.text, "N0011"));	// NaNAsciiBinary
	
	iok = i = *ix;					// location of initial search
	i += strspn (&s [i], white);	// eats blanks (white space)
	if (s [i] == '+') i++;			// look for + sign
	else if (s [i] == '-') {		// look for - sign
		i++;
		d->sgn = 1;
	}
	if ((j = strspn (&s [i], digitsdot))) {	// digits found 
		if ((j == 1) && (s [i] == '.')) {	// solo '.' found
			*vp = (short) (s [++i] == '\0');	// set valid prefix
			return;
		}
		if (strspn (&s [i], ".") > 1) return;	// lead ".." found, <klh 9/27/96>
		expadjust = i1 = i2 = n2 = 0;
		i += strspn (&s [i], zero);			// skip leading zeros
		
		if ((n1 = strspn (&s [i], digits))) {	// case of leading significant digit
			i1 = i;
			i += n1;
			if (s [i] == '.') {				// valid decimal point found
				i++;
				if ((n2 = strspn (&s [i], digits))) {
					i2 = i;
					expadjust = -n2;
					i += n2;
				}
			}
		}
		else {								// case of no leading significant digits
			if (s [i] == '.') {				// valid decimal point found
				i++;
				j =  strspn (&s [i], zero);	// skip leading zeros, count in j
				i += j;
				if ((n2 = strspn (&s [i], digits))) {
					i2 = i;                 // location of first significant digit
					expadjust = -(j+n2);
					i += n2;
				}
			}
		}
		if (n2)
			while (s [i2 + n2 - 1] == '0') {			// strip trailing zeros
				n2--;
				expadjust++;
			}
		if ((!n2) && n1) while (s [i1 + n1 - 1] == '0') {	// strip trailing zeros
			n1--;
			expadjust++;
		}

		//  The following code block restricts to number of significant digits used in s[]
		//  to a maximum of SIGDIGLEN.  This eliminates the possibility of TOO MANY DIGITS!
		//  in the next if{} block allowing str2dec() to convert decimal strings that have
		//  more than SIGDIGLEN digits.  This block is the only change to this function and
		//  removal of this block restores the old behavior.  PAF 8/21/96.

		if ((j = n1 + n2) > SIGDIGLEN) {
																	
			rndChar = (n1 > SIGDIGLEN) ? s [i1 + SIGDIGLEN]: s [i2 + SIGDIGLEN - n1]; // round digit
			excess = j - SIGDIGLEN;	// if (excess >= 2) then a sticky decimal digit exists
			n2 -= excess;
			if (n2 < 0) {
				n1 += n2;
				n2 = 0;
			}
			expadjust += excess;
		}

		if (!(j = n1 + n2)) d->sig.length = strlen (strcpy ((char*) d->sig.text, "0"));
		else if (j <= SIGDIGLEN) {
			if (n1) strncpy ((char*) d->sig.text, &s [i1], n1);
			if (n2) strncpy ((char*) &d->sig.text [n1], &s [i2], n2);
			d->sig.text [j] = '\0';
			if (excess) excessDigits (d, rndChar, excess, &expadjust);

			if (expadjust > 32767) d->exp = 32767;				//	check for overflow
			else {
				if (expadjust < -32767) d->exp = 0x8000;		//	check for underflow
				else d->exp = expadjust;						//	in range
			}
		}
		else return;	//	TOO MANY DIGITS!
		iok = i;
		if ((s [i] == 'E') || (s [i] == 'e')){
			i++;
			expsign = 0;
			if (s [i] == '+') i++;		// look for sign
			else if (s [i] == '-') {
				i++;
				expsign = 1;
			}
			expdigits = 0;
			if ((j = strspn (&s [i], digits))) {	// digits found
				while (j) {
					if (expdigits > 0x0c000000L) expdigits = 0x0c000000L;	// prevent exp overflow
					expdigits *= 10;
					expdigits += (s [i++] - '0');
					j--;
				}
				if (expsign) expdigits = -expdigits;				//	change sign if necessary
				iok = i;
			}
			if (d->sig.text [0] != '0') {
				expdigits += expadjust;
				if (expdigits > 32767) d->exp = 32767;				//	check for overflow
				else {
					if (expdigits < -32767) d->exp = 0x8000;		//	check for underflow
					else d->exp = expdigits;						//	in range
				}
			}
		}
		d->sig.length = strlen ((char*) d->sig.text);
	}
	else if ((s [i] == 'N') || (s [i] == 'n')) {	// beginning of NaN?
		i++;
		if ((s [i] == 'A') || (s [i] == 'a')) {
			i++;
			if ((s [i] == 'N') || (s [i] == 'n')) {
				iok = ++i;
				d->sig.length = strlen (strcpy ((char*) d->sig.text, "N4000"));	// NaN
				if (s [i] == '(') {
					i++;
					nancode = 0;
					if ((j = strspn (&s [i], digits))) {	// digits found
						while (j) {
							nancode *= 10;
							nancode += (s [i++] - '0');
							j--;
						}
					}
					if (s [i] == ')') {
						iok = ++i;
						for (j = 4; j >= 3; j--) {
							d->sig.text [j] = hex [0x0000000f & nancode];
							nancode >>= 4;
						}
					}
				}
			}
		}
	}
	else if ((s [i] == 'I') || (s [i] == 'i')) { // beginning of INF?
		i++;
		if ((s [i] == 'N') || (s [i] == 'n')) {
			i++;
			if ((s [i] == 'F') || (s [i] == 'f')) {
				iok = ++i;
				d->sig.length = strlen (strcpy ((char*) d->sig.text, "I"));	// Infinity
			}
		}
	}
	else if (s [i] == '°') {
		iok = ++i;
		d->sig.length = strlen (strcpy ((char*) d->sig.text, "I"));	// Infinity
	}
	*ix = (short) iok;					// set next character pointer
	*vp = (short) (s [i] == '\0');		// set valid prefix
}

/////////////////////////////////////////////////////////////////////////
// routine for handling excessive significant digits in str2dec.
//
/////////////////////////////////////////////////////////////////////////


void excessDigits (decimal *d, char rndChar, int excess, long *expadjust) {
int i;

// Inexact operation, information lost, i.e., excessive digits.
	
	feraiseexcept (FE_INEXACT);
	
// Either the truncated result is maintained in d->sig.text (return) or we need
// to increment d->sig.text.  This switch statement determines the correct action.
// Note, for excessive digits, we do not trim trailing zeros.
	
	switch (fegetround ()) {
		case FE_TONEAREST : 
							if ((excess > 1) || (rndChar != '5')) {	//	not half way case
								if (rndChar < '5') return;
							}
							else {						//	 half way case
								if (strchr ("13579", d->sig.text [SIGDIGLEN - 1]) == 0)
									return;		// Truncated value is round to even.
							}
							break;
		case FE_UPWARD : 
							if (d->sgn) return;
							break;
		case FE_DOWNWARD : 
							if (!d->sgn) return;
							break;
		case FE_TOWARDZERO : 
							return;
							break;
	}

// Increment d->sig.text.

	i = SIGDIGLEN - 1;	
	while (((++d->sig.text [i]) > '9') && (i >= 0)) {	// bump a digit & check for overflow	
		d->sig.text [i--] = '0';						// reset overflowed digit
	}

	if (i < 0) {										// case of all 9's, "9999...9999"
		d->sig.text [0] = '1';							// reset overflowed digit
		(*expadjust)++;									// bump exponent
	}
}

/////////////////////////////////////////////////////////////////////////
// floor of approximate log base 10
// see Jerome Coonen's PhD thesis
/////////////////////////////////////////////////////////////////////////

long apml ( double x )
{
eparts s;
register long e, g = 0x4D10;

	s.extval = x;
	if ((e = ((s.intval.highsig & 0x7FF00000) >> 20))) {	// strip sign & shift
		s.intval.highsig = (((s.intval.highsig & 0x000FFFFF) | 0x00100000) << 11) |
												((s.intval.lowsig >> 21) & 0x000007ff);
	}
	else {
		e++;
		s.intval.highsig = ((s.intval.highsig & 0x000FFFFF) << 11) |
												((s.intval.lowsig >> 21) & 0x000007ff);
	}
	e -= 0x3FF;			//	unbias
	while (s.intval.highsig >= 0) {						// normalizing loop
		e--;
		s.intval.highsig = s.intval.highsig << 1;
		if (s.intval.lowsig < 0)						// if necessary
			s.intval.highsig++;	// add carry
		s.intval.lowsig = s.intval.lowsig << 1;
	}
	if (e < 0) g++;				// ensure a lower bound
	return (g * e + ((g * ((s.intval.highsig & 0x7FFFFFFF) >> 15)) >> 16)) >> 16;
}

/////////////////////////////////////////////////////////////////////////
// returns power of double power of ten
//
/////////////////////////////////////////////////////////////////////////

double tenpower (const long n)
{
ldiv_t qr;
double x, y;
const double tens [] = {
						1,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,
					 1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19,
					 1e20, 1e21, 1e22};

	unsigned long i = labs (n);
	if (i <= MAXTEN) return tens [i];
	
	qr = ldiv (i, MAXTEN);
	i = qr.quot;
	y = tens [qr.rem];
	x = tens [MAXTEN];
	do {
		if (i & 1) y *= x;
		if (i >>= 1) x *= x;
	} while (i);
	return y;
}
	
/////////////////////////////////////////////////////////////////////////
//	returns big power of ten
//
//  We need to consider inexact not being set when inexact table entries are
//  used.  When this occurs, inexact will always be set by a narrowing operation.
//  With the source and target of all bin <--> dec being less than the big type,
//  it is impossible to exactly scale with an inexact big type, i.e., the scaling
//  operation will correctly set inexact.
/////////////////////////////////////////////////////////////////////////

void bigtenpower (const long n, big *y )
{
big xtable [] = {
 { 0x00000049, {{ 0x87867832, 0x6EAC9000, 0, 0, 0, 0, 0, 0 }} },
 { 0x00000092, {{ 0x8F7E32CE, 0x7BEA5C6F, 0xE4820023, 0xA2000000, 0, 0, 0, 0 }} },
 { 0x00000124, {{ 0xA0DC75F1, 0x778E39D6, 0x696361AE, 0x3DB1C721, 0x373B6B34, 0x318119EB,
     0x65080000, 0 }} },
 { 0x00000248, {{ 0xCA28A291, 0x859BBF93, 0x7D7B8F75, 0x03CFDCFE, 0xD11F91FF, 0x10629770,
     0x56291E60, 0x0B4D00DB }} },
 { 0x00000491, {{ 0x9FA42700, 0xDB900AD2, 0x5EBF18B6, 0xD27795FF, 0x9DF3E0BD, 0x5F019366,
     0xF477189F, 0xA875F113 }} },
 { 0x00000922, {{ 0xC71AA36A, 0x1F8F01CB, 0x9DAD43F2, 0x30E1226E, 0x83689C3C, 0xBD362290,
     0x50C00F72, 0x12E04C7D }} },
 { 0x00001245, {{ 0x9ADA6CD4, 0x96EF0E05, 0x2F1A208F, 0xDEDFF747, 0x57E155F4, 0xAE05D035,
     0xE00E35AB, 0xDA0C5952 }} },
 { 0x0000248A, {{ 0xBB570A9A, 0x9BD977CC, 0x4C808753, 0xBB22FEF8, 0x6FC5802C, 0xDE0B3272,
     0x10E980A1, 0xC0CCFD83 }} } };
int j;
ldiv_t qr;
unsigned long i;

fenv_t e;
int dummy;

	dummy = feholdexcept (&e);	//	Does not change the current rounding direction mode.
	dummy = fesetround (FE_TONEAREST);		// need best result

	i = labs (n);

	if (i > 5631) i = 5631;				// 5631 = MAXTEN*2^8 - 1



	qr = ldiv (i, MAXTEN);
	i = qr.quot;
	ext2big (tenpower (qr.rem), y);
	if (i == 0) {


	feupdateenv (&e);

		return;
	}
	j = 0;
	do {
		if (i & 1) axb2c ( y, &xtable [j], y, TRUE );
		j++;
		i >>= 1;
	} while (i);


	feupdateenv (&e);

}
	

/////////////////////////////////////////////////////////////////////////
//  returns the fixed part of an double into d->sig.text
//
/////////////////////////////////////////////////////////////////////////

void getsig ( double x, decimal *d )
{
eparts s;
register int e, i, carry, round, sticky;
unsigned char length, c[128];

	s.extval = x;
	c [length = 0] = 0;
	if ((e = ((s.intval.highsig & 0x7FF00000) >> 20))) {	// strip sign & shift
		s.intval.highsig = (((s.intval.highsig & 0x000FFFFF) | 0x00100000) << 11) |
												((s.intval.lowsig >> 21) & 0x000007ff);
	}
	else {
		e++;
		s.intval.highsig = ((s.intval.highsig & 0x000FFFFF) << 11) |
												((s.intval.lowsig >> 21) & 0x000007ff);
	}
	e -= 0x3FF;			//	unbias
	s.intval.lowsig = s.intval.lowsig << 11;

// fixed point case with no significant digits
// we need to denormalize as necessary for rounding algorithm
	while ((e < 0)	&& ((s.intval.highsig & 0xC0000000) != 0)) {
		if (s.intval.highsig & 1) s.intval.lowsig = 1;
		s.intval.highsig = (s.intval.highsig >> 1) & 0x7FFFFFFF;
		e++;
	}
	do {
		if ((s.intval.highsig & 0x80000000) != 0) carry = 1;
		else carry = 0;
		if (--e < 0) {						// last bit, let's round
//  We are processing the last bit.  Thus we need to round correctly.
//  We either truncate the last digit or we bump it by one. 
//	Factors we need to consider are;
//	1) sign of number, found in d->sgn
//	2) rounding direction
//	3) least significant bit, currently in carry
//	4) round bit
//  5) sticky bit
			round = (s.intval.highsig & 0x40000000) != 0;
			sticky = ((s.intval.highsig & 0x3FFFFFFF) != 0) ||
					 (s.intval.lowsig != 0) || fetestexcept (FE_INEXACT);
			switch ( fegetround () ) {
				case FE_TONEAREST:
					if (round && (carry || sticky))
						carry++;
					break;
				case FE_UPWARD:
					if ((d->sgn == 0) && ((round != 0) || (sticky != 0)))
						carry++;
					break;
				case FE_DOWNWARD:
					if ((d->sgn == 1) && ((round != 0) || (sticky != 0)))
						carry++;
					break;
				case FE_TOWARDZERO:
					break;
			}
			if ((round != 0) || (sticky != 0)) feraiseexcept (FE_INEXACT);
		}
		for ( i = length; i >= 0; i-- )	{// Build string
				c [i] = c [i] + c [i] + carry;
				carry = 0;
				while (c [i] > 9) {
					c [i] -= 10;
					carry++;
				}
		}
		if (carry != 0)	{					// expand string if necessary
			for ( i = ++length; i; i-- ) c [i] = c [i - 1];
			c [0] = carry;
		}
		
		
		s.intval.highsig = s.intval.highsig << 1;
		if ((s.intval.lowsig & 0x80000000) != 0)		// if necessary
			s.intval.highsig = s.intval.highsig + 1;	// add carry
		s.intval.lowsig = s.intval.lowsig << 1;
	}	while ((e >= 0) && (length < MAXDIGITS));		// end of conversion loop
	for ( i = 0; i <= length; i++ ) c [i] += '0';
												// change string to ASCII
	d->sig.length = length + 1;
	for (i = 0; i <= length; i++) d->sig.text [i] = c [i];
	d->sig.text [length + 1] = 0;		// attach trailing null character
	return;
}


/////////////////////////////////////////////////////////////////////////
//  returns the fixed part of an big into d->sig.text
//
/////////////////////////////////////////////////////////////////////////

void biggetsig ( big *s, decimal *d )
{
register int i, carry, round, sticky;
unsigned char length, c[128];

	c [length = 0] = 0;

// fixed point case with no significant digits
// we need to denormalize as necessary for rounding algorithm
	while ((s->exp < 0) && ((s->sig.lng [0] & 0xC0000000) != 0)) {	//	JPO 9/2/92
		if (s->sig.lng [0] & 1) s->sig.lng [1] = 1;			// mantain sticky bit
		s->sig.lng [0] = (s->sig.lng [0] >> 1) & 0x7FFFFFFF;
		s->exp++;
	}
	do {								// conversion loop
		if ((s->sig.lng [0] & 0x80000000) != 0) carry = 1;
		else carry = 0;
		if (--s->exp < 0) {						// last bit, let's round
//  We are processing the last bit.  Thus we need to round correctly.
//  We either truncate the last digit or we bump it by one. 
//	Factors we need to consider are;
//	1) sign of number, found in d->sgn
//	2) rounding direction
//	3) least significant bit, currently in carry
//	4) round bit
//  5) sticky bit
			round = (s->sig.lng [0] & 0x40000000) != 0;
			sticky = ((s->sig.lng [0] & 0x3FFFFFFF) != 0) || fetestexcept (FE_INEXACT);
			i = 1;
			while ((!sticky) && (i < SIGSIZE)) { // look for occurrence of sticky bit
				sticky = s->sig.lng [i] != 0;
				i++;
			}
			switch ( fegetround () ) {
				case FE_TONEAREST:
					if ((round != 0) && ((carry != 0) || (sticky != 0)))	// klh 8/27/92
						carry++;
					break;
				case FE_UPWARD:
					if ((d->sgn == 0) && ((round != 0) || (sticky != 0)))
						carry++;
					break;
				case FE_DOWNWARD:
					if ((d->sgn == 1) && ((round != 0) || (sticky != 0)))
						carry++;
					break;
				case FE_TOWARDZERO:
					break;
			}
			if ((round != 0) || (sticky != 0)) feraiseexcept (FE_INEXACT);
		}
		for ( i = length; i >= 0; i-- )	{		// Build string
				c [i] = c [i] + c [i] + carry;
				carry = 0;
				while (c [i] > 9) {
					c [i] -= 10;
					carry++;
				}
		}
		if (carry != 0) {					// expand string if necessary
			for ( i = ++length; i; i-- ) c [i] = c [i - 1];
			c [0] = carry;
		}
		for (i = 0; i < SIGSIZEM; i++)
			s->sig.lng [i] = (s->sig.lng [i] << 1) + ((s->sig.lng [i + 1] & 0x80000000) != 0);
		s->sig.lng [i] = s->sig.lng [i] << 1;
	} while ((s->exp >= 0) && (length <= MAXDIGITS));	// end of conversion loop
	for ( i = 0; i <= length; i++ ) c [i] += '0';
												// change string to ASCII
	d->sig.length = length + 1;
	for (i = 0; i <= length; i++) d->sig.text [i] = c [i];
	d->sig.text [length + 1] = 0;			// attach trailing null character
	return;
}
	
/////////////////////////////////////////////////////////////////////////
// similar to SANE dec2x except written in c and input is decimal
// handles precision control and rounding direction.
// assumes special cases (zeros, nans and infinities have been handled
// by calling routine.
/////////////////////////////////////////////////////////////////////////


double dec2d ( const decimal *d)  {
big a, b;
long i, i2, j, j2;
eparts s;
int sticky;
long bias =  1023;

	j2 = 0;						// initialize big digit string length
	a.sig.lng [j2] = 0;			// initialize big digit string	
	i2 = d->sig.length - 1;		// set i2 to AASCI string length
	for (i = 0; i <= i2; i++) {	// build the big significand
		a.sig.lng [0] += (d->sig.text [i] - '0');	// translate AASCI to binary
	// if not last chracter multiple by 10
		if (i < i2) for (j = 0; j <= j2; j++) a.sig.lng [j] *= 10;
		for (j = 0; j <= j2; j++) {
			if (a.sig.lng [j] & 0xffff0000) {	// test for carry(s)
				if (j == j2) {	// if last digit overflowed expand binary digit
					j2++;
					a.sig.lng [j2] = 0;
				}
				a.sig.lng [j + 1] += ((a.sig.lng [j] >> 16) & 0x0000ffff); // add carry(s)
				a.sig.lng [j] &= 0x0000ffff;		// mask out carry(s)
			}
		}
	}

	for (i = 0; i < SIGSIZE; i++) b.sig.lng [i] = 0;
	// Reverse the order
	for (j = 0; j <= j2; j++) b.sig.shrt [j] = (short) a.sig.lng [j2 - j];
	b.exp = 16*j2 + 15;
	// Normalize it
	j2 /= 2;
	while ((b.sig.lng [0] & 0x80000000) == 0) {		// normalizing loop
		b.exp--;
		for (j = 0; j < j2; j++) {
			b.sig.lng [j] <<= 1;
			if ((b.sig.lng [j + 1] & 0x80000000) != 0)
				b.sig.lng [j]++;
		}
		b.sig.lng [j2] <<= 1;
	}
		
			//	check to see if sig fits exactly into double
	
	sticky = (b.sig.lng [1] & 0x000007ff) != 0;		
	i = 2;
	while ((!sticky) && (i < SIGSIZE)) sticky = b.sig.lng [i++] != 0;	// JPO 8/28/92
	
	if ((!sticky) && (abs(d->exp) <= MAXTEN)) {	//	can be handled by double
		s.intval.highsig = ((b.exp + bias) << 20) | ((b.sig.lng [0] >> 11) & 0x000fffff);
		if (d->sgn) s.intval.highsig |= 0x80000000;	//	set sign bit if necessary
		s.intval.lowsig = (b.sig.lng [0] << 21) | ((b.sig.lng [1] >> 11) & 0x001fffff);
		
		if (d->exp < 0) s.extval /= tenpower (d->exp);	// scale by d->exp
		if (d->exp > 0) s.extval *= tenpower (d->exp);	// scale by d->exp

		return s.extval;
	}
	else {	//	can not be handled by double, use bigs

		if (d->exp < 0) {						// scale by d->exp
			bigtenpower (d->exp, &a);
			adivb2c (&b, &a, &b); 
		}
		else if (d->exp > 0) {
			bigtenpower (d->exp, &a);
			axb2c ( &b, &a, &b, FALSE ); 
		}
		return big2d ( (int) d->sgn, &b );
	}
}
							
/////////////////////////////////////////////////////////////////////////
// translates  big into double
// for different data types one needs to write the corresponding routines
// NOTE: This routine alters ithe input
/////////////////////////////////////////////////////////////////////////

double big2d ( int sgn, big *b )
{
long i, j;
eparts s;
int carry, round, sticky, bump;
long maxexp =  1023;
long bias =  maxexp;
long minexp = -1022;
int numbits = 54;		// maximum number of denormal shifts

//	UNNORMALIZE BIG SO THAT WE CAN DO ROUNDING AND BOUNDS CHECK

	b->sig.lng [2] = (b->sig.lng [1] << 21) | (((b->sig.lng [2] >> 11) | b->sig.lng [2]) & 0x001fffff);
	b->sig.lng [1] = (b->sig.lng [0] << 21) | ((b->sig.lng [1] >> 11) & 0x001fffff);
	b->sig.lng [0] = (b->sig.lng [0] >> 11) & 0x001fffff;

	if (b->exp < minexp) {		// denormalizing code
		j = numbits;
		while ((j) && (b->exp < minexp)) {		// denormalizing loop
			j--;
			b->exp++;
			bump = 0;
			for (i = 0; i < SIGSIZE; i++) {
				carry = (b->sig.lng [i] & 0x00000001) != 0;
				b->sig.lng [i] =  0x7fffffff & (b->sig.lng [i] >> 1);
				if (bump) b->sig.lng [i] |= 0x80000000;
				bump = carry;
			}
			if (carry) b->sig.lng [SIGSIZEM] |= 0x00000001;
		}
		b->exp = minexp;
	}
	
	carry = (b->sig.lng [1] & 0x00000001) != 0;
	round = (b->sig.lng [2] & 0x80000000) != 0;
	sticky = (b->sig.lng [2] & 0x7fffffff) != 0;
	i = 3;
	while ((!sticky) && (i < SIGSIZE)) sticky = b->sig.lng [i++] != 0; // klh 8/27/92
	bump = 0;
	switch ( fegetround () ) {
		case FE_TONEAREST:
			if ((round != 0) && ((carry != 0) || (sticky != 0)))	// klh 8/27/92
				bump++;
			break;
		case FE_UPWARD:
			if ((sgn == 0) && ((round != 0) || (sticky != 0)))
				bump++;
			break;
		case FE_DOWNWARD:
			if ((sgn == 1) && ((round != 0) || (sticky != 0)))
				bump++;
			break;
		case FE_TOWARDZERO:
			break;
	}
	if ((round != 0) || (sticky != 0)) feraiseexcept (FE_INEXACT);

	if (bump) {			// bump the significand and propagate carry
		if (!(++b->sig.lng [1]))
			if ((++b->sig.lng [0]) == 0x00200000) {
				b->sig.lng [0] = 0x00100000;
				b->exp++;
			}
	}

	if ((b->sig.lng [0] & 0xfff00000) == 0) {
		b->exp--;
		if ((round != 0) || (sticky != 0)) feraiseexcept (FE_UNDERFLOW);
	}
	
	if (b->exp <= maxexp)  {		//	Stuff it
		s.intval.highsig = ((b->exp + bias) << 20) | (b->sig.lng [0] & 0x000fffff);
		if (sgn) s.intval.highsig |= 0x80000000;	//	set sign bit if necessary
		s.intval.lowsig = b->sig.lng [1];
	}
	else {						//	OVERFLOW happens!
		s.intval.lowsig = 0;
		s.intval.highsig = (sgn) ? 0xfff00000 : 0x7ff00000 ;

		feraiseexcept (FE_INEXACT + FE_OVERFLOW);       // <11/4/93 klh>
		if (((fegetround () == FE_UPWARD) && (sgn == 1)) ||
			((fegetround () == FE_DOWNWARD) && (sgn == 0)) ||
			 (fegetround () == FE_TOWARDZERO)) {
				s.intval.lowsig = 0xffffffff;
				s.intval.highsig = (sgn) ? 0xffefffff : 0x7fefffff ;
		}
	}
	return s.extval;
}

/////////////////////////////////////////////////////////////////////////
// translates  big into float
// for different data types one needs to write the corresponding routines
// NOTE: This routine alters the input
/////////////////////////////////////////////////////////////////////////

float big2float ( int sgn, big *b ) {
long i, j;
fparts y;
int carry, round, sticky, bump;
long maxexp =  127;
long bias =  maxexp;
long minexp = -126;
int numbits = 25;		// maximum number of denormal shifts

//	UNNORMALIZE BIG SO THAT WE CAN DO ROUNDING AND BOUNDS CHECK

	b->sig.lng [1] = (b->sig.lng [0] << 24) | (((b->sig.lng [1] >> 8) | b->sig.lng [1]) & 0x00ffffff);
	b->sig.lng [0] = (b->sig.lng [0] >> 8) & 0x00ffffff;

	if (b->exp < minexp) {		// denormalizing code
		j = numbits;
		while ((j) && (b->exp < minexp)) {		// denormalizing loop
			j--;
			b->exp++;
			bump = 0;
			for (i = 0; i < SIGSIZE; i++) {
				carry = (b->sig.lng [i] & 0x00000001) != 0;
				b->sig.lng [i] =  0x7fffffff & (b->sig.lng [i] >> 1);
				if (bump) b->sig.lng [i] |= 0x80000000;
				bump = carry;
			}
			if (carry) b->sig.lng [SIGSIZEM] |= 0x00000001;
		}
		b->exp = minexp;
	}
	
	carry = (b->sig.lng [0] & 0x00000001) != 0;
	round = (b->sig.lng [1] & 0x80000000) != 0;
	sticky = (b->sig.lng [1] & 0x7fffffff) != 0;
	i = 2;
	while ((!sticky) && (i < SIGSIZE)) sticky = b->sig.lng [i++] != 0; // klh 8/27/92
	bump = 0;
	switch ( fegetround () ) {
		case FE_TONEAREST:
			if ((round != 0) && ((carry != 0) || (sticky != 0)))	// klh 8/27/92
				bump++;
			break;
		case FE_UPWARD:
			if ((sgn == 0) && ((round != 0) || (sticky != 0)))
				bump++;
			break;
		case FE_DOWNWARD:
			if ((sgn == 1) && ((round != 0) || (sticky != 0)))
				bump++;
			break;
		case FE_TOWARDZERO:
			break;
	}
	if ((round != 0) || (sticky != 0)) feraiseexcept (FE_INEXACT);

	if (bump) {			// bump the significand
		if ((++b->sig.lng [0]) == 0x01000000) {
			b->sig.lng [0] = 0x00800000;
			b->exp++;
		}
	}

	if ((b->sig.lng [0] & 0xff800000) == 0) {
		b->exp--;
		if ((round != 0) || (sticky != 0)) feraiseexcept (FE_UNDERFLOW);
	}

	if (b->exp <= maxexp)  {		//	Stuff it
		y.along = ((b->exp + bias) << 23) | (b->sig.lng [0] & 0x007fffff);
		if (sgn) y.along |= 0x80000000;	//	set sign bit if necessary
	}
	else {						//	OVERFLOW happens!
		y.along = (sgn) ? 0xfff00000 : 0x7ff00000 ;

	    feraiseexcept (FE_INEXACT + FE_OVERFLOW);       // <11/4/93 klh>
		if (((fegetround () == FE_UPWARD) && (sgn == 1)) ||
			((fegetround () == FE_DOWNWARD) && (sgn == 0)) ||
			 (fegetround () == FE_TOWARDZERO)) {
				y.along = (sgn) ? 0xffefffff : 0x7fefffff ;
		}
	}
	return y.flt;
}

/////////////////////////////////////////////////////////////////////////
// translates  big into long
// NOTE: This routine alters the input
/////////////////////////////////////////////////////////////////////////

long big2long ( int sgn, big *b ) {
long i, j;
int carry, round, sticky, bump;
long maxexp =  31;
long minexp = 31;
int numbits = 33;		// maximum number of unnormal shifts

	if (b->exp < minexp) {		// denormalizing code
		j = numbits;
		while ((j) && (b->exp < minexp)) {		// denormalizing loop
			j--;
			b->exp++;
			bump = 0;
			for (i = 0; i < SIGSIZE; i++) {
				carry = (b->sig.lng [i] & 0x00000001) != 0;
				b->sig.lng [i] =  0x7fffffff & (b->sig.lng [i] >> 1);
				if (bump) b->sig.lng [i] |= 0x80000000;
				bump = carry;
			}
			if (carry) b->sig.lng [SIGSIZEM] |= 0x00000001;
		}
		b->exp = minexp;
	}
	
	carry = (b->sig.lng [0] & 0x00000001) != 0;
	round = (b->sig.lng [1] & 0x80000000) != 0;
	sticky = (b->sig.lng [1] & 0x7fffffff) != 0;
	i = 2;
	while ((!sticky) && (i < SIGSIZE)) sticky = b->sig.lng [i++] != 0; // klh 8/27/92
	bump = 0;
	switch ( fegetround () ) {
		case FE_TONEAREST:
			if ((round != 0) && ((carry != 0) || (sticky != 0)))	// klh 8/27/92
				bump++;
			break;
		case FE_UPWARD:
			if ((sgn == 0) && ((round != 0) || (sticky != 0)))
				bump++;
			break;
		case FE_DOWNWARD:
			if ((sgn == 1) && ((round != 0) || (sticky != 0)))
				bump++;
			break;
		case FE_TOWARDZERO:
			break;
	}

	if (bump) {			// bump the significand
		if ((++b->sig.lng [0]) == 0x01000000) {
			b->sig.lng [0] = 0x00800000;
			b->exp++;
		}
	}
	
	if (sgn) {
		if ((b->exp > maxexp) || 
			(((b->sig.lng [0] & 0x80000000) != 0) && (b->sig.lng [0] != 0x80000000))) {
			feraiseexcept (FE_INVALID);
			return 0x80000000;
		}
		else {
			if ((round != 0) || (sticky != 0)) feraiseexcept (FE_INEXACT);
			return -b->sig.lng [0];
		}
	}
	else {
		if ((b->exp > maxexp) || ((b->sig.lng [0] & 0x80000000) != 0)) {
			feraiseexcept (FE_INVALID);
			return 0x80000000;
		}
		else {
			if ((round != 0) || (sticky != 0)) feraiseexcept (FE_INEXACT);
			return b->sig.lng [0];
		}
	}
}

/////////////////////////////////////////////////////////////////////////
// translates  big into short
// NOTE: This routine alters the input
/////////////////////////////////////////////////////////////////////////

short big2short ( int sgn, big *b ) {
long i, j;
int carry, round, sticky, bump;
long maxexp =  31;
long minexp = 31;
int numbits = 33;		// maximum number of unnormal shifts

	if (b->exp < minexp) {		// denormalizing code
		j = numbits;
		while ((j) && (b->exp < minexp)) {		// denormalizing loop
			j--;
			b->exp++;
			bump = 0;
			for (i = 0; i < SIGSIZE; i++) {
				carry = (b->sig.lng [i] & 0x00000001) != 0;
				b->sig.lng [i] =  0x7fffffff & (b->sig.lng [i] >> 1);
				if (bump) b->sig.lng [i] |= 0x80000000;
				bump = carry;
			}
			if (carry) b->sig.lng [SIGSIZEM] |= 0x00000001;
		}
		b->exp = minexp;
	}
	
	carry = (b->sig.lng [0] & 0x00000001) != 0;
	round = (b->sig.lng [1] & 0x80000000) != 0;
	sticky = (b->sig.lng [1] & 0x7fffffff) != 0;
	i = 2;
	while ((!sticky) && (i < SIGSIZE)) sticky = b->sig.lng [i++] != 0; // klh 8/27/92
	bump = 0;
	switch ( fegetround () ) {
		case FE_TONEAREST:
			if ((round != 0) && ((carry != 0) || (sticky != 0)))	// klh 8/27/92
				bump++;
			break;
		case FE_UPWARD:
			if ((sgn == 0) && ((round != 0) || (sticky != 0)))
				bump++;
			break;
		case FE_DOWNWARD:
			if ((sgn == 1) && ((round != 0) || (sticky != 0)))
				bump++;
			break;
		case FE_TOWARDZERO:
			break;
	}

	if (bump) {			// bump the significand
		if ((++b->sig.lng [0]) == 0x01000000) {
			b->sig.lng [0] = 0x00800000;
			b->exp++;
		}
	}
	
	if (sgn) {
		if ((b->exp > maxexp) ||										//  <10/20/93 klh>
			(((b->sig.lng [0] & 0xffff8000) != 0) && (b->sig.lng [0] != 0x00008000))) {
			feraiseexcept (FE_INVALID);
			return 0x8000;
		}
		else {
			if ((round != 0) || (sticky != 0)) feraiseexcept (FE_INEXACT);
			return -b->sig.shrt [1];
		}
	}
	else {
		if ((b->exp > maxexp) || ((b->sig.lng [0] & 0xffff8000) != 0)) {
			feraiseexcept (FE_INVALID);
			return 0x8000;
		}
		else {
			if ((round != 0) || (sticky != 0)) feraiseexcept (FE_INEXACT);
			return b->sig.shrt [1];
		}
	}
}

/////////////////////////////////////////////////////////////////////////
//	first we must break up b into two parts where b + c = original b,  s.t.,
//	b has the 53 most significant bits and c is the smallest signed normalized big
/////////////////////////////////////////////////////////////////////////

int big2twobigs ( big *b, big *c ) {
int i, j, j1, j2, kleft, kright, sgn, carry;
long mask0; 

	c->exp = b->exp;
	if ((sgn = (b->sig.lng [1] & 0x00000400) != 0)) {	//	determine sign of second part
		carry = 1;
		for (i = SIGSIZEM; i > 1; i--)
			carry = ((c->sig.lng [i-1] = ~b->sig.lng [i] + carry) == 0) && carry;
		c->sig.lng [0] = ~b->sig.lng [1] + carry;
		b->sig.lng [1] &= 0xfffff800;
		if ((b->sig.lng [1] += 0x00000800) == 0) 
			if ((++b->sig.lng [0]) == 0) {
				b->sig.lng [0] = 0x80000000;
				b->exp++;
			}
	}
	else {											//	positive
		for (i = SIGSIZEM; i > 1; i--) c->sig.lng [i-1] = b->sig.lng [i];
		c->sig.lng [0] = b->sig.lng [1];
		b->sig.lng [1] &= 0xfffff800;
	}

//		CLEAN UP 	
	
	for (i = 2; i < SIGSIZE; i++) b->sig.lng [i] = 0; 
	c->sig.lng [SIGSIZEM] = 0;
	
	if (c->sig.lng [0] &= 0x000007ff) {
		mask0 = 0x00000400;
		kleft = 21;
		j1 = 0;
		while ((mask0 & c->sig.lng [j1]) == 0) {
			kleft++;
			mask0 = mask0 >> 1;
		}
	}
	else {
		j1 = 1;
		while ((j1 < SIGSIZE) && (c->sig.lng [j1] == 0)) j1++;
		if (j1 == SIGSIZE) {			//	second part is zero
			c->exp = 0;
			c->sig.lng [0] = 0;
			return 0;					//	return positive zero
		}
		else {
			mask0 = 0x80000000;
			kleft = 0;
			while ((mask0 & c->sig.lng [j1]) == 0) {
				kleft++;
				mask0 = mask0 >> 1;
			}
		}
	}
	c->exp -= (32*(j1 + 1) + kleft);
	
	kright = 32 - kleft;
	j2 = SIGSIZEM - j1 - 1;
	for (j = 0; j <= j2; j++) {
		c->sig.lng [j] = (c->sig.lng [j + j1] << kleft) | (c->sig.lng [j + j1 + 1] >> kright);
	}
	for (j = j2; j < SIGSIZEM; j++) c->sig.lng [j] = 0;	
	return sgn;	
}
							

/////////////////////////////////////////////////////////////////////////
//	support routine for num2dec, extracts nan code
//	and builds decimal string
/////////////////////////////////////////////////////////////////////////

void getnan ( double x, decimal *d )
{
eparts s;
register long n;

	s.extval = x;
	d->sig.length = 5;			// pascal type length
	d->sig.text [0] = 'N';
	d->sig.text [1] = (unsigned char) ('4' + ((s.intval.highsig & 0x00020000) >> 17));
	n = (s.intval.highsig & 0x0001E000) >> 13;
	if (n > 9) n = n + 7;
	d->sig.text [2] = (unsigned char) ('0' + n);
	n = (s.intval.highsig & 0x00001E00) >> 9;
	if (n > 9) n = n + 7;
	d->sig.text [3] = (unsigned char) ('0' + n);
	n = (s.intval.highsig & 0x000001E0) >> 5;
	if (n > 9) n = n + 7;
	d->sig.text [4] = (unsigned char) ('0' + n);
	d->sig.text [5] = 0;		//  null byte
}
	
/////////////////////////////////////////////////////////////////////////
// support routine for num2dec
//
/////////////////////////////////////////////////////////////////////////

void bigb2d ( double x, long scale, decimal *d ) {
big y, z;

	ext2big (x, &y);
	if (scale < 0) {
		bigtenpower (scale, &z);
		adivb2c (&y, &z, &y); 
	}
	else if (scale > 0) {
		bigtenpower (scale, &z);
		axb2c ( &y, &z, &y, FALSE ); 
	}
	biggetsig (&y, d);
}
	
/////////////////////////////////////////////////////////////////////////
// translates double into big
// for different data types one needs to write the corresponding routines
/////////////////////////////////////////////////////////////////////////

void ext2big ( double x, big *b )
{
eparts s;
register int i, j;
unsigned long carry;


	s.extval = x;
	if ((b->exp = ((s.intval.highsig & 0x7FF00000) >> 20))) {	// strip sign & shift
		b->sig.lng [0] = (((s.intval.highsig & 0x000FFFFF) | 0x00100000) << 11) |
												((s.intval.lowsig >> 21) & 0x000007ff);
	}
	else {
		b->exp++;
		b->sig.lng [0] = ((s.intval.highsig & 0x000FFFFF) << 11) |
												((s.intval.lowsig >> 21) & 0x000007ff);
	}
	b->exp -= 0x3FF;			//	unbias
	b->sig.lng [1] = s.intval.lowsig << 11;
	for (i = 2; i < SIGSIZE; i++) b->sig.lng [i] = 0;

//		normalizing loop

	j = 32*SIGSIZE;		//	protection against the unexpected infinite loop
	
	while ((j-- > 0) && ((b->sig.lng [0] & 0x80000000) == 0)) {
		for (i = 0; i < SIGSIZE; i++) {
			carry = (i < SIGSIZEM) ? (b->sig.lng [i + 1] & 0x80000000) != 0 : 0;
			b->sig.lng [i] = (b->sig.lng [i] << 1) + carry;
		}
		b->exp--;
	}
}

//
//
//	product [0]     --- ---
//	product [1]         --- ---
//	product [2]             --- ---
//                                         edge of big
//                                              |
//  big->sig.lng [SIGSIZE - 1]           --- ---|
//  big->sig.shrt [SIGSIZE2M]                ---|
//
//	product [SIGSIZE2M2]                 --- ---|
//	product [SIGSIZE2M]                      ---|---
//	product [SIGSIZE2]                          |--- ---
//
//

/////////////////////////////////////////////////////////////////////////
// multiplies a times b and returns result in c, if finishRounding then correctly
// round else or sticky bit into last bit (useful in avoiding double rounding)
/////////////////////////////////////////////////////////////////////////

void axb2c ( big *a, big *b, big *c, int finishRounding )
{
int i, j, k, i2, j2, k2, carry, round, sticky;
unsigned long temp, product [SIGSIZE4M];

	c->exp = a->exp + b->exp + 1;
	i2 = SIGSIZE2M;
	while (a->sig.shrt [i2] == 0) i2--;		// find length of a
	j2 = SIGSIZE2M;
	while (b->sig.shrt [j2] == 0) j2--;		// find length of b
	k2 = i2 + j2;
	for (k = 0; k < SIGSIZE4M; k++) product [k] = 0;	// clear product
	for (i = 0; i <= i2; i++) {					// main multiply loop
		for (j = 0; j <= j2; j++) {
			k = i + j;
			temp = a->sig.shrt [i] * b->sig.shrt [j];
			if (k != 0) {
				product [k - 1] += ((temp >> 16) & 0x0000FFFFUL);
				product [k] += (temp & 0x0000FFFFUL);
			}
			else product [k] = temp; // this is the first iteration of both loops
		}
	}	
	for (k = k2; k > 0; k--) {	// clean up all dangling products
		product [k - 1] += (product [k] >> 16);
		product [k] &= 0x0000FFFFUL;
	}
	while ((product [0] & 0x80000000UL) == 0) {		// normalizing loop
		c->exp--;
		for (k = 0; k < k2; k++) {
			product [k] <<= 1;
			if ((product [k + 1] & 0x00008000UL) != 0)
				product [k]++;
		}
		product [k2] <<= 1;
	}
	c->sig.lng [0] = product [0];
	carry = (product [SIGSIZE2M2] & 0x00000001UL) != 0;
	round = (product [SIGSIZE2M ] & 0x00008000UL) != 0;
	sticky = (product [SIGSIZE2M] & 0x00007FFFUL) != 0;
	k = SIGSIZE2;
	while ((!sticky) && (k <= k2)) {	// look for occurrence of sticky bit
		sticky = (product [k] & 0x00007FFFUL) != 0;
		k++;
	}
	if (finishRounding) {
		if ((round != 0) || (sticky != 0)) feraiseexcept (FE_INEXACT);
		if ((round != 0) && ((carry != 0) || (sticky != 0))) { // klh 8/27/92
			k = SIGSIZE2M2;
			product [k]++;
			while (((product [k] & 0x0000FFFFUL) == 0) && (k > 0)) { // carry has occurred
				k--;
				product [k]++;
			}
			if ((k == 0) && (product [0] == 0))	{ // carry has propagated off the left
				product [0] = 0x80000000UL;		// put leading bit back in significand
				c->exp++;						// adjust exponent
			}
		}
	}
	else if ((round != 0) || (sticky != 0)) { 		// JPO 9/24/92
			feraiseexcept (FE_INEXACT);
			product [k] |= 1;						// JPO 9/2/92
											// assumes correct rounding is done
	}										// later to a narrower precision
	c->sig.lng [0] = product [0];			// copy significand to c
	for (k = 2; k <= SIGSIZE2M; k++)
		c->sig.shrt [k] = (short) product [k - 1] & 0x0000FFFFUL;
}
	
/////////////////////////////////////////////////////////////////////////
//	c gets a divided by b.
//	or sticky bit into lsb
/////////////////////////////////////////////////////////////////////////

void adivb2c ( big *a, big *b, big *c )
{
int i, j, k, carry, sticky, excess = FALSE;
int asize = SIGSIZEM, bsize = SIGSIZEM, csize, dqsize;
unsigned long dq [SIGDIVQOUT];
struct prod prodresult;

	c->exp = a->exp - b->exp - 1;
	while (a->sig.lng [asize] == 0) asize--;	// find length of quotient
	while (b->sig.lng [bsize] == 0) bsize--;	// find length of divisor
	for (i = 0; i <= asize; i++) dq [i] = a->sig.lng [i];
	dq [dqsize = asize + 1] = 0;				// buffer for 1 bit shift
	while (dqsize < (SIGSIZEDIV + bsize)) dq [++dqsize] = 0;
	csize = (asize > bsize) ? asize : bsize;	// max (asize, bsize)
				// find first difference or last word, whichever occurs first
	i = firstdiff (&(a->sig.lng [0]), &(b->sig.lng [0]), csize);	// klh 8/27/92
	if (a->sig.lng [i] >= b->sig.lng [i]) {	// shift is needed to prevent overflow
		c->exp++;
		carry = FALSE;
		for (i = dqsize; i > 0; i--) {
			dq [i] >>= 1;
			if ((dq [i - 1] & 0x00000001UL) != 0)  dq [i] |= 0x80000000UL;
			else dq [i] &= 0x7fffffffUL;
		}
		dq [0] >>= 1;
		dq [0] &= 0x7fffffffUL;
	}
	
	for (i = 0; i < SIGSIZEDIV; i++) {
		if (divuL (b->sig.lng [0], &dq [i])) {	
			dq [i] = 0;
			subL ( &(b->sig.lng [1]), &dq [i + 1], bsize-1);
			do {
				dq [i]--;
				k = addL ( &(b->sig.lng [0]), &dq [i + 1], bsize);
				k = firstdiff (&(b->sig.lng [0]), &dq [i + 1], bsize);
			}	while (b->sig.lng [k] <= dq [i + 1 + k]);
		}
		else {
			excess = FALSE;
			for (j = bsize; j > 0; j--) {
				prodresult = muluL (b->sig.lng [j], dq [i]);
				k = i + j;
				if (subDL (prodresult, &dq [k])) {
					do {
						if (--k > i) dq [k]--;
						else excess = TRUE;
					}
					while ((k > i) && (dq [k] == 0xffffffffUL));
				}
			}
			if (excess)	{
				do dq [i]--;
				while (!addL ( &(b->sig.lng [0]), &dq [i + 1], bsize));
			}
		}
	}

	i = SIGSIZEDIV;
	sticky = dq [i] != 0;
	while ((!sticky) && (i < dqsize)) sticky = dq [++i] != 0;
	for (i = 0; i < SIGSIZEDIV; i++) c->sig.lng [i] = dq [i];
	c->sig.lng [SIGSIZEDIV] = sticky;
	for (i = SIGSIZEDIV + 1; i < SIGSIZE; i++) c->sig.lng [i] = 0;
}

/////////////////////////////////////////////////////////////////////////
//	bigshifter		shifts big a into bid d n bits and returns shifted
//					info with sticky bit set .
//////////////////////////////////////////////////////////////////////////

unsigned long bigshifter ( big *a, big *d, int n) {
ldiv_t qr;
unsigned long buf [SIGSIZE + 2];
int j, j1, j2, kleft;

	for (j = 0; j < SIGSIZEP2; j++) buf [j] = 0;		//	initialize with zeros
	qr = ldiv (n, 32);
	j2 = j1 = (qr.quot < SIGSIZEP2) ? qr.quot : SIGSIZEP2;	// # of word shifts
	for (j = 0; j < SIGSIZE; j++) {
		if (j1 < SIGSIZEP2)	buf [j1++] = a->sig.lng [j];
		else buf [SIGSIZEP2 - 1] |= a->sig.lng [j];
	}
	
	if (qr.rem != 0) {
		kleft = 32 - qr.rem;
		buf [SIGSIZE + 1] |= buf [SIGSIZE] << kleft;
		for (j = SIGSIZE; j > j2; j--)
			buf [j] = (buf [j - 1] << kleft) | (buf [j] >> qr.rem);
		if (j2 <= SIGSIZE) buf [j2] = buf [j2] >> qr.rem;
	}
	
	for (j = 0; j < SIGSIZE; j++) d->sig.lng [j] = buf [j];

	return buf [SIGSIZE] | (buf [SIGSIZE + 1] != 0);
}

/////////////////////////////////////////////////////////////////////////
//	addbigs		c gets a + b.
//  non distructive ie all three arguments can be the same
/////////////////////////////////////////////////////////////////////////

void addbigs ( big *a, big *b, big *c ) {
big tbig;
unsigned long temp, ovrflw, round, carry, sticky;
int i;

	if (a->exp >= b->exp) {
		c->exp = a->exp;
		temp = bigshifter (b, &tbig, a->exp - b->exp);
		ovrflw = addL ( &(a->sig.lng [0]), &(tbig.sig.lng [0]), SIGSIZEM);
	}
	else {
		c->exp = b->exp;
		temp = bigshifter (a, &tbig, b->exp - a->exp);
		ovrflw = addL ( &(b->sig.lng [0]), &(tbig.sig.lng [0]), SIGSIZEM);
	}
	if (ovrflw != 0) {		// overflow occurred, readjust big
		c->exp++;
		sticky = temp != 0;
		temp = bigshifter (&tbig, &tbig, 1) | sticky;
		tbig.sig.lng [0] |= 0x80000000;
	}
	carry = (tbig.sig.lng [SIGSIZEM] & 1) != 0;
	round = (temp & 0x80000000) != 0;
	sticky = (temp & 0x7FFFFFFF) != 0;
	if (round || sticky) feraiseexcept (FE_INEXACT);
	if (round && (carry || sticky)) { 	// round to nearest
		i = SIGSIZEM;
		while ((++tbig.sig.lng [i] == 0) && (i > 0)) i--;
		if (i == 0)
			if (++tbig.sig.lng [i] == 0) {
				tbig.sig.lng [i] = 0x80000000;
				c->exp++;
			}
	}
	for (i = 0; i < SIGSIZE; i++) c->sig.lng [i] = tbig.sig.lng [i];
}

/////////////////////////////////////////////////////////////////////////
//	subbigs		c gets a - b.
//  non distructive ie all three arguments can be the same
//	WARNING! a must be greater than or equal to b
//	We also assume that the two bigs are really head and tail parts of a double double
//	therefore, we can round the tail before subtracting without loss of accuracy
/////////////////////////////////////////////////////////////////////////

void subbigs ( big *a, big *b, big *c ) {
big tbig;
unsigned long temp, round, carry, sticky;
int i, j;

	c->exp = a->exp;
	temp = bigshifter (b, &tbig, a->exp - b->exp);
	carry = (tbig.sig.lng [SIGSIZEM] & 1) != 0;
	round = (temp & 0x80000000) != 0;
	sticky = (temp & 0x7FFFFFFF) != 0;
	if (round || sticky) feraiseexcept (FE_INEXACT);
	if (round && (carry || sticky)) { 	// round to nearest
		i = SIGSIZEM;
		while ((++tbig.sig.lng [i] == 0) && (i > 0)) i--;
		if (i == 0) ++tbig.sig.lng [i];
	}
	for (i = 0; i < SIGSIZE; i++) c->sig.lng [i] = a->sig.lng [i];
	subL ( &(tbig.sig.lng [0]), &(c->sig.lng [0]), SIGSIZEM);		// b -= a

//		We assume there will be no massive cancellation so we will use a naive
//		normalizing loop

	j = 32*SIGSIZE;		//	protection against the unexpected infinite loop
	
	while ((j-- > 0) && ((c->sig.lng [0] & 0x80000000) == 0)) {
		for (i = 0; i < SIGSIZE; i++) {
			carry = (i < SIGSIZEM) ? (c->sig.lng [i + 1] & 0x80000000) != 0 : 0;
			c->sig.lng [i] = (c->sig.lng [i] << 1) + carry;
		}
		c->exp--;
	}

}

/////////////////////////////////////////////////////////////////////////
// intended to mimic 68020 divuL instruction 64/32
// returns 1 if overflow is set, 0 otherwise
/////////////////////////////////////////////////////////////////////////

int divuL (unsigned long dvsr, unsigned long *dvdnd)
{
register unsigned long quo = 0;
register int i;

	if (dvsr <= dvdnd [0]) return 1;		// overflow will occur
	else {
		for (i = 1; i <= 32; i++) {
			quo <<= 1;
			if ((dvdnd [0] & 0x80000000UL) != 0) {
				quo += 1;
				dvdnd [0] <<= 1;
				if ((dvdnd [1] & 0x80000000UL) != 0) dvdnd [0]++;
				dvdnd [0] -= dvsr;
			}
			else {
				dvdnd [0] <<= 1;
				if ((dvdnd [1] & 0x80000000UL) != 0) dvdnd [0]++;
				if (dvsr <= dvdnd [0]) {
					quo += 1;
					dvdnd [0] -= dvsr;
				}
			}
			dvdnd [1] <<= 1;
		}
		dvdnd [1] = dvdnd [0];
		dvdnd [0] = quo;
		return 0;						// no overflow
	}
}

/////////////////////////////////////////////////////////////////////////
// intended to mimic 68020 muluL instruction
//
/////////////////////////////////////////////////////////////////////////

struct prod muluL (unsigned long a, unsigned long b)
{
union {
	unsigned long along;
	unsigned short ashort [2];
	} u1, u2, p00, p01, p10, p11, r;
struct prod temp;

	u1.along = a;
	u2.along = b;
	
	p00.along = (unsigned long) u1.ashort [0] * (unsigned long) u2.ashort [0];
	p01.along = (unsigned long) u1.ashort [0] * (unsigned long) u2.ashort [1];
	p10.along = (unsigned long) u1.ashort [1] * (unsigned long) u2.ashort [0];
	p11.along = (unsigned long) u1.ashort [1] * (unsigned long) u2.ashort [1];
	
	r.ashort [1] = p11.ashort [1];
	u1.along = 	(unsigned long) p11.ashort [0]
			 +  (unsigned long) p01.ashort [1]
			 + 	(unsigned long) p10.ashort [1];
	r.ashort [0] = u1.ashort [1];
	temp.low = r.along;
	
	u2.along =  (unsigned long) p00.ashort [1] 
			 + 	(unsigned long) p01.ashort [0]
			 + 	(unsigned long) p10.ashort [0]
			 + 	(unsigned long) u1.ashort [0];	
	r.ashort [1] = u2.ashort [1];
	r.ashort [0] = p00.ashort [0] + u2.ashort [0];
	temp.high = r.along;
	
	return temp;
}



/////////////////////////////////////////////////////////////////////////
// support function for adivb2c
//
/////////////////////////////////////////////////////////////////////////

int subDL (prod a, unsigned long *b)
{
register int borrow, carry;
	
	borrow = a.low > b [1];
	b [1] -= a.low;
	carry = (a.high > b [0]) || ((a.high == b [0]) && borrow);
	b [0] -= (a.high + (unsigned long) borrow);
	return carry;
}

/////////////////////////////////////////////////////////////////////////
// support function for adivb2c and subbigs
//
/////////////////////////////////////////////////////////////////////////

void subL (unsigned long *a, unsigned long *b, int length)		// b -= a
{
register int k, borrow = FALSE, carry;

	for (k = length; k >= 0; k--) {
		carry = (a [k] > b [k]) || ((a [k] == b [k]) && borrow);
		b [k] -= (a [k] + (unsigned long) borrow);
		borrow = carry;
	}
}

/////////////////////////////////////////////////////////////////////////
// support function for adivb2c and addbigs
//
/////////////////////////////////////////////////////////////////////////

int addL (unsigned long *a, unsigned long *b, int length)		// b += a
{
register int k, carry = FALSE;
register unsigned long c;

	for (k = length; k >= 0; k--) {
		c = b [k] + a [k] + (unsigned long) carry;
		carry = 	(((0x80000000UL & a [k]) != 0) && ((0x80000000UL & b [k]) != 0))
			  	||  (((0x80000000UL & a [k]) != 0) && ((0x80000000UL & c) == 0))
			  	||  (((0x80000000UL & b [k]) != 0) && ((0x80000000UL & c) == 0));
		b [k] = c;
	}
	return carry;
}

/////////////////////////////////////////////////////////////////////////
// support function for adivb2c
//
/////////////////////////////////////////////////////////////////////////

int firstdiff (unsigned long *a, unsigned long *b, int length)
{				// find first differnce or last word, whichever occurs first
register int i = 0;
	while ((a [i] == b [i]) && (i < length)) i++;
	return i;
}




