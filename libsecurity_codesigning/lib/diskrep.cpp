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
#include "foreigndiskrep.h"


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
{ /* virtual */ }


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


//
// Given a file system path, come up with the most likely correct
// disk representation for what's there.
// This is, strictly speaking, a heuristic that could be fooled - there's
// no fool-proof rule for figuring this out. But we'd expect this to work
// fine in ordinary use. If you happen to know what you're looking at
// (say, a bundle), then just create the suitable subclass of DiskRep directly.
// That's quite legal.
//
DiskRep *DiskRep::bestGuess(const char *path)
{
	try {
    struct stat st;
    if (::stat(path, &st))
        UnixError::throwMe();
		
	// if it's a directory, assume it's a bundle
    if ((st.st_mode & S_IFMT) == S_IFDIR)	// directory - assume bundle
		return new BundleDiskRep(path);
	
	// see if it's the main executable of a recognized bundle
	if (CFRef<CFURLRef> pathURL = makeCFURL(path))
		if (CFRef<CFBundleRef> bundle = _CFBundleCreateWithExecutableURLIfMightBeBundle(NULL, pathURL))
				return new BundleDiskRep(bundle);
	
	// follow the file choosing rules
	return bestFileGuess(path);
	} catch (const CommonError &error) {
		switch (error.unixError()) {
		case ENOENT:
			MacOSError::throwMe(errSecCSStaticCodeNotFound);
		default:
			throw;
		}
	}
}


DiskRep *DiskRep::bestFileGuess(const char *path)
{
	AutoFileDesc fd(path, O_RDONLY);
	if (MachORep::candidiate(fd))
		return new MachORep(path);
	if (CFMDiskRep::candidiate(fd))
		return new CFMDiskRep(path);
	if (ForeignDiskRep::candidate(fd))
		return new ForeignDiskRep(path);

	return new FileDiskRep(path);
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


} // end namespace CodeSigning
} // end namespace Security
