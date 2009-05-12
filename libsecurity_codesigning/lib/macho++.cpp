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
#include <mach/machine.h>

namespace Security {


//
// Architecture values
//
Architecture::Architecture(const fat_arch &arch)
	: pair<cpu_type_t, cpu_subtype_t>(arch.cputype, arch.cpusubtype)
{
}


//
// The local architecture (on demand; cached)
//
struct LocalArch {
	const NXArchInfo *arch;
	LocalArch() { arch = NXGetLocalArchInfo(); }
};
static ModuleNexus<LocalArch> localArch;

Architecture Architecture::local()
{
	const NXArchInfo &local = *localArch().arch;
	return Architecture(local.cputype, local.cpusubtype);
}


#define CPU_SUBTYPE_ARM_V7		((cpu_subtype_t) 9)

//
// Translate between names and numbers
//
static const char *uob(char *s) { if (*s != 'a') for (char *p = s; *p; p++) *p ^= 0x77; return s; }

const char *Architecture::name() const
{
	if (const NXArchInfo *info = NXGetArchInfoFromCpuType(cpuType(), cpuSubtype()))
		return info->name;
	else if (cpuType() == CPU_TYPE_ARM)	 {	// work-around for non-ARM Leopard systems
		static char arm5[] = "\026\005\032\001B";
		static char arm6[] = "\026\005\032\001A";
		static char arm7[] = "\026\005\032\001@";
		static char arm[] = "\026\005\032";
		if (cpuSubtype() == CPU_SUBTYPE_ARM_V5TEJ)
			return uob(arm5);
		else if (cpuSubtype() == CPU_SUBTYPE_ARM_V6)
			return uob(arm6);
		else if (cpuSubtype() == CPU_SUBTYPE_ARM_V7)
			return uob(arm7);
		else
			return uob(arm);
	} else
		return NULL;
}


//
// Create a MachO object from an open file and a starting offset.
// We load (only) the header and load commands into memory at that time.
//
MachO::MachO(FileDesc fd, size_t offset, size_t length)
	: FileDesc(fd), mOffset(offset), mLength(length ? length : (fd.fileSize() - offset))
{
	size_t size = fd.read(&mHeader, sizeof(mHeader), mOffset);
	if (size != sizeof(mHeader))
		UnixError::throwMe(ENOEXEC);
	switch (mHeader.magic) {
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
	
	size_t cmdSize = flip(mHeader.sizeofcmds);
	size_t cmdStart = m64 ? sizeof(mach_header_64) : sizeof(mach_header);
	mCommands = (load_command *)malloc(cmdSize);
	if (!mCommands)
		UnixError::throwMe();
	if (fd.read(mCommands, cmdSize, cmdStart + mOffset) != cmdSize)
		UnixError::throwMe(ENOEXEC);
	mEndCommands = LowLevelMemoryUtilities::increment<load_command>(mCommands, cmdSize);
	secdebug("macho", "%p created fd=%d offset=0x%zx size=0x%zx %s%s %d command(s)",
		this, this->fd(), mOffset, mLength, mFlip ? " flipped" : "", m64 ? " 64-bit" : "",
		flip(mHeader.ncmds));
}


//
// Destroy a MachO.
// Note that we don't close the file descriptor.
//
MachO::~MachO()
{
	secdebug("macho", "%p destroyed", this);
	::free(mCommands);
}


//
// Return various header fields
//
Architecture MachO::architecture() const
{
	return Architecture(flip(mHeader.cputype), flip(mHeader.cpusubtype));
}

uint32_t MachO::type() const
{
	return flip(mHeader.filetype);
}

uint32_t MachO::flags() const
{
	return flip(mHeader.flags);
}


//
// Iterate through load commands
//
const load_command *MachO::nextCommand(const load_command *command) const
{
	using LowLevelMemoryUtilities::increment;
	command = increment<const load_command>(command, flip(command->cmdsize));
	return (command < mEndCommands) ? command : NULL;
}


//
// Locate a segment command, by name
//	
const segment_command *MachO::findSegment(const char *segname) const
{
	for (const load_command *command = loadCommands(); command; command = nextCommand(command)) {
		if (flip(command->cmd) == LC_SEGMENT) {
			const segment_command *seg = reinterpret_cast<const segment_command *>(command);
			if (!strcmp(seg->segname, segname))
				return seg;
		}
	}
	return NULL;
}

const section *MachO::findSection(const char *segname, const char *sectname) const
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
// Figure out where the Code Signing information starts in the Mach-O binary image.
// The code signature is at the end of the file, and identified
// by a specially-named section. So its starting offset is also the end
// of the signable part.
// Note that the offset returned is relative to the start of the Mach-O image.
// Returns zero if not found (usually indicating that the binary was not signed).
//
const linkedit_data_command *MachO::findCodeSignature() const
{
	for (const load_command *cmd = loadCommands(); cmd; cmd = nextCommand(cmd))
		if (flip(cmd->cmd) == LC_CODE_SIGNATURE)
			return reinterpret_cast<const linkedit_data_command *>(cmd);
	return NULL;		// not found
}

size_t MachO::signingOffset() const
{
	if (const linkedit_data_command *lec = findCodeSignature())
		return flip(lec->dataoff);
	else
		return 0;
}

size_t MachO::signingLength() const
{
	if (const linkedit_data_command *lec = findCodeSignature())
		return flip(lec->datasize);
	else
		return 0;
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
// Fat (aka universal) file wrappers
//
Universal::Universal(FileDesc fd)
	: FileDesc(fd)
{
	union {
		fat_header header;		// if this is a fat file
		mach_header mheader;	// if this is a thin file
	};
	const size_t size = max(sizeof(header), sizeof(mheader));
	if (fd.read(&header, size, 0) != size)
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
			if (fd.read(mArchList, archSize, sizeof(header)) != archSize) {
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
		return new MachO(*this);
}

size_t Universal::archOffset() const
{
	if (isUniversal())
		return findArch(bestNativeArch())->offset;
	else
		return 0;
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
		return new MachO(*this);
	else
		UnixError::throwMe(ENOEXEC);
}

size_t Universal::archOffset(const Architecture &arch) const
{
	if (isUniversal())
		return findArch(arch)->offset;
	else if (mThinArch.matches(arch))
		return 0;
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
	// match for any subarchitecture of the main architeture (questionable)
	for (const fat_arch *arch = mArchList; arch < end; ++arch)
		if (arch->cputype == target.cpuType())
			return arch;
	// no match
	UnixError::throwMe(ENOEXEC);	// not found	
}

MachO *Universal::findImage(const Architecture &target) const
{
	const fat_arch *arch = findArch(target);
	return new MachO(*this, arch->offset, arch->size);
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
// Does not reposition the file.
//
uint32_t Universal::typeOf(FileDesc fd)
{
	mach_header header;
	if (fd.read(&header, sizeof(header), 0) != sizeof(header))
		return false;
	for (;;) {
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
				continue;
			}
		default:
			return 0;
		}
	}
}


} // Security
