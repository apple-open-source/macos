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
 * AT&T Bell Laboratories
 *
 * copy from rfd to wfd (with conditional mmap hacks)
 */

#include <ast.h>
#include <ast_mmap.h>

#if _mmap_worthy > 1

#include <ls.h>

#define MAPSIZE		(1024*256)

#endif

#undef	BUFSIZ
#define BUFSIZ		4096

/*
 * copy n bytes from rfd to wfd
 * actual byte count returned
 * if n<=0 then ``good'' size is used
 */

off_t
astcopy(int rfd, int wfd, off_t n)
{
	register off_t	c;
#ifdef MAPSIZE
	off_t		pos;
	off_t		mapsize;
	char*		mapbuf;
	struct stat	st;
#endif

	static int	bufsiz;
	static char*	buf;

	if (n <= 0 || n >= BUFSIZ * 2)
	{
#if MAPSIZE
		if (!fstat(rfd, &st) && S_ISREG(st.st_mode) && (pos = lseek(rfd, (off_t)0, 1)) != ((off_t)-1))
		{
			if (pos >= st.st_size) return(0);
			mapsize = st.st_size - pos;
			if (mapsize > MAPSIZE) mapsize = (mapsize > n && n > 0) ? n : MAPSIZE;
			if (mapsize >= BUFSIZ * 2 && (mapbuf = (char*)mmap(NiL, mapsize, PROT_READ, MAP_SHARED, rfd, pos)) != ((caddr_t)-1))
			{
				if (write(wfd, mapbuf, mapsize) != mapsize || lseek(rfd, mapsize, 1) == ((off_t)-1)) return(-1);
				munmap((caddr_t)mapbuf, mapsize);
				return(mapsize);
			}
		}
#endif
		if (n <= 0) n = BUFSIZ;
	}
	if (n > bufsiz)
	{
		if (buf) free(buf);
		bufsiz = roundof(n, BUFSIZ);
		if (!(buf = newof(0, char, bufsiz, 0))) return(-1);
	}
	if ((c = read(rfd, buf, (size_t)n)) > 0 && write(wfd, buf, (size_t)c) != c) c = -1;
	return(c);
}
