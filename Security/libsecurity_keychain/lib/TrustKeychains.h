/*
 * Copyright (c) 2009 Apple Inc. All Rights Reserved.
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

/*
 * TrustKeychains.h - manages the standard keychains searched for trusted certificates. 
 */

#ifndef	_TRUST_KEYCHAINS_H_
#define _TRUST_KEYCHAINS_H_

#include <security_utilities/threading.h>
#include <Security/cssmtype.h>
/*
#if defined(__cplusplus)
extern "C" {
#endif	
*/

/*!
 @function SecTrustKeychainsGetMutex
 @abstract Get the global mutex for accessing trust keychains during an evaluation
 @param result On return, a reference to the global mutex which manages access to trust keychains
 @discussion This function is intended to be used by C++ implementation layers to share a
 common global mutex for managing access to trust keychains (i.e. the root certificate store).
 */
RecursiveMutex& SecTrustKeychainsGetMutex();

/*
#if defined(__cplusplus)
}
#endif
*/

#endif	/* _TRUST_KEYCHAINS_H_ */

