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
 *  handleDevice.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Dec 6 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: handleDevice.c,v 1.13 2002/05/30 06:43:18 ssen Exp $
 *
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>


#include "enums.h"
#include "structs.h"

#include "bless.h"

int modeDevice(BLContext context, struct clopt commandlineopts[klast], struct clarg actargs[klast]) {
    unsigned char parent[MAXPATHLEN];
    unsigned long pNum;
    int err = 0;
    off_t wrapperBytesLeft = 0;
    struct stat sb;

#if defined(DARWIN)
    void * data = NULL;
    u_int32_t entrypoint, loadbase, size, checksum;
#endif /* DARWIN */

    if(!(geteuid() == 0)) {
		contextprintf(context, kBLLogLevelError,  "Not run as root\n" );
		return 1;
    }


    if(actargs[ksystem].present && actargs[kwrapper].present) {
		snprintf(actargs[kwrapper].argument, kMaxArgLength-1, "%s/..namedfork/rsrc",
		   actargs[ksystem].argument);
		actargs[kwrapper].argument[kMaxArgLength-1] = '\0';
    }


    if(actargs[kformat].present) {
        if(!strcmp("hfs", actargs[kformat].argument)
	   || !actargs[kformat].hasArg) {
           
	  if(actargs[kwrapper].present) {
	    stat(actargs[kwrapper].argument, &sb);
	    if(err < 0) {
	      contextprintf(context, kBLLogLevelError,  "Could not find system file for wrapper %s\n", actargs[kwrapper].argument );
	      return 1;
            }
	    
            wrapperBytesLeft += sb.st_size + 512;
	  }

#if defined(DARWIN)
            err = BLLoadXCOFFLoader(context,
                        actargs[kxcoff].argument,
                        &entrypoint,
                        &loadbase,
                        &size,
                        &checksum,
                        &data);

            if(err) {
                contextprintf(context, kBLLogLevelError,  "Could not load XCOFF loader from %s\n", actargs[kxcoff].argument );
                return 1;
            }

            wrapperBytesLeft += size + 512;
#endif /* DARWIN */
           
            err = BLFormatHFS(context, actargs[kdevice].argument,
				  wrapperBytesLeft,
				  ( actargs[klabel].present ?
                                    (char *)actargs[klabel].argument :
                                    kDefaultHFSLabel),
                                  ( actargs[kfsargs].present ?
                                    (char *)actargs[kfsargs].argument :
                                    ""));
            if(err) {
                contextprintf(context, kBLLogLevelError,  "Error while formatting %s\n", actargs[kdevice].argument );
                return 1;
            }
        } else {
            contextprintf(context, kBLLogLevelError,  "Unsupported filesystem %s\n", actargs[kformat].argument );
            return 1;
        }
    }

    if(actargs[kwrapper].present) {

        if(!actargs[kmount].present) {
			contextprintf(context, kBLLogLevelVerbose,  "No temporary mount point specified for wrapper, using /mnt\n" );
			strcpy(actargs[kmount].argument, "/mnt");
                        actargs[kmount].present = 1;
        }

        err = BLMountHFSWrapper(context, actargs[kdevice].argument, actargs[kmount].argument);
        if(err) {
            contextprintf(context, kBLLogLevelError,  "Error while mounting wrapper for %s\n", actargs[kdevice].argument );
            return 1;
        }

        contextprintf(context, kBLLogLevelVerbose,  "Wrapper for %s mounted at %s\n", actargs[kdevice].argument, actargs[kmount].argument );

        err = BLUpdateHFSWrapper(context, actargs[kmount].argument, actargs[kwrapper].argument);
        if(err) {
            contextprintf(context, kBLLogLevelError,  "Error %d while updating wrapper for %s\n", err, actargs[kdevice].argument );
            return 1;
        }

#if defined(DARWIN)
        if(actargs[kxcoff].present) {
            err = BLCreateBootXXCOFF(context, (void *)data, actargs[kmount].argument);
            if(err) {
                contextprintf(context, kBLLogLevelError,  "Error %d while updating wrapper for %s\n", err, actargs[kdevice].argument );
                return 1;
            }
            err = BLGetParentDevice(context, actargs[kdevice].argument, parent, &pNum);
            if(err) {
                    parent[0] = '\0';
                    pNum = 0;
            }

        }

#endif /* DARWIN */

        if(actargs[kbootblockfile].present) {
			unsigned char bBlocks[1024];

			if(actargs[ksystem].present) {
				err = BLGetBootBlocksFromFile(context, actargs[ksystem].argument, bBlocks);
				if(err) {
					contextprintf(context, kBLLogLevelError,  "Can't get boot blocks from system file %s\n", actargs[ksystem].argument );
					return 1;
				} else {
					contextprintf(context, kBLLogLevelVerbose,  "Boot blocks read from %s\n", actargs[ksystem].argument );
				}
			} else {
				err = BLGetBootBlocksFromDataForkFile(context, actargs[kbootblockfile].argument, bBlocks);
				if(err) {
					contextprintf(context, kBLLogLevelError, "Can't get boot blocks from data-fork file %s\n",
				 actargs[kbootblockfile].argument);
					return 1;
				} else {
					contextprintf(context, kBLLogLevelVerbose,  "Boot blocks read from %s\n", actargs[kbootblockfile].argument );
				}

			}

			err = BLSetBootBlocks(context, actargs[kmount].argument, bBlocks);
			if(err) {
				contextprintf(context, kBLLogLevelError,  "Can't set boot blocks for %s\n", actargs[kmount].argument );
				return 1;
			} else {
				contextprintf(context, kBLLogLevelVerbose,  "Boot blocks set successfully\n" );
			}
        }

        err = BLUnmountHFSWrapper(context, actargs[kdevice].argument, actargs[kmount].argument);
        if(err) {
            contextprintf(context, kBLLogLevelError,  "Error while unmounting wrapper for %s\n", actargs[kdevice].argument );
            return 1;
        }
    }

#if !defined(DARWIN)
    if(actargs[kxcoff].present) {
        err = BLGetParentDevice(context, actargs[kdevice].argument, parent, &pNum);
        if(err) {
                parent[0] = '\0';
                pNum = 0;
        }
        
        err = BLWriteStartupFile(context, actargs[kxcoff].argument, actargs[kdevice].argument, parent, pNum);
        if(err) {
            contextprintf(context, kBLLogLevelError,  "Error while writing StartupFile for %s\n", actargs[kdevice].argument );
            return 1;
        }
    }
#endif /* !DARWIN */

    /* Set Open Firmware to boot off the specified volume*/
    if(actargs[ksetOF].present) {
		err = BLSetOpenFirmwareBootDevice(context, actargs[kdevice].argument);
        if(err) {
            contextprintf(context, kBLLogLevelError,  "Can't set Open Firmware\n" );
            return 1;
        } else {
            contextprintf(context, kBLLogLevelVerbose,  "Open Firmware set successfully\n" );
		}
    }

	return 0;
}
