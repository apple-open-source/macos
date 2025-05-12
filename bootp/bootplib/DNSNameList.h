/*
 * Copyright (c) 2005-2025 Apple Inc. All rights reserved.
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
 * DNSNameList.h
 * - convert a list of DNS domain names to/from the compact
 *   DNS form described in RFC 1035
 */

/* 
 * Modification History
 *
 * January 4, 2006	Dieter Siegmund (dieter@apple)
 * - created
 */

#ifndef _S_DNSNAMELIST_H
#define _S_DNSNAMELIST_H

#include <CoreFoundation/CFArray.h>
#include <stdint.h>

/* 
 * Function: DNSNameListBufferCreate
 *
 * Purpose:
 *   Convert the given list of DNS domain names into either of two formats
 *   described in RFC 1035.  If "buffer" is NULL, this routine allocates
 *   a buffer of sufficient size and returns its size in "buffer_size".
 *   Use free() to release the memory.
 *
 *   If "buffer" is not NULL, this routine places at most "buffer_size" 
 *   bytes into "buffer".  If "buffer" is too small, NULL is returned, and
 *   "buffer_size" reflects the number of bytes used in the partial conversion.
 *
 *   If "compact" is true, generates the compact form (RFC 1035 section 4.1.4),
 *   otherwise generates the non-compact form (RFC 1035 section 3.1).
 *   
 * Returns:
 *   NULL if the conversion failed, non-NULL otherwise.
 */
uint8_t *
DNSNameListBufferCreate(const char * names[], int names_count,
			uint8_t * buffer, int * buffer_size, bool compact);

/*
 * Function: DNSNameListDataCreateWithString, DNSNameListDataCreateWithCString
 * Purpose:
 *   Convert a single string to DNS-encoded data.
 *   Preserves the end label, if it is present.
 */
CFDataRef
DNSNameListDataCreateWithString(CFStringRef cfstr);

CFDataRef
DNSNameListDataCreateWithCString(const char * str);

/*
 * Function: DNSNameListDataCreateWithArray
 * Purpose:
 *   Convert an array of strings to DNS-encoded data. If `compact` is true,
 *   uses the compact encoding.
 */
CFDataRef
DNSNameListDataCreateWithArray(CFArrayRef list, bool compact);

/* 
 * Function: DNSNameListCreate
 *
 * Purpose:
 *   Convert compact domain name list form described in RFC 1035 to a list
 *   of domain names.  The memory for the list and names buffer area is
 *   dynamically allocated in a single allocation.  Use free() to release
 *   the memory.
 *
 * Returns:
 *   NULL if an error occurred i.e. buffer did not contain a valid encoding.
 *   non-NULL if the conversion was successful, and "names_count" contains
 *   the number of names in the returned list.
 */
const char * *
DNSNameListCreate(const uint8_t * buffer, int buffer_size, int * names_count);

/*
 * Function: DNSNameListCreateArray
 * Purpose:
 *   Convert compact (or not) domain name list form described in RFC 1035 to an
 *   array of domain name strings.
 *
 *   The names in the strings will *not* have trailing dots.
 */
CFArrayRef /* of CFStringRef */
DNSNameListCreateArray(const uint8_t * buffer, int buffer_size);

/*
 * Function: DNSNameStringCreate
 * Purpose:
 *   Convert domain name in RFC 1035 format to a single string.
 *   If `preserve_end_label` is `true` and the name has an end label,
 *   the returned name will be terminated with a dot ".".
 * Returns:
 *   NULL if failure, non-NULL CFString otherwise.
 */
CFStringRef
DNSNameStringCreate(const uint8_t * buffer, int buffer_size,
		    bool preserve_end_label);

#endif /* _S_DNSNAMELIST_H */
