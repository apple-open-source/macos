/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
// acl_codesigning - ACL subject for signature of calling application
//
#ifndef _H_ACL_CODESIGNING
#define _H_ACL_CODESIGNING

#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_utilities/cssmacl.h>
#include <security_utilities/codesigning.h>

namespace Security
{

using CodeSigning::Signature;
using CodeSigning::Signer;

//
// The CodeSignature subject type matches a code signature applied to the
// disk image that originated the client process.
//
class CodeSignatureAclSubject : public AclSubject {
public:
    bool validate(const AclValidationContext &baseCtx) const;
    CssmList toList(Allocator &alloc) const;
    
    CodeSignatureAclSubject(Allocator &alloc, const Signature *signature);
    CodeSignatureAclSubject(Allocator &alloc, 
		const Signature *signature, const void *comment, size_t commentLength);
	~CodeSignatureAclSubject();
    
    Allocator &allocator;
    
    void exportBlob(Writer::Counter &pub, Writer::Counter &priv);
    void exportBlob(Writer &pub, Writer &priv);
	
	IFDUMP(void debugDump() const);
    
public:
    class Environment : public virtual AclValidationEnvironment {
    public:
		virtual bool verifyCodeSignature(const Signature *signature, const CssmData *comment) = 0;
    };

public:
    class Maker : public AclSubject::Maker {
    public:
    	Maker(Signer &sgn) 
		: AclSubject::Maker(CSSM_ACL_SUBJECT_TYPE_CODE_SIGNATURE), signer(sgn) { }
    	CodeSignatureAclSubject *make(const TypedList &list) const;
    	CodeSignatureAclSubject *make(Version version, Reader &pub, Reader &priv) const;
		
		Signer &signer;
    };
    
private:
	const Signature *mSignature;			// signature of object
	bool mHaveComment;						// mComment present
	CssmAutoData mComment;					// arbitrary comment blob
};

} // end namespace Security



#endif //_H_ACL_CODESIGNING
