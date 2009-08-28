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
 *  minibless.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Mon Aug 25 2003.
 *  Copyright (c) 2003-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: minibless.c,v 1.12 2006/07/18 22:09:51 ssen Exp $
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

#include "bless.h"
#include "protos.h"

void miniusage(char *program);

int main(int argc, char *argv[]) {
	
    char *mountpath = NULL;
    char *device = NULL;
    struct statfs sb;
	
    
    if(argc != 2)
		miniusage(argv[0]);
	
    mountpath = argv[1];
	
    if(0 != statfs(mountpath, &sb)) {
		err(1, "Can't access %s", mountpath);
    }
	
    device = sb.f_mntfromname;
	
	// normally i would object to using preprocessor macros for harware
	// tests on principle. in this case, we do this so that IOKit
	// features and stuff that only apply to 10.4 and greater are only
	// linked in for the i386 side, for instance if we were to use per-arch
	// SDKs in the future.
#ifdef __ppc__
    if(!BLIsOpenFirmwarePresent(NULL)) {
		errx(1, "OpenFirmware not present");
    }
	
    if(0 != BLSetOpenFirmwareBootDevice(NULL, device)) {
		errx(1, "Can't set OpenFirmware");
    }
#else
#ifdef __i386__
	if(0 != setefidevice(NULL, device + 5 /* strlen("/dev/") */,
			     0,
			     0,
			     NULL,
			     NULL,
                       false)) {
		errx(1, "Can't set EFI");		
	}
	
#else
#error wha?????
#endif
#endif
	
    return 0;
}

void miniusage(char *program)
{
    FILE *mystderr = fdopen(STDERR_FILENO, "w");
    
    if(mystderr == NULL)
		err(1, "Can't open stderr");
    
    fprintf(mystderr, "Usage: %s mountpoint\n", program);
    exit(1);
}

// we don't implement output
int blesscontextprintf(BLContextPtr context, int loglevel, char const *fmt, ...) {
	return 0;
}
