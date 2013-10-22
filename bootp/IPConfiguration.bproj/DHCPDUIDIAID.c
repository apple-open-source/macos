/*
 * Copyright (c) 2010-2013 Apple Inc. All rights reserved.
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
 * DHCPDUIDIAID.c
 * - routines to set/access the DHCP client DUID and the IAIDs for particular
 *   interfaces
 */

/* 
 * Modification History
 *
 * May 14, 2010
 * - created
 */

#include <CoreFoundation/CFString.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>
#include <unistd.h>
#include "util.h"
#include "globals.h"
#include "ipconfigd_globals.h"
#include "cfutil.h"
#include "HostUUID.h"
#include "DHCPDUID.h"
#include "DHCPDUIDIAID.h"

#define DUID_IA_FILE		IPCONFIGURATION_PRIVATE_DIR "/DUID_IA.plist"

#define kDUIDKey		CFSTR("DUID")		/* data */
#define kIAIDListKey		CFSTR("IAIDList")	/* array[string] */
#define kHostUUIDKey		CFSTR("HostUUID")	/* data */

STATIC CFDataRef		S_DUID;
STATIC CFMutableArrayRef	S_IAIDList;

/*
 * Function: seconds_since_Jan_1_2000
 * Purpose:
 *   Return the number of seconds since midnight (UTC), January 1, 2000, the
 *   epoch for the DHCP DUID LLT.
 */
STATIC uint32_t
S_seconds_since_Jan_1_2000(void)
{
    time_t		DHCPDUID_epoch;
    uint32_t	      	seconds;
    struct tm		tm;

    bzero(&tm, sizeof(tm));
    tm.tm_year = 100; 	/* 2000 (100 years since 1900) */
    tm.tm_mon = 0;	/* January (0 months since January) */
    tm.tm_mday = 1;	/* 1st (day of the month) */
    DHCPDUID_epoch = timegm(&tm);
    seconds = time(NULL) - DHCPDUID_epoch;
    return (seconds);
}

STATIC void
save_DUID_info(void)
{
    CFMutableDictionaryRef      duid_ia;
    CFDataRef			host_UUID;

    if (S_DUID == NULL) {
	return;
    }
    duid_ia = CFDictionaryCreateMutable(NULL, 0,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(duid_ia, kDUIDKey, S_DUID);
    if (S_IAIDList != NULL) {
	CFDictionarySetValue(duid_ia, kIAIDListKey, S_IAIDList);
    }
    host_UUID = HostUUIDGet();
    if (host_UUID != NULL) {
	CFDictionarySetValue(duid_ia, kHostUUIDKey, host_UUID);
    }
    if (my_CFPropertyListWriteFile(duid_ia, DUID_IA_FILE, 0644) < 0) {
	/*
	 * An ENOENT error is expected on a read-only filesystem.  All 
	 * other errors should be reported.
	 */
	if (errno != ENOENT) {
	    my_log(LOG_ERR,
		   "DHCPDUID: failed to write %s, %s", DUID_IA_FILE,
		   strerror(errno));
	}
    }
    CFRelease(duid_ia);
    return;
}

STATIC bool
load_DUID_info(void)
{
    CFDataRef		duid;
    CFDictionaryRef	duid_ia;
    CFDataRef		host_uuid;
    CFArrayRef		ia_list;

    duid_ia = my_CFPropertyListCreateFromFile(DUID_IA_FILE);
    if (isA_CFDictionary(duid_ia) == NULL) {
	goto done;
    }
    duid = CFDictionaryGetValue(duid_ia, kDUIDKey);
    if (isA_CFData(duid) == NULL) {
	goto done;
    }
    ia_list = CFDictionaryGetValue(duid_ia, kIAIDListKey);
    ia_list = isA_CFArray(ia_list);
    if (ia_list != NULL) {
	int		count;
	int		i;

	count = CFArrayGetCount(ia_list);
	for (i = 0; i < count; i++) {
	    CFStringRef	name = CFArrayGetValueAtIndex(ia_list, i);
	    if (isA_CFString(name) == NULL) {
		/* invalid property */
		ia_list = NULL;
		break;
	    }
	}
    }
    host_uuid = CFDictionaryGetValue(duid_ia, kHostUUIDKey);
    if (isA_CFData(host_uuid) != NULL 
	&& CFDataGetLength(host_uuid) == sizeof(uuid_t)) {
	CFDataRef	our_UUID;

	our_UUID = HostUUIDGet();
	if (our_UUID != NULL && CFEqual(host_uuid, our_UUID) == FALSE) {
	    syslog(LOG_NOTICE,
		   "DHCPDUID: ignoring DUID - host UUID doesn't match");
	    goto done;
	}
    }
    S_DUID = CFRetain(duid);
    if (ia_list != NULL) {
	S_IAIDList = CFArrayCreateMutableCopy(NULL, 0, ia_list);
    }

 done:
    my_CFRelease(&duid_ia);
    return (S_DUID != NULL);
}

STATIC CFDataRef
make_DUID_LL_data(interface_t * if_p)
{
    CFMutableDataRef	data;
    int			duid_len;
    DHCPDUID_LLRef	ll_p;

    duid_len = offsetof(DHCPDUID_LL, linklayer_address) + if_link_length(if_p);
    data = CFDataCreateMutable(NULL, duid_len);
    CFDataSetLength(data, duid_len);
    ll_p = (DHCPDUID_LLRef)CFDataGetMutableBytePtr(data);
    DHCPDUIDSetType((DHCPDUIDRef)ll_p, kDHCPDUIDTypeLL);
    DHCPDUID_LLSetHardwareType(ll_p, if_link_arptype(if_p));
    bcopy(if_link_address(if_p), ll_p->linklayer_address,
	  if_link_length(if_p));
    return (data);
}

STATIC CFDataRef
make_DUID_LLT_data(interface_t * if_p)
{
    CFMutableDataRef	data;
    int			duid_len;
    DHCPDUID_LLTRef	llt_p;

    duid_len = offsetof(DHCPDUID_LLT, linklayer_address) + if_link_length(if_p);
    data = CFDataCreateMutable(NULL, duid_len);
    CFDataSetLength(data, duid_len);
    llt_p = (DHCPDUID_LLTRef)CFDataGetMutableBytePtr(data);
    DHCPDUIDSetType((DHCPDUIDRef)llt_p, kDHCPDUIDTypeLLT);
    DHCPDUID_LLTSetHardwareType(llt_p, if_link_arptype(if_p));
    bcopy(if_link_address(if_p), llt_p->linklayer_address,
	  if_link_length(if_p));
    DHCPDUID_LLTSetTime(llt_p, S_seconds_since_Jan_1_2000());
    return (data);
}

PRIVATE_EXTERN CFDataRef
DHCPDUIDGet(interface_list_t * interfaces)
{
    interface_t *		if_p;
    interface_t *	       	if_with_linkaddr_p = NULL;

    if (S_DUID != NULL) {
	goto done;
    }
    /* try to load the DUID from filesystem */
    if (G_is_netboot == FALSE && load_DUID_info()) {
	goto done;
    }
    if (interfaces == NULL) {
	goto done;
    }
    if_p = ifl_find_name(interfaces, "en0");
    if (if_p == NULL) {
	int		count;
	int		i;

	count = ifl_count(interfaces);
	for (i = 0; i < count; i++) {
	    interface_t *	scan = ifl_at_index(interfaces, i);

	    switch (if_ift_type(scan)) {
	    case IFT_ETHER:
	    case IFT_IEEE1394:
		break;
	    default:
		if (if_with_linkaddr_p == NULL
		    && if_link_length(scan) > 0) {
		    if_with_linkaddr_p = scan;
		}
		continue;
	    }
	    if (if_p == NULL) {
		if_p = scan;
	    }
	    else if (strcmp(if_name(scan), if_name(if_p)) < 0) {
		/* pick "lowest" named interface */
		if_p = scan;
	    }
	}
    }
    if (if_p == NULL) {
	if (G_dhcp_duid_type == kDHCPDUIDTypeLL || if_with_linkaddr_p == NULL) {
	    my_log(LOG_ERR,
		   "DHCPv6Client: can't determine interface for DUID");
	    goto done;
	}
	if_p = if_with_linkaddr_p;
    }
    if (G_IPConfiguration_verbose) {
	my_log(LOG_DEBUG, "DHCPv6Client: chose %s for DUID",
	       if_name(if_p));
    }
    if (G_dhcp_duid_type == kDHCPDUIDTypeLL || G_is_netboot) {
	S_DUID = make_DUID_LL_data(if_p);
    }
    else {
	S_DUID = make_DUID_LLT_data(if_p);
    }
    save_DUID_info();

 done:
    return (S_DUID);
}

PRIVATE_EXTERN DHCPIAID
DHCPIAIDGet(const char * ifname)
{
    int			count;
    DHCPIAID 		iaid;
    CFStringRef		ifname_cf;
    CFIndex		where = kCFNotFound;

    ifname_cf = CFStringCreateWithCString(NULL, ifname, kCFStringEncodingASCII);
    if (S_IAIDList == NULL) {
	S_IAIDList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	count = 0;
    }
    else {
	CFRange 	range;

	count = CFArrayGetCount(S_IAIDList);
	range = CFRangeMake(0, count);
	where = CFArrayGetFirstIndexOfValue(S_IAIDList, range, ifname_cf);
    }
    if (where != kCFNotFound) {
	iaid = where;
    }
    else {
	CFArrayAppendValue(S_IAIDList, ifname_cf);
	iaid = count;
	save_DUID_info();
    }
    CFRelease(ifname_cf);
    return (iaid);
}

#ifdef TEST_DHCPDUIDIAID

Boolean		G_IPConfiguration_verbose = 1;
int		G_dhcp_duid_type;
boolean_t	G_is_netboot;

int
main(int argc, char * argv[])
{
    CFDataRef		duid;
    int			i;
    interface_list_t *	interfaces;
    CFMutableStringRef 	str;

    (void) openlog("DHCPDUIDIAID", LOG_PERROR | LOG_PID, LOG_DAEMON);
    interfaces = ifl_init();

    ipconfigd_create_paths();
    duid = DHCPDUIDGet(interfaces);
    if (duid == NULL) {
	fprintf(stderr, "Couldn't determine DUID\n");
	exit(1);
    }

    str = CFStringCreateMutable(NULL, 0);
    DHCPDUIDPrintToString(str, (const DHCPDUIDRef)CFDataGetBytePtr(duid),
			  CFDataGetLength(duid));
    SCPrint(TRUE, stdout, CFSTR("%@\n"), str);
    CFRelease(str);
    if (argc > 1) {
	for (i = 1; i < argc; i++) {
	    DHCPIAID	iaid;

	    iaid = DHCPIAIDGet(argv[i]);
	    printf("%s = %d\n", argv[i], iaid);
	}
    }
    exit(0);
    return (0);
}

#endif /* TEST_DHCPDUIDIAID */
