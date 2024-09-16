/*
 * Copyright (c) 2023 Apple Inc.  All Rights Reserved.
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
 * NetworkInterfaceUtil.c
 * - access to reserved network interface unit numbers
 */

#define __SC_CFRELEASE_NEEDED	1
#include <SystemConfiguration/SCNetworkConfiguration.h>
#include <SystemConfiguration/SCNetworkConfigurationPrivate.h>
#include <SystemConfiguration/SCPrivate.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOBSD.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <SystemConfiguration/SCValidation.h>
#include "NetworkInterfaceUtil.h"

/*
 * Global: S_reserved_units
 * Purpose:
 *   For a given interface prefix i.e. "en", remember the number of
 *   reserved/pre-allocated unit numbers.
 *
 *   A dictionary with key interface prefix (CFStringRef), value an
 *   IFUnit (unsigned int) representing number of units to reserve.
 */
static CFMutableDictionaryRef	S_reserved_units;

static Boolean
IFUnitEqual(const void *ptr1, const void *ptr2)
{
    return (IFUnit)ptr1 == (IFUnit)ptr2;
}

static CFStringRef
IFUnitCopyDescription(const void *ptr)
{
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("%u"), (IFUnit)ptr);
}

static CFDictionaryValueCallBacks
IFUnitValueCallBacks = {
	0, NULL, NULL, IFUnitCopyDescription, IFUnitEqual
};

static Boolean
getReservedUnitsForPrefix(CFStringRef prefix, IFUnit * ret_unit)
{
	Boolean		present = FALSE;
	const void *	val;

	if (S_reserved_units == NULL) {
		goto done;
	}
	present = CFDictionaryGetValueIfPresent(S_reserved_units,
						prefix, &val);
	if (present) {
		*ret_unit = (IFUnit)(uintptr_t)val;
	}
 done:
	return (present);
}

static void
setReservedUnitsForPrefix(CFStringRef prefix, IFUnit how_many)
{
	if (S_reserved_units == NULL) {
		S_reserved_units
			= CFDictionaryCreateMutable(NULL, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &IFUnitValueCallBacks);
	}
	CFDictionarySetValue(S_reserved_units,
			     prefix,
			     (const void *)(uintptr_t)how_many);
}

static CFStringRef
network_interface_unit_string_copy(CFStringRef prefix)
{
	return CFStringCreateWithFormat(NULL, NULL,
					CFSTR("network-interface-unit-%@"),
					prefix);
}

static Boolean
IFUnitRangeInitWithData(CFDataRef data, IFUnitRangeRef unit_range)
{
	unsigned int 	how_many;
	unsigned int	length;
	const void *	ptr;
	Boolean		success = FALSE;

	/*
	 * The format of the data is an array of up to two uint32_t's.
	 * The first element is the first unit to reserve. The second
	 * element is the last unit to reserve.
	 */
	length = (unsigned int)CFDataGetLength(data);
	how_many = length / 4;
	if ((length % 4) != 0 || how_many == 0) {
		SC_log(LOG_ERR, "%s: bad length %u",
		       __func__, length);
		goto done;
	}
	bzero(unit_range, sizeof(*unit_range));
	ptr = CFDataGetBytePtr(data);
	if (how_many > 2) {
		/* ignore all but the first two elements */
		how_many = 2;
	}
	bcopy(ptr, &unit_range->start, how_many * sizeof(unit_range->start));
	if (how_many == 2) {
		if (unit_range->end < unit_range->start) {
			SC_log(LOG_ERR, "%s: unit end %u < start %u",
			       __func__, unit_range->end, unit_range->start);
			goto done;
		}
	}
	else {
		/* single element */
		unit_range->end = unit_range->start;
	}
	success = TRUE;
 done:
	return (success);
}

static io_registry_entry_t
getRegistryEntryWithID(uint64_t entryID)
{
	io_registry_entry_t	entry = MACH_PORT_NULL;
	io_iterator_t		iterator = MACH_PORT_NULL;
	kern_return_t		kr;
	
	kr = IOServiceGetMatchingServices(kIOMainPortDefault,
					  IORegistryEntryIDMatching(entryID),
					  &iterator);
	if (kr != KERN_SUCCESS || iterator == MACH_PORT_NULL) {
		SC_log(LOG_NOTICE,
		       "%s: can't find entry 0x%llx, %d",
		       __func__, entryID, kr);
		goto done;
	}
	entry = IOIteratorNext(iterator);
	if (entry == MACH_PORT_NULL) {
		SC_log(LOG_NOTICE,
		       "%s: no such matching entryID 0x%llx",
		       __func__, entryID);
	}
 done:
	if (iterator != MACH_PORT_NULL) {
		IOObjectRelease(iterator);
	}
	return (entry);
}


#define ITERATE_PARENTS 	(kIORegistryIterateRecursively	\
				 | kIORegistryIterateParents)

Boolean
NetworkInterfaceGetReservedRange(SCNetworkInterfaceRef netif,
				 IFUnitRangeRef unit_range)
{
	CFDataRef		data = NULL;
	io_registry_entry_t	obj = MACH_PORT_NULL;
	uint64_t		entryID;
	CFStringRef		propname = NULL;
	CFStringRef		prefix;
	Boolean			success = FALSE;

	prefix = _SCNetworkInterfaceGetIOInterfaceNamePrefix(netif);
	if (prefix == NULL) {
		SC_log(LOG_NOTICE, "%s: %@ has no prefix",
		       __func__, netif);
		goto done;
	}
	entryID = _SCNetworkInterfaceGetIORegistryEntryID(netif);
	obj = getRegistryEntryWithID(entryID);
	propname = network_interface_unit_string_copy(prefix);
	data = (CFDataRef)
		IORegistryEntrySearchCFProperty(obj,
						kIOServicePlane,
						propname,
						NULL,
						ITERATE_PARENTS);
	if (isA_CFData(data) == NULL) {
		goto done;
	}
	if (IFUnitRangeInitWithData(data, unit_range)) {
		success = TRUE;
	}

 done:
	if (obj != MACH_PORT_NULL) {
		IOObjectRelease(obj);
	}
	__SC_CFRELEASE(data);
	__SC_CFRELEASE(propname);
	return (success);
}

IFUnit
NetworkInterfacePrefixGetReservedUnits(CFStringRef prefix)
{
	kern_return_t	kr;
	io_iterator_t	iterator = MACH_PORT_NULL;
	IFUnit		max_unit = 0;
	Boolean		max_unit_set = FALSE;
	CFStringRef	propname = NULL;
	IFUnit		ret_units = 0;
	Boolean		update_cache = TRUE;

	if (getReservedUnitsForPrefix(prefix, &ret_units)) {
		/* cached value */
#ifdef TEST_NETWORKINTERFACEUTIL
		printf("Cached value %u\n", ret_units);
#endif
		update_cache = FALSE;
		goto done;
	}
	propname = network_interface_unit_string_copy(prefix);
	kr = IORegistryCreateIterator(kIOMainPortDefault,
				      kIODeviceTreePlane,
				      kIORegistryIterateRecursively,
				      &iterator);
	if (kr != KERN_SUCCESS) {
		SC_log(LOG_NOTICE,
		       "IORegistryCreateIterator failed %d", kr);
		goto done;
	}
	while (1) {
		CFDataRef		data;
		io_registry_entry_t	obj;
		IFUnitRange		unit_range;

		obj = IOIteratorNext(iterator);
		if (obj == MACH_PORT_NULL) {
			break;
		}
		data = (CFDataRef)
			IORegistryEntryCreateCFProperty(obj,
							propname,
							NULL,
							0);
		if (data == NULL) {
			continue;
		}
		if (isA_CFData(data) != NULL
		    && IFUnitRangeInitWithData(data, &unit_range)
		    && (!max_unit_set || unit_range.end > max_unit)) {
			max_unit = unit_range.end;
			max_unit_set = TRUE;
		}
		CFRelease(data);
	}
 done:
	if (max_unit_set) {
		ret_units = max_unit + 1;
	}
	if (iterator != MACH_PORT_NULL) {
		IOObjectRelease(iterator);
	}
	__SC_CFRELEASE(propname);
	if (update_cache) {
		setReservedUnitsForPrefix(prefix, ret_units);
	}
	return (ret_units);
}

#ifdef TEST_NETWORKINTERFACEUTIL
__dead2
static void
usage(const char * progname)
{
	fprintf(stderr, "%s: [ <ifname> ]\n", progname);
	exit(2);
}

int
main(int argc, char * argv[])
{
	const char *	ifname = NULL;

	switch (argc) {
	case 1:
		break;
	case 2:
		ifname = argv[1];
		break;
	default:
		usage(argv[0]);
	}
	if (ifname == NULL) {
		uint32_t	how_many;

		how_many = NetworkInterfacePrefixGetReservedUnits(CFSTR("en"));
		printf("There are %d reserved en units\n",
		       how_many);
		/* call it again to test the cached path */
		how_many = NetworkInterfacePrefixGetReservedUnits(CFSTR("en"));
		printf("There are %d reserved en units\n",
		       how_many);
	}
	else {
		CFStringRef		ifname_cf;
		SCNetworkInterfaceRef	netif;
		Boolean			success;
		IFUnitRange		unit_range;

		ifname_cf = CFStringCreateWithCString(NULL, ifname,
						      kCFStringEncodingUTF8);
		netif = _SCNetworkInterfaceCreateWithBSDName(NULL, ifname_cf,
							     0);
		CFRelease(ifname_cf);
		if (netif == NULL) {
			fprintf(stderr,
				"Can't allocate network interface, %s\n",
				SCErrorString(SCError()));
			exit(2);
				
		}
		success = NetworkInterfaceGetReservedRange(netif, &unit_range);
		CFRelease(netif);
		if (!success) {
			fprintf(stderr, "Can't find unit for %s\n",
				ifname);
			exit(2);
		}
		if (unit_range.start == unit_range.end) {
			printf("Unit %d\n", unit_range.start);
		}
		else {
			printf("Units %d .. %d\n",
			       unit_range.start, unit_range.end);
		}
	}
	exit(0);
	return (0);
}
#endif /* TEST_NETWORKINTERFACEUTIL */
