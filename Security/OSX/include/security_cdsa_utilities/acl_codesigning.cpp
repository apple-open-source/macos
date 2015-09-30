/*
 * Copyright (c) 2000-2006,2011,2014 Apple Inc. All Rights Reserved.
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
// Code signature credentials are validated globally - they are entirely
// a feature of "the" process (defined by the environment), and take no
// samples whatsoever.
//
bool CodeSignatureAclSubject::validate(const AclValidationContext &context) const
{
	// a suitable environment is required for a match
    if (Environment *env = context.environment<Environment>())
			return env->verifyCodeSignature(*this, context);
	else
		return false;
}


//
// Make a copy of this subject in CSSM_LIST form.
// The format is (head), (type code: Wordid), (signature data: datum), (comment: datum)
//
CssmList CodeSignatureAclSubject::toList(Allocator &alloc) const
{
	assert(path().find('\0') == string::npos);	// no embedded nulls in path
	uint32_t type = CSSM_ACL_CODE_SIGNATURE_OSX;
	TypedList list(alloc, CSSM_ACL_SUBJECT_TYPE_CODE_SIGNATURE,
		new(alloc) ListElement(type),
		new(alloc) ListElement(alloc, CssmData::wrap(legacyHash(), SHA1::digestLength)),
		new(alloc) ListElement(alloc, CssmData::wrap(path().c_str(), path().size() + 1)));
	if (requirement()) {
		CFRef<CFDataRef> reqData;
		MacOSError::check(SecRequirementCopyData(requirement(), kSecCSDefaultFlags, &reqData.aref()));
		list += new(alloc) ListElement(alloc,
			CssmData::wrap(CFDataGetBytePtr(reqData), CFDataGetLength(reqData)));
	}
	for (AuxMap::const_iterator it = beginAux(); it != endAux(); it++)
		list += new(alloc) ListElement(alloc, CssmData(*it->second));
	return list;
}


//
// Create a CodeSignatureAclSubject
//
CodeSignatureAclSubject *CodeSignatureAclSubject::Maker::make(const TypedList &list) const
{
	// there once was a format with only a hash (length 2+1). It is no longer supported
	unsigned total = list.length();		// includes subject type header
	if (total >= 3 + 1
		&& list[1].is(CSSM_LIST_ELEMENT_WORDID)		// [1] == signature type
		&& list[1] == CSSM_ACL_CODE_SIGNATURE_OSX
		&& list[2].is(CSSM_LIST_ELEMENT_DATUM)		// [2] == legacy hash
		&& list[2].data().length() == SHA1::digestLength
		&& list[3].is(CSSM_LIST_ELEMENT_DATUM)) {
		// structurally okay
		CodeSignatureAclSubject *subj =
			new CodeSignatureAclSubject(list[2].data().interpretedAs<const SHA1::Byte>(),
				list[3].data().interpretedAs<const char>());
		for (unsigned n = 3 + 1; n < total; n++) {
			if (list[n].is(CSSM_LIST_ELEMENT_DATUM)) {
				const BlobCore *blob = list[n].data().interpretedAs<const BlobCore>();
				if (blob->length() < sizeof(BlobCore)) {
					secdebug("csblob", "runt blob (0x%x/%zd) slot %d in CSSM_LIST",
						blob->magic(), blob->length(), n);
					CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
				} else if (blob->length() != list[n].data().length()) {
					secdebug("csblob", "badly sized blob (0x%x/%zd) slot %d in CSSM_LIST",
						blob->magic(), blob->length(), n);
					CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
				}
				subj->add(blob);
			} else
				CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
		}
		return subj;
	} else
		CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
}

CodeSignatureAclSubject *CodeSignatureAclSubject::Maker::make(Version version,
	Reader &pub, Reader &priv) const
{
	assert(version == 0);
	Endian<uint32> sigType; pub(sigType);
	const void *data; size_t length; pub.countedData(data, length);
	const void *commentData; size_t commentLength; pub.countedData(commentData, commentLength);
	if (sigType == CSSM_ACL_CODE_SIGNATURE_OSX
		&& length == SHA1::digestLength) {
		return make((const SHA1::Byte *)data, CssmData::wrap(commentData, commentLength));
	}
	CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
}

CodeSignatureAclSubject *CodeSignatureAclSubject::Maker::make(const SHA1::Byte *hash,
	const CssmData &commentBag) const
{
	using namespace LowLevelMemoryUtilities;
	const char *path = commentBag.interpretedAs<const char>();
	CodeSignatureAclSubject *subj = new CodeSignatureAclSubject(hash, path);
	for (const BlobCore *blob = increment<BlobCore>(commentBag.data(), alignUp(strlen(path) + 1, commentBagAlignment));
			blob < commentBag.end();
			blob = increment<const BlobCore>(blob, alignUp(blob->length(), commentBagAlignment))) {
		size_t leftInBag = difference(commentBag.end(), blob);
		if (leftInBag < sizeof(BlobCore) || blob->length() < sizeof(BlobCore) || blob->length() > leftInBag) {
			secdebug("csblob", "invalid blob (0x%x/%zd) [%zd in bag] in code signing ACL for %s - stopping scan",
				blob->magic(), blob->length(), leftInBag, subj->path().c_str());
			break;	// can't trust anything beyond this blob
		}
		subj->add(blob);
	}
	return subj;
}


//
// Export the subject to a memory blob
//
void CodeSignatureAclSubject::exportBlob(Writer::Counter &pub, Writer::Counter &priv)
{
	using LowLevelMemoryUtilities::alignUp;
	assert(path().find('\0') == string::npos);	// no embedded nulls in path
	Endian<uint32> sigType = CSSM_ACL_CODE_SIGNATURE_OSX; pub(sigType);
	pub.countedData(legacyHash(), SHA1::digestLength);
	size_t size = path().size() + 1;
	if (requirement()) {
		CFRef<CFDataRef> reqData;
		MacOSError::check(SecRequirementCopyData(requirement(), kSecCSDefaultFlags, &reqData.aref()));
		size = alignUp(size, commentBagAlignment) + CFDataGetLength(reqData);
	}
	for (AuxMap::const_iterator it = beginAux(); it != endAux(); it++) {
		size = alignUp(size, commentBagAlignment) + it->second->length();
	}
	pub.countedData(NULL, size);
}

void CodeSignatureAclSubject::exportBlob(Writer &pub, Writer &priv)
{
	using LowLevelMemoryUtilities::alignUp;
	Endian<uint32> sigType = CSSM_ACL_CODE_SIGNATURE_OSX; pub(sigType);
	pub.countedData(legacyHash(), SHA1::digestLength);
	CssmAutoData commentBag(Allocator::standard(), path().c_str(), path().size() + 1);
	static const uint32_t zero = 0;
	if (requirement()) {
		CFRef<CFDataRef> reqData;
		MacOSError::check(SecRequirementCopyData(requirement(), kSecCSDefaultFlags, &reqData.aref()));
		commentBag.append(&zero,
			alignUp(commentBag.length(), commentBagAlignment) - commentBag.length());
		commentBag.append(CFDataGetBytePtr(reqData), CFDataGetLength(reqData));
	}
	for (AuxMap::const_iterator it = beginAux(); it != endAux(); it++) {
		commentBag.append(&zero,
			alignUp(commentBag.length(), commentBagAlignment) - commentBag.length());
		commentBag.append(CssmData(*it->second));
	}
	pub.countedData(commentBag);
}


#ifdef DEBUGDUMP

void CodeSignatureAclSubject::debugDump() const
{
	Debug::dump("CodeSigning ");
	OSXVerifier::dump();
}

#endif //DEBUGDUMP
