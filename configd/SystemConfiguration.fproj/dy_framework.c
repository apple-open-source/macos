/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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
 * Modification History
 *
 * October 31, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <mach-o/dyld.h>

#include "dy_framework.h"


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


__private_extern__ CFMutableDictionaryRef
_IOBSDNameMatching(mach_port_t masterPort, unsigned int options, const char *bsdName)
{
	#undef IOBSDNameMatching
	static typeof (IOBSDNameMatching) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IOBSDNameMatching", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(masterPort, options, bsdName) : NULL;
}


__private_extern__ io_object_t
_IOIteratorNext(io_iterator_t iterator)
{
	#undef IOIteratorNext
	static typeof (IOIteratorNext) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IOIteratorNext", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(iterator) : 0;
}


__private_extern__ kern_return_t
_IOMasterPort(mach_port_t bootstrapPort, mach_port_t *masterPort)
{
	#undef IOMasterPort
	static typeof (IOMasterPort) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IOMasterPort", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(bootstrapPort, masterPort) : KERN_FAILURE;
}


__private_extern__ boolean_t
_IOObjectConformsTo(io_object_t object, const io_name_t className)
{
	#undef IOObjectConformsTo
	static typeof (IOObjectConformsTo) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IOObjectConformsTo", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(object, className) : FALSE;
}


__private_extern__ boolean_t
_IOObjectGetClass(io_object_t object, io_name_t className)
{
	#undef IOObjectGetClass
	static typeof (IOObjectGetClass) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IOObjectGetClass", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(object, className) : FALSE;
}


__private_extern__ kern_return_t
_IOObjectRelease(io_object_t object)
{
	#undef IOObjectRelease
	static typeof (IOObjectRelease) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IOObjectRelease", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(object) : KERN_FAILURE;
}


__private_extern__ CFTypeRef
_IORegistryEntryCreateCFProperty(io_registry_entry_t entry, CFStringRef key, CFAllocatorRef allocator, IOOptionBits options)
{
	#undef IORegistryEntryCreateCFProperty
	static typeof (IORegistryEntryCreateCFProperty) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IORegistryEntryCreateCFProperty", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(entry, key, allocator, options) : NULL;
}


__private_extern__ kern_return_t
_IORegistryEntryCreateCFProperties(io_registry_entry_t entry, CFMutableDictionaryRef *properties, CFAllocatorRef allocator, IOOptionBits options)
{
	#undef IORegistryEntryCreateCFProperties
	static typeof (IORegistryEntryCreateCFProperties) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IORegistryEntryCreateCFProperties", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(entry, properties, allocator, options) : KERN_FAILURE;
}


__private_extern__ kern_return_t
_IORegistryEntryCreateIterator(mach_port_t masterPort, const io_name_t plane, IOOptionBits options, io_iterator_t *iterator)
{
	#undef IORegistryEntryCreateIterator
	static typeof (IORegistryEntryCreateIterator) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IORegistryEntryCreateIterator", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(masterPort, plane, options, iterator) : KERN_FAILURE;
}


__private_extern__ kern_return_t
_IORegistryEntryGetName(io_registry_entry_t entry, io_name_t name)
{
	#undef IORegistryEntryGetName
	static typeof (IORegistryEntryGetName) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IORegistryEntryGetName", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(entry, name) : KERN_FAILURE;
}


__private_extern__ kern_return_t
_IORegistryEntryGetParentEntry(io_registry_entry_t entry, const io_name_t plane, io_registry_entry_t *parent)
{
	#undef IORegistryEntryGetParentEntry
	static typeof (IORegistryEntryGetParentEntry) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IORegistryEntryGetParentEntry", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(entry, plane, parent) : KERN_FAILURE;
}


__private_extern__ kern_return_t
_IORegistryEntryGetPath(io_registry_entry_t entry, const io_name_t plane, io_string_t path)
{
	#undef IORegistryEntryGetPath
	static typeof (IORegistryEntryGetPath) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IORegistryEntryGetPath", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(entry, plane, path) : KERN_FAILURE;
}


__private_extern__ CFTypeRef
_IORegistryEntrySearchCFProperty(io_registry_entry_t entry, const io_name_t plane, CFStringRef key, CFAllocatorRef allocator, IOOptionBits options)
{
	#undef IORegistryEntrySearchCFProperty
	static typeof (IORegistryEntrySearchCFProperty) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IORegistryEntrySearchCFProperty", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(entry, plane, key, allocator, options) : NULL;
}


__private_extern__ kern_return_t
_IOServiceGetMatchingServices(mach_port_t masterPort, CFDictionaryRef matching, io_iterator_t *existing)
{
	#undef IOServiceGetMatchingServices
	static typeof (IOServiceGetMatchingServices) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IOServiceGetMatchingServices", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(masterPort, matching, existing) : KERN_FAILURE;
}


__private_extern__ CFMutableDictionaryRef
_IOServiceMatching(const char *name)
{
	#undef IOServiceMatching
	static typeof (IOServiceMatching) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadIOKit();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_IOServiceMatching", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(name) : NULL;
}

static void *
__loadSecurity(void) {
	static const void *image = NULL;
	if (NULL == image) {
		const char	*framework		= "/System/Library/Frameworks/Security.framework/Security";
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


__private_extern__ OSStatus
_SecKeychainItemCopyContent(SecKeychainItemRef itemRef, SecItemClass *itemClass, SecKeychainAttributeList *attrList, UInt32 *length, void **outData)
{
	#undef SecKeychainItemCopyContent
	static typeof (SecKeychainItemCopyContent) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadSecurity();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_SecKeychainItemCopyContent", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(itemRef, itemClass, attrList, length, outData) : -1;
}

__private_extern__ OSStatus
_SecKeychainSearchCopyNext(SecKeychainSearchRef searchRef, SecKeychainItemRef *itemRef)
{
	#undef SecKeychainSearchCopyNext
	static typeof (SecKeychainSearchCopyNext) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadSecurity();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_SecKeychainSearchCopyNext", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(searchRef, itemRef) : -1;
}

__private_extern__ OSStatus
_SecKeychainSearchCreateFromAttributes(CFTypeRef keychainOrArray, SecItemClass itemClass, const SecKeychainAttributeList *attrList, SecKeychainSearchRef *searchRef)
{
	#undef SecKeychainSearchCreateFromAttributes
	static typeof (SecKeychainSearchCreateFromAttributes) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadSecurity();
		if (image) dyfunc = NSAddressOfSymbol(NSLookupSymbolInImage(image, "_SecKeychainSearchCreateFromAttributes", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND));
	}
	return dyfunc ? dyfunc(keychainOrArray, itemClass, attrList, searchRef) : -1;
}
