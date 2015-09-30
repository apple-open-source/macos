/*
 * Copyright (c) 2000-2004,2006,2011,2013-2014 Apple Inc. All Rights Reserved.
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
#ifndef _ACLSUBJECT
#define _ACLSUBJECT

#include <security_cdsa_utilities/cssmaclpod.h>
#include <security_cdsa_utilities/cssmcred.h>
#include <security_utilities/refcount.h>
#include <security_utilities/globalizer.h>
#include <security_utilities/memutils.h>
#include <security_utilities/adornments.h>
#include <map>
#include <set>
#include <string>
#include <limits.h>


namespace Security {

class ObjectAcl;
class AclValidationContext;
class AclSubject;


//
// An AclValidationEnvironment can be subclassed to add context access to ACL subject
// validation. If you use ACL subject classes that need context beyond the credential
// structure itself, add that context to (a virtual subclass of) AclValidationContext, pass that
// to ObjectAcl::validate() along with the credentials, and have the Subject implementation
// access validationContext.environment().
//
class AclValidationEnvironment {
	friend class AclValidationContext;
public:
    virtual ~AclValidationEnvironment();	// ensure virtual methods (need dynamic_cast)
	
	// provide an Adornable for a given subject to store data in, or throw if none available (default)
	virtual Adornable &store(const AclSubject *subject);
};


//
// An AclValidationContext holds all context for an ACL evaluation in one
// package. It's designed to provide a uniform representation of credentials, plus
// any (trusted path and/or implicit) context information useful for ACL validation.
//
// Contexts are immutable (constant) for validators; they do not change at all
// during a validation exercise. Anything that should be mutable must go into
// the environment (which is indirect and modifyable).
//
class AclValidationContext {
	friend class ObjectAcl;
public:
    AclValidationContext(const AccessCredentials *cred,
        AclAuthorization auth, AclValidationEnvironment *env = NULL)
    : mAcl((ObjectAcl*) 0xDEADDEADDEADDEAD), mSubject((AclSubject*) 0xDEADDEADDEADDEAD), mCred(cred),
        mAuth(auth), mEnv(env), mEntryTag(NULL) { }
    AclValidationContext(const AclValidationContext &ctx)
    : mAcl(ctx.mAcl), mSubject(ctx.mSubject), mCred(ctx.mCred),
	  mAuth(ctx.mAuth), mEnv(ctx.mEnv), mEntryTag(NULL) { }
	virtual ~AclValidationContext();

    // access to (suitably focused) sample set
    virtual uint32 count() const = 0;	// number of samples
	uint32 size() const { return count(); } // alias
    virtual const TypedList &sample(uint32 n) const = 0;	// retrieve one sample
    const TypedList &operator [] (uint32 n) const { return sample(n); }
    
    // context access
    AclAuthorization authorization() const	{ return mAuth; }
	const AccessCredentials *cred() const	{ return mCred; }
	AclValidationEnvironment *environment() const { return mEnv; }
    template <class Env> Env *environment() const { return dynamic_cast<Env *>(mEnv); }
	AclSubject *subject() const				{ return mSubject; }
	ObjectAcl *acl() const					{ return mAcl; }

	// tag manipulation
	virtual const char *credTag() const;
	virtual const char *entryTag() const;
	std::string s_credTag() const;
	void entryTag(const char *tag);
	void entryTag(const std::string &tag);
	
	// selective match support - not currently implemented
	virtual void matched(const TypedList *match) const = 0;
	void matched(const TypedList &match) const { return matched(&match); }
	
private:
	void init(ObjectAcl *acl, AclSubject *subject);

private:
	ObjectAcl *mAcl;					// underlying ObjectAcl
	AclSubject *mSubject;				// subject being validated
    const AccessCredentials *mCred;		// original credentials
    AclAuthorization mAuth;				// action requested
    AclValidationEnvironment *mEnv;		// environmental context (if any)
	const char *mEntryTag;				// entry tag
};


//
// The AclSubject class models an ACL "subject" object. If you have a new ACL
// subject type or variant, you make a subclass of this (plus a suitable Maker).
//
// Note that AclSubjects can contain both configuration and state information.
// Configuration is set during AclSubject creation (passwords to check against,
// evaluation options, etc.) and are typically passed on in the externalized form;
// it is persistent.
// On the other hand, state is volatile and is lost when the AclSubject dies.
// This is stuff that accumulates during a particular lifetime, such as results
// of previous evaluations (for caching or more nefarious purposes).
// Be clear what each of your subclass members are, and document accordingly.
//
class AclSubject : public RefCount {
public:
    typedef LowLevelMemoryUtilities::Writer Writer;
    typedef LowLevelMemoryUtilities::Reader Reader;
	
	typedef uint8 Version;		// binary version marker
	static const int versionShift = 24;	// highest-order byte of type is version
	static const uint32 versionMask = 0xff000000;

public:
	explicit AclSubject(uint32 type, Version v = 0);
    virtual ~AclSubject();
    CSSM_ACL_SUBJECT_TYPE type() const { return mType; }
    
	// validation (evaluation) primitive
    virtual bool validate(const AclValidationContext &ctx) const = 0;
    
    // export to CSSM interface
    virtual CssmList toList(Allocator &alloc) const = 0;
        
    // export/import for save/restore interface
    virtual void exportBlob(Writer::Counter &pub, Writer::Counter &priv);
    virtual void exportBlob(Writer &pub, Writer &priv);
    virtual void importBlob(Reader &pub, Reader &priv);
	
	// binary compatibility version management. The version defaults to zero
	Version version() const	{ return mVersion; }
	
	// forget any validation-related state you have acquired
	virtual void reset();
	
	// debug suupport (dummied out but present for -UDEBUGDUMP)
	virtual void debugDump() const;
	IFDUMP(void dump(const char *title) const);
	
protected:
	void version(Version v)	{ mVersion = v; }
    
private:
    CSSM_ACL_SUBJECT_TYPE mType;
	Version mVersion;
    
public:
    class Maker {
    public:
        Maker(CSSM_ACL_SUBJECT_TYPE type);
		virtual ~Maker();
        
        uint32 type() const { return mType; }
        virtual AclSubject *make(const TypedList &list) const = 0;
        virtual AclSubject *make(Version version, Reader &pub, Reader &priv) const = 0;
		
    protected:
        // list parsing helpers
        static void crack(const CssmList &list, uint32 count,
            ListElement **array = NULL, ...);
        static CSSM_WORDID_TYPE getWord(const ListElement &list,
            int min = 0, int max = INT_MAX);
		
    private:
        CSSM_ACL_SUBJECT_TYPE mType;
    };
};


//
// A SimpleAclSubject validates a credential by scanning its samples
// one at a time, without any interactions between them. Thus its validate()
// can be a lot simpler.
// Note that this layer assumes that subject and sample types have the same
// value, as is typical when both are derived from a WORDID.
//
class SimpleAclSubject : public AclSubject {
public:
    SimpleAclSubject(CSSM_ACL_SUBJECT_TYPE type) : AclSubject(type) { }
    
    bool validate(const AclValidationContext &ctx) const;
    virtual bool validate(const AclValidationContext &baseCtx,
        const TypedList &sample) const = 0;
};


} // end namespace Security


#endif //_ACLSUBJECT
