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


#ifndef __OBJECTFILE__
#define __OBJECTFILE__

#include <stdint.h>
#include <vector>
#include <map>



namespace ObjectFile {

struct StabsInfo
{
	uint64_t	atomOffset;
	const char* string;
	uint8_t		type;
	uint8_t		other;
	uint16_t	desc;
};

class ReaderOptions
{
public:
						ReaderOptions() : fFullyLoadArchives(false), fLoadObjcClassesInArchives(false), fFlatNamespace(false), 
											fStripDebugInfo(false), fTraceDylibs(false), fTraceIndirectDylibs(false), fTraceArchives(false) {}

	bool				fFullyLoadArchives;
	bool				fLoadObjcClassesInArchives;
	bool				fFlatNamespace;
	bool				fStripDebugInfo;
	bool				fTraceDylibs;
	bool				fTraceIndirectDylibs;
	bool				fTraceArchives;
};


class Reader
{
public:
	static Reader* createReader(const char* path, const ReaderOptions& options);
	
	virtual const char*					getPath() = 0;
	virtual std::vector<class Atom*>&	getAtoms() = 0;
	virtual std::vector<class Atom*>*	getJustInTimeAtomsFor(const char* name) = 0;
	virtual std::vector<StabsInfo>*		getStabsDebugInfo() = 0;
	
	// For Dynamic Libraries only
	virtual const char*					getInstallPath()			{ return NULL; }
	virtual uint32_t					getTimestamp()				{ return 0; }
	virtual uint32_t					getCurrentVersion()			{ return 0; }
	virtual uint32_t					getCompatibilityVersion()	{ return 0; }
	virtual std::vector<const char*>*	getDependentLibraryPaths()	{ return NULL; }
	virtual bool						reExports(Reader*)			{ return false; }
	virtual bool						isDefinitionWeak(const Atom&){ return false; }
	
	
	
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


class ContentWriter
{
public:
	virtual void	write(uint64_t atomOffset, const void* buffer, uint64_t size) = 0;
protected:
								ContentWriter() {}
	virtual						~ContentWriter() {}
};

class Atom 
{
public:
	enum Scope { scopeTranslationUnit, scopeLinkageUnit, scopeGlobal };
	enum WeakImportSetting { kWeakUnset, kWeakImport, kNonWeakImport };
	
	virtual Reader*							getFile() const = 0;
	virtual const char*						getName() const = 0;
	virtual const char*						getDisplayName() const = 0;
	virtual Scope							getScope() const = 0;
	virtual bool							isTentativeDefinition() const = 0;
	virtual bool							isWeakDefinition() const = 0;
	virtual bool							isCoalesableByName() const = 0;
	virtual bool							isCoalesableByValue() const = 0;
	virtual bool							isZeroFill() const = 0;
	virtual bool							dontDeadStrip() const = 0;
	virtual bool							dontStripName() const = 0;  // referenced dynamically
	virtual bool							isImportProxy() const = 0;
	virtual uint64_t						getSize() const = 0;
	virtual std::vector<ObjectFile::Reference*>&  getReferences() const = 0;
	virtual bool							mustRemainInSection() const = 0;
	virtual const char*						getSectionName() const = 0;
	virtual Segment&						getSegment() const = 0;
	virtual bool							requiresFollowOnAtom() const = 0;
	virtual Atom&							getFollowOnAtom() const = 0;
	virtual std::vector<StabsInfo>*			getStabsDebugInfo() const = 0;
	virtual uint8_t							getAlignment() const = 0;
	virtual WeakImportSetting				getImportWeakness() const = 0;
	virtual void							copyRawContent(uint8_t buffer[]) const = 0;
	virtual void							writeContent(bool finalLinkedImage, ContentWriter&) const = 0;
	virtual void							setScope(Scope) = 0;
	virtual void							setImportWeakness(bool weakImport) = 0; 

	
			uint64_t						getSectionOffset() const	{ return fSectionOffset; }
			uint64_t						getSegmentOffset() const	{ return fSegmentOffset; }
			uint64_t						getAddress() const	{ return fSection->getBaseAddress() + fSectionOffset; }
			unsigned int					getSortOrder() const { return fSortOrder; }
			class Section*					getSection() const { return fSection; }

			void							setSegmentOffset(uint64_t offset) { fSegmentOffset = offset; }
			void							setSectionOffset(uint64_t offset) { fSectionOffset = offset; }
			void							setSection(class Section* sect) { fSection = sect; } 
			unsigned int					setSortOrder(unsigned int order); // recursively sets follow-on atoms

protected:
											Atom() : fSegmentOffset(0), fSectionOffset(0), fSortOrder(0), fSection(NULL) {}
		virtual								~Atom() {}
		
		uint64_t							fSegmentOffset;
		uint64_t							fSectionOffset;
		unsigned int						fSortOrder;
		class Section*						fSection;
};



// recursively sets follow-on atoms
inline unsigned int Atom::setSortOrder(unsigned int order)
{
	if ( this->requiresFollowOnAtom() ) {
		fSortOrder = order;
		return this->getFollowOnAtom().setSortOrder(order+1);
	}
	else {
		fSortOrder = order;
		return (order + 1);
	}
}



class Reference
{
public:
	enum Kind { noFixUp, pointer, ppcFixupBranch24, ppcFixupBranch14, 
				ppcFixupPicBaseLow16, ppcFixupPicBaseLow14, ppcFixupPicBaseHigh16, 
				ppcFixupAbsLow16, ppcFixupAbsLow14, ppcFixupAbsHigh16, ppcFixupAbsHigh16AddLow,
				pointer32Difference, pointer64Difference, x86FixupBranch32 };

	virtual bool			isTargetUnbound() const = 0;
	virtual bool			isFromTargetUnbound() const = 0;
	virtual bool			requiresRuntimeFixUp(bool slideable) const = 0;
	virtual bool			isWeakReference() const = 0;
	virtual bool			isLazyReference() const = 0;
	virtual Kind			getKind() const = 0;
	virtual uint64_t		getFixUpOffset() const = 0;
	virtual const char*		getTargetName() const = 0;
	virtual Atom&			getTarget() const = 0;
	virtual uint64_t		getTargetOffset() const = 0;
	virtual bool			hasFromTarget() const = 0;
	virtual Atom&			getFromTarget() const = 0;
	virtual const char*		getFromTargetName() const = 0;
	virtual uint64_t		getFromTargetOffset() const = 0;

	virtual void			setTarget(Atom&, uint64_t offset) = 0;
	virtual void			setFromTarget(Atom&) = 0;
	virtual const char*		getDescription() const = 0;
	
protected:
							Reference() {}
	virtual					~Reference() {}
};


};	// namespace ObjectFile


#endif // __OBJECTFILE__






