/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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
#include <libc.h>
#include <DiskArbitration/DiskArbitration.h>
#include <DiskArbitration/DiskArbitrationPrivate.h>
#include <err.h>
#include "UtilitiesCFPrettyPrint.h"

// cc -o testdadisk testdadisk.c UtilitiesCFPrettyPrint.c -framework CoreFoundation -framework DiskArbitration


int main(int argc, char *argv[]) {
  
  char *dev = NULL;
  DADiskRef disk = NULL;
  DASessionRef session = NULL;
  CFDictionaryRef props = NULL;


  if(argc != 2) {
    fprintf(stderr, "Usage: %s disk1s2\n", getprogname());
    exit(1);
  }

  dev = argv[1];

  session = DASessionCreate(kCFAllocatorDefault);
  if(session == NULL)
    errx(1, "DASessionCreate");

  if (1) {
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
							   dev,
							   strlen(dev),
							   false);
    disk = DADiskCreateFromVolumePath(kCFAllocatorDefault, session, url);

    CFRelease(url);
  } else {

    disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, dev);
  }
  if(disk == NULL) {
    CFRelease(session);
    errx(1, "DADiskCreateFromBSDName");
  }

  props = DADiskCopyDescription(disk);
  if(props == NULL) {
    CFRelease(session);
    CFRelease(disk);
    errx(1, "DADiskCopyDescription");
  }

  TAOCFPrettyPrint(disk);
  TAOCFPrettyPrint(props);

  printf("Options: %lu\n", DADiskGetOptions(disk));

  CFRelease(session);
  CFRelease(disk);
  CFRelease(props);

  return 0;
}
