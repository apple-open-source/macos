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
 *  BLCopyFileFromCFData.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Fri Oct 19 2001.
 *  Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "bless.h"
#include "bless_private.h"

int BLCopyFileFromCFData(BLContextPtr context, const CFDataRef data,
						 const char * dest, int shouldPreallocate) {
	
    int fdw;
    CFDataRef theData = data;
    ssize_t byteswritten;
	
    fstore_t preall;
    int err = 0;
	
    fdw = open(dest, O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if(fdw == -1) {
        contextprintf(context, kBLLogLevelError,  "Error opening %s for writing\n", dest );
        return 2;
    } else {
        contextprintf(context, kBLLogLevelVerbose,  "Opened dest at %s for writing\n", dest );
    }
	
    if(shouldPreallocate) {
		preall.fst_length = CFDataGetLength(theData);
		preall.fst_offset = 0;
		preall.fst_flags = F_ALLOCATECONTIG;
		preall.fst_posmode = F_PEOFPOSMODE;
		
		err = fcntl(fdw, F_PREALLOCATE, &preall);
		if(err == -1 && errno == ENOTSUP) {
			contextprintf(context, kBLLogLevelVerbose,  "preallocation not supported on this filesystem for %s\n", dest );
		} else if(err == -1) {
			contextprintf(context, kBLLogLevelError,  "preallocation of %s failed\n", dest );
			close(fdw);
			return 3;
		} else {
			contextprintf(context, kBLLogLevelVerbose,  "0x%08X bytes preallocated for %s\n", (unsigned int)preall.fst_bytesalloc, dest );
		}
    } else {
		contextprintf(context, kBLLogLevelVerbose,  "No preallocation attempted for %s\n", dest );
    }
	
    byteswritten = write(fdw, (char *)CFDataGetBytePtr(theData), CFDataGetLength(theData));
    if(byteswritten != CFDataGetLength(theData)) {
		contextprintf(context, kBLLogLevelError,  "Error while writing to %s: %s\n", dest, strerror(errno) );
		contextprintf(context, kBLLogLevelError,  "%ld bytes written\n", byteswritten );
		close(fdw);
		return 2;
    }
    contextprintf(context, kBLLogLevelVerbose,  "\n" );
	
    close(fdw);
	
    return 0;
}

