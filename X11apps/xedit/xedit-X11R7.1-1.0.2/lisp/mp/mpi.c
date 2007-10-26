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

/* $XFree86: xc/programs/xedit/lisp/mp/mpi.c,v 1.12 2002/11/20 07:44:43 paulo Exp $ */

#include "mp.h"

#ifdef __UNIXOS2__
# define finite(x) isfinite(x)
#endif

/*
 * Prototypes
 */
	/* do the hard work of mpi_add and mpi_sub */
static void mpi_addsub(mpi *rop, mpi *op1, mpi *op2, int sub);

	/* logical functions implementation */
static INLINE BNS mpi_logic(BNS op1, BNS op2, BNS op);
static void mpi_log(mpi *rop, mpi *op1, mpi *op2,  BNS op);

	/* internal mpi_seti, whithout memory allocation */
static void _mpi_seti(mpi *rop, long si);

/*
 * Initialization
 */
static BNS onedig[1] = { 1 };
static mpi mpone = { 1, 1, 0, (BNS*)&onedig };

/*
 * Implementation
 */
void
mpi_init(mpi *op)
{
    op->sign = 0;
    op->size = op->alloc = 1;
    op->digs = mp_malloc(sizeof(BNS));
    op->digs[0] = 0;
}

void
mpi_clear(mpi *op)
{
    op->sign = 0;
    op->size = op->alloc = 0;
    mp_free(op->digs);
}

void
mpi_set(mpi *rop, mpi *op)
{
    if (rop != op) {
	if (rop->alloc < op->size) {
	    rop->digs = mp_realloc(rop->digs, sizeof(BNS) * op->size);
	    rop->alloc = op->size;
	}
	rop->size = op->size;
	memcpy(rop->digs, op->digs, sizeof(BNS) * op->size);
	rop->sign = op->sign;
    }
}

void
mpi_seti(mpi *rop, long si)
{
    unsigned long ui;
    int sign = si < 0;
    int size;

    if (si == MINSLONG) {
	ui = MINSLONG;
	size = 2;
    }
    else {
	if (sign)
	    ui = -si;
	else
	    ui = si;
	if (ui < CARRY)
	    size = 1;
	else
	    size = 2;
    }

    if (rop->alloc < size) {
	rop->digs = mp_realloc(rop->digs, sizeof(BNS) * size);
	rop->alloc = size;
    }
    rop->size = size;

    /* store data in small mp integer */
    rop->digs[0] = (BNS)ui;
    if (size > 1)
	rop->digs[1] = (BNS)(ui >> BNSBITS);
    rop->size = size;

    /* adjust result sign */
    rop->sign = sign;
}

static void
_mpi_seti(mpi *rop, long si)
{
    unsigned long ui;
    int sign = si < 0;
    int size;

    if (si == MINSLONG) {
	ui = MINSLONG;
	size = 2;
    }
    else {
	if (sign)
	    ui = -si;
	else
	    ui = si;
	if (ui < CARRY)
	    size = 1;
	else
	    size = 2;
    }

    rop->digs[0] = (BNS)ui;
    if (size > 1)
	rop->digs[1] = (BNS)(ui >> BNSBITS);
    rop->size = size;

    rop->sign = sign;
}

void
mpi_setd(mpi *rop, double d)
{
    long i;
    double mantissa;
    int shift, exponent;
    BNI size;

    if (isnan(d))
	d = 0.0;
    else if (!finite(d))
	d = copysign(1.0, d) * DBL_MAX;

    /* check if number is larger than 1 */
    if (fabs(d) < 1.0) {
	rop->digs[0] = 0;
	rop->size = 1;
	rop->sign = d < 0.0;

	return;
    }

    mantissa = frexp(d, &exponent);
    if (mantissa < 0)
	mantissa = -mantissa;

    size = (exponent + (BNSBITS - 1)) / BNSBITS;
    shift = BNSBITS - (exponent & (BNSBITS - 1));

    /* adjust amount of memory */
    if (rop->alloc < size) {
	rop->digs = mp_realloc(rop->digs, sizeof(BNS) * size);
	rop->alloc = size;
    }
    rop->size = size;

    /* adjust the exponent */
    if (shift < BNSBITS)
	mantissa = ldexp(mantissa, -shift);

    /* convert double */
    for (i = size - 1; i >= 0 && mantissa != 0.0; i--) {
	mantissa = ldexp(mantissa, BNSBITS);
	rop->digs[i] = (BNS)mantissa;
	mantissa -= rop->digs[i];
    }
    for (; i >= 0; i--)
	rop->digs[i] = 0;

    /* normalize */
    if (size > 1 && rop->digs[size - 1] == 0)
	--rop->size;

    rop->sign = d < 0.0;
}

/* how many BNS in the given base, log(base) / log(CARRY) */
#ifdef LONG64
static double str_bases[37] = {
    0.0000000000000000, 0.0000000000000000, 0.0312500000000000,
    0.0495300781475362, 0.0625000000000000, 0.0725602529652301,
    0.0807800781475362, 0.0877298413143002, 0.0937500000000000,
    0.0990601562950723, 0.1038102529652301, 0.1081072380824156,
    0.1120300781475362, 0.1156387411919092, 0.1189798413143002,
    0.1220903311127662, 0.1250000000000000, 0.1277332137890731,
    0.1303101562950723, 0.1327477347951120, 0.1350602529652300,
    0.1372599194618363, 0.1393572380824156, 0.1413613111267817,
    0.1432800781475362, 0.1451205059304602, 0.1468887411919092,
    0.1485902344426084, 0.1502298413143002, 0.1518119060977367,
    0.1533403311127662, 0.1548186346995899, 0.1562500000000000,
    0.1576373162299517, 0.1589832137890731, 0.1602900942795302,
    0.1615601562950723,
};
#else
static double str_bases[37] = {
    0.0000000000000000, 0.0000000000000000, 0.0625000000000000,
    0.0990601562950723, 0.1250000000000000, 0.1451205059304602,
    0.1615601562950723, 0.1754596826286003, 0.1875000000000000,
    0.1981203125901446, 0.2076205059304602, 0.2162144761648311,
    0.2240601562950723, 0.2312774823838183, 0.2379596826286003,
    0.2441806622255325, 0.2500000000000000, 0.2554664275781462,
    0.2606203125901445, 0.2654954695902241, 0.2701205059304602,
    0.2745198389236725, 0.2787144761648311, 0.2827226222535633,
    0.2865601562950723, 0.2902410118609203, 0.2937774823838183,
    0.2971804688852168, 0.3004596826286003, 0.3036238121954733,
    0.3066806622255324, 0.3096372693991797, 0.3125000000000000,
    0.3152746324599034, 0.3179664275781462, 0.3205801885590604,
    0.3231203125901446,
};
#endif

void
mpi_setstr(mpi *rop, char *str, int base)
{
    long i;			/* counter */
    int sign;			/* result sign */
    BNI carry;			/* carry value */
    BNI value;			/* temporary value */
    BNI size;			/* size of result */
    char *ptr;			/* end of valid input */

    /* initialization */
    sign = 0;
    carry = 0;

    /* skip leading spaces */
    while (isspace(*str))
	++str;

    /* check if sign supplied */
    if (*str == '-') {
	sign = 1;
	++str;
    }
    else if (*str == '+')
	++str;

    /* skip leading zeros */
    while (*str == '0')
	++str;

    ptr = str;
    while (*ptr) {
	if (*ptr >= '0' && *ptr <= '9') {
	    if (*ptr - '0' >= base)
		break;
	}
	else if (*ptr >= 'A' && *ptr <= 'Z') {
	    if (*ptr - 'A' + 10 >= base)
		break;
	}
	else if (*ptr >= 'a' && *ptr <= 'z') {
	    if (*ptr - 'a' + 10 >= base)
		break;
	}
	else
	    break;
	++ptr;
    }

    /* resulting size */
    size = (ptr - str) * str_bases[base] + 1;

    /* make sure rop has enough storage */
    if (rop->alloc < size) {
	rop->digs = mp_realloc(rop->digs, size * sizeof(BNS));
	rop->alloc = size;
    }
    rop->size = size;

    /* initialize rop to zero */
    memset(rop->digs, '\0', size * sizeof(BNS));

    /* set result sign */
    rop->sign = sign;

    /* convert string */
    for (; str < ptr; str++) {
	value = *str;
	if (islower(value))
	    value = toupper(value);
	value = value > '9' ? value - 'A' + 10 : value - '0';
	value += rop->digs[0] * base;
	carry = value >> BNSBITS;
	rop->digs[0] = (BNS)value;
	for (i = 1; i < size; i++) {
	    value = (BNI)rop->digs[i] * base + carry;
	    carry = value >> BNSBITS;
	    rop->digs[i] = (BNS)value;
	}
    }

    /* normalize */
    if (rop->size > 1 && rop->digs[rop->size - 1] == 0)
	--rop->size;
}

void
mpi_add(mpi *rop, mpi *op1, mpi *op2)
{
    mpi_addsub(rop, op1, op2, 0);
}

void
mpi_addi(mpi *rop, mpi *op1, long op2)
{
    BNS digs[2];
    mpi op;

    op.digs = (BNS*)digs;
    _mpi_seti(&op, op2);

    mpi_addsub(rop, op1, &op, 0);
}

void
mpi_sub(mpi *rop, mpi *op1, mpi *op2)
{
    mpi_addsub(rop, op1, op2, 1);
}

void
mpi_subi(mpi *rop, mpi *op1, long op2)
{
    BNS digs[2];
    mpi op;

    op.digs = (BNS*)digs;
    _mpi_seti(&op, op2);

    mpi_addsub(rop, op1, &op, 1);
}

static void
mpi_addsub(mpi *rop, mpi *op1, mpi *op2, int sub)
{
    long xlen;				/* maximum result size */

    if (sub ^ (op1->sign == op2->sign)) {
	/* plus one for possible carry */
	xlen = MAX(op1->size, op2->size) + 1;
	if (rop->alloc < xlen) {
	    rop->digs = mp_realloc(rop->digs, sizeof(BNS) * xlen);
	    rop->alloc = xlen;
	}
	rop->size = mp_add(rop->digs, op1->digs, op2->digs,
			   op1->size, op2->size);
	rop->sign = op1->sign;
    }
    else {
	long cmp;			/* check for larger operator */

	cmp = mpi_cmpabs(op1, op2);
	if (cmp == 0) {
	    rop->digs[0] = 0;
	    rop->size = 1;
	    rop->sign = 0;
	}
	else {
	    xlen = MAX(op1->size, op2->size);
	    if (rop->alloc < xlen) {
		rop->digs = mp_realloc(rop->digs, sizeof(BNS) * xlen);
		rop->alloc = xlen;
	    }
	    if (cmp > 0) {
		rop->size = mp_sub(rop->digs, op1->digs, op2->digs,
				   op1->size, op2->size);
		rop->sign = op1->sign;
	    }
	    else {
		rop->size = mp_sub(rop->digs, op2->digs, op1->digs,
				   op2->size, op1->size);
		rop->sign = sub ^ op2->sign;
	    }
	}
    }
}

void
mpi_mul(mpi *rop, mpi *op1, mpi *op2)
{
    int sign;				/* sign flag */
    BNS *digs;				/* result data */
    long xsize;				/* result size */

    /* get result sign */
    sign = op1->sign ^ op2->sign;

    /* check for special cases */
    if (op1->size == 1) {
	if (*op1->digs == 0) {
	    /* multiply by 0 */
	    mpi_seti(rop, 0);
	    return;
	}
	else if (*op1->digs == 1) {
	    /* multiply by +-1 */
	    if (rop->alloc < op2->size) {
		rop->digs = mp_realloc(rop->digs, sizeof(BNS) * op2->size);
		rop->alloc = op2->size;
	    }
	    rop->size = op2->size;
	    memmove(rop->digs, op2->digs, sizeof(BNS) * op2->size);
	    rop->sign = op2->size > 1 || *op2->digs ? sign : 0;

	    return;
	}
    }
    else if (op2->size == 1) {
	if (*op2->digs == 0) {
	    /* multiply by 0 */
	    mpi_seti(rop, 0);
	    return;
	}
	else if (*op2->digs == 1) {
	    /* multiply by +-1 */
	    if (rop->alloc < op1->size) {
		rop->digs = mp_realloc(rop->digs, sizeof(BNS) * op1->size);
		rop->alloc = op1->size;
	    }
	    rop->size = op1->size;
	    memmove(rop->digs, op1->digs, sizeof(BNS) * op1->size);
	    rop->sign = op1->size > 1 || *op1->digs ? sign : 0;

	    return;
	}
    }

    /* allocate result data and set it to zero */
    xsize = op1->size + op2->size;
    if (rop->digs == op1->digs || rop->digs == op2->digs)
	/* rop is also an operand */
	digs = mp_calloc(1, sizeof(BNS) * xsize);
    else {
	if (rop->alloc < xsize) {
	    rop->digs = mp_realloc(rop->digs, sizeof(BNS) * xsize);
	    rop->alloc = xsize;
	}
	digs = rop->digs;
	memset(digs, '\0', sizeof(BNS) * xsize);
    }

    /* multiply operands */
    xsize = mp_mul(digs, op1->digs, op2->digs, op1->size, op2->size);

    /* store result in rop */
    if (digs != rop->digs) {
	/* if rop was an operand, free old data */
	mp_free(rop->digs);
	rop->digs = digs;
    }
    rop->size = xsize;

    /* set result sign */
    rop->sign = sign;
}

void
mpi_muli(mpi *rop, mpi *op1, long op2)
{
    BNS digs[2];
    mpi op;

    op.digs = (BNS*)digs;
    _mpi_seti(&op, op2);

    mpi_mul(rop, op1, &op);
}

void
mpi_div(mpi *rop, mpi *num, mpi *den)
{
    mpi_divqr(rop, NULL, num, den);
}

void
mpi_rem(mpi *rop, mpi *num, mpi *den)
{
    mpi_divqr(NULL, rop, num, den);
}

/*
 * Could/should be changed to not allocate qdigs if qrop is NULL
 * Performance wouldn't suffer too much with a test on every loop iteration.
 */
void
mpi_divqr(mpi *qrop, mpi *rrop, mpi *num, mpi *den)
{
    long i, j;			/* counters */
    int qsign;			/* sign of quotient */
    int rsign;			/* sign of remainder */
    BNI qsize;			/* size of quotient */
    BNI rsize;			/* size of remainder */
    BNS qest;			/* estimative of quotient value */
    BNS *qdigs, *rdigs;		/* work copy or result */
    BNS *ndigs, *ddigs;		/* work copy or divisor and dividend */
    BNI value;			/* temporary result */
    long svalue;		/* signed temporary result (2's complement) */
    BNS carry, scarry, denorm;	/* carry and normalization */
    BNI dpos, npos;		/* offsets in data */

    /* get signs */
    rsign = num->sign;
    qsign = rsign ^ den->sign;

    /* check for special case */
    if (num->size < den->size) {
	/* quotient is zero and remainder is numerator */
	if (rrop && rrop->digs != num->digs) {
	    if (rrop->alloc < num->size) {
		rrop->digs = mp_realloc(rrop->digs, sizeof(BNS) * num->size);
		rrop->alloc = num->size;
	    }
	    rrop->size = num->size;
	    memcpy(rrop->digs, num->digs, sizeof(BNS) * num->size);
	    rrop->sign = rsign;
	}
	if (qrop)
	    mpi_seti(qrop, 0);

	return;
    }

    /* estimate result sizes */
    rsize = den->size;
    qsize = num->size - den->size + 1;

    /* offsets */
    npos = num->size - 1;
    dpos = den->size - 1;

    /* allocate space for quotient and remainder */
    if (qrop == NULL || qrop->digs == num->digs || qrop->digs == den->digs)
	qdigs = mp_calloc(1, sizeof(BNS) * qsize);
    else {
	if (qrop->alloc < qsize) {
	    qrop->digs = mp_realloc(qrop->digs, sizeof(BNS) * qsize);
	    qrop->alloc = qsize;
	}
	memset(qrop->digs, '\0', sizeof(BNS) * qsize);
	qdigs = qrop->digs;
    }
    if (rrop) {
	if (rrop->digs == num->digs || rrop->digs == den->digs)
	    rdigs = mp_calloc(1, sizeof(BNS) * rsize);
	else {
	    if (rrop->alloc < rsize) {
		rrop->digs = mp_realloc(rrop->digs, sizeof(BNS) * rsize);
		rrop->alloc = rsize;
	    }
	    memset(rrop->digs, '\0', sizeof(BNS) * rsize);
	    rdigs = rrop->digs;
	}
    }
    else
	rdigs = NULL;	/* fix gcc warning */

    /* special case, only one word in divisor */
    if (dpos == 0) {
	for (carry = 0, i = npos; i >= 0; i--) {
	    value = ((BNI)carry << BNSBITS) + num->digs[i];
	    qdigs[i] = (BNS)(value / den->digs[0]);
	    carry = (BNS)(value % den->digs[0]);
	}
	if (rrop)
	    rdigs[0] = carry;

	goto mpi_divqr_done;
    }

    /* make work copy of numerator */
    ndigs = mp_malloc(sizeof(BNS) * (num->size + 1));
    /* allocate one extra word an update offset */
    memcpy(ndigs, num->digs, sizeof(BNS) * num->size);
    ndigs[num->size] = 0;
    ++npos;

    /* normalize */
    denorm = (BNS)((BNI)CARRY / ((BNI)(den->digs[dpos]) + 1));

    if (denorm > 1) {
	/* i <= num->size because ndigs has an extra word */
	for (carry = 0, i = 0; i <= num->size; i++) {
	    value = ndigs[i] * (BNI)denorm + carry;
	    ndigs[i] = (BNS)value;
	    carry = (BNS)(value >> BNSBITS);
	}
	/* make work copy of denominator */
	ddigs = mp_malloc(sizeof(BNS) * den->size);
	memcpy(ddigs, den->digs, sizeof(BNS) * den->size);
	for (carry = 0, i = 0; i < den->size; i++) {
	    value = ddigs[i] * (BNI)denorm + carry;
	    ddigs[i] = (BNS)value;
	    carry = (BNS)(value >> BNSBITS);
	}
    }
    else
	/* only allocate copy of denominator if going to change it */
	ddigs = den->digs;

    /* divide mp integers */
    for (j = qsize - 1; j >= 0; j--, npos--) {
	/* estimate quotient */
	if (ndigs[npos] == ddigs[dpos])
	    qest = (BNS)SMASK;
	else
	    qest = (BNS)((((BNI)(ndigs[npos]) << 16) + ndigs[npos - 1]) /
			 ddigs[dpos]);

	while ((value = ((BNI)(ndigs[npos]) << 16) + ndigs[npos - 1] -
	        qest * (BNI)(ddigs[dpos])) < CARRY &&
		ddigs[dpos - 1] * (BNI)qest >
		(value << BNSBITS) + ndigs[npos - 2])
	       --qest;

	/* multiply and subtract */
	carry = scarry = 0;
	for (i = 0; i < den->size; i++) {
	    value = qest * (BNI)ddigs[i] + carry;
	    carry = (BNS)(value >> BNSBITS);
	    svalue = (long)ndigs[npos - dpos + i - 1] - (long)(value & SMASK) -
		     (long)scarry;
	    ndigs[npos - dpos + i - 1] = (BNS)svalue;
	    scarry = svalue < 0;
	}

	svalue = (long)ndigs[npos] - (long)(carry & SMASK) - (long)scarry;
	ndigs[npos] = (BNS)svalue;

	if (svalue & LMASK) {
	    /* quotient too big */
	    --qest;
	    carry = 0;
	    for (i = 0; i < den->size; i++) {
		value = ndigs[npos - dpos + i - 1] + (BNI)carry + (BNI)ddigs[i];
		ndigs[npos - dpos + i - 1] = (BNS)value;
		carry = (BNS)(value >> BNSBITS);
	    }
	    ndigs[npos] += carry;
	}

	qdigs[j] = qest;
    }

    /* calculate remainder */
    if (rrop) {
	for (carry = 0, j = dpos; j >= 0; j--) {
	    value = ((BNI)carry << BNSBITS) + ndigs[j];
	    rdigs[j] = (BNS)(value / denorm);
	    carry = (BNS)(value % denorm);
	}
    }

    mp_free(ndigs);
    if (ddigs != den->digs)
	mp_free(ddigs);

mpi_divqr_done:
    if (rrop) {
	if (rrop->digs != rdigs)
	    mp_free(rrop->digs);
	/* normalize remainder */
	for (i = rsize - 1; i >= 0; i--)
	    if (rdigs[i] != 0)
		break;
	if (i != rsize - 1) {
	    if (i < 0) {
		rsign = 0;
		rsize = 1;
	    }
	    else
		rsize = i + 1;
	}
	rrop->digs = rdigs;
	rrop->sign = rsign;
	rrop->size = rsize;
    }

    /* normalize quotient */
    if (qrop) {
	if (qrop->digs != qdigs)
	    mp_free(qrop->digs);
	for (i = qsize - 1; i >= 0; i--)
	    if (qdigs[i] != 0)
		break;
	if (i != qsize - 1) {
	    if (i < 0) {
		qsign = 0;
		qsize = 1;
	    }
	    else
		qsize = i + 1;
	}
	qrop->digs = qdigs;
	qrop->sign = qsign;
	qrop->size = qsize;
    }
    else
	mp_free(qdigs);
}

long
mpi_divqri(mpi *qrop, mpi *num, long den)
{
    BNS ddigs[2];
    mpi dop, rrop;
    long remainder;

    dop.digs = (BNS*)ddigs;
    _mpi_seti(&dop, den);

    memset(&rrop, '\0', sizeof(mpi));
    mpi_init(&rrop);
    mpi_divqr(qrop, &rrop, num, &dop);
    remainder = rrop.digs[0];
    if (rrop.size > 1)
	remainder |= (BNI)(rrop.digs[1]) << BNSBITS;
    if (rrop.sign)
	remainder = -remainder;
    mpi_clear(&rrop);

    return (remainder);
}

void
mpi_divi(mpi *rop, mpi *num, long den)
{
    BNS ddigs[2];
    mpi dop;

    dop.digs = (BNS*)ddigs;
    _mpi_seti(&dop, den);

    mpi_divqr(rop, NULL, num, &dop);
}

long
mpi_remi(mpi *num, long den)
{
    return (mpi_divqri(NULL, num, den));
}

void
mpi_mod(mpi *rop, mpi *num, mpi *den)
{
    mpi_rem(rop, num, den);
    if (num->sign ^ den->sign)
	mpi_add(rop, rop, den);
}

long
mpi_modi(mpi *num, long den)
{
    long remainder;

    remainder = mpi_remi(num, den);
    if (num->sign ^ (den < 0))
	remainder += den;

    return (remainder);
}

void
mpi_gcd(mpi *rop, mpi *num, mpi *den)
{
    long cmp;
    mpi rem;

    /* check if result already given */
    cmp = mpi_cmpabs(num, den);

    /* check if num is equal to den or if num is zero */
    if (cmp == 0 || (num->size == 1 && num->digs[0] == 0)) {
	mpi_set(rop, den);
	rop->sign = 0;
	return;
    }
    /* check if den is not zero */
    if (den->size == 1 && den->digs[0] == 0) {
	mpi_set(rop, num);
	rop->sign = 0;
	return;
    }

    /* don't call mpi_init, relies on realloc(0, size) == malloc(size) */
    memset(&rem, '\0', sizeof(mpi));

    /* if num larger than den */
    if (cmp > 0) {
	mpi_rem(&rem, num, den);
	if (rem.size == 1 && rem.digs[0] == 0) {
	    /* exact division */
	    mpi_set(rop, den);
	    rop->sign = 0;
	    mpi_clear(&rem);
	    return;
	}
	mpi_set(rop, den);
    }
    else {
	mpi_rem(&rem, den, num);
	if (rem.size == 1 && rem.digs[0] == 0) {
	    /* exact division */
	    mpi_set(rop, num);
	    rop->sign = 0;
	    mpi_clear(&rem);
	    return;
	}
	mpi_set(rop, num);
    }

    /* loop using positive values */
    rop->sign = rem.sign = 0;

    /* cannot optimize this inverting rem/rop assignment earlier
     * because rop mais be an operand */
    mpi_swap(rop, &rem);

    /* Euclides algorithm */
    for (;;) {
	mpi_rem(&rem, &rem, rop);
	if (rem.size == 1 && rem.digs[0] == 0)
	    break;
	mpi_swap(rop, &rem);
    }
    mpi_clear(&rem);
}

void
mpi_lcm(mpi *rop, mpi *num, mpi *den)
{
    mpi gcd;

    /* check for zero operand */
    if ((num->size == 1 && num->digs[0] == 0) ||
	(den->size == 1 && den->digs[0] == 0)) {
	rop->digs[0] = 0;
	rop->sign = 0;
	return;
    }

    /* don't call mpi_init, relies on realloc(0, size) == malloc(size) */
    memset(&gcd, '\0', sizeof(mpi));

    mpi_gcd(&gcd, num, den);
    mpi_div(&gcd, den, &gcd);
    mpi_mul(rop, &gcd, num);
    rop->sign = 0;

    mpi_clear(&gcd);
}

void
mpi_pow(mpi *rop, mpi *op, unsigned long exp)
{
    mpi zop, top;

    if (exp == 2) {
	mpi_mul(rop, op, op);
	return;
    }
    /* check for op**0 */
    else if (exp == 0) {
	rop->digs[0] = 1;
	rop->size = 1;
	rop->sign = 0;
	return;
    }
    else if (exp == 1) {
	mpi_set(rop, op);
	return;
    }
    else if (op->size == 1) {
	if (op->digs[0] == 0) {
	    mpi_seti(rop, 0);
	    return;
	}
	else if (op->digs[0] == 1) {
	    mpi_seti(rop, op->sign && (exp & 1) ? -1 : 1);
	    return;
	}
    }

    memset(&zop, '\0', sizeof(mpi));
    memset(&top, '\0', sizeof(mpi));
    mpi_set(&zop, op);
    mpi_set(&top, op);
    for (--exp; exp; exp >>= 1) {
	if (exp & 1)
	    mpi_mul(&zop, &top, &zop);
	mpi_mul(&top, &top, &top);
    }

    mpi_clear(&top);
    rop->sign = zop.sign;
    mp_free(rop->digs);
    rop->digs = zop.digs;
    rop->size = zop.size;
}

/* Find integer root of given number using the iteration
 *	x{n+1} = ((K-1) * x{n} + N / x{n}^(K-1)) / K
 */
int
mpi_root(mpi *rop, mpi *op, unsigned long nth)
{
    long bits, cmp;
    int exact;
    int sign;
    mpi *r, t, temp, quot, old, rem;

    sign = op->sign;

    /* divide by zero op**1/0 */
    if (nth == 0) {
	int one = 1, zero = 0;
	one = one / zero;
    }
    /* result is complex */
    if (sign && !(nth & 1)) {
	int one = 1, zero = 0;
	one = one / zero;
    }

    /* special case op**1/1 = op */
    if (nth == 1) {
	mpi_set(rop, op);
	return (1);
    }

    bits = mpi_getsize(op, 2) - 2;

    if (bits < 0 || bits / nth == 0) {
	/* integral root is surely less than 2 */
	exact = op->size == 1 && (op->digs[0] == 1 || op->digs[0] == 0);
	mpi_seti(rop, sign ? -1 : op->digs[0] == 0 ? 0 : 1);

	return (exact == 1);
    }

    /* initialize */
    if (rop != op)
	r = rop;
    else {
	r = &t;
	memset(r, '\0', sizeof(mpi));
    }
    memset(&temp, '\0', sizeof(mpi));
    memset(&quot, '\0', sizeof(mpi));
    memset(&old, '\0', sizeof(mpi));
    memset(&rem, '\0', sizeof(mpi));

    if (sign)
	r->sign = 0;

    /* root aproximation */
    mpi_ash(r, op, -(bits - (bits / nth)));

    for (;;) {
	mpi_pow(&temp, r, nth - 1);
	mpi_divqr(&quot, &rem, op, &temp);
	cmp = mpi_cmpabs(r, &quot);
	if (cmp == 0) {
	    exact = mpi_cmpi(&rem, 0) == 0;
	    break;
	}
	else if (cmp < 0) {
	    if (mpi_cmpabs(r, &old) == 0) {
		exact = 0;
		break;
	    }
	    mpi_set(&old, r);
	}
	mpi_muli(&temp, r, nth - 1);
	mpi_add(&quot, &quot, &temp);
	mpi_divi(r, &quot, nth);
    }

    mpi_clear(&temp);
    mpi_clear(&quot);
    mpi_clear(&old);
    mpi_clear(&rem);
    if (r != rop) {
	mpi_set(rop, r);
	mpi_clear(r);
    }
    rop->sign = sign;

    return (exact);
}

/*
 * Find square root using the iteration:
 *	x{n+1} = (x{n}+N/x{n})/2
 */
int
mpi_sqrt(mpi *rop, mpi *op)
{
    long bits, cmp;
    int exact;
    mpi *r, t, quot, rem, old;

    /* result is complex */
    if (op->sign) {
	int one = 1, zero = 0;
	one = one / zero;
    }

    bits = mpi_getsize(op, 2) - 2;

    if (bits < 2) {
	/* integral root is surely less than 2 */
	exact = op->size == 1 && (op->digs[0] == 1 || op->digs[0] == 0);
	mpi_seti(rop, op->digs[0] == 0 ? 0 : 1);

	return (exact == 1);
    }

    /* initialize */
    if (rop != op)
	r = rop;
    else {
	r = &t;
	memset(r, '\0', sizeof(mpi));
    }
    memset(&quot, '\0', sizeof(mpi));
    memset(&rem, '\0', sizeof(mpi));
    memset(&old, '\0', sizeof(mpi));

    /* root aproximation */
    mpi_ash(r, op, -(bits - (bits / 2)));

    for (;;) {
	if (mpi_cmpabs(r, &old) == 0) {
	    exact = 0;
	    break;
	}
	mpi_divqr(&quot, &rem, op, r);
	cmp = mpi_cmpabs(&quot, r);
	if (cmp == 0) {
	    exact = mpi_cmpi(&rem, 0) == 0;
	    break;
	}
	else if (cmp > 0 && rem.size == 1 && rem.digs[0] == 0)
	    mpi_subi(&quot, &quot, 1);
	mpi_set(&old, r);
	mpi_add(r, r, &quot);
	mpi_ash(r, r, -1);
    }
    mpi_clear(&quot);
    mpi_clear(&rem);
    mpi_clear(&old);
    if (r != rop) {
	mpi_set(rop, r);
	mpi_clear(r);
    }

    return (exact);
}

void
mpi_ash(mpi *rop, mpi *op, long shift)
{
    long i;			/* counter */
    long xsize;			/* maximum result size */
    BNS *digs;

    /* check for 0 shift, multiply/divide by 1 */
    if (shift == 0) {
	if (rop != op) {
	    if (rop->alloc < op->size) {
		rop->digs = mp_realloc(rop->digs, sizeof(BNS) * op->size);
		rop->alloc = op->size;
	    }
	    rop->size = op->size;
	    memcpy(rop->digs, op->digs, sizeof(BNS) * op->size);
	}

	return;
    }
    else if (op->size == 1 && op->digs[0] == 0) {
	rop->sign = 0;
	rop->size = 1;
	rop->digs[0] = 0;

	return;
    }

    /* check shift and initialize */
    if (shift > 0)
	xsize = op->size + (shift / BNSBITS) + 1;
    else {
	xsize = op->size - ((-shift) / BNSBITS);
	if (xsize <= 0) {
	    rop->size = 1;
	    rop->sign = op->sign;
	    rop->digs[0] = op->sign ? 1 : 0;

	    return;
	}
    }

    /* allocate/adjust memory for result */
    if (rop == op)
	digs = mp_malloc(sizeof(BNS) * xsize);
    else {
	if (rop->alloc < xsize) {
	    rop->digs = mp_realloc(rop->digs, sizeof(BNS) * xsize);
	    rop->alloc = xsize;
	}
	digs = rop->digs;
    }

    /* left shift, multiply by power of two */
    if (shift > 0)
	rop->size = mp_lshift(digs, op->digs, op->size, shift);
    /* right shift, divide by power of two */
    else {
	long carry = 0;

	if (op->sign) {
	    BNI words, bits;

	    words = -shift / BNSBITS;
	    bits = -shift % BNSBITS;
	    for (i = 0; i < words; i++)
		carry |= op->digs[xsize + i];
	    if (!carry) {
		for (i = 0; i < bits; i++)
		    if (op->digs[op->size - xsize] & (1 << i)) {
			carry = 1;
			break;
		    }
	    }
	}
	rop->size = mp_rshift(digs, op->digs, op->size, -shift);

	if (carry)
	    /* emulates two's complement subtracting 1 from the result */
	    rop->size = mp_add(digs, digs, mpone.digs, rop->size, 1);
    }

    if (rop->digs != digs) {
	mp_free(rop->digs);
	rop->alloc = rop->size;
	rop->digs = digs;
    }
    rop->sign = op->sign;
}

static INLINE BNS
mpi_logic(BNS op1, BNS op2, BNS op)
{
    switch (op) {
	case '&':
	    return (op1 & op2);
	case '|':
	    return (op1 | op2);
	case '^':
	    return (op1 ^ op2);
    }

    return (SMASK);
}

static void
mpi_log(mpi *rop, mpi *op1, mpi *op2, BNS op)
{
    long i;			/* counter */
    long c, c1, c2;		/* carry */
    BNS *digs, *digs1, *digs2;	/* pointers to mp data */
    BNI size, size1, size2;
    BNS sign, sign1, sign2;
    BNS n, n1, n2;		/* logical operands */
    BNI sum;

    /* initialize */
    size1 = op1->size;
    size2 = op2->size;

    sign1 = op1->sign ? SMASK : 0;
    sign2 = op2->sign ? SMASK : 0;

    sign = mpi_logic(sign1, sign2, op);

    size = MAX(size1, size2);
    if (sign)
	++size;
    if (rop->alloc < size) {
	rop->digs = mp_realloc(rop->digs, sizeof(BNS) * size);
	rop->alloc = size;
    }

    digs = rop->digs;
    digs1 = op1->digs;
    digs2 = op2->digs;

    c = c1 = c2 = 1;

    /* apply logical operation */
    for (i = 0; i < size; i++) {
	if (i >= size1)
	    n1 = sign1;
	else if (sign1) {
	    sum = (BNI)(BNS)(~digs1[i]) + c1;
	    c1 = (long)(sum >> BNSBITS);
	    n1 = (BNS)sum;
	}
	else
	    n1 = digs1[i];

	if (i >= size2)
	    n2 = sign2;
	else if (sign2) {
	    sum = (BNI)(BNS)(~digs2[i]) + c2;
	    c2 = (long)(sum >> BNSBITS);
	    n2 = (BNS)sum;
	}
	else
	    n2 = digs2[i];

	n = mpi_logic(n1, n2, op);
	if (sign) {
	    sum = (BNI)(BNS)(~n) + c;
	    c = (long)(sum >> BNSBITS);
	    digs[i] = (BNS)sum;
	}
	else
	    digs[i] = n;
    }

    /* normalize */
    for (i = size - 1; i >= 0; i--)
	if (digs[i] != 0)
	    break;
    if (i != size - 1) {
	if (i < 0) {
	    sign = 0;
	    size = 1;
	}
	else
	    size = i + 1;
    }

    rop->sign = sign != 0;
    rop->size = size;
}

void
mpi_and(mpi *rop, mpi *op1, mpi *op2)
{
    mpi_log(rop, op1, op2, '&');
}

void
mpi_ior(mpi *rop, mpi *op1, mpi *op2)
{
    mpi_log(rop, op1, op2, '|');
}

void
mpi_xor(mpi *rop, mpi *op1, mpi *op2)
{
    mpi_log(rop, op1, op2, '^');
}

void
mpi_com(mpi *rop, mpi *op)
{
    static BNS digs[1] = { 1 };
    static mpi one = { 0, 1, 1, (BNS*)&digs };

    mpi_log(rop, rop, &one, '^');
}

void
mpi_neg(mpi *rop, mpi *op)
{
    int sign = op->sign ^ 1;

    if (rop->digs != op->digs) {
	if (rop->alloc < op->size) {
	    rop->digs = mp_realloc(rop->digs, sizeof(BNS) * rop->size);
	    rop->alloc = op->size;
	}
	rop->size = op->size;
	memcpy(rop->digs, op->digs, sizeof(BNS) * rop->size);
    }

    rop->sign = sign;
}

void
mpi_abs(mpi *rop, mpi *op)
{
    if (rop->digs != op->digs) {
	if (rop->alloc < op->size) {
	    rop->digs = mp_realloc(rop->digs, sizeof(BNS) * rop->size);
	    rop->alloc = op->size;
	}
	rop->size = op->size;
	memcpy(rop->digs, op->digs, sizeof(BNS) * rop->size);
    }

    rop->sign = 0;
}

int
mpi_cmp(mpi *op1, mpi *op2)
{
    if (op1->sign ^ op2->sign)
	return (op1->sign ? -1 : 1);

    if (op1->size == op2->size) {
	long i, cmp = 0;

	for (i = op1->size - 1; i >= 0; i--)
	    if ((cmp = (long)op1->digs[i] - (long)op2->digs[i]) != 0)
		break;

	return (cmp == 0 ? 0 : (cmp < 0) ^ op1->sign ? -1 : 1);
    }

    return ((op1->size < op2->size) ^ op1->sign ? -1 : 1);
}

int
mpi_cmpi(mpi *op1, long op2)
{
    long cmp;

    if (op1->size > 2)
	return (op1->sign ? -1 : 1);

    cmp = op1->digs[0];
    if (op1->size == 2) {
	cmp |= (long)op1->digs[1] << BNSBITS;
	if (cmp == MINSLONG)
	    return (op2 == cmp && op1->sign ? 0 : op1->sign ? -1 : 1);
    }
    if (op1->sign)
	cmp = -cmp;

    return (cmp - op2);
}

int
mpi_cmpabs(mpi *op1, mpi *op2)
{
    if (op1->size == op2->size) {
	long i, cmp = 0;

	for (i = op1->size - 1; i >= 0; i--)
	    if ((cmp = (long)op1->digs[i] - (long)op2->digs[i]) != 0)
		break;

	return (cmp);
    }

    return ((op1->size < op2->size) ? -1 : 1);
}

int
mpi_cmpabsi(mpi *op1, long op2)
{
    unsigned long cmp;

    if (op1->size > 2)
	return (1);

    cmp = op1->digs[0];
    if (op1->size == 2)
	cmp |= (unsigned long)op1->digs[1] << BNSBITS;

    return (cmp > op2 ? 1 : cmp == op2 ? 0 : -1);
}

int
mpi_sgn(mpi *op)
{
    return (op->sign ? -1 : op->size > 1 || op->digs[0] ? 1 : 0);
}

void
mpi_swap(mpi *op1, mpi *op2)
{
    if (op1 != op2) {
	mpi swap;

	memcpy(&swap, op1, sizeof(mpi));
	memcpy(op1, op2, sizeof(mpi));
	memcpy(op2, &swap, sizeof(mpi));
    }
}

int
mpi_fiti(mpi *op)
{
    if (op->size == 1)
	return (1);
    else if (op->size == 2) {
	unsigned long value = ((BNI)(op->digs[1]) << BNSBITS) | op->digs[0];

	if (value & MINSLONG)
	    return (op->sign && value == MINSLONG) ? 1 : 0;

	return (1);
    }

    return (0);
}

long
mpi_geti(mpi *op)
{
    long value;

    value = op->digs[0];
    if (op->size > 1)
	value |= (BNI)(op->digs[1]) << BNSBITS;

    return (op->sign && value != MINSLONG ? -value : value);
}

double
mpi_getd(mpi *op)
{
    long i, len;
    double d = 0.0;
    int exponent;

#define FLOATDIGS	sizeof(double) / sizeof(BNS)

    switch (op->size) {
	case 2:
	    d = (BNI)(op->digs[1]) << BNSBITS;
	case 1:
	    d += op->digs[0];
	    return (op->sign ? -d : d);
	default:
	    break;
    }

    for (i = 0, len = op->size; len > 0 && i < FLOATDIGS; i++)
	d = ldexp(d, BNSBITS) + op->digs[--len];
    d = frexp(d, &exponent);
    if (len > 0)
	exponent += len * BNSBITS;

    if (d == 0.0)
	return (d);

    d = ldexp(d, exponent);

    return (op->sign ? -d : d);
}

/* how many digits in a given base, floor(log(CARRY) / log(base)) */
#ifdef LONG64
static char dig_bases[37] = {
     0,  0, 32, 20, 16, 13, 12, 11, 10, 10,  9,  9,  8,  8,  8,  8,
     8,  7,  7,  7,  7,  7,  7,  7,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,
};
#else
static char dig_bases[37] = {
     0,  0, 16, 10,  8,  6,  6,  5,  5,  5,  4,  4,  4,  4,  4,  4,
     4,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,
};
#endif

/* how many digits per bit in a given base, log(2) / log(base) */
static double bit_bases[37] = {
    0.0000000000000000, 0.0000000000000000, 1.0000000000000000,
    0.6309297535714575, 0.5000000000000000, 0.4306765580733931,
    0.3868528072345416, 0.3562071871080222, 0.3333333333333334,
    0.3154648767857287, 0.3010299956639811, 0.2890648263178878,
    0.2789429456511298, 0.2702381544273197, 0.2626495350371936,
    0.2559580248098155, 0.2500000000000000, 0.2446505421182260,
    0.2398124665681315, 0.2354089133666382, 0.2313782131597592,
    0.2276702486969530, 0.2242438242175754, 0.2210647294575037,
    0.2181042919855316, 0.2153382790366965, 0.2127460535533632,
    0.2103099178571525, 0.2080145976765095, 0.2058468324604344,
    0.2037950470905062, 0.2018490865820999, 0.2000000000000000,
    0.1982398631705605, 0.1965616322328226, 0.1949590218937863,
    0.1934264036172708,
};

/* normalization base for string conversion, pow(base, dig_bases[base]) & ~CARRY */
#ifdef LONG64
static BNS big_bases[37] = {
    0x00000001, 0x00000001, 0x00000000, 0xCFD41B91, 0x00000000, 0x48C27395,
    0x81BF1000, 0x75DB9C97, 0x40000000, 0xCFD41B91, 0x3B9ACA00, 0x8C8B6D2B,
    0x19A10000, 0x309F1021, 0x57F6C100, 0x98C29B81, 0x00000000, 0x18754571,
    0x247DBC80, 0x3547667B, 0x4C4B4000, 0x6B5A6E1D, 0x94ACE180, 0xCAF18367,
    0x0B640000, 0x0E8D4A51, 0x1269AE40, 0x17179149, 0x1CB91000, 0x23744899,
    0x2B73A840, 0x34E63B41, 0x40000000, 0x4CFA3CC1, 0x5C13D840, 0x6D91B519,
    0x81BF1000,
};
#else
static BNS big_bases[37] = {
    0x0001, 0x0001, 0x0000, 0xE6A9, 0x0000, 0x3D09, 0xB640, 0x41A7, 0x8000,
    0xE6A9, 0x2710, 0x3931, 0x5100, 0x6F91, 0x9610, 0xC5C1, 0x0000, 0x1331,
    0x16C8, 0x1ACB, 0x1F40, 0x242D, 0x2998, 0x2F87, 0x3600, 0x3D09, 0x44A8,
    0x4CE3, 0x55C0, 0x5F45, 0x6978, 0x745F, 0x8000, 0x8C61, 0x9988, 0xA77B,
    0xb640,
};
#endif

unsigned long
mpi_getsize(mpi *op, int base)
{
    unsigned long value, bits;

    value = op->digs[op->size - 1];

    /* count leading bits */
    if (value) {
	unsigned long count, carry;

	for (count = 0, carry = CARRY >> 1; carry; count++, carry >>= 1)
	    if (value & carry)
		break;

	bits = BNSBITS - count;
    }
    else
	bits = 0;

    return ((bits + (op->size - 1) * BNSBITS) * bit_bases[base] + 1);
}

char *
mpi_getstr(char *str, mpi *op, int base)
{
    long i;			/* counter */
    BNS *digs, *xdigs;		/* copy of op data */
    BNI size;			/* size of op */
    BNI digits;			/* digits per word in given base */
    BNI bigbase;		/* big base of given base */
    BNI strsize;		/* size of resulting string */
    char *cp;			/* pointer in str for conversion */

    /* initialize */
    size = op->size;
    strsize = mpi_getsize(op, base) + op->sign + 1;

    if (str == NULL)
	str = mp_malloc(strsize);

    /* check for zero */
    if (size == 1 && op->digs[0] == 0) {
	str[0] = '0';
	str[1] = '\0';

	return (str);
    }

    digits = dig_bases[base];
    bigbase = big_bases[base];

    cp = str + strsize;
    *--cp = '\0';

    /* make copy of op data and adjust digs */
    xdigs = mp_malloc(size * sizeof(BNS));
    memcpy(xdigs, op->digs, size * sizeof(unsigned short));
    digs = xdigs + size - 1;

    /* convert to string */
    for (;;) {
	long count = -1;
	BNI value;
	BNS quotient, remainder = 0;

	/* if power of two base */
	if ((base & (base - 1)) == 0) {
	    for (i = 0; i < size; i++) {
		quotient = remainder;
		remainder = digs[-i];
		digs[-i] = quotient;
		if (count < 0 && quotient)
		    count = i;
	    }
	}
	else {
	    for (i = 0; i < size; i++) {
		value = digs[-i] + ((BNI)remainder << BNSBITS);
		quotient = (BNS)(value / bigbase);
		remainder = (BNS)(value % bigbase);
		digs[-i] = quotient;
		if (count < 0 && quotient)
		    count = i;
	    }
	}
	quotient = remainder;
	for (i = 0; i < digits; i++) {
	    if (quotient == 0 && count < 0)
		break;
	    remainder = quotient % base;
	    quotient /= base;
	    *--cp = remainder < 10 ? remainder + '0' : remainder - 10 + 'A';
	}
	if (count < 0)
	    break;
	digs -= count;
	size -= count;
    }

    /* adjust sign */
    if (op->sign)
	*--cp = '-';

    /* remove any extra characters */
    if (cp > str)
	strcpy(str, cp);

    mp_free(xdigs);

    return (str);
}
