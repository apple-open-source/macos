//
//  blessUtilities.c
//

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <libgen.h>
#include <unistd.h>
#include <errno.h>
#include <paths.h>
#include <sys/param.h>
#include <sys/mount.h>
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


static int DeleteHierarchy(char *path, int pathMax);
static int DeleteFileWithPrejudice(const char *path, struct stat *sb);


int BlessPrebootVolume(BLContextPtr context, const char *rootBSD, const char *bootEFISourceLocation,
					   CFDataRef labelData, CFDataRef labelData2, bool setIDs)
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
    char            *pathEnd2;
	size_t			pathFreeSpace;
    
    // Is this already a preboot or recovery volume?
    if (APFSVolumeRole(rootBSD, &role, NULL)) {
        ret = 8;
        goto exit;
    }
	if (role == APFS_VOL_ROLE_PREBOOT || role == APFS_VOL_ROLE_RECOVERY) {
        // Nothing to do here.  See ya.
        ret = 0;
        goto exit;
    }
    
    // Let's get the IOMedia for this device.  We'll need it for the UUID later.
    rootMedia = IOServiceGetMatchingService(kIOMasterPortDefault, IOBSDNameMatching(kIOMasterPortDefault, 0, rootBSD));
    if (!rootMedia) {
        ret = 1;
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
        ret = BLMountContainerVolume(context, prebootBSD, prebootMountPoint, sizeof prebootMountPoint, false);
        if (ret) goto exit;
        mustUnmount = true;
    }
    
    // Find the appropriate UUID folder
	// We look for a volume UUID folder first.  If that's present, use it.
	// If not, look for a group UUID folder.  If that's present, use it.
	// If not, we fail out.
    snprintf(prebootFolderPath, sizeof prebootFolderPath, "%s/", prebootMountPoint);
	pathEnd = prebootFolderPath + strlen(prebootFolderPath);
	pathFreeSpace = sizeof prebootFolderPath - strlen(prebootFolderPath);
	rootUUID = IORegistryEntryCreateCFProperty(rootMedia, CFSTR(kIOMediaUUIDKey), kCFAllocatorDefault, 0);
	if (!rootUUID) {
		blesscontextprintf(context, kBLLogLevelError, "No valid volume UUID for device %s\n", rootBSD);
		ret = 2;
		goto exit;
	}
	CFStringGetCString(rootUUID, pathEnd, pathFreeSpace, kCFStringEncodingUTF8);
	if (stat(prebootFolderPath, &existingStat) < 0 || !S_ISDIR(existingStat.st_mode)) {
		CFRelease(rootUUID);
		rootUUID = NULL;
	} else {
		blesscontextprintf(context, kBLLogLevelVerbose, "Found system volume UUID path in preboot for %s\n", rootBSD);
	}
	if (!rootUUID) {
		rootUUID = IORegistryEntryCreateCFProperty(rootMedia, CFSTR(kAPFSVolGroupUUIDKey), kCFAllocatorDefault, 0);
		if (rootUUID) CFStringGetCString(rootUUID, pathEnd, pathFreeSpace, kCFStringEncodingUTF8);
		if (!rootUUID || stat(prebootFolderPath, &existingStat) < 0 || !S_ISDIR(existingStat.st_mode)) {
			blesscontextprintf(context, kBLLogLevelError, "No valid group or volume UUID folder for %s\n", rootBSD);
			ret = 2;
			goto exit;
		} else {
			blesscontextprintf(context, kBLLogLevelVerbose, "Found volume group UUID path in preboot for %s\n", rootBSD);
		}
	}
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
	
    if (setIDs) {
        ret = BLGetAPFSInodeNum(context, prebootFolderPath, &blessIDs[1]);
        if (ret) {
            blesscontextprintf(context, kBLLogLevelError, "Preboot folder path \"%s\" doesn't exist.\n", prebootFolderPath);
            ret = 6;
            goto exit;
        }
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
        strlcat(bridgeDstVersionPath, "/", sizeof bridgeDstVersionPath);
        pathEnd = bridgeSrcVersionPath + strlen(bridgeSrcVersionPath);
        pathEnd2 = bridgeDstVersionPath + strlen(bridgeDstVersionPath);
        strlcpy(pathEnd, kBL_PATH_BRIDGE_VERSION_BIN, bridgeSrcVersionPath + sizeof bridgeSrcVersionPath - pathEnd);
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
            strlcpy(pathEnd2, basename(kBL_PATH_BRIDGE_VERSION_BIN), bridgeDstVersionPath + sizeof bridgeDstVersionPath - pathEnd2);
			ret = BLCreateFileWithOptions(context, versionData, bridgeDstVersionPath, 0, 0, 0, 0);
			CFRelease(versionData);
			if (ret) {
				blesscontextprintf(context, kBLLogLevelError, "Couldn't create file at %s\n", bridgeDstVersionPath);
				goto exit;
			}
		}
        strlcpy(pathEnd, kBL_PATH_BRIDGE_VERSION_PLIST, bridgeSrcVersionPath + sizeof bridgeSrcVersionPath - pathEnd);
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
            strlcpy(pathEnd2, basename(kBL_PATH_BRIDGE_VERSION_PLIST), bridgeDstVersionPath + sizeof bridgeDstVersionPath - pathEnd2);
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
    if (setIDs) {
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
    }
    
exit:
    if (rootMedia) IOObjectRelease(rootMedia);
    if (rootUUID) CFRelease(rootUUID);
    if (booterDict) CFRelease(booterDict);
    if (mustUnmount) {
        BLUnmountContainerVolume(context, prebootMountPoint);
    }
    return ret;
}



int GetPrebootBSDForVolumeBSD(BLContextPtr context, const char *volBSD, char *prebootBSD, int prebootBSDLen)
{
	int				ret;
	CFDictionaryRef	booterDict;
	CFArrayRef		prebootVols;
	CFStringRef		prebootVol;
	
	ret = BLCreateBooterInformationDictionary(context, volBSD, &booterDict);
	if (ret) {
		blesscontextprintf(context, kBLLogLevelError, "Could not get boot information for device %s\n", volBSD);
		goto exit;
	}
	prebootVols = CFDictionaryGetValue(booterDict, kBLAPFSPrebootVolumesKey);
	if (!prebootVols || !CFArrayGetCount(prebootVols)) {
		blesscontextprintf(context, kBLLogLevelError, "No preboot volume associated with device %s\n", volBSD);
		ret = EINVAL;
		goto exit;
	}
	prebootVol = CFArrayGetValueAtIndex(prebootVols, 0);
	if (CFGetTypeID(prebootVol) != CFStringGetTypeID()) {
		blesscontextprintf(context, kBLLogLevelError, "Badly formed registry entry for preboot volume\n");
		ret = EILSEQ;
		goto exit;
	}
	CFStringGetCString(prebootVol, prebootBSD, prebootBSDLen, kCFStringEncodingUTF8);
	
exit:
	return ret;
}


int GetVolumeUUIDs(BLContextPtr context, const char *volBSD, CFStringRef *volUUID, CFStringRef *groupUUID)
{
	int				ret = 0;
	io_service_t	mntMedia = IO_OBJECT_NULL;
	
    mntMedia = IOServiceGetMatchingService(kIOMasterPortDefault, IOBSDNameMatching(kIOMasterPortDefault, 0, volBSD));
    if (!mntMedia) {
		blesscontextprintf(context, kBLLogLevelError, "No media object for device %s\n", volBSD);
		ret = EINVAL;
        goto exit;
    }
	if (volUUID) *volUUID = IORegistryEntryCreateCFProperty(mntMedia, CFSTR(kIOMediaUUIDKey), kCFAllocatorDefault, 0);
	if (groupUUID) *groupUUID = IORegistryEntryCreateCFProperty(mntMedia, CFSTR(kAPFSVolGroupUUIDKey), kCFAllocatorDefault, 0);
	
exit:
	if (mntMedia) IOObjectRelease(mntMedia);
	return ret;
}



int GetMountForBSD(BLContextPtr context, const char *bsd, char *mountPoint, int mountPointLen)
{
	struct statfs	*mnts;
    int             mntsize;
    int             i;

    mntsize = getmntinfo(&mnts, MNT_NOWAIT);
    if (!mntsize) {
		return errno;
	}
    for (i = 0; i < mntsize; i++) {
        if (strcmp(mnts[i].f_mntfromname + strlen(_PATH_DEV), bsd) == 0) break;
    }
    if (i < mntsize) {
        strlcpy(mountPoint, mnts[i].f_mntonname, mountPointLen);
    } else {
		*mountPoint = '\0';
	}
	return 0;
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




int DeleteFileOrDirectory(const char *path)
{
	int			ret = 0;
	struct stat sb;
	char		modPath[MAXPATHLEN];
	
	if (stat(path, &sb) < 0) {
		ret = errno;
		goto exit;
	}
	if (S_ISDIR(sb.st_mode)) {
		strlcpy(modPath, path, sizeof modPath);
		ret = DeleteHierarchy(modPath, sizeof modPath);
	} else {
		ret = DeleteFileWithPrejudice(path, &sb);
	}
	
exit:
	return ret;
}



static int DeleteHierarchy(char *path, int pathMax)
{
	int				ret;
	DIR				*dp = NULL;
	struct dirent	*dirent;
	struct stat		sb;
	int				endIdx;
	
	dp = opendir(path);
	if (!dp) {
		ret = errno;
		goto exit;
	}
	readdir(dp);
	readdir(dp);
	if (path[strlen(path)-1] != '/') strlcat(path, "/", pathMax);
	endIdx = strlen(path);
	ret = 0;
	while ((dirent = readdir(dp)) != NULL) {
		if (endIdx + dirent->d_namlen >= pathMax) continue;
		strlcpy(path + endIdx, dirent->d_name, pathMax - endIdx);
		if (stat(path, &sb) < 0) {
			ret = errno;
			break;
		}
		if (S_ISDIR(sb.st_mode)) {
			ret = DeleteHierarchy(path, pathMax);
		} else {
			ret = DeleteFileWithPrejudice(path, &sb);
		}
		if (ret) break;
	}
	if (ret) goto exit;
	path[endIdx] = '\0';
	rmdir(path);
	
exit:
	if (dp) closedir(dp);
	return ret;
}



static int DeleteFileWithPrejudice(const char *path, struct stat *sb)
{
	if ((sb->st_flags & UF_IMMUTABLE) != 0) {
		chflags(path, sb->st_flags & ~UF_IMMUTABLE);
	}
	return (unlink(path) < 0) ? errno : 0;
}
