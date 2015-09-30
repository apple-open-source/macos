/*
 * Copyright (c) 2009,2011-2012 Apple Inc. All Rights Reserved.
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
#ifndef _H_DYLDCACHE
#define _H_DYLDCACHE

#include <security_utilities/unix++.h>
#include <security_utilities/memutils.h>
#include <security_utilities/macho++.h>
#include <security_utilities/endian.h>
#include "dyld_cache_format.h"


//
// One (architecture of the) Shared Library Cache.
// We mmap the file rather than reading a copy, since its format is rather scattered, and we're not
// interested in the vast majority of it.
// This is a read-only view of the cache file. It will not allow modifications, though it may
// tell you where in the file they should go.
//
class DYLDCache : public UnixPlusPlus::AutoFileDesc {
public:
	DYLDCache(const std::string &path);
	virtual ~DYLDCache();
	
	std::string magic() const { return mHeader->magic; }
	uint64_t baseAddress() const { return flip(mHeader->dyldBaseAddress); }
	Architecture architecture() const { return mArch->architecture(); }
	size_t mapSize() const { return mSigStart; }		// size of all the mappings
	size_t signatureLength() const { return mSigLength; } // size of all the mappings
	size_t totalSize() const { return mLength; }		// size of entire file (>= mapSize(), we hope)
	
	template <class Int> Int flip(Int x) const { return mFlip ? Security::flip(x) : x; }
	
public:
	static std::string pathFor(const Architecture &arch); // file path for given architecture
	static bool validate(UnixPlusPlus::FileDesc &fd);	// does this look like a shared library cache?

protected:
	template <class SubStruct>
	class Item {
	public:
		Item(const DYLDCache &c, uint32_t off)
			: cache(c), mStruct(c.at<SubStruct>(off)) { }
		const DYLDCache &cache;
	
	protected:
		const SubStruct *mStruct;
		template <class Int> Int flip(Int x) const { return cache.flip(x); }
	};
	
public:
	//
	// A contiguous mapping established by the cache builder
	//
	struct Mapping : public Item<shared_file_mapping_np> {
		mach_vm_address_t address() const { return flip(mStruct->sfm_address); }
		mach_vm_size_t size() const { return flip(mStruct->sfm_size); }
		mach_vm_address_t limit() const { return address() + size(); }
		mach_vm_offset_t offset() const { return flip(mStruct->sfm_file_offset); }
		vm_prot_t maxProt() const { return flip(mStruct->sfm_max_prot); }
		vm_prot_t initProt() const { return flip(mStruct->sfm_init_prot); }
		
		bool contains(uint64_t address) const
			{ return address >= this->address() && address < this->limit(); }
		
		Mapping(const DYLDCache &c, uint32_t off) : Item<shared_file_mapping_np>(c, off) { }
	};

	uint32_t mappingCount() const { return flip(mHeader->mappingCount); }
	Mapping mapping(unsigned ix) const;

	Mapping findMap(uint64_t address) const;
	uint64_t mapAddress(uint64_t address) const;

public:
	//
	// One original binary ("image") as embedded in the cache.
	//
	struct Image : public Item<dyld_cache_image_info> {
		Image(const DYLDCache &c, uint32_t off) : Item<dyld_cache_image_info>(c, off) { }
		
		uint64_t address() const { return flip(mStruct->address); }
		uint64_t modTime() const { return flip(mStruct->modTime); }
		uint64_t inode() const { return flip(mStruct->inode); }
		uint32_t pad() const { return flip(mStruct->pad); }
		std::string path() const { return cache.at<char>(flip(mStruct->pathFileOffset)); }
	};

	uint32_t imageCount() const { return flip(mHeader->imagesCount); }
	Image image(unsigned ix) const;

public:
	template <class T>
	const T *at(uint32_t offset) const
	{
		size_t end = offset + sizeof(T);
		if (offset > end || end > mLength)
			UnixError::throwMe(ENOEXEC);
		return LowLevelMemoryUtilities::increment<const T>(mBase, offset);
	}

private:
	//
	// A private table correlating cache "magic strings" to information we need about the cache
	// (This should be in the cache files themselves)
	//
	struct ArchType {
		cpu_type_t cpu;			// main architecture
		cpu_subtype_t sub;		// subarchitecture
		char magic[16];			// cache file magic string
		char filename[10];		// conventional file name (off cacheFileBase)
		uint16_t order;			// byte order marker
		
		std::string path() const
			{ return std::string(DYLD_SHARED_CACHE_DIR DYLD_SHARED_CACHE_BASE_NAME) + filename; }
		
		Architecture architecture() const { return Architecture(cpu, sub); }
	};
	static const ArchType architectures[];
	static const ArchType defaultArchitecture;
	
	static const ArchType *matchArchitecture(const dyld_cache_header &header);

private:
	const void *mBase;
	size_t mLength;
	
	const dyld_cache_header *mHeader;	// cache file header (NOT byte order corrected)
	const ArchType *mArch;				// ArchType entry that describes this file	
	bool mFlip;							// need to flip all integers?
	size_t mSigStart;					// end of all file mappings (start of signature)
	size_t mSigLength;					// indicated length of signature
};


#endif //_H_DYLDCACHE
