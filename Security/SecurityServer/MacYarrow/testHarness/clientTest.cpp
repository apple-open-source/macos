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


/*
 * Simple YarrowClient test.
 */

#include <stdlib.h>
#include <stdio.h>
#include <Security/SecurityYarrowClient.h>

#define BUFSIZE		32

static void dumpBuf(UInt8 *buf,
	unsigned len)
{
	unsigned i;
	
	printf("   ");
	for(i=0; i<len; i++) {
		printf("%02X  ", buf[i]);
		if((i % 8) == 7) {
			printf("\n   ");
		}
	}
	printf("\n");
}

int main()
{
	try {
		YarrowClient client;		// take default constructor
		UInt8	buf[BUFSIZE];
		char	resp = 'm';			// initial op = get random data

		while(1) {
			switch(resp) {
				case 'm':
					client.getRandomBytes(buf, BUFSIZE);
					dumpBuf(buf, BUFSIZE);
					break;
				case 'a':
					/* claim it's half random */
					client.addEntropy(buf, BUFSIZE, BUFSIZE * 4);
					break;
				case '\n':
					goto nextChar;
				default:
					printf("Huh?\n");
			}
			printf(" a   Add this as entropy\n");
			printf(" m   Get more random data\n");
			printf(" q   quit\n");
			printf("\ncommand me: ");
		nextChar:
			resp = getchar();
			if(resp == 'q') {
				break;
			}
		}
	}
	catch (OSErr ortn) {
		printf("YarrowClient threw OSErr %d\n", ortn);
	}
	catch (...) {
		printf("Whoops! YarrowClient threw an exception!\n");
	}
	/* and YarrowClient cleans up on the way out */
	return 0;
}
