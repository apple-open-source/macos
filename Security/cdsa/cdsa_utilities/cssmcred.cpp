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
#include <Security/cssmcred.h>


namespace Security {


//
// The null credential constant.
//
static const CSSM_ACCESS_CREDENTIALS null_credentials = { "" };	// and more nulls
#if BUG_GCC
const AccessCredentials &AccessCredentials::null =
    *static_cast<const AccessCredentials *>(&null_credentials);
#else
const AccessCredentials &AccessCredentials::null =
    static_cast<const AccessCredentials &>(null_credentials);
#endif


//
// Scan a SampleGroup for samples with a given CSSM_SAMPLE_TYPE.
// Collect all matching samples into a list (which is cleared to begin with).
// Return true if any were found, false if none.
// Throw if any of the samples are obviously malformed.
//
bool SampleGroup::collect(CSSM_SAMPLE_TYPE sampleType, list<CssmSample> &matches) const
{
	for (uint32 n = 0; n < length(); n++) {
		TypedList sample = (*this)[n];
		sample.checkProper();
		if (sample.type() == sampleType) {
			sample.snip();	// skip sample type
			matches.push_back(sample);
		}
	}
	return !matches.empty();
}



//
// AutoCredentials self-constructing credentials structure
//
AutoCredentials::AutoCredentials(CssmAllocator &alloc) : allocator(alloc)
{
	init();
}

AutoCredentials::AutoCredentials(CssmAllocator &alloc, uint32 nSamples) : allocator(alloc)
{
	init();
}

void AutoCredentials::init()
{
	sampleArray = NULL;
	nSamples = 0;
}


CssmSample &AutoCredentials::getSample(uint32 n)
{
	if (n >= nSamples) {
		sampleArray = allocator.alloc<CssmSample>(sampleArray, nSamples = n + 1);
		Samples.Samples = sampleArray;
		Samples.NumberOfSamples = nSamples;
	}
	return sampleArray[n];
}

}	// end namespace Security
