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
    @header SecItemServer
    The functions provided in SecItemServer.h provide an interface to 
    the backed for SecItem APIs in the server.
*/

#ifndef _SECURITYD_SECITEMSERVER_H_
#define _SECURITYD_SECITEMSERVER_H_

#include <CoreFoundation/CoreFoundation.h>

#if defined(__cplusplus)
extern "C" {
#endif

OSStatus _SecItemCopyMatching(CFDictionaryRef query, CFTypeRef *result,
    CFArrayRef accessGroups);
OSStatus _SecItemAdd(CFDictionaryRef attributes, CFTypeRef *result,
    CFArrayRef accessGroups);
OSStatus _SecItemUpdate(CFDictionaryRef query,
    CFDictionaryRef attributesToUpdate, CFArrayRef accessGroups);
OSStatus _SecItemDelete(CFDictionaryRef query, CFArrayRef accessGroups);
bool _SecItemDeleteAll(void);
OSStatus _SecServerRestoreKeychain(void);
OSStatus _SecServerMigrateKeychain(CFArrayRef args, CFTypeRef *result);
OSStatus _SecServerKeychainBackup(CFArrayRef args_in, CFTypeRef *args_out);
OSStatus _SecServerKeychainRestore(CFArrayRef args_in, CFTypeRef *dummy);

#if defined(__cplusplus)
}
#endif

#endif /* _SECURITYD_SECITEMSERVER_H_ */
