/*
 * Copyright (c) 2004-2005 Apple Computer, Inc. All rights reserved.
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
 *  BLGetACPIBootDeviceForMountPoint.c
 *  bless
 *
 *  Created by Shantonu Sen on Mon May 17 2004.
 *  Copyright 2004-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLGetACPIBootDeviceForMountPoint.c,v 1.2 2005/02/03 00:42:24 ssen Exp $
 *
 *  $Log: BLGetACPIBootDeviceForMountPoint.c,v $
 *  Revision 1.2  2005/02/03 00:42:24  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.1  2004/05/17 23:32:33  ssen
 *  <rdar://problem/3654275>: (Boot device key not written for UFS filesystem)
 *  Add a new API to get ACPI paths, which still uses an ioctl
 *  like for OF paths, but doesn't try to look for external booters
 *  Bug #: 3654275
 *
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>

#include "bless.h"
#include "bless_private.h"

int BLGetACPIBootDeviceForMountPoint(BLContextPtr context,
				     const unsigned char mountpoint[],
				     char acpistring[])
{

    unsigned char mntfrm[MAXPATHLEN];
    int err;
    struct stat sb;
    
    err = stat(mountpoint, &sb);
    if(err) {
	contextprintf(context, kBLLogLevelError,  "Can't stat mount point %s\n",
		      mountpoint );
	return 1;
    }
    
    if(devname(sb.st_dev, S_IFBLK) == NULL) {
	return 2;
    }
    
    snprintf(mntfrm, MAXPATHLEN, "/dev/%s", devname(sb.st_dev, S_IFBLK));
    return BLGetACPIBootDevice(context, mntfrm, acpistring);
}


