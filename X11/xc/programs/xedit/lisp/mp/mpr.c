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

/* $XFree86: xc/programs/xedit/lisp/mp/mpr.c,v 1.2 2002/11/08 08:01:00 paulo Exp $ */

#include "mp.h"

/*
 * TODO:
 *  Implement a fast gcd and divexact for integers, so that this code
 * could be changed to do intermediary calculations faster using smaller
 * numbers.
 */

/*
 * Prototypes
 */
	/* do the hard work of mpr_add and mpr_sub */
static void mpr_addsub(mpr *rop, mpr *op1, mpr *op2, int sub);

	/* do the hard work of mpr_cmp and mpr_cmpabs */
static int mpr_docmp(mpr *op1, mpr *op2, int sign);

/*
 * Implementation
 */
void
mpr_init(mpr *op)
{
    op->num.digs = mp_malloc(sizeof(BNS));
    op->num.sign = 0;
    op->num.size = op->num.alloc = 1;
    op->num.digs[0] = 0;

    op->den.digs = mp_malloc(sizeof(BNS));
    op->den.sign = 0;
    op->den.size = op->den.alloc = 1;
    op->den.digs[0] = 1;
}

void
mpr_clear(mpr *op)
{
    op->num.sign = 0;
    op->num.size = op->num.alloc = 0;
    mp_free(op->num.digs);

    op->den.sign = 0;
    op->den.size = op->den.alloc = 0;
    mp_free(op->den.digs);
}

void
mpr_set(mpr *rop, mpr *op)
{
    if (rop != op) {
	mpi_set(mpr_num(rop), mpr_num(op));
	mpi_set(mpr_den(rop), mpr_den(op));
    }
}

void
mpr_seti(mpr *rop, long num, long den)
{
    mpi_seti(mpr_num(rop), num);
    mpi_seti(mpr_den(rop), den);
}

void
mpr_setd(mpr *rop, double d)
{
    double val, num;
    int e, sign;

    /* initialize */
    if (d < 0) {
	sign = 1;
	val = -d;
    }
    else {
	sign = 0;
	val = d;
    }

    val = frexp(val, &e);
    while (modf(val, &num) != 0.0 && val <= DBL_MAX / 2.0) {
	--e;
	val *= 2.0;
    }
    if (e >= 0) {
	mpi_setd(mpr_num(rop), d);
	mpi_seti(mpr_den(rop), 1);
    }
    else {
	mpi_setd(mpr_num(rop), sign ? -num : num);
	mpi_setd(mpr_den(rop), ldexp(1.0, -e));
    }
}

void
mpr_setstr(mpr *rop, char *str, int base)
{
    char *slash = strchr(str, '/');

    mpi_setstr(mpr_num(rop), str, base);
    if (slash != NULL)
	mpi_setstr(mpr_den(rop), slash + 1, base);
    else
	mpi_seti(mpr_den(rop), 1);
}

void
mpr_canonicalize(mpr *op)
{
    mpi gcd;

    memset(&gcd, '\0', sizeof(mpi));

    mpi_gcd(&gcd, mpr_num(op), mpr_den(op));
    if (mpi_cmpabsi(&gcd, 1)) {
	mpi_div(mpr_num(op), mpr_num(op), &gcd);
	mpi_div(mpr_den(op), mpr_den(op), &gcd);
    }

    if (op->den.sign) {
	op->num.sign = !op->num.sign;
	op->den.sign = 0;
    }

    mpi_clear(&gcd);
}

void
mpr_add(mpr *rop, mpr *op1, mpr *op2)
{
    mpr_addsub(rop, op1, op2, 0);
}

void
mpr_addi(mpr *rop, mpr *op1, long op2)
{
    mpi prod;

    memset(&prod, '\0', sizeof(mpi));

    mpi_muli(&prod, mpr_den(op1), op2);
    mpi_add(mpr_num(rop), mpr_num(op1), &prod);
    mpi_clear(&prod);
}

void
mpr_sub(mpr *rop, mpr *op1, mpr *op2)
{
    mpr_addsub(rop, op1, op2, 1);
}

void
mpr_subi(mpr *rop, mpr *op1, long op2)
{
    mpi prod;

    memset(&prod, '\0', sizeof(mpi));

    mpi_muli(&prod, mpr_den(op1), op2);
    mpi_sub(mpr_num(rop), mpr_num(op1), &prod);
    mpi_clear(&prod);
}

static void
mpr_addsub(mpr *rop, mpr *op1, mpr *op2, int sub)
{
    mpi prod1, prod2;

    memset(&prod1, '\0', sizeof(mpi));
    memset(&prod2, '\0', sizeof(mpi));

    mpi_mul(&prod1, mpr_num(op1), mpr_den(op2));
    mpi_mul(&prod2, mpr_num(op2), mpr_den(op1));

    if (sub)
	mpi_sub(mpr_num(rop), &prod1, &prod2);
    else
	mpi_add(mpr_num(rop), &prod1, &prod2);

    mpi_clear(&prod1);
    mpi_clear(&prod2);

    mpi_mul(mpr_den(rop), mpr_den(op1), mpr_den(op2));
}

void
mpr_mul(mpr *rop, mpr *op1, mpr *op2)
{
    /* check if temporary storage is required */
    if (op1 == op2 && rop == op1) {
	mpi prod;

	memset(&prod, '\0', sizeof(mpi));

	mpi_mul(&prod, mpr_num(op1), mpr_num(op2));
	mpi_mul(mpr_den(rop), mpr_den(op1), mpr_den(op2));
	mpi_set(mpr_num(rop), &prod);

	mpi_clear(&prod);
    }
    else {
	mpi_mul(mpr_num(rop), mpr_num(op1), mpr_num(op2));
	mpi_mul(mpr_den(rop), mpr_den(op1), mpr_den(op2));
    }
}

void
mpr_muli(mpr *rop, mpr *op1, long op2)
{
    mpi_muli(mpr_num(rop), mpr_num(op1), op2);
}

void
mpr_div(mpr *rop, mpr *op1, mpr *op2)
{
    /* check if temporary storage is required */
    if (op1 == op2 && rop == op1) {
	mpi prod;

	memset(&prod, '\0', sizeof(mpi));

	mpi_mul(&prod, mpr_num(op1), mpr_den(op2));
	mpi_mul(mpr_den(rop), mpr_num(op2), mpr_den(op1));
	mpi_set(mpr_num(rop), &prod);

	mpi_clear(&prod);
    }
    else {
	mpi_mul(mpr_num(rop), mpr_num(op1), mpr_den(op2));
	mpi_mul(mpr_den(rop), mpr_num(op2), mpr_den(op1));
    }
}

void
mpr_divi(mpr *rop, mpr *op1, long op2)
{
    mpi_muli(mpr_den(rop), mpr_den(op1), op2);
}

void
mpr_inv(mpr *rop, mpr *op)
{
    if (rop == op)
	mpi_swap(mpr_num(op), mpr_den(op));
    else {
	mpi_set(mpr_num(rop), mpr_den(op));
	mpi_set(mpr_den(rop), mpr_num(op));
    }
}

void
mpr_neg(mpr *rop, mpr *op)
{
    mpi_neg(mpr_num(rop), mpr_num(op));
    mpi_set(mpr_den(rop), mpr_den(op));
}

void
mpr_abs(mpr *rop, mpr *op)
{
    if (mpr_num(op)->sign)
	mpi_neg(mpr_num(rop), mpr_num(op));
    else
	mpi_set(mpr_num(rop), mpr_num(op));

    /* op may not be canonicalized */
    if (mpr_den(op)->sign)
	mpi_neg(mpr_den(rop), mpr_den(op));
    else
	mpi_set(mpr_den(rop), mpr_den(op));
}

int
mpr_cmp(mpr *op1, mpr *op2)
{
    return (mpr_docmp(op1, op2, 1));
}

int
mpr_cmpi(mpr *op1, long op2)
{
    int cmp;
    mpr rat;

    mpr_init(&rat);
    mpi_seti(mpr_num(&rat), op2);
    cmp = mpr_docmp(op1, &rat, 1);
    mpr_clear(&rat);

    return (cmp);
}

int
mpr_cmpabs(mpr *op1, mpr *op2)
{
    return (mpr_docmp(op1, op2, 0));
}

int
mpr_cmpabsi(mpr *op1, long op2)
{
    int cmp;
    mpr rat;

    mpr_init(&rat);
    mpi_seti(mpr_num(&rat), op2);
    cmp = mpr_docmp(op1, &rat, 0);
    mpr_clear(&rat);

    return (cmp);
}

static int
mpr_docmp(mpr *op1, mpr *op2, int sign)
{
    int cmp, neg;
    mpi prod1, prod2;

    neg = 0;
    if (sign) {
	/* if op1 is negative */
	if (mpr_num(op1)->sign ^ mpr_den(op1)->sign) {
	    /* if op2 is positive */
	    if (!(mpr_num(op2)->sign ^ mpr_den(op2)->sign))
		return (-1);
	    else
		neg = 1;
	}
	/* if op2 is negative */
	else if (mpr_num(op2)->sign ^ mpr_den(op2)->sign)
	    return (1);
	/* else same sign */
    }

    /* if denominators are equal, compare numerators */
    if (mpi_cmpabs(mpr_den(op1), mpr_den(op2)) == 0) {
	cmp = mpi_cmpabs(mpr_num(op1), mpr_num(op2));
	if (cmp == 0)
	    return (0);
	if (sign && neg)
	    return (cmp < 0 ? 1 : -1);
	return (cmp);
    }

    memset(&prod1, '\0', sizeof(mpi));
    memset(&prod2, '\0', sizeof(mpi));

    /* "divide" op1 by op2
     * if result is smaller than 1, op1 is smaller than op2 */
    mpi_mul(&prod1, mpr_num(op1), mpr_den(op2));
    mpi_mul(&prod2, mpr_num(op2), mpr_den(op1));

    cmp = mpi_cmpabs(&prod1, &prod2);

    mpi_clear(&prod1);
    mpi_clear(&prod2);

    if (sign && neg)
	return (cmp < 0 ? 1 : -1);
    return (cmp);
}

void
mpr_swap(mpr *op1, mpr *op2)
{
    if (op1 != op2) {
	mpr swap;

	memcpy(&swap, op1, sizeof(mpr));
	memcpy(op1, op2, sizeof(mpr));
	memcpy(op2, &swap, sizeof(mpr));
    }
}

int
mpr_fiti(mpr *op)
{
    return (mpi_fiti(mpr_num(op)) && mpi_fiti(mpr_den(op)));
}

double
mpr_getd(mpr *op)
{
    return (mpi_getd(mpr_num(op)) / mpi_getd(mpr_den(op)));
}

char *
mpr_getstr(char *str, mpr *op, int base)
{
    int len;

    if (str == NULL) {
	len = mpi_getsize(mpr_num(op), base) + mpr_num(op)->sign + 1 +
	      mpi_getsize(mpr_den(op), base) + mpr_den(op)->sign + 1;

	str = mp_malloc(len);
    }

    (void)mpi_getstr(str, mpr_num(op), base);
    len = strlen(str);
    str[len] = '/';
    (void)mpi_getstr(str + len + 1, mpr_den(op), base);

    return (str);
}
