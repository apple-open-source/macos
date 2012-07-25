/*
 * Copyright (c) 2000 - 2004 Apple Inc. All rights reserved.
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
 * DHCPServer.c
 */
/* 
 * Modification History
 *
 * November 10, 2000 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <CoreFoundation/CoreFoundation.h>
#include "NICache.h"
#include "NICachePrivate.h"
#include "netinfo.h"
#include "bsdp.h"
#include "DHCPServer.h"

const CFStringRef	kDHCPSPropName = CFSTR(NIPROP_NAME);
const CFStringRef	kDHCPSPropIdentifier = CFSTR(NIPROP_IDENTIFIER);

const CFStringRef	kDHCPSPropDHCPHWAddress = CFSTR(NIPROP_HWADDR);
const CFStringRef	kDHCPSPropDHCPIPAddress = CFSTR(NIPROP_IPADDR);
const CFStringRef	kDHCPSPropDHCPLease = CFSTR(NIPROP_DHCP_LEASE);

const CFStringRef	kDHCPSPropNetBootArch = CFSTR(NIPROP_NETBOOT_ARCH);
const CFStringRef	kDHCPSPropNetBootSysid = CFSTR(NIPROP_NETBOOT_SYSID);
const CFStringRef	kDHCPSPropNetBootLastBootTime = CFSTR(NIPROP_NETBOOT_LAST_BOOT_TIME);
const CFStringRef	kDHCPSPropNetBootIPAddress = CFSTR(NIPROP_IPADDR);
const CFStringRef	kDHCPSPropNetBootImageID = CFSTR(NIPROP_NETBOOT_IMAGE_ID);
const CFStringRef	kDHCPSPropNetBootImageIndex = CFSTR(NIPROP_NETBOOT_IMAGE_INDEX);
const CFStringRef	kDHCPSPropNetBootImageKind = CFSTR(NIPROP_NETBOOT_IMAGE_KIND);
const CFStringRef	kDHCPSPropNetBootImageIsInstall = CFSTR(NIPROP_NETBOOT_IMAGE_IS_INSTALL);

static CFMutableArrayRef
read_host_list(const char * filename)
{
    CFMutableArrayRef		arr = NULL;		
    PLCache_t			cache;
    PLCacheEntry_t * 		scan;

    PLCache_init(&cache);
#define ARBITRARILY_LARGE_NUMBER	(100 * 1024 * 1024)
    PLCache_set_max(&cache, ARBITRARILY_LARGE_NUMBER);
    if (PLCache_read(&cache, filename) == FALSE) {
	return (NULL);
    }

    arr = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (scan = cache.head; scan != NULL; scan = scan->next) {
	int			i;
	CFMutableDictionaryRef	dict = NULL;
	ni_proplist *		pl = &scan->pl;

	if (pl->ni_proplist_len == 0) {
	    continue;
	}
	for (i = 0; i < pl->ni_proplist_len; i++) {
	    CFStringRef		name;
	    ni_property *	prop;
	    CFStringRef		val;

	    prop = pl->nipl_val + i;
	    if (prop->nip_val.ninl_len == 0) {
		continue;
	    }
	    if (dict == NULL) {
		dict = CFDictionaryCreateMutable(NULL, 0,
						 &kCFTypeDictionaryKeyCallBacks,
						 &kCFTypeDictionaryValueCallBacks);
	    }
	    name = CFStringCreateWithCString(NULL, prop->nip_name,
					     kCFStringEncodingUTF8);
	    val = CFStringCreateWithCString(NULL,
					    prop->nip_val.ninl_val[0],
					     kCFStringEncodingUTF8);
	    CFDictionarySetValue(dict, name, val);
	    CFRelease(name);
	    CFRelease(val);
	}
	if (dict != NULL) {
	    CFArrayAppendValue(arr, dict);
	    CFRelease(dict);
	}
    }
    if (CFArrayGetCount(arr) == 0) {
	CFRelease(arr);
	arr = NULL;
    }
    PLCache_free(&cache);
    return (arr);
}

static int
cfstring_to_cstring(CFStringRef cfstr, char * str, int len)
{
    CFIndex		l;
    CFRange		range;

    range = CFRangeMake(0, CFStringGetLength(cfstr));
    (void)CFStringGetBytes(cfstr, range, kCFStringEncodingMacRoman,
			   0, FALSE, (UInt8 *)str, len, &l);
    str[l] = '\0';
    return (l);
}

#ifdef TEST_DHCPHOSTLIST
static void
dump_gregorian_date(CFGregorianDate d)
{
    printf("%d/%d/%d %d:%d:%d\n",
	   (int)d.year, d.month, d.day, d.hour, d.minute, (int)d.second);
    return;
}

static void
show_date(CFAbsoluteTime t)
{
    CFGregorianDate d;
    static CFTimeZoneRef tz = NULL;

    if (tz == NULL) {
#if 1
	tz = CFTimeZoneCopySystem();
#endif
    }

    d = CFAbsoluteTimeGetGregorianDate(t, tz);
    dump_gregorian_date(d);
    return;
}
#endif /* TEST_DHCPHOSTLIST */

static CFArrayRef
cook_for_dhcp(CFArrayRef arr) 
{
    int			count;
    int 		i;
    CFAbsoluteTime 	now_cf;
    struct timeval 	now;

    gettimeofday(&now, 0);
    now_cf = CFAbsoluteTimeGetCurrent();
    
    count = CFArrayGetCount(arr);
    for (i = 0; i < count; i++) {
	char			buf[128];
	CFDateRef		expiration;
	long			lease_val = 0;
	long			lease_delta = 0;
	CFStringRef		lease;
	CFMutableDictionaryRef 	dict = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(arr, i);

	lease = CFDictionaryGetValue(dict, kDHCPSPropDHCPLease);
	if (lease) {
	    cfstring_to_cstring(lease, buf, sizeof(buf));
	    lease_val = strtol(buf, 0, 0);
	    lease_delta = lease_val - now.tv_sec;
#ifdef TEST_DHCPHOSTLIST
	    {
		CFAbsoluteTime		abs_exp;
		abs_exp = lease_delta + now_cf;
		show_date(abs_exp);
	    }
#endif /* TEST_DHCPHOSTLIST */
	    expiration = CFDateCreate(NULL, lease_delta + now_cf);
	    CFDictionarySetValue(dict, kDHCPSPropDHCPLease,
				 expiration);
	    CFRelease(expiration);
	}
    }
    return (arr);
}

static CFArrayRef
cook_for_netboot(CFArrayRef arr) 
{
    int			count;
    int 		i;
    CFAbsoluteTime 	now_cf;
    struct timeval 	now;

    gettimeofday(&now, 0);
    now_cf = CFAbsoluteTimeGetCurrent();
    
    count = CFArrayGetCount(arr);
    for (i = 0; i < count; i++) {
	CFMutableDictionaryRef 	dict;
	char			buf[128];
	CFStringRef		image_id_str;
	uint32_t		image_id;
	CFDateRef		last_boot_time;
	long			last_boot_val = 0;
	long			last_boot_delta = 0;
	CFStringRef		last_boot_time_str;

	dict = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(arr, i);
	last_boot_time_str 
	    = CFDictionaryGetValue(dict, kDHCPSPropNetBootLastBootTime);
	if (last_boot_time_str) {
	    cfstring_to_cstring(last_boot_time_str, buf, sizeof(buf));
	    last_boot_val = strtol(buf, 0, 0);
	    last_boot_delta = last_boot_val - now.tv_sec;
#ifdef TEST_DHCPHOSTLIST
	    {
		CFAbsoluteTime		abs_exp;
		abs_exp = last_boot_delta + now_cf;
		show_date(abs_exp);
	    }
#endif /* TEST_DHCPHOSTLIST */
	    last_boot_time = CFDateCreate(NULL, last_boot_delta + now_cf);
	    CFDictionarySetValue(dict, kDHCPSPropNetBootLastBootTime,
				 last_boot_time);
	    CFRelease(last_boot_time);
	}
	image_id_str = CFDictionaryGetValue(dict, kDHCPSPropNetBootImageID);
	if (image_id_str != NULL) {
	    CFNumberRef		num;
	    uint32_t		image_attrs;
	    uint32_t		image_index;
	    uint32_t		image_kind;

	    cfstring_to_cstring(image_id_str, buf, sizeof(buf));
	    image_id = strtoul(buf, NULL, 0);
	    image_attrs = bsdp_image_attributes(image_id);
	    image_index = bsdp_image_index(image_id);
	    image_kind = bsdp_image_kind_from_attributes(image_attrs);

	    /* set the Index */
	    num = CFNumberCreate(NULL, kCFNumberSInt32Type, &image_index);
	    CFDictionarySetValue(dict, kDHCPSPropNetBootImageIndex, num);
	    CFRelease(num);

	    /* set the Kind */
	    num = CFNumberCreate(NULL, kCFNumberSInt32Type, &image_kind);
	    CFDictionarySetValue(dict, kDHCPSPropNetBootImageKind, num);
	    CFRelease(num);
	    
	    /* set IsInstall */
	    if (bsdp_image_identifier_is_install(image_id)) {
		CFDictionarySetValue(dict, kDHCPSPropNetBootImageIsInstall,
				     kCFBooleanTrue);
	    }
	    else {
		CFDictionarySetValue(dict, kDHCPSPropNetBootImageIsInstall,
				     kCFBooleanFalse);
	    }
	}
    }
    return (arr);
}

CFArrayRef
DHCPSDHCPLeaseListCreate()
{
    CFArrayRef arr;

    arr = read_host_list("/var/db/dhcpd_leases");
    if (arr == NULL) {
	return (NULL);
    }

    if (cook_for_dhcp(arr) == NULL) {
	CFRelease(arr);
	return (NULL);
    }
    return (arr);
}

CFArrayRef
DHCPSNetBootClientListCreate()
{
    CFArrayRef arr;

    arr = read_host_list("/var/db/bsdpd_clients");
    if (arr == NULL) {
	return (NULL);
    }
    if (cook_for_netboot(arr) == NULL) {
	CFRelease(arr);
	return (NULL);
    }
    return (arr);
}

#ifdef TEST_DHCPHOSTLIST
int
main(int argc, char * argv[])
{
    CFArrayRef arr;

    arr = DHCPSDHCPLeaseListCreate();
    if (arr) {
	printf("DHCP Clients\n");
	CFShow(arr);
	CFRelease(arr);
    }
    arr = DHCPSNetBootClientListCreate();
    if (arr) {
	printf("\nNetBoot Clients\n");
	CFShow(arr);
	CFRelease(arr);
    }
    exit(0);
}
#endif /* TEST_DHCPHOSTLIST */
