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
// cssmacl - core ACL management interface.
//
// This header once contain the entire canonical ACL machinery. It's been split up
// since, into objectacl.h and aclsubject.h. What remains is the PodWrapper for
// ResourceControlContext, because nobody else wants it.
//
#ifndef _CSSMACL
#define _CSSMACL

#include <security_cdsa_utilities/objectacl.h>
#include <security_cdsa_utilities/aclsubject.h>


namespace Security {


//
// This bastard child of two different data structure sets has no natural home.
// We'll take pity on it here.
//
class ResourceControlContext : public PodWrapper<ResourceControlContext, CSSM_RESOURCE_CONTROL_CONTEXT> {
public:
    ResourceControlContext() { clearPod(); }
    ResourceControlContext(const AclEntryInput &initial,
		const AccessCredentials *cred = NULL)
    { InitialAclEntry = initial; AccessCred = const_cast<AccessCredentials *>(cred); }
    
	AclEntryInput &input()		{ return AclEntryInput::overlay(InitialAclEntry); }
    operator AclEntryInput &()	{ return input(); }
    AccessCredentials *credentials() const { return AccessCredentials::overlay(AccessCred); }
	void credentials(const CSSM_ACCESS_CREDENTIALS *creds)
		{ AccessCred = const_cast<CSSM_ACCESS_CREDENTIALS *>(creds); }
};

} // end namespace Security


#endif //_CSSMACL
