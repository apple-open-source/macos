/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  BLCopyFileFromCFData.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Fri Oct 19 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLCopyFileFromCFData.c,v 1.3 2002/04/27 17:55:00 ssen Exp $
 *
 *  $Log: BLCopyFileFromCFData.c,v $
 *  Revision 1.3  2002/04/27 17:55:00  ssen
 *  Rewrite output logic to format the string before sending of to logger
 *
 *  Revision 1.2  2002/04/25 07:27:29  ssen
 *  Go back to using errorprint and verboseprint inside library
 *
 *  Revision 1.1  2002/03/04 23:10:11  ssen
 *  Add helpers to write CFData out to a file
 *
 *
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


int BLCopyFileFromCFData(BLContext context, void *data,
	     unsigned char dest[]) {

    int fdw;
    CFDataRef theData = (CFDataRef)data;
    int byteswritten;

    fstore_t preall;
    int err = 0;

       
    
    fdw = open(dest, O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if(fdw == -1) {
        contextprintf(context, kBLLogLevelError,  "Error opening %s for writing\n", dest );
        return 2;
    } else {
        contextprintf(context, kBLLogLevelVerbose,  "Opened dest at %s for writing\n", dest );
    }

    preall.fst_length = CFDataGetLength(theData);
    preall.fst_offset = 0;
    preall.fst_flags = F_ALLOCATECONTIG;
    preall.fst_posmode = F_PEOFPOSMODE;

    err = fcntl(fdw, F_PREALLOCATE, (int) &preall);
    if(err != 0) {
      contextprintf(context, kBLLogLevelError,  "preallocation of %s failed\n", dest );
      close(fdw);
      return 3;
    } else {
      contextprintf(context, kBLLogLevelVerbose,  "0x%08X bytes preallocated for %s\n", (unsigned int)preall.fst_bytesalloc, dest );
    }

    byteswritten = write(fdw, (char *)CFDataGetBytePtr(theData), CFDataGetLength(theData));
    if(byteswritten != CFDataGetLength(theData)) {
            contextprintf(context, kBLLogLevelError,  "Error while writing to %s: %s\n", dest, strerror(errno) );
            contextprintf(context, kBLLogLevelError,  "%d bytes written\n", byteswritten );
            close(fdw);
            return 2;
    }
    contextprintf(context, kBLLogLevelVerbose,  "\n" );

    close(fdw);

    return 0;
}

