/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Modification History
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
#include <sys/stat.h>
#include <sys/param.h>
#include <mach/mach.h>
#include <net/if_types.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDPlugin.h>
#include <SystemConfiguration/SCPrivate.h>	// for SCLog()
#include <SystemConfiguration/SCValidation.h>
#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>

#define MY_PLUGIN_NAME			"InterfaceNamer"

#define kBSDName			"BSD Name"
#define kIOInterfaceUnit		"IOInterfaceUnit"
#define kIOInterfaceType		"IOInterfaceType"
#define kIOMACAddress			"IOMACAddress"
#define kIOPrimaryInterface		"IOPrimaryInterface"
#define kIONetworkStackUserCommand     "IONetworkStackUserCommand"
#define kIORegisterOne                 1

static boolean_t			S_debug = FALSE;
static CFMutableArrayRef		S_dblist = NULL;
static io_connect_t			S_connect = NULL;
static io_iterator_t			S_iter = NULL;
static IONotificationPortRef		S_notify = NULL;

static void
displayInterface(CFDictionaryRef if_dict);

static CFDictionaryRef
lookupIOKitPath(CFStringRef if_path);

static int
cfstring_to_cstring(CFStringRef cfstr, char * str, int len)
{
    CFIndex		l;
    CFIndex		n;
    CFRange		range;

    range = CFRangeMake(0, CFStringGetLength(cfstr));
    n = CFStringGetBytes(cfstr, range, kCFStringEncodingMacRoman,
			 0, FALSE, str, len, &l);
    str[l] = '\0';
    return (l);
}

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
if_type_compare(const void *val1, const void *val2, void *context)
{
    CFDataRef		addr1;
    CFDataRef		addr2;
    CFComparisonResult	res;
    CFNumberRef		type1;
    CFNumberRef		type2;

    type1 = CFDictionaryGetValue((CFDictionaryRef)val1,
				 CFSTR(kIOInterfaceType));
    type2 = CFDictionaryGetValue((CFDictionaryRef)val2,
				 CFSTR(kIOInterfaceType));
    res = CFNumberCompare(type1, type2, NULL);
    if (res != kCFCompareEqualTo) {
	return (res);
    }
    addr1 = CFDictionaryGetValue((CFDictionaryRef)val1,
				 CFSTR(kIOMACAddress));
    addr2 = CFDictionaryGetValue((CFDictionaryRef)val2,
				 CFSTR(kIOMACAddress));
    res = compareMacAddress(addr1, addr2);
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

static void
CFStringShow(CFStringRef object)
{
    const char * c = CFStringGetCStringPtr(object,
					   kCFStringEncodingMacRoman);
    if (c) {
	printf("%s\n", c);
    }
    else {
	CFIndex bufferSize = CFStringGetLength(object) + 1;
	char *  buffer     = (char *) malloc(bufferSize);

	if (buffer) {
	    if (CFStringGetCString(object, buffer, bufferSize,
				   kCFStringEncodingMacRoman)) {
		printf("%s\n", buffer);
	    }
	    free(buffer);
	}
    }
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

static void
write_file(char * filename, void * data, size_t data_length)
{
    char		path[MAXPATHLEN];
    int			fd = -1;

    snprintf(path, sizeof(path), "%s-", filename);
    fd = open(path, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (fd < 0) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME ": open(%s) failed, %s"),
	      filename, strerror(errno));
	goto done;
    }

    if (write(fd, data, data_length) != data_length) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME ": write %s failed, %s"),
	      filename, strerror(errno));
	goto done;
    }
    rename(path, filename);
 done:
    if (fd >= 0) {
	close(fd);
    }
    return;
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

static void
writePropertyList(char * filename, CFPropertyListRef plist)
{
    CFDataRef	data;

    if (plist == NULL)
	return;

    data = CFPropertyListCreateXMLData(NULL, S_dblist);
    if (data == NULL) {
	return;
    }
    write_file(filename, (void *)CFDataGetBytePtr(data), CFDataGetLength(data));
    CFRelease(data);
    return;
}

#define NETWORK_INTERFACES_FILE	"/var/db/NetworkInterfaces.xml"

static CFMutableArrayRef
readInterfaceList()
{
    CFPropertyListRef 	plist;

    plist = readPropertyList(NETWORK_INTERFACES_FILE);
    if (plist == NULL) {
	return (NULL);
    }
    if (isA_CFArray(plist) == FALSE) {
	CFRelease(plist);
	return (NULL);
    }
    return ((CFMutableArrayRef)plist);
}


static void
writeInterfaceList()
{
    if (S_dblist)
	writePropertyList(NETWORK_INTERFACES_FILE, S_dblist);
    return;
}

#define INDEX_BAD	(-1)

CFDictionaryRef
lookupInterfaceByType(CFArrayRef list, CFDictionaryRef if_dict, int * where)
{
    CFDataRef	addr;
    int 	i;
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

    for (i = 0; i < CFArrayGetCount(list); i++) {
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

CFDictionaryRef
lookupInterfaceByUnit(CFArrayRef list, CFDictionaryRef if_dict, int * where)
{
    int 	i;
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

    for (i = 0; i < CFArrayGetCount(list); i++) {
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

static __inline__ boolean_t
pathIsAirPort(CFStringRef path)
{
    CFRange r;

    r = CFStringFind(path, kAirPortDriverPath, 0);

    if (r.location == kCFNotFound) {
	return (FALSE);
    }
    return (TRUE);
}

CFDictionaryRef
lookupAirPortInterface(CFArrayRef list, int * where)
{
    int 	i;

    if (where) {
	*where = INDEX_BAD;
    }
    if (list == NULL) {
	return (NULL);
    }
    for (i = 0; i < CFArrayGetCount(list); i++) {
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
    int 		i;
    CFNumberRef		if_type;
    CFNumberRef		if_unit;
    CFComparisonResult	res;

    if_type = CFDictionaryGetValue(if_dict, CFSTR(kIOInterfaceType));
    if_unit = CFDictionaryGetValue(if_dict, CFSTR(kIOInterfaceUnit));
    for (i = 0; i < CFArrayGetCount(list); i++) {
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
    CFNumberRef		ret_unit = NULL;

    if (S_dblist == NULL)
	return (NULL);
    for (i = CFArrayGetCount(S_dblist) - 1; i >= 0; i--) {
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
    kern_return_t           	kr = kIOReturnNoMemory;

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
    io_iterator_t	iter = NULL;
    kern_return_t	kr;
    io_object_t		stack = NULL;

    kr = IOServiceGetMatchingServices(masterPort,
				      IOServiceMatching("IONetworkStack"),
				      &iter);
    if (iter != NULL) {
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
    int i;

    for (i = 0; i < CFDataGetLength(data); i++) {
	if (i != 0) printf(":");
	printf("%02x", CFDataGetBytePtr(data)[i]);
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
    io_object_t			parent_obj = NULL;

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
    kern_return_t			kr;
    CFDataRef				mac_address = NULL;
    CFMutableDictionaryRef		new_if = NULL;
    io_string_t				path;
    CFBooleanRef			primary;
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
    primary = isA_CFBoolean(CFDictionaryGetValue(reginfo_if,
						 CFSTR(kIOPrimaryInterface)));
    new_if = CFDictionaryCreateMutable(NULL, 0,
				       &kCFTypeDictionaryKeyCallBacks,
				       &kCFTypeDictionaryValueCallBacks);
    if (new_if == NULL) {
	goto failed;
    }
    CFDictionarySetValue(new_if, CFSTR(kIOInterfaceType), type);
    CFDictionarySetValue(new_if, CFSTR(kIOMACAddress), mac_address);
    if (primary) {
	CFDictionarySetValue(new_if, CFSTR(kIOPrimaryInterface), primary);
    }
    addCFStringProperty(new_if, kIOPathMatchKey, path);

    unit = isA_CFNumber(CFDictionaryGetValue(reginfo_if,
					     CFSTR(kIOInterfaceUnit)));
    if (unit) {
	CFDictionarySetValue(new_if, CFSTR(kIOInterfaceUnit), unit);
    }
    string = isA_CFString(CFDictionaryGetValue(reginfo_if, CFSTR(kBSDName)));
    if (string) {
	CFDictionarySetValue(new_if, CFSTR(kBSDName), string);
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
    kern_return_t           	kr;
    mach_port_t			masterPort = MACH_PORT_NULL;
    io_string_t			path;

    kr = IOMasterPort(bootstrap_port, &masterPort);
    if (kr != KERN_SUCCESS) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME ": IOMasterPort returned 0x%x\n"),
	      kr);
	goto error;
    }
    cfstring_to_cstring(if_path, path, sizeof(path));
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
    int 		type_val;
    CFNumberRef		unit;
    int 		unit_val;

    name = CFDictionaryGetValue(if_dict, CFSTR(kBSDName));
    if (name) {
	printf("BSD Name: ");
	CFStringShow(name);
    }
    type = CFDictionaryGetValue(if_dict, CFSTR(kIOInterfaceType));
    unit = CFDictionaryGetValue(if_dict, CFSTR(kIOInterfaceUnit));
    if (unit) {
	CFNumberGetValue(unit, kCFNumberIntType, &unit_val);
	printf("Unit: %d\n", unit_val);
    }

    CFNumberGetValue(type,
		     kCFNumberIntType, &type_val);
    printf("Type: %d\n", type_val);
    printf("MAC address: ");
    printMacAddress(CFDictionaryGetValue(if_dict, CFSTR(kIOMACAddress)));
    printf("\n");
}

static void
sort_interfaces_by_type(CFMutableArrayRef if_list)
{
    int		count = CFArrayGetCount(if_list);
    CFRange	range = CFRangeMake(0, count);

    if (count < 2)
	return;
    CFArraySortValues(if_list, range, if_type_compare, NULL);
    return;
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
name_interfaces(CFArrayRef if_list)
{
    int i;

    if (S_debug)
	printf("\n");
    for (i = 0; i < CFArrayGetCount(if_list); i++) {
	CFDictionaryRef if_dict;
	CFNumberRef	type;
	CFNumberRef	unit;

	if (S_debug) {
	    if (i != 0)
		printf("\n");
	}

	if_dict = CFArrayGetValueAtIndex(if_list, i);
	unit = CFDictionaryGetValue(if_dict, CFSTR(kIOInterfaceUnit));
	type = CFDictionaryGetValue(if_dict, CFSTR(kIOInterfaceType));
	if (unit) {
	    if (S_debug) {
		printf("Interface already has a unit number\n");
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
		boolean_t	is_primary = FALSE;
		int 		next_unit = 0;

		CFNumberGetValue(type,
				 kCFNumberIntType, &if_type);
		if (if_type == IFT_ETHER) { /* ethernet */
		    CFBooleanRef	primary;

		    primary = CFDictionaryGetValue(if_dict,
						   CFSTR(kIOPrimaryInterface));
		    if (primary && CFBooleanGetValue(primary)) {
			is_primary = TRUE;
		    }
		    else {
#if defined(__ppc__)
			next_unit = 1; /* reserve 0 for primary ethernet */
#endif
		    }
		}
		if (is_primary == FALSE) {
		    unit = getHighestUnitForType(type);
		    if (unit) {
			CFNumberGetValue(unit,
					 kCFNumberIntType, &next_unit);
			next_unit++;
		    }
		}
		unit = CFNumberCreate(NULL,
				      kCFNumberIntType, &next_unit);
	    }
	    if (S_debug) {
		int u;
		CFNumberGetValue(unit,
				 kCFNumberIntType, &u);
		printf("Interface assigned unit %d %s\n", u,
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
    writeInterfaceList();
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
	sort_interfaces_by_type(if_list);
	name_interfaces(if_list);
	CFRelease(if_list);
    }
    return;
}


void
load(CFBundleRef bundle, Boolean bundleVerbose)
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
    if (stack == NULL) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR(MY_PLUGIN_NAME ": No network stack object"));
	goto error;
    }
    kr = IOServiceOpen(stack, mach_task_self(), 0, &S_connect);
    if (kr != KERN_SUCCESS) {
	printf(MY_PLUGIN_NAME ": IOServiceOpen returned 0x%x\n", kr);
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
    if (S_connect != NULL) {
	IOServiceClose(S_connect);
	S_connect = NULL;
    }
    if (S_iter != NULL) {
	IOObjectRelease(S_iter);
	S_iter = NULL;
    }
    if (S_notify != NULL) {
	IONotificationPortDestroy(S_notify);
    }
    return;
}

//------------------------------------------------------------------------
// Main function.
#ifdef TEST_IFNAMER
int
main(int argc, char ** argv)
{
    load(CFBundleGetMainBundle(),
	 (argc > 1) ? TRUE : FALSE);
    CFRunLoopRun();
    /* not reached */
    exit(0);
    return 0;
}

#endif TEST_IFNAMER
