/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <CoreFoundation/CoreFoundation.h>
#include "DiskArbitrationTypes.h"
#include "DiskArbitrationServerMain.h"
#include "FSParticular.h"

#define FS_RESERVED_PREFIX	"Apple_"


#define ADM_COOKIE_FILE		".autodiskmounted"

struct DiskVolume
{
    char *		fs_type;
    char *		disk_dev_name;
    char *		dev_type;
    char *		disk_name;
    char *		mount_point;
    char *		util_path;
    boolean_t		removable;
    boolean_t		writable;
    boolean_t		dirty;
    boolean_t		mounted;
};
typedef struct DiskVolume DiskVolume, *DiskVolumePtr;

void DiskVolume_new(DiskVolumePtr *diskVolume);
void DiskVolume_delete(DiskVolumePtr diskVolume);
void DiskVolume_print(DiskVolumePtr diskVolume);
void DiskVolume_setFSType(DiskVolumePtr diskVolume,char *t);
void DiskVolume_setDiskDevName(DiskVolumePtr diskVolume,char *d);
void DiskVolume_setDeviceType(DiskVolumePtr diskVolume,char *t);
void DiskVolume_setDiskName(DiskVolumePtr diskVolume,char *n);
void DiskVolume_setMountPoint(DiskVolumePtr diskVolume,char *m);
void DiskVolume_setRemovable(DiskVolumePtr diskVolume,boolean_t val);
void DiskVolume_setWritable(DiskVolumePtr diskVolume,boolean_t val);
void DiskVolume_setDirtyFS(DiskVolumePtr diskVolume,boolean_t val);
void DiskVolume_setMounted(DiskVolumePtr diskVolume,boolean_t val);
void DiskVolume_free(DiskVolumePtr diskVolume);
boolean_t DiskVolume_mount(DiskVolumePtr diskVolume);
boolean_t DiskVolume_mount_foreign(DiskVolumePtr diskVolume);
boolean_t DiskVolume_mount_ufs(DiskVolumePtr diskVolume);

struct DiskVolumes
{
    CFMutableArrayRef list;
};
typedef struct DiskVolumes DiskVolumes, *DiskVolumesPtr;

void 		DiskVolumes_new(DiskVolumesPtr *diskList);
void 		DiskVolumes_delete(DiskVolumesPtr diskList);
DiskVolumesPtr 		DiskVolumes_do_volumes(DiskVolumesPtr diskList);
unsigned 	DiskVolumes_count(DiskVolumesPtr);
DiskVolumePtr 	DiskVolumes_objectAtIndex(DiskVolumesPtr diskList,int index);
DiskVolumePtr 	DiskVolumes_volumeWithMount(DiskVolumesPtr diskList,char *path);
boolean_t     	DiskVolumes_setVolumeMountPoint(DiskVolumesPtr diskList,DiskVolumePtr vol);
DiskVolumesPtr   DiskVolumes_print(DiskVolumesPtr diskList);

extern boolean_t DiskVolume_mount_ufs(DiskVolumePtr diskVolume);

/* UI display stuff */

void StartUnrecognizedDiskDialogThread(DiskPtr disk);
int DiskArbIsHandlingUnrecognizedDisks(void);
void DiskVolume_SetTrashes(DiskVolumePtr dptr);
void StartUnmountableDiskThread(DiskPtr disk);
