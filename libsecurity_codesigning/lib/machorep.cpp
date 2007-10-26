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


namespace Security {
namespace CodeSigning {

using namespace UnixPlusPlus;


//
// Object management.
// We open the main executable lazily, so nothing much happens on construction.
//
MachORep::MachORep(const char *path)
	: SingleDiskRep(path), mSigningData(NULL)
{
	mExecutable = new Universal(fd());
}

MachORep::~MachORep()
{
	delete mExecutable;
	::free(mSigningData);
}


//
// Sniffer function for "plausible Mach-O binary"
//
bool MachORep::candidiate(FileDesc &fd)
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
// For Mach-O binaries that are of PowerPC architecture, we recommend
// allowing the Rosetta translator as a host. Otherwise, no suggestions.
//
static const uint8_t ppc_ireqs[] = {	// host => anchor apple and identifier com.apple.translate
	0xfa, 0xde, 0x0c, 0x01, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x14, 0xfa, 0xde, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x13,
	0x63, 0x6f, 0x6d, 0x2e, 0x61, 0x70, 0x70, 0x6c, 0x65, 0x2e, 0x74, 0x72, 0x61, 0x6e, 0x73, 0x6c,
	0x61, 0x74, 0x65, 0x00,
};

const Requirements *MachORep::defaultRequirements(const Architecture *arch)
{
	assert(arch);		// enforced by signing infrastructure
	if (arch->cpuType() == CPU_TYPE_POWERPC)
		return ((const Requirements *)ppc_ireqs)->clone();	// need to pass ownership
	else
		return NULL;
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
	if (!mSigningData)		// fetch and cache
		try {
			auto_ptr<MachO> macho(mainExecutableImage()->architecture());
			if (macho.get())
				if (size_t offset = macho->signingOffset()) {
					macho->seek(offset);
					mSigningData = EmbeddedSignatureBlob::readBlob(macho->fd());
					if (mSigningData)
						secdebug("machorep", "%zd signing bytes in %d blob(s) from %s(%s)",
							mSigningData->length(), mSigningData->count(),
							mainExecutablePath().c_str(), macho->architecture().name());
					else
						secdebug("machorep", "failed to read signing bytes from %s(%s)",
							mainExecutablePath().c_str(), macho->architecture().name());
				}
		} catch (...) {
			secdebug("machorep", "exception reading Mach-O from universal");
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
				info = macho->dataAt(macho->flip(sect64->offset), macho->flip(sect64->size));
			} else {
				info = macho->dataAt(macho->flip(sect->offset), macho->flip(sect->size));
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
				s += it->name();
			}
			return s + ")";
		} else {
			assert(archs.size() == 1);
			return string("Mach-O thin (") + archs.begin()->name() + ")";
		}
	} else
		return "not Mach-O";		// (you don't usually show that one to the user)
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
	MacOSError::throwMe(errSecCSInternalError);
}


} // end namespace CodeSigning
} // end namespace Security
