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
// acl_threshold - Threshold-based group ACL subjects
//
#ifdef __MWERKS__
#define _CPP_ACL_THRESHOLD
#endif

#include <Security/acl_threshold.h>
#include <algorithm>
#include <Security/endian.h>


//
// Validate a credential set against this subject.
//
// With STRICTCOUNTING set, we assume that every match in the threshold ACL
// "consumes" one sample in the corresponding threshold sample. This will not
// work as expected for subject types that may succeed without a sample (e.g. ANY)
// or subject types that may multiply match against a single sample. You have been
// warned.
//
class SublistValidationContext : public AclValidationContext {
public:
    SublistValidationContext(const AclValidationContext &ctx, const TypedList &list)
    : AclValidationContext(ctx), sampleList(list) { }
    
    uint32 count() const { return sampleList.length() - 1; }
    const TypedList &sample(uint32 n) const
    { return TypedList::overlay(sampleList[n+1].list()); }
    
    const TypedList &sampleList;
};

bool ThresholdAclSubject::validate(const AclValidationContext &baseCtx,
    const TypedList &sample) const
{
#ifdef STRICTCOUNTING
    // Pre-screen for reasonable number of subsamples.
    // We could more strictly require subSampleCount == elements.length();
    // this is more flexible in that it allows the caller to abbreviate.
    uint32 subSampleCount = sample.length() - 1;	// (drop type header)
    if (subSampleCount < minimumNeeded)	// can't possibly satisfy
		CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
    if (subSampleCount > totalSubjects)	// reject attempt at sample stuffing
		CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
#endif //STRICTCOUNTING

    // evaluate
    SublistValidationContext ctx(baseCtx, sample);
    uint32 matched = 0;
    for (uint32 n = 0; n < totalSubjects; n++) {
		if ((matched += elements[n]->validate(ctx)) >= minimumNeeded)
            return true;
#ifdef STRICTCOUNTING
        else if (matched + subSampleCount - n <= minimumNeeded)
            return false;	// can't get there anymore
#endif //STRICTCOUNTING
    }
	return false;
}


//
// Make a copy of this subject in CSSM_LIST form
//
CssmList ThresholdAclSubject::toList(CssmAllocator &alloc) const
{
	TypedList result(alloc, CSSM_ACL_SUBJECT_TYPE_THRESHOLD,
		new(alloc) ListElement(minimumNeeded),
		new(alloc) ListElement(totalSubjects));
    for (uint32 n = 0; n < totalSubjects; n++)
		result += new(alloc) ListElement(elements[n]->toList(alloc));
	return result;
}


//
// Create a ThresholdAclSubject
//
ThresholdAclSubject *ThresholdAclSubject::Maker::make(const TypedList &list) const
{
    // pick apart the input list
    if (list.length() < 4)	// head + "n" + "k" + at least one subSubject
        CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
    uint32 minimumNeeded = getWord(list[1], 1);
    uint32 totalSubjects = getWord(list[2], minimumNeeded);
    if (list.length() != 3 + totalSubjects)
        CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);

    // now compile the subSubjects
    AclSubjectVector elements(totalSubjects);
    const ListElement *subSubject = &list[3];
    for (uint32 n = 0; n < totalSubjects; n++, subSubject = subSubject->next())
        elements[n] = ObjectAcl::make(subSubject->typedList());
	return new ThresholdAclSubject(totalSubjects, minimumNeeded, elements);
}

ThresholdAclSubject *ThresholdAclSubject::Maker::make(Version, Reader &pub, Reader &priv) const
{
    Endian<uint32> totalSubjects; pub(totalSubjects);
    Endian<uint32> minimumNeeded; pub(minimumNeeded);
    AclSubjectVector subSubjects(totalSubjects);
    for (uint32 n = 0; n < totalSubjects; n++)
		subSubjects[n] = ObjectAcl::importSubject(pub, priv);
	return new ThresholdAclSubject(totalSubjects, minimumNeeded, subSubjects);
}

ThresholdAclSubject::ThresholdAclSubject(uint32 n, uint32 k,
    const AclSubjectVector &subSubjects)
: SimpleAclSubject(CSSM_ACL_SUBJECT_TYPE_THRESHOLD, CSSM_SAMPLE_TYPE_THRESHOLD),
  minimumNeeded(k), totalSubjects(n), elements(subSubjects)
{
}


//
// Export the subject to a memory blob
//
template <class Action>
void ThresholdAclSubject::exportBlobForm(Action &pub, Action &priv)
{
    pub(h2n(totalSubjects));
    pub(h2n(minimumNeeded));
    for (uint32 n = 0; n < totalSubjects; n++)
		ObjectAcl::exportSubject(elements[n], pub, priv);
}

void ThresholdAclSubject::exportBlob(Writer::Counter &pub, Writer::Counter &priv)
{ exportBlobForm(pub, priv); }

void ThresholdAclSubject::exportBlob(Writer &pub, Writer &priv)
{ exportBlobForm(pub, priv); }


#ifdef DEBUGDUMP

void ThresholdAclSubject::debugDump() const
{
	Debug::dump("Threshold(%ld of %ld)", minimumNeeded, totalSubjects);
	for (unsigned int n = 0; n < elements.size(); n++) {
		Debug::dump(" [");
		if (Version v = elements[n]->version())
			Debug::dump("V=%d ", v);
		elements[n]->debugDump();
		Debug::dump("]");
	}
}

#endif //DEBUGDUMP
