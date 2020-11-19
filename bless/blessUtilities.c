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
#include <fcntl.h>
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
#include <IOKit/IOBSD.h>
#include <Img4Decode.h>
#include <Img4Encode.h>
#include <apfs/apfs_fsctl.h>
#include <apfs/apfs_mount.h>
#include <System/sys/snapshot.h>
#include <arvhelper.h>

enum {
	kIsInvisible                  = 0x4000, /* Files and folders */
};


static int DeleteHierarchy(char *path, int pathMax);
static int DeleteFileWithPrejudice(const char *path, struct stat *sb);
static int CopyKernelCollectionFiles(BLContextPtr context, const char *systemKCPath, const char *prebootKCPath);
static int GetFilesInDirWithPrefix(const char *directory, const char *prefix, char ***outFileList, int *outNumFiles);
static void FreeFileList(char **list, int num);
static int StringCompare(const void *a, const void *b);
static int CopyKCFile(BLContextPtr context, const char *from, const char *to);
static bool StringHasSuffix(const char *str, const char *suffix);


int BlessPrebootVolume(BLContextPtr context, const char *rootBSD, const char *bootEFISourceLocation,
					   CFDataRef labelData, CFDataRef labelData2, struct clarg actargs[klast])
{
    int             ret;
    io_service_t    rootMedia = IO_OBJECT_NULL;
    io_service_t    systemMedia = IO_OBJECT_NULL;
    CFStringRef     bootUUID = NULL;
    CFDictionaryRef booterDict = NULL;
    char            prebootBSD[MAXPATHLEN];
    int             mntsize;
    int             i;
    struct statfs   *mnts;
    char            systemPath[MAXPATHLEN];
    char            rootHashPath[MAXPATHLEN];
    char            snapshotRootPath[MAXPATHLEN];
    char            snapshotName[MAXPATHLEN];
    char            kcPath[MAXPATHLEN];
    char            prebootMountPoint[MAXPATHLEN];
    char            prebootFolderPath[MAXPATHLEN];
	char            bootEFIloc[MAXPATHLEN];
    char            prebootKCPath[MAXPATHLEN];
    bool            mustUnmount = false;
    uint64_t        blessIDs[2];
    CFDataRef       booterData;
    struct stat     existingStat;
    uint16_t        role;
	struct statfs	sfs;
	char			bridgeSrcVersionPath[MAXPATHLEN];
	char			bridgeDstVersionPath[MAXPATHLEN];
    char            prebootDirPath[MAXPATHLEN];
	CFDataRef		versionData = NULL;
	char			*pathEnd;
    char            *pathEnd2;
	bool 			unmountSnapshot = false;
    CFStringRef     bsdCF;
    char            systemDev[64] = _PATH_DEV;
    bool            mustUnmountSystem = false;
    bool            setIDs = (actargs[knextonly].present == 0);
    bool            createSnapshot = (actargs[kcreatesnapshot].present != 0);
    bool            copyBootEFI = (actargs[kbootefi].hasArg == 1);
    bool            sealedSnapshot = (actargs[klastsealedsnapshot].present != 0);
    bool            isARV;
    int             vol_fd = -1;
    bool            untagRootSnapshot = false;
    
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

	if (IOObjectConformsTo(rootMedia, "AppleAPFSSnapshot")) {
		blesscontextprintf(context, kBLLogLevelVerbose, "%s is a snapshot device\n", rootBSD);
		ret = BLAPFSSnapshotToVolume(context, rootMedia, &systemMedia);
		if (ret) {
			blesscontextprintf(context, kBLLogLevelError, "Could not resolve snapshot at %s to volume\n", rootBSD);
			goto exit;
		}
	} else {
		systemMedia = rootMedia;
		IOObjectRetain(systemMedia);
	}

    // Now let's get the BSD name of the preboot volume.
    ret = GetPrebootBSDForVolumeBSD(context, rootBSD, prebootBSD, sizeof prebootBSD);
    if (ret) {
        blesscontextprintf(context, kBLLogLevelVerbose, "Could not find preboot BSD for : %s\n", rootBSD);
        goto exit;
    }
    
    ret = GetMountForBSD(context, prebootBSD, prebootMountPoint, sizeof prebootMountPoint);
    if (ret) {
        blesscontextprintf(context, kBLLogLevelError, "Error looking up mount points\n");
        goto exit;
    }
    if (!prebootMountPoint[0]) {
        // The preboot volume isn't mounted right now.  We'll have to mount it.
        ret = BLMountContainerVolume(context, prebootBSD, prebootMountPoint, sizeof prebootMountPoint, false);
        if (ret) {
           goto exit;
        }
        mustUnmount = true;
    }
    
    // Find the appropriate UUID folder
	// We look for a volume UUID folder first.  If that's present, use it.
	// If not, look for a group UUID folder.  If that's present, use it.
	// If not, we fail out.
    ret = GetUUIDFolderPathInPreboot(context, prebootMountPoint, rootBSD, prebootFolderPath, sizeof prebootFolderPath);
    if (ret) {
        blesscontextprintf(context, kBLLogLevelError, "Error looking up UUID folder in preboot\n");
        goto exit;
    }
    
    // Save away the preboot path for later use
    strlcpy(prebootDirPath, prebootFolderPath, sizeof prebootDirPath);
    
    strlcpy(prebootKCPath, prebootFolderPath, sizeof prebootKCPath);
    strlcat(prebootKCPath, "/boot" kBL_PATH_KERNELCOLLECTIONS, sizeof prebootKCPath);
    strlcat(prebootFolderPath, kBL_PATH_CORESERVICES, sizeof prebootFolderPath);
	
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

    if (ret) goto exit;
    if (bootEFISourceLocation) {
        
        snprintf(bootEFIloc, sizeof bootEFIloc, "%s", bootEFISourceLocation);
        ret = GetMountForBSD(context, rootBSD, systemPath, sizeof systemPath);
        if (ret) {
            blesscontextprintf(context, kBLLogLevelError, "Error looking up mount points\n");
            goto exit;
        }

        if (!systemPath[0]) {
            blesscontextprintf(context, kBLLogLevelError, "Couldn't find volume mounted from /dev/%s\n", rootBSD);
            goto exit;
        }
        ret = BLIsVolumeARV(context, systemPath, rootBSD, &isARV);
        if (ret) goto exit;
            
        snprintf(rootHashPath, sizeof rootHashPath, "%s/usr/standalone/OS.dmg.root_hash", prebootDirPath);
        
        // if the rootmedia is a snapshot root, mount the corresponding system volume
        if (IOObjectConformsTo(rootMedia, "AppleAPFSSnapshot")) {
            contextprintf(context, kBLLogLevelVerbose, "Volume %s is mounted from a snapshot.\n", systemPath);
            bsdCF = IORegistryEntryCreateCFProperty(systemMedia, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
            if (!bsdCF) {
                ret = ENOENT;
                contextprintf(context, kBLLogLevelError, "No BSD name for system volume media\n");
                goto exit;
            }
            CFStringGetCString(bsdCF, systemDev + strlen(_PATH_DEV), sizeof systemDev - strlen(_PATH_DEV), kCFStringEncodingUTF8);
            mntsize = getmntinfo(&mnts, MNT_NOWAIT);
            if (!mntsize) {
                ret = 5;
                goto exit;
            }
            for (i = 0; i < mntsize; i++) {
                if (strcmp(mnts[i].f_mntfromname, systemDev) == 0) break;
            }
            if (i < mntsize) {
                strlcpy(systemPath, mnts[i].f_mntonname, sizeof systemPath);
            } else {
                // The system volume isn't mounted right now.  We'll have to mount it.
                ret = BLMountContainerVolume(context, systemDev + strlen(_PATH_DEV), systemPath, sizeof systemPath, false);
                if (ret) {
                    contextprintf(context, kBLLogLevelError, "Couldn't mount system volume from %s\n", systemDev);
                    goto exit;
                }
                mustUnmountSystem = true;
            }
            rootBSD = systemDev + strlen(_PATH_DEV);
        }
		
        // If --create-snapshot is passed then use system mount point (live system)
        // if --sealed-snaphot is passed then locate the OS.dmg.root_hash
        // and use it as the source.
        // if NON ARV and has a root snapshot(unsealed golden), use that snapshot
        // if NON ARV volume with no root snapshot(jazz, liberty etc), use system path
        // if none of the above is true(ARV), do not copy KC and bootEFI files.
        kcPath[0] = '\0';
        if (true == createSnapshot) {
            snprintf(kcPath, sizeof kcPath, "%s", systemPath);
        }
        else if (true == sealedSnapshot) {
            ret = GetSnapshotNameFromRootHash(context, rootHashPath, snapshotName, sizeof snapshotName);
            if (ret) {
                blesscontextprintf(context, kBLLogLevelError, "Error looking snapshot name from roothash %s \n", rootHashPath );
                goto exit;
            }
            blesscontextprintf(context, kBLLogLevelVerbose, "Found root snapshot %s\n", snapshotName);
            ret = GetMountForSnapshot(context, snapshotName, rootBSD, snapshotRootPath, sizeof snapshotRootPath);
            if (ret) {
                blesscontextprintf(context, kBLLogLevelError, "Error looking up mount points for snapshot %s\n", snapshotName);
                goto exit;
            }
            if (!snapshotRootPath[0]) {
                ret = BLMountSnapshot(context, rootBSD, snapshotName, snapshotRootPath, sizeof snapshotRootPath);
                if (ret) goto exit;
                unmountSnapshot = true;
            }
            blesscontextprintf(context, kBLLogLevelVerbose, "snapshot is mounted at %s\n", snapshotRootPath);
            snprintf(kcPath, sizeof kcPath, "%s", snapshotRootPath);
            untagRootSnapshot = true;
        }
        else if (isARV == false) {
            if (0 == BLAPFSSnapshotAsRoot(context, systemPath, snapshotName, sizeof snapshotName))
            {
                blesscontextprintf(context, kBLLogLevelVerbose, "Found root snapshot %s\n", snapshotName);
                ret = GetMountForSnapshot(context, snapshotName, rootBSD, snapshotRootPath, sizeof snapshotRootPath);
                if (ret) {
                    blesscontextprintf(context, kBLLogLevelError, "Error looking up mount points for snapshot %s\n", snapshotName);
                    goto exit;
                }
                if (!snapshotRootPath[0]) {
                    ret = BLMountSnapshot(context, rootBSD, snapshotName, snapshotRootPath, sizeof snapshotRootPath);
                    if (ret) goto exit;
                    unmountSnapshot = true;
                }
                blesscontextprintf(context, kBLLogLevelVerbose, "snapshot is mounted at %s\n", snapshotRootPath);
                snprintf(kcPath, sizeof kcPath, "%s", snapshotRootPath);
            }
            else
            {
                snprintf(kcPath, sizeof kcPath, "%s", systemPath);
            }
        }

        
        if (kcPath[0]) {
            
            // booter needs to be fetched from snapshot root or live system if efiboot is not passed in.
            if (copyBootEFI == false) {
                snprintf(bootEFIloc, sizeof bootEFIloc, "%s%s", kcPath, bootEFISourceLocation);
                copyBootEFI = true;
            }
            
            // Let's copy kernel collections
            strlcat(kcPath, kBL_PATH_KERNELCOLLECTIONS, sizeof kcPath);
            ret = CopyKernelCollectionFiles(context, kcPath, prebootKCPath);
            if (ret) {
                goto exit;
            }
        }
        
		if (true == copyBootEFI)
		{
            blesscontextprintf(context, kBLLogLevelVerbose, "booter path %s\n", bootEFIloc);
		
            // The booter got written.  We'll have to rewrite it to the preboot volume.
            ret = BLLoadFile(context, bootEFIloc, 0, &booterData);
            if (ret) {
                blesscontextprintf(context, kBLLogLevelVerbose,  "Could not load booter data from %s\n",
							   bootEFIloc);
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
                ret = CopyManifests(context, prebootFolderPath, bootEFIloc, systemPath);
                if (ret) {
                    blesscontextprintf(context, kBLLogLevelError, "Couldn't copy img4 manifests for file %s\n", bootEFIloc);
                }
            } else {
                blesscontextprintf(context, kBLLogLevelVerbose,  "Could not create boot.efi, no X folder specified\n" );
            }
            ret = statfs(bootEFIloc, &sfs);
            if (ret) {
                blesscontextprintf(context, kBLLogLevelError, "Could not get filesystem information for file %s\n", bootEFIloc);
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
            
            if (true == untagRootSnapshot)
            {
                if ((vol_fd = open(systemPath, O_RDONLY)) < 0) {
                    ret = errno;
                    blesscontextprintf(context, kBLLogLevelError, "Couldn't open volume %s: %s\n", systemPath, strerror(ret));
                    goto exit;
                }
                
                if (fs_snapshot_root(vol_fd, "", 0) < 0) {
                    ret = errno;
                    blesscontextprintf(context, kBLLogLevelError, "Coulnd't revert current boot snapshot on volume %s: %s\n", systemPath, strerror(ret));
                    goto exit;
                }
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
    if (systemMedia) IOObjectRelease(systemMedia);
    if (bootUUID) CFRelease(bootUUID);
    if (booterDict) CFRelease(booterDict);
    if (mustUnmount) {
        BLUnmountContainerVolume(context, prebootMountPoint);
    }
    if (unmountSnapshot) {
        BLUnmountContainerVolume(context, snapshotRootPath);
    }
    if (mustUnmountSystem) {
        BLUnmountContainerVolume(context, systemPath);
    }
    if (vol_fd >= 0) close(vol_fd);
    return ret;
}


int GetVolumeUUIDs(BLContextPtr context, const char *volBSD, CFStringRef *volUUID, CFStringRef *groupUUID)
{
	int				ret = 0;
	io_service_t	mntMedia = IO_OBJECT_NULL;
	io_service_t	volMedia = IO_OBJECT_NULL;
	
    mntMedia = IOServiceGetMatchingService(kIOMasterPortDefault, IOBSDNameMatching(kIOMasterPortDefault, 0, volBSD));
    if (!mntMedia) {
		blesscontextprintf(context, kBLLogLevelError, "No media object for device %s\n", volBSD);
		ret = EINVAL;
        goto exit;
    }
	if (IOObjectConformsTo(mntMedia, "AppleAPFSSnapshot")) {
		ret = BLAPFSSnapshotToVolume(context, mntMedia, &volMedia);
		if (ret) {
			blesscontextprintf(context, kBLLogLevelError, "Could not resolve snapshot at %s to volume\n", volBSD);
			goto exit;
		}
	} else {
		volMedia = mntMedia;
		IOObjectRetain(mntMedia);
	}
	if (volUUID) *volUUID = IORegistryEntryCreateCFProperty(volMedia, CFSTR(kIOMediaUUIDKey), kCFAllocatorDefault, 0);
	if (groupUUID) *groupUUID = IORegistryEntryCreateCFProperty(volMedia, CFSTR(kAPFSVolGroupUUIDKey), kCFAllocatorDefault, 0);
	
exit:
	if (mntMedia) IOObjectRelease(mntMedia);
	if (volMedia) IOObjectRelease(volMedia);
	return ret;
}



int GetSnapshotNameFromRootHash(BLContextPtr context, const char *rootHashPath, char *snapName, int nameLen)
{
    struct stat             existingStat;
    CFDataRef               rootHashData = NULL;
    char *                  rootHashString = NULL;
    int                     ret = 0;
    CFErrorRef              error = NULL;
    
    ret = lstat(rootHashPath, &existingStat);
    if (ret) {
        blesscontextprintf(context, kBLLogLevelError, "Could not access roothash file at %s\n",
                           rootHashPath);
        ret = errno;
        goto exit;
    }
    ret = BLLoadFile(context, rootHashPath, 0, &rootHashData);
    if (ret) {
        blesscontextprintf(context, kBLLogLevelError,  "Could not load rootHashData data from %s\n",
                           rootHashPath);
        goto exit;
    }
    rootHashString = copy_arv_apfs_system_snapshot_name(rootHashData, true, &error);
    if (rootHashString == NULL) {
        blesscontextprintf(context, kBLLogLevelError,  "Could not parse root hash from %s\n",
                           rootHashPath);
        ret = (uint32_t)CFErrorGetCode(error);
        goto exit;
    }
    snprintf(snapName, nameLen,"%s",rootHashString);

exit:
    if (rootHashData) CFRelease(rootHashData);
    if (error) CFRelease(error);
    return ret;
}



int GetMountForSnapshot(BLContextPtr context, const char *snapshotName, const char *bsd, char *mountPoint, int mountPointLen)
{
	struct statfs	*mnts;
	int             mntsize;
	int             i;
	char mountname[MAXPATHLEN];
	struct statfs				sfs;
	char rootDev[MAXPATHLEN];
    io_service_t rootmedia = IO_OBJECT_NULL;
    int ret = 0;


	mntsize = getmntinfo(&mnts, MNT_NOWAIT);
	if (!mntsize) {
		return errno;
	}
	snprintf(mountname, sizeof mountname, "%s@/dev/%s", snapshotName, bsd);
	blesscontextprintf(context, kBLLogLevelVerbose,  "is mounted %s\n", mountname);
	for (i = 0; i < mntsize; i++) {
		if (strcmp(mnts[i].f_mntfromname, mountname) == 0) break;
	}
	if (i < mntsize) {
		strlcpy(mountPoint, mnts[i].f_mntonname, mountPointLen);
	} else {
		
		if (statfs("/", &sfs) < 0) {
			ret = errno;
		}
		strlcpy(rootDev, sfs.f_mntfromname + strlen(_PATH_DEV), sizeof rootDev);
	
        // check if the current boot is the snapshot root.
		rootmedia = IOServiceGetMatchingService(kIOMasterPortDefault, IOBSDNameMatching(kIOMasterPortDefault, 0, rootDev));
		if (!rootmedia) {
			blesscontextprintf(context, kBLLogLevelError, "No media object for device %s\n", rootDev);
			ret = ENOENT;
            goto exit;
		}
        if (IOObjectConformsTo(rootmedia, "AppleAPFSSnapshot")) {
            // Extract the snapshot UUID from the AppleAPFSSnapshot.
            uuid_string_t snapUUIDString = "";
            CFStringRef snapUUID = IORegistryEntryCreateCFProperty(rootmedia, CFSTR(kIOMediaUUIDKey), NULL, 0);
            if (!snapUUID) {
                blesscontextprintf(context, kBLLogLevelError, "No valid volume UUID for device %s", rootDev );
                ret = ENOENT;
                goto exit;
            }
            CFStringGetCString(snapUUID, snapUUIDString, sizeof snapUUIDString, kCFStringEncodingUTF8);
            CFRelease(snapUUID);
            
            // Look up the snapshot's name by its UUID.
            apfs_snap_name_lookup_t lookup = { .type = SNAP_LOOKUP_BY_UUID };
            if (uuid_parse(snapUUIDString, lookup.snap_uuid) != 0) {
                blesscontextprintf(context, kBLLogLevelError,  "could not parse  %s: %s", kIOMediaUUIDKey, snapUUIDString);
                ret = EINVAL;
                goto exit;
            } else if (fsctl(sfs.f_mntonname, APFSIOC_SNAP_LOOKUP, &lookup, 0) != 0) {
                    blesscontextprintf(context, kBLLogLevelError, "could not look up snapshot by UUID: %d (%s)",  errno, strerror(errno));
                    ret = EINVAL;
                    goto exit;
            } else {
                if (strcmp(lookup.snap_name, snapshotName) == 0) {
                    strlcpy(mountPoint, "/", mountPointLen);
                }
                else
                {
                    *mountPoint = '\0';
                }
            }
        }
        else {
            *mountPoint ='\0';
        }
	}
exit:
    if (rootmedia) IOObjectRelease(rootmedia);
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



// We have two tasks: copy any appropriate files from the system location,
// and delete any files in the target location that aren't getting copied.
// We'll first make lists of the files in each location that meet our criteria.
// We'll just delete all the files in the target location, then unconditionally
// copy all the files in the system location.
static int CopyKernelCollectionFiles(BLContextPtr context, const char *systemKCPath, const char *prebootKCPath)
{
	int		ret;
	char	**systemList = NULL;
	int		numSysFiles;
	char	**prebootList = NULL;
	int		numPrebootFiles;
	int		i;
	char	systemFullPath[MAXPATHLEN];
	char	*systemPathEnd;
	char	prebootFullPath[MAXPATHLEN];
	char	*prebootPathEnd;
	
    // First get a list of the relevant files in each location
	ret = GetFilesInDirWithPrefix(systemKCPath, kBL_NAME_BOOTKERNELEXTENSIONS, &systemList, &numSysFiles);
	if (ret && ret != ENOENT) {
		blesscontextprintf(context, kBLLogLevelError, "Couldn't get system kernel collection files - %s\n", strerror(ret));
		goto exit;
	}
	if (ret) {
		// No kernel collection directory in system.  Just fake up an empty list of files.
		blesscontextprintf(context, kBLLogLevelVerbose, "No KernelCollections directory in system volume\n");
		systemList = malloc(sizeof(char *));
		numSysFiles = 0;
		ret = 0;
	}
	
	ret = GetFilesInDirWithPrefix(prebootKCPath, kBL_NAME_BOOTKERNELEXTENSIONS, &prebootList, &numPrebootFiles);
	if (ret && ret != ENOENT) {
		blesscontextprintf(context, kBLLogLevelError, "Couldn't get preboot kernel collection files - %s\n", strerror(ret));
		goto exit;
	}
	if (ret) {
		// No kernel collection directory in preboot.  Create one if necessary, and fake up an emptry list of files.
		blesscontextprintf(context, kBLLogLevelVerbose, "No KernelCollections directory in preboot volume\n");
		prebootList = malloc(sizeof(char *));
		numPrebootFiles = 0;
		if (numSysFiles > 0) {
			if (mkdir(prebootKCPath, 0755) < 0) {
				ret = errno;
				blesscontextprintf(context, kBLLogLevelError, "Couldn't create kernel collections directory in preboot - %s", strerror(ret));
				goto exit;
			}
		}
		ret = 0;
	} else {
		// We don't want to delete any manifests!  Let's remove them from this list.
		for (i = 0; i < numPrebootFiles; i++) {
			if (StringHasSuffix(prebootList[i], ".im4m")) {
				free(prebootList[i]);
				memmove(prebootList + i, prebootList + i + 1, (numPrebootFiles - i - 1) * sizeof *prebootList);
				numPrebootFiles--;
				i--;
			}
		}
	}
	
	// Delete all the files in the target location
	strlcpy(prebootFullPath, prebootKCPath, sizeof prebootFullPath);
	strlcat(prebootFullPath, "/", sizeof prebootFullPath);
	prebootPathEnd = prebootFullPath + strlen(prebootFullPath);
	for (i = 0; i < numPrebootFiles; i++) {
		strlcpy(prebootPathEnd, prebootList[i], prebootFullPath + sizeof prebootFullPath - prebootPathEnd);
		if (unlink(prebootFullPath) < 0) {
			ret = errno;
			blesscontextprintf(context, kBLLogLevelError, "Couldn't delete file %s - %s\n", prebootFullPath, strerror(ret));
			goto exit;
		}
		blesscontextprintf(context, kBLLogLevelVerbose, "Deleted KC file %s\n", prebootFullPath);
	}
	
	// Copy all the files in the source location
	strlcpy(systemFullPath, systemKCPath, sizeof systemFullPath);
	strlcat(systemFullPath, "/", sizeof systemFullPath);
	systemPathEnd = systemFullPath + strlen(systemFullPath);
	for (i = 0; i < numSysFiles; i++) {
		strlcpy(systemPathEnd, systemList[i], systemFullPath + sizeof systemFullPath - systemPathEnd);
		strlcpy(prebootPathEnd, systemList[i], prebootFullPath + sizeof prebootFullPath - prebootPathEnd);
		ret = CopyKCFile(context, systemFullPath, prebootFullPath);
		if (ret) {
			blesscontextprintf(context, kBLLogLevelError, "Error %d copying KC file %s\n", ret, systemFullPath);
			goto exit;
		}
		blesscontextprintf(context, kBLLogLevelVerbose, "Copied KC file %s to preboot\n", systemFullPath);
	}

	
exit:
	if (systemList) FreeFileList(systemList, numSysFiles);
	if (prebootList) FreeFileList(prebootList, numPrebootFiles);
	return ret;
}



static int GetFilesInDirWithPrefix(const char *directory, const char *prefix, char ***outFileList, int *outNumFiles)
{
	int				ret = 0;
	DIR				*dp = NULL;
	struct dirent	*dirent;
	struct stat		sb;
	char			path[MAXPATHLEN];
	int				endIdx;
	int				prefixLen = strlen(prefix);
	char			**list = NULL;
	int				numFiles = 0;
	int				listSize = 10;

	list = malloc(listSize * sizeof(char *));
	if (!list) {
		ret = errno;
		goto exit;
	}
	dp = opendir(directory);
	if (!dp) {
		ret = errno;
		goto exit;
	}
	readdir(dp); readdir(dp);
	strlcpy(path, directory, sizeof path);
	if (path[strlen(path)-1] != '/') strlcat(path, "/", sizeof path);
	endIdx = strlen(path);
	while ((dirent = readdir(dp)) != NULL) {
		if (strncmp(dirent->d_name, prefix, prefixLen) != 0) continue;
		if (endIdx + dirent->d_namlen >= sizeof path) continue;
		strlcpy(path + endIdx, dirent->d_name, sizeof path - endIdx);
		if (stat(path, &sb) < 0) {
			ret = errno;
			goto exit;
		}
		if (!S_ISREG(sb.st_mode)) continue;
		if (numFiles == listSize) {
			listSize *= 2;
			list = realloc(list, listSize * sizeof(char *));
			if (!list) {
				ret = errno;
				goto exit;
			}
		}
		list[numFiles++] = strdup(dirent->d_name);
	}
	qsort(list, numFiles, sizeof list[0], StringCompare);
	*outFileList = list;
	*outNumFiles = numFiles;
	
exit:
	if (dp) closedir(dp);
	if (ret && list) {
		FreeFileList(list, numFiles);
	}
	return ret;
}



static void FreeFileList(char **list, int num)
{
	int i;
	
	for (i = 0; i < num; i++) {
		free(list[i]);
	}
	free(list);
}



static int StringCompare(const void *a, const void *b)
{
	const char * const *s1 = a;
	const char * const *s2 = b;
	
	return strcmp(*s1, *s2);
}



static int CopyKCFile(BLContextPtr context, const char *from, const char *to)
{
	int			ret = 0;
	char		*buffer = NULL;
	int			fdFrom = -1;
	int			fdTo = -1;
	off_t		fileSize;
	int			bytes;
	fstore_t	preall;
	
	buffer = malloc(0x100000);	// 1 MiB
	if (!buffer) {
		ret = errno;
		blesscontextprintf(context, kBLLogLevelError, "%s\n", strerror(ret));
		goto exit;
	}
	fdFrom = open(from, O_RDONLY);
	if (fdFrom < 0) {
		ret = errno;
		blesscontextprintf(context, kBLLogLevelError, "Couldn't open KC file %s: %s\n", from, strerror(ret));
		goto exit;
	}
	fileSize = lseek(fdFrom, 0, SEEK_END);
	if (fileSize < 0) {
		ret = errno;
		blesscontextprintf(context, kBLLogLevelError, "Couldn't get size of file %s: %s\n", from, strerror(ret));
		goto exit;
	}
	fdTo = open(to, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fdTo < 0) {
		ret = errno;
		blesscontextprintf(context, kBLLogLevelError, "Couldn't create KC file %s: %s\n", to, strerror(ret));
		goto exit;
	}
	preall.fst_length = fileSize;
	preall.fst_offset = 0;
	preall.fst_flags = F_ALLOCATECONTIG;
	preall.fst_posmode = F_PEOFPOSMODE;
	if (fcntl(fdTo, F_PREALLOCATE, &preall) < 0) {
		if (errno == ENOTSUP) {
			blesscontextprintf(context, kBLLogLevelVerbose, "Preallocation not supported on this filesystem for %s\n", to);
		} else {
			ret = errno;
			blesscontextprintf(context, kBLLogLevelError, "Preallocation of %s failed: %s\n", to, strerror(ret));
			goto exit;
		}
	} else {
		blesscontextprintf(context, kBLLogLevelVerbose, "0x%08X bytes preallocated for %s\n",
						   (unsigned int)preall.fst_bytesalloc, to);
	}
	lseek(fdFrom, 0, SEEK_SET);
	while (fileSize > 0) {
		bytes = MIN(fileSize, 0x100000);
		if ((bytes = read(fdFrom, buffer, bytes)) < 0) {
			ret = errno;
			blesscontextprintf(context, kBLLogLevelError, "Error reading from %s: %s\n", from, strerror(ret));
			goto exit;
		}
		if (write(fdTo, buffer, bytes) < 0) {
			ret = errno;
			blesscontextprintf(context, kBLLogLevelError, "Error writing to %s: %s\n", to, strerror(ret));
			goto exit;
		}
		fileSize -= bytes;
	}
	
exit:
	if (buffer) free(buffer);
	if (fdFrom >= 0) close(fdFrom);
	if (fdTo >= 0) close(fdTo);
	return ret;
}



static bool StringHasSuffix(const char *str, const char *suffix)
{
	const char *cmp;
	int strLength;
	int suffixLength;

	strLength = strlen(str);
	suffixLength = strlen(suffix);
	if (strLength < suffixLength) return false;
	cmp = str + strLength - suffixLength;
	return strcmp(cmp, suffix) == 0;
}
