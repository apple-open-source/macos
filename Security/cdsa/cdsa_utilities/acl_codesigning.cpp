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
#ifdef __MWERKS__
#define _CPP_ACL_CODESIGNING
#endif

#include <Security/acl_codesigning.h>
#include <Security/cssmdata.h>
#include <algorithm>


//
// Construct a password ACL subject.
// Note that this takes over ownership of the signature object.
//
CodeSignatureAclSubject::CodeSignatureAclSubject(CssmAllocator &alloc, 
	const Signature *signature, const void *comment, size_t commentLength)
	: AclSubject(CSSM_ACL_SUBJECT_TYPE_CODE_SIGNATURE),
    allocator(alloc), mSignature(signature),
	mHaveComment(true), mComment(alloc, comment, commentLength)
{ }

CodeSignatureAclSubject::CodeSignatureAclSubject(CssmAllocator &alloc, 
	const Signature *signature)
	: AclSubject(CSSM_ACL_SUBJECT_TYPE_CODE_SIGNATURE),
    allocator(alloc), mSignature(signature), mHaveComment(false), mComment(alloc)
{ }

CodeSignatureAclSubject::~CodeSignatureAclSubject()
{
	delete mSignature;
}

//
// Code signature credentials are validated globally - they are entirely
// a feature of "the" process (defined by the environment), and take no
// samples whatsoever.
//
bool CodeSignatureAclSubject::validate(const AclValidationContext &context) const
{
	// a suitable environment is required for a match
    if (Environment *env = context.environment<Environment>())
		return env->verifyCodeSignature(mSignature);
	else
		return false;
}


//
// Make a copy of this subject in CSSM_LIST form.
// The format is (head), (type code: Wordid), (signature data: datum), (comment: datum)
//
CssmList CodeSignatureAclSubject::toList(CssmAllocator &alloc) const
{
    // all associated data is public (no secrets)
	TypedList list(alloc, CSSM_ACL_SUBJECT_TYPE_CODE_SIGNATURE,
		new(alloc) ListElement(mSignature->type()),
		new(alloc) ListElement(alloc.alloc(*mSignature)));
	if (mHaveComment)
		list += new(alloc) ListElement(alloc.alloc(mComment));
	return list;
}


//
// Create a CodeSignatureAclSubject
//
CodeSignatureAclSubject *CodeSignatureAclSubject::Maker::make(const TypedList &list) const
{
    CssmAllocator &alloc = CssmAllocator::standard();
	if (list.length() == 3+1) {
		// signature type: int, signature data: datum, comment: datum
		ListElement *elem[3];
		crack(list, 3, elem, 
			CSSM_LIST_ELEMENT_WORDID, CSSM_LIST_ELEMENT_DATUM, CSSM_LIST_ELEMENT_DATUM);
		CssmData &commentData(*elem[2]);
		return new CodeSignatureAclSubject(alloc, signer.restore(*elem[0], *elem[1]), 
			commentData.data(), commentData.length());
	} else {
		// signature type: int, signature data: datum [no comment]
		ListElement *elem[2];
		crack(list, 2, elem, 
			CSSM_LIST_ELEMENT_WORDID, CSSM_LIST_ELEMENT_DATUM);
		return new CodeSignatureAclSubject(alloc, signer.restore(*elem[0], *elem[1]));
	}
}

CodeSignatureAclSubject *CodeSignatureAclSubject::Maker::make(Reader &pub, Reader &priv) const
{
    CssmAllocator &alloc = CssmAllocator::standard();
	uint32 sigType; pub(sigType);
	const void *data; uint32 length; pub.countedData(data, length);
	const void *commentData; uint32 commentLength; pub.countedData(commentData, commentLength);
	return new CodeSignatureAclSubject(alloc, 
		signer.restore(sigType, data, length),
		commentData, commentLength);
}


//
// Export the subject to a memory blob
//
void CodeSignatureAclSubject::exportBlob(Writer::Counter &pub, Writer::Counter &priv)
{
	uint32 sigType = mSignature->type(); pub(sigType);
	pub.countedData(*mSignature);
	pub.countedData(mComment);
}

void CodeSignatureAclSubject::exportBlob(Writer &pub, Writer &priv)
{
	uint32 sigType = mSignature->type(); pub(sigType);
	pub.countedData(*mSignature);
	pub.countedData(mComment);
}


#ifdef DEBUGDUMP

void CodeSignatureAclSubject::debugDump() const
{
	Debug::dump("CodeSigning");
	if (mHaveComment) {
		Debug::dump(" comment=");
		Debug::dumpData(mComment);
	}
}

#endif //DEBUGDUMP
