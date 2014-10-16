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
 * identity_find.h
 */

#ifndef _IDENTITY_FIND_H_
#define _IDENTITY_FIND_H_  1

#include <Security/SecBase.h>
#include <Security/cssmtype.h>

#ifdef __cplusplus
extern "C" {
#endif
	
extern SecIdentityRef find_identity(CFTypeRef keychainOrArray,
									const char *identity,
									const char *hash,
									CSSM_KEYUSE keyUsage);
	
extern int keychain_find_identity(int argc, char * const *argv);
	
#ifdef __cplusplus
}
#endif

#endif /* _IDENTITY_FIND_H_ */
