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
 *  BLSetOFLabelForDevice.c
 *  bless
 *
 *  Created by Shantonu Sen on Wed Apr 16 2003.
 *  Copyright (c) 2003-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLSetOFLabelForDevice.c,v 1.18 2006/02/20 22:49:55 ssen Exp $
 *
 */

#include <sys/types.h>

#include <CoreFoundation/CoreFoundation.h>
#include <MediaKit/MKMedia.h>
#include <MediaKit/MKHFSUtilities.h>

#include "bless.h"
#include "bless_private.h"


static int GetBlessedFolder(BLContextPtr context, const char *device, uint32_t *blessedFolderID);


int BLSetDiskLabelForDevice(BLContextPtr context,
                            const char *device,
                            const CFDataRef label,
                            int scale)
{
    
	int				status;
	uint32_t        blessedID;
	BLUpdateBooterFileSpec	spec;
    
	status = GetBlessedFolder(context, device, &blessedID);
	if (status) {
		return 1;
	}
	
	bzero(&spec, sizeof(spec));
	
	spec.version = 0;
	spec.reqType = 0;
	spec.reqCreator = 0;
	spec.reqParentDir = blessedID;
	spec.reqFilename = (scale == 1) ? ".disk_label" : ".disk_label_2x";
	spec.payloadData = label;
	spec.postType = 0; // no type
	spec.postCreator = 0; // no type
	spec.foundFile = 0;
	spec.updatedFile = 0;
	
	status = BLUpdateBooter(context, device, &spec, 1);
	if(status) {
		contextprintf(context, kBLLogLevelError,  "Error enumerating HFS+ volume\n");		
		return 1;
	}
	
	if(!spec.foundFile) {
		contextprintf(context, kBLLogLevelError,  "No pre-existing disk label found in HFS+ volume\n");
		return 1;
	}
	if(!spec.updatedFile) {
		contextprintf(context, kBLLogLevelError,  "disk label was not updated\n");
		return 2;
	}
	
	return 0;
}


int BLSetOFLabelForDevice(BLContextPtr context,
			  const char * device,
			  const CFDataRef label)
{

    int				status;
	BLUpdateBooterFileSpec	array[2];

	bzero(array, sizeof(array));
	
	array[0].version = 0;
	array[0].reqType = kBL_OSTYPE_PPC_TYPE_OFLABEL;
	array[0].reqCreator = kBL_OSTYPE_PPC_CREATOR_CHRP;
	array[0].reqFilename = NULL;
	array[0].payloadData = label;
	array[0].postType = 0; // no type
	array[0].postCreator = 0; // no type
	array[0].foundFile = 0;
	array[0].updatedFile = 0;

	array[1].version = 0;
	array[1].reqType = kBL_OSTYPE_PPC_TYPE_OFLABEL_PLACEHOLDER;
	array[1].reqCreator = kBL_OSTYPE_PPC_CREATOR_CHRP;
	array[1].reqFilename = NULL;
	array[1].payloadData = label;
	array[1].postType = kBL_OSTYPE_PPC_TYPE_OFLABEL;
	array[1].postCreator = 0; // no type
	array[1].foundFile = 0;
	array[1].updatedFile = 0;
	
	status = BLUpdateBooter(context, device, array, 2);
	if(status) {
		contextprintf(context, kBLLogLevelError,  "Error enumerating HFS+ volume\n");		
		return 1;
	}
	
    if(!(array[0].foundFile || array[1].foundFile)) {
		contextprintf(context, kBLLogLevelError,  "No pre-existing OF label found in HFS+ volume\n");
		return 1;
    }
    if(!(array[0].updatedFile || array[1].updatedFile)) {
		contextprintf(context, kBLLogLevelError,  "OF label was not updated\n");
		return 2;
    }

    return 0;
}



static int GetBlessedFolder(BLContextPtr context, const char *device, uint32_t *blessedFolderID)
{
	int32_t             i32;
	MKMediaRef          media;
	HFSTypes            hfsType;
	HFSPlusVolumeHeader vh;
	
	media = MKMediaCreateWithPath(kCFAllocatorDefault, device, NULL, &i32);
	if (!media) {
		contextprintf(context, kBLLogLevelError,  "Can't open device %s: %d\n", device, i32);
		return 1;
	}
	
	i32 = MKReadHFSMDBVHB(media, &hfsType, &vh, NULL, NULL);
	if (i32) {
		CFRelease(media);
		contextprintf(context, kBLLogLevelError,  "Can't read HFS+ VH for device %s: %d\n", device, i32);
		return 1;        
	}
	
	CFRelease(media);
	
	if (hfsType < kHFSPlusFormat) {
		CFRelease(media);
		contextprintf(context, kBLLogLevelError, "Device %s does not have an HFS+ volume\n", device);
		return 1;
	}
	
	*blessedFolderID = vh.finderInfo[0];
	return 0;
}
