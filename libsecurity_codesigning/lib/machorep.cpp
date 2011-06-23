/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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
// machorep - DiskRep mix-in for handling Mach-O main executables
//
#include "machorep.h"
#include "StaticCode.h"
#include "reqmaker.h"


namespace Security {
namespace CodeSigning {

using namespace UnixPlusPlus;


//
// Object management.
// We open the main executable lazily, so nothing much happens on construction.
// If the context specifies a file offset, we directly pick that Mach-O binary (only).
// if it specifies an architecture, we try to pick that. Otherwise, we deliver the whole
// Universal object (which will usually deliver the "native" architecture later).
//
MachORep::MachORep(const char *path, const Context *ctx)
	: SingleDiskRep(path), mSigningData(NULL)
{
	if (ctx)
		if (ctx->offset)
			mExecutable = new Universal(fd(), ctx->offset);
		else if (ctx->arch) {
			auto_ptr<Universal> full(new Universal(fd()));
			mExecutable = new Universal(fd(), full->archOffset(ctx->arch));
		} else
			mExecutable = new Universal(fd());
	else
		mExecutable = new Universal(fd());
	assert(mExecutable);
	CODESIGN_DISKREP_CREATE_MACHO(this, (char*)path, (void*)ctx);
}

MachORep::~MachORep()
{
	delete mExecutable;
	::free(mSigningData);
}


//
// Sniffer function for "plausible Mach-O binary"
//
bool MachORep::candidate(FileDesc &fd)
{
	switch (Universal::typeOf(fd)) {
	case MH_EXECUTE:
	case MH_DYLIB:
	case MH_DYLINKER:
	case MH_BUNDLE:
	case MH_PRELOAD:
		return true;		// dynamic image; supported
	case MH_OBJECT:
		return false;		// maybe later...
	default:
		return false;		// not Mach-O (or too exotic)
	}
}


//
// The default suggested requirements for Mach-O binaries are as follows:
// Hosting requirement: Rosetta if it's PPC, none otherwise.
// Library requirement: Composed from dynamic load commands.
//
static const uint8_t ppc_host_ireq[] = {	// anchor apple and identifier com.apple.translate
	0xfa, 0xde, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06,
	0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x13, 0x63, 0x6f, 0x6d, 0x2e,
	0x61, 0x70, 0x70, 0x6c, 0x65, 0x2e, 0x74, 0x72, 0x61, 0x6e, 0x73, 0x6c, 0x61, 0x74, 0x65, 0x00,
};

const Requirements *MachORep::defaultRequirements(const Architecture *arch)
{
	assert(arch);		// enforced by signing infrastructure
	Requirements::Maker maker;
	
	// if ppc architecture, add hosting requirement for Rosetta's translate tool
	if (arch->cpuType() == CPU_TYPE_POWERPC)
		maker.add(kSecHostRequirementType, ((const Requirement *)ppc_host_ireq)->clone());
		
	// add library requirements from DYLIB commands (if any)
	if (Requirement *libreq = libraryRequirements(arch))
		maker.add(kSecLibraryRequirementType, libreq);	// takes ownership

	// that's all
	return maker.make();
}

Requirement *MachORep::libraryRequirements(const Architecture *arch)
{
	auto_ptr<MachO> macho(mainExecutableImage()->architecture(*arch));
	Requirement::Maker maker;
	Requirement::Maker::Chain chain(maker, opOr);
	if (macho.get()) {
		for (const load_command *command = macho->loadCommands(); command; command = macho->nextCommand(command)) {
			if (macho->flip(command->cmd) == LC_LOAD_DYLIB) {
				const dylib_command *dycmd = (const dylib_command *)command;
				if (const char *name = macho->string(command, dycmd->dylib.name))
					try {
						secdebug("machorep", "examining DYLIB %s", name);
						// find path on disk, get designated requirement (if signed)
						if (RefPointer<DiskRep> rep = DiskRep::bestFileGuess(name))
							if (SecPointer<SecStaticCode> code = new SecStaticCode(rep))
								if (const Requirement *req = code->designatedRequirement()) {
									secdebug("machorep", "adding library requirement for %s", name);
									chain.add();
									chain.maker.copy(req);
								}
					} catch (...) {
						secdebug("machorep", "exception getting library requirement (ignored)");
					}
				else
					secdebug("machorep", "no string for DYLIB command (ignored)");
			}
		}
	}
	if (chain.empty())
		return NULL;
	else
		return maker.make();
}



//
// Obtain, cache, and return a Universal reference to the main executable,
// IF the main executable is a Mach-O binary (or fat version thereof).
// Returns NULL if the main executable can't be opened as such.
//
Universal *MachORep::mainExecutableImage()
{
	if (!mExecutable)
		mExecutable = new Universal(fd());
	return mExecutable;
}


//
// Default to system page size for segmented (paged) signatures
//
size_t MachORep::pageSize()
{
	return segmentedPageSize;
}


//
// Signing base is the start of the Mach-O architecture we're using
//
size_t MachORep::signingBase()
{
	return mainExecutableImage()->archOffset();
}


//
// We choose the binary identifier for a Mach-O binary as follows:
//	- If the Mach-O headers have a UUID command, use the UUID.
//	- Otherwise, use the SHA-1 hash of the (entire) load commands.
//
CFDataRef MachORep::identification()
{
	std::auto_ptr<MachO> macho(mainExecutableImage()->architecture());
	return identificationFor(macho.get());
}

CFDataRef MachORep::identificationFor(MachO *macho)
{
	// if there is a LC_UUID load command, use the UUID contained therein
	if (const load_command *cmd = macho->findCommand(LC_UUID)) {
		const uuid_command *uuidc = reinterpret_cast<const uuid_command *>(cmd);
		char result[4 + sizeof(uuidc->uuid)];
		memcpy(result, "UUID", 4);
		memcpy(result+4, uuidc->uuid, sizeof(uuidc->uuid));
		return makeCFData(result, sizeof(result));
	}
	
	// otherwise, use the SHA-1 hash of the entire load command area
	SHA1 hash;
	hash(&macho->header(), sizeof(mach_header));
	hash(macho->loadCommands(), macho->commandLength());
	SHA1::Digest digest;
	hash.finish(digest);
	return makeCFData(digest, sizeof(digest));
}


//
// Retrieve a component from the executable.
// This reads the entire signing SuperBlob when first called for an executable,
// and then caches it for further use.
// Note that we could read individual components directly off disk and only cache
// the SuperBlob Index directory. Our caller (usually SecStaticCode) is expected
// to cache the pieces anyway.
//
CFDataRef MachORep::component(CodeDirectory::SpecialSlot slot)
{
	switch (slot) {
	case cdInfoSlot:
		return infoPlist();
	default:
		return embeddedComponent(slot);
	}
}


// Retrieve a component from the embedded signature SuperBlob (if present).
// This reads the entire signing SuperBlob when first called for an executable,
// and then caches it for further use.
// Note that we could read individual components directly off disk and only cache
// the SuperBlob Index directory. Our caller (usually SecStaticCode) is expected
// to cache the pieces anyway. But it's not clear that the resulting multiple I/O
// calls wouldn't be slower in the end.
//
CFDataRef MachORep::embeddedComponent(CodeDirectory::SpecialSlot slot)
{
	if (!mSigningData) {		// fetch and cache
		auto_ptr<MachO> macho(mainExecutableImage()->architecture());
		if (macho.get())
			if (const linkedit_data_command *cs = macho->findCodeSignature()) {
				size_t offset = macho->flip(cs->dataoff);
				size_t length = macho->flip(cs->datasize);
				if (mSigningData = EmbeddedSignatureBlob::readBlob(macho->fd(), macho->offset() + offset, length)) {
					secdebug("machorep", "%zd signing bytes in %d blob(s) from %s(%s)",
						mSigningData->length(), mSigningData->count(),
						mainExecutablePath().c_str(), macho->architecture().name());
				} else {
					secdebug("machorep", "failed to read signing bytes from %s(%s)",
						mainExecutablePath().c_str(), macho->architecture().name());
					MacOSError::throwMe(errSecCSSignatureInvalid);
				}
			}
	}
	if (mSigningData)
		return mSigningData->component(slot);
	
	// not found
	return NULL;
}


//
// Extract an embedded Info.plist from the file.
// Returns NULL if none is found.
//
CFDataRef MachORep::infoPlist()
{
	CFRef<CFDataRef> info;
	try {
		auto_ptr<MachO> macho(mainExecutableImage()->architecture());
		if (const section *sect = macho->findSection("__TEXT", "__info_plist")) {
			if (macho->is64()) {
				const section_64 *sect64 = reinterpret_cast<const section_64 *>(sect);
				info.take(macho->dataAt(macho->flip(sect64->offset), macho->flip(sect64->size)));
			} else {
				info.take(macho->dataAt(macho->flip(sect->offset), macho->flip(sect->size)));
			}
		}
	} catch (...) {
		secdebug("machorep", "exception reading embedded Info.plist");
	}
	return info.yield();
}


//
// Return a recommended unique identifier.
// If our file has an embedded Info.plist, use the CFBundleIdentifier from that.
// Otherwise, use the default.
//
string MachORep::recommendedIdentifier()
{
	if (CFDataRef info = infoPlist()) {
		if (CFDictionaryRef dict = makeCFDictionaryFrom(info)) {
			CFStringRef code = CFStringRef(CFDictionaryGetValue(dict, kCFBundleIdentifierKey));
			if (code && CFGetTypeID(code) != CFStringGetTypeID())
				MacOSError::throwMe(errSecCSBadDictionaryFormat);
			if (code)
				return cfString(code);
		} else
			MacOSError::throwMe(errSecCSBadDictionaryFormat);
	}
	
	// ah well. Use the default
	return SingleDiskRep::recommendedIdentifier();
}


//
// Provide a (vaguely) human readable characterization of this code
//
string MachORep::format()
{
	if (Universal *fat = mainExecutableImage()) {
		Universal::Architectures archs;
		fat->architectures(archs);
		if (fat->isUniversal()) {
			string s = "Mach-O universal (";
			for (Universal::Architectures::const_iterator it = archs.begin();
					it != archs.end(); ++it) {
				if (it != archs.begin())
					s += " ";
				s += it->displayName();
			}
			return s + ")";
		} else {
			assert(archs.size() == 1);
			return string("Mach-O thin (") + archs.begin()->displayName() + ")";
		}
	} else
		return "Mach-O (unrecognized format)";
}


//
// Flush cached data
//
void MachORep::flush()
{
	delete mExecutable;
	mExecutable = NULL;
	::free(mSigningData);
	mSigningData = NULL;
	SingleDiskRep::flush();
}


//
// FileDiskRep::Writers
//
DiskRep::Writer *MachORep::writer()
{
	return new Writer(this);
}


//
// Write a component.
// MachORep::Writers don't write to components directly; the signing code uses special
// knowledge of the Mach-O format to build embedded signatures and blasts them directly
// to disk. Thus this implementation will never be called (and, if called, will simply fail).
//
void MachORep::Writer::component(CodeDirectory::SpecialSlot slot, CFDataRef data)
{
	assert(false);
	MacOSError::throwMe(errSecCSInternalError);
}


} // end namespace CodeSigning
} // end namespace Security
