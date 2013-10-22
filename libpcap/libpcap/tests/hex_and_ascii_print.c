/*
 * Copyright (c) 2013 Apple Inc. All rights reserved.
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


#include <stdio.h>
#include <ctype.h>

#define MAXBUF 16

void
hex_and_ascii_print(const char *prefix, const void *data,
					unsigned int len, const char *suffix)
{
	size_t i, j, k;
	unsigned char *ptr = (unsigned char *)data;
	unsigned char hexbuf[3 * MAXBUF + 1];
	unsigned char asciibuf[MAXBUF + 1];
	
	for (i = 0; i < len; i += MAXBUF) {
		for (j = i, k = 0; j < i + MAXBUF && j < len; j++) {
			unsigned char msnbl = ptr[j] >> 4;
			unsigned char lsnbl = ptr[j] & 0x0f;
			
			if (isprint(ptr[j]))
				asciibuf[j % MAXBUF]  = ptr[j];
			else
				asciibuf[j % MAXBUF]  = '.';
			asciibuf[(j % MAXBUF) + 1]  = 0;
			
			hexbuf[k++] = msnbl < 10 ? msnbl + '0' : msnbl + 'a' - 10;
			hexbuf[k++] = lsnbl < 10 ? lsnbl + '0' : lsnbl + 'a' - 10;
			if ((j % 2) == 1)
				hexbuf[k++] = ' ';
		}
		for (; j < i + MAXBUF;j++) {
			hexbuf[k++] = ' ';
			hexbuf[k++] = ' ';
			if ((j % 2) == 1)
				hexbuf[k++] = ' ';
		}
		hexbuf[k] = 0;
		printf("%s%s  %s%s\n", prefix, hexbuf, asciibuf, suffix);
	}
}
