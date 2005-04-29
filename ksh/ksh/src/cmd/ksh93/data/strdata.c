/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1982-2004 AT&T Corp.                *
*        and it may only be used by you under license from         *
*                       AT&T Corp. ("AT&T")                        *
*         A copy of the Source Code Agreement is available         *
*                at the AT&T Internet web site URL                 *
*                                                                  *
*       http://www.research.att.com/sw/license/ast-open.html       *
*                                                                  *
*    If you have copied or used this software without agreeing     *
*        to the terms of the license you are infringing on         *
*           the license and copyright and are violating            *
*               AT&T's intellectual property rights.               *
*                                                                  *
*            Information and Software Systems Research             *
*                        AT&T Labs Research                        *
*                         Florham Park NJ                          *
*                                                                  *
*                David Korn <dgk@research.att.com>                 *
*                                                                  *
*******************************************************************/
#pragma prototyped
/*
 * data for string evaluator library
 */

#include	"FEATURE/options"
#include	"FEATURE/math"
#include	"streval.h"

const unsigned char strval_precedence[35] =
	/* opcode	precedence,assignment  */
{
	/* DEFAULT */		MAXPREC|NOASSIGN,
	/* DONE */		0|NOASSIGN|RASSOC,
	/* NEQ */		10|NOASSIGN,
	/* NOT */		MAXPREC|NOASSIGN,
	/* MOD */		14,
	/* ANDAND */		6|NOASSIGN|SEQPOINT,
	/* AND */		9|NOFLOAT,
	/* LPAREN */		MAXPREC|NOASSIGN|SEQPOINT,
	/* RPAREN */		1|NOASSIGN|RASSOC|SEQPOINT,
	/* POW */		14|NOASSIGN|RASSOC,
	/* TIMES */		14,
	/* PLUSPLUS */		15|NOASSIGN|NOFLOAT|SEQPOINT,
	/* PLUS */		13,	
	/* COMMA */		1|NOASSIGN|SEQPOINT,
	/* MINUSMINUS */	15|NOASSIGN|NOFLOAT|SEQPOINT,
	/* MINUS */		13,
	/* DIV */		14,
	/* LSHIFT */		12|NOFLOAT,
	/* LE */		11|NOASSIGN,
	/* LT */		11|NOASSIGN,	
	/* EQ */		10|NOASSIGN,
	/* ASSIGNMENT */	2|RASSOC,
	/* COLON */		0|NOASSIGN,
	/* RSHIFT */		12|NOFLOAT,	
	/* GE */		11|NOASSIGN,
	/* GT */		11|NOASSIGN,
	/* QCOLON */		3|NOASSIGN|SEQPOINT,
	/* QUEST */		3|NOASSIGN|SEQPOINT|RASSOC,
	/* XOR */		8|NOFLOAT,
	/* OROR */		5|NOASSIGN|SEQPOINT,
	/* OR */		7|NOFLOAT,
	/* DEFAULT */		MAXPREC|NOASSIGN,
	/* DEFAULT */		MAXPREC|NOASSIGN,
	/* DEFAULT */		MAXPREC|NOASSIGN,
	/* DEFAULT */		MAXPREC|NOASSIGN
};

/*
 * This is for arithmetic expressions
 */
const char strval_states[64] =
{
	A_EOF,	A_REG,	A_REG,	A_REG,	A_REG,	A_REG,	A_REG,	A_REG,
	A_REG,	0,	0,	A_REG,	A_REG,	A_REG,	A_REG,	A_REG,
	A_REG,	A_REG,	A_REG,	A_REG,	A_REG,	A_REG,	A_REG,	A_REG,
	A_REG,	A_REG,	A_REG,	A_REG,	A_REG,	A_REG,	A_REG,	A_REG,

	0,	A_NOT,	0,	A_REG,	A_REG,	A_MOD,	A_AND,	A_LIT,
	A_LPAR,	A_RPAR,	A_TIMES,A_PLUS,	A_COMMA,A_MINUS,A_DOT,	A_DIV,
	A_DIG,	A_DIG,	A_DIG,	A_DIG,	A_DIG,	A_DIG,	A_DIG,	A_DIG,
	A_DIG,	A_DIG,	A_COLON,A_REG,	A_LT,	A_ASSIGN,A_GT,	A_QUEST

};


const char e_argcount[]		= "%s: funcion has wrong number of arguments";
const char e_badnum[]		= "%s: bad number";
const char e_moretokens[]	= "%s: more tokens expected";
const char e_paren[]		= "%s: unbalanced parenthesis";
const char e_badcolon[]		= "%s: invalid use of :";
const char e_divzero[]		= "%s: divide by zero";
const char e_synbad[]		= "%s: arithmetic syntax error";
const char e_notlvalue[]	= "%s: assignment requires lvalue";
const char e_recursive[]	= "%s: recursion too deep";
const char e_questcolon[]	= "%s: ':' expected for '?' operator";
const char e_function[]		= "%s: unknown function";
const char e_incompatible[]	= "%s: invalid floating point operation";
const char e_overflow[]		= "%s: overflow exception";
const char e_domain[]		= "%s: domain exception";
const char e_singularity[]	= "%s: singularity exception";
const char e_charconst[]	= "%s: invalid character constant";

#ifdef __NeXT
extern double	hypot(double, double);
#endif

typedef Sfdouble_t (*mathf)(Sfdouble_t,...);

#ifdef _ast_fltmax_double

#   define	fabsl	fabs
#   define	acosl	acos
#   define	asinl	asin
#   define	atanl	atan
#   define	atan2l	atan2
#   define	cosl	cos
#   define	coshl	cosh
#   define	expl	exp
#   define	fmodl	fmod
#   define	hypotl	hypot
#   define	floorl	floor
#   define	logl	log
#   define	powl	pow
#   define	sinl	sin
#   define	sinhl	sinh
#   define	sqrtl	sqrt
#   define	tanl	tan
#   define	tanhl	tanh

#else

#ifdef __STDARG__
#   define fundef(name)		static _ast_fltmax_t local_##name(_ast_fltmax_t d){ return(name(d));}
#   define protodef(name)	extern _ast_fltmax_t name(_ast_fltmax_t);
#   define macdef(name,x)	((Sfdouble_t)name((double)(x)))
#   define fundef2(name)	static _ast_fltmax_t local_##name(_ast_fltmax_t x, _ast_fltmax_t y){ return(name(x,y));}
#   define protodef2(name)	extern _ast_fltmax_t name(_ast_fltmax_t,_ast_fltmax_t);
#   define macdef2(name,x,y)	((Sfdouble_t)name((double)(x),(double)(y)))
#else
#   define fundef(name)		static _ast_fltmax_t local_##name(d) _ast_fltmax_t d;{ return(name(d));}
#   define protodef(name)	extern _ast_fltmax_t name();
#   define macdef(name,x)	((Sfdouble_t)name((double)(x)))
#   define fundef2(name)	static _ast_fltmax_t local_##name(x,y) _ast_fltmax_t x,y;{ return(name(x,y));}
#   define protodef2(name)	extern _ast_fltmax_t name();
#   define macdef2(name,x,y)	((Sfdouble_t)name((double)(x),(double)(y)))
#endif

#if 0 /* proto bug workaround */
{
#endif

#if defined(acosl) || !defined(_lib_acosl)
#   ifndef acosl
#       define acosl(x) macdef(acos,x)
#   endif
    fundef(acosl)
#   undef acosl
#   define acosl local_acosl
#else
#   if defined(_npt_acosl)
	protodef(acosl)
#   endif
#endif 

#if defined(asinl) || !defined(_lib_asinl)
#   ifndef asinl
#       define asinl(x) macdef(asin,x)
#   endif
    fundef(asinl)
#   undef asinl
#   define asinl local_asinl
#else
#   if defined(_npt_asinl)
	protodef(asinl)
#   endif
#endif 

#if defined(atanl) || !defined(_lib_atanl)
#   ifndef atanl
#       define atanl(x) macdef(atan,x)
#   endif
    fundef(atanl)
#   undef atanl
#   define atanl local_atanl
#else
#   if defined(_npt_atanl)
	protodef(atanl)
#   endif
#endif 

#if defined(atan2l) || !defined(_lib_atan2l)
#   ifndef atan2l
#       define atan2l(x,y) macdef2(atan2,x,y)
#   endif
    fundef2(atan2l)
#   undef atan2l
#   define atan2l local_atan2l
#else
#   if defined(_npt_atan2l)
	protodef2(atan2l)
#   endif
#endif 

#if defined(expl) || !defined(_lib_expl)
#   ifndef expl
#       define expl(x) macdef(exp,x)
#   endif
    fundef(expl)
#   undef expl
#   define expl local_expl
#else
#   if defined(_npt_expl)
	protodef(expl)
#   endif
#endif 

#if defined(cosl) || !defined(_lib_cosl)
#   ifndef cosl
#       define cosl(x) macdef(cos,x)
#   endif
    fundef(cosl)
#   undef cosl
#   define cosl local_cosl
#else
#   if defined(_npt_cosl)
	protodef(cosl)
#   endif
#endif 

#if defined(coshl) || !defined(_lib_coshl)
#   ifndef coshl
#       define coshl(x) macdef(cosh,x)
#   endif
    fundef(coshl)
#   undef coshl
#   define coshl local_coshl
#else
#   if defined(_npt_coshl)
	protodef(coshl)
#   endif
#endif 

#if defined(hypotl) || !defined(_lib_hypotl)
#   ifndef hypotl
#       define hypotl(x,y) macdef2(hypot,x,y)
#   endif
    fundef2(hypotl)
#   undef hypotl
#   define hypotl local_hypotl
#else
#   if defined(_npt_hypotl)
	protodef2(hypotl)
#   endif
#endif 

#if defined(floorl) || !defined(_lib_floorl)
#   ifndef floorl
#       define floorl(x) macdef(floor,x)
#   endif
    fundef(floorl)
#   undef floorl
#   define floorl local_floorl
#else
#   if defined(_npt_floorl)
	protodef(floorl)
#   endif
#endif 

#if defined(fmodl) || !defined(_lib_fmodl)
#   ifndef fmodl
#       define fmodl(x,y) macdef2(fmod,x,y)
#   endif
    fundef2(fmodl)
#   undef fmodl
#   define fmodl local_fmodl
#else
#   if defined(_npt_fmodl)
	protodef(fmodl)
#   endif
#endif 

#if defined(logl) || !defined(_lib_logl)
#   ifndef logl
#       define logl(x) macdef(log,x)
#   endif
    fundef(logl)
#   undef logl
#   define logl local_logl
#else
#   if defined(_npt_logl)
	protodef(logl)
#   endif
#endif 

#if defined(sinl) || !defined(_lib_sinl)
#   ifndef sinl
#       define sinl(x) macdef(sin,x)
#   endif
    fundef(sinl)
#   undef sinl
#   define sinl local_sinl
#else
#   if defined(_npt_sinl)
	protodef(sinl)
#   endif
#endif 

#if defined(sinhl) || !defined(_lib_sinhl)
#   ifndef sinhl
#       define sinhl(x) macdef(sinh,x)
#   endif
    fundef(sinhl)
#   undef sinhl
#   define sinhl local_sinhl
#else
#   if defined(_npt_sinhl)
	protodef(sinhl)
#   endif
#endif 

#if defined(sqrtl) || !defined(_lib_sqrtl)
#   ifndef sqrtl
#       define sqrtl(x) macdef(sqrt,x)
#   endif
    fundef(sqrtl)
#   undef sqrtl
#   define sqrtl local_sqrtl
#else
#   if defined(_npt_sqrtl)
	protodef(sqrtl)
#   endif
#endif 

#if defined(tanl) || !defined(_lib_tanl)
#   ifndef tanl
#       define tanl(x) macdef(tan,x)
#   endif
    fundef(tanl)
#   undef tanl
#   define tanl local_tanl
#else
#   if defined(_npt_tanl)
	protodef(tanl)
#   endif
#endif 

#if defined(tanhl) || !defined(_lib_tanhl)
#   ifndef tanhl
#       define tanhl(x) macdef(tanh,x)
#   endif
    fundef(tanhl)
#   undef tanhl
#   define tanhl local_tanhl
#else
#   if defined(_npt_tanhl)
	protodef(tanhl)
#   endif
#endif 

#if defined(powl) || !defined(_lib_powl)
#   ifndef powl
#       define powl(x,y) macdef2(pow,x,y)
#   endif
    fundef2(powl)
#   undef powl
#   define powl local_powl
#else
#   if defined(_npt_powl)
	protodef2(powl)
#   endif
#endif 

#if defined(fabsl) || !defined(_lib_fabsl)
#   ifndef fabsl
#       define fabsl(x) macdef(fabs,x)
#   endif
    fundef(fabsl)
#   undef fabsl
#   define fabsl local_fabsl
#else
#   if defined(_npt_fabsl)
	protodef(fabsl)
#   endif
#endif 


#if 0 /* proto bug workaround */
}
#endif

#endif


const struct mathtab shtab_math[] =
{
	"\01abs",		(mathf)fabsl,
	"\01acos",		(mathf)acosl,
	"\01asin",		(mathf)asinl,
	"\01atan",		(mathf)atanl,
	"\02atan2",		(mathf)atan2l,
	"\01cos",		(mathf)cosl,
	"\01cosh",		(mathf)coshl,
	"\01exp",		(mathf)expl,
	"\01floor",		(mathf)floorl,
	"\02fmod",		(mathf)fmodl,
	"\02hypot",		(mathf)hypotl,
	"\01int",		(mathf)floorl,
	"\01log",		(mathf)logl,
	"\02pow",		(mathf)powl,
	"\01sin",		(mathf)sinl,
	"\01sinh",		(mathf)sinhl,
	"\01sqrt",		(mathf)sqrtl,
	"\01tan",		(mathf)tanl,	
	"\01tanh",		(mathf)tanhl,
	"",			(mathf)0 
};
