/*
 * Copyright (c) 2000-2004,2011-2012,2014 Apple Inc. All Rights Reserved.
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
// dlquery - search query sublanguage for DL and MDS queries
//
#include <security_cdsa_client/dlquery.h>


namespace Security {
namespace CssmClient {


//
// Constructing Relations
//
Comparison::Comparison(const Comparison &r)
	: mName(r.mName), mOperator(r.mOperator), mFormat(r.mFormat),
	  mValue(Allocator::standard())
{
	mValue.copy(r.mValue);
}
	
Comparison &Comparison::operator = (const Comparison &r)
{
	mName = r.mName;
	mOperator = r.mOperator;
	mFormat = r.mFormat;
	mValue.copy(r.mValue);
	return *this;
}


Comparison::Comparison(const Attribute &attr, CSSM_DB_OPERATOR op, const char *s)
	: mName(attr.name()), mOperator(op), mFormat(CSSM_DB_ATTRIBUTE_FORMAT_STRING),
	mValue(Allocator::standard(), StringData(s))
{ }

Comparison::Comparison(const Attribute &attr, CSSM_DB_OPERATOR op, const std::string &s)
	: mName(attr.name()), mOperator(op), mFormat(CSSM_DB_ATTRIBUTE_FORMAT_STRING),
	mValue(Allocator::standard(), StringData(s))
{ }

Comparison::Comparison(const Attribute &attr, CSSM_DB_OPERATOR op, uint32 value)
	: mName(attr.name()), mOperator(op), mFormat(CSSM_DB_ATTRIBUTE_FORMAT_UINT32),
	mValue(Allocator::standard(), CssmData::wrap(value))
{ }

Comparison::Comparison(const Attribute &attr, CSSM_DB_OPERATOR op, bool value)
	: mName(attr.name()), mOperator(op), mFormat(CSSM_DB_ATTRIBUTE_FORMAT_UINT32),
	mValue(Allocator::standard(), CssmData::wrap(uint32(value ? 1 : 0)))
{ }

Comparison::Comparison(const Attribute &attr, CSSM_DB_OPERATOR op, const CssmData &data)
	: mName(attr.name()), mOperator(op), mFormat(CSSM_DB_ATTRIBUTE_FORMAT_BLOB),
	mValue(Allocator::standard(), data)
{ }

Comparison::Comparison(const Attribute &attr, CSSM_DB_OPERATOR op, const CSSM_GUID &guid)
	: mName(attr.name()), mOperator(op), mFormat(CSSM_DB_ATTRIBUTE_FORMAT_STRING),
	mValue(Allocator::standard(), StringData(Guid::overlay(guid).toString()))
{
}


Comparison::Comparison(const Attribute &attr)
	: mName(attr.name()), mOperator(CSSM_DB_NOT_EQUAL), mFormat(CSSM_DB_ATTRIBUTE_FORMAT_UINT32),
	  mValue(Allocator::standard(), CssmData::wrap(uint32(CSSM_FALSE)))
{
}

Comparison operator ! (const Attribute &attr)
{
	return Comparison(attr, CSSM_DB_EQUAL, uint32(CSSM_FALSE));
}


//
// Query methods
//
Query &Query::operator = (const Query &q)
{
	mRelations = q.mRelations;
	mQueryValid = false;
	return *this;
}


//
// Form the CssmQuery from a Query object.
// We cache this in mQuery, which we have made sure isn't copied along.
//
const CssmQuery &Query::cssmQuery() const
{
	if (!mQueryValid) {
		// record type remains at ANY
		mQuery.conjunctive(CSSM_DB_AND);
		for (vector<Comparison>::const_iterator it = mRelations.begin(); it != mRelations.end(); it++) {
			CssmSelectionPredicate pred;
			pred.dbOperator(it->mOperator);
			pred.attribute().info() = CssmDbAttributeInfo(it->mName.c_str(), it->mFormat);
			pred.attribute().set(it->mValue.get());
			mPredicates.push_back(pred);
		}
		mQuery.set((uint32)mPredicates.size(), &mPredicates[0]);
		mQueryValid = true;
	}
	return mQuery;
}


} // end namespace CssmClient
} // end namespace Security
