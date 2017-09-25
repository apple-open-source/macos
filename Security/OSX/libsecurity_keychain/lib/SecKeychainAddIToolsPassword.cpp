/*
 *  Copyright (c) 2003-2013 Apple Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 *
 *  SecKeychainAddIToolsPassword.c
 *  
 *	Based on Keychain item access control example
 *	  -- added "always allow" ACL support
 */

#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecAccess.h>
#include <Security/SecAccessPriv.h>
#include <Security/SecTrustedApplicationPriv.h>
#include <Security/SecACL.h>
#include <CoreFoundation/CoreFoundation.h>


OSStatus SecKeychainAddIToolsPassword(SecKeychainRef __unused keychain,
                                      UInt32 __unused accountNameLength,
                                      const char * __unused accountName,
                                      UInt32 __unused passwordLength,
                                      const void * __unused passwordData,
                                      SecKeychainItemRef * __unused itemRef)
{
    return errSecParam;
}
