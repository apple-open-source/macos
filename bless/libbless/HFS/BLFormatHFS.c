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
 *  BLFormatHFS.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Jul 05 2001.
 *  Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLFormatHFS.c,v 1.10 2003/07/22 15:58:31 ssen Exp $
 *
 *  $Log: BLFormatHFS.c,v $
 *  Revision 1.10  2003/07/22 15:58:31  ssen
 *  APSL 2.0
 *
 *  Revision 1.9  2003/04/19 00:11:08  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.8  2003/04/16 23:57:31  ssen
 *  Update Copyrights
 *
 *  Revision 1.7  2002/06/11 00:50:42  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
 *  Revision 1.6  2002/04/27 17:54:59  ssen
 *  Rewrite output logic to format the string before sending of to logger
 *
 *  Revision 1.5  2002/04/25 07:27:27  ssen
 *  Go back to using errorprint and verboseprint inside library
 *
 *  Revision 1.4  2002/03/10 23:51:09  ssen
 *  BLFrmatHFS now just takes the number of bytes to reserve
 *
 *  Revision 1.3  2002/02/23 04:13:05  ssen
 *  Update to context-based API
 *
 *  Revision 1.2  2002/02/04 01:34:16  ssen
 *  Add -fsargs option
 *
 *  Revision 1.1  2001/11/16 05:36:46  ssen
 *  Add libbless files
 *
 *  Revision 1.7  2001/10/26 04:19:41  ssen
 *  Add dollar Id and dollar Log
 *
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#include "bless.h"
#include "bless_private.h"

#define SECTORSIZE 512U

int BLFormatHFS(BLContextPtr context, unsigned char devicepath[], off_t bytesLeftFree,
                unsigned char fslabel[], unsigned char fsargs[]) {
    int err;
    unsigned int newsize;
    unsigned char commandline[1024];


    newsize = (bytesLeftFree + SECTORSIZE - 1) / SECTORSIZE;


    if(newsize > 0) {
            snprintf(commandline, 1024, "/sbin/newfs_hfs -w -v \"%s\" -c f=%u %s \"%s\"",
                fslabel, newsize, fsargs, devicepath);
    } else {
            snprintf(commandline, 1024, "/sbin/newfs_hfs -w -v \"%s\" %s \"%s\"",
                fslabel, fsargs, devicepath);	
    }
					            
    contextprintf(context, kBLLogLevelVerbose,  "Beginning `%s'\n", commandline );
    sleep(5);
    err = system(commandline);

   if(err) {
        contextprintf(context, kBLLogLevelError,  "Can't newfs_hfs %s\n", devicepath );
        return 7;
   }


    return 0;
}
