/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef __XLOG_H
#define __XLOG_H

/*
 * $Id: xlog.h,v 1.1.1.3 2003/05/22 01:16:36 rbraun Exp $
 */

/*
 * Flags
 */
#define XLOG_NOFLAGS					0x0
#define XLOG_SET_LEVEL				0x1
#define XLOG_NO_SIZECHECK			0x2
#define XLOG_NO_ERRNO				0x4
/* #define XLOG_PRINT_TIMESTAMP		0x8 */
#define XLOG_PRINT_ID				0x10
#define XLOG_PRINT_PID				0x20

/*
 * Errors
 */
#define XLOG_ENOERROR				0
#define XLOG_ESIZE					1
#define XLOG_EOPEN					2
#define XLOG_EFSTAT					3
#define XLOG_ENOMEM					4
#define XLOG_EWRITE					5

/*
 * Interface
 */

typedef enum { XLOG_SYSLOG, XLOG_FILELOG } xlog_e ;

typedef enum
	{
		XLOG_LINK,				/* generic: link this log to another log 			*/
		XLOG_CALLBACK,			/* generic: call this function in case of error */
		XLOG_GETFLAG,			/* generic:	get value of specified flag			*/
		XLOG_SETFLAG,			/* generic:	set value of specified flag			*/
		XLOG_LEVEL,				/* syslog:  set the default syslog level			*/
		XLOG_FACILITY,			/* syslog:  set the default syslog facility		*/
		XLOG_PREEXEC,			/* syslog:	prepare the log for an exec(2)		*/
		XLOG_POSTEXEC,			/* syslog:	exec(2) failed								*/
		XLOG_SIZECHECK,		/* filelog: check file size 							*/
		XLOG_GETFD,				/* filelog: get file descriptor of log file		*/
		XLOG_LIMITS 			/* filelog: set (new) soft/hard limits				*/
	} xlog_cmd_e ;

typedef void *xlog_h ;

xlog_h xlog_create	( xlog_e type, const char *id, int flags, ... ) ;
void xlog_destroy		( xlog_h ) ;
void xlog_write		( xlog_h, const char *buf, int len, int flags, ... ) ;
int xlog_control		( xlog_h, xlog_cmd_e, ... ) ;
int xlog_parms			( xlog_e type, ... ) ;

#endif	/* __XLOG_H */
