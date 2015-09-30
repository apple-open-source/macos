/*
 * Copyright (c) 2004,2011,2014 Apple Inc. All Rights Reserved.
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

#ifndef _H_CDSA_CLIENT_DLQUERY
#define _H_CDSA_CLIENT_DLQUERY

#include <security_cdsa_utilities/cssmdb.h>
#include <string>
#include <vector>


namespace Security {
namespace CssmClient {


//
// A DL record attribute
//
class Attribute {
public:
	Attribute(const std::string &name) : mName(name) { }
	Attribute(const char *name) : mName(name) { }
	
	const std::string &name() const { return mName; }

private:
	std::string mName;
};


//
// A comparison (attribute ~rel~ constant-value)
//
class Comparison {
	friend class Query;
public:
	Comparison(const Attribute &attr, CSSM_DB_OPERATOR op, const char *s);
	Comparison(const Attribute &attr, CSSM_DB_OPERATOR op, const std::string &s);
	Comparison(const Attribute &attr, CSSM_DB_OPERATOR op, uint32 v);
	Comparison(const Attribute &attr, CSSM_DB_OPERATOR op, bool v);
	Comparison(const Attribute &attr, CSSM_DB_OPERATOR op, const CSSM_GUID &guid);
	Comparison(const Attribute &attr, CSSM_DB_OPERATOR op, const CssmData &data);
	
	Comparison(const Attribute &attr);
	friend Comparison operator ! (const Attribute &attr);
	
	Comparison(const Comparison &r);
	Comparison &operator = (const Comparison &r);
	
private:
	std::string mName;
	CSSM_DB_OPERATOR mOperator;
	CSSM_DB_ATTRIBUTE_FORMAT mFormat;
	CssmAutoData mValue;
};

template <class Value>
Comparison operator == (const Attribute &attr, const Value &value)
{ return Comparison(attr, CSSM_DB_EQUAL, value); }

template <class Value>
Comparison operator != (const Attribute &attr, const Value &value)
{ return Comparison(attr, CSSM_DB_NOT_EQUAL, value); }

template <class Value>
Comparison operator < (const Attribute &attr, const Value &value)
{ return Comparison(attr, CSSM_DB_LESS_THAN, value); }

template <class Value>
Comparison operator > (const Attribute &attr, const Value &value)
{ return Comparison(attr, CSSM_DB_GREATER_THAN, value); }

template <class Value>
Comparison operator % (const Attribute &attr, const Value &value)
{ return Comparison(attr, CSSM_DB_CONTAINS, value); }


//
// A Query
//
class Query {
public:
	Query() : mQueryValid(false) { }
	Query(const Comparison r) : mQueryValid(false) { mRelations.push_back(r); }
	Query(const Attribute &attr) : mQueryValid(false) { mRelations.push_back(attr); }
	
	Query(const Query &q) : mRelations(q.mRelations), mQueryValid(false) { }
	
	Query &operator = (const Query &q);
	
	Query &add(const Comparison &r)
	{ mRelations.push_back(r); return *this; }
	
	const CssmQuery &cssmQuery() const;

private:
	std::vector<Comparison> mRelations;
	
	// cached CssmQuery equivalent of this object
	mutable bool mQueryValid;   // mQuery has been constructed
	mutable vector<CssmSelectionPredicate> mPredicates; // holds lifetimes for mQuery
	mutable CssmQuery mQuery;
};

inline Query operator && (Query c, const Comparison &r)
{ return c.add(r); }


} // end namespace CssmClient
} // end namespace Security

#endif // _H_CDSA_CLIENT_DLQUERY
