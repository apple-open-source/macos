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
 * DecodedItem.cpp - class representing the common portions of NSS-style
 * certs and CRLs, with extensions parsed and decoded (still in NSS
 * format).
 */

#include "DecodedItem.h"
#include "cldebugging.h"
#include "AppleX509CLSession.h"
#include "CSPAttacher.h"
#include "CLFieldsCommon.h"
#include "clNssUtils.h"
#include <Security/cssmapple.h>


DecodedItem::DecodedItem(
	AppleX509CLSession	&session)
	:	mState(IS_Empty),
		mAlloc(session),
		mSession(session),
		mDecodedExtensions(mCoder, session)
{
}

DecodedItem::~DecodedItem()
{
	/* nothing for now */
}

/* 
 * Search for DecodedExten by AsnOid or "any unknown extension".
 * Called from getField*() and inferKeyUsage. 
 * Returns NULL if specified extension not found.
 */
const DecodedExten *DecodedItem::findDecodedExt(
	const CSSM_OID		&extnId,		// for known extensions
	bool				unknown,		// otherwise		
	uint32				index, 
	uint32				&numFields) const
{
	unsigned dex;
	const DecodedExten *rtnExt = NULL;
	unsigned found = 0;
	
	for(dex=0; dex<mDecodedExtensions.numExtensions(); dex++) {
		const DecodedExten *decodedExt = mDecodedExtensions.getExtension(dex);
		/*
		 * known extensions: OID match AND successful decode (In case
		 *    we encountered a known extension which we couldn't
		 *    decode and fell back to giving the app an unparsed
		 *    BER blob). 
		 * unknown extensions: just know that we didn't decode it
		 */
		if( ( !unknown && !decodedExt->berEncoded() &&
		      (clCompareCssmData(&decodedExt->extnId(), &extnId))
			) || 
		    (unknown && decodedExt->berEncoded())
		   ) {
			
			if(found++ == index) {
				/* the one we want */
				rtnExt = decodedExt;
			}
			if((rtnExt != NULL) && (index != 0)) {
				/* only determine numFields on search for first one */
				break;
			}
		}
	}
	if(rtnExt != NULL) {
		/* sucessful return  */
		if(index == 0) {
			numFields = found;
		}
		return rtnExt;
	}
	else {
		return NULL;
	}
}

