/*
 * Copyright (c) 2000-2004,2006,2011,2014 Apple Inc. All Rights Reserved.
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
// cssmwalkers - walkers for standard CSSM datatypes and wrappers
//
#ifndef _H_CSSMWALKERS
#define _H_CSSMWALKERS

#include <security_cdsa_utilities/walkers.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_utilities/cssmpods.h>
#include <security_cdsa_utilities/cssmkey.h>



namespace Security {
namespace DataWalkers {


//
// There are lots of CSSM data structures that are variable-length records
// of the form { count, pointer-to-array }. If you have a regular PodWrapper
// for it, we can enumerate the array for you right here. Minimum requirement:
//		size_t size() const;
//		Element &operator [] (uint32 index);
//      // and some Element *&foo() that returns a reference-to-array-pointer
// and a reference walker for the element type (as returned by operator []).
//
template <class Action, class Record, class Element>
void enumerateArray(Action &operate, Record &record, Element *& (Record::*pointer)())
{
	if (record.size()) {
		Element *&root = (record.*pointer)();
		operate.blob(root, record.size() * sizeof(Element));
		for (uint32 ix = 0; ix < record.size(); ++ix)
			walk(operate, record[ix]);
	}
}


//
// The full set of walkers for CssmData in all its forms.
//
template <class Action>
void walk(Action &operate, CssmData &data)
{
	operate(data);
	operate.blob(data.Data, data.Length);
}

template <class Action>
CssmData *walk(Action &operate, CssmData * &data)
{
	operate(data);
	operate.blob(data->Data, data->Length);
	return data;
}

template <class Action>
void walk(Action &operate, CSSM_DATA &data)
{ walk(operate, CssmData::overlay(data)); }

template <class Action>
CSSM_DATA *walk(Action &operate, CSSM_DATA * &data)
{ return walk(operate, CssmData::overlayVar(data)); }



//
// Walking a C string is almost regular (the size comes from strlen()).
// Just make sure you honor the needsSize preference of the operator.
//
template <class Action>
char *walk(Action &operate, char * &s)
{
	if (s)
		operate(s, operate.needsSize ? (strlen(s) + 1) : 0);
	return s;
}


//
// Flattener functions for common CSSM data types that have internal structure.
//
template <class Action>
CssmKey *walk(Action &operate, CssmKey * &key)
{
	operate(key);
	walk(operate, key->keyData());
	return key;
}

template <class Action>
CSSM_KEY *walk(Action &operate, CSSM_KEY * &data)
{ return walk(operate, CssmKey::overlayVar(data)); }

template <class Action>
CssmCryptoData *walk(Action &operate, CssmCryptoData * &data)
{
	operate(data);
	walk(operate, data->param());
	return data;
}

template <class Action>
CSSM_CRYPTO_DATA *walk(Action &operate, CSSM_CRYPTO_DATA * &data)
{ return walk(operate, CssmCryptoData::overlayVar(data)); }

template <class Action>
void walk(Action &operate, CSSM_PKCS5_PBKDF2_PARAMS &data)
{
    operate(data);
    walk(operate, data.Passphrase);
}

//
// Walkers for flat datatypes
//
template <class Action>
CSSM_DATE_PTR walk(Action &operate, CSSM_DATE_PTR &date)
{
    operate(date);
	return date;
}

template <class Action>
CSSM_RANGE_PTR walk(Action &operate, CSSM_RANGE_PTR &range)
{
    operate(range);
	return range;
}

template <class Action>
CSSM_VERSION_PTR walk(Action &operate, CSSM_VERSION_PTR &version)
{
    operate(version);
	return version;
}

template <class Action>
CSSM_DL_DB_HANDLE_PTR walk(Action &operate, CSSM_DL_DB_HANDLE_PTR &dlDbHandle)
{
    operate(dlDbHandle);
	return dlDbHandle;
}

template <class Action>
CssmSubserviceUid *walk(Action &operate, CssmSubserviceUid * &ssUid)
{
    operate(ssUid);
	return ssUid;
}


//
// A synthetic variant of CssmData to model key derivation (input) parameters,
// which have algorithm dependent structure. This is not likely to be useful
// for anything else; but here's the common ancestor of all its users.
//
class CssmDeriveData {
public:
	CssmDeriveData(const CssmData &dat, CSSM_ALGORITHMS alg)
		: baseData(dat), algorithm(alg) { }
	
	CssmData baseData;
	CSSM_ALGORITHMS algorithm;
	
	template <class Action>
	void enumerate(Action &operate)
	{
		walk(operate, baseData);
		switch (algorithm) {
		case CSSM_ALGID_PKCS5_PBKDF2:
#if BUG_3762664
			walk(operate, *baseData.interpretedAs<CSSM_PKCS5_PBKDF2_PARAMS>
				(CSSMERR_CSP_INVALID_ATTR_ALG_PARAMS));
#else
			if (baseData.length() != sizeof(CSSM_PKCS5_PBKDF2_PARAMS))
				CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_ALG_PARAMS);
			walk(operate, *(CSSM_PKCS5_PBKDF2_PARAMS *)baseData.data());
#endif
			break;
		default:
			break;
		}
	}
};


template <class Action>
void walk(Action &operate, CssmDeriveData &data)
{
	operate(data);
	data.enumerate(operate);
}

template <class Action>
CssmDeriveData *walk(Action &operate, CssmDeriveData * &data)
{
	operate(data);
	if (data)
		data->enumerate(operate);
	return data;
}



} // end namespace DataWalkers
} // end namespace Security

#endif //_H_CSSMWALKERS
