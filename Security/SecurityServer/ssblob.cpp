/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
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
    this->version = version;
}


//
// Verify the blob header for basic sane-ness.
// Version is checked (for equality) if non-zero.
void CommonBlob::validate(CSSM_RETURN failureCode) const
{
    if (magic != magicNumber)
        CssmError::throwMe(failureCode);
}



} // end namespace SecurityServer

} // end namespace Security
