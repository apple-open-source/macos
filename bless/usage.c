/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "bless.h"

#include "enums.h"
#include "structs.h"

#define xstr(s) str(s)
#define str(s) #s

const char *modeList[] = { "Info Mode", "Device Mode", "Folder Mode" };

void usage(struct clopt commandlineopts[]) {
    int j;
    short oldMode;
    fprintf(stderr, "Usage: " xstr(PROGRAM) " [options]\n");
    
	for(oldMode = 0; oldMode <=2; oldMode++) {
		fprintf(stderr, "%s:\n", modeList[oldMode]);

		for(j=0; j < klast; j++) {
			if(!(commandlineopts[j].modes & 1 << oldMode)) {
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
