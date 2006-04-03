/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Network number to ascii conversion
 * Copyright (C) 1989 by NeXT, Inc.
 */

#include <stdio.h>

char *nettoa(unsigned long net)
{
	static char buf[10];
	unsigned char f1, f2, f3;

	f1 = net & 0xff;
	net >>= 8;
	f2 = net & 0xff;
	net >>= 8;
	f3 = net & 0xff;
	if (f3 != 0) {
		sprintf(buf, "%u.%u.%u", f3, f2, f1);
	} else if (f2 != 0) {
		sprintf(buf, "%u.%u", f2, f1);
	} else {
		sprintf(buf, "%u", f1);
	}
	return (buf);
}

