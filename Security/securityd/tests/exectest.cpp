/*
 * Copyright (c) 2000-2001,2003-2004 Apple Computer, Inc. All Rights Reserved.
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


//
// Exectest - privileged-execution test driver
//
#include <Security/Authorization.h>
#include <unistd.h>
#include <stdlib.h>


void doLoopback(int argc, char *argv[]);


int main(int argc, char **argv)
{
	const char *path = "/usr/bin/id";
	bool writeToPipe = false;
	bool loopback = false;
	
	int arg;
	extern char *optarg;
	extern int optind;
	while ((arg = getopt(argc, argv, "f:lLw")) != -1) {
		switch (arg) {
		case 'f':
			path = optarg;
			break;
		case 'l':
			loopback = true;
			break;
		case 'L':
			doLoopback(argc, argv);
			exit(0);
		case 'w':
			writeToPipe = true;
			break;
		case '?':
			exit(2);
		}
	}
	
	AuthorizationItem right = { "system.privilege.admin", 0, NULL, 0 };
	AuthorizationRights rights = { 1, &right };

	AuthorizationRef auth;
	if (OSStatus error = AuthorizationCreate(&rights, NULL /*env*/,
		kAuthorizationFlagInteractionAllowed |
		kAuthorizationFlagExtendRights |
		kAuthorizationFlagPreAuthorize,
		&auth)) {
		printf("create error %ld\n", error);
		exit(1);
	}
	
	if (loopback) {
		path = argv[0];
		argv[--optind] = "-L";	// backing over existing array element
	}
	
	FILE *f;
	if (OSStatus error = AuthorizationExecuteWithPrivileges(auth,
		path, 0, argv + optind, &f)) {
		printf("exec error %ld\n", error);
		exit(1);
	}
	printf("--- execute successful ---\n");
	if (writeToPipe) {
		char buffer[1024];
		while (fgets(buffer, sizeof(buffer), stdin))
			fprintf(f, "%s", buffer);
	} else {
		char buffer[1024];
		while (fgets(buffer, sizeof(buffer), f))
			printf("%s", buffer);
	}
	printf("--- end of output ---\n");
	exit(0);
}


void doLoopback(int argc, char *argv[])
{
	// general status
	printf("Authorization Execution Loopback Test\n");
	printf("Invoked as");
	for (int n = 0; argv[n]; n++)
		printf(" %s", argv[n]);
	printf("\n");
	
	// recover the authorization handle
	AuthorizationRef auth;
	if (OSStatus err = AuthorizationCopyPrivilegedReference(&auth, 0)) {
		printf("Cannot recover AuthorizationRef: error=%ld\n", err);
		exit(1);
	}
	
	printf("AuthorizationRef recovered.\n");
}
