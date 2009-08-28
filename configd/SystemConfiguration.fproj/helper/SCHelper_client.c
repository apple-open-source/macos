/*
 * Copyright (c) 2005-2007 Apple Inc. All rights reserved.
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

#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCPrivate.h>

#include "SCHelper_client.h"
#include "helper_comm.h"


#define	HELPER					"SCHelper"
#define	HELPER_LEN				(sizeof(HELPER) - 1)

#define	SUFFIX_SYM				"~sym"
#define	SUFFIX_SYM_LEN				(sizeof(SUFFIX_SYM) - 1)


__private_extern__
int
_SCHelperOpen(CFDataRef authorizationData)
{
	Boolean			ok;
	int			sock;
	struct sockaddr_un	sun;
	uint32_t		status	= 0;
	static int		yes	= 1;

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("_SCHelperOpen socket() failed");
		return -1;
	}

	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, "/var/run/SCHelper", sizeof(sun.sun_path));
	if (connect(sock, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		perror("_SCHelperOpen connect() failed");
		close(sock);
		return -1;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (const void *)&yes, sizeof(yes)) == -1) {
		perror("_SCHelperOpen setsockopt() failed");
		close(sock);
		return -1;
	}

	ok = _SCHelperExec(sock, SCHELPER_MSG_AUTH, authorizationData, &status, NULL);
	if (!ok) {
		SCLog(TRUE, LOG_INFO, CFSTR("_SCHelperOpen: could not send authorization"));
		close(sock);
		return -1;
	}

	ok = (status == 0);
	if (!ok) {
		SCLog(TRUE, LOG_INFO, CFSTR("could not start \"" HELPER "\", status = %u"), status);
		close(sock);
		return -1;
	}

	return sock;
}


__private_extern__
void
_SCHelperClose(int helper)
{
	if (!_SCHelperExec(helper, SCHELPER_MSG_EXIT, NULL, NULL, NULL)) {
		SCLog(TRUE, LOG_INFO, CFSTR("_SCHelperOpen: could not send exit request"));
	}

	(void)close(helper);
	return;
}
