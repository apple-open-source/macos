/*
 * Copyright (c) 2020 Apple Inc. All Rights Reserved.
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
 *  BLIsMountAPFSSSV.c
 *  bless
 *
 *  Copyright (c) 2020 Apple Inc. All Rights Reserved.
 *
 */

#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <string.h>
#include <APFS/APFS.h>

#include "bless.h"
#include "bless_private.h"



//
// GetIfGroupOfVolumeDevContainsCompatibleSystemRoleVolume
//
// Gets APFS Volume Group of the given dev-path-bsd-volume, then gets the parent
// container of that given volume, and then loops thru all of the container's
// volumes (siblings, including self).
//
// If a volume is found that is all of (A) a SYSTEM-role volume, AND (B) is
// fully-featured (compatible/mountable), AND (C) is part of the APFS Volume
// Group of interest (remember there can be more than one group per container
// and thus many SYSTEM/DATA/ETC volumes), then we return a YES result.
//
// The given volume is expected to have a group property, else error.
//
// Input: Volume, as Dev -- has a preceding /dev/; example: "/dev/disk12s66"
// Output: Boolean
//

static int GetIfGroupOfVolumeDevContainsCompatibleSystemRoleVolume(BLContextPtr context, const char *volumeDev, bool *result)
{
    int                 ret = 0;
    char                targetVolBSD[MAXPATHLEN];
    io_service_t        targetVolIOMedia = IO_OBJECT_NULL;
    CFStringRef         targetVolGroupUUID = NULL;
    io_service_t        conIOMedia = IO_OBJECT_NULL;
    io_iterator_t       volIter = IO_OBJECT_NULL;
    io_service_t        volIOMedia = IO_OBJECT_NULL;
    CFArrayRef          volRoles = NULL;
    CFStringRef         volRole = NULL;
    CFStringRef         volStatus = NULL;
    CFStringRef         volGroup = NULL;
    kern_return_t       kret = KERN_SUCCESS;
    
    *result = false;
    
    if (strnlen (volumeDev, MAXPATHLEN-1) < 10 /* at least "/dev/diskX" */) {
        contextprintf(context, kBLLogLevelError, "Volume dev format error\n");
        ret = 1;
        goto exit;
    }
    sscanf (volumeDev, "/dev/%s", targetVolBSD);
    /* this does not exhaustively catch all param input errors like NULL or super-long */
   
    targetVolIOMedia = IOServiceGetMatchingService(kIOMasterPortDefault, IOBSDNameMatching(kIOMasterPortDefault, 0, targetVolBSD));
    if (targetVolIOMedia == IO_OBJECT_NULL) {
        contextprintf(context, kBLLogLevelError, "Could not get IOService for %s\n", volumeDev);
        ret = 2;
        goto exit;
    }
	
	if (IOObjectConformsTo(targetVolIOMedia, "AppleAPFSSnapshot")) {
		io_registry_entry_t volMedia;
		
		contextprintf(context, kBLLogLevelVerbose, "%s is a snapshot\n", targetVolBSD);
		ret = BLAPFSSnapshotToVolume(context, targetVolIOMedia, &volMedia);
		if (ret) {
			contextprintf(context, kBLLogLevelError, "Could not resolve snapshot at %s to a volume\n", targetVolBSD);
			IOObjectRelease(targetVolIOMedia);
			ret = 2;
			goto exit;
		}
		IOObjectRelease(targetVolIOMedia);
		targetVolIOMedia = volMedia;
	}
	
    if (!IOObjectConformsTo(targetVolIOMedia, APFS_VOLUME_OBJECT)) {
        contextprintf(context, kBLLogLevelError, "%s is not an APFS volume\n", volumeDev);
        ret = 3;
        goto exit;
    }
    
    targetVolGroupUUID = IORegistryEntryCreateCFProperty(targetVolIOMedia, CFSTR(kAPFSVolGroupUUIDKey), kCFAllocatorDefault, 0);
    if (!targetVolGroupUUID) {
        contextprintf(context, kBLLogLevelError, "Volume has no group UUID\n");
        ret = 4;
        goto exit;
    }
    
    kret = IORegistryEntryGetParentEntry(targetVolIOMedia, kIOServicePlane, &conIOMedia);
    if (kret) {
        contextprintf(context, kBLLogLevelError, "Could not get parent for volume\n");
        ret = 5;
        goto exit;
    }
    if (!IOObjectConformsTo(conIOMedia, APFS_CONTAINER_OBJECT)) {
        ret = 6;
        goto exit;
    }
    
    kret = IORegistryEntryCreateIterator (conIOMedia, kIOServicePlane, 0, &volIter);
    if (kret) {
        contextprintf(context, kBLLogLevelError, "Could not get iterator for sibling volumes\n");
        ret = 7;
        goto exit;
    }
    
    while (IO_OBJECT_NULL != (volIOMedia = IOIteratorNext(volIter)))
    {
        if (IOObjectConformsTo(volIOMedia, APFS_VOLUME_OBJECT))
        {
            volRoles = IORegistryEntryCreateCFProperty(volIOMedia, CFSTR(kAPFSRoleKey), kCFAllocatorDefault, 0);
            if (volRoles != NULL) {
                if (CFArrayGetCount(volRoles) == 1) {
                    volRole = CFArrayGetValueAtIndex(volRoles, 0);
                    if (CFStringCompare(volRole, CFSTR(kAPFSVolumeRoleSystem), 0) == kCFCompareEqualTo) {
                        volStatus = IORegistryEntryCreateCFProperty(volIOMedia, CFSTR(kAPFSStatusKey), kCFAllocatorDefault, 0);
                        if (volStatus != NULL) {
                            if (CFStringCompare(volStatus, CFSTR("Online"), 0) == kCFCompareEqualTo) {
                                volGroup = IORegistryEntryCreateCFProperty(volIOMedia, CFSTR(kAPFSVolGroupUUIDKey), kCFAllocatorDefault, 0);
                                if (volGroup != NULL && CFEqual(volGroup, targetVolGroupUUID)) {
                                    *result = true;
                                    CFRelease(volGroup);
                                    volGroup = NULL;
                                    break;
                                }
                            }
                            CFRelease(volStatus);
                            volStatus = NULL;
                        }
                    }
                }
                CFRelease(volRoles);
                volRoles = NULL;
            }
        }
        IOObjectRelease(volIOMedia);
        volIOMedia = IO_OBJECT_NULL;
    }
    
exit:;
    if (volGroup)           CFRelease(volGroup);
    if (volStatus)          CFRelease(volStatus);
    if (volRoles)           CFRelease(volRoles);
    /*  volRole             is part of volRoles */
    if (volIter)            IOObjectRelease(volIter);
    if (volIOMedia)         IOObjectRelease(volIOMedia);
    if (conIOMedia)         IOObjectRelease(conIOMedia);
    if (targetVolIOMedia)   IOObjectRelease(targetVolIOMedia);
    if (targetVolGroupUUID) CFRelease(targetVolGroupUUID);
    contextprintf(context, kBLLogLevelVerbose, "Among the APFS volumes in %s's volume group %s System-roled volume which is compatible with the currently-running macOS\n", volumeDev, *result ? "exists a" : "there is no");
    return ret;
}



//
// DETERMINING THE Is-Data-Volume-Target-Driven APFS Pre-SSV To SSV RISING EDGE CONDITION
//
// 0.  Assume ANSWER NO.
// 1.  Get Target Volume (self) UUID.
// 2.  Get Target (parent) Group UUID.
// 3.  Get list of volumes in Group.
// 4.  Get Target Volume Role.
// 5.  If Target Role != DATA then ANSWER NO & goto (13).
// 6.  If Target's Group List Contains System Volume (thus its visiblity supported) then ANSWER NO & goto (13).
// 7.  Get PREBOOT Volume Mount State.
// 8.  Mount (ensure) PREBOOT as needed. Not err if already mounted, but goto (13) if unable to mount.
// 9.  Does PREBOOT/SELFUUID exist? If so, does it have a valid P/U/S/L/CS/SystemVersion.plist?
// 10. If (9) true, then ANSWER YES & goto (13).
// 11. Does PREBOOT/GROUPUUID exist? If so, does it have a valid P/U/S/L/CS/SystemVersion.plist?
// 12. If (11) true, then ANSWER YES & goto (13).
// 13. Restore PREBOOT Volume Mount State.
//

int BLIsMountAPFSDataRolePreSSVToSSV(BLContextPtr context, const char * mountpt, bool *isPreSSVToSSV)
{
    struct statfs   sc;
    char            specialMountPointPath[MAXPATHLEN];
    bool            mustUnmountPreboot = false;
    bool            yn = false;
    int             ret = 0;
	struct stat		sb;
    
    *isPreSSVToSSV = false;
    
    // Verify that we have an APFS volume, and get a /dev/diskCsV dev path.
    ret = statfs(mountpt, &sc);
    if (ret) {
        contextprintf(context, kBLLogLevelError,  "Could not statfs() %s\n", mountpt);
        return 1;
    }
    if (strcmp(sc.f_fstypename, "apfs") != 0) { /* not an error but not yes answer */ goto exit; }
    
    // Verify that we were passed a DATA-role volume.
    ret = BLIsDataRoleForAPFSVolumeDev(context, sc.f_mntfromname, &yn);
    if (ret) {
        contextprintf(context, kBLLogLevelError,  "Could not determine volume role\n");
        return 2;
    }
    if (!yn) { /* not an error but not yes answer */
        contextprintf(context, kBLLogLevelVerbose, "Non-Data-role volume, so not considered Data-Given-Pre-SSV-to-SSV case\n");
        goto exit;
    }
    
    // Verify that it is NOT the case that a SYSTEM-role volume is "reachable".
    ret = GetIfGroupOfVolumeDevContainsCompatibleSystemRoleVolume (context, sc.f_mntfromname, &yn);
    if (ret) {
        /* ProbeErr: not an error for this routine, but not yet done deciding so log, clear err, continue */
        contextprintf(context, kBLLogLevelVerbose,  "Indeterminate if reachable system volume case (%d)\n", ret);
        ret = 0;
    }
    if (yn) { /* no ProbeErr && ProbeResult=yes: not an error for this routine, and done with a NO answer */
        contextprintf(context, kBLLogLevelVerbose, "A System volume is reachable, so not considered Data-Given-Pre-SSV-to-SSV\n");
        goto exit;
    }
    
    // Try to mount Preboot and look for GROUPUUID.  We don't bother looking
	// for the volume UUID because this is a data volume, so its UUID is
	// irrelevant, and we know from looking above that we have no way of
	// finding out the system volume's UUID.
    ret = BLEnsureSpecialAPFSVolumeUUIDPath(context, sc.f_mntfromname,
                                            APFS_VOL_ROLE_PREBOOT, true,
                                            specialMountPointPath, sizeof specialMountPointPath,
                                            &mustUnmountPreboot);
	if (ret) { ret = 0; /* not an error but not yes answer either */ goto exit; }
	if (stat(specialMountPointPath, &sb) < 0 || !S_ISDIR(sb.st_mode)) {
		// Can't find appropriate UUID folder.  We'll answer no.
		goto exit;
	}
    contextprintf(context, kBLLogLevelVerbose, "Did mount or confirm Preboot at %s\n", specialMountPointPath);
    
    // To identify as a valid Preboot Subject dir, require that it have a systemversion.plist.
    ret = BLGetOSVersion(context, specialMountPointPath, NULL);
    if (ret) { ret = 0; /* not an error but not yes answer either */ goto exit; }
    contextprintf(context, kBLLogLevelVerbose, "Confirmed valid system on Preboot\n");
    
    // If here, we passed enough to rule yes. Add more checks above to construe more narrowly.
    *isPreSSVToSSV = true;
    
exit:;
    if (mustUnmountPreboot) {
        BLUnmountContainerVolume(context, specialMountPointPath);
    }
    contextprintf(context, kBLLogLevelVerbose, "This is%s an APFS Data-Volume-Parameter-Driven Pre-SSV to SSV case\n",
                  *isPreSSVToSSV ? "" : " not");
    return ret;
}
