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
// aclsubject - abstract ACL subject implementation
//
#include <security_cdsa_utilities/cssmacl.h>
#include <security_cdsa_utilities/cssmbridge.h>
#include <security_utilities/endian.h>
#include <security_utilities/debugging.h>
#include <algorithm>
#include <cstdarg>


//
// Validation contexts
//
AclValidationContext::~AclValidationContext()
{ /* virtual */ }


void AclValidationContext::init(ObjectAcl *acl, AclSubject *subject)
{
	mAcl = acl;
	mSubject = subject;
}


const char *AclValidationContext::credTag() const
{
	return mCred ? mCred->tag() : NULL;
}

std::string AclValidationContext::s_credTag() const
{
	const char *s = this->credTag();
	return s ? s : "";
}

const char *AclValidationContext::entryTag() const
{
	return mEntryTag;
}

void AclValidationContext::entryTag(const char *tag)
{
	mEntryTag = (tag && tag[0]) ? tag : NULL;
}

void AclValidationContext::entryTag(const std::string &tag)
{
	mEntryTag = tag.empty() ? NULL : tag.c_str();
}


//
// Common (basic) features of AclSubjects
//
AclSubject::AclSubject(uint32 type, Version v /* = 0 */)
	: mType(type), mVersion(v)
{
	assert(!(type & versionMask));
}

AclSubject::~AclSubject()
{ }

AclValidationEnvironment::~AclValidationEnvironment()
{ }

Adornable &AclValidationEnvironment::store(const AclSubject *subject)
{
	CssmError::throwMe(CSSM_ERRCODE_ACL_SUBJECT_TYPE_NOT_SUPPORTED);
}

void AclSubject::exportBlob(Writer::Counter &, Writer::Counter &)
{ }

void AclSubject::exportBlob(Writer &, Writer &)
{ }

void AclSubject::importBlob(Reader &, Reader &)
{ }

void AclSubject::reset()
{ }

AclSubject::Maker::~Maker()
{
}


//
// A SimpleAclSubject accepts only a single type of sample, validates
// samples independently, and makes no use of certificates.
//
bool SimpleAclSubject::validate(const AclValidationContext &ctx) const
{
    for (uint32 n = 0; n < ctx.count(); n++) {
        const TypedList &sample = ctx[n];
        if (!sample.isProper())
            CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
        if (sample.type() == type() && validate(ctx, sample)) {
			ctx.matched(ctx[n]);
            return true;	// matched this sample; validation successful
		}
    }
    return false;
}


//
// AclSubjects always have a (virtual) dump method.
// It's empty unless DEBUGDUMP is enabled.
//
void AclSubject::debugDump() const
{
#if defined(DEBUGDUMP)
	switch (type()) {
	case CSSM_ACL_SUBJECT_TYPE_ANY:
		Debug::dump("ANY");
		break;
	default:
		Debug::dump("subject type=%d", type());
		break;
	}
#endif //DEBUGDUMP
}

#if defined(DEBUGDUMP)

void AclSubject::dump(const char *title) const
{
	Debug::dump(" ** %s ", title);
	this->debugDump();
	Debug::dump("\n");
}

#endif //DEBUGDUMP
