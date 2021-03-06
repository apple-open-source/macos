/*
 * Copyright (c) 2019 Apple Inc. All Rights Reserved.
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
 * TAPI coverage for exported functions.
 */

#ifndef _SECEC_TAPI_H_
#define _SECEC_TAPI_H_

#ifndef SECURITY_PROJECT_TAPI_HACKS
#error This header is not for inclusion; it's a nasty hack to get the iOS Security framework to build with TAPI.
#endif

#include <Security/SecBase.h>
#include <corecrypto/ccec.h>

__BEGIN_DECLS

bool SecECDoWithFullKey(SecKeyRef key, CFErrorRef* error,void (^action)(ccec_full_ctx_t full_key));
bool SecECDoWithPubKey(SecKeyRef key, CFErrorRef* error, void (^action)(ccec_pub_ctx_t pub_key));

__END_DECLS

#endif
