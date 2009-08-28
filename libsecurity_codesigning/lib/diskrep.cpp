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
// diskrep - disk representations of code
//
#include "diskrep.h"
#include <sys/stat.h>
#include <CoreFoundation/CFBundlePriv.h>

// specific disk representations created by the bestGuess() function
#include "filediskrep.h"
#include "bundlediskrep.h"
#include "cfmdiskrep.h"
#include "slcrep.h"


namespace Security {
namespace CodeSigning {

using namespace UnixPlusPlus;


//
// Abstract features
//
DiskRep::DiskRep()
{
}

DiskRep::~DiskRep()
{
	CODESIGN_DISKREP_DESTROY(this);
}


//
// Normal DiskReps are their own base.
//
DiskRep *DiskRep::base()
{
	return this;
}


//
// By default, DiskReps are read-only.
//
DiskRep::Writer *DiskRep::writer()
{
	MacOSError::throwMe(errSecCSBadObjectFormat);
}


void DiskRep::Writer::addDiscretionary(CodeDirectory::Builder &)
{
	// do nothing
}


//
// Given a file system path, come up with the most likely correct
// disk representation for what's there.
// This is, strictly speaking, a heuristic that could be fooled - there's
// no fool-proof rule for figuring this out. But we'd expect this to work
// fine in ordinary use. If you happen to know what you're looking at
// (say, a bundle), then just create the suitable subclass of DiskRep directly.
// That's quite legal.
// The optional context argument can provide additional information that guides the guess.
//
DiskRep *DiskRep::bestGuess(const char *path, const Context *ctx)
{
	try {
		if (!(ctx && ctx->fileOnly)) {
			struct stat st;
			if (::stat(path, &st))
				UnixError::throwMe();
				
			// if it's a directory, assume it's a bundle
			if ((st.st_mode & S_IFMT) == S_IFDIR)	// directory - assume bundle
				return new BundleDiskRep(path, ctx);
			
			// see if it's the main executable of a recognized bundle
			if (CFRef<CFURLRef> pathURL = makeCFURL(path))
				if (CFRef<CFBundleRef> bundle = _CFBundleCreateWithExecutableURLIfMightBeBundle(NULL, pathURL))
						return new BundleDiskRep(bundle, ctx);
		}
		
		// try the various single-file representations
		AutoFileDesc fd(path, O_RDONLY);
		if (MachORep::candidate(fd))
			return new MachORep(path, ctx);
		if (CFMDiskRep::candidate(fd))
			return new CFMDiskRep(path);
		if (DYLDCacheRep::candidate(fd))
			return new DYLDCacheRep(path);

		// ultimate fallback - the generic file representation
		return new FileDiskRep(path);

	} catch (const CommonError &error) {
		switch (error.unixError()) {
		case ENOENT:
			MacOSError::throwMe(errSecCSStaticCodeNotFound);
		default:
			throw;
		}
	}
}


DiskRep *DiskRep::bestFileGuess(const char *path, const Context *ctx)
{
	Context dctx;
	if (ctx)
		dctx = *ctx;
	dctx.fileOnly = true;
	return bestGuess(path, &dctx);
}


//
// Given a main executable known to be a Mach-O binary, and an offset into
// the file of the actual architecture desired (of a Universal file),
// produce a suitable MachORep.
// This function does not consider non-MachO binaries. It does however handle
// bundles with Mach-O main executables correctly.
//
DiskRep *DiskRep::bestGuess(const char *path, size_t archOffset)
{
	try {
		// is it the main executable of a bundle?
		if (CFRef<CFURLRef> pathURL = makeCFURL(path))
			if (CFRef<CFBundleRef> bundle = _CFBundleCreateWithExecutableURLIfMightBeBundle(NULL, pathURL)) {
				Context ctx; ctx.offset = archOffset;
				return new BundleDiskRep(bundle, &ctx);	// ask bundle to make bundle-with-MachO-at-offset
			}
		// else, must be a Mach-O binary
		Context ctx; ctx.offset = archOffset;
		return new MachORep(path, &ctx);
	} catch (const CommonError &error) {
		switch (error.unixError()) {
		case ENOENT:
			MacOSError::throwMe(errSecCSStaticCodeNotFound);
		default:
			throw;
		}
	}
}


//
// Default behaviors of DiskRep
//
string DiskRep::resourcesRootPath()
{
	return "";		// has no resources directory
}

CFDictionaryRef DiskRep::defaultResourceRules()
{
	return NULL;	// none
}

void DiskRep::adjustResources(ResourceBuilder &builder)
{
	// do nothing
}

const Requirements *DiskRep::defaultRequirements(const Architecture *)
{
	return NULL;	// none
}

Universal *DiskRep::mainExecutableImage()
{
	return NULL;	// no Mach-O executable
}

size_t DiskRep::pageSize()
{
	return monolithicPageSize;	// unpaged (monolithic)
}

size_t DiskRep::signingBase()
{
	return 0;		// whole file (start at beginning)
}

CFArrayRef DiskRep::modifiedFiles()
{
	// by default, claim (just) the main executable modified
	CFRef<CFURLRef> mainURL = makeCFURL(mainExecutablePath());
	return makeCFArray(1, mainURL.get());
}

void DiskRep::flush()
{
	// nothing cached
}


//
// Writers
//
DiskRep::Writer::Writer(uint32_t attrs)
	: mArch(CPU_TYPE_ANY), mAttributes(attrs)
{
}

DiskRep::Writer::~Writer()
{ /* virtual */ }

uint32_t DiskRep::Writer::attributes() const
{ return mAttributes; }

void DiskRep::Writer::flush()
{ /* do nothing */ }

void DiskRep::Writer::remove()
{
	MacOSError::throwMe(errSecCSNotSupported);
}


} // end namespace CodeSigning
} // end namespace Security
