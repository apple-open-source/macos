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

#ifndef _SECURITY_KCUTILITIES_H_
#define _SECURITY_KCUTILITIES_H_

#include <Security/SecKeychainItem.h>
#include <Security/utilities.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <memory>

namespace Security
{

OSStatus GetKeychainErrFromCSSMErr(OSStatus cssmError);

//
// Helpers for memory pointer validation
//
template <class T>
inline T &RequiredParam(T *ptr,OSStatus err = paramErr)
{
    if (ptr == NULL)
        MacOSError::throwMe(err);
    return *ptr;
}

class StKCAttribute
{
public:
                    StKCAttribute( SecKeychainAttribute* attr );
        virtual		~StKCAttribute( void );
private:
        SecKeychainAttribute* 	fAttr;
};

// Class for cleaning up a KCItemRef when finished with it
// if an error occurs at any time when dealing with the item.
//
class StKCItem
{
public:
                    StKCItem( SecKeychainItemRef* item, OSStatus* result );
        virtual		~StKCItem( void );
private:
        SecKeychainItemRef* 	fItem;
        OSStatus*	fResult;
};

} // end namespace Security

#endif // !_SECURITY_KCUTILITIES_H_
