/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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


#ifndef _DY_FRAMEWORK_H
#define _DY_FRAMEWORK_H

#include <sys/cdefs.h>
#include <mach/mach.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <Security/Security.h>

__BEGIN_DECLS

CFMutableDictionaryRef
_IOBSDNameMatching			(
					mach_port_t		masterPort,
					unsigned int		options,
					const char		*bsdName
					);
#define IOBSDNameMatching _IOBSDNameMatching

io_object_t
_IOIteratorNext				(
					io_iterator_t		iterator
					);
#define IOIteratorNext _IOIteratorNext

kern_return_t
_IOMasterPort				(
					mach_port_t		bootstrapPort,
					mach_port_t		*masterPort
					);
#define IOMasterPort _IOMasterPort

kern_return_t
_IOObjectRelease			(
					io_object_t		object
					);
#define IOObjectRelease _IOObjectRelease

CFTypeRef
_IORegistryEntryCreateCFProperty	(
					io_registry_entry_t	entry,
					CFStringRef		key,
					CFAllocatorRef		allocator,
					IOOptionBits		options
					);
#define IORegistryEntryCreateCFProperty _IORegistryEntryCreateCFProperty

kern_return_t
_IORegistryEntryCreateCFProperties	(
					io_registry_entry_t	entry,
					CFMutableDictionaryRef	*properties,
					CFAllocatorRef		allocator,
					IOOptionBits		options
					);
#define IORegistryEntryCreateCFProperties _IORegistryEntryCreateCFProperties

kern_return_t
_IORegistryEntryGetParentEntry		(
					io_registry_entry_t	entry,
					const io_name_t		plane,
					io_registry_entry_t	*parent
					);
#define IORegistryEntryGetParentEntry _IORegistryEntryGetParentEntry

kern_return_t
_IORegistryEntryGetPath			(
					io_registry_entry_t	entry,
					const io_name_t		plane,
					io_string_t		path
					);
#define IORegistryEntryGetPath _IORegistryEntryGetPath

kern_return_t
_IOServiceGetMatchingServices		(
					mach_port_t		masterPort,
					CFDictionaryRef		matching,
					io_iterator_t		*existing
					);
#define IOServiceGetMatchingServices _IOServiceGetMatchingServices

CFMutableDictionaryRef
_IOServiceMatching			(
					const char		*name
					);
#define IOServiceMatching _IOServiceMatching

OSStatus
_SecKeychainItemCopyContent		(
					SecKeychainItemRef		itemRef,
					SecItemClass			*itemClass,
					SecKeychainAttributeList	*attrList,
					UInt32				*length,
					void				**outData
					);
#define SecKeychainItemCopyContent _SecKeychainItemCopyContent

OSStatus
_SecKeychainSearchCopyNext		(
					SecKeychainSearchRef		searchRef,
					SecKeychainItemRef		*itemRef
					);
#define SecKeychainSearchCopyNext _SecKeychainSearchCopyNext

OSStatus
_SecKeychainSearchCreateFromAttributes	(
					CFTypeRef			keychainOrArray,
					SecItemClass			itemClass,
					const SecKeychainAttributeList	*attrList,
					SecKeychainSearchRef		*searchRef
					);
#define SecKeychainSearchCreateFromAttributes _SecKeychainSearchCreateFromAttributes

__END_DECLS

#endif	/* _DY_FRAMEWORK_H */

