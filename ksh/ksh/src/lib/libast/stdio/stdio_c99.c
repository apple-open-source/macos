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
 * C99 stdio extensions
 */

#include "stdhdr.h"

void
clearerr_unlocked(Sfio_t* sp)
{
	clearerr(sp);
}

int
feof_unlocked(Sfio_t* sp)
{
	return feof(sp);
}

int
ferror_unlocked(Sfio_t* sp)
{
	return ferror(sp);
}

int
fflush_unlocked(Sfio_t* sp)
{
	return fflush(sp);
}

int
fgetc_unlocked(Sfio_t* sp)
{
	return fgetc(sp);
}

char*
fgets_unlocked(char* buf, int size, Sfio_t* sp)
{
	return fgets(buf, size, sp);
}

int
fileno_unlocked(Sfio_t* sp)
{
	return fileno(sp);
}

int
fputc_unlocked(int c, Sfio_t* sp)
{
	return fputc(c, sp);
}

int
fputs_unlocked(char* buf, Sfio_t* sp)
{
	return fputs(buf, sp);
}

size_t
fread_unlocked(void* buf, size_t size, size_t n, Sfio_t* sp)
{
	return fread(buf, size, n, sp);
}

size_t
fwrite_unlocked(void* buf, size_t size, size_t n, Sfio_t* sp)
{
	return fwrite(buf, size, n, sp);
}

int
getc_unlocked(Sfio_t* sp)
{
	return getc(sp);
}

int
getchar_unlocked(void)
{
	return getchar();
}

int
putc_unlocked(int c, Sfio_t* sp)
{
	return putc(c, sp);
}

int
putchar_unlocked(int c)
{
	return putchar(c);
}
