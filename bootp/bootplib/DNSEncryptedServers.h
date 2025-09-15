/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#ifndef DNSEncryptedServers_h
#define DNSEncryptedServers_h

#include <CoreFoundation/CFArray.h>
#include <stdint.h>

#ifndef kSCPropNetDNSEncryptedServers
#define kSCPropNetDNSEncryptedServers CFSTR("EncryptedServers")
#define kSCPropNetDNSEncryptedServerAuthenticationDomainName CFSTR("EncryptedServerName")
#define kSCPropNetDNSEncryptedServerServicePriority CFSTR("EncryptedServerPriority")
#define kSCPropNetDNSEncryptedServerAddresses CFSTR("EncryptedServerAddresses")
#define kSCPropNetDNSEncryptedServerServiceParameters CFSTR("EncryptedServerParameters")
#endif // kSCPropNetDNSEncryptedServers

/*
 * Function: DNSEncryptedServerListCreateWithDHCPv4Data
 *
 * Purpose:
 *	Converts a contiguous chunk of DHCPv4 option data to a well-formed
 *	CFArray<CFDictionary> whose values are valid encrypted servers,
 *	ordered in ascending service priority.
 */
CFArrayRef
DNSEncryptedServerListCreateWithDHCPv4Data(const uint8_t * opt_dnr_data,
					   int opt_dnr_data_len);

/*
 * Function: DNSEncryptedServerEntryCreateWithDHCPv6Data
 *
 * Purpose:
 *	Converts DHCPv6 DNR option data to a well-formed DNS Encrypted Server
 *	entry that can be readily included in a DNS Encrypted Servers list.
 */
CFDictionaryRef
DNSEncryptedServerEntryCreateWithDHCPv6Data(const uint8_t * opt_dnr_data,
					    int opt_dnr_data_len);

/*
 * Function: DNSEncryptedServerCreateWithRAData
 *
 * Purpose:
 *	Converts ND option DNR data to a well-formed DNS Encrypted Server
 *	entry that can be readily included in a DNS Encrypted Servers list.
 */
CFDictionaryRef
DNSEncryptedServerEntryCreateWithRAData(const uint8_t * opt_dnr_data,
					int opt_dnr_data_len);

/*
 * Function: DNSEncryptedServerListCompareEntries
 *
 * Purpose:
 *	Implements the CFComparatorFunction prototype.
 *
 *	The '...ListCreate...' functions declared in this header
 *	return a List<Dictionary<String:<T>>> where T is a CFType (Ref) of
 *	CFNumber, CFString, CFArray and CFData, for a DNS encrypted server's
 *	service priority, authentication domain name, address list, and service
 *	parameters, respectively. This function compares any 2 elements of
 *	such lists for equality. Meant to be used by merger or filter functions.
 *
 * Return:
 *	kCFCompareEqualTo if entry1 is equal to entry2
 *	other CFComparisonResult if not
 */
CFComparisonResult
DNSEncryptedServerListCompareEntries(const void * entry1, const void * entry2,
				     void * context);

/*
 * Function: DNSEncryptedServerListSortByServicePriority
 *
 * Purpose:
 *	Implements the CFComparatorFunction prototype.
 *
 *	Sorts by increasing order of service priority,
 *	i.e. svc_priority 0 is index 0. An error results
 *	in the subject getting sent to the end of the list.
 *	This will reflect on ADN-only resolvers by deprioritizing them,
 *	as others that actually have a service priority will sort ahead.
 */
CFComparisonResult
DNSEncryptedServerListSortByServicePriority(const void * val1,
					    const void * val2,
					    void * context);

void
DNSEncryptedServerListAppendUniqueEntry(CFMutableArrayRef server_entries,
					CFDictionaryRef encrypted_server);

#endif /* DNSEncryptedServers_h */
