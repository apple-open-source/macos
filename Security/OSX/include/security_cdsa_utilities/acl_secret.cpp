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
// acl_secret - secret-validation password ACLs framework.
//
#include <security_cdsa_utilities/acl_secret.h>
#include <security_utilities/trackingallocator.h>
#include <security_utilities/debugging.h>
#include <security_utilities/endian.h>
#include <algorithm>


//
// Construct a secret-bearing ACL subject
//
SecretAclSubject::SecretAclSubject(Allocator &alloc,
		CSSM_ACL_SUBJECT_TYPE type, const CssmData &password)
	: SimpleAclSubject(type), allocator(alloc),
	  mSecret(alloc, password), mSecretValid(true), mCacheSecret(false)
{ }

SecretAclSubject::SecretAclSubject(Allocator &alloc,
		CSSM_ACL_SUBJECT_TYPE type, CssmManagedData &password)
    : SimpleAclSubject(type), allocator(alloc),
	  mSecret(alloc, password), mSecretValid(true), mCacheSecret(false)
{ }

SecretAclSubject::SecretAclSubject(Allocator &alloc,
		CSSM_ACL_SUBJECT_TYPE type, bool doCache)
	: SimpleAclSubject(type), allocator(alloc),
	  mSecret(alloc), mSecretValid(false), mCacheSecret(doCache)
{ }


//
// Set the secret after creation.
//
// These are const methods by design, even though they obvious (may) set
// a field in the SecretAclSubject. The fields are mutable, following the
// general convention that transient state in AclSubjects is mutable.
//
void SecretAclSubject::secret(const CssmData &s) const
{
	assert(!mSecretValid);	// can't re-set it
	if (mCacheSecret) {
		mSecret = s;
		mSecretValid = true;
		secdebug("aclsecret", "%p secret stored", this);
	} else
		secdebug("aclsecret", "%p refused to store secret", this);
}

void SecretAclSubject::secret(CssmManagedData &s) const
{
	assert(!mSecretValid);	// can't re-set it
	if (mCacheSecret) {
		mSecret = s;
		mSecretValid = true;
		secdebug("aclsecret", "%p secret stored", this);
	} else
		secdebug("aclsecret", "%p refused to store secret", this);
}


//
// Validate a secret.
// The subclass has to come up with the secret somehow. We just validate it.
//
bool SecretAclSubject::validate(const AclValidationContext &context,
    const TypedList &sample) const
{
	CssmAutoData secret(allocator);
	
	// try to get the secret; fail if we can't
	if (!getSecret(context, sample, secret))
		return false;
	
	// now validate the secret
	if (mSecretValid) {
		return mSecret == secret;
	} else if (Environment *env = context.environment<Environment>()) {
		TrackingAllocator alloc(Allocator::standard());
		TypedList data(alloc, type(), new(alloc) ListElement(secret.get()));
		CssmSample sample(data);
		AccessCredentials cred((SampleGroup(sample)), context.credTag());
		return env->validateSecret(this, &cred);
	} else {
		return false;
	}
}


#ifdef DEBUGDUMP

void SecretAclSubject::debugDump() const
{
	if (mSecretValid) {
		Debug::dump(" ");
		Debug::dumpData(mSecret.data(), mSecret.length());
	}
	if (mCacheSecret)
		Debug::dump("; CACHING");
}

#endif //DEBUGDUMP
