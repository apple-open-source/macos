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
 * Advanced Software Technology Library
 * AT&T Research
 *
 * std + posix + ast
 */

#ifndef _AST_H
#define _AST_H

#include <ast_version.h>

#ifndef _AST_STD_H
#include <ast_std.h>
#endif

#ifndef _SFIO_H
#include <sfio.h>
#endif

#ifndef	ast
#define ast		_ast_info
#endif

#ifndef PATH_MAX
#define PATH_MAX	1024
#endif

/*
 * workaround botched headers that assume <stdio.h>
 */

#ifndef FILE
#ifndef _SFIO_H
struct _sfio_s;
#endif
#define FILE		struct _sfio_s
#ifndef	__FILE_typedef
#define __FILE_typedef	1
#endif
#endif

/*
 * exit() support -- this matches shell exit codes
 */

#define EXIT_BITS	8			/* # exit status bits	*/

#define EXIT_USAGE	2			/* usage exit code	*/
#define EXIT_QUIT	((1<<(EXIT_BITS))-1)	/* parent should quit	*/
#define EXIT_NOTFOUND	((1<<(EXIT_BITS-1))-1)	/* command not found	*/
#define EXIT_NOEXEC	((1<<(EXIT_BITS-1))-2)	/* other exec error	*/

#define EXIT_CODE(x)	((x)&((1<<EXIT_BITS)-1))
#define EXIT_CORE(x)	(EXIT_CODE(x)|(1<<EXIT_BITS)|(1<<(EXIT_BITS-1)))
#define EXIT_TERM(x)	(EXIT_CODE(x)|(1<<EXIT_BITS))

/*
 * NOTE: for compatibility the following work for EXIT_BITS={7,8}
 */

#define EXIT_STATUS(x)	(((x)&((1<<(EXIT_BITS-2))-1))?(x):EXIT_CODE((x)>>EXIT_BITS))

#define EXITED_CORE(x)	(((x)&((1<<EXIT_BITS)|(1<<(EXIT_BITS-1))))==((1<<EXIT_BITS)|(1<<(EXIT_BITS-1)))||((x)&((1<<(EXIT_BITS-1))|(1<<(EXIT_BITS-2))))==((1<<(EXIT_BITS-1))|(1<<(EXIT_BITS-2))))
#define EXITED_TERM(x)	((x)&((1<<EXIT_BITS)|(1<<(EXIT_BITS-1))))

/*
 * astconflist() flags
 */

#define ASTCONF_parse		0x001
#define ASTCONF_write		0x002
#define ASTCONF_read		0x004
#define ASTCONF_lower		0x008
#define ASTCONF_base		0x010
#define ASTCONF_defined		0x020
#define ASTCONF_quote		0x040
#define ASTCONF_table		0x080
#define ASTCONF_matchcall	0x100
#define ASTCONF_matchname	0x200
#define ASTCONF_matchstandard	0x400

/*
 * pathcanon() flags
 */

#define PATH_PHYSICAL	01
#define PATH_DOTDOT	02
#define PATH_EXISTS	04
#define PATH_VERIFIED(n) (((n)&01777)<<5)

/*
 * pathaccess() flags
 */

#define PATH_READ	004
#define PATH_WRITE	002
#define PATH_EXECUTE	001
#define	PATH_REGULAR	010
#define PATH_ABSOLUTE	020

/*
 * touch() flags
 */

#define PATH_TOUCH_CREATE	01
#define PATH_TOUCH_VERBATIM	02

/*
 * pathcheck() info
 */

typedef struct
{
	unsigned long	date;
	char*		feature;
	char*		host;
	char*		user;
} Pathcheck_t;

/*
 * strgrpmatch() flags
 */

#define STR_MAXIMAL	01		/* maximal match		*/
#define STR_LEFT	02		/* implicit left anchor		*/
#define STR_RIGHT	04		/* implicit right anchor	*/
#define STR_ICASE	010		/* ignore case			*/
#define STR_GROUP	020		/* (|&) inside [@|&](...) only	*/

/*
 * fmtquote() flags
 */

#define FMT_ALWAYS	0x01		/* always quote			*/
#define FMT_ESCAPED	0x02		/* already escaped		*/
#define FMT_SHELL	0x04		/* escape $ ` too		*/
#define FMT_WIDE	0x08		/* don't escape 8 bit chars	*/

/*
 * multibyte macros
 */

#define mbmax()		(ast.mb_cur_max)
#define mberr()		(ast.tmp_int<0)

#define mbcoll()	(ast.mb_xfrm!=0)
#define mbwide()	(mbmax()>1)

#define mbchar(p)	(mbwide()?((ast.tmp_int=(*ast.mb_towc)(&ast.tmp_wchar,(char*)(p),mbmax()))>0?((p+=ast.tmp_int),ast.tmp_wchar):(p++,ast.tmp_int)):(*(unsigned char*)(p++)))
#define mbinit()	(mbwide()?(*ast.mb_towc)((wchar_t*)0,(char*)0,mbmax()):0)
#define mbsize(p)	(mbwide()?(*ast.mb_len)((char*)(p),mbmax()):((p),1))
#define mbconv(s,w)	(ast.mb_conv?(*ast.mb_conv)(s,w):((*(s)=(w)),1))
#define mbwidth(w)	(ast.mb_width&&((ast.tmp_int=(*ast.mb_width)(w))>=0||(w)>UCHAR_MAX)?ast.tmp_int:1)
#define mbxfrm(t,f,n)	(mbcoll()?(*ast.mb_xfrm)((char*)(t),(char*)(f),n):0)

/*
 * common macros
 */

#define elementsof(x)	(sizeof(x)/sizeof(x[0]))
#define newof(p,t,n,x)	((p)?(t*)realloc((char*)(p),sizeof(t)*(n)+(x)):(t*)calloc(1,sizeof(t)*(n)+(x)))
#define oldof(p,t,n,x)	((p)?(t*)realloc((char*)(p),sizeof(t)*(n)+(x)):(t*)malloc(sizeof(t)*(n)+(x)))
#define roundof(x,y)	(((x)+(y)-1)&~((y)-1))
#define ssizeof(x)	((int)sizeof(x))
#define streq(a,b)	(*(a)==*(b)&&!strcmp(a,b))
#define strneq(a,b,n)	(*(a)==*(b)&&!strncmp(a,b,n))
#define strsignal(s)	fmtsignal(s)

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

#define NOT_USED(x)	NoP(x)

typedef int (*Error_f)(void*, void*, int, ...);

typedef int (*Ast_confdisc_f)(const char*, const char*, const char*);
typedef int (*Strcmp_context_f)(const char*, const char*, void*);
typedef int (*Strcmp_f)(const char*, const char*);

#if _BLD_ast && defined(__EXPORT__)
#define extern		__EXPORT__
#endif

extern char*		astgetconf(const char*, const char*, const char*, Error_f);
extern char*		astconf(const char*, const char*, const char*);
extern Ast_confdisc_f	astconfdisc(Ast_confdisc_f);
extern void		astconflist(Sfio_t*, const char*, int, const char*);
extern off_t		astcopy(int, int, off_t);
extern int		astlicense(char*, int, char*, char*, int, int, int);
extern int		astquery(int, const char*, ...);
extern void		astwinsize(int, int*, int*);

extern ssize_t		base64encode(const void*, size_t, void**, void*, size_t, void**);
extern ssize_t		base64decode(const void*, size_t, void**, void*, size_t, void**);
extern int		chresc(const char*, char**);
extern int		chrtoi(const char*);
extern char*		fmtbase(long, int, int);
extern char*		fmtbasell(_ast_intmax_t, int, int);
#define fmtbase(a,b,c)	fmtbasell((_ast_intmax_t)(a),b,c) /* until 2003-09-01 */
extern char*		fmtbuf(size_t);
extern char*		fmtclock(Sfulong_t);
extern char*		fmtelapsed(unsigned long, int);
extern char*		fmterror(int);
extern char*		fmtesc(const char*);
extern char*		fmtesq(const char*, const char*);
extern char*		fmtident(const char*);
extern char*		fmtip4(unsigned _ast_int4_t, int);
extern char*		fmtfmt(const char*);
extern char*		fmtgid(int);
extern char*		fmtmatch(const char*);
extern char*		fmtmode(int, int);
extern char*		fmtnesq(const char*, const char*, size_t);
extern char*		fmtnum(unsigned long, int);
extern char*		fmtperm(int);
extern char*		fmtquote(const char*, const char*, const char*, size_t, int);
extern char*		fmtre(const char*);
extern char*		fmtscale(Sfulong_t, int);
extern char*		fmtsignal(int);
extern char*		fmttime(const char*, time_t);
extern char*		fmtuid(int);
extern char*		fmtversion(unsigned long);
extern void*		memdup(const void*, size_t);
extern void		memfatal(void);
extern unsigned int	memhash(const void*, int);
extern unsigned long	memsum(const void*, int, unsigned long);
extern char*		pathaccess(char*, const char*, const char*, const char*, int);
extern char*		pathbin(void);
extern char*		pathcanon(char*, int);
extern char*		pathcat(char*, const char*, int, const char*, const char*);
extern int		pathcd(const char*, const char*);
extern int		pathcheck(const char*, const char*, Pathcheck_t*);
extern int		pathexists(char*, int);
extern char*		pathfind(const char*, const char*, const char*, char*, size_t);
extern int		pathgetlink(const char*, char*, int);
extern int		pathinclude(const char*);
extern char*		pathkey(char*, char*, const char*, const char*, const char*);
extern size_t		pathnative(const char*, char*, size_t);
extern char*		pathpath(char*, const char*, const char*, int);
extern size_t		pathposix(const char*, char*, size_t);
extern char*		pathprobe(char*, char*, const char*, const char*, const char*, int);
extern char*		pathrepl(char*, const char*, const char*);
extern int		pathsetlink(const char*, const char*);
extern char*		pathshell(void);
extern char*		pathtemp(char*, size_t, const char*, const char*, int*);
extern char*		pathtmp(char*, const char*, const char*, int*);
extern char*		setenviron(const char*);
extern int		stracmp(const char*, const char*);
extern char*		strcopy(char*, const char*);
extern unsigned long	strelapsed(const char*, char**, int);
extern int		stresc(char*);
extern long		streval(const char*, char**, long(*)(const char*, char**));
extern long		strexpr(const char*, char**, long(*)(const char*, char**, void*), void*);
extern int		strgid(const char*);
extern int		strgrpmatch(const char*, const char*, int*, int, int);
extern unsigned int	strhash(const char*);
extern void*		strlook(const void*, size_t, const char*);
extern int		strmatch(const char*, const char*);
extern int		strmode(const char*);
extern int		strnacmp(const char*, const char*, size_t);
extern char*		strncopy(char*, const char*, size_t);
extern double		strntod(const char*, size_t, char**);
extern _ast_fltmax_t	strntold(const char*, size_t, char**);
extern long		strntol(const char*, size_t, char**, int);
extern _ast_intmax_t	strntoll(const char*, size_t, char**, int);
extern unsigned long	strntoul(const char*, size_t, char**, int);
extern unsigned _ast_intmax_t	strntoull(const char*, size_t, char**, int);
extern int		stropt(const char*, const void*, int, int(*)(void*, const void*, int, const char*), void*);
extern int		strperm(const char*, char**, int);
extern void*		strpsearch(const void*, size_t, size_t, const char*, char**);
extern void*		strsearch(const void*, size_t, size_t, Strcmp_f, const char*, void*);
extern void		strsort(char**, int, int(*)(const char*, const char*));
extern char*		strsubmatch(const char*, const char*, int);
extern unsigned long	strsum(const char*, unsigned long);
extern char*		strtape(const char*, char**);
extern int		strtoip4(const char*, char**, unsigned _ast_int4_t*, unsigned char*);
extern long		strton(const char*, char**, char*, int);
extern _ast_intmax_t	strtonll(const char*, char**, char*, int);
extern int		struid(const char*);
extern int		struniq(char**, int);

#undef			extern

/*
 * C library global data symbols not prototyped by <unistd.h>
 */

#if !defined(environ) && defined(__DYNAMIC__)
#define	environ		__DYNAMIC__(environ)
#else
extern char**		environ;
#endif

/*
 * really handy malloc()/free() (__FILE__,__LINE__,__FUNCTION__) tracing
 * make with VMDEBUG==1 or debug=1 or CCFLAGS=$(CC.DEBUG)
 * VMDEBUG==0 disables
 * at runtime export VMDEBUG or VMTRACE per vmalloc.3
 * to list originating call locations
 */

#if !_std_malloc && !defined(VMFL) && !defined(_VMHDR_H) && \
	(!defined(VMDEBUG) || VMDEBUG) && (VMDEBUG || _BLD_DEBUG)

#define VMFL	1
#include <vmalloc.h>

#if defined(__STDPP__directive) && defined(__STDPP__ignore)

__STDPP__directive pragma pp:ignore "malloc.h"

#else

#ifndef _malloc_h
#define _malloc_h
#endif
#ifndef _malloc_h_
#define _malloc_h_
#endif
#ifndef __malloc_h
#define __malloc_h
#endif
#ifndef __malloc_h__
#define __malloc_h__
#endif
#ifndef _MALLOC_H
#define _MALLOC_H
#endif
#ifndef _MALLOC_H_
#define _MALLOC_H_
#endif
#ifndef __MALLOC_H
#define __MALLOC_H
#endif
#ifndef __MALLOC_H__
#define __MALLOC_H__
#endif
#ifndef _MALLOC_INCLUDED
#define _MALLOC_INCLUDED
#endif
#ifndef __MALLOC_INCLUDED
#define __MALLOC_INCLUDED
#endif
#ifndef _H_MALLOC
#define _H_MALLOC
#endif
#ifndef __H_MALLOC
#define __H_MALLOC
#endif

#endif

#endif

#endif
