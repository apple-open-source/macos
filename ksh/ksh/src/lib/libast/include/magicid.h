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
 * generic binary magic id definitions
 */

#ifndef _MAGICID_H
#define _MAGICID_H		1

#include <ast_common.h>

#define MAGICID		0x00010203

typedef unsigned _ast_int4_t Magicid_data_t;

typedef struct Magicid_s
{
	Magicid_data_t	magic;		/* magic number			*/
	char		name[8];	/* generic data/application name*/
	char		type[12];	/* specific data type		*/
	Magicid_data_t	version;	/* YYYYMMDD or 0xWWXXYYZZ	*/
	Magicid_data_t	size;
} Magicid_t;

#endif
