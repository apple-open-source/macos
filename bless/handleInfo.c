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
/*
 *  handleInfo.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Dec 6 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: handleInfo.c,v 1.8 2002/05/30 06:43:18 ssen Exp $
 *
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "enums.h"
#include "structs.h"

#include "bless.h"

#include <CoreFoundation/CoreFoundation.h>


/* 8 words of "finder info" in volume
 * 0 & 1 often set to blessed system folder
 * boot blocks contain name of system file, name of shell app, and startup app
 * starting w/sys8 added file OpenFolderListDF ... which wins open at mount
 * there are per-file/folder "finder info" fields such as invis, location, etc
 * "next-folder in "open folder list" chain ... first item came from word 2
 * 3 & 5 co-opted in the X timeframe to store dirID's of dual-install sysF's 
 * 6 & 7 is the vsdb stuff (64 bits) to see if sysA has seen diskB
 *
 * 0 is blessed system folder
 * 1 is folder which contains startup app (reserved for Finder these days)
 * 2 is first link in linked list of folders to open at mount (deprecated)
 *   (9 and X are supposed to honor this if set and ignore OpenFolderListDF)
 *   (but the X Finder has only done this flakily)
 * 3 OS 9 blessed system folder (maybe OS X?)
 * 4 thought to be unused (reserved for Finder, once was PowerTalk Inbox)
 * 5 OS X blessed system folder (maybe OS 9?)
 * 6 & 7 are 64 bit volume identifier (high 32 bits in 6; low in 7)
 */

static const char *messages[7][2] = {
        { "No Blessed System Folder", "Blessed System Folder is " },    /* 0 */
        { "No Startup App folder (ignored anyway)", "Startup App folder is " },
        { "Open-folder linked list empty", "1st dir in open-folder list is " },
        { "No OS Classic + X blessed 9 folder", "Classic blessed folder is " },  /* 3 */
        { "Unused field unset", "Thought-to-be-unused field points to " },
        { "No OS Classic + X blessed X folder", "OS X blessed folder is " },  /* 5 */
        { "64-bit VSDB volume id not present", "64-bit VSDB volume id: " }
};

int modeInfo(BLContext context, struct clopt commandlineopts[klast], struct clarg actargs[klast]) {
    int err;
    CFDictionaryRef dict;

    if(!actargs[kinfo].hasArg) {
            char currentString[1024];
            char currentDev[MNAMELEN];
            struct statfs *mnts;
            int vols;

            FILE *pop;
            // we didn't get any volumes, so add them all

            pop = popen("/usr/sbin/nvram boot-device", "r");
            if(pop == NULL) {
                    contextprintf(context, kBLLogLevelError,  "Could not determine current boot device\n" );
                    return 1;
            }

            if(1 != fscanf(pop, "%*s\t%s\n", &(currentString[0]))) {
                    contextprintf(context, kBLLogLevelError,  "Could not parse output from /usr/sbin/nvram\n" );
                    return 1;
            }

            pclose(pop);

            contextprintf(context, kBLLogLevelVerbose,  "Current OF: %s\n", currentString );

            err = BLGetDeviceForOpenFirmwarePath(context, currentString,
                                                currentDev);
            if(err) {
                contextprintf(context, kBLLogLevelError,  "Can't get device for %s: %d\n", currentString, err );
                return 1;

            }

            vols = getmntinfo(&mnts, MNT_NOWAIT);
            if(vols == -1) {
                    contextprintf(context, kBLLogLevelError,  "Error gettings mounts\n" );
                    return 1;
            }

            while(--vols >= 0) {
                if(strncmp(mnts[vols].f_mntfromname, currentDev, strlen(currentDev)+1) == 0) {
                    contextprintf(context, kBLLogLevelVerbose,  "mount: %s\n", mnts[vols].f_mntonname );
                    strcpy(actargs[kinfo].argument, mnts[vols].f_mntonname);
                    break;
                }

            }

    }


    err = BLGetCommonMountPoint(context, actargs[kinfo].argument, "", actargs[kmount].argument);
    if(err) {
            contextprintf(context, kBLLogLevelError,  "Can't get mount point for %s\n", actargs[kinfo].argument );
            return 1;
    }

    err = BLCreateVolumeInformationDictionary(context, actargs[kmount].argument,
                                            (void *)&dict);
    if(err) {
            contextprintf(context, kBLLogLevelError,  "Can't print Finder information\n" );
		return 1;
	}

    if(actargs[kplist].present) {
	CFDataRef		tempData = NULL;

	tempData = CFPropertyListCreateXMLData(kCFAllocatorDefault, dict);

        write(fileno(stdout), CFDataGetBytePtr(tempData), CFDataGetLength(tempData));

	CFRelease(tempData);

    } else {
        CFArrayRef finfo = CFDictionaryGetValue(dict, CFSTR("Finder Info"));
        int j;
        CFNumberRef vsdbref;
        u_int64_t vsdb;

        for(j = 0; j < (8-2); j++) {
            CFDictionaryRef word = CFArrayGetValueAtIndex(finfo, j);
            CFNumberRef dirID = CFDictionaryGetValue(word, CFSTR("Directory ID"));
            CFStringRef path = CFDictionaryGetValue(word, CFSTR("Path"));
            u_int32_t dirint;
            unsigned char cpath[MAXPATHLEN];
            
            if(!CFNumberGetValue(dirID, kCFNumberLongType, &dirint)) {
                continue;
            }

            if(!CFStringGetCString(path, cpath, MAXPATHLEN, kCFStringEncodingUTF8)) {
                continue;
            }

            contextprintf(context, kBLLogLevelNormal,
                        "finderinfo[%i]: %6lu => %s%s\n", j, dirint,
                        messages[j][dirID > 0], cpath);

        }


        vsdbref = CFDictionaryGetValue(dict, CFSTR("VSDB ID"));
        CFNumberGetValue(vsdbref, kCFNumberSInt64Type, &vsdb);
        
        
    	contextprintf(context, kBLLogLevelNormal, "%s 0x%016qX\n", messages[6][1],
		      vsdb);

    }
    
    
    CFRelease(dict);
	return 0;
}