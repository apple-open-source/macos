/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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
#include <stdarg.h>
#include "fsck_debug.h"
#include "check.h"

void HexDump(const void *p_arg, unsigned length, int showOffsets)
{
	const u_int8_t *p = p_arg;
	unsigned i;
	char ascii[17];
	u_int8_t byte;
	
	ascii[16] = '\0';
	
	for (i=0; i<length; ++i)
	{
		if (showOffsets && (i & 0xF) == 0)
            fsck_print(ctx, LOG_TYPE_INFO, "%08X: ", i);

		byte = p[i];
        fsck_print(ctx, LOG_TYPE_INFO, "%02X ", byte);
		if (byte < 32 || byte > 126)
			ascii[i & 0xF] = '.';
		else
			ascii[i & 0xF] = byte;
		
		if ((i & 0xF) == 0xF)
		{
            fsck_print(ctx, LOG_TYPE_INFO, "  %s\n", ascii);
		}
	}
	
	if (i & 0xF)
	{
		unsigned j;
		for (j = i & 0xF; j < 16; ++j)
            fsck_print(ctx, LOG_TYPE_INFO, "   ");
		ascii[i & 0xF] = 0;
        fsck_print(ctx, LOG_TYPE_INFO, "  %s\n", ascii);
	}
}
