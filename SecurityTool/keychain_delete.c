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
 *  keychain_delete.c
 *  security
 *
 *  Created by Michael Brouwer on Tue May 06 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include "keychain_delete.h"

#include "keychain_utilities.h"

#include <unistd.h>

static int
do_delete(CFTypeRef keychainOrArray)
{
	/* @@@ SecKeychainDelete should really take a CFTypeRef argument. */
	OSStatus result = SecKeychainDelete((SecKeychainRef)keychainOrArray);
	if (result)
	{
		/* @@@ Add printing of keychainOrArray. */
		fprintf(stderr, "SecKeychainDelete returned %ld(0x%lx)\n", result, result);
	}

	return result;
}

int
keychain_delete(int argc, char * const *argv)
{
	CFTypeRef keychainOrArray = NULL;
	int ch, result = 0;

    while ((ch = getopt(argc, argv, "h")) != -1)
	{
		switch  (ch)
		{
		case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}

	argc -= optind;
	argv += optind;

	keychainOrArray = keychain_create_array(argc, argv);

	result = do_delete(keychainOrArray);
	if (keychainOrArray)
		CFRelease(keychainOrArray);

	return result;
}
