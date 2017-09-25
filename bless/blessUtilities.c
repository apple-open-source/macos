//
//  blessUtilities.c
//

#include <libgen.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#include <APFS/APFS.h>
#include "bless.h"
#include "bless_private.h"
#include "protos.h"


enum {
	kIsInvisible                  = 0x4000, /* Files and folders */
};



int BlessPrebootVolume(BLContextPtr context, const char *rootBSD, const char *bootEFISourceLocation,
					   CFDataRef labelData, CFDataRef labelData2)
{
    int             ret;
    io_service_t    rootMedia = IO_OBJECT_NULL;
    CFStringRef     rootUUID = NULL;
    CFDictionaryRef booterDict = NULL;
    CFArrayRef      prebootVols;
    CFStringRef     prebootVol;
    char            prebootBSD[MAXPATHLEN];
    int             mntsize;
    int             i;
    struct statfs   *mnts;
    char            prebootMountPoint[MAXPATHLEN];
    char            prebootFolderPath[MAXPATHLEN];
    bool            mustUnmount = false;
    uint64_t        blessIDs[2];
    CFDataRef       booterData;
    struct stat     existingStat;
    uint16_t        role;
	struct statfs	sfs;
	char			bridgeSrcVersionPath[MAXPATHLEN];
	char			bridgeDstVersionPath[MAXPATHLEN];
	CFDataRef		versionData = NULL;
	char			*pathEnd;
    
    // Is this already a preboot or recovery volume?
    if (APFSVolumeRole(rootBSD, &role, NULL)) {
        ret = 8;
        goto exit;
    }
    if (role & (APFS_VOL_ROLE_PREBOOT | APFS_VOL_ROLE_RECOVERY)) {
        // Nothing to do here.  See ya.
        ret = 0;
        goto exit;
    }
    
    // First find the volume UUID
    rootMedia = IOServiceGetMatchingService(kIOMasterPortDefault, IOBSDNameMatching(kIOMasterPortDefault, 0, rootBSD));
    if (!rootMedia) {
        ret = 1;
        goto exit;
    }
    rootUUID = IORegistryEntryCreateCFProperty(rootMedia, CFSTR(kIOMediaUUIDKey), kCFAllocatorDefault, 0);
    if (!rootUUID) {
        ret = 2;
        goto exit;
    }
    
    // Now let's get the BSD name of the preboot volume.
    ret = BLCreateBooterInformationDictionary(context, rootBSD, &booterDict);
    if (ret) {
        ret = 3;
        goto exit;
    }
    prebootVols = CFDictionaryGetValue(booterDict, kBLAPFSPrebootVolumesKey);
    if (!prebootVols || !CFArrayGetCount(prebootVols)) {
        ret = 4;
        goto exit;
    }
    prebootVol = CFArrayGetValueAtIndex(prebootVols, 0);
    if (CFGetTypeID(prebootVol) != CFStringGetTypeID()) {
        ret = 4;
        goto exit;
    }
    CFStringGetCString(prebootVol, prebootBSD, sizeof prebootBSD, kCFStringEncodingUTF8);
    
    // Check if the preboot volume is mounted.
    mntsize = getmntinfo(&mnts, MNT_NOWAIT);
    if (!mntsize) {
        ret = 5;
        goto exit;
    }
    for (i = 0; i < mntsize; i++) {
        if (strcmp(mnts[i].f_mntfromname + 5, prebootBSD) == 0) break;
    }
    if (i < mntsize) {
        strlcpy(prebootMountPoint, mnts[i].f_mntonname, sizeof prebootMountPoint);
    } else {
        // The preboot volume isn't mounted right now.  We'll have to mount it.
        ret = MountPrebootVolume(context, prebootBSD, prebootMountPoint, sizeof prebootMountPoint);
        if (ret) goto exit;
        mustUnmount = true;
    }
    
    // Find the appropriate UUID folder
    snprintf(prebootFolderPath, sizeof prebootFolderPath, "%s/", prebootMountPoint);
    CFStringGetCString(rootUUID, prebootFolderPath + strlen(prebootFolderPath),
                       sizeof prebootFolderPath - strlen(prebootFolderPath), kCFStringEncodingUTF8);
    strlcat(prebootFolderPath, "/System/Library/CoreServices", sizeof prebootFolderPath);
	
	// Save away the /S/L/CS path for later use
	strlcpy(bridgeDstVersionPath, prebootFolderPath, sizeof bridgeDstVersionPath);

	pathEnd = prebootFolderPath + strlen(prebootFolderPath);
	
	// Label files?
	if (labelData) {
		strlcpy(pathEnd, "/.disk_label", prebootFolderPath + sizeof prebootFolderPath - pathEnd);
		ret = WriteLabelFile(context, prebootFolderPath, labelData, 0, kBitmapScale_1x);
		if (ret) {
			blesscontextprintf(context, kBLLogLevelError, "Couldn't write label file \"%s\"", prebootFolderPath);
			ret = 6;
			goto exit;
		}
		*pathEnd = '\0';
	}
	if (labelData2) {
		strlcpy(pathEnd, "/.disk_label_2x", prebootFolderPath + sizeof prebootFolderPath - pathEnd);
		ret = WriteLabelFile(context, prebootFolderPath, labelData2, 0, kBitmapScale_2x);
		if (ret) {
			blesscontextprintf(context, kBLLogLevelError, "Couldn't write label file \"%s\"", prebootFolderPath);
			ret = 6;
			goto exit;
		}
		*pathEnd = '\0';
	}
	
    ret = BLGetAPFSInodeNum(context, prebootFolderPath, &blessIDs[1]);
    if (ret) {
        blesscontextprintf(context, kBLLogLevelError, "Preboot folder path \"%s\" doesn't exist.\n", prebootFolderPath);
        ret = 6;
        goto exit;
    }
    strlcat(prebootFolderPath, "/boot.efi", sizeof prebootFolderPath);
    if (bootEFISourceLocation) {
        // The booter got written.  We'll have to rewrite it to the preboot volume.
        ret = BLLoadFile(context, bootEFISourceLocation, 0, &booterData);
        if (ret) {
            blesscontextprintf(context, kBLLogLevelVerbose,  "Could not load booter data from %s\n",
                               bootEFISourceLocation);
        }
        
        if (booterData) {
            // check to see if needed
            CFDataRef oldEFIdata = NULL;
            
            ret = lstat(prebootFolderPath, &existingStat);
            
            if (ret == 0 && S_ISREG(existingStat.st_mode)) {
                ret = BLLoadFile(context, prebootFolderPath, 0, &oldEFIdata);
            }
            if ((ret == 0) && oldEFIdata && CFEqual(oldEFIdata, booterData)) {
                blesscontextprintf(context, kBLLogLevelVerbose,  "boot.efi unchanged at %s. Skipping update...\n",
                                   prebootFolderPath);
            } else {
                ret = BLCreateFileWithOptions(context, booterData, prebootFolderPath, 0, 0, 0, kTryPreallocate);
                if (ret) {
                    blesscontextprintf(context, kBLLogLevelError,  "Could not create boot.efi at %s\n", prebootFolderPath);
                    ret = 7;
                    goto exit;
                } else {
                    blesscontextprintf(context, kBLLogLevelVerbose,  "boot.efi created successfully at %s\n",
                                       prebootFolderPath);
                }
            }
            if (oldEFIdata) CFRelease(oldEFIdata);
            ret = CopyManifests(context, prebootFolderPath, bootEFISourceLocation);
            if (ret) {
                blesscontextprintf(context, kBLLogLevelError, "Couldn't copy img4 manifests for file %s\n", bootEFISourceLocation);
            }
        } else {
            blesscontextprintf(context, kBLLogLevelVerbose,  "Could not create boot.efi, no X folder specified\n" );
        }
		ret = statfs(bootEFISourceLocation, &sfs);
		if (ret) {
			blesscontextprintf(context, kBLLogLevelError, "Could not get filesystem information for file %s\n", bootEFISourceLocation);
			ret = errno;
			goto exit;
		}
		strlcpy(bridgeSrcVersionPath, sfs.f_mntonname, sizeof bridgeSrcVersionPath);
		strlcat(bridgeSrcVersionPath, kBL_PATH_BRIDGE_VERSION, sizeof bridgeSrcVersionPath);
		if (access(bridgeSrcVersionPath, R_OK) < 0) {
			ret = ENOENT;
		} else {
			ret = BLLoadFile(context, bridgeSrcVersionPath, 0, &versionData);
		}
		if (ret) {
			blesscontextprintf(context, kBLLogLevelVerbose,  "Could not load version data from %s\n",
							   bridgeSrcVersionPath);
			ret = 0;
		}
		if (versionData) {
			strlcat(bridgeDstVersionPath, "/", sizeof bridgeDstVersionPath);
			strlcat(bridgeDstVersionPath, basename(kBL_PATH_BRIDGE_VERSION), sizeof bridgeDstVersionPath);
			ret = BLCreateFileWithOptions(context, versionData, bridgeDstVersionPath, 0, 0, 0, 0);
			CFRelease(versionData);
			if (ret) {
				blesscontextprintf(context, kBLLogLevelError, "Couldn't create file at %s\n", bridgeDstVersionPath);
				goto exit;
			}
		}
    } else {
        ret = lstat(prebootFolderPath, &existingStat);
        if (ret) {
            blesscontextprintf(context, kBLLogLevelError, "Could not access boot.efi file at %s\n",
                               prebootFolderPath);
            ret = errno;
            goto exit;
        }
    }
    ret = BLGetAPFSInodeNum(context, prebootFolderPath, &blessIDs[0]);
    if (ret) {
        blesscontextprintf(context, kBLLogLevelError, "Preboot booter path \"%s\" doesn't exist.", prebootFolderPath);
        ret = 6;
        goto exit;
    }
    ret = BLSetAPFSBlessData(context,  prebootMountPoint, blessIDs);
    if (ret) {
        blesscontextprintf(context, kBLLogLevelError,  "Can't set bless data for preboot volume: %s\n", strerror(errno));
        return 2;
    }
    
exit:
    if (rootMedia) IOObjectRelease(rootMedia);
    if (rootUUID) CFRelease(rootUUID);
    if (booterDict) CFRelease(booterDict);
    if (mustUnmount) {
        UnmountPrebootVolume(context, prebootMountPoint);
    }
    return ret;
}



int WriteLabelFile(BLContextPtr context, const char *path, CFDataRef labeldata, int doTypeCreator, int scale)
{
	int ret;
	
	blesscontextprintf(context, kBLLogLevelVerbose,  "Putting scale %d label bitmap in %s\n", scale, path);
	
	ret = BLCreateFile(context, labeldata, path,
					   0,
					   doTypeCreator ? kBL_OSTYPE_PPC_TYPE_OFLABEL : 0,
					   doTypeCreator ? kBL_OSTYPE_PPC_CREATOR_CHRP : 0);
	if (ret) {
		blesscontextprintf(context, kBLLogLevelError,  "Could not write scale %d bitmap label file\n", scale);
	} else {
		blesscontextprintf(context, kBLLogLevelVerbose, "Scale %d label written\n", scale);
	}
	
	if (!ret) {
		ret = BLSetFinderFlag(context, path, kIsInvisible, 1);
		if (ret) {
			blesscontextprintf(context, kBLLogLevelError,  "Could not set invisibility for %s\n", path);
		} else {
			blesscontextprintf(context, kBLLogLevelVerbose,  "Invisibility set for %s\n", path);
		}
	}
	return ret;
}


