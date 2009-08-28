/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*- 
 *
 * Copyright (c) 2008 Apple Inc. All rights reserved.
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
#include <errno.h>

#include <vector>
#include <set>
#include <ext/hash_set>

#include "MachOFileAbstraction.hpp"
#include "Architectures.hpp"
#include "MachOTrie.hpp"

static bool printRebase = false;
static bool printBind = false;
static bool printWeakBind = false;
static bool printLazyBind = false;
static bool printOpcodes = false;
static bool printExport = false;
static bool printExportGraph = false;
static cpu_type_t	sPreferredArch = CPU_TYPE_I386;


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
class DyldInfoPrinter
{
public:
	static bool									validFile(const uint8_t* fileContent);
	static DyldInfoPrinter<A>*					make(const uint8_t* fileContent, uint32_t fileLength, const char* path) 
														{ return new DyldInfoPrinter<A>(fileContent, fileLength, path); }
	virtual										~DyldInfoPrinter() {}


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

												DyldInfoPrinter(const uint8_t* fileContent, uint32_t fileLength, const char* path);
	void										printRebaseInfo();
	void										printRebaseInfoOpcodes();
	void										printBindingInfo();
	void										printWeakBindingInfo();
	void										printLazyBindingInfo();
	void										printBindingInfoOpcodes(bool weakBinding);
	void										printWeakBindingInfoOpcodes();
	void										printLazyBindingOpcodes();
	void										printExportInfo();
	void										printExportInfoGraph();
	void										processExportNode(const uint8_t* const start, const uint8_t* p, const uint8_t* const end, 
																char* cummulativeString, int curStrOffset);
	void										processExportGraphNode(const uint8_t* const start, const uint8_t* const end,  
																	const uint8_t* parent, const uint8_t* p,
																	char* cummulativeString, int curStrOffset);
	const char*									rebaseTypeName(uint8_t type);
	const char*									bindTypeName(uint8_t type);
	pint_t										segStartAddress(uint8_t segIndex);
	const char*									segmentName(uint8_t segIndex);
	const char*									sectionName(uint8_t segIndex, pint_t address);
	const char*									getSegAndSectName(uint8_t segIndex, pint_t address);
	const char*									ordinalName(int libraryOrdinal);

		
	const char*									fPath;
	const macho_header<P>*						fHeader;
	uint64_t									fLength;
	const char*									fStrings;
	const char*									fStringsEnd;
	const macho_nlist<P>*						fSymbols;
	uint32_t									fSymbolCount;
	const macho_dyld_info_command<P>*			fInfo;
	uint64_t									fBaseAddress;
	std::vector<const macho_segment_command<P>*>fSegments;
	std::vector<const char*>					fDylibs;
};



template <>
bool DyldInfoPrinter<ppc>::validFile(const uint8_t* fileContent)
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
bool DyldInfoPrinter<ppc64>::validFile(const uint8_t* fileContent)
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
bool DyldInfoPrinter<x86>::validFile(const uint8_t* fileContent)
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
bool DyldInfoPrinter<x86_64>::validFile(const uint8_t* fileContent)
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

template <>
bool DyldInfoPrinter<arm>::validFile(const uint8_t* fileContent)
{	
	const macho_header<P>* header = (const macho_header<P>*)fileContent;
	if ( header->magic() != MH_MAGIC )
		return false;
	if ( header->cputype() != CPU_TYPE_ARM )
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


template <typename A>
DyldInfoPrinter<A>::DyldInfoPrinter(const uint8_t* fileContent, uint32_t fileLength, const char* path)
 : fHeader(NULL), fLength(fileLength), 
   fStrings(NULL), fStringsEnd(NULL), fSymbols(NULL), fSymbolCount(0), fInfo(NULL), fBaseAddress(0)
{
	// sanity check
	if ( ! validFile(fileContent) )
		throw "not a mach-o file that can be checked";

	fPath = strdup(path);
	fHeader = (const macho_header<P>*)fileContent;
	
	// get LC_DYLD_INFO
	const uint8_t* const endOfFile = (uint8_t*)fHeader + fLength;
	const uint8_t* const endOfLoadCommands = (uint8_t*)fHeader + sizeof(macho_header<P>) + fHeader->sizeofcmds();
	const uint32_t cmd_count = fHeader->ncmds();
	const macho_load_command<P>* const cmds = (macho_load_command<P>*)((uint8_t*)fHeader + sizeof(macho_header<P>));
	const macho_load_command<P>* cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		uint32_t size = cmd->cmdsize();
		const uint8_t* endOfCmd = ((uint8_t*)cmd)+cmd->cmdsize();
		if ( endOfCmd > endOfLoadCommands )
			throwf("load command #%d extends beyond the end of the load commands", i);
		if ( endOfCmd > endOfFile )
			throwf("load command #%d extends beyond the end of the file", i);
		switch ( cmd->cmd() ) {
			case LC_DYLD_INFO:
			case LC_DYLD_INFO_ONLY:
				fInfo = (macho_dyld_info_command<P>*)cmd;
				break;
			case macho_segment_command<P>::CMD:
				{
				const macho_segment_command<P>* segCmd = (const macho_segment_command<P>*)cmd;
				fSegments.push_back(segCmd);
				if ( (segCmd->fileoff() == 0) && (segCmd->filesize() != 0) )
					fBaseAddress = segCmd->vmaddr();
				}
				break;
			case LC_LOAD_DYLIB:
			case LC_LOAD_WEAK_DYLIB:
			case LC_REEXPORT_DYLIB:
			case LC_LAZY_LOAD_DYLIB:
				{
				const macho_dylib_command<P>* dylib  = (macho_dylib_command<P>*)cmd;
				const char* lastSlash = strrchr(dylib->name(), '/');
				const char* leafName = (lastSlash != NULL) ? lastSlash+1 : dylib->name();
				const char* firstDot = strchr(leafName, '.');
				if ( firstDot != NULL ) {
					char* t = strdup(leafName);
					t[firstDot-leafName] = '\0';
					fDylibs.push_back(t);
				}
				else {
					fDylibs.push_back(leafName);
				}
				}
				break;
		}
		cmd = (const macho_load_command<P>*)endOfCmd;
	}
	
	if ( printRebase )
		printRebaseInfo();
	if ( printBind ) 
		printBindingInfo();
	if ( printWeakBind ) 
		printWeakBindingInfo();
	if ( printLazyBind )
		printLazyBindingInfo();
	if ( printExport )
		printExportInfo();
	if ( printOpcodes ) {
		printRebaseInfoOpcodes();
		printBindingInfoOpcodes(false);
		printBindingInfoOpcodes(true);
		printLazyBindingOpcodes();
	}
	if ( printExportGraph )
		printExportInfoGraph();
}

static uint64_t read_uleb128(const uint8_t*& p, const uint8_t* end)
{
	uint64_t result = 0;
	int		 bit = 0;
	do {
		if (p == end)
			throwf("malformed uleb128");

		uint64_t slice = *p & 0x7f;

		if (bit >= 64 || slice << bit >> bit != slice)
			throwf("uleb128 too big");
		else {
			result |= (slice << bit);
			bit += 7;
		}
	} 
	while (*p++ & 0x80);
	return result;
}

static int64_t read_sleb128(const uint8_t*& p, const uint8_t* end)
{
	int64_t result = 0;
	int bit = 0;
	uint8_t byte;
	do {
		if (p == end)
			throwf("malformed sleb128");
		byte = *p++;
		result |= ((byte & 0x7f) << bit);
		bit += 7;
	} while (byte & 0x80);
	// sign extend negative numbers
	if ( (byte & 0x40) != 0 )
		result |= (-1LL) << bit;
	return result;
}


template <typename A>
const char* DyldInfoPrinter<A>::rebaseTypeName(uint8_t type)
{
	switch (type ){
		case REBASE_TYPE_POINTER:
			return "pointer";
		case REBASE_TYPE_TEXT_ABSOLUTE32:
			return "text abs32";
		case REBASE_TYPE_TEXT_PCREL32:
			return "text rel32";
	}
	return "!!unknown!!";
}


template <typename A>
const char* DyldInfoPrinter<A>::bindTypeName(uint8_t type)
{
	switch (type ){
		case BIND_TYPE_POINTER:
			return "pointer";
		case BIND_TYPE_TEXT_ABSOLUTE32:
			return "text abs32";
		case BIND_TYPE_TEXT_PCREL32:
			return "text rel32";
	}
	return "!!unknown!!";
}


template <typename A>
typename A::P::uint_t DyldInfoPrinter<A>::segStartAddress(uint8_t segIndex)
{
	if ( segIndex > fSegments.size() )
		throw "segment index out of range";
	return fSegments[segIndex]->vmaddr();
}

template <typename A>
const char* DyldInfoPrinter<A>::segmentName(uint8_t segIndex)
{
	if ( segIndex > fSegments.size() )
		throw "segment index out of range";
	return fSegments[segIndex]->segname();
}

template <typename A>
const char* DyldInfoPrinter<A>::sectionName(uint8_t segIndex, pint_t address)
{
	if ( segIndex > fSegments.size() )
		throw "segment index out of range";
	const macho_segment_command<P>* segCmd = fSegments[segIndex];
	macho_section<P>* const sectionsStart = (macho_section<P>*)((char*)segCmd + sizeof(macho_segment_command<P>));
	macho_section<P>* const sectionsEnd = &sectionsStart[segCmd->nsects()];
	for(macho_section<P>* sect = sectionsStart; sect < sectionsEnd; ++sect) {
		if ( (sect->addr() <= address) && (address < (sect->addr()+sect->size())) ) {
			if ( strlen(sect->sectname()) > 15 ) {
				static char temp[18];
				strlcpy(temp, sect->sectname(), 17);
				return temp;
			}
			else {
				return sect->sectname();
			}
		}
	}
	return "??";
}

template <typename A>
const char* DyldInfoPrinter<A>::getSegAndSectName(uint8_t segIndex, pint_t address)
{
	static char buffer[64];
	strcpy(buffer, segmentName(segIndex));
	strcat(buffer, "/");
	const macho_segment_command<P>* segCmd = fSegments[segIndex];
	macho_section<P>* const sectionsStart = (macho_section<P>*)((char*)segCmd + sizeof(macho_segment_command<P>));
	macho_section<P>* const sectionsEnd = &sectionsStart[segCmd->nsects()];
	for(macho_section<P>* sect = sectionsStart; sect < sectionsEnd; ++sect) {
		if ( (sect->addr() <= address) && (address < (sect->addr()+sect->size())) ) {
			// section name may not be zero terminated
			char* end = &buffer[strlen(buffer)];
			strlcpy(end, sect->sectname(), 16);
			return buffer;				
		}
	}
	return "??";
}

template <typename A>
const char* DyldInfoPrinter<A>::ordinalName(int libraryOrdinal)
{
	switch ( libraryOrdinal) {
		case BIND_SPECIAL_DYLIB_SELF:
			return "this-image";
		case BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE:
			return "main-executable";
		case BIND_SPECIAL_DYLIB_FLAT_LOOKUP:
			return "flat-namespace";
	}
	if ( libraryOrdinal < BIND_SPECIAL_DYLIB_FLAT_LOOKUP )
		throw "unknown special ordinal";
	if ( libraryOrdinal > fDylibs.size() )
		throw "libraryOrdinal out of range";
	return fDylibs[libraryOrdinal-1];
}


template <typename A>
void DyldInfoPrinter<A>::printRebaseInfo()
{
	if ( (fInfo == NULL) || (fInfo->rebase_off() == 0) ) {
		printf("no compressed rebase info\n");
	}
	else {
		printf("rebase information:\n");
		printf("segment section          address     type\n");

		const uint8_t* p = (uint8_t*)fHeader + fInfo->rebase_off();
		const uint8_t* end = &p[fInfo->rebase_size()];
		
		uint8_t type = 0;
		uint64_t segOffset = 0;
		uint32_t count;
		uint32_t skip;
		int segIndex;
		pint_t segStartAddr = 0;
		const char* segName = "??";
		const char* typeName = "??";
		bool done = false;
		while ( !done && (p < end) ) {
			uint8_t immediate = *p & REBASE_IMMEDIATE_MASK;
			uint8_t opcode = *p & REBASE_OPCODE_MASK;
			++p;
			switch (opcode) {
				case REBASE_OPCODE_DONE:
					done = true;
					break;
				case REBASE_OPCODE_SET_TYPE_IMM:
					type = immediate;
					typeName = rebaseTypeName(type);
					break;
				case REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
					segIndex = immediate;
					segStartAddr = segStartAddress(segIndex);
					segName = segmentName(segIndex);
					segOffset = read_uleb128(p, end);
					break;
				case REBASE_OPCODE_ADD_ADDR_ULEB:
					segOffset += read_uleb128(p, end);
					break;
				case REBASE_OPCODE_ADD_ADDR_IMM_SCALED:
					segOffset += immediate*sizeof(pint_t);
					break;
				case REBASE_OPCODE_DO_REBASE_IMM_TIMES:
					for (int i=0; i < immediate; ++i) {
						printf("%-7s %-16s 0x%08llX  %s\n", segName, sectionName(segIndex, segStartAddr+segOffset), segStartAddr+segOffset, typeName);
						segOffset += sizeof(pint_t);
					}
					break;
				case REBASE_OPCODE_DO_REBASE_ULEB_TIMES:
					count = read_uleb128(p, end);
					for (uint32_t i=0; i < count; ++i) {
						printf("%-7s %-16s 0x%08llX  %s\n", segName, sectionName(segIndex, segStartAddr+segOffset), segStartAddr+segOffset, typeName);
						segOffset += sizeof(pint_t);
					}
					break;
				case REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB:
					printf("%-7s %-16s 0x%08llX  %s\n", segName, sectionName(segIndex, segStartAddr+segOffset), segStartAddr+segOffset, typeName);
					segOffset += read_uleb128(p, end) + sizeof(pint_t);
					break;
				case REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB:
					count = read_uleb128(p, end);
					skip = read_uleb128(p, end);
					for (uint32_t i=0; i < count; ++i) {
						printf("%-7s %-16s 0x%08llX  %s\n", segName, sectionName(segIndex, segStartAddr+segOffset), segStartAddr+segOffset, typeName);
						segOffset += skip + sizeof(pint_t);
					}
					break;
				default:
					throwf("bad rebase opcode %d", *p);
			}
		}	
	}

}



template <typename A>
void DyldInfoPrinter<A>::printRebaseInfoOpcodes()
{
	if ( (fInfo == NULL) || (fInfo->rebase_off() == 0) ) {
		printf("no compressed rebase info\n");
	}
	else {
		printf("rebase opcodes:\n");
		const uint8_t* p = (uint8_t*)fHeader + fInfo->rebase_off();
		const uint8_t* end = &p[fInfo->rebase_size()];
		
		uint8_t type = 0;
		uint64_t address = fBaseAddress;
		uint32_t count;
		uint32_t skip;
		unsigned int segmentIndex;
		bool done = false;
		while ( !done && (p < end) ) {
			uint8_t immediate = *p & REBASE_IMMEDIATE_MASK;
			uint8_t opcode = *p & REBASE_OPCODE_MASK;
			++p;
			switch (opcode) {
				case REBASE_OPCODE_DONE:
					done = true;
					printf("REBASE_OPCODE_DONE()\n");
					break;
				case REBASE_OPCODE_SET_TYPE_IMM:
					type = immediate;
					printf("REBASE_OPCODE_SET_TYPE_IMM(%d)\n", type);
					break;
				case REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
					segmentIndex = immediate;
					address = read_uleb128(p, end);
					printf("REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB(%d, 0x%08llX)\n", segmentIndex, address);
					break;
				case REBASE_OPCODE_ADD_ADDR_ULEB:
					address = read_uleb128(p, end);
					printf("REBASE_OPCODE_ADD_ADDR_ULEB(0x%0llX)\n", address);
					break;
				case REBASE_OPCODE_ADD_ADDR_IMM_SCALED:
					address = immediate*sizeof(pint_t);
					printf("REBASE_OPCODE_ADD_ADDR_IMM_SCALED(0x%0llX)\n", address);
					break;
				case REBASE_OPCODE_DO_REBASE_IMM_TIMES:
					printf("REBASE_OPCODE_DO_REBASE_IMM_TIMES(%d)\n", immediate);
					break;
				case REBASE_OPCODE_DO_REBASE_ULEB_TIMES:
					count = read_uleb128(p, end);
					printf("REBASE_OPCODE_DO_REBASE_ULEB_TIMES(%d)\n", count);
					break;
				case REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB:
					skip = read_uleb128(p, end) + sizeof(pint_t);
					printf("REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB(%d)\n", skip);
					break;
				case REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB:
					count = read_uleb128(p, end);
					skip = read_uleb128(p, end);
					printf("REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB(%d, %d)\n", count, skip);
					break;
				default:
					throwf("bad rebase opcode %d", *p);
			}
		}	
	}

}






template <typename A>
void DyldInfoPrinter<A>::printBindingInfoOpcodes(bool weakbinding)
{
	if ( fInfo == NULL ) {
		printf("no compressed binding info\n");
	}
	else if ( !weakbinding && (fInfo->bind_off() == 0) ) {
		printf("no compressed binding info\n");
	}
	else if ( weakbinding && (fInfo->weak_bind_off() == 0) ) {
		printf("no compressed weak binding info\n");
	}
	else {
		const uint8_t* start;
		const uint8_t* end;
		if ( weakbinding ) {
			printf("weak binding opcodes:\n");
			start = (uint8_t*)fHeader + fInfo->weak_bind_off();
			end = &start[fInfo->weak_bind_size()];
		}
		else {
			printf("binding opcodes:\n");
			start = (uint8_t*)fHeader + fInfo->bind_off();
			end = &start[fInfo->bind_size()];
		}
		const uint8_t* p = start;
		uint8_t type = 0;
		uint8_t flags;
		uint64_t address = fBaseAddress;
		const char* symbolName = NULL;
		int libraryOrdinal = 0;
		int64_t addend = 0;
		uint32_t segmentIndex = 0;
		uint32_t count;
		uint32_t skip;
		bool done = false;
		while ( !done && (p < end) ) {
			uint8_t immediate = *p & BIND_IMMEDIATE_MASK;
			uint8_t opcode = *p & BIND_OPCODE_MASK;
			uint32_t opcodeOffset = p-start;
			++p;
			switch (opcode) {
				case BIND_OPCODE_DONE:
					done = true;
					printf("0x%04X BIND_OPCODE_DONE\n", opcodeOffset);
					break;
				case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
					libraryOrdinal = immediate;
					printf("0x%04X BIND_OPCODE_SET_DYLIB_ORDINAL_IMM(%d)\n", opcodeOffset, libraryOrdinal);
					break;
				case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
					libraryOrdinal = read_uleb128(p, end);
					printf("0x%04X BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB(%d)\n", opcodeOffset, libraryOrdinal);
					break;
				case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
					// the special ordinals are negative numbers
					if ( immediate == 0 )
						libraryOrdinal = 0;
					else {
						int8_t signExtended = BIND_OPCODE_MASK | immediate;
						libraryOrdinal = signExtended;
					}
					printf("0x%04X BIND_OPCODE_SET_DYLIB_SPECIAL_IMM(%d)\n", opcodeOffset, libraryOrdinal);
					break;
				case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
					flags = immediate;
					symbolName = (char*)p;
					while (*p != '\0')
						++p;
					++p;
					printf("0x%04X BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM(0x%02X, %s)\n", opcodeOffset, flags, symbolName);
					break;
				case BIND_OPCODE_SET_TYPE_IMM:
					type = immediate;
					printf("0x%04X BIND_OPCODE_SET_TYPE_IMM(%d)\n", opcodeOffset, type);
					break;
				case BIND_OPCODE_SET_ADDEND_SLEB:
					addend = read_sleb128(p, end);
					printf("0x%04X BIND_OPCODE_SET_ADDEND_SLEB(%lld)\n", opcodeOffset, addend);
					break;
				case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
					segmentIndex = immediate;
					address = read_uleb128(p, end);
					printf("0x%04X BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB(0x%02X, 0x%08llX)\n", opcodeOffset, segmentIndex, address);
					break;
				case BIND_OPCODE_ADD_ADDR_ULEB:
					skip = read_uleb128(p, end);
					printf("0x%04X BIND_OPCODE_ADD_ADDR_ULEB(0x%08X)\n", opcodeOffset, skip);
					break;
				case BIND_OPCODE_DO_BIND:
					printf("0x%04X BIND_OPCODE_DO_BIND()\n", opcodeOffset);
					break;
				case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
					skip = read_uleb128(p, end);
					printf("0x%04X BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB(0x%08X)\n", opcodeOffset, skip);
					break;
				case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
					skip = immediate*sizeof(pint_t) + sizeof(pint_t);
					printf("0x%04X BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED(0x%08X)\n", opcodeOffset, skip);
					break;
				case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
					count = read_uleb128(p, end);
					skip = read_uleb128(p, end);
					printf("0x%04X BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB(%d, 0x%08X)\n", opcodeOffset, count, skip);
					break;
				default:
					throwf("unknown bind opcode %d", *p);
			}
		}	
	}

}



template <typename A>
void DyldInfoPrinter<A>::printBindingInfo()
{
	if ( (fInfo == NULL) || (fInfo->bind_off() == 0) ) {
		printf("no compressed binding info\n");
	}
	else {
		printf("bind information:\n");
		printf("segment section          address        type   weak  addend dylib            symbol\n");
		const uint8_t* p = (uint8_t*)fHeader + fInfo->bind_off();
		const uint8_t* end = &p[fInfo->bind_size()];
		
		uint8_t type = 0;
		uint8_t segIndex = 0;
		uint64_t segOffset = 0;
		const char* symbolName = NULL;
		const char* fromDylib = "??";
		int libraryOrdinal = 0;
		int64_t addend = 0;
		uint32_t count;
		uint32_t skip;
		pint_t segStartAddr = 0;
		const char* segName = "??";
		const char* typeName = "??";
		const char* weak_import = "";
		bool done = false;
		while ( !done && (p < end) ) {
			uint8_t immediate = *p & BIND_IMMEDIATE_MASK;
			uint8_t opcode = *p & BIND_OPCODE_MASK;
			++p;
			switch (opcode) {
				case BIND_OPCODE_DONE:
					done = true;
					break;
				case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
					libraryOrdinal = immediate;
					fromDylib = ordinalName(libraryOrdinal);
					break;
				case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
					libraryOrdinal = read_uleb128(p, end);
					fromDylib = ordinalName(libraryOrdinal);
					break;
				case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
					// the special ordinals are negative numbers
					if ( immediate == 0 )
						libraryOrdinal = 0;
					else {
						int8_t signExtended = BIND_OPCODE_MASK | immediate;
						libraryOrdinal = signExtended;
					}
					fromDylib = ordinalName(libraryOrdinal);
					break;
				case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
					symbolName = (char*)p;
					while (*p != '\0')
						++p;
					++p;
					if ( (immediate & BIND_SYMBOL_FLAGS_WEAK_IMPORT) != 0 )
						weak_import = "weak";
					else
						weak_import = "";
					break;
				case BIND_OPCODE_SET_TYPE_IMM:
					type = immediate;
					typeName = bindTypeName(type);
					break;
				case BIND_OPCODE_SET_ADDEND_SLEB:
					addend = read_sleb128(p, end);
					break;
				case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
					segIndex = immediate;
					segStartAddr = segStartAddress(segIndex);
					segName = segmentName(segIndex);
					segOffset = read_uleb128(p, end);
					break;
				case BIND_OPCODE_ADD_ADDR_ULEB:
					segOffset += read_uleb128(p, end);
					break;
				case BIND_OPCODE_DO_BIND:
					printf("%-7s %-16s 0x%08llX %10s %4s  %5lld %-16s %s\n", segName, sectionName(segIndex, segStartAddr+segOffset), segStartAddr+segOffset, typeName, weak_import, addend, fromDylib, symbolName );
					segOffset += sizeof(pint_t);
					break;
				case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
					printf("%-7s %-16s 0x%08llX %10s %4s  %5lld %-16s %s\n", segName, sectionName(segIndex, segStartAddr+segOffset), segStartAddr+segOffset, typeName, weak_import, addend, fromDylib, symbolName );
					segOffset += read_uleb128(p, end) + sizeof(pint_t);
					break;
				case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
					printf("%-7s %-16s 0x%08llX %10s %4s  %5lld %-16s %s\n", segName, sectionName(segIndex, segStartAddr+segOffset), segStartAddr+segOffset, typeName, weak_import, addend, fromDylib, symbolName );
					segOffset += immediate*sizeof(pint_t) + sizeof(pint_t);
					break;
				case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
					count = read_uleb128(p, end);
					skip = read_uleb128(p, end);
					for (uint32_t i=0; i < count; ++i) {
						printf("%-7s %-16s 0x%08llX %10s %4s  %5lld %-16s %s\n", segName, sectionName(segIndex, segStartAddr+segOffset), segStartAddr+segOffset, typeName, weak_import, addend, fromDylib, symbolName );
						segOffset += skip + sizeof(pint_t);
					}
					break;
				default:
					throwf("bad bind opcode %d", *p);
			}
		}	
	}

}

template <typename A>
void DyldInfoPrinter<A>::printWeakBindingInfo()
{
	if ( (fInfo == NULL) || (fInfo->weak_bind_off() == 0) ) {
		printf("no weak binding\n");
	}
	else {
		printf("weak binding information:\n");
		printf("segment section          address       type     addend symbol\n");
		const uint8_t* p = (uint8_t*)fHeader + fInfo->weak_bind_off();
		const uint8_t* end = &p[fInfo->weak_bind_size()];
		
		uint8_t type = 0;
		uint8_t segIndex = 0;
		uint64_t segOffset = 0;
		const char* symbolName = NULL;
		int64_t addend = 0;
		uint32_t count;
		uint32_t skip;
		pint_t segStartAddr = 0;
		const char* segName = "??";
		const char* typeName = "??";
		bool done = false;
		while ( !done && (p < end) ) {
			uint8_t immediate = *p & BIND_IMMEDIATE_MASK;
			uint8_t opcode = *p & BIND_OPCODE_MASK;
			++p;
			switch (opcode) {
				case BIND_OPCODE_DONE:
					done = true;
					break;
				case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
					symbolName = (char*)p;
					while (*p != '\0')
						++p;
					++p;
					if ( (immediate & BIND_SYMBOL_FLAGS_NON_WEAK_DEFINITION) != 0 )
						printf("                                       strong          %s\n", symbolName );
					break;
				case BIND_OPCODE_SET_TYPE_IMM:
					type = immediate;
					typeName = bindTypeName(type);
					break;
				case BIND_OPCODE_SET_ADDEND_SLEB:
					addend = read_sleb128(p, end);
					break;
				case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
					segIndex = immediate;
					segStartAddr = segStartAddress(segIndex);
					segName = segmentName(segIndex);
					segOffset = read_uleb128(p, end);
					break;
				case BIND_OPCODE_ADD_ADDR_ULEB:
					segOffset += read_uleb128(p, end);
					break;
				case BIND_OPCODE_DO_BIND:
					printf("%-7s %-16s 0x%08llX %10s   %5lld %s\n", segName, sectionName(segIndex, segStartAddr+segOffset), segStartAddr+segOffset, typeName, addend, symbolName );
					segOffset += sizeof(pint_t);
					break;
				case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
					printf("%-7s %-16s 0x%08llX %10s   %5lld %s\n", segName, sectionName(segIndex, segStartAddr+segOffset), segStartAddr+segOffset, typeName, addend, symbolName );
					segOffset += read_uleb128(p, end) + sizeof(pint_t);
					break;
				case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
					printf("%-7s %-16s 0x%08llX %10s   %5lld %s\n", segName, sectionName(segIndex, segStartAddr+segOffset), segStartAddr+segOffset, typeName, addend, symbolName );
					segOffset += immediate*sizeof(pint_t) + sizeof(pint_t);
					break;
				case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
					count = read_uleb128(p, end);
					skip = read_uleb128(p, end);
					for (uint32_t i=0; i < count; ++i) {
					printf("%-7s %-16s 0x%08llX %10s   %5lld %s\n", segName, sectionName(segIndex, segStartAddr+segOffset), segStartAddr+segOffset, typeName, addend, symbolName );
							segOffset += skip + sizeof(pint_t);
					}
					break;
				default:
					throwf("unknown weak bind opcode %d", *p);
			}
		}	
	}

}


template <typename A>
void DyldInfoPrinter<A>::printLazyBindingInfo()
{
	if ( fInfo == NULL ) {
		printf("no compressed dyld info\n");
	}
	else if ( fInfo->lazy_bind_off() == 0 ) {
		printf("no compressed lazy binding info\n");
	}
	else {
		printf("lazy binding information:\n");
		printf("segment section          address    index  dylib            symbol\n");
		const uint8_t* const start = (uint8_t*)fHeader + fInfo->lazy_bind_off();
		const uint8_t* const end = &start[fInfo->lazy_bind_size()];

		uint8_t type = BIND_TYPE_POINTER;
		uint8_t segIndex = 0;
		uint64_t segOffset = 0;
		const char* symbolName = NULL;
		const char* fromDylib = "??";
		int libraryOrdinal = 0;
		int64_t addend = 0;
		uint32_t lazy_offset = 0;
		pint_t segStartAddr = 0;
		const char* segName = "??";
		const char* typeName = "??";
		for (const uint8_t* p=start; p < end; ) {
			uint8_t immediate = *p & BIND_IMMEDIATE_MASK;
			uint8_t opcode = *p & BIND_OPCODE_MASK;
			++p;
			switch (opcode) {
				case BIND_OPCODE_DONE:
					lazy_offset = p-start;
					break;
				case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
					libraryOrdinal = immediate;
					fromDylib = ordinalName(libraryOrdinal);
					break;
				case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
					libraryOrdinal = read_uleb128(p, end);
					fromDylib = ordinalName(libraryOrdinal);
					break;
				case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
					// the special ordinals are negative numbers
					if ( immediate == 0 )
						libraryOrdinal = 0;
					else {
						int8_t signExtended = BIND_OPCODE_MASK | immediate;
						libraryOrdinal = signExtended;
					}
					fromDylib = ordinalName(libraryOrdinal);
					break;
				case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
					symbolName = (char*)p;
					while (*p != '\0')
						++p;
					++p;
					break;
				case BIND_OPCODE_SET_TYPE_IMM:
					type = immediate;
					typeName = bindTypeName(type);
					break;
				case BIND_OPCODE_SET_ADDEND_SLEB:
					addend = read_sleb128(p, end);
					break;
				case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
					segIndex = immediate;
					segStartAddr = segStartAddress(segIndex);
					segName = segmentName(segIndex);
					segOffset = read_uleb128(p, end);
					break;
				case BIND_OPCODE_ADD_ADDR_ULEB:
					segOffset += read_uleb128(p, end);
					break;
				case BIND_OPCODE_DO_BIND:
					printf("%-7s %-16s 0x%08llX 0x%04X %-16s %s\n", segName, sectionName(segIndex, segStartAddr+segOffset), segStartAddr+segOffset, lazy_offset, fromDylib, symbolName );
					segOffset += sizeof(pint_t);
					break;
				default:
					throwf("bad lazy bind opcode %d", *p);
			}
		}
	}

}

#if 0
		uint8_t type = BIND_TYPE_POINTER;
		uint8_t flags;
		uint64_t address = fBaseAddress;
		const char* symbolName = NULL;
		int libraryOrdinal = 0;
		int64_t addend = 0;
		uint32_t segmentIndex = 0;
		uint32_t count;
		uint32_t skip;
		for (const uint8_t* p = start; p < end; ) {
			uint8_t immediate = *p & BIND_IMMEDIATE_MASK;
			uint8_t opcode = *p & BIND_OPCODE_MASK;
			uint32_t opcodeOffset = p-start;
			++p;
			switch (opcode) {
				case BIND_OPCODE_DONE:
					printf("0x%08X BIND_OPCODE_DONE\n", opcodeOffset);
					break;
				case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
					libraryOrdinal = immediate;
					printf("0x%08X BIND_OPCODE_SET_DYLIB_ORDINAL_IMM(%d)\n", opcodeOffset, libraryOrdinal);
					break;
				case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
					libraryOrdinal = read_uleb128(p, end);
					printf("0x%08X BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB(%d)\n", opcodeOffset, libraryOrdinal);
					break;
				case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
					// the special ordinals are negative numbers
					if ( immediate == 0 )
						libraryOrdinal = 0;
					else {
						int8_t signExtended = BIND_OPCODE_MASK | immediate;
						libraryOrdinal = signExtended;
					}
					printf("0x%08X BIND_OPCODE_SET_DYLIB_SPECIAL_IMM(%d)\n", opcodeOffset, libraryOrdinal);
					break;
				case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
					flags = immediate;
					symbolName = (char*)p;
					while (*p != '\0')
						++p;
					++p;
					printf("0x%08X BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM(0x%02X, %s)\n", opcodeOffset, flags, symbolName);
					break;
				case BIND_OPCODE_SET_TYPE_IMM:
					type = immediate;
					printf("0x%08X BIND_OPCODE_SET_TYPE_IMM(%d)\n", opcodeOffset, type);
					break;
				case BIND_OPCODE_SET_ADDEND_SLEB:
					addend = read_sleb128(p, end);
					printf("0x%08X BIND_OPCODE_SET_ADDEND_SLEB(%lld)\n", opcodeOffset, addend);
					break;
				case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
					segmentIndex = immediate;
					address = read_uleb128(p, end);
					printf("0x%08X BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB(0x%02X, 0x%08llX)\n", opcodeOffset, segmentIndex, address);
					break;
				case BIND_OPCODE_ADD_ADDR_ULEB:
					skip = read_uleb128(p, end);
					printf("0x%08X BIND_OPCODE_ADD_ADDR_ULEB(0x%08X)\n", opcodeOffset, skip);
					break;
				case BIND_OPCODE_DO_BIND:
					printf("0x%08X BIND_OPCODE_DO_BIND()\n", opcodeOffset);
					break;
				case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
					skip = read_uleb128(p, end);
					printf("0x%08X BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB(0x%08X)\n", opcodeOffset, skip);
					break;
				case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
					skip = immediate*sizeof(pint_t) + sizeof(pint_t);
					printf("0x%08X BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED(0x%08X)\n", opcodeOffset, skip);
					break;
				case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
					count = read_uleb128(p, end);
					skip = read_uleb128(p, end);
					printf("0x%08X BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB(%d, 0x%08X)\n", opcodeOffset, count, skip);
					break;
				default:
					throwf("unknown bind opcode %d", *p);
			}
		}	
#endif

template <typename A>
void DyldInfoPrinter<A>::printLazyBindingOpcodes()
{
	if ( fInfo == NULL ) {
		printf("no compressed dyld info\n");
	}
	else if ( fInfo->lazy_bind_off() == 0 ) {
		printf("no compressed lazy binding info\n");
	}
	else {
		printf("lazy binding opcodes:\n");
		const uint8_t* const start = (uint8_t*)fHeader + fInfo->lazy_bind_off();
		const uint8_t* const end = &start[fInfo->lazy_bind_size()];
		uint8_t type = BIND_TYPE_POINTER;
		uint8_t flags;
		uint64_t address = fBaseAddress;
		const char* symbolName = NULL;
		int libraryOrdinal = 0;
		int64_t addend = 0;
		uint32_t segmentIndex = 0;
		uint32_t count;
		uint32_t skip;
		for (const uint8_t* p = start; p < end; ) {
			uint8_t immediate = *p & BIND_IMMEDIATE_MASK;
			uint8_t opcode = *p & BIND_OPCODE_MASK;
			uint32_t opcodeOffset = p-start;
			++p;
			switch (opcode) {
				case BIND_OPCODE_DONE:
					printf("0x%04X BIND_OPCODE_DONE\n", opcodeOffset);
					break;
				case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
					libraryOrdinal = immediate;
					printf("0x%04X BIND_OPCODE_SET_DYLIB_ORDINAL_IMM(%d)\n", opcodeOffset, libraryOrdinal);
					break;
				case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
					libraryOrdinal = read_uleb128(p, end);
					printf("0x%04X BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB(%d)\n", opcodeOffset, libraryOrdinal);
					break;
				case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
					// the special ordinals are negative numbers
					if ( immediate == 0 )
						libraryOrdinal = 0;
					else {
						int8_t signExtended = BIND_OPCODE_MASK | immediate;
						libraryOrdinal = signExtended;
					}
					printf("0x%04X BIND_OPCODE_SET_DYLIB_SPECIAL_IMM(%d)\n", opcodeOffset, libraryOrdinal);
					break;
				case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
					flags = immediate;
					symbolName = (char*)p;
					while (*p != '\0')
						++p;
					++p;
					printf("0x%04X BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM(0x%02X, %s)\n", opcodeOffset, flags, symbolName);
					break;
				case BIND_OPCODE_SET_TYPE_IMM:
					type = immediate;
					printf("0x%04X BIND_OPCODE_SET_TYPE_IMM(%d)\n", opcodeOffset, type);
					break;
				case BIND_OPCODE_SET_ADDEND_SLEB:
					addend = read_sleb128(p, end);
					printf("0x%04X BIND_OPCODE_SET_ADDEND_SLEB(%lld)\n", opcodeOffset, addend);
					break;
				case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
					segmentIndex = immediate;
					address = read_uleb128(p, end);
					printf("0x%04X BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB(0x%02X, 0x%08llX)\n", opcodeOffset, segmentIndex, address);
					break;
				case BIND_OPCODE_ADD_ADDR_ULEB:
					skip = read_uleb128(p, end);
					printf("0x%04X BIND_OPCODE_ADD_ADDR_ULEB(0x%08X)\n", opcodeOffset, skip);
					break;
				case BIND_OPCODE_DO_BIND:
					printf("0x%04X BIND_OPCODE_DO_BIND()\n", opcodeOffset);
					break;
				case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
					skip = read_uleb128(p, end);
					printf("0x%04X BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB(0x%08X)\n", opcodeOffset, skip);
					break;
				case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
					skip = immediate*sizeof(pint_t) + sizeof(pint_t);
					printf("0x%04X BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED(0x%08X)\n", opcodeOffset, skip);
					break;
				case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
					count = read_uleb128(p, end);
					skip = read_uleb128(p, end);
					printf("0x%04X BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB(%d, 0x%08X)\n", opcodeOffset, count, skip);
					break;
				default:
					throwf("unknown bind opcode %d", *p);
			}
		}	
	}

}


template <typename A>
void DyldInfoPrinter<A>::processExportNode(const uint8_t* const start, const uint8_t* p, const uint8_t* const end, 
											char* cummulativeString, int curStrOffset) 
{
	const uint8_t terminalSize = *p++;
	const uint8_t* children = p + terminalSize;
	if ( terminalSize != 0 ) {
		uint32_t flags = read_uleb128(p, end);
		uint64_t address = read_uleb128(p, end);
		if ( flags & EXPORT_SYMBOL_FLAGS_WEAK_DEFINITION )
			fprintf(stdout, "0x%08llX [weak_def] %s\n", address, cummulativeString);
		else
			fprintf(stdout, "0x%08llX %s\n", address, cummulativeString);
	}
	const uint8_t childrenCount = *children++;
	const uint8_t* s = children;
	for (uint8_t i=0; i < childrenCount; ++i) {
		int edgeStrLen = 0;
		while (*s != '\0') {
			cummulativeString[curStrOffset+edgeStrLen] = *s++;
			++edgeStrLen;
		}
		cummulativeString[curStrOffset+edgeStrLen] = *s++;
		uint32_t childNodeOffet = read_uleb128(s, end);
		processExportNode(start, start+childNodeOffet, end, cummulativeString, curStrOffset+edgeStrLen);	
	}
}

struct SortExportsByAddress
{
     bool operator()(const mach_o::trie::Entry& left, const mach_o::trie::Entry& right)
     {
        return ( left.address < right.address );
     }
};

template <typename A>
void DyldInfoPrinter<A>::printExportInfo()
{
	if ( (fInfo == NULL) || (fInfo->export_off() == 0) ) {
		printf("no compressed export info\n");
	}
	else {
		const uint8_t* start = (uint8_t*)fHeader + fInfo->export_off();
		const uint8_t* end = &start[fInfo->export_size()];
		std::vector<mach_o::trie::Entry> list;
		parseTrie(start, end, list);
		//std::sort(list.begin(), list.end(), SortExportsByAddress());
		for (std::vector<mach_o::trie::Entry>::iterator it=list.begin(); it != list.end(); ++it) {
			const char* flags = "";
			if ( it->flags & EXPORT_SYMBOL_FLAGS_WEAK_DEFINITION )
				flags = "[weak_def] ";
			fprintf(stdout, "0x%08llX %s%s\n", fBaseAddress+it->address, flags, it->name);
		}
	}
}


template <typename A>
void DyldInfoPrinter<A>::processExportGraphNode(const uint8_t* const start, const uint8_t* const end,  
											const uint8_t* parent, const uint8_t* p,
											char* cummulativeString, int curStrOffset) 
{
	const uint8_t* const me = p;
	const uint8_t terminalSize = *p++;
	const uint8_t* children = p + terminalSize;
	if ( terminalSize != 0 ) {
		uint32_t flags = read_uleb128(p, end);
		uint64_t address = read_uleb128(p, end);
		printf("\tnode%03ld [ label=%s,addr0x%08llX ];\n", (long)(me-start), cummulativeString, address);
	}
	else {
		printf("\tnode%03ld;\n", (long)(me-start));
	}
	const uint8_t childrenCount = *children++;
	const uint8_t* s = children;
	for (uint8_t i=0; i < childrenCount; ++i) {
		const char* edgeName = (char*)s;
		int edgeStrLen = 0;
		while (*s != '\0') {
			cummulativeString[curStrOffset+edgeStrLen] = *s++;
			++edgeStrLen;
		}
		cummulativeString[curStrOffset+edgeStrLen] = *s++;
		uint32_t childNodeOffet = read_uleb128(s, end);
		printf("\tnode%03ld -> node%03d [ label=%s ] ;\n", (long)(me-start), childNodeOffet, edgeName);
		processExportGraphNode(start, end, start, start+childNodeOffet, cummulativeString, curStrOffset+edgeStrLen);	
	}
}

template <typename A>
void DyldInfoPrinter<A>::printExportInfoGraph()
{
	if ( (fInfo == NULL) || (fInfo->export_off() == 0) ) {
		printf("no compressed export info\n");
	}
	else {
		const uint8_t* p = (uint8_t*)fHeader + fInfo->export_off();
		const uint8_t* end = &p[fInfo->export_size()];
		char cummulativeString[2000];
		printf("digraph {\n");
		processExportGraphNode(p, end, p, p, cummulativeString, 0);
		printf("}\n");
	}
}





static void dump(const char* path)
{
	struct stat stat_buf;
	
	try {
		int fd = ::open(path, O_RDONLY, 0);
		if ( fd == -1 )
			throw "cannot open file";
		if ( ::fstat(fd, &stat_buf) != 0 ) 
			throwf("fstat(%s) failed, errno=%d\n", path, errno);
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
				cpu_type_t cputype = OSSwapBigToHostInt32(archs[i].cputype);
				if ( cputype == (uint32_t)sPreferredArch ) {	
					switch(cputype) {
					case CPU_TYPE_POWERPC:
						if ( DyldInfoPrinter<ppc>::validFile(p + offset) )
							DyldInfoPrinter<ppc>::make(p + offset, size, path);
						else
							throw "in universal file, ppc slice does not contain ppc mach-o";
						break;
					case CPU_TYPE_I386:
						if ( DyldInfoPrinter<x86>::validFile(p + offset) )
							DyldInfoPrinter<x86>::make(p + offset, size, path);
						else
							throw "in universal file, i386 slice does not contain i386 mach-o";
						break;
					case CPU_TYPE_POWERPC64:
						if ( DyldInfoPrinter<ppc64>::validFile(p + offset) )
							DyldInfoPrinter<ppc64>::make(p + offset, size, path);
						else
							throw "in universal file, ppc64 slice does not contain ppc64 mach-o";
						break;
					case CPU_TYPE_X86_64:
						if ( DyldInfoPrinter<x86_64>::validFile(p + offset) )
							DyldInfoPrinter<x86_64>::make(p + offset, size, path);
						else
							throw "in universal file, x86_64 slice does not contain x86_64 mach-o";
						break;
					case CPU_TYPE_ARM:
						if ( DyldInfoPrinter<arm>::validFile(p + offset) )
							DyldInfoPrinter<arm>::make(p + offset, size, path);
						else
							throw "in universal file, arm slice does not contain arm mach-o";
						break;
					default:
							throwf("in universal file, unknown architecture slice 0x%x\n", cputype);
					}
				}
			}
		}
		else if ( DyldInfoPrinter<x86>::validFile(p) ) {
			DyldInfoPrinter<x86>::make(p, length, path);
		}
		else if ( DyldInfoPrinter<ppc>::validFile(p) ) {
			DyldInfoPrinter<ppc>::make(p, length, path);
		}
		else if ( DyldInfoPrinter<ppc64>::validFile(p) ) {
			DyldInfoPrinter<ppc64>::make(p, length, path);
		}
		else if ( DyldInfoPrinter<x86_64>::validFile(p) ) {
			DyldInfoPrinter<x86_64>::make(p, length, path);
		}
		else if ( DyldInfoPrinter<arm>::validFile(p) ) {
			DyldInfoPrinter<arm>::make(p, length, path);
		}
		else {
			throw "not a known file type";
		}
	}
	catch (const char* msg) {
		throwf("%s in %s", msg, path);
	}
}

static void usage()
{
	fprintf(stderr, "Usage: dyldinfo [-arch <arch>] <options> <mach-o file>\n"
			"\t-rebase           print addresses dyld will adjust if file not loaded at preferred address\n"
			"\t-bind             print addresses dyld will set based on symbolic lookups\n"
			"\t-weak_bind        print symbols which dyld must coalesce\n"
			"\t-lazy_bind        print addresses dyld will lazily set on first use\n"
			"\t-export           print addresses of all symbols this file exports\n"
			"\t-opcodes          print opcodes used to generate the rebase and binding information\n"
			"\t-export_dot       print a GraphViz .dot file of the exported symbols trie\n"
		);
}


int main(int argc, const char* argv[])
{
	if ( argc == 1 ) {
		usage();
		return 0;
	}

	try {
		std::vector<const char*> files;
		for(int i=1; i < argc; ++i) {
			const char* arg = argv[i];
			if ( arg[0] == '-' ) {
				if ( strcmp(arg, "-arch") == 0 ) {
					const char* arch = ++i<argc? argv[i]: "";
					if ( strcmp(arch, "ppc64") == 0 )
						sPreferredArch = CPU_TYPE_POWERPC64;
					else if ( strcmp(arch, "ppc") == 0 )
						sPreferredArch = CPU_TYPE_POWERPC;
					else if ( strcmp(arch, "i386") == 0 )
						sPreferredArch = CPU_TYPE_I386;
					else if ( strcmp(arch, "x86_64") == 0 )
						sPreferredArch = CPU_TYPE_X86_64;
					else 
						throwf("unknown architecture %s", arch);
				}
				else if ( strcmp(arg, "-rebase") == 0 ) {
					printRebase = true;
				}
				else if ( strcmp(arg, "-bind") == 0 ) {
					printBind = true;
				}
				else if ( strcmp(arg, "-weak_bind") == 0 ) {
					printWeakBind = true;
				}
				else if ( strcmp(arg, "-lazy_bind") == 0 ) {
					printLazyBind = true;
				}
				else if ( strcmp(arg, "-export") == 0 ) {
					printExport = true;
				}
				else if ( strcmp(arg, "-opcodes") == 0 ) {
					printOpcodes = true;
				}
				else if ( strcmp(arg, "-export_dot") == 0 ) {
					printExportGraph = true;
				}
				else {
					throwf("unknown option: %s\n", arg);
				}
			}
			else {
				files.push_back(arg);
			}
		}
		if ( files.size() == 0 )
			usage();
		if ( files.size() == 1 ) {
			dump(files[0]);
		}
		else {
			for(std::vector<const char*>::iterator it=files.begin(); it != files.end(); ++it) {
				printf("\n%s:\n", *it);
				dump(*it);
			}
		}
	}
	catch (const char* msg) {
		fprintf(stderr, "dyldinfo failed: %s\n", msg);
		return 1;
	}
	
	return 0;
}



