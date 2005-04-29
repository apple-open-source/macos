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
 * July 22, 2004		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCNetworkConfigurationInternal.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

#include <SystemConfiguration/BondConfiguration.h>
#include <SystemConfiguration/BondConfigurationPrivate.h>

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
#include <net/if_bond_var.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/route.h>

/* ---------- Bond support ---------- */

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

static int
siocgifmedia(int s, const char * ifname, int * status, int * active)
{
	struct ifmediareq	ifmr;

	*status = 0;
	*active = 0;
	bzero(&ifmr, sizeof(ifmr));
	strncpy(ifmr.ifm_name, ifname, sizeof(ifmr.ifm_name));
	if (ioctl(s, SIOCGIFMEDIA, &ifmr) < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("SIOCGIFMEDIA(%s) failed, %s\n"),
			ifname, strerror(errno));
		return (-1);
	}
	if (ifmr.ifm_count != 0) {
		*status = ifmr.ifm_status;
		*active = ifmr.ifm_active;
	}
	return (0);
}

static struct if_bond_status_req *
if_bond_status_req_copy(int s, const char * ifname)
{
	void *				buf = NULL;
	struct if_bond_req		ibr;
	struct if_bond_status_req *	ibsr_p;
	struct ifreq			ifr;

	bzero(&ifr, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	bzero((char *)&ibr, sizeof(ibr));
	ibr.ibr_op = IF_BOND_OP_GET_STATUS;
	ibsr_p = &ibr.ibr_ibru.ibru_status;
	ibsr_p->ibsr_version = IF_BOND_STATUS_REQ_VERSION;
	ifr.ifr_data = (caddr_t)&ibr;

	/* how many of them are there? */
	if (ioctl(s, SIOCGIFBOND, (caddr_t)&ifr) < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("SIOCGIFBOND(%s) failed: %s"),
		      ifname, strerror(errno));
		goto failed;
	}
	buf = malloc(sizeof(struct if_bond_status) * ibsr_p->ibsr_total + sizeof(*ibsr_p));
	if (buf == NULL) {
		goto failed;
	}
	if (ibsr_p->ibsr_total == 0) {
		goto done;
	}
	ibsr_p->ibsr_count = ibsr_p->ibsr_total;
	ibsr_p->ibsr_buffer = buf + sizeof(*ibsr_p);

	/* get the list */
	if (ioctl(s, SIOCGIFBOND, (caddr_t)&ifr) < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("SIOCGIFBOND(%s) failed: %s"),
		      ifname, strerror(errno));
		goto failed;
	}
 done:
	(*(struct if_bond_status_req *)buf) = *ibsr_p;
	return ((struct if_bond_status_req *)buf);

 failed:
	if (buf != NULL) {
		free(buf);
	}
	return (NULL);
}

static Boolean
_Bond_addDevice(int s, CFStringRef interface, CFStringRef device)
{
	struct if_bond_req	breq;
	struct ifreq		ifr;

	// bond interface
	bzero(&ifr, sizeof(ifr));
	(void) _SC_cfstring_to_cstring(interface,
				       ifr.ifr_name,
				       sizeof(ifr.ifr_name),
				       kCFStringEncodingASCII);
	ifr.ifr_data = (caddr_t)&breq;

	// new bond member
	bzero(&breq, sizeof(breq));
	breq.ibr_op = IF_BOND_OP_ADD_INTERFACE;
	(void) _SC_cfstring_to_cstring(device,
				       breq.ibr_ibru.ibru_if_name,
				       sizeof(breq.ibr_ibru.ibru_if_name),
				       kCFStringEncodingASCII);

	// add new bond member
	if (ioctl(s, SIOCSIFBOND, (caddr_t)&ifr) == -1) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("could not add interface \"%@\" to bond \"%@\": %s"),
		      device,
		      interface,
		      strerror(errno));
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	// mark the added interface "up"
	if (!__markInterfaceUp(s, device)) {
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	return TRUE;
}


static Boolean
_Bond_removeDevice(int s, CFStringRef interface, CFStringRef device)
{
	struct if_bond_req	breq;
	struct ifreq		ifr;

	// bond interface
	bzero(&ifr, sizeof(ifr));
	(void) _SC_cfstring_to_cstring(interface,
				       ifr.ifr_name,
				       sizeof(ifr.ifr_name),
				       kCFStringEncodingASCII);
	ifr.ifr_data = (caddr_t)&breq;

	// bond member to remove
	bzero(&breq, sizeof(breq));
	breq.ibr_op = IF_BOND_OP_REMOVE_INTERFACE;
	(void) _SC_cfstring_to_cstring(device,
				       breq.ibr_ibru.ibru_if_name,
				       sizeof(breq.ibr_ibru.ibru_if_name),
				       kCFStringEncodingASCII);

	// remove bond member
	if (ioctl(s, SIOCSIFBOND, (caddr_t)&ifr) == -1) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("could not remove interface \"%@\" from bond \"%@\": %s"),
		      device,
		      interface,
		      strerror(errno));
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	return TRUE;
}


/* ---------- Bond "device" ---------- */

Boolean
IsBondSupported(CFStringRef device)
{
	CFMutableDictionaryRef	entity;
	SCNetworkInterfaceRef	interface;
	Boolean			isBond		= FALSE;

	entity = CFDictionaryCreateMutable(NULL,
					   0,
					   &kCFTypeDictionaryKeyCallBacks,
					   &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(entity, kSCPropNetInterfaceType, kSCValNetInterfaceTypeEthernet);
	CFDictionarySetValue(entity, kSCPropNetInterfaceDeviceName, device);
	interface = __SCNetworkInterfaceCreateWithEntity(NULL, entity, NULL);
	CFRelease(entity);

	if (interface != NULL) {
		SCNetworkInterfacePrivateRef	interfacePrivate;

		interfacePrivate = (SCNetworkInterfacePrivateRef)interface;
		if (interfacePrivate->path != NULL) {
			isBond = interfacePrivate->supportsBond;
		}
		CFRelease(interface);
	}

	return isBond;
}

/* ---------- BondInterface ---------- */

typedef struct {

	/* base CFType information */
	CFRuntimeBase			cfBase;

	/* bond interface configuration */
	CFStringRef			ifname;		// e.g. bond0, bond1, ...
	CFArrayRef			devices;	// e.g. en0, en1, ...
	CFDictionaryRef			options;	// e.g. UserDefinedName

} BondInterfacePrivate, * BondInterfacePrivateRef;


static CFStringRef	__BondInterfaceCopyDescription	(CFTypeRef cf);
static void		__BondInterfaceDeallocate	(CFTypeRef cf);
static Boolean		__BondInterfaceEqual		(CFTypeRef cf1, CFTypeRef cf2);


static const CFRuntimeClass __BondInterfaceClass = {
	0,					// version
	"BondInterface",			// className
	NULL,					// init
	NULL,					// copy
	__BondInterfaceDeallocate,		// dealloc
	__BondInterfaceEqual,			// equal
	NULL,					// hash
	NULL,					// copyFormattingDesc
	__BondInterfaceCopyDescription		// copyDebugDesc
};


static CFTypeID		__kBondInterfaceTypeID	= _kCFRuntimeNotATypeID;


static pthread_once_t	bondInterface_init	= PTHREAD_ONCE_INIT;


static CFStringRef
__BondInterfaceCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef		allocator	= CFGetAllocator(cf);
	CFMutableStringRef	result;
	BondInterfacePrivateRef	bondPrivate	= (BondInterfacePrivateRef)cf;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<BondInterface %p [%p]> {"), cf, allocator);
	CFStringAppendFormat(result, NULL, CFSTR(" if = %@"), bondPrivate->ifname);
	if (bondPrivate->devices != NULL) {
		CFIndex	i;
		CFIndex	n;

		CFStringAppendFormat(result, NULL, CFSTR(", devices ="));

		n = CFArrayGetCount(bondPrivate->devices);
		for (i = 0; i < n; i++) {
			CFStringAppendFormat(result,
					     NULL,
					     CFSTR(" %@"),
					     CFArrayGetValueAtIndex(bondPrivate->devices, i));
		}
	}
	if (bondPrivate->options != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", options = %@"), bondPrivate->options);
	}
	CFStringAppendFormat(result, NULL, CFSTR(" }"));

	return result;
}


static void
__BondInterfaceDeallocate(CFTypeRef cf)
{
	BondInterfacePrivateRef	bondPrivate	= (BondInterfacePrivateRef)cf;

	/* release resources */

	CFRelease(bondPrivate->ifname);
	if (bondPrivate->devices)	CFRelease(bondPrivate->devices);
	if (bondPrivate->options)	CFRelease(bondPrivate->options);

	return;
}


static Boolean
__BondInterfaceEquiv(CFTypeRef cf1, CFTypeRef cf2)
{
	BondInterfacePrivateRef	bond1	= (BondInterfacePrivateRef)cf1;
	BondInterfacePrivateRef	bond2	= (BondInterfacePrivateRef)cf2;

	if (bond1 == bond2)
		return TRUE;

	if (!CFEqual(bond1->ifname, bond2->ifname))
		return FALSE;	// if not the same interface

	if (!CFEqual(bond1->devices, bond2->devices))
		return FALSE;	// if not the same device

	return TRUE;
}


static Boolean
__BondInterfaceEqual(CFTypeRef cf1, CFTypeRef cf2)
{
	BondInterfacePrivateRef	bond1	= (BondInterfacePrivateRef)cf1;
	BondInterfacePrivateRef	bond2	= (BondInterfacePrivateRef)cf2;

	if (!__BondInterfaceEquiv(bond1, bond2))
		return FALSE;	// if not the same Bond interface/devices

	if (bond1->options != bond2->options) {
		// if the options may differ
		if ((bond1->options != NULL) && (bond2->options != NULL)) {
			// if both Bonds have options
			if (!CFEqual(bond1->options, bond2->options)) {
				// if the options are not equal
				return FALSE;
			}
		} else {
			// if only one Bond has options
			return FALSE;
		}
	}

	return TRUE;
}


static void
__BondInterfaceInitialize(void)
{
	__kBondInterfaceTypeID = _CFRuntimeRegisterClass(&__BondInterfaceClass);
	return;
}


static __inline__ CFTypeRef
isA_BondInterface(CFTypeRef obj)
{
	return (isA_CFType(obj, BondInterfaceGetTypeID()));
}


CFTypeID
BondInterfaceGetTypeID(void)
{
	pthread_once(&bondInterface_init, __BondInterfaceInitialize);	/* initialize runtime */
	return __kBondInterfaceTypeID;
}


static BondInterfaceRef
__BondInterfaceCreatePrivate(CFAllocatorRef	allocator,
			     CFStringRef	ifname)
{
	BondInterfacePrivateRef		bondPrivate;
	uint32_t			size;

	/* initialize runtime */
	pthread_once(&bondInterface_init, __BondInterfaceInitialize);

	/* allocate bond */
	size        = sizeof(BondInterfacePrivate) - sizeof(CFRuntimeBase);
	bondPrivate = (BondInterfacePrivateRef)_CFRuntimeCreateInstance(allocator,
									__kBondInterfaceTypeID,
									size,
									NULL);
	if (bondPrivate == NULL) {
		return NULL;
	}

	/* establish the bond */

	bondPrivate->ifname  = CFStringCreateCopy(allocator, ifname);
	bondPrivate->devices = NULL;
	bondPrivate->options = NULL;

	return (BondInterfaceRef)bondPrivate;
}


CFStringRef
BondInterfaceGetInterface(BondInterfaceRef bond)
{
	BondInterfacePrivateRef	bondPrivate	= (BondInterfacePrivateRef)bond;
	CFStringRef		bond_if		= NULL;

	if (isA_BondInterface(bond)) {
		bond_if = bondPrivate->ifname;
	}

	return bond_if;
}


CFArrayRef
BondInterfaceGetDevices(BondInterfaceRef bond)
{
	BondInterfacePrivateRef	bondPrivate	= (BondInterfacePrivateRef)bond;
	CFArrayRef		bond_devices	= NULL;

	if (isA_BondInterface(bond)) {
		bond_devices = bondPrivate->devices;
	}

	return bond_devices;
}


CFDictionaryRef
BondInterfaceGetOptions(BondInterfaceRef bond)
{
	BondInterfacePrivateRef	bondPrivate	= (BondInterfacePrivateRef)bond;
	CFDictionaryRef		bond_options	= NULL;

	if (isA_BondInterface(bond)) {
		bond_options = bondPrivate->options;
	}

	return bond_options;
}


static void
BondInterfaceSetDevices(BondInterfaceRef bond, CFArrayRef newDevices)
{
	BondInterfacePrivateRef	bondPrivate	= (BondInterfacePrivateRef)bond;

	if (isA_BondInterface(bond)) {
		CFAllocatorRef	allocator	= CFGetAllocator(bond);

		if (bondPrivate->devices != NULL)	CFRelease(bondPrivate->devices);
		if ((newDevices != NULL) && (CFArrayGetCount(newDevices) > 0)) {
			bondPrivate->devices = CFArrayCreateCopy(allocator, newDevices);
		} else {
			bondPrivate->devices = NULL;
		}
	}

	return;
}


static void
BondInterfaceSetOptions(BondInterfaceRef bond, CFDictionaryRef newOptions)
{
	BondInterfacePrivateRef	bondPrivate	= (BondInterfacePrivateRef)bond;

	if (isA_BondInterface(bond)) {
		CFAllocatorRef	allocator	= CFGetAllocator(bond);

		if (bondPrivate->options)	CFRelease(bondPrivate->options);
		if (newOptions != NULL) {
			bondPrivate->options = CFDictionaryCreateCopy(allocator, newOptions);
		} else {
			bondPrivate->options = NULL;
		}
	}

	return;
}


/* ---------- BondPreferences ---------- */

#define	BOND_PREFERENCES_BONDS		CFSTR("Bonds")

#define	__kBondInterface_interface	CFSTR("interface")	// e.g. bond0, bond1, ...
#define	__kBondInterface_devices	CFSTR("devices")	// e.g. en0, en1, ...
#define __kBondInterface_options	CFSTR("options")	// e.g. UserDefinedName

typedef struct {

	/* base CFType information */
	CFRuntimeBase			cfBase;

	/* lock */
	pthread_mutex_t			lock;

	/* underlying preferences */
	SCPreferencesRef		prefs;

	/* base Bonds (before any commits) */
	CFArrayRef			bBase;

} BondPreferencesPrivate, * BondPreferencesPrivateRef;


static CFStringRef	__BondPreferencesCopyDescription	(CFTypeRef cf);
static void		__BondPreferencesDeallocate		(CFTypeRef cf);


static const CFRuntimeClass __BondPreferencesClass = {
	0,					// version
	"BondPreferences",			// className
	NULL,					// init
	NULL,					// copy
	__BondPreferencesDeallocate,		// dealloc
	NULL,					// equal
	NULL,					// hash
	NULL,					// copyFormattingDesc
	__BondPreferencesCopyDescription	// copyDebugDesc
};


static CFTypeID		__kBondPreferencesTypeID	= _kCFRuntimeNotATypeID;


static pthread_once_t	bondPreferences_init		= PTHREAD_ONCE_INIT;


static CFStringRef
__BondPreferencesCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef			allocator	= CFGetAllocator(cf);
	CFIndex				i;
	CFArrayRef			keys;
	CFIndex				n;
	BondPreferencesPrivateRef	prefsPrivate	= (BondPreferencesPrivateRef)cf;
	CFMutableStringRef		result;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<BondPreferences %p [%p]> {"), cf, allocator);

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


static void
__BondPreferencesDeallocate(CFTypeRef cf)
{
	BondPreferencesPrivateRef	prefsPrivate	= (BondPreferencesPrivateRef)cf;

	/* release resources */

	pthread_mutex_destroy(&prefsPrivate->lock);

	if (prefsPrivate->prefs)	CFRelease(prefsPrivate->prefs);
	if (prefsPrivate->bBase)	CFRelease(prefsPrivate->bBase);

	return;
}


static void
__BondPreferencesInitialize(void)
{
	__kBondPreferencesTypeID = _CFRuntimeRegisterClass(&__BondPreferencesClass);
	return;
}


static __inline__ CFTypeRef
isA_BondPreferences(CFTypeRef obj)
{
	return (isA_CFType(obj, BondPreferencesGetTypeID()));
}


CFArrayRef
_BondPreferencesCopyActiveInterfaces()
{
	CFArrayCallBacks	callbacks;
	struct ifaddrs		*ifap;
	struct ifaddrs		*ifp;
	int			s;
	CFMutableArrayRef	bonds	= NULL;

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
	callbacks.equal = __BondInterfaceEquiv;
	bonds = CFArrayCreateMutable(NULL, 0, &callbacks);

	for (ifp = ifap; ifp != NULL; ifp = ifp->ifa_next) {
		BondInterfaceRef		bond;
		CFStringRef			bond_if;
		CFMutableArrayRef		devices	= NULL;
		struct if_bond_status_req	*ibsr_p;
		struct if_data			*if_data;

		if_data = (struct if_data *)ifp->ifa_data;
		if (if_data == NULL
		    || ifp->ifa_addr->sa_family != AF_LINK
		    || if_data->ifi_type != IFT_IEEE8023ADLAG) {
			continue;
		}
		ibsr_p = if_bond_status_req_copy(s, ifp->ifa_name);
		if (ibsr_p == NULL) {
			SCLog(TRUE, LOG_ERR, CFSTR("if_bond_status_req_copy(%s) failed: %s"),
			      ifp->ifa_name, strerror(errno));
			_SCErrorSet(kSCStatusFailed);
			CFRelease(bonds);
			goto done;
		}
		if (ibsr_p->ibsr_total > 0) {
			int 			i;
			struct if_bond_status *	ibs_p;
			devices = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

			// iterate over each member device
			ibs_p = (struct if_bond_status *)ibsr_p->ibsr_buffer;
			for (i = 0; i < ibsr_p->ibsr_total; i++) {
				CFStringRef	device;
				char		if_name[IFNAMSIZ+1];

				strlcpy(if_name, ibs_p[i].ibs_if_name, sizeof(if_name));
				device = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingASCII);
				CFArrayAppendValue(devices, device);
				CFRelease(device);
			}
		}
		free(ibsr_p);
		bond_if = CFStringCreateWithCString(NULL, ifp->ifa_name, kCFStringEncodingASCII);
		bond    = __BondInterfaceCreatePrivate(NULL, bond_if);
		CFRelease(bond_if);

		if (devices != NULL) {
			BondInterfaceSetDevices(bond, devices);
			CFRelease(devices);
		}
		CFArrayAppendValue(bonds, bond);
		CFRelease(bond);
	}

    done :

	(void) close(s);
	freeifaddrs(ifap);
	return bonds;
}


static CFIndex
findBond(CFArrayRef bonds, CFStringRef interface)
{
	CFIndex	found	= kCFNotFound;
	CFIndex	i;
	CFIndex	n;

	n = isA_CFArray(bonds) ? CFArrayGetCount(bonds) : 0;
	for (i = 0; i < n; i++) {
		CFDictionaryRef	bond_dict;
		CFStringRef	bond_if;

		bond_dict = CFArrayGetValueAtIndex(bonds, i);
		if (!isA_CFDictionary(bond_dict)) {
			break;	// if the prefs are confused
		}

		bond_if = CFDictionaryGetValue(bond_dict, __kBondInterface_interface);
		if (!isA_CFString(bond_if)) {
			break;	// if the prefs are confused
		}

		if (!CFEqual(bond_if, interface)) {
			continue;	// if not a match
		}

		// if we have found a match
		found = i;
		break;
	}

	return found;
}


static void
setConfigurationChanged(BondPreferencesRef prefs)
{
	BondPreferencesPrivateRef	prefsPrivate	= (BondPreferencesPrivateRef)prefs;

	/*
	 * to facilitate device configuration we will take
	 * a snapshot of the Bond preferences before any
	 * changes are made.  Then, when the changes are
	 * applied we can compare what we had to what we
	 * want and configured the system accordingly.
	 */
	if (prefsPrivate->bBase == NULL) {
		prefsPrivate->bBase = BondPreferencesCopyInterfaces(prefs);
	}

	return;
}


CFTypeID
BondPreferencesGetTypeID(void)
{
	pthread_once(&bondPreferences_init, __BondPreferencesInitialize);	/* initialize runtime */
	return __kBondPreferencesTypeID;
}


BondPreferencesRef
BondPreferencesCreate(CFAllocatorRef allocator)
{
	CFBundleRef			bundle;
	CFStringRef			bundleID	= NULL;
	CFStringRef			name		= CFSTR("BondConfiguration");
	BondPreferencesPrivateRef	prefsPrivate;
	uint32_t			size;

	/* initialize runtime */
	pthread_once(&bondPreferences_init, __BondPreferencesInitialize);

	/* allocate preferences */
	size         = sizeof(BondPreferencesPrivate) - sizeof(CFRuntimeBase);
	prefsPrivate = (BondPreferencesPrivateRef)_CFRuntimeCreateInstance(allocator,
									   __kBondPreferencesTypeID,
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

	prefsPrivate->prefs = SCPreferencesCreate(allocator, name, BOND_PREFERENCES_ID);
	CFRelease(name);

	prefsPrivate->bBase = NULL;

	return (BondPreferencesRef)prefsPrivate;
}


CFArrayRef
BondPreferencesCopyInterfaces(BondPreferencesRef prefs)
{
	CFAllocatorRef			allocator;
	CFArrayCallBacks		callbacks;
	CFIndex				i;
	CFIndex				n;
	BondPreferencesPrivateRef	prefsPrivate	= (BondPreferencesPrivateRef)prefs;
	CFMutableArrayRef		result;
	CFArrayRef			bonds;

	if (!isA_BondPreferences(prefs)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	allocator = CFGetAllocator(prefs);
	callbacks = kCFTypeArrayCallBacks;
	callbacks.equal = __BondInterfaceEquiv;
	result = CFArrayCreateMutable(allocator, 0, &callbacks);

	bonds = SCPreferencesGetValue(prefsPrivate->prefs, BOND_PREFERENCES_BONDS);
	if ((bonds != NULL) && !isA_CFArray(bonds)) {
		goto error;	// if the prefs are confused
	}

	n = (bonds != NULL) ? CFArrayGetCount(bonds) : 0;
	for (i = 0; i < n; i++) {
		BondInterfaceRef	bond;
		CFDictionaryRef		bond_dict;
		CFStringRef		bond_if;
		CFArrayRef		devices;
		CFDictionaryRef		options;

		bond_dict = CFArrayGetValueAtIndex(bonds, i);
		if (!isA_CFDictionary(bond_dict)) {
			goto error;	// if the prefs are confused
		}

		bond_if = CFDictionaryGetValue(bond_dict, __kBondInterface_interface);
		if (!isA_CFString(bond_if)) {
			goto error;	// if the prefs are confused
		}


		devices = CFDictionaryGetValue(bond_dict, __kBondInterface_devices);
		if ((devices != NULL) && !isA_CFArray(devices)) {
			goto error;	// if the prefs are confused
		}

		options = CFDictionaryGetValue(bond_dict, __kBondInterface_options);
		if ((options != NULL) && !isA_CFDictionary(options)) {
			goto error;	// if the prefs are confused
		}

		bond = __BondInterfaceCreatePrivate(allocator, bond_if);
		BondInterfaceSetDevices(bond, devices);
		BondInterfaceSetOptions(bond, options);
		CFArrayAppendValue(result, bond);
		CFRelease(bond);
	}

	return result;

    error :

	_SCErrorSet(kSCStatusFailed);
	CFRelease(result);
	return NULL;
}


BondInterfaceRef
BondPreferencesCreateInterface(BondPreferencesRef	prefs)
{
	CFArrayRef			active_bonds	= NULL;
	CFAllocatorRef			allocator;
	CFArrayRef			config_bonds;
	CFIndex				i;
	CFIndex				nActive;
	CFIndex				nConfig;
	BondInterfaceRef		newBond		= NULL;
	BondPreferencesPrivateRef	prefsPrivate	= (BondPreferencesPrivateRef)prefs;

	if (!isA_BondPreferences(prefs)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	pthread_mutex_lock(&prefsPrivate->lock);

	/* get "configured" Bonds (and check to ensure the device is available) */
	config_bonds = SCPreferencesGetValue(prefsPrivate->prefs, BOND_PREFERENCES_BONDS);
	if ((config_bonds != NULL) && !isA_CFArray(config_bonds)) {
		// if the prefs are confused
		_SCErrorSet(kSCStatusFailed);
		goto done;
	}

	nConfig = (config_bonds != NULL) ? CFArrayGetCount(config_bonds) : 0;

	/* get "active" Bonds */
	active_bonds = _BondPreferencesCopyActiveInterfaces();
	nActive      = isA_CFArray(active_bonds) ? CFArrayGetCount(active_bonds) : 0;

	/* create a new bond using an unused interface name */
	allocator = CFGetAllocator(prefs);

	for (i = 0; newBond == NULL; i++) {
		CFIndex			j;
		CFMutableDictionaryRef	newDict;
		CFMutableArrayRef	newBonds;
		CFStringRef		bond_if;

		bond_if = CFStringCreateWithFormat(allocator, NULL, CFSTR("bond%d"), i);

		for (j = 0; j < nActive; j++) {
			CFStringRef		active_if;
			BondInterfaceRef	active_bond;

			active_bond = CFArrayGetValueAtIndex(active_bonds, j);
			active_if   = BondInterfaceGetInterface(active_bond);

			if (CFEqual(bond_if, active_if)) {
				goto next_if;	// if bond interface name not available
			}
		}

		for (j = 0; j < nConfig; j++) {
			CFDictionaryRef	config;
			CFStringRef	config_if;

			config = CFArrayGetValueAtIndex(config_bonds, j);
			if (!isA_CFDictionary(config)) {
				// if the prefs are confused
				_SCErrorSet(kSCStatusFailed);
				CFRelease(bond_if);
				goto done;
			}

			config_if = CFDictionaryGetValue(config, __kBondInterface_interface);
			if (!isA_CFString(config_if)) {
				// if the prefs are confused
				_SCErrorSet(kSCStatusFailed);
				CFRelease(bond_if);
				goto done;
			}

			if (CFEqual(bond_if, config_if)) {
				goto next_if;	// if bond interface name not available
			}
		}

		/* create the bond */

		newDict = CFDictionaryCreateMutable(allocator,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
		CFDictionaryAddValue(newDict, __kBondInterface_interface, bond_if);

		/* create the accessor handle to be returned */

		newBond = __BondInterfaceCreatePrivate(allocator, bond_if);

		/* save in the prefs */

		if (nConfig > 0) {
			newBonds = CFArrayCreateMutableCopy(allocator, 0, config_bonds);
		} else {
			newBonds = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);
		}
		CFArrayAppendValue(newBonds, newDict);
		CFRelease(newDict);

		/* yes, we're going to be changing the configuration */
		setConfigurationChanged(prefs);

		(void) SCPreferencesSetValue(prefsPrivate->prefs, BOND_PREFERENCES_BONDS, newBonds);
		CFRelease(newBonds);

	    next_if :
		CFRelease(bond_if);
	}

    done :

	if (active_bonds != NULL)	CFRelease(active_bonds);

	pthread_mutex_unlock(&prefsPrivate->lock);

	return (BondInterfaceRef) newBond;
}


static Boolean
_BondPreferencesUpdate(BondPreferencesRef prefs, BondInterfaceRef bond)
{
	CFAllocatorRef			allocator;
	CFIndex				bond_index;
	CFArrayRef			devices;
	CFStringRef			interface;
	CFMutableDictionaryRef		newDict;
	CFMutableArrayRef		newBonds;
	CFDictionaryRef			options;
	BondPreferencesPrivateRef	prefsPrivate	= (BondPreferencesPrivateRef)prefs;
	CFArrayRef			bonds;

	bonds = SCPreferencesGetValue(prefsPrivate->prefs, BOND_PREFERENCES_BONDS);
	if ((bonds != NULL) && !isA_CFArray(bonds)) {
		// if the prefs are confused
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	interface = BondInterfaceGetInterface(bond);
	bond_index = findBond(bonds, interface);
	if (bond_index == kCFNotFound) {
		_SCErrorSet(kSCStatusNoKey);
		return FALSE;
	}

	/* create the bond dictionary */

	allocator = CFGetAllocator(prefs);
	newDict = CFDictionaryCreateMutable(allocator,
					    0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	CFDictionaryAddValue(newDict, __kBondInterface_interface, interface);

	devices = BondInterfaceGetDevices(bond);
	if (devices != NULL) {
		CFDictionaryAddValue(newDict, __kBondInterface_devices, devices);
	}

	options = BondInterfaceGetOptions(bond);
	if (options != NULL) {
		CFDictionaryAddValue(newDict, __kBondInterface_options, options);
	}

	/* yes, we're going to be changing the configuration */
	setConfigurationChanged(prefs);

	/* update the prefs */

	newBonds = CFArrayCreateMutableCopy(allocator, 0, bonds);
	CFArraySetValueAtIndex(newBonds, bond_index, newDict);
	CFRelease(newDict);
	(void) SCPreferencesSetValue(prefsPrivate->prefs, BOND_PREFERENCES_BONDS, newBonds);
	CFRelease(newBonds);

	return TRUE;
}


Boolean
BondPreferencesAddDevice(BondPreferencesRef	prefs,
			 BondInterfaceRef	bond,
			 CFStringRef		device)
{
	CFArrayRef			config_bonds;
	CFIndex				i;
	CFIndex				nConfig;
	Boolean				ok		= TRUE;
	BondPreferencesPrivateRef	prefsPrivate	= (BondPreferencesPrivateRef)prefs;

	if (!isA_BondPreferences(prefs)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!isA_BondInterface(bond)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!isA_CFString(device)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!IsBondSupported(device)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	pthread_mutex_lock(&prefsPrivate->lock);

	/* get "configured" bonds */
	config_bonds = SCPreferencesGetValue(prefsPrivate->prefs, BOND_PREFERENCES_BONDS);
	if ((config_bonds != NULL) && !isA_CFArray(config_bonds)) {
		_SCErrorSet(kSCStatusFailed);
		ok = FALSE;
		goto done;
	}

	nConfig = (config_bonds != NULL) ? CFArrayGetCount(config_bonds) : 0;

	/* check to ensure the requested device is available */
	for (i = 0; ok && (i < nConfig); i++) {
		CFDictionaryRef	config_bond;
		CFArrayRef	devices;

		config_bond = CFArrayGetValueAtIndex(config_bonds, i);
		if (!isA_CFDictionary(config_bond)) {
			ok = FALSE;	// if the prefs are confused
			break;
		}

		devices = CFDictionaryGetValue(config_bond, __kBondInterface_devices);
		if ((devices != NULL) && !isA_CFArray(devices)) {
			ok = FALSE;	// if the prefs are confused
			break;
		}

		if (devices == NULL) {
			continue;	// if no devices
		}

		ok = !CFArrayContainsValue(devices,
					   CFRangeMake(0, CFArrayGetCount(devices)),
					   device);
	}

	if (ok) {
		CFArrayRef		devices;
		CFMutableArrayRef	newDevices;

		devices = BondInterfaceGetDevices(bond);
		if (devices != NULL) {
			devices = CFArrayCreateCopy(NULL, devices);
			newDevices = CFArrayCreateMutableCopy(NULL, 0, devices);
		} else {
			newDevices = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		}
		CFArrayAppendValue(newDevices, device);
		BondInterfaceSetDevices(bond, newDevices);
		CFRelease(newDevices);

		ok = _BondPreferencesUpdate(prefs, bond);
		if (!ok) {
			BondInterfaceSetDevices(bond, devices);
		}

		if (devices != NULL) {
			CFRelease(devices);
		}
	} else {
		_SCErrorSet(kSCStatusKeyExists);
	}

    done :

	pthread_mutex_unlock(&prefsPrivate->lock);

	return ok;
}


Boolean
BondPreferencesRemoveDevice(BondPreferencesRef	prefs,
			    BondInterfaceRef	bond,
			    CFStringRef		device)
{
	CFIndex				bond_index;
	CFArrayRef			devices;
	Boolean				ok		= FALSE;
	BondPreferencesPrivateRef	prefsPrivate	= (BondPreferencesPrivateRef)prefs;

	if (!isA_BondPreferences(prefs)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!isA_BondInterface(bond)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!isA_CFString(device)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	pthread_mutex_lock(&prefsPrivate->lock);

	devices = BondInterfaceGetDevices(bond);
	if (devices != NULL) {
		bond_index = CFArrayGetFirstIndexOfValue(devices,
							 CFRangeMake(0, CFArrayGetCount(devices)),
							 device);
		if (bond_index != kCFNotFound) {
			CFMutableArrayRef	newDevices;

			devices = CFArrayCreateCopy(NULL, devices);
			newDevices = CFArrayCreateMutableCopy(NULL, 0, devices);
			CFArrayRemoveValueAtIndex(newDevices, bond_index);
			BondInterfaceSetDevices(bond, newDevices);
			CFRelease(newDevices);

			ok = _BondPreferencesUpdate(prefs, bond);
			if (!ok) {
				BondInterfaceSetDevices(bond, devices);
			}

			CFRelease(devices);
		} else {
			_SCErrorSet(kSCStatusNoKey);
		}
	}

	pthread_mutex_unlock(&prefsPrivate->lock);

	return ok;
}


Boolean
BondPreferencesSetOptions(BondPreferencesRef prefs, BondInterfaceRef bond, CFDictionaryRef newOptions)
{
	Boolean				ok		= FALSE;
	CFDictionaryRef			options;
	BondPreferencesPrivateRef	prefsPrivate	= (BondPreferencesPrivateRef)prefs;

	if (!isA_BondPreferences(prefs)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!isA_BondInterface(bond)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	pthread_mutex_lock(&prefsPrivate->lock);

	options = BondInterfaceGetOptions(bond);
	if (options != NULL) {
		options = CFDictionaryCreateCopy(NULL, options);
	}

	BondInterfaceSetOptions(bond, newOptions);
	ok = _BondPreferencesUpdate(prefs, bond);
	if (!ok) {
		BondInterfaceSetOptions(bond, options);
	}

	if (options != NULL) {
		CFRelease(options);
	}

	pthread_mutex_unlock(&prefsPrivate->lock);

	return ok;
}


Boolean
BondPreferencesRemoveInterface(BondPreferencesRef	prefs,
			       BondInterfaceRef		bond)
{
	CFIndex				bond_index;
	Boolean				ok		= FALSE;
	BondPreferencesPrivateRef	prefsPrivate	= (BondPreferencesPrivateRef)prefs;
	CFArrayRef			bonds;

	if (!isA_BondPreferences(prefs)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!isA_BondInterface(bond)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	pthread_mutex_lock(&prefsPrivate->lock);

	bonds = SCPreferencesGetValue(prefsPrivate->prefs, BOND_PREFERENCES_BONDS);
	if (!isA_CFArray(bonds)) {
		// if the prefs are confused
		_SCErrorSet(kSCStatusFailed);
		goto done;
	}

	bond_index = findBond(bonds, BondInterfaceGetInterface(bond));
	if (bond_index == kCFNotFound) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}

	/* yes, we're going to be changing the configuration */
	setConfigurationChanged(prefs);

	/* remove the bond */

	if (CFArrayGetCount(bonds) > 1) {
		CFAllocatorRef		allocator;
		CFMutableArrayRef	newBonds;

		allocator = CFGetAllocator(prefs);
		newBonds = CFArrayCreateMutableCopy(allocator, 0, bonds);
		CFArrayRemoveValueAtIndex(newBonds, bond_index);
		(void) SCPreferencesSetValue(prefsPrivate->prefs, BOND_PREFERENCES_BONDS, newBonds);
		CFRelease(newBonds);
	} else {
		(void) SCPreferencesRemoveValue(prefsPrivate->prefs, BOND_PREFERENCES_BONDS);
	}

	ok = TRUE;

    done :

	pthread_mutex_unlock(&prefsPrivate->lock);

	return ok;
}


Boolean
BondPreferencesCommitChanges(BondPreferencesRef	prefs)
{
	Boolean				ok		= FALSE;
	BondPreferencesPrivateRef	prefsPrivate	= (BondPreferencesPrivateRef)prefs;

	if (!isA_BondPreferences(prefs)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	ok = SCPreferencesCommitChanges(prefsPrivate->prefs);
	if (!ok) {
		return ok;
	}

	if (prefsPrivate->bBase != NULL)  {
		CFRelease(prefsPrivate->bBase);
		prefsPrivate->bBase = NULL;
	}

	return TRUE;
}


Boolean
_BondPreferencesUpdateConfiguration(BondPreferencesRef prefs)
{
	CFArrayRef			active		= NULL;
	CFArrayRef			config		= NULL;
	CFIndex				i;
	CFIndex				nActive;
	CFIndex				nConfig;
	Boolean				ok		= FALSE;
	BondPreferencesPrivateRef	prefsPrivate	= (BondPreferencesPrivateRef)prefs;
	int				s		= -1;

	if (!isA_BondPreferences(prefs)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	/* configured Bonds */
	if (prefsPrivate->bBase != NULL) {
		/*
		 * updated Bond preferences have not been committed
		 * so we ignore any in-progress changes and apply the
		 * saved preferences.
		 */
		config = CFRetain(prefsPrivate->bBase);
	} else {
		/*
		 * apply the saved preferences
		 */
		config = BondPreferencesCopyInterfaces(prefs);
	}
	nConfig = CFArrayGetCount(config);

	/* active Bonds */
	active  = _BondPreferencesCopyActiveInterfaces();
	nActive = CFArrayGetCount(active);

	/*
	 * remove any no-longer-configured bond interfaces and
	 * any devices associated with a bond that are no longer
	 * associated with a bond.
	 */
	for (i = 0; i < nActive; i++) {
		BondInterfaceRef	a_bond;
		CFStringRef		a_bond_if;
		CFIndex			j;
		Boolean			found	= FALSE;

		a_bond    = CFArrayGetValueAtIndex(active, i);
		a_bond_if = BondInterfaceGetInterface(a_bond);

		for (j = 0; j < nConfig; j++) {
			BondInterfaceRef	c_bond;
			CFStringRef		c_bond_if;

			c_bond    = CFArrayGetValueAtIndex(config, j);
			c_bond_if = BondInterfaceGetInterface(c_bond);

			if (CFEqual(a_bond_if, c_bond_if)) {
				CFIndex		a;
				CFIndex		a_count;
				CFArrayRef	a_bond_devices;
				CFIndex		c_count;
				CFArrayRef	c_bond_devices;

				c_bond_devices = BondInterfaceGetDevices(c_bond);
				c_count        = (c_bond_devices != NULL) ? CFArrayGetCount(c_bond_devices) : 0;

				a_bond_devices = BondInterfaceGetDevices(a_bond);
				a_count        = (a_bond_devices != NULL) ? CFArrayGetCount(a_bond_devices) : 0;

				for (a = 0; a < a_count; a++) {
					CFStringRef	a_device;

					a_device = CFArrayGetValueAtIndex(a_bond_devices, a);
					if ((c_count == 0) ||
					    !CFArrayContainsValue(c_bond_devices,
								  CFRangeMake(0, c_count),
								  a_device)) {
						/*
						 * if this device is no longer part
						 * of the bond.
						 */
						if (s == -1) {
							s = inet_dgram_socket();
						}

						ok = _Bond_removeDevice(s, a_bond_if, a_device);
						if (!ok) {
							goto done;
						}
					}
				}

				found = TRUE;
				break;
			}
		}

		if (!found) {
			/*
			 * if this interface is no longer configured
			 */
			if (s == -1) {
				s = inet_dgram_socket();
			}

			ok = __destroyInterface(s, a_bond_if);
			if (!ok) {
				_SCErrorSet(kSCStatusFailed);
				goto done;
			}
		}
	}

	/*
	 * add any newly-configured bond interfaces and add any
	 * devices that should now be associated with the bond.
	 */
	for (i = 0; i < nConfig; i++) {
		BondInterfaceRef	c_bond;
		CFArrayRef		c_bond_devices;
		CFStringRef		c_bond_if;
		CFIndex			c_count;
		Boolean			found		= FALSE;
		CFIndex			j;

		c_bond         = CFArrayGetValueAtIndex(config, i);
		c_bond_if      = BondInterfaceGetInterface(c_bond);
		c_bond_devices = BondInterfaceGetDevices(c_bond);
		c_count        = (c_bond_devices != NULL) ? CFArrayGetCount(c_bond_devices) : 0;

		for (j = 0; j < nActive; j++) {
			BondInterfaceRef	a_bond;
			CFArrayRef		a_bond_devices;
			CFStringRef		a_bond_if;
			CFIndex			a_count;

			a_bond         = CFArrayGetValueAtIndex(active, j);
			a_bond_if      = BondInterfaceGetInterface(a_bond);
			a_bond_devices = BondInterfaceGetDevices(a_bond);
			a_count         = (a_bond_devices != NULL) ? CFArrayGetCount(a_bond_devices) : 0;

			if (CFEqual(c_bond_if, a_bond_if)) {
				CFIndex	c;

				found = TRUE;

				if ((c_bond_devices == NULL) &&
				    (a_bond_devices == NULL)) {
					break;	// if no change
				}

				if ((c_bond_devices != NULL) &&
				    (a_bond_devices != NULL) &&
				    CFEqual(c_bond_devices, a_bond_devices)) {
					break;	// if no change
				}

				if (s == -1) {
					s = inet_dgram_socket();
				}

				/*
				 * ensure that the first device of the bond matches, if
				 * not then we remove all current devices and add them
				 * back in the preferred order.
				 */
				if ((c_count > 0) &&
				    (a_count > 0) &&
				    !CFEqual(CFArrayGetValueAtIndex(c_bond_devices, 0),
					     CFArrayGetValueAtIndex(a_bond_devices, 0))) {
					CFIndex	a;

					for (a = 0; a < a_count; a++) {
						CFStringRef	a_device;

						a_device = CFArrayGetValueAtIndex(a_bond_devices, a);

						if (!CFArrayContainsValue(c_bond_devices,
									 CFRangeMake(0, c_count),
									 a_device)) {
							continue;	// if already removed
						}

						ok = _Bond_removeDevice(s, a_bond_if, a_device);
						if (!ok) {
							goto done;
						}
					}

					a_count = 0;	// all active devices have been removed
				}

				/*
				 * add any devices which are not currently associated
				 * with the bond interface.
				 */
				for (c = 0; c < c_count; c++) {
					CFStringRef	c_device;

					c_device = CFArrayGetValueAtIndex(c_bond_devices, c);
					if ((a_count == 0) ||
					    !CFArrayContainsValue(a_bond_devices,
								  CFRangeMake(0, a_count),
								  c_device)) {
						/*
						 * check if this device can be added to
						 * a bond.
						 */
						if (!IsBondSupported(c_device)) {
							// if not supported
							continue;
						}

						/*
						 * if this device is not currently part
						 * of the bond.
						 */

						ok = _Bond_addDevice(s, c_bond_if, c_device);
						if (!ok) {
							goto done;
						}
					}
				}

				break;
			}
		}

		if (!found) {
			CFIndex	c;

			if (s == -1) {
				s = inet_dgram_socket();
			}

			/*
			 * establish the new bond interface.
			 */
			ok = __createInterface(s, c_bond_if);
			if (!ok) {
				_SCErrorSet(kSCStatusFailed);
				goto done;
			}

			/*
			 * add any devices which are not currently associated
			 * with the bond interface.
			 */
			for (c = 0; c < c_count; c++) {
				CFStringRef	c_device;

				c_device = CFArrayGetValueAtIndex(c_bond_devices, c);

				if (!IsBondSupported(c_device)) {
					// if not supported
					continue;
				}

				ok = _Bond_addDevice(s, c_bond_if, c_device);
				if (!ok) {
					goto done;
				}
			}
		}

	}

	ok = TRUE;

    done :

	if (active != NULL)	CFRelease(active);
	if (config != NULL)	CFRelease(config);
	if (s != -1)		(void) close(s);

	return ok;
}


Boolean
BondPreferencesApplyChanges(BondPreferencesRef prefs)
{
	Boolean				ok		= FALSE;
	BondPreferencesPrivateRef	prefsPrivate	= (BondPreferencesPrivateRef)prefs;

	if (!isA_BondPreferences(prefs)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	pthread_mutex_lock(&prefsPrivate->lock);

	/* apply the preferences */
	ok = SCPreferencesApplyChanges(prefsPrivate->prefs);
	if (!ok) {
		goto done;
	}

	/* apply the Bond configuration */
	ok = _BondPreferencesUpdateConfiguration(prefs);
	if (!ok) {
		goto done;
	}

    done :

	pthread_mutex_unlock(&prefsPrivate->lock);

	return ok;
}


/* ---------- BondStatus ---------- */

typedef struct {

	/* base CFType information */
	CFRuntimeBase			cfBase;

	/* bond interface */
	BondInterfaceRef		bond;

	/* bond status */
	CFDictionaryRef			status_interface;	// interface status
	CFArrayRef			devices;		// per-device status
	CFDictionaryRef			status_devices;

} BondStatusPrivate, * BondStatusPrivateRef;


const CFStringRef kSCBondStatusDeviceAggregationStatus	= CFSTR("AggregationStatus");
const CFStringRef kSCBondStatusDeviceCollecting		= CFSTR("Collecting");
const CFStringRef kSCBondStatusDeviceDistributing	= CFSTR("Distributing");


static CFStringRef	__BondStatusCopyDescription	(CFTypeRef cf);
static void		__BondStatusDeallocate		(CFTypeRef cf);
static Boolean		__BondStatusEqual		(CFTypeRef cf1, CFTypeRef cf2);


static const CFRuntimeClass __BondStatusClass = {
	0,				// version
	"BondStatus",			// className
	NULL,				// init
	NULL,				// copy
	__BondStatusDeallocate,		// dealloc
	__BondStatusEqual,		// equal
	NULL,				// hash
	NULL,				// copyFormattingDesc
	__BondStatusCopyDescription	// copyDebugDesc
};


static CFTypeID		__kBondStatusTypeID	= _kCFRuntimeNotATypeID;


static pthread_once_t	bondStatus_init		= PTHREAD_ONCE_INIT;


static CFStringRef
__BondStatusCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef		allocator	= CFGetAllocator(cf);
	CFMutableStringRef	result;
	BondStatusPrivateRef	statusPrivate	= (BondStatusPrivateRef)cf;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<BondStatus %p [%p]> {"), cf, allocator);
	CFStringAppendFormat(result, NULL, CFSTR(" bond = %@"), statusPrivate->bond);
	CFStringAppendFormat(result, NULL, CFSTR(" interface = %@"), statusPrivate->status_interface);
	CFStringAppendFormat(result, NULL, CFSTR(" devices = %@"),   statusPrivate->status_devices);
	CFStringAppendFormat(result, NULL, CFSTR(" }"));

	return result;
}


static void
__BondStatusDeallocate(CFTypeRef cf)
{
	BondStatusPrivateRef	statusPrivate	= (BondStatusPrivateRef)cf;

	/* release resources */

	CFRelease(statusPrivate->bond);
	CFRelease(statusPrivate->status_interface);
	if (statusPrivate->devices != NULL) CFRelease(statusPrivate->devices);
	CFRelease(statusPrivate->status_devices);
	return;
}


static Boolean
__BondStatusEqual(CFTypeRef cf1, CFTypeRef cf2)
{
	BondStatusPrivateRef	status1	= (BondStatusPrivateRef)cf1;
	BondStatusPrivateRef	status2	= (BondStatusPrivateRef)cf2;

	if (status1 == status2)
		return TRUE;

	if (!CFEqual(status1->bond, status2->bond))
		return FALSE;	// if not the same bond

	if (!CFEqual(status1->status_interface, status2->status_interface))
		return FALSE;	// if not the same interface status

	if (!CFEqual(status1->status_devices, status2->status_devices))
		return FALSE;	// if not the same device status

	return TRUE;
}


static void
__BondStatusInitialize(void)
{
	__kBondStatusTypeID = _CFRuntimeRegisterClass(&__BondStatusClass);
	return;
}


static __inline__ CFTypeRef
isA_BondStatus(CFTypeRef obj)
{
	return (isA_CFType(obj, BondStatusGetTypeID()));
}


CFTypeID
BondStatusGetTypeID(void)
{
	pthread_once(&bondStatus_init, __BondStatusInitialize);	/* initialize runtime */
	return __kBondStatusTypeID;
}


static BondStatusRef
__BondStatusCreatePrivate(CFAllocatorRef	allocator,
			  BondInterfaceRef	bond,
			  CFDictionaryRef	status_interface,
			  CFDictionaryRef	status_devices)
{
	BondStatusPrivateRef	statusPrivate;
	uint32_t		size;

	/* initialize runtime */
	pthread_once(&bondStatus_init, __BondStatusInitialize);

	/* allocate bond */
	size          = sizeof(BondStatusPrivate) - sizeof(CFRuntimeBase);
	statusPrivate = (BondStatusPrivateRef)_CFRuntimeCreateInstance(allocator,
								       __kBondStatusTypeID,
								       size,
								       NULL);
	if (statusPrivate == NULL) {
		return NULL;
	}

	/* establish the bond status */

	statusPrivate->bond             = CFRetain(bond);
	statusPrivate->status_interface = CFDictionaryCreateCopy(allocator, status_interface);
	statusPrivate->devices          = NULL;
	statusPrivate->status_devices   = CFDictionaryCreateCopy(allocator, status_devices);

	return (BondStatusRef)statusPrivate;
}


BondStatusRef
BondInterfaceCopyStatus(BondInterfaceRef bond)
{
	BondInterfacePrivateRef	bondPrivate	= (BondInterfacePrivateRef)bond;
	int			bond_if_active;
	int			bond_if_status;
	char			bond_ifname[IFNAMSIZ + 1];
	CFIndex			i;
	struct if_bond_status_req *ibsr_p = NULL;
	CFIndex			n;
	CFNumberRef		num;
	int			s;
	struct if_bond_status * scan_p;
	BondStatusRef		status		= NULL;
	CFMutableDictionaryRef	status_devices;
	CFMutableDictionaryRef	status_interface;

	if (!isA_BondInterface(bond)) {
		return NULL;
	}

	s = inet_dgram_socket();
	if (s < 0) {
		goto done;
	}
	_SC_cfstring_to_cstring(bondPrivate->ifname, bond_ifname,
				sizeof(bond_ifname), kCFStringEncodingASCII);
	(void)siocgifmedia(s, bond_ifname, &bond_if_status, &bond_if_active);
	ibsr_p = if_bond_status_req_copy(s, bond_ifname);
	if (ibsr_p == NULL) {
		goto done;
	}
	status_interface = CFDictionaryCreateMutable(NULL,
						     0,
						     &kCFTypeDictionaryKeyCallBacks,
						     &kCFTypeDictionaryValueCallBacks);

	status_devices = CFDictionaryCreateMutable(NULL,
						   0,
						   &kCFTypeDictionaryKeyCallBacks,
						   &kCFTypeDictionaryValueCallBacks);
	n = ibsr_p->ibsr_total;
	for (i = 0, scan_p = (struct if_bond_status *)ibsr_p->ibsr_buffer; i < n; i++, scan_p++) {
		CFStringRef			bond_if;
		int				collecting = 0;
		int				distributing = 0;
		struct if_bond_partner_state *	ps;
		CFMutableDictionaryRef		status_device;
		int				status_val;

		ps = &scan_p->ibs_partner_state;

		status_device = CFDictionaryCreateMutable(NULL,
							  0,
							  &kCFTypeDictionaryKeyCallBacks,
							  &kCFTypeDictionaryValueCallBacks);
		if (lacp_actor_partner_state_in_sync(scan_p->ibs_state)) {
			/* we're in-sync */
			status_val = kSCBondStatusOK;
			if (lacp_actor_partner_state_in_sync(ps->ibps_state)) {
				/* partner is also in-sync */
				if (lacp_actor_partner_state_collecting(scan_p->ibs_state)
				    && lacp_actor_partner_state_distributing(ps->ibps_state)) {
					/* we're able to collect (receive) frames */
					collecting = 1;
				}
				if (lacp_actor_partner_state_distributing(scan_p->ibs_state)
				    && lacp_actor_partner_state_collecting(ps->ibps_state)) {
					/* we're able to distribute (transmit) frames */
					distributing = 1;
				}
			}
		}
		else {
			int		active = 0;
			int		status = 0;
			lacp_system	zeroes = {{0,0,0,0,0,0}};

			(void)siocgifmedia(s, scan_p->ibs_if_name, &status, &active);
			if ((status & IFM_AVALID) == 0 || (status & IFM_ACTIVE) == 0
			    || (active & IFM_FDX) == 0) {
				/* link down or not full-duplex */
				status_val = kSCBondStatusLinkInvalid;
			}
			else if (ps->ibps_system_priority == 0
				 && bcmp(&zeroes, &ps->ibps_system, sizeof(zeroes)) == 0) {
				/* no one on the other end of the link */
				status_val = kSCBondStatusNoPartner;
			}
			else if (active != bond_if_active) {
				/* the link speed was different */
				status_val = kSCBondStatusLinkInvalid;
			}
			else {
				/* partner is not in the active group */
				status_val = kSCBondStatusNotInActiveGroup;
			}
		}
		num = CFNumberCreate(NULL, kCFNumberIntType, &status_val);
		CFDictionarySetValue(status_device, kSCBondStatusDeviceAggregationStatus, num);
		CFRelease(num);
		num = CFNumberCreate(NULL, kCFNumberIntType, &collecting);
		CFDictionarySetValue(status_device, kSCBondStatusDeviceCollecting, num);
		CFRelease(num);
		num = CFNumberCreate(NULL, kCFNumberIntType, &distributing);
		CFDictionarySetValue(status_device, kSCBondStatusDeviceDistributing, num);
		CFRelease(num);
		bond_if = CFArrayGetValueAtIndex(bondPrivate->devices, i);
		CFDictionarySetValue(status_devices, bond_if, status_device);
		CFRelease(status_device);
	}

	status = __BondStatusCreatePrivate(NULL, bond, status_interface, status_devices);

	CFRelease(status_interface);
	CFRelease(status_devices);
 done:
	if (s >= 0) {
		close(s);
	}
	if (ibsr_p != NULL) {
		free(ibsr_p);
	}
	return status;
}


#define	N_QUICK	16


CFArrayRef
BondStatusGetDevices(BondStatusRef bondStatus)
{
	BondStatusPrivateRef	statusPrivate	= (BondStatusPrivateRef)bondStatus;

	if (!isA_BondStatus(bondStatus)) {
		return NULL;
	}

	if (statusPrivate->devices == NULL) {
		const void *	keys_q[N_QUICK];
		const void **	keys	= keys_q;
		CFIndex		n;

		n = CFDictionaryGetCount(statusPrivate->status_devices);
		if (n > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
			keys = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
		}
		CFDictionaryGetKeysAndValues(statusPrivate->status_devices, keys, NULL);
		statusPrivate->devices = CFArrayCreate(NULL, keys, n, &kCFTypeArrayCallBacks);
		if (keys != keys_q) {
			CFAllocatorDeallocate(NULL, keys);
		}
	}

	return statusPrivate->devices;
}


CFDictionaryRef
BondStatusGetInterfaceStatus(BondStatusRef bondStatus)
{
	BondStatusPrivateRef	statusPrivate	= (BondStatusPrivateRef)bondStatus;

	if (!isA_BondStatus(bondStatus)) {
		return NULL;
	}

	return statusPrivate->status_interface;
}


CFDictionaryRef
BondStatusGetDeviceStatus(BondStatusRef bondStatus, CFStringRef device)
{
	BondStatusPrivateRef	statusPrivate	= (BondStatusPrivateRef)bondStatus;

	if (!isA_BondStatus(bondStatus)) {
		return NULL;
	}

	return CFDictionaryGetValue(statusPrivate->status_devices, device);
}
