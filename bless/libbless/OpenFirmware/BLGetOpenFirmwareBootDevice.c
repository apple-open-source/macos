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
 *  BLGetOpenFirmwareBootDevice.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Apr 19 2001.
 *  Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLGetOpenFirmwareBootDevice.c,v 1.14 2003/07/25 01:16:25 ssen Exp $
 *
 *  $Log: BLGetOpenFirmwareBootDevice.c,v $
 *  Revision 1.14  2003/07/25 01:16:25  ssen
 *  When mapping OF -> device, if we found an Apple_Boot, try to
 *  find the corresponding partition that is the real root filesystem
 *
 *  Revision 1.13  2003/07/22 15:58:36  ssen
 *  APSL 2.0
 *
 *  Revision 1.12  2003/05/19 02:17:00  ssen
 *  don't look for booters if an Apple_Boot is specified
 *
 *  Revision 1.11  2003/04/23 00:08:03  ssen
 *  Use blostype2string for OSTypes
 *
 *  Revision 1.10  2003/04/19 00:11:14  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.9  2003/04/16 23:57:35  ssen
 *  Update Copyrights
 *
 *  Revision 1.8  2002/09/24 21:05:46  ssen
 *  Eliminate use of deprecated constants
 *
 *  Revision 1.7  2002/08/22 00:38:42  ssen
 *  Gah. Search for ",\\:tbxi" from the end of the OF path
 *  instead of the beginning. For SCSI cards that use commas
 *  in the OF path, the search was causing a mis-parse.
 *
 *  Revision 1.6  2002/06/11 00:50:51  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
 *  Revision 1.5  2002/04/27 17:55:00  ssen
 *  Rewrite output logic to format the string before sending of to logger
 *
 *  Revision 1.4  2002/04/25 07:27:30  ssen
 *  Go back to using errorprint and verboseprint inside library
 *
 *  Revision 1.3  2002/02/23 04:13:06  ssen
 *  Update to context-based API
 *
 *  Revision 1.2  2002/02/03 19:20:23  ssen
 *  look for external booter
 *
 *  Revision 1.1  2001/11/16 05:36:47  ssen
 *  Add libbless files
 *
 *  Revision 1.10  2001/11/11 06:20:59  ssen
 *  readding files
 *
 *  Revision 1.8  2001/10/26 04:19:41  ssen
 *  Add dollar Id and dollar Log
 *
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/param.h>

#import <mach/mach_error.h>

#import <IOKit/IOKitLib.h>
#import <IOKit/IOBSD.h>
#import <IOKit/storage/IOMedia.h>
#import <IOKit/storage/IOMediaBSDClient.h>
#import <IOKit/storage/IOPartitionScheme.h>

#import <CoreFoundation/CoreFoundation.h>

#include "bless.h"
#include "bless_private.h"

int getPNameAndPType(BLContextPtr context,
                            unsigned char target[],
			    unsigned char pname[],
			    unsigned char ptype[]);

static int getExternalBooter(BLContextPtr context,
                             unsigned long pNum,
			     unsigned char parentDev[],
			     unsigned long * extpNum);


/*
 * Get OF string for device
 * If it's a non-whole device
 *   Look for an external booter
 *     If there is, replace the partition number in OF string
 * For new world, add a tbxi
 */

int BLGetOpenFirmwareBootDevice(BLContextPtr context, unsigned char mntfrm[], char ofstring[]) {

    int err;

    kern_return_t           kret;
    mach_port_t             ourIOKitPort;
    io_iterator_t           services;
    io_object_t             obj;

    unsigned long extPnum = 0;
    dk_firmware_path_t      OFDev;
    int                     devfd;
    unsigned char           parentDev[MAXPATHLEN];
    unsigned char           rawDev[MAXPATHLEN];
    unsigned long           pNum;

    int isNewWorld = BLIsNewWorld(context);
    int isWholeDevice = 0;

    sprintf(rawDev, "/dev/r%s", mntfrm + 5);

    devfd = open(rawDev, O_RDONLY, 0);
    if(devfd < 0) return 1;

    err = ioctl(devfd, DKIOCGETFIRMWAREPATH, &OFDev);
    if(err) {
      contextprintf(context, kBLLogLevelError,  "Error getting OF path: %s\n", strerror(errno) );
      close(devfd);
      return 2;
    }

    close(devfd);

    /*      get

	    OpenFirmwareDevice = "/pci@f2000000/pci-bridge@D/mac-io@7/ata-4@1f000/@0:9";
	    PartitionMapDeviceNode = /dev/disk0;
	    PartitionNumber = 15;
	    PartitionType = Apple_HFS;

	    from IOKit...
    */

    // Obtain the I/O Kit communication handle.
    if((kret = IOMasterPort(bootstrap_port, &ourIOKitPort)) != KERN_SUCCESS) {
      return 2;
    }

    kret = IOServiceGetMatchingServices(ourIOKitPort,
					IOBSDNameMatching(ourIOKitPort,
							  0,
							  (unsigned char *)mntfrm + 5),
					&services);
    if (kret != KERN_SUCCESS) {
      return 3;
    }

    // Should only be one IOKit object for this volume. (And we only want one.)
    obj = IOIteratorNext(services);
    if (!obj) {
      return 4;
    }

    {
      CFTypeRef isWhole = NULL;

      isWhole = IORegistryEntryCreateCFProperty( obj, CFSTR(kIOMediaWholeKey),
						 kCFAllocatorDefault, 0);

      if(CFGetTypeID(isWhole) !=CFBooleanGetTypeID()) {
	contextprintf(context, kBLLogLevelError,  "Wrong type of IOKit entry for kIOMediaWholeKey\n" );
	CFRelease(isWhole);
	IOObjectRelease(obj);
	obj = NULL;
	IOObjectRelease(services);
	services = NULL;
	return 3;
      }

      isWholeDevice = (isWhole == kCFBooleanTrue) ;
      CFRelease(isWhole);
		contextprintf(context, kBLLogLevelVerbose,  "Device is whole: %d\n", isWholeDevice );
    }

    for(;;) {
      CFTypeRef value = NULL;

      if(isWholeDevice) {
	break;
      }

      // Okay, it's partitioned. There might be helper partitions, though...


      value = IORegistryEntryCreateCFProperty(obj, CFSTR(kIOMediaContentKey),
					      kCFAllocatorDefault, 0);

      if(CFGetTypeID(value) != CFStringGetTypeID()) {
	contextprintf(context, kBLLogLevelError,  "Wrong type of IOKit entry for kIOMediaContentKey\n" );
	if(value) CFRelease(value);
	IOObjectRelease(obj);
	obj = NULL;
	IOObjectRelease(services);
	services = NULL;
	return 4;
	}


      if(CFStringCompare((CFStringRef)value, CFSTR("Apple_HFS"), 0)
	 == kCFCompareEqualTo) {
		contextprintf(context, kBLLogLevelVerbose,  "Apple_HFS partition. No external loader\n" );
	// it's an HFS partition. no loader needed
	CFRelease(value);
	break;
      }

      if(CFStringCompare((CFStringRef)value, CFSTR("Apple_Boot"), 0) == kCFCompareEqualTo
	 || CFStringCompare((CFStringRef)value, CFSTR("Apple_Loader"), 0) == kCFCompareEqualTo) {
	  contextprintf(context, kBLLogLevelVerbose,  "Apple_Boot or Apple_Loader partition is an external loader\n" );
	  // it's an loader itself
	  CFRelease(value);
	  break;
      }
      
      CFRelease(value);

      contextprintf(context, kBLLogLevelVerbose,  "NOT Apple_HFS partition. Looking for external loader\n" );

      if(BLGetParentDevice(context, mntfrm, parentDev, &pNum)) {
	return 6;
      }

      IOObjectRelease(obj);
      obj = NULL;
      IOObjectRelease(services);
      services = NULL;
      
	err = getExternalBooter(context, pNum, parentDev, &extPnum);
      if(err) {
	return 10;
      }

      break;
    }

    if (extPnum) {
      char *split = OFDev.path;
      split = strsep(&split, ":");
      sprintf(ofstring, "%s:%ld", split, extPnum);
    } else {
      strcpy(ofstring, OFDev.path);
    }
	
    if (isNewWorld) {
	char tbxi[5];

	strcat(ofstring, ",\\\\:");
	strcat(ofstring, blostype2string(kBL_OSTYPE_PPC_TYPE_BOOTX, tbxi));
    }

    return 0;
}



int getPNameAndPType(BLContextPtr context,
                     unsigned char target[],
		     unsigned char pname[],
		     unsigned char ptype[]) {

  kern_return_t       status;
  mach_port_t         ourIOKitPort;
  io_iterator_t       services;
  io_registry_entry_t obj;
    CFStringRef temp = NULL;

  pname[0] = '\0';
  ptype[0] = '\0';

  // Obtain the I/O Kit communication handle.
  status = IOMasterPort(bootstrap_port, &ourIOKitPort);
  if (status != KERN_SUCCESS) {
    return 1;
  }

  // Obtain our list of one object
  // +5 to skip over "/dev/"
  status = IOServiceGetMatchingServices(ourIOKitPort,
					IOBSDNameMatching(ourIOKitPort,
							  0,
							  target),
					&services);
  if (status != KERN_SUCCESS) {
    return 2;
  }

  // Should only be one IOKit object for this volume. (And we only want one.)
  obj = IOIteratorNext(services);
  if (!obj) {
    return 3;
  }

    temp = (CFStringRef)IORegistryEntryCreateCFProperty(
	obj,
	CFSTR(kIOMediaContentKey),
        kCFAllocatorDefault,
	0);

  if (temp == NULL) {
    return 4;
  }

    if(!CFStringGetCString(temp, ptype, MAXPATHLEN, kCFStringEncodingMacRoman)) {
        CFRelease(temp);
        return 4;
    }

    CFRelease(temp);
    
    status = IORegistryEntryGetName(obj,pname);
  if (status != KERN_SUCCESS) {
    return 5;
  }


  
  IOObjectRelease(obj);
  obj = NULL;

  IOObjectRelease(services);
  services = NULL;

    contextprintf(context, kBLLogLevelVerbose,  "looking at partition %s, type %s, name %s\n", target, ptype, pname );

  return 0;

}

static
int getExternalBooter(BLContextPtr context,
                      unsigned long pNum,
		      unsigned char parentDev[],
		      unsigned long * extpNum) {

  unsigned char target[MAXPATHLEN];
  unsigned char targetPName[MAXPATHLEN];
  unsigned char targetPType[MAXPATHLEN];
  
  int isNewWorld = BLIsNewWorld(context);
  
  *extpNum = pNum - 1;
  
    
  snprintf(target, MAXPATHLEN, "%ss%ld", parentDev+5, *extpNum);
  // inspect partition N-1.
  // if the partition type is Apple_Boot, and the name is "eXternal booter",
  //    then it's a combined booter
  // if the partition type is Apple_Loader, then look at N-2 if we're new world,
  //    else make an object for oldWorld
  
  getPNameAndPType(context, target, targetPName, targetPType);

  if(!strcmp(targetPType, "Apple_Boot")
     && !strcmp(targetPName, "eXternal booter")) {
    return 0;
  } else if(!strcmp(targetPType, "Apple_Loader")
		&& !strcmp(targetPName, "SecondaryLoader")) {
    if(!isNewWorld) {
      return 0;
    } else {
      *extpNum = pNum - 2;
      snprintf(target, MAXPATHLEN, "%ss%ld", parentDev+5, *extpNum);
      getPNameAndPType(context, target, targetPName, targetPType);
      
      if(!strcmp(targetPType, "Apple_Boot")
	 && !strcmp(targetPName, "MOSX_OF3_Booter")) {
	
	return 0;
      }
    }
  }
  
  *extpNum = 0;
  return 1;
}
