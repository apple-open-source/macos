/*
 *  getOFDev.c
 *  bless
 *
 *  Created by ssen on Thu Apr 19 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#import <mach/mach_error.h>

#import <IOKit/IOKitLib.h>
#import <IOKit/IOBSD.h>
#import <IOKit/storage/IOMedia.h>
#import <IOKit/storage/IOPartitionScheme.h>


int getOFDev(io_object_t obj, unsigned long * pNum,
	     unsigned char parentDev[],
	     unsigned char OFDev[]) {
  io_iterator_t       parents;
  io_registry_entry_t service;
  io_object_t         loopedObj;
  io_string_t         name;
  unsigned long       savedPartnNum = 0;
  unsigned int       length;
  kern_return_t       status = 0;
  

  unsigned char savedOFDev[1024];

  parentDev[0] = '\0';
  OFDev[0] = '\0';
  savedOFDev[0] = '\0';
  *pNum = 0;

  // get the partition # of the object passed in
  // then get the parent, see if it's an apple pmap, and keep this partn number
  // if the parent isn't an Apple pmap, then get the partn # of that thing, and see if the grandpare\nt
  // is an apple pmap.
  length = sizeof(savedPartnNum);
  savedPartnNum = 0;
  status = IORegistryEntryGetProperty(obj, kIOMediaPartitionID, (void*) &savedPartnNum, &length);
  if (status) {
    savedPartnNum = 0;
  }
  /* get the OF path. We do this here because the OF path only exists as a property of the same      */
  /* object that contains the partition number, that is, only on a child of the Apple partition map.  */
  status = IORegistryEntryGetPath(obj, kIODeviceTreePlane, name);
  if (status == KERN_SUCCESS && name && strlen(name) > 13) {
    memmove(name, &(name[13]), strlen(name) - 12);
    strcpy(savedOFDev, name);
  }


  loopedObj = obj;
  while (1) {
    status = IORegistryEntryGetParentIterator (loopedObj, kIOServicePlane,
					       &parents);
    if (status) {
      return 6;
      /* We'll never loop forever. */
    }

    loopedObj = NULL; // make sure we abort if we don't get a parent
    while ( (service = IOIteratorNext(parents)) != NULL ) {

      loopedObj = service;
      if (IOObjectConformsTo(service, "IOMedia")) {

	// look at the partn type. If it's Apple_partition_scheme, use the last retrieved pnum
	length = sizeof(name);
	name[0] = 0;
	status = IORegistryEntryGetProperty(service, kIOMediaContent, name, &length);

	if (!strncmp(name, "Apple_partition_scheme", strlen("Apple_partition_scheme"))) {
	  // return the last pnum and OF path we saw
	  *pNum = savedPartnNum;
	  strcpy(OFDev, savedOFDev);

	  // get the BSD node for the Apple pmap
	  length = sizeof(name);
	  name[0] = 0;
	  status = IORegistryEntryGetProperty(service, kIOBSDName, name, &length);
	  if (status) {
	    return 7;
	  } else {
	    sprintf(parentDev, "/dev/%s",name);
	    return 0;
	  }

	}

	// get the partition # for the next round the loop
	length = sizeof(savedPartnNum);
	savedPartnNum = 0;
	status = IORegistryEntryGetProperty(service, kIOMediaPartitionID,
					    (void*) &savedPartnNum, &length);
	if (status) {
	  savedPartnNum = 0;
	}

	// get OF path for the next round the loop
	status = IORegistryEntryGetPath(service, kIODeviceTreePlane, name);
	if (status == KERN_SUCCESS && name && strlen(name) > 12) {
	  memmove(name, &(name[13]), strlen(name) - 12);
	  strcpy(savedOFDev, name);
	} else {
	  savedOFDev[0] = '\0';
	}
      }
    }

  }

  *pNum = 0;
  parentDev[0] = '\0';
  OFDev[0] = '\0';
  return 9;

}

// int getOFDev(, char ofPath[], SInt32 *pNum)
