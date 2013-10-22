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
#include <security_cdsa_utilities/objectacl.h>
#include <security_cdsa_utilities/cssmbridge.h>
#include <security_utilities/endian.h>
#include <security_utilities/debugging.h>
#include <algorithm>
#include <cstdarg>

#include <security_cdsa_utilities/acl_preauth.h>	//@@@ impure - will be removed

using namespace DataWalkers;


//
// The static map of available ACL subject makers.
// These are the kinds of ACL subjects we can deal with.
//
ModuleNexus<ObjectAcl::MakerMap> ObjectAcl::makers;


//
// Create an ObjectAcl
//
ObjectAcl::ObjectAcl(Allocator &alloc) : allocator(alloc), mNextHandle(1)
{
}

ObjectAcl::ObjectAcl(const AclEntryPrototype &proto, Allocator &alloc)
    : allocator(alloc), mNextHandle(1)
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
    mOwner = OwnerEntry(proto);
	add(proto.s_tag(), proto);
	IFDUMPING("acl", debugDump("create/proto"));
}

void ObjectAcl::cssmSetInitial(const AclSubjectPointer &subject)
{
    mOwner = OwnerEntry(subject);
	add("", subject);
	IFDUMPING("acl", debugDump("create/subject"));
}

ObjectAcl::Entry::~Entry()
{
}


//
// ObjectAcl::validate validates an access authorization claim.
// Returns normally if 'auth' is granted to the bearer of 'cred'.
// Otherwise, throws a suitable (ACL-related) CssmError exception.
//
class BaseValidationContext : public AclValidationContext {
public:
	BaseValidationContext(const AccessCredentials *cred,
		AclAuthorization auth, AclValidationEnvironment *env)
		: AclValidationContext(cred, auth, env) { }
	
	uint32 count() const	{ return cred() ? cred()->samples().length() : 0; }
	uint32 size() const		{ return count(); }
	const TypedList &sample(uint32 n) const
	{ assert(n < count()); return cred()->samples()[n]; }
	
	void matched(const TypedList *) const { }		// ignore match info
};


bool ObjectAcl::validates(AclAuthorization auth, const AccessCredentials *cred,
    AclValidationEnvironment *env)
{
	BaseValidationContext ctx(cred, auth, env);
	return validates(ctx);
}

bool ObjectAcl::validates(AclValidationContext &ctx)
{
	// make sure we are ready to go
	instantiateAcl();

	IFDUMPING("acleval", Debug::dump("<<WANT(%d)<", ctx.authorization()));

    //@@@ should pre-screen based on requested auth, maybe?

#if defined(ACL_OMNIPOTENT_OWNER)
    // try owner (owner can do anything)
    if (mOwner.validate(ctx))
        return;
#endif //ACL_OMNIPOTENT_OWNER

    // try applicable ACLs
    pair<EntryMap::const_iterator, EntryMap::const_iterator> range;
    if (getRange(ctx.s_credTag(), range) == 0)	// no such tag
        CssmError::throwMe(CSSM_ERRCODE_ACL_ENTRY_TAG_NOT_FOUND);
    // try each entry in turn
    for (EntryMap::const_iterator it = range.first; it != range.second; it++) {
        const AclEntry &slot = it->second;
		IFDUMPING("acleval", (Debug::dump(" EVAL["), slot.debugDump(), Debug::dump("]")));
        if (slot.authorizes(ctx.authorization())) {
			ctx.init(this, slot.subject);
			ctx.entryTag(slot.tag);
			if (slot.validate(ctx)) {
				IFDUMPING("acleval", Debug::dump(">PASS>>\n"));
				return true;		// passed
			}
			IFDUMPING("acleval", Debug::dump(" NO"));
		}
    }
	IFDUMPING("acleval", Debug::dump(">FAIL>>\n"));
	return false;	// no joy
}

void ObjectAcl::validate(AclAuthorization auth, const AccessCredentials *cred,
	AclValidationEnvironment *env)
{
	if (!validates(auth, cred, env))
		CssmError::throwMe(CSSM_ERRCODE_OPERATION_AUTH_DENIED);
}

void ObjectAcl::validate(AclValidationContext &ctx)
{
	if (!validates(ctx))
		CssmError::throwMe(CSSM_ERRCODE_OPERATION_AUTH_DENIED);
}


void ObjectAcl::validateOwner(AclAuthorization authorizationHint,
	const AccessCredentials *cred, AclValidationEnvironment *env)
{
    BaseValidationContext ctx(cred, authorizationHint, env);
	validateOwner(ctx);
}

void ObjectAcl::validateOwner(AclValidationContext &ctx)
{
    instantiateAcl();
    
    ctx.init(this, mOwner.subject);
    if (mOwner.validate(ctx))
        return;
    CssmError::throwMe(CSSM_ERRCODE_OPERATION_AUTH_DENIED);
}


//
// Export an ObjectAcl to two memory blobs: public and private data separated.
// This is a standard two-pass size+copy operation.
//
void ObjectAcl::exportBlob(CssmData &publicBlob, CssmData &privateBlob)
{
	instantiateAcl();
    Writer::Counter pubSize, privSize;
	Endian<uint32> entryCount = (uint32)mEntries.size();
    mOwner.exportBlob(pubSize, privSize);
	pubSize(entryCount);
    for (EntryMap::iterator it = begin(); it != end(); it++)
        it->second.exportBlob(pubSize, privSize);
    publicBlob = CssmData(allocator.malloc(pubSize), pubSize);
    privateBlob = CssmData(allocator.malloc(privSize), privSize);
    Writer pubWriter(publicBlob), privWriter(privateBlob);
    mOwner.exportBlob(pubWriter, privWriter);
	pubWriter(entryCount);
    for (EntryMap::iterator it = begin(); it != end(); it++)
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
    mOwner.importBlob(pubReader, privReader);
	Endian<uint32> entryCountIn; pubReader(entryCountIn);
	uint32 entryCount = entryCountIn;

	mEntries.erase(begin(), end());
	for (uint32 n = 0; n < entryCount; n++) {
		AclEntry newEntry;
		newEntry.importBlob(pubReader, privReader);
		add(newEntry.tag, newEntry);
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
unsigned int ObjectAcl::getRange(const std::string &tag,
	pair<EntryMap::const_iterator, EntryMap::const_iterator> &range) const
{
    if (!tag.empty()) {	// tag restriction in effect
        range = mEntries.equal_range(tag);
        unsigned int count = (unsigned int)mEntries.count(tag);
        if (count == 0)
            CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_ENTRY_TAG);
        return count;
    } else {				// try all tags
        range.first = mEntries.begin();
        range.second = mEntries.end();
        return (unsigned int)mEntries.size();
    }
}

ObjectAcl::EntryMap::iterator ObjectAcl::findEntryHandle(CSSM_ACL_HANDLE handle)
{
    for (EntryMap::iterator it = mEntries.begin(); it != mEntries.end(); it++)
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
    pair<EntryMap::const_iterator, EntryMap::const_iterator> range;
    count = getRange(tag ? tag : "", range);
    acls = allocator.alloc<AclEntryInfo>(count);
    uint32 n = 0;
    for (EntryMap::const_iterator it = range.first; it != range.second; it++, n++) {
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
		const AclEntryInput &input = Required(edit.newEntry());
		add(input.proto().s_tag(), input.proto());
		}
        break;
    case CSSM_ACL_EDIT_MODE_REPLACE: {
		// keep the handle, and try for some modicum of atomicity
        EntryMap::iterator it = findEntryHandle(edit.handle());
		AclEntryPrototype proto = Required(edit.newEntry()).proto(); // (bypassing callbacks)
		add(proto.s_tag(), proto, edit.handle());
		mEntries.erase(it);
        }
        break;
    case CSSM_ACL_EDIT_MODE_DELETE:
        mEntries.erase(findEntryHandle(edit.OldEntryHandle));
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
    outOwner.TypedSubject = mOwner.subject->toList(allocator);
    outOwner.Delegate = mOwner.delegate;
}

void ObjectAcl::cssmChangeOwner(const AclOwnerPrototype &newOwner,
                                const AccessCredentials *cred, AclValidationEnvironment *env)
{
	IFDUMPING("acl", debugDump("owner-change-from"));

	instantiateAcl();

    // only the owner entry can match
    validateOwner(CSSM_ACL_AUTHORIZATION_CHANGE_OWNER, cred, env);
        
    // okay, replace it
    mOwner = newOwner;
	
	changedAcl();

	IFDUMPING("acl", debugDump("owner-change-to"));
}


//
// Load a set of ACL entries from an AclEntryInfo array.
// This completely replaces the ACL's entries.
// Note that we will adopt the handles in the infos, so they better be proper
// (unique, nonzero).
//
template <class Input>
void ObjectAcl::owner(const Input &input)
{
	IFDUMPING("acl", debugDump("owner-load-old"));
	mOwner = OwnerEntry(input);
	IFDUMPING("acl", debugDump("owner-load-new"));
}

template void ObjectAcl::owner(const AclOwnerPrototype &);
template void ObjectAcl::owner(const AclSubjectPointer &);


void ObjectAcl::entries(uint32 count, const AclEntryInfo *info)
{
	IFDUMPING("acl", debugDump("entries-load-old"));
	mEntries.erase(mEntries.begin(), mEntries.end());
	for (uint32 n = 0; n < count; n++, info++)
		add(info->proto().s_tag(), info->proto());
	IFDUMPING("acl", debugDump("entries-load-new"));
}


//
// Clear out the ACL and return it to un-initialized state
//
void ObjectAcl::clear()
{
	mOwner = OwnerEntry();
	mEntries.erase(mEntries.begin(), mEntries.end());
	secdebug("acl", "%p cleared", this);
}


//
// Common gate to add an ACL entry
//
void ObjectAcl::add(const std::string &tag, const AclEntry &newEntry)
{
	add(tag, newEntry, mNextHandle++);
}

void ObjectAcl::add(const std::string &tag, AclEntry newEntry, CSSM_ACL_HANDLE handle)
{
	//@@@ This should use a hook-registry mechanism. But for now, we are explicit:
	if (!newEntry.authorizesAnything) {
		for (AclAuthorizationSet::const_iterator it = newEntry.authorizations.begin();
				it != newEntry.authorizations.end(); it++)
			if (*it >= CSSM_ACL_AUTHORIZATION_PREAUTH_BASE &&
					*it < CSSM_ACL_AUTHORIZATION_PREAUTH_END) {
				// preauthorization right - special handling
				if (newEntry.subject->type() != CSSM_ACL_SUBJECT_TYPE_PREAUTH_SOURCE)
					newEntry.subject =
						new PreAuthorizationAcls::SourceAclSubject(newEntry.subject);
			}
	}

	mEntries.insert(make_pair(tag, newEntry))->second.handle = handle;
	if (handle >= mNextHandle)
		mNextHandle = handle + 1;	// don't reuse this handle (in this ACL)
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
	// the delegation flag is 4 bytes for historic reasons
    Endian<uint32> del;
	pub(del);
	delegate = del;

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
    tag = proto.s_tag();
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

void ObjectAcl::AclEntry::toEntryInfo(CSSM_ACL_ENTRY_PROTOTYPE &info, Allocator &alloc) const
{
    info.TypedSubject = subject->toList(alloc);
    info.Delegate = delegate;
	info.Authorization = authorizesAnything ?
		AuthorizationGroup(CSSM_ACL_AUTHORIZATION_ANY, alloc) :
		AuthorizationGroup(authorizations, alloc);
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
AclSubject::Maker::Maker(CSSM_ACL_SUBJECT_TYPE type)
	: mType(type)
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
	Debug::dump("%p ACL %s: %d entries\n", this, what, int(mEntries.size()));
	Debug::dump(" OWNER ["); mOwner.debugDump(); Debug::dump("]\n");
	for (EntryMap::const_iterator it = begin(); it != end(); it++) {
		const AclEntry &ent = it->second;
		Debug::dump(" (%ld:%s) [", ent.handle, ent.tag.c_str());
		ent.debugDump();
		Debug::dump("]\n");
	}
	Debug::dump("%p ACL END\n", this);
#endif //DEBUGDUMP
}

#if defined(DEBUGDUMP)

void ObjectAcl::Entry::debugDump() const
{
	if (subject) {
		if (AclSubject::Version v = subject->version())
			Debug::dump("V=%d ", v);
		subject->debugDump();
	} else {
		Debug::dump("NULL subject");
	}
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
				it != authorizations.end(); it++) {
			if (*it >= CSSM_ACL_AUTHORIZATION_PREAUTH_BASE
					&& *it < CSSM_ACL_AUTHORIZATION_PREAUTH_END)
				Debug::dump(" PRE(%d)", *it - CSSM_ACL_AUTHORIZATION_PREAUTH_BASE);
			else
				Debug::dump(" %d", *it);
		}
		Debug::dump(")");
	}
}

#endif //DEBUGDUMP
