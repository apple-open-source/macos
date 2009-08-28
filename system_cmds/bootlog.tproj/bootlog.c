/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/un.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <utmpx.h>

#define _PATH_ASL_IN "/var/run/asl_input"

#define REPEAT	100
#define SLEEP	2

static void
asl_running(void)
{
	int sock, i;
	struct sockaddr_un server;
	socklen_t len;
	int status;

	sock = socket(AF_UNIX, SOCK_STREAM, 0); 
	if (sock < 0)
		exit(1);

	memset(&server, 0, sizeof(struct sockaddr_un));
	server.sun_family = AF_UNIX;

	strcpy(server.sun_path, _PATH_ASL_IN);
	server.sun_len = strlen(server.sun_path) + 1;
	len = sizeof(server.sun_len) + sizeof(server.sun_family) + server.sun_len;

	i = REPEAT;
	for(;;) {
	    status = connect(sock, (const struct sockaddr *)&server, len);
	    if (status >= 0)
		break;
	    if(--i <= 0)
		exit(1);
	    sleep(SLEEP);
	}

	close(sock);
}

int
main()
{
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    struct utmpx utx;
    size_t len;

    bzero(&utx, sizeof(utx));
    utx.ut_type = BOOT_TIME;
    utx.ut_pid = 1; // on behalf of launchd

    /* get the boot time */
    len = sizeof(struct timeval);
    if(sysctl(mib, 2, &utx.ut_tv, &len, NULL, 0) < 0)
	gettimeofday(&utx.ut_tv, NULL); /* fallback to now */

    /* wait for asl before logging */
    asl_running();
    pututxline(&utx);
    return 0;
}
