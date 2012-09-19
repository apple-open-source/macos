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
#include "macho++.h"
#include <security_utilities/memutils.h>
#include <security_utilities/endian.h>
#include <mach-o/dyld.h>

namespace Security {


//
// Architecture values
//
Architecture::Architecture(const fat_arch &arch)
	: pair<cpu_type_t, cpu_subtype_t>(arch.cputype, arch.cpusubtype)
{
}

Architecture::Architecture(const char *name)
{
	if (const NXArchInfo *nxa = NXGetArchInfoFromName(name)) {
		this->first = nxa->cputype;
		this->second = nxa->cpusubtype;
	} else {
		this->first = this->second = none;
	}
}


//
// The local architecture.
//
// We take this from ourselves - the architecture of our main program Mach-O binary.
// There's the NXGetLocalArchInfo API, but it insists on saying "i386" on modern
// x86_64-centric systems, and lies to ppc (Rosetta) programs claiming they're native ppc.
// So let's not use that.
//
Architecture Architecture::local()
{
	return MainMachOImage().architecture();
}


//
// Translate between names and numbers
//
const char *Architecture::name() const
{
	if (const NXArchInfo *info = NXGetArchInfoFromCpuType(cpuType(), cpuSubtype()))
		return info->name;
	else
		return NULL;
}

std::string Architecture::displayName() const
{
	if (const char *s = this->name())
		return s;
	char buf[20];
	snprintf(buf, sizeof(buf), "(%d:%d)", cpuType(), cpuSubtype());
	return buf;
}


//
// Compare architectures.
// This is asymmetrical; the second argument provides for some templating.
//
bool Architecture::matches(const Architecture &templ) const
{
	if (first != templ.first)
		return false;	// main architecture mismatch
	if (templ.second == CPU_SUBTYPE_MULTIPLE)
		return true;	// subtype wildcard
	// match subtypes, ignoring feature bits
	return ((second ^ templ.second) & ~CPU_SUBTYPE_MASK) == 0;
}


//
// MachOBase contains knowledge of the Mach-O object file format,
// but abstracts from any particular sourcing. It must be subclassed,
// and the subclass must provide the file header and commands area
// during its construction. Memory is owned by the subclass.
//
MachOBase::~MachOBase()
{ /* virtual */ }

// provide the Mach-O file header, somehow
void MachOBase::initHeader(const mach_header *header)
{
	mHeader = header;
	switch (mHeader->magic) {
	case MH_MAGIC:
		mFlip = false;
		m64 = false;
		break;
	case MH_CIGAM:
		mFlip = true;
		m64 = false;
		break;
	case MH_MAGIC_64:
		mFlip = false;
		m64 = true;
		break;
	case MH_CIGAM_64:
		mFlip = true;
		m64 = true;
		break;
	default:
		UnixError::throwMe(ENOEXEC);
	}
}

// provide the Mach-O commands section, somehow
void MachOBase::initCommands(const load_command *commands)
{
	mCommands = commands;
	mEndCommands = LowLevelMemoryUtilities::increment<load_command>(commands, flip(mHeader->sizeofcmds));
}


size_t MachOBase::headerSize() const
{
	return m64 ? sizeof(mach_header_64) : sizeof(mach_header);
}

size_t MachOBase::commandSize() const
{
	return flip(mHeader->sizeofcmds);
}


//
// Create a MachO object from an open file and a starting offset.
// We load (only) the header and load commands into memory at that time.
// Note that the offset must be relative to the start of the containing file
// (not relative to some intermediate container).
//
MachO::MachO(FileDesc fd, size_t offset, size_t length)
	: FileDesc(fd), mOffset(offset), mLength(length ? length : (fd.fileSize() - offset))
{
	size_t size = fd.read(&mHeaderBuffer, sizeof(mHeaderBuffer), mOffset);
	if (size != sizeof(mHeaderBuffer))
		UnixError::throwMe(ENOEXEC);
	this->initHeader(&mHeaderBuffer);
	size_t cmdSize = this->commandSize();
	mCommandBuffer = (load_command *)malloc(cmdSize);
	if (!mCommandBuffer)
		UnixError::throwMe();
	if (fd.read(mCommandBuffer, cmdSize, this->headerSize() + mOffset) != cmdSize)
		UnixError::throwMe(ENOEXEC);
	this->initCommands(mCommandBuffer);
}

MachO::~MachO()
{
	::free(mCommandBuffer);
}


//
// Create a MachO object that is (entirely) mapped into memory.
// The caller must ensire that the underlying mapping persists
// at least as long as our object.
//
MachOImage::MachOImage(const void *address)
{
	this->initHeader((const mach_header *)address);
	this->initCommands(LowLevelMemoryUtilities::increment<const load_command>(address, this->headerSize()));
}


//
// Locate the Mach-O image of the main program
//
MainMachOImage::MainMachOImage()
	: MachOImage(mainImageAddress())
{
}

const void *MainMachOImage::mainImageAddress()
{
	return _dyld_get_image_header(0);
}


//
// Return various header fields
//
Architecture MachOBase::architecture() const
{
	return Architecture(flip(mHeader->cputype), flip(mHeader->cpusubtype));
}

uint32_t MachOBase::type() const
{
	return flip(mHeader->filetype);
}

uint32_t MachOBase::flags() const
{
	return flip(mHeader->flags);
}


//
// Iterate through load commands
//
const load_command *MachOBase::nextCommand(const load_command *command) const
{
	using LowLevelMemoryUtilities::increment;
	command = increment<const load_command>(command, flip(command->cmdsize));
	return (command < mEndCommands) ? command : NULL;
}


//
// Find a specific load command, by command number.
// If there are multiples, returns the first one found.
//
const load_command *MachOBase::findCommand(uint32_t cmd) const
{
	for (const load_command *command = loadCommands(); command; command = nextCommand(command))
		if (flip(command->cmd) == cmd)
			return command;
	return NULL;
}


//
// Locate a segment command, by name
//	
const segment_command *MachOBase::findSegment(const char *segname) const
{
	for (const load_command *command = loadCommands(); command; command = nextCommand(command)) {
		switch (flip(command->cmd)) {
		case LC_SEGMENT:
		case LC_SEGMENT_64:
			{
				const segment_command *seg = reinterpret_cast<const segment_command *>(command);
				if (!strcmp(seg->segname, segname))
					return seg;
				break;
			}
		default:
			break;
		}
	}
	return NULL;
}

const section *MachOBase::findSection(const char *segname, const char *sectname) const
{
	using LowLevelMemoryUtilities::increment;
	if (const segment_command *seg = findSegment(segname)) {
		if (is64()) {
			const segment_command_64 *seg64 = reinterpret_cast<const segment_command_64 *>(seg);
			const section_64 *sect = increment<const section_64>(seg64 + 1, 0);
			for (unsigned n = flip(seg64->nsects); n > 0; n--, sect++) {
				if (!strcmp(sect->sectname, sectname))
					return reinterpret_cast<const section *>(sect);
			}
		} else {
			const section *sect = increment<const section>(seg + 1, 0);
			for (unsigned n = flip(seg->nsects); n > 0; n--, sect++) {
				if (!strcmp(sect->sectname, sectname))
					return sect;
			}
		}
	}
	return NULL;
}


//
// Translate a union lc_str into the string it denotes.
// Returns NULL (no exceptions) if the entry is corrupt.
//
const char *MachOBase::string(const load_command *cmd, const lc_str &str) const
{
	size_t offset = flip(str.offset);
	const char *sp = LowLevelMemoryUtilities::increment<const char>(cmd, offset);
	if (offset + strlen(sp) + 1 > flip(cmd->cmdsize))	// corrupt string reference
		return NULL;
	return sp;
}


//
// Figure out where the Code Signing information starts in the Mach-O binary image.
// The code signature is at the end of the file, and identified
// by a specially-named section. So its starting offset is also the end
// of the signable part.
// Note that the offset returned is relative to the start of the Mach-O image.
// Returns zero if not found (usually indicating that the binary was not signed).
//
const linkedit_data_command *MachOBase::findCodeSignature() const
{
	if (const load_command *cmd = findCommand(LC_CODE_SIGNATURE))
		return reinterpret_cast<const linkedit_data_command *>(cmd);
	return NULL;		// not found
}

size_t MachOBase::signingOffset() const
{
	if (const linkedit_data_command *lec = findCodeSignature())
		return flip(lec->dataoff);
	else
		return 0;
}

size_t MachOBase::signingLength() const
{
	if (const linkedit_data_command *lec = findCodeSignature())
		return flip(lec->datasize);
	else
		return 0;
}

#ifndef LC_DYLIB_CODE_SIGN_DRS
#define LC_DYLIB_CODE_SIGN_DRS 0x2B
#endif

const linkedit_data_command *MachOBase::findLibraryDependencies() const
{
	if (const load_command *cmd = findCommand(LC_DYLIB_CODE_SIGN_DRS))
		return reinterpret_cast<const linkedit_data_command *>(cmd);
	return NULL;		// not found
}


//
// Return the signing-limit length for this Mach-O binary image.
// This is the signingOffset if present, or the full length if not.
//
size_t MachO::signingExtent() const
{
	if (size_t offset = signingOffset())
		return offset;
	else
		return length();
}


//
// I/O operations
//
void MachO::seek(size_t offset)
{
	FileDesc::seek(mOffset + offset);
}

CFDataRef MachO::dataAt(size_t offset, size_t size)
{
	CFMallocData buffer(size);
	if (this->read(buffer, size, mOffset + offset) != size)
		UnixError::throwMe();
	return buffer;
}


//
// Fat (aka universal) file wrappers.
// The offset is relative to the start of the containing file.
//
Universal::Universal(FileDesc fd, off_t offset /* = 0 */)
	: FileDesc(fd), mBase(offset)
{
	union {
		fat_header header;		// if this is a fat file
		mach_header mheader;	// if this is a thin file
	};
	const size_t size = max(sizeof(header), sizeof(mheader));
	if (fd.read(&header, size, offset) != size)
		UnixError::throwMe(ENOEXEC);
	switch (header.magic) {
	case FAT_MAGIC:
	case FAT_CIGAM:
		{
			mArchCount = ntohl(header.nfat_arch);
			size_t archSize = sizeof(fat_arch) * mArchCount;
			mArchList = (fat_arch *)malloc(archSize);
			if (!mArchList)
				UnixError::throwMe();
			if (fd.read(mArchList, archSize, mBase + sizeof(header)) != archSize) {
				::free(mArchList);
				UnixError::throwMe(ENOEXEC);
			}
			for (fat_arch *arch = mArchList; arch < mArchList + mArchCount; arch++) {
				n2hi(arch->cputype);
				n2hi(arch->cpusubtype);
				n2hi(arch->offset);
				n2hi(arch->size);
				n2hi(arch->align);
			}
			secdebug("macho", "%p is a fat file with %d architectures",
				this, mArchCount);
			break;
		}
	case MH_MAGIC:
	case MH_MAGIC_64:
		mArchList = NULL;
		mArchCount = 0;
		mThinArch = Architecture(mheader.cputype, mheader.cpusubtype);
		secdebug("macho", "%p is a thin file (%s)", this, mThinArch.name());
		break;
	case MH_CIGAM:
	case MH_CIGAM_64:
		mArchList = NULL;
		mArchCount = 0;
		mThinArch = Architecture(flip(mheader.cputype), flip(mheader.cpusubtype));
		secdebug("macho", "%p is a thin file (%s)", this, mThinArch.name());
		break;
	default:
		UnixError::throwMe(ENOEXEC);
	}
}

Universal::~Universal()
{
	::free(mArchList);
}


//
// Get the "local" architecture from the fat file
// Throws ENOEXEC if not found.
//
MachO *Universal::architecture() const
{
	if (isUniversal())
		return findImage(bestNativeArch());
	else
		return new MachO(*this, mBase);
}

size_t Universal::archOffset() const
{
	if (isUniversal())
		return mBase + findArch(bestNativeArch())->offset;
	else
		return mBase;
}


//
// Get the specified architecture from the fat file
// Throws ENOEXEC if not found.
//
MachO *Universal::architecture(const Architecture &arch) const
{
	if (isUniversal())
		return findImage(arch);
	else if (mThinArch.matches(arch))
		return new MachO(*this, mBase);
	else
		UnixError::throwMe(ENOEXEC);
}

size_t Universal::archOffset(const Architecture &arch) const
{
	if (isUniversal())
		return mBase + findArch(arch)->offset;
	else if (mThinArch.matches(arch))
		return 0;
	else
		UnixError::throwMe(ENOEXEC);
}


//
// Get the architecture at a specified offset from the fat file.
// Throws an exception of the offset does not point at a Mach-O image.
//
MachO *Universal::architecture(off_t offset) const
{
	if (isUniversal())
		return new MachO(*this, offset);
	else if (offset == mBase)
		return new MachO(*this);
	else
		UnixError::throwMe(ENOEXEC);
}


//
// Locate an architecture from the fat file's list.
// Throws ENOEXEC if not found.
//
const fat_arch *Universal::findArch(const Architecture &target) const
{
	assert(isUniversal());
	const fat_arch *end = mArchList + mArchCount;
	// exact match
	for (const fat_arch *arch = mArchList; arch < end; ++arch)
		if (arch->cputype == target.cpuType()
			&& arch->cpusubtype == target.cpuSubtype())
			return arch;
	// match for generic model of main architecture
	for (const fat_arch *arch = mArchList; arch < end; ++arch)
		if (arch->cputype == target.cpuType() && arch->cpusubtype == 0)
			return arch;
	// match for any subarchitecture of the main architecture (questionable)
	for (const fat_arch *arch = mArchList; arch < end; ++arch)
		if (arch->cputype == target.cpuType())
			return arch;
	// no match
	UnixError::throwMe(ENOEXEC);	// not found	
}

MachO *Universal::findImage(const Architecture &target) const
{
	const fat_arch *arch = findArch(target);
	return new MachO(*this, mBase + arch->offset, arch->size);
}


//
// Find the best-matching architecture for this fat file.
// We pick the native architecture if it's available.
// If it contains exactly one architecture, we take that.
// Otherwise, we throw.
//
Architecture Universal::bestNativeArch() const
{
	if (isUniversal()) {
		// ask the NXArch API for our native architecture
		const Architecture native = Architecture::local();
		if (fat_arch *match = NXFindBestFatArch(native.cpuType(), native.cpuSubtype(), mArchList, mArchCount))
			return *match;
		// if the system can't figure it out, pick (arbitrarily) the first one
		return mArchList[0];
	} else
		return mThinArch;
}


//
// List all architectures from the fat file's list.
//
void Universal::architectures(Architectures &archs)
{
	if (isUniversal()) {
		for (unsigned n = 0; n < mArchCount; n++)
			archs.insert(mArchList[n]);
	} else {
		auto_ptr<MachO> macho(architecture());
		archs.insert(macho->architecture());
	}
}


//
// Quickly guess the Mach-O type of a file.
// Returns type zero if the file isn't Mach-O or Universal.
// Always looks at the start of the file, and does not change the file pointer.
//
uint32_t Universal::typeOf(FileDesc fd)
{
	mach_header header;
	int max_tries = 3;
	if (fd.read(&header, sizeof(header), 0) != sizeof(header))
		return 0;
	while (max_tries > 0) {
		switch (header.magic) {
		case MH_MAGIC:
		case MH_MAGIC_64:
			return header.filetype;
			break;
		case MH_CIGAM:
		case MH_CIGAM_64:
			return flip(header.filetype);
			break;
		case FAT_MAGIC:
		case FAT_CIGAM:
			{
				const fat_arch *arch1 =
					LowLevelMemoryUtilities::increment<fat_arch>(&header, sizeof(fat_header));
				if (fd.read(&header, sizeof(header), ntohl(arch1->offset)) != sizeof(header))
					return 0;
				max_tries--;
				continue;
			}
		default:
			return 0;
		}
	}
}


} // Security
