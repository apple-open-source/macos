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
// cssmacl - core ACL management interface.
//
// This file contains a set of C++ classes that implement ACLs in the local address space.
// ObjectAcl is the abstract interface to an implementation of a CSSM ACL. It supports
// the CSSM interfaces for ACL manipulation. AclSubject is the common parent of all
// types of ACL Subjects (in the CSSM sense); subclass this to implement a new subject type.
// AclValidationContext is an extensible, structured way of passing context information
// from the evaluation environment into particular subjects whose validation is context sensitive.
//
#ifndef _CSSMACL
#define _CSSMACL

#include <Security/cssmaclpod.h>
#include <Security/cssmcred.h>
#include <Security/refcount.h>
#include <Security/globalizer.h>
#include <Security/memutils.h>
#include <map>
#include <set>
#include <string>
#include <limits.h>


namespace Security {

class AclValidationContext;


//
// The AclSubject class models an ACL "subject" object.
// This is an abstract polymorphic class implementing various ACL subject types.
// Note that it does contain some common code to make everybody's life easier.
//
class AclSubject : public RefCount {
public:
    typedef LowLevelMemoryUtilities::Writer Writer;
    typedef LowLevelMemoryUtilities::Reader Reader;
	
	typedef uint8 Version;		// binary version marker
	static const int versionShift = 24;	// highest-order byte of type is version
	static const uint32 versionMask = 0xff000000;

    AclSubject(uint32 type) : mType(type), mVersion(0) { assert(!(type & versionMask)); }
    virtual ~AclSubject();
    uint32 type() const { return mType; }
    
    virtual bool validate(const AclValidationContext &ctx) const = 0;
    
    // export to CSSM interface
    virtual CssmList toList(CssmAllocator &alloc) const = 0;
        
    // export/import for save/restore interface
    virtual void exportBlob(Writer::Counter &pub, Writer::Counter &priv);
    virtual void exportBlob(Writer &pub, Writer &priv);
    virtual void importBlob(Reader &pub, Reader &priv);
	
	// binary compatibility version management. The version defaults to zero
	Version version() const	{ return mVersion; }
	void version(Version v)	{ mVersion = v; }
	
	// debug suupport (dummied out but present for -UDEBUGDUMP)
	virtual void debugDump() const;
    
private:
    CSSM_ACL_SUBJECT_TYPE mType;
	Version mVersion;
    
public:
    class Maker {
    public:
        Maker(CSSM_ACL_SUBJECT_TYPE type);
		virtual ~Maker();
        
        uint32 type() const { return myType; }
        virtual AclSubject *make(const TypedList &list) const = 0;
        virtual AclSubject *make(Version version, Reader &pub, Reader &priv) const = 0;
            
    protected:
        // list parsing helpers
        static void crack(const CssmList &list, uint32 count,
            ListElement **array = NULL, ...);
        static CSSM_WORDID_TYPE getWord(const ListElement &list,
            int min = 0, int max = INT_MAX);
        
    private:
        CSSM_ACL_SUBJECT_TYPE myType;
    };
};


//
// A SimpleAclSubject validates a credential by scanning its samples
// one at a time, without any interactions between them. Thus its validate()
// can be a lot simpler.
//
class SimpleAclSubject : public AclSubject {
public:
    SimpleAclSubject(CSSM_ACL_SUBJECT_TYPE su, CSSM_SAMPLE_TYPE sa)
    : AclSubject(su), acceptingSamples(sa) { }
    
    bool validate(const AclValidationContext &ctx) const;
    virtual bool validate(const AclValidationContext &baseCtx,
        const TypedList &sample) const = 0;
    
    const CSSM_SAMPLE_TYPE acceptingSamples;
};


//
// An AclValidationEnvironment can be subclassed to add context access to ACL subject
// validation. If you use ACL subject classes that need context beyond the credential
// structure itself, add that context to (a subclass of) AclValidationContext, pass that
// to ObjectAcl::validate() along with the credentials, and have the Subject implementation
// access validationContext.environment().
//
class AclValidationEnvironment {
public:
    virtual ~AclValidationEnvironment();	// ensure virtual methods (need dynamic_cast)
};


//
// An AclValidationContext holds credential information in a semi-transparent
// form. It's designed to provide a uniform representation of credentials, plus
// any (trusted path and/or implicit) context information useful for ACL validation.
//
class AclValidationContext {
public:
    AclValidationContext(const AccessCredentials *cred,
        AclAuthorization auth, AclValidationEnvironment *env = NULL)
    : mCred(cred), mAuth(auth), mEnv(env) { }
    AclValidationContext(const AclValidationContext &ctx)
    : mCred(ctx.mCred), mAuth(ctx.mAuth), mEnv(ctx.mEnv) { }
	virtual ~AclValidationContext();

    // access to (suitably focused) sample set
    virtual uint32 count() const = 0;	// number of samples
    virtual const TypedList &sample(uint32 n) const = 0;	// retrieve one sample
    const TypedList &operator [] (uint32 n) const { return sample(n); }
    
    // context access
    AclAuthorization authorization() const { return mAuth; }
    template <class Env>
    Env *environment() const { return dynamic_cast<Env *>(mEnv); }

    //@@@ add certificate access functions
    //@@@ add callback management

protected:
    const AccessCredentials *mCred;		// original credentials
    AclAuthorization mAuth;				// action requested
    AclValidationEnvironment *mEnv;		// environmental context (if any)
};


//
// An in-memory ACL object.
// This class implements an ACL-for-a-protected-object. It is complete in that
// it provides full ACL management functionality. You still need to (globally)
// register makers for the ACL subject types you want to use.
// Note that ObjectAcl does no integrity checking. ObjectAcl objects need to be
// protected from hostile access (by e.g. address space separation), and exported
// ACLs need to be protected somehow (by hiding, signing, or whatever works in
// your situation).
//
class ObjectAcl {
    friend AclSubject::Maker::Maker(CSSM_ACL_SUBJECT_TYPE);
    
public:
    typedef RefPointer<AclSubject> AclSubjectPointer;
    
    typedef LowLevelMemoryUtilities::Writer Writer;
    typedef LowLevelMemoryUtilities::Reader Reader;
    
public:
    ObjectAcl(CssmAllocator &alloc);
    ObjectAcl(const AclEntryPrototype &proto, CssmAllocator &alloc);
	virtual ~ObjectAcl();
    
    CssmAllocator &allocator;

    // access control validation: succeed or throw exception
    void validate(AclAuthorization auth, const AccessCredentials *cred,
        AclValidationEnvironment *env = NULL);
    void validateOwner(AclAuthorization authorizationHint, const AccessCredentials *cred,
		AclValidationEnvironment *env = NULL);

    // CSSM-style ACL access operations
	// (Gets are not const because underlying implementations usually want them writable) 
    void cssmGetAcl(const char *tag, uint32 &count, AclEntryInfo * &acls);
    void cssmChangeAcl(const AclEdit &edit, const AccessCredentials *cred,
		AclValidationEnvironment *env = NULL);
    void cssmGetOwner(AclOwnerPrototype &owner);
    void cssmChangeOwner(const AclOwnerPrototype &newOwner, const AccessCredentials *cred,
		AclValidationEnvironment *env = NULL);
    
    void cssmSetInitial(const AclEntryPrototype &proto);
    void cssmSetInitial(const AclSubjectPointer &subject);
        
    // Acl I/O (to/from memory blobs)
    void exportBlob(CssmData &publicBlob, CssmData &privateBlob);
    void importBlob(const void *publicBlob, const void *privateBlob);
	
	// setup hooks (called to delayed-construct the contents before use) - empty defaults
	virtual void instantiateAcl();	// called before ACL contents are used by external calls
	virtual void changedAcl();		// called after an ACL has been (possibly) changed
	
	// debug dump support (always there but stubbed out unless DEBUGDUMP)
	virtual void debugDump(const char *what = NULL) const;

public:
    class Entry {
    public:
        AclSubjectPointer subject;		// subject representation
        bool delegate;					// delegation flag
        
        Entry() { }						// make invalid Entry
        
        void toOwnerInfo(CSSM_ACL_OWNER_PROTOTYPE &info,
                        CssmAllocator &alloc) const; // encode copy in CSSM format

        virtual bool authorizes(AclAuthorization auth) const = 0;
        virtual bool validate(const AclValidationContext &ctx) const = 0;

		template <class Action>
		void ObjectAcl::Entry::exportBlob(Action &pub, Action &priv)
		{
			Endian<uint32> del = delegate; pub(del);	// 4 bytes delegate flag
			exportSubject(subject, pub, priv);	// subject itself (polymorphic)
		}
        void importBlob(Reader &pub, Reader &priv);
		
		IFDUMP(virtual void debugDump() const);
        
    private:
        void init(const AclSubjectPointer &subject, bool delegate = false);
        void init(const TypedList &subject, bool delegate = false) { init(make(subject), delegate); }

    protected:
        Entry(const AclEntryPrototype &proto) { init(proto.subject(), proto.delegate()); }
        Entry(const AclOwnerPrototype &proto) { init(proto.subject()); }
        Entry(const AclSubjectPointer &subject) { init(subject); }
        virtual ~Entry();
    };
    
    class OwnerEntry : public Entry {
    public:
        OwnerEntry() { }	// invalid OwnerEntry
        template <class Input>
        OwnerEntry(const Input &owner) : Entry(owner) { }
        OwnerEntry(const AclSubjectPointer &subject) : Entry(subject) { }

        bool authorizes(AclAuthorization auth) const;
        bool validate(const AclValidationContext &ctx) const;
    };
    
    class AclEntry : public Entry {
    public:
        std::string tag;						// entry tag
		AclAuthorizationSet authorizations;		// set of authorizations
        bool authorizesAnything;				// has the _ANY authorization tag
        //@@@ time range not yet implemented
        uint32 handle;							// entry handle
        
		AclEntry() { }							// invalid AclEntry
        AclEntry(const AclSubjectPointer &subject);
        AclEntry(const AclEntryPrototype &proto);
        
        void toEntryInfo(CSSM_ACL_ENTRY_PROTOTYPE &info,
                        CssmAllocator &alloc) const; // encode copy in CSSM format
        
        bool authorizes(AclAuthorization auth) const;
        bool validate(const AclValidationContext &ctx) const;
        
        template <class Action>
        void exportBlob(Action &pub, Action &priv)
        {
            Entry::exportBlob(pub, priv);
            const char *s = tag.c_str(); pub(s);
            uint32 aa = authorizesAnything; pub(aa);
            if (!authorizesAnything) {
                Endian<uint32> count = authorizations.size(); pub(count);
                for (AclAuthorizationSet::iterator it = authorizations.begin();
                    it != authorizations.end(); it++) {
                    Endian<AclAuthorization> auth = *it; pub(auth);
                }
            }
            //@@@ export time range
        }
        void importBlob(Reader &pub, Reader &priv);
		
		IFDUMP(void debugDump() const);
    };
	
public:
	// These helpers deal with transferring one subject from/to reader/writer streams.
	// You'd usually only call those from complex subject implementations (e.g. threshold)
	template <class Action>
	static void ObjectAcl::exportSubject(AclSubject *subject, Action &pub, Action &priv)
	{
		Endian<uint32> typeAndVersion = subject->type() | subject->version() << AclSubject::versionShift;
		pub(typeAndVersion);
		subject->exportBlob(pub, priv);
	}
	static AclSubject *importSubject(Reader &pub, Reader &priv);

public:
    typedef std::multimap<string, AclEntry> EntryMap;
    typedef EntryMap::iterator Iterator;
    typedef EntryMap::const_iterator ConstIterator;

    Iterator begin() { return entries.begin(); }
    Iterator end() { return entries.end(); }
    ConstIterator begin() const { return entries.begin(); }
    ConstIterator end() const { return entries.end(); }

    unsigned int getRange(const char *tag, pair<ConstIterator, ConstIterator> &range) const;	
    Iterator findEntryHandle(CSSM_ACL_HANDLE handle);

    // construct an AclSubject through the Maker registry (by subject type)
    static AclSubject *make(const TypedList &list);	// make from CSSM form
    static AclSubject *make(uint32 typeAndVersion,
                            Reader &pub, Reader &priv); // make from export form
    
private:
    EntryMap entries;				// ACL entries indexed by tag
    OwnerEntry owner;				// ACL owner entry
    uint32 nextHandle;				// next unused entry handle value

private:
    typedef map<CSSM_ACL_SUBJECT_TYPE, AclSubject::Maker *> MakerMap;
    static ModuleNexus<MakerMap> makers;	// registered subject Makers
    
    static AclSubject::Maker &makerFor(CSSM_ACL_SUBJECT_TYPE type);
};


//
// This bastard child of two different data structure sets has no natural home.
// We'll take pity on it here.
//
class ResourceControlContext : public PodWrapper<ResourceControlContext, CSSM_RESOURCE_CONTROL_CONTEXT> {
public:
    ResourceControlContext() { clearPod(); }
    ResourceControlContext(const AclEntryInput &initial, AccessCredentials *cred = NULL)
    { InitialAclEntry = initial; AccessCred = cred; }
    
	AclEntryInput &input()		{ return AclEntryInput::overlay(InitialAclEntry); }
    operator AclEntryInput &()	{ return input(); }
    AccessCredentials *credentials() const { return AccessCredentials::overlay(AccessCred); }
	void credentials(const CSSM_ACCESS_CREDENTIALS *creds)
		{ AccessCred = const_cast<CSSM_ACCESS_CREDENTIALS *>(creds); }
};

} // end namespace Security


#endif //_CSSMACL
