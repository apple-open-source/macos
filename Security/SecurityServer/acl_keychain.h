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
// acl_keychain - a subject type for the protected-path
//		keychain prompt interaction model.
//
#ifndef _ACL_KEYCHAIN
#define _ACL_KEYCHAIN

#include <Security/cssmacl.h>
#include "SecurityAgentClient.h"
#include <string>


//
// This is the actual subject implementation class
//
class KeychainPromptAclSubject : public SimpleAclSubject {
	static const Version pumaVersion = 0;	// 10.0, 10.1 -> default selector (not stored)
	static const Version jaguarVersion = 1;	// 10.2 et al -> first version selector
public:
    bool validate(const AclValidationContext &baseCtx, const TypedList &sample) const;
    CssmList toList(CssmAllocator &alloc) const;
    
    KeychainPromptAclSubject(string description, const CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR &selector);
    
    void exportBlob(Writer::Counter &pub, Writer::Counter &priv);
    void exportBlob(Writer &pub, Writer &priv);
	
	IFDUMP(void debugDump() const);

    class Maker : public AclSubject::Maker {
    public:
    	Maker(CSSM_ACL_SUBJECT_TYPE type = CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT)
			: AclSubject::Maker(type) { }
    	KeychainPromptAclSubject *make(const TypedList &list) const;
    	KeychainPromptAclSubject *make(Version version, Reader &pub, Reader &priv) const;
    };
    
private:
	CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR selector; // selector structure
    string description;				// description blob (string)
	
private:
	static CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR defaultSelector;
	
	typedef uint32 VersionMarker;
	static const VersionMarker currentVersion = 0x3BD5910D;
	
	bool isLegacyCompatible() const;
};


#endif //_ACL_KEYCHAIN
