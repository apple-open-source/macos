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
 * Useful memory managment macros
 * Copyright (C) 1989 by NeXT, Inc.
 */
#define  mm_used() mstats()

#define MM_ALLOC(obj) obj = ((void *)malloc(sizeof(*(obj))))

#define MM_FREE(obj)  free((void *)(obj))

#define MM_ZERO(obj)  bzero((void *)(obj), sizeof(*(obj)))

#define MM_BCOPY(b1, b2, size) bcopy((void *)(b1), (void *)(b2), \
				     (unsigned)(size))

#define MM_BEQ(b1, b2, size) (bcmp((void *)(b1), (void *)(b2), \
				   (unsigned)(size)) == 0)

#define MM_ALLOC_ARRAY(obj, len)  \
	obj = ((void *)malloc(sizeof(*(obj)) * (len)))

#define MM_ZERO_ARRAY(obj, len) bzero((void *)(obj), sizeof(*obj) * len)

#define MM_FREE_ARRAY(obj, len) free((void *)(obj))

#define MM_GROW_ARRAY(obj, len) \
	((obj == NULL) ? (MM_ALLOC_ARRAY((obj), (len) + 1)) : \
	 (obj = (void *)realloc((void *)(obj), \
				sizeof(*(obj)) * ((len) + 1))))

#define MM_SHRINK_ARRAY(obj, len) \
	obj = (void *)realloc((void *)(obj), \
			      sizeof(*(obj)) * ((len) - 1))

