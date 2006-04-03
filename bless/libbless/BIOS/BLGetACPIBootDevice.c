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
 *  BLGetACPIBootDevice.c
 *  bless
 *
 *  Created by Shantonu Sen on Mon May 17 2004.
 *  Copyright 2004-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLGetACPIBootDevice.c,v 1.4 2005/08/22 20:49:22 ssen Exp $
 *
 *  $Log: BLGetACPIBootDevice.c,v $
 *  Revision 1.4  2005/08/22 20:49:22  ssen
 *  Change functions to take "char *foo" instead of "char foo[]".
 *  It should be semantically identical, and be more consistent with
 *  other system APIs
 *
 *  Revision 1.3  2005/06/24 16:39:49  ssen
 *  Don't use "unsigned char[]" for paths. If regular char*s are
 *  good enough for the BSD system calls, they're good enough for
 *  bless.
 *
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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/disk.h>

#import <CoreFoundation/CoreFoundation.h>

#include "bless.h"
#include "bless_private.h"

int BLGetACPIBootDevice(BLContextPtr context,
			const char * mntfrm,
			char * acpistring)
{
    int			    err;
    dk_firmware_path_t      OFDev;
    int                     devfd;
    char           rawDev[MAXPATHLEN];


    if(!mntfrm || 0 != strncmp(mntfrm, "/dev/", 5)) return 1;
    
    sprintf(rawDev, "/dev/r%s", mntfrm + 5);
    
    devfd = open(rawDev, O_RDONLY, 0);
    if(devfd < 0) return 1;
    
    err = ioctl(devfd, DKIOCGETFIRMWAREPATH, &OFDev);
    if(err) {
	contextprintf(context, kBLLogLevelError,  "Error getting ACPI path: %s\n", strerror(errno) );
	close(devfd);
	return 2;
    }
    
    close(devfd);
    
    strcpy(acpistring, OFDev.path);

    return 0;
}
