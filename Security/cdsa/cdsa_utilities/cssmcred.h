/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
// cssmcred - enhanced PodWrappers and construction aids for ACL credentials
//
#ifndef _CSSMCRED
#define _CSSMCRED

#include <Security/utilities.h>
#include <Security/cssmlist.h>
#include <Security/cssmalloc.h>
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
	operator const TypedList & () const { return value(); }
	
	const CssmSubserviceUid *verifier() const { return CssmSubserviceUid::overlay(Verifier); }
	const CssmSubserviceUid * &verifier() { return CssmSubserviceUid::overlayVar(Verifier); }
};

class SampleGroup : public PodWrapper<SampleGroup, CSSM_SAMPLEGROUP> {
public:
	uint32 length() const { return NumberOfSamples; }

	const CssmSample &operator [] (uint32 n) const
	{ assert(n < length()); return CssmSample::overlay(Samples[n]); }
	
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
	
	const char *tag() const { return EntryTag; }

	SampleGroup &samples() { return SampleGroup::overlay(Samples); }
	const SampleGroup &samples() const { return SampleGroup::overlay(Samples); }
    
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
	AutoCredentials(CssmAllocator &alloc);
	AutoCredentials(CssmAllocator &alloc, uint32 nSamples);
	
	CssmAllocator &allocator;
	
	CssmSample &sample(uint32 n) { return getSample(n); }
	
	CssmSample &operator += (const CssmSample &sample)
	{ return getSample(samples().length()) = sample; }
	TypedList &operator += (const TypedList &exhibit)
	{ return (getSample(samples().length()) = exhibit).value(); }
	
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

template <class Action>
void walk(Action &operate, const CssmSample &sample)
{ walk(operate, const_cast<CssmSample &>(sample)); }

// SampleGroup
template <class Action>
void walk(Action &operate, SampleGroup &samples)
{
	operate(samples);
	operate.blob(const_cast<CSSM_SAMPLE * &>(samples.Samples),
		samples.length() * sizeof(CSSM_SAMPLE));
	for (uint32 n = 0; n < samples.length(); n++)
		walk(operate, samples[n]);
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
