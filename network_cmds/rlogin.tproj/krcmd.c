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
 * Copyright (c) 1989, 1993
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
 */


/*
 *	$Source: /Volumes/george/fs-svn/network_cmds/rlogin.tproj/krcmd.c,v $
 *	$Header: /mit/kerberos/ucb/mit/kcmd/RCS/krcmd.c,v 5.1
 *		89/07/25 15:38:44 kfall Exp Locker: kfall $
 * static char *rcsid_kcmd_c =
 * "$Header: /mit/kerberos/ucb/mit/kcmd/RCS/krcmd.c,v 5.1 89/07/25 15:38:44
 *	kfall Exp Locker: kfall $";
 */

#ifdef KERBEROS
#include <sys/types.h>
#ifdef CRYPT
#include <sys/socket.h>
#endif

#include <netinet/in.h>

#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>

#include <stdio.h>

#define	SERVICE_NAME	"rcmd"

int	kcmd __P((int *, char **, u_short, char *, char *, char *, int *,
	    KTEXT, char *, char *, CREDENTIALS *, Key_schedule, MSG_DAT *,
	    struct sockaddr_in *, struct sockaddr_in *, long));

/*
 * krcmd: simplified version of Athena's "kcmd"
 *	returns a socket attached to the destination, -1 or krb error on error 
 *	if fd2p is non-NULL, another socket is filled in for it
 */

int
krcmd(ahost, rport, remuser, cmd, fd2p, realm)
	char	**ahost;
	u_short	rport;
	char	*remuser, *cmd;
	int	*fd2p;
	char	*realm;
{
	int		sock = -1, err = 0;
	KTEXT_ST	ticket;
	long		authopts = 0L;

	err = kcmd(
		&sock,
		ahost,
		rport,
		NULL,	/* locuser not used */
		remuser,
		cmd,
		fd2p,
		&ticket,
		SERVICE_NAME,
		realm,
		(CREDENTIALS *)  NULL,		/* credentials not used */
		(bit_64 *) NULL,		/* key schedule not used */
		(MSG_DAT *) NULL,		/* MSG_DAT not used */
		(struct sockaddr_in *) NULL,	/* local addr not used */
		(struct sockaddr_in *) NULL,	/* foreign addr not used */
		authopts
	);

	if (err > KSUCCESS && err < MAX_KRB_ERRORS) {
		fprintf(stderr, "krcmd: %s\n", krb_err_txt[err]);
		return(-1);
	}
	if (err < 0)
		return(-1);
	return(sock);
}

#ifdef CRYPT
int
krcmd_mutual(ahost, rport, remuser, cmd, fd2p, realm, cred, sched)
	char		**ahost;
	u_short		rport;
	char		*remuser, *cmd;
	int		*fd2p;
	char		*realm;
	CREDENTIALS	*cred;
	Key_schedule	sched;
{
	int		sock, err;
	KTEXT_ST	ticket;
	MSG_DAT		msg_dat;
	struct sockaddr_in	laddr, faddr;
	long authopts = KOPT_DO_MUTUAL;

	err = kcmd(
		&sock,
		ahost,
		rport,
		NULL,	/* locuser not used */
		remuser,
		cmd,
		fd2p,
		&ticket,
		SERVICE_NAME,
		realm,
		cred,		/* filled in */
		sched,		/* filled in */
		&msg_dat,	/* filled in */
		&laddr,		/* filled in */
		&faddr,		/* filled in */
		authopts
	);

	if (err > KSUCCESS && err < MAX_KRB_ERRORS) {
		fprintf(stderr, "krcmd_mutual: %s\n", krb_err_txt[err]);
		return(-1);
	}

	if (err < 0)
		return (-1);
	return(sock);
}
#endif /* CRYPT */
#endif /* KERBEROS */
