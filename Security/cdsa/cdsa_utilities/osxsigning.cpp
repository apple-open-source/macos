/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// osxsigning - MacOS X's standard signable objects.
//
#include <Security/osxsigning.h>
#include <Security/cfutilities.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <CoreFoundation/CFBundle.h>


namespace Security
{

namespace CodeSigning
{

//
// Enumerate a single file on disk.
//
void OSXCode::scanFile(const char *pathname, Signer::State &state)
{
	// open the file (well, try)
	int fd = open(pathname, O_RDONLY);
	if (fd < 0)
		UnixError::throwMe();
	
	// how big is it?
	struct stat st;
	if (fstat(fd, &st)) {
		close(fd);
		UnixError::throwMe();
	}
#if defined(LIMITED_SIGNING)
	if (st.st_size >= 0x4000)
		st.st_size = 0x4000;
#endif
	
	// map it
	void *p = mmap(NULL, st.st_size, PROT_READ, MAP_FILE, fd, 0);
	close(fd);	// done with this either way
	if (p == MAP_FAILED)
		UnixError::throwMe();
	
	// scan it
	secdebug("codesign", "scanning file %s (%ld bytes)", pathname, long(st.st_size));
	state.enumerateContents(p, st.st_size);
	
	// unmap it (ignore error)
	munmap(p, st.st_size);
}


//
// Use prefix encoding for externalizing OSXCode objects
//
OSXCode *OSXCode::decode(const char *extForm)
{
	if (!extForm || !extForm[0] || extForm[1] != ':')
		return NULL;
	switch (extForm[0]) {
	case 't':
		return new ExecutableTool(extForm+2);
	case 'b':
		return new GenericBundle(extForm+2);
	default:
		return NULL;
	}
}


//
// Produce a Signable for the currently running application
//
OSXCode *OSXCode::main()
{
	//@@@ cache the main bundle?
	if (CFBundleRef mainBundle = CFBundleGetMainBundle()) {
        CFRef<CFURLRef> base = CFBundleCopyBundleURL(mainBundle);
        CFRef<CFURLRef> resources(CFBundleCopyResourcesDirectoryURL(mainBundle));
        if (base && resources && !CFEqual(resources, base)) {
			// assume this is a real bundle
			return new ApplicationBundle(getPath(CFBundleCopyBundleURL(mainBundle)).c_str());
        }

		// too weird; assume this is a single-file "tool" executable
		return new ExecutableTool(getPath(CFBundleCopyExecutableURL(mainBundle)).c_str());
	}
	// CF gives no error indications...
	CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
}


// Note: The public CFURLCopyFileSystemPath fails to resolve relative URLs as
// produced by CFURL methods. We need to call an internal(!) method of CF to get
// the full path.
extern "C" CFStringRef CFURLCreateStringWithFileSystemPath(CFAllocatorRef allocator,
	CFURLRef anURL, CFURLPathStyle fsType, Boolean resolveAgainstBase);

string OSXCode::getPath(CFURLRef url)
{
    if (url == NULL)
        CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);	// source CF call failed

	CFRef<CFStringRef> str(CFURLCreateStringWithFileSystemPath(NULL, 
		url, kCFURLPOSIXPathStyle, true));
	CFRelease(url);
	if (str) {
		char path[PATH_MAX];
		if (CFStringGetCString(str, path, PATH_MAX, kCFStringEncodingUTF8))
			return path;
	}
	// no error indications from CF...
	CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
}


//
// Produce a Signable for whatever is at a given path.
// This tries to guess at the type of Signable to be used.
// If you *know*, just create the suitable subclass directly.
//
OSXCode *OSXCode::at(const char *path)
{
	struct stat st;
	if (stat(path, &st))
		UnixError::throwMe();
	if ((st.st_mode & S_IFMT) == S_IFDIR) {	// directory - assume bundle
		return new GenericBundle(path);
	} else {
		// look for .../Contents/MacOS/<base>
		if (const char *slash = strrchr(path, '/'))
			if (const char *contents = strstr(path, "/Contents/MacOS/"))
				if (contents + 15 == slash)
					return new GenericBundle(string(path).substr(0, contents-path).c_str());
		// assume tool (single executable)
		return new ExecutableTool(path);
	}
}


//
// Executable Tools
//
void ExecutableTool::scanContents(Signer::State &state) const
{
	scanFile(mPath.c_str(), state);
}

string ExecutableTool::encode() const
{
	return "t:" + mPath;
}

string ExecutableTool::canonicalPath() const
{
	return path();
}


//
// Generic Bundles
//
GenericBundle::GenericBundle(const char *path) : mPath(path)
{
	CFRef<CFURLRef> url(CFURLCreateFromFileSystemRepresentation(NULL, 
		(const UInt8 *)path, strlen(path), true));
	if (!url || !(mBundle = CFBundleCreate(NULL, url)))
        CssmError::throwMe(CSSMERR_CSSM_ADDIN_LOAD_FAILED);
}

GenericBundle::~GenericBundle()
{
    CFRelease(mBundle);
}


void GenericBundle::scanContents(Signer::State &state) const
{
	scanFile(executablePath().c_str(), state);
}

string GenericBundle::encode() const
{
	return "b:" + mPath;
}

void *GenericBundle::lookupSymbol(const char *name)
{
    CFRef<CFStringRef> cfName(CFStringCreateWithCString(NULL, name,
                                                        kCFStringEncodingMacRoman));
    if (!cfName)
        CssmError::throwMe(CSSM_ERRCODE_UNKNOWN_FORMAT);
    void *function = CFBundleGetFunctionPointerForName(mBundle, cfName);
    if (function == NULL)
        CssmError::throwMe(CSSM_ERRCODE_UNKNOWN_FORMAT);
    return function;
}

string GenericBundle::canonicalPath() const
{
	return path();
}


//
// Load management for a loadable bundle
//
void LoadableBundle::load()
{
	if (!CFBundleLoadExecutable(mBundle))
		CssmError::throwMe(CSSMERR_CSSM_ADDIN_LOAD_FAILED);
    secdebug("bundle", "%p (%s) loaded", this, path().c_str());
}

void LoadableBundle::unload()
{
    secdebug("bundle", "%p (%s) unloaded", this, path().c_str());
	CFBundleUnloadExecutable(mBundle);
}

bool LoadableBundle::isLoaded() const
{
	return CFBundleIsExecutableLoaded(mBundle);
}


}; // end namespace CodeSigning

} // end namespace Security
