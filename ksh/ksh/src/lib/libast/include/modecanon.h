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
 * canonical mode_t representation
 */

#ifndef _MODECANON_H
#define _MODECANON_H

#define X_ITYPE(m)	((m)&X_IFMT)

#define	X_IFMT		0170000
#define	X_IFSOCK	0140000
#define	X_IFLNK		0120000
#define	X_IFCTG		0110000
#define	X_IFREG		0100000
#define	X_IFBLK		0060000
#define	X_IFDIR		0040000
#define	X_IFCHR		0020000
#define	X_IFIFO		0010000

#define X_IPERM		0007777
#define	X_ISUID		0004000
#define	X_ISGID		0002000
#define	X_ISVTX		0001000
#define	X_IRUSR		0000400
#define	X_IWUSR		0000200
#define	X_IXUSR		0000100
#define	X_IRGRP		0000040
#define	X_IWGRP		0000020
#define	X_IXGRP		0000010
#define	X_IROTH		0000004
#define	X_IWOTH		0000002
#define	X_IXOTH		0000001

#define X_IRWXU		(X_IRUSR|X_IWUSR|X_IXUSR)
#define X_IRWXG		(X_IRGRP|X_IWGRP|X_IXGRP)
#define X_IRWXO		(X_IROTH|X_IWOTH|X_IXOTH)

#endif
