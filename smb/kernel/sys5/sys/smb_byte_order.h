/*
 * Copyright (c) 2006 - 2010 Apple Inc. All rights reserved.
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

#ifndef _SMB_BYTEORDER_H_
#define _SMB_BYTEORDER_H_

#include <libkern/OSByteOrder.h>

#define htoles(x)	(OSSwapHostToLittleInt16(x))
#define letohs(x)	(OSSwapLittleToHostInt16(x))
#define	htolel(x)	(OSSwapHostToLittleInt32(x))
#define	letohl(x)	(OSSwapLittleToHostInt32(x))
#define	htoleq(x)	(OSSwapHostToLittleInt64(x))
#define	letohq(x)	(OSSwapLittleToHostInt64(x))

#define htobes(x)	(OSSwapHostToBigInt16(x))
#define betohs(x)	(OSSwapBigToHostInt16(x))
#define htobel(x)	(OSSwapHostToBigInt32(x))
#define betohl(x)	(OSSwapBigToHostInt32(x))
#define	htobeq(x)	(OSSwapHostToBigInt64(x))
#define	betohq(x)	(OSSwapBigToHostInt64(x))

#endif	/* !_SMB_BYTEORDER_H_ */
