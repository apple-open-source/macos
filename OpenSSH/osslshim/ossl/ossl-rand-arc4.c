/*
 * Copyright (c) 2011-12 Apple Inc. All Rights Reserved.
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

#include "ossl-config.h"


#include <stdio.h>
#include <stdlib.h>

#include "ossl-rand.h"
/* #include "rk-roken.h" */
#include "ossl-randi.h"

/*
 * ARC4RANDOM(3)
 */
static void
arc4_seed(const void *indata, int size)
{
	arc4random_addrandom((unsigned char *)indata, size);
}


static int
arc4_bytes(unsigned char *outdata, int size)
{
	arc4random_buf((void *)outdata, (size_t)size);
	return (1);
}


static void
arc4_cleanup(void)
{
}


static void
arc4_add(const void *indata, int size, double entropi)
{
	arc4random_addrandom((unsigned char *)indata, size);
}


static int
arc4_pseudorand(unsigned char *outdata, int size)
{
	return (arc4_bytes(outdata, size));
}


static int
arc4_status(void)
{
	return (1);
}


const RAND_METHOD ossl_rand_arc4_method =
{
	.seed		= arc4_seed,
	.bytes		= arc4_bytes,
	.cleanup	= arc4_cleanup,
	.add		= arc4_add,
	.pseudorand	= arc4_pseudorand,
	.status		= arc4_status
};

const RAND_METHOD *
RAND_arc4_method(void)
{
	return (&ossl_rand_arc4_method);
}
