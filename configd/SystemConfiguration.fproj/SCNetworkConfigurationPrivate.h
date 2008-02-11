/*
 * Copyright (c) 2005-2007 Apple Inc. All rights reserved.
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

#ifndef _SCNETWORKCONFIGURATIONPRIVATE_H
#define _SCNETWORKCONFIGURATIONPRIVATE_H

#include <AvailabilityMacros.h>
#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <IOKit/IOKitLib.h>

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1050

/*!
	@header SCNetworkConfigurationPrivate
 */

__BEGIN_DECLS


#pragma mark -
#pragma mark SCNetworkInterface configuration (SPI)


/*!
	@group Interface configuration
 */

static __inline__ CFTypeRef
isA_SCNetworkInterface(CFTypeRef obj)
{
	return (isA_CFType(obj, SCNetworkInterfaceGetTypeID()));
}

static __inline__ CFTypeRef
isA_SCBondInterface(CFTypeRef obj)
{
	CFStringRef	interfaceType;

	if (!isA_SCNetworkInterface(obj)) {
		// if not an SCNetworkInterface
		return NULL;
	}

	interfaceType = SCNetworkInterfaceGetInterfaceType((SCNetworkInterfaceRef)obj);
	if (!CFEqual(interfaceType, kSCNetworkInterfaceTypeBond)) {
		// if not a Bond
		return NULL;
	}

	return obj;
}

static __inline__ CFTypeRef
isA_SCVLANInterface(CFTypeRef obj)
{
	CFStringRef	interfaceType;

	if (!isA_SCNetworkInterface(obj)) {
		// if not an SCNetworkInterface
		return NULL;
	}

	interfaceType = SCNetworkInterfaceGetInterfaceType((SCNetworkInterfaceRef)obj);
	if (!CFEqual(interfaceType, kSCNetworkInterfaceTypeVLAN)) {
		// if not a VLAN
		return NULL;
	}

	return obj;
}

/*!
	@function _SCNetworkInterfaceCompare
	@discussion Compares two SCNetworkInterface objects.
	@param val1 The SCNetworkInterface object.
	@param val2 The SCNetworkInterface object.
	@param context Not used.
	@result A comparison result.
 */
CFComparisonResult
_SCNetworkInterfaceCompare				(const void			*val1,
							 const void			*val2,
							 void				*context)	AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

#define kIncludeNoVirtualInterfaces	0x0
#define kIncludeVLANInterfaces		0x1
#define kIncludeBondInterfaces		0x2
#define kIncludeAllVirtualInterfaces	0xffffffff

/*!
	@function _SCNetworkInterfaceCreateWithBSDName
	@discussion Create a new network interface associated with the provided
		BSD interface name.  This API supports Ethhernet, FireWire, and
		IEEE 802.11 interfaces.
	@param bsdName The BSD interface name.
	@param flags Indicates whether virtual (Bond, VLAN)
		network interfaces should be included.
	@result A reference to the new SCNetworkInterface.
		You must release the returned value.
 */
SCNetworkInterfaceRef
_SCNetworkInterfaceCreateWithBSDName			(CFAllocatorRef			allocator,
							 CFStringRef			bsdName,
							 UInt32				flags)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
	@function _SCNetworkInterfaceCreateWithEntity
	@discussion Create a new network interface associated with the provided
		SCDynamicStore service entity dictionary.
	@param interface_entity The entity dictionary.
	@param service The network service.
	@result A reference to the new SCNetworkInterface.
		You must release the returned value.
 */
SCNetworkInterfaceRef
_SCNetworkInterfaceCreateWithEntity			(CFAllocatorRef			allocator,
							 CFDictionaryRef		interface_entity,
							 SCNetworkServiceRef		service)	AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
	@function _SCNetworkInterfaceCreateWithIONetworkInterfaceObject
	@discussion Create a new network interface associated with the provided
		IORegistry "IONetworkInterface" object.
	@param if_obj The IONetworkInterface object.
	@result A reference to the new SCNetworkInterface.
		You must release the returned value.
 */
SCNetworkInterfaceRef
_SCNetworkInterfaceCreateWithIONetworkInterfaceObject	(io_object_t			if_obj)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
	@function _SCNetworkInterfaceGetHardwareAddress
	@discussion Returns a link layer address for the interface.
	@param interface The network interface.
	@result The hardware (MAC) address for the interface.
		NULL if no hardware address is available.
 */
CFDataRef
_SCNetworkInterfaceGetHardwareAddress			(SCNetworkInterfaceRef		interface)	AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
	@function _SCNetworkInterfaceGetIOInterfaceType
	@discussion Returns the IOInterfaceType for the interface.
	@param interface The network interface.
	@result The IOInterfaceType associated with the interface
 */
CFNumberRef
_SCNetworkInterfaceGetIOInterfaceType			(SCNetworkInterfaceRef		interface)	AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
	@function _SCNetworkInterfaceGetIOInterfaceUnit
	@discussion Returns the IOInterfaceUnit for the interface.
	@param interface The network interface.
	@result The IOInterfaceUnit associated with the interface;
		NULL if no IOLocation is available.
 */
CFNumberRef
_SCNetworkInterfaceGetIOInterfaceUnit			(SCNetworkInterfaceRef		interface)	AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
	@function _SCNetworkInterfaceGetIOPath
	@discussion Returns the IOPath for the interface.
	@param interface The network interface.
	@result The IOPath associated with the interface;
		NULL if no IOPath is available.
 */
CFStringRef
_SCNetworkInterfaceGetIOPath				(SCNetworkInterfaceRef		interface)	AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
	@function _SCNetworkInterfaceIsBuiltin
	@discussion Identifies if a network interface is "built-in".
	@param interface The network interface.
	@result TRUE if the interface is "built-in".
 */
Boolean
_SCNetworkInterfaceIsBuiltin				(SCNetworkInterfaceRef		interface)	AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
	@function _SCNetworkInterfaceIsModemV92
	@discussion Identifies if a modem network interface supports
		v.92 (hold).
	@param interface The network interface.
	@result TRUE if the interface is "v.92" modem.
 */
Boolean
_SCNetworkInterfaceIsModemV92				(SCNetworkInterfaceRef		interface)	AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
	@function _SCNetworkInterfaceForceConfigurationRefresh
	@discussion Forces a configuration refresh of the
		specified interface.
	@param ifName Network interface name.
	@result TRUE if the refresh was successfully posted.
 */
Boolean
_SCNetworkInterfaceForceConfigurationRefresh		(CFStringRef			ifName)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
	@function _SCBondInterfaceCopyActive
	@discussion Returns all Ethernet Bond interfaces on the system.
	@result The list of SCBondInterface interfaces on the system.
		You must release the returned value.
 */
CFArrayRef
_SCBondInterfaceCopyActive				(void)						AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
	@function _SCBondInterfaceUpdateConfiguration
	@discussion Updates the bond interface configuration.
	@param prefs The "preferences" session.
	@result TRUE if the bond interface configuration was updated.; FALSE if the
		an error was encountered.
 */
Boolean
_SCBondInterfaceUpdateConfiguration			(SCPreferencesRef		prefs)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
	@function SCBondInterfaceSetMode
	@discussion Set the mode on the bond interface.
	@param bond The bond interface on which to adjust the mode.
	@param mode The mode value (0=IF_BOND_MODE_LACP,1=IF_BOND_MODE_STATIC)
	@result TRUE if operation succeeded.
 */
Boolean
SCBondInterfaceSetMode					(SCBondInterfaceRef		bond,
							 CFNumberRef			mode)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
	@function SCBondInterfaceSetMode
	@discussion Return the mode for the given bond interface.
	@param bond The bond interface to get the mode from.
	@result A CFNumberRef containing the mode (IF_BOND_MODE_{LACP,STATIC}).
 */
CFNumberRef
SCBondInterfaceGetMode					(SCBondInterfaceRef		bond)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
	@function _SCVLANInterfaceCopyActive
	@discussion Returns all VLAN interfaces on the system.
	@result The list of SCVLANInterface interfaces on the system.
		You must release the returned value.
 */
CFArrayRef
_SCVLANInterfaceCopyActive				(void)						AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
	@function _SCVLANInterfaceUpdateConfiguration
	@discussion Updates the VLAN interface configuration.
	@param prefs The "preferences" session.
	@result TRUE if the VLAN interface configuration was updated.; FALSE if the
		an error was encountered.
 */
Boolean
_SCVLANInterfaceUpdateConfiguration			(SCPreferencesRef		prefs)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;


#pragma mark -
#pragma mark SCNetworkInterface Password SPIs


enum {
	kSCNetworkInterfacePasswordTypePPP		= 1,
	kSCNetworkInterfacePasswordTypeIPSecSharedSecret,
	kSCNetworkInterfacePasswordTypeEAPOL,
};
typedef uint32_t	SCNetworkInterfacePasswordType;

Boolean
SCNetworkInterfaceCheckPassword				(SCNetworkInterfaceRef		interface,
							 SCNetworkInterfacePasswordType	passwordType)	AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

CFDataRef
SCNetworkInterfaceCopyPassword				(SCNetworkInterfaceRef		interface,
							 SCNetworkInterfacePasswordType	passwordType)	AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

Boolean
SCNetworkInterfaceRemovePassword			(SCNetworkInterfaceRef		interface,
							 SCNetworkInterfacePasswordType	passwordType)	AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

Boolean
SCNetworkInterfaceSetPassword				(SCNetworkInterfaceRef		interface,
							 SCNetworkInterfacePasswordType	passwordType,
							 CFDataRef			password,
							 CFDictionaryRef		options)	AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;


#pragma mark -
#pragma mark SCNetworkProtocol configuration (SPI)


/*!
	@group Protocol configuration
 */


static __inline__ CFTypeRef
isA_SCNetworkProtocol(CFTypeRef obj)
{
	return (isA_CFType(obj, SCNetworkProtocolGetTypeID()));
}


#pragma mark -
#pragma mark SCNetworkService configuration (SPI)


/*!
	@group Service configuration
 */


static __inline__ CFTypeRef
isA_SCNetworkService(CFTypeRef obj)
{
	return (isA_CFType(obj, SCNetworkServiceGetTypeID()));
}


#pragma mark -
#pragma mark SCNetworkSet configuration (SPI)


/*!
	@group Set configuration
 */


static __inline__ CFTypeRef
isA_SCNetworkSet(CFTypeRef obj)
{
	return (isA_CFType(obj, SCNetworkSetGetTypeID()));
}


/*!
	@function SCNetworkSetEstablishDefaultConfiguration
	@discussion Updates a network set by adding services for
		any network interface that is not currently
		represented.
		If the provided set contains one (or more) services, new
		services will only be added for those interfaces that are
		not represented in *any* set.
		Otherwise, new services will be added for those interfaces
		that are not represented in the provided set.
		The new services are established with "default" configuration
		options.
	@param set The network set.
	@result TRUE if the configuration was updated; FALSE if no
		changes were required or if an error was encountered.
*/
Boolean
SCNetworkSetEstablishDefaultConfiguration		(SCNetworkSetRef		set);

/*!
	@function SCNetworkSetEstablishDefaultInterfaceConfiguration
	@discussion Updates a network set by adding services for
		the specified network interface if is not currently
		represented.
		If the provided set contains one (or more) services, new
		services will only be added for interfaces that are not
		represented in *any* set.
		Otherwise, new services will be added for interfaces that
		are not represented in the provided set.
		The new services are established with "default" configuration
		options.
	@param set The network set.
	@param interface The network interface.
	@result TRUE if the configuration was updated; FALSE if no
		changes were required or if an error was encountered.
 */
Boolean
SCNetworkSetEstablishDefaultInterfaceConfiguration	(SCNetworkSetRef		set,
							 SCNetworkInterfaceRef		interface);

__END_DECLS

#endif	/* MAC_OS_X_VERSION_MAX_ALLOWED >= 1050 */

#endif	/* _SCNETWORKCONFIGURATIONPRIVATE_H */
