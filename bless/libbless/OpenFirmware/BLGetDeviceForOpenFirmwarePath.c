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
 *  BLGetDeviceForOpenFirmwarePath.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Jan 24 2001.
 *  Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLGetDeviceForOpenFirmwarePath.c,v 1.12 2003/07/25 01:16:25 ssen Exp $
 *
 *  $Log: BLGetDeviceForOpenFirmwarePath.c,v $
 *  Revision 1.12  2003/07/25 01:16:25  ssen
 *  When mapping OF -> device, if we found an Apple_Boot, try to
 *  find the corresponding partition that is the real root filesystem
 *
 *  Revision 1.11  2003/07/22 15:58:36  ssen
 *  APSL 2.0
 *
 *  Revision 1.10  2003/04/19 00:11:14  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.9  2003/04/16 23:57:35  ssen
 *  Update Copyrights
 *
 *  Revision 1.8  2002/08/22 04:18:07  ssen
 *  typo
 *
 *  Revision 1.7  2002/08/22 00:38:42  ssen
 *  Gah. Search for ",\\:tbxi" from the end of the OF path
 *  instead of the beginning. For SCSI cards that use commas
 *  in the OF path, the search was causing a mis-parse.
 *
 *  Revision 1.6  2002/06/11 00:50:51  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
 *  Revision 1.5  2002/04/27 17:55:00  ssen
 *  Rewrite output logic to format the string before sending of to logger
 *
 *  Revision 1.4  2002/04/25 07:27:30  ssen
 *  Go back to using errorprint and verboseprint inside library
 *
 *  Revision 1.3  2002/02/23 04:13:06  ssen
 *  Update to context-based API
 *
 *  Revision 1.2  2002/02/03 17:25:22  ssen
 *  Fix "bless -info" usage to determine current device
 *
 *  Revision 1.1  2002/02/03 17:02:35  ssen
 *  Add of -> dev function
 *
 */
 
#import <mach/mach_error.h>
#import <IOKit/IOKitLib.h>

#import <CoreFoundation/CoreFoundation.h>

#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/mount.h>

#include <sys/stat.h>
 
#include "bless.h"
#include "bless_private.h"

extern int getPNameAndPType(BLContextPtr context,
			    unsigned char target[],
			    unsigned char pname[],
			    unsigned char ptype[]);

int BLGetDeviceForOpenFirmwarePath(BLContextPtr context, char ofstring[], unsigned char mntfrm[]) {
 
    kern_return_t           kret;
    mach_port_t             ourIOKitPort;
    char newof[1024];
    unsigned char targetPName[MAXPATHLEN];
    unsigned char targetPType[MAXPATHLEN];
    unsigned char parentDev[MNAMELEN];
    unsigned long slice = 0;
    int ret;
    
    BLPartitionType partitionType = 0;
    
    char *comma = NULL;;

    strcpy(newof, ofstring);

    comma = strrchr(newof, ',');
    if(comma == NULL) { // there should be a ,\\:tbxi
        return 1;
    }

    // strip off the booter path
    *comma = '\0';

     // Obtain the I/O Kit communication handle.
    if((kret = IOMasterPort(bootstrap_port, &ourIOKitPort)) != KERN_SUCCESS) {
      return 2;
    }

    strcpy(mntfrm, "/dev/");

    kret = IOServiceOFPathToBSDName(ourIOKitPort,
                         newof,
                         mntfrm + 5);

    contextprintf(context, kBLLogLevelVerbose,  "bsd name is %s\n", mntfrm );

    ret = getPNameAndPType(context, mntfrm+5, targetPName, targetPType);
    if(ret) {
	contextprintf(context, kBLLogLevelVerbose,  "Could not get partition type for %s\n", mntfrm );
	
	return 3;
    }
    
    contextprintf(context, kBLLogLevelVerbose,  "Partition name is %s. Partition type is %s\n",  targetPName, targetPType);
    
    if(strcmp("Apple_Boot", targetPType) == 0) {
	// this is a auxiliary booter partition
	contextprintf(context, kBLLogLevelVerbose,  "Looking for root partition\n");
	
	ret = BLGetParentDeviceAndPartitionType(context,mntfrm,
						parentDev,
						&slice,
					 &partitionType);

	if(ret) {
	    contextprintf(context, kBLLogLevelVerbose,  "Could not get information about partition map for %s\n", mntfrm);
	    return 3;
	    
	}
	
	slice += 1; // n+1 for "real" root partition
	sprintf(mntfrm, "%ss%lu", parentDev, slice);

	ret = getPNameAndPType(context, mntfrm+5, targetPName, targetPType);
	if(ret) {
	    contextprintf(context, kBLLogLevelVerbose,  "Could not get partition type for %s\n", mntfrm );
	    
	    return 3;
	}

	contextprintf(context, kBLLogLevelVerbose,  "Partition name is %s. Partition type is %s\n",  targetPName, targetPType);
	
	if(strcmp("Apple_Boot", targetPType) == 0) {
	    contextprintf(context, kBLLogLevelError,  "Apple_Boot followed by another Apple_Boot\n");
	    return 4;
	}

	if(strcmp("Apple_HFS", targetPType) == 0) {
	    contextprintf(context, kBLLogLevelError,  "Apple_HFS does not require an Apple_Boot\n");
	    return 4;
	}
    }
    
    return 0;

 }
