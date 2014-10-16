/*
 * Copyright (c) 2000-2004,2006-2007,2009 Apple Inc. All Rights Reserved.
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
// acl_keychain - a subject type for the protected-path
//		keychain prompt interaction model.
//
#ifndef _ACL_KEYCHAIN
#define _ACL_KEYCHAIN

#include <security_cdsa_utilities/cssmacl.h>
#include <security_agent_client/agentclient.h>
#include <string>


//
// This is the actual subject implementation class
//
class KeychainPromptAclSubject : public SimpleAclSubject {
	static const Version pumaVersion = 0;	// 10.0, 10.1 -> default selector (not stored)
	static const Version jaguarVersion = 1;	// 10.2 et al -> first version selector
	static const Version currentVersion = jaguarVersion; // what we write today
public:
    bool validate(const AclValidationContext &baseCtx, const TypedList &sample) const;
    CssmList toList(Allocator &alloc) const;
    bool hasAuthorizedForSystemKeychain() const;
    
    KeychainPromptAclSubject(string description, const CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR &selector);
    
    void exportBlob(Writer::Counter &pub, Writer::Counter &priv);
    void exportBlob(Writer &pub, Writer &priv);
	
	uint32_t selectorFlags() const			{ return selector.flags; }
	bool selectorFlag(uint32_t flag) const	{ return selectorFlags() & flag; }
	
	IFDUMP(void debugDump() const);

public:
    class Maker : public AclSubject::Maker {
		friend class KeychainPromptAclSubject;
    public:
    	Maker(uint32_t mode)
			: AclSubject::Maker(CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT) { defaultMode = mode; }
    	KeychainPromptAclSubject *make(const TypedList &list) const;
    	KeychainPromptAclSubject *make(Version version, Reader &pub, Reader &priv) const;
	
	private:
		static uint32_t defaultMode;
    };
    
private:
	CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR selector; // selector structure
    string description;				// description blob (string)
	
private:
	static CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR defaultSelector;
};


#endif //_ACL_KEYCHAIN
