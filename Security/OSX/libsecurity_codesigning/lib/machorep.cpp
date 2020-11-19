/*
 * Copyright (c) 2006,2011-2012,2014 Apple Inc. All Rights Reserved.
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
#include "notarization.h"
#include "StaticCode.h"
#include "reqmaker.h"
#include <security_utilities/logging.h>
#include <security_utilities/cfmunge.h>
#include <security_utilities/casts.h>



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
			mExecutable = new Universal(fd(), (size_t)ctx->offset, ctx->size);
		else if (ctx->arch) {
			unique_ptr<Universal> full(new Universal(fd()));
			mExecutable = new Universal(fd(), full->archOffset(ctx->arch), full->archLength(ctx->arch));
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
	case MH_KEXT_BUNDLE:
	case MH_PRELOAD:
		return true;		// dynamic image; supported
	case MH_OBJECT:
		return false;		// maybe later...
	default:
		return false;		// not Mach-O (or too exotic)
	}
}



//
// Nowadays, the main executable object is created upon construction.
//
Universal *MachORep::mainExecutableImage()
{
	return mExecutable;
}

	
//
// Explicitly default to SHA256 (only) digests if the minimum deployment
// target is young enough.
//
void MachORep::prepareForSigning(SigningContext &context)
{
	if (context.digestAlgorithms().empty()) {
        unique_ptr<MachO> macho(mainExecutableImage()->architecture());
		
		uint32_t limit = 0;
		switch (macho->platform()) {
			case 0:
				// If we don't know the platform, we stay agile.
				return;
			case PLATFORM_MACOS:
				// 10.11.4 had first proper sha256 support.
				limit = (10 << 16 | 11 << 8 | 4 << 0);
				break;
			case PLATFORM_TVOS:
			case PLATFORM_IOS:
				// iOS 11 and tvOS 11 had first proper sha256 support.
				limit = (11 << 16 | 0 << 8 | 0 << 0);
				break;
			case PLATFORM_WATCHOS:
				// We stay agile on the watch right now.
				return;
			default:
				// All other platforms are assumed to be new and support SHA256.
				break;
		}
		if (macho->minVersion() >= limit) {
			// young enough not to need SHA-1 legacy support
			context.setDigestAlgorithm(kSecCodeSignatureHashSHA256);
		}
	}
}


//
// Signing base is the start of the Mach-O architecture we're using
//
size_t MachORep::signingBase()
{
	return mainExecutableImage()->archOffset();
}
	
size_t MachORep::signingLimit()
{
	unique_ptr<MachO> macho(mExecutable->architecture());
	return macho->signingExtent();
}

bool MachORep::needsExecSeg(const MachO& macho) {
	uint32_t platform = macho.platform();
	
	// Everything gets an exec segment. This is ignored
	// on non-PPL devices, and explicitly wastes some
	// space on those devices, but is simpler logic.
	return platform != 0;
}

size_t MachORep::execSegBase(const Architecture *arch)
{
	unique_ptr<MachO> macho(arch ? mExecutable->architecture(*arch) : mExecutable->architecture());

	if (!needsExecSeg(*macho)) {
		return 0;
	}

	segment_command const * const text_cmd = macho->findSegment("__TEXT");

	if (text_cmd == NULL) {
		return 0;
	}

	size_t off = 0;

	if (macho->is64()) {
		off = int_cast<uint64_t,size_t>(reinterpret_cast<segment_command_64 const * const>(text_cmd)->fileoff);
	} else {
		off = text_cmd->fileoff;
	}

	return off;
}

size_t MachORep::execSegLimit(const Architecture *arch)
{
	unique_ptr<MachO> macho(arch ? mExecutable->architecture(*arch) : mExecutable->architecture());

	if (!needsExecSeg(*macho)) {
		return 0;
	}

	segment_command const * const text_cmd = macho->findSegment("__TEXT");

	if (text_cmd == NULL) {
		return 0;
	}

	size_t size = 0;

	if (macho->is64()) {
		size = int_cast<uint64_t,size_t>(reinterpret_cast<segment_command_64 const * const>(text_cmd)->filesize);
	} else {
		size = text_cmd->filesize;
	}

	return size;
}


//
// We choose the binary identifier for a Mach-O binary as follows:
//	- If the Mach-O headers have a UUID command, use the UUID.
//	- Otherwise, use the SHA-1 hash of the (entire) load commands.
//
CFDataRef MachORep::identification()
{
	std::unique_ptr<MachO> macho(mainExecutableImage()->architecture());
	return identificationFor(macho.get());
}

CFDataRef MachORep::identificationFor(MachO *macho)
{
	// if there is a LC_UUID load command, use the UUID contained therein
	if (const load_command *cmd = macho->findCommand(LC_UUID)) {
		const uuid_command *uuidc = reinterpret_cast<const uuid_command *>(cmd);
		// uuidc->cmdsize should be sizeof(uuid_command), so if it is not,
		// something is wrong. Fail out.
		if (macho->flip(uuidc->cmdsize) != sizeof(uuid_command))
			MacOSError::throwMe(errSecCSSignatureInvalid);
		char result[4 + sizeof(uuidc->uuid)];
		memcpy(result, "UUID", 4);
		memcpy(result+4, uuidc->uuid, sizeof(uuidc->uuid));
		return makeCFData(result, sizeof(result));
	}
	
	// otherwise, use the SHA-1 hash of the entire load command area (this is way, way obsolete)
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

//
// Retrieve all components, used for signature editing.
//
EditableDiskRep::RawComponentMap MachORep::createRawComponents()
{
	EditableDiskRep::RawComponentMap  blobMap;

	// First call to signingData() caches the result, so this
	// _should_ not cause performance issues.
	if (NULL == signingData()) {
		MacOSError::throwMe(errSecCSUnsigned);
	}
	const EmbeddedSignatureBlob &blobs = *signingData();
	
	for (unsigned int i = 0; i < blobs.count(); ++i) {
		CodeDirectory::Slot slot = blobs.type(i);
		const BlobCore *blob = blobs.blob(i);
		blobMap[slot] = blobs.blobData(slot, blob);
	}
	return blobMap;
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
	if (signingData()) {
		return signingData()->component(slot);
	}
	
	// not found
	return NULL;
}
	
	

EmbeddedSignatureBlob *MachORep::signingData()
{
	if (!mSigningData) {		// fetch and cache
		unique_ptr<MachO> macho(mainExecutableImage()->architecture());
		if (macho.get())
			if (const linkedit_data_command *cs = macho->findCodeSignature()) {
				size_t offset = macho->flip(cs->dataoff);
				size_t length = macho->flip(cs->datasize);
				if ((mSigningData = EmbeddedSignatureBlob::readBlob(macho->fd(), macho->offset() + offset, length))) {
					secinfo("machorep", "%zd signing bytes in %d blob(s) from %s(%s)",
							mSigningData->length(), mSigningData->count(),
							mainExecutablePath().c_str(), macho->architecture().name());
				} else {
					secinfo("machorep", "failed to read signing bytes from %s(%s)",
							mainExecutablePath().c_str(), macho->architecture().name());
					MacOSError::throwMe(errSecCSSignatureInvalid);
				}
			}
	}
	return mSigningData;
}


//
// Extract an embedded Info.plist from the file.
// Returns NULL if none is found.
//
CFDataRef MachORep::infoPlist()
{
	CFRef<CFDataRef> info;
	try {
		unique_ptr<MachO> macho(mainExecutableImage()->architecture());
		if (const section *sect = macho->findSection("__TEXT", "__info_plist")) {
			if (macho->is64()) {
				const section_64 *sect64 = reinterpret_cast<const section_64 *>(sect);
				info.take(macho->dataAt(macho->flip(sect64->offset), (size_t)macho->flip(sect64->size)));
			} else {
				info.take(macho->dataAt(macho->flip(sect->offset), macho->flip(sect->size)));
			}
		}
	} catch (...) {
		secinfo("machorep", "exception reading embedded Info.plist");
	}
	return info.yield();
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
	size_t offset = mExecutable->offset();
	size_t length = mExecutable->length();
	delete mExecutable;
	mExecutable = NULL;
	::free(mSigningData);
	mSigningData = NULL;
	SingleDiskRep::flush();
	mExecutable = new Universal(fd(), offset, length);
}

CFDictionaryRef MachORep::diskRepInformation()
{
    unique_ptr<MachO> macho (mainExecutableImage()->architecture());
    CFRef<CFDictionaryRef> info;

	uint32_t platform = 0;
	uint32_t minVersion = 0;
	uint32_t sdkVersion = 0;
	
    if (macho->version(&platform, &minVersion, &sdkVersion)) {

		/* These keys replace the old kSecCodeInfoDiskRepOSPlatform, kSecCodeInfoDiskRepOSVersionMin
		 * and kSecCodeInfoDiskRepOSSDKVersion. The keys were renamed because we changed what value
		 * "platform" represents: For the old key, the actual load command (e.g. LC_VERSION_MIN_MACOSX)
		 * was returned; for the new key, we return one of the PLATFORM_* values used by LC_BUILD_VERSION.
		 *
		 * The keys are private and undocumented, and maintaining a translation table between the old and
		 * new domain would provide little value at high cost, but we do remove the old keys to make
		 * the change obvious.
		 */
		
        info.take(cfmake<CFMutableDictionaryRef>("{%O = %d,%O = %d,%O = %d}",
                                              kSecCodeInfoDiskRepVersionPlatform, platform,
                                              kSecCodeInfoDiskRepVersionMin, minVersion,
                                              kSecCodeInfoDiskRepVersionSDK, sdkVersion));

        if (platform == PLATFORM_MACOS && sdkVersion < (10 << 16 | 9 << 8))
        {
            info.take(cfmake<CFMutableDictionaryRef>("{+%O, %O = 'OS X SDK version before 10.9 does not support Library Validation'}",
                                                  info.get(),
                                                  kSecCodeInfoDiskRepNoLibraryValidation));
        }
    }

    return info.yield();
}


//
// Return a recommended unique identifier.
// If our file has an embedded Info.plist, use the CFBundleIdentifier from that.
// Otherwise, use the default.
//
string MachORep::recommendedIdentifier(const SigningContext &ctx)
{
	if (CFDataRef info = infoPlist()) {
		if (CFRef<CFDictionaryRef> dict = makeCFDictionaryFrom(info)) {
			CFStringRef code = CFStringRef(CFDictionaryGetValue(dict, kCFBundleIdentifierKey));
			if (code && CFGetTypeID(code) != CFStringGetTypeID())
				MacOSError::throwMe(errSecCSBadDictionaryFormat);
			if (code)
				return cfString(code);
		} else
			MacOSError::throwMe(errSecCSBadDictionaryFormat);
	}
	
	// ah well. Use the default
	return SingleDiskRep::recommendedIdentifier(ctx);
}


//
// The default suggested requirements for Mach-O binaries are as follows:
// Library requirement: Composed from dynamic load commands.
//
const Requirements *MachORep::defaultRequirements(const Architecture *arch, const SigningContext &ctx)
{
	assert(arch);		// enforced by signing infrastructure
	Requirements::Maker maker;
		
	// add library requirements from DYLIB commands (if any)
	if (Requirement *libreq = libraryRequirements(arch, ctx))
		maker.add(kSecLibraryRequirementType, libreq);	// takes ownership

	// that's all
	return maker.make();
}

Requirement *MachORep::libraryRequirements(const Architecture *arch, const SigningContext &ctx)
{
	unique_ptr<MachO> macho(mainExecutableImage()->architecture(*arch));
	Requirement::Maker maker;
	Requirement::Maker::Chain chain(maker, opOr);

	if (macho.get())
		if (const linkedit_data_command *ldep = macho->findLibraryDependencies()) {
			size_t offset = macho->flip(ldep->dataoff);
			size_t length = macho->flip(ldep->datasize);
			if (LibraryDependencyBlob *deplist = LibraryDependencyBlob::readBlob(macho->fd(), macho->offset() + offset, length)) {
				try {
					secinfo("machorep", "%zd library dependency bytes in %d blob(s) from %s(%s)",
						deplist->length(), deplist->count(),
						mainExecutablePath().c_str(), macho->architecture().name());
					unsigned count = deplist->count();
					// we could walk through DYLIB load commands in parallel. We just don't need anything from them so far
					for (unsigned n = 0; n < count; n++) {
						const Requirement *req = NULL;
						if (const BlobCore *dep = deplist->blob(n)) {
							if ((req = Requirement::specific(dep))) {
								// binary code requirement; good to go
							} else if (const BlobWrapper *wrap = BlobWrapper::specific(dep)) {
								// blob-wrapped text form - convert to binary requirement
								std::string reqString = std::string((const char *)wrap->data(), wrap->length());
								CFRef<SecRequirementRef> areq;
								MacOSError::check(SecRequirementCreateWithString(CFTempString(reqString), kSecCSDefaultFlags, &areq.aref()));
								CFRef<CFDataRef> reqData;
								MacOSError::check(SecRequirementCopyData(areq, kSecCSDefaultFlags, &reqData.aref()));
								req = Requirement::specific((const BlobCore *)CFDataGetBytePtr(reqData));
							} else {
								secinfo("machorep", "unexpected blob type 0x%x in slot %d of binary dependencies", dep->magic(), n);
								continue;
							}
							chain.add();
							maker.copy(req);
						} else
							secinfo("machorep", "missing DR info for library index %d", n);
					}
					::free(deplist);
				} catch (...) {
					::free(deplist);
					throw;
				}
			}
		}
	if (chain.empty())
		return NULL;
	else
		return maker.make();
}


//
// Default to system page size for segmented (paged) signatures
//
size_t MachORep::pageSize(const SigningContext &)
{
	return segmentedPageSize;
}


//
// Strict validation
//
void MachORep::strictValidate(const CodeDirectory* cd, const ToleratedErrors& tolerated, SecCSFlags flags)
{
	SingleDiskRep::strictValidate(cd, tolerated, flags);
	
	// if the constructor found suspicious issues, fail a struct validation now
	if (mExecutable->isSuspicious() && tolerated.find(errSecCSBadMainExecutable) == tolerated.end())
		MacOSError::throwMe(errSecCSBadMainExecutable);
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
    Syslog::notice("code signing internal error: trying to write Mach-O component directly");
	MacOSError::throwMe(errSecCSInternalError);
}

void MachORep::registerStapledTicket()
{
	CFRef<CFDataRef> data = NULL;
	if (mSigningData) {
		data.take(mSigningData->component(cdTicketSlot));
		registerStapledTicketInMachO(data);
	}
}

} // end namespace CodeSigning
} // end namespace Security
