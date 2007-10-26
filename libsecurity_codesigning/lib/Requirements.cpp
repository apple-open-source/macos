/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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
// Requirements - SecRequirement API objects
//
#include "Requirements.h"

namespace Security {
namespace CodeSigning {


//
// Create from a Requirement blob in memory
//
SecRequirement::SecRequirement(const void *data, size_t length)
	: mReq(NULL)
{
	const Requirement *req = (const Requirement *)data;
	if (!req->validateBlob(length))
		MacOSError::throwMe(errSecCSReqInvalid);
	mReq = req->clone();
}


//
// Create from a genuine Requirement object
//
SecRequirement::SecRequirement(const Requirement *req, bool transferOwnership)
	: mReq(NULL)
{
	if (!req->validateBlob())
		MacOSError::throwMe(errSecCSReqInvalid);
	
	if (transferOwnership)
		mReq = req;
	else
		mReq = req->clone();
}
 

//
// Clean up a SecRequirement object
//
SecRequirement::~SecRequirement() throw()
{
	::free((void *)mReq);
}


} // end namespace CodeSigning
} // end namespace Security
