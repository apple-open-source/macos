/*
 * Copyright (c) 2001 - 2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <err.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <sysexits.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <cflib.h>

#include <netsmb/netbios.h>
#include <netsmb/smb_lib.h>
#include <netsmb/nb_lib.h>
#include <netsmb/smb_conn.h>
#include <charsets.h>

#include "common.h"


int
cmd_status(int argc, char *argv[])
{
	struct rcfile *smb_rc;
	struct nb_ctx ctx;
	struct sockaddr *sap;
	char *hostname;
	char servername[SMB_MAXNetBIOSNAMELEN + 1];
	char workgroupname[SMB_MAXNetBIOSNAMELEN + 1];
	char * displayName;
	int opt;

	if (argc < 2)
		status_usage();
	bzero(&ctx, sizeof(ctx));
	smb_rc = smb_open_rcfile(FALSE);
	if (smb_rc) {
		if (nb_ctx_readrcsection(smb_rc, &ctx, "default", 0) != 0)
			exit(1);
		nb_ctx_readcodepage(smb_rc, "default");
		rc_close(smb_rc);
		smb_rc = NULL;
	}
	while ((opt = getopt(argc, argv, "")) != EOF) {
		switch(opt){
		    default:
			status_usage();
			/*NOTREACHED*/
		}
	}
	if (optind >= argc)
		status_usage();

	hostname = argv[argc - 1];
	errno = nb_resolvehost_in(hostname, &sap, NBSS_TCP_PORT_139, TRUE);
	if (errno)
		err(EX_NOHOST, "unable to resolve DNS hostname %s", hostname);

	servername[0] = (char)0;
	workgroupname[0] = (char)0;
	errno = nbns_getnodestatus(sap, &ctx, servername, workgroupname);
	if (errno)
		err(EX_UNAVAILABLE, "unable to get status from %s", hostname);
	
	if (workgroupname[0]) {
		displayName = convert_wincs_to_utf8(workgroupname);
		if (displayName) {
			fprintf(stdout, "Workgroup: %s\n", displayName);
			free(displayName);
		} else 
			fprintf(stdout, "Workgroup: %s\n", workgroupname);
	}
	if (servername[0]) {
		displayName = convert_wincs_to_utf8(servername);
		if (displayName) {
			fprintf(stdout, "Server: %s\n", displayName);
			free(displayName);
		} else 
			fprintf(stdout, "Server: %s\n", servername);
	}
	nb_ctx_done(&ctx);
	
	return 0;
}


void
status_usage(void)
{
	fprintf(stderr, "usage: smbutil status hostname\n");
	exit(1);
}
