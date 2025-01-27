/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
// SelectionPredicate.cpp
//

#include "SelectionPredicate.h"

SelectionPredicate::SelectionPredicate(const MetaRecord &inMetaRecord,
									   const CSSM_SELECTION_PREDICATE &inPredicate)
:	mMetaAttribute(inMetaRecord.metaAttribute(inPredicate.Attribute.Info)),
	mDbOperator(inPredicate.DbOperator)
{
	// Make sure that the caller specified the attribute values in the correct format.
	if (inPredicate.Attribute.Info.AttributeFormat != mMetaAttribute.attributeFormat())
		CssmError::throwMe(CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);

	// XXX See ISSUES
	if (inPredicate.Attribute.NumberOfValues != 1)
		CssmError::throwMe(CSSMERR_DL_UNSUPPORTED_QUERY);

	mData = inPredicate.Attribute.Value[0];
	mValue = mMetaAttribute.createValue(mData);
}

SelectionPredicate::~SelectionPredicate()
{
	delete mValue;
}

bool
SelectionPredicate::evaluate(const ReadSection &rs) const
{
    return mMetaAttribute.evaluate(mValue, rs, mDbOperator);
}
