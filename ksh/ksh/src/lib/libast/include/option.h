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
 * command line option parse interface
 */

#ifndef _OPTION_H
#define _OPTION_H

#include <ast.h>

#define OPT_VERSION	20000401L

#define opt_info	_opt_info_

#define OPT_USER	(1L<<16)	/* first user flag bit		*/

struct Opt_s;
struct Optdisc_s;

typedef int (*Optinfo_f)(struct Opt_s*, Sfio_t*, const char*, struct Optdisc_s*);

typedef struct Optdisc_s
{
	unsigned long	version;	/* OPT_VERSION			*/
	unsigned long	flags;		/* OPT_* flags			*/
	char*		catalog;	/* error catalog id		*/
	Optinfo_f	infof;		/* runtime info function	*/
} Optdisc_t;

/* NOTE: Opt_t member order fixed by a previous binary release */

#ifndef _OPT_PRIVATE_
#define _OPT_PRIVATE_	\
	char		pad[3*sizeof(void*)];
#endif

typedef struct Opt_s
{
	int		again;		/* see optjoin()		*/
	char*		arg;		/* {:,#} string argument	*/
	char**		argv;		/* most recent argv		*/
	int		index;		/* argv index			*/
	char*		msg;		/* error/usage message buffer	*/
	long		num;		/* OBSOLETE -- use number	*/
	int		offset;		/* char offset in argv[index]	*/
	char		option[8];	/* current flag {-,+} + option  */
	char		name[64];	/* current long name or flag	*/
	Optdisc_t*	disc;		/* user discipline		*/
	_ast_intmax_t	number;		/* # numeric argument		*/
	unsigned char	assignment;	/* option arg assigment op	*/
	unsigned char	pads[sizeof(void*)-1];
	_OPT_PRIVATE_
} Opt_t;

#if _BLD_ast && defined(__EXPORT__)
#define extern		extern __EXPORT__
#endif
#if !_BLD_ast && defined(__IMPORT__)
#define extern		extern __IMPORT__
#endif

extern Opt_t		opt_info;

#undef	extern

#define optinit(d,f)	(memset(d,0,sizeof(*(d))),(d)->version=OPT_VERSION,(d)->infof=(f),opt_info.disc=(d))

#if _BLD_ast && defined(__EXPORT__)
#define extern		__EXPORT__
#endif

extern int		optget(char**, const char*);
extern int		optjoin(char**, ...);
extern char*		opthelp(const char*, const char*);
extern char*		optusage(const char*);
extern int		optstr(const char*, const char*);
extern int		optesc(Sfio_t*, const char*);

#undef	extern

#endif
