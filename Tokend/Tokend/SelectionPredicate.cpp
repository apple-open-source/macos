/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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

/*
 *  SelectionPredicate.cpp
 *  TokendMuscle
 */

#include "SelectionPredicate.h"
#include "MetaAttribute.h"
#include "MetaRecord.h"
#include "DbValue.h"
#include <Security/cssmerr.h>

namespace Tokend
{

SelectionPredicate::SelectionPredicate(const MetaRecord &inMetaRecord,
	const CSSM_SELECTION_PREDICATE &inPredicate)
	:	mMetaAttribute(inMetaRecord.metaAttribute(inPredicate.Attribute.Info)),
		mDbOperator(inPredicate.DbOperator)
{
	// Make sure that the caller specified the attribute values in the correct
	// format.
	if (inPredicate.Attribute.Info.AttributeFormat
		!= mMetaAttribute.attributeFormat())
		CssmError::throwMe(CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);

	// @@@ See ISSUES
	if (inPredicate.Attribute.NumberOfValues != 1)
		CssmError::throwMe(CSSMERR_DL_UNSUPPORTED_QUERY);

	mData = inPredicate.Attribute.Value[0];
	mValue = mMetaAttribute.createValue(mData);
}

SelectionPredicate::~SelectionPredicate()
{
	delete mValue;
}

bool SelectionPredicate::evaluate(TokenContext *tokenContext,
	Record& record) const
{
    return mMetaAttribute.evaluate(tokenContext, mValue, record, mDbOperator);
}


}	// end namespace Tokend

