/*
 * Copyright (c) 2004-2011 Apple Inc. All rights reserved.
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


#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPreferencesPathKey.h>
#include <SystemConfiguration/SCNetworkConfigurationPrivate.h>
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

	// misc
	Boolean			established;

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

	// store (for live SCNetworkService)
	SCDynamicStoreRef	store;

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

	// SCDynamicStore
	SCDynamicStoreRef	store;

	// serviceID (NULL if not associated with a service)
	CFStringRef		serviceID;

	// unsaved configuration (when prefs not [yet] available)
	CFMutableDictionaryRef	unsaved;

	// [SCPreferences] interface entity information
	CFStringRef		entity_device;		// interface device
	CFStringRef		entity_device_unique;	// ... UniqueIdentifier
	CFStringRef		entity_type;		// interface type
	CFStringRef		entity_subtype;		// interface subtype

	// configuration information
	CFMutableArrayRef       supported_interface_types;
	CFMutableArrayRef       supported_protocol_types;

	// IORegistry (service plane) information
	CFDataRef		address;
	CFStringRef		addressString;
	Boolean			builtin;
	CFStringRef		configurationAction;
	Boolean			hidden;
	CFStringRef		location;
	CFStringRef		path;
	uint64_t		entryID;
	CFMutableDictionaryRef	overrides;
	Boolean			modemIsV92;
	CFNumberRef		type;
	CFNumberRef		unit;
	struct {
		CFStringRef	name;
		CFNumberRef	vid;
		CFNumberRef	pid;
	} usb;

	// misc
	int			sort_order;		// sort order for this interface

	// for BOND interfaces
	Boolean			supportsBond;
	struct {
		CFArrayRef		interfaces;
		CFDictionaryRef		options;
		CFNumberRef		mode;
	} bond;

	// for Bridge interfaces
	Boolean			supportsBridge;
	struct {
		CFArrayRef		interfaces;
		CFDictionaryRef		options;
	} bridge;

	// for VLAN interfaces
	Boolean			supportsVLAN;
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
						 CFStringRef		serviceID);

SCNetworkInterfacePrivateRef
_SCBondInterfaceCreatePrivate			(CFAllocatorRef		allocator,
						 CFStringRef		bond_if);

SCNetworkInterfacePrivateRef
_SCBridgeInterfaceCreatePrivate			(CFAllocatorRef		allocator,
						 CFStringRef		bridge_if);

SCNetworkInterfacePrivateRef
_SCVLANInterfaceCreatePrivate			(CFAllocatorRef		allocator,
						 CFStringRef		vlan_if);

CFDictionaryRef
__SCNetworkInterfaceCopyInterfaceEntity		(SCNetworkInterfaceRef	interface);

CFArrayRef
__SCNetworkInterfaceCopyDeepConfiguration       (SCNetworkSetRef	set,
						 SCNetworkInterfaceRef	interface);

#if	!TARGET_OS_EMBEDDED && !TARGET_IPHONE_SIMULATOR
CFStringRef
__SCNetworkInterfaceCopyXLocalizedDisplayName	(SCNetworkInterfaceRef	interface);

CFStringRef
__SCNetworkInterfaceCopyXNonLocalizedDisplayName(SCNetworkInterfaceRef	interface);
#endif	// !TARGET_OS_EMBEDDED && !TARGET_IPHONE_SIMULATOR

int
__SCNetworkInterfaceCreateCapabilities		(SCNetworkInterfaceRef	interface,
						 int			capability_base,
						 CFDictionaryRef	capability_options);

int
__SCNetworkInterfaceCreateMediaOptions		(SCNetworkInterfaceRef	interface,
						 CFDictionaryRef	media_options);

CFStringRef
__SCNetworkInterfaceGetDefaultConfigurationType	(SCNetworkInterfaceRef	interface);

CFStringRef
__SCNetworkInterfaceGetNonLocalizedDisplayName	(SCNetworkInterfaceRef	interface);

Boolean
__SCNetworkInterfaceIsMember			(SCPreferencesRef	prefs,
						 SCNetworkInterfaceRef	interface);

Boolean
__SCNetworkInterfaceIsValidExtendedConfigurationType
						(SCNetworkInterfaceRef	interface,
						 CFStringRef		extendedType,
						 Boolean		requirePerInterface);

CFDictionaryRef
__SCNetworkInterfaceGetTemplateOverrides	(SCNetworkInterfaceRef	interface,
						 CFStringRef		overrideType);

int
__SCNetworkInterfaceOrder			(SCNetworkInterfaceRef	interface);

Boolean
__SCNetworkInterfaceSetConfiguration		(SCNetworkInterfaceRef  interface,
						 CFStringRef		extendedType,
						 CFDictionaryRef	config,
						 Boolean		okToHold);

void
__SCNetworkInterfaceSetDeepConfiguration	(SCNetworkSetRef	set,
						 SCNetworkInterfaceRef	interface,
						 CFArrayRef		configs);

Boolean
__SCNetworkInterfaceSupportsVLAN		(CFStringRef		bsd_if);

void
__SCBondInterfaceListCollectMembers		(CFArrayRef 		interfaces,
						 CFMutableSetRef 	set);

void
__SCBridgeInterfaceListCollectMembers		(CFArrayRef 		interfaces,
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


CFArrayRef /* of SCNetworkServiceRef's */
__SCNetworkServiceCopyAllEnabled		(SCPreferencesRef	prefs);

SCNetworkServicePrivateRef
__SCNetworkServiceCreatePrivate			(CFAllocatorRef		allocator,
						 SCPreferencesRef	prefs,
						 CFStringRef		serviceID,
						 SCNetworkInterfaceRef	interface);

Boolean
__SCNetworkServiceExistsForInterface		(CFArrayRef		services,
						 SCNetworkInterfaceRef	interface);

CF_RETURNS_RETAINED
CFStringRef
__SCNetworkServiceNextName			(SCNetworkServiceRef	service);


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

Boolean
__remove_password				(SCPreferencesRef	prefs,
						 CFDictionaryRef	config,
						 CFStringRef		passwordKey,
						 CFStringRef		encryptionKey,
						 CFStringRef		encryptionKeyChainValue,
						 CFStringRef		unique_id,
						 CFDictionaryRef	*newConfig);

Boolean
__rank_to_str					(SCNetworkServicePrimaryRank	rank,
						 CFStringRef			*rankStr);

Boolean
__str_to_rank					(CFStringRef			rankStr,
						 SCNetworkServicePrimaryRank	*rank);

__END_DECLS

#endif	/* _SCNETWORKCONFIGURATIONINTERNAL_H */
