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
// acl_process - Process-attribute ACL subject type.
//
// NOTE:
// The default Environment provides data about the current process (the one that
// validate() is run in). If this isn't right for you (e.g. because you want to
// validate against a process on the other side of some IPC connection), you must
// make your own version of Environment and pass it to validate().
//
#ifndef _ACL_PROCESS
#define _ACL_PROCESS

#include <Security/cssmacl.h>
#include <string>

#ifdef _CPP_ACL_PROCESS
#pragma export on
#endif

namespace Security
{

class AclProcessSubjectSelector
    : public PodWrapper<AclProcessSubjectSelector, CSSM_ACL_PROCESS_SUBJECT_SELECTOR> {
public:
    AclProcessSubjectSelector()
    { version = CSSM_ACL_PROCESS_SELECTOR_CURRENT_VERSION; mask = 0; }
    
    bool uses(uint32 m) const { return mask & m; }
};


//
// The ProcessAclSubject matches process attributes securely identified
// by the system across IPC channels.
//
class ProcessAclSubject : public SimpleAclSubject {
public:
    bool validate(const AclValidationContext &baseCtx, const TypedList &sample) const;
    CssmList toList(CssmAllocator &alloc) const;

    ProcessAclSubject(const AclProcessSubjectSelector &selector)
    : SimpleAclSubject(CSSM_ACL_SUBJECT_TYPE_PROCESS, CSSM_SAMPLE_TYPE_PROCESS),
      select(selector) { }

    void exportBlob(Writer::Counter &pub, Writer::Counter &priv);
    void exportBlob(Writer &pub, Writer &priv);
	
	IFDUMP(void debugDump() const);
    
public:
    class Environment : public virtual AclValidationEnvironment {
    public:
        virtual uid_t getuid() const;	// retrieve effective userid to match
        virtual gid_t getgid() const;	// retrieve effective groupid to match
    };
    
public:
    class Maker : public AclSubject::Maker {
    public:
    	Maker() : AclSubject::Maker(CSSM_ACL_SUBJECT_TYPE_PROCESS) { }
    	ProcessAclSubject *make(const TypedList &list) const;
    	ProcessAclSubject *make(Version, Reader &pub, Reader &priv) const;
    };

private:
    AclProcessSubjectSelector select;
};

} // end namespace Security

#ifdef _CPP_ACL_PROCESS
#pragma export off
#endif


#endif //_ACL_PROCESS
