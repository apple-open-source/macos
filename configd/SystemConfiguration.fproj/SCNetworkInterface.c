/*
 * Copyright (c) 2004-2009 Apple Inc. All rights reserved.
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
 * May 13, 2004		Allan Nathanson <ajn@apple.com>
 * - initial revision
 *	which includes code originally authored by
 *	  Robert Ulrich		<rulrich@apple.com>
 *	  Elizabeth Douglas	<elizabeth@apple.com>
 *	  Quinn			<eskimo1@apple.com>
 */


#include <Availability.h>
#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include "SCNetworkConfigurationInternal.h"
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCPreferencesInternal.h"
#include "SCHelper_client.h"

#if	!TARGET_OS_IPHONE
#include <EAP8021X/EAPClientProperties.h>
#else	// !TARGET_OS_IPHONE
#ifndef	kEAPClientPropUserName
#define kEAPClientPropUserName CFSTR("UserName")
#endif
#ifndef	kEAPClientPropUserPasswordKeychainItemID
#define kEAPClientPropUserPasswordKeychainItemID CFSTR("UserPasswordKeychainItemID")
#endif
#endif	// !TARGET_OS_IPHONE

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFBundle.h>
#include <IOKit/IOBSD.h>
#include <IOKit/network/IONetworkController.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOEthernetInterface.h>	// for kIOEthernetInterfaceClass
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/usb/USB.h>

#include "dy_framework.h"

#ifndef	kIODeviceSupportsHoldKey
#define	kIODeviceSupportsHoldKey	"V92Modem"
#endif

#ifndef	kUSBProductString
#define kUSBProductString		"USB Product Name"
#endif

#include <string.h>
#include <mach/mach.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <pthread.h>
#include <NSSystemDirectories.h>


static CFStringRef	copy_interface_string			(CFBundleRef bundle, CFStringRef key, Boolean localized);
static CFStringRef	__SCNetworkInterfaceCopyDescription	(CFTypeRef cf);
static void		__SCNetworkInterfaceDeallocate		(CFTypeRef cf);
static Boolean		__SCNetworkInterfaceEqual		(CFTypeRef cf1, CFTypeRef cf2);
static CFHashCode	__SCNetworkInterfaceHash		(CFTypeRef cf);


enum {
	kSortInternalModem,
	kSortUSBModem,
	kSortModem,
	kSortBluetooth,
	kSortIrDA,
	kSortSerialPort,
	kSortWWAN,
	kSortEthernetPPP,
	kSortAirportPPP,
	kSortEthernet,
	kSortFireWire,
	kSortAirPort,
	kSortOtherWireless,
	kSortTethered,
	kSortWWANEthernet,
	kSortBluetoothPAN,
#if	!TARGET_OS_IPHONE
	kSortBond,
	kSortVLAN,
#endif	// !TARGET_OS_IPHONE
	kSortUnknown
};


const CFStringRef kSCNetworkInterfaceType6to4		= CFSTR("6to4");
const CFStringRef kSCNetworkInterfaceTypeBluetooth	= CFSTR("Bluetooth");
#if	!TARGET_OS_IPHONE
const CFStringRef kSCNetworkInterfaceTypeBond		= CFSTR("Bond");
#endif	// !TARGET_OS_IPHONE
const CFStringRef kSCNetworkInterfaceTypeEthernet	= CFSTR("Ethernet");
const CFStringRef kSCNetworkInterfaceTypeFireWire	= CFSTR("FireWire");
const CFStringRef kSCNetworkInterfaceTypeIEEE80211	= CFSTR("IEEE80211");	// IEEE 802.11, AirPort
const CFStringRef kSCNetworkInterfaceTypeIPSec		= CFSTR("IPSec");
const CFStringRef kSCNetworkInterfaceTypeIrDA		= CFSTR("IrDA");
const CFStringRef kSCNetworkInterfaceTypeL2TP		= CFSTR("L2TP");
const CFStringRef kSCNetworkInterfaceTypeModem		= CFSTR("Modem");
const CFStringRef kSCNetworkInterfaceTypePPP		= CFSTR("PPP");
const CFStringRef kSCNetworkInterfaceTypePPTP		= CFSTR("PPTP");
const CFStringRef kSCNetworkInterfaceTypeSerial		= CFSTR("Serial");
#if	!TARGET_OS_IPHONE
const CFStringRef kSCNetworkInterfaceTypeVLAN		= CFSTR("VLAN");
#endif	// !TARGET_OS_IPHONE
const CFStringRef kSCNetworkInterfaceTypeWWAN		= CFSTR("WWAN");

const CFStringRef kSCNetworkInterfaceTypeIPv4		= CFSTR("IPv4");

static SCNetworkInterfacePrivate __kSCNetworkInterfaceIPv4      = {
	INIT_CFRUNTIME_BASE(NULL, 0, 0x0080),   // cfBase
	NULL,					// interface type
	NULL,					// name
	NULL,					// localized name
	NULL,					// localization key
	NULL,					// localization arg1
	NULL,					// localization arg2
	NULL,					// [layered] interface
	NULL,					// prefs
	NULL,					// serviceID
	NULL,					// unsaved
	NULL,					// entity_device
	NULL,					// entity_device_unique
	NULL,					// entity_type
	NULL,					// entity_subtype
	NULL,					// supported_interface_types
	NULL,					// supported_protocol_types
	NULL,					// address
	NULL,					// addressString
	FALSE,					// builtin
	NULL,					// configurationAction
	NULL,					// location
	NULL,					// path
	NULL,					// overrides
	FALSE,					// modemIsV92
	NULL,					// type
	NULL,					// unit
	kSortUnknown,				// sort_order
#if	!TARGET_OS_IPHONE
	FALSE,					// supportsBond
	{ NULL, NULL},				// bond { interfaces, options }
	FALSE,					// supportsVLAN
	{ NULL, NULL, NULL }			// vlan { interface, tag, options }
#endif	// !TARGET_OS_IPHONE
};

const SCNetworkInterfaceRef kSCNetworkInterfaceIPv4     = (SCNetworkInterfaceRef)&__kSCNetworkInterfaceIPv4;

#pragma mark -
#pragma mark SCNetworkInterface configuration details

#define doNone		0

#define do6to4		1<<0
#define doL2TP		1<<1
#define doPPP		1<<2
#define doPPTP		1<<3
#define doIPSec		1<<4
#define doOverIP	do6to4|doL2TP|doPPTP|doIPSec

#if	!TARGET_OS_IPHONE
#define doAppleTalk     1<<0
#else	// !TARGET_OS_IPHONE
#define doAppleTalk     0
#endif	// !TARGET_OS_IPHONE
#define doDNS		1<<1
#define doIPv4		1<<2
#define doIPv6		1<<3
#define doProxies       1<<4
#if	!TARGET_OS_IPHONE
#define doSMB		1<<5
#else	// !TARGET_OS_IPHONE
#define doSMB		0
#endif	// !TARGET_OS_IPHONE

static const struct {
	const CFStringRef       *interface_type;
	const CFStringRef	*entity_hardware;
	Boolean			per_interface_config;
	uint32_t		supported_interfaces;
	const CFStringRef       *ppp_subtype;
	uint32_t		supported_protocols;
} configurations[] = {
	// interface type			  entity_hardware    if config? interface types PPP sub-type				interface protocols
	// =====================================  =================  ========== =============== ======================================= =========================================
	{ &kSCNetworkInterfaceType6to4		, &kSCEntNet6to4    , FALSE,	doNone,		NULL,					doIPv6						},
	{ &kSCNetworkInterfaceTypeBluetooth     , &kSCEntNetModem   , FALSE,	doPPP,		&kSCValNetInterfaceSubTypePPPSerial,    doNone						},
#if	!TARGET_OS_IPHONE
	{ &kSCNetworkInterfaceTypeBond		, &kSCEntNetEthernet, TRUE ,	doNone,		NULL,					doAppleTalk|doDNS|doIPv4|doIPv6|doProxies|doSMB	},
#endif	// !TARGET_OS_IPHONE
	{ &kSCNetworkInterfaceTypeEthernet      , &kSCEntNetEthernet, TRUE ,	doPPP,		&kSCValNetInterfaceSubTypePPPoE,	doAppleTalk|doDNS|doIPv4|doIPv6|doProxies|doSMB	},
	{ &kSCNetworkInterfaceTypeFireWire      , &kSCEntNetFireWire, TRUE ,	doNone,		NULL,					doDNS|doIPv4|doIPv6|doProxies|doSMB		},
	{ &kSCNetworkInterfaceTypeIEEE80211     , &kSCEntNetAirPort , TRUE ,	doPPP,		&kSCValNetInterfaceSubTypePPPoE,	doAppleTalk|doDNS|doIPv4|doIPv6|doProxies|doSMB	},
	{ &kSCNetworkInterfaceTypeIPSec		, &kSCEntNetIPSec   , FALSE,	doNone,		NULL,					doDNS|doIPv4|doIPv6|doProxies|doSMB		},
	{ &kSCNetworkInterfaceTypeIrDA		, &kSCEntNetModem   , FALSE,	doPPP,		&kSCValNetInterfaceSubTypePPPSerial,    doNone						},
	{ &kSCNetworkInterfaceTypeL2TP		, NULL              , FALSE,	doPPP,		&kSCValNetInterfaceSubTypeL2TP,		doNone						},
	{ &kSCNetworkInterfaceTypeModem		, &kSCEntNetModem   , FALSE,	doPPP,		&kSCValNetInterfaceSubTypePPPSerial,    doNone						},
	{ &kSCNetworkInterfaceTypePPP		, &kSCEntNetPPP     , FALSE,	doNone,		NULL,					doDNS|doIPv4|doIPv6|doProxies|doSMB		},
	{ &kSCNetworkInterfaceTypePPTP		, NULL              , FALSE,	doPPP,		&kSCValNetInterfaceSubTypePPTP,		doNone						},
	{ &kSCNetworkInterfaceTypeSerial	, &kSCEntNetModem   , FALSE,	doPPP,		&kSCValNetInterfaceSubTypePPPSerial,    doNone						},
#if	!TARGET_OS_IPHONE
	{ &kSCNetworkInterfaceTypeVLAN		, &kSCEntNetEthernet, TRUE ,	doNone,		NULL,					doAppleTalk|doDNS|doIPv4|doIPv6|doProxies|doSMB	},
#endif	// !TARGET_OS_IPHONE
	{ &kSCNetworkInterfaceTypeWWAN          , &kSCEntNetModem   , FALSE,	doPPP,		&kSCValNetInterfaceSubTypePPPSerial,    doNone						},
	// =====================================  =================  ========== =============== ======================================= =========================================
	{ &kSCNetworkInterfaceTypeIPv4		, NULL              , FALSE,	doOverIP,	NULL,					doNone						}
};


#define	NETWORKINTERFACE_LOCALIZATIONS	CFSTR("NetworkInterface")
static CFBundleRef bundle			= NULL;


static CFTypeID __kSCNetworkInterfaceTypeID	= _kCFRuntimeNotATypeID;


static const CFRuntimeClass __SCNetworkInterfaceClass = {
	0,					// version
	"SCNetworkInterface",			// className
	NULL,					// init
	NULL,					// copy
	__SCNetworkInterfaceDeallocate,		// dealloc
	__SCNetworkInterfaceEqual,		// equal
	__SCNetworkInterfaceHash,		// hash
	NULL,					// copyFormattingDesc
	__SCNetworkInterfaceCopyDescription	// copyDebugDesc
};


static pthread_once_t		initialized	= PTHREAD_ONCE_INIT;
static pthread_once_t		iokit_quiet	= PTHREAD_ONCE_INIT;


static mach_port_t		masterPort	= MACH_PORT_NULL;


static CFStringRef
__SCNetworkInterfaceCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef			allocator		= CFGetAllocator(cf);
	CFMutableStringRef		result;
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)cf;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<SCNetworkInterface %p [%p]> {"), cf, allocator);
	CFStringAppendFormat(result, NULL, CFSTR("type = %@"), interfacePrivate->interface_type);
	CFStringAppendFormat(result, NULL, CFSTR(", entity_device = %@"), interfacePrivate->entity_device);
	if (interfacePrivate->entity_device_unique != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR("+%@"), interfacePrivate->entity_device_unique);
	}
	CFStringAppendFormat(result, NULL, CFSTR(", entity_type = %@"), interfacePrivate->entity_type);
	if (interfacePrivate->entity_subtype != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(" / %@"), interfacePrivate->entity_subtype);
	}
	if (interfacePrivate->name != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", name = %@"), interfacePrivate->name);
	}
	if (interfacePrivate->localized_name != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", name(l) = %@"), interfacePrivate->localized_name);
	} else {
		if (interfacePrivate->localized_key != NULL) {
			CFStringAppendFormat(result, NULL, CFSTR(", name(k) = \"%@\""), interfacePrivate->localized_key);
			if (interfacePrivate->localized_arg1 != NULL) {
				CFStringAppendFormat(result, NULL, CFSTR("+\"%@\""), interfacePrivate->localized_arg1);
			}
			if (interfacePrivate->localized_arg2 != NULL) {
				CFStringAppendFormat(result, NULL, CFSTR("+\"%@\""), interfacePrivate->localized_arg2);
			}
		}
	}
	if (interfacePrivate->address != NULL) {
		const uint8_t		*data;
		CFIndex			dataLen;
		CFIndex			i;

		CFStringAppendFormat(result, NULL, CFSTR(", address = 0x"));

		data    = CFDataGetBytePtr(interfacePrivate->address);
		dataLen = CFDataGetLength(interfacePrivate->address);
		for (i = 0; i < dataLen; i++) {
			CFStringAppendFormat(result, NULL, CFSTR("%02x"), data[i]);
		}
	}
	CFStringAppendFormat(result, NULL, CFSTR(", builtin = %s"), interfacePrivate->builtin ? "TRUE" : "FALSE");
	if (interfacePrivate->modemIsV92) {
		CFStringAppendFormat(result, NULL, CFSTR(", v.92"));
	}
	if (interfacePrivate->location != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", location = %@"), interfacePrivate->location);
	}
	if (interfacePrivate->type != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", type = %@"), interfacePrivate->type);
	}
	if (interfacePrivate->unit != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", unit = %@"), interfacePrivate->unit);
	}
	if (interfacePrivate->path != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", path = %@"), interfacePrivate->path);
	}
	if (interfacePrivate->configurationAction != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", action = %@"), interfacePrivate->configurationAction);
	}
	if (interfacePrivate->overrides != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", overrides = %p"), interfacePrivate->overrides);
	}
	CFStringAppendFormat(result, NULL, CFSTR(", order = %d"), interfacePrivate->sort_order);
	if (interfacePrivate->prefs != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", prefs = %p"), interfacePrivate->prefs);
	}
	if (interfacePrivate->serviceID != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", service = %@"), interfacePrivate->serviceID);
	}
	if (interfacePrivate->interface != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", interface = %@"), interfacePrivate->interface);
	}
	if (interfacePrivate->unsaved != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", unsaved = %@"), interfacePrivate->unsaved);
	}
#if	!TARGET_OS_IPHONE
	if (interfacePrivate->bond.interfaces != NULL) {
		CFIndex	i;
		CFIndex	n;

		n = CFArrayGetCount(interfacePrivate->bond.interfaces);
		for (i = 0; i < n; i++) {
			SCNetworkInterfaceRef	member;

			member = CFArrayGetValueAtIndex(interfacePrivate->bond.interfaces, i);
			CFStringAppendFormat(result, NULL,
					     CFSTR("%s%@"),
					     (i == 0) ? ", interfaces = " : ", ",
					     SCNetworkInterfaceGetBSDName(member));
		}
	}
	if (interfacePrivate->bond.mode != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", mode = %@"), interfacePrivate->bond.mode);
	}
	if (interfacePrivate->bond.options != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", options = %@"), interfacePrivate->bond.options);
	}
	if (interfacePrivate->bond.mode != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", mode = %@"), interfacePrivate->bond.mode);
	}
	if (interfacePrivate->vlan.interface != NULL) {
		CFStringAppendFormat(result, NULL,
				     CFSTR(", interface = %@"),
				     SCNetworkInterfaceGetBSDName(interfacePrivate->vlan.interface));
	}
	if (interfacePrivate->vlan.tag != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", tag = %@"), interfacePrivate->vlan.tag);
	}
	if (interfacePrivate->vlan.options != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", options = %@"), interfacePrivate->vlan.options);
	}
#endif	// !TARGET_OS_IPHONE
	CFStringAppendFormat(result, NULL, CFSTR("}"));

	return result;
}


static void
__SCNetworkInterfaceDeallocate(CFTypeRef cf)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)cf;

	/* release resources */

	if (interfacePrivate->interface != NULL)
		CFRelease(interfacePrivate->interface);

	if (interfacePrivate->name != NULL)
		CFRelease(interfacePrivate->name);

	if (interfacePrivate->localized_name != NULL)
		CFRelease(interfacePrivate->localized_name);

	if (interfacePrivate->localized_arg1 != NULL)
		CFRelease(interfacePrivate->localized_arg1);

	if (interfacePrivate->localized_arg2 != NULL)
		CFRelease(interfacePrivate->localized_arg2);

	if (interfacePrivate->prefs != NULL)
		CFRelease(interfacePrivate->prefs);

	if (interfacePrivate->serviceID != NULL)
		CFRelease(interfacePrivate->serviceID);

	if (interfacePrivate->unsaved != NULL)
		CFRelease(interfacePrivate->unsaved);

	if (interfacePrivate->entity_device != NULL)
		CFRelease(interfacePrivate->entity_device);

	if (interfacePrivate->entity_device_unique != NULL)
		CFRelease(interfacePrivate->entity_device_unique);

	if (interfacePrivate->supported_interface_types != NULL)
		CFRelease(interfacePrivate->supported_interface_types);

	if (interfacePrivate->supported_protocol_types != NULL)
		CFRelease(interfacePrivate->supported_protocol_types);

	if (interfacePrivate->address != NULL)
		CFRelease(interfacePrivate->address);

	if (interfacePrivate->addressString != NULL)
		CFRelease(interfacePrivate->addressString);

	if (interfacePrivate->location != NULL)
		CFRelease(interfacePrivate->location);

	if (interfacePrivate->path != NULL)
		CFRelease(interfacePrivate->path);

	if (interfacePrivate->configurationAction != NULL)
		CFRelease(interfacePrivate->configurationAction);

	if (interfacePrivate->overrides != NULL)
		CFRelease(interfacePrivate->overrides);

	if (interfacePrivate->type != NULL)
		CFRelease(interfacePrivate->type);

	if (interfacePrivate->unit != NULL)
		CFRelease(interfacePrivate->unit);

#if	!TARGET_OS_IPHONE
	if (interfacePrivate->bond.interfaces != NULL)
		CFRelease(interfacePrivate->bond.interfaces);

	if (interfacePrivate->bond.mode != NULL)
		CFRelease(interfacePrivate->bond.mode);

	if (interfacePrivate->bond.options != NULL)
		CFRelease(interfacePrivate->bond.options);

	if (interfacePrivate->vlan.interface != NULL)
		CFRelease(interfacePrivate->vlan.interface);

	if (interfacePrivate->vlan.tag != NULL)
		CFRelease(interfacePrivate->vlan.tag);

	if (interfacePrivate->vlan.options != NULL)
		CFRelease(interfacePrivate->vlan.options);
#endif	// !TARGET_OS_IPHONE

	return;
}


static Boolean
__SCNetworkInterfaceEqual(CFTypeRef cf1, CFTypeRef cf2)
{
	SCNetworkInterfacePrivateRef	if1	= (SCNetworkInterfacePrivateRef)cf1;
	SCNetworkInterfacePrivateRef	if2	= (SCNetworkInterfacePrivateRef)cf2;

	if (if1 == if2)
		return TRUE;

	if (!CFEqual(if1->interface_type, if2->interface_type)) {
		return FALSE;	// if not the same interface type
	}

	if (!_SC_CFEqual(if1->entity_device, if2->entity_device)) {
		return FALSE; // if not the same device
	}

	if ((if1->entity_device_unique != NULL) && (if2->entity_device_unique != NULL)) {
		if (!_SC_CFEqual(if1->entity_device_unique, if2->entity_device_unique)) {
			return FALSE; // if not the same device unique identifier
		}
	} else if ((if1->entity_device_unique != NULL) || (if2->entity_device_unique != NULL)) {
		CFStringRef	name1;
		CFStringRef	name2;

		name1 = __SCNetworkInterfaceGetNonLocalizedDisplayName((SCNetworkInterfaceRef)if1);
		name2 = __SCNetworkInterfaceGetNonLocalizedDisplayName((SCNetworkInterfaceRef)if2);
		if ((name1 != NULL) && (name2 != NULL) && !_SC_CFEqual(name1, name2)) {
			return FALSE; // if same device but not the same display name
		}
	}

#if	!TARGET_OS_IPHONE
	if (CFEqual(if1->interface_type, kSCNetworkInterfaceTypeBond)) {
		if (!_SC_CFEqual(if1->bond.interfaces, if2->bond.interfaces)) {
			return FALSE; // if not the same interfaces
		}
		if (!_SC_CFEqual(if1->bond.mode, if2->bond.mode)) {
			return FALSE; // if not the same mode
		}
	}

	if (CFEqual(if1->interface_type, kSCNetworkInterfaceTypeVLAN)) {
		if (!_SC_CFEqual(if1->vlan.interface, if2->vlan.interface)) {
			return FALSE;	// if not the same physical interface
		}
		if (!_SC_CFEqual(if1->vlan.tag, if2->vlan.tag)) {
			return FALSE;	// if not the same tag
		}
	}
#endif	// !TARGET_OS_IPHONE

	if (!_SC_CFEqual(if1->interface, if2->interface)) {
		return FALSE;	// if not the same layering
	}

	return TRUE;
}


static CFHashCode
__SCNetworkInterfaceHash(CFTypeRef cf)
{
	CFHashCode			hash			= 0;
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)cf;

	if (interfacePrivate->entity_device != NULL) {
		if (interfacePrivate->entity_device_unique == NULL) {
			hash = CFHash(interfacePrivate->entity_device);
		} else {
			CFStringRef	str;

			str = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@+%@"),
						       interfacePrivate->entity_device,
						       interfacePrivate->entity_device_unique);
			hash = CFHash(str);
			CFRelease(str);
		}
	}

	return hash;
}


static void
__SCNetworkInterfaceInitialize(void)
{
	kern_return_t   kr;

	// register w/CF
	__kSCNetworkInterfaceTypeID = _CFRuntimeRegisterClass(&__SCNetworkInterfaceClass);

	// initialize __kSCNetworkInterfaceIPv4
	_CFRuntimeSetInstanceTypeID(&__kSCNetworkInterfaceIPv4, __kSCNetworkInterfaceTypeID);
	__kSCNetworkInterfaceIPv4.interface_type = kSCNetworkInterfaceTypeIPv4;
	__kSCNetworkInterfaceIPv4.localized_key  = CFSTR("ipv4");

	// get CFBundleRef for SystemConfiguration.framework
	bundle = _SC_CFBundleGet();

	// get mach port used to communication with IOKit
	kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
	if (kr != KERN_SUCCESS) {
		SCLog(TRUE, LOG_DEBUG,
		      CFSTR("__SCNetworkInterfaceInitialize(), could not get IOMasterPort, kr = 0x%x"),
		      kr);
	}

	return;
}


__private_extern__
SCNetworkInterfacePrivateRef
__SCNetworkInterfaceCreatePrivate(CFAllocatorRef	allocator,
				  SCNetworkInterfaceRef	interface,
				  SCPreferencesRef	prefs,
				  CFStringRef		serviceID,
				  io_string_t		path)
{
	SCNetworkInterfacePrivateRef		interfacePrivate;
	uint32_t				size;

	/* initialize runtime */
	pthread_once(&initialized, __SCNetworkInterfaceInitialize);

	/* allocate target */
	size             = sizeof(SCNetworkInterfacePrivate) - sizeof(CFRuntimeBase);
	interfacePrivate = (SCNetworkInterfacePrivateRef)_CFRuntimeCreateInstance(allocator,
										  __kSCNetworkInterfaceTypeID,
										  size,
										  NULL);
	if (interfacePrivate == NULL) {
		return NULL;
	}

	interfacePrivate->interface_type		= NULL;
	interfacePrivate->name				= NULL;
	interfacePrivate->localized_name		= NULL;
	interfacePrivate->localized_key			= NULL;
	interfacePrivate->localized_arg1		= NULL;
	interfacePrivate->localized_arg2		= NULL;
	interfacePrivate->interface			= (interface != NULL) ? CFRetain(interface) : NULL;
	interfacePrivate->prefs				= (prefs     != NULL) ? CFRetain(prefs)     : NULL;
	interfacePrivate->serviceID			= (serviceID != NULL) ? CFRetain(serviceID) : NULL;
	interfacePrivate->unsaved			= NULL;
	interfacePrivate->entity_device			= NULL;
	interfacePrivate->entity_device_unique		= NULL;
	interfacePrivate->entity_type			= NULL;
	interfacePrivate->entity_subtype		= NULL;
	interfacePrivate->supported_interface_types     = NULL;
	interfacePrivate->supported_protocol_types      = NULL;
	interfacePrivate->address			= NULL;
	interfacePrivate->addressString			= NULL;
	interfacePrivate->builtin			= FALSE;
	interfacePrivate->configurationAction		= NULL;
	interfacePrivate->path				= (path != NULL) ? CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8)
									 : NULL;
	interfacePrivate->location			= NULL;
	interfacePrivate->overrides			= NULL;
	interfacePrivate->modemIsV92			= FALSE;
	interfacePrivate->type				= NULL;
	interfacePrivate->unit				= NULL;
	interfacePrivate->sort_order			= kSortUnknown;
#if	!TARGET_OS_IPHONE
	interfacePrivate->supportsBond			= FALSE;
	interfacePrivate->bond.interfaces		= NULL;
	interfacePrivate->bond.mode			= NULL;
	interfacePrivate->bond.options			= NULL;
	interfacePrivate->supportsVLAN			= FALSE;
	interfacePrivate->vlan.interface		= NULL;
	interfacePrivate->vlan.tag			= NULL;
	interfacePrivate->vlan.options			= NULL;
#endif	// !TARGET_OS_IPHONE

	return interfacePrivate;
}


#if	!TARGET_OS_IPHONE
__private_extern__
Boolean
__SCNetworkInterfaceSupportsVLAN(CFStringRef bsd_if)
{
	char *			buf	= NULL;
	size_t			buf_len	= 0;
	struct if_msghdr *	ifm;
	char *			if_name	= NULL;
	unsigned int		if_index;
	int			mib[6];
	Boolean			vlanOK	= FALSE;

	// get the interface index
	if_name = _SC_cfstring_to_cstring(bsd_if, NULL, 0, kCFStringEncodingASCII);
	if (if_name == NULL) {
		return FALSE;	// if conversion error
	}
	if_index = if_nametoindex(if_name);
	if (if_index == 0) {
		goto done;	// if unknown interface
	}

	// get information for the specified interface
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_LINK;
	mib[4] = NET_RT_IFLIST;
	mib[5] = if_index;	/* ask for exactly one interface */

	if (sysctl(mib, 6, NULL, &buf_len, NULL, 0) == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("sysctl() size failed: %s"), strerror(errno));
		goto done;
	}
	buf = CFAllocatorAllocate(NULL, buf_len, 0);
	if (sysctl(mib, 6, buf, &buf_len, NULL, 0) == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("sysctl() failed: %s"), strerror(errno));
		goto done;
	}

	// check the link type and hwassist flags
	ifm = (struct if_msghdr *)buf;
	switch (ifm->ifm_type) {
		case RTM_IFINFO : {
#if	defined(IF_HWASSIST_VLAN_TAGGING) && defined(IF_HWASSIST_VLAN_MTU)
			struct if_data	*if_data = &ifm->ifm_data;

			if (if_data->ifi_hwassist & (IF_HWASSIST_VLAN_TAGGING | IF_HWASSIST_VLAN_MTU)) {
				vlanOK = TRUE;
			}
#endif
			break;
		}
	}

    done :

	if (if_name != NULL)	CFAllocatorDeallocate(NULL, if_name);
	if (buf != NULL)	CFAllocatorDeallocate(NULL, buf);

	return vlanOK;
}


__private_extern__
SCNetworkInterfacePrivateRef
_SCBondInterfaceCreatePrivate(CFAllocatorRef	allocator,
			      CFStringRef	bond_if)
{
	SCNetworkInterfacePrivateRef	interfacePrivate;

	interfacePrivate = __SCNetworkInterfaceCreatePrivate(allocator, NULL, NULL, NULL, NULL);
	if (interfacePrivate == NULL) {
		return NULL;
	}

	interfacePrivate->interface_type	= kSCNetworkInterfaceTypeBond;
	interfacePrivate->entity_type		= kSCValNetInterfaceTypeEthernet;
	interfacePrivate->entity_device		= CFStringCreateCopy(allocator, bond_if);
	interfacePrivate->builtin		= TRUE;
	interfacePrivate->supportsVLAN		= __SCNetworkInterfaceSupportsVLAN(bond_if);
	interfacePrivate->sort_order		= kSortBond;

	interfacePrivate->localized_key		= CFSTR("bond");
	interfacePrivate->localized_arg1	= CFRetain(interfacePrivate->entity_device);

	interfacePrivate->bond.interfaces	= CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
//	interfacePrivate->bond.mode		= NULL;
//	interfacePrivate->bond.options		= NULL;

	return interfacePrivate;
}


__private_extern__
SCNetworkInterfacePrivateRef
_SCVLANInterfaceCreatePrivate(CFAllocatorRef		allocator,
			      CFStringRef		vlan_if)
{
	SCNetworkInterfacePrivateRef	interfacePrivate;

	interfacePrivate = __SCNetworkInterfaceCreatePrivate(allocator, NULL, NULL, NULL, NULL);
	if (interfacePrivate == NULL) {
		return NULL;
	}

	interfacePrivate->interface_type        = kSCNetworkInterfaceTypeVLAN;
	interfacePrivate->entity_type           = kSCValNetInterfaceTypeEthernet;
	interfacePrivate->entity_device         = CFStringCreateCopy(allocator, vlan_if);
	interfacePrivate->builtin               = TRUE;
	interfacePrivate->sort_order            = kSortVLAN;

	interfacePrivate->localized_key		= CFSTR("vlan");
	interfacePrivate->localized_arg1	= CFRetain(interfacePrivate->entity_device);

//	interfacePrivate->vlan.interface	= NULL;
//	interfacePrivate->vlan.tag		= NULL;
//	interfacePrivate->vlan.options		= NULL;

	return interfacePrivate;
}
#endif	// !TARGET_OS_IPHONE


#pragma mark -
#pragma mark Interface ordering


static CFArrayRef
split_path(CFStringRef path)
{
	CFArrayRef		components;
	CFMutableStringRef	nPath;

	// turn '@'s into '/'s
	nPath = CFStringCreateMutableCopy(NULL, 0, path);
	(void) CFStringFindAndReplace(nPath,
				      CFSTR("@"),
				      CFSTR("/"),
				      CFRangeMake(0, CFStringGetLength(nPath)),
				      0);

	// split path into components to be compared
	components = CFStringCreateArrayBySeparatingStrings(NULL, nPath, CFSTR("/"));
	CFRelease(nPath);

	return components;
}


CFComparisonResult
_SCNetworkInterfaceCompare(const void *val1, const void *val2, void *context)
{
	SCNetworkInterfacePrivateRef	dev1		= (SCNetworkInterfacePrivateRef)val1;
	SCNetworkInterfacePrivateRef	dev2		= (SCNetworkInterfacePrivateRef)val2;
	CFComparisonResult		res		= kCFCompareEqualTo;

	/* sort by interface type */
	if (dev1->sort_order != dev2->sort_order) {
		if (dev1->sort_order < dev2->sort_order) {
			res = kCFCompareLessThan;
		} else {
			res = kCFCompareGreaterThan;
		}
		return (res);
	}

	/* built-in interfaces sort first */
	if (dev1->builtin != dev2->builtin) {
		if (dev1->builtin) {
			res = kCFCompareLessThan;
		} else {
			res = kCFCompareGreaterThan;
		}
		return (res);
	}

	/* ... and then, sort built-in interfaces by "location" */
	if (dev1->builtin) {
		if (dev1->location != dev2->location) {
			if (isA_CFString(dev1->location)) {
				if (isA_CFString(dev2->location)) {
					res = CFStringCompare(dev1->location, dev2->location, 0);
				} else {
					res = kCFCompareLessThan;
				}
			} else {
				res = kCFCompareGreaterThan;
			}

			if (res != kCFCompareEqualTo) {
				return (res);
			}
		}
	}

	/* ... and, then sort by IOPathMatch */
	if ((dev1->path != NULL) && (dev2->path != NULL)) {
		CFArrayRef	elements1;
		CFArrayRef	elements2;
		CFIndex		i;
		CFIndex		n;
		CFIndex		n1;
		CFIndex		n2;

		elements1 = split_path(dev1->path);
		n1 = CFArrayGetCount(elements1);

		elements2 = split_path(dev2->path);
		n2 = CFArrayGetCount(elements2);

		n = (n1 <= n2) ? n1 : n2;
		for (i = 0; i < n; i++) {
			CFStringRef	e1;
			CFStringRef	e2;
			char		*end;
			quad_t		q1;
			quad_t		q2;
			char		*str;
			Boolean		isNum;

			e1 = CFArrayGetValueAtIndex(elements1, i);
			e2 = CFArrayGetValueAtIndex(elements2, i);

			str = _SC_cfstring_to_cstring(e1, NULL, 0, kCFStringEncodingUTF8);
			errno = 0;
			q1 = strtoq(str, &end, 16);
			isNum = ((*str != '\0') && (*end == '\0') && (errno == 0));
			CFAllocatorDeallocate(NULL, str);

			if (isNum) {
				// if e1 is a valid numeric string
				str = _SC_cfstring_to_cstring(e2, NULL, 0, kCFStringEncodingUTF8);
				errno = 0;
				q2 = strtoq(str, &end, 16);
				isNum = ((*str != '\0') && (*end == '\0') && (errno == 0));
				CFAllocatorDeallocate(NULL, str);

				if (isNum) {
					// if e2 is also a valid numeric string

					if (q1 == q2) {
						res = kCFCompareEqualTo;
						continue;
					} else if (q1 < q2) {
						res = kCFCompareLessThan;
					} else {
						res = kCFCompareGreaterThan;
					}
					break;
				}
			}

			res = CFStringCompare(e1, e2, 0);
			if (res != kCFCompareEqualTo) {
				break;
			}
		}

		if (res == kCFCompareEqualTo) {
			if (n1 < n2) {
				res = kCFCompareLessThan;
			} else if (n1 < n2) {
				res = kCFCompareGreaterThan;
			}
		}

		CFRelease(elements1);
		CFRelease(elements2);

		if (res != kCFCompareEqualTo) {
			return (res);
		}
	}

	/* ... and, then sort by BSD interface name */
	if ((dev1->entity_device != NULL) && (dev2->entity_device != NULL)) {
		res = CFStringCompare(dev1->entity_device, dev2->entity_device, 0);
		if (res != kCFCompareEqualTo) {
			return (res);
		}
	}

	/* ... and lastly, sort by BSD interface unique identifier */
	if ((dev1->entity_device_unique != NULL) && (dev2->entity_device_unique != NULL)) {
		res = CFStringCompare(dev1->entity_device_unique, dev2->entity_device_unique, 0);
//		if (res != kCFCompareEqualTo) {
//			return (res);
//		}
	}

	return res;
}


static void
sort_interfaces(CFMutableArrayRef all_interfaces)
{
	int	n	= CFArrayGetCount(all_interfaces);

	if (n < 2) {
		return;
	}

	CFArraySortValues(all_interfaces, CFRangeMake(0, n), _SCNetworkInterfaceCompare, NULL);
	return;
}


__private_extern__
int
__SCNetworkInterfaceOrder(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	return interfacePrivate->sort_order;
}


#pragma mark -
#pragma mark Interface details


static CFStringRef
IOCopyCFStringValue(CFTypeRef ioVal)
{
	if (isA_CFString(ioVal)) {
		return CFStringCreateCopy(NULL, ioVal);
	}

	if (isA_CFData(ioVal)) {
		return CFStringCreateWithCString(NULL,
						 (const char *)CFDataGetBytePtr(ioVal),
						 kCFStringEncodingUTF8);
	}

	return NULL;
}


static CFStringRef
IODictionaryCopyCFStringValue(CFDictionaryRef io_dict, CFStringRef io_key)
{
	CFTypeRef	ioVal;

	ioVal = CFDictionaryGetValue(io_dict, io_key);
	return IOCopyCFStringValue(ioVal);
}


static Boolean
IOStringValueHasPrefix(CFTypeRef ioVal, CFStringRef prefix)
{
	Boolean		match		= FALSE;
	CFIndex		prefixLen	= CFStringGetLength(prefix);
	CFStringRef	str		= NULL;

	if (!isA_CFString(ioVal)) {
		if (isA_CFData(ioVal)) {
			str = CFStringCreateWithCStringNoCopy(NULL,
							      (const char *)CFDataGetBytePtr(ioVal),
							      kCFStringEncodingUTF8,
							      kCFAllocatorNull);
			ioVal = str;
		} else {
			return FALSE;
		}
	}

	if ((ioVal != NULL) &&
	    (CFStringGetLength(ioVal) >= prefixLen) &&
	    (CFStringCompareWithOptions(ioVal,
					prefix,
					CFRangeMake(0, prefixLen),
					kCFCompareCaseInsensitive) == kCFCompareEqualTo)) {
		match = TRUE;
	}

	if (str != NULL) CFRelease(str);
	return match;
}


static const struct {
	const CFStringRef	name;
	const CFStringRef	slot;
} slot_mappings[] = {
	// Beige G3
	{ CFSTR("A1") , CFSTR("1") },
	{ CFSTR("B1") , CFSTR("2") },
	{ CFSTR("C1") , CFSTR("3") },

	// Blue&White G3, Yikes G4
	{ CFSTR("J12"), CFSTR("1") },
	{ CFSTR("J11"), CFSTR("2") },
	{ CFSTR("J10"), CFSTR("3") },
	{ CFSTR("J9"),  CFSTR("4") },

	// AGP G4
	{ CFSTR("A")  , CFSTR("1") },
	{ CFSTR("B")  , CFSTR("2") },
	{ CFSTR("C")  , CFSTR("3") },
	{ CFSTR("D") ,  CFSTR("4") },

	// Digital Audio G4 (and later models)
	{ CFSTR("1")  , CFSTR("1") },
	{ CFSTR("2")  , CFSTR("2") },
	{ CFSTR("3")  , CFSTR("3") },
	{ CFSTR("4") ,  CFSTR("4") },
	{ CFSTR("5") ,  CFSTR("5") }
};


static CFStringRef
pci_slot(io_registry_entry_t interface, CFTypeRef *pci_slot_name)
{
	kern_return_t		kr;
	io_registry_entry_t	parent;
	CFMutableStringRef	slot;
	CFTypeRef		slot_name;

	slot = NULL;
	if (pci_slot_name != NULL) *pci_slot_name = NULL;

	slot_name = IORegistryEntryCreateCFProperty(interface, CFSTR("AAPL,slot-name"), NULL, 0);
	if (slot_name != NULL) {
		CFIndex	i;

		slot = CFStringCreateMutable(NULL, 0);
		if (isA_CFString(slot_name)) {
			if (pci_slot_name != NULL) *pci_slot_name = CFStringCreateCopy(NULL, slot_name);
			CFStringAppend(slot, slot_name);
		} else if (isA_CFData(slot_name)) {
			if (pci_slot_name != NULL) *pci_slot_name = CFDataCreateCopy(NULL, slot_name);
			CFStringAppendCString(slot,
					      (const char *)CFDataGetBytePtr(slot_name),
					      kCFStringEncodingUTF8);
		}

		if (CFStringGetLength(slot) > 5) {
			(void) CFStringFindAndReplace(slot,
						      CFSTR("slot-"),
						      CFSTR(""),
						      CFRangeMake(0, 5),
						      kCFCompareCaseInsensitive|kCFCompareAnchored);
		}

		for (i = 0; i < sizeof(slot_mappings)/sizeof(slot_mappings[0]); i++) {
			if (CFStringCompare(slot,
					    slot_mappings[i].name,
					    kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
				CFRelease(slot);
				slot = (CFMutableStringRef)CFRetain(slot_mappings[i].slot);
				break;
			}
		}

		CFRelease(slot_name);
	}

	kr = IORegistryEntryGetParentEntry(interface, kIOServicePlane, &parent);
	switch (kr) {
		case kIOReturnSuccess : {
			CFTypeRef	parent_pci_slot_name	= NULL;
			CFStringRef	parent_slot;

			parent_slot = pci_slot(parent, &parent_pci_slot_name);
			if (parent_slot != NULL) {
				if (slot != NULL) CFRelease(slot);
				slot = (CFMutableStringRef)parent_slot;

				if (pci_slot_name != NULL) {
					if (*pci_slot_name != NULL) CFRelease(*pci_slot_name);
					*pci_slot_name = parent_pci_slot_name;
				} else {
					CFRelease(parent_pci_slot_name);
				}
			}

			IOObjectRelease(parent);
			break;
		}
		case kIOReturnNoDevice :
			// if we have hit the root node
			break;
		default :
			SCLog(TRUE, LOG_DEBUG, CFSTR("pci_slot IORegistryEntryGetParentEntry() failed, kr = 0x%x"), kr);
			break;
	}

	return slot;
}


static CFComparisonResult
compare_bsdNames(const void *val1, const void *val2, void *context)
{
	CFStringRef	bsd1	= (CFStringRef)val1;
	CFStringRef	bsd2	= (CFStringRef)val2;

	return CFStringCompare(bsd1, bsd2, 0);
}


static CFStringRef
pci_port(CFTypeRef slot_name, CFStringRef bsdName)
{
	CFIndex			n;
	CFStringRef		port_name	= NULL;
	CFMutableArrayRef	port_names;

	kern_return_t		kr;
	CFStringRef		match_keys[2];
	CFTypeRef		match_vals[2];
	CFDictionaryRef		match_dict;
	CFDictionaryRef		matching;
	io_registry_entry_t	slot;
	io_iterator_t		slot_iterator	= MACH_PORT_NULL;

	match_keys[0] = CFSTR("AAPL,slot-name");
	match_vals[0] = slot_name;

	match_dict = CFDictionaryCreate(NULL,
					(const void **)match_keys,
					(const void **)match_vals,
					1,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);

	match_keys[0] = CFSTR(kIOProviderClassKey);
	match_vals[0] = CFSTR("IOPCIDevice");

	match_keys[1] = CFSTR(kIOPropertyMatchKey);
	match_vals[1] = match_dict;

	// note: the "matching" dictionary will be consumed by the following
	matching = CFDictionaryCreate(NULL,
				      (const void **)match_keys,
				      (const void **)match_vals,
				      sizeof(match_keys)/sizeof(match_keys[0]),
				      &kCFTypeDictionaryKeyCallBacks,
				      &kCFTypeDictionaryValueCallBacks);
	CFRelease(match_dict);

	kr = IOServiceGetMatchingServices(masterPort, matching, &slot_iterator);
	if (kr != kIOReturnSuccess) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("pci_port IOServiceGetMatchingServices() failed, kr = 0x%x"), kr);
		return MACH_PORT_NULL;
	}

	port_names = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	while ((slot = IOIteratorNext(slot_iterator)) != MACH_PORT_NULL) {
		io_registry_entry_t	child;
		io_iterator_t		child_iterator	= MACH_PORT_NULL;

		kr = IORegistryEntryCreateIterator(slot,
						   kIOServicePlane,
						   kIORegistryIterateRecursively,
						   &child_iterator);
		if (kr != kIOReturnSuccess) {
			SCLog(TRUE, LOG_DEBUG, CFSTR("pci_port IORegistryEntryCreateIterator() failed, kr = 0x%x"), kr);
			CFRelease(port_names);
			return MACH_PORT_NULL;
		}

		while ((child = IOIteratorNext(child_iterator)) != MACH_PORT_NULL) {
			if (IOObjectConformsTo(child, kIONetworkInterfaceClass)) {
				CFStringRef	if_bsdName;

				if_bsdName = IORegistryEntryCreateCFProperty(child,
									     CFSTR(kIOBSDNameKey),
									     NULL,
									     0);
				if (if_bsdName != NULL) {
					CFArrayAppendValue(port_names, if_bsdName);
					CFRelease(if_bsdName);
				}
			}
			IOObjectRelease(child);
		}
		IOObjectRelease(child_iterator);
		IOObjectRelease(slot);
	}
	IOObjectRelease(slot_iterator);

	n = CFArrayGetCount(port_names);
	if (n > 1) {
		CFArraySortValues(port_names, CFRangeMake(0, n), compare_bsdNames, NULL);
		n = CFArrayGetFirstIndexOfValue(port_names, CFRangeMake(0, n), bsdName);
		if (n != kCFNotFound) {
			port_name = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), n + 1);
		}
	}

	CFRelease(port_names);
	return port_name;
}


static Boolean
pci_slot_info(io_registry_entry_t interface, CFStringRef *slot_name, CFStringRef *port_name)
{
	CFStringRef	bsd_name;
	Boolean		ok		= FALSE;
	CFTypeRef	pci_slot_name;

	*slot_name = NULL;
	*port_name = NULL;

	bsd_name = IORegistryEntryCreateCFProperty(interface, CFSTR(kIOBSDNameKey), NULL, 0);
	if (bsd_name == NULL) {
		return FALSE;
	}

	*slot_name = pci_slot(interface, &pci_slot_name);
	if (*slot_name != NULL) {
		if (pci_slot_name != NULL) {
			*port_name = pci_port(pci_slot_name, bsd_name);
			CFRelease(pci_slot_name);
		}
		ok = TRUE;
	}

	CFRelease(bsd_name);
	return ok;
}


static Boolean
isBuiltin(io_registry_entry_t interface)
{
	CFStringRef	slot;

	slot = pci_slot(interface, NULL);
	if (slot != NULL) {
		// interfaces which have a "slot" are not built-in
		CFRelease(slot);
		return FALSE;
	}

	return TRUE;
}


static Boolean
isBluetoothBuiltin(Boolean *haveController)
{
	Boolean		builtin		= FALSE;
	io_object_t	hciController;
	io_iterator_t	iter		= MACH_PORT_NULL;
	kern_return_t	kr;

	kr = IOServiceGetMatchingServices(masterPort,
					  IOServiceMatching("IOBluetoothHCIController"),
					  &iter);
	if ((kr != kIOReturnSuccess) || (iter == MACH_PORT_NULL)) {
		if (kr != kIOReturnSuccess) {
			SCLog(TRUE, LOG_DEBUG, CFSTR("isBluetoothBuiltin IOServiceGetMatchingServices() failed, kr = 0x%x"), kr);
		}
		*haveController = FALSE;
		return FALSE;
	}
	*haveController = TRUE;

	hciController = IOIteratorNext(iter);
	IOObjectRelease(iter);
	if(hciController != MACH_PORT_NULL) {
		CFNumberRef	idVendor;

		idVendor = IORegistryEntryCreateCFProperty(hciController, CFSTR(kUSBVendorID), NULL, 0);
		if (idVendor != NULL) {
			int	idVendorVal;

			if (isA_CFNumber(idVendor) &&
			    CFNumberGetValue(idVendor, kCFNumberIntType, &idVendorVal) &&
			    (idVendorVal == kIOUSBVendorIDAppleComputer)) {
				builtin = TRUE;
			}

			CFRelease(idVendor);
		}

		IOObjectRelease(hciController);
	}

	return builtin;
}


#pragma mark -
#pragma mark Interface enumeration


typedef Boolean (*processInterface)(SCNetworkInterfacePrivateRef	interfacePrivate,
				    io_registry_entry_t			interface,
				    CFDictionaryRef			interface_dict,
				    io_registry_entry_t			controller,
				    CFDictionaryRef			controller_dict,
				    io_registry_entry_t			bus,
				    CFDictionaryRef			bus_dict);


static Boolean
processNetworkInterface(SCNetworkInterfacePrivateRef	interfacePrivate,
			io_registry_entry_t		interface,
			CFDictionaryRef			interface_dict,
			io_registry_entry_t		controller,
			CFDictionaryRef			controller_dict,
			io_registry_entry_t		bus,
			CFDictionaryRef			bus_dict)
{
	CFDataRef	data;
	int		ift	= -1;
	int		iVal;
	CFNumberRef	num;
	CFStringRef	str;
	CFBooleanRef	val;

	// interface type
	num = CFDictionaryGetValue(interface_dict, CFSTR(kIOInterfaceType));
	if (isA_CFNumber(num) &&
	    CFNumberGetValue(num, kCFNumberIntType, &ift)) {
		interfacePrivate->type = CFRetain(num);
	} else {
		SCLog(TRUE, LOG_DEBUG, CFSTR("processNetworkInterface() failed, no interface type"));
		return FALSE;
	}

	switch (ift) {
		case IFT_ETHER :
			// Type, Hardware

			if ((IOObjectConformsTo(controller, "IO80211Controller")) ||
			    (IOObjectConformsTo(controller, "AirPortPCI"       )) ||
			    (IOObjectConformsTo(controller, "AirPortDriver"    ))) {
				interfacePrivate->interface_type	= kSCNetworkInterfaceTypeIEEE80211;
				interfacePrivate->entity_type		= kSCValNetInterfaceTypeEthernet;
				interfacePrivate->sort_order		= kSortAirPort;
			} else if (IOObjectConformsTo(controller, "IOBluetoothBNEPDriver")) {
				interfacePrivate->interface_type	= kSCNetworkInterfaceTypeEthernet;
				interfacePrivate->entity_type		= kSCValNetInterfaceTypeEthernet;
				interfacePrivate->sort_order		= kSortBluetoothPAN;
			} else {
				str = IODictionaryCopyCFStringValue(bus_dict, CFSTR("name"));
				if ((str != NULL) && CFEqual(str, CFSTR("radio"))) {
					interfacePrivate->interface_type	= kSCNetworkInterfaceTypeEthernet;	// ??
					interfacePrivate->entity_type		= kSCValNetInterfaceTypeEthernet;
					interfacePrivate->sort_order		= kSortOtherWireless;
				} else {
					interfacePrivate->interface_type	= kSCNetworkInterfaceTypeEthernet;
					interfacePrivate->entity_type		= kSCValNetInterfaceTypeEthernet;
					interfacePrivate->sort_order		= kSortEthernet;

#if	!TARGET_OS_IPHONE
					// BOND support only enabled for ethernet devices
					interfacePrivate->supportsBond = TRUE;
#endif	// !TARGET_OS_IPHONE
				}

				if (str != NULL) CFRelease(str);
			}

			// check if WWAN Ethernet
			if (IOObjectConformsTo(controller, "AppleUSBEthernetHost")) {
				interfacePrivate->sort_order = kSortTethered;
			} else if (IOObjectConformsTo(controller, "AppleUSBCDCECMData")) {
				interfacePrivate->sort_order = kSortWWANEthernet;
			}

			// built-in
			val = isA_CFBoolean(CFDictionaryGetValue(interface_dict, CFSTR(kIOBuiltin)));
			if (val == NULL) {
				val = isA_CFBoolean(CFDictionaryGetValue(interface_dict, CFSTR(kIOPrimaryInterface)));
			}
			if (val != NULL) {
				interfacePrivate->builtin = CFBooleanGetValue(val);
			} else {
				interfacePrivate->builtin = isBuiltin(interface);
			}

			if (!interfacePrivate->builtin &&
			    CFEqual(interfacePrivate->interface_type, kSCNetworkInterfaceTypeIEEE80211)) {
				// always treat AirPort interfaces as built-in
				interfacePrivate->builtin = TRUE;
			}

			// location
			interfacePrivate->location = IODictionaryCopyCFStringValue(interface_dict, CFSTR(kIOLocation));

#if	!TARGET_OS_IPHONE
			// VLAN support
			num = CFDictionaryGetValue(controller_dict, CFSTR(kIOFeatures));
			if (isA_CFNumber(num) &&
			    CFNumberGetValue(num, kCFNumberIntType, & iVal)) {
				if (iVal & (kIONetworkFeatureHardwareVlan | kIONetworkFeatureSoftwareVlan)) {
					interfacePrivate->supportsVLAN = TRUE;
				}
			}
#endif	// !TARGET_OS_IPHONE

			// localized name
			if (CFEqual(interfacePrivate->interface_type, kSCNetworkInterfaceTypeIEEE80211)) {
				interfacePrivate->localized_key = CFSTR("airport");
			} else if (interfacePrivate->sort_order == kSortBluetoothPAN) {
				interfacePrivate->localized_key  = CFSTR("bluetooth-pan");
			} else if (interfacePrivate->sort_order == kSortOtherWireless) {
				interfacePrivate->localized_key  = CFSTR("wireless");
				interfacePrivate->localized_arg1 = CFRetain(CFSTR(""));		// ??
			} else if (interfacePrivate->builtin) {
				if ((interfacePrivate->location == NULL) ||
				    (CFStringGetLength(interfacePrivate->location) == 0)) {
					interfacePrivate->localized_key = CFSTR("ether");
				} else {
					interfacePrivate->localized_key  = CFSTR("multiether");
					interfacePrivate->localized_arg1 = CFRetain(interfacePrivate->location);
				}
			} else {
				CFStringRef	provider;

				// check provider class
				provider = IORegistryEntrySearchCFProperty(interface,
									   kIOServicePlane,
									   CFSTR(kIOProviderClassKey),
									   NULL,
									   kIORegistryIterateRecursively | kIORegistryIterateParents);
				if (provider != NULL) {
					if (CFEqual(provider, CFSTR("IOPCIDevice"))) {
						CFStringRef		port_name;
						CFStringRef		slot_name;

						if (pci_slot_info(interface, &slot_name, &port_name)) {
							if (port_name == NULL) {
								interfacePrivate->localized_key  = CFSTR("pci-ether");
								interfacePrivate->localized_arg1 = slot_name;
							} else {
								interfacePrivate->localized_key  = CFSTR("pci-multiether");
								interfacePrivate->localized_arg1 = slot_name;
								interfacePrivate->localized_arg2 = port_name;
							}
						}
					} else if (CFEqual(provider, CFSTR("IOUSBDevice")) ||
						   CFEqual(provider, CFSTR("IOUSBInterface"))) {
						// check if a "Product Name" has been provided
						val = IORegistryEntrySearchCFProperty(interface,
										      kIOServicePlane,
										      CFSTR(kIOPropertyProductNameKey),
										      NULL,
										      kIORegistryIterateRecursively | kIORegistryIterateParents);
						if (val == NULL) {
							// check if a "USB Product Name" has been provided
							val = IORegistryEntrySearchCFProperty(interface,
											      kIOServicePlane,
											      CFSTR(kUSBProductString),
											      NULL,
											      kIORegistryIterateRecursively | kIORegistryIterateParents);
						}
						if (val != NULL) {
							CFStringRef	productName;

							productName = IOCopyCFStringValue(val);
							CFRelease(val);

							if (productName != NULL) {
								if (CFStringGetLength(productName) > 0) {
									// if we have a [somewhat reasonable?] product name
									if (interfacePrivate->name != NULL) {
										CFRelease(interfacePrivate->name);
									}
									interfacePrivate->name = CFRetain(productName);
									if (interfacePrivate->localized_name != NULL) {
										CFRelease(interfacePrivate->localized_name);
									}
									interfacePrivate->localized_name = copy_interface_string(bundle, productName, TRUE);
								}

								CFRelease(productName);
							}
						} else {
							interfacePrivate->localized_key  = CFSTR("usb-ether");
							interfacePrivate->localized_arg1 = IODictionaryCopyCFStringValue(interface_dict, CFSTR(kIOBSDNameKey));
						}
					}
					CFRelease(provider);
				}

				if (interfacePrivate->localized_key == NULL) {
					// if no provider, not a PCI device, or no slot information
					interfacePrivate->localized_key  = CFSTR("generic-ether");
					interfacePrivate->localized_arg1 = IODictionaryCopyCFStringValue(interface_dict, CFSTR(kIOBSDNameKey));
				}
			}

			break;
		case IFT_IEEE1394 :
			// Type
			interfacePrivate->interface_type = kSCNetworkInterfaceTypeFireWire;

			// Entity
			interfacePrivate->entity_type = kSCValNetInterfaceTypeFireWire;

			// built-in
			interfacePrivate->builtin = isBuiltin(interface);

			// sort order
			interfacePrivate->sort_order = kSortFireWire;

			// localized name
			if (interfacePrivate->builtin) {
				interfacePrivate->localized_key = CFSTR("firewire");
			} else {
				CFStringRef	slot_name;

				slot_name = pci_slot(interface, NULL);
				if (slot_name != NULL) {
					interfacePrivate->localized_key  = CFSTR("pci-firewire");
					interfacePrivate->localized_arg1 = slot_name;
				}
			}

			break;
		default :
			SCLog(TRUE, LOG_DEBUG, CFSTR("processNetworkInterface() failed, unknown interface type = %d"), ift);
			return FALSE;
	}

	// Device
	interfacePrivate->entity_device = IODictionaryCopyCFStringValue(interface_dict, CFSTR(kIOBSDNameKey));

	// Hardware (MAC) address
	data = CFDictionaryGetValue(controller_dict, CFSTR(kIOMACAddress));
	if (isA_CFData(data)) {
		interfacePrivate->address = CFRetain(data);
	}

	// interface unit
	num = CFDictionaryGetValue(interface_dict, CFSTR(kIOInterfaceUnit));
	if (isA_CFNumber(num) &&
	    CFNumberGetValue(num, kCFNumberIntType, & iVal)) {
		interfacePrivate->unit = CFRetain(num);
	}

	return TRUE;
}


static void
set_connection_script(SCNetworkInterfacePrivateRef interfacePrivate, CFStringRef script)
{
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict;

	if (interfacePrivate->overrides == NULL) {
		interfacePrivate->overrides = CFDictionaryCreateMutable(NULL,
									0,
									&kCFTypeDictionaryKeyCallBacks,
									&kCFTypeDictionaryValueCallBacks);
	}

	dict = CFDictionaryGetValue(interfacePrivate->overrides, kSCNetworkInterfaceTypeModem);
	if (dict != NULL) {
		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	} else {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}
	if (script != NULL) {
		CFDictionarySetValue(newDict, kSCPropNetModemConnectionScript, script);
	} else {
		CFDictionaryRemoveValue(newDict, kSCPropNetModemConnectionScript);
	}
	if (CFDictionaryGetCount(newDict) > 0) {
		CFDictionarySetValue(interfacePrivate->overrides, kSCNetworkInterfaceTypeModem, newDict);
	} else {
		CFDictionaryRemoveValue(interfacePrivate->overrides, kSCNetworkInterfaceTypeModem);
	}
	CFRelease(newDict);

	if (CFDictionaryGetCount(interfacePrivate->overrides) == 0) {
		CFRelease(interfacePrivate->overrides);
		interfacePrivate->overrides = NULL;
	}
	return;
}


static Boolean
is_valid_connection_script(CFStringRef script)
{
	char				ccl[MAXPATHLEN];
	char				path[MAXPATHLEN];
	NSSearchPathEnumerationState	state;

	(void) _SC_cfstring_to_cstring(script,
				       ccl,
				       sizeof(ccl),
				       kCFStringEncodingUTF8);

	state = NSStartSearchPathEnumeration(NSLibraryDirectory,
					     NSLocalDomainMask|NSSystemDomainMask);
	while ((state = NSGetNextSearchPathEnumeration(state, path))) {
		size_t		n;
		struct stat	statBuf;

		if (ccl[0] == '/') {
			path[0] = '\0';		// if modemCCL is a full path
		} else {
			strlcat(path, "/Modem Scripts/", sizeof(path));
		}
		strlcat(path, ccl, sizeof(path));

		if (stat(path, &statBuf) != 0) {
			if (errno == ENOENT) {
				goto bundle;
			}

			SCLog(TRUE, LOG_DEBUG,
			      CFSTR("processSerialInterface stat() failed: %s"),
			      strerror(errno));
			continue;
		}
		if (S_ISREG(statBuf.st_mode)) {
			// if we have a valid CCL script
			return TRUE;
		}

#define	BUNDLE_EXT	".ccl"
#define	BUNDLE_EXT_LEN	sizeof(BUNDLE_EXT) - 1

	    bundle :

		n = strlen(path);
		if ((n <= BUNDLE_EXT_LEN) ||
		    (strstr(&path[n - BUNDLE_EXT_LEN], BUNDLE_EXT) == NULL)) {
			strlcat(path, BUNDLE_EXT, sizeof(path));
			if (stat(path, &statBuf) != 0) {
				if (errno == ENOENT) {
					continue;
				}

				SCLog(TRUE, LOG_DEBUG,
				      CFSTR("processSerialInterface stat() failed: %s"),
				      strerror(errno));
				continue;
			}
		}
		if (S_ISDIR(statBuf.st_mode)) {
			// if we have a valid CCL bundle
			return TRUE;
		}
	}

	return FALSE;
}


static Boolean
processSerialInterface(SCNetworkInterfacePrivateRef	interfacePrivate,
		       io_registry_entry_t		interface,
		       CFDictionaryRef			interface_dict,
		       io_registry_entry_t		controller,
		       CFDictionaryRef			controller_dict,
		       io_registry_entry_t		bus,
		       CFDictionaryRef			bus_dict)
{
	CFStringRef		base		= NULL;
	CFStringRef		ift;
	Boolean			isModem		= FALSE;
	Boolean			isWWAN		= FALSE;
	CFStringRef		modemCCL	= NULL;
	Boolean			ok		= FALSE;
	CFTypeRef		val;

	// check if hidden
	val = IORegistryEntrySearchCFProperty(interface,
					      kIOServicePlane,
					      CFSTR("HiddenPort"),
					      NULL,
					      kIORegistryIterateRecursively | kIORegistryIterateParents);
	if (val != NULL) {
		CFRelease(val);
		return FALSE;	// if this interface should not be exposed
	}

	// check if initializing
	val = IORegistryEntrySearchCFProperty(interface,
					      kIOServicePlane,
					      CFSTR("Initializing"),
					      NULL,
					      kIORegistryIterateRecursively | kIORegistryIterateParents);
	if (val != NULL) {
		Boolean	initializing;

		initializing = isA_CFBoolean(val) && CFBooleanGetValue(val);
		CFRelease(val);
		if (initializing) {
			return FALSE;	// if this interface is still initializing
		}
	}

	// check if WWAN
	val = IORegistryEntrySearchCFProperty(interface,
					      kIOServicePlane,
					      CFSTR("WWAN"),
					      NULL,
					      kIORegistryIterateRecursively | kIORegistryIterateParents);
	if (val != NULL) {
		isWWAN = isA_CFBoolean(val) && CFBooleanGetValue(val);
		CFRelease(val);
	}

	// Entity (Device)
	interfacePrivate->entity_device = IODictionaryCopyCFStringValue(interface_dict, CFSTR(kIOTTYDeviceKey));
	if (interfacePrivate->entity_device == NULL) {
		return FALSE;
	}

	base = IODictionaryCopyCFStringValue(interface_dict, CFSTR(kIOTTYBaseNameKey));
	if (base == NULL) {
		base = CFRetain(interfacePrivate->entity_device);
	}

	/*
	 * From MoreSCF:
	 *
	 * Exclude ports named "irda" because otherwise the IrDA ports on the
	 * original iMac (rev's A through D) show up as serial ports.  Given
	 * that only the rev A actually had an IrDA port, and Mac OS X doesn't
	 * even support it, these ports definitely shouldn't be listed.
	 */
	if (CFStringCompare(base,
			    CFSTR("irda"),
			    kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
		goto done;
	}

	if (IOStringValueHasPrefix(base, CFSTR("bluetooth"))) {
		Boolean	haveController	= FALSE;

		// Bluetooth
		interfacePrivate->interface_type	= kSCNetworkInterfaceTypeBluetooth;
		interfacePrivate->sort_order		= kSortBluetooth;
		interfacePrivate->builtin		= isBluetoothBuiltin(&haveController);
		if (!haveController) {
			// if device with no controller present
			goto done;
		}
	} else if (IOStringValueHasPrefix(base, CFSTR("irda-ircomm"))) {
		// IrDA
		interfacePrivate->interface_type	= kSCNetworkInterfaceTypeIrDA;
		interfacePrivate->sort_order		= kSortIrDA;
	} else if (isWWAN) {
		// WWAN
		interfacePrivate->interface_type	= kSCNetworkInterfaceTypeWWAN;
		interfacePrivate->sort_order		= kSortWWAN;
	} else {
		// Modem
		interfacePrivate->interface_type	= kSCNetworkInterfaceTypeModem;
		interfacePrivate->sort_order		= kSortModem;

		// V.92 support
		val = IORegistryEntrySearchCFProperty(interface,
						      kIOServicePlane,
						      CFSTR(kIODeviceSupportsHoldKey),
						      NULL,
						      kIORegistryIterateRecursively | kIORegistryIterateParents);
		if (val != NULL) {
			uint32_t	v92;

			if (isA_CFNumber(val) &&
			    CFNumberGetValue(val, kCFNumberSInt32Type, &v92)) {
				interfacePrivate->modemIsV92 = (v92 == 1);
			}
			CFRelease(val);
		}
	}

	// Entity (Type)
	interfacePrivate->entity_type = kSCEntNetModem;

	// Entity (Hardware)
	ift = CFDictionaryGetValue(interface_dict, CFSTR(kIOSerialBSDTypeKey));
	if (!isA_CFString(ift)) {
		goto done;
	}

	if (CFEqual(ift, CFSTR(kIOSerialBSDModemType))) {
		// if modem
		isModem = TRUE;

		if (CFEqual(base, CFSTR("modem"))) {
			interfacePrivate->builtin = TRUE;
			interfacePrivate->sort_order = kSortInternalModem;
		} else if (CFEqual(base, CFSTR("usbmodem"))) {
			interfacePrivate->sort_order = kSortUSBModem;
		}
	} else if (CFEqual(ift, CFSTR(kIOSerialBSDRS232Type))) {
		// if serial port
		interfacePrivate->sort_order = kSortSerialPort;
	} else {
		goto done;
	}

	// configuration template overrides
	val = IORegistryEntrySearchCFProperty(interface,
					      kIOServicePlane,
					      CFSTR("DevicePPPOverrides"),
					      NULL,
					      kIORegistryIterateRecursively | kIORegistryIterateParents);
	if (val != NULL) {
		if (isA_CFDictionary(val)) {
			if (interfacePrivate->overrides == NULL) {
				interfacePrivate->overrides = CFDictionaryCreateMutable(NULL,
											0,
											&kCFTypeDictionaryKeyCallBacks,
											&kCFTypeDictionaryValueCallBacks);
			}
			CFDictionarySetValue(interfacePrivate->overrides, kSCNetworkInterfaceTypePPP, val);
		}
		CFRelease(val);
	}

	val = IORegistryEntrySearchCFProperty(interface,
					      kIOServicePlane,
					      CFSTR("DeviceModemOverrides"),
					      NULL,
					      kIORegistryIterateRecursively | kIORegistryIterateParents);
	if (val != NULL) {
		if (isA_CFDictionary(val)) {
			CFStringRef	uniqueID;

			if (interfacePrivate->overrides == NULL) {
				interfacePrivate->overrides = CFDictionaryCreateMutable(NULL,
											0,
											&kCFTypeDictionaryKeyCallBacks,
											&kCFTypeDictionaryValueCallBacks);
			}
			CFDictionarySetValue(interfacePrivate->overrides, kSCNetworkInterfaceTypeModem, val);

			modemCCL = CFDictionaryGetValue(val, kSCPropNetModemConnectionScript);
			modemCCL = isA_CFString(modemCCL);

			uniqueID = CFDictionaryGetValue(val, CFSTR("UniqueIdentifier"));
			uniqueID = isA_CFString(uniqueID);
			if (uniqueID != NULL) {
				// retain the device's base name and the unique id
				CFRelease(interfacePrivate->entity_device);
				interfacePrivate->entity_device = CFRetain(base);
				interfacePrivate->entity_device_unique = CFStringCreateCopy(NULL, uniqueID);
			}
		}
		CFRelease(val);
	}

	// if not part of the DeviceModemOverrides, look harder for the modem CCL
	if (modemCCL == NULL) {
		val = IORegistryEntrySearchCFProperty(interface,
						      kIOServicePlane,
						      CFSTR("ModemCCL"),
						      NULL,
						      kIORegistryIterateRecursively | kIORegistryIterateParents);
		if (val != NULL) {
			modemCCL = IOCopyCFStringValue(val);
			if (modemCCL != NULL) {
				set_connection_script(interfacePrivate, modemCCL);
				CFRelease(modemCCL);
			}

			CFRelease(val);
		}
	}

	// localized name
	if (CFEqual(interfacePrivate->interface_type, kSCNetworkInterfaceTypeIrDA)) {
		interfacePrivate->localized_key = CFSTR("irda");
	} else if (CFEqual(interfacePrivate->interface_type, kSCNetworkInterfaceTypeBluetooth)) {
		interfacePrivate->localized_key = CFSTR("bluetooth");
	} else {
		CFStringRef		localized	= NULL;
		CFStringRef		name		= NULL;
		CFMutableStringRef	port;

		port = CFStringCreateMutableCopy(NULL, 0, base);
		CFStringLowercase(port, NULL);

		if (!isModem) {
			CFStringAppend(port, CFSTR("-port"));
		}

		// set non-localized name
		if (bundle != NULL) {
			name = copy_interface_string(bundle, port, FALSE);
		}
		if (name != NULL) {
			if (!CFEqual(port, name)) {
				// if [English] localization available
				interfacePrivate->name = name;
			} else {
				// if no [English] localization available, use TTY base name
				CFRelease(name);
				interfacePrivate->name = CFStringCreateCopy(NULL, base);
			}
		} else {
			interfacePrivate->name = CFStringCreateCopy(NULL, base);
		}

		// set localized name
		if (bundle != NULL) {
			localized = copy_interface_string(bundle, port, TRUE);
		}
		if (localized != NULL) {
			if (!CFEqual(port, localized)) {
				// if localization available
				interfacePrivate->localized_name = localized;
			} else {
				// if no localization available, use TTY base name
				CFRelease(localized);
				interfacePrivate->localized_name = CFStringCreateCopy(NULL, base);
			}
		} else {
			interfacePrivate->localized_name = CFStringCreateCopy(NULL, base);
		}

		if (!isModem || !CFEqual(base, CFSTR("modem"))) {
			CFStringRef	productName;

			// check if a "Product Name" has been provided
			val = IORegistryEntrySearchCFProperty(interface,
							      kIOServicePlane,
							      CFSTR(kIOPropertyProductNameKey),
							      NULL,
							      kIORegistryIterateRecursively | kIORegistryIterateParents);
			if (val == NULL) {
				// check if a "USB Product Name" has been provided
				val = IORegistryEntrySearchCFProperty(interface,
								      kIOServicePlane,
								      CFSTR(kUSBProductString),
								      NULL,
								      kIORegistryIterateRecursively | kIORegistryIterateParents);
			}
			if (val != NULL) {
				productName = IOCopyCFStringValue(val);
				CFRelease(val);

				if (productName != NULL) {
					if (CFStringGetLength(productName) > 0) {
						// if we have a [somewhat reasonable?] product name
						if (interfacePrivate->name != NULL) {
							CFRelease(interfacePrivate->name);
						}
						interfacePrivate->name = CFRetain(productName);
						if (interfacePrivate->localized_name != NULL) {
							CFRelease(interfacePrivate->localized_name);
						}
						interfacePrivate->localized_name = copy_interface_string(bundle, productName, TRUE);

						// if not provided, also check if the product name
						// matches a CCL script
						if ((modemCCL == NULL) &&
						    is_valid_connection_script(productName)) {
							set_connection_script(interfacePrivate, productName);
						}
					}

					CFRelease(productName);
				}
			}
		}

		CFRelease(port);
	}

	ok = TRUE;

    done :

	if (!ok && (interfacePrivate->entity_device != NULL)) {
		CFRelease(interfacePrivate->entity_device);
		interfacePrivate->entity_device = NULL;
	}
	if (base != NULL) CFRelease(base);

	return ok;
}


static SCNetworkInterfaceRef
createInterface(io_registry_entry_t interface, processInterface func)
{
	io_registry_entry_t		bus			= MACH_PORT_NULL;
	CFMutableDictionaryRef		bus_dict		= NULL;
	io_registry_entry_t		controller		= MACH_PORT_NULL;
	CFMutableDictionaryRef		controller_dict		= NULL;
	SCNetworkInterfacePrivateRef	interfacePrivate	= NULL;
	CFMutableDictionaryRef		interface_dict		= NULL;
	kern_return_t			kr;
	io_string_t			path;

	kr = IORegistryEntryGetPath(interface, kIOServicePlane, path);
	if (kr != kIOReturnSuccess) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("createInterface IORegistryEntryGetPath() failed, kr = 0x%x"), kr);
		goto done;
	}

	kr = IORegistryEntryCreateCFProperties(interface, &interface_dict, NULL, kNilOptions);
	if (kr != kIOReturnSuccess) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("createInterface IORegistryEntryCreateCFProperties() failed, kr = 0x%x"), kr);
		goto done;
	}

	/* get the controller node */
	kr = IORegistryEntryGetParentEntry(interface, kIOServicePlane, &controller);
	if (kr != KERN_SUCCESS) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("createInterface IORegistryEntryGetParentEntry() failed, kr = 0x%x"), kr);
		goto done;
	}

	/* get the dictionary associated with the node */
	kr = IORegistryEntryCreateCFProperties(controller, &controller_dict, NULL, kNilOptions);
	if (kr != KERN_SUCCESS) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("createInterface IORegistryEntryCreateCFProperties() failed, kr = 0x%x"), kr);
		goto done;
	}

	/* get the bus node */
	kr = IORegistryEntryGetParentEntry(controller, kIOServicePlane, &bus);
	if (kr != KERN_SUCCESS) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("createInterface IORegistryEntryGetParentEntry() failed, kr = 0x%x"), kr);
		goto done;
	}

	/* get the dictionary associated with the node */
	kr = IORegistryEntryCreateCFProperties(bus, &bus_dict, NULL, kNilOptions);
	if (kr != KERN_SUCCESS) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("createInterface IORegistryEntryCreateCFProperties() failed, kr = 0x%x"), kr);
		goto done;
	}

	interfacePrivate = __SCNetworkInterfaceCreatePrivate(NULL, NULL, NULL, NULL, path);

	if ((*func)(interfacePrivate, interface, interface_dict, controller, controller_dict, bus, bus_dict)) {
		CFTypeRef	val;

		/* check user-notification / auto-configuration preferences */
		val = IORegistryEntrySearchCFProperty(interface,
						      kIOServicePlane,
						      kSCNetworkInterfaceConfigurationActionKey,
						      NULL,
						      kIORegistryIterateRecursively | kIORegistryIterateParents);
		if (val != NULL) {
			if (isA_CFString(val)) {
				interfacePrivate->configurationAction = CFRetain(val);
			}
			CFRelease(val);
		}
	} else {
		CFRelease(interfacePrivate);
		interfacePrivate = NULL;
	}

    done :

	if (interface_dict != NULL)		CFRelease(interface_dict);

	if (controller != MACH_PORT_NULL)	IOObjectRelease(controller);
	if (controller_dict != NULL)		CFRelease(controller_dict);

	if (bus != MACH_PORT_NULL)		IOObjectRelease(bus);
	if (bus_dict != NULL)			CFRelease(bus_dict);

	return (SCNetworkInterfaceRef)interfacePrivate;
}


static CFArrayRef
findMatchingInterfaces(CFDictionaryRef matching, processInterface func)
{
	CFMutableArrayRef	interfaces;
	io_registry_entry_t	interface;
	kern_return_t		kr;
	io_iterator_t		iterator	= MACH_PORT_NULL;

	kr = IOServiceGetMatchingServices(masterPort, matching, &iterator);
	if (kr != kIOReturnSuccess) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("findMatchingInterfaces IOServiceGetMatchingServices() failed, kr = 0x%x"), kr);
		return NULL;
	}

	interfaces = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	while ((interface = IOIteratorNext(iterator)) != MACH_PORT_NULL) {
		SCNetworkInterfaceRef		match;

		match = createInterface(interface, func);
		if (match != NULL) {
			CFArrayAppendValue(interfaces, match);
			CFRelease(match);
		}

		IOObjectRelease(interface);
	}

	IOObjectRelease(iterator);

	return interfaces;
}


#pragma mark -
#pragma mark helper functions


static CFIndex
findConfiguration(CFStringRef interface_type)
{
	CFIndex i;

	for (i = 0; i < sizeof(configurations)/sizeof(configurations[0]); i++) {
		if (CFEqual(interface_type, *configurations[i].interface_type)) {
			return i;
		}
	}

	return kCFNotFound;
}


__private_extern__
CFStringRef
__SCNetworkInterfaceGetDefaultConfigurationType(SCNetworkInterfaceRef interface)
{
	CFIndex				interfaceIndex;
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	if (interfacePrivate->serviceID == NULL) {
		// if not associated with a service (yet)
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	interfaceIndex = findConfiguration(interfacePrivate->interface_type);
	if (interfaceIndex == kCFNotFound) {
		// unknown interface type, use per-service configuration preferences
		return interfacePrivate->interface_type;	// entity
	}

	if (configurations[interfaceIndex].entity_hardware != NULL) {
		// if configuration information can be associated with this interface type
		return *configurations[interfaceIndex].entity_hardware;
	}

	_SCErrorSet(kSCStatusInvalidArgument);
	return NULL;
}


__private_extern__
Boolean
__SCNetworkInterfaceIsValidExtendedConfigurationType(SCNetworkInterfaceRef	interface,
						     CFStringRef		extendedType,
						     Boolean			requirePerInterface)
{
	CFStringRef			defaultType;
	CFIndex				extendedIndex;
	CFIndex				interfaceIndex;
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;
	Boolean				isL2TP			= FALSE;
	Boolean				ok			= FALSE;

	defaultType = __SCNetworkInterfaceGetDefaultConfigurationType(interface);
	if (defaultType == NULL) {
		goto done;
	}

	if (CFEqual(extendedType, defaultType)) {
		// extended and default configuration types cannot conflict
		goto done;
	}

	interfaceIndex = findConfiguration(interfacePrivate->interface_type);
	if (interfaceIndex == kCFNotFound) {
		// configuration information for unknown interface type's
		// are stored along with the service and we don't allow
		// per-service extended configurations
		goto done;
	}

	if (CFEqual(extendedType, kSCEntNetIPSec)) {
		CFStringRef	interfaceType;

		interfaceType = SCNetworkInterfaceGetInterfaceType(interface);
		if (CFEqual(interfaceType, kSCNetworkInterfaceTypePPP)) {
			SCNetworkInterfaceRef	child;

			child = SCNetworkInterfaceGetInterface(interface);
			if (child != NULL) {
				interfaceType = SCNetworkInterfaceGetInterfaceType(child);
				if (CFEqual(interfaceType, kSCNetworkInterfaceTypeL2TP)) {
					isL2TP = TRUE;
				}
			}
		}
	}

	if (requirePerInterface &&
	    !configurations[interfaceIndex].per_interface_config &&
	    !isL2TP) {
		// we don't allow per-service extended configurations (except
		// that we do allow IPSec as an extended type for PPP->L2TP)
		goto done;
	}

	extendedIndex = findConfiguration(extendedType);
	if ((extendedIndex != kCFNotFound) && !isL2TP) {
		// extended type cannot match a known interface type (except
		// that we do allow IPSec as an extended type for PPP->L2TP)
		goto done;
	}

	/*
	 * ???
	 * Do we match specific/known extended configuration types (e.g. EAPOL)
	 * and ensure that any non-standard extended configuration types be of
	 * the form com.myCompany.myType?
	 * ???
	 */

	ok = TRUE;

    done :

	if (!ok) {
		_SCErrorSet(kSCStatusInvalidArgument);
	}
	return ok;
}


typedef struct {
	CFStringRef		defaultType;
	CFMutableArrayRef	types;
} extendedConfiguration, *extendedConfigurationRef;


static void
__addExtendedConfigurationType(const void *key, const void *value, void *context)
{
	CFStringRef			extendedType	= (CFStringRef)key;
	extendedConfigurationRef	myContextRef	= (extendedConfigurationRef)context;

	if (CFEqual(extendedType, myContextRef->defaultType)) {
		// do not include the default configuration type
		return;
	}

	if (CFArrayContainsValue(myContextRef->types,
				 CFRangeMake(0, CFArrayGetCount(myContextRef->types)),
				 extendedType)) {
		// if extendedType already has already been added
		return;
	}

	CFArrayAppendValue(myContextRef->types, extendedType);

	return;
}


static CFArrayRef
extendedConfigurationTypes(SCNetworkInterfaceRef interface)
{
	CFIndex				i;
	CFIndex				interfaceIndex;
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;
	extendedConfiguration		myContext;
	SCNetworkServiceRef		service;
	CFArrayRef			sets;
	CFIndex				n;

	myContext.defaultType = __SCNetworkInterfaceGetDefaultConfigurationType(interface);
	if (myContext.defaultType == NULL) {
		myContext.types = NULL;
		goto done;
	}

	myContext.types = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	if (interfacePrivate->serviceID == NULL) {
		// if not associated with a service (yet)
		goto done;
	}

	interfaceIndex = findConfiguration(interfacePrivate->interface_type);
	if (interfaceIndex == kCFNotFound) {
		// we don't allow per-service extended configurations
		goto done;
	}

	if (!configurations[interfaceIndex].per_interface_config) {
		// known interface type but we still don't allow
		// per-service extended configurations
		goto done;
	}

	service = (SCNetworkServiceRef)__SCNetworkServiceCreatePrivate(NULL,
								       interfacePrivate->prefs,
								       interfacePrivate->serviceID,
								       interface);

	sets = SCNetworkSetCopyAll(interfacePrivate->prefs);
	n = (sets != NULL) ? CFArrayGetCount(sets) : 0;

	for (i = 0; i < n; i++) {
		CFDictionaryRef		configs;
		Boolean			found;
		CFStringRef		path;
		CFArrayRef		services;
		SCNetworkSetRef		set;

		set = CFArrayGetValueAtIndex(sets, i);
		services = SCNetworkSetCopyServices(set);
		found = CFArrayContainsValue(services,
					     CFRangeMake(0, CFArrayGetCount(services)),
					     service);
		CFRelease(services);

		if (!found) {
			continue;
		}

		// add stored extended configuration types
		path = SCPreferencesPathKeyCreateSetNetworkInterfaceEntity(NULL,				// allocator
									   SCNetworkSetGetSetID(set),		// set
									   interfacePrivate->entity_device,	// service
									   NULL);				// entity
		configs = __getPrefsConfiguration(interfacePrivate->prefs, path);
		CFRelease(path);
		if (isA_CFDictionary(configs)) {
			CFDictionaryApplyFunction(configs,
						  __addExtendedConfigurationType,
						  &myContext);
		}

		// add not-yet-stored extended configuration types
		if (interfacePrivate->unsaved != NULL) {
			CFDictionaryApplyFunction(interfacePrivate->unsaved,
						  __addExtendedConfigurationType,
						  &myContext);
		}

		break;
	}

	CFRelease(service);
	if (sets != NULL) CFRelease(sets);

    done :

	return myContext.types;
}


static CFArrayRef
copyConfigurationPaths(SCNetworkInterfacePrivateRef	interfacePrivate,
		       CFStringRef			extendedType)
{
	CFMutableArrayRef		array;
	CFIndex				i;
	CFIndex				interfaceIndex;
	CFIndex				n;
	CFStringRef			path;
	SCNetworkServiceRef		service;
	CFArrayRef			sets;

	array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	interfaceIndex = findConfiguration(interfacePrivate->interface_type);
	if (interfaceIndex == kCFNotFound) {
		// unknown interface type, use per-service configuration preferences
		path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
								      interfacePrivate->serviceID,	// service
								      extendedType);			// entity
		CFArrayAppendValue(array, path);
		CFRelease(path);
		return array;
	}

	if (!configurations[interfaceIndex].per_interface_config) {
		// known interface type, per-service configuration preferences
		path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
								      interfacePrivate->serviceID,	// service
								      extendedType);			// entity
		CFArrayAppendValue(array, path);
		CFRelease(path);
		return array;
	}

	// known interface type, per-interface configuration preferences
	//
	// 1. look for all sets which contain the associated service
	// 2. add a per-set path for the interface configuration for
	//    each set.

	service = (SCNetworkServiceRef)__SCNetworkServiceCreatePrivate(NULL,
								       interfacePrivate->prefs,
								       interfacePrivate->serviceID,
								       (SCNetworkInterfaceRef)interfacePrivate);

	sets = SCNetworkSetCopyAll(interfacePrivate->prefs);
	n = (sets != NULL) ? CFArrayGetCount(sets) : 0;

	for (i = 0; i < n; i++) {
		CFArrayRef      services;
		SCNetworkSetRef set;

		set = CFArrayGetValueAtIndex(sets, i);
		services = SCNetworkSetCopyServices(set);
		if (CFArrayContainsValue(services,
					 CFRangeMake(0, CFArrayGetCount(services)),
					 service)) {
			path = SCPreferencesPathKeyCreateSetNetworkInterfaceEntity(NULL,				// allocator
										   SCNetworkSetGetSetID(set),		// set
										   interfacePrivate->entity_device,	// service
										   extendedType);			// entity
			CFArrayAppendValue(array, path);
			CFRelease(path);
		}
		CFRelease(services);
	}

	if (CFArrayGetCount(array) == 0) {
		CFRelease(array);
		array = NULL;
	}

	CFRelease(service);
	if (sets != NULL) CFRelease(sets);
	return array;
}


#pragma mark -
#pragma mark SCNetworkInterface <--> preferences entity


__private_extern__
CFDictionaryRef
__SCNetworkInterfaceCopyInterfaceEntity(SCNetworkInterfaceRef interface)
{
	CFMutableDictionaryRef		entity;
	CFIndex				interfaceIndex;
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	entity = CFDictionaryCreateMutable(NULL,
					   0,
					   &kCFTypeDictionaryKeyCallBacks,
					   &kCFTypeDictionaryValueCallBacks);
	if (interfacePrivate->entity_type != NULL) {
		CFDictionarySetValue(entity,
				     kSCPropNetInterfaceType,
				     interfacePrivate->entity_type);
	}
	if (interfacePrivate->entity_subtype != NULL) {
		CFDictionarySetValue(entity,
				     kSCPropNetInterfaceSubType,
				     interfacePrivate->entity_subtype);
	}
	if (interfacePrivate->entity_device != NULL) {
		CFDictionarySetValue(entity,
				     kSCPropNetInterfaceDeviceName,
				     interfacePrivate->entity_device);
	}
	if (interfacePrivate->entity_device_unique != NULL) {
		CFDictionarySetValue(entity,
				     CFSTR("DeviceUniqueIdentifier"),
				     interfacePrivate->entity_device_unique);
	}

	// match the "hardware" with the lowest layer
	while (TRUE) {
		SCNetworkInterfaceRef	nextInterface;

		nextInterface = SCNetworkInterfaceGetInterface(interface);
		if (nextInterface == NULL) {
			break;
		}

		interface = nextInterface;
	}
	interfacePrivate = (SCNetworkInterfacePrivateRef)interface;

	if (CFEqual(interface, kSCNetworkInterfaceIPv4)) {
		return entity;
	}

	interfaceIndex = findConfiguration(interfacePrivate->interface_type);
	if (interfaceIndex != kCFNotFound) {
		if (configurations[interfaceIndex].entity_hardware != NULL) {
			CFDictionarySetValue(entity,
					     kSCPropNetInterfaceHardware,
					     *configurations[interfaceIndex].entity_hardware);
		}
	} else {
		CFDictionarySetValue(entity,
				     kSCPropNetInterfaceHardware,
				     interfacePrivate->interface_type);
	}

	// add the localized display name (which will only be used when/if the
	// interface is removed from the system)
	CFDictionarySetValue(entity,
			     kSCPropUserDefinedName,
			     SCNetworkInterfaceGetLocalizedDisplayName(interface));

	// note that this is a V.92 capable modem
	if (CFEqual(interfacePrivate->interface_type, kSCNetworkInterfaceTypeModem) &&
	    interfacePrivate->modemIsV92) {
		int		one     = 1;
		CFNumberRef     num;

		num = CFNumberCreate(NULL, kCFNumberIntType, &one);
		CFDictionarySetValue(entity,
				     kSCPropNetInterfaceSupportsModemOnHold,
				     num);
		CFRelease(num);
	}

	return entity;
}


#if	!TARGET_OS_IPHONE
static SCNetworkInterfaceRef
findInterface(CFArrayRef interfaces, CFStringRef match_if)
{
	CFIndex	i;
	CFIndex	n;

	n = CFArrayGetCount(interfaces);
	for (i = 0; i < n; i++) {
		SCNetworkInterfaceRef	interface	= CFArrayGetValueAtIndex(interfaces, i);
		CFStringRef		interfaceName;

		interfaceName = SCNetworkInterfaceGetBSDName(interface);
		if ((interfaceName != NULL) && CFEqual(interfaceName, match_if)) {
			CFRetain(interface);
			return interface;
		}
	}

	return NULL;
}


static SCNetworkInterfaceRef
findBondInterface(SCPreferencesRef prefs, CFStringRef ifDevice)
{
	CFArrayRef		bonds;
	SCNetworkInterfaceRef	interface	= NULL;

	if (prefs == NULL) {
		return (NULL);
	}

	// check if the interface is an Ethernet Bond
	bonds = SCBondInterfaceCopyAll(prefs);
	if (bonds != NULL) {
		interface = findInterface(bonds, ifDevice);
		CFRelease(bonds);
	}
	return interface;
}

static SCNetworkInterfaceRef
findVLANInterface(SCPreferencesRef prefs, CFStringRef ifDevice)
{
	SCNetworkInterfaceRef	interface	= NULL;
	CFArrayRef		vlans;

	if (prefs == NULL) {
		return (NULL);
	}

	// check if the interface is a VLAN
	vlans = SCVLANInterfaceCopyAll(prefs);
	if (vlans != NULL) {
		interface = findInterface(vlans, ifDevice);
		CFRelease(vlans);
	}
	return interface;
}
#endif	// !TARGET_OS_IPHONE


SCNetworkInterfaceRef
_SCNetworkInterfaceCreateWithBSDName(CFAllocatorRef		allocator,
				     CFStringRef		bsdName,
				     UInt32			flags)
{
	CFMutableDictionaryRef	entity;
	SCNetworkInterfaceRef	interface;

	entity = CFDictionaryCreateMutable(NULL,
					   0,
					   &kCFTypeDictionaryKeyCallBacks,
					   &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(entity, kSCPropNetInterfaceDeviceName, bsdName);
#if	!TARGET_OS_IPHONE
	if ((flags & kIncludeBondInterfaces) == 0) {
		CFDictionarySetValue(entity, CFSTR("_NO_BOND_INTERFACES_"), kCFBooleanTrue);
	}
	if ((flags & kIncludeVLANInterfaces) == 0) {
		CFDictionarySetValue(entity, CFSTR("_NO_VLAN_INTERFACES_"), kCFBooleanTrue);
	}
#endif	// !TARGET_OS_IPHONE
	interface = _SCNetworkInterfaceCreateWithEntity(NULL, entity, NULL);
	CFRelease(entity);

	return interface;
}


static void
__SCNetworkInterfaceSetService(SCNetworkInterfaceRef	interface,
			       SCNetworkServiceRef	service)
{
	SCNetworkInterfacePrivateRef	interfacePrivate;
	SCNetworkServicePrivateRef	servicePrivate;

	interfacePrivate = (SCNetworkInterfacePrivateRef)interface;
	if (interfacePrivate->prefs != NULL) {
		CFRelease(interfacePrivate->prefs);
		interfacePrivate->prefs = NULL;
	}
	if (interfacePrivate->serviceID != NULL) {
		CFRelease(interfacePrivate->serviceID);
		interfacePrivate->serviceID = NULL;
	}

	servicePrivate = (SCNetworkServicePrivateRef)service;
	if (servicePrivate->prefs != NULL) {
		interfacePrivate->prefs = CFRetain(servicePrivate->prefs);
	}
	if (servicePrivate->serviceID != NULL) {
		interfacePrivate->serviceID = CFRetain(servicePrivate->serviceID);
	}

	return;
}


static Boolean
_SCNetworkInterfaceMatchesName(CFStringRef name, CFStringRef key)
{
	Boolean		match;
	CFStringRef	str;

	if (bundle == NULL) {
		// if no bundle
		return FALSE;
	}

	if (!isA_CFString(name)) {
		// if no interface "name"
		return FALSE;
	}

	// check non-localized name for a match
	str = copy_interface_string(bundle, key, FALSE);
	if (str != NULL) {
		match = CFEqual(name, str);
		CFRelease(str);
		if (match) {
			return TRUE;
		}
	}

	// check localized name for a match
	str = copy_interface_string(bundle, key, TRUE);
	if (str != NULL) {
		match = CFEqual(name, str);
		CFRelease(str);
		if (match) {
			return TRUE;
		}
	}

	return FALSE;
}


SCNetworkInterfaceRef
_SCNetworkInterfaceCreateWithEntity(CFAllocatorRef		allocator,
				    CFDictionaryRef		interface_entity,
				    SCNetworkServiceRef		service)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= NULL;
	CFStringRef			ifDevice;
	CFStringRef			ifName			= NULL;
	CFStringRef			ifSubType;
	CFStringRef			ifType;
	CFStringRef			ifUnique;
	CFArrayRef			matching_interfaces	= NULL;

	/* initialize runtime (and kSCNetworkInterfaceIPv4) */
	pthread_once(&initialized, __SCNetworkInterfaceInitialize);

	ifType = CFDictionaryGetValue(interface_entity, kSCPropNetInterfaceType);
	if (ifType == NULL) {
		/*
		 * The interface "Type" was not specified.  We'll make an
		 * assumption that this is an "Ethernet" interface.  If a
		 * real interface exists with the provided interface name
		 * then the actual type will be set accordingly. If not, we'll
		 * end up crafting an "Ethernet" SCNetworkInterface which
		 * will keep the rest of the configuration APIs happy.
		 */
		ifType = kSCValNetInterfaceTypeEthernet;
	}

	if (!isA_CFString(ifType)) {
		return NULL;
	}

	ifSubType = CFDictionaryGetValue(interface_entity, kSCPropNetInterfaceSubType);
	if (CFEqual(ifType, kSCValNetInterfaceTypePPP)) {
		if (!isA_CFString(ifSubType)) {
			return NULL;
		}
	}

	ifDevice = CFDictionaryGetValue(interface_entity, kSCPropNetInterfaceDeviceName);
	ifUnique = CFDictionaryGetValue(interface_entity, CFSTR("DeviceUniqueIdentifier"));

	if (CFEqual(ifType, kSCValNetInterfaceTypeEthernet) ||
	    CFEqual(ifType, kSCValNetInterfaceTypeFireWire) ||
	    (CFEqual(ifType, kSCValNetInterfaceTypePPP) && CFEqual(ifSubType, kSCValNetInterfaceSubTypePPPoE))) {
		char			bsdName[IFNAMSIZ + 1];
		CFMutableDictionaryRef	matching;

		if (!isA_CFString(ifDevice)) {
			return NULL;
		}

		if (_SC_cfstring_to_cstring(ifDevice, bsdName, sizeof(bsdName), kCFStringEncodingASCII) == NULL) {
			goto done;
		}

		matching = IOBSDNameMatching(masterPort, 0, bsdName);
		if (matching == NULL) {
			goto done;
		}

		// note: the "matching" dictionary will be consumed by the following
		matching_interfaces = findMatchingInterfaces(matching, processNetworkInterface);

	} else if (CFEqual(ifType, kSCValNetInterfaceTypePPP)) {
		if (CFEqual(ifSubType, kSCValNetInterfaceSubTypePPPSerial)) {
			CFDictionaryRef	matching;
			CFStringRef	match_keys[2];
			CFStringRef	match_vals[2];

			if (!isA_CFString(ifDevice)) {
				return NULL;
			}

			match_keys[0] = CFSTR(kIOProviderClassKey);
			match_vals[0] = CFSTR(kIOSerialBSDServiceValue);

			match_keys[1] = CFSTR(kIOTTYBaseNameKey);
			match_vals[1] = ifDevice;

			matching = CFDictionaryCreate(NULL,
						      (const void **)match_keys,
						      (const void **)match_vals,
						      sizeof(match_keys)/sizeof(match_keys[0]),
						      &kCFTypeDictionaryKeyCallBacks,
						      &kCFTypeDictionaryValueCallBacks);

			// note: the "matching" dictionary will be consumed by the following
			matching_interfaces = findMatchingInterfaces(matching, processSerialInterface);

			if (ifUnique == NULL) {
				CFIndex	n;
				Boolean	useDeviceName	= TRUE;

				n = (matching_interfaces != NULL) ? CFArrayGetCount(matching_interfaces) : 0;
				if (n > 0) {
					CFIndex	i;

					for (i = 0; i < n; i++) {
						SCNetworkInterfacePrivateRef	scanPrivate;

						scanPrivate = (SCNetworkInterfacePrivateRef)CFArrayGetValueAtIndex(matching_interfaces, i);
						if (scanPrivate->entity_device_unique != NULL) {
							useDeviceName = FALSE;
							break;
						}
					}
				}

				if (useDeviceName) {
					if (matching_interfaces != NULL) {
						CFRelease(matching_interfaces);
					}

					match_keys[1] = CFSTR(kIOTTYDeviceKey);
					matching = CFDictionaryCreate(NULL,
								      (const void **)match_keys,
								      (const void **)match_vals,
								      sizeof(match_keys)/sizeof(match_keys[0]),
								      &kCFTypeDictionaryKeyCallBacks,
								      &kCFTypeDictionaryValueCallBacks);

					// note: the "matching" dictionary will be consumed by the following
					matching_interfaces = findMatchingInterfaces(matching, processSerialInterface);
				}
			}
		} else if (CFEqual(ifSubType, kSCValNetInterfaceSubTypeL2TP)) {
			interfacePrivate = (SCNetworkInterfacePrivateRef)SCNetworkInterfaceCreateWithInterface(kSCNetworkInterfaceIPv4,
													       kSCNetworkInterfaceTypeL2TP);
		} else if (CFEqual(ifSubType, kSCValNetInterfaceSubTypePPTP)) {
			interfacePrivate = (SCNetworkInterfacePrivateRef)SCNetworkInterfaceCreateWithInterface(kSCNetworkInterfaceIPv4,
													       kSCNetworkInterfaceTypePPTP);
		} else {
			// XXX do we allow non-Apple variants of PPP??? XXX
			interfacePrivate = (SCNetworkInterfacePrivateRef)SCNetworkInterfaceCreateWithInterface(kSCNetworkInterfaceIPv4,
													       ifSubType);
		}
	} else if (CFEqual(ifType, kSCValNetInterfaceType6to4)) {
		if (!isA_CFString(ifDevice)) {
			return NULL;
		}

		interfacePrivate = (SCNetworkInterfacePrivateRef)SCNetworkInterfaceCreateWithInterface(kSCNetworkInterfaceIPv4,
												       kSCNetworkInterfaceType6to4);
	} else if (CFEqual(ifType, kSCValNetInterfaceTypeIPSec)) {
		interfacePrivate = (SCNetworkInterfacePrivateRef)SCNetworkInterfaceCreateWithInterface(kSCNetworkInterfaceIPv4,
												       kSCNetworkInterfaceTypeIPSec);
	} else if ((CFStringFind(ifType, CFSTR("."), 0).location != kCFNotFound) && (ifDevice == NULL)) {
		interfacePrivate = (SCNetworkInterfacePrivateRef)SCNetworkInterfaceCreateWithInterface(kSCNetworkInterfaceIPv4,
												       ifType);
	}

	if (matching_interfaces != NULL) {
		CFIndex 		n;
		SCPreferencesRef	prefs;

		n = CFArrayGetCount(matching_interfaces);
		switch (n) {
			case 1 :
				interfacePrivate = (SCNetworkInterfacePrivateRef)CFArrayGetValueAtIndex(matching_interfaces, 0);
				if (_SC_CFEqual(ifUnique, interfacePrivate->entity_device_unique)) {
					// if the unique ID's  match
					CFRetain(interfacePrivate);
					break;
				}

				interfacePrivate = NULL;
				// fall through
			case 0 :
				if (!CFEqual(ifType, kSCValNetInterfaceTypeEthernet)) {
					break;
				}
				prefs = SCPreferencesCreate(NULL, CFSTR("SCNetworkInterface"), NULL);
				if (prefs == NULL) {
					break;
				}
#if	!TARGET_OS_IPHONE
				if (!CFDictionaryContainsKey(interface_entity, CFSTR("_NO_VLAN_INTERFACES_"))) {
					interfacePrivate = (SCNetworkInterfacePrivateRef)findVLANInterface(prefs, ifDevice);
				}
				if ((interfacePrivate == NULL)
				    && !CFDictionaryContainsKey(interface_entity, CFSTR("_NO_BOND_INTERFACES_"))) {
					interfacePrivate = (SCNetworkInterfacePrivateRef)findBondInterface(prefs, ifDevice);
				}
#endif	// !TARGET_OS_IPHONE
				CFRelease(prefs);
				break;
			default :
				if (ifUnique != NULL) {
					CFIndex	i;

					// we are looking for an interface with a unique ID
					// so let's try to focus our choices
					for (i = 0; i < n; i++) {
						SCNetworkInterfacePrivateRef	scanPrivate;

						scanPrivate = (SCNetworkInterfacePrivateRef)CFArrayGetValueAtIndex(matching_interfaces, i);
						if (_SC_CFEqual(ifUnique, scanPrivate->entity_device_unique)) {
							if (interfacePrivate != NULL) {
								// if we've matched more than one interface
								interfacePrivate = NULL;
								break;
							}
							interfacePrivate = scanPrivate;
						}
					}
				} else if (CFDictionaryGetValueIfPresent(interface_entity,
									kSCPropUserDefinedName,
									(const void **)&ifName)) {
					CFIndex	i;

					// we don't have a unique ID but do have an interface
					// name.  If the matching interfaces do have IDs than
					// we can try to focus our choices using the name
					for (i = 0; i < n; i++) {
						SCNetworkInterfacePrivateRef	scanPrivate;

						scanPrivate = (SCNetworkInterfacePrivateRef)CFArrayGetValueAtIndex(matching_interfaces, i);
						if (scanPrivate->entity_device_unique != NULL) {
							SCNetworkInterfaceRef	scan	= (SCNetworkInterfaceRef)scanPrivate;
							CFStringRef		scanName;

							scanName = __SCNetworkInterfaceGetNonLocalizedDisplayName(scan);
							if ((scanName != NULL) && !_SC_CFEqual(ifName, scanName)) {
								continue; // if not the same display name
							}
						}

						if (interfacePrivate != NULL) {
							// if we've matched more than one interface
							interfacePrivate = NULL;
							break;
						}
						interfacePrivate = scanPrivate;
					}
				}
				if (interfacePrivate == NULL) {
					SCLog(TRUE, LOG_ERR, CFSTR("_SCNetworkInterfaceCreateWithEntity() failed, more than one interface matches %@"), ifDevice);
					interfacePrivate = (SCNetworkInterfacePrivateRef)CFArrayGetValueAtIndex(matching_interfaces, 0);
				}
				CFRetain(interfacePrivate);
				break;
		}
		CFRelease(matching_interfaces);
	}

    done :

	if (interfacePrivate == NULL) {
		/*
		 * if device not present on this system
		 */
		interfacePrivate = __SCNetworkInterfaceCreatePrivate(NULL, NULL, NULL, NULL, NULL);
		interfacePrivate->entity_type          = ifType;
		interfacePrivate->entity_subtype       = ifSubType;
		interfacePrivate->entity_device        = (ifDevice != NULL) ? CFStringCreateCopy(NULL, ifDevice) : NULL;
		interfacePrivate->entity_device_unique = (ifUnique != NULL) ? CFStringCreateCopy(NULL, ifUnique) : NULL;

		if (CFEqual(ifType, kSCValNetInterfaceTypeEthernet)) {
			CFStringRef	entity_hardware;

			entity_hardware = CFDictionaryGetValue(interface_entity, kSCPropNetInterfaceHardware);
			if (isA_CFString((entity_hardware)) &&
			    CFEqual(entity_hardware, kSCEntNetAirPort)) {
				interfacePrivate->interface_type = kSCNetworkInterfaceTypeIEEE80211;
				interfacePrivate->sort_order     = kSortAirPort;
			} else {
				CFStringRef	name;

				interfacePrivate->interface_type = kSCNetworkInterfaceTypeEthernet;

				name = CFDictionaryGetValue(interface_entity, kSCPropUserDefinedName);
				if (_SCNetworkInterfaceMatchesName(name, CFSTR("iPhone"))) {
					interfacePrivate->sort_order = kSortTethered;
				} else if (_SCNetworkInterfaceMatchesName(name, CFSTR("bluetooth-pan"))) {
					interfacePrivate->sort_order = kSortBluetoothPAN;
				} else {
					interfacePrivate->sort_order = kSortEthernet;
				}
			}
		} else if (CFEqual(ifType, kSCValNetInterfaceTypeFireWire)) {
			interfacePrivate->interface_type = kSCNetworkInterfaceTypeFireWire;
			interfacePrivate->sort_order     = kSortFireWire;
		} else if (CFEqual(ifType, kSCValNetInterfaceTypePPP)) {
			if (CFEqual(ifSubType, kSCValNetInterfaceSubTypePPPoE)) {
				CFStringRef	entity_hardware;

				entity_hardware = CFDictionaryGetValue(interface_entity, kSCPropNetInterfaceHardware);
				if (isA_CFString((entity_hardware)) &&
				    CFEqual(entity_hardware, kSCEntNetAirPort)) {
					interfacePrivate->interface_type = kSCNetworkInterfaceTypeIEEE80211;
					interfacePrivate->sort_order     = kSortAirPort;
				} else {
					interfacePrivate->interface_type = kSCNetworkInterfaceTypeEthernet;
					interfacePrivate->sort_order     = kSortEthernet;
				}
			} else if (CFEqual(ifSubType, kSCValNetInterfaceSubTypePPPSerial)) {
				if (CFStringHasPrefix(ifDevice, CFSTR("Bluetooth"))) {
					interfacePrivate->interface_type = kSCNetworkInterfaceTypeBluetooth;
					interfacePrivate->sort_order     = kSortBluetooth;
				} else if (CFStringHasPrefix(ifDevice, CFSTR("irda"))) {
					interfacePrivate->interface_type = kSCNetworkInterfaceTypeIrDA;
					interfacePrivate->sort_order     = kSortIrDA;
				} else if (CFStringHasPrefix(ifDevice, CFSTR("wwan"))) {
					interfacePrivate->interface_type = kSCNetworkInterfaceTypeWWAN;
					interfacePrivate->sort_order     = kSortWWAN;
				} else {
					interfacePrivate->interface_type = kSCNetworkInterfaceTypeModem;
					interfacePrivate->sort_order     = kSortModem;
				}
			} else {
				// PPTP, L2TP, ...
				CFRelease(interfacePrivate);
				interfacePrivate = (SCNetworkInterfacePrivateRef)kSCNetworkInterfaceIPv4;
				CFRetain(interfacePrivate);
			}
		} else if (CFStringFind(ifType, CFSTR("."), 0).location != kCFNotFound) {
			// if vendor interface
			interfacePrivate->interface_type = ifType;
		} else {
			// if unknown interface
			CFRelease(interfacePrivate);
			interfacePrivate = NULL;
		}
	}

	if ((interfacePrivate != NULL) && (service != NULL)) {
		__SCNetworkInterfaceSetService((SCNetworkInterfaceRef)interfacePrivate,
					       service);
#if	!TARGET_OS_IPHONE
		// set prefs & serviceID to VLANs and Bonds
		if (CFEqual(interfacePrivate->interface_type, kSCNetworkInterfaceTypeBond)) {
			CFIndex		i;
			CFArrayRef	members;
			CFIndex		n;

			members = SCBondInterfaceGetMemberInterfaces((SCNetworkInterfaceRef)interfacePrivate);
			n = (members != NULL) ? CFArrayGetCount(members) : 0;
			for (i = 0; i < n; i++) {
				SCNetworkInterfaceRef	member;

				member = CFArrayGetValueAtIndex(members, i);
				__SCNetworkInterfaceSetService(member, service);
			}
		} else if (CFEqual(interfacePrivate->interface_type, kSCNetworkInterfaceTypeVLAN)) {
			SCNetworkInterfaceRef	vlan_physical;

			vlan_physical = SCVLANInterfaceGetPhysicalInterface((SCNetworkInterfaceRef)interfacePrivate);
			if (vlan_physical != NULL) {
				__SCNetworkInterfaceSetService(vlan_physical, service);
			}
		}
#endif	// !TARGET_OS_IPHONE
	}

	if (CFEqual(ifType, kSCValNetInterfaceTypePPP)) {
		SCNetworkInterfaceRef   parent;

		// create parent
		parent = SCNetworkInterfaceCreateWithInterface((SCNetworkInterfaceRef)interfacePrivate,
							       kSCNetworkInterfaceTypePPP);
		CFRelease(interfacePrivate);
		interfacePrivate = (SCNetworkInterfacePrivateRef)parent;
	}

	return (SCNetworkInterfaceRef)interfacePrivate;
}


#pragma mark -
#pragma mark SCNetworkInterface APIs


__private_extern__
CFArrayRef
__SCNetworkInterfaceCopyAll_IONetworkInterface(void)
{
	CFDictionaryRef		matching;
	CFArrayRef		new_interfaces;

	// get Ethernet, Firewire, and AirPort interfaces

	matching = IOServiceMatching(kIONetworkInterfaceClass);
	new_interfaces = findMatchingInterfaces(matching, processNetworkInterface);

	return new_interfaces;
}


static
CFArrayRef
__SCNetworkInterfaceCopyAll_Modem()
{
	CFDictionaryRef		matching;
	CFStringRef		match_keys[2];
	CFStringRef		match_vals[2];
	CFArrayRef		new_interfaces;

	match_keys[0] = CFSTR(kIOProviderClassKey);
	match_vals[0] = CFSTR(kIOSerialBSDServiceValue);

	match_keys[1] = CFSTR(kIOSerialBSDTypeKey);
	match_vals[1] = CFSTR(kIOSerialBSDModemType);

	matching = CFDictionaryCreate(NULL,
				      (const void **)match_keys,
				      (const void **)match_vals,
				      sizeof(match_keys)/sizeof(match_keys[0]),
				      &kCFTypeDictionaryKeyCallBacks,
				      &kCFTypeDictionaryValueCallBacks);
	new_interfaces = findMatchingInterfaces(matching, processSerialInterface);

	return new_interfaces;
}


static
CFArrayRef
__SCNetworkInterfaceCopyAll_RS232()
{
	CFDictionaryRef		matching;
	CFStringRef		match_keys[2];
	CFStringRef		match_vals[2];
	CFArrayRef		new_interfaces;

	match_keys[0] = CFSTR(kIOProviderClassKey);
	match_vals[0] = CFSTR(kIOSerialBSDServiceValue);

	match_keys[1] = CFSTR(kIOSerialBSDTypeKey);
	match_vals[1] = CFSTR(kIOSerialBSDRS232Type);

	matching = CFDictionaryCreate(NULL,
				      (const void **)match_keys,
				      (const void **)match_vals,
				      sizeof(match_keys)/sizeof(match_keys[0]),
				      &kCFTypeDictionaryKeyCallBacks,
				      &kCFTypeDictionaryValueCallBacks);
	new_interfaces = findMatchingInterfaces(matching, processSerialInterface);

	return new_interfaces;
}


static void
add_interfaces(CFMutableArrayRef all_interfaces, CFArrayRef new_interfaces)
{
	CFIndex	i;
	CFIndex	n;

	n = CFArrayGetCount(new_interfaces);
	for (i = 0; i < n; i++) {
		CFStringRef		bsdName;
		SCNetworkInterfaceRef	interface;

		interface = CFArrayGetValueAtIndex(new_interfaces, i);
		bsdName = SCNetworkInterfaceGetBSDName(interface);
		if (bsdName != NULL) {
			CFArrayAppendValue(all_interfaces, interface);
		}
	}

	return;
}


static void
__waitForInterfaces()
{
	CFStringRef		key;
	CFArrayRef		keys;
	Boolean			ok;
	SCDynamicStoreRef	store;

	store = SCDynamicStoreCreate(NULL, CFSTR("SCNetworkInterfaceCopyAll"), NULL, NULL);
	if (store == NULL) {
		return;
	}

	key = SCDynamicStoreKeyCreate(NULL, CFSTR("%@" "InterfaceNamer"), kSCDynamicStoreDomainPlugin);
	keys = CFArrayCreate(NULL, (const void **)&key, 1, &kCFTypeArrayCallBacks);
	ok = SCDynamicStoreSetNotificationKeys(store, keys, NULL);
	CFRelease(keys);
	if (!ok) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCDynamicStoreSetNotificationKeys() failed: %s"), SCErrorString(SCError()));
		goto done;
	}

	while (TRUE) {
		CFArrayRef	changedKeys;
		CFDictionaryRef	dict;
		Boolean		quiet	= FALSE;

		// check if quiet
		dict = SCDynamicStoreCopyValue(store, key);
		if (dict != NULL) {
			if (isA_CFDictionary(dict) &&
			    (CFDictionaryContainsKey(dict, CFSTR("*QUIET*")) ||
			     CFDictionaryContainsKey(dict, CFSTR("*TIMEOUT*")))) {
				quiet = TRUE;
			}
			CFRelease(dict);
		}
		if (quiet) {
			break;
		}

		ok = SCDynamicStoreNotifyWait(store);
		if (!ok) {
			SCLog(TRUE, LOG_ERR,
			      CFSTR("SCDynamicStoreNotifyWait() failed: %s"), SCErrorString(SCError()));
			goto done;
		}

		changedKeys = SCDynamicStoreCopyNotifiedKeys(store);
		if (changedKeys != NULL) {
			CFRelease(changedKeys);
		}
	}

    done :

	CFRelease(key);
	CFRelease(store);
	return;
}


CFArrayRef /* of SCNetworkInterfaceRef's */
SCNetworkInterfaceCopyAll()
{
	CFMutableArrayRef	all_interfaces;
	CFArrayRef		new_interfaces;
#if	!TARGET_OS_IPHONE
	SCPreferencesRef	prefs;
#endif	// !TARGET_OS_IPHONE

	/* initialize runtime */
	pthread_once(&initialized, __SCNetworkInterfaceInitialize);

	/* wait for IOKit to quiesce */
	pthread_once(&iokit_quiet, __waitForInterfaces);

	all_interfaces = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	// get Ethernet, Firewire, and AirPort interfaces
	new_interfaces = __SCNetworkInterfaceCopyAll_IONetworkInterface();
	if (new_interfaces != NULL) {
		add_interfaces(all_interfaces, new_interfaces);
		CFRelease(new_interfaces);
	}

	// get Modem interfaces
	new_interfaces = __SCNetworkInterfaceCopyAll_Modem();
	if (new_interfaces != NULL) {
		add_interfaces(all_interfaces, new_interfaces);
		CFRelease(new_interfaces);
	}

	// get serial (RS232) interfaces
	new_interfaces = __SCNetworkInterfaceCopyAll_RS232();
	if (new_interfaces != NULL) {
		add_interfaces(all_interfaces, new_interfaces);
		CFRelease(new_interfaces);
	}

#if	!TARGET_OS_IPHONE
	// get virtual network interfaces (Bond, VLAN)
	prefs = SCPreferencesCreate(NULL, CFSTR("SCNetworkInterfaceCopyAll"), NULL);
	if (prefs != NULL) {
		new_interfaces = SCBondInterfaceCopyAll(prefs);
		if (new_interfaces != NULL) {
			add_interfaces(all_interfaces, new_interfaces);
			CFRelease(new_interfaces);
		}

		new_interfaces = SCVLANInterfaceCopyAll(prefs);
		if (new_interfaces != NULL) {
			add_interfaces(all_interfaces, new_interfaces);
			CFRelease(new_interfaces);
		}

		CFRelease(prefs);
	}
#endif	// !TARGET_OS_IPHONE

	// all interfaces have been identified, order and return
	sort_interfaces(all_interfaces);

	return all_interfaces;
}


CFArrayRef /* of kSCNetworkInterfaceTypeXXX CFStringRef's */
SCNetworkInterfaceGetSupportedInterfaceTypes(SCNetworkInterfaceRef interface)
{
	CFIndex				i;
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	if (!isA_SCNetworkInterface(interface)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (interfacePrivate->supported_interface_types != NULL) {
		goto done;
	}

	i = findConfiguration(interfacePrivate->interface_type);
	if (i != kCFNotFound) {
		if (configurations[i].supported_interfaces != doNone) {
			interfacePrivate->supported_interface_types = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			if (configurations[i].supported_interfaces & do6to4) {
				CFArrayAppendValue(interfacePrivate->supported_interface_types, kSCNetworkInterfaceType6to4);
			}
			if (configurations[i].supported_interfaces & doL2TP) {
				CFArrayAppendValue(interfacePrivate->supported_interface_types, kSCNetworkInterfaceTypeL2TP);
			}
			if (configurations[i].supported_interfaces & doPPP) {
				CFArrayAppendValue(interfacePrivate->supported_interface_types, kSCNetworkInterfaceTypePPP);
			}
			if (configurations[i].supported_interfaces & doPPTP) {
				CFArrayAppendValue(interfacePrivate->supported_interface_types, kSCNetworkInterfaceTypePPTP);
			}
			if (configurations[i].supported_interfaces & doIPSec) {
				CFArrayAppendValue(interfacePrivate->supported_interface_types, kSCNetworkInterfaceTypeIPSec);
			}
		}
	}

    done :

	return interfacePrivate->supported_interface_types;
}


CFArrayRef /* of kSCNetworkProtocolTypeXXX CFStringRef's */
SCNetworkInterfaceGetSupportedProtocolTypes(SCNetworkInterfaceRef interface)
{
	CFIndex				i;
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	if (!isA_SCNetworkInterface(interface)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (interfacePrivate->supported_protocol_types != NULL) {
		goto done;
	}

	i = findConfiguration(interfacePrivate->interface_type);
	if (i != kCFNotFound) {
		if (configurations[i].supported_protocols != doNone) {
			interfacePrivate->supported_protocol_types = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
#if	!TARGET_OS_IPHONE
			if (configurations[i].supported_protocols & doAppleTalk) {
				CFArrayAppendValue(interfacePrivate->supported_protocol_types, kSCNetworkProtocolTypeAppleTalk);
			}
#endif	// !TARGET_OS_IPHONE
			if (configurations[i].supported_protocols & doDNS) {
				CFArrayAppendValue(interfacePrivate->supported_protocol_types, kSCNetworkProtocolTypeDNS);
			}
			if (configurations[i].supported_protocols & doIPv4) {
				CFArrayAppendValue(interfacePrivate->supported_protocol_types, kSCNetworkProtocolTypeIPv4);
			}
			if (configurations[i].supported_protocols & doIPv6) {
				CFArrayAppendValue(interfacePrivate->supported_protocol_types, kSCNetworkProtocolTypeIPv6);
			}
			if (configurations[i].supported_protocols & doProxies) {
				CFArrayAppendValue(interfacePrivate->supported_protocol_types, kSCNetworkProtocolTypeProxies);
			}
#if	!TARGET_OS_IPHONE
			if (configurations[i].supported_protocols & doSMB) {
				CFArrayAppendValue(interfacePrivate->supported_protocol_types, kSCNetworkProtocolTypeSMB);
			}
#endif	// !TARGET_OS_IPHONE
		}
	}

    done :

	return interfacePrivate->supported_protocol_types;
}


SCNetworkInterfaceRef
SCNetworkInterfaceCreateWithInterface(SCNetworkInterfaceRef child, CFStringRef interfaceType)
{
	SCNetworkInterfacePrivateRef	childPrivate	= (SCNetworkInterfacePrivateRef)child;
	CFIndex				childIndex;
	SCNetworkInterfacePrivateRef	parentPrivate;

	if (!isA_SCNetworkInterface(child)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (!isA_CFString(interfaceType)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	childIndex = findConfiguration(childPrivate->interface_type);

	parentPrivate = __SCNetworkInterfaceCreatePrivate(NULL,
							  child,
							  childPrivate->prefs,
							  childPrivate->serviceID,
							  NULL);
	if (parentPrivate == NULL) {
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	if (CFEqual(interfaceType, kSCNetworkInterfaceTypePPP)) {
		parentPrivate->interface_type = kSCNetworkInterfaceTypePPP;
		parentPrivate->entity_type    = kSCValNetInterfaceTypePPP;

		// entity subtype
		if (childIndex != kCFNotFound) {
			if (configurations[childIndex].ppp_subtype != NULL) {
				parentPrivate->entity_subtype = *configurations[childIndex].ppp_subtype;
			} else {
				// sorry, the child interface does not support PPP
				goto fail;
			}
		} else {
			// if the child's interface type not known, use the child entities "Type"
			parentPrivate->entity_subtype = childPrivate->entity_type;
		}

		if (childPrivate->entity_device != NULL) {
			parentPrivate->entity_device = CFStringCreateCopy(NULL, childPrivate->entity_device);
		}

		if (childPrivate->entity_device_unique != NULL) {
			parentPrivate->entity_device_unique = CFStringCreateCopy(NULL, childPrivate->entity_device_unique);
		}
	} else if (CFEqual(interfaceType, kSCNetworkInterfaceTypeL2TP)) {
		if ((childIndex == kCFNotFound) ||
		    ((configurations[childIndex].supported_interfaces & doL2TP) != doL2TP)) {
			// if the child interface does not support L2TP
			goto fail;
		}
		parentPrivate->interface_type = kSCNetworkInterfaceTypeL2TP;
		parentPrivate->localized_key  = CFSTR("l2tp");
		parentPrivate->entity_type    = kSCEntNetL2TP;			// interface config goes into "L2TP"
	} else if (CFEqual(interfaceType, kSCNetworkInterfaceTypePPTP)) {
		if ((childIndex == kCFNotFound) ||
		    ((configurations[childIndex].supported_interfaces & doPPTP) != doPPTP)) {
			// if the child interface does not support PPTP
			goto fail;
		}
		parentPrivate->interface_type = kSCNetworkInterfaceTypePPTP;
		parentPrivate->localized_key  = CFSTR("pptp");
		parentPrivate->entity_type    = kSCEntNetPPTP;			// interface config goes into "PPTP"
	} else if (CFEqual(interfaceType, kSCNetworkInterfaceType6to4)) {
		if ((childIndex == kCFNotFound) ||
		    ((configurations[childIndex].supported_interfaces & do6to4) != do6to4)) {
			// if the child interface does not support 6to4
			goto fail;
		}

		parentPrivate->interface_type = kSCNetworkInterfaceType6to4;
		parentPrivate->localized_key  = CFSTR("6to4");
		parentPrivate->entity_type    = kSCValNetInterfaceType6to4;
		parentPrivate->entity_device  = CFRetain(CFSTR("stf0"));
	} else if (CFEqual(interfaceType, kSCNetworkInterfaceTypeIPSec)) {
		if ((childIndex == kCFNotFound) ||
		    ((configurations[childIndex].supported_interfaces & doIPSec) != doIPSec)) {
			// if the child interface does not support IPSec
			goto fail;
		}
		parentPrivate->interface_type = kSCNetworkInterfaceTypeIPSec;
		parentPrivate->localized_key  = CFSTR("ipsec");
		parentPrivate->entity_type    = kSCValNetInterfaceTypeIPSec;
	} else if (CFStringFind(interfaceType, CFSTR("."), 0).location != kCFNotFound) {
		// if custom interface type
		parentPrivate->interface_type = interfaceType;
		parentPrivate->entity_type    = interfaceType;			// interface config goes into a
										// a dictionary with the same
										// name as the interfaceType
	} else {
		// unknown interface type
		goto fail;
	}

	if (childPrivate->overrides != NULL) {
		parentPrivate->overrides = CFDictionaryCreateMutableCopy(NULL, 0, childPrivate->overrides);
	}

	// The following change handles the case where a user has both an Ethernet and
	// PPPoE network service. Because a PPPoE service is typically associated with
	// an ISP we want it to be sorted higher in the service order.
	if ((parentPrivate->entity_subtype != NULL) &&
	    (CFEqual(parentPrivate->entity_subtype, kSCValNetInterfaceSubTypePPPoE))) {
		if ((childPrivate->interface_type != NULL) &&
		    (CFEqual(childPrivate->interface_type, kSCNetworkInterfaceTypeIEEE80211))) {
			parentPrivate->sort_order = kSortAirportPPP;
		} else {
			parentPrivate->sort_order = kSortEthernetPPP;
		}
	} else {
		// set sort order of the parent to match the child interface
		parentPrivate->sort_order = childPrivate->sort_order;
	}

	return (SCNetworkInterfaceRef)parentPrivate;

    fail :

	CFRelease(parentPrivate);
	_SCErrorSet(kSCStatusInvalidArgument);
	return NULL;
}


__private_extern__
CFDictionaryRef
__SCNetworkInterfaceGetDefaultConfiguration(SCNetworkSetRef set, SCNetworkInterfaceRef interface)
{
	CFDictionaryRef			config			= NULL;
	CFStringRef			defaultType;
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	/* initialize runtime (and kSCNetworkInterfaceIPv4) */
	pthread_once(&initialized, __SCNetworkInterfaceInitialize);

	defaultType = __SCNetworkInterfaceGetDefaultConfigurationType(interface);
	if (defaultType != NULL) {
		if (set != NULL) {
			CFStringRef	path;

			path = SCPreferencesPathKeyCreateSetNetworkInterfaceEntity(NULL,				// allocator
										   SCNetworkSetGetSetID(set),		// set
										   interfacePrivate->entity_device,	// interface
										   defaultType);			// entity
			if (path != NULL) {
				config = __getPrefsConfiguration(interfacePrivate->prefs, path);
				CFRelease(path);

				if (config == NULL) {
					// if the "set" does not have a saved configuration, use
					// the [template] "interface" configuration
					if (interfacePrivate->unsaved != NULL) {
						config = CFDictionaryGetValue(interfacePrivate->unsaved, defaultType);
						if (config == (CFDictionaryRef)kCFNull) {
							config = NULL;
						}
					}
				}
				if (isA_CFDictionary(config) && (CFDictionaryGetCount(config) == 0)) {
					config = NULL;
				}
			}
		}
	}

	return config;
}


static CFDictionaryRef
__SCNetworkInterfaceGetConfiguration(SCNetworkInterfaceRef	interface,
				     CFStringRef		extendedType)
{
	CFDictionaryRef			config			= NULL;
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;
	CFArrayRef			paths;

	/* initialize runtime (and kSCNetworkInterfaceIPv4) */
	pthread_once(&initialized, __SCNetworkInterfaceInitialize);

	paths = copyConfigurationPaths(interfacePrivate, extendedType);
	if (paths != NULL) {
		CFStringRef			path;

		path = CFArrayGetValueAtIndex(paths, 0);
		config = __getPrefsConfiguration(interfacePrivate->prefs, path);

		CFRelease(paths);
	} else {
		if (interfacePrivate->unsaved != NULL) {
			config = CFDictionaryGetValue(interfacePrivate->unsaved, extendedType);
			if (config == (CFDictionaryRef)kCFNull) {
				config = NULL;
			}
		}
	}

	if (isA_CFDictionary(config) && (CFDictionaryGetCount(config) == 0)) {
		config = NULL;
	}

	return config;
}


CFDictionaryRef
SCNetworkInterfaceGetConfiguration(SCNetworkInterfaceRef interface)
{
	CFDictionaryRef	config;
	CFStringRef	defaultType;

	if (!isA_SCNetworkInterface(interface)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	defaultType = __SCNetworkInterfaceGetDefaultConfigurationType(interface);
	if (defaultType == NULL) {
		return NULL;
	}

	config = __SCNetworkInterfaceGetConfiguration(interface, defaultType);
	if (config == NULL) {
		if (CFEqual(defaultType, kSCEntNetAirPort)) {
			SCNetworkInterfacePrivateRef	interfacePrivate;
			CFStringRef			path;

			// if AirPort interface, check for a per-service config
			interfacePrivate = (SCNetworkInterfacePrivateRef)interface;
			path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
									      interfacePrivate->serviceID,	// service
									      kSCEntNetAirPort);		// entity
			config = __getPrefsConfiguration(interfacePrivate->prefs, path);
			CFRelease(path);
		}
	}
	if (config == NULL) {
		_SCErrorSet(kSCStatusOK);
	}

	return config;
}


CFDictionaryRef
SCNetworkInterfaceGetExtendedConfiguration(SCNetworkInterfaceRef	interface,
					   CFStringRef			extendedType)
{
	CFDictionaryRef	config;

	if (!isA_SCNetworkInterface(interface)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (!__SCNetworkInterfaceIsValidExtendedConfigurationType(interface, extendedType, TRUE)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	config = __SCNetworkInterfaceGetConfiguration(interface, extendedType);
	if (config == NULL) {
		_SCErrorSet(kSCStatusOK);
	}

	return config;
}


CFStringRef
SCNetworkInterfaceGetBSDName(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	if (!isA_SCNetworkInterface(interface)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if ((interfacePrivate->interface != NULL) &&
	    (interfacePrivate->interface != kSCNetworkInterfaceIPv4)) {
		_SCErrorSet(kSCStatusOK);
		return NULL;
	}

	return interfacePrivate->entity_device;
}


CFStringRef
SCNetworkInterfaceGetHardwareAddressString(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	if (!isA_SCNetworkInterface(interface)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if ((interfacePrivate->address != NULL) &&
	    (interfacePrivate->addressString == NULL)) {
		uint8_t		*bp;
		char		*cp;
		CFIndex		n;
		char		mac[sizeof("xx:xx:xx:xx:xx:xx:xx:xx")];
		char		*mac_p	= mac;

		bp = (uint8_t *)CFDataGetBytePtr(interfacePrivate->address);
		n  = CFDataGetLength(interfacePrivate->address) * 3;

		if (n > sizeof(mac)) {
			mac_p = CFAllocatorAllocate(NULL, 0, n);
		}

		for (cp = mac_p; n > 0; n -= 3) {
			cp += snprintf(cp, n, "%2.2x:", *bp++);
		}

		interfacePrivate->addressString = CFStringCreateWithCString(NULL, mac_p, kCFStringEncodingUTF8);
		if (mac_p != mac)	CFAllocatorDeallocate(NULL, mac_p);
	}

	return interfacePrivate->addressString;
}


SCNetworkInterfaceRef
SCNetworkInterfaceGetInterface(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	if (!isA_SCNetworkInterface(interface)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	return interfacePrivate->interface;
}


CFStringRef
SCNetworkInterfaceGetInterfaceType(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	if (!isA_SCNetworkInterface(interface)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	return interfacePrivate->interface_type;
}


static CFStringRef
copy_interface_string(CFBundleRef bundle, CFStringRef key, Boolean localized)
{
	CFStringRef	str	= NULL;

	if (localized) {
		str = CFBundleCopyLocalizedString(bundle,
						  key,
						  key,
						  NETWORKINTERFACE_LOCALIZATIONS);
	} else {
		str = _SC_CFBundleCopyNonLocalizedString(bundle,
							 key,
							 key,
							 NETWORKINTERFACE_LOCALIZATIONS);
	}

	return str;
}


static CFStringRef
copy_display_name(SCNetworkInterfaceRef interface, Boolean localized, Boolean oldLocalization)
{
	CFMutableStringRef	local;
	CFStringRef		name;

	local = CFStringCreateMutable(NULL, 0);

	while (interface != NULL) {
		Boolean				added			= FALSE;
		SCNetworkInterfaceRef		child			= NULL;
		SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

		if ((interfacePrivate->interface != NULL) &&
		    (interfacePrivate->interface != kSCNetworkInterfaceIPv4)) {
			child = interfacePrivate->interface;
		}

		if ((bundle != NULL) && (interfacePrivate->localized_key != NULL)) {
			CFStringRef	fmt;
			CFStringRef	key	= interfacePrivate->localized_key;

			if (oldLocalization) {
				key = CFStringCreateWithFormat(NULL, NULL, CFSTR("X-%@"),
							       interfacePrivate->localized_key);
			}
			fmt = copy_interface_string(bundle, key, localized);
			if (fmt != NULL) {
				CFStringAppendFormat(local,
						     NULL,
						     fmt,
						     interfacePrivate->localized_arg1,
						     interfacePrivate->localized_arg2);
				CFRelease(fmt);
				added = TRUE;
			}
			if (oldLocalization) {
				CFRelease(key);
			}
		}

		if (!added &&
		    (interfacePrivate->prefs != NULL) &&
		    (interfacePrivate->serviceID != NULL) &&
		    (child == NULL)) {
			CFDictionaryRef	entity;
			CFStringRef	path;

			// check for (and use) the name of the interface when it
			// was last available
			path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,
									      interfacePrivate->serviceID,
									      kSCEntNetInterface);
			entity = SCPreferencesPathGetValue(interfacePrivate->prefs, path);
			CFRelease(path);
			if (isA_CFDictionary(entity)) {
				CFStringRef	name;

				name = CFDictionaryGetValue(entity, kSCPropUserDefinedName);
				if (isA_CFString(name)) {
					CFStringAppend(local, name);
					added = TRUE;
				}
			}
		}

		if (!added) {
			// create (non-)localized name based on the interface type
			CFStringAppend(local, interfacePrivate->interface_type);

			// ... and, if this is a leaf node, the interface device
			if ((interfacePrivate->entity_device != NULL) && (child == NULL)) {
				CFStringAppendFormat(local, NULL, CFSTR(" (%@)"), interfacePrivate->entity_device);
			}
		}

		if (child != NULL) {
			// if this interface is layered over another
			CFStringAppend(local, CFSTR(" --> "));
		}

		interface = child;
	}

	name = CFStringCreateCopy(NULL, local);
	CFRelease(local);

	return name;
}


__private_extern__
CFStringRef
__SCNetworkInterfaceCopyXLocalizedDisplayName(SCNetworkInterfaceRef interface)
{
	CFStringRef			name;

	if (!isA_SCNetworkInterface(interface)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	name = copy_display_name(interface, TRUE, TRUE);
	return name;
}


__private_extern__
CFStringRef
__SCNetworkInterfaceCopyXNonLocalizedDisplayName(SCNetworkInterfaceRef interface)
{
	CFStringRef			localized_name;

	if (!isA_SCNetworkInterface(interface)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	localized_name = copy_display_name(interface, FALSE, TRUE);
	return localized_name;
}


__private_extern__
CFStringRef
__SCNetworkInterfaceGetNonLocalizedDisplayName(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	if (!isA_SCNetworkInterface(interface)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (interfacePrivate->name == NULL) {
		interfacePrivate->name = copy_display_name(interface, FALSE, FALSE);
	}

	return interfacePrivate->name;
}


CFStringRef
SCNetworkInterfaceGetLocalizedDisplayName(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	if (!isA_SCNetworkInterface(interface)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (interfacePrivate->localized_name == NULL) {
		interfacePrivate->localized_name = copy_display_name(interface, TRUE, FALSE);
	}

	return interfacePrivate->localized_name;
}


__private_extern__
CFDictionaryRef
__SCNetworkInterfaceGetTemplateOverrides(SCNetworkInterfaceRef interface, CFStringRef interfaceType)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;
	CFDictionaryRef			overrides		= NULL;

	if (interfacePrivate->overrides != NULL) {
		overrides = CFDictionaryGetValue(interfacePrivate->overrides, interfaceType);
	}

	return overrides;
}


CFTypeID
SCNetworkInterfaceGetTypeID(void)
{
	pthread_once(&initialized, __SCNetworkInterfaceInitialize);	/* initialize runtime */
	return __kSCNetworkInterfaceTypeID;
}


__private_extern__
Boolean
__SCNetworkInterfaceSetDefaultConfiguration(SCNetworkSetRef		set,
					    SCNetworkInterfaceRef	interface,
					    CFStringRef			defaultType,
					    CFDictionaryRef		config,
					    Boolean			okToHold)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;
	Boolean				ok			= FALSE;

	/* initialize runtime (and kSCNetworkInterfaceIPv4) */
	pthread_once(&initialized, __SCNetworkInterfaceInitialize);

	if (defaultType == NULL) {
		defaultType = __SCNetworkInterfaceGetDefaultConfigurationType(interface);
		if (defaultType == NULL) {
			return FALSE;
		}
	}

	if (isA_CFDictionary(config) && (CFDictionaryGetCount(config) == 0)) {
		config = NULL;
	}

	if (set != NULL) {
		CFStringRef	path;

		path = SCPreferencesPathKeyCreateSetNetworkInterfaceEntity(NULL,				// allocator
									   SCNetworkSetGetSetID(set),		// set
									   interfacePrivate->entity_device,	// interface
									   defaultType);			// entity
		if (path != NULL) {
			ok = __setPrefsConfiguration(interfacePrivate->prefs, path, config, FALSE);
			CFRelease(path);
			if (ok) {
				// if configuration has been saved
				if (interfacePrivate->unsaved != NULL) {
					CFDictionaryRemoveValue(interfacePrivate->unsaved, defaultType);
					if (CFDictionaryGetCount(interfacePrivate->unsaved) == 0) {
						CFRelease(interfacePrivate->unsaved);
						interfacePrivate->unsaved = NULL;
					}
				}
			}
		} else {
			if (okToHold) {
				if (config == NULL) {
					// remember that we are clearing the configuration
					config = (CFDictionaryRef)kCFNull;
				}

				if (interfacePrivate->unsaved == NULL) {
					interfacePrivate->unsaved = CFDictionaryCreateMutable(NULL,
											      0,
											      &kCFTypeDictionaryKeyCallBacks,
											      &kCFTypeDictionaryValueCallBacks);
				}
				CFDictionarySetValue(interfacePrivate->unsaved, defaultType, config);
				ok = TRUE;
			} else {
				_SCErrorSet(kSCStatusNoKey);
			}
		}
	}

	return ok;
}


__private_extern__
Boolean
__SCNetworkInterfaceSetConfiguration(SCNetworkInterfaceRef	interface,
				     CFStringRef		extendedType,
				     CFDictionaryRef		config,
				     Boolean			okToHold)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;
	Boolean				ok			= FALSE;
	CFArrayRef			paths;

	/* initialize runtime (and kSCNetworkInterfaceIPv4) */
	pthread_once(&initialized, __SCNetworkInterfaceInitialize);

	if (extendedType == NULL) {
		extendedType = __SCNetworkInterfaceGetDefaultConfigurationType(interface);
		if (extendedType == NULL) {
			return FALSE;
		}
	}

	if (isA_CFDictionary(config) && (CFDictionaryGetCount(config) == 0)) {
		config = NULL;
	}

	paths = copyConfigurationPaths(interfacePrivate, extendedType);
	if (paths != NULL) {
		CFIndex				i;
		CFIndex				n;

		n = CFArrayGetCount(paths);
		for (i = 0; i < n; i++) {
			CFStringRef     path;

			path = CFArrayGetValueAtIndex(paths, i);
			ok = __setPrefsConfiguration(interfacePrivate->prefs, path, config, FALSE);
			if (!ok) {
				break;
			}
		}

		if (ok) {
			// if configuration has been saved
			if (interfacePrivate->unsaved != NULL) {
				CFDictionaryRemoveValue(interfacePrivate->unsaved, extendedType);
				if (CFDictionaryGetCount(interfacePrivate->unsaved) == 0) {
					CFRelease(interfacePrivate->unsaved);
					interfacePrivate->unsaved = NULL;
				}
			}
		}

		CFRelease(paths);
	} else {
		if (okToHold) {
			if (config == NULL) {
				// remember that we are clearing the configuration
				config = (CFDictionaryRef)kCFNull;
			}

			if (interfacePrivate->unsaved == NULL) {
				interfacePrivate->unsaved = CFDictionaryCreateMutable(NULL,
										      0,
										      &kCFTypeDictionaryKeyCallBacks,
										      &kCFTypeDictionaryValueCallBacks);
			}
			CFDictionarySetValue(interfacePrivate->unsaved, extendedType, config);
			ok = TRUE;
		} else {
			_SCErrorSet(kSCStatusNoKey);
		}
	}

	return ok;
}


Boolean
SCNetworkInterfaceSetConfiguration(SCNetworkInterfaceRef interface, CFDictionaryRef config)
{
	CFStringRef	defaultType;

	if (!isA_SCNetworkInterface(interface)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	defaultType = __SCNetworkInterfaceGetDefaultConfigurationType(interface);
	if (defaultType == NULL) {
		return FALSE;
	}

	return __SCNetworkInterfaceSetConfiguration(interface, defaultType, config, FALSE);
}


Boolean
SCNetworkInterfaceSetExtendedConfiguration(SCNetworkInterfaceRef	interface,
					   CFStringRef			extendedType,
					   CFDictionaryRef		config)
{
	if (!isA_SCNetworkInterface(interface)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!__SCNetworkInterfaceIsValidExtendedConfigurationType(interface, extendedType, TRUE)) {
		return FALSE;
	}

	return __SCNetworkInterfaceSetConfiguration(interface, extendedType, config, FALSE);
}


#pragma mark -
#pragma mark SCNetworkInterface [Refresh Configuration] API


#ifndef kSCEntNetRefreshConfiguration
#define kSCEntNetRefreshConfiguration	CFSTR("RefreshConfiguration")
#endif	// kSCEntNetRefreshConfiguration

Boolean
_SCNetworkInterfaceForceConfigurationRefresh(CFStringRef ifName)
{
	CFStringRef		key;
	Boolean			ok     = FALSE;
	SCDynamicStoreRef	store;

	if (!isA_CFString(ifName)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	store = SCDynamicStoreCreate(NULL, CFSTR("_SCNetworkInterfaceForceConfigurationRefresh"), NULL, NULL);
	if (store == NULL) {
		return FALSE;
	}

	key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainState,
							    ifName,
							    kSCEntNetRefreshConfiguration);
	ok = SCDynamicStoreNotifyValue(store, key);
	CFRelease(key);
	CFRelease(store);
	return ok;
}


static Boolean
__SCNetworkInterfaceForceConfigurationRefresh_helper(SCPreferencesRef prefs, CFStringRef ifName)
{
	CFDataRef		data		= NULL;
	Boolean			ok;
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	uint32_t		status		= kSCStatusOK;
	CFDataRef		reply		= NULL;

	if (prefsPrivate->helper == -1) {
		ok = __SCPreferencesCreate_helper(prefs);
		if (!ok) {
			return FALSE;
		}
	}

	// serialize the interface name
	ok = _SCSerializeString(ifName, &data, NULL, NULL);
	if (!ok) {
		goto fail;
	}

	// have the helper "refresh" the configuration
	status = kSCStatusOK;
	reply  = NULL;
	ok = _SCHelperExec(prefsPrivate->helper,
			   SCHELPER_MSG_INTERFACE_REFRESH,
			   data,
			   &status,
			   NULL);
	if (data != NULL) CFRelease(data);
	if (!ok) {
		goto fail;
	}

	if (status != kSCStatusOK) {
		goto error;
	}

	return TRUE;

    fail :

	// close helper
	if (prefsPrivate->helper != -1) {
		_SCHelperClose(prefsPrivate->helper);
		prefsPrivate->helper = -1;
	}

	status = kSCStatusAccessError;

    error :

	// return error
	_SCErrorSet(status);
	return FALSE;
}


Boolean
SCNetworkInterfaceForceConfigurationRefresh(SCNetworkInterfaceRef interface)
{
	CFStringRef			ifName;
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	if (!isA_SCNetworkInterface(interface)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	ifName = SCNetworkInterfaceGetBSDName(interface);
	if (ifName == NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (interfacePrivate->prefs != NULL) {
		SCPreferencesRef		prefs		= interfacePrivate->prefs;
		SCPreferencesPrivateRef		prefsPrivate	= (SCPreferencesPrivateRef)prefs;

		if (prefsPrivate->authorizationData != NULL) {
			return __SCNetworkInterfaceForceConfigurationRefresh_helper(prefs, ifName);
		}
	}

	return _SCNetworkInterfaceForceConfigurationRefresh(ifName);
}


Boolean
SCNetworkInterfaceRefreshConfiguration(CFStringRef ifName)
{
	return _SCNetworkInterfaceForceConfigurationRefresh(ifName);
}


#pragma mark -
#pragma mark SCNetworkInterface Password APIs


static CFStringRef
getPasswordID(CFDictionaryRef config, CFStringRef serviceID)
{
	CFStringRef	unique_id	= NULL;

	if (config != NULL) {
		CFStringRef	encryption;

		encryption = CFDictionaryGetValue(config, kSCPropNetPPPAuthPasswordEncryption);
		if (isA_CFString(encryption) &&
		    CFEqual(encryption, kSCValNetPPPAuthPasswordEncryptionKeychain)) {
			unique_id = CFDictionaryGetValue(config, kSCPropNetPPPAuthPassword);
		}
	}
	if (unique_id == NULL) {
		unique_id = serviceID;
	}

	return unique_id;
}


static CFStringRef
copySharedSecretID(CFDictionaryRef config, CFStringRef serviceID)
{
	CFMutableStringRef	shared_id	= NULL;

	if (config != NULL) {
		CFStringRef	encryption;

		encryption = CFDictionaryGetValue(config, kSCPropNetIPSecSharedSecretEncryption);
		if (isA_CFString(encryption) &&
		    CFEqual(encryption, kSCValNetIPSecSharedSecretEncryptionKeychain)) {
			shared_id = (CFMutableStringRef)CFDictionaryGetValue(config, kSCPropNetIPSecSharedSecret);
			if (shared_id != NULL) {
				CFRetain(shared_id);
			}
		}
	}

	if (shared_id == NULL) {
		CFStringRef	unique_id;

		unique_id = getPasswordID(config, serviceID);
		shared_id = CFStringCreateMutableCopy(NULL, 0, unique_id);
		CFStringAppend(shared_id, CFSTR(".SS"));
	}

	return shared_id;
}


static CFStringRef
copyXAuthID(CFDictionaryRef config, CFStringRef serviceID)
{
	CFMutableStringRef	xauth_id	= NULL;

	if (config != NULL) {
		CFStringRef	encryption;

		encryption = CFDictionaryGetValue(config, kSCPropNetIPSecXAuthPasswordEncryption);
		if (isA_CFString(encryption) &&
		    CFEqual(encryption, kSCValNetIPSecXAuthPasswordEncryptionKeychain)) {
			xauth_id = (CFMutableStringRef)CFDictionaryGetValue(config, kSCPropNetIPSecXAuthPassword);
			if (xauth_id != NULL) {
				CFRetain(xauth_id);
			}
		}
	}

	if (xauth_id == NULL) {
		CFStringRef	unique_id;

		unique_id = getPasswordID(config, serviceID);
		xauth_id = CFStringCreateMutableCopy(NULL, 0, unique_id);
		CFStringAppend(xauth_id, CFSTR(".XAUTH"));
	}

	return xauth_id;
}


static Boolean
checkInterfacePassword(SCNetworkInterfaceRef		interface,
		       SCNetworkInterfacePasswordType	passwordType,
		       SCPreferencesRef			*prefs,
		       CFStringRef			*serviceID)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	if (!isA_SCNetworkInterface(interface)) {
		goto error;
	}

	*serviceID = interfacePrivate->serviceID;
	if (*serviceID == NULL) {
		goto error;
	}

	*prefs = interfacePrivate->prefs;
	if (*prefs == NULL) {
		goto error;
	}

	switch (passwordType) {
		case kSCNetworkInterfacePasswordTypePPP : {
			CFStringRef	interfaceType;

			interfaceType = SCNetworkInterfaceGetInterfaceType(interface);
			if (CFEqual(interfaceType, kSCNetworkInterfaceTypePPP)) {
				// if PPP
				break;
			}

			goto error;
		}

		case kSCNetworkInterfacePasswordTypeIPSecSharedSecret : {
			CFStringRef	interfaceType;

			interfaceType = SCNetworkInterfaceGetInterfaceType(interface);
			if (CFEqual(interfaceType, kSCNetworkInterfaceTypePPP)) {
				interface = SCNetworkInterfaceGetInterface(interface);
				if (interface != NULL) {
					interfaceType = SCNetworkInterfaceGetInterfaceType(interface);
					if (CFEqual(interfaceType, kSCNetworkInterfaceTypeL2TP)) {
						// if PPP->L2TP interface
						break;
					}
				}
			} else if (CFEqual(interfaceType, kSCNetworkInterfaceTypeIPSec)) {
				// if IPSec interface
				break;
			}

			goto error;
		}

		case kSCNetworkInterfacePasswordTypeEAPOL : {
			break;
		}

		case kSCNetworkInterfacePasswordTypeIPSecXAuth : {
			CFStringRef	interfaceType;

			interfaceType = SCNetworkInterfaceGetInterfaceType(interface);
			if (CFEqual(interfaceType, kSCNetworkInterfaceTypeIPSec)) {
				// if IPSec interface
				break;
			}

			goto error;
		}

		default :
			break;
	}

	return TRUE;

    error :

	_SCErrorSet(kSCStatusInvalidArgument);
	return FALSE;
}


Boolean
SCNetworkInterfaceCheckPassword(SCNetworkInterfaceRef		interface,
				SCNetworkInterfacePasswordType	passwordType)
{
	Boolean			exists		= FALSE;
	SCPreferencesRef        prefs		= NULL;
	CFStringRef		serviceID	= NULL;

	if (!checkInterfacePassword(interface, passwordType, &prefs, &serviceID)) {
		return FALSE;
	}

	switch (passwordType) {
		case kSCNetworkInterfacePasswordTypePPP : {
			CFDictionaryRef	config;
			CFStringRef	unique_id;

			// get configuration
			config = SCNetworkInterfaceGetConfiguration(interface);

			// get serviceID
			unique_id = getPasswordID(config, serviceID);

			// check
			exists = __extract_password(prefs,
						    config,
						    kSCPropNetPPPAuthPassword,
						    kSCPropNetPPPAuthPasswordEncryption,
						    kSCValNetPPPAuthPasswordEncryptionKeychain,
						    unique_id,
						    NULL);
			break;
		}

		case kSCNetworkInterfacePasswordTypeIPSecSharedSecret : {
			CFDictionaryRef	config;
			Boolean		extended;
			CFStringRef	shared_id;

			// get configuration
			extended = CFEqual(SCNetworkInterfaceGetInterfaceType(interface), kSCNetworkInterfaceTypePPP);
			if (extended) {
				config = SCNetworkInterfaceGetExtendedConfiguration(interface, kSCEntNetIPSec);
			} else {
				config = SCNetworkInterfaceGetConfiguration(interface);
			}

			// get sharedSecret ID
			shared_id = copySharedSecretID(config, serviceID);

			// check
			exists = __extract_password(prefs,
						    config,
						    kSCPropNetIPSecSharedSecret,
						    kSCPropNetIPSecSharedSecretEncryption,
						    kSCValNetIPSecSharedSecretEncryptionKeychain,
						    shared_id,
						    NULL);
			CFRelease(shared_id);
			break;
		}

		case kSCNetworkInterfacePasswordTypeEAPOL : {
			CFDictionaryRef	config;
			CFStringRef	unique_id	= NULL;

			// get configuration
			config = SCNetworkInterfaceGetExtendedConfiguration(interface, kSCEntNetEAPOL);

			// get 802.1X identifier
			if (config != NULL) {
				unique_id = CFDictionaryGetValue(config, kEAPClientPropUserPasswordKeychainItemID);
			}
			if (!isA_CFString(unique_id)) {
				return FALSE;
			}

			// check password
			exists = _SCPreferencesSystemKeychainPasswordItemExists(prefs, unique_id);
			break;
		}

		case kSCNetworkInterfacePasswordTypeIPSecXAuth : {
			CFDictionaryRef	config;
			CFStringRef	xauth_id;

			// get configuration
			config = SCNetworkInterfaceGetConfiguration(interface);

			// get XAuth ID
			xauth_id = copyXAuthID(config, serviceID);

			// check
			exists = __extract_password(prefs,
						    config,
						    kSCPropNetIPSecXAuthPassword,
						    kSCPropNetIPSecXAuthPasswordEncryption,
						    kSCValNetIPSecXAuthPasswordEncryptionKeychain,
						    xauth_id,
						    NULL);
			CFRelease(xauth_id);
			break;
		}

		default :
			_SCErrorSet(kSCStatusInvalidArgument);
			return FALSE;
	}

	return exists;
}


CFDataRef
SCNetworkInterfaceCopyPassword(SCNetworkInterfaceRef		interface,
			       SCNetworkInterfacePasswordType	passwordType)
{
	CFDataRef		password	= NULL;
	SCPreferencesRef        prefs		= NULL;
	CFStringRef		serviceID	= NULL;

	if (!checkInterfacePassword(interface, passwordType, &prefs, &serviceID)) {
		return NULL;
	}

	switch (passwordType) {
		case kSCNetworkInterfacePasswordTypePPP : {
			CFDictionaryRef	config;
			CFStringRef	unique_id;

			// get configuration
			config = SCNetworkInterfaceGetConfiguration(interface);

			// get serviceID
			unique_id = getPasswordID(config, serviceID);

			// extract
			(void) __extract_password(prefs,
						  config,
						  kSCPropNetPPPAuthPassword,
						  kSCPropNetPPPAuthPasswordEncryption,
						  kSCValNetPPPAuthPasswordEncryptionKeychain,
						  unique_id,
						  &password);
			break;
		}

		case kSCNetworkInterfacePasswordTypeIPSecSharedSecret : {
			CFDictionaryRef	config;
			Boolean		extended;
			CFStringRef	shared_id;

			// get configuration
			extended = CFEqual(SCNetworkInterfaceGetInterfaceType(interface), kSCNetworkInterfaceTypePPP);
			if (extended) {
				config = SCNetworkInterfaceGetExtendedConfiguration(interface, kSCEntNetIPSec);
			} else {
				config = SCNetworkInterfaceGetConfiguration(interface);
			}

			// get sharedSecret ID
			shared_id = copySharedSecretID(config, serviceID);

			// extract
			(void) __extract_password(prefs,
						  config,
						  kSCPropNetIPSecSharedSecret,
						  kSCPropNetIPSecSharedSecretEncryption,
						  kSCValNetIPSecSharedSecretEncryptionKeychain,
						  shared_id,
						  &password);

			CFRelease(shared_id);
			break;
		}

		case kSCNetworkInterfacePasswordTypeEAPOL : {
			CFDictionaryRef	config;
			CFStringRef	unique_id	= NULL;

			// get configuration
			config = SCNetworkInterfaceGetExtendedConfiguration(interface, kSCEntNetEAPOL);

			// get 802.1X identifier
			if (config != NULL) {
				unique_id = CFDictionaryGetValue(config, kEAPClientPropUserPasswordKeychainItemID);
			}
			if (!isA_CFString(unique_id)) {
				_SCErrorSet(kSCStatusFailed);
				return NULL;
			}

			// copy password
			password = _SCPreferencesSystemKeychainPasswordItemCopy(prefs, unique_id);
			break;
		}

		case kSCNetworkInterfacePasswordTypeIPSecXAuth : {
			CFDictionaryRef	config;
			CFStringRef	xauth_id;

			// get configuration
			config = SCNetworkInterfaceGetConfiguration(interface);

			// get XAuth ID
			xauth_id = copyXAuthID(config, serviceID);

			// extract
			(void) __extract_password(prefs,
						  config,
						  kSCPropNetIPSecXAuthPassword,
						  kSCPropNetIPSecXAuthPasswordEncryption,
						  kSCValNetIPSecXAuthPasswordEncryptionKeychain,
						  xauth_id,
						  &password);
			CFRelease(xauth_id);
			break;
		}

		default :
			_SCErrorSet(kSCStatusInvalidArgument);
			return NULL;
	}

	return password;
}


Boolean
SCNetworkInterfaceRemovePassword(SCNetworkInterfaceRef		interface,
				 SCNetworkInterfacePasswordType	passwordType)
{
	Boolean			ok		= FALSE;
	SCPreferencesRef        prefs		= NULL;
	CFStringRef		serviceID	= NULL;

	if (!checkInterfacePassword(interface, passwordType, &prefs, &serviceID)) {
		return FALSE;
	}

	switch (passwordType) {
		case kSCNetworkInterfacePasswordTypePPP : {
			CFDictionaryRef	config;
			CFDictionaryRef	newConfig	= NULL;
			CFStringRef	unique_id;

			// get configuration
			config = SCNetworkInterfaceGetConfiguration(interface);

			// get serviceID
			unique_id = getPasswordID(config, serviceID);

			// remove password
			ok = __remove_password(prefs,
					       config,
					       kSCPropNetPPPAuthPassword,
					       kSCPropNetPPPAuthPasswordEncryption,
					       kSCValNetPPPAuthPasswordEncryptionKeychain,
					       unique_id,
					       &newConfig);
			if (ok) {
				ok = SCNetworkInterfaceSetConfiguration(interface, newConfig);
				if (newConfig != NULL) CFRelease(newConfig);
			}

			break;
		}

		case kSCNetworkInterfacePasswordTypeIPSecSharedSecret : {
			CFDictionaryRef	config;
			Boolean		extended;
			CFDictionaryRef	newConfig	= NULL;
			CFStringRef	shared_id;

			// get configuration
			extended = CFEqual(SCNetworkInterfaceGetInterfaceType(interface), kSCNetworkInterfaceTypePPP);
			if (extended) {
				config = SCNetworkInterfaceGetExtendedConfiguration(interface, kSCEntNetIPSec);
			} else {
				config = SCNetworkInterfaceGetConfiguration(interface);
			}

			// get sharedSecret ID
			shared_id = copySharedSecretID(config, serviceID);

			// remove password
			ok = __remove_password(prefs,
					       config,
					       kSCPropNetIPSecSharedSecret,
					       kSCPropNetIPSecSharedSecretEncryption,
					       kSCValNetIPSecSharedSecretEncryptionKeychain,
					       shared_id,
					       &newConfig);
			if (ok) {
				if (extended) {
					ok = SCNetworkInterfaceSetExtendedConfiguration(interface,
											kSCEntNetIPSec,
											newConfig);
				} else {
					ok = SCNetworkInterfaceSetConfiguration(interface,
										newConfig);
				}
				if (newConfig != NULL) CFRelease(newConfig);
			}

			CFRelease(shared_id);
			break;
		}

		case kSCNetworkInterfacePasswordTypeEAPOL : {
			CFDictionaryRef	config;
			CFStringRef	unique_id	= NULL;

			// get configuration
			config = SCNetworkInterfaceGetExtendedConfiguration(interface, kSCEntNetEAPOL);

			// get 802.1X identifier
			if (config != NULL) {
				unique_id = CFDictionaryGetValue(config, kEAPClientPropUserPasswordKeychainItemID);
			}
			if (!isA_CFString(unique_id)) {
				_SCErrorSet(kSCStatusFailed);
				return FALSE;
			}

			// remove password
			ok = _SCPreferencesSystemKeychainPasswordItemRemove(prefs, unique_id);
			break;
		}

		case kSCNetworkInterfacePasswordTypeIPSecXAuth : {
			CFDictionaryRef	config;
			CFDictionaryRef	newConfig	= NULL;
			CFStringRef	xauth_id;

			// get configuration
			config = SCNetworkInterfaceGetConfiguration(interface);

			// get XAuth ID
			xauth_id = copyXAuthID(config, serviceID);

			// remove password
			ok = __remove_password(prefs,
					       config,
					       kSCPropNetIPSecXAuthPassword,
					       kSCPropNetIPSecXAuthPasswordEncryption,
					       kSCValNetIPSecXAuthPasswordEncryptionKeychain,
					       xauth_id,
					       &newConfig);
			if (ok) {
				ok = SCNetworkInterfaceSetConfiguration(interface, newConfig);
				if (newConfig != NULL) CFRelease(newConfig);
			}

			CFRelease(xauth_id);
			break;
		}

		default :
			_SCErrorSet(kSCStatusInvalidArgument);
			return FALSE;
	}

	return ok;
}


Boolean
SCNetworkInterfaceSetPassword(SCNetworkInterfaceRef		interface,
			      SCNetworkInterfacePasswordType	passwordType,
			      CFDataRef				password,
			      CFDictionaryRef			options)
{
	CFStringRef		account		= NULL;
	CFDictionaryRef		config;
	CFStringRef		description	= NULL;
	CFStringRef		label		= NULL;
	Boolean			ok		= FALSE;
	SCPreferencesRef        prefs		= NULL;
	CFStringRef		serviceID	= NULL;

	if (!checkInterfacePassword(interface, passwordType, &prefs, &serviceID)) {
		return FALSE;
	}

	switch (passwordType) {
		case kSCNetworkInterfacePasswordTypePPP : {
			SCNetworkServiceRef	service	= NULL;
			CFStringRef		unique_id;

			// get configuration
			config = SCNetworkInterfaceGetConfiguration(interface);

			// get serviceID
			unique_id = getPasswordID(config, serviceID);

			// get "Account", "Name", "Kind"
			if (config != NULL) {
				// auth name --> keychain "Account"
				account = CFDictionaryGetValue(config, kSCPropNetPPPAuthName);

				// PPP [user defined] "name" --> keychain "Name"
				label = CFDictionaryGetValue(config, kSCPropUserDefinedName);
			}

			if (label == NULL) {
				// service name --> keychain "Name"
				service = (SCNetworkServiceRef)__SCNetworkServiceCreatePrivate(NULL,
											       prefs,
											       serviceID,
											       interface);

				label = SCNetworkServiceGetName(service);
				if (label == NULL) {
					// interface name --> keychain "Name"
					label = SCNetworkInterfaceGetLocalizedDisplayName(interface);
				}
			}

			if (bundle != NULL) {
				// "PPP Password" --> keychain "Kind"
				description = CFBundleCopyLocalizedString(bundle,
									  CFSTR("KEYCHAIN_KIND_PPP_PASSWORD"),
									  CFSTR("PPP Password"),
									  NULL);
			}

			// store password
			ok = _SCPreferencesSystemKeychainPasswordItemSet(prefs,
									 unique_id,
									 (label != NULL)       ? label       : CFSTR("Network Connection"),
									 (description != NULL) ? description : CFSTR("PPP Password"),
									 account,
									 password,
									 options);
			if (ok) {
				CFMutableDictionaryRef	newConfig;

				if (config != NULL) {
					newConfig = CFDictionaryCreateMutableCopy(NULL, 0, config);
				} else {
					newConfig = CFDictionaryCreateMutable(NULL,
									      0,
									      &kCFTypeDictionaryKeyCallBacks,
									      &kCFTypeDictionaryValueCallBacks);
				}
				CFDictionarySetValue(newConfig,
						     kSCPropNetPPPAuthPassword,
						     unique_id);
				CFDictionarySetValue(newConfig,
						     kSCPropNetPPPAuthPasswordEncryption,
						     kSCValNetPPPAuthPasswordEncryptionKeychain);
				ok = SCNetworkInterfaceSetConfiguration(interface, newConfig);
				CFRelease(newConfig);
			}

			if (description != NULL) CFRelease(description);
			if (service     != NULL) CFRelease(service);
			break;
		}

		case kSCNetworkInterfacePasswordTypeIPSecSharedSecret : {
			CFDictionaryRef		baseConfig	= NULL;
			Boolean			extended;
			SCNetworkServiceRef	service		= NULL;
			CFStringRef		shared_id;

			// get configuration
			extended = CFEqual(SCNetworkInterfaceGetInterfaceType(interface), kSCNetworkInterfaceTypePPP);
			config = SCNetworkInterfaceGetConfiguration(interface);
			if (extended) {
				baseConfig = config;
				config = SCNetworkInterfaceGetExtendedConfiguration(interface, kSCEntNetIPSec);
			}

			// get sharedSecret ID
			shared_id = copySharedSecretID(config, serviceID);

			// get "Account", "Name", "Kind"
			if (config != NULL) {
				CFStringRef	localIdentifier;
				CFStringRef	localIdentifierType;

				if (CFDictionaryGetValueIfPresent(config,
								  kSCPropNetIPSecLocalIdentifierType,
								  (const void **)&localIdentifierType)
				    && CFEqual(localIdentifierType, kSCValNetIPSecLocalIdentifierTypeKeyID)
				    && CFDictionaryGetValueIfPresent(config,
								     kSCPropNetIPSecLocalIdentifier,
								     (const void **)&localIdentifier)
				    && isA_CFString(localIdentifier)) {
					// local identifier --> keychain "Account"
					account = localIdentifier;
				}

				// PPP [user defined] "name" --> keychain "Name"
				if (!extended) {
					label = CFDictionaryGetValue(config, kSCPropUserDefinedName);
				} else {
					if (baseConfig != NULL) {
						label = CFDictionaryGetValue(baseConfig, kSCPropUserDefinedName);
					}
				}
			}

			if (label == NULL) {
				// service name --> keychain "Name"
				service = (SCNetworkServiceRef)__SCNetworkServiceCreatePrivate(NULL,
											       prefs,
											       serviceID,
											       interface);

				label = SCNetworkServiceGetName(service);
				if (label == NULL) {
					// interface name --> keychain "Name"
					label = SCNetworkInterfaceGetLocalizedDisplayName(interface);
				}
			}

			if (bundle != NULL) {
				// "IPSec Shared Secret" --> keychain "Kind"
				description = CFBundleCopyLocalizedString(bundle,
									  CFSTR("KEYCHAIN_KIND_IPSEC_SHARED_SECRET"),
									  CFSTR("IPSec Shared Secret"),
									  NULL);
			}

			// set password
			ok = _SCPreferencesSystemKeychainPasswordItemSet(prefs,
									 shared_id,
									 (label != NULL)       ? label       : CFSTR("VPN Connection"),
									 (description != NULL) ? description : CFSTR("IPSec Shared Secret"),
									 account,
									 password,
									 options);
			if (ok) {
				CFMutableDictionaryRef	newConfig	= NULL;

				if (config != NULL) {
					newConfig = CFDictionaryCreateMutableCopy(NULL, 0, config);
				} else {
					newConfig = CFDictionaryCreateMutable(NULL,
									      0,
									      &kCFTypeDictionaryKeyCallBacks,
									      &kCFTypeDictionaryValueCallBacks);
				}
				CFDictionarySetValue(newConfig,
						     kSCPropNetIPSecSharedSecret,
						     shared_id);
				CFDictionarySetValue(newConfig,
						     kSCPropNetIPSecSharedSecretEncryption,
						     kSCValNetIPSecSharedSecretEncryptionKeychain);
				if (extended) {
					ok = SCNetworkInterfaceSetExtendedConfiguration(interface,
											kSCEntNetIPSec,
											newConfig);
				} else {
					ok = SCNetworkInterfaceSetConfiguration(interface, newConfig);
				}
				CFRelease(newConfig);
			}

			if (description != NULL) CFRelease(description);
			if (service     != NULL) CFRelease(service);
			CFRelease(shared_id);
			break;
		}

		case kSCNetworkInterfacePasswordTypeEAPOL : {
			CFStringRef	account		= NULL;
			CFStringRef	unique_id	= NULL;

			// get configuration
			config = SCNetworkInterfaceGetExtendedConfiguration(interface, kSCEntNetEAPOL);

			// get 802.1X identifier
			if (config != NULL) {
				unique_id = CFDictionaryGetValue(config, kEAPClientPropUserPasswordKeychainItemID);
			}
			if (isA_CFString(unique_id)) {
				CFRetain(unique_id);
			} else {
				CFUUIDRef	uuid;

				uuid     = CFUUIDCreate(NULL);
				unique_id = CFUUIDCreateString(NULL, uuid);
				CFRelease(uuid);
			}

			// 802.1x UserName --> keychain "Account"
			if (config != NULL) {
				account = CFDictionaryGetValue(config, kEAPClientPropUserName);
			}

			// get "Name", "Kind"
			if (bundle != NULL) {
				CFStringRef	interface_name;

				// "Network Connection (%@)" --> keychain "Name"
				interface_name = SCNetworkInterfaceGetLocalizedDisplayName(interface);
				if (interface_name != NULL) {
					CFStringRef	label_fmt;

					label_fmt = CFBundleCopyLocalizedString(bundle,
										CFSTR("KEYCHAIN_DESCRIPTION_EAPOL_INTERFACE"),
										CFSTR("Network Connection (%@)"),
										NULL);
					label = CFStringCreateWithFormat(NULL, NULL, label_fmt, interface_name);
					CFRelease(label_fmt);
				} else {
					label = CFBundleCopyLocalizedString(bundle,
									    CFSTR("KEYCHAIN_DESCRIPTION_EAPOL"),
									    CFSTR("Network Connection"),
									    NULL);
				}

				// "802.1X Password" --> keychain "Kind"
				description = CFBundleCopyLocalizedString(bundle,
									  CFSTR("KEYCHAIN_KIND_EAPOL"),
									  CFSTR("802.1X Password"),
									  NULL);
			}

			// set password
			ok = _SCPreferencesSystemKeychainPasswordItemSet(prefs,
									 unique_id,
									 (label != NULL)       ? label       : CFSTR("Network Connection"),
									 (description != NULL) ? description : CFSTR("802.1X Password"),
									 account,
									 password,
									 options);
			if (ok) {
				CFMutableDictionaryRef	newConfig	= NULL;

				if (config != NULL) {
					newConfig = CFDictionaryCreateMutableCopy(NULL, 0, config);
				} else {
					newConfig = CFDictionaryCreateMutable(NULL,
									      0,
									      &kCFTypeDictionaryKeyCallBacks,
									      &kCFTypeDictionaryValueCallBacks);
				}
				CFDictionarySetValue(newConfig,
						     kEAPClientPropUserPasswordKeychainItemID,
						     unique_id);
				ok = SCNetworkInterfaceSetExtendedConfiguration(interface,
										kSCEntNetEAPOL,
										newConfig);
				CFRelease(newConfig);
			}

			CFRelease(unique_id);
			if (label       != NULL) CFRelease(label);
			if (description != NULL) CFRelease(description);
			break;
		}

		case kSCNetworkInterfacePasswordTypeIPSecXAuth : {
			SCNetworkServiceRef	service	= NULL;
			CFStringRef		xauth_id;

			// get configuration
			config = SCNetworkInterfaceGetConfiguration(interface);

			// get XAuth ID
			xauth_id = copyXAuthID(config, serviceID);

			// get "Account", "Name", "Kind"
			if (config != NULL) {
				// auth name --> keychain "Account"
				account = CFDictionaryGetValue(config, kSCPropNetIPSecXAuthName);

				// IPSec [user defined] "name" --> keychain "Name"
				label = CFDictionaryGetValue(config, kSCPropUserDefinedName);
			}

			if (label == NULL) {
				// service name --> keychain "Name"
				service = (SCNetworkServiceRef)__SCNetworkServiceCreatePrivate(NULL,
											       prefs,
											       serviceID,
											       interface);

				label = SCNetworkServiceGetName(service);
				if (label == NULL) {
					// interface name --> keychain "Name"
					label = SCNetworkInterfaceGetLocalizedDisplayName(interface);
				}
			}

			if (bundle != NULL) {
				// "IPSec XAuth Password" --> keychain "Kind"
				description = CFBundleCopyLocalizedString(bundle,
									  CFSTR("KEYCHAIN_KIND_IPSEC_XAUTH_PASSWORD"),
									  CFSTR("IPSec XAuth Password"),
									  NULL);
			}

			// store password
			ok = _SCPreferencesSystemKeychainPasswordItemSet(prefs,
									 xauth_id,
									 (label != NULL)       ? label       : CFSTR("VPN Connection"),
									 (description != NULL) ? description : CFSTR("IPSec XAuth Password"),
									 account,
									 password,
									 options);
			if (ok) {
				CFMutableDictionaryRef	newConfig;

				if (config != NULL) {
					newConfig = CFDictionaryCreateMutableCopy(NULL, 0, config);
				} else {
					newConfig = CFDictionaryCreateMutable(NULL,
									      0,
									      &kCFTypeDictionaryKeyCallBacks,
									      &kCFTypeDictionaryValueCallBacks);
				}
				CFDictionarySetValue(newConfig,
						     kSCPropNetIPSecXAuthPassword,
						     xauth_id);
				CFDictionarySetValue(newConfig,
						     kSCPropNetIPSecXAuthPasswordEncryption,
						     kSCValNetIPSecXAuthPasswordEncryptionKeychain);
				ok = SCNetworkInterfaceSetConfiguration(interface, newConfig);
				CFRelease(newConfig);
			}

			CFRelease(xauth_id);
			if (description != NULL) CFRelease(description);
			if (service     != NULL) CFRelease(service);
			break;
		}

		default :
			_SCErrorSet(kSCStatusInvalidArgument);
			break;
	}

	return ok;
}


#pragma mark -
#pragma mark SCNetworkInterface [InterfaceNamer] SPIs


SCNetworkInterfaceRef
_SCNetworkInterfaceCreateWithIONetworkInterfaceObject(io_object_t if_obj)
{
	SCNetworkInterfaceRef	interface	= NULL;

	/* initialize runtime */
	pthread_once(&initialized, __SCNetworkInterfaceInitialize);

	if (IOObjectConformsTo(if_obj, kIONetworkInterfaceClass)) {
		interface = createInterface(if_obj, processNetworkInterface);
	} else if (IOObjectConformsTo(if_obj, kIOSerialBSDServiceValue)) {
		interface = createInterface(if_obj, processSerialInterface);
	}

	return interface;
}


CFStringRef
_SCNetworkInterfaceGetConfigurationAction(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	return interfacePrivate->configurationAction;
}


CFDataRef
_SCNetworkInterfaceGetHardwareAddress(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	return interfacePrivate->address;
}


CFNumberRef
_SCNetworkInterfaceGetIOInterfaceType(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	return interfacePrivate->type;
}


CFNumberRef
_SCNetworkInterfaceGetIOInterfaceUnit(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	return interfacePrivate->unit;
}


CFStringRef
_SCNetworkInterfaceGetIOPath(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	return interfacePrivate->path;
}


Boolean
_SCNetworkInterfaceIsBuiltin(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	return interfacePrivate->builtin;
}


#pragma mark -
#pragma mark SCNetworkInterface SPIs


CFStringRef
_SCNetworkInterfaceCopySlashDevPath(SCNetworkInterfaceRef interface)
{
	io_registry_entry_t		device;
	io_iterator_t			device_iterator		= MACH_PORT_NULL;
	CFStringRef			device_path		= NULL;
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;
	kern_return_t			kr;
	CFStringRef			match_keys[2];
	CFTypeRef			match_vals[2];
	CFDictionaryRef			match_dict;
	CFDictionaryRef			matching;

	if (interfacePrivate->entity_device == NULL) {
		return NULL;
	}

	if (interfacePrivate->entity_device_unique == NULL) {
		goto done;
	}

	match_keys[0] = CFSTR(kIOTTYBaseNameKey);
	match_vals[0] = interfacePrivate->entity_device;
	match_dict = CFDictionaryCreate(NULL,
					(const void **)match_keys,
					(const void **)match_vals,
					1,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);

	match_keys[0] = CFSTR(kIOProviderClassKey);
	match_vals[0] = CFSTR(kIOSerialBSDServiceValue);
	match_keys[1] = CFSTR(kIOPropertyMatchKey);
	match_vals[1] = match_dict;
	matching = CFDictionaryCreate(NULL,
				      (const void **)match_keys,
				      (const void **)match_vals,
				      sizeof(match_keys)/sizeof(match_keys[0]),
				      &kCFTypeDictionaryKeyCallBacks,
				      &kCFTypeDictionaryValueCallBacks);
	CFRelease(match_dict);

	// note: this "matching" dictionary will be consumed by the call to IOServiceGetMatchingServices
	kr = IOServiceGetMatchingServices(masterPort, matching, &device_iterator);
	if (kr != kIOReturnSuccess) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("IOServiceGetMatchingServices() failed, kr = 0x%x"), kr);
		goto done;
	}

	while ((device_path == NULL) &&
	       ((device = IOIteratorNext(device_iterator)) != MACH_PORT_NULL)) {
		CFDictionaryRef	overrides;

		overrides = IORegistryEntrySearchCFProperty(device,
							    kIOServicePlane,
							    CFSTR("DeviceModemOverrides"),
							    NULL,
							    kIORegistryIterateRecursively | kIORegistryIterateParents);
		if (overrides != NULL) {
			if (isA_CFDictionary(overrides)) {
				CFStringRef	matchIdentifier;

				matchIdentifier = CFDictionaryGetValue(overrides, CFSTR("UniqueIdentifier"));
				if (isA_CFString(matchIdentifier) &&
				    CFEqual(interfacePrivate->entity_device_unique, matchIdentifier)) {
					device_path = IORegistryEntryCreateCFProperty(device,
										      CFSTR(kIOTTYDeviceKey),
										      NULL,
										      0);
				}
			}
			CFRelease(overrides);
		}
		IOObjectRelease(device);
	}

	IOObjectRelease(device_iterator);

    done :

	if (device_path == NULL) {
		// if we haven't found an exact match to our UniqueIdentifier
		// so we simply return the base name.
		device_path = SCNetworkInterfaceGetBSDName(interface);
		if (device_path != NULL) {
			CFRetain(device_path);
		}
	}

	return device_path;
}


Boolean
_SCNetworkInterfaceIsBluetoothPAN(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	return (interfacePrivate->sort_order == kSortBluetoothPAN);
}


Boolean
_SCNetworkInterfaceIsModemV92(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	return interfacePrivate->modemIsV92;
}


Boolean
_SCNetworkInterfaceIsTethered(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	return (interfacePrivate->sort_order == kSortTethered);
}


#pragma mark -
#pragma mark SCNetworkInterface [internal] SPIs


__private_extern__
SCNetworkInterfacePrivateRef
__SCNetworkInterfaceCreateCopy(CFAllocatorRef		allocator,
			       SCNetworkInterfaceRef	interface,
			       SCPreferencesRef		prefs,
			       CFStringRef		serviceID)
{
	SCNetworkInterfacePrivateRef		oldPrivate	= (SCNetworkInterfacePrivateRef)interface;
	SCNetworkInterfacePrivateRef		newPrivate;

	/* initialize runtime (and kSCNetworkInterfaceIPv4) */
	pthread_once(&initialized, __SCNetworkInterfaceInitialize);

	if (interface == kSCNetworkInterfaceIPv4) {
		return (SCNetworkInterfacePrivateRef)CFRetain(interface);
	}

	newPrivate = __SCNetworkInterfaceCreatePrivate(NULL, NULL, prefs, serviceID, NULL);
	newPrivate->interface_type		= oldPrivate->interface_type;
	if (oldPrivate->interface != NULL) {
		newPrivate->interface		= (SCNetworkInterfaceRef)__SCNetworkInterfaceCreateCopy(NULL,			// allocator
													oldPrivate->interface,	// interface
													prefs,			// [new] prefs
													serviceID);		// [new] serviceID
	}
	if (oldPrivate->name != NULL) {
		newPrivate->name		= CFRetain(oldPrivate->name);
	}
	if (oldPrivate->localized_name != NULL) {
		newPrivate->localized_name	= CFRetain(oldPrivate->localized_name);
	}
	newPrivate->localized_key		= oldPrivate->localized_key;
	if (oldPrivate->localized_arg1 != NULL) {
		newPrivate->localized_arg1	= CFRetain(oldPrivate->localized_arg1);
	}
	if (oldPrivate->localized_arg2 != NULL) {
		newPrivate->localized_arg2	= CFRetain(oldPrivate->localized_arg2);
	}
	if (oldPrivate->unsaved != NULL) {
		newPrivate->unsaved		= CFDictionaryCreateMutableCopy(NULL, 0, oldPrivate->unsaved);
	}
	if (oldPrivate->entity_device != NULL) {
		newPrivate->entity_device	= CFRetain(oldPrivate->entity_device);
	}
	if (oldPrivate->entity_device_unique != NULL) {
		newPrivate->entity_device_unique = CFRetain(oldPrivate->entity_device_unique);
	}
	newPrivate->entity_type			= oldPrivate->entity_type;
	newPrivate->entity_subtype		= oldPrivate->entity_subtype;
	if (oldPrivate->supported_interface_types != NULL) {
		newPrivate->supported_interface_types = CFArrayCreateMutableCopy(NULL, 0, oldPrivate->supported_interface_types);
	}
	if (oldPrivate->supported_protocol_types != NULL) {
		newPrivate->supported_protocol_types = CFArrayCreateMutableCopy(NULL, 0, oldPrivate->supported_protocol_types);
	}
	if (oldPrivate->address != NULL) {
		newPrivate->address		= CFRetain(oldPrivate->address);
	}
	newPrivate->builtin			= oldPrivate->builtin;
	if (oldPrivate->configurationAction != NULL) {
		newPrivate->configurationAction	= CFRetain(oldPrivate->configurationAction);
	}
	if (oldPrivate->location != NULL) {
		newPrivate->location		= CFRetain(oldPrivate->location);
	}
	if (oldPrivate->path != NULL) {
		newPrivate->path		= CFRetain(oldPrivate->path);
	}
	if (oldPrivate->overrides != NULL) {
		newPrivate->overrides		= CFDictionaryCreateMutableCopy(NULL, 0, oldPrivate->overrides);
	}
	newPrivate->modemIsV92			= oldPrivate->modemIsV92;
	if (oldPrivate->type != NULL) {
		newPrivate->type		= CFRetain(oldPrivate->type);
	}
	if (oldPrivate->unit != NULL) {
		newPrivate->unit		= CFRetain(oldPrivate->unit);
	}
	newPrivate->sort_order			= oldPrivate->sort_order;
#if	!TARGET_OS_IPHONE
	newPrivate->supportsBond		= oldPrivate->supportsBond;
	if (oldPrivate->bond.interfaces != NULL) {
		newPrivate->bond.interfaces	= CFRetain(oldPrivate->bond.interfaces);
	}
	if (oldPrivate->bond.mode != NULL) {
		newPrivate->bond.mode		= CFRetain(oldPrivate->bond.mode);
	}
	if (oldPrivate->bond.options != NULL) {
		newPrivate->bond.options	= CFRetain(oldPrivate->bond.options);
	}
	newPrivate->supportsVLAN		= oldPrivate->supportsVLAN;
	if (oldPrivate->vlan.interface != NULL) {
		newPrivate->vlan.interface	= CFRetain(oldPrivate->vlan.interface);
	}
	if (oldPrivate->vlan.tag != NULL) {
		newPrivate->vlan.tag		= CFRetain(oldPrivate->vlan.tag);
	}
	if (oldPrivate->vlan.options != NULL) {
		newPrivate->vlan.options	= CFRetain(oldPrivate->vlan.options);
	}
#endif	// !TARGET_OS_IPHONE

	return newPrivate;
}


__private_extern__
CFArrayRef
__SCNetworkInterfaceCopyDeepConfiguration(SCNetworkSetRef set, SCNetworkInterfaceRef interface)
{
	CFMutableArrayRef       configs;

	configs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	while (interface != NULL) {
		CFStringRef		defaultType;
		CFMutableDictionaryRef	interfaceConfiguration;

		interfaceConfiguration = CFDictionaryCreateMutable(NULL,
								   0,
								   &kCFTypeDictionaryKeyCallBacks,
								   &kCFTypeDictionaryValueCallBacks);

		defaultType = __SCNetworkInterfaceGetDefaultConfigurationType(interface);
		if (defaultType != NULL) {
			CFDictionaryRef	config;
			CFArrayRef	extendedTypes;

			if (set == NULL) {
				config = __SCNetworkInterfaceGetConfiguration(interface, defaultType);
			} else {
				config = __SCNetworkInterfaceGetDefaultConfiguration(set, interface);
			}
			if (config == NULL) {
				config = (CFDictionaryRef)kCFNull;
			}
			CFDictionarySetValue(interfaceConfiguration, defaultType, config);

			extendedTypes = extendedConfigurationTypes(interface);
			if (extendedTypes != NULL) {
				CFIndex	i;
				CFIndex	n;

				n = CFArrayGetCount(extendedTypes);
				for (i = 0; i < n; i++) {
					CFStringRef	extendedType;

					extendedType = CFArrayGetValueAtIndex(extendedTypes, i);
					config = __SCNetworkInterfaceGetConfiguration(interface, extendedType);
					if (config == NULL) {
						config = (CFDictionaryRef)kCFNull;
					}
					CFDictionarySetValue(interfaceConfiguration, extendedType, config);
				}

				CFRelease(extendedTypes);
			}
		}

		CFArrayAppendValue(configs, interfaceConfiguration);
		CFRelease(interfaceConfiguration);

		interface = SCNetworkInterfaceGetInterface(interface);
	}

	return configs;
}


__private_extern__
void
__SCNetworkInterfaceSetDeepConfiguration(SCNetworkSetRef set, SCNetworkInterfaceRef interface, CFArrayRef configs)
{
	CFIndex	i;

	for (i = 0; interface != NULL; i++) {
		CFStringRef	defaultType;
		CFDictionaryRef interfaceConfiguration;

		interfaceConfiguration = (configs != NULL) ? CFArrayGetValueAtIndex(configs, i) : NULL;

		defaultType = __SCNetworkInterfaceGetDefaultConfigurationType(interface);
		if (defaultType != NULL) {
			CFDictionaryRef	config;
			CFArrayRef	extendedTypes;

			config = (interfaceConfiguration != NULL) ? CFDictionaryGetValue(interfaceConfiguration, defaultType)
								  : NULL;
			if (config == (CFDictionaryRef)kCFNull) {
				config = NULL;
			}
			if (set == NULL) {
				// if service is not associated with the set
				if (!__SCNetworkInterfaceSetConfiguration(interface, defaultType, config, TRUE)) {
					SCLog(TRUE, LOG_DEBUG,
						  CFSTR("__SCNetworkInterfaceSetDeepConfiguration __SCNetworkInterfaceSetConfiguration() failed, interface=%@, type=%@"),
						  interface,
						  defaultType);
				}
			} else {
				// apply default configuration to this set
				if (!__SCNetworkInterfaceSetDefaultConfiguration(set, interface, defaultType, config, TRUE)) {
					SCLog(TRUE, LOG_DEBUG,
						  CFSTR("__SCNetworkInterfaceSetDeepConfiguration __SCNetworkInterfaceSetDefaultConfiguration() failed, interface=%@, type=%@"),
						  interface,
						  defaultType);
				}
			}

			extendedTypes = extendedConfigurationTypes(interface);
			if (extendedTypes != NULL) {
				CFIndex	j;
				CFIndex	n;

				n = CFArrayGetCount(extendedTypes);
				for (j = 0; j < n; j++) {
					CFStringRef	extendedType;

					extendedType = CFArrayGetValueAtIndex(extendedTypes, j);
					config = (interfaceConfiguration != NULL) ? CFDictionaryGetValue(interfaceConfiguration, extendedType)
					: NULL;
					if (config == (CFDictionaryRef)kCFNull) {
						config = NULL;
					}
					if (!__SCNetworkInterfaceSetConfiguration(interface, extendedType, config, TRUE)) {
						SCLog(TRUE, LOG_DEBUG,
						      CFSTR("__SCNetworkInterfaceSetDeepConfiguration __SCNetworkInterfaceSetConfiguration() failed, interface=%@, type=%@"),
						      interface,
						      defaultType);
					}
				}

				CFRelease(extendedTypes);
			}
		}

		interface = SCNetworkInterfaceGetInterface(interface);
	}

	return;
}
