/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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
// macho++ - Mach-O object file helpers
//
#ifndef _H_MACHOPLUSPLUS
#define _H_MACHOPLUSPLUS

#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/arch.h>
#include <security_utilities/globalizer.h>
#include <security_utilities/endian.h>
#include <security_utilities/unix++.h>
#include <security_utilities/cfutilities.h>

namespace Security {


//
// An architecture specification.
// Simply a pair or (cpu type, cpu subtype), really.
//
class Architecture : public std::pair<cpu_type_t, cpu_subtype_t> {
	typedef std::pair<cpu_type_t, cpu_subtype_t> _Pair;
public:
	Architecture() { }
	explicit Architecture(cpu_type_t type, cpu_subtype_t sub = CPU_SUBTYPE_MULTIPLE)
		: std::pair<cpu_type_t, cpu_subtype_t>(type, sub) { }
	Architecture(const fat_arch &archInFile);

	cpu_type_t cpuType() const { return this->first; }
	cpu_subtype_t cpuSubtype() const { return this->second; }
	const char *name() const;
	
	static const cpu_type_t none = 0;
	operator bool () const { return cpuType() != none; }
	bool operator ! () const { return cpuType() == none; }
	
public:
	friend bool operator == (const Architecture &a1, const Architecture &a2)
	{ return _Pair(a1) == _Pair(a2); }

	friend bool operator < (const Architecture &a1, const Architecture &a2)
	{ return _Pair(a1) < _Pair(a2); }
	
	bool matches(const Architecture &templ) const
	{ return first == templ.first && (second == templ.second || templ.second == 0 || templ.second == CPU_SUBTYPE_MULTIPLE); }

public:
	static Architecture local();
};


//
// A Mach-O formatted file segment.
//
class MachO : public UnixPlusPlus::FileDesc {
public:
	MachO(FileDesc fd, size_t offset = 0, size_t length = 0);
	~MachO();
	
	size_t offset() const { return mOffset; }
	size_t length() const { return mLength; }
	
	template <class T>
	T flip(T value) const
	{ return mFlip ? Security::flip(value) : value; }
	
	bool isFlipped() const { return mFlip; }
	bool is64() const { return m64; }
	
	const mach_header &header() const { return mHeader; }
	Architecture architecture() const;
	uint32_t type() const;
	uint32_t flags() const;
	
	const load_command *loadCommands() const { return mCommands; }
	const load_command *nextCommand(const load_command *command) const;
	size_t commandLength() const { return flip(mHeader.sizeofcmds); }
	
	const load_command *findCommand(uint32_t cmd) const;
	const segment_command *findSegment(const char *segname) const;
	const section *findSection(const char *segname, const char *sectname) const;
	
	const char *string(const load_command *cmd, const lc_str &str) const;

	const linkedit_data_command *findCodeSignature() const;
	
	size_t signingOffset() const;	// starting offset of CS section, or 0 if none
	size_t signingLength() const;	// length of CS section, or 0 if none
	size_t signingExtent() const;	// signingOffset, or file length if none

	void seek(size_t offset);	// relative to start of image
	CFDataRef dataAt(size_t offset, size_t size);

private:
	size_t mOffset;			// starting file offset
	size_t mLength;			// Mach-O file length
	bool m64;				// is 64-bit
	bool mFlip;				// wrong byte order (flip all integers)
	mach_header mHeader;	// Mach-O header
	load_command *mCommands; // load commands
	load_command *mEndCommands; // end of load commands
};


//
// A Universal object represents a Mach-O binary image (whole) file.
// It can represent a true Universal (aka "Fat") file with multiple
// architectures; but it will also represent a single Mach-O ("thin")
// binary and make you believe it's a Universal with just one architecture.
//
class Universal : public UnixPlusPlus::FileDesc {
public:
	Universal(FileDesc fd, off_t offset = 0);
	~Universal();
	
	// return a genuine MachO object for the given architecture
	MachO *architecture() const;		// native
	MachO *architecture(const Architecture &arch) const; // given
	MachO *architecture(off_t offset) const; // given by file offset
	
	// return (just) the starting offset of an architecture
	size_t archOffset() const;			// native
	size_t archOffset(const Architecture &arch) const; // given
	
	// return a set of architectures contained
	typedef std::set<Architecture> Architectures;
	void architectures(Architectures &archs);
	
	bool isUniversal() const { return mArchList != NULL; }
	Architecture bestNativeArch() const;
	
public:
	static uint32_t typeOf(FileDesc fd);

private:
	const fat_arch *findArch(const Architecture &arch) const;
	MachO *findImage(const Architecture &arch) const;

private:
	fat_arch *mArchList;		// architectures (NULL if thin file)
	unsigned mArchCount;		// number of architectures (if fat)
	Architecture mThinArch;		// single architecture (if thin)
	off_t mBase;				// overriding offset in file (all types)
};


} // end namespace Security

#endif // !_H_MACHOPLUSPLUS
