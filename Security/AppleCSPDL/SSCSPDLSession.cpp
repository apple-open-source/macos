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
// SSCSPDLSession.cpp - Security Server CSP/DL session.
//
#include "SSCSPDLSession.h"

#include "CSPDLPlugin.h"
#include "SSKey.h"

using namespace SecurityServer;

//
// SSCSPDLSession -- Security Server CSP session
//
SSCSPDLSession::SSCSPDLSession()
// @@@ FIXME allocators needs to change.
: mClientSession(CssmAllocator::standard(), CssmAllocator::standard())
{
}


//
// Reference Key management
//
void
SSCSPDLSession::makeReferenceKey(SSCSPSession &session, KeyHandle inKeyHandle,
								 CssmKey &outKey, SSDatabase &inSSDatabase,
								 uint32 inKeyAttr, const CssmData *inKeyLabel)
{
	new SSKey(session, inKeyHandle, outKey, inSSDatabase, inKeyAttr,
			  inKeyLabel);
}

SSKey &
SSCSPDLSession::lookupKey(const CssmKey &inKey)
{
	if (inKey.blobType() == CSSM_KEYBLOB_REFERENCE)
		return find<SSKey>(inKey);
	else if (inKey.blobType() == CSSM_KEYBLOB_RAW)
	{
		// @@@ How can we deal with this?
	}

	CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
}
