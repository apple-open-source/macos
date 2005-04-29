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
#include <security_cdsa_utilities/acl_codesigning.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <security_utilities/endian.h>
#include <algorithm>


//
// Construct a password ACL subject.
// Note that this takes over ownership of the signature object.
//
CodeSignatureAclSubject::CodeSignatureAclSubject(Allocator &alloc, 
	const Signature *signature, const void *comment, size_t commentLength)
	: AclSubject(CSSM_ACL_SUBJECT_TYPE_CODE_SIGNATURE),
    allocator(alloc), mSignature(signature),
	mHaveComment(true), mComment(alloc, comment, commentLength)
{ }

CodeSignatureAclSubject::CodeSignatureAclSubject(Allocator &alloc, 
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
			return env->verifyCodeSignature(mSignature,
				mHaveComment ? &mComment.get() : NULL);
	else
		return false;
}


//
// Make a copy of this subject in CSSM_LIST form.
// The format is (head), (type code: Wordid), (signature data: datum), (comment: datum)
//
CssmList CodeSignatureAclSubject::toList(Allocator &alloc) const
{
    // all associated data is public (no secrets)
	TypedList list(alloc, CSSM_ACL_SUBJECT_TYPE_CODE_SIGNATURE,
		new(alloc) ListElement(mSignature->type()),
		new(alloc) ListElement(alloc, CssmData(*mSignature)));
	if (mHaveComment)
		list += new(alloc) ListElement(alloc, mComment);
	return list;
}


//
// Create a CodeSignatureAclSubject
//
CodeSignatureAclSubject *CodeSignatureAclSubject::Maker::make(const TypedList &list) const
{
    Allocator &alloc = Allocator::standard();
	if (list.length() == 3+1) {
		// signature type: int, signature data: datum, comment: datum
		ListElement *elem[3];
		crack(list, 3, elem, 
			CSSM_LIST_ELEMENT_WORDID, CSSM_LIST_ELEMENT_DATUM, CSSM_LIST_ELEMENT_DATUM);
		u_int32_t sigType = *elem[0];
		CssmData &sigData(*elem[1]);
		CssmData &commentData(*elem[2]);
		return new CodeSignatureAclSubject(alloc,
			signer.restore(sigType, sigData), commentData.data(), commentData.length());
	} else {
		// signature type: int, signature data: datum [no comment]
		ListElement *elem[2];
		crack(list, 2, elem, 
			CSSM_LIST_ELEMENT_WORDID, CSSM_LIST_ELEMENT_DATUM);
		u_int32_t sigType = *elem[0];
		CssmData &sigData(*elem[1]);
		return new CodeSignatureAclSubject(alloc, signer.restore(sigType, sigData));
	}
}

CodeSignatureAclSubject *CodeSignatureAclSubject::Maker::make(Version version,
	Reader &pub, Reader &priv) const
{
	assert(version == 0);
    Allocator &alloc = Allocator::standard();
	Endian<uint32> sigType; pub(sigType);
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
	Endian<uint32> sigType = mSignature->type(); pub(sigType);
	pub.countedData(*mSignature);
	pub.countedData(mComment);
}

void CodeSignatureAclSubject::exportBlob(Writer &pub, Writer &priv)
{
	Endian<uint32> sigType = mSignature->type(); pub(sigType);
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
