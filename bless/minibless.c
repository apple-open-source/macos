/*
 * Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 *  minibless.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Mon Aug 25 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: minibless.c,v 1.1 2003/08/26 00:39:08 ssen Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <err.h>

#define xstr(s) str(s)
#define str(s) #s

#include "bless.h"

void usage();

int main(int argc, char *argv[]) {

    char *mountpath = NULL;
    char *device = NULL;
    struct statfs sb;

    
    if(argc != 2)
	usage();

    mountpath = argv[1];

    if(0 != statfs(mountpath, &sb)) {
	err(1, "Can't access %s", mountpath);
    }

    device = sb.f_mntfromname;

    if(!BLIsOpenFirmwarePresent(NULL)) {
	errx(1, "OpenFirmware not present");
    }

    if(0 != BLSetOpenFirmwareBootDevice(NULL, device)) {
	errx(1, "Can't set OpenFirmware");
    }

    return 0;
}

void usage()
{
    fprintf(stderr, "Usage: " xstr(PROGRAM) " mountpoint\n");
    exit(1);
}
