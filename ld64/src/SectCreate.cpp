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

#include <vector>

#include "ObjectFile.h"

namespace SectCreate {


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


class Reader : public ObjectFile::Reader 
{
public:
												Reader(const char* segmentName, const char* sectionName, const char* path, const uint8_t fileContent[], uint64_t fileLength);
	virtual										~Reader();
	
	virtual const char*								getPath()								{ return fPath; }
	virtual std::vector<class ObjectFile::Atom*>&	getAtoms()								{ return fAtoms; }
	virtual std::vector<class ObjectFile::Atom*>*	getJustInTimeAtomsFor(const char* name) { return NULL; }
	virtual std::vector<ObjectFile::StabsInfo>*		getStabsDebugInfo()						{ return NULL; }

private:
	const char*										fPath;
	std::vector<class ObjectFile::Atom*>			fAtoms;
};


class Atom : public ObjectFile::Atom {
public:
	virtual ObjectFile::Reader*					getFile() const				{ return &fOwner; }
	virtual const char*							getName() const				{ return NULL; }
	virtual const char*							getDisplayName() const;
	virtual Scope								getScope() const			{ return ObjectFile::Atom::scopeTranslationUnit; }
	virtual bool								isTentativeDefinition() const { return false; }
	virtual bool								isWeakDefinition() const	{ return false; }
	virtual bool								isCoalesableByName() const	{ return false; }
	virtual bool								isCoalesableByValue() const { return false; }
	virtual bool								isZeroFill() const			{ return false; }
	virtual bool								dontDeadStrip() const		{ return true; }
	virtual bool								dontStripName() const		{ return false; }
	virtual bool								isImportProxy() const		{ return false; }
	virtual uint64_t							getSize() const				{ return fFileLength; }
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const		{ return fgEmptyReferenceList; }
	virtual bool								mustRemainInSection() const { return false; }
	virtual const char*							getSectionName() const		{ return fSectionName; }
	virtual Segment&							getSegment() const			{ return fSegment; }
	virtual bool								requiresFollowOnAtom() const{ return false; }
	virtual ObjectFile::Atom&					getFollowOnAtom() const		{ return *((ObjectFile::Atom*)NULL); }
	virtual std::vector<ObjectFile::StabsInfo>*	getStabsDebugInfo() const	{ return NULL; }
	virtual uint8_t								getAlignment() const		{ return 4; }
	virtual WeakImportSetting					getImportWeakness() const	{ return ObjectFile::Atom::kWeakUnset; }
	virtual void								copyRawContent(uint8_t buffer[]) const;
	virtual void								writeContent(bool finalLinkedImage, ObjectFile::ContentWriter&) const;

	virtual void								setScope(Scope)				{ }
	virtual void								setImportWeakness(bool)		{ }

protected:
	friend class Reader;
	
											Atom(Reader& owner, Segment& segment, const char* sectionName, const uint8_t fileContent[], uint64_t fileLength) 
												: fOwner(owner), fSegment(segment), fSectionName(sectionName), fFileContent(fileContent), fFileLength(fileLength) {}
	virtual									~Atom() {}
	
	Reader&									fOwner;
	Segment&								fSegment;
	const char*								fSectionName;
	const uint8_t*							fFileContent;
	uint64_t								fFileLength;
	
	static std::vector<ObjectFile::Reference*> fgEmptyReferenceList;
};


std::vector<ObjectFile::Reference*> Atom::fgEmptyReferenceList;



Reader::Reader(const char* segmentName, const char* sectionName, const char* path, const uint8_t fileContent[], uint64_t fileLength)
 : fPath(path)
{
	fAtoms.push_back(new Atom(*this, *(new Segment(segmentName)), sectionName, fileContent, fileLength));
}

Reader::~Reader()
{
}


const char*	 Atom::getDisplayName() const
{
	static char name[64];
	sprintf(name, "-sectcreate %s %s", fSegment.getName(), fSectionName);
	return name;
}


void Atom::copyRawContent(uint8_t buffer[]) const
{
	memcpy(buffer, fFileContent, fFileLength);
}

void Atom::writeContent(bool finalLinkedImage, ObjectFile::ContentWriter& writer) const
{
	writer.write(0, fFileContent, fFileLength);
}


Reader* MakeReader(const char* segmentName, const char* sectionName, const char* path, const uint8_t fileContent[], uint64_t fileLength)
{
	return new Reader(segmentName, sectionName, path, fileContent, fileLength);
}



};







