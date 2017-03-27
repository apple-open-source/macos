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
#include "bundlediskrep.h"
#include "filediskrep.h"
#include "dirscanner.h"
#include <CoreFoundation/CFBundlePriv.h>
#include <CoreFoundation/CFURLAccess.h>
#include <CoreFoundation/CFBundlePriv.h>
#include <security_utilities/cfmunge.h>
#include <copyfile.h>
#include <fts.h>
#include <sstream>

namespace Security {
namespace CodeSigning {

using namespace UnixPlusPlus;


//
// Local helpers
//
static std::string findDistFile(const std::string &directory);


//
// We make a CFBundleRef immediately, but everything else is lazy
//
BundleDiskRep::BundleDiskRep(const char *path, const Context *ctx)
	: mBundle(_CFBundleCreateUnique(NULL, CFTempURL(path)))
{
	if (!mBundle)
		MacOSError::throwMe(errSecCSBadBundleFormat);
	setup(ctx);
	CODESIGN_DISKREP_CREATE_BUNDLE_PATH(this, (char*)path, (void*)ctx, mExecRep);
}

BundleDiskRep::BundleDiskRep(CFBundleRef ref, const Context *ctx)
{
	mBundle = ref;		// retains
	setup(ctx);
	CODESIGN_DISKREP_CREATE_BUNDLE_REF(this, ref, (void*)ctx, mExecRep);
}

BundleDiskRep::~BundleDiskRep()
{
}
	
void BundleDiskRep::checkMoved(CFURLRef oldPath, CFURLRef newPath)
{
	char cOld[PATH_MAX];
	char cNew[PATH_MAX];
	// The realpath call is important because alot of Framework bundles have a symlink
	// to their "Current" version binary in the main bundle
	if (realpath(cfString(oldPath).c_str(), cOld) == NULL ||
		realpath(cfString(newPath).c_str(), cNew) == NULL)
		MacOSError::throwMe(errSecCSAmbiguousBundleFormat);
	
	if (strcmp(cOld, cNew) != 0)
		recordStrictError(errSecCSAmbiguousBundleFormat);
}

// common construction code
void BundleDiskRep::setup(const Context *ctx)
{
	mComponentsFromExecValid = false; // not yet known
	mInstallerPackage = false;	// default
	mAppLike = false;			// pessimism first
	bool appDisqualified = false; // found reason to disqualify as app
	
	// capture the path of the main executable before descending into a specific version
	CFRef<CFURLRef> mainExecBefore = CFBundleCopyExecutableURL(mBundle);
	CFRef<CFURLRef> infoPlistBefore = _CFBundleCopyInfoPlistURL(mBundle);

	// validate the bundle root; fish around for the desired framework version
	string root = cfStringRelease(copyCanonicalPath());
	if (filehasExtendedAttribute(root, XATTR_FINDERINFO_NAME))
		recordStrictError(errSecCSInvalidAssociatedFileData);
	string contents = root + "/Contents";
	string supportFiles = root + "/Support Files";
	string version = root + "/Versions/"
		+ ((ctx && ctx->version) ? ctx->version : "Current")
		+ "/.";
	if (::access(contents.c_str(), F_OK) == 0) {	// not shallow
		DirValidator val;
		val.require("^Contents$", DirValidator::directory);	 // duh
		val.allow("^(\\.LSOverride|\\.DS_Store|Icon\r|\\.SoftwareDepot\\.tracking)$", DirValidator::file | DirValidator::noexec);
		try {
			val.validate(root, errSecCSUnsealedAppRoot);
		} catch (const MacOSError &err) {
			recordStrictError(err.error);
		}
	} else if (::access(supportFiles.c_str(), F_OK) == 0) {	// ancient legacy boondoggle bundle
		// treat like a shallow bundle; do not allow Versions arbitration
		appDisqualified = true;
	} else if (::access(version.c_str(), F_OK) == 0) {	// versioned bundle
		if (CFBundleRef versionBundle = _CFBundleCreateUnique(NULL, CFTempURL(version)))
			mBundle.take(versionBundle);	// replace top bundle ref
		else
			MacOSError::throwMe(errSecCSStaticCodeNotFound);
		appDisqualified = true;
		validateFrameworkRoot(root);
	} else {
		if (ctx && ctx->version)	// explicitly specified
			MacOSError::throwMe(errSecCSStaticCodeNotFound);
	}

	CFDictionaryRef infoDict = CFBundleGetInfoDictionary(mBundle);
	assert(infoDict);	// CFBundle will always make one up for us
	CFTypeRef mainHTML = CFDictionaryGetValue(infoDict, CFSTR("MainHTML"));
	CFTypeRef packageVersion = CFDictionaryGetValue(infoDict, CFSTR("IFMajorVersion"));

	// conventional executable bundle: CFBundle identifies an executable for us
	if (CFRef<CFURLRef> mainExec = CFBundleCopyExecutableURL(mBundle))		// if CFBundle claims an executable...
		if (mainHTML == NULL) {												// ... and it's not a widget

			// Note that this check is skipped if there is a specific framework version checked.
			// That's because you know what you are doing if you are looking at a specific version.
			// This check is designed to stop someone who did a verification on an app root, from mistakenly
			// verifying a framework
			if (!ctx || !ctx->version) {
				if (mainExecBefore)
					checkMoved(mainExecBefore, mainExec);
				if (infoPlistBefore)
					if (CFRef<CFURLRef> infoDictPath = _CFBundleCopyInfoPlistURL(mBundle))
						checkMoved(infoPlistBefore, infoDictPath);
			}

			mMainExecutableURL = mainExec;
			mExecRep = DiskRep::bestFileGuess(this->mainExecutablePath(), ctx);
			checkPlainFile(mExecRep->fd(), this->mainExecutablePath());
			CFDictionaryRef infoDict = CFBundleGetInfoDictionary(mBundle);
			bool isAppBundle = false;
			if (infoDict)
				if (CFTypeRef packageType = CFDictionaryGetValue(infoDict, CFSTR("CFBundlePackageType")))
					if (CFEqual(packageType, CFSTR("APPL")))
						isAppBundle = true;
			mFormat = "bundle with " + mExecRep->format();
			if (isAppBundle)
				mFormat = "app " + mFormat;
			mAppLike = isAppBundle && !appDisqualified;
			return;
		}
	
	// widget
	if (mainHTML) {
		if (CFGetTypeID(mainHTML) != CFStringGetTypeID())
			MacOSError::throwMe(errSecCSBadBundleFormat);
		mMainExecutableURL.take(makeCFURL(cfString(CFStringRef(mainHTML)), false,
			CFRef<CFURLRef>(CFBundleCopySupportFilesDirectoryURL(mBundle))));
		if (!mMainExecutableURL)
			MacOSError::throwMe(errSecCSBadBundleFormat);
		mExecRep = new FileDiskRep(this->mainExecutablePath().c_str());
		checkPlainFile(mExecRep->fd(), this->mainExecutablePath());
		mFormat = "widget bundle";
		mAppLike = true;
		return;
	}
	
	// do we have a real Info.plist here?
	if (CFRef<CFURLRef> infoURL = _CFBundleCopyInfoPlistURL(mBundle)) {
		// focus on the Info.plist (which we know exists) as the nominal "main executable" file
		mMainExecutableURL = infoURL;
		mExecRep = new FileDiskRep(this->mainExecutablePath().c_str());
		checkPlainFile(mExecRep->fd(), this->mainExecutablePath());
		if (packageVersion) {
			mInstallerPackage = true;
			mFormat = "installer package bundle";
		} else {
			mFormat = "bundle";
		}
		return;
	}

	// we're getting desperate here. Perhaps an oldish-style installer package? Look for a *.dist file
	std::string distFile = findDistFile(this->resourcesRootPath());
	if (!distFile.empty()) {
		mMainExecutableURL = makeCFURL(distFile);
		mExecRep = new FileDiskRep(this->mainExecutablePath().c_str());
		checkPlainFile(mExecRep->fd(), this->mainExecutablePath());
		mInstallerPackage = true;
		mFormat = "installer package bundle";
		return;
	}
	
	// this bundle cannot be signed
	MacOSError::throwMe(errSecCSBadBundleFormat);
}


//
// Return the full path to the one-and-only file named something.dist in a directory.
// Return empty string if none; throw an exception if multiple. Do not descend into subdirectories.
//
static std::string findDistFile(const std::string &directory)
{
	std::string found;
	char *paths[] = {(char *)directory.c_str(), NULL};
	FTS *fts = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR | FTS_NOSTAT, NULL);
	bool root = true;
	while (FTSENT *ent = fts_read(fts)) {
		switch (ent->fts_info) {
		case FTS_F:
		case FTS_NSOK:
			if (!strcmp(ent->fts_path + ent->fts_pathlen - 5, ".dist")) {	// found plain file foo.dist
				if (found.empty())	// first found
					found = ent->fts_path;
				else				// multiple *.dist files (bad)
					MacOSError::throwMe(errSecCSBadBundleFormat);
			}
			break;
		case FTS_D:
			if (!root)
				fts_set(fts, ent, FTS_SKIP);	// don't descend
			root = false;
			break;
		default:
			break;
		}
	}
	fts_close(fts);
	return found;
}


//
// Try to create the meta-file directory in our bundle.
// Does nothing if the directory already exists.
// Throws if an error occurs.
//
void BundleDiskRep::createMeta()
{
	string meta = metaPath(NULL);
	if (!mMetaExists) {
		if (::mkdir(meta.c_str(), 0755) == 0) {
			copyfile(cfStringRelease(copyCanonicalPath()).c_str(), meta.c_str(), NULL, COPYFILE_SECURITY);
			mMetaPath = meta;
			mMetaExists = true;
		} else if (errno != EEXIST)
			UnixError::throwMe();
	}
}
	

//
// Create a path to a bundle signing resource, by name.
// This is in the BUNDLEDISKREP_DIRECTORY directory in the bundle's support directory.
//
string BundleDiskRep::metaPath(const char *name)
{
	if (mMetaPath.empty()) {
		string support = cfStringRelease(CFBundleCopySupportFilesDirectoryURL(mBundle));
		mMetaPath = support + "/" BUNDLEDISKREP_DIRECTORY;
		mMetaExists = ::access(mMetaPath.c_str(), F_OK) == 0;
	}
	if (name)
		return mMetaPath + "/" + name;
	else
		return mMetaPath;
}
	
CFDataRef BundleDiskRep::metaData(const char *name)
{
    if (CFRef<CFURLRef> url = makeCFURL(metaPath(name))) {
        return cfLoadFile(url);
    } else {
        secnotice("bundlediskrep", "no metapath for %s", name);
        return NULL;
    }
}

CFDataRef BundleDiskRep::metaData(CodeDirectory::SpecialSlot slot)
{
	if (const char *name = CodeDirectory::canonicalSlotName(slot))
		return metaData(name);
	else
		return NULL;
}


	
//
// Load's a CFURL and makes sure that it is a regular file and not a symlink (or fifo, etc.)
//
CFDataRef BundleDiskRep::loadRegularFile(CFURLRef url)
{
	assert(url);

	CFDataRef data = NULL;

	std::string path(cfString(url));

	AutoFileDesc fd(path);

	checkPlainFile(fd, path);

	data = cfLoadFile(fd, fd.fileSize());

	if (!data) {
		secinfo("bundlediskrep", "failed to load %s", cfString(url).c_str());
		MacOSError::throwMe(errSecCSInvalidSymlink);
	}

	return data;
}

//
// Load and return a component, by slot number.
// Info.plist components come from the bundle, always (we don't look
// for Mach-O embedded versions).
// ResourceDirectory always comes from bundle files.
// Everything else comes from the embedded blobs of a Mach-O image, or from
// files located in the Contents directory of the bundle; but we must be consistent
// (no half-and-half situations).
//
CFDataRef BundleDiskRep::component(CodeDirectory::SpecialSlot slot)
{
	switch (slot) {
	// the Info.plist comes from the magic CFBundle-indicated place and ONLY from there
	case cdInfoSlot:
		if (CFRef<CFURLRef> info = _CFBundleCopyInfoPlistURL(mBundle))
			return loadRegularFile(info);
		else
			return NULL;
	case cdResourceDirSlot:
		mUsedComponents.insert(slot);
		return metaData(slot);
	// by default, we take components from the executable image or files (but not both)
	default:
		if (CFRef<CFDataRef> data = mExecRep->component(slot)) {
			componentFromExec(true);
			return data.yield();
		}
		if (CFRef<CFDataRef> data = metaData(slot)) {
			componentFromExec(false);
			mUsedComponents.insert(slot);
			return data.yield();
		}
		return NULL;
	}
}


// Check that all components of this BundleDiskRep come from either the main
// executable or the _CodeSignature directory (not mix-and-match).
void BundleDiskRep::componentFromExec(bool fromExec)
{
	if (!mComponentsFromExecValid) {
		// first use; set latch
		mComponentsFromExecValid = true;
		mComponentsFromExec = fromExec;
	} else if (mComponentsFromExec != fromExec) {
		// subsequent use: check latch
		MacOSError::throwMe(errSecCSSignatureFailed);
	}
}


//
// The binary identifier is taken directly from the main executable.
//
CFDataRef BundleDiskRep::identification()
{
	return mExecRep->identification();
}


//
// Various aspects of our DiskRep personality.
//
CFURLRef BundleDiskRep::copyCanonicalPath()
{
	if (CFURLRef url = CFBundleCopyBundleURL(mBundle))
		return url;
	CFError::throwMe();
}

string BundleDiskRep::mainExecutablePath()
{
	return cfString(mMainExecutableURL);
}

string BundleDiskRep::resourcesRootPath()
{
	return cfStringRelease(CFBundleCopySupportFilesDirectoryURL(mBundle));
}

void BundleDiskRep::adjustResources(ResourceBuilder &builder)
{
	// exclude entire contents of meta directory
	builder.addExclusion("^" BUNDLEDISKREP_DIRECTORY "$");
	builder.addExclusion("^" CODERESOURCES_LINK "$");	// ancient-ish symlink into it

	// exclude the store manifest directory
	builder.addExclusion("^" STORE_RECEIPT_DIRECTORY "$");
	
	// exclude the main executable file
	string resources = resourcesRootPath();
	if (resources.compare(resources.size() - 2, 2, "/.") == 0)	// chop trailing /.
		resources = resources.substr(0, resources.size()-2);
	string executable = mainExecutablePath();
	if (!executable.compare(0, resources.length(), resources, 0, resources.length())
		&& executable[resources.length()] == '/')	// is proper directory prefix
		builder.addExclusion(string("^")
			+ ResourceBuilder::escapeRE(executable.substr(resources.length()+1)) + "$", ResourceBuilder::softTarget);
}



Universal *BundleDiskRep::mainExecutableImage()
{
	return mExecRep->mainExecutableImage();
}

void BundleDiskRep::prepareForSigning(SigningContext &context)
{
	return mExecRep->prepareForSigning(context);
}

size_t BundleDiskRep::signingBase()
{
	return mExecRep->signingBase();
}

size_t BundleDiskRep::signingLimit()
{
	return mExecRep->signingLimit();
}

string BundleDiskRep::format()
{
	return mFormat;
}

CFArrayRef BundleDiskRep::modifiedFiles()
{
    CFRef<CFArrayRef> execFiles = mExecRep->modifiedFiles();
    CFRef<CFMutableArrayRef> files = CFArrayCreateMutableCopy(NULL, 0, execFiles);
	checkModifiedFile(files, cdCodeDirectorySlot);
	checkModifiedFile(files, cdSignatureSlot);
	checkModifiedFile(files, cdResourceDirSlot);
	checkModifiedFile(files, cdTopDirectorySlot);
	checkModifiedFile(files, cdEntitlementSlot);
	checkModifiedFile(files, cdRepSpecificSlot);
	for (CodeDirectory::Slot slot = cdAlternateCodeDirectorySlots; slot < cdAlternateCodeDirectoryLimit; ++slot)
		checkModifiedFile(files, slot);
	return files.yield();
}

void BundleDiskRep::checkModifiedFile(CFMutableArrayRef files, CodeDirectory::SpecialSlot slot)
{
	if (CFDataRef data = mExecRep->component(slot))	// provided by executable file
		CFRelease(data);
	else if (const char *resourceName = CodeDirectory::canonicalSlotName(slot)) {
		string file = metaPath(resourceName);
		if (::access(file.c_str(), F_OK) == 0)
			CFArrayAppendValue(files, CFTempURL(file));
	}
}

FileDesc &BundleDiskRep::fd()
{
	return mExecRep->fd();
}

void BundleDiskRep::flush()
{
	mExecRep->flush();
}

CFDictionaryRef BundleDiskRep::diskRepInformation()
{
    return mExecRep->diskRepInformation();
}

//
// Defaults for signing operations
//
string BundleDiskRep::recommendedIdentifier(const SigningContext &)
{
	if (CFStringRef identifier = CFBundleGetIdentifier(mBundle))
		return cfString(identifier);
	if (CFDictionaryRef infoDict = CFBundleGetInfoDictionary(mBundle))
		if (CFStringRef identifier = CFStringRef(CFDictionaryGetValue(infoDict, kCFBundleNameKey)))
			return cfString(identifier);
	
	// fall back to using the canonical path
	return canonicalIdentifier(cfStringRelease(this->copyCanonicalPath()));
}

string BundleDiskRep::resourcesRelativePath()
{
	// figure out the resource directory base. Clean up some gunk inserted by CFBundle in frameworks
	string rbase = this->resourcesRootPath();
	size_t pos = rbase.find("/./");	// gratuitously inserted by CFBundle in some frameworks
	while (pos != std::string::npos) {
		rbase = rbase.replace(pos, 2, "", 0);
		pos = rbase.find("/./");
	}
	if (rbase.substr(rbase.length()-2, 2) == "/.")	// produced by versioned bundle implicit "Current" case
		rbase = rbase.substr(0, rbase.length()-2);	// ... so take it off for this
	
	// find the resources directory relative to the resource base
	string resources = cfStringRelease(CFBundleCopyResourcesDirectoryURL(mBundle));
	if (resources == rbase)
		resources = "";
	else if (resources.compare(0, rbase.length(), rbase, 0, rbase.length()) != 0)	// Resources not in resource root
		MacOSError::throwMe(errSecCSBadBundleFormat);
	else
		resources = resources.substr(rbase.length() + 1) + "/";	// differential path segment

	return resources;
}

CFDictionaryRef BundleDiskRep::defaultResourceRules(const SigningContext &ctx)
{
	string resources = this->resourcesRelativePath();

	// installer package rules
	if (mInstallerPackage)
		return cfmake<CFDictionaryRef>("{rules={"
			"'^.*' = #T"							// include everything, but...
			"%s = {optional=#T, weight=1000}"		// make localizations optional
			"'^.*/.*\\.pkg/' = {omit=#T, weight=10000}" // and exclude all nested packages (by name)
			"}}",
			(string("^") + resources + ".*\\.lproj/").c_str()
		);
	
	// old (V1) executable bundle rules - compatible with before
	if (ctx.signingFlags() & kSecCSSignV1)				// *** must be exactly the same as before ***
		return cfmake<CFDictionaryRef>("{rules={"
			"'^version.plist$' = #T"                    // include version.plist
			"%s = #T"                                   // include Resources
			"%s = {optional=#T, weight=1000}"           // make localizations optional
			"%s = {omit=#T, weight=1100}"               // exclude all locversion.plist files
			"}}",
			(string("^") + resources).c_str(),
			(string("^") + resources + ".*\\.lproj/").c_str(),
			(string("^") + resources + ".*\\.lproj/locversion.plist$").c_str()
		);
	
	// FMJ (everything is a resource) rules
	if (ctx.signingFlags() & kSecCSSignOpaque)			// Full Metal Jacket - everything is a resource file
		return cfmake<CFDictionaryRef>("{rules={"
			"'^.*' = #T"								// everything is a resource
			"'^Info\\.plist$' = {omit=#T,weight=10}"	// explicitly exclude this for backward compatibility
		"}}");
	
	// new (V2) executable bundle rules
	return cfmake<CFDictionaryRef>("{"					// *** the new (V2) world ***
		"rules={"										// old (V1; legacy) version
			"'^version.plist$' = #T"					// include version.plist
			"%s = #T"									// include Resources
			"%s = {optional=#T, weight=1000}"			// make localizations optional
			"%s = {weight=1010}"						// ... except for Base.lproj which really isn't optional at all
			"%s = {omit=#T, weight=1100}"				// exclude all locversion.plist files
		"},rules2={"
			"'^.*' = #T"								// include everything as a resource, with the following exceptions
			"'^[^/]+$' = {nested=#T, weight=10}"		// files directly in Contents
			"'^(Frameworks|SharedFrameworks|PlugIns|Plug-ins|XPCServices|Helpers|MacOS|Library/(Automator|Spotlight|LoginItems))/' = {nested=#T, weight=10}" // dynamic repositories
			"'.*\\.dSYM($|/)' = {weight=11}"			// but allow dSYM directories in code locations (parallel to their code)
			"'^(.*/)?\\.DS_Store$' = {omit=#T,weight=2000}"	// ignore .DS_Store files
			"'^Info\\.plist$' = {omit=#T, weight=20}"	// excluded automatically now, but old systems need to be told
			"'^version\\.plist$' = {weight=20}"			// include version.plist as resource
			"'^embedded\\.provisionprofile$' = {weight=20}"	// include embedded.provisionprofile as resource
			"'^PkgInfo$' = {omit=#T, weight=20}"		// traditionally not included
			"%s = {weight=20}"							// Resources override default nested (widgets)
			"%s = {optional=#T, weight=1000}"			// make localizations optional
			"%s = {weight=1010}"						// ... except for Base.lproj which really isn't optional at all
			"%s = {omit=#T, weight=1100}"				// exclude all locversion.plist files
		"}}",
			
		(string("^") + resources).c_str(),
		(string("^") + resources + ".*\\.lproj/").c_str(),
		(string("^") + resources + "Base\\.lproj/").c_str(),
		(string("^") + resources + ".*\\.lproj/locversion.plist$").c_str(),
			
		(string("^") + resources).c_str(),
		(string("^") + resources + ".*\\.lproj/").c_str(),
		(string("^") + resources + "Base\\.lproj/").c_str(),
		(string("^") + resources + ".*\\.lproj/locversion.plist$").c_str()
	);
}


CFArrayRef BundleDiskRep::allowedResourceOmissions()
{
	return cfmake<CFArrayRef>("["
		"'^(.*/)?\\.DS_Store$'"
		"'^Info\\.plist$'"
		"'^PkgInfo$'"
		"%s"
		"]",
		(string("^") + this->resourcesRelativePath() + ".*\\.lproj/locversion.plist$").c_str()
	);
}


const Requirements *BundleDiskRep::defaultRequirements(const Architecture *arch, const SigningContext &ctx)
{
	return mExecRep->defaultRequirements(arch, ctx);
}

size_t BundleDiskRep::pageSize(const SigningContext &ctx)
{
	return mExecRep->pageSize(ctx);
}


//
// Strict validation.
// Takes an array of CFNumbers of errors to tolerate.
//
void BundleDiskRep::strictValidate(const CodeDirectory* cd, const ToleratedErrors& tolerated, SecCSFlags flags)
{
	// scan our metadirectory (_CodeSignature) for unwanted guests
	if (!(flags & kSecCSQuickCheck))
		validateMetaDirectory(cd);
	
	// check accumulated strict errors and report them
	if (!(flags & kSecCSRestrictSidebandData))	// tolerate resource forks etc.
		mStrictErrors.erase(errSecCSInvalidAssociatedFileData);
	
	std::vector<OSStatus> fatalErrors;
	set_difference(mStrictErrors.begin(), mStrictErrors.end(), tolerated.begin(), tolerated.end(), back_inserter(fatalErrors));
	if (!fatalErrors.empty())
		MacOSError::throwMe(fatalErrors[0]);
	
	// if app focus is requested and this doesn't look like an app, fail - but allow whitelist overrides
	if (flags & kSecCSRestrictToAppLike)
		if (!mAppLike)
			if (tolerated.find(kSecCSRestrictToAppLike) == tolerated.end())
				MacOSError::throwMe(errSecCSNotAppLike);
	
	// now strict-check the main executable (which won't be an app-like object)
	mExecRep->strictValidate(cd, tolerated, flags & ~kSecCSRestrictToAppLike);
}

void BundleDiskRep::recordStrictError(OSStatus error)
{
	mStrictErrors.insert(error);
}


void BundleDiskRep::validateMetaDirectory(const CodeDirectory* cd)
{
	// we know the resource directory will be checked after this call, so we'll give it a pass here
	if (cd->slotIsPresent(-cdResourceDirSlot))
		mUsedComponents.insert(cdResourceDirSlot);
	
	// make a set of allowed (regular) filenames in this directory
	std::set<std::string> allowedFiles;
	for (auto it = mUsedComponents.begin(); it != mUsedComponents.end(); ++it) {
		switch (*it) {
		case cdInfoSlot:
			break;		// always from Info.plist, not from here
		default:
			if (const char *name = CodeDirectory::canonicalSlotName(*it)) {
				allowedFiles.insert(name);
			}
			break;
		}
	}
	DirScanner scan(mMetaPath);
	if (scan.initialized()) {
		while (struct dirent* ent = scan.getNext()) {
			if (!scan.isRegularFile(ent))
				MacOSError::throwMe(errSecCSUnsealedAppRoot);	// only regular files allowed
			if (allowedFiles.find(ent->d_name) == allowedFiles.end()) {	// not in expected set of files
				if (strcmp(ent->d_name, kSecCS_SIGNATUREFILE) == 0) {
					// special case - might be empty and unused (adhoc signature)
					AutoFileDesc fd(metaPath(kSecCS_SIGNATUREFILE));
					if (fd.fileSize() == 0)
						continue;	// that's okay, then
				}
				// not on list of needed files; it's a freeloading rogue!
				recordStrictError(errSecCSUnsealedAppRoot);	// funnel through strict set so GKOpaque can override it
			}
		}
	}
}


//
// Check framework root for unsafe symlinks and unsealed content.
//
void BundleDiskRep::validateFrameworkRoot(string root)
{
	// build regex element that matches either the "Current" symlink, or the name of the current version
	string current = "Current";
	char currentVersion[PATH_MAX];
	ssize_t len = ::readlink((root + "/Versions/Current").c_str(), currentVersion, sizeof(currentVersion)-1);
	if (len > 0) {
		currentVersion[len] = '\0';
		current = string("(Current|") + ResourceBuilder::escapeRE(currentVersion) + ")";
	}

	DirValidator val;
	val.require("^Versions$", DirValidator::directory | DirValidator::descend);	// descend into Versions directory
	val.require("^Versions/[^/]+$", DirValidator::directory);					// require at least one version
	val.require("^Versions/Current$", DirValidator::symlink,					// require Current symlink...
		"^(\\./)?(\\.\\.[^/]+|\\.?[^\\./][^/]*)$");								// ...must point to a version
	val.allow("^(Versions/)?\\.DS_Store$", DirValidator::file | DirValidator::noexec); // allow .DS_Store files
	val.allow("^[^/]+$", DirValidator::symlink, ^ string (const string &name, const string &target) {
		// top-level symlinks must point to namesake in current version
		return string("^(\\./)?Versions/") + current + "/" + ResourceBuilder::escapeRE(name) + "$";
	});
	// module.map must be regular non-executable file, or symlink to module.map in current version
	val.allow("^module\\.map$", DirValidator::file | DirValidator::noexec | DirValidator::symlink,
		string("^(\\./)?Versions/") + current + "/module\\.map$");

	try {
		val.validate(root, errSecCSUnsealedFrameworkRoot);
	} catch (const MacOSError &err) {
		recordStrictError(err.error);
	}
}

	
//
// Check a file descriptor for harmlessness. This is a strict check (only).
//
void BundleDiskRep::checkPlainFile(FileDesc fd, const std::string& path)
{
	if (!fd.isPlainFile(path))
		recordStrictError(errSecCSRegularFile);
	checkForks(fd);
}
	
void BundleDiskRep::checkForks(FileDesc fd)
{
	if (fd.hasExtendedAttribute(XATTR_RESOURCEFORK_NAME) || fd.hasExtendedAttribute(XATTR_FINDERINFO_NAME))
		recordStrictError(errSecCSInvalidAssociatedFileData);
}


//
// Writers
//
DiskRep::Writer *BundleDiskRep::writer()
{
	return new Writer(this);
}

BundleDiskRep::Writer::Writer(BundleDiskRep *r)
	: rep(r), mMadeMetaDirectory(false)
{
	execWriter = rep->mExecRep->writer();
}


//
// Write a component.
// Note that this isn't concerned with Mach-O writing; this is handled at
// a much higher level. If we're called, we write to a file in the Bundle's meta directory.
//
void BundleDiskRep::Writer::component(CodeDirectory::SpecialSlot slot, CFDataRef data)
{
	switch (slot) {
	default:
		if (!execWriter->attribute(writerLastResort))	// willing to take the data...
			return execWriter->component(slot, data);	// ... so hand it through
		// execWriter doesn't want the data; store it as a resource file (below)
	case cdResourceDirSlot:
		// the resource directory always goes into a bundle file
		if (const char *name = CodeDirectory::canonicalSlotName(slot)) {
			rep->createMeta();
			string path = rep->metaPath(name);
			AutoFileDesc fd(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			fd.writeAll(CFDataGetBytePtr(data), CFDataGetLength(data));
			mWrittenFiles.insert(name);
		} else
			MacOSError::throwMe(errSecCSBadBundleFormat);
	}
}


//
// Remove all signature data
//
void BundleDiskRep::Writer::remove()
{
	// remove signature from the executable
	execWriter->remove();
	
	// remove signature files from bundle
	for (CodeDirectory::SpecialSlot slot = 0; slot < cdSlotCount; slot++)
		remove(slot);
	remove(cdSignatureSlot);
}

void BundleDiskRep::Writer::remove(CodeDirectory::SpecialSlot slot)
{
	if (const char *name = CodeDirectory::canonicalSlotName(slot))
		if (::unlink(rep->metaPath(name).c_str()))
			switch (errno) {
			case ENOENT:		// not found - that's okay
				break;
			default:
				UnixError::throwMe();
			}
}


void BundleDiskRep::Writer::flush()
{
	execWriter->flush();
	purgeMetaDirectory();
}
	
	
// purge _CodeSignature of all left-over files from any previous signature
void BundleDiskRep::Writer::purgeMetaDirectory()
{
	DirScanner scan(rep->mMetaPath);
	if (scan.initialized()) {
		while (struct dirent* ent = scan.getNext()) {
			if (!scan.isRegularFile(ent))
				MacOSError::throwMe(errSecCSUnsealedAppRoot);	// only regular files allowed
			if (mWrittenFiles.find(ent->d_name) == mWrittenFiles.end()) {	// we didn't write this!
				scan.unlink(ent, 0);
			}
		}
	}
	
}


} // end namespace CodeSigning
} // end namespace Security
