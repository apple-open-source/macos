/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 * Modification History
 *
 * May 29, 2002		Roger Smith <rsmith@apple.com>
 * - initial revision
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <pthread.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

#include <IOKit/IOKitLib.h>

#include "moh_msg.h"
#include "moh.h"
#include "DeviceOnHold.h"


#define kIODeviceSupportsHoldKey	"DeviceSupportsHold"


static void *
__loadIOKit(void) {
	static const void *image = NULL;
	if (NULL == image) {
		const char	*framework		= "/System/Library/Frameworks/IOKit.framework/IOKit";
		struct stat	statbuf;
		const char	*suffix			= getenv("DYLD_IMAGE_SUFFIX");
		char		path[MAXPATHLEN];

		strcpy(path, framework);
		if (suffix) strcat(path, suffix);
		if (0 <= stat(path, &statbuf)) {
			image = NSAddImage(path, NSADDIMAGE_OPTION_NONE);
		} else {
			image = NSAddImage(framework, NSADDIMAGE_OPTION_NONE);
		}
	}
	return (void *)image;
}


static io_object_t
_IOIteratorNext(io_iterator_t iterator)
{
	static io_object_t (*dyfunc)(io_iterator_t) = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IOIteratorNext", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(iterator) : 0;
}
#define IOIteratorNext _IOIteratorNext


static kern_return_t
_IOMasterPort(mach_port_t bootstrapPort, mach_port_t *masterPort)
{
	static kern_return_t (*dyfunc)(mach_port_t, mach_port_t *) = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IOMasterPort", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(bootstrapPort, masterPort) : KERN_FAILURE;
}
#define IOMasterPort _IOMasterPort


static kern_return_t
_IOObjectRelease(io_object_t object)
{
	static kern_return_t (*dyfunc)(io_object_t) = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IOObjectRelease", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(object) : KERN_FAILURE;
}
#define IOObjectRelease _IOObjectRelease


static kern_return_t
_IORegistryEntryCreateCFProperties(io_registry_entry_t entry, CFMutableDictionaryRef *properties, CFAllocatorRef allocator, IOOptionBits options)
{
	static kern_return_t (*dyfunc)(io_registry_entry_t, CFMutableDictionaryRef *, CFAllocatorRef, IOOptionBits) = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IORegistryEntryCreateCFProperties", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(entry, properties, allocator, options) : KERN_FAILURE;
}
#define IORegistryEntryCreateCFProperties _IORegistryEntryCreateCFProperties


static kern_return_t
_IORegistryEntryGetPath(io_registry_entry_t entry, const io_name_t plane, io_string_t path)
{
	static kern_return_t (*dyfunc)(io_registry_entry_t, const io_name_t, io_string_t) = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IORegistryEntryGetPath", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(entry, plane, path) : KERN_FAILURE;
}
#define IORegistryEntryGetPath _IORegistryEntryGetPath


static kern_return_t
_IOServiceGetMatchingServices(mach_port_t masterPort, CFDictionaryRef matching, io_iterator_t *existing)
{
	static kern_return_t (*dyfunc)(mach_port_t, CFDictionaryRef, io_iterator_t *) = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IOServiceGetMatchingServices", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(masterPort, matching, existing) : KERN_FAILURE;
}
#define IOServiceGetMatchingServices _IOServiceGetMatchingServices


static CFMutableDictionaryRef
_IOServiceMatching(const char *name)
{
	static CFMutableDictionaryRef (*dyfunc)(const char *) = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IOServiceMatching", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(name) : NULL;
}
#define IOServiceMatching _IOServiceMatching


typedef struct {

	/* base CFType information */
	CFRuntimeBase	cfBase;

	/* device name (e.g. "modem") */
	CFStringRef	name;

	int		sock;

} DeviceOnHoldPrivate, *DeviceOnHoldPrivateRef;


static CFStringRef
__DeviceOnHoldCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef		allocator	= CFGetAllocator(cf);
	CFMutableStringRef	result;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<DeviceOnHold %p [%p]> {\n"), cf, allocator);
	CFStringAppendFormat(result, NULL, CFSTR("}"));

	return result;
}


static void
__DeviceOnHoldDeallocate(CFTypeRef cf)
{
	DeviceOnHoldPrivateRef	DeviceOnHoldPrivate	= (DeviceOnHoldPrivateRef)cf;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("__DeviceOnHoldDeallocate:"));

	/* release resources */
	if (DeviceOnHoldPrivate->name)		CFRelease(DeviceOnHoldPrivate->name);
	if (DeviceOnHoldPrivate->sock != -1) {

	}

	return;
}


static CFTypeID __kDeviceOnHoldTypeID = _kCFRuntimeNotATypeID;


static const CFRuntimeClass __DeviceOnHoldClass = {
	0,				// version
	"DeviceOnHold",			// className
	NULL,				// init
	NULL,				// copy
	__DeviceOnHoldDeallocate,	// dealloc
	NULL,				// equal
	NULL,				// hash
	NULL,				// copyFormattingDesc
	__DeviceOnHoldCopyDescription	// copyDebugDesc
};


static pthread_once_t initialized	= PTHREAD_ONCE_INIT;


static void
__DeviceOnHoldInitialize(void)
{
	__kDeviceOnHoldTypeID = _CFRuntimeRegisterClass(&__DeviceOnHoldClass);
	return;
}


DeviceOnHoldRef
__DeviceOnHoldCreatePrivate(CFAllocatorRef	allocator)
{
	DeviceOnHoldPrivateRef		devicePrivate;
	UInt32				size;

	/* initialize runtime */
	pthread_once(&initialized, __DeviceOnHoldInitialize);

	/* allocate session */
	size          = sizeof(DeviceOnHoldPrivate) - sizeof(CFRuntimeBase);
	devicePrivate = (DeviceOnHoldPrivateRef)_CFRuntimeCreateInstance(allocator,
									 __kDeviceOnHoldTypeID,
									 size,
									 NULL);
	if (!devicePrivate) {
		return NULL;
	}

	devicePrivate->name	= NULL;
	devicePrivate->sock	= -1;

	return (DeviceOnHoldRef)devicePrivate;
}


/*
 * TBD: We determine whether a device supports on hold capability by looking at
 * the numeric property DeviceSupportsHold (1 - yes, 0 or no property - no). For
 * the Apple Dash II internal modem we also use the property V92Modem to track
 * this same capability.
 */

Boolean
IsDeviceOnHoldSupported(CFStringRef	deviceName,	// "modem"
			CFDictionaryRef	options)
{
	CFMutableDictionaryRef	deviceToMatch;
	u_int32_t		deviceSupportsHoldValue;
	kern_return_t		kr;
	mach_port_t		masterPort;
	io_iterator_t 		matchingServices;
	CFNumberRef		num;
	CFMutableDictionaryRef	properties;
	Boolean			result				= FALSE;
	io_service_t 		service;

	if (CFStringCompare(deviceName, CFSTR("modem"), NULL) == kCFCompareEqualTo) {
		kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
		if (kr != KERN_SUCCESS) {
			goto errorExit;
		}

		deviceToMatch = IOServiceMatching("InternalModemSupport");
		if (!deviceToMatch) {
			goto errorExit;
		}

		kr = IOServiceGetMatchingServices(masterPort, deviceToMatch, &matchingServices);
		if (kr != KERN_SUCCESS) {
			goto errorExit;
		}

		for ( ; service = IOIteratorNext(matchingServices) ; IOObjectRelease(service)) {
			io_string_t	path;

			kr = IORegistryEntryGetPath(service, kIOServicePlane, path);
			assert( kr == KERN_SUCCESS );

			// grab a copy of the properties
			kr = IORegistryEntryCreateCFProperties(service, &properties, kCFAllocatorDefault, kNilOptions);
			assert( kr == KERN_SUCCESS );

			num = CFDictionaryGetValue(properties, CFSTR(kIODeviceSupportsHoldKey));
			if (isA_CFNumber(num)) {
				CFNumberGetValue(num, kCFNumberSInt32Type, &deviceSupportsHoldValue);
				if (deviceSupportsHoldValue == 1) {
					result = TRUE;
				}
			}

			CFRelease(properties);
		}

		IOObjectRelease(matchingServices);
	}

	// Note: The issue for the general case is how to go from the SystemConfiguration
	//       dynamic store to the actual driver. The devicesupportshold property is not
	//       copied the either of the setup/state descriptions so the caller would need
	//       to know the exact driver they are searching for.

	return result;

    errorExit:

	return FALSE;
}


DeviceOnHoldRef
DeviceOnHoldCreate(CFAllocatorRef	allocator,
		   CFStringRef		deviceName,	// "modem"
		   CFDictionaryRef	options)
{
	DeviceOnHoldRef		device		= NULL;
	DeviceOnHoldPrivateRef	devicePrivate;
	int			status;

	if (CFStringCompare(deviceName, CFSTR("modem"), NULL) != kCFCompareEqualTo) {
		return NULL;
	}

	device = __DeviceOnHoldCreatePrivate(allocator);
	if (!device) {
		return NULL;
	}

	devicePrivate = (DeviceOnHoldPrivateRef)device;

	status = MOHInit(&devicePrivate->sock, deviceName);
	if (status != 0) {
		CFRelease(device);
		return NULL;
	}

	devicePrivate->name = CFRetain(deviceName);

	return device;
}



int32_t
DeviceOnHoldGetStatus(DeviceOnHoldRef	device)
{
	DeviceOnHoldPrivateRef	devicePrivate	= (DeviceOnHoldPrivateRef)device;
	int			err;
	u_long			link		= 1;
	void			*replyBuf;
	u_long			replyBufLen;
	int32_t			result		= -1;

	if (!device) {
		return -1;
	}

	if (devicePrivate->sock == -1) {
		return -1;
	}

	err = MOHExec(devicePrivate->sock,
		      link,
		      MOH_SESSION_GET_STATUS,
		      NULL,
		      0,
		      &replyBuf,
		      &replyBufLen);

	if (err != 0) {
		return -1;
	}

	if (replyBufLen == sizeof(result)) {
		result = *(int32_t *)replyBuf;
	}

	if (replyBuf)	CFAllocatorDeallocate(NULL, replyBuf);
	return result;
}


Boolean
DeviceOnHoldSuspend(DeviceOnHoldRef	device)
{
	DeviceOnHoldPrivateRef	devicePrivate	= (DeviceOnHoldPrivateRef)device;
	int			err;
	u_long			link		= 1;
	void			*replyBuf;
	u_long			replyBufLen;
	Boolean			result		= FALSE;

	if (!device) {
		return FALSE;
	}

	if (devicePrivate->sock == -1) {
		return FALSE;
	}

	err = MOHExec(devicePrivate->sock,
		      link,
		      MOH_PUT_SESSION_ON_HOLD,
		      NULL,
		      0,
		      &replyBuf,
		      &replyBufLen);

	if (err != 0) {
		return -1;
	}

	if (replyBufLen == sizeof(result)) {
		result = (*(int32_t *)replyBuf) ? TRUE : FALSE;
	}

	if (replyBuf)	CFAllocatorDeallocate(NULL, replyBuf);
	return result;
}


Boolean
DeviceOnHoldResume(DeviceOnHoldRef	device)
{
	DeviceOnHoldPrivateRef	devicePrivate	= (DeviceOnHoldPrivateRef)device;
	int			err;
	u_long			link		= 1;
	void			*replyBuf;
	u_long			replyBufLen;
	Boolean			result		= FALSE;

	if (!device) {
		return FALSE;
	}

	if (devicePrivate->sock == -1) {
		return FALSE;
	}

	err = MOHExec(devicePrivate->sock,
		      link,
		      MOH_RESUME_SESSION_ON_HOLD,NULL,
		      0,
		      &replyBuf,
		      &replyBufLen);

	if (err != 0) {
		return -1;
	}

	if (replyBufLen == sizeof(result)) {
		result = (*(int32_t *)replyBuf) ? TRUE : FALSE;
	}

	if (replyBuf)	CFAllocatorDeallocate(NULL, replyBuf);
	return result;
}
