/*
 * Copyright (c) 2002 by The XFree86 Project, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *  
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the XFree86 Project shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * XFree86 Project.
 *
 * Author: Paulo César Pereira de Andrade
 */

/* $XFree86: xc/programs/xedit/lisp/mp/mp.h,v 1.5tsi Exp $ */

#include <stdio.h>
#include <math.h>
#ifdef sun
#include <ieeefp.h>
#endif
#include <float.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>

#ifndef __mp_h_
#define __mp_h_

#ifdef __GNUC__
#define INLINE __inline__
#else
#define INLINE /**/
#endif

/* this normally is better for multiplication and also
 * simplify addition loops putting the larger value first */
#define MP_SWAP(op1, op2, len1, len2) {	\
    BNS *top = op1;			\
    BNI tlen = len1;			\
					\
    op1 = op2;				\
    len1 = len2;			\
    op2 = top;				\
    len2 = tlen;			\
}

/*
 * At least this length to use Karatsuba multiplication method
 */
#define KARATSUBA		32

/*
 * At least this length to use Toom multiplication method
 */
#define TOOM			128

#if ULONG_MAX > 4294967295UL
  /* sizeof(long) == 8 and sizeof(int) == 4 */
# define BNI		unsigned long
# define BNS		unsigned int
# define MINSLONG	0x8000000000000000UL
# define CARRY		0x100000000
# define LMASK		0xffffffff00000000UL
# define SMASK		0x00000000ffffffffUL
# define BNIBITS	64
# define BNSBITS	32
# ifndef LONG64
#  define LONG64
# endif
#else
  /* sizeof(long) == 4 and sizeof(short) == 2 */
# define BNI		unsigned long
# define BNS		unsigned short
# define MINSLONG	0x80000000UL
# define CARRY		0x10000
# define LMASK		0xffff0000UL
# define SMASK		0x0000ffffUL
# define BNIBITS	32
# define BNSBITS	16
#endif

#ifdef MAX
#undef MAX
#endif
#define MAX(a, b)	((a) > (b) ? (a) : (b))

#ifdef MIN
#undef MIN
#endif
#define MIN(a, b)	((a) < (b) ? (a) : (b))

/*
 * Types
 */
typedef struct _mpi {
    unsigned int size : 31;
    unsigned int sign : 1;
    BNI alloc;
    BNS *digs;		/* LSF format */
} mpi;

typedef struct _mpr {
    mpi num;
    mpi den;
} mpr;

typedef void *(*mp_malloc_fun)(size_t);
typedef void *(*mp_calloc_fun)(size_t, size_t);
typedef void *(*mp_realloc_fun)(void*, size_t);
typedef void (*mp_free_fun)(void*);

/*
 * Prototypes
 */
/* GENERIC FUNCTIONS */
	/* memory allocation wrappers */
void *mp_malloc(size_t size);
void *mp_calloc(size_t nmemb, size_t size);
void *mp_realloc(void *pointer, size_t size);
void mp_free(void *pointer);
mp_malloc_fun mp_set_malloc(mp_malloc_fun);
mp_calloc_fun mp_set_calloc(mp_calloc_fun);
mp_realloc_fun mp_set_realloc(mp_realloc_fun);
mp_free_fun mp_set_free(mp_free_fun);

	/* adds op1 and op2, stores result in rop
	 * rop must pointer to at least len1 + len2 + 1 elements
	 * rop can be either op1 or op2 */
long mp_add(BNS *rop, BNS *op1, BNS *op2, BNI len1, BNI len2);

	/* subtracts op2 from op1, stores result in rop
	 * rop must pointer to at least len1 + len2 elements
	 * op1 must be >= op2
	 * rop can be either op1 or op2 */
long mp_sub(BNS *rop, BNS *op1, BNS *op2, BNI len1, BNI len2);

	/* shift op to the left shift bits
	 * rop must have enough storage for result
	 * rop can be op */
long mp_lshift(BNS *rop, BNS *op, BNI len, long shift);

	/* shift op to the right shift bits
	 * shift must be positive
	 * rop can be op */
long mp_rshift(BNS *rop, BNS *op, BNI len, long shift);

	/* use simple generic multiplication method
	 * rop cannot be the same as op1 or op2
	 * rop must be zeroed
	 * op1 can be op2 */
long mp_base_mul(BNS *rop, BNS *op1, BNS *op2, BNI len1, BNI len2);

	/* use Karatsuba method
	 * MIN(len1, len2) must be larger than (MAX(len1, len2) + 1) >> 1
	 * MAX(len1, len2) should be at least 2
	 * rop cannot be the same as op1 or op2
	 * rop must be zeroed
	 * op1 can be op2 */
long mp_karatsuba_mul(BNS *rop, BNS *op1, BNS *op2, BNI len1, BNI len2);

	/* use Toom method
	 * len1 / 3 should be equal to len2 / 3
	 * len1 / 3 should be at least 1
	 * rop cannot be the same as op1 or op2
	 * rop must be zeroed
	 * op1 can be op2 */
long mp_toom_mul(BNS *rop, BNS *op1, BNS *op2, BNI len1, BNI len2);

	/* chooses the available multiplication methods based on it's input
	 * rop must be a pointer to len1 + len2 elements
	 * rop cannot be the same as op1 or op2
	 * rop must be zeroed
	 * op1 can be op2 */
long mp_mul(BNS *rop, BNS *op1, BNS *op2, BNI len1, BNI len2);

/* INTEGER FUNCTIONS */
	/* initialize op and set it to 0 */
void mpi_init(mpi *op);

	/* clear memory associated to op */
void mpi_clear(mpi *op);

	/* set rop to the value of op */
void mpi_set(mpi *rop, mpi *op);

	/* set rop to the value of si */
void mpi_seti(mpi *rop, long si);

	/* set rop to the floor(fabs(d)) */
void mpi_setd(mpi *rop, double d);

	/* initialize rop to number representation in str in the given base.
	 * leading zeros are skipped.
	 * if sign present, it is processed.
	 * base must be in the range 2 to 36. */
void mpi_setstr(mpi *rop, char *str, int base);

	/* adds two mp integers */
void mpi_add(mpi *rop, mpi *op1, mpi *op2);

	/* adds op1 and op2 */
void mpi_addi(mpi *rop, mpi *op1, long op2);

	/* subtracts two mp integers */
void mpi_sub(mpi *rop, mpi *op1, mpi *op2);

	/* subtracts op2 from op1 */
void mpi_subi(mpi *rop, mpi *op1, long op2);

	/* multiply two mp integers */
void mpi_mul(mpi *rop, mpi *op1, mpi *op2);

	/* multiply op1 by op2 */
void mpi_muli(mpi *rop, mpi *op1, long op2);

	/* divides num by den and sets rop to result */
void mpi_div(mpi *rop, mpi *num, mpi *den);

	/* divides num by den and sets rop to the remainder */
void mpi_rem(mpi *rop, mpi *num, mpi *den);

	/* divides num by den, sets quotient to qrop and remainder to rrop
	 * qrop is truncated towards zero.
	 * qrop and rrop are optional
	 * qrop and rrop cannot be the same variable */
void mpi_divqr(mpi *qrop, mpi *rrop, mpi *num, mpi *den);

	/* divides num by then and stores result in rop */
void mpi_divi(mpi *rop, mpi *num, long den);

	/* divides num by den and returns remainder */
long mpi_remi(mpi *num, long den);

	/* divides num by den
	 * stores quotient in qrop and returns remainder */
long mpi_divqri(mpi *qrop, mpi *num, long den);

	/* sets rop to num modulo den */
void mpi_mod(mpi *rop, mpi *num, mpi *den);

	/* returns num modulo den */
long mpi_modi(mpi *num, long den);

	/* sets rop to the greatest common divisor of num and den
	 * result is always positive */
void mpi_gcd(mpi *rop, mpi *num, mpi *den);

	/* sets rop to the least common multiple of num and den
	 * result is always positive */
void mpi_lcm(mpi *rop, mpi *num, mpi *den);

	/* sets rop to op raised to exp */
void mpi_pow(mpi *rop, mpi *op, unsigned long exp);

	/* sets rop to the integer part of the nth root of op.
	 * returns 1 if result is exact, 0 otherwise */
int mpi_root(mpi *rop, mpi *op, unsigned long nth);

	/* sets rop to the integer part of the square root of op.
	 * returns 1 if result is exact, 0 otherwise */
int mpi_sqrt(mpi *rop, mpi *op);

	/* bit shift, left if shift positive, right if negative
	 * a fast way to multiply and divide by powers of two */
void mpi_ash(mpi *rop, mpi *op, long shift);

	/* sets rop to op1 logand op2 */
void mpi_and(mpi *rop, mpi *op1, mpi *op2);

	/* sets rop to op1 logior op2 */
void mpi_ior(mpi *rop, mpi *op1, mpi *op2);

	/* sets rop to op1 logxor op2 */
void mpi_xor(mpi *rop, mpi *op1, mpi *op2);

	/* sets rop to one's complement of op */
void mpi_com(mpi *rop, mpi *op);

	/* sets rop to -op */
void mpi_neg(mpi *rop, mpi *op);

	/* sets rop to the absolute value of op */
void mpi_abs(mpi *rop, mpi *op);

	/* compares op1 and op2
	 * returns >0 if op1 > op2, 0 if op1 = op2, and  <0 if op1 < op2 */
int mpi_cmp(mpi *op1, mpi *op2);

	/* mpi_cmp with a long integer operand */
int mpi_cmpi(mpi *op1, long op2);

	/* compares absolute value of op1 and op2
	 * returns >0 if abs(op1) > abs(op2), 0 if abs(op1) = abs(op2),
	 * and  <0 if abs(op1) < abs(op2) */
int mpi_cmpabs(mpi *op1, mpi *op2);

	/* mpi_cmpabs with a long integer operand */
int mpi_cmpabsi(mpi *op1, long op2);

	/* returns 1 if op1 > 0, 0 if op1 = 0, and  -1 if op1 < 0 */
int mpi_sgn(mpi *op);

	/* fastly swaps contents of op1 and op2 */
void mpi_swap(mpi *op1, mpi *op2);

	/* returns 1 if op fits in a signed long int, 0 otherwise */
int mpi_fiti(mpi *op);

	/* converts mp integer to long int
	 * to know if the value will fit, call mpi_fiti */
long mpi_geti(mpi *op);

	/* convert mp integer to double */
double mpi_getd(mpi *op);

	/* returns exact number of characters to represent mp integer
	 * in given base, excluding sign and ending null character.
	 * base must be in the range 2 to 36 */
unsigned long mpi_getsize(mpi *op, int base);

	/* returns pointer to string with representation of mp integer
	 * if str is not NULL, it must have enough space to store integer
	 * representation, if NULL a newly allocated string is returned.
	 * base must be in the range 2 to 36 */
char *mpi_getstr(char *str, mpi *op, int base);

/* RATIO FUNCTIONS */
#define mpr_num(op)	(&((op)->num))
#define mpr_den(op)	(&((op)->den))

	/* initialize op and set it to 0/1 */
void mpr_init(mpr *op);

	/* clear memory associated to op */
void mpr_clear(mpr *op);

	/* set rop to the value of op */
void mpr_set(mpr *rop, mpr *op);

	/* set rop to num/den */
void mpr_seti(mpr *rop, long num, long den);

	/* set rop to the value of d */
void mpr_setd(mpr *rop, double d);

	/* initialize rop to number representation in str in the given base.
	 * leading zeros are skipped.
	 * if sign present, it is processed.
	 * base must be in the range 2 to 36. */
void mpr_setstr(mpr *rop, char *str, int base);

	/* remove common factors of op */
void mpr_canonicalize(mpr *op);

	/* adds two mp rationals */
void mpr_add(mpr *rop, mpr *op1, mpr *op2);

	/* adds op1 and op2 */
void mpr_addi(mpr *rop, mpr *op1, long op2);

	/* subtracts two mp rationals */
void mpr_sub(mpr *rop, mpr *op1, mpr *op2);

	/* subtracts op2 from op1 */
void mpr_subi(mpr *rop, mpr *op1, long op2);

	/* multiply two mp rationals */
void mpr_mul(mpr *rop, mpr *op1, mpr *op2);

	/* multiply op1 by op2 */
void mpr_muli(mpr *rop, mpr *op1, long op2);

	/* divide two mp rationals */
void mpr_div(mpr *rop, mpr *op1, mpr *op2);

	/* divides op1 by op2 */
void mpr_divi(mpr *rop, mpr *op1, long op2);

	/* sets rop to 1/op */
void mpr_inv(mpr *rop, mpr *op);

	/* sets rop to -op */
void mpr_neg(mpr *rop, mpr *op);

	/* sets rop to the absolute value of op */
void mpr_abs(mpr *rop, mpr *op);

	/* compares op1 and op2
	 * returns >0 if op1 > op2, 0 if op1 = op2, and  <0 if op1 < op2 */
int mpr_cmp(mpr *op1, mpr *op2);

	/* mpr_cmp with a long integer operand */
int mpr_cmpi(mpr *op1, long op2);

	/* compares absolute value of op1 and op2
	 * returns >0 if abs(op1) > abs(op2), 0 if abs(op1) = abs(op2),
	 * and  <0 if abs(op1) < abs(op2) */
int mpr_cmpabs(mpr *op1, mpr *op2);

	/* mpr_cmpabs with a long integer operand */
int mpr_cmpabsi(mpr *op1, long op2);

	/* fastly swaps contents of op1 and op2 */
void mpr_swap(mpr *op1, mpr *op2);

	/* returns 1 if op fits in a signed long int, 0 otherwise */
int mpr_fiti(mpr *op);

	/* convert mp rational to double */
double mpr_getd(mpr *op);

	/* returns pointer to string with representation of mp rational
	 * if str is not NULL, it must have enough space to store rational
	 * representation, if NULL a newly allocated string is returned.
	 * base must be in the range 2 to 36 */
char *mpr_getstr(char *str, mpr *op, int base);

#endif /* __mp_h_ */
