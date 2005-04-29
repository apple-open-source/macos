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
 * virtual db file directory entry constants
 */

#ifndef VDB_MAGIC

#define VDB_MAGIC	"vdb"

#define VDB_DIRECTORY	"DIRECTORY"
#define VDB_UNION	"UNION"
#define VDB_DATE	"DATE"
#define VDB_MODE	"MODE"

#define VDB_DELIMITER	';'
#define VDB_IGNORE	'_'
#define VDB_FIXED	10
#define VDB_LENGTH	((int)sizeof(VDB_DIRECTORY)+2*(VDB_FIXED+1))
#define VDB_OFFSET	((int)sizeof(VDB_DIRECTORY))
#define VDB_SIZE	(VDB_OFFSET+VDB_FIXED+1)

#endif
