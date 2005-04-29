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
#pragma prototyped
/*
 * Glenn Fowler
 * AT&T Research
 *
 * generate limits features
 *
 *	FOPEN_MAX	POSIX says ANSI defines it but it's not in ANSI
 *
 * NOTE: two's complement binary integral representation assumed
 */

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:hide getpagesize getdtablesize printf
#else
#define getpagesize	______getpagesize
#define getdtablesize	______getdtablesize
#define printf		______printf
#endif

/*
 * we'd like as many symbols as possible defined
 * the standards push the vendors the other way
 * but don't provide guard that lets everything through
 * so each vendor adds their own guard
 * many now include something like <standards.h> to
 * get it straight in one place -- <sys/types.h> should
 * kick that in
 */

#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE	1
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE	1
#endif
#ifndef __EXTENSIONS__
#define __EXTENSIONS__	1
#endif
#ifdef __sun
#define _timespec	timespec
#endif

#include <sys/types.h>

#undef	_SGIAPI
#define _SGIAPI		1

#include "FEATURE/limits.lcl"

#undef	_SGIAPI
#define _SGIAPI		0

#include "FEATURE/lib"
#include "FEATURE/common"
#include "FEATURE/unistd.lcl"
#include "FEATURE/param"

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:nohide getpagesize getdtablesize printf
#else
#undef	getpagesize
#undef	getdtablesize   
#endif

#if defined(__STDPP__hide) || defined(printf)
#undef	printf
extern int		printf(const char*, ...);
#endif

#include "conflib.h"

main()
{
	char			c;
	unsigned char		uc;
	unsigned short		us;
	unsigned int		ui;
	unsigned long		ul;
	unsigned long		val;
#ifdef _ast_int8_t
	unsigned _ast_int8_t	ull;
	unsigned _ast_int8_t	vll;
#endif

	/*
	 * <limits.h> with *constant* valued macros
	 */

	printf("\n");
#ifdef CHAR_BIT
	val = CHAR_BIT;
	printf("#undef	CHAR_BIT\n");
#else
	uc = 0;
	uc = ~uc;
	val = 1;
	while (uc >>= 1) val++;
#endif
	printf("#define CHAR_BIT	%lu\n", val);
#ifdef MB_LEN_MAX
	val = MB_LEN_MAX;
	printf("#undef	MB_LEN_MAX\n");
#else
	val = 1;
#endif
	printf("#define MB_LEN_MAX	%lu\n", val);

	c = 0;
	c = ~c;
	uc = 0;
	uc = ~uc;
	us = 0;
	us = ~us;
	ui = 0;
	ui = ~ui;
	ul = 0;
	ul = ~ul;
#ifdef _ast_int8_t
	ull = 0;
	ull = ~ull;
#endif

#ifdef UCHAR_MAX
	val = UCHAR_MAX;
	printf("#undef	UCHAR_MAX\n");
#else
	val = uc;
#endif
	printf("#if defined(__STDC__)\n");
	printf("#define UCHAR_MAX	%luU\n", val);
	printf("#else\n");
	printf("#define UCHAR_MAX	%lu\n", val);
	printf("#endif\n");

#ifdef SCHAR_MIN
	val = -(unsigned long)(SCHAR_MIN);
	printf("#undef	SCHAR_MIN\n");
#else
	val = (unsigned char)(uc >> 1) + 1;
#endif
	printf("#define SCHAR_MIN	(-%lu)\n", val);

#ifdef SCHAR_MAX
	val = SCHAR_MAX;
	printf("#undef	SCHAR_MAX\n");
#else
	val = (unsigned char)(uc >> 1);
#endif
	printf("#define SCHAR_MAX	%lu\n", val);

	if (c < 0)
	{
#ifdef CHAR_MIN
		printf("#undef	CHAR_MIN\n");
#endif
		printf("#define CHAR_MIN	SCHAR_MIN\n");

#ifdef CHAR_MAX
		printf("#undef	CHAR_MAX\n");
#endif
		printf("#define CHAR_MAX	SCHAR_MAX\n");
	}
	else
	{
#ifdef CHAR_MIN
		printf("#undef	CHAR_MIN\n");
#endif
		printf("#define CHAR_MIN	0\n");

#ifdef CHAR_MAX
		printf("#undef	CHAR_MAX\n");
#endif
		printf("#define CHAR_MAX	UCHAR_MAX\n");
	}

#ifdef USHRT_MAX
	val = USHRT_MAX;
	printf("#undef	USHRT_MAX\n");
#else
	val = us;
#endif
	printf("#if defined(__STDC__)\n");
	printf("#define USHRT_MAX	%luU\n", val);
	printf("#else\n");
	printf("#define USHRT_MAX	%lu\n", val);
	printf("#endif\n");

#ifdef SHRT_MIN
	val = -(unsigned long)(SHRT_MIN);
	printf("#undef	SHRT_MIN\n");
#else
	val = (unsigned short)(us >> 1) + 1;
#endif
	printf("#define SHRT_MIN	(-%lu)\n", val);

#ifdef SHRT_MAX
	val = SHRT_MAX;
	printf("#undef	SHRT_MAX\n");
#else
	val = (unsigned short)(us >> 1);
#endif
	printf("#define SHRT_MAX	%lu\n", val);

	if (ui == us)
	{
#ifdef UINT_MAX
		printf("#undef	UINT_MAX\n");
#endif
		printf("#define UINT_MAX	USHRT_MAX\n");

#ifdef INT_MIN
		printf("#undef	INT_MIN\n");
#endif
		printf("#define INT_MIN		SHRT_MIN\n");

#ifdef INT_MAX
		printf("#undef	INT_MAX\n");
#endif
		printf("#define INT_MAX		SHRT_MAX\n");
	}
	else
	{
#ifdef UINT_MAX
		val = UINT_MAX;
		printf("#undef	UINT_MAX\n");
#else
		val = ui;
#endif
		printf("#if defined(__STDC__)\n");
		printf("#define UINT_MAX	%luU\n", val);
		printf("#else\n");
		printf("#define UINT_MAX	%lu\n", val);
		printf("#endif\n");

#ifdef INT_MIN
		val = -(unsigned long)(INT_MIN);
		printf("#undef	INT_MIN\n");
#else
		val = (unsigned int)(ui >> 1) + 1;
#endif
		if (ui == ul) printf("#define INT_MIN		(-%lu-1)\n", val - 1);
		else printf("#define INT_MIN		(-%lu)\n", val);

#ifdef INT_MAX
		val = INT_MAX;
		printf("#undef	INT_MAX\n");
#else
		val = (unsigned int)(ui >> 1);
#endif
		printf("#define INT_MAX		%lu\n", val);
	}

	if (ul == ui)
	{
#ifdef ULONG_MAX
		printf("#undef	ULONG_MAX\n");
#endif
		printf("#define ULONG_MAX	UINT_MAX\n");

#ifdef LONG_MIN
		printf("#undef	LONG_MIN\n");
#endif
		printf("#define LONG_MIN	INT_MIN\n");

#ifdef LONG_MAX
		printf("#undef	LONG_MAX\n");
#endif
		printf("#define LONG_MAX	INT_MAX\n");
	}
	else
	{
#ifdef ULONG_MAX
		val = ULONG_MAX;
		printf("#undef	ULONG_MAX\n");
#else
		val = ul;
#endif
		printf("#if defined(__STDC__)\n");
		printf("#define ULONG_MAX	%luLU\n", val);
		printf("#else\n");
		printf("#define ULONG_MAX	%lu\n", val);
		printf("#endif\n");

#ifdef LONG_MIN
		val = -(unsigned long)(LONG_MIN);
		printf("#undef	LONG_MIN\n");
#else
		val = (unsigned long)(ul >> 1) + 1;
#endif
		printf("#define LONG_MIN	(-%luL-1L)\n", val - 1);

#ifdef LONG_MAX
		val = LONG_MAX;
		printf("#undef	LONG_MAX\n");
#else
		val = (unsigned long)(ul >> 1);
#endif
		printf("#define LONG_MAX	%luL\n", val);
	}

#ifdef _ast_int8_t
	if (ull == ul)
	{
#ifdef ULONGLONG_MAX
		printf("#undef	ULONGLONG_MAX\n");
#endif
		printf("#define ULONGLONG_MAX	ULONG_MAX\n");

#ifdef LONGLONG_MIN
		printf("#undef	LONGLONG_MIN\n");
#endif
		printf("#define LONGLONG_MIN	LONG_MIN\n");

#ifdef LONGLONG_MAX
		printf("#undef	LONGLONG_MAX\n");
#endif
		printf("#define LONGLONG_MAX	LONG_MAX\n");
	}
	else
	{
#ifdef ULONGLONG_MAX
		vll = ULONGLONG_MAX;
		printf("#undef	ULONGLONG_MAX\n");
#else
		vll = ull;
#endif
		printf("#if defined(__STDC__) && _ast_LL\n");
		printf("#define ULONGLONG_MAX	%lluLLU\n", vll);
		printf("#else\n");
		printf("#define ULONGLONG_MAX	%llu\n", vll);
		printf("#endif\n");

#ifdef LONGLONG_MIN
		vll = -(unsigned _ast_int8_t)(LONGLONG_MIN);
		printf("#undef	LONGLONG_MIN\n");
#else
		vll = (unsigned _ast_int8_t)(ull >> 1) + 1;
#endif
		printf("#if defined(__STDC__) && _ast_LL\n");
		printf("#define LONGLONG_MIN	(-%lluLL-1LL)\n", vll - 1);
		printf("#else\n");
		printf("#define LONGLONG_MIN	(-%llu-1)\n", vll - 1);
		printf("#endif\n");

#ifdef LONGLONG_MAX
		vll = LONGLONG_MAX;
		printf("#undef	LONGLONG_MAX\n");
#else
		vll = (unsigned _ast_int8_t)(ull >> 1);
#endif
		printf("#if defined(__STDC__) && _ast_LL\n");
		printf("#define LONGLONG_MAX	%lluLL\n", vll);
		printf("#else\n");
		printf("#define LONGLONG_MAX	%llu\n", vll);
		printf("#endif\n");
	}
#endif

	printf("\n");
#include "conflim.h"
	printf("\n");
#ifdef _UWIN
	printf("#ifdef _UWIN\n");
	printf("#ifndef DBL_DIG\n");
	printf("#define DBL_DIG		15\n");
	printf("#endif\n");
	printf("#ifndef DBL_MAX\n");
	printf("#define DBL_MAX		1.7976931348623158e+308\n");
	printf("#endif\n");
	printf("#ifndef FLT_DIG\n");
	printf("#define FLT_DIG		6\n");
	printf("#endif\n");
	printf("#ifndef FLT_MAX\n");
	printf("#define FLT_MAX		3.402823466e+38F\n");
	printf("#endif\n");
	printf("#endif\n");
	printf("\n");
#endif

	/*
	 * pollution control
	 */

	printf("/*\n");
	printf(" * pollution control\n");
	printf(" */\n");
	printf("\n");
	printf("#if defined(__STDPP__directive) && defined(__STDPP__ignore)\n");
	printf("__STDPP__directive pragma pp:ignore \"limits.h\"\n");
	printf("__STDPP__directive pragma pp:ignore \"bits/posix1_lim.h\"\n");
	printf("#else\n");
#ifdef	_limits_h
	printf("#define _limits_h\n");
#endif
#ifdef	__limits_h
	printf("#define __limits_h\n");
#endif
#ifdef	_sys_limits_h
	printf("#define _sys_limits_h\n");
#endif
#ifdef	__sys_limits_h
	printf("#define __sys_limits_h\n");
#endif
#ifdef	_BITS_POSIX1_LIM_H
	printf("#ifndef _BITS_POSIX1_LIM_H\n");
	printf("#define _BITS_POSIX1_LIM_H\n");
	printf("#endif\n");
#endif
#ifdef	_LIMITS_H_
	printf("#define _LIMITS_H_\n");
#endif
#ifdef	_LIMITS_H__
	printf("#define _LIMITS_H__\n");
#endif
#ifdef	_LIMITS_H___
	printf("#define _LIMITS_H___\n");
#endif
#ifdef	__LIMITS_H
	printf("#define __LIMITS_H\n");
#endif
#ifdef	__LIMITS_INCLUDED
	printf("#define __LIMITS_INCLUDED\n");
#endif
#ifdef	_MACH_MACHLIMITS_H_
	printf("#define _MACH_MACHLIMITS_H_\n");
#endif
#ifdef	_MACHINE_LIMITS_H_
	printf("#define _MACHINE_LIMITS_H_\n");
#endif
#ifdef	_SYS_LIMITS_H_
	printf("#define _SYS_LIMITS_H_\n");
#endif
#ifdef	__SYS_LIMITS_H
	printf("#define __SYS_LIMITS_H\n");
#endif
#ifdef	__SYS_LIMITS_INCLUDED
	printf("#define __SYS_LIMITS_INCLUDED\n");
#endif
#ifdef	_SYS_SYSLIMITS_H_
	printf("#define _SYS_SYSLIMITS_H_\n");
#endif
#ifdef	_H_LIMITS
	printf("#define _H_LIMITS\n");
#endif
#ifdef	__H_LIMITS
	printf("#define __H_LIMITS\n");
#endif
#ifdef	_H_SYS_LIMITS
	printf("#define _H_SYS_LIMITS\n");
#endif
#ifdef	__H_SYS_LIMITS
	printf("#define __H_SYS_LIMITS\n");
#endif
	printf("#endif\n");
	printf("\n");

	return(0);
}
