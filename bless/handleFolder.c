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
 *  handleFolder.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Dec 6 2001.
 *  Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: handleFolder.c,v 1.40 2003/08/04 05:24:16 ssen Exp $
 *
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "enums.h"
#include "structs.h"

#include "bless.h"
#include "bootconfig.h"

extern int blesscontextprintf(BLContextPtr context, int loglevel, char const *fmt, ...);

static int isOFLabel(const char *data, int labelsize);

int modeFolder(BLContextPtr context, struct clopt commandlineopts[klast], struct clarg actargs[klast]) {

    int err;
    int isHFS;

    uint32_t folderXid = 0;                   // The directory ID specified by folderXpath
    uint32_t folder9id = 0;                   // The directory ID specified by folder9path
    
    if(!(geteuid() == 0)) {
	blesscontextprintf(context, kBLLogLevelError,  "Not run as root\n" );
	return 1;
    }

    if(actargs[kmount].present) {
	err = BLGetCommonMountPoint(context, actargs[kmount].argument, "", actargs[kmount].argument);
	if(err) {
	    blesscontextprintf(context, kBLLogLevelError,  "Can't determine mount point of '%s'\n", actargs[kmount].argument );
	} else {
	    blesscontextprintf(context, kBLLogLevelVerbose,  "Mount point is '%s'\n", actargs[kmount].argument );
	}

	// if -mount was specified, it implies we want to preserve what exists
	actargs[ksave9].present = 1;
	actargs[ksaveX].present = 1;

    } else {
	/* We know that at least one folder has been specified */
	err = BLGetCommonMountPoint(context, actargs[kfolder].argument, actargs[kfolder9].argument, actargs[kmount].argument);
	if(err) {
	    blesscontextprintf(context, kBLLogLevelError, "Can't determine mount point of '%s' and '%s'\n",
			actargs[kfolder].argument, actargs[kfolder9].argument);
	    return 1;
	} else {
	    blesscontextprintf(context, kBLLogLevelVerbose, "Common mount point of '%s' and '%s' is %s\n",
			actargs[kfolder].argument,
			actargs[kfolder9].argument, actargs[kmount].argument );
	}
    }

    
    /* If user gave options that require BootX creation, do it now. */
    if(actargs[kbootinfo].present) {
        if(actargs[kfolder].present) {
	    CFDataRef bootXdata = NULL;

	    err = BLLoadFile(context, actargs[kbootinfo].argument, 0, &bootXdata);
	    if(err) {
		blesscontextprintf(context, kBLLogLevelError,  "Could not load BootX data from %s\n",
		     actargs[kbootinfo].argument);
	    } else {

		err = BLCreateFile(context, bootXdata, actargs[kfolder].argument, "BootX", 0, kBL_OSTYPE_PPC_TYPE_BOOTX, kBL_OSTYPE_PPC_CREATOR_CHRP);
		if(err) {
		    blesscontextprintf(context, kBLLogLevelError,  "Could not create BootX in %s\n", actargs[kfolder].argument );
		} else {
		    blesscontextprintf(context, kBLLogLevelVerbose,  "BootX created successfully in %s\n",
			 actargs[kfolder].argument );
		}
	    }

	} else {
	    blesscontextprintf(context, kBLLogLevelVerbose,  "Could not create BootX, no X folder specified\n" );
	}
    } else {
        blesscontextprintf(context, kBLLogLevelVerbose,  "No BootX creation requested\n" );
    }

    // copy system file over
    if(actargs[ksystemfile].present) {
        if(actargs[kfolder9].present || actargs[kfolder].present) {
            CFDataRef systemdata = NULL;
	    

            err = BLLoadFile(context, actargs[ksystemfile].argument, 0, &systemdata);
            if(err) {
		blesscontextprintf(context, kBLLogLevelError,  "Could not load system file data from %s\n",
		     actargs[ksystemfile].argument);
            } else {
                err = BLCreateFile(context, (void *)systemdata,
				   actargs[kfolder9].present ? actargs[kfolder9].argument : actargs[kfolder].argument,
				   "System", 1, 'zsys', 'MACS');
                if(err) {
		    blesscontextprintf(context, kBLLogLevelError,  "Could not create System in %s\n",
			 actargs[kfolder9].argument );
                } else {
		    blesscontextprintf(context, kBLLogLevelVerbose,  "System created successfully in %s\n",
			 actargs[kfolder9].argument );
                }
            }
        } else {
	    blesscontextprintf(context, kBLLogLevelVerbose,  "Could not create System file, no folder specified\n" );
        }
    }


    err = BLIsMountHFS(context, actargs[kmount].argument, &isHFS);
    if(err) {
	blesscontextprintf(context, kBLLogLevelError,  "Could not determine filesystem of %s\n", actargs[kmount].argument );
	return 1;
    }

    if(isHFS && (actargs[kfolder].present || actargs[kfolder9].present)) {
	uint32_t oldwords[8];
	int useX = 1;
	uint32_t openfolder = 0;

	err = BLGetVolumeFinderInfo(context, actargs[kmount].argument, oldwords);
	if(err) {
	    blesscontextprintf(context, kBLLogLevelError,  "Error getting old Finder info words for %s\n", actargs[kmount].argument );
	    return 1;
	}

	if(actargs[ksave9].present) {
	    folder9id = oldwords[3];
	    blesscontextprintf(context, kBLLogLevelVerbose,  "Saved folder 9\n" );
	}
	if(actargs[ksaveX].present) {
	    folderXid = oldwords[5];
	    blesscontextprintf(context, kBLLogLevelVerbose,  "Saved folder X\n" );
	}

	/* bless! bless */

	/* First get any directory IDs we need */
	if(actargs[kfolder].present) {
	    err = BLGetFileID(context, actargs[kfolder].argument, &folderXid);
	    if(err) {
		blesscontextprintf(context, kBLLogLevelError,  "Error while get directory ID of %s\n", actargs[kfolder].argument );
	    } else {
		blesscontextprintf(context, kBLLogLevelVerbose,  "Got directory ID of %u for %s\n",
		     folderXid, actargs[kfolder].argument );
	    }
	}

	if(actargs[kfolder9].present) {
	    err = BLGetFileID(context, actargs[kfolder9].argument, &folder9id);
	    if(err) {
		blesscontextprintf(context, kBLLogLevelError,  "Error while get directory ID of %s\n", actargs[kfolder9].argument );
	    } else {
		blesscontextprintf(context, kBLLogLevelVerbose,  "Got directory ID of %u for %s\n",
		     folder9id, actargs[kfolder9].argument );
	    }
	}

	if(actargs[kopenfolder].present) {
	    unsigned char openmount[kMaxArgLength];

	    openmount[0] = '\0';

	    err = BLGetCommonMountPoint(context, actargs[kopenfolder].argument,
		actargs[kmount].argument, openmount);

	    if(err || strcmp(actargs[kmount].argument, openmount)) {
		// if there's an error with the openfolder, or it's
		// not on the target volume, abort
		blesscontextprintf(context, kBLLogLevelError,  "Error determining mountpoint of %s\n", actargs[kopenfolder].argument );
	    }
	    
	    err = BLGetFileID(context, actargs[kopenfolder].argument, &openfolder);
	    if(err) {
		blesscontextprintf(context, kBLLogLevelError,  "Error while get directory ID of %s\n", actargs[kopenfolder].argument );
	    } else {
		blesscontextprintf(context, kBLLogLevelVerbose,  "Got directory ID of %u for %s\n",
		     openfolder, actargs[kopenfolder].argument );
	    }
	}
	
	if(actargs[kuse9].present) {
	    useX = 0;
	}

	/* If either directory was not specified, the dirID
	    * variables will be 0, so we can use that to initialize
	    * the FI fields */

	/* Set Finder info words 3 & 5 + 2*/
	oldwords[2] = openfolder;
	oldwords[3] = folder9id;
	oldwords[5] = folderXid;

	if(!folderXid || !useX) {
	    /* The 9 folder is what we really want */
	    oldwords[0] = folder9id;
	} else {
	    /* X */
	    oldwords[0] = folderXid;
	}

	blesscontextprintf(context, kBLLogLevelVerbose,  "finderinfo[0] = %d\n", oldwords[0] );
	blesscontextprintf(context, kBLLogLevelVerbose,  "finderinfo[2] = %d\n", oldwords[2] );
	blesscontextprintf(context, kBLLogLevelVerbose,  "finderinfo[3] = %d\n", oldwords[3] );
	blesscontextprintf(context, kBLLogLevelVerbose,  "finderinfo[5] = %d\n", oldwords[5] );

	err = BLSetVolumeFinderInfo(context,  actargs[kmount].argument, oldwords);
	if(err) {
	    blesscontextprintf(context, kBLLogLevelError,  "Can't set Finder info fields for volume mounted at %s\n", actargs[kmount].argument );
	    return 2;
	}
	
	
	/* Bless the folders */
	/*
	err = BLBlessDir(context, actargs[kmount].argument, folderXid, folder9id, useX);
	if(err) {
	    blesscontextprintf(context, kBLLogLevelError,  "Can't bless directories\n" );
	    return 1;
	} else {
	    blesscontextprintf(context, kBLLogLevelVerbose,  "Volume at %s blessed successfully\n", actargs[kmount].argument );
	}
	 */
	 
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

    }

    /* Set Open Firmware to boot off the specified volume*/
    if(actargs[ksetboot].present) {
	if(BLIsOpenFirmwarePresent(context)) {
	    err = BLSetOpenFirmwareBootDeviceForMountPoint(context, actargs[kmount].argument);
	    if(err) {
		blesscontextprintf(context, kBLLogLevelError,  "Can't set Open Firmware\n" );
		return 1;
	    } else {
		blesscontextprintf(context, kBLLogLevelVerbose,  "Open Firmware set successfully\n" );
	    }
	} else {
	    // for fdisk partition maps, write the OF path for the root device
	    struct statfs sb;
	    unsigned char parDev[MNAMELEN];
	    unsigned long slice;
	    BLPartitionType ptype;
	    CFDataRef bootconfigdata = NULL;
	    
	    err = statfs(actargs[kmount].argument, &sb);
	    if(err) {
		return 2;
	    }
	    
	    err = BLGetParentDeviceAndPartitionType(context, sb.f_mntfromname, parDev, &slice, &ptype);
	    if(err) {
		return 3;
	    }
	    
	    blesscontextprintf(context, kBLLogLevelVerbose, "Mount point '%s' is part of an %s partition map\n",
			actargs[kmount].argument,
			ptype == kBLPartitionType_APM ? "Apple" : (ptype == kBLPartitionType_MBR ? "MBR" : "Unknown"));
	    
	    if(ptype == kBLPartitionType_MBR) {
		CFMutableDictionaryRef plist = NULL;
		char ofstring[1024];
		CFStringRef ofref;
		unsigned char configpath[MAXPATHLEN+1];
		
		snprintf(configpath, MAXPATHLEN, "%s/%s", actargs[kmount].argument, kBL_PATH_I386_BOOT2_CONFIG_PLIST);
		
		err = BLLoadFile(context, configpath, 0, &bootconfigdata);
		if(err) {
		    blesscontextprintf(context, kBLLogLevelVerbose, "Could not load boot2 config plist from %s\n", configpath);
		    blesscontextprintf(context, kBLLogLevelVerbose, "Using built-in default\n");
		    bootconfigdata = CFDataCreate(kCFAllocatorDefault, bootconfigplist, sizeof(bootconfigplist));
		}
		
		plist = (CFMutableDictionaryRef)CFPropertyListCreateFromXMLData(kCFAllocatorDefault, bootconfigdata, kCFPropertyListMutableContainers, NULL);
		CFRelease(bootconfigdata);
		if(plist == NULL || CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
		    blesscontextprintf(context, kBLLogLevelError, "Could not parse boot config plist as dictionary\n");
		    if(plist) CFRelease(plist);
		    return 5;
		}
		
		err = BLGetOpenFirmwareBootDevice(context, sb.f_mntfromname, ofstring);
		if(err) {
		    blesscontextprintf(context, kBLLogLevelError, "Could not map %s to an OpenFirmware path\n", sb.f_mntfromname);
		    CFRelease(plist);
		    return 6;
		}
		
		ofref = CFStringCreateWithCString(kCFAllocatorDefault, ofstring, kCFStringEncodingUTF8);
		if(!ofref) {
		    CFRelease(plist);
		    return 7;
		}
		
		CFDictionaryAddValue(plist, CFSTR("Boot Device"), ofref);
		CFRelease(ofref);
		
		bootconfigdata = CFPropertyListCreateXMLData(kCFAllocatorDefault, plist);
		CFRelease(plist);
		
		if(!bootconfigdata) {
		    blesscontextprintf(context, kBLLogLevelError, "Could not convert dictionary to XML\n");
		    return 8;
		}
		
		err = BLCreateFile(context, bootconfigdata, actargs[kmount].argument, kBL_PATH_I386_BOOT2_CONFIG_PLIST,
		     0, 0, 0);
		if(err) {
		    blesscontextprintf(context, kBLLogLevelError, "Could not write new %s\n", configpath);
		    CFRelease(bootconfigdata);
		    return 8;
		}
		
		CFRelease(bootconfigdata);
	    }
	    
	}
	
    }

    if((actargs[klabel].present||actargs[klabelfile].present) && isHFS) {
        CFDataRef labeldata = NULL;
	uint32_t oldwords2[8];
	unsigned char sysfolder[MAXPATHLEN];
	int isLabel = 0;

	if(actargs[klabelfile].present) {
	    err = BLLoadFile(context, actargs[klabelfile].argument, 0, &labeldata);
	    if(err) {
		blesscontextprintf(context, kBLLogLevelError, "Can't load label '%s'\n",
		     actargs[klabelfile].argument);
		return 2;
	    }
	}

	isLabel = isOFLabel(CFDataGetBytePtr(labeldata),
		     CFDataGetLength(labeldata));
	blesscontextprintf(context, kBLLogLevelVerbose,  "OF label data is valid: %s\n", isLabel ? "YES" : "NO");

	
	err = BLGetVolumeFinderInfo(context, actargs[kmount].argument, oldwords2);
	if(err) {
	    blesscontextprintf(context, kBLLogLevelError,  "Don't know where to generate bitmap\n" );
	    return 1;
	}

	err = BLLookupFileIDOnMount(context, actargs[kmount].argument, oldwords2[0], sysfolder);
	if(err) {
	    return 2;
	}

	blesscontextprintf(context, kBLLogLevelVerbose,  "Putting label bitmap in %s/%s\n", sysfolder, "Volume Name Icon" );
	if(err) {
	    return 2;
	}

	err = BLCreateFile(context, (void *)labeldata,
		    sysfolder,
		    "Volume Name Icon",
		    0,
		    isLabel ? kBL_OSTYPE_PPC_TYPE_OFLABEL : 'xxxx',
		    kBL_OSTYPE_PPC_CREATOR_CHRP);
	if(err) {
	    blesscontextprintf(context, kBLLogLevelError,  "Could not write bitmap label file\n" );
	    CFRelease(labeldata);
	    return 1;
	} else {
	    blesscontextprintf(context, kBLLogLevelVerbose, "OF label written\n");
	}

	CFRelease(labeldata);
	strcat(sysfolder, "/Volume Name Icon");

	err = BLSetFinderFlag(context, sysfolder, kIsInvisible, 1);
	if(err) {
	    blesscontextprintf(context, kBLLogLevelError,  "Could not set invisibility for %s\n", sysfolder );
	    return 1;
	} else {
	    blesscontextprintf(context, kBLLogLevelVerbose,  "Invisibility set for %s\n", sysfolder );
	}

    }

    if(actargs[kxcoff].present && isHFS) {
        if(actargs[kfolder].present) {
            CFDataRef data = NULL;            
            uint32_t entrypoint, loadbase, size, checksum;
            off_t extents[8][2];
            unsigned char parent[MAXPATHLEN];
            unsigned long pNum;
            unsigned char sysfolder[MAXPATHLEN];
            unsigned char command[2048];

            unsigned char device[MNAMELEN];
    
            strncpy(sysfolder, actargs[kfolder].argument, MAXPATHLEN-1);
            sysfolder[MAXPATHLEN-1] = '\0';

            err = BLLoadXCOFFLoader(context,
				    actargs[kxcoff].argument,
				    &entrypoint,
				    &loadbase,
				    &size,
				    &checksum,
				    &data);

            if(err) {
                blesscontextprintf(context, kBLLogLevelError,  "Could not load XCOFF loader from %s\n", actargs[kxcoff].argument );
                return 1;
            }

            blesscontextprintf(context, kBLLogLevelVerbose,  "Creating loader BootX.image in %s\n", sysfolder );

            err = BLCreateFile(context, data,
			       sysfolder,
			       "BootX.image",
			       0, 0, kBL_OSTYPE_PPC_CREATOR_CHRP);

	    CFRelease(data);
	    
            if(err) {
                blesscontextprintf(context, kBLLogLevelError,  "Error %d while creating BootX.image\n", err);
                return 1;
            }
            
            strcat(sysfolder, "/BootX.image");
            
            err = BLGetDiskSectorsForFile(context,
					  sysfolder,
					  extents,
					  device);
            if(err) {
                blesscontextprintf(context, kBLLogLevelError,  "Error while finding BootX.image on disk\n", err);
                return 1;
            }
                        
                                      
            err = BLGetParentDevice(context, device, parent, &pNum);
            if(err) {
		parent[0] = '\0';
		pNum = 0;
            }

            sprintf(command, "/usr/sbin/pdisk %s -makeBootable %lu %qd %u %u %u",
		    parent, pNum, extents[0][0], size, loadbase, entrypoint);

            blesscontextprintf(context, kBLLogLevelVerbose, "pdisk command: %s\n", command);

            blesscontextprintf(context, kBLLogLevelNormal, "%s\n", command);
        } else {
	    blesscontextprintf(context, kBLLogLevelVerbose,  "Could not create BootX.image, no X folder specified\n" );            
        }
    }

    return 0;
}

static int isOFLabel(const char *data, int labelsize)
{
    uint16_t width, height;
    
    if(data[0] != 1) return 0;

    width = CFSwapInt16BigToHost(*(uint16_t *)&data[1]);
    height = CFSwapInt16BigToHost(*(uint16_t *)&data[3]);

    if(labelsize != (width*height+5)) return 0;

    // basic sanity checks for version and dimensions were satisfied
    return 1;
}
