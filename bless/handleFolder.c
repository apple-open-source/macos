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
 *  handleFolder.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Dec 6 2001.
 *  Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: handleFolder.c,v 1.56 2005/02/14 21:55:23 ssen Exp $
 *
 *
 */

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/param.h>

#include "enums.h"
#include "structs.h"

#include "bless.h"
#include "bootconfig.h"

enum {
  kIsInvisible                  = 0x4000, /* Files and folders */
};

extern int blesscontextprintf(BLContextPtr context, int loglevel, char const *fmt, ...);

static int isOFLabel(const char *data, int labelsize);
extern int setboot(BLContextPtr context, char *device, CFDataRef bootxData,
				   CFDataRef labelData);

int modeFolder(BLContextPtr context, struct clarg actargs[klast]) {
	
    int err;
    int isHFS;
	
    uint32_t folderXid = 0;                   // The directory ID specified by folderXpath
    uint32_t folder9id = 0;                   // The directory ID specified by folder9path
    
	CFDataRef bootXdata = NULL;
	CFDataRef labeldata = NULL;
    struct statfs sb;

	
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
		
    } else if(actargs[kfolder].present || actargs[kfolder9].present) {
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
    } else if(actargs[kopenfolder].present) {
		// didn't give a -folder or -mount
		err = BLGetCommonMountPoint(context, actargs[kopenfolder].argument, actargs[kopenfolder].argument, actargs[kmount].argument);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError, "Can't determine mount point of '%s'\n",
							   actargs[kopenfolder].argument);
			return 1;
		} else {
			blesscontextprintf(context, kBLLogLevelVerbose, "Common mount point of '%s' is %s\n",
							   actargs[kopenfolder].argument, actargs[kmount].argument );
		}
    } else {
		blesscontextprintf(context, kBLLogLevelError, "No volume specified\n" );
		return 1;
    }
	
    
    /* If user gave options that require BootX creation, do it now. */
    if(actargs[kbootinfo].present) {
        if(!actargs[kbootinfo].hasArg) {
            snprintf(actargs[kbootinfo].argument, kMaxArgLength-1, "%s/%s", actargs[kmount].argument, kBL_PATH_PPC_BOOTX_BOOTINFO);
        }

		err = BLLoadFile(context, actargs[kbootinfo].argument, 0, &bootXdata);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError,  "Could not load BootX data from %s\n",
							   actargs[kbootinfo].argument);
		}
		
        if(actargs[kfolder].present && bootXdata) {            
			err = BLCreateFile(context, bootXdata, actargs[kfolder].argument, "BootX", 0, 1, kBL_OSTYPE_PPC_TYPE_BOOTX, kBL_OSTYPE_PPC_CREATOR_CHRP);
			if(err) {
				blesscontextprintf(context, kBLLogLevelError,  "Could not create BootX in %s\n", actargs[kfolder].argument );
			} else {
				blesscontextprintf(context, kBLLogLevelVerbose,  "BootX created successfully in %s\n",
								   actargs[kfolder].argument );
			}
        } else {
            blesscontextprintf(context, kBLLogLevelVerbose,  "Could not create BootX, no X folder specified\n" );
        }
    } else {
        blesscontextprintf(context, kBLLogLevelVerbose,  "No BootX creation requested\n" );
    }
	
	
    err = BLIsMountHFS(context, actargs[kmount].argument, &isHFS);
    if(err) {
		blesscontextprintf(context, kBLLogLevelError,  "Could not determine filesystem of %s\n", actargs[kmount].argument );
		return 1;
    }

    if(0 != statfs(actargs[kmount].argument, &sb)) {
        blesscontextprintf(context, kBLLogLevelError,  "Can't statfs %s\n" ,
                           actargs[kmount].argument);
        return 1;	    
    }
    
    
    if(isHFS && (actargs[kfolder].present || actargs[kfolder9].present
				 || actargs[kopenfolder].present)) {
		uint32_t oldwords[8];
		int useX = 1;
		uint32_t openfolder = 0;
		
        
		err = BLGetVolumeFinderInfo(context, actargs[kmount].argument, oldwords);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError,  "Error getting old Finder info words for %s\n", actargs[kmount].argument );
			return 1;
		}
		
		if(!actargs[kfolder].present && !actargs[kfolder9].present) {
			// if no blessed folder, preserve what's there already
			actargs[ksave9].present = 1;
			actargs[ksaveX].present = 1;
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
		
        // reserved1 returns the f_fssubtype attribute. Right now, 0 == HFS+,
        // 1 == HFS+J. Anything else is either HFS plain, or some form
        // of HFSX. These filesystems we don't wany blessed, because we don't
        // want future versions of OF to list them as bootable, but rather
        // prefer the Apple_Boot partition
        
        if((sb.f_reserved1 & ~1) && (getenv("BL_OVERRIDE_BLESS_HFSX") == NULL)) {
            blesscontextprintf(context, kBLLogLevelVerbose,  "%s is not HFS+ or Journaled HFS+. Not setting finderinfo[0]...\n", actargs[kmount].argument );
            oldwords[0] = 0;
        } else {
            if(!folderXid || !useX) {
                /* The 9 folder is what we really want */
                oldwords[0] = folder9id;
            } else {
                /* X */
                oldwords[0] = folderXid;
            }
        }
		
		blesscontextprintf(context, kBLLogLevelVerbose,  "finderinfo[0] = %d\n", oldwords[0] );
		blesscontextprintf(context, kBLLogLevelVerbose,  "finderinfo[2] = %d\n", oldwords[2] );
		blesscontextprintf(context, kBLLogLevelVerbose,  "finderinfo[3] = %d\n", oldwords[3] );
		blesscontextprintf(context, kBLLogLevelVerbose,  "finderinfo[5] = %d\n", oldwords[5] );
		
		
		if(geteuid() != 0 && geteuid() != sb.f_owner) {
		    blesscontextprintf(context, kBLLogLevelError,  "Authorization required\n" );
			return 1;
		}
		
		err = BLSetVolumeFinderInfo(context,  actargs[kmount].argument, oldwords);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError,  "Can't set Finder info fields for volume mounted at %s: %s\n", actargs[kmount].argument , strerror(errno));
			return 2;
		}
		
    }
	
    if(isHFS && actargs[kbootblockfile].present) {
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
    
    
	if(actargs[klabel].present||actargs[klabelfile].present) {
		int isLabel = 0;
		
		if(actargs[klabelfile].present) {
			err = BLLoadFile(context, actargs[klabelfile].argument, 0, &labeldata);
			if(err) {
				blesscontextprintf(context, kBLLogLevelError, "Can't load label '%s'\n",
								   actargs[klabelfile].argument);
				return 2;
			}
		} else {
			err = BLGenerateOFLabel(context, actargs[klabel].argument, &labeldata);
			if(err) {
				blesscontextprintf(context, kBLLogLevelError, "Can't render label '%s'\n",
								   actargs[klabel].argument);
				return 3;
			}
		}
		
		isLabel = isOFLabel(CFDataGetBytePtr(labeldata),
							CFDataGetLength(labeldata));
		blesscontextprintf(context, kBLLogLevelVerbose,  "OF label data is valid: %s\n", isLabel ? "YES" : "NO");
		
		if(actargs[kfolder].present) {
			char sysfolder[MAXPATHLEN];
		
			blesscontextprintf(context, kBLLogLevelVerbose,  "Putting label bitmap in %s/%s\n",
							   actargs[kfolder].argument, "Volume Name Icon" );

			err = BLCreateFile(context, labeldata,
							   actargs[kfolder].argument,
							   "Volume Name Icon",
							   0, 0,
							   isLabel ? kBL_OSTYPE_PPC_TYPE_OFLABEL :
									kBL_OSTYPE_PPC_TYPE_OFLABEL_PLACEHOLDER,
							   kBL_OSTYPE_PPC_CREATOR_CHRP);
			if(err) {
				blesscontextprintf(context, kBLLogLevelError,  "Could not write bitmap label file\n" );
			} else {
				blesscontextprintf(context, kBLLogLevelVerbose, "OF label written\n");
			}
			
			sprintf(sysfolder, "%s/Volume Name Icon", actargs[kfolder].argument);
			
			err = BLSetFinderFlag(context, sysfolder, kIsInvisible, 1);
			if(err) {
				blesscontextprintf(context, kBLLogLevelError,  "Could not set invisibility for %s\n", sysfolder );
				return 1;
			} else {
				blesscontextprintf(context, kBLLogLevelVerbose,  "Invisibility set for %s\n", sysfolder );
			}
		}
    }
	
    /* Set Open Firmware to boot off the specified volume*/
    if(actargs[ksetboot].present) {
		struct statfs sb;
		
		err = statfs(actargs[kmount].argument, &sb);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError,  "Can't statfs: %s\n",
							   strerror(errno));			
			return 2;
		}
		
		err = setboot(context, sb.f_mntfromname, bootXdata, labeldata);
		if(err) {
			return 3;
		}
		
		if(!BLIsOpenFirmwarePresent(context)) {
			// for fdisk partition maps, write the OF path for the root device
			unsigned char parDev[MNAMELEN];
			unsigned long slice;
			BLPartitionType ptype;
			CFDataRef bootconfigdata = NULL;
			
			
			err = BLGetParentDeviceAndPartitionType(context, sb.f_mntfromname, parDev, &slice, &ptype);
			if(err) {
				return 3;
			}
			
			blesscontextprintf(context, kBLLogLevelVerbose, "Mount point '%s' is part of an %s partition map\n",
							   actargs[kmount].argument,
							   ptype == kBLPartitionType_APM ? "Apple" : (ptype == kBLPartitionType_MBR ? "MBR" : "Unknown"));
			
			if(ptype == kBLPartitionType_MBR) {
				CFMutableDictionaryRef plist = NULL;
				char acpistring[1024];
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
				
				err = BLGetACPIBootDevice(context, sb.f_mntfromname, acpistring);
				if(err) {
					blesscontextprintf(context, kBLLogLevelError, "Could not map %s to an ACPI path\n", sb.f_mntfromname);
					CFRelease(plist);
					return 6;
				}
				
				ofref = CFStringCreateWithCString(kCFAllocatorDefault, acpistring, kCFStringEncodingUTF8);
				if(!ofref) {
					CFRelease(plist);
					return 7;
				}
				
				CFDictionarySetValue(plist, CFSTR("Boot Device"), ofref);
				CFRelease(ofref);
				
				bootconfigdata = CFPropertyListCreateXMLData(kCFAllocatorDefault, plist);
				CFRelease(plist);
				
				if(!bootconfigdata) {
					blesscontextprintf(context, kBLLogLevelError, "Could not convert dictionary to XML\n");
					return 8;
				}
				
				err = BLCreateFile(context, bootconfigdata, actargs[kmount].argument, kBL_PATH_I386_BOOT2_CONFIG_PLIST,
								   0, 0, 0, 0);
				if(err) {
					blesscontextprintf(context, kBLLogLevelError, "Could not write new %s\n", configpath);
					CFRelease(bootconfigdata);
					return 8;
				}
				
				CFRelease(bootconfigdata);
			}
			
		}
		
    }
	
	if(bootXdata) CFRelease(bootXdata);
	if(labeldata) CFRelease(labeldata);

	
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
