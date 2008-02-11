/*
 * Copyright (c) 2006-2007 Apple Inc. All Rights Reserved.
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
#include "signerutils.h"
#include "signer.h"
#include "SecCodeSigner.h"
#include <Security/SecIdentity.h>
#include <Security/CMSEncoder.h>
#include "renum.h"
#include <security_utilities/unix++.h>
#include <security_utilities/unixchild.h>
#include <vector>

// for helper validation
#include "Code.h"
#include "cfmunge.h"
#include <sys/codesign.h>


namespace Security {
namespace CodeSigning {


//
// About the Mach-O allocation helper
//
static const char helperName[] = "codesign_allocate";
static const char helperPath[] = "/usr/bin/codesign_allocate";
static const char helperOverride[] = "CODESIGN_ALLOCATE";
static const size_t csAlign = 16;


//
// InternalRequirements
//
void InternalRequirements::operator () (const Requirements *given, const Requirements *defaulted)
{
	if (defaulted) {
		this->add(defaulted);
		::free((void *)defaulted);		// was malloc(3)ed by DiskRep
	}
	if (given)
		this->add(given);
	mReqs = make();
}


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
	signer.state.returnDetachedSignature(blob);
	::free(blob);
}


//
// ArchEditor
//
ArchEditor::ArchEditor(Universal &code, uint32_t attrs /* = 0 */)
	: DiskRep::Writer(attrs)
{
	Universal::Architectures archList;
	code.architectures(archList);
	for (Universal::Architectures::const_iterator it = archList.begin();
			it != archList.end(); ++it)
		architecture[*it] = new Arch(*it);
}


ArchEditor::~ArchEditor()
{
	for (ArchMap::iterator it = begin(); it != end(); ++it)
		delete it->second;
}


//
// BlobEditor
//
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
	signer.state.returnDetachedSignature(blob);
	::free(blob);
}


//
// MachOEditor's allocate() method spawns the codesign_allocate helper tool to
// "drill up" the Mach-O binary for insertion of Code Signing signature data.
// After the tool succeeds, we open the new file and are ready to write it.
//
MachOEditor::MachOEditor(DiskRep::Writer *w, Universal &code, std::string srcPath)
	: ArchEditor(code, w->attributes()), writer(w), sourcePath(srcPath), tempPath(srcPath + ".cstemp"),
	  mNewCode(NULL), mTempMayExist(false)
{
	if (const char *path = getenv(helperOverride)) {
		mHelperPath = path;
		mHelperOverridden = true;
	} else {
		mHelperPath = helperPath;
		mHelperOverridden = false;
	}
}

MachOEditor::~MachOEditor()
{
	delete mNewCode;
	if (mTempMayExist)
		::remove(tempPath.c_str());		// ignore error (can't do anything about it)

	//@@@ this code should be in UnixChild::kill() -- migrate it there
	if (state() == alive) {
		this->kill(SIGTERM);		// shoot it once
		checkChildren();			// check for quick death
		if (state() == alive) {
			usleep(500000);			// give it some grace
			if (state() == alive) {	// could have been reaped by another thread
				checkChildren();	// check again
				if (state() == alive) {	// it... just... won't... die...
					this->kill(SIGKILL); // take THAT!
					checkChildren();
					if (state() == alive) // stuck zombie
						abandon();	// leave the body behind
				}
			}
		}
	}
}


void MachOEditor::component(CodeDirectory::SpecialSlot slot, CFDataRef data)
{
	writer->component(slot, data);
}


void MachOEditor::allocate()
{
	// note that we may have a temporary file from now on (for cleanup in the error case)
	mTempMayExist = true;

	// run codesign_allocate to make room in the executable file
	fork();
	wait();
	if (!Child::succeeded())
		UnixError::throwMe(ENOEXEC);	//@@@ how to signal "it din' work"?
	
	// open the new (temporary) Universal file
	{
		UidGuard guard(0);
		mFd.open(tempPath, O_RDWR);
	}
	mNewCode = new Universal(mFd);
}

static const unsigned char appleReq[] = {	// anchor apple
	0xfa, 0xde, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03,
};

void MachOEditor::parentAction()
{
	if (mHelperOverridden) {
		secdebug("machoedit", "validating alternate codesign_allocate at %s (pid=%d)", mHelperPath, this->pid());
		// check code identity of an overridden allocation helper
		SecPointer<SecStaticCode> code = new SecStaticCode(DiskRep::bestGuess(mHelperPath));
		code->validateDirectory();
		code->validateExecutable();
		code->validateResources();
		code->validateRequirements((const Requirement *)appleReq, errSecCSReqFailed);
	}
}

void MachOEditor::childAction()
{
	vector<const char *> arguments;
	arguments.push_back(helperName);
	arguments.push_back("-i");
	arguments.push_back(sourcePath.c_str());
	arguments.push_back("-o");
	arguments.push_back(tempPath.c_str());
	
	for (Iterator it = architecture.begin(); it != architecture.end(); ++it) {
		char *size;				// we'll leak this (execv is coming soon)
		asprintf(&size, "%d", LowLevelMemoryUtilities::alignUp(it->second->blobSize, csAlign));
		secdebug("machoedit", "preparing %s size=%s", it->first.name(), size);

		if (const char *arch = it->first.name()) {
			arguments.push_back("-a");
			arguments.push_back(arch);
		} else {
			arguments.push_back("-A");
			char *anum;
			asprintf(&anum, "%d", it->first.cpuType());
			arguments.push_back(anum);
			asprintf(&anum, "%d", it->first.cpuSubtype());
			arguments.push_back(anum);
		}
		arguments.push_back(size);
	}
	arguments.push_back(NULL);
	
	if (mHelperOverridden)
		::csops(0, CS_EXEC_SET_KILL, NULL, 0);		// force code integrity
	::seteuid(0);	// activate privilege if caller has it; ignore error if not
	execv(mHelperPath, (char * const *)&arguments[0]);
}

void MachOEditor::reset(Arch &arch)
{
	arch.source.reset(mNewCode->architecture(arch.architecture));
	arch.cdbuilder.reopen(tempPath,
		arch.source->offset(), arch.source->signingOffset());
}


//
// MachOEditor's write() method actually writes the blob into the CODESIGNING section
// of the executable image file.
//
void MachOEditor::write(Arch &arch, EmbeddedSignatureBlob *blob)
{
	if (size_t offset = arch.source->signingOffset()) {
		size_t signingLength = arch.source->signingLength();
		secdebug("codesign", "writing architecture %s at 0x%zx (%zd of %zd)",
			arch.architecture.name(), offset, blob->length(), signingLength);
		if (signingLength < blob->length()) {
			secdebug("codesign", "trying to write %zd bytes into %zd area",
				blob->length(), signingLength);
			MacOSError::throwMe(errSecCSInternalError);
		}
		arch.source->seek(offset);
		arch.source->writeAll(*blob);
		::free(blob);		// done with it
	} else {
		secdebug("signer", "%p cannot find CODESIGNING section", this);
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
			guard.seteuid(st.st_uid);
		copy(sourcePath.c_str(), NULL, COPYFILE_SECURITY | COPYFILE_METADATA);

		// move the new file into place
		UnixError::check(::rename(tempPath.c_str(), sourcePath.c_str()));
		mTempMayExist = false;		// we renamed it away
	}
}


//
// Copyfile
//
Copyfile::Copyfile()
{
	if (!(mState = copyfile_state_alloc()))
		UnixError::throwMe();
}
	
void Copyfile::set(uint32_t flag, const void *value)
{
	check(::copyfile_state_set(mState, flag, value));
}

void Copyfile::get(uint32_t flag, void *value)
{
	check(::copyfile_state_set(mState, flag, value));
}
	
void Copyfile::operator () (const char *src, const char *dst, copyfile_flags_t flags)
{
	check(::copyfile(src, dst, mState, flags));
}

void Copyfile::check(int rc)
{
	if (rc < 0)
		UnixError::throwMe();
}


} // end namespace CodeSigning
} // end namespace Security
