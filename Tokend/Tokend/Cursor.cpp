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
 *  Cursor.cpp
 *  TokendMuscle
 */

#include "Cursor.h"

#include "MetaRecord.h"
#include "Record.h"
#include "RecordHandle.h"
#include "Relation.h"
#include "Token.h"
#include "SelectionPredicate.h"

namespace Tokend
{

#pragma mark ---------------- Cursor methods --------------

//
// Cursor implemetation
//
Cursor::Cursor()
{
}

Cursor::~Cursor()
{
}

//
// LinearCursor implemetation
//
LinearCursor::LinearCursor(const CSSM_QUERY *inQuery,
	const Relation &inRelation) :
	mIterator(inRelation.begin()),
	mEnd(inRelation.end()),
    mMetaRecord(inRelation.metaRecord())
{
	mConjunctive = inQuery->Conjunctive;
	mQueryFlags = inQuery->QueryFlags;
	// @@@ Do something with inQuery->QueryLimits?
	uint32 aPredicatesCount = inQuery->NumSelectionPredicates;
	mPredicates.resize(aPredicatesCount);
	try
	{
		for (uint32 anIndex = 0; anIndex < aPredicatesCount; anIndex++)
		{
			CSSM_SELECTION_PREDICATE &aPredicate =
				inQuery->SelectionPredicate[anIndex];
			mPredicates[anIndex] =
				new SelectionPredicate(mMetaRecord, aPredicate);
		}
	}
	catch (...)
	{
		for_each_delete(mPredicates.begin(), mPredicates.end());
		throw;
	}
}

LinearCursor::~LinearCursor()
{
	for_each_delete(mPredicates.begin(), mPredicates.end());
}

RecordHandle *LinearCursor::next(TokenContext *tokenContext)
{
	while (mIterator != mEnd)
	{
		RefPointer<Record> rec = *mIterator;
		++mIterator;

        PredicateVector::const_iterator anIt = mPredicates.begin();
        PredicateVector::const_iterator anEnd = mPredicates.end();
		bool aMatch;
		if (anIt == anEnd)	// If there are no predicates we have a match.
			aMatch = true;
		else if (mConjunctive == CSSM_DB_OR)
		{
			// If mConjunctive is OR, the first predicate that returns
			// true indicates a match. Dropthough means no match
			aMatch = false;
			for (; anIt != anEnd; anIt++)
			{
				if ((*anIt)->evaluate(tokenContext, *rec))
				{
					aMatch = true;
                    break;
				}
			}
		}
		else if (mConjunctive == CSSM_DB_AND || mConjunctive == CSSM_DB_NONE)
		{
			// If mConjunctive is AND (or NONE), the first predicate that
			// returns false indicates a mismatch. Dropthough means a match.
			aMatch = true;
			for (; anIt != anEnd; anIt++)
			{
				if (!(*anIt)->evaluate(tokenContext, *rec))
				{
					aMatch = false;
                    break;
				}
			}
		}
		else
		{
			CssmError::throwMe(CSSMERR_DL_INVALID_QUERY);
		}

        if (aMatch)
			return new RecordHandle(mMetaRecord, rec);
    }

	return NULL;
}

#pragma mark ---------------- MultiCursor methods --------------

MultiCursor::MultiCursor(const CSSM_QUERY *inQuery, const Schema &inSchema) :
	mRelationIterator(inSchema.begin()),
	mRelationEnd(inSchema.end())
{
	if (inQuery)
		mQuery.reset(new CssmAutoQuery(*inQuery));
	else
	{
		mQuery.reset(new CssmAutoQuery());
		mQuery->recordType(CSSM_DL_DB_RECORD_ANY);
	}
}

MultiCursor::~MultiCursor()
{
}

RecordHandle *MultiCursor::next(TokenContext *tokenContext)
{
	RecordHandle *result =  NULL;
	for (;;)
	{
		if (!mCursor.get())
		{
			if (mRelationIterator == mRelationEnd)
				return NULL;

			const Relation &aRelation = *(mRelationIterator->second);
			++mRelationIterator;
			if (!aRelation.matchesId(mQuery->recordType()))
				continue;

			mCursor.reset(new LinearCursor(mQuery.get(), aRelation));
		}

		if ((result = mCursor->next(tokenContext)))
			return result;
			
		mCursor.reset(NULL);
	}
}


}	// end namespace Tokend


