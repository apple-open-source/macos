/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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


#include <stdbool.h>
#include <CoreFoundation/CoreFoundation.h>

typedef void *SOSDataSourceFactoryRef;
//typedef void *SOSAccountRef;

// XXX Need to plumb these from security to secd.   If we can.

typedef SOSDataSourceFactoryRef (^AccountDataSourceFactoryBlock)();

bool SOSKeychainAccountSetFactoryForAccount(AccountDataSourceFactoryBlock factory);

bool SOSKeychainAccountSetFactoryForAccount(AccountDataSourceFactoryBlock factory)
{
    return false;
}

#if 0
SOSAccountRef SOSKeychainAccountGetSharedAccount(void);

SOSAccountRef SOSKeychainAccountGetSharedAccount(void)
{
    return (void*)0;
}
#endif

typedef struct _OpaqueFakeSecAccessControlRef *SecAccessControlRef;

CFTypeRef kSecAttrAccessControl = CFSTR("accc");

extern SecAccessControlRef SecAccessControlCreateFromData(CFAllocatorRef allocator, CFDataRef data, CFErrorRef *error);
extern CFDataRef SecAccessControlCopyData(SecAccessControlRef access_control);

SecAccessControlRef SecAccessControlCreateFromData(CFAllocatorRef allocator, CFDataRef data, CFErrorRef *error) {
    return NULL;
}

/*! Creates Access control instance from data serialized by SecAccessControlCopyData(). */
CFDataRef SecAccessControlCopyData(SecAccessControlRef access_control) {
    return NULL;    
}
