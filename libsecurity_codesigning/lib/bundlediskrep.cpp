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
#include "bundlediskrep.h"
#include <CoreFoundation/CFURLAccess.h>
#include <CoreFoundation/CFBundlePriv.h>
#include <security_codesigning/cfmunge.h>


namespace Security {
namespace CodeSigning {

using namespace UnixPlusPlus;


//
// We make a CFBundleRef immediately, but everything else is lazy
//
BundleDiskRep::BundleDiskRep(const char *path)
	: mBundle(_CFBundleCreateIfLooksLikeBundle(NULL, CFTempURL(path)))
{
	if (!mBundle)
		MacOSError::throwMe(errSecCSBadObjectFormat);
	mExecRep = DiskRep::bestFileGuess(this->mainExecutablePath());
}

BundleDiskRep::BundleDiskRep(CFBundleRef ref)
{
	mBundle = ref;		// retains
	mExecRep = DiskRep::bestFileGuess(this->mainExecutablePath());
}


//
// Create a path to a bundle signing resource, by name.
// Note that these are stored in the bundle's Content directory,
// not its Resources directory.
//
string BundleDiskRep::resourcePath(const char *name)
{
	if (mResourcePath.empty())
		mResourcePath = cfString(CFBundleCopySupportFilesDirectoryURL(mBundle), true);
	return mResourcePath + "/" + name;
}


//
// Load the data for a signing resource, by URL.
//
CFDataRef BundleDiskRep::resourceData(CFURLRef url)
{
	CFDataRef data;
	SInt32 error;
	if (CFURLCreateDataAndPropertiesFromResource(NULL, url,
		&data, NULL, NULL, &error)) {
		return data;
	} else {
		secdebug("bundlerep", "failed to fetch %s error=%d",
			cfString(url).c_str(), int(error));
		return NULL;
	}
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
			return resourceData(info);
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
			return resourceData(name);
		else
			return NULL;
	}
}


//
// Various aspects of our DiskRep personality.
//
CFURLRef BundleDiskRep::canonicalPath()
{
	return CFBundleCopyBundleURL(mBundle);
}

string BundleDiskRep::recommendedIdentifier()
{
	if (CFStringRef identifier = CFBundleGetIdentifier(mBundle))
		return cfString(identifier);
	if (CFDictionaryRef infoDict = CFBundleGetInfoDictionary(mBundle))
		if (CFStringRef identifier = CFStringRef(CFDictionaryGetValue(infoDict, kCFBundleNameKey)))
			return cfString(identifier);
	
	// fall back to using the $(basename) of the canonical path. Drop any .app suffix
	string path = cfString(this->canonicalPath());
	if (path.substr(path.size() - 4) == ".app")
		path = path.substr(0, path.size() - 4);
	string::size_type p = path.rfind('/');
	if (p == string::npos)
		return path;
	else
		return path.substr(p+1);
}

string BundleDiskRep::mainExecutablePath()
{
	if (CFURLRef exec = CFBundleCopyExecutableURL(mBundle))
		return cfString(exec, true);
	else
		MacOSError::throwMe(errSecCSBadObjectFormat);
}

string BundleDiskRep::resourcesRootPath()
{
	return cfString(CFBundleCopySupportFilesDirectoryURL(mBundle), true);
}

CFDictionaryRef BundleDiskRep::defaultResourceRules()
{
	return cfmake<CFDictionaryRef>("{rules={"
		"'^version.plist$' = #T"
		"'^Resources/' = #T"
		"'^Resources/.*\\.lproj/' = {optional=#T, weight=1000}"
		"'^Resources/.*\\.lproj/locversion.plist$' = {omit=#T, weight=1100}"
		"}}");
}

const Requirements *BundleDiskRep::defaultRequirements(const Architecture *arch)
{
	return mExecRep->defaultRequirements(arch);
}


Universal *BundleDiskRep::mainExecutableImage()
{
	return mExecRep->mainExecutableImage();
}

size_t BundleDiskRep::pageSize()
{
	return mExecRep->pageSize();
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
	return string("bundle with ") + mExecRep->format();
}

CFArrayRef BundleDiskRep::modifiedFiles()
{
	CFMutableArrayRef files = CFArrayCreateMutableCopy(NULL, 0, mExecRep->modifiedFiles());
	checkModifiedFile(files, cdCodeDirectorySlot);
	checkModifiedFile(files, cdSignatureSlot);
	checkModifiedFile(files, cdResourceDirSlot);
	return files;
}

void BundleDiskRep::checkModifiedFile(CFMutableArrayRef files, CodeDirectory::SpecialSlot slot)
{
	if (CFDataRef data = mExecRep->component(slot))	// provided by executable file
		CFRelease(data);
	else if (const char *resourceName = CodeDirectory::canonicalSlotName(slot)) // bundle file
		CFArrayAppendValue(files, CFTempURL(resourcePath(resourceName)));
	else
		/* we don't have that one */;
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
// Writers
//
DiskRep::Writer *BundleDiskRep::writer()
{
	return new Writer(this);
}

BundleDiskRep::Writer::Writer(BundleDiskRep *r)
	: rep(r)
{
	execWriter = rep->mExecRep->writer();
}


//
// Write a component.
// Note that this isn't concerned with Mach-O writing; this is handled at
// a much higher level. If we're called, we write to a file in the Bundle's contents directory.
//
void BundleDiskRep::Writer::component(CodeDirectory::SpecialSlot slot, CFDataRef data)
{
	switch (slot) {
	default:
		if (!execWriter->attribute(writerLastResort))	// willing to take the data...
			return execWriter->component(slot, data);	// ... so hand it through
		// execWriter doesn't want the data; store it as a resource file (below)
	case cdResourceDirSlot:
	case cdRequirementsSlot:
		// the resource directory always goes into a bundle file
		if (const char *name = CodeDirectory::canonicalSlotName(slot)) {
			AutoFileDesc fd(rep->resourcePath(name), O_WRONLY | O_CREAT | O_TRUNC);
			fd.writeAll(CFDataGetBytePtr(data), CFDataGetLength(data));
		} else
			MacOSError::throwMe(errSecCSBadObjectFormat);
	}
}


void BundleDiskRep::Writer::flush()
{
	execWriter->flush();
}


} // end namespace CodeSigning
} // end namespace Security
