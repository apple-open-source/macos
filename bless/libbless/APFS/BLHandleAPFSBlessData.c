/*
 * Copyright (c) 2001-2016 Apple Inc. All Rights Reserved.
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
 *  BLHandleAPFSBlessData.c
 *  bless
 *
 *  Created by Jon Becker on 9/21/16.
 *  Copyright (c) 2001-2016 Apple Inc. All Rights Reserved.
 *
 */
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <apfs/apfs_fsctl.h>
#include <System/sys/snapshot.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/IOBSD.h>
#include <APFS/APFS.h>

#include "bless.h"
#include "bless_private.h"


int BLGetAPFSBlessData(BLContextPtr context, const char *mountpoint, uint64_t *words)
{
    int ret = 0;
    
    if (fsctl(mountpoint, APFSIOC_GET_BOOTINFO, words, 0) < 0) {
        if (errno == ENOENT) {
            words[0] = words[1] = 0;
        } else {
            ret = errno;
        }
    }
    return ret;
}


int BLSetAPFSBlessData(BLContextPtr context, const char *mountpoint, uint64_t *words)
{
    return fsctl(mountpoint, APFSIOC_SET_BOOTINFO, words, 0) < 0 ? errno : 0;
}



int BLCreateAPFSVolumeInformationDictionary(BLContextPtr context, const char *mountpoint, CFDictionaryRef *outDict)
{
    uint64_t blessWords[2];
    int err;
    uint32_t i;
    uint64_t dirID;
    CFMutableDictionaryRef dict = NULL;
    CFMutableArrayRef infarray = NULL;
    
    char blesspath[MAXPATHLEN];
    
    err = BLGetAPFSBlessData(context, mountpoint, blessWords);
    if (err) {
        return 1;
    }
    
    infarray = CFArrayCreateMutable(kCFAllocatorDefault, 2, &kCFTypeArrayCallBacks);
    
    dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    for (i = 0; i < 2; i++) {
        CFMutableDictionaryRef word =
        CFDictionaryCreateMutable(kCFAllocatorDefault,0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFTypeRef val;
        
        dirID = blessWords[i];
        blesspath[0] = '\0';
        
        err = BLLookupFileIDOnMount64(context, mountpoint, dirID, blesspath, sizeof blesspath);
        
        val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &dirID);
        CFDictionaryAddValue(word, CFSTR("Directory ID"), val);
        CFRelease(val); val = NULL;
        
        val = CFStringCreateWithCString(kCFAllocatorDefault, blesspath, kCFStringEncodingUTF8);
        CFDictionaryAddValue(word, CFSTR("Path"), val);
        CFRelease(val); val = NULL;
        
        if (strlen(blesspath) == 0 || 0 == strcmp(mountpoint, "/")) {
            val = CFStringCreateWithCString(kCFAllocatorDefault, blesspath, kCFStringEncodingUTF8);
        } else {
            val = CFStringCreateWithCString(kCFAllocatorDefault, blesspath+strlen(mountpoint), kCFStringEncodingUTF8);
        }
        CFDictionaryAddValue(word, CFSTR("Relative Path"), val);
        CFRelease(val); val = NULL;
        
        CFArrayAppendValue(infarray, word);
        CFRelease(word); word = NULL;
    }
    
    CFDictionaryAddValue(dict, CFSTR("Bless Info"),
                         infarray);
    
    CFRelease(infarray); infarray = NULL;
    
    *outDict = dict;
    return 0;
}

int BLGetAPFSSnapshotBlessData(BLContextPtr context, const char *mountpoint, uuid_string_t snap_uuid)
{
    if (!snap_uuid) {
        return -1;
    }

    apfs_snap_name_lookup_t snap_lookup_data = {0};
    snap_lookup_data.type = SNAP_LOOKUP_ROOT;

    if (fsctl(mountpoint, APFSIOC_SNAP_LOOKUP, (void *)&snap_lookup_data, 0)) {
        return -1;
    }

    if (snap_lookup_data.snap_xid != 0) {
        uuid_unparse(snap_lookup_data.snap_uuid, snap_uuid);
    } else {
        memset(snap_uuid, 0, sizeof(uuid_string_t));
    }

    return 0;
}

int BLSetAPFSSnapshotBlessData(BLContextPtr context, const char *mountpoint, uuid_string_t snap_uuid)
{
    apfs_snap_name_lookup_t snap_lookup_data = {0};
    int vol_fd, err = 0;

    if (!snap_uuid) {
        return -1;
    }

    if ((vol_fd = open(mountpoint, O_RDONLY, 0 )) < 0) {
        return -1;
    }

    // passing an empty snapshot uuid is used to bless the live fs
    // this is done by calling fs_snapshot_root with an empty snapshot name
    // so no need for snapshot lookup when blessing the live fs
    if (strlen(snap_uuid)) {
        err = uuid_parse(snap_uuid, snap_lookup_data.snap_uuid);
        snap_lookup_data.type = SNAP_LOOKUP_BY_UUID;

        if (!err) {
            err = fsctl(mountpoint, APFSIOC_SNAP_LOOKUP, (void *)&snap_lookup_data, 0);
        }
    }

    if (!err) {
        err = fs_snapshot_root(vol_fd, snap_lookup_data.snap_name, 0);
    }

    close(vol_fd);
    return err;
}

CFStringRef BLGetAPFSBlessedVolumeBSDName(BLContextPtr context, const char *mountpoint, char *bless_folder, uuid_string_t vol_uuid)
{
    char *slash;
    uuid_t c_uuid;
    CFStringRef inUUID, bsd_name = NULL;
	CFStringRef testUUID;
    CFMutableDictionaryRef matching = NULL;
    io_service_t service = IO_OBJECT_NULL;
	io_service_t dataService = IO_OBJECT_NULL;
	io_iterator_t iter = IO_OBJECT_NULL;
	CFArrayRef	roleArray;
	CFStringRef role;
	kern_return_t kret;
	bool skipVolUUIDCheck;

    if (!vol_uuid) {
        return NULL;
    }

    memset(vol_uuid, 0, sizeof(uuid_string_t));
    memmove(bless_folder, bless_folder + strlen(mountpoint), strlen(bless_folder) - strlen(mountpoint) + 1);
    slash = strchr(bless_folder + 1, '/');

    if (slash) *slash = '\0';
    if (uuid_parse(bless_folder + 1, c_uuid)) {
        return NULL;
    }

    inUUID = CFStringCreateWithCString(kCFAllocatorDefault, bless_folder + 1, kCFStringEncodingUTF8);
    matching = IOServiceMatching("AppleAPFSVolume");
	kret = IOServiceGetMatchingServices(kIOMasterPortDefault, matching, &iter);
	if (kret == KERN_SUCCESS) {
		while ((service = IOIteratorNext(iter)) != IO_OBJECT_NULL) {
			skipVolUUIDCheck = false;
			
			// Try group UUID and system role
			testUUID = IORegistryEntryCreateCFProperty(service, CFSTR(kAPFSVolGroupUUIDKey), kCFAllocatorDefault, 0);
			if (testUUID) {
				if (CFEqual(testUUID, inUUID)) {
					roleArray = IORegistryEntryCreateCFProperty(service, CFSTR(kAPFSRoleKey), kCFAllocatorDefault, 0);
					if (roleArray) {
						if (CFArrayGetCount(roleArray) > 0) {
							role = CFArrayGetValueAtIndex(roleArray, 0);
							if (CFEqual(role, CFSTR("System"))) {
								CFRelease(testUUID);
								CFRelease(roleArray);
								break;
							} else {
								// This is the wrong volume in the right group.
								// We can't check its volume UUID because if it's
								// the data volume then it will match.
								// But save it away, in case this is an ARV situation.
								skipVolUUIDCheck = true;
								dataService = service;
								IOObjectRetain(dataService);
							}
						}
						CFRelease(roleArray);
					}
				}
				CFRelease(testUUID);
			}
			
			// No match.  Just try volume UUID
			if (!skipVolUUIDCheck) {
				testUUID = IORegistryEntryCreateCFProperty(service, CFSTR(kIOMediaUUIDKey), kCFAllocatorDefault, 0);
				if (testUUID) {
					if (CFEqual(testUUID, inUUID)) {
						CFRelease(testUUID);
						break;
					}
					CFRelease(testUUID);
				}
			}
			
			IOObjectRelease(service);
		}
		IOObjectRelease(iter);
		if (service) {
			bsd_name = IORegistryEntryCreateCFProperty(service, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
			if (bsd_name) strlcpy(vol_uuid, bless_folder + 1, sizeof(uuid_string_t));
			IOObjectRelease(service);
		} else {
			// We didn't find an appropriate volume, did we find a data volume without a system volume?
			if (dataService) {
				// We did!  The system volume must be ARV and we can't see it because our OS is too old.
				// Use the data volume instead.
				bsd_name = IORegistryEntryCreateCFProperty(dataService, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
				if (bsd_name) strlcpy(vol_uuid, bless_folder + 1, sizeof(uuid_string_t));
				IOObjectRelease(dataService);
			}
		}
	}
    CFRelease(inUUID);

    *slash = '/';

    return bsd_name;
}
