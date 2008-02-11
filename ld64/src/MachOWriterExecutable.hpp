/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
 *
 * Copyright (c) 2005-2007 Apple Inc. All rights reserved.
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
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/reloc.h>
//#include <mach-o/ppc/reloc.h>
#include <mach-o/stab.h>
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

#ifndef S_DTRACE_DOF
  #define S_DTRACE_DOF 0xF
#endif

#ifndef MH_PIE
	#define MH_PIE 0x200000 
#endif

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
template <typename A> class SymbolTableLoadCommandsAtom;
template <typename A> class ThreadsLoadCommandsAtom;
template <typename A> class DylibIDLoadCommandsAtom;
template <typename A> class RoutinesLoadCommandsAtom;
template <typename A> class DyldLoadCommandsAtom;
template <typename A> class UUIDLoadCommandAtom;
template <typename A> class LinkEditAtom;
template <typename A> class SectionRelocationsLinkEditAtom;
template <typename A> class LocalRelocationsLinkEditAtom;
template <typename A> class ExternalRelocationsLinkEditAtom;
template <typename A> class SymbolTableLinkEditAtom;
template <typename A> class SegmentSplitInfoLoadCommandsAtom;
template <typename A> class SegmentSplitInfoContentAtom;
template <typename A> class IndirectTableLinkEditAtom;
template <typename A> class ModuleInfoLinkEditAtom;
template <typename A> class StringsLinkEditAtom;
template <typename A> class LoadCommandsPaddingAtom;
template <typename A> class StubAtom;
template <typename A> class StubHelperAtom;
template <typename A> class LazyPointerAtom;
template <typename A> class NonLazyPointerAtom;
template <typename A> class DylibLoadCommandsAtom;


// SectionInfo should be nested inside Writer, but I can't figure out how to make the type accessible to the Atom classes
class SectionInfo : public ObjectFile::Section {
public:
										SectionInfo() : fFileOffset(0), fSize(0), fRelocCount(0), fRelocOffset(0), fIndirectSymbolOffset(0),
														fAlignment(0), fAllLazyPointers(false), fAllNonLazyPointers(false), fAllStubs(false),
														fAllSelfModifyingStubs(false), fAllZeroFill(false), fVirtualSection(false)
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
	bool								fAllNonLazyPointers;
	bool								fAllStubs;
	bool								fAllSelfModifyingStubs;
	bool								fAllZeroFill;
	bool								fVirtualSection;
};

// SegmentInfo should be nested inside Writer, but I can't figure out how to make the type accessible to the Atom classes
class SegmentInfo
{
public:
										SegmentInfo() : fInitProtection(0), fMaxProtection(0), fFileOffset(0), fFileSize(0),
														fBaseAddress(0), fSize(0), fFixedAddress(false), 
														fIndependentAddress(false) { fName[0] = '\0'; }
	std::vector<class SectionInfo*>		fSections;
	char								fName[20];
	uint32_t							fInitProtection;
	uint32_t							fMaxProtection;
	uint64_t							fFileOffset;
	uint64_t							fFileSize;
	uint64_t							fBaseAddress;
	uint64_t							fSize;
	bool								fFixedAddress;
	bool								fIndependentAddress;
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
														  class ObjectFile::Atom* dyldHelperAtom,
														  bool createUUID, bool canScatter,
														  ObjectFile::Reader::CpuConstraint cpuConstraint,
														  bool biggerThanTwoGigs);

private:
	typedef typename A::P			P;
	typedef typename A::P::uint_t	pint_t;

	enum RelocKind { kRelocNone, kRelocInternal, kRelocExternal };

	void						assignFileOffsets();
	void						synthesizeStubs();
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
	void						writeNoOps(uint32_t from, uint32_t to);
	void						copyNoOps(uint8_t* from, uint8_t* to);
	bool						segmentsCanSplitApart(const ObjectFile::Atom& from, const ObjectFile::Atom& to);
	void						addCrossSegmentRef(const ObjectFile::Atom* atom, const ObjectFile::Reference* ref);
	void						collectExportedAndImportedAndLocalAtoms();
	void						setNlistRange(std::vector<class ObjectFile::Atom*>& atoms, uint32_t startIndex, uint32_t count);
	void						addLocalLabel(ObjectFile::Atom& atom, uint32_t offsetInAtom, const char* name);
	void						addGlobalLabel(ObjectFile::Atom& atom, uint32_t offsetInAtom, const char* name);
	void						buildSymbolTable();
	const char*					symbolTableName(const ObjectFile::Atom* atom);
	void						setExportNlist(const ObjectFile::Atom* atom, macho_nlist<P>* entry);
	void						setImportNlist(const ObjectFile::Atom* atom, macho_nlist<P>* entry);
	void						setLocalNlist(const ObjectFile::Atom* atom, macho_nlist<P>* entry);
	void						copyNlistRange(const std::vector<macho_nlist<P> >& entries, uint32_t startIndex);
	uint64_t					getAtomLoadAddress(const ObjectFile::Atom* atom);
	uint8_t						ordinalForLibrary(ObjectFile::Reader* file);
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
	bool						stubableReference(const ObjectFile::Reference* ref);
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
	bool						mightNeedPadSegment();
	void						scanForAbsoluteReferences();
	bool						needsModuleTable();
	void						optimizeDylibReferences();
	bool						indirectSymbolIsLocal(const ObjectFile::Reference* ref) const;

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
	friend class SymbolTableLoadCommandsAtom<A>;
	friend class ThreadsLoadCommandsAtom<A>;
	friend class DylibIDLoadCommandsAtom<A>;
	friend class RoutinesLoadCommandsAtom<A>;
	friend class DyldLoadCommandsAtom<A>;
	friend class UUIDLoadCommandAtom<A>;
	friend class LinkEditAtom<A>;
	friend class SectionRelocationsLinkEditAtom<A>;
	friend class LocalRelocationsLinkEditAtom<A>;
	friend class ExternalRelocationsLinkEditAtom<A>;
	friend class SymbolTableLinkEditAtom<A>;
	friend class SegmentSplitInfoLoadCommandsAtom<A>;
	friend class SegmentSplitInfoContentAtom<A>;
//	friend class IndirectTableLinkEditAtom<A>;
	friend class ModuleInfoLinkEditAtom<A>;
	friend class StringsLinkEditAtom<A>;
	friend class LoadCommandsPaddingAtom<A>;
	friend class StubAtom<A>;
	friend class StubHelperAtom<A>;
	friend class LazyPointerAtom<A>;
	friend class NonLazyPointerAtom<A>;
	friend class DylibLoadCommandsAtom<A>;

	const char*										fFilePath;
	Options&										fOptions;
	int												fFileDescriptor;
	std::vector<class ObjectFile::Atom*>*			fAllAtoms;
	std::vector<class ObjectFile::Reader::Stab>*	fStabs;
	class SectionInfo*								fLoadCommandsSection;
	class SegmentInfo*								fLoadCommandsSegment;
	class SegmentLoadCommandsAtom<A>*				fSegmentCommands;
	class SymbolTableLoadCommandsAtom<A>*			fSymbolTableCommands;
	class LoadCommandsPaddingAtom<A>*				fHeaderPadding;
	class UUIDLoadCommandAtom<A>*				    fUUIDAtom;
	std::vector<class ObjectFile::Atom*>			fWriterSynthesizedAtoms;
	std::vector<SegmentInfo*>						fSegmentInfos;
	class SegmentInfo*								fPadSegmentInfo;
	class ObjectFile::Atom*							fEntryPoint;
	class ObjectFile::Atom*							fDyldHelper;
	std::map<class ObjectFile::Reader*, DylibLoadCommandsAtom<A>*>	fLibraryToLoadCommand;
	std::map<class ObjectFile::Reader*, uint32_t>	fLibraryToOrdinal;
	std::map<class ObjectFile::Reader*, class ObjectFile::Reader*>	fLibraryAliases;
	std::vector<class ObjectFile::Atom*>			fExportedAtoms;
	std::vector<class ObjectFile::Atom*>			fImportedAtoms;
	std::vector<class ObjectFile::Atom*>			fLocalSymbolAtoms;
	std::vector<macho_nlist<P> >					fLocalExtraLabels;
	std::vector<macho_nlist<P> >					fGlobalExtraLabels;
	class SectionRelocationsLinkEditAtom<A>*		fSectionRelocationsAtom;
	class LocalRelocationsLinkEditAtom<A>*			fLocalRelocationsAtom;
	class ExternalRelocationsLinkEditAtom<A>*		fExternalRelocationsAtom;
	class SymbolTableLinkEditAtom<A>*				fSymbolTableAtom;
	class SegmentSplitInfoContentAtom<A>*			fSplitCodeToDataContentAtom;
	class IndirectTableLinkEditAtom<A>*				fIndirectTableAtom;
	class ModuleInfoLinkEditAtom<A>*				fModuleInfoAtom;
	class StringsLinkEditAtom<A>*					fStringsAtom;
	class PageZeroAtom<A>*							fPageZeroAtom;
	macho_nlist<P>*									fSymbolTable;
	std::vector<macho_relocation_info<P> >			fSectionRelocs;
	std::vector<macho_relocation_info<P> >			fInternalRelocs;
	std::vector<macho_relocation_info<P> >			fExternalRelocs;
	std::map<const ObjectFile::Atom*,ObjectFile::Atom*>	fStubsMap;
	std::map<ObjectFile::Atom*,ObjectFile::Atom*>	fGOTMap;
	std::vector<class StubAtom<A>*>					fAllSynthesizedStubs;
	std::vector<ObjectFile::Atom*>					fAllSynthesizedStubHelpers;
	std::vector<class LazyPointerAtom<A>*>			fAllSynthesizedLazyPointers;
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
													MachHeaderAtom(Writer<A>& writer) : WriterAtom<A>(writer, Segment::fgTextSegment) {}
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
											LoadCommandAtom(Writer<A>& writer, Segment& segment) : WriterAtom<A>(writer, segment), fOrdinal(fgCurrentOrdinal++) {}
	virtual ObjectFile::Alignment			getAlignment() const	{ return ObjectFile::Alignment(log2(sizeof(typename A::P::uint_t))); }
	virtual const char*						getSectionName() const	{ return "._load_commands"; }
	virtual uint32_t						getOrdinal() const		{ return fOrdinal; }
	static uint64_t							alignedSize(uint64_t size);
private:
	uint32_t								fOrdinal;
	static uint32_t							fgCurrentOrdinal;
};

template <typename A> uint32_t LoadCommandAtom<A>::fgCurrentOrdinal = 0;

template <typename A>
class SegmentLoadCommandsAtom : public LoadCommandAtom<A>
{
public:
											SegmentLoadCommandsAtom(Writer<A>& writer)  
												: LoadCommandAtom<A>(writer, Segment::fgTextSegment), fCommandCount(0), fSize(0) 
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
												: LoadCommandAtom<A>(writer, Segment::fgTextSegment) {}
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
											DyldLoadCommandsAtom(Writer<A>& writer)  : LoadCommandAtom<A>(writer, Segment::fgTextSegment) {}
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
											SegmentSplitInfoLoadCommandsAtom(Writer<A>& writer)  : LoadCommandAtom<A>(writer, Segment::fgTextSegment) {}
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
		LoadCommandAtom<A>(writer, Segment::fgTextSegment), clientString(client) {}
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
											 : LoadCommandAtom<A>(writer, Segment::fgTextSegment), fInfo(info), fOptimizedAway(false) {}
	virtual const char*						getDisplayName() const	{ return "dylib load command"; }
	virtual uint64_t						getSize() const;
	virtual void							copyRawContent(uint8_t buffer[]) const;
	virtual void							optimizeAway() { fOptimizedAway = true; }
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
											DylibIDLoadCommandsAtom(Writer<A>& writer) : LoadCommandAtom<A>(writer, Segment::fgTextSegment) {}
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
											RoutinesLoadCommandsAtom(Writer<A>& writer) : LoadCommandAtom<A>(writer, Segment::fgTextSegment) {}
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
											 : LoadCommandAtom<A>(writer, Segment::fgTextSegment), fName(name) {}
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
												: LoadCommandAtom<A>(writer, Segment::fgTextSegment), fNameStart(nameStart), fNameLength(nameLen) {}
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
													: LoadCommandAtom<A>(writer, Segment::fgTextSegment), fName(name) {}
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
												: LoadCommandAtom<A>(writer, Segment::fgTextSegment), fEmit(false) {}
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
												: LoadCommandAtom<A>(writer, Segment::fgTextSegment), fPath(path) {}
	virtual const char*						getDisplayName() const	{ return "rpath load command"; }
	virtual uint64_t						getSize() const;
	virtual void							copyRawContent(uint8_t buffer[]) const;
private:
	using WriterAtom<A>::fWriter;
	typedef typename A::P					P;
	const char*								fPath;
};


template <typename A>
class LoadCommandsPaddingAtom : public WriterAtom<A>
{
public:
											LoadCommandsPaddingAtom(Writer<A>& writer)
													: WriterAtom<A>(writer, Segment::fgTextSegment), fSize(0) {}
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
	virtual const char*						getDisplayName() const	{ return "modulel table"; }
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
											StubAtom(Writer<A>& writer, ObjectFile::Atom& target);
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
	bool									pic() const;
	using WriterAtom<A>::fWriter;
	const char*								fName;
	ObjectFile::Atom&						fTarget;
	std::vector<ObjectFile::Reference*>		fReferences;
};

template <typename A>
class StubHelperAtom : public WriterAtom<A>
{
public:
											StubHelperAtom(Writer<A>& writer, ObjectFile::Atom& target, ObjectFile::Atom& lazyPointer);
	virtual const char*						getName() const				{ return fName; }
	virtual ObjectFile::Atom::Scope			getScope() const			{ return ObjectFile::Atom::scopeLinkageUnit; }
	virtual uint64_t						getSize() const;
	virtual const char*						getSectionName() const		{ return "__stub_helper"; }
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const	{ return (std::vector<ObjectFile::Reference*>&)(fReferences); }
	virtual void							copyRawContent(uint8_t buffer[]) const;
	ObjectFile::Atom*						getTarget()					{ return &fTarget; }
private:
	static const char*						stubName(const char* importName);
	using WriterAtom<A>::fWriter;
	const char*								fName;
	ObjectFile::Atom&						fTarget;
	std::vector<ObjectFile::Reference*>		fReferences;
};

template <typename A>
class LazyPointerAtom : public WriterAtom<A>
{
public:
											LazyPointerAtom(Writer<A>& writer, ObjectFile::Atom& target);
	virtual const char*						getName() const				{ return fName; }
	virtual ObjectFile::Atom::Scope			getScope() const			{ return ObjectFile::Atom::scopeLinkageUnit; }
	virtual uint64_t						getSize() const				{ return sizeof(typename A::P::uint_t); }
	virtual const char*						getSectionName() const		{ return "__la_symbol_ptr"; }
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const		{ return (std::vector<ObjectFile::Reference*>&)(fReferences); }
	virtual void							copyRawContent(uint8_t buffer[]) const;
	ObjectFile::Atom*						getTarget()					{ return &fTarget; }
private:
	using WriterAtom<A>::fWriter;
	static const char*						lazyPointerName(const char* importName);
	const char*								fName;
	ObjectFile::Atom&						fTarget;
	std::vector<ObjectFile::Reference*>		fReferences;
};


template <typename A>
class NonLazyPointerAtom : public WriterAtom<A>
{
public:
											NonLazyPointerAtom(Writer<A>& writer, ObjectFile::Atom& target);
	virtual const char*						getName() const				{ return fName; }
	virtual ObjectFile::Atom::Scope			getScope() const			{ return ObjectFile::Atom::scopeLinkageUnit; }
	virtual uint64_t						getSize() const				{ return sizeof(typename A::P::uint_t); }
	virtual const char*						getSectionName() const		{ return "__nl_symbol_ptr"; }
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const		{ return (std::vector<ObjectFile::Reference*>&)(fReferences); }
	virtual void							copyRawContent(uint8_t buffer[]) const;
	ObjectFile::Atom*						getTarget()					{ return &fTarget; }
private:
	using WriterAtom<A>::fWriter;
	static const char*						nonlazyPointerName(const char* importName);
	const char*								fName;
	ObjectFile::Atom&						fTarget;
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
										  : fKind(kind), fFixUpOffsetInSrc(offset), fTarget(target),
											fTargetOffset(toOffset), fFromTarget(fromTarget), fFromTargetOffset(fromOffset) {}

	virtual					~WriterReference() {}

	virtual ObjectFile::Reference::TargetBinding getTargetBinding() const { return ObjectFile::Reference::kBoundDirectly; }
	virtual ObjectFile::Reference::TargetBinding getFromTargetBinding() const { return (fFromTarget != NULL) ? ObjectFile::Reference::kBoundDirectly : ObjectFile::Reference::kDontBind; }
	virtual uint8_t			getKind() const									{ return (uint8_t)fKind; }
	virtual uint64_t		getFixUpOffset() const							{ return fFixUpOffsetInSrc; }
	virtual const char*		getTargetName() const							{ return fTarget->getName(); }
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
	uint32_t				fTargetOffset;
	ObjectFile::Atom*		fFromTarget;
	uint32_t				fFromTargetOffset;
};



template <>
StubHelperAtom<x86_64>::StubHelperAtom(Writer<x86_64>& writer, ObjectFile::Atom& target, ObjectFile::Atom& lazyPointer)
 : WriterAtom<x86_64>(writer, Segment::fgTextSegment), fName(stubName(target.getName())), fTarget(target)
{
	writer.fAllSynthesizedStubHelpers.push_back(this);

	fReferences.push_back(new WriterReference<x86_64>(3, x86_64::kPCRel32, &lazyPointer));
	fReferences.push_back(new WriterReference<x86_64>(8, x86_64::kPCRel32, writer.fDyldHelper));
	if ( writer.fDyldHelper == NULL )
		throw "symbol dyld_stub_binding_helper not defined (usually in crt1.o/dylib1.o/bundle1.o)";
}

template <>
uint64_t StubHelperAtom<x86_64>::getSize() const
{
	return 12;
}

template <>
void StubHelperAtom<x86_64>::copyRawContent(uint8_t buffer[]) const
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

template <typename A>
const char* StubHelperAtom<A>::stubName(const char* name)
{
	char* buf;
	asprintf(&buf, "%s$stubHelper", name);
	return buf;
}


// specialize lazy pointer for x86_64 to initially pointer to stub helper
template <>
LazyPointerAtom<x86_64>::LazyPointerAtom(Writer<x86_64>& writer, ObjectFile::Atom& target)
 : WriterAtom<x86_64>(writer, Segment::fgDataSegment), fName(lazyPointerName(target.getName())), fTarget(target)
{
	writer.fAllSynthesizedLazyPointers.push_back(this);

	StubHelperAtom<x86_64>* helper = new StubHelperAtom<x86_64>(writer, target, *this);
	fReferences.push_back(new WriterReference<x86_64>(0, x86_64::kPointer, helper));
}


template <typename A>
LazyPointerAtom<A>::LazyPointerAtom(Writer<A>& writer, ObjectFile::Atom& target)
 : WriterAtom<A>(writer, Segment::fgDataSegment), fName(lazyPointerName(target.getName())), fTarget(target)
{
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
 : WriterAtom<A>(writer, Segment::fgDataSegment), fName(nonlazyPointerName(target.getName())), fTarget(target)
{
	writer.fAllSynthesizedNonLazyPointers.push_back(this);

	fReferences.push_back(new WriterReference<A>(0, A::kPointer, &target));
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
bool StubAtom<ppc>::pic() const
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
StubAtom<ppc>::StubAtom(Writer<ppc>& writer, ObjectFile::Atom& target)
 : WriterAtom<ppc>(writer, Segment::fgTextSegment), fName(stubName(target.getName())), fTarget(target)
{
	writer.fAllSynthesizedStubs.push_back(this);

	LazyPointerAtom<ppc>* lp = new LazyPointerAtom<ppc>(writer, target);
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
StubAtom<ppc64>::StubAtom(Writer<ppc64>& writer, ObjectFile::Atom& target)
 : WriterAtom<ppc64>(writer, Segment::fgTextSegment), fName(stubName(target.getName())), fTarget(target)
{
	writer.fAllSynthesizedStubs.push_back(this);

	LazyPointerAtom<ppc64>* lp = new LazyPointerAtom<ppc64>(writer, target);
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

// specialize to put x86 fast stub in __IMPORT segment with no lazy pointer
template <>
StubAtom<x86>::StubAtom(Writer<x86>& writer, ObjectFile::Atom& target)
 : WriterAtom<x86>(writer, writer.fOptions.readOnlyx86Stubs() ? Segment::fgROImportSegment : Segment::fgImportSegment), 
				fTarget(target)
{
	if ( &target == NULL )
		fName = "cache-line-crossing-stub";
	else {
		fName = stubName(target.getName());
		writer.fAllSynthesizedStubs.push_back(this);
	}
}

template <>
StubAtom<x86_64>::StubAtom(Writer<x86_64>& writer, ObjectFile::Atom& target)
 : WriterAtom<x86_64>(writer, Segment::fgTextSegment), fName(stubName(target.getName())), fTarget(target)
{
	writer.fAllSynthesizedStubs.push_back(this);

	LazyPointerAtom<x86_64>* lp = new LazyPointerAtom<x86_64>(writer, target);
	fReferences.push_back(new WriterReference<x86_64>(2, x86_64::kPCRel32, lp));
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
uint64_t StubAtom<x86>::getSize() const
{
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
	// special case x86 fast stubs to be byte aligned
	return 0;
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

// x86_64 stubs are 7 bytes and need no alignment
template <>
ObjectFile::Alignment StubAtom<x86_64>::getAlignment() const
{
	return 0;
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
const char*	StubAtom<x86>::getSectionName() const
{
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
	: ExecutableFile::Writer(dynamicLibraries), fFilePath(strdup(path)), fOptions(options), fLoadCommandsSection(NULL),
	  fLoadCommandsSegment(NULL), fPadSegmentInfo(NULL),  fSplitCodeToDataContentAtom(NULL), fModuleInfoAtom(NULL), 
	  fPageZeroAtom(NULL),  fSymbolTableCount(0),  fLargestAtomSize(1), 
	 fEmitVirtualSections(false), fHasWeakExports(false), fReferencesWeakImports(false), 
	  fCanScatter(false), fWritableSegmentPastFirst4GB(false), fNoReExportedDylibs(false), fSlideable(false), 
	  fFirstWritableSegment(NULL), fAnonNameIndex(1000)
{
	// for UNIX conformance, error if file exists and is not writable
	if ( (access(path, F_OK) == 0) && (access(path, W_OK) == -1) )
		throwf("can't write output file: %s", path);

	int permissions = 0777;
	if ( fOptions.outputKind() == Options::kObjectFile )
		permissions = 0666;
	// Calling unlink first assures the file is gone so that open creates it with correct permissions
	// It also handles the case where fFilePath file is not writable but its directory is
	// And it means we don't have to truncate the file when done writing (in case new is smaller than old)
	(void)unlink(fFilePath);
	fFileDescriptor = open(fFilePath, O_CREAT | O_WRONLY | O_TRUNC, permissions);
	if ( fFileDescriptor == -1 ) {
		throwf("can't open output file for writing: %s", path);
	}

	switch ( fOptions.outputKind() ) {
		case Options::kDynamicExecutable:
		case Options::kStaticExecutable:
			if ( fOptions.zeroPageSize() != 0 )
				fWriterSynthesizedAtoms.push_back(fPageZeroAtom = new PageZeroAtom<A>(*this));
			if ( fOptions.outputKind() == Options::kDynamicExecutable )
				fWriterSynthesizedAtoms.push_back(new DsoHandleAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new MachHeaderAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new SegmentLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new SymbolTableLoadCommandsAtom<A>(*this));
			if ( fOptions.outputKind() == Options::kDynamicExecutable )
				fWriterSynthesizedAtoms.push_back(new DyldLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fUUIDAtom = new UUIDLoadCommandAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new ThreadsLoadCommandsAtom<A>(*this));
			if ( fOptions.hasCustomStack() )
				fWriterSynthesizedAtoms.push_back(new CustomStackAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fHeaderPadding = new LoadCommandsPaddingAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fSectionRelocationsAtom = new SectionRelocationsLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fLocalRelocationsAtom = new LocalRelocationsLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fSymbolTableAtom = new SymbolTableLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fExternalRelocationsAtom = new ExternalRelocationsLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fIndirectTableAtom = new IndirectTableLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fStringsAtom = new StringsLinkEditAtom<A>(*this));
			break;
		case Options::kDynamicLibrary:
		case Options::kDynamicBundle:
			fWriterSynthesizedAtoms.push_back(new DsoHandleAtom<A>(*this));
			// fall through
		case Options::kObjectFile:
			fWriterSynthesizedAtoms.push_back(new MachHeaderAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new SegmentLoadCommandsAtom<A>(*this));
			if ( fOptions.outputKind() == Options::kDynamicLibrary ) {
				fWriterSynthesizedAtoms.push_back(new DylibIDLoadCommandsAtom<A>(*this));
				if ( fOptions.initFunctionName() != NULL )
					fWriterSynthesizedAtoms.push_back(new RoutinesLoadCommandsAtom<A>(*this));
			}
			fWriterSynthesizedAtoms.push_back(fUUIDAtom = new UUIDLoadCommandAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new SymbolTableLoadCommandsAtom<A>(*this));
			if ( fOptions.sharedRegionEligible() )
				fWriterSynthesizedAtoms.push_back(new SegmentSplitInfoLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fHeaderPadding = new LoadCommandsPaddingAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fSectionRelocationsAtom = new SectionRelocationsLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fLocalRelocationsAtom = new LocalRelocationsLinkEditAtom<A>(*this));
			if ( fOptions.sharedRegionEligible() ) {
				fWriterSynthesizedAtoms.push_back(fSplitCodeToDataContentAtom = new SegmentSplitInfoContentAtom<A>(*this));
			}
			fWriterSynthesizedAtoms.push_back(fSymbolTableAtom = new SymbolTableLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fExternalRelocationsAtom = new ExternalRelocationsLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fIndirectTableAtom = new IndirectTableLinkEditAtom<A>(*this));
			if ( this->needsModuleTable() )
				fWriterSynthesizedAtoms.push_back(fModuleInfoAtom = new ModuleInfoLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fStringsAtom = new StringsLinkEditAtom<A>(*this));
			break;
		case Options::kDyld:
			fWriterSynthesizedAtoms.push_back(new DsoHandleAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new MachHeaderAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new SegmentLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new SymbolTableLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new DyldLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fUUIDAtom = new UUIDLoadCommandAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(new ThreadsLoadCommandsAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fHeaderPadding = new LoadCommandsPaddingAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fLocalRelocationsAtom = new LocalRelocationsLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fSymbolTableAtom = new SymbolTableLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fExternalRelocationsAtom = new ExternalRelocationsLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fIndirectTableAtom = new IndirectTableLinkEditAtom<A>(*this));
			fWriterSynthesizedAtoms.push_back(fStringsAtom = new StringsLinkEditAtom<A>(*this));
			break;
	}

	// add extra commmands
	bool hasReExports = false;
	uint8_t ordinal = 1;
	switch ( fOptions.outputKind() ) {
		case Options::kDynamicExecutable:
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
			fSlideable =	true;
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
	if ( fOptions.outputKind() == Options::kObjectFile ) {
		// when doing -r -exported_symbols_list, don't creat proxy for a symbol
		// that is supposed to be exported.  We want an error instead
		// <rdar://problem/5062685> ld does not report error when -r is used and exported symbols are not defined.
		if ( fOptions.hasExportRestrictList() && fOptions.shouldExport(name) )
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
ObjectFile::Atom& Writer<A>::makeObjcInfoAtom(ObjectFile::Reader::ObjcConstraint objcContraint, bool objcReplacementClasses)
{
	return *(new ObjCInfoAtom<A>(*this, objcContraint, objcReplacementClasses));
}


template <typename A>
uint64_t Writer<A>::write(std::vector<class ObjectFile::Atom*>& atoms,
						  std::vector<class ObjectFile::Reader::Stab>& stabs,
						  class ObjectFile::Atom* entryPointAtom, class ObjectFile::Atom* dyldHelperAtom,
						  bool createUUID, bool canScatter, ObjectFile::Reader::CpuConstraint cpuConstraint,
						  bool biggerThanTwoGigs)
{
	fAllAtoms =  &atoms;
	fStabs =  &stabs;
	fEntryPoint = entryPointAtom;
	fDyldHelper = dyldHelperAtom;
	fCanScatter = canScatter;
	fCpuConstraint = cpuConstraint;
	fBiggerThanTwoGigs = biggerThanTwoGigs;

	try {
		// Set for create UUID
		if (createUUID)
			fUUIDAtom->generate();

		// remove uneeded dylib load commands
		optimizeDylibReferences();

		// check for mdynamic-no-pic codegen which force code into low 4GB
		scanForAbsoluteReferences();

		// create inter-library stubs
		synthesizeStubs();

		// create SegmentInfo and SectionInfo objects and assign all atoms to a section
		partitionIntoSections();

		// segment load command can now be sized and padding can be set
		adjustLoadCommandsAndPadding();

		// assign each section a file offset
		assignFileOffsets();

		// if need to add branch islands, reassign file offsets
		if ( addBranchIslands() )
			assignFileOffsets();

		// build symbol table and relocations
		buildLinkEdit();

		// write map file if requested
		writeMap();

		// write everything
		return writeAtoms();
	} catch (...) {
		// clean up if any errors
		close(fFileDescriptor);
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
const char* Writer<x86_64>::symbolTableName(const ObjectFile::Atom* atom)
{
	static unsigned int counter = 0;
	const char* name = atom->getName();
	if ( strncmp(name, "cstring=", 8) == 0 )
		asprintf((char**)&name, "LC%u", counter++);
	return name;
}

template <typename A>
const char* Writer<A>::symbolTableName(const ObjectFile::Atom* atom)
{
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
	if ( atom->getSymbolTableInclusion() == ObjectFile::Atom::kSymbolTableInAndNeverStrip )
		desc |= REFERENCED_DYNAMICALLY;
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
	if ( (fOptions.outputKind() == Options::kObjectFile) 
		&& (atom->getScope() == ObjectFile::Atom::scopeLinkageUnit) 
		&& (atom->getDefinitionKind() == ObjectFile::Atom::kTentativeDefinition) )
		entry->set_n_type(N_UNDF | N_EXT | N_PEXT);
	else if ( fOptions.prebind() )
		entry->set_n_type(N_PBUD | N_EXT);
	else 
		entry->set_n_type(N_UNDF | N_EXT);

	// set n_sect
	entry->set_n_sect(0);

	uint16_t desc = 0;
	if ( fOptions.outputKind() != Options::kObjectFile ) {
		// set n_desc ( high byte is library ordinal, low byte is reference type )
		std::map<const ObjectFile::Atom*,ObjectFile::Atom*>::iterator pos = fStubsMap.find(atom);
		if ( pos != fStubsMap.end() )
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
		// if alignment does not match size, then record the custom alignment
		if ( align != __builtin_ctz(atom->getSize()) )
			SET_COMM_ALIGN(desc, align);
	}
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
		sprintf(anonName, "l%u", fAnonNameIndex++);
		symbolName = anonName;
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
	if ( atom->getDefinitionKind() == ObjectFile::Atom::kWeakDefinition )
		desc |= N_WEAK_DEF;
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
				case ObjectFile::Atom::kRegularDefinition:
				case ObjectFile::Atom::kWeakDefinition:
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
		if ( fOptions.outputKind() == Options::kObjectFile) {
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
		else if ( fOptions.outputKind() == Options::kStaticExecutable ) {
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
	// search imports
	int i = 0;
	for(std::vector<ObjectFile::Atom*>::iterator it=fImportedAtoms.begin(); it != fImportedAtoms.end(); ++it) {
		if ( &atom == *it )
			return i + fSymbolTableImportStartIndex;
		++i;
	}

	// search locals
	i = 0;
	for(std::vector<ObjectFile::Atom*>::iterator it=fLocalSymbolAtoms.begin(); it != fLocalSymbolAtoms.end(); ++it) {
		if ( &atom == *it )
			return i + fSymbolTableLocalStartIndex;
		++i;
	}

	// search exports
	i = 0;
	for(std::vector<ObjectFile::Atom*>::iterator it=fExportedAtoms.begin(); it != fExportedAtoms.end(); ++it) {
		if ( &atom == *it )
			return i + fSymbolTableExportStartIndex;
		++i;
	}

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
		case x86_64::kFollowOn:
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
		fprintf(stderr, "ld: warning section index == 0 for %s (kind=%d, scope=%d, inclusion=%d) in %s\n",
		 target.getDisplayName(), target.getDefinitionKind(), target.getScope(), target.getSymbolTableInclusion(), target.getFile()->getPath());


	switch ( kind ) {
		case x86::kNoFixUp:
		case x86::kFollowOn:
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
				//if ( &ref->getFromTarget() == &ref->getTarget() )
					sreloc2->set_r_value(ref->getFromTarget().getAddress() + ref->getFromTargetOffset());
				//else
				//	sreloc2->set_r_value(ref->getFromTarget().getAddress());
				fSectionRelocs.push_back(reloc2);
				fSectionRelocs.push_back(reloc1);
				return 2;
			}

		case x86::kPCRel32WeakImport:
		case x86::kPCRel32:
		case x86::kPCRel16:
		case x86::kDtraceProbeSite:
		case x86::kDtraceIsEnabledSite:
			if ( !isExtern && (ref->getTargetOffset() != 0) ) {
				// use scattered reloc is target offset is non-zero
				sreloc1->set_r_scattered(true);
				sreloc1->set_r_pcrel(true);
				sreloc1->set_r_length( (kind==x86::kPCRel16) ? 1 : 2);
				sreloc1->set_r_type(GENERIC_RELOC_VANILLA);
				sreloc1->set_r_address(address);
				sreloc1->set_r_value(target.getAddress());
			}
			else {
				reloc1.set_r_address(address);
				reloc1.set_r_symbolnum(isExtern ? symbolIndex : sectionNum);
				reloc1.set_r_pcrel(true);
				reloc1.set_r_length( (kind==x86::kPCRel16) ? 1 : 2);
				reloc1.set_r_extern(isExtern);
				reloc1.set_r_type(GENERIC_RELOC_VANILLA);
			}
			fSectionRelocs.push_back(reloc1);
			return 1;
			
		case x86::kDtraceTypeReference:
		case x86::kDtraceProbe:
			// generates no relocs
			return 0;

	}
	return 0;
}




template <> uint64_t    Writer<ppc>::maxAddress() { return 0xFFFFFFFFULL; }
template <> uint64_t  Writer<ppc64>::maxAddress() { return 0xFFFFFFFFFFFFFFFFULL; }
template <> uint64_t    Writer<x86>::maxAddress() { return 0xFFFFFFFFULL; }
template <> uint64_t Writer<x86_64>::maxAddress() { return 0xFFFFFFFFFFFFFFFFULL; }


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
				pint_t toAddr = target.getAddress() + ref->getTargetOffset();
				pint_t fromAddr = ref->getFromTarget().getAddress() + ref->getFromTargetOffset();
				sreloc1->set_r_scattered(true);
				sreloc1->set_r_pcrel(false);
				sreloc1->set_r_length( (kind == A::kPointerDiff32) ? 2 : ((kind == A::kPointerDiff64) ? 3 : 1));
				sreloc1->set_r_type(ref->getTargetOffset() != 0 ? PPC_RELOC_LOCAL_SECTDIFF : PPC_RELOC_SECTDIFF);
				sreloc1->set_r_address(address);
				sreloc1->set_r_value(toAddr);
				sreloc2->set_r_scattered(true);
				sreloc2->set_r_pcrel(false);
				sreloc1->set_r_length( (kind == A::kPointerDiff32) ? 2 : ((kind == A::kPointerDiff64) ? 3 : 1));
				sreloc2->set_r_type(PPC_RELOC_PAIR);
				sreloc2->set_r_address(0);
				sreloc2->set_r_value(fromAddr);
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
bool Writer<A>::indirectSymbolIsLocal(const ObjectFile::Reference* ref) const
{
	//  use INDIRECT_SYMBOL_LOCAL in non-lazy-pointers for atoms that won't be in symbol table or have an addend
	return ( !this->shouldExport(ref->getTarget()) || (ref->getTargetOffset() != 0) );
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
				if ( curSection->fAllNonLazyPointers || curSection->fAllLazyPointers || curSection->fAllStubs )
					curSection->fIndirectSymbolOffset = fIndirectTableAtom->fTable.size();
				curSection->fRelocOffset = relocIndex;
				const int atomCount = sectionAtoms.size();
				for (int k=0; k < atomCount; ++k) {
					ObjectFile::Atom* atom = sectionAtoms[k];
					//fprintf(stderr, "buildObjectFileFixups(): atom %s\n", atom->getDisplayName());
					std::vector<ObjectFile::Reference*>& refs = atom->getReferences();
					const int refCount = refs.size();
					for (int l=0; l < refCount; ++l) {
						ObjectFile::Reference* ref = refs[l];
						if ( curSection->fAllNonLazyPointers || curSection->fAllLazyPointers || curSection->fAllStubs ) {
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
								if ( this->indirectSymbolIsLocal(ref) )
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
									fprintf(stderr, "lazy pointer %s missing initial binding\n", atom->getDisplayName());
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
				if ( this->shouldExport(target) && fOptions.interposable() )
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
			  || fOptions.interposable()) 
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
uint64_t Writer<x86_64>::relocAddressInFinalLinkedImage(uint64_t address, const ObjectFile::Atom* atom)  const
{
	// for x86_64, the r_address field in relocs for final linked images 
	// is the offset from the start address of the first writable segment
	uint64_t result = address - fFirstWritableSegment->fBaseAddress;
	if ( result > 0xFFFFFFFF ) {
		throwf("image too large: address can't fit in 32-bit r_address field in %s from %s",
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


template <typename A>
void Writer<A>::buildExecutableFixups()
{
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
				if ( curSection->fAllNonLazyPointers || curSection->fAllLazyPointers || curSection->fAllStubs || curSection->fAllSelfModifyingStubs )
					curSection->fIndirectSymbolOffset = fIndirectTableAtom->fTable.size();
				const int atomCount = sectionAtoms.size();
				for (int k=0; k < atomCount; ++k) {
					ObjectFile::Atom* atom = sectionAtoms[k];
					std::vector<ObjectFile::Reference*>& refs = atom->getReferences();
					const int refCount = refs.size();
					//fprintf(stderr, "atom %s has %d references in section %s, %p\n", atom->getDisplayName(), refCount, curSection->fSectionName, atom->getSection());
					for (int l=0; l < refCount; ++l) {
						ObjectFile::Reference* ref = refs[l];
						if ( curSection->fAllNonLazyPointers || curSection->fAllLazyPointers ) {
							// if atom is in (non)lazy_pointer section, this is encoded as an indirect symbol
							if ( atom->getSize() != sizeof(pint_t) ) {
								printf("wrong size pointer atom %s from file %s\n", atom->getDisplayName(), atom->getFile()->getPath());
							}
							ObjectFile::Atom* pointerTarget = &(ref->getTarget());
							if ( curSection->fAllLazyPointers ) {
								pointerTarget = ((LazyPointerAtom<A>*)atom)->getTarget();
							}
							uint32_t offsetInSection = atom->getSectionOffset();
							uint32_t indexInSection = offsetInSection / sizeof(pint_t);
							uint32_t undefinedSymbolIndex = INDIRECT_SYMBOL_LOCAL;
							if ( this->relocationNeededInFinalLinkedImage(*pointerTarget) == kRelocExternal )
								undefinedSymbolIndex = this->symbolIndex(*pointerTarget);
							uint32_t indirectTableIndex = indexInSection + curSection->fIndirectSymbolOffset;
							IndirectEntry entry = { indirectTableIndex, undefinedSymbolIndex };
							//fprintf(stderr,"fIndirectTableAtom->fTable.add(%d-%d => 0x%X-%s), size=%lld\n", indexInSection, indirectTableIndex, undefinedSymbolIndex, ref->getTarget().getName(), atom->getSize());
							fIndirectTableAtom->fTable.push_back(entry);
							if ( curSection->fAllLazyPointers ) {
								uint8_t preboundLazyType;
								if ( fOptions.prebind() && (fDyldHelper != NULL) && preboundLazyPointerType(&preboundLazyType) ) {
									// this is a prebound image, need special relocs for dyld to reset lazy pointers if prebinding is invalid
									macho_scattered_relocation_info<P> pblaReloc;
									pblaReloc.set_r_scattered(true);
									pblaReloc.set_r_pcrel(false);
									pblaReloc.set_r_length();
									pblaReloc.set_r_type(preboundLazyType);
									pblaReloc.set_r_address(relocAddressInFinalLinkedImage(atom->getAddress(), atom));
									pblaReloc.set_r_value(fDyldHelper->getAddress());
									fInternalRelocs.push_back(*((macho_relocation_info<P>*)&pblaReloc));
								}
								else if ( fSlideable ) {
									// this is a non-prebound dylib/bundle, need vanilla internal relocation to fix up binding handler if image slides
									macho_relocation_info<P> dyldHelperReloc;
									uint32_t sectionNum = 1;
									if ( fDyldHelper != NULL )
										sectionNum = ((SectionInfo*)(fDyldHelper->getSection()))->getIndex();
									//fprintf(stderr, "lazy pointer reloc, section index=%u, section name=%s\n", sectionNum, curSection->fSectionName);
									dyldHelperReloc.set_r_address(relocAddressInFinalLinkedImage(atom->getAddress(), atom));
									dyldHelperReloc.set_r_symbolnum(sectionNum);
									dyldHelperReloc.set_r_pcrel(false);
									dyldHelperReloc.set_r_length();
									dyldHelperReloc.set_r_extern(false);
									dyldHelperReloc.set_r_type(GENERIC_RELOC_VANILLA);
									fInternalRelocs.push_back(dyldHelperReloc);
								}
							}
						}
 						else if ( (ref->getKind() == A::kPointer) || (ref->getKind() == A::kPointerWeakImport) ) {
							if ( fSlideable && ((curSegment->fInitProtection & VM_PROT_WRITE) == 0) ) {
								throwf("pointer in read-only segment not allowed in slidable image, used in %s from %s",
										atom->getDisplayName(), atom->getFile()->getPath());
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
									}
									break;
							}
						}
						else if ( this->illegalRelocInFinalLinkedImage(*ref) ) {
							throwf("absolute addressing (perhaps -mdynamic-no-pic) used in %s from %s not allowed in slidable image", atom->getDisplayName(), atom->getFile()->getPath());
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
		case ppc::kPointer:
		case ppc::kPointerWeakImport:
		case ppc::kPICBaseLow16:
		case ppc::kPICBaseLow14:
			// ignore
			break;
		default:
			fprintf(stderr, "ld: warning codegen with reference kind %d in %s prevents image from loading in dyld shared cache\n", ref->getKind(), atom->getDisplayName());
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
		case ppc64::kPointer:
		case ppc64::kPointerWeakImport:
		case ppc64::kPICBaseLow16:
		case ppc64::kPICBaseLow14:
			// ignore
			break;
		default:
			fprintf(stderr, "ld: warning codegen with reference kind %d in %s prevents image from loading in dyld shared cache\n", ref->getKind(), atom->getDisplayName());
			fSplitCodeToDataContentAtom->setCantEncode();
	}
}

template <>
void Writer<x86>::addCrossSegmentRef(const ObjectFile::Atom* atom, const ObjectFile::Reference* ref)
{
	switch ( (x86::ReferenceKinds)ref->getKind()  ) {
		case x86::kPointerDiff:
			if ( strcmp(ref->getTarget().getSegment().getName(), "__IMPORT") == 0 ) 
				fSplitCodeToDataContentAtom->add32bitImportLocation(atom, ref->getFixUpOffset());
			else
				fSplitCodeToDataContentAtom->add32bitPointerLocation(atom, ref->getFixUpOffset());
			break;
		case x86::kNoFixUp:
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
			fprintf(stderr, "ld: warning codegen in %s (offset 0x%08llX) prevents image from loading in dyld shared cache\n", atom->getDisplayName(), ref->getFixUpOffset());
			fSplitCodeToDataContentAtom->setCantEncode();
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
			fSplitCodeToDataContentAtom->add32bitPointerLocation(atom, ref->getFixUpOffset());
			break;
		case x86_64::kPointerDiff:
			fSplitCodeToDataContentAtom->add64bitPointerLocation(atom, ref->getFixUpOffset());
			break;
		case x86_64::kNoFixUp:
		case x86_64::kPointer:
			// ignore
			break;
		default:
			fprintf(stderr, "ld: warning codegen in %s with kind %d prevents image from loading in dyld shared cache\n", atom->getDisplayName(), ref->getKind());
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
void Writer<ppc>::writeNoOps(uint32_t from, uint32_t to)
{
	uint32_t ppcNop;
	OSWriteBigInt32(&ppcNop, 0, 0x60000000);
	for (uint32_t p=from; p < to; p += 4)
		::pwrite(fFileDescriptor, &ppcNop, 4, p);
}

template <>
void Writer<ppc64>::writeNoOps(uint32_t from, uint32_t to)
{
	uint32_t ppcNop;
	OSWriteBigInt32(&ppcNop, 0, 0x60000000);
	for (uint32_t p=from; p < to; p += 4)
		::pwrite(fFileDescriptor, &ppcNop, 4, p);
}

template <>
void Writer<x86>::writeNoOps(uint32_t from, uint32_t to)
{
	uint8_t x86Nop = 0x90;
	for (uint32_t p=from; p < to; ++p)
		::pwrite(fFileDescriptor, &x86Nop, 1, p);
}

template <>
void Writer<x86_64>::writeNoOps(uint32_t from, uint32_t to)
{
	uint8_t x86Nop = 0x90;
	for (uint32_t p=from; p < to; ++p)
		::pwrite(fFileDescriptor, &x86Nop, 1, p);
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
			fprintf(stderr, "ld: warning could not write map file: %s\n", fOptions.generatedMapPath());
		}
	}
}

template <typename A>
uint64_t Writer<A>::writeAtoms()
{
	// try to allocate buffer for entire output file content
	SectionInfo* lastSection = fSegmentInfos.back()->fSections.back();
	uint64_t fileBufferSize = (lastSection->fFileOffset + lastSection->fSize + 4095) & (-4096);
	uint8_t* wholeBuffer = (uint8_t*)calloc(fileBufferSize, 1);
	uint8_t* atomBuffer = NULL;
	bool streaming = false;
	if ( wholeBuffer == NULL ) {
		atomBuffer = new uint8_t[(fLargestAtomSize+4095) & (-4096)];
		streaming = true;
	}
	uint32_t size = 0;
	uint32_t end = 0;
	for (std::vector<SegmentInfo*>::iterator segit = fSegmentInfos.begin(); segit != fSegmentInfos.end(); ++segit) {
		SegmentInfo* curSegment = *segit;
		bool isTextSeg = (strcmp(curSegment->fName, "__TEXT") == 0);
		std::vector<SectionInfo*>& sectionInfos = curSegment->fSections;
		for (std::vector<SectionInfo*>::iterator secit = sectionInfos.begin(); secit != sectionInfos.end(); ++secit) {
			SectionInfo* curSection = *secit;
			std::vector<ObjectFile::Atom*>& sectionAtoms = curSection->fAtoms;
			//printf("writing with max atom size 0x%X\n", fLargestAtomSize);
			//fprintf(stderr, "writing %d atoms for section %s\n", (int)sectionAtoms.size(), curSection->fSectionName);
			if ( ! curSection->fAllZeroFill ) {
				end = curSection->fFileOffset;
				bool needsNops = isTextSeg && (strcmp(curSection->fSectionName, "__cstring") != 0);
				for (std::vector<ObjectFile::Atom*>::iterator ait = sectionAtoms.begin(); ait != sectionAtoms.end(); ++ait) {
					ObjectFile::Atom* atom = *ait;
					if ( (atom->getDefinitionKind() != ObjectFile::Atom::kExternalDefinition)
					  && (atom->getDefinitionKind() != ObjectFile::Atom::kExternalWeakDefinition)
					  && (atom->getDefinitionKind() != ObjectFile::Atom::kAbsoluteSymbol) ) {
						uint32_t fileOffset = curSection->fFileOffset + atom->getSectionOffset();
						if ( fileOffset != end ) {
							if ( needsNops ) {
								// fill gaps with no-ops
								if ( streaming )
									writeNoOps(end, fileOffset);
								else
									copyNoOps(&wholeBuffer[end], &wholeBuffer[fileOffset]);
							}
							else if ( streaming ) {
								// zero fill gaps
								if ( (fileOffset-end) == 4 ) {
									uint32_t zero = 0;
									::pwrite(fFileDescriptor, &zero, 4, end);
								}
								else {
									uint8_t zero = 0x00;
									for (uint32_t p=end; p < fileOffset; ++p)
										::pwrite(fFileDescriptor, &zero, 1, p);
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
						//fprintf(stderr, "writing 0x%08X -> 0x%08X (addr=0x%llX, size=0x%llX), atom %s from %s\n", 
						//	fileOffset, end, atom->getAddress(), atom->getSize(), atom->getDisplayName(), atom->getFile()->getPath());
						if ( streaming ) {
							// write out
							::pwrite(fFileDescriptor, buffer, atomSize, fileOffset);
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
				::lseek(fFileDescriptor, 0, SEEK_SET);
				ssize_t len;
				while ( (len = ::read(fFileDescriptor, md5Buffer, kMD5BufferSize)) > 0 ) 
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
			::pwrite(fFileDescriptor, atomBuffer, fUUIDAtom->getSize(), uuidOffset);
		}
		else {
			// if output file fit in memory, just genrate an md5 hash in memory
			CC_MD5(wholeBuffer, size, digest);
			fUUIDAtom->setContent(digest);
			uint32_t uuidOffset = ((SectionInfo*)fUUIDAtom->getSection())->fFileOffset + fUUIDAtom->getSectionOffset();
			fUUIDAtom->copyRawContent(&wholeBuffer[uuidOffset]);
		}
	}
	
	// finish up
	if ( streaming ) {
		delete [] atomBuffer;
	}
	else {
		// write whole output file in one chunk
		::pwrite(fFileDescriptor, wholeBuffer, size, 0);
		delete [] wholeBuffer;
	}
	
	close(fFileDescriptor);
	return end;
}


template <>
void Writer<x86>::fixUpReferenceFinal(const ObjectFile::Reference* ref, const ObjectFile::Atom* inAtom, uint8_t buffer[]) const
{
	uint32_t* fixUp = (uint32_t*)&buffer[ref->getFixUpOffset()];
	uint8_t*  dtraceProbeSite;
	const int64_t kTwoGigLimit = 0x7FFFFFFF;
	const int64_t kSixtyFourKiloLimit = 0x7FFF;
	int64_t displacement;
	x86::ReferenceKinds kind = (x86::ReferenceKinds)(ref->getKind());
	switch ( kind ) {
		case x86::kNoFixUp:
		case x86::kFollowOn:
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
					else {
						// external realocation ==> pointer contains addend
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
			if ( kind == x86::kPCRel16 ) {
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
					// external realocation ==> pointer contains addend
					LittleEndian::set32(*fixUp, ref->getTargetOffset());
					break;
				case ObjectFile::Atom::kAbsoluteSymbol:
					// pointer contains target address
					LittleEndian::set32(*fixUp, ref->getTarget().getSectionOffset() + ref->getTargetOffset());
					break;
			}
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
	uint32_t* fixUp = (uint32_t*)&buffer[ref->getFixUpOffset()];
	bool isExtern = this->makesExternalRelocatableReference(ref->getTarget());	
	int64_t displacement;
	x86::ReferenceKinds kind = (x86::ReferenceKinds)(ref->getKind());
	switch ( kind ) {
		case x86::kNoFixUp:
		case x86::kFollowOn:
			// do nothing
			break;
		case x86::kPointer:
		case x86::kPointerWeakImport:
		case x86::kAbsolute32:
			{
				if ( isExtern ) {
					// external realocation ==> pointer contains addend
					LittleEndian::set32(*fixUp, ref->getTargetOffset());
				}
				else if ( ((SectionInfo*)inAtom->getSection())->fAllNonLazyPointers ) {
					// if INDIRECT_SYMBOL_LOCAL the content is pointer, else it is zero
					if ( this->indirectSymbolIsLocal(ref) ) 
						LittleEndian::set32(*fixUp, ref->getTarget().getAddress() + ref->getTargetOffset());
					else
						LittleEndian::set32(*fixUp, 0);
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
				if ( kind == x86::kPCRel16 ) {
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
						//fprintf(stderr, "call out of range from %s in %s to %s in %s\n", this->getDisplayName(), this->getFile()->getPath(), target.getDisplayName(), target.getFile()->getPath());
						throwf("rel32 out of range in %s", inAtom->getDisplayName());
					}
					LittleEndian::set32(*fixUp, (int32_t)displacement);
				}
			}
			break;
		case x86::kDtraceProbe:
		case x86::kDtraceTypeReference:
			// nothing to fix up
			break;
	}
}

template <>
void Writer<x86_64>::fixUpReferenceFinal(const ObjectFile::Reference* ref, const ObjectFile::Atom* inAtom, uint8_t buffer[]) const
{
	const int64_t twoGigLimit		  = 0x7FFFFFFF;
	uint64_t* fixUp = (uint64_t*)&buffer[ref->getFixUpOffset()];
	uint8_t*  dtraceProbeSite;
	int64_t displacement = 0;
	switch ( (x86_64::ReferenceKinds)(ref->getKind()) ) {
		case x86_64::kNoFixUp:
		case x86_64::kFollowOn:
			// do nothing
			break;
		case x86_64::kPointerWeakImport:
		case x86_64::kPointer:
			{
				//fprintf(stderr, "fixUpReferenceFinal: %s reference to %s\n", this->getDisplayName(), target.getDisplayName());
				if ( this->relocationNeededInFinalLinkedImage(ref->getTarget()) == kRelocExternal ) {
					// external realocation ==> pointer contains addend
					LittleEndian::set64(*fixUp, ref->getTargetOffset());
				}
				else {
					// internal relocation
					// pointer contains target address
					//printf("Atom::fixUpReferenceFinal) target.name=%s, target.address=0x%08llX\n", target.getDisplayName(), target.getAddress());
					LittleEndian::set64(*fixUp, ref->getTarget().getAddress() + ref->getTargetOffset());
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
					throw "codegen problem, can't use rel32 to external symbol";
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
			}
			if ( (displacement > twoGigLimit) || (displacement < (-twoGigLimit)) ) {
				fprintf(stderr, "call out of range from %s (%llX) in %s to %s (%llX) in %s\n", 
					inAtom->getDisplayName(), inAtom->getAddress(), inAtom->getFile()->getPath(), ref->getTarget().getDisplayName(), ref->getTarget().getAddress(), ref->getTarget().getFile()->getPath());
				throw "rel32 out of range";
			}
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
		case x86_64::kFollowOn:
			// do nothing
			break;
		case x86_64::kPointer:
		case x86_64::kPointerWeakImport:
			{
				if ( external ) {
					// external realocation ==> pointer contains addend
					LittleEndian::set64(*fixUp, ref->getTargetOffset());
				}
				else {
					// internal relocation ==> pointer contains target address
					LittleEndian::set64(*fixUp, ref->getTarget().getAddress() + ref->getTargetOffset());
				}
			}
			break;
		case x86_64::kPointerDiff32:
				// addend in content
				LittleEndian::set32(*((uint32_t*)fixUp), ref->getTargetOffset() - ref->getFromTargetOffset() );
			break;
		case x86_64::kPointerDiff:
				// addend in content
				LittleEndian::set64(*fixUp, ref->getTargetOffset() - ref->getFromTargetOffset() );
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
		case x86_64::kPCRel32GOT:
		case x86_64::kPCRel32GOTLoad:
		case x86_64::kPCRel32GOTWeakImport:
		case x86_64::kPCRel32GOTLoadWeakImport:
			// contains addend (usually zero)
			LittleEndian::set32(*((uint32_t*)fixUp), (uint32_t)(ref->getTargetOffset()));
			break;
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
			// do nothing
			break;
		case A::kPointerWeakImport:
		case A::kPointer:
			{
				//fprintf(stderr, "fixUpReferenceFinal: %s reference to %s\n", this->getDisplayName(), target.getDisplayName());
				if ( finalLinkedImage && ((SectionInfo*)inAtom->getSection())->fAllLazyPointers ) {
					if ( fOptions.prebind() ) {
						switch (ref->getTarget().getDefinitionKind()) {
							case ObjectFile::Atom::kExternalDefinition:
							case ObjectFile::Atom::kExternalWeakDefinition:
								// prebound lazy pointer to another dylib ==> pointer contains zero
								P::setP(*fixUpPointer, 0);
								break;
							case ObjectFile::Atom::kTentativeDefinition:
							case ObjectFile::Atom::kRegularDefinition:
							case ObjectFile::Atom::kWeakDefinition:
								// prebound lazy pointer to withing this dylib ==> pointer contains address
								P::setP(*fixUpPointer, targetAddr);
								break;
							case ObjectFile::Atom::kAbsoluteSymbol:
								break;
						}
					}
					else {
						// lazy-symbol ==> pointer contains address of dyld_stub_binding_helper (stored in "from" target)
						if ( fDyldHelper == NULL )
							throw "symbol dyld_stub_binding_helper not defined (usually in crt1.o/dylib1.o/bundle1.o)";
						P::setP(*fixUpPointer, fDyldHelper->getAddress());
					}
				}
				else if ( !finalLinkedImage && ((SectionInfo*)inAtom->getSection())->fAllNonLazyPointers ) {
					// if INDIRECT_SYMBOL_LOCAL the content is pointer, else it is zero
					if ( this->indirectSymbolIsLocal(ref) ) 
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
						// external realocation ==> pointer contains addend
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
				//fprintf(stderr, "bc fixup %p to %s+0x%08X == 0x%08llX\n", this, ref->getTarget().getDisplayName(), ref->getTargetOffset(), targetAddr);
				int64_t displacement = targetAddr - (inAtom->getAddress() + ref->getFixUpOffset());
				if ( relocateableExternal )  {
					// doing "ld -r" to an external symbol
					// the mach-o way of encoding this is that the bl instruction's target addr is the offset into the target
					displacement -= ref->getTarget().getAddress();
				}
				else {
					const int64_t b_sixtyFourKiloLimit = 0x0000FFFF;
					if ( (displacement > b_sixtyFourKiloLimit) || (displacement < (-b_sixtyFourKiloLimit)) ) {
						//fprintf(stderr, "bl out of range (%lld max is +/-16M) from %s in %s to %s in %s\n", displacement, this->getDisplayName(), this->getFile()->getPath(), target.getDisplayName(), target.getFile()->getPath());
						throwf("bc out of range (%lld max is +/-64K) from %s in %s to %s in %s",
							displacement, inAtom->getDisplayName(), inAtom->getFile()->getPath(),
							ref->getTarget().getDisplayName(), ref->getTarget().getFile()->getPath());
					}
				}
				//fprintf(stderr, "bc fixup displacement=0x%08llX, atom.addr=0x%08llX, atom.offset=0x%08X\n", displacement, inAtom->getAddress(), (uint32_t)ref->getFixUpOffset());
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
bool Writer<ppc>::stubableReference(const ObjectFile::Reference* ref)
{
	uint8_t kind = ref->getKind();
	switch ( (ppc::ReferenceKinds)kind ) {
		case ppc::kNoFixUp:
		case ppc::kFollowOn:
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
					return true;
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
bool Writer<ppc64>::stubableReference(const ObjectFile::Reference* ref)
{
	uint8_t kind = ref->getKind();
	switch ( (ppc64::ReferenceKinds)kind ) {
		case ppc::kNoFixUp:
		case ppc::kFollowOn:
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
bool Writer<x86>::stubableReference(const ObjectFile::Reference* ref)
{
	uint8_t kind = ref->getKind();
	return (kind == x86::kPCRel32 || kind == x86::kPCRel32WeakImport);
}

template <>
bool Writer<x86_64>::stubableReference(const ObjectFile::Reference* ref)
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
			return true;
	}
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
		else if ( !reader->providedExportAtom() && (reader->implicitlyLinked() || fOptions.deadStripDylibs()) ) {
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
	uint32_t newOrdinal = 0;
	for (std::map<uint32_t, ObjectFile::Reader*>::iterator it = ordinalToReader.begin(); it != ordinalToReader.end(); ++it) {
		if ( it->first <= fLibraryToOrdinal.size() )
			fLibraryToOrdinal[it->second] = ++newOrdinal;
	}

	// add aliases (e.g. -lm points to libSystem.dylib)
	for (std::map<ObjectFile::Reader*, ObjectFile::Reader*>::iterator it = readerAliases.begin(); it != readerAliases.end(); ++it) {
		fLibraryToOrdinal[it->first] = fLibraryToOrdinal[it->second];
	}

	// fprintf(stderr, "new ordinals table:\n");
	//for (std::map<class ObjectFile::Reader*, uint32_t>::iterator it = fLibraryToOrdinal.begin(); it != fLibraryToOrdinal.end(); ++it) {
	//	fprintf(stderr, "%u <== %p/%s\n", it->second, it->first, it->first->getPath());
	//}
}


template <>
void Writer<x86_64>::scanForAbsoluteReferences()
{
	// x86_64 codegen never has absolute references
}

template <>
void  Writer<x86>::scanForAbsoluteReferences()
{
	// when linking -pie verify there are no absolute addressing
	if ( fOptions.positionIndependentExecutable() ) {
		for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
			ObjectFile::Atom* atom = *it;
			std::vector<ObjectFile::Reference*>&  references = atom->getReferences();
			for (std::vector<ObjectFile::Reference*>::iterator rit=references.begin(); rit != references.end(); rit++) {
				ObjectFile::Reference* ref = *rit;
				switch (ref->getKind()) {
					case x86::kAbsolute32:
						throwf("cannot link -pie: -mdynamic-no-pic codegen found in %s from %s\n", atom->getDisplayName(), atom->getFile()->getPath());
						return;
				}
			}
		}
	}
}

template <>
void  Writer<ppc>::scanForAbsoluteReferences()
{
	// when linking -pie verify there are no absolute addressing
	if ( fOptions.positionIndependentExecutable() ) {
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
						throwf("cannot link -pie: -mdynamic-no-pic codegen found in %s from %s\n", atom->getDisplayName(), atom->getFile()->getPath());
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
						fPadSegmentInfo = new SegmentInfo();
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
				betterStubs.push_back(new StubAtom<x86>(*this, *((ObjectFile::Atom*)NULL))); //pad with dummy stub
				break;
		}
		betterStubs.push_back(*it);
	}
	// replace 
	fAllSynthesizedStubs.clear();
	fAllSynthesizedStubs.insert(fAllSynthesizedStubs.begin(), betterStubs.begin(), betterStubs.end());
}

template <typename A>
void Writer<A>::synthesizeStubs()
{
	switch ( fOptions.outputKind() ) {
		case Options::kObjectFile:
			// these output kinds never have stubs
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
					}
					// create stubs as needed
					if ( this->stubableReference(ref) 
						&& (ref->getTargetOffset() == 0)
						&& this->relocationNeededInFinalLinkedImage(target) == kRelocExternal ) {
						ObjectFile::Atom* stub = NULL;
						std::map<const ObjectFile::Atom*,ObjectFile::Atom*>::iterator pos = fStubsMap.find(&target);
						if ( pos == fStubsMap.end() ) {
							stub = new StubAtom<A>(*this, target);
							fStubsMap[&target] = stub;
						}
						else {
							stub = pos->second;
						}
						// alter reference to use stub instead
						ref->setTarget(*stub, 0);
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
						if ( useGOT  ) {
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

	// add dummy stubs (x86 only)
	this->insertDummyStubs();

	// sort lazy pointers
	std::sort(fAllSynthesizedLazyPointers.begin(), fAllSynthesizedLazyPointers.end(), AtomByNameSorter());

	// add stubs to fAllAtoms
	if ( fAllSynthesizedStubs.size() != 0 ) {
		std::vector<ObjectFile::Atom*>* stubs = (std::vector<ObjectFile::Atom*>*)&fAllSynthesizedStubs;
		std::vector<ObjectFile::Atom*> mergedStubs;
		if ( fAllSynthesizedStubHelpers.size() != 0 ) {
			// when we have stubs and helpers, insert both into fAllAtoms
			mergedStubs.insert(mergedStubs.end(), fAllSynthesizedStubs.begin(), fAllSynthesizedStubs.end());
			mergedStubs.insert(mergedStubs.end(), fAllSynthesizedStubHelpers.begin(), fAllSynthesizedStubHelpers.end());
			stubs = &mergedStubs;
		}
		ObjectFile::Section* curSection = NULL;
		ObjectFile::Atom* prevAtom = NULL;
		for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
			ObjectFile::Atom* atom = *it;
			ObjectFile::Section* nextSection = atom->getSection();
			if ( nextSection != curSection ) {
				// HACK HACK for i386 where stubs are not in _TEXT segment
				if ( strcmp(fAllSynthesizedStubs[0]->getSegment().getName(), "__IMPORT") == 0 ) {
					if ( ((prevAtom != NULL) && (strcmp(prevAtom->getSegment().getName(), "__IMPORT") == 0))
						|| (strcmp(atom->getSegment().getName(), "__LINKEDIT") == 0) ) {
						// insert stubs at end of __IMPORT segment, or before __LINKEDIT
						fAllAtoms->insert(it, fAllSynthesizedStubs.begin(), fAllSynthesizedStubs.end());
						break;
					}
				}
				else {
					if ( (prevAtom != NULL) && (strcmp(prevAtom->getSectionName(), "__text") == 0) ) {
						// found end of __text section, insert stubs here
						fAllAtoms->insert(it, stubs->begin(), stubs->end());
						break;
					}
				}
				curSection = nextSection;
			}
			prevAtom = atom;
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
				if ( (prevAtom != NULL) && (strcmp(prevAtom->getSectionName(), "__dyld") == 0) ) {
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
	
	// add non-lazy pointers to fAllAtoms
	if ( fAllSynthesizedNonLazyPointers.size() != 0 ) {
		ObjectFile::Section* curSection = NULL;
		ObjectFile::Atom* prevAtom = NULL;
		bool inserted = false;
		for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms->begin(); it != fAllAtoms->end(); it++) {
			ObjectFile::Atom* atom = *it;
			ObjectFile::Section* nextSection = atom->getSection();
			if ( nextSection != curSection ) {
				if ( (prevAtom != NULL) 
					&& ((strcmp(prevAtom->getSectionName(), "__dyld") == 0) 
					|| ((strcmp(prevAtom->getSectionName(), "__data") == 0) &&
						((fOptions.outputKind() == Options::kDyld) || (fOptions.outputKind() == Options::kStaticExecutable))) ) ) {
					// found end of __dyld section, insert lazy pointers here
					fAllAtoms->insert(it, fAllSynthesizedNonLazyPointers.begin(), fAllSynthesizedNonLazyPointers.end());
					inserted = true;
					break;
				}
				curSection = nextSection;
			}
			prevAtom = atom;
		}
		if ( !inserted ) {
			throw "can't insert non-lazy pointers, __dyld section not found";
		}
	}
	
	// build LC_SEGMENT_SPLIT_INFO content now that all atoms exist
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
	}

}


template <typename A>
void Writer<A>::partitionIntoSections()
{
	const bool oneSegmentCommand = (fOptions.outputKind() == Options::kObjectFile);

	// for every atom, set its sectionInfo object and section offset
	// build up fSegmentInfos along the way
	ObjectFile::Section* curSection = NULL;
	SectionInfo* currentSectionInfo = NULL;
	SegmentInfo* currentSegmentInfo = NULL;
	SectionInfo* cstringSectionInfo = NULL;
	unsigned int sectionIndex = 1;
	fSegmentInfos.reserve(8);
	for (unsigned int i=0; i < fAllAtoms->size(); ++i) {
		ObjectFile::Atom* atom = (*fAllAtoms)[i];
		if ( (atom->getSection() != curSection) || ((curSection==NULL) && (strcmp(atom->getSectionName(),currentSectionInfo->fSectionName) != 0)) ) {
			if ( oneSegmentCommand ) {
				if ( currentSegmentInfo == NULL ) {
					currentSegmentInfo = new SegmentInfo();
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
					currentSegmentInfo = new SegmentInfo();
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
			if ( (strcmp(currentSectionInfo->fSegmentName, "__TEXT") == 0) && (strcmp(currentSectionInfo->fSectionName, "._load_commands") == 0) ) {
				fLoadCommandsSection = currentSectionInfo;
				fLoadCommandsSegment = currentSegmentInfo;
			}
			if ( (strcmp(currentSectionInfo->fSegmentName, "__DATA") == 0) && (strcmp(currentSectionInfo->fSectionName, "__la_symbol_ptr") == 0) )
				currentSectionInfo->fAllLazyPointers = true;
			if ( (strcmp(currentSectionInfo->fSegmentName, "__DATA") == 0) && (strcmp(currentSectionInfo->fSectionName, "__la_sym_ptr2") == 0) )
				currentSectionInfo->fAllLazyPointers = true;
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
			if ( (strcmp(currentSectionInfo->fSegmentName, "__IMPORT") == 0) && (strcmp(currentSectionInfo->fSectionName, "__jump_table") == 0) ) {
				currentSectionInfo->fAllSelfModifyingStubs = true;
				currentSectionInfo->fAlignment = 6; // force x86 fast stubs to start on 64-byte boundary
			}
			curSection = atom->getSection();
			if ( currentSectionInfo->fAllNonLazyPointers || currentSectionInfo->fAllLazyPointers 
				|| currentSectionInfo->fAllStubs || currentSectionInfo->fAllStubs ) {
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
// Otherwise, every 15MB into the __TEXT segment is region is
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
		const uint32_t kBetweenRegions = 15*1024*1024; // place regions of islands every 15MB in __text section
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
							int64_t islandRegionAddr = kBetweenRegions * (i+1);
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
						if (log) fprintf(stderr, "using island %s for %s\n", nextTarget->getDisplayName(), atom->getDisplayName());
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
			uint64_t islandRegionAddr = kBetweenRegions;
			uint64_t textSectionAlignment = (1 << textSection->fAlignment);
			int regionIndex = 0;
			uint64_t atomSlide = 0;
			uint64_t sectionOffset = 0;
			for (std::vector<ObjectFile::Atom*>::iterator it=textSection->fAtoms.begin(); it != textSection->fAtoms.end(); it++) {
				ObjectFile::Atom* atom = *it;
				newAtomList.push_back(atom);
				if ( atom->getAddress() > islandRegionAddr ) {
					uint64_t islandStartOffset = atom->getSectionOffset();
					sectionOffset = islandStartOffset + atomSlide;
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
	uint32_t totalSizeOfHeaderAndLoadCommands = 0;
	for(int j=0; j < sectionCount; ++j) {
		SectionInfo* curSection = sectionInfos[j];
		totalSizeOfHeaderAndLoadCommands += curSection->fSize;
		if ( strcmp(curSection->fSectionName, fHeaderPadding->getSectionName()) == 0 )
			break;
	}
	uint64_t paddingSize = 0;
	if ( fOptions.outputKind() == Options::kDyld ) {
		// dyld itself has special padding requirements.  We want the beginning __text section to start at a stable address
		paddingSize = 4096 - (totalSizeOfHeaderAndLoadCommands % 4096);
	}
	else if ( fOptions.outputKind() == Options::kObjectFile ) {
		// mach-o .o files need no padding between load commands and first section
		paddingSize = 0;
	}
	else {
		// work backwards from end of segment and lay out sections so that extra room goes to padding atom
		uint64_t addr = 0;
		for(int j=sectionCount-1; j >=0; --j) {
			SectionInfo* curSection = sectionInfos[j];
			addr -= curSection->fSize;
			addr = addr & (0 - (1 << curSection->fAlignment));
			if ( strcmp(curSection->fSectionName, fHeaderPadding->getSectionName()) == 0 ) {
				addr -= totalSizeOfHeaderAndLoadCommands;
				paddingSize = addr % 4096;
				break;
			}
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
			int extraPages = (minPad - paddingSize + 4095)/4096;
			paddingSize += extraPages * 4096;
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

// assign file offsets and logical address to all segments
template <typename A>
void Writer<A>::assignFileOffsets()
{
	bool finalLinkedImage = (fOptions.outputKind() != Options::kObjectFile);
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

	// Run through the segments and each segment's sections to assign addresses
	for (std::vector<SegmentInfo*>::iterator segit = fSegmentInfos.begin(); segit != fSegmentInfos.end(); ++segit) {
		SegmentInfo* curSegment = *segit;
		
		if ( fOptions.splitSeg() ) {
			if ( curSegment->fInitProtection & VM_PROT_WRITE ) 
				nextContiguousAddress = nextWritableAddress;
			else
				nextContiguousAddress = nextReadOnlyAddress;
		}
		
		fileOffset = (fileOffset+4095) & (-4096);
		curSegment->fFileOffset = fileOffset;
		
		// Set the segment base address
		if ( curSegment->fFixedAddress )
			haveFixedSegments = true;
		else
			curSegment->fBaseAddress = nextContiguousAddress;

		// We've set the segment address, now run through each section.
		uint64_t address = curSegment->fBaseAddress;
		SectionInfo* firstZeroFillSection = NULL;
		SectionInfo* prevSection = NULL;
		
		std::vector<SectionInfo*>& sectionInfos = curSegment->fSections;
		
		for (std::vector<SectionInfo*>::iterator it = sectionInfos.begin(); it != sectionInfos.end(); ++it) {
			SectionInfo* curSection = *it;
		
			// adjust section address based on alignment
			uint64_t alignment = 1 << curSection->fAlignment;
			address    = ( (address+alignment-1) & (-alignment) );
			
			// adjust file offset to match address
			if ( prevSection != NULL ) {
				if ( finalLinkedImage || !prevSection->fVirtualSection )
					fileOffset = (address - prevSection->getBaseAddress()) + prevSection->fFileOffset;
				else
					fileOffset = ( (fileOffset+alignment-1) & (-alignment) );
			}
			
			// update section info
			curSection->fFileOffset = fileOffset;
			curSection->setBaseAddress(address);

			// keep track of trailing zero fill sections
			if ( curSection->fAllZeroFill && (firstZeroFillSection == NULL) )
				firstZeroFillSection = curSection;
			if ( !curSection->fAllZeroFill && (firstZeroFillSection != NULL) && finalLinkedImage ) 
				throwf("zero-fill section %s not at end of segment", curSection->fSectionName);
			
			// update running pointers
			if ( finalLinkedImage || !curSection->fVirtualSection )
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
			curSegment->fFileSize = (curSegment->fFileSize+4095) & (-4096);
			curSegment->fSize	  = (curSegment->fSize+4095) & (-4096);
			if ( !curSegment->fIndependentAddress && (curSegment->fBaseAddress >= nextContiguousAddress) ) {
				nextContiguousAddress = (curSegment->fBaseAddress+curSegment->fSize+4095) & (-4096);
				if ( curSegment->fInitProtection & VM_PROT_WRITE )
					nextWritableAddress = nextContiguousAddress;
				else
					nextReadOnlyAddress = nextContiguousAddress;
			}
		}
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
			if ( fWriter.fHasWeakExports )
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
	switch ( fWriter.fCpuConstraint ) {
		case ObjectFile::Reader::kCpuAny:
			header.set_cpusubtype(CPU_SUBTYPE_POWERPC_ALL);
			break;
		case ObjectFile::Reader::kCpuG3:
			header.set_cpusubtype(CPU_SUBTYPE_POWERPC_750);
			break;
		case ObjectFile::Reader::kCpuG4:
			header.set_cpusubtype(CPU_SUBTYPE_POWERPC_7400);
			break;
		case ObjectFile::Reader::kCpuG5:
			header.set_cpusubtype(CPU_SUBTYPE_POWERPC_970);
			break;
	}
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


template <typename A>
CustomStackAtom<A>::CustomStackAtom(Writer<A>& writer)
 : WriterAtom<A>(writer, Segment::fgStackSegment)
{
	if ( stackGrowsDown() )
		Segment::fgStackSegment.setBaseAddress(writer.fOptions.customStackAddr() - writer.fOptions.customStackSize());
	else
		Segment::fgStackSegment.setBaseAddress(writer.fOptions.customStackAddr());
}


template <>
bool CustomStackAtom<ppc>::stackGrowsDown()
{
	return true;
}

template <>
bool CustomStackAtom<ppc64>::stackGrowsDown()
{
	return true;
}

template <>
bool CustomStackAtom<x86>::stackGrowsDown()
{
	return true;
}

template <>
bool CustomStackAtom<x86_64>::stackGrowsDown()
{
	return true;
}


template <typename A>
void SegmentLoadCommandsAtom<A>::computeSize()
{
	uint64_t size = 0;
	std::vector<SegmentInfo*>& segmentInfos = fWriter.fSegmentInfos;
	const int segCount = segmentInfos.size();
	for(int i=0; i < segCount; ++i) {
		size += sizeof(macho_segment_command<P>);
		std::vector<SectionInfo*>& sectionInfos = segmentInfos[i]->fSections;
		const int sectionCount = sectionInfos.size();
		for(int j=0; j < sectionCount; ++j) {
			if ( fWriter.fEmitVirtualSections || ! sectionInfos[j]->fVirtualSection )
				size += sizeof(macho_section<P>);
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

template <typename A>
void SegmentLoadCommandsAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	uint64_t size = this->getSize();
	const bool oneSegment =( fWriter.fOptions.outputKind() == Options::kObjectFile );
	bzero(buffer, size);
	uint8_t* p = buffer;
	typename std::vector<SegmentInfo*>& segmentInfos = fWriter.fSegmentInfos;
	const int segCount = segmentInfos.size();
	for(int i=0; i < segCount; ++i) {
		SegmentInfo* segInfo = segmentInfos[i];
		const int sectionCount = segInfo->fSections.size();
		macho_segment_command<P>* cmd = (macho_segment_command<P>*)p;
		cmd->set_cmd(macho_segment_command<P>::CMD);
		cmd->set_segname(segInfo->fName);
		cmd->set_vmaddr(segInfo->fBaseAddress);
		cmd->set_vmsize(segInfo->fSize);
		cmd->set_fileoff(segInfo->fFileOffset);
		cmd->set_filesize(segInfo->fFileSize);
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
				else if ( sectInfo->fAllNonLazyPointers ) {
					sect->set_flags(S_NON_LAZY_SYMBOL_POINTERS);
					sect->set_reserved1(sectInfo->fIndirectSymbolOffset);
				}
				else if ( sectInfo->fAllStubs ) {
					sect->set_flags(S_SYMBOL_STUBS | S_ATTR_SOME_INSTRUCTIONS | S_ATTR_PURE_INSTRUCTIONS);
					sect->set_reserved1(sectInfo->fIndirectSymbolOffset);
					sect->set_reserved2(sectInfo->fSize / sectInfo->fAtoms.size());
				}
				else if ( sectInfo->fAllSelfModifyingStubs ) {
					sect->set_flags(S_SYMBOL_STUBS | S_ATTR_SELF_MODIFYING_CODE);
					sect->set_reserved1(sectInfo->fIndirectSymbolOffset);
					sect->set_reserved2(sectInfo->fSize / sectInfo->fAtoms.size());
				}
				else if ( (strcmp(sectInfo->fSectionName, "__mod_init_func") == 0) && (strcmp(sectInfo->fSegmentName, "__DATA") == 0) ) {
					sect->set_flags(S_MOD_INIT_FUNC_POINTERS);
				}
				else if ( (strcmp(sectInfo->fSectionName, "__mod_term_func") == 0) && (strcmp(sectInfo->fSegmentName, "__DATA") == 0) ) {
					sect->set_flags(S_MOD_TERM_FUNC_POINTERS);
				}
				else if ( (strcmp(sectInfo->fSectionName, "__eh_frame") == 0) && (strcmp(sectInfo->fSegmentName, "__TEXT") == 0) ) {
					sect->set_flags(S_COALESCED | S_ATTR_NO_TOC | S_ATTR_STRIP_STATIC_SYMS);
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
				else if ( (strcmp(sectInfo->fSectionName, "__cstring") == 0) && (strcmp(sectInfo->fSegmentName, "__TEXT") == 0) ) {
					sect->set_flags(S_CSTRING_LITERALS);
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
				else if ( (strncmp(sectInfo->fSectionName, "__dof_", 6) == 0) && (strcmp(sectInfo->fSegmentName, "__TEXT") == 0) ) {
					sect->set_flags(S_DTRACE_DOF);
				}
				else if ( (strncmp(sectInfo->fSectionName, "__dof_", 6) == 0) && (strcmp(sectInfo->fSegmentName, "__DATA") == 0) ) {
					sect->set_flags(S_DTRACE_DOF);
				}
				else if ( (strncmp(sectInfo->fSectionName, "__text", 6) == 0) && (strcmp(sectInfo->fSegmentName, "__TEXT") == 0) ) {
					sect->set_flags(S_REGULAR | S_ATTR_SOME_INSTRUCTIONS | S_ATTR_PURE_INSTRUCTIONS);
				}
			}
		}
		p = &p[sizeof(macho_segment_command<P>) + sectionsEmitted*sizeof(macho_section<P>)];
		cmd->set_cmdsize(sizeof(macho_segment_command<P>) + sectionsEmitted*sizeof(macho_section<P>));
		cmd->set_nsects(sectionsEmitted);
	}
}


template <typename A>
SymbolTableLoadCommandsAtom<A>::SymbolTableLoadCommandsAtom(Writer<A>& writer)
 : LoadCommandAtom<A>(writer, Segment::fgTextSegment)
{
	bzero(&fSymbolTable, sizeof(macho_symtab_command<P>));
	bzero(&fDynamicSymbolTable, sizeof(macho_dysymtab_command<P>));
	switch ( fWriter.fOptions.outputKind() ) {
		case Options::kDynamicExecutable:
		case Options::kDynamicLibrary:
		case Options::kDynamicBundle:
		case Options::kDyld:
			fNeedsDynamicSymbolTable = true;
			break;
		case Options::kObjectFile:
		case Options::kStaticExecutable:
			fNeedsDynamicSymbolTable = false;
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
	// build LC_DYSYMTAB command
	macho_symtab_command<P>*   symbolTableCmd = (macho_symtab_command<P>*)buffer;
	bzero(symbolTableCmd, sizeof(macho_symtab_command<P>));
	symbolTableCmd->set_cmd(LC_SYMTAB);
	symbolTableCmd->set_cmdsize(sizeof(macho_symtab_command<P>));
	symbolTableCmd->set_nsyms(fWriter.fSymbolTableCount);
	symbolTableCmd->set_symoff(fWriter.fSymbolTableAtom->getFileOffset());
	symbolTableCmd->set_stroff(fWriter.fStringsAtom->getFileOffset());
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
		dynamicSymbolTableCmd->set_indirectsymoff(fWriter.fIndirectTableAtom->getFileOffset());
		dynamicSymbolTableCmd->set_nindirectsyms(fWriter.fIndirectTableAtom->fTable.size());
		if ( fWriter.fOptions.outputKind() != Options::kObjectFile ) {
			dynamicSymbolTableCmd->set_extreloff((fWriter.fExternalRelocs.size()==0) ? 0 : fWriter.fExternalRelocationsAtom->getFileOffset());
			dynamicSymbolTableCmd->set_nextrel(fWriter.fExternalRelocs.size());
			dynamicSymbolTableCmd->set_locreloff((fWriter.fInternalRelocs.size()==0) ? 0 : fWriter.fLocalRelocationsAtom->getFileOffset());
			dynamicSymbolTableCmd->set_nlocrel(fWriter.fInternalRelocs.size());
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
	if ( fInfo.options.fWeakImport )
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
		if ( it->indirectIndex < indirectTableSize ) {
			A::P::E::set32(indirectTable[it->indirectIndex], it->symbolIndex);
		}
		else {
			throwf("malformed indirect table. size=%d, index=%d", indirectTableSize, it->indirectIndex);
		}
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
	uint16_t numInits = 0;
	uint16_t numTerms = 0;
	std::vector<SegmentInfo*>& segmentInfos = fWriter.fSegmentInfos;
	for (std::vector<SegmentInfo*>::iterator segit = segmentInfos.begin(); segit != segmentInfos.end(); ++segit) {
		if ( strcmp((*segit)->fName, "__DATA") == 0 ) {
			std::vector<SectionInfo*>& sectionInfos = (*segit)->fSections;
			for (std::vector<SectionInfo*>::iterator sectit = sectionInfos.begin(); sectit != sectionInfos.end(); ++sectit) {
				if ( strcmp((*sectit)->fSectionName, "__mod_init_func") == 0 ) 
					numInits = (*sectit)->fSize / sizeof(typename A::P::uint_t);
				else if ( strcmp((*sectit)->fSectionName, "__mod_term_func") == 0 ) 
					numTerms = (*sectit)->fSize / sizeof(typename A::P::uint_t);
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
	module->set_objc_module_info_addr(0);	// Not used by ld_classic, and not used by objc runtime for many years
	module->set_objc_module_info_size(0);	// Not used by ld_classic, and not used by objc runtime for many years
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
	bzero(buffer, size);
	macho_linkedit_data_command<P>* cmd = (macho_linkedit_data_command<P>*)buffer;
	cmd->set_cmd(LC_SEGMENT_SPLIT_INFO);
	cmd->set_cmdsize(size);
	cmd->set_dataoff(fWriter.fSplitCodeToDataContentAtom->getFileOffset());
	cmd->set_datasize(fWriter.fSplitCodeToDataContentAtom->getSize());
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
template <> const char* ObjCInfoAtom<ppc64>::getSectionName()  const { return "__objc_imageinfo"; }
template <> const char* ObjCInfoAtom<x86_64>::getSectionName() const { return "__objc_imageinfo"; }

template <> Segment& ObjCInfoAtom<ppc>::getInfoSegment()    const { return Segment::fgObjCSegment; }
template <> Segment& ObjCInfoAtom<x86>::getInfoSegment()    const { return Segment::fgObjCSegment; }
template <> Segment& ObjCInfoAtom<ppc64>::getInfoSegment()  const { return Segment::fgDataSegment; }
template <> Segment& ObjCInfoAtom<x86_64>::getInfoSegment() const { return Segment::fgDataSegment; }



}; // namespace executable
}; // namespace mach_o


#endif // __EXECUTABLE_MACH_O__
