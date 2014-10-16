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
	: mBundle(CFBundleCreate(NULL, CFTempURL(path)))
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

// common construction code
void BundleDiskRep::setup(const Context *ctx)
{
	mInstallerPackage = false;	// default

	// capture the path of the main executable before descending into a specific version
	CFRef<CFURLRef> mainExecBefore = CFBundleCopyExecutableURL(mBundle);

	// validate the bundle root; fish around for the desired framework version
	string root = cfStringRelease(copyCanonicalPath());
	string contents = root + "/Contents";
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
	} else if (::access(version.c_str(), F_OK) == 0) {	// versioned bundle
		if (CFBundleRef versionBundle = CFBundleCreate(NULL, CFTempURL(version)))
			mBundle.take(versionBundle);	// replace top bundle ref
		else
			MacOSError::throwMe(errSecCSStaticCodeNotFound);
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
			if (mainExecBefore && (!ctx || !ctx->version)) {
				char main_exec_before[PATH_MAX];
				char main_exec[PATH_MAX];
				// The realpath call is important because alot of Framework bundles have a symlink
				// to their "Current" version binary in the main bundle
				if (realpath(cfString(mainExecBefore).c_str(), main_exec_before) == NULL ||
					realpath(cfString(mainExec).c_str(), main_exec) == NULL)
					MacOSError::throwMe(errSecCSInternalError);

				if (strcmp(main_exec_before, main_exec) != 0)
					recordStrictError(errSecCSAmbiguousBundleFormat);
			}

			mMainExecutableURL = mainExec;
			mExecRep = DiskRep::bestFileGuess(this->mainExecutablePath(), ctx);
			if (!mExecRep->fd().isPlainFile(this->mainExecutablePath()))
				recordStrictError(errSecCSRegularFile);
			mFormat = "bundle with " + mExecRep->format();
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
		if (!mExecRep->fd().isPlainFile(this->mainExecutablePath()))
			recordStrictError(errSecCSRegularFile);
		mFormat = "widget bundle";
		return;
	}
	
	// do we have a real Info.plist here?
	if (CFRef<CFURLRef> infoURL = _CFBundleCopyInfoPlistURL(mBundle)) {
		// focus on the Info.plist (which we know exists) as the nominal "main executable" file
		mMainExecutableURL = infoURL;
		mExecRep = new FileDiskRep(this->mainExecutablePath().c_str());
		if (!mExecRep->fd().isPlainFile(this->mainExecutablePath()))
			recordStrictError(errSecCSRegularFile);
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
		if (!mExecRep->fd().isPlainFile(this->mainExecutablePath()))
			recordStrictError(errSecCSRegularFile);
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
// Create a path to a bundle signing resource, by name.
// If the BUNDLEDISKREP_DIRECTORY directory exists in the bundle's support directory, files
// will be read and written there. Otherwise, they go directly into the support directory.
//
string BundleDiskRep::metaPath(const char *name)
{
	if (mMetaPath.empty()) {
		string support = cfStringRelease(CFBundleCopySupportFilesDirectoryURL(mBundle));
		mMetaPath = support + "/" BUNDLEDISKREP_DIRECTORY;
		if (::access(mMetaPath.c_str(), F_OK) == 0) {
			mMetaExists = true;
		} else {
			mMetaPath = support;
			mMetaExists = false;
		}
	}
	return mMetaPath + "/" + name;
}


//
// Try to create the meta-file directory in our bundle.
// Does nothing if the directory already exists.
// Throws if an error occurs.
//
void BundleDiskRep::createMeta()
{
	string meta = metaPath(BUNDLEDISKREP_DIRECTORY);
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
// Load's a CFURL and makes sure that it is a regular file and not a symlink (or fifo, etc.)
//
CFDataRef BundleDiskRep::loadRegularFile(CFURLRef url)
{
	assert(url);

	CFDataRef data = NULL;

	std::string path(cfString(url));

	AutoFileDesc fd(path);

	if (!fd.isPlainFile(path))
		recordStrictError(errSecCSRegularFile);

	data = cfLoadFile(fd, fd.fileSize());

	if (!data) {
		secdebug(__PRETTY_FUNCTION__, "failed to load %s", cfString(url).c_str());
		MacOSError::throwMe(errSecCSInternalError);
	}

	return data;
}

//
// Load and return a component, by slot number.
// Info.plist components come from the bundle, always (we don't look
// for Mach-O embedded versions).
// Everything else comes from the embedded blobs of a Mach-O image, or from
// files located in the Contents directory of the bundle.
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
	// by default, we take components from the executable image or files
	default:
		if (CFDataRef data = mExecRep->component(slot))
			return data;
		// falling through
	// but the following always come from files
	case cdResourceDirSlot:
		if (const char *name = CodeDirectory::canonicalSlotName(slot))
			return metaData(name);
		else
			return NULL;
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
			+ ResourceBuilder::escapeRE(executable.substr(resources.length()+1)) + "$");
}



Universal *BundleDiskRep::mainExecutableImage()
{
	return mExecRep->mainExecutableImage();
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
	CFMutableArrayRef files = CFArrayCreateMutableCopy(NULL, 0, mExecRep->modifiedFiles());
	checkModifiedFile(files, cdCodeDirectorySlot);
	checkModifiedFile(files, cdSignatureSlot);
	checkModifiedFile(files, cdResourceDirSlot);
	checkModifiedFile(files, cdEntitlementSlot);
	return files;
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
			"%s = {omit=#T, weight=1100}"				// exclude all locversion.plist files
		"}}",
			
		(string("^") + resources).c_str(),
		(string("^") + resources + ".*\\.lproj/").c_str(),
		(string("^") + resources + ".*\\.lproj/locversion.plist$").c_str(),
			
		(string("^") + resources).c_str(),
		(string("^") + resources + ".*\\.lproj/").c_str(),
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
void BundleDiskRep::strictValidate(const ToleratedErrors& tolerated)
{
	std::vector<OSStatus> fatalErrors;
	set_difference(mStrictErrors.begin(), mStrictErrors.end(), tolerated.begin(), tolerated.end(), back_inserter(fatalErrors));
	if (!fatalErrors.empty())
		MacOSError::throwMe(fatalErrors[0]);
	mExecRep->strictValidate(tolerated);
}

void BundleDiskRep::recordStrictError(OSStatus error)
{
	mStrictErrors.insert(error);
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
}


} // end namespace CodeSigning
} // end namespace Security
