/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 *  KCExceptions.h
 */
#ifndef _SECURITY_KCEXCEPTIONS_H_
#define _SECURITY_KCEXCEPTIONS_H_

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#ifdef lock
#undef lock
#endif
#include <Security/utilities.h>

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
inline T &Required(T *ptr,OSStatus err = paramErr)
{
    return Required(ptr,err);
}
*/

template <class T>
inline void KCThrowIfMemFail_(const T *ptr)
{
    if (ptr==NULL)
		MacOSError::throwMe(memFullErr);
}

inline void KCThrowIf_(OSStatus theErr)
{
	// will also work for OSErr
    if (theErr!=noErr)
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
        MacOSError::throwMe(paramErr);
}

inline void KCUnimplemented_()
{
	MacOSError::throwMe(unimpErr);
}

} // end namespace KeychainCore

} // end namespace Security

#endif /* !_SECURITY_KCEXCEPTIONS_H_ */
