/*
 * Copyright (c) 2002, 2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#include "math.h"
#include "complex.h"


/*	The following implementations of long double functions use double
	functions.

	THESE ARE NOT GOOD IMPLEMENTATIONS.

	These implementations are placedholders to allow programs to build and get
	crude results (results with the accuracies of the double-precision
	functions).  This is only for a proof-of-concept demonstration of Mac OS X
	for WWDC and should not become part of a product.

	-- Eric Postpischil, April 14, 2005.
*/
