/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: a64l.c,v 1.3 1997/08/17 22:58:34 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <errno.h>
#include <stdlib.h>

long
a64l(s)
	const char *s;
{
	long value, digit, shift;
	int i;

	if (s == NULL) {
		errno = EINVAL;
		return(-1L);
	}

	value = 0;
	shift = 0;
	for (i = 0; *s && i < 6; i++, s++) {
		if (*s >= '.' && *s <= '/')
			digit = *s - '.';
		else if (*s >= '0' && *s <= '9')
			digit = *s - '0' + 2;
		else if (*s >= 'A' && *s <= 'Z')
			digit = *s - 'A' + 12;
		else if (*s >= 'a' && *s <= 'z')
			digit = *s - 'a' + 38;
		else {
			errno = EINVAL;
			return(-1L);
		}

		value |= digit << shift;
		shift += 6;
	}

	return(value);
}

