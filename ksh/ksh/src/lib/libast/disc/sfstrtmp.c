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
 * sfio tmp string buffer support
 */

#include <sfio_t.h>
#include <ast.h>
#include <sfstr.h>

/*
 * replace buffer in string stream f for either SF_READ or SF_WRITE
 */

int
sfstrtmp(register Sfio_t* f, int mode, void* buf, size_t siz)
{
	if (!(f->_flags & SF_STRING))
		return -1;
	if (f->_flags & SF_MALLOC)
		free(f->_data);
	f->_flags &= ~(SF_ERROR|SF_MALLOC);
	f->mode = mode;
	f->_next = f->_data = (unsigned char*)buf;
	f->_endw = f->_endr = f->_endb = f->_data + siz;
	f->_size = siz;
	return 0;
}
