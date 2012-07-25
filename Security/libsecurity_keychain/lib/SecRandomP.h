/*
 * Copyright (c) 2007-2009 Apple Inc. All Rights Reserved.
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

/*!
	@header SecRandomP
    Provides an additional CFDataRef returning random function
 */

#ifndef _SECURITY_SECRANDOMP_H_
#define _SECURITY_SECRANDOMP_H_

#include <Security/SecBase.h>
#include <stdint.h>
#include <sys/types.h>
#if SEC_BUILDER
#include "SecRandom.h"
#else
#include <Security/SecRandom.h>
#endif
#if defined(__cplusplus)
extern "C" {
#endif

/*!
 @function SecRandomCopyData
 @abstract Return count random bytes as a CFDataRef.
 @result Returns CFDataRef on success or NULL if something went wrong.
 */

CFDataRef
SecRandomCopyData(SecRandomRef rnd, size_t count)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECRANDOM_H_ */
