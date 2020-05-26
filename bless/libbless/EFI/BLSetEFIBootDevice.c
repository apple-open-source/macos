/*
 * Copyright (c) 2011-2012 Apple Inc. All rights reserved.
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

#include <dlfcn.h>
#include <sys/stat.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#include <APFS/APFS.h>

#include "bless.h"
#include "bless_private.h"

#define kAPFS_FRAMEWORK_PATH    "/System/Library/PrivateFrameworks/APFS.framework/APFS"
#define APFS_VOL_ROLE_RECOVERY	0x0004
#define APFS_VOL_ROLE_PREBOOT	0x0010


// public entry points (bless.h)
kern_return_t
BLSetEFIBootDevice(BLContextPtr context, char *bsdName)
{
    if (0 == setefidevice(context, bsdName, false /* next */,
                          false, NULL, NULL, false)) {
        return KERN_SUCCESS;
    } else {
        return KERN_FAILURE;
    }
}

kern_return_t
BLSetEFIBootDeviceOnce(BLContextPtr context, char *bsdName)
{
    if (0 == setefidevice(context, bsdName, true /* next */,
                          false, NULL, NULL, false)) {
        return KERN_SUCCESS;
    } else {
        return KERN_FAILURE;
    }
}

kern_return_t
BLSetEFIBootFileOnce(BLContextPtr context, char *path)
{
    if (0 == setefifilepath(context, path, true, NULL, NULL)) {
        return KERN_SUCCESS;
    } else {
        return KERN_FAILURE;
    }
}

/*
 * private entry points (bless_private.h)
 */

// one static helper (defined at the bottom of this file)
static int setefibootargs(BLContextPtr context, mach_port_t masterPort);

int setefidevice(BLContextPtr context, const char * bsdname, int bootNext,
				 int bootLegacy, const char *legacyHint, const char *optionalData, bool shortForm)
{
    int ret;
    
    CFStringRef xmlString = NULL;
    const char *bootString = NULL;
    struct statfs   *mnts;
    int             mntsize;
	int				i;
	struct stat		st;

	if(bootLegacy) {
        if(legacyHint) {
            ret = BLCreateEFIXMLRepresentationForDevice(context,
                                                legacyHint+5,
                                                NULL,
                                                &xmlString,
                                                false);

            if(ret) {
                return 1;
            }
            
            ret = setit(context, kIOMasterPortDefault, "efi-legacy-drive-hint", xmlString);    
            if(ret) return ret;

            ret = _forwardNVRAM(context, CFSTR("efi-legacy-drive-hint-data"), CFSTR("BootCampHD"));
            if(ret) return ret;     
            
            ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-legacy-drive-hint"));    
            if(ret) return ret;
       
        }

        ret = BLCreateEFIXMLRepresentationForLegacyDevice(context,
                                                  bsdname,
                                                  &xmlString);
	} else {
        // the given device may be pointing at a RAID
        CFDictionaryRef dict = NULL;
        CFArrayRef      array = NULL;
        char            newBSDName[64];
        
        CFStringRef firstBooter = NULL;
        CFStringRef firstData = NULL;
        int         uefiDiscBootEntry = 0;
        int         uefiDiscPartitionStart = 0;
        int         uefiDiscPartitionSize = 0;
		CFStringRef firstVolume = NULL;
		char        prebootBSD[64];
		char		prebootPath[MAXPATHLEN];
		char		prebootMountPoint[MAXPATHLEN];
		char        *partialPath;
		bool        mustUnmount = false;
		char		*pathEnd;
		size_t		pathFreeSpace;

        strlcpy(newBSDName, bsdname, sizeof newBSDName);
        
        // first check to see if we are dealing with a disk that has the following properties:
        // is optical AND is DVD AND has an El Torito Boot Catalog AND has an UEFI-bootable entry.
        // get yes/no on that, and if yes, also get some facts about where on disc it is.
        bool isUEFIDisc = isDVDWithElToritoWithUEFIBootableOS(context, newBSDName, &uefiDiscBootEntry, &uefiDiscPartitionStart, &uefiDiscPartitionSize);
        
        if (isUEFIDisc) {
            contextprintf(context, kBLLogLevelVerbose, "Disk is DVD disc with BootCatalog with UEFIBootableOS\n");
        } else {
            contextprintf(context, kBLLogLevelVerbose, "Checking if disk is complex (if it is associated with booter partitions)\n");
            ret = BLCreateBooterInformationDictionary(context, newBSDName,
                                                      &dict);
            if(ret) {
                return 1;
            }
            
			// Check for APFS preboot volume
			array = CFDictionaryGetValue(dict, kBLAPFSPrebootVolumesKey);
			if (array && CFArrayGetCount(array) > 0) {
				io_registry_entry_t rootMedia;
				CFStringRef         rootUUID;
				uint16_t            role;
				void                *frameworkHandle;
				OSStatus            (*volumeRoleFn)(const char *disk, UInt16 *getrole, const UInt16 *setrole);
				bool                substitute = true;
				
				// First check if we need to substitute or not. If installer or someone blesses
				// a folder inside the preboot or recovery volume, we don't want to substitute
				frameworkHandle = dlopen(kAPFS_FRAMEWORK_PATH, RTLD_LAZY | RTLD_LOCAL);
				if (frameworkHandle) {
					volumeRoleFn = dlsym(frameworkHandle, "APFSVolumeRole");
					if (volumeRoleFn) {
						if (volumeRoleFn(newBSDName, &role, NULL)) {
							CFRelease(dict);
							dlclose(frameworkHandle);
							return 2;
						}
						if (role == APFS_VOL_ROLE_PREBOOT || role == APFS_VOL_ROLE_RECOVERY) {
							substitute = false;
						}
					}
					dlclose(frameworkHandle);
				}
				
				if (substitute) {
					firstVolume = CFArrayGetValueAtIndex(array, 0);
					CFStringGetCString(firstVolume, prebootBSD, sizeof prebootBSD, kCFStringEncodingUTF8);
					
					// We need to mount the preboot volume so we know which UUID to use in the path.
					// Check if the preboot volume is mounted.
					mntsize = getmntinfo(&mnts, MNT_NOWAIT);
					if (!mntsize) return 3;
					for (i = 0; i < mntsize; i++) {
						if (strcmp(mnts[i].f_mntfromname + 5, prebootBSD) == 0) break;
					}
					if (i < mntsize) {
						strlcpy(prebootMountPoint, mnts[i].f_mntonname, sizeof prebootMountPoint);
					} else {
						// The preboot volume isn't mounted right now.  We'll have to mount it.
						ret = BLMountContainerVolume(context, prebootBSD, prebootMountPoint, sizeof prebootMountPoint, true);
						if (ret) return 4;
						mustUnmount = true;
					}
					snprintf(prebootPath, sizeof prebootPath, "%s/", prebootMountPoint);
					pathEnd = prebootPath + strlen(prebootPath);
					pathFreeSpace = sizeof prebootPath - strlen(prebootPath);

					rootMedia = IOServiceGetMatchingService(kIOMasterPortDefault, IOBSDNameMatching(kIOMasterPortDefault, 0, newBSDName));
					if (!rootMedia) {
						CFRelease(dict);
						if (mustUnmount) BLUnmountContainerVolume(context, prebootMountPoint);
						return 2;
					}
					rootUUID = IORegistryEntryCreateCFProperty(rootMedia, CFSTR(kIOMediaUUIDKey), kCFAllocatorDefault, 0);
					if (!rootUUID) {
						contextprintf(context, kBLLogLevelError, "No valid volume UUID for device %s\n", newBSDName);
						IOObjectRelease(rootMedia);
						if (mustUnmount) BLUnmountContainerVolume(context, prebootMountPoint);
						return 2;
					}
					CFStringGetCString(rootUUID, pathEnd, pathFreeSpace, kCFStringEncodingUTF8);
					if (stat(prebootPath, &st) < 0 || !S_ISDIR(st.st_mode)) {
						CFRelease(rootUUID);
						rootUUID = NULL;
					} else {
						contextprintf(context, kBLLogLevelVerbose, "Found system volume UUID path in preboot for %s\n", newBSDName);
					}
					if (!rootUUID) {
						rootUUID = IORegistryEntryCreateCFProperty(rootMedia, CFSTR(kAPFSVolGroupUUIDKey), kCFAllocatorDefault, 0);
						if (rootUUID) CFStringGetCString(rootUUID, pathEnd, pathFreeSpace, kCFStringEncodingUTF8);
						if (!rootUUID || stat(prebootPath, &st) < 0 || !S_ISDIR(st.st_mode)) {
							contextprintf(context, kBLLogLevelError, "No valid group or volume UUID folder for %s\n", newBSDName);
							IOObjectRelease(rootMedia);
							CFRelease(rootUUID);
							if (mustUnmount) BLUnmountContainerVolume(context, prebootMountPoint);
							return 2;
						} else {
							contextprintf(context, kBLLogLevelVerbose, "Found volume group UUID path in preboot for %s\n", newBSDName);
						}
					}
					IOObjectRelease(rootMedia);
					CFRelease(rootUUID);
					if (mustUnmount) BLUnmountContainerVolume(context, prebootMountPoint);
					contextprintf(context, kBLLogLevelVerbose, "Substituting preboot volume %s\n", prebootBSD);
					strlcat(prebootPath, "/System/Library/CoreServices/boot.efi", sizeof prebootPath);
					partialPath = pathEnd - 1;
				}
			}

			
			// check to see if there's a booter partition. If so, use it
			if (!firstVolume) {
				array = CFDictionaryGetValue(dict, kBLAuxiliaryPartitionsKey);
				if(array) {
					if(CFArrayGetCount(array) > 0) {
						firstBooter = CFArrayGetValueAtIndex(array, 0);
						if(!CFStringGetCString(firstBooter, newBSDName, sizeof(newBSDName),
											   kCFStringEncodingUTF8)) {
							return 1;
						}
						contextprintf(context, kBLLogLevelVerbose, "Substituting booter %s\n", newBSDName);
					}
				}
			}

            // check to see if there's a data partition (without auxiliary) that
            // we should boot from
            if (!firstVolume && !firstBooter) {
                array = CFDictionaryGetValue(dict, kBLDataPartitionsKey);
                if(array) {
                    if(CFArrayGetCount(array) > 0) {
                        firstData = CFArrayGetValueAtIndex(array, 0);
                        if(!CFStringGetCString(firstData, newBSDName, sizeof(newBSDName),
                                               kCFStringEncodingUTF8)) {
                            return 1;
                        }
                        if (0 == strcmp(newBSDName, bsdname)) {
                            /* same as before, no substitution */
                            firstData = NULL;
                        } else {
                            contextprintf(context, kBLLogLevelVerbose, "Substituting bootable data partition %s\n", newBSDName);
                        }
                    }
                }
            }
        }
        
        // we have the real entity we want to boot from: whether the "direct" disk, an actual underlying
        // boot helper disk, or ElTorito offset information. create EFI boot settings depending on case.
        
		if (firstVolume) {
			ret = BLCreateEFIXMLRepresentationForPartialPath(context,
															 prebootBSD,
															 partialPath,
															 optionalData,
															 &xmlString,
															 shortForm);
		} else if (isUEFIDisc) {
			ret = BLCreateEFIXMLRepresentationForElToritoEntry(context,
															   newBSDName,
															   uefiDiscBootEntry,
															   uefiDiscPartitionStart,
															   uefiDiscPartitionSize,
															   &xmlString);
        } else {
			ret = BLCreateEFIXMLRepresentationForDevice(context,
														newBSDName,
														optionalData,
														&xmlString,
														shortForm);
        }
        
        if (dict)
            CFRelease(dict);
	}
    
    if(ret) {
        return 1;
    }
    
	// TODO merge this code with BLSetEFIBootDevice() [9300208]
    if(bootNext) {
        bootString = "efi-boot-next";
    } else {
        bootString = "efi-boot-device";
    }    
    
    ret = setit(context, kIOMasterPortDefault, bootString, xmlString);    
    CFRelease(xmlString);
    if(ret) return ret;

	ret = efinvramcleanup(context);
	if(ret) return ret;
        
    return ret;
}

int setefifilepath(BLContextPtr context, const char *path, int bootNext,
				   const char *optionalData, bool shortForm)
{
    CFStringRef xmlString = NULL;
    const char *bootString = NULL;
    int ret;
    struct statfs sb;
    struct statfs   *mnts;
    int             mntsize;
	int				i;
	struct stat		st;

	if(0 != blsustatfs(path, &sb)) {
        contextprintf(context, kBLLogLevelError,  "Can't statfs %s\n" ,
                           path);
        return 1;           
    }
    
    // first try to get booter information for the block device.
    // if there is none, we can do our normal path
    // the given device may be pointing at a RAID
    CFDictionaryRef dict = NULL;
    CFArrayRef array = NULL;
    char    newBSDName[MAXPATHLEN];
    
    CFStringRef firstBooter = NULL;
    CFStringRef firstData = NULL;
    CFStringRef firstVolume = NULL;
	char        prebootBSD[64];
	char        *partialPath;
    int         uefiDiscBootEntry = 0;
    int         uefiDiscPartitionStart = 0;
    int         uefiDiscPartitionSize = 0;
	char		prebootPath[MAXPATHLEN];
	char		prebootMountPoint[MAXPATHLEN];
	bool        mustUnmount = false;
	char		*pathEnd;
	size_t		pathFreeSpace;

    strlcpy(newBSDName, sb.f_mntfromname + 5, sizeof newBSDName);
    
    // first check to see if we are dealing with a disk that has the following properties:
    // is optical AND is DVD AND has an El Torito Boot Catalog AND has an UEFI-bootable entry.
    // get yes/no on that, and if yes, also get some facts about where on disc it is.
    bool isUEFIDisc = isDVDWithElToritoWithUEFIBootableOS(context, newBSDName, &uefiDiscBootEntry, &uefiDiscPartitionStart, &uefiDiscPartitionSize);
    
    if (isUEFIDisc) {
        contextprintf(context, kBLLogLevelVerbose, "Disk is DVD disc with BootCatalog with UEFIBootableOS\n");
    } else {
        contextprintf(context, kBLLogLevelVerbose, "Checking if disk is complex (if it is associated with booter partitions)\n");
        
        ret = BLCreateBooterInformationDictionary(context, newBSDName, &dict);
        if (ret) {
            return 1;
        }
        
        // Check for APFS preboot volume
        array = CFDictionaryGetValue(dict, kBLAPFSPrebootVolumesKey);
        if (array && CFArrayGetCount(array) > 0) {
            io_registry_entry_t rootMedia;
            CFStringRef         rootUUID;
            uint16_t            role;
            void                *frameworkHandle;
            OSStatus            (*volumeRoleFn)(const char *disk, UInt16 *getrole, const UInt16 *setrole);
            bool                substitute = true;
            
            // First check if we need to substitute or not. If installer or someone blesses
            // a folder inside the preboot or recovery volume, we don't want to substitute
            frameworkHandle = dlopen(kAPFS_FRAMEWORK_PATH, RTLD_LAZY | RTLD_LOCAL);
            if (frameworkHandle) {
                volumeRoleFn = dlsym(frameworkHandle, "APFSVolumeRole");
                if (volumeRoleFn) {
                    if (volumeRoleFn(newBSDName, &role, NULL)) {
                        CFRelease(dict);
                        dlclose(frameworkHandle);
                        return 2;
                    }
					if (role == APFS_VOL_ROLE_PREBOOT || role == APFS_VOL_ROLE_RECOVERY) {
                        substitute = false;
                    }
                }
                dlclose(frameworkHandle);
            }

            if (substitute) {
                firstVolume = CFArrayGetValueAtIndex(array, 0);
                CFStringGetCString(firstVolume, prebootBSD, sizeof prebootBSD, kCFStringEncodingUTF8);
                
				// We need to mount the preboot volume so we know which UUID to use in the path.
				// Check if the preboot volume is mounted.
				mntsize = getmntinfo(&mnts, MNT_NOWAIT);
				if (!mntsize) return 3;
				for (i = 0; i < mntsize; i++) {
					if (strcmp(mnts[i].f_mntfromname + 5, prebootBSD) == 0) break;
				}
				if (i < mntsize) {
					strlcpy(prebootMountPoint, mnts[i].f_mntonname, sizeof prebootMountPoint);
				} else {
					// The preboot volume isn't mounted right now.  We'll have to mount it.
					ret = BLMountContainerVolume(context, prebootBSD, prebootMountPoint, sizeof prebootMountPoint, true);
					if (ret) return 4;
					mustUnmount = true;
				}
				snprintf(prebootPath, sizeof prebootPath, "%s/", prebootMountPoint);
				pathEnd = prebootPath + strlen(prebootPath);
				pathFreeSpace = sizeof prebootPath - strlen(prebootPath);

				rootMedia = IOServiceGetMatchingService(kIOMasterPortDefault, IOBSDNameMatching(kIOMasterPortDefault, 0, newBSDName));
				if (!rootMedia) {
					CFRelease(dict);
					if (mustUnmount) BLUnmountContainerVolume(context, prebootMountPoint);
					return 2;
				}
				rootUUID = IORegistryEntryCreateCFProperty(rootMedia, CFSTR(kIOMediaUUIDKey), kCFAllocatorDefault, 0);
				if (!rootUUID) {
					contextprintf(context, kBLLogLevelError, "No valid volume UUID for device %s\n", newBSDName);
					IOObjectRelease(rootMedia);
					if (mustUnmount) BLUnmountContainerVolume(context, prebootMountPoint);
					return 2;
				}
				CFStringGetCString(rootUUID, pathEnd, pathFreeSpace, kCFStringEncodingUTF8);
				if (stat(prebootPath, &st) < 0 || !S_ISDIR(st.st_mode)) {
					CFRelease(rootUUID);
					rootUUID = NULL;
				} else {
					contextprintf(context, kBLLogLevelVerbose, "Found system volume UUID path in preboot for %s\n", newBSDName);
				}
				if (!rootUUID) {
					rootUUID = IORegistryEntryCreateCFProperty(rootMedia, CFSTR(kAPFSVolGroupUUIDKey), kCFAllocatorDefault, 0);
					if (rootUUID) CFStringGetCString(rootUUID, pathEnd, pathFreeSpace, kCFStringEncodingUTF8);
					if (!rootUUID || stat(prebootPath, &st) < 0 || !S_ISDIR(st.st_mode)) {
						contextprintf(context, kBLLogLevelError, "No valid group or volume UUID folder for %s\n", newBSDName);
						IOObjectRelease(rootMedia);
						CFRelease(rootUUID);
						if (mustUnmount) BLUnmountContainerVolume(context, prebootMountPoint);
						return 2;
					} else {
						contextprintf(context, kBLLogLevelVerbose, "Found volume group UUID path in preboot for %s\n", newBSDName);
					}
				}
				IOObjectRelease(rootMedia);
				CFRelease(rootUUID);
				if (mustUnmount) BLUnmountContainerVolume(context, prebootMountPoint);
                contextprintf(context, kBLLogLevelVerbose, "Substituting preboot volume %s\n", prebootBSD);
				strlcat(prebootPath, "/System/Library/CoreServices/boot.efi", sizeof prebootPath);
				partialPath = pathEnd - 1;
            }
        }
        
        // check to see if there's a booter partition. If so, use it
        if (!firstVolume) {
            array = CFDictionaryGetValue(dict, kBLAuxiliaryPartitionsKey);
            if(array) {
                if(CFArrayGetCount(array) > 0) {
                    firstBooter = CFArrayGetValueAtIndex(array, 0);
                    if(!CFStringGetCString(firstBooter, newBSDName, sizeof(newBSDName),
                                           kCFStringEncodingUTF8)) {
                        return 1;
                    }
                    contextprintf(context, kBLLogLevelVerbose, "Substituting booter %s\n", newBSDName);
                }
            }
        }
        
        // check to see if there's a data partition (without auxiliary) that
        // we should boot from
        if (!firstVolume && !firstBooter) {
            array = CFDictionaryGetValue(dict, kBLDataPartitionsKey);
            if(array) {
                if(CFArrayGetCount(array) > 0) {
                    firstData = CFArrayGetValueAtIndex(array, 0);
                    if(!CFStringGetCString(firstData, newBSDName, sizeof(newBSDName),
                                           kCFStringEncodingUTF8)) {
                        return 1;
                    }
                    if (0 == strcmp(newBSDName, sb.f_mntfromname + 5)) {
                        /* same as before, no substitution */
                        firstData = NULL;
                    } else {
                        contextprintf(context, kBLLogLevelVerbose, "Substituting bootable data partition %s\n", newBSDName);
                    }
                }
            }
        }
    }
    
    // we have the real entity we want to boot from: whether the "direct" disk, an actual underlying
    // boot helper disk, or ElTorito offset information. create EFI boot settings depending on case.
    
    if (firstBooter || firstData) {
        // so this is probably a RAID. Validate that we were passed a mountpoint
        if(0 != strncmp(sb.f_mntonname, path, MAXPATHLEN)) {
            contextprintf(context, kBLLogLevelError,  "--file not supported for %s\n" ,
                               sb.f_mntonname);
            return 2;
        }
        ret = BLCreateEFIXMLRepresentationForDevice(context,
                                                    newBSDName,
                                                    optionalData,
                                                    &xmlString,
                                                    shortForm);
    } else if (isUEFIDisc) {
        ret = BLCreateEFIXMLRepresentationForElToritoEntry(context,
                                                           newBSDName,
                                                           uefiDiscBootEntry,
                                                           uefiDiscPartitionStart,
                                                           uefiDiscPartitionSize,
                                                           &xmlString);
    } else if (firstVolume) {
        ret = BLCreateEFIXMLRepresentationForPartialPath(context,
                                                         prebootBSD,
                                                         partialPath,
                                                         optionalData,
                                                         &xmlString,
                                                         shortForm);
    } else {
        ret = BLCreateEFIXMLRepresentationForPath(context,
                                                  path,
                                                  optionalData,
                                                  &xmlString,
                                                  shortForm);
    }
    
    if (dict)
        CFRelease (dict);
    
    if(ret) {
        return 1;
    }
    
    if(bootNext) {
        bootString = "efi-boot-next";
    } else {
        bootString = "efi-boot-device";
    }
    
    ret = setit(context, kIOMasterPortDefault, bootString, xmlString);
    CFRelease(xmlString);
    if(ret) {
        return 2;
    }        
    
	ret = efinvramcleanup(context);
	if(ret) return ret;
        
    return 0;
}

// no BLSet...() wrapper yet
int setefinetworkpath(BLContextPtr context, CFStringRef booterXML,
					  CFStringRef kernelXML, CFStringRef mkextXML,
					  CFStringRef kernelcacheXML, int bootNext)
{
    const char *bootString = NULL;
    int ret;
    
    if(bootNext) {
        bootString = "efi-boot-next";
    } else {
        bootString = "efi-boot-device";
    }
    
    ret = setit(context, kIOMasterPortDefault, bootString, booterXML);
    if(ret) return ret;
    
	if(kernelXML) {
		ret = setit(context, kIOMasterPortDefault, "efi-boot-file", kernelXML);
	} else {
		ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-boot-file"));
	}
    if(ret) return ret;

	if(mkextXML) {
		ret = setit(context, kIOMasterPortDefault, "efi-boot-mkext", mkextXML);
	} else {
		ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-boot-mkext"));
	}
    if(ret) return ret;

    if(kernelcacheXML) {
		ret = setit(context, kIOMasterPortDefault, "efi-boot-kernelcache", kernelcacheXML);
	} else {
		ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-boot-kernelcache"));
	}
    if(ret) return ret;
	
    
    ret = setefibootargs(context, kIOMasterPortDefault);
    if(ret) return ret;
    
    return 0;
}

int efinvramcleanup(BLContextPtr context)
{
	int ret;

	ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-boot-file"));    
    if(ret) return ret;
    
    ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-boot-mkext"));    
    if(ret) return ret;
    
    ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-boot-kernelcache"));    
    if(ret) return ret;
    
    ret = setefibootargs(context, kIOMasterPortDefault);
    if(ret) return ret;
    
    return 0;	
}


// shared helpers (used by setboot.c)

int _forwardNVRAM(BLContextPtr context, CFStringRef from, CFStringRef to)
{
    
    io_registry_entry_t optionsNode = 0;
    CFTypeRef       valRef;
    kern_return_t   kret;
    
    optionsNode = IORegistryEntryFromPath(kIOMasterPortDefault, kIODeviceTreePlane ":/options");
    
    if(IO_OBJECT_NULL == optionsNode) {
        contextprintf(context, kBLLogLevelError,  "Could not find " kIODeviceTreePlane ":/options\n");
        return 1;
    }
    
    valRef = IORegistryEntryCreateCFProperty(optionsNode, from, kCFAllocatorDefault, 0);
    
    if(valRef == NULL) {
        contextprintf(context, kBLLogLevelError,  "Could not find variable '%s'\n",
                                                    BLGetCStringDescription(from));
        return 2;
    }
    
    contextprintf(context, kBLLogLevelVerbose,  "Setting EFI NVRAM:\n" );
    contextprintf(context, kBLLogLevelVerbose,  "\t%s='...'\n", BLGetCStringDescription(to) );

    kret = IORegistryEntrySetCFProperty(optionsNode, to, valRef);
    if(kret) {
        CFRelease(valRef);
        IOObjectRelease(optionsNode);
        contextprintf(context, kBLLogLevelError,  "Could not set boot property '%s': %#x\n",
                                                        BLGetCStringDescription(to),
                                                        kret);
        return 3;        
    }

    CFRelease(valRef);    
    IOObjectRelease(optionsNode);

    return 0;
}

int setit(BLContextPtr context, mach_port_t masterPort, const char *bootvar, CFStringRef xmlstring)
{
    
    io_registry_entry_t optionsNode = 0;
    CFStringRef bootName = NULL;
    kern_return_t kret;
    char    cStr[1024];

    optionsNode = IORegistryEntryFromPath(masterPort, kIODeviceTreePlane ":/options");
    
    if(IO_OBJECT_NULL == optionsNode) {
        contextprintf(context, kBLLogLevelError,  "Could not find " kIODeviceTreePlane ":/options\n");
        return 1;
    }
    
    bootName = CFStringCreateWithCString(kCFAllocatorDefault, bootvar, kCFStringEncodingUTF8);
    if(bootName == NULL) {
        IOObjectRelease(optionsNode);
        return 2;
    }
    
    CFStringGetCString(xmlstring, cStr, sizeof(cStr), kCFStringEncodingUTF8);
    
    contextprintf(context, kBLLogLevelVerbose,  "Setting EFI NVRAM:\n" );
    contextprintf(context, kBLLogLevelVerbose,  "\t%s='%s'\n", bootvar, cStr );

    kret = IORegistryEntrySetCFProperty(optionsNode, bootName, xmlstring);
    if(kret) {
        CFRelease(bootName);
        IOObjectRelease(optionsNode);
        contextprintf(context, kBLLogLevelError,  "Could not set boot device property: %#x\n", kret);
        return 2;        
    }
    
    CFRelease(bootName);
    IOObjectRelease(optionsNode);
        
    return 0;
}

// truly private helper?
// fetch old args. If set, filter them and reset
static int setefibootargs(BLContextPtr context, mach_port_t masterPort)
{
    
    int             ret;
    char        cStr[1024], newArgs[1024];
    bool            didChange = false;
    CFStringRef     newString;
    
    ret = BLCopyEFINVRAMVariableAsString(context,
                                   CFSTR("boot-args"),
                                   &newString);
    
    if(ret) {
        contextprintf(context, kBLLogLevelError,  "Error getting NVRAM variable \"boot-args\"\n");        
        return 1;
    }
    
    if(newString == NULL) {
        // nothing set. that's OK
        contextprintf(context, kBLLogLevelVerbose,  "NVRAM variable \"boot-args\" not set.\n");        
        return 0;        
    }
        
    if(!CFStringGetCString(newString, cStr, sizeof(cStr), kCFStringEncodingUTF8)) {
        contextprintf(context, kBLLogLevelError,  "Could not interpret boot-args as string. Ignoring...\n");
        cStr[0] = '\0';
        // since we truncate boot-args, act like everything was filtered
        newArgs[0] = '\0';
        didChange = true;
        ret = 0;
    } else {
        // apply boot-args filtering to the result, track changed status
        ret = BLPreserveBootArgsIfChanged(context, cStr, newArgs, sizeof newArgs, &didChange);
    }
    
    CFRelease(newString);
    
    if(ret) {
        return ret;
    }
    if(!didChange) {
        contextprintf(context, kBLLogLevelVerbose, "New boot-args unchanged, skipping update.\n");
        return 0; // nothing changed, return success
    }
        
    newString = CFStringCreateWithCString(kCFAllocatorDefault, newArgs, kCFStringEncodingUTF8);
    if(newString == NULL) {
        return 2;
    }

    ret = setit(context, masterPort, "boot-args", newString);
    CFRelease(newString);
    if(ret)
        return ret;
    
    return 0;
}

