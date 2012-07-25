/*
 * Copyright (c) 2003-2007 Apple Inc. All Rights Reserved.
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
 *  extractBootBlocks.c
 *  bless/extractBootBlocks - Tool for extracting 'boot' 1 from System file
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Mon May 19 2003.
 *  Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: extractBootBlocks.c,v 1.7 2005/02/03 00:42:22 ssen Exp $
 *
 */

#include <CoreServices/CoreServices.h>

#include <libc.h>
#include <err.h>

int main(int argc, char *argv[]) {

    char *system = NULL;
    FSRef ref;
    OSStatus ret;
    short resFile;
    Handle bbHandle;
    Size size;
    size_t outputsize;
    
    if(argc != 2) {
	fprintf(stderr, "Usage: %s /S/L/CS/System\n", argv[0]);
	exit(1);
    }

    system = argv[1];

    if(isatty(fileno(stdout))) {
	fprintf(stderr, "I refuse to spew to a tty\n");
	exit(1);
    }

    ret = FSPathMakeRef((UInt8*)system, &ref, NULL);
    if(ret != noErr) {
	err(1, "Something bad happened with FSPathMakeRef");
    }

    resFile = FSOpenResFile(&ref, fsRdPerm);

    bbHandle = GetResource('boot', 1);

    if(!bbHandle || !*bbHandle) {
	CloseResFile(resFile);
	err(1, "Could not get 'boot' 1 resource");
    }

    DetachResource(bbHandle);

    size = GetHandleSize(bbHandle);
    outputsize = write(fileno(stdout), *bbHandle, size);

    if(outputsize != size) {
	DisposeHandle(bbHandle);
	CloseResFile(resFile);
	err(1, "Could not write all of boot blocks to stdout");
    }
    
    DisposeHandle(bbHandle);
    
    CloseResFile(resFile);
}
