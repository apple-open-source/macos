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
#ifndef  _ENV_H
#define	_ENV_H	1

#ifdef _BLD_env
#    ifdef __EXPORT__
#	define export	__EXPORT__
#    endif
#else
     typedef void *Env_t;
#endif

/* for use with env_open */
#define ENV_STABLE	(-1)

/* for third agument to env_add */
#define ENV_MALLOCED	1
#define ENV_STRDUP	2

extern void	env_close(Env_t*);
extern int	env_add(Env_t*, const char*, int);
extern int	env_delete(Env_t*, const char*);
extern char	**env_get(Env_t*);
extern Env_t	*env_open(char**,int);
extern Env_t	*env_scope(Env_t*,int);

#undef extern

#endif


