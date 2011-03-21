/*
 *  Copyright (c) 2008 Apple Inc. All Rights Reserved.
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
 */

#ifndef PADDING_H
#define PADDING_H

#include "byte_string.h"
#include <Security/cssmtype.h>
#include <security_cdsa_utilities/cssmerrors.h>

using namespace Security;

/** Utility class to unify padding/hash-header handling
 *
 */
class Padding {
public:
	/** Applies padding and hash-headers for signing */
	static void apply(byte_string &data, size_t keySize, CSSM_PADDING padding = CSSM_PADDING_NONE, CSSM_ALGORITHMS hashAlg = CSSM_ALGID_NONE) throw(CssmError);
	/** Removes padding for decryption
	 * Note: Securely eliminates data such that the 'leftover' bytes are not left to be read after data's destruction
	 */
	static void remove(byte_string &data, CSSM_PADDING padding = CSSM_PADDING_NONE) throw(CssmError);

	/** Returns boolean whether a specific padding/hash-header can be applied */
	static bool canApply(CSSM_PADDING padding = CSSM_PADDING_NONE, CSSM_ALGORITHMS hashAlg = CSSM_ALGID_NONE) throw();
	/** Returns boolean whether a specific padding can be removed */
	static bool canRemove(CSSM_PADDING padding) throw();
};

#endif