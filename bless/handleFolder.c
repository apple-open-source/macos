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
 *  handleFolder.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Dec 6 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: handleFolder.c,v 1.14 2002/05/30 06:43:18 ssen Exp $
 *
 *
 */

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "enums.h"
#include "structs.h"

#include "bless.h"


/* From Finder.h */
#define  kIsInvisible                  0x4000 /* Files and folders */


int modeFolder(BLContext context, struct clopt commandlineopts[klast], struct clarg actargs[klast]) {


	int err;
	int isHFS;

    u_int32_t folderXid = 0;                   // The directory ID specified by folderXpath
    u_int32_t folder9id = 0;                   // The directory ID specified by folder9path

    if(!(geteuid() == 0)) {
		contextprintf(context, kBLLogLevelError,  "Not run as root\n" );
		return 1;
    }


    /* If user gave options that require BootX creation, do it now. */
    if(actargs[kbootinfo].present) {
        if(actargs[kfolder].present) {
                CFDataRef bootXdata = NULL;
                
                err = BLLoadFile(context, actargs[kbootinfo].argument, 0, (void **)&bootXdata);
                if(err) {
                        contextprintf(context, kBLLogLevelError,  "Could not load BootX data from %s\n", actargs[kbootinfo].argument);
                } else {
                        
                    err = BLCreateFile(context, (void *)bootXdata, actargs[kfolder].argument, "BootX", 0, 'tbxi', 'chrp');
                    if(err) {
                            contextprintf(context, kBLLogLevelError,  "Could not create BootX in %s\n", actargs[kfolder].argument );
                    } else {
                            contextprintf(context, kBLLogLevelVerbose,  "BootX created successfully in %s\n", actargs[kfolder].argument );
                    }
		}
                
            } else {
                    contextprintf(context, kBLLogLevelVerbose,  "Could not create BootX, no X folder specified\n" );
            }
    } else {
        contextprintf(context, kBLLogLevelVerbose,  "No BootX creation requested\n" );
    }

    // copy system file over
    if(actargs[ksystemfile].present) {
        if(actargs[kfolder9].present) {
            CFDataRef systemdata = NULL;


            err = BLLoadFile(context, actargs[ksystemfile].argument, 0, (void **)&systemdata);
            if(err) {
                    contextprintf(context, kBLLogLevelError,  "Could not load system file data from %s\n", actargs[ksystemfile].argument);
            } else {
                err = BLCreateFile(context, (void *)systemdata, actargs[kfolder9].argument,
                                    "System", 1, 'zsys', 'MACS');
                if(err) {
                        contextprintf(context, kBLLogLevelError,  "Could not create System in %s\n", actargs[kfolder9].argument );
                } else {
                        contextprintf(context, kBLLogLevelVerbose,  "System created successfully in %s\n", actargs[kfolder9].argument );
                }
            }
        } else {
            contextprintf(context, kBLLogLevelError,  "Could not create system file, no 9 folder specified\n" );
        }
    }

    if(actargs[kmount].present) {
		err = BLGetCommonMountPoint(context, actargs[kmount].argument, "", actargs[kmount].argument);
		if(err) {
			contextprintf(context, kBLLogLevelError,  "Can't determine mount point of '%s'\n", actargs[kmount].argument );
		} else {
			contextprintf(context, kBLLogLevelVerbose,  "Mount point is '%s'\n", actargs[kmount].argument );
		}
                
                actargs[ksave9].present = 1;
                actargs[ksaveX].present = 1;

    } else {
		/* We know that at least one folder has been specified */
		err = BLGetCommonMountPoint(context, actargs[kfolder].argument, actargs[kfolder9].argument, actargs[kmount].argument);
		if(err) {
			contextprintf(context, kBLLogLevelError, "Can't determine mount point of '%s' and '%s'\n", actargs[kfolder].argument,
			   actargs[kfolder9].argument);
			return 1;
		} else {
            contextprintf(context, kBLLogLevelVerbose, "Common mount point of '%s' and '%s' is %s\n",
                                        actargs[kfolder].argument,
                                        actargs[kfolder9].argument, actargs[kmount].argument );
		}
    }

    err = BLIsMountHFS(context, actargs[kmount].argument, &isHFS);
    if(err) {
		contextprintf(context, kBLLogLevelError,  "Could not determine filesystem of %s\n", actargs[kmount].argument );
		return 1;
    }

    if(isHFS && (actargs[kfolder].present || actargs[kfolder9].present)) {
		u_int32_t oldwords[8];
		int useX = 1;

		err = BLGetVolumeFinderInfo(context, actargs[kmount].argument, oldwords);
		if(err) {
			contextprintf(context, kBLLogLevelError,  "Error getting old Finder info words for %s\n", actargs[kmount].argument );
			return 1;
		}

		if(actargs[ksave9].present) {
			folder9id = oldwords[3];
			contextprintf(context, kBLLogLevelVerbose,  "Saved folder 9\n" );
		}
		if(actargs[ksaveX].present) {
			folderXid = oldwords[5];
			contextprintf(context, kBLLogLevelVerbose,  "Saved folder X\n" );
		}

		/* We shouldn't need to create anything else at this point. Just bless */

		/* First get any directory IDs we need */
		if(actargs[kfolder].present) {
			err = BLGetFileID(context, actargs[kfolder].argument, &folderXid);
			if(err) {
				contextprintf(context, kBLLogLevelError,  "Error while get directory ID of %s\n", actargs[kfolder].argument );
			} else {
				contextprintf(context, kBLLogLevelVerbose,  "Got directory ID of %u for %s\n", folderXid, actargs[kfolder].argument );
			}
		}

		if(actargs[kfolder9].present) {
			err = BLGetFileID(context, actargs[kfolder9].argument, &folder9id);
			if(err) {
				contextprintf(context, kBLLogLevelError,  "Error while get directory ID of %s\n", actargs[kfolder9].argument );
			} else {
				contextprintf(context, kBLLogLevelVerbose,  "Got directory ID of %u for %s\n", folder9id, actargs[kfolder9].argument );
			}
		}

		if(actargs[kuse9].present) {
			useX = 0;
		}

		/* Bless the folders */
		err = BLBlessDir(context, actargs[kmount].argument, folderXid, folder9id, useX);
		if(err) {
			contextprintf(context, kBLLogLevelError,  "Can't bless directories\n" );
			return 1;
		} else {
            contextprintf(context, kBLLogLevelVerbose,  "Volume at %s blessed successfully\n", actargs[kmount].argument );
		}

		if(actargs[kbootblocks].present || actargs[kbootblockfile].present) {
			unsigned char bBlocks[1024];

			if(actargs[kbootblockfile].present) {
				err = BLGetBootBlocksFromDataForkFile(context, actargs[kbootblockfile].argument, bBlocks);
				if(err) {
					contextprintf(context, kBLLogLevelError, "Can't get boot blocks from data-fork file %s\n",
				 actargs[kbootblockfile].argument);
					return 1;
				} else {
					contextprintf(context, kBLLogLevelVerbose,  "Boot blocks read from %s\n", actargs[kbootblockfile].argument );
				}
			} else if(folder9id) {
				err = BLGetBootBlocksFromFolder(context, actargs[kmount].argument, folder9id, bBlocks);
				if(err) {
					contextprintf(context, kBLLogLevelError,  "Can't get boot blocks from system file\n" );
					return 1;
				} else {
					contextprintf(context, kBLLogLevelVerbose,  "Boot blocks read from %s\n", actargs[ksystem].argument );
				}
			} else {
				contextprintf(context, kBLLogLevelError,  "Don't know how to get boot blocks\n" );
				return 1;
			}

			err = BLSetBootBlocks(context, actargs[kmount].argument, bBlocks);
			if(err) {
				contextprintf(context, kBLLogLevelError,  "Can't set boot blocks for %s\n", actargs[kmount].argument );
				return 1;
			} else {
				contextprintf(context, kBLLogLevelVerbose,  "Boot blocks set successfully\n" );
			}
		}

    }

    /* Set Open Firmware to boot off the specified volume*/
    if(actargs[ksetOF].present) {
		err = BLSetOpenFirmwareBootDeviceForMountPoint(context, actargs[kmount].argument);
        if(err) {
            contextprintf(context, kBLLogLevelError,  "Can't set Open Firmware\n" );
            return 1;
        } else {
			contextprintf(context, kBLLogLevelVerbose,  "Open Firmware set successfully\n" );
		}
    }

    if(actargs[klabel].present && isHFS) {
        CFDataRef labeldata = NULL;

        err = BLGenerateOFLabel(context, actargs[klabel].argument, (void **)&labeldata);
        if(err) {
            contextprintf(context, kBLLogLevelError, "Can't render label '%s'\n",
                    actargs[klabel].argument);
        } else {
            u_int32_t oldwords2[8];
            unsigned char sysfolder[MAXPATHLEN];
    
            err = BLGetVolumeFinderInfo(context, actargs[kmount].argument, oldwords2);
            if(err) {
                contextprintf(context, kBLLogLevelError,  "Don't know where to generate bitmap\n" );
                return 1;
            }
    
            err = BLLookupFileIDOnMount(context, actargs[kmount].argument, oldwords2[0], sysfolder);
            if(err) {
                return 2;
            }
    
            contextprintf(context, kBLLogLevelVerbose,  "Putting label bitmap in %s/%s\n", sysfolder, "Volume Name Icon" );
            if(err) {
                return 2;
            }

            err = BLCreateFile(context, (void *)labeldata,
                                        sysfolder,
                                        "Volume Name Icon",
                                        0, 'tbxj', 'chrp');
            if(err) {
                contextprintf(context, kBLLogLevelError,  "Could not write bitmap label file\n" );
                CFRelease(labeldata);
                return 1;
            } else {
                contextprintf(context, kBLLogLevelVerbose, "OF label written\n");
            }

            CFRelease(labeldata);
            strcat(sysfolder, "/Volume Name Icon");
    
            err = BLSetFinderFlag(context, sysfolder, 0x0400, 1);
            if(err) {
                contextprintf(context, kBLLogLevelError,  "Could not set invisibility for %s\n", sysfolder );
                return 1;
            } else {
                contextprintf(context, kBLLogLevelVerbose,  "Invisibility set for %s\n", sysfolder );
            }
        }
    }

    return 0;
}
