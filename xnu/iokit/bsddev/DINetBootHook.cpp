/*
 * Copyright (c) 2002-2016 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 *  DINetBootHook.c
 *  DiskImages
 *
 *  Created by Byron Han on Sat Apr 13 2002.
 *
 *	Revision History
 *
 *	$Log: DINetBootHook.cpp,v $
 *	Revision 1.4  2005/07/29 21:49:57  lindak
 *	Merge of branch "chardonnay" to pick up all chardonnay changes in Leopard
 *	as of xnu-792.7.4
 *
 *	Revision 1.3.1558.1  2005/06/24 01:47:25  lindak
 *	Bringing over all of the Karma changes into chardonnay.
 *
 *	Revision 1.1.1.1  2005/02/24 21:48:06  akosut
 *	Import xnu-764 from Tiger8A395
 *
 *	Revision 1.3  2002/06/16 20:36:02  lindak
 *	Merged PR-2957314 into Jaguar (siegmund: netboot kernel code needs to set
 *	com.apple.AppleDiskImageController.load to boolean Yes)
 *
 *	Revision 1.2.40.2  2002/06/15 03:50:38  dieter
 *	- corrected com.apple.AppleDiskImageController.load string
 *
 *	Revision 1.2.40.1  2002/06/15 03:01:08  dieter
 *	Bug #: 2957314
 *	- add call to force IOHDIXController to get loaded/matched
 *
 *	Revision 1.2  2002/05/03 18:08:39  lindak
 *	Merged PR-2909558 into Jaguar (siegmund POST WWDC: add support for NetBoot
 *	over IOHDIXController)
 *
 *	Revision 1.1.2.1  2002/04/24 22:29:12  dieter
 *	Bug #: 2909558
 *	- added IOHDIXController netboot stubs
 *
 *	Revision 1.3  2002/04/16 00:41:37  han
 *	migrated code out of here to IOHDIXController's setProperty method
 *
 *	Revision 1.2  2002/04/14 23:53:53  han
 *	eliminate qDEBUG=1, use emums instead of hard coded string constants
 *
 *	Revision 1.1  2002/04/14 22:54:42  han
 *	Renamed from DINetBookHook.c.
 *	First stab at implementing this code.
 *
 *	Revision 1.1  2002/04/13 19:22:28  han
 *	added stub file DINetBookHook.c
 *
 *
 */
#ifndef qDEBUG
#define qDEBUG 0
#endif

#if qDEBUG
#warning qDEBUG is 1!
#endif

#include <sys/types.h>
#include <mach/clock_types.h>
#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include "DINetBootHook.h"

#define kIOHDIXControllerClassName      "IOHDIXController"
#define kDIRootImageKey                         "di-root-image"
#define kDIRootImageRemovableKey                "di-root-removable"
#define kDIRootImageResultKey           "di-root-image-result"
#define kDIRootImageDevNameKey          "di-root-image-devname"
#define kDIRootImageDevTKey                     "di-root-image-devt"
#define kDIRootRamFileKey           "di-root-ram-file"

#define kDIMatchQuiesceTimeout          30ull

static IOService *
di_load_controller( void )
{
	OSIterator *    controllerIterator      = NULL;
	OSDictionary *  matchDictionary         = NULL;
	IOService *     controller                      = NULL;

	do {
		IOService::getResourceService()->publishResource("com.apple.AppleDiskImageController.load", kOSBooleanTrue);
		IOService::getResourceService()->waitQuiet();

		// first find IOHDIXController
		matchDictionary = IOService::serviceMatching(kIOHDIXControllerClassName);
		if (!matchDictionary) {
			break;
		}

		controllerIterator = IOService::getMatchingServices(matchDictionary);
		if (!controllerIterator) {
			break;
		}

		controller = OSDynamicCast(IOService, controllerIterator->getNextObject());
		if (!controller) {
			break;
		}

		controller->retain();
	} while (false);

	if (matchDictionary) {
		matchDictionary->release();
	}
	if (controllerIterator) {
		controllerIterator->release();
	}

	return controller;
}

extern "C" {
/* FIXME: removable should be replaced with a struct (so it could be easily
 * extensible in the future). However, since there is no common header file
 * between imageboot and NetBoot, we opt for a simple bool for now.
 * Refactor this into a common header file.
 */
static int
di_add_properties(IOService *controller, bool removable)
{
	if (!controller->setProperty(kDIRootImageRemovableKey, removable ? kOSBooleanTrue : kOSBooleanFalse)) {
		IOLog("IOHDIXController::setProperty(%s, %d) failed.\n", kDIRootImageRemovableKey, !!removable);
		return kIOReturnBadArgument;
	}

	return kIOReturnSuccess;
}

int
di_root_image_ext(const char *path, char *devname, size_t devsz, dev_t *dev_p, bool removable)
{
	IOReturn                        res                             = 0;
	IOService               *       controller                      = NULL;
	OSString                *       pathString                      = NULL;
	OSNumber                *       myResult                        = NULL;
	OSString                *       myDevName                       = NULL;
	OSNumber                *       myDevT                          = NULL;

	// sanity check arguments please
	if (devname) {
		*devname = 0;
	}
	if (dev_p) {
		*dev_p = 0;
	}

	if (!path) {
		return kIOReturnBadArgument;
	}
	if (!devname) {
		return kIOReturnBadArgument;
	}
	if (!dev_p) {
		return kIOReturnBadArgument;
	}

	controller = di_load_controller();
	if (!controller) {
		res = kIOReturnNotFound;
		goto NoIOHDIXController;
	}

	// okay create path object
	pathString = OSString::withCString(path);
	if (!pathString) {
		res = kIOReturnNoMemory;
		goto CannotCreatePathOSString;
	}

	/*
	 * This is a bit racy, as two concurrent attached could have
	 * different properties. However, since we query the result and dev
	 * below locklessly, the existing code is already racy, so we
	 * keep the status quo.
	 */
	res = di_add_properties(controller, removable);
	if (res) {
		goto error_add_properties;
	}

	// do it
	if (!controller->setProperty(kDIRootImageKey, pathString)) {
		IOLog("IOHDIXController::setProperty(%s, %s) failed.\n", kDIRootImageKey, pathString->getCStringNoCopy());
	}

	myResult = OSDynamicCast(OSNumber, controller->getProperty(kDIRootImageResultKey));
	res = kIOReturnError;
	if (myResult) {
		res = myResult->unsigned32BitValue();
	}

	if (res) {
		IOLog("%s is 0x%08X/%d\n", kDIRootImageResultKey, res, res);
		goto di_root_image_FAILED;
	}

	// success - grab
	myDevT = OSDynamicCast(OSNumber, controller->getProperty(kDIRootImageDevTKey));
	if (myDevT) {
		*dev_p = myDevT->unsigned32BitValue();
	} else {
		IOLog("could not get %s\n", kDIRootImageDevTKey);
		res = kIOReturnError;
		goto di_root_image_FAILED;
	}

	myDevName = OSDynamicCast(OSString, controller->getProperty(kDIRootImageDevNameKey));
	if (myDevName) {
		strlcpy(devname, myDevName->getCStringNoCopy(), devsz);
	} else {
		IOLog("could not get %s\n", kDIRootImageDevNameKey);
		res = kIOReturnError;
		goto di_root_image_FAILED;
	}

	/*
	 * NOTE: The attached disk image may trigger IOKit matching. At the very least, an IOMedia
	 * must claim it.  More complex scenarios might include a GPT containing a partition mapping
	 * to an APFS container, both of which need to probe and claim their respective media devices.
	 *
	 * After the attach is complete, we should quiesce the disk image controller before returning
	 * from this function successfully.  If we failed to quiesce, then we should treat it as a hard
	 * failure, to make it more obvious to triage.
	 */
	res = controller->waitQuiet((NSEC_PER_SEC * kDIMatchQuiesceTimeout));
	if (res) {
		IOLog("failed to quiesce attached disk image (%s)! \n", devname);
		goto di_root_image_FAILED;
	}

di_root_image_FAILED:
CannotCreatePathOSString:
NoIOHDIXController:
error_add_properties:

	// clean up memory allocations
	if (pathString) {
		pathString->release();
	}
	if (controller) {
		controller->release();
	}

	return res;
}

/*
 *       Name:		di_root_image
 *       Function:	mount the disk image returning the dev node
 *       Parameters:	path	->		path/url to disk image
 *                               devname	<-		dev node used to set the rootdevice global variable
 *                               dev_p	<-		device number generated from major/minor numbers
 *       Comments:
 *       This is an exported function. Changing this will break API.
 */
int
di_root_image(const char *path, char *devname, size_t devsz, dev_t *dev_p)
{
	return di_root_image_ext(path, devname, devsz, dev_p, false);
}

int
di_root_ramfile_buf(void *buf, size_t bufsz, char *devname, size_t devsz, dev_t *dev_p)
{
	IOReturn res = 0;
	IOService *controller = NULL;
	OSNumber *myResult = NULL;
	OSString *myDevName = NULL;
	OSNumber *myDevT = NULL;
	IOMemoryDescriptor *mem = NULL;

	/* Use kIOMemoryAutoPrepare and wire down the buffer so readBytes() will work. */
	mem = IOMemoryDescriptor::withAddressRange(
		(mach_vm_address_t)buf, (mach_vm_size_t)bufsz,
		kIODirectionOut | kIOMemoryAutoPrepare, kernel_task);
	if (!mem) {
		res = kIOReturnNoMemory;
		goto out;
	}

	controller = di_load_controller();
	if (controller) {
		/* attach the image */
		controller->setProperty(kDIRootRamFileKey, mem);
		controller->release();
	} else {
		res = kIOReturnNotFound;
		goto out;
	}

	myResult = OSDynamicCast(OSNumber, controller->getProperty(kDIRootImageResultKey));
	res = kIOReturnError;
	if (myResult) {
		res = myResult->unsigned32BitValue();
	}

	if (res) {
		IOLog("%s is 0x%08X/%d\n", kDIRootImageResultKey, res, res);
		goto out;
	}

	myDevT = OSDynamicCast(OSNumber, controller->getProperty(kDIRootImageDevTKey));
	if (myDevT) {
		*dev_p = myDevT->unsigned32BitValue();
	} else {
		IOLog("could not get %s\n", kDIRootImageDevTKey);
		res = kIOReturnError;
		goto out;
	}

	myDevName = OSDynamicCast(OSString, controller->getProperty(kDIRootImageDevNameKey));
	if (myDevName) {
		strlcpy(devname, myDevName->getCStringNoCopy(), devsz);
	} else {
		IOLog("could not get %s\n", kDIRootImageDevNameKey);
		res = kIOReturnError;
		goto out;
	}

out:
	OSSafeReleaseNULL(mem);
	return res;
}

void
di_root_ramfile( IORegistryEntry * entry )
{
	OSData *                data;
	IOMemoryDescriptor *    mem;
	uint64_t                dmgSize;
	uint64_t                remain, length;
	OSData *                extentData = NULL;
	IOAddressRange *        extentList;
	uint64_t                extentSize;
	uint32_t                extentCount;

	do {
		data = OSDynamicCast(OSData, entry->getProperty("boot-ramdmg-size"));
		if (!data || (data->getLength() != sizeof(uint64_t))) {
			break; // bad disk image size
		}
		dmgSize = *(uint64_t *) data->getBytesNoCopy();
		if (!dmgSize) {
			break;
		}

		data = OSDynamicCast(OSData, entry->getProperty("boot-ramdmg-extents"));
		if (!data || (data->getLength() == 0) ||
		    ((data->getLength() & (sizeof(IOAddressRange) - 1)) != 0)) {
			break; // bad extents
		}
		// make modifications to local copy
		extentData  = OSData::withData(data);
		assert(extentData);

		/* BEGIN IGNORE CODESTYLE */
		__typed_allocators_ignore_push
		extentList  = (IOAddressRange *) extentData->getBytesNoCopy();
		__typed_allocators_ignore_pop
		/* END IGNORE CODESTYLE */
		extentCount = extentData->getLength() / sizeof(IOAddressRange);
		extentSize  = 0;
		remain = dmgSize;

		// truncate extent length to enclosing disk image
		for (uint32_t i = 0; i < extentCount; i++) {
			length = extentList[i].length;
			if (!length) {
				break;
			}

			extentSize += length;
			if (length >= remain) {
				extentList[i].length = remain;
				extentCount = i + 1;
				break;
			}
			remain -= length;
		}
		if (extentSize < dmgSize) {
			break; // not enough extent bytes for enclosing disk image
		}
		mem = IOMemoryDescriptor::withAddressRanges(
			extentList, extentCount,
			kIODirectionOut | kIOMemoryMapperNone, NULL);

		if (mem) {
			IOService * controller = di_load_controller();
			if (controller) {
				controller->setProperty(kDIRootRamFileKey, mem);
				controller->release();
			}
			mem->release();
		}
	} while (false);

	if (extentData) {
		extentData->release();
	}
}
};
