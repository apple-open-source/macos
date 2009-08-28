/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*          Copyright (c) 1985-2007 AT&T Intellectual Property          *
*                      and is licensed under the                       *
*                  Common Public License, Version 1.0                  *
*                    by AT&T Intellectual Property                     *
*                                                                      *
*                A copy of the License is available at                 *
*            http://www.opensource.org/licenses/cpl1.0.txt             *
*         (with md5 checksum 059e8cd6165cb4c31e351f2b69388fd9)         *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                  David Korn <dgk@research.att.com>                   *
*                   Phong Vo <kpv@research.att.com>                    *
*                                                                      *
***********************************************************************/
#pragma prototyped
/*
 * standalone mini error interface
 */

#ifndef _ERROR_H
#define _ERROR_H

#include <option.h>
#include <stdarg.h>

typedef struct Error_info_s
{
	int		errors;
	int		line;
	int		warnings;
	char*		catalog;
	char*		file;
	char*		id;
} Error_info_t;

#define ERROR_catalog(s)	s

#define ERROR_INFO	0		/* info message -- no err_id	*/
#define ERROR_WARNING	1		/* warning message		*/
#define ERROR_ERROR	2		/* error message -- no err_exit	*/
#define ERROR_FATAL	3		/* error message with err_exit	*/
#define ERROR_PANIC	ERROR_LEVEL	/* panic message with err_exit	*/

#define ERROR_LEVEL	0x00ff		/* level portion of status	*/
#define ERROR_SYSTEM	0x0100		/* report system errno message	*/
#define ERROR_USAGE	0x0800		/* usage message		*/

#define error_info	_err_info
#define error		_err_msg
#define errorv		_err_msgv

extern Error_info_t	error_info;

#define errorx(l,x,c,m)	(char*)m

extern void	error(int, ...);
extern void	errorf(void*, void*, int, ...);
extern void	errorv(const char*, int, va_list);

#endif
