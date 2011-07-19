/*
 * Copyright (c) 2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>


#include <stdio.h>
#include <stdlib.h>
#include <rand.h>
#include <heim_threads.h>

#include <roken.h>

#include "randi.h"

#if defined(__APPLE_PRIVATE__) && !defined(__APPLE_TARGET_EMBEDDED__)

#include <CommonCrypto/CommonRandomSPI.h>

/*
 * Unix /dev/random
 */

static void
cc_seed(const void *indata, int size)
{
}


static int
cc_bytes(unsigned char *outdata, int size)
{
    if (CCRandomCopyBytes(kCCRandomDefault, outdata, size) != kCCSuccess)
	return 0;
    return 1;
}

static void
cc_cleanup(void)
{
}

static void
cc_add(const void *indata, int size, double entropi)
{
}

static int
cc_pseudorand(unsigned char *outdata, int size)
{
    return cc_bytes(outdata, size);
}

static int
cc_status(void)
{
    return 1;
}

const RAND_METHOD hc_rand_cc_method = {
    cc_seed,
    cc_bytes,
    cc_cleanup,
    cc_add,
    cc_pseudorand,
    cc_status
};

#endif /* __APPLE_PRIVATE__ */

const RAND_METHOD *
RAND_cc_method(void)
{
#if defined(__APPLE_PRIVATE__) && !defined(__APPLE_TARGET_EMBEDDED__)
    return &hc_rand_cc_method;
#else
    return NULL;
#endif
}
