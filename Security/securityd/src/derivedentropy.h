/*
 * Copyright (c) 2025 Apple Inc. All Rights Reserved.
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


//
// derivedentropy - securityd derived entropy layer
//

#ifndef _H_DERIVEDENTROPY
#define _H_DERIVEDENTROPY

#include <securityd_client/ss_types.h>
//#include "handletypes.h"

using namespace Security::SecurityServer;

KeyHandle storeHandleData(const CssmData& data, uint64_t timeoutSeconds = 5);
bool retrieveHandleData(KeyHandle handle, CssmDataContainer& data);
void removeHandle(KeyHandle handle);
KeyHandle generateDerivedEntropy(const CssmData& salt, const CssmData& passphrase);

#endif /* _H_DERIVEDENTROPY */
