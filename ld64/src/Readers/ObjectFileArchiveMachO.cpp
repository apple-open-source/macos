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



namespace ObjectFileArchiveMachO {

class Reader : public ObjectFile::Reader 
{
public:
												Reader(const uint8_t fileContent[], uint64_t fileLength, const char* path, const ObjectFile::ReaderOptions& options);
	virtual										~Reader();
	
	virtual const char*								getPath();
	virtual std::vector<class ObjectFile::Atom*>&	getAtoms();
	virtual std::vector<class ObjectFile::Atom*>*	getJustInTimeAtomsFor(const char* name);
	virtual std::vector<ObjectFile::StabsInfo>*		getStabsDebugInfo();

private:
	class Entry : ar_hdr
	{
	public:
		const char*			getName() const;
		const uint8_t*		getContent() const;
		uint32_t			getContentSize() const;
		const Entry*		getNext() const;
	private:
		bool				hasLongName() const;
		unsigned int		getLongNameSpace() const;
		
	};

	const struct ranlib*							ranlibBinarySearch(const char* name);
	const struct ranlib*							ranlibLinearSearch(const char* name);
	ObjectFile::Reader*								makeObjectReaderForMember(const Entry* member);
	void											dumpTableOfContents();
	
	const char*										fPath;
	const ObjectFile::ReaderOptions&				fOptions;
	const uint8_t*									fFileContent;
	uint64_t										fFileLength;
	const struct ranlib*							fTableOfContents;
	uint32_t										fTableOfContentCount;
	bool											fSorted;
	const char*										fStringPool;
	std::vector<class ObjectFile::Atom*>			fAllAtoms;
	std::set<const class Entry*>					fInstantiatedEntries;
	
	static std::vector<class ObjectFile::Atom*>		fgEmptyList;
};

std::vector<class ObjectFile::Atom*>		Reader::fgEmptyList;


bool Reader::Entry::hasLongName() const
{
	return ( strncmp(this->ar_name, AR_EFMT1, strlen(AR_EFMT1)) == 0 );
}

unsigned int Reader::Entry::getLongNameSpace() const
{
	char* endptr;
	long result = strtol(&this->ar_name[strlen(AR_EFMT1)], &endptr, 10);
	return result;
}

const char* Reader::Entry::getName() const
{
	if ( this->hasLongName() ) {
		int len = this->getLongNameSpace();
		static char longName[256];
		strncpy(longName, ((char*)this)+sizeof(ar_hdr), len);
		longName[len] = '\0';
		return longName;
	}
	else {
		static char shortName[20];
		strncpy(shortName, this->ar_name, 16);
		shortName[16] = '\0';
		char* space = strchr(shortName, ' ');
		if ( space != NULL )
			*space = '\0';
		return shortName;
	}
}


const uint8_t* Reader::Entry::getContent() const
{
	if ( this->hasLongName() ) 
		return ((uint8_t*)this) + sizeof(ar_hdr) + this->getLongNameSpace();
	else
		return ((uint8_t*)this) + sizeof(ar_hdr);
}


uint32_t Reader::Entry::getContentSize() const
{
	char temp[12];
	strncpy(temp, this->ar_size, 10);
	temp[10] = '\0';
	char* endptr;
	long size = strtol(temp, &endptr, 10);
	// long name is included in ar_size
	if ( this->hasLongName() ) 
		size -= this->getLongNameSpace();
	return size;
}

const Reader::Entry*	Reader::Entry::getNext() const
{
	const uint8_t* p = this->getContent() + getContentSize();
	p = (const uint8_t*)(((uint32_t)p+3) & (-4));  // 4-byte align
	return (Reader::Entry*)p;
}



Reader::Reader(const uint8_t fileContent[], uint64_t fileLength, const char* path, const ObjectFile::ReaderOptions& options)
 : fPath(NULL), fOptions(options), fFileContent(NULL), fTableOfContents(NULL), fTableOfContentCount(0),
   fSorted(false), fStringPool(NULL)
{
	fPath = strdup(path);
	fFileContent = fileContent;
	fFileLength = fileLength;

	if ( strncmp((const char*)fileContent, "!<arch>\n", 8) != 0 )
		throw "not an archive";
	
	if ( !options.fFullyLoadArchives ) {
		const Entry* const firstMember = (Entry*)&fFileContent[8];
		if ( strcmp(firstMember->getName(), SYMDEF_SORTED) == 0 )
			fSorted = true;
		else if ( strcmp(firstMember->getName(), SYMDEF) == 0 )
			fSorted = false;
		else
			throw "archive has no table of contents";
		const uint8_t* contents = firstMember->getContent();
		uint32_t ranlibArrayLen = OSReadBigInt32((void *) contents, 0);
		fTableOfContents = (const struct ranlib*)&contents[4];
		fTableOfContentCount = ranlibArrayLen / sizeof(struct ranlib);
		fStringPool = (const char*)&contents[ranlibArrayLen+8];
	}
	
	if ( options.fTraceArchives )
		printf("[Logging for Build & Integration] Used static archive: %s\n", fPath);
}

Reader::~Reader()
{
}



ObjectFile::Reader* Reader::makeObjectReaderForMember(const Entry* member)
{
	const char* memberName = member->getName();
	char memberPath[strlen(fPath) + strlen(memberName)+4];
	strcpy(memberPath, fPath);
	strcat(memberPath, "(");
	strcat(memberPath, memberName);
	strcat(memberPath, ")");
	//fprintf(stderr, "using %s from %s\n", memberName, fPath);
	try {
		return ObjectFileMachO::MakeReader((class macho_header*)member->getContent(), memberPath, fOptions);
	}
	catch (const char* msg) {
		throwf("in %s, %s", memberPath, msg);
	}
}

const char* Reader::getPath()
{
	return fPath;
}

std::vector<class ObjectFile::Atom*>&	Reader::getAtoms()
{
	if ( fOptions.fFullyLoadArchives ) {
		// build vector of all atoms from all .o files in this archive
		const Entry* const start = (Entry*)&fFileContent[8];
		const Entry* const end = (Entry*)&fFileContent[fFileLength];
		for (const Entry* p=start; p < end; p = p->getNext()) {
			const char* memberName = p->getName();
			if ( (p==start) && (strcmp(memberName, SYMDEF_SORTED) == 0) )
				continue;
			ObjectFile::Reader* r = this->makeObjectReaderForMember(p);
			std::vector<class ObjectFile::Atom*>&	atoms = r->getAtoms();
			fAllAtoms.insert(fAllAtoms.end(), atoms.begin(), atoms.end());
		}
		return fAllAtoms;
	}
	else {
		// return nonthing for now, getJustInTimeAtomsFor() will return atoms as needed
		return fgEmptyList;
	}
}


const struct ranlib* Reader::ranlibBinarySearch(const char* key)
{
	const struct ranlib* base = fTableOfContents;
	for (uint32_t n = fTableOfContentCount; n > 0; n /= 2) {
		const struct ranlib* pivot = &base[n/2];
		const char* pivotStr = &fStringPool[OSSwapBigToHostInt32(pivot->ran_un.ran_strx)];
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

const struct ranlib* Reader::ranlibLinearSearch(const char* key)
{
	for (uint32_t i = 0; i < fTableOfContentCount; ++i) {
		const struct ranlib* entry = &fTableOfContents[i];
		const char* entryName = &fStringPool[OSSwapBigToHostInt32(entry->ran_un.ran_strx)];
		if ( strcmp(key, entryName) == 0 )
			return entry;
	}
	return NULL;
}


void Reader::dumpTableOfContents()
{
	for (unsigned int i=0; i < fTableOfContentCount; ++i) {
		const struct ranlib* e = &fTableOfContents[i];
		printf("%s in %s\n", &fStringPool[OSSwapBigToHostInt32(e->ran_un.ran_strx)], ((Entry*)&fFileContent[OSSwapBigToHostInt32(e->ran_off)])->getName());
	}
}

std::vector<class ObjectFile::Atom*>* Reader::getJustInTimeAtomsFor(const char* name)
{
	if ( fOptions.fFullyLoadArchives ) {
		return NULL;
	}
	else {
		const struct ranlib* result = NULL;
		if ( fSorted ) {
			// do a binary search of table of contents lookig for requested symbol
			result = ranlibBinarySearch(name);
		}
		else {
			// do a linear search of table of contents lookig for requested symbol
			result = ranlibLinearSearch(name);
		}
		if ( result != NULL ) {
			const Entry* member = (Entry*)&fFileContent[OSSwapBigToHostInt32(result->ran_off)];
			//fprintf(stderr, "%s found in %s\n", name, member->getName());
			if ( fInstantiatedEntries.count(member) == 0 ) {
				// only return these atoms once
				fInstantiatedEntries.insert(member);
				ObjectFile::Reader* r = makeObjectReaderForMember(member);
				return new std::vector<class ObjectFile::Atom*>(r->getAtoms());
			}
		}
		//fprintf(stderr, "%s NOT found in archive %s\n", name, fPath);
		return NULL; 
	}
}


std::vector<ObjectFile::StabsInfo>*	Reader::getStabsDebugInfo()
{
	return NULL;
}



Reader* MakeReader(const uint8_t fileContent[], uint64_t fileLength, const char* path, const ObjectFile::ReaderOptions& options)
{
	return new Reader(fileContent, fileLength, path, options);
}



};







