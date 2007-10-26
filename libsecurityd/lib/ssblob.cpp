/*
 * Copyright (c) 2000-2004,2006 Apple Computer, Inc. All Rights Reserved.
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
// ssclient - SecurityServer client interface library
//
#include "ssblob.h"


namespace Security {
namespace SecurityServer {


//
// Initialize the blob header for a given version
//
void CommonBlob::initialize(uint32 version)
{
    magic = magicNumber;
    this->blobVersion = version;
}


//
// Verify the blob header for basic sane-ness.
//
bool CommonBlob::isValid() const
{
	return magic == magicNumber;
}

void CommonBlob::validate(CSSM_RETURN failureCode) const
{
    if (!isValid())
        CssmError::throwMe(failureCode);
}

/*
 * This string is placed in KeyBlob.blobSignature to indicate a cleartext
 * public key.
 */
static const char clearPubKeySig[] = "Cleartext public key";

bool KeyBlob::isClearText()
{
	return (memcmp(blobSignature, clearPubKeySig, 
		sizeof(blobSignature)) == 0);
}

void KeyBlob::setClearTextSignature()
{
	memmove(blobSignature, clearPubKeySig, sizeof(blobSignature));
}
	


} // end namespace SecurityServer

} // end namespace Security
