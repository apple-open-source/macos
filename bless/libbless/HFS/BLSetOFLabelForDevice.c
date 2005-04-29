/*
 * Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
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
 *  Copyright (c) 2003-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLSetOFLabelForDevice.c,v 1.15 2005/02/08 00:18:47 ssen Exp $
 *
 *  $Log: BLSetOFLabelForDevice.c,v $
 *  Revision 1.15  2005/02/08 00:18:47  ssen
 *  Implement support for offline updating of BootX and OF labels
 *  in Apple_Boot partitions, and also for RAIDs. Only works
 *  in --device mode so far
 *
 *  Revision 1.14  2005/02/04 20:30:43  ssen
 *  try to set type/creator even if not HFS+. UFS supports this now
 *
 *  Revision 1.13  2005/02/04 13:11:45  ssen
 *  Convert OF label code to using generic booter updating code.
 *
 *  Revision 1.12  2005/02/03 00:42:25  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.11  2004/12/03 20:05:08  ssen
 *  Port to use new MKMediaRef for OF label writing, and new
 *  BTrees.h API
 *
 *  Revision 1.10  2004/04/20 21:40:42  ssen
 *  Update copyrights to 2004
 *
 *  Revision 1.9  2004/03/21 18:10:04  ssen
 *  Update includes
 *
 *  Revision 1.8  2004/02/18 19:24:35  ssen
 *  <rdar://problem/3562918>: bless: Adopt MediaKit legacy headers
 *  Use #include <MediaKit/legacy/foo.h>
 *
 *  Revision 1.7  2003/10/17 00:10:39  ssen
 *  add more const
 *
 *  Revision 1.6  2003/08/04 05:24:16  ssen
 *  Add #ifndef _OPEN_SOURCE so that some stuff isn't in darwin
 *
 *  Revision 1.5  2003/07/22 15:58:31  ssen
 *  APSL 2.0
 *
 *  Revision 1.4  2003/04/23 00:07:51  ssen
 *  Use blostype2string for OSTypes
 *
 *  Revision 1.3  2003/04/19 00:11:08  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.2  2003/04/17 00:18:26  ssen
 *  Make compile
 *
 *  Revision 1.1  2003/04/16 23:54:28  ssen
 *  Add new BLSetOFLabelForDevice to set OF Label for unmounted
 *  HFS+ partitions
 *
 *
 */

#include <sys/types.h>

#include <CoreFoundation/CoreFoundation.h>

#include "bless.h"
#include "bless_private.h"


int BLSetOFLabelForDevice(BLContextPtr context,
			  const unsigned char device[],
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

