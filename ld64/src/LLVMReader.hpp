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

#ifndef __LLVM_READER_H__
#define __LLVM_READER_H__

#include <stdlib.h>
#include <vector>
#include "MachOFileAbstraction.hpp"
#include "Architectures.hpp"
#include "ObjectFile.h"
#include "llvm/LinkTimeOptimizer.h"

#define LLVMLinkTimeOptimizer "LLVMlto.dylib"

class LLVMReader;

//
// LLVMReference handles LLVMAtom references. These references facilitate 
// symbol resolution.
//

class LLVMReference : public ObjectFile::Reference
{
public:
										LLVMReference (const char *n) : fName(n), fAtom(0), fFromAtom(0) { }

	bool 								isTargetUnbound() const 			{ return fAtom == 0; }
	bool 								isFromTargetUnbound() const 		{ return true; }
	uint8_t 							getKind() const 					{ return 0; }
	uint64_t 							getFixUpOffset() const 				{ return 0; }
	const char * 						getTargetName() const 				{ return fName; }
	ObjectFile::Atom& 					getTarget() const 					{ return *fAtom; }
	uint64_t 							getTargetOffset() const 			{ return 0; }
	bool 								hasFromTarget() const 				{ return false; }
	ObjectFile::Atom& 					getFromTarget() const 				{ return *fFromAtom; }
	const char *						getFromTargetName() const 			{ return NULL; }
	uint64_t 							getFromTargetOffset() const 		{ return 0; }
	TargetBinding						getTargetBinding() const;
	TargetBinding						getFromTargetBinding() const  { return kDontBind; }
	void 								setTarget (ObjectFile::Atom &a, uint64_t offset) 
										{ fAtom = &a; }
	void 								setFromTarget(ObjectFile::Atom &a) 	{ }
	const char *						getDescription() const 				{ return NULL; }

private:
	const char *						fName;
	ObjectFile::Atom *					fAtom;
	ObjectFile::Atom *					fFromAtom;
};

ObjectFile::Reference::TargetBinding LLVMReference::getTargetBinding() const 
{ 
	if (strncmp (fName, "__ld64.llvm", 11) == 0)
		return kDontBind;
	else return kUnboundByName;
}

//
// LLVMAtom acts as a proxy Atom for the symbols that are exported by LLVM bytecode file. Initially,
// LLVMReader creates LLVMAtoms to allow linker proceed with usual symbol resolution phase. After
// optimization is performed, real Atoms are created for these symobls. However these real Atoms
// are not inserted into global symbol table. LLVMAtom holds real Atom and forwards appropriate
// methods to real atom.
//

class LLVMAtom : public ObjectFile::Atom
{
public:
	ObjectFile::Reader *						getFile() const 			{ return fOwner; }
	bool 										getTranslationUnitSource (const char **dir, const char **name) const 
																			{ return fRealAtom->getTranslationUnitSource (dir, name); }
	const char *								getName () const 			{ return fAtomName;	}
	const char *								getDisplayName() const 		{ return this->getName(); }
	Scope 										getScope() const 			{ return fScope; }
	DefinitionKind 								getDefinitionKind() const;
	SymbolTableInclusion 						getSymbolTableInclusion() const 
																			{ return fRealAtom->getSymbolTableInclusion(); }
	bool 										dontDeadStrip() const 		{ return false; }
	bool 										isZeroFill() const 			{ return fRealAtom->isZeroFill(); }
	uint64_t 									getSize() const 			{ return fRealAtom->getSize(); }
	std::vector<ObjectFile::Reference*>& 		getReferences() const 
																			{ return (fRealAtom ? fRealAtom->getReferences() : (std::vector<ObjectFile::Reference*>&)fReferences); }
	bool 										mustRemainInSection() const { return fRealAtom->mustRemainInSection(); }
	const char *								getSectionName() const 		{ return (fRealAtom ? fRealAtom->getSectionName() : NULL); }
	// Linker::optimize() sets section for this atom, not fRealAtom. Use this Atom's fSection.
	class ObjectFile::Section *					getSection() const 			{ return fSection; }
	ObjectFile::Segment& 						getSegment() const 			{ return fRealAtom->getSegment(); }
	uint32_t									getOrdinal() const { return (fRealAtom ? fRealAtom->getOrdinal() : 0); }
	ObjectFile::Atom& 							getFollowOnAtom() const 	{ return fRealAtom->getFollowOnAtom(); }
	std::vector<ObjectFile::LineInfo>* 			getLineInfo() const 		{ return fRealAtom->getLineInfo(); }
	ObjectFile::Alignment						getAlignment() const;
	void 										copyRawContent(uint8_t buffer[]) const 
																			{ fRealAtom->copyRawContent(buffer); }
	void 										setScope(Scope s) 			{ if (fRealAtom) fRealAtom->setScope(s); }

	LLVMAtom(ObjectFile::Reader *owner, const char *n, llvm::LLVMSymbol *ls);
	
	void 										setRealAtom (ObjectFile::Atom *atom) 
																			{ fRealAtom = atom; }
	void 										addReference(ObjectFile::Reference *ref) 
																			{ fReferences.push_back(ref); }

	void										setSectionOffset(uint64_t offset) { fSectionOffset = offset; if (fRealAtom) fRealAtom->setSectionOffset(offset); }
	void										setSection(class ObjectFile::Section* sect) { fSection = sect; if (fRealAtom) fRealAtom->setSection(sect); }
																			
private:
	ObjectFile::Reader *						fOwner;
	const char *								fAtomName;
	llvm::LLVMSymbol *							fLLVMSymbol;
	ObjectFile::Atom *							fRealAtom;
	std::vector<ObjectFile::Reference*> 		fReferences;
	ObjectFile::Atom::Scope						fScope;
	ObjectFile::Atom::DefinitionKind 			fDefKind;
};

ObjectFile::Atom::DefinitionKind LLVMAtom::getDefinitionKind() const 
{ 
	if (fRealAtom) 
		return fRealAtom->getDefinitionKind();
	else
		return fDefKind;
}

LLVMAtom::LLVMAtom(ObjectFile::Reader *owner, const char *n, llvm::LLVMSymbol *ls) : fOwner(owner), fAtomName(n), fLLVMSymbol(ls), fRealAtom(0) 
{

	if (!ls) return;

	switch (ls->getLinkage()) {
	case llvm::LTOExternalLinkage:
		fScope = scopeGlobal;
		fDefKind = kRegularDefinition;
		break;
	case llvm::LTOLinkOnceLinkage:
	case llvm::LTOWeakLinkage:
		// ??? How to differentiate between this two linkage types ?
		fScope = scopeGlobal;
		fDefKind = kWeakDefinition;
		break;
	default:
		throw "Unexpected LLVM Symbol Linkage info\n";
		break;
	}
}

ObjectFile::Alignment LLVMAtom::getAlignment() const
{
	if (fRealAtom)
		return fRealAtom->getAlignment(); 
	else {
		ObjectFile::Alignment alignment(fLLVMSymbol->getAlignment());
		return alignment;
	}
}

//
// LLVMReader does not expose internal symbols defined and used inside bytecode file. However, 
// these symbols may refere other external symbols. IntercessorAtom facilitate by acting as a 
// orignator of such references during pre-optimization symbol resoultion phase. These atoms
// are immediately removed after optimization.
//

class IntercessorAtom : public ObjectFile::Atom
{
public:
	ObjectFile::Reader *						getFile() const 			{ return fOwner; }
	bool 										getTranslationUnitSource (const char **dir, const char **name) const 
																			{ return false; }
	const char *								getName () const 			{ return fAtomName;	}
	const char *								getDisplayName() const 		{ return this->getName(); }
	Scope 										getScope() const 			{ return scopeGlobal; }
	DefinitionKind 								getDefinitionKind() const   { return kRegularDefinition; }
	SymbolTableInclusion 						getSymbolTableInclusion() const 
	{ return kSymbolTableNotIn; }
	bool 										dontDeadStrip() const 		{ return false; }
	bool 										isZeroFill() const 			{ return false; }
	uint64_t 									getSize() const 			{ return 0; }
	std::vector<ObjectFile::Reference*>& 		getReferences() const   	{ return  (std::vector<ObjectFile::Reference*>&)fReferences; }
	bool 										mustRemainInSection() const { return false; }
	const char *								getSectionName() const 		{ return NULL; }
	class ObjectFile::Section *					getSection() const 			{ return NULL; }
	ObjectFile::Segment& 						getSegment() const 			{ return this->getSegment(); }
	uint32_t									getOrdinal() const          { return 0; }
	ObjectFile::Atom& 							getFollowOnAtom() const 	{ return this->getFollowOnAtom(); }
	std::vector<ObjectFile::LineInfo>* 			getLineInfo() const 		{ return NULL; }
	ObjectFile::Alignment						getAlignment() const 		{ ObjectFile::Alignment a(0); return a; }
	void 										copyRawContent(uint8_t buffer[]) const 
																			{ }
	void 										setScope(Scope s) 			{ }


	IntercessorAtom(ObjectFile::Reader *owner, std::set<std::string> &references);
	
	void 										addReference(ObjectFile::Reference *ref) 
																			{ fReferences.push_back(ref); }
	void										addReferences(std::set<std::string> &references);
private:
	ObjectFile::Reader *						fOwner;
	char *								fAtomName;
	std::vector<ObjectFile::Reference*> 		fReferences;
	ObjectFile::Atom::Scope						fScope;
	ObjectFile::Atom::DefinitionKind 			fDefKind;
};

IntercessorAtom::IntercessorAtom(ObjectFile::Reader *owner, std::set<std::string> &references)
{
	static int sCount = 0;
	fOwner = owner;
	fAtomName = (char *) malloc (sizeof(char)*20);
	sprintf (fAtomName,"__ld64.llvm%d__",sCount++);

	for (std::set<std::string>::iterator it = references.begin(); it != references.end(); it++) {
	std::string r = *it;
	this->addReference(new LLVMReference(r.c_str()));
	}
}

void IntercessorAtom::addReferences(std::set<std::string> &references)
{
	for (std::set<std::string>::iterator it = references.begin(); it != references.end(); it++) {
	std::string r = *it;
	this->addReference(new LLVMReference(r.c_str()));
	}
}

class InIntercessorSet
{
public:
	InIntercessorSet(std::set<ObjectFile::Atom*>& iAtoms) : fIntercessorAtoms(iAtoms) {}

	bool operator()(ObjectFile::Atom*& atom) const {
		return ( fIntercessorAtoms.count(atom) != 0 );
	}

private:
	std::set<ObjectFile::Atom*>& fIntercessorAtoms;
};

//
// LLVMOptimizer class is responsible for communicating with LLVM LTO library.
// One LLVMOptimizer object is created per Linker invocation. All LLVMReaders share this
// one single optimizer object.
//

class LLVMOptimizer
{
public:
												LLVMOptimizer(Options &opt);
												~LLVMOptimizer() 				{ if (fLLVMHandle) dlclose(fLLVMHandle);	}


	void 										optimize(std::vector<ObjectFile::Atom *>&, std::vector<ObjectFile::Atom*>&, uint32_t);
	void 										read(ObjectFile::Reader *, const char *, std::set<std::string>&, std::vector<ObjectFile::Atom*>&, const char *);
	void 										reconcileOptimizedAtoms(std::vector<class ObjectFile::Atom*>&, std::vector<class ObjectFile::Atom*>&);
	void										addIntercessor(IntercessorAtom * atom) { fIntercessorAtoms.insert(atom); }
	void										addReader(ObjectFile::Reader *reader) { fLLVMReaders[reader->getPath()] = reader; }
	cpu_type_t									getCpuType(std::string &targetTriple);
	bool										validArchitecture(const char *path, cpu_type_t architecture);
	class LCStringEquals
	{
	public:
		bool operator()(const char* left, const char* right) const { return (strcmp(left, right) == 0); }
	};
	typedef hash_map<const char*, LLVMAtom*, hash<const char*>, LCStringEquals> LLVMAtomToNameMapper;
	typedef hash_map<const char*, ObjectFile::Reader*, hash<const char*>, LCStringEquals> ReaderToPathMapper;
	
    typedef llvm::LinkTimeOptimizer * (*createLLVMOptimizer_func_t) ();
private:
	bool 										fOptimized;
	llvm::LinkTimeOptimizer						*fOptimizer;
	void										*fLLVMHandle;
	LLVMAtomToNameMapper 						fLLVMSymbols;
	Options&									fOptions;
	std::set<ObjectFile::Atom*>					fIntercessorAtoms;
	ReaderToPathMapper							fLLVMReaders;
};

LLVMOptimizer::LLVMOptimizer(Options &opts) : fOptions(opts)
{
	fLLVMHandle = (llvm::LinkTimeOptimizer *) dlopen (LLVMLinkTimeOptimizer, RTLD_LAZY);
	if (!fLLVMHandle)
		throwf("Unable to load LLVM library: \n", dlerror());

	createLLVMOptimizer_func_t createLLVMOptimizer_fp = (createLLVMOptimizer_func_t)dlsym(fLLVMHandle, "createLLVMOptimizer");
	if (createLLVMOptimizer_fp == NULL)
		throwf("couldn't find \"createLLVMOptimizer\" ", dlerror());
	fOptimizer = createLLVMOptimizer_fp();
	fOptimized = false;
}

cpu_type_t LLVMOptimizer::getCpuType(std::string &targetTriple)
{
	if ( strncmp (targetTriple.c_str(), "powerpc-", 8) == 0)
		return CPU_TYPE_POWERPC;
	else if ( strncmp (targetTriple.c_str(), "powerpc64-", 10))
		return CPU_TYPE_POWERPC64;
	// match "i[3-9]86-*".
	else if ( targetTriple.size() >= 5 && targetTriple[0] == 'i' && targetTriple[2] == '8' && targetTriple[3] == '6' && targetTriple[4] == '-' && targetTriple[1] - '3' < 6  )
		return CPU_TYPE_I386;
	else
		return CPU_TYPE_ANY;
}

bool LLVMOptimizer::validArchitecture(const char *path, cpu_type_t architecture)
{
	std::string targetTriple;
	fOptimizer->getTargetTriple(path, targetTriple);
	if (architecture != getCpuType(targetTriple)) {
		fOptimizer->removeModule(path);
		return false;
	}

	return true;
}

void LLVMOptimizer::optimize(std::vector<ObjectFile::Atom*> &allAtoms, std::vector<ObjectFile::Atom*> &newAtoms, uint32_t nextInputOrdinal)
{
	if (fOptimized)
		return;

	char * tmp = "/tmp/ld64XXXXXXXX";
	char * bigOfile = (char *) malloc (strlen (tmp) + 3);
	if (!bigOfile)
		throw "Unable to create temp file name";
	strcpy (bigOfile, tmp);
	mktemp (bigOfile);
	strcat (bigOfile, ".o");

	std::vector <const char *> exportList;
	for (std::vector<ObjectFile::Atom*>::iterator it = allAtoms.begin(); it != allAtoms.end(); ++it) {
		ObjectFile::Atom *atom = *it;
		if (atom->getName()) {
			ReaderToPathMapper::iterator pos = fLLVMReaders.find(atom->getFile()->getPath());
			if (pos != fLLVMReaders.end())
				exportList.push_back(atom->getName());
		}
			
	}

	std::string targetTriple;
	llvm::LTOStatus status = fOptimizer->optimizeModules(bigOfile, exportList, targetTriple, fOptions.saveTempFiles(), fOptions.getOutputFilePath());
	if (status != llvm::LTO_OPT_SUCCESS) {
		if (status == llvm::LTO_WRITE_FAILURE)
			throw "Unable to write optimized output file";
		if (status == llvm::LTO_ASM_FAILURE)
			throw "Unable to assemble optimized output file";
		if (status == llvm::LTO_MODULE_MERGE_FAILURE)
			throw "Unable to merge bytecode files";
		if (status == llvm::LTO_NO_TARGET)
			throw "Unable to load target optimizer";
	}
	fOptimized = true;

	Options::FileInfo info = fOptions.findFile (bigOfile);
	ObjectFile::Reader* nr = NULL;
	int fd = ::open(info.path, O_RDONLY, 0);
	if ( fd == -1 )
		throwf("can't open file, errno=%d", errno);
	if ( info.fileLen < 20 )
		throw "file too small";

	uint8_t* p = (uint8_t*)::mmap(NULL, info.fileLen, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0);
	if ( p == (uint8_t*)(-1) )
		throwf("can't map file, errno=%d", errno);
	
	cpu_type_t cpt = getCpuType(targetTriple);
	switch (cpt) {
	case CPU_TYPE_POWERPC:
		if ( mach_o::relocatable::Reader<ppc>::validFile(p) )
			nr = new mach_o::relocatable::Reader<ppc>(p, info.path, info.modTime, fOptions.readerOptions(), nextInputOrdinal);
		break;
	case CPU_TYPE_POWERPC64:
		if ( mach_o::relocatable::Reader<ppc64>::validFile(p) )
			nr = new mach_o::relocatable::Reader<ppc64>(p, info.path, info.modTime, fOptions.readerOptions(), nextInputOrdinal);
		break;
	case CPU_TYPE_I386:
		if ( mach_o::relocatable::Reader<x86>::validFile(p) )
			nr = new mach_o::relocatable::Reader<x86>(p, info.path, info.modTime, fOptions.readerOptions(), nextInputOrdinal);
		break;
	default:
		throw "file is not of required architecture";
		break;
	}

	std::vector<class ObjectFile::Atom*> optimizedAtoms;
	optimizedAtoms = nr->getAtoms();
	reconcileOptimizedAtoms(optimizedAtoms, newAtoms);

	allAtoms.erase(std::remove_if(allAtoms.begin(), allAtoms.end(), InIntercessorSet(fIntercessorAtoms)), allAtoms.end());
	unlink(bigOfile);
	free(bigOfile);	
}

void LLVMOptimizer::read(ObjectFile::Reader *reader, const char *path, std::set<std::string> &references, std::vector<ObjectFile::Atom*> &atoms, const char *intercessorName)
{
	llvm::LinkTimeOptimizer::NameToSymbolMap symbols;
	llvm::LTOStatus status = fOptimizer->readLLVMObjectFile (path, symbols, references);
	if (status != llvm::LTO_READ_SUCCESS)
		throw "Unable to read LLVM bytecode file";

	for (llvm::LinkTimeOptimizer::NameToSymbolMap::iterator itr = symbols.begin();
		 itr != symbols.end(); itr++) {
		const char *name = itr->first;
		llvm::LLVMSymbol *ls = itr->second;
		LLVMAtom *a = new LLVMAtom(reader, name, ls);

		LLVMAtomToNameMapper::iterator pos = fLLVMSymbols.find(name);
		bool insertNewAtom = true;
		if (pos != fLLVMSymbols.end()) {
			LLVMAtom *existingAtom = pos->second;
			ObjectFile::Atom::DefinitionKind newDefKind = a->getDefinitionKind();
			ObjectFile::Atom::DefinitionKind existingDefKind = existingAtom->getDefinitionKind();
			if (newDefKind == ObjectFile::Atom::kRegularDefinition 
				&& existingDefKind == ObjectFile::Atom::kRegularDefinition)
				throwf ("duplicate symbol %s in %s and %s\n", name, a->getFile()->getPath(), existingAtom->getFile()->getPath());
			else if (newDefKind == ObjectFile::Atom::kWeakDefinition 
					 && existingDefKind == ObjectFile::Atom::kRegularDefinition)
				insertNewAtom = false;
			else if (newDefKind == ObjectFile::Atom::kWeakDefinition 
					 && existingDefKind == ObjectFile::Atom::kWeakDefinition)
				// pick one
				insertNewAtom = false;
			else if (newDefKind == ObjectFile::Atom::kRegularDefinition 
					 && existingDefKind == ObjectFile::Atom::kWeakDefinition)
				insertNewAtom = true;
		}
		if (insertNewAtom) {
			atoms.push_back(a);
			fLLVMSymbols[name] = a;
			a->addReference(new LLVMReference (intercessorName));
		}
	}
}

void LLVMOptimizer::reconcileOptimizedAtoms(std::vector<class ObjectFile::Atom*>& optimizedAtoms,
											std::vector<class ObjectFile::Atom*>& newAtoms)
{
	for (std::vector<ObjectFile::Atom *>::iterator itr = optimizedAtoms.begin(); 
		 itr != optimizedAtoms.end(); ++itr) {

		ObjectFile::Atom* atom = *itr;
		if (!atom->getName()) {
			newAtoms.push_back(atom);
			continue;
		}

		LLVMAtomToNameMapper::iterator pos = fLLVMSymbols.find(atom->getName());
		if ( pos != fLLVMSymbols.end() ) {

			LLVMAtom *la = fLLVMSymbols[atom->getName()];
			la->setRealAtom(atom);

		} 
		else
			newAtoms.push_back(atom);
	}
}

//
// LLVM bytecode file reader
//

class LLVMReader : public ObjectFile::Reader
{
public:
	static bool										validFile(const uint8_t* fileContent, const char *path, cpu_type_t architecture, Options &opts);
	static LLVMReader*								make(const uint8_t* fileContent, const char* path, time_t modTime, Options& options) 
																			{ return new LLVMReader(fileContent, path, modTime, options); }
	virtual											~LLVMReader();
	virtual std::vector<class ObjectFile::Atom*>&	getAtoms()				{ return (std::vector<class ObjectFile::Atom*>&)(fAtoms); }
	virtual std::vector<class ObjectFile::Atom*>*	getJustInTimeAtomsFor(const char* name) { return NULL; }
	virtual const char*								getPath()				{ return fPath; }
	virtual time_t									getModificationTime()	{ return fModTime; }
	virtual ObjectFile::Reader::DebugInfoKind		getDebugInfoKind()		{ return kDebugInfoNone; }
	virtual std::vector<Stab>*						getStabs()				{ return NULL; }

	ObjectFile::Atom * 								retriveIntercessorAtom() 	{ fAtoms.pop_back();return fIntercessorAtom; }
	ObjectFile::Atom * 								getIntercessorAtom() 	{ return fIntercessorAtom; }

private:

													LLVMReader(const uint8_t* fileContent, const char* path, time_t modTime, Options& options);
	void 											optimize(std::vector<ObjectFile::Atom*>& allAtoms, std::vector<ObjectFile::Atom*> &newAtoms, uint32_t);
	
	const char*										fPath;
	time_t											fModTime;
	std::vector<ObjectFile::Atom*>					fAtoms;
	IntercessorAtom *								fIntercessorAtom;
	static LLVMOptimizer							*fOptimizer;
	std::set<std::string>							fLLVMReferences;
};

LLVMOptimizer	*LLVMReader::fOptimizer = NULL;

LLVMReader::~LLVMReader()
{
	if (fOptimizer)
		delete fOptimizer;
}

LLVMReader::LLVMReader (const uint8_t* fileContent, const char *path, time_t modTime, Options& options)
{

	fPath = path;
	fModTime = modTime;
	fIntercessorAtom = new IntercessorAtom(this, fLLVMReferences);
	fOptimizer->read(this, path, fLLVMReferences, fAtoms, fIntercessorAtom->getName());
	fIntercessorAtom->addReferences(fLLVMReferences);
	fAtoms.push_back(fIntercessorAtom);
	fOptimizer->addIntercessor(fIntercessorAtom);
	fOptimizer->addReader(this);
}

bool LLVMReader::validFile(const uint8_t* fileContent, const char *path, cpu_type_t architecture, Options &opts)
{
	if (fileContent[0] == 'l'
		&& fileContent[1] == 'l'
		&& fileContent[2] == 'v'
		&& (fileContent[3] == 'c' || fileContent[3] == 'm')) {

		// create optimizer
		if (!fOptimizer)
			fOptimizer = new LLVMOptimizer(opts);

		if (fOptimizer->validArchitecture(path, architecture))
			return true;
	}
		
	return false;
}

void LLVMReader::optimize(std::vector<ObjectFile::Atom *> &allAtoms, std::vector<ObjectFile::Atom*> &newAtoms, uint32_t nextInputOrdinal)
{ 
	if (fOptimizer)  
		fOptimizer->optimize(allAtoms, newAtoms, nextInputOrdinal);
}

#endif

