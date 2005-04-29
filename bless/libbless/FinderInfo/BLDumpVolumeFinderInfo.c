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
 *  BLDumpVolumeFinderInfo.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Apr 19 2001.
 *  Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLDumpVolumeFinderInfo.c,v 1.21 2005/02/26 09:03:05 ssen Exp $
 *
 *  $Log: BLDumpVolumeFinderInfo.c,v $
 *  Revision 1.21  2005/02/26 09:03:05  ssen
 *  compile with gcc 4.0. Turn of signed pointer warnings, and
 *  use packed alignment
 *
 *  Revision 1.20  2005/02/03 00:42:24  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.19  2004/09/21 16:44:51  ssen
 *  delete comment
 *
 *  Revision 1.18  2004/09/20 20:59:57  ssen
 *  <rdar://problem/3808101> bless (open source) shouldn't use CoreServices.h
 *
 *  Revision 1.17  2004/04/20 21:40:41  ssen
 *  Update copyrights to 2004
 *
 *  Revision 1.16  2003/10/16 23:50:05  ssen
 *  Partially finish cleanup of headers to add "const" to char[] arguments
 *  that won't be modified.
 *
 *  Revision 1.15  2003/07/22 15:58:30  ssen
 *  APSL 2.0
 *
 *  Revision 1.14  2003/04/23 00:02:17  ssen
 *  Add a "Relative Path" key to plist output for blessed dir relative to mountpoint
 *
 *  Revision 1.13  2003/04/19 00:11:05  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.12  2003/04/16 23:57:30  ssen
 *  Update Copyrights
 *
 *  Revision 1.11  2003/03/24 19:03:48  ssen
 *  Use the BootBlock structure to get pertinent info form boot blocks
 *
 *  Revision 1.10  2003/03/20 05:06:16  ssen
 *  remove some more non-c99 types
 *
 *  Revision 1.9  2003/03/20 03:40:53  ssen
 *  Merge in from PR-3202649
 *
 *  Revision 1.8.2.1  2003/03/20 02:34:21  ssen
 *  don't use bit shifts for VSDB, just interpret last two words as a 64-bit int in host order (already swapped)
 *
 *  Revision 1.8  2003/03/19 22:56:58  ssen
 *  C99 types
 *
 *  Revision 1.6  2002/06/11 00:50:39  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
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

#include "bless.h"
#include "bless_private.h"

struct BootBlocks {
  short   id;
  long    entryPoint;
  short   version;
  short   pageFlags;
  Str15   system;
  Str15   shellApplication;
  Str15   debugger1;
  Str15   debugger2;
  Str15   startupScreen;
  Str15   startupApplication;
  char    otherStuff[1024 - (2+4+2+2+16+16+16+16+16+16)];
} __attribute__((packed));

typedef struct BootBlocks BootBlocks;

/*
 * 1. getattrlist on the mountpoint to get the volume id
 * 2. read in the finder words
 * 3. for the directories we're interested in, get the entries in /.vol
 */
int BLCreateVolumeInformationDictionary(BLContextPtr context, const unsigned char mount[],
					CFDictionaryRef *outDict) {
    uint32_t finderinfo[8];
    int err;
    uint32_t i;
    uint32_t dirID;
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

      if(strlen(blesspath) == 0 || 0 == strcmp(mount, "/")) {
	  val = CFStringCreateWithCString(kCFAllocatorDefault, blesspath, kCFStringEncodingUTF8);
      } else {
	  val = CFStringCreateWithCString(kCFAllocatorDefault, blesspath+strlen(mount), kCFStringEncodingUTF8);
      }
      CFDictionaryAddValue(word, CFSTR("Relative Path"), val);
      CFRelease(val); val = NULL;
      
      CFArrayAppendValue(infarray, word);
      CFRelease(word); word = NULL;
    }

    CFDictionaryAddValue(dict, CFSTR("Finder Info"),
			 infarray);

    CFRelease(infarray); infarray = NULL;

    {
        CFNumberRef vsdbref = NULL;
        uint64_t vsdb;
        vsdb = (*(uint64_t *)&finderinfo[8-2]);
        
        vsdbref = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &vsdb);
        CFDictionaryAddValue(dict, CFSTR("VSDB ID"), vsdbref);
        CFRelease(vsdbref); vsdbref = NULL;
    }

    
    {
        CFTypeRef val2;

      fbootstraptransfer_t        bbr;
      int                         fd;
      unsigned char                       bbPtr[kBootBlocksSize];
      
      CFMutableDictionaryRef bdict =
	CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDataRef bdata = NULL;

      fd = open(mount, O_RDONLY);
      if (fd == -1) {
	contextprintf(context, kBLLogLevelError,  "Can't open volume mount point for %s\n", mount );
	return 2;
      }
  
      bbr.fbt_offset = 0;
      bbr.fbt_length = kBootBlocksSize;
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

      if((uint16_t)CFSwapInt16BigToHost(*(uint16_t *)&bbPtr[0]) == kBootBlockTradOSSig) {
	  BootBlocks		*bb = (BootBlocks *)bbPtr;
    
	    val2 = CFStringCreateWithPascalString(kCFAllocatorDefault, bb->system, kCFStringEncodingMacRoman);
	    CFDictionaryAddValue(bdict, CFSTR("System"), val2);
	    CFRelease(val2); val2 = NULL;
    
	    val2 = CFStringCreateWithPascalString(kCFAllocatorDefault, bb->shellApplication, kCFStringEncodingMacRoman);
	    CFDictionaryAddValue(bdict, CFSTR("ShellApplication"), val2);
	    CFRelease(val2); val2 = NULL;
    
	    val2 = CFStringCreateWithPascalString(kCFAllocatorDefault, bb->debugger1, kCFStringEncodingMacRoman);
	    CFDictionaryAddValue(bdict, CFSTR("Debugger1"), val2);
	    CFRelease(val2); val2 = NULL;
    
	    val2 = CFStringCreateWithPascalString(kCFAllocatorDefault, bb->debugger2, kCFStringEncodingMacRoman);
	    CFDictionaryAddValue(bdict, CFSTR("Debugger2"), val2);
	    CFRelease(val2); val2 = NULL;
    
	    val2 = CFStringCreateWithPascalString(kCFAllocatorDefault, bb->startupScreen,  kCFStringEncodingMacRoman);
	    CFDictionaryAddValue(bdict, CFSTR("StartupScreen"), val2);
	    CFRelease(val2); val2 = NULL;
    
	    val2 = CFStringCreateWithPascalString(kCFAllocatorDefault, bb->startupApplication, kCFStringEncodingMacRoman);
	    CFDictionaryAddValue(bdict, CFSTR("StartupApplication"), val2);
	    CFRelease(val2); val2 = NULL;
    
      }

      bdata = CFDataCreate(kCFAllocatorDefault, bbPtr, 1024);
      CFDictionaryAddValue(bdict, CFSTR("Data"), bdata);
      CFRelease(bdata); bdata = NULL;

      CFDictionaryAddValue(dict, CFSTR("BootBlocks"),
			   bdict);
      CFRelease(bdict); bdict = NULL;
    }
    
    *outDict = dict;
    return 0;
}
