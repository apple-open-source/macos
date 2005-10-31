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



namespace ObjectFileDylibMachO {

class Reader : public ObjectFile::Reader 
{
public:
												Reader(const macho_header* header, const char* path, const ObjectFile::ReaderOptions& options);
	virtual										~Reader();
												
	virtual const char*								getPath();
	virtual std::vector<class ObjectFile::Atom*>&	getAtoms();
	virtual std::vector<class ObjectFile::Atom*>*	getJustInTimeAtomsFor(const char* name);
	virtual std::vector<ObjectFile::StabsInfo>*		getStabsDebugInfo();
	virtual const char*								getInstallPath();
	virtual uint32_t								getTimestamp();
	virtual uint32_t								getCurrentVersion();
	virtual uint32_t								getCompatibilityVersion();
	virtual std::vector<const char*>*				getDependentLibraryPaths();
	virtual bool									reExports(ObjectFile::Reader*);
	virtual bool									isDefinitionWeak(const ObjectFile::Atom&);
	
private:
	struct CStringComparor
	{
		 bool operator()(const char* left, const char* right) { return (strcmp(left, right) > 0); }
	};
	typedef std::map<const char*, ObjectFile::Atom*, CStringComparor> Mapper;


	void										init(const macho_header* header,const char* path);
	const macho_nlist*							binarySearchWithToc(const char* key, const char stringPool[], const macho_nlist symbols[], const struct dylib_table_of_contents toc[], uint32_t symbolCount);
	const macho_nlist*							binarySearch(const char* key, const char stringPool[], const macho_nlist symbols[], uint32_t symbolCount);
	bool										hasExport(const char* name);
	const macho_nlist*							findExport(const char* name);
	
	const char*									fPath;
	const macho_header*							fHeader;
	const char*									fStrings;
	const macho_dysymtab_command*				fDynamicInfo;
	const macho_dylib_command*					fDylibID;
	const macho_nlist*							fSymbols;
	uint32_t									fSymbolCount;
	Mapper										fAtoms;
	std::vector<Reader*>						fReExportedDylibs;

	static std::vector<class ObjectFile::Atom*>	fEmptyAtomList;
};

std::vector<class ObjectFile::Atom*>	Reader::fEmptyAtomList;


class Segment : public ObjectFile::Segment
{
public:
								Segment(const char* name)		{ fName = name; }
	virtual const char*			getName() const					{ return fName; }
	virtual bool				isContentReadable() const		{ return true; }
	virtual bool				isContentWritable() const		{ return false; }
	virtual bool				isContentExecutable() const		{ return false; }
private:
	const char*					fName;
};


class ExportAtom : public ObjectFile::Atom
{
public:
	virtual ObjectFile::Reader*					getFile() const				{ return &fOwner; }
	virtual const char*							getName() const				{ return fName; }
	virtual const char*							getDisplayName() const;
	virtual Scope								getScope() const			{ return ObjectFile::Atom::scopeGlobal; }
	virtual bool								isTentativeDefinition() const { return false; }
	virtual bool								isWeakDefinition() const	{ return false; }
	virtual bool								isCoalesableByName() const	{ return false; }
	virtual bool								isCoalesableByValue() const { return false; }
	virtual bool								isZeroFill() const			{ return false; }
	virtual bool								dontDeadStrip() const		{ return false; }
	virtual bool								dontStripName() const		{ return false; }
	virtual bool								isImportProxy() const		{ return true; }
	virtual uint64_t							getSize() const				{ return 0; }
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const		{ return fgEmptyReferenceList; }
	virtual bool								mustRemainInSection() const { return false; }
	virtual const char*							getSectionName() const		{ return "._imports"; }
	virtual Segment&							getSegment() const			{ return fgImportSegment; }
	virtual bool								requiresFollowOnAtom() const{ return false; }
	virtual ObjectFile::Atom&					getFollowOnAtom() const		{ return *((ObjectFile::Atom*)NULL); }
	virtual std::vector<ObjectFile::StabsInfo>*	getStabsDebugInfo() const	{ return NULL; }
	virtual uint8_t								getAlignment() const		{ return 0; }
	virtual WeakImportSetting					getImportWeakness() const	{ return fWeakImportSetting; }
	virtual void								copyRawContent(uint8_t buffer[]) const  {}
	virtual void								writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const {}

	virtual void								setScope(Scope)				{ }
	virtual void								setImportWeakness(bool weakImport) { fWeakImportSetting = weakImport ? kWeakImport : kNonWeakImport; }

protected:
	friend class Reader;
	
											ExportAtom(Reader& owner, const char* name)  : fOwner(owner), fName(name), fWeakImportSetting(kWeakUnset) {}
	virtual									~ExportAtom() {}
	
	Reader&									fOwner;
	const char*								fName;
	WeakImportSetting						fWeakImportSetting;
	
	static std::vector<ObjectFile::Reference*>	fgEmptyReferenceList;
	static Segment								fgImportSegment;
};

Segment								ExportAtom::fgImportSegment("__LINKEDIT");
std::vector<ObjectFile::Reference*>	ExportAtom::fgEmptyReferenceList;

const char* ExportAtom::getDisplayName() const
{
	static char temp[300];
	strcpy(temp, fName);
	strcat(temp, "$import");
	return temp;
}



Reader::Reader(const macho_header* header, const char* path, const ObjectFile::ReaderOptions& options)
 : fHeader(header), fStrings(NULL), fDylibID(NULL), fSymbols(NULL), fSymbolCount(0)
{
	typedef std::pair<const macho_dylib_command*, bool> DylibAndReExportFlag;
	std::vector<DylibAndReExportFlag>		dependentDylibs;

	fPath = strdup(path);
	const uint32_t cmd_count = header->ncmds();
	const macho_load_command* const cmds = (macho_load_command*)((char*)header + macho_header::size);
	// get all dylib load commands
	const macho_load_command* cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		switch (cmd->cmd()) {
			case LC_LOAD_DYLIB:
			case LC_LOAD_WEAK_DYLIB:
				{
					DylibAndReExportFlag info;
					info.first = (struct macho_dylib_command*)cmd;
					info.second = options.fFlatNamespace;
					dependentDylibs.push_back(info);
				}
				break;
		}
		cmd = (const macho_load_command*)(((char*)cmd)+cmd->cmdsize());
	}
	
	// cache interesting pointers
	cmd = cmds;
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
				fDynamicInfo = (macho_dysymtab_command*)cmd;
				break;
			case LC_ID_DYLIB:
				fDylibID = (macho_dylib_command*)cmd;
				break;
			case LC_SUB_UMBRELLA:
				if ( !options.fFlatNamespace ) {
					const char* frameworkLeafName = ((macho_sub_umbrella_command*)cmd)->name();
					for (std::vector<DylibAndReExportFlag>::iterator it = dependentDylibs.begin(); it != dependentDylibs.end(); it++) {
						const char* dylibName = it->first->name();
						const char* lastSlash = strrchr(dylibName, '/');
						if ( (lastSlash != NULL) && (strcmp(&lastSlash[1], frameworkLeafName) == 0) )
							it->second = true;
					}
				}
				break;
			case LC_SUB_LIBRARY:
				if ( !options.fFlatNamespace ) {
					const char* dylibBaseName = ((macho_sub_library_command*)cmd)->name();
					for (std::vector<DylibAndReExportFlag>::iterator it = dependentDylibs.begin(); it != dependentDylibs.end(); it++) {
						const char* dylibName = it->first->name();
						const char* lastSlash = strrchr(dylibName, '/');
						const char* leafStart = &lastSlash[1];
						if ( lastSlash == NULL )
							leafStart = dylibName;
						const char* firstDot = strchr(leafStart, '.');
						int len = strlen(leafStart);
						if ( firstDot != NULL )
							len = firstDot - leafStart;
						if ( strncmp(leafStart, dylibBaseName, len) == 0 )
							it->second = true;
					}
				}
				break;
		}
		cmd = (const macho_load_command*)(((char*)cmd)+cmd->cmdsize());
	}
	
	// load dylibs we need to re-export
	for (std::vector<DylibAndReExportFlag>::iterator it = dependentDylibs.begin(); it != dependentDylibs.end(); it++) {
		if ( it->second ) {
			// printf("%s need to re-export %s\n", path, it->first->name());
			//fReExportedDylibs.push_back(
		}
	}
}


Reader::~Reader()
{
}

const char*	 Reader::getPath()
{
	return fPath;
}


std::vector<class ObjectFile::Atom*>&	Reader::getAtoms()
{
	return fEmptyAtomList;
}



const macho_nlist* Reader::binarySearchWithToc(const char* key, const char stringPool[], const macho_nlist symbols[], 
												const struct dylib_table_of_contents toc[], uint32_t symbolCount)
{
	int32_t high = symbolCount-1;
	int32_t mid = symbolCount/2;
		
	for (int32_t low = 0; low <= high; mid = (low+high)/2) {
		const uint32_t index = ENDIAN_READ32(toc[mid].symbol_index);
		const macho_nlist* pivot = &symbols[index];
		const char* pivotStr = &stringPool[pivot->n_strx()];
		int cmp = strcmp(key, pivotStr);
		if ( cmp == 0 )
			return pivot;
		if ( cmp > 0 ) {
			// key > pivot 
			low = mid + 1;
		}
		else {
			// key < pivot 
			high = mid - 1;
		}
	}
	return NULL;
}


const macho_nlist* Reader::binarySearch(const char* key, const char stringPool[], const macho_nlist symbols[], uint32_t symbolCount)
{
	const macho_nlist* base = symbols;
	for (uint32_t n = symbolCount; n > 0; n /= 2) {
		const macho_nlist* pivot = &base[n/2];
		const char* pivotStr = &stringPool[pivot->n_strx()];
		int cmp = strcmp(key, pivotStr);
		if ( cmp == 0 )
			return pivot;
		if ( cmp > 0 ) {
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
	return NULL;
}

const macho_nlist* Reader::findExport(const char* name)
{
	if ( fDynamicInfo->tocoff() == 0 )
		return binarySearch(name, fStrings, &fSymbols[fDynamicInfo->iextdefsym()], fDynamicInfo->nextdefsym());
	else {
		return binarySearchWithToc(name, fStrings, fSymbols, (dylib_table_of_contents*)((char*)fHeader + fDynamicInfo->tocoff()), 
								fDynamicInfo->nextdefsym());
	}
}

bool Reader::hasExport(const char* name)
{
	return ( findExport(name) != NULL );
}

std::vector<class ObjectFile::Atom*>* Reader::getJustInTimeAtomsFor(const char* name)
{
	std::vector<class ObjectFile::Atom*>* atoms = NULL;
	// search exports
	if ( this->hasExport(name) ) {
		// see if this atom already synthesized
		ObjectFile::Atom* atom = NULL;
		Mapper::iterator pos = fAtoms.find(name);
		if ( pos != fAtoms.end() ) {
			atom = pos->second;
		}
		else {
			atom = new ExportAtom(*this, name);
			fAtoms[name] = atom;
		}
		// return a vector of one atom
		atoms = new std::vector<class ObjectFile::Atom*>;
		atoms->push_back(atom);
		return atoms;
	}
	
	// check re-exports
	for (std::vector<Reader*>::iterator it = fReExportedDylibs.begin(); it != fReExportedDylibs.end(); it++) {
		Reader* reExportedReader = *it;
		atoms = reExportedReader->getJustInTimeAtomsFor(name);
		if ( atoms != NULL )
			return atoms;
	}
	
	return NULL;
}


std::vector<ObjectFile::StabsInfo>*	Reader::getStabsDebugInfo()
{
	return NULL;
}

const char*	Reader::getInstallPath()
{
	return fDylibID->name();
}

uint32_t Reader::getTimestamp()
{
	return fDylibID->timestamp();
}

uint32_t Reader::getCurrentVersion()
{
	return fDylibID->current_version();
}

uint32_t Reader::getCompatibilityVersion()
{
	return fDylibID->compatibility_version();
}

std::vector<const char*>* Reader::getDependentLibraryPaths()
{
	std::vector<const char*>* result = new std::vector<const char*>;
	const uint32_t cmd_count = fHeader->ncmds();
	const macho_load_command* const cmds = (macho_load_command*)((char*)fHeader + macho_header::size);
	const macho_load_command* cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		switch (cmd->cmd()) {
			case LC_LOAD_DYLIB:
			case LC_LOAD_WEAK_DYLIB:
				{
					result->push_back(((struct macho_dylib_command*)cmd)->name());
				}
				break;
		}
		cmd = (const macho_load_command*)(((char*)cmd)+cmd->cmdsize());
	}
	return result;
}

bool Reader::reExports(ObjectFile::Reader* child)
{
	// A dependent dylib is re-exported under two conditions:
	//  1) parent contains LC_SUB_UMBRELLA or LC_SUB_LIBRARY with child name
	{
		const uint32_t cmd_count = fHeader->ncmds();
		const macho_load_command* const cmds = (macho_load_command*)((char*)fHeader + macho_header::size);
		const macho_load_command* cmd = cmds;
		for (uint32_t i = 0; i < cmd_count; ++i) {
			switch (cmd->cmd()) {
				case LC_SUB_UMBRELLA:
					{
						const char* frameworkLeafName = ((macho_sub_umbrella_command*)cmd)->name();
						const char* dylibName = child->getPath();
						const char* lastSlash = strrchr(dylibName, '/');
						if ( (lastSlash != NULL) && (strcmp(&lastSlash[1], frameworkLeafName) == 0) )
							return true;
					}
					break;
				case LC_SUB_LIBRARY:
					{
						const char* dylibBaseName = ((macho_sub_library_command*)cmd)->name();
						const char* dylibName = child->getPath();
						const char* lastSlash = strrchr(dylibName, '/');
						const char* leafStart = &lastSlash[1];
						if ( lastSlash == NULL )
							leafStart = dylibName;
						const char* firstDot = strchr(leafStart, '.');
						int len = strlen(leafStart);
						if ( firstDot != NULL )
							len = firstDot - leafStart;
						if ( strncmp(leafStart, dylibBaseName, len) == 0 )
							return true;
					}
					break;
			}
			cmd = (const macho_load_command*)(((char*)cmd)+cmd->cmdsize());
		}
	}
	
	//  2) child contains LC_SUB_FRAMEWORK with parent name 
	{
		const uint32_t cmd_count = ((Reader*)child)->fHeader->ncmds();
		const macho_load_command* const cmds = (macho_load_command*)((char*)(((Reader*)child)->fHeader) + macho_header::size);
		const macho_load_command* cmd = cmds;
		for (uint32_t i = 0; i < cmd_count; ++i) {
			switch (cmd->cmd()) {
				case LC_SUB_FRAMEWORK:
					{
						const char* frameworkLeafName = ((macho_sub_framework_command*)cmd)->name();
						const char* parentName = this->getPath();
						const char* lastSlash = strrchr(parentName, '/');
						if ( (lastSlash != NULL) && (strcmp(&lastSlash[1], frameworkLeafName) == 0) )
							return true;
					}
					break;
			}
			cmd = (const macho_load_command*)(((char*)cmd)+cmd->cmdsize());
		}
	}
	
	
	return false;
}

bool Reader::isDefinitionWeak(const ObjectFile::Atom& atom)
{
	const macho_nlist* sym = findExport(atom.getName());
	if ( sym != NULL ) {
		if ( (sym->n_desc() & N_WEAK_DEF) != 0 )
			return true;
	}
	return false;
}



Reader* MakeReader(const macho_header* mh, const char* path, const ObjectFile::ReaderOptions& options)
{
	return new Reader(mh, path, options);
}



};







