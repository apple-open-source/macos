/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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


/*
 * DecodedCrl.cpp - object representing a decoded CRL, in NSS format,
 * with extensions parsed and decoded (still in NSS format).
 *
 * Created 8/28/2002 by Doug Mitchell. 
 */

#include "DecodedCrl.h"
#include "cldebugging.h"
#include "AppleX509CLSession.h"
#include "CSPAttacher.h"
#include <Security/cssmapple.h>

DecodedCrl::DecodedCrl(
	AppleX509CLSession	&session)
	: DecodedItem(session)
{
	memset(&mCrl, 0, sizeof(mCrl));
}

/* one-shot constructor, decoding from DER-encoded data */
DecodedCrl::DecodedCrl(
	AppleX509CLSession	&session,
	const CssmData 		&encodedCrl)
	: DecodedItem(session)
{
	memset(&mCrl, 0, sizeof(mCrl));
	PRErrorCode prtn = mCoder.decode(encodedCrl.data(), encodedCrl.length(), 
		kSecAsn1SignedCrlTemplate, &mCrl);
	if(prtn) {
		CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
	}
	mDecodedExtensions.decodeFromNss(mCrl.tbs.extensions);
	mState = IS_DecodedAll;
}
		
DecodedCrl::~DecodedCrl()
{
}
	
/* decode mCrl.tbs and its extensions */
void DecodedCrl::decodeCts(
	const CssmData	&encodedCts)
{
	assert(mState == IS_Empty);
	memset(&mCrl, 0, sizeof(mCrl));
	PRErrorCode prtn = mCoder.decode(encodedCts.data(), encodedCts.length(), 
		kSecAsn1TBSCrlTemplate, &mCrl.tbs);
	if(prtn) {
		CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
	}
	mDecodedExtensions.decodeFromNss(mCrl.tbs.extensions);
	mState = IS_DecodedTBS;
}

void DecodedCrl::encodeExtensions()
{
	NSS_TBSCrl &tbs = mCrl.tbs;
	assert(mState == IS_Building);
	assert(tbs.extensions == NULL);

	if(mDecodedExtensions.numExtensions() == 0) {
		/* no extensions, no error */
		return;
	}
	mDecodedExtensions.encodeToNss(tbs.extensions);
}

/*
 * FIXME : how to determine max encoding size at run time!?
 */
#define MAX_TEMPLATE_SIZE	(16 * 1024)

/* encode TBS component; only called from CrlCreateTemplate */
void DecodedCrl::encodeCts(
	CssmOwnedData	&encodedCts)
{
	encodeExtensions();
	assert(mState == IS_Building);
	
	/* enforce required fields - could go deeper, maybe we should */
	NSS_TBSCrl &tbs = mCrl.tbs;
	if((tbs.signature.algorithm.Data == NULL) ||
	   (tbs.issuer.rdns == NULL)) {
		clErrorLog("DecodedCrl::encodeTbs: incomplete TBS");
		/* an odd, undocumented error return */
		CssmError::throwMe(CSSMERR_CL_NO_FIELD_VALUES);
	}
	
	PRErrorCode prtn;
	prtn = SecNssEncodeItemOdata(&tbs, kSecAsn1TBSCrlTemplate,
		encodedCts);
	if(prtn) {
		CssmError::throwMe(CSSMERR_CL_MEMORY_ERROR);
	}
}

