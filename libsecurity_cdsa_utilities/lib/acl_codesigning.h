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
// acl_codesigning - ACL subject for signature of calling application
//
// Note:
// Once upon a time, a code signature was a single binary blob, a "signature".
// Then we added an optional second blob, a "comment". The comment was only
// ancilliary (non-security) data first, but then we added more security data
// to it later. Now, the security-relevant data is kept in a (signature, comment)
// pair, all of which is relevant for the security of such subjects.
// Don't read any particular semantics into this separation. It is historical only
// (having to do with backward binary compatibility of ACL blobs).
//
#ifndef _H_ACL_CODESIGNING
#define _H_ACL_CODESIGNING

#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_utilities/cssmacl.h>
#include <security_cdsa_utilities/osxverifier.h>

namespace Security {


//
// The CodeSignature subject type matches a code signature applied to the
// disk image that originated the client process.
//
class CodeSignatureAclSubject : public AclSubject, public OSXVerifier {
public:
	class Maker; friend class Maker;
	
	static const size_t commentBagAlignment = 4;
	
    CodeSignatureAclSubject(const SHA1::Byte *hash, const std::string &path)
		: AclSubject(CSSM_ACL_SUBJECT_TYPE_CODE_SIGNATURE), OSXVerifier(hash, path) { }

	CodeSignatureAclSubject(const OSXVerifier &verifier)
		: AclSubject(CSSM_ACL_SUBJECT_TYPE_CODE_SIGNATURE), OSXVerifier(verifier) { }
	
    bool validate(const AclValidationContext &baseCtx) const;
    CssmList toList(Allocator &alloc) const;
    
    void exportBlob(Writer::Counter &pub, Writer::Counter &priv);
    void exportBlob(Writer &pub, Writer &priv);
	
	IFDUMP(void debugDump() const);
    
public:
    class Environment : public virtual AclValidationEnvironment {
    public:
		virtual bool verifyCodeSignature(const OSXVerifier &verifier,
			const AclValidationContext &context) = 0;
    };

public:
    class Maker : public AclSubject::Maker {
    public:
    	Maker()
		: AclSubject::Maker(CSSM_ACL_SUBJECT_TYPE_CODE_SIGNATURE) { }
    	CodeSignatureAclSubject *make(const TypedList &list) const;
    	CodeSignatureAclSubject *make(Version version, Reader &pub, Reader &priv) const;
	
	private:
		CodeSignatureAclSubject *make(const SHA1::Byte *hash, const CssmData &commentBag) const;
    };
};

} // end namespace Security



#endif //_H_ACL_CODESIGNING
