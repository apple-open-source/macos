/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
/*
 * more string operations
 *
 */
 
#import "libsa.h"


#if i386 && !defined(DONT_USE_ASM)
void *memset(void *dst, int val, size_t len)
{
    asm("rep; stosb"
	: /* no outputs */
	: "&c" (len), "D" (dst), "a" (val)
	: "ecx", "esi", "eax");
    return dst;
}
/* Simple forward character copy */
void *memcpy(void *dst, const void *src, size_t len)
{
    asm("rep; movsb"
	: /* no outputs */
	: "&c" (len), "D" (dst), "e" (src)
	: "ecx", "esi", "edi");
    return src;
}
#else
void *memset(void *p, int val, size_t len)
{
	register char *ptr = (char *)p;
	while (len-- > 0) *ptr++ = val;
	return p;
}

void *memcpy(
    void *dst,
    const void *src,
    size_t len
)
{
	register char *src_c, *dst_c;
	
	src_c = (char *)src;
	dst_c = (char *)dst;
	
	while (len-- > 0)
		*dst_c++ = *src_c++;
	return src;
}
#endif

