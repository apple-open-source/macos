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
 *  BLDumpVolumeFinderInfo.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Apr 19 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLDumpVolumeFinderInfo.c,v 1.5 2002/04/27 17:54:58 ssen Exp $
 *
 *  $Log: BLDumpVolumeFinderInfo.c,v $
 *  Revision 1.5  2002/04/27 17:54:58  ssen
 *  Rewrite output logic to format the string before sending of to logger
 *
 *  Revision 1.4  2002/04/25 07:27:26  ssen
 *  Go back to using errorprint and verboseprint inside library
 *
 *  Revision 1.3  2002/02/23 04:13:05  ssen
 *  Update to context-based API
 *
 *  Revision 1.2  2002/02/03 18:32:40  ssen
 *  print VSDB
 *
 *  Revision 1.1  2001/11/16 05:36:46  ssen
 *  Add libbless files
 *
 *  Revision 1.7  2001/10/26 04:19:41  ssen
 *  Add dollar Id and dollar Log
 *
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/param.h>

#include <CoreFoundation/CoreFoundation.h>

#include "dumpFI.h"
#include "bless.h"


/*
 * 1. getattrlist on the mountpoint to get the volume id
 * 2. read in the finder words
 * 3. for the directories we're interested in, get the entries in /.vol
 */
int BLCreateVolumeInformationDictionary(BLContext context, unsigned char mount[], void /* CFDictionaryRef */ **outDict) {
    u_int32_t finderinfo[8];
    int err;
    u_int32_t i;
    u_int32_t dirID;
    CFMutableDictionaryRef dict = NULL;
    CFMutableArrayRef infarray = NULL;

    unsigned char blesspath[MAXPATHLEN];

    err = BLGetVolumeFinderInfo(context, mount, finderinfo);
    if(err) {
        return 1;
    }

    infarray = CFArrayCreateMutable(kCFAllocatorDefault,
				    8,
				    &kCFTypeArrayCallBacks);

    dict =  CFDictionaryCreateMutable(kCFAllocatorDefault, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    for(i = 0; i< 8-2; i++) {
      CFMutableDictionaryRef word =
	CFDictionaryCreateMutable(kCFAllocatorDefault,6, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
      CFTypeRef val;
      
      dirID = finderinfo[i];
      blesspath[0] = '\0';
      
      err = BLLookupFileIDOnMount(context, mount, dirID, blesspath);
      
      val = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &dirID);
      CFDictionaryAddValue(word, CFSTR("Directory ID"), val);
      CFRelease(val); val = NULL;
      
      val = CFStringCreateWithCString(kCFAllocatorDefault, blesspath, kCFStringEncodingUTF8);
      CFDictionaryAddValue(word, CFSTR("Path"), val);
      CFRelease(val); val = NULL;
      
      CFArrayAppendValue(infarray, word);
      CFRelease(word); word = NULL;
    }

    CFDictionaryAddValue(dict, CFSTR("Finder Info"),
			 infarray);

    CFRelease(infarray); infarray = NULL;

    {
        CFNumberRef vsdbref = NULL;
        u_int64_t vsdb;
        vsdb = ((u_int64_t)finderinfo[8-2] << 32) | (finderinfo[8-1]);
        
        vsdbref = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &vsdb);
        CFDictionaryAddValue(dict, CFSTR("VSDB ID"), vsdbref);
        CFRelease(vsdbref); vsdbref = NULL;
    }

    
    {
        CFTypeRef val2;

      fbootstraptransfer_t        bbr;
      int                         fd;
      unsigned char                       bbPtr[1024];
  
      unsigned char    system[16];
      unsigned char    shellApplication[16];
      unsigned char    debugger1[16];
      unsigned char    debugger2[16];
      unsigned char    startupScreen[16];
      unsigned char    startupApplication[16];

      CFMutableDictionaryRef bdict =
	CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDataRef bdata = NULL;

      fd = open(mount, O_RDONLY);
      if (fd == -1) {
	contextprintf(context, kBLLogLevelError,  "Can't open volume mount point for %s\n", mount );
	return 2;
      }
  
      bbr.fbt_offset = 0;
      bbr.fbt_length = 1024;
      bbr.fbt_buffer = bbPtr;
  
      err = fcntl(fd, F_READBOOTSTRAP, &bbr);
      if (err) {
        contextprintf(context, kBLLogLevelError,  "Can't read boot blocks\n" );
        close(fd);
        return 3;
      } else {
        contextprintf(context, kBLLogLevelVerbose,  "Boot blocks read successfully\n" );
      }
      close(fd);

      //            write(fileno(stdout), bbPtr, 1024);
  

      snprintf(system, 16, "%.*s",  bbPtr[2+4+2+2], &bbPtr[2+4+2+2+1]);
      snprintf(shellApplication, 16, "%.*s",
	       bbPtr[2+4+2+2+16], &bbPtr[2+4+2+2+16+1]);
      snprintf(debugger1, 16, "%.*s",
	       bbPtr[2+4+2+2+16+16], &bbPtr[2+4+2+2+16+16+1]);
      snprintf(debugger2, 16, "%.*s",
	       bbPtr[2+4+2+2+16+16+16], &bbPtr[2+4+2+2+16+16+16+1]);
      snprintf(startupScreen, 16, "%.*s",
	       bbPtr[2+4+2+2+16+16+16+16], &bbPtr[2+4+2+2+16+16+16+16+1]);
      snprintf(startupApplication, 16, "%.*s",
	       bbPtr[2+4+2+2+16+16+16+16+16], &bbPtr[2+4+2+2+16+16+16+16+16+1]);


        val2 = CFStringCreateWithCString(kCFAllocatorDefault, system, kCFStringEncodingUTF8);
        CFDictionaryAddValue(bdict, CFSTR("System"), val2);
        CFRelease(val2); val2 = NULL;

        val2 = CFStringCreateWithCString(kCFAllocatorDefault, shellApplication, kCFStringEncodingUTF8);
        CFDictionaryAddValue(bdict, CFSTR("ShellApplication"), val2);
        CFRelease(val2); val2 = NULL;

        val2 = CFStringCreateWithCString(kCFAllocatorDefault, debugger1, kCFStringEncodingUTF8);
        CFDictionaryAddValue(bdict, CFSTR("Debugger1"), val2);
        CFRelease(val2); val2 = NULL;

        val2 = CFStringCreateWithCString(kCFAllocatorDefault, debugger2, kCFStringEncodingUTF8);
        CFDictionaryAddValue(bdict, CFSTR("Debugger2"), val2);
        CFRelease(val2); val2 = NULL;

        val2 = CFStringCreateWithCString(kCFAllocatorDefault, startupScreen,  kCFStringEncodingUTF8);
        CFDictionaryAddValue(bdict, CFSTR("StartupScreen"), val2);
        CFRelease(val2); val2 = NULL;

        val2 = CFStringCreateWithCString(kCFAllocatorDefault, startupApplication, kCFStringEncodingUTF8);
        CFDictionaryAddValue(bdict, CFSTR("StartupApplication"), val2);
        CFRelease(val2); val2 = NULL;

        bdata = CFDataCreate(kCFAllocatorDefault, bbPtr, 1024);
        CFDictionaryAddValue(bdict, CFSTR("Data"), bdata);
        CFRelease(bdata); bdata = NULL;


        CFDictionaryAddValue(dict, CFSTR("BootBlocks"),
			   bdict);
        CFRelease(bdict); bdict = NULL;
    }
    
    *outDict = (void *)dict;
    return 0;
}
