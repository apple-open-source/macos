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
 *  BLCopyFileFromCFData.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Fri Oct 19 2001.
 *  Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLCopyFileFromCFData.c,v 1.13 2005/02/03 00:42:27 ssen Exp $
 *
 *  $Log: BLCopyFileFromCFData.c,v $
 *  Revision 1.13  2005/02/03 00:42:27  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.12  2004/04/20 21:40:44  ssen
 *  Update copyrights to 2004
 *
 *  Revision 1.11  2004/03/17 01:38:19  ssen
 *  Don't cast fcntl(2) argument to int, since it's a pointer
 *
 *  Revision 1.10  2003/10/17 00:10:39  ssen
 *  add more const
 *
 *  Revision 1.9  2003/07/22 15:58:34  ssen
 *  APSL 2.0
 *
 *  Revision 1.8  2003/05/20 14:43:58  ssen
 *  pass isHFS status explicitly to avoid trying to preallocate on UFS
 *
 *  Revision 1.7  2003/04/19 00:11:12  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.6  2003/04/16 23:57:33  ssen
 *  Update Copyrights
 *
 *  Revision 1.5  2003/03/19 20:27:56  ssen
 *  #include <CF/CF.h> and use full CFData/CFDictionary pointers instead of
 *  void *. Eww, what in the world was I thinking.
 *
 *  Revision 1.4  2002/06/11 00:50:49  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
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
#include "bless_private.h"


int BLCopyFileFromCFData(BLContextPtr context, const CFDataRef data,
	     const unsigned char dest[], int shouldPreallocate) {

    int fdw;
    CFDataRef theData = data;
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

    if(shouldPreallocate) {
	preall.fst_length = CFDataGetLength(theData);
	preall.fst_offset = 0;
	preall.fst_flags = F_ALLOCATECONTIG;
	preall.fst_posmode = F_PEOFPOSMODE;
    
	err = fcntl(fdw, F_PREALLOCATE, &preall);
	if(err != 0) {
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
            contextprintf(context, kBLLogLevelError,  "%d bytes written\n", byteswritten );
            close(fdw);
            return 2;
    }
    contextprintf(context, kBLLogLevelVerbose,  "\n" );

    close(fdw);

    return 0;
}

