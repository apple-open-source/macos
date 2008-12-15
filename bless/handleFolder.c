/*
 * Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
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
 *  Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: handleFolder.c,v 1.79 2006/07/21 14:59:24 ssen Exp $
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
#include "bless_private.h"
#include "protos.h"

enum {
  kIsInvisible                  = 0x4000, /* Files and folders */
};


static int isOFLabel(const char *data, int labelsize);

int modeFolder(BLContextPtr context, struct clarg actargs[klast]) {
	
    int ret;
    int isHFS, shouldBless;
	
    uint32_t folderXid = 0;                   // The directory ID specified by folderXpath
    uint32_t folder9id = 0;                   // The directory ID specified by folder9path
    
	CFDataRef bootXdata = NULL;
	CFDataRef bootEFIdata = NULL;
	CFDataRef labeldata = NULL;
    struct statfs sb;

    BLPreBootEnvType	preboot;
	
	ret = BLGetPreBootEnvironmentType(context, &preboot);
	if(ret) {
		blesscontextprintf(context, kBLLogLevelError,  "Could not determine preboot environment\n");
		return 1;
	}
	
    
    if(actargs[kmount].present) {
		ret = BLGetCommonMountPoint(context, actargs[kmount].argument, "", actargs[kmount].argument);
		if(ret) {
			blesscontextprintf(context, kBLLogLevelError,  "Can't determine mount point of '%s'\n", actargs[kmount].argument );
		} else {
			blesscontextprintf(context, kBLLogLevelVerbose,  "Mount point is '%s'\n", actargs[kmount].argument );
		}
		
		// if -mount was specified, it implies we want to preserve what exists
		actargs[ksave9].present = 1;
		actargs[ksaveX].present = 1;
		
    } else if(actargs[kfolder].present || actargs[kfolder9].present) {
		/* We know that at least one folder has been specified */
		ret = BLGetCommonMountPoint(context, actargs[kfolder].argument, actargs[kfolder9].argument, actargs[kmount].argument);
		if(ret) {
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
		ret = BLGetCommonMountPoint(context, actargs[kopenfolder].argument, actargs[kopenfolder].argument, actargs[kmount].argument);
		if(ret) {
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
	
	/*
	 * actargs[kmount].argument will always be filled in as the volume we are
	 * operating. Look at actargs[kfolder].present to see if the user wanted
	 * to bless something specifically, or just wanted to use -setBoot
	 * or something
	 */
	if( actargs[kfolder].present || actargs[kfolder9].present
		|| actargs[kopenfolder].present) {
		shouldBless = 1;
	} else {
		shouldBless = 0;
	}
	
	if(shouldBless) {
		// if we're blessing the volume, we need something for
		// finderinfo[1]. If you didn't provide a file, but we're
		// planning on generating one, fill in the path now
		
		if(!actargs[kfile].present && actargs[kbootefi].present) {
            // you didn't give a booter file explicitly, so we have to guess
            // based on the system folder.
            snprintf(actargs[kfile].argument, sizeof(actargs[kfile].argument),
                     "%s/boot.efi", actargs[kfolder].argument);
            actargs[kfile].present = 1;
        }        
    }
    
    /* If user gave options that require BootX creation, do it now. */
    if(actargs[kbootinfo].present) {
        char bootxpath[MAXPATHLEN];
        
        if(!actargs[kbootinfo].hasArg) {
            snprintf(actargs[kbootinfo].argument, kMaxArgLength-1, "%s/%s", actargs[kmount].argument, kBL_PATH_PPC_BOOTX_BOOTINFO);
        }

		ret = BLLoadFile(context, actargs[kbootinfo].argument, 0, &bootXdata);
		if(ret) {
			blesscontextprintf(context, kBLLogLevelVerbose,  "Could not load BootX data from %s\n",
							   actargs[kbootinfo].argument);
		}
		
        if(actargs[kfolder].present && bootXdata) {            
            // check to see if needed
            CFDataRef oldBootXdata = NULL;
            struct stat oldPresence;

            snprintf(bootxpath, sizeof(bootxpath), "%s/BootX", actargs[kfolder].argument);            
            
            ret = lstat(bootxpath, &oldPresence);
            
            if(ret == 0 && S_ISREG(oldPresence.st_mode)) {
                ret = BLLoadFile(context, bootxpath, 0, &oldBootXdata);            
            }
            if((ret == 0) && oldBootXdata && CFEqual(oldBootXdata, bootXdata)) {
				blesscontextprintf(context, kBLLogLevelVerbose,  "BootX unchanged at %s. Skipping update...\n",
                    bootxpath );
            } else {
                ret = BLCreateFile(context, bootXdata, bootxpath, 1, kBL_OSTYPE_PPC_TYPE_BOOTX, kBL_OSTYPE_PPC_CREATOR_CHRP);
                if(ret) {
                    blesscontextprintf(context, kBLLogLevelError,  "Could not create BootX at %s\n", bootxpath );
                    return 2;
                } else {
                    blesscontextprintf(context, kBLLogLevelVerbose,  "BootX created successfully at %s\n",
                    bootxpath );
                }                
            }
        } else {
            blesscontextprintf(context, kBLLogLevelVerbose,  "Could not create BootX, no X folder specified\n" );
        }
    } else {
        blesscontextprintf(context, kBLLogLevelVerbose,  "No BootX creation requested\n" );
    }

	/* If user gave options that require boot.efi creation, do it now. */
    if(actargs[kbootefi].present) {
        if(!actargs[kbootefi].hasArg) {
			
            snprintf(actargs[kbootefi].argument, kMaxArgLength-1, "%s/%s", actargs[kmount].argument, kBL_PATH_I386_BOOT_EFI);
			
        }
		
		ret = BLLoadFile(context, actargs[kbootefi].argument, 0, &bootEFIdata);
		if(ret) {
			blesscontextprintf(context, kBLLogLevelVerbose,  "Could not load boot.efi data from %s\n",
							   actargs[kbootefi].argument);
		}
		
        if(actargs[kfile].present && bootEFIdata) {
            
            // check to see if needed
            CFDataRef oldEFIdata = NULL;
            struct stat oldPresence;
            
            ret = lstat(actargs[kfile].argument, &oldPresence);
            
            if(ret == 0 && S_ISREG(oldPresence.st_mode)) {
                ret = BLLoadFile(context, actargs[kfile].argument, 0, &oldEFIdata);                
            }
            if((ret == 0) && oldEFIdata && CFEqual(oldEFIdata, bootEFIdata)) {
				blesscontextprintf(context, kBLLogLevelVerbose,  "boot.efi unchanged at %s. Skipping update...\n",
                actargs[kfile].argument );
            } else {
                ret = BLCreateFile(context, bootEFIdata, actargs[kfile].argument, 1, 0, 0);
                if(ret) {
                    blesscontextprintf(context, kBLLogLevelError,  "Could not create boot.efi at %s\n", actargs[kfile].argument );
                    return 2;
                } else {
                    blesscontextprintf(context, kBLLogLevelVerbose,  "boot.efi created successfully at %s\n",
                        actargs[kfile].argument );
                }                
            }
            
            if(oldEFIdata) CFRelease(oldEFIdata);
            
        } else {
            blesscontextprintf(context, kBLLogLevelVerbose,  "Could not create boot.efi, no X folder specified\n" );
        }
    } else {
        blesscontextprintf(context, kBLLogLevelVerbose,  "No boot.efi creation requested\n" );
    }
	
	
    ret = BLIsMountHFS(context, actargs[kmount].argument, &isHFS);
    if(ret) {
		blesscontextprintf(context, kBLLogLevelError,  "Could not determine filesystem of %s\n", actargs[kmount].argument );
		return 1;
    }

    if(0 != statfs(actargs[kmount].argument, &sb)) {
        blesscontextprintf(context, kBLLogLevelError,  "Can't statfs %s\n" ,
                           actargs[kmount].argument);
        return 1;	    
    }
    
    
    if(isHFS && shouldBless) {
		uint32_t oldwords[8];
		int useX = 1;
		uint32_t openfolder = 0;
		uint32_t bootfile = 0;
        
		ret = BLGetVolumeFinderInfo(context, actargs[kmount].argument, oldwords);
		if(ret) {
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
		
		/* always save boot file */
		bootfile = oldwords[1];
		
		/* bless! bless */
		
		/* First get any directory IDs we need */
		if(actargs[kfolder].present) {
			ret = BLGetFileID(context, actargs[kfolder].argument, &folderXid);
			if(ret) {
				blesscontextprintf(context, kBLLogLevelError,  "Error while getting directory ID of %s\n", actargs[kfolder].argument );
			} else {
				blesscontextprintf(context, kBLLogLevelVerbose,  "Got directory ID of %u for %s\n",
								   folderXid, actargs[kfolder].argument );
			}
		}
		
		if(actargs[kfolder9].present) {
			ret = BLGetFileID(context, actargs[kfolder9].argument, &folder9id);
			if(ret) {
				blesscontextprintf(context, kBLLogLevelError,  "Error while getting directory ID of %s\n", actargs[kfolder9].argument );
			} else {
				blesscontextprintf(context, kBLLogLevelVerbose,  "Got directory ID of %u for %s\n",
								   folder9id, actargs[kfolder9].argument );
			}
		}

		if(actargs[kfile].present) {
			ret = BLGetFileID(context, actargs[kfile].argument, &bootfile);
			if(ret) {
				blesscontextprintf(context, kBLLogLevelError,  "Error while getting file ID of %s. Ignoring...\n", actargs[kfile].argument );
				bootfile = 0;
			} else {
				struct stat checkForFile;
				
				ret = lstat(actargs[kfile].argument, &checkForFile);
				if(ret || !S_ISREG(checkForFile.st_mode)) {
					blesscontextprintf(context, kBLLogLevelError,  "%s cannot be accessed, or is not a regular file. Ignoring...\n", actargs[kfile].argument );				
					bootfile = 0;
				} else {
					blesscontextprintf(context, kBLLogLevelVerbose,  "Got file ID of %u for %s\n",
									   bootfile, actargs[kfile].argument );
				}
			}			
			
		} else {
            // no file given. we should try to verify the existing booter
            if(bootfile) {
                ret = BLLookupFileIDOnMount(context, actargs[kmount].argument, bootfile, actargs[kfile].argument);
                if(ret) {
                    blesscontextprintf(context, kBLLogLevelVerbose,  "Invalid EFI blessed file ID %u. Zeroing...\n",
                                       bootfile );                    
                    bootfile = 0;
                } else {
					struct stat checkForFile;

					ret = lstat(actargs[kfile].argument, &checkForFile);
					if(ret || !S_ISREG(checkForFile.st_mode)) {
						blesscontextprintf(context, kBLLogLevelError,  "%s cannot be accessed, or is not a regular file. Ignoring...\n", actargs[kfile].argument );				
						bootfile = 0;
					} else {						
						blesscontextprintf(context, kBLLogLevelVerbose,
										   "Preserving EFI blessed file ID %u for %s\n",
										   bootfile, actargs[kfile].argument );                    
					}
                }
            }
            
        }
		
		if(actargs[kopenfolder].present) {
			char openmount[kMaxArgLength];
			
			openmount[0] = '\0';
			
			ret = BLGetCommonMountPoint(context, actargs[kopenfolder].argument,
										actargs[kmount].argument, openmount);
			
			if(ret || strcmp(actargs[kmount].argument, openmount)) {
				// if there's an error with the openfolder, or it's
				// not on the target volume, abort
				blesscontextprintf(context, kBLLogLevelError,  "Error determining mountpoint of %s\n", actargs[kopenfolder].argument );
			}
			
			ret = BLGetFileID(context, actargs[kopenfolder].argument, &openfolder);
			if(ret) {
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
		
		/* Set Finder info words 3 & 5 + 2 + 1*/
		oldwords[1] = bootfile;
		oldwords[2] = openfolder;
		oldwords[3] = folder9id;
		oldwords[5] = folderXid;
		
        // reserved1 returns the f_fssubtype attribute. Right now, 0 == HFS+,
        // 1 == HFS+J. Anything else is either HFS plain, or some form
        // of HFSX. These filesystems we don't want blessed, because we don't
        // want future versions of OF to list them as bootable, but rather
        // prefer the Apple_Boot partition
        //
        // For EFI-based systems, it's OK to set finderinfo[0], and indeed
        // a better user experience so that the EFI label shows up
        
        if(actargs[ksetboot].present &&
           (preboot == kBLPreBootEnvType_OpenFirmware) &&
#if _DARWIN_FEATURE_64_BIT_INODE
           (sb.f_fssubtype & ~1)
#else
           (sb.f_reserved1 & ~1)
#endif
           ) {
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
		blesscontextprintf(context, kBLLogLevelVerbose,  "finderinfo[1] = %d\n", oldwords[1] );
		blesscontextprintf(context, kBLLogLevelVerbose,  "finderinfo[2] = %d\n", oldwords[2] );
		blesscontextprintf(context, kBLLogLevelVerbose,  "finderinfo[3] = %d\n", oldwords[3] );
		blesscontextprintf(context, kBLLogLevelVerbose,  "finderinfo[5] = %d\n", oldwords[5] );
		
		
		if(geteuid() != 0 && geteuid() != sb.f_owner) {
		    blesscontextprintf(context, kBLLogLevelError,  "Authorization required\n" );
			return 1;
		}
		
		ret = BLSetVolumeFinderInfo(context,  actargs[kmount].argument, oldwords);
		if(ret) {
			blesscontextprintf(context, kBLLogLevelError,  "Can't set Finder info fields for volume mounted at %s: %s\n", actargs[kmount].argument , strerror(errno));
			return 2;
		}
		
    }
	
    if(isHFS && actargs[kbootblockfile].present) {
        CFDataRef bbdata = NULL;
        
        ret = BLLoadFile(context, actargs[kbootblockfile].argument, 0, &bbdata);
        if(ret) {
            blesscontextprintf(context, kBLLogLevelError, "Can't get boot blocks from data-fork file %s\n",
                               actargs[kbootblockfile].argument);
            return 1;
        } else {
            blesscontextprintf(context, kBLLogLevelVerbose,  "Boot blocks read from %s\n", actargs[kbootblockfile].argument );
        }
        
        ret = BLSetBootBlocks(context, actargs[kmount].argument, bbdata);
        CFRelease(bbdata);
        
        if(ret) {
            blesscontextprintf(context, kBLLogLevelError,  "Can't set boot blocks for %s\n", actargs[kmount].argument );
            return 1;
        } else {
            blesscontextprintf(context, kBLLogLevelVerbose,  "Boot blocks set successfully\n" );
        }
    }
    
    
	if(actargs[klabel].present||actargs[klabelfile].present) {
		int isLabel = 0;
		
		if(actargs[klabelfile].present) {
			ret = BLLoadFile(context, actargs[klabelfile].argument, 0, &labeldata);
			if(ret) {
				blesscontextprintf(context, kBLLogLevelError, "Can't load label '%s'\n",
								   actargs[klabelfile].argument);
				return 2;
			}
		} else {
			ret = BLGenerateOFLabel(context, actargs[klabel].argument, &labeldata);
			if(ret) {
				blesscontextprintf(context, kBLLogLevelError, "Can't render label '%s'\n",
								   actargs[klabel].argument);
				return 3;
			}
		}
		
		isLabel = isOFLabel((const char *)CFDataGetBytePtr(labeldata),
							CFDataGetLength(labeldata));
		blesscontextprintf(context, kBLLogLevelVerbose,  "OF label data is valid: %s\n", isLabel ? "YES" : "NO");
		
		if(actargs[kfolder].present) {
			char sysfolder[MAXPATHLEN];
		
			sprintf(sysfolder, "%s/.disk_label", actargs[kfolder].argument);
			
			blesscontextprintf(context, kBLLogLevelVerbose,  "Putting label bitmap in %s\n",
							   sysfolder );

			ret = BLCreateFile(context, labeldata,
							   sysfolder,
							   0,
							   isLabel ? kBL_OSTYPE_PPC_TYPE_OFLABEL :
									kBL_OSTYPE_PPC_TYPE_OFLABEL_PLACEHOLDER,
							   kBL_OSTYPE_PPC_CREATOR_CHRP);
			if(ret) {
				blesscontextprintf(context, kBLLogLevelError,  "Could not write bitmap label file\n" );
			} else {
				blesscontextprintf(context, kBLLogLevelVerbose, "OF label written\n");
			}
			
			ret = BLSetFinderFlag(context, sysfolder, kIsInvisible, 1);
			if(ret) {
				blesscontextprintf(context, kBLLogLevelError,  "Could not set invisibility for %s\n", sysfolder );
				return 1;
			} else {
				blesscontextprintf(context, kBLLogLevelVerbose,  "Invisibility set for %s\n", sysfolder );
			}
		}
    }
	
    /* Set Open Firmware to boot off the specified volume*/
    if(actargs[ksetboot].present) {
        if(preboot == kBLPreBootEnvType_EFI) {
			// if you blessed the volume, then just point EFI at the volume.
			// only if you didn't bless, but you have something interesting
			// to point at, should you use actargs[kfile]
            

            ret = setefifilepath(context, ( !shouldBless && actargs[kfile].present ?
											actargs[kfile].argument :
											actargs[kmount].argument),
                                 actargs[knextonly].present,
                                 actargs[klegacy].present,
                                 actargs[klegacydrivehint].present ? actargs[klegacydrivehint].argument : NULL,
                                 actargs[koptions].present ? actargs[koptions].argument : NULL,
                                 actargs[kshortform].present ? true : false);
            if(ret) {
                return 3;
            }
        } else {
            struct statfs sb;
            
            ret = blsustatfs(actargs[kmount].argument, &sb);
            if(ret) {
                blesscontextprintf(context, kBLLogLevelError,  "Can't statfs: %s\n",
                                   strerror(errno));			
                return 2;
            }
            
            ret = setboot(context, sb.f_mntfromname, bootXdata, labeldata);
            if(ret) {
                return 3;
            }
        }		
    }
	
	if(bootXdata) CFRelease(bootXdata);
	if(bootEFIdata) CFRelease(bootEFIdata);
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
