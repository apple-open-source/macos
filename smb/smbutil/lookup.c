/*
 * Copyright (c) 2000, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2005 - 2010 Apple Inc. All rights reserved 
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
#include <sys/socket.h>
#include <err.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <sysexits.h>

#include <SMBClient/smbclient.h>
#include <SMBClient/netbios.h>
#include <arpa/inet.h>

#include "common.h"

/*
 * Make a copy of the name and if request unpercent escape the name
 */
static char * 
getHostName(const char *name, Boolean escapeNames)
{	
	char *newName = strdup(name);
	CFStringRef nameRef = NULL;
	CFStringRef newNameRef = NULL;
		
	/* They don't want it escape or the strdup failed */
	if (!escapeNames || !newName) {
		return newName;
	}
	/* Get a CFString */
	nameRef = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8);
	
	/* unpercent escape out the CFString */
	if (nameRef) {
		newNameRef = CFURLCreateStringByReplacingPercentEscapesUsingEncoding(kCFAllocatorDefault, 
																			 nameRef, CFSTR(" "), kCFStringEncodingUTF8);
	}
	/* Now create an unpercent escape out c style string */
	if (newNameRef) {
		int maxlen = (int)CFStringGetLength(newNameRef)+1;
		char *tempName = malloc(maxlen);
		
		if (tempName) {
			free(newName);
			newName = tempName;
			CFStringGetCString(newNameRef, newName, maxlen, kCFStringEncodingUTF8);
		}	
	}
	
	if (nameRef) {
		CFRelease(nameRef);
	}
	if (newNameRef) {
		CFRelease(newNameRef);
	}
	return newName;
}


int
cmd_lookup(int argc, char *argv[])
{
	char *hostname;
	int opt;
	struct sockaddr_storage *startAddr = NULL, *listAddr = NULL;
	const char *winsServer = NULL;
	int32_t ii, count = 0;
	struct sockaddr_storage respAddr;
	struct sockaddr_in *in4 = NULL;
	struct sockaddr_in6 *in6 = NULL;
	char addrStr[INET6_ADDRSTRLEN+1];
	uint8_t nodeType = kNetBIOSFileServerService;
	Boolean escapeNames= FALSE;
	
	bzero(&respAddr, sizeof(respAddr));
	if (argc < 2)
		lookup_usage();
	while ((opt = getopt(argc, argv, "ew:t:")) != EOF) {
		switch(opt) {
			case 'e':
				escapeNames = TRUE;
				break;
			case 'w':
				winsServer = optarg;
				break;
			case 't':
				errno = 0;
				nodeType = (uint8_t)strtol(optarg, NULL, 0);
				if (errno)
					errx(EX_DATAERR, "invalid value for node type");
				break;
		    default:
				lookup_usage();
				/*NOTREACHED*/
		}
	}
	if (optind >= argc)
		lookup_usage();

	hostname = getHostName(argv[argc - 1], escapeNames);
	if (!hostname) {
		err(EX_OSERR, "failed to resolve %s", argv[argc - 1]);
	}
	
	startAddr = listAddr = SMBResolveNetBIOSNameEx(hostname, nodeType, winsServer, 
													0, &respAddr, &count);
	if (startAddr == NULL) {
		err(EX_NOHOST, "unable to resolve %s", hostname);
	}
	
	if (respAddr.ss_family == AF_INET) {
		in4 = (struct sockaddr_in *)&respAddr;
		inet_ntop(respAddr.ss_family, &in4->sin_addr, addrStr, sizeof(addrStr));
	} else if (respAddr.ss_family == AF_INET6) {
		in6 = (struct sockaddr_in6 *)&respAddr;
		inet_ntop(respAddr.ss_family, &in6->sin6_addr, addrStr, sizeof(addrStr));
	} else {
		strcpy(addrStr, "unknown address family");
	}

	fprintf(stdout, "Got response from %s\n", addrStr);

	for (ii=0; ii < count; ii++) {
		if (listAddr->ss_family == AF_INET) {
			in4 = (struct sockaddr_in *)listAddr;
			inet_ntop(listAddr->ss_family, &in4->sin_addr, addrStr, sizeof(addrStr));
		} else if (respAddr.ss_family == AF_INET6) {
			in6 = (struct sockaddr_in6 *)listAddr;
			inet_ntop(respAddr.ss_family, &in6->sin6_addr, addrStr, sizeof(addrStr));
		} else {
			strcpy(addrStr, "unknown address family");
		}
		fprintf(stdout, "IP address of %s: %s\n", hostname, addrStr);
		listAddr++;
	}
	if (startAddr) {
		free(startAddr);
	}
	if (hostname) {
		free(hostname);
	}
	return 0;
}


void
lookup_usage(void)
{
	fprintf(stderr, "usage: smbutil lookup [-e] [-w host] [-t node type] name\n");
	exit(1);
}
