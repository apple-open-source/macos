/*
 * Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 *  usage.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Wed Nov 14 2001.
 *  Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: usage.c,v 1.18 2003/08/04 06:38:45 ssen Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "bless.h"

#include "enums.h"
#include "structs.h"

#define xstr(s) str(s)
#define str(s) #s

const char *modeList[] = { "Global Options", "Info Mode", "Device Mode", "Folder Mode" };

void usage(struct clopt commandlineopts[]) {
    int j;
    size_t oldMode;
    fprintf(stderr, "Usage: " xstr(PROGRAM) " [options]\n");
    
	for(oldMode = 0; oldMode < (sizeof(modeList)/sizeof(const char *)); oldMode++) {
		fprintf(stderr, "%s:\n", modeList[oldMode]);

		for(j=0; j < klast; j++) {
			if(!(commandlineopts[j].modes & (1 << oldMode)) || (commandlineopts[j].modes & mHidden)) {
				continue;
			}

			if(commandlineopts[j].takesarg == aRequired) {
				fprintf(stderr, "   -%-9s arg ", commandlineopts[j].flag);
			} else if(commandlineopts[j].takesarg == aOptional) {
				fprintf(stderr, "   -%-9s[arg]", commandlineopts[j].flag);
			} else {
				fprintf(stderr, "   -%-12s  ", commandlineopts[j].flag);
			}
			fprintf(stderr, "\t%s\n", commandlineopts[j].description);
		}
		fprintf(stderr, "\n");
    }
    exit(1);
}

/* Basically lifted from the man page */
void usage_short() {
    fprintf(stderr, "Usage: " xstr(PROGRAM) " [options]\n");
    fputs(
"bless -help\n"
"\n"
"bless -folder directory [-folder9 directory] [-mount directory]\n"
"\t[-bootinfo file] [-bootBlocks | -bootBlockFile file] [-save9]\n"
"\t[-saveX] [-use9] [-system file] [-systemfile file]\n"
"\t[-label name | -labelfile file] [-openfolder directory] [-setBoot]\n"
"\t[-quiet | -verbose]\n"
"\n"
"bless -device device [-format [fstype] [-fsargs args]\n"
"\t[-label name | -labelfile file]] [-bootBlockFile file]\n"
"\t[-mount directory] [-wrapper file] [-system file] [-startupfile file]\n"
"\t[-setBoot] [-quiet | -verbose]\n"
"\n"
"bless -info [directory] [-bootBlocks] [-getBoot] [-plist]\n"
"\t[-quiet | -verbose] [-version]\n"
,
	  stderr);
    exit(1);
}
