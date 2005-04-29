/*
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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>

#include <string>
#include <set>
#include <string>
#include <vector>
#include <list>
#include <algorithm>
#include <ext/hash_map>

#include "Options.h"

#include "ObjectFile.h"
#include "ObjectFileMachO-all.h"

#include "ExecutableFile.h"
#include "ExecutableFileMachO-all.h"

#include "SectCreate.h"

#if 0
static void dumpAtom(ObjectFile::Atom* atom)
{
	//printf("atom:    %p\n", atom);
	
	// name
	printf("name:    %s\n",  atom->getDisplayName());
	
	// scope
	switch ( atom->getScope() ) {
		case ObjectFile::Atom::scopeTranslationUnit:
			printf("scope:   translation unit\n");
			break;
		case ObjectFile::Atom::scopeLinkageUnit:
			printf("scope:   linkage unit\n");
			break;
		case ObjectFile::Atom::scopeGlobal:
			printf("scope:   global\n");
			break;
		default:
			printf("scope:   unknown\n");
	}
	
	// segment and section
	printf("section: %s,%s\n", atom->getSegment().getName(), atom->getSectionName());

	// attributes
	printf("attrs:   ");
	if ( atom->isTentativekDefinition() )
		printf("tentative ");
	else if ( atom->isWeakDefinition() )
		printf("weak ");
	if ( atom->isCoalesableByName() )
		printf("coalesce-by-name ");
	if ( atom->isCoalesableByValue() )
		printf("coalesce-by-value ");
	if ( atom->dontDeadStrip() )
		printf("dont-dead-strip ");
	if ( atom->isZeroFill() )
		printf("zero-fill ");
	printf("\n");
	
	// size
	printf("size:    0x%012llX\n", atom->getSize());
	
	// content
	uint8_t content[atom->getSize()];
	atom->copyRawContent(content);
	printf("content: ");
	if ( strcmp(atom->getSectionName(), "__cstring") == 0 ) {
		printf("\"%s\"", content);
	}
	else {
		for (unsigned int i=0; i < sizeof(content); ++i)
			printf("%02X ", content[i]);
	}
	printf("\n");
	
	// references
	std::vector<ObjectFile::Reference*>&  references = atom->getReferences();
	const int refCount = references.size();
	printf("references: (%u)\n", refCount);
	for (int i=0; i < refCount; ++i) {
		ObjectFile::Reference* ref = references[i];
		printf("   %s\n", ref->getDescription());
	}
	
	// attributes
	
}

#endif

class CStringComparor
{
public:
	bool operator()(const char* left, const char* right) const { return (strcmp(left, right) < 0); }
};

class CStringEquals
{
public:
	bool operator()(const char* left, const char* right) const { return (strcmp(left, right) == 0); }
};

class Section : public ObjectFile::Section
{
public:
	static Section*	find(const char* sectionName, const char* segmentName, bool zeroFill);
	static void		assignIndexes();

private:
					Section(const char* sectionName, const char* segmentName, bool zeroFill);

	struct Sorter {
		static int	segmentOrdinal(const char* segName);
		bool operator()(Section* left, Section* right);
	};

	typedef __gnu_cxx::hash_map<const char*, class Section*, __gnu_cxx::hash<const char*>, CStringEquals> NameToSection;
	//typedef std::map<const char*, class Section*, CStringComparor> NameToSection;

	const char*		fSectionName;
	const char*		fSegmentName;
	bool			fZeroFill;

	static NameToSection			fgMapping;
	static std::vector<Section*>	fgSections;
};

Section::NameToSection	Section::fgMapping;
std::vector<Section*>	Section::fgSections;

Section::Section(const char* sectionName, const char* segmentName, bool zeroFill)
 : fSectionName(sectionName), fSegmentName(segmentName), fZeroFill(zeroFill)
{
	//fprintf(stderr, "new Section(%s, %s)\n", sectionName, segmentName);
}

Section* Section::find(const char* sectionName, const char* segmentName, bool zeroFill)
{
#if 0
	std::pair<NameToSection::iterator, NameToSection::iterator> range = fgMapping.equal_range(sectionName);
	for (NameToSection::iterator it=range.first; it != range.second; it++) {
		if ( strcmp(it->second->fSegmentName, segmentName) == 0 )
			return it->second;
	}
#endif
	NameToSection::iterator pos = fgMapping.find(sectionName);
	if ( pos != fgMapping.end() ) {
		if ( strcmp(pos->second->fSegmentName, segmentName) == 0 )
			return pos->second;
		// otherwise same section name is used in different segments, look slow way
		for (std::vector<Section*>::iterator it=fgSections.begin(); it != fgSections.end(); it++) {
			if ( (strcmp((*it)->fSectionName, sectionName) == 0) && (strcmp((*it)->fSegmentName, segmentName) == 0) )
				return *it;
		}
	}
	
	// does not exist, so make a new one
	Section* sect = new Section(sectionName, segmentName, zeroFill);
	sect->fIndex = fgMapping.size();
	fgMapping[sectionName] = sect;
	fgSections.push_back(sect);
	return sect;
}

int Section::Sorter::segmentOrdinal(const char* segName)
{
	if ( strcmp(segName, "__PAGEZERO") == 0 )
		return 1;
	if ( strcmp(segName, "__TEXT") == 0 )
		return 2;
	if ( strcmp(segName, "__DATA") == 0 )
		return 3;
	if ( strcmp(segName, "__OBJC") == 0 )
		return 4;
	if ( strcmp(segName, "__LINKEDIT") == 0 )
		return INT_MAX;	// linkedit segment should always sort last
	else 
		return 5;
}


bool Section::Sorter::operator()(Section* left, Section* right)
{
	// Segment is primary sort key
	const char* leftSegName = left->fSegmentName;
	const char* rightSegName = right->fSegmentName;
	int segNameCmp = strcmp(leftSegName, rightSegName);
	if ( segNameCmp != 0 )
	{
		int leftSegOrdinal = segmentOrdinal(leftSegName);
		int rightSegOrdinal = segmentOrdinal(rightSegName);
		if ( leftSegOrdinal < rightSegOrdinal )
			return true;
		if ( leftSegOrdinal == rightSegOrdinal )
			return segNameCmp < 0;
		return false;
	}

	// zerofill section sort to the end
	if ( !left->fZeroFill && right->fZeroFill )
		return true;
	if ( left->fZeroFill && !right->fZeroFill )
		return false;

	// section discovery order is last sort key
	return left->fIndex < right->fIndex;
}

void Section::assignIndexes()
{
	//printf("unsorted:\n");
	//for (std::vector<Section*>::iterator it=fgSections.begin(); it != fgSections.end(); it++) {
	//	printf("section: name=%s, segment: name=%s, discovery order=%d\n", (*it)->fSectionName, (*it)->fSegmentName, (*it)->fIndex);
	//}

	// sort it
	std::sort(fgSections.begin(), fgSections.end(), Section::Sorter());
	
	// assign correct section ordering to each Section object
	unsigned int newOrder = 1;
	for (std::vector<Section*>::iterator it=fgSections.begin(); it != fgSections.end(); it++) 
		(*it)->fIndex = newOrder++;

	//printf("sorted:\n");
	//for (std::vector<Section*>::iterator it=fgSections.begin(); it != fgSections.end(); it++) {
	//	printf("section: name=%s\n", (*it)->fSectionName);
	//}
}

class Linker {
public:
						Linker(int argc, const char* argv[]);

	void				createReaders();
	void				createWriter();
	void				addInputFile(ObjectFile::Reader* reader);
	void				setOutputFile(ExecutableFile::Writer* writer);
	void				link();
	
	
private:
	ObjectFile::Reader*	createReader(const Options::FileInfo&);
	void				addAtom(ObjectFile::Atom& atom);
	void				addAtoms(std::vector<class ObjectFile::Atom*>& atoms);
	void				buildAtomList();
	void				loadUndefines();
	void				addWeakAtomOverrides();
	void				resolveReferences();
	void				deadStrip();
	void				sortAtoms();
	void				tweakLayout();
	void				writeOutput();
	
	void				resolve(ObjectFile::Reference* reference);
	void				addJustInTimeAtoms(const char* name);
	
	void				addDylib(ObjectFile::Reader* reader, const Options::FileInfo& info);
	void				addIndirectLibraries(ObjectFile::Reader* reader);
	bool				haveIndirectLibrary(const char* path, ObjectFile::Reader* reader);
	bool				haveDirectLibrary(const char* path);

	struct SegmentAndItsAtoms
	{
		class Segment*						fSegment;
		uint64_t							fSegmentSize;
		uint64_t							fSegmentBaseAddress;
		std::vector<class ObjectFile::Atom*>	fAtoms;
	};
	
	
	class SymbolTable
	{
	public:
							SymbolTable(Linker&);
		void				require(const char* name);
		bool				add(ObjectFile::Atom& atom);
		ObjectFile::Atom*	find(const char* name);
		unsigned int		getRequireCount() { return fRequireCount; }
		void				getNeededNames(bool andWeakDefintions, std::vector<const char*>& undefines);
	private:
		typedef std::map<const char*, ObjectFile::Atom*, CStringComparor> Mapper;
		Linker&				fOwner;
		Mapper				fTable;
		unsigned int		fRequireCount;
	};
	
	struct AtomSorter
	{
		bool operator()(ObjectFile::Atom* left, ObjectFile::Atom* right);
	};
	
	typedef std::map<const char*, uint32_t, CStringComparor> SectionOrder;
	
	struct IndirectLibrary {
		const char*							path;
		uint64_t							fileLen;
		ObjectFile::Reader*					reader;
		std::set<ObjectFile::Reader*>		parents;
		ObjectFile::Reader*					reExportParent;
	};
	
	Options												fOptions;
	SymbolTable											fGlobalSymbolTable;
	unsigned int										fWeakSymbolsAddedCount;
	std::vector<class ObjectFile::Reader*>				fInputFiles;
	ExecutableFile::Writer*								fOutputFile;
	std::vector<ExecutableFile::DyLibUsed>				fDynamicLibraries;
	std::list<IndirectLibrary>							fIndirectDynamicLibraries;
	std::vector<class ObjectFile::Atom*>				fAllAtoms;
	std::vector< SegmentAndItsAtoms >					fAllAtomsBySegment;
	std::set<class ObjectFile::Atom*>					fDeadAtoms;
	SectionOrder										fSectionOrder;
	unsigned int										fNextSortOrder;
	bool												fDirectLibrariesComplete;
};



Linker::Linker(int argc, const char* argv[])
 : fOptions(argc, argv), fGlobalSymbolTable(*this), fOutputFile(NULL), fNextSortOrder(1), fDirectLibrariesComplete(false)
{
}

void Linker::addInputFile(ObjectFile::Reader* reader)
{
	fInputFiles.push_back(reader);
}

void Linker::setOutputFile(ExecutableFile::Writer* writer)
{
	fOutputFile = writer;
}

void Linker::link()
{		
	this->buildAtomList();
	this->loadUndefines();
	this->resolveReferences();
	this->deadStrip();
	this->sortAtoms();
	this->tweakLayout();
	this->writeOutput();
}

inline void Linker::addAtom(ObjectFile::Atom& atom)
{
	// add to list of all atoms
	fAllAtoms.push_back(&atom);
	
	// add atom's references's names to symbol table as to-be-resolved-later
	std::vector<class ObjectFile::Reference*>& references = atom.getReferences();
	for (std::vector<ObjectFile::Reference*>::iterator it=references.begin(); it != references.end(); it++) {
		ObjectFile::Reference* reference = *it;
		if ( reference->isUnbound() ) {
			fGlobalSymbolTable.require(reference->getTargetName());
		}
	}
	
	// if in global namespace, add atom itself to symbol table
	ObjectFile::Atom::Scope scope = atom.getScope();
	const char* name = atom.getName();
	if ( (scope != ObjectFile::Atom::scopeTranslationUnit) && (name != NULL) ) {
		fGlobalSymbolTable.add(atom);
		
		// update scope based on export list (possible that globals are downgraded to private_extern)
		if ( (scope == ObjectFile::Atom::scopeGlobal) && fOptions.hasExportRestrictList() ) {
			bool doExport = fOptions.shouldExport(name);
			if ( !doExport ) {
				atom.setScope(ObjectFile::Atom::scopeLinkageUnit);
			}
		}
	}
		
	// record section orders so output file can have same order
	atom.setSection(Section::find(atom.getSectionName(), atom.getSegment().getName(), atom.isZeroFill()));
	
	// assign order in which this atom was originally seen
	if ( atom.getSortOrder() == 0 )
		fNextSortOrder = atom.setSortOrder(fNextSortOrder);
}

inline void Linker::addAtoms(std::vector<class ObjectFile::Atom*>& atoms)
{
	for (std::vector<ObjectFile::Atom*>::iterator it=atoms.begin(); it != atoms.end(); it++) {
		this->addAtom(**it);
	}
}

void Linker::buildAtomList()
{
	// add initial undefines from -u option
	std::vector<const char*>& initialUndefines = fOptions.initialUndefines();
	for (std::vector<const char*>::iterator it=initialUndefines.begin(); it != initialUndefines.end(); it++) {
		fGlobalSymbolTable.require(*it);
	}
	
	// writer can contribute atoms 
	this->addAtoms(fOutputFile->getAtoms());
	
	// each reader contributes atoms
	const int readerCount = fInputFiles.size();
	for (int i=0; i < readerCount; ++i) {
		this->addAtoms(fInputFiles[i]->getAtoms());
	}
	
	// extra command line section always at end
	std::vector<Options::ExtraSection>& extraSections = fOptions.extraSections();
	for( std::vector<Options::ExtraSection>::iterator it=extraSections.begin(); it != extraSections.end(); ++it) {
		this->addAtoms(SectCreate::MakeReader(it->segmentName, it->sectionName, it->path, it->data, it->dataLen)->getAtoms());
	}
}

void Linker::loadUndefines()
{
	// keep looping until no more undefines were added in last loop
	unsigned int undefineCount = 0xFFFFFFFF;
	while ( undefineCount != fGlobalSymbolTable.getRequireCount() ) {
		undefineCount = fGlobalSymbolTable.getRequireCount();
		std::vector<const char*> undefineNames;
		fGlobalSymbolTable.getNeededNames(true, undefineNames);
		const int undefineCount = undefineNames.size();
		for (int i=0; i < undefineCount; ++i) {
			const char* name = undefineNames[i];
			ObjectFile::Atom* possibleAtom = fGlobalSymbolTable.find(name);
			if ( (possibleAtom == NULL) || (possibleAtom->isWeakDefinition() && (fOptions.outputKind() != Options::kObjectFile)) )
				this->addJustInTimeAtoms(name);
		}
	}
	
	if ( fOptions.outputKind() != Options::kObjectFile ) {
		// error out on any remaining undefines
		bool doPrint = true;
		bool doError = true;
		switch ( fOptions.undefinedTreatment() ) {
			case Options::kUndefinedError:
				break;
			case Options::kUndefinedDynamicLookup:
				doError = false;
				break;
			case Options::kUndefinedWarning:
				doError = false;
				break;
			case Options::kUndefinedSuppress:
				doError = false;
				doPrint = false;
				break;
		}
		std::vector<const char*> unresolvableUndefines;
		fGlobalSymbolTable.getNeededNames(false, unresolvableUndefines);
		const int unresolvableCount = unresolvableUndefines.size();
		if ( unresolvableCount != 0 ) {
			if ( doPrint ) {
				fprintf(stderr, "can't resolve symbols:\n");
				for (int i=0; i < unresolvableCount; ++i) {
					const char* name = unresolvableUndefines[i];
					const unsigned int nameLen = strlen(name);
					fprintf(stderr, "  %s, referenced from:\n", name);
					char stubName[nameLen+6];
					strcpy(stubName, name);
					strcat(stubName, "$stub");
					char nonLazyName[nameLen+16];
					strcpy(nonLazyName, name);
					strcat(nonLazyName, "$non_lazy_ptr");
					ObjectFile::Atom* lastStubAtomWithUnresolved = NULL;
					ObjectFile::Atom* lastNonLazyAtomWithUnresolved = NULL;
					for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms.begin(); it != fAllAtoms.end(); it++) {
						ObjectFile::Atom* atom = *it;
						std::vector<class ObjectFile::Reference*>& references = atom->getReferences();
						for (std::vector<ObjectFile::Reference*>::iterator rit=references.begin(); rit != references.end(); rit++) {
							ObjectFile::Reference* reference = *rit;
							if ( reference->isUnbound() ) {
								if ( (atom != lastStubAtomWithUnresolved) && (strcmp(reference->getTargetName(), stubName) == 0) ) {
									const char* path = atom->getFile()->getPath();
									const char* shortPath = strrchr(path, '/');
									if ( shortPath == NULL )
										shortPath = path;
									else
										shortPath = &shortPath[1];
									fprintf(stderr, "      %s in %s\n", atom->getDisplayName(), shortPath);
									lastStubAtomWithUnresolved = atom;
								}
								else if ( (atom != lastNonLazyAtomWithUnresolved) && (strcmp(reference->getTargetName(), nonLazyName) == 0) ) {
									const char* path = atom->getFile()->getPath();
									const char* shortPath = strrchr(path, '/');
									if ( shortPath == NULL )
										shortPath = path;
									else
										shortPath = &shortPath[1];
									fprintf(stderr, "      %s in %s\n", atom->getDisplayName(), shortPath);
									lastNonLazyAtomWithUnresolved = atom;
								}
							}
						}
					}
				}
			}
			if ( doError )
				throw "symbol(s) not found";
		}
		
		// now verify that -init routine exists
		if ( fOptions.initFunctionName() != NULL ) {
			if ( fGlobalSymbolTable.find(fOptions.initFunctionName()) == NULL )
				throwf("symbol %s not found for -init", fOptions.initFunctionName());
		}
	}
}



void Linker::addJustInTimeAtoms(const char* name)
{
	// give writer a crack at it
	ObjectFile::Atom* atom = fOutputFile->getUndefinedProxyAtom(name);
	if ( atom != NULL ) {
		this->addAtom(*atom);
	}
	else {
		// give direct readers a chance
		const int readerCount = fInputFiles.size();
		for (int i=0; i < readerCount; ++i) {
			// if this reader is a static archive that has the symbol we need, pull in all atoms in that module
			// if this reader is a dylib that exports the symbol we need, have it synthesize an atom for us.
			std::vector<class ObjectFile::Atom*>* atoms = fInputFiles[i]->getJustInTimeAtomsFor(name);
			if ( atoms != NULL ) {
				this->addAtoms(*atoms);
				delete atoms;
				return;  // found a definition, no need to search anymore
				//fprintf(stderr, "addJustInTimeAtoms(%s) => found in file #%d\n", name, i);
			}
		}
		
		// give indirect readers a chance
		for (std::list<IndirectLibrary>::iterator it=fIndirectDynamicLibraries.begin(); it != fIndirectDynamicLibraries.end(); it++) {
			ObjectFile::Reader* reader = it->reader;
			if ( reader != NULL ) {
				std::vector<class ObjectFile::Atom*>* atoms = reader->getJustInTimeAtomsFor(name);
				if ( atoms != NULL ) {
					this->addAtoms(*atoms);
					delete atoms;
					break;
					//fprintf(stderr, "addJustInTimeAtoms(%s) => found in file #%d\n", name, i);
				}
			}
		}
	}
}

void Linker::resolve(ObjectFile::Reference* reference)
{
	ObjectFile::Atom* target = NULL;
	const char* targetName = reference->getTargetName();
	const int targetNameLen = strlen(targetName);
	if ( (targetNameLen > 5) && (strcmp(&targetName[targetNameLen-5], "$stub") == 0) ) {
		// when looking up "_foo$stub", first look for "_foo"
		char nonStubTarget[targetNameLen+1];
		strcpy(nonStubTarget, targetName);
		nonStubTarget[targetNameLen-5] = '\0';
		// unless interposing and the symbol is exported 
		if ( !fOptions.interposable() || !fOptions.shouldExport(nonStubTarget) ) {
			target = fGlobalSymbolTable.find(nonStubTarget);	
			// also need indirection to all exported weak symbols for C++ support
			if ( (target != NULL) && !target->isImportProxy() && (!target->isWeakDefinition() || (target->getScope() != ObjectFile::Atom::scopeGlobal)) ) {
				reference->setTarget(*target);
				// mark stub as no longer being needed
				ObjectFile::Atom* stub = fGlobalSymbolTable.find(targetName);
				if ( stub != NULL ) {
					char lazySymbol[targetNameLen+8];
					strcpy(lazySymbol, nonStubTarget);
					strcat(lazySymbol, "$lazy_ptr");
					ObjectFile::Atom* lazyPtr = fGlobalSymbolTable.find(lazySymbol);
					fDeadAtoms.insert(stub);
					if ( lazyPtr != NULL )
						fDeadAtoms.insert(lazyPtr);
				}
				return;
			}
		}
	}
	
	// look in global symbol table
	target = fGlobalSymbolTable.find(targetName);
	if ( target == NULL ) {
		fprintf(stderr, "can't resolve: %s\n", targetName);
	}
	reference->setTarget(*target);
	
	// handle weak-imports
	if ( target->isImportProxy() ) {
		bool mismatch = false;
		if ( reference->isWeakReference() ) {
			switch(target->getImportWeakness()) {
				case ObjectFile::Atom::kWeakUnset:
					target->setImportWeakness(true);
                    break;	
				case ObjectFile::Atom::kWeakImport:
					break;
				case ObjectFile::Atom::kNonWeakImport:
					mismatch = true;
					break;
			}
		}
		else {
			switch(target->getImportWeakness()) {
				case ObjectFile::Atom::kWeakUnset:
					target->setImportWeakness(false);	
                    break;	
				case ObjectFile::Atom::kWeakImport:
					mismatch = true;
					break;
				case ObjectFile::Atom::kNonWeakImport:
					break;
			}
		}
		if ( mismatch ) {
			switch ( fOptions.weakReferenceMismatchTreatment() ) {
				case Options::kWeakReferenceMismatchError:
					throwf("mismatching weak references for symbol: %s", target->getName());
				case Options::kWeakReferenceMismatchWeak:
					target->setImportWeakness(true);
					break;
				case Options::kWeakReferenceMismatchNonWeak:
					target->setImportWeakness(false);
					break;
			}
		}
	}
	
	// handle references that have two (from and to) targets
	if ( reference->isUnbound() ) {
		const char* fromTargetName = reference->getFromTargetName();
		ObjectFile::Atom* fromTarget = fGlobalSymbolTable.find(fromTargetName);
		if ( target == NULL ) {
			fprintf(stderr, "can't resolve: %s\n", fromTargetName);
		}
		reference->setFromTarget(*fromTarget);
	}
}


void Linker::resolveReferences()
{
	// note: the atom list may grow during this loop as libraries supply needed atoms
	for (unsigned int j=0; j < fAllAtoms.size(); ++j) {
		ObjectFile::Atom* atom = fAllAtoms[j];
		std::vector<class ObjectFile::Reference*>& references = atom->getReferences();
		for (std::vector<ObjectFile::Reference*>::iterator it=references.begin(); it != references.end(); it++) {
			ObjectFile::Reference* reference = *it;
			if ( reference->isUnbound() ) {
				this->resolve(reference);
			}
		}
	}
}

class InSet
{
public:
	InSet(std::set<ObjectFile::Atom*>& deadAtoms) : fDeadAtoms(deadAtoms) {}
	
	bool operator()(ObjectFile::Atom*& atom) const { 
		return ( fDeadAtoms.count(atom) != 0 );
	}
	
private:
	std::set<ObjectFile::Atom*>& fDeadAtoms;
};


void Linker::deadStrip()
{
	//printf("Stripping atoms:\n");
	//for (std::set<ObjectFile::Atom*>::iterator it=fDeadAtoms.begin(); it != fDeadAtoms.end(); it++) {
	//	printf("\t%s\n", (*it)->getDisplayName());
	//}

	// for now, just remove atoms weak atoms that have been overridden
	fAllAtoms.erase(std::remove_if(fAllAtoms.begin(), fAllAtoms.end(), InSet(fDeadAtoms)), fAllAtoms.end());
}



void Linker::sortAtoms()
{
	Section::assignIndexes();
	std::sort(fAllAtoms.begin(), fAllAtoms.end(), Linker::AtomSorter());
}



// make sure given addresses are within reach of branches, etc
void Linker::tweakLayout()
{
	


}


void Linker::writeOutput()
{
	// if main executable, find entry point atom
	ObjectFile::Atom* entryPoint;
	switch ( fOptions.outputKind() ) {
		case Options::kDynamicExecutable:
		case Options::kStaticExecutable:
		case Options::kDyld:
			entryPoint = fGlobalSymbolTable.find(fOptions.entryName());
			if ( entryPoint == NULL ) {
				throwf("could not find entry point: %s", fOptions.entryName());
			}
			break;
		case Options::kDynamicLibrary:
			if ( fOptions.initFunctionName() != NULL ) {
				entryPoint = fGlobalSymbolTable.find(fOptions.initFunctionName());
				if ( entryPoint == NULL ) {
					throwf("could not find -init function: %s", fOptions.initFunctionName());
				}
			}
			break;
		default:
			entryPoint = NULL;
	}
	
	// tell writer about each segment's atoms
	fOutputFile->write(fAllAtoms, entryPoint);
}




ObjectFile::Reader* Linker::createReader(const Options::FileInfo& info)
{
	// map in whole file
	uint64_t len = info.fileLen;
	int fd = ::open(info.path, O_RDONLY, 0);
	if ( fd == -1 )
		throw "can't open file";
	if ( info.fileLen < 20 )
		throw "file too small";
	char* p = (char*)::mmap(NULL, info.fileLen, PROT_READ, MAP_FILE, fd, 0);
	if ( p == (char*)(-1) )
		throw "can't map file";
	::close(fd);
	
	// if fat file, skip to architecture we want
	const mach_header* mh = (mach_header*)p;
	if ( mh->magic == OSSwapBigToHostInt32(FAT_MAGIC) ) {
		// Fat header is always big-endian
		const struct fat_header* fh = (struct fat_header*)p;
		const struct fat_arch* archs = (struct fat_arch*)(p + sizeof(struct fat_header));
		for (unsigned long i=0; i < OSSwapBigToHostInt32(fh->nfat_arch); ++i) {
			if ( OSSwapBigToHostInt32(archs[i].cputype) == (uint32_t)fOptions.architecture() ) {
				mh = (struct mach_header*)((char*)p + OSSwapBigToHostInt32(archs[i].offset));
				len = OSSwapBigToHostInt32(archs[i].size);
				break;
			}
		}
	}
	
	if ( mh->magic == OSSwapBigToHostInt32(FAT_MAGIC) ) {
		const char* archName = "unknown";
		switch (fOptions.architecture()) {
			case CPU_TYPE_POWERPC:
				archName = "ppc";
				break;
			case CPU_TYPE_POWERPC64:
				archName = "ppc64";
				break;
			case CPU_TYPE_I386:
				archName = "i386";
				break;
		}
		throwf("missing required architecture %s in fat file", archName);
	}
	
	// pull out cpu-type and file-type in endian-safe way
	cpu_type_t cpuType = 0;
	uint32_t fileType = 0;
	if ( mh->magic == MH_MAGIC ) {
		fileType = mh->filetype;
		cpuType = mh->cputype;
	}
	else if ( mh->magic == OSSwapInt32(MH_MAGIC) ) {
		fileType = OSSwapInt32(mh->filetype);
		cpuType = OSSwapInt32(mh->cputype);
	}
	else if ( mh->magic == MH_MAGIC_64 ) {
		fileType = ((mach_header_64*)mh)->filetype;
		cpuType = ((mach_header_64*)mh)->cputype;
	}
	else if ( mh->magic == OSSwapInt32(MH_MAGIC_64) ) {
		fileType = OSSwapInt32(((mach_header_64*)mh)->filetype);
		cpuType = OSSwapInt32(((mach_header_64*)mh)->cputype);
	}
	else if ( strncmp((const char*)mh, "!<arch>\n", 8) == 0 ) {
		// is static archive
		switch ( fOptions.architecture() ) {
			case CPU_TYPE_POWERPC:
				return ppc::ObjectFileArchiveMachO::MakeReader((const uint8_t*)mh, len, info.path, fOptions.readerOptions());
			case CPU_TYPE_POWERPC64:
				return ppc64::ObjectFileArchiveMachO::MakeReader((const uint8_t*)mh, len, info.path, fOptions.readerOptions());
			case CPU_TYPE_I386:
				return i386::ObjectFileArchiveMachO::MakeReader((const uint8_t*)mh, len, info.path, fOptions.readerOptions());
		}
		throw "no matching archive reader";
	}
	else {
		throw "unknown file type";	
	}
	
	// bail out if cpu-type does not match requrired architecture
	if ( fOptions.architecture() == cpuType ) {
		// make appropriate reader object
		if ( fileType == MH_OBJECT ) {
			switch ( cpuType ) {
				case CPU_TYPE_POWERPC:
					return ppc::ObjectFileMachO::MakeReader((class ppc::macho_header*)mh, info.path, fOptions.readerOptions());
				case CPU_TYPE_POWERPC64:
					return ppc64::ObjectFileMachO::MakeReader((class ppc64::macho_header*)mh, info.path, fOptions.readerOptions());
				case CPU_TYPE_I386:
					return i386::ObjectFileMachO::MakeReader((class i386::macho_header*)mh, info.path, fOptions.readerOptions());
				default:
					throw "wrong architecture in object file";
			}
		}
		else if ( fileType == MH_DYLIB ) {
			ObjectFile::Reader* dylibReader = NULL;
			switch ( cpuType ) {
				case CPU_TYPE_POWERPC:
					dylibReader = ppc::ObjectFileDylibMachO::MakeReader((class ppc::macho_header*)mh, info.path, fOptions.readerOptions());
					break;
				case CPU_TYPE_POWERPC64:
					dylibReader = ppc64::ObjectFileDylibMachO::MakeReader((class ppc64::macho_header*)mh, info.path, fOptions.readerOptions());
					break;
				case CPU_TYPE_I386:
					dylibReader = i386::ObjectFileDylibMachO::MakeReader((class i386::macho_header*)mh, info.path, fOptions.readerOptions());
					break;
				default:
					throw "wrong architecture in dylib";
			}
			this->addDylib(dylibReader, info);
			return dylibReader;
		}
		throw "unknown mach-o file type";
	}
	else {
		throw "file does not contain requested architecture";
	}

}


void Linker::createReaders()
{
	std::vector<Options::FileInfo>& files = fOptions.getInputFiles();
	const int count = files.size();
	if ( count == 0 )
		throw "no object files specified";
	// add all direct object, archives, and dylibs
	for (int i=0; i < count; ++i) {
		Options::FileInfo& entry = files[i];
		// ignore /usr/lib/dyld on command line in crt.o build
		if ( strcmp(entry.path, "/usr/lib/dyld") != 0 ) {
			try {
				this->addInputFile(this->createReader(entry));
			}
			catch (const char* msg) {
				if ( strstr(msg, "architecture") != NULL ) {
					if (  fOptions.ignoreOtherArchInputFiles() ) {
						// ignore, because this is about an architecture not in use
					}
					else {
						fprintf(stderr, "ld64 warning: in %s, %s\n", entry.path, msg);
					}
				}
				else {
					throwf("in %s, %s", entry.path, msg);
				}
			}
		}
	}
	
	// add first level of indirect dylibs
	fDirectLibrariesComplete = true;
	for (std::vector<ExecutableFile::DyLibUsed>::iterator it=fDynamicLibraries.begin(); it != fDynamicLibraries.end(); it++) {
		this->addIndirectLibraries(it->reader);
	}
	
	// indirect handling depends on namespace
	switch ( fOptions.nameSpace() ) {
		case Options::kFlatNameSpace:
		case Options::kForceFlatNameSpace:
			// with flat namespace, blindly load all indirect libraries
			// the indirect list will grow as indirect libraries are loaded
			for (std::list<IndirectLibrary>::iterator it=fIndirectDynamicLibraries.begin(); it != fIndirectDynamicLibraries.end(); it++) {
				struct stat statBuffer;
				if ( stat(it->path, &statBuffer) == 0 ) {
					Options::FileInfo info;
					info.path = it->path;
					info.fileLen = statBuffer.st_size;
					info.options.fWeakImport = false;
					info.options.fReExport = false;
					info.options.fInstallPathOverride = NULL;
					it->reader = this->createReader(info);
				}
				else {
					fprintf(stderr, "ld64 warning: indirect library not found: %s\n", it->path);
				}
			}
			break;
			
		case Options::kTwoLevelNameSpace:
			// with two-level namespace we only want to use indirect libraries that are re-exported through a library that is used
			{
				bool indirectAdded = true;
				while ( indirectAdded ) {
					indirectAdded = false;
					// instantiate a reader for each indirect library and try to find parent that re-exports it
					for (std::list<IndirectLibrary>::iterator it=fIndirectDynamicLibraries.begin(); it != fIndirectDynamicLibraries.end(); it++) {
						if ( it->reader == NULL ) {
							try {
								struct stat statBuffer;
								if ( stat(it->path, &statBuffer) != 0 ) 
									throw "file not found";
									
								Options::FileInfo info;
								info.path = it->path;
								info.fileLen = statBuffer.st_size;
								info.options.fWeakImport = false;
								info.options.fReExport = false;
								info.options.fInstallPathOverride = NULL;
								it->reader = this->createReader(info);
								indirectAdded = true;
							}
							catch (const char* msg) {
								fprintf(stderr, "ld64 warning: indirect library %s could not be loaded: %s\n", it->path, msg);
							}
						}
						// if an indirect library does not have an assigned parent, look for one
						if ( (it->reader != NULL) && (it->reExportParent == NULL) ) {
							// ask each parent if they re-export this dylib
							for (std::set<ObjectFile::Reader*>::iterator pit=it->parents.begin(); pit != it->parents.end(); pit++) {
								if ( (*pit)->reExports(it->reader) ) {
									it->reExportParent = *pit;
									break;
								}
							}
						}
					}
				}
			}
			break;
	}
	
	// add relevant indirect libraries to the end of fDynamicLibraries
	for (std::list<IndirectLibrary>::iterator it=fIndirectDynamicLibraries.begin(); it != fIndirectDynamicLibraries.end(); it++) {
		if ( (it->reExportParent != NULL) || (fOptions.nameSpace() != Options::kTwoLevelNameSpace) ) {
			ExecutableFile::DyLibUsed dylibInfo;
			dylibInfo.reader = it->reader;
			dylibInfo.options.fWeakImport = false;
			dylibInfo.options.fReExport = false;
			dylibInfo.options.fInstallPathOverride = NULL;
			dylibInfo.indirect = true;
			dylibInfo.directReader = it->reExportParent;
			fDynamicLibraries.push_back(dylibInfo);
			if ( fOptions.readerOptions().fTraceIndirectDylibs )
				printf("[Logging for Build & Integration] Used indirect dynamic library: %s\n", it->path);
		}
	}
}



void Linker::addDylib(ObjectFile::Reader* reader, const Options::FileInfo& info)
{
	if ( fDirectLibrariesComplete ) {
		this->addIndirectLibraries(reader);
	}
	else {
		if ( fOptions.readerOptions().fTraceDylibs )
			printf("[Logging for Build & Integration] Used dynamic library: %s\n", reader->getPath());
		ExecutableFile::DyLibUsed dylibInfo;
		dylibInfo.reader = reader;
		dylibInfo.options = info.options;
		dylibInfo.indirect = false;
		dylibInfo.directReader = NULL;
		fDynamicLibraries.push_back(dylibInfo);
	}
}


void Linker::addIndirectLibraries(ObjectFile::Reader* reader)
{
	std::vector<const char*>* dependentLibs = reader->getDependentLibraryPaths();
	if ( dependentLibs != NULL ) {
		for (std::vector<const char*>::iterator it=dependentLibs->begin(); it != dependentLibs->end(); it++) {
			if ( this->haveDirectLibrary(*it) ) {
				// do nothing, direct library already exists
			}
			else if ( this->haveIndirectLibrary(*it, reader) ) {
				// side effect of haveIndirectLibrary() added reader to parent list
			}
			else {
				// add to list of indirect libraries
				IndirectLibrary indirectLib;
				indirectLib.path = *it;
				indirectLib.fileLen = 0;
				indirectLib.reader = NULL;
				indirectLib.parents.insert(reader);
				indirectLib.reExportParent = NULL;
				fIndirectDynamicLibraries.push_back(indirectLib);
				//fprintf(stderr, "add indirect library: %s\n", *it);
			}
		}
	}
}

bool Linker::haveIndirectLibrary(const char* path, ObjectFile::Reader* parentReader)
{
	for (std::list<IndirectLibrary>::iterator it=fIndirectDynamicLibraries.begin(); it != fIndirectDynamicLibraries.end(); it++) {
		if ( strcmp(path, it->path) == 0 ) {
			it->parents.insert(parentReader);
			return true;
		}
		if ( it->reader != NULL ) {
			const char* installPath = it->reader->getInstallPath();
			if ( (installPath != NULL) && (strcmp(path, installPath) == 0) )
				return true;
		}
	}
	return false;
}

bool Linker::haveDirectLibrary(const char* path)
{
	for (std::vector<ExecutableFile::DyLibUsed>::iterator it=fDynamicLibraries.begin(); it != fDynamicLibraries.end(); it++) {
		if ( strcmp(path, it->reader->getPath()) == 0 )
			return true;
		const char* installPath = it->reader->getInstallPath();
		if ( (installPath != NULL) && (strcmp(path, installPath) == 0) )
			return true;
	}
	return false;
}




void Linker::createWriter()
{
	const char* path = fOptions.getOutputFilePath();
	switch ( fOptions.architecture() ) {
		case CPU_TYPE_POWERPC:
			this->setOutputFile(ppc::ExecutableFileMachO::MakeWriter(path, fOptions, fDynamicLibraries));
			break;
		case CPU_TYPE_POWERPC64:
			this->setOutputFile(ppc64::ExecutableFileMachO::MakeWriter(path, fOptions, fDynamicLibraries));
			break;
		case CPU_TYPE_I386:
			this->setOutputFile(i386::ExecutableFileMachO::MakeWriter(path, fOptions, fDynamicLibraries));
			break;
		default:
			throw "unknown architecture";
	}
}


Linker::SymbolTable::SymbolTable(Linker& owner)
 : fOwner(owner), fRequireCount(0)
{
}

void Linker::SymbolTable::require(const char* name)
{
	//fprintf(stderr, "require(%s)\n", name);
	Mapper::iterator pos = fTable.find(name);
	if ( pos == fTable.end() ) {
		fTable[name] = NULL;
		++fRequireCount;
	}
}

bool Linker::SymbolTable::add(ObjectFile::Atom& atom)
{
	const bool log = false;
	const char* name = atom.getName();
	//fprintf(stderr, "map.add(%p: %s => %p)\n", &fTable, name, &atom);
	Mapper::iterator pos = fTable.find(name);
	if ( pos != fTable.end() ) {
		ObjectFile::Atom* existingAtom = pos->second;
		if ( existingAtom != NULL ) {
			if ( existingAtom->isTentativeDefinition() ) {
				if ( atom.isTentativeDefinition() ) {
					if ( atom.getSize() > existingAtom->getSize() ) {
						// replace common-symbol atom with another larger common-symbol
						if ( fOwner.fOptions.warnCommons() ) 
							fprintf(stderr, "ld64: replacing common symbol %s size %lld from %s with larger symbol size %lld from %s\n", 
									existingAtom->getName(), existingAtom->getSize(), existingAtom->getFile()->getPath(), atom.getSize(), atom.getFile()->getPath());
						fOwner.fDeadAtoms.insert(existingAtom);
						fTable[name] = &atom;
						return true;
					}
					else {
						// keep existing common-symbol atom
						if ( fOwner.fOptions.warnCommons() ) {
							if ( atom.getSize() == existingAtom->getSize() ) 
								fprintf(stderr, "ld64: ignoring common symbol %s from %s because already have common from %s with same size\n", 
									atom.getName(), atom.getFile()->getPath(), existingAtom->getFile()->getPath());
							else
								fprintf(stderr, "ld64: ignoring common symbol %s size %lld from %s because already have larger symbol size %lld from %s\n", 
									atom.getName(), atom.getSize(), atom.getFile()->getPath(), existingAtom->getSize(), existingAtom->getFile()->getPath());
						}
						fOwner.fDeadAtoms.insert(&atom);
						return false;
					}
				}
				else {
					// have common symbol, now found true defintion
					if ( atom.isImportProxy() ) {
						// definition is in a dylib, so commons-mode decides how to handle
						switch ( fOwner.fOptions.commonsMode() ) {
							case Options::kCommonsIgnoreDylibs:
								if ( fOwner.fOptions.warnCommons() ) 
									fprintf(stderr, "ld64: using common symbol %s from %s and ignoring defintion from dylib %s\n", 
											existingAtom->getName(), existingAtom->getFile()->getPath(), atom.getFile()->getPath());
								fOwner.fDeadAtoms.insert(&atom);
								return false;
							case Options::kCommonsOverriddenByDylibs:
								if ( fOwner.fOptions.warnCommons() ) 
									fprintf(stderr, "ld64: replacing common symbol %s from %s with true definition from %s\n", 
											existingAtom->getName(), existingAtom->getFile()->getPath(), atom.getFile()->getPath());
								fOwner.fDeadAtoms.insert(existingAtom);
								fTable[name] = &atom;
								return true;
							case Options::kCommonsConflictsDylibsError:
								throwf("common symbol %s from %s conflicts with defintion from dylib %s", 
										existingAtom->getName(), existingAtom->getFile()->getPath(), atom.getFile()->getPath());
						}
					}
					else {
						// replace common-symbol atom with true definition from .o file
						if ( fOwner.fOptions.warnCommons() ) {
							if ( atom.getSize() < existingAtom->getSize() ) 
								fprintf(stderr, "ld64: warning: replacing common symbol %s size %lld from %s with smaller true definition size %lld from %s\n", 
										existingAtom->getName(), existingAtom->getSize(), existingAtom->getFile()->getPath(), atom.getSize(), atom.getFile()->getPath());
							else
								fprintf(stderr, "ld64: replacing common symbol %s from %s with true definition from %s\n", 
										existingAtom->getName(), existingAtom->getFile()->getPath(), atom.getFile()->getPath());
						}
						fOwner.fDeadAtoms.insert(existingAtom);
						fTable[name] = &atom;
						return true;
					}
				}
			}
			else if ( atom.isTentativeDefinition() ) {
				// keep existing true definition, ignore new tentative definition
				if ( fOwner.fOptions.warnCommons() ) {
					if ( atom.getSize() > existingAtom->getSize() ) 
						fprintf(stderr, "ld64: warning: ignoring common symbol %s size %lld from %s because already have definition from %s size %lld, even though definition is smaller\n", 
							atom.getName(), atom.getSize(), atom.getFile()->getPath(), existingAtom->getFile()->getPath(), existingAtom->getSize());
					else
						fprintf(stderr, "ld64: ignoring common symbol %s from %s because already have definition from %s\n", 
							atom.getName(), atom.getFile()->getPath(), existingAtom->getFile()->getPath());
				}
				fOwner.fDeadAtoms.insert(&atom);
				return false;
			}
			else {
				// neither existing nor new atom are tentative definitions
				// if existing is weak, we may replace it
				if ( existingAtom->isWeakDefinition() ) {
					if ( atom.isImportProxy() ) {
						// keep weak definition even though one exists in a dylib, because coalescing means dylib's copy may not be used
						if ( log ) fprintf(stderr, "keep weak atom even though also in a dylib: %s\n", atom.getName());
						fOwner.fDeadAtoms.insert(&atom);
						return false;
					}
					else if ( atom.isWeakDefinition() ) {
						// have another weak atom, use existing, mark new as dead
						if ( log ) fprintf(stderr, "already have weak atom: %s\n", atom.getName());
						fOwner.fDeadAtoms.insert(&atom);
						return false;
					}
					else {
						// replace weak atom with non-weak atom
						if ( log ) fprintf(stderr, "replacing weak atom %p from %s with %p from %s: %s\n", existingAtom, existingAtom->getFile()->getPath(), &atom, atom.getFile()->getPath(), atom.getName());
						fOwner.fDeadAtoms.insert(existingAtom);
						fTable[name] = &atom;
						return true;
					}
				}
			}
			if ( atom.isWeakDefinition() ) {
				// ignore new weak atom, because we already have a non-weak one
				return false;
			}
			if ( atom.isCoalesableByName() && existingAtom->isCoalesableByName() ) {
				// both coalesable, so ignore duplicate
				return false;
			}
			fprintf(stderr, "duplicate symbol %s in %s and %s\n", name, atom.getFile()->getPath(), existingAtom->getFile()->getPath());
		}
	}
	fTable[name] = &atom;
	return true;
}

ObjectFile::Atom* Linker::SymbolTable::find(const char* name)
{
	Mapper::iterator pos = fTable.find(name);
	if ( pos != fTable.end() ) {
		return pos->second;
	}
	return NULL;
}


void Linker::SymbolTable::getNeededNames(bool andWeakDefintions, std::vector<const char*>& undefines)
{
	for (Mapper::iterator it=fTable.begin(); it != fTable.end(); it++) {
		if ( (it->second == NULL) || (andWeakDefintions && it->second->isWeakDefinition()) ) {
			undefines.push_back(it->first);
		}
	}
}




bool Linker::AtomSorter::operator()(ObjectFile::Atom* left, ObjectFile::Atom* right)
{
	// first sort by section order (which is already sorted by segment)
	unsigned int leftSectionIndex  =  left->getSection()->getIndex();
	unsigned int rightSectionIndex = right->getSection()->getIndex();
	if ( leftSectionIndex != rightSectionIndex)
		return (leftSectionIndex < rightSectionIndex);
		
	// with a section, sort by original atom order (.o file order and atom order in .o files)
	return left->getSortOrder() < right->getSortOrder();
}


int main(int argc, const char* argv[])
{
	try {
		// create linker object given command line arguments
		Linker ld(argc, argv);
		
		// open all input files
		ld.createReaders();
		
		// open output file
		ld.createWriter();
		
		// do linking
		ld.link();
	}
	catch (const char* msg) {
		fprintf(stderr, "ld64 failed: %s\n", msg);
		return 1;
	}
	
	return 0;
}







