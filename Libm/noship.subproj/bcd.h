
/********************************************************************************
*                                                                               *
*                         Binary to decimal conversions                         *
*                                                                               *
*   SIGDIGLEN   Significant decimal digits.                                     *
*                                                                               *
*   decimal     A record which provides an intermediate unpacked form for       *
*               programmers who wish to do their own parsing of numeric input   *
*               or formatting of numeric output.                                *
*                                                                               *
*   decform     Controls each conversion to a decimal string.  The style field  *
*               is either FLOATDECIMAL or FIXEDDECIMAL. If FLOATDECIMAL, the    *
*               value of the field digits is the number of significant digits.  *
*               If FIXEDDECIMAL value of the field digits is the number of      *
*               digits to the right of the decimal point.                       *
*                                                                               *
*   num2dec     Converts a double_t to a decimal record using a decform.        *
*   dec2num     Converts a decimal record d to a double_t value.                *
*   dec2str     Converts a decform and decimal to a string using a decform.     *
*   str2dec     Converts a string to a decimal struct.                          *
*   dec2d       Similar to dec2num except a double is returned (68k only).      *
*   dec2f       Similar to dec2num except a float is returned.                  *
*   dec2s       Similar to dec2num except a short is returned.                  *
*   dec2l       Similar to dec2num except a long is returned.                   *
*                                                                               *
********************************************************************************/
#if TARGET_CPU_PPC
    #define SIGDIGLEN      36  
#elif TARGET_CPU_68K
    #define SIGDIGLEN      20
#elif TARGET_CPU_X86
    #define SIGDIGLEN      36
#endif
#define      DECSTROUTLEN   80               /* max length for dec2str output */
#define      FLOATDECIMAL   ((char)(0))
#define      FIXEDDECIMAL   ((char)(1))
struct decimal {
    char                            sgn;                        /* sign 0 for +, 1 for - */
    char                            unused;
    short                           exp;                        /* decimal exponent */
    struct {
        unsigned char                   length;
        unsigned char                   text[SIGDIGLEN];        /* significant digits */
        unsigned char                   unused;
    }                               sig;
};
typedef struct decimal decimal;

struct decform {
    char                            style;                      /*  FLOATDECIMAL or FIXEDDECIMAL */
    char                            unused;
    short                           digits;
};
typedef struct decform decform;
/*
 *  num2dec()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later in CoreServices.framework
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
extern void  num2dec(const decform *f, double_t x, decimal *d)                                 AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER;


/*
 *  dec2num()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later in CoreServices.framework
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
extern double_t  dec2num(const decimal * d)                   AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER;


/*
 *  dec2str()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later in CoreServices.framework
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
extern void  dec2str(const decform *f, const decimal *d, char *s)                                 AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER;


/*
 *  str2dec()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later in CoreServices.framework
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
extern void  str2dec(const char *s, short *ix, decimal *d, short *vp)                            AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER;


/*
 *  dec2f()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later in CoreServices.framework
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
extern float  dec2f(const decimal * d)                        AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER;


/*
 *  dec2s()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later in CoreServices.framework
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
extern short  dec2s(const decimal * d)                        AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER;


/*
 *  dec2l()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later in CoreServices.framework
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
extern long  dec2l(const decimal * d)                         AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER;

#ifdef notdef
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
/////////////////////////////////////////////////////////////////////////
// File: bcd.h
// void num2dec(const decform *f,double x,decimal *d);
// double dec2num(const decimal *d);
// void dec2str(const decform *f,const decimal *d,char *s);
// void str2dec(const char *s,short *ix,decimal *d,short *vp);
// Status: ALPHA
// Copyright Apple Computer, Inc. 1984, 1985, 1990, 1991, 1992, 1993
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
//		28 Jan 93  KLH  Added double notation.
//
//		13 May 93  KLH  long double part split out into file "dec2bin2L.c"
//
/////////////////////////////////////////////////////////////////////////
#define	FALSE		0
#define	TRUE		1

#define SIGDIGLEN      36              
#define      DECSTROUTLEN   80               /* max length for dec2str output */
#define      FLOATDECIMAL   ((char)(0))
#define      FIXEDDECIMAL   ((char)(1))

#define	FORDEBUGGER	FALSE	// is this a stanalone all c debugger version

#define	BIGARITH TRUE		// provides access to bigarithmetic 
							// necessary for conversions.c prgram

#define CACHESIZE 0			// size of tens cache, if zero no cache code generated
							// cache must be set to zero if FORDEBUGGER TRUE
					
#define GENERATEBIGTENS FALSE	// code necessary for generating big tens tables

#define USE68020 FALSE		// use 68020 assembly language routines
					// debugger shouldn't use assembly code if FORDEBUGGER TRUE

#define MAXDIGITS	36		// 21;	proposed = floor(log10(2^bits))+2

//typedef double double;
#define MAXTEN		22	// largest double exact power of ten

#define	SIGSIZE		8		// # of long words used in signficand of type big

#define SIGSIZE8 (SIGSIZE == 8)
#define FORDBGRORSIG8 (FORDEBUGGER || SIGSIZE8)

#if 0
struct decimal {
	char 							sgn;						/* sign 0 for +, 1 for - */
	char 							unused;
	short 							exp;						/* decimal exponent */
	struct {
		unsigned char 					length;
		unsigned char 					text[SIGDIGLEN];		/* significant digits */
		unsigned char 					unused;
	} 								sig;
};
typedef struct decimal decimal;

struct decform {
	char 							style;						/*  FLOATDECIMAL or FIXEDDECIMAL */
	char 							unused;
	short 							digits;
};
typedef struct decform decform;
#endif


//#define SIGDIGLEN 128						/* significant decimal digits */


//struct decimal {
//	char sgn;								/*sign 0 for +, 1 for -*/
//	char unused;
//	short exp;								/*decimal exponent*/
//	struct{
//		unsigned char length;
//		char text[SIGDIGLEN];		/*significant digits */
//		char unused;
//		}sig;
//};

//typedef struct decimal decimal;


//struct decform {
//	char style; 							/*FLOATDECIMAL or FIXEDDECIMAL*/
//	char unused;
//	short digits;
//};

//typedef struct decform decform;


#define DECSTROUTLEN 80 					/* max length for dec2str output */

/* Decimal Formatting Styles */

//#define FLOATDECIMAL ((char)(0))
//#define FIXEDDECIMAL ((char)(1))


void num2dec(const decform *f,double x, decimal *d);
double dec2num(const decimal *d);
float dec2f ( const decimal *d );		//	float
short dec2s ( const decimal *d );		//	short
long dec2l ( const decimal *d );		//	long

void dec2str(const decform *f,const decimal *d,char *s);
void str2dec(const char *s,short *ix,decimal *d,short *vp);

#define	SIGSIZEM	SIGSIZE-1
#define	SIGSIZEP2	SIGSIZE+2
#define	SIGSIZE2	2*SIGSIZE
#define	SIGSIZE2M	SIGSIZE2-1
#define	SIGSIZE2M2	SIGSIZE2-2
#define	SIGSIZE4M	4*SIGSIZE-1

struct big {
	long exp;
	union {
	uint32_t lng [SIGSIZE];
	uint16_t shrt [2*SIGSIZE];
		} sig;
};

typedef struct big big;

void bigtenpower (const int32_t n, big *y );
void axb2c ( big *a, big *b, big *c, int finishRounding );
void adivb2c ( big *a, big *b, big *c );
void biggetsig ( big *s, decimal *d );

#endif


