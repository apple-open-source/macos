/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1985, 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ftp_var.h	8.4 (Berkeley) 10/9/94
 */

#include <stdio.h>

#include "ftp_var.h"

/*
 * FTP global variables.
 */

#include <sys/param.h>
#include <setjmp.h>

#include "extern.h"

/*
 * Options and other state info.
 */
int	trace;			/* trace packets exchanged */
int	hash;			/* print # for each buffer transferred */
int	sendport;		/* use PORT cmd for each data connection */
int	verbose;		/* print messages coming back from server */
int	connected;		/* connected to server */
int	fromatty;		/* input is from a terminal */
int	interactive;		/* interactively prompt on m* cmds */
int	debug;			/* debugging level */
int	bell;			/* ring bell on cmd completion */
int	doglob;			/* glob local file names */
int	autologin;		/* establish user account on connection */
int	proxy;			/* proxy server connection active */
int	proxflag;		/* proxy connection exists */
int	sunique;		/* store files on server with unique name */
int	runique;		/* store local files with unique name */
int	mcase;			/* map upper to lower case for mget names */
int	ntflag;			/* use ntin ntout tables for name translation */
int	mapflag;		/* use mapin mapout templates on file names */
int	code;			/* return/reply code for ftp command */
int	crflag;			/* if 1, strip car. rets. on ascii gets */
char	pasv[64];		/* passive port for proxy data connection */
int	passivemode;		/* passive mode enabled */
char	*altarg;		/* argv[1] with no shell-like preprocessing  */
char	ntin[17];		/* input translation table */
char	ntout[17];		/* output translation table */
char	mapin[MAXPATHLEN];	/* input map template */
char	mapout[MAXPATHLEN];	/* output map template */
char	typename[32];		/* name of file transfer type */
int	type;			/* requested file transfer type */
int	curtype;		/* current file transfer type */
char	structname[32];		/* name of file transfer structure */
int	stru;			/* file transfer structure */
char	formname[32];		/* name of file transfer format */
int	form;			/* file transfer format */
char	modename[32];		/* name of file transfer mode */
int	mode;			/* file transfer mode */
char	bytename[32];		/* local byte size in ascii */
int	bytesize;		/* local byte size in binary */

char	*hostname;		/* name of host connected to */
int	unix_server;		/* server is unix, can use binary for ascii */
int	unix_proxy;		/* proxy is unix, can use binary for ascii */

struct	servent *sp;		/* service spec for tcp/ftp */

jmp_buf	toplevel;		/* non-local goto stuff for cmd scanner */

char	line[200];		/* input line buffer */
char	*stringbase;		/* current scan point in line buffer */
char	argbuf[200];		/* argument storage buffer */
char	*argbase;		/* current storage point in arg buffer */
int	margc;			/* count of arguments on input line */
char	*margv[20];		/* args parsed from input line */
int     cpend;                  /* flag: if != 0, then pending server reply */
int	mflag;			/* flag: if != 0, then active multi command */

int	options;		/* used during socket creation */

int macnum;			/* number of defined macros */
struct macel macros[16];
char macbuf[4096];
