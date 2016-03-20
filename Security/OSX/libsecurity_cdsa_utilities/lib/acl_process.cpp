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
// acl_process - Process-attribute ACL subject type.
//
#include <security_cdsa_utilities/acl_process.h>
#include <security_utilities/endian.h>
#include <algorithm>


//
// Validate a credential set against this subject.
// No credential is required for this match.
//
bool ProcessAclSubject::validates(const AclValidationContext &context) const
{
    // reality check (internal structure was validated when created)
    assert(select.uses(CSSM_ACL_MATCH_BITS));
    
    // access the environment
    Environment *env = context.environment<Environment>();
    if (env == NULL) {
        static Environment localEnvironment;
        env = &localEnvironment;
    }
    
    // match uid
    if (select.uses(CSSM_ACL_MATCH_UID)) {
        uid_t uid = env->getuid();
        if (!(uid == select.uid || (select.uses(CSSM_ACL_MATCH_HONOR_ROOT) && uid == 0)))
            return false;
    }
    
    // match gid
    if (select.uses(CSSM_ACL_MATCH_GID) && select.gid != env->getgid())
        return false;
        
    return true;
}


//
// Make a copy of this subject in CSSM_LIST form
//
CssmList ProcessAclSubject::toList(Allocator &alloc) const
{
    // all associated data is public (no secrets)
    //@@@ ownership of selector data is murky; revisit after leak-plugging pass
    CssmData sData(memcpy(alloc.alloc<CSSM_ACL_PROCESS_SUBJECT_SELECTOR>(),
        &select, sizeof(select)), sizeof(select));
	return TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_PROCESS,
        new(alloc) ListElement(sData));
}


//
// Create a ProcessAclSubject
//
ProcessAclSubject *ProcessAclSubject::Maker::make(const TypedList &list) const
{
    // crack input apart
	ListElement *selectorData;
	crack(list, 1, &selectorData, CSSM_LIST_ELEMENT_DATUM);
    AclProcessSubjectSelector selector;
    selectorData->extract(selector);
    
    // validate input
    if (selector.version != CSSM_ACL_PROCESS_SELECTOR_CURRENT_VERSION)
        CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
    if (!selector.uses(CSSM_ACL_MATCH_BITS))
        CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
        
    // okay
	return new ProcessAclSubject(selector);
}

ProcessAclSubject *ProcessAclSubject::Maker::make(Version, Reader &pub, Reader &priv) const
{
    AclProcessSubjectSelector selector; pub(selector);
	n2hi(selector.version);
	n2hi(selector.mask);
	n2hi(selector.uid);
	n2hi(selector.gid);
	return new ProcessAclSubject(selector);
}


//
// Export the subject to a memory blob
//
void ProcessAclSubject::exportBlob(Writer::Counter &pub, Writer::Counter &priv)
{
    pub(select);
}

void ProcessAclSubject::exportBlob(Writer &pub, Writer &priv)
{
	AclProcessSubjectSelector temp;
	temp.version = h2n (select.version);
	temp.mask = h2n (select.mask);
	temp.uid = h2n (select.uid);
	temp.gid = h2n (select.gid);
    pub(temp);
}


//
// Implement the default methods of a ProcessEnvironment
//
uid_t ProcessAclSubject::Environment::getuid() const
{
    return ::getuid();
}

gid_t ProcessAclSubject::Environment::getgid() const
{
    return ::getgid();
}


#ifdef DEBUGDUMP

void ProcessAclSubject::debugDump() const
{
	Debug::dump("Process ");
	if (select.uses(CSSM_ACL_MATCH_UID)) {
		Debug::dump("uid=%d", int(select.uid));
		if (select.uses(CSSM_ACL_MATCH_HONOR_ROOT))
			Debug::dump("+root");
	}
	if (select.uses(CSSM_ACL_MATCH_GID))
		Debug::dump("gid=%d", int(select.gid));
}

#endif //DEBUGDUMP
