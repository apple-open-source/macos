/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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

#include "SecItem.h"
#include "SecItemPriv.h"
#include "SecBridge.h"
#include <security_utilities/cfutilities.h>
#include <CoreFoundation/CoreFoundation.h>

//
// SecItem* API bridge functions
//

OSStatus SecItemCopyMatching(CFDictionaryRef query, CFTypeRef *result)
{
    BEGIN_SECAPI
	Required(result);
    *result = (CFTypeRef) NULL;
    //%%%TBI
    return unimpErr;
    END_SECAPI2("SecItemCopyMatching")
}

OSStatus SecItemCopyDisplayNames(CFArrayRef items, CFArrayRef *displayNames)
{
    BEGIN_SECAPI
	Required(items);
	Required(displayNames);
    //%%%TBI
    return unimpErr;
    END_SECAPI2("SecItemCopyDisplayNames")
}

OSStatus SecItemAdd(CFDictionaryRef attributes, CFTypeRef *result)
{
    BEGIN_SECAPI
	Required(result);
    *result = (CFTypeRef) NULL;
    //%%%TBI
    return unimpErr;
    END_SECAPI2("SecItemAdd")
}

OSStatus SecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate)
{
    BEGIN_SECAPI
	Required(query);
	Required(attributesToUpdate);
    //%%%TBI
    return unimpErr;
    END_SECAPI2("SecItemUpdate")
}

OSStatus SecItemDelete(CFDictionaryRef query)
{
    BEGIN_SECAPI
	Required(query);
    //%%%TBI
    return unimpErr;
    END_SECAPI2("SecItemDelete")
}
