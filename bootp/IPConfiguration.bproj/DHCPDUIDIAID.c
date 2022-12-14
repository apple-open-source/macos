/*
 * Copyright (c) 2010-2022 Apple Inc. All rights reserved.
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
 * - routines to set/access the DHCP client DUID and the IAIDs for specific
 *   interfaces
 * - the IA_ID of an interface is its index in the S_IAIDList array
 */

/* 
 * Modification History
 *
 * May 14, 2010
 * - created
 */

#include "ipconfigd_globals.h"
#include "ipconfigd_threads.h"
#include <CoreFoundation/CFString.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>
#include <unistd.h>
#include "util.h"
#include "globals.h"
#include "cfutil.h"
#include "HostUUID.h"
#include "DHCPDUIDIAID.h"

#define DUID_IA_FILE		IPCONFIGURATION_PRIVATE_DIR "/DUID_IA.plist"

#define kDUIDKey		CFSTR("DUID")		/* data */
#define kIAIDListKey		CFSTR("IAIDList")	/* array[string] */
#define kHostUUIDKey		CFSTR("HostUUID")	/* data */

STATIC CFDataRef		S_DUID;
STATIC CFMutableArrayRef	S_IAIDList;

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
	    my_log(LOG_NOTICE,
		   "DHCPDUID: failed to write %s, %s", DUID_IA_FILE,
		   strerror(errno));
	}
    }
    CFRelease(duid_ia);
    return;
}

STATIC DHCPDUIDType
DUID_data_get_type(CFDataRef duid_data)
{
    DHCPDUIDRef		duid = (DHCPDUIDRef)CFDataGetBytePtr(duid_data);
    int			duid_len = (int)CFDataGetLength(duid_data);
    DHCPDUIDType	duid_type;

    if (!DHCPDUIDIsValid(duid, duid_len)) {
	my_log(LOG_NOTICE, "DUID is invalid");
	duid_type = kDHCPDUIDTypeNone;	
    }
    else {
	duid_type = DHCPDUIDGetType(duid);
    }
    return (duid_type);
}

STATIC bool
load_DUID_info(DHCPDUIDType type)
{
    CFDataRef		duid;
    CFDictionaryRef	duid_ia_dict;
    DHCPDUIDType	existing_type;
    CFDataRef		host_uuid;
    CFArrayRef		ia_list;

    duid_ia_dict = my_CFPropertyListCreateFromFile(DUID_IA_FILE);
    if (isA_CFDictionary(duid_ia_dict) == NULL) {
	goto done;
    }

    /* verify that the stored UUID matches this system */
    host_uuid = CFDictionaryGetValue(duid_ia_dict, kHostUUIDKey);
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

    /* verify that the stored DUID has the expected type */
    duid = CFDictionaryGetValue(duid_ia_dict, kDUIDKey);
    if (isA_CFData(duid) == NULL) {
	goto done;
    }
    existing_type = DUID_data_get_type(duid);
    if (existing_type != type) {
	my_log(LOG_NOTICE,
	       "Discarding existing DUID with type %s (%d), need type %s (%d)",
	       DHCPDUIDTypeToString(existing_type), existing_type,
	       DHCPDUIDTypeToString(type), type);
	goto done;
    }

    /* load the ordered interface name list */
    ia_list = CFDictionaryGetValue(duid_ia_dict, kIAIDListKey);
    ia_list = isA_CFArray(ia_list);
    if (ia_list != NULL) {
	CFIndex		count;
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
    S_DUID = CFRetain(duid);
    if (ia_list != NULL) {
	S_IAIDList = CFArrayCreateMutableCopy(NULL, 0, ia_list);
    }

 done:
    my_CFRelease(&duid_ia_dict);
    return (S_DUID != NULL);
}

STATIC CF_RETURNS_RETAINED CFDataRef
make_DUID_LL_data(interface_t * if_p)
{
    return (DHCPDUID_LLDataCreate(if_link_address(if_p),
				  if_link_length(if_p),
				  if_link_arptype(if_p)));
}

STATIC CF_RETURNS_RETAINED CFDataRef
make_DUID_LLT_data(interface_t * if_p)
{
    return (DHCPDUID_LLTDataCreate(if_link_address(if_p),
				   if_link_length(if_p),
				   if_link_arptype(if_p)));
}

STATIC CF_RETURNS_RETAINED CFDataRef
make_DUID_UUID_data(void)
{
    uuid_t		uuid;
    struct timespec	ts = { 0, 0 };

    if (gethostuuid(uuid, &ts) != 0) {
	return (FALSE);
    }
    return (DHCPDUID_UUIDDataCreate(uuid));
}

STATIC CF_RETURNS_RETAINED CFDataRef
make_DUID_data(DHCPDUIDType type)
{
    CFDataRef		data = NULL;
    interface_t *	if_p;
    interface_list_t * 	interfaces;

    interfaces = get_interface_list();
    if (interfaces == NULL) {
	goto done;
    }
    if_p = ifl_find_stable_interface(interfaces);
    if (if_p == NULL) {
	my_log(LOG_NOTICE, "%s: can't find suitable interface",
	       __func__);
	goto done;
    }
    my_log(LOG_NOTICE, "DHCPDUID: chose %s for DUID", if_name(if_p));
    if (type == kDHCPDUIDTypeLL) {
	data = make_DUID_LL_data(if_p);
    }
    else {
	data = make_DUID_LLT_data(if_p);
    }

 done:
    return (data);
}

STATIC void
log_DUID_data(const char * msg, CFDataRef duid)
{
    CFMutableStringRef	str;

    str = CFStringCreateMutable(NULL, 0);
    DHCPDUIDPrintToString(str, (DHCPDUIDRef)CFDataGetBytePtr(duid),
			  CFDataGetLength(duid));
    my_log(LOG_NOTICE, "%s %@", msg, str);
    CFRelease(str);
}

PRIVATE_EXTERN CFDataRef
DHCPDUIDGet(void)
{
    return (S_DUID);
}

PRIVATE_EXTERN CFDataRef
DHCPDUIDEstablishAndGet(DHCPDUIDType type)
{
    if (S_DUID != NULL) {
	goto done;
    }

    /* try to load the DUID from filesystem */
    if (load_DUID_info(type)) {
	goto done;
    }
    if (type == kDHCPDUIDTypeUUID) {
	S_DUID = make_DUID_UUID_data();
    }
    else {
	S_DUID = make_DUID_data(type);
    }
    if (S_DUID == NULL) {
	my_log(LOG_ERR, "%s: failed to establish DUID\n",
	       __func__);
    }
    else {
	log_DUID_data("Established", S_DUID);
	save_DUID_info();
    }

 done:
    return (S_DUID);
}

PRIVATE_EXTERN CFDataRef
DHCPDUIDCopy(interface_t * if_p)
{
    return (make_DUID_LL_data(if_p));
}

PRIVATE_EXTERN DHCPIAID
DHCPIAIDGet(const char * ifname)
{
    CFIndex		count;
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
	iaid = (DHCPIAID)where;
    }
    else {
	CFArrayAppendValue(S_IAIDList, ifname_cf);
	iaid = (DHCPIAID)count;
	save_DUID_info();
    }
    CFRelease(ifname_cf);
    return (iaid);
}

#ifdef TEST_DHCPDUIDIAID

Boolean		G_IPConfiguration_verbose = 1;

interface_list_t *
get_interface_list(void)
{
    STATIC interface_list_t *	S_interfaces;

    if (S_interfaces == NULL) {
	S_interfaces = ifl_init();
    }
    return (S_interfaces);
}

int
main(int argc, char * argv[])
{
    CFDataRef		duid;
    int			i;
    CFMutableStringRef 	str;

    (void) openlog("DHCPDUIDIAID", LOG_PERROR | LOG_PID, LOG_DAEMON);
    ipconfigd_create_paths();
    duid = DHCPDUIDEstablishAndGet(kDHCPDUIDTypeLLT);
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
