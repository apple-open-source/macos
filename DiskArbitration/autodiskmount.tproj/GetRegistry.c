/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>

#include <mach/mach_init.h>

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/storage/IOMedia.h>

#include "GetRegistry.h"
#include "FSParticular.h"

#include "DiskArbitrationServerMain.h"

/*
-----------------------------------------------------------------------------------------
*/

typedef enum {
	kDiskTypeUnknown = 0x00,
	kDiskTypeHD = 0x01,
	kDiskTypeCD = 0x02,
	kDiskTypeDVD = 0x04
} DiskType;

DiskType        GetDiskType(io_registry_entry_t media);

/*
-----------------------------------------------------------------------------------------
*/

extern mach_port_t ioMasterPort;

static kern_return_t openHIDService(mach_port_t io_master_port, io_connect_t *connection)
{
    kern_return_t  kr;
    mach_port_t ev, service, iter;

    kr = IOServiceGetMatchingServices(io_master_port, IOServiceMatching(kIOHIDSystemClass), &iter);
    if (kr != KERN_SUCCESS) {
        return kr;
    }

    service = IOIteratorNext(iter);
    kr = IOServiceOpen(service, mach_task_self(), kIOHIDParamConnectType, &ev);
    // These objects need to be released whether or not there is an error from IOServiceOpen, so
    // just do it up front.
    IOObjectRelease(service);
    IOObjectRelease(iter);
    if (kr != KERN_SUCCESS) {
        return kr;
    }

    *connection = ev;
    return kr;
}

static void wakeMonitor()
{
    IOGPoint loc;
    kern_return_t kr;
    NXEvent nullEvent = {NX_NULLEVENT, {0, 0}, 0, -1, 0};
    static io_connect_t io_connection = MACH_PORT_NULL;
    static struct timeval last = {0};
    struct timeval current;
    enum { kNULLEventPostThrottle = 10 };

    if (!io_connection) {
        kr = openHIDService(ioMasterPort, &io_connection);
        if (kr != KERN_SUCCESS) {
            return;
        }
    }

    (void) gettimeofday(&current, NULL);

    // If a user event has been posted recently (i.e., within the throttle period) just return without
    // doing anything, because people tend to call this thing a *lot*..
    if ((current.tv_sec - last.tv_sec) < kNULLEventPostThrottle) {
        return;
    }

    // Finally, post a NULL event
    last = current;
    kr = IOHIDPostEvent(io_connection, NX_NULLEVENT, loc, &nullEvent.data, FALSE, 0, FALSE);
    if (kr != KERN_SUCCESS) {
        return;
    }

    return;
}

void 
GetDisksFromRegistry(io_iterator_t iter, int initialRun, int mountExisting)
{
	kern_return_t   kr;
	io_registry_entry_t entry;

	io_name_t       ioMediaName;
	UInt32          ioBSDUnit;
	UInt64          ioSize;
	int             ioWhole, ioWritable, ioEjectable, ioLeaf;
	DiskType        diskType;
	unsigned        flags;
	mach_port_t     masterPort;
	mach_timespec_t timeSpec;


	timeSpec.tv_sec = (initialRun ? 1 : 10);
	timeSpec.tv_nsec = 0;

	IOMasterPort(bootstrap_port, &masterPort);

	//sleep(1);
	IOKitWaitQuiet(masterPort, &timeSpec);

	while ((entry = IOIteratorNext(iter))) {
		char           *ioBSDName = NULL;
		//(needs release)
			char           *ioContent = NULL;
		//(needs release)
			CFBooleanRef    boolean = 0;
		//(don 't release)
		   CFNumberRef number = 0;
		//(don 't release)
		   CFDictionaryRef properties = 0;
		//(needs release)
			CFStringRef     string = 0;
		//(don 't release)

		   // CFDictionaryRef ioMatchingDictionary = NULL;

		io_string_t     ioDeviceTreePath;
		char           *ioDeviceTreePathPtr;

		int             ejectOnLogout = 0;

		//MediaName

			kr = IORegistryEntryGetName(entry, ioMediaName);
		if (KERN_SUCCESS != kr) {
			dwarning(("can't obtain name for media object\n"));
			goto Next;
		}
		//Get Properties

        kr = IORegistryEntryCreateCFProperties(entry, (CFMutableDictionaryRef *)&properties, kCFAllocatorDefault, kNilOptions);
		if (KERN_SUCCESS != kr) {
			dwarning(("can't obtain properties for '%s'\n", ioMediaName));
			goto Next;
		}
		assert(CFGetTypeID(properties) == CFDictionaryGetTypeID());

		//BSDName

			string = (CFStringRef) CFDictionaryGetValue(properties, CFSTR(kIOBSDNameKey));
		if (!string) {
			/* We're only interested in disks accessible via BSD */
			dwarning(("kIOBSDNameKey property missing for '%s'\n", ioMediaName));
			goto Next;
		}
		assert(CFGetTypeID(string) == CFStringGetTypeID());

		ioBSDName = daCreateCStringFromCFString(string);
		assert(ioBSDName);

		dwarning(("ioBSDName = '%s'\t", ioBSDName));

		//BSDUnit

			number = (CFNumberRef) CFDictionaryGetValue(properties, CFSTR(kIOBSDUnitKey));
		if (!number) {
			/* We're only interested in disks accessible via BSD */
			dwarning(("\nkIOBSDUnitKey property missing for '%s'\n", ioBSDName));
			goto Next;
		}
		assert(CFGetTypeID(number) == CFNumberGetTypeID());

		if (!CFNumberGetValue(number, kCFNumberSInt32Type, &ioBSDUnit)) {
			goto Next;
		}
		dwarning(("ioBSDUnit = %ld\t", ioBSDUnit));

		//Content

			string = (CFStringRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaContentKey));
		if (!string) {
			dwarning(("\nkIOMediaContentKey property missing for '%s'\n", ioBSDName));
			goto Next;
		}
		assert(CFGetTypeID(string) == CFStringGetTypeID());

		ioContent = daCreateCStringFromCFString(string);
		assert(ioContent);

		dwarning(("ioContent = '%s'\t", ioContent));

		//Leaf

			boolean = (CFBooleanRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaLeafKey));
		if (!boolean) {
			dwarning(("\nkIOMediaLeafKey property missing for '%s'\n", ioBSDName));
			goto Next;
		}
		assert(CFGetTypeID(boolean) == CFBooleanGetTypeID());

		ioLeaf = (kCFBooleanTrue == boolean);

		dwarning(("ioLeaf = %d\t", ioLeaf));

		//Whole
			boolean = (CFBooleanRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaWholeKey));
		if (!boolean) {
			dwarning(("\nkIOMediaWholeKey property missing for '%s'\n", ioBSDName));
			goto Next;
		}
		assert(CFGetTypeID(boolean) == CFBooleanGetTypeID());

		ioWhole = (kCFBooleanTrue == boolean);

		dwarning(("ioWhole = %d\t", ioWhole));

		//Writable

			boolean = (CFBooleanRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaWritableKey));
		if (!boolean) {
			dwarning(("\nkIOMediaWritableKey property missing for '%s'\n", ioBSDName));
			goto Next;
		}
		assert(CFGetTypeID(boolean) == CFBooleanGetTypeID());

		ioWritable = (kCFBooleanTrue == boolean);

		dwarning(("ioWritable = %d\t", ioWritable));

		//Ejectable

			boolean = (CFBooleanRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaEjectableKey));
		if (!boolean) {
			dwarning(("\nkIOMediaEjectableKey property missing for '%s'\n", ioBSDName));
			goto Next;
		}
		assert(CFGetTypeID(boolean) == CFBooleanGetTypeID());

		ioEjectable = (kCFBooleanTrue == boolean);

		dwarning(("ioEjectable = %d\t", ioEjectable));

		//ioSize

			number = (CFNumberRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaSizeKey));
		if (!number) {
			dwarning(("\nkIOMediaSizeKey property missing for '%s'\n", ioBSDName));
		}
		assert(CFGetTypeID(number) == CFNumberGetTypeID());

		if (!CFNumberGetValue(number, kCFNumberLongLongType, &ioSize)) {
			goto Next;
		}
		dwarning(("ioSize = %ld\t", (long int) ioSize));


		/* Obtain the device tree path. */

		kr = IORegistryEntryGetPath(entry, kIODeviceTreePlane, ioDeviceTreePath);
		if (kr) {
			//this warning is unneeded, since many volumes won 't have this pointer.
				// dwarning(("\nERROR: IORegistryEntryGetPath -> %d\n", kr));
			ioDeviceTreePathPtr = NULL;
		} else {
			dwarning(("ioDeviceTreePath = '%s'\t", ioDeviceTreePath));
			if (strlen(ioDeviceTreePath) < strlen("IODeviceTree:")) {
				dwarning(("\nERROR: expected leading 'IODeviceTree:' in ioDeviceTreePath\n"));
				ioDeviceTreePathPtr = ioDeviceTreePath;	/* for lack of a better
									 * alternative */
			} else {
				ioDeviceTreePathPtr = ioDeviceTreePath + strlen("IODeviceTree:");
				dwarning(("\ntrimmed ioDeviceTreePath = '%s'\n", ioDeviceTreePathPtr));
			}
		}

		//Construct the < flags > word

			flags = 0;

		if (!ioWritable)
			flags |= kDiskArbDiskAppearedLockedMask;

		if (ioEjectable)
			flags |= kDiskArbDiskAppearedEjectableMask;

		if (ioWhole)
			flags |= kDiskArbDiskAppearedWholeDiskMask;

		if (!ioLeaf)
			flags |= kDiskArbDiskAppearedNonLeafDiskMask;

		if (!ioSize)
			flags |= kDiskArbDiskAppearedNoSizeMask;
		//blank media


			// DiskType

        diskType = GetDiskType(entry);
            switch (diskType) {
            case kDiskTypeHD:
                    /* do nothing */
                    break;
            case kDiskTypeCD:
                    flags |= kDiskArbDiskAppearedCDROMMask;
                    wakeMonitor();
                    break;
            case kDiskTypeDVD:
                    flags |= kDiskArbDiskAppearedDVDROMMask;
                    wakeMonitor();
                    break;
            case kDiskTypeUnknown:
                    /* do nothing */
                    break;
            default:
                    /* do nothing */
                    break;
        }

        if (diskIsInternal(entry)) {
            dwarning(("\nInternal disk appeared ...\n"));
            flags |= kDiskArbDiskAppearedInternal;
        }

		//Create a disk record

		if (!shouldAutomount(entry)) {
			dwarning(("\nDo not mount this entry ...\n"));
			flags |= kDiskArbDiskAppearedNoMountMask;
		}
		if (shouldEjectOnLogout(entry)) {
			dwarning(("\nEject this entry on logout ...\n"));
			ejectOnLogout = 1;
		}
        {

			DiskPtr         dp;

			/*
			 * Is there an existing disk on our list with this
			 * IOBSDName?
			 */

			dp = LookupDiskByIOBSDName(ioBSDName);
			if (dp) {
				dwarning(("%s: '%s' already exists\n", __FUNCTION__, ioBSDName));

				if (dp->state != kDiskStatePostponed) {
					if (mountExisting) {
						if (dp->mountpoint && 0 == strcmp(dp->mountpoint, "")) {
							dp->state = kDiskStateNew;
                                                        dp->approvedForMounting = 0;
						}
					}
				}
			} else {
				/*
				 * Create a new disk, leaving the
				 * <mountpoint> initialized to NULL
				 */
				DiskPtr         disk = NewDisk(ioBSDName,
							       ioBSDUnit,
							       ioContent,
							   kDiskFamily_SCSI,
							       NULL,
							       ioMediaName,
							ioDeviceTreePathPtr,
							       entry,
						    ownerUIDForMedia(entry),
							       flags,
							       ioSize);
				if (!disk) {
					LogErrorMessage("%s: NewDisk() failed!\n", __FUNCTION__);
				}
				if (initialRun) {
					disk->state = kDiskStateNew;
                                        disk->approvedForMounting = 1;
				}
				if (ejectOnLogout) {
					disk->ejectOnLogout = ejectOnLogout;
				}
			}
		}

Next:

		if (properties)
			CFRelease(properties);
		if (ioBSDName)
			free(ioBSDName);
		if (ioContent)
			free(ioContent);

		IOObjectRelease(entry);

	}			/* while */

}				/* GetDisksFromRegistry */


/*
-----------------------------------------------------------------------------------------
*/

DiskType 
GetDiskType(io_registry_entry_t media)
{
	io_registry_entry_t parent = 0;
	//(needs release)
		io_registry_entry_t service = media;
	//mandatory initialization
		DiskType type = kDiskTypeUnknown;
	//mandatory initialization
		kern_return_t kr;

	while (service) {
		if (IOObjectConformsTo(service, "IOCDMedia")) {
			dwarning(("DiskType = CD\n"));
			type = kDiskTypeCD;
			break;
		} else if (IOObjectConformsTo(service, "IODVDMedia")) {
			dwarning(("DiskType = DVD\n"));
			type = kDiskTypeDVD;
			break;
		} else if (IOObjectConformsTo(service, "IOBlockStorageDevice")) {
			dwarning(("DiskType = HD\n"));
			type = kDiskTypeHD;
			break;
		}
		kr = IORegistryEntryGetParentEntry(service, kIOServicePlane, &parent);
		if (kr != KERN_SUCCESS)
			break;

		if (service != media)
			IOObjectRelease(service);

		service = parent;
	}

	if (service != media)
		IOObjectRelease(service);

	return type;
}

int 
shouldAutomount(io_registry_entry_t media)
{
	io_registry_entry_t parent = 0;
	//(needs release
	   io_registry_entry_t parentsParent = 0;
	//(needs release)
		io_registry_entry_t service = media;
	//mandatory initialization
		kern_return_t kr;

	int             mount = 1;
	//by default uninited

		kr = IORegistryEntryGetParentEntry(service, kIOServicePlane, &parent);
	if (kr != KERN_SUCCESS)
		return mount;

	while (parent) {

		kr = IORegistryEntryGetParentEntry(parent, kIOServicePlane, &parentsParent);
		if (kr != KERN_SUCCESS)
			break;

		{
			CFBooleanRef    autodiskmountRef = IORegistryEntryCreateCFProperty(parent, CFSTR("autodiskmount"), kCFAllocatorDefault, kNilOptions);

			if (autodiskmountRef) {
				assert(CFGetTypeID(autodiskmountRef) == CFBooleanGetTypeID());
				if (!(kCFBooleanTrue == autodiskmountRef)) {
					mount = 0;
					break;
				}
				CFRelease(autodiskmountRef);
			}
		}
		if (parent)
			IOObjectRelease(parent);
		parent = parentsParent;
		parentsParent = 0;

	}

	if (parent)
		IOObjectRelease(parent);
	if (parentsParent)
		IOObjectRelease(parentsParent);

	return mount;
}

int diskIsInternal(io_registry_entry_t media)
{
    io_registry_entry_t parent = 0;
    //(needs release
       io_registry_entry_t parentsParent = 0;
    //(needs release)
        io_registry_entry_t service = media;
    //mandatory initialization
        kern_return_t kr;

        int             isInternal = 0;
    //by default inited

    kr = IORegistryEntryGetParentEntry(service, kIOServicePlane, &parent);
    if (kr != KERN_SUCCESS)
        return 1;

    while (parent) {

        kr = IORegistryEntryGetParentEntry(parent, kIOServicePlane, &parentsParent);
        if (kr != KERN_SUCCESS)
            break;

        if (IOObjectConformsTo(parent, "IOBlockStorageDevice"))
        {
            CFDictionaryRef characteristics = IORegistryEntryCreateCFProperty(parent, CFSTR("Protocol Characteristics"), kCFAllocatorDefault, kNilOptions);

            if (characteristics) {
                CFStringRef connection;
                // CFShow(characteristics);
                connection = (CFStringRef) CFDictionaryGetValue(characteristics, CFSTR("Physical Interconnect Location"));
                if (connection) {
                    CFComparisonResult result;
                    assert(CFGetTypeID(connection) == CFStringGetTypeID());

                    result = CFStringCompare(connection, CFSTR("Internal"), NULL);
                    if (result == kCFCompareEqualTo) {
                        isInternal = 1;
                    }
                }

                CFRelease(characteristics);
            }
            break;
        }
        if (parent)
            IOObjectRelease(parent);
        parent = parentsParent;
        parentsParent = 0;

    }

    if (parent)
        IOObjectRelease(parent);
    if (parentsParent)
        IOObjectRelease(parentsParent);

    return isInternal;
}

int 
shouldEjectOnLogout(io_registry_entry_t media)
{
	io_registry_entry_t parent = 0;
	//(needs release
	   io_registry_entry_t parentsParent = 0;
	//(needs release)
		io_registry_entry_t service = media;
	//mandatory initialization
		kern_return_t kr;

	int             eject = 0;
	//by default uninited

		kr = IORegistryEntryGetParentEntry(service, kIOServicePlane, &parent);
	if (kr != KERN_SUCCESS)
		return eject;

	while (parent) {

		kr = IORegistryEntryGetParentEntry(parent, kIOServicePlane, &parentsParent);
		if (kr != KERN_SUCCESS)
			break;

		{
			CFBooleanRef    ejectRef = IORegistryEntryCreateCFProperty(parent, CFSTR("eject-upon-logout"), kCFAllocatorDefault, kNilOptions);

			if (ejectRef) {
				assert(CFGetTypeID(ejectRef) == CFBooleanGetTypeID());
				if (kCFBooleanTrue == ejectRef) {
					eject = 1;
					break;
				}
				CFRelease(ejectRef);
			}
		}
		if (parent)
			IOObjectRelease(parent);
		parent = parentsParent;
		parentsParent = 0;

	}

	if (parent)
		IOObjectRelease(parent);
	if (parentsParent)
		IOObjectRelease(parentsParent);

	return eject;
}


int 
ownerUIDForMedia(io_registry_entry_t media)
{
	io_registry_entry_t parent = 0;
	//(needs release
	   io_registry_entry_t parentsParent = 0;
	//(needs release)
		io_registry_entry_t service = media;
	//mandatory initialization
		kern_return_t kr;

	int             ownerUID = -1;
	//by default uninited

		kr = IORegistryEntryGetParentEntry(service, kIOServicePlane, &parent);
	if (kr != KERN_SUCCESS)
		return ownerUID;

	while (parent) {

		kr = IORegistryEntryGetParentEntry(parent, kIOServicePlane, &parentsParent);
		if (kr != KERN_SUCCESS)
			break;

		{
			//get owner - uid property
				CFNumberRef ownerRef = IORegistryEntryCreateCFProperty(parent, CFSTR("owner-uid"), kCFAllocatorDefault, kNilOptions);
			if (ownerRef) {
				assert(CFGetTypeID(ownerRef) == CFNumberGetTypeID());
				CFNumberGetValue(ownerRef, kCFNumberIntType, &ownerUID);
				dwarning(("Owner UID found %d\n", ownerUID));
				CFRelease(ownerRef);
				break;
			}
		}

		if (parent)
			IOObjectRelease(parent);
		parent = parentsParent;
		parentsParent = 0;

	}

	if (parent)
		IOObjectRelease(parent);
	if (parentsParent)
		IOObjectRelease(parentsParent);

	return ownerUID;
}

/*
-----------------------------------------------------------------------------------------
*/
