/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1992-2004 AT&T Corp.                *
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
*                                                                  *
*******************************************************************/
#pragma prototyped
/*
 * David Korn
 * AT&T Bell Laboratories
 *
 * header for wc library interface
 */

#ifndef _WC_H
#define _WC_H

#include <ast.h>

#define WC_LINES	1
#define WC_WORDS	2
#define WC_CHARS	4
#define WC_MBYTE	8

typedef struct
{
	signed char space[1<<CHAR_BIT];
	Sfoff_t words;
	Sfoff_t lines;
	Sfoff_t chars;
} Wc_t;

#define wc_count	_cmd_wccount
#define wc_init		_cmd_wcinit

extern Wc_t*		wc_init(char*);
extern int		wc_count(Wc_t*, Sfio_t*);

#endif /* _WC_H */
