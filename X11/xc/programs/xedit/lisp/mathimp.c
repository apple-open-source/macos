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

/* $XFree86: xc/programs/xedit/lisp/mathimp.c,v 1.14 2003/01/30 02:46:25 paulo Exp $ */


/*
 * Defines
 */
#ifdef __GNUC__
#define CONST			__attribute__ ((__const__))
#else
#define CONST			/**/
#endif

/* mask for checking overflow on long operations */
#ifdef LONG64
#define FI_MASK			0x4000000000000000L
#define LONGSBITS		63
#else
#define FI_MASK			0x40000000L
#define LONGSBITS		31
#endif

#define N_FIXNUM		1
#define N_BIGNUM		2
#define N_FLONUM		3
#define N_FIXRATIO		4
#define N_BIGRATIO		5

#define NOP_ADD			1
#define NOP_SUB			2
#define NOP_MUL			3
#define NOP_DIV			4

#define NDIVIDE_CEIL		1
#define NDIVIDE_FLOOR		2
#define NDIVIDE_ROUND		3
#define NDIVIDE_TRUNC		4

/* real part from number */
#define NREAL(num)		&((num)->real)
#define NRTYPE(num)		(num)->real.type
#define NRFI(num)		(num)->real.data.fixnum
#define NRBI(num)		(num)->real.data.bignum
#define NRFF(num)		(num)->real.data.flonum
#define NRFRN(Num)		(Num)->real.data.fixratio.num
#define NRFRD(num)		(num)->real.data.fixratio.den
#define NRBR(num)		(num)->real.data.bigratio
#define NRBRN(num)		mpr_num(NRBR(num))
#define NRBRD(num)		mpr_den(NRBR(num))

#define NRCLEAR_BI(num)		mpi_clear(NRBI(num)); XFREE(NRBI(num))
#define NRCLEAR_BR(num)		mpr_clear(NRBR(num)); XFREE(NRBR(num))

/* imag part from number */
#define NIMAG(num)		&((num)->imag)
#define NITYPE(num)		(num)->imag.type
#define NIFI(num)		(num)->imag.data.fixnum
#define NIBI(num)		(num)->imag.data.bignum
#define NIFF(num)		(num)->imag.data.flonum
#define NIFRN(Num)		(Num)->imag.data.fixratio.num
#define NIFRD(num)		(num)->imag.data.fixratio.den
#define NIBR(num)		(num)->imag.data.bigratio
#define NIBRN(obj)		mpr_num(NIBR(obj))
#define NIBRD(obj)		mpr_den(NIBR(obj))

/* real number fields */
#define RTYPE(real)		(real)->type
#define RFI(real)		(real)->data.fixnum
#define RBI(real)		(real)->data.bignum
#define RFF(real)		(real)->data.flonum
#define RFRN(real)		(real)->data.fixratio.num
#define RFRD(real)		(real)->data.fixratio.den
#define RBR(real)		(real)->data.bigratio
#define RBRN(real)		mpr_num(RBR(real))
#define RBRD(real)		mpr_den(RBR(real))

#define RINTEGERP(real)				\
    (RTYPE(real) == N_FIXNUM || RTYPE(real) == N_BIGNUM)

#define RCLEAR_BI(real)		mpi_clear(RBI(real)); XFREE(RBI(real))
#define RCLEAR_BR(real)		mpr_clear(RBR(real)); XFREE(RBR(real))

/* numeric value from lisp object */
#define OFI(object)		FIXNUM_VALUE(object)
#define OII(object)		INT_VALUE(object)
#define OBI(object)		(object)->data.mp.integer
#define ODF(object)		DFLOAT_VALUE(object)
#define OFRN(object)		(object)->data.ratio.numerator
#define OFRD(object)		(object)->data.ratio.denominator
#define OBR(object)		(object)->data.mp.ratio
#define OBRN(object)		mpr_num(OBR(object))
#define OBRD(object)		mpr_den(OBR(object))
#define OCXR(object)		(object)->data.complex.real
#define OCXI(object)		(object)->data.complex.imag

#define XALLOC(type)		LispMalloc(sizeof(type))
#define XFREE(ptr)		LispFree(ptr)


/*
 * Types
 */
typedef struct _n_real {
    char type;
    union {
	long fixnum;
	mpi *bignum;
	double flonum;
	struct {
	    long num;
	    long den;
	} fixratio;
	mpr *bigratio;
    } data;
} n_real;

typedef struct _n_number {
    char complex;
    n_real real;
    n_real imag;
} n_number;


/*
 * Prototypes
 */
static void number_init(void);
static LispObj *number_pi(void);

static void set_real_real(n_real*, n_real*);
static void set_real_object(n_real*, LispObj*);
static void set_number_object(n_number*, LispObj*);
static void clear_real(n_real*);
static void clear_number(n_number*);

static LispObj *make_real_object(n_real*);
static LispObj *make_number_object(n_number*);

static void fatal_error(int);
static void fatal_object_error(LispObj*, int);
static void fatal_builtin_object_error(LispBuiltin*, LispObj*, int);

static double bi_getd(mpi*);
static double br_getd(mpr*);

/* add */
static void add_real_object(n_real*, LispObj*);
static void add_number_object(n_number*, LispObj*);

/* sub */
static void sub_real_object(n_real*, LispObj*);
static void sub_number_object(n_number*, LispObj*);

/* mul */
static void mul_real_object(n_real*, LispObj*);
static void mul_number_object(n_number*, LispObj*);

/* div */
static void div_real_object(n_real*, LispObj*);
static void div_number_object(n_number*, LispObj*);

/* compare */
static int cmp_real_real(n_real*, n_real*);
static int cmp_real_object(n_real*, LispObj*);
#if 0	/* not used */
static int cmp_number_object(n_number*, LispObj*);
#endif
static int cmp_object_object(LispObj*, LispObj*, int);

/* fixnum */
static INLINE int fi_fi_add_overflow(long, long) CONST;
static INLINE int fi_fi_sub_overflow(long, long) CONST;
static INLINE int fi_fi_mul_overflow(long, long) CONST;

/* bignum */
static void rbi_canonicalize(n_real*);

/* ratio */
static void rfr_canonicalize(n_real*);
static void rbr_canonicalize(n_real*);

/* complex */
static void ncx_canonicalize(n_number*);

/* abs */
static void abs_real(n_real*);
static void abs_number(n_number*);
static void nabs_cx(n_number*);
static INLINE void rabs_fi(n_real*);
static INLINE void rabs_bi(n_real*);
static INLINE void rabs_ff(n_real*);
static INLINE void rabs_fr(n_real*);
static INLINE void rabs_br(n_real*);

/* neg */
static void neg_real(n_real*);
static void neg_number(n_number*);
static void rneg_fi(n_real*);
static INLINE void rneg_bi(n_real*);
static INLINE void rneg_ff(n_real*);
static INLINE void rneg_fr(n_real*);
static INLINE void rneg_br(n_real*);

/* sqrt */
static void sqrt_real(n_real*);
static void sqrt_number(n_number*);
static void rsqrt_xi(n_real*);
static void rsqrt_xr(n_real*);
static void rsqrt_ff(n_real*);
static void nsqrt_cx(n_number*);
static void nsqrt_xi(n_number*);
static void nsqrt_ff(n_number*);
static void nsqrt_xr(n_number*);

/* mod */
static void mod_real_real(n_real*, n_real*);
static void mod_real_object(n_real*, LispObj*);
static void rmod_fi_fi(n_real*, long);
static void rmod_fi_bi(n_real*, mpi*);
static void rmod_bi_fi(n_real*, long);
static void rmod_bi_bi(n_real*, mpi*);

/* rem */
static void rem_real_object(n_real*, LispObj*);
static void rrem_fi_fi(n_real*, long);
static void rrem_fi_bi(n_real*, mpi*);
static void rrem_bi_fi(n_real*, long);
static void rrem_bi_bi(n_real*, mpi*);

/* gcd */
static void gcd_real_object(n_real*, LispObj*);

/* and */
static void and_real_object(n_real*, LispObj*);

/* eqv */
static void eqv_real_object(n_real*, LispObj*);

/* ior */
static void ior_real_object(n_real*, LispObj*);

/* not */
static void not_real(n_real*);

/* xor */
static void xor_real_object(n_real*, LispObj*);

/* divide */
static void divide_number_object(n_number*, LispObj*, int, int);
static void ndivide_xi_xi(n_number*, LispObj*, int, int);
static void ndivide_flonum(n_number*, double, double, int, int);
static void ndivide_xi_xr(n_number*, LispObj*, int, int);
static void ndivide_xr_xi(n_number*, LispObj*, int, int);
static void ndivide_xr_xr(n_number*, LispObj*, int, int);

/* real complex */
static void nadd_re_cx(n_number*, LispObj*);
static void nsub_re_cx(n_number*, LispObj*);
static void nmul_re_cx(n_number*, LispObj*);
static void ndiv_re_cx(n_number*, LispObj*);

/* complex real */
static void nadd_cx_re(n_number*, LispObj*);
static void nsub_cx_re(n_number*, LispObj*);
static void nmul_cx_re(n_number*, LispObj*);
static void ndiv_cx_re(n_number*, LispObj*);

/* complex complex */
static void nadd_cx_cx(n_number*, LispObj*);
static void nsub_cx_cx(n_number*, LispObj*);
static void nmul_cx_cx(n_number*, LispObj*);
static void ndiv_cx_cx(n_number*, LispObj*);
static int cmp_cx_cx(LispObj*, LispObj*);

/* flonum flonum */
static void radd_flonum(n_real*, double, double);
static void rsub_flonum(n_real*, double, double);
static void rmul_flonum(n_real*, double, double);
static void rdiv_flonum(n_real*, double, double);
static int cmp_flonum(double, double);

/* fixnum fixnum */
static void rop_fi_fi_bi(n_real*, long, int);
static INLINE void radd_fi_fi(n_real*, long);
static INLINE void rsub_fi_fi(n_real*, long);
static INLINE void rmul_fi_fi(n_real*, long);
static INLINE void rdiv_fi_fi(n_real*, long);
static INLINE int cmp_fi_fi(long, long);
static void ndivide_fi_fi(n_number*, long, int, int);

/* fixnum bignum */
static void rop_fi_bi_xi(n_real*, mpi*, int);
static INLINE void radd_fi_bi(n_real*, mpi*);
static INLINE void rsub_fi_bi(n_real*, mpi*);
static INLINE void rmul_fi_bi(n_real*, mpi*);
static void rdiv_fi_bi(n_real*, mpi*);
static INLINE int cmp_fi_bi(long, mpi*);

/* fixnum fixratio */
static void rop_fi_fr_as_xr(n_real*, long, long, int);
static void rop_fi_fr_md_xr(n_real*, long, long, int);
static INLINE void radd_fi_fr(n_real*, long, long);
static INLINE void rsub_fi_fr(n_real*, long, long);
static INLINE void rmul_fi_fr(n_real*, long, long);
static INLINE void rdiv_fi_fr(n_real*, long, long);
static INLINE int cmp_fi_fr(long, long, long);

/* fixnum bigratio */
static void rop_fi_br_as_xr(n_real*, mpr*, int);
static void rop_fi_br_md_xr(n_real*, mpr*, int);
static INLINE void radd_fi_br(n_real*, mpr*);
static INLINE void rsub_fi_br(n_real*, mpr*);
static INLINE void rmul_fi_br(n_real*, mpr*);
static INLINE void rdiv_fi_br(n_real*, mpr*);
static INLINE int cmp_fi_br(long, mpr*);

/* bignum fixnum */
static INLINE void radd_bi_fi(n_real*, long);
static INLINE void rsub_bi_fi(n_real*, long);
static INLINE void rmul_bi_fi(n_real*, long);
static void rdiv_bi_fi(n_real*, long);
static INLINE int cmp_bi_fi(mpi*, long);

/* bignum bignum */
static INLINE void radd_bi_bi(n_real*, mpi*);
static INLINE void rsub_bi_bi(n_real*, mpi*);
static INLINE void rmul_bi_bi(n_real*, mpi*);
static void rdiv_bi_bi(n_real*, mpi*);
static INLINE int cmp_bi_bi(mpi*, mpi*);

/* bignum fixratio */
static void rop_bi_fr_as_xr(n_real*, long, long, int);
static void rop_bi_fr_md_xr(n_real*, long, long, int);
static INLINE void radd_bi_fr(n_real*, long, long);
static INLINE void rsub_bi_fr(n_real*, long, long);
static INLINE void rmul_bi_fr(n_real*, long, long);
static INLINE void rdiv_bi_fr(n_real*, long, long);
static int cmp_bi_fr(mpi*, long, long);

/* bignum bigratio */
static void rop_bi_br_as_xr(n_real*, mpr*, int);
static void rop_bi_br_md_xr(n_real*, mpr*, int);
static INLINE void radd_bi_br(n_real*, mpr*);
static INLINE void rsub_bi_br(n_real*, mpr*);
static INLINE void rmul_bi_br(n_real*, mpr*);
static INLINE void rdiv_bi_br(n_real*, mpr*);
static int cmp_bi_br(mpi*, mpr*);

/* fixratio fixnum */
static void rop_fr_fi_as_xr(n_real*, long, int);
static void rop_fr_fi_md_xr(n_real*, long, int);
static INLINE void radd_fr_fi(n_real*, long);
static INLINE void rsub_fr_fi(n_real*, long);
static INLINE void rmul_fr_fi(n_real*, long);
static INLINE void rdiv_fr_fi(n_real*, long);
static INLINE int cmp_fr_fi(long, long, long);

/* fixratio bignum */
static void rop_fr_bi_as_xr(n_real*, mpi*, int);
static void rop_fr_bi_md_xr(n_real*, mpi*, int);
static INLINE void radd_fr_bi(n_real*, mpi*);
static INLINE void rsub_fr_bi(n_real*, mpi*);
static INLINE void rmul_fr_bi(n_real*, mpi*);
static INLINE void rdiv_fr_bi(n_real*, mpi*);
static int cmp_fr_bi(long, long, mpi*);

/* fixratio fixratio */
static void rop_fr_fr_as_xr(n_real*, long, long, int);
static void rop_fr_fr_md_xr(n_real*, long, long, int);
static INLINE void radd_fr_fr(n_real*, long, long);
static INLINE void rsub_fr_fr(n_real*, long, long);
static INLINE void rmul_fr_fr(n_real*, long, long);
static INLINE void rdiv_fr_fr(n_real*, long, long);
static INLINE int cmp_fr_fr(long, long, long, long);

/* fixratio bigratio */
static void rop_fr_br_asmd_xr(n_real*, mpr*, int);
static INLINE void radd_fr_br(n_real*, mpr*);
static INLINE void rsub_fr_br(n_real*, mpr*);
static INLINE void rmul_fr_br(n_real*, mpr*);
static INLINE void rdiv_fr_br(n_real*, mpr*);
static int cmp_fr_br(long, long, mpr*);

/* bigratio fixnum */
static void rop_br_fi_asmd_xr(n_real*, long, int);
static INLINE void radd_br_fi(n_real*, long);
static INLINE void rsub_br_fi(n_real*, long);
static INLINE void rmul_br_fi(n_real*, long);
static INLINE void rdiv_br_fi(n_real*, long);
static int cmp_br_fi(mpr*, long);

/* bigratio bignum */
static void rop_br_bi_as_xr(n_real*, mpi*, int);
static INLINE void radd_br_bi(n_real*, mpi*);
static INLINE void rsub_br_bi(n_real*, mpi*);
static INLINE void rmul_br_bi(n_real*, mpi*);
static INLINE void rdiv_br_bi(n_real*, mpi*);
static int cmp_br_bi(mpr*, mpi*);

/* bigratio fixratio */
static void rop_br_fr_asmd_xr(n_real*, long, long, int);
static INLINE void radd_br_fr(n_real*, long, long);
static INLINE void rsub_br_fr(n_real*, long, long);
static INLINE void rmul_br_fr(n_real*, long, long);
static INLINE void rdiv_br_fr(n_real*, long, long);
static int cmp_br_fr(mpr*, long, long);

/* bigratio bigratio */
static INLINE void radd_br_br(n_real*, mpr*);
static INLINE void rsub_br_br(n_real*, mpr*);
static INLINE void rmul_br_br(n_real*, mpr*);
static INLINE void rdiv_br_br(n_real*, mpr*);
static INLINE int cmp_br_br(mpr*, mpr*);

/*
 * Initialization
 */
static n_real zero, one, two;

static char *fatal_error_strings[] = {
#define DIVIDE_BY_ZERO			0
    "divide by zero",
#define FLOATING_POINT_OVERFLOW		1
    "floating point overflow",
#define FLOATING_POINT_EXCEPTION	2
    "floating point exception"
};

static char *fatal_object_error_strings[] = {
#define NOT_A_NUMBER			0
    "is not a number",
#define NOT_A_REAL_NUMBER		1
    "is not a real number",
#define NOT_AN_INTEGER			2
    "is not an integer"
};

/*
 * Implementation
 */
static void
fatal_error(int num)
{
    LispDestroy(fatal_error_strings[num]);
}

static void
fatal_object_error(LispObj *obj, int num)
{
    LispDestroy("%s %s", STROBJ(obj), fatal_object_error_strings[num]);
}

static void
fatal_builtin_object_error(LispBuiltin *builtin, LispObj *obj, int num)
{
    LispDestroy("%s: %s %s", STRFUN(builtin), STROBJ(obj),
		fatal_object_error_strings[num]);
}

static void
number_init(void)
{
    zero.type = one.type = two.type = N_FIXNUM;
    zero.data.fixnum = 0;
    one.data.fixnum = 1;
    two.data.fixnum = 2;
}

static double
bi_getd(mpi *bignum)
{
    double value = mpi_getd(bignum);

    if (!finite(value))
	fatal_error(FLOATING_POINT_EXCEPTION);

    return (value);
}

static double
br_getd(mpr *bigratio)
{
    double value = mpr_getd(bigratio);

    if (!finite(value))
	fatal_error(FLOATING_POINT_EXCEPTION);

    return (value);
}

static LispObj *
number_pi(void)
{
    LispObj *result;
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
    result = DFLOAT(M_PI);

    return (result);
}

static void
set_real_real(n_real *real, n_real *val)
{
    switch (RTYPE(real) = RTYPE(val)) {
	case N_FIXNUM:
	    RFI(real) = RFI(val);
	    break;
	case N_BIGNUM:
	    RBI(real) = XALLOC(mpi);
	    mpi_init(RBI(real));
	    mpi_set(RBI(real), RBI(val));
	    break;
	case N_FLONUM:
	    RFF(real) = RFF(val);
	    break;
	case N_FIXRATIO:
	    RFRN(real) = RFRN(val);
	    RFRD(real) = RFRD(val);
	    break;
	case N_BIGRATIO:
	    RBR(real) = XALLOC(mpr);
	    mpr_init(RBR(real));
	    mpr_set(RBR(real), RBR(val));
	    break;
    }
}

static void
set_real_object(n_real *real, LispObj *obj)
{
    switch (OBJECT_TYPE(obj)) {
	case LispFixnum_t:
	    RTYPE(real) = N_FIXNUM;
	    RFI(real) = OFI(obj);
	    break;
	case LispInteger_t:
	    RTYPE(real) = N_FIXNUM;
	    RFI(real) = OII(obj);
	    break;
	case LispBignum_t:
	    RTYPE(real) = N_BIGNUM;
	    RBI(real) = XALLOC(mpi);
	    mpi_init(RBI(real));
	    mpi_set(RBI(real), OBI(obj));
	    break;
	case LispDFloat_t:
	    RTYPE(real) = N_FLONUM;
	    RFF(real) = ODF(obj);
	    break;
	case LispRatio_t:
	    RTYPE(real) = N_FIXRATIO;
	    RFRN(real) = OFRN(obj);
	    RFRD(real) = OFRD(obj);
	    break;
	case LispBigratio_t:
	    RTYPE(real) = N_BIGRATIO;
	    RBR(real) = XALLOC(mpr);
	    mpr_init(RBR(real));
	    mpr_set(RBR(real), OBR(obj));
	    break;
	default:
	    fatal_object_error(obj, NOT_A_REAL_NUMBER);
	    break;
    }
}

static void
set_number_object(n_number *num, LispObj *obj)
{
    switch (OBJECT_TYPE(obj)) {
	case LispFixnum_t:
	    num->complex = 0;
	    NRTYPE(num) = N_FIXNUM;
	    NRFI(num) = OFI(obj);
	    break;
	case LispInteger_t:
	    num->complex = 0;
	    NRTYPE(num) = N_FIXNUM;
	    NRFI(num) = OII(obj);
	    break;
	case LispBignum_t:
	    num->complex = 0;
	    NRTYPE(num) = N_BIGNUM;
	    NRBI(num) = XALLOC(mpi);
	    mpi_init(NRBI(num));
	    mpi_set(NRBI(num), OBI(obj));
	    break;
	case LispDFloat_t:
	    num->complex = 0;
	    NRTYPE(num) = N_FLONUM;
	    NRFF(num) = ODF(obj);
	    break;
	case LispRatio_t:
	    num->complex = 0;
	    NRTYPE(num) = N_FIXRATIO;
	    NRFRN(num) = OFRN(obj);
	    NRFRD(num) = OFRD(obj);
	    break;
	case LispBigratio_t:
	    num->complex = 0;
	    NRTYPE(num) = N_BIGRATIO;
	    NRBR(num) = XALLOC(mpr);
	    mpr_init(NRBR(num));
	    mpr_set(NRBR(num), OBR(obj));
	    break;
	case LispComplex_t:
	    num->complex = 1;
	    set_real_object(NREAL(num), OCXR(obj));
	    set_real_object(NIMAG(num), OCXI(obj));
	    break;
	default:
	    fatal_object_error(obj, NOT_A_NUMBER);
	    break;
    }
}

static void
clear_real(n_real *real)
{
    if (RTYPE(real) == N_BIGNUM) {
	mpi_clear(RBI(real));
	XFREE(RBI(real));
    }
    else if (RTYPE(real) == N_BIGRATIO) {
	mpr_clear(RBR(real));
	XFREE(RBR(real));
    }
}

static void
clear_number(n_number *num)
{
    clear_real(NREAL(num));
    if (num->complex)
	clear_real(NIMAG(num));
}

static LispObj *
make_real_object(n_real *real)
{
    LispObj *obj;

    switch (RTYPE(real)) {
	case N_FIXNUM:
	    if (RFI(real) > MOST_POSITIVE_FIXNUM ||
		RFI(real) < MOST_NEGATIVE_FIXNUM) {
		obj = LispNew(NIL, NIL);
		obj->type = LispInteger_t;
		OII(obj) = RFI(real);
	    }
	    else
		obj = FIXNUM(RFI(real));
	    break;
	case N_BIGNUM:
	    obj = BIGNUM(RBI(real));
	    break;
	case N_FLONUM:
	    obj = DFLOAT(RFF(real));
	    break;
	case N_FIXRATIO:
	    obj = LispNew(NIL, NIL);
	    obj->type = LispRatio_t;
	    OFRN(obj) = RFRN(real);
	    OFRD(obj) = RFRD(real);
	    break;
	case N_BIGRATIO:
	    obj = BIGRATIO(RBR(real));
	    break;
	default:
	    obj = NIL;
	    break;
    }

    return (obj);
}

static LispObj *
make_number_object(n_number *num)
{
    LispObj *obj;

    if (num->complex) {
	GC_ENTER();

	obj = LispNew(NIL, NIL);
	GC_PROTECT(obj);
	OCXI(obj) = NIL;
	obj->type = LispComplex_t;
	OCXR(obj) = make_real_object(NREAL(num));
	OCXI(obj) = make_real_object(NIMAG(num));
	GC_LEAVE();
    }
    else {
	switch (NRTYPE(num)) {
	    case N_FIXNUM:
		if (NRFI(num) > MOST_POSITIVE_FIXNUM ||
		    NRFI(num) < MOST_NEGATIVE_FIXNUM) {
		    obj = LispNew(NIL, NIL);
		    obj->type = LispInteger_t;
		    OII(obj) = NRFI(num);
		}
		else
		    obj = FIXNUM(NRFI(num));
		break;
	    case N_BIGNUM:
		obj = BIGNUM(NRBI(num));
		break;
	    case N_FLONUM:
		obj = DFLOAT(NRFF(num));
		break;
	    case N_FIXRATIO:
		obj = LispNew(NIL, NIL);
		obj->type = LispRatio_t;
		OFRN(obj) = NRFRN(num);
		OFRD(obj) = NRFRD(num);
		break;
	    case N_BIGRATIO:
		obj = BIGRATIO(NRBR(num));
		break;
	    default:
		obj = NIL;
		break;
	}
    }

    return (obj);
}

#define DEFOP_REAL_REAL(OP)						\
OP##_real_real(n_real *real, n_real *val)				\
{									\
    switch (RTYPE(real)) {						\
	case N_FIXNUM:							\
	    switch (RTYPE(val)) {					\
		case N_FIXNUM:						\
		    r##OP##_fi_fi(real, RFI(val));			\
		    break;						\
		case N_BIGNUM:						\
		    r##OP##_fi_bi(real, RBI(val));			\
		    break;						\
		case N_FLONUM:						\
		    r##OP##_flonum(real, (double)RFI(real), RFF(val));	\
		    break;						\
		case N_FIXRATIO:					\
		    r##OP##_fi_fr(real, RFRN(val), RFRD(val));		\
		    break;						\
		case N_BIGRATIO:					\
		    r##OP##_fi_br(real, RBR(val));			\
		    break;						\
	    }								\
	    break;							\
	case N_BIGNUM:							\
	    switch (RTYPE(val)) {					\
		case N_FIXNUM:						\
		    r##OP##_bi_fi(real, RFI(val));			\
		    break;						\
		case N_BIGNUM:						\
		    r##OP##_bi_bi(real, RBI(val));			\
		    break;						\
		case N_FLONUM:						\
		    r##OP##_flonum(real, bi_getd(RBI(real)), RFF(val));	\
		    break;						\
		case N_FIXRATIO:					\
		    r##OP##_bi_fr(real, RFRN(val), RFRD(val));		\
		    break;						\
		case N_BIGRATIO:					\
		    r##OP##_bi_br(real, RBR(val));			\
		    break;						\
	    }								\
	    break;							\
	case N_FLONUM:							\
	    switch (RTYPE(val)) {					\
		case N_FIXNUM:						\
		    r##OP##_flonum(real, RFF(real), (double)RFI(val));	\
		    break;						\
		case N_BIGNUM:						\
		    r##OP##_flonum(real, RFF(real), bi_getd(RBI(val)));	\
		    break;						\
		case N_FLONUM:						\
		    r##OP##_flonum(real, RFF(real), RFF(val));		\
		    break;						\
		case N_FIXRATIO:					\
		    r##OP##_flonum(real, RFF(real),			\
				 (double)RFRN(val) / (double)RFRD(val));\
		    break;						\
		case N_BIGRATIO:					\
		    r##OP##_flonum(real, RFF(real), br_getd(RBR(val)));	\
		    break;						\
	    }								\
	    break;							\
	case N_FIXRATIO:						\
	    switch (RTYPE(val)) {					\
		case N_FIXNUM:						\
		    r##OP##_fr_fi(real, RFI(val));			\
		    break;						\
		case N_BIGNUM:						\
		    r##OP##_fr_bi(real, RBI(val));			\
		    break;						\
		case N_FLONUM:						\
		    r##OP##_flonum(real,				\
				(double)RFRN(real) / (double)RFRD(real),\
				RFF(val));				\
		    break;						\
		case N_FIXRATIO:					\
		    r##OP##_fr_fr(real, RFRN(val), RFRD(val));		\
		    break;						\
		case N_BIGRATIO:					\
		    r##OP##_fr_br(real, RBR(val));			\
		    break;						\
	    }								\
	    break;							\
	case N_BIGRATIO:						\
	    switch (RTYPE(val)) {					\
		case N_FIXNUM:						\
		    r##OP##_br_fi(real, RFI(val));			\
		    break;						\
		case N_BIGNUM:						\
		    r##OP##_br_bi(real, RBI(val));			\
		    break;						\
		case N_FLONUM:						\
		    r##OP##_flonum(real, br_getd(RBR(real)), RFF(val));	\
		    break;						\
		case N_FIXRATIO:					\
		    r##OP##_br_fr(real, RFRN(val), RFRD(val));		\
		    break;						\
		case N_BIGRATIO:					\
		    r##OP##_br_br(real, RBR(val));			\
		    break;						\
	    }								\
	    break;							\
    }									\
}

static void
DEFOP_REAL_REAL(add)

static void
DEFOP_REAL_REAL(sub)

static void
DEFOP_REAL_REAL(div)

static void
DEFOP_REAL_REAL(mul)


#define DEFOP_REAL_OBJECT(OP)						\
OP##_real_object(n_real *real, LispObj *obj)				\
{									\
    switch (OBJECT_TYPE(obj)) {						\
	case LispFixnum_t:						\
	    switch (RTYPE(real)) {					\
		case N_FIXNUM:						\
		    r##OP##_fi_fi(real, OFI(obj));			\
		    break;						\
		case N_BIGNUM:						\
		    r##OP##_bi_fi(real, OFI(obj));			\
		    break;						\
		case N_FLONUM:						\
		    r##OP##_flonum(real, RFF(real), (double)OFI(obj));	\
		    break;						\
		case N_FIXRATIO:					\
		    r##OP##_fr_fi(real, OFI(obj));			\
		    break;						\
		case N_BIGRATIO:					\
		    r##OP##_br_fi(real, OFI(obj));			\
		    break;						\
	    }								\
	    break;							\
	case LispInteger_t:						\
	    switch (RTYPE(real)) {					\
		case N_FIXNUM:						\
		    r##OP##_fi_fi(real, OII(obj));			\
		    break;						\
		case N_BIGNUM:						\
		    r##OP##_bi_fi(real, OII(obj));			\
		    break;						\
		case N_FLONUM:						\
		    r##OP##_flonum(real, RFF(real), (double)OII(obj));	\
		    break;						\
		case N_FIXRATIO:					\
		    r##OP##_fr_fi(real, OII(obj));			\
		    break;						\
		case N_BIGRATIO:					\
		    r##OP##_br_fi(real, OII(obj));			\
		    break;						\
	    }								\
	    break;							\
	case LispBignum_t:						\
	    switch (RTYPE(real)) {					\
		case N_FIXNUM:						\
		    r##OP##_fi_bi(real, OBI(obj));			\
		    break;						\
		case N_BIGNUM:						\
		    r##OP##_bi_bi(real, OBI(obj));			\
		    break;						\
		case N_FLONUM:						\
		    r##OP##_flonum(real, RFF(real), bi_getd(OBI(obj)));	\
		    break;						\
		case N_FIXRATIO:					\
		    r##OP##_fr_bi(real, OBI(obj));			\
		    break;						\
		case N_BIGRATIO:					\
		    r##OP##_br_bi(real, OBI(obj));			\
		    break;						\
	    }								\
	    break;							\
	case LispDFloat_t:						\
	    switch (RTYPE(real)) {					\
		case N_FIXNUM:						\
		    r##OP##_flonum(real, (double)RFI(real), ODF(obj));	\
		    break;						\
		case N_BIGNUM:						\
		    r##OP##_flonum(real, bi_getd(RBI(real)), ODF(obj));	\
		    break;						\
		case N_FLONUM:						\
		    r##OP##_flonum(real, RFF(real), ODF(obj));		\
		    break;						\
		case N_FIXRATIO:					\
		    r##OP##_flonum(real,				\
				(double)RFRN(real) / (double)RFRD(real),\
				ODF(obj));				\
		    break;						\
		case N_BIGRATIO:					\
		    r##OP##_flonum(real, br_getd(RBR(real)), ODF(obj));	\
		    break;						\
	    }								\
	    break;							\
	case LispRatio_t:						\
	    switch (RTYPE(real)) {					\
		case N_FIXNUM:						\
		    r##OP##_fi_fr(real, OFRN(obj), OFRD(obj));		\
		    break;						\
		case N_BIGNUM:						\
		    r##OP##_bi_fr(real, OFRN(obj), OFRD(obj));		\
		    break;						\
		case N_FLONUM:						\
		    r##OP##_flonum(real, RFF(real),			\
				(double)OFRN(obj) / (double)OFRD(obj));	\
		    break;						\
		case N_FIXRATIO:					\
		    r##OP##_fr_fr(real, OFRN(obj), OFRD(obj));		\
		    break;						\
		case N_BIGRATIO:					\
		    r##OP##_br_fr(real, OFRN(obj), OFRD(obj));		\
		    break;						\
	    }								\
	    break;							\
	case LispBigratio_t:						\
	    switch (RTYPE(real)) {					\
		case N_FIXNUM:						\
		    r##OP##_fi_br(real, OBR(obj));			\
		    break;						\
		case N_BIGNUM:						\
		    r##OP##_bi_br(real, OBR(obj));			\
		    break;						\
		case N_FLONUM:						\
		    r##OP##_flonum(real, RFF(real), br_getd(OBR(obj)));	\
		    break;						\
		case N_FIXRATIO:					\
		    r##OP##_fr_br(real, OBR(obj));			\
		    break;						\
		case N_BIGRATIO:					\
		    r##OP##_br_br(real, OBR(obj));			\
		    break;						\
	    }								\
	    break;							\
	default:							\
	    fatal_object_error(obj, NOT_A_REAL_NUMBER);			\
	    break;							\
    }									\
}

static void
DEFOP_REAL_OBJECT(add)

static void
DEFOP_REAL_OBJECT(sub)

static void
DEFOP_REAL_OBJECT(div)

static void
DEFOP_REAL_OBJECT(mul)


#define DEFOP_NUMBER_OBJECT(OP)						\
OP##_number_object(n_number *num, LispObj *obj)				\
{									\
    if (num->complex) {							\
	switch (OBJECT_TYPE(obj)) {					\
	    case LispFixnum_t:						\
	    case LispInteger_t:						\
	    case LispBignum_t:						\
	    case LispDFloat_t:						\
	    case LispRatio_t:						\
	    case LispBigratio_t:					\
		n##OP##_cx_re(num, obj);				\
		break;							\
	    case LispComplex_t:						\
		n##OP##_cx_cx(num, obj);				\
		break;							\
	    default:							\
		fatal_object_error(obj, NOT_A_NUMBER);			\
		break;							\
	}								\
    }									\
    else {								\
	switch (OBJECT_TYPE(obj)) {					\
	    case LispFixnum_t:						\
		switch (NRTYPE(num)) {					\
		    case N_FIXNUM:					\
			r##OP##_fi_fi(NREAL(num), OFI(obj));		\
			break;						\
		    case N_BIGNUM:					\
			r##OP##_bi_fi(NREAL(num), OFI(obj));		\
			break;						\
		    case N_FLONUM:					\
			r##OP##_flonum(NREAL(num), NRFF(num),		\
				    (double)OFI(obj));			\
			break;						\
		    case N_FIXRATIO:					\
			r##OP##_fr_fi(NREAL(num), OFI(obj));		\
			break;						\
		    case N_BIGRATIO:					\
			r##OP##_br_fi(NREAL(num), OFI(obj));		\
			break;						\
		}							\
		break;							\
	    case LispInteger_t:						\
		switch (NRTYPE(num)) {					\
		    case N_FIXNUM:					\
			r##OP##_fi_fi(NREAL(num), OII(obj));		\
			break;						\
		    case N_BIGNUM:					\
			r##OP##_bi_fi(NREAL(num), OII(obj));		\
			break;						\
		    case N_FLONUM:					\
			r##OP##_flonum(NREAL(num), NRFF(num),		\
				    (double)OII(obj));			\
			break;						\
		    case N_FIXRATIO:					\
			r##OP##_fr_fi(NREAL(num), OII(obj));		\
			break;						\
		    case N_BIGRATIO:					\
			r##OP##_br_fi(NREAL(num), OII(obj));		\
			break;						\
		}							\
		break;							\
	    case LispBignum_t:						\
		switch (NRTYPE(num)) {					\
		    case N_FIXNUM:					\
			r##OP##_fi_bi(NREAL(num), OBI(obj));		\
			break;						\
		    case N_BIGNUM:					\
			r##OP##_bi_bi(NREAL(num), OBI(obj));		\
			break;						\
		    case N_FLONUM:					\
			r##OP##_flonum(NREAL(num), NRFF(num),		\
				       bi_getd(OBI(obj)));		\
			break;						\
		    case N_FIXRATIO:					\
			r##OP##_fr_bi(NREAL(num), OBI(obj));		\
			break;						\
		    case N_BIGRATIO:					\
			r##OP##_br_bi(NREAL(num), OBI(obj));		\
			break;						\
		}							\
		break;							\
	    case LispDFloat_t:						\
		switch (NRTYPE(num)) {					\
		    case N_FIXNUM:					\
			r##OP##_flonum(NREAL(num), (double)NRFI(num),	\
				    ODF(obj));				\
			break;						\
		    case N_BIGNUM:					\
			r##OP##_flonum(NREAL(num), bi_getd(NRBI(num)),	\
				    ODF(obj));				\
			break;						\
		    case N_FLONUM:					\
			r##OP##_flonum(NREAL(num), NRFF(num), ODF(obj));\
			break;						\
		    case N_FIXRATIO:					\
			r##OP##_flonum(NREAL(num),			\
				    (double)NRFRN(num) /		\
				    (double)NRFRD(num),			\
				    ODF(obj));				\
			break;						\
		    case N_BIGRATIO:					\
			r##OP##_flonum(NREAL(num), br_getd(NRBR(num)),	\
				    ODF(obj));				\
			break;						\
		}							\
		break;							\
	    case LispRatio_t:						\
		switch (NRTYPE(num)) {					\
		    case N_FIXNUM:					\
			r##OP##_fi_fr(NREAL(num), OFRN(obj), OFRD(obj));\
			break;						\
		    case N_BIGNUM:					\
			r##OP##_bi_fr(NREAL(num), OFRN(obj), OFRD(obj));\
			break;						\
		    case N_FLONUM:					\
			r##OP##_flonum(NREAL(num), NRFF(num),		\
				    (double)OFRN(obj) /			\
				    (double)OFRD(obj));			\
			break;						\
		    case N_FIXRATIO:					\
			r##OP##_fr_fr(NREAL(num), OFRN(obj), OFRD(obj));\
			break;						\
		    case N_BIGRATIO:					\
			r##OP##_br_fr(NREAL(num), OFRN(obj), OFRD(obj));\
			break;						\
		}							\
		break;							\
	    case LispBigratio_t:					\
		switch (NRTYPE(num)) {					\
		    case N_FIXNUM:					\
			r##OP##_fi_br(NREAL(num), OBR(obj));		\
			break;						\
		    case N_BIGNUM:					\
			r##OP##_bi_br(NREAL(num), OBR(obj));		\
			break;						\
		    case N_FLONUM:					\
			r##OP##_flonum(NREAL(num), NRFF(num),		\
				    br_getd(OBR(obj)));		\
			break;						\
		    case N_FIXRATIO:					\
			r##OP##_fr_br(NREAL(num), OBR(obj));		\
			break;						\
		    case N_BIGRATIO:					\
			r##OP##_br_br(NREAL(num), OBR(obj));		\
			break;						\
		}							\
		break;							\
	    case LispComplex_t:						\
		n##OP##_re_cx(num, obj);				\
		break;							\
	    default:							\
		fatal_object_error(obj, NOT_A_NUMBER);			\
		break;							\
	}								\
    }									\
}

static void
DEFOP_NUMBER_OBJECT(add)

static void
DEFOP_NUMBER_OBJECT(sub)

static void
DEFOP_NUMBER_OBJECT(div)

static void
DEFOP_NUMBER_OBJECT(mul)


/************************************************************************
 * ABS
 ************************************************************************/
static void
abs_real(n_real *real)
{
    switch (RTYPE(real)) {
	case N_FIXNUM:		rabs_fi(real);	break;
	case N_BIGNUM:		rabs_bi(real);	break;
	case N_FLONUM:		rabs_ff(real);	break;
	case N_FIXRATIO:	rabs_fr(real);	break;
	case N_BIGRATIO:	rabs_br(real);	break;
    }
}

static void
abs_number(n_number *num)
{
    if (num->complex)
	nabs_cx(num);
    else {
	switch (NRTYPE(num)) {
	    case N_FIXNUM:	rabs_fi(NREAL(num));	break;
	    case N_BIGNUM:	rabs_bi(NREAL(num));	break;
	    case N_FLONUM:	rabs_ff(NREAL(num));	break;
	    case N_FIXRATIO:	rabs_fr(NREAL(num));	break;
	    case N_BIGRATIO:	rabs_br(NREAL(num));	break;
	}
    }
}

static void
nabs_cx(n_number *num)
{
    n_real temp;

    abs_real(NREAL(num));
    abs_real(NIMAG(num));

    if (cmp_real_real(NREAL(num), NIMAG(num)) < 0) {
	memcpy(&temp, NIMAG(num), sizeof(n_real));
	memcpy(NIMAG(num), NREAL(num), sizeof(n_real));
	memcpy(NREAL(num), &temp, sizeof(n_real));
    }

    if (cmp_real_real(NIMAG(num), &zero) == 0) {
	num->complex = 0;
	if (NITYPE(num) == N_FLONUM) {
	    /* change number type */
	    temp.type = N_FLONUM;
	    temp.data.flonum = 1.0;
	    mul_real_real(NREAL(num), &temp);
	}
	else
	    clear_real(NIMAG(num));
    }
    else {
	div_real_real(NIMAG(num), NREAL(num));
	set_real_real(&temp, NIMAG(num));
	mul_real_real(NIMAG(num), &temp);
	clear_real(&temp);

	add_real_real(NIMAG(num), &one);
	sqrt_real(NIMAG(num));

	mul_real_real(NIMAG(num), NREAL(num));
	clear_real(NREAL(num));
	memcpy(NREAL(num), NIMAG(num), sizeof(n_real));
	num->complex = 0;
    }
}

static INLINE void
rabs_fi(n_real *real)
{
    if (RFI(real) < 0)
	rneg_fi(real);
}

static INLINE void
rabs_bi(n_real *real)
{
    if (mpi_cmpi(RBI(real), 0) < 0)
	mpi_neg(RBI(real), RBI(real));
}

static INLINE void
rabs_ff(n_real *real)
{
    if (RFF(real) < 0.0)
	RFF(real) = -RFF(real);
}

static INLINE void
rabs_fr(n_real *real)
{
    if (RFRN(real) < 0)
	rneg_fr(real);
}

static INLINE void
rabs_br(n_real *real)
{
    if (mpi_cmpi(RBRN(real), 0) < 0)
	mpi_neg(RBRN(real), RBRN(real));
}


/************************************************************************
 * NEG
 ************************************************************************/
static void
neg_real(n_real *real)
{
    switch (RTYPE(real)) {
	case N_FIXNUM:		rneg_fi(real);	break;
	case N_BIGNUM:		rneg_bi(real);	break;
	case N_FLONUM:		rneg_ff(real);	break;
	case N_FIXRATIO:	rneg_fr(real);	break;
	case N_BIGRATIO:	rneg_br(real);	break;
    }
}

static void
neg_number(n_number *num)
{
    if (num->complex) {
	neg_real(NREAL(num));
	neg_real(NIMAG(num));
    }
    else {
	switch (NRTYPE(num)) {
	    case N_FIXNUM:	rneg_fi(NREAL(num));	break;
	    case N_BIGNUM:	rneg_bi(NREAL(num));	break;
	    case N_FLONUM:	rneg_ff(NREAL(num));	break;
	    case N_FIXRATIO:	rneg_fr(NREAL(num));	break;
	    case N_BIGRATIO:	rneg_br(NREAL(num));	break;
	}
    }
}

static void
rneg_fi(n_real *real)
{
    if (RFI(real) == MINSLONG) {
	mpi *bigi = XALLOC(mpi);

	mpi_init(bigi);
	mpi_seti(bigi, RFI(real));
	mpi_neg(bigi, bigi);
	RTYPE(real) = N_BIGNUM;
	RBI(real) = bigi;
    }
    else
	RFI(real) = -RFI(real);
}

static INLINE void
rneg_bi(n_real *real)
{
    mpi_neg(RBI(real), RBI(real));
}

static INLINE void
rneg_ff(n_real *real)
{
    RFF(real) = -RFF(real);
}

static void
rneg_fr(n_real *real)
{
    if (RFRN(real) == MINSLONG) {
	mpr *bigr = XALLOC(mpr);

	mpr_init(bigr);
	mpr_seti(bigr, RFRN(real), RFRD(real));
	mpi_neg(mpr_num(bigr), mpr_num(bigr));
	RTYPE(real) = N_BIGRATIO;
	RBR(real) = bigr;
    }
    else
	RFRN(real) = -RFRN(real);
}

static INLINE void
rneg_br(n_real *real)
{
    mpi_neg(RBRN(real), RBRN(real));
}


/************************************************************************
 * SQRT
 ************************************************************************/
static void
sqrt_real(n_real *real)
{
    switch (RTYPE(real)) {
	case N_FIXNUM:
	case N_BIGNUM:
	    rsqrt_xi(real);
	    break;
	case N_FLONUM:
	    rsqrt_ff(real);
	    break;
	case N_FIXRATIO:
	case N_BIGRATIO:
	    rsqrt_xr(real);
	    break;
    }
}

static void
sqrt_number(n_number *num)
{
    if (num->complex)
	nsqrt_cx(num);
    else {
	switch (NRTYPE(num)) {
	    case N_FIXNUM:
	    case N_BIGNUM:
		nsqrt_xi(num);
		break;
	    case N_FLONUM:
		nsqrt_ff(num);
		break;
	    case N_FIXRATIO:
	    case N_BIGRATIO:
		nsqrt_xr(num);
		break;
	}
    }
}

static void
rsqrt_xi(n_real *real)
{
    int exact;
    mpi bignum;

    if (cmp_real_real(real, &zero) < 0)
	fatal_error(FLOATING_POINT_EXCEPTION);

    mpi_init(&bignum);
    if (RTYPE(real) == N_BIGNUM)
	exact = mpi_sqrt(&bignum, RBI(real));
    else {
	mpi tmp;

	mpi_init(&tmp);
	mpi_seti(&tmp, RFI(real));
	exact = mpi_sqrt(&bignum, &tmp);
	mpi_clear(&tmp);
    }
    if (exact) {
	if (RTYPE(real) == N_BIGNUM) {
	    mpi_set(RBI(real), &bignum);
	    rbi_canonicalize(real);
	}
	else
	    RFI(real) = mpi_geti(&bignum);
    }
    else {
	double value;

	if (RTYPE(real) == N_BIGNUM) {
	    value = bi_getd(RBI(real));
	    RCLEAR_BI(real);
	}
	else
	    value = (double)RFI(real);

	value = sqrt(value);
	RTYPE(real) = N_FLONUM;
	RFF(real) = value;
    }
    mpi_clear(&bignum);
}

static void
rsqrt_xr(n_real *real)
{
    n_real num, den;

    if (cmp_real_real(real, &zero) < 0)
	fatal_error(FLOATING_POINT_EXCEPTION);

    if (RTYPE(real) == N_FIXRATIO) {
	num.type = den.type = N_FIXNUM;
	num.data.fixnum = RFRN(real);
	den.data.fixnum = RFRD(real);
    }
    else {
	mpi *bignum;

	if (mpi_fiti(RBRN(real))) {
	    num.type = N_FIXNUM;
	    num.data.fixnum = mpi_geti(RBRN(real));
	}
	else {
	    bignum = XALLOC(mpi);
	    mpi_init(bignum);
	    mpi_set(bignum, RBRN(real));
	    num.type = N_BIGNUM;
	    num.data.bignum = bignum;
	}

	if (mpi_fiti(RBRD(real))) {
	    den.type = N_FIXNUM;
	    den.data.fixnum = mpi_geti(RBRD(real));
	}
	else {
	    bignum = XALLOC(mpi);
	    mpi_init(bignum);
	    mpi_set(bignum, RBRD(real));
	    den.type = N_BIGNUM;
	    den.data.bignum = bignum;
	}
    }

    rsqrt_xi(&num);
    rsqrt_xi(&den);

    clear_real(real);
    memcpy(real, &num, sizeof(n_real));
    div_real_real(real, &den);
    clear_real(&den);
}

static void
rsqrt_ff(n_real *real)
{
    if (RFF(real) < 0.0)
	fatal_error(FLOATING_POINT_EXCEPTION);
    RFF(real) = sqrt(RFF(real));
}


static void
nsqrt_cx(n_number *num)
{
    n_number mag;
    n_real *real, *imag;

    real = &(mag.real);
    imag = &(mag.imag);
    set_real_real(real, NREAL(num));
    set_real_real(imag, NIMAG(num));
    mag.complex = 1;

    nabs_cx(&mag);	/* this will free the imag part data */
    if (cmp_real_real(real, &zero) == 0) {
	clear_number(num);
	memcpy(NREAL(num), real, sizeof(n_real));
	clear_real(real);
	num->complex = 0;
	return;
    }
    else if (cmp_real_real(NREAL(num), &zero) > 0) {
	/* R = sqrt((mag + Ra) / 2) */
	add_real_real(NREAL(num), real);
	clear_real(real);
	div_real_real(NREAL(num), &two);
	sqrt_real(NREAL(num));

	/* I = Ia / R / 2 */
	div_real_real(NIMAG(num), NREAL(num));
	div_real_real(NIMAG(num), &two);
    }
    else {
	/* remember old imag part */
	memcpy(imag, NIMAG(num), sizeof(n_real));

	/* I = sqrt((mag - Ra) / 2) */
	memcpy(NIMAG(num), real, sizeof(n_real));
	sub_real_real(NIMAG(num), NREAL(num));
	div_real_real(NIMAG(num), &two);
	sqrt_real(NIMAG(num));
	if (cmp_real_real(imag, &zero) < 0)
	    neg_real(NIMAG(num));

	/* R = Ia / I / 2 */
	clear_real(NREAL(num));
	/* start with old imag part */
	memcpy(NREAL(num), imag, sizeof(n_real));
	div_real_real(NREAL(num), NIMAG(num));
	div_real_real(NREAL(num), &two);
    }

    ncx_canonicalize(num);
}

static void
nsqrt_xi(n_number *num)
{
    if (cmp_real_real(NREAL(num), &zero) < 0) {
	memcpy(NIMAG(num), NREAL(num), sizeof(n_real));
	neg_real(NIMAG(num));
	rsqrt_xi(NIMAG(num));
	NRTYPE(num) = N_FIXNUM;
	NRFI(num) = 0;
	num->complex = 1;
    }
    else
	rsqrt_xi(NREAL(num));
}

static void
nsqrt_ff(n_number *num)
{
    double value;

    if (NRFF(num) < 0.0) {
	value = sqrt(-NRFF(num));

	NITYPE(num) = N_FLONUM;
	NIFF(num) = value;
	NRTYPE(num) = N_FIXNUM;
	NRFI(num) = 0;
	num->complex = 1;
    }
    else {
	value = sqrt(NRFF(num));
	NRFF(num) = value;
    }
}

static void
nsqrt_xr(n_number *num)
{
    if (cmp_real_real(NREAL(num), &zero) < 0) {
	memcpy(NIMAG(num), NREAL(num), sizeof(n_real));
	neg_real(NIMAG(num));
	rsqrt_xr(NIMAG(num));
	NRTYPE(num) = N_FIXNUM;
	NRFI(num) = 0;
	num->complex = 1;
    }
    else
	rsqrt_xr(NREAL(num));
}


/************************************************************************
 * MOD
 ************************************************************************/
static void
mod_real_real(n_real *real, n_real *val)
{
    /* Assume both operands are integers */
    switch (RTYPE(real)) {
	case N_FIXNUM:
	    switch (RTYPE(val)) {
		case N_FIXNUM:
		    rmod_fi_fi(real, RFI(val));
		    break;
		case N_BIGNUM:
		    rmod_fi_bi(real, RBI(val));
		    break;
	    }
	    break;
	case N_BIGNUM:
	    switch (RTYPE(val)) {
		case N_FIXNUM:
		    rmod_bi_fi(real, RFI(val));
		    break;
		case N_BIGNUM:
		    rmod_bi_bi(real, RBI(val));
		    break;
	    }
	    break;
    }
}

static void
mod_real_object(n_real *real, LispObj *obj)
{
    switch (RTYPE(real)) {
	case N_FIXNUM:
	    switch (OBJECT_TYPE(obj)) {
		case LispFixnum_t:
		    rmod_fi_fi(real, OFI(obj));
		    return;
		case LispInteger_t:
		    rmod_fi_fi(real, OII(obj));
		    return;
		case LispBignum_t:
		    rmod_fi_bi(real, OBI(obj));
		    return;
		default:
		    break;
	    }
	    break;
	case N_BIGNUM:
	    switch (OBJECT_TYPE(obj)) {
		case LispFixnum_t:
		    rmod_bi_fi(real, OFI(obj));
		    return;
		case LispInteger_t:
		    rmod_bi_fi(real, OII(obj));
		    return;
		case LispBignum_t:
		    rmod_bi_bi(real, OBI(obj));
		    return;
		default:
		    break;
	    }
	    break;
	/* Assume the n_real object is an integer */
    }
    fatal_object_error(obj, NOT_AN_INTEGER);
}

static void
rmod_fi_fi(n_real *real, long fi)
{
    if (fi == 0)
	fatal_error(DIVIDE_BY_ZERO);

    if ((RFI(real) < 0) ^ (fi < 0))
	RFI(real) = (RFI(real) % fi) + fi;
    else if (RFI(real) == MINSLONG || fi == MINSLONG) {
	mpi bignum;

	mpi_init(&bignum);
	mpi_seti(&bignum, RFI(real));
	RFI(real) = mpi_modi(&bignum, fi);
	mpi_clear(&bignum);
    }
    else
	RFI(real) = RFI(real) % fi;
}

static void
rmod_fi_bi(n_real *real, mpi *bignum)
{
    mpi *bigi;

    if (mpi_cmpi(bignum, 0) == 0)
	fatal_error(DIVIDE_BY_ZERO);

    bigi = XALLOC(mpi);
    mpi_init(bigi);
    mpi_seti(bigi, RFI(real));
    mpi_mod(bigi, bigi, bignum);
    RTYPE(real) = N_BIGNUM;
    RBI(real) = bigi;
    rbi_canonicalize(real);
}

static void
rmod_bi_fi(n_real *real, long fi)
{
    mpi iop;

    if (fi == 0)
	fatal_error(DIVIDE_BY_ZERO);

    mpi_init(&iop);
    mpi_seti(&iop, fi);
    mpi_mod(RBI(real), RBI(real), &iop);
    mpi_clear(&iop);
    rbi_canonicalize(real);
}

static void
rmod_bi_bi(n_real *real, mpi *bignum)
{
    if (mpi_cmpi(bignum, 0) == 0)
	fatal_error(DIVIDE_BY_ZERO);

    mpi_mod(RBI(real), RBI(real), bignum);
    rbi_canonicalize(real);
}

/************************************************************************
 * REM
 ************************************************************************/
static void
rem_real_object(n_real *real, LispObj *obj)
{
    switch (RTYPE(real)) {
	case N_FIXNUM:
	    switch (OBJECT_TYPE(obj)) {
		case LispFixnum_t:
		    rrem_fi_fi(real, OFI(obj));
		    return;
		case LispInteger_t:
		    rrem_fi_fi(real, OII(obj));
		    return;
		case LispBignum_t:
		    rrem_fi_bi(real, OBI(obj));
		    return;
		default:
		    break;
	    }
	    break;
	case N_BIGNUM:
	    switch (OBJECT_TYPE(obj)) {
		case LispFixnum_t:
		    rrem_bi_fi(real, OFI(obj));
		    return;
		case LispInteger_t:
		    rrem_bi_fi(real, OII(obj));
		    return;
		case LispBignum_t:
		    rrem_bi_bi(real, OBI(obj));
		    return;
		default:
		    break;
	    }
	    break;
	/* Assume the n_real object is an integer */
    }
    fatal_object_error(obj, NOT_AN_INTEGER);
}

static void
rrem_fi_fi(n_real *real, long fi)
{
    if (fi == 0)
	fatal_error(DIVIDE_BY_ZERO);

    if (RFI(real) == MINSLONG || fi == MINSLONG) {
	mpi bignum;

	mpi_init(&bignum);
	mpi_seti(&bignum, RFI(real));
	RFI(real) = mpi_remi(&bignum, fi);
	mpi_clear(&bignum);
    }
    else
	RFI(real) = RFI(real) % fi;
}

static void
rrem_fi_bi(n_real *real, mpi *bignum)
{
    mpi *bigi;

    if (mpi_cmpi(bignum, 0) == 0)
	fatal_error(DIVIDE_BY_ZERO);

    bigi = XALLOC(mpi);
    mpi_init(bigi);
    mpi_seti(bigi, RFI(real));
    mpi_rem(bigi, bigi, bignum);
    RTYPE(real) = N_BIGNUM;
    RBI(real) = bigi;
    rbi_canonicalize(real);
}

static void
rrem_bi_fi(n_real *real, long fi)
{
    mpi iop;

    if (fi == 0)
	fatal_error(DIVIDE_BY_ZERO);

    mpi_init(&iop);
    mpi_seti(&iop, fi);
    mpi_rem(RBI(real), RBI(real), &iop);
    mpi_clear(&iop);
    rbi_canonicalize(real);
}

static void
rrem_bi_bi(n_real *real, mpi *bignum)
{
    if (mpi_cmpi(bignum, 0) == 0)
	fatal_error(DIVIDE_BY_ZERO);

    mpi_rem(RBI(real), RBI(real), bignum);
    rbi_canonicalize(real);
}


/************************************************************************
 * GCD
 ************************************************************************/
static void
gcd_real_object(n_real *real, LispObj *obj)
{
    if (!INTEGERP(obj))
	fatal_object_error(obj, NOT_AN_INTEGER);

    /* check for zero operand */
    if (cmp_real_real(real, &zero) == 0)
	set_real_object(real, obj);
    else if (cmp_real_object(&zero, obj) != 0) {
	n_real rest, temp;

	set_real_object(&rest, obj);
	for (;;) {
	    mod_real_real(&rest, real);
	    if (cmp_real_real(&rest, &zero) == 0)
		break;
	    memcpy(&temp, real, sizeof(n_real));
	    memcpy(real, &rest, sizeof(n_real));
	    memcpy(&rest, &temp, sizeof(n_real));
	}
	clear_real(&rest);
    }
}

/************************************************************************
 * AND
 ************************************************************************/
static void
and_real_object(n_real *real, LispObj *obj)
{
    mpi *bigi, iop;

    switch (OBJECT_TYPE(obj)) {
	case LispFixnum_t:
	    switch (RTYPE(real)) {
		case N_FIXNUM:
		    RFI(real) &= OFI(obj);
		    break;
		case N_BIGNUM:
		    mpi_init(&iop);
		    mpi_seti(&iop, OFI(obj));
		    mpi_and(RBI(real), RBI(real), &iop);
		    mpi_clear(&iop);
		    rbi_canonicalize(real);
		    break;
	    }
	    break;
	case LispInteger_t:
	    switch (RTYPE(real)) {
		case N_FIXNUM:
		    RFI(real) &= OII(obj);
		    break;
		case N_BIGNUM:
		    mpi_init(&iop);
		    mpi_seti(&iop, OII(obj));
		    mpi_and(RBI(real), RBI(real), &iop);
		    mpi_clear(&iop);
		    rbi_canonicalize(real);
		    break;
	    }
	    break;
	case LispBignum_t:
	    switch (RTYPE(real)) {
		case N_FIXNUM:
		    bigi = XALLOC(mpi);
		    mpi_init(bigi);
		    mpi_seti(bigi, RFI(real));
		    mpi_and(bigi, bigi, OBI(obj));
		    RTYPE(real) = N_BIGNUM;
		    RBI(real) = bigi;
		    rbi_canonicalize(real);
		    break;
		case N_BIGNUM:
		    mpi_and(RBI(real), RBI(real), OBI(obj));
		    rbi_canonicalize(real);
		    break;
	    }
	    break;
	default:
	    fatal_object_error(obj, NOT_AN_INTEGER);
	    break;
    }
}


/************************************************************************
 * EQV
 ************************************************************************/
static void
eqv_real_object(n_real *real, LispObj *obj)
{
    mpi *bigi, iop;

    switch (OBJECT_TYPE(obj)) {
	case LispFixnum_t:
	    switch (RTYPE(real)) {
		case N_FIXNUM:
		    RFI(real) ^= ~OFI(obj);
		    break;
		case N_BIGNUM:
		    mpi_init(&iop);
		    mpi_seti(&iop, OFI(obj));
		    mpi_com(&iop, &iop);
		    mpi_xor(RBI(real), RBI(real), &iop);
		    mpi_clear(&iop);
		    rbi_canonicalize(real);
		    break;
	    }
	    break;
	case LispInteger_t:
	    switch (RTYPE(real)) {
		case N_FIXNUM:
		    RFI(real) ^= ~OII(obj);
		    break;
		case N_BIGNUM:
		    mpi_init(&iop);
		    mpi_seti(&iop, OII(obj));
		    mpi_com(&iop, &iop);
		    mpi_xor(RBI(real), RBI(real), &iop);
		    mpi_clear(&iop);
		    rbi_canonicalize(real);
		    break;
	    }
	    break;
	case LispBignum_t:
	    switch (RTYPE(real)) {
		case N_FIXNUM:
		    bigi = XALLOC(mpi);
		    mpi_init(bigi);
		    mpi_seti(bigi, RFI(real));
		    mpi_com(bigi, bigi);
		    mpi_xor(bigi, bigi, OBI(obj));
		    RTYPE(real) = N_BIGNUM;
		    RBI(real) = bigi;
		    rbi_canonicalize(real);
		    break;
		case N_BIGNUM:
		    mpi_com(RBI(real), RBI(real));
		    mpi_xor(RBI(real), RBI(real), OBI(obj));
		    rbi_canonicalize(real);
		    break;
	    }
	    break;
	default:
	    fatal_object_error(obj, NOT_AN_INTEGER);
	    break;
    }
}


/************************************************************************
 * IOR
 ************************************************************************/
static void
ior_real_object(n_real *real, LispObj *obj)
{
    mpi *bigi, iop;

    switch (OBJECT_TYPE(obj)) {
	case LispFixnum_t:
	    switch (RTYPE(real)) {
		case N_FIXNUM:
		    RFI(real) |= OFI(obj);
		    break;
		case N_BIGNUM:
		    mpi_init(&iop);
		    mpi_seti(&iop, OFI(obj));
		    mpi_ior(RBI(real), RBI(real), &iop);
		    mpi_clear(&iop);
		    rbi_canonicalize(real);
		    break;
	    }
	    break;
	case LispInteger_t:
	    switch (RTYPE(real)) {
		case N_FIXNUM:
		    RFI(real) |= OII(obj);
		    break;
		case N_BIGNUM:
		    mpi_init(&iop);
		    mpi_seti(&iop, OII(obj));
		    mpi_ior(RBI(real), RBI(real), &iop);
		    mpi_clear(&iop);
		    rbi_canonicalize(real);
		    break;
	    }
	    break;
	case LispBignum_t:
	    switch (RTYPE(real)) {
		case N_FIXNUM:
		    bigi = XALLOC(mpi);
		    mpi_init(bigi);
		    mpi_seti(bigi, RFI(real));
		    mpi_ior(bigi, bigi, OBI(obj));
		    RTYPE(real) = N_BIGNUM;
		    RBI(real) = bigi;
		    rbi_canonicalize(real);
		    break;
		case N_BIGNUM:
		    mpi_ior(RBI(real), RBI(real), OBI(obj));
		    rbi_canonicalize(real);
		    break;
	    }
	    break;
	default:
	    fatal_object_error(obj, NOT_AN_INTEGER);
	    break;
    }
}


/************************************************************************
 * NOT
 ************************************************************************/
static void
not_real(n_real *real)
{
    if (RTYPE(real) == N_FIXNUM)
	RFI(real) = ~RFI(real);
    else {
	mpi_com(RBI(real), RBI(real));
	rbi_canonicalize(real);
    }
}

/************************************************************************
 * XOR
 ************************************************************************/
static void
xor_real_object(n_real *real, LispObj *obj)
{
    mpi *bigi, iop;

    switch (OBJECT_TYPE(obj)) {
	case LispFixnum_t:
	    switch (RTYPE(real)) {
		case N_FIXNUM:
		    RFI(real) ^= OFI(obj);
		    break;
		case N_BIGNUM:
		    mpi_init(&iop);
		    mpi_seti(&iop, OFI(obj));
		    mpi_xor(RBI(real), RBI(real), &iop);
		    mpi_clear(&iop);
		    rbi_canonicalize(real);
		    break;
	    }
	    break;
	case LispInteger_t:
	    switch (RTYPE(real)) {
		case N_FIXNUM:
		    RFI(real) ^= OII(obj);
		    break;
		case N_BIGNUM:
		    mpi_init(&iop);
		    mpi_seti(&iop, OII(obj));
		    mpi_xor(RBI(real), RBI(real), &iop);
		    mpi_clear(&iop);
		    rbi_canonicalize(real);
		    break;
	    }
	    break;
	case LispBignum_t:
	    switch (RTYPE(real)) {
		case N_FIXNUM:
		    bigi = XALLOC(mpi);
		    mpi_init(bigi);
		    mpi_seti(bigi, RFI(real));
		    mpi_xor(bigi, bigi, OBI(obj));
		    RTYPE(real) = N_BIGNUM;
		    RBI(real) = bigi;
		    rbi_canonicalize(real);
		    break;
		case N_BIGNUM:
		    mpi_xor(RBI(real), RBI(real), OBI(obj));
		    rbi_canonicalize(real);
		    break;
	    }
	    break;
	default:
	    fatal_object_error(obj, NOT_AN_INTEGER);
	    break;
    }
}


/************************************************************************
 * DIVIDE
 ************************************************************************/
static void
divide_number_object(n_number *num, LispObj *obj, int fun, int flo)
{
    switch (OBJECT_TYPE(obj)) {
	case LispFixnum_t:
	    switch (NRTYPE(num)) {
		case N_FIXNUM:
		    ndivide_fi_fi(num, OFI(obj), fun, flo);
		    break;
		case N_BIGNUM:
		    ndivide_xi_xi(num, obj, fun, flo);
		    break;
		case N_FLONUM:
		    ndivide_flonum(num, NRFF(num), (double)OFI(obj), fun, flo);
		    break;
		case N_FIXRATIO:
		case N_BIGRATIO:
		    ndivide_xr_xi(num, obj, fun, flo);
		    break;
	    }
	    break;
	case LispInteger_t:
	    switch (NRTYPE(num)) {
		case N_FIXNUM:
		    ndivide_fi_fi(num, OII(obj), fun, flo);
		    break;
		case N_BIGNUM:
		    ndivide_xi_xi(num, obj, fun, flo);
		    break;
		case N_FLONUM:
		    ndivide_flonum(num, NRFF(num), (double)OII(obj), fun, flo);
		    break;
		case N_FIXRATIO:
		case N_BIGRATIO:
		    ndivide_xr_xi(num, obj, fun, flo);
		    break;
	    }
	    break;
	case LispBignum_t:
	    switch (NRTYPE(num)) {
		case N_FIXNUM:
		case N_BIGNUM:
		    ndivide_xi_xi(num, obj, fun, flo);
		    break;
		case N_FLONUM:
		    ndivide_flonum(num, NRFF(num), bi_getd(OBI(obj)),
				   fun, flo);
		    break;
		case N_FIXRATIO:
		case N_BIGRATIO:
		    ndivide_xr_xi(num, obj, fun, flo);
		    break;
	    }
	    break;
	case LispDFloat_t:
	    switch (NRTYPE(num)) {
		case N_FIXNUM:
		    ndivide_flonum(num, (double)NRFI(num), ODF(obj),
				   fun, flo);
		    break;
		case N_BIGNUM:
		    ndivide_flonum(num, bi_getd(NRBI(num)), ODF(obj),
				   fun, flo);
		    break;
		case N_FLONUM:
		    ndivide_flonum(num, NRFF(num), ODF(obj), fun, flo);
		    break;
		case N_FIXRATIO:
		    ndivide_flonum(num,
				   (double)NRFRN(num) / (double)NRFRD(num),
				   ODF(obj), fun, flo);
		    break;
		case N_BIGRATIO:
		    ndivide_flonum(num, br_getd(NRBR(num)), ODF(obj),
				   fun, flo);
		    break;
	    }
	    break;
	case LispRatio_t:
	    switch (NRTYPE(num)) {
		case N_FIXNUM:
		case N_BIGNUM:
		    ndivide_xi_xr(num, obj, fun, flo);
		    break;
		case N_FLONUM:
		    ndivide_flonum(num, NRFF(num),
				   (double)OFRN(obj) / (double)OFRD(obj),
				   fun, flo);
		    break;
		case N_FIXRATIO:
		case N_BIGRATIO:
		    ndivide_xr_xr(num, obj, fun, flo);
		    break;
	    }
	    break;
	case LispBigratio_t:
	    switch (NRTYPE(num)) {
		case N_FIXNUM:
		case N_BIGNUM:
		    ndivide_xi_xr(num, obj, fun, flo);
		    break;
		case N_FLONUM:
		    ndivide_flonum(num, NRFF(num), br_getd(OBR(obj)),
				   fun, flo);
		    break;
		case N_FIXRATIO:
		case N_BIGRATIO:
		    ndivide_xr_xr(num, obj, fun, flo);
		    break;
	    }
	    break;
	default:
	    fatal_object_error(obj, NOT_A_REAL_NUMBER);
	    break;
    }
}


/************************************************************************
 * COMPARE
 ************************************************************************/
static int
cmp_real_real(n_real *op1, n_real *op2)
{
    switch (RTYPE(op1)) {
	case N_FIXNUM:
	    switch (RTYPE(op2)) {
		case N_FIXNUM:
		    return (cmp_fi_fi(RFI(op1), RFI(op2)));
		case N_BIGNUM:
		    return (cmp_fi_bi(RFI(op1), RBI(op2)));
		case N_FLONUM:
		    return (cmp_flonum((double)RFI(op1), RFF(op2)));
		case N_FIXRATIO:
		    return (cmp_fi_fr(RFI(op1), RFRN(op2), RFRD(op2)));
		case N_BIGRATIO:
		    return (cmp_fi_br(RFI(op1), RBR(op2)));
	    }
	    break;
	case N_BIGNUM:
	    switch (RTYPE(op2)) {
		case N_FIXNUM:
		    return (cmp_bi_fi(RBI(op1), RFI(op2)));
		case N_BIGNUM:
		    return (cmp_bi_bi(RBI(op1), RBI(op2)));
		case N_FLONUM:
		    return (cmp_flonum(bi_getd(RBI(op1)), RFF(op2)));
		case N_FIXRATIO:
		    return (cmp_bi_fr(RBI(op1), RFRN(op2), RFRD(op2)));
		case N_BIGRATIO:
		    return (cmp_bi_br(RBI(op1), RBR(op2)));
	    }
	    break;
	case N_FLONUM:
	    switch (RTYPE(op2)) {
		case N_FIXNUM:
		    return (cmp_flonum(RFF(op1), (double)RFI(op2)));
		case N_BIGNUM:
		    return (cmp_flonum(RFF(op1), bi_getd(RBI(op2))));
		case N_FLONUM:
		    return (cmp_flonum(RFF(op1), RFF(op2)));
		case N_FIXRATIO:
		    return (cmp_flonum(RFF(op1),
				       (double)RFRN(op2) / (double)RFRD(op2)));
		case N_BIGRATIO:
		    return (cmp_flonum(RFF(op1), br_getd(RBR(op2))));
	    }
	    break;
	case N_FIXRATIO:
	    switch (RTYPE(op2)) {
		case N_FIXNUM:
		    return (cmp_fr_fi(RFRN(op1), RFRD(op1), RFI(op2)));
		case N_BIGNUM:
		    return (cmp_fr_bi(RFRN(op1), RFRD(op1), RBI(op2)));
		case N_FLONUM:
		    return (cmp_flonum((double)RFRN(op1) / (double)RFRD(op1),
				       RFF(op2)));
		case N_FIXRATIO:
		    return (cmp_fr_fr(RFRN(op1), RFRD(op1),
				      RFRN(op2), RFRD(op2)));
		case N_BIGRATIO:
		    return (cmp_fr_br(RFRN(op1), RFRD(op1), RBR(op2)));
	    }
	    break;
	case N_BIGRATIO:
	    switch (RTYPE(op2)) {
		case N_FIXNUM:
		    return (cmp_br_fi(RBR(op1), RFI(op2)));
		case N_BIGNUM:
		    return (cmp_br_bi(RBR(op1), RBI(op2)));
		case N_FLONUM:
		    return (cmp_flonum(br_getd(RBR(op1)), RFF(op2)));
		case N_FIXRATIO:
		    return (cmp_br_fr(RBR(op1), RFRN(op2), RFRD(op2)));
		case N_BIGRATIO:
		    return (cmp_br_br(RBR(op1), RBR(op2)));
	    }
    }

    return (0);
}

static int
cmp_real_object(n_real *op1, LispObj *op2)
{
    switch (OBJECT_TYPE(op2)) {
	case LispFixnum_t:
	    switch (RTYPE(op1)) {
		case N_FIXNUM:
		    return (cmp_fi_fi(RFI(op1), OFI(op2)));
		case N_BIGNUM:
		    return (cmp_bi_fi(RBI(op1), OFI(op2)));
		case N_FLONUM:
		    return (cmp_flonum(RFF(op1), (double)OFI(op2)));
		case N_FIXRATIO:
		    return (cmp_fr_fi(RFRD(op1), RFRN(op1), OFI(op2)));
		case N_BIGRATIO:
		    return (cmp_br_fi(RBR(op1), OFI(op2)));
	    }
	    break;
	case LispInteger_t:
	    switch (RTYPE(op1)) {
		case N_FIXNUM:
		    return (cmp_fi_fi(RFI(op1), OII(op2)));
		case N_BIGNUM:
		    return (cmp_bi_fi(RBI(op1), OII(op2)));
		case N_FLONUM:
		    return (cmp_flonum(RFF(op1), (double)OII(op2)));
		case N_FIXRATIO:
		    return (cmp_fr_fi(RFRD(op1), RFRN(op1), OII(op2)));
		case N_BIGRATIO:
		    return (cmp_br_fi(RBR(op1), OII(op2)));
	    }
	    break;
	case LispBignum_t:
	    switch (RTYPE(op1)) {
		case N_FIXNUM:
		    return (cmp_fi_bi(RFI(op1), OBI(op2)));
		case N_BIGNUM:
		    return (cmp_bi_bi(RBI(op1), OBI(op2)));
		case N_FLONUM:
		    return (cmp_flonum(RFF(op1), bi_getd(OBI(op2))));
		case N_FIXRATIO:
		    return (cmp_fr_bi(RFRD(op1), RFRN(op1), OBI(op2)));
		case N_BIGRATIO:
		    return (cmp_br_bi(RBR(op1), OBI(op2)));
	    }
	    break;
	case LispDFloat_t:
	    switch (RTYPE(op1)) {
		case N_FIXNUM:
		    return (cmp_flonum((double)RFI(op1), ODF(op2)));
		case N_BIGNUM:
		    return (cmp_flonum(bi_getd(RBI(op1)), ODF(op2)));
		case N_FLONUM:
		    return (cmp_flonum(RFF(op1), ODF(op2)));
		case N_FIXRATIO:
		    return (cmp_flonum((double)RFRN(op1) / (double)RFRD(op1),
				       ODF(op2)));
		case N_BIGRATIO:
		    return (cmp_flonum(br_getd(RBR(op1)), ODF(op2)));
	    }
	    break;
	case LispRatio_t:
	    switch (RTYPE(op1)) {
		case N_FIXNUM:
		    return (cmp_fi_fr(RFI(op1), OFRN(op2), OFRD(op2)));
		case N_BIGNUM:
		    return (cmp_bi_fr(RBI(op1), OFRN(op2), OFRD(op2)));
		case N_FLONUM:
		    return (cmp_flonum(RFF(op1),
				       (double)OFRN(op2) / (double)OFRD(op2)));
		case N_FIXRATIO:
		    return (cmp_fr_fr(RFRN(op1), RFRD(op1),
				      OFRN(op2), OFRD(op2)));
		case N_BIGRATIO:
		    return (cmp_br_fr(RBR(op1), OFRN(op2), OFRD(op2)));
	    }
	    break;
	case LispBigratio_t:
	    switch (RTYPE(op1)) {
		case N_FIXNUM:
		    return (cmp_fi_br(RFI(op1), OBR(op2)));
		case N_BIGNUM:
		    return (cmp_bi_br(RBI(op1), OBR(op2)));
		case N_FLONUM:
		    return (cmp_flonum(RFF(op1), br_getd(OBR(op2))));
		case N_FIXRATIO:
		    return (cmp_fr_br(RFRN(op1), RFRD(op1), OBR(op2)));
		case N_BIGRATIO:
		    return (cmp_br_br(RBR(op1), OBR(op2)));
	    }
	    break;
	default:
	    fatal_object_error(op2, NOT_A_REAL_NUMBER);
	    break;
    }

    return (0);
}

#if 0		/* not used */
static int
cmp_number_object(n_number *op1, LispObj *op2)
{
    if (op1->complex) {
	if (OBJECT_TYPE(op2) == LispComplex_t) {
	    if (cmp_real_object(NREAL(op1), OCXR(op2)) == 0)
		return (cmp_real_object(NIMAG(op1), OCXI(op2)));
	    return (1);
	}
	else if (cmp_real_real(NIMAG(op1), &zero) == 0)
	    return (cmp_real_object(NREAL(op1), op2));
	else
	    return (1);
    }
    else {
	switch (OBJECT_TYPE(op2)) {
	    case LispFixnum_t:
		switch (NRTYPE(op1)) {
		    case N_FIXNUM:
			return (cmp_fi_fi(NRFI(op1), OFI(op2)));
		    case N_BIGNUM:
			return (cmp_bi_fi(NRBI(op1), OFI(op2)));
		    case N_FLONUM:
			return (cmp_flonum(NRFF(op1), (double)OFI(op2)));
		    case N_FIXRATIO:
			return (cmp_fr_fi(NRFRD(op1), NRFRN(op1), OFI(op2)));
		    case N_BIGRATIO:
			return (cmp_br_fi(NRBR(op1), OFI(op2)));
		}
		break;
	    case LispInteger_t:
		switch (NRTYPE(op1)) {
		    case N_FIXNUM:
			return (cmp_fi_fi(NRFI(op1), OII(op2)));
		    case N_BIGNUM:
			return (cmp_bi_fi(NRBI(op1), OII(op2)));
		    case N_FLONUM:
			return (cmp_flonum(NRFF(op1), (double)OII(op2)));
		    case N_FIXRATIO:
			return (cmp_fr_fi(NRFRD(op1), NRFRN(op1), OII(op2)));
		    case N_BIGRATIO:
			return (cmp_br_fi(NRBR(op1), OII(op2)));
		}
		break;
	    case LispBignum_t:
		switch (NRTYPE(op1)) {
		    case N_FIXNUM:
			return (cmp_fi_bi(NRFI(op1), OBI(op2)));
		    case N_BIGNUM:
			return (cmp_bi_bi(NRBI(op1), OBI(op2)));
		    case N_FLONUM:
			return (cmp_flonum(NRFF(op1), bi_getd(OBI(op2))));
		    case N_FIXRATIO:
			return (cmp_fr_bi(NRFRD(op1), NRFRN(op1), OBI(op2)));
		    case N_BIGRATIO:
			return (cmp_br_bi(NRBR(op1), OBI(op2)));
		}
		break;
	    case LispDFloat_t:
		switch (NRTYPE(op1)) {
		    case N_FIXNUM:
			return (cmp_flonum((double)NRFI(op1), ODF(op2)));
		    case N_BIGNUM:
			return (cmp_flonum(bi_getd(NRBI(op1)), ODF(op2)));
		    case N_FLONUM:
			return (cmp_flonum(NRFF(op1), ODF(op2)));
		    case N_FIXRATIO:
			return (cmp_flonum((double)NRFRN(op1) /
					   (double)NRFRD(op1),
					   ODF(op2)));
		    case N_BIGRATIO:
			return (cmp_flonum(br_getd(NRBR(op1)), ODF(op2)));
		}
		break;
	    case LispRatio_t:
		switch (NRTYPE(op1)) {
		    case N_FIXNUM:
			return (cmp_fi_fr(NRFI(op1), OFRN(op2), OFRD(op2)));
		    case N_BIGNUM:
			return (cmp_bi_fr(NRBI(op1), OFRN(op2), OFRD(op2)));
		    case N_FLONUM:
			return (cmp_flonum(NRFF(op1),
					   (double)OFRN(op2) / (double)OFRD(op2)));
		    case N_FIXRATIO:
			return (cmp_fr_fr(NRFRN(op1), NRFRD(op1),
					  OFRN(op2), OFRD(op2)));
		    case N_BIGRATIO:
			return (cmp_br_fr(NRBR(op1), OFRN(op2), OFRD(op2)));
		}
		break;
	    case LispBigratio_t:
		switch (NRTYPE(op1)) {
		    case N_FIXNUM:
			return (cmp_fi_br(NRFI(op1), OBR(op2)));
		    case N_BIGNUM:
			return (cmp_bi_br(NRBI(op1), OBR(op2)));
		    case N_FLONUM:
			return (cmp_flonum(NRFF(op1), br_getd(OBR(op2))));
		    case N_FIXRATIO:
			return (cmp_fr_br(NRFRN(op1), NRFRD(op1), OBR(op2)));
		    case N_BIGRATIO:
			return (cmp_br_br(NRBR(op1), OBR(op2)));
		}
		break;
	    case LispComplex_t:
		if (cmp_real_object(&zero, OCXI(op2)) == 0)
		    return (cmp_real_object(NREAL(op1), OCXR(op2)));
		return (1);
	    default:
		fatal_object_error(op2, NOT_A_NUMBER);
		break;
	}
    }

    return (0);
}
#endif

static int
cmp_object_object(LispObj *op1, LispObj *op2, int real)
{
    if (OBJECT_TYPE(op1) == LispComplex_t) {
	if (real)
	    fatal_object_error(op1, NOT_A_REAL_NUMBER);
	if (OBJECT_TYPE(op2) == LispComplex_t)
	    return (cmp_cx_cx(op1, op2));
	else if (cmp_real_object(&zero, OCXI(op1)) == 0)
	    return (cmp_object_object(OCXR(op1), op2, real));
	return (1);
    }
    else if (OBJECT_TYPE(op2) == LispComplex_t) {
	if (real)
	    fatal_object_error(op1, NOT_A_REAL_NUMBER);
	if (cmp_real_object(&zero, OCXI(op2)) == 0)
	    return (cmp_object_object(op1, OCXR(op2), real));
	return (1);
    }
    else {
	switch (OBJECT_TYPE(op1)) {
	    case LispFixnum_t:
		switch (OBJECT_TYPE(op2)) {
		    case LispFixnum_t:
			return (cmp_fi_fi(OFI(op1), OFI(op2)));
		    case LispInteger_t:
			return (cmp_fi_fi(OFI(op1), OII(op2)));
		    case LispBignum_t:
			return (cmp_fi_bi(OFI(op1), OBI(op2)));
		    case LispDFloat_t:
			return (cmp_flonum((double)OFI(op1), ODF(op2)));
		    case LispRatio_t:
			return (cmp_fi_fr(OFI(op1),
					  OFRN(op2), OFRD(op2)));
		    case LispBigratio_t:
			return (cmp_fi_br(OFI(op1), OBR(op2)));
		    default:
			break;
		}
		break;
	    case LispInteger_t:
		switch (OBJECT_TYPE(op2)) {
		    case LispFixnum_t:
			return (cmp_fi_fi(OII(op1), OFI(op2)));
		    case LispInteger_t:
			return (cmp_fi_fi(OII(op1), OII(op2)));
		    case LispBignum_t:
			return (cmp_fi_bi(OII(op1), OBI(op2)));
		    case LispDFloat_t:
			return (cmp_flonum((double)OII(op1), ODF(op2)));
		    case LispRatio_t:
			return (cmp_fi_fr(OII(op1),
					  OFRN(op2), OFRD(op2)));
		    case LispBigratio_t:
			return (cmp_fi_br(OII(op1), OBR(op2)));
		    default:
			break;
		}
		break;
	    case LispBignum_t:
		switch (OBJECT_TYPE(op2)) {
		    case LispFixnum_t:
			return (cmp_bi_fi(OBI(op1), OFI(op2)));
		    case LispInteger_t:
			return (cmp_bi_fi(OBI(op1), OII(op2)));
		    case LispBignum_t:
			return (cmp_bi_bi(OBI(op1), OBI(op2)));
		    case LispDFloat_t:
			return (cmp_flonum(bi_getd(OBI(op1)), ODF(op2)));
		    case LispRatio_t:
			return (cmp_bi_fr(OBI(op1),
					  OFRN(op2), OFRD(op2)));
		    case LispBigratio_t:
			return (cmp_bi_br(OBI(op1), OBR(op2)));
		    default:
			break;
		}
		break;
	    case LispDFloat_t:
		switch (OBJECT_TYPE(op2)) {
		    case LispFixnum_t:
			return (cmp_flonum(ODF(op1), (double)OFI(op2)));
		    case LispInteger_t:
			return (cmp_flonum(ODF(op1), (double)OII(op2)));
		    case LispBignum_t:
			return (cmp_flonum(ODF(op1), bi_getd(OBI(op2))));
		    case LispDFloat_t:
			return (cmp_flonum(ODF(op1), ODF(op2)));
			break;
		    case LispRatio_t:
			return (cmp_flonum(ODF(op1),
					   (double)OFRN(op2) /
					   (double)OFRD(op2)));
		    case LispBigratio_t:
			return (cmp_flonum(ODF(op1), br_getd(OBR(op2))));
		    default:
			break;
		}
		break;
	    case LispRatio_t:
		switch (OBJECT_TYPE(op2)) {
		    case LispFixnum_t:
			return (cmp_fr_fi(OFRN(op1), OFRD(op1), OFI(op2)));
		    case LispInteger_t:
			return (cmp_fr_fi(OFRN(op1), OFRD(op1), OII(op2)));
		    case LispBignum_t:
			return (cmp_fr_bi(OFRN(op1), OFRD(op1), OBI(op2)));
		    case LispDFloat_t:
			return (cmp_flonum((double)OFRN(op1) /
					   (double)OFRD(op1),
					   ODF(op2)));
		    case LispRatio_t:
			return (cmp_fr_fr(OFRN(op1), OFRD(op1),
					  OFRN(op2), OFRD(op2)));
		    case LispBigratio_t:
			return (cmp_fr_br(OFRN(op1), OFRD(op1), OBR(op2)));
		    default:
			break;
		}
		break;
	    case LispBigratio_t:
		switch (OBJECT_TYPE(op2)) {
		    case LispFixnum_t:
			return (cmp_br_fi(OBR(op1), OFI(op2)));
		    case LispInteger_t:
			return (cmp_br_fi(OBR(op1), OII(op2)));
		    case LispBignum_t:
			return (cmp_br_bi(OBR(op1), OBI(op2)));
		    case LispDFloat_t:
			return (cmp_flonum(br_getd(OBR(op1)), ODF(op2)));
		    case LispRatio_t:
			return (cmp_br_fr(OBR(op1), OFRN(op2), OFRD(op2)));
		    case LispBigratio_t:
			return (cmp_br_br(OBR(op1), OBR(op2)));
		    default:
			break;
		}
		break;
	    default:
		fatal_object_error(op1, NOT_A_NUMBER);
		break;
	}
    }

    fatal_object_error(op2, NOT_A_NUMBER);
    return (0);
}


/************************************************************************
 * FIXNUM
 ************************************************************************/
/*
 * check if op1 + op2 will overflow
 */
static INLINE int
fi_fi_add_overflow(long op1, long op2)
{
    long op = op1 + op2;

    return (op1 > 0 ? op2 > op : op2 < op);
}

/*
 * check if op1 - op2 will overflow
 */
static INLINE int
fi_fi_sub_overflow(long op1, long op2)
{
    long op = op1 - op2;

    return (((op1 < 0) ^ (op2 < 0)) && ((op < 0) ^ (op1 < 0)));
}

/*
 * check if op1 * op2 will overflow
 */
static INLINE int
fi_fi_mul_overflow(long op1, long op2)
{
#ifndef LONG64
    double op = (double)op1 * (double)op2;

    return (op > 2147483647.0 || op < -2147483648.0);
#else
    int shift, sign;
    long mask;

    if (op1 == 0 || op1 == 1 || op2 == 0 || op2 == 1)
	return (0);

    if (op1 == MINSLONG || op2 == MINSLONG)
	return (1);

    sign = (op1 < 0) ^ (op2 < 0);

    if (op1 < 0)
	op1 = -op1;
    if (op2 < 0)
	op2 = -op2;

    for (shift = 0, mask = FI_MASK; shift < LONGSBITS; shift++, mask >>= 1)
	if (op1 & mask)
	    break;
    ++shift;
    for (mask = FI_MASK; shift < LONGSBITS; shift++, mask >>= 1)
	if (op2 & mask)
	    break;

    return (shift < LONGSBITS);
#endif
}


/************************************************************************
 * BIGNUM
 ************************************************************************/
static void
rbi_canonicalize(n_real *real)
{
    if (mpi_fiti(RBI(real))) {
	long fi = mpi_geti(RBI(real));

	RTYPE(real) = N_FIXNUM;
	mpi_clear(RBI(real));
	XFREE(RBI(real));
	RFI(real) = fi;
    }
}


/************************************************************************
 * RATIO
 ************************************************************************/
static void
rfr_canonicalize(n_real *real)
{
    long num, numerator, den, denominator, rest;

    num = numerator = RFRN(real);
    den = denominator = RFRD(real);
    if (denominator == 0)
	fatal_error(DIVIDE_BY_ZERO);

    if (num == MINSLONG || den == MINSLONG) {
	mpr *bigratio = XALLOC(mpr);

	mpr_init(bigratio);
	mpr_seti(bigratio, num, den);
	RTYPE(real) = N_BIGRATIO;
	RBR(real) = bigratio;
	rbr_canonicalize(real);
	return;
    }

    if (num < 0)
	num = -num;
    else if (num == 0) {
	RFI(real) = 0;
	RTYPE(real) = N_FIXNUM;
	return;
    }
    for (;;) {
	if ((rest = den % num) == 0)
	    break;
	den = num;
	num = rest;
    }
    if (den != 1) {
	denominator /= num;
	numerator /= num;
    }
    if (denominator < 0) {
	numerator = -numerator;
	denominator = -denominator;
    }
    if (denominator == 1) {
	RTYPE(real) = N_FIXNUM;
	RFI(real) = numerator;
    }
    else {
	RFRN(real) = numerator;
	RFRD(real) = denominator;
    }
}

static void
rbr_canonicalize(n_real *real)
{
    int fitnum, fitden;
    long numerator, denominator;

    mpr_canonicalize(RBR(real));
    fitnum = mpi_fiti(RBRN(real));
    fitden = mpi_fiti(RBRD(real));
    if (fitnum && fitden) {
	numerator = mpi_geti(RBRN(real));
	denominator = mpi_geti(RBRD(real));
	mpr_clear(RBR(real));
	XFREE(RBR(real));
	if (numerator == 0) {
	    RFI(real) = 0;
	    RTYPE(real) = N_FIXNUM;
	}
	else if (denominator == 1) {
	    RTYPE(real) = N_FIXNUM;
	    RFI(real) = numerator;
	}
	else {
	    RTYPE(real) = N_FIXRATIO;
	    RFRN(real) = numerator;
	    RFRD(real) = denominator;
	}
    }
    else if (fitden) {
	denominator = mpi_geti(RBRD(real));
	if (denominator == 1) {
	    mpi *bigi = XALLOC(mpi);

	    mpi_init(bigi);
	    mpi_set(bigi, RBRN(real));
	    mpr_clear(RBR(real));
	    XFREE(RBR(real));
	    RTYPE(real) = N_BIGNUM;
	    RBI(real) = bigi;
	}
	else if (denominator == 0)
	    fatal_error(DIVIDE_BY_ZERO);
    }
}


/************************************************************************
 * COMPLEX
 ************************************************************************/
static void
ncx_canonicalize(n_number *num)
{
    if (NITYPE(num) == N_FIXNUM && NIFI(num) == 0)
	num->complex = 0;
}


/************************************************************************
 * DIVIDE
 ************************************************************************/
#define NDIVIDE_NOP	0
#define NDIVIDE_ADD	1
#define NDIVIDE_SUB	2
static void
ndivide_fi_fi(n_number *num, long div, int fun, int flo)
{
    long quo, rem;

    if (NRFI(num) == MINSLONG || div == MINSLONG) {
	LispObj integer;
	mpi *bignum = XALLOC(mpi);

	mpi_init(bignum);
	mpi_seti(bignum, NRFI(num));
	NRBI(num) = bignum;
	NRTYPE(num) = N_BIGNUM;
	integer.type = LispInteger_t;
	integer.data.integer = div;
	ndivide_xi_xi(num, &integer, fun, flo);
	return;
    }
    else {
	quo = NRFI(num) / div;
	rem = NRFI(num) % div;
    }

    switch (fun) {
	case NDIVIDE_CEIL:
	    if ((rem < 0 && div < 0) || (rem > 0 && div > 0)) {
		++quo;
		rem -= div;
	    }
	    break;
	case NDIVIDE_FLOOR:
	    if ((rem < 0 && div > 0) || (rem > 0 && div < 0)) {
		--quo;
		rem += div;
	    }
	    break;
	case NDIVIDE_ROUND:
	    if (div > 0) {
		if (rem > 0) {
		    if (rem >= (div + 1) / 2) {
			++quo;
			rem -= div;
		    }
		}
		else {
		    if (rem <= (-div - 1) / 2) {
			--quo;
			rem += div;
		    }
		}
	    }
	    else {
		if (rem > 0) {
		    if (rem >= (-div + 1) / 2) {
			--quo;
			rem += div;
		    }
		}
		else {
		    if (rem <= (div - 1) / 2) {
			++quo;
			rem -= div;
		    }
		}
	    }
	    break;
    }

    NITYPE(num) = N_FIXNUM;
    NIFI(num) = rem;
    if (flo) {
	NRTYPE(num) = N_FLONUM;
	NRFF(num) = (double)quo;
    }
    else
	NRFI(num) = quo;
}

static void
ndivide_xi_xi(n_number *num, LispObj *div, int fun, int flo)
{
    LispType type = OBJECT_TYPE(div);
    int state = NDIVIDE_NOP, dsign, rsign;
    mpi *quo, *rem;

    quo = XALLOC(mpi);
    mpi_init(quo);
    if (NRTYPE(num) == N_FIXNUM)
	mpi_seti(quo, NRFI(num));
    else
	mpi_set(quo, NRBI(num));

    rem = XALLOC(mpi);
    mpi_init(rem);

    switch (type) {
	case LispFixnum_t:
	    mpi_seti(rem, OFI(div));
	    break;
	case LispInteger_t:
	    mpi_seti(rem, OII(div));
	    break;
	default:
	    mpi_set(rem, OBI(div));
    }

    dsign = mpi_sgn(rem);

    mpi_divqr(quo, rem, quo, rem);
    rsign = mpi_sgn(rem);

    switch (fun) {
	case NDIVIDE_CEIL:
	    if ((rsign < 0 && dsign < 0) || (rsign > 0 && dsign > 0))
		state = NDIVIDE_ADD;
	    break;
	case NDIVIDE_FLOOR:
	    if ((rsign < 0 && dsign > 0) || (rsign > 0 && dsign < 0))
		state = NDIVIDE_SUB;
	    break;
	case NDIVIDE_ROUND: {
	    mpi test;

	    mpi_init(&test);
	    switch (type) {
		case LispFixnum_t:
		    mpi_seti(&test, OFI(div));
		    break;
		case LispInteger_t:
		    mpi_seti(&test, OII(div));
		    break;
		default:
		    mpi_set(&test, OBI(div));
	    }
	    if (dsign > 0) {
		if (rsign > 0) {
		    mpi_addi(&test, &test, 1);
		    mpi_divi(&test, &test, 2);
		    if (mpi_cmp(rem, &test) >= 0)
			state = NDIVIDE_ADD;
		}
		else {
		    mpi_neg(&test, &test);
		    mpi_subi(&test, &test, 1);
		    mpi_divi(&test, &test, 2);
		    if (mpi_cmp(rem, &test) <= 0)
			state = NDIVIDE_SUB;
		}
	    }
	    else {
		if (rsign > 0) {
		    mpi_neg(&test, &test);
		    mpi_addi(&test, &test, 1);
		    mpi_divi(&test, &test, 2);
		    if (mpi_cmp(rem, &test) >= 0)
			state = NDIVIDE_SUB;
		}
		else {
		    mpi_subi(&test, &test, 1);
		    mpi_divi(&test, &test, 2);
		    if (mpi_cmp(rem, &test) <= 0)
			state = NDIVIDE_ADD;
		}
	    }
	    mpi_clear(&test);
	}   break;
    }

    if (state == NDIVIDE_ADD) {
	mpi_addi(quo, quo, 1);
	switch (type) {
	    case LispFixnum_t:
		mpi_subi(rem, rem, OFI(div));
		break;
	    case LispInteger_t:
		mpi_subi(rem, rem, OII(div));
		break;
	    default:
		mpi_sub(rem, rem, OBI(div));
	}
    }
    else if (state == NDIVIDE_SUB) {
	mpi_subi(quo, quo, 1);
	switch (type) {
	    case LispFixnum_t:
		mpi_addi(rem, rem, OFI(div));
		break;
	    case LispInteger_t:
		mpi_addi(rem, rem, OII(div));
		break;
	    default:
		mpi_add(rem, rem, OBI(div));
	}
    }

    if (mpi_fiti(rem)) {
	NITYPE(num) = N_FIXNUM;
	NIFI(num) = mpi_geti(rem);
	mpi_clear(rem);
	XFREE(rem);
    }
    else {
	NITYPE(num) = N_BIGNUM;
	NIBI(num) = rem;
    }

    clear_real(NREAL(num));

    if (flo) {
	double dval = bi_getd(quo);

	mpi_clear(quo);
	XFREE(quo);
	NRTYPE(num) = N_FLONUM;
	NRFF(num) = dval;
    }
    else {
	NRTYPE(num) = N_BIGNUM;
	NRBI(num) = quo;
	rbi_canonicalize(NREAL(num));
    }
}

static void
ndivide_flonum(n_number *number, double num, double div, int fun, int flo)
{
    double quo, rem, modp, tmp;

    modp = modf(num / div, &quo);
    rem = num - quo * div;

    switch (fun) {
	case NDIVIDE_CEIL:
	    if ((rem < 0.0 && div < 0.0) || (rem > 0.0 && div > 0.0)) {
		quo += 1.0;
		rem -= div;
	    }
	    break;
	case NDIVIDE_FLOOR:
	    if ((rem < 0.0 && div > 0.0) || (rem > 0.0 && div < 0.0)) {
		quo -= 1.0;
		rem += div;
	    }
	    break;
	case NDIVIDE_ROUND:
	    if (fabs(modp) != 0.5 || modf(quo * 0.5, &tmp) != 0.0) {
		if (div > 0.0) {
		    if (rem > 0.0) {
			if (rem >= div * 0.5) {
			    quo += 1.0;
			    rem -= div;
			}
		    }
		    else {
			if (rem <= div * -0.5) {
			    quo -= 1.0;
			    rem += div;
			}
		    }
		}
		else {
		    if (rem > 0.0) {
			if (rem >= div * -0.5) {
			    quo -= 1.0;
			    rem += div;
			}
		    }
		    else {
			if (rem <= div * 0.5) {
			    quo += 1.0;
			    rem -= div;
			}
		    }
		}
	    }
	    break;
    }
    if (!finite(quo) || !finite(rem))
	fatal_error(FLOATING_POINT_OVERFLOW);

    NITYPE(number) = N_FLONUM;
    NIFF(number) = rem;

    clear_real(NREAL(number));

    if (flo) {
	NRTYPE(number) = N_FLONUM;
	NRFF(number) = quo;
    }
    else {
	if ((long)quo == quo) {
	    NRTYPE(number) = N_FIXNUM;
	    NRFI(number) = (long)quo;
	}
	else {
	    mpi *bigi = XALLOC(mpi);

	    mpi_init(bigi);
	    mpi_setd(bigi, quo);
	    NRBI(number) = bigi;
	    NRTYPE(number) = N_BIGNUM;
	}
    }
}

static void
ndivide_xi_xr(n_number *num, LispObj *div, int fun, int flo)
{
    int state = NDIVIDE_NOP, dsign, rsign;
    mpi *quo;
    mpr *rem;

    quo = XALLOC(mpi);
    mpi_init(quo);
    if (NRTYPE(num) == N_FIXNUM)
	mpi_seti(quo, NRFI(num));
    else    
	mpi_set(quo, NRBI(num));

    rem = XALLOC(mpr);
    mpr_init(rem);

    if (XOBJECT_TYPE(div) == LispRatio_t)
	mpr_seti(rem, OFRN(div), OFRD(div));
    else
	mpr_set(rem, OBR(div));
    dsign = mpi_sgn(mpr_num(rem));
    mpi_mul(quo, quo, mpr_den(rem));

    mpi_divqr(quo, mpr_num(rem), quo, mpr_num(rem));
    mpr_canonicalize(rem);

    rsign = mpi_sgn(mpr_num(rem));
    if (mpr_fiti(rem)) {
	if (mpi_geti(mpr_den(rem)) == 1) {
	    NITYPE(num) = N_FIXNUM;
	    NIFI(num) = mpi_geti(mpr_num(rem));
	}
	else {
	    NITYPE(num) = N_FIXRATIO;
	    NIFRN(num) = mpi_geti(mpr_num(rem));
	    NIFRD(num) = mpi_geti(mpr_den(rem));
	}
	mpr_clear(rem);
	XFREE(rem);	
    }
    else {
	if (mpi_fiti(mpr_den(rem)) && mpi_geti(mpr_den(rem)) == 1) {
	    NITYPE(num) = N_BIGNUM;
	    NIBI(num) = mpr_num(rem);
	    mpi_clear(mpr_den(rem));
	    XFREE(rem);
	}
	else {
	    NITYPE(num) = N_BIGRATIO;
	    NIBR(num) = rem;
	}
    }

    switch (fun) {
	case NDIVIDE_CEIL:
	    if ((rsign < 0 && dsign < 0) || (rsign > 0 && dsign > 0))
		state = NDIVIDE_ADD;
	    break;
	case NDIVIDE_FLOOR:
	    if ((rsign < 0 && dsign > 0) || (rsign > 0 && dsign < 0))
		state = NDIVIDE_SUB;
	    break;
	case NDIVIDE_ROUND: {
	    n_real cmp;

	    set_real_object(&cmp, div);
	    div_real_real(&cmp, &two);
	    if (dsign > 0) {
		if (rsign > 0) {
		    if (cmp_real_real(NIMAG(num), &cmp) >= 0)
			state = NDIVIDE_ADD;
		}
		else {
		    neg_real(&cmp);
		    if (cmp_real_real(NIMAG(num), &cmp) <= 0)
			state = NDIVIDE_SUB;
		}
	    }
	    else {
		if (rsign > 0) {
		    neg_real(&cmp);
		    if (cmp_real_real(NIMAG(num), &cmp) >= 0)
			state = NDIVIDE_SUB;
		}
		else {
		    if (cmp_real_real(NIMAG(num), &cmp) <= 0)
			state = NDIVIDE_ADD;
		}
	    }
	    clear_real(&cmp);
	}   break;
    }

    if (state == NDIVIDE_ADD) {
	mpi_addi(quo, quo, 1);
	sub_real_object(NIMAG(num), div);
    }
    else if (state == NDIVIDE_SUB) {
	mpi_subi(quo, quo, 1);
	add_real_object(NIMAG(num), div);
    }

    clear_real(NREAL(num));

    if (flo) {
	double dval = bi_getd(quo);

	mpi_clear(quo);
	XFREE(quo);
	NRTYPE(num) = N_FLONUM;
	NRFF(num) = dval;
    }
    else {
	NRBI(num)  = quo;
	NRTYPE(num) = N_BIGNUM;
	rbi_canonicalize(NREAL(num));
    }
}

static void
ndivide_xr_xi(n_number *num, LispObj *div, int fun, int flo)
{
    LispType type = OBJECT_TYPE(div);
    int state = NDIVIDE_NOP, dsign, rsign;
    mpi *quo;
    mpr *rem;

    quo = XALLOC(mpi);
    mpi_init(quo);
    switch (type) {
	case LispFixnum_t:
	    dsign = OFI(div) < 0 ? -1 : OFI(div) > 0 ? 1 : 0;
	    mpi_seti(quo, OFI(div));
	    break;
	case LispInteger_t:
	    dsign = OII(div) < 0 ? -1 : OII(div) > 0 ? 1 : 0;
	    mpi_seti(quo, OII(div));
	    break;
	default:
	    dsign = mpi_sgn(OBI(div));
	    mpi_set(quo, OBI(div));
	    break;
    }

    rem = XALLOC(mpr);
    mpr_init(rem);
    if (NRTYPE(num) == N_FIXRATIO) {
	mpr_seti(rem, NRFRN(num), NRFRD(num));
	mpi_muli(quo, quo, NRFRD(num));
    }
    else {
	mpr_set(rem, NRBR(num));
	mpi_mul(quo, quo, NRBRD(num));
    }
    mpi_divqr(quo, mpr_num(rem), mpr_num(rem), quo);
    mpr_canonicalize(rem);

    rsign = mpi_sgn(mpr_num(rem));
    if (mpr_fiti(rem)) {
	NITYPE(num) = N_FIXRATIO;
	NIFRN(num) = mpi_geti(mpr_num(rem));
	NIFRD(num) = mpi_geti(mpr_den(rem));
	mpr_clear(rem);
	XFREE(rem);
    }
    else {
	NITYPE(num) = N_BIGRATIO;
	NIBR(num) = rem;
    }

    switch (fun) {
	case NDIVIDE_CEIL:
	    if ((rsign < 0 && dsign < 0) || (rsign > 0 && dsign > 0))
		state = NDIVIDE_ADD;
	    break;
	case NDIVIDE_FLOOR:
	    if ((rsign < 0 && dsign > 0) || (rsign > 0 && dsign < 0))
		state = NDIVIDE_SUB;
	    break;
	case NDIVIDE_ROUND: {
	    n_real cmp;

	    set_real_object(&cmp, div);
	    div_real_real(&cmp, &two);
	    if (dsign > 0) {
		if (rsign > 0) {
		    if (cmp_real_real(NIMAG(num), &cmp) >= 0)
			state = NDIVIDE_ADD;
		}
		else {
		    neg_real(&cmp);
		    if (cmp_real_real(NIMAG(num), &cmp) <= 0)
			state = NDIVIDE_SUB;
		}
	    }
	    else {
		if (rsign > 0) {
		    neg_real(&cmp);
		    if (cmp_real_real(NIMAG(num), &cmp) >= 0)
			state = NDIVIDE_SUB;
		}
		else {
		    if (cmp_real_real(NIMAG(num), &cmp) <= 0)
			state = NDIVIDE_ADD;
		}
	    }
	    clear_real(&cmp);
	}   break;
    }

    if (state == NDIVIDE_ADD) {
	mpi_addi(quo, quo, 1);
	sub_real_object(NIMAG(num), div);
    }
    else if (state == NDIVIDE_SUB) {
	mpi_subi(quo, quo, 1);
	add_real_object(NIMAG(num), div);
    }

    clear_real(NREAL(num));

    if (flo) {
	double dval = bi_getd(quo);

	mpi_clear(quo);
	XFREE(quo);
	NRTYPE(num) = N_FLONUM;
	NRFF(num) = dval;
    }
    else {
	NRBI(num) = quo;
	NRTYPE(num) = N_BIGNUM;
	rbi_canonicalize(NREAL(num));
    }
}

static void
ndivide_xr_xr(n_number *num, LispObj *div, int fun, int flo)
{
    int state = NDIVIDE_NOP, dsign, rsign, modp;
    mpr *bigr;
    mpi *bigi;

    bigr = XALLOC(mpr);
    mpr_init(bigr);
    if (NRTYPE(num) == N_FIXRATIO)
	mpr_seti(bigr, NRFRN(num), NRFRD(num));
    else
	mpr_set(bigr, NRBR(num));

    NITYPE(num) = N_BIGRATIO;
    NIBR(num) = bigr;

    if (OBJECT_TYPE(div) == LispRatio_t) {
	dsign = OFRN(div) < 0 ? -1 : OFRN(div) > 0 ? 1 : 0;
	mpi_muli(mpr_num(bigr), mpr_num(bigr), OFRD(div));
	mpi_muli(mpr_den(bigr), mpr_den(bigr), OFRN(div));
    }
    else {
	dsign = mpi_sgn(OBRN(div));
	mpr_div(bigr, bigr, OBR(div));
    }
    modp = mpi_fiti(mpr_den(bigr)) && mpi_geti(mpr_den(bigr)) == 2;

    bigi = XALLOC(mpi);
    mpi_init(bigi);
    mpi_divqr(bigi, mpr_num(bigr), mpr_num(bigr), mpr_den(bigr));

    if (OBJECT_TYPE(div) == LispRatio_t)
	mpi_seti(mpr_den(bigr), OFRD(div));
    else
	mpi_set(mpr_den(bigr), OBRD(div));
    if (NRTYPE(num) == N_FIXRATIO)
	mpi_muli(mpr_den(bigr), mpr_den(bigr), NRFRD(num));
    else
	mpi_mul(mpr_den(bigr), mpr_den(bigr), NRBRD(num));

    clear_real(NREAL(num));
    NRTYPE(num) = N_BIGNUM;
    NRBI(num) = bigi;

    rbr_canonicalize(NIMAG(num));
    rsign = cmp_real_real(NIMAG(num), &zero);

    switch (fun) {
	case NDIVIDE_CEIL:
	    if ((rsign < 0 && dsign < 0) || (rsign > 0 && dsign > 0))
		state = NDIVIDE_ADD;
	    break;
	case NDIVIDE_FLOOR:
	    if ((rsign < 0 && dsign > 0) || (rsign > 0 && dsign < 0))
		state = NDIVIDE_SUB;
	    break;
	case NDIVIDE_ROUND:
	    if (!modp || (bigi->digs[0] & 1) == 1) {
		n_real cmp;

		set_real_object(&cmp, div);
		div_real_real(&cmp, &two);
		if (dsign > 0) {
		    if (rsign > 0) {
			if (cmp_real_real(NIMAG(num), &cmp) >= 0)
			    state = NDIVIDE_ADD;
		    }
		    else {
			neg_real(&cmp);
			if (cmp_real_real(NIMAG(num), &cmp) <= 0)
			    state = NDIVIDE_SUB;
		    }
		}
		else {
		    if (rsign > 0) {
			neg_real(&cmp);
			if (cmp_real_real(NIMAG(num), &cmp) >= 0)
			    state = NDIVIDE_SUB;
		    }
		    else {
			if (cmp_real_real(NIMAG(num), &cmp) <= 0)
			    state = NDIVIDE_ADD;
		    }
		}
		clear_real(&cmp);
	    }
	    break;
    }

    if (state == NDIVIDE_ADD) {
	add_real_real(NREAL(num), &one);
	sub_real_object(NIMAG(num), div);
    }
    else if (state == NDIVIDE_SUB) {
	sub_real_real(NREAL(num), &one);
	add_real_object(NIMAG(num), div);
    }

    if (NRTYPE(num) == N_BIGNUM) {
	if (flo) {
	    double dval = bi_getd(bigi);

	    mpi_clear(bigi);
	    XFREE(bigi);
	    NRTYPE(num) = N_FLONUM;
	    NRFF(num) = dval;
	}
	else
	    rbi_canonicalize(NREAL(num));
    }
    else if (flo) {
	NRTYPE(num) = N_FLONUM;
	NRFF(num) = (double)NRFI(num);
    }
}


/************************************************************************
 * REAL COMPLEX
 ************************************************************************/
static void
nadd_re_cx(n_number *num, LispObj *comp)
{
/*
	Ra+Rb Ib
 */
    /* Ra+Rb */
    add_real_object(NREAL(num), OCXR(comp));

    /* Ib */
    set_real_object(NIMAG(num), OCXI(comp));

    num->complex = 1;

    ncx_canonicalize(num);
}

static void
nsub_re_cx(n_number *num, LispObj *comp)
{
/*
	Ra-Rb -Ib
 */
    /* Ra-Rb */
    sub_real_object(NREAL(num), OCXR(comp));

    /* -Ib */
    NITYPE(num) = N_FIXNUM;
    NIFI(num) = -1;
    mul_real_object(NIMAG(num), OCXI(comp));

    num->complex = 1;

    ncx_canonicalize(num);
}

static void
nmul_re_cx(n_number *num, LispObj *comp)
{
/*
	Ra*Rb Ra*Ib
 */
    /* copy before change */
    set_real_real(NIMAG(num), NREAL(num));

    /* Ra*Rb */
    mul_real_object(NREAL(num), OCXR(comp));

    /* Ra*Ib */
    mul_real_object(NIMAG(num), OCXI(comp));

    num->complex = 1;

    ncx_canonicalize(num);
}

static void
ndiv_re_cx(n_number *num, LispObj *comp)
{
/*
	Ra*Rb        -Ib*Ra
	-----------  -----------
	Rb*Rb+Ib*Ib  Rb*Rb+Ib*Ib
 */
    n_real div, temp;

    /* Rb*Rb */
    set_real_object(&div, OCXR(comp));
    mul_real_object(&div, OCXR(comp));

    /* Ib*Ib */
    set_real_object(&temp, OCXI(comp));
    mul_real_object(&temp, OCXI(comp));

    /* Rb*Rb+Ib*Ib */
    add_real_real(&div, &temp);
    clear_real(&temp);

    /* -Ib*Ra */
    NITYPE(num) = N_FIXNUM;
    NIFI(num) = -1;
    mul_real_object(NIMAG(num), OCXI(comp));
    mul_real_real(NIMAG(num), NREAL(num));

    /* Ra*Rb */
    mul_real_object(NREAL(num), OCXR(comp));

    div_real_real(NREAL(num), &div);
    div_real_real(NIMAG(num), &div);
    clear_real(&div);

    num->complex = 1;

    ncx_canonicalize(num);
}


/************************************************************************
 * COMPLEX REAL
 ************************************************************************/
static void
nadd_cx_re(n_number *num, LispObj *re)
{
/*
	Ra+Rb Ia
 */
    add_real_object(NREAL(num), re);

    ncx_canonicalize(num);
}

static void
nsub_cx_re(n_number *num, LispObj *re)
{
/*
	Ra-Rb Ia
 */
    sub_real_object(NREAL(num), re);

    ncx_canonicalize(num);
}

static void
nmul_cx_re(n_number *num, LispObj *re)
{
/*
	Ra*Rb Ia*Rb
 */
    mul_real_object(NREAL(num), re);
    mul_real_object(NIMAG(num), re);

    ncx_canonicalize(num);
}

static void
ndiv_cx_re(n_number *num, LispObj *re)
{
/*
	Ra/Rb Ia/Rb
 */
    div_real_object(NREAL(num), re);
    div_real_object(NIMAG(num), re);

    ncx_canonicalize(num);
}


/************************************************************************
 * COMPLEX COMPLEX
 ************************************************************************/
static void
nadd_cx_cx(n_number *num, LispObj *comp)
{
/*
	Ra+Rb Ia+Ib
 */
    add_real_object(NREAL(num), OCXR(comp));
    add_real_object(NIMAG(num), OCXI(comp));

    ncx_canonicalize(num);
}

static void
nsub_cx_cx(n_number *num, LispObj *comp)
{
/*
	Ra-Rb Ia-Ib
 */
    sub_real_object(NREAL(num), OCXR(comp));
    sub_real_object(NIMAG(num), OCXI(comp));

    ncx_canonicalize(num);
}

static void
nmul_cx_cx(n_number *num, LispObj *comp)
{
/*
	Ra*Rb-Ia*Ib Ra*Ib+Ia*Rb
 */
    n_real IaIb, RaIb;

    set_real_real(&IaIb, NIMAG(num));
    mul_real_object(&IaIb, OCXI(comp));

    set_real_real(&RaIb, NREAL(num));
    mul_real_object(&RaIb, OCXI(comp));

    /* Ra*Rb-Ia*Ib */
    mul_real_object(NREAL(num), OCXR(comp));
    sub_real_real(NREAL(num), &IaIb);
    clear_real(&IaIb);

    /* Ra*Ib+Ia*Rb */
    mul_real_object(NIMAG(num), OCXR(comp));
    add_real_real(NIMAG(num), &RaIb);
    clear_real(&RaIb);

    ncx_canonicalize(num);
}

static void
ndiv_cx_cx(n_number *num, LispObj *comp)
{
/*
	Ra*Rb+Ia*Ib  Ia*Rb-Ib*Ra
	-----------  -----------
	Rb*Rb+Ib*Ib  Rb*Rb+Ib*Ib
 */
    n_real temp1, temp2;

    /* IaIb */
    set_real_real(&temp1, NIMAG(num));
    mul_real_object(&temp1, OCXI(comp));

    /* IbRa */
    set_real_real(&temp2, NREAL(num));
    mul_real_object(&temp2, OCXI(comp));

    /* Ra*Rb+Ia*Ib */
    mul_real_object(NREAL(num), OCXR(comp));
    add_real_real(NREAL(num), &temp1);
    clear_real(&temp1);

    /* Ia*Rb-Ib*Ra */
    mul_real_object(NIMAG(num), OCXR(comp));
    sub_real_real(NIMAG(num), &temp2);
    clear_real(&temp2);


    /* Rb*Rb */
    set_real_object(&temp1, OCXR(comp));
    mul_real_object(&temp1, OCXR(comp));

    /* Ib*Ib */
    set_real_object(&temp2, OCXI(comp));
    mul_real_object(&temp2, OCXI(comp));

    /* Rb*Rb+Ib*Ib */
    add_real_real(&temp1, &temp2);
    clear_real(&temp2);

    div_real_real(NREAL(num), &temp1);
    div_real_real(NIMAG(num), &temp1);
    clear_real(&temp1);

    ncx_canonicalize(num);
}

static int
cmp_cx_cx(LispObj *op1, LispObj *op2)
{
    int cmp;

    cmp = cmp_object_object(OCXR(op1), OCXR(op2), 1);
    if (cmp == 0)
	cmp = cmp_object_object(OCXI(op1), OCXI(op2), 1);

    return (cmp);
}


/************************************************************************
 * FLONUM FLONUM
 ************************************************************************/
static void
radd_flonum(n_real *real, double op1, double op2)
{
    double value = op1 + op2;

    if (!finite(value))
	fatal_error(FLOATING_POINT_OVERFLOW);
    switch (RTYPE(real)) {
	case N_FIXNUM:
	case N_FIXRATIO:
	    RTYPE(real) = N_FLONUM;
	    break;
	case N_BIGNUM:
	    RCLEAR_BI(real);
	    RTYPE(real) = N_FLONUM;
	    break;
	case N_BIGRATIO:
	    RCLEAR_BR(real);
	    RTYPE(real) = N_FLONUM;
	    break;
    }
    RFF(real) = value;
}

static void
rsub_flonum(n_real *real, double op1, double op2)
{
    double value = op1 - op2;

    if (!finite(value))
	fatal_error(FLOATING_POINT_OVERFLOW);
    switch (RTYPE(real)) {
	case N_FIXNUM:
	case N_FIXRATIO:
	    RTYPE(real) = N_FLONUM;
	    break;
	case N_BIGNUM:
	    RCLEAR_BI(real);
	    RTYPE(real) = N_FLONUM;
	    break;
	case N_BIGRATIO:
	    RCLEAR_BR(real);
	    RTYPE(real) = N_FLONUM;
	    break;
    }
    RFF(real) = value;
}

static void
rmul_flonum(n_real *real, double op1, double op2)
{
    double value = op1 * op2;

    if (!finite(value))
	fatal_error(FLOATING_POINT_OVERFLOW);
    switch (RTYPE(real)) {
	case N_FIXNUM:
	case N_FIXRATIO:
	    RTYPE(real) = N_FLONUM;
	    break;
	case N_BIGNUM:
	    RCLEAR_BI(real);
	    RTYPE(real) = N_FLONUM;
	    break;
	case N_BIGRATIO:
	    RCLEAR_BR(real);
	    RTYPE(real) = N_FLONUM;
	    break;
    }
    RFF(real) = value;
}

static void
rdiv_flonum(n_real *real, double op1, double op2)
{
    double value;

    if (op2 == 0.0)
	fatal_error(DIVIDE_BY_ZERO);
    value = op1 / op2;
    if (!finite(value))
	fatal_error(FLOATING_POINT_OVERFLOW);
    switch (RTYPE(real)) {
	case N_FIXNUM:
	case N_FIXRATIO:
	    RTYPE(real) = N_FLONUM;
	    break;
	case N_BIGNUM:
	    RCLEAR_BI(real);
	    RTYPE(real) = N_FLONUM;
	    break;
	case N_BIGRATIO:
	    RCLEAR_BR(real);
	    RTYPE(real) = N_FLONUM;
	    break;
    }
    RFF(real) = value;
}

static int
cmp_flonum(double op1, double op2)
{
    double value = op1 - op2;

    if (!finite(value))
	fatal_error(FLOATING_POINT_OVERFLOW);

    return (value > 0.0 ? 1 : value < 0.0 ? -1 : 0);
}


/************************************************************************
 * FIXNUM FIXNUM
 ************************************************************************/
static void
rop_fi_fi_bi(n_real *real, long fi, int op)
{
    mpi *bigi = XALLOC(mpi);

    mpi_init(bigi);
    mpi_seti(bigi, RFI(real));
    if (op == NOP_ADD)
	mpi_addi(bigi, bigi, fi);
    else if (op == NOP_SUB)
	mpi_subi(bigi, bigi, fi);
    else
	mpi_muli(bigi, bigi, fi);
    RBI(real) = bigi;
    RTYPE(real) = N_BIGNUM;
}

static INLINE void
radd_fi_fi(n_real *real, long fi)
{
    if (!fi_fi_add_overflow(RFI(real), fi))
	RFI(real) += fi;
    else
	rop_fi_fi_bi(real, fi, NOP_ADD);
}

static INLINE void
rsub_fi_fi(n_real *real, long fi)
{
    if (!fi_fi_sub_overflow(RFI(real), fi))
	RFI(real) -= fi;
    else
	rop_fi_fi_bi(real, fi, NOP_SUB);
}

static INLINE void
rmul_fi_fi(n_real *real, long fi)
{
    if (!fi_fi_mul_overflow(RFI(real), fi))
	RFI(real) *= fi;
    else
	rop_fi_fi_bi(real, fi, NOP_MUL);
}

static INLINE void
rdiv_fi_fi(n_real *real, long fi)
{
    RTYPE(real) = N_FIXRATIO;
    RFRN(real) = RFI(real);
    RFRD(real) = fi;
    rfr_canonicalize(real);
}

static INLINE int
cmp_fi_fi(long op1, long op2)
{
    if (op1 > op2)
	return (1);
    else if (op1 < op2)
	return (-1);

    return (0);
}


/************************************************************************
 * FIXNUM BIGNUM
 ************************************************************************/
static void
rop_fi_bi_xi(n_real *real, mpi *bi, int nop)
{
    mpi *bigi = XALLOC(mpi);

    mpi_init(bigi);
    mpi_seti(bigi, RFI(real));
    if (nop == NOP_ADD)
	mpi_add(bigi, bigi, bi);
    else if (nop == NOP_SUB)
	mpi_sub(bigi, bigi, bi);
    else
	mpi_mul(bigi, bigi, bi);

    if (mpi_fiti(bigi)) {
	RFI(real) = mpi_geti(bigi);
	mpi_clear(bigi);
	XFREE(bigi);
    }
    else {
	RBI(real) = bigi;
	RTYPE(real) = N_BIGNUM;
    }
}

static INLINE void
radd_fi_bi(n_real *real, mpi *bi)
{
    rop_fi_bi_xi(real, bi, NOP_ADD);
}

static INLINE void
rsub_fi_bi(n_real *real, mpi *bi)
{
    rop_fi_bi_xi(real, bi, NOP_SUB);
}

static INLINE void
rmul_fi_bi(n_real *real, mpi *bi)
{
    rop_fi_bi_xi(real, bi, NOP_MUL);
}

static void
rdiv_fi_bi(n_real *real, mpi *bi)
{
    mpr *bigr;

    if (mpi_cmpi(bi, 0) == 0)
	fatal_error(DIVIDE_BY_ZERO);

    bigr = XALLOC(mpr);
    mpr_init(bigr);
    mpi_seti(mpr_num(bigr), RFI(real));
    mpi_set(mpr_den(bigr), bi);
    RBR(real) = bigr;
    RTYPE(real) = N_BIGRATIO;
    rbr_canonicalize(real);
}

static INLINE int
cmp_fi_bi(long fixnum, mpi *bignum)
{
    return (-mpi_cmpi(bignum, fixnum));
}


/************************************************************************
 * FIXNUM FIXRATIO
 ************************************************************************/
static void
rop_fi_fr_as_xr(n_real *real, long num, long den, int nop)
{
    int fit;
    long value = 0, op = RFI(real);

    fit = !fi_fi_mul_overflow(op, den);
    if (fit) {
	value = op * den;
	if (nop == NOP_ADD)
	    fit = !fi_fi_add_overflow(value, num);
	else
	    fit = !fi_fi_sub_overflow(value, num);
    }
    if (fit) {
	if (nop == NOP_ADD)
	    RFRN(real) = value + num;
	else
	    RFRN(real) = value - num;
	RFRD(real) = den;
	RTYPE(real) = N_FIXRATIO;
	rfr_canonicalize(real);
    }
    else {
	mpi iop;
	mpr *bigr = XALLOC(mpr);

	mpi_init(&iop);
	mpi_seti(&iop, op);
	mpi_muli(&iop, &iop, den);

	mpr_init(bigr);
	mpr_seti(bigr, num, den);
	if (nop == NOP_ADD)
	    mpi_add(mpr_num(bigr), &iop, mpr_num(bigr));
	else
	    mpi_sub(mpr_num(bigr), &iop, mpr_num(bigr));
	mpi_clear(&iop);
	RBR(real) = bigr;
	RTYPE(real) = N_BIGRATIO;
	rbr_canonicalize(real);
    }
}

static void
rop_fi_fr_md_xr(n_real *real, long num, long den, int nop)
{
    int fit;
    long op = RFI(real);

    if (nop == NOP_MUL)
	fit = !fi_fi_mul_overflow(op, num);
    else
	fit = !fi_fi_mul_overflow(op, den);
    if (fit) {
	if (nop == NOP_MUL) {
	    RFRN(real) = op * num;
	    RFRD(real) = den;
	}
	else {
	    RFRN(real) = op * den;
	    RFRD(real) = num;
	}
	RTYPE(real) = N_FIXRATIO;
	rfr_canonicalize(real);
    }
    else {
	mpi iop;
	mpr *bigr = XALLOC(mpr);

	mpi_init(&iop);
	mpi_seti(&iop, op);

	mpr_init(bigr);
	if (nop == NOP_MUL)
	    mpr_seti(bigr, num, den);
	else
	    mpr_seti(bigr, den, num);
	mpi_mul(mpr_num(bigr), mpr_num(bigr), &iop);
	mpi_clear(&iop);
	RBR(real) = bigr;
	RTYPE(real) = N_BIGRATIO;
	rbr_canonicalize(real);
    }
}

static INLINE void
radd_fi_fr(n_real *real, long num, long den)
{
    rop_fi_fr_as_xr(real, num, den, NOP_ADD);
}

static INLINE void
rsub_fi_fr(n_real *real, long num, long den)
{
    rop_fi_fr_as_xr(real, num, den, NOP_SUB);
}

static INLINE void
rmul_fi_fr(n_real *real, long num, long den)
{
    rop_fi_fr_md_xr(real, num, den, NOP_MUL);
}

static INLINE void
rdiv_fi_fr(n_real *real, long num, long den)
{
    rop_fi_fr_md_xr(real, num, den, NOP_DIV);
}

static INLINE int
cmp_fi_fr(long fi, long num, long den)
{
    return (cmp_flonum((double)fi, (double)num / (double)den));
}


/************************************************************************
 * FIXNUM BIGRATIO
 ************************************************************************/
static void
rop_fi_br_as_xr(n_real *real, mpr *ratio, int nop)
{
    mpi iop;
    mpr *bigr = XALLOC(mpr);

    mpi_init(&iop);
    mpi_seti(&iop, RFI(real));

    mpr_init(bigr);
    mpr_set(bigr, ratio);

    mpi_mul(&iop, &iop, mpr_den(ratio));
    if (nop == NOP_ADD)
	mpi_add(mpr_num(bigr), &iop, mpr_num(bigr));
    else
	mpi_sub(mpr_num(bigr), &iop, mpr_num(bigr));

    mpi_clear(&iop);
    RBR(real) = bigr;
    RTYPE(real) = N_BIGRATIO;
    rbr_canonicalize(real); 
}

static void
rop_fi_br_md_xr(n_real *real, mpr *ratio, int nop)
{
    mpi iop;
    mpr *bigr = XALLOC(mpr);

    mpi_init(&iop);
    mpi_seti(&iop, RFI(real));

    mpr_init(bigr);
    if (nop == NOP_MUL)
	mpr_set(bigr, ratio);
    else
	mpr_inv(bigr, ratio);

    mpi_mul(mpr_num(bigr), &iop, mpr_num(bigr));

    mpi_clear(&iop);
    RBR(real) = bigr;
    RTYPE(real) = N_BIGRATIO;
    rbr_canonicalize(real);
}

static INLINE void
radd_fi_br(n_real *real, mpr *ratio)
{
    rop_fi_br_as_xr(real, ratio, NOP_ADD);
}

static INLINE void
rsub_fi_br(n_real *real, mpr *ratio)
{
    rop_fi_br_as_xr(real, ratio, NOP_SUB);
}

static INLINE void
rmul_fi_br(n_real *real, mpr *ratio)
{
    rop_fi_br_md_xr(real, ratio, NOP_MUL);
}

static INLINE void
rdiv_fi_br(n_real *real, mpr *ratio)
{
    rop_fi_br_md_xr(real, ratio, NOP_DIV);
}

static INLINE int
cmp_fi_br(long op1, mpr *op2)
{
    return (-mpr_cmpi(op2, op1));
}


/************************************************************************
 * BIGNUM FIXNUM
 ************************************************************************/
static INLINE void
radd_bi_fi(n_real *real, long fi)
{
    mpi_addi(RBI(real), RBI(real), fi);
    rbi_canonicalize(real);
}

static INLINE void
rsub_bi_fi(n_real *real, long fi)
{
    mpi_subi(RBI(real), RBI(real), fi);
    rbi_canonicalize(real);
}

static INLINE void
rmul_bi_fi(n_real *real, long fi)
{
    mpi_muli(RBI(real), RBI(real), fi);
    rbi_canonicalize(real);
}

static void
rdiv_bi_fi(n_real *real, long fi)
{
    mpr *bigr;

    if (RFI(real) == 0)
	fatal_error(DIVIDE_BY_ZERO);

    bigr = XALLOC(mpr);
    mpr_init(bigr);
    mpi_set(mpr_num(bigr), RBI(real));
    mpi_seti(mpr_den(bigr), fi);
    RCLEAR_BI(real);
    RBR(real) = bigr;
    RTYPE(real) = N_BIGRATIO;
    rbr_canonicalize(real);
}

static INLINE int
cmp_bi_fi(mpi *bignum, long fi)
{
    return (mpi_cmpi(bignum, fi));
}


/************************************************************************
 * BIGNUM BIGNUM
 ************************************************************************/
static INLINE void
radd_bi_bi(n_real *real, mpi *bignum)
{
    mpi_add(RBI(real), RBI(real), bignum);
    rbi_canonicalize(real);
}

static INLINE void
rsub_bi_bi(n_real *real, mpi *bignum)
{
    mpi_sub(RBI(real), RBI(real), bignum);
    rbi_canonicalize(real);
}

static INLINE void
rmul_bi_bi(n_real *real, mpi *bignum)
{
    mpi_mul(RBI(real), RBI(real), bignum);
    rbi_canonicalize(real);
}

static void
rdiv_bi_bi(n_real *real, mpi *bignum)
{
    mpr *bigr;

    if (mpi_cmpi(bignum, 0) == 0)
	fatal_error(DIVIDE_BY_ZERO);

    bigr = XALLOC(mpr);
    mpr_init(bigr);
    mpi_set(mpr_num(bigr), RBI(real));
    mpi_set(mpr_den(bigr), bignum);
    RCLEAR_BI(real);
    RBR(real) = bigr;
    RTYPE(real) = N_BIGRATIO;
    rbr_canonicalize(real); 
}

static INLINE int
cmp_bi_bi(mpi *op1, mpi *op2)
{
    return (mpi_cmp(op1, op2));
}


/************************************************************************
 * BIGNUM FIXRATIO
 ************************************************************************/
static void
rop_bi_fr_as_xr(n_real *real, long num, long den, int nop)
{
    mpi iop;
    mpr *bigr = XALLOC(mpr);

    mpi_init(&iop);
    mpi_set(&iop, RBI(real));
    mpi_muli(&iop, &iop, den);

    mpr_init(bigr);
    mpr_seti(bigr, num, den);

    if (nop == NOP_ADD)
	mpi_add(mpr_num(bigr), &iop, mpr_num(bigr));
    else
	mpi_sub(mpr_num(bigr), &iop, mpr_num(bigr));
    mpi_clear(&iop);

    RCLEAR_BI(real);
    RBR(real) = bigr;
    RTYPE(real) = N_BIGRATIO;
    rbr_canonicalize(real);
}

static INLINE void
rop_bi_fr_md_xr(n_real *real, long num, long den, int nop)
{
    mpr *bigr = XALLOC(mpr);

    mpr_init(bigr);

    mpr_seti(bigr, num, den);

    if (nop == NOP_MUL)
	mpi_mul(mpr_num(bigr), RBI(real), mpr_num(bigr));
    else {
	mpi_mul(mpr_den(bigr), RBI(real), mpr_den(bigr));
	mpr_inv(bigr, bigr);
    }

    RCLEAR_BI(real);
    RBR(real) = bigr;
    RTYPE(real) = N_BIGRATIO;
    rbr_canonicalize(real);
}

static INLINE void
radd_bi_fr(n_real *real, long num, long den)
{
    rop_bi_fr_as_xr(real, num, den, NOP_ADD);
}

static INLINE void
rsub_bi_fr(n_real *real, long num, long den)
{
    rop_bi_fr_as_xr(real, num, den, NOP_SUB);
}

static INLINE void
rmul_bi_fr(n_real *real, long num, long den)
{
    rop_bi_fr_md_xr(real, num, den, NOP_MUL);
}

static INLINE void
rdiv_bi_fr(n_real *real, long num, long den)
{
    rop_bi_fr_md_xr(real, num, den, NOP_DIV);
}

static int
cmp_bi_fr(mpi *bignum, long num, long den)
{
    int cmp;
    mpr cmp1, cmp2;

    mpr_init(&cmp1);
    mpi_set(mpr_num(&cmp1), bignum);
    mpi_seti(mpr_den(&cmp1), 1);

    mpr_init(&cmp2);
    mpr_seti(&cmp2, num, den);

    cmp = mpr_cmp(&cmp1, &cmp2);
    mpr_clear(&cmp1);
    mpr_clear(&cmp2);

    return (cmp);
}


/************************************************************************
 * BIGNUM BIGRATIO
 ************************************************************************/
static void
rop_bi_br_as_xr(n_real *real, mpr *bigratio, int nop)
{
    mpi iop;
    mpr *bigr = XALLOC(mpr);

    mpi_init(&iop);
    mpi_set(&iop, RBI(real));
    mpr_init(bigr);
    mpr_set(bigr, bigratio);

    mpi_mul(&iop, &iop, mpr_den(bigratio));

    if (nop == NOP_ADD)
	mpi_add(mpr_num(bigr), &iop, mpr_num(bigr));
    else
	mpi_sub(mpr_num(bigr), &iop, mpr_num(bigr));
    mpi_clear(&iop);

    RCLEAR_BI(real);
    RBR(real) = bigr;
    RTYPE(real) = N_BIGRATIO;
    rbr_canonicalize(real);
}

static void
rop_bi_br_md_xr(n_real *real, mpr *bigratio, int nop)
{
    mpr *bigr = XALLOC(mpr);

    mpr_init(bigr);
    if (nop == NOP_MUL)
	mpr_set(bigr, bigratio);
    else
	mpr_inv(bigr, bigratio);

    mpi_mul(mpr_num(bigr), RBI(real), mpr_num(bigr));

    RCLEAR_BI(real);
    RBR(real) = bigr;
    RTYPE(real) = N_BIGRATIO;
    rbr_canonicalize(real); 
}

static INLINE void
radd_bi_br(n_real *real, mpr *bigratio)
{
    rop_bi_br_as_xr(real, bigratio, NOP_ADD);
}

static INLINE void
rsub_bi_br(n_real *real, mpr *bigratio)
{
    rop_bi_br_as_xr(real, bigratio, NOP_SUB);
}

static INLINE void
rmul_bi_br(n_real *real, mpr *bigratio)
{
    rop_bi_br_md_xr(real, bigratio, NOP_MUL);
}

static INLINE void
rdiv_bi_br(n_real *real, mpr *bigratio)
{
    rop_bi_br_md_xr(real, bigratio, NOP_DIV);
}

static int
cmp_bi_br(mpi *bignum, mpr *bigratio)
{
    int cmp;
    mpr cmp1;

    mpr_init(&cmp1);
    mpi_set(mpr_num(&cmp1), bignum);
    mpi_seti(mpr_den(&cmp1), 1);

    cmp = mpr_cmp(&cmp1, bigratio);
    mpr_clear(&cmp1);

    return (cmp);
}


/************************************************************************
 * FIXRATIO FIXNUM
 ************************************************************************/
static void
rop_fr_fi_as_xr(n_real *real, long op, int nop)
{
    int fit;
    long value = 0, num = RFRN(real), den = RFRD(real);

    fit = !fi_fi_mul_overflow(op, den);

    if (fit) {
	value = op * den;
	if (nop == NOP_ADD)
	    fit = !fi_fi_add_overflow(value, num);
	else
	    fit = !fi_fi_sub_overflow(value, num);
    }
    if (fit) {
	if (nop == NOP_ADD)
	    RFRN(real) = num + value;
	else
	    RFRN(real) = num - value;
	rfr_canonicalize(real);
    }
    else {
	mpi iop;
	mpr *bigr = XALLOC(mpr);

	mpr_init(bigr);
	mpr_seti(bigr, num, den);
	mpi_init(&iop);
	mpi_seti(&iop, op);
	mpi_muli(&iop, &iop, den);
	if (nop == NOP_ADD)
	    mpi_add(mpr_num(bigr), mpr_num(bigr), &iop);
	else
	    mpi_sub(mpr_num(bigr), mpr_num(bigr), &iop);
	mpi_clear(&iop);
	RBR(real) = bigr;
	RTYPE(real) = N_BIGRATIO;
	rbr_canonicalize(real);
    }
}

static void
rop_fr_fi_md_xr(n_real *real, long op, int nop)
{
    long num = RFRN(real), den = RFRD(real);

    if (nop == NOP_MUL) {
	if (!fi_fi_mul_overflow(op, num)) {
	    RFRN(real) = op * num;
	    rfr_canonicalize(real);
	    return;
	}
    }
    else if (!fi_fi_mul_overflow(op, den)) {
	RFRD(real) = op * den;
	rfr_canonicalize(real);
	return;
    }

    {
	mpr *bigr = XALLOC(mpr);

	mpr_init(bigr);
	mpr_seti(bigr, num, den);
	if (nop == NOP_MUL)
	    mpr_muli(bigr, bigr, op);
	else
	    mpr_divi(bigr, bigr, op);
	RBR(real) = bigr;
	RTYPE(real) = N_BIGRATIO;
	rbr_canonicalize(real);
    }
}

static INLINE void
radd_fr_fi(n_real *real, long op)
{
    rop_fr_fi_as_xr(real, op, NOP_ADD);
}

static INLINE void
rsub_fr_fi(n_real *real, long op)
{
    rop_fr_fi_as_xr(real, op, NOP_SUB);
}

static INLINE void
rmul_fr_fi(n_real *real, long op)
{
    rop_fr_fi_md_xr(real, op, NOP_MUL);
}

static INLINE void
rdiv_fr_fi(n_real *real, long op)
{
    rop_fr_fi_md_xr(real, op, NOP_DIV);
}

static INLINE int
cmp_fr_fi(long num, long den, long fixnum)
{
    return (cmp_flonum((double)num / (double)den, (double)fixnum));
}


/************************************************************************
 * FIXRATIO BIGNUM
 ************************************************************************/
static void
rop_fr_bi_as_xr(n_real *real, mpi *bignum, int nop)
{
    mpi iop;
    mpr *bigr = XALLOC(mpr);

    mpr_init(bigr);
    mpr_seti(bigr, RFRN(real), RFRD(real));

    mpi_init(&iop);
    mpi_set(&iop, bignum);
    mpi_muli(&iop, &iop, RFRD(real));

    if (nop == NOP_ADD)
	mpi_add(mpr_num(bigr), mpr_num(bigr), &iop);
    else
	mpi_sub(mpr_num(bigr), mpr_num(bigr), &iop);
    mpi_clear(&iop);

    RBR(real) = bigr;
    RTYPE(real) = N_BIGRATIO;
    rbr_canonicalize(real); 
}

static void
rop_fr_bi_md_xr(n_real *real, mpi *bignum, int nop)
{
    mpr *bigr = XALLOC(mpr);

    mpr_init(bigr);
    mpr_seti(bigr, RFRN(real), RFRD(real));

    if (nop == NOP_MUL)
	mpi_mul(mpr_num(bigr), mpr_num(bigr), bignum);
    else
	mpi_mul(mpr_den(bigr), mpr_den(bigr), bignum);

    RBR(real) = bigr;
    RTYPE(real) = N_BIGRATIO;
    rbr_canonicalize(real); 
}

static INLINE void
radd_fr_bi(n_real *real, mpi *bignum)
{
    rop_fr_bi_as_xr(real, bignum, NOP_ADD);
}

static INLINE void
rsub_fr_bi(n_real *real, mpi *bignum)
{
    rop_fr_bi_as_xr(real, bignum, NOP_SUB);
}

static INLINE void
rmul_fr_bi(n_real *real, mpi *bignum)
{
    rop_fr_bi_md_xr(real, bignum, NOP_MUL);
}

static INLINE void
rdiv_fr_bi(n_real *real, mpi *bignum)
{
    rop_fr_bi_md_xr(real, bignum, NOP_DIV);
}

static int
cmp_fr_bi(long num, long den, mpi *bignum)
{
    int cmp;
    mpr cmp1, cmp2;

    mpr_init(&cmp1);
    mpr_seti(&cmp1, num, den);

    mpr_init(&cmp2);
    mpi_set(mpr_num(&cmp2), bignum);
    mpi_seti(mpr_den(&cmp2), 1);

    cmp = mpr_cmp(&cmp1, &cmp2);
    mpr_clear(&cmp1);
    mpr_clear(&cmp2);

    return (cmp);
}


/************************************************************************
 * FIXRATIO FIXRATIO
 ************************************************************************/
static void
rop_fr_fr_as_xr(n_real *real, long num2, long den2, int nop)
{
    int fit;
    long num1 = RFRN(real), den1 = RFRD(real), num = 0, den = 0;

    fit = !fi_fi_mul_overflow(num1, den2);
    if (fit) {
	num = num1 * den2;
	fit = !fi_fi_mul_overflow(num2, den1);
	if (fit) {
	    den = num2 * den1;
	    if (nop == NOP_ADD) {
		if ((fit = !fi_fi_add_overflow(num, den)) != 0)
		    num += den;
	    }
	    else if ((fit = !fi_fi_sub_overflow(num, den)) != 0)
		num -= den;
	    if (fit) {
		fit = !fi_fi_mul_overflow(den1, den2);
		if (fit)
		    den = den1 * den2;
	    }
	}
    }
    if (fit) {
	RFRN(real) = num;
	RFRD(real) = den;
	rfr_canonicalize(real);
    }
    else {
	mpi iop;
	mpr *bigr = XALLOC(mpr);

	mpr_init(bigr);
	mpr_seti(bigr, num1, den1);
	mpi_muli(mpr_den(bigr), mpr_den(bigr), den2);
	mpi_init(&iop);
	mpi_seti(&iop, num2);
	mpi_muli(&iop, &iop, den1);
	mpi_muli(mpr_num(bigr), mpr_num(bigr), den2);
	if (nop == NOP_ADD)
	    mpi_add(mpr_num(bigr), mpr_num(bigr), &iop);
	else
	    mpi_sub(mpr_num(bigr), mpr_num(bigr), &iop);
	mpi_clear(&iop);
	RBR(real) = bigr;
	RTYPE(real) = N_BIGRATIO;
	rbr_canonicalize(real);
    }
}

static void
rop_fr_fr_md_xr(n_real *real, long num2, long den2, int nop)
{
    int fit;
    long num1 = RFRN(real), den1 = RFRD(real), num = 0, den = 0;

    if (nop == NOP_MUL) {
	fit = !fi_fi_mul_overflow(num1, num2) && !fi_fi_mul_overflow(den1, den2);
	if (fit) {
	    num = num1 * num2;
	    den = den1 * den2;
	}
    }
    else {
	fit = !fi_fi_mul_overflow(num1, den2) && !fi_fi_mul_overflow(den1, num2);
	if (fit) {
	    num = num1 * den2;
	    den = den1 * num2;
	}
    }

    if (fit) {
	RFRN(real) = num;
	RFRD(real) = den;
	rfr_canonicalize(real);
    }
    else {
	mpr *bigr = XALLOC(mpr);

	mpr_init(bigr);

	if (nop == NOP_MUL) {
	    mpr_seti(bigr, num1, den1);
	    mpi_muli(mpr_num(bigr), mpr_num(bigr), num2);
	    mpi_muli(mpr_den(bigr), mpr_den(bigr), den2);
	}
	else {
	    mpr_seti(bigr, num1, num2);
	    mpi_muli(mpr_num(bigr), mpr_num(bigr), den2);
	    mpi_muli(mpr_den(bigr), mpr_den(bigr), den1);
	}

	RBR(real) = bigr;
	RTYPE(real) = N_BIGRATIO;
	rbr_canonicalize(real);
    }
}

static INLINE void
radd_fr_fr(n_real *real, long num, long den)
{
    rop_fr_fr_as_xr(real, num, den, NOP_ADD);
}

static INLINE void
rsub_fr_fr(n_real *real, long num, long den)
{
    rop_fr_fr_as_xr(real, num, den, NOP_SUB);
}

static INLINE void
rmul_fr_fr(n_real *real, long num, long den)
{
    rop_fr_fr_md_xr(real, num, den, NOP_MUL);
}

static INLINE void
rdiv_fr_fr(n_real *real, long num, long den)
{
    rop_fr_fr_md_xr(real, num, den, NOP_DIV);
}

static INLINE int
cmp_fr_fr(long num1, long den1, long num2, long den2)
{
    return (cmp_flonum((double)num1 / (double)den1,
		       (double)num2 / (double)den2));
}


/************************************************************************
 * FIXRATIO BIGRATIO
 ************************************************************************/
static void
rop_fr_br_asmd_xr(n_real *real, mpr *bigratio, int nop)
{
    mpr *bigr = XALLOC(mpr);

    mpr_init(bigr);
    mpr_seti(bigr, RFRN(real), RFRD(real));

    switch (nop) {
	case NOP_ADD:
	    mpr_add(bigr, bigr, bigratio);
	    break;
	case NOP_SUB:
	    mpr_sub(bigr, bigr, bigratio);
	    break;
	case NOP_MUL:
	    mpr_mul(bigr, bigr, bigratio);
	    break;
	default:
	    mpr_div(bigr, bigr, bigratio);
	    break;
    }

    RBR(real) = bigr;
    RTYPE(real) = N_BIGRATIO;
    rbr_canonicalize(real); 
}

static INLINE void
radd_fr_br(n_real *real, mpr *bigratio)
{
    rop_fr_br_asmd_xr(real, bigratio, NOP_ADD);
}

static INLINE void
rsub_fr_br(n_real *real, mpr *bigratio)
{
    rop_fr_br_asmd_xr(real, bigratio, NOP_SUB);
}

static INLINE void
rmul_fr_br(n_real *real, mpr *bigratio)
{
    rop_fr_br_asmd_xr(real, bigratio, NOP_MUL);
}

static INLINE void
rdiv_fr_br(n_real *real, mpr *bigratio)
{
    rop_fr_br_asmd_xr(real, bigratio, NOP_DIV);
}

static int
cmp_fr_br(long num, long den, mpr *bigratio)
{
    int cmp;
    mpr cmp1;

    mpr_init(&cmp1);
    mpr_seti(&cmp1, num, den);

    cmp = mpr_cmp(&cmp1, bigratio);
    mpr_clear(&cmp1);

    return (cmp);
}


/************************************************************************
 * BIGRATIO FIXNUM
 ************************************************************************/
static void
rop_br_fi_asmd_xr(n_real *real, long fixnum, int nop)
{
    mpr *bigratio = RBR(real);

    switch (nop) {
	case NOP_ADD:
	    mpr_addi(bigratio, bigratio, fixnum);
	    break;
	case NOP_SUB:
	    mpr_subi(bigratio, bigratio, fixnum);
	    break;
	case NOP_MUL:
	    mpr_muli(bigratio, bigratio, fixnum);
	    break;
	default:
	    if (fixnum == 0)
		fatal_error(DIVIDE_BY_ZERO);
	    mpr_divi(bigratio, bigratio, fixnum);
	    break;
    }
    rbr_canonicalize(real); 
}

static INLINE void
radd_br_fi(n_real *real, long fixnum)
{
    rop_br_fi_asmd_xr(real, fixnum, NOP_ADD);
}

static INLINE void
rsub_br_fi(n_real *real, long fixnum)
{
    rop_br_fi_asmd_xr(real, fixnum, NOP_SUB);
}

static INLINE void
rmul_br_fi(n_real *real, long fixnum)
{
    rop_br_fi_asmd_xr(real, fixnum, NOP_MUL);
}

static INLINE void
rdiv_br_fi(n_real *real, long fixnum)
{
    rop_br_fi_asmd_xr(real, fixnum, NOP_DIV);
}

static int
cmp_br_fi(mpr *bigratio, long fixnum)
{
    int cmp;
    mpr cmp2;

    mpr_init(&cmp2);
    mpr_seti(&cmp2, fixnum, 1);
    cmp = mpr_cmp(bigratio, &cmp2);
    mpr_clear(&cmp2);

    return (cmp);
}


/************************************************************************
 * BIGRATIO BIGNUM
 ************************************************************************/
static void
rop_br_bi_as_xr(n_real *real, mpi *bignum, int nop)
{
    mpi iop;

    mpi_init(&iop);
    mpi_set(&iop, bignum);

    mpi_mul(&iop, &iop, RBRD(real));
    if (nop == NOP_ADD)
	mpi_add(RBRN(real), RBRN(real), &iop);
    else
	mpi_sub(RBRN(real), RBRN(real), &iop);
    mpi_clear(&iop);
    rbr_canonicalize(real); 
}

static INLINE void
radd_br_bi(n_real *real, mpi *bignum)
{
    rop_br_bi_as_xr(real, bignum, NOP_ADD);
}

static INLINE void
rsub_br_bi(n_real *real, mpi *bignum)
{
    rop_br_bi_as_xr(real, bignum, NOP_SUB);
}

static INLINE void
rmul_br_bi(n_real *real, mpi *bignum)
{
    mpi_mul(RBRN(real), RBRN(real), bignum);
    rbr_canonicalize(real);
}

static INLINE void
rdiv_br_bi(n_real *real, mpi *bignum)
{
    mpi_mul(RBRD(real), RBRD(real), bignum);
    rbr_canonicalize(real);
}

static int
cmp_br_bi(mpr *bigratio, mpi *bignum)
{
    int cmp;
    mpr cmp1;

    mpr_init(&cmp1);
    mpi_set(mpr_num(&cmp1), bignum);
    mpi_seti(mpr_den(&cmp1), 1);

    cmp = mpr_cmp(bigratio, &cmp1);
    mpr_clear(&cmp1);

    return (cmp);
}


/************************************************************************
 * BIGRATIO FIXRATIO
 ************************************************************************/
static void
rop_br_fr_asmd_xr(n_real *real, long num, long den, int nop)
{
    mpr *bigratio = RBR(real), rop;

    mpr_init(&rop);
    mpr_seti(&rop, num, den);
    switch (nop) {
	case NOP_ADD:
	    mpr_add(bigratio, bigratio, &rop);
	    break;
	case NOP_SUB:
	    mpr_sub(bigratio, bigratio, &rop);
	    break;
	case NOP_MUL:
	    mpr_mul(bigratio, bigratio, &rop);
	    break;
	default:
	    mpr_div(bigratio, bigratio, &rop);
	    break;
    }
    mpr_clear(&rop);
    rbr_canonicalize(real); 
}

static INLINE void
radd_br_fr(n_real *real, long num, long den)
{
    rop_br_fr_asmd_xr(real, num, den, NOP_ADD);
}

static INLINE void
rsub_br_fr(n_real *real, long num, long den)
{
    rop_br_fr_asmd_xr(real, num, den, NOP_SUB);
}

static INLINE void
rmul_br_fr(n_real *real, long num, long den)
{
    rop_br_fr_asmd_xr(real, num, den, NOP_MUL);
}

static INLINE void
rdiv_br_fr(n_real *real, long num, long den)
{
    rop_br_fr_asmd_xr(real, num, den, NOP_DIV);
}

static int
cmp_br_fr(mpr *bigratio, long num, long den)
{
    int cmp;
    mpr cmp2;

    mpr_init(&cmp2);
    mpr_seti(&cmp2, num, den);
    cmp = mpr_cmp(bigratio, &cmp2);
    mpr_clear(&cmp2);

    return (cmp);
}


/************************************************************************
 * BIGRATIO BIGRATIO
 ************************************************************************/
static INLINE void
radd_br_br(n_real *real, mpr *bigratio)
{
    mpr_add(RBR(real), RBR(real), bigratio);
    rbr_canonicalize(real); 
}

static INLINE void
rsub_br_br(n_real *real, mpr *bigratio)
{
    mpr_sub(RBR(real), RBR(real), bigratio);
    rbr_canonicalize(real); 
}

static INLINE void
rmul_br_br(n_real *real, mpr *bigratio)
{
    mpr_mul(RBR(real), RBR(real), bigratio);
    rbr_canonicalize(real); 
}

static INLINE void
rdiv_br_br(n_real *real, mpr *bigratio)
{
    mpr_div(RBR(real), RBR(real), bigratio);
    rbr_canonicalize(real); 
}

static INLINE int
cmp_br_br(mpr *op1, mpr *op2)
{
    return (mpr_cmp(op1, op2));
}
