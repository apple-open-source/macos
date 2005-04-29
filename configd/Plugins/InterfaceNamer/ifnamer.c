/*
 * Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
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
 * October 3, 2003		Allan Nathanson <ajn@apple.com>
 * - sort new interfaces by IOKit path (rather than MAC address) to
 *   help facilitate a more predictable interface-->name mapping for
 *   like hardware configurations.
 *
 * June 23, 2001		Allan Nathanson <ajn@apple.com>
 * - update to public SystemConfiguration.framework APIs
 *
 * January 23, 2001		Dieter Siegmund <dieter@apple.com>
 * - initial revision
 */

/*
 * ifnamer.c
 * - module that receives IOKit Network Interface messages
 *   and names any interface that currently does not have a name
 * - uses Interface Type and MACAddress as the unique identifying
 *   keys; any interface that doesn't contain both of these properties
 *   is ignored and not processed
 * - stores the Interface Type, MACAddress, and Unit in permanent storage
 *   to give persistent interface names
 */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sockio.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <mach/mach.h>
#include <net/if_types.h>

#include <CoreFoundation/CoreFoundation.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDPlugin.h>
#include <SystemConfiguration/SCPrivate.h>	// for SCLog(), SCPrint()
#include <SystemConfiguration/SCValidation.h>

#include <SystemConfiguration/BondConfiguration.h>
#include <SystemConfiguration/BondConfigurationPrivate.h>

#include <SystemConfiguration/VLANConfiguration.h>
#include <SystemConfiguration/VLANConfigurationPrivate.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/network/IONetworkController.h>
#include <IOKit/network/IONetworkInterface.h>

#ifndef	kIOBuiltin
#define	kIOBuiltin			"IOBuiltin"
#endif

#ifndef	kIOLocation
#define	kIOLocation			"IOLocation"
#endif

#define kIONetworkStackUserCommand	"IONetworkStackUserCommand"
#define kIORegisterOne			1

#define MY_PLUGIN_NAME			"InterfaceNamer"

static boolean_t			S_debug = FALSE;
static CFMutableArrayRef		S_dblist = NULL;
static io_connect_t			S_connect = MACH_PORT_NULL;
static io_iterator_t			S_iter = MACH_PORT_NULL;
static IONotificationPortRef		S_notify = NULL;

static void
writeInterfaceList(CFArrayRef ilist);

static void
displayInterface(CFDictionaryRef if_dict);

static CFDictionaryRef
lookupIOKitPath(CFStringRef if_path);

static __inline__ CFComparisonResult
compareMacAddress(CFDataRef addr1, CFDataRef addr2)
{
    int len1;
    int len2;
    int clen;
    int res;

    len1 = CFDataGetLength(addr1);
    len2 = CFDataGetLength(addr2);

    if (len1 == len2) {
	if (len1 == 0)
	    return (kCFCompareEqualTo);
	return (memcmp(CFDataGetBytePtr(addr1),
		       CFDataGetBytePtr(addr2),
		       len1));
    }
    clen = len1;
    if (len2 < clen)
	clen = len2;
    res = memcmp(CFDataGetBytePtr(addr1),
		 CFDataGetBytePtr(addr2),
		 clen);
    if (res == 0) {
	return (len1 - len2);
    }
    return (res);
}

static CFComparisonResult
if_unit_compare(const void *val1, const void *val2, void *context)
{
    CFComparisonResult	res;
    CFNumberRef		type1;
    CFNumberRef		type2;
    CFNumberRef		unit1;
    CFNumberRef		unit2;

    type1 = CFDictionaryGetValue((CFDictionaryRef)val1,
				 CFSTR(kIOInterfaceType));
    type2 = CFDictionaryGetValue((CFDictionaryRef)val2,
				 CFSTR(kIOInterfaceType));
    res = CFNumberCompare(type1, type2, NULL);
    if (res != kCFCompareEqualTo) {
	return (res);
    }
    unit1 = CFDictionaryGetValue((CFDictionaryRef)val1,
				 CFSTR(kIOInterfaceUnit));
    unit2 = CFDictionaryGetValue((CFDictionaryRef)val2,
				 CFSTR(kIOInterfaceUnit));
    return (CFNumberCompare(unit1, unit2, NULL));
}

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
if_path_compare(const void *val1, const void *val2, void *context)
{
    CFBooleanRef	builtin;
    Boolean		builtin_val1	= FALSE;
    Boolean		builtin_val2	= FALSE;
    CFArrayRef		elements1	= NULL;
    CFArrayRef		elements2	= NULL;
    CFIndex		i;
    CFIndex		n;
    CFIndex		n1		= 0;
    CFIndex		n2		= 0;
    CFStringRef		path;
    CFComparisonResult	res;
    CFNumberRef		type1;
    CFNumberRef		type2;

    /* sort by interface type */

    type1 = CFDictionaryGetValue((CFDictionaryRef)val1, CFSTR(kIOInterfaceType));
    type2 = CFDictionaryGetValue((CFDictionaryRef)val2, CFSTR(kIOInterfaceType));
    res = CFNumberCompare(type1, type2, NULL);
    if (res != kCFCompareEqualTo) {
	return (res);
    }

    /* built-in interfaces sort first */
    builtin = CFDictionaryGetValue((CFDictionaryRef)val1, CFSTR(kIOBuiltin));
    if (isA_CFBoolean(builtin) != NULL) {
	builtin_val1 = CFBooleanGetValue(builtin);
    }
    builtin = CFDictionaryGetValue((CFDictionaryRef)val2, CFSTR(kIOBuiltin));
    if (isA_CFBoolean(builtin) != NULL) {
	builtin_val2 = CFBooleanGetValue(builtin);
    }
    if (builtin_val1 != builtin_val2) {
	if (builtin_val1) {
	    res = kCFCompareLessThan;
	} else {
	    res = kCFCompareGreaterThan;
	}
	return (res);
    }

    /* ... and then sort built-in interfaces by "location" */
    if (builtin_val1) {
	CFStringRef	location1;
	CFStringRef	location2;

	location1 = CFDictionaryGetValue((CFDictionaryRef)val1, CFSTR(kIOLocation));
	location2 = CFDictionaryGetValue((CFDictionaryRef)val2, CFSTR(kIOLocation));
	if (location1 != location2) {
	    if (isA_CFString(location1)) {
		if (isA_CFString(location2)) {
		    res = CFStringCompare(location1, location2, 0);
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

    /* ... and then sort by IOPathMatch */

    path = CFDictionaryGetValue((CFDictionaryRef)val1, CFSTR(kIOPathMatchKey));
    if (isA_CFString(path)) {
	elements1 = split_path(path);
	n1 = CFArrayGetCount(elements1);
    } else {
	goto done;
    }

    path = CFDictionaryGetValue((CFDictionaryRef)val2, CFSTR(kIOPathMatchKey));
    if (isA_CFString(path)) {
	elements2 = split_path(path);
	n2 = CFArrayGetCount(elements2);
    } else {
	goto done;
    }

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

	str = _SC_cfstring_to_cstring(e1, NULL, 0, kCFStringEncodingASCII);
	errno = 0;
	q1 = strtoq(str, &end, 16);
	isNum = ((*str != '\0') && (*end == '\0') && (errno == 0));
	CFAllocatorDeallocate(NULL, str);

	if (isNum) {
	    // if e1 is a valid numeric string
	    str = _SC_cfstring_to_cstring(e2, NULL, 0, kCFStringEncodingASCII);
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

 done :
    if ( elements1 ) CFRelease( elements1 );
    if ( elements2 ) CFRelease( elements2 );

    return res;
}

static boolean_t
addCFStringProperty( CFMutableDictionaryRef dict,
		     const char *           key,
		     const char *           string )
{
    boolean_t    ret = false;
    CFStringRef  valObj, keyObj;

    if ( (string == 0) || (key == 0) || (dict == 0) )
	return false;

    keyObj = CFStringCreateWithCString(NULL,
				       key,
				       kCFStringEncodingASCII );

    valObj = CFStringCreateWithCString(NULL,
				       string,
				       kCFStringEncodingASCII );

    if (valObj && keyObj) {
	CFDictionarySetValue( dict, keyObj, valObj );
	ret = true;
    }

    if ( keyObj ) CFRelease( keyObj );
    if ( valObj ) CFRelease( valObj );

    return ret;
}

static boolean_t
addCFNumberProperty( CFMutableDictionaryRef dict,
		     const char *           key,
		     unsigned int           number )
{
    boolean_t    ret = false;
    CFNumberRef  numObj;
    CFStringRef  keyObj;

    if ( (key == 0) || (dict == 0) )
	return false;

    numObj = CFNumberCreate(NULL,
			    kCFNumberLongType,
			    &number);

    keyObj = CFStringCreateWithCString(NULL,
				       key,
				       kCFStringEncodingASCII );

    if ( numObj && keyObj )
	{
	    CFDictionarySetValue( dict, keyObj, numObj );
	    ret = true;
	}

    if ( numObj ) CFRelease( numObj );
    if ( keyObj ) CFRelease( keyObj );

    return ret;
}

static void *
read_file(char * filename, size_t * data_length)
{
    void *		data = NULL;
    size_t		len = 0;
    int			fd = -1;
    struct stat		sb;

    *data_length = 0;
    if (stat(filename, &sb) < 0)
	goto done;
    len = sb.st_size;
    if (len == 0)
	goto done;

    data = malloc(len);
    if (data == NULL)
	goto done;

    fd = open(filename, O_RDONLY);
    if (fd < 0)
	goto done;

    if (read(fd, data, len) != len) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME ": read %s failed, %s"),
	      filename, strerror(errno));
	goto done;
    }
 done:
    if (fd >= 0)
	close(fd);
    if (data) {
	*data_length = len;
    }
    return (data);
}

static CFPropertyListRef
readPropertyList(char * filename)
{
    void *		buf;
    size_t		bufsize;
    CFDataRef		data = NULL;
    CFPropertyListRef	plist = NULL;
    CFStringRef		errorString = NULL;

    buf = read_file(filename, &bufsize);
    if (buf == NULL) {
	return (NULL);
    }
    data = CFDataCreate(NULL, buf, bufsize);
    if (data == NULL) {
	goto error;
    }

    plist = CFPropertyListCreateFromXMLData(NULL, data,
					    kCFPropertyListMutableContainers,
					    &errorString);
    if (plist == NULL) {
	if (errorString) {
	    SCLog(TRUE, LOG_INFO,
		  CFSTR(MY_PLUGIN_NAME ":%@"),
		  errorString);
	    CFRelease(errorString);
	}
    }
 error:
    if (data)
	CFRelease(data);
    if (buf)
	free(buf);
    return (plist);
}

#define	IFNAMER_ID			CFSTR("com.apple.SystemConfiguration.InterfaceNamer")
#define	INTERFACES			CFSTR("Interfaces")
#define	NETWORK_INTERFACES_PREFS	CFSTR("NetworkInterfaces.plist")
#define	OLD_NETWORK_INTERFACES_FILE	"/var/db/NetworkInterfaces.xml"

static CFMutableArrayRef
readInterfaceList()
{
    CFArrayRef		ilist;
    CFMutableArrayRef 	plist = NULL;
    SCPreferencesRef	prefs = NULL;

    prefs = SCPreferencesCreate(NULL, IFNAMER_ID, NETWORK_INTERFACES_PREFS);
    if (!prefs) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME ": SCPreferencesCreate failed, %s"),
	      SCErrorString(SCError()));
	return (NULL);
    }

    ilist = SCPreferencesGetValue(prefs, INTERFACES);
    if (isA_CFArray(ilist)) {
	plist = CFArrayCreateMutableCopy(NULL, 0, ilist);
    } else {
	plist = (CFMutableArrayRef)readPropertyList(OLD_NETWORK_INTERFACES_FILE);
	if (plist == NULL) {
	    goto done;
	}
	if (isA_CFArray(plist) == NULL) {
	    CFRelease(plist);
	    goto done;
	}
	writeInterfaceList(plist);
	(void)unlink(OLD_NETWORK_INTERFACES_FILE);
    }

  done:
    if (prefs) {
	CFRelease(prefs);
    }
    return (plist);
}

static void
writeInterfaceList(CFArrayRef ilist)
{
    SCPreferencesRef	prefs;

    if (isA_CFArray(ilist) == NULL) {
	return;
    }

    prefs = SCPreferencesCreate(NULL, IFNAMER_ID, NETWORK_INTERFACES_PREFS);
    if (prefs == NULL) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME ": SCPreferencesCreate failed, %s"),
	      SCErrorString(SCError()));
	return;
    }

    if (!SCPreferencesSetValue(prefs, INTERFACES, ilist)) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME ": SCPreferencesSetValue failed, %s"),
	      SCErrorString(SCError()));
	goto done;
    }

    if (!SCPreferencesCommitChanges(prefs)) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME ": SCPreferencesCommitChanges failed, %s"),
	      SCErrorString(SCError()));
	goto done;
    }

done:

    CFRelease(prefs);
    return;
}

static void
updateBondConfiguration(void)
{
    BondPreferencesRef	prefs;

    prefs = BondPreferencesCreate(NULL);
    if (prefs == NULL) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME ": BondPreferencesCreate failed, %s"),
	      SCErrorString(SCError()));
	return;
    }

    if (!_BondPreferencesUpdateConfiguration(prefs)) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME ": _BondPreferencesUpdateConfiguration failed, %s"),
	      SCErrorString(SCError()));
	goto done;
    }

done:

    CFRelease(prefs);
    return;
}

static void
updateVLANConfiguration(void)
{
    VLANPreferencesRef	prefs;

    prefs = VLANPreferencesCreate(NULL);
    if (prefs == NULL) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME ": VLANPreferencesCreate failed, %s"),
	      SCErrorString(SCError()));
	return;
    }

    if (!_VLANPreferencesUpdateConfiguration(prefs)) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME ": _VLANPreferencesUpdateConfiguration failed, %s"),
	      SCErrorString(SCError()));
	goto done;
    }

done:

    CFRelease(prefs);
    return;
}

#define INDEX_BAD	(-1)

static CFDictionaryRef
lookupInterfaceByType(CFArrayRef list, CFDictionaryRef if_dict, int * where)
{
    CFDataRef	addr;
    CFIndex	i;
    CFIndex	n;
    CFNumberRef	type;

    if (where) {
	*where = INDEX_BAD;
    }
    if (list == NULL) {
	return (NULL);
    }
    addr = CFDictionaryGetValue(if_dict, CFSTR(kIOMACAddress));
    type = CFDictionaryGetValue(if_dict, CFSTR(kIOInterfaceType));
    if (type == NULL || addr == NULL) {
	return (NULL);
    }

    n = CFArrayGetCount(list);
    for (i = 0; i < n; i++) {
	CFDictionaryRef	dict = CFArrayGetValueAtIndex(list, i);
	CFDataRef	a;
	CFNumberRef	t;

	a = CFDictionaryGetValue(dict, CFSTR(kIOMACAddress));
	t = CFDictionaryGetValue(dict, CFSTR(kIOInterfaceType));
	if (a == NULL || t == NULL)
	    continue;

	if (CFNumberCompare(type, t, NULL) == kCFCompareEqualTo
	    && compareMacAddress(addr, a) == kCFCompareEqualTo) {
	    if (where) {
		*where = i;
	    }
	    return (dict);
	}
    }
    return (NULL);
}

static CFDictionaryRef
lookupInterfaceByUnit(CFArrayRef list, CFDictionaryRef if_dict, int * where)
{
    CFIndex 	i;
    CFIndex	n;
    CFNumberRef	type;
    CFNumberRef	unit;

    if (where) {
	*where = INDEX_BAD;
    }
    if (list == NULL) {
	return (NULL);
    }
    type = CFDictionaryGetValue(if_dict, CFSTR(kIOInterfaceType));
    unit = CFDictionaryGetValue(if_dict, CFSTR(kIOInterfaceUnit));
    if (type == NULL || unit == NULL) {
	return (NULL);
    }

    n = CFArrayGetCount(list);
    for (i = 0; i < n; i++) {
	CFDictionaryRef	dict = CFArrayGetValueAtIndex(list, i);
	CFNumberRef	t;
	CFNumberRef	u;

	t = CFDictionaryGetValue(dict, CFSTR(kIOInterfaceType));
	u = CFDictionaryGetValue(dict, CFSTR(kIOInterfaceUnit));
	if (t == NULL || u == NULL) {
	    continue;
	}

	if (CFNumberCompare(type, t, NULL) == kCFCompareEqualTo
	    && CFNumberCompare(unit, u, NULL) == kCFCompareEqualTo) {
	    if (where)
		*where = i;
	    return (dict);
	}
    }
    return (NULL);
}

#define kAirPortDriverPath	CFSTR("AirPort")
#define kIO80211InterfacePath	CFSTR("IO80211Interface")
#define APPLE_WIRELESS_80211	CFSTR("AppleWireless80211")

static __inline__ boolean_t
pathIsAirPort(CFStringRef path)
{
    CFRange r;

    r = CFStringFind(path, kIO80211InterfacePath, 0);
    if (r.location != kCFNotFound) {
	return (TRUE);
    }

    r = CFStringFind(path, kAirPortDriverPath, 0);
    if (r.location != kCFNotFound) {
	return (TRUE);
    }

    r = CFStringFind(path, APPLE_WIRELESS_80211, 0);
    if (r.location != kCFNotFound) {
	return (TRUE);
    }

    return (FALSE);
}

static CFDictionaryRef
lookupAirPortInterface(CFArrayRef list, int * where)
{
    CFIndex 	i;
    CFIndex	n;

    if (where) {
	*where = INDEX_BAD;
    }
    if (list == NULL) {
	return (NULL);
    }
    n = CFArrayGetCount(list);
    for (i = 0; i < n; i++) {
	CFDictionaryRef	dict = CFArrayGetValueAtIndex(list, i);
	CFStringRef	path;

	path = CFDictionaryGetValue(dict, CFSTR(kIOPathMatchKey));
	if (path == NULL) {
	    continue;
	}
	if (pathIsAirPort(path) == TRUE) {
	    if (where)
		*where = i;
	    return (dict);
	}
    }
    return (NULL);
}

static void
insertInterface(CFMutableArrayRef list, CFDictionaryRef if_dict)
{
    CFIndex		i;
    CFNumberRef		if_type;
    CFNumberRef		if_unit;
    CFIndex		n	= CFArrayGetCount(list);
    CFComparisonResult	res;

    if_type = CFDictionaryGetValue(if_dict, CFSTR(kIOInterfaceType));
    if_unit = CFDictionaryGetValue(if_dict, CFSTR(kIOInterfaceUnit));
    for (i = 0; i < n; i++) {
	CFDictionaryRef	dict = CFArrayGetValueAtIndex(list, i);
	CFNumberRef	type;
	CFNumberRef	unit;

	type = CFDictionaryGetValue(dict, CFSTR(kIOInterfaceType));
	unit = CFDictionaryGetValue(dict, CFSTR(kIOInterfaceUnit));
	res = CFNumberCompare(if_type, type, NULL);
	if (res == kCFCompareLessThan
	    || (res == kCFCompareEqualTo
		&& (CFNumberCompare(if_unit, unit, NULL)
		    == kCFCompareLessThan))) {
	    CFArrayInsertValueAtIndex(list, i, if_dict);
	    return;
	}
    }
    CFArrayAppendValue(S_dblist, if_dict);
    return;
}

static void
replaceInterface(CFDictionaryRef if_dict)
{
    int where;

    if (S_dblist == NULL) {
	S_dblist = CFArrayCreateMutable(NULL, 0,
					&kCFTypeArrayCallBacks);
    }
    /* remove any dict that has our type/addr */
    if (lookupInterfaceByType(S_dblist, if_dict, &where) != NULL) {
	CFArrayRemoveValueAtIndex(S_dblist, where);
    }
    /* remove any dict that has the same type/unit */
    if (lookupInterfaceByUnit(S_dblist, if_dict, &where) != NULL) {
	CFArrayRemoveValueAtIndex(S_dblist, where);
    }
    insertInterface(S_dblist, if_dict);
    return;
}

static CFNumberRef
getHighestUnitForType(CFNumberRef if_type)
{
    int 		i;
    CFIndex		n;
    CFNumberRef		ret_unit = NULL;

    if (S_dblist == NULL)
	return (NULL);

    n = CFArrayGetCount(S_dblist);
    for (i = 0; i < n; i++) {
	CFDictionaryRef	dict = CFArrayGetValueAtIndex(S_dblist, i);
	CFNumberRef	type;
	CFNumberRef	unit;

	type = CFDictionaryGetValue(dict, CFSTR(kIOInterfaceType));
	if (CFEqual(type, if_type)) {
	    unit = CFDictionaryGetValue(dict, CFSTR(kIOInterfaceUnit));
	    if (ret_unit == NULL
		|| (CFNumberCompare(unit, ret_unit, NULL)
		    == kCFCompareGreaterThan)) {
		ret_unit = unit;
	    }
	}
    }
    return (ret_unit);
}

//------------------------------------------------------------------------
// Register a single interface with the given service path to the
// data link layer (BSD), using the specified unit number.

static kern_return_t
registerInterface(io_connect_t connect,
		  CFStringRef path,
		  CFNumberRef unit)
{
    CFMutableDictionaryRef	dict;
    kern_return_t		kr = kIOReturnNoMemory;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    if (dict == NULL
	|| addCFNumberProperty(dict, kIONetworkStackUserCommand,
			       kIORegisterOne) == FALSE)
	;
    else {
	CFDictionarySetValue(dict, CFSTR(kIOPathMatchKey), path);
	CFDictionarySetValue(dict, CFSTR(kIOInterfaceUnit), unit);
	kr = IOConnectSetCFProperties(connect, dict);
    }
    if (dict) CFRelease( dict );
    return kr;
}

/*
 * Note: this function blocks all other plug-ins until it completes
 */
static void
waitForQuiet(mach_port_t masterPort)
{
    mach_timespec_t t;
    kern_return_t   wait_ret;

    t.tv_sec  = 4;
    t.tv_nsec = 0;

    // kIOReturnTimeout if the wait timed out.
    // kIOReturnSuccess on success.
    wait_ret = IOKitWaitQuiet(masterPort, &t);
    return;
}

/*
 * Function: createNetworkStackObject
 * Purpose:
 *   Get a reference to the single IONetworkStack object instance in
 *   the kernel. Naming requests must be sent to this object, which is
 *   attached as a client to all network interface objects in the system.
 * Note:
 *   Call IOObjectRelease on the returned object.
 */
static io_object_t
createNetworkStackObject(mach_port_t masterPort)
{
    io_iterator_t	iter = MACH_PORT_NULL;
    kern_return_t	kr;
    io_object_t		stack = MACH_PORT_NULL;

    kr = IOServiceGetMatchingServices(masterPort,
				      IOServiceMatching("IONetworkStack"),
				      &iter);
    if (iter != MACH_PORT_NULL) {
	if (kr == KERN_SUCCESS) {
	    stack = IOIteratorNext(iter);
	}
	IOObjectRelease(iter);
    }
    return stack;
}

static void
printMacAddress(CFDataRef data)
{
    int		i;
    CFIndex	n = CFDataGetLength(data);

    for (i = 0; i < n; i++) {
	if (i != 0) SCPrint(TRUE, stdout, CFSTR(":"));
	SCPrint(TRUE, stdout, CFSTR("%02x"), CFDataGetBytePtr(data)[i]);
    }
    return;
}

/*
 * Function: getMacAddress
 *
 * Purpose:
 *   Given an interface object if_obj, return its associated mac address.
 *   The mac address is stored in the parent, the network controller object.
 *
 * Returns:
 *   The CFDataRef containing the bytes of the mac address.
 */
static CFDataRef
getMacAddress(io_object_t if_obj)
{
    CFMutableDictionaryRef	dict = NULL;
    CFDataRef			data = NULL;
    kern_return_t		kr;
    io_object_t			parent_obj = MACH_PORT_NULL;

    /* get the parent node */
    kr = IORegistryEntryGetParentEntry(if_obj, kIOServicePlane, &parent_obj);
    if (kr != KERN_SUCCESS) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME
		    ": IORegistryEntryGetParentEntry returned 0x%x"),
	      kr);
	goto failed;
    }

    /* get the dictionary associated with the node */
    kr = IORegistryEntryCreateCFProperties(parent_obj,
					   &dict,
					   NULL,
					   kNilOptions );
    if (kr != KERN_SUCCESS) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME
		    ": IORegistryEntryCreateCFProperties returned 0x%x"),
	      kr);
	goto failed;
    }
    data = CFDictionaryGetValue(dict, CFSTR(kIOMACAddress));
    if (data) {
	CFRetain(data);
    }

 failed:
    if (dict)
	CFRelease(dict);
    if (parent_obj)
	IOObjectRelease(parent_obj);
    return (data);
}

static CFDictionaryRef
getInterface(io_object_t if_obj)
{
    CFBooleanRef			builtin;
    kern_return_t			kr;
    CFDataRef				mac_address = NULL;
    CFStringRef				location;
    CFMutableDictionaryRef		new_if = NULL;
    io_string_t				path;
    CFMutableDictionaryRef		reginfo_if = NULL;
    CFDictionaryRef			ret_dict = NULL;
    CFStringRef				string;
    CFNumberRef				type;
    CFNumberRef				unit;

    kr = IORegistryEntryGetPath(if_obj, kIOServicePlane, path);
    if (kr != KERN_SUCCESS) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME
		    ": IORegistryEntryGetPath returned 0x%x"),
	      kr);
	goto failed;
    }
    kr = IORegistryEntryCreateCFProperties(if_obj,
					   &reginfo_if,
					   NULL,
					   kNilOptions);
    if (kr != KERN_SUCCESS) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME
		    ": IORegistryEntryCreateCFProperties returned 0x%x"),
	      kr);
	goto failed;
    }
    type = isA_CFNumber(CFDictionaryGetValue(reginfo_if,
					     CFSTR(kIOInterfaceType)));
    if (type == NULL) {
	goto failed;
    }
    mac_address = getMacAddress(if_obj);
    if (mac_address == NULL) {
	goto failed;
    }
    builtin = isA_CFBoolean(CFDictionaryGetValue(reginfo_if,
						 CFSTR(kIOBuiltin)));
    if ((builtin == NULL) || !CFBooleanGetValue(builtin)) {
	builtin = isA_CFBoolean(CFDictionaryGetValue(reginfo_if,
						 CFSTR(kIOPrimaryInterface)));
    }
    location = isA_CFString(CFDictionaryGetValue(reginfo_if,
						 CFSTR(kIOLocation)));

    new_if = CFDictionaryCreateMutable(NULL, 0,
				       &kCFTypeDictionaryKeyCallBacks,
				       &kCFTypeDictionaryValueCallBacks);
    if (new_if == NULL) {
	goto failed;
    }
    CFDictionarySetValue(new_if, CFSTR(kIOInterfaceType), type);
    CFDictionarySetValue(new_if, CFSTR(kIOMACAddress), mac_address);
    if (builtin) {
	CFDictionarySetValue(new_if, CFSTR(kIOBuiltin), builtin);
    }
    if (location) {
	CFDictionarySetValue(new_if, CFSTR(kIOLocation), location);
    }
    addCFStringProperty(new_if, kIOPathMatchKey, path);

    unit = isA_CFNumber(CFDictionaryGetValue(reginfo_if,
					     CFSTR(kIOInterfaceUnit)));
    if (unit) {
	CFDictionarySetValue(new_if, CFSTR(kIOInterfaceUnit), unit);
    }
    string = isA_CFString(CFDictionaryGetValue(reginfo_if, CFSTR(kIOBSDNameKey)));
    if (string) {
	CFDictionarySetValue(new_if, CFSTR(kIOBSDNameKey), string);
    }
    ret_dict = new_if;
    new_if = NULL;

 failed:
    if (new_if) {
	CFRelease(new_if);
    }
    if (reginfo_if) {
	CFRelease(reginfo_if);
    }
    if (mac_address) {
	CFRelease(mac_address);
    }
    return (ret_dict);
}

static CFDictionaryRef
lookupIOKitPath(CFStringRef if_path)
{
    CFDictionaryRef		dict = NULL;
    io_registry_entry_t		entry = MACH_PORT_NULL;
    kern_return_t		kr;
    mach_port_t			masterPort = MACH_PORT_NULL;
    io_string_t			path;

    kr = IOMasterPort(bootstrap_port, &masterPort);
    if (kr != KERN_SUCCESS) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME ": IOMasterPort returned 0x%x\n"),
	      kr);
	goto error;
    }
    _SC_cfstring_to_cstring(if_path, path, sizeof(path), kCFStringEncodingASCII);
    entry = IORegistryEntryFromPath(masterPort, path);
    if (entry == MACH_PORT_NULL) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME ": IORegistryEntryFromPath(%@) failed"),
	      if_path);
	goto error;
    }
    dict = getInterface(entry);

 error:
    if (masterPort != MACH_PORT_NULL) {
	mach_port_deallocate(mach_task_self(), masterPort);
    }
    if (entry != MACH_PORT_NULL) {
	IOObjectRelease(entry);
    }
    return (dict);

}

static void
displayInterface(CFDictionaryRef if_dict)
{
    CFStringRef		name;
    CFNumberRef		type;
    CFNumberRef		unit;

    name = CFDictionaryGetValue(if_dict, CFSTR(kIOBSDNameKey));
    if (name) {
	SCPrint(TRUE, stdout, CFSTR("BSD Name: %@\n"), name);
    }

    unit = CFDictionaryGetValue(if_dict, CFSTR(kIOInterfaceUnit));
    if (unit) {
	SCPrint(TRUE, stdout, CFSTR("Unit: %@\n"), unit);
    }

    type = CFDictionaryGetValue(if_dict, CFSTR(kIOInterfaceType));
    SCPrint(TRUE, stdout, CFSTR("Type: %@\n"), type);

    SCPrint(TRUE, stdout, CFSTR("MAC address: "));
    printMacAddress(CFDictionaryGetValue(if_dict, CFSTR(kIOMACAddress)));
    SCPrint(TRUE, stdout, CFSTR("\n"));
}

static void
sort_interfaces_by_unit(CFMutableArrayRef if_list)
{
    int		count = CFArrayGetCount(if_list);
    CFRange	range = CFRangeMake(0, count);

    if (count < 2)
	return;
    CFArraySortValues(if_list, range, if_unit_compare, NULL);
    return;
}

static void
sort_interfaces_by_path(CFMutableArrayRef if_list)
{
    int		count = CFArrayGetCount(if_list);
    CFRange	range = CFRangeMake(0, count);

    if (count < 2)
	return;
    CFArraySortValues(if_list, range, if_path_compare, NULL);
    return;
}

static void
name_interfaces(CFArrayRef if_list)
{
    CFIndex	i;
    CFIndex	n = CFArrayGetCount(if_list);
    CFIndex	i_builtin = 0;
    CFIndex	n_builtin = 0;

    if (S_debug)
	SCPrint(TRUE, stdout, CFSTR("\n"));

    for (i = 0; i < n; i++) {
	CFBooleanRef	builtin;
	CFDictionaryRef if_dict;

	if_dict = CFArrayGetValueAtIndex(if_list, i);
	builtin = CFDictionaryGetValue(if_dict, CFSTR(kIOBuiltin));
	if (builtin && CFBooleanGetValue(builtin)) {
	    n_builtin++;	// reserve unit number for built-in interface
	}
    }

    for (i = 0; i < n; i++) {
	CFDictionaryRef if_dict;
	CFNumberRef	type;
	CFNumberRef	unit;

	if (S_debug) {
	    if (i != 0)
		SCPrint(TRUE, stdout, CFSTR("\n"));
	}

	if_dict = CFArrayGetValueAtIndex(if_list, i);
	unit = CFDictionaryGetValue(if_dict, CFSTR(kIOInterfaceUnit));
	type = CFDictionaryGetValue(if_dict, CFSTR(kIOInterfaceType));
	if (unit) {
	    if (S_debug) {
		SCPrint(TRUE, stdout, CFSTR("Interface already has a unit number\n"));
		displayInterface(if_dict);
	    }
	    replaceInterface(if_dict);
	}
	else {
	    CFDictionaryRef 		dbdict = NULL;
	    kern_return_t		kr = KERN_SUCCESS;
	    CFStringRef			path;
	    CFNumberRef			unit = NULL;

	    path = CFDictionaryGetValue(if_dict, CFSTR(kIOPathMatchKey));
	    dbdict = lookupInterfaceByType(S_dblist, if_dict, NULL);
	    if (dbdict == NULL
		&& pathIsAirPort(path) == TRUE) {
		dbdict = lookupAirPortInterface(S_dblist, NULL);
	    }
	    if (dbdict != NULL) {
		unit = CFDictionaryGetValue(dbdict,
					    CFSTR(kIOInterfaceUnit));
		CFRetain(unit);
	    }
	    else {
		int		if_type;
		boolean_t	is_builtin = FALSE;
		int 		next_unit = 0;

		CFNumberGetValue(type,
				 kCFNumberIntType, &if_type);
		if (if_type == IFT_ETHER) { /* ethernet */
		    CFBooleanRef	builtin;

		    builtin = CFDictionaryGetValue(if_dict,
						   CFSTR(kIOBuiltin));
		    if (builtin && CFBooleanGetValue(builtin)) {
			is_builtin = TRUE;
			next_unit = i_builtin++;
		    }
		    else {
#if defined(__ppc__)
			/* skip over slots reserved for built-in ethernet interface(s) */
			next_unit = n_builtin;
#endif
		    }
		}
		if (is_builtin == FALSE) {
		    unit = getHighestUnitForType(type);
		    if (unit) {
			int	high_unit;

			CFNumberGetValue(unit,
					 kCFNumberIntType, &high_unit);
			if (high_unit >= next_unit) {
			    next_unit = high_unit + 1;
			}
		    }
		}
		unit = CFNumberCreate(NULL,
				      kCFNumberIntType, &next_unit);
	    }
	    if (S_debug) {
		SCPrint(TRUE, stdout, CFSTR("Interface assigned unit %@ %s\n"), unit,
			dbdict ? "(from database)" : "(next available)");
	    }
	    kr = registerInterface(S_connect, path, unit);
	    if (kr != KERN_SUCCESS) {
		SCLog(TRUE, LOG_INFO,
		      CFSTR(MY_PLUGIN_NAME
			    ": failed to name the interface 0x%x"),
		      kr);
		if (S_debug) {
		    displayInterface(if_dict);
		}
	    }
	    else {
		CFDictionaryRef	new_dict;

		path = CFDictionaryGetValue(if_dict,
					    CFSTR(kIOPathMatchKey));
		new_dict = lookupIOKitPath(path);
		if (new_dict != NULL) {
		    CFNumberRef		new_unit;

		    new_unit = CFDictionaryGetValue(new_dict,
						    CFSTR(kIOInterfaceUnit));
		    if (CFEqual(unit, new_unit) == FALSE) {
			SCLog(TRUE, LOG_INFO,
			      CFSTR(MY_PLUGIN_NAME
				    ": interface type %@ assigned "
				    "unit %@ instead of %@"),
			      type, new_unit, unit);
		    }
		    if (S_debug) {
			displayInterface(new_dict);
		    }
		    replaceInterface(new_dict);
		    CFRelease(new_dict);
		}
	    }
	    CFRelease(unit);
	}
    }
    writeInterfaceList(S_dblist);
    return;
}

static void
interfaceArrivalCallback( void * refcon, io_iterator_t iter )
{
    CFMutableArrayRef	if_list = NULL;
    io_object_t  	obj;


    while ((obj = IOIteratorNext(iter))) {
	CFDictionaryRef dict;

	dict = getInterface(obj);
	if (dict) {
	    if (if_list == NULL) {
		if_list = CFArrayCreateMutable(NULL, 0,
					       &kCFTypeArrayCallBacks);
	    }
	    if (if_list)
		CFArrayAppendValue(if_list, dict);
	    CFRelease(dict);
	}
	IOObjectRelease(obj);
    }
    if (if_list) {
	sort_interfaces_by_path(if_list);
	name_interfaces(if_list);
	updateBondConfiguration();
	updateVLANConfiguration();
	CFRelease(if_list);
    }
    return;
}


__private_extern__
void
load_InterfaceNamer(CFBundleRef bundle, Boolean bundleVerbose)
{
    kern_return_t	kr;
    mach_port_t		masterPort = MACH_PORT_NULL;
    io_object_t		stack = MACH_PORT_NULL;

    if (bundleVerbose) {
	S_debug++;
    }

    kr = IOMasterPort(bootstrap_port, &masterPort);
    if (kr != KERN_SUCCESS) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME ": IOMasterPort returned 0x%x"),
	      kr);
	goto error;
    }

    /* synchronize with any drivers that might be loading at boot time */
    waitForQuiet(masterPort);

    stack = createNetworkStackObject(masterPort);
    if (stack == MACH_PORT_NULL) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME ": No network stack object"));
	goto error;
    }
    kr = IOServiceOpen(stack, mach_task_self(), 0, &S_connect);
    if (kr != KERN_SUCCESS) {
	SCPrint(TRUE, stdout, CFSTR(MY_PLUGIN_NAME ": IOServiceOpen returned 0x%x\n"), kr);
	goto error;
    }

    // Creates and returns a notification object for receiving IOKit
    // notifications of new devices or state changes.

    S_notify = IONotificationPortCreate(masterPort);
    if (S_notify == NULL) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME ": IONotificationPortCreate failed"));
	goto error;
    }
    kr = IOServiceAddMatchingNotification(S_notify,
					  kIOFirstMatchNotification,
					  IOServiceMatching("IONetworkInterface"),
					  &interfaceArrivalCallback,
					  (void *) S_notify, /* refCon */
					  &S_iter );         /* notification */

    if (kr != KERN_SUCCESS) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME
		    ": IOServiceAddMatchingNotification returned 0x%x"),
	      kr);
	goto error;
    }

    S_dblist = readInterfaceList();
    if (S_dblist) {
	sort_interfaces_by_unit(S_dblist);
    }
    // Get the current list of matches and arms the notification for
    // future interface arrivals.

    interfaceArrivalCallback((void *) S_notify, S_iter);

    CFRunLoopAddSource(CFRunLoopGetCurrent(),
		       IONotificationPortGetRunLoopSource(S_notify),
		       kCFRunLoopDefaultMode);
    if (stack != MACH_PORT_NULL) {
	IOObjectRelease(stack);
    }
    if (masterPort != MACH_PORT_NULL) {
	mach_port_deallocate(mach_task_self(), masterPort);
    }
    return;
 error:
    if (stack != MACH_PORT_NULL) {
	IOObjectRelease(stack);
    }
    if (masterPort != MACH_PORT_NULL) {
	mach_port_deallocate(mach_task_self(), masterPort);
    }
    if (S_connect != MACH_PORT_NULL) {
	IOServiceClose(S_connect);
	S_connect = MACH_PORT_NULL;
    }
    if (S_iter != MACH_PORT_NULL) {
	IOObjectRelease(S_iter);
	S_iter = MACH_PORT_NULL;
    }
    if (S_notify != MACH_PORT_NULL) {
	IONotificationPortDestroy(S_notify);
    }
    return;
}

//------------------------------------------------------------------------
// Main function.
#ifdef MAIN
int
main(int argc, char ** argv)
{
    load_InterfaceNamer(CFBundleGetMainBundle(),
	 (argc > 1) ? TRUE : FALSE);
    CFRunLoopRun();
    /* not reached */
    exit(0);
    return 0;
}
#endif /* MAIN */
