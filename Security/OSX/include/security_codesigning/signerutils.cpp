/*
 * Copyright (c) 2006-2013 Apple Inc. All Rights Reserved.
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
// signerutils - utilities for signature generation
//
#include "csutilities.h"
#include "drmaker.h"
#include "resources.h"
#include "signerutils.h"
#include "signer.h"

#include <Security/SecCmsBase.h>
#include <Security/SecIdentity.h>
#include <Security/CMSEncoder.h>

#include "SecCodeSigner.h"

#include <security_utilities/unix++.h>
#include <security_utilities/logging.h>

#include <vector>

#include "codesign_alloc.h"

namespace Security {
namespace CodeSigning {


//
// About the Mach-O allocation helper
//
static const size_t csAlign = 16;


//
// BlobWriters
//
void BlobWriter::component(CodeDirectory::SpecialSlot slot, CFDataRef data)
{
	return EmbeddedSignatureBlob::Maker::component(slot, data);
}


void DetachedBlobWriter::flush()
{
	EmbeddedSignatureBlob *blob = this->make();
	signer.code->detachedSignature(CFTempData(*blob));
	signer.state.returnDetachedSignature(blob, signer);
	::free(blob);
}


//
// ArchEditor
//
ArchEditor::ArchEditor(Universal &code, CodeDirectory::HashAlgorithms hashTypes, uint32_t attrs)
	: DiskRep::Writer(attrs)
{
	Universal::Architectures archList;
	code.architectures(archList);
	for (Universal::Architectures::const_iterator it = archList.begin();
			it != archList.end(); ++it)
		architecture[*it] = new Arch(*it, hashTypes);
}


ArchEditor::~ArchEditor()
{
	for (ArchMap::iterator it = begin(); it != end(); ++it)
		delete it->second;
}
	
	
ArchEditor::Arch::Arch(const Architecture &arch, CodeDirectory::HashAlgorithms hashTypes)
	: architecture(arch)
{
	blobSize = 0;
	for (auto type = hashTypes.begin(); type != hashTypes.end(); ++type)
		cdBuilders.insert(make_pair(*type, new CodeDirectory::Builder(*type)));
}


//
// BlobEditor
//
BlobEditor::BlobEditor(Universal &fat, SecCodeSigner::Signer &s)
	: ArchEditor(fat, s.digestAlgorithms(), 0), signer(s)
{ }


void BlobEditor::component(CodeDirectory::SpecialSlot slot, CFDataRef data)
{
	mGlobal.component(slot, data);
}

void BlobEditor::write(Arch &arch, EmbeddedSignatureBlob *blob)
{
	mMaker.add(arch.architecture.cpuType(), blob);
}


void BlobEditor::commit()
{
	// create the architecture-global blob and store it into the superblob
	mMaker.add(0, mGlobal.make());	// takes ownership of blob

	// finish up the superblob and deliver it
	DetachedSignatureBlob *blob = mMaker.make();
	signer.state.returnDetachedSignature(blob, signer);
	::free(blob);
}


//
// MachOEditor's allocate() method spawns the codesign_allocate helper tool to
// "drill up" the Mach-O binary for insertion of Code Signing signature data.
// After the tool succeeds, we open the new file and are ready to write it.
//
MachOEditor::MachOEditor(DiskRep::Writer *w, Universal &code, CodeDirectory::HashAlgorithms hashTypes, std::string srcPath)
	: ArchEditor(code, hashTypes, w->attributes()),
	  writer(w),
	  sourcePath(srcPath),
	  tempPath(srcPath + ".cstemp"),
	  mHashTypes(hashTypes),
	  mNewCode(NULL),
	  mTempMayExist(false)
{}

MachOEditor::~MachOEditor()
{
	delete mNewCode;
	if (mTempMayExist) {
		::remove(tempPath.c_str());		// ignore error (can't do anything about it)
	}
}


void MachOEditor::component(CodeDirectory::SpecialSlot slot, CFDataRef data)
{
	writer->component(slot, data);
}


void MachOEditor::allocate()
{
	char* errorMessage = NULL;
	// note that we may have a temporary file from now on (for cleanup in the error case)
	mTempMayExist = true;

	bool removeSignature = false;
	for (auto arch : architecture) {
		if(arch.second->blobSize == 0) {
			removeSignature = true;
		} else if (removeSignature) {
			secerror("codesign allocate error: one architecture signaled removal while another signaled signing");
			MacOSError::throwMe(errSecCSInternalError);
		}
	}

	if (removeSignature) {
		if (!code_sign_deallocate(sourcePath.c_str(), tempPath.c_str(), errorMessage)) {
			secerror("codesign deallocation failed: %s", errorMessage);
			::free(errorMessage);
			MacOSError::throwMe(errSecCSInternalError);
		}
	} else {
		if (!code_sign_allocate(sourcePath.c_str(),
								tempPath.c_str(),
								^unsigned int(cpu_type_t cputype, cpu_subtype_t cpusubtype) {
									Architecture currentArch(cputype,cpusubtype);
									size_t blobSize = UINT32_MAX;
									for (auto arch : architecture) {
										if (arch.first.matches(currentArch)) {
											blobSize = arch.second->blobSize;
										}
									}
									blobSize = LowLevelMemoryUtilities::alignUp(blobSize, csAlign);
									return (blobSize < UINT32_MAX) ? (unsigned int) blobSize : UINT32_MAX;
								}, errorMessage)) {
			secerror("codesign allocation failed: %s", errorMessage);
			::free(errorMessage);
			MacOSError::throwMe(errSecCSInternalError);
		}
	}

	// open the new (temporary) Universal file
	{
		UidGuard guard(0);
		mFd.open(tempPath, O_RDWR);
	}
	mNewCode = new Universal(mFd);
}
void MachOEditor::reset(Arch &arch)
{
	arch.source.reset(mNewCode->architecture(arch.architecture));

	for (auto type = mHashTypes.begin(); type != mHashTypes.end(); ++type) {
		arch.eachDigest(^(CodeDirectory::Builder& builder) {
			/* Signature editing may have no need for cd builders, and not
			 * have opened them, so only reopen them conditionally. */
			if (builder.opened()) {
				builder.reopen(tempPath, arch.source->offset(), arch.source->signingOffset());
			}
		});
	}
}


//
// MachOEditor's write() method actually writes the blob into the CODESIGNING section
// of the executable image file.
//
void MachOEditor::write(Arch &arch, EmbeddedSignatureBlob *blob)
{
	if (size_t offset = arch.source->signingOffset()) {
		size_t signingLength = arch.source->signingLength();
		CODESIGN_ALLOCATE_WRITE((char*)arch.architecture.name(), offset, (unsigned)blob->length(), (unsigned)signingLength);
		if (signingLength < blob->length()) {
			MacOSError::throwMe(errSecCSCMSTooLarge);
		}
		arch.source->seek(offset);
		arch.source->writeAll(*blob);
		::free(blob);		// done with it
	} else {
		secinfo("signer", "%p cannot find CODESIGNING data in Mach-O", this);
		MacOSError::throwMe(errSecCSInternalError);
	}
}


//
// Commit the edit.
// This moves the temporary editor copy over the source image file.
// Note that the Universal object returned by allocate() is still open
// and valid; the caller owns it.
//
void MachOEditor::commit()
{
	// if the file's owned by someone else *and* we can become root...
	struct stat st;
	UnixError::check(::stat(sourcePath.c_str(), &st));

	// copy over all the *other* stuff
	Copyfile copy;
	int fd = mFd;
	copy.set(COPYFILE_STATE_DST_FD, &fd);
	{
		// perform copy under root or file-owner privileges if available
		UidGuard guard;
		if (!guard.seteuid(0))
			(void)guard.seteuid(st.st_uid);
		
		// copy metadata from original file...
		copy(sourcePath.c_str(), NULL, COPYFILE_SECURITY | COPYFILE_METADATA);

#if TARGET_OS_OSX
		// determine AFSC status if we are told to preserve compression
		bool conductCompression = false;
		cmpInfo cInfo;
		if (writer->getPreserveAFSC()) {
			if (queryCompressionInfo(sourcePath.c_str(), &cInfo) == 0) {
				if (cInfo.compressionType != 0 && cInfo.compressedSize > 0)
					conductCompression = true;
			}
		}
#endif

		// ... but explicitly update the timestamps since we did change the file
		char buf;
		mFd.read(&buf, sizeof(buf), 0);
		mFd.write(&buf, sizeof(buf), 0);

		// move the new file into place
		UnixError::check(::rename(tempPath.c_str(), sourcePath.c_str()));
		mTempMayExist = false;		// we renamed it away

#if TARGET_OS_OSX
		// if the original file was compressed, compress the new file after move
		if (conductCompression) {
			CFMutableDictionaryRef options = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			CFStringRef val = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), cInfo.compressionType);
			CFDictionarySetValue(options, kAFSCCompressionTypes, val);
			CFRelease(val);

			CompressionQueueContext compressionQueue = CreateCompressionQueue(NULL, NULL, NULL, NULL, options);

			if (!CompressFile(compressionQueue, sourcePath.c_str(), NULL)) {
				secinfo("signer", "%p Failed to queue compression of file %s", this, sourcePath.c_str());
				MacOSError::throwMe(errSecCSInternalError);
			}
			FinishCompressionAndCleanUp(compressionQueue);

			compressionQueue = NULL;
			CFRelease(options);
		}
#endif

	}
	this->writer->flush();
}


//
// InternalRequirements
//
void InternalRequirements::operator () (const Requirements *given, const Requirements *defaulted, const Requirement::Context &context)
{
	bool requirements_supported = true;

#if !TARGET_OS_OSX
	requirements_supported = false;
#endif

	if (!requirements_supported) {
		secinfo("signer", "Platform does not support signing internal requirements");
		mReqs = NULL;
		return;
	}

	// first add the default internal requirements
	if (defaulted) {
		this->add(defaulted);
		::free((void *)defaulted);		// was malloc(3)ed by DiskRep
	}
	
	// now override them with any requirements explicitly given by the signer
	if (given)
		this->add(given);

	// now add the Designated Requirement, if we can make it and it's not been provided
	if (!this->contains(kSecDesignatedRequirementType)) {
		DRMaker maker(context);
		if (Requirement *dr = maker.make()) {
			this->add(kSecDesignatedRequirementType, dr);		// takes ownership of dr
		}
	}
	
	// return the result
	mReqs = this->make();
}


//
// Pre-Signing contexts
//
PreSigningContext::PreSigningContext(const SecCodeSigner::Signer &signer)
{
	// construct a cert chain
	if (signer.signingIdentity() != SecIdentityRef(kCFNull)) {
		CFRef<SecCertificateRef> signingCert;
		MacOSError::check(SecIdentityCopyCertificate(signer.signingIdentity(), &signingCert.aref()));
		CFRef<SecPolicyRef> policy = SecPolicyCreateWithProperties(kSecPolicyAppleCodeSigning, NULL);
		CFRef<SecTrustRef> trust;
		MacOSError::check(SecTrustCreateWithCertificates(CFArrayRef(signingCert.get()), policy, &trust.aref()));

		if (!SecTrustEvaluateWithError(trust, NULL)) {
			secinfo("signer", "SecTrust evaluation of signing certificate failed - not fatal");
		}

		CFRef<CFArrayRef> certChain = SecTrustCopyCertificateChain(trust);
		if (!certChain) {
			secerror("Certificate chain is NULL");
			MacOSError::throwMe(errSecCSInvalidObjectRef);
		}

		mCerts.take(CFArrayCreateCopy(kCFAllocatorDefault, certChain));
		if (!mCerts) {
			secerror("Unable to copy certChain array");
			MacOSError::throwMe(errSecCSInternalError);
		}
		this->certs = mCerts;
	}
	
	// other stuff
	this->identifier = signer.signingIdentifier();
}
	
	
//
// A collector of CodeDirectories for hash-agile construction of signatures.
//
CodeDirectorySet::~CodeDirectorySet()
{
	for (auto it = begin(); it != end(); ++it)
		::free(const_cast<CodeDirectory*>(it->second));
}
	
	
void CodeDirectorySet::add(const Security::CodeSigning::CodeDirectory *cd)
{
	insert(make_pair(cd->hashType, cd));
	if (cd->hashType == kSecCodeSignatureHashSHA1)
		mPrimary = cd;
}
	
	
void CodeDirectorySet::populate(DiskRep::Writer *writer) const
{
	assert(!empty());
	
	if (mPrimary == NULL)	// didn't add SHA-1; pick another occupant for this slot
		mPrimary = begin()->second;
	
	// reserve slot zero for a SHA-1 digest if present; else pick something else
	CodeDirectory::SpecialSlot nextAlternate = cdAlternateCodeDirectorySlots;
	for (auto it = begin(); it != end(); ++it) {
		if (it->second == mPrimary) {
			writer->codeDirectory(it->second, cdCodeDirectorySlot);
		} else {
			writer->codeDirectory(it->second, nextAlternate++);
		}
	}
}
	

const CodeDirectory* CodeDirectorySet::primary() const
{
	if (mPrimary == NULL)
		mPrimary = begin()->second;
	return mPrimary;
}

CFArrayRef CodeDirectorySet::hashList() const
{
	CFRef<CFMutableArrayRef> hashList = makeCFMutableArray(0);
	for (auto it = begin(); it != end(); ++it) {
		CFRef<CFDataRef> cdhash = it->second->cdhash(true);
		CFArrayAppendValue(hashList, cdhash);
	}
	return hashList.yield();
}

CFDictionaryRef CodeDirectorySet::hashDict() const
{
	CFRef<CFMutableDictionaryRef> hashDict = makeCFMutableDictionary();

	for (auto it = begin(); it != end(); ++it) {
		SECOidTag tag = CodeDirectorySet::SECOidTagForAlgorithm(it->first);

		if (tag == SEC_OID_UNKNOWN) {
			MacOSError::throwMe(errSecCSUnsupportedDigestAlgorithm);
		}

		CFRef<CFNumberRef> hashType = makeCFNumber(int(tag));
		CFRef<CFDataRef> fullCdhash = it->second->cdhash(false); // Full-length cdhash!
		CFDictionarySetValue(hashDict, hashType, fullCdhash);
	}

	return hashDict.yield();
}

SECOidTag CodeDirectorySet::SECOidTagForAlgorithm(CodeDirectory::HashAlgorithm algorithm) {
	SECOidTag tag;

	switch (algorithm) {
		case kSecCodeSignatureHashSHA1:
			tag = SEC_OID_SHA1;
			break;
		case kSecCodeSignatureHashSHA256:
		case kSecCodeSignatureHashSHA256Truncated: // truncated *page* hashes, not cdhash
			tag = SEC_OID_SHA256;
			break;
		case kSecCodeSignatureHashSHA384:
			tag = SEC_OID_SHA384;
			break;
		default:
			tag = SEC_OID_UNKNOWN;
	}

	return tag;
}



} // end namespace CodeSigning
} // end namespace Security
