/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// simple daemon to run yarrow server
//

#include <YarrowServer/YarrowServer_OSX.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	char *entropyFilePath = NULL;
	int arg;
	
	for(arg=1; arg<argc; arg++) {
		switch(argv[arg][0]) {
			case 'e':
				entropyFilePath = &argv[arg][2];
				break;
			default:
				printf("Usage: %s [e=entropyFilePath]\n", argv[0]);
				exit(1);
		}
	}
	printf("starting up server...\n");
	
	YarrowServer *server = new YarrowServer(entropyFilePath);
	server->runYarrow();		// forks off thread
	printf("server running; hit q exit: ");
	while(1) {
		char c = getchar();
		if(c == 'q') {
			break;
		}
		printf("...still running\n");
	}
	return 0;
}