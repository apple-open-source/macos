/*
 * Copyright (c) 2000-2004,2006 Apple Computer, Inc. All Rights Reserved.
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
// objectacl - core implementation of an ACL-bearing object
//
#ifndef _OBJECTACL
#define _OBJECTACL

#include <security_cdsa_utilities/aclsubject.h>
#include <security_utilities/globalizer.h>
#include <map>
#include <set>
#include <limits.h>


namespace Security {


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
    ObjectAcl(Allocator &alloc);
    ObjectAcl(const AclEntryPrototype &proto, Allocator &alloc);
	virtual ~ObjectAcl();
    
    Allocator &allocator;


	//
    // access control validation (evaluation)
	//
	
	// validate(): succeed or throw exception
    void validate(AclAuthorization auth, const AccessCredentials *cred,
        AclValidationEnvironment *env = NULL);
	void validate(AclValidationContext &ctx);
	
	// validates(): return true or false (or throw on error)
    bool validates(AclAuthorization auth, const AccessCredentials *cred,
        AclValidationEnvironment *env = NULL);
	bool validates(AclValidationContext &ctx);

	// owner validation (simpler)
    void validateOwner(AclAuthorization authorizationHint, const AccessCredentials *cred,
		AclValidationEnvironment *env = NULL);
	void validateOwner(AclValidationContext &ctx);

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
	
	// clear everything from this ACL (return it to un-initialized state)
	void clear();
	
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
                        Allocator &alloc) const; // encode copy in CSSM format

        virtual bool authorizes(AclAuthorization auth) const = 0;
        virtual bool validate(const AclValidationContext &ctx) const = 0;

		template <class Action>
		void exportBlob(Action &pub, Action &priv)
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

        bool authorizes(AclAuthorization auth) const;
        bool validate(const AclValidationContext &ctx) const;
    };
    
    class AclEntry : public Entry {
    public:
        std::string tag;						// entry tag
		AclAuthorizationSet authorizations;		// set of authorizations
        bool authorizesAnything;				// has the _ANY authorization tag
        //@@@ time range not yet implemented
        CSSM_ACL_HANDLE handle;					// entry handle
        
		AclEntry() { }							// invalid AclEntry
        AclEntry(const AclSubjectPointer &subject);
        AclEntry(const AclEntryPrototype &proto);
        
        void toEntryInfo(CSSM_ACL_ENTRY_PROTOTYPE &info,
                        Allocator &alloc) const; // encode copy in CSSM format
        
        bool authorizes(AclAuthorization auth) const;
        bool validate(const AclValidationContext &ctx) const;
        
        template <class Action>
        void exportBlob(Action &pub, Action &priv)
        {
            Entry::exportBlob(pub, priv);
            const char *s = tag.c_str(); pub(s);
            uint32 aa = authorizesAnything; pub(aa);
            if (!authorizesAnything) {
                Endian<uint32> count = (uint32)authorizations.size(); pub(count);
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
	static void exportSubject(AclSubject *subject, Action &pub, Action &priv)
	{
		Endian<uint32> typeAndVersion = subject->type() | subject->version() << AclSubject::versionShift;
		pub(typeAndVersion);
		subject->exportBlob(pub, priv);
	}
	static AclSubject *importSubject(Reader &pub, Reader &priv);

public:
    typedef std::multimap<string, AclEntry> EntryMap;

    EntryMap::iterator begin() { return mEntries.begin(); }
	EntryMap::iterator end() { return mEntries.end(); }
    EntryMap::const_iterator begin() const { return mEntries.begin(); }
    EntryMap::const_iterator end() const { return mEntries.end(); }

    unsigned int getRange(const std::string &tag,
		pair<EntryMap::const_iterator, EntryMap::const_iterator> &range) const;	
    EntryMap::iterator findEntryHandle(CSSM_ACL_HANDLE handle);

    // construct an AclSubject through the Maker registry (by subject type)
    static AclSubject *make(const TypedList &list);	// make from CSSM form
    static AclSubject *make(uint32 typeAndVersion,
                            Reader &pub, Reader &priv); // make from export form
	
protected:
	template <class Input>
	void owner(const Input &input);
	void entries(uint32 count, const AclEntryInfo *infos);

private:
	void add(const std::string &tag, const AclEntry &newEntry);
	void add(const std::string &tag, AclEntry newEntry, CSSM_ACL_HANDLE handle);

private:
    EntryMap mEntries;				// ACL entries indexed by tag
    OwnerEntry mOwner;				// ACL owner entry
    CSSM_ACL_HANDLE mNextHandle;	// next unused entry handle value

private:
    typedef map<CSSM_ACL_SUBJECT_TYPE, AclSubject::Maker *> MakerMap;
    static ModuleNexus<MakerMap> makers;	// registered subject Makers

    static AclSubject::Maker &makerFor(CSSM_ACL_SUBJECT_TYPE type);
};


} // end namespace Security


#endif //_OBJECTACL
