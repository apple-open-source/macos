/*
 *  getOFInfo.c
 *  bless
 *
 *  Created by ssen on Thu Apr 19 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>

#import <mach/mach_error.h>

#import <IOKit/IOKitLib.h>
#import <IOKit/IOBSD.h>
#import <IOKit/storage/IOMedia.h>
#import <IOKit/storage/IOPartitionScheme.h>

#import <CoreFoundation/CoreFoundation.h>

#include "bless.h"

static int getPNameAndPType(unsigned char target[],
			    unsigned char pname[],
			    unsigned char ptype[]);

static int getExternalBooter(unsigned long pNum,
			     unsigned char parentDev[],
			     unsigned long * extpNum);


int getOFInfo(unsigned char mountpoint[], char ofstring[]) {

    int err;
    struct stat sb;

    kern_return_t           kret;
    mach_port_t             ourIOKitPort;
    io_iterator_t           services;
    io_object_t             obj;
    unsigned                length;
    io_name_t               name;
    unsigned long           pNum;
    unsigned long           extpNum;

    unsigned char           mntfrm[MAXPATHLEN];
    unsigned char           applePmapName[32];
    unsigned char           OFDev[1024];
    unsigned char           parentDev[MAXPATHLEN];


    if(err = stat(mountpoint, &sb)) {
      errorprintf("Can't stat mount point %s\n", mountpoint);
      return 1;
    }

    snprintf(mntfrm, MAXPATHLEN, "/dev/%s", devname(sb.st_dev, S_IFBLK));

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
							  devname(sb.st_dev, S_IFBLK)),
					&services);
    if (kret != KERN_SUCCESS) {
      return 3;
    }

    // Should only be one IOKit object for this volume. (And we only want one.)
    obj = IOIteratorNext(services);
    if (!obj) {
      return 4;
    }

    length = sizeof(name);
    name[0] = 0;
    kret = IORegistryEntryGetProperty(obj, kIOMediaContent, name, &length);
    if (kret == KERN_SUCCESS && name && name[0]) {
      strcpy(applePmapName, name);
    } else {
      return 5;
    }

    if(err = getOFDev(obj, &pNum, parentDev, OFDev)) {
      return 6;
    }

    IOObjectRelease(obj);
    obj = NULL;
    IOObjectRelease(services);
    services = NULL;


    if(err = getExternalBooter(pNum, parentDev, &extpNum)) {
      return 10;
    }

    if (extpNum) {
      char *split = OFDev;
      strsep(&split, ":");
      sprintf(ofstring, "%s:%ld", split, extpNum);
    } else {
      strcpy(ofstring, OFDev);
    }
    return 0;
}



int getPNameAndPType(unsigned char target[],
		     unsigned char pname[],
		     unsigned char ptype[]) {

  kern_return_t       status;
  mach_port_t         ourIOKitPort;
  io_iterator_t       services;
  io_registry_entry_t obj;
  unsigned            length;
  char                name[128];

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
							  target+5),
					&services);
  if (status != KERN_SUCCESS) {
    return 2;
  }

  // Should only be one IOKit object for this volume. (And we only want one.)
  obj = IOIteratorNext(services);
  if (!obj) {
    return 3;
  }

  name[0] = 0;
  status = IORegistryEntryGetName(obj, name);     // media/partition name
  if (status != KERN_SUCCESS) {
    return 4;
  }

  strcpy(pname, name);
  
  length = sizeof(name);
  name[0] = 0;

  status = IORegistryEntryGetProperty(obj, kIOMediaContent, name, &length);
  if (status != KERN_SUCCESS) {
    return 5;
  }

  strcpy(ptype, name);
  
  IOObjectRelease(obj);
  obj = NULL;

  IOObjectRelease(services);
  services = NULL;

  return 0;

}


int getExternalBooter(unsigned long pNum,
		      unsigned char parentDev[],
		      unsigned long * extpNum) {

  unsigned char target[MAXPATHLEN];
  unsigned char targetPName[MAXPATHLEN];
  unsigned char targetPType[MAXPATHLEN];
  
  int isNewWorld = getNewWorld();
  
  *extpNum = pNum - 1;
  
  
  if(pNum < 3) {
    return 0;
  }
  
  snprintf(target, MAXPATHLEN, "%ss%d", parentDev, *extpNum);
  // inspect partition N-1.
  // if the partition type is Apple_Boot, and the name is "eXternal booter",
  //    then it's a combined booter
  // if the partition type is Apple_Loader, then look at N-2 if we're new world,
  //    else make an object for oldWorld
  
  getPNameAndPType(target, targetPName, targetPType);

  if(!strcmp(targetPType, "Apple_Boot")
     && !strcmp(targetPName, "eXternal booter")) {
    return 0;
  } else if(!strcmp(targetPType, "Apple_Loader")
		&& !strcmp(targetPName, "SecondaryLoader")) {
    if(!isNewWorld) {
      return 0;
    } else {
      *extpNum = pNum - 2;
      snprintf(target, MAXPATHLEN, "%ss%d", parentDev, *extpNum);
      getPNameAndPType(target, targetPName, targetPType);
      
      if(!strcmp(targetPType, "Apple_Boot")
	 && !strcmp(targetPName, "MOSX_OF3_Booter")) {
	
	return 0;
      }
    }
  }
  
  *extpNum = 0;
  return 0;
}
