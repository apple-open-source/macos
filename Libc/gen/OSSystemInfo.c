/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdbool.h>

static int	osi_oid[2] = {-1, 0};

bool
OSSystemInfo(int selector, unsigned long long *resultp)
{
	int oid[3];
	size_t size;

	/*
	 * Check cached OID, look it up if we haven't already.
	 *
	 * NB. Whilst this isn't strictly thread safe, since the
	 *     result as written by any thread will be the same
	 *     there is no actual risk of corruption.
	 */
	if (osi_oid[0] == -1) {
		size = 2;
		if (sysctlnametomib("hw.systeminfo", &osi_oid, &size) ||
		    (size != 2))
			return(false);
	}

	/* build OID */
	oid[0] = osi_oid[0];
	oid[1] = osi_oid[1];
	oid[2] = selector;
	
	/* make the call */
	size = sizeof(*resultp);
	if (sysctl(oid, 3, resultp, &size, NULL, 0) ||
	    (size != sizeof(*resultp)))
		return(false);

	return(true);
}

