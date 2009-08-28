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


#ifndef __OBJECTFILE__
#define __OBJECTFILE__

#include <stdint.h>
#include <vector>
#include <map>
#include <set>



//
// These classes represent the abstract Atoms and References that are the basis of the linker.
// An Atom and a Reference correspond to a Node and Edge in graph theory.
//
// A Reader is a class which parses an object file and presents it as Atoms and References.
// All linking operations are done on Atoms and References.  This makes the linker file
// format independent.
//
// A Writer takes a vector of Atoms with all References resolved and produces an executable file.
//
//



namespace ObjectFile {


struct LineInfo
{
	uint32_t	atomOffset;
	const char* fileName;
	uint32_t	lineNumber;
};


class ReaderOptions
{
public:
						ReaderOptions() : fFullyLoadArchives(false), fLoadAllObjcObjectsFromArchives(false), fFlatNamespace(false),
										fLinkingMainExecutable(false), 
										fForFinalLinkedImage(false), fNoEHLabels(false), fForStatic(false), fForDyld(false), fMakeTentativeDefinitionsReal(false), 
										fWhyLoad(false), fRootSafe(false), fSetuidSafe(false),fDebugInfoStripping(kDebugInfoFull),
										fImplicitlyLinkPublicDylibs(true),
										fAddCompactUnwindEncoding(true), 
										fWarnCompactUnwind(false),
										fRemoveDwarfUnwindIfCompactExists(false),
										fMakeCompressedDyldInfo(false),
										fAutoOrderInitializers(true),
										fLogObjectFiles(false), fLogAllFiles(false),
										fTraceDylibs(false), fTraceIndirectDylibs(false), fTraceArchives(false), 
										fTraceOutputFile(NULL), fMacVersionMin(kMinMacVersionUnset), fIPhoneVersionMin(kMinIPhoneVersionUnset) {}
	enum DebugInfoStripping { kDebugInfoNone, kDebugInfoMinimal, kDebugInfoFull };
	enum MacVersionMin { kMinMacVersionUnset, k10_1, k10_2, k10_3, k10_4, k10_5, k10_6 };
	enum IPhoneVersionMin { kMinIPhoneVersionUnset, k2_0, k2_1, k2_2, k3_0 };

	struct AliasPair {
		const char*			realName;
		const char*			alias;
	};

	bool					fFullyLoadArchives;
	bool					fLoadAllObjcObjectsFromArchives;
	bool					fFlatNamespace;
	bool					fLinkingMainExecutable;
	bool					fForFinalLinkedImage;
	bool					fNoEHLabels;
	bool					fForStatic;
	bool					fForDyld;
	bool					fMakeTentativeDefinitionsReal;
	bool					fWhyLoad;
	bool					fRootSafe;
	bool					fSetuidSafe;
	DebugInfoStripping		fDebugInfoStripping;
	bool					fImplicitlyLinkPublicDylibs;
	bool					fAddCompactUnwindEncoding;
	bool					fWarnCompactUnwind;
	bool					fRemoveDwarfUnwindIfCompactExists;
	bool					fMakeCompressedDyldInfo;
	bool					fAutoOrderInitializers;
	bool					fLogObjectFiles;
	bool					fLogAllFiles;
	bool					fTraceDylibs;
	bool					fTraceIndirectDylibs;
	bool					fTraceArchives;
	const char*				fTraceOutputFile;
	MacVersionMin			fMacVersionMin;
	IPhoneVersionMin		fIPhoneVersionMin;
	std::vector<AliasPair>	fAliases;
};


class Reader
{
public:
	enum DebugInfoKind { kDebugInfoNone=0, kDebugInfoStabs=1, kDebugInfoDwarf=2, kDebugInfoStabsUUID=3 };
	struct Stab
	{
		class Atom*	atom;
		uint8_t		type;
		uint8_t		other;
		uint16_t	desc;
		uint32_t	value;
		const char* string;
	};
	enum ObjcConstraint { kObjcNone,  kObjcRetainRelease,  kObjcRetainReleaseOrGC,  kObjcGC };
	enum CpuConstraint  { kCpuAny = 0 };

	class DylibHander
	{
	public:
		virtual				~DylibHander()	{}
		virtual Reader*		findDylib(const char* installPath, const char* fromPath) = 0;
	};


	static Reader* createReader(const char* path, const ReaderOptions& options);

	virtual const char*					getPath() = 0;
	virtual time_t						getModificationTime() = 0;
	virtual DebugInfoKind				getDebugInfoKind() = 0;
	virtual std::vector<class Atom*>&	getAtoms() = 0;
	virtual std::vector<class Atom*>*	getJustInTimeAtomsFor(const char* name) = 0;
	virtual std::vector<Stab>*			getStabs() = 0;
	virtual ObjcConstraint				getObjCConstraint()			{ return kObjcNone; }
	virtual uint32_t					updateCpuConstraint(uint32_t current) { return current; }
	virtual bool						objcReplacementClasses()	{ return false; }

	// For relocatable object files only
	virtual bool						canScatterAtoms()			{ return true; }
	virtual bool						optimize(const std::vector<ObjectFile::Atom*>&, std::vector<ObjectFile::Atom*>&, 
													std::vector<const char*>&, const std::set<ObjectFile::Atom*>&,
													std::vector<ObjectFile::Atom*>&,
													uint32_t, ObjectFile::Reader* writer, 
													ObjectFile::Atom* entryPointAtom,
													const std::vector<const char*>& llvmOptions,
													bool allGlobalsAReDeadStripRoots, int okind, 
													bool verbose, bool saveTemps, const char* outputFilePath,
													bool pie, bool allowTextRelocs) { return false; }
	virtual bool						hasLongBranchStubs()		{ return false; }

	// For Dynamic Libraries only
	virtual const char*					getInstallPath()			{ return NULL; }
	virtual uint32_t					getTimestamp()				{ return 0; }
	virtual uint32_t					getCurrentVersion()			{ return 0; }
	virtual uint32_t					getCompatibilityVersion()	{ return 0; }
	virtual void						processIndirectLibraries(DylibHander* handler)	{ }
	virtual void						setExplicitlyLinked()		{ }
	virtual bool						explicitlyLinked()			{ return false; }
	virtual bool						implicitlyLinked()			{ return false; }
	virtual bool						providedExportAtom()		{ return false; }
	virtual const char*					parentUmbrella()			{ return NULL; }
	virtual std::vector<const char*>*	getAllowableClients()  		{ return NULL; }
	virtual bool						hasWeakExternals()			{ return false; }
	virtual bool						deadStrippable()			{ return false; }
	virtual bool						isLazyLoadedDylib()			{ return false; }

protected:
										Reader() {}
	virtual								~Reader() {}
};

class Segment
{
public:
	virtual const char*			getName() const  = 0;
	virtual bool				isContentReadable() const = 0;
	virtual bool				isContentWritable() const = 0;
	virtual bool				isContentExecutable() const = 0;

	uint64_t					getBaseAddress() const { return fBaseAddress; }
	void						setBaseAddress(uint64_t addr) { fBaseAddress = addr; }
	virtual bool				hasFixedAddress() const { return false; }

protected:
								Segment() : fBaseAddress(0) {}
	virtual						~Segment() {}
	uint64_t					fBaseAddress;
};

class Reference;

class Section
{
public:
	unsigned int	getIndex() { return fIndex; }
	uint64_t		getBaseAddress() { return fBaseAddress; }
	void			setBaseAddress(uint64_t addr) { fBaseAddress = addr; }
	void*			fOther;

protected:
					Section() : fOther(NULL), fBaseAddress(0), fIndex(0)  {}
	uint64_t		fBaseAddress;
	unsigned int	fIndex;
};


struct Alignment 
{ 
				Alignment(int p2, int m=0) : powerOf2(p2), modulus(m) {}
	uint8_t		trailingZeros() const { return (modulus==0) ? powerOf2 : __builtin_ctz(modulus); }
	uint16_t	powerOf2;  
	uint16_t	modulus; 
};

struct UnwindInfo
{
	uint32_t	startOffset;
	uint32_t	unwindInfo;
	
	typedef UnwindInfo* iterator;

};
 

//
// An atom is the fundamental unit of linking.  A C function or global variable is an atom.
// An atom has content and some attributes. The content of a function atom is the instructions
// that implement the function.  The content of a global variable atom is its initial bits.
//
// Name:
// The name of an atom is the label name generated by the compiler.  A C compiler names foo()
// as _foo.  A C++ compiler names foo() as __Z3foov.
// The name refers to the first byte of the content.  An atom cannot have multiple entry points.
// Such code is modeled as multiple atoms, each having a "follow on" reference to the next.
// A "follow on" reference is a contraint to the linker to the atoms must be laid out contiguously.
//
// Scope:
// An atom is in one of three scopes: translation-unit, linkage-unit, or global.  These correspond
// to the C visibility of static, hidden, default.
//
// DefinitionKind:
// An atom is one of five defintion kinds:
//	regular			Most atoms.
//	weak			C++ compiler makes some functions weak if there might be multiple copies
//					that the linker needs to coalesce.
//	tentative		A straggler from ancient C when the extern did not exist. "int foo;" is ambiguous.
//					It could be a prototype or it could be a definition.
//	external		This is a "proxy" atom produced by a dylib reader.  It has no content.  It exists
//					so that all References can be resolved.
//	external-weak	Same as external, but the definition in the dylib is weak.
//
// SymbolTableInclusion:
// An atom may or may not be in the symbol table in an object file.
//  in				Most atoms for functions or global data
//	not-in			Anonymous atoms such literal c-strings, or other compiler generated data
//	in-never-strip	Atom whose name the strip tool should never remove (e.g. REFERENCED_DYNAMICALLY in mach-o)
//
// Ordinal:
// When a reader is created it is given a base ordinal number.  All atoms created by the reader
// should return a contiguous range of ordinal values that start at the base ordinal.  The ordinal
// values are used by the linker to sort the atom graph when producing the output file. 
//
class Atom
{
public:
	enum Scope { scopeTranslationUnit, scopeLinkageUnit, scopeGlobal };
	enum DefinitionKind { kRegularDefinition, kWeakDefinition, kTentativeDefinition, kExternalDefinition, kExternalWeakDefinition, kAbsoluteSymbol };
	enum ContentType { kUnclassifiedType, kCStringType, kCFIType, kLSDAType };
	enum SymbolTableInclusion { kSymbolTableNotIn, kSymbolTableIn, kSymbolTableInAndNeverStrip, kSymbolTableInAsAbsolute };

	virtual Reader*							getFile() const = 0;
	virtual bool							getTranslationUnitSource(const char** dir, const char** name) const = 0;
	virtual const char*						getName() const = 0;
	virtual const char*						getDisplayName() const = 0;
	virtual Scope							getScope() const = 0;
	virtual DefinitionKind					getDefinitionKind() const = 0;
	virtual ContentType						getContentType() const { return kUnclassifiedType; }
	virtual SymbolTableInclusion			getSymbolTableInclusion() const = 0;
	virtual	bool							dontDeadStrip() const = 0;
	virtual bool							isZeroFill() const = 0;
	virtual bool							isThumb() const = 0;
	virtual uint64_t						getSize() const = 0;
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const = 0;
	virtual bool							mustRemainInSection() const = 0;
	virtual const char*						getSectionName() const = 0;
	virtual Segment&						getSegment() const = 0;
	virtual Atom&							getFollowOnAtom() const = 0;
	virtual uint32_t						getOrdinal() const = 0;
	virtual std::vector<LineInfo>*			getLineInfo() const = 0;
	virtual Alignment						getAlignment() const = 0;
	virtual void							copyRawContent(uint8_t buffer[]) const = 0;
	virtual void							setScope(Scope) = 0;
	virtual UnwindInfo::iterator			beginUnwind() { return NULL; }
	virtual UnwindInfo::iterator			endUnwind() { return NULL; }
	virtual Reference*						getLSDA() { return NULL; }
	virtual Reference*						getFDE() { return NULL; }
	virtual Atom*							getPersonalityPointer() { return NULL; }

			uint64_t						getSectionOffset() const	{ return fSectionOffset; }
			uint64_t						getAddress() const	{ return fSection->getBaseAddress() + fSectionOffset; }
			class Section*					getSection() const { return fSection; }

	virtual void							setSectionOffset(uint64_t offset) { fSectionOffset = offset; }
	virtual void							setSection(class Section* sect) { fSection = sect; }

protected:
											Atom() :  fSectionOffset(0), fSection(NULL) {}
		virtual								~Atom() {}

		uint64_t							fSectionOffset;
		class Section*						fSection;
};


//
// A Reference is a directed edge to another Atom.  When an instruction in
// the content of an Atom refers to another Atom, that is represented by a
// Reference.
//
// There are two kinds of references: direct and by-name.  With a direct Reference,
// the target is bound by the Reader that created it.  For instance a reference to a
// static would produce a direct reference.  A by-name reference requires the linker
// to find the target Atom with the required name in order to be bound.
//
// For a link to succeed all References must be bound.
//
// A Reference has an optional "from" target.  This is used when the content to fix-up
// is the difference of two Atom address.  For instance, if a pointer sized data Atom
// is to contain A - B, then the Atom would have on Reference with a target of "A" and
// a from-target of "B".
//
// A Reference also has a fix-up-offset.  This is the offset into the content of the
// Atom holding the reference where the fix-up (relocation) will be applied.
//
//
//
class Reference
{
public:
	enum TargetBinding { kUnboundByName, kBoundDirectly, kBoundByName, kDontBind };

	virtual TargetBinding	getTargetBinding() const = 0;
	virtual TargetBinding	getFromTargetBinding() const = 0;
	virtual uint8_t			getKind() const = 0;
	virtual uint64_t		getFixUpOffset() const = 0;
	virtual const char*		getTargetName() const = 0;
	virtual Atom&			getTarget() const = 0;
	virtual uint64_t		getTargetOffset() const = 0;
	virtual Atom&			getFromTarget() const = 0;
	virtual const char*		getFromTargetName() const = 0;
	virtual uint64_t		getFromTargetOffset() const = 0;

	virtual void			setTarget(Atom&, uint64_t offset) = 0;
	virtual void			setFromTarget(Atom&) = 0;
	virtual const char*		getDescription() const = 0;
	virtual bool			isBranch() const  { return false; }

protected:
							Reference() {}
	virtual					~Reference() {}
};


};	// namespace ObjectFile


#endif // __OBJECTFILE__
