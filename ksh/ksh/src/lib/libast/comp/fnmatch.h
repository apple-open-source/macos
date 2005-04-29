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
 * posix fnmatch interface definitions
 */

#ifndef _FNMATCH_H
#define _FNMATCH_H

/* fnmatch flags */

#define FNM_NOESCAPE	0x0001		/* \ is literal			*/
#define FNM_PATHNAME	0x0002		/* explicit match for /		*/
#define FNM_PERIOD	0x0004		/* explicit match for leading .	*/
#define FNM_NOSYS	0x0010		/* not implemented		*/

/* nonstandard fnmatch() flags */

#define FNM_AUGMENTED	0x0008		/* enable ! & ( | )		*/
#define FNM_ICASE	0x0020		/* ignore case in match		*/
#define FNM_LEADING_DIR	0x0040		/* match up to implicit /	*/

#define FNM_CASEFOLD	FNM_ICASE	/* gnu compatibility		*/
#define FNM_FILE_NAME	FNM_PATHNAME	/* gnu compatibility		*/

/* fnmatch error codes -- other non-zero values from <regex.h> */

#define FNM_NOMATCH	1		/* == REG_NOMATCH		*/

extern int	fnmatch(const char*, const char*, int);

#endif
