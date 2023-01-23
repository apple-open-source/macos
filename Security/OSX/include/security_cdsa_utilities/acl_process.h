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
// NOTE:
// The default Environment provides data about the current process (the one that
// validate() is run in). If this isn't right for you (e.g. because you want to
// validate against a process on the other side of some IPC connection), you must
// make your own version of Environment and pass it to validate().
//
#ifndef _ACL_PROCESS
#define _ACL_PROCESS

#include <security_cdsa_utilities/cssmacl.h>
#include <string>

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
class ProcessAclSubject : public AclSubject {
public:
    bool validates(const AclValidationContext &baseCtx) const;
    CssmList toList(Allocator &alloc) const;

    ProcessAclSubject(const AclProcessSubjectSelector &selector)
    : AclSubject(CSSM_ACL_SUBJECT_TYPE_PROCESS),
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


#endif //_ACL_PROCESS
