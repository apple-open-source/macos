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
#include <string.h>
#include <unistd.h>

#include "enums.h"
#include "structs.h"

#include "bless.h"

#define xstr(s) str(s)
#define str(s) #s

struct clopt commandlineopts[klast];
struct clarg actargs[klast];

int modeInfo(BLContext context, struct clopt commandlineopts[klast], struct clarg actargs[klast]);
int modeDevice(BLContext context, struct clopt commandlineopts[klast], struct clarg actargs[klast]);
int modeFolder(BLContext context, struct clopt commandlineopts[klast], struct clarg actargs[klast]);

int blesslog(void *context, int loglevel, const char *string);
static void initConfig();
void usage(struct clopt[]);

int main (int argc, const char * argv[])
{

    int i;
    
    BLContextStruct context;
    struct blesscon bcon;

    bcon.quiet = 0;
    bcon.verbose = 0;

    context.logstring = blesslog;
    context.logrefcon = &bcon;
    
    
#ifndef __ppc__
		// yay, this works on ppc-based arches
	fprintf(stderr, xstr(PROGRAM) " only runs on PowerPC-based Darwin machines\n");
	exit(1);
#endif
	
    initConfig();
	
    if(argc == 1) {
        usage(commandlineopts);
    }
        
    /* start at 1, since argc >=2 */
    for(i=1; i < argc; i++) {
        int j;
        int found = 0;
        
        /* check against each option */
        for(j=0; j < klast; j++) {

            /* if it matches the option text */
            if(!strcmp(&(argv[i][1]), commandlineopts[j].flag)) {

                if(commandlineopts[j].takesarg == aRequired) {
                    i++;
                    if(i >= argc ) usage(commandlineopts); /* no arg given */
                    strncpy(actargs[j].argument, argv[i], kMaxArgLength-1);
					actargs[j].argument[kMaxArgLength-1] = '\0';
					actargs[j].hasArg = 1;
					found = 1;
					break;
                } else if(commandlineopts[j].takesarg == aOptional) {
					if((i+1>=argc) || ((i+1<argc) && argv[i+1][0] == '-')) {
						// if next item appears to be a flag, or doesn't exist, no opt
						actargs[j].argument[0] = '\0';
						found = 1;
						break;
					} else if(i+1<argc) {
						// looks like we're taking an argument
						i++;
						strncpy(actargs[j].argument, argv[i], kMaxArgLength-1);
						actargs[j].argument[kMaxArgLength-1] = '\0';
						actargs[j].hasArg = 1;
						found = 1;
						break;
					}
				} else if(commandlineopts[j].takesarg == aNone) {
					actargs[j].argument[0] = '\0';
					found = 1;
					break;
				}
            }
        }
        
        /* if the option wasn't found, we have a problem */
        if(!found) {
            usage(commandlineopts);
        } else {
			actargs[j].present = 1;
		}
    }

	bcon.verbose = actargs[kverbose].present ? 1 : 0;
	bcon.quiet = actargs[kquiet].present ? 1 : 0;

    /* There are three modes of execution: info, device, folder.
     * These are all one-way function jumps.
     */
     
    /* If it was requested, print out the Finder Info words */
    if(actargs[kinfo].present) {
        return modeInfo(&context, commandlineopts, actargs);
    }

    if(actargs[kdevice].present) {
        return modeDevice(&context, commandlineopts, actargs);
    }

	/* default */
	return modeFolder(&context, commandlineopts, actargs);

}

#define setoption(opt, desc, fflag, hasarg, mode)  commandlineopts[opt].description = desc; \
                                            commandlineopts[opt].flag = fflag; \
                                            commandlineopts[opt].takesarg = hasarg; \
					    commandlineopts[opt].modes = mode


static void initConfig() {
    int i;


    for(i=0; i< klast; i++) {
		bzero(&actargs[i], sizeof(struct clarg));
    }

setoption(kbootinfo, "Path to a bootx.bootinfo file to be used as a BootX", "bootinfo", aRequired, mFolder);
setoption(kbootblocks, "Get/set boot blocks if an OS 9 folder was specified", "bootBlocks", aNone, mFolder|mInfo);
setoption(kbootblockfile, "Data fork file with boot blocks", "bootBlockFile", aRequired, mFolder);
setoption(kdevice, "Unmounted block device to operate on", "device", aRequired, mDevice|mModeFlag);
setoption(kfolder, "Darwin/Mac OS X folder to be blessed", "folder", aRequired, mFolder|mModeFlag);
setoption(kfolder9, "Classic/Mac OS 9 folder to be blessed", "folder9", aRequired, mFolder|mModeFlag);
setoption(kformat, "Format the device with the given filesystem", "format", aOptional, mDevice);
setoption(kfsargs, "Extra arguments to newfs", "fsargs", aRequired, mDevice);
setoption(kinfo, "Print out Finder info fields for the specified volume", "info", aOptional, mInfo|mModeFlag);
setoption(klabel, "Label for a newly-formatted volume", "label", aRequired, mDevice|mFolder);
setoption(kmount, "Mount point to use", "mount", aRequired, 
mFolder|mDevice);
setoption(kquiet, "Quiet mode", "quiet", aNone, mDevice|mFolder|mInfo);
setoption(kplist, "Output in plist format", "plist", aNone, mInfo);
setoption(ksave9, "Save the existing 9 blessed folder", "save9", aNone, mFolder);
setoption(ksaveX, "Save the existing X blessed folder", "saveX", aNone, mFolder);
setoption(ksetOF, "Set Open Firmware to boot off this partition", "setOF", aNone, mDevice|mFolder);
setoption(ksystem, "Fallback system for wrapper or boot blocks", "system", aRequired, mDevice|mFolder);
setoption(kuse9, "If both an X and 9 folder is specified, prefer the 9 one", "use9", aNone, mFolder);
setoption(kverbose, "Verbose mode", "verbose", aNone, mDevice|mFolder|mInfo);
setoption(kwrapper, "Data fork System file to place in HFS+ wrapper", "wrapper", aRequired, mDevice);
setoption(ksystemfile, "Data fork System file to place in blessed System Folder", "systemfile", aRequired, mFolder);
setoption(kxcoff, "Path to bootx.xcoff to be used as StartupFile", "xcoff", aRequired, mDevice);

}
