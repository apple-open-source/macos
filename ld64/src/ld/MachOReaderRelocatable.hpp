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

#ifndef __OBJECT_FILE_MACH_O__
#define __OBJECT_FILE_MACH_O__

#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <sys/param.h>

#include <vector>
#include <set>
#include <algorithm>

#include "MachOFileAbstraction.hpp"
#include "Architectures.hpp"
#include "ObjectFile.h"
#include "dwarf2.h"
#include "debugline.h"

#include <libunwind/DwarfInstructions.hpp>
#include <libunwind/AddressSpace.hpp>
#include <libunwind/Registers.hpp>

//
//
//	To implement architecture xxx, you must write template specializations for the following six methods:
//			Reader<xxx>::validFile()
//			Reader<xxx>::validSectionType()
//			Reader<xxx>::addRelocReference()
//			Reference<xxx>::getDescription()
//
//



extern  __attribute__((noreturn)) void throwf(const char* format, ...);
extern void warning(const char* format, ...);

namespace mach_o {
namespace relocatable {



class ReferenceSorter
{
public:
	bool operator()(const ObjectFile::Reference* left, const ObjectFile::Reference* right)
	{
		return ( left->getFixUpOffset() < right->getFixUpOffset() );
	}
};


// forward reference
template <typename A> class Reader;

struct AtomAndOffset
{
						AtomAndOffset(ObjectFile::Atom* a=NULL) : atom(a), offset(0) {}
						AtomAndOffset(ObjectFile::Atom* a, uint32_t off) : atom(a), offset(off) {}
	ObjectFile::Atom*	atom;
	uint32_t			offset;
};


template <typename A>
class Reference : public ObjectFile::Reference
{
public:
	typedef typename A::P						P;
	typedef typename A::P::uint_t				pint_t;
	typedef typename A::ReferenceKinds			Kinds;

							Reference(Kinds kind, const AtomAndOffset& at, const AtomAndOffset& toTarget);
							Reference(Kinds kind, const AtomAndOffset& at, const AtomAndOffset& fromTarget, const AtomAndOffset& toTarget);
							Reference(Kinds kind, const AtomAndOffset& at, const char* toName, uint32_t toOffset);

	virtual					~Reference() {}


	virtual ObjectFile::Reference::TargetBinding	getTargetBinding() const;
	virtual ObjectFile::Reference::TargetBinding	getFromTargetBinding() const;
	virtual uint8_t			getKind() const									{ return (uint8_t)fKind; }
	virtual uint64_t		getFixUpOffset() const							{ return fFixUpOffsetInSrc; }
	virtual const char*		getTargetName() const							{ return (fToTarget.atom != NULL) ? fToTarget.atom->getName() : fToTargetName;  }
	virtual ObjectFile::Atom& getTarget() const								{ return *fToTarget.atom; }
	virtual uint64_t		getTargetOffset() const							{ return (int64_t)((int32_t)fToTarget.offset); }
	virtual ObjectFile::Atom& getFromTarget() const							{ return *fFromTarget.atom; }
	virtual const char*		getFromTargetName() const						{ return (fFromTarget.atom != NULL) ? fFromTarget.atom->getName() : fFromTargetName; }
	virtual void			setTarget(ObjectFile::Atom& target, uint64_t offset)	{ fToTarget.atom = &target; fToTarget.offset = offset;  }
	virtual void			setToTargetOffset(uint64_t offset)				{ fToTarget.offset = offset; }
	virtual void			setFromTarget(ObjectFile::Atom& target)			{ fFromTarget.atom = &target; }
	virtual void			setFromTargetName(const char* name)				{ fFromTargetName = name; }
	virtual void			setFromTargetOffset(uint64_t offset)			{ fFromTarget.offset = offset; }
	virtual const char*		getDescription() const;
	virtual uint64_t		getFromTargetOffset() const						{ return fFromTarget.offset; }
	virtual bool			isBranch() const;
	virtual const char*		getTargetDisplayName() const					{ return (fToTarget.atom != NULL) ? fToTarget.atom->getDisplayName() : fToTargetName;  }
	virtual const char*		getFromTargetDisplayName() const				{ return (fFromTarget.atom != NULL) ? fFromTarget.atom->getDisplayName() : fFromTargetName; }

	static bool				fgForFinalLinkedImage;

private:
	pint_t					fFixUpOffsetInSrc;
	AtomAndOffset			fToTarget;
	AtomAndOffset			fFromTarget;
	const char*				fToTargetName;
	const char*				fFromTargetName;
	Kinds					fKind;
	
};


template <typename A> bool Reference<A>::fgForFinalLinkedImage = true;

template <typename A>
Reference<A>::Reference(Kinds kind, const AtomAndOffset& at, const AtomAndOffset& toTarget)
 : fFixUpOffsetInSrc(at.offset), fToTarget(toTarget), fToTargetName(NULL), fFromTargetName(NULL),
    fKind(kind)
{
	// make reference a by-name unless:
	// - the reference type is only used with direct references
	// - the target is translation unit scoped
	// - the target kind is not regular (is weak or tentative)
	if ( (kind != A::kNoFixUp) && (kind != A::kFollowOn) && (kind != A::kGroupSubordinate)
		&& (toTarget.atom->getScope() != ObjectFile::Atom::scopeTranslationUnit) 
		&& (toTarget.atom->getDefinitionKind() != ObjectFile::Atom::kRegularDefinition)
		&& (toTarget.atom != at.atom) ) {
		fToTargetName = toTarget.atom->getName();
		//fprintf(stderr, "Reference(): changing to by-name %p %s, target scope=%d, target section=%s\n", toTarget.atom, fToTargetName, toTarget.atom->getScope(), toTarget.atom->getSectionName());
		fToTarget.atom = NULL;
	}
	((class BaseAtom*)at.atom)->addReference(this);
	//fprintf(stderr, "Reference(): %p fToTarget<%s, %08X>\n", this, (fToTarget.atom != NULL) ? fToTarget.atom->getDisplayName() : fToTargetName , fToTarget.offset);
}

template <typename A>
Reference<A>::Reference(Kinds kind, const AtomAndOffset& at, const AtomAndOffset& fromTarget, const AtomAndOffset& toTarget)
 : fFixUpOffsetInSrc(at.offset), fToTarget(toTarget), fFromTarget(fromTarget),
   fToTargetName(NULL), fFromTargetName(NULL), fKind(kind)
{
	// make reference a by-name where needed
	if ( (kind != A::kNoFixUp) && (kind != A::kFollowOn) && (kind != A::kGroupSubordinate)
		&& (toTarget.atom->getScope() != ObjectFile::Atom::scopeTranslationUnit)
		&& (toTarget.atom->getDefinitionKind() != ObjectFile::Atom::kRegularDefinition) 
		&& (toTarget.atom != at.atom) ) {
			fToTargetName = toTarget.atom->getName();
			fToTarget.atom = NULL;
	}
	((class BaseAtom*)at.atom)->addReference(this);
	//fprintf(stderr, "Reference(): %p kind=%d, fToTarget<%s, %08X>, fromTarget<%s, %08X>\n", this, kind,
	//	 this->getTargetName(), fToTarget.offset, this->getFromTargetName(), fromTarget.offset);
}

template <typename A>
Reference<A>::Reference(Kinds kind, const AtomAndOffset& at, const char* toName, uint32_t toOffset)
 : fFixUpOffsetInSrc(at.offset),
   fToTargetName(toName), fFromTargetName(NULL), fKind(kind)
{
	fToTarget.offset = toOffset;
	((class BaseAtom*)at.atom)->addReference(this);
}

template <typename A>
ObjectFile::Reference::TargetBinding Reference<A>::getTargetBinding() const
{
	if ( fgForFinalLinkedImage ) {
		if ( (fKind == A::kDtraceProbe) || (fKind == A::kDtraceProbeSite) || (fKind == A::kDtraceIsEnabledSite) || (fKind == A::kDtraceTypeReference) )
			return ObjectFile::Reference::kDontBind;
	}
	if ( fToTarget.atom == NULL ) 
		return ObjectFile::Reference::kUnboundByName;
	if ( fToTargetName == NULL ) 
		return ObjectFile::Reference::kBoundDirectly;
	else
		return ObjectFile::Reference::kBoundByName;
}

template <typename A>
ObjectFile::Reference::TargetBinding Reference<A>::getFromTargetBinding() const
{
	if ( fFromTarget.atom == NULL ) {
		if ( fFromTargetName == NULL ) 
			return ObjectFile::Reference::kDontBind;
		else
			return ObjectFile::Reference::kUnboundByName;
	}
	else {
		if ( fFromTargetName == NULL ) 
			return ObjectFile::Reference::kBoundDirectly;
		else
			return ObjectFile::Reference::kBoundByName;
	}
}



template <typename A>
class Segment : public ObjectFile::Segment
{
public:
								Segment(const macho_section<typename A::P>* sect);
	virtual const char*			getName() const						{ return fSection->segname(); }
	virtual bool				isContentReadable() const			{ return true; }
	virtual bool				isContentWritable() const			{ return fWritable; }
	virtual bool				isContentExecutable() const			{ return fExecutable; }
private:
	const macho_section<typename A::P>*		fSection;
	bool									fWritable;
	bool									fExecutable;
};

template <typename A>
Segment<A>::Segment(const macho_section<typename A::P>* sect) 
 :	fSection(sect), fWritable(true),  fExecutable(false) 
{
	if ( strcmp(fSection->segname(), "__TEXT") == 0 ) {
		fWritable = false;
		fExecutable = true;
	}
	else if ( strcmp(fSection->segname(), "__IMPORT") == 0 ) {
		fWritable = true;
		fExecutable = true;
	}
}


class DataSegment : public ObjectFile::Segment
{
public:
	virtual const char*			getName() const						{ return "__DATA"; }
	virtual bool				isContentReadable() const			{ return true; }
	virtual bool				isContentWritable() const			{ return true; }
	virtual bool				isContentExecutable() const			{ return false; }

	static DataSegment			fgSingleton;
};

DataSegment DataSegment::fgSingleton;

class LinkEditSegment : public ObjectFile::Segment
{
public:
	virtual const char*			getName() const						{ return "__LINKEDIT"; }
	virtual bool				isContentReadable() const			{ return true; }
	virtual bool				isContentWritable() const			{ return false; }
	virtual bool				isContentExecutable() const			{ return false; }

	static LinkEditSegment			fgSingleton;
};

LinkEditSegment LinkEditSegment::fgSingleton;

class BaseAtom : public ObjectFile::Atom
{
public:
												BaseAtom() : fStabsStartIndex(0), fStabsCount(0), fHasCompactUnwindInfo(false) {}

	virtual void								setSize(uint64_t size) = 0;
	virtual void								addReference(ObjectFile::Reference* ref) = 0;
	virtual void								sortReferences() = 0;
	virtual void								addLineInfo(const ObjectFile::LineInfo& info) = 0;
	virtual const ObjectFile::ReaderOptions&	getOptions() const = 0;
	virtual uint64_t							getObjectAddress() const = 0;
	virtual uint32_t							getOrdinal() const { return fOrdinal; }
	virtual void								setOrdinal(uint32_t value) { fOrdinal = value; }
	virtual const void*							getSectionRecord() const = 0;
	virtual bool								isAlias() const { return false; }
	virtual uint8_t								getLSDAReferenceKind() const { return 0; }
	virtual uint8_t								getPersonalityReferenceKind() const { return 0; }
	virtual uint32_t							getCompactUnwindEncoding(uint64_t ehAtomAddress) { return 0; }
	virtual ObjectFile::UnwindInfo::iterator	beginUnwind()					{ return fHasCompactUnwindInfo ? &fSingleUnwindInfo[0] : NULL; }
	virtual ObjectFile::UnwindInfo::iterator	endUnwind()						{ return fHasCompactUnwindInfo ? &fSingleUnwindInfo[1] : NULL; }
	virtual ObjectFile::Reference*				getLSDA();
	virtual ObjectFile::Reference*				getFDE();
	virtual Atom*								getPersonalityPointer();
	virtual void								setCompactUnwindEncoding(uint64_t ehAtomAddress);

	uint32_t									fStabsStartIndex;
	uint32_t									fStabsCount;
	uint32_t									fOrdinal;
	ObjectFile::UnwindInfo						fSingleUnwindInfo[1];
	bool										fHasCompactUnwindInfo;
};


ObjectFile::Reference* BaseAtom::getLSDA()
{
	const uint8_t groupKind = this->getLSDAReferenceKind(); 
	const std::vector<ObjectFile::Reference*>& refs = this->getReferences();
	for (std::vector<ObjectFile::Reference*>::const_iterator it=refs.begin(); it != refs.end(); it++) {
		ObjectFile::Reference* ref = *it;
		if ( (ref->getKind() == groupKind) && (ref->getTarget().getContentType() == ObjectFile::Atom::kLSDAType) ) {
			return ref;
		}
	}
	return NULL;
}

ObjectFile::Reference* BaseAtom::getFDE()
{
	const uint8_t groupKind = this->getLSDAReferenceKind(); 
	const std::vector<ObjectFile::Reference*>& refs = this->getReferences();
	for (std::vector<ObjectFile::Reference*>::const_iterator it=refs.begin(); it != refs.end(); it++) {
		ObjectFile::Reference* ref = *it;
		if ( (ref->getKind() == groupKind) && (ref->getTarget().getContentType() == ObjectFile::Atom::kCFIType) ) {
			return ref;
		}
	}
	return NULL;
}

ObjectFile::Atom* BaseAtom::getPersonalityPointer()
{
	const uint8_t personalityKind = this->getPersonalityReferenceKind(); 
	const std::vector<ObjectFile::Reference*>& refs = this->getReferences();
	for (std::vector<ObjectFile::Reference*>::const_iterator it=refs.begin(); it != refs.end(); it++) {
		ObjectFile::Reference* ref = *it;
		if ( ref->getKind() == personalityKind ) {
			if ( strcmp(ref->getTarget().getSectionName(), "__nl_symbol_ptr") == 0 )
				return &ref->getTarget();
			if ( strcmp(ref->getTarget().getSectionName(), "__pointers") == 0 )
				return &ref->getTarget();
		}
	}
	return NULL;
}


void BaseAtom::setCompactUnwindEncoding(uint64_t ehAtomAddress)
{
	fSingleUnwindInfo[0].unwindInfo = this->getCompactUnwindEncoding(ehAtomAddress);
	fHasCompactUnwindInfo = true;
}


class BaseAtomSorter
{
public:
	bool operator()(const class BaseAtom* left, const class BaseAtom* right)  {
		if ( left == right )
			return false;
		uint64_t leftAddr  =  left->getObjectAddress();
		uint64_t rightAddr = right->getObjectAddress();
		if ( leftAddr < rightAddr ) {
			return true;
		}
		else if ( leftAddr > rightAddr ) {
			return false;
		}
		else {
			// if they have same address, one might be the end of a section and the other the start of the next section
			const void* leftSection  =  left->getSectionRecord();
			const void* rightSection =  right->getSectionRecord();
			if ( leftSection != rightSection ) {
				return ( leftSection < rightSection );
			}
			// if they have same address and section, one might be an alias
			bool leftAlias  = left->isAlias();
			bool rightAlias = right->isAlias();
			if ( leftAlias && rightAlias ) {
				// sort multiple aliases for same address first by scope
				ObjectFile::Atom::Scope	leftScope  = left->getScope();
				ObjectFile::Atom::Scope	rightScope = right->getScope();
				if ( leftScope != rightScope ) {
					return ( leftScope < rightScope );
				}
				// sort multiple aliases for same address then by name
				return ( strcmp(left->getName(), right->getName()) < 0 );
			}
			else if ( leftAlias ) {
				return true;
			}
			else if ( rightAlias ) {
				return false;
			}
			else {
				// they must be tentative defintions
				switch ( left->getDefinitionKind() ) {
					case ObjectFile::Atom::kTentativeDefinition:
						// sort tentative definitions by name
						return ( strcmp(left->getName(), right->getName()) < 0 );
					case ObjectFile::Atom::kAbsoluteSymbol:
						// sort absolute symbols with same address by name
						return ( strcmp(left->getName(), right->getName()) < 0 );
					default:
						// hack for rdar://problem/5102873
						if ( !left->isZeroFill() || !right->isZeroFill() )
							warning("atom sorting error for %s and %s in %s", left->getDisplayName(), right->getDisplayName(), left->getFile()->getPath());
						break;
				}
			}
		}
		return false;
	}
};


//
// A SymbolAtom represents a chunk of a mach-o object file that has a symbol table entry
// pointing to it.  A C function or global variable is represented by one of these atoms.
//
//
template <typename A>
class SymbolAtom : public BaseAtom
{
public:
	virtual ObjectFile::Reader*					getFile() const					{ return &fOwner; }
	virtual bool								getTranslationUnitSource(const char** dir, const char** name) const
																				{ return fOwner.getTranslationUnitSource(dir, name); }
	virtual const char*							getName() const					{ return &fOwner.fStrings[fSymbol->n_strx()]; }
	virtual const char*							getDisplayName() const			{ return getName(); }
	virtual ObjectFile::Atom::Scope				getScope() const				{ return fScope; }
	virtual ObjectFile::Atom::DefinitionKind	getDefinitionKind() const		{ return ((fSymbol->n_desc() & N_WEAK_DEF) != 0)
																						? ObjectFile::Atom::kWeakDefinition : ObjectFile::Atom::kRegularDefinition; }
	virtual ObjectFile::Atom::ContentType		getContentType() const			{ return fType; }
	virtual SymbolTableInclusion				getSymbolTableInclusion() const	{ return fSymbolTableInclusion; }
	virtual	bool								dontDeadStrip() const;
	virtual bool								isZeroFill() const				{ return ((fSection->flags() & SECTION_TYPE) == S_ZEROFILL); }
	virtual bool								isThumb() const					{ return ((fSymbol->n_desc() & N_ARM_THUMB_DEF) != 0); }
	virtual uint64_t							getSize() const					{ return fSize; }
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const			{ return (std::vector<ObjectFile::Reference*>&)(fReferences); }
	virtual bool								mustRemainInSection() const		{ return true; }
	virtual const char*							getSectionName() const;
	virtual Segment<A>&							getSegment() const				{ return *fSegment; }
	virtual ObjectFile::Atom&					getFollowOnAtom() const;
	virtual std::vector<ObjectFile::LineInfo>*	getLineInfo() const				{ return (std::vector<ObjectFile::LineInfo>*)&fLineInfo; }
	virtual ObjectFile::Alignment				getAlignment() const			{ return fAlignment; }
	virtual void								copyRawContent(uint8_t buffer[]) const;
	virtual void								setScope(ObjectFile::Atom::Scope newScope)		{ fScope = newScope; }
	virtual void								setSize(uint64_t size);
	virtual void								addReference(ObjectFile::Reference* ref) { fReferences.push_back((Reference<A>*)ref); }
	virtual void								sortReferences() { std::sort(fReferences.begin(), fReferences.end(), ReferenceSorter()); }
	virtual void								addLineInfo(const  ObjectFile::LineInfo& info)	{ fLineInfo.push_back(info); }
	virtual const ObjectFile::ReaderOptions&	getOptions() const				{ return fOwner.fOptions; }
	virtual uint64_t							getObjectAddress() const 		{ return fAddress; }
	virtual const void*							getSectionRecord() const		{ return (const void*)fSection; }
	virtual uint8_t								getLSDAReferenceKind() const;
	virtual uint8_t								getPersonalityReferenceKind() const;
	virtual uint32_t							getCompactUnwindEncoding(uint64_t ehAtomAddress);

protected:
	typedef typename A::P						P;
	typedef typename A::P::E					E;
	typedef typename A::P::uint_t				pint_t;
	typedef typename A::ReferenceKinds			Kinds;
	typedef typename std::vector<Reference<A>*>			ReferenceVector;
	typedef typename ReferenceVector::iterator			ReferenceVectorIterator;		// seems to help C++ parser
	typedef typename ReferenceVector::const_iterator	ReferenceVectorConstIterator;	// seems to help C++ parser
	friend class Reader<A>;

											SymbolAtom(Reader<A>&, const macho_nlist<P>*, const macho_section<P>*);
	virtual									~SymbolAtom() {}

	Reader<A>&									fOwner;
	const macho_nlist<P>*						fSymbol;
	pint_t										fAddress;
	pint_t										fSize;
	const macho_section<P>*						fSection;
	Segment<A>*									fSegment;
	ReferenceVector								fReferences;
	std::vector<ObjectFile::LineInfo>			fLineInfo;
	ObjectFile::Atom::Scope						fScope;
	SymbolTableInclusion						fSymbolTableInclusion;
	ObjectFile::Atom::ContentType				fType;
	ObjectFile::Alignment						fAlignment;
};


template <typename A>
SymbolAtom<A>::SymbolAtom(Reader<A>& owner, const macho_nlist<P>* symbol, const macho_section<P>* section)
 : fOwner(owner), fSymbol(symbol), fAddress(0), fSize(0), fSection(section), fSegment(NULL), fType(ObjectFile::Atom::kUnclassifiedType), fAlignment(0)
{
    fSingleUnwindInfo[0].startOffset = 0;
	fSingleUnwindInfo[0].unwindInfo = 0;
	uint8_t type =  symbol->n_type();
	if ( (type & N_EXT) == 0 )
		fScope = ObjectFile::Atom::scopeTranslationUnit;
	else if ( (type & N_PEXT) != 0 )
		fScope = ObjectFile::Atom::scopeLinkageUnit;
	else
		fScope = ObjectFile::Atom::scopeGlobal;
	if ( (type & N_TYPE) == N_SECT ) {
		// real definition
 		fSegment = new Segment<A>(fSection);
		fAddress = fSymbol->n_value();
		pint_t sectionStartAddr = section->addr();
		pint_t sectionEndAddr = section->addr()+section->size();
		if ( (fAddress < sectionStartAddr) || (fAddress > (sectionEndAddr)) ) {
			throwf("malformed .o file, symbol %s with address 0x%0llX is not with section %d (%s,%s) address range of 0x%0llX to 0x%0llX",
				this->getName(), (uint64_t)fAddress, fSymbol->n_sect(), section->segname(), section->sectname(), 
				(uint64_t)sectionStartAddr, (uint64_t)(sectionEndAddr) );
		}
	}	
	else {
		warning("unknown symbol type: %d", type);
	}
	
	//fprintf(stderr, "SymbolAtom(%p) %s fAddress=0x%X\n", this, this->getDisplayName(), (uint32_t)fAddress);
	// support for .o files built with old ld64
	if ( (fSymbol->n_desc() & N_WEAK_DEF) && (strcmp(fSection->sectname(),"__picsymbolstub1__TEXT") == 0) ) {
		const char* name = this->getName();
		const int nameLen = strlen(name);
		if ( (nameLen > 6) && strcmp(&name[nameLen-5], "$stub") == 0 ) {
			// switch symbol to point at name that does not have trailing $stub
			char correctName[nameLen];
			strncpy(correctName, name, nameLen-5);
			correctName[nameLen-5] = '\0';
			const macho_nlist<P>* symbolsStart = fOwner.fSymbols;
			const macho_nlist<P>* symbolsEnd = &symbolsStart[fOwner.fSymbolCount];
			for(const macho_nlist<P>* s = symbolsStart; s < symbolsEnd; ++s) {
				if ( strcmp(&fOwner.fStrings[s->n_strx()], correctName) == 0 ) {
					fSymbol = s;
					break;
				}
			}
		}
	}
	// support for labeled stubs
	switch ( section->flags() & SECTION_TYPE ) {
		case S_SYMBOL_STUBS:
			setSize(section->reserved2());
			break;
		case S_LAZY_SYMBOL_POINTERS:
		case S_NON_LAZY_SYMBOL_POINTERS:
			setSize(sizeof(pint_t));
			break;
		case S_4BYTE_LITERALS:
			setSize(4);
			break;
		case S_8BYTE_LITERALS:
			setSize(8);
			break;
		case S_16BYTE_LITERALS:
			setSize(16);
			break;
		case S_CSTRING_LITERALS:
			setSize(strlen((char*)(fOwner.fHeader) + section->offset() + fAddress - section->addr()) + 1);
			fType = ObjectFile::Atom::kCStringType;
			break;
		case S_REGULAR:
		case S_ZEROFILL:
		case S_COALESCED:
			// size calculate later after next atom is found
			break;
	}
	
	// compute alignment
	fAlignment = ObjectFile::Alignment(fSection->align(), fAddress % (1 << fSection->align()));

	// compute whether this atom needs to be in symbol table
	if ( (fSymbol->n_desc() & REFERENCED_DYNAMICALLY) != 0) {
		fSymbolTableInclusion = ObjectFile::Atom::kSymbolTableInAndNeverStrip;
	}
	else if ( fOwner.fOptions.fForFinalLinkedImage && !fOwner.fOptions.fForStatic && (fOwner.fStrings[fSymbol->n_strx()] == 'l') ) {
		// labels beginning with a lowercase ell are automatically removed in final linked images <rdar://problem/4571042>
		// xnu code base uses a lot of asesembly labels that start with 'l', don't strip those (static executable)
		fSymbolTableInclusion = ObjectFile::Atom::kSymbolTableNotIn;
	}
	else {
		fSymbolTableInclusion = ObjectFile::Atom::kSymbolTableIn;
	}
	
	// work around malformed icc generated .o files  <rdar://problem/5349847>
	// if section starts with a symbol and that symbol address does not match section alignment, then force it to
	if ( (section->addr() == fAddress) && (fAlignment.modulus != 0) )
		fAlignment.modulus = 0;
}

template <typename A>
bool SymbolAtom<A>::dontDeadStrip() const
{
	// the symbol can have a no-dead-strip bit
	if ( (fSymbol->n_desc() & (N_NO_DEAD_STRIP|REFERENCED_DYNAMICALLY)) != 0 )
		return true;
	// or the section can have a no-dead-strip bit
	return ( fSection->flags() & S_ATTR_NO_DEAD_STRIP );
}


template <typename A>
const char*	SymbolAtom<A>::getSectionName() const
{
	if ( fOwner.fOptions.fForFinalLinkedImage ) {
		if ( strcmp(fSection->sectname(), "__textcoal_nt") == 0 )
			return "__text";
		else if ( strcmp(fSection->sectname(), "__const_coal") == 0 )
			return "__const";
		else if ( strcmp(fSection->sectname(), "__datacoal_nt") == 0 )
			return "__data";
		else if ( fOwner.fOptions.fAutoOrderInitializers && (strcmp(fSection->sectname(), "__StaticInit") == 0) )
			return "__text";
		else {
			switch ( fSection->flags() & SECTION_TYPE ) {
				case S_4BYTE_LITERALS:
				case S_8BYTE_LITERALS:
				case S_16BYTE_LITERALS:
					return "__const";
			}
		}
	}
	
	if ( strlen(fSection->sectname()) > 15 ) {
		static char temp[18];
		strncpy(temp, fSection->sectname(), 16);
		temp[17] = '\0';
		return temp;
	}
	return fSection->sectname();
}

template <typename A>
ObjectFile::Atom& SymbolAtom<A>::getFollowOnAtom() const
{
	for (ReferenceVectorConstIterator it=fReferences.begin(); it != fReferences.end(); it++) {
		Reference<A>* ref = *it;
		if ( ref->getKind() == A::kFollowOn )
			return ref->getTarget();
	}
	return *((ObjectFile::Atom*)NULL);
}


class Beyond
{
public:
	Beyond(uint64_t offset) : fOffset(offset) {}
	bool operator()(ObjectFile::Reference* ref) const {
		return ( ref->getFixUpOffset() >= fOffset );
	}
private:
	uint64_t fOffset;
};


template <typename A>
void SymbolAtom<A>::setSize(uint64_t size)
{
	// when resizing, any references beyond the new size are tossed
	if ( (fSize != 0) && (fReferences.size() > 0) ) 
		fReferences.erase(std::remove_if(fReferences.begin(), fReferences.end(), Beyond(size)), fReferences.end());
	// set new size
	fSize = size;
}

template <typename A>
void SymbolAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	// copy base bytes
	if ( isZeroFill() )
		bzero(buffer, fSize);
	else {
		uint32_t fileOffset = fSection->offset() - fSection->addr() + fAddress;
		memcpy(buffer, (char*)(fOwner.fHeader)+fileOffset, fSize);
	}
}




//
// A SymbolAliasAtom represents an alternate name for a SymbolAtom
//
//
template <typename A>
class SymbolAliasAtom : public BaseAtom
{
public:
	virtual ObjectFile::Reader*					getFile() const					{ return fAliasOf.getFile(); }
	virtual bool								getTranslationUnitSource(const char** dir, const char** name) const
																				{ return fAliasOf.getTranslationUnitSource(dir, name); }
	virtual const char*							getName() const					{ return fName; }
	virtual const char*							getDisplayName() const			{ return fName; }
	virtual ObjectFile::Atom::Scope				getScope() const				{ return fScope; }
	virtual ObjectFile::Atom::DefinitionKind	getDefinitionKind() const		{ return fAliasOf.getDefinitionKind(); }
	virtual SymbolTableInclusion				getSymbolTableInclusion() const	{ return fAliasOf.getSymbolTableInclusion(); }
	virtual	bool								dontDeadStrip() const			{ return fDontDeadStrip; }
	virtual bool								isZeroFill() const				{ return fAliasOf.isZeroFill(); }
	virtual bool								isThumb() const					{ return fAliasOf.isThumb(); }
	virtual uint64_t							getSize() const					{ return 0; }
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const			{ return (std::vector<ObjectFile::Reference*>&)(fReferences); }
	virtual bool								mustRemainInSection() const		{ return true; }
	virtual const char*							getSectionName() const			{ return fAliasOf.getSectionName(); }
	virtual Segment<A>&							getSegment() const				{ return (Segment<A>&)fAliasOf.getSegment(); }
	virtual ObjectFile::Atom&					getFollowOnAtom() const			{ return (ObjectFile::Atom&)fAliasOf; }
	virtual std::vector<ObjectFile::LineInfo>*	getLineInfo() const				{ return NULL; }
	virtual ObjectFile::Alignment				getAlignment() const			{ return  fAliasOf.getAlignment(); }
	virtual void								copyRawContent(uint8_t buffer[]) const {}
	virtual void								setScope(ObjectFile::Atom::Scope newScope)		{ fScope = newScope; }
	virtual void								setSize(uint64_t size)			{  }
	virtual void								addReference(ObjectFile::Reference* ref) { fReferences.push_back((Reference<A>*)ref); }
	virtual void								sortReferences() { std::sort(fReferences.begin(), fReferences.end(), ReferenceSorter()); }
	virtual void								addLineInfo(const  ObjectFile::LineInfo& info)	{  }
	virtual const ObjectFile::ReaderOptions&	getOptions() const				{ return fAliasOf.getOptions(); }
	virtual uint64_t							getObjectAddress() const		{ return fAliasOf.getObjectAddress(); }
	virtual const void*							getSectionRecord() const		{ return fAliasOf.getSectionRecord(); }
	virtual bool								isAlias() const					{ return true; }

protected:
	typedef typename A::P						P;
	typedef typename std::vector<Reference<A>*>			ReferenceVector;
	typedef typename ReferenceVector::iterator			ReferenceVectorIterator;		// seems to help C++ parser
	typedef typename ReferenceVector::const_iterator	ReferenceVectorConstIterator;	// seems to help C++ parser
	friend class Reader<A>;

											SymbolAliasAtom(const char* name, const macho_nlist<P>*, const BaseAtom& );
	virtual									~SymbolAliasAtom() {}

	const char*									fName;
	const BaseAtom&								fAliasOf;
	ObjectFile::Atom::Scope						fScope;
	bool										fDontDeadStrip;
	ReferenceVector								fReferences;
};


template <typename A>
SymbolAliasAtom<A>::SymbolAliasAtom(const char* name, const macho_nlist<P>* symbol, const BaseAtom& aliasOf)
 : fName(name), fAliasOf(aliasOf)
{
	//fprintf(stderr, "SymbolAliasAtom(%p) %s\n", this, name);
	if ( symbol != NULL ) {
		uint8_t type =  symbol->n_type();
		if ( (type & N_EXT) == 0 )
			fScope = ObjectFile::Atom::scopeTranslationUnit;
		else if ( (type & N_PEXT) != 0 )
			fScope = ObjectFile::Atom::scopeLinkageUnit;
		else
			fScope = ObjectFile::Atom::scopeGlobal;
		fDontDeadStrip = ((symbol->n_desc() & (N_NO_DEAD_STRIP|REFERENCED_DYNAMICALLY)) != 0);
	}
	else {
		// aliases defined on the command line are initially global scope
		fScope = ObjectFile::Atom::scopeGlobal;
		fDontDeadStrip = false;
	}
	// add follow-on reference to real atom	
	new Reference<A>(A::kFollowOn, AtomAndOffset(this), AtomAndOffset((ObjectFile::Atom*)&aliasOf));
}


//
// A TentativeAtom represents a C "common" or "tentative" defintion of data.
// For instance, "int foo;" is neither a declaration or a definition and
// is represented by a TentativeAtom.
//
template <typename A>
class TentativeAtom : public BaseAtom
{
public:
	virtual ObjectFile::Reader*					getFile() const					{ return &fOwner; }
	virtual bool								getTranslationUnitSource(const char** dir, const char** name) const
																				{ return fOwner.getTranslationUnitSource(dir, name); }
	virtual const char*							getName() const					{ return &fOwner.fStrings[fSymbol->n_strx()]; }
	virtual const char*							getDisplayName() const			{ return getName(); }
	virtual ObjectFile::Atom::Scope				getScope() const				{ return fScope; }
	virtual ObjectFile::Atom::DefinitionKind	getDefinitionKind() const		{ return ObjectFile::Atom::kTentativeDefinition; }
	virtual bool								isZeroFill() const				{ return true; }
	virtual bool								isThumb() const					{ return false; }
	virtual SymbolTableInclusion				getSymbolTableInclusion() const	{ return ((fSymbol->n_desc() & REFERENCED_DYNAMICALLY) != 0)
																						? ObjectFile::Atom::kSymbolTableInAndNeverStrip : ObjectFile::Atom::kSymbolTableIn; }
	virtual	bool								dontDeadStrip() const			{ return ((fSymbol->n_desc() & (N_NO_DEAD_STRIP|REFERENCED_DYNAMICALLY)) != 0); }
	virtual uint64_t							getSize() const					{ return fSymbol->n_value(); }
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const			{ return fgNoReferences; }
	virtual bool								mustRemainInSection() const		{ return true; }
	virtual const char*							getSectionName() const;
	virtual ObjectFile::Segment&				getSegment() const				{ return DataSegment::fgSingleton; }
	virtual ObjectFile::Atom&					getFollowOnAtom() const			{ return *(ObjectFile::Atom*)NULL; }
	virtual std::vector<ObjectFile::LineInfo>*	getLineInfo() const				{ return NULL; }
	virtual ObjectFile::Alignment				getAlignment() const;
	virtual void								copyRawContent(uint8_t buffer[]) const;
	virtual void								setScope(ObjectFile::Atom::Scope newScope)		{ fScope = newScope; }
	virtual void								setSize(uint64_t size)			{ }
	virtual void								addReference(ObjectFile::Reference* ref) { throw "ld: can't add references"; }
	virtual void								sortReferences() { }
	virtual void								addLineInfo(const  ObjectFile::LineInfo& info)	{ throw "ld: can't add line info to tentative definition"; }
	virtual const ObjectFile::ReaderOptions&	getOptions() const				{ return fOwner.fOptions; }
	virtual uint64_t							getObjectAddress() const		{ return ULLONG_MAX; }
	virtual const void*							getSectionRecord() const		{ return NULL; }

protected:
	typedef typename A::P					P;
	typedef typename A::P::E				E;
	typedef typename A::P::uint_t			pint_t;
	typedef typename A::ReferenceKinds		Kinds;
	friend class Reader<A>;

											TentativeAtom(Reader<A>&, const macho_nlist<P>*);
	virtual									~TentativeAtom() {}

	Reader<A>&									fOwner;
	const macho_nlist<P>*						fSymbol;
	ObjectFile::Atom::Scope						fScope;
	static std::vector<ObjectFile::Reference*>	fgNoReferences;
};

template <typename A>
std::vector<ObjectFile::Reference*> TentativeAtom<A>::fgNoReferences;

template <typename A>
TentativeAtom<A>::TentativeAtom(Reader<A>& owner, const macho_nlist<P>* symbol)
 : fOwner(owner), fSymbol(symbol)
{
	uint8_t type =  symbol->n_type();
	if ( (type & N_EXT) == 0 )
		fScope = ObjectFile::Atom::scopeTranslationUnit;
	else if ( (type & N_PEXT) != 0 )
		fScope = ObjectFile::Atom::scopeLinkageUnit;
	else
		fScope = ObjectFile::Atom::scopeGlobal;
	if ( ((type & N_TYPE) == N_UNDF) && (symbol->n_value() != 0) ) {
		// tentative definition
	}
	else {
		warning("unknown symbol type: %d", type);
	}
	//fprintf(stderr, "TentativeAtom(%p) %s\n", this, this->getDisplayName());
}


template <typename A>
ObjectFile::Alignment TentativeAtom<A>::getAlignment() const
{
	uint8_t alignment = GET_COMM_ALIGN(fSymbol->n_desc());
	if ( alignment == 0 ) {
		// common symbols align to their size
		// that is, a 4-byte common aligns to 4-bytes
		// if this size is not a power of two, 
		// then round up to the next power of two
		uint64_t size = this->getSize();
		alignment = 63 - (uint8_t)__builtin_clzll(size);
		if ( size != (1ULL << alignment) )
			++alignment;
	}
	// limit alignment of extremely large commons to 2^15 bytes (8-page)
	if ( alignment < 12 )
		return ObjectFile::Alignment(alignment);
	else
		return ObjectFile::Alignment(12);
}

template <typename A>
const char* TentativeAtom<A>::getSectionName() const
{
	if ( fOwner.fOptions.fForFinalLinkedImage || fOwner.fOptions.fMakeTentativeDefinitionsReal )
		return "__common"; 
	else
		return "._tentdef"; 
}


template <typename A>
void TentativeAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	bzero(buffer, getSize());
}


//
// An AnonymousAtom represents compiler generated data that has no name.
// For instance, a literal C-string or a 64-bit floating point constant
// is represented by an AnonymousAtom.
//
template <typename A>
class AnonymousAtom : public BaseAtom
{
public:
	virtual ObjectFile::Reader*					getFile() const					{ return &fOwner; }
	virtual bool								getTranslationUnitSource(const char** dir, const char** name) const { return false; }
	virtual const char*							getName() const					{ return fSynthesizedName; }
	virtual const char*							getDisplayName() const;
	virtual ObjectFile::Atom::Scope				getScope() const;
	virtual ObjectFile::Atom::DefinitionKind	getDefinitionKind() const       { return fKind; }
	virtual ObjectFile::Atom::ContentType		getContentType() const			{ return fType; }
	virtual ObjectFile::Atom::SymbolTableInclusion getSymbolTableInclusion() const	{ return fSymbolTableInclusion; }
	virtual	bool								dontDeadStrip() const			{ return fDontDeadStrip; }
	virtual bool								isZeroFill() const;
	virtual bool								isThumb() const					{ return false; }
	virtual uint64_t							getSize() const					{ return fSize; }
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const			{ return (std::vector<ObjectFile::Reference*>&)(fReferences); }
	virtual bool								mustRemainInSection() const		{ return true; }
	virtual const char*							getSectionName() const;
	virtual Segment<A>&							getSegment() const				{ return *fSegment; }
	virtual ObjectFile::Atom&					getFollowOnAtom() const;
	virtual std::vector<ObjectFile::LineInfo>*	getLineInfo() const				{ return NULL; }
	virtual ObjectFile::Alignment				getAlignment() const;
	virtual void								copyRawContent(uint8_t buffer[]) const;
	virtual void								setScope(ObjectFile::Atom::Scope newScope)	{ fScope = newScope; }
	virtual void								setSize(uint64_t size)			{ fSize = size; }
	virtual void								addReference(ObjectFile::Reference* ref) { fReferences.push_back((Reference<A>*)ref); }
	virtual void								sortReferences() { std::sort(fReferences.begin(), fReferences.end(), ReferenceSorter()); }
	virtual void								addLineInfo(const  ObjectFile::LineInfo& info);
	virtual const ObjectFile::ReaderOptions&	getOptions() const				{ return fOwner.fOptions; }
	virtual uint64_t							getObjectAddress() const		{ return fAddress; }
	virtual const void*							getSectionRecord() const		{ return (const void*)fSection; }
	BaseAtom*									redirectTo()					{ return fRedirect; }
	bool										isWeakImportStub()				{ return fWeakImportStub; }
	void										resolveName();
	virtual uint8_t								getLSDAReferenceKind() const;
	virtual uint8_t								getPersonalityReferenceKind() const;
	virtual uint32_t							getCompactUnwindEncoding(uint64_t ehAtomAddress);
	
protected:
	typedef typename A::P						P;
	typedef typename A::P::E					E;
	typedef typename A::P::uint_t				pint_t;
	typedef typename A::ReferenceKinds			Kinds;
	typedef typename std::vector<Reference<A>*>			ReferenceVector;
	typedef typename ReferenceVector::iterator			ReferenceVectorIterator;		// seems to help C++ parser
	typedef typename ReferenceVector::const_iterator	ReferenceVectorConstIterator;	// seems to help C++ parser
	friend class Reader<A>;

											AnonymousAtom(Reader<A>&, const macho_section<P>*, pint_t addr, pint_t size);
	virtual									~AnonymousAtom() {}
	static bool									cstringsHaveLabels();

	Reader<A>&									fOwner;
	const char*									fSynthesizedName;
	const char*									fDisplayName;
	const macho_section<P>*						fSection;
	pint_t										fAddress;
	pint_t										fSize;
	Segment<A>*									fSegment;
	ReferenceVector								fReferences;
	BaseAtom*									fRedirect;
	bool										fDontDeadStrip;
	bool										fWeakImportStub;
	ObjectFile::Atom::SymbolTableInclusion		fSymbolTableInclusion;
	ObjectFile::Atom::Scope						fScope;
    ObjectFile::Atom::DefinitionKind            fKind;
	ObjectFile::Atom::ContentType				fType;
};

template <typename A>
AnonymousAtom<A>::AnonymousAtom(Reader<A>& owner, const macho_section<P>* section, pint_t addr, pint_t size)
 : fOwner(owner), fSynthesizedName(NULL), fDisplayName(NULL), fSection(section), fAddress(addr), fSize(size), 
	fSegment(NULL), fDontDeadStrip(true), fWeakImportStub(false), fSymbolTableInclusion(ObjectFile::Atom::kSymbolTableNotIn),
	fScope(ObjectFile::Atom::scopeTranslationUnit), fKind(ObjectFile::Atom::kRegularDefinition), 
	fType(ObjectFile::Atom::kUnclassifiedType)
{
	fSegment = new Segment<A>(fSection);
	fRedirect = this;
	uint8_t type = fSection->flags() & SECTION_TYPE;
	//fprintf(stderr, "AnonymousAtom(%p) addr=0x%llX in %s from %s\n", this, (long long)addr, section->sectname(), owner.getPath());
	switch ( type ) {
		case S_ZEROFILL:
			{
				asprintf((char**)&fSynthesizedName, "zero-fill-at-0x%08X", addr);
			}
			break;
		case S_COALESCED:
		case S_REGULAR:
			if ( section == owner.fehFrameSection ) {
				if ( fSize == 1 ) {
					// is CIE
					fSize = 0;
					fDontDeadStrip = false;
					if ( fOwner.fOptions.fForFinalLinkedImage ) 
						fSynthesizedName = "CIE";
					else
						fSynthesizedName = "EH_frame1";
				}
				else {
					// is FDE
					fSynthesizedName = ".eh_PENDING";
					fDontDeadStrip = false;
					owner.fAtomsPendingAName.push_back(this);
				}
				fType = ObjectFile::Atom::kCFIType;
				// FDEs and CIEs don't need to be in symbol table of final linked images <rdar://problem/4180168>
				if ( !fOwner.fOptions.fNoEHLabels ) 
					fSymbolTableInclusion = ObjectFile::Atom::kSymbolTableIn;
			}
			else if ( (strcmp(section->sectname(), "__class") == 0) && (strcmp(section->segname(), "__OBJC") == 0) && owner.fAppleObjc ) {
				// special case ObjC classes to synthesize .objc_class_name_* symbols, for Apple runtime only
				fSynthesizedName = ".objc_class_name_PENDING";
				owner.fAtomsPendingAName.push_back(this);
				owner.fSectionsWithAtomsPendingAName.insert(fSection);
				if ( fOwner.fOptions.fForFinalLinkedImage ) 
					fSymbolTableInclusion = ObjectFile::Atom::kSymbolTableIn;
				else
					fSymbolTableInclusion = ObjectFile::Atom::kSymbolTableInAsAbsolute;
				fScope = ObjectFile::Atom::scopeGlobal;
			}
			else if ( strcmp(fSection->sectname(), "__cstring") == 0 ) {
				// handle .o files created by old ld64 -r that are missing cstring section type
				const char* str = (char*)(owner.fHeader) + section->offset() + addr - section->addr();
				asprintf((char**)&fSynthesizedName, "cstring=%s", str);
			}
			else if ((strcmp(section->sectname(), "__cfstring") == 0) && (strcmp(section->segname(), "__DATA") == 0)) {
				fSynthesizedName = "cfstring-pointer-name-PENDING";
				fScope = ObjectFile::Atom::scopeLinkageUnit;
				owner.fAtomsPendingAName.push_back(this);
				owner.fSectionsWithAtomsPendingAName.insert(fSection);
				fDontDeadStrip = false;
				fKind = ObjectFile::Atom::kWeakDefinition;
			}
			else if ( (fSection->flags() & S_ATTR_SOME_INSTRUCTIONS) != 0 ) {
				fDontDeadStrip = false;
				asprintf((char**)&fSynthesizedName, "anon-func-0x%X", addr);
			}
			else if ( strncmp(fSection->sectname(), "__gcc_except_tab",16) == 0 ) {
				fType = ObjectFile::Atom::kLSDAType;
				fDontDeadStrip = false;
				fSynthesizedName = ".lsda_PENDING";
				owner.fAtomsPendingAName.push_back(this);
				if ( !fOwner.fOptions.fNoEHLabels ) 
					fSymbolTableInclusion = ObjectFile::Atom::kSymbolTableIn;
			}
			break;
		case S_CSTRING_LITERALS:
			{
				const char* str = (char*)(owner.fHeader) + section->offset() + addr - section->addr();
				if ( (strcmp(fSection->sectname(), "__cstring") == 0) && (strcmp(section->segname(), "__TEXT") == 0) ) 
					asprintf((char**)&fSynthesizedName, "cstring=%s", str);
				else
					asprintf((char**)&fSynthesizedName, "cstring%s%s=%s", fSection->segname(), fSection->sectname(), str);
				fScope = ObjectFile::Atom::scopeLinkageUnit;
				fKind = ObjectFile::Atom::kWeakDefinition;
				fType = ObjectFile::Atom::kCStringType;
				fDontDeadStrip = false;
				if ( !fOwner.fOptions.fForFinalLinkedImage && cstringsHaveLabels() ) 
					fSymbolTableInclusion = ObjectFile::Atom::kSymbolTableIn;
			}
			break;
		case S_4BYTE_LITERALS:
			{
				uint32_t value =  E::get32(*(uint32_t*)(((uint8_t*)owner.fHeader) + section->offset() + addr - section->addr()));
				asprintf((char**)&fSynthesizedName, "4-byte-literal=0x%08X", value);
				fScope = ObjectFile::Atom::scopeLinkageUnit;
				fKind = ObjectFile::Atom::kWeakDefinition;
				fDontDeadStrip = false;
			}
			break;
		case S_8BYTE_LITERALS:
			{
				uint64_t value =  E::get64(*(uint64_t*)(((uint8_t*)owner.fHeader) + section->offset() + addr - section->addr()));
				asprintf((char**)&fSynthesizedName, "8-byte-literal=0x%016llX", value);
				fScope = ObjectFile::Atom::scopeLinkageUnit;
				fKind = ObjectFile::Atom::kWeakDefinition;
				fDontDeadStrip = false;
			}
			break;
		case S_16BYTE_LITERALS:
			{
				uint64_t value1 =  E::get64(*(uint64_t*)(((uint8_t*)owner.fHeader) + section->offset() + addr - section->addr()));
				uint64_t value2 =  E::get64(*(uint64_t*)(((uint8_t*)owner.fHeader) + section->offset() + addr + 8 - section->addr()));
				asprintf((char**)&fSynthesizedName, "16-byte-literal=0x%016llX,%016llX", value1, value2);
				fScope = ObjectFile::Atom::scopeLinkageUnit;
				fKind = ObjectFile::Atom::kWeakDefinition;
				fDontDeadStrip = false;
			}
			break;
		case S_LITERAL_POINTERS:
			{
				//uint32_t literalNameAddr =  P::getP(*(pint_t*)(((uint8_t*)owner.fHeader) + section->offset() + addr - section->addr()));
				//const char* str = (char*)(owner.fHeader) + section->offset() + literalNameAddr - section->addr();
				//asprintf((char**)&fSynthesizedName, "literal-pointer@%s@%s@%s", section->segname(), section->sectname(), str);
				fSynthesizedName = "literal-pointer-name-PENDING";
				fScope = ObjectFile::Atom::scopeLinkageUnit;
				fKind = ObjectFile::Atom::kWeakDefinition;
				fDontDeadStrip = false;
				owner.fAtomsPendingAName.push_back(this);
				owner.fSectionsWithAtomsPendingAName.insert(fSection);
			}
			break;
		case S_MOD_INIT_FUNC_POINTERS:
				asprintf((char**)&fSynthesizedName, "initializer$%d", (addr - (uint32_t)fSection->addr())/sizeof(pint_t));
				break;
		case S_MOD_TERM_FUNC_POINTERS:
				asprintf((char**)&fSynthesizedName, "terminator$%d", (addr - (uint32_t)fSection->addr())/sizeof(pint_t));
				break;
		case S_SYMBOL_STUBS:
			{
				uint32_t index = (fAddress - fSection->addr()) / fSection->reserved2();
				index += fSection->reserved1();
				uint32_t symbolIndex = E::get32(fOwner.fIndirectTable[index]);
				const macho_nlist<P>* sym = &fOwner.fSymbols[symbolIndex];
				uint32_t strOffset = sym->n_strx();
				// want name to not have $stub suffix, this is what automatic stub generation expects
				fSynthesizedName = &fOwner.fStrings[strOffset];
				// check for weak import
				fWeakImportStub = fOwner.isWeakImportSymbol(sym);
				// sometimes the compiler gets confused and generates a stub to a static function
				// if so, we should redirect any call to the stub to be calls to the real static function atom
				if ( ((sym->n_type() & N_TYPE) != N_UNDF) && ((sym->n_type() & N_EXT) == 0) ) {
					BaseAtom* staticAtom = fOwner.findAtomByName(fSynthesizedName);
					if ( staticAtom != NULL ) 
						fRedirect = staticAtom;
				}
				fKind = ObjectFile::Atom::kWeakDefinition;
				// might be a spurious stub for a static function, make stub static too
				if ( (sym->n_type() & N_EXT) == 0 ) 
					fScope = ObjectFile::Atom::scopeTranslationUnit;
				else
					fScope = ObjectFile::Atom::scopeLinkageUnit;
			}
			break;
		case S_LAZY_SYMBOL_POINTERS:
		case S_NON_LAZY_SYMBOL_POINTERS:
			{
				// transform i386 __IMPORT/__pointers to __DATA/__nl_symbol_ptr when 
				// generating the new compressed LINKEDIT format
				if ( (type == S_NON_LAZY_SYMBOL_POINTERS) && fOwner.fOptions.fMakeCompressedDyldInfo && (strcmp(fSection->segname(),"__IMPORT") == 0) ) {
					macho_section<P>* dummySection = new macho_section<P>(*fSection);
					dummySection->set_segname("__DATA");
					dummySection->set_sectname("__nl_symbol_ptr");
					fSection = dummySection;
					fSegment = new Segment<A>(fSection);
				}
				
				fDontDeadStrip = false;
				fScope = ObjectFile::Atom::scopeLinkageUnit;
				uint32_t index = (fAddress - fSection->addr()) / sizeof(pint_t);
				index += fSection->reserved1();
				uint32_t symbolIndex = E::get32(fOwner.fIndirectTable[index]);
				if ( symbolIndex == INDIRECT_SYMBOL_LOCAL ) {
					// Silly codegen with non-lazy pointer to a local symbol
					uint32_t fileOffset = fSection->offset() - fSection->addr() + fAddress;
					pint_t nonLazyPtrValue = P::getP(*((pint_t*)((char*)(fOwner.fHeader)+fileOffset)));
					// All atoms not created yet, so we need to scan symbol table
					const macho_nlist<P>* closestSym = NULL; 
					const macho_nlist<P>* end = &fOwner.fSymbols[fOwner.fSymbolCount];
					for (const macho_nlist<P>* sym =  fOwner.fSymbols; sym < end; ++sym) {
						if ( ((sym->n_type() & N_TYPE) == N_SECT) 
						 && ((sym->n_type() & N_STAB) == 0) ) {
							if ( sym->n_value() == nonLazyPtrValue ) {
								const char* name = &fOwner.fStrings[sym->n_strx()];
								char* str = new char[strlen(name)+16];
								strcpy(str, name);
								strcat(str, "$non_lazy_ptr");
								fSynthesizedName = str;
								// add direct reference to target later, because its atom may not be constructed yet
								fOwner.fLocalNonLazys.push_back(this);
								fScope = ObjectFile::Atom::scopeTranslationUnit;
								return;
							}
							else if ( (sym->n_value() < nonLazyPtrValue) && ((closestSym == NULL) || (sym->n_value() > closestSym->n_value())) ) {
								closestSym = sym;
							}
						}
					}
					// add direct reference to target later, because its atom may not be constructed yet
					if ( closestSym != NULL ) {
						const char* name = &fOwner.fStrings[closestSym->n_strx()];
						char* str;
						asprintf(&str, "%s+%u$non_lazy_ptr", name, nonLazyPtrValue - closestSym->n_value());
						fSynthesizedName = str;
					}
					else {
						fSynthesizedName = "$interior$non_lazy_ptr";
					}
					fScope = ObjectFile::Atom::scopeTranslationUnit;
					fOwner.fLocalNonLazys.push_back(this);
					return;
				}
				const macho_nlist<P>* targetSymbol = &fOwner.fSymbols[symbolIndex];
				const char* name = &fOwner.fStrings[targetSymbol->n_strx()];
				char* str = new char[strlen(name)+16];
				strcpy(str, name);
				if ( type == S_LAZY_SYMBOL_POINTERS )
					strcat(str, "$lazy_ptr");
				else
					strcat(str, "$non_lazy_ptr");
				fSynthesizedName = str;

				if ( type == S_NON_LAZY_SYMBOL_POINTERS )
					fKind = ObjectFile::Atom::kWeakDefinition;

				if ( (targetSymbol->n_type() & N_EXT) == 0 ) {
					// target is translation unit scoped, so add direct reference to target
					//fOwner.makeReference(A::kPointer, addr, targetSymbol->n_value());
					new Reference<A>(A::kPointer, AtomAndOffset(this), fOwner.findAtomAndOffset(targetSymbol->n_value()));
				}
				else {	
					if ( fOwner.isWeakImportSymbol(targetSymbol) )
						new Reference<A>(A::kPointerWeakImport, AtomAndOffset(this), name, 0);
					else
						new Reference<A>(A::kPointer, AtomAndOffset(this), name, 0);
				}
			}
			break;
		default:
			throwf("section type %d not supported with address=0x%08X", type, addr);
	}
	//fprintf(stderr, "AnonymousAtom(%p) %s \n", this, this->getDisplayName());
}

// x86_64 uses L labels on cstrings to allow relocs with addends
template <> bool AnonymousAtom<x86_64>::cstringsHaveLabels() { return true; }
template <typename A> bool AnonymousAtom<A>::cstringsHaveLabels() { return false; }

template <typename A>
void AnonymousAtom<A>::addLineInfo(const ObjectFile::LineInfo& info) 
{ 
	// <rdar://problem/6545406> don't warn if line table has entries for stubs
	if ( (fSection->flags() & SECTION_TYPE) != S_SYMBOL_STUBS )
		warning("can't add line info to anonymous symbol %s from %s", this->getDisplayName(), this->getFile()->getPath()); 
}

template <typename A>
void AnonymousAtom<A>::resolveName()
{
	if ( (strcmp(fSection->sectname(), "__class") == 0) && (strcmp(fSection->segname(), "__OBJC") == 0) ) {
		std::vector<ObjectFile::Reference*>&  references = this->getReferences();
		// references are not yet sorted, so scan the vector
		for (std::vector<ObjectFile::Reference*>::iterator rit=references.begin(); rit != references.end(); rit++) {
			if ( ((*rit)->getFixUpOffset() == sizeof(pint_t)) && ((*rit)->getKind() == A::kPointer) ) {
				const char* superStr = (*rit)->getTargetName();
				if ( strncmp(superStr, "cstring", 7) == 0 ) {
					const char* superClassName;
					asprintf((char**)&superClassName, ".objc_class_name_%s", &superStr[8]);
					new Reference<A>(A::kNoFixUp, AtomAndOffset(this), superClassName, 0);
				}
				break;
			}
		}
		for (std::vector<ObjectFile::Reference*>::iterator rit=references.begin(); rit != references.end(); rit++) {
			if ( ((*rit)->getFixUpOffset() == 2*sizeof(pint_t)) && ((*rit)->getKind() == A::kPointer) ) {
				const char* classStr = (*rit)->getTargetName();
				if ( strncmp(classStr, "cstring", 7) == 0 ) {
					asprintf((char**)&fSynthesizedName, ".objc_class_name_%s", &classStr[8]);
				}
				break;
			}
		}
	}
	else if ( (fSection->flags() & SECTION_TYPE) == S_LITERAL_POINTERS) {
		std::vector<ObjectFile::Reference*>&  references = this->getReferences();
		if ( references.size() < 1 )
			throwf("S_LITERAL_POINTERS section %s,%s missing relocs", fSection->segname(), fSection->sectname());
		ObjectFile::Reference* ref = references[0];
		const char* str = ref->getTargetName();
		if ( strncmp(str, "cstring", 7) == 0 ) {
			asprintf((char**)&fSynthesizedName, "literal-pointer@%s@%s@%s", fSection->segname(), fSection->sectname(), &str[8]);
		}
	}
	else if ( (strcmp(fSection->sectname(), "__cfstring") == 0) && (strcmp(fSection->segname(), "__DATA") == 0) ) {
		// references are not yet sorted, so scan the vector
		std::vector<ObjectFile::Reference*>&  references = this->getReferences();
		for (std::vector<ObjectFile::Reference*>::iterator rit=references.begin(); rit != references.end(); rit++) {
			if ( ((*rit)->getFixUpOffset() == 2*sizeof(pint_t)) && ((*rit)->getKind() == A::kPointer) ) {
				const char* superStr = (*rit)->getTargetName();
				if ( (superStr != NULL) && (strncmp(superStr, "cstring=", 8) == 0) ) {
					asprintf((char**)&fSynthesizedName, "cfstring=%s", &superStr[8]);
				}
				else {
					// compiled with -fwritable-strings or a non-ASCII string 
					fKind = ObjectFile::Atom::kRegularDefinition; // these are not coalescable
					fScope = ObjectFile::Atom::scopeTranslationUnit;
					fSynthesizedName = "cfstring-not-coalesable";
					if ( (*rit)->getTargetOffset() != 0 )
						warning("-fwritable-strings not compatible with literal CF/NSString in %s", fOwner.getPath());
				}
				break;
			}
		}
	}
	else if ( fSection == fOwner.fehFrameSection ) {
		// give name to FDE
		ObjectFile::Atom* funcAtom = fOwner.getFunctionAtomFromFDEAddress(fAddress);
		if ( funcAtom != NULL )
			asprintf((char**)&fSynthesizedName, "%s.eh", funcAtom->getDisplayName());
	}
	else if ( fOwner.fLSDAAtoms.count(this) != 0) {
		// give name to LSDA
		ObjectFile::Atom* funcAtom = fOwner.getFunctionAtomFromLSDAAddress(fAddress);
		if ( funcAtom != NULL )
			asprintf((char**)&fSynthesizedName, "%s.lsda", funcAtom->getDisplayName());
	}
}


template <typename A>
const char* AnonymousAtom<A>::getDisplayName() const
{
	if ( fSynthesizedName != NULL )
		return fSynthesizedName;

	if ( fDisplayName != NULL )
		return fDisplayName;

	if ( (fSection->flags() & SECTION_TYPE) == S_CSTRING_LITERALS ) {
		uint32_t fileOffset = fSection->offset() - fSection->addr() + fAddress;
		asprintf((char**)&fDisplayName, "atom string literal: \"%s\"", (char*)(fOwner.fHeader)+fileOffset);
	}
	else {
		asprintf((char**)&fDisplayName, "%s@%d", fSection->sectname(), fAddress - (uint32_t)fSection->addr() );
	}
	return fDisplayName;
}


template <typename A>
ObjectFile::Atom::Scope AnonymousAtom<A>::getScope() const
{
	return fScope;
}


template <typename A>
bool AnonymousAtom<A>::isZeroFill() const
{
	return ( (fSection->flags() & SECTION_TYPE) == S_ZEROFILL );
}


template <typename A>
const char*	AnonymousAtom<A>::getSectionName() const
{
	if ( fOwner.fOptions.fForFinalLinkedImage ) {
		switch ( fSection->flags() & SECTION_TYPE ) {
			case S_4BYTE_LITERALS:
			case S_8BYTE_LITERALS:
			case S_16BYTE_LITERALS:
				return "__const";
		}
	}
	
	if ( strlen(fSection->sectname()) > 15 ) {
		static char temp[18];
		strncpy(temp, fSection->sectname(), 16);
		temp[17] = '\0';
		return temp;
	}
	return fSection->sectname();
}

template <typename A>
ObjectFile::Alignment AnonymousAtom<A>::getAlignment() const
{
	// FDEs and CIEs are always packed together in a final linked image, so ignore section alignment
	if ( fType == ObjectFile::Atom::kCFIType )
		return ObjectFile::Alignment(0);
		
	switch ( fSection->flags() & SECTION_TYPE ) {
		case S_4BYTE_LITERALS:
			return ObjectFile::Alignment(2);
		case S_8BYTE_LITERALS:
			return ObjectFile::Alignment(3);
		case S_16BYTE_LITERALS:
			return ObjectFile::Alignment(4);
		case S_NON_LAZY_SYMBOL_POINTERS:
			return ObjectFile::Alignment((uint8_t)log2(sizeof(pint_t)));
		case S_CSTRING_LITERALS:
			if ( ! fOwner.fOptions.fForFinalLinkedImage )
				return ObjectFile::Alignment(fSection->align());
		default:
			return ObjectFile::Alignment(fSection->align(), fAddress % (1 << fSection->align()));
	}
}


template <typename A>
ObjectFile::Atom& AnonymousAtom<A>::getFollowOnAtom() const
{
	for (ReferenceVectorConstIterator it=fReferences.begin(); it != fReferences.end(); it++) {
		Reference<A>* ref = *it;
		if ( ref->getKind() == A::kFollowOn )
			return ref->getTarget();
	}
	return *((ObjectFile::Atom*)NULL);
}

template <typename A>
void AnonymousAtom<A>::copyRawContent(uint8_t buffer[]) const
{
	// copy base bytes
	if ( isZeroFill() )
		bzero(buffer, fSize);
	else {
		uint32_t fileOffset = fSection->offset() - fSection->addr() + fAddress;
		memcpy(buffer, (char*)(fOwner.fHeader)+fileOffset, fSize);
	}
}

//
// An AbsoluteAtom represents an N_ABS symbol which can only be created in 
// assembly language and usable by static executables such as the kernel/
//
template <typename A>
class AbsoluteAtom : public BaseAtom
{
public:
	virtual ObjectFile::Reader*					getFile() const					{ return &fOwner; }
	virtual bool								getTranslationUnitSource(const char** dir, const char** name) const
																				{ return fOwner.getTranslationUnitSource(dir, name); }
	virtual const char*							getName() const					{ return &fOwner.fStrings[fSymbol->n_strx()]; }
	virtual const char*							getDisplayName() const			{ return getName(); }
	virtual ObjectFile::Atom::Scope				getScope() const				{ return fScope; }
	virtual ObjectFile::Atom::DefinitionKind	getDefinitionKind() const		{ return ObjectFile::Atom::kAbsoluteSymbol; }
	virtual bool								isZeroFill() const				{ return false; }
	virtual bool								isThumb() const					{ return ((fSymbol->n_desc() & N_ARM_THUMB_DEF) != 0); }
	virtual SymbolTableInclusion				getSymbolTableInclusion() const	{ return ObjectFile::Atom::kSymbolTableInAsAbsolute; }
	virtual	bool								dontDeadStrip() const			{ return false; }
	virtual uint64_t							getSize() const					{ return 0; }
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const			{ return fgNoReferences; }
	virtual bool								mustRemainInSection() const		{ return true; }
	virtual const char*							getSectionName() const			{ return "._absolute"; } 
	virtual ObjectFile::Segment&				getSegment() const				{ return LinkEditSegment::fgSingleton; }
	virtual ObjectFile::Atom&					getFollowOnAtom() const			{ return *(ObjectFile::Atom*)NULL; }
	virtual std::vector<ObjectFile::LineInfo>*	getLineInfo() const				{ return NULL; }
	virtual ObjectFile::Alignment				getAlignment() const			{ return ObjectFile::Alignment(0); }
	virtual void								copyRawContent(uint8_t buffer[]) const	{ }
	virtual void								setScope(ObjectFile::Atom::Scope newScope)		{ fScope = newScope; }
	virtual void								setSize(uint64_t size)			{ }
	virtual void								addReference(ObjectFile::Reference* ref) { throw "ld: can't add references"; }
	virtual void								sortReferences()				{ }
	virtual void								addLineInfo(const  ObjectFile::LineInfo& info)	{ throw "ld: can't add line info to tentative definition"; }
	virtual const ObjectFile::ReaderOptions&	getOptions() const				{ return fOwner.fOptions; }
	virtual uint64_t							getObjectAddress() const		{ return fSymbol->n_value(); }
	virtual void								setSectionOffset(uint64_t offset) { /* don't let fSectionOffset be altered*/ }
	virtual const void*							getSectionRecord() const		{ return NULL; }

protected:
	typedef typename A::P					P;
	typedef typename A::P::E				E;
	typedef typename A::P::uint_t			pint_t;
	typedef typename A::ReferenceKinds		Kinds;
	friend class Reader<A>;

											AbsoluteAtom(Reader<A>&, const macho_nlist<P>*);
	virtual									~AbsoluteAtom() {}

	Reader<A>&									fOwner;
	const macho_nlist<P>*						fSymbol;
	ObjectFile::Atom::Scope						fScope;
	static std::vector<ObjectFile::Reference*>	fgNoReferences;
};

template <typename A>
std::vector<ObjectFile::Reference*> AbsoluteAtom<A>::fgNoReferences;

template <typename A>
AbsoluteAtom<A>::AbsoluteAtom(Reader<A>& owner, const macho_nlist<P>* symbol)
 : fOwner(owner), fSymbol(symbol)
{
	// store absolute adress in fSectionOffset
	fSectionOffset = symbol->n_value();
	// compute scope
	uint8_t type =  symbol->n_type();
	if ( (type & N_EXT) == 0 )
		fScope = ObjectFile::Atom::scopeTranslationUnit;
	else if ( (type & N_PEXT) != 0 )
		fScope = ObjectFile::Atom::scopeLinkageUnit;
	else
		fScope = ObjectFile::Atom::scopeGlobal;
	//fprintf(stderr, "AbsoluteAtom(%p) %s\n", this, this->getDisplayName());
}




///
/// ObjectFileAddressSpace is used as a template parameter to UnwindCursor for parsing
/// dwarf CFI information in an object file.   
///
template <typename A>
class ObjectFileAddressSpace
{
public:
						ObjectFileAddressSpace(Reader<A>& reader);

		typedef typename A::P::uint_t	pint_t;
		typedef typename A::P			P;
		typedef typename A::P::uint_t	sint_t;

		uint8_t			get8(pint_t addr);
		uint16_t		get16(pint_t addr);
		uint32_t		get32(pint_t addr);
		uint64_t		get64(pint_t addr);
		pint_t			getP(pint_t addr);
		uint64_t		getULEB128(pint_t& addr, pint_t end);
		int64_t			getSLEB128(pint_t& addr, pint_t end);
		pint_t			getEncodedP(pint_t& addr, pint_t end, uint8_t encoding);
private:
	const void*			mappedAddress(pint_t addr, pint_t* relocTarget=NULL);
	pint_t				relocated(uint32_t sectOffset, uint32_t relocsOffset, uint32_t relocsCount);
	void				buildRelocatedMap(const macho_section<P>* sect, std::map<uint32_t,uint64_t>& map);
	
	Reader<A>&				fReader;
	const uint8_t*			fMappingStart;
	const macho_section<P>* fSectionsStart;
	const macho_section<P>* fSectionsEnd;
	std::map<uint32_t,uint64_t> fEHFrameOffsetToTargetMap;
};


template <typename A>
ObjectFileAddressSpace<A>::ObjectFileAddressSpace(Reader<A>& reader)
	: fReader(reader), fMappingStart(NULL), fSectionsStart(NULL), fSectionsEnd(NULL)
{
}



template <typename A>
const void* ObjectFileAddressSpace<A>::mappedAddress(pint_t addr, pint_t* relocTarget)
{
	if ( fMappingStart == NULL ) {
		// delay initialization until now when fReader.fSegment is set up
		fMappingStart = (uint8_t*)fReader.fHeader;
		fSectionsStart = (macho_section<P>*)((char*)fReader.fSegment + sizeof(macho_segment_command<P>));
		fSectionsEnd = &fSectionsStart[fReader.fSegment->nsects()];
		// find __eh_frame section and build map of relocations for performance
		buildRelocatedMap(fReader.fehFrameSection, fEHFrameOffsetToTargetMap);
	}
	// special case lookups in __eh_frame section to be fast
	const macho_section<P>* ehSect = fReader.fehFrameSection;
	if ( (ehSect->addr() <= addr) && (addr < (ehSect->addr()+ehSect->size())) ) {
		pint_t offsetOfAddrInSection = addr - ehSect->addr();
		if ( relocTarget != NULL ) {
			std::map<uint32_t,uint64_t>::iterator pos = fEHFrameOffsetToTargetMap.find(offsetOfAddrInSection);
			if ( pos != fEHFrameOffsetToTargetMap.end() )
				*relocTarget = pos->second;
			else
				*relocTarget = 0;
		}
		return fMappingStart + ehSect->offset() + offsetOfAddrInSection;
	}
	else {
		for (const macho_section<P>* sect=fSectionsStart; sect < fSectionsEnd; ++sect) {
			if ( (sect->addr() <= addr) && (addr < (sect->addr()+sect->size())) ) {
				pint_t offsetOfAddrInSection = addr - sect->addr();
				if ( (sect->flags() & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS ) {
					const uint32_t indirectTableOffset = sect->reserved1();
					const uint32_t sectionIndex = offsetOfAddrInSection/sizeof(pint_t);
					const uint32_t symbolIndex = A::P::E::get32(fReader.fIndirectTable[indirectTableOffset+sectionIndex]);
					// return pointer to symbol name which this non-lazy-pointer will point to
					if ( relocTarget != NULL )
						*relocTarget = (uintptr_t)&fReader.fStrings[fReader.fSymbols[symbolIndex].n_strx()];
				}
				else {
					if ( relocTarget != NULL )
						*relocTarget = relocated(offsetOfAddrInSection, sect->reloff(), sect->nreloc());
				}
				return fMappingStart + sect->offset() + offsetOfAddrInSection;
			}
		}
		throwf("ObjectFileAddressSpace::mappedAddress(0x%0lX) not in any section", (long)addr);
	}
}




template <typename A>
uint8_t ObjectFileAddressSpace<A>::get8(pint_t logicalAddr)
{
	return *((uint8_t*)mappedAddress(logicalAddr));
}

template <typename A>
uint16_t ObjectFileAddressSpace<A>::get16(pint_t logicalAddr)
{
	return P::E::get16(*((uint16_t*)mappedAddress(logicalAddr)));
}

template <typename A>
uint32_t ObjectFileAddressSpace<A>::get32(pint_t logicalAddr)
{
	pint_t relocTarget;
	return P::E::get32(*((uint32_t*)mappedAddress(logicalAddr, &relocTarget))) + relocTarget;
}

template <typename A>
uint64_t ObjectFileAddressSpace<A>::get64(pint_t logicalAddr)
{
	pint_t relocTarget;
	return P::E::get64(*((uint64_t*)mappedAddress(logicalAddr, &relocTarget))) + relocTarget;
}

template <typename A>
typename A::P::uint_t ObjectFileAddressSpace<A>::getP(pint_t logicalAddr)
{
	pint_t relocTarget;
	return P::getP(*((pint_t*)mappedAddress(logicalAddr, &relocTarget))) + relocTarget;
}

template <typename A>
uint64_t ObjectFileAddressSpace<A>::getULEB128(pint_t& logicalAddr, pint_t end)
{
	uintptr_t size = (end - logicalAddr);
	libunwind::LocalAddressSpace::pint_t laddr = (libunwind::LocalAddressSpace::pint_t)mappedAddress(logicalAddr);
	libunwind::LocalAddressSpace::pint_t sladdr = laddr;
	uint64_t result = libunwind::LocalAddressSpace::getULEB128(laddr, laddr+size);
	logicalAddr += (laddr-sladdr);
	return result;
}

template <typename A>
int64_t ObjectFileAddressSpace<A>::getSLEB128(pint_t& logicalAddr, pint_t end)
{
	uintptr_t size = (end - logicalAddr);
	libunwind::LocalAddressSpace::pint_t laddr = (libunwind::LocalAddressSpace::pint_t)mappedAddress(logicalAddr);
	libunwind::LocalAddressSpace::pint_t sladdr = laddr;
	int64_t result = libunwind::LocalAddressSpace::getSLEB128(laddr, laddr+size);
	logicalAddr += (laddr-sladdr);
	return result;
}






template <typename A>
class Reader : public ObjectFile::Reader
{
public:
	static bool										validFile(const uint8_t* fileContent, bool subtypeMustMatch=false, cpu_subtype_t subtype=0);
													Reader(const uint8_t* fileContent, const char* path, time_t modTime, 
														const ObjectFile::ReaderOptions& options, uint32_t ordinalBase);
	virtual											~Reader() {}

	virtual const char*								getPath()				{ return fPath; }
	virtual time_t									getModificationTime()	{ return fModTime; }
	virtual ObjectFile::Reader::DebugInfoKind		getDebugInfoKind()		{ return fDebugInfo; }
	virtual std::vector<class ObjectFile::Atom*>&	getAtoms()				{ return (std::vector<class ObjectFile::Atom*>&)(fAtoms); }
	virtual std::vector<class ObjectFile::Atom*>*	getJustInTimeAtomsFor(const char* name) { return NULL; }
	virtual std::vector<Stab>*						getStabs()				{ return &fStabs; }
	virtual ObjectFile::Reader::ObjcConstraint		getObjCConstraint()		{ return fObjConstraint; }
    virtual uint32_t                                updateCpuConstraint(uint32_t current);
	virtual bool									canScatterAtoms()		{ return (fHeader->flags() & MH_SUBSECTIONS_VIA_SYMBOLS); }
	virtual bool									objcReplacementClasses(){ return fReplacementClasses; }
	virtual bool									hasLongBranchStubs()	{ return fHasLongBranchStubs; }

	 bool											getTranslationUnitSource(const char** dir, const char** name) const;

private:
	typedef typename A::P						P;
	typedef typename A::P::E					E;
	typedef typename A::P::uint_t				pint_t;
	//typedef typename std::vector<Atom<A>*>		AtomVector;
	//typedef typename AtomVector::iterator		AtomVectorIterator;	// seems to help C++ parser
	typedef typename A::ReferenceKinds			Kinds;
	typedef typename libunwind::CFI_Parser<ObjectFileAddressSpace<A> >::FDE_Atom_Info  FDE_Atom_Info;
	typedef typename libunwind::CFI_Parser<ObjectFileAddressSpace<A> >::CIE_Atom_Info  CIE_Atom_Info;
	typedef class ObjectFileAddressSpace<A>	OAS;
	friend class ObjectFileAddressSpace<A>;
	friend class AnonymousAtom<A>;
	friend class TentativeAtom<A>;
	friend class AbsoluteAtom<A>;
	friend class SymbolAtom<A>;
	typedef std::map<pint_t, BaseAtom*>			AddrToAtomMap;

	void										addReferencesForSection(const macho_section<P>* sect);
	bool										addRelocReference(const macho_section<P>* sect, const macho_relocation_info<P>* reloc);
	bool										addRelocReference_powerpc(const macho_section<P>* sect, const macho_relocation_info<P>* reloc);
	const char*									getDwarfString(uint64_t form, const uint8_t* p);
	bool										read_comp_unit(const char ** name, const char ** comp_dir, uint64_t *stmt_list);
	static bool									isWeakImportSymbol(const macho_nlist<P>* sym);
	static bool									skip_form(const uint8_t ** offset, const uint8_t * end, uint64_t form, uint8_t addr_size, bool dwarf64);
	static const char*							assureFullPath(const char* path);
	AtomAndOffset								findAtomAndOffset(pint_t addr);
	AtomAndOffset								findAtomAndOffset(pint_t baseAddr, pint_t realAddr);
	Reference<A>*								makeReference(Kinds kind, pint_t atAddr, pint_t toAddr);
	Reference<A>*								makeReference(Kinds kind, pint_t atAddr, pint_t fromAddr, pint_t toAddr);
	Reference<A>*								makeReferenceWithToBase(Kinds kind, pint_t atAddr, pint_t toAddr, pint_t toBaseAddr);
	Reference<A>*								makeReferenceWithToBase(Kinds kind, pint_t atAddr, pint_t fromAddr, pint_t toAddr, pint_t toBaseAddr);
	Reference<A>*								makeByNameReference(Kinds kind, pint_t atAddr, const char* toName, uint32_t toOffset);
	BaseAtom*									makeReferenceToEH(const char* ehName, pint_t ehAtomAddress, const macho_section<P>* ehSect);
	Reference<A>*								makeReferenceToSymbol(Kinds kind, pint_t atAddr, const macho_nlist<P>* toSymbol, pint_t toOffset);
	void										validSectionType(uint8_t type);
	void										addDtraceExtraInfos(uint32_t probeAddr, const char* providerName);
	void										setCpuConstraint(uint32_t cpusubtype);
	const macho_section<P>*						getSectionForAddress(pint_t);
	ObjectFile::Atom*							getFunctionAtomFromFDEAddress(pint_t);
	ObjectFile::Atom*							getFunctionAtomFromLSDAAddress(pint_t);
	void										addFdeReference(uint8_t encoding, AtomAndOffset inFDE, AtomAndOffset target);
	void										addCiePersonalityReference(BaseAtom* cieAtom, uint32_t offsetInCIE, uint8_t encoding);
	bool										isSectDiffReloc(uint8_t r_type);


	BaseAtom*									findAtomByName(const char*);

	const char*									fPath;
	time_t										fModTime;
	uint32_t									fOrdinalBase;
	const ObjectFile::ReaderOptions&			fOptions;
	const macho_header<P>*						fHeader;
	const char*									fStrings;
	const macho_nlist<P>*						fSymbols;
	uint32_t									fSymbolCount;
	const macho_segment_command<P>*				fSegment;
	const uint32_t*								fIndirectTable;
	std::vector<BaseAtom*>						fAtoms;
	AddrToAtomMap								fAddrToAtom;
	AddrToAtomMap								fAddrToAbsoluteAtom;
	std::vector<class AnonymousAtom<A>*>		fLocalNonLazys;
	std::vector<class AnonymousAtom<A>*>		fAtomsPendingAName;
	std::set<const macho_section<P>*>			fSectionsWithAtomsPendingAName;
	std::vector<const char*>					fDtraceProviderInfo;
	ObjectFile::Reader::DebugInfoKind			fDebugInfo;
	bool										fHasUUID;
	const macho_section<P>*						fehFrameSection;
	std::set<BaseAtom*>							fLSDAAtoms;
	const macho_section<P>*						fDwarfDebugInfoSect;
	const macho_section<P>*						fDwarfDebugAbbrevSect;
	const macho_section<P>*						fDwarfDebugLineSect;
	const macho_section<P>*						fDwarfDebugStringSect;
	const char*									fDwarfTranslationUnitDir;
	const char*									fDwarfTranslationUnitFile;
	std::map<uint32_t,const char*>				fDwarfIndexToFile;
	std::vector<Stab>							fStabs;
	std::vector<FDE_Atom_Info>					fFDEInfos;
	std::vector<CIE_Atom_Info>					fCIEInfos;
	bool										fAppleObjc;
	bool										fHasDTraceProbes;
	bool										fHaveIndirectSymbols;
	bool										fReplacementClasses;
	bool										fHasLongBranchStubs;
	ObjectFile::Reader::ObjcConstraint			fObjConstraint;
	uint32_t                                    fCpuConstraint;
	const macho_section<P>*						fSectionsStart;
	const macho_section<P>*						fSectionsEnd;
	OAS											fObjectAddressSpace;
};

template <typename A>
Reader<A>::Reader(const uint8_t* fileContent, const char* path, time_t modTime, const ObjectFile::ReaderOptions& options, uint32_t ordinalBase)
	: fPath(strdup(path)), fModTime(modTime), fOrdinalBase(ordinalBase), fOptions(options), fHeader((const macho_header<P>*)fileContent),
	 fStrings(NULL), fSymbols(NULL), fSymbolCount(0), fSegment(NULL), fIndirectTable(NULL),
	 fDebugInfo(kDebugInfoNone), fHasUUID(false), fehFrameSection(NULL), 
	 fDwarfDebugInfoSect(NULL), fDwarfDebugAbbrevSect(NULL), fDwarfDebugLineSect(NULL),
	  fDwarfTranslationUnitDir(NULL), fDwarfTranslationUnitFile(NULL), fAppleObjc(false), fHasDTraceProbes(false),
	  fHaveIndirectSymbols(false), fReplacementClasses(false), fHasLongBranchStubs(false),
	  fObjConstraint(ObjectFile::Reader::kObjcNone), fCpuConstraint(ObjectFile::Reader::kCpuAny),
	  fSectionsStart(NULL), fSectionsEnd(NULL), fObjectAddressSpace(*this)
{
	// sanity check
	if ( ! validFile(fileContent, false, 0) )
		throw "not a valid mach-o object file";

	Reference<A>::fgForFinalLinkedImage = options.fForFinalLinkedImage;

	// write out path for -t or -whatsloaded option
	if ( options.fLogObjectFiles || options.fLogAllFiles )
		printf("%s\n", path);

	// cache intersting pointers
	const macho_header<P>* header = (const macho_header<P>*)fileContent;
	this->setCpuConstraint(header->cpusubtype());
	const uint32_t cmd_count = header->ncmds();
	const macho_load_command<P>* const cmds = (macho_load_command<P>*)((char*)header + sizeof(macho_header<P>));
	const macho_load_command<P>* const cmdsEnd = (macho_load_command<P>*)((char*)header + sizeof(macho_header<P>) + header->sizeofcmds());
	const macho_load_command<P>* cmd = cmds;
	uint32_t undefinedStartIndex = 0;
	uint32_t undefinedEndIndex = 0;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		switch (cmd->cmd()) {
		    case LC_SYMTAB:
				{
					const macho_symtab_command<P>* symtab = (macho_symtab_command<P>*)cmd;
					fSymbolCount = symtab->nsyms();
					fSymbols = (const macho_nlist<P>*)((char*)header + symtab->symoff());
					fStrings = (char*)header + symtab->stroff();
					if ( undefinedEndIndex == 0 ) {
						undefinedStartIndex = 0;
						undefinedEndIndex = symtab->nsyms();
					}
				}
				break;
			case LC_DYSYMTAB:
				{
					const macho_dysymtab_command<P>* dsymtab = (struct macho_dysymtab_command<P>*)cmd;
					fIndirectTable = (uint32_t*)((char*)fHeader + dsymtab->indirectsymoff());
					undefinedStartIndex = dsymtab->iundefsym();
					undefinedEndIndex = undefinedStartIndex + dsymtab->nundefsym();
				}
				break;
		    case LC_UUID:
				fHasUUID = true;
				break;

			default:
				if ( cmd->cmd() == macho_segment_command<P>::CMD ) {
					fSegment = (macho_segment_command<P>*)cmd;
				}
				break;
		}
		cmd = (const macho_load_command<P>*)(((char*)cmd)+cmd->cmdsize());
		if ( cmd > cmdsEnd )
			throwf("malformed dylb, load command #%d is outside size of load commands in %s", i, path);
	}

	// if there are no load commands, then this file has no content, so no atoms
	if ( header->ncmds() < 1 )
		return;

	fSectionsStart = (macho_section<P>*)((char*)fSegment + sizeof(macho_segment_command<P>));
	fSectionsEnd = &fSectionsStart[fSegment->nsects()];

	// inital guess for number of atoms
	fAtoms.reserve(fSymbolCount);

	// if there is an __eh_frame section, decode it into chunks to get atoms in that
	// section as well as division points for functions in __text
	for (const macho_section<P>* sect=fSectionsStart; sect < fSectionsEnd; ++sect) {
		if ( (strcmp(sect->sectname(), "__eh_frame") == 0) && (strcmp(sect->segname(), "__TEXT") == 0) ) {
			fehFrameSection = sect;
			const char* msg = libunwind::CFI_Parser<ObjectFileAddressSpace<A> >::getCFIs(fObjectAddressSpace, sect->addr(), 
																						sect->size(), fFDEInfos, fCIEInfos);
			if ( msg != NULL ) {
				throwf("malformed __eh_frame section: %s", msg);
			}
			else {
				//fprintf(stderr, "%lu CIEs, %lu FDEs\n", fCIEInfos.size(), fFDEInfos.size());
				// add anonymous atoms for each CIE
				for (typename std::vector<CIE_Atom_Info>::const_iterator it = fCIEInfos.begin(); it != fCIEInfos.end(); ++it) {
					AnonymousAtom<A>* cieAtom = new AnonymousAtom<A>(*this, sect, it->cieAddress, 1);
					fAtoms.push_back(cieAtom);
					fAddrToAtom[it->cieAddress] = cieAtom;
				}
				// add anonymous atoms for each FDE and LSDA
				for (typename std::vector<FDE_Atom_Info>::const_iterator it = fFDEInfos.begin(); it != fFDEInfos.end(); ++it) {
					//fprintf(stderr, "fdeAddress=0x%08llX, lsdaAddr=0x%08llX, funcAddr=0x%08llX\n", (uint64_t)it->fdeAddress, (uint64_t)it->lsda.address,  (uint64_t)it->function.address);
					AnonymousAtom<A>* fdeAtom = new AnonymousAtom<A>(*this, sect, it->fdeAddress, 0);
					fAtoms.push_back(fdeAtom);
					fAddrToAtom[it->fdeAddress] = fdeAtom;
					if ( it->lsda.address != 0 ) {
						AnonymousAtom<A>* lsdaAtom = new AnonymousAtom<A>(*this, getSectionForAddress(it->lsda.address), it->lsda.address, 0); 
						fAtoms.push_back(lsdaAtom);
						fAddrToAtom[it->lsda.address] = lsdaAtom;
						fLSDAAtoms.insert(lsdaAtom);
					}
				}
			}
		}
	}


	// add all atoms that have entries in symbol table
	BaseAtom* sectionEndAtoms[fSegment->nsects()];
	for (unsigned int i=0; i < fSegment->nsects(); ++i)
		sectionEndAtoms[i] = NULL;
	for (int i=fSymbolCount-1; i >= 0 ; --i) {
		// walk backwards through symbol table so globals are see before locals, otherwise a local alias would beome the real name
		const macho_nlist<P>& sym = fSymbols[i];
		if ( (sym.n_type() & N_STAB) == 0 ) {
			uint8_t type =  (sym.n_type() & N_TYPE);
			if ( type == N_SECT ) {
				const macho_section<P>* section	= &fSectionsStart[sym.n_sect()-1];
				const pint_t sectionStartAddr = section->addr();
				const pint_t sectionEndAddr = sectionStartAddr + section->size();
				bool suppress = false;
				// ignore atoms in debugger sections 
				if ( (section->flags() & S_ATTR_DEBUG) == 0 ) {
					if ( strncmp(&fStrings[sym.n_strx()], "__dtrace_probe$", 15) == 0 ) {
						// ignore dtrace probe labels 
						fHasDTraceProbes = true;
					}
					else if ( fStrings[sym.n_strx()] == 'L' ) {
						// ignore L labels, <rdar://problem/3962731>
					}
					else if ( section == fehFrameSection ) {
						// ignore labels in __eh_frame section
					}
					else {
						// ignore labels for atoms in other sections
						switch ( section->flags() & SECTION_TYPE ) {
							case S_REGULAR:
								if ( (sym.n_desc() & N_WEAK_DEF) && strcmp(section->sectname(), "__picsymbolstub1__TEXT") == 0 )
									suppress = true; // ignore stubs in crt1.o built by old ld64 that was missing S_SYMBOL_STUBS
							case S_ZEROFILL:
							case S_COALESCED:
							case S_4BYTE_LITERALS:
							case S_8BYTE_LITERALS:
							case S_16BYTE_LITERALS:
							case S_CSTRING_LITERALS:
								{
									BaseAtom* newAtom;
									typename AddrToAtomMap::iterator pos = fAddrToAtom.find(sym.n_value());
									if ( (pos != fAddrToAtom.end()) && (pos->second->getSectionRecord() == section) ) {
										if ( fLSDAAtoms.count(pos->second) != 0 ) {
											// already have LSDA atom from for this address, ignore compiler's label
											suppress = true;
											break;
										}
										else {
											// another label to an existing address in the same section, make this an alias
											newAtom = new SymbolAliasAtom<A>(&fStrings[sym.n_strx()], &sym, *pos->second);
										}
									}
									else {
										if ( sym.n_value() == sectionEndAddr ) {
											// Symbol address is at end of section.  This can interfere
											// with a symbol at the start of the next section, so don't
											// add to fAddrToAtom. But do track in sectionEndAtoms so we
											// still make aliases if there are duplicates.
											if ( sectionEndAtoms[sym.n_sect()-1] == NULL ) {
												newAtom = new SymbolAtom<A>(*this, &sym, section);
												sectionEndAtoms[sym.n_sect()-1] = newAtom;
												// if this is a zero length section, so add to fAddrToAtom
												if ( sym.n_value() == sectionStartAddr )
													fAddrToAtom[newAtom->getObjectAddress()] = newAtom;
											}
											else {
												newAtom = new SymbolAliasAtom<A>(&fStrings[sym.n_strx()], &sym, *sectionEndAtoms[sym.n_sect()-1]);
											}
										}
										else {
											// make SymbolAtom atom for this address
											newAtom = new SymbolAtom<A>(*this, &sym, section);
											fAddrToAtom[newAtom->getObjectAddress()] = newAtom;
										}
									}
									if ( ! suppress )
										fAtoms.push_back(newAtom);
									}
								break;
							case S_SYMBOL_STUBS:
							case S_LAZY_SYMBOL_POINTERS:
							case S_NON_LAZY_SYMBOL_POINTERS:
								// ignore symboled stubs produces by old ld64
								break;
							default:
								warning("symbol %s found in unsupported section in %s",
									&fStrings[sym.n_strx()], this->getPath());
						}
					}
				}
			}
			else if ( (type == N_UNDF) && (sym.n_value() != 0) ) {
				fAtoms.push_back(new TentativeAtom<A>(*this, &sym));
			}
			else if ( type == N_ABS ) {
				const char* symName = &fStrings[sym.n_strx()];
				if ( strncmp(symName, ".objc_class_name_", 17) == 0 ) {
					// ignore .objc_class_name_* symbols 
					fAppleObjc = true;
				}
				else if ( strcmp(&symName[strlen(symName)-3], ".eh") == 0 ) {
					// ignore empty *.eh symbols
				}
				else {
					BaseAtom* abAtom = new AbsoluteAtom<A>(*this, &sym);
					fAtoms.push_back(abAtom);
					fAddrToAbsoluteAtom[sym.n_value()] = abAtom;
				}
			}
			else if ( type == N_INDR ) {
				fHaveIndirectSymbols = true;
			}
		}
	}

	// add anonymous atoms for any functions (as determined by dwarf unwind) have no symbol names
	if ( fehFrameSection != NULL ) {
		for (typename std::vector<FDE_Atom_Info>::const_iterator it = fFDEInfos.begin(); it != fFDEInfos.end(); ++it) {
			// add if not already an atom at that address
			if ( fAddrToAtom.find(it->function.address) == fAddrToAtom.end() ) {
				AnonymousAtom<A>* funcAtom = new AnonymousAtom<A>(*this, getSectionForAddress(it->function.address), it->function.address, 0);
				fAtoms.push_back(funcAtom);
				fAddrToAtom[it->function.address] = funcAtom;
				// even though we've made a new atom, be conservative and make sure they lay out together
				if ( canScatterAtoms() ) {
					AtomAndOffset prev = findAtomAndOffset(it->function.address-1);
					if ( prev.atom != NULL ) {
						if ( ((BaseAtom*)(prev.atom))->getSectionRecord() == funcAtom->getSectionRecord() )
							new Reference<A>(A::kFollowOn, prev, AtomAndOffset(funcAtom));
					}
				}
			}
		}
	}


	// add all fixed size anonymous atoms from special sections
	for (const macho_section<P>* sect=fSectionsStart; sect < fSectionsEnd; ++sect) {
		pint_t atomSize = 0;
		uint8_t type (sect->flags() & SECTION_TYPE);
		validSectionType(type);
		bool suppress = false;
		switch ( type ) {
			case S_SYMBOL_STUBS:
				suppress = true;
				atomSize = sect->reserved2();
				break;
			case S_LAZY_SYMBOL_POINTERS:
				suppress = true;
				atomSize = sizeof(pint_t);
				break;
			case S_NON_LAZY_SYMBOL_POINTERS:
			case S_LITERAL_POINTERS:
			case S_MOD_INIT_FUNC_POINTERS:
			case S_MOD_TERM_FUNC_POINTERS:
				atomSize = sizeof(pint_t);
				break;
			case S_INTERPOSING:
				atomSize = sizeof(pint_t)*2;
				break;
			case S_4BYTE_LITERALS:
				atomSize = 4;
				break;
			case S_8BYTE_LITERALS:
				atomSize = 8;
				break;
			case S_16BYTE_LITERALS:
				atomSize = 16;
				break;
			case S_REGULAR:
				// special case ObjC classes to synthesize .objc_class_name_* symbols
				if ( (strcmp(sect->sectname(), "__class") == 0) && (strcmp(sect->segname(), "__OBJC") == 0) && fAppleObjc ) {
					// gcc sometimes over aligns class structure
					uint32_t align = 1 << sect->align();
					atomSize = ((12 * sizeof(pint_t)) + align-1) & (-align);
				}
				// get objc Garbage Collection info
				else if ( ((strcmp(sect->sectname(), "__image_info") == 0) && (strcmp(sect->segname(), "__OBJC") == 0))
					   || ((strncmp(sect->sectname(), "__objc_imageinfo", 16) == 0) && (strcmp(sect->segname(), "__DATA") == 0)) ) {
					//	struct objc_image_info  {
					//		uint32_t	version;	// initially 0
					//		uint32_t	flags;
					//	};
					// #define OBJC_IMAGE_SUPPORTS_GC   2
					// #define OBJC_IMAGE_GC_ONLY       4
					//
					const uint32_t* contents = (uint32_t*)(((char*)fHeader) + sect->offset());
					if ( (sect->size() >= 8) && (contents[0] == 0) ) {
						uint32_t flags = E::get32(contents[1]);
						if ( (flags & 4) == 4 )
							fObjConstraint = ObjectFile::Reader::kObjcGC;
						else if ( (flags & 2) == 2 )
							fObjConstraint = ObjectFile::Reader::kObjcRetainReleaseOrGC;
						else
							fObjConstraint = ObjectFile::Reader::kObjcRetainRelease;
						if ( (flags & 1) == 1 )
							fReplacementClasses = true;
						// don't make atom for this section
						atomSize = sect->size();
						suppress = true;
					}
					else {
						warning("can't parse __OBJC/__image_info section in %s", fPath);
					}
				}
				// special case constant NS/CFString literals and make an atom out of each one
				else if ((strcmp(sect->sectname(), "__cfstring") == 0) && (strcmp(sect->segname(), "__DATA") == 0)) {
					atomSize = 4 * sizeof(pint_t);
				}
				break;
		}
		if ( atomSize != 0 ) {
			for(pint_t sectOffset=0; sectOffset < sect->size(); sectOffset += atomSize) {
				pint_t atomAddr = sect->addr() + sectOffset;
				// add if not already an atom at that address
				if ( fAddrToAtom.find(atomAddr) == fAddrToAtom.end() ) {
					AnonymousAtom<A>* newAtom = new AnonymousAtom<A>(*this, sect, atomAddr, atomSize);
					if ( !suppress )
						fAtoms.push_back(newAtom);
					fAddrToAtom[atomAddr] = newAtom->redirectTo();
				}
			}
		}
	}

	// add all c-string anonymous atoms
	for (const macho_section<P>* sect=fSectionsStart; sect < fSectionsEnd; ++sect) {
		if ( ((sect->flags() & SECTION_TYPE) == S_CSTRING_LITERALS) || strcmp(sect->sectname(), "__cstring") == 0 ) {
			uint32_t stringLen;
			pint_t stringAddr;
			BaseAtom* mostAlignedEmptyString = NULL;
			uint32_t mostAlignedEmptyStringTrailingZeros = 0;
			std::vector<std::pair<pint_t,BaseAtom*> > emptyStrings;
			for(pint_t sectOffset=0; sectOffset < sect->size(); sectOffset += stringLen) {
				stringAddr = sect->addr() + sectOffset;
				stringLen  = strlen((char*)(fHeader) + sect->offset() + sectOffset) + 1;
				// add if not already an atom at that address
				if ( fAddrToAtom.find(stringAddr) == fAddrToAtom.end() ) {
					BaseAtom* newAtom = new AnonymousAtom<A>(*this, sect, stringAddr, stringLen);
					if ( stringLen == 1 ) {
						// because of padding it may look like there are lots of empty strings, keep track of all
						emptyStrings.push_back(std::make_pair<pint_t,BaseAtom*>(stringAddr, newAtom));
						// record empty string with greatest alignment requirement
						uint32_t stringAddrTrailingZeros = (stringAddr==0) ? sect->align() : __builtin_ctz(stringAddr);
						if ( (mostAlignedEmptyString == NULL) 
							|| ( stringAddrTrailingZeros > mostAlignedEmptyStringTrailingZeros) ) {
							mostAlignedEmptyString = newAtom;
							mostAlignedEmptyStringTrailingZeros = stringAddrTrailingZeros;
						}
					}
					else {
						fAtoms.push_back(newAtom);
						fAddrToAtom[stringAddr] = newAtom;
					}
				}
			}
			// map all uses of empty strings to the most aligned one
			if ( mostAlignedEmptyString != NULL ) {
				// make most aligned atom a real atom
				fAtoms.push_back(mostAlignedEmptyString);
				// map all other empty atoms to this one
				for (typename std::vector<std::pair<pint_t,BaseAtom*> >::iterator it=emptyStrings.begin(); it != emptyStrings.end(); it++) {
					fAddrToAtom[it->first] = mostAlignedEmptyString;
				}
			}
		}
	}

	// sort all atoms so far by address and section
	std::sort(fAtoms.begin(), fAtoms.end(), BaseAtomSorter());

	//fprintf(stderr, "sorted atoms:\n");
	//for (std::vector<BaseAtom*>::iterator it=fAtoms.begin(); it != fAtoms.end(); it++) 
	//	fprintf(stderr, "0x%08llX %s\n", (*it)->getObjectAddress(), (*it)->getDisplayName());

	// create atoms to cover any non-debug ranges not handled above
	for (const macho_section<P>* sect=fSectionsStart; sect < fSectionsEnd; ++sect) {
		pint_t sectionStartAddr = sect->addr();
		pint_t sectionEndAddr   = sect->addr() + sect->size();
		// don't set follow-on atoms in __eh_frame section
		const bool setFollowOnAtom = !canScatterAtoms() && (sect != fehFrameSection);
		if ( sect->size() != 0 ) {
			// ignore dwarf sections.  If ld every supports processing dwarf, this logic will need to change
			if ( (sect->flags() & S_ATTR_DEBUG) != 0 ) {
				fDebugInfo = kDebugInfoDwarf;
				if ( strcmp(sect->sectname(), "__debug_info") == 0 )
					fDwarfDebugInfoSect = sect;
				else if ( strcmp(sect->sectname(), "__debug_abbrev") == 0 )
					fDwarfDebugAbbrevSect = sect;
				else if ( strcmp(sect->sectname(), "__debug_line") == 0 )
					fDwarfDebugLineSect = sect;
				else if ( strcmp(sect->sectname(), "__debug_str") == 0 )
					fDwarfDebugStringSect = sect;
			}
			else {
				if ( strcmp(sect->segname(), "__DWARFA") == 0 ) {
					throw "object file contains old DWARF debug info - rebuild with newer compiler";
				}
				uint8_t type (sect->flags() & SECTION_TYPE);
				switch ( type ) {
					case S_REGULAR:
					case S_ZEROFILL:
					case S_COALESCED:
						// if there is not an atom already at the start of this section, add an anonymous one
						pint_t previousAtomAddr = 0;
						BaseAtom* previousAtom = NULL;
						if ( fAddrToAtom.find(sectionStartAddr) == fAddrToAtom.end() ) {
							BaseAtom* newAtom = new AnonymousAtom<A>(*this, sect, sect->addr(), 0);
							fAddrToAtom[sect->addr()] = newAtom;
							fAtoms.push_back(newAtom);
							previousAtomAddr = sectionStartAddr;
							previousAtom = newAtom;
							std::sort(fAtoms.begin(), fAtoms.end(), BaseAtomSorter());
						}
						// calculate size of all atoms in this section and add follow-on references
						for (std::vector<BaseAtom*>::iterator it=fAtoms.begin(); it != fAtoms.end(); it++) {
							BaseAtom* atom = (BaseAtom*)(*it);
							pint_t atomAddr = atom->getObjectAddress();
							if ( atom->getSectionRecord() == sect ) {
								//fprintf(stderr, "addr=0x%08llX, atom=%s\n", (uint64_t)atomAddr, atom->getDisplayName());
								if ( (previousAtom != NULL) && (previousAtomAddr != atomAddr) ) {
									previousAtom->setSize(atomAddr - previousAtomAddr);
									if ( setFollowOnAtom && (atom != previousAtom) )
										new Reference<A>(A::kFollowOn, AtomAndOffset(previousAtom), AtomAndOffset(atom));
								}
								previousAtomAddr = atomAddr;
								previousAtom = atom;
							} 
						}
						if ( previousAtom != NULL ) {
							// set last atom in section
							previousAtom->setSize(sectionEndAddr - previousAtomAddr);
						}
						break;
				}
			}
		}
	}

	// check for object file that defines no objc classes, but uses objc classes
	// check for dtrace provider info
	for (uint32_t i=undefinedStartIndex; i < undefinedEndIndex; ++i) {
		const macho_nlist<P>& sym = fSymbols[i];
		if ( (sym.n_type() & N_STAB) == 0 ) {
			if ( (sym.n_type() & N_TYPE) == N_UNDF ) {
				const char* undefinedName = &fStrings[sym.n_strx()];
				if ( !fAppleObjc && (strncmp(undefinedName, ".objc_class_name_", 17) == 0) ) {
					fAppleObjc = true;
				}
				else if ( strncmp(undefinedName, "___dtrace_", 10) == 0 ) {
					if ( strchr(undefinedName, '$') != NULL  ) {
						if ( (strncmp(&undefinedName[10], "probe$", 6) != 0) && (strncmp(&undefinedName[10], "isenabled$", 10) != 0) ) {
							// any undefined starting with __dtrace_*$ that is not ___dtrace_probe$* or ___dtrace_isenabled$*
							// is extra provider info
							fDtraceProviderInfo.push_back(undefinedName);
						}
					}
				}
			}
		}
	}
	
	// add relocation based references to sections that have atoms with pending names
	for (const macho_section<P>* sect=fSectionsStart; sect < fSectionsEnd; ++sect) {
		if ( fSectionsWithAtomsPendingAName.count(sect) != 0 )
			addReferencesForSection(sect);
	}
	
	// update any anonymous atoms that need references built in order to name themselves
	for (typename std::vector<AnonymousAtom<A>*>::iterator it=fAtomsPendingAName.begin(); it != fAtomsPendingAName.end(); it++) {
		(*it)->resolveName();
	}

	// add relocation based references to other sections
	for (const macho_section<P>* sect=fSectionsStart; sect < fSectionsEnd; ++sect) {
		if ( fSectionsWithAtomsPendingAName.count(sect) == 0 )
			addReferencesForSection(sect);
	}

	// add objective-c references
	if ( fAppleObjc ) {
		for (const macho_section<P>* sect=fSectionsStart; sect < fSectionsEnd; ++sect) {
			if ( (strcmp(sect->sectname(), "__cls_refs") == 0) && (strcmp(sect->segname(), "__OBJC") == 0) ) {
				for (uint32_t offset = 0; offset < sect->size(); offset += sizeof(pint_t)) {
					AtomAndOffset ao = this->findAtomAndOffset(sect->addr()+offset);
					ObjectFile::Reference* classRef = ao.atom->getReferences()[0];
					if ( classRef->getFixUpOffset() == 0 ) {
						const char* classStr = classRef->getTargetName();
						if ( strncmp(classStr, "cstring=", 8) == 0 ) {
							const char* className;
							asprintf((char**)&className, ".objc_class_name_%s", &classStr[8]);
							new Reference<A>(A::kNoFixUp, ao, className, 0);
						}
					}
				}
			}
		}
	}

	// add direct references to local non-lazy-pointers, can do this now that all atoms are constructed
	for (typename std::vector<AnonymousAtom<A>*>::iterator it=fLocalNonLazys.begin(); it != fLocalNonLazys.end(); it++) {
		AnonymousAtom<A>* localNonLazy = *it;
		uint32_t fileOffset = localNonLazy->fSection->offset() - localNonLazy->fSection->addr() + localNonLazy->fAddress;
		pint_t nonLazyPtrValue = P::getP(*((pint_t*)((char*)(fHeader)+fileOffset)));
		makeReference(A::kPointer, localNonLazy->fAddress, nonLazyPtrValue);
	}
	

	// add personality references to CIEs
	for (typename std::vector<CIE_Atom_Info>::const_iterator it = fCIEInfos.begin(); it != fCIEInfos.end(); ++it) {
		if ( it->personality.offsetInFDE != 0 )
			addCiePersonalityReference(fAddrToAtom[it->cieAddress], it->personality.offsetInFDE, it->personality.encodingOfAddress);
	}

	// add all references for FDEs, including implicit group references
	for (typename std::vector<FDE_Atom_Info>::const_iterator it = fFDEInfos.begin(); it != fFDEInfos.end(); ++it) {
		AtomAndOffset funcAO = this->findAtomAndOffset(it->function.address);
		if ( funcAO.offset != 0 )
			warning("FDE does not point to start of function %s\n", funcAO.atom->getDisplayName());
		AtomAndOffset fdeAO = this->findAtomAndOffset(it->fdeAddress);
		if ( fdeAO.offset != 0 )
			warning("FDE does start its own atom %s\n", funcAO.atom->getDisplayName());
		AtomAndOffset cieAO = this->findAtomAndOffset(it->cie.address);
		if ( cieAO.offset != 0 )
			warning("CIE does start its own atom %s\n", cieAO.atom->getDisplayName());
		AtomAndOffset lsdaAO;
		if ( it->lsda.address != 0 ) {
			lsdaAO = this->findAtomAndOffset(it->lsda.address);
			if ( lsdaAO.offset != 0 )
				warning("LSDA does start its own atom %s\n", lsdaAO.atom->getDisplayName());
		}
		
		// add reference from FDE to CIE	
		AtomAndOffset cieInfdeAO = AtomAndOffset(fdeAO.atom, it->cie.offsetInFDE);
		new Reference<A>(A::kPointerDiff32, cieInfdeAO, cieAO, cieInfdeAO);
		
		// add reference from FDE to function
		addFdeReference(it->function.encodingOfAddress, AtomAndOffset(fdeAO.atom, it->function.offsetInFDE), funcAO);
				
		// add reference from FDE to LSDA
		if ( it->lsda.address != 0 ) {
			addFdeReference(it->lsda.encodingOfAddress, AtomAndOffset(fdeAO.atom, it->lsda.offsetInFDE), lsdaAO);
		}
		
		// FDE is in group lead by function atom
		new Reference<A>(A::kGroupSubordinate, funcAO, fdeAO);
		
		// LSDA is in group lead by function atom
		if ( it->lsda.address != 0 ) {
			new Reference<A>(A::kGroupSubordinate, funcAO, lsdaAO);
			// add back reference from LSDA to owning function
			new Reference<A>(A::kNoFixUp, lsdaAO, funcAO);
		}

		// compute compact encoding for this FDE
		if ( fOptions.fAddCompactUnwindEncoding ) {
			((BaseAtom*)(funcAO.atom))->setCompactUnwindEncoding(it->fdeAddress);
			// add reference from function atom to personality function
			// the only reference a CIE can have is the reference to the personality function
			std::vector<class ObjectFile::Reference*>& cieRefs = cieAO.atom->getReferences();
			if ( cieRefs.size() == 1 ) {
				new Reference<A>((typename A::ReferenceKinds)((BaseAtom*)(funcAO.atom))->getPersonalityReferenceKind(), 
									funcAO, cieRefs[0]->getTargetName(), 0);
			}
		}
	}

	// add command line aliases
	for(std::vector<ObjectFile::ReaderOptions::AliasPair>::const_iterator it = fOptions.fAliases.begin(); it != fOptions.fAliases.end(); ++it) { 
		BaseAtom* target = this->findAtomByName(it->realName);
		if ( (target != NULL) && target->getSymbolTableInclusion() != ObjectFile::Atom::kSymbolTableNotIn )
			fAtoms.push_back(new SymbolAliasAtom<A>(it->alias, NULL, *target));
	}

	// add dtrace probe locations
	if ( fHasDTraceProbes ) {
		for (uint32_t i=0; i < fSymbolCount; ++i) {
			const macho_nlist<P>& sym = fSymbols[i];
			if ( (sym.n_type() & N_STAB) == 0 ) {
				if ( (sym.n_type() & N_TYPE) == N_SECT ) {
					const char* symbolName = &fStrings[sym.n_strx()];
					if ( strncmp(symbolName, "__dtrace_probe$", 15) == 0 ) {
						//fprintf(stderr, "adding dtrace probe at 0x%08llX %s\n", sym.n_value(), symbolName);
						makeByNameReference(A::kDtraceProbe, sym.n_value(), symbolName, 0);
					}
				}
			}
		}
	}
	
	// turn indirect symbols into SymbolAliasAtom 
	if ( fHaveIndirectSymbols ) {
		for (uint32_t i=0; i < fSymbolCount; ++i) {
			const macho_nlist<P>& sym = fSymbols[i];
			if ( (sym.n_type() & N_STAB) == 0 ) {
				if ( (sym.n_type() & N_TYPE) == N_INDR ) {
					const char* aliasName = &fStrings[sym.n_strx()];
					const char* targetName = &fStrings[sym.n_value()];
					//fprintf(stderr, "found alias %s for %s\n", aliasName, targetName);
					BaseAtom* target = this->findAtomByName(targetName);
					// only currently support N_INDR based aliases to something in the same .o file 
					if ( target != NULL ) {
						fAtoms.push_back(new SymbolAliasAtom<A>(aliasName, &sym, *target));
						//fprintf(stderr, "creating alias %s for %s\n", aliasName, targetName);
					}
				}
			}
		}
	}

	//for (typename AddrToAtomMap::iterator it=fAddrToAtom.begin(); it != fAddrToAtom.end(); it++) {
	//	fprintf(stderr, "[0x%0X -> 0x%0llX) : %s\n", it->first, it->first+it->second->getSize(), it->second->getDisplayName());
	//}

	// add translation unit info from dwarf
	uint64_t stmtList;
	if ( (fDebugInfo == kDebugInfoDwarf) && (fOptions.fDebugInfoStripping != ObjectFile::ReaderOptions::kDebugInfoNone) ) {
		// compiler sometimes emits emtpty dwarf sections when there is no debug info, skip those
		if ( (fDwarfDebugInfoSect != NULL) && (fDwarfDebugInfoSect->size() != 0) ) {
			if ( !read_comp_unit(&fDwarfTranslationUnitFile, &fDwarfTranslationUnitDir, &stmtList) ) {
				// if can't parse dwarf, warn and give up
				fDwarfTranslationUnitFile = NULL;
				fDwarfTranslationUnitDir = NULL;
				warning("can't parse dwarf compilation unit info in %s", this->getPath());
				fDebugInfo = kDebugInfoNone;
			}
		}
	}

	// add line number info to atoms from dwarf
	if ( (fDebugInfo == kDebugInfoDwarf) && (fOptions.fDebugInfoStripping != ObjectFile::ReaderOptions::kDebugInfoNone) ) {
		// file with just data will have no __debug_line info
		if ( (fDwarfDebugLineSect != NULL) && (fDwarfDebugLineSect->size() != 0) && (fAddrToAtom.size() != 0)
			&& (fDwarfDebugInfoSect != NULL) && (fDwarfDebugInfoSect->size() != 0) ) {
			// validate stmt_list
			if ( (stmtList != (uint64_t)-1) && (stmtList < fDwarfDebugLineSect->size()) ) {
				const uint8_t* debug_line = (uint8_t*)(fHeader) + fDwarfDebugLineSect->offset();
				if ( debug_line != NULL ) {
					struct line_reader_data* lines = line_open(&debug_line[stmtList],
															fDwarfDebugLineSect->size() - stmtList, E::little_endian);
					struct line_info result;
					ObjectFile::Atom* curAtom = NULL;
					uint32_t curAtomOffset = 0;
					uint32_t curAtomAddress = 0;
					uint32_t curAtomSize = 0;
					if ( lines != NULL ) {
						while ( line_next (lines, &result, line_stop_pc) ) {
							//fprintf(stderr, "curAtom=%p, result.pc=0x%llX, result.line=%llu, result.end_of_sequence=%d, curAtomAddress=0x%X, curAtomSize=0x%X\n",
							//		curAtom, result.pc, result.line, result.end_of_sequence, curAtomAddress, curAtomSize);
							// work around weird debug line table compiler generates if no functions in __text section
							if ( (curAtom == NULL) && (result.pc == 0) && result.end_of_sequence && (result.file == 1))
								continue;
							// for performance, see if in next pc is in current atom
							if ( (curAtom != NULL) && (curAtomAddress <= result.pc) && (result.pc < (curAtomAddress+curAtomSize)) ) {
								curAtomOffset = result.pc - curAtomAddress;
							}
							// or pc at end of current atom
							else if ( result.end_of_sequence && (curAtom != NULL) && (result.pc == (curAtomAddress+curAtomSize)) ) {
								curAtomOffset = result.pc - curAtomAddress;
							}
							else {
								// do slow look up of atom by address
								AtomAndOffset ao = this->findAtomAndOffset(result.pc);
								curAtom			= ao.atom;
								if ( curAtom == NULL )
									break; // file has line info but no functions
								if ( result.end_of_sequence && (curAtomAddress+curAtomSize < result.pc) ) {	
									// a one line function can be returned by line_next() as one entry with pc at end of blob
									// look for alt atom starting at end of previous atom
									uint32_t previousEnd = curAtomAddress+curAtomSize;
									AtomAndOffset alt = this->findAtomAndOffset(previousEnd);
									if ( result.pc <= previousEnd - alt.offset + alt.atom->getSize() ) {
										curAtom			= alt.atom;
										curAtomOffset	= alt.offset;
										curAtomAddress	= previousEnd - alt.offset;
										curAtomSize		= curAtom->getSize();
									}
									else {
										curAtomOffset	= ao.offset;
										curAtomAddress	= result.pc - ao.offset;
										curAtomSize		= curAtom->getSize();
									}
								}
								else {
									curAtomOffset	= ao.offset;
									curAtomAddress	= result.pc - ao.offset;
									curAtomSize		= curAtom->getSize();
								}
							}
							const char* filename;
							std::map<uint32_t,const char*>::iterator pos = fDwarfIndexToFile.find(result.file);
							if ( pos == fDwarfIndexToFile.end() ) {
								filename = line_file(lines, result.file);
								fDwarfIndexToFile[result.file] = filename;
							}
							else {
								filename = pos->second;
							}
							ObjectFile::LineInfo info;
							info.atomOffset = curAtomOffset;
							info.fileName = filename;
							info.lineNumber = result.line;
							//fprintf(stderr, "addr=0x%08llX, line=%lld, file=%s, atom=%s, atom.size=0x%X, end=%d\n", 
							//		result.pc, result.line, filename, curAtom->getDisplayName(), curAtomSize, result.end_of_sequence);
							((BaseAtom*)curAtom)->addLineInfo(info);
							if ( result.end_of_sequence ) {
								curAtom = NULL;
							}
						}
					line_free(lines);
					}
				}
				else {
					warning("could not parse dwarf line number info in %s", this->getPath());
				}
			}
		}
	}

	// if no dwarf, try processing stabs debugging info
	if ( (fDebugInfo == kDebugInfoNone) && (fOptions.fDebugInfoStripping != ObjectFile::ReaderOptions::kDebugInfoNone) ) {
		// scan symbol table for stabs entries
		fStabs.reserve(fSymbolCount);  // reduce re-allocations
		BaseAtom* currentAtom = NULL;
		pint_t currentAtomAddress = 0;
		enum { start, inBeginEnd, inFun } state = start;
		for (uint32_t symbolIndex = 0; symbolIndex < fSymbolCount; ++symbolIndex ) {
			const macho_nlist<P>* sym = &fSymbols[symbolIndex];
			bool useStab = true;
			uint8_t type = sym->n_type();
			const char* symString = (sym->n_strx() != 0) ? &fStrings[sym->n_strx()] : NULL;
			if ( (type & N_STAB) != 0 ) {
				fDebugInfo =  (fHasUUID ? kDebugInfoStabsUUID : kDebugInfoStabs);
				Stab stab;
				stab.atom	= NULL;
				stab.type	= type;
				stab.other	= sym->n_sect();
				stab.desc	= sym->n_desc();
				stab.value	= sym->n_value();
				stab.string = NULL;
				switch (state) {
					case start:
						switch (type) {
							case N_BNSYM:
								// beginning of function block
								state = inBeginEnd;
								// fall into case to lookup atom by addresss
							case N_LCSYM:
							case N_STSYM:
								currentAtomAddress = sym->n_value();
								currentAtom = (BaseAtom*)this->findAtomAndOffset(currentAtomAddress).atom;
								if ( currentAtom != NULL ) {
									stab.atom = currentAtom;
									stab.string = symString;
								}
								else {
									fprintf(stderr, "can't find atom for stabs BNSYM at %08llX in %s",
										(uint64_t)sym->n_value(), path);
								}
								break;
							case N_SO:
							case N_OSO:
							case N_OPT:
							case N_LSYM:
							case N_RSYM:
							case N_PSYM:
								// not associated with an atom, just copy
								stab.string = symString;
								break;
							case N_GSYM:
							{
								// n_value field is NOT atom address ;-(
								// need to find atom by name match
								const char* colon = strchr(symString, ':');
								if ( colon != NULL ) {
									// build underscore leading name
									int nameLen = colon - symString;
									char symName[nameLen+2];
									strlcpy(&symName[1], symString, nameLen+1);
									symName[0] = '_';
									symName[nameLen+1] = '\0';
									currentAtom = findAtomByName(symName);
									if ( currentAtom != NULL ) {
										stab.atom = currentAtom;
										stab.string = symString;
									}
								}
								else {
									// might be a debug-note without trailing :G()
									currentAtom = findAtomByName(symString);
									if ( currentAtom != NULL ) {
										stab.atom = currentAtom;
										stab.string = symString;
									}
								}
								if ( stab.atom == NULL ) {
									// ld_classic added bogus GSYM stabs for old style dtrace probes
									if ( (strncmp(symString, "__dtrace_probe$", 15) != 0) )
										warning("can't find atom for N_GSYM stabs %s in %s", symString, path);
									useStab = false;
								}
								break;
							}
							case N_FUN:
								// old style stabs without BNSYM
								state = inFun;
								currentAtomAddress = sym->n_value();
								currentAtom = (BaseAtom*)this->findAtomAndOffset(currentAtomAddress).atom;
								if ( currentAtom != NULL ) {
									stab.atom = currentAtom;
									stab.string = symString;
								}
								else {
									warning("can't find atom for stabs FUN at %08llX in %s",
										(uint64_t)currentAtomAddress, path);
								}
								break;
							case N_SOL:
							case N_SLINE:
								stab.string = symString;
								// old stabs
								break;
							case N_BINCL:
							case N_EINCL:
							case N_EXCL:
								stab.string = symString;
								// -gfull built .o file
								break;
							default:
								warning("unknown stabs type 0x%X in %s", type, path);
						}
						break;
					case inBeginEnd:
						stab.atom = currentAtom;
						switch (type) {
							case N_ENSYM:
								state = start;
								currentAtom = NULL;
								break;
							case N_LCSYM:
							case N_STSYM:
							{
								BaseAtom* nestedAtom = (BaseAtom*)this->findAtomAndOffset(sym->n_value()).atom;
								if ( nestedAtom != NULL ) {
									stab.atom = nestedAtom;
									stab.string = symString;
								}
								else {
									warning("can't find atom for stabs 0x%X at %08llX in %s",
										type, (uint64_t)sym->n_value(), path);
								}
								break;
							}
							case N_LBRAC:
							case N_RBRAC:
							case N_SLINE:
								// adjust value to be offset in atom
								stab.value -= currentAtomAddress;
							default:
								stab.string = symString;
								break;
						}
						break;
					case inFun:
						switch (type) {
							case N_FUN:
								if ( sym->n_sect() != 0 ) {
									// found another start stab, must be really old stabs...
									currentAtomAddress = sym->n_value();
									currentAtom = (BaseAtom*)this->findAtomAndOffset(currentAtomAddress).atom;
									if ( currentAtom != NULL ) {
										stab.atom = currentAtom;
										stab.string = symString;
									}
									else {
										warning("can't find atom for stabs FUN at %08llX in %s",
											(uint64_t)currentAtomAddress, path);
									}
								}
								else {
									// found ending stab, switch back to start state
									stab.string = symString;
									stab.atom = currentAtom;
									state = start;
									currentAtom = NULL;
								}
								break;
							case N_LBRAC:
							case N_RBRAC:
							case N_SLINE:
								// adjust value to be offset in atom
								stab.value -= currentAtomAddress;
								stab.atom = currentAtom;
								break;
							case N_SO:
								stab.string = symString;
								state = start;
								break;
							default:
								stab.atom = currentAtom;
								stab.string = symString;
								break;
						}
						break;
				}
				// add to list of stabs for this .o file
				if ( useStab )
					fStabs.push_back(stab);
			}
		}
	}

#if 0
	// special case precompiled header .o file (which has no content) to have one empty atom
	if ( fAtoms.size() == 0 ) {
		int pathLen = strlen(path);
		if ( (pathLen > 6) && (strcmp(&path[pathLen-6], ".gch.o")==0) ) {
			ObjectFile::Atom* phony = new AnonymousAtom<A>(*this, (uint32_t)0);
			//phony->fSynthesizedName = ".gch.o";
			fAtoms.push_back(phony);
		}
	}
#endif

	// sort all atoms by address
	std::sort(fAtoms.begin(), fAtoms.end(), BaseAtomSorter());

	// set ordinal and sort references in each atom
	uint32_t index = fOrdinalBase;
	for (std::vector<BaseAtom*>::iterator it=fAtoms.begin(); it != fAtoms.end(); it++) {
		BaseAtom* atom = (BaseAtom*)(*it);
		atom->setOrdinal(index++);
		atom->sortReferences();
	}
	
}

template <typename A>
const macho_section<typename A::P>* Reader<A>::getSectionForAddress(pint_t addr)
{
	for (const macho_section<P>* sect=fSectionsStart; sect < fSectionsEnd; ++sect) {
		if ( (sect->addr() <= addr) && (addr < (sect->addr()+sect->size())) ) 
			return sect;
	}
	throwf("section not found for address 0x%08llX", (uint64_t)addr);
}

template <typename A>
ObjectFile::Atom* Reader<A>::getFunctionAtomFromFDEAddress(pint_t addr)
{
	for (typename std::vector<FDE_Atom_Info>::const_iterator it = fFDEInfos.begin(); it != fFDEInfos.end(); ++it) {
		if ( it->fdeAddress == addr ) {
			return findAtomAndOffset(it->function.address).atom;
		}
	}
	// CIEs won't be in fFDEInfos
	return NULL;
}

template <typename A>
ObjectFile::Atom* Reader<A>::getFunctionAtomFromLSDAAddress(pint_t addr)
{
	for (typename std::vector<FDE_Atom_Info>::const_iterator it = fFDEInfos.begin(); it != fFDEInfos.end(); ++it) {
		if ( it->lsda.address == addr ) {
			return findAtomAndOffset(it->function.address).atom;
		}
	}
	return NULL;
}


template <>
void ObjectFileAddressSpace<x86_64>::buildRelocatedMap(const macho_section<P>* sect, std::map<uint32_t,uint64_t>& map)
{	
	// mach-o x86_64 is different, the content of a section with a relocation is the addend
	const macho_relocation_info<P>* relocs = (macho_relocation_info<P>*)((char*)(fReader.fHeader) + sect->reloff());
	const macho_relocation_info<P>* relocsEnd = &relocs[sect->nreloc()];
	for (const macho_relocation_info<P>* reloc = relocs; reloc < relocsEnd; ++reloc) {
		std::map<uint32_t,uint64_t>::iterator pos;
		switch ( reloc->r_type() ) {
			case X86_64_RELOC_UNSIGNED:
				pos = map.find(reloc->r_address());
				if ( pos != map.end() )
					pos->second += fReader.fSymbols[reloc->r_symbolnum()].n_value();
				else
					map[reloc->r_address()] = fReader.fSymbols[reloc->r_symbolnum()].n_value();
				break;
			case X86_64_RELOC_SUBTRACTOR:
				map[reloc->r_address()] = -fReader.fSymbols[reloc->r_symbolnum()].n_value();
				break;
			case X86_64_RELOC_GOT:
				// there is no good address to return here.
				// GOT slots are synthsized by the linker
				// this is used for the reference to the personality function in CIEs
				map[reloc->r_address()] = 0;
				break;
			default:
				fprintf(stderr, "ObjectFileAddressSpace::buildRelocatedMap() unexpected relocation at r_address=0x%08X\n", reloc->r_address());
				break;
		}
	}
}

template <typename A>
void ObjectFileAddressSpace<A>::buildRelocatedMap(const macho_section<P>* sect, std::map<uint32_t,uint64_t>& map)
{
	// in all architectures except x86_64, the section contents are already fixed up to point
	// to content in the same object file.
}

template <>
uint64_t ObjectFileAddressSpace<x86_64>::relocated(uint32_t sectOffset, uint32_t relocsOffset, uint32_t relocsCount)
{	
	// mach-o x86_64 is different, the content of a section with a relocation is the addend
	uint64_t result = 0;
	const macho_relocation_info<P>* relocs = (macho_relocation_info<P>*)((char*)(fReader.fHeader) + relocsOffset);
	const macho_relocation_info<P>* relocsEnd = &relocs[relocsCount];
	for (const macho_relocation_info<P>* reloc = relocs; reloc < relocsEnd; ++reloc) {
		//fprintf(stderr, "ObjectFileAddressSpace::relocated(0x%08X), r_address=0x%08X\n", sectOffset, reloc->r_address());
		if ( reloc->r_address() == sectOffset ) {
			switch ( reloc->r_type() ) {
				case X86_64_RELOC_UNSIGNED:
					result += fReader.fSymbols[reloc->r_symbolnum()].n_value();
					break;
				case X86_64_RELOC_SUBTRACTOR:
					result -= fReader.fSymbols[reloc->r_symbolnum()].n_value();
					break;
				case X86_64_RELOC_GOT:
					// there is no good address to return here.
					// GOT slots are synthsized by the linker
					// this is used for the reference to the personality function in CIEs
					result = 0;
					break;
				default:
					fprintf(stderr, "ObjectFileAddressSpace::relocated(0x%08X) => type=%d, value=0x%08X\n", sectOffset, reloc->r_type(), reloc->r_symbolnum());
					break;
			}
		}
	}
	//fprintf(stderr, "ObjectFileAddressSpace::relocated(0x%08X) => 0x%0llX\n", sectOffset, result);
	return result;
}

template <typename A>
typename A::P::uint_t ObjectFileAddressSpace<A>::relocated(uint32_t sectOffset, uint32_t relocsOffset, uint32_t relocsCount)
{
	// in all architectures except x86_64, the section contents are already fixed up to point
	// to content in the same object file.
	return 0;
}



// FSF exception handling Pointer-Encoding constants 
// Used in CFI augmentation by gcc compiler
enum {
	DW_EH_PE_ptr       = 0x00,
	DW_EH_PE_uleb128   = 0x01,
	DW_EH_PE_udata2    = 0x02,
	DW_EH_PE_udata4    = 0x03,
	DW_EH_PE_udata8    = 0x04,
	DW_EH_PE_signed    = 0x08,
	DW_EH_PE_sleb128   = 0x09,
	DW_EH_PE_sdata2    = 0x0A,
	DW_EH_PE_sdata4    = 0x0B,
	DW_EH_PE_sdata8    = 0x0C,
	DW_EH_PE_absptr    = 0x00,
	DW_EH_PE_pcrel     = 0x10,
	DW_EH_PE_textrel   = 0x20,
	DW_EH_PE_datarel   = 0x30,
	DW_EH_PE_funcrel   = 0x40,
	DW_EH_PE_aligned   = 0x50,
	DW_EH_PE_indirect  = 0x80,
	DW_EH_PE_omit      = 0xFF
};

template <>
void Reader<x86_64>::addCiePersonalityReference(BaseAtom* cieAtom, uint32_t offsetInCIE, uint8_t encoding)
{
	if ( encoding != (DW_EH_PE_indirect|DW_EH_PE_pcrel|DW_EH_PE_sdata4) )
		throw "unexpected personality encoding in CIE";

	// walk relocs looking for reloc in this CIE
	uint32_t sectOffset = (cieAtom->getObjectAddress() + offsetInCIE) - fehFrameSection->addr();
	const macho_relocation_info<P>* relocs = (macho_relocation_info<P>*)((char*)(fHeader) + fehFrameSection->reloff());
	const macho_relocation_info<P>* relocsEnd = &relocs[fehFrameSection->nreloc()];
	for (const macho_relocation_info<P>* reloc = relocs; reloc < relocsEnd; ++reloc) {
		if ( reloc->r_address() == sectOffset ) {
			switch ( reloc->r_type() ) {
				case X86_64_RELOC_GOT:
					if ( !reloc->r_extern() )
						throw "GOT reloc not extern for personality function";
					new Reference<x86_64>(x86_64::kPCRel32GOT, AtomAndOffset(cieAtom, offsetInCIE), &fStrings[fSymbols[reloc->r_symbolnum()].n_strx()], 4);
					return;
				default:
					throw "expected GOT reloc for personality function";
			}
		}
	}
	throw "personality function not found for CIE";
}

template <>
bool Reader<ppc>::isSectDiffReloc(uint8_t r_type)
{
	switch ( r_type ) {
		case PPC_RELOC_LOCAL_SECTDIFF:
		case PPC_RELOC_SECTDIFF:
			return true;
	}
	return false;
}

template <>
bool Reader<ppc64>::isSectDiffReloc(uint8_t r_type)
{
	switch ( r_type ) {
		case PPC_RELOC_LOCAL_SECTDIFF:
		case PPC_RELOC_SECTDIFF:
			return true;
	}
	return false;
}

template <>
bool Reader<x86>::isSectDiffReloc(uint8_t r_type)
{
	switch ( r_type ) {
		case GENERIC_RELOC_LOCAL_SECTDIFF:
		case GENERIC_RELOC_SECTDIFF:
			return true;
	}
	return false;
}

template <>
bool Reader<arm>::isSectDiffReloc(uint8_t r_type)
{
	switch ( r_type ) {
		case ARM_RELOC_LOCAL_SECTDIFF:
		case ARM_RELOC_SECTDIFF:
			return true;
	}
	return false;
}

template <typename A>
void Reader<A>::addCiePersonalityReference(BaseAtom* cieAtom, uint32_t offsetInCIE, uint8_t encoding)
{
	if ( (encoding != (DW_EH_PE_indirect|DW_EH_PE_pcrel|DW_EH_PE_sdata4)) && (encoding != (DW_EH_PE_indirect|DW_EH_PE_pcrel)) )
		throw "unexpected personality encoding in CIE";

	// walk relocs looking for personality reloc in this CIE
	uint32_t sectOffset = (cieAtom->getObjectAddress() + offsetInCIE) - fehFrameSection->addr();
	const macho_relocation_info<P>* relocs = (macho_relocation_info<P>*)((char*)(fHeader) + fehFrameSection->reloff());
	const macho_relocation_info<P>* relocsEnd = &relocs[fehFrameSection->nreloc()];
	for (const macho_relocation_info<P>* reloc = relocs; reloc < relocsEnd; ++reloc) {
		if ( (reloc->r_address() & R_SCATTERED) == 0 ) {
			// ignore
		}
		else {
		const macho_scattered_relocation_info<P>* sreloc = (macho_scattered_relocation_info<P>*)reloc;
			if ( sreloc->r_address() == sectOffset ) {
				if ( isSectDiffReloc(sreloc->r_type()) ) {
					// r_value is address of non-lazy-pointer to personality function
					new Reference<A>(A::kPointerDiff32, AtomAndOffset(cieAtom, offsetInCIE), AtomAndOffset(cieAtom, offsetInCIE), 
											findAtomAndOffset(sreloc->r_value()));
					return;
				}
			}
		}
	}
	throw "can't find relocation for personality in CIE";
}

template <typename A>
void Reader<A>::addFdeReference(uint8_t encoding, AtomAndOffset inFDE, AtomAndOffset target)
{
	if ( (encoding & 0xF0) != DW_EH_PE_pcrel ) 
		throw "unsupported encoding in FDE";
	Kinds kind = A::kNoFixUp;
	switch ( encoding & 0xF ) {
		case DW_EH_PE_ptr:
			kind = A::kPointerDiff;
			break;
		case DW_EH_PE_sdata4:
			kind = A::kPointerDiff32;
			break;
		default:
			throw "unsupported encoding in FDE";
	}
	new Reference<A>(kind, inFDE, inFDE, target);
}

template <typename A>
typename A::P::uint_t ObjectFileAddressSpace<A>::getEncodedP(pint_t& addr, pint_t end, uint8_t encoding)
{
	pint_t startAddr = addr;
	pint_t p = addr;
	pint_t result;
	
	// first get value
	switch (encoding & 0x0F) {
		case DW_EH_PE_ptr:
			result = getP(addr);
			p += sizeof(pint_t);
			addr = (pint_t)p;
			break;
		case DW_EH_PE_uleb128:
			result = getULEB128(addr, end);
			break;
		case DW_EH_PE_udata2:
			result = get16(addr);
			p += 2;
			addr = (pint_t)p;
			break;
		case DW_EH_PE_udata4:
			result = get32(addr);
			p += 4;
			addr = (pint_t)p;
			break;
		case DW_EH_PE_udata8:
			result = get64(addr);
			p += 8;
			addr = (pint_t)p;
			break;
		case DW_EH_PE_sleb128:
			result = getSLEB128(addr, end);
			break;
		case DW_EH_PE_sdata2:
			result = (int16_t)get16(addr);
			p += 2;
			addr = (pint_t)p;
			break;
		case DW_EH_PE_sdata4:
			result = (int32_t)get32(addr);
			p += 4;
			addr = (pint_t)p;
			break;
		case DW_EH_PE_sdata8:
			result = get64(addr);
			p += 8;
			addr = (pint_t)p;
			break;
		default:
			throwf("ObjectFileAddressSpace<A>::getEncodedP() encoding 0x%08X not supported", encoding);
	}
	
	// then add relative offset
	switch ( encoding & 0x70 ) {
		case DW_EH_PE_absptr:
			// do nothing
			break;
		case DW_EH_PE_pcrel:
			result += startAddr;
			break;
		case DW_EH_PE_textrel:
			throw "DW_EH_PE_textrel pointer encoding not supported";
			break;
		case DW_EH_PE_datarel:
			throw "DW_EH_PE_datarel pointer encoding not supported";
			break;
		case DW_EH_PE_funcrel:
			throw "DW_EH_PE_funcrel pointer encoding not supported";
			break;
		case DW_EH_PE_aligned:
			throw "DW_EH_PE_aligned pointer encoding not supported";
			break;
		default:
			throwf("ObjectFileAddressSpace<A>::getEncodedP() encoding 0x%08X not supported", encoding);
			break;
	}
	
	if ( encoding & DW_EH_PE_indirect )
		result = getP(result);
	
	return result;
}

template <>
uint32_t SymbolAtom<x86>::getCompactUnwindEncoding(uint64_t ehAtomAddress)
{
	pint_t lsda;
	pint_t personality;
	char warningBuffer[1024];
	uint32_t result = libunwind::DwarfInstructions<class ObjectFileAddressSpace<x86>, libunwind::Registers_x86>::createCompactEncodingFromFDE(
											fOwner.fObjectAddressSpace, ehAtomAddress, &lsda, &personality, warningBuffer);
	if ( (result & UNWIND_X86_MODE_MASK) == UNWIND_X86_MODE_DWARF ) {
		//if ( fOwner.fOptions.fForDyld )
		//	throwf("can't make compact unwind encoding from dwarf for %s",  this->getDisplayName());
		//else
			if ( fOwner.fOptions.fWarnCompactUnwind )
				warning("can't make compact unwind encoding from dwarf for %s in %s because %s", this->getDisplayName(), fOwner.getPath(), warningBuffer);
	}
	return result;
}

template <>
uint32_t SymbolAtom<x86_64>::getCompactUnwindEncoding(uint64_t ehAtomAddress)
{
	pint_t lsda;
	pint_t personality;
	char warningBuffer[1024];
	uint32_t result = libunwind::DwarfInstructions<class ObjectFileAddressSpace<x86_64>, libunwind::Registers_x86_64>::createCompactEncodingFromFDE(
											fOwner.fObjectAddressSpace, ehAtomAddress, &lsda, &personality, warningBuffer);
	if ( (result & UNWIND_X86_64_MODE_MASK) == UNWIND_X86_64_MODE_DWARF ) {
		//if ( fOwner.fOptions.fForDyld )
		//	throwf("can't make compact unwind encoding from dwarf for %s",  this->getDisplayName());
		//else
			if ( fOwner.fOptions.fWarnCompactUnwind )
				warning("can't make compact unwind encoding from dwarf for %s in %s because %s", this->getDisplayName(), fOwner.getPath(), warningBuffer);
	}
	return result;
}

template <>
uint32_t SymbolAtom<ppc>::getCompactUnwindEncoding(uint64_t ehAtomAddress)
{
	// compact encoding not supported for ppc
	return 0;
}

template <>
uint32_t SymbolAtom<ppc64>::getCompactUnwindEncoding(uint64_t ehAtomAddress)
{
	// compact encoding not supported for ppc64
	return 0;
}

template <>
uint32_t SymbolAtom<arm>::getCompactUnwindEncoding(uint64_t ehAtomAddress)
{
	// compact encoding not supported for arm
	return 0;
}


template <typename A>
uint8_t SymbolAtom<A>::getLSDAReferenceKind() const 
{
	return A::kGroupSubordinate;
}

template <>
uint8_t SymbolAtom<x86_64>::getPersonalityReferenceKind() const
{
	return x86_64::kGOTNoFixUp;
}

template <>
uint8_t SymbolAtom<x86>::getPersonalityReferenceKind() const
{
	return x86::kNoFixUp;
}

template <typename A>
uint8_t SymbolAtom<A>::getPersonalityReferenceKind() const
{
	// only used with architectures that support compact unwinding 
	return 0;
}


template <>
uint32_t AnonymousAtom<x86>::getCompactUnwindEncoding(uint64_t ehAtomAddress)
{
	pint_t lsda;
	pint_t personality;
	char warningBuffer[1024];
	uint32_t result = libunwind::DwarfInstructions<class ObjectFileAddressSpace<x86>, libunwind::Registers_x86>::createCompactEncodingFromFDE(
											fOwner.fObjectAddressSpace, ehAtomAddress, &lsda, &personality, warningBuffer);
	if ( (result & UNWIND_X86_MODE_MASK) == UNWIND_X86_MODE_DWARF ) {
		//if ( fOwner.fOptions.fForDyld )
		//	throwf("can't make compact unwind encoding from dwarf for %s",  this->getDisplayName());
		//else
		if ( fOwner.fOptions.fWarnCompactUnwind )
			warning("can't make compact unwind encoding from dwarf for %s in %s", this->getDisplayName(), fOwner.getPath());
	}
	return result;
}

template <>
uint32_t AnonymousAtom<x86_64>::getCompactUnwindEncoding(uint64_t ehAtomAddress)
{
	pint_t lsda;
	pint_t personality;
	char warningBuffer[1024];
	uint32_t result = libunwind::DwarfInstructions<class ObjectFileAddressSpace<x86_64>, libunwind::Registers_x86_64>::createCompactEncodingFromFDE(
												fOwner.fObjectAddressSpace, ehAtomAddress, &lsda, &personality, warningBuffer);
	if ( (result & UNWIND_X86_64_MODE_MASK) == UNWIND_X86_64_MODE_DWARF ) {
		//if ( fOwner.fOptions.fForDyld )
		//	throwf("can't make compact unwind encoding from dwarf for %s",  this->getDisplayName());
		//else
		if ( fOwner.fOptions.fWarnCompactUnwind )
			warning("can't make compact unwind encoding from dwarf for %s in %s", this->getDisplayName(), fOwner.getPath());
	}
	return result;
}

template <>
uint32_t AnonymousAtom<ppc>::getCompactUnwindEncoding(uint64_t ehAtomAddress)
{
	// compact encoding not supported for ppc
	return 0;
}

template <>
uint32_t AnonymousAtom<ppc64>::getCompactUnwindEncoding(uint64_t ehAtomAddress)
{
	// compact encoding not supported for ppc64
	return 0;
}

template <>
uint32_t AnonymousAtom<arm>::getCompactUnwindEncoding(uint64_t ehAtomAddress)
{
	// compact encoding not supported for arm
	return 0;
}


template <typename A>
uint8_t AnonymousAtom<A>::getLSDAReferenceKind() const 
{
	return A::kGroupSubordinate;
}

template <>
uint8_t AnonymousAtom<x86_64>::getPersonalityReferenceKind() const
{
	return x86_64::kGOTNoFixUp;
}

template <>
uint8_t AnonymousAtom<x86>::getPersonalityReferenceKind() const
{
	return x86::kNoFixUp;
}

template <typename A>
uint8_t AnonymousAtom<A>::getPersonalityReferenceKind() const
{
	// only used with architectures that support compact unwinding 
	return 0;
}







template <>
void Reader<ppc>::setCpuConstraint(uint32_t cpusubtype)
{
	switch (cpusubtype) {
		case CPU_SUBTYPE_POWERPC_ALL:
		case CPU_SUBTYPE_POWERPC_750:
		case CPU_SUBTYPE_POWERPC_7400:
		case CPU_SUBTYPE_POWERPC_7450:
		case CPU_SUBTYPE_POWERPC_970:
			fCpuConstraint = cpusubtype;
			break;
		default:
			warning("unknown ppc subtype 0x%08X in %s, defaulting to ALL", cpusubtype, fPath);
			fCpuConstraint = CPU_SUBTYPE_POWERPC_ALL;
            break;
	}
}

template <>
void Reader<arm>::setCpuConstraint(uint32_t cpusubtype)
{
	switch (cpusubtype) {
		case CPU_SUBTYPE_ARM_ALL:
		case CPU_SUBTYPE_ARM_V4T:
		case CPU_SUBTYPE_ARM_V5TEJ:
		case CPU_SUBTYPE_ARM_V6:
		case CPU_SUBTYPE_ARM_XSCALE:
		case CPU_SUBTYPE_ARM_V7:
			fCpuConstraint = cpusubtype;
			break;
		default:
			warning("unknown arm subtype 0x%08X in %s, defaulting to ALL", cpusubtype, fPath);
			fCpuConstraint = CPU_SUBTYPE_ARM_ALL;
            break;
	}
}

template <typename A>
void Reader<A>::setCpuConstraint(uint32_t cpusubtype)
{
	// no cpu sub types for this architecture
}

template <>
uint32_t Reader<ppc>::updateCpuConstraint(uint32_t previous)
{
	switch ( previous ) {
    case CPU_SUBTYPE_POWERPC_ALL:
        return fCpuConstraint;
        break;
    case CPU_SUBTYPE_POWERPC_750:
        if ( fCpuConstraint == CPU_SUBTYPE_POWERPC_7400 ||
             fCpuConstraint == CPU_SUBTYPE_POWERPC_7450 ||
             fCpuConstraint == CPU_SUBTYPE_POWERPC_970 )
            return fCpuConstraint;
        break;
    case CPU_SUBTYPE_POWERPC_7400:
    case CPU_SUBTYPE_POWERPC_7450:
        if ( fCpuConstraint == CPU_SUBTYPE_POWERPC_970 )
            return fCpuConstraint;
        break;
    case CPU_SUBTYPE_POWERPC_970:
        // G5 can run everything
        break;
    default:
        throw "Unhandled PPC cpu subtype!";
        break;
	}
    return previous;
}



template <>
uint32_t Reader<arm>::updateCpuConstraint(uint32_t previous)
{
    switch (previous) {
		case CPU_SUBTYPE_ARM_ALL:
			return fCpuConstraint;
			break;
		case CPU_SUBTYPE_ARM_V5TEJ:
			// v6, v7, and xscale are more constrained than previous file (v5), so use it
			if (   (fCpuConstraint == CPU_SUBTYPE_ARM_V6)
				|| (fCpuConstraint == CPU_SUBTYPE_ARM_V7) 
				|| (fCpuConstraint == CPU_SUBTYPE_ARM_XSCALE) )
				return fCpuConstraint;
			break;
		case CPU_SUBTYPE_ARM_V4T:
			// v5, v6, v7, and xscale are more constrained than previous file (v4t), so use it
			if (   (fCpuConstraint == CPU_SUBTYPE_ARM_V7)
				|| (fCpuConstraint == CPU_SUBTYPE_ARM_V6)
				|| (fCpuConstraint == CPU_SUBTYPE_ARM_V5TEJ)
				|| (fCpuConstraint == CPU_SUBTYPE_ARM_XSCALE) )
				return fCpuConstraint;
			break;
		case CPU_SUBTYPE_ARM_V6:
			// v6 can run everything except xscale and v7
			if ( fCpuConstraint == CPU_SUBTYPE_ARM_XSCALE )
				throw "can't mix xscale and v6 code";
			if ( fCpuConstraint == CPU_SUBTYPE_ARM_V7 )
				return fCpuConstraint;
			break;
		case CPU_SUBTYPE_ARM_XSCALE:
			// xscale can run everything except v6 and v7
			if ( fCpuConstraint == CPU_SUBTYPE_ARM_V6 )
				throw "can't mix xscale and v6 code";
			if ( fCpuConstraint == CPU_SUBTYPE_ARM_V7 )
				throw "can't mix xscale and v7 code";
			break;
		case CPU_SUBTYPE_ARM_V7:
			// v7 can run everything except xscale
			if ( fCpuConstraint == CPU_SUBTYPE_ARM_XSCALE )
				throw "can't mix xscale and v7 code";
			break;
		default:
			throw "Unhandled ARM cpu subtype!";
    }
    return previous;
}

template <typename A>
uint32_t Reader<A>::updateCpuConstraint(uint32_t current)
{
	// no cpu sub types for this architecture
	return current;
}

template <typename A>
void Reader<A>::addDtraceExtraInfos(uint32_t probeAddr, const char* providerName)
{
	// for every ___dtrace_stability$* and ___dtrace_typedefs$* undefine with
	// a matching provider name, add a by-name kDtraceTypeReference at probe site
 	const char* dollar = strchr(providerName, '$');
	if ( dollar != NULL ) {
		int providerNameLen = dollar-providerName+1;
		for ( std::vector<const char*>::iterator it = fDtraceProviderInfo.begin(); it != fDtraceProviderInfo.end(); ++it) {
			const char* typeDollar = strchr(*it, '$');
			if ( typeDollar != NULL ) {
				if ( strncmp(typeDollar+1, providerName, providerNameLen) == 0 ) {
					makeByNameReference(A::kDtraceTypeReference, probeAddr, *it, 0);
				}
			}
		}
	}
}


template <>
void Reader<x86_64>::validSectionType(uint8_t type)
{
	switch ( type ) {
		case S_SYMBOL_STUBS:
			throw "symbol_stub sections not valid in x86_64 object files";
		case S_LAZY_SYMBOL_POINTERS:
			throw "lazy pointer sections not valid in x86_64 object files";
		case S_NON_LAZY_SYMBOL_POINTERS:
			throw "non lazy pointer sections not valid in x86_64 object files";
	}
}

template <typename A>
void Reader<A>::validSectionType(uint8_t type)
{
}

template <typename A>
bool Reader<A>::getTranslationUnitSource(const char** dir, const char** name) const
{
	if ( fDebugInfo == kDebugInfoDwarf ) {
		*dir = fDwarfTranslationUnitDir;
		*name = fDwarfTranslationUnitFile;
		return (fDwarfTranslationUnitFile != NULL);
	}
	return false;
}

template <typename A>
BaseAtom* Reader<A>::findAtomByName(const char* name)
{
	// first search the more important atoms
	for (typename AddrToAtomMap::iterator it=fAddrToAtom.begin(); it != fAddrToAtom.end(); it++) {
		const char* atomName = it->second->getName();
		if ( (atomName != NULL) && (strcmp(atomName, name) == 0) ) {
			return it->second;
		}
	}
	// try all atoms, because this might have been a tentative definition
	for (std::vector<BaseAtom*>::iterator it=fAtoms.begin(); it != fAtoms.end(); it++) {
		BaseAtom* atom = (BaseAtom*)(*it);
		const char* atomName = atom->getName();
		if ( (atomName != NULL) && (strcmp(atomName, name) == 0) ) {
			return atom;
		}
	}
	return NULL;
}

template <typename A>
Reference<A>* Reader<A>::makeReference(Kinds kind, pint_t atAddr, pint_t toAddr)
{
	return new Reference<A>(kind, findAtomAndOffset(atAddr), findAtomAndOffset(toAddr));
}

template <typename A>
Reference<A>* Reader<A>::makeReference(Kinds kind, pint_t atAddr, pint_t fromAddr, pint_t toAddr)
{
	return new Reference<A>(kind, findAtomAndOffset(atAddr), findAtomAndOffset(fromAddr), findAtomAndOffset(toAddr));
}

template <typename A>
Reference<A>* Reader<A>::makeReferenceWithToBase(Kinds kind, pint_t atAddr, pint_t toAddr, pint_t toBaseAddr)
{
	return new Reference<A>(kind, findAtomAndOffset(atAddr), findAtomAndOffset(toBaseAddr, toAddr));
}

template <typename A>
Reference<A>* Reader<A>::makeReferenceWithToBase(Kinds kind, pint_t atAddr, pint_t fromAddr, pint_t toAddr, pint_t toBaseAddr)
{
	return new Reference<A>(kind, findAtomAndOffset(atAddr), findAtomAndOffset(fromAddr), findAtomAndOffset(toBaseAddr, toAddr));
}

template <typename A>
Reference<A>* Reader<A>::makeByNameReference(Kinds kind, pint_t atAddr, const char* toName, uint32_t toOffset)
{
	return new Reference<A>(kind, findAtomAndOffset(atAddr), toName, toOffset);
}

template <typename A>
BaseAtom* Reader<A>::makeReferenceToEH(const char* ehName, pint_t ehAtomAddress, const macho_section<P>* ehSect)
{
	// add a group subordinate reference from function atom to its eh frame atom
	const uint8_t* ehContent = (const uint8_t*)(fHeader) + ehAtomAddress - ehSect->addr() + ehSect->offset();
	int32_t deltaMinus8 = P::getP(*(pint_t*)(&ehContent[8]));	// offset 8 in eh info is delta to function
	pint_t funcAddr = ehAtomAddress + deltaMinus8 + 8;
	ObjectFile::Atom* funcAtom = findAtomAndOffset(funcAddr).atom;
	ObjectFile::Atom* ehAtom = findAtomAndOffset(ehAtomAddress).atom;
	new Reference<A>(A::kGroupSubordinate, funcAtom, ehAtom);
	return (BaseAtom*)funcAtom;
}


template <>
Reference<x86_64>* Reader<x86_64>::makeByNameReference(Kinds kind, pint_t atAddr, const char* toName, uint32_t toOffset)
{
	// x86_64 uses external relocations everywhere, so external relocations do not imply by-name references
	// instead check scope of target
	BaseAtom* target = findAtomByName(toName);
	if ( (target != NULL) && (target->getScope() == ObjectFile::Atom::scopeTranslationUnit) )
		return new Reference<x86_64>(kind, findAtomAndOffset(atAddr), AtomAndOffset(target, toOffset));
	else
		return new Reference<x86_64>(kind, findAtomAndOffset(atAddr), toName, toOffset);
}

template <>
Reference<x86_64>* Reader<x86_64>::makeReferenceToSymbol(Kinds kind, pint_t atAddr, const macho_nlist<P>* toSymbol, pint_t toOffset)
{
	// x86_64 uses external relocations everywhere, so external relocations do not imply by-name references
	// instead check scope of target
	const char* symbolName = &fStrings[toSymbol->n_strx()];
	if ( ((toSymbol->n_type() & N_TYPE) == N_SECT) && (((toSymbol->n_type() & N_EXT) == 0) || (symbolName[0] == 'L')) ) 
		return new Reference<x86_64>(kind, findAtomAndOffset(atAddr), findAtomAndOffset(toSymbol->n_value(), toSymbol->n_value()+toOffset));
	else
		return new Reference<x86_64>(kind, findAtomAndOffset(atAddr), symbolName, toOffset);
}


template <>
BaseAtom* Reader<x86_64>::makeReferenceToEH(const char* ehName, pint_t ehAtomAddress, const macho_section<P>* ehSect)
{
	// add a group subordinate reference from function atom to its eh frame atom
	// for x86_64 the __eh_frame section contains the addends, so need to use relocs to find target
	uint32_t ehAtomDeltaSectionOffset = ehAtomAddress + 8 - ehSect->addr(); // offset 8 in eh info is delta to function
	const macho_relocation_info<P>* relocs = (macho_relocation_info<P>*)((char*)(fHeader) + ehSect->reloff());
	const macho_relocation_info<P>* relocsEnd = &relocs[ehSect->nreloc()];
	for (const macho_relocation_info<P>* reloc = relocs; reloc < relocsEnd; ++reloc) {
		if ( (reloc->r_address() == ehAtomDeltaSectionOffset) && (reloc->r_type() == X86_64_RELOC_UNSIGNED) ) {
			pint_t funcAddr = fSymbols[reloc->r_symbolnum()].n_value();
			ObjectFile::Atom* funcAtom = findAtomAndOffset(funcAddr).atom;
			ObjectFile::Atom* ehAtom = findAtomAndOffset(ehAtomAddress).atom;
			new Reference<x86_64>(x86_64::kGroupSubordinate, funcAtom, ehAtom);
			return (BaseAtom*)funcAtom;
		}
	}
	warning("can't find matching function for eh symbol %s", ehName);
	return NULL;
}


template <typename A>
AtomAndOffset Reader<A>::findAtomAndOffset(pint_t addr)
{
	// STL has no built-in for "find largest key that is same or less than"
	typename AddrToAtomMap::iterator it = fAddrToAtom.upper_bound(addr);
	// if no atoms up to this address return none found
	if ( it == fAddrToAtom.begin() ) 
		return AtomAndOffset(NULL);
	// otherwise upper_bound gets us next key, so we back up one	
	--it;
	AtomAndOffset result;
	result.atom = it->second;
	result.offset = addr - it->first;
	//fprintf(stderr, "findAtomAndOffset(0x%0llX) ==> %s (0x%0llX -> 0x%0llX)\n",
	//			(uint64_t)addr, result.atom->getDisplayName(), (uint64_t)it->first, it->first+result.atom->getSize());
	return result;
}

// "scattered" relocations enable you to offset into an atom past the end of it
// baseAddr is the address of the target atom,
// realAddr is the points into it
template <typename A>
AtomAndOffset Reader<A>::findAtomAndOffset(pint_t baseAddr, pint_t realAddr)
{
	typename AddrToAtomMap::iterator it = fAddrToAtom.find(baseAddr);
	if ( it != fAddrToAtom.end() ) {
		AtomAndOffset result;
		result.atom = it->second;
		result.offset = realAddr - it->first;
		if ( result.atom->isThumb() )
			result.offset &= -2;
		//fprintf(stderr, "findAtomAndOffset(0x%08X, 0x%08X) => %s + 0x%08X\n", baseAddr, realAddr, result.atom->getDisplayName(), result.offset);
		return result;
	}
	// getting here means we have a scattered relocation to an address without a label
	// so, find the atom that contains the baseAddr, and offset from that to the readAddr
	AtomAndOffset result = findAtomAndOffset(baseAddr);
	result.offset += (realAddr-baseAddr);
	return result;
}


/* Skip over a LEB128 value (signed or unsigned).  */
static void
skip_leb128 (const uint8_t ** offset, const uint8_t * end)
{
  while (*offset != end && **offset >= 0x80)
    (*offset)++;
  if (*offset != end)
    (*offset)++;
}

/* Read a ULEB128 into a 64-bit word.  Return (uint64_t)-1 on overflow
   or error.  On overflow, skip past the rest of the uleb128.  */
static uint64_t
read_uleb128 (const uint8_t ** offset, const uint8_t * end)
{
  uint64_t result = 0;
  int bit = 0;

  do  {
    uint64_t b;

    if (*offset == end)
      return (uint64_t) -1;

    b = **offset & 0x7f;

    if (bit >= 64 || b << bit >> bit != b)
      result = (uint64_t) -1;
    else
      result |= b << bit, bit += 7;
  } while (*(*offset)++ >= 0x80);
  return result;
}


/* Skip over a DWARF attribute of form FORM.  */
template <typename A>
bool Reader<A>::skip_form(const uint8_t ** offset, const uint8_t * end, uint64_t form,
							uint8_t addr_size, bool dwarf64)
{
  int64_t sz=0;

  switch (form)
    {
    case DW_FORM_addr:
      sz = addr_size;
      break;

    case DW_FORM_block2:
      if (end - *offset < 2)
	return false;
      sz = 2 + A::P::E::get16(*(uint16_t*)offset);
      break;

    case DW_FORM_block4:
      if (end - *offset < 4)
	return false;
      sz = 2 + A::P::E::get32(*(uint32_t*)offset);
      break;

    case DW_FORM_data2:
    case DW_FORM_ref2:
      sz = 2;
      break;

    case DW_FORM_data4:
    case DW_FORM_ref4:
      sz = 4;
      break;

    case DW_FORM_data8:
    case DW_FORM_ref8:
      sz = 8;
      break;

    case DW_FORM_string:
      while (*offset != end && **offset)
	++*offset;
    case DW_FORM_data1:
    case DW_FORM_flag:
    case DW_FORM_ref1:
      sz = 1;
      break;

    case DW_FORM_block:
      sz = read_uleb128 (offset, end);
      break;

    case DW_FORM_block1:
      if (*offset == end)
	return false;
      sz = 1 + **offset;
      break;

    case DW_FORM_sdata:
    case DW_FORM_udata:
    case DW_FORM_ref_udata:
      skip_leb128 (offset, end);
      return true;

    case DW_FORM_strp:
    case DW_FORM_ref_addr:
      sz = 4;
      break;

    default:
      return false;
    }
  if (end - *offset < sz)
    return false;
  *offset += sz;
  return true;
}

template <typename A>
const char* Reader<A>::getDwarfString(uint64_t form, const uint8_t* p)
{
	if ( form == DW_FORM_string )
		return (const char*)p;
	else if ( form == DW_FORM_strp ) {
		uint32_t offset = E::get32(*((uint32_t*)p));
		const char* dwarfStrings = (char*)(fHeader) + fDwarfDebugStringSect->offset();
		if ( offset > fDwarfDebugStringSect->size() ) {
			warning("unknown dwarf DW_FORM_strp (offset=0x%08X) is too big in %s\n", offset, this->getPath());
			return NULL;
		}
		return &dwarfStrings[offset];
	}
	warning("unknown dwarf string encoding (form=%lld) in %s\n", form, this->getPath());
	return NULL;
}


// Look at the compilation unit DIE and determine
// its NAME, compilation directory (in COMP_DIR) and its
// line number information offset (in STMT_LIST).  NAME and COMP_DIR
// may be NULL (especially COMP_DIR) if they are not in the .o file;
// STMT_LIST will be (uint64_t) -1.
//
// At present this assumes that there's only one compilation unit DIE.
//
template <typename A>
bool Reader<A>::read_comp_unit(const char ** name, const char ** comp_dir,
							uint64_t *stmt_list)
{
	const uint8_t * debug_info;
	const uint8_t * debug_abbrev;
	const uint8_t * di;
	const uint8_t * da;
	const uint8_t * end;
	const uint8_t * enda;
	uint64_t sz;
	uint16_t vers;
	uint64_t abbrev_base;
	uint64_t abbrev;
	uint8_t address_size;
	bool dwarf64;

	*name = NULL;
	*comp_dir = NULL;
	*stmt_list = (uint64_t) -1;

	if ( (fDwarfDebugInfoSect == NULL) || (fDwarfDebugAbbrevSect == NULL) )
		return false;

	debug_info = (uint8_t*)(fHeader) + fDwarfDebugInfoSect->offset();
	debug_abbrev = (uint8_t*)(fHeader) + fDwarfDebugAbbrevSect->offset();
	di = debug_info;

	if (fDwarfDebugInfoSect->size() < 12)
		/* Too small to be a real debug_info section.  */
		return false;
	sz = A::P::E::get32(*(uint32_t*)di);
	di += 4;
	dwarf64 = sz == 0xffffffff;
	if (dwarf64)
		sz = A::P::E::get64(*(uint64_t*)di), di += 8;
	else if (sz > 0xffffff00)
		/* Unknown dwarf format.  */
		return false;

	/* Verify claimed size.  */
	if (sz + (di - debug_info) > fDwarfDebugInfoSect->size() || sz <= (dwarf64 ? 23 : 11))
		return false;

	vers = A::P::E::get16(*(uint16_t*)di);
	if (vers < 2 || vers > 3)
	/* DWARF version wrong for this code.
	   Chances are we could continue anyway, but we don't know for sure.  */
		return false;
	di += 2;

	/* Find the debug_abbrev section.  */
	abbrev_base = dwarf64 ? A::P::E::get64(*(uint64_t*)di) : A::P::E::get32(*(uint32_t*)di);
	di += dwarf64 ? 8 : 4;

	if (abbrev_base > fDwarfDebugAbbrevSect->size())
		return false;
	da = debug_abbrev + abbrev_base;
	enda = debug_abbrev + fDwarfDebugAbbrevSect->size();

	address_size = *di++;

	/* Find the abbrev number we're looking for.  */
	end = di + sz;
	abbrev = read_uleb128 (&di, end);
	if (abbrev == (uint64_t) -1)
		return false;

	/* Skip through the debug_abbrev section looking for that abbrev.  */
	for (;;)
	{
		uint64_t this_abbrev = read_uleb128 (&da, enda);
		uint64_t attr;

		if (this_abbrev == abbrev)
			/* This is almost always taken.  */
			break;
		skip_leb128 (&da, enda); /* Skip the tag.  */
		if (da == enda)
			return false;
		da++;  /* Skip the DW_CHILDREN_* value.  */

		do {
			attr = read_uleb128 (&da, enda);
			skip_leb128 (&da, enda);
		} while (attr != 0 && attr != (uint64_t) -1);
		if (attr != 0)
			return false;
	}

	/* Check that the abbrev is one for a DW_TAG_compile_unit.  */
	if (read_uleb128 (&da, enda) != DW_TAG_compile_unit)
	return false;
	if (da == enda)
	return false;
	da++;  /* Skip the DW_CHILDREN_* value.  */

	/* Now, go through the DIE looking for DW_AT_name,
	 DW_AT_comp_dir, and DW_AT_stmt_list.  */
	for (;;)
	{
		uint64_t attr = read_uleb128 (&da, enda);
		uint64_t form = read_uleb128 (&da, enda);

		if (attr == (uint64_t) -1)
			return false;
		else if (attr == 0)
			return true;

		if (form == DW_FORM_indirect)
			form = read_uleb128 (&di, end);

		if (attr == DW_AT_name)
			*name = getDwarfString(form, di);
		else if (attr == DW_AT_comp_dir)
			*comp_dir = getDwarfString(form, di);
		else if (attr == DW_AT_stmt_list && form == DW_FORM_data4)
			*stmt_list = A::P::E::get32(*(uint32_t*)di);
		else if (attr == DW_AT_stmt_list && form == DW_FORM_data8)
			*stmt_list = A::P::E::get64(*(uint64_t*)di);
		if (! skip_form (&di, end, form, address_size, dwarf64))
			return false;
	}
}

template <typename A>
const char* Reader<A>::assureFullPath(const char* path)
{
	if ( path[0] == '/' )
		return path;
	char cwdbuff[MAXPATHLEN];
	if ( getcwd(cwdbuff, MAXPATHLEN) != NULL ) {
		char* result;
		asprintf(&result, "%s/%s", cwdbuff, path);
		if ( result != NULL )
			return result;
	}
	return path;
}


//
//
//	To implement architecture xxx, you must write template specializations for the following six methods:
//			Reader<xxx>::validFile()
//			Reader<xxx>::addRelocReference()
//			Reference<xxx>::getDescription()
//
//


template <>
bool Reader<ppc>::validFile(const uint8_t* fileContent, bool, cpu_subtype_t)
{
	const macho_header<P>* header = (const macho_header<P>*)fileContent;
	if ( header->magic() != MH_MAGIC )
		return false;
	if ( header->cputype() != CPU_TYPE_POWERPC )
		return false;
	if ( header->filetype() != MH_OBJECT )
		return false;
	return true;
}

template <>
bool Reader<ppc64>::validFile(const uint8_t* fileContent, bool, cpu_subtype_t)
{
	const macho_header<P>* header = (const macho_header<P>*)fileContent;
	if ( header->magic() != MH_MAGIC_64 )
		return false;
	if ( header->cputype() != CPU_TYPE_POWERPC64 )
		return false;
	if ( header->filetype() != MH_OBJECT )
		return false;
	return true;
}

template <>
bool Reader<x86>::validFile(const uint8_t* fileContent, bool, cpu_subtype_t)
{
	const macho_header<P>* header = (const macho_header<P>*)fileContent;
	if ( header->magic() != MH_MAGIC )
		return false;
	if ( header->cputype() != CPU_TYPE_I386 )
		return false;
	if ( header->filetype() != MH_OBJECT )
		return false;
	return true;
}

template <>
bool Reader<x86_64>::validFile(const uint8_t* fileContent, bool, cpu_subtype_t)
{
	const macho_header<P>* header = (const macho_header<P>*)fileContent;
	if ( header->magic() != MH_MAGIC_64 )
		return false;
	if ( header->cputype() != CPU_TYPE_X86_64 )
		return false;
	if ( header->filetype() != MH_OBJECT )
		return false;
	return true;
}

template <>
bool Reader<arm>::validFile(const uint8_t* fileContent, bool subtypeMustMatch, cpu_subtype_t subtype)
{
	const macho_header<P>* header = (const macho_header<P>*)fileContent;
	if ( header->magic() != MH_MAGIC )
		return false;
	if ( header->cputype() != CPU_TYPE_ARM )
		return false;
	if ( header->filetype() != MH_OBJECT )
		return false;
	if ( subtypeMustMatch && ((cpu_subtype_t)header->cpusubtype() != subtype) )
		return false;
	return true;
}

template <typename A>
bool Reader<A>::isWeakImportSymbol(const macho_nlist<P>* sym)
{
	return ( ((sym->n_type() & N_TYPE) == N_UNDF) && ((sym->n_desc() & N_WEAK_REF) != 0) );
}

template <>
bool Reader<ppc64>::addRelocReference(const macho_section<ppc64::P>* sect, const macho_relocation_info<ppc64::P>* reloc)
{
	return addRelocReference_powerpc(sect, reloc);
}

template <>
bool Reader<ppc>::addRelocReference(const macho_section<ppc::P>* sect, const macho_relocation_info<ppc::P>* reloc)
{
	return addRelocReference_powerpc(sect, reloc);
}


//
// ppc and ppc64 both use the same relocations, so process them in one common routine
//
template <typename A>
bool Reader<A>::addRelocReference_powerpc(const macho_section<typename A::P>* sect,
										  const macho_relocation_info<typename A::P>* reloc)
{
	uint32_t srcAddr;
	uint32_t dstAddr;
	uint32_t* fixUpPtr;
	int32_t displacement = 0;
	uint32_t instruction = 0;
	uint32_t offsetInTarget;
	int16_t lowBits;
	bool result = false;
	if ( (reloc->r_address() & R_SCATTERED) == 0 ) {
		const macho_relocation_info<P>* nextReloc = &reloc[1];
		const char* targetName = NULL;
		bool weakImport = false;
		fixUpPtr = (uint32_t*)((char*)(fHeader) + sect->offset() + reloc->r_address());
		if ( reloc->r_type() != PPC_RELOC_PAIR )
			instruction = BigEndian::get32(*fixUpPtr);
		srcAddr = sect->addr() + reloc->r_address();
		if ( reloc->r_extern() ) {
			const macho_nlist<P>* targetSymbol = &fSymbols[reloc->r_symbolnum()];
			targetName = &fStrings[targetSymbol->n_strx()];
			weakImport = this->isWeakImportSymbol(targetSymbol);
		}
		switch ( reloc->r_type() ) {
			case PPC_RELOC_BR24:
				{
					if ( (instruction & 0x4C000000) == 0x48000000 ) {
						displacement = (instruction & 0x03FFFFFC);
						if ( (displacement & 0x02000000) != 0 )
							displacement |= 0xFC000000;
					}
					else {
						printf("bad instruction for BR24 reloc");
					}
					if ( reloc->r_extern() ) {
						offsetInTarget = srcAddr + displacement;
						if ( strncmp(targetName, "___dtrace_probe$", 16) == 0 ) {
							makeByNameReference(A::kDtraceProbeSite, srcAddr, targetName, 0);
							addDtraceExtraInfos(srcAddr, &targetName[16]);
						}
						else if ( strncmp(targetName, "___dtrace_isenabled$", 20) == 0 ) {
							makeByNameReference(A::kDtraceIsEnabledSite, srcAddr, targetName, 0);
							addDtraceExtraInfos(srcAddr, &targetName[20]);
						}
						else if ( weakImport )
							makeByNameReference(A::kBranch24WeakImport, srcAddr, targetName, offsetInTarget);
						else
							makeByNameReference(A::kBranch24, srcAddr, targetName, offsetInTarget);
					}
					else {
						dstAddr = srcAddr + displacement;
						// if this is a branch to a stub, we need to see if the stub is for a weak imported symbol
						ObjectFile::Atom* atom = findAtomAndOffset(dstAddr).atom;
						targetName = atom->getName();
						if ( (targetName != NULL) && (strncmp(targetName, "___dtrace_probe$", 16) == 0) ) {
							makeByNameReference(A::kDtraceProbeSite, srcAddr, targetName, 0);
							addDtraceExtraInfos(srcAddr, &targetName[16]);
						}
						else if ( (targetName != NULL) && (strncmp(targetName, "___dtrace_isenabled$", 20) == 0) ) {
							makeByNameReference(A::kDtraceIsEnabledSite, srcAddr, targetName, 0);
							addDtraceExtraInfos(srcAddr, &targetName[20]);
						}
						else if ( (atom->getSymbolTableInclusion() == ObjectFile::Atom::kSymbolTableNotIn)
							&& ((AnonymousAtom<A>*)atom)->isWeakImportStub() )
							makeReference(A::kBranch24WeakImport, srcAddr, dstAddr);
						else
							makeReference(A::kBranch24, srcAddr, dstAddr);
					}
				}
				break;
			case PPC_RELOC_BR14:
				{
					displacement = (instruction & 0x0000FFFC);
					if ( (displacement & 0x00008000) != 0 )
						displacement |= 0xFFFF0000;
					if ( reloc->r_extern() ) {
						offsetInTarget = srcAddr + displacement;
						makeByNameReference(A::kBranch14, srcAddr, targetName, offsetInTarget);
					}
					else {
						dstAddr = srcAddr + displacement;
						makeReference(A::kBranch14, srcAddr, dstAddr);
					}
				}
				break;
			case PPC_RELOC_PAIR:
				// skip, processed by a previous look ahead
				break;
			case PPC_RELOC_LO16:
				{
					if ( nextReloc->r_type() != PPC_RELOC_PAIR ) {
						throw "PPC_RELOC_LO16 missing following pair";
					}
					result = true;
					lowBits = (instruction & 0xFFFF);
					if ( reloc->r_extern() ) {
						offsetInTarget = (nextReloc->r_address() << 16) | ((uint32_t)lowBits & 0x0000FFFF);
						makeByNameReference(A::kAbsLow16, srcAddr, targetName, offsetInTarget);
					}
					else {
						dstAddr = (nextReloc->r_address() << 16) + ((uint32_t)lowBits & 0x0000FFFF);
						if ( reloc->r_symbolnum() == R_ABS ) {
							// find absolute symbol that corresponds to pointerValue
							typename AddrToAtomMap::iterator pos = fAddrToAbsoluteAtom.find(dstAddr);
							if ( pos != fAddrToAbsoluteAtom.end() ) 
								makeByNameReference(A::kAbsLow16, srcAddr, pos->second->getName(), 0);
							else
								makeReference(A::kAbsLow16, srcAddr, dstAddr);
						}
						else {
							makeReference(A::kAbsLow16, srcAddr, dstAddr);
						}
					}
				}
				break;
			case PPC_RELOC_LO14:
				{
					if ( nextReloc->r_type() != PPC_RELOC_PAIR ) {
						throw "PPC_RELOC_LO14 missing following pair";
					}
					result = true;
					lowBits = (instruction & 0xFFFC);
					if ( reloc->r_extern() ) {
						offsetInTarget = (nextReloc->r_address() << 16) | ((uint32_t)lowBits & 0x0000FFFF);
						makeByNameReference(A::kAbsLow14, srcAddr, targetName, offsetInTarget);
					}
					else {
						dstAddr = (nextReloc->r_address() << 16) | ((uint32_t)lowBits & 0x0000FFFF);
						if ( reloc->r_symbolnum() == R_ABS ) {
							// find absolute symbol that corresponds to pointerValue
							typename AddrToAtomMap::iterator pos = fAddrToAbsoluteAtom.find(dstAddr);
							if ( pos != fAddrToAbsoluteAtom.end() ) 
								makeByNameReference(A::kAbsLow14, srcAddr, pos->second->getName(), 0);
							else
								makeReference(A::kAbsLow14, srcAddr, dstAddr);
						}
						else {
							makeReference(A::kAbsLow14, srcAddr, dstAddr);
						}
					}
				}
				break;
			case PPC_RELOC_HI16:
				{
					if ( nextReloc->r_type() != PPC_RELOC_PAIR ) {
						throw "PPC_RELOC_HI16 missing following pair";
					}
					result = true;
					if ( reloc->r_extern() ) {
						offsetInTarget = ((instruction & 0x0000FFFF) << 16) | (nextReloc->r_address() & 0x0000FFFF);
						makeByNameReference(A::kAbsHigh16, srcAddr, targetName, offsetInTarget);
					}
					else {
						dstAddr = ((instruction & 0x0000FFFF) << 16) | (nextReloc->r_address() & 0x0000FFFF);
						if ( reloc->r_symbolnum() == R_ABS ) {
							// find absolute symbol that corresponds to pointerValue
							typename AddrToAtomMap::iterator pos = fAddrToAbsoluteAtom.find(dstAddr);
							if ( pos != fAddrToAbsoluteAtom.end() ) 
								makeByNameReference(A::kAbsHigh16, srcAddr, pos->second->getName(), 0);
							else
								makeReference(A::kAbsHigh16, srcAddr, dstAddr);
						}
						else {
							makeReference(A::kAbsHigh16, srcAddr, dstAddr);
						}
					}
				}
				break;
			case PPC_RELOC_HA16:
				{
					if ( nextReloc->r_type() != PPC_RELOC_PAIR ) {
						throw "PPC_RELOC_HA16 missing following pair";
					}
					result = true;
					lowBits = (nextReloc->r_address() & 0x0000FFFF);
					if ( reloc->r_extern() ) {
						offsetInTarget = ((instruction & 0x0000FFFF) << 16) + (int32_t)lowBits;
						makeByNameReference(A::kAbsHigh16AddLow, srcAddr, targetName, offsetInTarget);
					}
					else {
						dstAddr = ((instruction & 0x0000FFFF) << 16) + (int32_t)lowBits;
						if ( reloc->r_symbolnum() == R_ABS ) {
							// find absolute symbol that corresponds to pointerValue
							typename AddrToAtomMap::iterator pos = fAddrToAbsoluteAtom.find(dstAddr);
							if ( pos != fAddrToAbsoluteAtom.end() ) 
								makeByNameReference(A::kAbsHigh16AddLow, srcAddr, pos->second->getName(), 0);
							else
								makeReference(A::kAbsHigh16AddLow, srcAddr, dstAddr);
						}
						else {
							makeReference(A::kAbsHigh16AddLow, srcAddr, dstAddr);
						}
					}
				}
				break;
			case PPC_RELOC_VANILLA:
				{
					pint_t pointerValue = P::getP(*((pint_t*)fixUpPtr));
					if ( reloc->r_extern() ) {
						if ( weakImport )
							makeByNameReference(A::kPointerWeakImport, srcAddr, targetName, pointerValue);
						else
							makeByNameReference(A::kPointer, srcAddr, targetName, pointerValue);
					}
					else {
						makeReference(A::kPointer, srcAddr, pointerValue);
					}
				}
				break;
			case PPC_RELOC_JBSR:
				// this is from -mlong-branch codegen.  We ignore the jump island and make reference to the real target
				if ( nextReloc->r_type() != PPC_RELOC_PAIR ) {
					throw "PPC_RELOC_JBSR missing following pair";
				}
				if ( !fHasLongBranchStubs )
					warning("object file compiled with -mlong-branch which is no longer needed. To remove this warning, recompile without -mlong-branch: %s", fPath);
				fHasLongBranchStubs = true;
				result = true;
				if ( reloc->r_extern() ) {
					throw "PPC_RELOC_JBSR should not be using an external relocation";
				}
				makeReference(A::kBranch24, srcAddr, nextReloc->r_address());
				if ( (instruction & 0x4C000000) == 0x48000000 ) {
					displacement = (instruction & 0x03FFFFFC);
					if ( (displacement & 0x02000000) != 0 )
						displacement |= 0xFC000000;
				}
				else {
					fprintf(stderr, "bad instruction for BR24 reloc");
				}
				break;
			default:
				warning("unknown relocation type %d", reloc->r_type());
		}
	}
	else {
		const macho_scattered_relocation_info<P>* sreloc = (macho_scattered_relocation_info<P>*)reloc;
		srcAddr = sect->addr() + sreloc->r_address();
		dstAddr = sreloc->r_value();
		uint32_t betterDstAddr;
		fixUpPtr = (uint32_t*)((char*)(fHeader) + sect->offset() + sreloc->r_address());
		const macho_scattered_relocation_info<P>* nextSReloc = &sreloc[1];
		const macho_relocation_info<P>* nextReloc = &reloc[1];
		// file format allows pair to be scattered or not
		bool nextRelocIsPair = false;
		uint32_t nextRelocAddress = 0;
		uint32_t nextRelocValue = 0;
		if ( (nextReloc->r_address() & R_SCATTERED) == 0 ) {
			if ( nextReloc->r_type() == PPC_RELOC_PAIR ) {
				nextRelocIsPair = true;
				nextRelocAddress = nextReloc->r_address();
				result = true;
			}
		}
		else {
			if ( nextSReloc->r_type() == PPC_RELOC_PAIR ) {
				nextRelocIsPair = true;
				nextRelocAddress = nextSReloc->r_address();
				nextRelocValue = nextSReloc->r_value();
				result = true;
			}
		}
		switch (sreloc->r_type()) {
			case PPC_RELOC_VANILLA:
				{
					betterDstAddr = P::getP(*(pint_t*)fixUpPtr);
					//fprintf(stderr, "scattered pointer reloc: srcAddr=0x%08X, dstAddr=0x%08X, pointer=0x%08X\n", srcAddr, dstAddr, betterDstAddr);
					// with a scattered relocation we get both the target (sreloc->r_value()) and the target+offset (*fixUpPtr)
					makeReferenceWithToBase(A::kPointer, srcAddr, betterDstAddr, dstAddr);
				}
				break;
			case PPC_RELOC_BR14:
				{
					instruction = BigEndian::get32(*fixUpPtr);
					displacement = (instruction & 0x0000FFFC);
					if ( (displacement & 0x00008000) != 0 )
						displacement |= 0xFFFF0000;
					betterDstAddr = srcAddr+displacement;
					//fprintf(stderr, "betterDstAddr=0x%08X, srcAddr=0x%08X, displacement=0x%08X\n",  betterDstAddr, srcAddr, displacement);
					makeReferenceWithToBase(A::kBranch14, srcAddr, betterDstAddr, dstAddr);
				}
				break;
			case PPC_RELOC_BR24:
				{
					instruction = BigEndian::get32(*fixUpPtr);
					if ( (instruction & 0x4C000000) == 0x48000000 ) {
						displacement = (instruction & 0x03FFFFFC);
						if ( (displacement & 0x02000000) != 0 )
							displacement |= 0xFC000000;
						betterDstAddr = srcAddr+displacement;
						makeReferenceWithToBase(A::kBranch24, srcAddr, betterDstAddr, dstAddr);
					}
				}
				break;
			case PPC_RELOC_LO16_SECTDIFF:
				{
					if ( ! nextRelocIsPair ) {
						throw "PPC_RELOC_LO16_SECTDIFF missing following pair";
					}
					instruction = BigEndian::get32(*fixUpPtr);
					lowBits = (instruction & 0xFFFF);
					displacement = (nextRelocAddress << 16) | ((uint32_t)lowBits & 0x0000FFFF);
					makeReferenceWithToBase(A::kPICBaseLow16, srcAddr, nextRelocValue, nextRelocValue + displacement, dstAddr);
				}
				break;
			case PPC_RELOC_LO14_SECTDIFF:
				{
					if ( ! nextRelocIsPair ) {
						throw "PPC_RELOC_LO14_SECTDIFF missing following pair";
					}
					instruction = BigEndian::get32(*fixUpPtr);
					lowBits = (instruction & 0xFFFC);
					displacement = (nextRelocAddress << 16) | ((uint32_t)lowBits & 0x0000FFFF);
					makeReferenceWithToBase(A::kPICBaseLow14, srcAddr, nextRelocValue, nextRelocValue + displacement, dstAddr);
				}
				break;
			case PPC_RELOC_HA16_SECTDIFF:
				{
					if ( ! nextRelocIsPair ) {
						throw "PPC_RELOC_HA16_SECTDIFF missing following pair";
					}
					instruction = BigEndian::get32(*fixUpPtr);
					lowBits = (nextRelocAddress & 0x0000FFFF);
					displacement = ((instruction & 0x0000FFFF) << 16) + (int32_t)lowBits;
					makeReferenceWithToBase(A::kPICBaseHigh16, srcAddr, nextRelocValue, nextRelocValue + displacement, dstAddr);
				}
				break;
			case PPC_RELOC_LO14:
				{
					if ( ! nextRelocIsPair ) {
						throw "PPC_RELOC_LO14 missing following pair";
					}
					instruction = BigEndian::get32(*fixUpPtr);
					lowBits = (instruction & 0xFFFC);
					betterDstAddr = (nextRelocAddress << 16) + ((uint32_t)lowBits & 0x0000FFFF);
					makeReferenceWithToBase(A::kAbsLow14, srcAddr, betterDstAddr, dstAddr);
				}
				break;
			case PPC_RELOC_LO16:
				{
					if ( ! nextRelocIsPair ) {
						throw "PPC_RELOC_LO16 missing following pair";
					}
					instruction = BigEndian::get32(*fixUpPtr);
					lowBits = (instruction & 0xFFFF);
					betterDstAddr = (nextRelocAddress << 16) + ((uint32_t)lowBits & 0x0000FFFF);
					makeReferenceWithToBase(A::kAbsLow16, srcAddr, betterDstAddr, dstAddr);
				}
				break;
			case PPC_RELOC_HA16:
				{
					if ( ! nextRelocIsPair ) {
						throw "PPC_RELOC_HA16 missing following pair";
					}
					instruction = BigEndian::get32(*fixUpPtr);
					lowBits = (nextRelocAddress & 0xFFFF);
					betterDstAddr = ((instruction & 0xFFFF) << 16) + (int32_t)lowBits;
					makeReferenceWithToBase(A::kAbsHigh16AddLow, srcAddr, betterDstAddr, dstAddr);
				}
				break;
			case PPC_RELOC_HI16:
				{
					if ( ! nextRelocIsPair ) {
						throw "PPC_RELOC_HI16 missing following pair";
					}
					instruction = BigEndian::get32(*fixUpPtr);
					lowBits = (nextRelocAddress & 0xFFFF);
					betterDstAddr = ((instruction & 0xFFFF) << 16) | (lowBits & 0x0000FFFF);
					makeReferenceWithToBase(A::kAbsHigh16, srcAddr, betterDstAddr, dstAddr);
				}
				break;
			case PPC_RELOC_SECTDIFF:
			case PPC_RELOC_LOCAL_SECTDIFF:
				{
					if ( ! nextRelocIsPair ) {
						throw "PPC_RELOC_SECTDIFF missing following pair";
					}
					Kinds kind = A::kPointerDiff32;;
					uint32_t contentAddr = 0;
					switch ( sreloc->r_length() ) {
						case 0:
							throw "bad diff relocations r_length (0) for ppc architecture";
						case 1:
							kind = A::kPointerDiff16;
							contentAddr = BigEndian::get16(*((uint16_t*)fixUpPtr));
							break;
						case 2:
							kind = A::kPointerDiff32;
							contentAddr = BigEndian::get32(*fixUpPtr);
							break;
						case 3:
							kind = A::kPointerDiff64;
							contentAddr = BigEndian::get64(*((uint64_t*)fixUpPtr));
							break;
					}
					AtomAndOffset srcao  = findAtomAndOffset(srcAddr);
					AtomAndOffset fromao = findAtomAndOffset(nextRelocValue);
					AtomAndOffset toao   = findAtomAndOffset(dstAddr);
					// check for addend encoded in the section content
					//fprintf(stderr, "addRef: dstAddr=0x%X, nextRelocValue=0x%X, contentAddr=0x%X\n",
					//		dstAddr, nextRelocValue, contentAddr);
					if ( (dstAddr - nextRelocValue) != contentAddr ) {
						if ( toao.atom == srcao.atom )
							toao.offset += (contentAddr + nextRelocValue) - dstAddr;
						else if ( fromao.atom == srcao.atom )
							toao.offset += (contentAddr + nextRelocValue) - dstAddr;
						else
							fromao.offset += (dstAddr - contentAddr) - nextRelocValue;
					}
					//fprintf(stderr, "addRef: src=%s+0x%X, from=%s+0x%X, to=%s+0x%X\n",
					//	srcao.atom->getDisplayName(), srcao.offset, 
					//	fromao.atom->getDisplayName(), fromao.offset, 
					//	toao.atom->getDisplayName(), toao.offset);
					new Reference<A>(kind, srcao, fromao, toao);
				}
				break;
			case PPC_RELOC_PAIR:
				break;
			case PPC_RELOC_HI16_SECTDIFF:
				warning("unexpected scattered relocation type PPC_RELOC_HI16_SECTDIFF");
				break;
			default:
				warning("unknown scattered relocation type %d", sreloc->r_type());
		}
	}
	return result;
}


template <>
bool Reader<x86>::addRelocReference(const macho_section<x86::P>* sect, const macho_relocation_info<x86::P>* reloc)
{
	uint32_t srcAddr;
	uint32_t dstAddr;
	uint32_t* fixUpPtr;
	bool result = false;
	if ( (reloc->r_address() & R_SCATTERED) == 0 ) {
		srcAddr = sect->addr() + reloc->r_address();
		fixUpPtr = (uint32_t*)((char*)(fHeader) + sect->offset() + reloc->r_address());
		switch ( reloc->r_type() ) {
			case GENERIC_RELOC_VANILLA:
				{
					x86::ReferenceKinds kind = x86::kPointer;
					uint32_t pointerValue = E::get32(*fixUpPtr);
					if ( reloc->r_pcrel() ) {
						switch( reloc->r_length() ) {
							case 0:
								kind = x86::kPCRel8;
								pointerValue = srcAddr + *((int8_t*)fixUpPtr) + sizeof(int8_t);
								break;
							case 1:
								kind = x86::kPCRel16;
								pointerValue = srcAddr + (int16_t)E::get16(*((uint16_t*)fixUpPtr)) + sizeof(uint16_t);
								break;
							case 2:
								kind = x86::kPCRel32;
								pointerValue += srcAddr + sizeof(uint32_t);
								break;
							case 3:
								throw "bad pc-rel vanilla relocation length";
						}
					}
					else if ( strcmp(sect->segname(), "__TEXT") == 0 ) {
						kind = x86::kAbsolute32;
						if ( reloc->r_length() != 2 )
							throw "bad vanilla relocation length";
					}
					else {
						kind = x86::kPointer;
						if ( reloc->r_length() != 2 )
							throw "bad vanilla relocation length";
					}
					if ( reloc->r_extern() ) {
						const macho_nlist<P>* targetSymbol = &fSymbols[reloc->r_symbolnum()];
						if ( this->isWeakImportSymbol(targetSymbol) ) {
							if ( reloc->r_pcrel() )
								kind = x86::kPCRel32WeakImport;
							else
								kind = x86::kPointerWeakImport;
						}
						const char* targetName = &fStrings[targetSymbol->n_strx()];
						if ( strncmp(targetName, "___dtrace_probe$", 16) == 0 ) {
							makeByNameReference(x86::kDtraceProbeSite, srcAddr, targetName, 0);
							addDtraceExtraInfos(srcAddr, &targetName[16]);
						}
						else if ( strncmp(targetName, "___dtrace_isenabled$", 20) == 0 ) {
							makeByNameReference(x86::kDtraceIsEnabledSite, srcAddr, targetName, 0);
							addDtraceExtraInfos(srcAddr, &targetName[20]);
						}
						else
							makeByNameReference(kind, srcAddr, targetName, pointerValue);
					}
					else {
						// if this is a branch to a stub, we need to see if the stub is for a weak imported symbol
						ObjectFile::Atom* atom = findAtomAndOffset(pointerValue).atom;
						const char* targetName = atom->getName();
						if ( (targetName != NULL) && (strncmp(targetName, "___dtrace_probe$", 16) == 0) ) {
							makeByNameReference(x86::kDtraceProbeSite, srcAddr, targetName, 0);
							addDtraceExtraInfos(srcAddr, &targetName[16]);
						}
						else if ( (targetName != NULL) && (strncmp(targetName, "___dtrace_isenabled$", 20) == 0) ) {
							makeByNameReference(x86::kDtraceIsEnabledSite, srcAddr, targetName, 0);
							addDtraceExtraInfos(srcAddr, &targetName[20]);
						}
						else if ( reloc->r_pcrel() && (atom->getSymbolTableInclusion() == ObjectFile::Atom::kSymbolTableNotIn)
							&& ((AnonymousAtom<x86>*)atom)->isWeakImportStub() )
							makeReference(x86::kPCRel32WeakImport, srcAddr, pointerValue);
						else if ( reloc->r_symbolnum() != R_ABS )
							makeReference(kind, srcAddr, pointerValue);
						else {
							// find absolute symbol that corresponds to pointerValue
							AddrToAtomMap::iterator pos = fAddrToAbsoluteAtom.find(pointerValue);
							if ( pos != fAddrToAbsoluteAtom.end() ) 
								makeByNameReference(kind, srcAddr, pos->second->getName(), 0);
							else
								throwf("R_ABS reloc but no absolute symbol at target address");
						}
					}
				}
				break;
			default:
				warning("unknown relocation type %d", reloc->r_type());
		}
	}
	else {
		const macho_scattered_relocation_info<P>* sreloc = (macho_scattered_relocation_info<P>*)reloc;
		srcAddr = sect->addr() + sreloc->r_address();
		dstAddr = sreloc->r_value();
		fixUpPtr = (uint32_t*)((char*)(fHeader) + sect->offset() + sreloc->r_address());
		const macho_scattered_relocation_info<P>* nextSReloc = &sreloc[1];
		const macho_relocation_info<P>* nextReloc = &reloc[1];
		pint_t betterDstAddr;
		// file format allows pair to be scattered or not
		bool nextRelocIsPair = false;
		uint32_t nextRelocAddress = 0;
		uint32_t nextRelocValue = 0;
 		if ( (nextReloc->r_address() & R_SCATTERED) == 0 ) {
			if ( nextReloc->r_type() == GENERIC_RELOC_PAIR ) {
				nextRelocIsPair = true;
				nextRelocAddress = nextReloc->r_address();
				result = true;
			}
		}
		else {
			if ( nextSReloc->r_type() == GENERIC_RELOC_PAIR ) {
				nextRelocIsPair = true;
				nextRelocAddress = nextSReloc->r_address();
				nextRelocValue = nextSReloc->r_value();
			}
		}
		switch (sreloc->r_type()) {
			case GENERIC_RELOC_VANILLA:
					betterDstAddr = LittleEndian::get32(*fixUpPtr);
					//fprintf(stderr, "pointer reloc: srcAddr=0x%08X, dstAddr=0x%08X, pointer=0x%08lX\n", srcAddr, dstAddr, betterDstAddr);
					// with a scattered relocation we get both the target (sreloc->r_value()) and the target+offset (*fixUpPtr)
					if ( sreloc->r_pcrel() ) {
						switch ( sreloc->r_length() ) {
							case 2:
								betterDstAddr += srcAddr + 4;
								makeReferenceWithToBase(x86::kPCRel32, srcAddr, betterDstAddr, dstAddr);
								break;
							case 1:
								betterDstAddr = LittleEndian::get16(*((uint16_t*)fixUpPtr)) + srcAddr + 2;
								makeReferenceWithToBase(x86::kPCRel16, srcAddr, betterDstAddr, dstAddr);
								break;
							case 0:
								betterDstAddr = *((uint8_t*)fixUpPtr) + srcAddr + 1;
								makeReferenceWithToBase(x86::kPCRel8, srcAddr, betterDstAddr, dstAddr);
								break;
							case 3:
								throwf("unsupported r_length=3 for scattered pc-rel vanilla reloc");
								break;
						}
					}
					else {
						if ( sreloc->r_length() != 2 )
							throwf("unsupported r_length=%d for scattered vanilla reloc", sreloc->r_length());
						if ( strcmp(sect->segname(), "__TEXT") == 0 )
							makeReferenceWithToBase(x86::kAbsolute32, srcAddr, betterDstAddr, dstAddr);
						else
							makeReferenceWithToBase(x86::kPointer, srcAddr, betterDstAddr, dstAddr);
					}
				break;
			case GENERIC_RELOC_SECTDIFF:
			case GENERIC_RELOC_LOCAL_SECTDIFF:
				{
					if ( !nextRelocIsPair ) {
						throw "GENERIC_RELOC_SECTDIFF missing following pair";
					}
					x86::ReferenceKinds kind = x86::kPointerDiff;
					uint32_t contentAddr = 0;
					switch ( sreloc->r_length() ) {
						case 0:
						case 3:
							throw "bad length for GENERIC_RELOC_SECTDIFF";
						case 1:
							kind = x86::kPointerDiff16;
							contentAddr = LittleEndian::get16(*((uint16_t*)fixUpPtr));
							break;
						case 2:
							kind = x86::kPointerDiff;
							contentAddr = LittleEndian::get32(*fixUpPtr);
							break;
					}
					AtomAndOffset srcao  = findAtomAndOffset(srcAddr);
					AtomAndOffset fromao = findAtomAndOffset(nextRelocValue);
					AtomAndOffset toao   = findAtomAndOffset(dstAddr);
					// check for addend encoded in the section content
					//fprintf(stderr, "addRef: dstAddr=0x%X, nextRelocValue=0x%X, contentAddr=0x%X\n",
					//		dstAddr, nextRelocValue, contentAddr);
					if ( (dstAddr - nextRelocValue) != contentAddr ) {
						if ( toao.atom == srcao.atom )
							toao.offset += (contentAddr + nextRelocValue) - dstAddr;
						else if ( fromao.atom == srcao.atom )
							toao.offset += (contentAddr + nextRelocValue) - dstAddr;
						else
							fromao.offset += (dstAddr - contentAddr) - nextRelocValue;
					}
					//fprintf(stderr, "addRef: src=%s+0x%X, from=%s+0x%X, to=%s+0x%X\n",
					//	srcao.atom->getDisplayName(), srcao.offset, 
					//	fromao.atom->getDisplayName(), fromao.offset, 
					//	toao.atom->getDisplayName(), toao.offset);
					new Reference<x86>(kind, srcao, fromao, toao);
				}
				break;
			case GENERIC_RELOC_PAIR:
				// do nothing, already used via a look ahead
				break;
			default:
				warning("unknown scattered relocation type %d", sreloc->r_type());
		}
	}
	return result;
}

template <>
bool Reader<x86_64>::addRelocReference(const macho_section<x86_64::P>* sect, const macho_relocation_info<x86_64::P>* reloc)
{
	uint64_t srcAddr;
	uint64_t dstAddr = 0;
	uint64_t addend;
	uint32_t* fixUpPtr;
	x86_64::ReferenceKinds kind = x86_64::kNoFixUp;
	bool result = false;
	const macho_nlist<P>* targetSymbol = NULL;
	const char* targetName = NULL;
	srcAddr = sect->addr() + reloc->r_address();
	fixUpPtr = (uint32_t*)((char*)(fHeader) + sect->offset() + reloc->r_address());
	//fprintf(stderr, "addReloc type=%d, len=%d, address=0x%X\n", reloc->r_type(), reloc->r_length(), reloc->r_address());
	if ( reloc->r_extern() ) {
		targetSymbol = &fSymbols[reloc->r_symbolnum()];
		targetName = &fStrings[targetSymbol->n_strx()];
	}
	switch ( reloc->r_type() ) {
		case X86_64_RELOC_UNSIGNED:
			if ( reloc->r_pcrel() )
				throw "pcrel and X86_64_RELOC_UNSIGNED not supported";
			switch ( reloc->r_length() ) {
				case 0:
				case 1:
					throw "length < 2 and X86_64_RELOC_UNSIGNED not supported";
				case 2:
					kind = x86_64::kPointer32;
					break;
				case 3:
					if ( reloc->r_extern() && isWeakImportSymbol(targetSymbol) )
						kind = x86_64::kPointerWeakImport;
					else
						kind = x86_64::kPointer;
					break;
			}
			dstAddr = E::get64(*((uint64_t*)fixUpPtr));
			if ( reloc->r_extern() ) {
				makeReferenceToSymbol(kind, srcAddr, targetSymbol, dstAddr);
			}
			else {
				makeReference(kind, srcAddr, dstAddr);
				// verify that dstAddr is in the section being targeted
				int sectNum = reloc->r_symbolnum();
				const macho_section<P>* const sectionsStart = (macho_section<P>*)((char*)fSegment + sizeof(macho_segment_command<P>));
				const macho_section<P>* const targetSection = &sectionsStart[sectNum-1];
				if ( (dstAddr < targetSection->addr()) || (dstAddr > (targetSection->addr()+targetSection->size())) ) {
					throwf("local relocation for address 0x%08llX in section %s does not target section %s", 
							srcAddr, sect->sectname(), targetSection->sectname());
				}
			}
			break;
		case X86_64_RELOC_SIGNED:
		case X86_64_RELOC_SIGNED_1:
		case X86_64_RELOC_SIGNED_2:
		case X86_64_RELOC_SIGNED_4:
			if ( ! reloc->r_pcrel() )
				throw "not pcrel and X86_64_RELOC_SIGNED* not supported";
			if ( reloc->r_length() != 2 ) 
				throw "length != 2 and X86_64_RELOC_SIGNED* not supported";
			addend = (int64_t)((int32_t)(E::get32(*fixUpPtr)));
			if ( reloc->r_extern() ) {
				switch ( reloc->r_type() ) {
					case X86_64_RELOC_SIGNED:
						kind = x86_64::kPCRel32;
						// begin support for old .o files before X86_64_RELOC_SIGNED_1 was created
						if ( addend == (uint64_t)(-1) ) {
							addend = 0;
							kind = x86_64::kPCRel32_1;
						}
						else if ( addend == (uint64_t)(-2) ) {
							addend = 0;
							kind = x86_64::kPCRel32_2;
						}
						else if ( addend == (uint64_t)(-4) ) {
							addend = 0;
							kind = x86_64::kPCRel32_4;
						}
						break;
						// end support for old .o files before X86_64_RELOC_SIGNED_1 was created
					case X86_64_RELOC_SIGNED_1:
						kind = x86_64::kPCRel32_1;
						addend += 1;
						break;	
					case X86_64_RELOC_SIGNED_2:
						kind = x86_64::kPCRel32_2;
						addend += 2;
						break;	
					case X86_64_RELOC_SIGNED_4:
						kind = x86_64::kPCRel32_4;
						addend += 4;
						break;
				}
				makeReferenceToSymbol(kind, srcAddr, targetSymbol, addend);
			}
			else {
				uint64_t ripRelativeOffset = addend;
				switch ( reloc->r_type() ) {
					case X86_64_RELOC_SIGNED:
						dstAddr = srcAddr + 4 + ripRelativeOffset;
						kind = x86_64::kPCRel32;
						break;
					case X86_64_RELOC_SIGNED_1:
						dstAddr = srcAddr + 5 + ripRelativeOffset;
						kind = x86_64::kPCRel32_1;
						break;	
					case X86_64_RELOC_SIGNED_2:
						dstAddr = srcAddr + 6 + ripRelativeOffset;
						kind = x86_64::kPCRel32_2;
						break;	
					case X86_64_RELOC_SIGNED_4:
						dstAddr = srcAddr + 8 + ripRelativeOffset;
						kind = x86_64::kPCRel32_4;
						break;
				}
				makeReference(kind, srcAddr, dstAddr);
				// verify that dstAddr is in the section being targeted
				int sectNum = reloc->r_symbolnum();
				const macho_section<P>* const sectionsStart = (macho_section<P>*)((char*)fSegment + sizeof(macho_segment_command<P>));
				const macho_section<P>* const targetSection = &sectionsStart[sectNum-1];
				if ( (dstAddr < targetSection->addr()) || (dstAddr > (targetSection->addr()+targetSection->size())) ) {
					throwf("local relocation for address 0x%08llX in section %s does not target section %s", 
							srcAddr, sect->sectname(), targetSection->sectname());
				}
			}	
			break;
		case X86_64_RELOC_BRANCH:
			if ( ! reloc->r_pcrel() )
				throw "not pcrel and X86_64_RELOC_BRANCH not supported";
			if ( reloc->r_length() == 2 ) {
				dstAddr = (int64_t)((int32_t)(E::get32(*fixUpPtr)));
				if ( reloc->r_extern() ) {
					if ( strncmp(targetName, "___dtrace_probe$", 16) == 0 ) {
						makeByNameReference(x86_64::kDtraceProbeSite, srcAddr, targetName, 0);
						addDtraceExtraInfos(srcAddr, &targetName[16]);
					}
					else if ( strncmp(targetName, "___dtrace_isenabled$", 20) == 0 ) {
						makeByNameReference(x86_64::kDtraceIsEnabledSite, srcAddr, targetName, 0);
						addDtraceExtraInfos(srcAddr, &targetName[16]);
					}
					else if ( isWeakImportSymbol(targetSymbol) )
						makeReferenceToSymbol(x86_64::kBranchPCRel32WeakImport, srcAddr, targetSymbol, dstAddr);
					else
						makeReferenceToSymbol(x86_64::kBranchPCRel32, srcAddr, targetSymbol, dstAddr);
				}
				else {
					makeReference(x86_64::kBranchPCRel32, srcAddr, srcAddr+4+dstAddr);
				}
			}
			else if ( reloc->r_length() == 0 ) {
				dstAddr = *((int8_t*)fixUpPtr);
				if ( reloc->r_extern() ) {
					makeReferenceToSymbol(x86_64::kBranchPCRel8, srcAddr, targetSymbol, dstAddr);
				}
				else {
					makeReference(x86_64::kBranchPCRel8, srcAddr, srcAddr+1+dstAddr);
				}
			}
			else {
				throwf("length=%d and X86_64_RELOC_BRANCH not supported", reloc->r_length());;
			}
			break;
		case X86_64_RELOC_GOT:
			if ( ! reloc->r_extern() ) 
				throw "not extern and X86_64_RELOC_GOT not supported";
			if ( ! reloc->r_pcrel() )
				throw "not pcrel and X86_64_RELOC_GOT not supported";
			if ( reloc->r_length() != 2 ) 
				throw "length != 2 and X86_64_RELOC_GOT not supported";
			addend = (int64_t)((int32_t)(E::get32(*fixUpPtr)));
			if ( isWeakImportSymbol(targetSymbol) )
				makeReferenceToSymbol(x86_64::kPCRel32GOTWeakImport, srcAddr, targetSymbol, addend);
			else
				makeReferenceToSymbol(x86_64::kPCRel32GOT, srcAddr, targetSymbol, addend);
			break;
		case X86_64_RELOC_GOT_LOAD:
			if ( ! reloc->r_extern() ) 
				throw "not extern and X86_64_RELOC_GOT_LOAD not supported";
			if ( ! reloc->r_pcrel() )
				throw "not pcrel and X86_64_RELOC_GOT_LOAD not supported";
			if ( reloc->r_length() != 2 ) 
				throw "length != 2 and X86_64_RELOC_GOT_LOAD not supported";
			addend = (int64_t)((int32_t)(E::get32(*fixUpPtr)));
			if ( isWeakImportSymbol(targetSymbol) )
				makeReferenceToSymbol(x86_64::kPCRel32GOTLoadWeakImport, srcAddr, targetSymbol, addend);
			else
				makeReferenceToSymbol(x86_64::kPCRel32GOTLoad, srcAddr, targetSymbol, addend);
			break;
		case X86_64_RELOC_SUBTRACTOR:
		{
			if ( reloc->r_pcrel() )
				throw "X86_64_RELOC_SUBTRACTOR cannot be pc-relative";
			if ( reloc->r_length() < 2 )
				throw "X86_64_RELOC_SUBTRACTOR must have r_length of 2 or 3";
			if ( !reloc->r_extern() )
				throw "X86_64_RELOC_SUBTRACTOR must have r_extern=1";
			const macho_relocation_info<x86_64::P>* nextReloc = &reloc[1];
			if ( nextReloc->r_type() != X86_64_RELOC_UNSIGNED )
				throw "X86_64_RELOC_SUBTRACTOR must be followed by X86_64_RELOC_UNSIGNED";
			result = true;
			if ( nextReloc->r_pcrel() )
				throw "X86_64_RELOC_UNSIGNED following a X86_64_RELOC_SUBTRACTOR cannot be pc-relative";
			if ( nextReloc->r_length() != reloc->r_length() )
				throw "X86_64_RELOC_UNSIGNED following a X86_64_RELOC_SUBTRACTOR must have same r_length";
			Reference<x86_64>* ref;
			bool negativeAddend;
			if ( reloc->r_length() == 2 ) {
				kind = x86_64::kPointerDiff32;
				dstAddr = E::get32(*fixUpPtr); // addend is in content
				negativeAddend = ((dstAddr & 0x80000000) != 0);
			}
			else {
				kind = x86_64::kPointerDiff;
				dstAddr = E::get64(*((uint64_t*)fixUpPtr)); // addend is in content
				negativeAddend = ((dstAddr & 0x8000000000000000ULL) != 0);
			}
			AtomAndOffset inAtomAndOffset = this->findAtomAndOffset(srcAddr);
			ObjectFile::Atom* inAtom = inAtomAndOffset.atom;
			// create reference with "to" target
			if ( nextReloc->r_extern() ) {
				const macho_nlist<P>* targetSymbol = &fSymbols[nextReloc->r_symbolnum()];
				const char* targetName = &fStrings[targetSymbol->n_strx()];
				ref = makeReferenceToSymbol(kind, srcAddr, targetSymbol, 0);
				// if "to" is in this atom, change by-name to a direct reference
				if ( strcmp(targetName, inAtom->getName()) == 0 )
					ref->setTarget(*inAtom, 0);
			}
			else {
				ref = makeReference(kind, srcAddr, dstAddr);
			}
			// add in "from" target
			if ( reloc->r_extern() ) {
				const macho_nlist<P>* targetFromSymbol = &fSymbols[reloc->r_symbolnum()];
				const char* fromTargetName = &fStrings[targetFromSymbol->n_strx()];
				if ( (targetFromSymbol->n_type() & N_EXT) == 0 ) {
					// from target is translation unit scoped, so use a direct reference
					ref->setFromTarget(*(findAtomAndOffset(targetSymbol->n_value()).atom));
				}
				else if ( strcmp(fromTargetName, inAtom->getName()) == 0 ) {
					// if "from" is in this atom, change by-name to a direct reference
					ref->setFromTarget(*inAtom);
				}
				else {
					// some non-static other atom
					ref->setFromTargetName(fromTargetName);
				}
			}
			else {
				throw "X86_64_RELOC_SUBTRACTOR not supported with r_extern=0";
			}
			// addend goes in from side iff negative
			if ( negativeAddend )
				ref->setFromTargetOffset(-dstAddr);
			else
				ref->setToTargetOffset(dstAddr);
			break;
		}
		default:
			warning("unknown relocation type %d", reloc->r_type());
	}
	return result;
}


/// Reader<arm>::addRelocReference - 
/// turns arm relocation entries into references.  Returns true if the next
/// relocation should be skipped, false otherwise.
template <>
bool Reader<arm>::addRelocReference(const macho_section<arm::P>* sect, 
                                    const macho_relocation_info<arm::P>* reloc)
{
	uint32_t *  fixUpPtr;
	int32_t		displacement;												
	uint32_t    instruction = 0;
	bool        result = false;
	uint32_t	srcAddr;
	uint32_t	dstAddr;
	uint32_t	pointerValue;
	arm::ReferenceKinds kind = arm::kNoFixUp;
	
	if ( (reloc->r_address() & R_SCATTERED) == 0 ) {
		// non-scattered relocation
		const char* targetName = NULL;
		bool        weakImport = false;
    
		srcAddr = sect->addr() + reloc->r_address();
		fixUpPtr = (uint32_t*)((char*)(fHeader) + sect->offset() + reloc->r_address());
		if ( reloc->r_type() != ARM_RELOC_PAIR )
			instruction = LittleEndian::get32(*fixUpPtr);
    
		if ( reloc->r_extern() ) {
			const macho_nlist<P>* targetSymbol = &fSymbols[reloc->r_symbolnum()];
			targetName = &fStrings[targetSymbol->n_strx()];
			weakImport = this->isWeakImportSymbol(targetSymbol);
		}
    
		switch ( reloc->r_type() ) {
			case ARM_RELOC_BR24:
				// Sign-extend displacement
				displacement = (instruction & 0x00FFFFFF) << 2;
				if ( (displacement & 0x02000000) != 0 )
					displacement |= 0xFC000000;
				// The pc added will be +8 from the pc
				displacement += 8;
				// If this is BLX add H << 1
				if ((instruction & 0xFE000000) == 0xFA000000)
					displacement += ((instruction & 0x01000000) >> 23);

				if ( reloc->r_extern() ) {
					uint32_t offsetInTarget = srcAddr + displacement;
					if ( strncmp(targetName, "___dtrace_probe$", 16) == 0 ) {
						makeByNameReference(arm::kDtraceProbeSite, srcAddr, targetName, 0);
						addDtraceExtraInfos(srcAddr, &targetName[16]);
					}
					else if ( strncmp(targetName, "___dtrace_isenabled$", 20) == 0 ) {
						makeByNameReference(arm::kDtraceIsEnabledSite, srcAddr, targetName, 0);
						addDtraceExtraInfos(srcAddr, &targetName[20]);
					}
					else if ( weakImport )
						makeByNameReference(arm::kBranch24WeakImport, srcAddr, targetName, offsetInTarget);
					else
						makeByNameReference(arm::kBranch24, srcAddr, targetName, offsetInTarget);
				}
				else {
					dstAddr = srcAddr + displacement;
					ObjectFile::Atom* atom = findAtomAndOffset(dstAddr).atom;
					// check for dtrace probes and weak_import stubs 
					const char* targetName = atom->getName();
					if ( (targetName != NULL) && (strncmp(targetName, "___dtrace_probe$", 16) == 0) ) {
						makeByNameReference(arm::kDtraceProbeSite, srcAddr, targetName, 0);
						addDtraceExtraInfos(srcAddr, &targetName[16]);
					}
					else if ( (targetName != NULL) && (strncmp(targetName, "___dtrace_isenabled$", 20) == 0) ) {
						makeByNameReference(arm::kDtraceIsEnabledSite, srcAddr, targetName, 0);
						addDtraceExtraInfos(srcAddr, &targetName[20]);
					}
					else if ( (atom->getSymbolTableInclusion() == ObjectFile::Atom::kSymbolTableNotIn)
						&& ((AnonymousAtom<x86>*)atom)->isWeakImportStub() )
						makeReference(arm::kBranch24WeakImport, srcAddr, dstAddr);
					else if ( reloc->r_symbolnum() != R_ABS )
						makeReference(arm::kBranch24, srcAddr, dstAddr);
					else {
						// find absolute symbol that corresponds to pointerValue
						AddrToAtomMap::iterator pos = fAddrToAbsoluteAtom.find(dstAddr);
						if ( pos != fAddrToAbsoluteAtom.end() ) 
							makeByNameReference(arm::kBranch24, srcAddr, pos->second->getName(), 0);
						else
							throwf("R_ABS reloc but no absolute symbol at target address");
					}
				}
				break;
	
			case ARM_THUMB_RELOC_BR22:
				// thumb2 added two more bits to displacement, complicating the displacement decoding
				{
					uint32_t s = (instruction >> 10) & 0x1;
					uint32_t j1 = (instruction >> 29) & 0x1;
					uint32_t j2 = (instruction >> 27) & 0x1;
					uint32_t imm10 = instruction & 0x3FF;
					uint32_t imm11 = (instruction >> 16) & 0x7FF;
					uint32_t i1 = (j1 == s);
					uint32_t i2 = (j2 == s);
					uint32_t dis = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1);
					int32_t sdis = dis;
					if ( s )
						sdis |= 0xFE000000;
					displacement = sdis;
				}
				// The pc added will be +4 from the pc
				displacement += 4;
				// If the instruction was blx, force the low 2 bits to be clear
				dstAddr = srcAddr + displacement;
				if ((instruction & 0xF8000000) == 0xE8000000)
					dstAddr &= 0xFFFFFFFC;

				if ( reloc->r_extern() ) {
					uint32_t offsetInTarget = dstAddr;
					if ( strncmp(targetName, "___dtrace_probe$", 16) == 0 ) {
						makeByNameReference(arm::kDtraceProbeSite, srcAddr, targetName, 0);
						addDtraceExtraInfos(srcAddr, &targetName[16]);
					}
					else if ( strncmp(targetName, "___dtrace_isenabled$", 20) == 0 ) {
						makeByNameReference(arm::kDtraceIsEnabledSite, srcAddr, targetName, 0);
						addDtraceExtraInfos(srcAddr, &targetName[20]);
					}
					else if ( weakImport )
						makeByNameReference(arm::kThumbBranch22WeakImport, srcAddr, targetName, offsetInTarget);
					else
						makeByNameReference(arm::kThumbBranch22, srcAddr, targetName, offsetInTarget);
				}
				else {
					ObjectFile::Atom* atom = findAtomAndOffset(dstAddr).atom;
					// check for dtrace probes and weak_import stubs 
					const char* targetName = atom->getName();
					if ( (targetName != NULL) && (strncmp(targetName, "___dtrace_probe$", 16) == 0) ) {
						makeByNameReference(arm::kDtraceProbeSite, srcAddr, targetName, 0);
						addDtraceExtraInfos(srcAddr, &targetName[16]);
					}
					else if ( (targetName != NULL) && (strncmp(targetName, "___dtrace_isenabled$", 20) == 0) ) {
						makeByNameReference(arm::kDtraceIsEnabledSite, srcAddr, targetName, 0);
						addDtraceExtraInfos(srcAddr, &targetName[20]);
					}
					else if ( (atom->getSymbolTableInclusion() == ObjectFile::Atom::kSymbolTableNotIn)
						&& ((AnonymousAtom<x86>*)atom)->isWeakImportStub() )
						makeReference(arm::kThumbBranch22WeakImport, srcAddr, dstAddr);
					else if ( reloc->r_symbolnum() != R_ABS )
						makeReference(arm::kThumbBranch22, srcAddr, dstAddr);
					else {
						// find absolute symbol that corresponds to pointerValue
						AddrToAtomMap::iterator pos = fAddrToAbsoluteAtom.find(dstAddr);
						if ( pos != fAddrToAbsoluteAtom.end() ) 
							makeByNameReference(arm::kThumbBranch22, srcAddr, pos->second->getName(), 0);
						else
							throwf("R_ABS reloc but no absolute symbol at target address");
					}
				}
				break;

			case ARM_RELOC_VANILLA:
				if ( reloc->r_length() != 2 )
					throw "bad length for ARM_RELOC_VANILLA";

				pointerValue = instruction;
				kind = arm::kPointer;
				if ( strcmp(sect->segname(), "__TEXT") == 0 )
					kind = arm::kReadOnlyPointer;
				if ( weakImport )
					kind = arm::kPointerWeakImport;
				if ( reloc->r_extern() ) {
					makeByNameReference(kind, srcAddr, targetName, pointerValue);
				}
				else {
					AtomAndOffset at = findAtomAndOffset(srcAddr);
					AtomAndOffset to = findAtomAndOffset(pointerValue);
					if ( to.atom->isThumb() )
						to.offset &= -2;
					new Reference<arm>(kind, at, to);
				}
				break;
				
			case ARM_THUMB_32BIT_BRANCH:
				// work around for <rdar://problem/6489480>
				break;
				
			default:
				warning("unexpected relocation type %u", reloc->r_type());
				break;
		}
	} 
	else {
		const macho_scattered_relocation_info<P>* sreloc = (macho_scattered_relocation_info<P>*)reloc;
		const macho_scattered_relocation_info<P>* nextSReloc = &sreloc[1];
		srcAddr = sect->addr() + sreloc->r_address();
		dstAddr = sreloc->r_value();
		uint32_t betterDstAddr;
		fixUpPtr = (uint32_t*)((char*)(fHeader) + sect->offset() + sreloc->r_address());
		instruction = LittleEndian::get32(*fixUpPtr);
		
		// A ARM_RELOC_PAIR only follows ARM_RELOC_{SECTDIFF,LOCAL_SECTDIFF}
		// relocation types, and it is an error to see one otherwise.
		bool nextRelocIsPair = false;
		uint32_t nextRelocAddress = 0;
		uint32_t nextRelocValue = 0;
		if ( nextSReloc->r_type() == ARM_RELOC_PAIR ) {
			nextRelocIsPair = true;
			nextRelocAddress = nextSReloc->r_address();
			nextRelocValue = nextSReloc->r_value();
			result = true;
		}
		
		switch (sreloc->r_type()) {
			case ARM_RELOC_VANILLA:
				if ( sreloc->r_length() != 2 )
					throw "bad length for ARM_RELOC_VANILLA";

				//fprintf(stderr, "scattered pointer reloc: srcAddr=0x%08X, dstAddr=0x%08X, pointer=0x%08X\n", srcAddr, dstAddr, betterDstAddr);
				betterDstAddr = LittleEndian::get32(*fixUpPtr);
				kind = arm::kPointer;
				if ( strcmp(sect->segname(), "__TEXT") == 0 )
					kind = arm::kReadOnlyPointer;
				// with a scattered relocation we get both the target (sreloc->r_value()) and the target+offset (*fixUpPtr)
				makeReferenceWithToBase(kind, srcAddr, betterDstAddr, dstAddr);
				break;
		
			case ARM_RELOC_BR24:
				// Sign-extend displacement
				displacement = (instruction & 0x00FFFFFF) << 2;
				if ( (displacement & 0x02000000) != 0 )
					displacement |= 0xFC000000;
				// The pc added will be +8 from the pc
				displacement += 8;
				// If this is BLX add H << 1
				if ((instruction & 0xFE000000) == 0xFA000000)
					displacement += ((instruction & 0x01000000) >> 23);
				betterDstAddr = srcAddr+displacement;
				makeReferenceWithToBase(arm::kBranch24, srcAddr, betterDstAddr, dstAddr);
				break;
		
			case ARM_THUMB_RELOC_BR22:
				// thumb2 added two more bits to displacement, complicating the displacement decoding
				{
					uint32_t s = (instruction >> 10) & 0x1;
					uint32_t j1 = (instruction >> 29) & 0x1;
					uint32_t j2 = (instruction >> 27) & 0x1;
					uint32_t imm10 = instruction & 0x3FF;
					uint32_t imm11 = (instruction >> 16) & 0x7FF;
					uint32_t i1 = (j1 == s);
					uint32_t i2 = (j2 == s);
					uint32_t dis = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1);
					int32_t sdis = dis;
					if ( s )
						sdis |= 0xFE000000;
					displacement = sdis;
				}
				// The pc added will be +4 from the pc
				displacement += 4;
				betterDstAddr = srcAddr+displacement;
				// If the instruction was blx, force the low 2 bits to be clear
				if ((instruction & 0xF8000000) == 0xE8000000)
					betterDstAddr &= 0xFFFFFFFC;
				makeReferenceWithToBase(arm::kThumbBranch22, srcAddr, betterDstAddr, dstAddr);
				break;
		
			case ARM_RELOC_SECTDIFF:
			case ARM_RELOC_LOCAL_SECTDIFF:
				if ( !nextRelocIsPair ) {
					throw "ARM_RELOC_SECTDIFF missing following pair";
				}
				if ( sreloc->r_length() != 2 )
					throw "bad length for ARM_RELOC_SECTDIFF";
				{
				AtomAndOffset srcao  = findAtomAndOffset(srcAddr);
				AtomAndOffset fromao = findAtomAndOffset(nextRelocValue);
				AtomAndOffset toao   = findAtomAndOffset(dstAddr);
				// check for addend encoded in the section content
				pointerValue = LittleEndian::get32(*fixUpPtr);
				if ( (dstAddr - nextRelocValue) != pointerValue ) {
					if ( toao.atom == srcao.atom )
						toao.offset += (pointerValue + nextRelocValue) - dstAddr;
					else if ( fromao.atom == srcao.atom )
						toao.offset += (pointerValue + nextRelocValue) - dstAddr;
					else
						fromao.offset += (dstAddr - pointerValue) - nextRelocValue;
				}
				new Reference<arm>(arm::kPointerDiff, srcao, fromao, toao);
				}
				break;
			
			default:
				warning("unexpected srelocation type %u", sreloc->r_type());
				break;
		}
	}
	return result;
}

template <typename A>
void Reader<A>::addReferencesForSection(const macho_section<P>* sect)
{
	// ignore dwarf sections.  If ld ever supports processing dwarf, this logic will need to change
	if ( (sect->flags() & S_ATTR_DEBUG) == 0 ) {
		switch ( sect->flags() & SECTION_TYPE ) {
			case S_SYMBOL_STUBS:
			case S_LAZY_SYMBOL_POINTERS:
				// we ignore compiler generated stubs, so ignore those relocs too
				break;
			default:
				// ignore all relocations in __eh_frame section
				if ( sect == fehFrameSection )
					return;
				const macho_relocation_info<P>* relocs = (macho_relocation_info<P>*)((char*)(fHeader) + sect->reloff());
				const uint32_t relocCount = sect->nreloc();
				//fprintf(stderr, "relocCount = %d in section %s\n", relocCount, sect->sectname());
				for (uint32_t r = 0; r < relocCount; ++r) {
					try {
						if ( addRelocReference(sect, &relocs[r]) )
							++r; // skip next
					}
					catch (const char* msg) {
						throwf("in section %s,%s reloc %u: %s", sect->segname(), sect->sectname(), r, msg);
					}
				}
		}
	}
}


template <>
const char* Reference<x86>::getDescription() const
{
	static char temp[2048];
	switch( fKind ) {
		case x86::kNoFixUp:
			sprintf(temp, "reference to ");
			break;
		case x86::kFollowOn:
			sprintf(temp, "followed by ");
			break;
		case x86::kGroupSubordinate:
			sprintf(temp, "group subordinate ");
			break;
		case x86::kPointerWeakImport:
			sprintf(temp, "offset 0x%04X, weak import pointer to ", fFixUpOffsetInSrc);
			break;
		case x86::kPointer:
			sprintf(temp, "offset 0x%04X, pointer to ", fFixUpOffsetInSrc);
			break;
		case x86::kPointerDiff:
			{
			// by-name references have quoted names
			const char* targetQuotes = (&(this->getTarget()) == NULL) ? "\"" : "";
			const char* fromQuotes = (&(this->getFromTarget()) == NULL) ? "\"" : "";
			sprintf(temp, "offset 0x%04X, 32-bit pointer difference: (&%s%s%s + 0x%08X) - (&%s%s%s + 0x%08X)",
				fFixUpOffsetInSrc, targetQuotes, this->getTargetDisplayName(), targetQuotes, fToTarget.offset,
							   fromQuotes, this->getFromTargetDisplayName(), fromQuotes, fFromTarget.offset );
			return temp;
			}
			break;
		case x86::kPointerDiff16:
			{
			// by-name references have quoted names
			const char* targetQuotes = (&(this->getTarget()) == NULL) ? "\"" : "";
			const char* fromQuotes = (&(this->getFromTarget()) == NULL) ? "\"" : "";
			sprintf(temp, "offset 0x%04X, 16-bit pointer difference: (&%s%s%s + 0x%08X) - (&%s%s%s + 0x%08X)",
				fFixUpOffsetInSrc, targetQuotes, this->getTargetDisplayName(), targetQuotes, fToTarget.offset,
							   fromQuotes, this->getFromTargetDisplayName(), fromQuotes, fFromTarget.offset );
			return temp;
			}
			break;
		case x86::kPCRel32WeakImport:
			sprintf(temp, "offset 0x%04X, rel32 reference to weak imported ", fFixUpOffsetInSrc);
			break;
		case x86::kPCRel32:
			sprintf(temp, "offset 0x%04X, rel32 reference to ", fFixUpOffsetInSrc);
			break;
		case x86::kPCRel16:
			sprintf(temp, "offset 0x%04X, rel16 reference to ", fFixUpOffsetInSrc);
			break;
		case x86::kPCRel8:
			sprintf(temp, "offset 0x%04X, rel8 reference to ", fFixUpOffsetInSrc);
			break;
		case x86::kAbsolute32:
			sprintf(temp, "offset 0x%04X, absolute32 reference to ", fFixUpOffsetInSrc);
			break;
		case x86::kImageOffset32:
			sprintf(temp, "offset 0x%04X, 32-bit offset of ", fFixUpOffsetInSrc);
			break;
		case x86::kPointerDiff24:
			sprintf(temp, "offset 0x%04X, 24-bit pointer difference: (&%s + 0x%08X) - (&%s + 0x%08X)",
				fFixUpOffsetInSrc, this->getTargetDisplayName(), fToTarget.offset,
							   this->getFromTargetDisplayName(), fFromTarget.offset );
			return temp;
			break;
		case x86::kSectionOffset24:
			sprintf(temp, "offset 0x%04X, 24-bit section offset of ", fFixUpOffsetInSrc);
			break;
		case x86::kDtraceProbe:
			sprintf(temp, "offset 0x%04X, dtrace static probe ", fFixUpOffsetInSrc);
			break;
		case x86::kDtraceProbeSite:
			sprintf(temp, "offset 0x%04X, dtrace static probe site", fFixUpOffsetInSrc);
			break;
		case x86::kDtraceIsEnabledSite:
			sprintf(temp, "offset 0x%04X, dtrace static probe is-enabled site", fFixUpOffsetInSrc);
			break;
		case x86::kDtraceTypeReference:
			sprintf(temp, "offset 0x%04X, dtrace type/stability reference", fFixUpOffsetInSrc);
			break;
	}
	// always quote by-name references
	if ( fToTargetName != NULL ) {
		strcat(temp, "\"");
		strcat(temp, fToTargetName);
		strcat(temp, "\"");
	}
	else if ( fToTarget.atom != NULL ) {
		strcat(temp, fToTarget.atom->getDisplayName());
	}
	else {
		strcat(temp, "NULL target");
	}
	if ( fToTarget.offset != 0 )
		sprintf(&temp[strlen(temp)], " plus 0x%08X", fToTarget.offset);

	return temp;
}


template <>
const char* Reference<ppc>::getDescription() const
{
	static char temp[2048];
	switch( fKind ) {
		case ppc::kNoFixUp:
			sprintf(temp, "reference to ");
			break;
		case ppc::kFollowOn:
			sprintf(temp, "followed by ");
			break;
		case ppc::kGroupSubordinate:
			sprintf(temp, "group subordinate ");
			break;
		case ppc::kPointerWeakImport:
			sprintf(temp, "offset 0x%04X, weak import pointer to ", fFixUpOffsetInSrc);
			break;
		case ppc::kPointer:
			sprintf(temp, "offset 0x%04X, pointer to ", fFixUpOffsetInSrc);
			break;
		case ppc::kPointerDiff16:
			{
			// by-name references have quoted names
			const char* targetQuotes = (&(this->getTarget()) == NULL) ? "\"" : "";
			const char* fromQuotes = (&(this->getFromTarget()) == NULL) ? "\"" : "";
			sprintf(temp, "offset 0x%04X, 16-bit pointer difference: (&%s%s%s + %d) - (&%s%s%s + %d)",
				fFixUpOffsetInSrc, targetQuotes, this->getTargetDisplayName(), targetQuotes, fToTarget.offset,
							   fromQuotes, this->getFromTargetDisplayName(), fromQuotes, fFromTarget.offset );
			return temp;
			}
		case ppc::kPointerDiff32:
			{
			// by-name references have quoted names
			const char* targetQuotes = (&(this->getTarget()) == NULL) ? "\"" : "";
			const char* fromQuotes = (&(this->getFromTarget()) == NULL) ? "\"" : "";
			sprintf(temp, "offset 0x%04X, 32-bit pointer difference: (&%s%s%s + %d) - (&%s%s%s + %d)",
				fFixUpOffsetInSrc, targetQuotes, this->getTargetDisplayName(), targetQuotes, fToTarget.offset,
							   fromQuotes, this->getFromTargetDisplayName(), fromQuotes, fFromTarget.offset );
			return temp;
			}
		case ppc::kPointerDiff64:
			throw "unsupported refrence kind";
			break;
		case ppc::kBranch24WeakImport:
			sprintf(temp, "offset 0x%04X, pc-rel branch fixup to weak imported ", fFixUpOffsetInSrc);
			break;
		case ppc::kBranch24:
		case ppc::kBranch14:
			sprintf(temp, "offset 0x%04X, pc-rel branch fixup to ", fFixUpOffsetInSrc);
			break;
		case ppc::kPICBaseLow16:
			sprintf(temp, "offset 0x%04X, low  16 fixup from pic-base of %s plus 0x%04X to ", fFixUpOffsetInSrc, fFromTarget.atom->getDisplayName(), fFromTarget.offset);
			break;
		case ppc::kPICBaseLow14:
			sprintf(temp, "offset 0x%04X, low  14 fixup from pic-base of %s plus 0x%04X to ", fFixUpOffsetInSrc, fFromTarget.atom->getDisplayName(), fFromTarget.offset);
			break;
		case ppc::kPICBaseHigh16:
			sprintf(temp, "offset 0x%04X, high 16 fixup from pic-base of %s plus 0x%04X to ", fFixUpOffsetInSrc, fFromTarget.atom->getDisplayName(), fFromTarget.offset);
			break;
		case ppc::kAbsLow16:
			sprintf(temp, "offset 0x%04X, low  16 fixup to absolute address of ", fFixUpOffsetInSrc);
			break;
		case ppc::kAbsLow14:
			sprintf(temp, "offset 0x%04X, low  14 fixup to absolute address of ", fFixUpOffsetInSrc);
			break;
		case ppc::kAbsHigh16:
			sprintf(temp, "offset 0x%04X, high 16 fixup or to absolute address of ", fFixUpOffsetInSrc);
			break;
		case ppc::kAbsHigh16AddLow:
			sprintf(temp, "offset 0x%04X, high 16 fixup add to absolute address of ", fFixUpOffsetInSrc);
			break;
		case ppc::kDtraceProbe:
			sprintf(temp, "offset 0x%04X, dtrace static probe ", fFixUpOffsetInSrc);
			break;
		case ppc::kDtraceProbeSite:
			sprintf(temp, "offset 0x%04X, dtrace static probe site", fFixUpOffsetInSrc);
			break;
		case ppc::kDtraceIsEnabledSite:
			sprintf(temp, "offset 0x%04X, dtrace static probe is-enabled site", fFixUpOffsetInSrc);
			break;
		case ppc::kDtraceTypeReference:
			sprintf(temp, "offset 0x%04X, dtrace type/stability reference", fFixUpOffsetInSrc);
			break;
	}
	// always quote by-name references
	if ( fToTargetName != NULL ) {
		strcat(temp, "\"");
		strcat(temp, fToTargetName);
		strcat(temp, "\"");
	}
	else if ( fToTarget.atom != NULL ) {
		strcat(temp, fToTarget.atom->getDisplayName());
	}
	else {
		strcat(temp, "NULL target");
	}
	if ( fToTarget.offset != 0 )
		sprintf(&temp[strlen(temp)], " plus 0x%08X", fToTarget.offset);

	return temp;
}

template <>
const char* Reference<ppc64>::getDescription() const
{
	static char temp[2048];
	switch( fKind ) {
		case ppc64::kNoFixUp:
			sprintf(temp, "reference to ");
			break;
		case ppc64::kFollowOn:
			sprintf(temp, "followed by ");
			break;
		case ppc64::kGroupSubordinate:
			sprintf(temp, "group subordinate ");
			break;
		case ppc64::kPointerWeakImport:
			sprintf(temp, "offset 0x%04llX, weak import pointer to ", fFixUpOffsetInSrc);
			break;
		case ppc64::kPointer:
			sprintf(temp, "offset 0x%04llX, pointer to ", fFixUpOffsetInSrc);
			break;
		case ppc64::kPointerDiff64:
			{
			// by-name references have quoted names
			const char* targetQuotes = (&(this->getTarget()) == NULL) ? "\"" : "";
			const char* fromQuotes = (&(this->getFromTarget()) == NULL) ? "\"" : "";
			sprintf(temp, "offset 0x%04llX, 64-bit pointer difference: (&%s%s%s + %u) - (&%s%s%s + %u)",
				fFixUpOffsetInSrc, targetQuotes, this->getTargetDisplayName(), targetQuotes, fToTarget.offset,
							   fromQuotes, this->getFromTargetDisplayName(), fromQuotes, fFromTarget.offset );
			return temp;
			}
		case ppc64::kPointerDiff32:
			{
			// by-name references have quoted names
			const char* targetQuotes = (&(this->getTarget()) == NULL) ? "\"" : "";
			const char* fromQuotes = (&(this->getFromTarget()) == NULL) ? "\"" : "";
			sprintf(temp, "offset 0x%04llX, 32-bit pointer difference: (&%s%s%s + %u) - (&%s%s%s + %u)",
				fFixUpOffsetInSrc, targetQuotes, this->getTargetDisplayName(), targetQuotes, fToTarget.offset,
							   fromQuotes, this->getFromTargetDisplayName(), fromQuotes, fFromTarget.offset );
			return temp;
			}
		case ppc64::kPointerDiff16:
			{
			// by-name references have quoted names
			const char* targetQuotes = (&(this->getTarget()) == NULL) ? "\"" : "";
			const char* fromQuotes = (&(this->getFromTarget()) == NULL) ? "\"" : "";
			sprintf(temp, "offset 0x%04llX, 16-bit pointer difference: (&%s%s%s + %u) - (&%s%s%s + %u)",
				fFixUpOffsetInSrc, targetQuotes, this->getTargetDisplayName(), targetQuotes, fToTarget.offset,
							   fromQuotes, this->getFromTargetDisplayName(), fromQuotes, fFromTarget.offset );
			return temp;
			}
		case ppc64::kBranch24WeakImport:
			sprintf(temp, "offset 0x%04llX, pc-rel branch fixup to weak imported ", fFixUpOffsetInSrc);
			break;
		case ppc64::kBranch24:
		case ppc64::kBranch14:
			sprintf(temp, "offset 0x%04llX, pc-rel branch fixup to ", fFixUpOffsetInSrc);
			break;
		case ppc64::kPICBaseLow16:
			sprintf(temp, "offset 0x%04llX, low  16 fixup from pic-base offset 0x%04X to ", fFixUpOffsetInSrc, fFromTarget.offset);
			break;
		case ppc64::kPICBaseLow14:
			sprintf(temp, "offset 0x%04llX, low  14 fixup from pic-base offset 0x%04X to ", fFixUpOffsetInSrc, fFromTarget.offset);
			break;
		case ppc64::kPICBaseHigh16:
			sprintf(temp, "offset 0x%04llX, high 16 fixup from pic-base offset 0x%04X to ", fFixUpOffsetInSrc, fFromTarget.offset);
			break;
		case ppc64::kAbsLow16:
			sprintf(temp, "offset 0x%04llX, low  16 fixup to absolute address of ", fFixUpOffsetInSrc);
			break;
		case ppc64::kAbsLow14:
			sprintf(temp, "offset 0x%04llX, low  14 fixup to absolute address of ", fFixUpOffsetInSrc);
			break;
		case ppc64::kAbsHigh16:
			sprintf(temp, "offset 0x%04llX, high 16 fixup or to absolute address of ", fFixUpOffsetInSrc);
			break;
		case ppc64::kAbsHigh16AddLow:
			sprintf(temp, "offset 0x%04llX, high 16 fixup add to absolute address of ", fFixUpOffsetInSrc);
			break;
		case ppc64::kDtraceProbe:
			sprintf(temp, "offset 0x%04llX, dtrace static probe ", fFixUpOffsetInSrc);
			break;
		case ppc64::kDtraceProbeSite:
			sprintf(temp, "offset 0x%04llX, dtrace static probe site", fFixUpOffsetInSrc);
			break;
		case ppc64::kDtraceIsEnabledSite:
			sprintf(temp, "offset 0x%04llX, dtrace static probe is-enabled site", fFixUpOffsetInSrc);
			break;
		case ppc64::kDtraceTypeReference:
			sprintf(temp, "offset 0x%04llX, dtrace type/stability reference", fFixUpOffsetInSrc);
			break;
	}
	// always quote by-name references
	if ( fToTargetName != NULL ) {
		strcat(temp, "\"");
		strcat(temp, fToTargetName);
		strcat(temp, "\"");
	}
	else if ( fToTarget.atom != NULL ) {
		strcat(temp, fToTarget.atom->getDisplayName());
	}
	else {
		strcat(temp, "NULL target");
	}
	if ( fToTarget.offset != 0 )
		sprintf(&temp[strlen(temp)], " plus 0x%llX", this->getTargetOffset());

	return temp;
}


template <>
const char* Reference<x86_64>::getDescription() const
{
	static char temp[2048];
	switch( fKind ) {
		case x86_64::kNoFixUp:
			sprintf(temp, "reference to ");
			break;
		case x86_64::kFollowOn:
			sprintf(temp, "followed by ");
			break;
		case x86_64::kGroupSubordinate:
			sprintf(temp, "group subordinate ");
			break;
		case x86_64::kPointerWeakImport:
			sprintf(temp, "offset 0x%04llX, weak import pointer to ", fFixUpOffsetInSrc);
			break;
		case x86_64::kPointer:
			sprintf(temp, "offset 0x%04llX, pointer to ", fFixUpOffsetInSrc);
			break;
		case x86_64::kPointer32:
			sprintf(temp, "offset 0x%04llX, 32-bit pointer to ", fFixUpOffsetInSrc);
			break;
		case x86_64::kPointerDiff32:
		case x86_64::kPointerDiff:
			{
			// by-name references have quoted names
			const char* targetQuotes = (&(this->getTarget()) == NULL) ? "\"" : "";
			const char* fromQuotes = (&(this->getFromTarget()) == NULL) ? "\"" : "";
			const char* size = (fKind == x86_64::kPointerDiff32) ? "32-bit" : "64-bit";
			sprintf(temp, "offset 0x%04llX, %s pointer difference: (&%s%s%s + 0x%08X) - (&%s%s%s + 0x%08X)",
				fFixUpOffsetInSrc, size, targetQuotes, this->getTargetDisplayName(), targetQuotes, fToTarget.offset,
							   fromQuotes, this->getFromTargetDisplayName(), fromQuotes, fFromTarget.offset );
			return temp;
			}
			break;
		case x86_64::kPCRel32:
			sprintf(temp, "offset 0x%04llX, rel32 reference to ", fFixUpOffsetInSrc);
			break;
		case x86_64::kPCRel32_1:
			sprintf(temp, "offset 0x%04llX, rel32-1 reference to ", fFixUpOffsetInSrc);
			break;
		case x86_64::kPCRel32_2:
			sprintf(temp, "offset 0x%04llX, rel32-2 reference to ", fFixUpOffsetInSrc);
			break;
		case x86_64::kPCRel32_4:
			sprintf(temp, "offset 0x%04llX, rel32-4 reference to ", fFixUpOffsetInSrc);
			break;
		case x86_64::kBranchPCRel32:
			sprintf(temp, "offset 0x%04llX, branch rel32 reference to ", fFixUpOffsetInSrc);
			break;
		case x86_64::kBranchPCRel32WeakImport:
			sprintf(temp, "offset 0x%04llX, branch rel32 reference to weak imported ", fFixUpOffsetInSrc);
			break;
		case x86_64::kPCRel32GOT:
			sprintf(temp, "offset 0x%04llX, rel32 reference to GOT entry for ", fFixUpOffsetInSrc);
			break;
		case x86_64::kPCRel32GOTWeakImport:
			sprintf(temp, "offset 0x%04llX, rel32 reference to GOT entry for weak imported ", fFixUpOffsetInSrc);
			break;
		case x86_64::kPCRel32GOTLoad:
			sprintf(temp, "offset 0x%04llX, rel32 reference to GOT entry for ", fFixUpOffsetInSrc);
			break;
		case x86_64::kPCRel32GOTLoadWeakImport:
			sprintf(temp, "offset 0x%04llX, rel32 reference to GOT entry for weak imported ", fFixUpOffsetInSrc);
			break;
		case x86_64::kGOTNoFixUp:
			sprintf(temp, "reference to GOT entry for ");
			break;
		case x86_64::kBranchPCRel8:
			sprintf(temp, "offset 0x%04llX, branch rel8 reference to ", fFixUpOffsetInSrc);
			break;
		case x86_64::kPointerDiff24:
			sprintf(temp, "offset 0x%04llX, 24-bit pointer difference: (&%s + 0x%08X) - (&%s + 0x%08X)",
				fFixUpOffsetInSrc, this->getTargetDisplayName(), fToTarget.offset,
							   this->getFromTargetDisplayName(), fFromTarget.offset );
			return temp;
		case x86_64::kImageOffset32:
			sprintf(temp, "offset 0x%04llX, 32bit offset of ", fFixUpOffsetInSrc);
			break;
		case x86_64::kSectionOffset24:
			sprintf(temp, "offset 0x%04llX, 24-bit section offset of ", fFixUpOffsetInSrc);
			break;
		case x86_64::kDtraceProbe:
			sprintf(temp, "offset 0x%04llX, dtrace static probe ", fFixUpOffsetInSrc);
			break;
		case x86_64::kDtraceProbeSite:
			sprintf(temp, "offset 0x%04llX, dtrace static probe site", fFixUpOffsetInSrc);
			break;
		case x86_64::kDtraceIsEnabledSite:
			sprintf(temp, "offset 0x%04llX, dtrace static probe is-enabled site", fFixUpOffsetInSrc);
			break;
		case x86_64::kDtraceTypeReference:
			sprintf(temp, "offset 0x%04llX, dtrace type/stability reference", fFixUpOffsetInSrc);
			break;
	}
	// always quote by-name references
	if ( fToTargetName != NULL ) {
		strcat(temp, "\"");
		strcat(temp, fToTargetName);
		strcat(temp, "\"");
	}
	else if ( fToTarget.atom != NULL ) {
		strcat(temp, fToTarget.atom->getDisplayName());
	}
	else {
		strcat(temp, "NULL target");
	}
	if ( fToTarget.offset != 0 )
		sprintf(&temp[strlen(temp)], " plus 0x%llX", this->getTargetOffset());

	return temp;
}


template <>
const char* Reference<arm>::getDescription() const
{
	static char temp[2048];
	switch( fKind ) {
		case arm::kNoFixUp:
			sprintf(temp, "reference to ");
			break;
		case arm::kFollowOn:
			sprintf(temp, "followed by ");
			break;
		case arm::kGroupSubordinate:
			sprintf(temp, "group subordinate ");
			break;
		case arm::kPointer:
			sprintf(temp, "offset 0x%04X, pointer to ", fFixUpOffsetInSrc);
			break;
		case arm::kPointerWeakImport:
			sprintf(temp, "offset 0x%04X, weak import pointer to ", fFixUpOffsetInSrc);
			break;
		case arm::kPointerDiff:
			{
			// by-name references have quoted names
			const char* targetQuotes = (&(this->getTarget()) == NULL) ? "\"" : "";
			const char* fromQuotes = (&(this->getFromTarget()) == NULL) ? "\"" : "";
			sprintf(temp, "offset 0x%04X, 32-bit pointer difference: (&%s%s%s + %d) - (&%s%s%s + %d)",
				fFixUpOffsetInSrc, targetQuotes, this->getTargetDisplayName(), targetQuotes, fToTarget.offset,
							   fromQuotes, this->getFromTargetDisplayName(), fromQuotes, fFromTarget.offset );
			return temp;
			}
		case arm::kReadOnlyPointer:
			sprintf(temp, "offset 0x%04X, read-only pointer to ", fFixUpOffsetInSrc);
			break;
		case arm::kBranch24:
        case arm::kThumbBranch22:
			sprintf(temp, "offset 0x%04X, pc-rel branch fixup to ", fFixUpOffsetInSrc);
			break;
		case arm::kBranch24WeakImport:
        case arm::kThumbBranch22WeakImport:
			sprintf(temp, "offset 0x%04X, pc-rel branch fixup to weak imported ", fFixUpOffsetInSrc);
			break;
		case arm::kDtraceProbe:
			sprintf(temp, "offset 0x%04X, dtrace static probe ", fFixUpOffsetInSrc);
			break;
		case arm::kDtraceProbeSite:
			sprintf(temp, "offset 0x%04X, dtrace static probe site", fFixUpOffsetInSrc);
			break;
		case arm::kDtraceIsEnabledSite:
			sprintf(temp, "offset 0x%04X, dtrace static probe is-enabled site", fFixUpOffsetInSrc);
			break;
		case arm::kDtraceTypeReference:
			sprintf(temp, "offset 0x%04X, dtrace type/stability reference", fFixUpOffsetInSrc);
			break;
	}
	// always quote by-name references
	if ( fToTargetName != NULL ) {
		strcat(temp, "\"");
		strcat(temp, fToTargetName);
		strcat(temp, "\"");
	}
	else if ( fToTarget.atom != NULL ) {
		strcat(temp, fToTarget.atom->getDisplayName());
	}
	else {
		strcat(temp, "NULL target");
	}
	if ( fToTarget.offset != 0 )
		sprintf(&temp[strlen(temp)], " plus 0x%08X", fToTarget.offset);

	return temp;
}


template <>
bool Reference<x86>::isBranch() const
{
	switch ( fKind ) {
		case x86::kPCRel32:
		case x86::kPCRel32WeakImport:
			return true;
		default:
			return false;
	}
}

template <>
bool Reference<x86_64>::isBranch() const
{
	switch ( fKind ) {
		case x86_64::kBranchPCRel32:
		case x86_64::kBranchPCRel32WeakImport:
			return true;
		default:
			return false;
	}
}

template <>
bool Reference<ppc>::isBranch() const
{
	switch ( fKind ) {
		case ppc::kBranch24:
		case ppc::kBranch24WeakImport:
			return true;
		default:
			return false;
	}
}

template <>
bool Reference<ppc64>::isBranch() const
{
	switch ( fKind ) {
		case ppc64::kBranch24:
		case ppc64::kBranch24WeakImport:
			return true;
		default:
			return false;
	}
}

template <>
bool Reference<arm>::isBranch() const
{
	switch ( fKind ) {
		case arm::kBranch24:
		case arm::kBranch24WeakImport:
		case arm::kThumbBranch22:
		case arm::kThumbBranch22WeakImport:
			return true;
		default:
			return false;
	}
}



}; // namespace relocatable
}; // namespace mach_o

#endif // __OBJECT_FILE_MACH_O__
