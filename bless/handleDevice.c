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
 *  handleDevice.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Dec 6 2001.
 *  Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: handleDevice.c,v 1.47 2005/02/08 00:18:45 ssen Exp $
 *
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/paths.h>
#include <string.h>


#include "enums.h"
#include "structs.h"

#include "bless.h"

extern int blesscontextprintf(BLContextPtr context, int loglevel, char const *fmt, ...);
extern int setboot(BLContextPtr context, char *device, CFDataRef bootxData,
				   CFDataRef labelData);

int modeDevice(BLContextPtr context, struct clarg actargs[klast]) {
    int err = 0;
	CFDataRef labeldata = NULL;
	CFDataRef bootXdata = NULL;
	

    if(!(geteuid() == 0)) {
		blesscontextprintf(context, kBLLogLevelError,  "Not run as root\n" );
		return 1;
    }




    /* try to grovel the HFS+ catalog and update a label if present */
	if(actargs[klabelfile].present) {
		err = BLLoadFile(context, actargs[klabelfile].argument, 0, &labeldata);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError, "Can't load label '%s'\n",
							   actargs[klabelfile].argument);
			return 2;
		}
	} else if(actargs[klabel].present) {
		err = BLGenerateOFLabel(context, actargs[klabel].argument, &labeldata);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError, "Can't render label '%s'\n",
							   actargs[klabel].argument);
			return 3;
		}
	}
    
	if(actargs[kbootinfo].present) {
		if(!actargs[kbootinfo].hasArg) {
            blesscontextprintf(context, kBLLogLevelError,
							   "BootX file must be specified in Device Mode\n");
			return 4;
        }
		
		err = BLLoadFile(context, actargs[kbootinfo].argument, 0, &bootXdata);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError,  "Could not load BootX data from %s\n",
							   actargs[kbootinfo].argument);
		}
	}
		
    /* Set Open Firmware to boot off the specified volume*/
    if(actargs[ksetboot].present) {
		err = setboot(context, actargs[kdevice].argument, bootXdata, labeldata);
		if(err) {
			return 3;
		}
    } else if(labeldata) {
		err = BLSetOFLabelForDevice(context, actargs[kdevice].argument, labeldata);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError,  "Error while setting label for %s\n", actargs[kdevice].argument );
			return 3;
		}		
	}

	if(labeldata) CFRelease(labeldata);
	if(bootXdata) CFRelease(bootXdata);

    return 0;
}
