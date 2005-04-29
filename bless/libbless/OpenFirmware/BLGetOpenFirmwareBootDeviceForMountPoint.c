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
 *  BLGetOpenFirmwareBootDevice.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Apr 19 2001.
 *  Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLGetOpenFirmwareBootDeviceForMountPoint.c,v 1.11 2005/02/03 00:42:29 ssen Exp $
 *
 *  $Log: BLGetOpenFirmwareBootDeviceForMountPoint.c,v $
 *  Revision 1.11  2005/02/03 00:42:29  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.10  2004/04/20 21:40:45  ssen
 *  Update copyrights to 2004
 *
 *  Revision 1.9  2003/10/17 00:10:39  ssen
 *  add more const
 *
 *  Revision 1.8  2003/07/22 15:58:36  ssen
 *  APSL 2.0
 *
 *  Revision 1.7  2003/04/19 00:11:14  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.6  2003/04/16 23:57:35  ssen
 *  Update Copyrights
 *
 *  Revision 1.5  2002/06/11 00:50:51  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
 *  Revision 1.4  2002/04/27 17:55:00  ssen
 *  Rewrite output logic to format the string before sending of to logger
 *
 *  Revision 1.3  2002/04/25 07:27:30  ssen
 *  Go back to using errorprint and verboseprint inside library
 *
 *  Revision 1.2  2002/02/23 04:13:06  ssen
 *  Update to context-based API
 *
 *  Revision 1.1  2001/11/16 05:36:47  ssen
 *  Add libbless files
 *
 *  Revision 1.10  2001/11/11 06:20:59  ssen
 *  readding files
 *
 *  Revision 1.8  2001/10/26 04:19:41  ssen
 *  Add dollar Id and dollar Log
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

int BLGetOpenFirmwareBootDeviceForMountPoint(BLContextPtr context, const unsigned char mountpoint[], char ofstring[]) {
    unsigned char mntfrm[MAXPATHLEN];
    int err;
    struct stat sb;

    err = stat(mountpoint, &sb);
    if(err) {
      contextprintf(context, kBLLogLevelError,  "Can't stat mount point %s\n", mountpoint );
      return 1;
    }

    if(devname(sb.st_dev, S_IFBLK) == NULL) {
            return 2;
    }

    snprintf(mntfrm, MAXPATHLEN, "/dev/%s", devname(sb.st_dev, S_IFBLK));
    return BLGetOpenFirmwareBootDevice(context, mntfrm, ofstring);
}
    
