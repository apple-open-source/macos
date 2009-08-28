/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
 *
 * Copyright (c) 2006-2008 Apple Inc. All rights reserved.
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

#ifndef __LTO_READER_H__
#define __LTO_READER_H__

#include <stdlib.h>
#include <mach-o/dyld.h>
#include <vector>
#include <ext/hash_set>
#include <ext/hash_map>

#include "MachOFileAbstraction.hpp"
#include "Architectures.hpp"
#include "ObjectFile.h"
#include "Options.h"

#include "llvm-c/lto.h"


namespace lto {


//
// Reference handles Atom references. These references facilitate 
// symbol resolution.
//

class Reference : public ObjectFile::Reference
{
public:
										Reference(const char* name) : fTargetName(name), fTargetAtom(NULL) { }
										Reference(ObjectFile::Atom& atom) : fTargetName(NULL), fTargetAtom(&atom) { }

	bool 								isTargetUnbound() const 			{ return fTargetAtom == NULL; }
	bool 								isFromTargetUnbound() const 		{ return true; }
	uint8_t 							getKind() const 					{ return 0; }
	uint64_t 							getFixUpOffset() const 				{ return 0; }
	const char * 						getTargetName() const 				{ return fTargetName; }
	ObjectFile::Atom& 					getTarget() const 					{ return *fTargetAtom; }
	uint64_t 							getTargetOffset() const 			{ return 0; }
	bool 								hasFromTarget() const 				{ return false; }
	ObjectFile::Atom& 					getFromTarget() const 				{ return *((ObjectFile::Atom*)NULL); }
	const char *						getFromTargetName() const 			{ return NULL; }
	uint64_t 							getFromTargetOffset() const 		{ return 0; }
	TargetBinding						getTargetBinding() const;
	TargetBinding						getFromTargetBinding() const  { return kDontBind; }
	void 								setTarget (ObjectFile::Atom& a, uint64_t offset) 
										{ fTargetAtom = &a; }
	void 								setFromTarget(ObjectFile::Atom &a) 	{ }
	const char *						getDescription() const;

private:
	const char *						fTargetName;
	ObjectFile::Atom *					fTargetAtom;
};


ObjectFile::Reference::TargetBinding Reference::getTargetBinding() const 
{ 
	if ( fTargetAtom == NULL ) 
		return kUnboundByName;
	else if ( fTargetName == NULL ) 
		return kBoundDirectly;
	else
		return kBoundByName;
}

const char* Reference::getDescription() const
{ 
	static char temp[256];
	strcpy(temp, "reference to ");
	if ( fTargetName != NULL )
		strcat(temp, fTargetName);
	else
		strcat(temp, fTargetAtom->getDisplayName());
	return temp; 
}


class Segment : public ObjectFile::Segment
{
public:
										Segment(const char* name, bool readable, bool writable, bool executable, bool fixedAddress)
											: fName(name), fReadable(readable), fWritable(writable), fExecutable(executable), fFixedAddress(fixedAddress) {}
	virtual const char*					getName() const						{ return fName; }
	virtual bool						isContentReadable() const			{ return fReadable; }
	virtual bool						isContentWritable() const			{ return fWritable; }
	virtual bool						isContentExecutable() const			{ return fExecutable; }
	virtual bool						hasFixedAddress() const				{ return fFixedAddress; }

	static Segment						fgBootstrapSegment;

private:
	const char*							fName;
	const bool							fReadable;
	const bool							fWritable;
	const bool							fExecutable;
	const bool							fFixedAddress;
};

Segment	Segment:: fgBootstrapSegment("__TEMP", true, false, false, false);
  
	
	  

//
// Atom acts as a proxy Atom for the symbols that are exported by LLVM bitcode file. Initially,
// Reader creates Atoms to allow linker proceed with usual symbol resolution phase. After
// optimization is performed, real Atoms are created for these symobls. However these real Atoms
// are not inserted into global symbol table. Atom holds real Atom and forwards appropriate
// methods to real atom.
//
class Atom : public ObjectFile::Atom
{
public:
												Atom(class Reader& owner, const char* name, Scope, DefinitionKind, uint8_t alignment, ObjectFile::Atom& internalAtom);
	
	ObjectFile::Reader*							getFile() const 			{ return (ObjectFile::Reader*)&fOwner; }
	bool 										getTranslationUnitSource (const char **dir, const char **name) const 
																			{ return fRealAtom->getTranslationUnitSource(dir, name); }
	const char *								getName () const 			{ return fName;	}
	const char *								getDisplayName() const 		{ return this->getName(); }
	Scope 										getScope() const 			{ return (fRealAtom ? fRealAtom->getScope() : fScope); }
	DefinitionKind 								getDefinitionKind() const	{ return (fRealAtom ? fRealAtom->getDefinitionKind() : fKind); }
	SymbolTableInclusion 						getSymbolTableInclusion() const 
																			{ return fRealAtom->getSymbolTableInclusion(); }
	bool 										dontDeadStrip() const 		{ return false; }
	bool 										isZeroFill() const 			{ return (fRealAtom ? fRealAtom->isZeroFill() : false); }
	bool										isThumb() const				{ return false; }
	uint64_t 									getSize() const 			{ return (fRealAtom ? fRealAtom->getSize() : 0); }
	std::vector<ObjectFile::Reference*>& 		getReferences() const 
																			{ return (fRealAtom ? fRealAtom->getReferences() : (std::vector<ObjectFile::Reference*>&)fReferences); }
	bool 										mustRemainInSection() const { return fRealAtom->mustRemainInSection(); }
	const char *								getSectionName() const 		{ return (fRealAtom ? fRealAtom->getSectionName() : NULL); }
	// Linker::optimize() sets section for this atom, not fRealAtom. Use this Atom's fSection.
	class ObjectFile::Section *					getSection() const 			{ return fSection; }
	ObjectFile::Segment& 						getSegment() const 			{ return (fRealAtom ? fRealAtom->getSegment() : Segment::fgBootstrapSegment); }
	uint32_t									getOrdinal() const			{ return (fRealAtom ? fRealAtom->getOrdinal() : 0); }
	ObjectFile::Atom& 							getFollowOnAtom() const 	{ return fRealAtom->getFollowOnAtom(); }
	std::vector<ObjectFile::LineInfo>* 			getLineInfo() const 		{ return (fRealAtom ? fRealAtom->getLineInfo() : NULL); }
	ObjectFile::Alignment						getAlignment() const		{ return (fRealAtom ? fRealAtom->getAlignment() : ObjectFile::Alignment(fAlignment)); }
	void 										copyRawContent(uint8_t buffer[]) const 
																			{ if (fRealAtom) fRealAtom->copyRawContent(buffer); }
	void 										setScope(Scope s) 			{ if (fRealAtom) fRealAtom->setScope(s); else fScope = s; }

	void 										setRealAtom (ObjectFile::Atom *atom) 
																			{ fRealAtom = atom; }
	ObjectFile::Atom *							getRealAtom() { return fRealAtom; }
	void 										addReference(ObjectFile::Reference *ref) 
																			{ fReferences.push_back(ref); }

	void										setSectionOffset(uint64_t offset) { fSectionOffset = offset; if (fRealAtom) fRealAtom->setSectionOffset(offset); }
	void										setSection(class ObjectFile::Section* sect) { fSection = sect; if (fRealAtom) fRealAtom->setSection(sect); }
																			
private:
	class Reader&								fOwner;
	const char*									fName;
	ObjectFile::Atom::Scope						fScope;
	ObjectFile::Atom::DefinitionKind 			fKind;
	uint8_t										fAlignment;
	ObjectFile::Atom*							fRealAtom;
	std::vector<ObjectFile::Reference*> 		fReferences;
};


Atom::Atom(class Reader& owner, const char* name, Scope scope, DefinitionKind kind, uint8_t alignment, ObjectFile::Atom& internalAtom) 
: fOwner(owner), fName(name), fScope(scope), fKind(kind), fAlignment(alignment), fRealAtom(NULL)
{
	// every Atom references the InternalAtom for its reader
	fReferences.push_back(new Reference(internalAtom));
}


//
// ld64 only tracks non-internal symbols from an llvm bitcode file.  
// We model this by having an InternalAtom which represent all internal functions and data.
// All non-interal symbols from a bitcode file are represented by a Atom
// and each Atom has a reference to the InternalAtom.  The InternalAtom
// also has references to each symbol external to the bitcode file. 
//
class InternalAtom : public ObjectFile::Atom
{
public:
												InternalAtom(class Reader& owner) : fOwner(owner) {}

	ObjectFile::Reader *						getFile() const 			{ return (ObjectFile::Reader*)&fOwner; }
	bool 										getTranslationUnitSource (const char **dir, const char **name) const 
																			{ return false; }
	const char *								getName () const 			{ return "__llvm-internal-atom";	}
	const char *								getDisplayName() const 		{ return "llvm bitcode"; }
	Scope 										getScope() const 			{ return scopeTranslationUnit; }
	DefinitionKind 								getDefinitionKind() const   { return kRegularDefinition; }
	SymbolTableInclusion 						getSymbolTableInclusion() const { return kSymbolTableNotIn; }
	bool 										dontDeadStrip() const 		{ return false; }
	bool 										isZeroFill() const 			{ return false; }
	bool										isThumb() const				{ return false; }
	uint64_t 									getSize() const 			{ return 0; }
	std::vector<ObjectFile::Reference*>& 		getReferences() const   	{ return  (std::vector<ObjectFile::Reference*>&)fReferences; }
	bool 										mustRemainInSection() const { return false; }
	const char *								getSectionName() const 		{ return NULL; }
	class ObjectFile::Section *					getSection() const 			{ return NULL; }
	ObjectFile::Segment& 						getSegment() const 			{ return Segment::fgBootstrapSegment; }
	uint32_t									getOrdinal() const          { return 0; }
	ObjectFile::Atom& 							getFollowOnAtom() const 	{ return *((ObjectFile::Atom*)NULL); }
	std::vector<ObjectFile::LineInfo>* 			getLineInfo() const 		{ return NULL; }
	ObjectFile::Alignment						getAlignment() const 		{ return ObjectFile::Alignment(0); }
	void 										copyRawContent(uint8_t buffer[]) const { }
	void 										setScope(Scope s) 			{ }

	void 										addReference(const char* targetName);
	
private:
	class Reader&								fOwner;
	std::vector<ObjectFile::Reference*> 		fReferences;
};


void InternalAtom::addReference(const char* name)
{
	fReferences.push_back(new Reference(name));
}




class RemovableAtoms
{
public:
	RemovableAtoms(std::set<ObjectFile::Atom*>& iAtoms) : fAtoms(iAtoms) {}

	bool operator()(ObjectFile::Atom*& atom) const {
		return ( fAtoms.count(atom) != 0 );
	}

private:
	std::set<ObjectFile::Atom*>& fAtoms;
};



//
// LLVM bitcode file reader
//
class Reader : public ObjectFile::Reader
{
public:
	static bool										validFile(const uint8_t* fileContent, uint64_t fileLength, cpu_type_t architecture);
	static bool										loaded() { return (::lto_get_version() != NULL); }
													Reader(const uint8_t* fileContent, uint64_t fileLength, 
																const char* path, time_t modTime, 
																const ObjectFile::ReaderOptions&, cpu_type_t arch);
	virtual											~Reader();
	
	virtual std::vector<class ObjectFile::Atom*>&	getAtoms()				{ return (std::vector<class ObjectFile::Atom*>&)(fAtoms); }
	virtual std::vector<class ObjectFile::Atom*>*	getJustInTimeAtomsFor(const char* name) { return NULL; }
	virtual const char*								getPath()				{ return fPath; }
	virtual time_t									getModificationTime()	{ return fModTime; }
	virtual ObjectFile::Reader::DebugInfoKind		getDebugInfoKind()		{ return kDebugInfoNone; }
	virtual std::vector<Stab>*						getStabs()				{ return NULL; }
	virtual bool									optimize(const std::vector<ObjectFile::Atom*>& allAtoms, std::vector<ObjectFile::Atom*>& newAtoms, 
																std::vector<const char*>& additionalUndefines, const std::set<ObjectFile::Atom*>&,
																std::vector<ObjectFile::Atom*>& newDeadAtoms,
																uint32_t nextInputOrdinal, 
																ObjectFile::Reader* writer, ObjectFile::Atom* entryPointAtom,
																const std::vector<const char*>& llvmOptions,
																bool allGlobalsAReDeadStripRoots,
																int outputKind, bool verbose, bool saveTemps, const char* outputFilePath,
																bool pie, bool allowTextRelocs);

private:

	class CStringEquals
	{
	public:
		bool operator()(const char* left, const char* right) const { return (strcmp(left, right) == 0); }
	};
	typedef	__gnu_cxx::hash_set<const char*, __gnu_cxx::hash<const char*>, CStringEquals>  CStringSet;
	typedef __gnu_cxx::hash_map<const char*, Atom*, __gnu_cxx::hash<const char*>, CStringEquals> CStringToAtom;
	
	ObjectFile::Reader*								makeMachOReader(const uint8_t* p, size_t len, uint32_t nextInputOrdinal);
	static const char*								tripletPrefixForArch(cpu_type_t);

	cpu_type_t										fArchitecture;
	const char*										fPath;
	time_t											fModTime;
	lto_module_t									fModule;
	std::vector<ObjectFile::Atom*>					fAtoms;
	InternalAtom									fInternalAtom;
	const ObjectFile::ReaderOptions&				fReaderOptions;
	static std::set<Reader*>						fgReaders;
	static bool										fgOptimized;
};

bool					Reader::fgOptimized = false;
std::set<Reader*>		Reader::fgReaders;


Reader::~Reader()
{
	if ( fModule != NULL )
		::lto_module_dispose(fModule);
}

Reader::Reader(const uint8_t* fileContent, uint64_t fileLength, const char* path, time_t modTime, 
				const ObjectFile::ReaderOptions& options, cpu_type_t arch)
	: fArchitecture(arch), fPath(strdup(path)), fModTime(modTime), fInternalAtom(*this), fReaderOptions(options)
{
	fgReaders.insert(this);

	fModule = ::lto_module_create_from_memory(fileContent, fileLength);
    if ( fModule == NULL )
		throwf("could not parse object file %s: %s", path, lto_get_error_message());
	
	fAtoms.push_back(&fInternalAtom);
	
	uint32_t count = ::lto_module_get_num_symbols(fModule);
	for (uint32_t i=0; i < count; ++i) {
		const char* name = ::lto_module_get_symbol_name(fModule, i);
		lto_symbol_attributes attr = lto_module_get_symbol_attribute(fModule, i);

		// <rdar://problem/6378110> LTO doesn't like dtrace symbols
		// ignore dtrace static probes for now
		// later when codegen is done and a mach-o file is produces the probes will be processed
		if ( (strncmp(name, "___dtrace_probe$", 16) == 0) || (strncmp(name, "___dtrace_isenabled$", 20) == 0) )
			continue;
				
		ObjectFile::Atom::DefinitionKind kind;
		switch ( attr & LTO_SYMBOL_DEFINITION_MASK ) {
			case LTO_SYMBOL_DEFINITION_REGULAR:
				kind = ObjectFile::Atom::kRegularDefinition;
				break;
			case LTO_SYMBOL_DEFINITION_TENTATIVE:
				kind = ObjectFile::Atom::kTentativeDefinition;
				break;
			case LTO_SYMBOL_DEFINITION_WEAK:
				kind = ObjectFile::Atom::kWeakDefinition;
				break;
			case LTO_SYMBOL_DEFINITION_UNDEFINED:
			case LTO_SYMBOL_DEFINITION_WEAKUNDEF:
				kind = ObjectFile::Atom::kExternalDefinition;
				break;
			default:
				throwf("unknown definition kind for symbol %s in bitcode file %s", name, path);
		}

		// make LLVM atoms for definitions and a reference for undefines
		if ( kind != ObjectFile::Atom::kExternalDefinition ) {
			ObjectFile::Atom::Scope scope;
			switch ( attr & LTO_SYMBOL_SCOPE_MASK) {
				case LTO_SYMBOL_SCOPE_INTERNAL:
					scope = ObjectFile::Atom::scopeTranslationUnit;
					break;
				case LTO_SYMBOL_SCOPE_HIDDEN:
					scope = ObjectFile::Atom::scopeLinkageUnit;
					break;
				case LTO_SYMBOL_SCOPE_DEFAULT:
					scope = ObjectFile::Atom::scopeGlobal;
					break;
				default:
					throwf("unknown scope for symbol %s in bitcode file %s", name, path);
			}
			// only make atoms for non-internal symbols 
			if ( scope == ObjectFile::Atom::scopeTranslationUnit )
				continue;
			uint8_t alignment = (attr & LTO_SYMBOL_ALIGNMENT_MASK);
			// make Atom
			fAtoms.push_back(new Atom(*this, name, scope, kind, alignment, fInternalAtom));
		}
		else {
			// add to list of external references
			fInternalAtom.addReference(name);
		}
	}
}

const char* Reader::tripletPrefixForArch(cpu_type_t arch)
{
	switch (arch) {
		case CPU_TYPE_POWERPC:
			return "powerpc-";
		case CPU_TYPE_POWERPC64:
			return "powerpc64-";
		case CPU_TYPE_I386:
			return "i386-";
		case CPU_TYPE_X86_64:
			return "x86_64-";
		case CPU_TYPE_ARM:
			return "arm";
	}
	return "";
}

bool Reader::validFile(const uint8_t* fileContent, uint64_t fileLength, cpu_type_t architecture)
{
	return ::lto_module_is_object_file_in_memory_for_target(fileContent, fileLength, tripletPrefixForArch(architecture));
}

bool Reader::optimize(const std::vector<ObjectFile::Atom *>& allAtoms, std::vector<ObjectFile::Atom*>& newAtoms, 
							std::vector<const char*>& additionalUndefines, const std::set<ObjectFile::Atom*>& deadAtoms,
							std::vector<ObjectFile::Atom*>& newlyDeadAtoms,
							uint32_t nextInputOrdinal,  ObjectFile::Reader* writer, ObjectFile::Atom* entryPointAtom,
							const std::vector<const char*>& llvmOptions,
							bool allGlobalsAReDeadStripRoots,
							int okind, bool verbose, bool saveTemps, const char* outputFilePath,
							bool pie, bool allowTextRelocs)
{ 
	// this method is call on all Readers.  We want the first call to trigger optimization
	// across all Readers and the subsequent calls to do nothing.
	if ( fgOptimized ) 
		return false;
	fgOptimized = true;
	
	Options::OutputKind outputKind = (Options::OutputKind)okind; // HACK to work around upward dependency

	// print out LTO version string if -v was used
	if ( verbose )
		fprintf(stderr, "%s\n", lto_get_version());
	
	// create optimizer and add each Reader
	lto_code_gen_t generator = ::lto_codegen_create();
	for (std::set<Reader*>::iterator it=fgReaders.begin(); it != fgReaders.end(); ++it) {
		if ( ::lto_codegen_add_module(generator, (*it)->fModule) )
			throwf("lto: could not merge in %s because %s", (*it)->fPath, ::lto_get_error_message());
	}

	// add any -mllvm command line options
	for (std::vector<const char*>::const_iterator it=llvmOptions.begin(); it != llvmOptions.end(); ++it) {
		::lto_codegen_debug_options(generator, *it);
	}

	// The atom graph uses directed edges (references). Collect all references where 
	// originating atom is not part of any LTO Reader. This allows optimizer to optimize an 
	// external (i.e. not originated from same .o file) reference if all originating atoms are also 
	// defined in llvm bitcode file.
	CStringSet nonLLVMRefs;
	CStringToAtom llvmAtoms;
    bool hasNonllvmAtoms = false;
	for (std::vector<ObjectFile::Atom*>::const_iterator it = allAtoms.begin(); it != allAtoms.end(); ++it) {
		ObjectFile::Atom* atom = *it;
		// only look at references come from an atom that is not an llvm atom
		if ( fgReaders.count((Reader*)(atom->getFile())) == 0 ) {
				// remember if we've seen any atoms not from an llvm reader and not from the writer
				if ( atom->getFile() != writer )
					hasNonllvmAtoms = true;
				std::vector<ObjectFile::Reference*>& refs = atom->getReferences();
				for (std::vector<ObjectFile::Reference*>::iterator ri=refs.begin(), re=refs.end(); ri != re; ++ri) {
					ObjectFile::Reference* ref = *ri;
					// add target name to set if target is an llvm atom
					if ( (ref->getTargetName() != NULL) && (fgReaders.count((Reader*)(ref->getTarget().getFile())) != 0) ) {
						nonLLVMRefs.insert(ref->getTargetName());
				}
			}
		}
		else {
			const char* name = atom->getName();
			if ( name != NULL )
				llvmAtoms[name] = (Atom*)atom;
		}
	}
	// if entry  point is in a llvm bitcode file, it must be preserved by LTO
	if ( entryPointAtom != NULL ) {
		if ( fgReaders.count((Reader*)(entryPointAtom->getFile())) != 0 ) 
			nonLLVMRefs.insert(entryPointAtom->getName());
	}
	
	// deadAtoms are the atoms that the linker coalesced.  For instance weak or tentative definitions
	// overriden by another atom.  If any of these deadAtoms are llvm atoms and they were replaced
	// with a mach-o atom, we need to tell the lto engine to preserve (not optimize away) its dead 
	// atom so that the linker can replace it with the mach-o one later.
	CStringToAtom deadllvmAtoms;
	for (std::set<ObjectFile::Atom*>::iterator it = deadAtoms.begin(); it != deadAtoms.end(); ++it) {
		ObjectFile::Atom* atom = *it;
		if ( fgReaders.count((Reader*)(atom->getFile())) != 0 ) {
			const char* name = atom->getName();
			::lto_codegen_add_must_preserve_symbol(generator, name);
			deadllvmAtoms[name] = (Atom*)atom;
		}
	}

	
	// tell code generator about symbols that must be preserved
	for (CStringToAtom::iterator it = llvmAtoms.begin(); it != llvmAtoms.end(); ++it) {
		const char* name = it->first;
		Atom* atom = it->second;
		// Include llvm Symbol in export list if it meets one of following two conditions
		// 1 - atom scope is global (and not linkage unit).
		// 2 - included in nonLLVMRefs set.
		// If a symbol is not listed in exportList then LTO is free to optimize it away.
		if ( (atom->getScope() == ObjectFile::Atom::scopeGlobal) ) 
			::lto_codegen_add_must_preserve_symbol(generator, name);
		else if ( nonLLVMRefs.find(name) != nonLLVMRefs.end() ) 
			::lto_codegen_add_must_preserve_symbol(generator, name);
	}
	
    // special case running ld -r on all bitcode files to produce another bitcode file (instead of mach-o)
    if ( (outputKind == Options::kObjectFile) && !hasNonllvmAtoms ) {
		if ( ! ::lto_codegen_write_merged_modules(generator, outputFilePath) ) {
			// HACK, no good way to tell linker we are all done, so just quit
			exit(0);
		}
		warning("could not produce merged bitcode file");
    }
    
    // if requested, save off merged bitcode file
    if ( saveTemps ) {
        char tempBitcodePath[MAXPATHLEN];
        strcpy(tempBitcodePath, outputFilePath);
        strcat(tempBitcodePath, ".lto.bc");
        ::lto_codegen_write_merged_modules(generator, tempBitcodePath);
    }

	// set code-gen model
	lto_codegen_model model = LTO_CODEGEN_PIC_MODEL_DYNAMIC;
	switch ( outputKind ) {
		case Options::kDynamicExecutable:
		case Options::kPreload:
			if ( pie )
				model = LTO_CODEGEN_PIC_MODEL_DYNAMIC;
			else
				model = LTO_CODEGEN_PIC_MODEL_DYNAMIC_NO_PIC;
			break;
		case Options::kDynamicLibrary:
		case Options::kDynamicBundle:
		case Options::kObjectFile: // ?? Is this appropriate ?
		case Options::kDyld:
		case Options::kKextBundle:
			if ( allowTextRelocs )
				model = LTO_CODEGEN_PIC_MODEL_DYNAMIC_NO_PIC;
			else
				model = LTO_CODEGEN_PIC_MODEL_DYNAMIC;
			break;
		case Options::kStaticExecutable:
			model = LTO_CODEGEN_PIC_MODEL_STATIC;
			break;
	}
	if ( ::lto_codegen_set_pic_model(generator, model) )
		throwf("could not create set codegen model: %s", lto_get_error_message());

	// run code generator
	size_t machOFileLen;
	const uint8_t* machOFile = (uint8_t*)::lto_codegen_compile(generator, &machOFileLen);
	if ( machOFile == NULL ) 
		throwf("could not do LTO codegen: %s", ::lto_get_error_message());
	
    // if requested, save off temp mach-o file
    if ( saveTemps ) {
        char tempMachoPath[MAXPATHLEN];
        strcpy(tempMachoPath, outputFilePath);
        strcat(tempMachoPath, ".lto.o");
        int fd = ::open(tempMachoPath, O_CREAT | O_WRONLY | O_TRUNC, 0666);
		if ( fd != -1) {
			::write(fd, machOFile, machOFileLen);
			::close(fd);
		}
    }

	// parse generated mach-o file into a MachOReader
	ObjectFile::Reader* machoReader = this->makeMachOReader(machOFile, machOFileLen, nextInputOrdinal);
	
	// sync generated mach-o atoms with existing atoms ld knows about
	std::vector<ObjectFile::Atom*> machoAtoms = machoReader->getAtoms();
	for (std::vector<ObjectFile::Atom *>::iterator it = machoAtoms.begin(); it != machoAtoms.end(); ++it) {
		ObjectFile::Atom* atom = *it;
		const char* name = atom->getName();
		if ( name != NULL ) {
			CStringToAtom::iterator pos = llvmAtoms.find(name);
			if ( pos != llvmAtoms.end() ) {
				// turn Atom into a proxy for this mach-o atom
				pos->second->setRealAtom(atom);
			}
			else {
				// an atom of this name was not in the allAtoms list the linker gave us
				if ( deadllvmAtoms.find(name) != deadllvmAtoms.end() ) {
					// this corresponding to an atom that the linker coalesced away.  Ignore it
					// Make sure there any dependent atoms are also marked dead
					std::vector<ObjectFile::Reference*>& refs = atom->getReferences();
					for (std::vector<ObjectFile::Reference*>::iterator ri=refs.begin(), re=refs.end(); ri != re; ++ri) {
						ObjectFile::Reference* ref = *ri;
						if ( ref->getKind() == 2 /*kGroupSubordinate*/ ) {	// FIX FIX
							ObjectFile::Atom* targ = &ref->getTarget();
							deadllvmAtoms[targ->getName()] = (Atom*)atom;
						}
					}
				}
				else
				{
					// this is something new that lto conjured up, tell ld its new
					newAtoms.push_back(atom);
				}
			}
		}
		else {
			// ld only knew about named atoms, so this one must be new
			newAtoms.push_back(atom);
		}
		std::vector<class ObjectFile::Reference*>& references = atom->getReferences();
		for (std::vector<ObjectFile::Reference*>::iterator rit=references.begin(); rit != references.end(); ++rit) {
			ObjectFile::Reference* ref = *rit;
			const char* targetName = ref->getTargetName();
			CStringToAtom::iterator pos;
			if (targetName != NULL) {
				switch ( ref->getTargetBinding() ) {
					case ObjectFile::Reference::kUnboundByName:
						// accumulate unbounded references so that ld can bound them.
						additionalUndefines.push_back(targetName);
						break;
					case ObjectFile::Reference::kBoundDirectly:
					case ObjectFile::Reference::kBoundByName:
						// If mach-o atom is referencing another mach-o atom then 
						// reference is not going through Atom proxy. Fix it here to ensure that all
						// llvm symbol references always go through Atom proxy.
						pos = llvmAtoms.find(targetName);
						if ( pos != llvmAtoms.end() )
							ref->setTarget(*pos->second, ref->getTargetOffset());
						break;
					case ObjectFile::Reference::kDontBind:
						break;
				}
			}
		}
	}
		
	// Remove InternalAtoms from ld
	for (std::set<Reader*>::iterator it=fgReaders.begin(); it != fgReaders.end(); ++it) {
		newlyDeadAtoms.push_back(&((*it)->fInternalAtom));
	}
	// Remove Atoms from ld if code generator optimized them away
	for (CStringToAtom::iterator li = llvmAtoms.begin(), le = llvmAtoms.end(); li != le; ++li) {
		// check if setRealAtom() called on this Atom
		if ( li->second->getRealAtom() == NULL )
			newlyDeadAtoms.push_back(li->second);
	}
	
	return true;
}


ObjectFile::Reader* Reader::makeMachOReader(const uint8_t* p, size_t len, uint32_t nextInputOrdinal) 
{
	switch ( fArchitecture ) {
		case CPU_TYPE_POWERPC:
			if ( mach_o::relocatable::Reader<ppc>::validFile(p) )
				return new mach_o::relocatable::Reader<ppc>(p, "/tmp/lto.o", 0, fReaderOptions, nextInputOrdinal);
			break;
		case CPU_TYPE_POWERPC64:
			if ( mach_o::relocatable::Reader<ppc64>::validFile(p) )
				return new mach_o::relocatable::Reader<ppc64>(p, "/tmp/lto.o", 0, fReaderOptions, nextInputOrdinal);
			break;
		case CPU_TYPE_I386:
			if ( mach_o::relocatable::Reader<x86>::validFile(p) )
				return new mach_o::relocatable::Reader<x86>(p, "/tmp/lto.o", 0, fReaderOptions, nextInputOrdinal);
			break;
		case CPU_TYPE_X86_64:
			if ( mach_o::relocatable::Reader<x86_64>::validFile(p) )
				return new mach_o::relocatable::Reader<x86_64>(p, "/tmp/lto.o", 0, fReaderOptions, nextInputOrdinal);
			break;
		case CPU_TYPE_ARM:
			if ( mach_o::relocatable::Reader<arm>::validFile(p) )
				return new mach_o::relocatable::Reader<arm>(p, "/tmp/lto.o", 0, fReaderOptions, nextInputOrdinal);
			break;
	}
	throw "LLVM LTO, file is not of required architecture";
}

}; // namespace lto

extern void printLTOVersion(Options &opts);

void printLTOVersion(Options &opts) {
	const char* vers = lto_get_version();
	if ( vers != NULL )
		fprintf(stderr, "%s\n", vers);
}


#endif

