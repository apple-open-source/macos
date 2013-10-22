/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
 *  KCExceptions.h
 */
#ifndef _SECURITY_KCEXCEPTIONS_H_
#define _SECURITY_KCEXCEPTIONS_H_

#include <security_utilities/errors.h>
#include <SecBase.h>
#ifdef lock
#undef lock
#endif
//#include <security_cdsa_utilities/utilities.h>

#ifdef check
#undef check
#endif

namespace Security
{

namespace KeychainCore
{

//
// Helpers for memory pointer validation
//

/*	remove RequiredParam when cdsa does namespaces
template <class T>
inline T &Required(T *ptr,OSStatus err = errSecParam)
{
    return Required(ptr,err);
}
*/

template <class T>
inline void KCThrowIfMemFail_(const T *ptr)
{
    if (ptr==NULL)
		MacOSError::throwMe(errSecAllocate);
}

inline void KCThrowIf_(OSStatus theErr)
{
	// will also work for OSErr
    if (theErr!=errSecSuccess)
        MacOSError::throwMe(theErr);
}

inline void KCThrowIf_(bool test,OSStatus theErr)
{
	// will also work for OSErr
    if (test)
        MacOSError::throwMe(theErr);
}

inline void KCThrowParamErrIf_(bool test)
{
    if (test)
        MacOSError::throwMe(errSecParam);
}

inline void KCUnimplemented_()
{
	MacOSError::throwMe(errSecUnimplemented);
}

} // end namespace KeychainCore

} // end namespace Security

#endif /* !_SECURITY_KCEXCEPTIONS_H_ */
