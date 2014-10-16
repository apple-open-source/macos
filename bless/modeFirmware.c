/*
 * Copyright (c) 2005-2007 Apple Inc. All Rights Reserved.
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
 *  modeFirmware.c
 *  bless
 *
 *  Created by Shantonu Sen on 2/22/05.
 *  Copyright 2005-2007 Apple Inc. All Rights Reserved.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/mount.h>
#include <sys/stat.h>
#include <fts.h>
#include <dirent.h>

#include <AvailabilityMacros.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/IOCFUnserialize.h>

#include "enums.h"
#include "structs.h"

#include "bless.h"
#include "bless_private.h"
#include "protos.h"

#if USE_DISKARBITRATION
#include <DiskArbitration/DiskArbitration.h>
#endif


int modeFirmware(BLContextPtr context, struct clarg actargs[klast])
{
	int ret = 0;
	
	return ret;
}


