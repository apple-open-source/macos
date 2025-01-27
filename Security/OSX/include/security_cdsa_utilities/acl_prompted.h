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
// acl_prompted - password-based validation with out-of-band prompting.
//
// This implements simple password-based subject types with out-of-band
// prompting (via SecurityAgent), somewhat as per the CSSM standard.
//
#ifndef _ACL_PROMPTED
#define _ACL_PROMPTED

#include <security_cdsa_utilities/acl_secret.h>


namespace Security {


//
// A PromptedAclSubject obtains its sample by prompting the user interactively
// through some prompting mechanism defined in the environment.
//
class PromptedAclSubject : public SecretAclSubject {
public:
    CssmList toList(Allocator &alloc) const;
    
    PromptedAclSubject(Allocator &alloc,
		const CssmData &prompt, const CssmData &password);
    PromptedAclSubject(Allocator &alloc,
		CssmManagedData &prompt, CssmManagedData &password);
	PromptedAclSubject(Allocator &alloc, const CssmData &prompt, bool cache = false);
    
    void exportBlob(Writer::Counter &pub, Writer::Counter &priv);
    void exportBlob(Writer &pub, Writer &priv);
	
	IFDUMP(void debugDump() const);
	
public:
	class Environment : virtual public AclValidationEnvironment {
	public:
		virtual bool getSecret(CssmOwnedData &secret,
			const CssmData &prompt) const = 0;
	};

public:
    class Maker : public SecretAclSubject::Maker {
    public:
    	Maker() : SecretAclSubject::Maker(CSSM_ACL_SUBJECT_TYPE_PROMPTED_PASSWORD) { }
    	PromptedAclSubject *make(const TypedList &list) const;
    	PromptedAclSubject *make(Version, Reader &pub, Reader &priv) const;
    };
	
protected:
	bool getSecret(const AclValidationContext &context,
		const TypedList &subject, CssmOwnedData &secret) const;

private:
	CssmAutoData mPrompt;		// transparently handled prompt data
};

} // end namespace Security


#endif //_ACL_PROMPTED
