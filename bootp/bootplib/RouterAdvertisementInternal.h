/*
 * Copyright (c) 2010-2025 Apple Inc. All rights reserved.
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
 * RouterAdvertisementInternal.h
 * - moved from RouterAdvertisement.[ch]
 */

#ifndef _S_ROUTERADVERTISEMENTINTERNAL_H
#define _S_ROUTERADVERTISEMENTINTERNAL_H

#include <CoreFoundation/CFRuntime.h>
#include "ptrlist.h"

#ifdef TEST_DNSDNRINSTANCE

bool
parse_nd_options(ptrlist_t * options_p, const char * buf, int len);

const uint8_t *
find_dnr_option(ptrlist_t * options_p, uint8_t * dnr_data_len_p,
		uint32_t * lifetime_p, int * start_index);

#endif /* TEST_DNSDNRINSTANCE */

struct __RouterAdvertisement {
	CFRuntimeBase	cf_base;

	/*
	 * NOTE: if you add a field, add a line to initialize it.
	 */
	struct in6_addr	source_ip;
	CFStringRef	source_ip_str;
	CFAbsoluteTime	receive_time;
	ptrlist_t	options;
	size_t		ndra_length;
	uint8_t		ndra_buf[1]; /* variable length */
};

#endif /* _S_ROUTERADVERTISEMENTINTERNAL_H */
