/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1985-2004 AT&T Corp.                *
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
*               Glenn Fowler <gsf@research.att.com>                *
*                David Korn <dgk@research.att.com>                 *
*                 Phong Vo <kpv@research.att.com>                  *
*                                                                  *
*******************************************************************/
/*
 * types by byte capacity
 */

#ifndef _INT_H
#define _INT_H

#include <ast_common.h>

#ifdef _ast_int1_t
#define int_1		_ast_int1_t
#endif
#ifdef _ast_int2_t
#define int_2		_ast_int2_t
#endif
#ifdef _ast_int4_t
#define int_4		_ast_int4_t
#endif
#ifdef _ast_int8_t
#define int_8		_ast_int8_t
#endif

#define int_max		_ast_intmax_t
#define int_swap	_ast_intswap

#ifdef _ast_flt4_t
#define flt_4		_ast_flt4_t
#endif
#ifdef _ast_flt8_t
#define flt_8		_ast_flt8_t
#endif
#ifdef _ast_flt16_t
#define flt_16		_ast_flt16_t
#endif

#define flt_max		_ast_fltmax_t

#endif
