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
 * posix syslog implementation definitions
 */

#ifndef _SYSLOGLIB_H
#define _SYSLOGLIB_H

#include <syslog.h>

#define log		_log_info_
#define sendlog		_log_send_

/*
 * NOTE: syslog() has a static initializer for Syslog_state_t log
 */

typedef struct
{
	int		facility;	/* openlog facility		*/
	int		fd;		/* log to this fd		*/
	int		flags;		/* openlog flags		*/
	unsigned int	mask;		/* setlogmask mask		*/
	int		attempt;	/* logfile attempt state	*/
	char		ident[64];	/* openlog ident		*/
	char		host[64];	/* openlog host name		*/
} Syslog_state_t;

extern Syslog_state_t	log;

extern void		sendlog(const char*);

#endif
