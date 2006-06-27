/*
 * Copyright (c) 2004, 2005 Apple Computer, Inc. All rights reserved.
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

#ifndef _SCNETWORKCONFIGURATIONINTERNAL_H
#define _SCNETWORKCONFIGURATIONINTERNAL_H


#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPreferencesPathKey.h>
#include <IOKit/IOKitLib.h>

#include "SCNetworkConfiguration.h"

typedef struct {

	/* base CFType information */
	CFRuntimeBase		cfBase;

	/* set id */
	CFStringRef		setID;

	/* prefs */
	SCPreferencesRef	prefs;

} SCNetworkSetPrivate, *SCNetworkSetPrivateRef;


typedef struct {

	/* base CFType information */
	CFRuntimeBase		cfBase;

	/* service id */
	CFStringRef		serviceID;

	/* interface */
	SCNetworkInterfaceRef   interface;

	/* prefs */
	SCPreferencesRef	prefs;

} SCNetworkServicePrivate, *SCNetworkServicePrivateRef;


typedef struct {

	/* base CFType information */
	CFRuntimeBase		cfBase;

	/* entity id */
	CFStringRef		entityID;

	/* service */
	SCNetworkServiceRef     service;

} SCNetworkProtocolPrivate, *SCNetworkProtocolPrivateRef;


typedef struct {

	// base CFType information
	CFRuntimeBase		cfBase;

	// interface information
	CFStringRef		interface_type;		// interface type

	// localized name
	CFStringRef		localized_name;		// localized [display] name
	CFStringRef		localized_key;
	CFStringRef		localized_arg1;
	CFStringRef		localized_arg2;

	/* [layered] interface*/
	SCNetworkInterfaceRef   interface;

	/* service (NULL if not associated with a service)  */
	SCNetworkServiceRef     service;

	/* unsaved configuration (when prefs not [yet] available) */
	CFDictionaryRef		unsaved;

	// [SCPreferences] interface entity information
	CFStringRef		entity_device;		// interface device
	CFStringRef		entity_type;		// interface type
	CFStringRef		entity_subtype;		// interface subtype

	// configuration information
	CFMutableArrayRef       supported_interface_types;
	CFMutableArrayRef       supported_protocol_types;

	// IORegistry (service plane) information
	CFStringRef		address;
	Boolean			builtin;
	CFStringRef		location;
	CFStringRef		path;
	CFStringRef		modemCCL;
	Boolean			modemIsV92;
	Boolean			supportsBond;
	Boolean			supportsVLAN;

	// misc
	int			sort_order;		// sort order for this interface

} SCNetworkInterfacePrivate, *SCNetworkInterfacePrivateRef;


__BEGIN_DECLS


SCNetworkServicePrivateRef
__SCNetworkServiceCreatePrivate			(CFAllocatorRef		allocator,
						 CFStringRef		serviceID,
						 SCNetworkInterfaceRef	interface,
						 SCPreferencesRef	prefs);


SCNetworkProtocolPrivateRef
__SCNetworkProtocolCreatePrivate		(CFAllocatorRef		allocator,
						 CFStringRef		entityID,
						 SCNetworkServiceRef	service);

Boolean
__SCNetworkProtocolIsValidType			(CFStringRef		protocolType);

SCNetworkInterfacePrivateRef
__SCNetworkInterfaceCreateCopy			(CFAllocatorRef		allocator,
						 SCNetworkInterfaceRef  interface,
						 SCNetworkServiceRef    service);

SCNetworkInterfacePrivateRef
__SCNetworkInterfaceCreatePrivate		(CFAllocatorRef		allocator,
						 SCNetworkInterfaceRef	interface,
						 SCNetworkServiceRef	service,
						 io_string_t		path);

CFDictionaryRef
__SCNetworkInterfaceCopyInterfaceEntity		(SCNetworkInterfaceRef	interface);

SCNetworkInterfaceRef
__SCNetworkInterfaceCreateWithEntity		(CFAllocatorRef		allocator,
						 CFDictionaryRef	interface_entity,
						 SCNetworkServiceRef	service);

CFArrayRef
__SCNetworkInterfaceCopyDeepConfiguration       (SCNetworkInterfaceRef  interface);

CFStringRef
__SCNetworkInterfaceGetModemCCL			(SCNetworkInterfaceRef	interface);

Boolean
__SCNetworkInterfaceIsModemV92			(SCNetworkInterfaceRef	interface);

Boolean
__SCNetworkInterfaceSetConfiguration		(SCNetworkInterfaceRef  interface,
						 CFDictionaryRef	config,
						 Boolean		okToHold);

void
__SCNetworkInterfaceSetDeepConfiguration	(SCNetworkInterfaceRef  interface,
						 CFArrayRef		configs);

CFDictionaryRef
__copyInterfaceTemplate				(CFStringRef		interfaceType,
						 CFStringRef		childInterfaceType);

CFDictionaryRef
__copyProtocolTemplate				(CFStringRef		interfaceType,
						 CFStringRef		childInterfaceType,
						 CFStringRef		protocolType);

CFDictionaryRef
__getPrefsConfiguration				(SCPreferencesRef       prefs,
						 CFStringRef		path);

Boolean
__setPrefsConfiguration				(SCPreferencesRef       prefs,
						 CFStringRef		path,
						 CFDictionaryRef	config,
						 Boolean		keepInactive);

Boolean
__getPrefsEnabled				(SCPreferencesRef       prefs,
						 CFStringRef		path);

Boolean
__setPrefsEnabled				(SCPreferencesRef       prefs,
						 CFStringRef		path,
						 Boolean		enabled);

Boolean
__createInterface				(int			s,
						 CFStringRef		interface);

Boolean
__destroyInterface				(int			s,
						 CFStringRef		interface);

Boolean
__markInterfaceUp				(int			s,
						 CFStringRef		interface);

__END_DECLS

#endif	/* _SCNETWORKCONFIGURATIONINTERNAL_H */
