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
 *  handleDevice.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Dec 6 2001.
 *  Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: handleDevice.c,v 1.36 2003/08/04 06:50:05 ssen Exp $
 *
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/paths.h>
#include <string.h>


#include "enums.h"
#include "structs.h"

#include "bless.h"

extern int blesscontextprintf(BLContextPtr context, int loglevel, char const *fmt, ...);

int modeDevice(BLContextPtr context, struct clopt commandlineopts[klast], struct clarg actargs[klast]) {
    int err = 0;

    off_t wrapperBytesLeft = 0;
    struct stat sb;

    if(!(geteuid() == 0)) {
		blesscontextprintf(context, kBLLogLevelError,  "Not run as root\n" );
		return 1;
    }


    if(actargs[ksystem].present && actargs[kwrapper].present) {
		snprintf(actargs[kwrapper].argument, kMaxArgLength-1, "%s"_PATH_RSRCFORKSPEC,
		   actargs[ksystem].argument);
		actargs[kwrapper].argument[kMaxArgLength-1] = '\0';
    }


    if(actargs[kformat].present) {
        if(!strcmp("hfs", actargs[kformat].argument)
	   || !actargs[kformat].hasArg) {
           
	  if(actargs[kwrapper].present) {
	    stat(actargs[kwrapper].argument, &sb);
	    if(err < 0) {
	      blesscontextprintf(context, kBLLogLevelError,  "Could not find system file for wrapper %s\n", actargs[kwrapper].argument );
	      return 1;
            }
	    
            wrapperBytesLeft += sb.st_size + 512;
	  }
           
        err = BLFormatHFS(context, actargs[kdevice].argument,
                                wrapperBytesLeft,
                                ( actargs[klabel].present ?
                                (char *)actargs[klabel].argument :
                                kDefaultHFSLabel),
                                ( actargs[kfsargs].present ?
                                (char *)actargs[kfsargs].argument :
                                ""));
            if(err) {
                blesscontextprintf(context, kBLLogLevelError,  "Error while formatting %s\n", actargs[kdevice].argument );
                return 1;
            }
        } else {
            blesscontextprintf(context, kBLLogLevelError,  "Unsupported filesystem %s\n", actargs[kformat].argument );
            return 1;
        }
    }

    if(actargs[kwrapper].present) {

        if(!actargs[kmount].present) {
			blesscontextprintf(context, kBLLogLevelVerbose,  "No temporary mount point specified for wrapper, using /mnt\n" );
			strcpy(actargs[kmount].argument, "/mnt");
                        actargs[kmount].present = 1;
        }

        err = BLMountHFSWrapper(context, actargs[kdevice].argument, actargs[kmount].argument);
        if(err) {
            blesscontextprintf(context, kBLLogLevelError,  "Error while mounting wrapper for %s\n", actargs[kdevice].argument );
            return 1;
        }

        blesscontextprintf(context, kBLLogLevelVerbose,  "Wrapper for %s mounted at %s\n", actargs[kdevice].argument, actargs[kmount].argument );

        err = BLUpdateHFSWrapper(context, actargs[kmount].argument, actargs[kwrapper].argument);
        if(err) {
            blesscontextprintf(context, kBLLogLevelError,  "Error %d while updating wrapper for %s\n", err, actargs[kdevice].argument );
            return 1;
        }

        if(actargs[kbootblockfile].present) {
	    CFDataRef bbdata = NULL;
	    
			err = BLLoadFile(context, actargs[kbootblockfile].argument, 0, &bbdata);
			    if(err) {
				    blesscontextprintf(context, kBLLogLevelError, "Can't get boot blocks from data-fork file %s\n",
				actargs[kbootblockfile].argument);
				    return 1;
			    } else {
				    blesscontextprintf(context, kBLLogLevelVerbose,  "Boot blocks read from %s\n", actargs[kbootblockfile].argument );
			    }
			
			err = BLSetBootBlocks(context, actargs[kmount].argument, bbdata);
			CFRelease(bbdata);

			if(err) {
				blesscontextprintf(context, kBLLogLevelError,  "Can't set boot blocks for %s\n", actargs[kmount].argument );
				return 1;
			} else {
				blesscontextprintf(context, kBLLogLevelVerbose,  "Boot blocks set successfully\n" );
			}
        }

        err = BLUnmountHFSWrapper(context, actargs[kdevice].argument, actargs[kmount].argument);
        if(err) {
            blesscontextprintf(context, kBLLogLevelError,  "Error while unmounting wrapper for %s\n", actargs[kdevice].argument );
            return 1;
        }
    }


    
    /* Set Open Firmware to boot off the specified volume*/
    if(actargs[ksetboot].present) {
	unsigned char parDev[MNAMELEN];
	unsigned long slice;
	BLPartitionType ptype;

	err = BLGetParentDeviceAndPartitionType(context, actargs[kdevice].argument, parDev, &slice, &ptype);
	if(err) {
	    return 3;
	}

	blesscontextprintf(context, kBLLogLevelVerbose, "Device '%s' is part of an %s partition map\n",
		    actargs[kdevice].argument,
		    ptype == kBLPartitionType_APM ? "Apple" : (ptype == kBLPartitionType_MBR ? "MBR" : "Unknown"));

	if(ptype == kBLPartitionType_MBR) {
	    err = BLSetActiveBIOSBootDevice(context, actargs[kdevice].argument);
	    if(err) {
		blesscontextprintf(context, kBLLogLevelError,  "Can't set active boot partition\n" );
		return 4;
	    } else {
		blesscontextprintf(context, kBLLogLevelVerbose,  "%s set as active boot partition\n" , actargs[kdevice].argument);
	    }
	} else if(ptype == kBLPartitionType_APM && BLIsOpenFirmwarePresent(context)) {
	    err = BLSetOpenFirmwareBootDevice(context, actargs[kdevice].argument);
	    if(err) {
		blesscontextprintf(context, kBLLogLevelError,  "Can't set Open Firmware\n" );
		return 1;
	    } else {
		blesscontextprintf(context, kBLLogLevelVerbose,  "Open Firmware set successfully\n" );
	    }
	}
    }

    return 0;
}
