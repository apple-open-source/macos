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
// cssmacl - core ACL management interface
//
#include <Security/cssmacl.h>
#include <Security/debugging.h>
#include <algorithm>
#include <cstdarg>
#include <endian.h>

using namespace DataWalkers;


//
// The static map of available ACL subject makers.
// These are the kinds of ACL subjects we can deal with.
//
ModuleNexus<ObjectAcl::MakerMap> ObjectAcl::makers;


//
// Common (basic) features of AclSubjects
//
AclSubject::~AclSubject()
{ }

AclValidationEnvironment::~AclValidationEnvironment()
{ }

void AclSubject::exportBlob(Writer::Counter &, Writer::Counter &)
{ }

void AclSubject::exportBlob(Writer &, Writer &)
{ }

void AclSubject::importBlob(Reader &, Reader &)
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
        if (sample.type() == acceptingSamples && validate(ctx, sample))
            return true;	// matched this sample; validation successful
    }
    return false;
}


//
// Create an ObjectAcl
//
ObjectAcl::ObjectAcl(CssmAllocator &alloc) : allocator(alloc), nextHandle(1)
{
}

ObjectAcl::ObjectAcl(const AclEntryPrototype &proto, CssmAllocator &alloc)
    : allocator(alloc), nextHandle(1)
{
    cssmSetInitial(proto);
}

ObjectAcl::~ObjectAcl()
{ }


//
// Set an "initial ACL" from a CSSM-style initial ACL argument.
// This will replace the owner, as well as replace the entire ACL
// with a single-item slot, as per CSSM specification.
//
void ObjectAcl::cssmSetInitial(const AclEntryPrototype &proto)
{
    owner = OwnerEntry(proto);
    entries.insert(EntryMap::value_type(proto.tag(), proto))->second.handle = nextHandle++;
	IFDUMPING("acl", debugDump("create/proto"));
}

void ObjectAcl::cssmSetInitial(const AclSubjectPointer &subject)
{
    owner = OwnerEntry(subject);
    entries.insert(EntryMap::value_type("", subject))->second.handle = nextHandle++;
	IFDUMPING("acl", debugDump("create/subject"));
}

ObjectAcl::Entry::~Entry()
{
}

AclValidationContext::~AclValidationContext()
{
}

//
// ObjectAcl::validate validates an access authorization claim.
// Returns normally if 'auth' is granted to the bearer of 'cred'.
// Otherwise, throws a suitable (ACL-related) CssmError exception.
// @@@ Should it return a reference to the Entry that granted access?
//
class BaseValidationContext : public AclValidationContext {
public:
    BaseValidationContext(const AccessCredentials *cred,
        AclAuthorization auth, AclValidationEnvironment *env)
        : AclValidationContext(cred, auth, env) { }
    
    uint32 count() const { return mCred ? mCred->samples().length() : 0; }
    const TypedList &sample(uint32 n) const
    { assert(n < count()); return mCred->samples()[n]; }
};

void ObjectAcl::validate(AclAuthorization auth, const AccessCredentials *cred,
    AclValidationEnvironment *env)
{
	// make sure we are ready to go
	instantiateAcl();

    //@@@ should pre-screen based on requested auth, maybe?
    BaseValidationContext ctx(cred, auth, env);

#if defined(ACL_OMNIPOTENT_OWNER)
    // try owner (owner can do anything)
    if (owner.validate(ctx))
        return;
#endif //ACL_OMNIPOTENT_OWNER

    // try applicable ACLs
    pair<ConstIterator, ConstIterator> range;
    if (getRange(cred->EntryTag, range) == 0)	// no such tag
        CssmError::throwMe(CSSM_ERRCODE_ACL_ENTRY_TAG_NOT_FOUND);
    // try entries in turn
    for (ConstIterator it = range.first; it != range.second; it++) {
        const AclEntry &slot = it->second;
        if (slot.authorizes(ctx.authorization()) && slot.validate(ctx))
            return;	// passed
    }
    CssmError::throwMe(CSSM_ERRCODE_OPERATION_AUTH_DENIED);	//@@@ imprecise
}

void ObjectAcl::validateOwner(AclAuthorization authorizationHint,
	const AccessCredentials *cred, AclValidationEnvironment *env)
{
	instantiateAcl();
    BaseValidationContext ctx(cred, authorizationHint, env);
    if (owner.validate(ctx))
        return;
    CssmError::throwMe(CSSM_ERRCODE_OPERATION_AUTH_DENIED);
}


//
// Export an ObjectAcl to two memory blobs: public and private data separated.
// This is a standard two-pass size+copy operation.
//
void ObjectAcl::exportBlob(CssmData &publicBlob, CssmData &privateBlob)
{
    Writer::Counter pubSize, privSize;
	Endian<uint32> entryCount = entries.size();
    owner.exportBlob(pubSize, privSize);
	pubSize(entryCount);
    for (Iterator it = begin(); it != end(); it++)
        it->second.exportBlob(pubSize, privSize);
    publicBlob = CssmData(allocator.malloc(pubSize), pubSize);
    privateBlob = CssmData(allocator.malloc(privSize), privSize);
    Writer pubWriter(publicBlob), privWriter(privateBlob);
    owner.exportBlob(pubWriter, privWriter);
	pubWriter(entryCount);
    for (Iterator it = begin(); it != end(); it++)
        it->second.exportBlob(pubWriter, privWriter);
	IFDUMPING("acl", debugDump("exported"));
}


//
// Import an ObjectAcl's contents from two memory blobs representing public and
// private contents, respectively. These blobs must have been generated by the
// export method.
// Prior contents (if any) are deleted and replaced.
//
void ObjectAcl::importBlob(const void *publicBlob, const void *privateBlob)
{
    Reader pubReader(publicBlob), privReader(privateBlob);
    owner.importBlob(pubReader, privReader);
	Endian<uint32> entryCountIn; pubReader(entryCountIn);
	uint32 entryCount = entryCountIn;

	entries.erase(begin(), end());
	for (uint32 n = 0; n < entryCount; n++) {
		AclEntry newEntry;
		newEntry.importBlob(pubReader, privReader);
		entries.insert(EntryMap::value_type(newEntry.tag, newEntry))->second.handle = nextHandle++;
	}
	IFDUMPING("acl", debugDump("imported"));
}


//
// Import/export helpers for subjects.
// This is exported to (subject implementation) callers to maintain consistency
// in binary format handling.
//
AclSubject *ObjectAcl::importSubject(Reader &pub, Reader &priv)
{
    Endian<uint32> typeAndVersion; pub(typeAndVersion);
	return make(typeAndVersion, pub, priv);
}


//
// Setup/update hooks
//
void ObjectAcl::instantiateAcl()
{
	// nothing by default
}

void ObjectAcl::changedAcl()
{
	// nothing by default
}


//
// ACL utility methods
//
unsigned int ObjectAcl::getRange(const char *tag, pair<ConstIterator, ConstIterator> &range) const
{
    if (tag && tag[0]) {	// tag restriction in effect
        range = entries.equal_range(tag);
        uint32 count = entries.count(tag);
        if (count == 0)
            CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_ENTRY_TAG);
        return count;
    } else {				// try all tags
        range.first = entries.begin();
        range.second = entries.end();
        return entries.size();
    }
}

ObjectAcl::Iterator ObjectAcl::findEntryHandle(CSSM_ACL_HANDLE handle)
{
    for (Iterator it = entries.begin(); it != entries.end(); it++)
        if (it->second.handle == handle)
            return it;
    CssmError::throwMe(CSSMERR_CSSM_INVALID_HANDLE_USAGE);	//%%% imprecise error code
}


//
// CSSM style ACL access and modification functions.
//
void ObjectAcl::cssmGetAcl(const char *tag, uint32 &count, AclEntryInfo * &acls)
{
	instantiateAcl();
    pair<ConstIterator, ConstIterator> range;
    count = getRange(tag, range);
    acls = allocator.alloc<AclEntryInfo>(count);
    uint32 n = 0;
    for (ConstIterator it = range.first; it != range.second; it++, n++) {
        acls[n].EntryHandle = it->second.handle;
        it->second.toEntryInfo(acls[n].EntryPublicInfo, allocator);
    }
    count = n;
}

void ObjectAcl::cssmChangeAcl(const AclEdit &edit,
	const AccessCredentials *cred, AclValidationEnvironment *env)
{
	IFDUMPING("acl", debugDump("acl-change-from"));

	// make sure we're ready to go
	instantiateAcl();

    // validate access credentials
    validateOwner(CSSM_ACL_AUTHORIZATION_CHANGE_ACL, cred, env);
    
    // what is Thy wish, effendi?
    switch (edit.EditMode) {
    case CSSM_ACL_EDIT_MODE_ADD: {
        AclEntry ent(Required(edit.newEntry()).proto());	//@@@ bypassing callback
        ent.handle = nextHandle++;
        entries.insert(EntryMap::value_type(edit.NewEntry->Prototype.EntryTag, ent));
        }
        break;
    case CSSM_ACL_EDIT_MODE_REPLACE: {
		// keep the handle, and try for some modicum of atomicity
        Iterator it = findEntryHandle(edit.OldEntryHandle);
		AclEntry ent(Required(edit.newEntry()).proto());
		ent.handle = edit.OldEntryHandle;
		entries.insert(EntryMap::value_type(edit.NewEntry->Prototype.EntryTag, ent));
		entries.erase(it);
        }
        break;
    case CSSM_ACL_EDIT_MODE_DELETE:
        entries.erase(findEntryHandle(edit.OldEntryHandle));
        break;
    default:
        CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_EDIT_MODE);
    }
	
	// notify change
	changedAcl();

	IFDUMPING("acl", debugDump("acl-change-to"));
}

void ObjectAcl::cssmGetOwner(AclOwnerPrototype &outOwner)
{
	instantiateAcl();
    outOwner.TypedSubject = owner.subject->toList(allocator);
    outOwner.Delegate = owner.delegate;
}

void ObjectAcl::cssmChangeOwner(const AclOwnerPrototype &newOwner,
                                const AccessCredentials *cred, AclValidationEnvironment *env)
{
	IFDUMPING("acl", debugDump("owner-change-from"));

	instantiateAcl();

    // only the owner entry can match
    validateOwner(CSSM_ACL_AUTHORIZATION_CHANGE_OWNER, cred, env);
        
    // okay, replace it
    owner = newOwner;
	
	changedAcl();

	IFDUMPING("acl", debugDump("owner-change-to"));
}


//
// Common features of ACL entries/owners
//
void ObjectAcl::Entry::init(const AclSubjectPointer &subject, bool delegate)
{
    this->subject = subject;
    this->delegate = delegate;
}

void ObjectAcl::Entry::importBlob(Reader &pub, Reader &priv)
{
    Endian<uint32> del;
	pub(del);  // read del from the public blob
	
	delegate = del;	// 4 bytes delegate flag
	subject = importSubject(pub, priv);
}


//
// An OwnerEntry is a restricted EntryPrototype for use as the ACL owner.
//
bool ObjectAcl::OwnerEntry::authorizes(AclAuthorization) const
{
    return true;	// owner can do anything
}

bool ObjectAcl::OwnerEntry::validate(const AclValidationContext &ctx) const
{
    return subject->validate(ctx);		// simple subject match - no strings attached
}


//
// An AclEntry has some extra goodies
//
ObjectAcl::AclEntry::AclEntry(const AclEntryPrototype &proto) : Entry(proto)
{
    tag = proto.tag();
    if (proto.authorization().contains(CSSM_ACL_AUTHORIZATION_ANY))
        authorizesAnything = true;	// anything else wouldn't add anything
    else if (proto.authorization().empty())
        authorizesAnything = true;	// not in standard, but common sense
    else {
        authorizesAnything = false;
        authorizations = proto.authorization();
    }
    //@@@ not setting time range
    // handle = not set here. Set by caller when the AclEntry is created.
}

ObjectAcl::AclEntry::AclEntry(const AclSubjectPointer &subject) : Entry(subject)
{
    authorizesAnything = true;	// by default, everything
    //@@@ not setting time range
}

void ObjectAcl::AclEntry::toEntryInfo(CSSM_ACL_ENTRY_PROTOTYPE &info, CssmAllocator &alloc) const
{
    info.TypedSubject = subject->toList(alloc);
    info.Delegate = delegate;
    info.Authorization = AuthorizationGroup(authorizations, alloc);
    //@@@ info.TimeRange = 
    assert(tag.length() <= CSSM_MODULE_STRING_SIZE);
    memcpy(info.EntryTag, tag.c_str(), tag.length() + 1);
}

bool ObjectAcl::AclEntry::authorizes(AclAuthorization auth) const
{
    return authorizesAnything || authorizations.find(auth) != authorizations.end();
}

bool ObjectAcl::AclEntry::validate(const AclValidationContext &ctx) const
{
    //@@@ not checking time ranges
    return subject->validate(ctx);
}

void ObjectAcl::AclEntry::importBlob(Reader &pub, Reader &priv)
{
    Entry::importBlob(pub, priv);
    const char *s; pub(s); tag = s;
    
	// authorizesAnything is on disk as a 4-byte flag
    Endian<uint32> tmpAuthorizesAnything;
    pub(tmpAuthorizesAnything);
    authorizesAnything = tmpAuthorizesAnything;
	
    authorizations.erase(authorizations.begin(), authorizations.end());
    if (!authorizesAnything) {
        Endian<uint32> countIn; pub(countIn);
		uint32 count = countIn;
		
        for (uint32 n = 0; n < count; n++) {
            Endian<AclAuthorization> auth; pub(auth);
            authorizations.insert(auth);
        }
    }
    //@@@ import time range
}


//
// Subject factory and makers
//
AclSubject::Maker::Maker(CSSM_ACL_SUBJECT_TYPE type) : myType(type)
{
    ObjectAcl::makers()[type] = this;
}

AclSubject *ObjectAcl::make(const TypedList &list)
{
    if (!list.isProper())
        CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
    return makerFor(list.type()).make(list);
}

AclSubject *ObjectAcl::make(uint32 typeAndVersion, Reader &pub, Reader &priv)
{
	// this type is encoded as (version << 24) | type
    return makerFor(typeAndVersion & ~AclSubject::versionMask).make(typeAndVersion >> AclSubject::versionShift, pub, priv);
}

AclSubject::Maker &ObjectAcl::makerFor(CSSM_ACL_SUBJECT_TYPE type)
{
    AclSubject::Maker *maker = makers()[type];
    if (maker == NULL)
        CssmError::throwMe(CSSM_ERRCODE_ACL_SUBJECT_TYPE_NOT_SUPPORTED);
    return *maker;
}


//
// Parsing helper for subject makers.
// Note that count/array exclude the first element of list, which is the subject type wordid.
//
void AclSubject::Maker::crack(const CssmList &list, uint32 count, ListElement **array, ...)
{
    if (count != list.length() - 1)
        CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
    if (count > 0) {
        va_list args;
        va_start(args, array);
        ListElement *elem = list.first()->next();
        for (uint32 n = 0; n < count; n++, elem = elem->next()) {
            CSSM_LIST_ELEMENT_TYPE expectedType = va_arg(args, CSSM_LIST_ELEMENT_TYPE);
            if (elem->type() != expectedType)
                CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
            array[n] = elem;
        }
        va_end(args);
    }
}

CSSM_WORDID_TYPE AclSubject::Maker::getWord(const ListElement &elem,
    int min /*= 0*/, int max /*= INT_MAX*/)
{
    if (elem.type() != CSSM_LIST_ELEMENT_WORDID)
        CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
    CSSM_WORDID_TYPE value = elem;
    if (value < min || value > max)
        CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
    return value;
}


//
// Debug dumping support.
// Leave the ObjectAcl::debugDump method in (stubbed out)
// to keep the virtual table layout stable, and to allow
// proper linking in weird mix-and-match scenarios.
//
void ObjectAcl::debugDump(const char *what) const
{
#if defined(DEBUGDUMP)
	if (!what)
		what = "Dump";
	Debug::dump("%p ACL %s: %d entries\n", this, what, int(entries.size()));
	Debug::dump(" OWNER ["); owner.debugDump(); Debug::dump("]\n");
	for (ConstIterator it = begin(); it != end(); it++) {
		const AclEntry &ent = it->second;
		Debug::dump(" (%ld:%s) [", ent.handle, ent.tag.c_str());
		ent.debugDump();
		Debug::dump("]\n");
	}
	Debug::dump("%p ACL END\n", this);
#endif //DEBUGDUMP
}

void AclSubject::debugDump() const
{
#if defined(DEBUGDUMP)
	switch (type()) {
	case CSSM_ACL_SUBJECT_TYPE_ANY:
		Debug::dump("ANY");
		break;
	default:
		Debug::dump("subject type=%d", int(type()));
		break;
	}
#endif //DEBUGDUMP
}

#if defined(DEBUGDUMP)

void ObjectAcl::Entry::debugDump() const
{
	if (AclSubject::Version v = subject->version())
		Debug::dump("V=%d ", v);
	subject->debugDump();
	if (delegate)
		Debug::dump(" DELEGATE");
}

void ObjectAcl::AclEntry::debugDump() const
{
	Entry::debugDump();
	if (authorizesAnything) {
		Debug::dump(" auth(ALL)");
	} else {
		Debug::dump(" auth(");
		for (AclAuthorizationSet::iterator it = authorizations.begin();
				it != authorizations.end(); it++)
			Debug::dump(" %ld", *it);
		Debug::dump(")");
	}
}

#endif //DEBUGDUMP
