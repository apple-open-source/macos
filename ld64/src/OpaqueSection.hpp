/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*- 
 *
 * Copyright (c) 2005-2006 Apple Computer, Inc. All rights reserved.
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

#ifndef __OPAQUE_SECTION__
#define __OPAQUE_SECTION__


#include <vector>

#include "ObjectFile.h"

namespace opaque_section {


class Segment : public ObjectFile::Segment
{
public:
								Segment(const char* name)		{ fName = name; }
	virtual const char*			getName() const					{ return fName; }
	virtual bool				isContentReadable() const		{ return true; }
	virtual bool				isContentWritable() const		{ return false; }
	virtual bool				isContentExecutable() const		{ return (strcmp(fName, "__TEXT") == 0); }
private:
	const char*					fName;
};


class Reader : public ObjectFile::Reader 
{
public:
												Reader(const char* segmentName, const char* sectionName, const char* path, 
													const uint8_t fileContent[], uint64_t fileLength, uint32_t ordinal, const char* symbolName=NULL);
	virtual										~Reader();
	
	void											addSectionReference(uint8_t kind, uint64_t offsetInSection, const ObjectFile::Atom* targetAtom, 
																		uint64_t offsetInTarget, const ObjectFile::Atom* fromTargetAtom=NULL, uint64_t offsetInFromTarget=0);
	
	virtual const char*								getPath()								{ return fPath; }
	virtual time_t									getModificationTime()					{ return 0; }
	virtual DebugInfoKind							getDebugInfoKind()						{ return ObjectFile::Reader::kDebugInfoNone; }
	virtual std::vector<class ObjectFile::Atom*>&	getAtoms()								{ return fAtoms; }
	virtual std::vector<class ObjectFile::Atom*>*	getJustInTimeAtomsFor(const char* name) { return NULL; }
	virtual std::vector<Stab>*						getStabs()								{ return NULL; }

private:
	const char*										fPath;
	std::vector<class ObjectFile::Atom*>			fAtoms;
};

class Reference : public ObjectFile::Reference
{
public:
							Reference(uint8_t kind, uint64_t fixupOffset, const ObjectFile::Atom* target, uint64_t targetOffset,
										const ObjectFile::Atom* fromTarget=NULL, uint64_t fromTargetOffset=0)
								: fFixUpOffset(fixupOffset), fTarget(target), fTargetOffset(targetOffset), fKind(kind), 
									fFromTarget(fromTarget), fFromTargetOffset(fromTargetOffset)  {}
	virtual					~Reference() {}


	virtual ObjectFile::Reference::TargetBinding	getTargetBinding() const	{ return ObjectFile::Reference::kBoundDirectly; }
	virtual ObjectFile::Reference::TargetBinding	getFromTargetBinding() const{ return ObjectFile::Reference::kDontBind; }
	virtual uint8_t									getKind() const				{ return fKind; }
	virtual uint64_t								getFixUpOffset() const		{ return fFixUpOffset; }
	virtual const char*								getTargetName() const		{ return fTarget->getName(); }
	virtual ObjectFile::Atom&						getTarget() const			{ return *((ObjectFile::Atom*)fTarget); }
	virtual uint64_t								getTargetOffset() const		{ return fTargetOffset; }
	virtual ObjectFile::Atom&						getFromTarget() const		{ return *((ObjectFile::Atom*)fFromTarget); }
	virtual const char*								getFromTargetName() const	{ return fFromTarget->getName();  }
	virtual uint64_t								getFromTargetOffset() const { return fFromTargetOffset; }
	virtual void									setTarget(ObjectFile::Atom&, uint64_t offset) { throw "can't set target"; }
	virtual void									setFromTarget(ObjectFile::Atom&) { throw "can't set from target"; }
	virtual const char*								getDescription() const		{ return "opaque section reference"; }

private:
	uint64_t				fFixUpOffset;
	const ObjectFile::Atom*	fTarget;
	uint64_t				fTargetOffset;
	uint8_t					fKind;
	const ObjectFile::Atom*	fFromTarget;
	uint64_t				fFromTargetOffset;
};


class Atom : public ObjectFile::Atom {
public:
	virtual ObjectFile::Reader*					getFile() const				{ return &fOwner; }
	virtual bool								getTranslationUnitSource(const char** dir, const char** name) const { return false; }
	virtual const char*							getName() const				{ return fName; }
	virtual const char*							getDisplayName() const;
	virtual Scope								getScope() const			{ return ObjectFile::Atom::scopeLinkageUnit; }
	virtual DefinitionKind						getDefinitionKind() const	{ return kRegularDefinition; }
	virtual SymbolTableInclusion				getSymbolTableInclusion() const { return ObjectFile::Atom::kSymbolTableNotIn; }
	virtual	bool								dontDeadStrip() const		{ return true; }
	virtual bool								isZeroFill() const			{ return false; }
	virtual uint64_t							getSize() const				{ return fFileLength; }
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const		{ return (std::vector<ObjectFile::Reference*>&)(fReferences); }
	virtual bool								mustRemainInSection() const { return false; }
	virtual const char*							getSectionName() const		{ return fSectionName; }
	virtual Segment&							getSegment() const			{ return fSegment; }
	virtual ObjectFile::Atom&					getFollowOnAtom() const		{ return *((ObjectFile::Atom*)NULL); }
	virtual uint32_t							getOrdinal() const			{ return fOrdinal; }
	virtual std::vector<ObjectFile::LineInfo>*	getLineInfo() const			{ return NULL; }
	virtual ObjectFile::Alignment				getAlignment() const		{ return ObjectFile::Alignment(4); }
	virtual void								copyRawContent(uint8_t buffer[]) const;

	virtual void								setScope(Scope)				{ }

protected:
	friend class Reader;
	
											Atom(Reader& owner, Segment& segment, const char* sectionName, 
												const uint8_t fileContent[], uint64_t fileLength, uint32_t ordinal, const char* symbolName);
	virtual									~Atom() {}
	
	Reader&									fOwner;
	Segment&								fSegment;
	const char*								fName;
	const char*								fSectionName;
	const uint8_t*							fFileContent;
	uint32_t								fOrdinal;
	uint64_t								fFileLength;
	std::vector<ObjectFile::Reference*>		fReferences;
};


	
Atom::Atom(Reader& owner, Segment& segment, const char* sectionName, const uint8_t fileContent[], uint64_t fileLength, uint32_t ordinal, const char* symbolName) 
	: fOwner(owner), fSegment(segment), fSectionName(sectionName), fFileContent(fileContent), fOrdinal(ordinal), fFileLength(fileLength) 
{ 
	if ( symbolName != NULL )
		fName = strdup(symbolName);
	else
		asprintf((char**)&fName, "__section$%s%s", segment.getName(), sectionName);
}


Reader::Reader(const char* segmentName, const char* sectionName, const char* path, const uint8_t fileContent[], 
			uint64_t fileLength, uint32_t ordinal, const char* symbolName)
 : fPath(path)
{
	fAtoms.push_back(new Atom(*this, *(new Segment(segmentName)), strdup(sectionName), fileContent, fileLength, ordinal, symbolName));
}

Reader::~Reader()
{
}

void Reader::addSectionReference(uint8_t kind, uint64_t offsetInSection, const ObjectFile::Atom* targetAtom, 
								uint64_t offsetInTarget, const ObjectFile::Atom* fromTargetAtom, uint64_t offsetInFromTarget)
{
	fAtoms[0]->getReferences().push_back(new Reference(kind, offsetInSection, targetAtom, offsetInTarget, fromTargetAtom, offsetInFromTarget));
}


const char*	 Atom::getDisplayName() const
{
	static char name[64];
	sprintf(name, "opaque section %s %s", fSegment.getName(), fSectionName);
	return name;
}


void Atom::copyRawContent(uint8_t buffer[]) const
{
	memcpy(buffer, fFileContent, fFileLength);
}



};



#endif // __OPAQUE_SECTION__



