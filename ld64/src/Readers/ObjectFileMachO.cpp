/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*- 
 *
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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


namespace ObjectFileMachO {

class Reference : public ObjectFile::Reference
{
public:
							Reference(macho_uintptr_t fixUpOffset, Kind kind, const char* targetName, uint64_t offsetInTarget, uint64_t offsetInFromTarget);
							Reference(macho_uintptr_t fixUpOffset, Kind kind, class Atom& target, uint64_t offsetInTarget, uint64_t offsetInFromTarget);
							Reference(macho_uintptr_t fixUpOffset, Kind kind, class Atom& target, uint64_t offsetInTarget, class Atom& fromTarget, uint64_t offsetInFromTarget);
	virtual					~Reference();
	
	
	virtual bool			isTargetUnbound() const;
	virtual bool			isFromTargetUnbound() const;
	virtual bool			isWeakReference() const;
	virtual bool			requiresRuntimeFixUp(bool slideable) const;
	virtual bool			isLazyReference() const;
	virtual Kind			getKind() const;
	virtual uint64_t		getFixUpOffset() const;
	virtual const char*		getTargetName() const;
	virtual ObjectFile::Atom& getTarget() const;
	virtual uint64_t		getTargetOffset() const;
	virtual bool			hasFromTarget() const;
	virtual ObjectFile::Atom& getFromTarget() const;
	virtual const char*		getFromTargetName() const;
	virtual void			setTarget(ObjectFile::Atom&, uint64_t offset);
	virtual void			setFromTarget(ObjectFile::Atom&);
	virtual void			setFromTargetName(const char*);
	virtual void			setFromTargetOffset(uint64_t);
	virtual const char*		getDescription() const;
	virtual uint64_t		getFromTargetOffset() const;
	
	void					setLazy(bool);
	void					setWeak(bool);
private:
	ObjectFile::Atom*		fTarget;
	ObjectFile::Atom*		fFromTarget;
	const char*				fTargetName;
	const char*				fFromTargetName;
	macho_uintptr_t			fTargetOffset;
	macho_uintptr_t			fFromTargetOffset;
	macho_uintptr_t			fFixUpOffsetInSrc;
	Kind					fKind;
	bool					fLazy;
	bool					fWeak;
};



class Reader : public ObjectFile::Reader 
{
public:
												Reader(const char* path);
												Reader(const macho_header* header, const char* path, const ObjectFile::ReaderOptions& options);
	virtual										~Reader();
												
	virtual const char*								getPath();
	virtual std::vector<class ObjectFile::Atom*>&	getAtoms();
	virtual std::vector<class ObjectFile::Atom*>*	getJustInTimeAtomsFor(const char* name);
	virtual std::vector<ObjectFile::StabsInfo>*		getStabsDebugInfo(); // stabs info not associated with an atom


private:
	friend class Atom;
	void										init(const macho_header* header, const char* path);
	void										buildOffsetsSet(const macho_relocation_info* reloc, const macho_section* sect, std::set<uint32_t>& offsets, std::set<uint32_t>& dontUse);
	void										addRelocReference(const macho_section* sect, const macho_relocation_info* reloc);
	Atom*										findAtomCoveringOffset(uint32_t offset);
	uint32_t									findAtomIndex(const Atom& atom);
	void										addFixUp(uint32_t srcAddr, uint32_t dstAddr, Reference::Kind kind, uint32_t picBaseAddr);
	class Segment*								makeSegmentFromSection(const macho_section*);
	macho_uintptr_t								commonsOffset();
	void										insertOffsetIfText(std::set<uint32_t>& offsets, uint32_t value);
	void										insertOffsetIfNotText(std::set<uint32_t>& offsets, uint32_t value);
	const macho_section*						findSectionCoveringOffset(uint32_t offset);
	void										addCallSiteReference(Atom& src, uint32_t offsetInSrcAtom, Reference::Kind kind, Atom& target, uint32_t picBaseOffset, uint32_t offsetInTargetAtom);
	void										deadStub(Atom& target);
	
	const char*									fPath;
	const ObjectFile::ReaderOptions&			fOptions;
	const macho_header*							fHeader;
	const char*									fStrings;
	const macho_nlist*							fSymbols;
	uint32_t									fSymbolCount;
	const macho_segment_command*				fSegment;
	const uint32_t*								fIndirectTable;
	std::vector<class Atom*>					fAtoms;
	std::vector<class Segment*>					fSegments;
	std::set<class ObjectFile::Atom*>			fDeadAtoms;
	uint32_t									fNonAtomStabsStartIndex;
	uint32_t									fNonAtomStabsCount;
	std::vector<uint32_t>						fNonAtomExtras;
};

class Segment : public ObjectFile::Segment
{
public:
	virtual const char*			getName() const;
	virtual bool				isContentReadable() const;
	virtual bool				isContentWritable() const;
	virtual bool				isContentExecutable() const;
protected:
		Segment(const macho_section*);
		friend class Reader;
private:
	const macho_section*		fSection;
};

class Atom : public ObjectFile::Atom
{
public:
	virtual ObjectFile::Reader*				getFile() const;
	virtual const char*						getName() const;
	virtual const char*						getDisplayName() const;
	virtual Scope							getScope() const;
	virtual bool							isTentativeDefinition() const;
	virtual bool							isWeakDefinition() const;
	virtual bool							isCoalesableByName() const;
	virtual bool							isCoalesableByValue() const;
	virtual bool							isZeroFill() const;
	virtual bool							dontDeadStrip() const;
	virtual bool							dontStripName() const;  // referenced dynamically
	virtual bool							isImportProxy() const;
	virtual uint64_t							getSize() const;
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const;
	virtual bool								mustRemainInSection() const;
	virtual const char*							getSectionName() const;
	virtual Segment&							getSegment() const;
	virtual bool								requiresFollowOnAtom() const;
	virtual ObjectFile::Atom&					getFollowOnAtom() const;
	virtual std::vector<ObjectFile::StabsInfo>*	getStabsDebugInfo() const;
	virtual uint8_t								getAlignment() const;
	virtual WeakImportSetting					getImportWeakness() const { return ObjectFile::Atom::kWeakUnset; }
	virtual void								copyRawContent(uint8_t buffer[]) const;
	virtual void								writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;
	virtual void								setScope(Scope);
	virtual void								setImportWeakness(bool weakImport) { }

	bool										isLazyStub();

protected:
	friend class Reader;
											Atom(Reader&, const macho_nlist*);
											Atom(Reader&, uint32_t offset);
	virtual									~Atom();
	
	const macho_section*					findSectionFromOffset(uint32_t offset);
	const macho_section*					getCommonsSection();
	void									setSize(macho_uintptr_t);
	void									setFollowOnAtom(Atom&);
	static bool								atomCompare(const Atom* lhs, const Atom* rhs);
	Reference*								addDirectReference(macho_uintptr_t offsetInSrcAtom, Reference::Kind kind, Atom& target, uint64_t offsetInTarget, uint64_t offsetInFromTarget);
	Reference* 								addByNameReference(macho_uintptr_t offsetInSrcAtom, Reference::Kind kind, const char* targetName, uint64_t offsetInTarget, uint64_t offsetInFromTarget);
	Reference* 								addDifferenceReference(macho_uintptr_t offsetInSrcAtom, Reference::Kind kind, Atom& target, uint64_t offsetInTarget, Atom& fromTarget, uint64_t offsetInFromTarget);
	Reference* 								addReference(macho_uintptr_t offsetInSrcAtom, Reference::Kind kind, Atom& target, uint64_t offsetInTarget, uint64_t offsetInFromTarget);
	
	Reader&									fOwner;
	const macho_nlist*						fSymbol;
	macho_uintptr_t							fOffset;
	macho_uintptr_t							fSize;
	const macho_section*					fSection;
	Segment*								fSegment;
	const char*								fSynthesizedName;
	std::vector<class Reference*>			fReferences;
	ObjectFile::Atom::Scope					fScope;
	uint32_t								fStabsStartIndex;
	uint32_t								fStabsCount;
	
	static macho_section					fgCommonsSection;	// for us by tentative definitions
};


Reader* MakeReader(const macho_header* mh, const char* path, const ObjectFile::ReaderOptions& options)
{	
	return new Reader(mh, path, options);
}


Reader::Reader(const macho_header* header, const char* path, const ObjectFile::ReaderOptions& options)
 : fPath(NULL), fOptions(options), fHeader(NULL), fStrings(NULL), fSymbols(NULL), fSymbolCount(0), fSegment(NULL)
{
	init(header, path);
}

Reader::Reader(const char* path)
 : fPath(NULL), fOptions(*(new ObjectFile::ReaderOptions())), fHeader(NULL), fStrings(NULL), fSymbols(NULL), fSymbolCount(0), fSegment(NULL),
	fIndirectTable(NULL), fNonAtomStabsStartIndex(0), fNonAtomStabsCount(0)
{
	struct stat stat_buf;
	
	int fd = ::open(path, O_RDONLY, 0);
	::fstat(fd, &stat_buf);
	void* p = ::mmap(NULL, stat_buf.st_size, PROT_READ, MAP_FILE, fd, 0);
	::close(fd);
	if ( ((macho_header*)p)->magic() == MH_MAGIC ) {
		init((macho_header*)p, path);
		return;
	}
	throw "add fat handling";
}


Reader::~Reader()
{
}


bool Atom::atomCompare(const Atom* lhs, const Atom* rhs)
{
	return lhs->fOffset < rhs->fOffset;
}



void Reader::init(const macho_header* header, const char* path)
{
	// sanity check
#if defined(ARCH_PPC)
	if ( (header->magic() != MH_MAGIC) || (header->cputype() != CPU_TYPE_POWERPC) )
		throw "not a valid ppc mach-o file";
#elif defined(ARCH_I386)
	if ( (header->magic() != MH_MAGIC) || (header->cputype() != CPU_TYPE_I386) )
		throw "not a valid i386 mach-o file";
#elif defined(ARCH_PPC64)
	if ( (header->magic() != MH_MAGIC_64) || (header->cputype() != CPU_TYPE_POWERPC64) )
		throw "not a valid ppc64 mach-o file";
#endif

	// cache intersting pointers
	fPath = strdup(path);
	fHeader = header;
	const uint32_t cmd_count = header->ncmds();
	const macho_load_command* const cmds = (macho_load_command*)((char*)header + macho_header::size);
	const macho_load_command* cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		switch (cmd->cmd()) {
			case LC_SYMTAB:
				{
					const macho_symtab_command* symtab = (macho_symtab_command*)cmd;
					fSymbolCount = symtab->nsyms();
					fSymbols = (const macho_nlist*)((char*)header + symtab->symoff());
					fStrings = (char*)header + symtab->stroff();
				}
				break;
			case LC_DYSYMTAB:
				{
					const macho_dysymtab_command* dsymtab = (struct macho_dysymtab_command*)cmd;
					fIndirectTable = (uint32_t*)((char*)fHeader + dsymtab->indirectsymoff());
				}
				break;
			default:
				if ( cmd->cmd() == macho_segment_command::command ) {
					fSegment= (macho_segment_command*)cmd;
				}
				break;
		}
		cmd = (const macho_load_command*)(((char*)cmd)+cmd->cmdsize());
	}
	
	// add all atoms that have entries in symbol table
	std::set<uint32_t> symbolAtomOffsets;
	for (uint32_t i=0; i < fSymbolCount; ++i) {
		const macho_nlist& sym = fSymbols[i];
		if ( (sym.n_type() & N_STAB) == 0 ) {
			uint8_t type =  (sym.n_type() & N_TYPE);
			if ( (type == N_SECT) || ((type == N_UNDF) && (sym.n_value() != 0)) ) {
				// real definition or "tentative definition"
				Atom* newAtom = new Atom(*this, &sym);
				fAtoms.push_back(newAtom);
				symbolAtomOffsets.insert(newAtom->fOffset);
			}
		}
	}
	
	// add all points referenced in relocations
	const macho_section* const sectionsStart = (macho_section*)((char*)fSegment + sizeof(macho_segment_command));
	const macho_section* const sectionsEnd = &sectionsStart[fSegment->nsects()];
	std::set<uint32_t> cleavePoints;
	std::set<uint32_t> dontCleavePoints;
	for (const macho_section* sect=sectionsStart; sect < sectionsEnd; ++sect) {
		const macho_relocation_info* relocs = (macho_relocation_info*)((char*)(fHeader) + sect->reloff());
		const uint32_t relocCount = sect->nreloc();
		for (uint32_t r = 0; r < relocCount; ++r) {
			buildOffsetsSet(&relocs[r], sect, cleavePoints, dontCleavePoints);
		}
	}
	// add all stub functions and (non)lazy pointers
	std::set<uint32_t> deadStubOffsets;
	for (const macho_section* sect=sectionsStart; sect < sectionsEnd; ++sect) {
		uint8_t type (sect->flags() & SECTION_TYPE);
		switch ( type ) {
			case S_SYMBOL_STUBS:
				{
					const uint32_t stubSize = sect->reserved2();
					// TVector glue sections are marked as S_SYMBOL_STUBS but are only 8 bytes
					if ( stubSize > 8 ) {
						for(uint32_t sectOffset=0; sectOffset < sect->size(); sectOffset += stubSize) {
							uint32_t stubAddr = sect->addr() + sectOffset;
							if ( cleavePoints.count(stubAddr) == 0 ) {
								cleavePoints.insert(stubAddr);
								deadStubOffsets.insert(stubAddr);
							}
						}
					}
				}
				break;
			case S_NON_LAZY_SYMBOL_POINTERS:
			case S_LAZY_SYMBOL_POINTERS:
				for(uint32_t sectOffset=0; sectOffset < sect->size(); sectOffset += sizeof(macho_uintptr_t)) {
					uint32_t pointerAddr = sect->addr() + sectOffset;
					cleavePoints.insert(pointerAddr);
				}
				break;
		}
		// also make sure each section break is a cleave point
		if ( sect->size() != 0 ) 
			cleavePoints.insert(sect->addr());
	}
	
	for (std::set<uint32_t>::iterator it=cleavePoints.begin(); it != cleavePoints.end(); it++) {
		uint32_t cleavePoint = *it;
		//printf("cleave offset 0x%08X, don't cleave=%d, isSymbol=%d\n", cleavePoint, dontCleavePoints.count(cleavePoint), symbolAtomOffsets.count(cleavePoint));
		// only create an atom if it is not a don't-cleave point and there is not already an atom at this offset
		if ( (dontCleavePoints.count(cleavePoint) == 0) && (symbolAtomOffsets.count(cleavePoint) == 0) )
			fAtoms.push_back(new Atom(*this, cleavePoint));
	}
	
	const uint32_t atomCount = fAtoms.size();
	if ( atomCount > 0 ) {
		// sort the atoms so the occur in source file order
		std::sort(fAtoms.begin(), fAtoms.end(), Atom::atomCompare);
		
		// tell each atom its size and follow on
		const bool dontDeadStrip = ((fHeader->flags() & MH_SUBSECTIONS_VIA_SYMBOLS) == 0);
		Atom* lastAtom = fAtoms[0];
		for (uint32_t i=1; i < atomCount; ++i) {
			Atom* thisAtom = fAtoms[i];
			if ( lastAtom->getSize() == 0 ) {
				if ( lastAtom->fSection == thisAtom->fSection )
					lastAtom->setSize(thisAtom->fOffset - lastAtom->fOffset);
				else
					lastAtom->setSize(lastAtom->fSection->addr() + lastAtom->fSection->size() - lastAtom->fOffset);
			}
			if ( dontDeadStrip )
				lastAtom->setFollowOnAtom(*thisAtom);
			lastAtom = thisAtom;
		}
		lastAtom = fAtoms[atomCount-1];
		if ( lastAtom->getSize() == 0 )
			lastAtom->setSize(lastAtom->fSection->addr() + lastAtom->fSection->size() - lastAtom->fOffset);
		
		// add relocation based references
		for (const macho_section* sect=sectionsStart; sect < sectionsEnd; ++sect) {
			const macho_relocation_info* relocs = (macho_relocation_info*)((char*)(fHeader) + sect->reloff());
			const uint32_t relocCount = sect->nreloc();
			for (uint32_t r = 0; r < relocCount; ++r) {
				addRelocReference(sect, &relocs[r]);
			}
		}		
		
		// add dead stubs to list to delete
		for (std::set<uint32_t>::iterator it=deadStubOffsets.begin(); it != deadStubOffsets.end(); it++) {
			Atom* deadStub = findAtomCoveringOffset(*it);
			this->deadStub(*deadStub);
		}
		
		// remove dead stubs and lazy pointers
		for (std::set<ObjectFile::Atom*>::iterator deadIt=fDeadAtoms.begin(); deadIt != fDeadAtoms.end(); deadIt++) {
			for (std::vector<Atom*>::iterator it=fAtoms.begin(); it != fAtoms.end(); it++) {
				if ( *deadIt == *it ) {
					fAtoms.erase(it);
					break;
				}
			}
		}

	}

	// process stabs debugging info
	if ( ! fOptions.fStripDebugInfo ) {
		// scan symbol table for stabs entries
		fNonAtomStabsStartIndex = 0xFFFFFFFF;
		fNonAtomStabsCount = 0;
		uint32_t possibleStart = 0;
		Atom* atom = NULL;
		const uint32_t atomCount = fAtoms.size();
		enum { start, inBeginEnd, foundFirst, inFunction } state = start;
		for (uint32_t symbolIndex = 0; symbolIndex < fSymbolCount; ++symbolIndex ) {
			const macho_nlist* sym = &fSymbols[symbolIndex];
			uint8_t type = sym->n_type();
			if ( (type & N_STAB) != 0 ) {
				if ( fNonAtomStabsStartIndex == 0xFFFFFFFF )
					fNonAtomStabsStartIndex = symbolIndex;
				switch (state) {
					case start:
						if ( (type == N_SLINE) || (type == N_SOL) ) {
							possibleStart = symbolIndex;
							state = foundFirst;
						} 
						else if ( type == N_BNSYM ) {
							macho_uintptr_t targetAddr = sym->n_value();
							atom = this->findAtomCoveringOffset(targetAddr);
							if ( (atom != NULL) || (atom->fOffset == targetAddr) ) {
								atom->fStabsStartIndex = symbolIndex;
								if ( fNonAtomStabsCount == 0 )
									fNonAtomStabsCount = symbolIndex - fNonAtomStabsStartIndex;
							} 
							else {
								fprintf(stderr, "can't find atom for stabs 0x%02X at %08X in %s\n", type, targetAddr, path);
								atom = NULL;
							}
							state = inBeginEnd;
						} 
						else if ( (type == N_STSYM) || (type == N_LCSYM) ) {
							macho_uintptr_t targetAddr = sym->n_value();
							atom = this->findAtomCoveringOffset(targetAddr);
							if ( (atom != NULL) || (atom->fOffset == targetAddr) ) {
								atom->fStabsStartIndex = symbolIndex;
								atom->fStabsCount = 1;
								if ( fNonAtomStabsCount == 0 )
									fNonAtomStabsCount = symbolIndex - fNonAtomStabsStartIndex;
							} 
							else {
								fprintf(stderr, "can't find atom for stabs 0x%02X at %08X in %s\n", type, targetAddr, path);
								atom = NULL;
							}
						}
						else if ( type == N_GSYM ) {
							// n_value field is NOT atom address ;-(
							// need to find atom by name match
							const char* symString = &fStrings[sym->n_strx()];
							const char* colon = strchr(symString, ':');
							bool found = false;
							if ( colon != NULL ) {
								int nameLen = colon - symString;
								for (uint32_t searchIndex = 0; searchIndex < atomCount; ++searchIndex) {
									const char* atomName = fAtoms[searchIndex]->getName();
									if ( (atomName != NULL) && (strncmp(&atomName[1], symString, nameLen) == 0) ) {
										atom = fAtoms[searchIndex];
										atom->fStabsStartIndex = symbolIndex;
										atom->fStabsCount = 1;
										if ( fNonAtomStabsCount == 0 )
											fNonAtomStabsCount = symbolIndex - fNonAtomStabsStartIndex;
										found = true;
										break;
									}
								}
							}
							if ( !found ) {
								fprintf(stderr, "can't find atom for N_GSYM stabs %s in %s\n", symString, path);
								atom = NULL;
							}
						}
						else if ( type == N_LSYM ) {
							if ( fNonAtomStabsCount != 0 ) {
								// built with -gfull and some type definition not at start of source
								fNonAtomExtras.push_back(symbolIndex);
							}
						}
						break;
					case inBeginEnd:
						if ( type == N_ENSYM ) {
							state = start;
							if ( atom != NULL ) 
								atom->fStabsCount = symbolIndex - atom->fStabsStartIndex + 1;
						}
						break;
					case foundFirst:
						if ( (type == N_FUN) && (sym->n_sect() != 0) ) {
							state = inFunction;
							macho_uintptr_t targetAddr = sym->n_value();
							atom = this->findAtomCoveringOffset(targetAddr);
							if ( (atom == NULL) || (atom->fOffset != targetAddr) ) {
								fprintf(stderr, "can't find atom for stabs FUN index: %d at 0x%08llX in %s\n", symbolIndex, (uint64_t)targetAddr, path);
								atom = NULL;
							}
							else {
								atom->fStabsStartIndex = possibleStart;
								if ( fNonAtomStabsCount == 0 )
									fNonAtomStabsCount = possibleStart - fNonAtomStabsStartIndex;
							} 
						}
						else if ( (type == N_FUN) && (sym->n_sect() == 0) ) {
							fprintf(stderr, "end stab FUN found without start FUN, index=%d in %s\n", symbolIndex, path);
							state = start;
						}
						break;
					case inFunction:
						if ( (type == N_FUN) && (sym->n_sect() == 0) ) {
							state = start;
							if ( atom != NULL ) 
								atom->fStabsCount = symbolIndex - atom->fStabsStartIndex + 1;
						}
						break;
				}
			}
		}
	
	}
}


void Reader::addRelocReference(const macho_section* sect, const macho_relocation_info* reloc)
{	
	uint32_t srcAddr;
	uint32_t dstAddr;
	Atom* src;
	Atom* dst;
#if defined(ARCH_PPC) || defined(ARCH_PPC64)
	uint32_t instruction;
#endif
	uint32_t* fixUpPtr;
	if ( (reloc->r_address() & R_SCATTERED) == 0 ) {
		fixUpPtr = (uint32_t*)((char*)(fHeader) + sect->offset() + reloc->r_address());
#if defined(ARCH_PPC) || defined(ARCH_PPC64)
		const macho_relocation_info* nextReloc = &reloc[1];
#endif
		switch ( reloc->r_type() ) {
#if defined(ARCH_PPC) || defined(ARCH_PPC64)
			case PPC_RELOC_BR24:
				{
					if ( reloc->r_extern() ) {
						instruction = OSSwapBigToHostInt32(*fixUpPtr);
						int32_t displacement = (instruction & 0x03FFFFFC);
						if ( (displacement & 0x02000000) != 0 )
							displacement |= 0xFC000000;
						uint32_t offsetInTarget = sect->addr() + reloc->r_address() + displacement;
						srcAddr = sect->addr() + reloc->r_address();
						src = findAtomCoveringOffset(srcAddr);	
						const macho_nlist* targetSymbol = &fSymbols[reloc->r_symbolnum()];
						const char* targetName = &fStrings[targetSymbol->n_strx()];
						src->addByNameReference(srcAddr - src->fOffset, Reference::ppcFixupBranch24, targetName, offsetInTarget, 0);
					}
					else {
						instruction = OSSwapBigToHostInt32(*fixUpPtr);
						if ( (instruction & 0x4C000000) == 0x48000000 ) {
							int32_t displacement = (instruction & 0x03FFFFFC);
							if ( (displacement & 0x02000000) != 0 )
								displacement |= 0xFC000000;
							srcAddr = sect->addr() + reloc->r_address();
							dstAddr = srcAddr + displacement;
							src = findAtomCoveringOffset(srcAddr);
							dst = findAtomCoveringOffset(dstAddr);	
							this->addCallSiteReference(*src, srcAddr - src->fOffset, Reference::ppcFixupBranch24, *dst, 0, dstAddr - dst->fOffset);
						}
					}
				}
				break;
			case PPC_RELOC_BR14:
				{
					if ( reloc->r_extern() ) {
						srcAddr = sect->addr() + reloc->r_address();
						src = findAtomCoveringOffset(srcAddr);	
						const macho_nlist* targetSymbol = &fSymbols[reloc->r_symbolnum()];
						const char* targetName = &fStrings[targetSymbol->n_strx()];
						src->addByNameReference(srcAddr - src->fOffset, Reference::ppcFixupBranch14, targetName, 0, 0);
					}
					else {
						instruction = OSSwapBigToHostInt32(*fixUpPtr);
						int32_t displacement = (instruction & 0x0000FFFC);
						if ( (displacement & 0x00008000) != 0 )
							displacement |= 0xFFFF0000;
						srcAddr = sect->addr() + reloc->r_address();
						dstAddr = srcAddr + displacement;
						src = findAtomCoveringOffset(srcAddr);
						dst = findAtomCoveringOffset(dstAddr);	
						this->addCallSiteReference(*src, srcAddr - src->fOffset, Reference::ppcFixupBranch14, *dst, 0, dstAddr - dst->fOffset);
					}
				}
				break;
			case PPC_RELOC_PAIR:
				// skip, processed by a previous look ahead 
				break;
			case PPC_RELOC_LO16:
				{
					if ( nextReloc->r_type() != PPC_RELOC_PAIR ) {
						printf("PPC_RELOC_LO16 missing following pair\n");
						break;
					}
					srcAddr = sect->addr() + reloc->r_address();
					if ( reloc->r_extern() ) {
						const macho_nlist* targetSymbol = &fSymbols[reloc->r_symbolnum()];
						const char* targetName = &fStrings[targetSymbol->n_strx()];
						src = findAtomCoveringOffset(srcAddr);	
						instruction = OSSwapBigToHostInt32(*fixUpPtr);
						dstAddr = (nextReloc->r_address() << 16) | (instruction & 0x0000FFFF);
						src->addByNameReference(srcAddr - src->fOffset, Reference::ppcFixupAbsLow16, targetName, dstAddr, 0);
					}
					else {
						instruction = OSSwapBigToHostInt32(*fixUpPtr);
						int16_t lowBits = (instruction & 0xFFFF);
						dstAddr = (nextReloc->r_address() << 16) + (int32_t)lowBits;
						if ( (lowBits & 0x8000) != 0 )
							dstAddr += 0x10000;
						addFixUp(srcAddr, dstAddr, Reference::ppcFixupAbsLow16, 0);
					}
				}
				break;
			case PPC_RELOC_LO14:
				{
					if ( nextReloc->r_type() != PPC_RELOC_PAIR ) {
						printf("PPC_RELOC_LO14 missing following pair\n");
						break;
					}
					srcAddr = sect->addr() + reloc->r_address();
					if ( reloc->r_extern() ) {
						const macho_nlist* targetSymbol = &fSymbols[reloc->r_symbolnum()];
						const char* targetName = &fStrings[targetSymbol->n_strx()];
						src = findAtomCoveringOffset(srcAddr);	
						instruction = OSSwapBigToHostInt32(*fixUpPtr);
						dstAddr = (nextReloc->r_address() << 16) | (instruction & 0x0000FFFC);
						src->addByNameReference(srcAddr - src->fOffset, Reference::ppcFixupAbsLow14, targetName, dstAddr, 0);
					}
					else {
						instruction = OSSwapBigToHostInt32(*fixUpPtr);
						dstAddr = (nextReloc->r_address() << 16) | (instruction & 0x0000FFFC);
						addFixUp(srcAddr, dstAddr, Reference::ppcFixupAbsLow14, 0);
					}
				}
				break;
			case PPC_RELOC_HI16:
				{
					if ( nextReloc->r_type() != PPC_RELOC_PAIR ) {
						printf("PPC_RELOC_HI16 missing following pair\n");
						break;
					}
					srcAddr = sect->addr() + reloc->r_address();
					if ( reloc->r_extern() ) {
						const macho_nlist* targetSymbol = &fSymbols[reloc->r_symbolnum()];
						const char* targetName = &fStrings[targetSymbol->n_strx()];
						src = findAtomCoveringOffset(srcAddr);	
						instruction = OSSwapBigToHostInt32(*fixUpPtr);
						dstAddr = ((instruction & 0x0000FFFF) << 16) | (nextReloc->r_address() & 0x0000FFFF);
						src->addByNameReference(srcAddr - src->fOffset, Reference::ppcFixupAbsHigh16, targetName, dstAddr, 0);
					}
					else {
						instruction = OSSwapBigToHostInt32(*fixUpPtr);
						dstAddr = ((instruction & 0x0000FFFF) << 16) | (nextReloc->r_address() & 0x0000FFFF);
						addFixUp(srcAddr, dstAddr, Reference::ppcFixupAbsHigh16, 0);
					}
				}
				break;
			case PPC_RELOC_HA16:
				{
					if ( nextReloc->r_type() != PPC_RELOC_PAIR ) {
						printf("PPC_RELOC_HA16 missing following pair\n");
						break;
					}
					srcAddr = sect->addr() + reloc->r_address();
					if ( reloc->r_extern() ) {
						const macho_nlist* targetSymbol = &fSymbols[reloc->r_symbolnum()];
						const char* targetName = &fStrings[targetSymbol->n_strx()];
						instruction = OSSwapBigToHostInt32(*fixUpPtr);
						int16_t lowBits = (nextReloc->r_address() & 0x0000FFFF);
						dstAddr = ((instruction & 0x0000FFFF) << 16) + (int32_t)lowBits;
						src = findAtomCoveringOffset(srcAddr);	
						src->addByNameReference(srcAddr - src->fOffset, Reference::ppcFixupAbsHigh16AddLow, targetName, dstAddr, 0);
					}
					else {
						instruction = OSSwapBigToHostInt32(*fixUpPtr);
						int16_t lowBits = (nextReloc->r_address() & 0x0000FFFF);
						dstAddr = ((instruction & 0x0000FFFF) << 16) + (int32_t)lowBits;
						addFixUp(srcAddr, dstAddr, Reference::ppcFixupAbsHigh16AddLow, 0);
					}
				}
				break;
			case GENERIC_RELOC_VANILLA:
				{
					srcAddr = sect->addr() + reloc->r_address();
					Atom* srcAtom = findAtomCoveringOffset(srcAddr);
					uint32_t offsetInSrcAtom = srcAddr - srcAtom->fOffset;
					macho_uintptr_t pointerValue = ENDIAN_SWAP_POINTER(*((macho_uintptr_t*)fixUpPtr));
					if ( reloc->r_extern() ) {
						const macho_nlist* targetSymbol = &fSymbols[reloc->r_symbolnum()];
						uint8_t type = targetSymbol->n_type() & N_TYPE;
						if ( type == N_UNDF ) {
							const char* targetName = &fStrings[targetSymbol->n_strx()];
							macho_uintptr_t addend = pointerValue;
							// ppc lazy pointers have initial reference to dyld_stub_binding_helper
							if ( (srcAtom->fSection->flags() & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS ) {
								std::vector<ObjectFile::Reference*>&  refs = srcAtom->getReferences();
								if ( refs.size() > 0 ) {
									Reference* ref = (Reference*)refs[0];
		#if defined(ARCH_PPC64)
									 // hack to work around bad crt1.o in Mac OS X 10.4
									targetName = "dyld_stub_binding_helper";
		#endif
									ref->setFromTargetName(targetName);
								}
								else {
									fprintf(stderr, "lazy pointer (%s) should only have one reference - has %ld references\n", srcAtom->getDisplayName(), refs.size());
								}
							} 
		#if defined(ARCH_PPC64)
							// hack to work around bad crt1.o in Mac OS X 10.4
							else if ( (srcAtom->fSection->flags() & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS ) {
								// ignore extra relocation
							}
		#endif
							else {
								srcAtom->addByNameReference(offsetInSrcAtom, Reference::pointer, targetName, addend, 0);
							}
						}
						else {
							dstAddr = targetSymbol->n_value();
							Atom* dstAtom = findAtomCoveringOffset(dstAddr);
							macho_uintptr_t addend = pointerValue;
							// ppc lazy pointers have initial reference to dyld_stub_binding_helper
							if ( (srcAtom->fSection->flags() & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS ) {
								std::vector<ObjectFile::Reference*>&  refs = srcAtom->getReferences();
								if ( refs.size() > 0 ) {
									Reference* ref = (Reference*)refs[0];
									ref->setFromTarget(*dstAtom);
									ref->setFromTargetOffset(dstAddr - dstAtom->fOffset);
								}
								else {
									fprintf(stderr, "lazy pointer (%s) should only have one reference - has %ld references\n", srcAtom->getDisplayName(), refs.size());
								}
							} 
							else {
								srcAtom->addReference(offsetInSrcAtom, Reference::pointer, *dstAtom, addend, 0);
							}
						}
					}
					else {
						Atom* dstAtom = findAtomCoveringOffset(pointerValue);
						// lazy pointers have references to dyld_stub_binding_helper which need to be ignored
						if ( (srcAtom->fSection->flags() & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS ) {
							std::vector<ObjectFile::Reference*>&  refs = srcAtom->getReferences();
							if ( refs.size() > 0 ) {
								Reference* ref = (Reference*)refs[0];
								ref->setFromTarget(*dstAtom);
								ref->setFromTargetOffset(pointerValue - dstAtom->fOffset);
							}
							else {
								fprintf(stderr, "lazy pointer (%s) should only have one reference - has %ld references\n", srcAtom->getDisplayName(), refs.size());
							}
						}
						else {
							srcAtom->addReference(offsetInSrcAtom, Reference::pointer, *dstAtom, pointerValue-dstAtom->fOffset, 0);
						}
					}
				}
				break;
			case PPC_RELOC_JBSR:
				// ignore for now
				break;
#endif
#if defined(ARCH_I386)
			case GENERIC_RELOC_VANILLA:
				{
					srcAddr = sect->addr() + reloc->r_address();
					src = findAtomCoveringOffset(srcAddr);
					if ( reloc->r_length() != 2 )
						throw "bad vanilla relocation length";
					Reference::Kind kind;
					macho_uintptr_t pointerValue = ENDIAN_SWAP_POINTER(*((macho_uintptr_t*)fixUpPtr));
					if ( reloc->r_pcrel() ) {
						kind = Reference::x86FixupBranch32;
						pointerValue += srcAddr + sizeof(macho_uintptr_t);
					}
					else {
						kind = Reference::pointer;
					}
					uint32_t offsetInSrcAtom = srcAddr - src->fOffset;
					if ( reloc->r_extern() ) {
						const macho_nlist* targetSymbol = &fSymbols[reloc->r_symbolnum()];
						uint8_t type = targetSymbol->n_type() & N_TYPE;
						if ( type == N_UNDF ) {
							const char* targetName = &fStrings[targetSymbol->n_strx()];
							macho_uintptr_t addend = pointerValue;
							src->addByNameReference(offsetInSrcAtom, kind, targetName, addend, 0);
						}
						else {
							dstAddr = targetSymbol->n_value();
							dst = findAtomCoveringOffset(dstAddr);
							macho_uintptr_t addend = pointerValue - dstAddr;
							src->addReference(offsetInSrcAtom, kind, *dst, addend, 0);
						}
					}
					else {
						dst = findAtomCoveringOffset(pointerValue);	
						// lazy pointers have references to dyld_stub_binding_helper which need to be ignored
						if ( (src->fSection->flags() & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS ) {
							std::vector<ObjectFile::Reference*>&  refs = src->getReferences();
							if ( refs.size() == 1 ) {
								Reference* ref = (Reference*)refs[0];
								ref->setFromTarget(*dst);
								ref->setFromTargetOffset(pointerValue - dst->fOffset);
							}
							else {
								fprintf(stderr, "lazy pointer (%s) should only have one reference - has %ld references\n", src->getDisplayName(), refs.size());
							}
						}
						else if ( ((uint8_t*)fixUpPtr)[-1] == 0xE8 )  // special case call instruction
							this->addCallSiteReference(*src, offsetInSrcAtom, kind, *dst, 0, pointerValue - dst->fOffset);
						else
							src->addReference(offsetInSrcAtom, kind, *dst, 0, 0);
					}
				}
				break;
#endif

			default:
				printf("unknown relocation type %d\n", reloc->r_type());
		}
	}
	else {
		const macho_scattered_relocation_info* sreloc = (macho_scattered_relocation_info*)reloc;
		srcAddr = sect->addr() + sreloc->r_address();
		dstAddr = sreloc->r_value();
		fixUpPtr = (uint32_t*)((char*)(fHeader) + sect->offset() + sreloc->r_address());
		const macho_scattered_relocation_info* nextSReloc = &sreloc[1];
		const macho_relocation_info* nextReloc = &reloc[1];
		// file format allows pair to be scattered or not
		bool nextRelocIsPair = false;
		uint32_t nextRelocAddress = 0;
		uint32_t nextRelocValue = 0;
		if ( (nextReloc->r_address() & R_SCATTERED) == 0 ) {
			if ( nextReloc->r_type() == PPC_RELOC_PAIR ) {
				nextRelocIsPair = true;
				nextRelocAddress = nextReloc->r_address();
			}
		}
		else {
			if ( nextSReloc->r_type() == PPC_RELOC_PAIR ) {
				nextRelocIsPair = true;
				nextRelocAddress = nextSReloc->r_address();
				nextRelocValue = nextSReloc->r_value();
			}
		}
		switch (sreloc->r_type()) {
			case GENERIC_RELOC_VANILLA:
				{
					macho_uintptr_t betterDstAddr = ENDIAN_SWAP_POINTER(*((macho_uintptr_t*)fixUpPtr));
					//fprintf(stderr, "pointer reloc: srcAddr=0x%08X, dstAddr=0x%08X, pointer=0x%08lX\n", srcAddr, dstAddr, betterDstAddr);
					// with a scattered relocation we get both the target (sreloc->r_value()) and the target+offset (*fixUpPtr)
					Atom* src = findAtomCoveringOffset(srcAddr);
					Atom* dst = findAtomCoveringOffset(dstAddr);
					src->addReference(srcAddr - src->fOffset, Reference::pointer, *dst, betterDstAddr - dst->fOffset, 0);
				}
				break;
#if defined(ARCH_PPC) || defined(ARCH_PPC64)
			case PPC_RELOC_BR24:
				{
					instruction = OSSwapBigToHostInt32(*fixUpPtr);
					if ( (instruction & 0x4C000000) == 0x48000000 ) {
						int32_t displacement = (instruction & 0x03FFFFFC);
						if ( (displacement & 0x02000000) != 0 )
							displacement |= 0xFC000000;
						srcAddr = sect->addr() + sreloc->r_address();
						dstAddr = sreloc->r_value();
						src = findAtomCoveringOffset(srcAddr);
						dst = findAtomCoveringOffset(dstAddr);	
						this->addCallSiteReference(*src, srcAddr - src->fOffset, Reference::ppcFixupBranch24, *dst, 0, srcAddr + displacement - sreloc->r_value());
					}
				}
				break;
			case PPC_RELOC_LO16_SECTDIFF:
				{
					if ( ! nextRelocIsPair) {
						printf("PPC_RELOC_LO16_SECTDIFF missing following PAIR\n");
						break;
					}
					src = findAtomCoveringOffset(srcAddr);	
					dst = findAtomCoveringOffset(dstAddr);	
					instruction = OSSwapBigToHostInt32(*fixUpPtr);
					int16_t lowBits = (instruction & 0xFFFF);
					int32_t displacement = (nextRelocAddress << 16) + (int32_t)lowBits;
					if ( (lowBits & 0x8000) != 0 )
						displacement += 0x10000;
					uint32_t picBaseOffset = nextRelocValue - src->fOffset;
					int64_t dstOffset = src->fOffset + picBaseOffset + displacement - dst->fOffset;
					src->addReference(srcAddr - src->fOffset, Reference::ppcFixupPicBaseLow16, *dst, dstOffset, picBaseOffset);
				}
				break;
			case PPC_RELOC_LO14_SECTDIFF:
				{
					if ( ! nextRelocIsPair) {
						printf("PPC_RELOC_LO14_SECTDIFF missing following PAIR\n");
						break;
					}
					src = findAtomCoveringOffset(srcAddr);	
					dst = findAtomCoveringOffset(dstAddr);	
					instruction = OSSwapBigToHostInt32(*fixUpPtr);
					int16_t lowBits = (instruction & 0xFFFC);
					int32_t displacement = (nextRelocAddress << 16) + (int32_t)lowBits;
					if ( (lowBits & 0x8000) != 0 )
						displacement += 0x10000;
					uint32_t picBaseOffset = nextRelocValue - src->fOffset;
					int64_t dstOffset = src->fOffset + picBaseOffset + displacement - dst->fOffset;
					src->addReference(srcAddr - src->fOffset, Reference::ppcFixupPicBaseLow14, *dst, dstOffset, picBaseOffset);
				}
				break;
			case PPC_RELOC_HA16_SECTDIFF:
				{
					if ( ! nextRelocIsPair) {
						printf("PPC_RELOC_LO14_SECTDIFF missing following PAIR\n");
						break;
					}
					src = findAtomCoveringOffset(srcAddr);	
					dst = findAtomCoveringOffset(dstAddr);	
					instruction = OSSwapBigToHostInt32(*fixUpPtr);
					int16_t lowBits = (nextRelocAddress & 0x0000FFFF);
					int32_t displacement = ((instruction & 0x0000FFFF) << 16) + (int32_t)lowBits;
					uint32_t picBaseOffset = nextRelocValue - src->fOffset;
					int64_t dstOffset = src->fOffset + picBaseOffset + displacement - dst->fOffset;
					src->addReference(srcAddr - src->fOffset, Reference::ppcFixupPicBaseHigh16, *dst, dstOffset, picBaseOffset);
				}
				break;
			case PPC_RELOC_LO14:
				{
					if ( ! nextRelocIsPair) {
						printf("PPC_RELOC_LO14 missing following PAIR\n");
						break;
					}
					src = findAtomCoveringOffset(srcAddr);	
					dst = findAtomCoveringOffset(dstAddr);	
					instruction = OSSwapBigToHostInt32(*fixUpPtr);
					int16_t lowBits = (instruction & 0xFFFC);
					uint32_t betterDstAddr = (nextRelocAddress << 16) + (int32_t)lowBits;
					if ( (lowBits & 0x8000) != 0 )
						betterDstAddr += 0x10000;
					src->addReference(srcAddr - src->fOffset, Reference::ppcFixupAbsLow14, *dst, betterDstAddr - dst->fOffset, 0);
				}
				break;
			case PPC_RELOC_LO16:
				{
					if ( ! nextRelocIsPair) {
						printf("PPC_RELOC_LO16 missing following PAIR\n");
						break;
					}
					src = findAtomCoveringOffset(srcAddr);	
					dst = findAtomCoveringOffset(dstAddr);	
					instruction = OSSwapBigToHostInt32(*fixUpPtr);
					int16_t lowBits = (instruction & 0xFFFF);
					uint32_t betterDstAddr = (nextRelocAddress << 16) + (int32_t)lowBits;
					if ( (lowBits & 0x8000) != 0 )
						betterDstAddr += 0x10000;
					src->addReference(srcAddr - src->fOffset, Reference::ppcFixupAbsLow16, *dst, betterDstAddr - dst->fOffset, 0);
				}
				break;
			case PPC_RELOC_HA16:
				{
					if ( ! nextRelocIsPair) {
						printf("PPC_RELOC_HA16 missing following PAIR\n");
						break;
					}
					src = findAtomCoveringOffset(srcAddr);	
					dst = findAtomCoveringOffset(dstAddr);	
					instruction = OSSwapBigToHostInt32(*fixUpPtr);
					int16_t lowBits = (nextRelocAddress & 0xFFFF);
					uint32_t betterDstAddr = ((instruction & 0xFFFF) << 16) + (int32_t)lowBits;
					src->addReference(srcAddr - src->fOffset, Reference::ppcFixupAbsHigh16AddLow, *dst, betterDstAddr - dst->fOffset, 0);
				}
				break;
			case PPC_RELOC_SECTDIFF:
			case PPC_RELOC_LOCAL_SECTDIFF:
				{
					const macho_scattered_relocation_info* nextReloc = &sreloc[1];
					if ( nextReloc->r_type() != PPC_RELOC_PAIR ) {
						printf("PPC_RELOC_SECTDIFF missing following pair\n");
						break;
					}
					srcAddr = sect->addr() + sreloc->r_address();
					uint32_t toAddr = sreloc->r_value();
					uint32_t fromAddr = nextReloc->r_value();
					src = findAtomCoveringOffset(srcAddr);	
					Atom* to = findAtomCoveringOffset(toAddr);	
					Atom* from = findAtomCoveringOffset(fromAddr);	
					//macho_intptr_t pointerValue = *(macho_intptr_t*)fixUpPtr;
					//uint64_t toOffset = to->fOffset;
					//uint64_t fromOffset = from->fOffset;
					//int64_t pointerValue64 = pointerValue;
					//uint64_t addend = pointerValue64 - (toOffset - fromOffset);
					Reference::Kind kind = Reference::pointer32Difference;
					if ( sreloc->r_length() == 3 )
						kind =  Reference::pointer64Difference;
					src->addDifferenceReference(srcAddr - src->fOffset, kind, *to, toAddr - to->fOffset, *from, fromAddr - from->fOffset);
				}
				break;
			case PPC_RELOC_PAIR:
				break;
			case PPC_RELOC_HI16_SECTDIFF:
				printf("unexpected scattered relocation type PPC_RELOC_HI16_SECTDIFF\n");
				break;
#endif
#if defined(ARCH_I386)
			case GENERIC_RELOC_SECTDIFF:
			case GENERIC_RELOC_LOCAL_SECTDIFF:
				{
					if ( nextSReloc->r_type() != GENERIC_RELOC_PAIR ) {
						printf("GENERIC_RELOC_SECTDIFF missing following pair\n");
						break;
					}
					srcAddr = sect->addr() + sreloc->r_address();
					uint32_t toAddr = sreloc->r_value();
					uint32_t fromAddr = nextSReloc->r_value();
					src = findAtomCoveringOffset(srcAddr);	
					Atom* to = findAtomCoveringOffset(toAddr);	
					Atom* from = findAtomCoveringOffset(fromAddr);	
					Reference::Kind kind = Reference::pointer32Difference;
					if ( sreloc->r_length() != 2 )
						throw "bad length for GENERIC_RELOC_SECTDIFF";
					src->addDifferenceReference(srcAddr - src->fOffset, kind, *to, toAddr - to->fOffset, *from, fromAddr - from->fOffset);
				}
				break;
			case GENERIC_RELOC_PAIR:
				// do nothing, already used via a look ahead
				break;
#endif
			default:
				printf("unknown scattered relocation type %d\n", sreloc->r_type());
		}
		
	}
}


void Reader::addFixUp(uint32_t srcAddr, uint32_t dstAddr, Reference::Kind kind, uint32_t picBaseAddr)
{
	Atom* src = findAtomCoveringOffset(srcAddr);	
	Atom* dst = findAtomCoveringOffset(dstAddr);	
	src->addReference(srcAddr - src->fOffset, kind, *dst, dstAddr - dst->fOffset, picBaseAddr - src->fOffset);
}

Atom* Reader::findAtomCoveringOffset(uint32_t offset)
{
#if 1
	// binary search of sorted atoms
	Atom** base = &fAtoms[0];
	for (uint32_t n = fAtoms.size(); n > 0; n /= 2) {
		Atom** pivot = &base[n/2];
		Atom* pivotAtom = *pivot;
		if ( pivotAtom->fOffset <= offset ) {
			if ( offset < (pivotAtom->fOffset + pivotAtom->fSize) )
				return pivotAtom;
			// key > pivot 
			// move base to symbol after pivot
			base = &pivot[1];
			--n; 
		}
		else {
			// key < pivot 
			// keep same base
		}
	}
	// possible that last atom is zero length
	Atom* lastAtom = fAtoms.back();
	if ( (lastAtom->fOffset == offset) && (lastAtom->fSize == 0) )
		return lastAtom;
#else
	const uint32_t atomCount = fAtoms.size();
	for (uint32_t i=0; i < atomCount; ++i) {
		Atom* atom = fAtoms[i];
		if ( (atom->fOffset <= offset) && (offset < (atom->fOffset + atom->fSize)) )
			return atom;
	}
#endif
	throwf("address 0x%08X is not in any atom", offset);
}

uint32_t Reader::findAtomIndex(const Atom& atom)
{
	const Atom* target = &atom;
	const uint32_t atomCount = fAtoms.size();
	for (uint32_t i=0; i < atomCount; ++i) {
		Atom* anAtom = fAtoms[i];
		if ( anAtom == target )
			return i;
	}
	return 0xffffffff;
}

static void insertOffset(std::set<uint32_t>& offsets, uint32_t value)
{
	//fprintf(stderr, "cleave point at 0x%08X\n", value);
	offsets.insert(value);
}

void Reader::insertOffsetIfNotText(std::set<uint32_t>& offsets, uint32_t value)
{
	const macho_section* sect = findSectionCoveringOffset(value);
	if ( (sect == NULL) || (strcmp(sect->segname(),"__TEXT") != 0) || (strncmp(sect->sectname(),"__text", 6) != 0) ) {
		offsets.insert(value);
	}
}

void Reader::insertOffsetIfText(std::set<uint32_t>& offsets, uint32_t value)
{
	const macho_section* sect = findSectionCoveringOffset(value);
	if ( (sect != NULL) && (strcmp(sect->segname(),"__TEXT") == 0) && (strncmp(sect->sectname(),"__text", 6) == 0) ) {
		//fprintf(stderr, "don't cleave point at 0x%08X\n", value);
		offsets.insert(value);
	}
}

const macho_section* Reader::findSectionCoveringOffset(uint32_t offset)
{
	const macho_section* const sectionsStart = (macho_section*)((char*)fSegment + sizeof(macho_segment_command));
	const macho_section* const sectionsEnd = &sectionsStart[fSegment->nsects()];
	for (const macho_section* sect=sectionsStart; sect < sectionsEnd; ++sect) {
		if ( (sect->addr() <= offset) && (offset < (sect->addr() + sect->size())) )
			return sect;
	}
	return NULL;
}


void Reader::buildOffsetsSet(const macho_relocation_info* reloc, const macho_section* sect, std::set<uint32_t>& cleavePoints, std::set<uint32_t>& dontCleavePoints)
{
#if defined(ARCH_PPC) || defined(ARCH_PPC64)
	uint32_t targetAddr;
#endif
	if ( (reloc->r_address() & R_SCATTERED) == 0 ) {
		uint32_t* fixUpPtr = (uint32_t*)((char*)(fHeader) + sect->offset() + reloc->r_address());
#if defined(ARCH_PPC) || defined(ARCH_PPC64)
		uint32_t instruction;
#endif
		switch ( reloc->r_type() ) {
#if defined(ARCH_PPC) || defined(ARCH_PPC64)
			case PPC_RELOC_BR14:
				// do nothing. local branch should not cleave
				break;
			case PPC_RELOC_BR24:
				{
					if ( ! reloc->r_extern() ) {
						instruction = OSSwapBigToHostInt32(*fixUpPtr);
						if ( (instruction & 0x4C000000) == 0x48000000 ) {
							int32_t displacement = (instruction & 0x03FFFFFC);
							if ( (displacement & 0x02000000) != 0 )
								displacement |= 0xFC000000;
							//cleavePoints.insert(reloc->r_address() + displacement);
							insertOffset(cleavePoints, sect->addr() + reloc->r_address() + displacement);
						}
					}
				}
				break;
			case PPC_RELOC_PAIR:
				// skip, processed by a look ahead 
				break;
			case PPC_RELOC_LO16:
				{
					const macho_relocation_info* nextReloc = &reloc[1];
					if ( nextReloc->r_type() != PPC_RELOC_PAIR ) {
						printf("PPC_RELOC_LO16 missing following pair\n");
						break;
					}
					if ( ! reloc->r_extern() ) {
						instruction = OSSwapBigToHostInt32(*fixUpPtr);
						targetAddr = (nextReloc->r_address() << 16) | (instruction & 0x0000FFFF);
						//cleavePoints.insert(targetAddr);
						insertOffset(cleavePoints, (targetAddr));
					}
				}
				break;
			case PPC_RELOC_LO14:
				{
					const macho_relocation_info* nextReloc = &reloc[1];
					if ( nextReloc->r_type() != PPC_RELOC_PAIR ) {
						printf("PPC_RELOC_LO14 missing following pair\n");
						break;
					}
					if ( ! reloc->r_extern() ) {
						instruction = OSSwapBigToHostInt32(*fixUpPtr);
						targetAddr = (nextReloc->r_address() << 16) | (instruction & 0x0000FFFC);
						//cleavePoints.insert(targetAddr);
						insertOffset(cleavePoints, (targetAddr));
					}
				}
				break;
			case PPC_RELOC_HI16:
				{
					const macho_relocation_info* nextReloc = &reloc[1];
					if ( nextReloc->r_type() != PPC_RELOC_PAIR ) {
						printf("PPC_RELOC_HI16 missing following pair\n");
						break;
					}
					if ( ! reloc->r_extern() ) {
						instruction = OSSwapBigToHostInt32(*fixUpPtr);
						targetAddr = ((instruction & 0x0000FFFF) << 16) | (nextReloc->r_address() & 0x0000FFFF);
						//cleavePoints.insert(targetAddr);
						insertOffset(cleavePoints, targetAddr);
					}
				}
				break;
			case PPC_RELOC_HA16:
				{
					const macho_relocation_info* nextReloc = &reloc[1];
					if ( nextReloc->r_type() != PPC_RELOC_PAIR ) {
						printf("PPC_RELOC_HA16 missing following pair\n");
						break;
					}
					if ( ! reloc->r_extern() ) {
						instruction = OSSwapBigToHostInt32(*fixUpPtr);
						int16_t lowBits = (nextReloc->r_address() & 0x0000FFFF);
						targetAddr = ((instruction & 0x0000FFFF) << 16) + (int32_t)lowBits;
						//cleavePoints.insert(targetAddr);
						insertOffset(cleavePoints, targetAddr);
					}
				}
				break;
			case PPC_RELOC_JBSR:
				// ignore for now
				break;
#endif
			case GENERIC_RELOC_VANILLA:
				{
#if defined(ARCH_PPC64)
					if  ( reloc->r_length() != 3 )
						throw "vanilla pointer relocation found that is not 8-bytes";
#elif defined(ARCH_PPC) || defined(ARCH_I386)
					if  ( reloc->r_length() != 2 )
						throw "vanilla pointer relocation found that is not 4-bytes";
#endif
					//fprintf(stderr, "addr=0x%08X, pcrel=%d, len=%d, extern=%d, type=%d\n", reloc->r_address(), reloc->r_pcrel(), reloc->r_length(), reloc->r_extern(), reloc->r_type());
					if ( !reloc->r_extern() ) {
						macho_uintptr_t pointerValue = ENDIAN_SWAP_POINTER(*((macho_uintptr_t*)fixUpPtr));
#if defined(ARCH_I386)
					// i386 stubs have internal relocs that should not cause a cleave
					if ( (sect->flags() & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS )
						break;
#endif
						if ( reloc->r_pcrel() )
							pointerValue += reloc->r_address() + sect->addr() + sizeof(macho_uintptr_t);
						// a pointer into code does not cleave the code (gcc always pointers to labels)
						insertOffsetIfNotText(cleavePoints, pointerValue);
					}
				}
				break;
			default:
				printf("unknown relocation type %d\n", reloc->r_type());
		}
	}
	else {
		const macho_scattered_relocation_info* sreloc = (macho_scattered_relocation_info*)reloc;
		switch (sreloc->r_type()) {
			case GENERIC_RELOC_VANILLA:
				insertOffset(cleavePoints, sreloc->r_value());
				break;
#if defined(ARCH_PPC) || defined(ARCH_PPC64)
			case PPC_RELOC_BR24:
				insertOffset(cleavePoints, sreloc->r_value());
				break;
			case PPC_RELOC_HA16:
			case PPC_RELOC_HI16:
			case PPC_RELOC_LO16:
			case PPC_RELOC_LO14:
			case PPC_RELOC_LO16_SECTDIFF:
			case PPC_RELOC_LO14_SECTDIFF:
			case PPC_RELOC_HA16_SECTDIFF:
			case PPC_RELOC_HI16_SECTDIFF:
				//cleavePoints.insert(sreloc->r_value());
				insertOffset(cleavePoints, sreloc->r_value());
				insertOffsetIfText(dontCleavePoints, sreloc->r_value());
				break;
			case PPC_RELOC_SECTDIFF:
			case PPC_RELOC_LOCAL_SECTDIFF:
				// these do not cleave up a .o file
				// a SECTDIFF in __TEXT probably means a jump table, and should prevent a cleave
				{
					const macho_scattered_relocation_info* nextReloc = &sreloc[1];
					if ( nextReloc->r_type() != PPC_RELOC_PAIR ) {
						printf("PPC_RELOC_SECTDIFF missing following pair\n");
						break;
					}
					insertOffsetIfText(dontCleavePoints, sreloc->r_value());
					insertOffsetIfText(dontCleavePoints, nextReloc->r_value());
				}
				break;
			case PPC_RELOC_PAIR:
				// do nothing, already used via a look ahead
				break;
#endif
#if defined(ARCH_I386)
			case GENERIC_RELOC_SECTDIFF:
			case GENERIC_RELOC_LOCAL_SECTDIFF:
				// these do not cleave up a .o file
				// a SECTDIFF in __TEXT probably means a jump table, and should prevent a cleave
				{
					const macho_scattered_relocation_info* nextReloc = &sreloc[1];
					if ( nextReloc->r_type() != GENERIC_RELOC_PAIR ) {
						printf("GENERIC_RELOC_SECTDIFF missing following pair\n");
						break;
					}
					insertOffsetIfText(dontCleavePoints, sreloc->r_value());
					insertOffsetIfText(dontCleavePoints, nextReloc->r_value());
				}
				break;
			case GENERIC_RELOC_PAIR:
				// do nothing, already used via a look ahead
				break;
#endif
			default:
				printf("unknown relocation type %d\n", sreloc->r_type());
		}
		
	}
}


Segment* Reader::makeSegmentFromSection(const macho_section* sect)
{
	// make segment object if one does not already exist
	const uint32_t segmentCount = fSegments.size();
	for (uint32_t i=0; i < segmentCount; ++i) {
		Segment* seg = fSegments[i];
		if ( strcmp(sect->segname(), seg->getName()) == 0 )
			return seg;
	}
	Segment* seg = new Segment(sect);
	fSegments.push_back(seg);
	return seg;
}

macho_uintptr_t Reader::commonsOffset()
{
	return fSegment->vmsize();
}

const char*	Reader::getPath()
{
	return fPath;
}

std::vector<class ObjectFile::Atom*>&	Reader::getAtoms()
{
	return (std::vector<class ObjectFile::Atom*>&)(fAtoms);
}

std::vector<class ObjectFile::Atom*>*	Reader::getJustInTimeAtomsFor(const char* name)
{
	// object files have no just-in-time atoms
	return NULL;
}


std::vector<ObjectFile::StabsInfo>*	Reader::getStabsDebugInfo()
{
	if ( fNonAtomStabsCount == 0 )
		return NULL;
		
	std::vector<ObjectFile::StabsInfo>* stabs = new std::vector<ObjectFile::StabsInfo>();
	stabs->reserve(fNonAtomStabsCount);
	
	for (uint32_t i=0; i < fNonAtomStabsCount; ++i) {
		const macho_nlist* sym = &fSymbols[fNonAtomStabsStartIndex+i];
		if ( (sym->n_type() & N_STAB) != 0 ) {
			ObjectFile::StabsInfo stab;
			stab.atomOffset = sym->n_value();
			stab.string		= &fStrings[sym->n_strx()];
			stab.type		= sym->n_type();
			stab.other		= sym->n_sect();
			stab.desc		= sym->n_desc();
			// for N_SO n_value is offset of first atom, but our gdb ignores this, so we omit that calculation
			if ( stab.type == N_SO ) 
				stab.atomOffset = 0;
			stabs->push_back(stab);
		}
	}
	
	// add any extra N_LSYM's not at start of symbol table
	for (std::vector<uint32_t>::iterator it=fNonAtomExtras.begin(); it != fNonAtomExtras.end(); ++it) {
		const macho_nlist* sym = &fSymbols[*it];
		ObjectFile::StabsInfo stab;
		stab.atomOffset = sym->n_value();
		stab.string		= &fStrings[sym->n_strx()];
		stab.type		= sym->n_type();
		stab.other		= sym->n_sect();
		stab.desc		= sym->n_desc();
		stabs->push_back(stab);
	}
	
	return stabs;
}

void Reader::deadStub(Atom& target)
{
	// remove stub
	fDeadAtoms.insert(&target);
		
	// remove lazy pointer
	const int stubNameLen = strlen(target.fSynthesizedName);
	char lazyName[stubNameLen+8];
	strcpy(lazyName, target.fSynthesizedName);
	strcpy(&lazyName[stubNameLen-5], "$lazy_ptr");
	const uint32_t atomCount = fAtoms.size();
	for (uint32_t i=1; i < atomCount; ++i) {
		Atom* atom = fAtoms[i];
		if ( (atom->fSynthesizedName != NULL) && (strcmp(atom->fSynthesizedName, lazyName) == 0) ) {
			fDeadAtoms.insert(atom);
			break;
		}
	}
}


void Reader::addCallSiteReference(Atom& src, uint32_t offsetInSrcAtom, Reference::Kind kind, Atom& target, uint32_t picBaseOffset, uint32_t offsetInTargetAtom)
{
	// the compiler some times produces stub to static functions and then calls the stubs
	// we need to skip the stub if a static function exists with the same name and remove the stub
	if ( target.isLazyStub() ) {
		const macho_section* section = target.fSection;
		uint32_t index = (target.fOffset - section->addr()) / section->reserved2();
		uint32_t indirectTableIndex = section->reserved1() + index;
		uint32_t symbolIndex = ENDIAN_READ32(fIndirectTable[indirectTableIndex]);
		if ( (symbolIndex & INDIRECT_SYMBOL_LOCAL) == 0 ) {
			const macho_nlist* sym = &fSymbols[symbolIndex];
			if ( (sym->n_value() != 0) && ((sym->n_type() & N_EXT) == 0) ) {
				Atom* betterTarget = this->findAtomCoveringOffset(sym->n_value());
				if ( (betterTarget != NULL) && (betterTarget->getScope() == ObjectFile::Atom::scopeTranslationUnit) ) {
					// use direct reference to static function
					src.addDirectReference(offsetInSrcAtom, kind, *betterTarget, offsetInTargetAtom, picBaseOffset);
					
					// remove stub and lazy pointer
					this->deadStub(target);
					return;
				}
			}
		}
	}
	
	// fall through to general case
	src.addReference(offsetInSrcAtom, kind, target, offsetInTargetAtom, picBaseOffset);
}


Atom::Atom(Reader& owner, const macho_nlist* symbol)
 : fOwner(owner), fSymbol(symbol), fOffset(0), fSize(0), fSection(NULL), fSegment(NULL), fSynthesizedName(NULL),
	fStabsStartIndex(0), fStabsCount(0)
{
	uint8_t type =  symbol->n_type();
	if ( (type & N_EXT) == 0 )
		fScope = ObjectFile::Atom::scopeTranslationUnit;
	else if ( (type & N_PEXT) != 0 )
		fScope = ObjectFile::Atom::scopeLinkageUnit;
	else
		fScope = ObjectFile::Atom::scopeGlobal;
	if ( (type & N_TYPE) == N_SECT ) {
		// real definition
		const macho_section* sections = (macho_section*)((char*)fOwner.fSegment + sizeof(macho_segment_command));
		fSection = &sections[fSymbol->n_sect()-1];
		fSegment = owner.makeSegmentFromSection(fSection);
		fOffset = fSymbol->n_value();
		uint8_t type = fSection->flags() & SECTION_TYPE;
		switch ( type ) {
			case S_LAZY_SYMBOL_POINTERS:
			case S_NON_LAZY_SYMBOL_POINTERS:
				{
					// get target name out of indirect symbol table
					uint32_t index = (fOffset - fSection->addr()) / sizeof(macho_uintptr_t);
					index += fSection->reserved1();
					uint32_t symbolIndex = ENDIAN_READ32(fOwner.fIndirectTable[index]);
					uint32_t strOffset = fOwner.fSymbols[symbolIndex].n_strx();
					const char* name = &fOwner.fStrings[strOffset];
					Reference* ref = this->addByNameReference(0, Reference::pointer, name, 0, 0);
					if ( type == S_LAZY_SYMBOL_POINTERS ) {
						ref->setLazy(true);
					}
				}
				break;
		}
	}
	else if ( ((type & N_TYPE) == N_UNDF) && (symbol->n_value() != 0) ) {
		// tentative definition
		fSize = symbol->n_value();
		fSection = getCommonsSection();
		fSegment = owner.makeSegmentFromSection(fSection);
		fOffset = owner.commonsOffset();
	}
	else {
		printf("unknown symbol type: %d\n", type);
	}	
}

Atom::Atom(Reader& owner, uint32_t offset)
 : fOwner(owner), fSymbol(NULL), fOffset(offset), fSize(0), fSection(NULL), fSegment(NULL), fSynthesizedName(NULL),
	fStabsStartIndex(0), fStabsCount(0)
{
	fSection = findSectionFromOffset(offset);
	fScope = ObjectFile::Atom::scopeLinkageUnit;
	fSegment = owner.makeSegmentFromSection(fSection);
	uint8_t type = fSection->flags() & SECTION_TYPE;
	switch ( type ) {
		case S_SYMBOL_STUBS:
			{
				uint32_t index = (offset - fSection->addr()) / fSection->reserved2(); 
				index += fSection->reserved1();
				uint32_t symbolIndex = ENDIAN_READ32(fOwner.fIndirectTable[index]);
				uint32_t strOffset = fOwner.fSymbols[symbolIndex].n_strx();
				const char* name = &fOwner.fStrings[strOffset];
				char* str = new char[strlen(name)+8];
				strcpy(str, name);
				strcat(str, "$stub");
				fSynthesizedName = str;
			}
			break;
		case S_LAZY_SYMBOL_POINTERS:
		case S_NON_LAZY_SYMBOL_POINTERS:
			{
				uint32_t index = (offset - fSection->addr()) / sizeof(macho_uintptr_t);
				index += fSection->reserved1();
				uint32_t symbolIndex = ENDIAN_READ32(fOwner.fIndirectTable[index]);
				uint32_t strOffset = fOwner.fSymbols[symbolIndex].n_strx();
				const char* name = &fOwner.fStrings[strOffset];
				char* str = new char[strlen(name)+16];
				strcpy(str, name);
				if ( type == S_LAZY_SYMBOL_POINTERS )
					strcat(str, "$lazy_ptr");
				else
					strcat(str, "$non_lazy_ptr");
				fSynthesizedName = str;
				Reference* ref = this->addByNameReference(0, Reference::pointer, name, 0, 0);
				if ( type == S_LAZY_SYMBOL_POINTERS ) {
					ref->setLazy(true);
				}
				const macho_nlist* sym = &fOwner.fSymbols[symbolIndex];
				if ( (sym->n_type() & N_TYPE) == N_UNDF ) {
					if ( (sym->n_desc() & N_WEAK_REF) != 0 )
						ref->setWeak(true);
				}
			}
			break;
	}
}


Atom::~Atom()
{
}

macho_section Atom::fgCommonsSection;


bool Atom::isLazyStub()
{
	return ( (fSection->flags() & SECTION_TYPE) == S_SYMBOL_STUBS);
}

const macho_section* Atom::getCommonsSection() {
	if ( strcmp(fgCommonsSection.sectname(), "__common") != 0 ) {
		fgCommonsSection.set_sectname("__common");
		fgCommonsSection.set_segname("__DATA");
		fgCommonsSection.set_flags(S_ZEROFILL);
	}
	return &fgCommonsSection;
}

ObjectFile::Reader* Atom::getFile() const
{
	return &fOwner;
}


const char* Atom::getName() const
{
	if ( fSymbol != NULL )
		return &fOwner.fStrings[fSymbol->n_strx()];
	else
		return fSynthesizedName;
}

const char* Atom::getDisplayName() const
{
	if ( fSymbol != NULL )
		return &fOwner.fStrings[fSymbol->n_strx()];
	
	if ( fSynthesizedName != NULL )
		return fSynthesizedName;
		
	static char temp[32];
	sprintf(temp, "atom #%u", fOwner.findAtomIndex(*this));
	return temp;
}

ObjectFile::Atom::Scope Atom::getScope() const
{
	return fScope;
}

void Atom::setScope(ObjectFile::Atom::Scope newScope)
{
	fScope = newScope;
}


bool Atom::isWeakDefinition() const
{
	if ( isTentativeDefinition() )
		return true;
	if ( fSymbol != NULL ) 
		return ( (fSymbol->n_desc() & N_WEAK_DEF) != 0 );
	uint8_t type = fSection->flags() & SECTION_TYPE;
	switch ( type ) {
		case S_SYMBOL_STUBS:
		case S_LAZY_SYMBOL_POINTERS:
		case S_NON_LAZY_SYMBOL_POINTERS:
			return true;
	}
	return false;
}

bool Atom::isTentativeDefinition() const
{
	return (fSection == &fgCommonsSection);
}

bool Atom::isCoalesableByName() const
{
	uint8_t type = fSection->flags() & SECTION_TYPE;
	switch ( type ) {
		case S_SYMBOL_STUBS:
		case S_COALESCED:
			return true;
	};
	if ( isTentativeDefinition() )
		return true;
	return false;
}

bool Atom::isCoalesableByValue() const
{
	uint8_t type = fSection->flags() & SECTION_TYPE;
	switch ( type ) {
		case S_CSTRING_LITERALS:
		case S_4BYTE_LITERALS:
		case S_8BYTE_LITERALS:
			return true;
	};
	return false;
}

bool Atom::isZeroFill() const
{
	return ((fSection->flags() & SECTION_TYPE) == S_ZEROFILL);
}

bool Atom::dontDeadStrip() const
{
	if ( fSymbol != NULL ) 
		return ( (fSymbol->n_desc() & N_NO_DEAD_STRIP) != 0 );
	return false;
}


bool Atom::dontStripName() const
{
	if ( fSymbol != NULL ) 
		return ( (fSymbol->n_desc() & REFERENCED_DYNAMICALLY) != 0 );
	return false;
}

bool Atom::isImportProxy() const
{
	return false;
}
	

uint64_t Atom::getSize() const
{
	//return fOffset;
	return fSize;
}


std::vector<ObjectFile::Reference*>& Atom::getReferences() const
{
	return (std::vector<ObjectFile::Reference*>&)(fReferences);
}

bool Atom::mustRemainInSection() const
{
	return true;
}

const char*	 Atom::getSectionName() const
{
	if ( strlen(fSection->sectname()) > 15 ) {
		static char temp[18];
		strncpy(temp, fSection->sectname(), 16);
		temp[17] = '\0';
		return temp;
	}
	return fSection->sectname();
}

Segment& Atom::getSegment() const
{
	return *fSegment;
}

bool Atom::requiresFollowOnAtom() const
{
	// requires follow-on if built with old compiler and not the last atom
	if ( (fOwner.fHeader->flags() & MH_SUBSECTIONS_VIA_SYMBOLS) == 0) {
		if ( fOwner.findAtomIndex(*this) < (fOwner.fAtoms.size()-1) )
			return true;
	}
	return false;
}

ObjectFile::Atom& Atom::getFollowOnAtom() const
{
	uint32_t myIndex = fOwner.findAtomIndex(*this);
	return *fOwner.fAtoms[myIndex+1];
}

std::vector<ObjectFile::StabsInfo>* Atom::getStabsDebugInfo() const
{
	if ( fStabsCount == 0 )
		return NULL;
		
	std::vector<ObjectFile::StabsInfo>* stabs = new std::vector<ObjectFile::StabsInfo>();
	stabs->reserve(fStabsCount);
	
	for (uint32_t i=0; i < fStabsCount; ++i) {
		const macho_nlist* sym = &fOwner.fSymbols[fStabsStartIndex+i];
		if ( (sym->n_type() & N_STAB) != 0 ) {
			ObjectFile::StabsInfo stab;
			stab.atomOffset = sym->n_value();
			stab.string		= &fOwner.fStrings[sym->n_strx()];
			stab.type		= sym->n_type();
			stab.other		= sym->n_sect();
			stab.desc		= sym->n_desc();
			switch ( stab.type ) {
				case N_FUN:
					if ( stab.other == 0 )
						break;
					// end of function N_FUN has size (not address) so should not be adjusted
					// fall through
				case N_BNSYM:
				case N_ENSYM:
				case N_LBRAC:
				case N_RBRAC:
				case N_SLINE:
				case N_STSYM:
				case N_LCSYM:
					// all these stab types need their value changed from an absolute address to the atom offset
					stab.atomOffset -= fOffset;
					break;
			}
			stabs->push_back(stab);
		}
	}
	
	return stabs;
}

uint8_t	Atom::getAlignment() const
{
	// mach-o file format has no alignment information for atoms - just whole sections
	if ( fSection != NULL ) {
		if ( isTentativeDefinition() ) {
			// common symbols align to their size
			// that is, a 4-byte common aligns to 4-bytes
			// to be safe, odd size commons align to the next power-of-2 size
			uint8_t alignment = (uint8_t)ceil(log2(this->getSize()));
			// limit alignment of extremely large commons to 2^15 bytes (8-page)
			if ( alignment < 15 )
				return alignment;
			else
				return 15;
		}
		else {
			// so we assume every atom requires the same alignment as the whole section
			return fSection->align();
		}
	}
	else {
		return 2;
	}
}

void Atom::copyRawContent(uint8_t buffer[]) const
{
	// copy base bytes
	if ( isZeroFill() )
		bzero(buffer, fSize);
	else {
		uint32_t fileOffset = fSection->offset() - fSection->addr() + fOffset;
		memcpy(buffer, (char*)(fOwner.fHeader)+fileOffset, fSize);
	}
}

void Atom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	const uint32_t referencesCount = fReferences.size();

	// skip copy if no fix-ups
	if ( referencesCount == 0 ) {
		uint32_t fileOffset = fSection->offset() - fSection->addr() + fOffset;
		writer.write(0, (char*)(fOwner.fHeader)+fileOffset, fSize);
		return;
	}
	
	// copy base bytes
	uint8_t buffer[this->getSize()];
	this->copyRawContent(buffer);

	// apply any fix-ups
	for (uint32_t i=0; i < referencesCount; ++i) {
		Reference* ref = fReferences[i];
		uint32_t offset = ref->getFixUpOffset();
		uint32_t* instructionPtr = (uint32_t*)&buffer[offset];
		ObjectFile::Atom& target = ref->getTarget();
		if ( &target == NULL ) {
			if ( finalLinkedImage )
				throw "target not found";
			else
				continue;
		}
		uint32_t instruction;
		uint32_t newInstruction;
		switch ( ref->getKind() ) {
			case Reference::noFixUp:
				break;
			case Reference::pointer:
				{
					//fprintf(stderr, "writeContent: %s reference to %s\n", this->getDisplayName(), target.getDisplayName());
					if ( ref->isLazyReference() && finalLinkedImage ) {
						// lazy-symbol ==> pointer contains address of dyld_stub_binding_helper (stored in "from" target)
						*((macho_uintptr_t*)instructionPtr) = ENDIAN_SWAP_POINTER(ref->getFromTarget().getAddress());
					}
					else if ( target.isImportProxy() ) {
						// external realocation ==> pointer contains addend
						*((macho_uintptr_t*)instructionPtr) = ENDIAN_SWAP_POINTER(ref->getTargetOffset());
					}
					else {
						// internal relocation
						if ( finalLinkedImage || (strcmp(target.getSectionName(), "__common") != 0) ) {
							// pointer contains target address
							//printf("Atom::writeContent() target.name=%s, target.address=0x%08llX\n", target.getDisplayName(), target.getAddress());
							*((macho_uintptr_t*)instructionPtr) = ENDIAN_SWAP_POINTER(target.getAddress() + ref->getTargetOffset());
						}
						else {
							// pointer contains addend
							*((macho_uintptr_t*)instructionPtr) = ENDIAN_SWAP_POINTER(ref->getTargetOffset());
						}
					}
				}
				break;
			case Reference::ppcFixupBranch24:
				{
					//fprintf(stderr, "bl fixup to %s at 0x%08llX, ", target.getDisplayName(), target.getAddress());
					int64_t displacement = (target.getAddress() + ref->getTargetOffset() ) - (this->getAddress() + offset);
					if ( !finalLinkedImage && target.isImportProxy() )  {
						// doing "ld -r" to an external symbol
						// the mach-o way of encoding this is that the bl instruction's target addr is the offset into the target
						displacement -= target.getAddress();
					}
					else {
						const int64_t bl_eightMegLimit = 0x00FFFFFF;
						if ( (displacement > bl_eightMegLimit) || (displacement < (-bl_eightMegLimit)) ) {
							//fprintf(stderr, "bl out of range (%lld max is +/-16M) from %s in %s to %s in %s\n", displacement, this->getDisplayName(), this->getFile()->getPath(), target.getDisplayName(), target.getFile()->getPath());
							throwf("bl out of range (%lld max is +/-16M) from %s in %s to %s in %s", displacement, this->getDisplayName(), this->getFile()->getPath(), target.getDisplayName(), target.getFile()->getPath());
						}
					}
					instruction = OSReadBigInt32(instructionPtr, 0);
					newInstruction = (instruction & 0xFC000003) | ((uint32_t)displacement & 0x03FFFFFC);
					//fprintf(stderr, "bl fixup: 0x%08X -> 0x%08X\n", instruction, newInstruction);
					OSWriteBigInt32(instructionPtr, 0, newInstruction);			
				}
				break;
			case Reference::ppcFixupBranch14:
				break;
			case Reference::ppcFixupPicBaseLow16:
				{
					uint64_t targetAddr = target.getAddress() + ref->getTargetOffset();
					uint64_t picBaseAddr = this->getAddress() + ref->getFromTargetOffset();
					int64_t displacement = targetAddr - picBaseAddr;
					const int64_t picbase_twoGigLimit = 0x80000000;
					if ( (displacement > picbase_twoGigLimit) || (displacement < (-picbase_twoGigLimit)) )
						throw "32-bit pic-base out of range";
					uint16_t instructionLowHalf = (displacement & 0xFFFF);
					instruction = OSReadBigInt32(instructionPtr, 0);
					newInstruction = (instruction & 0xFFFF0000) | instructionLowHalf;
					OSWriteBigInt32(instructionPtr, 0, newInstruction);			
				}
				break;
			case Reference::ppcFixupPicBaseLow14:
				{
					uint64_t targetAddr = target.getAddress() + ref->getTargetOffset();
					uint64_t picBaseAddr = this->getAddress() + ref->getFromTargetOffset();
					int64_t displacement = targetAddr - picBaseAddr;
					const int64_t picbase_twoGigLimit = 0x80000000;
					if ( (displacement > picbase_twoGigLimit) || (displacement < (-picbase_twoGigLimit)) )
						throw "32-bit pic-base out of range";
					uint16_t instructionLowHalf = (displacement & 0xFFFF);
					if ( (instructionLowHalf & 0x3) != 0 )
						throw "bad address for lo14 instruction fix-up";
					instruction = OSReadBigInt32(instructionPtr, 0);
					newInstruction = (instruction & 0xFFFF0003) | instructionLowHalf;
					OSWriteBigInt32(instructionPtr, 0, newInstruction);			
				}
				break;
			case Reference::ppcFixupPicBaseHigh16:
				{
					uint64_t targetAddr = target.getAddress() + ref->getTargetOffset();
					uint64_t picBaseAddr = this->getAddress() + ref->getFromTargetOffset();
					int64_t displacement = targetAddr - picBaseAddr;
					const int64_t picbase_twoGigLimit = 0x80000000;
					if ( (displacement > picbase_twoGigLimit) || (displacement < (-picbase_twoGigLimit)) ) 
						throw "32-bit pic-base out of range";
					uint16_t instructionLowHalf = displacement >> 16;
					if ( (displacement & 0x00008000) != 0 ) 
						++instructionLowHalf;
					instruction = OSReadBigInt32(instructionPtr, 0);
					newInstruction = (instruction & 0xFFFF0000) | instructionLowHalf;
					OSWriteBigInt32(instructionPtr, 0, newInstruction);			
				}
				break;
			case Reference::ppcFixupAbsLow16:
				{
					int64_t addr = target.getAddress() + ref->getTargetOffset();
					if ( !finalLinkedImage && target.isImportProxy() )
						addr -= target.getAddress() ;
					uint16_t instructionLowHalf = (addr & 0xFFFF);
					instruction = OSReadBigInt32(instructionPtr, 0);
					newInstruction = (instruction & 0xFFFF0000) | instructionLowHalf;
					OSWriteBigInt32(instructionPtr, 0, newInstruction);			
				}
				break;
			case Reference::ppcFixupAbsLow14:
				{
					int64_t addr = target.getAddress() + ref->getTargetOffset();
					if ( !finalLinkedImage && target.isImportProxy() )
						addr -= target.getAddress() ;
					uint16_t instructionLowHalf = (addr & 0xFFFF);
					if ( (instructionLowHalf & 0x3) != 0 )
						throw "bad address for lo14 instruction fix-up";
					instruction = OSReadBigInt32(instructionPtr, 0);
					newInstruction = (instruction & 0xFFFF0003) | instructionLowHalf;
					OSWriteBigInt32(instructionPtr, 0, newInstruction);			
				}
				break;
			case Reference::ppcFixupAbsHigh16:
				{
					int64_t addr = target.getAddress() + ref->getTargetOffset();
					if ( !finalLinkedImage && target.isImportProxy() )
						addr -= target.getAddress() ;
					uint16_t hi16 = (addr >> 16);
					instruction = OSReadBigInt32(instructionPtr, 0);
					newInstruction = (instruction & 0xFFFF0000) | hi16;
					OSWriteBigInt32(instructionPtr, 0, newInstruction);	
				}
				break;
			case Reference::ppcFixupAbsHigh16AddLow:
				{
					int64_t addr = target.getAddress() + ref->getTargetOffset();
					if ( !finalLinkedImage && target.isImportProxy() )
						addr -= target.getAddress() ;
					if ( addr & 0x00008000 )
						addr += 0x00010000;
					instruction = OSReadBigInt32(instructionPtr, 0);
					newInstruction = (instruction & 0xFFFF0000) | (addr >> 16);
					OSWriteBigInt32(instructionPtr, 0, newInstruction);	
				}
				break;
			case Reference::pointer32Difference:
				ENDIAN_WRITE32(*instructionPtr, target.getAddress() + ref->getTargetOffset() - (ref->getFromTarget().getAddress() + ref->getFromTargetOffset()));
				break;
			case Reference::pointer64Difference:
				*((uint64_t*)instructionPtr) = ENDIAN_SWAP64(target.getAddress() + ref->getTargetOffset() - (ref->getFromTarget().getAddress() + ref->getFromTargetOffset()));
				break;
			case Reference::x86FixupBranch32:
				{
					int64_t displacement = target.getAddress() - (this->getAddress() + offset);
					if ( target.isImportProxy() )  {
						displacement = 0;
					}
					else {
						const int64_t bl_twoGigLimit = 0x7FFFFFFF;
						if ( (displacement > bl_twoGigLimit) || (displacement < (-bl_twoGigLimit)) ) {
							//fprintf(stderr, "call out of range from %s in %s to %s in %s\n", this->getDisplayName(), this->getFile()->getPath(), target.getDisplayName(), target.getFile()->getPath());
							throw "call out of range";
						}
					}
					OSWriteLittleInt32(instructionPtr, 0, (int32_t)displacement);			
				}
				break;
		}
	}

	// write out
	writer.write(0, buffer, getSize());
}



const macho_section* Atom::findSectionFromOffset(uint32_t offset)
{
	const macho_section* const sectionsStart = (const macho_section*)( (char*)fOwner.fSegment + sizeof(macho_segment_command) );
	const macho_section* const sectionsEnd   = &sectionsStart[fOwner.fSegment->nsects()];
	for (const macho_section* s = sectionsStart; s < sectionsEnd; ++s) {
		if ( (s->addr() <= offset) && (offset < (s->addr()+s->size())) ) 
			return s;
	}
	throwf("address 0x%08X is not in any section", offset);
}

void Atom::setSize(macho_uintptr_t size)
{
	fSize = size;
}


void Atom::setFollowOnAtom(Atom&)
{

}

Reference* Atom::addReference(macho_uintptr_t offsetInSrcAtom, Reference::Kind kind, Atom& target, uint64_t offsetInTarget, uint64_t offsetInFromTarget)
{
	if ( (target.getScope() != ObjectFile::Atom::scopeTranslationUnit) && ((target.fSymbol != NULL) || (target.fSynthesizedName != NULL)) )
		return this->addByNameReference(offsetInSrcAtom, kind, target.getName(), offsetInTarget, offsetInFromTarget);
	else 
		return this->addDirectReference(offsetInSrcAtom, kind, target, offsetInTarget, offsetInFromTarget);
}


Reference* Atom::addDirectReference(macho_uintptr_t offsetInSrcAtom, Reference::Kind kind, Atom& target, uint64_t offsetInTarget, uint64_t offsetInFromTarget)
{	
	Reference* ref = new Reference(offsetInSrcAtom, kind, target, offsetInTarget, offsetInFromTarget);
	// in rare cases, there may already be a by-name reference to the same atom.  If so, replace with this direct reference
	for (std::vector<Reference*>::iterator it=fReferences.begin(); it != fReferences.end(); it++) {
		ObjectFile::Reference* aRef = *it;
		if ( (aRef->getFixUpOffset() == offsetInSrcAtom) && (aRef->getKind() == kind) ) {
			*it = ref;
			return ref;
		}
	}
	
	// note: adding to start of list because mach-o relocs are in reverse offset order in the .o file
	fReferences.insert(fReferences.begin(), ref);
	return ref;
}

Reference* Atom::addByNameReference(macho_uintptr_t offsetInSrcAtom, Reference::Kind kind, const char* targetName, uint64_t offsetInTarget, uint64_t offsetInFromTarget)
{
	Reference* ref = new Reference(offsetInSrcAtom, kind, targetName, offsetInTarget, offsetInFromTarget);
	// note: adding to start of list because mach-o relocs are in reverse offset order in the .o file
	fReferences.insert(fReferences.begin(), ref);
	return ref;
}

Reference* Atom::addDifferenceReference(macho_uintptr_t offsetInSrcAtom, Reference::Kind kind, Atom& target, uint64_t offsetInTarget, Atom& fromTarget, uint64_t offsetInFromTarget)
{
	Reference* ref = new Reference(offsetInSrcAtom, kind, target, offsetInTarget, fromTarget, offsetInFromTarget);
	// note: adding to start of list because mach-o relocs are in reverse offset order in the .o file
	fReferences.insert(fReferences.begin(), ref);
	return ref;
}


Segment::Segment(const macho_section* sect)
	: fSection(sect)
{
}

const char* Segment::getName() const
{
	return fSection->segname();
}

bool Segment::isContentReadable() const
{
	return true;
}

bool Segment::isContentWritable() const
{
	if ( strcmp(fSection->segname(), "__DATA") == 0 )
		return true;
	if ( strcmp(fSection->segname(), "__OBJC") == 0 )
		return true;
	return false;
}

bool Segment::isContentExecutable() const
{
	return ( strcmp(fSection->segname(), "__TEXT") == 0 );
}

	
Reference::Reference(macho_uintptr_t fixUpOffset, Kind kind, const char* targetName, uint64_t offsetInTarget, uint64_t offsetInFromTarget)
 : fTarget(NULL), fFromTarget(NULL), fTargetName(targetName), fFromTargetName(NULL), fTargetOffset(offsetInTarget), fFromTargetOffset(offsetInFromTarget),
  fFixUpOffsetInSrc(fixUpOffset), fKind(kind), fLazy(false), fWeak(false)
{
}

Reference::Reference(macho_uintptr_t fixUpOffset, Kind kind, class Atom& target, uint64_t offsetInTarget, uint64_t offsetInFromTarget)
 : fTarget(&target), fFromTarget(NULL), fTargetName(NULL), fFromTargetName(NULL), fTargetOffset(offsetInTarget), fFromTargetOffset(offsetInFromTarget), 
 fFixUpOffsetInSrc(fixUpOffset), fKind(kind), fLazy(false), fWeak(false)
{
}

Reference::Reference(macho_uintptr_t fixUpOffset, Kind kind, class Atom& target, uint64_t offsetInTarget, class Atom& fromTarget, uint64_t offsetInFromTarget)
 : fTarget(&target), fFromTarget(&fromTarget), fTargetName(NULL), fFromTargetName(NULL), fTargetOffset(offsetInTarget), fFromTargetOffset(offsetInFromTarget),
  fFixUpOffsetInSrc(fixUpOffset), fKind(kind), fLazy(false), fWeak(false)
{
	// assure no direct references to something that might be coalesced
	if ( (target.isWeakDefinition() || target.isCoalesableByName()) && (target.getScope() != ObjectFile::Atom::scopeTranslationUnit) && (target.getName() != NULL) ) {
		//fprintf(stderr, "change TO direct reference to by-name: from %s to %s in %p\n", fromTarget.getDisplayName(), target.getName(), this);
		fTargetName = target.getName();
		fTarget = NULL;
	}
// Note: We should also allow by-name from references, but many other chunks of code assume from targets are always direct//
//	if ( (fromTarget.isWeakDefinition() || fromTarget.isCoalesableByName()) && (fromTarget.getScope() != ObjectFile::Atom::scopeTranslationUnit) && (fromTarget.getName() != NULL)) {
//		fprintf(stderr, "change FROM direct reference to by-name: from %s to %s in %p\n", fromTarget.getDisplayName(), target.getName(), this);
//		fFromTargetName = fromTarget.getName();
//		fFromTarget = NULL;
//	}
}



Reference::~Reference()
{
}

bool Reference::isTargetUnbound() const
{
	return ( fTarget == NULL );
}

bool Reference::isFromTargetUnbound() const
{
	return ( fFromTarget == NULL );
}

bool Reference::isWeakReference() const
{
	return fWeak;
}

bool Reference::requiresRuntimeFixUp(bool slideable) const
{
	// This static linker only supports pure code (no code fixups are runtime)
#if defined(ARCH_PPC) || defined(ARCH_PPC64)
	// Only data can be fixed up, and the codegen assures only "pointers" need runtime fixups
	return ( (fKind == Reference::pointer) && (fTarget->isImportProxy() || fTarget->isWeakDefinition() || slideable) );
#elif defined(ARCH_I386)
	// For i386, Reference::pointer is used for both data pointers and instructions with 32-bit absolute operands
	if ( fKind == Reference::pointer ) {
		if ( fTarget->isImportProxy() )
			return true;
		else
			return slideable;
	}
	return false; 
#else
	#error
#endif
}

bool Reference::isLazyReference() const
{
	return fLazy;
}

ObjectFile::Reference::Kind	Reference::getKind() const
{
	return fKind;
}

uint64_t Reference::getFixUpOffset() const
{
	return fFixUpOffsetInSrc;
}

const char* Reference::getTargetName() const
{
	if ( fTargetName != NULL )
		return fTargetName;
	return fTarget->getName();
}

ObjectFile::Atom& Reference::getTarget() const
{
	return *fTarget;
}

void Reference::setTarget(ObjectFile::Atom& target, uint64_t offset)
{
	fTarget = &target;
	fTargetOffset = offset;
}


ObjectFile::Atom& Reference::getFromTarget() const
{
	return *fFromTarget;
}

bool Reference::hasFromTarget() const
{
	return ( (fFromTarget != NULL) || (fFromTargetName != NULL) );
}

const char* Reference::getFromTargetName() const
{
	if ( fFromTargetName != NULL )
		return fFromTargetName;
	return fFromTarget->getName();
}

void Reference::setFromTarget(ObjectFile::Atom& target)
{
	fFromTarget = &target;
}

void Reference::setFromTargetName(const char* name)
{
	fFromTargetName = name;
}

void Reference::setFromTargetOffset(uint64_t offset)
{
	fFromTargetOffset = offset;
}

uint64_t Reference::getTargetOffset() const
{
	return fTargetOffset;
}


uint64_t Reference::getFromTargetOffset() const
{
	return fFromTargetOffset;
}

void Reference::setLazy(bool lazy)
{
	fLazy = lazy;
}

void Reference::setWeak(bool weak)
{
	fWeak = weak;
}

const char* Reference::getDescription() const
{
	static char temp[256];
	if ( fKind == pointer32Difference ) {
		// by-name references have quoted names
		bool targetByName = ( &(this->getTarget()) == NULL );
		bool fromByName   = ( &(this->getFromTarget()) == NULL );
		const char* targetQuotes = targetByName ? "\"" : "";
		const char* fromQuotes = fromByName ? "\"" : "";
		sprintf(temp, "offset 0x%04llX, 32-bit pointer difference: (&%s%s%s + %lld) - (&%s%s%s + %lld)", 
			this->getFixUpOffset(), targetQuotes, this->getTargetName(), targetQuotes, this->getTargetOffset(), 
						   fromQuotes, this->getFromTargetName(), fromQuotes, this->getFromTargetOffset() );
	}
	else if ( fKind == pointer64Difference ) {
		// by-name references have quoted names
		bool targetByName = ( &(this->getTarget()) == NULL );
		bool fromByName   = ( &(this->getFromTarget()) == NULL );
		const char* targetQuotes = targetByName ? "\"" : "";
		const char* fromQuotes = fromByName ? "\"" : "";
		sprintf(temp, "offset 0x%04llX, 64-bit pointer difference: (&%s%s%s + %lld) - (&%s%s%s + %lld)", 
			this->getFixUpOffset(), targetQuotes, this->getTargetName(), targetQuotes, this->getTargetOffset(), 
						   fromQuotes, this->getFromTargetName(), fromQuotes, this->getFromTargetOffset() );
	}
	else {
		switch( fKind ) {
			case noFixUp:
				sprintf(temp, "reference to ");
				break;
			case pointer:
				{
				const char* weak = "";
				if ( fWeak )
					weak = "weak ";
				const char* lazy = "";
				if ( fLazy )
					lazy = "lazy ";
				sprintf(temp, "offset 0x%04llX, %s%spointer to ", this->getFixUpOffset(), weak, lazy);
				}
				break;
			case ppcFixupBranch14:
			case ppcFixupBranch24:
				sprintf(temp, "offset 0x%04llX, bl pc-rel fixup to ", this->getFixUpOffset());
				break;
			case ppcFixupPicBaseLow16:
				sprintf(temp, "offset 0x%04llX, low  16 fixup from pic-base offset 0x%04llX to ", this->getFixUpOffset(), this->getFromTargetOffset());
				break;
			case ppcFixupPicBaseLow14:
				sprintf(temp, "offset 0x%04llX, low  14 fixup from pic-base offset 0x%04llX to ", this->getFixUpOffset(), this->getFromTargetOffset());
				break;
			case ppcFixupPicBaseHigh16:
				sprintf(temp, "offset 0x%04llX, high 16 fixup from pic-base offset 0x%04llX to ", this->getFixUpOffset(), this->getFromTargetOffset());
				break;
			case ppcFixupAbsLow16:
				sprintf(temp, "offset 0x%04llX, low  16 fixup to absolute address of ", this->getFixUpOffset());
				break;
			case ppcFixupAbsLow14:
				sprintf(temp, "offset 0x%04llX, low  14 fixup to absolute address of ", this->getFixUpOffset());
				break;
			case ppcFixupAbsHigh16:
				sprintf(temp, "offset 0x%04llX, high 16 fixup to absolute address of ", this->getFixUpOffset());
				break;
			case ppcFixupAbsHigh16AddLow:
				sprintf(temp, "offset 0x%04llX, high 16 fixup to absolute address of ", this->getFixUpOffset());
				break;
			case pointer32Difference:
			case pointer64Difference:
				// handled above
				break;
			case x86FixupBranch32:
				sprintf(temp, "offset 0x%04llX, pc-rel fixup to ", this->getFixUpOffset());
				break;
		}
		// always quote by-name references
		if ( fTargetName != NULL ) {
			strcat(temp, "\"");
			strcat(temp, fTargetName);
			strcat(temp, "\"");
		}
		else if ( fTarget != NULL ) {
			strcat(temp, fTarget->getDisplayName());
		}
		else {
			strcat(temp, "NULL target");
		}
		if ( fTargetOffset != 0 )
			sprintf(&temp[strlen(temp)], " plus 0x%08llX", this->getTargetOffset());
		if ( (fKind==pointer) && fLazy ) {
			strcat(temp, " initially bound to \"");
			if ( (fFromTarget != NULL) || (fFromTargetName != NULL) ) {
				strcat(temp, this->getFromTargetName());
				strcat(temp, "\"");
				if ( this->getFromTargetOffset() != 0 ) 
					sprintf(&temp[strlen(temp)], " plus 0x%08llX", this->getFromTargetOffset());
			}
			else {
				strcat(temp, "\" << missing >>");
			}
		}
	}
	return temp;
}

};







