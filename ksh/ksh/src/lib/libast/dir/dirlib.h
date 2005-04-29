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
 * AT&T Research
 *
 * directory stream access library private definitions
 * library routines should include this file rather than <dirent.h>
 */

#ifndef _DIRLIB_H
#define _DIRLIB_H

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:hide getdents getdirentries
#else
#define getdents	______getdents
#define getdirentries	______getdirentries
#endif

#include <ast.h>
#include <errno.h>

#if _lib_opendir && ( _hdr_dirent || _hdr_ndir || _sys_dir )

#define _dir_ok		1

#include <ls.h>

#ifndef _DIRENT_H
#if _hdr_dirent
#if _typ_off64_t
#undef	off_t
#endif
#include <dirent.h>
#if _typ_off64_t
#define off_t	off64_t
#endif
#else
#if _hdr_ndir
#include <ndir.h>
#else
#include <sys/dir.h>
#endif
#ifndef dirent
#define dirent	direct
#endif
#endif
#endif

#define DIRdirent	dirent

#else

#define dirent	DIRdirent

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:hide DIR closedir opendir readdir seekdir telldir
#else
#define DIR		______DIR
#define closedir	______closedir
#define opendir		______opendir
#define readdir		______readdir
#define seekdir		______seekdir
#define telldir		______telldir
#endif

#include <ast_param.h>

#include <ls.h>
#include <limits.h>

#ifndef _DIRENT_H
#if _hdr_dirent
#if _typ_off64_t
#undef	off_t
#endif
#include <dirent.h>
#if _typ_off64_t
#define off_t	off64_t
#endif
#else
#if _hdr_direntry
#include <direntry.h>
#else
#include <sys/dir.h>
#endif
#endif
#endif

#undef	dirent
#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:nohide DIR closedir opendir readdir seekdir telldir
#else
#undef	DIR
#undef	closedir
#undef	opendir
#undef	readdir
#undef	seekdir
#undef	telldir
#endif

#define _DIR_PRIVATE_ \
	int		dd_loc;		/* offset in block		*/ \
	int		dd_size;	/* valid data in block		*/ \
	char*		dd_buf;		/* directory block		*/

#ifdef _BLD_3d
#define DIR		DIRDIR
#endif
#undef	_DIRENT_H
#include "dirstd.h"
#ifndef _DIRENT_H
#define _DIRENT_H	1
#endif
#ifdef _BLD_3d
#undef	DIR
#endif

#ifndef	DIRBLKSIZ
#ifdef	DIRBLK
#define DIRBLKSIZ	DIRBLK
#else
#ifdef	DIRBUF
#define DIRBLKSIZ	DIRBUF
#else
#define DIRBLKSIZ	8192
#endif
#endif
#endif

#endif

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:nohide getdents getdirentries
#else
#undef	getdents
#undef	getdirentries
#endif

#ifndef errno
extern int	errno;
#endif

extern ssize_t		getdents(int, void*, size_t);

#endif
