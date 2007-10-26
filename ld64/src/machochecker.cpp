/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*- 
 *
 * Copyright (c) 2006-2007 Apple Inc. All rights reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/stab.h>
#include <mach-o/reloc.h>
#include <mach-o/ppc/reloc.h>
#include <mach-o/x86_64/reloc.h>

#include <vector>
#include <set>
#include <ext/hash_set>

#include "MachOFileAbstraction.hpp"
#include "Architectures.hpp"


 __attribute__((noreturn))
void throwf(const char* format, ...) 
{
	va_list	list;
	char*	p;
	va_start(list, format);
	vasprintf(&p, format, list);
	va_end(list);
	
	const char*	t = p;
	throw t;
}


template <typename A>
class MachOChecker
{
public:
	static bool									validFile(const uint8_t* fileContent);
	static MachOChecker<A>*						make(const uint8_t* fileContent, uint32_t fileLength, const char* path) 
														{ return new MachOChecker<A>(fileContent, fileLength, path); }
	virtual										~MachOChecker() {}


private:
	typedef typename A::P					P;
	typedef typename A::P::E				E;
	typedef typename A::P::uint_t			pint_t;
	
	class CStringEquals
	{
	public:
		bool operator()(const char* left, const char* right) const { return (strcmp(left, right) == 0); }
	};

	typedef __gnu_cxx::hash_set<const char*, __gnu_cxx::hash<const char*>, CStringEquals>  StringSet;

												MachOChecker(const uint8_t* fileContent, uint32_t fileLength, const char* path);
	void										checkMachHeader();
	void										checkLoadCommands();
	void										checkSection(const macho_segment_command<P>* segCmd, const macho_section<P>* sect);
	uint8_t										loadCommandSizeMask();
	void										checkSymbolTable();
	void										checkIndirectSymbolTable();
	void										checkRelocations();
	void										checkExternalReloation(const macho_relocation_info<P>* reloc);
	void										checkLocalReloation(const macho_relocation_info<P>* reloc);
	pint_t										relocBase();
	bool										addressInWritableSegment(pint_t address);

	const char*									fPath;
	const macho_header<P>*						fHeader;
	uint32_t									fLength;
	const char*									fStrings;
	const char*									fStringsEnd;
	const macho_nlist<P>*						fSymbols;
	uint32_t									fSymbolCount;
	const macho_dysymtab_command<P>*			fDynamicSymbolTable;
	const uint32_t*								fIndirectTable;
	uint32_t									fIndirectTableCount;
	const macho_relocation_info<P>*				fLocalRelocations;
	uint32_t									fLocalRelocationsCount;
	const macho_relocation_info<P>*				fExternalRelocations;
	uint32_t									fExternalRelocationsCount;
	bool										fWriteableSegmentWithAddrOver4G;
	const macho_segment_command<P>*				fFirstSegment;
	const macho_segment_command<P>*				fFirstWritableSegment;
};



template <>
bool MachOChecker<ppc>::validFile(const uint8_t* fileContent)
{	
	const macho_header<P>* header = (const macho_header<P>*)fileContent;
	if ( header->magic() != MH_MAGIC )
		return false;
	if ( header->cputype() != CPU_TYPE_POWERPC )
		return false;
	switch (header->filetype()) {
		case MH_EXECUTE:
		case MH_DYLIB:
		case MH_BUNDLE:
		case MH_DYLINKER:
			return true;
	}
	return false;
}

template <>
bool MachOChecker<ppc64>::validFile(const uint8_t* fileContent)
{	
	const macho_header<P>* header = (const macho_header<P>*)fileContent;
	if ( header->magic() != MH_MAGIC_64 )
		return false;
	if ( header->cputype() != CPU_TYPE_POWERPC64 )
		return false;
	switch (header->filetype()) {
		case MH_EXECUTE:
		case MH_DYLIB:
		case MH_BUNDLE:
		case MH_DYLINKER:
			return true;
	}
	return false;
}

template <>
bool MachOChecker<x86>::validFile(const uint8_t* fileContent)
{	
	const macho_header<P>* header = (const macho_header<P>*)fileContent;
	if ( header->magic() != MH_MAGIC )
		return false;
	if ( header->cputype() != CPU_TYPE_I386 )
		return false;
	switch (header->filetype()) {
		case MH_EXECUTE:
		case MH_DYLIB:
		case MH_BUNDLE:
		case MH_DYLINKER:
			return true;
	}
	return false;
}

template <>
bool MachOChecker<x86_64>::validFile(const uint8_t* fileContent)
{	
	const macho_header<P>* header = (const macho_header<P>*)fileContent;
	if ( header->magic() != MH_MAGIC_64 )
		return false;
	if ( header->cputype() != CPU_TYPE_X86_64 )
		return false;
	switch (header->filetype()) {
		case MH_EXECUTE:
		case MH_DYLIB:
		case MH_BUNDLE:
		case MH_DYLINKER:
			return true;
	}
	return false;
}


template <> uint8_t MachOChecker<ppc>::loadCommandSizeMask()	{ return 0x03; }
template <> uint8_t MachOChecker<ppc64>::loadCommandSizeMask()	{ return 0x07; }
template <> uint8_t MachOChecker<x86>::loadCommandSizeMask()	{ return 0x03; }
template <> uint8_t MachOChecker<x86_64>::loadCommandSizeMask() { return 0x07; }


template <typename A>
MachOChecker<A>::MachOChecker(const uint8_t* fileContent, uint32_t fileLength, const char* path)
 : fHeader(NULL), fLength(fileLength), fStrings(NULL), fSymbols(NULL), fSymbolCount(0), fDynamicSymbolTable(NULL), fIndirectTableCount(0),
 fLocalRelocations(NULL),  fLocalRelocationsCount(0),  fExternalRelocations(NULL),  fExternalRelocationsCount(0),
 fWriteableSegmentWithAddrOver4G(false), fFirstSegment(NULL), fFirstWritableSegment(NULL)
{
	// sanity check
	if ( ! validFile(fileContent) )
		throw "not a mach-o file that can be checked";

	fPath = strdup(path);
	fHeader = (const macho_header<P>*)fileContent;
	
	// sanity check header
	checkMachHeader();
	
	// check load commands
	checkLoadCommands();
	
	checkIndirectSymbolTable();

	checkRelocations();
	
	checkSymbolTable();
}


template <typename A>
void MachOChecker<A>::checkMachHeader()
{
	if ( (fHeader->sizeofcmds() + sizeof(macho_header<P>)) > fLength )
		throw "sizeofcmds in mach_header is larger than file";
	
	uint32_t flags = fHeader->flags();
	const uint32_t invalidBits = MH_INCRLINK | MH_LAZY_INIT | 0xFFE00000;
	if ( flags & invalidBits )
		throw "invalid bits in mach_header flags";
	if ( (flags & MH_NO_REEXPORTED_DYLIBS) && (fHeader->filetype() != MH_DYLIB) ) 
		throw "MH_NO_REEXPORTED_DYLIBS bit of mach_header flags only valid for dylibs";
}

template <typename A>
void MachOChecker<A>::checkLoadCommands()
{
	// check that all load commands fit within the load command space file
	const uint8_t* const endOfFile = (uint8_t*)fHeader + fLength;
	const uint8_t* const endOfLoadCommands = (uint8_t*)fHeader + sizeof(macho_header<P>) + fHeader->sizeofcmds();
	const uint32_t cmd_count = fHeader->ncmds();
	const macho_load_command<P>* const cmds = (macho_load_command<P>*)((uint8_t*)fHeader + sizeof(macho_header<P>));
	const macho_load_command<P>* cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		uint32_t size = cmd->cmdsize();
		if ( (size & this->loadCommandSizeMask()) != 0 )
			throwf("load command #%d has a unaligned size", i);
		const uint8_t* endOfCmd = ((uint8_t*)cmd)+cmd->cmdsize();
		if ( endOfCmd > endOfLoadCommands )
			throwf("load command #%d extends beyond the end of the load commands", i);
		if ( endOfCmd > endOfFile )
			throwf("load command #%d extends beyond the end of the file", i);
		switch ( cmd->cmd()	) {
			case macho_segment_command<P>::CMD:
			case LC_SYMTAB:
			case LC_UNIXTHREAD:
			case LC_DYSYMTAB:
			case LC_LOAD_DYLIB:
			case LC_ID_DYLIB:
			case LC_LOAD_DYLINKER:
			case LC_ID_DYLINKER:
			case macho_routines_command<P>::CMD:
			case LC_SUB_FRAMEWORK:
			case LC_SUB_CLIENT:
			case LC_TWOLEVEL_HINTS:
			case LC_PREBIND_CKSUM:
			case LC_LOAD_WEAK_DYLIB:
			case LC_UUID:
			case LC_REEXPORT_DYLIB:
			case LC_SEGMENT_SPLIT_INFO:
				break;
			case LC_SUB_UMBRELLA:
			case LC_SUB_LIBRARY:
				if ( fHeader->flags() & MH_NO_REEXPORTED_DYLIBS )
					throw "MH_NO_REEXPORTED_DYLIBS bit of mach_header flags should not be set in an image with LC_SUB_LIBRARY or LC_SUB_UMBRELLA";
				break;
			default:
				throwf("load command #%d is an unknown kind 0x%X", i, cmd->cmd());
		}
		cmd = (const macho_load_command<P>*)endOfCmd;
	}
	
	// check segments
	cmd = cmds;
	std::vector<std::pair<pint_t, pint_t> > segmentAddressRanges;
	std::vector<std::pair<pint_t, pint_t> > segmentFileOffsetRanges;
	const macho_segment_command<P>* linkEditSegment = NULL;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		if ( cmd->cmd() == macho_segment_command<P>::CMD ) {
			const macho_segment_command<P>* segCmd = (const macho_segment_command<P>*)cmd;
			if ( segCmd->cmdsize() != (sizeof(macho_segment_command<P>) + segCmd->nsects() * sizeof(macho_section_content<P>)) )
				throw "invalid segment load command size";
				
			// see if this overlaps another segment address range
			uint64_t startAddr = segCmd->vmaddr();
			uint64_t endAddr = startAddr + segCmd->vmsize();
			for (typename std::vector<std::pair<pint_t, pint_t> >::iterator it = segmentAddressRanges.begin(); it != segmentAddressRanges.end(); ++it) {
				if ( it->first < startAddr ) {
					if ( it->second > startAddr )
						throw "overlapping segment vm addresses";
				}
				else if ( it->first > startAddr ) {
					if ( it->first < endAddr )
						throw "overlapping segment vm addresses";
				}
				else {
					throw "overlapping segment vm addresses";
				}
				segmentAddressRanges.push_back(std::make_pair<pint_t, pint_t>(startAddr, endAddr));
			}
			// see if this overlaps another segment file offset range
			uint64_t startOffset = segCmd->fileoff();
			uint64_t endOffset = startOffset + segCmd->filesize();
			for (typename std::vector<std::pair<pint_t, pint_t> >::iterator it = segmentFileOffsetRanges.begin(); it != segmentFileOffsetRanges.end(); ++it) {
				if ( it->first < startOffset ) {
					if ( it->second > startOffset )
						throw "overlapping segment file data";
				}
				else if ( it->first > startOffset ) {
					if ( it->first < endOffset )
						throw "overlapping segment file data";
				}
				else {
					throw "overlapping segment file data";
				}
				segmentFileOffsetRanges.push_back(std::make_pair<pint_t, pint_t>(startOffset, endOffset));
				// check is within file bounds
				if ( (startOffset > fLength) || (endOffset > fLength) )
					throw "segment file data is past end of file";
			}
			// verify it fits in file
			if ( startOffset > fLength )
				throw "segment fileoff does not fit in file";
			if ( endOffset > fLength )
				throw "segment fileoff+filesize does not fit in file";
				
			// keep LINKEDIT segment 
			if ( strcmp(segCmd->segname(), "__LINKEDIT") == 0 )
				linkEditSegment = segCmd;

			// cache interesting segments
			if ( fFirstSegment == NULL )
				fFirstSegment = segCmd;
			if ( (segCmd->initprot() & VM_PROT_WRITE) != 0 ) {
				if ( fFirstWritableSegment == NULL )
					fFirstWritableSegment = segCmd;
				if ( segCmd->vmaddr() > 0x100000000ULL )
					fWriteableSegmentWithAddrOver4G = true;
			}
	
			// check section ranges
			const macho_section<P>* const sectionsStart = (macho_section<P>*)((char*)segCmd + sizeof(macho_segment_command<P>));
			const macho_section<P>* const sectionsEnd = &sectionsStart[segCmd->nsects()];
			for(const macho_section<P>* sect = sectionsStart; sect < sectionsEnd; ++sect) {
				// check all sections are within segment
				if ( sect->addr() < startAddr )
					throwf("section %s vm address not within segment", sect->sectname());
				if ( (sect->addr()+sect->size()) > endAddr )
					throwf("section %s vm address not within segment", sect->sectname());
				if ( ((sect->flags() &SECTION_TYPE) != S_ZEROFILL) && (segCmd->filesize() != 0) ) {
					if ( sect->offset() < startOffset )
						throwf("section %s file offset not within segment", sect->sectname());
					if ( (sect->offset()+sect->size()) > endOffset )
						throwf("section %s file offset not within segment", sect->sectname());
				}	
				checkSection(segCmd, sect);
			}
		}
		cmd = (const macho_load_command<P>*)(((uint8_t*)cmd)+cmd->cmdsize());
	}
	
	// verify there was a LINKEDIT segment
	if ( linkEditSegment == NULL )
		throw "no __LINKEDIT segment";
	
	// checks for executables
	bool isStaticExecutable = false;
	if ( fHeader->filetype() == MH_EXECUTE ) {
		isStaticExecutable = true;
		cmd = cmds;
		for (uint32_t i = 0; i < cmd_count; ++i) {
			switch ( cmd->cmd() ) {
				case LC_LOAD_DYLINKER:
					// the existence of a dyld load command makes a executable dynamic
					isStaticExecutable = false;
					break;
			}
			cmd = (const macho_load_command<P>*)(((uint8_t*)cmd)+cmd->cmdsize());
		}
		if ( isStaticExecutable ) {
			if ( fHeader->flags() != MH_NOUNDEFS )
				throw "invalid bits in mach_header flags for static executable";
		}
	}

	// check LC_SYMTAB, LC_DYSYMTAB, and LC_SEGMENT_SPLIT_INFO
	cmd = cmds;
	bool foundDynamicSymTab = false;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		switch ( cmd->cmd() ) {
			case LC_SYMTAB:
				{
					const macho_symtab_command<P>* symtab = (macho_symtab_command<P>*)cmd;
					fSymbolCount = symtab->nsyms();
					fSymbols = (const macho_nlist<P>*)((char*)fHeader + symtab->symoff());
					if ( symtab->symoff() < linkEditSegment->fileoff() )
						throw "symbol table not in __LINKEDIT";
					if ( (symtab->symoff() + fSymbolCount*sizeof(macho_nlist<P>*)) > (linkEditSegment->fileoff()+linkEditSegment->filesize()) )
						throw "symbol table end not in __LINKEDIT";
					if ( (symtab->symoff() % sizeof(pint_t)) != 0 )
						throw "symbol table start not pointer aligned";
					fStrings = (char*)fHeader + symtab->stroff();
					fStringsEnd = fStrings + symtab->strsize();
					if ( symtab->stroff() < linkEditSegment->fileoff() )
						throw "string pool not in __LINKEDIT";
					if ( (symtab->stroff()+symtab->strsize()) > (linkEditSegment->fileoff()+linkEditSegment->filesize()) )
						throw "string pool extends beyond __LINKEDIT";
					if ( (symtab->stroff() % 4) != 0 ) // work around until rdar://problem/4737991 is fixed
						throw "string pool start not pointer aligned";
					if ( (symtab->strsize() % sizeof(pint_t)) != 0 )	
						throw "string pool size not a multiple of pointer size";
				}
				break;
			case LC_DYSYMTAB:
				{
					if ( isStaticExecutable )
						throw "LC_DYSYMTAB should not be used in static executable";
					foundDynamicSymTab = true;
					fDynamicSymbolTable = (struct macho_dysymtab_command<P>*)cmd;
					fIndirectTable = (uint32_t*)((char*)fHeader + fDynamicSymbolTable->indirectsymoff());
					fIndirectTableCount = fDynamicSymbolTable->nindirectsyms();
					if ( fIndirectTableCount != 0  ) {
						if ( fDynamicSymbolTable->indirectsymoff() < linkEditSegment->fileoff() )
							throw "indirect symbol table not in __LINKEDIT";
						if ( (fDynamicSymbolTable->indirectsymoff()+fIndirectTableCount*8) > (linkEditSegment->fileoff()+linkEditSegment->filesize()) )
							throw "indirect symbol table not in __LINKEDIT";
						if ( (fDynamicSymbolTable->indirectsymoff() % sizeof(pint_t)) != 0 )
							throw "indirect symbol table not pointer aligned";
					}
					fLocalRelocationsCount = fDynamicSymbolTable->nlocrel();
					if ( fLocalRelocationsCount != 0 ) {
						fLocalRelocations = (const macho_relocation_info<P>*)((char*)fHeader + fDynamicSymbolTable->locreloff());
						if ( fDynamicSymbolTable->locreloff() < linkEditSegment->fileoff() )
							throw "local relocations not in __LINKEDIT";
						if ( (fDynamicSymbolTable->locreloff()+fLocalRelocationsCount*sizeof(macho_relocation_info<P>)) > (linkEditSegment->fileoff()+linkEditSegment->filesize()) )
							throw "local relocations not in __LINKEDIT";
						if ( (fDynamicSymbolTable->locreloff() % sizeof(pint_t)) != 0 )
							throw "local relocations table not pointer aligned";
					}
					fExternalRelocationsCount = fDynamicSymbolTable->nextrel();
					if ( fExternalRelocationsCount != 0 ) {
						fExternalRelocations = (const macho_relocation_info<P>*)((char*)fHeader + fDynamicSymbolTable->extreloff());
						if ( fDynamicSymbolTable->extreloff() < linkEditSegment->fileoff() )
							throw "external relocations not in __LINKEDIT";
						if ( (fDynamicSymbolTable->extreloff()+fExternalRelocationsCount*sizeof(macho_relocation_info<P>)) > (linkEditSegment->fileoff()+linkEditSegment->filesize()) )
							throw "external relocations not in __LINKEDIT";
						if ( (fDynamicSymbolTable->extreloff() % sizeof(pint_t)) != 0 )
							throw "external relocations table not pointer aligned";
					}
				}
				break;
			case LC_SEGMENT_SPLIT_INFO:
				{
					if ( isStaticExecutable )
						throw "LC_SEGMENT_SPLIT_INFO should not be used in static executable";
					const macho_linkedit_data_command<P>* info = (struct macho_linkedit_data_command<P>*)cmd;
					if ( info->dataoff() < linkEditSegment->fileoff() )
						throw "split seg info not in __LINKEDIT";
					if ( (info->dataoff()+info->datasize()) > (linkEditSegment->fileoff()+linkEditSegment->filesize()) )
						throw "split seg info not in __LINKEDIT";
					if ( (info->dataoff() % sizeof(pint_t)) != 0 )
						throw "split seg info table not pointer aligned";
					if ( (info->datasize() % sizeof(pint_t)) != 0 )
						throw "split seg info size not a multiple of pointer size";
				}
				break;
		}
		cmd = (const macho_load_command<P>*)(((uint8_t*)cmd)+cmd->cmdsize());
	}
	if ( !isStaticExecutable && !foundDynamicSymTab )
		throw "missing dynamic symbol table";
	if ( fStrings == NULL )
		throw "missing symbol table";
		
}

template <typename A>
void MachOChecker<A>::checkSection(const macho_segment_command<P>* segCmd, const macho_section<P>* sect)
{
	uint8_t sectionType = (sect->flags() & SECTION_TYPE);
	if ( sectionType == S_ZEROFILL ) {
		if ( sect->offset() != 0 )
			throwf("section offset should be zero for zero-fill section %s", sect->sectname());
	}
	
	// more section tests here
}

template <typename A>
void MachOChecker<A>::checkIndirectSymbolTable()
{
	const macho_load_command<P>* const cmds = (macho_load_command<P>*)((uint8_t*)fHeader + sizeof(macho_header<P>));
	const uint32_t cmd_count = fHeader->ncmds();
	const macho_load_command<P>* cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		if ( cmd->cmd() == macho_segment_command<P>::CMD ) {
			const macho_segment_command<P>* segCmd = (const macho_segment_command<P>*)cmd;
			const macho_section<P>* const sectionsStart = (macho_section<P>*)((char*)segCmd + sizeof(macho_segment_command<P>));
			const macho_section<P>* const sectionsEnd = &sectionsStart[segCmd->nsects()];
			for(const macho_section<P>* sect = sectionsStart; sect < sectionsEnd; ++sect) {
				// make sure all magic sections that use indirect symbol table fit within it
				uint32_t start = 0;
				uint32_t elementSize = 0;
				switch ( sect->flags() & SECTION_TYPE ) {
					case S_SYMBOL_STUBS:
						elementSize = sect->reserved2();
						start = sect->reserved1();
						break;
					case S_LAZY_SYMBOL_POINTERS:
					case S_NON_LAZY_SYMBOL_POINTERS:
						elementSize = sizeof(pint_t);
						start = sect->reserved1();
						break;
				}
				if ( elementSize != 0 ) {
					uint32_t count = sect->size() / elementSize;
					if ( (count*elementSize) != sect->size() )
						throwf("%s section size is not an even multiple of element size", sect->sectname());
					if ( (start+count) > fIndirectTableCount )
						throwf("%s section references beyond end of indirect symbol table (%d > %d)", sect->sectname(), start+count, fIndirectTableCount );
				}
			}
		}
		cmd = (const macho_load_command<P>*)(((uint8_t*)cmd)+cmd->cmdsize());
	}
}


template <typename A>
void MachOChecker<A>::checkSymbolTable()
{
	// verify no duplicate external symbol names
	if ( fDynamicSymbolTable != NULL ) {
		StringSet externalNames;
		const macho_nlist<P>* const	exportedStart = &fSymbols[fDynamicSymbolTable->iextdefsym()];
		const macho_nlist<P>* const exportedEnd = &exportedStart[fDynamicSymbolTable->nextdefsym()];
		for(const macho_nlist<P>* p = exportedStart; p < exportedEnd; ++p) {
			const char* symName = &fStrings[p->n_strx()];
			if ( externalNames.find(symName) != externalNames.end() )
				throwf("duplicate external symbol: %s", symName);
			externalNames.insert(symName);
		}
	}
}


template <>
ppc::P::uint_t MachOChecker<ppc>::relocBase()
{
	if ( fHeader->flags() & MH_SPLIT_SEGS )
		return fFirstWritableSegment->vmaddr();
	else
		return fFirstSegment->vmaddr();
}

template <>
ppc64::P::uint_t MachOChecker<ppc64>::relocBase()
{
	if ( fWriteableSegmentWithAddrOver4G ) 
		return fFirstWritableSegment->vmaddr();
	else
		return fFirstSegment->vmaddr();
}

template <>
x86::P::uint_t MachOChecker<x86>::relocBase()
{
	if ( fHeader->flags() & MH_SPLIT_SEGS )
		return fFirstWritableSegment->vmaddr();
	else
		return fFirstSegment->vmaddr();
}

template <>
x86_64::P::uint_t MachOChecker<x86_64>::relocBase()
{
	// check for split-seg
	return fFirstWritableSegment->vmaddr();
}


template <typename A>
bool MachOChecker<A>::addressInWritableSegment(pint_t address)
{
	const macho_load_command<P>* const cmds = (macho_load_command<P>*)((uint8_t*)fHeader + sizeof(macho_header<P>));
	const uint32_t cmd_count = fHeader->ncmds();
	const macho_load_command<P>* cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		if ( cmd->cmd() == macho_segment_command<P>::CMD ) {
			const macho_segment_command<P>* segCmd = (const macho_segment_command<P>*)cmd;
			if ( (segCmd->initprot() & VM_PROT_WRITE) != 0 ) {
				if ( (address >= segCmd->vmaddr()) && (address < segCmd->vmaddr()+segCmd->vmsize()) )
					return true;
			}
		}
		cmd = (const macho_load_command<P>*)(((uint8_t*)cmd)+cmd->cmdsize());
	}
	return false;
}


template <>
void MachOChecker<ppc>::checkExternalReloation(const macho_relocation_info<P>* reloc)
{
	if ( reloc->r_length() != 2 ) 
		throw "bad external relocation length";
	if ( reloc->r_type() != GENERIC_RELOC_VANILLA ) 
		throw "unknown external relocation type";
	if ( reloc->r_pcrel() != 0 ) 
		throw "bad external relocation pc_rel";
	if ( reloc->r_extern() == 0 )
		throw "local relocation found with external relocations";
	if ( ! this->addressInWritableSegment(reloc->r_address() + this->relocBase()) )
		throw "external relocation address not in writable segment";
	// FIX: check r_symbol
}

template <>
void MachOChecker<ppc64>::checkExternalReloation(const macho_relocation_info<P>* reloc)
{
	if ( reloc->r_length() != 3 ) 
		throw "bad external relocation length";
	if ( reloc->r_type() != GENERIC_RELOC_VANILLA ) 
		throw "unknown external relocation type";
	if ( reloc->r_pcrel() != 0 ) 
		throw "bad external relocation pc_rel";
	if ( reloc->r_extern() == 0 )
		throw "local relocation found with external relocations";
	if ( ! this->addressInWritableSegment(reloc->r_address() + this->relocBase()) )
		throw "external relocation address not in writable segment";
	// FIX: check r_symbol
}

template <>
void MachOChecker<x86>::checkExternalReloation(const macho_relocation_info<P>* reloc)
{
	if ( reloc->r_length() != 2 ) 
		throw "bad external relocation length";
	if ( reloc->r_type() != GENERIC_RELOC_VANILLA ) 
		throw "unknown external relocation type";
	if ( reloc->r_pcrel() != 0 ) 
		throw "bad external relocation pc_rel";
	if ( reloc->r_extern() == 0 )
		throw "local relocation found with external relocations";
	if ( ! this->addressInWritableSegment(reloc->r_address() + this->relocBase()) )
		throw "external relocation address not in writable segment";
	// FIX: check r_symbol
}


template <>
void MachOChecker<x86_64>::checkExternalReloation(const macho_relocation_info<P>* reloc)
{
	if ( reloc->r_length() != 3 ) 
		throw "bad external relocation length";
	if ( reloc->r_type() != X86_64_RELOC_UNSIGNED ) 
		throw "unknown external relocation type";
	if ( reloc->r_pcrel() != 0 ) 
		throw "bad external relocation pc_rel";
	if ( reloc->r_extern() == 0 ) 
		throw "local relocation found with external relocations";
	if ( ! this->addressInWritableSegment(reloc->r_address() + this->relocBase()) )
		throw "exernal relocation address not in writable segment";
	// FIX: check r_symbol
}

template <>
void MachOChecker<ppc>::checkLocalReloation(const macho_relocation_info<P>* reloc)
{
	if ( reloc->r_address() & R_SCATTERED ) {
		// scattered
		const macho_scattered_relocation_info<P>* sreloc = (const macho_scattered_relocation_info<P>*)reloc;
		// FIX
	
	}
	else {
		// FIX
		if ( ! this->addressInWritableSegment(reloc->r_address() + this->relocBase()) )
			throw "local relocation address not in writable segment";
	}
}


template <>
void MachOChecker<ppc64>::checkLocalReloation(const macho_relocation_info<P>* reloc)
{
	if ( reloc->r_length() != 3 ) 
		throw "bad local relocation length";
	if ( reloc->r_type() != GENERIC_RELOC_VANILLA ) 
		throw "unknown local relocation type";
	if ( reloc->r_pcrel() != 0 ) 
		throw "bad local relocation pc_rel";
	if ( reloc->r_extern() != 0 ) 
		throw "external relocation found with local relocations";
	if ( ! this->addressInWritableSegment(reloc->r_address() + this->relocBase()) )
		throw "local relocation address not in writable segment";
}

template <>
void MachOChecker<x86>::checkLocalReloation(const macho_relocation_info<P>* reloc)
{
	// FIX
}

template <>
void MachOChecker<x86_64>::checkLocalReloation(const macho_relocation_info<P>* reloc)
{
	if ( reloc->r_length() != 3 ) 
		throw "bad local relocation length";
	if ( reloc->r_type() != X86_64_RELOC_UNSIGNED ) 
		throw "unknown local relocation type";
	if ( reloc->r_pcrel() != 0 ) 
		throw "bad local relocation pc_rel";
	if ( reloc->r_extern() != 0 ) 
		throw "external relocation found with local relocations";
	if ( ! this->addressInWritableSegment(reloc->r_address() + this->relocBase()) )
		throw "local relocation address not in writable segment";
}



template <typename A>
void MachOChecker<A>::checkRelocations()
{
	// external relocations should be sorted to minimize dyld symbol lookups
	// therefore every reloc with the same r_symbolnum value should be contiguous 
	std::set<uint32_t> previouslySeenSymbolIndexes;
	uint32_t lastSymbolIndex = 0xFFFFFFFF;
	const macho_relocation_info<P>* const externRelocsEnd = &fExternalRelocations[fExternalRelocationsCount];
	for (const macho_relocation_info<P>* reloc = fExternalRelocations; reloc < externRelocsEnd; ++reloc) {
		this->checkExternalReloation(reloc);
		if ( reloc->r_symbolnum() != lastSymbolIndex ) {
			if ( previouslySeenSymbolIndexes.count(reloc->r_symbolnum()) != 0 )
				throw "external relocations not sorted";
			previouslySeenSymbolIndexes.insert(lastSymbolIndex);
			lastSymbolIndex = reloc->r_symbolnum();
		}
	}
	
	const macho_relocation_info<P>* const localRelocsEnd = &fLocalRelocations[fLocalRelocationsCount];
	for (const macho_relocation_info<P>* reloc = fLocalRelocations; reloc < localRelocsEnd; ++reloc) {
		this->checkLocalReloation(reloc);
	}
}


static void check(const char* path)
{
	struct stat stat_buf;
	
	try {
		int fd = ::open(path, O_RDONLY, 0);
		if ( fd == -1 )
			throw "cannot open file";
		::fstat(fd, &stat_buf);
		uint32_t length = stat_buf.st_size;
		uint8_t* p = (uint8_t*)::mmap(NULL, stat_buf.st_size, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0);
		if ( p == ((uint8_t*)(-1)) )
			throw "cannot map file";
		::close(fd);
		const mach_header* mh = (mach_header*)p;
		if ( mh->magic == OSSwapBigToHostInt32(FAT_MAGIC) ) {
			const struct fat_header* fh = (struct fat_header*)p;
			const struct fat_arch* archs = (struct fat_arch*)(p + sizeof(struct fat_header));
			for (unsigned long i=0; i < OSSwapBigToHostInt32(fh->nfat_arch); ++i) {
				size_t offset = OSSwapBigToHostInt32(archs[i].offset);
				size_t size = OSSwapBigToHostInt32(archs[i].size);
				unsigned int cputype = OSSwapBigToHostInt32(archs[i].cputype);

				switch(cputype) {
				case CPU_TYPE_POWERPC:
					if ( MachOChecker<ppc>::validFile(p + offset) )
						MachOChecker<ppc>::make(p + offset, size, path);
					else
						throw "in universal file, ppc slice does not contain ppc mach-o";
					break;
				case CPU_TYPE_I386:
					if ( MachOChecker<x86>::validFile(p + offset) )
						MachOChecker<x86>::make(p + offset, size, path);
					else
						throw "in universal file, i386 slice does not contain i386 mach-o";
					break;
				case CPU_TYPE_POWERPC64:
					if ( MachOChecker<ppc64>::validFile(p + offset) )
						MachOChecker<ppc64>::make(p + offset, size, path);
					else
						throw "in universal file, ppc64 slice does not contain ppc64 mach-o";
					break;
				case CPU_TYPE_X86_64:
					if ( MachOChecker<x86_64>::validFile(p + offset) )
						MachOChecker<x86_64>::make(p + offset, size, path);
					else
						throw "in universal file, x86_64 slice does not contain x86_64 mach-o";
					break;
				default:
						throwf("in universal file, unknown architecture slice 0x%x\n", cputype);
				}
			}
		}
		else if ( MachOChecker<x86>::validFile(p) ) {
			MachOChecker<x86>::make(p, length, path);
		}
		else if ( MachOChecker<ppc>::validFile(p) ) {
			MachOChecker<ppc>::make(p, length, path);
		}
		else if ( MachOChecker<ppc64>::validFile(p) ) {
			MachOChecker<ppc64>::make(p, length, path);
		}
		else if ( MachOChecker<x86_64>::validFile(p) ) {
			MachOChecker<x86_64>::make(p, length, path);
		}
		else {
			throw "not a known file type";
		}
	}
	catch (const char* msg) {
		throwf("%s in %s", msg, path);
	}
}


int main(int argc, const char* argv[])
{
	try {
		for(int i=1; i < argc; ++i) {
			const char* arg = argv[i];
			if ( arg[0] == '-' ) {
				if ( strcmp(arg, "-no_content") == 0 ) {
					
				}
				else {
					throwf("unknown option: %s\n", arg);
				}
			}
			else {
				check(arg);
			}
		}
	}
	catch (const char* msg) {
		fprintf(stderr, "machocheck failed: %s\n", msg);
		return 1;
	}
	
	return 0;
}



