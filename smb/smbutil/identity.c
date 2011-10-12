/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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
#include <err.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <sysexits.h>
#include <pwd.h>
#include <membership.h>

#include <smbclient/smbclient.h>
#include <smbclient/smbclient_internal.h>
#include <smbclient/ntstatus.h>

#include "common.h"
#include "LsarLookup.h"
#include "SetNetworkAccountSID.h"
#include <netsmb/smb.h>


#define MAX_SID_PRINTBUFFER	256	/* Used to print out the sid in case of an error */

/*
 * print_ntsid
 *
 * Take a nt style sid and convert it into a printable string.
 */
static
void print_ntsid(ntsid_t *sidptr, const char *printstr)
{
	char sidprintbuf[MAX_SID_PRINTBUFFER];
	char *s = sidprintbuf;
	int subs;
	uint64_t auth = 0;
	unsigned i;
	uint32_t *ip;
	size_t len;
	
	bzero(sidprintbuf, MAX_SID_PRINTBUFFER);
	for (i = 0; i < sizeof(sidptr->sid_authority); i++)
		auth = (auth << 8) | sidptr->sid_authority[i];
	s += snprintf(s, MAX_SID_PRINTBUFFER, "S-%u-%llu", sidptr->sid_kind, auth);
	
	subs = sidptr->sid_authcount;
	
	for (ip = sidptr->sid_authorities; subs--; ip++)  { 
		len = MAX_SID_PRINTBUFFER - (s - sidprintbuf);
		s += snprintf(s, len, "-%u", *ip); 
	}
	fprintf(stdout, "%-16s%s \n", printstr, sidprintbuf);
}

/*
 * get_local_sid
 *
 * Take the local users uid, use that to obtain the uuid and then use 
 * the uuid to to obtain the nt style sid of the local user.
 */
static 
int get_local_sid(void *sid)
{
	uuid_t uu;
	int error;
	
	error = mbr_uid_to_uuid(geteuid(), uu);
	if (!error) {
		error = mbr_uuid_to_sid(uu, sid);
	}
	return error;
}

int
cmd_identity(int argc, char *argv[])
{
	struct passwd	*pwd = NULL;
	const char *url = NULL;
	int			opt;
	SMBHANDLE	serverConnection = NULL;
	uint64_t	options = 0;
	NTSTATUS	status;
	SMBServerPropertiesV1 properties;
	char *AccountName = NULL, *DomainName = NULL;
	ntsid_t	*sid = NULL;
	ntsid_t	localSid;
	
	while ((opt = getopt(argc, argv, "N")) != EOF) {
		switch(opt){
			case 'N':
				options |= kSMBOptionNoPrompt;
				break;
			default:
				identity_usage();
				/*NOTREACHED*/
		}
	}
	if (optind >= argc)
		identity_usage();
	url = argv[optind];
	argc -= optind;
	/* One more check to make sure we have the correct number of arguments */
	if (argc != 1)
		identity_usage();
	
	status = SMBOpenServerEx(url, &serverConnection, options);
	/* 
	 * SMBOpenServerEx now sets errno, so err will work correctly. We change 
	 * the string based on the NTSTATUS Error.
	 */
	if (!NT_SUCCESS(status)) {
		/* This routine will exit the program */
		ntstatus_to_err(status);
	}
	status = SMBGetServerProperties(serverConnection, &properties, kPropertiesVersion, sizeof(properties));
	if (!NT_SUCCESS(status)) {
		err(EX_UNAVAILABLE, "internal error");
	}
	/* Only use RPC if the server supports DCE/RPC and UNICODE */
	if ((properties.capabilities & SMB_CAP_RPC_REMOTE_APIS) && 
		(properties.capabilities & SMB_CAP_UNICODE)) {
		status = GetNetworkAccountSID(properties.serverName, &AccountName, &DomainName, &sid, 
									  (properties.internalFlags & kWorkAroundEMCPanic) ? TRUE : FALSE);
	} else {
		status = STATUS_NOT_SUPPORTED;
	}
	if (!NT_SUCCESS(status)) {
		/* This routine will exit the program */
		ntstatus_to_err(status);
	}
	fprintf(stdout, "\n");
	if (AccountName) {
		fprintf(stdout, "%-16s%s\n", "Network User:", AccountName);
		free(AccountName);
	} else {
		fprintf(stdout, "%-16s%s\n", "Network User:", "UNKNOWN");
	}
	if (DomainName) {
		fprintf(stdout, "%-16s%s\n", "Network Domain:", DomainName);
		free(DomainName);
	} else {
		fprintf(stdout, "%-16s%s\n", "Network Domain:", "UNKNOWN");
	}
	if (sid) {
		print_ntsid(sid, "Network SID:");
		free(sid);
	} else {
		fprintf(stdout, "%-16s%s\n", "Network SID:", "UNKNOWN");
	}
	/* 
	 * Remmeber the contents of this data structure is automatically released by 
	 * subsequent calls to any of these rou tines on the same thread, or when 
	 * the thread exits.	 
	 */
	pwd = getpwuid(geteuid());
	if (pwd && pwd->pw_name) {
		fprintf(stdout, "%-16s%s\n", "Local User:", pwd->pw_name);
	} else {
		fprintf(stdout, "Local User UNKNOWN\n");
	}
	if (get_local_sid(&localSid) == 0) {
		print_ntsid(&localSid, "Local SID:");
	} else {
		fprintf(stdout, "%-16s%s\n", "Local SID:", "UNKNOWN");
	}

	SMBReleaseServer(serverConnection);
	return 0;
}


void
identity_usage(void)
{
	fprintf(stderr, "usage: smbutil identity [connection options] //"
			"[domain;][user[:password]@]"
			"server\n");
	fprintf(stderr, "where options are:\n"
			"    -N    don't prompt for a password\n");
	exit(1);
}
