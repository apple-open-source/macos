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
#include "bundlediskrep.h"
#include "der_plist.h"
#include "signer.h"
#include "resources.h"
#include "remotesigner.h"
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
#include <CoreEntitlements/CoreEntitlements.h>
#include <CoreEntitlements/FoundationUtils.h>

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
		if (!teamIDFromCert.empty() && teamID != teamIDFromCert) {
			MacOSError::throwMe(errSecCSInvalidFlags);
		}
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
	if (state.mDetached) {
		MacOSError::throwMe(errSecCSNotSupported);
	}

	rep = code->diskRep();

	if (state.mPreserveAFSC) {
		rep->writer()->setPreserveAFSC(state.mPreserveAFSC);
	}

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
	if (strict) {
		rep->strictValidate(NULL, MacOSErrorSet(), flags | (kSecCSQuickCheck|kSecCSRestrictSidebandData));
	}
	
	// initialize progress/cancellation state
	code->prepareProgress(0);			// totally fake workload - we don't know how many files we'll encounter

	// get the Info.plist out of the rep for some creative defaulting
	CFRef<CFDictionaryRef> infoDict;
	if (CFRef<CFDataRef> infoData = rep->component(cdInfoSlot)) {
		infoDict.take(makeCFDictionaryFrom(infoData));
	}
	
	uint32_t inherit = 0;

	if (code->isSigned() && (code->codeDirectory(false)->flags & kSecCodeSignatureLinkerSigned) == 0) {
		inherit = state.mPreserveMetadata;
	}

	// work out the canonical identifier
	identifier = state.mIdentifier;
	if (identifier.empty() && (inherit & kSecCodeSignerPreserveIdentifier)) {
		identifier = code->identifier();
	}
	if (identifier.empty()) {
		identifier = rep->recommendedIdentifier(*this);
		if (identifier.find('.') == string::npos) {
			identifier = state.mIdentifierPrefix + identifier;
		}
		if (identifier.find('.') == string::npos && isAdhoc() && rep->explicitIdentifier().empty()) {
			// Only add a unique name to the end of the identifier if its adhoc AND did not have an explicit
			// identifier provided by the diskrep, like from a bundle ID in an Info.plist.
			identifier = identifier + "-" + uniqueName();
		}
		secinfo("signer", "using default identifier=%s", identifier.c_str());
	} else {
		secinfo("signer", "using explicit identifier=%s", identifier.c_str());
	}

	teamID = state.mTeamID;
	if (teamID.empty() && (inherit & kSecCodeSignerPreserveTeamIdentifier)) {
		const char *c_id = code->teamID();
		if (c_id) {
			teamID = c_id;
		}
	}
	
	// Digest algorithms: explicit or preserved. Subject to diskRep defaults or final default later.
	hashAlgorithms = state.mDigestAlgorithms;
	if (hashAlgorithms.empty() && (inherit & kSecCodeSignerPreserveDigestAlgorithm)) {
		hashAlgorithms = code->hashAlgorithms();
	}
	
	entitlements = state.mEntitlementData;
	if (!entitlements && (inherit & kSecCodeSignerPreserveEntitlements)) {
		entitlements = code->component(cdEntitlementSlot);
	}
	
	// Setup/Inherit launch constraints
	launchConstraints = state.mLaunchConstraints;
	// The copy constructor for CFRef does not retain the reference but launchConstraints will get destructed
	// before state.mLaunchConstraints, so make sure we retain any references we do find in the array.
	for (auto& ref : launchConstraints) {
		ref.retain();
	}
	if ((inherit & kSecCodeSignerPreserveLaunchConstraints) &&
		!launchConstraints[0] &&
		!launchConstraints[1] &&
		!launchConstraints[2]) {
		CFDataRef lwcr = code->component(cdLaunchConstraintSelf);
		if (lwcr) {
			launchConstraints[0] = lwcr;
		}
		lwcr = code->component(cdLaunchConstraintParent);
		if (lwcr) {
			launchConstraints[1] = lwcr;
		}
		lwcr = code->component(cdLaunchConstraintResponsible);
		if (lwcr) {
			launchConstraints[2] = lwcr;
		}
	}
	libraryConstraints = state.mLibraryConstraints;
	if ((inherit & kSecCodeSignerPreserveLibraryConstraints) && !libraryConstraints) {
		CFDataRef lwcr = code->component(cdLibraryConstraint);
		if (lwcr) {
			libraryConstraints = lwcr;
		}
	}
	
	// work out the CodeDirectory flags word
	bool haveCdFlags = false;
	cdFlags = 0;
	if (!haveCdFlags && state.mCdFlagsGiven) {
		cdFlags = state.mCdFlags;
		secinfo("signer", "using explicit cdFlags=0x%x", cdFlags);
		haveCdFlags = true;
	}
	if (!haveCdFlags && infoDict) {
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
	if (!haveCdFlags && (inherit & kSecCodeSignerPreserveRuntime)) {
		cdFlags |= code->codeDirectory(false)->flags & kSecCodeSignatureRuntime;
	}
	if (!haveCdFlags && (inherit & kSecCodeSignerPreserveFlags)) {
		cdFlags = code->codeDirectory(false)->flags & ~kSecCodeSignatureAdhoc;
		secinfo("signer", "using inherited cdFlags=0x%x", cdFlags);
	}
	if ((state.mSigner == SecIdentityRef(kCFNull)) &&
		!state.mOmitAdhocFlag) { // ad-hoc signing requested...
		cdFlags |= kSecCodeSignatureAdhoc;	// ... so note that
	}

	// prepare the internal requirements input
	if (state.mRequirements) {
		if (CFGetTypeID(state.mRequirements) == CFDataGetTypeID()) {		// binary form
			const Requirements *rp = (const Requirements *)CFDataGetBytePtr(state.mRequirements.as<CFDataRef>());
			if (!rp->validateBlob()) {
				MacOSError::throwMe(errSecCSReqInvalid);
			}
			requirements = rp->clone();
		} else if (CFGetTypeID(state.mRequirements) == CFStringGetTypeID()) { // text form
			CFRef<CFMutableStringRef> reqText = CFStringCreateMutableCopy(NULL, 0, state.mRequirements.as<CFStringRef>());
			// substitute $ variable tokens
			CFRange range = { 0, CFStringGetLength(reqText) };
			CFStringFindAndReplace(reqText, CFSTR("$self.identifier"), CFTempString(identifier), range, 0);
			requirements = parseRequirements(cfString(reqText));
		} else {
			MacOSError::throwMe(errSecCSInvalidObjectRef);
		}
	} else if (inherit & kSecCodeSignerPreserveRequirements) {
		if (const Requirements *rp = code->internalRequirements()) {
			requirements = rp->clone();
		}
	}
	
	// prepare the resource directory, if any
	string rpath = rep->resourcesRootPath();
	string rrpath;
	CFCopyRef<CFDictionaryRef> resourceRules;
	if (!rpath.empty()) {
		// explicitly given resource rules always win
		resourceRules = state.mResourceRules;
		
		// inherited rules come next (overriding embedded ones!)
		if (!resourceRules && (inherit & kSecCodeSignerPreserveResourceRules)) {
			if (CFDictionaryRef oldRules = code->resourceDictionary(false)) {
				resourceRules = oldRules;
			}
		}
		
		// embedded resource rules come next
		if (!resourceRules && infoDict) {
			if (CFTypeRef spec = CFDictionaryGetValue(infoDict, _kCFBundleResourceSpecificationKey)) {
				if (CFGetTypeID(spec) == CFStringGetTypeID()) {
					if (CFRef<CFDataRef> data = cfLoadFile(rpath + "/" + cfString(CFStringRef(spec)))) {
						if (CFDictionaryRef dict = makeCFDictionaryFrom(data)) {
							resourceRules.take(dict);
						}
					}
				}
				if (!resourceRules)	{ // embedded rules present but unacceptable
					MacOSError::throwMe(errSecCSResourceRulesInvalid);
				}
			}
		}
		
		// if we got one from anywhere (but the defaults), check it
		if (resourceRules) {
			CFTypeRef rules = CFDictionaryGetValue(resourceRules, CFSTR("rules"));
			if (!rules || CFGetTypeID(rules) != CFDictionaryGetTypeID()) {
				MacOSError::throwMe(errSecCSResourceRulesInvalid);
			}
		}

		// finally, ask the DiskRep for its default
		if (!resourceRules) {
			resourceRules.take(rep->defaultResourceRules(*this));
		}

		// resource root can optionally be the canonical bundle path,
		// but sealed resource paths are always relative to rpath
		rrpath = rpath;
		if (signingFlags() & kSecCSSignBundleRoot) {
			rrpath = cfStringRelease(rep->copyCanonicalPath());
		}
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

	if (inherit & kSecCodeSignerPreservePEH) {
		/* We need at least one architecture in all cases because we index our
		 * PreEncryptionMaps by architecture. However, only machOs have any
		 * architecture at all, for generic targets there will just be one
		 * PreEncryptionHashMap.
		 * So if the main executable is not a machO, we just choose the local
		 * (signer's) main architecture as dummy value for the first element in our pair. */
		preEncryptMainArch = (code->diskRep()->mainExecutableIsMachO() ?
							  code->diskRep()->mainExecutableImage()->bestNativeArch() :
							   Architecture::local());

		addPreEncryptHashes(preEncryptHashMaps[preEncryptMainArch], code);
		
		code->handleOtherArchitectures(^(Security::CodeSigning::SecStaticCode *subcode) {
			Universal *fat = subcode->diskRep()->mainExecutableImage();
			assert(fat && fat->narrowed());	// handleOtherArchitectures gave us a focused architecture slice.
			Architecture arch = fat->bestNativeArch();	// actually, only architecture for this slice.
			addPreEncryptHashes(preEncryptHashMaps[arch], subcode);
		});
	}

	if (inherit & kSecCodeSignerPreserveRuntime) {
		/* We need at least one architecture in all cases because we index our
		 * RuntimeVersionMaps by architecture. However, only machOs have any
		 * architecture at all, for generic targets there will just be one
		 * RuntimeVersionMap.
		 * So if the main executable is not a machO, we just choose the local
		 * (signer's) main architecture as dummy value for the first element in our pair. */
		runtimeVersionMainArch = (code->diskRep()->mainExecutableIsMachO() ?
							  code->diskRep()->mainExecutableImage()->bestNativeArch() :
							  Architecture::local());

		addRuntimeVersions(runtimeVersionMap[runtimeVersionMainArch], code);

		code->handleOtherArchitectures(^(Security::CodeSigning::SecStaticCode *subcode) {
			Universal *fat = subcode->diskRep()->mainExecutableImage();
			assert(fat && fat->narrowed());	// handleOtherArchitectures gave us a focused architecture slice.
			Architecture arch = fat->bestNativeArch();	// actually, only architecture for this slice.
			addRuntimeVersions(runtimeVersionMap[arch], subcode);
		});
	}
}

void SecCodeSigner::Signer::addPreEncryptHashes(PreEncryptHashMap &map, SecStaticCode const *code) {
	SecStaticCode::CodeDirectoryMap const *cds = code->codeDirectories();
	
	if (cds != NULL) {
		for(auto const& pair : *cds) {
			CodeDirectory::HashAlgorithm const alg = pair.first;
			CFDataRef const cddata = pair.second;
			
			CodeDirectory const * cd =
			reinterpret_cast<const CodeDirectory *>(CFDataGetBytePtr(cddata));
			if (cd->preEncryptHashes() != NULL) {
				CFRef<CFDataRef> preEncrypt = makeCFData(cd->preEncryptHashes(),
														 cd->nCodeSlots * cd->hashSize);
				map[alg] = preEncrypt;
			}
		}
	}
}

void SecCodeSigner::Signer::addRuntimeVersions(RuntimeVersionMap &map, const SecStaticCode *code)
{
	SecStaticCode::CodeDirectoryMap const *cds = code->codeDirectories();

	if (cds != NULL) {
		for(auto const& pair : *cds) {
			CodeDirectory::HashAlgorithm const alg = pair.first;
			CFDataRef const cddata = pair.second;

			CodeDirectory const * cd =
			reinterpret_cast<const CodeDirectory *>(CFDataGetBytePtr(cddata));
			if (cd->runtimeVersion()) {
				map[alg] = cd->runtimeVersion();
			}
		}
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
		if (signingFlags() & kSecCSSingleThreadedSigning) {
			this->state.mLimitedAsync = new LimitedAsync(false);
		} else {
			this->state.mLimitedAsync = new LimitedAsync(rep->fd().mediumType() == kIOPropertyMediumTypeSolidStateKey);
		}
	}

	CFDictionaryRef files2 = NULL;
	if (!(signingFlags() & kSecCSSignV1)) {
		CFCopyRef<CFDictionaryRef> rules2 = cfget<CFDictionaryRef>(rulesDict, "rules2");
		if (!rules2) {
			// Clone V1 rules and add default nesting rules at weight 0 (overridden by anything in rules,
			// because the default weight, according to ResourceBuilder::addRule(), is 1).
			// V1 rules typically do not cover these places so we'll prevail, but if they do, we defer to them.
			rules2.take(cfmake<CFDictionaryRef>("{+%O"
				"'^(Frameworks|SharedFrameworks|PlugIns|Plug-ins|XPCServices|Helpers|MacOS|Library/(Automator|Spotlight|LoginItems))/' = {nested=#T, weight=0}" // exclude dynamic repositories
			"}", rules));
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
			bool isNested = (ruleFlags & ResourceBuilder::nested);
			const std::string path(ent->fts_path);
			const std::string accpath(ent->fts_accpath);
			
			// We want to transfer files into the perform block, so this is equivalent to CFRetain(filesRef)
			CFRef<CFMutableDictionaryRef> filesTransfer;
			filesTransfer = filesRef;
			filesTransfer.yield();
			
			this->state.mLimitedAsync->perform(groupRef, ^{
				CFRef<CFMutableDictionaryRef> localFiles = filesRef;
				
				CFRef<CFMutableDictionaryRef> seal;
				if (isNested) {
					seal.take(signNested(path, relpath));
				} else if (isSymlink) {
					char target[PATH_MAX];
					ssize_t len = ::readlink(accpath.c_str(), target, sizeof(target)-1);
					if (len < 0)
						UnixError::check(-1);
					target[len] = '\0';
					seal.take(cfmake<CFMutableDictionaryRef>("{symlink=%s}", target));
				} else {
					if (signingFlags() & kSecCSStripDisallowedXattrs) {
						UnixPlusPlus::AutoFileDesc fd(path);
						if (fd.hasExtendedAttribute(XATTR_RESOURCEFORK_NAME)) {
							fd.removeAttr(XATTR_RESOURCEFORK_NAME);
						}
						if (fd.hasExtendedAttribute(XATTR_FINDERINFO_NAME)) {
							fd.removeAttr(XATTR_FINDERINFO_NAME);
						}
					}
					seal.take(resources.hashFile(accpath.c_str(), digestAlgorithms(), signingFlags() & kSecCSSignStrictPreflight));
				}
				if (seal.get() == NULL) {
					secerror("Failed to generate sealed resource: %d, %d, %s", isNested, isSymlink, accpath.c_str());
					MacOSError::throwMe(errSecCSBadResource);
				}
				if (ruleFlags & ResourceBuilder::optional)
					CFDictionaryAddValue(seal, CFSTR("optional"), kCFBooleanTrue);
				CFTypeRef hash;
				StLock<Mutex> _(resourceLock);
				if ((hash = CFDictionaryGetValue(seal, CFSTR("hash"))) && CFDictionaryGetCount(seal) == 1) // simple form
					CFDictionaryAddValue(localFiles, CFTempString(relpath).get(), hash);
				else
					CFDictionaryAddValue(localFiles, CFTempString(relpath).get(), seal.get());
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
				// The user controlled rule flag is a runtime only flag and shouldn't cause use of a more
				// complex resource serialization as doing so would break serialized adhoc opaque hashes.
				if ((ruleFlags & ~ResourceBuilder::user_controlled) == 0) {	// default case - plain hash
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
		CSError::throwMe(err.osStatus(), kSecCFErrorPath, CFTempURL(path));
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

	if (state.mPreserveAFSC)
		writer->setPreserveAFSC(state.mPreserveAFSC);

	unique_ptr<ArchEditor> editor(state.mDetached
		? static_cast<ArchEditor *>(new BlobEditor(*fat, *this))
		: new MachOEditor(writer, *fat, this->digestAlgorithms(), rep->mainExecutablePath()));
	assert(editor->count() > 0);

	if (!editor->attribute(writerNoGlobal))	{ // can store architecture-common components
		populate(*editor);
	}
	
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
		bool mainBinary = arch.source.get()->type() == MH_EXECUTE;

		uint32_t runtimeVersion = 0;
		if (cdFlags & kSecCodeSignatureRuntime) {
			runtimeVersion = state.mRuntimeVersionOverride ? state.mRuntimeVersionOverride : arch.source.get()->sdkVersion();
		}

		arch.ireqs(requirements, rep->defaultRequirements(&arch.architecture, *this), context);
		if (editor->attribute(writerNoGlobal))	// can't store globally, add per-arch
			populate(arch);
		for (auto type = digestAlgorithms().begin(); type != digestAlgorithms().end(); ++type) {
			uint32_t runtimeVersionToUse = runtimeVersion;
			if ((cdFlags & kSecCodeSignatureRuntime) && runtimeVersionMap.count(arch.architecture)) {
				if (runtimeVersionMap[arch.architecture].count(*type)) {
					runtimeVersionToUse = runtimeVersionMap[arch.architecture][*type];
				}
			}
			arch.eachDigest(^(CodeDirectory::Builder& builder) {
				populate(builder, arch, arch.ireqs,
						 arch.source->offset(), arch.source->signingExtent(),
						 mainBinary, rep->execSegBase(&(arch.architecture)), rep->execSegLimit(&(arch.architecture)),
						 unsigned(digestAlgorithms().size()-1),
						 preEncryptHashMaps[arch.architecture], runtimeVersionToUse, true);
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

		CFRef<CFDictionaryRef> hashDict = cdSet.hashDict();
		CFRef<CFArrayRef> hashList = cdSet.hashList();
		CFRef<CFDataRef> signature = signCodeDirectory(cdSet.primary(), hashDict, hashList);
		
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

	if (state.mPreserveAFSC)
		writer->setPreserveAFSC(state.mPreserveAFSC);

	CodeDirectorySet cdSet;

	for (auto type = digestAlgorithms().begin(); type != digestAlgorithms().end(); ++type) {
		CodeDirectory::Builder builder(*type);
		InternalRequirements ireqs;
		ireqs(requirements, rep->defaultRequirements(NULL, *this), context);
		populate(*writer);
		populate(builder, *writer, ireqs, rep->signingBase(), rep->signingLimit(),
				 false,		// only machOs can currently be main binaries
				 rep->execSegBase(NULL), rep->execSegLimit(NULL),
				 unsigned(digestAlgorithms().size()-1),
				 preEncryptHashMaps[preEncryptMainArch], // Only one map, the default.
				 (cdFlags & kSecCodeSignatureRuntime) ? state.mRuntimeVersionOverride : 0,
				 true);
		
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

	CFRef<CFDictionaryRef> hashDict = cdSet.hashDict();
	CFRef<CFArrayRef> hashList = cdSet.hashList();
	CFRef<CFDataRef> signature = signCodeDirectory(cdSet.primary(), hashDict, hashList);
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
									 InternalRequirements &ireqs, size_t offset, size_t length,
									 bool mainBinary, size_t execSegBase, size_t execSegLimit,
									 unsigned alternateDigestCount,
									 PreEncryptHashMap const &preEncryptHashMap,
									 uint32_t runtimeVersion, bool generateEntitlementDER)
{
	// fill the CodeDirectory
	builder.executable(rep->mainExecutablePath(), pagesize, offset, length);
	builder.flags(cdFlags);
	builder.identifier(identifier);
	builder.teamID(teamID);
	builder.platform(state.mPlatform);
	builder.execSeg(execSegBase, execSegLimit, mainBinary ? kSecCodeExecSegMainBinary : 0);
	builder.generatePreEncryptHashes(signingFlags() & kSecCSSignGeneratePEH);
	builder.preservePreEncryptHashMap(preEncryptHashMap);
	builder.runTimeVersion(runtimeVersion);

	if (CFRef<CFDataRef> data = rep->component(cdInfoSlot)) {
		builder.specialSlot(cdInfoSlot, data);
	}
	if (ireqs) {
		CFRef<CFDataRef> data = makeCFData(*ireqs);
		writer.component(cdRequirementsSlot, data);
		builder.specialSlot(cdRequirementsSlot, data);
	}
	if (resourceDirectory) {
		builder.specialSlot(cdResourceDirSlot, resourceDictData);
	}
	
	int lwcrSlot = cdLaunchConstraintSelf;
	for (auto& lwcr : launchConstraints) {
		if (lwcr) {
			writer.component(lwcrSlot, lwcr);
			builder.specialSlot(lwcrSlot, lwcr);
		}
		lwcrSlot++;
	}
	if (libraryConstraints) {
		writer.component(cdLibraryConstraint, libraryConstraints);
		builder.specialSlot(cdLibraryConstraint, libraryConstraints);
	}
	
	if (entitlements) {

		if (mainBinary || state.mForceLibraryEntitlements) {
			writer.component(cdEntitlementSlot, entitlements);
			builder.specialSlot(cdEntitlementSlot, entitlements);
			
			CFRef<CFDataRef> entitlementDER;
			uint64_t execSegFlags = 0;
			cookEntitlements(entitlements, generateEntitlementDER,
							 &execSegFlags, &entitlementDER.aref());

			if (generateEntitlementDER) {
				writer.component(cdEntitlementDERSlot, entitlementDER);
				builder.specialSlot(cdEntitlementDERSlot, entitlementDER);
			}

			builder.addExecSegFlags(execSegFlags);
		}
	}
	if (CFRef<CFDataRef> repSpecific = rep->component(cdRepSpecificSlot)) {
		builder.specialSlot(cdRepSpecificSlot, repSpecific);
	}
	
	writer.addDiscretionary(builder);
}

#if TARGET_OS_OSX
// Secure timestamps are only supported when signing on macOS.
#include <security_smime/tsaSupport.h>
#endif

//
// Generate the CMS signature for a (finished) CodeDirectory.
//
CFDataRef
SecCodeSigner::Signer::signCodeDirectory(const CodeDirectory *cd,
										 CFDictionaryRef hashDict,
										 CFArrayRef hashList)
{
	if (useRemoteSigning) {
		return signCodeDirectoryRemote(cd, hashDict, hashList);
	} else {
		return signCodeDirectoryWithIdentity(cd, hashDict, hashList);
	}
}

CFDataRef
SecCodeSigner::Signer::signCodeDirectoryWithIdentity(const CodeDirectory *cd,
													 CFDictionaryRef hashDict,
													 CFArrayRef hashList)
{
	assert(state.mSigner);
	CFRef<CFMutableDictionaryRef> defaultTSContext = NULL;

	// a null signer generates a null signature blob
	if (state.mSigner == SecIdentityRef(kCFNull))
		return CFDataCreate(NULL, NULL, 0);

	// generate CMS signature
	CFRef<CMSEncoderRef> cms;
	MacOSError::check(CMSEncoderCreate(&cms.aref()));
	MacOSError::check(CMSEncoderSetCertificateChainMode(cms, kCMSCertificateChainWithRootOrFail));
	CMSEncoderAddSigners(cms, state.mSigner);
	CMSEncoderSetSignerAlgorithm(cms, kCMSEncoderDigestAlgorithmSHA256);
	MacOSError::check(CMSEncoderSetHasDetachedContent(cms, true));

	if (emitSigningTime) {
		MacOSError::check(CMSEncoderAddSignedAttributes(cms, kCMSAttrSigningTime));
		CFAbsoluteTime time = signingTime ? signingTime : CFAbsoluteTimeGetCurrent();
		MacOSError::check(CMSEncoderSetSigningTime(cms, time));
	}

	if (hashDict != NULL) {
		assert(hashList != NULL);

		// V2 Hash Agility

		MacOSError::check(CMSEncoderAddSignedAttributes(cms, kCMSAttrAppleCodesigningHashAgilityV2));
		MacOSError::check(CMSEncoderSetAppleCodesigningHashAgilityV2(cms, hashDict));

		// V1 Hash Agility

		CFTemp<CFDictionaryRef> hashDict("{cdhashes=%O}", hashList);
		CFRef<CFDataRef> hashAgilityV1Attribute = makeCFData(hashDict.get());

		MacOSError::check(CMSEncoderAddSignedAttributes(cms, kCMSAttrAppleCodesigningHashAgility));
		MacOSError::check(CMSEncoderSetAppleCodesigningHashAgility(cms, hashAgilityV1Attribute));
	}

	MacOSError::check(CMSEncoderUpdateContent(cms, cd, cd->length()));

	// Set up to call Timestamp server if requested
	if (state.mWantTimeStamp) {
#if !TARGET_OS_OSX
		secerror("Platform does not support signing secure timestamps");
		MacOSError::throwMe(errSecUnimplemented);
#else
		CFRef<CFErrorRef> error = NULL;
		defaultTSContext = SecCmsTSAGetDefaultContext(&error.aref());
		if (error) {
			MacOSError::throwMe(errSecDataNotAvailable);
		}

		if (state.mNoTimeStampCerts || state.mTimestampService) {
			if (state.mTimestampService) {
				CFDictionarySetValue(defaultTSContext, kTSAContextKeyURL, state.mTimestampService);
			}
			if (state.mNoTimeStampCerts) {
				CFDictionarySetValue(defaultTSContext, kTSAContextKeyNoCerts, kCFBooleanTrue);
			}
		}

		CmsMessageSetTSAContext(cms, defaultTSContext);
#endif /* !TARGET_OS_OSX */
	}

	CFDataRef signature;
	MacOSError::check(CMSEncoderCopyEncodedContent(cms, &signature));

	return signature;
}

CFDataRef
SecCodeSigner::Signer::signCodeDirectoryRemote(const CodeDirectory *cd,
											   CFDictionaryRef hashDict,
											   CFArrayRef hashList)
{
	CFAbsoluteTime time = 0;
	if (emitSigningTime) {
		time = signingTime ? signingTime : CFAbsoluteTimeGetCurrent();
	}

	CFRef<CFDataRef> signature;
	MacOSError::check(doRemoteSigning(cd, hashDict, hashList, time, rsCertChain, rsHandler, signature.take()));
	return signature.yield();
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

bool SecCodeSigner::Signer::booleanEntitlement(CFDictionaryRef entDict, CFStringRef key) {
	CFBooleanRef entValue = (CFBooleanRef)CFDictionaryGetValue(entDict, key);

	if (entValue == NULL || CFGetTypeID(entValue) != CFBooleanGetTypeID()) {
		return false;
	}

	return CFBooleanGetValue(entValue);
}

void SecCodeSigner::Signer::cookEntitlements(CFDataRef entitlements, bool generateDER,
											 uint64_t *execSegFlags, CFDataRef *entitlementDER)
{
	if (!entitlements) {
		return; // nothing to do.
	}

	try {
		const EntitlementBlob *blob = reinterpret_cast<const EntitlementBlob *>(CFDataGetBytePtr(entitlements));

		if (blob == NULL || !blob->validateBlob(CFDataGetLength(entitlements))) {
			MacOSError::throwMe(errSecCSInvalidEntitlements);
		}

		CFRef<CFDictionaryRef> entDict = blob->entitlements();

		if (generateDER) {
			CFRef<CFErrorRef> error = NULL;

			CFDataRef serializedDER = NULL;
			if (CESerializeCFDictionary(CECRuntime, entDict.get(), &serializedDER) != kCENoError) {
				secerror("Serializing DER entitlements failed");
				MacOSError::throwMe(errSecCSInvalidEntitlements);
				
			}
			
			*entitlementDER = EntitlementDERBlob::blobify(serializedDER);
			CFRelease(serializedDER);
		}

		if (execSegFlags != NULL) {
			uint64_t flags = 0;

			flags |= booleanEntitlement(entDict, CFSTR("get-task-allow")) ? kSecCodeExecSegAllowUnsigned : 0;
			flags |= booleanEntitlement(entDict, CFSTR("run-unsigned-code")) ? kSecCodeExecSegAllowUnsigned : 0;
			flags |= booleanEntitlement(entDict, CFSTR("com.apple.private.cs.debugger")) ? kSecCodeExecSegDebugger : 0;
			flags |= booleanEntitlement(entDict, CFSTR("dynamic-codesigning")) ? kSecCodeExecSegJit : 0;
			flags |= booleanEntitlement(entDict, CFSTR("com.apple.private.skip-library-validation")) ? kSecCodeExecSegSkipLibraryVal : 0;
			flags |= booleanEntitlement(entDict, CFSTR("com.apple.private.amfi.can-load-cdhash")) ? kSecCodeExecSegCanLoadCdHash : 0;
			flags |= booleanEntitlement(entDict, CFSTR("com.apple.private.amfi.can-execute-cdhash")) ? kSecCodeExecSegCanExecCdHash : 0;

			*execSegFlags = flags;
		}

	} catch (const CommonError &err) {
		secwarning("failed to parse entitlements: %s", err.what());
		if (generateDER) {
			throw;
		}
	}
}

//// Signature Editing
	
void SecCodeSigner::Signer::edit(SecCSFlags flags)
{
	rep = code->diskRep()->base();
	
	Universal *fat = state.mNoMachO ? NULL : rep->mainExecutableImage();
	
	prepareForEdit(flags);
	
	if (fat != NULL) {
		editMachO(fat);
	} else {
		editArchitectureAgnostic();
	}
}

EditableDiskRep *SecCodeSigner::Signer::editMainExecutableRep(DiskRep *rep)
{
	EditableDiskRep *mainExecRep = NULL;
	BundleDiskRep *bundleDiskRep = dynamic_cast<BundleDiskRep*>(rep);
	
	if (bundleDiskRep) {
		mainExecRep = dynamic_cast<EditableDiskRep*>(bundleDiskRep->mainExecRep());
	}
	
	return mainExecRep;
}
	

void SecCodeSigner::Signer::prepareForEdit(SecCSFlags flags) {
	setDigestAlgorithms(code->hashAlgorithms());
	
	Universal *machO = (code->diskRep()->mainExecutableIsMachO() ?
						code->diskRep()->mainExecutableImage() : NULL);
	
	/* We need at least one architecture in all cases because we index our
	 * RawComponentMaps by architecture. However, only machOs have any
	 * architecture at all, for generic targets there will just be one
	 * RawComponentMap.
	 * So if the main executable is not a machO, we just choose the local
	 * (signer's) main architecture as dummy value for the first element in our pair. */
	editMainArch = (machO != NULL ? machO->bestNativeArch() : Architecture::local());

	if (machO != NULL) {
		if (machO->narrowed()) {
			/* --arch gives us a narrowed SecStaticCode, but because
			 * codesign_allocate always creates or replaces signatures
			 * for all slices, we must operate on the universal
			 * SecStaticCode. Instead, we provide --edit-arch to specify
			 * which slices to edit, the others have their code signature
			 * copied without modifications.
			 */
			MacOSError::throwMe(errSecCSNotSupported,
								"Signature editing must be performed on universal binary instead of narrow slice (using --edit-arch instead of --arch).");
		}

		if (state.mEditArch && !machO->isUniversal()) {
			MacOSError::throwMe(errSecCSInvalidFlags,
								"--edit-arch is only valid for universal binaries.");
		}
		
		if (state.mEditCMS && machO->isUniversal() && !state.mEditArch) {
			/* Each slice has its own distinct code signature,
			 * so a CMS blob is only valid for its one slice.
			 * Therefore, replacing all CMS blobs in all slices
			 * with the same blob is rather nonsensical, and we refuse.
			 *
			 * (Universal binaries with only one slice can exist,
			 * and in that case the slice to operate on would be
			 * umambiguous, but we are not treating those binaries
			 * specially and still want --edit-arch for consistency.)
			 */
			MacOSError::throwMe(errSecCSNotSupported,
								"CMS editing must be performed on specific slice (specified with --edit-arch).");
		}
	}
	
	void (^editArch)(SecStaticCode *code, Architecture arch) =
	^(SecStaticCode *code, Architecture arch) {
		EditableDiskRep *editRep = dynamic_cast<EditableDiskRep *>(code->diskRep());
		
		if (editRep == NULL) {
			MacOSError::throwMe(errSecCSNotSupported,
								"Signature editing not supported for code of this type.");
		}
		
		EditableDiskRep *mainExecRep = editMainExecutableRep(code->diskRep());
		
		if (mainExecRep != NULL) {
			// Delegate editing to the main executable if it is an EditableDiskRep.
			//(Which is the case for machOs.)
			editRep = mainExecRep;
		}

		editComponents[arch] = std::make_unique<RawComponentMap>(editRep->createRawComponents());
		
		bool archMatch = (arch.cpuType() == state.mEditArch.cpuType()) && (arch.cpuSubtype() == state.mEditArch.cpuSubtype());
		if (!state.mEditArch || archMatch) {
			if (state.mEditCMS) {
				CFDataRef cms = state.mEditCMS.get();
				(*editComponents[arch])[cdSignatureSlot] = cms;
			}
		}
	};
	
	editArch(code, editMainArch);
	
	code->handleOtherArchitectures(^(Security::CodeSigning::SecStaticCode *subcode) {
		Universal *fat = subcode->diskRep()->mainExecutableImage();
		assert(fat && fat->narrowed());	// handleOtherArchitectures gave us a focused architecture slice.
		Architecture arch = fat->bestNativeArch();	// actually, only architecture for this slice.
		editArch(subcode, arch);
	});
	
	/* The resource dictionary is special, because it is
	 * considered "global" instead of per architecture.
	 * For editing, that means it's usually not embedded
	 * in the main executable's signature if it exists,
	 * but in the containing disk rep (e.g. the
	 * CodeResources file if the rep is a Bundle).
	 */
	resourceDictData = rep->component(cdResourceDirSlot);
}
	
void SecCodeSigner::Signer::editMachO(Universal *fat) {
	// Mach-O executable at the core - perform multi-architecture signature editing
	RefPointer<DiskRep::Writer> writer = rep->writer();

	if (state.mPreserveAFSC)
		writer->setPreserveAFSC(state.mPreserveAFSC);
	
	unique_ptr<ArchEditor> editor(new MachOEditor(writer, *fat,
												  this->digestAlgorithms(),
												  rep->mainExecutablePath()));
	assert(editor->count() > 0);
	
	if (resourceDictData && !editor->attribute(writerNoGlobal)) {
		// For when the resource dict is "global", e.g. for bundles.
		editor->component(cdResourceDirSlot, resourceDictData);
	}
	
	for (MachOEditor::Iterator it = editor->begin(); it != editor->end(); ++it) {
		MachOEditor::Arch &arch = *it->second;
		arch.source.reset(fat->architecture(it->first)); // transfer ownership
		
		if (resourceDictData && editor->attribute(writerNoGlobal)) {
			// Technically possible to embed a resource dict in the embedded sig.
			arch.component(cdResourceDirSlot, resourceDictData);
		}
		
		for (auto const &entry : *editComponents[arch.architecture]) {
			CodeDirectory::Slot slot = entry.first;
			CFDataRef data = entry.second.get();
			arch.component(slot, data);
		}
		
		/* We must preserve the original superblob's size, as the size is
		 * also in the macho's load commands, which are itself covered
		 * by the signature. */
		arch.blobSize = arch.source->signingLength();
	}
	
	editor->allocate();
	
	for (MachOEditor::Iterator it = editor->begin(); it != editor->end(); ++it) {
		MachOEditor::Arch &arch = *it->second;
		editor->reset(arch);
		
		if (!state.mDryRun) {
			EmbeddedSignatureBlob *blob = arch.make();
			editor->write(arch, blob);	// takes ownership of blob
		}
	}
	
	if (!state.mDryRun) {
		editor->commit();
	}

}

void SecCodeSigner::Signer::editArchitectureAgnostic()
{
	if (state.mDryRun) {
		return;

	}
	// non-Mach-O executable - single-instance signature editing
	RefPointer<DiskRep::Writer> writer = rep->writer();

	if (state.mPreserveAFSC)
		writer->setPreserveAFSC(state.mPreserveAFSC);
	
	for (auto const &entry : *editComponents[editMainArch]) {
		CodeDirectory::Slot slot = entry.first;
		CFDataRef data = entry.second.get();
		
		writer->component(slot, data);
	}

	// commit to storage
	writer->flush();
}

void
SecCodeSigner::Signer::setupRemoteSigning(CFArrayRef certChain, SecCodeRemoteSignHandler handler)
{
	secinfo("signer", "configuring remote signing with cert chain: %@", certChain);
	useRemoteSigning = true;
	rsHandler = handler;
	rsCertChain = certChain;
}

} // end namespace CodeSigning
} // end namespace Security
