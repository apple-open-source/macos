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
//				  keychain prompt interaction model.
//
#ifndef _ACL_KEYCHAIN
#define _ACL_KEYCHAIN

#include <Security/cssmacl.h>
#include <string>

#ifdef _CPP_ACL_KEYCHAIN
#pragma export on
#endif

class KeychainPromptInterface;
class SecurityAgentClient;


//
// This is the actual subject implementation class
//
class KeychainPromptAclSubject : public SimpleAclSubject {
public:
    bool validate(const AclValidationContext &baseCtx, const TypedList &sample) const;
    CssmList toList(CssmAllocator &alloc) const;
    
    KeychainPromptAclSubject(KeychainPromptInterface &ifc, string description);
    
    void exportBlob(Writer::Counter &pub, Writer::Counter &priv);
    void exportBlob(Writer &pub, Writer &priv);

    class Maker : public AclSubject::Maker {
    public:
    	Maker(KeychainPromptInterface &ifc)
         : AclSubject::Maker(CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT),
           interface(ifc) { }
    	KeychainPromptAclSubject *make(const TypedList &list) const;
    	KeychainPromptAclSubject *make(Reader &pub, Reader &priv) const;
        
    private:
        KeychainPromptInterface &interface;
    };
    
private:
    KeychainPromptInterface &interface;
    string description;
};


//
// A KeychainPromptAcl needs to use some I/O facility to validate a credential.
// You must thus subclass this interface class (which acts as an AclSubject::Maker)
// to provide the actual testing interface. The subject type will take care of
// the formalities.
//
class KeychainPromptInterface {
public:
    KeychainPromptInterface() : maker(*this) { }

    virtual bool validate(string description) = 0;	// implement this
    
private:
    const KeychainPromptAclSubject::Maker maker;
};


#ifdef _CPP_ACL_KEYCHAIN
#pragma export off
#endif


#endif //_ACL_KEYCHAIN
