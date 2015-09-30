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
// cssmcred - enhanced PodWrappers and construction aids for ACL credentials
//
#ifndef _CSSMCRED
#define _CSSMCRED

#include <security_utilities/utilities.h>
#include <security_cdsa_utilities/cssmlist.h>
#include <security_cdsa_utilities/cssmalloc.h>
#include <list>

namespace Security {


//
// PodWrappers for samples and sample groups
//
class CssmSample : public PodWrapper<CssmSample, CSSM_SAMPLE> {
public:
	CssmSample(const TypedList &list)
	{ TypedSample = list; Verifier = NULL; }
	CssmSample(const TypedList &list, const CssmSubserviceUid &ver)
	{ TypedSample = list; Verifier = &ver; }

	TypedList &value() { return TypedList::overlay(TypedSample); }
	const TypedList &value() const { return TypedList::overlay(TypedSample); }
	operator TypedList & () { return value(); }
	
	const CssmSubserviceUid *verifier() const { return CssmSubserviceUid::overlay(Verifier); }
	CssmSubserviceUid * &verifier()
	{ return const_cast<CssmSubserviceUid * &>(CssmSubserviceUid::overlayVar(Verifier)); }
};

class SampleGroup : public PodWrapper<SampleGroup, CSSM_SAMPLEGROUP> {
public:
	SampleGroup() { clearPod(); }
	SampleGroup(CssmSample &single)	{ NumberOfSamples = 1; Samples = &single; }

	uint32 size() const { return NumberOfSamples; }
	uint32 length() const { return size(); }	// legacy; prefer size()
	CssmSample *&samples() { return CssmSample::overlayVar(const_cast<CSSM_SAMPLE *&>(Samples)); }
	CssmSample *samples() const { return CssmSample::overlay(const_cast<CSSM_SAMPLE *>(Samples)); }

	CssmSample &operator [] (uint32 ix) const
	{ assert(ix < size()); return samples()[ix]; }
	
public:
	// extract all samples of a given sample type. return true if any found
	// note that you get a shallow copy of the sample structures for temporary use ONLY
	bool collect(CSSM_SAMPLE_TYPE sampleType, list<CssmSample> &samples) const;
};


//
// The PodWrapper for the top-level CSSM credentials structure
//
class AccessCredentials : public PodWrapper<AccessCredentials, CSSM_ACCESS_CREDENTIALS> {
public:
	AccessCredentials() { clearPod(); }
	explicit AccessCredentials(const SampleGroup &samples, const char *tag = NULL)
	{ this->samples() = samples; this->tag(tag); }
	explicit AccessCredentials(const SampleGroup &samples, const std::string &tag)
	{ this->samples() = samples; this->tag(tag); }
	
	const char *tag() const { return EntryTag[0] ? EntryTag : NULL; }
	std::string s_tag() const { return EntryTag; }
	void tag(const char *tagString);
	void tag(const std::string &tagString) { return tag(tagString.c_str()); }

	SampleGroup &samples() { return SampleGroup::overlay(Samples); }
	const SampleGroup &samples() const { return SampleGroup::overlay(Samples); }
	
	// pass-throughs to our SampleGroup
	uint32 size() const { return samples().size(); }
	CssmSample &operator [] (uint32 ix) const { return samples()[ix]; }
    
public:
    static const AccessCredentials &null;	// all null credential
	
	// turn NULL into a null credential if needed
	static const AccessCredentials *needed(const CSSM_ACCESS_CREDENTIALS *cred)
	{ return cred ? overlay(cred) : &null; }
};


//
// An AccessCredentials object with some construction help.
// Note that this is NOT a PodWrapper.
//
class AutoCredentials : public AccessCredentials {
public:
	AutoCredentials(Allocator &alloc);
	AutoCredentials(Allocator &alloc, uint32 nSamples);
	
	Allocator &allocator;
	
	CssmSample &sample(uint32 n) { return getSample(n); }
	
	CssmSample &append(const CssmSample &sample)
	{ return getSample(samples().length()) = sample; }
	TypedList &append(const TypedList &exhibit)
	{ return (getSample(samples().length()) = exhibit).value(); }
	
	CssmSample &operator += (const CssmSample &sample)	{ return append(sample); }
	TypedList &operator += (const TypedList &exhibit)	{ return append(exhibit); }
	
private:
	void init();
	CssmSample &getSample(uint32 n);
	
	CssmSample *sampleArray;
	uint32 nSamples;
};


//
// Walkers for the CSSM API structure types.
// Note that there are irrational "const"s strewn about the credential sub-structures.
// They make it essentially impossible to incrementally construction them without
// violating them. Since we know what we're doing, we do.
//
namespace DataWalkers
{

// CssmSample (with const override)
template <class Action>
void walk(Action &operate, CssmSample &sample)
{
	operate(sample);
	walk(operate, sample.value());
	if (sample.verifier())
		walk(operate, sample.verifier());
}

// SampleGroup
template <class Action>
void walk(Action &operate, SampleGroup &samples)
{
	operate(samples);
	enumerateArray(operate, samples, &SampleGroup::samples);
}

// AccessCredentials
template <class Action>
AccessCredentials *walk(Action &operate, AccessCredentials * &cred)
{
	operate(cred);
	//@@@ ignoring BaseCerts
	walk(operate, cred->samples());
	//@@@ ignoring challenge callback
	return cred;
}

template <class Action>
CSSM_ACCESS_CREDENTIALS *walk(Action &operate, CSSM_ACCESS_CREDENTIALS * &cred)
{ return walk(operate, AccessCredentials::overlayVar(cred)); }

template <class Action>
AutoCredentials *walk(Action &operate, AutoCredentials * &cred)
{ return (AutoCredentials *)walk(operate, (AccessCredentials * &)cred); }


} // end namespace DataWalkers
} // end namespace Security


#endif //_CSSMCRED
