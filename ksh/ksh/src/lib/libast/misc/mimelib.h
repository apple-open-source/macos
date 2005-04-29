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
 * mime/mailcap internal interface
 */

#ifndef _MIMELIB_H
#define _MIMELIB_H	1

#include <ast.h>
#include <cdt.h>
#include <magic.h>
#include <sfstr.h>
#include <tok.h>

struct Mime_s;

typedef void (*Free_f)(struct Mime_s*);

#define _MIME_PRIVATE_ \
	Mimedisc_t*	disc;		/* mime discipline		*/ \
	Dtdisc_t	dict;		/* cdt discipline		*/ \
	Magicdisc_t	magicd;		/* magic discipline		*/ \
	Dt_t*		cap;		/* capability tree		*/ \
	Sfio_t*		buf;		/* string buffer		*/ \
	Magic_t*	magic;		/* mimetype() magic handle	*/ \
	Free_f		freef;		/* avoid magic lib if possible	*/ \

#include <mime.h>
#include <ctype.h>

#endif
