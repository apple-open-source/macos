/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
#include <libc.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

int main(int argc, char *argv[]) {

  char *path;
  kern_return_t ret;
  io_registry_entry_t entry = NULL;
  CFMutableDictionaryRef props = NULL;

  if(argc != 2) {
    fprintf(stderr, "Usage: %s ofpath\n", getprogname());
    fprintf(stderr, "\nFor example: %s IODeviceTree:/openprom\n", getprogname());
    exit(1);
  }

  path = argv[1];

  entry = IORegistryEntryFromPath(kIOMasterPortDefault, path);

  printf("entry is %p\n", entry);

  if(entry == NULL) exit(1);


  ret = IORegistryEntryCreateCFProperties(entry, &props,
					  kCFAllocatorDefault, 0);


  if(ret) {
    fprintf(stderr, "Could not get entry properties\n");
    exit(1);
  }

  TAOCFPrettyPrint(props);

  CFRelease(props);
 IOObjectRelease(entry); 

  return 0;
}
