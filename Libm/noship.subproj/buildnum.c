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
#include "stdio.h"
#include "ctype.h"
#include "string.h"

#define EXTMAXEXP	 16384
#define EXTMINEXP	-16383
#define EXTSIGBITS		64
#define MANLEN			 9

#define EXTBITS			 80
#define EXTPAD  			((EXTBITS/8)-10)	// Number of pad bytes for extended (0 or 2)

#define strcat(a,b)		strcat((char *)a,(char *)b)
#define strcpy(a,b)		strcpy((char *)a,(char *)b)
#define strcmp(a,b)		strcmp((char *)a,(char *)b)
#define strlen(a)		strlen((char *)a)

typedef int Boolean;
enum {
    false = 0,
    true = 1
};

typedef unsigned char Str90 [91];

typedef struct {
	short int sgn;			// 0 for + and 1 for -
	short int exp;			// unbiased
	short int man [MANLEN];	// explicit 1-bit to left of binary point
} UnpForm;

typedef union {
//	unsigned char b[EXTBITS/8];
	unsigned char b[16];		//	KLH:  altered 3/25/93
	float f;					//	KLH:  ADDED 2/26/93
	double u;					//	KLH:  ADDED 2/26/93
	short w[5];

} PckForm;

static double Str90to (Str90 StrArg1, const char *op, const char pc);	
													// returns a double from a Str90
double Str90todbl (Str90 StrArg1, const char *op);	// returns a double from a Str90
double Str90toflt (Str90 StrArg1, const char *op);	// returns a float from a Str90
double Str90toint (Str90 StrArg1, const char *op);	// returns a short from a Str90
double Str90tolng (Str90 StrArg1, const char *op);	// returns a long from a Str90

UnpForm UnpArg1;

PckForm PckArg1;

short int aptr, HiTol, LoTol, RSign;
static short int MaxExp, MinExp, SigBits, LowBit, LowByte;
static int Nancode;

//
//	** Called by AddUlps and AddExp to normalize an UnpForm.
//
void Normalize (UnpForm *r)
{
	short int i, c, t;
  
	while (r->man[0] < 128 && r->exp > MinExp) {
    	c = 0;
    	for (i = MANLEN - 1; i >= 0; i--) {
			t = r->man[i] * 2 + c;
			if (t > 255) {
				r->man[i] = t - 256;
				c = 1;
			}
			else {
					r->man[i] = t;
					c = 0;
			}
		}
		r->exp--;
	}
	// (r->exp = MinExp) or ((r->exp > MinExp) and (r->man[0] >= 128))
}

//
//	** Called by BuildNum.
//	** Add n ulps to the number in UnpForm r and normalize the result
//	** as much as possible. This routine is complicated by the need
//	** to do bit operations using Pascal types.
//
void AddUlps (UnpForm *r, short int n)
{
	short int c, i, j, t;
  
	if (n >= 0) 
		//
		//	** Add one ulp at a time up to n. This is much easier
		//	** than trying to add all at once. Integer c propagates
		//	** the carry-out from byte to byte.
		//
		for (i = 1; i <= n; i++) {
			c = LowBit;
			for (j = LowByte - 1; j >= 0; j--) {
				t = r->man[j] + c;
				if (t > 255) {
					r->man[j] = t - 256;
					c = 1;
				}
				else {
						r->man[j] = t;
						c = 0;
				}
			}
			if (c == 1) {	//	Carry out of left end?
				r->man[0] = 128;
				r->exp++;
			}
		}
    else	// n < 0
		 for (i = 1; i <= -n; i++) {
			 c = LowBit;
			 for (j = LowByte - 1; j >= 0; j--) {
			 	 t = r->man[j] - c;
			 	 if (t < 0) {
			 		 r->man[j] = t + 256;
			 		 c = 1;
			 	 }
				 else {
						 r->man[j] = t;
						 c = 0;
				 }
		  	 }
		  	 if (r->man[0] < 128 && r->exp > MinExp) {
				 r->man[0] += 128;
				 r->exp--;
		  	 }
		  }
	Normalize (r);
}

//
//	** Called by BuildNum.
//	** Add n to the exponent of UnpForm r, taking account of
//	** the bottom of the exponent range. If the number must
//	** be denormalized, shift right by a given number of bytes and
//	** then normalize to the extent possible.
//
void AddExp (UnpForm *r, short int n)
{
	short int i, j;

	if ((r->exp += n) < MinExp) {
		i = (MinExp - r->exp) / 8 + 1;
		for (j = MANLEN - 1;	j >= i;	j--) r->man[j] = r->man[j - i];
		for (j = 0;				j < i;	j++) r->man[j] = 0;
		r->exp += i * 8;
	}
	
	Normalize (r);
}

//
//	** Called by BuildNum.
//

				void getExponent (Str90 s, UnpForm *r)
				{
					short int i;
					int val;
					char c;
					
					//	Get biased exponent.
					val = 0;
					r->sgn = 0;
					for (i = 0; (i < 4) && (aptr < strlen (s)); i++) {
						c = s [aptr++];
						if (isxdigit (c) && (i == 0))
							if (c > '7') {
								if (isdigit (c)) c -= '8';
								else { c = toupper(c); c -= ('A' - '2'); }
								r->sgn = 1;
							}
						if (isdigit (c))		val = 16 * val + c - '0';
						else if (isxdigit (c))	val = 16 * val + toupper (c) - 'A' + 10;
							 else break;
					}
					r->exp = val - 16383;	//	Unbias the exponent.
				}
				
				void getMantissa (Str90 s, UnpForm *r)
				{
					short int i, val;
					Boolean HiNib;
					char c;
						
					HiNib	= true;	//	place first nibble in high half of byte
					i		= 0;	//	index of first man[]
					while (aptr < strlen (s)) {
						c = s [aptr];
			
						//
						// val = hex value of s [aptr]
						if (isdigit (c))		val = c - '0';
						else if (isxdigit (c))	val = toupper (c) - 'A' + 10; 
							 else break;
		
						if (HiNib) val *= 16;		//	left-align nibble in byte
						else i--;					//	recover from last i := i + 1
				        r->man[i] += val;
						i++;
				        HiNib =  !HiNib;
				        aptr++;
					}
//					if (r->man [6] & 4) r->man [6] += 8;	//	for rough double rounding
//					r->man [6] = r->man [6] & 0xffffff8;	//	klh 9/15/93
				}
					
void HexFloating (Str90 s, UnpForm *r)
{
	short int i, bptr;
	char c = 0;
	
	aptr++;			//	skip over $
	//	Colin’s fix for two hex formats:
	//		(1) $HHHHHHHHHHHHHHHHHH^DDDDDD	-	at most 18 hex digits followed
	//											by unbiased exponent in decimal
	//		(2)	$HHHHHHHHHHHHHHHHHHHH		-	at most 20 hex digits with biased
	//											exponent (first four digits)
	for (bptr = aptr; bptr < strlen (s); bptr++) {
		c = s [bptr];
		if (!isdigit (c) && !isxdigit (c)) break;
	
	}
	if ((bptr < strlen (s)) && (c == '^')) {
		getMantissa (s, r);
		//	(aptr >= strlen (s)) or (s [aptr] is not a legal hex digit)
	
		r->exp	= 0;
		i		= 1;		//	exponent sign carrier
		if (aptr < strlen (s)) {
			//	s [aptr] is not a legal hex digit
			if (s [aptr] == '^') {
				if (++aptr < strlen (s))  {
					if (s [aptr] == '+') aptr++;
					else if (s [aptr] == '-') {
							aptr++;
							i = -1;
						 }
                }
					 
				while (aptr < strlen (s))
					if (isdigit (s [aptr])) r->exp = r->exp * 10 + (s [aptr++] - '0');
					else break;
			}
		}
		r->exp *= i;
	}
	else {
			getExponent (s, r);
			getMantissa (s, r);
	}
	aptr--;		//	because will increment upon return
}


//
//	** Called by BuildNum.
//
void DecInteger (Str90 s, short int *d)
{
	*d = 0;
	
	do 
		if (isdigit (s [aptr])) *d = *d * 10 + (s [aptr++] - '0');
		else break;
	while (aptr < strlen (s));
}

void DecLongInteger (Str90 s, long signed int *d)
{
	*d = 0;
	
	do 
		if (isdigit (s [aptr])) *d = *d * 10 + (s [aptr++] - '0');
		else break;
	while (aptr < strlen (s));
}

//
//	** Called by BuildUnpOps.
//
void BuildNum (Str90 s, UnpForm *r)
{
	short int i, d;
    long signed int dd;
	char c;
	
	aptr	= 0;		//	index into argument string
	
	r->sgn  = 0;
	c = s [aptr];
	if (c == '+') aptr++;
	else if (c == '-') {
			r->sgn = 1;
			aptr++;
		 }
		 
	RSign = r->sgn;
		 
	for (i = 0; i < MANLEN; i++) r->man[i] = 0;
	
	c = s [aptr];
	if (isdigit (c)) {
#if 0
		DecInteger(s, &d);
 		if (d == 0) r->exp = MinExp;	// zero
		else {
				r->exp = 15;
				if (d < 0) {
					r->man[0] = 128;			//	set hi mantissa bit
					d = (d + 32767) + 1;		//	clear d's sign bit
				}
				r->man[0] += d / 256;
				r->man[1]  = d % 256;
				Normalize (r);
			 }
#else
		DecLongInteger(s, &dd);
 		if (dd == 0) r->exp = MinExp;	// zero
		else {
				r->exp = 31;
				if (dd < 0) {
					r->man[0] = 128;			//	set hi mantissa bit
					dd = (dd + 2147483647) + 1;		//	clear d's sign bit
				}
				r->man[0] += (dd >> 24);
				r->man[1]  = ((dd >> 16) & 0xff);
				r->man[2]  = ((dd >> 8) & 0xff);
				r->man[3]  = (dd & 0xff);
				Normalize (r);
			 }
#endif
	}
	else {
			switch (c) {
				case 'e': 
				case 'E':		r->exp	  = MinExp;
								r->man[0] = 128;
							break;
							
				case 'h': 
				case 'H':		r->exp	  = MaxExp;
								r->man[0] = 128;
							break;
							
				case 'M':		r->exp	  =  63;
								r->man[0] = 128;
								AddUlps (r,	-2);
							break;
							
//		Truncates correctly to double, but not float (needs to be bumped)
							
				case 'p': 
				case 'P':		r->exp	  = 1;
								r->man[0] = 0xc9;
								r->man[1] = 0x0f;
								r->man[2] = 0xda;
								r->man[3] = 0xa2;
								r->man[4] = 0x21;
								r->man[5] = 0x68;
								r->man[6] = 0xc2;
								r->man[7] = 0x35;
							break;
							
				case 'q': 
				case 'Q':		r->exp	  = MaxExp;
								r->man[0] = 64;
								r->man[1] = Nancode;
							break;
							
				case 's': 
				case 'S':		r->exp	  = MaxExp;
								r->man[0] = 1;
							break;
							
//		Approximate 3/4 * Pi, not necessarily correct to the last bit?
//		Truncates correctly to double, but not float (needs to be bumped)
							
				case 't': 
				case 'T':		r->exp	  = 1;
								r->man[0] = 0x96;
								r->man[1] = 0xcb;
								r->man[2] = 0xe3;
								r->man[3] = 0xf9;
								r->man[4] = 0x99;
								r->man[5] = 0x0e;
								r->man[6] = 0x91;
								r->man[7] = 0xa8;
							break;
							
				case '$':	HexFloating (s, r);
								//	leaves aptr at last character
			 }
			 aptr++;	//	advance beyond character
		 }
	LoTol = 0;
	HiTol = 0;
	
	while (aptr < strlen (s) - 1) {
		c = s [aptr++];			//	get i,d,u,p,m specifier
		DecInteger (s, &d);
		switch (c) {
			case 'i':		AddUlps (r,	 d);
						break;
			case 'd':		AddUlps (r,	-d);
						break;
			case 'u':		for (i = 0; i < MANLEN; i++) r->man[i] = 0;
							AddUlps (r,	 d);
						break;
			case 'p':		AddExp (r,	 d);
						break;
			case 'm':		AddExp (r,	-d);
						break;
			case '+':		HiTol = d;
						break;
			case '-':	LoTol = d;
		}
	}
}


//
//	** Pack number in UnpForm x into PckForm a with precision pc.
//	** SYSTEM DEPENDENCY : The ordering of bytes in a floating-point
//	** "word" is the vital issue here.
//	** Substitute '.b [{?-}' for '.b [?-' for non-Mac orderings.
//
#if defined (__ppc__)
void FpPack (UnpForm *x, PckForm *a, char pc)
{
	short int i, bexp;
	
	switch (pc) {
		case 's':		bexp = x->exp + 127;
						a->b [3 - 3] = bexp / 2 + 128 * x->sgn;
						a->b [3 - 2] = (bexp % 2) * 128 + x->man[0] % 128;
						a->b [3 - 1] = x->man[1];
						a->b [3 - 0] = x->man[2];
						if (x->man[0] < 128 && bexp == 1) a->b [3 - 2] -= 128;
					break;
					
		case 'i':
		case 'l':
		case 'd':		bexp = x->exp + 1023;
                        if (x->exp < MinExp) {
                            bexp = MinExp + 1023;
                            for (i = 8; i; i--) x->man[8 - i] = 0;
                            x->man[0] = 128;
                        } else if (x->exp > MaxExp) {
                            bexp = MaxExp +1023;
                            for (i = 8; i; i--) x->man[8 - i] = 0;
                            x->man[0] = 128;
                        }
						a->b [8 - 8] = bexp / 16 + 128 * x->sgn;
						a->b [8 - 7] = (bexp % 16) * 16 + (x->man[0] / 8) % 16;
						for (i = 6; i; i--)
							a->b [8 - i] = (x->man[6 - i] % 8) * 32 + x->man[7 - i] / 8;
						if (x->man[0] < 128 && bexp == 1) a->b [8 - 7] -= 16;
                    break;
					
		case 'e':	bexp = x->exp + 16383;
					a->b [EXTPAD + 10 - 10] = bexp / 256 + 128 * x->sgn;
					a->b [EXTPAD + 10 -  9] = bexp % 256;
					for (i = 8; i; i--)
						a->b [EXTPAD + 10 - i] = x->man[8 - i];
					if (x->exp == EXTMAXEXP && x->man[0] > 127)
						a->b [EXTPAD + 10 - 8] -= 128;
#if EXTPAD
					a->b [0] = a->b [EXTPAD];
					a->b [1] = a->b [EXTPAD + 1];
#endif
	}
}
#elif defined (__i386__)
void FpPack (UnpForm *x, PckForm *a, char pc)
{
	short int i, bexp;
	
	switch (pc) {
		case 's':		bexp = x->exp + 127;
						a->b [3] = bexp / 2 + 128 * x->sgn;
						a->b [2] = (bexp % 2) * 128 + x->man[0] % 128;
						a->b [1] = x->man[1];
						a->b [0] = x->man[2];
						if (x->man[0] < 128 && bexp == 1) a->b [2] -= 128;
					break;
					
		case 'i':
		case 'l':
		case 'd':		bexp = x->exp + 1023;
                        if (x->exp < MinExp) {
                            bexp = MinExp + 1023;
                            for (i = 8; i; i--) x->man[8 - i] = 0;
                            x->man[0] = 128;
                        } else if (x->exp > MaxExp) {
                            bexp = MaxExp +1023;
                            for (i = 8; i; i--) x->man[8 - i] = 0;
                            x->man[0] = 128;
                        }
						a->b [7] = bexp / 16 + 128 * x->sgn;
						a->b [6] = (bexp % 16) * 16 + (x->man[0] / 8) % 16;
						for (i = 6; i; i--)
							a->b [i-1] = (x->man[6 - i] % 8) * 32 + x->man[7 - i] / 8;
						if (x->man[0] < 128 && bexp == 1) a->b [6] -= 16;
                    break;
					
		case 'e':	bexp = x->exp + 16383;
					a->b [EXTPAD + 9] = bexp / 256 + 128 * x->sgn;
					a->b [EXTPAD + 8] = bexp % 256;
					for (i = 8; i; i--)
						a->b [EXTPAD + i-1] = x->man[8 - i];
					if (x->exp == EXTMAXEXP && x->man[0] > 127)
						a->b [EXTPAD + 8] -= 128;
#if EXTPAD
					a->b [0] = a->b [EXTPAD];
					a->b [1] = a->b [EXTPAD + 1];
#endif
	}
}
#else
#error Unknown architecture
#endif

static double Str90to (Str90 StrArg1, const char *op, const char pc) {

short int i;
	  
	switch (pc) {
		case 'i':		MaxExp	=    15;
						MinExp	= -1022;
						SigBits	=    15;
 						LowBit	=     2;
						LowByte	=     2;
					break;
					
		case 'l':		MaxExp	=    31;
						MinExp	= -1022;
						SigBits	=    31;
 						LowBit	=     2;
						LowByte	=     4;
					break;
					
		case 's':		MaxExp	=   128;
						MinExp	=  -126;
						SigBits	=    24;
 						LowBit	=     1;
						LowByte	=     3;
					break;
					
		case 'p':								//	klh 3/25/93
		case 'd':		MaxExp	=  1024;
						MinExp	= -1022;
						SigBits	=    53;
						LowBit	=     8;
						LowByte	=     7;
					break;
					
		case 'e':	MaxExp	= EXTMAXEXP;
					MinExp	= EXTMINEXP;
					SigBits	= EXTSIGBITS;
 					LowBit	= 1;
					i		= EXTSIGBITS % 8;
					while (i++ % 8) LowBit += LowBit;
					LowByte = (EXTSIGBITS + 7) / 8;
	}
	
		 if (op [1] == '+') Nancode = 0 /* XXX 2 XXX */;
	else if (op [1] == '-') Nancode = 0 /* XXX 2 XXX */;
	else if (op [1] == '/') Nancode = 0 /* XXX 4 XXX */;
	else if (op [1] == '*') Nancode = 0 /* XXX 8 XXX */;
	else if (op [1] == '%') Nancode = 9;		//	rem
	else if (op [1] == '1') Nancode = 33;
	else if (op [1] == '2') Nancode = 33;
	else if (op [1] == '3') Nancode = 33;
	else if (op [1] == '4') Nancode = 34;
	else if (op [1] == '5') Nancode = 34;
	else if (op [1] == '6') Nancode = 34;
	else if (op [1] == '7') Nancode = 34;
	else if (op [1] == '8') Nancode = 36;
	else if (op [1] == 'M') Nancode = 9;
	else if (op [1] == 'O') Nancode = 36;
	else if (op [1] == 'P') Nancode = 36;
	else if (op [1] == 'Q') Nancode = 36;
	else if (op [1] == 'R') Nancode = 36;
	else if (op [1] == 'T') Nancode = 36;
	else if (op [1] == 'U') Nancode = 36;
	else if (op [1] == 'V') Nancode = 1;		//	sqrt
	else if (op [1] == 'X') Nancode = 37 /* XXX 37 XXX */;
	else if (op [1] == 'Y') Nancode = 38;
	else if (op [1] == 'Z') Nancode = 38;
	else if (op [1] == 'd') Nancode = 17;		//  decimal binary conversion
	else if (op [1] == 'u') Nancode = 40;
	else if (op [1] == 'v') Nancode = 40;
	else if (op [1] == 'w') Nancode = 40;
	else if (op [1] == 'g') Nancode = 42;
	else if (op [1] == 'h') Nancode = 42;
	else if (op [1] == 'q') Nancode = 9;		//	rem
	else if (op [1] == 'r') Nancode = 9;		//	rem
	else Nancode = 255;
    
	BuildNum (StrArg1,  &UnpArg1);
	FpPack   (&UnpArg1, &PckArg1, pc);
	
	switch (pc) {
		case 's':	return (double) PckArg1.f;
					break;
					
		case 'i':
		case 'l':
		case 'd':	return PckArg1.u;
					break;
	}
	return 0;
}


double Str90todbl (Str90 StrArg1, const char *op) {	// returns a double from a Str90
	
	return Str90to (StrArg1, op, 'd');
}

double Str90toflt (Str90 StrArg1, const char *op) {	// returns a float from a Str90

	return Str90to (StrArg1, op, 's');
}

double Str90toint (Str90 StrArg1, const char *op) {	// returns a short from a Str90

	return Str90to (StrArg1, op, 'i');
}

double Str90tolng (Str90 StrArg1, const char *op) {	// returns a long from a Str90

	return Str90to (StrArg1, op, 'l');
}
