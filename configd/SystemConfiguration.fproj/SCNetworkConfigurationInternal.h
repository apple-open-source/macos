/*
 * Copyright (c) 2004-2007 Apple Inc. All rights reserved.
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


typedef struct {

	// base CFType information
	CFRuntimeBase		cfBase;

	// set id
	CFStringRef		setID;

	// prefs
	SCPreferencesRef	prefs;

	// name
	CFStringRef		name;

} SCNetworkSetPrivate, *SCNetworkSetPrivateRef;


typedef struct {

	// base CFType information
	CFRuntimeBase		cfBase;

	// service id
	CFStringRef		serviceID;

	// interface
	SCNetworkInterfaceRef   interface;

	// prefs
	SCPreferencesRef	prefs;

	// name
	CFStringRef		name;

} SCNetworkServicePrivate, *SCNetworkServicePrivateRef;


typedef struct {

	// base CFType information
	CFRuntimeBase		cfBase;

	// entity id
	CFStringRef		entityID;

	// service
	SCNetworkServiceRef     service;

} SCNetworkProtocolPrivate, *SCNetworkProtocolPrivateRef;


typedef struct {

	// base CFType information
	CFRuntimeBase		cfBase;

	// interface information
	CFStringRef		interface_type;		// interface type

	// [non-localized] name
	CFStringRef		name;			// non-localized [display] name

	// localized name
	CFStringRef		localized_name;		// localized [display] name
	CFStringRef		localized_key;
	CFStringRef		localized_arg1;
	CFStringRef		localized_arg2;

	// [layered] interface
	SCNetworkInterfaceRef   interface;

	// prefs (for associated service, BOND interfaces, and VLAN interfaces)
	SCPreferencesRef	prefs;

	// serviceID (NULL if not associated with a service)
	CFStringRef		serviceID;

	// unsaved configuration (when prefs not [yet] available)
	CFMutableDictionaryRef	unsaved;

	// [SCPreferences] interface entity information
	CFStringRef		entity_device;		// interface device
	CFStringRef		entity_type;		// interface type
	CFStringRef		entity_subtype;		// interface subtype

	// configuration information
	CFMutableArrayRef       supported_interface_types;
	CFMutableArrayRef       supported_protocol_types;

	// IORegistry (service plane) information
	CFDataRef		address;
	CFStringRef		addressString;
	Boolean			builtin;
	CFStringRef		location;
	CFStringRef		path;
	CFMutableDictionaryRef	overrides;
	Boolean			modemIsV92;
	Boolean			supportsBond;
	Boolean			supportsVLAN;
	CFNumberRef		type;
	CFNumberRef		unit;

	// misc
	int			sort_order;		// sort order for this interface

	// for BOND interfaces
	struct {
		CFArrayRef		interfaces;
		CFDictionaryRef		options;
		CFNumberRef		mode;
	} bond;

	// for VLAN interfaces
	struct {
		SCNetworkInterfaceRef	interface;
		CFNumberRef		tag;		// e.g. 1 <= tag <= 4094
		CFDictionaryRef		options;
	} vlan;

} SCNetworkInterfacePrivate, *SCNetworkInterfacePrivateRef;


__BEGIN_DECLS


#pragma mark -
#pragma mark SCNetworkInterface configuration (internal)


CFArrayRef
__SCNetworkInterfaceCopyAll_IONetworkInterface	(void);

SCNetworkInterfacePrivateRef
__SCNetworkInterfaceCreateCopy			(CFAllocatorRef		allocator,
						 SCNetworkInterfaceRef  interface,
						 SCPreferencesRef	prefs,
						 CFStringRef		serviceID);

SCNetworkInterfacePrivateRef
__SCNetworkInterfaceCreatePrivate		(CFAllocatorRef		allocator,
						 SCNetworkInterfaceRef	interface,
						 SCPreferencesRef	prefs,
						 CFStringRef		serviceID,
						 io_string_t		path);

SCNetworkInterfacePrivateRef
_SCBondInterfaceCreatePrivate			(CFAllocatorRef		allocator,
						 CFStringRef		bond_if);

SCNetworkInterfacePrivateRef
_SCVLANInterfaceCreatePrivate			(CFAllocatorRef		allocator,
						 CFStringRef		vlan_if);

CFDictionaryRef
__SCNetworkInterfaceCopyInterfaceEntity		(SCNetworkInterfaceRef	interface);

CFArrayRef
__SCNetworkInterfaceCopyDeepConfiguration       (SCNetworkInterfaceRef  interface);

CFStringRef
__SCNetworkInterfaceGetDefaultConfigurationType	(SCNetworkInterfaceRef	interface);

CFStringRef
__SCNetworkInterfaceGetNonLocalizedDisplayName	(SCNetworkInterfaceRef	interface);

Boolean
__SCNetworkInterfaceIsValidExtendedConfigurationType
						(SCNetworkInterfaceRef	interface,
						 CFStringRef		extendedType,
						 Boolean		requirePerInterface);

CFDictionaryRef
__SCNetworkInterfaceGetTemplateOverrides	(SCNetworkInterfaceRef	interface,
						 CFStringRef		interfaceType);

int
__SCNetworkInterfaceOrder			(SCNetworkInterfaceRef	interface);

Boolean
__SCNetworkInterfaceSetConfiguration		(SCNetworkInterfaceRef  interface,
						 CFStringRef		extendedType,
						 CFDictionaryRef	config,
						 Boolean		okToHold);

void
__SCNetworkInterfaceSetDeepConfiguration	(SCNetworkInterfaceRef  interface,
						 CFArrayRef		configs);

Boolean
__SCNetworkInterfaceSupportsVLAN		(CFStringRef		bsd_if);

void
__SCBondInterfaceListCopyMembers		(CFArrayRef 		interfaces,
						 CFMutableSetRef 	set);

#pragma mark -
#pragma mark SCNetworkProtocol configuration (internal)


SCNetworkProtocolPrivateRef
__SCNetworkProtocolCreatePrivate		(CFAllocatorRef		allocator,
						 CFStringRef		entityID,
						 SCNetworkServiceRef	service);

Boolean
__SCNetworkProtocolIsValidType			(CFStringRef		protocolType);


#pragma mark -
#pragma mark SCNetworkService configuration (internal)


SCNetworkServicePrivateRef
__SCNetworkServiceCreatePrivate			(CFAllocatorRef		allocator,
						 SCPreferencesRef	prefs,
						 CFStringRef		serviceID,
						 SCNetworkInterfaceRef	interface);


#pragma mark -
#pragma mark SCNetworkSet configuration (internal)


#pragma mark -
#pragma mark Miscellaneous (internal)


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

CFStringRef
__SCPreferencesPathCreateUniqueChild_WithMoreSCFCompatibility
						(SCPreferencesRef	prefs,
						 CFStringRef		prefix);

Boolean
__extract_password				(SCPreferencesRef	prefs,
						 CFDictionaryRef	config,
						 CFStringRef		passwordKey,
						 CFStringRef		encryptionKey,
						 CFStringRef		encryptionKeyChainValue,
						 CFStringRef		unique_id,
						 CFDataRef		*password);

__END_DECLS

#endif	/* _SCNETWORKCONFIGURATIONINTERNAL_H */
