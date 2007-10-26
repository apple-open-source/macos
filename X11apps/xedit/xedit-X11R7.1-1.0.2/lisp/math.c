/*
 * Copyright (c) 2001 by The XFree86 Project, Inc.
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

/* $XFree86: xc/programs/xedit/lisp/math.c,v 1.23tsi Exp $ */

#include "lisp/math.h"
#include "lisp/private.h"

#ifdef __UNIXOS2__
# define finite(x) isfinite(x)
#endif

/*
 * Prototypes
 */
static LispObj *LispDivide(LispBuiltin*, int, int);

/*
 * Initialization
 */
static LispObj *obj_zero, *obj_one;
LispObj *Ocomplex, *Oequal_;

LispObj *Oshort_float, *Osingle_float, *Odouble_float, *Olong_float;

Atom_id Sdefault_float_format;

/*
 * Implementation
 */
#include "lisp/mathimp.c"

void
LispMathInit(void)
{
    LispObj *object, *result;

    mp_set_malloc(LispMalloc);
    mp_set_calloc(LispCalloc);
    mp_set_realloc(LispRealloc);
    mp_set_free(LispFree);

    number_init();
    obj_zero = FIXNUM(0);
    obj_one = FIXNUM(1);

    Oequal_		= STATIC_ATOM("=");
    Ocomplex		= STATIC_ATOM(Scomplex);
    Oshort_float	= STATIC_ATOM("SHORT-FLOAT");
    LispExportSymbol(Oshort_float);
    Osingle_float	= STATIC_ATOM("SINGLE-FLOAT");
    LispExportSymbol(Osingle_float);
    Odouble_float	= STATIC_ATOM("DOUBLE-FLOAT");
    LispExportSymbol(Odouble_float);
    Olong_float		= STATIC_ATOM("LONG-FLOAT");
    LispExportSymbol(Olong_float);

    object		= STATIC_ATOM("*DEFAULT-FLOAT-FORMAT*");
    LispProclaimSpecial(object, Odouble_float, NIL);
    LispExportSymbol(object);
    Sdefault_float_format = ATOMID(object);

    object		= STATIC_ATOM("PI");
    result = number_pi();
    LispProclaimSpecial(object, result, NIL);
    LispExportSymbol(object);

    object		= STATIC_ATOM("MOST-POSITIVE-FIXNUM");
    LispDefconstant(object, FIXNUM(MOST_POSITIVE_FIXNUM), NIL);
    LispExportSymbol(object);

    object		= STATIC_ATOM("MOST-NEGATIVE-FIXNUM");
    LispDefconstant(object, FIXNUM(MOST_NEGATIVE_FIXNUM), NIL);
    LispExportSymbol(object);
}

LispObj *
Lisp_Mul(LispBuiltin *builtin)
/*
 * &rest numbers
 */
{
    n_number num;
    LispObj *number, *numbers;

    numbers = ARGUMENT(0);

    if (CONSP(numbers)) {
	number = CAR(numbers);

	numbers = CDR(numbers);
	if (!CONSP(numbers)) {
	    CHECK_NUMBER(number);
	    return (number);
	}
    }
    else
	return (FIXNUM(1));

    set_number_object(&num, number);
    do {
	mul_number_object(&num, CAR(numbers));
	numbers = CDR(numbers);
    } while (CONSP(numbers));

    return (make_number_object(&num));
}

LispObj *
Lisp_Plus(LispBuiltin *builtin)
/*
 + &rest numbers
 */
{
    n_number num;
    LispObj *number, *numbers;

    numbers = ARGUMENT(0);

    if (CONSP(numbers)) {
	number = CAR(numbers);

	numbers = CDR(numbers);
	if (!CONSP(numbers)) {
	    CHECK_NUMBER(number);
	    return (number);
	}
    }
    else
	return (FIXNUM(0));

    set_number_object(&num, number);
    do {
	add_number_object(&num, CAR(numbers));
	numbers = CDR(numbers);
    } while (CONSP(numbers));

    return (make_number_object(&num));
}

LispObj *
Lisp_Minus(LispBuiltin *builtin)
/*
 - number &rest more_numbers
 */
{
    n_number num;
    LispObj *number, *more_numbers;

    more_numbers = ARGUMENT(1);
    number = ARGUMENT(0);

    set_number_object(&num, number);
    if (!CONSP(more_numbers)) {
	neg_number(&num);

	return (make_number_object(&num));
    }
    do {
	sub_number_object(&num, CAR(more_numbers));
	more_numbers = CDR(more_numbers);
    } while (CONSP(more_numbers));

    return (make_number_object(&num));
}

LispObj *
Lisp_Div(LispBuiltin *builtin)
/*
 / number &rest more_numbers
 */
{
    n_number num;
    LispObj *number, *more_numbers;

    more_numbers = ARGUMENT(1);
    number = ARGUMENT(0);

    if (CONSP(more_numbers))
	set_number_object(&num, number);
    else {
	num.complex = 0;
	num.real.type = N_FIXNUM;
	num.real.data.fixnum = 1;
	goto div_one_argument;
    }

    for (;;) {
	number = CAR(more_numbers);
	more_numbers = CDR(more_numbers);

div_one_argument:
	div_number_object(&num, number);
	if (!CONSP(more_numbers))
	    break;
    }

    return (make_number_object(&num));
}

LispObj *
Lisp_OnePlus(LispBuiltin *builtin)
/*
 1+ number
 */
{
    n_number num;
    LispObj *number;

    number = ARGUMENT(0);
    num.complex = 0;
    num.real.type = N_FIXNUM;
    num.real.data.fixnum = 1;
    add_number_object(&num, number);

    return (make_number_object(&num));
}

LispObj *
Lisp_OneMinus(LispBuiltin *builtin)
/*
 1- number
 */
{
    n_number num;
    LispObj *number;

    number = ARGUMENT(0);
    num.complex = 0;
    num.real.type = N_FIXNUM;
    num.real.data.fixnum = -1;
    add_number_object(&num, number);

    return (make_number_object(&num));
}

LispObj *
Lisp_Less(LispBuiltin *builtin)
/*
 < number &rest more-numbers
 */
{
    LispObj *compare, *number, *more_numbers;

    more_numbers = ARGUMENT(1);
    compare = ARGUMENT(0);

    if (CONSP(more_numbers)) {
	do {
	    number = CAR(more_numbers);
	    if (cmp_object_object(compare, number, 1) >= 0)
		return (NIL);
	    compare = number;
	    more_numbers = CDR(more_numbers);
	} while (CONSP(more_numbers));
    }
    else {
	CHECK_REAL(compare);
    }

    return (T);
}

LispObj *
Lisp_LessEqual(LispBuiltin *builtin)
/*
 <= number &rest more-numbers
 */
{
    LispObj *compare, *number, *more_numbers;

    more_numbers = ARGUMENT(1);
    compare = ARGUMENT(0);

    if (CONSP(more_numbers)) {
	do {
	    number = CAR(more_numbers);
	    if (cmp_object_object(compare, number, 1) > 0)
		return (NIL);
	    compare = number;
	    more_numbers = CDR(more_numbers);
	} while (CONSP(more_numbers));
    }
    else {
	CHECK_REAL(compare);
    }

    return (T);
}

LispObj *
Lisp_Equal_(LispBuiltin *builtin)
/*
 = number &rest more-numbers
 */
{
    LispObj *compare, *number, *more_numbers;

    more_numbers = ARGUMENT(1);
    compare = ARGUMENT(0);

    if (CONSP(more_numbers)) {
	do {
	    number = CAR(more_numbers);
	    if (cmp_object_object(compare, number, 0) != 0)
		return (NIL);
	    compare = number;
	    more_numbers = CDR(more_numbers);
	} while (CONSP(more_numbers));
    }
    else {
	CHECK_REAL(compare);
    }

    return (T);
}

LispObj *
Lisp_Greater(LispBuiltin *builtin)
/*
 > number &rest more-numbers
 */
{
    LispObj *compare, *number, *more_numbers;

    more_numbers = ARGUMENT(1);
    compare = ARGUMENT(0);

    if (CONSP(more_numbers)) {
	do {
	    number = CAR(more_numbers);
	    if (cmp_object_object(compare, number, 1) <= 0)
		return (NIL);
	    compare = number;
	    more_numbers = CDR(more_numbers);
	} while (CONSP(more_numbers));
    }
    else {
	CHECK_REAL(compare);
    }

    return (T);
}

LispObj *
Lisp_GreaterEqual(LispBuiltin *builtin)
/*
 >= number &rest more-numbers
 */
{
    LispObj *compare, *number, *more_numbers;

    more_numbers = ARGUMENT(1);
    compare = ARGUMENT(0);

    if (CONSP(more_numbers)) {
	do {
	    number = CAR(more_numbers);
	    if (cmp_object_object(compare, number, 1) < 0)
		return (NIL);
	    compare = number;
	    more_numbers = CDR(more_numbers);
	} while (CONSP(more_numbers));
    }
    else {
	CHECK_REAL(compare);
    }

    return (T);
}

LispObj *
Lisp_NotEqual(LispBuiltin *builtin)
/*
 /= number &rest more-numbers
 */
{
    LispObj *object, *compare, *number, *more_numbers;

    more_numbers = ARGUMENT(1);
    number = ARGUMENT(0);

    if (!CONSP(more_numbers)) {
	CHECK_REAL(number);

	return (T);
    }

    /* compare all numbers */
    while (1) {
	compare = number;
	for (object = more_numbers; CONSP(object); object = CDR(object)) {
	    number = CAR(object);

	    if (cmp_object_object(compare, number, 0) == 0)
		return (NIL);
	}
	if (CONSP(more_numbers)) {
	    number = CAR(more_numbers);
	    more_numbers = CDR(more_numbers);
	}
	else
	    break;
    }

    return (T);
}

LispObj *
Lisp_Min(LispBuiltin *builtin)
/*
 min number &rest more-numbers
 */
{
    LispObj *result, *number, *more_numbers;

    more_numbers = ARGUMENT(1);
    result = ARGUMENT(0);

    if (CONSP(more_numbers)) {
	do {
	    number = CAR(more_numbers);
	    if (cmp_object_object(result, number, 1) > 0)
		result = number;
	    more_numbers = CDR(more_numbers);
	} while (CONSP(more_numbers));
    }
    else {
	CHECK_REAL(result);
    }

    return (result);
}

LispObj *
Lisp_Max(LispBuiltin *builtin)
/*
 max number &rest more-numbers
 */
{
    LispObj *result, *number, *more_numbers;

    more_numbers = ARGUMENT(1);
    result = ARGUMENT(0);

    if (CONSP(more_numbers)) {
	do {
	    number = CAR(more_numbers);
	    if (cmp_object_object(result, number, 1) < 0)
		result = number;
	    more_numbers = CDR(more_numbers);
	} while (CONSP(more_numbers));
    }
    else {
	CHECK_REAL(result);
    }

    return (result);
}

LispObj *
Lisp_Abs(LispBuiltin *builtin)
/*
 abs number
 */
{
    LispObj *result, *number;

    result = number = ARGUMENT(0);

    switch (OBJECT_TYPE(number)) {
	case LispFixnum_t:
	case LispInteger_t:
	case LispBignum_t:
	case LispDFloat_t:
	case LispRatio_t:
	case LispBigratio_t:
	    if (cmp_real_object(&zero, number) > 0) {
		n_real real;

		set_real_object(&real, number);
		neg_real(&real);
		result = make_real_object(&real);
	    }
	    break;
	case LispComplex_t: {
	    n_number num;

	    set_number_object(&num, number);
	    abs_number(&num);
	    result = make_number_object(&num);
	}   break;
	default:
	    fatal_builtin_object_error(builtin, number, NOT_A_NUMBER);
	    break;
    }

    return (result);
}

LispObj *
Lisp_Complex(LispBuiltin *builtin)
/*
 complex realpart &optional imagpart
 */
{
    LispObj *realpart, *imagpart;

    imagpart = ARGUMENT(1);
    realpart = ARGUMENT(0);

    CHECK_REAL(realpart);

    if (imagpart == UNSPEC)
	return (realpart);
    else {
	CHECK_REAL(imagpart);
    }
    if (!FLOATP(imagpart) && cmp_real_object(&zero, imagpart) == 0)
	return (realpart);

    return (COMPLEX(realpart, imagpart));
}

LispObj *
Lisp_Complexp(LispBuiltin *builtin)
/*
 complexp object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (COMPLEXP(object) ? T : NIL);
}

LispObj *
Lisp_Conjugate(LispBuiltin *builtin)
/*
 conjugate number
 */
{
    n_number num;
    LispObj *number, *realpart, *imagpart;

    number = ARGUMENT(0);

    CHECK_NUMBER(number);

    if (REALP(number))
	return (number);

    realpart = OCXR(number);
    num.complex = 0;
    num.real.type = N_FIXNUM;
    num.real.data.fixnum = -1;
    mul_number_object(&num, OCXI(number));
    imagpart = make_number_object(&num);

    return (COMPLEX(realpart, imagpart));
}

LispObj *
Lisp_Decf(LispBuiltin *builtin)
/*
 decf place &optional delta
 */
{
    n_number num;
    LispObj *place, *delta, *number;

    delta = ARGUMENT(1);
    place = ARGUMENT(0);

    if (SYMBOLP(place)) {
	number = LispGetVar(place);
	if (number == NULL)
	    LispDestroy("EVAL: the variable %s is unbound", STROBJ(place));
    }
    else
	number = EVAL(place);

    if (delta != UNSPEC) {
	LispObj *operand;

	operand = EVAL(delta);
	set_number_object(&num, number);
	sub_number_object(&num, operand);
	number = make_number_object(&num);
    }
    else {
	num.complex = 0;
	num.real.type = N_FIXNUM;
	num.real.data.fixnum = -1;
	add_number_object(&num, number);
	number = make_number_object(&num);
    }

    if (SYMBOLP(place)) {
	CHECK_CONSTANT(place);
	LispSetVar(place, number);
    }
    else {
	GC_ENTER();

	GC_PROTECT(number);
	(void)APPLY2(Osetf, place, number);
	GC_LEAVE();
    }

    return (number);
}

LispObj *
Lisp_Denominator(LispBuiltin *builtin)
/*
 denominator rational
 */
{
    LispObj *result, *rational;

    rational = ARGUMENT(0);

    switch (OBJECT_TYPE(rational)) {
	case LispFixnum_t:
	case LispInteger_t:
	case LispBignum_t:
	    result = FIXNUM(1);
	    break;
	case LispRatio_t:
	    result = INTEGER(OFRD(rational));
	    break;
	case LispBigratio_t:
	    if (mpi_fiti(OBRD(rational)))
		result = INTEGER(mpi_geti(OBRD(rational)));
	    else {
		mpi *den = XALLOC(mpi);

		mpi_init(den);
		mpi_set(den, OBRD(rational));
		result = BIGNUM(den);
	    }
	    break;
	default:
	    LispDestroy("%s: %s is not a rational number",
			STRFUN(builtin), STROBJ(rational));
	    /*NOTREACHED*/
	    result = NIL;
    }

    return (result);
}

LispObj *
Lisp_Evenp(LispBuiltin *builtin)
/*
 evenp integer
 */
{
    LispObj *result, *integer;

    integer = ARGUMENT(0);

    switch (OBJECT_TYPE(integer)) {
	case LispFixnum_t:
	    result = FIXNUM_VALUE(integer) % 2 ? NIL : T;
	    break;
	case LispInteger_t:
	    result = INT_VALUE(integer) % 2 ? NIL : T;
	    break;
	case LispBignum_t:
	    result = mpi_remi(OBI(integer), 2) ? NIL : T;
	    break;
	default:
	    fatal_builtin_object_error(builtin, integer, NOT_AN_INTEGER);
	    /*NOTREACHED*/
	    result = NIL;
    }

    return (result);
}

/* only one float format */
LispObj *
Lisp_Float(LispBuiltin *builtin)
/*
 float number &optional other
 */
{
    LispObj *number, *other;

    other = ARGUMENT(1);
    number = ARGUMENT(0);

    if (other != UNSPEC) {
	CHECK_DFLOAT(other);
    }

    return (LispFloatCoerce(builtin, number));
}

LispObj *
LispFloatCoerce(LispBuiltin *builtin, LispObj *number)
{
    double value;

    switch (OBJECT_TYPE(number)) {
	case LispFixnum_t:
	    value = FIXNUM_VALUE(number);
	    break;
	case LispInteger_t:
	    value = INT_VALUE(number);
	    break;
	case LispBignum_t:
	    value = mpi_getd(OBI(number));
	    break;
	case LispDFloat_t:
	    return (number);
	case LispRatio_t:
	    value = (double)OFRN(number) / (double)OFRD(number);
	    break;
	case LispBigratio_t:
	    value = mpr_getd(OBR(number));
	    break;
	default:
	    value = 0.0;
	    fatal_builtin_object_error(builtin, number, NOT_A_REAL_NUMBER);
	    break;
    }

    if (!finite(value))
	fatal_error(FLOATING_POINT_OVERFLOW);

    return (DFLOAT(value));
}

LispObj *
Lisp_Floatp(LispBuiltin *builtin)
/*
 floatp object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (FLOATP(object) ? T : NIL);
}

LispObj *
Lisp_Gcd(LispBuiltin *builtin)
/*
 gcd &rest integers
 */
{
    n_real real;
    LispObj *integers, *integer, *operand;

    integers = ARGUMENT(0);

    if (!CONSP(integers))
	return (FIXNUM(0));

    integer = CAR(integers);

    CHECK_INTEGER(integer);
    set_real_object(&real, integer);
    integers = CDR(integers);

    for (; CONSP(integers); integers = CDR(integers)) {
	operand = CAR(integers);
	gcd_real_object(&real, operand);
    }
    abs_real(&real);

    return (make_real_object(&real));
}

LispObj *
Lisp_Imagpart(LispBuiltin *builtin)
/*
 imagpart number
 */
{
    LispObj *number;

    number = ARGUMENT(0);

    if (COMPLEXP(number))
	return (OCXI(number));
    else {
	CHECK_REAL(number);
    }

    return (FIXNUM(0));
}

LispObj *
Lisp_Incf(LispBuiltin *builtin)
/*
 incf place &optional delta
 */
{
    n_number num;
    LispObj *place, *delta, *number;

    delta = ARGUMENT(1);
    place = ARGUMENT(0);

    if (SYMBOLP(place)) {
	number = LispGetVar(place);
	if (number == NULL)
	    LispDestroy("EVAL: the variable %s is unbound", STROBJ(place));
    }
    else
	number = EVAL(place);

    if (delta != UNSPEC) {
	LispObj *operand;

	operand = EVAL(delta);
	set_number_object(&num, number);
	add_number_object(&num, operand);
	number = make_number_object(&num);
    }
    else {
	num.complex = 0;
	num.real.type = N_FIXNUM;
	num.real.data.fixnum = 1;
	add_number_object(&num, number);
	number = make_number_object(&num);
    }

    if (SYMBOLP(place)) {
	CHECK_CONSTANT(place);
	LispSetVar(place, number);
    }
    else {
	GC_ENTER();

	GC_PROTECT(number);
	(void)APPLY2(Osetf, place, number);
	GC_LEAVE();
    }

    return (number);
}

LispObj *
Lisp_Integerp(LispBuiltin *builtin)
/*
 integerp object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (INTEGERP(object) ? T : NIL);
}

LispObj *
Lisp_Isqrt(LispBuiltin *builtin)
/*
 isqrt natural
 */
{
    LispObj *natural, *result;

    natural = ARGUMENT(0);

    if (cmp_object_object(natural, obj_zero, 1) < 0)
	goto not_a_natural_number;

    switch (OBJECT_TYPE(natural)) {
	case LispFixnum_t:
	    result = FIXNUM((long)floor(sqrt(FIXNUM_VALUE(natural))));
	    break;
	case LispInteger_t:
	    result = INTEGER((long)floor(sqrt(INT_VALUE(natural))));
	    break;
	case LispBignum_t: {
	    mpi *bigi;

	    bigi = XALLOC(mpi);
	    mpi_init(bigi);
	    mpi_sqrt(bigi, OBI(natural));
	    if (mpi_fiti(bigi)) {
		result = INTEGER(mpi_geti(bigi));
		mpi_clear(bigi);
		XFREE(bigi);
	    }
	    else
		result = BIGNUM(bigi);
	}   break;
	default:
	    goto not_a_natural_number;
    }

    return (result);

not_a_natural_number:
    LispDestroy("%s: %s is not a natural number",
		STRFUN(builtin), STROBJ(natural));
    /*NOTREACHED*/
    return (NIL);
}

LispObj *
Lisp_Lcm(LispBuiltin *builtin)
/*
 lcm &rest integers
 */
{
    n_real real, gcd;
    LispObj *integers, *operand;

    integers = ARGUMENT(0);

    if (!CONSP(integers))
	return (FIXNUM(1));

    operand = CAR(integers);

    CHECK_INTEGER(operand);
    set_real_object(&real, operand);
    integers = CDR(integers);

    gcd.type = N_FIXNUM;
    gcd.data.fixnum = 0;

    for (; CONSP(integers); integers = CDR(integers)) {
	operand = CAR(integers);

	if (real.type == N_FIXNUM && real.data.fixnum == 0)
	    break;

	/* calculate gcd before changing integer */
	clear_real(&gcd);
	set_real_real(&gcd, &real);
	gcd_real_object(&gcd, operand);

	/* calculate lcm */
	mul_real_object(&real, operand);
	div_real_real(&real, &gcd);
    }
    clear_real(&gcd);
    abs_real(&real);

    return (make_real_object(&real));
}

LispObj *
Lisp_Logand(LispBuiltin *builtin)
/*
 logand &rest integers
 */
{
    n_real real;

    LispObj *integers;

    integers = ARGUMENT(0);

    real.type = N_FIXNUM;
    real.data.fixnum = -1;

    for (; CONSP(integers); integers = CDR(integers))
	and_real_object(&real, CAR(integers));

    return (make_real_object(&real));
}

LispObj *
Lisp_Logeqv(LispBuiltin *builtin)
/*
 logeqv &rest integers
 */
{
    n_real real;

    LispObj *integers;

    integers = ARGUMENT(0);

    real.type = N_FIXNUM;
    real.data.fixnum = -1;

    for (; CONSP(integers); integers = CDR(integers))
	eqv_real_object(&real, CAR(integers));

    return (make_real_object(&real));
}

LispObj *
Lisp_Logior(LispBuiltin *builtin)
/*
 logior &rest integers
 */
{
    n_real real;

    LispObj *integers;

    integers = ARGUMENT(0);

    real.type = N_FIXNUM;
    real.data.fixnum = 0;

    for (; CONSP(integers); integers = CDR(integers))
	ior_real_object(&real, CAR(integers));

    return (make_real_object(&real));
}

LispObj *
Lisp_Lognot(LispBuiltin *builtin)
/*
 lognot integer
 */
{
    n_real real;

    LispObj *integer;

    integer = ARGUMENT(0);

    CHECK_INTEGER(integer);

    set_real_object(&real, integer);
    not_real(&real);

    return (make_real_object(&real));
}

LispObj *
Lisp_Logxor(LispBuiltin *builtin)
/*
 logxor &rest integers
 */
{
    n_real real;

    LispObj *integers;

    integers = ARGUMENT(0);

    real.type = N_FIXNUM;
    real.data.fixnum = 0;

    for (; CONSP(integers); integers = CDR(integers))
	xor_real_object(&real, CAR(integers));

    return (make_real_object(&real));
}

LispObj *
Lisp_Minusp(LispBuiltin *builtin)
/*
 minusp number
 */
{
    LispObj *number;

    number = ARGUMENT(0);

    CHECK_REAL(number);

    return (cmp_real_object(&zero, number) > 0 ? T : NIL);
}

LispObj *
Lisp_Mod(LispBuiltin *builtin)
/*
 mod number divisor
 */
{
    LispObj *result;

    LispObj *number, *divisor;

    divisor = ARGUMENT(1);
    number = ARGUMENT(0);

    if (INTEGERP(number) && INTEGERP(divisor)) {
	n_real real;

	set_real_object(&real, number);
	mod_real_object(&real, divisor);
	result = make_real_object(&real);
    }
    else {
	n_number num;

	set_number_object(&num, number);
	divide_number_object(&num, divisor, NDIVIDE_FLOOR, 0);
	result = make_real_object(&(num.imag));
	clear_real(&(num.real));
    }

    return (result);
}

LispObj *
Lisp_Numberp(LispBuiltin *builtin)
/*
 numberp object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (NUMBERP(object) ? T : NIL);
}

LispObj *
Lisp_Numerator(LispBuiltin *builtin)
/*
 numerator rational
 */
{
    LispObj *result, *rational;

    rational = ARGUMENT(0);

    switch (OBJECT_TYPE(rational)) {
	case LispFixnum_t:
	case LispInteger_t:
	case LispBignum_t:
	    result = rational;
	    break;
	case LispRatio_t:
	    result = INTEGER(OFRN(rational));
	    break;
	case LispBigratio_t:
	    if (mpi_fiti(OBRN(rational)))
		result = INTEGER(mpi_geti(OBRN(rational)));
	    else {
		mpi *num = XALLOC(mpi);

		mpi_init(num);
		mpi_set(num, OBRN(rational));
		result = BIGNUM(num);
	    }
	    break;
	default:
	    LispDestroy("%s: %s is not a rational number",
			STRFUN(builtin), STROBJ(rational));
	    /*NOTREACHED*/
	    result = NIL;
    }

    return (result);
}

LispObj *
Lisp_Oddp(LispBuiltin *builtin)
/*
 oddp integer
 */
{
    LispObj *result, *integer;

    integer = ARGUMENT(0);

    switch (OBJECT_TYPE(integer)) {
	case LispFixnum_t:
	    result = FIXNUM_VALUE(integer) % 2 ? T : NIL;
	    break;
	case LispInteger_t:
	    result = INT_VALUE(integer) % 2 ? T : NIL;
	    break;
	case LispBignum_t:
	    result = mpi_remi(OBI(integer), 2) ? T : NIL;
	    break;
	default:
	    fatal_builtin_object_error(builtin, integer, NOT_AN_INTEGER);
	    /*NOTREACHED*/
	    result = NIL;
    }

    return (result);
}

LispObj *
Lisp_Plusp(LispBuiltin *builtin)
/*
 plusp number
 */
{
    LispObj *number;

    number = ARGUMENT(0);

    CHECK_REAL(number);

    return (cmp_real_object(&zero, number) < 0 ? T : NIL);
}

LispObj *
Lisp_Rational(LispBuiltin *builtin)
/*
 rational number
 */
{
    LispObj *number;

    number = ARGUMENT(0);

    if (DFLOATP(number)) {
	double numerator = ODF(number);

	if ((long)numerator == numerator)
	    number = INTEGER(numerator);
	else {
	    n_real real;
	    mpr *bigr = XALLOC(mpr);

	    mpr_init(bigr);
	    mpr_setd(bigr, numerator);
	    real.type = N_BIGRATIO;
	    real.data.bigratio = bigr;
	    rbr_canonicalize(&real);
	    number = make_real_object(&real);
	}
    }
    else {
	CHECK_REAL(number);
    }

    return (number);
}

LispObj *
Lisp_Rationalp(LispBuiltin *builtin)
/*
 rationalp object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (RATIONALP(object) ? T : NIL);
}

LispObj *
Lisp_Realpart(LispBuiltin *builtin)
/*
 realpart number
 */
{
    LispObj *number;

    number = ARGUMENT(0);

    if (COMPLEXP(number))
	return (OCXR(number));
    else {
	CHECK_REAL(number);
    }

    return (number);
}

LispObj *
Lisp_Rem(LispBuiltin *builtin)
/*
 rem number divisor
 */
{
    LispObj *result;

    LispObj *number, *divisor;

    divisor = ARGUMENT(1);
    number = ARGUMENT(0);

    if (INTEGERP(number) && INTEGERP(divisor)) {
	n_real real;

	set_real_object(&real, number);
	rem_real_object(&real, divisor);
	result = make_real_object(&real);
    }
    else {
	n_number num;

	set_number_object(&num, number);
	divide_number_object(&num, divisor, NDIVIDE_TRUNC, 0);
	result = make_real_object(&(num.imag));
	clear_real(&(num.real));
    }

    return (result);
}

LispObj *
Lisp_Sqrt(LispBuiltin *builtin)
/*
 sqrt number
 */
{
    n_number num;
    LispObj *number;

    number = ARGUMENT(0);

    set_number_object(&num, number);
    sqrt_number(&num);

    return (make_number_object(&num));
}

LispObj *
Lisp_Zerop(LispBuiltin *builtin)
/*
 zerop number
 */
{
    LispObj *result, *number;

    number = ARGUMENT(0);

    switch (OBJECT_TYPE(number)) {
	case LispFixnum_t:
	case LispInteger_t:
	case LispBignum_t:
	case LispDFloat_t:
	case LispRatio_t:
	case LispBigratio_t:
	    result = cmp_real_object(&zero, number) == 0 ? T : NIL;
	    break;
	case LispComplex_t:
	    result = cmp_real_object(&zero, OCXR(number)) == 0 &&
		     cmp_real_object(&zero, OCXI(number)) == 0 ? T : NIL;
	    break;
	default:
	    fatal_builtin_object_error(builtin, number, NOT_A_NUMBER);
	    /*NOTREACHED*/
	    result = NIL;
    }

    return (result);
}

static LispObj *
LispDivide(LispBuiltin *builtin, int fun, int flo)
{
    n_number num;
    LispObj *number, *divisor;

    divisor = ARGUMENT(1);
    number = ARGUMENT(0);

    RETURN_COUNT = 1;

    if (cmp_real_object(&zero, number) == 0) {
	if (divisor != NIL) {
	    CHECK_REAL(divisor);
	}

	return (RETURN(0) = obj_zero);
    }

    if (divisor == UNSPEC)
	divisor = obj_one;

    set_number_object(&num, number);
    if (num.complex)
	fatal_builtin_object_error(builtin, divisor, NOT_A_REAL_NUMBER);

    divide_number_object(&num, divisor, fun, flo);
    RETURN(0) = make_real_object(&(num.imag));

    return (make_real_object(&(num.real)));
}

LispObj *
Lisp_Ceiling(LispBuiltin *builtin)
/*
 ceiling number &optional divisor
 */
{
    return (LispDivide(builtin, NDIVIDE_CEIL, 0));
}

LispObj *
Lisp_Fceiling(LispBuiltin *builtin)
/*
 fceiling number &optional divisor
 */
{
    return (LispDivide(builtin, NDIVIDE_CEIL, 1));
}

LispObj *
Lisp_Floor(LispBuiltin *builtin)
/*
 floor number &optional divisor
 */
{
    return (LispDivide(builtin, NDIVIDE_FLOOR, 0));
}

LispObj *
Lisp_Ffloor(LispBuiltin *builtin)
/*
 ffloor number &optional divisor
 */
{
    return (LispDivide(builtin, NDIVIDE_FLOOR, 1));
}

LispObj *
Lisp_Round(LispBuiltin *builtin)
/*
 round number &optional divisor
 */
{
    return (LispDivide(builtin, NDIVIDE_ROUND, 0));
}

LispObj *
Lisp_Fround(LispBuiltin *builtin)
/*
 fround number &optional divisor
 */
{
    return (LispDivide(builtin, NDIVIDE_ROUND, 1));
}

LispObj *
Lisp_Truncate(LispBuiltin *builtin)
/*
 truncate number &optional divisor
 */
{
    return (LispDivide(builtin, NDIVIDE_TRUNC, 0));
}

LispObj *
Lisp_Ftruncate(LispBuiltin *builtin)
/*
 ftruncate number &optional divisor
 */
{
    return (LispDivide(builtin, NDIVIDE_TRUNC, 1));
}
