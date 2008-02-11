/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

int main(int argc, char *argv[]) {

  char *path;
  kern_return_t ret;
  io_registry_entry_t entry = 0;
  io_string_t iopath;

  if(argc != 2) {
    fprintf(stderr, "Usage: %s disk1s1\n", getprogname());
    exit(1);
  }

  path = argv[1];

  //  entry = IORegistryEntryFromPath(kIOMasterPortDefault, path);
  entry = IOServiceGetMatchingService(kIOMasterPortDefault,
				      IOBSDNameMatching(kIOMasterPortDefault, 0, path));

  printf("entry is %p\n", entry);

  if(entry == 0) exit(1);


  ret = IORegistryEntryGetPath(entry, kIOServicePlane, iopath);
  if(ret) {
    fprintf(stderr, "Could not get entry path\n");
    exit(1);
  }
  printf("%s path: %s\n", kIOServicePlane, iopath);

  ret = IORegistryEntryGetPath(entry, kIODeviceTreePlane, iopath);
  if(ret) {
    fprintf(stderr, "Could not get entry path\n");
    exit(1);
  }
  printf("%s path: %s\n", kIODeviceTreePlane, iopath);


 IOObjectRelease(entry); 

  return 0;
}
