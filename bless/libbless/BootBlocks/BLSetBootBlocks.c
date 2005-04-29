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

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "bless.h"
#include "bless_private.h"

int BLSetBootBlocks(BLContextPtr context, const unsigned char mountpoint[],
		    const CFDataRef bootblocks) {

    fbootstraptransfer_t        bbr;
    int                         fd;
    int err;

    if(bootblocks == NULL) {
	contextprintf(context, kBLLogLevelError, "No boot blocks specified\n");
	return 1;
    }

    if(CFDataGetLength(bootblocks) > kBootBlocksSize) {
	contextprintf(context, kBLLogLevelError, "Boot blocks too large: %ld > %d\n",
	      CFDataGetLength(bootblocks), kBootBlocksSize);
	return 1;
    }
    
    fd = open((char *)mountpoint, O_RDONLY);
    if (fd == -1) {
	contextprintf(context, kBLLogLevelError,  "Can't open volume mount point for %s\n", mountpoint );
	return 2;
    }
    
    bbr.fbt_offset = 0;
    bbr.fbt_length = CFDataGetLength(bootblocks);
    bbr.fbt_buffer = (unsigned char *)CFDataGetBytePtr(bootblocks);
    
    err = fcntl(fd, F_WRITEBOOTSTRAP, &bbr);
    if (err) {
	contextprintf(context, kBLLogLevelError,  "Can't write boot blocks\n" );
	close(fd);
	return 3;
    } else {
	contextprintf(context, kBLLogLevelVerbose,  "Boot blocks written successfully\n" );
    }
    close(fd);
    
    return 0;
}
