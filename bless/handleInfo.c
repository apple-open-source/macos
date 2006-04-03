/*
 * Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
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
/*
 *  handleInfo.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Dec 6 2001.
 *  Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: handleInfo.c,v 1.40 2006/01/02 22:27:27 ssen Exp $
 *
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/param.h>

#include <sys/socket.h>
#include <net/if.h>
#include <arpa/nameser.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "enums.h"
#include "structs.h"

#include "bless.h"
#include "bless_private.h"

#include <CoreFoundation/CoreFoundation.h>

extern int blesscontextprintf(BLContextPtr context, int loglevel, char const *fmt, ...)
    __attribute__ ((format (printf, 3, 4)));

static int interpretEFIString(BLContextPtr context, CFStringRef efiString, 
                              char *bootdevice);

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
 * 3 OS 9 blessed system folder (maybe OS X?)
 * 4 thought to be unused (reserved for Finder, once was PowerTalk Inbox)
 * 5 OS X blessed system folder (maybe OS 9?)
 * 6 & 7 are 64 bit volume identifier (high 32 bits in 6; low in 7)
 */

#define MISSINGMSG "<missing>"
static const char *messages[7][2] = {

       { "No Blessed System Folder", "Blessed System Folder is " },    /* 0 */
       { "No Blessed System File", "Blessed System File is " },
       { "Open-folder linked list empty", "1st dir in open-folder list is " },
       { "No OS 9 + X blessed 9 folder", "OS 9 blessed folder is " },  /* 3 */
       { "Unused field unset", "Thought-to-be-unused field points to " },
       { "No OS 9 + X blessed X folder", "OS X blessed folder is " },  /* 5 */
       { "64-bit VSDB volume id not present", "64-bit VSDB volume id: " }
       
};

int modeInfo(BLContextPtr context, struct clarg actargs[klast]) {
    int                 err;
    CFDictionaryRef     dict;
    int                 isNetboot = 0;

    if(!actargs[kinfo].hasArg ||  actargs[kgetboot].present) {
            char currentString[1024];
            char currentDev[1024]; // may contain URLs like bsdp://foo
            struct statfs *mnts;
            int vols;
			BLPreBootEnvType	preboot;
			
			err = BLGetPreBootEnvironmentType(context, &preboot);
			if(err)
				return 1;

            if(preboot == kBLPreBootEnvType_OpenFirmware) {

                FILE *pop;
    
                pop = popen("/usr/sbin/nvram boot-device", "r");
                if(pop == NULL) {
                        blesscontextprintf(context, kBLLogLevelError,  "Could not determine current boot device\n" );
                        return 1;
                }
    
                if(1 != fscanf(pop, "%*s\t%s\n", &(currentString[0]))) {
                        blesscontextprintf(context, kBLLogLevelError,  "Could not parse output from /usr/sbin/nvram\n" );
                        return 1;
                }
    
                pclose(pop);
    
                blesscontextprintf(context, kBLLogLevelVerbose,  "Current OF: %s\n", currentString );

                // XXX temporarily trap enet:bootp. Doesn't work with enet1:bootp
                if(strncmp(currentString, "enet:", strlen("enet:")) == 0) {
                    blesscontextprintf(context, kBLLogLevelVerbose,  "Synthesizing BSDP boot device\n" );
                    strcpy(currentDev, "bsdp://en0@255.255.255.255");
                    isNetboot = 1;
                } else {
                    err = BLGetDeviceForOpenFirmwarePath(context, currentString,
                                                        currentDev);
                    if(err) {
                        blesscontextprintf(context, kBLLogLevelError,  "Can't get device for %s: %d\n", currentString, err );
                        return 1;
        
                    }
                }
            } else if(preboot == kBLPreBootEnvType_EFI) {
				// fill this in later
				//struct statfs sb;
                CFStringRef     efibootdev = NULL;
                char currentString[1024];

                err = BLCopyEFINVRAMVariableAsString(context,
                                                     CFSTR("efi-boot-device"),
                                                     &efibootdev);
                    
                if(err || efibootdev == NULL) {
                    blesscontextprintf(context, kBLLogLevelError,
                                       "Can't access \"efi-boot-device\" NVRAM variable\n");
                    return 1;
                }
                
                if(CFStringGetCString(efibootdev, currentString, sizeof(currentString), kCFStringEncodingUTF8)) {
                    blesscontextprintf(context, kBLLogLevelVerbose,  "Current EFI boot device string is: '%s'\n",
                                       currentString);                    
                }
                
                err = interpretEFIString(context, efibootdev, currentDev);
                if(err) {
                    blesscontextprintf(context, kBLLogLevelError,
                                       "Can't interpet EFI boot device\n");
                    return 2;                    
                }
                				
			} else {
				blesscontextprintf(context, kBLLogLevelError,  "Unknown preboot environment\n");
				return 1;
			}

	    if( actargs[kgetboot].present) {
                if(actargs[kplist].present) {
                    CFStringRef vol = CFStringCreateWithCString(kCFAllocatorDefault,
                                                                currentDev,
                                                                kCFStringEncodingUTF8);
                    CFMutableDictionaryRef dict = NULL;
                    CFDataRef		tempData = NULL;
                    
                    dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                     &kCFTypeDictionaryKeyCallBacks,
                                                     &kCFTypeDictionaryValueCallBacks);
                    CFDictionaryAddValue(dict, CFSTR("Boot Volume"), vol);
                    CFRelease(vol);
                    
                    tempData = CFPropertyListCreateXMLData(kCFAllocatorDefault, dict);
                    
                    write(fileno(stdout), CFDataGetBytePtr(tempData), CFDataGetLength(tempData));
                    
                    CFRelease(tempData);
                    CFRelease(dict);
                    
                } else {
                    blesscontextprintf(context, kBLLogLevelNormal, "%s\n", currentDev);
                }
                return 0;
	    }
	    
            vols = getmntinfo(&mnts, MNT_NOWAIT);
            if(vols == -1) {
                    blesscontextprintf(context, kBLLogLevelError,  "Error gettings mounts\n" );
                    return 1;
            }

            while(--vols >= 0) {
                struct statfs sb;
                
                // somewhat redundant, but blsustatfs will canonicalize the mount device
                if(0 != blsustatfs(mnts[vols].f_mntonname, &sb)) {
                    continue;
                }
                
                if(strncmp(sb.f_mntfromname, currentDev, strlen(currentDev)+1) == 0) {
                    blesscontextprintf(context, kBLLogLevelVerbose,  "mount: %s\n", mnts[vols].f_mntonname );
                    strcpy(actargs[kinfo].argument, mnts[vols].f_mntonname);
                    actargs[kinfo].hasArg = 1;
                    break;
                }

            }

	    if(!actargs[kinfo].hasArg) {
	      blesscontextprintf(context, kBLLogLevelError,
			    "Volume for OpenFirmware path %s is not available\n",
			    currentString);
	      return 2;
	    }
    }


    err = BLGetCommonMountPoint(context, actargs[kinfo].argument, "", actargs[kmount].argument);
    if(err) {
            blesscontextprintf(context, kBLLogLevelError,  "Can't get mount point for %s\n", actargs[kinfo].argument );
            return 1;
    }

    err = BLCreateVolumeInformationDictionary(context, actargs[kmount].argument,
                                            &dict);
    if(err) {
            blesscontextprintf(context, kBLLogLevelError,  "Can't print Finder information\n" );
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
        uint64_t vsdb;

        for(j = 0; j < (8-2); j++) {
            CFDictionaryRef word = CFArrayGetValueAtIndex(finfo, j);
            CFNumberRef dirID = CFDictionaryGetValue(word, CFSTR("Directory ID"));
            CFStringRef path = CFDictionaryGetValue(word, CFSTR("Path"));
            uint32_t dirint;
            char cpath[MAXPATHLEN];
            
            if(!CFNumberGetValue(dirID, kCFNumberLongType, &dirint)) {
                continue;
            }

            if(dirint > 0 && CFStringGetLength(path) == 0) {
                    strcpy(cpath, MISSINGMSG);
            } else {
                    if(!CFStringGetCString(path, cpath, MAXPATHLEN, kCFStringEncodingUTF8)) {
                            continue;
                    }
            }

            blesscontextprintf(context, kBLLogLevelNormal,
                        "finderinfo[%i]: %6u => %s%s\n", j, dirint,
                        messages[j][dirint > 0], cpath);

        }


        vsdbref = CFDictionaryGetValue(dict, CFSTR("VSDB ID"));
        CFNumberGetValue(vsdbref, kCFNumberSInt64Type, &vsdb);
        
        
    	blesscontextprintf(context, kBLLogLevelNormal, "%s 0x%016qX\n", messages[6][1],
		      vsdb);

    }
    
    
    CFRelease(dict);
    return 0;
}

static int interpretEFIString(BLContextPtr context, CFStringRef efiString, 
                              char *bootdevice)
{
    char                interface[IF_NAMESIZE];
    char                host[NS_MAXDNAME];
    char                path[1024];
    int                 ret;
    
    ret = BLInterpretEFIXMLRepresentationAsDevice(context,
                                                  efiString,
                                                  path);
    if(ret == 0) {
        blesscontextprintf(context, kBLLogLevelVerbose,  "Disk boot device detected\n" );
        
        sprintf(bootdevice, "/dev/%s", path);
        
        return 0;
    } else {
        ret = BLInterpretEFIXMLRepresentationAsNetworkPath(context,
                                                           efiString,
                                                           interface,
                                                           host,
                                                           path);
        
        if(ret == 0) {
            blesscontextprintf(context, kBLLogLevelVerbose,  "Network boot device detected\n" );
            
            if(strlen(path) > 0) {
                sprintf(bootdevice, "tftp://%s@%s/%s", interface, host, path);
            } else {
                sprintf(bootdevice, "bsdp://%s@%s", interface, host);            
            }
            
            return 0;
        }            
    }

    blesscontextprintf(context, kBLLogLevelError,  "Could not interpret boot device as either network or disk\n" );
    
    return 1;
}
