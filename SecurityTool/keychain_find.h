/*
 * Copyright (c) 2003-2009 Apple Inc. All Rights Reserved.
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
 *
 * keychain_find.h
 */

#ifndef _KEYCHAIN_FINDINTERNETPASSWORD_H_
#define _KEYCHAIN_FINDINTERNETPASSWORD_H_  1

#include <Security/SecBase.h>
#include <Security/SecKeychain.h>

#ifdef __cplusplus
extern "C" {
#endif

extern SecKeychainItemRef find_first_generic_password(
							CFTypeRef keychainOrArray,
							FourCharCode itemCreator,
							FourCharCode itemType,
							const char *kind,
							const char *value,
							const char *comment,
							const char *label,
							const char *serviceName,
							const char *accountName);

extern SecKeychainItemRef find_first_internet_password(
							CFTypeRef keychainOrArray,
							FourCharCode itemCreator,
							FourCharCode itemType,
							const char *kind,
							const char *comment,
							const char *label,
							const char *serverName,
							const char *securityDomain,
							const char *accountName,
							const char *path,
							UInt16 port,
							SecProtocolType protocol,
							SecAuthenticationType authenticationType);

extern SecKeychainItemRef find_unique_certificate(
							CFTypeRef keychainOrArray,
							const char *name,
							const char *hash);

extern int keychain_find_internet_password(int argc, char * const *argv);

extern int keychain_find_generic_password(int argc, char * const *argv);

extern int keychain_find_certificate(int argc, char * const *argv);

extern int keychain_dump(int argc, char * const *argv);

#ifdef __cplusplus
}
#endif

#endif /* _KEYCHAIN_FINDINTERNETPASSWORD_H_ */
