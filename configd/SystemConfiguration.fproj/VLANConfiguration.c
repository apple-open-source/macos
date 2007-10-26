/*
 * Copyright (c) 2003-2007 Apple Inc. All rights reserved.
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
 * November 28, 2005		Allan Nathanson <ajn@apple.com>
 * - public API
 *
 * November 14, 2003		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include "SCNetworkConfigurationInternal.h"
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

#include <ifaddrs.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#define	KERNEL_PRIVATE
#include <net/if.h>
#include <net/if_var.h>
#undef	KERNEL_PRIVATE
#include <net/if_vlan_var.h>
#include <net/if_types.h>

#include <SystemConfiguration/VLANConfiguration.h>

/* ---------- VLAN support ---------- */

static int
inet_dgram_socket()
{
	int	s;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("socket() failed: %s"), strerror(errno));
	}

	return s;
}


typedef struct {
	CFMutableArrayRef	vlans;
	SCPreferencesRef	prefs;
} addContext, *addContextRef;


static void
add_configured_interface(const void *key, const void *value, void *context)
{
	SCNetworkInterfacePrivateRef	interfacePrivate;
	addContextRef			myContext	= (addContextRef)context;
	SCVLANInterfaceRef		vlan;
	CFStringRef			vlan_if		= (CFStringRef)key;
	CFDictionaryRef			vlan_info	= (CFDictionaryRef)value;
	CFStringRef			vlan_name;
	CFDictionaryRef			vlan_options;
	SCNetworkInterfaceRef		vlan_physical;
	CFStringRef			vlan_physical_if;
	CFNumberRef			vlan_tag;

	vlan_physical_if = CFDictionaryGetValue(vlan_info, kSCPropVirtualNetworkInterfacesVLANInterface);
	if (!isA_CFString(vlan_physical_if)) {
		// if prefs are confused
		return;
	}

	vlan_tag = CFDictionaryGetValue(vlan_info, kSCPropVirtualNetworkInterfacesVLANTag);
	if (!isA_CFNumber(vlan_tag)) {
		// if prefs are confused
		return;
	}

	// create the VLAN interface
	vlan = (SCVLANInterfaceRef)_SCVLANInterfaceCreatePrivate(NULL, vlan_if);

	// set physical interface and tag
	vlan_physical = _SCNetworkInterfaceCreateWithBSDName(NULL, vlan_physical_if,
							     kIncludeBondInterfaces);
	SCVLANInterfaceSetPhysicalInterfaceAndTag(vlan, vlan_physical, vlan_tag);
	CFRelease(vlan_physical);

	// set display name
	vlan_name = CFDictionaryGetValue(vlan_info, kSCPropUserDefinedName);
	if (isA_CFString(vlan_name)) {
		SCVLANInterfaceSetLocalizedDisplayName(vlan, vlan_name);
	}

	// set options
	vlan_options = CFDictionaryGetValue(vlan_info, kSCPropVirtualNetworkInterfacesVLANOptions);
	if (isA_CFDictionary(vlan_options)) {
		SCVLANInterfaceSetOptions(vlan, vlan_options);
	}

	// estabish link to the stored configuration
	interfacePrivate = (SCNetworkInterfacePrivateRef)vlan;
	interfacePrivate->prefs = CFRetain(myContext->prefs);

	CFArrayAppendValue(myContext->vlans, vlan);
	CFRelease(vlan);

	return;
}


static void
add_legacy_configuration(addContextRef myContext)
{
	CFIndex				i;
	CFIndex				n;
	SCPreferencesRef		prefs;
	CFArrayRef			vlans;

#define VLAN_PREFERENCES_ID		CFSTR("VirtualNetworkInterfaces.plist")
#define	VLAN_PREFERENCES_VLANS		CFSTR("VLANs")
#define	__kVLANInterface_interface	CFSTR("interface")	// e.g. vlan0, vlan1, ...
#define	__kVLANInterface_device		CFSTR("device")		// e.g. en0, en1, ...
#define __kVLANInterface_tag		CFSTR("tag")		// e.g. 1 <= tag <= 4094
#define __kVLANInterface_options	CFSTR("options")	// e.g. UserDefinedName

	prefs = SCPreferencesCreate(NULL, CFSTR("SCVLANInterfaceCopyAll"), VLAN_PREFERENCES_ID);
	if (prefs == NULL) {
		return;
	}

	vlans = SCPreferencesGetValue(prefs, VLAN_PREFERENCES_VLANS);
	if ((vlans != NULL) && !isA_CFArray(vlans)) {
		CFRelease(prefs);	// if the prefs are confused
		return;
	}

	n = (vlans != NULL) ? CFArrayGetCount(vlans) : 0;
	for (i = 0; i < n; i++) {
		CFDictionaryRef			dict;
		SCNetworkInterfacePrivateRef	interfacePrivate;
		Boolean				ok;
		CFDictionaryRef			options;
		CFStringRef			path;
		SCVLANInterfaceRef		vlan;
		CFStringRef			vlan_if;
		CFDictionaryRef			vlan_dict;
		SCNetworkInterfaceRef		vlan_physical;
		CFStringRef			vlan_physical_if;
		CFNumberRef			vlan_tag;

		vlan_dict = CFArrayGetValueAtIndex(vlans, i);
		if (!isA_CFDictionary(vlan_dict)) {
			continue;	// if the prefs are confused
		}

		vlan_if = CFDictionaryGetValue(vlan_dict, __kVLANInterface_interface);
		if (!isA_CFString(vlan_if)) {
			continue;	// if the prefs are confused
		}

		vlan_physical_if = CFDictionaryGetValue(vlan_dict, __kVLANInterface_device);
		if (!isA_CFString(vlan_physical_if)) {
			continue;	// if the prefs are confused
		}

		vlan_tag = CFDictionaryGetValue(vlan_dict, __kVLANInterface_tag);
		if (!isA_CFNumber(vlan_tag)) {
			continue;	// if the prefs are confused
		}

		// check if this VLAN interface has already been allocated
		path = CFStringCreateWithFormat(NULL,
						NULL,
						CFSTR("/%@/%@/%@"),
						kSCPrefVirtualNetworkInterfaces,
						kSCNetworkInterfaceTypeVLAN,
						vlan_if);
		dict = SCPreferencesPathGetValue(myContext->prefs, path);
		if (dict != NULL) {
			// if VLAN interface name not available
			CFRelease(path);
			continue;
		}

		// add a placeholder for the VLAN in the stored preferences
		dict = CFDictionaryCreate(NULL,
					  NULL, NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
		ok = SCPreferencesPathSetValue(myContext->prefs, path, dict);
		CFRelease(dict);
		CFRelease(path);
		if (!ok) {
			// if the VLAN could not be saved
			continue;
		}

		// create the VLAN interface
		vlan = (SCVLANInterfaceRef)_SCVLANInterfaceCreatePrivate(NULL, vlan_if);

		// estabish link to the stored configuration
		interfacePrivate = (SCNetworkInterfacePrivateRef)vlan;
		interfacePrivate->prefs = CFRetain(myContext->prefs);

		// set the interface and tag (which updates the stored preferences)
		vlan_physical = _SCNetworkInterfaceCreateWithBSDName(NULL, vlan_physical_if,
								     kIncludeBondInterfaces);
		SCVLANInterfaceSetPhysicalInterfaceAndTag(vlan, vlan_physical, vlan_tag);
		CFRelease(vlan_physical);

		// set display name (which updates the stored preferences)
		options = CFDictionaryGetValue(vlan_dict, __kVLANInterface_options);
		if (isA_CFDictionary(options)) {
			CFStringRef	vlan_name;

			vlan_name = CFDictionaryGetValue(options, CFSTR("VLAN Name"));
			if (isA_CFString(vlan_name)) {
				SCVLANInterfaceSetLocalizedDisplayName(vlan, vlan_name);
			}
		}

		CFArrayAppendValue(myContext->vlans, vlan);
		CFRelease(vlan);
	}

	CFRelease(prefs);
	return;
}


static SCVLANInterfaceRef
findVLANInterfaceAndTag(SCPreferencesRef prefs, SCNetworkInterfaceRef physical, CFNumberRef tag)
{
	CFIndex			i;
	CFIndex			n;
	SCVLANInterfaceRef	vlan	= NULL;
	CFArrayRef		vlans;

	vlans = SCVLANInterfaceCopyAll(prefs);

	n = CFArrayGetCount(vlans);
	for (i = 0; i < n; i++) {
		SCVLANInterfaceRef	config_vlan;
		SCNetworkInterfaceRef	config_physical;
		CFNumberRef		config_tag;

		config_vlan     = CFArrayGetValueAtIndex(vlans, i);
		config_physical = SCVLANInterfaceGetPhysicalInterface(config_vlan);
		config_tag      = SCVLANInterfaceGetTag(config_vlan);

		if ((config_physical != NULL) && (config_tag != NULL)) {
			if (!CFEqual(physical, config_physical)) {
				// if this VLAN has a different physical interface
				continue;
			}

			if (!CFEqual(tag, config_tag)) {
				// if this VLAN has a different tag
				continue;
			}

			vlan = CFRetain(config_vlan);
			break;
		}
	}
	CFRelease(vlans);

	return vlan;
}


#pragma mark -
#pragma mark SCVLANInterface APIs


CFArrayRef
SCVLANInterfaceCopyAll(SCPreferencesRef prefs)
{
	addContext		context;
	CFDictionaryRef		dict;
	CFStringRef		path;

	context.vlans = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	context.prefs = prefs;

	path = CFStringCreateWithFormat(NULL,
					NULL,
					CFSTR("/%@/%@"),
					kSCPrefVirtualNetworkInterfaces,
					kSCNetworkInterfaceTypeVLAN);
	dict = SCPreferencesPathGetValue(prefs, path);
	if (isA_CFDictionary(dict)) {
		CFDictionaryApplyFunction(dict, add_configured_interface, &context);
	} else {
		// no VLAN configuration, upgrade from legacy configuration
		dict = CFDictionaryCreate(NULL,
					  NULL, NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
		(void) SCPreferencesPathSetValue(prefs, path, dict);
		CFRelease(dict);

		add_legacy_configuration(&context);
	}
	CFRelease(path);

	return context.vlans;
}


static void
addAvailableInterfaces(CFMutableArrayRef available, CFArrayRef interfaces,
		       CFSetRef exclude)
{
	CFIndex	i;
	CFIndex	n;

	n = CFArrayGetCount(interfaces);
	for (i = 0; i < n; i++) {
		SCNetworkInterfaceRef		interface;
		SCNetworkInterfacePrivateRef	interfacePrivate;

		interface = CFArrayGetValueAtIndex(interfaces, i);
		interfacePrivate = (SCNetworkInterfacePrivateRef)interface;

		if (exclude != NULL
		    && CFSetContainsValue(exclude, interface)) {
			// exclude this interface
			continue;
		}
		if (interfacePrivate->supportsVLAN) {
			// if this interface is available
			CFArrayAppendValue(available, interface);
		}
	}

	return;
}


CFArrayRef
SCVLANInterfaceCopyAvailablePhysicalInterfaces()
{
	CFMutableArrayRef	available;
	CFArrayRef		bond_interfaces = NULL;
	CFMutableSetRef		exclude = NULL;
	CFArrayRef		interfaces;
	SCPreferencesRef	prefs;

	available = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	prefs = SCPreferencesCreate(NULL, CFSTR("SCVLANInterfaceCopyAvailablePhysicalInterfaces"), NULL);
	if (prefs != NULL) {
		bond_interfaces = SCBondInterfaceCopyAll(prefs);
		CFRelease(prefs);
		if (bond_interfaces != NULL) {
			exclude = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
			__SCBondInterfaceListCopyMembers(bond_interfaces, exclude);
		}
	}

	// add real interfaces that aren't part of a bond
	interfaces = __SCNetworkInterfaceCopyAll_IONetworkInterface();
	if (interfaces != NULL) {
		addAvailableInterfaces(available, interfaces, exclude);
		CFRelease(interfaces);
	}

	// add bond interfaces
	if (bond_interfaces != NULL) {
		addAvailableInterfaces(available, bond_interfaces, NULL);
		CFRelease(bond_interfaces);
	}
	if (exclude != NULL) {
		CFRelease(exclude);
	}

	return available;
}


CFArrayRef
_SCVLANInterfaceCopyActive(void)
{
	struct ifaddrs		*ifap;
	struct ifaddrs		*ifp;
	int			s;
	CFMutableArrayRef	vlans	= NULL;

	if (getifaddrs(&ifap) == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("getifaddrs() failed: %s"), strerror(errno));
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	s = inet_dgram_socket();
	if (s == -1) {
		_SCErrorSet(errno);
		goto done;
	}

	vlans = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	for (ifp = ifap; ifp != NULL; ifp = ifp->ifa_next) {
		struct if_data		*if_data;
		struct ifreq		ifr;
		SCVLANInterfaceRef	vlan;
		CFStringRef		vlan_if;
		SCNetworkInterfaceRef	vlan_physical;
		CFStringRef		vlan_physical_if;
		CFNumberRef		vlan_tag;
		char			vlr_parent[IFNAMSIZ + 1];
		int			vlr_tag;
		struct vlanreq		vreq;

		if_data = (struct if_data *)ifp->ifa_data;
		if (if_data == NULL
		    || ifp->ifa_addr->sa_family != AF_LINK
		    || if_data->ifi_type != IFT_L2VLAN) {
			continue;
		}

		bzero(&ifr, sizeof(ifr));
		bzero(&vreq, sizeof(vreq));
		strncpy(ifr.ifr_name, ifp->ifa_name, sizeof(ifr.ifr_name));
		ifr.ifr_data = (caddr_t)&vreq;

		if (ioctl(s, SIOCGIFVLAN, (caddr_t)&ifr) == -1) {
			SCLog(TRUE, LOG_ERR, CFSTR("ioctl() failed: %s"), strerror(errno));
			CFRelease(vlans);
			vlans = NULL;
			_SCErrorSet(kSCStatusFailed);
			goto done;
		}

		// create the VLAN interface
		vlan_if = CFStringCreateWithCString(NULL, ifp->ifa_name, kCFStringEncodingASCII);
		vlan    = (SCVLANInterfaceRef)_SCVLANInterfaceCreatePrivate(NULL, vlan_if);
		CFRelease(vlan_if);

		// set the physical interface and tag
		bzero(&vlr_parent, sizeof(vlr_parent));
		bcopy(vreq.vlr_parent, vlr_parent, IFNAMSIZ);
		vlan_physical_if = CFStringCreateWithCString(NULL, vlr_parent, kCFStringEncodingASCII);
		vlan_physical = _SCNetworkInterfaceCreateWithBSDName(NULL, vlan_physical_if,
								     kIncludeBondInterfaces);
		CFRelease(vlan_physical_if);

		vlr_tag  = vreq.vlr_tag;
		vlan_tag = CFNumberCreate(NULL, kCFNumberIntType, &vlr_tag);

		SCVLANInterfaceSetPhysicalInterfaceAndTag(vlan, vlan_physical, vlan_tag);
		CFRelease(vlan_physical);
		CFRelease(vlan_tag);

		// add VLAN
		CFArrayAppendValue(vlans, vlan);
		CFRelease(vlan);
	}

    done :

	(void) close(s);
	freeifaddrs(ifap);
	return vlans;
}


SCVLANInterfaceRef
SCVLANInterfaceCreate(SCPreferencesRef prefs, SCNetworkInterfaceRef physical, CFNumberRef tag)
{
	CFAllocatorRef			allocator;
	CFIndex				i;
	SCNetworkInterfacePrivateRef	interfacePrivate;
	SCVLANInterfaceRef		vlan;

	if (prefs == NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (!isA_SCNetworkInterface(physical)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	interfacePrivate = (SCNetworkInterfacePrivateRef)physical;
	if (!interfacePrivate->supportsVLAN) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (isA_CFNumber(tag)) {
		int	tag_val;

		CFNumberGetValue(tag, kCFNumberIntType, &tag_val);
		if ((tag_val < 1) || (tag_val > 4094)) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return NULL;
		}
	} else {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	// make sure that physical interface and tag are not used
	vlan = findVLANInterfaceAndTag(prefs, physical, tag);
	if (vlan != NULL) {
		CFRelease(vlan);
		_SCErrorSet(kSCStatusKeyExists);
		return NULL;
	}

	allocator = CFGetAllocator(prefs);

	// create a new VLAN using an unused interface name
	for (i = 0; vlan == NULL; i++) {
		CFDictionaryRef			dict;
		CFStringRef			vlan_if;
		Boolean				ok;
		CFStringRef			path;

		vlan_if = CFStringCreateWithFormat(allocator, NULL, CFSTR("vlan%d"), i);
		path    = CFStringCreateWithFormat(allocator,
						   NULL,
						   CFSTR("/%@/%@/%@"),
						   kSCPrefVirtualNetworkInterfaces,
						   kSCNetworkInterfaceTypeVLAN,
						   vlan_if);
		dict = SCPreferencesPathGetValue(prefs, path);
		if (dict != NULL) {
			// if VLAN interface name not available
			CFRelease(path);
			CFRelease(vlan_if);
			continue;
		}

		// add the VLAN to the stored preferences
		dict = CFDictionaryCreate(allocator,
					  NULL, NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
		ok = SCPreferencesPathSetValue(prefs, path, dict);
		CFRelease(dict);
		CFRelease(path);
		if (!ok) {
			// if the VLAN could not be saved
			CFRelease(vlan_if);
			_SCErrorSet(kSCStatusFailed);
			break;
		}

		// create the SCVLANInterfaceRef
		vlan = (SCVLANInterfaceRef)_SCVLANInterfaceCreatePrivate(allocator, vlan_if);
		CFRelease(vlan_if);

		// estabish link to the stored configuration
		interfacePrivate = (SCNetworkInterfacePrivateRef)vlan;
		interfacePrivate->prefs = CFRetain(prefs);

		// set physical interface and tag
		SCVLANInterfaceSetPhysicalInterfaceAndTag(vlan, physical, tag);
	}

	return vlan;
}


Boolean
SCVLANInterfaceRemove(SCVLANInterfaceRef vlan)
{
	CFStringRef			vlan_if;
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)vlan;
	Boolean				ok;
	CFStringRef			path;

	if (!isA_SCVLANInterface(vlan)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (interfacePrivate->prefs == NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	vlan_if = SCNetworkInterfaceGetBSDName(vlan);
	path    = CFStringCreateWithFormat(NULL,
					   NULL,
					   CFSTR("/%@/%@/%@"),
					   kSCPrefVirtualNetworkInterfaces,
					   kSCNetworkInterfaceTypeVLAN,
					   vlan_if);
	ok = SCPreferencesPathRemoveValue(interfacePrivate->prefs, path);
	CFRelease(path);

	return ok;
}


SCNetworkInterfaceRef
SCVLANInterfaceGetPhysicalInterface(SCVLANInterfaceRef vlan)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)vlan;

	if (!isA_SCVLANInterface(vlan)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	return interfacePrivate->vlan.interface;
}


CFNumberRef
SCVLANInterfaceGetTag(SCVLANInterfaceRef vlan)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)vlan;

	if (!isA_SCVLANInterface(vlan)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	return interfacePrivate->vlan.tag;
}


CFDictionaryRef
SCVLANInterfaceGetOptions(SCVLANInterfaceRef vlan)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)vlan;

	if (!isA_SCVLANInterface(vlan)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	return interfacePrivate->vlan.options;
}


Boolean
SCVLANInterfaceSetPhysicalInterfaceAndTag(SCVLANInterfaceRef vlan, SCNetworkInterfaceRef physical, CFNumberRef tag)
{
	SCNetworkInterfacePrivateRef	interfacePrivate;
	Boolean				ok			= TRUE;

	if (!isA_SCVLANInterface(vlan)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!isA_SCNetworkInterface(physical)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	interfacePrivate = (SCNetworkInterfacePrivateRef)physical;
	if (!interfacePrivate->supportsVLAN) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (isA_CFNumber(tag)) {
		int	tag_val;

		CFNumberGetValue(tag, kCFNumberIntType, &tag_val);
		if ((tag_val < 1) || (tag_val > 4094)) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return FALSE;
		}
	} else {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	interfacePrivate = (SCNetworkInterfacePrivateRef)vlan;
	if (interfacePrivate->prefs != NULL) {
		SCVLANInterfaceRef	config_vlan;
		CFDictionaryRef		dict;
		CFMutableDictionaryRef	newDict;
		CFStringRef		path;

		// make sure that physical interface and tag are not used
		config_vlan = findVLANInterfaceAndTag(interfacePrivate->prefs, physical, tag);
		if (config_vlan != NULL) {
			if (!CFEqual(vlan, config_vlan)) {
				CFRelease(config_vlan);
				_SCErrorSet(kSCStatusKeyExists);
				return FALSE;
			}
			CFRelease(config_vlan);
		}

		// set interface/tag in the stored preferences
		path = CFStringCreateWithFormat(NULL,
						NULL,
						CFSTR("/%@/%@/%@"),
						kSCPrefVirtualNetworkInterfaces,
						kSCNetworkInterfaceTypeVLAN,
						interfacePrivate->entity_device);
		dict = SCPreferencesPathGetValue(interfacePrivate->prefs, path);
		if (!isA_CFDictionary(dict)) {
			// if the prefs are confused
			CFRelease(path);
			_SCErrorSet(kSCStatusFailed);
			return FALSE;
		}

		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
		CFDictionarySetValue(newDict,
				     kSCPropVirtualNetworkInterfacesVLANInterface,
				     SCNetworkInterfaceGetBSDName(physical));
		CFDictionarySetValue(newDict, kSCPropVirtualNetworkInterfacesVLANTag, tag);
		ok = SCPreferencesPathSetValue(interfacePrivate->prefs, path, newDict);
		CFRelease(newDict);
		CFRelease(path);
	}

	if (ok) {
		// set physical interface
		if (interfacePrivate->vlan.interface != NULL)
			CFRelease(interfacePrivate->vlan.interface);
		interfacePrivate->vlan.interface = CFRetain(physical);

		// set tag
		if (interfacePrivate->vlan.tag != NULL)
			CFRelease(interfacePrivate->vlan.tag);
		interfacePrivate->vlan.tag = CFRetain(tag);
	}

	return ok;
}


Boolean
SCVLANInterfaceSetLocalizedDisplayName(SCVLANInterfaceRef vlan, CFStringRef newName)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)vlan;
	Boolean				ok			= TRUE;

	if (!isA_SCVLANInterface(vlan)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if ((newName != NULL) && !isA_CFString(newName)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	// set name in the stored preferences
	if (interfacePrivate->prefs != NULL) {
		CFDictionaryRef		dict;
		CFMutableDictionaryRef	newDict;
		CFStringRef		path;

		path = CFStringCreateWithFormat(NULL,
						NULL,
						CFSTR("/%@/%@/%@"),
						kSCPrefVirtualNetworkInterfaces,
						kSCNetworkInterfaceTypeVLAN,
						interfacePrivate->entity_device);
		dict = SCPreferencesPathGetValue(interfacePrivate->prefs, path);
		if (!isA_CFDictionary(dict)) {
			// if the prefs are confused
			CFRelease(path);
			_SCErrorSet(kSCStatusFailed);
			return FALSE;
		}

		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
		if (newName != NULL) {
			CFDictionarySetValue(newDict, kSCPropUserDefinedName, newName);
		} else {
			CFDictionaryRemoveValue(newDict, kSCPropUserDefinedName);
		}
		ok = SCPreferencesPathSetValue(interfacePrivate->prefs, path, newDict);
		CFRelease(newDict);
		CFRelease(path);
	}

	// set name in the SCVLANInterfaceRef
	if (ok) {
		if (interfacePrivate->localized_name != NULL) {
			CFRelease(interfacePrivate->localized_name);
			interfacePrivate->localized_name = NULL;
		}
		if (newName != NULL) {
			interfacePrivate->localized_name = CFStringCreateCopy(NULL, newName);
		}
	}

	return ok;
}


Boolean
SCVLANInterfaceSetOptions(SCVLANInterfaceRef vlan, CFDictionaryRef newOptions)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)vlan;
	Boolean				ok			= TRUE;

	if (!isA_SCVLANInterface(vlan)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if ((newOptions != NULL) && !isA_CFDictionary(newOptions)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	// set options in the stored preferences
	if (interfacePrivate->prefs != NULL) {
		CFDictionaryRef		dict;
		CFMutableDictionaryRef	newDict;
		CFStringRef		path;

		path = CFStringCreateWithFormat(NULL,
						NULL,
						CFSTR("/%@/%@/%@"),
						kSCPrefVirtualNetworkInterfaces,
						kSCNetworkInterfaceTypeVLAN,
						interfacePrivate->entity_device);
		dict = SCPreferencesPathGetValue(interfacePrivate->prefs, path);
		if (!isA_CFDictionary(dict)) {
			// if the prefs are confused
			CFRelease(path);
			_SCErrorSet(kSCStatusFailed);
			return FALSE;
		}

		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
		if (newOptions != NULL) {
			CFDictionarySetValue(newDict, kSCPropVirtualNetworkInterfacesVLANOptions, newOptions);
		} else {
			CFDictionaryRemoveValue(newDict, kSCPropVirtualNetworkInterfacesVLANOptions);
		}
		ok = SCPreferencesPathSetValue(interfacePrivate->prefs, path, newDict);
		CFRelease(newDict);
		CFRelease(path);
	}

	// set options in the SCVLANInterfaceRef
	if (ok) {
		if (interfacePrivate->vlan.options != NULL) {
			CFRelease(interfacePrivate->vlan.options);
			interfacePrivate->vlan.options = NULL;
		}
		if (newOptions != NULL) {
			interfacePrivate->vlan.options = CFDictionaryCreateCopy(NULL, newOptions);
		}
	}

	return ok;
}


#pragma mark -
#pragma mark SCVLANInterface management


static Boolean
__vlan_set(int s, CFStringRef interface_if, CFStringRef physical_if, CFNumberRef tag)
{
	struct ifreq	ifr;
	int		tag_val;
	struct vlanreq	vreq;

	bzero(&ifr, sizeof(ifr));
	bzero(&vreq, sizeof(vreq));

	// interface
	(void) _SC_cfstring_to_cstring(interface_if,
				       ifr.ifr_name,
				       sizeof(ifr.ifr_name),
				       kCFStringEncodingASCII);
	ifr.ifr_data = (caddr_t)&vreq;

	// physical interface
	(void) _SC_cfstring_to_cstring(physical_if,
				       vreq.vlr_parent,
				       sizeof(vreq.vlr_parent),
				       kCFStringEncodingASCII);

	// tag
	CFNumberGetValue(tag, kCFNumberIntType, &tag_val);
	vreq.vlr_tag = tag_val;

	// update physical interface and tag
	if (ioctl(s, SIOCSIFVLAN, (caddr_t)&ifr) == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("ioctl(SIOCSIFVLAN) failed: %s"), strerror(errno));
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	return TRUE;
}


static Boolean
__vlan_clear(int s, CFStringRef interface_if)
{
	struct ifreq	ifr;
	struct vlanreq	vreq;

	bzero(&ifr, sizeof(ifr));
	bzero(&vreq, sizeof(vreq));

	// interface
	(void) _SC_cfstring_to_cstring(interface_if,
				       ifr.ifr_name,
				       sizeof(ifr.ifr_name),
				       kCFStringEncodingASCII);
	ifr.ifr_data = (caddr_t)&vreq;

	// clear physical interface
	bzero(&vreq.vlr_parent, sizeof(vreq.vlr_parent));

	// clear tag
	vreq.vlr_tag = 0;

	// update physical interface and tag
	if (ioctl(s, SIOCSIFVLAN, (caddr_t)&ifr) == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("ioctl(SIOCSIFVLAN) failed: %s"), strerror(errno));
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	return TRUE;
}


Boolean
_SCVLANInterfaceUpdateConfiguration(SCPreferencesRef prefs)
{
	CFArrayRef			active		= NULL;
	CFArrayRef			config		= NULL;
	CFMutableDictionaryRef		devices		= NULL;
	CFIndex				i;
	CFIndex				nActive;
	CFIndex				nConfig;
	Boolean				ok		= TRUE;
	int				s		= -1;

	if (prefs == NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	/* configured VLANs */
	config = SCVLANInterfaceCopyAll(prefs);
	nConfig = CFArrayGetCount(config);

	/* physical interfaces */
	devices = CFDictionaryCreateMutable(NULL,
					    0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);

	/* active VLANs */
	active  = _SCVLANInterfaceCopyActive();
	nActive = CFArrayGetCount(active);

	/* remove any no-longer-configured VLAN interfaces */
	for (i = 0; i < nActive; i++) {
		SCVLANInterfaceRef	a_vlan;
		CFStringRef		a_vlan_if;
		CFIndex			j;
		Boolean			found	= FALSE;

		a_vlan    = CFArrayGetValueAtIndex(active, i);
		a_vlan_if = SCNetworkInterfaceGetBSDName(a_vlan);

		for (j = 0; j < nConfig; j++) {
			SCVLANInterfaceRef	c_vlan;
			CFStringRef		c_vlan_if;

			c_vlan    = CFArrayGetValueAtIndex(config, j);
			c_vlan_if = SCNetworkInterfaceGetBSDName(c_vlan);

			if (CFEqual(a_vlan_if, c_vlan_if)) {
				found = TRUE;
				break;
			}
		}

		if (!found) {
			// remove VLAN interface
			if (s == -1) {
				s = inet_dgram_socket();
				if (s == -1) {
					_SCErrorSet(errno);
					ok = FALSE;
					goto done;
				}
			}
			if (!__destroyInterface(s, a_vlan_if)) {
				ok = FALSE;
				_SCErrorSet(errno);
			}
		}
	}

	/* create (and update) configured VLAN interfaces */
	for (i = 0; i < nConfig; i++) {
		SCVLANInterfaceRef	c_vlan;
		CFStringRef		c_vlan_if;
		SCNetworkInterfaceRef	c_vlan_physical;
		Boolean			found		= FALSE;
		CFIndex			j;
		CFBooleanRef		supported;

		c_vlan          = CFArrayGetValueAtIndex(config, i);
		c_vlan_if       = SCNetworkInterfaceGetBSDName(c_vlan);
		c_vlan_physical = SCVLANInterfaceGetPhysicalInterface(c_vlan);

		if (c_vlan_physical == NULL) {
			continue;
		}
		// determine if the physical interface supports VLANs
		supported = CFDictionaryGetValue(devices, c_vlan_physical);
		if (supported == NULL) {
			SCNetworkInterfacePrivateRef	c_vlan_physicalPrivate	= (SCNetworkInterfacePrivateRef)c_vlan_physical;

			supported = c_vlan_physicalPrivate->supportsVLAN ? kCFBooleanTrue
									 : kCFBooleanFalse;
			CFDictionaryAddValue(devices, c_vlan_physical, supported);
		}

		for (j = 0; j < nActive; j++) {
			SCVLANInterfaceRef	a_vlan;
			CFStringRef		a_vlan_if;

			a_vlan    = CFArrayGetValueAtIndex(active, j);
			a_vlan_if = SCNetworkInterfaceGetBSDName(a_vlan);

			if (CFEqual(c_vlan_if, a_vlan_if)) {
				if (!CFEqual(c_vlan, a_vlan)) {
					// update VLAN interface
					if (s == -1) {
						s = inet_dgram_socket();
						if (s == -1) {
							_SCErrorSet(errno);
							ok = FALSE;
							goto done;
						}
					}

					if (!CFBooleanGetValue(supported)
					    || !__vlan_clear(s, c_vlan_if)
					    || !__vlan_set(s, c_vlan_if,
							   SCNetworkInterfaceGetBSDName(c_vlan_physical),
							   SCVLANInterfaceGetTag(c_vlan))) {
						// something went wrong, try to blow the VLAN away
						if (!CFBooleanGetValue(supported)) {
							_SCErrorSet(kSCStatusFailed);
						}
						(void)__destroyInterface(s, c_vlan_if);
						ok = FALSE;
					}
				}

				found = TRUE;
				break;
			}
		}

		if (!found && CFBooleanGetValue(supported)) {
			// if the physical interface supports VLANs, add new interface
			Boolean		created;

			if (s == -1) {
				s = inet_dgram_socket();
				if (s == -1) {
					_SCErrorSet(errno);
					ok = FALSE;
					goto done;
				}
			}

			created = __createInterface(s, c_vlan_if);
			if (!created
			    || !__vlan_set(s,
					   c_vlan_if,
					   SCNetworkInterfaceGetBSDName(c_vlan_physical),
					   SCVLANInterfaceGetTag(c_vlan))) {
				if (created) {
					// something went wrong, try to blow the VLAN away
					(void)__destroyInterface(s, c_vlan_if);
				} else {
					_SCErrorSet(errno);
				}
				ok = FALSE;
			}
		}

	}

    done :

	if (active)	CFRelease(active);
	if (config)	CFRelease(config);
	if (devices)	CFRelease(devices);
	if (s != -1)	(void) close(s);

	return ok;
}


#pragma mark -
#pragma mark Deprecated SPIs (remove when no longer referenced)


/* ---------- VLAN "device" ---------- */

Boolean
IsVLANSupported(CFStringRef device)
{
	return __SCNetworkInterfaceSupportsVLAN(device);
}

/* ---------- VLANInterface ---------- */

typedef struct {

	/* base CFType information */
	CFRuntimeBase			cfBase;

	/* vlan interface configuration */
	CFStringRef			ifname;		// e.g. vlan0, vlan1, ...
	CFStringRef			device;		// e.g. en0, en1, ...
	CFNumberRef			tag;		// e.g. 1 <= tag <= 4094
	CFDictionaryRef			options;	// e.g. UserDefinedName

} VLANInterfacePrivate, * VLANInterfacePrivateRef;


static CFStringRef	__VLANInterfaceCopyDescription	(CFTypeRef cf);
static void		__VLANInterfaceDeallocate	(CFTypeRef cf);
static Boolean		__VLANInterfaceEqual		(CFTypeRef cf1, CFTypeRef cf2);


static const CFRuntimeClass __VLANInterfaceClass = {
	0,					// version
	"VLANInterface",			// className
	NULL,					// init
	NULL,					// copy
	__VLANInterfaceDeallocate,		// dealloc
	__VLANInterfaceEqual,			// equal
	NULL,					// hash
	NULL,					// copyFormattingDesc
	__VLANInterfaceCopyDescription		// copyDebugDesc
};


static CFTypeID		__kVLANInterfaceTypeID	= _kCFRuntimeNotATypeID;


static pthread_once_t	vlanInterface_init	= PTHREAD_ONCE_INIT;


static CFStringRef
__VLANInterfaceCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef		allocator	= CFGetAllocator(cf);
	CFMutableStringRef	result;
	VLANInterfacePrivateRef	vlanPrivate	= (VLANInterfacePrivateRef)cf;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<VLANInterface %p [%p]> {"), cf, allocator);
	CFStringAppendFormat(result, NULL, CFSTR("if = %@"), vlanPrivate->ifname);
	CFStringAppendFormat(result, NULL, CFSTR(", device = %@"), vlanPrivate->device);
	CFStringAppendFormat(result, NULL, CFSTR(", tag = %@"), vlanPrivate->tag);
	if (vlanPrivate->options != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", options = %@"), vlanPrivate->options);
	}
	CFStringAppendFormat(result, NULL, CFSTR("}"));

	return result;
}


static void
__VLANInterfaceDeallocate(CFTypeRef cf)
{
	VLANInterfacePrivateRef	vlanPrivate	= (VLANInterfacePrivateRef)cf;

	/* release resources */

	CFRelease(vlanPrivate->ifname);
	CFRelease(vlanPrivate->device);
	CFRelease(vlanPrivate->tag);
	if (vlanPrivate->options)	CFRelease(vlanPrivate->options);

	return;
}


static Boolean
__VLANInterfaceEquiv(CFTypeRef cf1, CFTypeRef cf2)
{
	VLANInterfacePrivateRef	vlan1	= (VLANInterfacePrivateRef)cf1;
	VLANInterfacePrivateRef	vlan2	= (VLANInterfacePrivateRef)cf2;

	if (vlan1 == vlan2)
		return TRUE;

	if (!CFEqual(vlan1->ifname, vlan2->ifname))
		return FALSE;	// if not the same interface

	if (!CFEqual(vlan1->device, vlan2->device))
		return FALSE;	// if not the same device

	if (!CFEqual(vlan1->tag, vlan2->tag))
		return FALSE;	// if not the same tag

	return TRUE;
}


static Boolean
__VLANInterfaceEqual(CFTypeRef cf1, CFTypeRef cf2)
{
	VLANInterfacePrivateRef	vlan1	= (VLANInterfacePrivateRef)cf1;
	VLANInterfacePrivateRef	vlan2	= (VLANInterfacePrivateRef)cf2;

	if (!__VLANInterfaceEquiv(vlan1, vlan2))
		return FALSE;	// if not the same VLAN interface/device/tag

	if (vlan1->options != vlan2->options) {
		// if the options may differ
		if ((vlan1->options != NULL) && (vlan2->options != NULL)) {
			// if both VLANs have options
			if (!CFEqual(vlan1->options, vlan2->options)) {
				// if the options are not equal
				return FALSE;
			}
		} else {
			// if only one VLAN has options
			return FALSE;
		}
	}

	return TRUE;
}


static void
__VLANInterfaceInitialize(void)
{
	__kVLANInterfaceTypeID = _CFRuntimeRegisterClass(&__VLANInterfaceClass);
	return;
}


static __inline__ CFTypeRef
isA_VLANInterface(CFTypeRef obj)
{
	return (isA_CFType(obj, VLANInterfaceGetTypeID()));
}


CFTypeID
VLANInterfaceGetTypeID(void)
{
	pthread_once(&vlanInterface_init, __VLANInterfaceInitialize);	/* initialize runtime */
	return __kVLANInterfaceTypeID;
}


static VLANInterfaceRef
__VLANInterfaceCreatePrivate(CFAllocatorRef	allocator,
			     CFStringRef	ifname,
			     CFStringRef	device,
			     CFNumberRef	tag,
			     CFDictionaryRef	options)
{
	VLANInterfacePrivateRef		vlanPrivate;
	uint32_t			size;

	/* initialize runtime */
	pthread_once(&vlanInterface_init, __VLANInterfaceInitialize);

	/* allocate vlan */
	size        = sizeof(VLANInterfacePrivate) - sizeof(CFRuntimeBase);
	vlanPrivate = (VLANInterfacePrivateRef)_CFRuntimeCreateInstance(allocator,
									__kVLANInterfaceTypeID,
									size,
									NULL);
	if (!vlanPrivate) {
		return NULL;
	}

	/* establish the vlan */

	vlanPrivate->ifname  = CFStringCreateCopy(allocator, ifname);
	vlanPrivate->device  = CFStringCreateCopy(allocator, device);
	vlanPrivate->tag     = CFRetain(tag);
	if (options != NULL) {
		vlanPrivate->options = CFDictionaryCreateCopy(allocator, options);
	} else {
		vlanPrivate->options = NULL;
	}

	return (VLANInterfaceRef)vlanPrivate;
}


CFStringRef
VLANInterfaceGetInterface(VLANInterfaceRef vlan)
{
	VLANInterfacePrivateRef	vlanPrivate	= (VLANInterfacePrivateRef)vlan;
	CFStringRef		vlan_if		= NULL;

	if (isA_VLANInterface(vlan)) {
		vlan_if = vlanPrivate->ifname;
	}

	return vlan_if;
}


CFStringRef
VLANInterfaceGetDevice(VLANInterfaceRef	vlan)
{
	VLANInterfacePrivateRef	vlanPrivate	= (VLANInterfacePrivateRef)vlan;
	CFStringRef		vlan_device	= NULL;

	if (isA_VLANInterface(vlan)) {
		vlan_device = vlanPrivate->device;
	}

	return vlan_device;
}


static void
VLANInterfaceSetDevice(VLANInterfaceRef	vlan, CFStringRef newDevice)
{
	VLANInterfacePrivateRef	vlanPrivate	= (VLANInterfacePrivateRef)vlan;

	if (isA_VLANInterface(vlan)) {
		CFAllocatorRef	allocator	= CFGetAllocator(vlan);

		CFRelease(vlanPrivate->device);
		vlanPrivate->device = CFStringCreateCopy(allocator, newDevice);
	}

	return;
}


CFNumberRef
VLANInterfaceGetTag(VLANInterfaceRef vlan)
{
	VLANInterfacePrivateRef	vlanPrivate	= (VLANInterfacePrivateRef)vlan;
	CFNumberRef		vlan_tag	= NULL;

	if (isA_VLANInterface(vlan)) {
		vlan_tag = vlanPrivate->tag;
	}

	return vlan_tag;
}


static void
VLANInterfaceSetTag(VLANInterfaceRef vlan, CFNumberRef newTag)
{
	VLANInterfacePrivateRef	vlanPrivate	= (VLANInterfacePrivateRef)vlan;

	if (isA_VLANInterface(vlan)) {
		CFRelease(vlanPrivate->tag);
		vlanPrivate->tag = CFRetain(newTag);
	}

	return;
}


CFDictionaryRef
VLANInterfaceGetOptions(VLANInterfaceRef vlan)
{
	VLANInterfacePrivateRef	vlanPrivate	= (VLANInterfacePrivateRef)vlan;
	CFDictionaryRef		vlan_options	= NULL;

	if (isA_VLANInterface(vlan)) {
		vlan_options = vlanPrivate->options;
	}

	return vlan_options;
}


static void
VLANInterfaceSetOptions(VLANInterfaceRef vlan, CFDictionaryRef newOptions)
{
	VLANInterfacePrivateRef	vlanPrivate	= (VLANInterfacePrivateRef)vlan;

	if (isA_VLANInterface(vlan)) {
		CFAllocatorRef	allocator	= CFGetAllocator(vlan);

		if (vlanPrivate->options)	CFRelease(vlanPrivate->options);
		if (newOptions != NULL) {
			vlanPrivate->options = CFDictionaryCreateCopy(allocator, newOptions);
		} else {
			vlanPrivate->options = NULL;
		}
	}

	return;
}


/* ---------- VLANPreferences ---------- */

typedef struct {

	/* base CFType information */
	CFRuntimeBase			cfBase;

	/* lock */
	pthread_mutex_t			lock;

	/* underlying preferences */
	SCPreferencesRef		prefs;

	/* base VLANs (before any commits) */
	CFArrayRef			vlBase;

} VLANPreferencesPrivate, * VLANPreferencesPrivateRef;


static CFStringRef	__VLANPreferencesCopyDescription	(CFTypeRef cf);
static void		__VLANPreferencesDeallocate		(CFTypeRef cf);


static const CFRuntimeClass __VLANPreferencesClass = {
	0,					// version
	"VLANPreferences",			// className
	NULL,					// init
	NULL,					// copy
	__VLANPreferencesDeallocate,		// dealloc
	NULL,					// equal
	NULL,					// hash
	NULL,					// copyFormattingDesc
	__VLANPreferencesCopyDescription	// copyDebugDesc
};


static CFTypeID		__kVLANPreferencesTypeID	= _kCFRuntimeNotATypeID;


static pthread_once_t	vlanPreferences_init		= PTHREAD_ONCE_INIT;


static CFStringRef
__VLANPreferencesCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef			allocator	= CFGetAllocator(cf);
	CFIndex				i;
	CFArrayRef			keys;
	CFIndex				n;
	VLANPreferencesPrivateRef	prefsPrivate	= (VLANPreferencesPrivateRef)cf;
	CFMutableStringRef		result;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<VLANPreferences %p [%p]> {"), cf, allocator);

	keys = SCPreferencesCopyKeyList(prefsPrivate->prefs);
	n = CFArrayGetCount(keys);
	for (i = 0; i < n; i++) {
		CFStringRef		key;
		CFPropertyListRef	val;

		key = CFArrayGetValueAtIndex(keys, i);
		val = SCPreferencesGetValue(prefsPrivate->prefs, key);

		CFStringAppendFormat(result, NULL, CFSTR("%@ : %@"), key, val);
	}
	CFRelease(keys);

	CFStringAppendFormat(result, NULL, CFSTR(" }"));

	return result;
}


#define N_QUICK	8


static void
__VLANPreferencesDeallocate(CFTypeRef cf)
{
	VLANPreferencesPrivateRef	prefsPrivate	= (VLANPreferencesPrivateRef)cf;

	/* release resources */

	pthread_mutex_destroy(&prefsPrivate->lock);

	if (prefsPrivate->prefs)	CFRelease(prefsPrivate->prefs);
	if (prefsPrivate->vlBase)	CFRelease(prefsPrivate->vlBase);

	return;
}


static void
__VLANPreferencesInitialize(void)
{
	__kVLANPreferencesTypeID = _CFRuntimeRegisterClass(&__VLANPreferencesClass);
	return;
}


static __inline__ CFTypeRef
isA_VLANPreferences(CFTypeRef obj)
{
	return (isA_CFType(obj, VLANPreferencesGetTypeID()));
}


CFArrayRef
_VLANPreferencesCopyActiveInterfaces()
{
	CFArrayCallBacks	callbacks;
	struct ifaddrs		*ifap;
	struct ifaddrs		*ifp;
	int			s;
	CFMutableArrayRef	vlans	= NULL;

	if (getifaddrs(&ifap) == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("getifaddrs() failed: %s"), strerror(errno));
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	s = inet_dgram_socket();
	if (s == -1) {
		_SCErrorSet(errno);
		goto done;
	}

	callbacks = kCFTypeArrayCallBacks;
	callbacks.equal = __VLANInterfaceEquiv;
	vlans = CFArrayCreateMutable(NULL, 0, &callbacks);

	for (ifp = ifap; ifp != NULL; ifp = ifp->ifa_next) {
		switch (ifp->ifa_addr->sa_family) {
			case AF_LINK : {
				CFStringRef		device;
				struct if_data		*if_data;
				struct ifreq		ifr;
				CFNumberRef		tag;
				VLANInterfaceRef	vlan;
				CFStringRef		vlan_if;
				char			vlr_parent[IFNAMSIZ + 1];
				int			vlr_tag;
				struct vlanreq		vreq;

				if_data = (struct if_data *)ifp->ifa_data;
				if (if_data == NULL) {
					break;	// if no interface data
				}

				if (if_data->ifi_type != IFT_L2VLAN) {
					break;	// if not VLAN
				}

				bzero(&ifr, sizeof(ifr));
				bzero(&vreq, sizeof(vreq));
				strncpy(ifr.ifr_name, ifp->ifa_name, sizeof(ifr.ifr_name));
				ifr.ifr_data = (caddr_t)&vreq;

				if (ioctl(s, SIOCGIFVLAN, (caddr_t)&ifr) == -1) {
					SCLog(TRUE, LOG_ERR, CFSTR("ioctl() failed: %s"), strerror(errno));
					CFRelease(vlans);
					vlans = NULL;
					_SCErrorSet(kSCStatusFailed);
					goto done;
				}
				vlr_tag = vreq.vlr_tag;
				bzero(&vlr_parent, sizeof(vlr_parent));
				bcopy(vreq.vlr_parent, vlr_parent, IFNAMSIZ);

				vlan_if = CFStringCreateWithCString(NULL, ifp->ifa_name, kCFStringEncodingASCII);
				device  = CFStringCreateWithCString(NULL, vlr_parent, kCFStringEncodingASCII);
				tag     = CFNumberCreate(NULL, kCFNumberIntType, &vlr_tag);
				vlan    = __VLANInterfaceCreatePrivate(NULL, vlan_if, device, tag, NULL);
				CFArrayAppendValue(vlans, vlan);
				CFRelease(vlan_if);
				CFRelease(device);
				CFRelease(tag);
				CFRelease(vlan);
				break;
			}

			default :
				break;
		}
	}

    done :

	(void) close(s);
	freeifaddrs(ifap);
	return vlans;
}


static CFIndex
findVLAN(CFArrayRef vlans, CFStringRef device, CFNumberRef tag)
{
	CFIndex	found	= kCFNotFound;
	CFIndex	i;
	CFIndex	n;

	n = isA_CFArray(vlans) ? CFArrayGetCount(vlans) : 0;
	for (i = 0; i < n; i++) {
		CFDictionaryRef	vlan_dict;
		CFStringRef	vlan_device;
		CFStringRef	vlan_if;
		CFNumberRef	vlan_tag;

		vlan_dict = CFArrayGetValueAtIndex(vlans, i);
		if (!isA_CFDictionary(vlan_dict)) {
			continue;	// if the prefs are confused
		}

		vlan_if = CFDictionaryGetValue(vlan_dict, __kVLANInterface_interface);
		if (!isA_CFString(vlan_if)) {
			continue;	// if the prefs are confused
		}

		vlan_device = CFDictionaryGetValue(vlan_dict, __kVLANInterface_device);
		if (isA_CFString(vlan_device)) {
			if (!CFEqual(device, vlan_device)) {
				continue;	// if not a match
			}
		}

		vlan_tag = CFDictionaryGetValue(vlan_dict, __kVLANInterface_tag);
		if (isA_CFNumber(vlan_tag)) {
			if (!CFEqual(tag, vlan_tag)) {
				continue;	// if not a match
			}
		}

		// if we have found a match
		found = i;
		break;
	}

	return found;
}


static void
setConfigurationChanged(VLANPreferencesRef prefs)
{
	VLANPreferencesPrivateRef	prefsPrivate	= (VLANPreferencesPrivateRef)prefs;

	/*
	 * to facilitate device configuration we will take
	 * a snapshot of the VLAN preferences before any
	 * changes are made.  Then, when the changes are
	 * applied we can compare what we had to what we
	 * want and configured the system accordingly.
	 */
	if (prefsPrivate->vlBase == NULL) {
		prefsPrivate->vlBase = VLANPreferencesCopyInterfaces(prefs);
	}

	return;
}


CFTypeID
VLANPreferencesGetTypeID(void)
{
	pthread_once(&vlanPreferences_init, __VLANPreferencesInitialize);	/* initialize runtime */
	return __kVLANPreferencesTypeID;
}


VLANPreferencesRef
VLANPreferencesCreate(CFAllocatorRef allocator)
{
	CFBundleRef			bundle;
	CFStringRef			bundleID	= NULL;
	CFStringRef			name		= CFSTR("VLANConfiguration");
	VLANPreferencesPrivateRef	prefsPrivate;
	uint32_t			size;

	/* initialize runtime */
	pthread_once(&vlanPreferences_init, __VLANPreferencesInitialize);

	/* allocate preferences */
	size         = sizeof(VLANPreferencesPrivate) - sizeof(CFRuntimeBase);
	prefsPrivate = (VLANPreferencesPrivateRef)_CFRuntimeCreateInstance(allocator,
									   __kVLANPreferencesTypeID,
									   size,
									   NULL);
	if (prefsPrivate == NULL) {
		return NULL;
	}

	/* establish the prefs */

	pthread_mutex_init(&prefsPrivate->lock, NULL);

	bundle = CFBundleGetMainBundle();
	if (bundle) {
		bundleID = CFBundleGetIdentifier(bundle);
		if (bundleID) {
			CFRetain(bundleID);
		} else {
			CFURLRef	url;

			url = CFBundleCopyExecutableURL(bundle);
			if (url) {
				bundleID = CFURLCopyPath(url);
				CFRelease(url);
			}
		}
	}

	if (bundleID) {
		CFStringRef     fullName;

		if (CFEqual(bundleID, CFSTR("/"))) {
			CFRelease(bundleID);
			bundleID = CFStringCreateWithFormat(allocator, NULL, CFSTR("(%d)"), getpid());
		}

		fullName = CFStringCreateWithFormat(allocator, NULL, CFSTR("%@:%@"), bundleID, name);
		name = fullName;
		CFRelease(bundleID);
	} else {
		CFRetain(name);
	}

	prefsPrivate->prefs = SCPreferencesCreate(allocator, name, VLAN_PREFERENCES_ID);
	CFRelease(name);

	prefsPrivate->vlBase = NULL;

	return (VLANPreferencesRef)prefsPrivate;
}


CFArrayRef
VLANPreferencesCopyInterfaces(VLANPreferencesRef prefs)
{
	CFAllocatorRef			allocator;
	CFArrayCallBacks		callbacks;
	CFIndex				i;
	CFIndex				n;
	VLANPreferencesPrivateRef	prefsPrivate	= (VLANPreferencesPrivateRef)prefs;
	CFMutableArrayRef		result;
	CFArrayRef			vlans;

	if (!isA_VLANPreferences(prefs)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	allocator = CFGetAllocator(prefs);
	callbacks = kCFTypeArrayCallBacks;
	callbacks.equal = __VLANInterfaceEquiv;
	result = CFArrayCreateMutable(allocator, 0, &callbacks);

	vlans = SCPreferencesGetValue(prefsPrivate->prefs, VLAN_PREFERENCES_VLANS);
	n = isA_CFArray(vlans) ? CFArrayGetCount(vlans) : 0;
	for (i = 0; i < n; i++) {
		CFDictionaryRef		vlan_dict;
		CFStringRef		device;
		CFDictionaryRef		options;
		CFNumberRef		tag;
		VLANInterfaceRef	vlan;
		CFStringRef		vlan_if;

		vlan_dict = CFArrayGetValueAtIndex(vlans, i);
		if (!isA_CFDictionary(vlan_dict)) {
			continue;	// if the prefs are confused
		}

		vlan_if = CFDictionaryGetValue(vlan_dict, __kVLANInterface_interface);
		if (!isA_CFString(vlan_if)) {
			continue;	// if the prefs are confused
		}


		device = CFDictionaryGetValue(vlan_dict, __kVLANInterface_device);
		if (!isA_CFString(device)) {
			continue;	// if the prefs are confused
		}

		tag = CFDictionaryGetValue(vlan_dict, __kVLANInterface_tag);
		if (!isA_CFNumber(tag)) {
			continue;	// if the prefs are confused
		}

		options = CFDictionaryGetValue(vlan_dict, __kVLANInterface_options);
		if ((options != NULL) && !isA_CFDictionary(options)) {
			continue;	// if the prefs are confused
		}

		vlan = __VLANInterfaceCreatePrivate(allocator, vlan_if, device, tag, options);
		CFArrayAppendValue(result, vlan);
		CFRelease(vlan);
	}

	return result;
}


VLANInterfaceRef
VLANPreferencesAddInterface(VLANPreferencesRef	prefs,
			    CFStringRef		device,
			    CFNumberRef		tag,
			    CFDictionaryRef	options)
{
	CFArrayRef			active_vlans;
	CFAllocatorRef			allocator;
	CFArrayRef			config_vlans;
	CFIndex				dup_if;
	CFIndex				i;
	CFIndex				nActive;
	CFIndex				nConfig;
	VLANInterfaceRef		newVlan		= NULL;
	VLANPreferencesPrivateRef	prefsPrivate	= (VLANPreferencesPrivateRef)prefs;

	if (!isA_VLANPreferences(prefs)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (!isA_CFString(device)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (isA_CFNumber(tag)) {
		int	tag_val;

		CFNumberGetValue(tag, kCFNumberIntType, &tag_val);
		if ((tag_val < 1) || (tag_val > 4094)) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return NULL;
		}
	} else {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if ((options != NULL) && !isA_CFDictionary(options)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	pthread_mutex_lock(&prefsPrivate->lock);

	/* get "configured" VLANs (and check to ensure we are not creating a duplicate) */
	config_vlans = SCPreferencesGetValue(prefsPrivate->prefs, VLAN_PREFERENCES_VLANS);
	nConfig      = isA_CFArray(config_vlans) ? CFArrayGetCount(config_vlans) : 0;

	dup_if = findVLAN(config_vlans, device, tag);
	if (dup_if != kCFNotFound) {
		// sorry, you can't add a vlan using the same device/tag */
		_SCErrorSet(kSCStatusKeyExists);
		goto done;
	}

	/* get "active" VLANs */
	active_vlans = _VLANPreferencesCopyActiveInterfaces();
	nActive      = isA_CFArray(active_vlans) ? CFArrayGetCount(active_vlans) : 0;

	/* create a new vlan using an unused interface name */
	allocator = CFGetAllocator(prefs);

	for (i = 0; newVlan == NULL; i++) {
		CFIndex			j;
		CFMutableDictionaryRef	newDict;
		CFMutableArrayRef	newVlans;
		CFStringRef		vlan_if;

		vlan_if = CFStringCreateWithFormat(allocator, NULL, CFSTR("vlan%d"), i);

		for (j = 0; j < nActive; j++) {
			CFStringRef		active_if;
			VLANInterfaceRef	active_vlan;

			active_vlan = CFArrayGetValueAtIndex(active_vlans, j);
			active_if   = VLANInterfaceGetInterface(active_vlan);

			if (CFEqual(vlan_if, active_if)) {
				goto next_if;	// if VLAN interface name not available
			}
		}

		for (j = 0; j < nConfig; j++) {
			CFDictionaryRef	config;
			CFStringRef	config_if;

			config = CFArrayGetValueAtIndex(config_vlans, j);
			if (!isA_CFDictionary(config)) {
				continue;	// if the prefs are confused
			}

			config_if = CFDictionaryGetValue(config, __kVLANInterface_interface);
			if (!isA_CFString(config_if)) {
				continue;	// if the prefs are confused
			}

			if (CFEqual(vlan_if, config_if)) {
				goto next_if;	// if VLAN interface name not available
			}
		}

		/* create the vlan */

		newDict = CFDictionaryCreateMutable(allocator,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
		CFDictionaryAddValue(newDict, __kVLANInterface_interface, vlan_if);
		CFDictionaryAddValue(newDict, __kVLANInterface_device,    device);
		CFDictionaryAddValue(newDict, __kVLANInterface_tag,       tag);
		if (options != NULL) {
			CFDictionaryAddValue(newDict, __kVLANInterface_options, options);
		}

		/* create the accessor handle to be returned */

		newVlan = __VLANInterfaceCreatePrivate(allocator, vlan_if, device, tag, options);

		/* yes, we're going to be changing the configuration */
		setConfigurationChanged(prefs);

		/* save in the prefs */

		if (nConfig == 0) {
			newVlans = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);
		} else {
			newVlans = CFArrayCreateMutableCopy(allocator, 0, config_vlans);
		}
		CFArrayAppendValue(newVlans, newDict);
		CFRelease(newDict);

		(void) SCPreferencesSetValue(prefsPrivate->prefs, VLAN_PREFERENCES_VLANS, newVlans);
		CFRelease(newVlans);

	    next_if :
		CFRelease(vlan_if);
	}

	CFRelease(active_vlans);

    done :

	pthread_mutex_unlock(&prefsPrivate->lock);

	return (VLANInterfaceRef) newVlan;
}


Boolean
VLANPreferencesUpdateInterface(VLANPreferencesRef	prefs,
			       VLANInterfaceRef		vlan,
			       CFStringRef		newDevice,
			       CFNumberRef		newTag,
			       CFDictionaryRef		newOptions)
{
	CFAllocatorRef			allocator;
	CFIndex				cur_if;
	CFIndex				dup_if;
	CFMutableDictionaryRef		newDict;
	CFMutableArrayRef		newVlans;
	Boolean				ok		= FALSE;
	VLANPreferencesPrivateRef	prefsPrivate	= (VLANPreferencesPrivateRef)prefs;
	CFArrayRef			vlans;
	CFStringRef			vlan_if;

	if (!isA_VLANPreferences(prefs)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!isA_VLANInterface(vlan)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if ((newDevice != NULL) && !isA_CFString(newDevice)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (newTag != NULL) {
		if (isA_CFNumber(newTag)) {
			int	tag_val;

			CFNumberGetValue(newTag, kCFNumberIntType, &tag_val);
			if ((tag_val < 1) || (tag_val > 4094)) {
				_SCErrorSet(kSCStatusInvalidArgument);
				return FALSE;
			}
		} else {
			_SCErrorSet(kSCStatusInvalidArgument);
			return FALSE;
		}
	}

	if ((newOptions != NULL)
	    && !isA_CFDictionary(newOptions) && (newOptions != (CFDictionaryRef)kCFNull)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	pthread_mutex_lock(&prefsPrivate->lock);

	vlan_if = VLANInterfaceGetInterface(vlan);

	vlans = SCPreferencesGetValue(prefsPrivate->prefs, VLAN_PREFERENCES_VLANS);
	if (!isA_CFArray(vlans)) {
		goto done;	// if the prefs are confused
	}

	cur_if = findVLAN(vlans,
			  VLANInterfaceGetDevice(vlan),
			  VLANInterfaceGetTag   (vlan));
	if (cur_if == kCFNotFound) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}

	dup_if = findVLAN(vlans,
			  newDevice != NULL ? newDevice : VLANInterfaceGetDevice(vlan),
			  newTag    != NULL ? newTag    : VLANInterfaceGetTag   (vlan));
	if (dup_if != kCFNotFound) {
		// if the same device/tag has already been defined
		if (cur_if != dup_if) {
			/*
			 * sorry, you can't update another vlan that is using
			 * the same device/tag
			 */
			_SCErrorSet(kSCStatusKeyExists);
			goto done;
		}
	}

	/* update the vlan */

	if (newDevice != NULL) {
		VLANInterfaceSetDevice(vlan, newDevice);
	} else {
		newDevice = VLANInterfaceGetDevice(vlan);
	}

	if (newTag != NULL) {
		VLANInterfaceSetTag(vlan, newTag);
	} else {
		newTag = VLANInterfaceGetTag(vlan);
	}

	if (newOptions != NULL) {
		if (newOptions != (CFDictionaryRef)kCFNull) {
			VLANInterfaceSetOptions(vlan, newOptions);
		} else {
			VLANInterfaceSetOptions(vlan, NULL);
			newOptions = NULL;
		}
	} else {
		newOptions = VLANInterfaceGetOptions(vlan);
	}

	/* update the prefs */

	allocator = CFGetAllocator(prefs);
	newDict = CFDictionaryCreateMutable(allocator,
					    0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	CFDictionaryAddValue(newDict, __kVLANInterface_interface, vlan_if);
	CFDictionaryAddValue(newDict, __kVLANInterface_device,    newDevice);
	CFDictionaryAddValue(newDict, __kVLANInterface_tag,       newTag);
	if (newOptions != NULL) {
		CFDictionaryAddValue(newDict, __kVLANInterface_options, newOptions);
	}

	/* yes, we're going to be changing the configuration */
	setConfigurationChanged(prefs);

	/* update the prefs */

	newVlans = CFArrayCreateMutableCopy(allocator, 0, vlans);
	CFArraySetValueAtIndex(newVlans, cur_if, newDict);
	CFRelease(newDict);

	(void) SCPreferencesSetValue(prefsPrivate->prefs, VLAN_PREFERENCES_VLANS, newVlans);
	CFRelease(newVlans);

	ok = TRUE;

    done :

	pthread_mutex_unlock(&prefsPrivate->lock);

	return ok;
}


Boolean
VLANPreferencesRemoveInterface(VLANPreferencesRef	prefs,
			       VLANInterfaceRef		vlan)
{
	CFAllocatorRef			allocator;
	CFIndex				cur_if;
	CFMutableArrayRef		newVlans;
	Boolean				ok		= FALSE;
	VLANPreferencesPrivateRef	prefsPrivate	= (VLANPreferencesPrivateRef)prefs;
	CFArrayRef			vlans;

	if (!isA_VLANPreferences(prefs)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!isA_VLANInterface(vlan)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	pthread_mutex_lock(&prefsPrivate->lock);

	vlans = SCPreferencesGetValue(prefsPrivate->prefs, VLAN_PREFERENCES_VLANS);
	if (!isA_CFArray(vlans)) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;	// if the prefs are confused
	}

	cur_if = findVLAN(vlans,
			  VLANInterfaceGetDevice(vlan),
			  VLANInterfaceGetTag   (vlan));
	if (cur_if == kCFNotFound) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}

	/* yes, we're going to be changing the configuration */
	setConfigurationChanged(prefs);

	/* remove the vlan */

	allocator = CFGetAllocator(prefs);
	newVlans = CFArrayCreateMutableCopy(allocator, 0, vlans);
	CFArrayRemoveValueAtIndex(newVlans, cur_if);

	(void) SCPreferencesSetValue(prefsPrivate->prefs, VLAN_PREFERENCES_VLANS, newVlans);
	CFRelease(newVlans);

	ok = TRUE;

    done :

	pthread_mutex_unlock(&prefsPrivate->lock);

	return ok;
}


Boolean
VLANPreferencesCommitChanges(VLANPreferencesRef	prefs)
{
	Boolean				ok		= FALSE;
	VLANPreferencesPrivateRef	prefsPrivate	= (VLANPreferencesPrivateRef)prefs;

	if (!isA_VLANPreferences(prefs)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	ok = SCPreferencesCommitChanges(prefsPrivate->prefs);
	if (!ok) {
		return ok;
	}

	if (prefsPrivate->vlBase != NULL)  {
		CFRelease(prefsPrivate->vlBase);
		prefsPrivate->vlBase = NULL;
	}

	return TRUE;
}


Boolean
_VLANPreferencesUpdateConfiguration(VLANPreferencesRef prefs)
{
	return TRUE;
}


Boolean
VLANPreferencesApplyChanges(VLANPreferencesRef prefs)
{
	SCPreferencesRef		defaultPrefs;
	Boolean				ok		= FALSE;
	VLANPreferencesPrivateRef	prefsPrivate	= (VLANPreferencesPrivateRef)prefs;

	if (!isA_VLANPreferences(prefs)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	pthread_mutex_lock(&prefsPrivate->lock);

	/* apply the preferences */
	ok = SCPreferencesApplyChanges(prefsPrivate->prefs);
	if (!ok) {
		goto done;
	}

	/* apply the VLAN configuration */
	defaultPrefs = SCPreferencesCreate(NULL, CFSTR("VLANPreferencesApplyChanges"), NULL);
	{
		/*
		 * Note: In an ideal world, we'd simply call SCPreferencesApplyChanges()
		 *       Unfortunately, it's possible that the caller (e.g NetworkCfgTool)
		 *       is holding the lock on the default prefs and since "Apply" attempts
		 *       to grab the lock we could end up in a deadlock situation.
		 */
#include "SCPreferencesInternal.h"
		SCPreferencesPrivateRef		defaultPrefsPrivate;

		defaultPrefsPrivate = (SCPreferencesPrivateRef)defaultPrefs;

		pthread_mutex_lock(&defaultPrefsPrivate->lock);
		if (defaultPrefsPrivate->session == NULL) {
			__SCPreferencesAddSession(defaultPrefs);
		}
		pthread_mutex_unlock(&defaultPrefsPrivate->lock);

		ok = SCDynamicStoreNotifyValue(defaultPrefsPrivate->session, defaultPrefsPrivate->sessionKeyApply);
	}
	CFRelease(defaultPrefs);
	if (!ok) {
		goto done;
	}

    done :

	pthread_mutex_unlock(&prefsPrivate->lock);

	return ok;
}
