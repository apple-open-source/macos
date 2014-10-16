/*
 * Copyright (c) 2011 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2011 - 2013 Apple Inc. All rights reserved.
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

#include <sys/types.h>
#include <inttypes.h>
#include <roken.h>
#define HEIM_FUZZER_INTERNALS 1
#include "fuzzer.h"

#define SIZE(_array) (sizeof(_array) / sizeof((_array)[0]))

/*
 *
 */

static void
null_free(void *ctx)
{
    if (ctx != NULL)
	abort();
}

/*
 *
 */

#define MIN_RANDOM_TRIES 30000

static unsigned long
random_tries(size_t length)
{
    length = length * 12;
    if (length < MIN_RANDOM_TRIES)
	length = MIN_RANDOM_TRIES;
    return (unsigned long)length;
}

static int
random_fuzz(void **ctx, unsigned long iteration, uint8_t *data, size_t length)
{
    *ctx = NULL;

    if (iteration > MIN_RANDOM_TRIES && iteration > length * 12)
	return 1;

    data[rk_random() % length] = rk_random();

    return 0;
}


const struct heim_fuzz_type_data __heim_fuzz_random = {
    "random",
    random_tries,
    random_fuzz,
    null_free
};

/*
 *
 */

static unsigned long
bitflip_tries(size_t length)
{
    return length << 3;
}

static int
bitflip_fuzz(void **ctx, unsigned long iteration, uint8_t *data, size_t length)
{
    *ctx = NULL;
    if ((iteration >> 3) >= length)
	return 1;
    data[iteration >> 3] ^= (1 << (iteration & 7));

    return 0;
}

const struct heim_fuzz_type_data __heim_fuzz_bitflip = {
    "bitflip",
    bitflip_tries,
    bitflip_fuzz,
    null_free
};

/*
 *
 */

static unsigned long
byteflip_tries(size_t length)
{
    return length;
}

static int
byteflip_fuzz(void **ctx, unsigned long iteration, uint8_t *data, size_t length)
{
    *ctx = NULL;
    if (iteration >= length)
	return 1;
    data[iteration] ^= 0xff;

    return 0;
}

const struct heim_fuzz_type_data __heim_fuzz_byteflip = {
    "byteflip",
    byteflip_tries,
    byteflip_fuzz,
    null_free
};

/*
 *
 */

static unsigned long
shortflip_tries(size_t length)
{
    return length / 2;
}

static int
shortflip_fuzz(void **ctx, unsigned long iteration, uint8_t *data, size_t length)
{
    *ctx = NULL;
    if (iteration + 1 >= length / 2)
	return 1;
    data[iteration + 0] ^= 0xff;
    data[iteration + 1] ^= 0xff;

    return 0;
}

const struct heim_fuzz_type_data __heim_fuzz_shortflip = {
    "shortflip",
    shortflip_tries,
    shortflip_fuzz,
    null_free
};

/*
 *
 */

static unsigned long
wordflip_tries(size_t length)
{
    return length / 4;
}

static int
wordflip_fuzz(void **ctx, unsigned long iteration, uint8_t *data, size_t length)
{
    if (ctx)
	*ctx = NULL;
    if (iteration + 3 >= length / 4)
	return 1;
    data[iteration + 0] ^= 0xff;
    data[iteration + 1] ^= 0xff;
    data[iteration + 2] ^= 0xff;
    data[iteration + 3] ^= 0xff;

    return 0;
}

const struct heim_fuzz_type_data __heim_fuzz_wordflip = {
    "wordflip",
    wordflip_tries,
    wordflip_fuzz,
    null_free
};

/*
 * interesting values picked from AFL
 */

static uint8_t interesting_u8[] = {
    -128,
    -1,
    0,
    1,
    16,
    32,
    64,
    100,
    127
};

static uint16_t interesting_u16[] = {
    (uint16_t)-32768,
    (uint16_t)-129,
    128,
    255,
    256,
    512,
    1000,
    1024,
    4096,
    32767
};

static uint32_t interesting_u32[] = {
    (uint32_t)-2147483648LL,
    (uint32_t)-100000000,
    (uint32_t)-32769,
    32768,
   65535,
   65536,
   100000000,
   2147483647
};


static unsigned long
interesting_tries(size_t length)
{
    return length;
}

static int
interesting8_fuzz(void **ctx, unsigned long iteration, uint8_t *data, size_t length)
{
    if (length < iteration / SIZE(interesting_u8))
	return 1;

    memcpy(&data[iteration % SIZE(interesting_u8)], &interesting_u8[iteration / SIZE(interesting_u8)], 1);
    return 0;
}

const struct heim_fuzz_type_data __heim_fuzz_interesting8 = {
    "interesting uint8",
    interesting_tries,
    interesting8_fuzz,
    null_free
};

static int
interesting16_fuzz(void **ctx, unsigned long iteration, uint8_t *data, size_t length)
{
    if (length < 1 + (iteration / SIZE(interesting_u16)))
	return 1;

    memcpy(&data[iteration % SIZE(interesting_u16)], &interesting_u16[iteration / SIZE(interesting_u16)], 2);
    return 0;
}

const struct heim_fuzz_type_data __heim_fuzz_interesting16 = {
    "interesting uint16",
    interesting_tries,
    interesting16_fuzz,
    null_free
};

static int
interesting32_fuzz(void **ctx, unsigned long iteration, uint8_t *data, size_t length)
{
    if (length < 3 + (iteration / SIZE(interesting_u32)))
	return 1;

    memcpy(&data[iteration % SIZE(interesting_u32)], &interesting_u32[iteration / SIZE(interesting_u32)], 4);
    return 0;
}

const struct heim_fuzz_type_data __heim_fuzz_interesting32 = {
    "interesting uint32",
    interesting_tries,
    interesting32_fuzz,
    null_free
};

/*
 *
 */

const char *
heim_fuzzer_name(heim_fuzz_type_t type)
{
    return type->name;
}

unsigned long
heim_fuzzer_tries(heim_fuzz_type_t type, size_t length)
{
    return type->tries(length);
}

int
heim_fuzzer(heim_fuzz_type_t type,
	    void **ctx, 
	    unsigned long iteration,
	    uint8_t *data,
	    size_t length)
{
    if (length == 0)
	return 1;
    return type->fuzz(ctx, iteration, data, length);
}

void
heim_fuzzer_free(heim_fuzz_type_t type,
		 void *ctx)
{
    if (ctx != NULL)
	type->freectx(ctx);
}
