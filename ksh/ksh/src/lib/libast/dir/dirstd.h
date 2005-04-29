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
 * AT&T Bell Laboratories
 *
 * <dirent.h> for systems with no opendir()
 */

#ifndef _DIRENT_H
#define _DIRENT_H

typedef struct
{
	int		dd_fd;		/* file descriptor		*/
#ifdef _DIR_PRIVATE_
	_DIR_PRIVATE_
#endif
} DIR;

struct dirent
{
	long		d_fileno;	/* entry serial number		*/
	int		d_reclen;	/* entry length			*/
	int		d_namlen;	/* entry name length		*/
	char		d_name[1];	/* entry name			*/
};

#ifndef _BLD_3d

#ifdef	rewinddir
#undef	rewinddir
#define rewinddir(p)	seekdir(p,0L)
#endif

extern DIR*		opendir(const char*);
extern void		closedir(DIR*);
extern struct dirent*	readdir(DIR*);
extern void		seekdir(DIR*, long);
extern long		telldir(DIR*);

#endif

#endif
