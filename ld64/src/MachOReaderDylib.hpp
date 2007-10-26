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

#ifndef __OBJECT_FILE_DYLIB_MACH_O__
#define __OBJECT_FILE_DYLIB_MACH_O__

#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <sys/param.h>


#include <vector>
#include <set>
#include <algorithm>
#include <ext/hash_map>

#include "MachOFileAbstraction.hpp"
#include "ObjectFile.h"

//
//
//	To implement architecture xxx, you must write template specializations for the following method:
//			Reader<xxx>::validFile()
//
//




namespace mach_o {
namespace dylib {


// forward reference
template <typename A> class Reader;


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


//
// An ExportAtom has no content.  It exists so that the linker can track which imported
// symbols came from which dynamic libraries.
//
template <typename A>
class ExportAtom : public ObjectFile::Atom
{
public:
	virtual ObjectFile::Reader*					getFile() const				{ return &fOwner; }
	virtual bool								getTranslationUnitSource(const char** dir, const char** name) const { return false; }
	virtual const char*							getName() const				{ return fName; }
	virtual const char*							getDisplayName() const		{ return fName; }
	virtual Scope								getScope() const			{ return ObjectFile::Atom::scopeGlobal; }
	virtual DefinitionKind						getDefinitionKind() const	{ return fWeakDefinition ? kExternalWeakDefinition : kExternalDefinition; }
	virtual SymbolTableInclusion				getSymbolTableInclusion() const	{ return ObjectFile::Atom::kSymbolTableIn; }
	virtual	bool								dontDeadStrip() const		{ return false; }
	virtual bool								isZeroFill() const			{ return false; }
	virtual uint64_t							getSize() const				{ return 0; }
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const		{ return fgEmptyReferenceList; }
	virtual bool								mustRemainInSection() const { return false; }
	virtual const char*							getSectionName() const		{ return "._imports"; }
	virtual Segment&							getSegment() const			{ return fgImportSegment; }
	virtual ObjectFile::Atom&					getFollowOnAtom() const		{ return *((ObjectFile::Atom*)NULL); }
	virtual uint32_t							getOrdinal() const			{ return fOrdinal; }
	virtual std::vector<ObjectFile::LineInfo>*	getLineInfo() const			{ return NULL; }
	virtual ObjectFile::Alignment				getAlignment() const		{ return ObjectFile::Alignment(0); }
	virtual void								copyRawContent(uint8_t buffer[]) const  {}

	virtual void								setScope(Scope)				{ }

protected:
	friend class Reader<A>;
	typedef typename A::P					P;

											ExportAtom(ObjectFile::Reader& owner, const char* name, bool weak, uint32_t ordinal)
												: fOwner(owner), fName(name), fOrdinal(ordinal), fWeakDefinition(weak) {}
	virtual									~ExportAtom() {}

	ObjectFile::Reader&						fOwner;
	const char*								fName;
	uint32_t								fOrdinal;
	bool									fWeakDefinition;

	static std::vector<ObjectFile::Reference*>	fgEmptyReferenceList;
	static Segment								fgImportSegment;
};

template <typename A>
Segment								ExportAtom<A>::fgImportSegment("__LINKEDIT");

template <typename A>
std::vector<ObjectFile::Reference*>	ExportAtom<A>::fgEmptyReferenceList;



class ImportReference : public ObjectFile::Reference
{
public:
							ImportReference(const char* name)
								: fTarget(NULL), fTargetName(strdup(name))  {}
	virtual					~ImportReference() {}


	virtual ObjectFile::Reference::TargetBinding	getTargetBinding() const	{ return (fTarget==NULL) ? ObjectFile::Reference::kUnboundByName : ObjectFile::Reference::kBoundByName; }
	virtual ObjectFile::Reference::TargetBinding	getFromTargetBinding() const{ return ObjectFile::Reference::kDontBind; }
	virtual uint8_t									getKind() const				{ return 0; }
	virtual uint64_t								getFixUpOffset() const		{ return 0; }
	virtual const char*								getTargetName() const		{ return fTargetName; }
	virtual ObjectFile::Atom&						getTarget() const			{ return *((ObjectFile::Atom*)fTarget); }
	virtual uint64_t								getTargetOffset() const		{ return 0; }
	virtual ObjectFile::Atom&						getFromTarget() const		{ return *((ObjectFile::Atom*)NULL); }
	virtual const char*								getFromTargetName() const	{ return NULL; }
	virtual uint64_t								getFromTargetOffset() const { return 0; }
	virtual void									setTarget(ObjectFile::Atom& atom, uint64_t offset) { fTarget = &atom; }
	virtual void									setFromTarget(ObjectFile::Atom&) { throw "can't set from target"; }
	virtual const char*								getDescription() const		{ return "dylib import reference"; }

private:
	const ObjectFile::Atom*	fTarget;
	const char*				fTargetName;
};


//
// An ImportAtom has no content.  It exists so that when linking a main executable flat-namespace
// the imports of all flat dylibs are checked
//
template <typename A>
class ImportAtom : public ObjectFile::Atom
{
public:
	virtual ObjectFile::Reader*					getFile() const				{ return &fOwner; }
	virtual bool								getTranslationUnitSource(const char** dir, const char** name) const { return false; }
	virtual const char*							getName() const				{ return "flat-imports"; }
	virtual const char*							getDisplayName() const		{ return "flat_namespace undefines"; }
	virtual Scope								getScope() const			{ return ObjectFile::Atom::scopeTranslationUnit; }
	virtual DefinitionKind						getDefinitionKind() const	{ return kRegularDefinition; }
	virtual SymbolTableInclusion				getSymbolTableInclusion() const	{ return ObjectFile::Atom::kSymbolTableNotIn; }
	virtual	bool								dontDeadStrip() const		{ return false; }
	virtual bool								isZeroFill() const			{ return false; }
	virtual uint64_t							getSize() const				{ return 0; }
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const		{ return (std::vector<ObjectFile::Reference*>&)(fReferences); }
	virtual bool								mustRemainInSection() const { return false; }
	virtual const char*							getSectionName() const		{ return "._imports"; }
	virtual Segment&							getSegment() const			{ return fgImportSegment; }
	virtual ObjectFile::Atom&					getFollowOnAtom() const		{ return *((ObjectFile::Atom*)NULL); }
	virtual uint32_t							getOrdinal() const			{ return fOrdinal; }
	virtual std::vector<ObjectFile::LineInfo>*	getLineInfo() const			{ return NULL; }
	virtual ObjectFile::Alignment				getAlignment() const		{ return ObjectFile::Alignment(0); }
	virtual void								copyRawContent(uint8_t buffer[]) const  {}

	virtual void								setScope(Scope)				{ }

protected:
	friend class Reader<A>;
	typedef typename A::P					P;

											ImportAtom(ObjectFile::Reader& owner, uint32_t ordinal, std::vector<const char*>& imports)
												: fOwner(owner), fOrdinal(ordinal) { makeReferences(imports); }
	virtual									~ImportAtom() {}
	void									makeReferences(std::vector<const char*>& imports) {
												for (std::vector<const char*>::iterator it=imports.begin(); it != imports.end(); ++it) {
													fReferences.push_back(new ImportReference(*it));
												}
											}


	ObjectFile::Reader&						fOwner;
	uint32_t								fOrdinal;
	std::vector<ObjectFile::Reference*>		fReferences;
	
	static Segment							fgImportSegment;
};

template <typename A>
Segment								ImportAtom<A>::fgImportSegment("__LINKEDIT");




//
// The reader for a dylib extracts all exported symbols names from the memory-mapped
// dylib, builds a hash table, then unmaps the file.  This is an important memory
// savings for large dylibs.
//
template <typename A>
class Reader : public ObjectFile::Reader
{
public:
	static bool										validFile(const uint8_t* fileContent, bool executableOrDylib);
													Reader(const uint8_t* fileContent, uint64_t fileLength, const char* path,
														bool executableOrDylib, const ObjectFile::ReaderOptions& options,
														uint32_t ordinalBase);
	virtual											~Reader() {}

	virtual const char*								getPath()					{ return fPath; }
	virtual time_t									getModificationTime()		{ return 0; }
	virtual DebugInfoKind							getDebugInfoKind()			{ return ObjectFile::Reader::kDebugInfoNone; }
	virtual std::vector<class ObjectFile::Atom*>&	getAtoms();
	virtual std::vector<class ObjectFile::Atom*>*	getJustInTimeAtomsFor(const char* name);
	virtual std::vector<Stab>*						getStabs()					{ return NULL; }
	virtual ObjectFile::Reader::ObjcConstraint		getObjCConstraint()			{ return fObjcContraint; }
	virtual const char*								getInstallPath()			{ return fDylibInstallPath; }
	virtual uint32_t								getTimestamp()				{ return fDylibTimeStamp; }
	virtual uint32_t								getCurrentVersion()			{ return fDylibtCurrentVersion; }
	virtual uint32_t								getCompatibilityVersion()	{ return fDylibCompatibilityVersion; }
	virtual void									processIndirectLibraries(DylibHander* handler);
	virtual void									setExplicitlyLinked()		{ fExplicitlyLinked = true; }
	virtual bool									explicitlyLinked()			{ return fExplicitlyLinked; }
	virtual bool									implicitlyLinked()			{ return fImplicitlyLinked; }
	virtual bool									providedExportAtom()		{ return fProvidedAtom; }
	virtual const char*								parentUmbrella()			{ return fParentUmbrella; }
	virtual std::vector<const char*>*				getAllowableClients();

	virtual void									setImplicitlyLinked()		{ fImplicitlyLinked = true; }

protected:

	struct ReExportChain { ReExportChain* prev; Reader<A>* reader; };

	void											assertNoReExportCycles(std::set<ObjectFile::Reader*>& chainedReExportReaders, ReExportChain*);

private:
	typedef typename A::P						P;
	typedef typename A::P::E					E;

	class CStringEquals
	{
	public:
		bool operator()(const char* left, const char* right) const { return (strcmp(left, right) == 0); }
	};
	struct AtomAndWeak { ObjectFile::Atom* atom; bool weak; uint32_t ordinal; };
	typedef __gnu_cxx::hash_map<const char*, AtomAndWeak, __gnu_cxx::hash<const char*>, CStringEquals> NameToAtomMap;
	typedef __gnu_cxx::hash_set<const char*, __gnu_cxx::hash<const char*>, CStringEquals>  NameSet;
	typedef typename NameToAtomMap::iterator		NameToAtomMapIterator;

	struct PathAndFlag { const char* path; bool reExport; };

	bool										isPublicLocation(const char* path);
	void										addSymbol(const char* name, bool weak, uint32_t ordinal);

	const char*									fPath;
	const char*									fParentUmbrella;
	std::vector<const char*>   					fAllowableClients;
	const char*									fDylibInstallPath;
	uint32_t									fDylibTimeStamp;
	uint32_t									fDylibtCurrentVersion;
	uint32_t									fDylibCompatibilityVersion;
	uint32_t									fReExportedOrdinal;
	std::vector<PathAndFlag>					fDependentLibraryPaths;
	NameToAtomMap								fAtoms;
	NameSet										fIgnoreExports;
	bool										fNoRexports;
	const bool									fLinkingFlat;
	const bool									fLinkingMainExecutable;
	bool										fExplictReExportFound;
	bool										fExplicitlyLinked;
	bool										fImplicitlyLinked;
	bool										fProvidedAtom;
	ObjectFile::Reader::ObjcConstraint			fObjcContraint;
	std::vector<ObjectFile::Reader*>   			fReExportedChildren;
	const ObjectFile::ReaderOptions::VersionMin	fDeploymentVersionMin;
	std::vector<class ObjectFile::Atom*>		fFlatImports;

	static bool									fgLogHashtable;
	static std::vector<class ObjectFile::Atom*>	fgEmptyAtomList;
};

template <typename A>
std::vector<class ObjectFile::Atom*>	Reader<A>::fgEmptyAtomList;
template <typename A>
bool									Reader<A>::fgLogHashtable = false;


template <typename A>
Reader<A>::Reader(const uint8_t* fileContent, uint64_t fileLength, const char* path, bool executableOrDylib, 
		const ObjectFile::ReaderOptions& options, uint32_t ordinalBase)
	: fParentUmbrella(NULL), fDylibInstallPath(NULL), fDylibTimeStamp(0), fDylibtCurrentVersion(0), 
	fDylibCompatibilityVersion(0), fLinkingFlat(options.fFlatNamespace), 
	fLinkingMainExecutable(options.fLinkingMainExecutable), fExplictReExportFound(false), 
	fExplicitlyLinked(false), fImplicitlyLinked(false), fProvidedAtom(false), fObjcContraint(ObjectFile::Reader::kObjcNone),
	fDeploymentVersionMin(options.fVersionMin)
{
	// sanity check
	if ( ! validFile(fileContent, executableOrDylib) )
		throw "not a valid mach-o object file";

	fPath = strdup(path);
	
	const macho_header<P>* header = (const macho_header<P>*)fileContent;
	const uint32_t cmd_count = header->ncmds();
	const macho_load_command<P>* const cmds = (macho_load_command<P>*)((char*)header + sizeof(macho_header<P>));

	// write out path for -whatsloaded option
	if ( options.fLogAllFiles )
		printf("%s\n", path);

	if ( options.fRootSafe && ((header->flags() & MH_ROOT_SAFE) == 0) )
		fprintf(stderr, "ld: warning using -root_safe but linking against %s which is not root safe\n", path);

	if ( options.fSetuidSafe && ((header->flags() & MH_SETUID_SAFE) == 0) )
		fprintf(stderr, "ld: warning using -setuid_safe but linking against %s which is not setuid safe\n", path);

	// a "blank" stub has zero load commands
	if ( (header->filetype() == MH_DYLIB_STUB) && (cmd_count == 0) ) {	
		// no further processing needed
		munmap((caddr_t)fileContent, fileLength);
		return;
	}


	// optimize the case where we know there is no reason to look at indirect dylibs
	fNoRexports = (header->flags() & MH_NO_REEXPORTED_DYLIBS);
	bool trackDependentLibraries = !fNoRexports || options.fFlatNamespace;
	
	// pass 1 builds list of all dependent libraries
	const macho_load_command<P>* cmd = cmds;
	if ( trackDependentLibraries ) {
		for (uint32_t i = 0; i < cmd_count; ++i) {
			switch (cmd->cmd()) {
				case LC_REEXPORT_DYLIB:
					fExplictReExportFound = true;
					// fall into next case
				case LC_LOAD_DYLIB:
				case LC_LOAD_WEAK_DYLIB:
					PathAndFlag entry;
					entry.path = strdup(((struct macho_dylib_command<P>*)cmd)->name());
					entry.reExport = (cmd->cmd() == LC_REEXPORT_DYLIB);
					fDependentLibraryPaths.push_back(entry);
					break;
			}
			cmd = (const macho_load_command<P>*)(((char*)cmd)+cmd->cmdsize());
		}
	}
	
	// pass 2 determines re-export info
	const macho_dysymtab_command<P>* dynamicInfo = NULL;
	const macho_nlist<P>* symbolTable = NULL;
	const char*	strings = NULL;
	cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		switch (cmd->cmd()) {
			case LC_SYMTAB:
				{
					const macho_symtab_command<P>* symtab = (macho_symtab_command<P>*)cmd;
					symbolTable = (const macho_nlist<P>*)((char*)header + symtab->symoff());
					strings = (char*)header + symtab->stroff();
				}
				break;
			case LC_DYSYMTAB:
				dynamicInfo = (macho_dysymtab_command<P>*)cmd;
				break;
			case LC_ID_DYLIB:
				{
				macho_dylib_command<P>* dylibID = (macho_dylib_command<P>*)cmd;
				fDylibInstallPath			= strdup(dylibID->name());
				fDylibTimeStamp				= dylibID->timestamp();
				fDylibtCurrentVersion		= dylibID->current_version();
				fDylibCompatibilityVersion	= dylibID->compatibility_version();
				}
				break;
			case LC_SUB_UMBRELLA:
				if ( trackDependentLibraries ) {
					const char* frameworkLeafName = ((macho_sub_umbrella_command<P>*)cmd)->sub_umbrella();
					for (typename std::vector<PathAndFlag>::iterator it = fDependentLibraryPaths.begin(); it != fDependentLibraryPaths.end(); it++) {
						const char* dylibName = it->path;
						const char* lastSlash = strrchr(dylibName, '/');
						if ( (lastSlash != NULL) && (strcmp(&lastSlash[1], frameworkLeafName) == 0) )
							it->reExport = true;
					}
				}
				break;
			case LC_SUB_LIBRARY:
				if ( trackDependentLibraries) {
					const char* dylibBaseName = ((macho_sub_library_command<P>*)cmd)->sub_library();
					for (typename std::vector<PathAndFlag>::iterator it = fDependentLibraryPaths.begin(); it != fDependentLibraryPaths.end(); it++) {
						const char* dylibName = it->path;
						const char* lastSlash = strrchr(dylibName, '/');
						const char* leafStart = &lastSlash[1];
						if ( lastSlash == NULL )
							leafStart = dylibName;
						const char* firstDot = strchr(leafStart, '.');
						int len = strlen(leafStart);
						if ( firstDot != NULL )
							len = firstDot - leafStart;
						if ( strncmp(leafStart, dylibBaseName, len) == 0 )
							it->reExport = true;
					}
				}
				break;
			case LC_SUB_FRAMEWORK:
				fParentUmbrella = strdup(((macho_sub_framework_command<P>*)cmd)->umbrella());
				break;
			case macho_segment_command<P>::CMD:
				// check for Objective-C info
				if ( strcmp(((macho_segment_command<P>*)cmd)->segname(), "__OBJC") == 0 ) {
					const macho_segment_command<P>* segment = (macho_segment_command<P>*)cmd;
					const macho_section<P>* const sectionsStart = (macho_section<P>*)((char*)segment + sizeof(macho_segment_command<P>));
					const macho_section<P>* const sectionsEnd = &sectionsStart[segment->nsects()];
					for (const macho_section<P>* sect=sectionsStart; sect < sectionsEnd; ++sect) {
							if ( strcmp(sect->sectname(), "__image_info") == 0 ) {
							//	struct objc_image_info  {
							//		uint32_t	version;	// initially 0
							//		uint32_t	flags;
							//	};
							// #define OBJC_IMAGE_SUPPORTS_GC   2
							// #define OBJC_IMAGE_GC_ONLY       4
							//
							const uint32_t* contents = (uint32_t*)(&fileContent[sect->offset()]);
							if ( (sect->size() >= 8) && (contents[0] == 0) ) {
								uint32_t flags = E::get32(contents[1]);
								if ( (flags & 4) == 4 )
									fObjcContraint = ObjectFile::Reader::kObjcGC;
								else if ( (flags & 2) == 2 )
									fObjcContraint = ObjectFile::Reader::kObjcRetainReleaseOrGC;
								else
									fObjcContraint = ObjectFile::Reader::kObjcRetainRelease;
							}
							else if ( sect->size() > 0 ) {
								fprintf(stderr, "ld: warning, can't parse __OBJC/__image_info section in %s\n", fPath);
							}
						}
					}
				}
		}

		cmd = (const macho_load_command<P>*)(((char*)cmd)+cmd->cmdsize());
	}
	
	// Process the rest of the commands here.
	cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		switch (cmd->cmd()) {
		case LC_SUB_CLIENT:
			const char *temp = strdup(((macho_sub_client_command<P>*)cmd)->client());
			fAllowableClients.push_back(temp);
			break;
		}
		cmd = (const macho_load_command<P>*)(((char*)cmd)+cmd->cmdsize());
	}

	// validate minimal load commands
	if ( (fDylibInstallPath == NULL) && (header->filetype() != MH_EXECUTE) ) 
		throw "dylib missing LC_ID_DYLIB load command";
	if ( symbolTable == NULL )
		throw "dylib missing LC_SYMTAB load command";
	if ( dynamicInfo == NULL )
		throw "dylib missing LC_DYSYMTAB load command";
	
	// if linking flat and this is a flat dylib, create one atom that references all imported symbols
	if ( fLinkingFlat && fLinkingMainExecutable && ((header->flags() & MH_TWOLEVEL) == 0) ) {
		std::vector<const char*> importNames;
		importNames.reserve(dynamicInfo->nundefsym());
		const macho_nlist<P>* start = &symbolTable[dynamicInfo->iundefsym()];
		const macho_nlist<P>* end = &start[dynamicInfo->nundefsym()];
		for (const macho_nlist<P>* sym=start; sym < end; ++sym) {
			importNames.push_back(&strings[sym->n_strx()]);
		}
		fFlatImports.push_back(new ImportAtom<A>(*this, ordinalBase++, importNames));
	}

	// build hash table
	if ( dynamicInfo->tocoff() == 0 ) {
		if ( fgLogHashtable ) fprintf(stderr, "ld: building hashtable of %u toc entries for %s\n", dynamicInfo->nextdefsym(), path);
		const macho_nlist<P>* start = &symbolTable[dynamicInfo->iextdefsym()];
		const macho_nlist<P>* end = &start[dynamicInfo->nextdefsym()];
		fAtoms.resize(dynamicInfo->nextdefsym()); // set initial bucket count
		uint32_t index = ordinalBase;
		for (const macho_nlist<P>* sym=start; sym < end; ++sym, ++index) {
			this->addSymbol(&strings[sym->n_strx()], (sym->n_desc() & N_WEAK_DEF) != 0, index);
		}
		fReExportedOrdinal = index;
	}
	else {
		int32_t count = dynamicInfo->ntoc();
		fAtoms.resize(count); // set initial bucket count
		if ( fgLogHashtable ) fprintf(stderr, "ld: building hashtable of %u entries for %s\n", count, path);
		const struct dylib_table_of_contents* toc = (dylib_table_of_contents*)((char*)header + dynamicInfo->tocoff());
		for (int32_t i = 0; i < count; ++i) {
			const uint32_t index = E::get32(toc[i].symbol_index);
			const macho_nlist<P>* sym = &symbolTable[index];
			this->addSymbol(&strings[sym->n_strx()], (sym->n_desc() & N_WEAK_DEF) != 0, ordinalBase+i);
		}
		fReExportedOrdinal = ordinalBase + count;
	}

	
	// unmap file
	munmap((caddr_t)fileContent, fileLength);
}



template <typename A>
void Reader<A>::addSymbol(const char* name, bool weak, uint32_t ordinal)
{
	// symbols that start with $ld$ are meta-data to the static linker
	// <rdar://problem/5182537> need way for ld and dyld to see different exported symbols in a dylib
	if ( strncmp(name, "$ld$", 4) == 0 ) {	
		//    $ld$ <action> $ <condition> $ <symbol-name>
		const char* symAction = &name[4];
		const char* symCond = strchr(symAction, '$');
		if ( symCond != NULL ) {
			ObjectFile::ReaderOptions::VersionMin symVersionCondition = ObjectFile::ReaderOptions::kMinUnset;
			if ( (strncmp(symCond, "$os10.", 6) == 0) && isdigit(symCond[6]) && (symCond[7] == '$') ) {
				switch ( symCond[6] - '0' ) {
					case 0:
					case 1:
						symVersionCondition = ObjectFile::ReaderOptions::k10_1;
						break;
					case 2:
						symVersionCondition = ObjectFile::ReaderOptions::k10_2;
						break;
					case 3:
						symVersionCondition = ObjectFile::ReaderOptions::k10_3;
						break;
					case 4:
						symVersionCondition = ObjectFile::ReaderOptions::k10_4;
						break;
					case 5:
						symVersionCondition = ObjectFile::ReaderOptions::k10_5;
						break;
				}
				const char* symName = strchr(&symCond[1], '$');
				if ( symName != NULL ) {
					++symName;
					if ( fDeploymentVersionMin == symVersionCondition ) {
						if ( strncmp(symAction, "hide$", 5) == 0 ) {
							if ( fgLogHashtable ) fprintf(stderr, "  adding %s to ignore set for %s\n", symName, this->getPath());
							fIgnoreExports.insert(strdup(symName));
							return;
						}
						else if ( strncmp(symAction, "add$", 4) == 0 ) {
							this->addSymbol(symName, weak, ordinal);
							return;
						}
						else {
							fprintf(stderr, "ld: warning bad symbol action: %s in dylib %s\n", name, this->getPath());
						}
					}
				}
				else {
					fprintf(stderr, "ld: warning bad symbol name: %s in dylib %s\n", name, this->getPath());
				}
			}
			else {
				fprintf(stderr, "ld: warning bad symbol version: %s in dylib %s\n", name, this->getPath());
			}
		}	
		else {
			fprintf(stderr, "ld: warning bad symbol condition: %s in dylib %s\n", name, this->getPath());
		}
	}
	
	// add symbol as possible export if we are not supposed to ignore it
	if ( fIgnoreExports.count(name) == 0 ) {
		AtomAndWeak bucket;
		bucket.atom = NULL;
		bucket.weak = weak;
		bucket.ordinal = ordinal;
		if ( fgLogHashtable ) fprintf(stderr, "  adding %s to hash table for %s\n", name, this->getPath());
		fAtoms[strdup(name)] = bucket;
	}
}


template <typename A>
std::vector<class ObjectFile::Atom*>& Reader<A>::getAtoms()
{
	return fFlatImports;
}


template <typename A>
std::vector<class ObjectFile::Atom*>* Reader<A>::getJustInTimeAtomsFor(const char* name)
{
	std::vector<class ObjectFile::Atom*>* atoms = NULL;
	
	NameToAtomMapIterator pos = fAtoms.find(name);
	if ( pos != fAtoms.end() ) {
		if ( pos->second.atom == NULL ) {
			// instantiate atom and update hash table
			pos->second.atom = new ExportAtom<A>(*this, name, pos->second.weak, pos->second.ordinal);
			fProvidedAtom = true;
			if ( fgLogHashtable ) fprintf(stderr, "getJustInTimeAtomsFor: %s found in %s\n", name, this->getPath());
		}
		// return a vector of one atom
		atoms = new std::vector<class ObjectFile::Atom*>;
		atoms->push_back(pos->second.atom);
	}
	else {
		if ( fgLogHashtable ) fprintf(stderr, "getJustInTimeAtomsFor: %s NOT found in %s\n", name, this->getPath());
			// if not supposed to ignore this export, see if I have it
		if ( fIgnoreExports.count(name) == 0 ) {
			// look in children that I re-export
			for (std::vector<ObjectFile::Reader*>::iterator it = fReExportedChildren.begin(); it != fReExportedChildren.end(); it++) {
				//fprintf(stderr, "getJustInTimeAtomsFor: %s NOT found in %s, looking in child %s\n", name, this->getPath(), (*it)->getInstallPath());
				std::vector<class ObjectFile::Atom*>* childAtoms = (*it)->getJustInTimeAtomsFor(name);
				if ( childAtoms != NULL ) {
					// make a new atom that says this reader is the owner 
					bool isWeakDef = (childAtoms->at(0)->getDefinitionKind() == ObjectFile::Atom::kExternalWeakDefinition);
					// return a vector of one atom
					ExportAtom<A>* newAtom = new ExportAtom<A>(*this, name, isWeakDef, fReExportedOrdinal++);
					fProvidedAtom = true;
					atoms = new std::vector<class ObjectFile::Atom*>;
					atoms->push_back(newAtom);
					delete childAtoms;
					return atoms;
				}
			}
		}
	}
	return atoms;
}



template <typename A>
bool Reader<A>::isPublicLocation(const char* path)
{
	// /usr/lib is a public location
	if ( (strncmp(path, "/usr/lib/", 9) == 0) && (strchr(&path[9], '/') == NULL) )
		return true;

	// /System/Library/Frameworks/ is a public location
	if ( strncmp(path, "/System/Library/Frameworks/", 27) == 0 ) {
		const char* frameworkDot = strchr(&path[27], '.');
		// but only top level framework
		// /System/Library/Frameworks/Foo.framework/Versions/A/Foo                 ==> true
		// /System/Library/Frameworks/Foo.framework/Resources/libBar.dylib         ==> false
		// /System/Library/Frameworks/Foo.framework/Frameworks/Bar.framework/Bar   ==> false
		// /System/Library/Frameworks/Foo.framework/Frameworks/Xfoo.framework/XFoo ==> false
		if ( frameworkDot != NULL ) {
			int frameworkNameLen = frameworkDot - &path[27];
			if ( strncmp(&path[strlen(path)-frameworkNameLen-1], &path[26], frameworkNameLen+1) == 0 )
				return true;
		}
	}
		
	return false;
}

template <typename A>
void Reader<A>::processIndirectLibraries(DylibHander* handler)
{
	if ( fLinkingFlat ) {
		for (typename std::vector<PathAndFlag>::iterator it = fDependentLibraryPaths.begin(); it != fDependentLibraryPaths.end(); it++) {
			handler->findDylib(it->path, this->getPath());
		}
	}
	else if ( fNoRexports ) {
		// MH_NO_REEXPORTED_DYLIBS bit set, then nothing to do
	}
	else {
		// two-level, might have re-exports
		for (typename std::vector<PathAndFlag>::iterator it = fDependentLibraryPaths.begin(); it != fDependentLibraryPaths.end(); it++) {
			if ( it->reExport ) {
				//fprintf(stderr, "processIndirectLibraries() parent=%s, child=%s\n", this->getInstallPath(), it->path);
				// a LC_REEXPORT_DYLIB, LC_SUB_UMBRELLA or LC_SUB_LIBRARY says we re-export this child
				ObjectFile::Reader* child = handler->findDylib(it->path, this->getPath());
				if ( isPublicLocation(child->getInstallPath()) ) {
					// promote this child to be automatically added as a direct dependent if this already is
					if ( this->explicitlyLinked() || this->implicitlyLinked() ) {
						//fprintf(stderr, "processIndirectLibraries() implicitly linking %s\n", child->getInstallPath());
						((Reader<A>*)child)->setImplicitlyLinked();
					}
					else
						fReExportedChildren.push_back(child);
				}
				else {
					// add all child's symbols to me
					fReExportedChildren.push_back(child);
					//fprintf(stderr, "processIndirectLibraries() parent=%s will re-export child=%s\n", this->getInstallPath(), it->path);
				}
			}
			else if ( !fExplictReExportFound ) {
				// see if child contains LC_SUB_FRAMEWORK with my name
				ObjectFile::Reader* child = handler->findDylib(it->path, this->getPath());
				const char* parentUmbrellaName = ((Reader<A>*)child)->parentUmbrella();
				if ( parentUmbrellaName != NULL ) {
					const char* parentName = this->getPath();
					const char* lastSlash = strrchr(parentName, '/');
					if ( (lastSlash != NULL) && (strcmp(&lastSlash[1], parentUmbrellaName) == 0) ) {
						// add all child's symbols to me
						fReExportedChildren.push_back(child);
						//fprintf(stderr, "processIndirectLibraries() umbrella=%s will re-export child=%s\n", this->getInstallPath(), it->path);
					}
				}
			}
		}
	}
	
	// check for re-export cycles
	std::set<ObjectFile::Reader*> chainedReExportReaders;
	ReExportChain chain;
	chain.prev = NULL;
	chain.reader = this;
	this->assertNoReExportCycles(chainedReExportReaders, &chain);
}

template <typename A>
void Reader<A>::assertNoReExportCycles(std::set<ObjectFile::Reader*>& chainedReExportReaders, ReExportChain* prev)
{
	// check none of my re-exported dylibs are already in set
	for (std::vector<ObjectFile::Reader*>::iterator it = fReExportedChildren.begin(); it != fReExportedChildren.end(); it++) {
		if ( chainedReExportReaders.count(*it) != 0 ) {
			// we may want to print out the chain of dylibs causing the cylce...
			throwf("cycle in dylib re-exports with %s", this->getPath());
		}
	}
	// recursively check my re-exportted dylibs
	chainedReExportReaders.insert(this);
	ReExportChain chain;
	chain.prev = prev;
	chain.reader = this;
	for (std::vector<ObjectFile::Reader*>::iterator it = fReExportedChildren.begin(); it != fReExportedChildren.end(); it++) {
		((Reader<A>*)(*it))->assertNoReExportCycles(chainedReExportReaders, &chain);
	}
}


template <typename A>
std::vector<const char*>* Reader<A>::getAllowableClients()
{
	std::vector<const char*>* result = new std::vector<const char*>;
	for (typename std::vector<const char*>::iterator it = fAllowableClients.begin();
		 it != fAllowableClients.end();
		 it++) {
		result->push_back(*it);
	}
	return (fAllowableClients.size() != 0 ? result : NULL);
}

template <>
bool Reader<ppc>::validFile(const uint8_t* fileContent, bool executableOrDylib)
{
	const macho_header<P>* header = (const macho_header<P>*)fileContent;
	if ( header->magic() != MH_MAGIC )
		return false;
	if ( header->cputype() != CPU_TYPE_POWERPC )
		return false;
	switch ( header->filetype() ) {
		case MH_DYLIB:
		case MH_DYLIB_STUB:
			return true;
		case MH_EXECUTE:
			return executableOrDylib;
		default:
			return false;
	}
}

template <>
bool Reader<ppc64>::validFile(const uint8_t* fileContent, bool executableOrDylib)
{
	const macho_header<P>* header = (const macho_header<P>*)fileContent;
	if ( header->magic() != MH_MAGIC_64 )
		return false;
	if ( header->cputype() != CPU_TYPE_POWERPC64 )
		return false;
	switch ( header->filetype() ) {
		case MH_DYLIB:
		case MH_DYLIB_STUB:
			return true;
		case MH_EXECUTE:
			return executableOrDylib;
		default:
			return false;
	}
}

template <>
bool Reader<x86>::validFile(const uint8_t* fileContent, bool executableOrDylib)
{
	const macho_header<P>* header = (const macho_header<P>*)fileContent;
	if ( header->magic() != MH_MAGIC )
		return false;
	if ( header->cputype() != CPU_TYPE_I386 )
		return false;
	switch ( header->filetype() ) {
		case MH_DYLIB:
		case MH_DYLIB_STUB:
			return true;
		case MH_EXECUTE:
			return executableOrDylib;
		default:
			return false;
	}
}

template <>
bool Reader<x86_64>::validFile(const uint8_t* fileContent, bool executableOrDylib)
{
	const macho_header<P>* header = (const macho_header<P>*)fileContent;
	if ( header->magic() != MH_MAGIC_64 )
		return false;
	if ( header->cputype() != CPU_TYPE_X86_64 )
		return false;
	switch ( header->filetype() ) {
		case MH_DYLIB:
		case MH_DYLIB_STUB:
			return true;
		case MH_EXECUTE:
			return executableOrDylib;
		default:
			return false;
	}
}




}; // namespace dylib
}; // namespace mach_o


#endif // __OBJECT_FILE_DYLIB_MACH_O__
