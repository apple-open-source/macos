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

/* $XFree86: xc/programs/xedit/lisp/mp/mp.c,v 1.3 2002/11/20 07:44:43 paulo Exp $ */

#include "mp.h"

/*
 * TODO:
 *	o Optimize squaring
 *	o Write better division code and move from mpi.c to here
 *	o Make multiplication code don't required memory to be zeroed
 *		+ The first step is easy, just multiply the low word,
 *		then the high word, that may overlap with the result
 *		of the first multiply (in case of carry), and then
 *		just make sure carry is properly propagated in the
 *		subsequent multiplications.
 *		+ Some code needs also to be rewritten because some
 *		intermediate addition code in mp_mul, mp_karatsuba_mul,
 *		and mp_toom_mul is assuming the memory is zeroed.
 */

/*
 * Prototypes
 */
	/* out of memory handler */
static void mp_outmem(void);

	/* memory allocation fallback functions */
static void *_mp_malloc(size_t);
static void *_mp_calloc(size_t, size_t);
static void *_mp_realloc(void*, size_t);
static void _mp_free(void*);

/*
 * Initialization
 */
static mp_malloc_fun __mp_malloc = _mp_malloc;
static mp_calloc_fun __mp_calloc = _mp_calloc;
static mp_realloc_fun __mp_realloc = _mp_realloc;
static mp_free_fun __mp_free = _mp_free;

/*
 * Implementation
 */
static void
mp_outmem(void)
{
    fprintf(stderr, "out of memory in MP library.\n");
    exit(1);
}

static void *
_mp_malloc(size_t size)
{
    return (malloc(size));
}

void *
mp_malloc(size_t size)
{
    void *pointer = (*__mp_malloc)(size);

    if (pointer == NULL)
	mp_outmem();

    return (pointer);
}

mp_malloc_fun
mp_set_malloc(mp_malloc_fun fun)
{
    mp_malloc_fun old = __mp_malloc;

    __mp_malloc = fun;

    return (old);
}

static void *
_mp_calloc(size_t nmemb, size_t size)
{
    return (calloc(nmemb, size));
}

void *
mp_calloc(size_t nmemb, size_t size)
{
    void *pointer = (*__mp_calloc)(nmemb, size);

    if (pointer == NULL)
	mp_outmem();

    return (pointer);
}

mp_calloc_fun
mp_set_calloc(mp_calloc_fun fun)
{
    mp_calloc_fun old = __mp_calloc;

    __mp_calloc = fun;

    return (old);
}

static void *
_mp_realloc(void *old, size_t size)
{
    return (realloc(old, size));
}

void *
mp_realloc(void *old, size_t size)
{
    void *pointer = (*__mp_realloc)(old, size);

    if (pointer == NULL)
	mp_outmem();

    return (pointer);
}

mp_realloc_fun
mp_set_realloc(mp_realloc_fun fun)
{
    mp_realloc_fun old = __mp_realloc;

    __mp_realloc = fun;

    return (old);
}

static void
_mp_free(void *pointer)
{
    free(pointer);
}

void
mp_free(void *pointer)
{
    (*__mp_free)(pointer);
}

mp_free_fun
mp_set_free(mp_free_fun fun)
{
    mp_free_fun old = __mp_free;

    __mp_free = fun;

    return (old);
}

long
mp_add(BNS *rop, BNS *op1, BNS *op2, BNI len1, BNI len2)
{
    BNI value;			/* intermediate result */
    BNS carry;			/* carry flag */
    long size;			/* result size */

    if (len1 < len2)
	MP_SWAP(op1, op2, len1, len2);

    /* unroll start of loop */
    value = op1[0] + op2[0];
    rop[0] = value;
    carry = value >> BNSBITS;

    /* add op1 and op2 */
    for (size = 1; size < len2; size++) {
	value = op1[size] + op2[size] + carry;
	rop[size] = value;
	carry = value >> BNSBITS;
    }
    if (rop != op1) {
	for (; size < len1; size++) {
	    value = op1[size] + carry;
	    rop[size] = value;
	    carry = value >> BNSBITS;
	}
    }
    else {
	/* if rop == op1, than just adjust carry */
	for (; carry && size < len1; size++) {
	    value = op1[size] + carry;
	    rop[size] = value;
	    carry = value >> BNSBITS;
	}
	size = len1;
    }
    if (carry)
	rop[size++] = carry;

    return (size);
}

long
mp_sub(BNS *rop, BNS *op1, BNS *op2, BNI len1, BNI len2)
{
    long svalue;		/* intermediate result */
    BNS carry;			/* carry flag */
    long size;			/* result size */

    /* special case */
    if (op1 == op2) {
	rop[0] = 0;

	return (1);
    }

    /* unroll start of loop */
    svalue = op1[0] - op2[0];
    rop[0] = svalue;
    carry = svalue < 0;

    /* subtracts op2 from op1 */
    for (size = 1; size < len2; size++) {
	svalue = (long)(op1[size]) - op2[size] - carry;
	rop[size] = svalue;
	carry = svalue < 0;
    }
    if (rop != op1) {
	for (; size < len1; size++) {
	    svalue = op1[size] - carry;
	    rop[size] = svalue;
	    carry = svalue < 0;
	}
    }
    else {
	/* if rop == op1, than just adjust carry */
	for (; carry && size < len1; size++) {
	    svalue = op1[size] - carry;
	    rop[size] = svalue;
	    carry = svalue < 0;
	}
	size = len1;
    }

    /* calculate result size */
    while (size > 1 && rop[size - 1] == 0)
	--size;

    return (size);
}

long
mp_lshift(BNS *rop, BNS *op, BNI len, long shift)
{
    long i, size;
    BNI words, bits;		/* how many word and bit shifts */

    words = shift / BNSBITS;
    bits = shift % BNSBITS;
    size = len + words;

    if (bits) {
	BNS hi, lo;
	BNI carry;
	int adj;

	for (i = 1, carry = CARRY >> 1; carry; i++, carry >>= 1)
	    if (op[len - 1] & carry)
		break;
	adj = (bits + (BNSBITS - i)) / BNSBITS;
	size += adj;

	lo = hi = op[0];
	rop[words] = lo << bits;
	for (i = 1; i < len; i++) {
	    hi = op[i];
	    rop[words + i] = hi << bits | (lo >> (BNSBITS - bits));
	    lo = hi;
	}
	if (adj)
	    rop[size - 1] = hi >> (BNSBITS - bits);
    }
    else
	memmove(rop + size - len, op, sizeof(BNS) * len);

    if (words)
	memset(rop, '\0', sizeof(BNS) * words);

    return (size);
}

long
mp_rshift(BNS *rop, BNS *op, BNI len, long shift)
{
    int adj = 0;
    long i, size;
    BNI words, bits;		/* how many word and bit shifts */

    words = shift / BNSBITS;
    bits = shift % BNSBITS;
    size = len - words;

    if (bits) {
	BNS hi, lo;
	BNI carry;

	for (i = 0, carry = CARRY >> 1; carry; i++, carry >>= 1)
	    if (op[len - 1] & carry)
		break;
	adj = (bits + i) / BNSBITS;
	if (size - adj == 0) {
	    rop[0] = 0;

	    return (1);
	}

	hi = lo = op[words + size - 1];
	rop[size - 1] = hi >> bits;
	for (i = size - 2; i >= 0; i--) {
	    lo = op[words + i];
	    rop[i] = (lo >> bits) | (hi << (BNSBITS - bits));
	    hi = lo;
	}
	if (adj)
	    rop[0] |= lo << (BNSBITS - bits);
    }
    else
	memmove(rop, op + len - size, size * sizeof(BNS));

    return (size - adj);
}

	/* rop must be a pointer to len1 + len2 elements
	 * rop cannot be either op1 or op2
	 * rop must be all zeros */
long
mp_base_mul(BNS *rop, BNS *op1, BNS *op2, BNI len1, BNI len2)
{
    long i, j;			/* counters */
    BNI value;			/* intermediate result */
    BNS carry;			/* carry value */
    long size = len1 + len2;

    /* simple optimization: first pass does not need to deference rop[i+j] */
    if (op1[0]) {
	value = (BNI)(op1[0]) * op2[0];
	rop[0] = value;
	carry = (BNS)(value >> BNSBITS);
	for (j = 1; j < len2; j++) {
	    value = (BNI)(op1[0]) * op2[j] + carry;
	    rop[j] = value;
	    carry = (BNS)(value >> BNSBITS);
	}
	rop[j] = carry;
    }

    /* do the multiplication */
    for (i = 1; i < len1; i++) {
	if (op1[i]) {
	    /* unrool loop initialization */
	    value = (BNI)(op1[i]) * op2[0] + rop[i];
	    rop[i] = value;
	    carry = (BNS)(value >> BNSBITS);
	    /* multiply */
	    for (j = 1; j < len2; j++) {
		value = (BNI)(op1[i]) * op2[j] + rop[i + j] + carry;
		rop[i + j] = value;
		carry = (BNS)(value >> BNSBITS);
	    }
	    rop[i + j] = carry;
	}
    }

    if (size > 1 && rop[size - 1] == 0)
	--size;

    return (size);
}

	/* Karatsuba method
	 * t + ((a0 + a1) (b0 + b1) - t - u) x + ux²
	 * where t = a0b0 and u = a1b1
	 *
	 * Karatsuba method reduces the number of multiplications. Example:
	 *	Square a 40 length number
	 *	instead of a plain 40*40 = 1600 multiplies/adds, it does:
	 *	20*20+20*20+20*20 = 1200
	 *	but since it is recursive, every 20*20=400 is reduced to
	 *	10*10+10*10+10*10=300
	 *	and so on.
	 * The multiplication by x and x² is a just a shift, as it is a
	 * power of two, and is implemented below by just writting at the
	 * correct offset */
long
mp_karatsuba_mul(BNS *rop, BNS *op1, BNS *op2, BNI len1, BNI len2)
{
    BNI x;				/* shift count */
    BNI la0, la1, lb0, lb1;		/* length of a0, a1, b0, and b1 */
    BNS *t;				/* temporary memory for t product */
    BNS *u;				/* temporary memory for u product */
    BNS *r;				/* pointer to rop */
    long xlen, tlen, ulen;

    /* calculate value of x, that is 2^(BNSBITS*x) */
    if (len1 >= len2)
	x = (len1 + 1) >> 1;
    else
	x = (len2 + 1) >> 1;

    /* calculate length of operands */
    la0 = x;
    la1 = len1 - x;
    lb0 = x;
    lb1 = len2 - x;

    /* allocate buffer for t and (a0 + a1) */
    tlen = la0 + lb0;
    t = mp_malloc(sizeof(BNS) * tlen);

    /* allocate buffer for u and (b0 + b1) */
    if (la1 + lb1 < lb0 + lb1 + 1)
	ulen = lb0 + lb1 + 1;
    else
	ulen = la1 + lb1;
    u = mp_malloc(sizeof(BNS) * ulen);

    /* calculate a0 + a1, store result in t */
    tlen = mp_add(t, op1, op1 + x, la0, la1);

    /* calculate b0 + b1, store result in u */
    ulen = mp_add(u, op2, op2 + x, lb0, lb1);

    /* store (a0 + a1) * (b0 + b1) in rop */

    r = rop + x;		/* multiplied by 2^(BNSBITS*x) */
    xlen = mp_mul(r, t, u, tlen, ulen);

    /* must zero t and u memory, this is required for mp_mul */

    /* calculate t = a0 * b0 */
    tlen = la0 + lb0;
    memset(t, '\0', sizeof(BNS) * tlen);
    tlen = mp_mul(t, op1, op2, la0, lb0);

    /* calculate u = a1 * b1 */
    ulen = la1 + lb1;
    memset(u, '\0', sizeof(BNS) * ulen);
    ulen = mp_mul(u, op1 + x, op2 + x, la1, lb1);

    /* subtract t from partial result */
    xlen = mp_sub(r, r, t, xlen, tlen);

    /* subtract u form partial result */
    xlen = mp_sub(r, r, u, xlen, ulen);

    /* add ux^2 to partial result */

    r = rop + (x << 1);		/* multiplied by x^2 = 2^(BNSBITS*x*2) */
    xlen = len1 + len2;
    xlen = mp_add(r, r, u, xlen, ulen);

    /* now add t to final result */
    xlen = mp_add(rop, rop, t, xlen, tlen);

    mp_free(t);
    mp_free(u);

    if (xlen > 1 && rop[xlen - 1] == 0)
	--xlen;

    return (xlen);
}

	/* Toom method	(partially based on GMP documentation)
	 * Evaluation at k = [ 0 1/2 1 2 oo ]
	 * U(x) = (U2k + U1)k + U0
	 * V(x) = (V2k + V1)k + V0
	 * W(x) = U(x)V(x)
	 *
	 * Sample:
	 *	123 * 456
	 *
	 *	EVALUATION:
	 * U(0) = (1*0+2)*0+3	=> 3
	 * U(1) = 1+(2+3*2)*2	=> 17
	 * U(2) = 1+2+3		=> 6
	 * U(3) = (1*2+2)*2+3	=> 11
	 * U(4)	= 1+(2+3*0)*0	=> 1
	 *
	 * V(0) = (4*0+5)*0+6	=> 6
	 * V(1) = 4+(5+6*2)*2	=> 38
	 * V(2) = 4+5+6		=> 15
	 * V(3) = (4*2+5)*2+6	=> 32
	 * V(4)	= 4+(5+6*0)*0	=> 4
	 *
	 *	U = [ 3   17  6  11 1 ]
	 *	V = [ 6   38 15  32 4 ]
	 *	W = [ 18 646 90 352 4 ]
	 *
	 * After that, we have:
	 *	a = 18					(w0 already known)
	 *	b = 16w0 + 8w1 + 4w2 + 2w3 +   w4
	 *	c =   w0 +  w1 +  w2 +  w3 +   w4
	 *	d =   w0 + 2w1 + 4w2 + 8w3 + 16w4
	 *	e = 4					(w4 already known)
	 *
	 *	INTERPOLATION:
	 *	b = b -16a - e		(354)
	 *	c = c - a - e		(68)
	 *	d = d - a - 16e		(270)
	 *
	 *	w = (b + d) - 8c = (10w1+8w2+10w3) - (8w1+8w2+8w3) = 2w1+2w3
	 *	w = 2c - w		(56)
	 *	b = b/2 = 4w1+w+w3
	 *	b = b-c = 4w1+w+w3 - w1+w2+w3 = 3w1+w2
	 *	c = w/2			(w2 = 28)
	 *	b = b-c = 3w1+c - c = 3w1
	 *	b = b/3			(w1 = 27)
	 *	d = d/2
	 *	d = d-b-w = b+w+4w3 - b-w = 4w3
	 *	d = d/4			(w3 = 13)
	 *
	 *	RESULT:
	 *	w4*10^4 + w3*10³ + w2*10² + w1*10 + w0
	 *	40000   + 13000   + 2800   + 270    + 18
	 *	10 is the base where the calculation was done
	 *
	 *	This sample uses small numbers, so it does not show the
	 * advantage of the method. But for example (in base 10), when squaring
	 *	123456789012345678901234567890
	 *	The normal method would do 30*30=900 multiplications
	 *	Karatsuba method would do 15*15*3=675 multiplications
	 *	Toom method would do 10*10*5=500 multiplications
	 * Toom method has a larger overhead if compared with Karatsuba method,
	 * due to evaluation and interpolation, so it should be used for larger
	 * numbers, so that the computation time of evaluation/interpolation
	 * would be smaller than the time spent using other methods.
	 *
	 *	Note that Karatsuba method can be seen as a special case of
	 * Toom method, i.e:
	 *	U1U0 * V1V0
	 *	with k = [ 0 1 oo ]
	 *	U = [ U0 U1+U0 U1 ]
	 *	V = [ V0 V1+V0 V1 ]
	 *	W = [ U0*V0 (U1+U0)*(V1+V0) (U1+V1) ]
	 *
	 *	w0 = U0*V0
	 *	w = (U1+U0)*(V1+V0)
	 *	w2 = (U1*V1)
	 *
	 *	w1 = w - w0 - w2
	 * w2x² + w1x + w0
	 *
	 *	See Knuth's Seminumerical Algorithms for a sample implemention
	 * using 4 stacks and k = [ 0 1 2 3 ... ], based on the size of the
	 * input.
	 */
long
mp_toom_mul(BNS *rop, BNS *op1, BNS *op2, BNI len1, BNI len2)
{
    long size, xsize, i;
    BNI value;				/* used in division */
    BNS carry;
    BNI x;				/* shift count */
    BNI l1, l2;
    BNI al, bl, cl, dl, el, Ul[3], Vl[3];
    BNS *a, *b, *c, *d, *e, *U[3], *V[3];

    /* x is the base i.e. 2^(BNSBITS*x) */
    x = (len1 + len2 + 4) / 6;
    l1 = len1 - (x << 1);	/* length of remaining piece of op1 */
    l2 = len2 - (x << 1);	/* length of remaining piece of op2 */

    /* allocate memory for storing U and V */
    U[0] = mp_malloc(sizeof(BNS) * (x + 2));
    V[0] = mp_malloc(sizeof(BNS) * (x + 2));
    U[1] = mp_malloc(sizeof(BNS) * (x + 1));
    V[1] = mp_malloc(sizeof(BNS) * (x + 1));
    U[2] = mp_malloc(sizeof(BNS) * (x + 2));
    V[2] = mp_malloc(sizeof(BNS) * (x + 2));

	/* EVALUATE U AND V */

    /* Numbers are in the format U2x²+U1x+U0 and V2x²+V1x+V0 */

	/* U[0] = U2+U1*2+U0*4 */

    /* store U1*2 in U[1], this value is used twice */
    Ul[1] = mp_lshift(U[1], op1 + x, x, 1);

    /* store U0*4 in U[0] */
    Ul[0] = mp_lshift(U[0], op1, x, 2);
    /* add U1*2 to U[0] */
    Ul[0] = mp_add(U[0], U[0], U[1], Ul[0], Ul[1]);
    /* add U2 to U[0] */
    Ul[0] = mp_add(U[0], U[0], op1 + x + x, Ul[0], l1);

	/* U[2] = U2*4+U1*2+U0 */

    /* store U2*4 in U[2] */
    Ul[2] = mp_lshift(U[2], op1 + x + x, l1, 2);
    /* add U1*2 to U[2] */
    Ul[2] = mp_add(U[2], U[2], U[1], Ul[2], Ul[1]);
    /* add U0 to U[2] */
    Ul[2] = mp_add(U[2], U[2], op1, Ul[2], x);

	/* U[1] = U2+U1+U0 */

    Ul[1] = mp_add(U[1], op1, op1 + x, x, x);
    Ul[1] = mp_add(U[1], U[1], op1 + x + x, Ul[1], l1);


	/* Evaluate V[x], same code as U[x] */
    Vl[1] = mp_lshift(V[1], op2 + x, x, 1);
    Vl[0] = mp_lshift(V[0], op2, x, 2);
    Vl[0] = mp_add(V[0], V[0], V[1], Vl[0], Vl[1]);
    Vl[0] = mp_add(V[0], V[0], op2 + x + x, Vl[0], l2);
    Vl[2] = mp_lshift(V[2], op2 + x + x, l2, 2);
    Vl[2] = mp_add(V[2], V[2], V[1], Vl[2], Vl[1]);
    Vl[2] = mp_add(V[2], V[2], op2, Vl[2], x);
    Vl[1] = mp_add(V[1], op2, op2 + x, x, x);
    Vl[1] = mp_add(V[1], V[1], op2 + x + x, Vl[1], l2);


	/* MULTIPLY U[] AND V[] */

	/* calculate (U2+U1*2+U0*4) * (V2+V1*2+V0*4) */
    b = mp_calloc(1, sizeof(BNS) * (Ul[0] * Vl[0]));
    bl = mp_mul(b, U[0], V[0], Ul[0], Vl[0]);
    mp_free(U[0]);
    mp_free(V[0]);

	/* calculate (U2+U1+U0) * (V2+V1+V0) */
    c = mp_calloc(1, sizeof(BNS) * (Ul[1] * Vl[1]));
    cl = mp_mul(c, U[1], V[1], Ul[1], Vl[1]);
    mp_free(U[1]);
    mp_free(V[1]);

	/* calculate (U2*4+U1*2+U0) * (V2*4+V1*2+V0) */
    d = mp_calloc(1, sizeof(BNS) * (Ul[2] * Vl[2]));
    dl = mp_mul(d, U[2], V[2], Ul[2], Vl[2]);
    mp_free(U[2]);
    mp_free(V[2]);

	/* calculate U0 * V0 */
    a = mp_calloc(1, sizeof(BNS) * (x + x));
    al = mp_mul(a, op1, op2, x, x);

	/* calculate U2 * V2 */
    e = mp_calloc(1, sizeof(BNS) * (l1 + l2));
    el = mp_mul(e, op1 + x + x, op2 + x + x, l1, l2);


	/* INTERPOLATE COEFFICIENTS */

    /* b = b - 16a - e */
    size = mp_lshift(rop, a, al, 4);
    bl = mp_sub(b, b, rop, bl, size);
    bl = mp_sub(b, b, e, bl, el);

    /* c = c - a - e*/
    cl = mp_sub(c, c, a, cl, al);
    cl = mp_sub(c, c, e, cl, el);

    /* d = d - a - 16e */
    dl = mp_sub(d, d, a, dl, al);
    size = mp_lshift(rop, e, el, 4);
    dl = mp_sub(d, d, rop, dl, size);

    /* w = (b + d) - 8c */
    size = mp_add(rop, b, d, bl, dl);
    xsize = mp_lshift(rop + size, c, cl, 3);	/* rop has enough storage */
    size = mp_sub(rop, rop, rop + size, size, xsize);

    /* w = 2c - w*/
    xsize = mp_lshift(rop + size, c, cl, 1);
    size = mp_sub(rop, rop + size, rop, xsize, size);

    /* b = b/2 */
    bl = mp_rshift(b, b, bl, 1);

    /* b = b - c */
    bl = mp_sub(b, b, c, bl, cl);

    /* c = w / 2 */
    cl = mp_rshift(c, rop, size, 1);

    /* b = b - c */
    bl = mp_sub(b, b, c, bl, cl);

    /* b = b/3 */
	/* maybe the most expensive calculation */
    i = bl - 1;
    value = b[i];
    b[i] = value / 3;
    for (--i; i >= 0; i--) {
	carry = value % 3;
	value = ((BNI)carry << BNSBITS) + b[i];
	b[i] = (BNS)(value / 3);
    }

    /* d = d/2 */
    dl = mp_rshift(d, d, dl, 1);

    /* d = d - b - w */
    dl = mp_sub(d, d, b, dl, bl);
    dl = mp_sub(d, d, rop, dl, size);

    /* d = d/4 */
    dl = mp_rshift(d, d, dl, 2);


	/* STORE RESULT IN ROP */
    /* first clear memory used as temporary variable w and 8c */
    memset(rop, '\0', sizeof(BNS) * (len1 + len2));

    i = x * 4;
    xsize = (len1 + len2) - i;
    size = mp_add(rop + i, rop + i, e, xsize, el) + i;
    i = x * 3;
    xsize = size - i;
    size = mp_add(rop + i, rop + i, d, xsize, dl) + i;
    i = x * 2;
    xsize = size - i;
    size = mp_add(rop + i, rop + i, c, xsize, cl) + i;
    i = x;
    xsize = size - i;
    size = mp_add(rop + i, rop + i, b, xsize, bl) + i;
    size = mp_add(rop, rop, a, size, al);

    mp_free(e);
    mp_free(d);
    mp_free(c);
    mp_free(b);
    mp_free(a);

    if (size > 1 && rop[size - 1] == 0)
	--size;

    return (size);
}

long
mp_mul(BNS *rop, BNS *op1, BNS *op2, BNI len1, BNI len2)
{
    if (len1 < len2)
	MP_SWAP(op1, op2, len1, len2);

    if (len1 < KARATSUBA || len2 < KARATSUBA)
	return (mp_base_mul(rop, op1, op2, len1, len2));
    else if (len1 < TOOM && len2 < TOOM && len2 > ((len1 + 1) >> 1))
	return (mp_karatsuba_mul(rop, op1, op2, len1, len2));
    else if (len1 >= TOOM && len2 >= TOOM && (len2 + 2) / 3 == (len1 + 2) / 3)
	return (mp_toom_mul(rop, op1, op2, len1, len2));
    else {
	long xsize, psize, isize;
	BNS *ptr;

	/* adjust index pointer and estimated size of result */
	isize = 0;
	xsize = len1 + len2;
	mp_mul(rop, op1, op2, len2, len2);
	/* adjust pointers */
	len1 -= len2;
	op1 += len2;

	/* allocate buffer for intermediate multiplications */
	if (len1 > len2)
	    ptr = mp_calloc(1, sizeof(BNS) * (len2 + len2));
	else
	    ptr = mp_calloc(1, sizeof(BNS) * (len1 + len2));

	/* loop multiplying len2 size operands at a time */
	while (len1 >= len2) {
	    isize += len2;
	    psize = mp_mul(ptr, op1, op2, len2, len2);
	    mp_add(rop + isize, rop + isize, ptr, xsize - isize, psize);
	    len1 -= len2;
	    op1 += len2;

	    /* multiplication routines require zeroed memory */
	    memset(ptr, '\0', sizeof(BNS) * (MIN(len1, len2) + len2));
	}

	/* len1 was not a multiple of len2 */
	if (len1) {
	    isize += len2;
	    psize = mp_mul(ptr, op2, op1, len2, len1);
	    mp_add(rop + isize, rop + isize, ptr, xsize, psize);
	}

	/* adjust result size */
	if (rop[xsize - 1] == 0)
	    --xsize;

	mp_free(ptr);

	return (xsize);
    }
}
