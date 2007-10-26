/*
 * Copyright (c) 2000-2001,2003-2004,2006 Apple Computer, Inc. All Rights Reserved.
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
#include <security_cdsa_utilities/cssmcred.h>


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
// AccessCredentials
//
void AccessCredentials::tag(const char *tagString)
{
	if (tagString == NULL)
		EntryTag[0] = '\0';
	else if (strlen(tagString) > CSSM_MODULE_STRING_SIZE)
		CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_ENTRY_TAG);
	else
		strcpy(EntryTag, tagString);
}


//
// AutoCredentials self-constructing credentials structure
//
AutoCredentials::AutoCredentials(Allocator &alloc) : allocator(alloc)
{
	init();
}

AutoCredentials::AutoCredentials(Allocator &alloc, uint32 nSamples) : allocator(alloc)
{
	init();
	getSample(nSamples - 1);	// extend array to nSamples elements
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
