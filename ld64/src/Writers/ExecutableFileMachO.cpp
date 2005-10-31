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



namespace ExecutableFileMachO {

class Writer : public ExecutableFile::Writer
{ 
public:
								Writer(const char* path, Options& options, std::vector<ExecutableFile::DyLibUsed>& dynamicLibraries);
	virtual						~Writer();

	virtual const char*								getPath();
	virtual std::vector<class ObjectFile::Atom*>&	getAtoms();
	virtual std::vector<class ObjectFile::Atom*>*	getJustInTimeAtomsFor(const char* name);
	virtual std::vector<ObjectFile::StabsInfo>*		getStabsDebugInfo();

	virtual class ObjectFile::Atom*					getUndefinedProxyAtom(const char* name);
	virtual void									write(std::vector<class ObjectFile::Atom*>& atoms, class ObjectFile::Atom* entryPointAtom);
	
private:
	void						assignFileOffsets();
	void						partitionIntoSections();
	bool						addBranchIslands();
	void						adjustLoadCommandsAndPadding();
	void						createDynamicLinkerCommand();
	void						createDylibCommands();
	void						buildLinkEdit();
	void						writeAtoms();
	void						collectExportedAndImportedAndLocalAtoms();
	void						setNlistRange(std::vector<class ObjectFile::Atom*>& atoms, uint32_t startIndex, uint32_t count);
	void						buildSymbolTable();
	void						setExportNlist(const ObjectFile::Atom* atom, macho_nlist* entry);
	void						setImportNlist(const ObjectFile::Atom* atom, macho_nlist* entry);
	void						setLocalNlist(const ObjectFile::Atom* atom, macho_nlist* entry);
	uint64_t					getAtomLoadAddress(const ObjectFile::Atom* atom);
	uint8_t						ordinalForLibrary(ObjectFile::Reader* file);
	bool						shouldExport(ObjectFile::Atom& atom);
	void						buildFixups();
	void						adjustLinkEditSections();
	void						buildObjectFileFixups();
	void						buildExecutableFixups();
	uint32_t					symbolIndex(ObjectFile::Atom& atom);
	uint32_t					addRelocs(ObjectFile::Atom* atom, ObjectFile::Reference* ref);
	unsigned int				collectStabs();
	macho_uintptr_t				valueForStab(const ObjectFile::StabsInfo& stab, const ObjectFile::Atom* atom);
	void						addStabs(uint32_t startIndex, uint32_t count);

	
	class SectionInfo : public ObjectFile::Section {
	public:
											SectionInfo();
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
		bool								fAllZeroFill;
		bool								fVirtualSection;
	};
	
	class SegmentInfo 
	{
	public:
											SegmentInfo();
		std::vector<class SectionInfo*>		fSections;
		char								fName[20];
		uint32_t							fInitProtection;
		uint32_t							fMaxProtection;
		uint64_t							fFileOffset;
		uint64_t							fFileSize;
		uint64_t							fBaseAddress;
		uint64_t							fSize;
		bool								fFixedAddress;
	};

	
	struct DirectLibrary {
		class ObjectFile::Reader*		fLibrary;
		bool						fWeak;
		bool						fReExport;	
	};
	
	struct IndirectEntry {
		uint32_t	indirectIndex;	
		uint32_t	symbolIndex;
	};

	struct StabChunks {
		ObjectFile::Atom*					fAtom;
		ObjectFile::Reader*					fReader;
		unsigned int						fReaderOrder;
		unsigned int						fOrderInReader;
		std::vector<ObjectFile::StabsInfo>*	fStabs;
	};

	static bool						stabChunkCompare(const StabChunks& lhs, const StabChunks& rhs);

	friend class WriterAtom;
	friend class PageZeroAtom;
	friend class CustomStackAtom;
	friend class MachHeaderAtom;
	friend class SegmentLoadCommandsAtom;
	friend class SymbolTableLoadCommandsAtom;
	friend class ThreadsLoadCommandsAtom;
	friend class DylibIDLoadCommandsAtom;
	friend class RoutinesLoadCommandsAtom;
	friend class DyldLoadCommandsAtom;
	friend class LinkEditAtom;
	friend class LocalRelocationsLinkEditAtom;
	friend class ExternalRelocationsLinkEditAtom;
	friend class SymbolTableLinkEditAtom;
	friend class IndirectTableLinkEditAtom;
	friend class StringsLinkEditAtom;

	const char*										fFilePath;
	Options&										fOptions;
	int												fFileDescriptor;
	std::vector<class ObjectFile::Atom*>*			fAllAtoms;
	class SectionInfo*								fLoadCommandsSection;
	class SegmentInfo*								fLoadCommandsSegment;
	class SegmentLoadCommandsAtom*					fSegmentCommands;
	class SymbolTableLoadCommandsAtom*				fSymbolTableCommands;
	class LoadCommandsPaddingAtom*					fHeaderPadding;
	std::vector<class ObjectFile::Atom*>			fWriterSynthesizedAtoms;
	std::vector<SegmentInfo*>						fSegmentInfos;
	class ObjectFile::Atom*							fEntryPoint;
	std::vector<DirectLibrary>						fDirectLibraries;
	std::map<class ObjectFile::Reader*, uint32_t>	fLibraryToOrdinal;
	std::vector<StabChunks>							fStabChunks;
	std::vector<class ObjectFile::Atom*>			fExportedAtoms;
	std::vector<class ObjectFile::Atom*>			fImportedAtoms;
	std::vector<class ObjectFile::Atom*>			fLocalSymbolAtoms;	
	LocalRelocationsLinkEditAtom*					fLocalRelocationsAtom;
	ExternalRelocationsLinkEditAtom*				fExternalRelocationsAtom;
	SymbolTableLinkEditAtom*						fSymbolTableAtom;
	IndirectTableLinkEditAtom*						fIndirectTableAtom;
	StringsLinkEditAtom*							fStringsAtom;
	macho_nlist*									fSymbolTable;
	//char*											fStringPool;
	//uint32_t										fStringPoolUsed;
	//uint32_t										fStringPoolSize;
	std::vector<macho_relocation_info>				fInternalRelocs;
	std::vector<macho_relocation_info>				fExternalRelocs;
	std::vector<IndirectEntry>						fIndirectSymbolTable;
	uint32_t										fSymbolTableCount;			
	uint32_t										fSymbolTableStabsCount;			
	uint32_t										fSymbolTableStabsStartIndex;			
	uint32_t										fSymbolTableLocalCount;			
	uint32_t										fSymbolTableLocalStartIndex;			
	uint32_t										fSymbolTableExportCount;			
	uint32_t										fSymbolTableExportStartIndex;			
	uint32_t										fSymbolTableImportCount;			
	uint32_t										fSymbolTableImportStartIndex;	
	bool											fEmitVirtualSections;
	bool											fHasWeakExports;
	bool											fReferencesWeakImports;
};


class WriterAtom : public ObjectFile::Atom 
{
protected:
	class Segment;
public:
	enum Kind { zeropage, machHeaderApp, machHeaderDylib, machHeaderBundle, machHeaderObject, loadCommands, undefinedProxy };
											WriterAtom(Writer& writer, class WriterAtom::Segment& segment) : fWriter(writer), fSegment(segment) {}
											
	virtual ObjectFile::Reader*				getFile() const					{ return &fWriter; }
	virtual const char*						getName() const					{ return NULL; }
	virtual const char*						getDisplayName() const			{ return this->getName(); }
	virtual Scope							getScope() const				{ return ObjectFile::Atom::scopeTranslationUnit; }
	virtual bool							isTentativeDefinition() const	{ return false; }
	virtual bool							isWeakDefinition() const		{ return false; }
	virtual bool							isCoalesableByName() const		{ return false; }
	virtual bool							isCoalesableByValue() const		{ return false; }
	virtual bool							isZeroFill() const				{ return false; }
	virtual bool							dontDeadStrip() const			{ return true; }
	virtual bool							dontStripName() const			{ return false; }
	virtual bool							isImportProxy() const			{ return false; }
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const		{ return fgEmptyReferenceList; }
	virtual bool							mustRemainInSection() const		{ return true; }
	virtual ObjectFile::Segment&			getSegment() const				{ return fSegment; }
	virtual bool							requiresFollowOnAtom() const	{ return false; } 
	virtual ObjectFile::Atom&				getFollowOnAtom() const			{ return *((ObjectFile::Atom*)NULL); }
	virtual std::vector<ObjectFile::StabsInfo>*	getStabsDebugInfo() const	{ return NULL; }
	virtual uint8_t							getAlignment() const			{ return 2; }
	virtual WeakImportSetting				getImportWeakness() const		{ return Atom::kWeakUnset; }
	virtual void							copyRawContent(uint8_t buffer[]) const { throw "don't use copyRawContent"; }
	virtual void							setScope(Scope)					{ }
	virtual void							setImportWeakness(bool weakImport) { }


protected:
	virtual									~WriterAtom() {}
	
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
	private:
		const char*					fName;
		const bool					fReadable;
		const bool					fWritable;
		const bool					fExecutable;
		const bool					fFixedAddress;
	};
	
	static std::vector<ObjectFile::Reference*>	fgEmptyReferenceList;
	static Segment								fgTextSegment;
	static Segment								fgPageZeroSegment;
	static Segment								fgLinkEditSegment;
	static Segment								fgStackSegment;
	
	
	Writer&									fWriter;
	Segment&								fSegment;
};


WriterAtom::Segment						WriterAtom::fgPageZeroSegment("__PAGEZERO", false, false, false, true);
WriterAtom::Segment						WriterAtom::fgTextSegment("__TEXT", true, false, true, false);
WriterAtom::Segment						WriterAtom::fgLinkEditSegment("__LINKEDIT", true, false, false, false);
WriterAtom::Segment						WriterAtom::fgStackSegment("__UNIXSTACK", true, true, false, true);
std::vector<ObjectFile::Reference*>		WriterAtom::fgEmptyReferenceList;

class PageZeroAtom : public WriterAtom 
{
public:
											PageZeroAtom(Writer& writer) : WriterAtom(writer, fgPageZeroSegment) {}
	virtual const char*						getDisplayName() const	{ return "page zero content"; }
	virtual bool							isZeroFill() const		{ return true; }
	virtual uint64_t						getSize() const			{ return fWriter.fOptions.zeroPageSize(); }
	virtual const char*						getSectionName() const	{ return "._zeropage"; }
	virtual uint8_t							getAlignment() const	{ return 12; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const {}
};

class MachHeaderAtom : public WriterAtom 
{
public:
											MachHeaderAtom(Writer& writer)  : WriterAtom(writer, fgTextSegment) {}
	virtual const char*						getName() const;
	virtual const char*						getDisplayName() const;
	virtual Scope							getScope() const;
	virtual bool							dontStripName() const;
	virtual uint64_t						getSize() const;
	virtual uint8_t							getAlignment() const		{ return 12; }
	virtual const char*						getSectionName() const		{ return "._mach_header"; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;
};

class CustomStackAtom : public WriterAtom 
{
public:
											CustomStackAtom(Writer& writer);
	virtual const char*						getDisplayName() const	{ return "custom stack content"; }
	virtual bool							isZeroFill() const		{ return true; }
	virtual uint64_t						getSize() const			{ return fWriter.fOptions.customStackSize(); }
	virtual const char*						getSectionName() const	{ return "._stack"; }
	virtual uint8_t							getAlignment() const	{ return 12; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const {}
};

class SegmentLoadCommandsAtom : public WriterAtom 
{
public:
											SegmentLoadCommandsAtom(Writer& writer)  : WriterAtom(writer, fgTextSegment), fCommandCount(0), fSize(0) { writer.fSegmentCommands = this; }
	virtual const char*						getDisplayName() const	{ return "segment load commands"; }
	virtual uint64_t						getSize() const			{ return fSize; }
	virtual uint8_t							getAlignment() const	{ return 2; }
	virtual const char*						getSectionName() const	{ return "._load_commands"; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;
	
	void									computeSize();
	void									setup();
	unsigned int							commandCount()			{ return fCommandCount; }
	void									assignFileOffsets();
private:
	unsigned int							fCommandCount;
	uint32_t								fSize;
};

class SymbolTableLoadCommandsAtom : public WriterAtom 
{
public:
											SymbolTableLoadCommandsAtom(Writer&);
	virtual const char*						getDisplayName() const { return "symbol table load commands"; }
	virtual uint64_t						getSize() const;
	virtual uint8_t							getAlignment() const	{ return 2; }
	virtual const char*						getSectionName() const	{ return "._load_commands"; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;
	
private:
	macho_symtab_command					fSymbolTable;
	macho_dysymtab_command					fDynamicSymbolTable;
};

class ThreadsLoadCommandsAtom : public WriterAtom 
{
public:
											ThreadsLoadCommandsAtom(Writer& writer) : WriterAtom(writer, fgTextSegment) {}
	virtual const char*						getDisplayName() const { return "thread load commands"; }
	virtual uint64_t						getSize() const;
	virtual uint8_t							getAlignment() const	{ return 2; }
	virtual const char*						getSectionName() const	{ return "._load_commands"; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;
private:
	uint8_t*								fBuffer;
	uint32_t								fBufferSize;
};

class DyldLoadCommandsAtom : public WriterAtom 
{
public:
											DyldLoadCommandsAtom(Writer& writer)  : WriterAtom(writer, fgTextSegment) {}
	virtual const char*						getDisplayName() const	{ return "dyld load command"; }
	virtual uint64_t						getSize() const;
	virtual uint8_t							getAlignment() const	{ return 2; }
	virtual const char*						getSectionName() const	{ return "._load_commands"; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;
};

class DylibLoadCommandsAtom : public WriterAtom 
{
public:
											DylibLoadCommandsAtom(Writer& writer, ExecutableFile::DyLibUsed& info)  : WriterAtom(writer, fgTextSegment), fInfo(info) {}
	virtual const char*						getDisplayName() const	{ return "dylib load command"; }
	virtual uint64_t						getSize() const;
	virtual uint8_t							getAlignment() const	{ return 2; }
	virtual const char*						getSectionName() const	{ return "._load_commands"; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;
private:
	ExecutableFile::DyLibUsed&				fInfo;
};

class DylibIDLoadCommandsAtom : public WriterAtom 
{
public:
											DylibIDLoadCommandsAtom(Writer& writer) : WriterAtom(writer, fgTextSegment) {}
	virtual const char*						getDisplayName() const { return "dylib ID load command"; }
	virtual uint64_t						getSize() const;
	virtual uint8_t							getAlignment() const	{ return 2; }
	virtual const char*						getSectionName() const	{ return "._load_commands"; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;
};

class RoutinesLoadCommandsAtom : public WriterAtom 
{
public:
											RoutinesLoadCommandsAtom(Writer& writer) : WriterAtom(writer, fgTextSegment) {}
	virtual const char*						getDisplayName() const { return "routines load command"; }
	virtual uint64_t						getSize() const;
	virtual uint8_t							getAlignment() const	{ return 2; }
	virtual const char*						getSectionName() const	{ return "._load_commands"; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;
};

class SubUmbrellaLoadCommandsAtom : public WriterAtom 
{
public:
											SubUmbrellaLoadCommandsAtom(Writer& writer, const char* name)  : WriterAtom(writer, fgTextSegment), fName(name) {}
	virtual const char*						getDisplayName() const	{ return "sub-umbrella load command"; }
	virtual uint64_t						getSize() const;
	virtual uint8_t							getAlignment() const	{ return 2; }
	virtual const char*						getSectionName() const	{ return "._load_commands"; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;
private:
	const char*								fName;
};

class SubLibraryLoadCommandsAtom : public WriterAtom 
{
public:
											SubLibraryLoadCommandsAtom(Writer& writer,  const char* nameStart, int nameLen)  
													: WriterAtom(writer, fgTextSegment), fNameStart(nameStart), fNameLength(nameLen) {}
	virtual const char*						getDisplayName() const	{ return "sub-library load command"; }
	virtual uint64_t						getSize() const;
	virtual uint8_t							getAlignment() const	{ return 2; }
	virtual const char*						getSectionName() const	{ return "._load_commands"; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;
private:
	const char*								fNameStart;
	int										fNameLength;
};

class UmbrellaLoadCommandsAtom : public WriterAtom 
{
public:
											UmbrellaLoadCommandsAtom(Writer& writer, const char* name)  
													: WriterAtom(writer, fgTextSegment), fName(name) {}
	virtual const char*						getDisplayName() const	{ return "umbrella load command"; }
	virtual uint64_t						getSize() const;
	virtual uint8_t							getAlignment() const	{ return 2; }
	virtual const char*						getSectionName() const	{ return "._load_commands"; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;
private:
	const char*								fName;
};

class LoadCommandsPaddingAtom : public WriterAtom 
{
public:
											LoadCommandsPaddingAtom(Writer& writer)  
													: WriterAtom(writer, fgTextSegment), fSize(0) {}
	virtual const char*						getDisplayName() const	{ return "header padding"; }
	virtual uint64_t						getSize() const;
	virtual uint8_t							getAlignment() const	{ return 2; }
	virtual const char*						getSectionName() const	{ return "._load_cmds_pad"; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;
	
	void									setSize(uint64_t newSize) { fSize = newSize; }
private:
	uint64_t								fSize;
};

class LinkEditAtom : public WriterAtom 
{
public:
											LinkEditAtom(Writer& writer) : WriterAtom(writer, fgLinkEditSegment) {}
	uint64_t								getFileOffset() const;
};

class LocalRelocationsLinkEditAtom : public LinkEditAtom 
{
public:
											LocalRelocationsLinkEditAtom(Writer& writer) : LinkEditAtom(writer) { }
	virtual const char*						getDisplayName() const	{ return "local relocations"; }
	virtual uint64_t						getSize() const;
	virtual uint8_t							getAlignment() const	{ return 3; }
	virtual const char*						getSectionName() const	{ return "._local_relocs"; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;
};

class SymbolTableLinkEditAtom : public LinkEditAtom 
{
public:
											SymbolTableLinkEditAtom(Writer& writer) : LinkEditAtom(writer) { }
	virtual const char*						getDisplayName() const	{ return "symbol table"; }
	virtual uint64_t						getSize() const;
	virtual uint8_t							getAlignment() const	{ return 2; }
	virtual const char*						getSectionName() const	{ return "._symbol_table"; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;
};

class ExternalRelocationsLinkEditAtom : public LinkEditAtom 
{
public:
											ExternalRelocationsLinkEditAtom(Writer& writer) : LinkEditAtom(writer) { }
	virtual const char*						getDisplayName() const	{ return "external relocations"; }
	virtual uint64_t						getSize() const;
	virtual uint8_t							getAlignment() const	{ return 3; }
	virtual const char*						getSectionName() const	{ return "._extern_relocs"; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;
};

class IndirectTableLinkEditAtom : public LinkEditAtom 
{
public:
											IndirectTableLinkEditAtom(Writer& writer) : LinkEditAtom(writer) { }
	virtual const char*						getDisplayName() const	{ return "indirect symbol table"; }
	virtual uint64_t						getSize() const;
	virtual uint8_t							getAlignment() const	{ return 2; }
	virtual const char*						getSectionName() const	{ return "._indirect_syms"; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;
};

class StringsLinkEditAtom : public LinkEditAtom 
{
public:
											StringsLinkEditAtom(Writer& writer);
	virtual const char*						getDisplayName() const	{ return "string pool"; }
	virtual uint64_t						getSize() const;
	virtual uint8_t							getAlignment() const	{ return 2; }
	virtual const char*						getSectionName() const	{ return "._string_pool"; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;
	
	int32_t									add(const char* name);
	int32_t									emptyString();
	
private:
	enum { kBufferSize = 0x01000000 };

	std::vector<char*>						fFullBuffers;
	char*									fCurrentBuffer;
	uint32_t								fCurrentBufferUsed;
};



class UndefinedSymbolProxyAtom : public WriterAtom 
{
public:
											UndefinedSymbolProxyAtom(Writer& writer, const char* name) : WriterAtom(writer, fgLinkEditSegment), fName(name), fWeakImportSetting(Atom::kWeakUnset) {}
	virtual const char*						getName() const				{ return fName; }
	virtual Scope							getScope() const			{ return ObjectFile::Atom::scopeGlobal; }
	virtual uint64_t						getSize() const				{ return 0; }
	virtual bool							isWeakDefinition() const	{ return true; }
	virtual bool							isImportProxy() const		{ return true; }
	virtual const char*						getSectionName() const		{ return "._imports"; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const {}
	virtual WeakImportSetting				getImportWeakness() const	{ return fWeakImportSetting; }
	virtual void							setImportWeakness(bool weakImport) { fWeakImportSetting = weakImport ? kWeakImport : kNonWeakImport; }
private:
	const char*								fName;
	WeakImportSetting						fWeakImportSetting;
};

#if defined(ARCH_PPC) || defined(ARCH_PPC64) 
class BranchIslandAtom : public WriterAtom 
{
public:
											BranchIslandAtom(Writer& writer, const char* name, int islandRegion, ObjectFile::Atom& target, uint32_t targetOffset);
	virtual const char*						getName() const				{ return fName; }
	virtual Scope							getScope() const			{ return ObjectFile::Atom::scopeLinkageUnit; }
	virtual uint64_t						getSize() const				{ return 4; }
	virtual const char*						getSectionName() const		{ return "__text"; }
	virtual void							writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;
private:
	const char*								fName;
	ObjectFile::Atom&						fTarget;
	uint32_t								fTargetOffset;
};
#endif

struct ExportSorter
{
     bool operator()(ObjectFile::Atom* left, ObjectFile::Atom* right)
     {
          return (strcmp(left->getName(), right->getName()) < 0);
     }
};


ExecutableFile::Writer* MakeWriter(const char* path, Options& options, std::vector<ExecutableFile::DyLibUsed>& dynamicLibraries)
{
	return new Writer(path, options, dynamicLibraries);
}

Writer::SectionInfo::SectionInfo()
 : fFileOffset(0), fSize(0), fRelocCount(0), fRelocOffset(0), fIndirectSymbolOffset(0), fAlignment(0),
	fAllLazyPointers(false), fAllNonLazyPointers(false), fAllZeroFill(false), fVirtualSection(false)
{
	fSegmentName[0] = '\0';
	fSectionName[0] = '\0';
}
	
Writer::SegmentInfo::SegmentInfo()
 : fInitProtection(0), fMaxProtection(0), fFileOffset(0), fFileSize(0), fBaseAddress(0), fSize(0), fFixedAddress(false)
{
	fName[0] = '\0';
}


Writer::Writer(const char* path, Options& options, std::vector<ExecutableFile::DyLibUsed>& dynamicLibraries)
 : ExecutableFile::Writer(dynamicLibraries), fFilePath(strdup(path)), fOptions(options), fLoadCommandsSection(NULL), 
	fLoadCommandsSegment(NULL),
	//fStringPool(NULL), fStringPoolUsed(0), fStringPoolSize(0), 
	fEmitVirtualSections(false), fHasWeakExports(false), fReferencesWeakImports(false)
{
	int permissions = 0777;
	if ( fOptions.outputKind() == Options::kObjectFile )
		permissions = 0666;
	// Calling unlink first assures the file is gone so that open creates it with correct permissions
	// It also handles the case where fFilePath file is not writeable but its directory is
	// And it means we don't have to truncate the file when done writing (in case new is smaller than old)
	(void)unlink(fFilePath);
	fFileDescriptor = open(fFilePath, O_CREAT | O_WRONLY | O_TRUNC, permissions);	
	if ( fFileDescriptor == -1 ) {
		throw "can't open file for writing";
	}

	switch ( fOptions.outputKind() ) {
		case Options::kDynamicExecutable:
		case Options::kStaticExecutable:
			fWriterSynthesizedAtoms.push_back(new PageZeroAtom(*this));
			fWriterSynthesizedAtoms.push_back(new MachHeaderAtom(*this));
			fWriterSynthesizedAtoms.push_back(new SegmentLoadCommandsAtom(*this));
			fWriterSynthesizedAtoms.push_back(new SymbolTableLoadCommandsAtom(*this));
			if ( fOptions.outputKind() == Options::kDynamicExecutable )
				fWriterSynthesizedAtoms.push_back(new DyldLoadCommandsAtom(*this));
			fWriterSynthesizedAtoms.push_back(new ThreadsLoadCommandsAtom(*this));
			if ( fOptions.hasCustomStack() )
				fWriterSynthesizedAtoms.push_back(new CustomStackAtom(*this));
			fWriterSynthesizedAtoms.push_back(fHeaderPadding = new LoadCommandsPaddingAtom(*this));
			fWriterSynthesizedAtoms.push_back(fLocalRelocationsAtom = new LocalRelocationsLinkEditAtom(*this));
			fWriterSynthesizedAtoms.push_back(fSymbolTableAtom = new SymbolTableLinkEditAtom(*this));
			fWriterSynthesizedAtoms.push_back(fExternalRelocationsAtom = new ExternalRelocationsLinkEditAtom(*this));
			fWriterSynthesizedAtoms.push_back(fIndirectTableAtom = new IndirectTableLinkEditAtom(*this));
			fWriterSynthesizedAtoms.push_back(fStringsAtom = new StringsLinkEditAtom(*this));
			break;
		case Options::kDynamicLibrary:
		case Options::kDynamicBundle:
		case Options::kObjectFile:
			fWriterSynthesizedAtoms.push_back(new MachHeaderAtom(*this));
			fWriterSynthesizedAtoms.push_back(new SegmentLoadCommandsAtom(*this));
			if ( fOptions.outputKind() == Options::kDynamicLibrary ) {
				fWriterSynthesizedAtoms.push_back(new DylibIDLoadCommandsAtom(*this));
				if ( fOptions.initFunctionName() != NULL )
					fWriterSynthesizedAtoms.push_back(new RoutinesLoadCommandsAtom(*this));
			}
			fWriterSynthesizedAtoms.push_back(new SymbolTableLoadCommandsAtom(*this));
			fWriterSynthesizedAtoms.push_back(fHeaderPadding = new LoadCommandsPaddingAtom(*this));
			fWriterSynthesizedAtoms.push_back(fLocalRelocationsAtom = new LocalRelocationsLinkEditAtom(*this));
			fWriterSynthesizedAtoms.push_back(fSymbolTableAtom = new SymbolTableLinkEditAtom(*this));
			fWriterSynthesizedAtoms.push_back(fExternalRelocationsAtom = new ExternalRelocationsLinkEditAtom(*this));
			fWriterSynthesizedAtoms.push_back(fIndirectTableAtom = new IndirectTableLinkEditAtom(*this));
			fWriterSynthesizedAtoms.push_back(fStringsAtom = new StringsLinkEditAtom(*this));
			break;
		case Options::kDyld:
			fWriterSynthesizedAtoms.push_back(new MachHeaderAtom(*this));
			fWriterSynthesizedAtoms.push_back(new SegmentLoadCommandsAtom(*this));
			fWriterSynthesizedAtoms.push_back(new SymbolTableLoadCommandsAtom(*this));
			fWriterSynthesizedAtoms.push_back(new DyldLoadCommandsAtom(*this));
			fWriterSynthesizedAtoms.push_back(new ThreadsLoadCommandsAtom(*this));
			fWriterSynthesizedAtoms.push_back(fHeaderPadding = new LoadCommandsPaddingAtom(*this));
			fWriterSynthesizedAtoms.push_back(fLocalRelocationsAtom = new LocalRelocationsLinkEditAtom(*this));
			fWriterSynthesizedAtoms.push_back(fSymbolTableAtom = new SymbolTableLinkEditAtom(*this));
			fWriterSynthesizedAtoms.push_back(fExternalRelocationsAtom = new ExternalRelocationsLinkEditAtom(*this));
			fWriterSynthesizedAtoms.push_back(fIndirectTableAtom = new IndirectTableLinkEditAtom(*this));
			fWriterSynthesizedAtoms.push_back(fStringsAtom = new StringsLinkEditAtom(*this));
			break;
	}
	
	// add extra commmands
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
					if ( dylibInfo.indirect ) {
						// find ordinal of direct reader
						if ( fOptions.nameSpace() == Options::kTwoLevelNameSpace ) {
							bool found = false;
							for (std::map<class ObjectFile::Reader*, uint32_t>::iterator it = fLibraryToOrdinal.begin(); it != fLibraryToOrdinal.end(); ++it) {
								if ( it->first == dylibInfo.directReader ) {
									//fprintf(stderr, "ordinal %d for indirect %s\n", it->second, dylibInfo.reader->getPath());
									fLibraryToOrdinal[dylibInfo.reader] = it->second;
									found = true;
									break;
								}
							}
							if ( ! found ) 
								fprintf(stderr, "ld64 warning: ordinal not found for %s, parent %s\n", dylibInfo.reader->getPath(), dylibInfo.directReader != NULL ? dylibInfo.directReader->getPath() : NULL);
						}
					}
					else {
						// see if a DylibLoadCommandsAtom has already been created for this install path
						bool newDylib = true;
						const char* dylibInstallPath = dylibInfo.reader->getInstallPath();
						if ( dylibInfo.options.fInstallPathOverride != NULL ) 
							dylibInstallPath = dylibInfo.options.fInstallPathOverride;
						for (unsigned int seenLib=0; seenLib < i; ++seenLib) {
							ExecutableFile::DyLibUsed& seenDylibInfo = dynamicLibraries[seenLib];
							if ( !seenDylibInfo.indirect ) {
								const char* seenDylibInstallPath = seenDylibInfo.reader->getInstallPath();
								if ( seenDylibInfo.options.fInstallPathOverride != NULL ) 
									seenDylibInstallPath = dylibInfo.options.fInstallPathOverride;
								if ( strcmp(seenDylibInstallPath, dylibInstallPath) == 0 ) {
									fLibraryToOrdinal[dylibInfo.reader] = fLibraryToOrdinal[seenDylibInfo.reader];
									newDylib = false;
									break;
								}
							}
						}
						
						if ( newDylib ) {
							// assign new ordinal and check for other paired load commands
							fLibraryToOrdinal[dylibInfo.reader] = ordinal++;
							fWriterSynthesizedAtoms.push_back(new DylibLoadCommandsAtom(*this, dylibInfo));
							if ( dylibInfo.options.fReExport ) {
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
									fWriterSynthesizedAtoms.push_back(new SubUmbrellaLoadCommandsAtom(*this, &lastSlash[1]));
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
									fWriterSynthesizedAtoms.push_back(new SubLibraryLoadCommandsAtom(*this, nameStart, len));
								}
							}
						}
					}
				}
				// add umbrella command if needed
				if ( fOptions.umbrellaName() != NULL ) {
					fWriterSynthesizedAtoms.push_back(new UmbrellaLoadCommandsAtom(*this, fOptions.umbrellaName()));
				}
			}
			break;
		case Options::kStaticExecutable:
		case Options::kObjectFile:
		case Options::kDyld:
			break;
	}
	
	//fprintf(stderr, "ordinals table:\n");
	//for (std::map<class ObjectFile::Reader*, uint32_t>::iterator it = fLibraryToOrdinal.begin(); it != fLibraryToOrdinal.end(); ++it) {
	//	fprintf(stderr, "%d <== %s\n", it->second, it->first->getPath());
	//}
}

Writer::~Writer()
{
	if ( fFilePath != NULL )
		free((void*)fFilePath);
	if ( fSymbolTable != NULL )
		delete [] fSymbolTable;
	//if ( fStringPool != NULL )
	//	delete [] fStringPool;
}

const char*	 Writer::getPath()
{
	return fFilePath;
}


std::vector<class ObjectFile::Atom*>& Writer::getAtoms()
{
	return fWriterSynthesizedAtoms;
}

std::vector<class ObjectFile::Atom*>* Writer::getJustInTimeAtomsFor(const char* name)
{
	return NULL;
}

std::vector<ObjectFile::StabsInfo>*	Writer::getStabsDebugInfo()
{
	return NULL;
}

ObjectFile::Atom* Writer::getUndefinedProxyAtom(const char* name)
{
	if ( (fOptions.outputKind() == Options::kObjectFile)
		|| (fOptions.undefinedTreatment() != Options::kUndefinedError) )
		return new UndefinedSymbolProxyAtom(*this, name);
	else
		return NULL;
}

uint8_t Writer::ordinalForLibrary(ObjectFile::Reader* lib)
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

 
void Writer::write(std::vector<class ObjectFile::Atom*>& atoms, class ObjectFile::Atom* entryPointAtom)
{
	fAllAtoms =  &atoms;
	fEntryPoint = entryPointAtom;

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
	
	// write everything
	writeAtoms();
}

void Writer::buildLinkEdit()
{
	this->collectExportedAndImportedAndLocalAtoms();
	this->buildSymbolTable();
	this->buildFixups();
	this->adjustLinkEditSections();
}



uint64_t Writer::getAtomLoadAddress(const ObjectFile::Atom* atom)
{
	return atom->getAddress();
//	SectionInfo* info = (SectionInfo*)atom->getSection();
//	return info->getBaseAddress() + atom->getSectionOffset();
}

void Writer::setExportNlist(const ObjectFile::Atom* atom, macho_nlist* entry)
{
	// set n_type
	entry->set_n_type(N_EXT | N_SECT);
	if ( (atom->getScope() == ObjectFile::Atom::scopeLinkageUnit) && fOptions.keepPrivateExterns() && (fOptions.outputKind() == Options::kObjectFile) )
		entry->set_n_type(N_EXT | N_SECT | N_PEXT);
	
	// set n_sect (section number of implementation )
	uint8_t sectionIndex = atom->getSection()->getIndex();
	entry->set_n_sect(sectionIndex);
	
	// the __mh_execute_header is magic and must be an absolute symbol  
	if ( (fOptions.outputKind() == Options::kDynamicExecutable) && (sectionIndex==0) && atom->dontStripName())
		entry->set_n_type(N_EXT | N_ABS);

	// set n_desc
	uint16_t desc = 0;
	if ( atom->dontStripName() )
		desc |= REFERENCED_DYNAMICALLY;
	if ( atom->isWeakDefinition() && (strcmp(atom->getSectionName(), "__common") != 0) ) {
		desc |= N_WEAK_DEF;
		fHasWeakExports = true;
	}
	entry->set_n_desc(desc);
	
	// set n_value ( address this symbol will be at if this executable is loaded at it preferred address )
	entry->set_n_value(this->getAtomLoadAddress(atom));
}

void Writer::setImportNlist(const ObjectFile::Atom* atom, macho_nlist* entry)
{
	// set n_type
	entry->set_n_type(N_UNDF | N_EXT);
	
	// set n_sect
	entry->set_n_sect(0);
	
	uint16_t desc = 0;
	if ( fOptions.outputKind() != Options::kObjectFile ) {
		// set n_desc ( high byte is library ordinal, low byte is reference type )
		desc = REFERENCE_FLAG_UNDEFINED_LAZY; // FIXME
		try {
			uint8_t ordinal = this->ordinalForLibrary(atom->getFile());
			SET_LIBRARY_ORDINAL(desc, ordinal);
		}
		catch (const char* msg) {
			throwf("%s %s from %s", msg, atom->getDisplayName(), atom->getFile()->getPath());
		}
	}
	if ( atom->dontStripName() )
		desc |= REFERENCED_DYNAMICALLY;
	// an import proxy is always weak (overridden by definition in .o files)
	// so we ask its reader if the exported symbol in its dylib is weak
	if ( ( fOptions.outputKind() != Options::kObjectFile) && atom->getFile()->isDefinitionWeak(*atom) ) {
		desc |= N_REF_TO_WEAK;
		fReferencesWeakImports = true;
	}
	// set weak_import attribute
	if ( atom->getImportWeakness() == ObjectFile::Atom::kWeakImport ) 
		desc |= N_WEAK_REF;
	entry->set_n_desc(desc);
	
	// set n_value, zero for import proxy and size for tentative definition
	entry->set_n_value(atom->getSize());
}

void Writer::setLocalNlist(const ObjectFile::Atom* atom, macho_nlist* entry)
{
	// set n_type
	uint8_t type = N_SECT;
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
	if ( atom->isWeakDefinition() && (strcmp(atom->getSectionName(), "__common") != 0) ) // commons on not weak
		desc |= N_WEAK_DEF;
	entry->set_n_desc(desc);
	
	// set n_value ( address this symbol will be at if this executable is loaded at it preferred address )
	entry->set_n_value(this->getAtomLoadAddress(atom));
}


void Writer::setNlistRange(std::vector<class ObjectFile::Atom*>& atoms, uint32_t startIndex, uint32_t count)
{
	macho_nlist* entry = &fSymbolTable[startIndex];
	for (uint32_t i=0; i < count; ++i, ++entry) {
		ObjectFile::Atom* atom = atoms[i];
		entry->set_n_strx(this->fStringsAtom->add(atom->getName()));
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

void Writer::buildSymbolTable()
{
	fSymbolTableStabsStartIndex		= 0;
	fSymbolTableStabsCount			= this->collectStabs();
	fSymbolTableLocalStartIndex		= fSymbolTableStabsStartIndex + fSymbolTableStabsCount;
	fSymbolTableLocalCount			= fLocalSymbolAtoms.size();
	fSymbolTableExportStartIndex	= fSymbolTableLocalStartIndex + fSymbolTableLocalCount;
	fSymbolTableExportCount			= fExportedAtoms.size();
	fSymbolTableImportStartIndex	= fSymbolTableExportStartIndex + fSymbolTableExportCount;
	fSymbolTableImportCount			= fImportedAtoms.size();
	
	// allocate symbol table
	fSymbolTableCount = fSymbolTableStabsCount + fSymbolTableLocalCount + fSymbolTableExportCount + fSymbolTableImportCount;
	fSymbolTable = new macho_nlist[fSymbolTableCount];
	
	// fill in symbol table and string pool (do stabs last so strings are at end of pool)
	setNlistRange(fLocalSymbolAtoms, fSymbolTableLocalStartIndex,  fSymbolTableLocalCount);
	setNlistRange(fExportedAtoms,    fSymbolTableExportStartIndex, fSymbolTableExportCount);
	setNlistRange(fImportedAtoms,    fSymbolTableImportStartIndex, fSymbolTableImportCount);
	addStabs(fSymbolTableStabsStartIndex, fSymbolTableStabsCount);
}



bool Writer::shouldExport(ObjectFile::Atom& atom)
{
	switch ( atom.getScope() ) {
		case ObjectFile::Atom::scopeGlobal:
			return true;
		case ObjectFile::Atom::scopeLinkageUnit:
			return ( fOptions.keepPrivateExterns() && (fOptions.outputKind() == Options::kObjectFile) );
		default:
			return false;
	}
}

void Writer::collectExportedAndImportedAndLocalAtoms()
{
	const int atomCount = fAllAtoms->size();
	for (int i=0; i < atomCount; ++i) {
		ObjectFile::Atom* atom = (*fAllAtoms)[i];
		// only named atoms go in symbol table
		if ( atom->getName() != NULL ) {
			// put atom into correct bucket: imports, exports, locals
			//printf("collectExportedAndImportedAndLocalAtoms() name=%s\n", atom->getDisplayName());
			if ( atom->isImportProxy() || ((fOptions.outputKind() == Options::kObjectFile) && (strcmp(atom->getSectionName(), "__common") == 0)) )
				fImportedAtoms.push_back(atom);
			else if ( this->shouldExport(*atom) )
				fExportedAtoms.push_back(atom);
			else if ( !fOptions.stripLocalSymbols() )
				fLocalSymbolAtoms.push_back(atom);
		}
	}
	
	// sort exported atoms by name
	std::sort(fExportedAtoms.begin(), fExportedAtoms.end(), ExportSorter());
}


bool Writer::stabChunkCompare(const struct StabChunks& lhs, const struct StabChunks& rhs)
{
	if ( lhs.fReader != rhs.fReader ) {
		return lhs.fReaderOrder < rhs.fReaderOrder;
	}
	return lhs.fOrderInReader < rhs.fOrderInReader;
}

unsigned int Writer::collectStabs()
{	
	unsigned int count = 0;
	
	// collect all stabs chunks
	std::set<ObjectFile::Reader*> seenReaders;
	std::map<ObjectFile::Reader*, unsigned int> readerOrdinals;
	const int atomCount = fAllAtoms->size();
	for (int i=0; i < atomCount; ++i) {
		ObjectFile::Atom* atom = (*fAllAtoms)[i];
		ObjectFile::Reader* atomsReader = atom->getFile();
		unsigned int readerOrder = 0;
		if ( atomsReader != NULL ) {
			std::map<ObjectFile::Reader*, unsigned int>::iterator pos = readerOrdinals.find(atomsReader);
			if ( pos == readerOrdinals.end() ) {
				readerOrder = readerOrdinals.size();
				readerOrdinals[atomsReader] = readerOrder;
				std::vector<ObjectFile::StabsInfo>* readerStabs = atomsReader->getStabsDebugInfo();
				if ( readerStabs != NULL ) {
					StabChunks chunk;
					chunk.fAtom				= NULL;
					chunk.fReader			= atomsReader;
					chunk.fReaderOrder		= readerOrder;
					chunk.fOrderInReader	= 0;
					chunk.fStabs			= readerStabs;
					fStabChunks.push_back(chunk);
					count += readerStabs->size() + 1; // extra one is for trailing N_SO
				}
			}
			else {
				readerOrder = pos->second;
			}
		}
		std::vector<ObjectFile::StabsInfo>* atomStabs = atom->getStabsDebugInfo();
		if ( atomStabs != NULL ) {
			StabChunks chunk;
			chunk.fAtom				= atom;
			chunk.fReader			= atomsReader;
			chunk.fReaderOrder		= readerOrder;
			chunk.fOrderInReader	= atom->getSortOrder();
			chunk.fStabs			= atomStabs;
			fStabChunks.push_back(chunk);
			count += atomStabs->size();
		}
	}
	
	// sort stabs: group by .o file 
	std::sort(fStabChunks.begin(), fStabChunks.end(), stabChunkCompare);
	
	//fprintf(stderr, "Sorted stabs:\n");
	//for (std::vector<StabChunks>::iterator it=fStabChunks.begin(); it != fStabChunks.end(); it++) {
	//	ObjectFile::Atom* atom = (*it).fAtom;
	//	if ( atom != NULL )
	//		fprintf(stderr, "\t%s\n", (*it).fAtom->getDisplayName());
	//	else
	//		fprintf(stderr, "\t%s\n", (*it).fReader->getPath());
	//}
	
	return count;
}

macho_uintptr_t Writer::valueForStab(const ObjectFile::StabsInfo& stab, const ObjectFile::Atom* atom)
{
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
			// all these stab types need their value changed from an offset in the atom to an address
			if ( atom != NULL )
				return getAtomLoadAddress(atom) + stab.atomOffset;
	}
	return stab.atomOffset;
}
 

void Writer::addStabs(uint32_t startIndex, uint32_t count)
{
	macho_nlist* entry = &fSymbolTable[startIndex];
	const int chunkCount = fStabChunks.size();
	for (int i=0; i < chunkCount; ++i ) {
		const StabChunks& chunk = fStabChunks[i];
		const int stabCount = chunk.fStabs->size();
		for (int j=0; j < stabCount; ++j ) {
			const ObjectFile::StabsInfo& stab = (*chunk.fStabs)[j];
			entry->set_n_type(stab.type);
			entry->set_n_sect(stab.other);
			entry->set_n_desc(stab.desc);
			entry->set_n_value(valueForStab(stab, chunk.fAtom));
			entry->set_n_strx(this->fStringsAtom->add(stab.string));
			++entry;
		}	
		if ( (i == chunkCount-1) || (fStabChunks[i+1].fReader != chunk.fReader) ) {
			// need to add empty SO at end of each file
			entry->set_n_type(N_SO);
			entry->set_n_sect(1);
			entry->set_n_desc(0);
			entry->set_n_value(0);
			entry->set_n_strx(this->fStringsAtom->emptyString());
			++entry;
		}
	}
}



uint32_t Writer::symbolIndex(ObjectFile::Atom& atom)
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
	
	fprintf(stderr, "symbolIndex(%s)\n", atom.getDisplayName());
	fprintf(stderr, "from %s\n", atom.getFile()->getPath());
	throw "atom not found";
}

void Writer::buildFixups()
{
	if ( fOptions.outputKind() == Options::kObjectFile )
		this->buildObjectFileFixups();
	else
		this->buildExecutableFixups();
}

uint32_t Writer::addRelocs(ObjectFile::Atom* atom, ObjectFile::Reference* ref)
{
	ObjectFile::Atom& target = ref->getTarget();
	bool isExtern = target.isImportProxy() || ( strcmp(target.getSectionName(), "__common") == 0 );
	uint32_t symbolIndex = 0;
	if ( isExtern ) 
		symbolIndex = this->symbolIndex(target);
	uint32_t sectionNum = target.getSection()->getIndex();
	uint32_t address = atom->getSectionOffset()+ref->getFixUpOffset();
	macho_relocation_info reloc1;
	macho_relocation_info reloc2;
	macho_scattered_relocation_info* sreloc1 = (macho_scattered_relocation_info*)&reloc1;
	macho_scattered_relocation_info* sreloc2 = (macho_scattered_relocation_info*)&reloc2;
	
	switch ( ref->getKind() ) {
		case ObjectFile::Reference::noFixUp:
			return 0;
			
		case ObjectFile::Reference::pointer:
			reloc1.set_r_address(address);
			if ( isExtern )
				reloc1.set_r_symbolnum(symbolIndex);
			else
				reloc1.set_r_symbolnum(sectionNum);
			reloc1.set_r_pcrel(false);
			reloc1.set_r_length(macho_relocation_info::pointer_length); 
			reloc1.set_r_extern(isExtern);
			reloc1.set_r_type(GENERIC_RELOC_VANILLA);
			fInternalRelocs.insert(fInternalRelocs.begin(), reloc1);
			return 1;
			
		case ObjectFile::Reference::ppcFixupBranch24:
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
			fInternalRelocs.insert(fInternalRelocs.begin(), reloc1);
			return 1;
		
		case ObjectFile::Reference::ppcFixupBranch14:
			reloc1.set_r_address(address);
			reloc1.set_r_symbolnum(sectionNum);
			reloc1.set_r_pcrel(true);
			reloc1.set_r_length(2); 
			reloc1.set_r_extern(false);
			reloc1.set_r_type(PPC_RELOC_BR14);
			fInternalRelocs.insert(fInternalRelocs.begin(), reloc1);
			return 1;

		case ObjectFile::Reference::ppcFixupPicBaseLow14:
		case ObjectFile::Reference::ppcFixupPicBaseLow16:
			{
				macho_uintptr_t fromAddr = atom->getAddress() + ref->getFromTargetOffset();
				macho_uintptr_t toAddr = target.getAddress() + ref->getTargetOffset();
				uint32_t overflow = 0;
				if ( ((toAddr-fromAddr) & 0x00008000) != 0 )
					overflow = 1;
				sreloc1->set_r_scattered(true);
				sreloc1->set_r_pcrel(false);
				sreloc1->set_r_length(2); 
				if ( ref->getKind() == ObjectFile::Reference::ppcFixupPicBaseLow16 )
					sreloc1->set_r_type(PPC_RELOC_LO16_SECTDIFF);
				else
					sreloc1->set_r_type(PPC_RELOC_LO14_SECTDIFF);
				sreloc1->set_r_address(address);
				sreloc1->set_r_value(target.getAddress());
				sreloc2->set_r_scattered(true);
				sreloc2->set_r_pcrel(false);
				sreloc2->set_r_length(2); 
				sreloc2->set_r_type(PPC_RELOC_PAIR);
				sreloc2->set_r_address(((toAddr-fromAddr) >> 16));
				sreloc2->set_r_value(fromAddr);
				fInternalRelocs.insert(fInternalRelocs.begin(), reloc2);
				fInternalRelocs.insert(fInternalRelocs.begin(), reloc1);
				return 2;
			}
		
		case ObjectFile::Reference::ppcFixupPicBaseHigh16:
			{
				macho_uintptr_t fromAddr = atom->getAddress() + ref->getFromTargetOffset();
				macho_uintptr_t toAddr = target.getAddress() + ref->getTargetOffset();
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
				fInternalRelocs.insert(fInternalRelocs.begin(), reloc2);
				fInternalRelocs.insert(fInternalRelocs.begin(), reloc1);
				return 2;
			}
			
		case ObjectFile::Reference::ppcFixupAbsLow14:
		case ObjectFile::Reference::ppcFixupAbsLow16:
			{
				macho_uintptr_t toAddr = target.getAddress() + ref->getTargetOffset();
				if ( (ref->getTargetOffset() == 0) || isExtern ) {
					reloc1.set_r_address(address);
					if ( isExtern )
						reloc1.set_r_symbolnum(symbolIndex);
					else
						reloc1.set_r_symbolnum(sectionNum);
					reloc1.set_r_pcrel(false);
					reloc1.set_r_length(2); 
					reloc1.set_r_extern(isExtern);
					if ( ref->getKind() == ObjectFile::Reference::ppcFixupAbsLow16 )
						reloc1.set_r_type(PPC_RELOC_LO16);
					else
						reloc1.set_r_type(PPC_RELOC_LO14);
				}
				else {
					sreloc1->set_r_scattered(true);
					sreloc1->set_r_pcrel(false);
					sreloc1->set_r_length(2); 
					if ( ref->getKind() == ObjectFile::Reference::ppcFixupAbsLow16 )
						sreloc1->set_r_type(PPC_RELOC_LO16);
					else
						sreloc1->set_r_type(PPC_RELOC_LO14);
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
				fInternalRelocs.insert(fInternalRelocs.begin(), reloc2);
				fInternalRelocs.insert(fInternalRelocs.begin(), reloc1);
				return 2;
			}
			
		case ObjectFile::Reference::ppcFixupAbsHigh16:
			{
				macho_uintptr_t toAddr = target.getAddress() + ref->getTargetOffset();
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
				fInternalRelocs.insert(fInternalRelocs.begin(), reloc2);
				fInternalRelocs.insert(fInternalRelocs.begin(), reloc1);
				return 2;
			}
		
		case ObjectFile::Reference::ppcFixupAbsHigh16AddLow:
			{
				macho_uintptr_t toAddr = target.getAddress() + ref->getTargetOffset();
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
				fInternalRelocs.insert(fInternalRelocs.begin(), reloc2);
				fInternalRelocs.insert(fInternalRelocs.begin(), reloc1);
				return 2;
			}
		
		case ObjectFile::Reference::pointer32Difference:
		case ObjectFile::Reference::pointer64Difference:
			{
				macho_uintptr_t toAddr = target.getAddress() + ref->getTargetOffset();
				macho_uintptr_t fromAddr = ref->getFromTarget().getAddress() + ref->getFromTargetOffset();
				sreloc1->set_r_scattered(true);
				sreloc1->set_r_pcrel(false);
				if ( ref->getKind() == ObjectFile::Reference::pointer64Difference )
					sreloc1->set_r_length(3); 
				else
					sreloc1->set_r_length(2); 
				if ( ref->getTargetOffset() != 0 )
					sreloc1->set_r_type(PPC_RELOC_LOCAL_SECTDIFF); 
				else
					sreloc1->set_r_type(PPC_RELOC_SECTDIFF); 
				sreloc1->set_r_address(address);
				sreloc1->set_r_value(toAddr);
				sreloc2->set_r_scattered(true);
				sreloc2->set_r_pcrel(false);
				sreloc2->set_r_length(macho_relocation_info::pointer_length); 
				sreloc2->set_r_type(PPC_RELOC_PAIR);
				sreloc2->set_r_address(0);
				sreloc2->set_r_value(fromAddr);
				fInternalRelocs.insert(fInternalRelocs.begin(), reloc2);
				fInternalRelocs.insert(fInternalRelocs.begin(), reloc1);
				return 2;
			}
			
		case ObjectFile::Reference::x86FixupBranch32:
			reloc1.set_r_address(address);
			reloc1.set_r_symbolnum(sectionNum);
			reloc1.set_r_pcrel(true);
			reloc1.set_r_length(2); 
			reloc1.set_r_extern(false);
			reloc1.set_r_type(GENERIC_RELOC_VANILLA);
			fInternalRelocs.insert(fInternalRelocs.begin(), reloc1);
			return 1;
		
	}
	return 0;
}


void Writer::buildObjectFileFixups()
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
			std::vector<ObjectFile::Atom*>& sectionAtoms = curSection->fAtoms;
			if ( ! curSection->fAllZeroFill ) {
				if ( curSection->fAllNonLazyPointers || curSection->fAllLazyPointers )
					curSection->fIndirectSymbolOffset = fIndirectSymbolTable.size();
				curSection->fRelocOffset = relocIndex;
				const int atomCount = sectionAtoms.size();
				for (int k=0; k < atomCount; ++k) {
					ObjectFile::Atom* atom = sectionAtoms[k];
					std::vector<ObjectFile::Reference*>& refs = atom->getReferences();
					const int refCount = refs.size();
					for (int l=0; l < refCount; ++l) {
						ObjectFile::Reference* ref = refs[l];
						if ( curSection->fAllNonLazyPointers || curSection->fAllLazyPointers ) {
							uint32_t offsetInSection = atom->getSectionOffset();
							uint32_t indexInSection = offsetInSection / sizeof(macho_uintptr_t);	
							uint32_t undefinedSymbolIndex = this->symbolIndex(ref->getTarget());
							uint32_t indirectTableIndex = indexInSection + curSection->fIndirectSymbolOffset;
							IndirectEntry entry = { indirectTableIndex, undefinedSymbolIndex };
							//printf("fIndirectSymbolTable.add(%d-%d => 0x%X-%s), size=%lld\n", indexInSection, indirectTableIndex, undefinedSymbolIndex, ref->getTarget().getName(), atom->getSize());
							fIndirectSymbolTable.push_back(entry);
							if ( curSection->fAllLazyPointers ) {
								ObjectFile::Atom& target = ref->getTarget();
								ObjectFile::Atom& fromTarget = ref->getFromTarget();
								if ( &fromTarget == NULL ) {
									fprintf(stderr, "lazy pointer %s missing initial binding\n", atom->getDisplayName());
								}
								else {
									bool isExtern = target.isImportProxy();
									uint32_t symbolIndex = 0;
									if ( isExtern ) 
										symbolIndex = this->symbolIndex(target);
									uint32_t sectionNum = target.getSection()->getIndex();
									uint32_t address = atom->getSectionOffset();
									macho_relocation_info reloc1;
									reloc1.set_r_address(address);
									if ( isExtern )
										reloc1.set_r_symbolnum(symbolIndex);
									else
										reloc1.set_r_symbolnum(sectionNum);
									reloc1.set_r_pcrel(false);
									reloc1.set_r_length(macho_relocation_info::pointer_length); 
									reloc1.set_r_extern(isExtern);
									reloc1.set_r_type(GENERIC_RELOC_VANILLA);
									fInternalRelocs.insert(fInternalRelocs.begin(), reloc1);
									++relocIndex;
								}
							}
						}
						else {
							relocIndex += this->addRelocs(atom, ref);
						}
					}
				}
				curSection->fRelocCount = relocIndex - curSection->fRelocOffset;
			}
		}
	}
	
	// now reverse reloc entries
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


void Writer::buildExecutableFixups()
{
	const bool slideable = (fOptions.outputKind() != Options::kDynamicExecutable) && (fOptions.outputKind() != Options::kStaticExecutable);
	std::vector<SegmentInfo*>& segmentInfos = fSegmentInfos;
	const int segCount = segmentInfos.size();
	for(int i=0; i < segCount; ++i) {
		SegmentInfo* curSegment = segmentInfos[i];
		std::vector<SectionInfo*>& sectionInfos = curSegment->fSections;
		const int sectionCount = sectionInfos.size();
		for(int j=0; j < sectionCount; ++j) {
			SectionInfo* curSection = sectionInfos[j];
			std::vector<ObjectFile::Atom*>& sectionAtoms = curSection->fAtoms;
			if ( ! curSection->fAllZeroFill ) {
				if ( curSection->fAllNonLazyPointers || curSection->fAllLazyPointers )
					curSection->fIndirectSymbolOffset = fIndirectSymbolTable.size();
				const int atomCount = sectionAtoms.size();
				for (int k=0; k < atomCount; ++k) {
					ObjectFile::Atom* atom = sectionAtoms[k];
					std::vector<ObjectFile::Reference*>& refs = atom->getReferences();
					const int refCount = refs.size();
					//printf("atom %s has %d references\n", atom->getDisplayName(), refCount);
					for (int l=0; l < refCount; ++l) {
						ObjectFile::Reference* ref = refs[l];
						if ( curSection->fAllNonLazyPointers || curSection->fAllLazyPointers ) {
							// if atom is in (non)lazy_pointer section, this is encoded as an indirect symbol
							if ( atom->getSize() != sizeof(macho_uintptr_t) ) {
								printf("wrong size pointer atom %s from file %s\n", atom->getDisplayName(), atom->getFile()->getPath());
							}
							uint32_t offsetInSection = atom->getSectionOffset();
							uint32_t indexInSection = offsetInSection / sizeof(macho_uintptr_t);	
							uint32_t undefinedSymbolIndex = INDIRECT_SYMBOL_LOCAL;
							//fprintf(stderr,"indirect pointer atom %p %s section offset = %d\n", atom, atom->getDisplayName(), offsetInSection);
							if ( ref->getTarget().isImportProxy() 
							  || ref->getTarget().isWeakDefinition() 
							  || (fOptions.interposable() && fOptions.shouldExport(ref->getTarget().getName()))
							  || (fOptions.nameSpace() == Options::kFlatNameSpace) 
							  || (fOptions.nameSpace() == Options::kForceFlatNameSpace) ) {
								undefinedSymbolIndex = this->symbolIndex(ref->getTarget());
							}
							uint32_t indirectTableIndex = indexInSection + curSection->fIndirectSymbolOffset;
							IndirectEntry entry = { indirectTableIndex, undefinedSymbolIndex };
							//fprintf(stderr,"fIndirectSymbolTable.add(%d-%d => 0x%X-%s), size=%lld\n", indexInSection, indirectTableIndex, undefinedSymbolIndex, ref->getTarget().getName(), atom->getSize());
							fIndirectSymbolTable.push_back(entry);
							if ( slideable && curSection->fAllLazyPointers ) {
								// if this is a dylib/bundle, need vanilla internal relocation to fix up binding handler if image slides
								macho_relocation_info pblaReloc;
								SectionInfo* sectInfo = (SectionInfo*)ref->getFromTarget().getSection();
								uint32_t sectionNum = sectInfo->getIndex();
								pblaReloc.set_r_address(atom->getAddress()-fOptions.baseAddress());
								pblaReloc.set_r_symbolnum(sectionNum); 
								pblaReloc.set_r_pcrel(false);
								pblaReloc.set_r_length(macho_relocation_info::pointer_length); 
								pblaReloc.set_r_extern(false);
								pblaReloc.set_r_type(GENERIC_RELOC_VANILLA);
								fInternalRelocs.push_back(pblaReloc);
							}
						}
						else if ( ref->requiresRuntimeFixUp(slideable) ) {
							if ( ! atom->getSegment().isContentWritable() )
								throwf("relocations in read-only segments not supported. %s in %s reference to %s", atom->getDisplayName(), atom->getFile()->getPath(), ref->getTarget().getDisplayName());
							if ( ref->getTarget().isImportProxy() ) {
								// if import is to antoher dylib, this is encoded as an external relocation
								macho_relocation_info externalReloc;
								externalReloc.set_r_address(atom->getAddress()+ref->getFixUpOffset()-fOptions.baseAddress());
								externalReloc.set_r_symbolnum(this->symbolIndex(ref->getTarget()));
								externalReloc.set_r_pcrel(false);
								externalReloc.set_r_length(macho_relocation_info::pointer_length); 
								externalReloc.set_r_extern(true);
								externalReloc.set_r_type(GENERIC_RELOC_VANILLA);
								fExternalRelocs.push_back(externalReloc);
							}
							else {
								// if this is a dylib/bundle, need fix-up encoded as an internal relocation
								macho_relocation_info internalReloc;
								SectionInfo* sectInfo = (SectionInfo*)ref->getTarget().getSection();
								uint32_t sectionNum = sectInfo->getIndex();
								// special case _mh_dylib_header and friends which are not in any real section
								if ( (sectionNum ==0) && sectInfo->fVirtualSection && (strcmp(sectInfo->fSectionName, "._mach_header") == 0) ) 
									sectionNum = 1;
								internalReloc.set_r_address(atom->getAddress()+ref->getFixUpOffset()-fOptions.baseAddress());
								internalReloc.set_r_symbolnum(sectionNum);
								internalReloc.set_r_pcrel(false);
								internalReloc.set_r_length(macho_relocation_info::pointer_length); 
								internalReloc.set_r_extern(false);
								internalReloc.set_r_type(GENERIC_RELOC_VANILLA);
								fInternalRelocs.push_back(internalReloc);
							}
						}
					}
				}
			}
		}
	}
}

class ContentWriter : public ObjectFile::ContentWriter
{
public:
					ContentWriter(int fd, uint64_t fileOffset) : fFileDescriptor(fd), fFileOffset(fileOffset) {}
	virtual void	write(uint64_t atomOffset, const void* buffer, uint64_t size) {
		::pwrite(fFileDescriptor, buffer, size, fFileOffset+atomOffset);
	}
private:
	int			fFileDescriptor;
	uint64_t 	fFileOffset;
};


void Writer::writeAtoms()
{
	const bool requireAllFixUps = (fOptions.outputKind() != Options::kObjectFile);

	std::vector<SegmentInfo*>& segmentInfos = fSegmentInfos;
	const int segCount = segmentInfos.size();
	for(int i=0; i < segCount; ++i) {
		SegmentInfo* curSegment = segmentInfos[i];
		bool isText = ((curSegment->fInitProtection & VM_PROT_EXECUTE) != 0);
		std::vector<SectionInfo*>& sectionInfos = curSegment->fSections;
		const int sectionCount = sectionInfos.size();
		for(int j=0; j < sectionCount; ++j) {
			SectionInfo* curSection = sectionInfos[j];
			std::vector<ObjectFile::Atom*>& sectionAtoms = curSection->fAtoms;
			//printf("writing %d atoms for section %s\n", (int)sectionAtoms.size(), curSection->fSectionName);
			if ( ! curSection->fAllZeroFill ) {
				const int atomCount = sectionAtoms.size();
				uint32_t end = curSection->fFileOffset;
				for (int k=0; k < atomCount; ++k) {
					ObjectFile::Atom* atom = sectionAtoms[k];
					if ( !atom->isImportProxy() ) {
						uint32_t offset = curSection->fFileOffset + atom->getSectionOffset();
						if ( isText && (offset != end) ) {
							// fill gaps with no-ops
			#if defined(ARCH_PPC) || defined(ARCH_PPC64) 
							uint32_t ppcNop;
							OSWriteBigInt32(&ppcNop, 0, 0x60000000);
							for (uint32_t p=end; p < offset; p += 4)
								::pwrite(fFileDescriptor, &ppcNop, 4, p);
			#else  defined(ARCH_I386)
							uint8_t x86Nop = 0x90;
							for (uint32_t p=end; p < offset; ++p)
								::pwrite(fFileDescriptor, &x86Nop, 1, p);
			#endif
						}
						end = offset+atom->getSize();
						//fprintf(stderr, "writing 0x%08X -> 0x%08X, atom %s\n", offset, end, atom->getDisplayName());
						ContentWriter writer(fFileDescriptor, offset);
						atom->writeContent(requireAllFixUps, writer);
					}
				}
			}
		}
	}
}	


void Writer::partitionIntoSections()
{
	const bool oneSegmentCommand = (fOptions.outputKind() == Options::kObjectFile);
	
	// for every atom, set its sectionInfo object and section offset
	// build up fSegmentInfos along the way
	ObjectFile::Section* curSection = NULL;
	SectionInfo* currentSectionInfo = NULL;
	SegmentInfo* currentSegmentInfo = NULL;
	unsigned int sectionIndex = 1;
	for (unsigned int i=0; i < fAllAtoms->size(); ++i) {
		ObjectFile::Atom* atom = (*fAllAtoms)[i];
		if ( atom->getSection() != curSection ) {
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
				currentSectionInfo->fAlignment = atom->getAlignment();
				currentSectionInfo->fAllZeroFill = atom->isZeroFill();
				currentSectionInfo->fVirtualSection = ( currentSectionInfo->fSectionName[0] == '.');
				if ( !currentSectionInfo->fVirtualSection || fEmitVirtualSections )
					currentSectionInfo->setIndex(sectionIndex++);
				currentSegmentInfo->fSections.push_back(currentSectionInfo);
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
					currentSegmentInfo->fInitProtection = initprot;
					if ( initprot == 0 )
						currentSegmentInfo->fMaxProtection = 0;  // pagezero should have maxprot==initprot==0
					else
						currentSegmentInfo->fMaxProtection = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
					currentSegmentInfo->fBaseAddress = atom->getSegment().getBaseAddress();
					currentSegmentInfo->fFixedAddress = atom->getSegment().hasFixedAddress();
					this->fSegmentInfos.push_back(currentSegmentInfo);
				}
				currentSectionInfo = new SectionInfo();
				strcpy(currentSectionInfo->fSectionName, atom->getSectionName());
				strcpy(currentSectionInfo->fSegmentName, atom->getSegment().getName());
				currentSectionInfo->fAlignment = atom->getAlignment();
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
			curSection = atom->getSection();
		}
		// any non-zero fill atoms make whole section marked not-zero-fill
		if ( currentSectionInfo->fAllZeroFill && ! atom->isZeroFill() )
			currentSectionInfo->fAllZeroFill = false;
		// change section object to be Writer's SectionInfo object
		atom->setSection(currentSectionInfo);
		// section alignment is that of a contained atom with the greatest alignment
		uint8_t atomAlign = atom->getAlignment();
		if ( currentSectionInfo->fAlignment < atomAlign )
			currentSectionInfo->fAlignment = atomAlign;
		// calculate section offset for this atom
		uint64_t offset = currentSectionInfo->fSize;
		uint64_t alignment = 1 << atomAlign;
		offset = ( (offset+alignment-1) & (-alignment) );
		atom->setSectionOffset(offset);
		currentSectionInfo->fSize = offset + atom->getSize();
		// add atom to section vector
		currentSectionInfo->fAtoms.push_back(atom);
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
bool Writer::addBranchIslands()
{
	bool result = false;
#if defined(ARCH_PPC) || defined(ARCH_PPC64) 
	// Can only possibly need branch islands if __TEXT segment > 16M
	if ( fLoadCommandsSegment->fSize > 16000000 ) {
		const uint32_t kBetweenRegions = 15000000; // place regions of islands every 15MB in __text section
		SectionInfo* textSection = NULL;
		for (std::vector<SectionInfo*>::iterator it=fLoadCommandsSegment->fSections.begin(); it != fLoadCommandsSegment->fSections.end(); it++) {
			if ( strcmp((*it)->fSectionName, "__text") == 0 )
				textSection = *it;
		}
		const int kIslandRegionsCount = textSection->fSize / kBetweenRegions; 
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
				if ( ref->getKind() == ObjectFile::Reference::ppcFixupBranch24 ) {
					ObjectFile::Atom& target = ref->getTarget();
					int64_t srcAddr = atom->getAddress() + ref->getFixUpOffset();
					int64_t dstAddr = target.getAddress() + ref->getTargetOffset();
					int64_t displacement = dstAddr - srcAddr;
					const int64_t kFifteenMegLimit = kBetweenRegions;
					if ( (displacement > kFifteenMegLimit) || (displacement < (-kFifteenMegLimit)) ) {
						for (int i=0; i < kIslandRegionsCount; ++i) {
							AtomToIsland* region=&regionsMap[i];
							int64_t islandRegionAddr = kBetweenRegions * (i+1);
							if ( ((srcAddr < islandRegionAddr) && (dstAddr > islandRegionAddr)) 
							   ||((dstAddr < islandRegionAddr) && (srcAddr > islandRegionAddr)) ) {
								TargetAndOffset islandTarget = { &target, ref->getTargetOffset() };
								AtomToIsland::iterator pos = region->find(islandTarget);
								if ( pos == region->end() ) {
									BranchIslandAtom* island = new BranchIslandAtom(*this, target.getDisplayName(), i, target, ref->getTargetOffset());
									(*region)[islandTarget] = island;
									regionsIslands[i].push_back(island);
									++islandCount;
									ref->setTarget(*island, 0);
								}
								else {
									ref->setTarget(*(pos->second), 0);
								}
							}
						}
					}
				}
			}
		}
		
		// insert islands into __text section and adjust section offsets
		if ( islandCount > 0 ) {
			std::vector<ObjectFile::Atom*> newAtomList;
			newAtomList.reserve(textSection->fAtoms.size()+islandCount);
			uint64_t islandRegionAddr = kBetweenRegions;
			int regionIndex = 0;
			uint64_t sectionOffset = 0;
			for (std::vector<ObjectFile::Atom*>::iterator it=textSection->fAtoms.begin(); it != textSection->fAtoms.end(); it++) {
				ObjectFile::Atom* atom = *it;
				newAtomList.push_back(atom);
				if ( atom->getAddress() > islandRegionAddr ) {
					std::vector<ObjectFile::Atom*>* regionIslands = &regionsIslands[regionIndex];
					for (std::vector<ObjectFile::Atom*>::iterator rit=regionIslands->begin(); rit != regionIslands->end(); rit++) {
						ObjectFile::Atom* islandAtom = *rit;
						newAtomList.push_back(islandAtom);
						islandAtom->setSection(textSection);
						uint64_t alignment = 1 << (islandAtom->getAlignment());
						sectionOffset = ( (sectionOffset+alignment-1) & (-alignment) );
						islandAtom->setSectionOffset(sectionOffset);
						sectionOffset += islandAtom->getSize();
					}
					++regionIndex;
					islandRegionAddr += kBetweenRegions;
				}
				uint64_t alignment = 1 << (atom->getAlignment());
				sectionOffset = ( (sectionOffset+alignment-1) & (-alignment) );
				atom->setSectionOffset(sectionOffset);
				sectionOffset += atom->getSize();
			}
			textSection->fAtoms = newAtomList;
			textSection->fSize = sectionOffset;
			result = true;
		}
		
	}
#endif
	return result;
}


void Writer::adjustLoadCommandsAndPadding()
{
	fSegmentCommands->computeSize();
	
	// recompute load command section offsets
	uint64_t offset = 0;
	std::vector<class ObjectFile::Atom*>& loadCommandAtoms = fLoadCommandsSection->fAtoms;
	const unsigned int atomCount = loadCommandAtoms.size();
	for (unsigned int i=0; i < atomCount; ++i) {
		ObjectFile::Atom* atom = loadCommandAtoms[i];
		uint64_t alignment = 1 << atom->getAlignment();
		offset = ( (offset+alignment-1) & (-alignment) );
		atom->setSectionOffset(offset);
		offset += atom->getSize();
		fLoadCommandsSection->fSize = offset;
	}
	
	std::vector<SectionInfo*>& sectionInfos = fLoadCommandsSegment->fSections;
	const int sectionCount = sectionInfos.size();
	uint64_t paddingSize = 0;
	if ( fOptions.outputKind() == Options::kDyld ) {
		// dyld itself has special padding requirements.  We want the beginning __text section to start at a stable address
		uint32_t totalSizeOfHeaderAndLoadCommands = 0;
		for(int j=0; j < sectionCount; ++j) {
			SectionInfo* curSection = sectionInfos[j];
			totalSizeOfHeaderAndLoadCommands += curSection->fSize;
			if ( strcmp(curSection->fSectionName, fHeaderPadding->getSectionName()) == 0 )
				break;
		}
		paddingSize = 4096 - (totalSizeOfHeaderAndLoadCommands % 4096);
	}
	else {
		// calculate max padding to keep segment size same, but all free space at end of load commands
		uint64_t totalSize = 0;
		uint64_t worstCaseAlignmentPadding = 0;
		for(int j=0; j < sectionCount; ++j) {
			SectionInfo* curSection = sectionInfos[j];
			totalSize += curSection->fSize;
			if ( j != 0 ) // don't count aligment of mach_header which is page-aligned
				worstCaseAlignmentPadding += (1 << curSection->fAlignment) - 1;
		}
		uint64_t segmentSize = ((totalSize+worstCaseAlignmentPadding+4095) & (-4096));
		// don't know exactly how it will layout, but we can inflate padding atom this big and still keep aligment constraints
		paddingSize = segmentSize - totalSize;
		
		// if command line requires more padding than this
		if ( paddingSize < fOptions.minimumHeaderPad() ) {
			int extraPages = (fOptions.minimumHeaderPad() - paddingSize + 4095)/4096;
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
void Writer::assignFileOffsets()
{
	bool haveFixedSegments = false;
	uint64_t fileOffset = 0;
	uint64_t nextContiguousAddress = 0;
	bool baseAddressUsed = false; 
	std::vector<SegmentInfo*>& segmentInfos = fSegmentInfos;
	const int segCount = segmentInfos.size();
	for(int i=0; i < segCount; ++i) {
		SegmentInfo* curSegment = segmentInfos[i];
		fileOffset = (fileOffset+4095) & (-4096);
		curSegment->fFileOffset = fileOffset;
		if ( curSegment->fFixedAddress ) {
			// segment has fixed address already set
			haveFixedSegments = true;
		}
		else {
			// segment uses next address
			if ( !baseAddressUsed ) {
				baseAddressUsed = true;
				if ( fOptions.baseAddress() != 0 ) 
					nextContiguousAddress = fOptions.baseAddress();
			}
			curSegment->fBaseAddress = nextContiguousAddress;
		}
		uint64_t address = curSegment->fBaseAddress;
		std::vector<SectionInfo*>& sectionInfos = curSegment->fSections;
		const int sectionCount = sectionInfos.size();
		for(int j=0; j < sectionCount; ++j) {
			SectionInfo* curSection = sectionInfos[j];
			uint64_t alignment = 1 << curSection->fAlignment;
			fileOffset = ( (fileOffset+alignment-1) & (-alignment) );
			address    = ( (address+alignment-1)    & (-alignment) );
			curSection->fFileOffset = fileOffset;
			curSection->setBaseAddress(address);
			//printf("assignFileOffsets(): setBaseAddress(%s, 0x%08llX)\n", curSection->fSectionName, address);
			curSegment->fSize = curSection->getBaseAddress() + curSection->fSize - curSegment->fBaseAddress;
			if ( (fOptions.outputKind() != Options::kObjectFile) || ! curSection->fVirtualSection )
				address += curSection->fSize;
			if ( !curSection->fAllZeroFill ) {
				curSegment->fFileSize = curSegment->fSize;
				fileOffset += curSection->fSize;
			}
		}
		// page align segment size
		curSegment->fFileSize = (curSegment->fFileSize+4095) & (-4096);
		curSegment->fSize	  = (curSegment->fSize+4095) & (-4096);
		if ( curSegment->fBaseAddress == nextContiguousAddress )
			nextContiguousAddress = (curSegment->fBaseAddress+curSegment->fSize+4095) & (-4096);
	}
	
	// check for segment overlaps
	if ( haveFixedSegments ) {
		for(int i=0; i < segCount; ++i) {
			SegmentInfo* segment1 = segmentInfos[i];
			for(int j=0; j < segCount; ++j) {
				if ( i != j ) {
					SegmentInfo* segment2 = segmentInfos[j];
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
					else {
							throwf("segments overlap: %s (0x%08llX + 0x%08llX) and %s (0x%08llX + 0x%08llX)", 
								segment1->fName, segment1->fBaseAddress, segment1->fSize, segment2->fName, segment2->fBaseAddress, segment2->fSize);
					}
				}
			}
		}
	}
}

void Writer::adjustLinkEditSections()
{
	// link edit content is always in last segment
	SegmentInfo* lastSeg = fSegmentInfos[fSegmentInfos.size()-1];
	unsigned int firstLinkEditSectionIndex = 0;
	while ( strcmp(lastSeg->fSections[firstLinkEditSectionIndex]->fSegmentName, "__LINKEDIT") != 0 )
		++firstLinkEditSectionIndex;
	
	const unsigned int sectionCount = lastSeg->fSections.size();
	uint64_t fileOffset = lastSeg->fSections[firstLinkEditSectionIndex]->fFileOffset;
	uint64_t address = lastSeg->fSections[firstLinkEditSectionIndex]->getBaseAddress();
	for (unsigned int i=firstLinkEditSectionIndex; i < sectionCount; ++i) {
		std::vector<class ObjectFile::Atom*>& atoms = lastSeg->fSections[i]->fAtoms;
		const unsigned int atomCount = atoms.size();
		uint64_t sectionOffset = 0;
		lastSeg->fSections[i]->fFileOffset = fileOffset;
		lastSeg->fSections[i]->setBaseAddress(address);
		for (unsigned int j=0; j < atomCount; ++j) {
			ObjectFile::Atom* atom = atoms[j];
			uint64_t alignment = 1 << atom->getAlignment();
			sectionOffset = ( (sectionOffset+alignment-1) & (-alignment) );
			atom->setSectionOffset(sectionOffset);
			sectionOffset += atom->getSize();
		}
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


ObjectFile::Atom::Scope MachHeaderAtom::getScope() const
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

bool MachHeaderAtom::dontStripName() const
{ 
	switch ( fWriter.fOptions.outputKind() ) {
		case Options::kDynamicExecutable:
		case Options::kStaticExecutable:
			return true; 
		case Options::kDynamicLibrary:
		case Options::kDynamicBundle:
		case Options::kDyld:
		case Options::kObjectFile:
			return false; 
	}
	throw "unknown header type";
}

const char* MachHeaderAtom::getName() const
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

const char* MachHeaderAtom::getDisplayName() const
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

uint64_t MachHeaderAtom::getSize() const
{
	return macho_header::size;
}

void MachHeaderAtom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	macho_header mh;

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
		flags = MH_SUBSECTIONS_VIA_SYMBOLS;
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
	}
	
	// get commands info
	uint32_t commandsSize = 0;
	uint32_t commandsCount = 0;
	
	std::vector<class ObjectFile::Atom*>& loadCommandAtoms = fWriter.fLoadCommandsSection->fAtoms;
	const unsigned int atomCount = loadCommandAtoms.size();
	for (unsigned int i=0; i < atomCount; ++i) {
		ObjectFile::Atom* atom = loadCommandAtoms[i];
		commandsSize += atom->getSize();
		// segment and symbol table atoms can contain more than one load command
		if ( atom == fWriter.fSegmentCommands )
			commandsCount += fWriter.fSegmentCommands->commandCount();
		else if ( atom == fWriter.fSymbolTableCommands )
			commandsCount += 2;
		else
			++commandsCount;
	}
	
	// fill out mach_header
	mh.set_magic(macho_header::magic_value);
	mh.set_cputype(fWriter.fOptions.architecture());
#if defined(ARCH_PPC) || defined(ARCH_PPC64) 
	mh.set_cpusubtype(CPU_SUBTYPE_POWERPC_ALL);
#elif defined(ARCH_I386)
	mh.set_cpusubtype(CPU_SUBTYPE_I386_ALL);
#else
	#error unknown architecture
#endif
	mh.set_filetype(fileType);
	mh.set_ncmds(commandsCount);		
	mh.set_sizeofcmds(commandsSize);	
	mh.set_flags(flags);
	mh.set_reserved();
	
	// write it
	writer.write(0, &mh, macho_header::size);
}


CustomStackAtom::CustomStackAtom(Writer& writer) 
 : WriterAtom(writer, fgStackSegment)
{
#if defined(ARCH_PPC) || defined(ARCH_PPC64) || defined(ARCH_I386)
	// stack grows down for these architectures
	fgStackSegment.setBaseAddress(writer.fOptions.customStackAddr() - writer.fOptions.customStackSize());
#else
	#error unknown architecture
#endif
}


void SegmentLoadCommandsAtom::computeSize() 
{
	uint64_t size = 0;
	std::vector<Writer::SegmentInfo*>& segmentInfos = fWriter.fSegmentInfos;
	const int segCount = segmentInfos.size();
	for(int i=0; i < segCount; ++i) {
		size += macho_segment_command::size;
		std::vector<Writer::SectionInfo*>& sectionInfos = segmentInfos[i]->fSections;
		const int sectionCount = sectionInfos.size();		
		for(int j=0; j < sectionCount; ++j) {
			if ( fWriter.fEmitVirtualSections || ! sectionInfos[j]->fVirtualSection )
				size += macho_section::content_size;
		}
	}
	fSize = size;
	fCommandCount = segCount;
}



void SegmentLoadCommandsAtom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	uint64_t size = this->getSize();
	uint8_t buffer[size];
	const bool oneSegment =( fWriter.fOptions.outputKind() == Options::kObjectFile );
	bzero(buffer, fSize);
	uint8_t* p = buffer;
	std::vector<Writer::SegmentInfo*>& segmentInfos = fWriter.fSegmentInfos;
	const int segCount = segmentInfos.size();
	for(int i=0; i < segCount; ++i) {
		Writer::SegmentInfo* segInfo = segmentInfos[i];
		const int sectionCount = segInfo->fSections.size();
		macho_segment_command* cmd = (macho_segment_command*)p;
		cmd->set_cmd(macho_segment_command::command);
		cmd->set_segname(segInfo->fName);
		cmd->set_vmaddr(segInfo->fBaseAddress);
		cmd->set_vmsize(segInfo->fSize);
		cmd->set_fileoff(segInfo->fFileOffset);
		cmd->set_filesize(segInfo->fFileSize);
		cmd->set_maxprot(segInfo->fMaxProtection);
		cmd->set_initprot(segInfo->fInitProtection);
		// add sections array
		macho_section* const sections = (macho_section*)&p[macho_segment_command::size];
		unsigned int sectionsEmitted = 0;
		for (int j=0; j < sectionCount; ++j) {
			Writer::SectionInfo* sectInfo = segInfo->fSections[j];
			if ( fWriter.fEmitVirtualSections || !sectInfo->fVirtualSection ) {
				macho_section* sect = &sections[sectionsEmitted++];
				if ( oneSegment ) {
					// .o files have weird segment range
					if ( sectionsEmitted == 1 ) {
						cmd->set_vmaddr(sectInfo->getBaseAddress());
						cmd->set_fileoff(sectInfo->fFileOffset);
						cmd->set_filesize(segInfo->fFileSize-sectInfo->fFileOffset);
					}
					cmd->set_vmsize(sectInfo->getBaseAddress() + sectInfo->fSize);
				}
				sect->set_sectname(sectInfo->fSectionName);
				sect->set_segname(sectInfo->fSegmentName);
				sect->set_addr(sectInfo->getBaseAddress());
				sect->set_size(sectInfo->fSize);
				sect->set_offset(sectInfo->fFileOffset);
				sect->set_align(sectInfo->fAlignment);
				if ( sectInfo->fRelocCount != 0 ) {
					sect->set_reloff(sectInfo->fRelocOffset * macho_relocation_info::size + fWriter.fLocalRelocationsAtom->getFileOffset());
					sect->set_nreloc(sectInfo->fRelocCount);
				}
				if ( sectInfo->fAllZeroFill ) {
					sect->set_flags(S_ZEROFILL);
				}
				else if ( sectInfo->fAllLazyPointers ) {
					sect->set_flags(S_LAZY_SYMBOL_POINTERS);
					sect->set_reserved1(sectInfo->fIndirectSymbolOffset);
				}
				else if ( sectInfo->fAllNonLazyPointers ) {
					sect->set_flags(S_NON_LAZY_SYMBOL_POINTERS);
					sect->set_reserved1(sectInfo->fIndirectSymbolOffset);
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
			}
		}
		p = &p[macho_segment_command::size + sectionsEmitted*macho_section::content_size];
		cmd->set_cmdsize(macho_segment_command::size + sectionsEmitted*macho_section::content_size);
		cmd->set_nsects(sectionsEmitted);
	}
	writer.write(0, buffer, size);
}


SymbolTableLoadCommandsAtom::SymbolTableLoadCommandsAtom(Writer& writer)
 : WriterAtom(writer, fgTextSegment)
{
	bzero(&fSymbolTable, macho_symtab_command::size);
	bzero(&fDynamicSymbolTable, macho_dysymtab_command::size);
	writer.fSymbolTableCommands = this;
}

uint64_t SymbolTableLoadCommandsAtom::getSize() const
{
	return macho_symtab_command::size + macho_dysymtab_command::size;
}

void SymbolTableLoadCommandsAtom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	// build LC_DYSYMTAB command
	macho_symtab_command   symbolTableCmd;
	bzero(&symbolTableCmd, macho_symtab_command::size);
	symbolTableCmd.set_cmd(LC_SYMTAB);
	symbolTableCmd.set_cmdsize(macho_symtab_command::size);
	symbolTableCmd.set_nsyms(fWriter.fSymbolTableCount);
	symbolTableCmd.set_symoff(fWriter.fSymbolTableAtom->getFileOffset());
	symbolTableCmd.set_stroff(fWriter.fStringsAtom->getFileOffset());
	symbolTableCmd.set_strsize(fWriter.fStringsAtom->getSize());
	writer.write(0, &symbolTableCmd, macho_symtab_command::size);
		
	// build LC_DYSYMTAB command
	macho_dysymtab_command dynamicSymbolTableCmd;
	bzero(&dynamicSymbolTableCmd, macho_dysymtab_command::size);
	dynamicSymbolTableCmd.set_cmd(LC_DYSYMTAB);
	dynamicSymbolTableCmd.set_cmdsize(macho_dysymtab_command::size);
	dynamicSymbolTableCmd.set_ilocalsym(fWriter.fSymbolTableStabsStartIndex);
	dynamicSymbolTableCmd.set_nlocalsym(fWriter.fSymbolTableStabsCount + fWriter.fSymbolTableLocalCount);
	dynamicSymbolTableCmd.set_iextdefsym(fWriter.fSymbolTableExportStartIndex);
	dynamicSymbolTableCmd.set_nextdefsym(fWriter.fSymbolTableExportCount);
	dynamicSymbolTableCmd.set_iundefsym(fWriter.fSymbolTableImportStartIndex);
	dynamicSymbolTableCmd.set_nundefsym(fWriter.fSymbolTableImportCount);
	dynamicSymbolTableCmd.set_indirectsymoff(fWriter.fIndirectTableAtom->getFileOffset());
	dynamicSymbolTableCmd.set_nindirectsyms(fWriter.fIndirectSymbolTable.size());
	if ( fWriter.fOptions.outputKind() != Options::kObjectFile ) {
		dynamicSymbolTableCmd.set_extreloff((fWriter.fExternalRelocs.size()==0) ? 0 : fWriter.fExternalRelocationsAtom->getFileOffset());
		dynamicSymbolTableCmd.set_nextrel(fWriter.fExternalRelocs.size());
		dynamicSymbolTableCmd.set_locreloff((fWriter.fInternalRelocs.size()==0) ? 0 : fWriter.fLocalRelocationsAtom->getFileOffset());
		dynamicSymbolTableCmd.set_nlocrel(fWriter.fInternalRelocs.size());
	}
	writer.write(macho_symtab_command::size, &dynamicSymbolTableCmd, macho_dysymtab_command::size);
}

uint64_t DyldLoadCommandsAtom::getSize() const
{
	uint32_t len = macho_dylinker_command::name_offset + strlen("/usr/lib/dyld");
	len = (len+7) & (-8);	// 8-byte align
	return len;
}

void DyldLoadCommandsAtom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	uint64_t size = this->getSize();
	uint8_t buffer[size];
	bzero(buffer, size);
	macho_dylinker_command* cmd = (macho_dylinker_command*)buffer;
	if ( fWriter.fOptions.outputKind() == Options::kDyld )
		cmd->set_cmd(LC_ID_DYLINKER);
	else
		cmd->set_cmd(LC_LOAD_DYLINKER);
	cmd->set_cmdsize(this->getSize());
	cmd->set_name_offset();
	strcpy((char*)&buffer[macho_dylinker_command::name_offset], "/usr/lib/dyld");
	writer.write(0, buffer, size);
}



uint64_t DylibLoadCommandsAtom::getSize() const
{
	const char* path = fInfo.reader->getInstallPath();
	if ( fInfo.options.fInstallPathOverride != NULL ) 
		path = fInfo.options.fInstallPathOverride;
	uint32_t len = macho_dylib_command::name_offset + strlen(path);
	len = (len+7) & (-8);	// 8-byte align
	return len;
}

void DylibLoadCommandsAtom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	uint64_t size = this->getSize();
	uint8_t buffer[size];
	bzero(buffer, size);
	const char* path = fInfo.reader->getInstallPath();
	if ( fInfo.options.fInstallPathOverride != NULL ) 
		path = fInfo.options.fInstallPathOverride;
	macho_dylib_command* cmd = (macho_dylib_command*)buffer;
	if ( fInfo.options.fWeakImport )
		cmd->set_cmd(LC_LOAD_WEAK_DYLIB);
	else
		cmd->set_cmd(LC_LOAD_DYLIB);
	cmd->set_cmdsize(this->getSize());
	cmd->set_timestamp(fInfo.reader->getTimestamp());
	cmd->set_current_version(fInfo.reader->getCurrentVersion());
	cmd->set_compatibility_version(fInfo.reader->getCompatibilityVersion());
	cmd->set_name_offset();
	strcpy((char*)&buffer[macho_dylib_command::name_offset], path);
	writer.write(0, buffer, size);
}



uint64_t DylibIDLoadCommandsAtom::getSize() const
{
	uint32_t len = macho_dylib_command::name_offset + strlen(fWriter.fOptions.installPath());
	len = (len+7) & (-8);	// 8-byte align
	return len;
}

void DylibIDLoadCommandsAtom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	struct timeval currentTime = { 0 , 0 };
	gettimeofday(&currentTime, NULL);
	time_t timestamp = currentTime.tv_sec;
	uint64_t size = this->getSize();
	uint8_t buffer[size];
	bzero(buffer, size);
	macho_dylib_command* cmd = (macho_dylib_command*)buffer;
	cmd->set_cmd(LC_ID_DYLIB);
	cmd->set_cmdsize(this->getSize());
	cmd->set_name_offset();
	cmd->set_timestamp(timestamp);
	cmd->set_current_version(fWriter.fOptions.currentVersion());
	cmd->set_compatibility_version(fWriter.fOptions.compatibilityVersion());
	strcpy((char*)&buffer[macho_dylib_command::name_offset], fWriter.fOptions.installPath());
	writer.write(0, buffer, size);
}


uint64_t RoutinesLoadCommandsAtom::getSize() const
{
	return macho_routines_command::size;
}

void RoutinesLoadCommandsAtom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	uint64_t initAddr = fWriter.getAtomLoadAddress(fWriter.fEntryPoint);
	uint8_t buffer[macho_routines_command::size];
	bzero(buffer, macho_routines_command::size);
	macho_routines_command* cmd = (macho_routines_command*)buffer;
	cmd->set_cmd(macho_routines_command::command);
	cmd->set_cmdsize(this->getSize());
	cmd->set_init_address(initAddr);
	writer.write(0, buffer, macho_routines_command::size);
}


uint64_t SubUmbrellaLoadCommandsAtom::getSize() const
{
	uint32_t len = macho_sub_umbrella_command::name_offset + strlen(fName);
	len = (len+7) & (-8);	// 8-byte align
	return len;
}

void SubUmbrellaLoadCommandsAtom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	uint64_t size = this->getSize();
	uint8_t buffer[size];
	bzero(buffer, size);
	macho_sub_umbrella_command* cmd = (macho_sub_umbrella_command*)buffer;
	cmd->set_cmd(LC_SUB_UMBRELLA);
	cmd->set_cmdsize(this->getSize());
	cmd->set_name_offset();
	strcpy((char*)&buffer[macho_sub_umbrella_command::name_offset], fName);
	writer.write(0, buffer, size);
}


uint64_t SubLibraryLoadCommandsAtom::getSize() const
{
	uint32_t len = macho_sub_library_command::name_offset + fNameLength + 1;
	len = (len+7) & (-8);	// 8-byte align
	return len;
}

void SubLibraryLoadCommandsAtom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	uint64_t size = this->getSize();
	uint8_t buffer[size];
	bzero(buffer, size);
	macho_sub_library_command* cmd = (macho_sub_library_command*)buffer;
	cmd->set_cmd(LC_SUB_LIBRARY);
	cmd->set_cmdsize(this->getSize());
	cmd->set_name_offset();
	strncpy((char*)&buffer[macho_sub_library_command::name_offset], fNameStart, fNameLength);
	buffer[macho_sub_library_command::name_offset+fNameLength] = '\0';
	writer.write(0, buffer, size);
}

uint64_t UmbrellaLoadCommandsAtom::getSize() const
{
	uint32_t len = macho_sub_framework_command::name_offset + strlen(fName);
	len = (len+7) & (-8);	// 8-byte align
	return len;
}

void UmbrellaLoadCommandsAtom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	uint64_t size = this->getSize();
	uint8_t buffer[size];
	bzero(buffer, size);
	macho_sub_framework_command* cmd = (macho_sub_framework_command*)buffer;
	cmd->set_cmd(LC_SUB_FRAMEWORK);
	cmd->set_cmdsize(this->getSize());
	cmd->set_name_offset();
	strcpy((char*)&buffer[macho_sub_framework_command::name_offset], fName);
	writer.write(0, buffer, size);
}

uint64_t ThreadsLoadCommandsAtom::getSize() const
{
#if defined(ARCH_PPC)
		uint32_t stateSize = 40;		// PPC_THREAD_STATE_COUNT;
#elif defined(ARCH_PPC64)
		uint32_t stateSize = 76;		// PPC_THREAD_STATE64_COUNT;
#elif defined(ARCH_I386)
		uint32_t stateSize = 16;		// i386_THREAD_STATE_COUNT;
#else
	#error unknown architecture
#endif
	return macho_thread_command::size + stateSize*4;
}

void ThreadsLoadCommandsAtom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	uint64_t size = this->getSize();
	uint8_t buffer[size];
	uint64_t start = fWriter.getAtomLoadAddress(fWriter.fEntryPoint);
	bzero(buffer, size);
	macho_thread_command* cmd = (macho_thread_command*)buffer;
	cmd->set_cmd(LC_UNIXTHREAD);
	cmd->set_cmdsize(size);
#if defined(ARCH_PPC)
	cmd->set_flavor(1);				// PPC_THREAD_STATE
	cmd->set_count(40);				// PPC_THREAD_STATE_COUNT;
	cmd->set_threadState32(0, start);
	if ( fWriter.fOptions.hasCustomStack() )
		cmd->set_threadState32(3, fWriter.fOptions.customStackAddr());	// r1
#elif defined(ARCH_PPC64)
	cmd->set_flavor(5);				// PPC_THREAD_STATE64
	cmd->set_count(76);				// PPC_THREAD_STATE64_COUNT;
	cmd->set_threadState64(0, start);
	if ( fWriter.fOptions.hasCustomStack() )
		cmd->set_threadState64(6, fWriter.fOptions.customStackAddr());	// r1
#elif defined(ARCH_I386)
	cmd->set_flavor(0xFFFFFFFF);	// i386_THREAD_STATE
	cmd->set_count(16);				// i386_THREAD_STATE_COUNT;
	cmd->set_threadState32(0, start);
	if ( fWriter.fOptions.hasCustomStack() )
		cmd->set_threadState32(15, fWriter.fOptions.customStackAddr());	// uesp
#else
	#error unknown architecture
#endif
	writer.write(0, buffer, size);
}



uint64_t LoadCommandsPaddingAtom::getSize() const
{
	return fSize;
}

void LoadCommandsPaddingAtom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	uint8_t buffer[fSize];
	bzero(buffer, fSize);
	writer.write(0, buffer, fSize);
}


uint64_t LinkEditAtom::getFileOffset() const
{
	return ((Writer::SectionInfo*)this->getSection())->fFileOffset + this->getSectionOffset();
}


uint64_t LocalRelocationsLinkEditAtom::getSize() const
{
	return fWriter.fInternalRelocs.size() * macho_relocation_info::size;
}

void LocalRelocationsLinkEditAtom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	writer.write(0, &fWriter.fInternalRelocs[0], this->getSize());
}



uint64_t SymbolTableLinkEditAtom::getSize() const
{
	return fWriter.fSymbolTableCount * macho_nlist::size;
}

void SymbolTableLinkEditAtom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	writer.write(0, fWriter.fSymbolTable, this->getSize());
}

uint64_t ExternalRelocationsLinkEditAtom::getSize() const
{
	return fWriter.fExternalRelocs.size() * macho_relocation_info::size;
}

void ExternalRelocationsLinkEditAtom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	writer.write(0, &fWriter.fExternalRelocs[0], this->getSize());
}



uint64_t IndirectTableLinkEditAtom::getSize() const
{
	return fWriter.fIndirectSymbolTable.size() * sizeof(uint32_t);
}

void IndirectTableLinkEditAtom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	uint64_t size = this->getSize();
	uint8_t buffer[size];
	bzero(buffer, size);
	const uint32_t indirectTableSize = fWriter.fIndirectSymbolTable.size();
	uint32_t* indirectTable = (uint32_t*)buffer;
	for (uint32_t i=0; i < indirectTableSize; ++i) {
		Writer::IndirectEntry& entry = fWriter.fIndirectSymbolTable[i];
		if ( entry.indirectIndex < indirectTableSize ) {
			ENDIAN_WRITE32(indirectTable[entry.indirectIndex], entry.symbolIndex);
		}
		else {
			throwf("malformed indirect table. size=%d, index=%d", indirectTableSize, entry.indirectIndex);
		}
	}
	writer.write(0, buffer, size);
}



StringsLinkEditAtom::StringsLinkEditAtom(Writer& writer) 
	: LinkEditAtom(writer), fCurrentBuffer(NULL), fCurrentBufferUsed(0)
{ 
	fCurrentBuffer = new char[kBufferSize];
	// burn first byte of string pool (so zero is never a valid string offset)
	fCurrentBuffer[fCurrentBufferUsed++] = ' ';
	// make offset 1 always point to an empty string
	fCurrentBuffer[fCurrentBufferUsed++] = '\0';
}

uint64_t StringsLinkEditAtom::getSize() const
{
	return kBufferSize * fFullBuffers.size() + fCurrentBufferUsed;
}

void StringsLinkEditAtom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	uint64_t offset = 0;
	for (unsigned int i=0; i < fFullBuffers.size(); ++i) {
		writer.write(offset, fFullBuffers[i], kBufferSize);
		offset += kBufferSize;
	}
	writer.write(offset, fCurrentBuffer, fCurrentBufferUsed);
}

int32_t StringsLinkEditAtom::add(const char* name)
{
	int lenNeeded = strlen(name)+1;
	while ( lenNeeded + fCurrentBufferUsed >= kBufferSize ) {
		// first part of string fits in current buffer
		int firstLen = kBufferSize - fCurrentBufferUsed;
		memcpy(&fCurrentBuffer[fCurrentBufferUsed], name, firstLen);
		// alloc next buffer
		fFullBuffers.push_back(fCurrentBuffer);
		fCurrentBuffer = new char[kBufferSize];
		fCurrentBufferUsed = 0;
		// advance name to second part
		name += firstLen;
		lenNeeded -= firstLen;
	}
	//fprintf(stderr, "StringsLinkEditAtom::add(): lenNeeded=%d, fCurrentBuffer=%d, fCurrentBufferUsed=%d\n", lenNeeded, fCurrentBuffer, fCurrentBufferUsed);
	// string all fits in current buffer
	strcpy(&fCurrentBuffer[fCurrentBufferUsed], name);
	int32_t offset = kBufferSize * fFullBuffers.size() + fCurrentBufferUsed;
	fCurrentBufferUsed += lenNeeded;
	return offset;
}

// returns the index of an empty string
int32_t StringsLinkEditAtom::emptyString()
{
	 return 1;
}

#if defined(ARCH_PPC) || defined(ARCH_PPC64) 
BranchIslandAtom::BranchIslandAtom(Writer& writer, const char* name, int islandRegion, ObjectFile::Atom& target, uint32_t targetOffset) 
 : WriterAtom(writer, fgTextSegment), fTarget(target), fTargetOffset(targetOffset)
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


void BranchIslandAtom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	int64_t displacement = fTarget.getAddress() + fTargetOffset - this->getAddress();
	uint8_t instruction[4];
	int32_t branchInstruction = 0x48000000 | ((uint32_t)displacement & 0x03FFFFFC);
	OSWriteBigInt32(&instruction, 0, branchInstruction);			
	writer.write(0, &instruction, 4);
}


#endif


};



