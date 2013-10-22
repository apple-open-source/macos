/*
 * Copyright (c) 1999-2001,2005-2008,2010-2011 Apple Inc. All Rights Reserved.
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
 * sslRand.c - Randomness
 */

/* THIS FILE CONTAINS KERNEL CODE */

#include "sslRand.h"
#include "sslDebug.h"
#include <AssertMacros.h>

#ifdef KERNEL

void read_random(void* buffer, u_int numBytes);

#else

#include <TargetConditionals.h>

#ifdef TARGET_OS_EMBEDDED
#include <Security/SecRandom.h>
#else
static
int sslRandMacOSX(void *data, size_t len)
{
    static int random_fd = -1;

    if (random_fd == -1) {
        random_fd = open("/dev/random", O_RDONLY);
        if (random_fd == -1) {
            sslErrorLog("sslRand: error opening /dev/random: %s\n",
                        strerror(errno));
            return -1;
        }
    }

    ssize_t bytesRead = read(random_fd, data, len);
    if (bytesRead != len) {
        sslErrorLog("sslRand: error reading %lu bytes from /dev/random: %s\n",
                    len, strerror(errno));
        serr = -1;
    }

    return serr;
}
#endif /* TARGET_OS_EMBEDDED */

#endif /* KERNEL */

/*
 * Common RNG function.
 */
int sslRand(SSLBuffer *buf)
{
	check(buf != NULL);
	check(buf->data != NULL);

	if(buf->length == 0) {
		sslErrorLog("sslRand: zero buf->length\n");
		return 0;
	}

#ifdef KERNEL
    read_random(buf->data, (u_int)buf->length);
    return 0;
#else
#ifdef TARGET_OS_EMBEDDED
	return SecRandomCopyBytes(kSecRandomDefault, buf->length, buf->data);
#else
    return sslRandMacOSX(ctx, buf);
#endif
#endif
}
