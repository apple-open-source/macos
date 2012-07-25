/*
 * Copyright (c) 2009 Apple Inc. All Rights Reserved.
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
// dyldcache - access layer to the DYLD Shared Library Cache file
//
#include "dyldcache.h"


//
// Table of supported architectures.
// The cache file header has no direct architecture information, so we need to deduce it like this.
//
static const uint16_t bigEndian = 0x1200;
static const uint16_t littleEndian = 0x0012;

const DYLDCache::ArchType DYLDCache::architectures[] = {
	{ CPU_TYPE_X86_64, CPU_SUBTYPE_MULTIPLE,	"dyld_v1  x86_64", "x86_64", littleEndian },
	{ CPU_TYPE_X86, CPU_SUBTYPE_MULTIPLE,		"dyld_v1    i386", "i386", littleEndian },
	{ CPU_TYPE_POWERPC, CPU_SUBTYPE_MULTIPLE,	"dyld_v1     ppc", "rosetta", bigEndian },
	{ CPU_TYPE_ARM, CPU_SUBTYPE_ARM_V6,			"dyld_v1   armv6", "armv6", littleEndian },
	{ CPU_TYPE_ARM, CPU_SUBTYPE_ARM_V7,			"dyld_v1   armv7", "armv7", littleEndian },
	{ 0 }
};


//
// Architecture matching and lookup
//
std::string DYLDCache::pathFor(const Architecture &arch)
{
	for (const ArchType *it = architectures; it->cpu; it++)
		if (arch.matches(it->architecture()))
			return it->path();
	UnixError::throwMe(ENOEXEC);
}

const DYLDCache::ArchType *DYLDCache::matchArchitecture(const dyld_cache_header &header)
{
	for (const ArchType *arch = architectures; arch->cpu; arch++)
		if (!strcmp(header.magic, arch->magic))
			return arch;
	return NULL;
}


//
// Construction and teardown
//
DYLDCache::DYLDCache(const std::string &path)
{
	this->open(path);
	mLength = this->fileSize();
	mBase = this->mmap(PROT_READ, mLength);
	mHeader = at<dyld_cache_header>(0);

	if ((mArch = matchArchitecture(*mHeader)) == NULL)
		UnixError::throwMe(ENOEXEC);
	mFlip = *((const uint8_t *)&mArch->order) != 0x12;
	
	mSigStart = flip(mHeader->codeSignatureOffset);
	mSigLength = flip(mHeader->codeSignatureSize);
}


DYLDCache::~DYLDCache()
{
	::munmap((void *)mBase, mLength);
}


//
// Preflight a file for file type
//
bool DYLDCache::validate(UnixPlusPlus::FileDesc &fd)
{
	dyld_cache_header header;
	return fd.read(&header, sizeof(header), 0) == sizeof(header)
		&& matchArchitecture(header) != NULL;
}


//
// Locate a mapping in the cache
//
DYLDCache::Mapping DYLDCache::mapping(unsigned ix) const
{
	assert(ix < this->mappingCount());
	return Mapping(*this, flip(mHeader->mappingOffset) + ix * sizeof(shared_file_mapping_np));
}


//
// Locate an image in the cache
//
DYLDCache::Image DYLDCache::image(unsigned ix) const
{
	assert(ix < this->imageCount());
	return Image(*this, flip(mHeader->imagesOffset) + ix * sizeof(dyld_cache_image_info));
}



DYLDCache::Mapping DYLDCache::findMap(uint64_t address) const
{
	for (unsigned ix = 0; ix < mappingCount(); ix++) {
		Mapping map = this->mapping(ix);
		if (map.contains(address))
			return map;
	}
	UnixError::throwMe(EINVAL);
}

uint64_t DYLDCache::mapAddress(uint64_t address) const
{
	Mapping map = this->findMap(address);
	return (address - map.address()) + map.offset();
}
