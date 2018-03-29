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
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>


#include <APFS/APFS.h>

#include "enums.h"
#include "structs.h"

#include "bless.h"
#include "bless_private.h"
#include "protos.h"


static int isOFLabel(const char *data, int labelsize);


int modeFolder(BLContextPtr context, struct clarg actargs[klast]) {
	
    int ret;
    int isHFS, isAPFS, shouldBless;
	
    uint32_t folderXid = 0;                   // The directory ID specified by folderXpath
    
	CFDataRef bootXdata = NULL;
	CFDataRef bootEFIdata = NULL;
	CFDataRef labeldata = NULL;
	CFDataRef labeldata2 = NULL;
	struct statfs sb;
	BLVersionRec	osVersion;

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
    } else if(actargs[kalternateos].present) {
		// didn't give a -folder or -mount
		ret = BLGetCommonMountPoint(context, actargs[kalternateos].argument, actargs[kalternateos].argument, actargs[kmount].argument);
		if(ret) {
			blesscontextprintf(context, kBLLogLevelError, "Can't determine mount point of '%s'\n",
							   actargs[kalternateos].argument);
			return 1;
		} else {
			blesscontextprintf(context, kBLLogLevelVerbose, "Common mount point of '%s' is %s\n",
							   actargs[kalternateos].argument, actargs[kmount].argument );
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
		|| actargs[kopenfolder].present || actargs[kalternateos].present) {
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
            
            if (oldBootXdata) CFRelease(oldBootXdata);
        } else {
            blesscontextprintf(context, kBLLogLevelVerbose,  "Could not create BootX, no X folder specified\n" );
        }
    } else {
        blesscontextprintf(context, kBLLogLevelVerbose,  "No BootX creation requested\n" );
    }

    ret = BLIsMountHFS(context, actargs[kmount].argument, &isHFS);
    if (ret) {
        blesscontextprintf(context, kBLLogLevelError,  "Could not determine filesystem of %s\n", actargs[kmount].argument );
        return 1;
    }
    
    ret = BLIsMountAPFS(context, actargs[kmount].argument, &isAPFS);
    if (ret) {
        blesscontextprintf(context, kBLLogLevelError,  "Could not determine filesystem of %s\n", actargs[kmount].argument );
        return 1;
    }
    

    /* If user gave options that require boot.efi creation, do it now. */
    if(actargs[kbootefi].present) {
        if(!actargs[kbootefi].hasArg) {
			// Let's see what OS we're on
			BLGetOSVersion(context, actargs[kmount].argument, &osVersion);
			if (osVersion.major != 10) {
				// Uh-oh, what do we do with this?
				blesscontextprintf(context, kBLLogLevelError, "OS Major version unrecognized\n");
				return 2;
			}
			actargs[kbootefi].argument[0] = '\0';
			if (osVersion.minor >= 11) {
				// use bootdev.efi by default if it exists
				snprintf(actargs[kbootefi].argument, kMaxArgLength-1, "%s%s", actargs[kmount].argument, kBL_PATH_I386_BOOTDEV_EFI);
				if (access(actargs[kbootefi].argument, R_OK) != 0) {
					actargs[kbootefi].argument[0] = '\0';
				}
			}
			if (!actargs[kbootefi].argument[0]) {
				snprintf(actargs[kbootefi].argument, kMaxArgLength-1, "%s%s", actargs[kmount].argument, kBL_PATH_I386_BOOT_EFI);
			}
        }
		
		ret = BLLoadFile(context, actargs[kbootefi].argument, 0, &bootEFIdata);
		if (ret) {
			blesscontextprintf(context, kBLLogLevelVerbose,  "Could not load boot.efi data from %s\n",
							   actargs[kbootefi].argument);
		}
		
        if (actargs[kfile].present && bootEFIdata) {
            
            // check to see if needed
            CFDataRef oldEFIdata = NULL;
            struct stat oldPresence;
            
            ret = lstat(actargs[kfile].argument, &oldPresence);
            
            if (ret == 0 && S_ISREG(oldPresence.st_mode)) {
                ret = BLLoadFile(context, actargs[kfile].argument, 0, &oldEFIdata);                
            }
            if ((ret == 0) && oldEFIdata && CFEqual(oldEFIdata, bootEFIdata)) {
				blesscontextprintf(context, kBLLogLevelVerbose,  "boot.efi unchanged at %s. Skipping update...\n",
                                   actargs[kfile].argument );
            } else {
                ret = BLCreateFileWithOptions(context, bootEFIdata, actargs[kfile].argument, 0, 0, 0, isAPFS ? kTryPreallocate : kMustPreallocate);
                if (ret) {
                    blesscontextprintf(context, kBLLogLevelError,  "Could not create boot.efi at %s\n", actargs[kfile].argument );
                    return 2;
                } else {
                    blesscontextprintf(context, kBLLogLevelVerbose,  "boot.efi created successfully at %s\n",
                        actargs[kfile].argument );
                }                
            }
            
            if (oldEFIdata) CFRelease(oldEFIdata);
			ret = CopyManifests(context, actargs[kfile].argument, actargs[kbootefi].argument);
			if (ret) {
				blesscontextprintf(context, kBLLogLevelError, "Can't copy img4 manifests for file %s\n", actargs[kfile].argument);
				return 3;
			}
        } else {
            blesscontextprintf(context, kBLLogLevelVerbose,  "Could not create boot.efi, no X folder specified\n" );
        }
    } else {
        blesscontextprintf(context, kBLLogLevelVerbose,  "No boot.efi creation requested\n" );
    }
	
	if (actargs[kpersonalize].present) {
		ret = PersonalizeOSVolume(context, actargs[kmount].argument, NULL, true);
		if (ret) {
			blesscontextprintf(context, kBLLogLevelError, "Couldn't personalize volume %s\n", actargs[kmount].argument);
			return ret;
		}
	}

	if (0 != blsustatfs(actargs[kmount].argument, &sb)) {
        blesscontextprintf(context, kBLLogLevelError,  "Can't statfs %s\n" ,
                           actargs[kmount].argument);
        return 1;	    
    }
    
    if (isAPFS && actargs[ksetboot].present) {
        char        pathBuf[MAXPATHLEN];
        CFDataRef   apfsDriverData;
        char        wholeDiskBSD[1024];
        int         unit;
        
        // We need to embed the APFS driver in the container.
        sscanf(sb.f_mntfromname + 5, "disk%d", &unit);
        snprintf(wholeDiskBSD, sizeof wholeDiskBSD, "disk%d", unit);
        if (!actargs[kapfsdriver].present) {
            snprintf(pathBuf, sizeof pathBuf, "%s/%s", actargs[kmount].argument, kBL_PATH_I386_APFS_EFI);
            ret = BLLoadFile(context, pathBuf, 0, &apfsDriverData);
        } else {
            ret = BLLoadFile(context, actargs[kapfsdriver].argument, 0, &apfsDriverData);
        }
        if (ret) {
            blesscontextprintf(context, kBLLogLevelError,  "Could not load apfs.efi data from %s\n",
                               pathBuf);
            return 1;
        }
        ret = APFSContainerEFIEmbed(wholeDiskBSD, (const char *)CFDataGetBytePtr(apfsDriverData), CFDataGetLength(apfsDriverData));
        if (ret) {
            blesscontextprintf(context, kBLLogLevelError, "Could not embed APFS driver in %s - error #%d\n",
                               wholeDiskBSD, ret);
            return 1;
        }
        CFRelease(apfsDriverData);
    }
	
	
	if (actargs[klabel].present||actargs[klabelfile].present) {
		int isLabel = 0;
		
		if(actargs[klabelfile].present) {
			ret = BLLoadFile(context, actargs[klabelfile].argument, 0, &labeldata);
			if(ret) {
				blesscontextprintf(context, kBLLogLevelError, "Can't load label '%s'\n",
								   actargs[klabelfile].argument);
				return 2;
			}
		} else {
			ret = BLGenerateLabelData(context, actargs[klabel].argument, kBitmapScale_1x, &labeldata);
			if (ret) {
				blesscontextprintf(context, kBLLogLevelError, "Can't render label '%s'\n",
								   actargs[klabel].argument);
				return 3;
			}
			ret = BLGenerateLabelData(context, actargs[klabel].argument, kBitmapScale_2x, &labeldata2);
			if (ret) {
				blesscontextprintf(context, kBLLogLevelError, "Can't render label '%s'\n",
								   actargs[klabel].argument);
				return 3;
			}
		}
		
		isLabel = isOFLabel((const char *)CFDataGetBytePtr(labeldata), CFDataGetLength(labeldata));
		blesscontextprintf(context, kBLLogLevelVerbose,  "Scale 1 label data is valid: %s\n",
						   isLabel ? "YES" : "NO");
		
		if (actargs[kfolder].present) {
			char sysfolder[MAXPATHLEN];
			
			snprintf(sysfolder, sizeof sysfolder, "%s/.disk_label", actargs[kfolder].argument);
			ret = WriteLabelFile(context, sysfolder, labeldata, isLabel && isHFS, kBitmapScale_1x);
			if (ret) return 1;
			
			if (labeldata2) {
				snprintf(sysfolder, sizeof sysfolder, "%s/.disk_label_2x", actargs[kfolder].argument);
				ret = WriteLabelFile(context, sysfolder, labeldata2, 0, kBitmapScale_2x);
				if (ret) return 1;
			}
		}
	}
	

    if (shouldBless || isAPFS) {
        if (isHFS) {
            uint32_t oldwords[8];
            int useX = 1;
            uint32_t openfolder = 0;
            uint32_t bootfile = 0;
            uint32_t alternateosid = 0; /* aliased with folder9 */
            
            ret = BLGetVolumeFinderInfo(context, actargs[kmount].argument, oldwords);
            if(ret) {
                blesscontextprintf(context, kBLLogLevelError,  "Error getting old Finder info words for %s\n", actargs[kmount].argument );
                return 1;
            }
            
            if(!actargs[kfolder].present && !actargs[kfolder9].present) {
                // if no blessed folder, preserve what's there already
                actargs[ksaveX].present = 1;
            }
            
            if(actargs[ksaveX].present) {
                folderXid = oldwords[5];
                blesscontextprintf(context, kBLLogLevelVerbose,  "Saved folder X\n" );
            }
            
            /* always save boot file */
            bootfile = oldwords[1];
            alternateosid = oldwords[3];
            
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
                ret = BLGetFileID(context, actargs[kfolder9].argument, &alternateosid);
                if(ret) {
                    blesscontextprintf(context, kBLLogLevelError,  "Error while getting directory ID of %s\n", actargs[kfolder9].argument );
                } else {
                    blesscontextprintf(context, kBLLogLevelVerbose,  "Got directory ID of %u for %s\n",
                                       alternateosid, actargs[kfolder9].argument );
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
            
            if(actargs[kalternateos].present) {
                ret = BLGetFileID(context, actargs[kalternateos].argument, &alternateosid);
                if(ret) {
                    blesscontextprintf(context, kBLLogLevelError,  "Error while getting file ID of %s. Ignoring...\n", actargs[kalternateos].argument );
                    alternateosid = 0;
                } else {
                    struct stat checkForFile;
                    
                    ret = lstat(actargs[kalternateos].argument, &checkForFile);
                    if(ret || (!S_ISREG(checkForFile.st_mode) && !S_ISDIR(checkForFile.st_mode))) {
                        blesscontextprintf(context, kBLLogLevelError,  "%s cannot be accessed, or is not a regular file or directory. Ignoring...\n", actargs[kalternateos].argument );
                        alternateosid = 0;
                    } else {
                        blesscontextprintf(context, kBLLogLevelVerbose,  "Got file/directory ID of %u for %s\n",
                                           alternateosid, actargs[kalternateos].argument );
                    }
                }
                
            } else {
                // no file/directory given. we should try to verify the existing ID
                if(alternateosid) {
                    ret = BLLookupFileIDOnMount(context, actargs[kmount].argument, alternateosid, actargs[kalternateos].argument);
                    if(ret) {
                        blesscontextprintf(context, kBLLogLevelVerbose,  "Invalid EFI alternate OS file/dir ID %u. Zeroing...\n",
                                           alternateosid );
                        alternateosid = 0;
                    } else {
                        struct stat checkForFile;
                        
                        ret = lstat(actargs[kalternateos].argument, &checkForFile);
                        if(ret || (!S_ISREG(checkForFile.st_mode) && !S_ISDIR(checkForFile.st_mode))) {
                            blesscontextprintf(context, kBLLogLevelError,  "%s cannot be accessed, or is not a regular file. Ignoring...\n", actargs[kalternateos].argument );
                            alternateosid = 0;
                        } else {
                            blesscontextprintf(context, kBLLogLevelVerbose,
                                               "Preserving EFI alternate OS file/dir ID %u for %s\n",
                                               alternateosid, actargs[kalternateos].argument );
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
            oldwords[3] = alternateosid;
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
                    oldwords[0] = alternateosid;
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
            
        } else if (isAPFS) {
            uint64_t oldWords[2] = { 0, 0 };
            uint64_t folderInum = 0;
            uint64_t fileInum = 0;
            uint16_t role;
            char     *bootEFISource;
            
            if (shouldBless) {
                if (APFSVolumeRole(sb.f_mntfromname + 5, &role, NULL)) {
                    blesscontextprintf(context, kBLLogLevelError, "Couldn't get role for volume %s\n", actargs[kmount].argument);
                    return 2;
                }
                
                if (role & (APFS_VOL_ROLE_PREBOOT | APFS_VOL_ROLE_RECOVERY)) {
                    ret = BLGetAPFSBlessData(context, actargs[kmount].argument, oldWords);
                    if (ret && ret != ENOENT) {
                        blesscontextprintf(context, kBLLogLevelError,  "Error getting bless data for %s\n", actargs[kmount].argument);
                        return 1;
                    }
                    
                    /* bless! bless */
                    
                    /* First get any directory IDs we need */
                    if (actargs[kfolder].present) {
                        ret = BLGetAPFSInodeNum(context, actargs[kfolder].argument, &folderInum);
                        if (ret) {
                            blesscontextprintf(context, kBLLogLevelError,  "Error while getting inum of %s\n", actargs[kfolder].argument);
                        } else {
                            blesscontextprintf(context, kBLLogLevelVerbose,  "Got inum of %llu for %s\n",
                                               folderInum, actargs[kfolder].argument );
                        }
                    }
                    
                    if (actargs[kfile].present) {
                        ret = BLGetAPFSInodeNum(context, actargs[kfile].argument, &fileInum);
                        if (ret) {
                            blesscontextprintf(context, kBLLogLevelError,  "Error while getting inum of %s. Ignoring...\n", actargs[kfile].argument );
                            fileInum = 0;
                        } else {
                            struct stat checkForFile;
                            
                            ret = lstat(actargs[kfile].argument, &checkForFile);
                            if (ret || !S_ISREG(checkForFile.st_mode)) {
                                blesscontextprintf(context, kBLLogLevelError,  "%s cannot be accessed, or is not a regular file. Ignoring...\n", actargs[kfile].argument );
                                fileInum = 0;
                            } else {
                                blesscontextprintf(context, kBLLogLevelVerbose,  "Got inum of %llu for %s\n",
                                                   fileInum, actargs[kfile].argument);
                            }
                        }
                        
                    } else {
                        // no file given. we should try to verify the existing booter
                        if (oldWords[0]) {
                            ret = BLLookupFileIDOnMount64(context, actargs[kmount].argument, oldWords[0], actargs[kfile].argument, sizeof actargs[kfile].argument);
                            if (ret) {
                                blesscontextprintf(context, kBLLogLevelVerbose,  "Invalid EFI blessed file ID %llu. Zeroing...\n",
                                                   oldWords[0] );
                                oldWords[0] = 0;
                            } else {
                                struct stat checkForFile;
                                
                                ret = lstat(actargs[kfile].argument, &checkForFile);
                                if(ret || !S_ISREG(checkForFile.st_mode)) {
                                    blesscontextprintf(context, kBLLogLevelError,  "%s cannot be accessed, or is not a regular file. Ignoring...\n", actargs[kfile].argument );
                                    oldWords[0] = 0;
                                } else {
                                    blesscontextprintf(context, kBLLogLevelVerbose,
                                                       "Preserving EFI blessed file ID %llu for %s\n",
                                                       oldWords[0], actargs[kfile].argument );
									fileInum = oldWords[0];
                                }
                            }
                        }
                        
                    }
                    
                    oldWords[0] = fileInum;
                    oldWords[1] = folderInum;
                    
                    blesscontextprintf(context, kBLLogLevelVerbose,  "blessed file = %lld\n", oldWords[0]);
                    blesscontextprintf(context, kBLLogLevelVerbose,  "blessed folder = %lld\n", oldWords[1]);
                } else {
                    oldWords[0] = oldWords[1] = 0;
                    blesscontextprintf(context, kBLLogLevelVerbose, "clearing blessed file and folder data\n");
                }
                
                
                if (geteuid() != 0 && geteuid() != sb.f_owner) {
                    blesscontextprintf(context, kBLLogLevelError,  "Authorization required\n" );
                    return 1;
                }
                
                
                ret = BLSetAPFSBlessData(context,  actargs[kmount].argument, oldWords);
                if (ret) {
                    blesscontextprintf(context, kBLLogLevelError,  "Can't set bless data for volume mounted at %s: %s\n", actargs[kmount].argument , strerror(errno));
                    return 2;
                }
            }
            
            bootEFISource = (shouldBless && actargs[kbootefi].present) ? actargs[kbootefi].argument : NULL;
            ret = BlessPrebootVolume(context, sb.f_mntfromname + 5, bootEFISource, labeldata, labeldata2);
            if (ret) {
                blesscontextprintf(context, kBLLogLevelError,  "Couldn't bless the APFS preboot volume for volume mounted at %s: %s\n",
                                   actargs[kmount].argument, strerror(errno));
                return 2;
            }
        }
    }
    
    /* Set Open Firmware to boot off the specified volume*/
    if(actargs[ksetboot].present) {
        if(preboot == kBLPreBootEnvType_EFI) {
			// if you blessed the volume, then just point EFI at the volume.
			// only if you didn't bless, but you have something interesting
			// to point at, should you use actargs[kfile]
            

            if (actargs[klegacy].present) {
                ret = setefilegacypath(context, actargs[kmount].argument, actargs[knextonly].present,
									   actargs[klegacydrivehint].present ? actargs[klegacydrivehint].argument : NULL,
									   actargs[koptions].present ? actargs[koptions].argument : NULL);

            } else {
                ret = setefifilepath(context, ( (!shouldBless || isAPFS) && actargs[kfile].present ?
											actargs[kfile].argument :
											actargs[kmount].argument),
                                 actargs[knextonly].present,
                                 actargs[koptions].present ? actargs[koptions].argument : NULL,
                                 actargs[kshortform].present ? true : false);
            }
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
	
	if (bootXdata) CFRelease(bootXdata);
	if (bootEFIdata) CFRelease(bootEFIdata);
	if (labeldata) CFRelease(labeldata);
	if (labeldata2) CFRelease(labeldata2);

	
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



