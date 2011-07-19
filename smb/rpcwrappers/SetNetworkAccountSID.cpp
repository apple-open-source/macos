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

#include <stdint.h>
#include <netsmb/smb.h>

#include <smbclient/smbclient.h>
#include <smbclient/smbclient_internal.h>
#include <smbclient/smbclient_netfs.h>
#include <smbclient/ntstatus.h>

#include "SetNetworkAccountSID.h"
#include "LsarLookup.h"

#define MAX_SID_PRINTBUFFER	256	/* Used to print out the sid in case of an error */
static
void print_ntsid(ntsid_t *sidptr, const char *account, const char *domain)
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
	SMBLogInfo("%s\\%s network sid %s \n", ASL_LEVEL_DEBUG,
			   (domain) ? domain : "", (account) ? account : "", sidprintbuf);
}

void setNetworkAccountSID(void *sessionRef, void *args) 
{
#pragma unused(args)
	SMBHANDLE serverConnection = SMBAllocateAndSetContext(sessionRef);
	ntsid_t *ntsid = NULL;
	SMBServerPropertiesV1 properties;
	NTSTATUS status;
	char *account = NULL, *domain = NULL;
	
	if (!serverConnection) {
		goto done;
	}
	status = SMBGetServerProperties(serverConnection, &properties, kPropertiesVersion, sizeof(properties));
	if (!NT_SUCCESS(status)) {
		goto done;
	}
	/* We already have a network sid assigned, then do nothing */
	if (properties.internalFlags & kHasNtwrkSID) {
		goto done;
	}
	
	/* We never set the user sid if guest or anonymous authentication */
	if ((properties.authType == kSMBAuthTypeGuest) || (properties.authType == kSMBAuthTypeAnonymous)) {
		goto done;
	}
	status = GetNetworkAccountSID(properties.serverName, &account, &domain, &ntsid);
	if (!NT_SUCCESS(status)) {
		goto done;
	}
	print_ntsid(ntsid, account, domain);
	/* 
	 * In the future this should return an ntstatus and set errno. Currently we
	 * ignore the error, since the failure just means ACLs are off. 
	 */
	(void)SMBSetNetworkIdentity(serverConnection, ntsid, account, domain);
done:
	if (account) {
		free(account);
	}
	if (domain) {
		free(domain);
	}
	if (ntsid) {
		free(ntsid);
	}
	if (serverConnection) {
		free(serverConnection);
	}
}
