/*
 * Copyright (c) 2006-2014 Apple Inc. All Rights Reserved.
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
// signer - Signing operation supervisor and controller
//
#include "signer.h"
#include "resources.h"
#include "signerutils.h"
#include "SecCodeSigner.h"
#include <Security/SecIdentity.h>
#include <Security/CMSEncoder.h>
#include <Security/CMSPrivate.h>
#include <Security/CSCommonPriv.h>
#include <CoreFoundation/CFBundlePriv.h>
#include "resources.h"
#include "machorep.h"
#include "reqparser.h"
#include "reqdumper.h"
#include "csutilities.h"
#include <security_utilities/unix++.h>
#include <security_utilities/unixchild.h>
#include <security_utilities/cfmunge.h>
#include <security_utilities/dispatch.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>

namespace Security {
namespace CodeSigning {


//
// Sign some code.
//
void SecCodeSigner::Signer::sign(SecCSFlags flags)
{
	rep = code->diskRep()->base();
	this->prepare(flags);

	PreSigningContext context(*this);

	considerTeamID(context);

	if (Universal *fat = state.mNoMachO ? NULL : rep->mainExecutableImage()) {
		signMachO(fat, context);
	} else {
		signArchitectureAgnostic(context);
	}
}
	
	
void SecCodeSigner::Signer::considerTeamID(const PreSigningContext& context)
{
	/* If an explicit teamID was passed in it must be
	 the same as what came from the cert */
	std::string teamIDFromCert = state.getTeamIDFromSigner(context.certs);
	
	if (state.mPreserveMetadata & kSecCodeSignerPreserveTeamIdentifier) {
		/* If preserving the team identifier, teamID is set previously when the
		 code object is still available */
		if (!teamIDFromCert.empty() && teamID != teamIDFromCert)
			MacOSError::throwMe(errSecCSInvalidFlags);
	} else {
		if (teamIDFromCert.empty()) {
			/* state.mTeamID is an explicitly passed teamID */
			teamID = state.mTeamID;
		} else if (state.mTeamID.empty() || (state.mTeamID == teamIDFromCert)) {
			/* If there was no explicit team ID set, or the explicit team ID matches
			 what is in the cert, use the team ID from the certificate */
			teamID = teamIDFromCert;
		} else {
			/* The caller passed in an explicit team ID that does not match what is
			 in the signing cert, which is an invalid usage */
			MacOSError::throwMe(errSecCSInvalidFlags);
		}
	}
}
	

//
// Remove any existing code signature from code
//
void SecCodeSigner::Signer::remove(SecCSFlags flags)
{
	// can't remove a detached signature
	if (state.mDetached)
		MacOSError::throwMe(errSecCSNotSupported);

	rep = code->diskRep();
	if (Universal *fat = state.mNoMachO ? NULL : rep->mainExecutableImage()) {
		// architecture-sensitive removal
		MachOEditor editor(rep->writer(), *fat, digestAlgorithms(), rep->mainExecutablePath());
		editor.allocate();		// create copy
		editor.commit();		// commit change
	} else {
		// architecture-agnostic removal
		RefPointer<DiskRep::Writer> writer = rep->writer();
		writer->remove();
		writer->flush();
	}
}


//
// Contemplate the object-to-be-signed and set up the Signer state accordingly.
//
void SecCodeSigner::Signer::prepare(SecCSFlags flags)
{
	// make sure the rep passes strict validation
	if (strict)
		rep->strictValidate(NULL, MacOSErrorSet(), flags | (kSecCSQuickCheck|kSecCSRestrictSidebandData));
	
	// initialize progress/cancellation state
	code->prepareProgress(0);			// totally fake workload - we don't know how many files we'll encounter

	// get the Info.plist out of the rep for some creative defaulting
	CFRef<CFDictionaryRef> infoDict;
	if (CFRef<CFDataRef> infoData = rep->component(cdInfoSlot))
		infoDict.take(makeCFDictionaryFrom(infoData));
	
	uint32_t inherit = code->isSigned() ? state.mPreserveMetadata : 0;

	// work out the canonical identifier
	identifier = state.mIdentifier;
	if (identifier.empty() && (inherit & kSecCodeSignerPreserveIdentifier))
		identifier = code->identifier();
	if (identifier.empty()) {
		identifier = rep->recommendedIdentifier(*this);
		if (identifier.find('.') == string::npos)
			identifier = state.mIdentifierPrefix + identifier;
		if (identifier.find('.') == string::npos && isAdhoc())
			identifier = identifier + "-" + uniqueName();
		secinfo("signer", "using default identifier=%s", identifier.c_str());
	} else
		secinfo("signer", "using explicit identifier=%s", identifier.c_str());

	teamID = state.mTeamID;
	if (teamID.empty() && (inherit & kSecCodeSignerPreserveTeamIdentifier)) {
		const char *c_id = code->teamID();
		if (c_id)
			teamID = c_id;
	}
	
	// Digest algorithms: explicit or preserved. Subject to diskRep defaults or final default later.
	hashAlgorithms = state.mDigestAlgorithms;
	if (hashAlgorithms.empty() && (inherit & kSecCodeSignerPreserveDigestAlgorithm))
		hashAlgorithms = code->hashAlgorithms();
	
	entitlements = state.mEntitlementData;
	if (!entitlements && (inherit & kSecCodeSignerPreserveEntitlements))
		entitlements = code->component(cdEntitlementSlot);
	
	// work out the CodeDirectory flags word
	bool haveCdFlags = false;
	if (!haveCdFlags && state.mCdFlagsGiven) {
		cdFlags = state.mCdFlags;
		secinfo("signer", "using explicit cdFlags=0x%x", cdFlags);
		haveCdFlags = true;
	}
	if (!haveCdFlags) {
		cdFlags = 0;
		if (infoDict)
			if (CFTypeRef csflags = CFDictionaryGetValue(infoDict, CFSTR("CSFlags"))) {
				if (CFGetTypeID(csflags) == CFNumberGetTypeID()) {
					cdFlags = cfNumber<uint32_t>(CFNumberRef(csflags));
					secinfo("signer", "using numeric cdFlags=0x%x from Info.plist", cdFlags);
				} else if (CFGetTypeID(csflags) == CFStringGetTypeID()) {
					cdFlags = cdTextFlags(cfString(CFStringRef(csflags)));
					secinfo("signer", "using text cdFlags=0x%x from Info.plist", cdFlags);
				} else
					MacOSError::throwMe(errSecCSBadDictionaryFormat);
				haveCdFlags = true;
			}
	}
	if (!haveCdFlags && (inherit & kSecCodeSignerPreserveFlags)) {
		cdFlags = code->codeDirectory(false)->flags & ~kSecCodeSignatureAdhoc;
		secinfo("signer", "using inherited cdFlags=0x%x", cdFlags);
		haveCdFlags = true;
	}
	if (!haveCdFlags)
		cdFlags = 0;
	if (state.mSigner == SecIdentityRef(kCFNull))	// ad-hoc signing requested...
		cdFlags |= kSecCodeSignatureAdhoc;	// ... so note that

	// prepare the internal requirements input
	if (state.mRequirements) {
		if (CFGetTypeID(state.mRequirements) == CFDataGetTypeID()) {		// binary form
			const Requirements *rp = (const Requirements *)CFDataGetBytePtr(state.mRequirements.as<CFDataRef>());
			if (!rp->validateBlob())
				MacOSError::throwMe(errSecCSReqInvalid);
			requirements = rp->clone();
		} else if (CFGetTypeID(state.mRequirements) == CFStringGetTypeID()) { // text form
			CFRef<CFMutableStringRef> reqText = CFStringCreateMutableCopy(NULL, 0, state.mRequirements.as<CFStringRef>());
			// substitute $ variable tokens
			CFRange range = { 0, CFStringGetLength(reqText) };
			CFStringFindAndReplace(reqText, CFSTR("$self.identifier"), CFTempString(identifier), range, 0);
			requirements = parseRequirements(cfString(reqText));
		} else
			MacOSError::throwMe(errSecCSInvalidObjectRef);
	} else if (inherit & kSecCodeSignerPreserveRequirements)
		if (const Requirements *rp = code->internalRequirements())
			requirements = rp->clone();
	
	// prepare the resource directory, if any
	string rpath = rep->resourcesRootPath();
	string rrpath;
	CFCopyRef<CFDictionaryRef> resourceRules;
	if (!rpath.empty()) {
		// explicitly given resource rules always win
		resourceRules = state.mResourceRules;
		
		// inherited rules come next (overriding embedded ones!)
		if (!resourceRules && (inherit & kSecCodeSignerPreserveResourceRules))
			if (CFDictionaryRef oldRules = code->resourceDictionary(false))
				resourceRules = oldRules;
		
		// embedded resource rules come next
		if (!resourceRules && infoDict)
			if (CFTypeRef spec = CFDictionaryGetValue(infoDict, _kCFBundleResourceSpecificationKey)) {
				if (CFGetTypeID(spec) == CFStringGetTypeID())
					if (CFRef<CFDataRef> data = cfLoadFile(rpath + "/" + cfString(CFStringRef(spec))))
						if (CFDictionaryRef dict = makeCFDictionaryFrom(data))
							resourceRules.take(dict);
				if (!resourceRules)	// embedded rules present but unacceptable
					MacOSError::throwMe(errSecCSResourceRulesInvalid);
			}
		
		// if we got one from anywhere (but the defaults), sanity-check it
		if (resourceRules) {
			CFTypeRef rules = CFDictionaryGetValue(resourceRules, CFSTR("rules"));
			if (!rules || CFGetTypeID(rules) != CFDictionaryGetTypeID())
				MacOSError::throwMe(errSecCSResourceRulesInvalid);
		}

		// finally, ask the DiskRep for its default
		if (!resourceRules)
			resourceRules.take(rep->defaultResourceRules(*this));

		// resource root can optionally be the canonical bundle path,
		// but sealed resource paths are always relative to rpath
		rrpath = rpath;
		if (signingFlags() & kSecCSSignBundleRoot)
			rrpath = cfStringRelease(rep->copyCanonicalPath());
	}
	
	// screen and set the signing time
	if (state.mSigningTime == CFDateRef(kCFNull)) {
		emitSigningTime = false;		// no time at all
	} else if (!state.mSigningTime) {
		emitSigningTime = true;
		signingTime = 0;			// wall clock, established later
	} else {
		CFAbsoluteTime time = CFDateGetAbsoluteTime(state.mSigningTime);
		if (time > CFAbsoluteTimeGetCurrent())	// not allowed to post-date a signature
			MacOSError::throwMe(errSecCSBadDictionaryFormat);
		emitSigningTime = true;
		signingTime = time;
	}
	
	pagesize = state.mPageSize ? cfNumber<size_t>(state.mPageSize) : rep->pageSize(*this);
	
	// Allow the DiskRep to modify the signing parameters. This sees explicit and inherited values but not defaults.
	rep->prepareForSigning(*this);
	
	// apply some defaults after diskRep intervention
	if (hashAlgorithms.empty()) {	// default to SHA256 + SHA-1
		hashAlgorithms.insert(kSecCodeSignatureHashSHA1);
		hashAlgorithms.insert(kSecCodeSignatureHashSHA256);
	}
	
	// build the resource directory (once and for all, using the digests determined above)
	if (!rpath.empty()) {
		buildResources(rrpath, rpath, resourceRules);
	}
}


//
// Collect the resource seal for a program.
// This includes both sealed resources and information about nested code.
//
void SecCodeSigner::Signer::buildResources(std::string root, std::string relBase, CFDictionaryRef rulesDict)
{
	typedef ResourceBuilder::Rule Rule;
	
	secinfo("codesign", "start building resource directory");
	__block CFRef<CFMutableDictionaryRef> result = makeCFMutableDictionary();
	
	CFDictionaryRef rules = cfget<CFDictionaryRef>(rulesDict, "rules");
	assert(rules);

	if (this->state.mLimitedAsync == NULL) {
		this->state.mLimitedAsync =
			/* rdar://problem/20299541: Async workers (i.e. parallelization) are currently
			 * turned off, because the paths for signing code are not ready for it yet. */
			// new LimitedAsync(rep->fd().mediumType() == kIOPropertyMediumTypeSolidStateKey);
			new LimitedAsync(false);
	}

	CFDictionaryRef files2 = NULL;
	if (!(signingFlags() & kSecCSSignV1)) {
		CFCopyRef<CFDictionaryRef> rules2 = cfget<CFDictionaryRef>(rulesDict, "rules2");
		if (!rules2) {
			// Clone V1 rules and add default nesting rules at weight 0 (overridden by anything in rules).
			// V1 rules typically do not cover these places so we'll prevail, but if they do, we defer to them.
			rules2 = cfmake<CFDictionaryRef>("{+%O"
				"'^(Frameworks|SharedFrameworks|PlugIns|Plug-ins|XPCServices|Helpers|MacOS|Library/(Automator|Spotlight|LoginItems))/' = {nested=#T, weight=0}" // exclude dynamic repositories
			"}", rules);
		}

		Dispatch::Group group;
		Dispatch::Group &groupRef = group;  // (into block)

		// build the modern (V2) resource seal
		__block CFRef<CFMutableDictionaryRef> files = makeCFMutableDictionary();
		CFMutableDictionaryRef filesRef = files.get();	// (into block)
		ResourceBuilder resourceBuilder(root, relBase, rules2, strict, MacOSErrorSet());
		ResourceBuilder	&resources = resourceBuilder;	// (into block)
		rep->adjustResources(resources);

		resources.scan(^(FTSENT *ent, uint32_t ruleFlags, const std::string relpath, Rule *rule) {
			bool isSymlink = (ent->fts_info == FTS_SL);
			const std::string path(ent->fts_path);
			const std::string accpath(ent->fts_accpath);
			this->state.mLimitedAsync->perform(groupRef, ^{
				CFRef<CFMutableDictionaryRef> seal;
				if (ruleFlags & ResourceBuilder::nested) {
					seal.take(signNested(path, relpath));
				} else if (isSymlink) {
					char target[PATH_MAX];
					ssize_t len = ::readlink(accpath.c_str(), target, sizeof(target)-1);
					if (len < 0)
						UnixError::check(-1);
					target[len] = '\0';
					seal.take(cfmake<CFMutableDictionaryRef>("{symlink=%s}", target));
				} else {
					seal.take(resources.hashFile(accpath.c_str(), digestAlgorithms(), signingFlags() & kSecCSSignStrictPreflight));
				}
				if (ruleFlags & ResourceBuilder::optional)
					CFDictionaryAddValue(seal, CFSTR("optional"), kCFBooleanTrue);
				CFTypeRef hash;
				StLock<Mutex> _(resourceLock);
				if ((hash = CFDictionaryGetValue(seal, CFSTR("hash"))) && CFDictionaryGetCount(seal) == 1) // simple form
					CFDictionaryAddValue(filesRef, CFTempString(relpath).get(), hash);
				else
					CFDictionaryAddValue(filesRef, CFTempString(relpath).get(), seal.get());
				code->reportProgress();
			});
		});
		group.wait();
		CFDictionaryAddValue(result, CFSTR("rules2"), resourceBuilder.rules());
		files2 = files;
		CFDictionaryAddValue(result, CFSTR("files2"), files2);
	}
	
	CFDictionaryAddValue(result, CFSTR("rules"), rules);	// preserve V1 rules in any case
	if (!(signingFlags() & kSecCSSignNoV1)) {
		// build the legacy (V1) resource seal
		__block CFRef<CFMutableDictionaryRef> files = makeCFMutableDictionary();
		ResourceBuilder resourceBuilder(root, relBase, rules, strict, MacOSErrorSet());
		ResourceBuilder	&resources = resourceBuilder;
		rep->adjustResources(resources);	// DiskRep-specific adjustments
		resources.scan(^(FTSENT *ent, uint32_t ruleFlags, std::string relpath, Rule *rule) {
			if (ent->fts_info == FTS_F) {
				CFRef<CFDataRef> hash;
				if (files2)	// try to get the hash from a previously-made version
					if (CFTypeRef seal = CFDictionaryGetValue(files2, CFTempString(relpath))) {
						if (CFGetTypeID(seal) == CFDataGetTypeID())
							hash = CFDataRef(seal);
						else
							hash = CFDataRef(CFDictionaryGetValue(CFDictionaryRef(seal), CFSTR("hash")));
					}
				if (!hash)
					hash.take(resources.hashFile(ent->fts_accpath, kSecCodeSignatureHashSHA1));
				if (ruleFlags == 0) {	// default case - plain hash
					cfadd(files, "{%s=%O}", relpath.c_str(), hash.get());
					secinfo("csresource", "%s added simple (rule %p)", relpath.c_str(), rule);
				} else {	// more complicated - use a sub-dictionary
					cfadd(files, "{%s={hash=%O,optional=%B}}",
						relpath.c_str(), hash.get(), ruleFlags & ResourceBuilder::optional);
					secinfo("csresource", "%s added complex (rule %p)", relpath.c_str(), rule);
				}
			}
		});
		CFDictionaryAddValue(result, CFSTR("files"), files.get());
	}
	
	resourceDirectory = result.get();
	resourceDictData.take(makeCFData(resourceDirectory.get()));
}


//
// Deal with one piece of nested code
//
CFMutableDictionaryRef SecCodeSigner::Signer::signNested(const std::string &path, const std::string &relpath)
{
	// sign nested code and collect nesting information
	try {
		SecPointer<SecStaticCode> code = new SecStaticCode(DiskRep::bestGuess(path));
		if (signingFlags() & kSecCSSignNestedCode)
			this->state.sign(code, signingFlags());
		std::string dr = Dumper::dump(code->designatedRequirement());
		if (CFDataRef hash = code->cdHash())
			return cfmake<CFMutableDictionaryRef>("{requirement=%s,cdhash=%O}",
				Dumper::dump(code->designatedRequirement()).c_str(),
				hash);
		MacOSError::throwMe(errSecCSUnsigned);
	} catch (const CommonError &err) {
		CSError::throwMe(err.osStatus(), kSecCFErrorPath, CFTempURL(relpath, false, this->code->resourceBase()));
	}
}


//
// Sign a Mach-O binary, using liberal dollops of that special Mach-O magic sauce.
// Note that this will deal just fine with non-fat Mach-O binaries, but it will
// treat them as architectural binaries containing (only) one architecture - that
// interpretation is courtesy of the Universal/MachO support classes.
//
void SecCodeSigner::Signer::signMachO(Universal *fat, const Requirement::Context &context)
{
	// Mach-O executable at the core - perform multi-architecture signing
	RefPointer<DiskRep::Writer> writer = rep->writer();
	auto_ptr<ArchEditor> editor(state.mDetached
		? static_cast<ArchEditor *>(new BlobEditor(*fat, *this))
		: new MachOEditor(writer, *fat, this->digestAlgorithms(), rep->mainExecutablePath()));
	assert(editor->count() > 0);
	if (!editor->attribute(writerNoGlobal))	// can store architecture-common components
		populate(*editor);
	
	// pass 1: prepare signature blobs and calculate sizes
	for (MachOEditor::Iterator it = editor->begin(); it != editor->end(); ++it) {
		MachOEditor::Arch &arch = *it->second;
		arch.source.reset(fat->architecture(it->first));
		
		// library validation is not compatible with i386
		if (arch.architecture.cpuType() == CPU_TYPE_I386) {
			if (cdFlags & kSecCodeSignatureLibraryValidation) {
				MacOSError::throwMe(errSecCSBadLVArch);
			}
		}
		
		arch.ireqs(requirements, rep->defaultRequirements(&arch.architecture, *this), context);
		if (editor->attribute(writerNoGlobal))	// can't store globally, add per-arch
			populate(arch);
		for (auto type = digestAlgorithms().begin(); type != digestAlgorithms().end(); ++type) {
			arch.eachDigest(^(CodeDirectory::Builder& builder) {
				populate(builder, arch, arch.ireqs,
						 arch.source->offset(), arch.source->signingExtent(), unsigned(digestAlgorithms().size()-1));
			});
		}
	
		// add identification blob (made from this architecture) only if we're making a detached signature
		if (state.mDetached) {
			CFRef<CFDataRef> identification = MachORep::identificationFor(arch.source.get());
			arch.add(cdIdentificationSlot, BlobWrapper::alloc(
				CFDataGetBytePtr(identification), CFDataGetLength(identification)));
		}
		
		// prepare SuperBlob size estimate
		__block std::vector<size_t> sizes;
		arch.eachDigest(^(CodeDirectory::Builder& builder){
			sizes.push_back(builder.size(CodeDirectory::currentVersion));
		});
		arch.blobSize = arch.size(sizes, state.mCMSSize, 0);
	}
	
	editor->allocate();
	
	// pass 2: Finish and generate signatures, and write them
	for (MachOEditor::Iterator it = editor->begin(); it != editor->end(); ++it) {
		MachOEditor::Arch &arch = *it->second;
		editor->reset(arch);

		// finish CodeDirectories (off new binary) and sign it
		__block CodeDirectorySet cdSet;
		arch.eachDigest(^(CodeDirectory::Builder &builder) {
			CodeDirectory *cd = builder.build();
			cdSet.add(cd);
		});
		CFRef<CFArrayRef> hashes = cdSet.hashBag();
		CFTemp<CFDictionaryRef> hashDict("{cdhashes=%O}", hashes.get());
		CFRef<CFDataRef> hashBag = makeCFData(hashDict.get());
		CFRef<CFDataRef> signature = signCodeDirectory(cdSet.primary(), hashBag);
		
		// complete the SuperBlob
		cdSet.populate(&arch);
		arch.add(cdSignatureSlot, BlobWrapper::alloc(
			CFDataGetBytePtr(signature), CFDataGetLength(signature)));
		if (!state.mDryRun) {
			EmbeddedSignatureBlob *blob = arch.make();
			editor->write(arch, blob);	// takes ownership of blob
		}
	}
	
	// done: write edit copy back over the original
	if (!state.mDryRun) {
		editor->commit();
	}
}


//
// Sign a binary that has no notion of architecture.
// That currently means anything that isn't Mach-O format.
//
void SecCodeSigner::Signer::signArchitectureAgnostic(const Requirement::Context &context)
{
	// non-Mach-O executable - single-instance signing
	RefPointer<DiskRep::Writer> writer = state.mDetached ?
		(new DetachedBlobWriter(*this)) : rep->writer();
	
	CodeDirectorySet cdSet;
	for (auto type = digestAlgorithms().begin(); type != digestAlgorithms().end(); ++type) {
		CodeDirectory::Builder builder(*type);
		InternalRequirements ireqs;
		ireqs(requirements, rep->defaultRequirements(NULL, *this), context);
		populate(*writer);
		populate(builder, *writer, ireqs, rep->signingBase(), rep->signingLimit(), unsigned(digestAlgorithms().size()-1));
		
		CodeDirectory *cd = builder.build();
		if (!state.mDryRun)
			cdSet.add(cd);
	}
	
	// add identification blob (made from this architecture) only if we're making a detached signature
	if (state.mDetached) {
		CFRef<CFDataRef> identification = rep->identification();
		writer->component(cdIdentificationSlot, identification);
	}

	// write out all CodeDirectories
	if (!state.mDryRun)
		cdSet.populate(writer);

	CFRef<CFArrayRef> hashes = cdSet.hashBag();
	CFTemp<CFDictionaryRef> hashDict("{cdhashes=%O}", hashes.get());
	CFRef<CFDataRef> hashBag = makeCFData(hashDict.get());
	CFRef<CFDataRef> signature = signCodeDirectory(cdSet.primary(), hashBag);
	writer->signature(signature);
	
	// commit to storage
	writer->flush();
}


//
// Global populate - send components to destination buffers ONCE
//
void SecCodeSigner::Signer::populate(DiskRep::Writer &writer)
{
	if (resourceDirectory && !state.mDryRun)
		writer.component(cdResourceDirSlot, resourceDictData);
}


//
// Per-architecture populate - send components to per-architecture buffers
// and populate the CodeDirectory for an architecture. In architecture-agnostic
// signing operations, the non-architectural binary is considered one (arbitrary) architecture
// for the purposes of this call.
//
void SecCodeSigner::Signer::populate(CodeDirectory::Builder &builder, DiskRep::Writer &writer,
	InternalRequirements &ireqs, size_t offset, size_t length, unsigned alternateDigestCount)
{
	// fill the CodeDirectory
	builder.executable(rep->mainExecutablePath(), pagesize, offset, length);
	builder.flags(cdFlags);
	builder.identifier(identifier);
	builder.teamID(teamID);
	builder.platform(state.mPlatform);

	if (CFRef<CFDataRef> data = rep->component(cdInfoSlot))
		builder.specialSlot(cdInfoSlot, data);
	if (ireqs) {
		CFRef<CFDataRef> data = makeCFData(*ireqs);
		writer.component(cdRequirementsSlot, data);
		builder.specialSlot(cdRequirementsSlot, data);
	}
	if (resourceDirectory)
		builder.specialSlot(cdResourceDirSlot, resourceDictData);
	if (entitlements) {
		writer.component(cdEntitlementSlot, entitlements);
		builder.specialSlot(cdEntitlementSlot, entitlements);
	}
	if (CFRef<CFDataRef> repSpecific = rep->component(cdRepSpecificSlot))
		builder.specialSlot(cdRepSpecificSlot, repSpecific);
	
	writer.addDiscretionary(builder);
	
#if 0 // rdar://problem/25720754
	if ((signingFlags() & (kSecCSSignOpaque|kSecCSSignV1)) == 0 && builder.hashType() != kSecCodeSignatureHashSHA1) {
		// calculate sorted list of top SuperBlob keys in this EmbeddedSignatureBlob (if any)
		// (but not for opaque or V1 construction, which must remain bit-for-bit compatible)
		std::vector<Endian<uint32_t> > slotVector;
		slotVector.push_back(cdCodeDirectorySlot);	// mandatory
		std::set<CodeDirectory::Slot> filledSlots = builder.filledSpecialSlots();
		filledSlots.insert(cdTopDirectorySlot);		// will be added below
		copy(filledSlots.begin(), filledSlots.end(), back_inserter(slotVector));
		for (unsigned n = 0; n < alternateDigestCount; n++)
			slotVector.push_back(cdAlternateCodeDirectorySlots + n);
		slotVector.push_back(cdSignatureSlot);
		CFTempData cfSlotVector(&slotVector[0], slotVector.size() * sizeof(slotVector[0]));
		writer.component(cdTopDirectorySlot, cfSlotVector);
		builder.specialSlot(cdTopDirectorySlot, cfSlotVector);
	}
#endif
}

	
#include <security_smime/tsaSupport.h>

//
// Generate the CMS signature for a (finished) CodeDirectory.
//
CFDataRef SecCodeSigner::Signer::signCodeDirectory(const CodeDirectory *cd, CFDataRef hashBag)
{
	assert(state.mSigner);
	CFRef<CFMutableDictionaryRef> defaultTSContext = NULL;
    
	// a null signer generates a null signature blob
	if (state.mSigner == SecIdentityRef(kCFNull))
		return CFDataCreate(NULL, NULL, 0);
	
	// generate CMS signature
	CFRef<CMSEncoderRef> cms;
	MacOSError::check(CMSEncoderCreate(&cms.aref()));
	MacOSError::check(CMSEncoderSetCertificateChainMode(cms, kCMSCertificateChainWithRoot));
	CMSEncoderAddSigners(cms, state.mSigner);
	CMSEncoderSetSignerAlgorithm(cms, kCMSEncoderDigestAlgorithmSHA256);
	MacOSError::check(CMSEncoderSetHasDetachedContent(cms, true));
	
	if (emitSigningTime) {
		MacOSError::check(CMSEncoderAddSignedAttributes(cms, kCMSAttrSigningTime));
		CFAbsoluteTime time = signingTime ? signingTime : CFAbsoluteTimeGetCurrent();
		MacOSError::check(CMSEncoderSetSigningTime(cms, time));
	}
	
	if (hashBag) {
		MacOSError::check(CMSEncoderAddSignedAttributes(cms, kCMSAttrAppleCodesigningHashAgility));
		MacOSError::check(CMSEncoderSetAppleCodesigningHashAgility(cms, hashBag));
	}
	
	MacOSError::check(CMSEncoderUpdateContent(cms, cd, cd->length()));
    
    // Set up to call Timestamp server if requested
    if (state.mWantTimeStamp)
    {
        CFRef<CFErrorRef> error = NULL;
        defaultTSContext = SecCmsTSAGetDefaultContext(&error.aref());
        if (error)
            MacOSError::throwMe(errSecDataNotAvailable);
            
        if (state.mNoTimeStampCerts || state.mTimestampService) {
            if (state.mTimestampService)
                CFDictionarySetValue(defaultTSContext, kTSAContextKeyURL, state.mTimestampService);
            if (state.mNoTimeStampCerts)
                CFDictionarySetValue(defaultTSContext, kTSAContextKeyNoCerts, kCFBooleanTrue);
		}
            
		CmsMessageSetTSAContext(cms, defaultTSContext);
    }
	
	CFDataRef signature;
	MacOSError::check(CMSEncoderCopyEncodedContent(cms, &signature));

	return signature;
}
	
	
//
// Our DiskRep::signingContext methods communicate with the signing subsystem
// in terms those callers can easily understand.
//
string SecCodeSigner::Signer::sdkPath(const std::string &path) const
{
	assert(path[0] == '/');	// need absolute path here
	if (state.mSDKRoot)
		return cfString(state.mSDKRoot) + path;
	else
		return path;
}

bool SecCodeSigner::Signer::isAdhoc() const
{
	return state.mSigner == SecIdentityRef(kCFNull);
}

SecCSFlags SecCodeSigner::Signer::signingFlags() const
{
	return state.mOpFlags;
}


//
// Parse a text of the form
//	flag,...,flag
// where each flag is the canonical name of a signable CodeDirectory flag.
// No abbreviations are allowed, and internally set flags are not accepted.
//
uint32_t SecCodeSigner::Signer::cdTextFlags(std::string text)
{
	uint32_t flags = 0;
	for (string::size_type comma = text.find(','); ; text = text.substr(comma+1), comma = text.find(',')) {
		string word = (comma == string::npos) ? text : text.substr(0, comma);
		const SecCodeDirectoryFlagTable *item;
		for (item = kSecCodeDirectoryFlagTable; item->name; item++)
			if (item->signable && word == item->name) {
				flags |= item->value;
				break;
			}
		if (!item->name)	// not found
			MacOSError::throwMe(errSecCSInvalidFlags);
		if (comma == string::npos)	// last word
			break;
	}
	return flags;
}


//
// Generate a unique string from our underlying DiskRep.
// We could get 90%+ of the uniquing benefit by just generating
// a random string here. Instead, we pick the (hex string encoding of)
// the source rep's unique identifier blob. For universal binaries,
// this is the canonical local architecture, which is a bit arbitrary.
// This provides us with a consistent unique string for all architectures
// of a fat binary, *and* (unlike a random string) is reproducible
// for identical inputs, even upon resigning.
//
std::string SecCodeSigner::Signer::uniqueName() const
{
	CFRef<CFDataRef> identification = rep->identification();
	const UInt8 *ident = CFDataGetBytePtr(identification);
	const CFIndex length = CFDataGetLength(identification);
	string result;
	for (CFIndex n = 0; n < length; n++) {
		char hex[3];
		snprintf(hex, sizeof(hex), "%02x", ident[n]);
		result += hex;
	}
	return result;
}


} // end namespace CodeSigning
} // end namespace Security
