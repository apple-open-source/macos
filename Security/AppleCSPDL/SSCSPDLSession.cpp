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
	/* for now we only allow ref keys */
	if(inKey.blobType() != CSSM_KEYBLOB_REFERENCE) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
	}
	
	/* fetch key (this is just mapping the value in inKey.KeyData to an SSKey) */
	SSKey &theKey = find<SSKey>(inKey);
	
	#ifdef someday 
	/* 
	 * Make sure caller hasn't changed any crucial header fields.
	 * Some fields were changed by makeReferenceKey, so make a local copy....
	 */
	CSSM_KEYHEADER localHdr = cssmKey.KeyHeader;
	get binKey-like thing from SSKey, maybe SSKey should keep a copy of 
	hdr...but that's' not supersecure....;
	
	localHdr.BlobType = binKey->mKeyHeader.BlobType;
	localHdr.Format = binKey->mKeyHeader.Format;
	if(memcmp(&localHdr, &binKey->mKeyHeader, sizeof(CSSM_KEYHEADER))) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_REFERENCE);
	}
	#endif
	return theKey;
}
