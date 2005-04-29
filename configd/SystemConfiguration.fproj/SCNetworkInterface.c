/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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
 *	  Elizaabeth Douglas	<elizabeth@apple.com>
 *	  Quinn			<eskimo1@apple.com>
 */


#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCNetworkConfigurationInternal.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/BondConfiguration.h>
#include <SystemConfiguration/VLANConfiguration.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFBundle.h>
#include <IOKit/IOBSD.h>
#include <IOKit/network/IONetworkController.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOEthernetInterface.h>	// for kIOEthernetInterfaceClass
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include "dy_framework.h"

#ifndef	kIODeviceSupportsHoldKey
#define	kIODeviceSupportsHoldKey	"V92Modem"
#endif

#include "SCNetworkConfiguration.h"
#include "SCNetworkConfigurationInternal.h"

#include <mach/mach.h>
#include <net/if.h>
#include <net/if_types.h>
#include <pthread.h>


static CFStringRef	__SCNetworkInterfaceCopyDescription	(CFTypeRef cf);
static void		__SCNetworkInterfaceDeallocate		(CFTypeRef cf);
static Boolean		__SCNetworkInterfaceEqual		(CFTypeRef cf1, CFTypeRef cf2);


enum {
	kSortInternalModem,
	kSortUSBModem,
	kSortModem,
	kSortBluetooth,
	kSortIrDA,
	kSortSerialPort,
	kSortEthernet,
	kSortFireWire,
	kSortAirPort,
	kSortOtherWireless,
	kSortBond,
	kSortVLAN,
	kSortUnknown
};


const CFStringRef kSCNetworkInterfaceType6to4		= CFSTR("6to4");
const CFStringRef kSCNetworkInterfaceTypeBluetooth	= CFSTR("Bluetooth");
const CFStringRef kSCNetworkInterfaceTypeBond		= CFSTR("Bond");
const CFStringRef kSCNetworkInterfaceTypeEthernet	= CFSTR("Ethernet");
const CFStringRef kSCNetworkInterfaceTypeFireWire	= CFSTR("FireWire");
const CFStringRef kSCNetworkInterfaceTypeIEEE80211	= CFSTR("IEEE80211");	// IEEE 802.11, AirPort
const CFStringRef kSCNetworkInterfaceTypeIrDA		= CFSTR("IrDA");
const CFStringRef kSCNetworkInterfaceTypeL2TP		= CFSTR("L2TP");
const CFStringRef kSCNetworkInterfaceTypeModem		= CFSTR("Modem");
const CFStringRef kSCNetworkInterfaceTypePPP		= CFSTR("PPP");
const CFStringRef kSCNetworkInterfaceTypePPTP		= CFSTR("PPTP");
const CFStringRef kSCNetworkInterfaceTypeSerial		= CFSTR("Serial");
const CFStringRef kSCNetworkInterfaceTypeVLAN		= CFSTR("VLAN");

const CFStringRef kSCNetworkInterfaceTypeIPv4		= CFSTR("IPv4");

static SCNetworkInterfacePrivate __kSCNetworkInterfaceIPv4      = {
	INIT_CFRUNTIME_BASE(NULL, 0, 0x0080),   // cfBase
	NULL,					// interface type
	NULL,					// localized name
	NULL,					// localization key
	NULL,					// localization arg1
	NULL,					// localization arg2
	NULL,					// [layered] interface
	NULL,					// service
	NULL,					// unsaved
	NULL,					// entity_device
	NULL,					// entity_hardware
	NULL,					// entity_type
	NULL,					// entity_subtype
	NULL,					// supported_interface_types
	NULL,					// supported_protocol_types
	NULL,					// address
	FALSE,					// builtin
	NULL,					// location
	NULL,					// path
	FALSE,					// supportsDeviceOnHold
	FALSE,					// supportsBond
	FALSE,					// supportsVLAN
	kSortUnknown				// sort_order
};

const SCNetworkInterfaceRef kSCNetworkInterfaceIPv4     = (SCNetworkInterfaceRef)&__kSCNetworkInterfaceIPv4;

#define doNone		0

#define do6to4		1<<0
#define doL2TP		1<<1
#define doPPP		1<<2
#define doPPTP		1<<3

#define doAppleTalk     1<<0
#define doDNS		1<<1
#define doIPv4		1<<2
#define doIPv6		1<<3
#define doProxies       1<<4

static const struct {
	const CFStringRef       *interface_type;
	Boolean			per_interface_config;
	uint32_t		supported_interfaces;
	const CFStringRef       *ppp_subtype;
	uint32_t		supported_protocols;
} configurations[] = {
	// interface type			  if config?    interface types         PPP sub-type				interface protocols
	// =====================================  ==========    ======================= ======================================= =========================================
	{ &kSCNetworkInterfaceType6to4		, FALSE,	doNone,			NULL,					doIPv6						},
	{ &kSCNetworkInterfaceTypeBluetooth     , FALSE,	doPPP,			&kSCValNetInterfaceSubTypePPPSerial,    doNone						},
	{ &kSCNetworkInterfaceTypeBond		, TRUE,		doNone,			NULL,					doAppleTalk|doDNS|doIPv4|doIPv6|doProxies	},
	{ &kSCNetworkInterfaceTypeEthernet      , TRUE,		doPPP,			&kSCValNetInterfaceSubTypePPPoE,	doAppleTalk|doDNS|doIPv4|doIPv6|doProxies	},
	{ &kSCNetworkInterfaceTypeFireWire      , TRUE,		doNone,			NULL,					doDNS|doIPv4|doIPv6|doProxies			},
	{ &kSCNetworkInterfaceTypeIEEE80211     , TRUE,		doPPP,			&kSCValNetInterfaceSubTypePPPoE,	doAppleTalk|doDNS|doIPv4|doIPv6|doProxies	},
	{ &kSCNetworkInterfaceTypeIrDA		, FALSE,	doPPP,			&kSCValNetInterfaceSubTypePPPSerial,    doNone						},
	{ &kSCNetworkInterfaceTypeL2TP		, FALSE,	doPPP,			&kSCValNetInterfaceSubTypeL2TP,		doNone						},
	{ &kSCNetworkInterfaceTypeModem		, FALSE,	doPPP,			&kSCValNetInterfaceSubTypePPPSerial,    doNone						},
	{ &kSCNetworkInterfaceTypePPP		, FALSE,	doNone,			NULL,					doDNS|doIPv4|doIPv6|doProxies			},
	{ &kSCNetworkInterfaceTypePPTP		, FALSE,	doPPP,			&kSCValNetInterfaceSubTypePPTP,		doNone						},
	{ &kSCNetworkInterfaceTypeSerial	, FALSE,	doPPP,			&kSCValNetInterfaceSubTypePPPSerial,    doNone						},
	{ &kSCNetworkInterfaceTypeVLAN		, TRUE,		doNone,			NULL,					doAppleTalk|doDNS|doIPv4|doIPv6|doProxies	},
	// =====================================  ==========    ======================= ======================================= =========================================
	{ &kSCNetworkInterfaceTypeIPv4		, FALSE,	do6to4|doPPTP|doL2TP,   NULL,					doNone						}
};


#define	SYSTEMCONFIGURATION_BUNDLE_ID	CFSTR("com.apple.SystemConfiguration")
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
	NULL,					// hash
	NULL,					// copyFormattingDesc
	__SCNetworkInterfaceCopyDescription	// copyDebugDesc
};


static pthread_once_t		initialized	= PTHREAD_ONCE_INIT;


static __inline__ CFTypeRef
isA_SCNetworkInterface(CFTypeRef obj)
{
	return (isA_CFType(obj, SCNetworkInterfaceGetTypeID()));
}


static CFStringRef
__SCNetworkInterfaceCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef			allocator		= CFGetAllocator(cf);
	CFMutableStringRef		result;
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)cf;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<SCNetworkInterface %p [%p]> { "), cf, allocator);
	CFStringAppendFormat(result, NULL, CFSTR("type = %@"), interfacePrivate->interface_type);
	CFStringAppendFormat(result, NULL, CFSTR(", entity = %@ / %@ / %@"),
			     interfacePrivate->entity_device,
			     interfacePrivate->entity_hardware,
			     interfacePrivate->entity_type);
	if (interfacePrivate->entity_subtype != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(" / %@"), interfacePrivate->entity_subtype);
	}
	if (interfacePrivate->localized_name != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", name = %@"), interfacePrivate->localized_name);
	} else {
		if (interfacePrivate->localized_key != NULL) {
			CFStringAppendFormat(result, NULL, CFSTR(", name = \"%@\""), interfacePrivate->localized_key);
			if (interfacePrivate->localized_arg1 != NULL) {
				CFStringAppendFormat(result, NULL, CFSTR("+\"%@\""), interfacePrivate->localized_arg1);
			}
			if (interfacePrivate->localized_arg2 != NULL) {
				CFStringAppendFormat(result, NULL, CFSTR("+\"%@\""), interfacePrivate->localized_arg2);
			}
		}
	}
	if (interfacePrivate->address != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", address = %@"), interfacePrivate->address);
	}
	CFStringAppendFormat(result, NULL, CFSTR(", builtin = %s"), interfacePrivate->builtin ? "TRUE" : "FALSE");
	if (interfacePrivate->location != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", location = %@"), interfacePrivate->location);
	}
	CFStringAppendFormat(result, NULL, CFSTR(", path = %@"), interfacePrivate->path);
	CFStringAppendFormat(result, NULL, CFSTR(", order = %d"), interfacePrivate->sort_order);

	if (interfacePrivate->service != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", service=%@"), interfacePrivate->service);
	}

	if (interfacePrivate->interface != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", interface=%@"), interfacePrivate->interface);
	}

	if (interfacePrivate->unsaved != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", unsaved=%@"), interfacePrivate->unsaved);
	}
	CFStringAppendFormat(result, NULL, CFSTR(" }"));

	return result;
}


static void
__SCNetworkInterfaceDeallocate(CFTypeRef cf)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)cf;

	/* release resources */

	if (interfacePrivate->interface != NULL)
		CFRelease(interfacePrivate->interface);

	if (interfacePrivate->localized_name != NULL)
		CFRelease(interfacePrivate->localized_name);

	if (interfacePrivate->localized_arg1 != NULL)
		CFRelease(interfacePrivate->localized_arg1);

	if (interfacePrivate->localized_arg2 != NULL)
		CFRelease(interfacePrivate->localized_arg2);

	if (interfacePrivate->unsaved != NULL)
		CFRelease(interfacePrivate->unsaved);

	if (interfacePrivate->entity_device != NULL)
		CFRelease(interfacePrivate->entity_device);

	if (interfacePrivate->supported_interface_types != NULL)
		CFRelease(interfacePrivate->supported_interface_types);

	if (interfacePrivate->supported_protocol_types != NULL)
		CFRelease(interfacePrivate->supported_protocol_types);

	if (interfacePrivate->address != NULL)
		CFRelease(interfacePrivate->address);

	if (interfacePrivate->location != NULL)
		CFRelease(interfacePrivate->location);

	if (interfacePrivate->path != NULL)
		CFRelease(interfacePrivate->path);

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

	if (if1->entity_device != if2->entity_device) {
		if ((if1->entity_device != NULL) && (if2->entity_device != NULL)) {
			if (!CFEqual(if1->entity_device, if2->entity_device)) {
				return FALSE;	// if not the same device
			}
		} else {
			return FALSE;	// if only one interface has a device
		}
	}

	return TRUE;
}


static void
__SCNetworkInterfaceInitialize(void)
{
	// register w/CF
	__kSCNetworkInterfaceTypeID = _CFRuntimeRegisterClass(&__SCNetworkInterfaceClass);

	// initialize __kSCNetworkInterfaceIPv4
	_CFRuntimeSetInstanceTypeID(&__kSCNetworkInterfaceIPv4, __kSCNetworkInterfaceTypeID);
	__kSCNetworkInterfaceIPv4.interface_type = kSCNetworkInterfaceTypeIPv4;
	__kSCNetworkInterfaceIPv4.localized_key  = CFSTR("ipv4");

	// get CFBundleRef for SystemConfiguration.framework
	bundle = CFBundleGetBundleWithIdentifier(SYSTEMCONFIGURATION_BUNDLE_ID);
	if (bundle == NULL) {
		// try a bit harder
		CFURLRef	url;

		url = CFURLCreateWithFileSystemPath(NULL,
						    CFSTR("/System/Library/Frameworks/SystemConfiguration.framework"),
						    kCFURLPOSIXPathStyle,
						    TRUE);
		bundle = CFBundleCreate(NULL, url);
		CFRelease(url);
	}

	return;
}


__private_extern__ SCNetworkInterfacePrivateRef
__SCNetworkInterfaceCreatePrivate(CFAllocatorRef	allocator,
				  SCNetworkInterfaceRef	interface,
				  SCNetworkServiceRef	service,
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
	interfacePrivate->localized_name		= NULL;
	interfacePrivate->localized_key			= NULL;
	interfacePrivate->localized_arg1		= NULL;
	interfacePrivate->localized_arg2		= NULL;
	interfacePrivate->interface			= (interface != NULL) ? CFRetain(interface) : NULL;
	interfacePrivate->service			= service;
	interfacePrivate->unsaved			= NULL;
	interfacePrivate->entity_device			= NULL;
	interfacePrivate->entity_hardware		= NULL;
	interfacePrivate->entity_type			= NULL;
	interfacePrivate->entity_subtype		= NULL;
	interfacePrivate->supported_interface_types     = NULL;
	interfacePrivate->supported_protocol_types      = NULL;
	interfacePrivate->address			= NULL;
	interfacePrivate->builtin			= FALSE;
	interfacePrivate->path				= (path != NULL) ? CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8)
									 : NULL;
	interfacePrivate->location			= NULL;
	interfacePrivate->supportsDeviceOnHold		= FALSE;
	interfacePrivate->supportsBond			= FALSE;
	interfacePrivate->supportsVLAN			= FALSE;
	interfacePrivate->sort_order			= kSortUnknown;

	return interfacePrivate;
}


/* ---------- ordering ---------- */


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


static CFComparisonResult
compare_interfaces(const void *val1, const void *val2, void *context)
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

	/* ... and lastly, sort by BSD interface name */
	if ((dev1->entity_device != NULL) && (dev2->entity_device != NULL)) {
		res = CFStringCompare(dev1->entity_device, dev2->entity_device, 0);
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

	CFArraySortValues(all_interfaces, CFRangeMake(0, n), compare_interfaces, NULL);
	return;
}


/* ---------- interface details ---------- */


static CFStringRef
IOCopyCFStringValue(CFTypeRef ioVal)
{
	if (isA_CFString(ioVal)) {
		return CFStringCreateCopy(NULL, ioVal);
	}

	if (isA_CFData(ioVal)) {
		return CFStringCreateWithCString(NULL, CFDataGetBytePtr(ioVal), kCFStringEncodingUTF8);
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
	CFStringRef	str		= ioVal;

	if (!isA_CFString(ioVal)) {
		if (isA_CFData(ioVal)) {
			str = CFStringCreateWithCStringNoCopy(NULL,
							      (const char *)CFDataGetBytePtr(ioVal),
							      kCFStringEncodingUTF8,
							      kCFAllocatorNull);
		} else {
			return FALSE;
		}
	}

	if ((str != NULL) &&
	    (CFStringGetLength(str) >= prefixLen) &&
	    (CFStringCompareWithOptions(str,
					prefix,
					CFRangeMake(0, prefixLen),
					kCFCompareCaseInsensitive) == kCFCompareEqualTo)) {
		match = TRUE;
	}

	if (str != ioVal)	CFRelease(str);
	return match;
}


static CFStringRef
copyMACAddress(CFDictionaryRef controller_dict)
{
	CFStringRef	address	= NULL;
	uint8_t		*bp;
	char		*cp;
	CFDataRef	data;
	CFIndex		n;
	char		mac[sizeof("xx:xx:xx:xx:xx:xx:xx:xx")];
	char		*mac_p	= mac;

	data = CFDictionaryGetValue(controller_dict, CFSTR(kIOMACAddress));
	if (data == NULL) {
		return NULL;
	}

	bp = (uint8_t *)CFDataGetBytePtr(data);
	n  = CFDataGetLength(data) * 3;

	if (n > sizeof(mac)) {
		mac_p = CFAllocatorAllocate(NULL, 0, n);
	}

	for (cp = mac_p; n > 0; n -= 3) {
		cp += snprintf(cp, n, "%2.2x:", *bp++);
	}

	address = CFStringCreateWithCString(NULL, mac_p, kCFStringEncodingUTF8);
	if (mac_p != mac)	CFAllocatorDeallocate(NULL, mac_p);
	return address;
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
	io_registry_entry_t	slot	= interface;

	if (pci_slot_name != NULL) *pci_slot_name = NULL;

	while (slot != MACH_PORT_NULL) {
		io_registry_entry_t	parent;
		CFTypeRef		slot_name;

		slot_name = IORegistryEntryCreateCFProperty(slot, CFSTR("AAPL,slot-name"), NULL, 0);
		if (slot_name != NULL) {
			Boolean		found;

			found = IOStringValueHasPrefix(slot_name, CFSTR("slot"));
			if (found) {
				CFIndex			i;
				CFMutableStringRef	name;

				// if we found a slot #
				name = CFStringCreateMutable(NULL, 0);
				if (isA_CFString(slot_name)) {
					if (pci_slot_name != NULL) *pci_slot_name = CFStringCreateCopy(NULL, slot_name);
					CFStringAppend(name, slot_name);
				} else if (isA_CFData(slot_name)) {
					if (pci_slot_name != NULL) *pci_slot_name = CFDataCreateCopy(NULL, slot_name);
					CFStringAppendCString(name, CFDataGetBytePtr(slot_name), kCFStringEncodingUTF8);
				}

				(void) CFStringFindAndReplace(name,
							      CFSTR("slot-"),
							      CFSTR(""),
							      CFRangeMake(0, 5),
							      kCFCompareCaseInsensitive|kCFCompareAnchored);
				for (i = 0; i < sizeof(slot_mappings)/sizeof(slot_mappings[0]); i++) {
					if (CFStringCompareWithOptions(name,
								       slot_mappings[i].name,
								       CFRangeMake(0, CFStringGetLength(slot_mappings[i].name)),
								       kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
						CFRelease(name);
						name = (CFMutableStringRef)CFRetain(slot_mappings[i].slot);
						break;
					}
				}

				CFRelease(slot_name);
				if (slot != interface) IOObjectRelease(slot);
				return name;
			}

			CFRelease(slot_name);
		}

		kr = IORegistryEntryGetParentEntry(slot, kIOServicePlane, &parent);
		if (slot != interface) IOObjectRelease(slot);
		switch (kr) {
			case kIOReturnSuccess :
				slot = parent;
				break;
			case kIOReturnNoDevice :
				// if we have hit the root node without finding a slot #
				goto done;
			default :
				SCLog(TRUE, LOG_INFO, CFSTR("pci_slot IORegistryEntryGetParentEntry() failed, kr = 0x%x"), kr);
				goto done;
		}
	}

    done :

	return NULL;
}


static CFComparisonResult
compare_bsdNames(const void *val1, const void *val2, void *context)
{
	CFStringRef	bsd1	= (CFStringRef)val1;
	CFStringRef	bsd2	= (CFStringRef)val2;

	return CFStringCompare(bsd1, bsd2, 0);
}


static CFStringRef
pci_port(mach_port_t masterPort, CFTypeRef slot_name, CFStringRef bsdName)
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
		SCPrint(TRUE, stderr, CFSTR("IOServiceGetMatchingServices() failed, kr = 0x%x\n"), kr);
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
			SCPrint(TRUE, stderr, CFSTR("IORegistryEntryCreateIterator() failed, kr = 0x%x\n"), kr);
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
pci_slot_info(mach_port_t masterPort, io_registry_entry_t interface, CFStringRef *slot_name, CFStringRef *port_name)
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
			*port_name = pci_port(masterPort, pci_slot_name, bsd_name);
			CFRelease(pci_slot_name);
		}
		ok = TRUE;
	}

	CFRelease(bsd_name);
	return ok;
}


static Boolean
isBuiltIn(io_registry_entry_t interface)
{
	kern_return_t		kr;
	io_registry_entry_t	slot	= interface;

	while (slot != MACH_PORT_NULL) {
		io_registry_entry_t	parent;
		CFTypeRef		slot_name;

		slot_name = IORegistryEntryCreateCFProperty(slot, CFSTR("AAPL,slot-name"), NULL, 0);
		if (slot_name != NULL) {
			Boolean		found;

			found = IOStringValueHasPrefix(slot_name, CFSTR("slot"));
			CFRelease(slot_name);

			if (found) {
				// if we found a slot # then this is not a built-in interface
				if (slot != interface) IOObjectRelease(slot);
				return FALSE;
			}
		}

		kr = IORegistryEntryGetParentEntry(slot, kIOServicePlane, &parent);
		if (slot != interface) IOObjectRelease(slot);
		switch (kr) {
			case kIOReturnSuccess :
				slot = parent;
				break;
			case kIOReturnNoDevice :
				// if we have hit the root node without finding a slot #
				return TRUE;
			default :
				SCLog(TRUE, LOG_INFO, CFSTR("isBuiltIn IORegistryEntryGetParentEntry() failed, kr = 0x%x"), kr);
				return FALSE;
		}
	}

	return FALSE;
}


/* ---------- interface enumeration ---------- */


typedef Boolean (*processInterface)(mach_port_t				masterPort,
				    SCNetworkInterfacePrivateRef	interfacePrivate,
				    io_registry_entry_t			interface,
				    CFDictionaryRef			interface_dict,
				    io_registry_entry_t			controller,
				    CFDictionaryRef			controller_dict,
				    io_registry_entry_t			bus,
				    CFDictionaryRef			bus_dict);


static Boolean
processNetworkInterface(mach_port_t			masterPort,
			SCNetworkInterfacePrivateRef	interfacePrivate,
			io_registry_entry_t		interface,
			CFDictionaryRef			interface_dict,
			io_registry_entry_t		controller,
			CFDictionaryRef			controller_dict,
			io_registry_entry_t		bus,
			CFDictionaryRef			bus_dict)
{
	CFBooleanRef	bVal;
	io_name_t	c_IOClass;
	io_name_t	c_IOName;
	io_name_t	i_IOClass;
	int		ift	= -1;
	int		iVal;
	CFNumberRef	num;
	CFStringRef	str;

	// get the interface type

	num = CFDictionaryGetValue(interface_dict, CFSTR(kIOInterfaceType));
	if (!isA_CFNumber(num) ||
	    !CFNumberGetValue(num, kCFNumberIntType, &ift)) {
		SCPrint(TRUE, stderr, CFSTR("Could not get interface type\n"));
		return FALSE;
	}

	switch (ift) {
		case IFT_ETHER :
			// Type, Hardware

			if (IOObjectGetClass(interface, i_IOClass) != KERN_SUCCESS) {
				i_IOClass[0] = '\0';
			}
			if (IOObjectGetClass(controller, c_IOClass) != KERN_SUCCESS) {
				c_IOClass[0] = '\0';
			}
			if (IORegistryEntryGetName(controller, c_IOName) != KERN_SUCCESS) {
				c_IOName[0] = '\0';
			}

			if ((strcmp(i_IOClass, "IO80211Interface"  ) == 0) ||
			    (strcmp(c_IOClass, "AirPortPCI"        ) == 0) ||
			    (strcmp(c_IOClass, "AirPortDriver"     ) == 0) ||
			    (strcmp(c_IOName , "AppleWireless80211") == 0)) {
				interfacePrivate->interface_type	= kSCNetworkInterfaceTypeIEEE80211;
				interfacePrivate->entity_type		= kSCEntNetEthernet;
				interfacePrivate->entity_hardware	= kSCEntNetAirPort;
				interfacePrivate->sort_order		= kSortAirPort;
			} else {
				str = IODictionaryCopyCFStringValue(bus_dict, CFSTR("name"));
				if ((str != NULL) && CFEqual(str, CFSTR("radio"))) {
					interfacePrivate->interface_type	= kSCNetworkInterfaceTypeEthernet;	// ??
					interfacePrivate->entity_type		= kSCEntNetEthernet;
					interfacePrivate->entity_hardware	= kSCEntNetEthernet;			// ??
					interfacePrivate->sort_order		= kSortOtherWireless;
				} else {
					interfacePrivate->interface_type	= kSCNetworkInterfaceTypeEthernet;
					interfacePrivate->entity_type		= kSCEntNetEthernet;
					interfacePrivate->entity_hardware	= kSCEntNetEthernet;
					interfacePrivate->sort_order		= kSortEthernet;

					// BOND support only enabled for ethernet devices
					interfacePrivate->supportsBond = TRUE;
				}

				if (str != NULL) CFRelease(str);
			}

			// built-in
			bVal = isA_CFBoolean(CFDictionaryGetValue(interface_dict, CFSTR(kIOBuiltin)));
			if ((bVal == NULL) || !CFBooleanGetValue(bVal)) {
				bVal = isA_CFBoolean(CFDictionaryGetValue(interface_dict, CFSTR(kIOPrimaryInterface)));
			}
			if (bVal != NULL) {
				interfacePrivate->builtin = CFBooleanGetValue(bVal);
			} else {
				interfacePrivate->builtin = isBuiltIn(interface);
			}

			// location
			interfacePrivate->location = IODictionaryCopyCFStringValue(interface_dict, CFSTR(kIOLocation));

			// VLAN support
			num = CFDictionaryGetValue(controller_dict, CFSTR(kIOFeatures));
			if (isA_CFNumber(num) &&
			    CFNumberGetValue(num, kCFNumberIntType, & iVal)) {
				if (iVal & (kIONetworkFeatureHardwareVlan | kIONetworkFeatureSoftwareVlan)) {
					interfacePrivate->supportsVLAN = TRUE;
				}
			}

			// localized name
			if (interfacePrivate->builtin) {
				if ((interfacePrivate->location == NULL) ||
				    (CFStringGetLength(interfacePrivate->location) == 0)) {
					interfacePrivate->localized_key = CFSTR("ether");
				} else {
					interfacePrivate->localized_key  = CFSTR("multiether");
					interfacePrivate->localized_arg1 = CFRetain(interfacePrivate->location);
				}
			} else if (CFEqual(interfacePrivate->interface_type, kSCNetworkInterfaceTypeIEEE80211)) {
				interfacePrivate->localized_key = CFSTR("airport");
			} else  if (interfacePrivate->sort_order == kSortOtherWireless) {
				interfacePrivate->localized_key  = CFSTR("wireless");
				interfacePrivate->localized_arg1 = CFRetain(CFSTR(""));		// ??
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

						if (pci_slot_info(masterPort, interface, &slot_name, &port_name)) {
							if (port_name == NULL) {
								interfacePrivate->localized_key  = CFSTR("pci-ether");
								interfacePrivate->localized_arg1 = slot_name;
							} else {
								interfacePrivate->localized_key  = CFSTR("pci-multiether");
								interfacePrivate->localized_arg1 = slot_name;
								interfacePrivate->localized_arg2 = port_name;
							}
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
			interfacePrivate->entity_type     = kSCEntNetFireWire;
			interfacePrivate->entity_hardware = kSCEntNetFireWire;

			// built-in
			interfacePrivate->builtin = isBuiltIn(interface);

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
			SCPrint(TRUE, stderr, CFSTR("Unknown interface type = %d\n"), ift);
			return FALSE;
	}

	// Device
	interfacePrivate->entity_device = IODictionaryCopyCFStringValue(interface_dict, CFSTR(kIOBSDNameKey));

	// Hardware (MAC) address
	interfacePrivate->address = copyMACAddress(controller_dict);

	return TRUE;
}


static Boolean
processSerialInterface(mach_port_t			masterPort,
		       SCNetworkInterfacePrivateRef	interfacePrivate,
		       io_registry_entry_t		interface,
		       CFDictionaryRef			interface_dict,
		       io_registry_entry_t		controller,
		       CFDictionaryRef			controller_dict,
		       io_registry_entry_t		bus,
		       CFDictionaryRef			bus_dict)
{
	CFStringRef		ift;
	Boolean			isModem	= FALSE;
	CFStringRef		str;
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

	// Type
	str = CFDictionaryGetValue(interface_dict, CFSTR(kIOTTYBaseNameKey));
	if (str == NULL) {
		return FALSE;
	}

	/*
	 * From MoreSCF:
	 *
	 * Exclude ports named "irda" because otherwise the IrDA ports on the
	 * original iMac (rev's A through D) show up as serial ports.  Given
	 * that only the rev A actually had an IrDA port, and Mac OS X doesn't
	 * even support it, these ports definitely shouldn't be listed.
	 */
	if (CFStringCompare(str,
			    CFSTR("irda"),
			    kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
		return FALSE;
	}

	if (IOStringValueHasPrefix(str, CFSTR("irda-ircomm"))) {
		// IrDA
		interfacePrivate->interface_type	= kSCNetworkInterfaceTypeIrDA;
		interfacePrivate->sort_order		= kSortIrDA;
	} else if (IOStringValueHasPrefix(str, CFSTR("bluetooth"))) {
		// Bluetooth
		interfacePrivate->interface_type	= kSCNetworkInterfaceTypeBluetooth;
		interfacePrivate->sort_order		= kSortBluetooth;
	} else {
		// Modem
		interfacePrivate->interface_type	= kSCNetworkInterfaceTypeModem;

		// DeviceOnHold support
		val = IORegistryEntrySearchCFProperty(interface,
						      kIOServicePlane,
						      CFSTR(kIODeviceSupportsHoldKey),
						      NULL,
						      kIORegistryIterateRecursively | kIORegistryIterateParents);
		if (val != NULL) {
			uint32_t	supportsHold;

			if (isA_CFNumber(val) &&
			    CFNumberGetValue(val, kCFNumberSInt32Type, &supportsHold)) {
				interfacePrivate->supportsDeviceOnHold = (supportsHold == 1);
			}
			CFRelease(val);
		}
	}

	// Entity (Type)
	interfacePrivate->entity_type = kSCEntNetModem;

	// Entity (Hardware)
	ift = CFDictionaryGetValue(interface_dict, CFSTR(kIOSerialBSDTypeKey));
	if (!isA_CFString(ift)) {
		return FALSE;
	}

	if (CFEqual(ift, CFSTR(kIOSerialBSDModemType))) {
		// if modem
		isModem = TRUE;
		interfacePrivate->entity_hardware = kSCEntNetModem;

		if (CFEqual(str, CFSTR("modem"))) {
			interfacePrivate->builtin = TRUE;
			interfacePrivate->sort_order = kSortInternalModem;
		} else if (CFEqual(str, CFSTR("usbmodem"))) {
			interfacePrivate->sort_order = kSortUSBModem;
		} else {
			interfacePrivate->sort_order = kSortModem;
		}
	} else if (CFEqual(ift, CFSTR(kIOSerialBSDRS232Type))) {
		// if serial port
		interfacePrivate->entity_hardware = kSCEntNetModem;
		interfacePrivate->sort_order = kSortSerialPort;
	} else {
		return FALSE;
	}

	// Entity (Device)
	interfacePrivate->entity_device = IODictionaryCopyCFStringValue(interface_dict, CFSTR(kIOTTYDeviceKey));

	// localized name
	if (CFEqual(interfacePrivate->interface_type, kSCNetworkInterfaceTypeIrDA)) {
		interfacePrivate->localized_key = CFSTR("irda");
	} else if (CFEqual(interfacePrivate->interface_type, kSCNetworkInterfaceTypeBluetooth)) {
		interfacePrivate->localized_key = CFSTR("bluetooth");
	} else {
		CFStringRef		localized;
		CFMutableStringRef	port;

		port = CFStringCreateMutableCopy(NULL, 0, str);
		CFStringLowercase(port, NULL);

		if (!isModem) {
			CFStringAppend(port, CFSTR("-port"));
		}

		localized = CFBundleCopyLocalizedString(bundle,
							port,
							port,
							NETWORKINTERFACE_LOCALIZATIONS);
		if (localized != NULL) {
			if (!CFEqual(port, localized)) {
				// if localization available
				interfacePrivate->localized_name = localized;
			} else {
				// if no localization available, use TTY base name
				CFRelease(localized);
				interfacePrivate->localized_name = CFStringCreateCopy(NULL, str);
			}
		} else {
			interfacePrivate->localized_name = CFStringCreateCopy(NULL, str);
		}

		if (!isModem || !CFEqual(str, CFSTR("modem"))) {
			CFStringRef	productName;

			// check if a "Product Name" has been provided
			val = IORegistryEntrySearchCFProperty(interface,
							      kIOServicePlane,
							      CFSTR(kIOPropertyProductNameKey),
							      NULL,
							      kIORegistryIterateRecursively | kIORegistryIterateParents);
			if (val != NULL) {
				productName = IOCopyCFStringValue(val);
				CFRelease(val);

				if (productName != NULL) {
					if (CFStringGetLength(productName) > 0) {
						// if we have a [somewhat reasonable?] product name
						CFRelease(interfacePrivate->localized_name);
						interfacePrivate->localized_name = CFRetain(productName);
					}
					CFRelease(productName);
				}
			}
		}

		CFRelease(port);
	}

	return TRUE;
}


static CFArrayRef
findMatchingInterfaces(mach_port_t masterPort, CFDictionaryRef matching, processInterface func)
{
	CFMutableArrayRef	interfaces;
	io_registry_entry_t	interface;
	kern_return_t		kr;
	io_iterator_t		iterator	= MACH_PORT_NULL;

	kr = IOServiceGetMatchingServices(masterPort, matching, &iterator);
	if (kr != kIOReturnSuccess) {
		SCPrint(TRUE, stderr, CFSTR("IOServiceGetMatchingServices() failed, kr = 0x%x\n"), kr);
		return NULL;
	}

	interfaces = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	while ((interface = IOIteratorNext(iterator)) != MACH_PORT_NULL) {
		io_registry_entry_t		bus			= MACH_PORT_NULL;
		CFMutableDictionaryRef		bus_dict		= NULL;
		io_registry_entry_t		controller		= MACH_PORT_NULL;
		CFMutableDictionaryRef		controller_dict		= NULL;
		SCNetworkInterfacePrivateRef	interfacePrivate	= NULL;
		CFMutableDictionaryRef		interface_dict		= NULL;
		io_string_t			path;

		kr = IORegistryEntryGetPath(interface, kIOServicePlane, path);
		if (kr != kIOReturnSuccess) {
			SCPrint(TRUE, stderr, CFSTR("IORegistryEntryGetPath() failed, kr = 0x%x"), kr);
			goto done;
		}

		kr = IORegistryEntryCreateCFProperties(interface, &interface_dict, NULL, kNilOptions);
		if (kr != kIOReturnSuccess) {
			SCPrint(TRUE, stderr, CFSTR("IORegistryEntryCreateCFProperties() failed, kr = 0x%x\n"), kr);
			goto done;
		}

		/* get the controller node */
		kr = IORegistryEntryGetParentEntry(interface, kIOServicePlane, &controller);
		if (kr != KERN_SUCCESS) {
			SCLog(TRUE, LOG_INFO, CFSTR("findMatchingInterfaces IORegistryEntryGetParentEntry() failed, kr = 0x%x"), kr);
			goto done;
		}

		/* get the dictionary associated with the node */
		kr = IORegistryEntryCreateCFProperties(controller, &controller_dict, NULL, kNilOptions);
		if (kr != KERN_SUCCESS) {
			SCLog(TRUE, LOG_INFO, CFSTR("findMatchingInterfaces IORegistryEntryCreateCFProperties() failed, kr = 0x%x"), kr);
			goto done;
		}

		/* get the bus node */
		kr = IORegistryEntryGetParentEntry(controller, kIOServicePlane, &bus);
		if (kr != KERN_SUCCESS) {
			SCLog(TRUE, LOG_INFO, CFSTR("findMatchingInterfaces IORegistryEntryGetParentEntry() failed, kr = 0x%x"), kr);
			goto done;
		}

		/* get the dictionary associated with the node */
		kr = IORegistryEntryCreateCFProperties(bus, &bus_dict, NULL, kNilOptions);
		if (kr != KERN_SUCCESS) {
			SCLog(TRUE, LOG_INFO, CFSTR("findMatchingInterfaces IORegistryEntryCreateCFProperties() failed, kr = 0x%x"), kr);
			goto done;
		}

		interfacePrivate = __SCNetworkInterfaceCreatePrivate(NULL, NULL, NULL, path);

		if ((*func)(masterPort, interfacePrivate, interface, interface_dict, controller, controller_dict, bus, bus_dict)) {
			CFArrayAppendValue(interfaces, (SCNetworkInterfaceRef)interfacePrivate);
		}

		CFRelease(interfacePrivate);

	    done:

		if (interface != MACH_PORT_NULL)	IOObjectRelease(interface);
		if (interface_dict != NULL)		CFRelease(interface_dict);

		if (controller != MACH_PORT_NULL)	IOObjectRelease(controller);
		if (controller_dict != NULL)		CFRelease(controller_dict);

		if (bus != MACH_PORT_NULL)		IOObjectRelease(bus);
		if (bus_dict != NULL)			CFRelease(bus_dict);
	}

	IOObjectRelease(iterator);

	return interfaces;
}


/* ---------- Bond configuration ---------- */

Boolean
SCNetworkInterfaceSupportsBonding(SCNetworkInterfaceRef interface)
{
	return ((SCNetworkInterfacePrivateRef)interface)->supportsBond;
}


SCNetworkInterfaceRef
SCNetworkInterfaceCreateWithBond(BondInterfaceRef bond)
{
	SCNetworkInterfacePrivateRef	interfacePrivate;
	CFStringRef                     bond_if;

	bond_if = BondInterfaceGetInterface(bond);
	if (bond_if == NULL) {
		return NULL;
	}

	interfacePrivate = __SCNetworkInterfaceCreatePrivate(NULL, NULL, NULL, NULL);
	if (interfacePrivate == NULL) {
		return NULL;
	}

	interfacePrivate->interface_type        = kSCNetworkInterfaceTypeBond;
	interfacePrivate->entity_type           = kSCEntNetEthernet;
	interfacePrivate->entity_hardware       = kSCEntNetEthernet;
	interfacePrivate->entity_device         = CFStringCreateCopy(NULL, bond_if);
	interfacePrivate->builtin               = TRUE;
	interfacePrivate->sort_order            = kSortBond;

	interfacePrivate->localized_key		= CFSTR("bond");
	interfacePrivate->localized_arg1	= CFRetain(interfacePrivate->entity_device);

	return (SCNetworkInterfaceRef)interfacePrivate;
}


static CFArrayRef
findBondInterfaces(CFStringRef match)
{
	CFMutableArrayRef	interfaces	= NULL;
	CFIndex			i;
	CFIndex			n;
	BondPreferencesRef	prefs;
	CFArrayRef		bonds		= NULL;

	prefs = BondPreferencesCreate(NULL);
	if (prefs == NULL) {
		// if no bonds
		return NULL;
	}

	bonds = BondPreferencesCopyInterfaces(prefs);
	if (bonds == NULL) {
		// if no bonds
		goto done;
	}

	n = CFArrayGetCount(bonds);
	if (n == 0) {
		// if no bonds
		goto done;
	}

	interfaces = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	for (i = 0; i < n; i++) {
		SCNetworkInterfaceRef	interface;
		BondInterfaceRef	bond		= CFArrayGetValueAtIndex(bonds, i);
		CFStringRef		bond_if;

		bond_if = BondInterfaceGetInterface(bond);
		if (bond_if == NULL) {
			continue;
		}

		if ((match != NULL) && !CFEqual(bond_if, match)) {
			continue;
		}

		interface = SCNetworkInterfaceCreateWithBond(bond);
		CFArrayAppendValue(interfaces, interface);
		CFRelease(interface);
	}

    done :

	if (bonds != NULL)	CFRelease(bonds);
	CFRelease(prefs);
	return interfaces;
}


/* ---------- VLAN configuration ---------- */

SCNetworkInterfaceRef
SCNetworkInterfaceCreateWithVLAN(VLANInterfaceRef vlan)
{
	SCNetworkInterfacePrivateRef	interfacePrivate;
	CFStringRef                     vlan_if;

	vlan_if = VLANInterfaceGetInterface(vlan);
	if (vlan_if == NULL) {
		return NULL;
	}

	interfacePrivate = __SCNetworkInterfaceCreatePrivate(NULL, NULL, NULL, NULL);
	if (interfacePrivate == NULL) {
		return NULL;
	}

	interfacePrivate->interface_type	= kSCNetworkInterfaceTypeVLAN;
	interfacePrivate->entity_type		= kSCEntNetEthernet;
	interfacePrivate->entity_hardware	= kSCEntNetEthernet;
	interfacePrivate->entity_device		= CFStringCreateCopy(NULL, vlan_if);
	interfacePrivate->builtin		= TRUE;
	interfacePrivate->sort_order		= kSortVLAN;

	interfacePrivate->localized_key		= CFSTR("vlan");
	interfacePrivate->localized_arg1	= CFRetain(interfacePrivate->entity_device);

	return (SCNetworkInterfaceRef)interfacePrivate;
}


static CFArrayRef
findVLANInterfaces(CFStringRef match)
{
	CFMutableArrayRef	interfaces	= NULL;
	CFIndex			i;
	CFIndex			n;
	VLANPreferencesRef	prefs;
	CFArrayRef		vlans		= NULL;

	prefs = VLANPreferencesCreate(NULL);
	if (prefs == NULL) {
		// if no VLANs
		return NULL;
	}

	vlans = VLANPreferencesCopyInterfaces(prefs);
	if (vlans == NULL) {
		// if no VLANs
		goto done;
	}

	n = CFArrayGetCount(vlans);
	if (n == 0) {
		// if no VLANs
		goto done;
	}

	interfaces = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	for (i = 0; i < n; i++) {
		SCNetworkInterfaceRef	interface;
		VLANInterfaceRef	vlan		= CFArrayGetValueAtIndex(vlans, i);
		CFStringRef		vlan_if;

		vlan_if = VLANInterfaceGetInterface(vlan);
		if (vlan_if == NULL) {
			continue;
		}

		if ((match != NULL) && !CFEqual(vlan_if, match)) {
			continue;
		}

		interface = SCNetworkInterfaceCreateWithVLAN(vlan);
		CFArrayAppendValue(interfaces, interface);
		CFRelease(interface);
	}

    done :

	if (vlans != NULL)	CFRelease(vlans);
	CFRelease(prefs);
	return interfaces;
}


/* ---------- interface from preferences ---------- */


__private_extern__ SCNetworkInterfaceRef
__SCNetworkInterfaceCreateWithEntity(CFAllocatorRef		allocator,
				     CFDictionaryRef		interface_entity,
				     SCNetworkServiceRef	service)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= NULL;
	CFStringRef			ifDevice;
	CFStringRef			ifSubType;
	CFStringRef			ifType;
	static mach_port_t		masterPort		= MACH_PORT_NULL;
	CFArrayRef			matching_interfaces	= NULL;

	if (masterPort == MACH_PORT_NULL) {
		kern_return_t   kr;

		kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
		if (kr != KERN_SUCCESS) {
			return NULL;
		}
	}

	ifType = CFDictionaryGetValue(interface_entity, kSCPropNetInterfaceType);
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
		matching_interfaces = findMatchingInterfaces(masterPort, matching, processNetworkInterface);

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

			match_keys[1] = CFSTR(kIOTTYDeviceKey);
			match_vals[1] = ifDevice;

			matching = CFDictionaryCreate(NULL,
						      (const void **)match_keys,
						      (const void **)match_vals,
						      sizeof(match_keys)/sizeof(match_keys[0]),
						      &kCFTypeDictionaryKeyCallBacks,
						      &kCFTypeDictionaryValueCallBacks);

			// note: the "matching" dictionary will be consumed by the following
			matching_interfaces = findMatchingInterfaces(masterPort, matching, processSerialInterface);

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
	}

	if (matching_interfaces != NULL) {
		CFIndex n;

		n = CFArrayGetCount(matching_interfaces);
		switch (n) {
			case 0 :
				if (CFEqual(ifType, kSCValNetInterfaceTypeEthernet)) {
					CFArrayRef	bonds;
					CFArrayRef	vlans;

					bonds = findBondInterfaces(ifDevice);
					if (bonds != NULL) {
						if (CFArrayGetCount(bonds) == 1) {
							interfacePrivate = (SCNetworkInterfacePrivateRef)CFArrayGetValueAtIndex(bonds, 0);
							CFRetain(interfacePrivate);
						}
						CFRelease(bonds);
						break;
					}

					vlans = findVLANInterfaces(ifDevice);
					if (vlans != NULL) {
						if (CFArrayGetCount(vlans) == 1) {
							interfacePrivate = (SCNetworkInterfacePrivateRef)CFArrayGetValueAtIndex(vlans, 0);
							CFRetain(interfacePrivate);
						}
						CFRelease(vlans);
						break;
					}
				}
				break;
			case 1 :
				interfacePrivate = (SCNetworkInterfacePrivateRef)CFArrayGetValueAtIndex(matching_interfaces, 0);
				CFRetain(interfacePrivate);
				break;
			default :
				SCPrint(TRUE, stderr, CFSTR("more than one interface matches %@\n"), ifDevice);
				if (matching_interfaces != NULL) CFRelease(matching_interfaces);
				_SCErrorSet(kSCStatusFailed);
				return NULL;
		}
		CFRelease(matching_interfaces);
	}

    done :

	if (interfacePrivate == NULL) {
		/*
		 * if device not present on this system
		 */
		interfacePrivate = __SCNetworkInterfaceCreatePrivate(NULL, NULL, NULL, NULL);
		interfacePrivate->entity_type     = ifType;
		interfacePrivate->entity_subtype  = ifSubType;
		interfacePrivate->entity_device   = (ifDevice != NULL) ? CFStringCreateCopy(NULL, ifDevice) : NULL;
		interfacePrivate->entity_hardware = CFDictionaryGetValue(interface_entity, kSCPropNetInterfaceHardware);

		if (CFEqual(ifType, kSCValNetInterfaceTypeEthernet)) {
			if ((interfacePrivate->entity_hardware != NULL) &&
			    CFEqual(interfacePrivate->entity_hardware, kSCEntNetAirPort)) {
				interfacePrivate->interface_type = kSCNetworkInterfaceTypeIEEE80211;
			} else {
				interfacePrivate->interface_type = kSCNetworkInterfaceTypeEthernet;
			}
		} else if (CFEqual(ifType, kSCValNetInterfaceTypeFireWire)) {
			interfacePrivate->interface_type = kSCNetworkInterfaceTypeFireWire;
		} else if (CFEqual(ifType, kSCValNetInterfaceTypePPP)) {
			if (CFEqual(ifSubType, kSCValNetInterfaceSubTypePPPoE)) {
				interfacePrivate->interface_type = kSCNetworkInterfaceTypeEthernet;
			} else if (CFEqual(ifSubType, kSCValNetInterfaceSubTypePPPSerial)) {
				if        (CFStringHasPrefix(ifDevice, CFSTR("irda"))) {
					interfacePrivate->interface_type = kSCNetworkInterfaceTypeIrDA;
				} else if (CFStringHasPrefix(ifDevice, CFSTR("Bluetooth"))) {
					interfacePrivate->interface_type = kSCNetworkInterfaceTypeBluetooth;
				} else {
					interfacePrivate->interface_type = kSCNetworkInterfaceTypeModem;
				}
			} else {
				// PPTP, L2TP, ...
				CFRelease(interfacePrivate);
				interfacePrivate = (SCNetworkInterfacePrivateRef)kSCNetworkInterfaceIPv4;
				CFRetain(interfacePrivate);
			}
		} else {
			// unknown interface type
			CFRelease(interfacePrivate);
			interfacePrivate = NULL;
		}
	}

	if ((interfacePrivate != NULL) && (service != NULL)) {
		interfacePrivate->service = service;
	}

	if (CFEqual(ifType, kSCValNetInterfaceTypePPP)) {
		SCNetworkInterfaceRef   parent;

		parent = SCNetworkInterfaceCreateWithInterface((SCNetworkInterfaceRef)interfacePrivate,
							       kSCNetworkInterfaceTypePPP);
		CFRelease(interfacePrivate);
		interfacePrivate = (SCNetworkInterfacePrivateRef)parent;
	}

	return (SCNetworkInterfaceRef)interfacePrivate;
}


/* ---------- helper functions ---------- */


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


static CFArrayRef
copyConfigurationPaths(SCNetworkInterfacePrivateRef interfacePrivate)
{
	CFMutableArrayRef		array		= NULL;
	CFIndex				interfaceIndex;
	CFStringRef			path;
	SCNetworkServicePrivateRef      servicePrivate;

	interfaceIndex = findConfiguration(interfacePrivate->interface_type);
	if (interfaceIndex == kCFNotFound) {
		// if unknown interface type
		return NULL;
	}

	servicePrivate = (SCNetworkServicePrivateRef)interfacePrivate->service;
	if (servicePrivate == NULL) {
		// if not associated with a service (yet)
		return NULL;
	}

	array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	if (configurations[interfaceIndex].per_interface_config) {
		CFIndex		i;
		CFIndex		n;
		CFArrayRef      sets;

		/*
		 * per-interface configuration preferences
		 *
		 * 1. look for all sets which contain the associated service
		 * 2. add a per-set path for the interface configuration for
		 *    each set.
		 */

		sets = SCNetworkSetCopyAll(servicePrivate->prefs);
		n = (sets != NULL) ? CFArrayGetCount(sets) : 0;

		for (i = 0; i < n; i++) {
			CFArrayRef      services;
			SCNetworkSetRef set;

			set = CFArrayGetValueAtIndex(sets, i);
			services = SCNetworkSetCopyServices(set);
			if (CFArrayContainsValue(services,
						 CFRangeMake(0, CFArrayGetCount(services)),
						 interfacePrivate->service)) {
				path = SCPreferencesPathKeyCreateSetNetworkInterfaceEntity(NULL,				// allocator
											   SCNetworkSetGetSetID(set),		// set
											   interfacePrivate->entity_device,	// service
											   interfacePrivate->entity_type);	// entity
				CFArrayAppendValue(array, path);
				CFRelease(path);
			}
			CFRelease(services);
		}

		if (CFArrayGetCount(array) == 0) {
			CFRelease(array);
			array = NULL;
		}

		CFRelease(sets);
	} else {
		// per-service configuration preferences
		path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,					// allocator
								      servicePrivate->serviceID,		// service
								      interfacePrivate->entity_type);		// entity
		CFArrayAppendValue(array, path);
		CFRelease(path);
	}

	return array;
}


/* ---------- SCNetworkInterface APIs ---------- */


CFArrayRef /* of SCNetworkInterfaceRef's */
SCNetworkInterfaceCopyAll()
{
	CFMutableArrayRef	all_interfaces;
	static mach_port_t      masterPort      = MACH_PORT_NULL;
	CFDictionaryRef		matching;
	CFStringRef		match_keys[2];
	CFStringRef		match_vals[2];
	CFArrayRef		new_interfaces;

	if (masterPort == MACH_PORT_NULL) {
		kern_return_t   kr;

		kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
		if (kr != KERN_SUCCESS) {
			return NULL;
		}
	}

	all_interfaces = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	// get Ethernet, Firewire, and AirPort interfaces

	matching = IOServiceMatching(kIONetworkInterfaceClass);
	new_interfaces = findMatchingInterfaces(masterPort, matching, processNetworkInterface);
	if (new_interfaces != NULL) {
		CFArrayAppendArray(all_interfaces, new_interfaces, CFRangeMake(0, CFArrayGetCount(new_interfaces)));
		CFRelease(new_interfaces);
	}

	// get Modem interfaces

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
	new_interfaces = findMatchingInterfaces(masterPort, matching, processSerialInterface);
	if (new_interfaces != NULL) {
		CFArrayAppendArray(all_interfaces, new_interfaces, CFRangeMake(0, CFArrayGetCount(new_interfaces)));
		CFRelease(new_interfaces);
	}

	// get serial (RS232) interfaces

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
	new_interfaces = findMatchingInterfaces(masterPort, matching, processSerialInterface);
	if (new_interfaces != NULL) {
		CFArrayAppendArray(all_interfaces, new_interfaces, CFRangeMake(0, CFArrayGetCount(new_interfaces)));
		CFRelease(new_interfaces);
	}

	new_interfaces = findBondInterfaces(NULL);
	if (new_interfaces != NULL) {
		CFArrayAppendArray(all_interfaces, new_interfaces, CFRangeMake(0, CFArrayGetCount(new_interfaces)));
		CFRelease(new_interfaces);
	}

	new_interfaces = findVLANInterfaces(NULL);
	if (new_interfaces != NULL) {
		CFArrayAppendArray(all_interfaces, new_interfaces, CFRangeMake(0, CFArrayGetCount(new_interfaces)));
		CFRelease(new_interfaces);
	}

	sort_interfaces(all_interfaces);

	return all_interfaces;
}


CFArrayRef /* of kSCNetworkInterfaceTypeXXX CFStringRef's */
SCNetworkInterfaceGetSupportedInterfaceTypes(SCNetworkInterfaceRef interface)
{
	CFIndex				i;
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	if (interfacePrivate->supported_interface_types != NULL) {
		goto done;
	}

	/* initialize runtime (and kSCNetworkInterfaceIPv4) */
	pthread_once(&initialized, __SCNetworkInterfaceInitialize);

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

	if (interfacePrivate->supported_protocol_types != NULL) {
		goto done;
	}

	/* initialize runtime (and kSCNetworkInterfaceIPv4) */
	pthread_once(&initialized, __SCNetworkInterfaceInitialize);

	i = findConfiguration(interfacePrivate->interface_type);
	if (i != kCFNotFound) {
		if (configurations[i].supported_protocols != doNone) {
			interfacePrivate->supported_protocol_types = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			if (configurations[i].supported_protocols & doAppleTalk) {
				CFArrayAppendValue(interfacePrivate->supported_protocol_types, kSCNetworkProtocolTypeAppleTalk);
			}
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

	/* initialize runtime (and kSCNetworkInterfaceIPv4) */
	pthread_once(&initialized, __SCNetworkInterfaceInitialize);

	childIndex = findConfiguration(childPrivate->interface_type);

	parentPrivate = __SCNetworkInterfaceCreatePrivate(NULL, child, childPrivate->service, NULL);
	if (parentPrivate == NULL) {
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	if (CFEqual(interfaceType, kSCNetworkInterfaceTypePPP)) {
		parentPrivate->interface_type = kSCNetworkInterfaceTypePPP;
		parentPrivate->entity_type    = kSCEntNetPPP;

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
	} else if (CFEqual(interfaceType, kSCNetworkInterfaceTypeL2TP)) {
		if ((childIndex == kCFNotFound) ||
		    ((configurations[childIndex].supported_interfaces & doL2TP) != doL2TP)) {
			// if the child interface does not support L2TP
			goto fail;
		}
		parentPrivate->interface_type = kSCNetworkInterfaceTypeL2TP;
		parentPrivate->entity_type    = kSCValNetInterfaceSubTypeL2TP;	// interface config goes into "L2TP"
	} else if (CFEqual(interfaceType, kSCNetworkInterfaceTypePPTP)) {
		if ((childIndex == kCFNotFound) ||
		    ((configurations[childIndex].supported_interfaces & doPPTP) != doPPTP)) {
			// if the child interface does not support PPTP
			goto fail;
		}
		parentPrivate->interface_type = kSCNetworkInterfaceTypePPTP;
		parentPrivate->entity_type    = kSCValNetInterfaceSubTypePPTP;	// interface config goes into "PPTP"
	} else if (CFEqual(interfaceType, kSCNetworkInterfaceType6to4)) {
		if ((childIndex == kCFNotFound) ||
		    ((configurations[childIndex].supported_interfaces & do6to4) != do6to4)) {
			// if the child interface does not support 6to4
			goto fail;
		}

		parentPrivate->interface_type = kSCNetworkInterfaceType6to4;
		parentPrivate->entity_type    = kSCEntNet6to4;
		parentPrivate->entity_device  = CFRetain(CFSTR("stf0"));
		CFRetain(parentPrivate->entity_device);
	} else {
		// unknown interface type
		goto fail;
	}

	parentPrivate->entity_hardware = childPrivate->entity_hardware;
	parentPrivate->sort_order = childPrivate->sort_order;

	return (SCNetworkInterfaceRef)parentPrivate;

    fail :

	CFRelease(parentPrivate);
	_SCErrorSet(kSCStatusInvalidArgument);
	return NULL;
}


static CFDictionaryRef
__SCNetworkInterfaceGetConfiguration(SCNetworkInterfaceRef interface, Boolean okToHold)
{
	CFDictionaryRef			config			= NULL;
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;
	CFArrayRef			paths;

	/* initialize runtime (and kSCNetworkInterfaceIPv4) */
	pthread_once(&initialized, __SCNetworkInterfaceInitialize);

	paths = copyConfigurationPaths(interfacePrivate);
	if (paths != NULL) {
		CFStringRef			path;
		SCNetworkServicePrivateRef      servicePrivate  = (SCNetworkServicePrivateRef)interfacePrivate->service;

		path = CFArrayGetValueAtIndex(paths, 0);
		config = __getPrefsConfiguration(servicePrivate->prefs, path);

		CFRelease(paths);
	} else if (okToHold) {
		config = interfacePrivate->unsaved;
	}

	return config;
}


CFDictionaryRef
SCNetworkInterfaceGetConfiguration(SCNetworkInterfaceRef interface)
{
	return __SCNetworkInterfaceGetConfiguration(interface, FALSE);
}


CFStringRef
SCNetworkInterfaceGetBSDName(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	if (interfacePrivate->interface != NULL) {
		return NULL;
	}

	return interfacePrivate->entity_device;
}


CFStringRef
SCNetworkInterfaceGetHardwareAddressString(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	return interfacePrivate->address;
}

SCNetworkInterfaceRef
SCNetworkInterfaceGetInterface(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	return interfacePrivate->interface;
}


CFStringRef
SCNetworkInterfaceGetInterfaceType(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	/* initialize runtime (and kSCNetworkInterfaceIPv4) */
	pthread_once(&initialized, __SCNetworkInterfaceInitialize);

	return interfacePrivate->interface_type;
}


CFStringRef
SCNetworkInterfaceGetLocalizedDisplayName(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	if (interfacePrivate->localized_name == NULL) {
		CFStringRef		child	= NULL;
		CFMutableStringRef	local	= NULL;

		pthread_once(&initialized, __SCNetworkInterfaceInitialize);	/* initialize runtime */

		if (interfacePrivate->interface != NULL) {
			child = SCNetworkInterfaceGetLocalizedDisplayName(interfacePrivate->interface);
		}

		if (interfacePrivate->localized_key != NULL) {
			CFStringRef	fmt;

			fmt = CFBundleCopyLocalizedString(bundle,
							  interfacePrivate->localized_key,
							  interfacePrivate->localized_key,
							  NETWORKINTERFACE_LOCALIZATIONS);
			if (fmt != NULL) {
				local = CFStringCreateMutable(NULL, 0);
				CFStringAppendFormat(local,
						     NULL,
						     fmt,
						     interfacePrivate->localized_arg1,
						     interfacePrivate->localized_arg2);
				CFRelease(fmt);
			}
		}

		if (local == NULL) {
			// create (non-)localized name based on the interface type
			local = CFStringCreateMutableCopy(NULL, 0, interfacePrivate->interface_type);

			// ... and, if this is a leaf node, the interface device
			if ((interfacePrivate->entity_device != NULL) && (child == NULL)) {
				CFStringAppendFormat(local, NULL, CFSTR(" (%@)"), interfacePrivate->entity_device);
			}
		}

		if (child == NULL) {
			// no child, show just this interfaces localized name
			interfacePrivate->localized_name = CFStringCreateCopy(NULL, local);
		} else {
			// show localized interface name layered over child
			interfacePrivate->localized_name = CFStringCreateWithFormat(NULL,
										    NULL,
										    CFSTR("%@ --> %@"),
										    local,
										    child);
		}
		CFRelease(local);
	}

	return interfacePrivate->localized_name;
}


CFTypeID
SCNetworkInterfaceGetTypeID(void)
{
	pthread_once(&initialized, __SCNetworkInterfaceInitialize);	/* initialize runtime */
	return __kSCNetworkInterfaceTypeID;
}


__private_extern__ Boolean
__SCNetworkInterfaceSetConfiguration(SCNetworkInterfaceRef interface, CFDictionaryRef config, Boolean okToHold)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;
	Boolean				ok			= FALSE;
	CFArrayRef			paths;

	/* initialize runtime (and kSCNetworkInterfaceIPv4) */
	pthread_once(&initialized, __SCNetworkInterfaceInitialize);

	paths = copyConfigurationPaths(interfacePrivate);
	if (paths != NULL) {
		CFIndex				i;
		CFIndex				n;
		SCPreferencesRef		prefs;
		SCNetworkServicePrivateRef      servicePrivate;

		servicePrivate  = (SCNetworkServicePrivateRef)interfacePrivate->service;
		prefs = servicePrivate->prefs;

		n = CFArrayGetCount(paths);
		for (i = 0; i < n; i++) {
			CFStringRef     path;

			path = CFArrayGetValueAtIndex(paths, i);
			ok = __setPrefsConfiguration(prefs, path, config, FALSE);
			if (!ok) {
				break;
			}
		}

		CFRelease(paths);
	} else if (okToHold) {
		interfacePrivate->unsaved = config;
		ok = TRUE;
	} else {
		_SCErrorSet(kSCStatusNoKey);
	}

	return ok;
}


Boolean
SCNetworkInterfaceSetConfiguration(SCNetworkInterfaceRef interface, CFDictionaryRef config)
{
	return __SCNetworkInterfaceSetConfiguration(interface, config, FALSE);
}


/* ---------- SCNetworkInterface internal SPIs ---------- */


__private_extern__ SCNetworkInterfacePrivateRef
__SCNetworkInterfaceCreateCopy(CFAllocatorRef		allocator,
			       SCNetworkInterfaceRef	interface,
			       SCNetworkServiceRef      service)
{
	SCNetworkInterfacePrivateRef		oldPrivate	= (SCNetworkInterfacePrivateRef)interface;
	SCNetworkInterfacePrivateRef		newPrivate;

	newPrivate = __SCNetworkInterfaceCreatePrivate(NULL, NULL, NULL, NULL);
	newPrivate->interface_type		= CFRetain(oldPrivate->interface_type);
	if (oldPrivate->interface != NULL) {
		newPrivate->interface = (SCNetworkInterfaceRef)__SCNetworkInterfaceCreateCopy(NULL,			// allocator
											      oldPrivate->interface,    // interface
											      service);			// [new] service
	}
	newPrivate->localized_name		= (oldPrivate->localized_name != NULL) ? CFRetain(oldPrivate->localized_name) : NULL;
	newPrivate->service			= service;
	newPrivate->unsaved			= (oldPrivate->unsaved != NULL) ? CFRetain(oldPrivate->unsaved) : NULL;
	newPrivate->entity_device		= (oldPrivate->entity_device != NULL) ? CFRetain(oldPrivate->entity_device) : NULL;
	newPrivate->entity_hardware		= CFRetain(oldPrivate->entity_hardware);
	newPrivate->entity_type			= oldPrivate->entity_type;
	newPrivate->entity_subtype		= oldPrivate->entity_subtype;
	if (oldPrivate->supported_interface_types != NULL) {
		newPrivate->supported_interface_types = CFArrayCreateMutableCopy(NULL, 0, oldPrivate->supported_interface_types);
	}
	if (oldPrivate->supported_protocol_types != NULL) {
		newPrivate->supported_protocol_types = CFArrayCreateMutableCopy(NULL, 0, oldPrivate->supported_protocol_types);
	}
	newPrivate->address			= (oldPrivate->address != NULL) ? CFRetain(oldPrivate->address) : NULL;
	newPrivate->builtin			= oldPrivate->builtin;
	newPrivate->path			= (oldPrivate->path != NULL) ? CFRetain(oldPrivate->path) : NULL;
	newPrivate->location			= (oldPrivate->location != NULL) ? CFRetain(oldPrivate->location) : NULL;
	newPrivate->supportsDeviceOnHold	= oldPrivate->supportsDeviceOnHold;
	newPrivate->supportsBond		= oldPrivate->supportsBond;
	newPrivate->supportsVLAN		= oldPrivate->supportsVLAN;
	newPrivate->sort_order			= oldPrivate->sort_order;

	return newPrivate;
}


__private_extern__ CFArrayRef
__SCNetworkInterfaceCopyDeepConfiguration(SCNetworkInterfaceRef interface)
{
	CFDictionaryRef		config;
	CFMutableArrayRef       configs;

	configs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	while (interface != NULL) {
		config = __SCNetworkInterfaceGetConfiguration(interface, TRUE);
		CFArrayAppendValue(configs,
				   (config != NULL) ? config : (CFDictionaryRef)kCFNull);
		interface = SCNetworkInterfaceGetInterface(interface);
	}

	return configs;
}


__private_extern__ void
__SCNetworkInterfaceSetDeepConfiguration(SCNetworkInterfaceRef interface, CFArrayRef configs)
{
	CFIndex		i;

	for (i = 0; interface != NULL; i++) {
		CFDictionaryRef config;

		config = (configs != NULL) ? CFArrayGetValueAtIndex(configs, i) : NULL;
		if (!isA_CFDictionary(config) || (CFDictionaryGetCount(config) == 0)) {
			config = NULL;
		}

		(void) __SCNetworkInterfaceSetConfiguration(interface, config, TRUE);
		interface = SCNetworkInterfaceGetInterface(interface);
	}

	return;
}
