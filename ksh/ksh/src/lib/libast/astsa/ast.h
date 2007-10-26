/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*           Copyright (c) 1985-2007 AT&T Knowledge Ventures            *
*                      and is licensed under the                       *
*                  Common Public License, Version 1.0                  *
*                      by AT&T Knowledge Ventures                      *
*                                                                      *
*                A copy of the License is available at                 *
*            http://www.opensource.org/licenses/cpl1.0.txt             *
*         (with md5 checksum 059e8cd6165cb4c31e351f2b69388fd9)         *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                  David Korn <dgk@research.att.com>                   *
*                   Phong Vo <kpv@research.att.com>                    *
*                                                                      *
***********************************************************************/
#pragma prototyped

/*
 * _PACKAGE_astsa <ast.h>
 */

#ifndef _PACKAGE_astsa
#define _PACKAGE_astsa		1

#include <ast_common.h>
#include <stdarg.h>
#include <sfio.h>
#include <limits.h>
#if _hdr_stdlib
#include <stdlib.h>
#endif
#if _hdr_unistd
#include <unistd.h>
#else
extern ssize_t		write(int, const void*, size_t);
#endif

#define elementsof(x)	(sizeof(x)/sizeof(x[0]))
#define newof(p,t,n,x)	((p)?(t*)realloc((char*)(p),sizeof(t)*(n)+(x)):(t*)calloc(1,sizeof(t)*(n)+(x)))
#define oldof(p,t,n,x)	((p)?(t*)realloc((char*)(p),sizeof(t)*(n)+(x)):(t*)malloc(sizeof(t)*(n)+(x)))
#define roundof(x,y)	(((x)+(y)-1)&~((y)-1))
#define ssizeof(x)	((int)sizeof(x))
#define streq(a,b)	(*(a)==*(b)&&!strcmp(a,b))
#define strneq(a,b,n)	(*(a)==*(b)&&!strncmp(a,b,n))

#define ERROR_translating()		0
#define ERROR_translate(a,b,c,s)	errorx(a,b,c,s)
#define errorx(a,b,c,s)			(s)

#define STR_MAXIMAL	01		/* maximal match		*/
#define STR_LEFT	02		/* implicit left anchor		*/
#define STR_RIGHT	04		/* implicit right anchor	*/
#define STR_ICASE	010		/* ignore case			*/
#define STR_GROUP	020		/* (|&) inside [@|&](...) only	*/

#if defined(__STDC__) || defined(__cplusplus) || defined(c_plusplus)
#define NiL		0
#define NoP(x)		(void)(x)
#else
#define NiL		((char*)0)
#define NoP(x)		(&x,1)
#endif

#if !defined(NoF)
#define NoF(x)		void _DATA_ ## x () {}
#if !defined(_DATA_)
#define _DATA_
#endif
#endif

#if !defined(NoN)
#define NoN(x)		void _STUB_ ## x () {}
#if !defined(_STUB_)
#define _STUB_
#endif
#endif

typedef int (*Error_f)(void*, void*, int, ...);

#define strerror	_ast_strerror
#define fmterror	_ast_strerror

#if _BLD_ast && defined(__EXPORT__)
#define extern		extern __EXPORT__
#endif

extern char*		_ast_strerror(int);
extern void		astwinsize(int, int*, int*);
extern int		strgrpmatch(const char*, const char*, int*, int, int);
extern int		strmatch(const char*, const char*);

#undef	extern

#endif
