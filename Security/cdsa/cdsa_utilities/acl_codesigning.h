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
// acl_codesigning - ACL subject for signature of calling application
//
#ifndef _H_ACL_CODESIGNING
#define _H_ACL_CODESIGNING

#include <Security/cssmdata.h>
#include <Security/cssmacl.h>
#include <Security/codesigning.h>

#ifdef _CPP_ACL_CODESIGNING
#pragma export on
#endif

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
    CssmList toList(CssmAllocator &alloc) const;
    
    CodeSignatureAclSubject(CssmAllocator &alloc, const Signature *signature);
    CodeSignatureAclSubject(CssmAllocator &alloc, 
		const Signature *signature, const void *comment, size_t commentLength);
	~CodeSignatureAclSubject();
    
    CssmAllocator &allocator;
    
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


#ifdef _CPP_ACL_CODESIGNING
#pragma export off
#endif


#endif //_H_ACL_CODESIGNING
