/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * magic.c - PPP Magic Number routines.
 *
 * Copyright (c) 1984-2000 Carnegie Mellon University. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any legal
 *    details, please contact
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define RCSID	"$Id: magic.c,v 1.6 2004/03/04 01:36:32 lindak Exp $"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#include "pppd.h"
#include "magic.h"

#ifndef __APPLE__
#define MAGIC_SRANDOM(seedval)  srandom((int)seedval)
#define MAGIC_RANDOM()          random()
#else
#define NO_DRAND48
#define MAGIC_SRANDOM(seedval) /* no need to seed arc4random */
#define MAGIC_RANDOM()         (arc4random() % ((unsigned)RAND_MAX + 1))
#endif

#ifndef lint
static const char rcsid[] = RCSID;
#endif

#ifdef NO_DRAND48
extern long mrand48 __P((void));
extern void srand48 __P((long));
#endif

/*
 * magic_init - Initialize the magic number generator.
 *
 * Attempts to compute a random number seed which will not repeat.
 * The current method uses the current hostid, current process ID
 * and current time, currently.
 */
void
magic_init()
{
    long seed;
    struct timeval t;

    gettimeofday(&t, NULL);
    seed = get_host_seed() ^ t.tv_sec ^ t.tv_usec ^ getpid();
    srand48(seed);
}

/*
 * magic - Returns the next magic number.
 */
u_int32_t
magic()
{
    return (u_int32_t) mrand48();
}

/*
 * random_bytes - Fill a buffer with random bytes.
 */
void
random_bytes(unsigned char *buf, int len)
{
	int i;

	for (i = 0; i < len; ++i) {
		buf[i] = mrand48() >> 24;
		if (buf[i] == '\0')
			// work around a DS API bug that treats a challenge as a C-string instead of byte stream.
			buf[i] = 1;
	}
}

#ifdef NO_DRAND48
/*
 * Substitute procedures for those systems which don't have
 * drand48 et al.
 */

double
drand48()
{
    return (double)MAGIC_RANDOM() / (double)RAND_MAX; /* 2**31-1 */
}

long
mrand48()
{
    return MAGIC_RANDOM();
}

void
srand48(seedval)
long seedval;
{
    MAGIC_SRANDOM(seedval);
}

#endif
