/* 
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "LKDCHelper.h"

int main (int argc, char * const argv[])
{
	LKDCHelperErrorType error;
	char		*string = NULL;
	uint16_t	kdcport = 0;
	int			i, ch;

	while ((ch = getopt (argc, argv, "sqvr")) != -1) {
		switch (ch) {
			case 's':
				LKDCDumpStatus (ASL_LEVEL_NOTICE);
				break;
				
			case 'q':
				LKDCHelperExit ();
				break;
				
			case 'v':
				LKDCSetLogLevel (ASL_LEVEL_NOTICE);
				break;

			case 'r':
				error = LKDCGetLocalRealm (&string);
				if (0 != error) {
					fprintf (stderr, "LKDCGetLocalRealm -> %d (%s): %s\n", error, LKDCHelperError (error));
				} else {
					printf ("%s\n", string);
				}
				break;
				
			case '?':
			default:
				fprintf (stderr, "The -x flag causes LKDCHelper to dump its cache\n");
				fprintf (stderr, "Otherwise supply either a hostname or an LKDC Realm to look up\n");
				error ++;
				break;
		}
	}
	argc -= optind;
	argv += optind;
	
	for (i = 0; i < argc; i++) {
		char * argument = argv[i];
		
		if (strncmp ("LKDC:", argument, 5) == 0) {
			error = LKDCFindKDCForRealm (argument, &string, &kdcport);
		} else {
			error = LKDCDiscoverRealm (argument, &string);
		}

		if (0 != error) {
			fprintf (stderr, "LKDCHelper -> %d (%s): %s\n", error, LKDCHelperError (error), argument);
			error++;
		} else {
			printf ("%s\n", string);
		}
	}

	exit (error);
}

