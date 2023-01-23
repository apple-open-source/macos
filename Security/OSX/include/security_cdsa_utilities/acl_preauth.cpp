/*
 * Copyright (c) 2000-2004,2006-2007,2011,2013 Apple Inc. All Rights Reserved.
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
// acl_preauth - a subject type for modeling PINs and similar slot-specific
//		pre-authentication schemes.
//
#include "acl_preauth.h"
#include <security_utilities/debugging.h>


namespace Security {
namespace PreAuthorizationAcls {


//
// Origin forms
//
AclSubject *OriginMaker::make(const TypedList &list) const
{
	ListElement *args[1];
	crack(list, 1, args, CSSM_LIST_ELEMENT_WORDID);
		return new OriginAclSubject(*args[0]);
}

AclSubject *OriginMaker::make(AclSubject::Version version, Reader &pub, Reader &) const
{
	// just an integer containing the auth tag
	Endian<uint32> auth;
	pub(auth);
	return new OriginAclSubject(AclAuthorization(auth));
}


//
// Validate the origin form.
// This tries to find the source AclObject and hands the question off to it.
// If anything isn't right, fail the validation.
//
bool OriginAclSubject::validates(const AclValidationContext &ctx) const
{
	if (Environment *env = ctx.environment<Environment>())
		if (ObjectAcl *source = env->preAuthSource())
			if (source->validates(mAuthTag, ctx.cred(), ctx.environment()))
				return true;

	// no joy (the sad default)
	return false;
}


CssmList OriginAclSubject::toList(Allocator &alloc) const
{
	return TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_PREAUTH,
		new(alloc) ListElement(mAuthTag));
}

OriginAclSubject::OriginAclSubject(AclAuthorization auth)
	: AclSubject(CSSM_ACL_SUBJECT_TYPE_PREAUTH), mAuthTag(auth)
{
	if (auth < CSSM_ACL_AUTHORIZATION_PREAUTH_BASE || auth >= CSSM_ACL_AUTHORIZATION_PREAUTH_END)
		CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
}


//
// Origin exported form is just a four-byte integer (preauth authorization tag)
//
void OriginAclSubject::exportBlob(Writer::Counter &pub, Writer::Counter &priv)
{
	Endian<uint32> auth = mAuthTag;
	pub(auth);
}

void OriginAclSubject::exportBlob(Writer &pub, Writer &priv)
{
	Endian<uint32> auth = mAuthTag;
	pub(auth);
}


//
// Now for the other side of the coin.
// SourceAclSubjects describe the unusual side (for ACL management) of this game.
// The AclSubject of a preauth source MUST be of PREAUTH_SOURCE type. This subject
// contains the actual validation conditions as a sub-subject, and may provide
// additional information to represent known state of the preauth system.
//
// Think of the extra data in a PreAuthSource ACL as "current state informational"
// that only exists internally, and in the CssmList view. It does not get put into
// persistent (externalized) ACL storage at all. (After all, there's nothing persistent
// about it.)
//
AclSubject *SourceMaker::make(const TypedList &list) const
{
	// minimum requirement: item[1] = sub-subject (sublist)
	if (list.length() < 2)
        CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
	ListElement &sub = list[1];
	RefPointer<AclSubject> subSubject = ObjectAcl::make(sub);
	
	// anything else is interpreted as tracking state (defaulted if missing)
	switch (list.length()) {
	case 2:		// no tracking state
		return new SourceAclSubject(subSubject);
	case 3:
		if (list[2].type() == CSSM_LIST_ELEMENT_WORDID)
			return new SourceAclSubject(subSubject, list[2]);
		// fall through
	default:
		CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
	}
}

AclSubject *SourceMaker::make(AclSubject::Version version, Reader &pub, Reader &priv) const
{
	// external form does not contain tracking state - defaults to unknown
	RefPointer<AclSubject> subSubject = ObjectAcl::importSubject(pub, priv);
	return new SourceAclSubject(subSubject);
}


//
// Source validation uses its own home-cooked validation context.
//
class SourceValidationContext : public AclValidationContext {
public:
	SourceValidationContext(const AclValidationContext &base)
		: AclValidationContext(base), mCredTag(base.entryTag()) { }
		
	uint32 count() const	{ return cred() ? cred()->samples().length() : 0; }
	uint32 size() const		{ return count(); }
	const TypedList &sample(uint32 n) const
	{ assert(n < count()); return cred()->samples()[n]; }
	
	const char *credTag() const { return mCredTag; }	// override
	
	void matched(const TypedList *) const { }	//@@@ prelim
	
private:
	const char *mCredTag;
};

bool SourceAclSubject::SourceAclSubject::validates(const AclValidationContext &baseCtx) const
{
	// try to authenticate our sub-subject
	if (Environment *env = baseCtx.environment<Environment>()) {
		AclAuthorization auth = baseCtx.authorization();
		if (!CSSM_ACL_AUTHORIZATION_IS_PREAUTH(auth))	// all muddled up; bail
			CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
		uint32 slot = CSSM_ACL_AUTHORIZATION_PREAUTH_SLOT(auth);
		secinfo("preauth", "using state %d@%p", slot, &env->store(this));
		bool &accepted = env->store(this).attachment<AclState>((void *)((size_t) slot)).accepted;
		if (!accepted) {
			secinfo("preauth", "%p needs to authenticate its subject", this);
			SourceValidationContext ctx(baseCtx);
			if (mSourceSubject->validates(ctx)) {
				secinfo("preauth", "%p pre-authenticated", this);
				accepted = true;
			}
		}
		return accepted;
	}
	return false;
}


CssmList SourceAclSubject::toList(Allocator &alloc) const
{
	return TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_PREAUTH_SOURCE,
		new(alloc) ListElement(mSourceSubject->toList(alloc)));
}


SourceAclSubject::SourceAclSubject(AclSubject *subSubject, CSSM_ACL_PREAUTH_TRACKING_STATE state)
	: AclSubject(CSSM_ACL_SUBJECT_TYPE_PREAUTH),
	  mSourceSubject(subSubject)
{
}


//
// Export the subject to a memory blob
//
void SourceAclSubject::exportBlob(Writer::Counter &pub, Writer::Counter &priv)
{
	mSourceSubject->exportBlob(pub, priv);
}

void SourceAclSubject::exportBlob(Writer &pub, Writer &priv)
{
	mSourceSubject->exportBlob(pub, priv);
	// tracking state is not exported
}


#ifdef DEBUGDUMP

void OriginAclSubject::debugDump() const
{
	Debug::dump("Preauth(to slot %d)", mAuthTag - CSSM_ACL_AUTHORIZATION_PREAUTH_BASE);
}

void SourceAclSubject::debugDump() const
{
	Debug::dump("Preauth source: ");
	if (mSourceSubject)
		mSourceSubject->debugDump();
	else
		Debug::dump("NULL?");
}

#endif //DEBUGDUMP


}	//  namespace PreAuthorizationAcls
}	//  namespace Security
