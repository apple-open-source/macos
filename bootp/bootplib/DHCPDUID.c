/*
 * Copyright (c) 2009-2022 Apple Inc. All rights reserved.
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
 * DHCPDUID.c
 */

/* 
 * Modification History
 *
 * September 30, 2009		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include "DHCPDUID.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <CoreFoundation/CFString.h>
#include "util.h"
#include "cfutil.h"
#include "symbol_scope.h"

PRIVATE_EXTERN const char *
DHCPDUIDTypeToString(DHCPDUIDType type)
{
    static const char * str[] = {
				 "none",
				 "LLT",
				 "EN",
				 "LL",
				 "UUID"
    };
    unsigned int	str_count = countof(str);

    if (type < str_count) {
	return (str[type]);
    }
    return ("<unknown>");
}

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
    seconds = (uint32_t)(time(NULL) - DHCPDUID_epoch);
    return (seconds);
}

PRIVATE_EXTERN void
DHCPDUIDPrintToString(CFMutableStringRef str,
		      const DHCPDUIDRef duid, int duid_len)
{
    int			required_len;
    DHCPDUIDType	type;
    uuid_string_t	uuid_str;

    required_len = offsetof(DHCPDUID_LLT, hardware_type);
    if (duid_len < required_len) {
	goto too_short;
    }
    type = DHCPDUIDGetType(duid);
    switch (type) {
    case kDHCPDUIDTypeLLT:
	required_len = offsetof(DHCPDUID_LLT, linklayer_address);
	if (duid_len < required_len) {
	    goto too_short;
	}
	CFStringAppendFormat(str, 
			     NULL,
			     CFSTR("DUID LLT HW %d Time %u Addr "),
			     DHCPDUID_LLTGetHardwareType(&duid->llt),
			     DHCPDUID_LLTGetTime(&duid->llt));
	my_CFStringAppendBytesAsHex(str, duid->llt.linklayer_address,
				    duid_len - required_len, ':');
	break;
    case kDHCPDUIDTypeEN:
	required_len = offsetof(DHCPDUID_EN, identifier);
	if (duid_len < required_len) {
	    goto too_short;
	}
	CFStringAppendFormat(str, NULL, CFSTR("DUID EN Num %d Id "),
			     DHCPDUID_ENGetEnterpriseNumber(&duid->en));
	my_CFStringAppendBytesAsHex(str, duid->en.identifier,
				    duid_len - required_len, ':');
	break;
    case kDHCPDUIDTypeLL:
	required_len = offsetof(DHCPDUID_LL, linklayer_address);
	if (duid_len < required_len) {
	    goto too_short;
	}
	CFStringAppendFormat(str, NULL,
			     CFSTR("DUID LL HW %d Addr "),
			     DHCPDUID_LLGetHardwareType(&duid->ll));
	my_CFStringAppendBytesAsHex(str, duid->ll.linklayer_address,
				    duid_len - required_len, ':');
	break;
    case kDHCPDUIDTypeUUID:
	required_len = sizeof(duid->uuid);
	if (duid_len < required_len) {
	    goto too_short;
	}
	uuid_unparse(duid->uuid.uuid, uuid_str);
	CFStringAppendFormat(str, NULL,
			     CFSTR("DUID UUID %s"),
			     uuid_str);
	break;
    default:
	CFStringAppendFormat(str, NULL, CFSTR("DUID (unrecognized type=%d): "),
			     type);
	my_CFStringAppendBytesAsHex(str, (const uint8_t *)duid, duid_len, ' ');
	break;
    }
    return;

 too_short:
    CFStringAppendFormat(str, NULL,
			 CFSTR("DUID too short (%d < %d), Data = { "),
			 duid_len, required_len);
    my_CFStringAppendBytesAsHex(str, (const uint8_t *)duid, duid_len, ' ');
    CFStringAppendCString(str, " }", kCFStringEncodingASCII);
    return;
}

PRIVATE_EXTERN bool
DHCPDUIDIsValid(const DHCPDUIDRef duid, int duid_len)
{
    int			required_len;
    DHCPDUIDType	type;
    bool		valid = false;

    required_len = offsetof(DHCPDUID_LLT, hardware_type);
    if (duid_len < required_len) {
	goto done;
    }
    type = DHCPDUIDGetType(duid);
    switch (type) {
    case kDHCPDUIDTypeLLT:
	required_len = offsetof(DHCPDUID_LLT, linklayer_address) + 1;
	break;
    case kDHCPDUIDTypeEN:
	required_len = offsetof(DHCPDUID_EN, identifier) + 1;
	break;
    case kDHCPDUIDTypeLL:
	required_len = offsetof(DHCPDUID_LL, linklayer_address) + 1;
	break;
    case kDHCPDUIDTypeUUID:
	required_len = sizeof(duid->uuid);
	break;
    default:
	goto done;
    }
    valid = (duid_len >= required_len);

 done:
    return (valid);
}

PRIVATE_EXTERN CFDataRef
DHCPDUID_LLDataCreate(const void * ll_addr, int ll_len, int ll_type)
{
    CFMutableDataRef	data;
    int			duid_len;
    DHCPDUID_LLRef	ll_p;

    duid_len = offsetof(DHCPDUID_LL, linklayer_address) + ll_len;
    data = CFDataCreateMutable(NULL, duid_len);
    CFDataSetLength(data, duid_len);
    ll_p = (DHCPDUID_LLRef)CFDataGetMutableBytePtr(data);
    DHCPDUIDSetType((DHCPDUIDRef)ll_p, kDHCPDUIDTypeLL);
    DHCPDUID_LLSetHardwareType(ll_p, ll_type);
    memcpy(ll_p->linklayer_address, ll_addr, ll_len);
    return (data);
}

PRIVATE_EXTERN CFDataRef
DHCPDUID_LLTDataCreate(const void * ll_addr, int ll_len, int ll_type)
{
    CFMutableDataRef	data;
    int			duid_len;
    DHCPDUID_LLTRef	llt_p;

    duid_len = offsetof(DHCPDUID_LLT, linklayer_address) + ll_len;
    data = CFDataCreateMutable(NULL, duid_len);
    CFDataSetLength(data, duid_len);
    llt_p = (DHCPDUID_LLTRef)CFDataGetMutableBytePtr(data);
    DHCPDUIDSetType((DHCPDUIDRef)llt_p, kDHCPDUIDTypeLLT);
    DHCPDUID_LLTSetHardwareType(llt_p, ll_type);
    memcpy(llt_p->linklayer_address, ll_addr, ll_len);
    DHCPDUID_LLTSetTime(llt_p, S_seconds_since_Jan_1_2000());
    return (data);
}

PRIVATE_EXTERN CFDataRef
DHCPDUID_UUIDDataCreate(const uuid_t uuid)
{
    CFMutableDataRef	data;
    int			duid_len;
    DHCPDUID_UUIDRef	uuid_p;

    duid_len = sizeof(DHCPDUID_UUID);
    data = CFDataCreateMutable(NULL, duid_len);
    CFDataSetLength(data, duid_len);
    uuid_p = (DHCPDUID_UUIDRef)CFDataGetMutableBytePtr(data);
    DHCPDUIDSetType((DHCPDUIDRef)uuid_p, kDHCPDUIDTypeUUID);
    uuid_copy(uuid_p->uuid, uuid);
    return (data);
}

#ifdef TEST_DHCPDUID

#include <net/if_arp.h>
#include <SystemConfiguration/SCPrivate.h>

int
main(int argc, char * argv[])
{
    CFDataRef		duid_ll;
    CFDataRef		duid_llt;
    CFDataRef		duid_uuid;
    uint8_t		lladdr[6] = { 0, 1, 2, 3, 4, 5 };
    CFMutableStringRef	str;
    struct timespec	ts = { 0, 0 };
    uuid_t		uuid;

    duid_ll = DHCPDUID_LLDataCreate(lladdr, sizeof(lladdr), ARPHRD_ETHER);
    duid_llt = DHCPDUID_LLTDataCreate(lladdr, sizeof(lladdr), ARPHRD_ETHER);
    if (gethostuuid(uuid, &ts) != 0) {
	perror("gethostuuid");
	exit(1);
    }
    duid_uuid = DHCPDUID_UUIDDataCreate(uuid);
    str = CFStringCreateMutable(NULL, 0);
    DHCPDUIDPrintToString(str, (DHCPDUIDRef)CFDataGetBytePtr(duid_ll),
			  CFDataGetLength(duid_ll));
    STRING_APPEND_STR(str, "\n");
    DHCPDUIDPrintToString(str, (DHCPDUIDRef)CFDataGetBytePtr(duid_llt),
			  CFDataGetLength(duid_llt));
    STRING_APPEND_STR(str, "\n");
    DHCPDUIDPrintToString(str, (DHCPDUIDRef)CFDataGetBytePtr(duid_uuid),
			  CFDataGetLength(duid_uuid));
    STRING_APPEND_STR(str, "\n");
    SCPrint(TRUE, stdout, CFSTR("%@"), str);
    CFRelease(str);
    exit(0);
}

#endif /* TEST_DHCPDUID */
