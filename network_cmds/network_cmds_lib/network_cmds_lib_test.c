/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <strings.h>
#include <sys/syslimits.h>

#include "network_cmds_lib.h"

uint8_t buffer[LINE_MAX];

static void
hexdump(void *data, size_t len)
{
	size_t i, j, k;
	unsigned char *ptr = (unsigned char *)data;
#define MAX_PER_LINE 16
#define HEX_SIZE (3 * MAX_PER_LINE)
#define ASCII_OFFSET (HEX_SIZE + 2)
#define ASCII_SIZE MAX_PER_LINE
#define BUF_SIZE (ASCII_OFFSET + MAX_PER_LINE + 1)

	for (i = 0; i < len; i += MAX_PER_LINE) {
		unsigned char buf[BUF_SIZE];

		memset(buf, ' ', BUF_SIZE);
		buf[BUF_SIZE - 1] = 0;

		for (j = i, k = 0; j < i + MAX_PER_LINE && j < len; j++) {
			unsigned char msnbl = ptr[j] >> 4;
			unsigned char lsnbl = ptr[j] & 0x0f;

			buf[k++] = msnbl < 10 ? msnbl + '0' : msnbl + 'a' - 10;
			buf[k++] = lsnbl < 10 ? lsnbl + '0' : lsnbl + 'a' - 10;

			buf[k++] = ' ';

			buf[ASCII_OFFSET + j - i] = isprint(ptr[j]) ? ptr[j] : '.';
		}
		(void) printf("%s\n", buf);
	}
}

int
main(int argc, char *argv[])
{
	char *str;

	for (uint16_t val = 0; val < UINT8_MAX; val++) {
		buffer[val] = (uint8_t)(val + 1);
	}
	buffer[UINT8_MAX] = 0;

	printf("\n# dirty string buffer:\n");
	hexdump(buffer, UINT8_MAX +1);

	str = (char *)buffer;
	str = clean_non_printable(str, UINT8_MAX);

	printf("\n# cleanup string buffer:\n");
	hexdump(str, UINT8_MAX +1);

	printf("\n# printf string:\n");
	printf("%s\n", str);

	return 0;
}
