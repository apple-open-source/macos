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
 *  minibless.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Mon Aug 25 2003.
 *  Copyright (c) 2003-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: minibless.c,v 1.8 2005/12/04 07:42:58 ssen Exp $
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

void usage(char *program);
extern int setefidevice(BLContextPtr context, const char * bsdname,
						int bootNext, const char *optionalData);

int main(int argc, char *argv[]) {
	
    char *mountpath = NULL;
    char *device = NULL;
    struct statfs sb;
	
    
    if(argc != 2)
		usage(argv[0]);
	
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
					   NULL)) {
		errx(1, "Can't set EFI");		
	}
	
#else
#err wha?????
#endif
#endif
	
    return 0;
}

void usage(char *program)
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
