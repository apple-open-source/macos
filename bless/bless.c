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
 *  bless.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Wed Nov 14 2001.
 *  Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: bless.c,v 1.58 2003/08/04 06:38:45 ssen Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include "enums.h"
#include "structs.h"

#include "bless.h"

#define xstr(s) str(s)
#define str(s) #s

struct clopt commandlineopts[klast];
struct clarg actargs[klast];

int modeInfo(BLContextPtr context, struct clopt commandlineopts[klast], struct clarg actargs[klast]);
int modeDevice(BLContextPtr context, struct clopt commandlineopts[klast], struct clarg actargs[klast]);
int modeFolder(BLContextPtr context, struct clopt commandlineopts[klast], struct clarg actargs[klast]);

int blesslog(void *context, int loglevel, const char *string);
static void initConfig();
void usage(struct clopt[]);
void usage_short();

void arg_err(char *message, char *opt);

int main (int argc, char * argv[])
{

    int i;
    
    BLContext context;
    struct blesscon bcon;

    bcon.quiet = 0;
    bcon.verbose = 0;

    context.version = 0;
    context.logstring = blesslog;
    context.logrefcon = &bcon;
    
    initConfig();
	
    if(argc == 1) {
        arg_err(NULL, NULL);
    }
        
    /* start at 1, since argc >=2 */
    for(i=1; i < argc; i++) {
        int j;
        int found = 0;
        
        /* check against each option */
        for(j=0; j < klast; j++) {

            /* if it matches the option text */
            if(!strcasecmp(&(argv[i][1]), commandlineopts[j].flag)) {

                if(commandlineopts[j].takesarg == aRequired) {
                    if(i+1 >= argc) {
			arg_err("Missing argument for option", argv[i]); /* no arg given */
		    }
                    i++;
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

	if(strcmp("-h", argv[i]) == 0) {
	    actargs[khelp].present = 1;
	    found = 1;
	}

        /* if the option wasn't found, we have a problem */
        if(!found) {
            arg_err("Unrecognized argument:", argv[i]);
        } else {
	    actargs[j].present = 1;
	}
    }

    bcon.verbose = actargs[kverbose].present ? 1 : 0;
    bcon.quiet = actargs[kquiet].present ? 1 : 0;

    // hack for now so the frontend can parse the new option
    if(actargs[ksetOF].present && !actargs[ksetboot].present) {
      actargs[ksetboot].present = 1;
    }

    /* There are four modes of execution: help, info, device, folder.
     * These are all one-way function jumps.
     */

    if(actargs[khelp].present) {
	usage(commandlineopts);
    }
	
    /* If it was requested, print out the Finder Info words */
    if(actargs[kinfo].present || actargs[kversion].present || actargs[kgetboot].present) {
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
setoption(kdevice, "Unmounted block device to operate on", "device", aRequired, mDevice);
setoption(kfolder, "Darwin/Mac OS X folder to be blessed", "folder", aRequired, mFolder);
setoption(kfolder9, "Classic/Mac OS 9 folder to be blessed", "folder9", aRequired, mFolder);
setoption(kformat, "Format the device with the given filesystem", "format", aOptional, mDevice);
setoption(kfsargs, "Extra arguments to newfs", "fsargs", aRequired, mDevice);
setoption(kgetboot, "Get boot device", "getBoot", aNone, mInfo);
setoption(khelp, "Usage statement", "help", aNone, mGlobal);
setoption(kinfo, "Print out Finder info fields for the specified volume", "info", aOptional, mInfo);
setoption(klabel, "Label for a volume", "label", aRequired, mDevice|mFolder);
setoption(klabelfile, "Label bitmap volume", "labelfile", aRequired, mDevice|mFolder);
setoption(kmount, "Mount point to use", "mount", aRequired, mFolder|mDevice);
setoption(kopenfolder, "Directory to open automatically in Finder", "openfolder", aRequired, mFolder);
setoption(kquiet, "Quiet mode", "quiet", aNone, mGlobal);
setoption(kplist, "Output in plist format", "plist", aNone, mInfo);
setoption(ksave9, "Save the existing 9 blessed folder", "save9", aNone, mFolder);
setoption(ksaveX, "Save the existing X blessed folder", "saveX", aNone, mFolder);
setoption(ksetboot, "Set machine to boot off this partition", "setBoot", aNone, mDevice|mFolder);
setoption(ksetOF, "Set OF to boot off this partition (deprecated)", "setOF", aNone, mDevice|mFolder);
setoption(ksystem, "Fallback system for wrapper or boot blocks", "system", aRequired, mDevice|mFolder);
setoption(kuse9, "If both an X and 9 folder is specified, prefer the 9 one", "use9", aNone, mFolder);
setoption(kverbose, "Verbose mode", "verbose", aNone, mGlobal);
setoption(kwrapper, "Data fork System file to place in HFS+ wrapper", "wrapper", aRequired, mDevice);
setoption(kstartupfile, "Path to data to by used as StartupFile", "startupfile", aRequired, mDevice);
setoption(ksystemfile, "Data fork System file to place in blessed System Folder", "systemfile", aRequired, mFolder);
setoption(kxcoff, "Path to bootx.xcoff to be used as StartupFile (ignored)", "xcoff", aRequired, mDevice|mFolder);
setoption(kversion, "Print bless version and exit", "version", aNone, mInfo);

}

int blesscontextprintf(BLContextPtr context, int loglevel, char const *fmt, ...) {
    int ret;
    char *out;
    va_list ap;

    va_start(ap, fmt);
#if OSX_TARGET < 1020
    out = malloc(1024);
    ret = vsnprintf(out, 1024, fmt, ap);  
#else
    ret = vasprintf(&out, fmt, ap);  
#endif
    va_end(ap);
    
    if((ret == -1) || (out == NULL)) {
        return context->logstring(context->logrefcon, loglevel, "Memory error, log entry not available");
    }

    ret = context->logstring(context->logrefcon, loglevel, out);
    free(out);
    return ret;
}

void arg_err(char *message, char *opt) {
    if(!(message == NULL && opt == NULL))
	fprintf(stderr, "%s \"%s\"\n", message, opt);

    usage_short();
}
