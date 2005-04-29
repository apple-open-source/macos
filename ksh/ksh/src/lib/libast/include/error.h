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
 * option, error and message formatter external definitions
 */

#ifndef _ERROR_H
#define _ERROR_H

#include <ast.h>
#include <option.h>
#include <errno.h>

#define ERROR_VERSION	20030214L

#ifndef	error_info
#define error_info	_error_info_
#endif

#if !defined(errno) && defined(__DYNAMIC__)
#define errno		__DYNAMIC__(errno)
#endif

#define ERROR_debug(n)	(-(n))
#define ERROR_exit(n)	((n)+ERROR_ERROR)
#define ERROR_system(n)	(((n)+ERROR_ERROR)|ERROR_SYSTEM)
#define ERROR_usage(n)	((((n)?2:0)+ERROR_ERROR)|ERROR_USAGE)
#define ERROR_warn(n)	(ERROR_WARNING)

#ifndef ERROR_catalog
#define ERROR_catalog(t)		t
#endif
#ifndef ERROR_dictionary
#define ERROR_dictionary(t)		t
#endif

#ifndef ERROR_translate
#define ERROR_translating()		(error_info.translate&&(ast.locale.set&(1<<AST_LC_MESSAGES)))
#define ERROR_translate(l,i,d,m)	(ERROR_translating()?errorx((const char*)l,(const char*)i,(const char*)d,(const char*)m):(m))
#endif

#define ERROR_INFO	0		/* info message -- no err_id	*/
#define ERROR_WARNING	1		/* warning message		*/
#define ERROR_ERROR	2		/* error message -- no err_exit	*/
#define ERROR_FATAL	3		/* error message with err_exit	*/
#define ERROR_NOEXEC	EXIT_NOEXEC	/* shell convention		*/
#define ERROR_NOENT	EXIT_NOTFOUND	/* shell convention		*/
#define ERROR_PANIC	ERROR_LEVEL	/* panic message with err_exit	*/

#define ERROR_LEVEL	0x00ff		/* level portion of status	*/
#define ERROR_SYSTEM	0x0100		/* report system errno message	*/
#define ERROR_OUTPUT	0x0200		/* next arg is error fd		*/
#define ERROR_SOURCE	0x0400		/* next 2 args are FILE,LINE	*/
#define ERROR_USAGE	0x0800		/* usage message		*/
#define ERROR_PROMPT	0x1000		/* omit trailing newline	*/
#define ERROR_NOID	0x2000		/* omit err_id			*/
#define ERROR_LIBRARY	0x4000		/* library routine error	*/

#define ERROR_INTERACTIVE	0x0001	/* context is interactive	*/
#define ERROR_SILENT		0x0002	/* context is silent		*/
#define ERROR_NOTIFY		0x0004	/* main(-sig,0,ctx) on signal	*/

#define errorpush(p,f)	(*(p)=*ERROR_CONTEXT_BASE,*ERROR_CONTEXT_BASE=error_info.empty,error_info.context=(p),error_info.flags=(f))
#define errorpop(p)	(*ERROR_CONTEXT_BASE=*(p))

#define ERROR_CONTEXT_BASE	((Error_context_t*)&error_info.context)

#define ERROR_CONTEXT \
	Error_context_t* context;	/* prev context stack element	*/ \
	int	errors;			/* >= ERROR_ERROR count		*/ \
	int	flags;			/* context flags		*/ \
	int	line;			/* input|output line number	*/ \
	int	warnings;		/* ERROR_WARNING count		*/ \
	char*	file;			/* input|output file name	*/ \
	char*	id;			/* command id			*/

#define errorcontext Error_context_s	/* compatibility til 2004	*/

typedef struct Error_context_s Error_context_t;

struct Error_context_s			/* context stack element	*/
{
	ERROR_CONTEXT
};

typedef struct Error_info_s		/* error state			*/
{
	int	fd;			/* write(2) fd			*/

	void	(*exit)(int);		/* error exit			*/
	ssize_t	(*write)(int, const void*, size_t); /* error output	*/

	/* the rest are implicitly initialized				*/

	int	clear;			/* default clear ERROR_* flags	*/
	int	core;			/* level>=core -> core dump	*/
	int	indent;			/* debug trace indent level	*/
	int	init;			/* initialized			*/
	int	last_errno;		/* last reported errno		*/
	int	mask;			/* multi level debug trace mask	*/
	int	set;			/* default set ERROR_* flags	*/
	int	trace;			/* debug trace level		*/

	char*	version;		/* ERROR_SOURCE command version	*/

	int	(*auxilliary)(Sfio_t*, int, int); /* aux info to append	*/

	ERROR_CONTEXT			/* top of context stack		*/

	Error_context_t	empty;		/* empty context stack element	*/

	unsigned long	time;		/* debug time trace		*/

	char*	(*translate)(const char*, const char*, const char*, const char*);	/* format translator */
	const char*	catalog;	/* default message catalog	*/

} Error_info_t;

#ifndef errno
extern int	errno;			/* system call error status	*/
#endif

#if _BLD_ast && defined(__EXPORT__)
#define extern		extern __EXPORT__
#endif
#if !_BLD_ast && defined(__IMPORT__)
#define extern		extern __IMPORT__
#endif

extern Error_info_t	error_info;

#undef	extern

#if _BLD_ast && defined(__EXPORT__)
#define extern		__EXPORT__
#endif

extern void	error(int, ...);
extern int	errormsg(const char*, int, ...);
extern int	errorf(void*, void*, int, ...);
extern void	errorv(const char*, int, va_list);
extern char*	errorx(const char*, const char*, const char*, const char*);

extern void	liberror(const char*, int, ...);	/* OBSOLETE 20000101 */
extern int	libevent(void*, void*, int, ...);	/* OBSOLETE 19990101 */

#undef	extern

#endif
