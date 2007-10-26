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
 *  Cursor.h
 *  TokendMuscle
 */

#ifndef _TOKEND_CURSOR_H_
#define _TOKEND_CURSOR_H_

#include "Relation.h"
#include "Schema.h"
#include <security_cdsa_utilities/handleobject.h>
#include <vector>

namespace Tokend
{

class MetaRecord;
class RecordHandle;
class Relation;
class SelectionPredicate;

class Cursor : public HandleObject
{
	NOCOPY(Cursor)
public:
	Cursor();
    virtual ~Cursor() = 0;
    virtual RecordHandle *next(TokenContext *tokenContext) = 0;
};

class LinearCursor : public Cursor
{
    NOCOPY(LinearCursor)
public:
    LinearCursor(const CSSM_QUERY *inQuery, const Relation &inRelation);
    virtual ~LinearCursor();
    virtual RecordHandle *next(TokenContext *tokenContext);

private:
	Relation::const_iterator mIterator;
	Relation::const_iterator mEnd;

    const MetaRecord &mMetaRecord;

    CSSM_DB_CONJUNCTIVE mConjunctive;

	// If CSSM_QUERY_RETURN_DATA is set return the raw key bits
    CSSM_QUERY_FLAGS mQueryFlags;
    typedef vector<SelectionPredicate *> PredicateVector;

    PredicateVector mPredicates;
};

class MultiCursor : public Cursor
{
    NOCOPY(MultiCursor)
public:
    MultiCursor(const CSSM_QUERY *inQuery, const Schema &inSchema);
    virtual ~MultiCursor();
    virtual RecordHandle *next(TokenContext *tokenContext);

private:
	Schema::ConstRelationMapIterator mRelationIterator;
	Schema::ConstRelationMapIterator mRelationEnd;
	auto_ptr<CssmAutoQuery> mQuery;
	auto_ptr<Cursor> mCursor;
};

} // end namespace Tokend

#endif /* !_TOKEND_CURSOR_H_ */


