/* 
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 *
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Tony Nardo of the Johns Hopkins University/Applied Physics Lab.
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 *
 *	@(#)finger.h	8.1 (Berkeley) 6/6/93
 */

typedef struct person {
	uid_t uid;			/* user id */
	char *dir;			/* user's home directory */
	char *homephone;		/* pointer to home phone no. */
	char *name;			/* login name */
	char *office;			/* pointer to office name */
	char *officephone;		/* pointer to office phone no. */
	char *realname;			/* pointer to full name */
	char *shell;			/* user's shell */
	struct where *whead, *wtail;	/* list of where user is or has been */
} PERSON;

enum status { LASTLOG, LOGGEDIN };

typedef struct where {
	struct where *next;		/* next place user is or has been */
	enum status info;		/* type/status of request */
	short writable;			/* tty is writable */
	time_t loginat;			/* time of (last) login */
	time_t idletime;		/* how long idle (if logged in) */
	char tty[UT_LINESIZE+1];	/* null terminated tty line */
	char host[UT_HOSTSIZE+1];	/* null terminated remote host name */
} WHERE;

#include "extern.h"
