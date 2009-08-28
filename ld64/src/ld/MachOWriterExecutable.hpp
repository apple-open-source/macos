/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
 *
 * Copyright (c) 2005-2009 Apple Inc. All rights reserved.
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

#ifndef __EXECUTABLE_MACH_O__
#define __EXECUTABLE_MACH_O__

#include <stdint.h>
#include <stddef.h>
#include <fcntl.h>
#include <sys/time.h>
#include <uuid/uuid.h>
#include <mach/i386/thread_status.h>
#include <mach/ppc/thread_status.h>
#include <CommonCrypto/CommonDigest.h>

#include <vector>
#include <algorithm>
#include <map>
#include <set>
#include <ext/hash_map>

#include "ObjectFile.h"
#include "ExecutableFile.h"
#include "Options.h"

#include "MachOFileAbstraction.hpp"
#include "MachOTrie.hpp"


//
//
//	To implement architecture xxx, you must write template specializations for the following methods:
//			MachHeaderAtom<xxx>::setHeaderInfo()
//			ThreadsLoadCommandsAtom<xxx>::getSize()
//			ThreadsLoadCommandsAtom<xxx>::copyRawContent()
//			Writer<xxx>::addObjectRelocs()
//			Writer<xxx>::fixUpReferenceRelocatable()
//			Writer<xxx>::fixUpReferenceFinal()
//			Writer<xxx>::stubableReference()
//			Writer<xxx>::weakImportReferenceKind()
//			Writer<xxx>::GOTReferenceKind()
//


namespace mach_o {
namespace executable {

// forward references
template <typename A> class WriterAtom;
template <typename A> class PageZeroAtom;
template <typename A> class CustomStackAtom;
template <typename A> class MachHeaderAtom;
template <typename A> class SegmentLoadCommandsAtom;
template <typename A> class EncryptionLoadCommandsAtom;
template <typename A> class SymbolTableLoadCommandsAtom;
template <typename A> class DyldInfoLoadCommandsAtom;
template <typename A> class ThreadsLoadCommandsAtom;
template <typename A> class DylibIDLoadCommandsAtom;
template <typename A> class RoutinesLoadCommandsAtom;
template <typename A> class DyldLoadCommandsAtom;
template <typename A> class UUIDLoadCommandAtom;
template <typename A> class LinkEditAtom;
template <typename A> class SectionRelocationsLinkEditAtom;
template <typename A> class CompressedRebaseInfoLinkEditAtom;
template <typename A> class CompressedBindingInfoLinkEditAtom;
template <typename A> class CompressedWeakBindingInfoLinkEditAtom;
template <typename A> class CompressedLazyBindingInfoLinkEditAtom;
template <typename A> class CompressedExportInfoLinkEditAtom;
template <typename A> class LocalRelocationsLinkEditAtom;
template <typename A> class ExternalRelocationsLinkEditAtom;
template <typename A> class SymbolTableLinkEditAtom;
template <typename A> class SegmentSplitInfoLoadCommandsAtom;
template <typename A> class SegmentSplitInfoContentAtom;
template <typename A> class IndirectTableLinkEditAtom;
template <typename A> class ModuleInfoLinkEditAtom;
template <typename A> class StringsLinkEditAtom;
template <typename A> class LoadCommandsPaddingAtom;
template <typename A> class UnwindInfoAtom;
template <typename A> class StubAtom;
template <typename A> class StubHelperAtom;
template <typename A> class ClassicStubHelperAtom;
template <typename A> class HybridStubHelperAtom;
template <typename A> class HybridStubHelperHelperAtom;
template <typename A> class FastStubHelperAtom;
template <typename A> class FastStubHelperHelperAtom;
template <typename A> class LazyPointerAtom;
template <typename A> class NonLazyPointerAtom;
template <typename A> class DylibLoadCommandsAtom;


// SectionInfo should be nested inside Writer, but I can't figure out how to make the type accessible to the Atom classes
class SectionInfo : public ObjectFile::Section {
public:
										SectionInfo() : fFileOffset(0), fSize(0), fRelocCount(0), fRelocOffset(0), 
														fIndirectSymbolOffset(0), fAlignment(0), fAllLazyPointers(false), 
														fAllLazyDylibPointers(false),fAllNonLazyPointers(false), fAllStubs(false),
														fAllSelfModifyingStubs(false), fAllStubHelpers(false),
														fAllZeroFill(false), fVirtualSection(false),
														fHasTextLocalRelocs(false), fHasTextExternalRelocs(false)
														{ fSegmentName[0] = '\0'; fSectionName[0] = '\0'; }
	void								setIndex(unsigned int index) { fIndex=index; }
	std::vector<ObjectFile::Atom*>		fAtoms;
	char								fSegmentName[20];
	char								fSectionName[20];
	uint64_t							fFileOffset;
	uint64_t							fSize;
	uint32_t							fRelocCount;
	uint32_t							fRelocOffset;
	uint32_t							fIndirectSymbolOffset;
	uint8_t								fAlignment;
	bool								fAllLazyPointers;
	bool								fAllLazyDylibPointers;
	bool								fAllNonLazyPointers;
	bool								fAllStubs;
	bool								fAllSelfModifyingStubs;
	bool								fAllStubHelpers;
	bool								fAllZeroFill;
	bool								fVirtualSection;
	bool								fHasTextLocalRelocs;
	bool								fHasTextExternalRelocs;
};

// SegmentInfo should be nested inside Writer, but I can't figure out how to make the type accessible to the Atom classes
class SegmentInfo
{
public:
										SegmentInfo(uint64_t pageSize) : fInitProtection(0), fMaxProtection(0), fFileOffset(0), fFileSize(0),
														fBaseAddress(0), fSize(0), fPageSize(pageSize), fFixedAddress(false), 
														fIndependentAddress(false), fHasLoadCommand(true) { fName[0] = '\0'; }
	std::vector<class SectionInfo*>		fSections;
	char								fName[20];
	uint32_t							fInitProtection;
	uint32_t							fMaxProtection;
	uint64_t							fFileOffset;
	uint64_t							fFileSize;
	uint64_t							fBaseAddress;
	uint64_t							fSize;
	uint64_t							fPageSize;
	bool								fFixedAddress;
	bool								fIndependentAddress;
	bool								fHasLoadCommand;
};


struct RebaseInfo {
					RebaseInfo(uint8_t t, uint64_t addr) : fType(t), fAddress(addr) {}
	uint8_t			fType;
	uint64_t		fAddress;
	// for sorting
	int operator<(const RebaseInfo& rhs) const {
		// sort by type, then address
		if ( this->fType != rhs.fType )
			return  (this->fType < rhs.fType );
		return  (this->fAddress < rhs.fAddress );
	}
};

struct BindingInfo {
					BindingInfo(uint8_t t, int ord, const char* sym, bool weak_import, uint64_t addr, int64_t addend) 
						: fType(t), fFlags(weak_import ? BIND_SYMBOL_FLAGS_WEAK_IMPORT : 0 ), fLibraryOrdinal(ord), 
							fSymbolName(sym), fAddress(addr), fAddend(addend) {}
					BindingInfo(uint8_t t, const char* sym, bool non_weak_definition, uint64_t addr, int64_t addend) 
						: fType(t), fFlags(non_weak_definition ? BIND_SYMBOL_FLAGS_NON_WEAK_DEFINITION : 0 ), fLibraryOrdinal(0), 
							fSymbolName(sym), fAddress(addr), fAddend(addend) {}
	uint8_t			fType;
	uint8_t			fFlags;
	int				fLibraryOrdinal;
	const char*		fSymbolName;
	uint64_t		fAddress;
	int64_t			fAddend;
	
	// for sorting
	int operator<(const BindingInfo& rhs) const {
		// sort by library, symbol, type, then address
		if ( this->fLibraryOrdinal != rhs.fLibraryOrdinal )
			return  (this->fLibraryOrdinal < rhs.fLibraryOrdinal );
		if ( this->fSymbolName != rhs.fSymbolName )
			return ( strcmp(this->fSymbolName, rhs.fSymbolName) < 0 );
		if ( this->fType != rhs.fType )
			return  (this->fType < rhs.fType );
		return  (this->fAddress < rhs.fAddress );
	}
};


class ByteStream {
private:
	std::vector<uint8_t>		fData;
public:
	std::vector<uint8_t>& bytes() { return fData; }
	unsigned long size() const { return fData.size(); }
	void reserve(unsigned long l) { fData.reserve(l); }
	const uint8_t* start() const { return &fData[0]; }

	void append_uleb128(uint64_t value) {
		uint8_t byte;
		do {
			byte = value & 0x7F;
			value &= ~0x7F;
			if ( value != 0 )
				byte |= 0x80;
			fData.push_back(byte);
			value = value >> 7;
		} while( byte >= 0x80 );
	}
	
	void append_sleb128(int64_t value) {
		bool isNeg = ( value < 0 );
		uint8_t byte;
		bool more;
		do {
			byte = value & 0x7F;
			value = value >> 7;
			if ( isNeg ) 
				more = ( (value != -1) || ((byte & 0x40) == 0) );
			else
				more = ( (value != 0) || ((byte & 0x40) != 0) );
			if ( more )
				byte |= 0x80;
			fData.push_back(byte);
		} 
		while( more );
	}
	
	void append_string(const char* str) {
		for (const char* s = str; *s != '\0'; ++s)
			fData.push_back(*s);
		fData.push_back('\0');
	}
	
	void append_byte(uint8_t byte) {
		fData.push_back(byte);
	}
	
	static unsigned int	uleb128_size(uint64_t value) {
		uint32_t result = 0;
		do {
			value = value >> 7;
			++result;
		} while ( value != 0 );
		return result;
	}
	
	void pad_to_size(unsigned int alignment) {
		while ( (fData.size() % alignment) != 0 )
			fData.push_back(0);
	}
};


template <typename A>
class Writer : public ExecutableFile::Writer
{
public:
	Writer(const char* path, Options& options, std::vector<ExecutableFile::DyLibUsed>& dynamicLibraries);
	virtual						~Writer();

	virtual const char*								getPath()								{ return fFilePath; }
	virtual time_t									getModificationTime()					{ return 0; }
	virtual DebugInfoKind							getDebugInfoKind()						{ return ObjectFile::Reader::kDebugInfoNone; }
	virtual std::vector<class ObjectFile::Atom*>&	getAtoms()								{ return fWriterSynthesizedAtoms; }
	virtual std::vector<class ObjectFile::Atom*>*	getJustInTimeAtomsFor(const char* name)	{ return NULL; }
	virtual std::vector<Stab>*						getStabs()								{ return NULL; }

	virtual ObjectFile::Atom&						makeObjcInfoAtom(ObjectFile::Reader::ObjcConstraint objcContraint, 
																		bool objcReplacementClasses);
	virtual class ObjectFile::Atom*					getUndefinedProxyAtom(const char* name);
	virtual uint64_t								write(std::vector<class ObjectFile::Atom*>& atoms,
														  std::vector<class ObjectFile::Reader::Stab>& stabs,
														  class ObjectFile::Atom* entryPointAtom,
														  class ObjectFile::Atom* dyldClassicHelperAtom,
														  class ObjectFile::Atom* dyldCompressedHelperAtom,
														  class ObjectFile::Atom* dyldLazyDylibHelperAtom,
														  bool createUUID, bool canScatter,
														  ObjectFile::Reader::CpuConstraint cpuConstraint,
														  bool biggerThanTwoGigs,
														  std::set<const class ObjectFile::Atom*>& atomsThatOverrideWeak,
														  bool hasExternalWeakDefinitions);

private:
	typedef typename A::P			P;
	typedef typename A::P::uint_t	pint_t;

	enum RelocKind { kRelocNone, kRelocInternal, kRelocExternal };

	void						assignFileOffsets();
	void						synthesizeStubs();
	void						synthesizeKextGOT();
	void						createSplitSegContent();
	void						synthesizeUnwindInfoTable();
	void						insertDummyStubs();
	void						partitionIntoSections();
	bool						addBranchIslands();
	bool						addPPCBranchIslands();
	bool						isBranch24Reference(uint8_t kind);
	void						adjustLoadCommandsAndPadding();
	void						createDynamicLinkerCommand();
	void						createDylibCommands();
	void						buildLinkEdit();
	const char*					getArchString();
	void						writeMap();
	uint64_t					writeAtoms();
	void						writeNoOps(int fd, uint32_t from, uint32_t to);
	void						copyNoOps(uint8_t* from, uint8_t* to);
	bool						segmentsCanSplitApart(const ObjectFile::Atom& from, const ObjectFile::Atom& to);
	void						addCrossSegmentRef(const ObjectFile::Atom* atom, const ObjectFile::Reference* ref);
	void						collectExportedAndImportedAndLocalAtoms();
	void						setNlistRange(std::vector<class ObjectFile::Atom*>& atoms, uint32_t startIndex, uint32_t count);
	void						addLocalLabel(ObjectFile::Atom& atom, uint32_t offsetInAtom, const char* name);
	void						addGlobalLabel(ObjectFile::Atom& atom, uint32_t offsetInAtom, const char* name);
	void						buildSymbolTable();
	bool						stringsNeedLabelsInObjects();
	const char*					symbolTableName(const ObjectFile::Atom* atom);
	void						setExportNlist(const ObjectFile::Atom* atom, macho_nlist<P>* entry);
	void						setImportNlist(const ObjectFile::Atom* atom, macho_nlist<P>* entry);
	void						setLocalNlist(const ObjectFile::Atom* atom, macho_nlist<P>* entry);
	void						copyNlistRange(const std::vector<macho_nlist<P> >& entries, uint32_t startIndex);
	uint64_t					getAtomLoadAddress(const ObjectFile::Atom* atom);
	uint8_t						ordinalForLibrary(ObjectFile::Reader* file);
	bool						targetRequiresWeakBinding(const ObjectFile::Atom& target);
	int							compressedOrdinalForImortedAtom(ObjectFile::Atom* target);
	bool						shouldExport(const ObjectFile::Atom& atom) const;
	void						buildFixups();
	void						adjustLinkEditSections();
	void						buildObjectFileFixups();
	void						buildExecutableFixups();
	bool						preboundLazyPointerType(uint8_t* type);
	uint64_t					relocAddressInFinalLinkedImage(uint64_t address, const ObjectFile::Atom* atom) const;
	void						fixUpReferenceFinal(const ObjectFile::Reference* ref, const ObjectFile::Atom* inAtom, uint8_t buffer[]) const;
	void						fixUpReferenceRelocatable(const ObjectFile::Reference* ref, const ObjectFile::Atom* inAtom, uint8_t buffer[]) const;
	void						fixUpReference_powerpc(const ObjectFile::Reference* ref, const ObjectFile::Atom* inAtom,
														uint8_t buffer[], bool finalLinkedImage) const;
	uint32_t					symbolIndex(ObjectFile::Atom& atom);
	bool						makesExternalRelocatableReference(ObjectFile::Atom& target) const;
	uint32_t					addObjectRelocs(ObjectFile::Atom* atom, ObjectFile::Reference* ref);
	uint32_t					addObjectRelocs_powerpc(ObjectFile::Atom* atom, ObjectFile::Reference* ref);
	uint8_t						getRelocPointerSize();
	uint64_t					maxAddress();
	bool						stubableReference(const ObjectFile::Atom* inAtom, const ObjectFile::Reference* ref);
	bool						GOTReferenceKind(uint8_t kind);
	bool						optimizableGOTReferenceKind(uint8_t kind);
	bool						weakImportReferenceKind(uint8_t kind);
	unsigned int				collectStabs();
	uint64_t					valueForStab(const ObjectFile::Reader::Stab& stab);
	uint32_t					stringOffsetForStab(const ObjectFile::Reader::Stab& stab);
	uint8_t						sectionIndexForStab(const ObjectFile::Reader::Stab& stab);
	void						addStabs(uint32_t startIndex);
	RelocKind					relocationNeededInFinalLinkedImage(const ObjectFile::Atom& target) const;
	bool						illegalRelocInFinalLinkedImage(const ObjectFile::Reference&);
	bool						generatesLocalTextReloc(const ObjectFile::Reference&, const ObjectFile::Atom& atom, SectionInfo* curSection);
	bool						generatesExternalTextReloc(const ObjectFile::Reference&, const ObjectFile::Atom& atom, SectionInfo* curSection);
	bool						mightNeedPadSegment();
	void						scanForAbsoluteReferences();
	bool						needsModuleTable();
	void						optimizeDylibReferences();
	bool						indirectSymbolInRelocatableIsLocal(const ObjectFile::Reference* ref) const;

	struct DirectLibrary {
		class ObjectFile::Reader*	fLibrary;
		bool						fWeak;
		bool						fReExport;
	};

	friend class WriterAtom<A>;
	friend class PageZeroAtom<A>;
	friend class CustomStackAtom<A>;
	friend class MachHeaderAtom<A>;
	friend class SegmentLoadCommandsAtom<A>;
	friend class EncryptionLoadCommandsAtom<A>;
	friend class SymbolTableLoadCommandsAtom<A>;
	friend class DyldInfoLoadCommandsAtom<A>;
	friend class ThreadsLoadCommandsAtom<A>;
	friend class DylibIDLoadCommandsAtom<A>;
	friend class RoutinesLoadCommandsAtom<A>;
	friend class DyldLoadCommandsAtom<A>;
	friend class UUIDLoadCommandAtom<A>;
	friend class LinkEditAtom<A>;
	friend class SectionRelocationsLinkEditAtom<A>;
	friend class CompressedRebaseInfoLinkEditAtom<A>;
	friend class CompressedBindingInfoLinkEditAtom<A>;
	friend class CompressedWeakBindingInfoLinkEditAtom<A>;
	friend class CompressedLazyBindingInfoLinkEditAtom<A>;
	friend class CompressedExportInfoLinkEditAtom<A>;
	friend class LocalRelocationsLinkEditAtom<A>;
	friend class ExternalRelocationsLinkEditAtom<A>;
	friend class SymbolTableLinkEditAtom<A>;
	friend class SegmentSplitInfoLoadCommandsAtom<A>;
	friend class SegmentSplitInfoContentAtom<A>;
	friend class IndirectTableLinkEditAtom<A>;
	friend class ModuleInfoLinkEditAtom<A>;
	friend class StringsLinkEditAtom<A>;
	friend class LoadCommandsPaddingAtom<A>;
	friend class UnwindInfoAtom<A>;
	friend class StubAtom<A>;
	friend class StubHelperAtom<A>;
	friend class ClassicStubHelperAtom<A>;
	friend class HybridStubHelperAtom<A>;
	friend class FastStubHelperAtom<A>;
	friend class FastStubHelperHelperAtom<A>;
	friend class HybridStubHelperHelperAtom<A>;
	friend class LazyPointerAtom<A>;
	friend class NonLazyPointerAtom<A>;
	friend class DylibLoadCommandsAtom<A>;

	const char*										fFilePath;
	Options&										fOptions;
	std::vector<class ObjectFile::Atom*>*			fAllAtoms;
	std::vector<class ObjectFile::Reader::Stab>*	fStabs;
	std::set<const class ObjectFile::Atom*>*		fRegularDefAtomsThatOverrideADylibsWeakDef;
	class SectionInfo*								fLoadCommandsSection;
	class SegmentInfo*								fLoadCommandsSegment;
	class MachHeaderAtom<A>*						fMachHeaderAtom;
	class EncryptionLoadCommandsAtom<A>*			fEncryptionLoadCommand;
	class SegmentLoadCommandsAtom<A>*				fSegmentCommands;
	class SymbolTableLoadCommandsAtom<A>*			fSymbolTableCommands;
	class LoadCommandsPaddingAtom<A>*				fHeaderPadding;
	class UnwindInfoAtom<A>*						fUnwindInfoAtom;
	class UUIDLoadCommandAtom<A>*				    fUUIDAtom;
	std::vector<class ObjectFile::Atom*>			fWriterSynthesizedAtoms;
	std::vector<SegmentInfo*>						fSegmentInfos;
	class SegmentInfo*								fPadSegmentInfo;
	class ObjectFile::Atom*							fEntryPoint;
	class ObjectFile::Atom*							fDyldClassicHelperAtom;
	class ObjectFile::Atom*							fDyldCompressedHelperAtom;
	class ObjectFile::Atom*							fDyldLazyDylibHelper;
	std::map<class ObjectFile::Reader*, DylibLoadCommandsAtom<A>*>	fLibraryToLoadCommand;
	std::map<class ObjectFile::Reader*, uint32_t>	fLibraryToOrdinal;
	std::map<class ObjectFile::Reader*, class ObjectFile::Reader*>	fLibraryAliases;
	std::set<class ObjectFile::Reader*>				fForcedWeakImportReaders;
	std::vector<class ObjectFile::Atom*>			fExportedAtoms;
	std::vector<class ObjectFile::Atom*>			fImportedAtoms;
	std::vector<class ObjectFile::Atom*>			fLocalSymbolAtoms;
	std::vector<macho_nlist<P> >					fLocalExtraLabels;
	std::vector<macho_nlist<P> >					fGlobalExtraLabels;
	std::map<ObjectFile::Atom*, uint32_t>			fAtomToSymbolIndex;
	class SectionRelocationsLinkEditAtom<A>*		fSectionRelocationsAtom;	
	class CompressedRebaseInfoLinkEditAtom<A>*		fCompressedRebaseInfoAtom;
	class CompressedBindingInfoLinkEditAtom<A>*		fCompressedBindingInfoAtom;
	class CompressedWeakBindingInfoLinkEditAtom<A>*	fCompressedWeakBindingInfoAtom;
	class CompressedLazyBindingInfoLinkEditAtom<A>*	fCompressedLazyBindingInfoAtom;
	class CompressedExportInfoLinkEditAtom<A>*		fCompressedExportInfoAtom;
	class LocalRelocationsLinkEditAtom<A>*			fLocalRelocationsAtom;
	class ExternalRelocationsLinkEditAtom<A>*		fExternalRelocationsAtom;
	class SymbolTableLinkEditAtom<A>*				fSymbolTableAtom;
	class SegmentSplitInfoContentAtom<A>*			fSplitCodeToDataContentAtom;
	class IndirectTableLinkEditAtom<A>*				fIndirectTableAtom;
	class ModuleInfoLinkEditAtom<A>*				fModuleInfoAtom;
	class StringsLinkEditAtom<A>*					fStringsAtom;
	class PageZeroAtom<A>*							fPageZeroAtom;
	class NonLazyPointerAtom<A>*					fFastStubGOTAtom;
	macho_nlist<P>*									fSymbolTable;
	std::vector<macho_relocation_info<P> >			fSectionRelocs;
	std::vector<macho_relocation_info<P> >			fInternalRelocs;
	std::vector<macho_relocation_info<P> >			fExternalRelocs;
	std::vector<RebaseInfo>							fRebaseInfo;
	std::vector<BindingInfo>						fBindingInfo;
	std::vector<BindingInfo>						fWeakBindingInfo;
	std::map<const ObjectFile::Atom*,ObjectFile::Atom*>	fStubsMap;
	std::map<ObjectFile::Atom*,ObjectFile::Atom*>	fGOTMap;
	std::vector<class StubAtom<A>*>					fAllSynthesizedStubs;
	std::vector<ObjectFile::Atom*>					fAllSynthesizedStubHelpers;
	std::vector<class LazyPointerAtom<A>*>			fAllSynthesizedLazyPointers;
	std::vector<class LazyPointerAtom<A>*>			fAllSynthesizedLazyDylibPointers;
	std::vector<class NonLazyPointerAtom<A>*>		fAllSynthesizedNonLazyPointers;
	uint32_t										fSymbolTableCount;
	uint32_t										fSymbolTableStabsCount;
	uint32_t										fSymbolTableStabsStartIndex;
	uint32_t										fSymbolTableLocalCount;
	uint32_t										fSymbolTableLocalStartIndex;
	uint32_t										fSymbolTableExportCount;
	uint32_t										fSymbolTableExportStartIndex;
	uint32_t										fSymbolTableImportCount;
	uint32_t										fSymbolTableImportStartIndex;
	uint32_t										fLargestAtomSize;
	bool											fEmitVirtualSections;
	bool											fHasWeakExports;
	bool											fReferencesWeakImports;
	bool											fCanScatter;
	bool											fWritableSegmentPastFirst4GB;
	bool											fNoReExportedDylibs;
	bool											fBiggerThanTwoGigs;
	bool											fSlideable;
	std::map<const ObjectFile::Atom*,bool>			fWeakImportMap;
	std::set<const ObjectFile::Reader*>				fDylibReadersWithNonWeakImports;
	std::set<const ObjectFile::Reader*>				fDylibReadersWithWeakImports;
	SegmentInfo*									fFirstWritableSegment;
	ObjectFile::Reader::CpuConstraint				fCpuConstraint;
	uint32_t										fAnonNameIndex;
};


class Segment : public ObjectFile::Segment
{
public:
								Segment(const char* name, bool readable, bool writable, bool executable, bool fixedAddress)
											 : fName(name), fReadable(readable), fWritable(writable), fExecutable(executable), fFixedAddress(fixedAddress) {}
	virtual const char*			getName() const					{ return fName; }
	virtual bool				isContentReadable() const		{ return fReadable; }
	virtual bool				isContentWritable() const		{ return fWritable; }
	virtual bool				isContentExecutable() const		{ return fExecutable; }
	virtual bool				hasFixedAddress() const			{ return fFixedAddress; }
	
	static Segment								fgTextSegment;
	static Segment								fgPageZeroSegment;
	static Segment								fgLinkEditSegment;
	static Segment								fgStackSegment;
	static Segment								fgImportSegment;
	static Segment								fgROImportSegment;
	static Segment								fgDataSegment;
	static Segment								fgObjCSegment;
	static Segment								fgHeaderSegment;
	
	
private:
	const char*					fName;
	const bool					fReadable;
	const bool					fWritable;
	const bool					fExecutable;
	const bool					fFixedAddress;
};

Segment		Segment::fgPageZeroSegment("__PAGEZERO", false, false, false, true);
Segment		Segment::fgTextSegment("__TEXT", true, false, true, false);
Segment		Segment::fgLinkEditSegment("__LINKEDIT", true, false, false, false);
Segment		Segment::fgStackSegment("__UNIXSTACK", true, true, false, true);
Segment		Segment::fgImportSegment("__IMPORT", true, true, true, false);
Segment		Segment::fgROImportSegment("__IMPORT", true, false, true, false);
Segment		Segment::fgDataSegment("__DATA", true, true, false, false);
Segment		Segment::fgObjCSegment("__OBJC", true, true, false, false);
Segment		Segment::fgHeaderSegment("__HEADER", true, false, true, false);


template <typename A>
class WriterAtom : public ObjectFile::Atom
{
public:
	enum Kind { zeropage, machHeaderApp, machHeaderDylib, machHeaderBundle, machHeaderObject, loadCommands, undefinedProxy };
											WriterAtom(Writer<A>& writer, Segment& segment) : fWriter(writer), fSegment(segment) { }

	virtual ObjectFile::Reader*				getFile() const					{ return &fWriter; }
	virtual bool							getTranslationUnitSource(const char** dir, const char** name) const { return false; }
	virtual const char*						getName() const					{ return NULL; }
	virtual const char*						getDisplayName() const			{ return this->getName(); }
	virtual Scope							getScope() const				{ return ObjectFile::Atom::scopeTranslationUnit; }
	virtual DefinitionKind					getDefinitionKind() const		{ return kRegularDefinition; }
	virtual SymbolTableInclusion			getSymbolTableInclusion() const	{ return ObjectFile::Atom::kSymbolTableNotIn; }
	virtual	bool							dontDeadStrip() const			{ return true; }
	virtual bool							isZeroFill() const				{ return false; }
	virtual bool							isThumb() const					{ return false; }
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const		{ return fgEmptyReferenceList; }
	virtual bool							mustRemainInSection() const		{ return true; }
	virtual ObjectFile::Segment&			getSegment() const				{ return fSegment; }
	virtual ObjectFile::Atom&				getFollowOnAtom() const			{ return *((ObjectFile::Atom*)NULL); }
	virtual uint32_t						getOrdinal() const				{ return 0; }
	virtual std::vector<ObjectFile::LineInfo>*	getLineInfo() const			{ return NULL; }
	virtual ObjectFile::Alignment			getAlignment() const			{ return ObjectFile::Alignment(2); }
	virtual void							copyRawContent(uint8_t buffer[]) const { throw "don't use copyRawContent"; }
	virtual void							setScope(Scope)					{ }


protected:
	virtual									~WriterAtom() {}
	typedef typename A::P					P;
	typedef typename A::P::E				E;

	static Segment&			headerSegment(Writer<A>& writer) { return (writer.fOptions.outputKind()==Options::kPreload)
																	? Segment::fgHeaderSegment : Segment::fgTextSegment; }

	static std::vector<ObjectFile::Reference*>	fgEmptyReferenceList;

	Writer<A>&									fWriter;
	Segment&									fSegment;
};

template <typename A> std::vector<ObjectFile::Reference*>	WriterAtom<A>::fgEmptyReferenceList;


template <typename A>
class PageZeroAtom : public WriterAtom<A>
{
public:
											PageZeroAtom(Writer<A>& writer) : WriterAtom<A>(writer, Segment::fgPageZeroSegment),
																			fSize(fWriter.fOptions.zeroPageSize()) {}
	virtual const char*						getDisplayName() const	{ return "page zero content"; }
	virtual bool							isZeroFill() const		{ return true; }
	virtual uint64_t						getSize() const 		{ return fSize; }
	virtual const char*						getSectionName() const	{ return "._zeropage"; }
	virtual ObjectFile::Alignment			getAlignment() const	{ return ObjectFile::Alignment(12); }
	void									setSize(uint64_t size)	{ fSize = size; }
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
	uint64_t								fSize;
};


template <typename A>
class DsoHandleAtom : public WriterAtom<A>
{
public:
													DsoHandleAtom(Writer<A>& writer) : WriterAtom<A>(writer, Segment::fgTextSegment) {}
	virtual const char*								getName() const				{ return "___dso_handle"; }
	virtual ObjectFile::Atom::Scope					getScope() const			{ return ObjectFile::Atom::scopeLinkageUnit; }
	virtual ObjectFile::Atom::SymbolTableInclusion	getSymbolTableInclusion() const { return ObjectFile::Atom::kSymbolTableNotIn; }
	virtual uint64_t								getSize() const				{ return 0; }
	virtual ObjectFile::Alignment					getAlignment() const		{ return ObjectFile::Alignment(12); }
	virtual const char*								getSectionName() const		{ return "._mach_header"; }
	virtual void									copyRawContent(uint8_t buffer[]) const {}
};


template <typename A>
class MachHeaderAtom : public WriterAtom<A>
{
public:
													MachHeaderAtom(Writer<A>& writer) : WriterAtom<A>(writer, headerSegment(writer)) {}
	virtual const char*								getName() const;
	virtual const char*								getDisplayName() const;
	virtual ObjectFile::Atom::Scope					getScope() const;
	virtual ObjectFile::Atom::SymbolTableInclusion	getSymbolTableInclusion() const;
	virtual uint64_t								getSize() const				{ return sizeof(macho_header<typename A::P>); }
	virtual ObjectFile::Alignment					getAlignment() const		{ return ObjectFile::Alignment(12); }
	virtual const char*								getSectionName() const		{ return "._mach_header"; }
	virtual uint32_t								getOrdinal() const			{ return 1; }
	virtual void									copyRawContent(uint8_t buffer[]) const;
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
	void									setHeaderInfo(macho_header<typename A::P>& header) const;
};

template <typename A>
class CustomStackAtom : public WriterAtom<A>
{
public:
											CustomStackAtom(Writer<A>& writer);
	virtual const char*						getDisplayName() const	{ return "custom stack content"; }
	virtual bool							isZeroFill() const		{ return true; }
	virtual uint64_t						getSize() const 		{ return fWriter.fOptions.customStackSize(); }
	virtual const char*						getSectionName() const	{ return "._stack"; }
	virtual ObjectFile::Alignment			getAlignment() const	{ return ObjectFile::Alignment(12); }
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
	static bool								stackGrowsDown();
};

template <typename A>
class LoadCommandAtom : public WriterAtom<A>
{
protected:
											LoadCommandAtom(Writer<A>& writer) : WriterAtom<A>(writer, headerSegment(writer)), fOrdinal(fgCurrentOrdinal++) {}
	virtual ObjectFile::Alignment			getAlignment() const	{ return ObjectFile::Alignment(log2(sizeof(typename A::P::uint_t))); }
	virtual const char*						getSectionName() const	{ return "._load_commands"; }
	virtual uint32_t						getOrdinal() const		{ return fOrdinal; }
	static uint64_t							alignedSize(uint64_t size);
protected:
	uint32_t								fOrdinal;
	static uint32_t							fgCurrentOrdinal;
};

template <typename A> uint32_t LoadCommandAtom<A>::fgCurrentOrdinal = 0;

template <typename A>
class SegmentLoadCommandsAtom : public LoadCommandAtom<A>
{
public:
											SegmentLoadCommandsAtom(Writer<A>& writer)  
												: LoadCommandAtom<A>(writer), fCommandCount(0), fSize(0) 
												{ writer.fSegmentCommands = this; }
	virtual const char*						getDisplayName() const	{ return "segment load commands"; }
	virtual uint64_t						getSize() const			{ return fSize; }
	virtual void							copyRawContent(uint8_t buffer[]) const;

	void									computeSize();
	void									setup();
	unsigned int							commandCount()			{ return fCommandCount; }
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
	unsigned int							fCommandCount;
	uint32_t								fSize;
};


template <typename A>
class SymbolTableLoadCommandsAtom : public LoadCommandAtom<A>
{
public:
											SymbolTableLoadCommandsAtom(Writer<A>&);
	virtual const char*						getDisplayName() const { return "symbol table load commands"; }
	virtual uint64_t						getSize() const;
	virtual void							copyRawContent(uint8_t buffer[]) const;
	unsigned int							commandCount();
			void							needDynamicTable();
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
	bool									fNeedsDynamicSymbolTable;
	macho_symtab_command<typename A::P>		fSymbolTable;
	macho_dysymtab_command<typename A::P>	fDynamicSymbolTable;
};

template <typename A>
class ThreadsLoadCommandsAtom : public LoadCommandAtom<A>
{
public:
											ThreadsLoadCommandsAtom(Writer<A>& writer) 
												: LoadCommandAtom<A>(writer) {}
	virtual const char*						getDisplayName() const { return "thread load commands"; }
	virtual uint64_t						getSize() const;
	virtual void							copyRawContent(uint8_t buffer[]) const;
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
	uint8_t*								fBuffer;
	uint32_t								fBufferSize;
};

template <typename A>
class DyldLoadCommandsAtom : public LoadCommandAtom<A>
{
public:
											DyldLoadCommandsAtom(Writer<A>& writer)  : LoadCommandAtom<A>(writer) {}
	virtual const char*						getDisplayName() const	{ return "dyld load command"; }
	virtual uint64_t						getSize() const;
	virtual void							copyRawContent(uint8_t buffer[]) const;
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
};

template <typename A>
class SegmentSplitInfoLoadCommandsAtom : public LoadCommandAtom<A>
{
public:
											SegmentSplitInfoLoadCommandsAtom(Writer<A>& writer)  : LoadCommandAtom<A>(writer) {}
	virtual const char*						getDisplayName() const	{ return "segment split info load command"; }
	virtual uint64_t						getSize() const;
	virtual void							copyRawContent(uint8_t buffer[]) const;
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
};

template <typename A>
class AllowableClientLoadCommandsAtom : public LoadCommandAtom<A>
{
public:
	AllowableClientLoadCommandsAtom(Writer<A>& writer, const char* client)  :
		LoadCommandAtom<A>(writer), clientString(client) {}
	virtual const char*							getDisplayName() const  { return "allowable_client load command"; }
	virtual uint64_t							getSize() const;
	virtual void								copyRawContent(uint8_t buffer[]) const;
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P						P;
	const char*							   		clientString;
};

template <typename A>
class DylibLoadCommandsAtom : public LoadCommandAtom<A>
{
public:
											DylibLoadCommandsAtom(Writer<A>& writer, ExecutableFile::DyLibUsed& info) 
											 : LoadCommandAtom<A>(writer), fInfo(info), 
												fOptimizedAway(false) { if (fInfo.options.fLazyLoad) this->fOrdinal += 256; }
	virtual const char*						getDisplayName() const	{ return "dylib load command"; }
	virtual uint64_t						getSize() const;
	virtual void							copyRawContent(uint8_t buffer[]) const;
	virtual void							optimizeAway() { fOptimizedAway = true; }
			bool							linkedWeak() { return fInfo.options.fWeakImport; }
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
	ExecutableFile::DyLibUsed				fInfo;
	bool									fOptimizedAway;
};

template <typename A>
class DylibIDLoadCommandsAtom : public LoadCommandAtom<A>
{
public:
											DylibIDLoadCommandsAtom(Writer<A>& writer) : LoadCommandAtom<A>(writer) {}
	virtual const char*						getDisplayName() const { return "dylib ID load command"; }
	virtual uint64_t						getSize() const;
	virtual void							copyRawContent(uint8_t buffer[]) const;
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
};

template <typename A>
class RoutinesLoadCommandsAtom : public LoadCommandAtom<A>
{
public:
											RoutinesLoadCommandsAtom(Writer<A>& writer) : LoadCommandAtom<A>(writer) {}
	virtual const char*						getDisplayName() const { return "routines load command"; }
	virtual uint64_t						getSize() const			{ return sizeof(macho_routines_command<typename A::P>); }
	virtual void							copyRawContent(uint8_t buffer[]) const;
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
};

template <typename A>
class SubUmbrellaLoadCommandsAtom : public LoadCommandAtom<A>
{
public:
											SubUmbrellaLoadCommandsAtom(Writer<A>& writer, const char* name) 
											 : LoadCommandAtom<A>(writer), fName(name) {}
	virtual const char*						getDisplayName() const	{ return "sub-umbrella load command"; }
	virtual uint64_t						getSize() const;
	virtual void							copyRawContent(uint8_t buffer[]) const;
private:
	typedef typename A::P					P;
	const char*								fName;
};

template <typename A>
class SubLibraryLoadCommandsAtom : public LoadCommandAtom<A>
{
public:
											SubLibraryLoadCommandsAtom(Writer<A>& writer,  const char* nameStart, int nameLen)
												: LoadCommandAtom<A>(writer), fNameStart(nameStart), fNameLength(nameLen) {}
	virtual const char*						getDisplayName() const	{ return "sub-library load command"; }
	virtual uint64_t						getSize() const;
	virtual void							copyRawContent(uint8_t buffer[]) const;
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
	const char*								fNameStart;
	int										fNameLength;
};

template <typename A>
class UmbrellaLoadCommandsAtom : public LoadCommandAtom<A>
{
public:
											UmbrellaLoadCommandsAtom(Writer<A>& writer, const char* name)
													: LoadCommandAtom<A>(writer), fName(name) {}
	virtual const char*						getDisplayName() const	{ return "umbrella load command"; }
	virtual uint64_t						getSize() const;
	virtual void							copyRawContent(uint8_t buffer[]) const;
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
	const char*								fName;
};

template <typename A>
class UUIDLoadCommandAtom : public LoadCommandAtom<A>
{
public:
											UUIDLoadCommandAtom(Writer<A>& writer)
												: LoadCommandAtom<A>(writer), fEmit(false) {}
	virtual const char*						getDisplayName() const	{ return "uuid load command"; }
	virtual uint64_t						getSize() const			{ return fEmit ? sizeof(macho_uuid_command<typename A::P>) : 0; }
	virtual void							copyRawContent(uint8_t buffer[]) const;
	virtual void						    generate();
			void						    setContent(const uint8_t uuid[16]);
			const uint8_t*					getUUID()				{ return fUUID; }
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
	uuid_t									fUUID;
	bool								    fEmit;
};


template <typename A>
class RPathLoadCommandsAtom : public LoadCommandAtom<A>
{
public:
											RPathLoadCommandsAtom(Writer<A>& writer, const char* path)
												: LoadCommandAtom<A>(writer), fPath(path) {}
	virtual const char*						getDisplayName() const	{ return "rpath load command"; }
	virtual uint64_t						getSize() const;
	virtual void							copyRawContent(uint8_t buffer[]) const;
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
	const char*								fPath;
};

template <typename A>
class EncryptionLoadCommandsAtom : public LoadCommandAtom<A>
{
public:
											EncryptionLoadCommandsAtom(Writer<A>& writer)
												: LoadCommandAtom<A>(writer), fStartOffset(0),
												  fEndOffset(0) {}
	virtual const char*						getDisplayName() const	{ return "encryption info load command"; }
	virtual uint64_t						getSize() const { return sizeof(macho_encryption_info_command<typename A::P>); }
	virtual void							copyRawContent(uint8_t buffer[]) const;
	void									setStartEncryptionOffset(uint32_t off) { fStartOffset = off; }
	void									setEndEncryptionOffset(uint32_t off) { fEndOffset = off; }
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
	uint32_t								fStartOffset;
	uint32_t								fEndOffset;
};

template <typename A>
class DyldInfoLoadCommandsAtom : public LoadCommandAtom<A>
{
public:
											DyldInfoLoadCommandsAtom(Writer<A>& writer)
												: LoadCommandAtom<A>(writer) {}
	virtual const char*						getDisplayName() const	{ return "dyld info load command"; }
	virtual uint64_t						getSize() const { return sizeof(macho_dyld_info_command<typename A::P>); }
	virtual void							copyRawContent(uint8_t buffer[]) const;
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
};


template <typename A>
class LoadCommandsPaddingAtom : public WriterAtom<A>
{
public:
											LoadCommandsPaddingAtom(Writer<A>& writer)
													: WriterAtom<A>(writer, headerSegment(writer)), fSize(0) {}
	virtual const char*						getDisplayName() const	{ return "header padding"; }
	virtual uint64_t						getSize() const			{ return fSize; }
	virtual const char*						getSectionName() const	{ return "._load_cmds_pad"; }
	virtual void							copyRawContent(uint8_t buffer[]) const;

	void									setSize(uint64_t newSize);
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
	uint64_t								fSize;
};

template <typename A>
class UnwindInfoAtom : public WriterAtom<A>
{
public:
													UnwindInfoAtom(Writer<A>& writer) : WriterAtom<A>(writer, Segment::fgTextSegment), 
																						fHeaderSize(0), fPagesSize(0), fAlignment(4) {}
	virtual const char*								getName() const				{ return "unwind info"; }
	virtual ObjectFile::Atom::Scope					getScope() const			{ return ObjectFile::Atom::scopeTranslationUnit; }
	virtual ObjectFile::Atom::SymbolTableInclusion	getSymbolTableInclusion() const { return ObjectFile::Atom::kSymbolTableNotIn; }
	virtual uint64_t								getSize() const				{ return fHeaderSize+fPagesSize; }
	virtual ObjectFile::Alignment					getAlignment() const		{ return fAlignment; }
	virtual const char*								getSectionName() const		{ return "__unwind_info"; }
	virtual uint32_t								getOrdinal() const			{ return 1; }
	virtual std::vector<ObjectFile::Reference*>&	getReferences() const		{ return (std::vector<ObjectFile::Reference*>&)fReferences; }
	virtual void									copyRawContent(uint8_t buffer[]) const;

	void											addUnwindInfo(ObjectFile::Atom* func, uint32_t offset, uint32_t encoding, 
															ObjectFile::Reference* fdeRef, ObjectFile::Reference* lsda,
															 ObjectFile::Atom* personalityPointer);
	void											generate();
	
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
	struct Info { ObjectFile::Atom* func; ObjectFile::Atom* fde; ObjectFile::Atom* lsda; uint32_t lsdaOffset; ObjectFile::Atom* personalityPointer; uint32_t encoding; };
	struct LSDAEntry { ObjectFile::Atom* func; ObjectFile::Atom* lsda; uint32_t lsdaOffset; };
	struct RegFixUp { uint8_t* contentPointer; ObjectFile::Atom* func; ObjectFile::Atom* fde; };
	struct CompressedFixUp { uint8_t* contentPointer; ObjectFile::Atom* func; ObjectFile::Atom* fromFunc; };
	struct CompressedEncodingFixUp { uint8_t* contentPointer; ObjectFile::Atom* fde; };

	bool				encodingMeansUseDwarf(compact_unwind_encoding_t encoding);
	void				compressDuplicates(std::vector<Info>& uniqueInfos);
	void				findCommonEncoding(const std::vector<Info>& uniqueInfos, std::map<uint32_t, unsigned int>& commonEncodings);
	void				makeLsdaIndex(const std::vector<Info>& uniqueInfos, std::map<ObjectFile::Atom*, uint32_t>& lsdaIndexOffsetMap);
	unsigned int		makeRegularSecondLevelPage(const std::vector<Info>& uniqueInfos, uint32_t pageSize, unsigned int endIndex, 
													uint8_t*& pageEnd);
	unsigned int		makeCompressedSecondLevelPage(const std::vector<Info>& uniqueInfos,   
														const std::map<uint32_t,unsigned int> commonEncodings,  
														uint32_t pageSize, unsigned int endIndex, uint8_t*& pageEnd);
	void				makePersonalityIndex(std::vector<Info>& uniqueInfos);
	

	uint32_t								fHeaderSize;
	uint32_t								fPagesSize;
	uint8_t*								fHeaderContent;
	uint8_t*								fPagesContent;
	uint8_t*								fPagesContentForDelete;
	ObjectFile::Alignment					fAlignment;
	std::vector<Info>						fInfos;
	std::map<ObjectFile::Atom*, uint32_t>	fPersonalityIndexMap;
	std::vector<LSDAEntry>					fLSDAIndex;
	std::vector<RegFixUp>					fRegFixUps;
	std::vector<CompressedFixUp>			fCompressedFixUps;
	std::vector<CompressedEncodingFixUp>	fCompressedEncodingFixUps;
	std::vector<ObjectFile::Reference*>		fReferences;
};



template <typename A>
class LinkEditAtom : public WriterAtom<A>
{
public:
											LinkEditAtom(Writer<A>& writer) : WriterAtom<A>(writer, Segment::fgLinkEditSegment), fOrdinal(fgCurrentOrdinal++) {}
	uint64_t								getFileOffset() const;
	virtual ObjectFile::Alignment			getAlignment() const	{ return ObjectFile::Alignment(log2(sizeof(typename A::P::uint_t))); }
	virtual uint32_t						getOrdinal() const		{ return fOrdinal; }
private:
	uint32_t								fOrdinal;
	static uint32_t							fgCurrentOrdinal;
private:
	typedef typename A::P					P;
};

template <typename A> uint32_t LinkEditAtom<A>::fgCurrentOrdinal = 0;

template <typename A>
class SectionRelocationsLinkEditAtom : public LinkEditAtom<A>
{
public:
											SectionRelocationsLinkEditAtom(Writer<A>& writer) : LinkEditAtom<A>(writer) { }
	virtual const char*						getDisplayName() const	{ return "section relocations"; }
	virtual uint64_t						getSize() const;
	virtual const char*						getSectionName() const	{ return "._section_relocs"; }
	virtual void							copyRawContent(uint8_t buffer[]) const;
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
};

template <typename A>
class CompressedInfoLinkEditAtom : public LinkEditAtom<A>
{
public:
											CompressedInfoLinkEditAtom(Writer<A>& writer) : LinkEditAtom<A>(writer) { }
	virtual uint64_t						getSize() const { return fEncodedData.size(); }
	virtual void							copyRawContent(uint8_t buffer[]) const { memcpy(buffer, fEncodedData.start(), fEncodedData.size()); }
protected:
	typedef typename A::P::uint_t			pint_t;
	ByteStream								fEncodedData;
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
};



template <typename A>
class CompressedRebaseInfoLinkEditAtom : public CompressedInfoLinkEditAtom<A>
{
public:
											CompressedRebaseInfoLinkEditAtom(Writer<A>& writer) : CompressedInfoLinkEditAtom<A>(writer) { }
	virtual const char*						getDisplayName() const	{ return "compressed rebase info"; }
	virtual const char*						getSectionName() const	{ return "._rebase info"; }
	void									encode();
private:
	using CompressedInfoLinkEditAtom<A>::fEncodedData;
	using CompressedInfoLinkEditAtom<A>::fWriter;
	typedef typename A::P					P;
	typedef typename A::P::uint_t			pint_t;
};

template <typename A>
class CompressedBindingInfoLinkEditAtom : public CompressedInfoLinkEditAtom<A>
{
public:
											CompressedBindingInfoLinkEditAtom(Writer<A>& writer) : CompressedInfoLinkEditAtom<A>(writer) { }
	virtual const char*						getDisplayName() const	{ return "compressed binding info"; }
	virtual const char*						getSectionName() const	{ return "._binding info"; }
	void									encode();
private:
	using CompressedInfoLinkEditAtom<A>::fWriter;
	using CompressedInfoLinkEditAtom<A>::fEncodedData;
	typedef typename A::P					P;
	typedef typename A::P::uint_t			pint_t;
};

template <typename A>
class CompressedWeakBindingInfoLinkEditAtom : public CompressedInfoLinkEditAtom<A>
{
public:
											CompressedWeakBindingInfoLinkEditAtom(Writer<A>& writer) : CompressedInfoLinkEditAtom<A>(writer) { }
	virtual const char*						getDisplayName() const	{ return "compressed weak binding info"; }
	virtual const char*						getSectionName() const	{ return "._wkbinding info"; }
	void									encode();
private:
	using CompressedInfoLinkEditAtom<A>::fWriter;
	using CompressedInfoLinkEditAtom<A>::fEncodedData;
	typedef typename A::P					P;
	typedef typename A::P::uint_t			pint_t;
};

template <typename A>
class CompressedLazyBindingInfoLinkEditAtom : public CompressedInfoLinkEditAtom<A>
{
public:
											CompressedLazyBindingInfoLinkEditAtom(Writer<A>& writer) : CompressedInfoLinkEditAtom<A>(writer) { }
	virtual const char*						getDisplayName() const	{ return "compressed lazy binding info"; }
	virtual const char*						getSectionName() const	{ return "._lzbinding info"; }
	void									encode();
private:
	std::vector<uint32_t>					fStarts;
	
	using CompressedInfoLinkEditAtom<A>::fWriter;
	using CompressedInfoLinkEditAtom<A>::fEncodedData;
	typedef typename A::P					P;
	typedef typename A::P::uint_t			pint_t;
};


template <typename A>
class CompressedExportInfoLinkEditAtom : public CompressedInfoLinkEditAtom<A>
{
public:
											CompressedExportInfoLinkEditAtom(Writer<A>& writer) 
											: CompressedInfoLinkEditAtom<A>(writer), fStartNode(strdup("")) { }
	virtual const char*						getDisplayName() const	{ return "compressed export info"; }
	virtual const char*						getSectionName() const	{ return "._export info"; }
	void									encode();
private:
	using WriterAtom<A>::fWriter;
	using CompressedInfoLinkEditAtom<A>::fEncodedData;
	typedef typename A::P					P;
	typedef typename A::P::uint_t			pint_t;
	struct node;
	
	struct edge
	{
						edge(const char* s, struct node* n) : fSubString(s), fChild(n) { }
						~edge() {  }
		const char*		fSubString;
		struct node*	fChild;
		
	};

	struct node
	{
							node(const char* s) : fCummulativeString(s), fAddress(0), fFlags(0), fOrdered(false), 
												fHaveExportInfo(false), fTrieOffset(0) {}
							~node() { }
		const char*			fCummulativeString;
		std::vector<edge>	fChildren;
		uint64_t			fAddress;
		uint32_t			fFlags;
		bool				fOrdered;
		bool				fHaveExportInfo;
		uint32_t			fTrieOffset;
		
		void addSymbol(const char* fullStr, uint64_t address, uint32_t flags) {
			const char* partialStr = &fullStr[strlen(fCummulativeString)];
			for (typename std::vector<edge>::iterator it = fChildren.begin(); it != fChildren.end(); ++it) {
				edge& e = *it;
				int subStringLen = strlen(e.fSubString);
				if ( strncmp(e.fSubString, partialStr, subStringLen) == 0 ) {
					// already have matching edge, go down that path
					e.fChild->addSymbol(fullStr, address, flags);
					return;
				}
				else {
					for (int i=subStringLen-1; i > 0; --i) {
						if ( strncmp(e.fSubString, partialStr, i) == 0 ) {
							// found a common substring, splice in new node
							//  was A -> C,  now A -> B -> C
							char* bNodeCummStr = strdup(e.fChild->fCummulativeString);
							bNodeCummStr[strlen(bNodeCummStr)+i-subStringLen] = '\0';
							//node* aNode = this;
							node* bNode = new node(bNodeCummStr);
							node* cNode = e.fChild;
							char* abEdgeStr = strdup(e.fSubString);
							abEdgeStr[i] = '\0';
							char* bcEdgeStr = strdup(&e.fSubString[i]);
							edge& abEdge = e;
							abEdge.fSubString = abEdgeStr;
							abEdge.fChild = bNode;
							edge bcEdge(bcEdgeStr, cNode);
							bNode->fChildren.push_back(bcEdge);
							bNode->addSymbol(fullStr, address, flags);
							return;
						}
					}
				}
			}
			// no commonality with any existing child, make a new edge that is this whole string
			node* newNode = new node(strdup(fullStr));
			edge newEdge(strdup(partialStr), newNode);
			fChildren.push_back(newEdge);
			newNode->fAddress = address;
			newNode->fFlags = flags;
			newNode->fHaveExportInfo = true;
		}
		
		void addOrderedNodes(const char* name, std::vector<node*>& orderedNodes) {
			if ( !fOrdered ) {
				orderedNodes.push_back(this);
				//fprintf(stderr, "ordered %p %s\n", this, fCummulativeString);
				fOrdered = true;
			}
			const char* partialStr = &name[strlen(fCummulativeString)];
			for (typename std::vector<edge>::iterator it = fChildren.begin(); it != fChildren.end(); ++it) {
				edge& e = *it;
				int subStringLen = strlen(e.fSubString);
				if ( strncmp(e.fSubString, partialStr, subStringLen) == 0 ) {
					// already have matching edge, go down that path
					e.fChild->addOrderedNodes(name, orderedNodes);
					return;
				}
			}
		}

		// byte for terminal node size in bytes, or 0x00 if not terminal node
		// teminal node (uleb128 flags, uleb128 addr)
		// byte for child node count
		//  each child: zero terminated substring, uleb128 node offset
		bool updateOffset(uint32_t& offset) {
			uint32_t nodeSize = 1; // byte for length of export info
			if ( fHaveExportInfo ) 
				nodeSize += ByteStream::uleb128_size(fFlags) + ByteStream::uleb128_size(fAddress);

			// add children
			++nodeSize; // byte for count of chidren
			for (typename std::vector<edge>::iterator it = fChildren.begin(); it != fChildren.end(); ++it) {
				edge& e = *it;
				nodeSize += strlen(e.fSubString) + 1 + ByteStream::uleb128_size(e.fChild->fTrieOffset);
			}
			bool result = (fTrieOffset != offset);
			fTrieOffset = offset;
			//fprintf(stderr, "updateOffset %p %05d %s\n", this, fTrieOffset, fCummulativeString);
			offset += nodeSize;
			// return true if fTrieOffset was changed
			return result;
		}
	
		void appendToStream(ByteStream& out) {
			if ( fHaveExportInfo ) {
				// nodes with export info: size, flags, address
				out.append_byte(out.uleb128_size(fFlags) + out.uleb128_size(fAddress));
				out.append_uleb128(fFlags);
				out.append_uleb128(fAddress);
			}
			else {
				// no export info
				out.append_byte(0);
			}
			// write number of children
			out.append_byte(fChildren.size());
			// write each child
			for (typename std::vector<edge>::iterator it = fChildren.begin(); it != fChildren.end(); ++it) {
				edge& e = *it;
				out.append_string(e.fSubString);
				out.append_uleb128(e.fChild->fTrieOffset);
			}
		}
	
	};


	struct node		fStartNode;
};

template <typename A>
class LocalRelocationsLinkEditAtom : public LinkEditAtom<A>
{
public:
											LocalRelocationsLinkEditAtom(Writer<A>& writer) : LinkEditAtom<A>(writer) { }
	virtual const char*						getDisplayName() const	{ return "local relocations"; }
	virtual uint64_t						getSize() const;
	virtual const char*						getSectionName() const	{ return "._local_relocs"; }
	virtual void							copyRawContent(uint8_t buffer[]) const;
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
};

template <typename A>
class SymbolTableLinkEditAtom : public LinkEditAtom<A>
{
public:
											SymbolTableLinkEditAtom(Writer<A>& writer) : LinkEditAtom<A>(writer) { }
	virtual const char*						getDisplayName() const	{ return "symbol table"; }
	virtual uint64_t						getSize() const;
	virtual const char*						getSectionName() const	{ return "._symbol_table"; }
	virtual void							copyRawContent(uint8_t buffer[]) const;
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
};

template <typename A>
class ExternalRelocationsLinkEditAtom : public LinkEditAtom<A>
{
public:
											ExternalRelocationsLinkEditAtom(Writer<A>& writer) : LinkEditAtom<A>(writer) { }
	virtual const char*						getDisplayName() const	{ return "external relocations"; }
	virtual uint64_t						getSize() const;
	virtual const char*						getSectionName() const	{ return "._extern_relocs"; }
	virtual void							copyRawContent(uint8_t buffer[]) const;
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
};

struct IndirectEntry {
	uint32_t	indirectIndex;
	uint32_t	symbolIndex;
};


template <typename A>
class SegmentSplitInfoContentAtom : public LinkEditAtom<A>
{
public:
											SegmentSplitInfoContentAtom(Writer<A>& writer) : LinkEditAtom<A>(writer), fCantEncode(false) { }
	virtual const char*						getDisplayName() const	{ return "split segment info"; }
	virtual uint64_t						getSize() const;
	virtual const char*						getSectionName() const	{ return "._split_info"; }
	virtual void							copyRawContent(uint8_t buffer[]) const;
	bool									canEncode()						{ return !fCantEncode; }
	void									setCantEncode()					{ fCantEncode = true; }
	void									add32bitPointerLocation(const ObjectFile::Atom* atom, uint32_t offset) { fKind1Locations.push_back(AtomAndOffset(atom, offset)); }
	void									add64bitPointerLocation(const ObjectFile::Atom* atom, uint32_t offset) { fKind2Locations.push_back(AtomAndOffset(atom, offset)); }
	void									addPPCHi16Location(const ObjectFile::Atom* atom, uint32_t offset) { fKind3Locations.push_back(AtomAndOffset(atom, offset)); }
	void									add32bitImportLocation(const ObjectFile::Atom* atom, uint32_t offset) { fKind4Locations.push_back(AtomAndOffset(atom, offset)); }
	void									encode();

private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
	typedef typename A::P::uint_t			pint_t;
	struct AtomAndOffset { 
			AtomAndOffset(const ObjectFile::Atom* a, uint32_t off) : atom(a), offset(off) {}
			const ObjectFile::Atom*		atom; 
			uint32_t					offset; 
	};
	void									uleb128EncodeAddresses(const std::vector<AtomAndOffset>& locations);
	
	std::vector<AtomAndOffset>				fKind1Locations;
	std::vector<AtomAndOffset>				fKind2Locations;
	std::vector<AtomAndOffset>				fKind3Locations;
	std::vector<AtomAndOffset>				fKind4Locations;
	std::vector<uint8_t>					fEncodedData;
	bool									fCantEncode;
};

template <typename A>
class IndirectTableLinkEditAtom : public LinkEditAtom<A>
{
public:
											IndirectTableLinkEditAtom(Writer<A>& writer) : LinkEditAtom<A>(writer) { }
	virtual const char*						getDisplayName() const	{ return "indirect symbol table"; }
	virtual uint64_t						getSize() const;
	virtual const char*						getSectionName() const	{ return "._indirect_syms"; }
	virtual void							copyRawContent(uint8_t buffer[]) const;

	std::vector<IndirectEntry>				fTable;

private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
};

template <typename A>
class ModuleInfoLinkEditAtom : public LinkEditAtom<A>
{
public:
											ModuleInfoLinkEditAtom(Writer<A>& writer) : LinkEditAtom<A>(writer), fModuleNameOffset(0) { }
	virtual const char*						getDisplayName() const	{ return "module table"; }
	virtual uint64_t						getSize() const;
	virtual const char*						getSectionName() const	{ return "._module_info"; }
	virtual void							copyRawContent(uint8_t buffer[]) const;

			void							setName() { fModuleNameOffset = fWriter.fStringsAtom->add("single module"); }
			uint32_t						getTableOfContentsFileOffset() const;
			uint32_t						getModuleTableFileOffset() const;
			uint32_t						getReferencesFileOffset() const;
			uint32_t						getReferencesCount() const;

private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
	typedef typename A::P::uint_t			pint_t;
	uint32_t								fModuleNameOffset;
};


class CStringEquals
{
public:
	bool operator()(const char* left, const char* right) const { return (strcmp(left, right) == 0); }
};

template <typename A>
class StringsLinkEditAtom : public LinkEditAtom<A>
{
public:
											StringsLinkEditAtom(Writer<A>& writer);
	virtual const char*						getDisplayName() const	{ return "string pool"; }
	virtual uint64_t						getSize() const;
	virtual const char*						getSectionName() const	{ return "._string_pool"; }
	virtual void							copyRawContent(uint8_t buffer[]) const;

	int32_t									add(const char* name);
	int32_t									addUnique(const char* name);
	int32_t									emptyString()			{ return 1; }
	const char*								stringForIndex(int32_t) const;

private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
	enum { kBufferSize = 0x01000000 };
	typedef __gnu_cxx::hash_map<const char*, int32_t, __gnu_cxx::hash<const char*>, CStringEquals> StringToOffset;

	std::vector<char*>						fFullBuffers;
	char*									fCurrentBuffer;
	uint32_t								fCurrentBufferUsed;
	StringToOffset							fUniqueStrings;
};



template <typename A>
class UndefinedSymbolProxyAtom : public WriterAtom<A>
{
public:
													UndefinedSymbolProxyAtom(Writer<A>& writer, const char* name) : WriterAtom<A>(writer, Segment::fgLinkEditSegment), fName(name) {}
	virtual const char*								getName() const				{ return fName; }
	virtual ObjectFile::Atom::Scope					getScope() const			{ return ObjectFile::Atom::scopeGlobal; }
	virtual ObjectFile::Atom::DefinitionKind		getDefinitionKind() const	{ return ObjectFile::Atom::kExternalDefinition; }
	virtual ObjectFile::Atom::SymbolTableInclusion	getSymbolTableInclusion() const	{ return ObjectFile::Atom::kSymbolTableIn; }
	virtual uint64_t								getSize() const				{ return 0; }
	virtual const char*								getSectionName() const		{ return "._imports"; }
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
	const char*								fName;
};

template <typename A>
class BranchIslandAtom : public WriterAtom<A>
{
public:
											BranchIslandAtom(Writer<A>& writer, const char* name, int islandRegion, ObjectFile::Atom& target, uint32_t targetOffset);
	virtual const char*						getName() const				{ return fName; }
	virtual ObjectFile::Atom::Scope			getScope() const			{ return ObjectFile::Atom::scopeLinkageUnit; }
	virtual uint64_t						getSize() const;
	virtual const char*						getSectionName() const		{ return "__text"; }
	virtual void							copyRawContent(uint8_t buffer[]) const;
private:
	using WriterAtom<A>::fWriter;
	const char*								fName;
	ObjectFile::Atom&						fTarget;
	uint32_t								fTargetOffset;
};

template <typename A>
class StubAtom : public WriterAtom<A>
{
public:
											StubAtom(Writer<A>& writer, ObjectFile::Atom& target, bool forLazyDylib);
	virtual const char*						getName() const				{ return fName; }
	virtual ObjectFile::Atom::Scope			getScope() const			{ return ObjectFile::Atom::scopeLinkageUnit; }
	virtual uint64_t						getSize() const;
	virtual ObjectFile::Alignment			getAlignment() const;
	virtual const char*						getSectionName() const		{ return "__symbol_stub1"; }
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const		{ return (std::vector<ObjectFile::Reference*>&)(fReferences); }
	virtual void							copyRawContent(uint8_t buffer[]) const;
	ObjectFile::Atom*						getTarget()					{ return &fTarget; }
private:
	static const char*						stubName(const char* importName);
	bool									pic() const					{ return fWriter.fSlideable; }
	using WriterAtom<A>::fWriter;
	const char*								fName;
	ObjectFile::Atom&						fTarget;
	std::vector<ObjectFile::Reference*>		fReferences;
	bool									fForLazyDylib;
};


template <typename A>
class FastStubHelperHelperAtom : public WriterAtom<A>
{
public:
													FastStubHelperHelperAtom(Writer<A>& writer);
	virtual const char*								getName() const					{ return " stub helpers"; }  // name sorts to start of helpers
	virtual ObjectFile::Atom::SymbolTableInclusion	getSymbolTableInclusion() const	{ return ObjectFile::Atom::kSymbolTableIn; }
	virtual ObjectFile::Atom::Scope					getScope() const				{ return ObjectFile::Atom::scopeLinkageUnit; }
	virtual uint64_t								getSize() const;
	virtual const char*								getSectionName() const			{ return "__stub_helper"; }
	virtual std::vector<ObjectFile::Reference*>&	getReferences() const			{ return (std::vector<ObjectFile::Reference*>&)(fReferences); }
	virtual void									copyRawContent(uint8_t buffer[]) const;
	virtual ObjectFile::Alignment					getAlignment() const			{ return ObjectFile::Alignment(0); }
protected:
	using WriterAtom<A>::fWriter;
	std::vector<ObjectFile::Reference*>		fReferences;
};

template <typename A>
class HybridStubHelperHelperAtom : public WriterAtom<A>
{
public:
													HybridStubHelperHelperAtom(Writer<A>& writer);
	virtual const char*								getName() const					{ return " stub helpers"; }  // name sorts to start of helpers
	virtual ObjectFile::Atom::SymbolTableInclusion	getSymbolTableInclusion() const	{ return ObjectFile::Atom::kSymbolTableIn; }
	virtual ObjectFile::Atom::Scope					getScope() const				{ return ObjectFile::Atom::scopeLinkageUnit; }
	virtual uint64_t								getSize() const;
	virtual const char*								getSectionName() const			{ return "__stub_helper"; }
	virtual std::vector<ObjectFile::Reference*>&	getReferences() const			{ return (std::vector<ObjectFile::Reference*>&)(fReferences); }
	virtual void									copyRawContent(uint8_t buffer[]) const;
	virtual ObjectFile::Alignment					getAlignment() const			{ return ObjectFile::Alignment(0); }
protected:
	using WriterAtom<A>::fWriter;
	std::vector<ObjectFile::Reference*>		fReferences;
};

template <typename A>
class StubHelperAtom : public WriterAtom<A>
{
public:
											StubHelperAtom(Writer<A>& writer, ObjectFile::Atom& target, 
														LazyPointerAtom<A>& lazyPointer, bool forLazyDylib) 
													: WriterAtom<A>(writer, Segment::fgTextSegment), fName(stubName(target.getName())), 
														fTarget(target), fLazyPointerAtom(lazyPointer) {
															writer.fAllSynthesizedStubHelpers.push_back(this);
														}

	virtual const char*						getName() const				{ return fName; }
	virtual ObjectFile::Atom::Scope			getScope() const			{ return ObjectFile::Atom::scopeLinkageUnit; }
	virtual const char*						getSectionName() const		{ return "__stub_helper"; }
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const	{ return (std::vector<ObjectFile::Reference*>&)(fReferences); }
	ObjectFile::Atom*						getTarget()					{ return &fTarget; }
	virtual ObjectFile::Alignment			getAlignment() const		{ return ObjectFile::Alignment(0); }
protected:
	static const char*						stubName(const char* importName);
	using WriterAtom<A>::fWriter;
	const char*								fName;
	ObjectFile::Atom&						fTarget;
	LazyPointerAtom<A>&						fLazyPointerAtom;
	std::vector<ObjectFile::Reference*>		fReferences;
};
										
template <typename A>
class ClassicStubHelperAtom : public StubHelperAtom<A>
{
public:
											ClassicStubHelperAtom(Writer<A>& writer, ObjectFile::Atom& target, 
															class LazyPointerAtom<A>& lazyPointer, bool forLazyDylib);

	virtual uint64_t						getSize() const;
	virtual void							copyRawContent(uint8_t buffer[]) const;
};												


template <typename A>
class HybridStubHelperAtom : public StubHelperAtom<A>
{
public:
											HybridStubHelperAtom(Writer<A>& writer, ObjectFile::Atom& target, 
															class LazyPointerAtom<A>& lazyPointer, bool forLazyDylib);

	virtual uint64_t						getSize() const;
	virtual void							copyRawContent(uint8_t buffer[]) const;
	static class HybridStubHelperHelperAtom<A>*	fgHelperHelperAtom;
};												
template <typename A> class HybridStubHelperHelperAtom<A>* HybridStubHelperAtom<A>::fgHelperHelperAtom = NULL;

template <typename A>
class FastStubHelperAtom : public StubHelperAtom<A>
{
public:
											FastStubHelperAtom(Writer<A>& writer, ObjectFile::Atom& target, 
																class LazyPointerAtom<A>& lazyPointer, bool forLazyDylib);
	virtual uint64_t						getSize() const;
	virtual void							copyRawContent(uint8_t buffer[]) const;
	static FastStubHelperHelperAtom<A>*		fgHelperHelperAtom;
};					
template <typename A>  FastStubHelperHelperAtom<A>* FastStubHelperAtom<A>::fgHelperHelperAtom = NULL;
			


template <typename A>
class LazyPointerAtom : public WriterAtom<A>
{
public:
											LazyPointerAtom(Writer<A>& writer, ObjectFile::Atom& target, 
															StubAtom<A>& stub, bool forLazyDylib);
	virtual const char*						getName() const				{ return fName; }
	virtual ObjectFile::Atom::Scope			getScope() const			{ return ObjectFile::Atom::scopeLinkageUnit; }
	virtual uint64_t						getSize() const				{ return sizeof(typename A::P::uint_t); }
	virtual const char*						getSectionName() const		{ return fForLazyDylib ? "__ld_symbol_ptr" : "__la_symbol_ptr"; }
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const		{ return (std::vector<ObjectFile::Reference*>&)(fReferences); }
	virtual void							copyRawContent(uint8_t buffer[]) const;
	ObjectFile::Atom*						getTarget()					{ return &fExternalTarget; }
	void									setLazyBindingInfoOffset(uint32_t off) { fLazyBindingOffset = off; }
	uint32_t								getLazyBindingInfoOffset()	{ return fLazyBindingOffset; }
private:
	using WriterAtom<A>::fWriter;
	static const char*						lazyPointerName(const char* importName);
	const char*								fName;
	ObjectFile::Atom&						fTarget;
	ObjectFile::Atom&						fExternalTarget;
	std::vector<ObjectFile::Reference*>		fReferences;
	bool									fForLazyDylib;
	uint32_t								fLazyBindingOffset;
};


template <typename A>
class NonLazyPointerAtom : public WriterAtom<A>
{
public:
											NonLazyPointerAtom(Writer<A>& writer, ObjectFile::Atom& target);
											NonLazyPointerAtom(Writer<A>& writer, const char* targetName);
											NonLazyPointerAtom(Writer<A>& writer);
	virtual const char*						getName() const				{ return fName; }
	virtual ObjectFile::Atom::Scope			getScope() const			{ return ObjectFile::Atom::scopeLinkageUnit; }
	virtual uint64_t						getSize() const				{ return sizeof(typename A::P::uint_t); }
	virtual const char*						getSectionName() const		{ return (fWriter.fOptions.outputKind() == Options::kKextBundle) ? "__got" : "__nl_symbol_ptr"; }
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const		{ return (std::vector<ObjectFile::Reference*>&)(fReferences); }
	virtual void							copyRawContent(uint8_t buffer[]) const;
	ObjectFile::Atom*						getTarget()					{ return fTarget; }
private:
	using WriterAtom<A>::fWriter;
	static const char*						nonlazyPointerName(const char* importName);
	const char*								fName;
	ObjectFile::Atom*						fTarget;
	std::vector<ObjectFile::Reference*>		fReferences;
};


template <typename A>
class ObjCInfoAtom : public WriterAtom<A>
{
public:
											ObjCInfoAtom(Writer<A>& writer, ObjectFile::Reader::ObjcConstraint objcContraint,
														bool objcReplacementClasses);
	virtual const char*						getName() const				{ return "objc$info"; }
	virtual ObjectFile::Atom::Scope			getScope() const			{ return ObjectFile::Atom::scopeLinkageUnit; }
	virtual uint64_t						getSize() const				{ return 8; }
	virtual const char*						getSectionName() const;
	virtual void							copyRawContent(uint8_t buffer[]) const;
private:
	Segment&								getInfoSegment() const;
	uint32_t								fContent[2];
};


template <typename A>
class WriterReference : public ObjectFile::Reference
{
public:
	typedef typename A::ReferenceKinds			Kinds;

							WriterReference(uint32_t offset, Kinds kind, ObjectFile::Atom* target,
											uint32_t toOffset=0, ObjectFile::Atom* fromTarget=NULL, uint32_t fromOffset=0)
										  : fKind(kind), fFixUpOffsetInSrc(offset), fTarget(target), fTargetName(target->getName()),
											fTargetOffset(toOffset), fFromTarget(fromTarget), fFromTargetOffset(fromOffset) {}
							WriterReference(uint32_t offset, Kinds kind, const char* targetName)
										  : fKind(kind), fFixUpOffsetInSrc(offset), fTarget(NULL), fTargetName(targetName),
											fTargetOffset(0), fFromTarget(NULL), fFromTargetOffset(0) {}

	virtual					~WriterReference() {}

	virtual ObjectFile::Reference::TargetBinding getTargetBinding() const { return (fTarget != NULL) ? ObjectFile::Reference::kBoundDirectly :  ObjectFile::Reference::kUnboundByName; }
	virtual ObjectFile::Reference::TargetBinding getFromTargetBinding() const { return (fFromTarget != NULL) ? ObjectFile::Reference::kBoundDirectly : ObjectFile::Reference::kDontBind; }
	virtual uint8_t			getKind() const									{ return (uint8_t)fKind; }
	virtual uint64_t		getFixUpOffset() const							{ return fFixUpOffsetInSrc; }
	virtual const char*		getTargetName() const							{ return fTargetName; }
	virtual ObjectFile::Atom& getTarget() const								{ return *fTarget; }
	virtual uint64_t		getTargetOffset() const							{ return fTargetOffset; }
	virtual ObjectFile::Atom& getFromTarget() const							{ return *fFromTarget; }
	virtual const char*		getFromTargetName() const						{ return fFromTarget->getName(); }
	virtual void			setTarget(ObjectFile::Atom& target, uint64_t offset)	{ fTarget = &target; fTargetOffset = offset; }
	virtual void			setFromTarget(ObjectFile::Atom& target)			{ fFromTarget = &target; }
	virtual void			setFromTargetName(const char* name)				{  }
	virtual void			setFromTargetOffset(uint64_t offset)			{ fFromTargetOffset = offset; }
	virtual const char*		getDescription() const							{ return "writer reference"; }
	virtual uint64_t		getFromTargetOffset() const						{ return fFromTargetOffset; }

private:
	Kinds					fKind;
	uint32_t				fFixUpOffsetInSrc;
	ObjectFile::Atom*		fTarget;
	const char*				fTargetName;
	uint32_t				fTargetOffset;
	ObjectFile::Atom*		fFromTarget;
	uint32_t				fFromTargetOffset;
};


template <typename A>
const char* StubHelperAtom<A>::stubName(const char* name)
{
	char* buf;
	asprintf(&buf, "%s$stubHelper", name);
	return buf;
}

template <>
ClassicStubHelperAtom<x86_64>::ClassicStubHelperAtom(Writer<x86_64>& writer, ObjectFile::Atom& target, 
															class LazyPointerAtom<x86_64>& lazyPointer, bool forLazyDylib)
	: StubHelperAtom<x86_64>(writer, target, lazyPointer, forLazyDylib)
{
	fReferences.push_back(new WriterReference<x86_64>(3, x86_64::kPCRel32, &fLazyPointerAtom));
	if ( forLazyDylib ) {
		if ( fWriter.fDyldLazyDylibHelper == NULL )
			throw "symbol dyld_lazy_dylib_stub_binding_helper not defined (usually in lazydylib1.o)";
		fReferences.push_back(new WriterReference<x86_64>(8, x86_64::kPCRel32, fWriter.fDyldLazyDylibHelper));
	}
	else {
		if ( fWriter.fDyldClassicHelperAtom == NULL )
			throw "symbol dyld_stub_binding_helper not defined (usually in crt1.o/dylib1.o/bundle1.o)";
		fReferences.push_back(new WriterReference<x86_64>(8, x86_64::kPCRel32, fWriter.fDyldClassicHelperAtom));
	}
}


template <>
uint64_t ClassicStubHelperAtom<x86_64>::getSize() const
{
	return 12;
}

template <>
void ClassicStubHelperAtom<x86_64>::copyRawContent(uint8_t buffer[]) const
{
	buffer[0]  = 0x4C;		// lea foo$lazy_ptr(%rip),%r11
	buffer[1]  = 0x8D;
	buffer[2]  = 0x1D;
	buffer[3]  = 0x00;
	buffer[4]  = 0x00;
	buffer[5]  = 0x00;
	buffer[6]  = 0x00;
	buffer[7]  = 0xE9;		// jmp dyld_stub_binding_helper
	buffer[8]  = 0x00;
	buffer[9]  = 0x00;
	buffer[10] = 0x00;
	buffer[11] = 0x00;
}


template <>
FastStubHelperHelperAtom<x86_64>::FastStubHelperHelperAtom(Writer<x86_64>& writer)
	: WriterAtom<x86_64>(writer, Segment::fgTextSegment)
{
	fReferences.push_back(new WriterReference<x86_64>(3, x86_64::kPCRel32, new NonLazyPointerAtom<x86_64>(writer)));
	fReferences.push_back(new WriterReference<x86_64>(11, x86_64::kPCRel32, writer.fFastStubGOTAtom));
}

template <>
uint64_t FastStubHelperHelperAtom<x86_64>::getSize() const
{
	return 16;
}

template <>
void FastStubHelperHelperAtom<x86_64>::copyRawContent(uint8_t buffer[]) const
{
	buffer[0]  = 0x4C;		// leaq dyld_mageLoaderCache(%rip),%r11
	buffer[1]  = 0x8D;
	buffer[2]  = 0x1D;
	buffer[3]  = 0x00;
	buffer[4]  = 0x00;
	buffer[5]  = 0x00;
	buffer[6]  = 0x00;
	buffer[7]  = 0x41;		// pushq %r11
	buffer[8]  = 0x53;
	buffer[9]  = 0xFF;		// jmp *_fast_lazy_bind(%rip)
	buffer[10] = 0x25;
	buffer[11] = 0x00;
	buffer[12] = 0x00;
	buffer[13] = 0x00;
	buffer[14] = 0x00;
	buffer[15] = 0x90;		// nop
}


template <>
HybridStubHelperHelperAtom<x86_64>::HybridStubHelperHelperAtom(Writer<x86_64>& writer)
	: WriterAtom<x86_64>(writer, Segment::fgTextSegment)
{
	if ( writer.fDyldClassicHelperAtom == NULL )
		throw "symbol dyld_stub_binding_helper not defined (usually in crt1.o/dylib1.o/bundle1.o)";
	fReferences.push_back(new WriterReference<x86_64>(3, x86_64::kPCRel32_1, writer.fFastStubGOTAtom));
	fReferences.push_back(new WriterReference<x86_64>(13, x86_64::kPCRel32, new NonLazyPointerAtom<x86_64>(writer)));
	fReferences.push_back(new WriterReference<x86_64>(21, x86_64::kPCRel32, writer.fFastStubGOTAtom));
	fReferences.push_back(new WriterReference<x86_64>(30, x86_64::kPCRel32, writer.fDyldClassicHelperAtom));
}

template <>
uint64_t HybridStubHelperHelperAtom<x86_64>::getSize() const
{
	return 34;
}

template <>
void HybridStubHelperHelperAtom<x86_64>::copyRawContent(uint8_t buffer[]) const
{
	buffer[0]  = 0x48;		// cmpl	$0x00,_fast_lazy_bind
	buffer[1]  = 0x83;		
	buffer[2]  = 0x3D;
	buffer[3]  = 0x00;		
	buffer[4]  = 0x00;
	buffer[5]  = 0x00;
	buffer[6]  = 0x00;
	buffer[7]  = 0x00;
	buffer[8]  = 0x74;		// je 16
	buffer[9]  = 0x0F;		
	buffer[10] = 0x4C;		// leaq	imageCache(%rip),%r11
	buffer[11] = 0x8D;		
	buffer[12] = 0x1D;		
	buffer[13] = 0x00;		
	buffer[14] = 0x00;
	buffer[15] = 0x00;
	buffer[16] = 0x00;
	buffer[17] = 0x41;		// pushq %r11
	buffer[18] = 0x53;
	buffer[19] = 0xFF;		// jmp *_fast_lazy_bind(%rip)
	buffer[20] = 0x25;	
	buffer[21] = 0x00;
	buffer[22] = 0x00;
	buffer[23] = 0x00;
	buffer[24] = 0x00;
	buffer[25] = 0x48;		// addq	$8,%rsp
	buffer[26] = 0x83;
	buffer[27] = 0xC4;
	buffer[28] = 0x08;
	buffer[29] = 0xE9;		// jmp dyld_stub_binding_helper
	buffer[30] = 0x00;
	buffer[31] = 0x00;
	buffer[32] = 0x00;
	buffer[33] = 0x00;
}


template <>
HybridStubHelperAtom<x86_64>::HybridStubHelperAtom(Writer<x86_64>& writer, ObjectFile::Atom& target, 
															class LazyPointerAtom<x86_64>& lazyPointer, bool forLazyDylib)
	: StubHelperAtom<x86_64>(writer, target, lazyPointer, forLazyDylib)
{
	if ( fgHelperHelperAtom == NULL ) {
		fgHelperHelperAtom = new HybridStubHelperHelperAtom<x86_64>::HybridStubHelperHelperAtom(fWriter);
		fWriter.fAllSynthesizedStubHelpers.push_back(fgHelperHelperAtom);
	}
	fReferences.push_back(new WriterReference<x86_64>(8,  x86_64::kPCRel32, &fLazyPointerAtom));
	fReferences.push_back(new WriterReference<x86_64>(13, x86_64::kPCRel32, fgHelperHelperAtom));
}

template <>
uint64_t HybridStubHelperAtom<x86_64>::getSize() const
{
	return 18;
}

template <>
void HybridStubHelperAtom<x86_64>::copyRawContent(uint8_t buffer[]) const
{
	buffer[0]  = 0x68;		// pushq $lazy-info-offset
	buffer[1]  = 0x00;
	buffer[2]  = 0x00;
	buffer[3]  = 0x00;
	buffer[4]  = 0x00;
	buffer[5]  = 0x4C;		// lea foo$lazy_ptr(%rip),%r11
	buffer[6]  = 0x8D;
	buffer[7]  = 0x1D;
	buffer[8]  = 0x00;
	buffer[9]  = 0x00;
	buffer[10] = 0x00;
	buffer[11] = 0x00;
	buffer[12] = 0xE9;		// jmp helper-helper
	buffer[13] = 0x00;
	buffer[14] = 0x00;
	buffer[15] = 0x00;
	buffer[16] = 0x00;
	buffer[17] = 0x90;		// nop
	
	// the lazy binding info is created later than this helper atom, so there
	// is no Reference to update.  Instead we blast the offset here.
	uint32_t offset;
	LittleEndian::set32(offset, fLazyPointerAtom.getLazyBindingInfoOffset());
	memcpy(&buffer[1], &offset, 4);
}

template <>
FastStubHelperAtom<x86_64>::FastStubHelperAtom(Writer<x86_64>& writer, ObjectFile::Atom& target, 
															class LazyPointerAtom<x86_64>& lazyPointer, bool forLazyDylib)
	: StubHelperAtom<x86_64>(writer, target, lazyPointer, forLazyDylib)
{
	if ( fgHelperHelperAtom == NULL ) {
		fgHelperHelperAtom = new FastStubHelperHelperAtom<x86_64>::FastStubHelperHelperAtom(fWriter);
		fWriter.fAllSynthesizedStubHelpers.push_back(fgHelperHelperAtom);
	}
	fReferences.push_back(new WriterReference<x86_64>(6, x86_64::kPCRel32, fgHelperHelperAtom));
}

template <>
uint64_t FastStubHelperAtom<x86_64>::getSize() const
{
	return 10;
}

template <>
void FastStubHelperAtom<x86_64>::copyRawContent(uint8_t buffer[]) const
{
	buffer[0]  = 0x68;		// pushq $lazy-info-offset
	buffer[1]  = 0x00;
	buffer[2]  = 0x00;
	buffer[3]  = 0x00;
	buffer[4]  = 0x00;
	buffer[5]  = 0xE9;		// jmp helperhelper
	buffer[6]  = 0x00;
	buffer[7]  = 0x00;
	buffer[8]  = 0x00;
	buffer[9]  = 0x00;
	
	// the lazy binding info is created later than this helper atom, so there
	// is no Reference to update.  Instead we blast the offset here.
	uint32_t offset;
	LittleEndian::set32(offset, fLazyPointerAtom.getLazyBindingInfoOffset());
	memcpy(&buffer[1], &offset, 4);
}

template <>
FastStubHelperHelperAtom<x86>::FastStubHelperHelperAtom(Writer<x86>& writer)
	: WriterAtom<x86>(writer, Segment::fgTextSegment)
{
	fReferences.push_back(new WriterReference<x86>(1, x86::kAbsolute32, new NonLazyPointerAtom<x86>(writer)));
	fReferences.push_back(new WriterReference<x86>(7, x86::kAbsolute32, writer.fFastStubGOTAtom));
}

template <>
uint64_t FastStubHelperHelperAtom<x86>::getSize() const
{
	return 12;
}

template <>
void FastStubHelperHelperAtom<x86>::copyRawContent(uint8_t buffer[]) const
{
	buffer[0]  = 0x68;		// pushl $dyld_ImageLoaderCache
	buffer[1]  = 0x00;
	buffer[2]  = 0x00;
	buffer[3]  = 0x00;
	buffer[4]  = 0x00;
	buffer[5]  = 0xFF;		// jmp *_fast_lazy_bind(%rip)
	buffer[6]  = 0x25;
	buffer[7]  = 0x00;
	buffer[8]  = 0x00;
	buffer[9]  = 0x00;
	buffer[10] = 0x00;
	buffer[11] = 0x90;		// nop
}


template <>
HybridStubHelperHelperAtom<x86>::HybridStubHelperHelperAtom(Writer<x86>& writer)
	: WriterAtom<x86>(writer, Segment::fgTextSegment)
{
	if ( writer.fDyldClassicHelperAtom == NULL )
		throw "symbol dyld_stub_binding_helper not defined (usually in crt1.o/dylib1.o/bundle1.o)";
	fReferences.push_back(new WriterReference<x86>(2,  x86::kAbsolute32, writer.fFastStubGOTAtom));
	fReferences.push_back(new WriterReference<x86>(18, x86::kPCRel32, writer.fDyldClassicHelperAtom));
	fReferences.push_back(new WriterReference<x86>(26, x86::kAbsolute32, new NonLazyPointerAtom<x86>(writer)));
	fReferences.push_back(new WriterReference<x86>(32, x86::kAbsolute32, writer.fFastStubGOTAtom));
}

template <>
uint64_t HybridStubHelperHelperAtom<x86>::getSize() const
{
	return 36;
}


template <>
void HybridStubHelperHelperAtom<x86>::copyRawContent(uint8_t buffer[]) const
{
	buffer[0]  = 0x83;		// cmpl	$0x00,_fast_lazy_bind
	buffer[1]  = 0x3D;
	buffer[2]  = 0x00;		
	buffer[3]  = 0x00;
	buffer[4]  = 0x00;
	buffer[5]  = 0x00;
	buffer[6]  = 0x00;
	buffer[7]  = 0x75;		// jne 22
	buffer[8]  = 0x0D;
	buffer[9]  = 0x89;		// %eax,4(%esp)
	buffer[10] = 0x44;
	buffer[11] = 0x24;
	buffer[12] = 0x04;
	buffer[13] = 0x58;		// popl	 %eax
	buffer[14] = 0x87;		// xchgl (%esp),%eax
	buffer[15] = 0x04;
	buffer[16] = 0x24;
	buffer[17] = 0xE9;		// jmpl dyld_stub_binding_helper
	buffer[18] = 0x00;
	buffer[19] = 0x00;
	buffer[20] = 0x00;		
	buffer[21] = 0x00;		
	buffer[22] = 0x83;		// addl	$0x04,%esp
	buffer[23] = 0xC4;
	buffer[24] = 0x04;
	buffer[25] = 0x68;		// pushl imageloadercahce
	buffer[26] = 0x00;
	buffer[27] = 0x00;
	buffer[28] = 0x00;
	buffer[29] = 0x00;		
	buffer[30] = 0xFF;		// jmp *_fast_lazy_bind(%rip)
	buffer[31] = 0x25;	
	buffer[32] = 0x00;
	buffer[33] = 0x00;
	buffer[34] = 0x00;
	buffer[35] = 0x00;	
}


template <>
ClassicStubHelperAtom<x86>::ClassicStubHelperAtom(Writer<x86>& writer, ObjectFile::Atom& target, 
															class LazyPointerAtom<x86>& lazyPointer, bool forLazyDylib)
	: StubHelperAtom<x86>(writer, target, lazyPointer, forLazyDylib)
{
	fReferences.push_back(new WriterReference<x86>(1, x86::kAbsolute32, &fLazyPointerAtom));
	if ( forLazyDylib ) {
		if ( fWriter.fDyldLazyDylibHelper == NULL )
			throw "symbol dyld_lazy_dylib_stub_binding_helper not defined (usually in lazydylib1.o)";
		fReferences.push_back(new WriterReference<x86>(6, x86::kPCRel32, fWriter.fDyldLazyDylibHelper));
	}
	else {
		if ( fWriter.fDyldClassicHelperAtom == NULL )
			throw "symbol dyld_stub_binding_helper not defined (usually in crt1.o/dylib1.o/bundle1.o)";
		fReferences.push_back(new WriterReference<x86>(6, x86::kPCRel32, fWriter.fDyldClassicHelperAtom));
	}
}

template <>
uint64_t ClassicStubHelperAtom<x86>::getSize() const
{
	return 10;
}

template <>
void ClassicStubHelperAtom<x86>::copyRawContent(uint8_t buffer[]) const
{
	buffer[0]  = 0x68;		// pushl $foo$lazy_ptr
	buffer[1]  = 0x00;
	buffer[2]  = 0x00;
	buffer[3]  = 0x00;
	buffer[4]  = 0x00;
	buffer[5]  = 0xE9;		// jmp helperhelper
	buffer[6]  = 0x00;
	buffer[7]  = 0x00;
	buffer[8]  = 0x00;
	buffer[9]  = 0x00;
}

template <>
HybridStubHelperAtom<x86>::HybridStubHelperAtom(Writer<x86>& writer, ObjectFile::Atom& target, 
															class LazyPointerAtom<x86>& lazyPointer, bool forLazyDylib)
	: StubHelperAtom<x86>(writer, target, lazyPointer, forLazyDylib)
{
	if ( fgHelperHelperAtom == NULL ) {
		fgHelperHelperAtom = new HybridStubHelperHelperAtom<x86>::HybridStubHelperHelperAtom(fWriter);
		fWriter.fAllSynthesizedStubHelpers.push_back(fgHelperHelperAtom);
	}	
	fReferences.push_back(new WriterReference<x86>(6,  x86::kAbsolute32, &fLazyPointerAtom));
	fReferences.push_back(new WriterReference<x86>(11, x86::kPCRel32, fgHelperHelperAtom));
}


template <>
uint64_t HybridStubHelperAtom<x86>::getSize() const
{
	return 16;
}

template <>
void HybridStubHelperAtom<x86>::copyRawContent(uint8_t buffer[]) const
{
	buffer[0]  = 0x68;		// pushl $lazy-info-offset
	buffer[1]  = 0x00;
	buffer[2]  = 0x00;
	buffer[3]  = 0x00;
	buffer[4]  = 0x00;
	buffer[5]  = 0x68;		// pushl $foo$lazy_ptr
	buffer[6]  = 0x00;
	buffer[7]  = 0x00;
	buffer[8]  = 0x00;
	buffer[9]  = 0x00;
	buffer[10] = 0xE9;		// jmp dyld_hybrid_stub_binding_helper
	buffer[11] = 0x00;
	buffer[12] = 0x00;
	buffer[13] = 0x00;
	buffer[14] = 0x00;
	buffer[15] = 0x90;		// nop
	
	// the lazy binding info is created later than this helper atom, so there
	// is no Reference to update.  Instead we blast the offset here.
	uint32_t offset;
	LittleEndian::set32(offset, fLazyPointerAtom.getLazyBindingInfoOffset());
	memcpy(&buffer[1], &offset, 4);	
}


template <>
FastStubHelperAtom<x86>::FastStubHelperAtom(Writer<x86>& writer, ObjectFile::Atom& target, 
															class LazyPointerAtom<x86>& lazyPointer, bool forLazyDylib)
	: StubHelperAtom<x86>(writer, target, lazyPointer, forLazyDylib)
{
	if ( fgHelperHelperAtom == NULL ) {
		fgHelperHelperAtom = new FastStubHelperHelperAtom<x86>::FastStubHelperHelperAtom(fWriter);
		fWriter.fAllSynthesizedStubHelpers.push_back(fgHelperHelperAtom);
	}
	fReferences.push_back(new WriterReference<x86>(6, x86::kPCRel32, fgHelperHelperAtom));
}


template <>
uint64_t FastStubHelperAtom<x86>::getSize() const
{
	return 10;
}

template <>
void FastStubHelperAtom<x86>::copyRawContent(uint8_t buffer[]) const
{
	buffer[0]  = 0x68;		// pushl $lazy-info-offset
	buffer[1]  = 0x00;
	buffer[2]  = 0x00;
	buffer[3]  = 0x00;
	buffer[4]  = 0x00;
	buffer[5]  = 0xE9;		// jmp helperhelper
	buffer[6]  = 0x00;
	buffer[7]  = 0x00;
	buffer[8]  = 0x00;
	buffer[9]  = 0x00;
	
	// the lazy binding info is created later than this helper atom, so there
	// is no Reference to update.  Instead we blast the offset here.
	uint32_t offset;
	LittleEndian::set32(offset, fLazyPointerAtom.getLazyBindingInfoOffset());
	memcpy(&buffer[1], &offset, 4);
}



// specialize lazy pointer for x86_64 to initially pointer to stub helper
template <>
LazyPointerAtom<x86_64>::LazyPointerAtom(Writer<x86_64>& writer, ObjectFile::Atom& target, StubAtom<x86_64>& stub, bool forLazyDylib)
 : WriterAtom<x86_64>(writer, Segment::fgDataSegment), fName(lazyPointerName(target.getName())), fTarget(target), 
	fExternalTarget(*stub.getTarget()), fForLazyDylib(forLazyDylib), fLazyBindingOffset(0)
{
	if ( forLazyDylib ) 
		writer.fAllSynthesizedLazyDylibPointers.push_back(this);
	else
		writer.fAllSynthesizedLazyPointers.push_back(this);

	ObjectFile::Atom* helper;
	if ( writer.fOptions.makeCompressedDyldInfo() && !forLazyDylib ) {
		if ( writer.fOptions.makeClassicDyldInfo() ) 
			// hybrid LINKEDIT, no fast bind info for weak symbols so use traditional helper
			if ( writer.targetRequiresWeakBinding(target) )
				helper = new ClassicStubHelperAtom<x86_64>(writer, target, *this, forLazyDylib);
			else
				helper = new HybridStubHelperAtom<x86_64>(writer, target, *this, forLazyDylib);
		else {
			if ( target.getDefinitionKind() == ObjectFile::Atom::kWeakDefinition ) 
				helper = &target;
			else
				helper = new FastStubHelperAtom<x86_64>(writer, target, *this, forLazyDylib);
		}
	}
	else {
		helper = new ClassicStubHelperAtom<x86_64>(writer, target, *this, forLazyDylib);
	}
	fReferences.push_back(new WriterReference<x86_64>(0, x86_64::kPointer, helper));
}


// specialize lazy pointer for x86 to initially pointer to stub helper
template <>
LazyPointerAtom<x86>::LazyPointerAtom(Writer<x86>& writer, ObjectFile::Atom& target, StubAtom<x86>& stub, bool forLazyDylib)
 : WriterAtom<x86>(writer, Segment::fgDataSegment), fName(lazyPointerName(target.getName())), fTarget(target), 
	fExternalTarget(*stub.getTarget()), fForLazyDylib(forLazyDylib)
{
	if ( forLazyDylib ) 
		writer.fAllSynthesizedLazyDylibPointers.push_back(this);
	else
		writer.fAllSynthesizedLazyPointers.push_back(this);

	ObjectFile::Atom* helper;
	if ( writer.fOptions.makeCompressedDyldInfo() && !forLazyDylib ) {
		if ( writer.fOptions.makeClassicDyldInfo() ) {
			// hybrid LINKEDIT, no fast bind info for weak symbols so use traditional helper
			if ( writer.targetRequiresWeakBinding(target) )
				helper = new ClassicStubHelperAtom<x86>(writer, target, *this, forLazyDylib);
			else
				helper = new HybridStubHelperAtom<x86>(writer, target, *this, forLazyDylib);
		}
		else {
			if ( target.getDefinitionKind() == ObjectFile::Atom::kWeakDefinition ) 
				helper = &target;
			else
				helper = new FastStubHelperAtom<x86>(writer, target, *this, forLazyDylib);
		}
	}
	else {
		helper = new ClassicStubHelperAtom<x86>(writer, target, *this, forLazyDylib);
	}
	fReferences.push_back(new WriterReference<x86>(0, x86::kPointer, helper));
}

template <typename A>
LazyPointerAtom<A>::LazyPointerAtom(Writer<A>& writer, ObjectFile::Atom& target, StubAtom<A>& stub, bool forLazyDylib)
 : WriterAtom<A>(writer, Segment::fgDataSegment), fName(lazyPointerName(target.getName())), fTarget(target), 
	fExternalTarget(*stub.getTarget()), fForLazyDylib(forLazyDylib)
{
	if ( forLazyDylib ) 
		writer.fAllSynthesizedLazyDylibPointers.push_back(this);
	else
		writer.fAllSynthesizedLazyPointers.push_back(this);

	fReferences.push_back(new WriterReference<A>(0, A::kPointer, &target));
}



template <typename A>
const char* LazyPointerAtom<A>::lazyPointerName(const char* name)
{
	char* buf;
	asprintf(&buf, "%s$lazy_pointer", name);
	return buf;
}

template <typename A>
void LazyPointerAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	bzero(buffer, getSize());
}


template <typename A>
NonLazyPointerAtom<A>::NonLazyPointerAtom(Writer<A>& writer, ObjectFile::Atom& target)
 : WriterAtom<A>(writer, Segment::fgDataSegment), fName(nonlazyPointerName(target.getName())), fTarget(&target)
{
	writer.fAllSynthesizedNonLazyPointers.push_back(this);
	fReferences.push_back(new WriterReference<A>(0, A::kPointer, &target));
}

template <typename A>
NonLazyPointerAtom<A>::NonLazyPointerAtom(Writer<A>& writer)
 : WriterAtom<A>(writer, Segment::fgDataSegment), fName("none"), fTarget(NULL)
{
	writer.fAllSynthesizedNonLazyPointers.push_back(this);
}

template <typename A>
NonLazyPointerAtom<A>::NonLazyPointerAtom(Writer<A>& writer, const char* targetName)
 : WriterAtom<A>(writer, Segment::fgDataSegment), fName(nonlazyPointerName(targetName)), fTarget(NULL)
{
	writer.fAllSynthesizedNonLazyPointers.push_back(this);
	fReferences.push_back(new WriterReference<A>(0, A::kPointer, targetName));
}

template <typename A>
const char* NonLazyPointerAtom<A>::nonlazyPointerName(const char* name)
{
	char* buf;
	asprintf(&buf, "%s$non_lazy_pointer", name);
	return buf;
}

template <typename A>
void NonLazyPointerAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	bzero(buffer, getSize());
}



template <>
bool StubAtom<ppc64>::pic() const
{
	// no-pic stubs for ppc64 don't work if lazy pointer is above low 2GB.
	// Usually that only happens if page zero is very large
	return ( fWriter.fSlideable || ((fWriter.fPageZeroAtom != NULL) && (fWriter.fPageZeroAtom->getSize() > 4096)) );
}


template <>
bool StubAtom<arm>::pic() const
{
	return fWriter.fSlideable;
}

template <>
ObjectFile::Alignment StubAtom<ppc>::getAlignment() const
{
	return 2;
}

template <>
ObjectFile::Alignment StubAtom<ppc64>::getAlignment() const
{
	return 2;
}

template <>
ObjectFile::Alignment StubAtom<arm>::getAlignment() const
{
	return 2;
}

template <>
StubAtom<ppc>::StubAtom(Writer<ppc>& writer, ObjectFile::Atom& target, bool forLazyDylib)
 : WriterAtom<ppc>(writer, Segment::fgTextSegment), fName(stubName(target.getName())), 
	fTarget(target), fForLazyDylib(forLazyDylib)
{
	writer.fAllSynthesizedStubs.push_back(this);
	LazyPointerAtom<ppc>* lp;
	if (  fWriter.fOptions.prebind() ) {
		// for prebound ppc, lazy pointer starts out pointing to target symbol's address
		// if target is a weak definition within this linkage unit or zero if in some dylib
		lp = new LazyPointerAtom<ppc>(writer, target, *this, forLazyDylib);
	}
	else {
		// for non-prebound ppc, lazy pointer starts out pointing to dyld_stub_binding_helper glue code
		if ( forLazyDylib ) {
			if ( writer.fDyldLazyDylibHelper == NULL )
				throw "symbol dyld_lazy_dylib_stub_binding_helper not defined (usually in lazydylib1.o)";
			lp = new LazyPointerAtom<ppc>(writer, *writer.fDyldLazyDylibHelper, *this, forLazyDylib);
		}
		else {
			if ( writer.fDyldClassicHelperAtom == NULL )
				throw "symbol dyld_stub_binding_helper not defined (usually in crt1.o/dylib1.o/bundle1.o)";
			lp = new LazyPointerAtom<ppc>(writer, *writer.fDyldClassicHelperAtom, *this, forLazyDylib);
		}
	}
	if ( pic() ) {
		// picbase is 8 bytes into atom
		fReferences.push_back(new WriterReference<ppc>(12, ppc::kPICBaseHigh16, lp, 0, this, 8));
		fReferences.push_back(new WriterReference<ppc>(20, ppc::kPICBaseLow16, lp, 0, this, 8));
	}
	else {
		fReferences.push_back(new WriterReference<ppc>(0, ppc::kAbsHigh16AddLow, lp));
		fReferences.push_back(new WriterReference<ppc>(4, ppc::kAbsLow16, lp));
	}
}

template <>
StubAtom<ppc64>::StubAtom(Writer<ppc64>& writer, ObjectFile::Atom& target, bool forLazyDylib)
 : WriterAtom<ppc64>(writer, Segment::fgTextSegment), fName(stubName(target.getName())), 
	fTarget(target), fForLazyDylib(forLazyDylib)
{
	writer.fAllSynthesizedStubs.push_back(this);

	LazyPointerAtom<ppc64>* lp;
	if ( forLazyDylib ) {
		if ( writer.fDyldLazyDylibHelper == NULL )
			throw "symbol dyld_lazy_dylib_stub_binding_helper not defined (usually in lazydylib1.o)";
		lp = new LazyPointerAtom<ppc64>(writer, *writer.fDyldLazyDylibHelper, *this, forLazyDylib);
	}
	else {
		if ( writer.fDyldClassicHelperAtom == NULL )
			throw "symbol dyld_stub_binding_helper not defined (usually in crt1.o/dylib1.o/bundle1.o)";
		lp = new LazyPointerAtom<ppc64>(writer, *writer.fDyldClassicHelperAtom, *this, forLazyDylib);
	}
	if ( pic() ) {
		// picbase is 8 bytes into atom
		fReferences.push_back(new WriterReference<ppc64>(12, ppc64::kPICBaseHigh16, lp, 0, this, 8));
		fReferences.push_back(new WriterReference<ppc64>(20, ppc64::kPICBaseLow14, lp, 0, this, 8));
	}
	else {
		fReferences.push_back(new WriterReference<ppc64>(0, ppc64::kAbsHigh16AddLow, lp));
		fReferences.push_back(new WriterReference<ppc64>(4, ppc64::kAbsLow14, lp));
	}
}

template <>
StubAtom<x86>::StubAtom(Writer<x86>& writer, ObjectFile::Atom& target, bool forLazyDylib)
 : WriterAtom<x86>(writer, (writer.fOptions.makeCompressedDyldInfo()|| forLazyDylib) ? Segment::fgTextSegment : Segment::fgImportSegment),
					fName(NULL), fTarget(target), fForLazyDylib(forLazyDylib)
{
	if ( writer.fOptions.makeCompressedDyldInfo() || forLazyDylib ) {
		fName = stubName(target.getName());
		LazyPointerAtom<x86>* lp = new LazyPointerAtom<x86>(writer, target, *this, forLazyDylib);
		fReferences.push_back(new WriterReference<x86>(2, x86::kAbsolute32, lp));
		writer.fAllSynthesizedStubs.push_back(this);
	}
	else {
		if ( &target == NULL ) 
			fName = "cache-line-crossing-stub";
		else {
			fName = stubName(target.getName());
			writer.fAllSynthesizedStubs.push_back(this);
		}
	}
}


template <>
StubAtom<x86_64>::StubAtom(Writer<x86_64>& writer, ObjectFile::Atom& target, bool forLazyDylib)
 : WriterAtom<x86_64>(writer, Segment::fgTextSegment), fName(stubName(target.getName())), fTarget(target)
{
	writer.fAllSynthesizedStubs.push_back(this);

	LazyPointerAtom<x86_64>* lp = new LazyPointerAtom<x86_64>(writer, target, *this, forLazyDylib);
	fReferences.push_back(new WriterReference<x86_64>(2, x86_64::kPCRel32, lp));
}

template <>
StubAtom<arm>::StubAtom(Writer<arm>& writer, ObjectFile::Atom& target, bool forLazyDylib)
 : WriterAtom<arm>(writer, Segment::fgTextSegment), fName(stubName(target.getName())), fTarget(target)
{
	writer.fAllSynthesizedStubs.push_back(this);

	LazyPointerAtom<arm>* lp;
	if (  fWriter.fOptions.prebind() && !forLazyDylib ) {
		// for prebound arm, lazy pointer starts out pointing to target symbol's address
		// if target is a weak definition within this linkage unit or zero if in some dylib
		lp = new LazyPointerAtom<arm>(writer, target, *this, forLazyDylib);
	}
	else {
		// for non-prebound arm, lazy pointer starts out pointing to dyld_stub_binding_helper glue code
		ObjectFile::Atom* helper;
		if ( forLazyDylib ) {
			if ( writer.fDyldLazyDylibHelper == NULL )
				throw "symbol dyld_lazy_dylib_stub_binding_helper not defined (usually in lazydylib1.o)";
			helper = writer.fDyldLazyDylibHelper;
		}
		else {
			if ( writer.fDyldClassicHelperAtom == NULL )
				throw "symbol dyld_stub_binding_helper not defined (usually in crt1.o/dylib1.o/bundle1.o)";
			helper = writer.fDyldClassicHelperAtom;
		}
		lp = new LazyPointerAtom<arm>(writer, *helper, *this, forLazyDylib);
	}
	if ( pic() )
		fReferences.push_back(new WriterReference<arm>(12, arm::kPointerDiff, lp, 0, this, 12));
	else
		fReferences.push_back(new WriterReference<arm>(8, arm::kPointer, lp));
}

template <typename A>
const char* StubAtom<A>::stubName(const char* name)
{
	char* buf;
	asprintf(&buf, "%s$stub", name);
	return buf;
}

template <>
uint64_t StubAtom<ppc>::getSize() const
{
	return ( pic() ? 32 : 16 );
}

template <>
uint64_t StubAtom<ppc64>::getSize() const
{
	return ( pic() ? 32 : 16 );
}


template <>
uint64_t StubAtom<arm>::getSize() const
{
	return ( pic() ? 16 : 12 );
}

template <>
uint64_t StubAtom<x86>::getSize() const
{
	if ( fWriter.fOptions.makeCompressedDyldInfo() || fForLazyDylib )
		return 6;
	else
		return 5;
}

template <>
uint64_t StubAtom<x86_64>::getSize() const
{
	return 6;
}

template <>
ObjectFile::Alignment StubAtom<x86>::getAlignment() const
{
	if ( fWriter.fOptions.makeCompressedDyldInfo() || fForLazyDylib )
		return 1;
	else
		return 0; // special case x86 self-modifying stubs to be byte aligned
}

template <>
void StubAtom<ppc64>::copyRawContent(uint8_t buffer[]) const
{
	if ( pic() ) {
		OSWriteBigInt32(&buffer [0], 0, 0x7c0802a6);	// 	mflr r0
		OSWriteBigInt32(&buffer[ 4], 0, 0x429f0005);	//  bcl 20,31,Lpicbase
		OSWriteBigInt32(&buffer[ 8], 0, 0x7d6802a6);	// Lpicbase: mflr r11
		OSWriteBigInt32(&buffer[12], 0, 0x3d6b0000);	// 	addis r11,r11,ha16(L_fwrite$lazy_ptr-Lpicbase)
		OSWriteBigInt32(&buffer[16], 0, 0x7c0803a6);	// 	mtlr r0
		OSWriteBigInt32(&buffer[20], 0, 0xe98b0001);	// 	ldu r12,lo16(L_fwrite$lazy_ptr-Lpicbase)(r11)
		OSWriteBigInt32(&buffer[24], 0, 0x7d8903a6);	//  mtctr r12
		OSWriteBigInt32(&buffer[28], 0, 0x4e800420);	//  bctr
	}
	else {
		OSWriteBigInt32(&buffer[ 0], 0, 0x3d600000);	// lis r11,ha16(L_fwrite$lazy_ptr)
		OSWriteBigInt32(&buffer[ 4], 0, 0xe98b0001);	// ldu r12,lo16(L_fwrite$lazy_ptr)(r11)
		OSWriteBigInt32(&buffer[ 8], 0, 0x7d8903a6);	// mtctr r12
		OSWriteBigInt32(&buffer[12], 0, 0x4e800420);	// bctr
	}
}

template <>
void StubAtom<ppc>::copyRawContent(uint8_t buffer[]) const
{
	if ( pic() ) {
		OSWriteBigInt32(&buffer[ 0], 0, 0x7c0802a6);	// 	mflr r0
		OSWriteBigInt32(&buffer[ 4], 0, 0x429f0005);	//  bcl 20,31,Lpicbase
		OSWriteBigInt32(&buffer[ 8], 0, 0x7d6802a6);	// Lpicbase: mflr r11
		OSWriteBigInt32(&buffer[12], 0, 0x3d6b0000);	// 	addis r11,r11,ha16(L_fwrite$lazy_ptr-Lpicbase)
		OSWriteBigInt32(&buffer[16], 0, 0x7c0803a6);	// 	mtlr r0
		OSWriteBigInt32(&buffer[20], 0, 0x858b0000);	// 	lwzu r12,lo16(L_fwrite$lazy_ptr-Lpicbase)(r11)
		OSWriteBigInt32(&buffer[24], 0, 0x7d8903a6);	//  mtctr r12
		OSWriteBigInt32(&buffer[28], 0, 0x4e800420);	//  bctr
	}
	else {
		OSWriteBigInt32(&buffer[ 0], 0, 0x3d600000);	// lis r11,ha16(L_fwrite$lazy_ptr)
		OSWriteBigInt32(&buffer[ 4], 0, 0x858b0000);	// lwzu r12,lo16(L_fwrite$lazy_ptr)(r11)
		OSWriteBigInt32(&buffer[ 8], 0, 0x7d8903a6);	// mtctr r12
		OSWriteBigInt32(&buffer[12], 0, 0x4e800420);	// bctr
	}
}

template <>
void StubAtom<x86>::copyRawContent(uint8_t buffer[]) const
{
	if ( fWriter.fOptions.makeCompressedDyldInfo() || fForLazyDylib ) {
		buffer[0] = 0xFF;		// jmp *foo$lazy_pointer
		buffer[1] = 0x25;
		buffer[2] = 0x00;
		buffer[3] = 0x00;
		buffer[4] = 0x00;
		buffer[5] = 0x00;
	}
	else {
		if ( fWriter.fOptions.prebind() ) {
			uint32_t address = this->getAddress();
			int32_t rel32 = 0 - (address+5); 
			buffer[0] = 0xE9;
			buffer[1] = rel32 & 0xFF;
			buffer[2] = (rel32 >> 8) & 0xFF;
			buffer[3] = (rel32 >> 16) & 0xFF;
			buffer[4] = (rel32 >> 24) & 0xFF;
		}
		else {
			buffer[0] = 0xF4;
			buffer[1] = 0xF4;
			buffer[2] = 0xF4;
			buffer[3] = 0xF4;
			buffer[4] = 0xF4;
		}
	}
}

template <>
void StubAtom<x86_64>::copyRawContent(uint8_t buffer[]) const
{
	buffer[0] = 0xFF;		// jmp *foo$lazy_pointer(%rip)
	buffer[1] = 0x25;
	buffer[2] = 0x00;
	buffer[3] = 0x00;
	buffer[4] = 0x00;
	buffer[5] = 0x00;
}

template <>
void StubAtom<arm>::copyRawContent(uint8_t buffer[]) const
{
	if ( pic() ) {
		OSWriteLittleInt32(&buffer[ 0], 0, 0xe59fc004);	// 	ldr ip, pc + 12
		OSWriteLittleInt32(&buffer[ 4], 0, 0xe08fc00c);	// 	add ip, pc, ip
		OSWriteLittleInt32(&buffer[ 8], 0, 0xe59cf000);	// 	ldr pc, [ip]
		OSWriteLittleInt32(&buffer[12], 0, 0x00000000);	// 	.long L_foo$lazy_ptr - (L1$scv + 8)
	}
	else {
		OSWriteLittleInt32(&buffer[ 0], 0, 0xe59fc000);	// 	ldr ip, [pc, #0]
		OSWriteLittleInt32(&buffer[ 4], 0, 0xe59cf000);	// 	ldr pc, [ip]
		OSWriteLittleInt32(&buffer[ 8], 0, 0x00000000);	// 	.long   L_foo$lazy_ptr
	}
}

// x86_64 stubs are 6 bytes 
template <>
ObjectFile::Alignment StubAtom<x86_64>::getAlignment() const
{
	return 1;
}

template <>
const char*	StubAtom<ppc>::getSectionName() const
{
	return ( pic() ? "__picsymbolstub1" : "__symbol_stub1");
}

template <>
const char*	StubAtom<ppc64>::getSectionName() const
{
	return ( pic() ? "__picsymbolstub1" : "__symbol_stub1");
}

template <>
const char*	StubAtom<arm>::getSectionName() const
{
	return ( pic() ? "__picsymbolstub4" : "__symbol_stub4");
}

template <>
const char*	StubAtom<x86>::getSectionName() const
{
	if ( fWriter.fOptions.makeCompressedDyldInfo() || fForLazyDylib ) 
		return "__symbol_stub";
	else
		return "__jump_table";
}




struct AtomByNameSorter
{
     bool operator()(ObjectFile::Atom* left, ObjectFile::Atom* right)
     {
          return (strcmp(left->getName(), right->getName()) < 0);
     }
};

template <typename P>
struct ExternalRelocSorter
{
     bool operator()(const macho_relocation_info<P>& left, const macho_relocation_info<P>& right)
     {
		// sort first by symbol number
		if ( left.r_symbolnum() != right.r_symbolnum() )
			return (left.r_symbolnum() < right.r_symbolnum());
		// then sort all uses of the same symbol by address
		return (left.r_address() < right.r_address());
     }
};


template <typename A>
Writer<A>::Writer(const char* path, Options& options, std::vector<ExecutableFile::DyLibUsed>& dynamicLibraries)
	: ExecutableFile::Writer(dynamicLibraries), fFilePath(strdup(path)), fOptions(options), 
	  fAllAtoms(NULL), fStabs(NULL), fRegularDefAtomsThatOverrideADylibsWeakDef(NULL), fLoadCommandsSection(NULL),
	  fLoadCommandsSegment(NULL), fMachHeaderAtom(NULL), fEncryptionLoadCommand(NULL), fSegmentCommands(NULL), 
	  fSymbolTableCommands(NULL), fHeaderPadding(NULL), fUnwindInfoAtom(NULL),
	  fUUIDAtom(NULL), fPadSegmentInfo(NULL), fEntryPoint( NULL), 
	  fDyldClassicHelperAtom(NULL), fDyldCompressedHelperAtom(NULL), fDyldLazyDylibHelper(NULL),
	  fSectionRelocationsAtom(NULL),   fCompressedRebaseInfoAtom(NULL),  fCompressedBindingInfoAtom(NULL),
	  fCompressedWeakBindingInfoAtom(NULL), fCompressedLazyBindingInfoAtom(NULL), fCompressedExportInfoAtom(NULL),
	  fLocalRelocationsAtom(NULL), fExternalRelocationsAtom(NULL),
	  fSymbolTableAtom(NULL), fSplitCodeToDataContentAtom(NULL), fIndirectTableAtom(NULL), fModuleInfoAtom(NULL), 
	  fStringsAtom(NULL), fPageZeroAtom(NULL), fFastStubGOTAtom(NULL), fSymbolTable(NULL), fSymbolTableCount(0), 
	  fSymbolTableStabsCount(0), fSymbolTableLocalCount(0), fSymbolTableExportCount(0), fSymbolTableImportCount(0), 
	  fLargestAtomSize(1), 
	  fEmitVirtualSections(false), fHasWeakExports(false), fReferencesWeakImports(false), 
	  fCanScatter(false), fWritableSegmentPastFirst4GB(false), fNoReExportedDylibs(false), 
	  fBiggerThanTwoGigs(false), fSlideable(false), 
	  fFirstWritableSegment(NULL), fAnonNameIndex(1000)
{
	switch ( fOptions.outputKind() ) {
		case Options::kDynamicExecutable:
		case Options::kStaticExecutable:
			if ( fOptions.zeroPageSize() != 0 )
				fWriterSynthesizedAtoms.push_back(fPageZeroAtom = new PageZeroAtom<A>(*this));
			if ( fOptions.outputKind() == Options::kDynamicExecutable )
				fWriterSynthesizedAtoms.push_back(new DsoHandleAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fMachHeaderAtom = new MachHeaderAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new SegmentLoadCommandsAtom<A>(*this));
			if ( fOptions.makeCompressedDyldInfo() ) 
				fWriterSynthesizedAtoms.push_back(new DyldInfoLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new SymbolTableLoadCommandsAtom<A>(*this));
			if ( fOptions.outputKind() == Options::kDynamicExecutable )
				fWriterSynthesizedAtoms.push_back(new DyldLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fUUIDAtom = new UUIDLoadCommandAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new ThreadsLoadCommandsAtom<A>(*this));
			if ( fOptions.hasCustomStack() )
				fWriterSynthesizedAtoms.push_back(new CustomStackAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fHeaderPadding = new LoadCommandsPaddingAtom<A>(*this));
			if ( fOptions.needsUnwindInfoSection() )
				fWriterSynthesizedAtoms.push_back(fUnwindInfoAtom = new UnwindInfoAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fSectionRelocationsAtom = new SectionRelocationsLinkEditAtom<A>(*this));
			if ( fOptions.makeCompressedDyldInfo() ) {
				fWriterSynthesizedAtoms.push_back(fCompressedRebaseInfoAtom = new CompressedRebaseInfoLinkEditAtom<A>(*this));
				fWriterSynthesizedAtoms.push_back(fCompressedBindingInfoAtom = new CompressedBindingInfoLinkEditAtom<A>(*this));
				fWriterSynthesizedAtoms.push_back(fCompressedWeakBindingInfoAtom = new CompressedWeakBindingInfoLinkEditAtom<A>(*this));
				fWriterSynthesizedAtoms.push_back(fCompressedLazyBindingInfoAtom = new CompressedLazyBindingInfoLinkEditAtom<A>(*this));
				fWriterSynthesizedAtoms.push_back(fCompressedExportInfoAtom = new CompressedExportInfoLinkEditAtom<A>(*this));
			}
			if ( fOptions.makeClassicDyldInfo() ) 
				fWriterSynthesizedAtoms.push_back(fLocalRelocationsAtom = new LocalRelocationsLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fSymbolTableAtom = new SymbolTableLinkEditAtom<A>(*this));
			if ( fOptions.makeClassicDyldInfo() ) 
				fWriterSynthesizedAtoms.push_back(fExternalRelocationsAtom = new ExternalRelocationsLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fIndirectTableAtom = new IndirectTableLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fStringsAtom = new StringsLinkEditAtom<A>(*this));
			break;
		case Options::kPreload:
			fWriterSynthesizedAtoms.push_back(fMachHeaderAtom = new MachHeaderAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new SegmentLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new SymbolTableLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fUUIDAtom = new UUIDLoadCommandAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new ThreadsLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fHeaderPadding = new LoadCommandsPaddingAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fSectionRelocationsAtom = new SectionRelocationsLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fLocalRelocationsAtom = new LocalRelocationsLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fExternalRelocationsAtom = new ExternalRelocationsLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fIndirectTableAtom = new IndirectTableLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fSymbolTableAtom = new SymbolTableLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fStringsAtom = new StringsLinkEditAtom<A>(*this));
			break;
		case Options::kDynamicLibrary:
		case Options::kDynamicBundle:
			fWriterSynthesizedAtoms.push_back(new DsoHandleAtom<A>(*this));
		case Options::kKextBundle:
			fWriterSynthesizedAtoms.push_back(fMachHeaderAtom = new MachHeaderAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new SegmentLoadCommandsAtom<A>(*this));
			if ( fOptions.outputKind() == Options::kDynamicLibrary ) {
				fWriterSynthesizedAtoms.push_back(new DylibIDLoadCommandsAtom<A>(*this));
				if ( fOptions.initFunctionName() != NULL )
					fWriterSynthesizedAtoms.push_back(new RoutinesLoadCommandsAtom<A>(*this));
			}
			fWriterSynthesizedAtoms.push_back(fUUIDAtom = new UUIDLoadCommandAtom<A>(*this));
			if ( fOptions.makeCompressedDyldInfo() )
				fWriterSynthesizedAtoms.push_back(new DyldInfoLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new SymbolTableLoadCommandsAtom<A>(*this));
			if ( fOptions.sharedRegionEligible() )
				fWriterSynthesizedAtoms.push_back(new SegmentSplitInfoLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fHeaderPadding = new LoadCommandsPaddingAtom<A>(*this));
			if ( fOptions.needsUnwindInfoSection() )
				fWriterSynthesizedAtoms.push_back(fUnwindInfoAtom = new UnwindInfoAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fSectionRelocationsAtom = new SectionRelocationsLinkEditAtom<A>(*this));
			if ( fOptions.makeCompressedDyldInfo() ) {
				fWriterSynthesizedAtoms.push_back(fCompressedRebaseInfoAtom = new CompressedRebaseInfoLinkEditAtom<A>(*this));
				fWriterSynthesizedAtoms.push_back(fCompressedBindingInfoAtom = new CompressedBindingInfoLinkEditAtom<A>(*this));
				fWriterSynthesizedAtoms.push_back(fCompressedWeakBindingInfoAtom = new CompressedWeakBindingInfoLinkEditAtom<A>(*this));
				fWriterSynthesizedAtoms.push_back(fCompressedLazyBindingInfoAtom = new CompressedLazyBindingInfoLinkEditAtom<A>(*this));
				fWriterSynthesizedAtoms.push_back(fCompressedExportInfoAtom = new CompressedExportInfoLinkEditAtom<A>(*this));
			}
			if ( fOptions.makeClassicDyldInfo() ) 
				fWriterSynthesizedAtoms.push_back(fLocalRelocationsAtom = new LocalRelocationsLinkEditAtom<A>(*this));
			if ( fOptions.sharedRegionEligible() ) {
				fWriterSynthesizedAtoms.push_back(fSplitCodeToDataContentAtom = new SegmentSplitInfoContentAtom<A>(*this));
			}
			fWriterSynthesizedAtoms.push_back(fSymbolTableAtom = new SymbolTableLinkEditAtom<A>(*this));
			if ( fOptions.makeClassicDyldInfo() ) 
				fWriterSynthesizedAtoms.push_back(fExternalRelocationsAtom = new ExternalRelocationsLinkEditAtom<A>(*this));
			if ( fOptions.outputKind() != Options::kKextBundle ) 
				fWriterSynthesizedAtoms.push_back(fIndirectTableAtom = new IndirectTableLinkEditAtom<A>(*this));
			if ( this->needsModuleTable() )
				fWriterSynthesizedAtoms.push_back(fModuleInfoAtom = new ModuleInfoLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fStringsAtom = new StringsLinkEditAtom<A>(*this));
			break;
		case Options::kObjectFile:
			fWriterSynthesizedAtoms.push_back(fMachHeaderAtom = new MachHeaderAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new SegmentLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fUUIDAtom = new UUIDLoadCommandAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new SymbolTableLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fHeaderPadding = new LoadCommandsPaddingAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fSectionRelocationsAtom = new SectionRelocationsLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fLocalRelocationsAtom = new LocalRelocationsLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fSymbolTableAtom = new SymbolTableLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fExternalRelocationsAtom = new ExternalRelocationsLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fIndirectTableAtom = new IndirectTableLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fStringsAtom = new StringsLinkEditAtom<A>(*this));
			break;
		case Options::kDyld:
			fWriterSynthesizedAtoms.push_back(new DsoHandleAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fMachHeaderAtom = new MachHeaderAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new SegmentLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new SymbolTableLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new DyldLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fUUIDAtom = new UUIDLoadCommandAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new ThreadsLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fHeaderPadding = new LoadCommandsPaddingAtom<A>(*this));
			if ( fOptions.needsUnwindInfoSection() )
				fWriterSynthesizedAtoms.push_back(fUnwindInfoAtom = new UnwindInfoAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fLocalRelocationsAtom = new LocalRelocationsLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fSymbolTableAtom = new SymbolTableLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fExternalRelocationsAtom = new ExternalRelocationsLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fIndirectTableAtom = new IndirectTableLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fStringsAtom = new StringsLinkEditAtom<A>(*this));
			break;
	}

	// add extra commmands
	bool hasReExports = false;
	uint32_t ordinal = 1;
	switch ( fOptions.outputKind() ) {
		case Options::kDynamicExecutable:
			if ( fOptions.makeEncryptable() ) {
				fEncryptionLoadCommand = new EncryptionLoadCommandsAtom<A>(*this);
				fWriterSynthesizedAtoms.push_back(fEncryptionLoadCommand);
			}
			// fall through
		case Options::kDynamicLibrary:
		case Options::kDynamicBundle:
			{
				// add dylib load command atoms for all dynamic libraries
				const unsigned int libCount = dynamicLibraries.size();
				for (unsigned int i=0; i < libCount; ++i) {
					ExecutableFile::DyLibUsed& dylibInfo = dynamicLibraries[i];
					//fprintf(stderr, "dynamicLibraries[%d]: reader=%p, %s, install=%s\n", i, dylibInfo.reader, dylibInfo.reader->getPath(), dylibInfo.reader->getInstallPath() );
					
					if ( dylibInfo.options.fReExport ) {
						hasReExports = true;
					}
					else {
						const char* parentUmbrella = dylibInfo.reader->parentUmbrella();
						if ( (parentUmbrella != NULL) && (fOptions.outputKind() == Options::kDynamicLibrary) ) {
							const char* thisIDLastSlash = strrchr(fOptions.installPath(), '/');
							if ( (thisIDLastSlash != NULL) && (strcmp(&thisIDLastSlash[1], parentUmbrella) == 0) )
								hasReExports = true;
						}
					}
				
					if ( dylibInfo.options.fWeakImport ) {
						fForcedWeakImportReaders.insert(dylibInfo.reader);
					}

					if ( dylibInfo.options.fBundleLoader ) {
						fLibraryToOrdinal[dylibInfo.reader] = EXECUTABLE_ORDINAL;
					}
					else {
						// see if a DylibLoadCommandsAtom has already been created for this install path
						bool newDylib = true;
						const char* dylibInstallPath = dylibInfo.reader->getInstallPath();
						for (unsigned int seenLib=0; seenLib < i; ++seenLib) {
							ExecutableFile::DyLibUsed& seenDylibInfo = dynamicLibraries[seenLib];
							if ( !seenDylibInfo.options.fBundleLoader ) {
								const char* seenDylibInstallPath = seenDylibInfo.reader->getInstallPath();
								if ( strcmp(seenDylibInstallPath, dylibInstallPath) == 0 ) {
									fLibraryToOrdinal[dylibInfo.reader] = fLibraryToOrdinal[seenDylibInfo.reader];
									fLibraryToLoadCommand[dylibInfo.reader] = fLibraryToLoadCommand[seenDylibInfo.reader]; 
									fLibraryAliases[dylibInfo.reader] = seenDylibInfo.reader;
									newDylib = false;
									break;
								}
							}
						}

						if ( newDylib ) {
							// assign new ordinal and check for other paired load commands
							fLibraryToOrdinal[dylibInfo.reader] = ordinal++;
							DylibLoadCommandsAtom<A>* dyliblc = new DylibLoadCommandsAtom<A>(*this, dylibInfo);
							fLibraryToLoadCommand[dylibInfo.reader] = dyliblc;
							fWriterSynthesizedAtoms.push_back(dyliblc);
							if ( dylibInfo.options.fReExport 
								&& (fOptions.macosxVersionMin() < ObjectFile::ReaderOptions::k10_5)
								&& (fOptions.outputKind() == Options::kDynamicLibrary) ) {
								// see if child has sub-framework that is this
								bool isSubFramework = false;
								const char* childInUmbrella = dylibInfo.reader->parentUmbrella();
								if ( childInUmbrella != NULL ) {
									const char* myLeaf = strrchr(fOptions.installPath(), '/');
									if ( myLeaf != NULL ) {
										if ( strcmp(childInUmbrella, &myLeaf[1]) == 0 )
											isSubFramework = true;
									}
								}
								// LC_SUB_FRAMEWORK is in child, so do nothing in parent 
								if ( ! isSubFramework ) {
									// this dylib also needs a sub_x load command
									bool isFrameworkReExport = false;
									const char* lastSlash = strrchr(dylibInstallPath, '/');
									if ( lastSlash != NULL ) {
										char frameworkName[strlen(lastSlash)+20];
										sprintf(frameworkName, "/%s.framework/", &lastSlash[1]);
										isFrameworkReExport = (strstr(dylibInstallPath, frameworkName) != NULL);
									}
									if ( isFrameworkReExport ) {
										// needs a LC_SUB_UMBRELLA command
										fWriterSynthesizedAtoms.push_back(new SubUmbrellaLoadCommandsAtom<A>(*this, &lastSlash[1]));
									}
									else {
										// needs a LC_SUB_LIBRARY command
										const char* nameStart = &lastSlash[1];
										if ( lastSlash == NULL )
											nameStart = dylibInstallPath;
										int len = strlen(nameStart);
										const char* dot = strchr(nameStart, '.');
										if ( dot != NULL )
											len = dot - nameStart;
										fWriterSynthesizedAtoms.push_back(new SubLibraryLoadCommandsAtom<A>(*this, nameStart, len));
									}
								}
							}
						}
					}
				}
				// add umbrella command if needed
				if ( fOptions.umbrellaName() != NULL ) {
					fWriterSynthesizedAtoms.push_back(new UmbrellaLoadCommandsAtom<A>(*this, fOptions.umbrellaName()));
				}
				// add allowable client commands if used
				std::vector<const char*>& allowableClients = fOptions.allowableClients();
				for (std::vector<const char*>::iterator it=allowableClients.begin(); it != allowableClients.end(); ++it)
					fWriterSynthesizedAtoms.push_back(new AllowableClientLoadCommandsAtom<A>(*this, *it));
			}
			break;
		case Options::kStaticExecutable:
		case Options::kObjectFile:
		case Options::kDyld:
		case Options::kPreload:
		case Options::kKextBundle:
			break;
	}
	fNoReExportedDylibs = !hasReExports;
	
	// add any rpath load commands
	for(std::vector<const char*>::const_iterator it=fOptions.rpaths().begin(); it != fOptions.rpaths().end(); ++it) {
		fWriterSynthesizedAtoms.push_back(new RPathLoadCommandsAtom<A>(*this, *it));
	}
	
	// set up fSlideable
	switch ( fOptions.outputKind() ) {
		case Options::kObjectFile:
		case Options::kStaticExecutable:
			fSlideable = false;
			break;
		case Options::kDynamicExecutable:
			fSlideable = fOptions.positionIndependentExecutable();
			break;
		case Options::kDyld:
		case Options::kDynamicLibrary:
		case Options::kDynamicBundle:
		case Options::kPreload:
		case Options::kKextBundle:
			fSlideable = true;
			break;
	}
	
	//fprintf(stderr, "ordinals table:\n");
	//for (std::map<class ObjectFile::Reader*, uint32_t>::iterator it = fLibraryToOrdinal.begin(); it != fLibraryToOrdinal.end(); ++it) {
	//	fprintf(stderr, "%d <== %s\n", it->second, it->first->getPath());
	//}
}

template <typename A>
Writer<A>::~Writer()
{
	if ( fFilePath != NULL )
		free((void*)fFilePath);
	if ( fSymbolTable != NULL )
		delete [] fSymbolTable;
}


// for ppc64, -mdynamic-no-pic only works in low 2GB, so we might need to split the zeropage into two segments
template <>bool Writer<ppc64>::mightNeedPadSegment() { return (fOptions.zeroPageSize() >= 0x80000000ULL); }
template <typename A> bool Writer<A>::mightNeedPadSegment() { return false; }


template <typename A>
ObjectFile::Atom* Writer<A>::getUndefinedProxyAtom(const char* name)
{
	if ( fOptions.outputKind() == Options::kKextBundle ) {
		return new UndefinedSymbolProxyAtom<A>(*this, name);
	}
	else if ( fOptions.outputKind() == Options::kObjectFile ) {
		// when doing -r -exported_symbols_list, don't create proxy for a symbol
		// that is supposed to be exported.  We want an error instead
		// <rdar://problem/5062685> ld does not report error when -r is used and exported symbols are not defined.
		if ( fOptions.hasExportMaskList() && fOptions.shouldExport(name) )
			return NULL;
		else
			return new UndefinedSymbolProxyAtom<A>(*this, name);
	}
	else if ( (fOptions.undefinedTreatment() != Options::kUndefinedError) || fOptions.allowedUndefined(name) )  
		return new UndefinedSymbolProxyAtom<A>(*this, name);
	else
		return NULL;
}

template <typename A>
uint8_t Writer<A>::ordinalForLibrary(ObjectFile::Reader* lib)
{
	// flat namespace images use zero for all ordinals
	if (  fOptions.nameSpace() != Options::kTwoLevelNameSpace )
		return 0;

	// is an UndefinedSymbolProxyAtom
	if ( lib == this )
		if ( fOptions.nameSpace() == Options::kTwoLevelNameSpace )
			return DYNAMIC_LOOKUP_ORDINAL;

	std::map<class ObjectFile::Reader*, uint32_t>::iterator pos = fLibraryToOrdinal.find(lib);
	if ( pos != fLibraryToOrdinal.end() )
		return pos->second;

	throw "can't find ordinal for imported symbol";
}

template <typename A>
bool Writer<A>::targetRequiresWeakBinding(const ObjectFile::Atom& target)
{
	switch ( target.getDefinitionKind() ) {
		case ObjectFile::Atom::kExternalWeakDefinition:
		case ObjectFile::Atom::kWeakDefinition:
			return true;
		case ObjectFile::Atom::kExternalDefinition:
		case ObjectFile::Atom::kAbsoluteSymbol:
		case ObjectFile::Atom::kRegularDefinition:
		case ObjectFile::Atom::kTentativeDefinition:
			break;
	}
	return false;
}

template <typename A>
int Writer<A>::compressedOrdinalForImortedAtom(ObjectFile::Atom* target)
{
	// flat namespace images use zero for all ordinals
	if (  fOptions.nameSpace() != Options::kTwoLevelNameSpace )
		return BIND_SPECIAL_DYLIB_FLAT_LOOKUP;

	// is an UndefinedSymbolProxyAtom
	ObjectFile::Reader* lib = target->getFile();
	if ( lib == this )
		if ( fOptions.nameSpace() == Options::kTwoLevelNameSpace )
			return BIND_SPECIAL_DYLIB_FLAT_LOOKUP;

	std::map<class ObjectFile::Reader*, uint32_t>::iterator pos;
	switch ( target->getDefinitionKind() ) {
		case ObjectFile::Atom::kExternalDefinition:
		case ObjectFile::Atom::kExternalWeakDefinition:
			pos = fLibraryToOrdinal.find(lib);
			if ( pos != fLibraryToOrdinal.end() ) {
				if ( pos->second == EXECUTABLE_ORDINAL )
					return BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE;
				else 
					return pos->second;
			}
			break;
		case ObjectFile::Atom::kWeakDefinition:
			throw "compressedOrdinalForImortedAtom() should not have been called on a weak definition";
		case ObjectFile::Atom::kAbsoluteSymbol:
		case ObjectFile::Atom::kRegularDefinition:
		case ObjectFile::Atom::kTentativeDefinition:
			return BIND_SPECIAL_DYLIB_SELF;
	}	

	throw "can't find ordinal for imported symbol";
}


template <typename A>
ObjectFile::Atom& Writer<A>::makeObjcInfoAtom(ObjectFile::Reader::ObjcConstraint objcContraint, bool objcReplacementClasses)
{
	return *(new ObjCInfoAtom<A>(*this, objcContraint, objcReplacementClasses));
}


template <typename A>
uint64_t Writer<A>::write(std::vector<class ObjectFile::Atom*>& atoms,
						  std::vector<class ObjectFile::Reader::Stab>& stabs,
						  class ObjectFile::Atom* entryPointAtom, 
						  class ObjectFile::Atom* dyldClassicHelperAtom,
						  class ObjectFile::Atom* dyldCompressedHelperAtom,
						  class ObjectFile::Atom* dyldLazyDylibHelperAtom,
						  bool createUUID, bool canScatter, ObjectFile::Reader::CpuConstraint cpuConstraint,
						  bool biggerThanTwoGigs, 
						  std::set<const class ObjectFile::Atom*>& atomsThatOverrideWeak,
						  bool hasExternalWeakDefinitions)
{
	fAllAtoms =  &atoms;
	fStabs =  &stabs;
	fEntryPoint = entryPointAtom;
	fDyldClassicHelperAtom = dyldClassicHelperAtom;
	fDyldCompressedHelperAtom = dyldCompressedHelperAtom;
	fDyldLazyDylibHelper = dyldLazyDylibHelperAtom;
	fCanScatter = canScatter;
	fCpuConstraint = cpuConstraint;
	fBiggerThanTwoGigs = biggerThanTwoGigs;
	fHasWeakExports = hasExternalWeakDefinitions; // dyld needs to search this image as if it had weak exports
	fRegularDefAtomsThatOverrideADylibsWeakDef = &atomsThatOverrideWeak;
	

	try {
		// Set for create UUID
		if (createUUID)
			fUUIDAtom->generate();

		// remove uneeded dylib load commands
		optimizeDylibReferences();

		// check for mdynamic-no-pic codegen
		scanForAbsoluteReferences();

		// create inter-library stubs
		synthesizeStubs();

		// create table of unwind info
		synthesizeUnwindInfoTable();

		// create SegmentInfo and SectionInfo objects and assign all atoms to a section
		partitionIntoSections();

		// segment load command can now be sized and padding can be set
		adjustLoadCommandsAndPadding();

		// assign each section a file offset
		assignFileOffsets();

		// if need to add branch islands, reassign file offsets
		if ( addBranchIslands() )
			assignFileOffsets();
	
		// now that addresses are assigned, create unwind info 
		if ( fUnwindInfoAtom != NULL ) {
			fUnwindInfoAtom->generate();
			// re-layout 
			adjustLoadCommandsAndPadding();
			assignFileOffsets();
		}

		// make spit-seg info now that all atoms exist
		createSplitSegContent();

		// build symbol table and relocations
		buildLinkEdit();

		// write map file if requested
		writeMap();

		// write everything
		return writeAtoms();
	} catch (...) {
		// clean up if any errors
		(void)unlink(fFilePath);
		throw;
	}
}

template <typename A>
void Writer<A>::buildLinkEdit()
{
	this->collectExportedAndImportedAndLocalAtoms();
	this->buildSymbolTable();
	this->buildFixups();
	this->adjustLinkEditSections();
}



template <typename A>
uint64_t Writer<A>::getAtomLoadAddress(const ObjectFile::Atom* atom)
{
	return atom->getAddress();
//	SectionInfo* info = (SectionInfo*)atom->getSection();
//	return info->getBaseAddress() + atom->getSectionOffset();
}

template <>
bool Writer<x86_64>::stringsNeedLabelsInObjects()
{
	return true;
}

template <typename A>
bool Writer<A>::stringsNeedLabelsInObjects()
{
	return false;
}

template <typename A>
const char* Writer<A>::symbolTableName(const ObjectFile::Atom* atom)
{
	static unsigned int counter = 0;
	const char* name;
	if ( stringsNeedLabelsInObjects() 
		&& (atom->getContentType() == ObjectFile::Atom::kCStringType) 
		&& (atom->getDefinitionKind() == ObjectFile::Atom::kWeakDefinition) )
		asprintf((char**)&name, "LC%u", counter++);
	else
		name = atom->getName();
	return name;
	return atom->getName();
}

template <typename A>
void Writer<A>::setExportNlist(const ObjectFile::Atom* atom, macho_nlist<P>* entry)
{
	// set n_strx
	entry->set_n_strx(this->fStringsAtom->add(this->symbolTableName(atom)));

	// set n_type
	if ( atom->getSymbolTableInclusion() == ObjectFile::Atom::kSymbolTableInAsAbsolute ) {
		entry->set_n_type(N_EXT | N_ABS);
	}
	else {
		entry->set_n_type(N_EXT | N_SECT);
		if ( (atom->getScope() == ObjectFile::Atom::scopeLinkageUnit) && (fOptions.outputKind() == Options::kObjectFile) ) {
			if ( fOptions.keepPrivateExterns() )
				entry->set_n_type(N_EXT | N_SECT | N_PEXT);
		}
	}
	
	// set n_sect (section number of implementation )
	uint8_t sectionIndex = atom->getSection()->getIndex();
	entry->set_n_sect(sectionIndex);

	// the __mh_execute_header is magic and must be an absolute symbol
	if ( (sectionIndex==0) 
		&& (fOptions.outputKind() == Options::kDynamicExecutable)
		&& (atom->getSymbolTableInclusion() == ObjectFile::Atom::kSymbolTableInAndNeverStrip ))
		entry->set_n_type(N_EXT | N_ABS);

	// set n_desc
	uint16_t desc = 0;
    if ( atom->isThumb() )
        desc |= N_ARM_THUMB_DEF;
    if ( atom->getSymbolTableInclusion() == ObjectFile::Atom::kSymbolTableInAndNeverStrip )
        desc |= REFERENCED_DYNAMICALLY;
    if ( atom->dontDeadStrip() && (fOptions.outputKind() == Options::kObjectFile) )
        desc |= N_NO_DEAD_STRIP;
    if ( atom->getDefinitionKind() == ObjectFile::Atom::kWeakDefinition ) {
        desc |= N_WEAK_DEF;
        fHasWeakExports = true;
    }
	entry->set_n_desc(desc);

	// set n_value ( address this symbol will be at if this executable is loaded at it preferred address )
	if ( atom->getDefinitionKind() == ObjectFile::Atom::kAbsoluteSymbol ) 
		entry->set_n_value(atom->getSectionOffset());
	else
		entry->set_n_value(this->getAtomLoadAddress(atom));
}

template <typename A>
void Writer<A>::setImportNlist(const ObjectFile::Atom* atom, macho_nlist<P>* entry)
{
	// set n_strx
	entry->set_n_strx(this->fStringsAtom->add(atom->getName()));

	// set n_type
	if ( fOptions.outputKind() == Options::kObjectFile ) {
		if ( (atom->getScope() == ObjectFile::Atom::scopeLinkageUnit) 
				&& (atom->getDefinitionKind() == ObjectFile::Atom::kTentativeDefinition) )
			entry->set_n_type(N_UNDF | N_EXT | N_PEXT);
		else 
			entry->set_n_type(N_UNDF | N_EXT);
	}
	else {
		if ( fOptions.prebind() )
			entry->set_n_type(N_PBUD | N_EXT);
		else 
			entry->set_n_type(N_UNDF | N_EXT);
	}

	// set n_sect
	entry->set_n_sect(0);

	uint16_t desc = 0;
	if ( fOptions.outputKind() != Options::kObjectFile ) {
		// set n_desc ( high byte is library ordinal, low byte is reference type )
		std::map<const ObjectFile::Atom*,ObjectFile::Atom*>::iterator pos = fStubsMap.find(atom);
		if ( pos != fStubsMap.end() || ( strncmp(atom->getName(), ".objc_class_name_", 17) == 0) )
			desc = REFERENCE_FLAG_UNDEFINED_LAZY;
		else
			desc = REFERENCE_FLAG_UNDEFINED_NON_LAZY;
		try {
			uint8_t ordinal = this->ordinalForLibrary(atom->getFile());
			//fprintf(stderr, "ordinal=%u from reader=%p for symbol=%s\n", ordinal, atom->getFile(), atom->getName());
			SET_LIBRARY_ORDINAL(desc, ordinal);
		}
		catch (const char* msg) {
			throwf("%s %s from %s", msg, atom->getDisplayName(), atom->getFile()->getPath());
		}
	}
	else if ( atom->getDefinitionKind() == ObjectFile::Atom::kTentativeDefinition ) {
		uint8_t align = atom->getAlignment().powerOf2;
		// always record custom alignment of common symbols to match what compiler does
		SET_COMM_ALIGN(desc, align);
	}
	if ( atom->isThumb() )
		desc |= N_ARM_THUMB_DEF;
	if ( atom->getSymbolTableInclusion() == ObjectFile::Atom::kSymbolTableInAndNeverStrip )
		desc |= REFERENCED_DYNAMICALLY;
	if ( ( fOptions.outputKind() != Options::kObjectFile) && (atom->getDefinitionKind() == ObjectFile::Atom::kExternalWeakDefinition) ) {
		desc |= N_REF_TO_WEAK;
		fReferencesWeakImports = true;
	}
	// set weak_import attribute
	if ( fWeakImportMap[atom] ) 
		desc |= N_WEAK_REF;
	entry->set_n_desc(desc);

	// set n_value, zero for import proxy and size for tentative definition
	entry->set_n_value(atom->getSize());
}


template <typename A>
void Writer<A>::setLocalNlist(const ObjectFile::Atom* atom, macho_nlist<P>* entry)
{
	// set n_strx
	const char* symbolName = this->symbolTableName(atom);
	char anonName[32];
	if ( (fOptions.outputKind() == Options::kObjectFile) && !fOptions.keepLocalSymbol(symbolName) ) {
		if ( stringsNeedLabelsInObjects() && (atom->getContentType() == ObjectFile::Atom::kCStringType) ) {
			// don't use 'l' labels for x86_64 strings
			// <rdar://problem/6605499> x86_64 obj-c runtime confused when static lib is stripped
		}
		else {
			sprintf(anonName, "l%u", fAnonNameIndex++);
			symbolName = anonName;
		}
	}
	entry->set_n_strx(this->fStringsAtom->add(symbolName));

	// set n_type
	uint8_t type = N_SECT;
	if ( atom->getDefinitionKind() == ObjectFile::Atom::kAbsoluteSymbol ) 
		type = N_ABS;
	if ( atom->getScope() == ObjectFile::Atom::scopeLinkageUnit )
		type |= N_PEXT;
	entry->set_n_type(type);

	// set n_sect (section number of implementation )
	uint8_t sectIndex = atom->getSection()->getIndex();
	if ( sectIndex == 0 ) {
		// see <mach-o/ldsyms.h> synthesized lable for mach_header needs special section number...
		if ( strcmp(atom->getSectionName(), "._mach_header") == 0 )
			sectIndex = 1;
	}
	entry->set_n_sect(sectIndex);

	// set n_desc
	uint16_t desc = 0;
    if ( atom->dontDeadStrip() && (fOptions.outputKind() == Options::kObjectFile) )
        desc |= N_NO_DEAD_STRIP;
	if ( atom->getDefinitionKind() == ObjectFile::Atom::kWeakDefinition )
		desc |= N_WEAK_DEF;
	if ( atom->isThumb() )
		desc |= N_ARM_THUMB_DEF;
	entry->set_n_desc(desc);

	// set n_value ( address this symbol will be at if this executable is loaded at it preferred address )
	if ( atom->getDefinitionKind() == ObjectFile::Atom::kAbsoluteSymbol ) 
		entry->set_n_value(atom->getSectionOffset());
	else
		entry->set_n_value(this->getAtomLoadAddress(atom));
}


template <typename A>
void Writer<A>::addLocalLabel(ObjectFile::Atom& atom, uint32_t offsetInAtom, const char* name)
{
	macho_nlist<P> entry;
	
	// set n_strx
	entry.set_n_strx(fStringsAtom->add(name));

	// set n_type
	entry.set_n_type(N_SECT);

	// set n_sect (section number of implementation )
	entry.set_n_sect(atom.getSection()->getIndex());

	// set n_desc
	entry.set_n_desc(0);

	// set n_value ( address this symbol will be at if this executable is loaded at it preferred address )
	entry.set_n_value(this->getAtomLoadAddress(&atom) + offsetInAtom);
	
	// add
	fLocalExtraLabels.push_back(entry);
}



template <typename A>
void Writer<A>::addGlobalLabel(ObjectFile::Atom& atom, uint32_t offsetInAtom, const char* name)
{
	macho_nlist<P> entry;
	
	// set n_strx
	entry.set_n_strx(fStringsAtom->add(name));

	// set n_type
	entry.set_n_type(N_SECT|N_EXT);

	// set n_sect (section number of implementation )
	entry.set_n_sect(atom.getSection()->getIndex());

	// set n_desc
	entry.set_n_desc(0);

	// set n_value ( address this symbol will be at if this executable is loaded at it preferred address )
	entry.set_n_value(this->getAtomLoadAddress(&atom) + offsetInAtom);
	
	// add
	fGlobalExtraLabels.push_back(entry);
}

template <typename A>
void Writer<A>::setNlistRange(std::vector<class ObjectFile::Atom*>& atoms, uint32_t startIndex, uint32_t count)
{
	macho_nlist<P>* entry = &fSymbolTable[startIndex];
	for (uint32_t i=0; i < count; ++i, ++entry) {
		ObjectFile::Atom* atom = atoms[i];
		if ( &atoms == &fExportedAtoms ) {
			this->setExportNlist(atom, entry);
		}
		else if ( &atoms == &fImportedAtoms ) {
			this->setImportNlist(atom, entry);
		}
		else {
			this->setLocalNlist(atom, entry);
		}
	}
}

template <typename A>
void Writer<A>::copyNlistRange(const std::vector<macho_nlist<P> >& entries, uint32_t startIndex)
{
	for ( typename std::vector<macho_nlist<P> >::const_iterator it = entries.begin(); it != entries.end(); ++it) 
		fSymbolTable[startIndex++] = *it;
}


template <typename A>
struct NListNameSorter
{
	NListNameSorter(StringsLinkEditAtom<A>* pool) : fStringPool(pool) {}
	
     bool operator()(const macho_nlist<typename A::P>& left, const macho_nlist<typename A::P>& right)
     {
          return (strcmp(fStringPool->stringForIndex(left.n_strx()), fStringPool->stringForIndex(right.n_strx())) < 0);
     }
private:
	StringsLinkEditAtom<A>*		fStringPool;
};


template <typename A>
void Writer<A>::buildSymbolTable()
{
	fSymbolTableStabsStartIndex		= 0;
	fSymbolTableStabsCount			= fStabs->size();
	fSymbolTableLocalStartIndex		= fSymbolTableStabsStartIndex + fSymbolTableStabsCount;
	fSymbolTableLocalCount			= fLocalSymbolAtoms.size() + fLocalExtraLabels.size();
	fSymbolTableExportStartIndex	= fSymbolTableLocalStartIndex + fSymbolTableLocalCount;
	fSymbolTableExportCount			= fExportedAtoms.size() + fGlobalExtraLabels.size();
	fSymbolTableImportStartIndex	= fSymbolTableExportStartIndex + fSymbolTableExportCount;
	fSymbolTableImportCount			= fImportedAtoms.size();

	// allocate symbol table
	fSymbolTableCount = fSymbolTableStabsCount + fSymbolTableLocalCount + fSymbolTableExportCount + fSymbolTableImportCount;
	fSymbolTable = new macho_nlist<P>[fSymbolTableCount];

	// fill in symbol table and string pool (do stabs last so strings are at end of pool)
	setNlistRange(fLocalSymbolAtoms, fSymbolTableLocalStartIndex,  fLocalSymbolAtoms.size());
	if ( fLocalExtraLabels.size() != 0 )
		copyNlistRange(fLocalExtraLabels, fSymbolTableLocalStartIndex+fLocalSymbolAtoms.size());
	setNlistRange(fExportedAtoms,    fSymbolTableExportStartIndex, fExportedAtoms.size());
	if ( fGlobalExtraLabels.size() != 0 ) {
		copyNlistRange(fGlobalExtraLabels, fSymbolTableExportStartIndex+fExportedAtoms.size());
		// re-sort combined range
		std::sort(  &fSymbolTable[fSymbolTableExportStartIndex], 
					&fSymbolTable[fSymbolTableExportStartIndex+fSymbolTableExportCount], 
					NListNameSorter<A>(fStringsAtom) );
	}
	setNlistRange(fImportedAtoms,    fSymbolTableImportStartIndex, fSymbolTableImportCount);
	addStabs(fSymbolTableStabsStartIndex);
	
	// set up module table
	if ( fModuleInfoAtom != NULL )
		fModuleInfoAtom->setName();
		
	// create atom to symbol index map
	// imports
	int i = 0;
	for(std::vector<ObjectFile::Atom*>::iterator it=fImportedAtoms.begin(); it != fImportedAtoms.end(); ++it) {
		fAtomToSymbolIndex[*it] = i + fSymbolTableImportStartIndex;
		++i;
	}
	// locals
	i = 0;
	for(std::vector<ObjectFile::Atom*>::iterator it=fLocalSymbolAtoms.begin(); it != fLocalSymbolAtoms.end(); ++it) {
		fAtomToSymbolIndex[*it] = i + fSymbolTableLocalStartIndex;
		++i;
	}
	// exports
	i = 0;
	for(std::vector<ObjectFile::Atom*>::iterator it=fExportedAtoms.begin(); it != fExportedAtoms.end(); ++it) {
		fAtomToSymbolIndex[*it] = i + fSymbolTableExportStartIndex;
		++i;
	}
	
}



template <typename A>
bool Writer<A>::shouldExport(const ObjectFile::Atom& atom) const
{
	switch ( atom.getSymbolTableInclusion() ) {
		case ObjectFile::Atom::kSymbolTableNotIn:
			return false;
		case ObjectFile::Atom::kSymbolTableInAndNeverStrip:
			return true;
		case ObjectFile::Atom::kSymbolTableInAsAbsolute:
		case ObjectFile::Atom::kSymbolTableIn:
			switch ( atom.getScope() ) {
				case ObjectFile::Atom::scopeGlobal:
					return true;
				case ObjectFile::Atom::scopeLinkageUnit:
					return ( (fOptions.outputKind() == Options::kObjectFile) && fOptions.keepPrivateExterns() );
				default:
					return false;
			}
			break;
	}
	return false;
}

template <typename A>
void Writer<A>::collectExportedAndImportedAndLocalAtoms()
{
	const int atomCount = fAllAtoms->size();
	// guess at sizes of each bucket to minimize re-allocations
	fImportedAtoms.reserve(100);
	fExportedAtoms.reserve(atomCount/2);
	fLocalSymbolAtoms.reserve(atomCount);
	for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
		ObjectFile::Atom* atom = *it;
		// only named atoms go in symbol table
		if ( atom->getName() != NULL ) {
			// put atom into correct bucket: imports, exports, locals
			//fprintf(stderr, "collectExportedAndImportedAndLocalAtoms() name=%s\n", atom->getDisplayName());
			switch ( atom->getDefinitionKind() ) {
				case ObjectFile::Atom::kExternalDefinition:
				case ObjectFile::Atom::kExternalWeakDefinition:
					fImportedAtoms.push_back(atom);
					break;
				case ObjectFile::Atom::kTentativeDefinition:
					if ( (fOptions.outputKind() == Options::kObjectFile) && !fOptions.readerOptions().fMakeTentativeDefinitionsReal ) {
						fImportedAtoms.push_back(atom);
						break;
					}
					// else fall into
				case ObjectFile::Atom::kWeakDefinition:
					if ( stringsNeedLabelsInObjects() 
							&& (fOptions.outputKind() == Options::kObjectFile)	
							&& (atom->getSymbolTableInclusion() == ObjectFile::Atom::kSymbolTableIn) 
							&& (atom->getScope() == ObjectFile::Atom::scopeLinkageUnit) 
							&& (atom->getContentType() == ObjectFile::Atom::kCStringType) ) {
						fLocalSymbolAtoms.push_back(atom);
						break;
					}
					// else fall into
				case ObjectFile::Atom::kRegularDefinition:
				case ObjectFile::Atom::kAbsoluteSymbol:
					if ( this->shouldExport(*atom) )
						fExportedAtoms.push_back(atom);
					else if ( (atom->getSymbolTableInclusion() != ObjectFile::Atom::kSymbolTableNotIn)
						&& ((fOptions.outputKind() == Options::kObjectFile) || fOptions.keepLocalSymbol(atom->getName())) )
						fLocalSymbolAtoms.push_back(atom);
					break;
			}
		}
		// when geneating a .o file, dtrace static probes become local labels
		if ( (fOptions.outputKind() == Options::kObjectFile) && !fOptions.readerOptions().fForStatic ) {
			std::vector<ObjectFile::Reference*>&  references = atom->getReferences();
			for (std::vector<ObjectFile::Reference*>::iterator rit=references.begin(); rit != references.end(); rit++) {
				ObjectFile::Reference* ref = *rit;
				if ( ref->getKind() == A::kDtraceProbe ) {
					// dtrace probe points to be add back into generated .o file
					this->addLocalLabel(*atom, ref->getFixUpOffset(), ref->getTargetName());
				}
			}
		}
		// when linking kernel, old style dtrace static probes become global labels
		else if ( fOptions.readerOptions().fForStatic ) {
			std::vector<ObjectFile::Reference*>&  references = atom->getReferences();
			for (std::vector<ObjectFile::Reference*>::iterator rit=references.begin(); rit != references.end(); rit++) {
				ObjectFile::Reference* ref = *rit;
				if ( ref->getKind() == A::kDtraceProbe ) {
					// dtrace probe points to be add back into generated .o file
					this->addGlobalLabel(*atom, ref->getFixUpOffset(), ref->getTargetName());
				}
			}
		}
	}

	// sort exported atoms by name
	std::sort(fExportedAtoms.begin(), fExportedAtoms.end(), AtomByNameSorter());
	// sort imported atoms by name (not required by runtime, but helps make generated files binary diffable)
	std::sort(fImportedAtoms.begin(), fImportedAtoms.end(), AtomByNameSorter());
}


template <typename A>
uint64_t Writer<A>::valueForStab(const ObjectFile::Reader::Stab& stab)
{
	switch ( stab.type ) {
		case N_FUN:
			if ( (stab.string == NULL) || (strlen(stab.string) == 0) ) {
				// end of function N_FUN has size
				return stab.atom->getSize();
			}
			else {
				// start of function N_FUN has address
				return getAtomLoadAddress(stab.atom);
			}
		case N_LBRAC:
		case N_RBRAC:
		case N_SLINE:
			if ( stab.atom == NULL )
				// some weird assembly files have slines not associated with a function
				return stab.value;
			else
				// all these stab types need their value changed from an offset in the atom to an address
				return getAtomLoadAddress(stab.atom) + stab.value;
		case N_STSYM:
		case N_LCSYM:
		case N_BNSYM:
			// all these need address of atom
			return getAtomLoadAddress(stab.atom);;
		case N_ENSYM:
			return stab.atom->getSize();
		case N_SO:
			if ( stab.atom == NULL ) {
				return 0;
			}
			else {
				if ( (stab.string == NULL) || (strlen(stab.string) == 0) ) {
					// end of translation unit N_SO has address of end of last atom
					return getAtomLoadAddress(stab.atom) + stab.atom->getSize();
				}
				else {
					// start of translation unit N_SO has address of end of first atom
					return getAtomLoadAddress(stab.atom);
				}
			}
			break;
		default:
			return stab.value;
	}
}

template <typename A>
uint32_t Writer<A>::stringOffsetForStab(const ObjectFile::Reader::Stab& stab)
{
	switch (stab.type) {
		case N_SO:
			if ( (stab.string == NULL) || stab.string[0] == '\0' ) {
				return this->fStringsAtom->emptyString();
				break;
			}
			// fall into uniquing case
		case N_SOL:
		case N_BINCL:
		case N_EXCL:
			return this->fStringsAtom->addUnique(stab.string);
			break;
		default:
			if ( stab.string == NULL )
				return 0;
			else if ( stab.string[0] == '\0' )
				return this->fStringsAtom->emptyString();
			else
				return this->fStringsAtom->add(stab.string);
	}
	return 0;
}

template <typename A>
uint8_t Writer<A>::sectionIndexForStab(const ObjectFile::Reader::Stab& stab)
{
	// in FUN stabs, n_sect field is 0 for start FUN and 1 for end FUN
	if ( stab.type == N_FUN )
		return stab.other;
	else if ( stab.atom != NULL ) 
		return stab.atom->getSection()->getIndex();
	else
		return stab.other;
}

template <typename A>
void Writer<A>::addStabs(uint32_t startIndex)
{
	macho_nlist<P>* entry = &fSymbolTable[startIndex];
	for(std::vector<ObjectFile::Reader::Stab>::iterator it = fStabs->begin(); it != fStabs->end(); ++it, ++entry) {
		const ObjectFile::Reader::Stab& stab = *it;
		entry->set_n_type(stab.type);
		entry->set_n_sect(sectionIndexForStab(stab));
		entry->set_n_desc(stab.desc);
		entry->set_n_value(valueForStab(stab));
		entry->set_n_strx(stringOffsetForStab(stab));
	}
}



template <typename A>
uint32_t Writer<A>::symbolIndex(ObjectFile::Atom& atom)
{
	std::map<ObjectFile::Atom*, uint32_t>::iterator pos = fAtomToSymbolIndex.find(&atom);
	if ( pos != fAtomToSymbolIndex.end() )
		return pos->second;
	throwf("atom not found in symbolIndex(%s) for %s", atom.getDisplayName(), atom.getFile()->getPath());
}


template <>
bool Writer<x86_64>::makesExternalRelocatableReference(ObjectFile::Atom& target) const
{
	switch ( target.getSymbolTableInclusion() )	 {
		case ObjectFile::Atom::kSymbolTableNotIn:
			return false;
		case ObjectFile::Atom::kSymbolTableInAsAbsolute:
		case ObjectFile::Atom::kSymbolTableIn:
		case ObjectFile::Atom::kSymbolTableInAndNeverStrip:
			return true;
	};
	return false;
}

template <typename A>
bool Writer<A>::makesExternalRelocatableReference(ObjectFile::Atom& target) const
{
	switch ( target.getDefinitionKind() ) {
		case ObjectFile::Atom::kRegularDefinition:
		case ObjectFile::Atom::kWeakDefinition:
		case ObjectFile::Atom::kAbsoluteSymbol:
			return false;
		case ObjectFile::Atom::kTentativeDefinition:
			if ( fOptions.readerOptions().fMakeTentativeDefinitionsReal )
				return false;
			else
				return (target.getScope() != ObjectFile::Atom::scopeTranslationUnit);
		case ObjectFile::Atom::kExternalDefinition:
		case ObjectFile::Atom::kExternalWeakDefinition:
			return shouldExport(target);
	}
	return false;
}

template <typename A>
void Writer<A>::buildFixups()
{
	if ( fOptions.outputKind() == Options::kObjectFile ) {
		this->buildObjectFileFixups();
	}
	else {
		if ( fOptions.keepRelocations() )
			this->buildObjectFileFixups();
		this->buildExecutableFixups();
	}
}

template <>
uint32_t Writer<x86_64>::addObjectRelocs(ObjectFile::Atom* atom, ObjectFile::Reference* ref)
{
	ObjectFile::Atom& target = ref->getTarget();
	bool external = this->makesExternalRelocatableReference(target);
	uint32_t symbolIndex = external ? this->symbolIndex(target) : target.getSection()->getIndex();
	uint32_t address = atom->getSectionOffset()+ref->getFixUpOffset();
	macho_relocation_info<P> reloc1;
	macho_relocation_info<P> reloc2;
	x86_64::ReferenceKinds kind = (x86_64::ReferenceKinds)ref->getKind();

	switch ( kind ) {
		case x86_64::kNoFixUp:
		case x86_64::kGOTNoFixUp:
		case x86_64::kFollowOn:
		case x86_64::kGroupSubordinate:
			return 0;

		case x86_64::kPointer:
		case x86_64::kPointerWeakImport:
			reloc1.set_r_address(address);
			reloc1.set_r_symbolnum(symbolIndex);
			reloc1.set_r_pcrel(false);
			reloc1.set_r_length(3);
			reloc1.set_r_extern(external);
			reloc1.set_r_type(X86_64_RELOC_UNSIGNED);
			fSectionRelocs.push_back(reloc1);
			return 1;

		case x86_64::kPointer32:
			reloc1.set_r_address(address);
			reloc1.set_r_symbolnum(symbolIndex);
			reloc1.set_r_pcrel(false);
			reloc1.set_r_length(2);
			reloc1.set_r_extern(external);
			reloc1.set_r_type(X86_64_RELOC_UNSIGNED);
			fSectionRelocs.push_back(reloc1);
			return 1;

		case x86_64::kPointerDiff32:
		case x86_64::kPointerDiff:	
			{
			ObjectFile::Atom& fromTarget = ref->getFromTarget();
			bool fromExternal = (fromTarget.getSymbolTableInclusion() != ObjectFile::Atom::kSymbolTableNotIn);
			uint32_t fromSymbolIndex = fromExternal ? this->symbolIndex(fromTarget) : fromTarget.getSection()->getIndex();
			reloc1.set_r_address(address);
			reloc1.set_r_symbolnum(symbolIndex);
			reloc1.set_r_pcrel(false);
			reloc1.set_r_length(kind==x86_64::kPointerDiff32 ? 2 : 3);
			reloc1.set_r_extern(external);
			reloc1.set_r_type(X86_64_RELOC_UNSIGNED);
			reloc2.set_r_address(address);
			reloc2.set_r_symbolnum(fromSymbolIndex);
			reloc2.set_r_pcrel(false);
			reloc2.set_r_length(kind==x86_64::kPointerDiff32 ? 2 : 3);
			reloc2.set_r_extern(fromExternal);
			reloc2.set_r_type(X86_64_RELOC_SUBTRACTOR);
			fSectionRelocs.push_back(reloc1);
			fSectionRelocs.push_back(reloc2);
			return 2;
			}

		case x86_64::kBranchPCRel32:
		case x86_64::kBranchPCRel32WeakImport:
		case x86_64::kDtraceProbeSite:
		case x86_64::kDtraceIsEnabledSite:
			reloc1.set_r_address(address);
			reloc1.set_r_symbolnum(symbolIndex);
			reloc1.set_r_pcrel(true);
			reloc1.set_r_length(2);
			reloc1.set_r_extern(external);
			reloc1.set_r_type(X86_64_RELOC_BRANCH);
			fSectionRelocs.push_back(reloc1);
			return 1;

		case x86_64::kPCRel32:
			reloc1.set_r_address(address);
			reloc1.set_r_symbolnum(symbolIndex);
			reloc1.set_r_pcrel(true);
			reloc1.set_r_length(2);
			reloc1.set_r_extern(external);
			reloc1.set_r_type(X86_64_RELOC_SIGNED);
			fSectionRelocs.push_back(reloc1);
			return 1;

		case x86_64::kPCRel32_1:
			reloc1.set_r_address(address);
			reloc1.set_r_symbolnum(symbolIndex);
			reloc1.set_r_pcrel(true);
			reloc1.set_r_length(2);
			reloc1.set_r_extern(external);
			reloc1.set_r_type(X86_64_RELOC_SIGNED_1);
			fSectionRelocs.push_back(reloc1);
			return 1;

		case x86_64::kPCRel32_2:
			reloc1.set_r_address(address);
			reloc1.set_r_symbolnum(symbolIndex);
			reloc1.set_r_pcrel(true);
			reloc1.set_r_length(2);
			reloc1.set_r_extern(external);
			reloc1.set_r_type(X86_64_RELOC_SIGNED_2);
			fSectionRelocs.push_back(reloc1);
			return 1;

		case x86_64::kPCRel32_4:
			reloc1.set_r_address(address);
			reloc1.set_r_symbolnum(symbolIndex);
			reloc1.set_r_pcrel(true);
			reloc1.set_r_length(2);
			reloc1.set_r_extern(external);
			reloc1.set_r_type(X86_64_RELOC_SIGNED_4);
			fSectionRelocs.push_back(reloc1);
			return 1;

		case x86_64::kBranchPCRel8:
			reloc1.set_r_address(address);
			reloc1.set_r_symbolnum(symbolIndex);
			reloc1.set_r_pcrel(true);
			reloc1.set_r_length(0);
			reloc1.set_r_extern(external);
			reloc1.set_r_type(X86_64_RELOC_BRANCH);
			fSectionRelocs.push_back(reloc1);
			return 1;

		case x86_64::kPCRel32GOT:
		case x86_64::kPCRel32GOTWeakImport:
			reloc1.set_r_address(address);
			reloc1.set_r_symbolnum(symbolIndex);
			reloc1.set_r_pcrel(true);
			reloc1.set_r_length(2);
			reloc1.set_r_extern(external);
			reloc1.set_r_type(X86_64_RELOC_GOT);
			fSectionRelocs.push_back(reloc1);
			return 1;

		case x86_64::kPCRel32GOTLoad:
		case x86_64::kPCRel32GOTLoadWeakImport:
			reloc1.set_r_address(address);
			reloc1.set_r_symbolnum(symbolIndex);
			reloc1.set_r_pcrel(true);
			reloc1.set_r_length(2);
			reloc1.set_r_extern(external);
			reloc1.set_r_type(X86_64_RELOC_GOT_LOAD);
			fSectionRelocs.push_back(reloc1);
			return 1;
			
		case x86_64::kPointerDiff24:
			throw "internal linker error, kPointerDiff24 can't be encoded into object files";

		case x86_64::kImageOffset32:
			throw "internal linker error, kImageOffset32 can't be encoded into object files";

		case x86_64::kSectionOffset24:
			throw "internal linker error, kSectionOffset24 can't be encoded into object files";

		case x86_64::kDtraceTypeReference:
		case x86_64::kDtraceProbe:
			// generates no relocs
			return 0;
	}
	return 0;
}


template <>
uint32_t Writer<x86>::addObjectRelocs(ObjectFile::Atom* atom, ObjectFile::Reference* ref)
{
	ObjectFile::Atom& target = ref->getTarget();
	bool isExtern = this->makesExternalRelocatableReference(target);
	uint32_t symbolIndex = 0;
	if ( isExtern )
		symbolIndex = this->symbolIndex(target);
	uint32_t sectionNum = target.getSection()->getIndex();
	uint32_t address = atom->getSectionOffset()+ref->getFixUpOffset();
	macho_relocation_info<P> reloc1;
	macho_relocation_info<P> reloc2;
	macho_scattered_relocation_info<P>* sreloc1 = (macho_scattered_relocation_info<P>*)&reloc1;
	macho_scattered_relocation_info<P>* sreloc2 = (macho_scattered_relocation_info<P>*)&reloc2;
	x86::ReferenceKinds kind = (x86::ReferenceKinds)ref->getKind();
	
	if ( !isExtern && (sectionNum == 0) && (target.getDefinitionKind() != ObjectFile::Atom::kAbsoluteSymbol) )
		warning("section index == 0 for %s (kind=%d, scope=%d, inclusion=%d) in %s",
		 target.getDisplayName(), target.getDefinitionKind(), target.getScope(), target.getSymbolTableInclusion(), target.getFile()->getPath());


	switch ( kind ) {
		case x86::kNoFixUp:
		case x86::kFollowOn:
		case x86::kGroupSubordinate:
			return 0;

		case x86::kPointer:
		case x86::kPointerWeakImport:
		case x86::kAbsolute32:
			if ( !isExtern && (ref->getTargetOffset() != 0) ) {
				// use scattered reloc is target offset is non-zero
				sreloc1->set_r_scattered(true);
				sreloc1->set_r_pcrel(false);
				sreloc1->set_r_length(2);
				sreloc1->set_r_type(GENERIC_RELOC_VANILLA);
				sreloc1->set_r_address(address);
				sreloc1->set_r_value(target.getAddress());
			}
			else {
				reloc1.set_r_address(address);
				reloc1.set_r_symbolnum(isExtern ? symbolIndex : sectionNum);
				reloc1.set_r_pcrel(false);
				reloc1.set_r_length(2);
				reloc1.set_r_extern(isExtern);
				reloc1.set_r_type(GENERIC_RELOC_VANILLA);
			}
			fSectionRelocs.push_back(reloc1);
			return 1;

		case x86::kPointerDiff16:
		case x86::kPointerDiff:
			{
				//pint_t fromAddr = ref->getFromTarget().getAddress() + ref->getFromTargetOffset();
				//fprintf(stderr, "addObjectRelocs(): refFromTarget=%s, refTarget=%s, refFromTargetAddr=0x%llX, refFromTargetOffset=0x%llX\n",
				//			ref->getFromTarget().getDisplayName(), ref->getTarget().getDisplayName(), 
				//			ref->getFromTarget().getAddress(), ref->getFromTargetOffset());
				sreloc1->set_r_scattered(true);
				sreloc1->set_r_pcrel(false);
				sreloc1->set_r_length( (kind==x86::kPointerDiff) ? 2 : 1 );
				if ( ref->getTarget().getScope() == ObjectFile::Atom::scopeTranslationUnit )
					sreloc1->set_r_type(GENERIC_RELOC_LOCAL_SECTDIFF);
				else
					sreloc1->set_r_type(GENERIC_RELOC_SECTDIFF);
				sreloc1->set_r_address(address);
				sreloc1->set_r_value(target.getAddress());
				
				sreloc2->set_r_scattered(true);
				sreloc2->set_r_pcrel(false);
				sreloc2->set_r_length( (kind==x86::kPointerDiff) ? 2 : 1 );
				sreloc2->set_r_type(GENERIC_RELOC_PAIR);
				sreloc2->set_r_address(0);
				if ( &ref->getFromTarget() == atom )
					sreloc2->set_r_value(ref->getFromTarget().getAddress()+ref->getFromTargetOffset());
				else
					sreloc2->set_r_value(ref->getFromTarget().getAddress());
				fSectionRelocs.push_back(reloc2);
				fSectionRelocs.push_back(reloc1);
				return 2;
			}

		case x86::kPCRel32WeakImport:
		case x86::kPCRel32:
		case x86::kPCRel16:
		case x86::kPCRel8:
		case x86::kDtraceProbeSite:
		case x86::kDtraceIsEnabledSite:
			if ( !isExtern && (ref->getTargetOffset() != 0) ) {
				// use scattered reloc is target offset is non-zero
				sreloc1->set_r_scattered(true);
				sreloc1->set_r_pcrel(true);
				sreloc1->set_r_length( (kind==x86::kPCRel8) ? 0 : ((kind==x86::kPCRel16) ? 1 : 2) );
				sreloc1->set_r_type(GENERIC_RELOC_VANILLA);
				sreloc1->set_r_address(address);
				sreloc1->set_r_value(target.getAddress());
			}
			else {
				reloc1.set_r_address(address);
				reloc1.set_r_symbolnum(isExtern ? symbolIndex : sectionNum);
				reloc1.set_r_pcrel(true);
				reloc1.set_r_length( (kind==x86::kPCRel8) ? 0 : ((kind==x86::kPCRel16) ? 1 : 2) );
				reloc1.set_r_extern(isExtern);
				reloc1.set_r_type(GENERIC_RELOC_VANILLA);
			}
			fSectionRelocs.push_back(reloc1);
			return 1;
			
		case x86::kPointerDiff24:
			throw "internal linker error, kPointerDiff24 can't be encoded into object files";

		case x86::kImageOffset32:
			throw "internal linker error, kImageOffset32 can't be encoded into object files";
			
		case x86::kSectionOffset24:
			throw "internal linker error, kSectionOffset24 can't be encoded into object files";

		case x86::kDtraceTypeReference:
		case x86::kDtraceProbe:
			// generates no relocs
			return 0;

	}
	return 0;
}

template <>
uint32_t Writer<arm>::addObjectRelocs(ObjectFile::Atom* atom, ObjectFile::Reference* ref)
{
	ObjectFile::Atom& target = ref->getTarget();
	bool isExtern = this->makesExternalRelocatableReference(target);
	uint32_t symbolIndex = 0;
	if ( isExtern )
		symbolIndex = this->symbolIndex(target);
	uint32_t sectionNum = target.getSection()->getIndex();
	uint32_t address = atom->getSectionOffset()+ref->getFixUpOffset();
	macho_relocation_info<P> reloc1;
	macho_relocation_info<P> reloc2;
	macho_scattered_relocation_info<P>* sreloc1 = (macho_scattered_relocation_info<P>*)&reloc1;
	macho_scattered_relocation_info<P>* sreloc2 = (macho_scattered_relocation_info<P>*)&reloc2;
	arm::ReferenceKinds kind = (arm::ReferenceKinds)ref->getKind();
	
	if ( !isExtern && (sectionNum == 0) && (target.getDefinitionKind() != ObjectFile::Atom::kAbsoluteSymbol) )
		warning("section index == 0 for %s (kind=%d, scope=%d, inclusion=%d) in %s",
		 target.getDisplayName(), target.getDefinitionKind(), target.getScope(), target.getSymbolTableInclusion(), target.getFile()->getPath());


	switch ( kind ) {
		case arm::kNoFixUp:
		case arm::kFollowOn:
		case arm::kGroupSubordinate:
			return 0;

		case arm::kPointer:
		case arm::kReadOnlyPointer:
		case arm::kPointerWeakImport:
			if ( !isExtern && (ref->getTargetOffset() != 0) ) {
				// use scattered reloc is target offset is non-zero
				sreloc1->set_r_scattered(true);
				sreloc1->set_r_pcrel(false);
				sreloc1->set_r_length(2);
				sreloc1->set_r_type(ARM_RELOC_VANILLA);
				sreloc1->set_r_address(address);
				sreloc1->set_r_value(target.getAddress());
			}
			else {
				reloc1.set_r_address(address);
				reloc1.set_r_symbolnum(isExtern ? symbolIndex : sectionNum);
				reloc1.set_r_pcrel(false);
				reloc1.set_r_length(2);
				reloc1.set_r_extern(isExtern);
				reloc1.set_r_type(ARM_RELOC_VANILLA);
			}
			fSectionRelocs.push_back(reloc1);
			return 1;

		case arm::kPointerDiff:
			{
				sreloc1->set_r_scattered(true);
				sreloc1->set_r_pcrel(false);
				sreloc1->set_r_length(2);
				if ( ref->getTarget().getScope() == ObjectFile::Atom::scopeTranslationUnit )
					sreloc1->set_r_type(ARM_RELOC_LOCAL_SECTDIFF);
				else
					sreloc1->set_r_type(ARM_RELOC_SECTDIFF);
				sreloc1->set_r_address(address);
				sreloc1->set_r_value(target.getAddress());
				sreloc2->set_r_scattered(true);
				sreloc2->set_r_pcrel(false);
				sreloc2->set_r_length(2);
				sreloc2->set_r_type(ARM_RELOC_PAIR);
				sreloc2->set_r_address(0);
				if ( &ref->getFromTarget() == atom )
					sreloc2->set_r_value(ref->getFromTarget().getAddress()+ref->getFromTargetOffset());
				else
					sreloc2->set_r_value(ref->getFromTarget().getAddress());
				fSectionRelocs.push_back(reloc2);
				fSectionRelocs.push_back(reloc1);
				return 2;
			}

		case arm::kBranch24WeakImport:
		case arm::kBranch24:
		case arm::kDtraceProbeSite:
		case arm::kDtraceIsEnabledSite:
			if ( !isExtern && (ref->getTargetOffset() != 0) ) {
				// use scattered reloc is target offset is non-zero
				sreloc1->set_r_scattered(true);
				sreloc1->set_r_pcrel(true);
				sreloc1->set_r_length(2);
				sreloc1->set_r_type(ARM_RELOC_BR24);
				sreloc1->set_r_address(address);
				sreloc1->set_r_value(target.getAddress());
			}
			else {
				reloc1.set_r_address(address);
				reloc1.set_r_symbolnum(isExtern ? symbolIndex : sectionNum);
				reloc1.set_r_pcrel(true);
				reloc1.set_r_length(2);
				reloc1.set_r_extern(isExtern);
				reloc1.set_r_type(ARM_RELOC_BR24);
			}
			fSectionRelocs.push_back(reloc1);
			return 1;
			
		case arm::kThumbBranch22WeakImport:
		case arm::kThumbBranch22:
			if ( !isExtern && (ref->getTargetOffset() != 0) ) {
				// use scattered reloc if target offset is non-zero
				sreloc1->set_r_scattered(true);
				sreloc1->set_r_pcrel(true);
				sreloc1->set_r_length(2);
				sreloc1->set_r_type(ARM_THUMB_RELOC_BR22);
				sreloc1->set_r_address(address);
				sreloc1->set_r_value(target.getAddress());
			}
			else {
				reloc1.set_r_address(address);
				reloc1.set_r_symbolnum(isExtern ? symbolIndex : sectionNum);
				reloc1.set_r_pcrel(true);
				reloc1.set_r_length(2);
				reloc1.set_r_extern(isExtern);
				reloc1.set_r_type(ARM_THUMB_RELOC_BR22);
			}
			fSectionRelocs.push_back(reloc1);
			return 1;

		case arm::kDtraceTypeReference:
		case arm::kDtraceProbe:
			// generates no relocs
			return 0;

	}
	return 0;
}

template <> uint64_t    Writer<ppc>::maxAddress() { return 0xFFFFFFFFULL; }
template <> uint64_t  Writer<ppc64>::maxAddress() { return 0xFFFFFFFFFFFFFFFFULL; }
template <> uint64_t    Writer<x86>::maxAddress() { return 0xFFFFFFFFULL; }
template <> uint64_t Writer<x86_64>::maxAddress() { return 0xFFFFFFFFFFFFFFFFULL; }
template <> uint64_t    Writer<arm>::maxAddress() { return 0xFFFFFFFFULL; }

template <>
uint8_t Writer<ppc>::getRelocPointerSize()
{
	return 2;
}

template <>
uint8_t Writer<ppc64>::getRelocPointerSize()
{
	return 3;
}

template <>
uint32_t Writer<ppc>::addObjectRelocs(ObjectFile::Atom* atom, ObjectFile::Reference* ref)
{
	return addObjectRelocs_powerpc(atom, ref);
}

template <>
uint32_t Writer<ppc64>::addObjectRelocs(ObjectFile::Atom* atom, ObjectFile::Reference* ref)
{
	return addObjectRelocs_powerpc(atom, ref);
}

//
// addObjectRelocs<ppc> and addObjectRelocs<ppc64> are almost exactly the same, so
// they use a common addObjectRelocs_powerpc() method.
//
template <typename A>
uint32_t Writer<A>::addObjectRelocs_powerpc(ObjectFile::Atom* atom, ObjectFile::Reference* ref)
{
	ObjectFile::Atom& target = ref->getTarget();
	bool isExtern = this->makesExternalRelocatableReference(target);	
	uint32_t symbolIndex = 0;
	if ( isExtern )
		symbolIndex = this->symbolIndex(target);
	uint32_t sectionNum = target.getSection()->getIndex();
	uint32_t address = atom->getSectionOffset()+ref->getFixUpOffset();
	macho_relocation_info<P> reloc1;
	macho_relocation_info<P> reloc2;
	macho_scattered_relocation_info<P>* sreloc1 = (macho_scattered_relocation_info<P>*)&reloc1;
	macho_scattered_relocation_info<P>* sreloc2 = (macho_scattered_relocation_info<P>*)&reloc2;
	typename A::ReferenceKinds kind = (typename A::ReferenceKinds)ref->getKind();

	switch ( kind ) {
		case A::kNoFixUp:
		case A::kFollowOn:
		case A::kGroupSubordinate:
			return 0;

		case A::kPointer:
		case A::kPointerWeakImport:
			if ( !isExtern && (ref->getTargetOffset() >= target.getSize()) ) {
				// use scattered reloc is target offset is outside target
				sreloc1->set_r_scattered(true);
				sreloc1->set_r_pcrel(false);
				sreloc1->set_r_length(getRelocPointerSize());
				sreloc1->set_r_type(GENERIC_RELOC_VANILLA);
				sreloc1->set_r_address(address);
				sreloc1->set_r_value(target.getAddress());
			}
			else {
				reloc1.set_r_address(address);
				if ( isExtern )
					reloc1.set_r_symbolnum(symbolIndex);
				else
					reloc1.set_r_symbolnum(sectionNum);
				reloc1.set_r_pcrel(false);
				reloc1.set_r_length(getRelocPointerSize());
				reloc1.set_r_extern(isExtern);
				reloc1.set_r_type(GENERIC_RELOC_VANILLA);
			}
			fSectionRelocs.push_back(reloc1);
			return 1;

		case A::kPointerDiff16:
		case A::kPointerDiff32:
		case A::kPointerDiff64:
			{
				sreloc1->set_r_scattered(true);
				sreloc1->set_r_pcrel(false);
				sreloc1->set_r_length( (kind == A::kPointerDiff32) ? 2 : ((kind == A::kPointerDiff64) ? 3 : 1));
				if ( ref->getTarget().getScope() == ObjectFile::Atom::scopeTranslationUnit )
					sreloc1->set_r_type(PPC_RELOC_LOCAL_SECTDIFF);
				else
					sreloc1->set_r_type(PPC_RELOC_SECTDIFF);
				sreloc1->set_r_address(address);
				sreloc1->set_r_value(target.getAddress()); 
				sreloc2->set_r_scattered(true);
				sreloc2->set_r_pcrel(false);
				sreloc2->set_r_length(sreloc1->r_length());
				sreloc2->set_r_type(PPC_RELOC_PAIR);
				sreloc2->set_r_address(0);
				sreloc2->set_r_value(ref->getFromTarget().getAddress()+ref->getFromTargetOffset()); 
				fSectionRelocs.push_back(reloc2);
				fSectionRelocs.push_back(reloc1);
				return 2;
			}

		case A::kBranch24WeakImport:
		case A::kBranch24:
		case A::kDtraceProbeSite:
		case A::kDtraceIsEnabledSite:
			if ( (ref->getTargetOffset() == 0) || isExtern ) {
				reloc1.set_r_address(address);
				if ( isExtern )
					reloc1.set_r_symbolnum(symbolIndex);
				else
					reloc1.set_r_symbolnum(sectionNum);
				reloc1.set_r_pcrel(true);
				reloc1.set_r_length(2);
				reloc1.set_r_type(PPC_RELOC_BR24);
				reloc1.set_r_extern(isExtern);
			}
			else {
				sreloc1->set_r_scattered(true);
				sreloc1->set_r_pcrel(true);
				sreloc1->set_r_length(2);
				sreloc1->set_r_type(PPC_RELOC_BR24);
				sreloc1->set_r_address(address);
				sreloc1->set_r_value(target.getAddress());
			}
			fSectionRelocs.push_back(reloc1);
			return 1;

		case A::kBranch14:
			if ( (ref->getTargetOffset() == 0) || isExtern ) {
				reloc1.set_r_address(address);
				if ( isExtern )
					reloc1.set_r_symbolnum(symbolIndex);
				else
					reloc1.set_r_symbolnum(sectionNum);
				reloc1.set_r_pcrel(true);
				reloc1.set_r_length(2);
				reloc1.set_r_type(PPC_RELOC_BR14);
				reloc1.set_r_extern(isExtern);
			}
			else {
				sreloc1->set_r_scattered(true);
				sreloc1->set_r_pcrel(true);
				sreloc1->set_r_length(2);
				sreloc1->set_r_type(PPC_RELOC_BR14);
				sreloc1->set_r_address(address);
				sreloc1->set_r_value(target.getAddress());
			}
			fSectionRelocs.push_back(reloc1);
			return 1;

		case A::kPICBaseLow16:
		case A::kPICBaseLow14:
			{
				pint_t fromAddr = atom->getAddress() + ref->getFromTargetOffset();
				pint_t toAddr = target.getAddress() + ref->getTargetOffset();
				sreloc1->set_r_scattered(true);
				sreloc1->set_r_pcrel(false);
				sreloc1->set_r_length(2);
				sreloc1->set_r_type(kind == A::kPICBaseLow16 ? PPC_RELOC_LO16_SECTDIFF : PPC_RELOC_LO14_SECTDIFF);
				sreloc1->set_r_address(address);
				sreloc1->set_r_value(target.getAddress());
				sreloc2->set_r_scattered(true);
				sreloc2->set_r_pcrel(false);
				sreloc2->set_r_length(2);
				sreloc2->set_r_type(PPC_RELOC_PAIR);
				sreloc2->set_r_address(((toAddr-fromAddr) >> 16) & 0xFFFF);
				sreloc2->set_r_value(fromAddr);
				fSectionRelocs.push_back(reloc2);
				fSectionRelocs.push_back(reloc1);
				return 2;
			}

		case A::kPICBaseHigh16:
			{
				pint_t fromAddr = atom->getAddress() + ref->getFromTargetOffset();
				pint_t toAddr = target.getAddress() + ref->getTargetOffset();
				sreloc1->set_r_scattered(true);
				sreloc1->set_r_pcrel(false);
				sreloc1->set_r_length(2);
				sreloc1->set_r_type(PPC_RELOC_HA16_SECTDIFF);
				sreloc1->set_r_address(address);
				sreloc1->set_r_value(target.getAddress());
				sreloc2->set_r_scattered(true);
				sreloc2->set_r_pcrel(false);
				sreloc2->set_r_length(2);
				sreloc2->set_r_type(PPC_RELOC_PAIR);
				sreloc2->set_r_address((toAddr-fromAddr) & 0xFFFF);
				sreloc2->set_r_value(fromAddr);
				fSectionRelocs.push_back(reloc2);
				fSectionRelocs.push_back(reloc1);
				return 2;
			}

		case A::kAbsLow14:
		case A::kAbsLow16:
			{
				pint_t toAddr = target.getAddress() + ref->getTargetOffset();
				if ( (ref->getTargetOffset() == 0) || isExtern ) {
					reloc1.set_r_address(address);
					if ( isExtern )
						reloc1.set_r_symbolnum(symbolIndex);
					else
						reloc1.set_r_symbolnum(sectionNum);
					reloc1.set_r_pcrel(false);
					reloc1.set_r_length(2);
					reloc1.set_r_extern(isExtern);
					reloc1.set_r_type(kind==A::kAbsLow16 ? PPC_RELOC_LO16 : PPC_RELOC_LO14);
				}
				else {
					sreloc1->set_r_scattered(true);
					sreloc1->set_r_pcrel(false);
					sreloc1->set_r_length(2);
					sreloc1->set_r_type(kind==A::kAbsLow16 ? PPC_RELOC_LO16 : PPC_RELOC_LO14);
					sreloc1->set_r_address(address);
					sreloc1->set_r_value(target.getAddress());
				}
				if ( isExtern )
					reloc2.set_r_address(ref->getTargetOffset() >> 16);
				else
					reloc2.set_r_address(toAddr >> 16);
				reloc2.set_r_symbolnum(0);
				reloc2.set_r_pcrel(false);
				reloc2.set_r_length(2);
				reloc2.set_r_extern(false);
				reloc2.set_r_type(PPC_RELOC_PAIR);
				fSectionRelocs.push_back(reloc2);
				fSectionRelocs.push_back(reloc1);
				return 2;
			}

		case A::kAbsHigh16:
			{
				pint_t toAddr = target.getAddress() + ref->getTargetOffset();
				if ( (ref->getTargetOffset() == 0) || isExtern ) {
					reloc1.set_r_address(address);
					if ( isExtern )
						reloc1.set_r_symbolnum(symbolIndex);
					else
						reloc1.set_r_symbolnum(sectionNum);
					reloc1.set_r_pcrel(false);
					reloc1.set_r_length(2);
					reloc1.set_r_extern(isExtern);
					reloc1.set_r_type(PPC_RELOC_HI16);
				}
				else {
					sreloc1->set_r_scattered(true);
					sreloc1->set_r_pcrel(false);
					sreloc1->set_r_length(2);
					sreloc1->set_r_type(PPC_RELOC_HI16);
					sreloc1->set_r_address(address);
					sreloc1->set_r_value(target.getAddress());
				}
				if ( isExtern )
					reloc2.set_r_address(ref->getTargetOffset() & 0xFFFF);
				else
					reloc2.set_r_address(toAddr & 0xFFFF);
				reloc2.set_r_symbolnum(0);
				reloc2.set_r_pcrel(false);
				reloc2.set_r_length(2);
				reloc2.set_r_extern(false);
				reloc2.set_r_type(PPC_RELOC_PAIR);
				fSectionRelocs.push_back(reloc2);
				fSectionRelocs.push_back(reloc1);
				return 2;
			}

		case A::kAbsHigh16AddLow:
			{
				pint_t toAddr = target.getAddress() + ref->getTargetOffset();
				uint32_t overflow = 0;
				if ( (toAddr & 0x00008000) != 0 )
					overflow = 0x10000;
				if ( (ref->getTargetOffset() == 0) || isExtern ) {
					reloc1.set_r_address(address);
					if ( isExtern )
						reloc1.set_r_symbolnum(symbolIndex);
					else
						reloc1.set_r_symbolnum(sectionNum);
					reloc1.set_r_pcrel(false);
					reloc1.set_r_length(2);
					reloc1.set_r_extern(isExtern);
					reloc1.set_r_type(PPC_RELOC_HA16);
				}
				else {
					sreloc1->set_r_scattered(true);
					sreloc1->set_r_pcrel(false);
					sreloc1->set_r_length(2);
					sreloc1->set_r_type(PPC_RELOC_HA16);
					sreloc1->set_r_address(address);
					sreloc1->set_r_value(target.getAddress());
				}
				if ( isExtern )
					reloc2.set_r_address(ref->getTargetOffset() & 0xFFFF);
				else
					reloc2.set_r_address(toAddr & 0xFFFF);
				reloc2.set_r_symbolnum(0);
				reloc2.set_r_pcrel(false);
				reloc2.set_r_length(2);
				reloc2.set_r_extern(false);
				reloc2.set_r_type(PPC_RELOC_PAIR);
				fSectionRelocs.push_back(reloc2);
				fSectionRelocs.push_back(reloc1);
				return 2;
			}

		case A::kDtraceTypeReference:
		case A::kDtraceProbe:
			// generates no relocs
			return 0;
	}
	return 0;
}



//
// There are cases when an entry in the indirect symbol table is the magic value
// INDIRECT_SYMBOL_LOCAL instead of being a symbol index.  When that happens
// the content of the corresponding part of the __nl_symbol_pointer section
// must also change. 
//
template <typename A>
bool Writer<A>::indirectSymbolInRelocatableIsLocal(const ObjectFile::Reference* ref) const
{
	// cannot use INDIRECT_SYMBOL_LOCAL to tentative definitions in object files
	// because tentative defs don't have addresses
	if ( ref->getTarget().getDefinitionKind() == ObjectFile::Atom::kTentativeDefinition )
		return false;
		
	// must use INDIRECT_SYMBOL_LOCAL if there is an addend 
	if ( ref->getTargetOffset() != 0 )
		return true;
	
	// don't use INDIRECT_SYMBOL_LOCAL for external symbols
	return ! this->shouldExport(ref->getTarget());
}


template <typename A>
void Writer<A>::buildObjectFileFixups()
{
	uint32_t relocIndex = 0;
	std::vector<SegmentInfo*>& segmentInfos = fSegmentInfos;
	const int segCount = segmentInfos.size();
	for(int i=0; i < segCount; ++i) {
		SegmentInfo* curSegment = segmentInfos[i];
		std::vector<SectionInfo*>& sectionInfos = curSegment->fSections;
		const int sectionCount = sectionInfos.size();
		for(int j=0; j < sectionCount; ++j) {
			SectionInfo* curSection = sectionInfos[j];
			//fprintf(stderr, "buildObjectFileFixups(): starting section %s\n", curSection->fSectionName);
			std::vector<ObjectFile::Atom*>& sectionAtoms = curSection->fAtoms;
			if ( ! curSection->fAllZeroFill ) {
				if ( curSection->fAllNonLazyPointers || curSection->fAllLazyPointers
							|| curSection->fAllLazyDylibPointers || curSection->fAllStubs )
					curSection->fIndirectSymbolOffset = fIndirectTableAtom->fTable.size();
				curSection->fRelocOffset = relocIndex;
				const int atomCount = sectionAtoms.size();
				for (int k=0; k < atomCount; ++k) {
					ObjectFile::Atom* atom = sectionAtoms[k];
					//fprintf(stderr, "buildObjectFileFixups(): atom %s has %lu references\n", atom->getDisplayName(), atom->getReferences().size());
					std::vector<ObjectFile::Reference*>& refs = atom->getReferences();
					const int refCount = refs.size();
					for (int l=0; l < refCount; ++l) {
						ObjectFile::Reference* ref = refs[l];
						if ( curSection->fAllNonLazyPointers || curSection->fAllLazyPointers 
										|| curSection->fAllLazyDylibPointers || curSection->fAllStubs ) {
							uint32_t offsetInSection = atom->getSectionOffset();
							uint32_t indexInSection = offsetInSection / atom->getSize();
							uint32_t undefinedSymbolIndex;
							if ( curSection->fAllStubs ) {
								ObjectFile::Atom& stubTarget =ref->getTarget();
								ObjectFile::Atom& stubTargetTarget = stubTarget.getReferences()[0]->getTarget();
								undefinedSymbolIndex = this->symbolIndex(stubTargetTarget);
								//fprintf(stderr, "stub %s ==> %s ==> %s ==> index:%u\n", atom->getDisplayName(), stubTarget.getDisplayName(), stubTargetTarget.getDisplayName(), undefinedSymbolIndex);
							}
							else if ( curSection->fAllNonLazyPointers) {
								// only use INDIRECT_SYMBOL_LOCAL in non-lazy-pointers for atoms that won't be in symbol table or have an addend
								if ( this->indirectSymbolInRelocatableIsLocal(ref) )
									undefinedSymbolIndex = INDIRECT_SYMBOL_LOCAL;
								else
									undefinedSymbolIndex = this->symbolIndex(ref->getTarget());
							}
							else {
								// should never get here, fAllLazyPointers not used in generated .o files
								undefinedSymbolIndex = INDIRECT_SYMBOL_LOCAL;
							}
							uint32_t indirectTableIndex = indexInSection + curSection->fIndirectSymbolOffset;
							IndirectEntry entry = { indirectTableIndex, undefinedSymbolIndex };
							//printf("fIndirectTableAtom->fTable.add(sectionIndex=%u, indirectTableIndex=%u => %u), size=%lld\n", indexInSection, indirectTableIndex, undefinedSymbolIndex, atom->getSize());
							fIndirectTableAtom->fTable.push_back(entry);
							if ( curSection->fAllLazyPointers ) {
								ObjectFile::Atom& target = ref->getTarget();
								ObjectFile::Atom& fromTarget = ref->getFromTarget();
								if ( &fromTarget == NULL ) {
									warning("lazy pointer %s missing initial binding", atom->getDisplayName());
								}
								else {
									bool isExtern = ( ((target.getDefinitionKind() == ObjectFile::Atom::kExternalDefinition)
										|| (target.getDefinitionKind() == ObjectFile::Atom::kExternalWeakDefinition))
										&& (target.getSymbolTableInclusion() != ObjectFile::Atom::kSymbolTableNotIn) );
									macho_relocation_info<P> reloc1;
									reloc1.set_r_address(atom->getSectionOffset());
									reloc1.set_r_symbolnum(isExtern ? this->symbolIndex(target) : target.getSection()->getIndex());
									reloc1.set_r_pcrel(false);
									reloc1.set_r_length();
									reloc1.set_r_extern(isExtern);
									reloc1.set_r_type(GENERIC_RELOC_VANILLA);
									fSectionRelocs.push_back(reloc1);
									++relocIndex;
								}
							}
							else if ( curSection->fAllStubs ) {
								relocIndex += this->addObjectRelocs(atom, ref);
							}
						}
						else if ( (ref->getKind() != A::kNoFixUp) && (ref->getTargetBinding() != ObjectFile::Reference::kDontBind) ) {
							relocIndex += this->addObjectRelocs(atom, ref);
						}
					}
				}
				curSection->fRelocCount = relocIndex - curSection->fRelocOffset;
			}
		}
	}

	// reverse the relocs
	std::reverse(fSectionRelocs.begin(), fSectionRelocs.end());
	
	// now reverse section reloc offsets
	for(int i=0; i < segCount; ++i) {
		SegmentInfo* curSegment = segmentInfos[i];
		std::vector<SectionInfo*>& sectionInfos = curSegment->fSections;
		const int sectionCount = sectionInfos.size();
		for(int j=0; j < sectionCount; ++j) {
			SectionInfo* curSection = sectionInfos[j];
			curSection->fRelocOffset = relocIndex - curSection->fRelocOffset - curSection->fRelocCount;
		}
	}

}


template <>
uint64_t Writer<x86_64>::relocAddressInFinalLinkedImage(uint64_t address, const ObjectFile::Atom* atom)  const
{
	uint64_t result;
	if ( fOptions.outputKind() == Options::kKextBundle ) {
		// for x86_64 kext bundles, the r_address field in relocs 
		// is the offset from the start address of the first segment
		result = address - fSegmentInfos[0]->fBaseAddress;
		if ( result > 0xFFFFFFFF ) {
			throwf("kext bundle too large: address can't fit in 31-bit r_address field in %s from %s",
				atom->getDisplayName(), atom->getFile()->getPath());
		}
	}
	else {
		// for x86_64, the r_address field in relocs for final linked images 
		// is the offset from the start address of the first writable segment
		result = address - fFirstWritableSegment->fBaseAddress;
		if ( result > 0xFFFFFFFF ) {
			if ( strcmp(atom->getSegment().getName(), "__TEXT") == 0 )
				throwf("text relocs not supported for x86_64 in %s from %s",
						atom->getDisplayName(), atom->getFile()->getPath());
			else
				throwf("image too large: address can't fit in 32-bit r_address field in %s from %s",
						atom->getDisplayName(), atom->getFile()->getPath());
		}
	}
	return result;
}


template <>
bool Writer<ppc>::illegalRelocInFinalLinkedImage(const ObjectFile::Reference& ref)
{
	switch ( ref.getKind() ) {
		case ppc::kAbsLow16:
		case ppc::kAbsLow14:
		case ppc::kAbsHigh16:
		case ppc::kAbsHigh16AddLow:
			if ( fSlideable )
				return true;
	}
	return false;
}


template <>
bool Writer<ppc64>::illegalRelocInFinalLinkedImage(const ObjectFile::Reference& ref)
{
	switch ( ref.getKind() ) {
		case ppc::kAbsLow16:
		case ppc::kAbsLow14:
		case ppc::kAbsHigh16:
		case ppc::kAbsHigh16AddLow:
			if ( fSlideable )
				return true;
	}
	return false;
}

template <>
bool Writer<x86>::illegalRelocInFinalLinkedImage(const ObjectFile::Reference& ref)
{
	if ( ref.getKind() == x86::kAbsolute32 ) {
		switch ( ref.getTarget().getDefinitionKind() ) {
			case ObjectFile::Atom::kTentativeDefinition:
			case ObjectFile::Atom::kRegularDefinition:
			case ObjectFile::Atom::kWeakDefinition:
				// illegal in dylibs/bundles, until we support TEXT relocs 
				return fSlideable;
			case ObjectFile::Atom::kExternalDefinition:
			case ObjectFile::Atom::kExternalWeakDefinition:
				// illegal until we support TEXT relocs
				return true;
			case ObjectFile::Atom::kAbsoluteSymbol:
				// absolute symbbols only allowed in static executables
				return ( fOptions.outputKind() != Options::kStaticExecutable);
		}
	}
	return false;
}

template <>
bool Writer<x86_64>::illegalRelocInFinalLinkedImage(const ObjectFile::Reference& ref)
{
	if ( fOptions.outputKind() == Options::kKextBundle ) {
		switch ( ref.getTarget().getDefinitionKind() ) {
			case ObjectFile::Atom::kTentativeDefinition:
			case ObjectFile::Atom::kRegularDefinition:
			case ObjectFile::Atom::kWeakDefinition:
			case ObjectFile::Atom::kAbsoluteSymbol:
				return false;
			case ObjectFile::Atom::kExternalDefinition:
			case ObjectFile::Atom::kExternalWeakDefinition:
				// true means we need a TEXT relocs
				switch ( ref.getKind() ) {
					case x86_64::kBranchPCRel32:
					case x86_64::kBranchPCRel32WeakImport:
					case x86_64::kPCRel32GOTLoad:
					case x86_64::kPCRel32GOTLoadWeakImport:
					case x86_64::kPCRel32GOT:
					case x86_64::kPCRel32GOTWeakImport:
						return true;
				}
				break;
		}
	}
	return false;
}

template <>
bool Writer<arm>::illegalRelocInFinalLinkedImage(const ObjectFile::Reference& ref)
{
	if ( ref.getKind() == arm::kReadOnlyPointer ) {
		switch ( ref.getTarget().getDefinitionKind() ) {
			case ObjectFile::Atom::kTentativeDefinition:
			case ObjectFile::Atom::kRegularDefinition:
			case ObjectFile::Atom::kWeakDefinition:
				// illegal in dylibs/bundles, until we support TEXT relocs 
				return fSlideable;
			case ObjectFile::Atom::kExternalDefinition:
			case ObjectFile::Atom::kExternalWeakDefinition:
				// illegal until we support TEXT relocs
				return true;
			case ObjectFile::Atom::kAbsoluteSymbol:
				// absolute symbbols only allowed in static executables
				return ( fOptions.outputKind() != Options::kStaticExecutable);
		}
	}
	return false;
}

template <>
bool Writer<x86>::generatesLocalTextReloc(const ObjectFile::Reference& ref, const ObjectFile::Atom& atom, SectionInfo* atomSection)
{
	if ( ref.getKind() == x86::kAbsolute32 ) {
		switch ( ref.getTarget().getDefinitionKind() ) {
			case ObjectFile::Atom::kTentativeDefinition:
			case ObjectFile::Atom::kRegularDefinition:
			case ObjectFile::Atom::kWeakDefinition:
				// a reference to the absolute address of something in this same linkage unit can be 
				// encoded as a local text reloc in a dylib or bundle 
				if ( fSlideable ) {
					macho_relocation_info<P> reloc;
					SectionInfo* sectInfo = (SectionInfo*)(ref.getTarget().getSection());
					reloc.set_r_address(this->relocAddressInFinalLinkedImage(atom.getAddress() + ref.getFixUpOffset(), &atom));
					reloc.set_r_symbolnum(sectInfo->getIndex());
					reloc.set_r_pcrel(false);
					reloc.set_r_length();
					reloc.set_r_extern(false);
					reloc.set_r_type(GENERIC_RELOC_VANILLA);
					fInternalRelocs.push_back(reloc);
					atomSection->fHasTextLocalRelocs = true;
					if ( fOptions.makeCompressedDyldInfo() ) {
						fRebaseInfo.push_back(RebaseInfo(REBASE_TYPE_TEXT_ABSOLUTE32, atom.getAddress() + ref.getFixUpOffset()));
					}
					return true;
				}
				return false;
			case ObjectFile::Atom::kExternalDefinition:
			case ObjectFile::Atom::kExternalWeakDefinition:
			case ObjectFile::Atom::kAbsoluteSymbol:
				return false;
		}
	}
	return false;
}

template <>
bool Writer<ppc>::generatesLocalTextReloc(const ObjectFile::Reference& ref, const ObjectFile::Atom& atom, SectionInfo* atomSection)
{
	macho_relocation_info<P> reloc1;
	macho_relocation_info<P> reloc2;
	switch ( ref.getTarget().getDefinitionKind() ) {
		case ObjectFile::Atom::kTentativeDefinition:
		case ObjectFile::Atom::kRegularDefinition:
		case ObjectFile::Atom::kWeakDefinition:
			switch ( ref.getKind() ) {
				case ppc::kAbsLow16:
				case ppc::kAbsLow14:
					// a reference to the absolute address of something in this same linkage unit can be 
					// encoded as a local text reloc in a dylib or bundle 
					if ( fSlideable ) {
						SectionInfo* sectInfo = (SectionInfo*)(ref.getTarget().getSection());
						uint32_t targetAddr = ref.getTarget().getAddress() + ref.getTargetOffset();
						reloc1.set_r_address(this->relocAddressInFinalLinkedImage(atom.getAddress() + ref.getFixUpOffset(), &atom));
						reloc1.set_r_symbolnum(sectInfo->getIndex());
						reloc1.set_r_pcrel(false);
						reloc1.set_r_length(2);
						reloc1.set_r_extern(false);
						reloc1.set_r_type(ref.getKind()==ppc::kAbsLow16 ? PPC_RELOC_LO16 : PPC_RELOC_LO14);
						reloc2.set_r_address(targetAddr >> 16);
						reloc2.set_r_symbolnum(0);
						reloc2.set_r_pcrel(false);
						reloc2.set_r_length(2);
						reloc2.set_r_extern(false);
						reloc2.set_r_type(PPC_RELOC_PAIR);
						fInternalRelocs.push_back(reloc1);
						fInternalRelocs.push_back(reloc2);
						atomSection->fHasTextLocalRelocs = true;
						return true;
					}
					break;
				case ppc::kAbsHigh16:
				case ppc::kAbsHigh16AddLow:
					if ( fSlideable ) {
						SectionInfo* sectInfo = (SectionInfo*)(ref.getTarget().getSection());
						uint32_t targetAddr = ref.getTarget().getAddress() + ref.getTargetOffset();
						reloc1.set_r_address(this->relocAddressInFinalLinkedImage(atom.getAddress() + ref.getFixUpOffset(), &atom));
						reloc1.set_r_symbolnum(sectInfo->getIndex());
						reloc1.set_r_pcrel(false);
						reloc1.set_r_length(2);
						reloc1.set_r_extern(false);
						reloc1.set_r_type(ref.getKind()==ppc::kAbsHigh16AddLow ? PPC_RELOC_HA16 : PPC_RELOC_HI16);
						reloc2.set_r_address(targetAddr & 0xFFFF);
						reloc2.set_r_symbolnum(0);
						reloc2.set_r_pcrel(false);
						reloc2.set_r_length(2);
						reloc2.set_r_extern(false);
						reloc2.set_r_type(PPC_RELOC_PAIR);
						fInternalRelocs.push_back(reloc1);
						fInternalRelocs.push_back(reloc2);
						atomSection->fHasTextLocalRelocs = true;
						return true;
					}
			}
			break;
		case ObjectFile::Atom::kExternalDefinition:
		case ObjectFile::Atom::kExternalWeakDefinition:
		case ObjectFile::Atom::kAbsoluteSymbol:
			return false;
	}
	return false;
}

template <>
bool Writer<arm>::generatesLocalTextReloc(const ObjectFile::Reference& ref, const ObjectFile::Atom& atom, SectionInfo* atomSection)
{
	if ( ref.getKind() == arm::kReadOnlyPointer ) {
		switch ( ref.getTarget().getDefinitionKind() ) {
			case ObjectFile::Atom::kTentativeDefinition:
			case ObjectFile::Atom::kRegularDefinition:
			case ObjectFile::Atom::kWeakDefinition:
				// a reference to the absolute address of something in this same linkage unit can be 
				// encoded as a local text reloc in a dylib or bundle 
				if ( fSlideable ) {
					macho_relocation_info<P> reloc;
					SectionInfo* sectInfo = (SectionInfo*)(ref.getTarget().getSection());
					reloc.set_r_address(this->relocAddressInFinalLinkedImage(atom.getAddress() + ref.getFixUpOffset(), &atom));
					reloc.set_r_symbolnum(sectInfo->getIndex());
					reloc.set_r_pcrel(false);
					reloc.set_r_length();
					reloc.set_r_extern(false);
					reloc.set_r_type(GENERIC_RELOC_VANILLA);
					fInternalRelocs.push_back(reloc);
					atomSection->fHasTextLocalRelocs = true;
					return true;
				}
				return false;
			case ObjectFile::Atom::kExternalDefinition:
			case ObjectFile::Atom::kExternalWeakDefinition:
			case ObjectFile::Atom::kAbsoluteSymbol:
				return false;
		}
	}
	return false;
}


template <>
bool Writer<x86_64>::generatesLocalTextReloc(const ObjectFile::Reference&, const ObjectFile::Atom& atom, SectionInfo* curSection)
{
	// text relocs not supported (usually never needed because of RIP addressing)
	return false;
}

template <>
bool Writer<ppc64>::generatesLocalTextReloc(const ObjectFile::Reference&, const ObjectFile::Atom& atom, SectionInfo* curSection)
{
	// text relocs not supported
	return false;
}

template <>
bool Writer<x86>::generatesExternalTextReloc(const ObjectFile::Reference& ref, const ObjectFile::Atom& atom, SectionInfo* atomSection)
{
	if ( ref.getKind() == x86::kAbsolute32 ) {
		macho_relocation_info<P> reloc;
		switch ( ref.getTarget().getDefinitionKind() ) {
			case ObjectFile::Atom::kTentativeDefinition:
			case ObjectFile::Atom::kRegularDefinition:
			case ObjectFile::Atom::kWeakDefinition:
				return false;
			case ObjectFile::Atom::kExternalDefinition:
			case ObjectFile::Atom::kExternalWeakDefinition:
				// a reference to the absolute address of something in another linkage unit can be 
				// encoded as an external text reloc in a dylib or bundle 
				reloc.set_r_address(this->relocAddressInFinalLinkedImage(atom.getAddress() + ref.getFixUpOffset(), &atom));
				reloc.set_r_symbolnum(this->symbolIndex(ref.getTarget()));
				reloc.set_r_pcrel(false);
				reloc.set_r_length();
				reloc.set_r_extern(true);
				reloc.set_r_type(GENERIC_RELOC_VANILLA);
				fExternalRelocs.push_back(reloc);
				atomSection->fHasTextExternalRelocs = true;
				return true;
			case ObjectFile::Atom::kAbsoluteSymbol:
				return false;
		}
	}
	return false;
}

template <>
bool Writer<x86_64>::generatesExternalTextReloc(const ObjectFile::Reference& ref, const ObjectFile::Atom& atom, SectionInfo* atomSection)
{
	if ( fOptions.outputKind() == Options::kKextBundle ) {
		macho_relocation_info<P> reloc;
		switch ( ref.getTarget().getDefinitionKind() ) {
			case ObjectFile::Atom::kTentativeDefinition:
			case ObjectFile::Atom::kRegularDefinition:
			case ObjectFile::Atom::kWeakDefinition:
			case ObjectFile::Atom::kAbsoluteSymbol:
				return false;
			case ObjectFile::Atom::kExternalDefinition:
			case ObjectFile::Atom::kExternalWeakDefinition:
				switch ( ref.getKind() ) {
					case x86_64::kBranchPCRel32:
					case x86_64::kBranchPCRel32WeakImport:
						// a branch to something in another linkage unit is
						// encoded as an external text reloc in a kext bundle 
						reloc.set_r_address(this->relocAddressInFinalLinkedImage(atom.getAddress() + ref.getFixUpOffset(), &atom));
						reloc.set_r_symbolnum(this->symbolIndex(ref.getTarget()));
						reloc.set_r_pcrel(true);
						reloc.set_r_length(2);
						reloc.set_r_extern(true);
						reloc.set_r_type(X86_64_RELOC_BRANCH);
						fExternalRelocs.push_back(reloc);
						atomSection->fHasTextExternalRelocs = true;
						return true;
					case x86_64::kPCRel32GOTLoad:
					case x86_64::kPCRel32GOTLoadWeakImport:
						// a load of the GOT entry for a symbol in another linkage unit is 
						// encoded as an external text reloc in a kext bundle 
						reloc.set_r_address(this->relocAddressInFinalLinkedImage(atom.getAddress() + ref.getFixUpOffset(), &atom));
						reloc.set_r_symbolnum(this->symbolIndex(ref.getTarget()));
						reloc.set_r_pcrel(true);
						reloc.set_r_length(2);
						reloc.set_r_extern(true);
						reloc.set_r_type(X86_64_RELOC_GOT_LOAD);
						fExternalRelocs.push_back(reloc);
						atomSection->fHasTextExternalRelocs = true;
						return true;
					case x86_64::kPCRel32GOT:
					case x86_64::kPCRel32GOTWeakImport:
						// a use of the GOT entry for a symbol in another linkage unit is 
						// encoded as an external text reloc in a kext bundle 
						reloc.set_r_address(this->relocAddressInFinalLinkedImage(atom.getAddress() + ref.getFixUpOffset(), &atom));
						reloc.set_r_symbolnum(this->symbolIndex(ref.getTarget()));
						reloc.set_r_pcrel(true);
						reloc.set_r_length(2);
						reloc.set_r_extern(true);
						reloc.set_r_type(X86_64_RELOC_GOT);
						fExternalRelocs.push_back(reloc);
						atomSection->fHasTextExternalRelocs = true;
						return true;
				}
				break;	
		}
	}
	return false;
}


template <typename A>
bool Writer<A>::generatesExternalTextReloc(const ObjectFile::Reference&, const ObjectFile::Atom& atom, SectionInfo* curSection)
{
	return false;
}




template <typename A>
typename Writer<A>::RelocKind Writer<A>::relocationNeededInFinalLinkedImage(const ObjectFile::Atom& target) const
{
	switch ( target.getDefinitionKind() ) {
		case ObjectFile::Atom::kTentativeDefinition:
		case ObjectFile::Atom::kRegularDefinition:
			// in main executables, the only way regular symbols are indirected is if -interposable is used
			if ( fOptions.outputKind() == Options::kDynamicExecutable ) {
				if ( this->shouldExport(target) && fOptions.interposable(target.getName()) )
					return kRelocExternal;
				else if ( fSlideable )
					return kRelocInternal;
				else
					return kRelocNone;
			}
			// for flat-namespace or interposable two-level-namespace
			// all references to exported symbols get indirected
			else if ( this->shouldExport(target) &&
			   ((fOptions.nameSpace() == Options::kFlatNameSpace)
			  || (fOptions.nameSpace() == Options::kForceFlatNameSpace)
			  || fOptions.interposable(target.getName())) 
			  && (target.getName() != NULL) 
			  && (strncmp(target.getName(), ".objc_class_", 12) != 0) ) // <rdar://problem/5254468>
				return kRelocExternal;
			else if ( fSlideable )
				return kRelocInternal;
			else
				return kRelocNone;
		case ObjectFile::Atom::kWeakDefinition:
			// all calls to global weak definitions get indirected
			if ( this->shouldExport(target) )
				return kRelocExternal;
			else if ( fSlideable )
				return kRelocInternal;
			else
				return kRelocNone;
		case ObjectFile::Atom::kExternalDefinition:
		case ObjectFile::Atom::kExternalWeakDefinition:
			return kRelocExternal;
		case ObjectFile::Atom::kAbsoluteSymbol:
			return kRelocNone;
	}
	return kRelocNone;
}

template <typename A>
uint64_t Writer<A>::relocAddressInFinalLinkedImage(uint64_t address, const ObjectFile::Atom* atom) const
{
	// for 32-bit architectures, the r_address field in relocs
	// for final linked images is the offset from the first segment
	uint64_t result = address - fSegmentInfos[0]->fBaseAddress;
	if ( fOptions.outputKind() == Options::kPreload ) {
		// kPreload uses a virtual __HEADER segment to cover the load commands
		result = address - fSegmentInfos[1]->fBaseAddress;
	}
	// or the offset from the first writable segment if built split-seg
	if ( fOptions.splitSeg() )
		result = address - fFirstWritableSegment->fBaseAddress;
	if ( result > 0x7FFFFFFF ) {
		throwf("image too large: address can't fit in 31-bit r_address field in %s from %s",
			atom->getDisplayName(), atom->getFile()->getPath());
	}
	return result;
}

template <>
uint64_t Writer<ppc64>::relocAddressInFinalLinkedImage(uint64_t address, const ObjectFile::Atom* atom)  const
{
	// for ppc64, the Mac OS X 10.4 dyld assumes r_address is always the offset from the base address.  
	// the 10.5 dyld, iterprets the r_address as:
	//   1) an offset from the base address, iff there are no writable segments with a address > 4GB from base address, otherwise
	//   2) an offset from the base address of the first writable segment
	// For dyld, r_address is always the offset from the base address
	uint64_t result;
	bool badFor10_4 = false;
	if ( fWritableSegmentPastFirst4GB ) {
		if ( fOptions.macosxVersionMin() < ObjectFile::ReaderOptions::k10_5 )
			badFor10_4 = true;
		result = address - fFirstWritableSegment->fBaseAddress;
		if ( result > 0xFFFFFFFF ) {
			throwf("image too large: address can't fit in 32-bit r_address field in %s from %s",
				atom->getDisplayName(), atom->getFile()->getPath());
		}
	}
	else {
		result = address - fSegmentInfos[0]->fBaseAddress;
		if ( (fOptions.macosxVersionMin() < ObjectFile::ReaderOptions::k10_5) && (result > 0x7FFFFFFF) )
			badFor10_4 = true;
	}
	if ( badFor10_4 ) {
			throwf("image or pagezero_size too large for Mac OS X 10.4: address can't fit in 31-bit r_address field for %s from %s",
				atom->getDisplayName(), atom->getFile()->getPath());
	}
	return result;
}


template <> bool    Writer<ppc>::preboundLazyPointerType(uint8_t* type) { *type = PPC_RELOC_PB_LA_PTR; return true; }
template <> bool  Writer<ppc64>::preboundLazyPointerType(uint8_t* type) { throw "prebinding not supported"; }
template <> bool    Writer<x86>::preboundLazyPointerType(uint8_t* type) { *type = GENERIC_RELOC_PB_LA_PTR; return true; }
template <> bool Writer<x86_64>::preboundLazyPointerType(uint8_t* type) { throw "prebinding not supported"; }
template <> bool    Writer<arm>::preboundLazyPointerType(uint8_t* type) { *type = ARM_RELOC_PB_LA_PTR; return true; }

template <typename A>
void Writer<A>::buildExecutableFixups()
{
	if ( fIndirectTableAtom != NULL )
		fIndirectTableAtom->fTable.reserve(50);  // minimize reallocations
	std::vector<SegmentInfo*>& segmentInfos = fSegmentInfos;
	const int segCount = segmentInfos.size();
	for(int i=0; i < segCount; ++i) {
		SegmentInfo* curSegment = segmentInfos[i];
		std::vector<SectionInfo*>& sectionInfos = curSegment->fSections;
		const int sectionCount = sectionInfos.size();
		for(int j=0; j < sectionCount; ++j) {
			SectionInfo* curSection = sectionInfos[j];
			//fprintf(stderr, "starting section %s\n", curSection->fSectionName);
			std::vector<ObjectFile::Atom*>& sectionAtoms = curSection->fAtoms;
			if ( ! curSection->fAllZeroFill ) {
				if ( curSection->fAllNonLazyPointers || curSection->fAllLazyPointers || curSection->fAllLazyDylibPointers 
												|| curSection->fAllStubs || curSection->fAllSelfModifyingStubs ) {
					if ( fIndirectTableAtom != NULL )
						curSection->fIndirectSymbolOffset = fIndirectTableAtom->fTable.size();
				}
				const int atomCount = sectionAtoms.size();
				for (int k=0; k < atomCount; ++k) {
					ObjectFile::Atom* atom = sectionAtoms[k];
					std::vector<ObjectFile::Reference*>& refs = atom->getReferences();
					const int refCount = refs.size();
					//fprintf(stderr, "atom %s has %d references in section %s, %p\n", atom->getDisplayName(), refCount, curSection->fSectionName, atom->getSection());
					if ( curSection->fAllNonLazyPointers && (refCount == 0) ) {
						// handle imageloadercache GOT slot
						uint32_t offsetInSection = atom->getSectionOffset();
						uint32_t indexInSection = offsetInSection / sizeof(pint_t);
						uint32_t indirectTableIndex = indexInSection + curSection->fIndirectSymbolOffset;
						// use INDIRECT_SYMBOL_ABS so 10.5 dyld will leave value as zero
						IndirectEntry entry = { indirectTableIndex, INDIRECT_SYMBOL_ABS };
						//fprintf(stderr,"fIndirectTableAtom->fTable.push_back(tableIndex=%d, symIndex=0x%X, section=%s)\n", 
						//		indirectTableIndex, INDIRECT_SYMBOL_LOCAL, curSection->fSectionName);
						fIndirectTableAtom->fTable.push_back(entry);
					}
					for (int l=0; l < refCount; ++l) {
						ObjectFile::Reference* ref = refs[l];
						if ( (fOptions.outputKind() != Options::kKextBundle) && 
								(curSection->fAllNonLazyPointers || curSection->fAllLazyPointers || curSection->fAllLazyDylibPointers) ) {
							// if atom is in (non)lazy_pointer section, this is encoded as an indirect symbol
							if ( atom->getSize() != sizeof(pint_t) ) {
								warning("wrong size pointer atom %s from file %s", atom->getDisplayName(), atom->getFile()->getPath());
							}
							ObjectFile::Atom* pointerTarget = &(ref->getTarget());
							if ( curSection->fAllLazyPointers || curSection->fAllLazyDylibPointers ) {
								pointerTarget = ((LazyPointerAtom<A>*)atom)->getTarget();
							}
							uint32_t offsetInSection = atom->getSectionOffset();
							uint32_t indexInSection = offsetInSection / sizeof(pint_t);
							uint32_t undefinedSymbolIndex = INDIRECT_SYMBOL_LOCAL;
							if  (atom == fFastStubGOTAtom)
								undefinedSymbolIndex = INDIRECT_SYMBOL_ABS;
							else if ( this->relocationNeededInFinalLinkedImage(*pointerTarget) == kRelocExternal )
								undefinedSymbolIndex = this->symbolIndex(*pointerTarget);
							uint32_t indirectTableIndex = indexInSection + curSection->fIndirectSymbolOffset;
							IndirectEntry entry = { indirectTableIndex, undefinedSymbolIndex };
							//fprintf(stderr,"fIndirectTableAtom->fTable.push_back(tableIndex=%d, symIndex=0x%X, section=%s)\n", 
							//		indirectTableIndex, undefinedSymbolIndex, curSection->fSectionName);
							fIndirectTableAtom->fTable.push_back(entry);
							if ( curSection->fAllLazyPointers || curSection->fAllLazyDylibPointers ) {
								uint8_t preboundLazyType;
								if ( fOptions.prebind() && (fDyldClassicHelperAtom != NULL) 
										&& curSection->fAllLazyPointers && preboundLazyPointerType(&preboundLazyType) ) {
									// this is a prebound image, need special relocs for dyld to reset lazy pointers if prebinding is invalid
									macho_scattered_relocation_info<P> pblaReloc;
									pblaReloc.set_r_scattered(true);
									pblaReloc.set_r_pcrel(false);
									pblaReloc.set_r_length();
									pblaReloc.set_r_type(preboundLazyType);
									pblaReloc.set_r_address(relocAddressInFinalLinkedImage(atom->getAddress(), atom));
									pblaReloc.set_r_value(fDyldClassicHelperAtom->getAddress());
									fInternalRelocs.push_back(*((macho_relocation_info<P>*)&pblaReloc));
								}
								else if ( fSlideable ) {
									// this is a non-prebound dylib/bundle, need vanilla internal relocation to fix up binding handler if image slides
									macho_relocation_info<P> dyldHelperReloc;
									uint32_t sectionNum = 1;
									if ( fDyldClassicHelperAtom != NULL )
										sectionNum = ((SectionInfo*)(fDyldClassicHelperAtom->getSection()))->getIndex();
									//fprintf(stderr, "lazy pointer reloc, section index=%u, section name=%s\n", sectionNum, curSection->fSectionName);
									dyldHelperReloc.set_r_address(relocAddressInFinalLinkedImage(atom->getAddress(), atom));
									dyldHelperReloc.set_r_symbolnum(sectionNum);
									dyldHelperReloc.set_r_pcrel(false);
									dyldHelperReloc.set_r_length();
									dyldHelperReloc.set_r_extern(false);
									dyldHelperReloc.set_r_type(GENERIC_RELOC_VANILLA);
									fInternalRelocs.push_back(dyldHelperReloc);
									if ( fOptions.makeCompressedDyldInfo() ) {
										fRebaseInfo.push_back(RebaseInfo(REBASE_TYPE_POINTER,atom->getAddress()));
									}
								}
								if ( fOptions.makeCompressedDyldInfo() ) {
									uint8_t type = BIND_TYPE_POINTER;
									uint64_t addresss = atom->getAddress() + ref->getFixUpOffset();
									if ( pointerTarget->getDefinitionKind() == ObjectFile::Atom::kExternalWeakDefinition ) {
										// This is a referece to a weak def in some dylib (e.g. operator new)
										// need to bind into to directly bind this
										// later weak binding info may override
										int ordinal = compressedOrdinalForImortedAtom(pointerTarget);
										fBindingInfo.push_back(BindingInfo(type, ordinal, pointerTarget->getName(), false, addresss, 0));
									}
									if ( targetRequiresWeakBinding(*pointerTarget) ) {
										// note: lazy pointers to weak symbols are not bound lazily
										fWeakBindingInfo.push_back(BindingInfo(type, pointerTarget->getName(), false, addresss, 0));
									}
								}
							}
							if ( curSection->fAllNonLazyPointers && fOptions.makeCompressedDyldInfo() ) {
								if ( pointerTarget != NULL ) {
									switch ( this->relocationNeededInFinalLinkedImage(*pointerTarget) ) {
										case kRelocNone:
										// no rebase or binding info needed
										break;
										case kRelocInternal:
											// a non-lazy pointer that has been optimized to LOCAL needs rebasing info
											// but not the magic fFastStubGOTAtom atom
											if  (atom != fFastStubGOTAtom)
												fRebaseInfo.push_back(RebaseInfo(REBASE_TYPE_POINTER,atom->getAddress()));
											break;
										case kRelocExternal:
										{
											uint8_t type = BIND_TYPE_POINTER;
											uint64_t addresss = atom->getAddress();
											if ( targetRequiresWeakBinding(ref->getTarget()) ) {
												fWeakBindingInfo.push_back(BindingInfo(type, ref->getTarget().getName(), false, addresss, 0));
												// if this is a non-lazy pointer to a weak definition within this linkage unit
												// the pointer needs to initially point within linkage unit and have
												// rebase command to slide it.
												if ( ref->getTarget().getDefinitionKind() == ObjectFile::Atom::kWeakDefinition ) {
													// unless if this is a hybrid format, in which case the non-lazy pointer
													// is zero on disk.  So use a bind instead of a rebase to set initial value
													if ( fOptions.makeClassicDyldInfo() )
														fBindingInfo.push_back(BindingInfo(type, BIND_SPECIAL_DYLIB_SELF, ref->getTarget().getName(), false, addresss, 0));
													else
														fRebaseInfo.push_back(RebaseInfo(REBASE_TYPE_POINTER,atom->getAddress()));
												}
												// if this is a non-lazy pointer to a weak definition in a dylib,
												// the pointer needs to initially bind to the dylib
												else if ( ref->getTarget().getDefinitionKind() == ObjectFile::Atom::kExternalWeakDefinition ) {
													int ordinal = compressedOrdinalForImortedAtom(pointerTarget);
													fBindingInfo.push_back(BindingInfo(BIND_TYPE_POINTER, ordinal, pointerTarget->getName(), false, addresss, 0));
												}
											}
											else {
												int ordinal = compressedOrdinalForImortedAtom(pointerTarget);
												bool weak_import = fWeakImportMap[pointerTarget];
												fBindingInfo.push_back(BindingInfo(type, ordinal, ref->getTarget().getName(), weak_import, addresss, 0));
											}
										}
									}
								}
							}
						}
 						else if ( (ref->getKind() == A::kPointer) || (ref->getKind() == A::kPointerWeakImport) ) {
							if ( fSlideable && ((curSegment->fInitProtection & VM_PROT_WRITE) == 0) ) {
								if ( fOptions.allowTextRelocs() ) {
									if ( fOptions.warnAboutTextRelocs() )
										warning("text reloc in %s to %s", atom->getDisplayName(), ref->getTargetName());
								}
								else {
									throwf("pointer in read-only segment not allowed in slidable image, used in %s from %s",
										atom->getDisplayName(), atom->getFile()->getPath());
								}
							}
							switch ( this->relocationNeededInFinalLinkedImage(ref->getTarget()) ) {
								case kRelocNone:
									// no reloc needed
									break;
								case kRelocInternal:
									{
										macho_relocation_info<P> internalReloc;
										SectionInfo* sectInfo = (SectionInfo*)ref->getTarget().getSection();
										uint32_t sectionNum = sectInfo->getIndex();
										// special case _mh_dylib_header and friends which are not in any real section
										if ( (sectionNum ==0) && sectInfo->fVirtualSection && (strcmp(sectInfo->fSectionName, "._mach_header") == 0) )
											sectionNum = 1;
										internalReloc.set_r_address(this->relocAddressInFinalLinkedImage(atom->getAddress() + ref->getFixUpOffset(), atom));
										internalReloc.set_r_symbolnum(sectionNum);
										internalReloc.set_r_pcrel(false);
										internalReloc.set_r_length();
										internalReloc.set_r_extern(false);
										internalReloc.set_r_type(GENERIC_RELOC_VANILLA);
										fInternalRelocs.push_back(internalReloc);
										if ( fOptions.makeCompressedDyldInfo() ) {
											fRebaseInfo.push_back(RebaseInfo(REBASE_TYPE_POINTER, atom->getAddress() + ref->getFixUpOffset()));
										}
									}
									break;
								case kRelocExternal:
									{
										macho_relocation_info<P> externalReloc;
										externalReloc.set_r_address(this->relocAddressInFinalLinkedImage(atom->getAddress() + ref->getFixUpOffset(), atom));
										externalReloc.set_r_symbolnum(this->symbolIndex(ref->getTarget()));
										externalReloc.set_r_pcrel(false);
										externalReloc.set_r_length();
										externalReloc.set_r_extern(true);
										externalReloc.set_r_type(GENERIC_RELOC_VANILLA);
										fExternalRelocs.push_back(externalReloc);
										if ( fOptions.makeCompressedDyldInfo() ) {
											int64_t addend = ref->getTargetOffset();
											uint64_t addresss = atom->getAddress() + ref->getFixUpOffset();
											if ( !fOptions.makeClassicDyldInfo() ) {
												if ( ref->getTarget().getDefinitionKind() == ObjectFile::Atom::kWeakDefinition ) {
													// pointers to internal weak defs need a rebase
													fRebaseInfo.push_back(RebaseInfo(REBASE_TYPE_POINTER, addresss));
												}
											}
											uint8_t type = BIND_TYPE_POINTER;
											if ( targetRequiresWeakBinding(ref->getTarget()) ) {
												fWeakBindingInfo.push_back(BindingInfo(type, ref->getTarget().getName(), false, addresss, addend));
												if ( fOptions.makeClassicDyldInfo() && (ref->getTarget().getDefinitionKind() == ObjectFile::Atom::kWeakDefinition) ) {
													// hybrid linkedit puts addend in data, so we need bind phase to reset pointer to local definifion
													fBindingInfo.push_back(BindingInfo(type, BIND_SPECIAL_DYLIB_SELF, ref->getTarget().getName(), false, addresss, addend));
												}
												// if this is a pointer to a weak definition in a dylib,
												// the pointer needs to initially bind to the dylib
												else if ( ref->getTarget().getDefinitionKind() == ObjectFile::Atom::kExternalWeakDefinition ) {
													int ordinal = compressedOrdinalForImortedAtom(&ref->getTarget());
													fBindingInfo.push_back(BindingInfo(BIND_TYPE_POINTER, ordinal, ref->getTarget().getName(), false, addresss, addend));
												}
											}
											else {
												int ordinal = compressedOrdinalForImortedAtom(&ref->getTarget());
												bool weak_import = fWeakImportMap[&(ref->getTarget())];
												fBindingInfo.push_back(BindingInfo(type, ordinal, ref->getTarget().getName(), weak_import, addresss, addend));
											}
										}
									}
									break;
							}
						}
						else if ( this->illegalRelocInFinalLinkedImage(*ref) ) {
							// new x86 stubs always require text relocs
							if ( curSection->fAllStubs || curSection->fAllStubHelpers ) {
								if ( this->generatesLocalTextReloc(*ref, *atom, curSection) ) {
									// relocs added to fInternalRelocs
								}
							}
							else if ( fOptions.allowTextRelocs() && !atom->getSegment().isContentWritable() ) {
								if ( fOptions.warnAboutTextRelocs() )
									warning("text reloc in %s to %s", atom->getDisplayName(), ref->getTargetName());
								if (  this->generatesLocalTextReloc(*ref, *atom, curSection) ) {
									// relocs added to fInternalRelocs
								}
								else if ( this->generatesExternalTextReloc(*ref, *atom, curSection) ) {
									// relocs added to fExternalRelocs
								}
								else {
									throwf("relocation used in %s from %s not allowed in slidable image", atom->getDisplayName(), atom->getFile()->getPath());
								}
							}
							else {
								throwf("absolute addressing (perhaps -mdynamic-no-pic) used in %s from %s not allowed in slidable image. "
										"Use '-read_only_relocs suppress' to enable text relocs", atom->getDisplayName(), atom->getFile()->getPath());
							}
						}
					}
					if ( curSection->fAllSelfModifyingStubs || curSection->fAllStubs ) {
						ObjectFile::Atom* stubTarget = ((StubAtom<A>*)atom)->getTarget();
						uint32_t undefinedSymbolIndex = (stubTarget != NULL) ? this->symbolIndex(*stubTarget) : INDIRECT_SYMBOL_ABS;
						uint32_t offsetInSection = atom->getSectionOffset();
						uint32_t indexInSection = offsetInSection / atom->getSize();
						uint32_t indirectTableIndex = indexInSection + curSection->fIndirectSymbolOffset;
						IndirectEntry entry = { indirectTableIndex, undefinedSymbolIndex };
						//fprintf(stderr,"for stub: fIndirectTableAtom->fTable.add(%d-%d => 0x%X-%s), size=%lld\n", indexInSection, indirectTableIndex, undefinedSymbolIndex, stubTarget->getName(), atom->getSize());
						fIndirectTableAtom->fTable.push_back(entry);
					}
				}
			}
		}
	}
	if ( fSplitCodeToDataContentAtom != NULL )
		fSplitCodeToDataContentAtom->encode();
	if ( fCompressedRebaseInfoAtom != NULL ) 
		fCompressedRebaseInfoAtom->encode();
	if ( fCompressedBindingInfoAtom != NULL ) 
		fCompressedBindingInfoAtom->encode();
	if ( fCompressedWeakBindingInfoAtom != NULL ) 
		fCompressedWeakBindingInfoAtom->encode();
	if ( fCompressedLazyBindingInfoAtom != NULL ) 
		fCompressedLazyBindingInfoAtom->encode();
	if ( fCompressedExportInfoAtom != NULL ) 
		fCompressedExportInfoAtom->encode();
}


template <>
void Writer<ppc>::addCrossSegmentRef(const ObjectFile::Atom* atom, const ObjectFile::Reference* ref)
{
	switch ( (ppc::ReferenceKinds)ref->getKind() ) {
		case ppc::kPICBaseHigh16:
			fSplitCodeToDataContentAtom->addPPCHi16Location(atom, ref->getFixUpOffset());
			break;
		case ppc::kPointerDiff32:
			fSplitCodeToDataContentAtom->add32bitPointerLocation(atom, ref->getFixUpOffset());
			break;
		case ppc::kPointerDiff64:
			fSplitCodeToDataContentAtom->add64bitPointerLocation(atom, ref->getFixUpOffset());
			break;
		case ppc::kNoFixUp:
		case ppc::kGroupSubordinate:
		case ppc::kPointer:
		case ppc::kPointerWeakImport:
		case ppc::kPICBaseLow16:
		case ppc::kPICBaseLow14:
			// ignore
			break;
		default:
			warning("codegen with reference kind %d in %s prevents image from loading in dyld shared cache", ref->getKind(), atom->getDisplayName());
			fSplitCodeToDataContentAtom->setCantEncode();
	}
}

template <>
void Writer<ppc64>::addCrossSegmentRef(const ObjectFile::Atom* atom, const ObjectFile::Reference* ref)
{
	switch ( (ppc64::ReferenceKinds)ref->getKind()  ) {
		case ppc64::kPICBaseHigh16:
			fSplitCodeToDataContentAtom->addPPCHi16Location(atom, ref->getFixUpOffset());
			break;
		case ppc64::kPointerDiff32:
			fSplitCodeToDataContentAtom->add32bitPointerLocation(atom, ref->getFixUpOffset());
			break;
		case ppc64::kPointerDiff64:
			fSplitCodeToDataContentAtom->add64bitPointerLocation(atom, ref->getFixUpOffset());
			break;
		case ppc64::kNoFixUp:
		case ppc64::kGroupSubordinate:
		case ppc64::kPointer:
		case ppc64::kPointerWeakImport:
		case ppc64::kPICBaseLow16:
		case ppc64::kPICBaseLow14:
			// ignore
			break;
		default:
			warning("codegen with reference kind %d in %s prevents image from loading in dyld shared cache", ref->getKind(), atom->getDisplayName());
			fSplitCodeToDataContentAtom->setCantEncode();
	}
}

template <>
void Writer<x86>::addCrossSegmentRef(const ObjectFile::Atom* atom, const ObjectFile::Reference* ref)
{
	switch ( (x86::ReferenceKinds)ref->getKind()  ) {
		case x86::kPointerDiff:
		case x86::kImageOffset32:
			if ( strcmp(ref->getTarget().getSegment().getName(), "__IMPORT") == 0 ) 
				fSplitCodeToDataContentAtom->add32bitImportLocation(atom, ref->getFixUpOffset());
			else
				fSplitCodeToDataContentAtom->add32bitPointerLocation(atom, ref->getFixUpOffset());
			break;
		case x86::kNoFixUp:
		case x86::kGroupSubordinate:
		case x86::kPointer:
		case x86::kPointerWeakImport:
			// ignore
			break;
		case x86::kPCRel32:
		case x86::kPCRel32WeakImport:
			if (    (&(ref->getTarget().getSegment()) == &Segment::fgImportSegment)
				||  (&(ref->getTarget().getSegment()) == &Segment::fgROImportSegment) ) {
				fSplitCodeToDataContentAtom->add32bitImportLocation(atom, ref->getFixUpOffset());
				break;
			}
			// fall into warning case
		default:
			if ( fOptions.makeCompressedDyldInfo() && (ref->getKind() == x86::kAbsolute32) ) {
				// will be encoded in rebase info
			}
			else {
				warning("codegen in %s (offset 0x%08llX) prevents image from loading in dyld shared cache", atom->getDisplayName(), ref->getFixUpOffset());
				fSplitCodeToDataContentAtom->setCantEncode();
			}
	}
}

template <>
void Writer<x86_64>::addCrossSegmentRef(const ObjectFile::Atom* atom, const ObjectFile::Reference* ref)
{
	switch ( (x86_64::ReferenceKinds)ref->getKind()  ) {
		case x86_64::kPCRel32:
		case x86_64::kPCRel32_1:
		case x86_64::kPCRel32_2:
		case x86_64::kPCRel32_4:
		case x86_64::kPCRel32GOTLoad:
		case x86_64::kPCRel32GOTLoadWeakImport:
		case x86_64::kPCRel32GOT:
		case x86_64::kPCRel32GOTWeakImport:
		case x86_64::kPointerDiff32:
		case x86_64::kImageOffset32:
			fSplitCodeToDataContentAtom->add32bitPointerLocation(atom, ref->getFixUpOffset());
			break;
		case x86_64::kPointerDiff:
			fSplitCodeToDataContentAtom->add64bitPointerLocation(atom, ref->getFixUpOffset());
			break;
		case x86_64::kNoFixUp:
		case x86_64::kGroupSubordinate:
		case x86_64::kPointer:
		case x86_64::kGOTNoFixUp:
			// ignore
			break;
		default:
			warning("codegen in %s with kind %d prevents image from loading in dyld shared cache", atom->getDisplayName(), ref->getKind());
			fSplitCodeToDataContentAtom->setCantEncode();
	}
}

template <>
void Writer<arm>::addCrossSegmentRef(const ObjectFile::Atom* atom, const ObjectFile::Reference* ref)
{
	switch ( (arm::ReferenceKinds)ref->getKind()  ) {
		case arm::kPointerDiff:
			fSplitCodeToDataContentAtom->add32bitPointerLocation(atom, ref->getFixUpOffset());
			break;
		case arm::kNoFixUp:
		case arm::kGroupSubordinate:
		case arm::kPointer:
		case arm::kPointerWeakImport:
		case arm::kReadOnlyPointer:
			// ignore
			break;
		default:
			warning("codegen in %s prevents image from loading in dyld shared cache", atom->getDisplayName());
			fSplitCodeToDataContentAtom->setCantEncode();
	}
}

template <typename A>
bool Writer<A>::segmentsCanSplitApart(const ObjectFile::Atom& from, const ObjectFile::Atom& to)
{
	switch ( to.getDefinitionKind() ) {
		case ObjectFile::Atom::kExternalDefinition:
		case ObjectFile::Atom::kExternalWeakDefinition:
		case ObjectFile::Atom::kAbsoluteSymbol:
			return false;
		case ObjectFile::Atom::kRegularDefinition:
		case ObjectFile::Atom::kWeakDefinition:
		case ObjectFile::Atom::kTentativeDefinition:
			// segments with same permissions slide together
			return ( (from.getSegment().isContentExecutable() != to.getSegment().isContentExecutable())
					|| (from.getSegment().isContentWritable() != to.getSegment().isContentWritable()) );
	}
	throw "ld64 internal error";
}


template <>
void Writer<ppc>::writeNoOps(int fd, uint32_t from, uint32_t to)
{
	uint32_t ppcNop;
	OSWriteBigInt32(&ppcNop, 0, 0x60000000);
	for (uint32_t p=from; p < to; p += 4)
		::pwrite(fd, &ppcNop, 4, p);
}

template <>
void Writer<ppc64>::writeNoOps(int fd, uint32_t from, uint32_t to)
{
	uint32_t ppcNop;
	OSWriteBigInt32(&ppcNop, 0, 0x60000000);
	for (uint32_t p=from; p < to; p += 4)
		::pwrite(fd, &ppcNop, 4, p);
}

template <>
void Writer<x86>::writeNoOps(int fd, uint32_t from, uint32_t to)
{
	uint8_t x86Nop = 0x90;
	for (uint32_t p=from; p < to; ++p)
		::pwrite(fd, &x86Nop, 1, p);
}

template <>
void Writer<x86_64>::writeNoOps(int fd, uint32_t from, uint32_t to)
{
	uint8_t x86Nop = 0x90;
	for (uint32_t p=from; p < to; ++p)
		::pwrite(fd, &x86Nop, 1, p);
}

template <>
void Writer<arm>::writeNoOps(int fd, uint32_t from, uint32_t to)
{
	// FIXME: need thumb nop?
	uint32_t armNop;
	OSWriteLittleInt32(&armNop, 0, 0xe1a00000);
	for (uint32_t p=from; p < to; p += 4)
		::pwrite(fd, &armNop, 4, p);
}

template <>
void Writer<ppc>::copyNoOps(uint8_t* from, uint8_t* to)
{
	for (uint8_t* p=from; p < to; p += 4)
		OSWriteBigInt32((uint32_t*)p, 0, 0x60000000);
}

template <>
void Writer<ppc64>::copyNoOps(uint8_t* from, uint8_t* to)
{
	for (uint8_t* p=from; p < to; p += 4)
		OSWriteBigInt32((uint32_t*)p, 0, 0x60000000);
}

template <>
void Writer<x86>::copyNoOps(uint8_t* from, uint8_t* to)
{
	for (uint8_t* p=from; p < to; ++p)
		*p = 0x90;
}

template <>
void Writer<x86_64>::copyNoOps(uint8_t* from, uint8_t* to)
{
	for (uint8_t* p=from; p < to; ++p)
		*p = 0x90;
}

template <>
void Writer<arm>::copyNoOps(uint8_t* from, uint8_t* to)
{
    // fixme: need thumb nop?
	for (uint8_t* p=from; p < to; p += 4)
		OSWriteBigInt32((uint32_t*)p, 0, 0xe1a00000);
}

static const char* stringName(const char* str)
{
	if ( strncmp(str, "cstring=", 8) == 0)  {
		static char buffer[1024];
		char* t = buffer;
		*t++ = '\"';
		for(const char*s = &str[8]; *s != '\0'; ++s) {
			switch(*s) {
				case '\n':
					*t++ = '\\';
					*t++ = 'n';
					break;
				case '\t':
					*t++ = '\\';
					*t++ = 't';
					break;
				default:
					*t++ = *s;
					break;
			}
			if ( t > &buffer[1020] ) {
				*t++= '\"';
				*t++= '.';
				*t++= '.';
				*t++= '.';
				*t++= '\0';
				return buffer;
			}
		}
		*t++= '\"';
		*t++= '\0';
		return buffer;
	}
	else {
		return str;
	}
}


template <> const char* Writer<ppc>::getArchString()    { return "ppc"; }
template <> const char* Writer<ppc64>::getArchString()  { return "ppc64"; }
template <> const char* Writer<x86>::getArchString()    { return "i386"; }
template <> const char* Writer<x86_64>::getArchString() { return "x86_64"; }
template <> const char* Writer<arm>::getArchString()    { return "arm"; }

template <typename A>
void Writer<A>::writeMap()
{
	if ( fOptions.generatedMapPath() != NULL ) {
		FILE* mapFile = fopen(fOptions.generatedMapPath(), "w"); 
		if ( mapFile != NULL ) {
			// write output path
			fprintf(mapFile, "# Path: %s\n", fFilePath);
			// write output architecure
			fprintf(mapFile, "# Arch: %s\n", getArchString());
			// write UUID
			if ( fUUIDAtom != NULL ) {
				const uint8_t* uuid = fUUIDAtom->getUUID();
				fprintf(mapFile, "# UUID: %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X \n",
					uuid[0], uuid[1], uuid[2],  uuid[3],  uuid[4],  uuid[5],  uuid[6],  uuid[7],
					uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
			}
			// write table of object files
			std::map<ObjectFile::Reader*, uint32_t> readerToOrdinal;
			std::map<uint32_t, ObjectFile::Reader*> ordinalToReader;
			std::map<ObjectFile::Reader*, uint32_t> readerToFileOrdinal;
			for (std::vector<SegmentInfo*>::iterator segit = fSegmentInfos.begin(); segit != fSegmentInfos.end(); ++segit) {
				std::vector<SectionInfo*>& sectionInfos = (*segit)->fSections;
				for (std::vector<SectionInfo*>::iterator secit = sectionInfos.begin(); secit != sectionInfos.end(); ++secit) {
					if ( ! (*secit)->fVirtualSection ) {
						std::vector<ObjectFile::Atom*>& sectionAtoms = (*secit)->fAtoms;
						for (std::vector<ObjectFile::Atom*>::iterator ait = sectionAtoms.begin(); ait != sectionAtoms.end(); ++ait) {
							ObjectFile::Reader* reader = (*ait)->getFile();
							uint32_t readerOrdinal = (*ait)->getOrdinal();
							std::map<ObjectFile::Reader*, uint32_t>::iterator pos = readerToOrdinal.find(reader);
							if ( pos == readerToOrdinal.end() ) {
								readerToOrdinal[reader] = readerOrdinal;
								ordinalToReader[readerOrdinal] = reader;
							}
						}
					}
				}
			}
			fprintf(mapFile, "# Object files:\n");
			fprintf(mapFile, "[%3u] %s\n", 0, "linker synthesized");
			uint32_t fileIndex = 0;
			readerToFileOrdinal[this] = fileIndex++;
			for(std::map<uint32_t, ObjectFile::Reader*>::iterator it = ordinalToReader.begin(); it != ordinalToReader.end(); ++it) {
				if ( it->first != 0 ) {
					fprintf(mapFile, "[%3u] %s\n", fileIndex, it->second->getPath());
					readerToFileOrdinal[it->second] = fileIndex++;
				}
			}
			// write table of sections
			fprintf(mapFile, "# Sections:\n");
			fprintf(mapFile, "# Address\tSize    \tSegment\tSection\n"); 
			for (std::vector<SegmentInfo*>::iterator segit = fSegmentInfos.begin(); segit != fSegmentInfos.end(); ++segit) {
				std::vector<SectionInfo*>& sectionInfos = (*segit)->fSections;
				for (std::vector<SectionInfo*>::iterator secit = sectionInfos.begin(); secit != sectionInfos.end(); ++secit) {
					if ( ! (*secit)->fVirtualSection ) {
						SectionInfo* sect = *secit;
						fprintf(mapFile, "0x%08llX\t0x%08llX\t%s\t%s\n", sect->getBaseAddress(), sect->fSize, 
							(*segit)->fName, sect->fSectionName);
					}
				}
			}
			// write table of symbols
			fprintf(mapFile, "# Symbols:\n");
			fprintf(mapFile, "# Address\tSize    \tFile  Name\n"); 
			for (std::vector<SegmentInfo*>::iterator segit = fSegmentInfos.begin(); segit != fSegmentInfos.end(); ++segit) {
				std::vector<SectionInfo*>& sectionInfos = (*segit)->fSections;
				for (std::vector<SectionInfo*>::iterator secit = sectionInfos.begin(); secit != sectionInfos.end(); ++secit) {
					if ( ! (*secit)->fVirtualSection ) {
						std::vector<ObjectFile::Atom*>& sectionAtoms = (*secit)->fAtoms;
						bool isCstring = (strcmp((*secit)->fSectionName, "__cstring") == 0);
						for (std::vector<ObjectFile::Atom*>::iterator ait = sectionAtoms.begin(); ait != sectionAtoms.end(); ++ait) {
							ObjectFile::Atom* atom = *ait;
							fprintf(mapFile, "0x%08llX\t0x%08llX\t[%3u] %s\n", atom->getAddress(), atom->getSize(), 
								readerToFileOrdinal[atom->getFile()], isCstring ? stringName(atom->getDisplayName()): atom->getDisplayName());
						}
					}
				}
			}
			fclose(mapFile);
		}
		else {
			warning("could not write map file: %s\n", fOptions.generatedMapPath());
		}
	}
}

static const char* sCleanupFile = NULL;
static void cleanup(int sig)
{
	::signal(sig, SIG_DFL);
	if ( sCleanupFile != NULL ) {
		::unlink(sCleanupFile);
	}
	if ( sig == SIGINT )
		::exit(1);
}


template <typename A>
uint64_t Writer<A>::writeAtoms()
{
	// for UNIX conformance, error if file exists and is not writable
	if ( (access(fFilePath, F_OK) == 0) && (access(fFilePath, W_OK) == -1) )
		throwf("can't write output file: %s", fFilePath);

	int permissions = 0777;
	if ( fOptions.outputKind() == Options::kObjectFile )
		permissions = 0666;
	// Calling unlink first assures the file is gone so that open creates it with correct permissions
	// It also handles the case where fFilePath file is not writable but its directory is
	// And it means we don't have to truncate the file when done writing (in case new is smaller than old)
	(void)unlink(fFilePath);
	
	// try to allocate buffer for entire output file content
	int fd = -1;
	SectionInfo* lastSection = fSegmentInfos.back()->fSections.back();
	uint64_t fileBufferSize = (lastSection->fFileOffset + lastSection->fSize + 4095) & (-4096);
	uint8_t* wholeBuffer = (uint8_t*)calloc(fileBufferSize, 1);
	uint8_t* atomBuffer = NULL;
	bool streaming = false;
	if ( wholeBuffer == NULL ) {
		fd = open(fFilePath, O_CREAT | O_WRONLY | O_TRUNC, permissions);
		if ( fd == -1 ) 
			throwf("can't open output file for writing: %s, errno=%d", fFilePath, errno);
		atomBuffer = new uint8_t[(fLargestAtomSize+4095) & (-4096)];
		streaming = true;
		// install signal handlers to delete output file if program is killed 
		sCleanupFile = fFilePath;
		::signal(SIGINT, cleanup);
		::signal(SIGBUS, cleanup);
		::signal(SIGSEGV, cleanup);
	}
	uint32_t size = 0;
	uint32_t end = 0;
	try {
		for (std::vector<SegmentInfo*>::iterator segit = fSegmentInfos.begin(); segit != fSegmentInfos.end(); ++segit) {
			SegmentInfo* curSegment = *segit;
			std::vector<SectionInfo*>& sectionInfos = curSegment->fSections;
			for (std::vector<SectionInfo*>::iterator secit = sectionInfos.begin(); secit != sectionInfos.end(); ++secit) {
				SectionInfo* curSection = *secit;
				std::vector<ObjectFile::Atom*>& sectionAtoms = curSection->fAtoms;
				//printf("writing with max atom size 0x%X\n", fLargestAtomSize);
				//fprintf(stderr, "writing %lu atoms for section %p %s at file offset 0x%08llX\n", sectionAtoms.size(), curSection, curSection->fSectionName, curSection->fFileOffset);
				if ( ! curSection->fAllZeroFill ) {
					bool needsNops = ((strcmp(curSection->fSegmentName, "__TEXT") == 0) && (strncmp(curSection->fSectionName, "__text", 6) == 0));
					for (std::vector<ObjectFile::Atom*>::iterator ait = sectionAtoms.begin(); ait != sectionAtoms.end(); ++ait) {
						ObjectFile::Atom* atom = *ait;
						if ( (atom->getDefinitionKind() != ObjectFile::Atom::kExternalDefinition)
						  && (atom->getDefinitionKind() != ObjectFile::Atom::kExternalWeakDefinition)
						  && (atom->getDefinitionKind() != ObjectFile::Atom::kAbsoluteSymbol) ) {
							uint32_t fileOffset = curSection->fFileOffset + atom->getSectionOffset();
							if ( fileOffset != end ) {
								//fprintf(stderr, "writing %d pad bytes, needsNops=%d\n", fileOffset-end, needsNops);
								if ( needsNops ) {
									// fill gaps with no-ops
									if ( streaming )
										writeNoOps(fd, end, fileOffset);
									else
										copyNoOps(&wholeBuffer[end], &wholeBuffer[fileOffset]);
								}
								else if ( streaming ) {
									// zero fill gaps
									if ( (fileOffset-end) == 4 ) {
										uint32_t zero = 0;
										::pwrite(fd, &zero, 4, end);
									}
									else {
										uint8_t zero = 0x00;
										for (uint32_t p=end; p < fileOffset; ++p)
											::pwrite(fd, &zero, 1, p);
									}
								}
							}
							uint64_t atomSize = atom->getSize();
							if ( streaming ) {
								if ( atomSize > fLargestAtomSize ) 
									throwf("ld64 internal error: atom \"%s\"is larger than expected 0x%X > 0x%llX", 
												atom->getDisplayName(), atomSize, fLargestAtomSize);
							}
							else {
								if ( fileOffset > fileBufferSize )
									throwf("ld64 internal error: atom \"%s\" has file offset greater thatn expceted 0x%X > 0x%llX", 
												atom->getDisplayName(), fileOffset, fileBufferSize);
							}
							uint8_t* buffer = streaming ? atomBuffer : &wholeBuffer[fileOffset];
							end = fileOffset+atomSize;
							// copy raw bytes
							atom->copyRawContent(buffer);
							// apply any fix-ups
							try {
								std::vector<ObjectFile::Reference*>&  references = atom->getReferences();
								for (std::vector<ObjectFile::Reference*>::iterator it=references.begin(); it != references.end(); it++) {
									ObjectFile::Reference* ref = *it;
									if ( fOptions.outputKind() == Options::kObjectFile ) {
										// doing ld -r
										// skip fix-ups for undefined targets
										if ( &(ref->getTarget()) != NULL )
											this->fixUpReferenceRelocatable(ref, atom, buffer);
									}
									else {
										// producing final linked image
										this->fixUpReferenceFinal(ref, atom, buffer);
									}
								}
							}
							catch (const char* msg) {
								throwf("%s in %s from %s", msg, atom->getDisplayName(), atom->getFile()->getPath());
							}
							//fprintf(stderr, "writing 0x%08X -> 0x%08X (addr=0x%llX, size=0x%llX), atom %p %s from %s\n", 
							//	fileOffset, end, atom->getAddress(), atom->getSize(), atom, atom->getDisplayName(), atom->getFile()->getPath());
							if ( streaming ) {
								// write out
								::pwrite(fd, buffer, atomSize, fileOffset);
							}
							else {
								if ( (fileOffset + atomSize) > size )
									size = fileOffset + atomSize;
							}
						}
					}
				}
			}
		}

		// update content based UUID
		if ( fOptions.getUUIDMode() == Options::kUUIDContent ) {
			uint8_t digest[CC_MD5_DIGEST_LENGTH];
			if ( streaming ) {
				// if output file file did not fit in memory, re-read file to generate md5 hash
				uint32_t kMD5BufferSize = 16*1024;
				uint8_t* md5Buffer = (uint8_t*)::malloc(kMD5BufferSize);
				if ( md5Buffer != NULL ) {
					CC_MD5_CTX md5State;
					CC_MD5_Init(&md5State);
					::lseek(fd, 0, SEEK_SET);
					ssize_t len;
					while ( (len = ::read(fd, md5Buffer, kMD5BufferSize)) > 0 ) 
						CC_MD5_Update(&md5State, md5Buffer, len);
					CC_MD5_Final(digest, &md5State);
					::free(md5Buffer);
				}
				else {
					// if malloc fails, fall back to random uuid
					::uuid_generate_random(digest);
				}
				fUUIDAtom->setContent(digest);
				uint32_t uuidOffset = ((SectionInfo*)fUUIDAtom->getSection())->fFileOffset + fUUIDAtom->getSectionOffset();
				fUUIDAtom->copyRawContent(atomBuffer);
				::pwrite(fd, atomBuffer, fUUIDAtom->getSize(), uuidOffset);
			}
			else {
				// if output file fit in memory, just genrate an md5 hash in memory
			#if 1
				// temp hack for building on Tiger
				CC_MD5_CTX md5State;
				CC_MD5_Init(&md5State);
				CC_MD5_Update(&md5State, wholeBuffer, size);
				CC_MD5_Final(digest, &md5State);
			#else
				CC_MD5(wholeBuffer, size, digest);
			#endif
				fUUIDAtom->setContent(digest);
				uint32_t uuidOffset = ((SectionInfo*)fUUIDAtom->getSection())->fFileOffset + fUUIDAtom->getSectionOffset();
				fUUIDAtom->copyRawContent(&wholeBuffer[uuidOffset]);
			}
		}
	}
	catch (...) {
		if ( sCleanupFile != NULL ) 
			::unlink(sCleanupFile);
		throw;
	}
	
	// finish up
	if ( streaming ) {
		delete [] atomBuffer;
		close(fd);
		// restore default signal handlers
		sCleanupFile = NULL;
		::signal(SIGINT, SIG_DFL);
		::signal(SIGBUS, SIG_DFL);
		::signal(SIGSEGV, SIG_DFL);
	}
	else {
		// write whole output file in one chunk
		fd = open(fFilePath, O_CREAT | O_WRONLY | O_TRUNC, permissions);
		if ( fd == -1 ) 
			throwf("can't open output file for writing: %s, errno=%d", fFilePath, errno);
		::pwrite(fd, wholeBuffer, size, 0);
		close(fd);
		delete [] wholeBuffer;
	}
	
	return end;
}

template <>
void Writer<arm>::fixUpReferenceFinal(const ObjectFile::Reference* ref, const ObjectFile::Atom* inAtom, uint8_t buffer[]) const
{
	int64_t		displacement;
	int64_t		baseAddr;
	uint32_t	instruction;
	uint32_t	newInstruction;
	uint64_t	targetAddr = 0;
	uint32_t	firstDisp;
	uint32_t	nextDisp;
	uint32_t	opcode = 0;
	bool		relocateableExternal = false;
	bool		is_bl;
	bool		is_blx;
	bool		targetIsThumb;

	if ( ref->getTargetBinding() != ObjectFile::Reference::kDontBind ) {
		targetAddr = ref->getTarget().getAddress() + ref->getTargetOffset();
		relocateableExternal = (relocationNeededInFinalLinkedImage(ref->getTarget()) == kRelocExternal);
	}

	uint32_t* fixUp = (uint32_t*)&buffer[ref->getFixUpOffset()];
	switch ( (arm::ReferenceKinds)(ref->getKind()) ) {
		case arm::kNoFixUp:
		case arm::kFollowOn:
		case arm::kGroupSubordinate:
			// do nothing
			break;
		case arm::kPointerWeakImport:
		case arm::kPointer:
			// If this is the lazy pointers section, then set all lazy pointers to
			// point to the dyld stub binding helper.
			if ( ((SectionInfo*)inAtom->getSection())->fAllLazyPointers 
			  || ((SectionInfo*)inAtom->getSection())->fAllLazyDylibPointers ) {
				switch (ref->getTarget().getDefinitionKind()) {
					case ObjectFile::Atom::kExternalDefinition:
					case ObjectFile::Atom::kExternalWeakDefinition:
						// prebound lazy pointer to another dylib ==> pointer contains zero
						LittleEndian::set32(*fixUp, 0);
						break;
					case ObjectFile::Atom::kTentativeDefinition:
					case ObjectFile::Atom::kRegularDefinition:
					case ObjectFile::Atom::kWeakDefinition:
					case ObjectFile::Atom::kAbsoluteSymbol:
						// prebound lazy pointer to withing this dylib ==> pointer contains address
						if ( ref->getTarget().isThumb() && (ref->getTargetOffset() == 0) )
							targetAddr |= 1;
						LittleEndian::set32(*fixUp, targetAddr);
						break;
				}
			}
			else if ( relocateableExternal ) {
				if ( fOptions.prebind() ) {
					switch (ref->getTarget().getDefinitionKind()) {
						case ObjectFile::Atom::kExternalDefinition:
						case ObjectFile::Atom::kExternalWeakDefinition:
							// prebound external relocation ==> pointer contains addend
							LittleEndian::set32(*fixUp, ref->getTargetOffset());
							break;
						case ObjectFile::Atom::kTentativeDefinition:
						case ObjectFile::Atom::kRegularDefinition:
						case ObjectFile::Atom::kWeakDefinition:
							// prebound external relocation to internal atom ==> pointer contains target address + addend
							if ( ref->getTarget().isThumb() && (ref->getTargetOffset() == 0) )
								targetAddr |= 1;
							LittleEndian::set32(*fixUp, targetAddr);
							break;
						case ObjectFile::Atom::kAbsoluteSymbol:
							break;
					}
				} 
				else {
					// external relocation ==> pointer contains addend
					LittleEndian::set32(*fixUp, ref->getTargetOffset());
				}
			}
			else {
				// pointer contains target address
				if ( ref->getTarget().isThumb() && (ref->getTargetOffset() == 0))
					targetAddr |= 1;
				LittleEndian::set32(*fixUp, targetAddr);
			}
			break;
		case arm::kPointerDiff:
			LittleEndian::set32(*fixUp,
				(ref->getTarget().getAddress() + ref->getTargetOffset()) - (ref->getFromTarget().getAddress() + ref->getFromTargetOffset()) );
			break;
		case arm::kReadOnlyPointer:
			if ( ref->getTarget().isThumb() && (ref->getTargetOffset() == 0))
				targetAddr |= 1;
			switch ( ref->getTarget().getDefinitionKind() ) {
				case ObjectFile::Atom::kRegularDefinition:
				case ObjectFile::Atom::kWeakDefinition:
				case ObjectFile::Atom::kTentativeDefinition:
					// pointer contains target address
					LittleEndian::set32(*fixUp, targetAddr);
					break;
				case ObjectFile::Atom::kExternalDefinition:
				case ObjectFile::Atom::kExternalWeakDefinition:
					// external relocation ==> pointer contains addend
					LittleEndian::set32(*fixUp, ref->getTargetOffset());
					break;
				case ObjectFile::Atom::kAbsoluteSymbol:
					// pointer contains target address
					LittleEndian::set32(*fixUp, targetAddr);
					break;
			}
			break;
		case arm::kBranch24WeakImport:
		case arm::kBranch24:
			displacement = targetAddr - (inAtom->getAddress() + ref->getFixUpOffset());
			// The pc added will be +8 from the pc
			displacement -= 8;
			// fprintf(stderr, "bl/blx fixup to %s at 0x%08llX, displacement = 0x%08llX\n", ref->getTarget().getDisplayName(), ref->getTarget().getAddress(), displacement);
			// max positive displacement is 0x007FFFFF << 2
			// max negative displacement is 0xFF800000 << 2
			if ( (displacement > 33554428LL) || (displacement < (-33554432LL)) ) {
				throwf("b/bl/blx out of range (%lld max is +/-32M) from %s in %s to %s in %s",
							displacement, inAtom->getDisplayName(), inAtom->getFile()->getPath(),
							ref->getTarget().getDisplayName(), ref->getTarget().getFile()->getPath());
			}
			instruction = LittleEndian::get32(*fixUp);
			// Make sure we are calling arm with bl, thumb with blx
			is_bl = ((instruction & 0xFF000000) == 0xEB000000);
			is_blx = ((instruction & 0xFE000000) == 0xFA000000);
			if ( is_bl && ref->getTarget().isThumb() ) {
				uint32_t opcode = 0xFA000000;
				uint32_t disp = (uint32_t)(displacement >> 2) & 0x00FFFFFF;
				uint32_t h_bit = (uint32_t)(displacement << 23) & 0x01000000;
				newInstruction = opcode | h_bit | disp;
			} 
			else if ( is_blx && !ref->getTarget().isThumb() ) {
				uint32_t opcode = 0xEB000000;
				uint32_t disp = (uint32_t)(displacement >> 2) & 0x00FFFFFF;
				newInstruction = opcode | disp;
			} 
			else if ( !is_bl && !is_blx && ref->getTarget().isThumb() ) {
				throwf("don't know how to convert instruction %x referencing %s to thumb",
					 instruction, ref->getTarget().getDisplayName());
			} 
			else {
				newInstruction = (instruction & 0xFF000000) | ((uint32_t)(displacement >> 2) & 0x00FFFFFF);
			}
			LittleEndian::set32(*fixUp, newInstruction);
			break;
		case arm::kThumbBranch22WeakImport:
		case arm::kThumbBranch22:
			instruction = LittleEndian::get32(*fixUp);
			is_bl = ((instruction & 0xD000F800) == 0xD000F000);
			is_blx = ((instruction & 0xD000F800) == 0xC000F000);
			targetIsThumb = ref->getTarget().isThumb();
			
			// The pc added will be +4 from the pc
			baseAddr = inAtom->getAddress() + ref->getFixUpOffset() + 4;
			// If the target is not thumb, we will be generating a blx instruction
			// Since blx cannot have the low bit set, set bit[1] of the target to
			// bit[1] of the base address, so that the difference is a multiple of
			// 4 bytes.
			if ( !targetIsThumb ) {
			  targetAddr &= -3ULL;
			  targetAddr |= (baseAddr & 2LL);
			}
			displacement = targetAddr - baseAddr;
			
			// max positive displacement is 0x003FFFFE
			// max negative displacement is 0xFFC00000
			if ( (displacement > 4194302LL) || (displacement < (-4194304LL)) ) {
				// armv7 supports a larger displacement
				if ( fOptions.preferSubArchitecture() && fOptions.subArchitecture() == CPU_SUBTYPE_ARM_V7 ) {
					if ( (displacement > 16777214) || (displacement < (-16777216LL)) ) {
						throwf("thumb bl/blx out of range (%lld max is +/-16M) from %s in %s to %s in %s",
								displacement, inAtom->getDisplayName(), inAtom->getFile()->getPath(),
								ref->getTarget().getDisplayName(), ref->getTarget().getFile()->getPath());
					}
					else {
						// The instruction is really two instructions:
						// The lower 16 bits are the first instruction, which contains the high
						//   11 bits of the displacement.
						// The upper 16 bits are the second instruction, which contains the low
						//   11 bits of the displacement, as well as differentiating bl and blx.
						uint32_t s = (uint32_t)(displacement >> 24) & 0x1;
						uint32_t i1 = (uint32_t)(displacement >> 23) & 0x1;
						uint32_t i2 = (uint32_t)(displacement >> 22) & 0x1;
						uint32_t imm10 = (uint32_t)(displacement >> 12) & 0x3FF;
						uint32_t imm11 = (uint32_t)(displacement >> 1) & 0x7FF;
						uint32_t j1 = (i1 == s);
						uint32_t j2 = (i2 == s);
						if ( is_bl ) {
							if ( targetIsThumb )
								opcode = 0xD000F000; // keep bl
							else
								opcode = 0xC000F000; // change to blx
						} 
						else if ( is_blx ) {
							if ( targetIsThumb )
								opcode = 0xD000F000; // change to bl
							else
								opcode = 0xC000F000; // keep blx
						} 
						else if ( !is_bl && !is_blx && !targetIsThumb ) {
						  throwf("don't know how to convert instruction %x referencing %s to arm",
								 instruction, ref->getTarget().getDisplayName());
						} 
						nextDisp = (j1 << 13) | (j2 << 11) | imm11;
						firstDisp = (s << 10) | imm10;
						newInstruction = opcode | (nextDisp << 16) | firstDisp;
						//warning("s=%d, j1=%d, j2=%d, imm10=0x%0X, imm11=0x%0X, opcode=0x%08X, first=0x%04X, next=0x%04X, new=0x%08X, disp=0x%llX for %s to %s\n",
						//	s, j1, j2, imm10, imm11, opcode, firstDisp, nextDisp, newInstruction, displacement, inAtom->getDisplayName(), ref->getTarget().getDisplayName());
						LittleEndian::set32(*fixUp, newInstruction);
					}
				}
				else {
					throwf("thumb bl/blx out of range (%lld max is +/-4M) from %s in %s to %s in %s",
								displacement, inAtom->getDisplayName(), inAtom->getFile()->getPath(),
								ref->getTarget().getDisplayName(), ref->getTarget().getFile()->getPath());
				}
			}
			else {
				// The instruction is really two instructions:
				// The lower 16 bits are the first instruction, which contains the high
				//   11 bits of the displacement.
				// The upper 16 bits are the second instruction, which contains the low
				//   11 bits of the displacement, as well as differentiating bl and blx.
				firstDisp = (uint32_t)(displacement >> 12) & 0x7FF;
				nextDisp = (uint32_t)(displacement >> 1) & 0x7FF;
				if ( is_bl && !targetIsThumb ) {
					opcode = 0xE800F000;
				} 
				else if ( is_blx && targetIsThumb ) {
					opcode = 0xF800F000;
				} 
				else if ( !is_bl && !is_blx && !targetIsThumb ) {
				  throwf("don't know how to convert instruction %x referencing %s to arm",
						 instruction, ref->getTarget().getDisplayName());
				} 
				else {
					opcode = instruction & 0xF800F800;
				}
				newInstruction = opcode | (nextDisp << 16) | firstDisp;
				LittleEndian::set32(*fixUp, newInstruction);
			}
			break;
		case arm::kDtraceProbeSite:
			if ( inAtom->isThumb() ) {
				// change 32-bit blx call site to two thumb NOPs
				LittleEndian::set32(*fixUp, 0x46C046C0);
			}
			else {
				// change call site to a NOP
				LittleEndian::set32(*fixUp, 0xE1A00000);
			}
			break;
		case arm::kDtraceIsEnabledSite:
			if ( inAtom->isThumb() ) {
				// change 32-bit blx call site to 'nop', 'eor r0, r0'
				LittleEndian::set32(*fixUp, 0x46C04040);
			}
			else {
				// change call site to 'eor r0, r0, r0'
				LittleEndian::set32(*fixUp, 0xE0200000);
			}
			break;
		case arm::kDtraceTypeReference:
		case arm::kDtraceProbe:
			// nothing to fix up
			break;
		default:
			throw "boom shaka laka";
	}
}

template <>
void Writer<arm>::fixUpReferenceRelocatable(const ObjectFile::Reference* ref, const ObjectFile::Atom* inAtom, uint8_t buffer[]) const
{
	int64_t		displacement;
	uint32_t	instruction;
	uint32_t	newInstruction;
	uint64_t	targetAddr = 0;
	int64_t		baseAddr;
	uint32_t	firstDisp;
	uint32_t	nextDisp;
	uint32_t	opcode = 0;
	bool		relocateableExternal = false;
	bool		is_bl;
	bool		is_blx;
	bool		targetIsThumb;

	if ( ref->getTargetBinding() != ObjectFile::Reference::kDontBind ) {
		targetAddr = ref->getTarget().getAddress() + ref->getTargetOffset();
		relocateableExternal = this->makesExternalRelocatableReference(ref->getTarget());	
	}

	uint32_t* fixUp = (uint32_t*)&buffer[ref->getFixUpOffset()];
	switch ( (arm::ReferenceKinds)(ref->getKind()) ) {
		case arm::kNoFixUp:
		case arm::kFollowOn:
		case arm::kGroupSubordinate:
			// do nothing
			break;
		case arm::kPointer:
		case arm::kReadOnlyPointer:
		case arm::kPointerWeakImport:
			{
			if ( ((SectionInfo*)inAtom->getSection())->fAllNonLazyPointers ) {
				// indirect symbol table has INDIRECT_SYMBOL_LOCAL, so we must put address in content
				if ( this->indirectSymbolInRelocatableIsLocal(ref) ) 
					LittleEndian::set32(*fixUp, targetAddr);
				else
					LittleEndian::set32(*fixUp, 0);
			}
			else if ( relocateableExternal ) {
				if ( fOptions.prebind() ) {
					switch (ref->getTarget().getDefinitionKind()) {
						case ObjectFile::Atom::kExternalDefinition:
						case ObjectFile::Atom::kExternalWeakDefinition:
							// prebound external relocation ==> pointer contains addend
							LittleEndian::set32(*fixUp, ref->getTargetOffset());
							break;
						case ObjectFile::Atom::kTentativeDefinition:
						case ObjectFile::Atom::kRegularDefinition:
						case ObjectFile::Atom::kWeakDefinition:
							// prebound external relocation to internal atom ==> pointer contains target address + addend
							LittleEndian::set32(*fixUp, targetAddr);
							break;
						case ObjectFile::Atom::kAbsoluteSymbol:
							break;
					}
				}
			}
			else {
				// internal relocation
				if ( ref->getTarget().getDefinitionKind() != ObjectFile::Atom::kTentativeDefinition ) {
					// pointer contains target address
					if ( ref->getTarget().isThumb() && (ref->getTargetOffset() == 0))
						targetAddr |= 1;
						LittleEndian::set32(*fixUp, targetAddr);
					}
					else {
						// pointer contains addend
						LittleEndian::set32(*fixUp, ref->getTargetOffset());
					}
				}
			}
			break;
		case arm::kPointerDiff:
				LittleEndian::set32(*fixUp,
					(ref->getTarget().getAddress() + ref->getTargetOffset()) - (ref->getFromTarget().getAddress() + ref->getFromTargetOffset()) );
			break;
		case arm::kDtraceProbeSite:
		case arm::kDtraceIsEnabledSite:
		case arm::kBranch24WeakImport:
		case arm::kBranch24:
			displacement = targetAddr - (inAtom->getAddress() + ref->getFixUpOffset());
			// The pc added will be +8 from the pc
			displacement -= 8;
			// fprintf(stderr, "b/bl/blx fixup to %s at 0x%08llX, displacement = 0x%08llX\n", ref->getTarget().getDisplayName(), ref->getTarget().getAddress(), displacement);
			if ( relocateableExternal )  {
				// doing "ld -r" to an external symbol
				// the mach-o way of encoding this is that the bl instruction's target addr is the offset into the target
				displacement -= ref->getTarget().getAddress();
			}
			else {
				// max positive displacement is 0x007FFFFF << 2
				// max negative displacement is 0xFF800000 << 2
				if ( (displacement > 33554428LL) || (displacement < (-33554432LL)) ) {
					throwf("arm b/bl/blx out of range (%lld max is +/-32M) from %s in %s to %s in %s",
							displacement, inAtom->getDisplayName(), inAtom->getFile()->getPath(),
							ref->getTarget().getDisplayName(), ref->getTarget().getFile()->getPath());
				}
			}
			instruction = LittleEndian::get32(*fixUp);
			// Make sure we are calling arm with bl, thumb with blx
			is_bl = ((instruction & 0xFF000000) == 0xEB000000);
			is_blx = ((instruction & 0xFE000000) == 0xFA000000);
			if ( is_bl && ref->getTarget().isThumb() ) {
				uint32_t opcode = 0xFA000000;
				uint32_t disp = (uint32_t)(displacement >> 2) & 0x00FFFFFF;
				uint32_t h_bit = (uint32_t)(displacement << 23) & 0x01000000;
				newInstruction = opcode | h_bit | disp;
			}
			else if ( is_blx && !ref->getTarget().isThumb() ) {
				uint32_t opcode = 0xEB000000;
				uint32_t disp = (uint32_t)(displacement >> 2) & 0x00FFFFFF;
				newInstruction = opcode | disp;
			} 
			else if ( !is_bl && !is_blx && ref->getTarget().isThumb() ) {
				throwf("don't know how to convert instruction %x referencing %s to thumb",
					 instruction, ref->getTarget().getDisplayName());
			} 
			else {
				newInstruction = (instruction & 0xFF000000) | ((uint32_t)(displacement >> 2) & 0x00FFFFFF);
			}
			LittleEndian::set32(*fixUp, newInstruction);
			break;
		case arm::kThumbBranch22WeakImport:
		case arm::kThumbBranch22:
			instruction = LittleEndian::get32(*fixUp);
			is_bl = ((instruction & 0xF8000000) == 0xF8000000);
			is_blx = ((instruction & 0xF8000000) == 0xE8000000);
			targetIsThumb = ref->getTarget().isThumb();
			
			// The pc added will be +4 from the pc
			baseAddr = inAtom->getAddress() + ref->getFixUpOffset() + 4;
			// If the target is not thumb, we will be generating a blx instruction
			// Since blx cannot have the low bit set, set bit[1] of the target to
			// bit[1] of the base address, so that the difference is a multiple of
			// 4 bytes.
			if (!targetIsThumb) {
				targetAddr &= -3ULL;
				targetAddr |= (baseAddr & 2LL);
			}
			displacement = targetAddr - baseAddr;
			
			//fprintf(stderr, "thumb %s fixup to %s at 0x%08llX, baseAddr = 0x%08llX, displacement = 0x%08llX, %d\n", is_blx ? "blx" : "bl",  ref->getTarget().getDisplayName(), targetAddr, baseAddr, displacement, targetIsThumb);
			if ( relocateableExternal )  {
				// doing "ld -r" to an external symbol
				// the mach-o way of encoding this is that the bl instruction's target addr is the offset into the target
				displacement -= ref->getTarget().getAddress();
			}
			
			if ( (displacement > 4194302LL) || (displacement < (-4194304LL)) ) {
				// armv7 supports a larger displacement
				if ( fOptions.preferSubArchitecture() && fOptions.subArchitecture() == CPU_SUBTYPE_ARM_V7 ) {
					if ( (displacement > 16777214) || (displacement < (-16777216LL)) ) {
						throwf("thumb bl/blx out of range (%lld max is +/-16M) from %s in %s to %s in %s",
								displacement, inAtom->getDisplayName(), inAtom->getFile()->getPath(),
								ref->getTarget().getDisplayName(), ref->getTarget().getFile()->getPath());
					}
					else {
						// The instruction is really two instructions:
						// The lower 16 bits are the first instruction, which contains the high
						//   11 bits of the displacement.
						// The upper 16 bits are the second instruction, which contains the low
						//   11 bits of the displacement, as well as differentiating bl and blx.
						uint32_t s = (uint32_t)(displacement >> 24) & 0x1;
						uint32_t i1 = (uint32_t)(displacement >> 23) & 0x1;
						uint32_t i2 = (uint32_t)(displacement >> 22) & 0x1;
						uint32_t imm10 = (uint32_t)(displacement >> 12) & 0x3FF;
						uint32_t imm11 = (uint32_t)(displacement >> 1) & 0x7FF;
						uint32_t j1 = (i1 == s);
						uint32_t j2 = (i2 == s);
						if ( is_bl ) {
							if ( targetIsThumb )
								opcode = 0xD000F000; // keep bl
							else
								opcode = 0xC000F000; // change to blx
						} 
						else if ( is_blx ) {
							if ( targetIsThumb )
								opcode = 0xD000F000; // change to bl
							else
								opcode = 0xC000F000; // keep blx
						} 
						else if ( !is_bl && !is_blx && !targetIsThumb ) {
						  throwf("don't know how to convert instruction %x referencing %s to arm",
								 instruction, ref->getTarget().getDisplayName());
						} 
						nextDisp = (j1 << 13) | (j2 << 11) | imm11;
						firstDisp = (s << 10) | imm10;
						newInstruction = opcode | (nextDisp << 16) | firstDisp;
						//warning("s=%d, j1=%d, j2=%d, imm10=0x%0X, imm11=0x%0X, opcode=0x%08X, first=0x%04X, next=0x%04X, new=0x%08X, disp=0x%llX for %s to %s\n",
						//	s, j1, j2, imm10, imm11, opcode, firstDisp, nextDisp, newInstruction, displacement, inAtom->getDisplayName(), ref->getTarget().getDisplayName());
						LittleEndian::set32(*fixUp, newInstruction);
						break;
					}
				}
				else {
					throwf("thumb bl/blx out of range (%lld max is +/-4M) from %s in %s to %s in %s",
								displacement, inAtom->getDisplayName(), inAtom->getFile()->getPath(),
								ref->getTarget().getDisplayName(), ref->getTarget().getFile()->getPath());
				}
			}
			// The instruction is really two instructions:
			// The lower 16 bits are the first instruction, which contains the first
			//   11 bits of the displacement.
			// The upper 16 bits are the second instruction, which contains the next
			//   11 bits of the displacement, as well as differentiating bl and blx.
			firstDisp = (uint32_t)(displacement >> 12) & 0x7FF;
			nextDisp = (uint32_t)(displacement >> 1) & 0x7FF;
			if ( is_bl && !targetIsThumb ) {
				opcode = 0xE800F000;
			} 
			else if ( is_blx && targetIsThumb ) {
				opcode = 0xF800F000;
			} 
			else if ( !is_bl && !is_blx && !targetIsThumb ) {
				throwf("don't know how to convert instruction %x referencing %s to arm",
					 instruction, ref->getTarget().getDisplayName());
			} 
			else {
				opcode = instruction & 0xF800F800;
			}
			newInstruction = opcode | (nextDisp << 16) | firstDisp;
			LittleEndian::set32(*fixUp, newInstruction);
			break;
		case arm::kDtraceProbe:
		case arm::kDtraceTypeReference:
			// nothing to fix up
			break;
	}
}

template <>
void Writer<x86>::fixUpReferenceFinal(const ObjectFile::Reference* ref, const ObjectFile::Atom* inAtom, uint8_t buffer[]) const
{
	uint32_t* fixUp = (uint32_t*)&buffer[ref->getFixUpOffset()];
	uint8_t*  dtraceProbeSite;
	const int64_t kTwoGigLimit = 0x7FFFFFFF;
	const int64_t kSixteenMegLimit = 0x00FFFFFF;
	const int64_t kSixtyFourKiloLimit = 0x7FFF;
	const int64_t kOneTwentyEightLimit = 0x7F;
	int64_t displacement;
	uint32_t temp;
	x86::ReferenceKinds kind = (x86::ReferenceKinds)(ref->getKind());
	switch ( kind ) {
		case x86::kNoFixUp:
		case x86::kFollowOn:
		case x86::kGroupSubordinate:
			// do nothing
			break;
		case x86::kPointerWeakImport:
		case x86::kPointer:
			{
				if ( this->relocationNeededInFinalLinkedImage(ref->getTarget()) == kRelocExternal ) {
					if ( fOptions.prebind() ) {
						switch (ref->getTarget().getDefinitionKind()) {
							case ObjectFile::Atom::kExternalDefinition:
							case ObjectFile::Atom::kExternalWeakDefinition:
								// prebound external relocation ==> pointer contains addend
								LittleEndian::set32(*fixUp, ref->getTargetOffset());
								break;
							case ObjectFile::Atom::kTentativeDefinition:
							case ObjectFile::Atom::kRegularDefinition:
							case ObjectFile::Atom::kWeakDefinition:
								// prebound external relocation to internal atom ==> pointer contains target address + addend
								LittleEndian::set32(*fixUp, ref->getTarget().getAddress() + ref->getTargetOffset());
								break;
							case ObjectFile::Atom::kAbsoluteSymbol:
								break;
						}
					} 
					else if ( !fOptions.makeClassicDyldInfo() 
									&& (ref->getTarget().getDefinitionKind() == ObjectFile::Atom::kWeakDefinition) ) {
						// when using only compressed dyld info, pointer is initially set to point directly to weak definition
						LittleEndian::set32(*fixUp, ref->getTarget().getAddress() + ref->getTargetOffset());
					}
					else {
						// external relocation ==> pointer contains addend
						LittleEndian::set32(*fixUp, ref->getTargetOffset());
					}
				}
				else {
					// pointer contains target address
					//printf("Atom::fixUpReferenceFinal() target.name=%s, target.address=0x%08llX\n", target.getDisplayName(), target.getAddress());
					LittleEndian::set32(*fixUp, ref->getTarget().getAddress() + ref->getTargetOffset());
				}
			}
			break;
		case x86::kPointerDiff:
			displacement = (ref->getTarget().getAddress() + ref->getTargetOffset()) - (ref->getFromTarget().getAddress() + ref->getFromTargetOffset());
			LittleEndian::set32(*fixUp, (uint32_t)displacement);
			break;
		case x86::kPointerDiff16:
			displacement = (ref->getTarget().getAddress() + ref->getTargetOffset()) - (ref->getFromTarget().getAddress() + ref->getFromTargetOffset());
			if ( (displacement > kSixtyFourKiloLimit) || (displacement < -(kSixtyFourKiloLimit)) ) 
				throwf("16-bit pointer diff out of range in %s", inAtom->getDisplayName());
			LittleEndian::set16(*((uint16_t*)fixUp), (uint16_t)displacement);
			break;
		case x86::kPointerDiff24:
			displacement = (ref->getTarget().getAddress() + ref->getTargetOffset()) - (ref->getFromTarget().getAddress() + ref->getFromTargetOffset());
			if ( (displacement > kSixteenMegLimit) || (displacement < 0) ) 
				throwf("24-bit pointer diff out of range in %s", inAtom->getDisplayName());
			temp = LittleEndian::get32(*fixUp);
			temp &= 0xFF000000;
			temp |= (displacement & 0x00FFFFFF);
			LittleEndian::set32(*fixUp, temp);
			break;
		case x86::kSectionOffset24:
			displacement = ref->getTarget().getSectionOffset();
			if ( (displacement > kSixteenMegLimit) || (displacement < 0) ) 
				throwf("24-bit pointer diff out of range in %s", inAtom->getDisplayName());
			temp = LittleEndian::get32(*fixUp);
			temp &= 0xFF000000;
			temp |= (displacement & 0x00FFFFFF);
			LittleEndian::set32(*fixUp, temp);
			break;
		case x86::kDtraceProbeSite:
			// change call site to a NOP
			dtraceProbeSite = (uint8_t*)fixUp;
			dtraceProbeSite[-1] = 0x90;	// 1-byte nop
			dtraceProbeSite[0] = 0x0F;	// 4-byte nop 
			dtraceProbeSite[1] = 0x1F;
			dtraceProbeSite[2] = 0x40;
			dtraceProbeSite[3] = 0x00;
			break;
		case x86::kDtraceIsEnabledSite:
			// change call site to a clear eax
			dtraceProbeSite = (uint8_t*)fixUp;
			dtraceProbeSite[-1] = 0x33;		// xorl eax,eax
			dtraceProbeSite[0] = 0xC0;
			dtraceProbeSite[1] = 0x90;		// 1-byte nop
			dtraceProbeSite[2] = 0x90;		// 1-byte nop
			dtraceProbeSite[3] = 0x90;		// 1-byte nop
			break;
		case x86::kPCRel32WeakImport:
		case x86::kPCRel32:
		case x86::kPCRel16:
		case x86::kPCRel8:
			displacement = 0;
			switch ( ref->getTarget().getDefinitionKind() ) {
				case ObjectFile::Atom::kRegularDefinition:
				case ObjectFile::Atom::kWeakDefinition:
					displacement = (ref->getTarget().getAddress() + ref->getTargetOffset()) - (inAtom->getAddress() + ref->getFixUpOffset() + 4);
					break;
				case ObjectFile::Atom::kExternalDefinition:
				case ObjectFile::Atom::kExternalWeakDefinition:
					throw "codegen problem, can't use rel32 to external symbol";
				case ObjectFile::Atom::kTentativeDefinition:
					displacement = 0;
					break;
				case ObjectFile::Atom::kAbsoluteSymbol:
					displacement = (ref->getTarget().getSectionOffset() + ref->getTargetOffset()) - (inAtom->getAddress() + ref->getFixUpOffset() + 4);
					break;
			}
			if ( kind == x86::kPCRel8 ) {
				displacement += 3;
				if ( (displacement > kOneTwentyEightLimit) || (displacement < -(kOneTwentyEightLimit)) ) {
					//fprintf(stderr, "call out of range from %s in %s to %s in %s\n", this->getDisplayName(), this->getFile()->getPath(), target.getDisplayName(), target.getFile()->getPath());
					throwf("rel8 out of range in %s", inAtom->getDisplayName());
				}
				*(int8_t*)fixUp = (int8_t)displacement;
			}
			else if ( kind == x86::kPCRel16 ) {
				displacement += 2;
				if ( (displacement > kSixtyFourKiloLimit) || (displacement < -(kSixtyFourKiloLimit)) ) {
					//fprintf(stderr, "call out of range from %s in %s to %s in %s\n", this->getDisplayName(), this->getFile()->getPath(), target.getDisplayName(), target.getFile()->getPath());
					throwf("rel16 out of range in %s", inAtom->getDisplayName());
				}
				LittleEndian::set16(*((uint16_t*)fixUp), (uint16_t)displacement);
			}
			else {
				if ( (displacement > kTwoGigLimit) || (displacement < (-kTwoGigLimit)) ) {
					//fprintf(stderr, "call out of range from %s in %s to %s in %s\n", this->getDisplayName(), this->getFile()->getPath(), target.getDisplayName(), target.getFile()->getPath());
					throwf("rel32 out of range in %s", inAtom->getDisplayName());
				}
				LittleEndian::set32(*fixUp, (int32_t)displacement);
			}
			break;
		case x86::kAbsolute32:
			switch ( ref->getTarget().getDefinitionKind() ) {
				case ObjectFile::Atom::kRegularDefinition:
				case ObjectFile::Atom::kWeakDefinition:
				case ObjectFile::Atom::kTentativeDefinition:
					// pointer contains target address
					LittleEndian::set32(*fixUp, ref->getTarget().getAddress() + ref->getTargetOffset());
					break;
				case ObjectFile::Atom::kExternalDefinition:
				case ObjectFile::Atom::kExternalWeakDefinition:
					// external relocation ==> pointer contains addend
					LittleEndian::set32(*fixUp, ref->getTargetOffset());
					break;
				case ObjectFile::Atom::kAbsoluteSymbol:
					// pointer contains target address
					LittleEndian::set32(*fixUp, ref->getTarget().getSectionOffset() + ref->getTargetOffset());
					break;
			}
			break;
		case x86::kImageOffset32:
			// offset of target atom from mach_header
			displacement = ref->getTarget().getAddress() + ref->getTargetOffset() - fMachHeaderAtom->getAddress();
			LittleEndian::set32(*fixUp, (int32_t)displacement);
			break;
		case x86::kDtraceTypeReference:
		case x86::kDtraceProbe:
			// nothing to fix up
			break;
	}
}



template <>
void Writer<x86>::fixUpReferenceRelocatable(const ObjectFile::Reference* ref, const ObjectFile::Atom* inAtom, uint8_t buffer[]) const
{
	const int64_t kTwoGigLimit = 0x7FFFFFFF;
	const int64_t kSixtyFourKiloLimit = 0x7FFF;
	const int64_t kOneTwentyEightLimit = 0x7F;
	uint32_t* fixUp = (uint32_t*)&buffer[ref->getFixUpOffset()];
	bool isExtern = this->makesExternalRelocatableReference(ref->getTarget());	
	int64_t displacement;
	x86::ReferenceKinds kind = (x86::ReferenceKinds)(ref->getKind());
	switch ( kind ) {
		case x86::kNoFixUp:
		case x86::kFollowOn:
		case x86::kGroupSubordinate:
			// do nothing
			break;
		case x86::kPointer:
		case x86::kPointerWeakImport:
		case x86::kAbsolute32:
			{
				if ( ((SectionInfo*)inAtom->getSection())->fAllNonLazyPointers ) {
					// if INDIRECT_SYMBOL_LOCAL the content is pointer, else it is zero
					if ( this->indirectSymbolInRelocatableIsLocal(ref) ) 
						LittleEndian::set32(*fixUp, ref->getTarget().getAddress() + ref->getTargetOffset());
					else
						LittleEndian::set32(*fixUp, 0);
				} 
				else if ( isExtern ) {
					// external relocation ==> pointer contains addend
					LittleEndian::set32(*fixUp, ref->getTargetOffset());
				}
				else if ( ref->getTarget().getDefinitionKind() != ObjectFile::Atom::kTentativeDefinition ) {
					// internal relocation => pointer contains target address
					LittleEndian::set32(*fixUp, ref->getTarget().getAddress() + ref->getTargetOffset());
				}
				else {
					// internal relocation to tentative ==> pointer contains addend
					LittleEndian::set32(*fixUp, ref->getTargetOffset());
				}
			}
			break;
		case x86::kPointerDiff:
				displacement = (ref->getTarget().getAddress() + ref->getTargetOffset()) - (ref->getFromTarget().getAddress() + ref->getFromTargetOffset());
				LittleEndian::set32(*fixUp, (uint32_t)displacement);
			break;
		case x86::kPointerDiff16:
				displacement = (ref->getTarget().getAddress() + ref->getTargetOffset()) - (ref->getFromTarget().getAddress() + ref->getFromTargetOffset());
				if ( (displacement > kSixtyFourKiloLimit) || (displacement < -(kSixtyFourKiloLimit)) ) 
					throwf("16-bit pointer diff out of range in %s", inAtom->getDisplayName());
				LittleEndian::set16(*((uint16_t*)fixUp), (uint16_t)displacement);
			break;
		case x86::kPCRel8:
		case x86::kPCRel16:
		case x86::kPCRel32:
		case x86::kPCRel32WeakImport:
		case x86::kDtraceProbeSite:
		case x86::kDtraceIsEnabledSite:
			{
				if ( isExtern )
					displacement = ref->getTargetOffset() - (inAtom->getAddress() + ref->getFixUpOffset() + 4);
				else
					displacement = (ref->getTarget().getAddress() + ref->getTargetOffset()) - (inAtom->getAddress() + ref->getFixUpOffset() + 4);
				if ( kind == x86::kPCRel8 ) {
					displacement += 3;
					if ( (displacement > kOneTwentyEightLimit) || (displacement < -(kOneTwentyEightLimit)) ) {
						//fprintf(stderr, "call out of range from %s in %s to %s in %s\n", this->getDisplayName(), this->getFile()->getPath(), target.getDisplayName(), target.getFile()->getPath());
						throwf("rel8 out of range (%lld)in %s", displacement, inAtom->getDisplayName());
					}
					int8_t byte = (int8_t)displacement;
					*((int8_t*)fixUp) = byte;
				}
				else if ( kind == x86::kPCRel16 ) {
					displacement += 2;
					if ( (displacement > kSixtyFourKiloLimit) || (displacement < -(kSixtyFourKiloLimit)) ) {
						//fprintf(stderr, "call out of range from %s in %s to %s in %s\n", this->getDisplayName(), this->getFile()->getPath(), target.getDisplayName(), target.getFile()->getPath());
						throwf("rel16 out of range in %s", inAtom->getDisplayName());
					}
					int16_t word = (int16_t)displacement;
					LittleEndian::set16(*((uint16_t*)fixUp), word);
				}
				else {
					if ( (displacement > kTwoGigLimit) || (displacement < (-kTwoGigLimit)) ) {
						//fprintf(stderr, "call out of range, displacement=ox%llX, from %s in %s to %s in %s\n", displacement, 
						//	inAtom->getDisplayName(), inAtom->getFile()->getPath(), ref->getTarget().getDisplayName(), ref->getTarget().getFile()->getPath());
						throwf("rel32 out of range in %s", inAtom->getDisplayName());
					}
					LittleEndian::set32(*fixUp, (int32_t)displacement);
				}
			}
			break;
		case x86::kPointerDiff24:
			throw "internal linker error, kPointerDiff24 can't be encoded into object files";
		case x86::kImageOffset32:
			throw "internal linker error, kImageOffset32 can't be encoded into object files";
		case x86::kSectionOffset24:
			throw "internal linker error, kSectionOffset24 can't be encoded into object files";
		case x86::kDtraceProbe:
		case x86::kDtraceTypeReference:
			// nothing to fix up
			break;
	}
}

template <>
void Writer<x86_64>::fixUpReferenceFinal(const ObjectFile::Reference* ref, const ObjectFile::Atom* inAtom, uint8_t buffer[]) const
{
	const int64_t twoGigLimit		= 0x7FFFFFFF;
	const int64_t kSixteenMegLimit	= 0x00FFFFFF;
	uint64_t* fixUp = (uint64_t*)&buffer[ref->getFixUpOffset()];
	uint8_t*  dtraceProbeSite;
	int64_t displacement = 0;
	uint32_t temp;
	switch ( (x86_64::ReferenceKinds)(ref->getKind()) ) {
		case x86_64::kNoFixUp:
		case x86_64::kGOTNoFixUp:
		case x86_64::kFollowOn:
		case x86_64::kGroupSubordinate:
			// do nothing
			break;
		case x86_64::kPointerWeakImport:
		case x86_64::kPointer:
			{
				if ( &ref->getTarget() != NULL ) {
					//fprintf(stderr, "fixUpReferenceFinal: %s reference to %s\n", this->getDisplayName(), target.getDisplayName());
					if ( this->relocationNeededInFinalLinkedImage(ref->getTarget()) == kRelocExternal) {
						if ( !fOptions.makeClassicDyldInfo() 
									&& (ref->getTarget().getDefinitionKind() == ObjectFile::Atom::kWeakDefinition) ) {
							// when using only compressed dyld info, pointer is initially set to point directly to weak definition
							LittleEndian::set64(*fixUp, ref->getTarget().getAddress() + ref->getTargetOffset());
						}
						else {
							// external relocation ==> pointer contains addend
							LittleEndian::set64(*fixUp, ref->getTargetOffset());
						}
					}
					else {
						// internal relocation
						// pointer contains target address
						//printf("Atom::fixUpReferenceFinal) target.name=%s, target.address=0x%08llX\n", target.getDisplayName(), target.getAddress());
						LittleEndian::set64(*fixUp, ref->getTarget().getAddress() + ref->getTargetOffset());
					}
				}
			}
			break;
		case x86_64::kPointer32:
			{
				//fprintf(stderr, "fixUpReferenceFinal: %s reference to %s\n", this->getDisplayName(), target.getDisplayName());
				if ( this->relocationNeededInFinalLinkedImage(ref->getTarget()) == kRelocExternal ) {
					// external relocation
					throwf("32-bit pointer to dylib or weak symbol %s not supported for x86_64",ref->getTarget().getDisplayName());
				}
				else {
					// internal relocation
					// pointer contains target address
					//printf("Atom::fixUpReferenceFinal) target.name=%s, target.address=0x%08llX\n", target.getDisplayName(), target.getAddress());
					displacement = ref->getTarget().getAddress() + ref->getTargetOffset();
					switch ( fOptions.outputKind() ) {
						case Options::kObjectFile:
						case Options::kPreload:
						case Options::kDyld:
						case Options::kDynamicLibrary:
						case Options::kDynamicBundle:
						case Options::kKextBundle:
							throwf("32-bit pointer to symbol %s not supported for x86_64",ref->getTarget().getDisplayName());
						case Options::kDynamicExecutable:
							// <rdar://problem/5855588> allow x86_64 main executables to use 32-bit pointers if program loads in load 2GB
							if ( (displacement > twoGigLimit) || (displacement < (-twoGigLimit)) )
								throw "32-bit pointer out of range";
							break;
						case Options::kStaticExecutable:
							// <rdar://problem/5855588> allow x86_64 mach_kernel to truncate pointers
							break;
					}
					LittleEndian::set32(*((uint32_t*)fixUp), (uint32_t)displacement);
				}
			}
			break;
		case x86_64::kPointerDiff32:
			displacement = (ref->getTarget().getAddress() + ref->getTargetOffset()) - (ref->getFromTarget().getAddress() + ref->getFromTargetOffset());
			if ( (displacement > twoGigLimit) || (displacement < (-twoGigLimit)) )
				throw "32-bit pointer difference out of range";
			LittleEndian::set32(*((uint32_t*)fixUp), (uint32_t)displacement);
			break;
		case x86_64::kPointerDiff:
			LittleEndian::set64(*fixUp,
				(ref->getTarget().getAddress() + ref->getTargetOffset()) - (ref->getFromTarget().getAddress() + ref->getFromTargetOffset()) );
			break;
		case x86_64::kPointerDiff24:
			displacement = (ref->getTarget().getAddress() + ref->getTargetOffset()) - (ref->getFromTarget().getAddress() + ref->getFromTargetOffset());
			if ( (displacement > kSixteenMegLimit) || (displacement < 0) ) 
				throwf("24-bit pointer diff out of range in %s", inAtom->getDisplayName());
			temp = LittleEndian::get32(*((uint32_t*)fixUp));
			temp &= 0xFF000000;
			temp |= (displacement & 0x00FFFFFF);
			LittleEndian::set32(*((uint32_t*)fixUp), temp);
			break;
		case x86_64::kSectionOffset24:
			displacement = ref->getTarget().getSectionOffset();
			if ( (displacement > kSixteenMegLimit) || (displacement < 0) ) 
				throwf("24-bit pointer diff out of range in %s", inAtom->getDisplayName());
			temp = LittleEndian::get32(*((uint32_t*)fixUp));
			temp &= 0xFF000000;
			temp |= (displacement & 0x00FFFFFF);
			LittleEndian::set32(*((uint32_t*)fixUp), temp);
			break;
		case x86_64::kPCRel32GOTLoad:
		case x86_64::kPCRel32GOTLoadWeakImport:
			// if GOT entry was optimized away, change movq instruction to a leaq
			if ( std::find(fAllSynthesizedNonLazyPointers.begin(), fAllSynthesizedNonLazyPointers.end(), &(ref->getTarget())) == fAllSynthesizedNonLazyPointers.end() ) {
				//fprintf(stderr, "GOT for %s optimized away\n", ref->getTarget().getDisplayName());
				uint8_t* opcodes = (uint8_t*)fixUp;
				if ( opcodes[-2] != 0x8B )
					throw "GOT load reloc does not point to a movq instruction";
				opcodes[-2] = 0x8D;
			}
			// fall into general rel32 case
		case x86_64::kBranchPCRel32WeakImport:
		case x86_64::kBranchPCRel32:
		case x86_64::kBranchPCRel8:
		case x86_64::kPCRel32:
		case x86_64::kPCRel32_1:
		case x86_64::kPCRel32_2:
		case x86_64::kPCRel32_4:
		case x86_64::kPCRel32GOT:
		case x86_64::kPCRel32GOTWeakImport:
			switch ( ref->getTarget().getDefinitionKind() ) {
				case ObjectFile::Atom::kRegularDefinition:
				case ObjectFile::Atom::kWeakDefinition:
				case ObjectFile::Atom::kTentativeDefinition:
					displacement = (ref->getTarget().getAddress() + (int32_t)ref->getTargetOffset()) - (inAtom->getAddress() + ref->getFixUpOffset() + 4);
					break;
				case ObjectFile::Atom::kAbsoluteSymbol:
					displacement = (ref->getTarget().getSectionOffset() + (int32_t)ref->getTargetOffset()) - (inAtom->getAddress() + ref->getFixUpOffset() + 4);
					break;
				case ObjectFile::Atom::kExternalDefinition:
				case ObjectFile::Atom::kExternalWeakDefinition:
					if ( fOptions.outputKind() == Options::kKextBundle )
						displacement = 0;
					else
						throwf("codegen problem, can't use rel32 to external symbol %s", ref->getTarget().getDisplayName());
					break;
			}
			switch ( ref->getKind() ) {
				case x86_64::kPCRel32_1:
					displacement -= 1;
					break;
				case x86_64::kPCRel32_2:
					displacement -= 2;
					break;
				case x86_64::kPCRel32_4:
					displacement -= 4;
					break;
				case x86_64::kBranchPCRel8:
					displacement += 3;
					break;
			}
			if ( ref->getKind() == x86_64::kBranchPCRel8 ) {
				if ( (displacement > 127) || (displacement < (-128)) ) {
					fprintf(stderr, "branch out of range from %s (%llX) in %s to %s (%llX) in %s\n", 
						inAtom->getDisplayName(), inAtom->getAddress(), inAtom->getFile()->getPath(), ref->getTarget().getDisplayName(), ref->getTarget().getAddress(), ref->getTarget().getFile()->getPath());
					throw "rel8 out of range";
				}
				*((int8_t*)fixUp) = (int8_t)displacement;
			}
			else {
				if ( (displacement > twoGigLimit) || (displacement < (-twoGigLimit)) ) {
					fprintf(stderr, "call out of range from %s (%llX) in %s to %s (%llX) in %s\n", 
						inAtom->getDisplayName(), inAtom->getAddress(), inAtom->getFile()->getPath(), ref->getTarget().getDisplayName(), ref->getTarget().getAddress(), ref->getTarget().getFile()->getPath());
					throw "rel32 out of range";
				}
				LittleEndian::set32(*((uint32_t*)fixUp), (int32_t)displacement);
			}
			break;
		case x86_64::kImageOffset32:
			// offset of target atom from mach_header
			displacement = ref->getTarget().getAddress() + ref->getTargetOffset() - fMachHeaderAtom->getAddress();
			LittleEndian::set32(*((uint32_t*)fixUp), (int32_t)displacement);
			break;
		case x86_64::kDtraceProbeSite:
			// change call site to a NOP
			dtraceProbeSite = (uint8_t*)fixUp;
			dtraceProbeSite[-1] = 0x90;	// 1-byte nop
			dtraceProbeSite[0] = 0x0F;	// 4-byte nop 
			dtraceProbeSite[1] = 0x1F;
			dtraceProbeSite[2] = 0x40;
			dtraceProbeSite[3] = 0x00;
			break;
		case x86_64::kDtraceIsEnabledSite:
			// change call site to a clear eax
			dtraceProbeSite = (uint8_t*)fixUp;
			dtraceProbeSite[-1] = 0x48;		// xorq eax,eax
			dtraceProbeSite[0] = 0x33;
			dtraceProbeSite[1] = 0xC0;		
			dtraceProbeSite[2] = 0x90;		// 1-byte nop
			dtraceProbeSite[3] = 0x90;		// 1-byte nop
			break;
		case x86_64::kDtraceTypeReference:
		case x86_64::kDtraceProbe:
			// nothing to fix up
			break;
	}
}

template <>
void Writer<x86_64>::fixUpReferenceRelocatable(const ObjectFile::Reference* ref, const ObjectFile::Atom* inAtom, uint8_t buffer[]) const
{
	const int64_t twoGigLimit		  = 0x7FFFFFFF;
	bool external = this->makesExternalRelocatableReference(ref->getTarget());
	uint64_t* fixUp = (uint64_t*)&buffer[ref->getFixUpOffset()];
	int64_t displacement = 0;
	int32_t temp32;
	switch ( (x86_64::ReferenceKinds)(ref->getKind()) ) {
		case x86_64::kNoFixUp:
		case x86_64::kGOTNoFixUp:
		case x86_64::kFollowOn:
		case x86_64::kGroupSubordinate:
			// do nothing
			break;
		case x86_64::kPointer:
		case x86_64::kPointerWeakImport:
			{
				if ( external ) {
					// external relocation ==> pointer contains addend
					LittleEndian::set64(*fixUp, ref->getTargetOffset());
				}
				else {
					// internal relocation ==> pointer contains target address
					LittleEndian::set64(*fixUp, ref->getTarget().getAddress() + ref->getTargetOffset());
				}
			}
			break;
		case x86_64::kPointer32:
			{
				if ( external ) {
					// external relocation ==> pointer contains addend
					LittleEndian::set32(*((uint32_t*)fixUp), ref->getTargetOffset());
				}
				else {
					// internal relocation ==> pointer contains target address
					LittleEndian::set32(*((uint32_t*)fixUp), ref->getTarget().getAddress() + ref->getTargetOffset());
				}
			}
			break;
		case x86_64::kPointerDiff32:
				displacement = ref->getTargetOffset() - ref->getFromTargetOffset();
				if ( ref->getTarget().getSymbolTableInclusion() == ObjectFile::Atom::kSymbolTableNotIn )
					displacement += ref->getTarget().getAddress();
				if ( ref->getFromTarget().getSymbolTableInclusion() == ObjectFile::Atom::kSymbolTableNotIn ) 
					displacement -= ref->getFromTarget().getAddress();
				LittleEndian::set32(*((uint32_t*)fixUp), displacement);
			break;
		case x86_64::kPointerDiff:
				displacement = ref->getTargetOffset() - ref->getFromTargetOffset();
				if ( ref->getTarget().getSymbolTableInclusion() == ObjectFile::Atom::kSymbolTableNotIn )
					displacement += ref->getTarget().getAddress();
				if ( ref->getFromTarget().getSymbolTableInclusion() == ObjectFile::Atom::kSymbolTableNotIn ) 
					displacement -= ref->getFromTarget().getAddress();
				LittleEndian::set64(*fixUp, displacement);
			break;
		case x86_64::kBranchPCRel32:
		case x86_64::kBranchPCRel32WeakImport:
		case x86_64::kDtraceProbeSite:
		case x86_64::kDtraceIsEnabledSite:
		case x86_64::kPCRel32:
		case x86_64::kPCRel32_1:
		case x86_64::kPCRel32_2:
		case x86_64::kPCRel32_4:
			// turn unsigned 64-bit target offset in signed 32-bit offset, since that is what source originally had
			temp32 = ref->getTargetOffset();
			if ( external ) {
				// extern relocation contains addend
				displacement = temp32;
			}
			else {
				// internal relocations contain delta to target address
				displacement = (ref->getTarget().getAddress() + temp32) - (inAtom->getAddress() + ref->getFixUpOffset() + 4);
			}
			switch ( ref->getKind() ) {
				case x86_64::kPCRel32_1:
					displacement -= 1;
					break;
				case x86_64::kPCRel32_2:
					displacement -= 2;
					break;
				case x86_64::kPCRel32_4:
					displacement -= 4;
					break;
			}
			if ( (displacement > twoGigLimit) || (displacement < (-twoGigLimit)) ) {
				//fprintf(stderr, "call out of range from %s in %s to %s in %s\n", this->getDisplayName(), this->getFile()->getPath(), target.getDisplayName(), target.getFile()->getPath());
				throw "rel32 out of range";
			}
			LittleEndian::set32(*((uint32_t*)fixUp), (int32_t)displacement);
			break;
		case x86_64::kBranchPCRel8:
			// turn unsigned 64-bit target offset in signed 32-bit offset, since that is what source originally had
			temp32 = ref->getTargetOffset();
			if ( external ) {
				// extern relocation contains addend
				displacement = temp32;
			}
			else {
				// internal relocations contain delta to target address
				displacement = (ref->getTarget().getAddress() + temp32) - (inAtom->getAddress() + ref->getFixUpOffset() + 1);
			}
			if ( (displacement > 127) || (displacement < (-128)) ) {
				//fprintf(stderr, "call out of range from %s in %s to %s in %s\n", this->getDisplayName(), this->getFile()->getPath(), target.getDisplayName(), target.getFile()->getPath());
				throw "rel8 out of range";
			}
			*((int8_t*)fixUp) = (int8_t)displacement;
			break;
		case x86_64::kPCRel32GOT:
		case x86_64::kPCRel32GOTLoad:
		case x86_64::kPCRel32GOTWeakImport:
		case x86_64::kPCRel32GOTLoadWeakImport:
			// contains addend (usually zero)
			LittleEndian::set32(*((uint32_t*)fixUp), (uint32_t)(ref->getTargetOffset()));
			break;
		case x86_64::kPointerDiff24:
			throw "internal linker error, kPointerDiff24 can't be encoded into object files";
		case x86_64::kImageOffset32:
			throw "internal linker error, kImageOffset32 can't be encoded into object files";
		case x86_64::kSectionOffset24:
			throw "internal linker error, kSectionOffset24 can't be encoded into object files";
		case x86_64::kDtraceTypeReference:
		case x86_64::kDtraceProbe:
			// nothing to fix up
			break;
	}
}

template <>
void Writer<ppc>::fixUpReferenceFinal(const ObjectFile::Reference* ref, const ObjectFile::Atom* inAtom, uint8_t buffer[]) const
{
	fixUpReference_powerpc(ref, inAtom, buffer, true);
}

template <>
void Writer<ppc64>::fixUpReferenceFinal(const ObjectFile::Reference* ref, const ObjectFile::Atom* inAtom, uint8_t buffer[]) const
{
	fixUpReference_powerpc(ref, inAtom, buffer, true);
}

template <>
void Writer<ppc>::fixUpReferenceRelocatable(const ObjectFile::Reference* ref, const ObjectFile::Atom* inAtom, uint8_t buffer[]) const
{
	fixUpReference_powerpc(ref, inAtom, buffer, false);
}

template <>
void Writer<ppc64>::fixUpReferenceRelocatable(const ObjectFile::Reference* ref, const ObjectFile::Atom* inAtom, uint8_t buffer[]) const
{
	fixUpReference_powerpc(ref, inAtom, buffer, false);
}

//
// ppc and ppc64 are mostly the same, so they share a template specialzation
//
template <typename A>
void Writer<A>::fixUpReference_powerpc(const ObjectFile::Reference* ref, const ObjectFile::Atom* inAtom, uint8_t buffer[], bool finalLinkedImage) const
{
	uint32_t	instruction;
	uint32_t	newInstruction;
	int64_t		displacement;
	uint64_t	targetAddr = 0;
	uint64_t	picBaseAddr;
	uint16_t	instructionLowHalf;
	uint16_t	instructionHighHalf;
	uint32_t*	fixUp = (uint32_t*)&buffer[ref->getFixUpOffset()];
	pint_t*		fixUpPointer = (pint_t*)&buffer[ref->getFixUpOffset()];
	bool		relocateableExternal = false;
	const int64_t picbase_twoGigLimit = 0x80000000;

	if ( ref->getTargetBinding() != ObjectFile::Reference::kDontBind ) {
		targetAddr = ref->getTarget().getAddress() + ref->getTargetOffset();
		if ( finalLinkedImage )
			relocateableExternal = (relocationNeededInFinalLinkedImage(ref->getTarget()) == kRelocExternal);
		else
			relocateableExternal = this->makesExternalRelocatableReference(ref->getTarget());	
	}

	switch ( (typename A::ReferenceKinds)(ref->getKind()) ) {
		case A::kNoFixUp:
		case A::kFollowOn:
		case A::kGroupSubordinate:
			// do nothing
			break;
		case A::kPointerWeakImport:
		case A::kPointer:
			{
				//fprintf(stderr, "fixUpReferenceFinal: %s reference to %s\n", this->getDisplayName(), target.getDisplayName());
				if ( finalLinkedImage && (((SectionInfo*)inAtom->getSection())->fAllLazyPointers 
									   || ((SectionInfo*)inAtom->getSection())->fAllLazyDylibPointers) ) {
					switch (ref->getTarget().getDefinitionKind()) {
						case ObjectFile::Atom::kExternalDefinition:
						case ObjectFile::Atom::kExternalWeakDefinition:
							// prebound lazy pointer to another dylib ==> pointer contains zero
							P::setP(*fixUpPointer, 0);
							break;
						case ObjectFile::Atom::kTentativeDefinition:
						case ObjectFile::Atom::kRegularDefinition:
						case ObjectFile::Atom::kWeakDefinition:
						case ObjectFile::Atom::kAbsoluteSymbol:
							// prebound lazy pointer to withing this dylib ==> pointer contains address
							P::setP(*fixUpPointer, targetAddr);
							break;
					}
				}
				else if ( !finalLinkedImage && ((SectionInfo*)inAtom->getSection())->fAllNonLazyPointers ) {
					// if INDIRECT_SYMBOL_LOCAL the content is pointer, else it is zero
					if ( this->indirectSymbolInRelocatableIsLocal(ref) ) 
						P::setP(*fixUpPointer, targetAddr);
					else
						P::setP(*fixUpPointer, 0);
				}
				else if ( relocateableExternal ) {
					if ( fOptions.prebind() ) {
						switch (ref->getTarget().getDefinitionKind()) {
							case ObjectFile::Atom::kExternalDefinition:
							case ObjectFile::Atom::kExternalWeakDefinition:
								// prebound external relocation ==> pointer contains addend
								P::setP(*fixUpPointer, ref->getTargetOffset());
								break;
							case ObjectFile::Atom::kTentativeDefinition:
							case ObjectFile::Atom::kRegularDefinition:
							case ObjectFile::Atom::kWeakDefinition:
								// prebound external relocation to internal atom ==> pointer contains target address + addend
								P::setP(*fixUpPointer, targetAddr);
								break;
							case ObjectFile::Atom::kAbsoluteSymbol:
								break;
						}
					} 
					else {
						// external relocation ==> pointer contains addend
						P::setP(*fixUpPointer, ref->getTargetOffset());
					}
				}
				else {
					// internal relocation
					if ( finalLinkedImage || (ref->getTarget().getDefinitionKind() != ObjectFile::Atom::kTentativeDefinition)  ) {
						// pointer contains target address
						//printf("Atom::fixUpReference_powerpc() target.name=%s, target.address=0x%08llX\n",  ref->getTarget().getDisplayName(), targetAddr);
						P::setP(*fixUpPointer, targetAddr);
					}
					else {
						// pointer contains addend
						P::setP(*fixUpPointer, ref->getTargetOffset());
					}
				}
			}
			break;
		case A::kPointerDiff64:
			P::setP(*fixUpPointer, targetAddr - (ref->getFromTarget().getAddress() + ref->getFromTargetOffset()) );
			break;
		case A::kPointerDiff32:
			P::E::set32(*fixUp, targetAddr - (ref->getFromTarget().getAddress() + ref->getFromTargetOffset()) );
			break;
		case A::kPointerDiff16:
			P::E::set16(*((uint16_t*)fixUp), targetAddr - (ref->getFromTarget().getAddress() + ref->getFromTargetOffset()) );
			break;
		case A::kDtraceProbeSite:
			if ( finalLinkedImage ) {
				// change call site to a NOP
				BigEndian::set32(*fixUp, 0x60000000);
			}
			else {
				// set  bl instuction to branch to address zero in .o file
				int64_t displacement = ref->getTargetOffset() - (inAtom->getAddress() + ref->getFixUpOffset());
				instruction = BigEndian::get32(*fixUp);
				newInstruction = (instruction & 0xFC000003) | ((uint32_t)displacement & 0x03FFFFFC);
				BigEndian::set32(*fixUp, newInstruction);
			}
			break;
		case A::kDtraceIsEnabledSite:
			if ( finalLinkedImage ) {
				// change call site to a li r3,0
				BigEndian::set32(*fixUp, 0x38600000);
			}
			else { 
				// set  bl instuction to branch to address zero in .o file
				int64_t displacement = ref->getTargetOffset() - (inAtom->getAddress() + ref->getFixUpOffset());
				instruction = BigEndian::get32(*fixUp);
				newInstruction = (instruction & 0xFC000003) | ((uint32_t)displacement & 0x03FFFFFC);
				BigEndian::set32(*fixUp, newInstruction);
			}
			break;
		case A::kBranch24WeakImport:
		case A::kBranch24:
			{
				//fprintf(stderr, "bl fixup to %s at 0x%08llX, ", target.getDisplayName(), target.getAddress());
				int64_t displacement = targetAddr - (inAtom->getAddress() + ref->getFixUpOffset());
				if ( relocateableExternal )  {
					// doing "ld -r" to an external symbol
					// the mach-o way of encoding this is that the bl instruction's target addr is the offset into the target
					displacement -= ref->getTarget().getAddress();
				}
				else {
					const int64_t bl_eightMegLimit = 0x00FFFFFF;
					if ( (displacement > bl_eightMegLimit) || (displacement < (-bl_eightMegLimit)) ) {
						//fprintf(stderr, "bl out of range (%lld max is +/-16M) from %s in %s to %s in %s\n", displacement, this->getDisplayName(), this->getFile()->getPath(), target.getDisplayName(), target.getFile()->getPath());
						throwf("bl out of range (%lld max is +/-16M) from %s at 0x%08llX in %s of %s to %s at 0x%08llX in %s of  %s",
							displacement, inAtom->getDisplayName(), inAtom->getAddress(), inAtom->getSectionName(), inAtom->getFile()->getPath(),
							ref->getTarget().getDisplayName(), ref->getTarget().getAddress(), ref->getTarget().getSectionName(), ref->getTarget().getFile()->getPath());
					}
				}
				instruction = BigEndian::get32(*fixUp);
				newInstruction = (instruction & 0xFC000003) | ((uint32_t)displacement & 0x03FFFFFC);
				//fprintf(stderr, "bl fixup: 0x%08X -> 0x%08X\n", instruction, newInstruction);
				BigEndian::set32(*fixUp, newInstruction);
			}
			break;
		case A::kBranch14:
			{
				int64_t displacement = targetAddr - (inAtom->getAddress() + ref->getFixUpOffset());
				if ( relocateableExternal )  {
					// doing "ld -r" to an external symbol
					// the mach-o way of encoding this is that the bl instruction's target addr is the offset into the target
					displacement -= ref->getTarget().getAddress();
				}
				const int64_t b_sixtyFourKiloLimit = 0x0000FFFF;
				if ( (displacement > b_sixtyFourKiloLimit) || (displacement < (-b_sixtyFourKiloLimit)) ) {
					//fprintf(stderr, "bl out of range (%lld max is +/-16M) from %s in %s to %s in %s\n", displacement, this->getDisplayName(), this->getFile()->getPath(), target.getDisplayName(), target.getFile()->getPath());
					throwf("bcc out of range (%lld max is +/-64K) from %s in %s to %s in %s",
						displacement, inAtom->getDisplayName(), inAtom->getFile()->getPath(),
						ref->getTarget().getDisplayName(), ref->getTarget().getFile()->getPath());
				}
				
				//fprintf(stderr, "bcc fixup displacement=0x%08llX, atom.addr=0x%08llX, atom.offset=0x%08X\n", displacement, inAtom->getAddress(), (uint32_t)ref->getFixUpOffset());
				instruction = BigEndian::get32(*fixUp);
				newInstruction = (instruction & 0xFFFF0003) | ((uint32_t)displacement & 0x0000FFFC);
				//fprintf(stderr, "bc fixup: 0x%08X -> 0x%08X\n", instruction, newInstruction);
				BigEndian::set32(*fixUp, newInstruction);
			}
			break;
		case A::kPICBaseLow16:
			picBaseAddr = ref->getFromTarget().getAddress() + ref->getFromTargetOffset();
			displacement = targetAddr - picBaseAddr;
			if ( (displacement > picbase_twoGigLimit) || (displacement < (-picbase_twoGigLimit)) )
				throw "32-bit pic-base out of range";
			instructionLowHalf = (displacement & 0xFFFF);
			instruction = BigEndian::get32(*fixUp);
			newInstruction = (instruction & 0xFFFF0000) | instructionLowHalf;
			BigEndian::set32(*fixUp, newInstruction);
			break;
		case A::kPICBaseLow14:
			picBaseAddr = ref->getFromTarget().getAddress() + ref->getFromTargetOffset();
			displacement = targetAddr - picBaseAddr;
			if ( (displacement > picbase_twoGigLimit) || (displacement < (-picbase_twoGigLimit)) )
				throw "32-bit pic-base out of range";
			if ( (displacement & 0x3) != 0 )
				throwf("bad offset (0x%08X) for lo14 instruction pic-base fix-up", (uint32_t)displacement);
			instructionLowHalf = (displacement & 0xFFFC);
			instruction = BigEndian::get32(*fixUp);
			newInstruction = (instruction & 0xFFFF0003) | instructionLowHalf;
			BigEndian::set32(*fixUp, newInstruction);
			break;
		case A::kPICBaseHigh16:
			picBaseAddr = ref->getFromTarget().getAddress() + ref->getFromTargetOffset();
			displacement = targetAddr - picBaseAddr;
			if ( (displacement > picbase_twoGigLimit) || (displacement < (-picbase_twoGigLimit)) )
				throw "32-bit pic-base out of range";
			instructionLowHalf = displacement >> 16;
			if ( (displacement & 0x00008000) != 0 )
				++instructionLowHalf;
			instruction = BigEndian::get32(*fixUp);
			newInstruction = (instruction & 0xFFFF0000) | instructionLowHalf;
			BigEndian::set32(*fixUp, newInstruction);
			break;
		case A::kAbsLow16:
			if ( relocateableExternal && !finalLinkedImage )
				targetAddr -= ref->getTarget().getAddress();
			instructionLowHalf = (targetAddr & 0xFFFF);
			instruction = BigEndian::get32(*fixUp);
			newInstruction = (instruction & 0xFFFF0000) | instructionLowHalf;
			BigEndian::set32(*fixUp, newInstruction);
			break;
		case A::kAbsLow14:
			if ( relocateableExternal && !finalLinkedImage )
				targetAddr -= ref->getTarget().getAddress();
			if ( (targetAddr & 0x3) != 0 )
				throw "bad address for absolute lo14 instruction fix-up";
			instructionLowHalf = (targetAddr & 0xFFFF);
			instruction = BigEndian::get32(*fixUp);
			newInstruction = (instruction & 0xFFFF0003) | instructionLowHalf;
			BigEndian::set32(*fixUp, newInstruction);
			break;
		case A::kAbsHigh16:
			if ( relocateableExternal ) {
				if ( finalLinkedImage ) {
					switch (ref->getTarget().getDefinitionKind()) {
						case ObjectFile::Atom::kExternalDefinition:
						case ObjectFile::Atom::kExternalWeakDefinition:
							throwf("absolute address to symbol %s in a different linkage unit not supported", ref->getTargetName());
							break;
						case ObjectFile::Atom::kTentativeDefinition:
						case ObjectFile::Atom::kRegularDefinition:
						case ObjectFile::Atom::kWeakDefinition:
							// use target address
							break;
						case ObjectFile::Atom::kAbsoluteSymbol:
							targetAddr = ref->getTarget().getSectionOffset();
							break;
					}
				}
				else {
					targetAddr -= ref->getTarget().getAddress();
				}
			}
			instructionHighHalf = (targetAddr >> 16);
			instruction = BigEndian::get32(*fixUp);
			newInstruction = (instruction & 0xFFFF0000) | instructionHighHalf;
			BigEndian::set32(*fixUp, newInstruction);
			break;
		case A::kAbsHigh16AddLow:
			if ( relocateableExternal ) {
				if ( finalLinkedImage ) {
					switch (ref->getTarget().getDefinitionKind()) {
						case ObjectFile::Atom::kExternalDefinition:
						case ObjectFile::Atom::kExternalWeakDefinition:
							throwf("absolute address to symbol %s in a different linkage unit not supported", ref->getTargetName());
							break;
						case ObjectFile::Atom::kTentativeDefinition:
						case ObjectFile::Atom::kRegularDefinition:
						case ObjectFile::Atom::kWeakDefinition:
							// use target address
							break;
						case ObjectFile::Atom::kAbsoluteSymbol:
							targetAddr = ref->getTarget().getSectionOffset();
							break;
					}
				}
				else {
					targetAddr -= ref->getTarget().getAddress();
				}
			}
			if ( targetAddr & 0x00008000 )
				targetAddr += 0x00010000;
			instruction = BigEndian::get32(*fixUp);
			newInstruction = (instruction & 0xFFFF0000) | (targetAddr >> 16);
			BigEndian::set32(*fixUp, newInstruction);
			break;
		case A::kDtraceTypeReference:
		case A::kDtraceProbe:
			// nothing to fix up
			break;
	}
}

template <>
bool Writer<ppc>::stubableReference(const ObjectFile::Atom* inAtom, const ObjectFile::Reference* ref)
{
	uint8_t kind = ref->getKind();
	switch ( (ppc::ReferenceKinds)kind ) {
		case ppc::kNoFixUp:
		case ppc::kFollowOn:
		case ppc::kGroupSubordinate:
		case ppc::kPointer:
		case ppc::kPointerWeakImport:
		case ppc::kPointerDiff16:
		case ppc::kPointerDiff32:
		case ppc::kPointerDiff64:
		case ppc::kDtraceProbe:
		case ppc::kDtraceProbeSite:
		case ppc::kDtraceIsEnabledSite:
		case ppc::kDtraceTypeReference:
			// these are never used to call external functions
			return false;
		case ppc::kBranch24: 
		case ppc::kBranch24WeakImport: 
		case ppc::kBranch14: 
			// these are used to call external functions
			return true;
		case ppc::kPICBaseLow16:
		case ppc::kPICBaseLow14:
		case ppc::kPICBaseHigh16:
		case ppc::kAbsLow16:
		case ppc::kAbsLow14:
		case ppc::kAbsHigh16:
		case ppc::kAbsHigh16AddLow:
			// these are only used to call external functions
			// in -mlong-branch stubs
			switch ( ref->getTarget().getDefinitionKind() ) {
				case ObjectFile::Atom::kExternalDefinition:
				case ObjectFile::Atom::kExternalWeakDefinition:
					// if the .o file this atom came from has long-branch stubs,
					// then assume these instructions in a stub.
					// Otherwise, these are a direct reference to something (maybe a runtime text reloc)
					return ( inAtom->getFile()->hasLongBranchStubs() );
				case ObjectFile::Atom::kTentativeDefinition:
				case ObjectFile::Atom::kRegularDefinition:
				case ObjectFile::Atom::kWeakDefinition:
				case ObjectFile::Atom::kAbsoluteSymbol:
					return false;
			}
			break;
	}
	return false;
}

template <>
bool Writer<arm>::stubableReference(const ObjectFile::Atom* inAtom, const ObjectFile::Reference* ref)
{
	uint8_t kind = ref->getKind();
	switch ( (arm::ReferenceKinds)kind ) {
		case arm::kBranch24:
		case arm::kBranch24WeakImport:
		case arm::kThumbBranch22:
		case arm::kThumbBranch22WeakImport:
			return true;
		case arm::kNoFixUp:
		case arm::kFollowOn:
		case arm::kGroupSubordinate:
		case arm::kPointer:
		case arm::kReadOnlyPointer:
		case arm::kPointerWeakImport:
		case arm::kPointerDiff:
		case arm::kDtraceProbe:
		case arm::kDtraceProbeSite:
		case arm::kDtraceIsEnabledSite:
		case arm::kDtraceTypeReference:
			return false;
	}
	return false;
}

template <>
bool Writer<ppc64>::stubableReference(const ObjectFile::Atom* inAtom, const ObjectFile::Reference* ref)
{
	uint8_t kind = ref->getKind();
	switch ( (ppc64::ReferenceKinds)kind ) {
		case ppc::kNoFixUp:
		case ppc::kFollowOn:
		case ppc::kGroupSubordinate:
		case ppc::kPointer:
		case ppc::kPointerWeakImport:
		case ppc::kPointerDiff16:
		case ppc::kPointerDiff32:
		case ppc::kPointerDiff64:
		case ppc::kPICBaseLow16:
		case ppc::kPICBaseLow14:
		case ppc::kPICBaseHigh16:
		case ppc::kAbsLow16:
		case ppc::kAbsLow14:
		case ppc::kAbsHigh16:
		case ppc::kAbsHigh16AddLow:
		case ppc::kDtraceProbe:
		case ppc::kDtraceProbeSite:
		case ppc::kDtraceIsEnabledSite:
		case ppc::kDtraceTypeReference:
			// these are never used to call external functions
			return false;
		case ppc::kBranch24: 
		case ppc::kBranch24WeakImport: 
		case ppc::kBranch14: 
			// these are used to call external functions
			return true;
	}
	return false;
}

template <>
bool Writer<x86>::stubableReference(const ObjectFile::Atom* inAtom, const ObjectFile::Reference* ref)
{
	uint8_t kind = ref->getKind();
	return (kind == x86::kPCRel32 || kind == x86::kPCRel32WeakImport);
}

template <>
bool Writer<x86_64>::stubableReference(const ObjectFile::Atom* inAtom, const ObjectFile::Reference* ref)
{
	uint8_t kind = ref->getKind();
	return (kind == x86_64::kBranchPCRel32 || kind == x86_64::kBranchPCRel32WeakImport);
}


template <>
bool Writer<ppc>::weakImportReferenceKind(uint8_t kind)
{
	return (kind == ppc::kBranch24WeakImport || kind == ppc::kPointerWeakImport);
}

template <>
bool Writer<ppc64>::weakImportReferenceKind(uint8_t kind)
{
	return (kind == ppc64::kBranch24WeakImport || kind == ppc64::kPointerWeakImport);
}

template <>
bool Writer<x86>::weakImportReferenceKind(uint8_t kind)
{
	return (kind == x86::kPCRel32WeakImport || kind == x86::kPointerWeakImport);
}

template <>
bool Writer<x86_64>::weakImportReferenceKind(uint8_t kind)
{
	switch ( kind ) {
		case x86_64::kPointerWeakImport:
		case x86_64::kBranchPCRel32WeakImport:
		case x86_64::kPCRel32GOTWeakImport:
		case x86_64::kPCRel32GOTLoadWeakImport:
			return true;
	}
	return false;
}

template <>
bool Writer<arm>::weakImportReferenceKind(uint8_t kind)
{
	return (kind == arm::kBranch24WeakImport || kind == arm::kThumbBranch22WeakImport ||
            kind == arm::kPointerWeakImport);
}

template <>
bool Writer<ppc>::GOTReferenceKind(uint8_t kind)
{
	return false;
}

template <>
bool Writer<ppc64>::GOTReferenceKind(uint8_t kind)
{
	return false;
}

template <>
bool Writer<x86>::GOTReferenceKind(uint8_t kind)
{
	return false;
}

template <>
bool Writer<x86_64>::GOTReferenceKind(uint8_t kind)
{
	switch ( kind ) {
		case x86_64::kPCRel32GOT:
		case x86_64::kPCRel32GOTWeakImport:
		case x86_64::kPCRel32GOTLoad:
		case x86_64::kPCRel32GOTLoadWeakImport:
		case x86_64::kGOTNoFixUp:
			return true;
	}
	return false;
}

template <>
bool Writer<arm>::GOTReferenceKind(uint8_t kind)
{
	return false;
}

template <>
bool Writer<ppc>::optimizableGOTReferenceKind(uint8_t kind)
{
	return false;
}

template <>
bool Writer<ppc64>::optimizableGOTReferenceKind(uint8_t kind)
{
	return false;
}

template <>
bool Writer<x86>::optimizableGOTReferenceKind(uint8_t kind)
{
	return false;
}

template <>
bool Writer<x86_64>::optimizableGOTReferenceKind(uint8_t kind)
{
	switch ( kind ) {
		case x86_64::kPCRel32GOTLoad:
		case x86_64::kPCRel32GOTLoadWeakImport:
			return true;
	}
	return false;
}

template <>
bool Writer<arm>::optimizableGOTReferenceKind(uint8_t kind)
{
	return false;
}

// 64-bit architectures never need module table, 32-bit sometimes do for backwards compatiblity
template <typename A> bool Writer<A>::needsModuleTable() {return fOptions.needsModuleTable(); }
template <> bool Writer<ppc64>::needsModuleTable() { return false; }
template <> bool Writer<x86_64>::needsModuleTable() { return false; }


template <typename A>
void Writer<A>::optimizeDylibReferences()
{
	//fprintf(stderr, "original ordinals table:\n");
	//for (std::map<class ObjectFile::Reader*, uint32_t>::iterator it = fLibraryToOrdinal.begin(); it != fLibraryToOrdinal.end(); ++it) {
	//	fprintf(stderr, "%u <== %p/%s\n", it->second, it->first, it->first->getPath());
	//}
	// find unused dylibs that can be removed
	std::map<uint32_t, ObjectFile::Reader*> ordinalToReader;
	std::map<ObjectFile::Reader*, ObjectFile::Reader*> readerAliases;
	for (std::map<ObjectFile::Reader*, uint32_t>::iterator it = fLibraryToOrdinal.begin(); it != fLibraryToOrdinal.end(); ++it) {	
		ObjectFile::Reader* reader = it->first;
		std::map<ObjectFile::Reader*, ObjectFile::Reader*>::iterator aliasPos = fLibraryAliases.find(reader);
		if ( aliasPos != fLibraryAliases.end() ) {
			// already noticed that this reader has same install name as another reader
			readerAliases[reader] = aliasPos->second;
		}
		else if ( !reader->providedExportAtom() && (reader->implicitlyLinked() || reader->deadStrippable() || fOptions.deadStripDylibs()) ) {
			// this reader can be optimized away
			it->second = 0xFFFFFFFF;
			typename std::map<class ObjectFile::Reader*, class DylibLoadCommandsAtom<A>* >::iterator pos = fLibraryToLoadCommand.find(reader);
			if ( pos != fLibraryToLoadCommand.end() ) 
				pos->second->optimizeAway();
		}
		else {
			// mark this reader as using it ordinal
			std::map<uint32_t, ObjectFile::Reader*>::iterator pos = ordinalToReader.find(it->second);
			if ( pos == ordinalToReader.end() ) 
				ordinalToReader[it->second] = reader;
			else
				readerAliases[reader] = pos->second;
		}
	}
	// renumber ordinals (depends on iterator walking in ordinal order)
	// all LC_LAZY_LOAD_DYLIB load commands must have highest ordinals
	uint32_t newOrdinal = 0;
	for (std::map<uint32_t, ObjectFile::Reader*>::iterator it = ordinalToReader.begin(); it != ordinalToReader.end(); ++it) {
		if ( it->first <= fLibraryToOrdinal.size() ) {
			if ( ! it->second->isLazyLoadedDylib() )
				fLibraryToOrdinal[it->second] = ++newOrdinal;
		}
	}
	for (std::map<uint32_t, ObjectFile::Reader*>::iterator it = ordinalToReader.begin(); it != ordinalToReader.end(); ++it) {
		if ( it->first <= fLibraryToOrdinal.size() ) {
			if ( it->second->isLazyLoadedDylib() ) {
				fLibraryToOrdinal[it->second] = ++newOrdinal;
			}
		}
	}
	
	// <rdar://problem/5504954> linker does not error when dylib ordinal exceeds 250
	if ( (newOrdinal >= MAX_LIBRARY_ORDINAL) && (fOptions.nameSpace() == Options::kTwoLevelNameSpace) )
		throwf("two level namespace mach-o files can link with at most %d dylibs, this link would use %d dylibs", MAX_LIBRARY_ORDINAL, newOrdinal);

	// add aliases (e.g. -lm points to libSystem.dylib)
	for (std::map<ObjectFile::Reader*, ObjectFile::Reader*>::iterator it = readerAliases.begin(); it != readerAliases.end(); ++it) {
		fLibraryToOrdinal[it->first] = fLibraryToOrdinal[it->second];
	}

	//fprintf(stderr, "new ordinals table:\n");
	//for (std::map<class ObjectFile::Reader*, uint32_t>::iterator it = fLibraryToOrdinal.begin(); it != fLibraryToOrdinal.end(); ++it) {
	//	fprintf(stderr, "%u <== %p/%s\n", it->second, it->first, it->first->getPath());
	//}
}


template <>
void Writer<arm>::scanForAbsoluteReferences()
{
	// arm codegen never has absolute references.  FIXME: Is this correct?
}

template <>
void Writer<x86_64>::scanForAbsoluteReferences()
{
	// x86_64 codegen never has absolute references
}

template <>
void  Writer<x86>::scanForAbsoluteReferences()
{
	// when linking -pie verify there are no absolute addressing, unless -read_only_relocs is also used
	if ( fOptions.positionIndependentExecutable() && !fOptions.allowTextRelocs() ) {
		for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
			ObjectFile::Atom* atom = *it;
			std::vector<ObjectFile::Reference*>&  references = atom->getReferences();
			for (std::vector<ObjectFile::Reference*>::iterator rit=references.begin(); rit != references.end(); rit++) {
				ObjectFile::Reference* ref = *rit;
				switch (ref->getKind()) {
					case x86::kAbsolute32:
						throwf("cannot link -pie: -mdynamic-no-pic codegen found in %s from %s", atom->getDisplayName(), atom->getFile()->getPath());
						return;
				}
			}
		}
	}
}

template <>
void  Writer<ppc>::scanForAbsoluteReferences()
{
	// when linking -pie verify there are no absolute addressing, unless -read_only_relocs is also used
	if ( fOptions.positionIndependentExecutable()  && !fOptions.allowTextRelocs() ) {
		for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
			ObjectFile::Atom* atom = *it;
			std::vector<ObjectFile::Reference*>&  references = atom->getReferences();
			for (std::vector<ObjectFile::Reference*>::iterator rit=references.begin(); rit != references.end(); rit++) {
				ObjectFile::Reference* ref = *rit;
				switch (ref->getKind()) {
					case ppc::kAbsLow16:
					case ppc::kAbsLow14:
					case ppc::kAbsHigh16:
					case ppc::kAbsHigh16AddLow:
						throwf("cannot link -pie: -mdynamic-no-pic codegen found in %s from %s", atom->getDisplayName(), atom->getFile()->getPath());
						return;
				}
			}
		}
	}
}


// for ppc64 look for any -mdynamic-no-pic codegen
template <>
void  Writer<ppc64>::scanForAbsoluteReferences()
{
	// only do this for main executable
	if ( mightNeedPadSegment() && (fPageZeroAtom != NULL) ) {
		for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
			ObjectFile::Atom* atom = *it;
			std::vector<ObjectFile::Reference*>&  references = atom->getReferences();
			for (std::vector<ObjectFile::Reference*>::iterator rit=references.begin(); rit != references.end(); rit++) {
				ObjectFile::Reference* ref = *rit;
				switch (ref->getKind()) {
					case ppc64::kAbsLow16:
					case ppc64::kAbsLow14:
					case ppc64::kAbsHigh16:
					case ppc64::kAbsHigh16AddLow:
						//fprintf(stderr, "found -mdynamic-no-pic codegen in %s in %s\n", atom->getDisplayName(), atom->getFile()->getPath());
						// shrink page-zero and add pad segment to compensate
						fPadSegmentInfo = new SegmentInfo(4096);
						strcpy(fPadSegmentInfo->fName, "__4GBFILL");
						fPageZeroAtom->setSize(0x1000);
						return;
				}
			}
		}
	}
}

		
template <typename A>
void Writer<A>::insertDummyStubs()
{
	// only needed for x86
}

template <>
void Writer<x86>::insertDummyStubs()
{
	// any 5-byte stubs that cross a 32-byte cache line may update incorrectly
	std::vector<class StubAtom<x86>*>	 betterStubs;
	for (std::vector<class StubAtom<x86>*>::iterator it=fAllSynthesizedStubs.begin(); it != fAllSynthesizedStubs.end(); it++) {
		switch (betterStubs.size() % 64 ) {
			case 12:// stub would occupy 0x3C->0x41
			case 25:// stub would occupy 0x7D->0x82
			case 38:// stub would occupy 0xBE->0xC3
			case 51:// stub would occupy 0xFF->0x04
				betterStubs.push_back(new StubAtom<x86>(*this, *((ObjectFile::Atom*)NULL), false)); //pad with dummy stub
				break;
		}
		betterStubs.push_back(*it);
	}
	// replace 
	fAllSynthesizedStubs.clear();
	fAllSynthesizedStubs.insert(fAllSynthesizedStubs.begin(), betterStubs.begin(), betterStubs.end());
}


template <typename A>
void Writer<A>::synthesizeKextGOT()
{
	// walk every atom and reference
	for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
		ObjectFile::Atom* atom = *it;
		std::vector<ObjectFile::Reference*>&  references = atom->getReferences();
		for (std::vector<ObjectFile::Reference*>::iterator rit=references.begin(); rit != references.end(); rit++) {
			ObjectFile::Reference* ref = *rit;
			switch ( ref->getTargetBinding()) {
				case ObjectFile::Reference::kUnboundByName:
				case ObjectFile::Reference::kDontBind:
					break;
				case ObjectFile::Reference::kBoundByName:
				case ObjectFile::Reference::kBoundDirectly:
					ObjectFile::Atom& target = ref->getTarget();
					// create GOT slots (non-lazy pointers) as needed
					if ( this->GOTReferenceKind(ref->getKind()) ) {
						bool useGOT = ( this->relocationNeededInFinalLinkedImage(ref->getTarget()) == kRelocExternal );
						// if this GOT usage cannot be optimized away then make a GOT enry
						if ( ! this->optimizableGOTReferenceKind(ref->getKind()) )
							useGOT = true;
						if ( useGOT ) {
							ObjectFile::Atom* nlp = NULL;
							std::map<ObjectFile::Atom*,ObjectFile::Atom*>::iterator pos = fGOTMap.find(&target);
							if ( pos == fGOTMap.end() ) {
								nlp = new NonLazyPointerAtom<A>(*this, target);
								fGOTMap[&target] = nlp;
							}
							else {
								nlp = pos->second;
							}
							// alter reference to use non lazy pointer instead
							ref->setTarget(*nlp, ref->getTargetOffset());
						}
					}
					// build map of which symbols need weak importing
					if ( (target.getDefinitionKind() == ObjectFile::Atom::kExternalDefinition)
						|| (target.getDefinitionKind() == ObjectFile::Atom::kExternalWeakDefinition) ) {
						if ( this->weakImportReferenceKind(ref->getKind()) ) {
							fWeakImportMap[&target] = true;
						}
					}
					break;
			}
		}
	}
	
	// add non-lazy pointers to fAllAtoms
	if ( fAllSynthesizedNonLazyPointers.size() != 0 ) {
		ObjectFile::Section* curSection = NULL;
		ObjectFile::Atom* prevAtom = NULL;
		bool inserted = false;
		for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
			ObjectFile::Atom* atom = *it;
			ObjectFile::Section* nextSection = atom->getSection();
			if ( nextSection != curSection ) {
				if ( (prevAtom != NULL) && (strcmp(prevAtom->getSectionName(), "__data") == 0) ) {
					// found end of __data section, insert lazy pointers here
					fAllAtoms->insert(it, fAllSynthesizedNonLazyPointers.begin(), fAllSynthesizedNonLazyPointers.end());
					inserted = true;
					break;
				}
				curSection = nextSection;
			}
			prevAtom = atom;
		}
		if ( !inserted ) {
			throw "can't insert non-lazy pointers, __data section not found";
		}
	}

}


template <typename A>
void Writer<A>::synthesizeStubs()
{
	switch ( fOptions.outputKind() ) {
		case Options::kObjectFile:
		case Options::kPreload:
			// these output kinds never have stubs
			return;
		case Options::kKextBundle:
			// new kext need a synthesized GOT only
			synthesizeKextGOT();
			return;
		case Options::kStaticExecutable:
		case Options::kDyld:
		case Options::kDynamicLibrary:
		case Options::kDynamicBundle:
		case Options::kDynamicExecutable:
			// try to synthesize stubs for these
			break;
	}

	// walk every atom and reference
	for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
		ObjectFile::Atom* atom = *it;
		std::vector<ObjectFile::Reference*>&  references = atom->getReferences();
		for (std::vector<ObjectFile::Reference*>::iterator rit=references.begin(); rit != references.end(); rit++) {
			ObjectFile::Reference* ref = *rit;
			switch ( ref->getTargetBinding()) {
				case ObjectFile::Reference::kUnboundByName:
				case ObjectFile::Reference::kDontBind:
					break;
				case ObjectFile::Reference::kBoundByName:
				case ObjectFile::Reference::kBoundDirectly:
					ObjectFile::Atom& target = ref->getTarget();
					// build map of which symbols need weak importing
					if ( (target.getDefinitionKind() == ObjectFile::Atom::kExternalDefinition)
						|| (target.getDefinitionKind() == ObjectFile::Atom::kExternalWeakDefinition) ) {
						bool weakImport = this->weakImportReferenceKind(ref->getKind());
						// <rdar://problem/5633081> Obj-C Symbols in Leopard Can't Be Weak Linked
						// dyld in Mac OS X 10.3 and earlier need N_WEAK_REF bit set on undefines to objc symbols
						// in dylibs that are weakly linked.  
						if ( (ref->getKind() == A::kNoFixUp) && (strncmp(target.getName(), ".objc_class_name_", 17) == 0) ) {
							typename std::map<class ObjectFile::Reader*, class DylibLoadCommandsAtom<A>* >::iterator pos;
							pos = fLibraryToLoadCommand.find(target.getFile());
							if ( pos != fLibraryToLoadCommand.end() ) {
								if ( pos->second->linkedWeak() )
									weakImport = true;
							}
						}
						// <rdar://problem/6186838> -weak_library no longer forces uses to be weak_import
						if ( fForcedWeakImportReaders.count(target.getFile()) != 0 ) {
							fWeakImportMap[&target] = true;
							weakImport = true;
						}
						
						std::map<const ObjectFile::Atom*,bool>::iterator pos = fWeakImportMap.find(&target);
						if ( pos == fWeakImportMap.end() ) {
							// target not in fWeakImportMap, so add
							fWeakImportMap[&target] = weakImport;
						}
						else {
							// target in fWeakImportMap, check for weakness mismatch
							if ( pos->second != weakImport ) {
								// found mismatch
								switch ( fOptions.weakReferenceMismatchTreatment() ) {
									case Options::kWeakReferenceMismatchError:
										throwf("mismatching weak references for symbol: %s", target.getName());
									case Options::kWeakReferenceMismatchWeak:
										pos->second = true;
										break;
									case Options::kWeakReferenceMismatchNonWeak:
										pos->second = false;
										break;
								}
							}
						}
						// update if we use a weak_import or a strong import from this dylib
						if ( fWeakImportMap[&target] )
							fDylibReadersWithWeakImports.insert(target.getFile());
						else
							fDylibReadersWithNonWeakImports.insert(target.getFile());
					}
					// create stubs as needed
					if ( this->stubableReference(atom, ref) 
						&& (ref->getTargetOffset() == 0)
						&& this->relocationNeededInFinalLinkedImage(target) == kRelocExternal ) {
						ObjectFile::Atom* stub = NULL;
						std::map<const ObjectFile::Atom*,ObjectFile::Atom*>::iterator pos = fStubsMap.find(&target);
						if ( pos == fStubsMap.end() ) {
							bool forLazyDylib = false;
							switch ( target.getDefinitionKind() ) {
								case ObjectFile::Atom::kRegularDefinition:
								case ObjectFile::Atom::kWeakDefinition:
								case ObjectFile::Atom::kAbsoluteSymbol:
								case ObjectFile::Atom::kTentativeDefinition:
									break;
								case ObjectFile::Atom::kExternalDefinition:
								case ObjectFile::Atom::kExternalWeakDefinition:
									if ( target.getFile()->isLazyLoadedDylib() )
										forLazyDylib = true;
									break;
							}
							// just-in-time, create GOT slot to dyld_stub_binder
							if ( fOptions.makeCompressedDyldInfo() && (fFastStubGOTAtom == NULL) ) {
								if ( fDyldCompressedHelperAtom == NULL )
									throw "missing symbol dyld_stub_binder";
								fFastStubGOTAtom = new NonLazyPointerAtom<A>(*this, *fDyldCompressedHelperAtom);
							}
							stub = new StubAtom<A>(*this, target, forLazyDylib);
							fStubsMap[&target] = stub;
						}
						else {
							stub = pos->second;
						}
						// alter reference to use stub instead
						ref->setTarget(*stub, 0);
					}
					else if ( fOptions.usingLazyDylibLinking() && target.getFile()->isLazyLoadedDylib() ) {
						throwf("illegal reference to %s in lazy loaded dylib from %s in %s", 
								target.getDisplayName(), atom->getDisplayName(), 
								atom->getFile()->getPath());
					}
					// create GOT slots (non-lazy pointers) as needed
					else if ( this->GOTReferenceKind(ref->getKind()) ) {
						// 
						bool mustUseGOT = ( this->relocationNeededInFinalLinkedImage(ref->getTarget()) == kRelocExternal );
						bool useGOT;
						if ( fBiggerThanTwoGigs ) {
							// in big images use GOT for all zero fill atoms
							// this is just a heuristic and may need to be re-examined
							useGOT = mustUseGOT || ref->getTarget().isZeroFill();
						}
						else {
							// < 2GB image so remove all GOT entries that we can
							useGOT = mustUseGOT;
						}
						// if this GOT usage cannot be optimized away then make a GOT enry
						if ( ! this->optimizableGOTReferenceKind(ref->getKind()) )
							useGOT = true;
						if ( useGOT ) {
							ObjectFile::Atom* nlp = NULL;
							std::map<ObjectFile::Atom*,ObjectFile::Atom*>::iterator pos = fGOTMap.find(&target);
							if ( pos == fGOTMap.end() ) {
								nlp = new NonLazyPointerAtom<A>(*this, target);
								fGOTMap[&target] = nlp;
							}
							else {
								nlp = pos->second;
							}
							// alter reference to use non lazy pointer instead
							ref->setTarget(*nlp, ref->getTargetOffset());
						}
					}
			}
		}
	}

	// sort stubs
	std::sort(fAllSynthesizedStubs.begin(), fAllSynthesizedStubs.end(), AtomByNameSorter());
	std::sort(fAllSynthesizedStubHelpers.begin(), fAllSynthesizedStubHelpers.end(), AtomByNameSorter());

	// add dummy self-modifying stubs (x86 only)
	if ( ! fOptions.makeCompressedDyldInfo() )	
		this->insertDummyStubs();

	// sort lazy pointers
	std::sort(fAllSynthesizedLazyPointers.begin(), fAllSynthesizedLazyPointers.end(), AtomByNameSorter());
	std::sort(fAllSynthesizedLazyDylibPointers.begin(), fAllSynthesizedLazyDylibPointers.end(), AtomByNameSorter());


	// add stubs to fAllAtoms
	if ( fAllSynthesizedStubs.size() != 0 ) {
		std::vector<ObjectFile::Atom*> textStubs;
		std::vector<ObjectFile::Atom*> importStubs;
		for (typename std::vector<class StubAtom<A>*>::iterator sit=fAllSynthesizedStubs.begin(); sit != fAllSynthesizedStubs.end(); ++sit) {
			ObjectFile::Atom* stubAtom = *sit;
			if ( strcmp(stubAtom->getSegment().getName(), "__TEXT") == 0 )
				textStubs.push_back(stubAtom);
			else
				importStubs.push_back(stubAtom);
		}
		// any helper stubs go right after regular stubs
		if ( fAllSynthesizedStubHelpers.size() != 0 )
			textStubs.insert(textStubs.end(), fAllSynthesizedStubHelpers.begin(), fAllSynthesizedStubHelpers.end());
		// insert text stubs right after __text section
		ObjectFile::Section* curSection = NULL;
		ObjectFile::Atom* prevAtom = NULL;
		for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
			ObjectFile::Atom* atom = *it;
			ObjectFile::Section* nextSection = atom->getSection();
			if ( nextSection != curSection ) {
				if ( (prevAtom != NULL) && (strcmp(prevAtom->getSectionName(), "__text") == 0) ) {
					// found end of __text section, insert stubs here
					fAllAtoms->insert(it, textStubs.begin(), textStubs.end());
					break;
				}
				curSection = nextSection;
			}
			prevAtom = atom;
		}
		if ( importStubs.size() != 0 ) {
			// insert __IMPORTS stubs right before __LINKEDIT
			for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
				ObjectFile::Atom* atom = *it;
				ObjectFile::Section* nextSection = atom->getSection();
				if ( nextSection != curSection ) {
					// for i386 where stubs are not in __TEXT segment
					if ( ((prevAtom != NULL) && (strcmp(prevAtom->getSegment().getName(), "__IMPORT") == 0))
						|| (strcmp(atom->getSegment().getName(), "__LINKEDIT") == 0) ) {
						// insert stubs at end of __IMPORT segment, or before __LINKEDIT
						fAllAtoms->insert(it, importStubs.begin(), importStubs.end());
						break;
					}
					curSection = nextSection;
				}
				prevAtom = atom;
			}
		}
	}


	// add non-lazy pointers to fAllAtoms
	if ( fAllSynthesizedNonLazyPointers.size() != 0 ) {
		ObjectFile::Section* curSection = NULL;
		ObjectFile::Atom* prevAtom = NULL;
		bool inserted = false;
		// first try to insert at end of __nl_symbol_ptr
		for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
			ObjectFile::Atom* atom = *it;
			ObjectFile::Section* nextSection = atom->getSection();
			if ( nextSection != curSection ) {
				if ( (prevAtom != NULL) && (strcmp(prevAtom->getSectionName(), "__nl_symbol_ptr") == 0) ) {
					// found end of __nl_symbol_ptr section, insert non-lazy pointers at end of it
					fAllAtoms->insert(it, fAllSynthesizedNonLazyPointers.begin(), fAllSynthesizedNonLazyPointers.end());
					inserted = true;
					break;
				}
				curSection = nextSection;
			}
			prevAtom = atom;
		}
		if ( !inserted ) {
			// next try to insert after __dyld section
			for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
				ObjectFile::Atom* atom = *it;
				ObjectFile::Section* nextSection = atom->getSection();
				if ( nextSection != curSection ) {
					if ( strcmp(atom->getSegment().getName(), "__DATA") == 0 ) {
						const char* prevSectionName = (prevAtom != NULL) ? prevAtom->getSectionName() : "";
						if (   (strcmp(prevSectionName, "__dyld") != 0)
							&& (strcmp(prevSectionName, "__program_vars") != 0)
							&& (strcmp(prevSectionName, "__mod_init_func") != 0) ) {
							// found end of __dyld section, insert non-lazy pointers here
							fAllAtoms->insert(it, fAllSynthesizedNonLazyPointers.begin(), fAllSynthesizedNonLazyPointers.end());
							inserted = true;
							break;
						}
					}
				}
				prevAtom = atom;
			}
			if ( !inserted ) {
				// might not be any __DATA sections, insert after end of __TEXT
				for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
					ObjectFile::Atom* atom = *it;
					ObjectFile::Section* nextSection = atom->getSection();
					if ( nextSection != curSection ) {
						if ( (prevAtom != NULL) && (strcmp(prevAtom->getSegment().getName(), "__TEXT") == 0) && (strcmp(atom->getSegment().getName(), "__TEXT") != 0)) {
							// found end of __TEXT segment, insert non-lazy pointers at end of it
							fAllAtoms->insert(it, fAllSynthesizedNonLazyPointers.begin(), fAllSynthesizedNonLazyPointers.end());
							inserted = true;
							break;
						}
						curSection = nextSection;
					}
					prevAtom = atom;
				}
			}
			if ( !inserted ) 
				throw "can't insert non-lazy pointers, __dyld section not found";
		}
	}

	// add lazy dylib pointers to fAllAtoms
	if ( fAllSynthesizedLazyDylibPointers.size() != 0 ) {
		ObjectFile::Section* curSection = NULL;
		ObjectFile::Atom* prevAtom = NULL;
		bool inserted = false;
		for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
			ObjectFile::Atom* atom = *it;
			ObjectFile::Section* nextSection = atom->getSection();
			if ( nextSection != curSection ) {
				if ( (prevAtom != NULL) && 
					(   (strcmp(prevAtom->getSectionName(), "__dyld") == 0)
					 || (strcmp(prevAtom->getSectionName(), "__program_vars") == 0)
					 || (strcmp(prevAtom->getSectionName(), "__nl_symbol_ptr") == 0) ) ) {
					// found end of __dyld section, insert lazy pointers here
					fAllAtoms->insert(it, fAllSynthesizedLazyDylibPointers.begin(), fAllSynthesizedLazyDylibPointers.end());
					inserted = true;
					break;
				}
				curSection = nextSection;
			}
			prevAtom = atom;
		}
		if ( !inserted ) {
			throw "can't insert lazy pointers, __dyld section not found";
		}
	}

	// add lazy pointers to fAllAtoms
	if ( fAllSynthesizedLazyPointers.size() != 0 ) {
		ObjectFile::Section* curSection = NULL;
		ObjectFile::Atom* prevAtom = NULL;
		bool inserted = false;
		for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
			ObjectFile::Atom* atom = *it;
			ObjectFile::Section* nextSection = atom->getSection();
			if ( nextSection != curSection ) {
				if ( (prevAtom != NULL) && 
					(   (strcmp(prevAtom->getSectionName(), "__dyld") == 0)
					 || (strcmp(prevAtom->getSectionName(), "__program_vars") == 0)
					 || (strcmp(prevAtom->getSectionName(), "__nl_symbol_ptr") == 0) ) ) {
					// found end of __dyld section, insert lazy pointers here
					fAllAtoms->insert(it, fAllSynthesizedLazyPointers.begin(), fAllSynthesizedLazyPointers.end());
					inserted = true;
					break;
				}
				curSection = nextSection;
			}
			prevAtom = atom;
		}
		if ( !inserted ) {
			throw "can't insert lazy pointers, __dyld section not found";
		}
	}
	
	
}

template <typename A>
void Writer<A>::createSplitSegContent()
{
	// build LC_SEGMENT_SPLIT_INFO once all atoms exist
	if ( fSplitCodeToDataContentAtom != NULL ) {
		for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
			ObjectFile::Atom* atom = *it;
			std::vector<ObjectFile::Reference*>&  references = atom->getReferences();
			for (std::vector<ObjectFile::Reference*>::iterator rit=references.begin(); rit != references.end(); rit++) {
				ObjectFile::Reference* ref = *rit;
				switch ( ref->getTargetBinding()) {
					case ObjectFile::Reference::kUnboundByName:
					case ObjectFile::Reference::kDontBind:
						break;
					case ObjectFile::Reference::kBoundByName:
					case ObjectFile::Reference::kBoundDirectly:
						if ( this->segmentsCanSplitApart(*atom, ref->getTarget()) ) {
							this->addCrossSegmentRef(atom, ref);
						}
						break;
				}
			}
		}
		// bad codegen may cause LC_SEGMENT_SPLIT_INFO to be removed
		adjustLoadCommandsAndPadding();
	}

}


template <typename A>
void Writer<A>::synthesizeUnwindInfoTable()
{
	if ( fUnwindInfoAtom != NULL ) {
		// walk every atom and gets its unwind info
		for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
			ObjectFile::Atom* atom = *it;
			if ( atom->beginUnwind() == atom->endUnwind() ) {
				// be sure to mark that we have no unwind info for stuff in the TEXT segment without unwind info
				if ( strcmp(atom->getSegment().getName(), "__TEXT") == 0 )
					fUnwindInfoAtom->addUnwindInfo(atom, 0, 0, NULL, NULL, NULL);
			}
			else {
				// atom has unwind 
				for ( ObjectFile::UnwindInfo::iterator uit = atom->beginUnwind(); uit != atom->endUnwind(); ++uit ) {
					fUnwindInfoAtom->addUnwindInfo(atom, uit->startOffset, uit->unwindInfo, atom->getFDE(), atom->getLSDA(), atom->getPersonalityPointer());
				}
			}
		}
	}
}



template <typename A>
void Writer<A>::partitionIntoSections()
{
	const bool oneSegmentCommand = (fOptions.outputKind() == Options::kObjectFile);

	// for every atom, set its sectionInfo object and section offset
	// build up fSegmentInfos along the way
	ObjectFile::Section* curSection = (ObjectFile::Section*)(-1);
	SectionInfo* currentSectionInfo = NULL;
	SegmentInfo* currentSegmentInfo = NULL;
	SectionInfo* cstringSectionInfo = NULL;
	unsigned int sectionIndex = 1;
	fSegmentInfos.reserve(8);
	for (unsigned int i=0; i < fAllAtoms->size(); ++i) {
		ObjectFile::Atom* atom = (*fAllAtoms)[i];
		if ( ((atom->getSection() != curSection) || (curSection==NULL)) 
			&& ((currentSectionInfo == NULL) 
				|| (strcmp(atom->getSectionName(),currentSectionInfo->fSectionName) != 0) 
				|| (strcmp(atom->getSegment().getName(),currentSectionInfo->fSegmentName) != 0)) ) {
			if ( oneSegmentCommand ) {
				if ( currentSegmentInfo == NULL ) {
					currentSegmentInfo = new SegmentInfo(fOptions.segmentAlignment());
					currentSegmentInfo->fInitProtection = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
					currentSegmentInfo->fMaxProtection = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
					this->fSegmentInfos.push_back(currentSegmentInfo);
				}
				currentSectionInfo = new SectionInfo();
				strcpy(currentSectionInfo->fSectionName, atom->getSectionName());
				strcpy(currentSectionInfo->fSegmentName, atom->getSegment().getName());
				currentSectionInfo->fAlignment = atom->getAlignment().powerOf2;
				currentSectionInfo->fAllZeroFill = atom->isZeroFill();
				currentSectionInfo->fVirtualSection = (currentSectionInfo->fSectionName[0] == '.');
				if ( !currentSectionInfo->fVirtualSection || fEmitVirtualSections )
					currentSectionInfo->setIndex(sectionIndex++);
				currentSegmentInfo->fSections.push_back(currentSectionInfo);
				if ( (strcmp(currentSectionInfo->fSegmentName, "__TEXT") == 0) && (strcmp(currentSectionInfo->fSectionName, "__cstring") == 0) ) 
					cstringSectionInfo = currentSectionInfo;
			}
			else {
				if ( (currentSegmentInfo == NULL) || (strcmp(currentSegmentInfo->fName, atom->getSegment().getName()) != 0) ) {
					currentSegmentInfo = new SegmentInfo(fOptions.segmentAlignment());
					strcpy(currentSegmentInfo->fName, atom->getSegment().getName());
					uint32_t initprot  = 0;
					if ( atom->getSegment().isContentReadable() )
						initprot |= VM_PROT_READ;
					if ( atom->getSegment().isContentWritable() )
						initprot |= VM_PROT_WRITE;
					if ( atom->getSegment().isContentExecutable() )
						initprot |= VM_PROT_EXECUTE;
					if ( fOptions.readOnlyx86Stubs() && (strcmp(atom->getSegment().getName(), "__IMPORT") == 0) )
						initprot &= ~VM_PROT_WRITE;	// hack until i386 __pointers section is synthesized by linker
					currentSegmentInfo->fInitProtection = initprot;
					if ( initprot == 0 )
						currentSegmentInfo->fMaxProtection = 0;  // pagezero should have maxprot==initprot==0
					else if ( fOptions.architecture() == CPU_TYPE_ARM )
						currentSegmentInfo->fMaxProtection = currentSegmentInfo->fInitProtection; // iPhoneOS wants max==init
					else
						currentSegmentInfo->fMaxProtection = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
					std::vector<Options::SegmentProtect>& customSegProtections = fOptions.customSegmentProtections();
					for(std::vector<Options::SegmentProtect>::iterator it = customSegProtections.begin(); it != customSegProtections.end(); ++it) {
						if ( strcmp(it->name, currentSegmentInfo->fName) == 0 ) {
							currentSegmentInfo->fInitProtection = it->init;
							currentSegmentInfo->fMaxProtection = it->max;
						}
					}
					currentSegmentInfo->fBaseAddress = atom->getSegment().getBaseAddress();
					currentSegmentInfo->fFixedAddress = atom->getSegment().hasFixedAddress();
					if ( currentSegmentInfo->fFixedAddress && (&(atom->getSegment()) == &Segment::fgStackSegment) )
						currentSegmentInfo->fIndependentAddress = true;
					if ( (fOptions.outputKind() == Options::kPreload) && (strcmp(currentSegmentInfo->fName, "__LINKEDIT")==0) )
						currentSegmentInfo->fHasLoadCommand = false;
					if ( strcmp(currentSegmentInfo->fName, "__HEADER")==0 )
						currentSegmentInfo->fHasLoadCommand = false;
					this->fSegmentInfos.push_back(currentSegmentInfo);
				}
				currentSectionInfo = new SectionInfo();
				currentSectionInfo->fAtoms.reserve(fAllAtoms->size()/4); // reduce reallocations by starting large
				strcpy(currentSectionInfo->fSectionName, atom->getSectionName());
				strcpy(currentSectionInfo->fSegmentName, atom->getSegment().getName());
				currentSectionInfo->fAlignment = atom->getAlignment().powerOf2;
				// check for -sectalign override
				std::vector<Options::SectionAlignment>&	alignmentOverrides = fOptions.sectionAlignments();
				for(std::vector<Options::SectionAlignment>::iterator it=alignmentOverrides.begin(); it != alignmentOverrides.end(); ++it) {
					if ( (strcmp(it->segmentName, currentSectionInfo->fSegmentName) == 0) && (strcmp(it->sectionName, currentSectionInfo->fSectionName) == 0) )
						currentSectionInfo->fAlignment = it->alignment;
				}
				currentSectionInfo->fAllZeroFill = atom->isZeroFill();
				currentSectionInfo->fVirtualSection = ( currentSectionInfo->fSectionName[0] == '.');
				if ( !currentSectionInfo->fVirtualSection || fEmitVirtualSections )
					currentSectionInfo->setIndex(sectionIndex++);
				currentSegmentInfo->fSections.push_back(currentSectionInfo);
			}
			//fprintf(stderr, "new section %s for atom %s\n", atom->getSectionName(), atom->getDisplayName());
			if ( strcmp(currentSectionInfo->fSectionName, "._load_commands") == 0 ) {
				fLoadCommandsSection = currentSectionInfo;
				fLoadCommandsSegment = currentSegmentInfo;
			}
			if ( (strcmp(currentSectionInfo->fSegmentName, "__DATA") == 0) && (strcmp(currentSectionInfo->fSectionName, "__la_symbol_ptr") == 0) )
				currentSectionInfo->fAllLazyPointers = true;
			if ( (strcmp(currentSectionInfo->fSegmentName, "__DATA") == 0) && (strcmp(currentSectionInfo->fSectionName, "__la_sym_ptr2") == 0) )
				currentSectionInfo->fAllLazyPointers = true;
			if ( (strcmp(currentSectionInfo->fSegmentName, "__DATA") == 0) && (strcmp(currentSectionInfo->fSectionName, "__ld_symbol_ptr") == 0) )
				currentSectionInfo->fAllLazyDylibPointers = true;
			if ( (strcmp(currentSectionInfo->fSegmentName, "__DATA") == 0) && (strcmp(currentSectionInfo->fSectionName, "__nl_symbol_ptr") == 0) )
				currentSectionInfo->fAllNonLazyPointers = true;
			if ( (strcmp(currentSectionInfo->fSegmentName, "__IMPORT") == 0) && (strcmp(currentSectionInfo->fSectionName, "__pointers") == 0) )
				currentSectionInfo->fAllNonLazyPointers = true;
			if ( (fOptions.outputKind() == Options::kDyld) && (strcmp(currentSectionInfo->fSegmentName, "__DATA") == 0) && (strcmp(currentSectionInfo->fSectionName, "__pointers") == 0) )
				currentSectionInfo->fAllNonLazyPointers = true;
			if ( (strcmp(currentSectionInfo->fSegmentName, "__TEXT") == 0) && (strcmp(currentSectionInfo->fSectionName, "__picsymbolstub1") == 0) )
				currentSectionInfo->fAllStubs = true;
			if ( (strcmp(currentSectionInfo->fSegmentName, "__TEXT") == 0) && (strcmp(currentSectionInfo->fSectionName, "__symbol_stub1") == 0) )
				currentSectionInfo->fAllStubs = true;
			if ( (strcmp(currentSectionInfo->fSegmentName, "__TEXT") == 0) && (strcmp(currentSectionInfo->fSectionName, "__picsymbolstub2") == 0) )
				currentSectionInfo->fAllStubs = true;
			if ( (strcmp(currentSectionInfo->fSegmentName, "__TEXT") == 0) && (strcmp(currentSectionInfo->fSectionName, "__symbol_stub") == 0) )
				currentSectionInfo->fAllStubs = true;
			if ( (strcmp(currentSectionInfo->fSegmentName, "__TEXT") == 0) && (strcmp(currentSectionInfo->fSectionName, "__picsymbolstub4") == 0) )
				currentSectionInfo->fAllStubs = true;
			if ( (strcmp(currentSectionInfo->fSegmentName, "__TEXT") == 0) && (strcmp(currentSectionInfo->fSectionName, "__symbol_stub4") == 0) )
				currentSectionInfo->fAllStubs = true;
			if ( (strcmp(currentSectionInfo->fSegmentName, "__IMPORT") == 0) && (strcmp(currentSectionInfo->fSectionName, "__jump_table") == 0) ) {
				currentSectionInfo->fAllSelfModifyingStubs = true;
				currentSectionInfo->fAlignment = 6; // force x86 fast stubs to start on 64-byte boundary
			}
			if ( (strcmp(currentSectionInfo->fSegmentName, "__TEXT") == 0) && (strcmp(currentSectionInfo->fSectionName, "__stub_helper") == 0) )
				currentSectionInfo->fAllStubHelpers = true;
			if ( (strcmp(currentSectionInfo->fSegmentName, "__TEXT") == 0) && (strcmp(currentSectionInfo->fSectionName, "__eh_frame") == 0) )
				currentSectionInfo->fAlignment = __builtin_ctz(sizeof(pint_t)); // always start CFI info pointer aligned
			curSection = atom->getSection();
			if ( currentSectionInfo->fAllNonLazyPointers || currentSectionInfo->fAllLazyPointers || currentSectionInfo->fAllLazyDylibPointers
				|| currentSectionInfo->fAllStubs || currentSectionInfo->fAllSelfModifyingStubs ) {
					fSymbolTableCommands->needDynamicTable();
			}
		}
		// any non-zero fill atoms make whole section marked not-zero-fill
		if ( currentSectionInfo->fAllZeroFill && ! atom->isZeroFill() )
			currentSectionInfo->fAllZeroFill = false;
		// change section object to be Writer's SectionInfo object
		atom->setSection(currentSectionInfo);
		// section alignment is that of a contained atom with the greatest alignment
		uint8_t atomAlign = atom->getAlignment().powerOf2;
		if ( currentSectionInfo->fAlignment < atomAlign ) 
			currentSectionInfo->fAlignment = atomAlign;
		// calculate section offset for this atom
		uint64_t offset = currentSectionInfo->fSize;
		uint64_t alignment = 1 << atomAlign;
		uint64_t currentModulus = (offset % alignment);
		uint64_t requiredModulus = atom->getAlignment().modulus;
		if ( currentModulus != requiredModulus ) {
			if ( requiredModulus > currentModulus )
				offset += requiredModulus-currentModulus;
			else
				offset += requiredModulus+alignment-currentModulus;		
		}
		atom->setSectionOffset(offset);
		uint64_t curAtomSize = atom->getSize();
		currentSectionInfo->fSize = offset + curAtomSize;
		// add atom to section vector
		currentSectionInfo->fAtoms.push_back(atom);
		//fprintf(stderr, "  adding atom %p %s size=0x%0llX to section %p %s from %s\n", atom, atom->getDisplayName(), atom->getSize(),
		//				currentSectionInfo, currentSectionInfo->fSectionName, atom->getFile()->getPath());
		// update largest size
		if ( !currentSectionInfo->fAllZeroFill && (curAtomSize > fLargestAtomSize) )
			fLargestAtomSize = curAtomSize;
	}
	if ( (cstringSectionInfo != NULL) && (cstringSectionInfo->fAlignment > 0) ) {
		// when merging cstring sections in .o files, all strings need to use the max alignment
		uint64_t offset = 0;
		uint64_t cstringAlignment = 1 << cstringSectionInfo->fAlignment;
		for (std::vector<ObjectFile::Atom*>::iterator it=cstringSectionInfo->fAtoms.begin(); it != cstringSectionInfo->fAtoms.end(); it++) {
			offset = (offset + (cstringAlignment-1)) & (-cstringAlignment);
			ObjectFile::Atom* atom = *it;
			atom->setSectionOffset(offset);
			offset += atom->getSize();
		}
		cstringSectionInfo->fSize = offset;
	}
}


struct TargetAndOffset { ObjectFile::Atom* atom; uint32_t offset; };
class TargetAndOffsetComparor
{
public:
	bool operator()(const TargetAndOffset& left, const TargetAndOffset& right) const
	{
		if ( left.atom != right.atom )
			return ( left.atom < right.atom );
		return ( left.offset < right.offset );
	}
};

template <>
bool Writer<ppc>::addBranchIslands()
{
	return this->addPPCBranchIslands();
}

template <>
bool Writer<ppc64>::addBranchIslands()
{
	return this->addPPCBranchIslands();
}

template <>
bool Writer<x86>::addBranchIslands()
{
	// x86 branches can reach entire 4G address space, so no need for branch islands
	return false;
}

template <>
bool Writer<x86_64>::addBranchIslands()
{
	// x86 branches can reach entire 4G size of largest image
	return false;
}

template <>
bool Writer<arm>::addBranchIslands()
{
	// arm branch islands not (yet) supported
	// you can instead compile with -mlong-call
	return false;
}

template <>
bool Writer<ppc>::isBranch24Reference(uint8_t kind)
{
	switch (kind) {
		case ppc::kBranch24:
		case ppc::kBranch24WeakImport:
			return true;
	}
	return false;
}

template <>
bool Writer<ppc64>::isBranch24Reference(uint8_t kind)
{
	switch (kind) {
		case ppc64::kBranch24:
		case ppc64::kBranch24WeakImport:
			return true;
	}
	return false;
}

//
// PowerPC can do PC relative branches as far as +/-16MB.
// If a branch target is >16MB then we insert one or more
// "branch islands" between the branch and its target that
// allows island hoping to the target.
//
// Branch Island Algorithm
//
// If the __TEXT segment < 16MB, then no branch islands needed
// Otherwise, every 14MB into the __TEXT segment a region is
// added which can contain branch islands.  Every out of range
// bl instruction is checked.  If it crosses a region, an island
// is added to that region with the same target and the bl is
// adjusted to target the island instead.
//
// In theory, if too many islands are added to one region, it
// could grow the __TEXT enough that other previously in-range
// bl branches could be pushed out of range.  We reduce the
// probability this could happen by placing the ranges every
// 15MB which means the region would have to be 1MB (256K islands)
// before any branches could be pushed out of range.
//
template <typename A>
bool Writer<A>::addPPCBranchIslands()
{
	bool log = false;
	bool result = false;
	// Can only possibly need branch islands if __TEXT segment > 16M
	if ( fLoadCommandsSegment->fSize > 16000000 ) {
		if ( log) fprintf(stderr, "ld: checking for branch islands, __TEXT segment size=%llu\n", fLoadCommandsSegment->fSize);
		const uint32_t kBetweenRegions = 14*1024*1024; // place regions of islands every 14MB in __text section
		SectionInfo* textSection = NULL;
		for (std::vector<SectionInfo*>::iterator it=fLoadCommandsSegment->fSections.begin(); it != fLoadCommandsSegment->fSections.end(); it++) {
			if ( strcmp((*it)->fSectionName, "__text") == 0 ) {
				textSection = *it;
				if ( log) fprintf(stderr, "ld: checking for branch islands, __text section size=%llu\n", textSection->fSize);
				break;
			}
		}
		const int kIslandRegionsCount = fLoadCommandsSegment->fSize / kBetweenRegions;
		typedef std::map<TargetAndOffset,ObjectFile::Atom*, TargetAndOffsetComparor> AtomToIsland;
		AtomToIsland regionsMap[kIslandRegionsCount];
		std::vector<ObjectFile::Atom*> regionsIslands[kIslandRegionsCount];
		unsigned int islandCount = 0;
		if ( log) fprintf(stderr, "ld: will use %u branch island regions\n", kIslandRegionsCount);

		// create islands for branch references that are out of range
		for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
			ObjectFile::Atom* atom = *it;
			std::vector<ObjectFile::Reference*>&  references = atom->getReferences();
			for (std::vector<ObjectFile::Reference*>::iterator rit=references.begin(); rit != references.end(); rit++) {
				ObjectFile::Reference* ref = *rit;
				if ( this->isBranch24Reference(ref->getKind()) ) {
					ObjectFile::Atom& target = ref->getTarget();
					int64_t srcAddr = atom->getAddress() + ref->getFixUpOffset();
					int64_t dstAddr = target.getAddress() + ref->getTargetOffset();
					int64_t displacement = dstAddr - srcAddr;
					TargetAndOffset finalTargetAndOffset = { &target, ref->getTargetOffset() };
					const int64_t kFifteenMegLimit = kBetweenRegions;
					if ( displacement > kFifteenMegLimit ) {
						// create forward branch chain
						ObjectFile::Atom* nextTarget = &target;
						uint64_t nextTargetOffset = ref->getTargetOffset();
						for (int i=kIslandRegionsCount-1; i >=0 ; --i) {
							AtomToIsland* region = &regionsMap[i];
							int64_t islandRegionAddr = kBetweenRegions * (i+1) + textSection->getBaseAddress();
							if ( (srcAddr < islandRegionAddr) && (islandRegionAddr <= dstAddr) ) { 
								AtomToIsland::iterator pos = region->find(finalTargetAndOffset);
								if ( pos == region->end() ) {
									BranchIslandAtom<A>* island = new BranchIslandAtom<A>(*this, target.getDisplayName(), i, *nextTarget, nextTargetOffset);
									island->setSection(textSection);
									(*region)[finalTargetAndOffset] = island;
									if (log) fprintf(stderr, "added island %s to region %d for %s\n", island->getDisplayName(), i, atom->getDisplayName());
									regionsIslands[i].push_back(island);
									++islandCount;
									nextTarget = island;
									nextTargetOffset = 0;
								}
								else {
									nextTarget = pos->second;
									nextTargetOffset = 0;
								}
							}
						}
						if (log) fprintf(stderr, "using island %s for branch to %s from %s\n", nextTarget->getDisplayName(), target.getDisplayName(), atom->getDisplayName());
						ref->setTarget(*nextTarget, nextTargetOffset);
					}
					else if ( displacement < (-kFifteenMegLimit) ) {
						// create back branching chain
						ObjectFile::Atom* prevTarget = &target;
						uint64_t prevTargetOffset = ref->getTargetOffset();
						for (int i=0; i < kIslandRegionsCount ; ++i) {
							AtomToIsland* region = &regionsMap[i];
							int64_t islandRegionAddr = kBetweenRegions * (i+1);
							if ( (dstAddr <= islandRegionAddr) && (islandRegionAddr < srcAddr) ) {
								AtomToIsland::iterator pos = region->find(finalTargetAndOffset);
								if ( pos == region->end() ) {
									BranchIslandAtom<A>* island = new BranchIslandAtom<A>(*this, target.getDisplayName(), i, *prevTarget, prevTargetOffset);
									island->setSection(textSection);
									(*region)[finalTargetAndOffset] = island;
									if (log) fprintf(stderr, "added back island %s to region %d for %s\n", island->getDisplayName(), i, atom->getDisplayName());
									regionsIslands[i].push_back(island);
									++islandCount;
									prevTarget = island;
									prevTargetOffset = 0;
								}
								else {
									prevTarget = pos->second;
									prevTargetOffset = 0;
								}
							}
						}
						if (log) fprintf(stderr, "using back island %s for %s\n", prevTarget->getDisplayName(), atom->getDisplayName());
						ref->setTarget(*prevTarget, prevTargetOffset);
					}
				}
			}
		}

		// insert islands into __text section and adjust section offsets
		if ( islandCount > 0 ) {
			if ( log ) fprintf(stderr, "ld: %u branch islands required in %u regions\n", islandCount, kIslandRegionsCount);
			std::vector<ObjectFile::Atom*> newAtomList;
			newAtomList.reserve(textSection->fAtoms.size()+islandCount);
			uint64_t islandRegionAddr = kBetweenRegions + textSection->getBaseAddress();
			uint64_t textSectionAlignment = (1 << textSection->fAlignment);
			int regionIndex = 0;
			uint64_t atomSlide = 0;
			uint64_t sectionOffset = 0;
			for (std::vector<ObjectFile::Atom*>::iterator it=textSection->fAtoms.begin(); it != textSection->fAtoms.end(); it++) {
				ObjectFile::Atom* atom = *it;
				if ( (atom->getAddress()+atom->getSize()) > islandRegionAddr ) {
					uint64_t islandStartOffset = atom->getSectionOffset() + atomSlide;
					sectionOffset = islandStartOffset;
					std::vector<ObjectFile::Atom*>* regionIslands = &regionsIslands[regionIndex];
					for (std::vector<ObjectFile::Atom*>::iterator rit=regionIslands->begin(); rit != regionIslands->end(); rit++) {
						ObjectFile::Atom* islandAtom = *rit;
						newAtomList.push_back(islandAtom);
						uint64_t alignment = 1 << (islandAtom->getAlignment().powerOf2);
						sectionOffset = ( (sectionOffset+alignment-1) & (-alignment) );
						islandAtom->setSectionOffset(sectionOffset);
						sectionOffset += islandAtom->getSize();
					}
					++regionIndex;
					islandRegionAddr += kBetweenRegions;
					uint64_t islandRegionAlignmentBlocks = (sectionOffset - islandStartOffset + textSectionAlignment - 1) / textSectionAlignment;
					atomSlide += (islandRegionAlignmentBlocks * textSectionAlignment);
				}
				newAtomList.push_back(atom);
				if ( atomSlide != 0 )
					atom->setSectionOffset(atom->getSectionOffset()+atomSlide);
			}
			sectionOffset = textSection->fSize+atomSlide;
			// put any remaining islands at end of __text section
			if ( regionIndex < kIslandRegionsCount ) {
				std::vector<ObjectFile::Atom*>* regionIslands = &regionsIslands[regionIndex];
				for (std::vector<ObjectFile::Atom*>::iterator rit=regionIslands->begin(); rit != regionIslands->end(); rit++) {
					ObjectFile::Atom* islandAtom = *rit;
					newAtomList.push_back(islandAtom);
					uint64_t alignment = 1 << (islandAtom->getAlignment().powerOf2);
					sectionOffset = ( (sectionOffset+alignment-1) & (-alignment) );
					islandAtom->setSectionOffset(sectionOffset);
					sectionOffset += islandAtom->getSize();
				}
			}

			textSection->fAtoms = newAtomList;
			textSection->fSize = sectionOffset;
			result = true;
		}

	}
	return result;
}


template <typename A>
void Writer<A>::adjustLoadCommandsAndPadding()
{
	fSegmentCommands->computeSize();

	// recompute load command section offsets
	uint64_t offset = 0;
	std::vector<class ObjectFile::Atom*>& loadCommandAtoms = fLoadCommandsSection->fAtoms;
	const unsigned int atomCount = loadCommandAtoms.size();
	for (unsigned int i=0; i < atomCount; ++i) {
		ObjectFile::Atom* atom = loadCommandAtoms[i];
		uint64_t alignment = 1 << atom->getAlignment().powerOf2;
		offset = ( (offset+alignment-1) & (-alignment) );
		atom->setSectionOffset(offset);
		uint32_t atomSize = atom->getSize();
		if ( atomSize > fLargestAtomSize )
			fLargestAtomSize = atomSize;
		offset += atomSize;
		fLoadCommandsSection->fSize = offset;
	}

	std::vector<SectionInfo*>& sectionInfos = fLoadCommandsSegment->fSections;
	const int sectionCount = sectionInfos.size();
	uint32_t totalSizeOfTEXTLessHeaderAndLoadCommands = 0;
	for(int j=0; j < sectionCount; ++j) {
		SectionInfo* curSection = sectionInfos[j];
		if ( strcmp(curSection->fSectionName, fHeaderPadding->getSectionName()) == 0 )
			break;
		totalSizeOfTEXTLessHeaderAndLoadCommands += curSection->fSize;
	}
	uint64_t paddingSize = 0;
	if ( fOptions.outputKind() == Options::kDyld ) {
		// dyld itself has special padding requirements.  We want the beginning __text section to start at a stable address
		paddingSize = 4096 - (totalSizeOfTEXTLessHeaderAndLoadCommands % 4096);
	}
	else if ( fOptions.outputKind() == Options::kObjectFile ) {
		// mach-o .o files need no padding between load commands and first section
		// but leave enough room that the object file could be signed
		paddingSize = 32;
	}
	else if ( fOptions.outputKind() == Options::kPreload ) {
		// mach-o MH_PRELOAD files need no padding between load commands and first section
		paddingSize = 0;
	}
	else if ( fOptions.makeEncryptable() ) {
		// want load commands to end on a page boundary, so __text starts on page boundary
		paddingSize = 4096 - ((totalSizeOfTEXTLessHeaderAndLoadCommands+fOptions.minimumHeaderPad()) % 4096) + fOptions.minimumHeaderPad();
		fEncryptionLoadCommand->setStartEncryptionOffset(totalSizeOfTEXTLessHeaderAndLoadCommands+paddingSize);
	}
	else {
		// work backwards from end of segment and lay out sections so that extra room goes to padding atom
		uint64_t addr = 0;
		for(int j=sectionCount-1; j >=0; --j) {
			SectionInfo* curSection = sectionInfos[j];
			if ( strcmp(curSection->fSectionName, fHeaderPadding->getSectionName()) == 0 ) {
				addr -= (fLoadCommandsSection->fSize+fMachHeaderAtom->getSize());
				paddingSize = addr % fOptions.segmentAlignment();
				break;
			}
			addr -= curSection->fSize;
			addr = addr & (0 - (1 << curSection->fAlignment));
		}

		// if command line requires more padding than this
		uint32_t minPad = fOptions.minimumHeaderPad();
		if ( fOptions.maxMminimumHeaderPad() ) {
			// -headerpad_max_install_names means there should be room for every path load command to grow to 1204 bytes
			uint32_t altMin = fLibraryToOrdinal.size() * MAXPATHLEN;
			if ( fOptions.outputKind() ==  Options::kDynamicLibrary )
				altMin += MAXPATHLEN;
			if ( altMin > minPad )
				minPad = altMin;
		}
		if ( paddingSize < minPad ) {
			int extraPages = (minPad - paddingSize + fOptions.segmentAlignment() - 1)/fOptions.segmentAlignment();
			paddingSize += extraPages * fOptions.segmentAlignment();
		}
	}

	// adjust atom size and update section size
	fHeaderPadding->setSize(paddingSize);
	for(int j=0; j < sectionCount; ++j) {
		SectionInfo* curSection = sectionInfos[j];
		if ( strcmp(curSection->fSectionName, fHeaderPadding->getSectionName()) == 0 )
			curSection->fSize = paddingSize;
	}
}

static uint64_t segmentAlign(uint64_t addr, uint64_t alignment) 
{ 
	return ((addr+alignment-1) & (-alignment)); 
}

// assign file offsets and logical address to all segments
template <typename A>
void Writer<A>::assignFileOffsets()
{
	const bool virtualSectionOccupyAddressSpace = ((fOptions.outputKind() != Options::kObjectFile)
												&& (fOptions.outputKind() != Options::kPreload));
	bool haveFixedSegments = false;
	uint64_t fileOffset = 0;
	uint64_t nextContiguousAddress = fOptions.baseAddress();
	uint64_t nextReadOnlyAddress = fOptions.baseAddress();
	uint64_t nextWritableAddress = fOptions.baseWritableAddress();

	// process segments with fixed addresses (-segaddr)
	for (std::vector<Options::SegmentStart>::iterator it = fOptions.customSegmentAddresses().begin(); it != fOptions.customSegmentAddresses().end(); ++it) {
			for (std::vector<SegmentInfo*>::iterator segit = fSegmentInfos.begin(); segit != fSegmentInfos.end(); ++segit) {
			SegmentInfo* curSegment = *segit;
			if ( strcmp(curSegment->fName, it->name) == 0 ) {
				curSegment->fBaseAddress = it->address;
				curSegment->fFixedAddress = true;
				break;
			}
		}
	}

	// process segments with fixed addresses (-seg_page_size)
	for (std::vector<Options::SegmentSize>::iterator it = fOptions.customSegmentSizes().begin(); it != fOptions.customSegmentSizes().end(); ++it) {
			for (std::vector<SegmentInfo*>::iterator segit = fSegmentInfos.begin(); segit != fSegmentInfos.end(); ++segit) {
			SegmentInfo* curSegment = *segit;
			if ( strcmp(curSegment->fName, it->name) == 0 ) {
				curSegment->fPageSize = it->size;
				break;
			}
		}
	}

	// Run through the segments and each segment's sections to assign addresses
	for (std::vector<SegmentInfo*>::iterator segit = fSegmentInfos.begin(); segit != fSegmentInfos.end(); ++segit) {
		SegmentInfo* curSegment = *segit;
		
		if ( fOptions.splitSeg() ) {
			if ( curSegment->fInitProtection & VM_PROT_WRITE ) 
				nextContiguousAddress = nextWritableAddress;
			else
				nextContiguousAddress = nextReadOnlyAddress;
		}
		
		if ( fOptions.outputKind() == Options::kPreload ) {
			if ( strcmp(curSegment->fName, "__HEADER") == 0 ) 
				nextContiguousAddress = 0;
			else if ( strcmp(curSegment->fName, "__TEXT") == 0 ) 
				nextContiguousAddress = fOptions.baseAddress();
		}

		fileOffset = segmentAlign(fileOffset, curSegment->fPageSize);
		curSegment->fFileOffset = fileOffset;
		
		// Set the segment base address
		if ( curSegment->fFixedAddress )
			haveFixedSegments = true;
		else
			curSegment->fBaseAddress = segmentAlign(nextContiguousAddress, curSegment->fPageSize);

		// We've set the segment address, now run through each section.
		uint64_t address = curSegment->fBaseAddress;
		SectionInfo* firstZeroFillSection = NULL;
		SectionInfo* prevSection = NULL;
		
		std::vector<SectionInfo*>& sectionInfos = curSegment->fSections;
		
		for (std::vector<SectionInfo*>::iterator it = sectionInfos.begin(); it != sectionInfos.end(); ++it) {
			SectionInfo* curSection = *it;
		
			// adjust section address based on alignment
			uint64_t alignment = 1 << curSection->fAlignment;
			if ( curSection->fAtoms.size() == 1 ) {
				// if there is only one atom in section, use modulus for even better layout
				ObjectFile::Alignment atomAlign = curSection->fAtoms[0]->getAlignment();
				uint64_t atomAlignP2 = (1 << atomAlign.powerOf2);
				uint64_t currentModulus = (address % atomAlignP2);
				if ( currentModulus != atomAlign.modulus ) {
					if ( atomAlign.modulus > currentModulus )
						address += atomAlign.modulus-currentModulus;
					else
						address += atomAlign.modulus+atomAlignP2-currentModulus;		
				}
			}
			else {
				address = ( (address+alignment-1) & (-alignment) );
			}
			
			// adjust file offset to match address
			if ( prevSection != NULL ) {
				if ( virtualSectionOccupyAddressSpace || !prevSection->fVirtualSection )
					fileOffset = (address - prevSection->getBaseAddress()) + prevSection->fFileOffset;
				else
					fileOffset = ( (fileOffset+alignment-1) & (-alignment) );
			}
			
			// update section info
			curSection->fFileOffset = fileOffset;
			curSection->setBaseAddress(address);
			//fprintf(stderr, "%s %s addr=0x%llX, fileoffset=0x%llX, size=0x%llX\n", curSegment->fName, curSection->fSectionName, address, fileOffset, curSection->fSize);

			// keep track of trailing zero fill sections
			if ( curSection->fAllZeroFill && (firstZeroFillSection == NULL) )
				firstZeroFillSection = curSection;
			if ( !curSection->fAllZeroFill && (firstZeroFillSection != NULL) && (fOptions.outputKind() != Options::kObjectFile) ) 
				throwf("zero-fill section %s not at end of segment", curSection->fSectionName);
			
			// update running pointers
			if ( virtualSectionOccupyAddressSpace || !curSection->fVirtualSection )
				address += curSection->fSize;
			fileOffset += curSection->fSize;
			
			// sanity check size of 32-bit binaries
			if ( address > maxAddress() )
				throwf("section %s exceeds 4GB limit", curSection->fSectionName);
			
			// update segment info
			curSegment->fFileSize = fileOffset - curSegment->fFileOffset;
			curSegment->fSize = curSegment->fFileSize;
			prevSection = curSection;
		}
		
		if ( fOptions.outputKind() == Options::kObjectFile ) {
			// don't page align .o files
		}
		else {
			// optimize trailing zero-fill sections to not occupy disk space
			if ( firstZeroFillSection != NULL ) {
				curSegment->fFileSize = firstZeroFillSection->fFileOffset - curSegment->fFileOffset;
				fileOffset = firstZeroFillSection->fFileOffset;
			}
			// page align segment size
			curSegment->fFileSize = segmentAlign(curSegment->fFileSize, curSegment->fPageSize);
			curSegment->fSize	  = segmentAlign(curSegment->fSize, curSegment->fPageSize);
			if ( !curSegment->fIndependentAddress && (curSegment->fBaseAddress >= nextContiguousAddress) ) {
				nextContiguousAddress = segmentAlign(curSegment->fBaseAddress+curSegment->fSize, curSegment->fPageSize);
				fileOffset = segmentAlign(fileOffset, curSegment->fPageSize);
				if ( curSegment->fInitProtection & VM_PROT_WRITE )
					nextWritableAddress = nextContiguousAddress;
				else
					nextReadOnlyAddress = nextContiguousAddress;
			}
		}
		//fprintf(stderr, "end of seg %s, fileoffset=0x%llX, nextContiguousAddress=0x%llX\n", curSegment->fName, fileOffset, nextContiguousAddress);
	}
			
	// check for segment overlaps caused by user specified fixed segments (e.g. __PAGEZERO, __UNIXSTACK)
	if ( haveFixedSegments ) {
		int segCount = fSegmentInfos.size();
		for(int i=0; i < segCount; ++i) {
			SegmentInfo* segment1 = fSegmentInfos[i];
			
			for(int j=0; j < segCount; ++j) {
				if ( i != j ) {
					SegmentInfo* segment2 = fSegmentInfos[j];
					
					if ( segment1->fBaseAddress < segment2->fBaseAddress ) {
						if ( (segment1->fBaseAddress+segment1->fSize) > segment2->fBaseAddress )
							throwf("segments overlap: %s (0x%08llX + 0x%08llX) and %s (0x%08llX + 0x%08llX)",
								segment1->fName, segment1->fBaseAddress, segment1->fSize, segment2->fName, segment2->fBaseAddress, segment2->fSize);
					}
					else if ( segment1->fBaseAddress > segment2->fBaseAddress ) {
						if ( (segment2->fBaseAddress+segment2->fSize) > segment1->fBaseAddress )
							throwf("segments overlap: %s (0x%08llX + 0x%08llX) and %s (0x%08llX + 0x%08llX)",
								segment1->fName, segment1->fBaseAddress, segment1->fSize, segment2->fName, segment2->fBaseAddress, segment2->fSize);
					}
					else if ( (segment1->fSize != 0) && (segment2->fSize != 0) ) {
							throwf("segments overlap: %s (0x%08llX + 0x%08llX) and %s (0x%08llX + 0x%08llX)",
								segment1->fName, segment1->fBaseAddress, segment1->fSize, segment2->fName, segment2->fBaseAddress, segment2->fSize);
					}
				}
			}
		}
	}

	// set up fFirstWritableSegment and fWritableSegmentPastFirst4GB
	for (std::vector<SegmentInfo*>::iterator segit = fSegmentInfos.begin(); segit != fSegmentInfos.end(); ++segit) {
		SegmentInfo* curSegment = *segit;
		if ( (curSegment->fInitProtection & VM_PROT_WRITE) != 0 ) {
			if ( fFirstWritableSegment == NULL )
				fFirstWritableSegment = curSegment;
			if ( (curSegment->fBaseAddress + curSegment->fSize - fOptions.baseAddress()) >= 0x100000000LL )
				fWritableSegmentPastFirst4GB = true;
		}
	}
	
	// record size of encrypted part of __TEXT segment
	if ( fOptions.makeEncryptable() ) {
		for (std::vector<SegmentInfo*>::iterator segit = fSegmentInfos.begin(); segit != fSegmentInfos.end(); ++segit) {
			SegmentInfo* curSegment = *segit;
			if ( strcmp(curSegment->fName, "__TEXT") == 0 ) {
				fEncryptionLoadCommand->setEndEncryptionOffset(curSegment->fFileSize);
				break;
			}
		}
	}

}

template <typename A>
void Writer<A>::adjustLinkEditSections()
{
	// link edit content is always in last segment
	SegmentInfo* lastSeg = fSegmentInfos[fSegmentInfos.size()-1];
	unsigned int firstLinkEditSectionIndex = 0;
	while ( strcmp(lastSeg->fSections[firstLinkEditSectionIndex]->fSegmentName, "__LINKEDIT") != 0 )
		++firstLinkEditSectionIndex;

	const unsigned int linkEditSectionCount = lastSeg->fSections.size();
	uint64_t fileOffset = lastSeg->fSections[firstLinkEditSectionIndex]->fFileOffset;
	uint64_t address = lastSeg->fSections[firstLinkEditSectionIndex]->getBaseAddress();
	if ( fPadSegmentInfo != NULL ) {
		// insert __4GBFILL segment into segments vector before LINKEDIT
		for(std::vector<SegmentInfo*>::iterator it = fSegmentInfos.begin(); it != fSegmentInfos.end(); ++it) {
			if ( *it == lastSeg ) {
				fSegmentInfos.insert(it, fPadSegmentInfo);
				break;
			}
		}
		// adjust  __4GBFILL segment to span from end of last segment to zeroPageSize
		fPadSegmentInfo->fSize = fOptions.zeroPageSize() - address;
		fPadSegmentInfo->fBaseAddress = address;
		// adjust LINKEDIT to start at zeroPageSize
		address = fOptions.zeroPageSize();
		lastSeg->fBaseAddress = fOptions.zeroPageSize();
	}
	for (unsigned int i=firstLinkEditSectionIndex; i < linkEditSectionCount; ++i) {
		std::vector<class ObjectFile::Atom*>& atoms = lastSeg->fSections[i]->fAtoms;
		// adjust section address based on alignment
		uint64_t sectionAlignment = 1 << lastSeg->fSections[i]->fAlignment;
		uint64_t pad = ((address+sectionAlignment-1) & (-sectionAlignment)) - address;
		address += pad;
		fileOffset += pad;	// adjust file offset to match address
		lastSeg->fSections[i]->setBaseAddress(address);
		if ( strcmp(lastSeg->fSections[i]->fSectionName, "._absolute") == 0 )
			lastSeg->fSections[i]->setBaseAddress(0);
		lastSeg->fSections[i]->fFileOffset = fileOffset;
		uint64_t sectionOffset = 0;
		for (unsigned int j=0; j < atoms.size(); ++j) {
			ObjectFile::Atom* atom = atoms[j];
			uint64_t alignment = 1 << atom->getAlignment().powerOf2;
			sectionOffset = ( (sectionOffset+alignment-1) & (-alignment) );
			atom->setSectionOffset(sectionOffset);
			uint64_t size = atom->getSize();
			sectionOffset += size;
			if ( size > fLargestAtomSize )
				fLargestAtomSize = size;
		}
		//fprintf(stderr, "setting: lastSeg->fSections[%d]->fSize = 0x%08llX\n", i, sectionOffset);
		lastSeg->fSections[i]->fSize = sectionOffset;
		fileOffset += sectionOffset;
		address += sectionOffset;
	}
	if ( fOptions.outputKind() == Options::kObjectFile ) {
		//lastSeg->fBaseAddress = 0;
		//lastSeg->fSize = lastSeg->fSections[firstLinkEditSectionIndex]->
		//lastSeg->fFileOffset = 0;
		//lastSeg->fFileSize =
	}
	else {
		lastSeg->fFileSize = fileOffset - lastSeg->fFileOffset;
		lastSeg->fSize     = (address - lastSeg->fBaseAddress+4095) & (-4096);
	}
}


template <typename A>
ObjectFile::Atom::Scope MachHeaderAtom<A>::getScope() const
{
	switch ( fWriter.fOptions.outputKind() ) {
		case Options::kDynamicExecutable:
		case Options::kStaticExecutable:
			return ObjectFile::Atom::scopeGlobal;
		case Options::kDynamicLibrary:
		case Options::kDynamicBundle:
		case Options::kDyld:
		case Options::kObjectFile:
		case Options::kPreload:
		case Options::kKextBundle:
			return ObjectFile::Atom::scopeLinkageUnit;
	}
	throw "unknown header type";
}

template <typename A>
ObjectFile::Atom::SymbolTableInclusion MachHeaderAtom<A>::getSymbolTableInclusion() const
{
	switch ( fWriter.fOptions.outputKind() ) {
		case Options::kDynamicExecutable:
			return ObjectFile::Atom::kSymbolTableInAndNeverStrip;
		case Options::kStaticExecutable:
			return ObjectFile::Atom::kSymbolTableInAsAbsolute;
		case Options::kDynamicLibrary:
		case Options::kDynamicBundle:
		case Options::kDyld:
			return ObjectFile::Atom::kSymbolTableIn;
		case Options::kObjectFile:
		case Options::kPreload:
		case Options::kKextBundle:
			return ObjectFile::Atom::kSymbolTableNotIn;
	}
	throw "unknown header type";
}

template <typename A>
const char* MachHeaderAtom<A>::getName() const
{
	switch ( fWriter.fOptions.outputKind() ) {
		case Options::kDynamicExecutable:
		case Options::kStaticExecutable:
			return "__mh_execute_header";
		case Options::kDynamicLibrary:
			return "__mh_dylib_header";
		case Options::kDynamicBundle:
			return "__mh_bundle_header";
		case Options::kObjectFile:
		case Options::kPreload:
		case Options::kKextBundle:
			return NULL;
		case Options::kDyld:
			return "__mh_dylinker_header";
	}
	throw "unknown header type";
}

template <typename A>
const char* MachHeaderAtom<A>::getDisplayName() const
{
	switch ( fWriter.fOptions.outputKind() ) {
		case Options::kDynamicExecutable:
		case Options::kStaticExecutable:
		case Options::kDynamicLibrary:
		case Options::kDynamicBundle:
		case Options::kDyld:
			return this->getName();
		case Options::kObjectFile:
		case Options::kPreload:
		case Options::kKextBundle:
			return "mach header";
	}
	throw "unknown header type";
}

template <typename A>
void MachHeaderAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	// get file type
	uint32_t fileType = 0;
	switch ( fWriter.fOptions.outputKind() ) {
		case Options::kDynamicExecutable:
		case Options::kStaticExecutable:
			fileType = MH_EXECUTE;
			break;
		case Options::kDynamicLibrary:
			fileType = MH_DYLIB;
			break;
		case Options::kDynamicBundle:
			fileType = MH_BUNDLE;
			break;
		case Options::kObjectFile:
			fileType = MH_OBJECT;
			break;
		case Options::kDyld:
			fileType = MH_DYLINKER;
			break;
		case Options::kPreload:
			fileType = MH_PRELOAD;
			break;
		case Options::kKextBundle:
			fileType = MH_KEXT_BUNDLE;
			break;
	}

	// get flags
	uint32_t flags = 0;
	if ( fWriter.fOptions.outputKind() == Options::kObjectFile ) {
		if ( fWriter.fCanScatter )
			flags = MH_SUBSECTIONS_VIA_SYMBOLS;
	}
	else {
		if ( fWriter.fOptions.outputKind() == Options::kStaticExecutable ) {
			flags |= MH_NOUNDEFS;
		}
		else if ( fWriter.fOptions.outputKind() == Options::kPreload ) {
			flags |= MH_NOUNDEFS;
			if ( fWriter.fOptions.positionIndependentExecutable() ) 
				flags |= MH_PIE;
		}
		else {
			flags = MH_DYLDLINK;
			if ( fWriter.fOptions.bindAtLoad() )
				flags |= MH_BINDATLOAD;
			switch ( fWriter.fOptions.nameSpace() ) {
				case Options::kTwoLevelNameSpace:
					flags |= MH_TWOLEVEL | MH_NOUNDEFS;
					break;
				case Options::kFlatNameSpace:
					break;
				case Options::kForceFlatNameSpace:
					flags |= MH_FORCE_FLAT;
					break;
			}
			bool hasWeakDefines = fWriter.fHasWeakExports;
			if ( fWriter.fRegularDefAtomsThatOverrideADylibsWeakDef->size() != 0 ) {
				for(std::set<const ObjectFile::Atom*>::iterator it = fWriter.fRegularDefAtomsThatOverrideADylibsWeakDef->begin();
													it != fWriter.fRegularDefAtomsThatOverrideADylibsWeakDef->end(); ++it) {
					if ( fWriter.shouldExport(**it) ) {
						hasWeakDefines = true;
						break;
					}
				}
			}
			if ( hasWeakDefines )
				flags |= MH_WEAK_DEFINES;
			if ( fWriter.fReferencesWeakImports || fWriter.fHasWeakExports )
				flags |= MH_BINDS_TO_WEAK;
			if ( fWriter.fOptions.prebind() )
				flags |= MH_PREBOUND;
			if ( fWriter.fOptions.splitSeg() )
				flags |= MH_SPLIT_SEGS;
			if ( (fWriter.fOptions.outputKind() == Options::kDynamicLibrary) && fWriter.fNoReExportedDylibs ) 
				flags |= MH_NO_REEXPORTED_DYLIBS;
			if ( fWriter.fOptions.positionIndependentExecutable() ) 
				flags |= MH_PIE;
			if ( fWriter.fOptions.markAutoDeadStripDylib() ) 
				flags |= MH_DEAD_STRIPPABLE_DYLIB;
		}
		if ( fWriter.fOptions.hasExecutableStack() )
			flags |= MH_ALLOW_STACK_EXECUTION;
		if ( fWriter.fOptions.readerOptions().fRootSafe )
			flags |= MH_ROOT_SAFE;
		if ( fWriter.fOptions.readerOptions().fSetuidSafe )
			flags |= MH_SETUID_SAFE;
	}

	// get commands info
	uint32_t commandsSize = 0;
	uint32_t commandsCount = 0;

	std::vector<class ObjectFile::Atom*>& loadCommandAtoms = fWriter.fLoadCommandsSection->fAtoms;
	for (std::vector<ObjectFile::Atom*>::iterator it=loadCommandAtoms.begin(); it != loadCommandAtoms.end(); it++) {
		ObjectFile::Atom* atom = *it;
		commandsSize += atom->getSize();
		// segment and symbol table atoms can contain more than one load command
		if ( atom == fWriter.fSegmentCommands )
			commandsCount += fWriter.fSegmentCommands->commandCount();
		else if ( atom == fWriter.fSymbolTableCommands )
			commandsCount += fWriter.fSymbolTableCommands->commandCount();
		else if ( atom->getSize() != 0 )
			++commandsCount;
	}

	// fill out mach_header
	macho_header<typename A::P>* mh = (macho_header<typename A::P>*)buffer;
	setHeaderInfo(*mh);
	mh->set_filetype(fileType);
	mh->set_ncmds(commandsCount);
	mh->set_sizeofcmds(commandsSize);
	mh->set_flags(flags);
}

template <>
void MachHeaderAtom<ppc>::setHeaderInfo(macho_header<ppc::P>& header) const
{
	header.set_magic(MH_MAGIC);
	header.set_cputype(CPU_TYPE_POWERPC);
    header.set_cpusubtype(fWriter.fCpuConstraint);
}

template <>
void MachHeaderAtom<ppc64>::setHeaderInfo(macho_header<ppc64::P>& header) const
{
	header.set_magic(MH_MAGIC_64);
	header.set_cputype(CPU_TYPE_POWERPC64);
	if ( (fWriter.fOptions.outputKind() == Options::kDynamicExecutable) && (fWriter.fOptions.macosxVersionMin() >= ObjectFile::ReaderOptions::k10_5) )
		header.set_cpusubtype(CPU_SUBTYPE_POWERPC_ALL | 0x80000000);
	else
		header.set_cpusubtype(CPU_SUBTYPE_POWERPC_ALL);
	header.set_reserved(0);
}

template <>
void MachHeaderAtom<x86>::setHeaderInfo(macho_header<x86::P>& header) const
{
	header.set_magic(MH_MAGIC);
	header.set_cputype(CPU_TYPE_I386);
	header.set_cpusubtype(CPU_SUBTYPE_I386_ALL);
}

template <>
void MachHeaderAtom<x86_64>::setHeaderInfo(macho_header<x86_64::P>& header) const
{
	header.set_magic(MH_MAGIC_64);
	header.set_cputype(CPU_TYPE_X86_64);
	if ( (fWriter.fOptions.outputKind() == Options::kDynamicExecutable) && (fWriter.fOptions.macosxVersionMin() >= ObjectFile::ReaderOptions::k10_5) )
		header.set_cpusubtype(CPU_SUBTYPE_X86_64_ALL | 0x80000000);
	else
		header.set_cpusubtype(CPU_SUBTYPE_X86_64_ALL);
	header.set_reserved(0);
}

template <>
void MachHeaderAtom<arm>::setHeaderInfo(macho_header<arm::P>& header) const
{
	header.set_magic(MH_MAGIC);
	header.set_cputype(CPU_TYPE_ARM);
	header.set_cpusubtype(fWriter.fCpuConstraint);
}

template <typename A>
CustomStackAtom<A>::CustomStackAtom(Writer<A>& writer)
 : WriterAtom<A>(writer, Segment::fgStackSegment)
{
	if ( stackGrowsDown() )
		Segment::fgStackSegment.setBaseAddress(writer.fOptions.customStackAddr() - writer.fOptions.customStackSize());
	else
		Segment::fgStackSegment.setBaseAddress(writer.fOptions.customStackAddr());
}


template <> bool CustomStackAtom<ppc>::stackGrowsDown()    { return true; }
template <> bool CustomStackAtom<ppc64>::stackGrowsDown()  { return true; }
template <> bool CustomStackAtom<x86>::stackGrowsDown()    { return true; }
template <> bool CustomStackAtom<x86_64>::stackGrowsDown() { return true; }
template <> bool CustomStackAtom<arm>::stackGrowsDown()    { return true; }

template <typename A>
void SegmentLoadCommandsAtom<A>::computeSize()
{
	uint64_t size = 0;
	std::vector<SegmentInfo*>& segmentInfos = fWriter.fSegmentInfos;
	int segCount = 0;
	for(std::vector<SegmentInfo*>::iterator it = segmentInfos.begin(); it != segmentInfos.end(); ++it) {
		SegmentInfo* seg = *it;
		if ( seg->fHasLoadCommand ) {
			++segCount;
			size += sizeof(macho_segment_command<P>);
			std::vector<SectionInfo*>& sectionInfos = seg->fSections;
			const int sectionCount = sectionInfos.size();
			for(int j=0; j < sectionCount; ++j) {
				if ( fWriter.fEmitVirtualSections || ! sectionInfos[j]->fVirtualSection )
					size += sizeof(macho_section<P>);
			}
		}
	}
	fSize = size;
	fCommandCount = segCount;
	if ( fWriter.fPadSegmentInfo != NULL ) {
		++fCommandCount;
		fSize += sizeof(macho_segment_command<P>);
	}
}

template <>
uint64_t LoadCommandAtom<ppc>::alignedSize(uint64_t size)
{
	return ((size+3) & (-4));	// 4-byte align all load commands for 32-bit mach-o
}

template <>
uint64_t LoadCommandAtom<ppc64>::alignedSize(uint64_t size)
{
	return ((size+7) & (-8));	// 8-byte align all load commands for 64-bit mach-o
}

template <>
uint64_t LoadCommandAtom<x86>::alignedSize(uint64_t size)
{
	return ((size+3) & (-4));	// 4-byte align all load commands for 32-bit mach-o
}

template <>
uint64_t LoadCommandAtom<x86_64>::alignedSize(uint64_t size)
{
	return ((size+7) & (-8));	// 8-byte align all load commands for 64-bit mach-o
}

template <>
uint64_t LoadCommandAtom<arm>::alignedSize(uint64_t size)
{
	return ((size+3) & (-4));	// 4-byte align all load commands for 32-bit mach-o
}

template <typename A>
void SegmentLoadCommandsAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t size = this->getSize();
	const bool oneSegment =( fWriter.fOptions.outputKind() == Options::kObjectFile );
	bzero(buffer, size);
	uint8_t* p = buffer;
	typename std::vector<SegmentInfo*>& segmentInfos = fWriter.fSegmentInfos;
	for(std::vector<SegmentInfo*>::iterator it = segmentInfos.begin(); it != segmentInfos.end(); ++it) {
		SegmentInfo* segInfo = *it;
		if ( ! segInfo->fHasLoadCommand )
			continue;
		const int sectionCount = segInfo->fSections.size();
		macho_segment_command<P>* cmd = (macho_segment_command<P>*)p;
		cmd->set_cmd(macho_segment_command<P>::CMD);
		cmd->set_segname(segInfo->fName);
		cmd->set_vmaddr(segInfo->fBaseAddress);
		cmd->set_vmsize(oneSegment ? 0 : segInfo->fSize);
		cmd->set_fileoff(segInfo->fFileOffset);
		cmd->set_filesize(oneSegment ? 0 : segInfo->fFileSize);
		cmd->set_maxprot(segInfo->fMaxProtection);
		cmd->set_initprot(segInfo->fInitProtection);
		// add sections array
		macho_section<P>* const sections = (macho_section<P>*)&p[sizeof(macho_segment_command<P>)];
		unsigned int sectionsEmitted = 0;
		for (int j=0; j < sectionCount; ++j) {
			SectionInfo* sectInfo = segInfo->fSections[j];
			if ( fWriter.fEmitVirtualSections || !sectInfo->fVirtualSection ) {
				macho_section<P>* sect = &sections[sectionsEmitted++];
				if ( oneSegment ) {
					// .o file segment does not cover load commands, so recalc at first real section
					if ( sectionsEmitted == 1 ) {
						cmd->set_vmaddr(sectInfo->getBaseAddress());
						cmd->set_fileoff(sectInfo->fFileOffset);
					}
					cmd->set_filesize((sectInfo->fFileOffset+sectInfo->fSize)-cmd->fileoff());
					cmd->set_vmsize(sectInfo->getBaseAddress() + sectInfo->fSize);
				}
				sect->set_sectname(sectInfo->fSectionName);
				sect->set_segname(sectInfo->fSegmentName);
				sect->set_addr(sectInfo->getBaseAddress());
				sect->set_size(sectInfo->fSize);
				sect->set_offset(sectInfo->fFileOffset);
				sect->set_align(sectInfo->fAlignment);
				if ( sectInfo->fRelocCount != 0 ) {
					sect->set_reloff(sectInfo->fRelocOffset * sizeof(macho_relocation_info<P>) + fWriter.fSectionRelocationsAtom->getFileOffset());
					sect->set_nreloc(sectInfo->fRelocCount);
				}
				if ( sectInfo->fAllZeroFill ) {
					sect->set_flags(S_ZEROFILL);
					sect->set_offset(0);
				}
				else if ( sectInfo->fAllLazyPointers ) {
					sect->set_flags(S_LAZY_SYMBOL_POINTERS);
					sect->set_reserved1(sectInfo->fIndirectSymbolOffset);
				}
				else if ( sectInfo->fAllLazyDylibPointers ) {
					sect->set_flags(S_LAZY_DYLIB_SYMBOL_POINTERS);
					sect->set_reserved1(sectInfo->fIndirectSymbolOffset);
				}
				else if ( sectInfo->fAllNonLazyPointers ) {
					sect->set_flags(S_NON_LAZY_SYMBOL_POINTERS);
					sect->set_reserved1(sectInfo->fIndirectSymbolOffset);
				}
				else if ( sectInfo->fAllStubs ) {
					sect->set_flags(S_SYMBOL_STUBS | S_ATTR_SOME_INSTRUCTIONS | S_ATTR_PURE_INSTRUCTIONS);
					sect->set_reserved1(sectInfo->fIndirectSymbolOffset);
					sect->set_reserved2(sectInfo->fSize / sectInfo->fAtoms.size());
					if ( sectInfo->fHasTextLocalRelocs )
						sect->set_flags(sect->flags() | S_ATTR_LOC_RELOC);
				}
				else if ( sectInfo->fAllSelfModifyingStubs ) {
					sect->set_flags(S_SYMBOL_STUBS | S_ATTR_SELF_MODIFYING_CODE);
					sect->set_reserved1(sectInfo->fIndirectSymbolOffset);
					sect->set_reserved2(sectInfo->fSize / sectInfo->fAtoms.size());
				}
				else if ( sectInfo->fAllStubHelpers ) {
					sect->set_flags(S_ATTR_SOME_INSTRUCTIONS | S_ATTR_PURE_INSTRUCTIONS);
					if ( sectInfo->fHasTextLocalRelocs )
						sect->set_flags(sect->flags() | S_ATTR_LOC_RELOC);
				}
				else if ( sectInfo->fAtoms.at(0)->getContentType() == ObjectFile::Atom::kCStringType ) {
					sect->set_flags(S_CSTRING_LITERALS);
				}
				else if ( sectInfo->fAtoms.at(0)->getContentType() == ObjectFile::Atom::kCFIType ) {
					sect->set_flags(S_COALESCED | S_ATTR_NO_TOC | S_ATTR_STRIP_STATIC_SYMS);
				}
				else if ( (strcmp(sectInfo->fSectionName, "__mod_init_func") == 0) && (strcmp(sectInfo->fSegmentName, "__DATA") == 0) ) {
					sect->set_flags(S_MOD_INIT_FUNC_POINTERS);
				}
				else if ( (strcmp(sectInfo->fSectionName, "__mod_term_func") == 0) && (strcmp(sectInfo->fSegmentName, "__DATA") == 0) ) {
					sect->set_flags(S_MOD_TERM_FUNC_POINTERS);
				}
				else if ( (strcmp(sectInfo->fSectionName, "__textcoal_nt") == 0) && (strcmp(sectInfo->fSegmentName, "__TEXT") == 0) ) {
					sect->set_flags(S_COALESCED);
				}
				else if ( (strcmp(sectInfo->fSectionName, "__const_coal") == 0) && (strcmp(sectInfo->fSegmentName, "__DATA") == 0) ) {
					sect->set_flags(S_COALESCED);
				}
				else if ( (strcmp(sectInfo->fSectionName, "__interpose") == 0) && (strcmp(sectInfo->fSegmentName, "__DATA") == 0) ) {
					sect->set_flags(S_INTERPOSING);
				}
				else if ( (strcmp(sectInfo->fSectionName, "__literal4") == 0) && (strcmp(sectInfo->fSegmentName, "__TEXT") == 0) ) {
					sect->set_flags(S_4BYTE_LITERALS);
				}
				else if ( (strcmp(sectInfo->fSectionName, "__literal8") == 0) && (strcmp(sectInfo->fSegmentName, "__TEXT") == 0) ) {
					sect->set_flags(S_8BYTE_LITERALS);
				}
				else if ( (strcmp(sectInfo->fSectionName, "__literal16") == 0) && (strcmp(sectInfo->fSegmentName, "__TEXT") == 0) ) {
					sect->set_flags(S_16BYTE_LITERALS);
				}
				else if ( (strcmp(sectInfo->fSectionName, "__message_refs") == 0) && (strcmp(sectInfo->fSegmentName, "__OBJC") == 0) ) {
					sect->set_flags(S_LITERAL_POINTERS);
				}
				else if ( (strcmp(sectInfo->fSectionName, "__objc_selrefs") == 0) && (strcmp(sectInfo->fSegmentName, "__DATA") == 0) ) {
					sect->set_flags(S_LITERAL_POINTERS);
				}
				else if ( (strcmp(sectInfo->fSectionName, "__cls_refs") == 0) && (strcmp(sectInfo->fSegmentName, "__OBJC") == 0) ) {
					sect->set_flags(S_LITERAL_POINTERS);
				}
				else if ( (strncmp(sectInfo->fSectionName, "__dof_", 6) == 0) && (strcmp(sectInfo->fSegmentName, "__TEXT") == 0) ) {
					sect->set_flags(S_DTRACE_DOF);
				}
				else if ( (strncmp(sectInfo->fSectionName, "__dof_", 6) == 0) && (strcmp(sectInfo->fSegmentName, "__DATA") == 0) ) {
					sect->set_flags(S_DTRACE_DOF);
				}
				else if ( (strncmp(sectInfo->fSectionName, "__text", 6) == 0) && (strcmp(sectInfo->fSegmentName, "__TEXT") == 0) ) {
					sect->set_flags(S_REGULAR | S_ATTR_SOME_INSTRUCTIONS | S_ATTR_PURE_INSTRUCTIONS);
					if ( sectInfo->fHasTextLocalRelocs )
						sect->set_flags(sect->flags() | S_ATTR_LOC_RELOC);
					if ( sectInfo->fHasTextExternalRelocs )
						sect->set_flags(sect->flags() | S_ATTR_EXT_RELOC);
				}
				//fprintf(stderr, "section %s flags=0x%08X\n", sectInfo->fSectionName, sect->flags());
			}
		}
		p = &p[sizeof(macho_segment_command<P>) + sectionsEmitted*sizeof(macho_section<P>)];
		cmd->set_cmdsize(sizeof(macho_segment_command<P>) + sectionsEmitted*sizeof(macho_section<P>));
		cmd->set_nsects(sectionsEmitted);
	}
}


template <typename A>
SymbolTableLoadCommandsAtom<A>::SymbolTableLoadCommandsAtom(Writer<A>& writer)
 : LoadCommandAtom<A>(writer), fNeedsDynamicSymbolTable(false)
{
	bzero(&fSymbolTable, sizeof(macho_symtab_command<P>));
	bzero(&fDynamicSymbolTable, sizeof(macho_dysymtab_command<P>));
	switch ( fWriter.fOptions.outputKind() ) {
		case Options::kDynamicExecutable:
		case Options::kDynamicLibrary:
		case Options::kDynamicBundle:
		case Options::kDyld:
		case Options::kKextBundle:
			fNeedsDynamicSymbolTable = true;
			break;
		case Options::kObjectFile:
		case Options::kStaticExecutable:
			fNeedsDynamicSymbolTable = false;
		case Options::kPreload:
			fNeedsDynamicSymbolTable = fWriter.fOptions.positionIndependentExecutable();
			break;
	}
	writer.fSymbolTableCommands = this;
}



template <typename A>
void SymbolTableLoadCommandsAtom<A>::needDynamicTable() 
{
	fNeedsDynamicSymbolTable = true;
}
	

template <typename A>
uint64_t SymbolTableLoadCommandsAtom<A>::getSize() const
{
	if ( fNeedsDynamicSymbolTable )
		return this->alignedSize(sizeof(macho_symtab_command<P>) + sizeof(macho_dysymtab_command<P>));
	else
		return this->alignedSize(sizeof(macho_symtab_command<P>));
}

template <typename A>
void SymbolTableLoadCommandsAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	// build LC_SYMTAB command
	macho_symtab_command<P>*   symbolTableCmd = (macho_symtab_command<P>*)buffer;
	bzero(symbolTableCmd, sizeof(macho_symtab_command<P>));
	symbolTableCmd->set_cmd(LC_SYMTAB);
	symbolTableCmd->set_cmdsize(sizeof(macho_symtab_command<P>));
	symbolTableCmd->set_nsyms(fWriter.fSymbolTableCount);
	symbolTableCmd->set_symoff(fWriter.fSymbolTableCount == 0 ? 0 : fWriter.fSymbolTableAtom->getFileOffset());
	symbolTableCmd->set_stroff(fWriter.fStringsAtom->getSize() == 0 ? 0 : fWriter.fStringsAtom->getFileOffset());
	symbolTableCmd->set_strsize(fWriter.fStringsAtom->getSize());

	// build LC_DYSYMTAB command
	if ( fNeedsDynamicSymbolTable ) {
		macho_dysymtab_command<P>* dynamicSymbolTableCmd = (macho_dysymtab_command<P>*)&buffer[sizeof(macho_symtab_command<P>)];
		bzero(dynamicSymbolTableCmd, sizeof(macho_dysymtab_command<P>));
		dynamicSymbolTableCmd->set_cmd(LC_DYSYMTAB);
		dynamicSymbolTableCmd->set_cmdsize(sizeof(macho_dysymtab_command<P>));
		dynamicSymbolTableCmd->set_ilocalsym(fWriter.fSymbolTableStabsStartIndex);
		dynamicSymbolTableCmd->set_nlocalsym(fWriter.fSymbolTableStabsCount + fWriter.fSymbolTableLocalCount);
		dynamicSymbolTableCmd->set_iextdefsym(fWriter.fSymbolTableExportStartIndex);
		dynamicSymbolTableCmd->set_nextdefsym(fWriter.fSymbolTableExportCount);
		dynamicSymbolTableCmd->set_iundefsym(fWriter.fSymbolTableImportStartIndex);
		dynamicSymbolTableCmd->set_nundefsym(fWriter.fSymbolTableImportCount);
		if ( fWriter.fModuleInfoAtom != NULL ) {
			dynamicSymbolTableCmd->set_tocoff(fWriter.fModuleInfoAtom->getTableOfContentsFileOffset());
			dynamicSymbolTableCmd->set_ntoc(fWriter.fSymbolTableExportCount);
			dynamicSymbolTableCmd->set_modtaboff(fWriter.fModuleInfoAtom->getModuleTableFileOffset());
			dynamicSymbolTableCmd->set_nmodtab(1);
			dynamicSymbolTableCmd->set_extrefsymoff(fWriter.fModuleInfoAtom->getReferencesFileOffset());
			dynamicSymbolTableCmd->set_nextrefsyms(fWriter.fModuleInfoAtom->getReferencesCount());
		}
		dynamicSymbolTableCmd->set_indirectsymoff((fWriter.fIndirectTableAtom == NULL) ? 0 : fWriter.fIndirectTableAtom->getFileOffset());
		dynamicSymbolTableCmd->set_nindirectsyms((fWriter.fIndirectTableAtom == NULL) ? 0 : fWriter.fIndirectTableAtom->fTable.size());
		if ( fWriter.fOptions.outputKind() != Options::kObjectFile ) {
			if ( fWriter.fExternalRelocationsAtom != 0 ) {
				dynamicSymbolTableCmd->set_extreloff((fWriter.fExternalRelocs.size()==0) ? 0 : fWriter.fExternalRelocationsAtom->getFileOffset());
				dynamicSymbolTableCmd->set_nextrel(fWriter.fExternalRelocs.size());
			}
			if ( fWriter.fLocalRelocationsAtom != 0 ) {
				dynamicSymbolTableCmd->set_locreloff((fWriter.fInternalRelocs.size()==0) ? 0 : fWriter.fLocalRelocationsAtom->getFileOffset());
				dynamicSymbolTableCmd->set_nlocrel(fWriter.fInternalRelocs.size());
			}
		}
	}
}


template <typename A>
unsigned int SymbolTableLoadCommandsAtom<A>::commandCount()
{
	return fNeedsDynamicSymbolTable ? 2 : 1;
}

template <typename A>
uint64_t DyldLoadCommandsAtom<A>::getSize() const
{
	return this->alignedSize(sizeof(macho_dylinker_command<P>) + strlen("/usr/lib/dyld") + 1);
}

template <typename A>
void DyldLoadCommandsAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t size = this->getSize();
	bzero(buffer, size);
	macho_dylinker_command<P>* cmd = (macho_dylinker_command<P>*)buffer;
	if ( fWriter.fOptions.outputKind() == Options::kDyld )
		cmd->set_cmd(LC_ID_DYLINKER);
	else
		cmd->set_cmd(LC_LOAD_DYLINKER);
	cmd->set_cmdsize(this->getSize());
	cmd->set_name_offset();
	strcpy((char*)&buffer[sizeof(macho_dylinker_command<P>)], "/usr/lib/dyld");
}

template <typename A>
uint64_t AllowableClientLoadCommandsAtom<A>::getSize() const
{
	return this->alignedSize(sizeof(macho_sub_client_command<P>) + strlen(this->clientString) + 1);
}

template <typename A>
void AllowableClientLoadCommandsAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t size = this->getSize();

	bzero(buffer, size);
	macho_sub_client_command<P>* cmd = (macho_sub_client_command<P>*)buffer;
	cmd->set_cmd(LC_SUB_CLIENT);
	cmd->set_cmdsize(size);
	cmd->set_client_offset();
	strcpy((char*)&buffer[sizeof(macho_sub_client_command<P>)], this->clientString);

}

template <typename A>
uint64_t DylibLoadCommandsAtom<A>::getSize() const
{
	if ( fOptimizedAway ) {
		return 0;
	}
	else {
		const char* path = fInfo.reader->getInstallPath();
		return this->alignedSize(sizeof(macho_dylib_command<P>) + strlen(path) + 1);
	}
}

template <typename A>
void DylibLoadCommandsAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	if ( fOptimizedAway ) 
		return;
	uint64_t size = this->getSize();
	bzero(buffer, size);
	const char* path = fInfo.reader->getInstallPath();
	macho_dylib_command<P>* cmd = (macho_dylib_command<P>*)buffer;
	// <rdar://problem/5529626> If only weak_import symbols are used, linker should use LD_LOAD_WEAK_DYLIB
	bool autoWeakLoadDylib = ( (fWriter.fDylibReadersWithWeakImports.count(fInfo.reader) > 0) 
							&& (fWriter.fDylibReadersWithNonWeakImports.count(fInfo.reader) == 0) );
	if ( fInfo.options.fLazyLoad )
		cmd->set_cmd(LC_LAZY_LOAD_DYLIB);
	else if ( fInfo.options.fWeakImport || autoWeakLoadDylib )
		cmd->set_cmd(LC_LOAD_WEAK_DYLIB);
	else if ( fInfo.options.fReExport && (fWriter.fOptions.macosxVersionMin() >= ObjectFile::ReaderOptions::k10_5) )
		cmd->set_cmd(LC_REEXPORT_DYLIB);
	else
		cmd->set_cmd(LC_LOAD_DYLIB);
	cmd->set_cmdsize(this->getSize());
	cmd->set_timestamp(2);	// needs to be some constant value that is different than DylibIDLoadCommandsAtom uses
	cmd->set_current_version(fInfo.reader->getCurrentVersion());
	cmd->set_compatibility_version(fInfo.reader->getCompatibilityVersion());
	cmd->set_name_offset();
	strcpy((char*)&buffer[sizeof(macho_dylib_command<P>)], path);
}



template <typename A>
uint64_t DylibIDLoadCommandsAtom<A>::getSize() const
{
	return this->alignedSize(sizeof(macho_dylib_command<P>) + strlen(fWriter.fOptions.installPath()) + 1);
}

template <typename A>
void DylibIDLoadCommandsAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t size = this->getSize();
	bzero(buffer, size);
	macho_dylib_command<P>* cmd = (macho_dylib_command<P>*)buffer;
	cmd->set_cmd(LC_ID_DYLIB);
	cmd->set_cmdsize(this->getSize());
	cmd->set_name_offset();
	cmd->set_timestamp(1);	// needs to be some constant value that is different than DylibLoadCommandsAtom uses
	cmd->set_current_version(fWriter.fOptions.currentVersion());
	cmd->set_compatibility_version(fWriter.fOptions.compatibilityVersion());
	strcpy((char*)&buffer[sizeof(macho_dylib_command<P>)], fWriter.fOptions.installPath());
}


template <typename A>
void RoutinesLoadCommandsAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t initAddr = fWriter.getAtomLoadAddress(fWriter.fEntryPoint);
	if (fWriter.fEntryPoint->isThumb())
		initAddr |= 1ULL;
	bzero(buffer, sizeof(macho_routines_command<P>));
	macho_routines_command<P>* cmd = (macho_routines_command<P>*)buffer;
	cmd->set_cmd(macho_routines_command<P>::CMD);
	cmd->set_cmdsize(this->getSize());
	cmd->set_init_address(initAddr);
}


template <typename A>
uint64_t SubUmbrellaLoadCommandsAtom<A>::getSize() const
{
	return this->alignedSize(sizeof(macho_sub_umbrella_command<P>) + strlen(fName) + 1);
}

template <typename A>
void SubUmbrellaLoadCommandsAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t size = this->getSize();
	bzero(buffer, size);
	macho_sub_umbrella_command<P>* cmd = (macho_sub_umbrella_command<P>*)buffer;
	cmd->set_cmd(LC_SUB_UMBRELLA);
	cmd->set_cmdsize(this->getSize());
	cmd->set_sub_umbrella_offset();
	strcpy((char*)&buffer[sizeof(macho_sub_umbrella_command<P>)], fName);
}

template <typename A>
void UUIDLoadCommandAtom<A>::generate() 
{
	switch ( fWriter.fOptions.getUUIDMode() ) {
		case Options::kUUIDNone: 
			fEmit = false;
			break;
		case Options::kUUIDRandom:
			::uuid_generate_random(fUUID);
			fEmit = true;
			break;
		case Options::kUUIDContent: 
			bzero(fUUID, 16);
			fEmit = true;
			break;
	}
}

template <typename A>
void UUIDLoadCommandAtom<A>::setContent(const uint8_t uuid[16]) 
{
	memcpy(fUUID, uuid, 16);
}

template <typename A>
void UUIDLoadCommandAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	if (fEmit) {
		uint64_t size = this->getSize();
		bzero(buffer, size);
		macho_uuid_command<P>* cmd = (macho_uuid_command<P>*)buffer;
		cmd->set_cmd(LC_UUID);
		cmd->set_cmdsize(this->getSize());
		cmd->set_uuid((uint8_t*)fUUID);
	}
}


template <typename A>
uint64_t SubLibraryLoadCommandsAtom<A>::getSize() const
{
	return this->alignedSize(sizeof(macho_sub_library_command<P>) + fNameLength + 1);
}

template <typename A>
void SubLibraryLoadCommandsAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t size = this->getSize();
	bzero(buffer, size);
	macho_sub_library_command<P>* cmd = (macho_sub_library_command<P>*)buffer;
	cmd->set_cmd(LC_SUB_LIBRARY);
	cmd->set_cmdsize(this->getSize());
	cmd->set_sub_library_offset();
	strncpy((char*)&buffer[sizeof(macho_sub_library_command<P>)], fNameStart, fNameLength);
	buffer[sizeof(macho_sub_library_command<P>)+fNameLength] = '\0';
}

template <typename A>
uint64_t UmbrellaLoadCommandsAtom<A>::getSize() const
{
	return this->alignedSize(sizeof(macho_sub_framework_command<P>) + strlen(fName) + 1);
}

template <typename A>
void UmbrellaLoadCommandsAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t size = this->getSize();
	bzero(buffer, size);
	macho_sub_framework_command<P>* cmd = (macho_sub_framework_command<P>*)buffer;
	cmd->set_cmd(LC_SUB_FRAMEWORK);
	cmd->set_cmdsize(this->getSize());
	cmd->set_umbrella_offset();
	strcpy((char*)&buffer[sizeof(macho_sub_framework_command<P>)], fName);
}

template <>
uint64_t ThreadsLoadCommandsAtom<ppc>::getSize() const
{
	return this->alignedSize(16 + 40*4);	// base size + PPC_THREAD_STATE_COUNT * 4
}

template <>
uint64_t ThreadsLoadCommandsAtom<ppc64>::getSize() const
{
	return this->alignedSize(16 + 76*4);	// base size + PPC_THREAD_STATE64_COUNT * 4
}

template <>
uint64_t ThreadsLoadCommandsAtom<x86>::getSize() const
{
	return this->alignedSize(16 + 16*4);	// base size + i386_THREAD_STATE_COUNT * 4
}

template <>
uint64_t ThreadsLoadCommandsAtom<x86_64>::getSize() const
{
	return this->alignedSize(16 + x86_THREAD_STATE64_COUNT * 4); 
}

// We should be picking it up from a header
template <>
uint64_t ThreadsLoadCommandsAtom<arm>::getSize() const
{
	return this->alignedSize(16 + 17 * 4); // base size + ARM_THREAD_STATE_COUNT * 4
}

template <>
void ThreadsLoadCommandsAtom<ppc>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t size = this->getSize();
	uint64_t start = fWriter.getAtomLoadAddress(fWriter.fEntryPoint);
	bzero(buffer, size);
	macho_thread_command<ppc::P>* cmd = (macho_thread_command<ppc::P>*)buffer;
	cmd->set_cmd(LC_UNIXTHREAD);
	cmd->set_cmdsize(size);
	cmd->set_flavor(1);				// PPC_THREAD_STATE
	cmd->set_count(40);				// PPC_THREAD_STATE_COUNT;
	cmd->set_thread_register(0, start);
	if ( fWriter.fOptions.hasCustomStack() )
		cmd->set_thread_register(3, fWriter.fOptions.customStackAddr());	// r1
}


template <>
void ThreadsLoadCommandsAtom<ppc64>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t size = this->getSize();
	uint64_t start = fWriter.getAtomLoadAddress(fWriter.fEntryPoint);
	bzero(buffer, size);
	macho_thread_command<ppc64::P>* cmd = (macho_thread_command<ppc64::P>*)buffer;
	cmd->set_cmd(LC_UNIXTHREAD);
	cmd->set_cmdsize(size);
	cmd->set_flavor(5);				// PPC_THREAD_STATE64
	cmd->set_count(76);				// PPC_THREAD_STATE64_COUNT;
	cmd->set_thread_register(0, start);
	if ( fWriter.fOptions.hasCustomStack() )
		cmd->set_thread_register(3, fWriter.fOptions.customStackAddr());	// r1
}

template <>
void ThreadsLoadCommandsAtom<x86>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t size = this->getSize();
	uint64_t start = fWriter.getAtomLoadAddress(fWriter.fEntryPoint);
	bzero(buffer, size);
	macho_thread_command<x86::P>* cmd = (macho_thread_command<x86::P>*)buffer;
	cmd->set_cmd(LC_UNIXTHREAD);
	cmd->set_cmdsize(size);
	cmd->set_flavor(1);				// i386_THREAD_STATE
	cmd->set_count(16);				// i386_THREAD_STATE_COUNT;
	cmd->set_thread_register(10, start);
	if ( fWriter.fOptions.hasCustomStack() )
		cmd->set_thread_register(7, fWriter.fOptions.customStackAddr());	// esp
}

template <>
void ThreadsLoadCommandsAtom<x86_64>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t size = this->getSize();
	uint64_t start = fWriter.getAtomLoadAddress(fWriter.fEntryPoint);
	bzero(buffer, size);
	macho_thread_command<x86_64::P>* cmd = (macho_thread_command<x86_64::P>*)buffer;
	cmd->set_cmd(LC_UNIXTHREAD);
	cmd->set_cmdsize(size);
	cmd->set_flavor(x86_THREAD_STATE64);			
	cmd->set_count(x86_THREAD_STATE64_COUNT);	
	cmd->set_thread_register(16, start);		// rip 
	if ( fWriter.fOptions.hasCustomStack() )
		cmd->set_thread_register(7, fWriter.fOptions.customStackAddr());	// uesp
}

template <>
void ThreadsLoadCommandsAtom<arm>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t size = this->getSize();
	uint64_t start = fWriter.getAtomLoadAddress(fWriter.fEntryPoint);
	if ( fWriter.fEntryPoint->isThumb() )
		start |= 1ULL;
	bzero(buffer, size);
	macho_thread_command<arm::P>* cmd = (macho_thread_command<arm::P>*)buffer;
	cmd->set_cmd(LC_UNIXTHREAD);
	cmd->set_cmdsize(size);
	cmd->set_flavor(1);			
	cmd->set_count(17);	
	cmd->set_thread_register(15, start);		// pc
	if ( fWriter.fOptions.hasCustomStack() )
		cmd->set_thread_register(13, fWriter.fOptions.customStackAddr());	// FIXME: sp?
}

template <typename A>
uint64_t RPathLoadCommandsAtom<A>::getSize() const
{
	return this->alignedSize(sizeof(macho_rpath_command<P>) + strlen(fPath) + 1);
}

template <typename A>
void RPathLoadCommandsAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t size = this->getSize();
	bzero(buffer, size);
	macho_rpath_command<P>* cmd = (macho_rpath_command<P>*)buffer;
	cmd->set_cmd(LC_RPATH);
	cmd->set_cmdsize(this->getSize());
	cmd->set_path_offset();
	strcpy((char*)&buffer[sizeof(macho_rpath_command<P>)], fPath);
}



template <typename A>
void EncryptionLoadCommandsAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t size = this->getSize();
	bzero(buffer, size);
	macho_encryption_info_command<P>* cmd = (macho_encryption_info_command<P>*)buffer;
	cmd->set_cmd(LC_ENCRYPTION_INFO);
	cmd->set_cmdsize(this->getSize());
	cmd->set_cryptoff(fStartOffset);
	cmd->set_cryptsize(fEndOffset-fStartOffset);
	cmd->set_cryptid(0);
}



template <typename A>
void LoadCommandsPaddingAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	bzero(buffer, fSize);
}

template <typename A>
void LoadCommandsPaddingAtom<A>::setSize(uint64_t newSize) 
{ 
	fSize = newSize; 
	// this resizing by-passes the way fLargestAtomSize is set, so re-check here
	if ( fWriter.fLargestAtomSize < newSize )
		fWriter.fLargestAtomSize = newSize;
}

template <typename A>
void UnwindInfoAtom<A>::addUnwindInfo(ObjectFile::Atom* func, uint32_t offset, uint32_t encoding, 
										ObjectFile::Reference* fdeRef, ObjectFile::Reference* lsdaRef,
										ObjectFile::Atom* personalityPointer) 
{ 
	Info info;
	info.func = func;
	if ( fdeRef != NULL )  
		info.fde = &fdeRef->getTarget();
	else 
		info.fde = NULL;
	if ( lsdaRef != NULL )  {
		info.lsda = &lsdaRef->getTarget();
		info.lsdaOffset = lsdaRef->getTargetOffset();
	}
	else {
		info.lsda = NULL;
		info.lsdaOffset = 0;
	}
	info.personalityPointer = personalityPointer;
	info.encoding = encoding;
	fInfos.push_back(info);
	//fprintf(stderr, "addUnwindInfo() encoding=0x%08X, lsda=%p, lsdaOffset=%d, person=%p, func=%s\n", 
	//				encoding, info.lsda, info.lsdaOffset, personalityPointer, func->getDisplayName());
}

template <>
bool UnwindInfoAtom<x86>::encodingMeansUseDwarf(compact_unwind_encoding_t encoding)
{
	return ( (encoding & UNWIND_X86_MODE_MASK) == UNWIND_X86_MODE_DWARF);
}

template <>
bool UnwindInfoAtom<x86_64>::encodingMeansUseDwarf(compact_unwind_encoding_t encoding)
{
	return ( (encoding & UNWIND_X86_64_MODE_MASK) == UNWIND_X86_64_MODE_DWARF);
}

template <typename A>
bool UnwindInfoAtom<A>::encodingMeansUseDwarf(compact_unwind_encoding_t encoding)
{
	return false;
}


template <typename A>
void UnwindInfoAtom<A>::compressDuplicates(std::vector<Info>& uniqueInfos)
{
	// build new list removing entries where next function has same encoding 
	uniqueInfos.reserve(fInfos.size());
	Info last;
	last.func = NULL;
	last.lsda = NULL;
	last.lsdaOffset = 0;
	last.personalityPointer = NULL;
	last.encoding = 0xFFFFFFFF;
	for(typename std::vector<Info>::iterator it=fInfos.begin(); it != fInfos.end(); ++it) {
		Info& newInfo = *it;
		bool newNeedsDwarf = encodingMeansUseDwarf(newInfo.encoding);
		// remove infos which have same encoding and personalityPointer as last one
		if ( newNeedsDwarf || (newInfo.encoding != last.encoding) || (newInfo.personalityPointer != last.personalityPointer) 
						|| (newInfo.lsda != NULL) || (last.lsda != NULL) ) {
			uniqueInfos.push_back(newInfo);
		}
		last = newInfo;
	}
	//fprintf(stderr, "compressDuplicates() fInfos.size()=%lu, uniqueInfos.size()=%lu\n", fInfos.size(), uniqueInfos.size());
}

template <typename A>
void UnwindInfoAtom<A>::findCommonEncoding(const std::vector<Info>& uniqueInfos, std::map<uint32_t, unsigned int>& commonEncodings)
{
	// scan infos to get frequency counts for each encoding
	std::map<uint32_t, unsigned int> encodingsUsed;
	unsigned int mostCommonEncodingUsageCount = 0;
	for(typename std::vector<Info>::const_iterator it=uniqueInfos.begin(); it != uniqueInfos.end(); ++it) {
		// never put dwarf into common table
		if ( encodingMeansUseDwarf(it->encoding) )
			continue;
		std::map<uint32_t, unsigned int>::iterator pos = encodingsUsed.find(it->encoding);
		if ( pos == encodingsUsed.end() ) {
			encodingsUsed[it->encoding] = 1;
		}
		else {
			encodingsUsed[it->encoding] += 1;
			if ( mostCommonEncodingUsageCount < encodingsUsed[it->encoding] )
				mostCommonEncodingUsageCount = encodingsUsed[it->encoding];
		}
	}
	// put the most common encodings into the common table, but at most 127 of them
	for(unsigned int usages=mostCommonEncodingUsageCount; usages > 1; --usages) {
		for (std::map<uint32_t, unsigned int>::iterator euit=encodingsUsed.begin(); euit != encodingsUsed.end(); ++euit) {
			if ( euit->second == usages ) {
				unsigned int size = commonEncodings.size();
				if ( size < 127 ) {
					commonEncodings[euit->first] = size;
				}
			}
		}
	}
}

template <typename A>
void UnwindInfoAtom<A>::makeLsdaIndex(const std::vector<Info>& uniqueInfos, std::map<ObjectFile::Atom*, uint32_t>& lsdaIndexOffsetMap)
{
	for(typename std::vector<Info>::const_iterator it=uniqueInfos.begin(); it != uniqueInfos.end(); ++it) {
		lsdaIndexOffsetMap[it->func] = fLSDAIndex.size() * sizeof(macho_unwind_info_section_header_lsda_index_entry<P>);
		if ( it->lsda != NULL ) {
			LSDAEntry entry;
			entry.func = it->func;
			entry.lsda = it->lsda;
			entry.lsdaOffset = it->lsdaOffset;
			fLSDAIndex.push_back(entry);
		}
	}
}

template <typename A>
void UnwindInfoAtom<A>::makePersonalityIndex(std::vector<Info>& uniqueInfos)
{
	for(typename std::vector<Info>::iterator it=uniqueInfos.begin(); it != uniqueInfos.end(); ++it) {
		if ( it->personalityPointer != NULL ) {
			std::map<ObjectFile::Atom*, uint32_t>::iterator pos = fPersonalityIndexMap.find(it->personalityPointer);
			if ( pos == fPersonalityIndexMap.end() ) {
				const uint32_t nextIndex = fPersonalityIndexMap.size() + 1;
				fPersonalityIndexMap[it->personalityPointer] = nextIndex;
			}
			uint32_t personalityIndex = fPersonalityIndexMap[it->personalityPointer];
			it->encoding |= (personalityIndex << (__builtin_ctz(UNWIND_PERSONALITY_MASK)) );
		}
	}
}

template <typename A>
unsigned int UnwindInfoAtom<A>::makeRegularSecondLevelPage(const std::vector<Info>& uniqueInfos, uint32_t pageSize,  
															unsigned int endIndex, uint8_t*& pageEnd)
{
	const unsigned int maxEntriesPerPage = (pageSize - sizeof(unwind_info_regular_second_level_page_header))/sizeof(unwind_info_regular_second_level_entry);
	const unsigned int entriesToAdd = ((endIndex > maxEntriesPerPage) ? maxEntriesPerPage : endIndex);
	uint8_t* pageStart = pageEnd 
						- entriesToAdd*sizeof(unwind_info_regular_second_level_entry) 
						- sizeof(unwind_info_regular_second_level_page_header);
	macho_unwind_info_regular_second_level_page_header<P>* page = (macho_unwind_info_regular_second_level_page_header<P>*)pageStart;
	page->set_kind(UNWIND_SECOND_LEVEL_REGULAR);
	page->set_entryPageOffset(sizeof(macho_unwind_info_regular_second_level_page_header<P>));
	page->set_entryCount(entriesToAdd);
	macho_unwind_info_regular_second_level_entry<P>* entryTable = (macho_unwind_info_regular_second_level_entry<P>*)(pageStart + page->entryPageOffset());
	for (unsigned int i=0; i < entriesToAdd; ++i) {
		const Info& info = uniqueInfos[endIndex-entriesToAdd+i];
		entryTable[i].set_functionOffset(0);
		entryTable[i].set_encoding(info.encoding);
		RegFixUp fixup;
		fixup.contentPointer = (uint8_t*)(&entryTable[i]);
		fixup.func = info.func;
		fixup.fde = ( encodingMeansUseDwarf(info.encoding) ? info.fde : NULL );
 		fRegFixUps.push_back(fixup);
	}
	//fprintf(stderr, "regular page with %u entries\n", entriesToAdd);
	pageEnd = pageStart;
	return endIndex - entriesToAdd;
}


template <typename A>
unsigned int UnwindInfoAtom<A>::makeCompressedSecondLevelPage(const std::vector<Info>& uniqueInfos,   
															const std::map<uint32_t,unsigned int> commonEncodings,  
															uint32_t pageSize, unsigned int endIndex, uint8_t*& pageEnd)
{
	const bool log = false;
	if (log) fprintf(stderr, "makeCompressedSecondLevelPage(pageSize=%u, endIndex=%u)\n", pageSize, endIndex);
	// first pass calculates how many compressed entries we could fit in this sized page
	// keep adding entries to page until:
	//  1) encoding table plus entry table plus header exceed page size
	//  2) the file offset delta from the first to last function > 24 bits
	//  3) custom encoding index reachs 255
	//  4) run out of uniqueInfos to encode
	std::map<uint32_t, unsigned int> pageSpecificEncodings;
	uint32_t space4 =  (pageSize - sizeof(unwind_info_compressed_second_level_page_header))/sizeof(uint32_t);
	std::vector<uint8_t> encodingIndexes;
	int index = endIndex-1;
	int entryCount = 0;
	uint64_t lastEntryAddress = uniqueInfos[index].func->getAddress();
	bool canDo = true;
	while ( canDo && (index >= 0) ) {
		const Info& info = uniqueInfos[index--];
		// compute encoding index
		unsigned int encodingIndex;
		std::map<uint32_t, unsigned int>::const_iterator pos = commonEncodings.find(info.encoding);
		if ( pos != commonEncodings.end() ) {
			encodingIndex = pos->second;
		}
		else {
			// no commmon entry, so add one on this page
			uint32_t encoding = info.encoding;
			if ( encodingMeansUseDwarf(encoding) ) {
				// make unique pseudo encoding so this dwarf will gets is own encoding entry slot
				encoding += (index+1);
			}
			std::map<uint32_t, unsigned int>::iterator ppos = pageSpecificEncodings.find(encoding);
			if ( ppos != pageSpecificEncodings.end() ) {
				encodingIndex = pos->second;
			}
			else {
				encodingIndex = commonEncodings.size() + pageSpecificEncodings.size();
				if ( encodingIndex <= 255 ) {
					pageSpecificEncodings[encoding] = encodingIndex;
				}
				else {
					canDo = false; // case 3)
					if (log) fprintf(stderr, "end of compressed page with %u entries, %lu custom encodings because too many custom encodings\n", 
											entryCount, pageSpecificEncodings.size());
				}
			}
		}
		if ( canDo ) 
			encodingIndexes.push_back(encodingIndex);
		// compute function offset
		uint32_t funcOffsetWithInPage = lastEntryAddress - info.func->getAddress();
		if ( funcOffsetWithInPage > 0x00FFFF00 ) {
			// don't use 0x00FFFFFF because addresses may vary after atoms are laid out again
			canDo = false; // case 2)
			if (log) fprintf(stderr, "can't use compressed page with %u entries because function offset too big\n", entryCount);
		}
		else {
			++entryCount;
		}
		// check room for entry
		if ( (pageSpecificEncodings.size()+entryCount) >= space4 ) {
			canDo = false; // case 1)
			--entryCount;
			if (log) fprintf(stderr, "end of compressed page with %u entries because full\n", entryCount);
		}
		//if (log) fprintf(stderr, "space4=%d, pageSpecificEncodings.size()=%ld, entryCount=%d\n", space4, pageSpecificEncodings.size(), entryCount);
	}
	
	// check for cases where it would be better to use a regular (non-compressed) page
	const unsigned int compressPageUsed = sizeof(unwind_info_compressed_second_level_page_header) 
								+ pageSpecificEncodings.size()*sizeof(uint32_t)
								+ entryCount*sizeof(uint32_t);
	if ( (compressPageUsed < (pageSize-4) && (index >= 0) ) ) {
		const int regularEntriesPerPage = (pageSize - sizeof(unwind_info_regular_second_level_page_header))/sizeof(unwind_info_regular_second_level_entry);
		if ( entryCount < regularEntriesPerPage ) {
			return makeRegularSecondLevelPage(uniqueInfos, pageSize, endIndex, pageEnd);
		}
	}
	
	// check if we need any padding because adding another entry would take 8 bytes but only have room for 4
	uint32_t pad = 0;
	if ( compressPageUsed == (pageSize-4) )
		pad = 4;
	
	// second pass fills in page 
	uint8_t* pageStart = pageEnd - compressPageUsed - pad;
	macho_unwind_info_compressed_second_level_page_header<P>* page = (macho_unwind_info_compressed_second_level_page_header<P>*)pageStart;
	page->set_kind(UNWIND_SECOND_LEVEL_COMPRESSED);
	page->set_entryPageOffset(sizeof(macho_unwind_info_compressed_second_level_page_header<P>));
	page->set_entryCount(entryCount);
	page->set_encodingsPageOffset(page->entryPageOffset()+entryCount*sizeof(uint32_t));
	page->set_encodingsCount(pageSpecificEncodings.size());
	uint32_t* const encodingsArray = (uint32_t*)&pageStart[page->encodingsPageOffset()];
	// fill in entry table
	uint32_t* const entiresArray = (uint32_t*)&pageStart[page->entryPageOffset()];
	ObjectFile::Atom* firstFunc = uniqueInfos[endIndex-entryCount].func;
	for(unsigned int i=endIndex-entryCount; i < endIndex; ++i) {
		const Info& info = uniqueInfos[i];
		uint8_t encodingIndex;
		if ( encodingMeansUseDwarf(info.encoding) ) {
			// dwarf entries are always in page specific encodings
			encodingIndex = pageSpecificEncodings[info.encoding+i];
		}
		else {
			std::map<uint32_t, unsigned int>::const_iterator pos = commonEncodings.find(info.encoding);
			if ( pos != commonEncodings.end() ) 
				encodingIndex = pos->second;
			else 
				encodingIndex = pageSpecificEncodings[info.encoding];
		}
		uint32_t entryIndex = i - endIndex + entryCount;
		A::P::E::set32(entiresArray[entryIndex], encodingIndex << 24);
		CompressedFixUp			funcStartFixUp;
		funcStartFixUp.contentPointer = (uint8_t*)(&entiresArray[entryIndex]);
		funcStartFixUp.func = info.func;
		funcStartFixUp.fromFunc = firstFunc;
		fCompressedFixUps.push_back(funcStartFixUp);
		if ( encodingMeansUseDwarf(info.encoding) ) {
			CompressedEncodingFixUp	dwarfStartFixup;
			dwarfStartFixup.contentPointer = (uint8_t*)(&encodingsArray[encodingIndex-commonEncodings.size()]); 
			dwarfStartFixup.fde = info.fde;
			fCompressedEncodingFixUps.push_back(dwarfStartFixup);
		}
	}
	// fill in encodings table
	for(std::map<uint32_t, unsigned int>::const_iterator it = pageSpecificEncodings.begin(); it != pageSpecificEncodings.end(); ++it) {
		A::P::E::set32(encodingsArray[it->second-commonEncodings.size()], it->first);
	}
	
	if (log) fprintf(stderr, "compressed page with %u entries, %lu custom encodings\n", entryCount, pageSpecificEncodings.size());
	
	// update pageEnd;
	pageEnd = pageStart;
	return endIndex-entryCount;  // endIndex for next page
}

template <> void UnwindInfoAtom<ppc>::generate() { }
template <> void UnwindInfoAtom<ppc64>::generate() { }
template <> void UnwindInfoAtom<arm>::generate() { }


template <typename A>
void UnwindInfoAtom<A>::generate()
{
	// only generate table if there are functions with unwind info
	if ( fInfos.size() > 0 ) { 		
		// find offset of end of __unwind_info section 
		SectionInfo* unwindSectionInfo = (SectionInfo*)this->getSection();

		// build new list that has proper offsetInImage and remove entries where next function has same encoding 
		std::vector<Info> uniqueInfos;
		this->compressDuplicates(uniqueInfos);
		
		// build personality index, update encodings with personality index
		this->makePersonalityIndex(uniqueInfos);
		if ( fPersonalityIndexMap.size() > 3 )
			throw "too many personality routines for compact unwind to encode";
		
		// put the most common encodings into the common table, but at most 127 of them
		std::map<uint32_t, unsigned int> commonEncodings;
		this->findCommonEncoding(uniqueInfos, commonEncodings);
		
		// build lsda index
		std::map<ObjectFile::Atom*, uint32_t> lsdaIndexOffsetMap;
		this->makeLsdaIndex(uniqueInfos, lsdaIndexOffsetMap);
		
		// calculate worst case size for all unwind info pages when allocating buffer
		const unsigned int entriesPerRegularPage = (4096-sizeof(unwind_info_regular_second_level_page_header))/sizeof(unwind_info_regular_second_level_entry);
		const unsigned int pageCount = ((uniqueInfos.size() - 1)/entriesPerRegularPage) + 1;
		fPagesContentForDelete = (uint8_t*)calloc(pageCount,4096);
		fPagesSize = 0;
		if ( fPagesContentForDelete == NULL )
			throw "could not allocate space for compact unwind info";
		ObjectFile::Atom* secondLevelFirstFuncs[pageCount*3];
		uint8_t* secondLevelPagesStarts[pageCount*3];
		
		// make last second level page smaller so that all other second level pages can be page aligned
		uint32_t maxLastPageSize = unwindSectionInfo->fFileOffset % 4096;
		uint32_t tailPad = 0;
		if ( maxLastPageSize < 128 ) {
			tailPad = maxLastPageSize;
			maxLastPageSize = 4096;
		}

		// fill in pages in reverse order
		unsigned int endIndex = uniqueInfos.size();
		unsigned int secondLevelPageCount = 0;
		uint8_t* pageEnd = &fPagesContentForDelete[pageCount*4096];
		uint32_t pageSize = maxLastPageSize;
		while ( endIndex > 0 ) {
			endIndex = makeCompressedSecondLevelPage(uniqueInfos, commonEncodings, pageSize, endIndex, pageEnd);
			secondLevelPagesStarts[secondLevelPageCount] = pageEnd;
			secondLevelFirstFuncs[secondLevelPageCount] = uniqueInfos[endIndex].func;
			++secondLevelPageCount;
			pageSize = 4096;  // last page can be odd size, make rest up to 4096 bytes in size
		}
		fPagesContent = pageEnd;
		fPagesSize = &fPagesContentForDelete[pageCount*4096] - pageEnd;

		// calculate section layout
		const uint32_t commonEncodingsArraySectionOffset = sizeof(macho_unwind_info_section_header<P>);
		const uint32_t commonEncodingsArrayCount = commonEncodings.size();
		const uint32_t commonEncodingsArraySize = commonEncodingsArrayCount * sizeof(compact_unwind_encoding_t);
		const uint32_t personalityArraySectionOffset = commonEncodingsArraySectionOffset + commonEncodingsArraySize;
		const uint32_t personalityArrayCount = fPersonalityIndexMap.size();
		const uint32_t personalityArraySize = personalityArrayCount * sizeof(uint32_t);
		const uint32_t indexSectionOffset = personalityArraySectionOffset + personalityArraySize;
		const uint32_t indexCount = secondLevelPageCount+1;
		const uint32_t indexSize = indexCount * sizeof(macho_unwind_info_section_header_index_entry<P>);
		const uint32_t lsdaIndexArraySectionOffset = indexSectionOffset + indexSize;
		const uint32_t lsdaIndexArrayCount = fLSDAIndex.size();
		const uint32_t lsdaIndexArraySize = lsdaIndexArrayCount * sizeof(macho_unwind_info_section_header_lsda_index_entry<P>);
		const uint32_t headerEndSectionOffset = lsdaIndexArraySectionOffset + lsdaIndexArraySize;


		// allocate and fill in section header
		fHeaderSize = headerEndSectionOffset;
		fHeaderContent = new uint8_t[fHeaderSize];
		bzero(fHeaderContent, fHeaderSize);
		macho_unwind_info_section_header<P>* sectionHeader = (macho_unwind_info_section_header<P>*)fHeaderContent;
		sectionHeader->set_version(UNWIND_SECTION_VERSION);
		sectionHeader->set_commonEncodingsArraySectionOffset(commonEncodingsArraySectionOffset);
		sectionHeader->set_commonEncodingsArrayCount(commonEncodingsArrayCount);
		sectionHeader->set_personalityArraySectionOffset(personalityArraySectionOffset);
		sectionHeader->set_personalityArrayCount(personalityArrayCount);
		sectionHeader->set_indexSectionOffset(indexSectionOffset);
		sectionHeader->set_indexCount(indexCount);
		
		// copy common encodings
		uint32_t* commonEncodingsTable = (uint32_t*)&fHeaderContent[commonEncodingsArraySectionOffset];
		for (std::map<uint32_t, unsigned int>::iterator it=commonEncodings.begin(); it != commonEncodings.end(); ++it)
			A::P::E::set32(commonEncodingsTable[it->second], it->first);
			
		// make references for personality entries
		uint32_t* personalityArray = (uint32_t*)&fHeaderContent[sectionHeader->personalityArraySectionOffset()];
		for (std::map<ObjectFile::Atom*, unsigned int>::iterator it=fPersonalityIndexMap.begin(); it != fPersonalityIndexMap.end(); ++it) {
			uint32_t offset = (uint8_t*)&personalityArray[it->second-1] - fHeaderContent;
			fReferences.push_back(new WriterReference<A>(offset, A::kImageOffset32, it->first));
		}

		// build first level index and references
		macho_unwind_info_section_header_index_entry<P>* indexTable = (macho_unwind_info_section_header_index_entry<P>*)&fHeaderContent[indexSectionOffset];
		for (unsigned int i=0; i < secondLevelPageCount; ++i) {
			unsigned int reverseIndex = secondLevelPageCount - 1 - i;
			indexTable[i].set_functionOffset(0);
			indexTable[i].set_secondLevelPagesSectionOffset(secondLevelPagesStarts[reverseIndex]-fPagesContent+headerEndSectionOffset);
			indexTable[i].set_lsdaIndexArraySectionOffset(lsdaIndexOffsetMap[secondLevelFirstFuncs[reverseIndex]]+lsdaIndexArraySectionOffset); 
			uint32_t refOffset = (uint8_t*)&indexTable[i] - fHeaderContent;
			fReferences.push_back(new WriterReference<A>(refOffset, A::kImageOffset32, secondLevelFirstFuncs[reverseIndex]));
		}
		indexTable[secondLevelPageCount].set_functionOffset(0);
		indexTable[secondLevelPageCount].set_secondLevelPagesSectionOffset(0);
		indexTable[secondLevelPageCount].set_lsdaIndexArraySectionOffset(lsdaIndexArraySectionOffset+lsdaIndexArraySize); 
		fReferences.push_back(new WriterReference<A>((uint8_t*)&indexTable[secondLevelPageCount] - fHeaderContent, A::kImageOffset32, 
														fInfos.back().func, fInfos.back().func->getSize()+1));
		
		// build lsda references
		uint32_t lsdaEntrySectionOffset = lsdaIndexArraySectionOffset;
		for (typename std::vector<LSDAEntry>::iterator it = fLSDAIndex.begin(); it != fLSDAIndex.end(); ++it) {
			fReferences.push_back(new WriterReference<A>(lsdaEntrySectionOffset, A::kImageOffset32, it->func));
			fReferences.push_back(new WriterReference<A>(lsdaEntrySectionOffset+4, A::kImageOffset32, it->lsda, it->lsdaOffset));
			lsdaEntrySectionOffset += sizeof(unwind_info_section_header_lsda_index_entry);
		}
		
		// make references for regular second level entries
		for (typename std::vector<RegFixUp>::iterator it = fRegFixUps.begin(); it != fRegFixUps.end(); ++it) {
			uint32_t offset = (it->contentPointer - fPagesContent) + fHeaderSize;
			fReferences.push_back(new WriterReference<A>(offset, A::kImageOffset32, it->func));
			if ( it->fde != NULL )
				fReferences.push_back(new WriterReference<A>(offset+4, A::kSectionOffset24, it->fde));
		}
		// make references for compressed second level entries
		for (typename std::vector<CompressedFixUp>::iterator it = fCompressedFixUps.begin(); it != fCompressedFixUps.end(); ++it) {
			uint32_t offset = (it->contentPointer - fPagesContent) + fHeaderSize;
			fReferences.push_back(new WriterReference<A>(offset, A::kPointerDiff24, it->func, 0, it->fromFunc, 0));
		}
		for (typename std::vector<CompressedEncodingFixUp>::iterator it = fCompressedEncodingFixUps.begin(); it != fCompressedEncodingFixUps.end(); ++it) {
			uint32_t offset = (it->contentPointer - fPagesContent) + fHeaderSize;
			fReferences.push_back(new WriterReference<A>(offset, A::kSectionOffset24, it->fde));
		}
				
		// update section record with new size
		unwindSectionInfo->fSize = this->getSize();
		
		// alter alignment so this section lays out so second level tables are page aligned
		if ( secondLevelPageCount > 2 )
			fAlignment = ObjectFile::Alignment(12, (unwindSectionInfo->fFileOffset - this->getSize()) % 4096);
	}

}




template <typename A>
void UnwindInfoAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	memcpy(buffer, fHeaderContent, fHeaderSize);
	memcpy(&buffer[fHeaderSize], fPagesContent, fPagesSize);
}



template <typename A>
uint64_t LinkEditAtom<A>::getFileOffset() const
{
	return ((SectionInfo*)this->getSection())->fFileOffset + this->getSectionOffset();
}


template <typename A>
uint64_t SectionRelocationsLinkEditAtom<A>::getSize() const
{
	return fWriter.fSectionRelocs.size() * sizeof(macho_relocation_info<P>);
}

template <typename A>
void SectionRelocationsLinkEditAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	memcpy(buffer, &fWriter.fSectionRelocs[0], this->getSize());
}


template <typename A>
uint64_t LocalRelocationsLinkEditAtom<A>::getSize() const
{
	return fWriter.fInternalRelocs.size() * sizeof(macho_relocation_info<P>);
}

template <typename A>
void LocalRelocationsLinkEditAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	memcpy(buffer, &fWriter.fInternalRelocs[0], this->getSize());
}



template <typename A>
uint64_t SymbolTableLinkEditAtom<A>::getSize() const
{
	return fWriter.fSymbolTableCount * sizeof(macho_nlist<P>);
}

template <typename A>
void SymbolTableLinkEditAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	memcpy(buffer, fWriter.fSymbolTable, this->getSize());
}

template <typename A>
uint64_t ExternalRelocationsLinkEditAtom<A>::getSize() const
{
	return fWriter.fExternalRelocs.size() * sizeof(macho_relocation_info<P>);
}

template <typename A>
void ExternalRelocationsLinkEditAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	std::sort(fWriter.fExternalRelocs.begin(), fWriter.fExternalRelocs.end(), ExternalRelocSorter<P>());
	memcpy(buffer, &fWriter.fExternalRelocs[0], this->getSize());
}



template <typename A>
uint64_t IndirectTableLinkEditAtom<A>::getSize() const
{
	return fTable.size() * sizeof(uint32_t);
}

template <typename A>
void IndirectTableLinkEditAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t size = this->getSize();
	bzero(buffer, size);
	const uint32_t indirectTableSize = fTable.size();
	uint32_t* indirectTable = (uint32_t*)buffer;
	for(std::vector<IndirectEntry>::const_iterator it = fTable.begin(); it != fTable.end(); ++it) {
		if ( it->indirectIndex < indirectTableSize )
			A::P::E::set32(indirectTable[it->indirectIndex], it->symbolIndex);
		else 
			throwf("malformed indirect table. size=%d, index=%d", indirectTableSize, it->indirectIndex);
	}
}



template <typename A>
uint64_t ModuleInfoLinkEditAtom<A>::getSize() const
{
	return fWriter.fSymbolTableExportCount*sizeof(macho_dylib_table_of_contents<P>) 
			+ sizeof(macho_dylib_module<P>) 
			+ this->getReferencesCount()*sizeof(uint32_t);
}

template <typename A>
uint32_t ModuleInfoLinkEditAtom<A>::getTableOfContentsFileOffset() const
{
	return this->getFileOffset();
}

template <typename A>
uint32_t ModuleInfoLinkEditAtom<A>::getModuleTableFileOffset() const
{
	return this->getFileOffset() + fWriter.fSymbolTableExportCount*sizeof(macho_dylib_table_of_contents<P>);
}

template <typename A>
uint32_t ModuleInfoLinkEditAtom<A>::getReferencesFileOffset() const
{
	return this->getModuleTableFileOffset() + sizeof(macho_dylib_module<P>);
}

template <typename A>
uint32_t ModuleInfoLinkEditAtom<A>::getReferencesCount() const
{
	return fWriter.fSymbolTableExportCount + fWriter.fSymbolTableImportCount;
}

template <typename A>
void ModuleInfoLinkEditAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t size = this->getSize();
	bzero(buffer, size);
	// create toc.  The symbols are already sorted, they are all in the smae module
	macho_dylib_table_of_contents<P>* p = (macho_dylib_table_of_contents<P>*)buffer;
	for(uint32_t i=0; i < fWriter.fSymbolTableExportCount; ++i, ++p) {
		p->set_symbol_index(fWriter.fSymbolTableExportStartIndex+i);
		p->set_module_index(0);
	}
	// create module table (one entry)
	pint_t objcModuleSectionStart = 0;
	pint_t objcModuleSectionSize = 0;
	uint16_t numInits = 0;
	uint16_t numTerms = 0;
	std::vector<SegmentInfo*>& segmentInfos = fWriter.fSegmentInfos;
	for (std::vector<SegmentInfo*>::iterator segit = segmentInfos.begin(); segit != segmentInfos.end(); ++segit) {
		std::vector<SectionInfo*>& sectionInfos = (*segit)->fSections;
		if ( strcmp((*segit)->fName, "__DATA") == 0 ) {
			for (std::vector<SectionInfo*>::iterator sectit = sectionInfos.begin(); sectit != sectionInfos.end(); ++sectit) {
				if ( strcmp((*sectit)->fSectionName, "__mod_init_func") == 0 ) 
					numInits = (*sectit)->fSize / sizeof(typename A::P::uint_t);
				else if ( strcmp((*sectit)->fSectionName, "__mod_term_func") == 0 ) 
					numTerms = (*sectit)->fSize / sizeof(typename A::P::uint_t);
			}
		}
		else if ( strcmp((*segit)->fName, "__OBJC") == 0 ) {
			for (std::vector<SectionInfo*>::iterator sectit = sectionInfos.begin(); sectit != sectionInfos.end(); ++sectit) {
				SectionInfo* sectInfo = (*sectit);
				if ( strcmp(sectInfo->fSectionName, "__module_info") == 0 ) {
					objcModuleSectionStart = sectInfo->getBaseAddress();
					objcModuleSectionSize  = sectInfo->fSize;
				}
			}
		}
	}
	macho_dylib_module<P>* module = (macho_dylib_module<P>*)&buffer[fWriter.fSymbolTableExportCount*sizeof(macho_dylib_table_of_contents<P>)];
	module->set_module_name(fModuleNameOffset);
	module->set_iextdefsym(fWriter.fSymbolTableExportStartIndex);
	module->set_nextdefsym(fWriter.fSymbolTableExportCount);
	module->set_irefsym(0);
	module->set_nrefsym(this->getReferencesCount());
	module->set_ilocalsym(fWriter.fSymbolTableStabsStartIndex);
	module->set_nlocalsym(fWriter.fSymbolTableStabsCount+fWriter.fSymbolTableLocalCount);
	module->set_iextrel(0);
	module->set_nextrel(fWriter.fExternalRelocs.size());
	module->set_iinit_iterm(0,0);
	module->set_ninit_nterm(numInits,numTerms);
	module->set_objc_module_info_addr(objcModuleSectionStart);	
	module->set_objc_module_info_size(objcModuleSectionSize);	
	// create reference table
	macho_dylib_reference<P>* ref = (macho_dylib_reference<P>*)((uint8_t*)module + sizeof(macho_dylib_module<P>));
	for(uint32_t i=0; i < fWriter.fSymbolTableExportCount; ++i, ++ref) {
		ref->set_isym(fWriter.fSymbolTableExportStartIndex+i);
		ref->set_flags(REFERENCE_FLAG_DEFINED);
	}
	for(uint32_t i=0; i < fWriter.fSymbolTableImportCount; ++i, ++ref) {
		ref->set_isym(fWriter.fSymbolTableImportStartIndex+i);
		std::map<const ObjectFile::Atom*,ObjectFile::Atom*>::iterator pos = fWriter.fStubsMap.find(fWriter.fImportedAtoms[i]);
		if ( pos != fWriter.fStubsMap.end() )
			ref->set_flags(REFERENCE_FLAG_UNDEFINED_LAZY);
		else
			ref->set_flags(REFERENCE_FLAG_UNDEFINED_NON_LAZY);
	}
}



template <typename A>
StringsLinkEditAtom<A>::StringsLinkEditAtom(Writer<A>& writer)
	: LinkEditAtom<A>(writer), fCurrentBuffer(NULL), fCurrentBufferUsed(0)
{
	fCurrentBuffer = new char[kBufferSize];
	// burn first byte of string pool (so zero is never a valid string offset)
	fCurrentBuffer[fCurrentBufferUsed++] = ' ';
	// make offset 1 always point to an empty string
	fCurrentBuffer[fCurrentBufferUsed++] = '\0';
}

template <typename A>
uint64_t StringsLinkEditAtom<A>::getSize() const
{
	// align size
	return (kBufferSize * fFullBuffers.size() + fCurrentBufferUsed + sizeof(typename A::P::uint_t) - 1) & (-sizeof(typename A::P::uint_t));
}

template <typename A>
void StringsLinkEditAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t offset = 0;
	for (unsigned int i=0; i < fFullBuffers.size(); ++i) {
		memcpy(&buffer[offset], fFullBuffers[i], kBufferSize);
		offset += kBufferSize;
	}
	memcpy(&buffer[offset], fCurrentBuffer, fCurrentBufferUsed);
	// zero fill end to align
	offset += fCurrentBufferUsed;
	while ( (offset % sizeof(typename A::P::uint_t)) != 0 )
		buffer[offset++] = 0;
}

template <typename A>
int32_t StringsLinkEditAtom<A>::add(const char* name)
{
	int32_t offset = kBufferSize * fFullBuffers.size() + fCurrentBufferUsed;
	int lenNeeded = strlcpy(&fCurrentBuffer[fCurrentBufferUsed], name, kBufferSize-fCurrentBufferUsed)+1;
	if ( (fCurrentBufferUsed+lenNeeded) < kBufferSize ) {
		fCurrentBufferUsed += lenNeeded;
	}
	else {
		int copied = kBufferSize-fCurrentBufferUsed-1;
		// change trailing '\0' that strlcpy added to real char
		fCurrentBuffer[kBufferSize-1] = name[copied];
		// alloc next buffer
		fFullBuffers.push_back(fCurrentBuffer);
		fCurrentBuffer = new char[kBufferSize];
		fCurrentBufferUsed = 0;
		// append rest of string
		this->add(&name[copied+1]);
	}
	return offset;
}


template <typename A>
int32_t StringsLinkEditAtom<A>::addUnique(const char* name)
{
	StringToOffset::iterator pos = fUniqueStrings.find(name);
	if ( pos != fUniqueStrings.end() ) {
		return pos->second;
	}
	else {
		int32_t offset = this->add(name);
		fUniqueStrings[name] = offset;
		return offset;
	}
}


template <typename A>
const char* StringsLinkEditAtom<A>::stringForIndex(int32_t index) const
{
	int32_t currentBufferStartIndex = kBufferSize * fFullBuffers.size();
	int32_t maxIndex = currentBufferStartIndex + fCurrentBufferUsed;
	// check for out of bounds
	if ( index > maxIndex )
		return "";
	// check for index in fCurrentBuffer
	if ( index > currentBufferStartIndex )
		return &fCurrentBuffer[index-currentBufferStartIndex];
	// otherwise index is in a full buffer
	uint32_t fullBufferIndex = index/kBufferSize;
	return &fFullBuffers[fullBufferIndex][index-(kBufferSize*fullBufferIndex)];
}



template <typename A>
BranchIslandAtom<A>::BranchIslandAtom(Writer<A>& writer, const char* name, int islandRegion, ObjectFile::Atom& target, uint32_t targetOffset)
 : WriterAtom<A>(writer, Segment::fgTextSegment), fTarget(target), fTargetOffset(targetOffset)
{
	char* buf = new char[strlen(name)+32];
	if ( targetOffset == 0 ) {
		if ( islandRegion == 0 )
			sprintf(buf, "%s$island", name);
		else
			sprintf(buf, "%s$island_%d", name, islandRegion);
	}
	else {
		sprintf(buf, "%s_plus_%d$island_%d", name, targetOffset, islandRegion);
	}
	fName = buf;
}


template <>
void BranchIslandAtom<ppc>::copyRawContent(uint8_t buffer[]) const
{
	int64_t displacement = fTarget.getAddress() + fTargetOffset - this->getAddress();
	int32_t branchInstruction = 0x48000000 | ((uint32_t)displacement & 0x03FFFFFC);
	OSWriteBigInt32(buffer, 0, branchInstruction);
}

template <>
void BranchIslandAtom<ppc64>::copyRawContent(uint8_t buffer[]) const
{
	int64_t displacement = fTarget.getAddress() + fTargetOffset - this->getAddress();
	int32_t branchInstruction = 0x48000000 | ((uint32_t)displacement & 0x03FFFFFC);
	OSWriteBigInt32(buffer, 0, branchInstruction);
}

template <>
uint64_t BranchIslandAtom<ppc>::getSize() const
{
	return 4;
}

template <>
uint64_t BranchIslandAtom<ppc64>::getSize() const
{
	return 4;
}



template <typename A>
uint64_t SegmentSplitInfoLoadCommandsAtom<A>::getSize() const
{
	if ( fWriter.fSplitCodeToDataContentAtom->canEncode() )
		return this->alignedSize(sizeof(macho_linkedit_data_command<P>));
	else
		return 0;	// a zero size causes the load command to be suppressed
}

template <typename A>
void SegmentSplitInfoLoadCommandsAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t size = this->getSize();
	if ( size > 0 ) {
		bzero(buffer, size);
		macho_linkedit_data_command<P>* cmd = (macho_linkedit_data_command<P>*)buffer;
		cmd->set_cmd(LC_SEGMENT_SPLIT_INFO);
		cmd->set_cmdsize(size);
		cmd->set_dataoff(fWriter.fSplitCodeToDataContentAtom->getFileOffset());
		cmd->set_datasize(fWriter.fSplitCodeToDataContentAtom->getSize());
	}
}


template <typename A>
uint64_t SegmentSplitInfoContentAtom<A>::getSize() const
{
	return fEncodedData.size();
}

template <typename A>
void SegmentSplitInfoContentAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	memcpy(buffer, &fEncodedData[0], fEncodedData.size());
}


template <typename A>
void SegmentSplitInfoContentAtom<A>::uleb128EncodeAddresses(const std::vector<SegmentSplitInfoContentAtom<A>::AtomAndOffset>& locations)
{
	pint_t addr = fWriter.fOptions.baseAddress();
	for(typename std::vector<AtomAndOffset>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
		pint_t nextAddr = it->atom->getAddress() + it->offset;
		//fprintf(stderr, "\t0x%0llX\n", (uint64_t)nextAddr);
		uint64_t delta = nextAddr - addr;
		if ( delta == 0 ) 
			throw "double split seg info for same address";
		// uleb128 encode
		uint8_t byte;
		do {
			byte = delta & 0x7F;
			delta &= ~0x7F;
			if ( delta != 0 )
				byte |= 0x80;
			fEncodedData.push_back(byte);
			delta = delta >> 7;
		} 
		while( byte >= 0x80 );
		addr = nextAddr;
	}
}

template <typename A>
void SegmentSplitInfoContentAtom<A>::encode()
{
	if ( ! fCantEncode ) {
		fEncodedData.reserve(8192);
		
		if ( fKind1Locations.size() != 0 ) {
			fEncodedData.push_back(1);
			//fprintf(stderr, "type 1:\n");
			this->uleb128EncodeAddresses(fKind1Locations);
			fEncodedData.push_back(0);
		}
		
		if ( fKind2Locations.size() != 0 ) {
			fEncodedData.push_back(2);
			//fprintf(stderr, "type 2:\n");
			this->uleb128EncodeAddresses(fKind2Locations);
			fEncodedData.push_back(0);
		}
		
		if ( fKind3Locations.size() != 0 ) {
			fEncodedData.push_back(3);
			//fprintf(stderr, "type 3:\n");
			this->uleb128EncodeAddresses(fKind3Locations);
			fEncodedData.push_back(0);
		}
		
		if ( fKind4Locations.size() != 0 ) {
			fEncodedData.push_back(4);
			//fprintf(stderr, "type 4:\n");
			this->uleb128EncodeAddresses(fKind4Locations);
			fEncodedData.push_back(0);
		}
		
		// always add zero byte to mark end
		fEncodedData.push_back(0);

		// add zeros to end to align size
		while ( (fEncodedData.size() % sizeof(pint_t)) != 0 )
			fEncodedData.push_back(0);
	}
}


template <typename A>
ObjCInfoAtom<A>::ObjCInfoAtom(Writer<A>& writer, ObjectFile::Reader::ObjcConstraint objcConstraint, bool objcReplacementClasses)
	: WriterAtom<A>(writer, getInfoSegment())
{
	fContent[0] = 0;
	uint32_t value = 0;
	//	struct objc_image_info  {
	//		uint32_t	version;	// initially 0
	//		uint32_t	flags;
	//	};
	// #define OBJC_IMAGE_SUPPORTS_GC   2
	// #define OBJC_IMAGE_GC_ONLY       4
	//
	if ( objcReplacementClasses ) 
		value = 1;
	switch ( objcConstraint ) {
		case ObjectFile::Reader::kObjcNone:
		case ObjectFile::Reader::kObjcRetainRelease:
			break;
		case ObjectFile::Reader::kObjcRetainReleaseOrGC:
			value |= 2;
			break;
		case ObjectFile::Reader::kObjcGC:
			value |= 6;
			break;
	}
	A::P::E::set32(fContent[1], value);
}

template <typename A>
void ObjCInfoAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	memcpy(buffer, &fContent[0], 8);
}


// objc info section is in a different segment and section for 32 vs 64 bit runtimes
template <> const char* ObjCInfoAtom<ppc>::getSectionName()    const { return "__image_info"; }
template <> const char* ObjCInfoAtom<x86>::getSectionName()    const { return "__image_info"; }
template <> const char* ObjCInfoAtom<arm>::getSectionName()    const { return "__objc_imageinfo"; }
template <> const char* ObjCInfoAtom<ppc64>::getSectionName()  const { return "__objc_imageinfo"; }
template <> const char* ObjCInfoAtom<x86_64>::getSectionName() const { return "__objc_imageinfo"; }

template <> Segment& ObjCInfoAtom<ppc>::getInfoSegment()    const { return Segment::fgObjCSegment; }
template <> Segment& ObjCInfoAtom<x86>::getInfoSegment()    const { return Segment::fgObjCSegment; }
template <> Segment& ObjCInfoAtom<ppc64>::getInfoSegment()  const { return Segment::fgDataSegment; }
template <> Segment& ObjCInfoAtom<x86_64>::getInfoSegment() const { return Segment::fgDataSegment; }
template <> Segment& ObjCInfoAtom<arm>::getInfoSegment()    const { return Segment::fgDataSegment; }




template <typename A>
void DyldInfoLoadCommandsAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	// build LC_DYLD_INFO command
	macho_dyld_info_command<P>*  cmd = (macho_dyld_info_command<P>*)buffer;
	bzero(cmd, sizeof(macho_dyld_info_command<P>));
	
	cmd->set_cmd( fWriter.fOptions.makeClassicDyldInfo() ? LC_DYLD_INFO : LC_DYLD_INFO_ONLY);
	cmd->set_cmdsize(sizeof(macho_dyld_info_command<P>));
	if ( (fWriter.fCompressedRebaseInfoAtom != NULL) && (fWriter.fCompressedRebaseInfoAtom->getSize() != 0) ) {
		cmd->set_rebase_off(fWriter.fCompressedRebaseInfoAtom->getFileOffset());
		cmd->set_rebase_size(fWriter.fCompressedRebaseInfoAtom->getSize());
	}
	if ( (fWriter.fCompressedBindingInfoAtom != NULL) && (fWriter.fCompressedBindingInfoAtom->getSize() != 0) ) {
		cmd->set_bind_off(fWriter.fCompressedBindingInfoAtom->getFileOffset());
		cmd->set_bind_size(fWriter.fCompressedBindingInfoAtom->getSize());
	}
	if ( (fWriter.fCompressedWeakBindingInfoAtom != NULL) && (fWriter.fCompressedWeakBindingInfoAtom->getSize() != 0) ) {
		cmd->set_weak_bind_off(fWriter.fCompressedWeakBindingInfoAtom->getFileOffset());
		cmd->set_weak_bind_size(fWriter.fCompressedWeakBindingInfoAtom->getSize());
	}
	if ( (fWriter.fCompressedLazyBindingInfoAtom != NULL) && (fWriter.fCompressedLazyBindingInfoAtom->getSize() != 0) ) {
		cmd->set_lazy_bind_off(fWriter.fCompressedLazyBindingInfoAtom->getFileOffset());
		cmd->set_lazy_bind_size(fWriter.fCompressedLazyBindingInfoAtom->getSize());
	}
	if ( (fWriter.fCompressedExportInfoAtom != NULL) && (fWriter.fCompressedExportInfoAtom->getSize() != 0) ) {
		cmd->set_export_off(fWriter.fCompressedExportInfoAtom->getFileOffset());
		cmd->set_export_size(fWriter.fCompressedExportInfoAtom->getSize());
	}
}


struct rebase_tmp
{
	rebase_tmp(uint8_t op, uint64_t p1, uint64_t p2=0) : opcode(op), operand1(p1), operand2(p2) {}
	uint8_t		opcode;
	uint64_t	operand1;
	uint64_t	operand2;
};


template <typename A>
void CompressedRebaseInfoLinkEditAtom<A>::encode()
{
	// sort rebase info by type, then address
	const std::vector<SegmentInfo*>& segments = fWriter.fSegmentInfos;
	std::vector<RebaseInfo>& info = fWriter.fRebaseInfo;
	std::sort(info.begin(), info.end());
	
	// convert to temp encoding that can be more easily optimized
	std::vector<rebase_tmp> mid;
	const SegmentInfo* currentSegment = NULL;
	unsigned int segIndex = 0;
	uint8_t type = 0;
	uint64_t address = (uint64_t)(-1);
	for (std::vector<RebaseInfo>::iterator it = info.begin(); it != info.end(); ++it) {
		if ( type != it->fType ) {
			mid.push_back(rebase_tmp(REBASE_OPCODE_SET_TYPE_IMM, it->fType));
			type = it->fType;
		}
		if ( address != it->fAddress ) {
			if ( (currentSegment == NULL) || (it->fAddress < currentSegment->fBaseAddress) 
					|| ((currentSegment->fBaseAddress+currentSegment->fSize) <= it->fAddress) ) {
				segIndex = 0;
				for (std::vector<SegmentInfo*>::const_iterator segit = segments.begin(); segit != segments.end(); ++segit) {
					if ( ((*segit)->fBaseAddress <= it->fAddress) && (it->fAddress < ((*segit)->fBaseAddress+(*segit)->fSize)) ) {
						currentSegment = *segit;
						break;
					}
					++segIndex;
				}
				mid.push_back(rebase_tmp(REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB, segIndex, it->fAddress - currentSegment->fBaseAddress));
			}
			else {
				mid.push_back(rebase_tmp(REBASE_OPCODE_ADD_ADDR_ULEB, it->fAddress-address));
			}
			address = it->fAddress;
		}
		mid.push_back(rebase_tmp(REBASE_OPCODE_DO_REBASE_ULEB_TIMES, 1));
		address += sizeof(pint_t);
	}
	mid.push_back(rebase_tmp(REBASE_OPCODE_DONE, 0));

	// optimize phase 1, compress packed runs of pointers
	rebase_tmp* dst = &mid[0];
	for (const rebase_tmp* src = &mid[0]; src->opcode != REBASE_OPCODE_DONE; ++src) {
		if ( (src->opcode == REBASE_OPCODE_DO_REBASE_ULEB_TIMES) && (src->operand1 == 1) ) {
			*dst = *src++;
			while (src->opcode == REBASE_OPCODE_DO_REBASE_ULEB_TIMES ) {
				dst->operand1 += src->operand1;
				++src;
			}
			--src;
			++dst;
		}
		else {
			*dst++ = *src;
		}
	}
	dst->opcode = REBASE_OPCODE_DONE;

	// optimize phase 2, combine rebase/add pairs
	dst = &mid[0];
	for (const rebase_tmp* src = &mid[0]; src->opcode != REBASE_OPCODE_DONE; ++src) {
		if ( (src->opcode == REBASE_OPCODE_DO_REBASE_ULEB_TIMES) 
				&& (src->operand1 == 1) 
				&& (src[1].opcode == REBASE_OPCODE_ADD_ADDR_ULEB)) {
			dst->opcode = REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB;
			dst->operand1 = src[1].operand1;
			++src;
			++dst;
		}
		else {
			*dst++ = *src;
		}
	}
	dst->opcode = REBASE_OPCODE_DONE;
	
	// optimize phase 3, compress packed runs of REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB with
	// same addr delta into one REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB
	dst = &mid[0];
	for (const rebase_tmp* src = &mid[0]; src->opcode != REBASE_OPCODE_DONE; ++src) {
		uint64_t delta = src->operand1;
		if ( (src->opcode == REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB) 
				&& (src[1].opcode == REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB) 
				&& (src[2].opcode == REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB) 
				&& (src[1].operand1 == delta) 
				&& (src[2].operand1 == delta) ) {
			// found at least three in a row, this is worth compressing
			dst->opcode = REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB;
			dst->operand1 = 1;
			dst->operand2 = delta;
			++src;
			while ( (src->opcode == REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB)
					&& (src->operand1 == delta) ) {
				dst->operand1++;
				++src;
			}
			--src;
			++dst;
		}
		else {
			*dst++ = *src;
		}
	}
	dst->opcode = REBASE_OPCODE_DONE;
	
	// optimize phase 4, use immediate encodings
	for (rebase_tmp* p = &mid[0]; p->opcode != REBASE_OPCODE_DONE; ++p) {
		if ( (p->opcode == REBASE_OPCODE_ADD_ADDR_ULEB) 
			&& (p->operand1 < (15*sizeof(pint_t)))
			&& ((p->operand1 % sizeof(pint_t)) == 0) ) {
			p->opcode = REBASE_OPCODE_ADD_ADDR_IMM_SCALED;
			p->operand1 = p->operand1/sizeof(pint_t);
		}
		else if ( (p->opcode == REBASE_OPCODE_DO_REBASE_ULEB_TIMES) && (p->operand1 < 15) ) {
			p->opcode = REBASE_OPCODE_DO_REBASE_IMM_TIMES;
		}
	}

	// convert to compressed encoding
	const static bool log = false;
	fEncodedData.reserve(info.size()*2);
	bool done = false;
	for (std::vector<rebase_tmp>::iterator it = mid.begin(); !done && it != mid.end() ; ++it) {
		switch ( it->opcode ) {
			case REBASE_OPCODE_DONE:
				if ( log ) fprintf(stderr, "REBASE_OPCODE_DONE()\n");
				done = true;
				break;
			case REBASE_OPCODE_SET_TYPE_IMM:
				if ( log ) fprintf(stderr, "REBASE_OPCODE_SET_TYPE_IMM(%lld)\n", it->operand1);
				fEncodedData.append_byte(REBASE_OPCODE_SET_TYPE_IMM | it->operand1);
				break;
			case REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
				if ( log ) fprintf(stderr, "REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB(%lld, 0x%llX)\n", it->operand1, it->operand2);
				fEncodedData.append_byte(REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB | it->operand1);
				fEncodedData.append_uleb128(it->operand2);
				break;
			case REBASE_OPCODE_ADD_ADDR_ULEB:
				if ( log ) fprintf(stderr, "REBASE_OPCODE_ADD_ADDR_ULEB(0x%llX)\n", it->operand1);
				fEncodedData.append_byte(REBASE_OPCODE_ADD_ADDR_ULEB);
				fEncodedData.append_uleb128(it->operand1);
				break;
			case REBASE_OPCODE_ADD_ADDR_IMM_SCALED:
				if ( log ) fprintf(stderr, "REBASE_OPCODE_ADD_ADDR_IMM_SCALED(%lld=0x%llX)\n", it->operand1, it->operand1*sizeof(pint_t));
				fEncodedData.append_byte(REBASE_OPCODE_ADD_ADDR_IMM_SCALED | it->operand1 );
				break;
			case REBASE_OPCODE_DO_REBASE_IMM_TIMES:
				if ( log ) fprintf(stderr, "REBASE_OPCODE_DO_REBASE_IMM_TIMES(%lld)\n", it->operand1);
				fEncodedData.append_byte(REBASE_OPCODE_DO_REBASE_IMM_TIMES | it->operand1);
				break;
			case REBASE_OPCODE_DO_REBASE_ULEB_TIMES:
				if ( log ) fprintf(stderr, "REBASE_OPCODE_DO_REBASE_ULEB_TIMES(%lld)\n", it->operand1);
				fEncodedData.append_byte(REBASE_OPCODE_DO_REBASE_ULEB_TIMES);
				fEncodedData.append_uleb128(it->operand1);
				break;
			case REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB:
				if ( log ) fprintf(stderr, "REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB(0x%llX)\n", it->operand1);
				fEncodedData.append_byte(REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB);
				fEncodedData.append_uleb128(it->operand1);
				break;
			case REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB:
				if ( log ) fprintf(stderr, "REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB(%lld, %lld)\n", it->operand1, it->operand2);
				fEncodedData.append_byte(REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB);
				fEncodedData.append_uleb128(it->operand1);
				fEncodedData.append_uleb128(it->operand2);
				break;
		}
	}
	
		
	// align to pointer size
	fEncodedData.pad_to_size(sizeof(pint_t));

	if (log) fprintf(stderr, "total rebase info size = %ld\n", fEncodedData.size());
}


struct binding_tmp
{
	binding_tmp(uint8_t op, uint64_t p1, uint64_t p2=0, const char* s=NULL) 
		: opcode(op), operand1(p1), operand2(p2), name(s) {}
	uint8_t		opcode;
	uint64_t	operand1;
	uint64_t	operand2;
	const char*	name;
};



template <typename A>
void CompressedBindingInfoLinkEditAtom<A>::encode()
{
	// sort by library, symbol, type, then address
	const std::vector<SegmentInfo*>& segments = fWriter.fSegmentInfos;
	std::vector<BindingInfo>& info = fWriter.fBindingInfo;
	std::sort(info.begin(), info.end());
	
	// convert to temp encoding that can be more easily optimized
	std::vector<binding_tmp> mid;
	const SegmentInfo* currentSegment = NULL;
	unsigned int segIndex = 0;
	int ordinal = 0x80000000;
	const char* symbolName = NULL;
	uint8_t type = 0;
	uint64_t address = (uint64_t)(-1);
	int64_t addend = 0;
	for (std::vector<BindingInfo>::iterator it = info.begin(); it != info.end(); ++it) {
		if ( ordinal != it->fLibraryOrdinal ) {
			if ( it->fLibraryOrdinal <= 0 ) {
				// special lookups are encoded as negative numbers in BindingInfo
				mid.push_back(binding_tmp(BIND_OPCODE_SET_DYLIB_SPECIAL_IMM, it->fLibraryOrdinal));
			}
			else {
				mid.push_back(binding_tmp(BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB, it->fLibraryOrdinal));
			}
			ordinal = it->fLibraryOrdinal;
		}
		if ( symbolName != it->fSymbolName ) {
			mid.push_back(binding_tmp(BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM, it->fFlags, 0, it->fSymbolName));
			symbolName = it->fSymbolName;
		}
		if ( type != it->fType ) {
			mid.push_back(binding_tmp(BIND_OPCODE_SET_TYPE_IMM, it->fType));
			type = it->fType;
		}
		if ( address != it->fAddress ) {
			if ( (currentSegment == NULL) || (it->fAddress < currentSegment->fBaseAddress) 
					|| ((currentSegment->fBaseAddress+currentSegment->fSize) <=it->fAddress) 
					|| (it->fAddress < address) ) {
				segIndex = 0;
				for (std::vector<SegmentInfo*>::const_iterator segit = segments.begin(); segit != segments.end(); ++segit) {
					if ( ((*segit)->fBaseAddress <= it->fAddress) && (it->fAddress < ((*segit)->fBaseAddress+(*segit)->fSize)) ) {
						currentSegment = *segit;
						break;
					}
					++segIndex;
				}
				mid.push_back(binding_tmp(BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB, segIndex, it->fAddress - currentSegment->fBaseAddress));
			}
			else {
				mid.push_back(binding_tmp(BIND_OPCODE_ADD_ADDR_ULEB, it->fAddress-address));
			}
			address = it->fAddress;
		}
		if ( addend != it->fAddend ) {
			mid.push_back(binding_tmp(BIND_OPCODE_SET_ADDEND_SLEB, it->fAddend));
			addend = it->fAddend;
		}
		mid.push_back(binding_tmp(BIND_OPCODE_DO_BIND, 0));
		address += sizeof(pint_t);
	}
	mid.push_back(binding_tmp(BIND_OPCODE_DONE, 0));


	// optimize phase 1, combine bind/add pairs
	binding_tmp* dst = &mid[0];
	for (const binding_tmp* src = &mid[0]; src->opcode != BIND_OPCODE_DONE; ++src) {
		if ( (src->opcode == BIND_OPCODE_DO_BIND) 
				&& (src[1].opcode == BIND_OPCODE_ADD_ADDR_ULEB) ) {
			dst->opcode = BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB;
			dst->operand1 = src[1].operand1;
			++src;
			++dst;
		}
		else {
			*dst++ = *src;
		}
	}
	dst->opcode = BIND_OPCODE_DONE;

	// optimize phase 2, compress packed runs of BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB with
	// same addr delta into one BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB
	dst = &mid[0];
	for (const binding_tmp* src = &mid[0]; src->opcode != BIND_OPCODE_DONE; ++src) {
		uint64_t delta = src->operand1;
		if ( (src->opcode == BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB) 
				&& (src[1].opcode == BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB) 
				&& (src[1].operand1 == delta) ) {
			// found at least two in a row, this is worth compressing
			dst->opcode = BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB;
			dst->operand1 = 1;
			dst->operand2 = delta;
			++src;
			while ( (src->opcode == BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB)
					&& (src->operand1 == delta) ) {
				dst->operand1++;
				++src;
			}
			--src;
			++dst;
		}
		else {
			*dst++ = *src;
		}
	}
	dst->opcode = BIND_OPCODE_DONE;
	
	// optimize phase 3, use immediate encodings
	for (binding_tmp* p = &mid[0]; p->opcode != REBASE_OPCODE_DONE; ++p) {
		if ( (p->opcode == BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB) 
			&& (p->operand1 < (15*sizeof(pint_t)))
			&& ((p->operand1 % sizeof(pint_t)) == 0) ) {
			p->opcode = BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED;
			p->operand1 = p->operand1/sizeof(pint_t);
		}
		else if ( (p->opcode == BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB) && (p->operand1 <= 15) ) {
			p->opcode = BIND_OPCODE_SET_DYLIB_ORDINAL_IMM;
		}
	}	
	dst->opcode = BIND_OPCODE_DONE;

	// convert to compressed encoding
	const static bool log = false;
	fEncodedData.reserve(info.size()*2);
	bool done = false;
	for (std::vector<binding_tmp>::iterator it = mid.begin(); !done && it != mid.end() ; ++it) {
		switch ( it->opcode ) {
			case BIND_OPCODE_DONE:
				if ( log ) fprintf(stderr, "BIND_OPCODE_DONE()\n");
				done = true;
				break;
			case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
				if ( log ) fprintf(stderr, "BIND_OPCODE_SET_DYLIB_ORDINAL_IMM(%lld)\n", it->operand1);
				fEncodedData.append_byte(BIND_OPCODE_SET_DYLIB_ORDINAL_IMM | it->operand1);
				break;
			case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
				if ( log ) fprintf(stderr, "BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB(%lld)\n", it->operand1);
				fEncodedData.append_byte(BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB);
				fEncodedData.append_uleb128(it->operand1);
				break;
			case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
				if ( log ) fprintf(stderr, "BIND_OPCODE_SET_DYLIB_SPECIAL_IMM(%lld)\n", it->operand1);
				fEncodedData.append_byte(BIND_OPCODE_SET_DYLIB_SPECIAL_IMM | (it->operand1 & BIND_IMMEDIATE_MASK));
				break;
			case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
				if ( log ) fprintf(stderr, "BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM(0x%0llX, %s)\n", it->operand1, it->name);
				fEncodedData.append_byte(BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM | it->operand1);
				fEncodedData.append_string(it->name);
				break;
			case BIND_OPCODE_SET_TYPE_IMM:
				if ( log ) fprintf(stderr, "BIND_OPCODE_SET_TYPE_IMM(%lld)\n", it->operand1);
				fEncodedData.append_byte(BIND_OPCODE_SET_TYPE_IMM | it->operand1);
				break;
			case BIND_OPCODE_SET_ADDEND_SLEB:
				if ( log ) fprintf(stderr, "BIND_OPCODE_SET_ADDEND_SLEB(%lld)\n", it->operand1);
				fEncodedData.append_byte(BIND_OPCODE_SET_ADDEND_SLEB);
				fEncodedData.append_sleb128(it->operand1);
				break;
			case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
				if ( log ) fprintf(stderr, "BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB(%lld, 0x%llX)\n", it->operand1, it->operand2);
				fEncodedData.append_byte(BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB | it->operand1);
				fEncodedData.append_uleb128(it->operand2);
				break;
			case BIND_OPCODE_ADD_ADDR_ULEB:
				if ( log ) fprintf(stderr, "BIND_OPCODE_ADD_ADDR_ULEB(0x%llX)\n", it->operand1);
				fEncodedData.append_byte(BIND_OPCODE_ADD_ADDR_ULEB);
				fEncodedData.append_uleb128(it->operand1);
				break;
			case BIND_OPCODE_DO_BIND:
				if ( log ) fprintf(stderr, "BIND_OPCODE_DO_BIND()\n");
				fEncodedData.append_byte(BIND_OPCODE_DO_BIND);
				break;
			case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
				if ( log ) fprintf(stderr, "BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB(0x%llX)\n", it->operand1);
				fEncodedData.append_byte(BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB);
				fEncodedData.append_uleb128(it->operand1);
				break;
			case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
				if ( log ) fprintf(stderr, "BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED(%lld=0x%llX)\n", it->operand1, it->operand1*sizeof(pint_t));
				fEncodedData.append_byte(BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED | it->operand1 );
				break;
			case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
				if ( log ) fprintf(stderr, "BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB(%lld, %lld)\n", it->operand1, it->operand2);
				fEncodedData.append_byte(BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB);
				fEncodedData.append_uleb128(it->operand1);
				fEncodedData.append_uleb128(it->operand2);
				break;
		}
	}
	
	// align to pointer size
	fEncodedData.pad_to_size(sizeof(pint_t));

	if (log) fprintf(stderr, "total binding info size = %ld\n", fEncodedData.size());

}



struct WeakBindingSorter
{	
     bool operator()(const BindingInfo& left, const BindingInfo& right)
     {
		// sort by symbol, type, address
		if ( left.fSymbolName != right.fSymbolName )
			return ( strcmp(left.fSymbolName, right.fSymbolName) < 0 );
		if ( left.fType != right.fType )
			return  (left.fType < right.fType);
		return  (left.fAddress < right.fAddress);
     }
};



template <typename A>
void CompressedWeakBindingInfoLinkEditAtom<A>::encode()
{
	// add regular atoms that override a dylib's weak definitions 
	for(std::set<const class ObjectFile::Atom*>::iterator it = fWriter.fRegularDefAtomsThatOverrideADylibsWeakDef->begin();
													it != fWriter.fRegularDefAtomsThatOverrideADylibsWeakDef->end(); ++it) {
		if ( fWriter.shouldExport(**it) )
			fWriter.fWeakBindingInfo.push_back(BindingInfo(0, (*it)->getName(), true, 0, 0));
	}
	
	// add all exported weak definitions
	for(std::vector<class ObjectFile::Atom*>::iterator it = fWriter.fAllAtoms->begin(); it != fWriter.fAllAtoms->end(); ++it) {
		ObjectFile::Atom* atom = *it;
		if ( (atom->getDefinitionKind() == ObjectFile::Atom::kWeakDefinition) && fWriter.shouldExport(*atom) ) {
			fWriter.fWeakBindingInfo.push_back(BindingInfo(0, atom->getName(), false, 0, 0));
		}
	}	
	
	// sort by symbol, type, address
	const std::vector<SegmentInfo*>& segments = fWriter.fSegmentInfos;
	std::vector<BindingInfo>& info = fWriter.fWeakBindingInfo;
	if ( info.size() == 0 )
		return;
	std::sort(info.begin(), info.end(), WeakBindingSorter());
	
	// convert to temp encoding that can be more easily optimized
	std::vector<binding_tmp> mid;
	mid.reserve(info.size());
	const SegmentInfo* currentSegment = NULL;
	unsigned int segIndex = 0;
	const char* symbolName = NULL;
	uint8_t type = 0;
	uint64_t address = (uint64_t)(-1);
	int64_t addend = 0;
	for (std::vector<BindingInfo>::iterator it = info.begin(); it != info.end(); ++it) {
		if ( symbolName != it->fSymbolName ) {
			mid.push_back(binding_tmp(BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM, it->fFlags, 0, it->fSymbolName));
			symbolName = it->fSymbolName;
		}
		if ( it->fType != 0 ) {
			if ( type != it->fType ) {
				mid.push_back(binding_tmp(BIND_OPCODE_SET_TYPE_IMM, it->fType));
				type = it->fType;
			}
			if ( address != it->fAddress ) {
				// non weak symbols just have BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM
				// weak symbols have SET_SEG, ADD_ADDR, SET_ADDED, DO_BIND
				if ( (currentSegment == NULL) || (it->fAddress < currentSegment->fBaseAddress) 
						|| ((currentSegment->fBaseAddress+currentSegment->fSize) <=it->fAddress) ) {
					segIndex = 0;
					for (std::vector<SegmentInfo*>::const_iterator segit = segments.begin(); segit != segments.end(); ++segit) {
						if ( ((*segit)->fBaseAddress <= it->fAddress) && (it->fAddress < ((*segit)->fBaseAddress+(*segit)->fSize)) ) {
							currentSegment = *segit;
							break;
						}
						++segIndex;
					}
					mid.push_back(binding_tmp(BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB, segIndex, it->fAddress - currentSegment->fBaseAddress));
				}
				else {
					mid.push_back(binding_tmp(BIND_OPCODE_ADD_ADDR_ULEB, it->fAddress-address));
				}
				address = it->fAddress;
			}
			if ( addend != it->fAddend ) {
				mid.push_back(binding_tmp(BIND_OPCODE_SET_ADDEND_SLEB, it->fAddend));
				addend = it->fAddend;
			}
			mid.push_back(binding_tmp(BIND_OPCODE_DO_BIND, 0));
			address += sizeof(pint_t);
		}
	}
	mid.push_back(binding_tmp(BIND_OPCODE_DONE, 0));


	// optimize phase 1, combine bind/add pairs
	binding_tmp* dst = &mid[0];
	for (const binding_tmp* src = &mid[0]; src->opcode != BIND_OPCODE_DONE; ++src) {
		if ( (src->opcode == BIND_OPCODE_DO_BIND) 
				&& (src[1].opcode == BIND_OPCODE_ADD_ADDR_ULEB) ) {
			dst->opcode = BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB;
			dst->operand1 = src[1].operand1;
			++src;
			++dst;
		}
		else {
			*dst++ = *src;
		}
	}
	dst->opcode = BIND_OPCODE_DONE;

	// optimize phase 2, compress packed runs of BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB with
	// same addr delta into one BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB
	dst = &mid[0];
	for (const binding_tmp* src = &mid[0]; src->opcode != BIND_OPCODE_DONE; ++src) {
		uint64_t delta = src->operand1;
		if ( (src->opcode == BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB) 
				&& (src[1].opcode == BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB) 
				&& (src[1].operand1 == delta) ) {
			// found at least two in a row, this is worth compressing
			dst->opcode = BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB;
			dst->operand1 = 1;
			dst->operand2 = delta;
			++src;
			while ( (src->opcode == BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB)
					&& (src->operand1 == delta) ) {
				dst->operand1++;
				++src;
			}
			--src;
			++dst;
		}
		else {
			*dst++ = *src;
		}
	}
	dst->opcode = BIND_OPCODE_DONE;
	
	// optimize phase 3, use immediate encodings
	for (binding_tmp* p = &mid[0]; p->opcode != REBASE_OPCODE_DONE; ++p) {
		if ( (p->opcode == BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB) 
			&& (p->operand1 < (15*sizeof(pint_t)))
			&& ((p->operand1 % sizeof(pint_t)) == 0) ) {
			p->opcode = BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED;
			p->operand1 = p->operand1/sizeof(pint_t);
		}
	}	
	dst->opcode = BIND_OPCODE_DONE;


	// convert to compressed encoding
	const static bool log = false;
	fEncodedData.reserve(info.size()*2);
	bool done = false;
	for (std::vector<binding_tmp>::iterator it = mid.begin(); !done && it != mid.end() ; ++it) {
		switch ( it->opcode ) {
			case BIND_OPCODE_DONE:
				if ( log ) fprintf(stderr, "BIND_OPCODE_DONE()\n");
				fEncodedData.append_byte(BIND_OPCODE_DONE);
				done = true;
				break;
			case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
				if ( log ) fprintf(stderr, "BIND_OPCODE_SET_DYLIB_ORDINAL_IMM(%lld)\n", it->operand1);
				fEncodedData.append_byte(BIND_OPCODE_SET_DYLIB_ORDINAL_IMM | it->operand1);
				break;
			case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
				if ( log ) fprintf(stderr, "BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB(%lld)\n", it->operand1);
				fEncodedData.append_byte(BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB);
				fEncodedData.append_uleb128(it->operand1);
				break;
			case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
				if ( log ) fprintf(stderr, "BIND_OPCODE_SET_DYLIB_SPECIAL_IMM(%lld)\n", it->operand1);
				fEncodedData.append_byte(BIND_OPCODE_SET_DYLIB_SPECIAL_IMM | (it->operand1 & BIND_IMMEDIATE_MASK));
				break;
			case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
				if ( log ) fprintf(stderr, "BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM(0x%0llX, %s)\n", it->operand1, it->name);
				fEncodedData.append_byte(BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM | it->operand1);
				fEncodedData.append_string(it->name);
				break;
			case BIND_OPCODE_SET_TYPE_IMM:
				if ( log ) fprintf(stderr, "BIND_OPCODE_SET_TYPE_IMM(%lld)\n", it->operand1);
				fEncodedData.append_byte(BIND_OPCODE_SET_TYPE_IMM | it->operand1);
				break;
			case BIND_OPCODE_SET_ADDEND_SLEB:
				if ( log ) fprintf(stderr, "BIND_OPCODE_SET_ADDEND_SLEB(%lld)\n", it->operand1);
				fEncodedData.append_byte(BIND_OPCODE_SET_ADDEND_SLEB);
				fEncodedData.append_sleb128(it->operand1);
				break;
			case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
				if ( log ) fprintf(stderr, "BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB(%lld, 0x%llX)\n", it->operand1, it->operand2);
				fEncodedData.append_byte(BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB | it->operand1);
				fEncodedData.append_uleb128(it->operand2);
				break;
			case BIND_OPCODE_ADD_ADDR_ULEB:
				if ( log ) fprintf(stderr, "BIND_OPCODE_ADD_ADDR_ULEB(0x%llX)\n", it->operand1);
				fEncodedData.append_byte(BIND_OPCODE_ADD_ADDR_ULEB);
				fEncodedData.append_uleb128(it->operand1);
				break;
			case BIND_OPCODE_DO_BIND:
				if ( log ) fprintf(stderr, "BIND_OPCODE_DO_BIND()\n");
				fEncodedData.append_byte(BIND_OPCODE_DO_BIND);
				break;
			case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
				if ( log ) fprintf(stderr, "BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB(0x%llX)\n", it->operand1);
				fEncodedData.append_byte(BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB);
				fEncodedData.append_uleb128(it->operand1);
				break;
			case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
				if ( log ) fprintf(stderr, "BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED(%lld=0x%llX)\n", it->operand1, it->operand1*sizeof(pint_t));
				fEncodedData.append_byte(BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED | it->operand1 );
				break;
			case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
				if ( log ) fprintf(stderr, "BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB(%lld, %lld)\n", it->operand1, it->operand2);
				fEncodedData.append_byte(BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB);
				fEncodedData.append_uleb128(it->operand1);
				fEncodedData.append_uleb128(it->operand2);
				break;
		}
	}
	
	// align to pointer size
	fEncodedData.pad_to_size(sizeof(pint_t));

	if (log) fprintf(stderr, "total weak binding info size = %ld\n", fEncodedData.size());

}

template <typename A>
void CompressedLazyBindingInfoLinkEditAtom<A>::encode()
{
	// stream all lazy bindings and record start offsets
	const SegmentInfo* currentSegment = NULL;
	uint8_t segIndex = 0;
	const std::vector<SegmentInfo*>& segments = fWriter.fSegmentInfos;
	std::vector<class LazyPointerAtom<A>*>& allLazys = fWriter.fAllSynthesizedLazyPointers;
	for (typename std::vector<class LazyPointerAtom<A>*>::iterator it = allLazys.begin(); it != allLazys.end(); ++it) {
		LazyPointerAtom<A>* lazyPointerAtom = *it;
		ObjectFile::Atom* lazyPointerTargetAtom = lazyPointerAtom->getTarget();
		
		// skip lazy pointers that are bound non-lazily because they are coalesced
		if ( ! fWriter.targetRequiresWeakBinding(*lazyPointerTargetAtom) ) {			
			// record start offset for use by stub helper
			lazyPointerAtom->setLazyBindingInfoOffset(fEncodedData.size());

			// write address to bind
			pint_t address = lazyPointerAtom->getAddress();
			if ( (currentSegment == NULL) || (address < currentSegment->fBaseAddress) 
					|| ((currentSegment->fBaseAddress+currentSegment->fSize) <= address) ) {
				segIndex = 0;
				for (std::vector<SegmentInfo*>::const_iterator segit = segments.begin(); segit != segments.end(); ++segit) {
					if ( ((*segit)->fBaseAddress <= address) && (address < ((*segit)->fBaseAddress+(*segit)->fSize)) ) {
						currentSegment = *segit;
						break;
					}
					++segIndex;
				}
			}
			fEncodedData.append_byte(BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB | segIndex);
			fEncodedData.append_uleb128(lazyPointerAtom->getAddress() - currentSegment->fBaseAddress);
			
			// write ordinal
			int ordinal = fWriter.compressedOrdinalForImortedAtom(lazyPointerTargetAtom);
			if ( ordinal <= 0 ) {
				// special lookups are encoded as negative numbers in BindingInfo
				fEncodedData.append_byte(BIND_OPCODE_SET_DYLIB_SPECIAL_IMM | (ordinal & BIND_IMMEDIATE_MASK) );
			}
			else if ( ordinal <= 15 ) {
				// small ordinals are encoded in opcode
				fEncodedData.append_byte(BIND_OPCODE_SET_DYLIB_ORDINAL_IMM | ordinal);
			}
			else {
				fEncodedData.append_byte(BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB);
				fEncodedData.append_uleb128(ordinal);
			}
			// write symbol name
			bool weak_import = fWriter.fWeakImportMap[lazyPointerTargetAtom];
			if ( weak_import )
				fEncodedData.append_byte(BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM | BIND_SYMBOL_FLAGS_WEAK_IMPORT);
			else
				fEncodedData.append_byte(BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM);
			fEncodedData.append_string(lazyPointerTargetAtom->getName());
			// write do bind
			fEncodedData.append_byte(BIND_OPCODE_DO_BIND);
			fEncodedData.append_byte(BIND_OPCODE_DONE);
		}
	}
	// align to pointer size
	fEncodedData.pad_to_size(sizeof(pint_t));
	
	//fprintf(stderr, "lazy binding info size = %ld, for %ld entries\n", fEncodedData.size(), allLazys.size());
}
	
struct TrieEntriesSorter
{
	TrieEntriesSorter(Options& o) : fOptions(o) {}
	
     bool operator()(const mach_o::trie::Entry& left, const mach_o::trie::Entry& right)
     {
		unsigned int leftOrder;
		unsigned int rightOrder;
		fOptions.exportedSymbolOrder(left.name, &leftOrder);
		fOptions.exportedSymbolOrder(right.name, &rightOrder);
		if ( leftOrder != rightOrder ) 
			return (leftOrder < rightOrder);
		else
			return (left.address < right.address);
     }
private:
	Options&	fOptions;
};


template <typename A>
void CompressedExportInfoLinkEditAtom<A>::encode()
{
	// make vector of mach_o::trie::Entry for all exported symbols
	std::vector<class ObjectFile::Atom*>& exports = fWriter.fExportedAtoms;
	uint64_t imageBaseAddress = fWriter.fMachHeaderAtom->getAddress();
	std::vector<mach_o::trie::Entry> entries;
	entries.reserve(exports.size());
	for (std::vector<ObjectFile::Atom*>::iterator it = exports.begin(); it != exports.end(); ++it) {
		ObjectFile::Atom* atom = *it;
		uint64_t flags = 0;
		if ( atom->getDefinitionKind() == ObjectFile::Atom::kWeakDefinition )
			flags |= EXPORT_SYMBOL_FLAGS_WEAK_DEFINITION;
		uint64_t address = atom->getAddress() - imageBaseAddress;
		if ( atom->isThumb() )
			address |= 1;
		mach_o::trie::Entry entry;
		entry.name = atom->getName();
		entry.flags = flags;
		entry.address = address; 
		entries.push_back(entry);
	}

	// sort vector by -exported_symbols_order, and any others by address
	std::sort(entries.begin(), entries.end(), TrieEntriesSorter(fWriter.fOptions));
	
	// create trie
	mach_o::trie::makeTrie(entries, fEncodedData.bytes());

	// align to pointer size
	fEncodedData.pad_to_size(sizeof(pint_t));
}





}; // namespace executable
}; // namespace mach_o


#endif // __EXECUTABLE_MACH_O__
