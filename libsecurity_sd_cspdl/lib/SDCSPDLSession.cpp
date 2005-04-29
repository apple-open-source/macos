/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
// SDCSPDLSession.cpp - Security Server CSP/DL session.
//
#include "SDCSPDLSession.h"

#include "SDCSPDLPlugin.h"
#include "SDKey.h"

using namespace SecurityServer;

//
// SDCSPDLSession -- Security Server CSP session
//
SDCSPDLSession::SDCSPDLSession()
{
}


//
// Reference Key management
//
void
SDCSPDLSession::makeReferenceKey(SDCSPSession &session, KeyHandle inKeyHandle,
								 CssmKey &outKey, CSSM_DB_HANDLE inDBHandle,
								 uint32 inKeyAttr, const CssmData *inKeyLabel)
{
	new SDKey(session, inKeyHandle, outKey, inDBHandle, inKeyAttr,
			  inKeyLabel);
}

SDKey &
SDCSPDLSession::lookupKey(const CssmKey &inKey)
{
	/* for now we only allow ref keys */
	if(inKey.blobType() != CSSM_KEYBLOB_REFERENCE) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
	}
	
	/* fetch key (this is just mapping the value in inKey.KeyData to an SDKey) */
	SDKey &theKey = find<SDKey>(inKey);
	
	#ifdef someday 
	/* 
	 * Make sure caller hasn't changed any crucial header fields.
	 * Some fields were changed by makeReferenceKey, so make a local copy....
	 */
	CSSM_KEYHEADER localHdr = cssmKey.KeyHeader;
	get binKey-like thing from SDKey, maybe SDKey should keep a copy of 
	hdr...but that's' not supersecure....;
	
	localHdr.BlobType = binKey->mKeyHeader.BlobType;
	localHdr.Format = binKey->mKeyHeader.Format;
	if(memcmp(&localHdr, &binKey->mKeyHeader, sizeof(CSSM_KEYHEADER))) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_REFERENCE);
	}
	#endif
	return theKey;
}
