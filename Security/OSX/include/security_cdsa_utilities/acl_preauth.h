/*
 * Copyright (c) 2004,2006-2007,2011 Apple Inc. All Rights Reserved.
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
#ifndef _ACL_PREAUTH
#define _ACL_PREAUTH

#include <security_cdsa_utilities/cssmacl.h>
#include <string>


namespace Security {
namespace PreAuthorizationAcls {


class OriginMaker : public AclSubject::Maker {
protected:
	typedef LowLevelMemoryUtilities::Reader Reader;
	typedef LowLevelMemoryUtilities::Writer Writer;
public:
	OriginMaker() : AclSubject::Maker(CSSM_ACL_SUBJECT_TYPE_PREAUTH) { }
	AclSubject *make(const TypedList &list) const;
	AclSubject *make(AclSubject::Version version, Reader &pub, Reader &priv) const;
};

class SourceMaker : public AclSubject::Maker {
protected:
	typedef LowLevelMemoryUtilities::Reader Reader;
	typedef LowLevelMemoryUtilities::Writer Writer;
public:
	SourceMaker() : AclSubject::Maker(CSSM_ACL_SUBJECT_TYPE_PREAUTH_SOURCE) { }
	AclSubject *make(const TypedList &list) const;
	AclSubject *make(AclSubject::Version version, Reader &pub, Reader &priv) const;
};


//
// The actual designation of the PreAuth source AclBearer is provide by the environment.
//
class Environment : public virtual AclValidationEnvironment {
public:
	virtual ObjectAcl *preAuthSource() = 0;
};


//
// This is the object that is being "attached" (as an Adornment) to hold
// the pre-authorization state of a SourceAclSubject.
// The Adornable used for storage is determined by the Environment's store() method.
// 
struct AclState {
	AclState() : accepted(false) { }
	bool accepted;						// was previously accepted by upstream
};


//
// This is the "origin" subject class that gets created the usual way.
// It models a pre-auth "origin" - i.e. it points at a preauth slot and accepts
// its verdict on validation. Think of it as the "come from" part of the link.
//
class OriginAclSubject : public AclSubject {
public:
    bool validates(const AclValidationContext &ctx) const;
    CssmList toList(Allocator &alloc) const;
    
    OriginAclSubject(AclAuthorization auth);
    
    void exportBlob(Writer::Counter &pub, Writer::Counter &priv);
    void exportBlob(Writer &pub, Writer &priv);
	
	IFDUMP(void debugDump() const);
    
private:
	AclAuthorization mAuthTag;		// authorization tag referred to (origin only)
};


//
// The "source" subject class describes the other end of the link; the "go to" part
// if you will. Its sourceSubject is consulted for actual validation; and prior validation
// state is remembered (through the environment store facility) so that future validation
// attempts will automaticaly succeed (that's the "pre" in PreAuth).
//
class SourceAclSubject : public AclSubject {
public:
	bool validates(const AclValidationContext &ctx) const;
	CssmList toList(Allocator &alloc) const;
	
	SourceAclSubject(AclSubject *subSubject,
		CSSM_ACL_PREAUTH_TRACKING_STATE state = CSSM_ACL_PREAUTH_TRACKING_UNKNOWN);
	
	void exportBlob(Writer::Counter &pub, Writer::Counter &priv);
	void exportBlob(Writer &pub, Writer &priv);
	
	IFDUMP(void debugDump() const);
	
private:
	RefPointer<AclSubject> mSourceSubject;	// subject determining outcome (source only)
};



}	//  namespace PreAuthorizationAcls
}	//  namespace Security


#endif //_ACL_PREAUTH
