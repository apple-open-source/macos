/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * November 14, 2003		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCNetworkConfigurationInternal.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

#include <ifaddrs.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/ethernet.h>
#define	KERNEL_PRIVATE
#include <net/if.h>
#include <net/if_var.h>
#undef	KERNEL_PRIVATE
#include <net/if_vlan_var.h>
#include <net/if_types.h>
#include <net/route.h>

#include <SystemConfiguration/VLANConfiguration.h>
#include <SystemConfiguration/VLANConfigurationPrivate.h>

/* ---------- VLAN support ---------- */

static int
inet_dgram_socket()
{
	int	s;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("socket() failed: %s"), strerror(errno));
		_SCErrorSet(kSCStatusFailed);
	}

	return s;
}


static Boolean
_VLANDevice_set(int s, CFStringRef interface, CFStringRef device, CFNumberRef tag)
{
	struct ifreq	ifr;
	int		tag_val;
	struct vlanreq	vreq;

	bzero(&ifr, sizeof(ifr));
	bzero(&vreq, sizeof(vreq));

	// interface
	(void) _SC_cfstring_to_cstring(interface,
				       ifr.ifr_name,
				       sizeof(ifr.ifr_name),
				       kCFStringEncodingASCII);
	ifr.ifr_data = (caddr_t)&vreq;

	// parent device
	(void) _SC_cfstring_to_cstring(device,
				       vreq.vlr_parent,
				       sizeof(vreq.vlr_parent),
				       kCFStringEncodingASCII);

	// tag
	CFNumberGetValue(tag, kCFNumberIntType, &tag_val);
	vreq.vlr_tag = tag_val;

	// update parent device and tag
	if (ioctl(s, SIOCSIFVLAN, (caddr_t)&ifr) == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("ioctl(SIOCSIFVLAN) failed: %s"), strerror(errno));
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	// mark the parent device "up"
	if (!__markInterfaceUp(s, device)) {
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	return TRUE;
}


static Boolean
_VLANDevice_unset(int s, CFStringRef interface)
{
	struct ifreq	ifr;
	struct vlanreq	vreq;

	bzero(&ifr, sizeof(ifr));
	bzero(&vreq, sizeof(vreq));

	// interface
	(void) _SC_cfstring_to_cstring(interface,
				       ifr.ifr_name,
				       sizeof(ifr.ifr_name),
				       kCFStringEncodingASCII);
	ifr.ifr_data = (caddr_t)&vreq;

	// clear parent device
	bzero(&vreq.vlr_parent, sizeof(vreq.vlr_parent));

	// clear tag
	vreq.vlr_tag = 0;

	// update parent device and tag
	if (ioctl(s, SIOCSIFVLAN, (caddr_t)&ifr) == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("ioctl(SIOCSIFVLAN) failed: %s"), strerror(errno));
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	return TRUE;
}


/* ---------- VLAN "device" ---------- */

Boolean
IsVLANSupported(CFStringRef device)
{
	char *			buf	= NULL;
	size_t			buf_len	= 0;
	struct if_msghdr *	ifm;
	char *			if_name	= NULL;
	unsigned int		if_index;
	Boolean			isVlan	= FALSE;
	int			mib[6];

	/* get the interface index */

	if_name = _SC_cfstring_to_cstring(device, NULL, 0, kCFStringEncodingASCII);
	if (if_name == NULL) {
		return FALSE;	// if conversion error
	}
	if_index = if_nametoindex(if_name);
	if (if_index == 0) {
		goto done;	// if unknown interface
	}

	/* get information for the specified device */

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_LINK;
	mib[4] = NET_RT_IFLIST;
	mib[5] = if_index;	/* ask for exactly one interface */

	if (sysctl(mib, 6, NULL, &buf_len, NULL, 0) < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("sysctl() size failed: %s"), strerror(errno));
		goto done;
	}
	buf = CFAllocatorAllocate(NULL, buf_len, 0);
	if (sysctl(mib, 6, buf, &buf_len, NULL, 0) < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("sysctl() failed: %s"), strerror(errno));
		goto done;
	}

	/* check the link type and hwassist flags */

	ifm = (struct if_msghdr *)buf;
	switch (ifm->ifm_type) {
		case RTM_IFINFO : {
#if	defined(IF_HWASSIST_VLAN_TAGGING) && defined(IF_HWASSIST_VLAN_MTU)
			struct if_data	*if_data = &ifm->ifm_data;

			if (if_data->ifi_hwassist & (IF_HWASSIST_VLAN_TAGGING | IF_HWASSIST_VLAN_MTU)) {
				isVlan = TRUE;
			}
#endif
			break;
		}
	}

    done :

	if (if_name != NULL)	CFAllocatorDeallocate(NULL, if_name);
	if (buf != NULL)	CFAllocatorDeallocate(NULL, buf);

	return isVlan;
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
	CFStringAppendFormat(result, NULL, CFSTR(" if = %@"), vlanPrivate->ifname);
	CFStringAppendFormat(result, NULL, CFSTR(", device = %@"), vlanPrivate->device);
	CFStringAppendFormat(result, NULL, CFSTR(", tag = %@"), vlanPrivate->tag);
	if (vlanPrivate->options != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", options = %@"), vlanPrivate->options);
	}
	CFStringAppendFormat(result, NULL, CFSTR(" }"));

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

#define	VLAN_PREFERENCES_VLANS		CFSTR("VLANs")

#define	__kVLANInterface_interface	CFSTR("interface")	// e.g. vlan0, vlan1, ...
#define	__kVLANInterface_device		CFSTR("device")		// e.g. en0, en1, ...
#define __kVLANInterface_tag		CFSTR("tag")		// e.g. 1 <= tag <= 4094
#define __kVLANInterface_options	CFSTR("options")	// e.g. UserDefinedName

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
		SCLog(TRUE, LOG_ERR, CFSTR("socket() failed: %s"), strerror(errno));
		_SCErrorSet(kSCStatusFailed);
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
				char			vlr_parent[IFNAMSIZ+1];
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
					_SCErrorSet(kSCStatusFailed);
					CFRelease(vlans);
					goto done;
				}
				vlr_tag = vreq.vlr_tag;
				strlcpy(vlr_parent, vreq.vlr_parent, sizeof(vlr_parent));

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
	CFArrayRef			active		= NULL;
	CFArrayRef			config		= NULL;
	CFMutableDictionaryRef		devices		= NULL;
	CFIndex				i;
	CFIndex				nActive;
	CFIndex				nConfig;
	Boolean				ok		= FALSE;
	VLANPreferencesPrivateRef	prefsPrivate	= (VLANPreferencesPrivateRef)prefs;
	int				s		= -1;

	if (!isA_VLANPreferences(prefs)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	/* configured VLANs */
	if (prefsPrivate->vlBase != NULL) {
		/*
		 * updated VLAN preferences have not been committed
		 * so we ignore any in-progress changes and apply the
		 * saved preferences.
		 */
		config = CFRetain(prefsPrivate->vlBase);
	} else {
		/*
		 * apply the saved preferences
		 */
		config = VLANPreferencesCopyInterfaces(prefs);
	}
	nConfig = CFArrayGetCount(config);

	/* [parent] devices */
	devices = CFDictionaryCreateMutable(NULL,
					    0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);

	/* active VLANs */
	active  = _VLANPreferencesCopyActiveInterfaces();
	nActive = CFArrayGetCount(active);

	/* remove any no-longer-configured VLAN interfaces */
	for (i = 0; i < nActive; i++) {
		VLANInterfaceRef	a_vlan;
		CFStringRef		a_vlan_if;
		CFIndex			j;
		Boolean			found	= FALSE;

		a_vlan    = CFArrayGetValueAtIndex(active, i);
		a_vlan_if = VLANInterfaceGetInterface(a_vlan);

		for (j = 0; j < nConfig; j++) {
			VLANInterfaceRef	c_vlan;
			CFStringRef		c_vlan_if;

			c_vlan    = CFArrayGetValueAtIndex(config, j);
			c_vlan_if = VLANInterfaceGetInterface(c_vlan);

			if (CFEqual(a_vlan_if, c_vlan_if)) {
				found = TRUE;
				break;
			}
		}

		if (!found) {
			// remove VLAN interface
			if (s == -1) {
				s = inet_dgram_socket();
			}

			ok = __destroyInterface(s, a_vlan_if);
			if (!ok) {
				_SCErrorSet(kSCStatusFailed);
				goto done;
			}
		}
	}

	/* create (and update) configured VLAN interfaces */
	for (i = 0; i < nConfig; i++) {
		VLANInterfaceRef	c_vlan;
		CFStringRef		c_vlan_device;
		CFStringRef		c_vlan_if;
		Boolean			found		= FALSE;
		CFIndex			j;
		CFBooleanRef		supported;

		c_vlan        = CFArrayGetValueAtIndex(config, i);
		c_vlan_device = VLANInterfaceGetDevice(c_vlan);
		c_vlan_if     = VLANInterfaceGetInterface(c_vlan);

		// determine if the [parent] device supports VLANs
		supported = CFDictionaryGetValue(devices, c_vlan_device);
		if (supported == NULL) {
			supported = IsVLANSupported(c_vlan_device) ? kCFBooleanTrue
								   : kCFBooleanFalse;
			CFDictionaryAddValue(devices, c_vlan_device, supported);
		}

		for (j = 0; j < nActive; j++) {
			VLANInterfaceRef	a_vlan;
			CFStringRef		a_vlan_if;

			a_vlan    = CFArrayGetValueAtIndex(active, j);
			a_vlan_if = VLANInterfaceGetInterface(a_vlan);

			if (CFEqual(c_vlan_if, a_vlan_if)) {
				if (!__VLANInterfaceEquiv(c_vlan, a_vlan)) {
					// update VLAN interface;
					if (s == -1) {
						s = inet_dgram_socket();
					}

					if (CFBooleanGetValue(supported)) {
						// if the new [parent] device supports VLANs
						ok = _VLANDevice_unset(s, c_vlan_if);
						if (!ok) {
							goto done;
						}

						ok = _VLANDevice_set(s,
								     c_vlan_if,
								     c_vlan_device,
								     VLANInterfaceGetTag(c_vlan));
						if (!ok) {
							goto done;
						}
					} else {
						// if the new [parent] device does not support VLANs
						ok = __destroyInterface(s, c_vlan_if);
						if (!ok) {
							_SCErrorSet(kSCStatusFailed);
							goto done;
						}
					}
				}

				found = TRUE;
				break;
			}
		}

		if (!found && CFBooleanGetValue(supported)) {
			// if the [parent] device supports VLANs, add new interface
			if (s == -1) {
				s = inet_dgram_socket();
			}

			ok = __createInterface(s, c_vlan_if);
			if (!ok) {
				_SCErrorSet(kSCStatusFailed);
				goto done;
			}

			ok = _VLANDevice_set(s,
					     c_vlan_if,
					     c_vlan_device,
					     VLANInterfaceGetTag(c_vlan));
			if (!ok) {
				goto done;
			}
		}

	}

	ok = TRUE;

    done :

	if (active)	CFRelease(active);
	if (config)	CFRelease(config);
	if (devices)	CFRelease(devices);
	if (s != -1)	(void) close(s);

	return ok;
}


Boolean
VLANPreferencesApplyChanges(VLANPreferencesRef prefs)
{
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
	ok = _VLANPreferencesUpdateConfiguration(prefs);
	if (!ok) {
		goto done;
	}

    done :

	pthread_mutex_unlock(&prefsPrivate->lock);

	return ok;
}
