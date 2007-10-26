/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "autofs.h"
#include "automount.h"

int
host_is_us(const char *host, size_t hostlen)
{
	char thishost[MAXHOSTNAMELEN];
	static const char localhost[] = "localhost";
	size_t thishost_len;
	char *p;

	/*
	 * This is, by definition, us.
	 */
	if (hostlen == sizeof localhost - 1 &&
	    memcmp(host, localhost, sizeof localhost - 1) == 0)
		return (1);

	/*
	 * Get our hostname, and compare the counted string we were
	 * handed with the host name.
	 */
	gethostname(thishost, MAXHOSTNAMELEN);
	thishost_len = strlen(thishost);
	if (hostlen == thishost_len &&
	    memcmp(host, thishost, thishost_len) == 0)
		return (1);

	/*
	 * Compare the counted string we were handed with the first
	 * component of the host name, if it has more than one component.
	 */
	p = strchr(thishost, '.');
	if (p != NULL) {
		thishost_len = p - thishost;
		if (hostlen == thishost_len &&
		    memcmp(host, thishost, thishost_len) == 0)
			return(1);
	}

	/* Not us. */
	return (0);
}
