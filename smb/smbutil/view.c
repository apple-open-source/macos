/*
 * Copyright (c) 2000, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2007 Apple Inc. All rights reserved.
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <err.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <sysexits.h>

#include <cflib.h>

#include <sys/mchain.h>

#include <netsmb/smb_lib.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_netshareenum.h>

#include "common.h"

static char *shtype[] = {
	"disk",
	"printer",
	"comm",		/* Communications device */
	"pipe",		/* IPC Inter process communication */
	"unknown"
};

int
cmd_view(int argc, char *argv[])
{
	struct smb_ctx *ctx = NULL;
	struct share_info *share_info, *ep;
	int error, opt, i, entries, total;
	const char *cp;
	char * url = NULL;
	int prompt_user = (isatty(STDIN_FILENO)) ? TRUE : FALSE;
	
	if (argc < 2)
		view_usage();
	
	/* Get the url from the argument list */
	for (opt = 1; opt < argc; opt++) {
		cp = argv[opt];
		if (strncmp(cp, "//", 2) != 0)
			continue;
		url = (char *)cp;
		break;
	}
	if (! url)	/* No URL then a bad argument list */
		view_usage();
	error = smb_ctx_init(&ctx, url, SMBL_VC, SMB_ST_ANY, FALSE);
	if (error)
		exit(error);

	while ((opt = getopt(argc, argv, "N")) != EOF) {
		switch(opt){
			case 'N':
				prompt_user = FALSE;
				break;
			default:
				view_usage();
			/*NOTREACHED*/
		}
	}

	smb_ctx_setshare(ctx, "IPC$", SMB_ST_ANY);
	errno  = smb_connect(ctx);
	if (errno)
		err(EX_NOHOST, "server connection failed");
	
	/* The server supports Kerberos then see if we can connect */
	if (ctx->ct_vc_flags & SMBV_KERBEROS_SUPPORT)
		error = smb_session_security(ctx, NULL, NULL);
	else if (ctx->ct_ssn.ioc_opt & SMBV_EXT_SEC)
		error = ENOTSUP;
	else 
		error = smb_session_security(ctx, NULL, NULL);
	
	/* Either Kerberos failed or they do extended security, but not Kerberos */ 
	if (error) {
		ctx->ct_ssn.ioc_opt &= ~SMBV_EXT_SEC;	
		ctx->ct_flags &= ~SMBCF_CONNECTED;		
		errno  = smb_connect(ctx);
		if (errno)
			err(EX_NOHOST, "server connection failed");
		/* need to command-line prompting for the password */
		if (prompt_user && ((ctx->ct_flags & SMBCF_EXPLICITPWD) != SMBCF_EXPLICITPWD)) {
			char passwd[SMB_MAXPASSWORDLEN + 1];
			
			strncpy(passwd, getpass(SMB_PASSWORD_KEY ":"), SMB_MAXPASSWORDLEN);
			smb_ctx_setpassword(ctx, passwd);
		}
		errno = smb_session_security(ctx, NULL, NULL);
		if (errno)
			err(EX_NOPERM, "server rejected the connection");
	}
	
	errno = smb_share_connect(ctx);
	if (errno)
		err(EX_IOERR, "connection to the share failed");
	
	
	fprintf(stdout, "Share        Type       Comment\n");
	fprintf(stdout, "-------------------------------\n");
	errno = smb_netshareenum(ctx, &entries, &total, &share_info);
	if (errno)
		err(EX_IOERR, "unable to list resources");

	for (ep = share_info, i = 0; i < entries; i++, ep++) {
		fprintf(stdout, "%-12s %-10s %s\n", ep->netname,
		    shtype[min(ep->type, sizeof shtype / sizeof(char *) - 1)],
		    ep->remark ? ep->remark : "");
	}
	fprintf(stdout, "\n%d shares listed from %d available\n", entries, total);
	free(share_info);
	smb_ctx_done(ctx);
	return 0;
}


void
view_usage(void)
{
	fprintf(stderr, "usage: smbutil view [connection options] //"
		"[domain;][user[:password]@]"
	"server\n");
	exit(1);
}

