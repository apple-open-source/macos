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

#ifndef __OBJECT_FILE_ARCHIVE_MACH_O__
#define __OBJECT_FILE_ARCHIVE_MACH_O__

#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <sys/param.h>
#include <mach-o/ranlib.h>
#include <ar.h>

#include <vector>
#include <set>
#include <algorithm>
#include <ext/hash_map>

#include "MachOFileAbstraction.hpp"
#include "ObjectFile.h"
#include "MachOReaderRelocatable.hpp"


namespace mach_o {
namespace archive {

typedef const struct ranlib* ConstRanLibPtr;

template <typename A>
class Reader : public ObjectFile::Reader
{
public:
	static bool										validFile(const uint8_t* fileContent, uint64_t fileLength);
													Reader(const uint8_t fileContent[], uint64_t fileLength,
															const char* path, time_t modTime, 
															const ObjectFile::ReaderOptions& options, uint32_t ordinalBase);
	virtual											~Reader() {}

	virtual const char*								getPath()			{ return fPath; }
	virtual time_t									getModificationTime(){ return fModTime; }
	virtual DebugInfoKind							getDebugInfoKind()	{ return ObjectFile::Reader::kDebugInfoNone; }
	virtual std::vector<class ObjectFile::Atom*>&	getAtoms();
	virtual std::vector<class ObjectFile::Atom*>*	getJustInTimeAtomsFor(const char* name);
	virtual std::vector<Stab>*						getStabs()			{ return NULL; }

private:
	class Entry : ar_hdr
	{
	public:
		const char*			getName() const;
		time_t				getModTime() const;
		const uint8_t*		getContent() const;
		uint32_t			getContentSize() const;
		const Entry*		getNext() const;
	private:
		bool				hasLongName() const;
		unsigned int		getLongNameSpace() const;

	};

	class CStringEquals
	{
	public:
		bool operator()(const char* left, const char* right) const { return (strcmp(left, right) == 0); }
	};
	typedef __gnu_cxx::hash_map<const char*, const struct ranlib*, __gnu_cxx::hash<const char*>, CStringEquals> NameToEntryMap;

	typedef typename A::P							P;
	typedef typename A::P::E						E;

	const struct ranlib*							ranlibHashSearch(const char* name);
	ObjectFile::Reader*								makeObjectReaderForMember(const Entry* member);
	void											dumpTableOfContents();
	void											buildHashTable();

	const char*										fPath;
	time_t											fModTime;
	const ObjectFile::ReaderOptions&				fOptions;
	uint32_t										fOrdinalBase;
	const uint8_t*									fFileContent;
	uint64_t										fFileLength;
	const struct ranlib*							fTableOfContents;
	uint32_t										fTableOfContentCount;
	const char*										fStringPool;
	std::vector<class ObjectFile::Atom*>			fAllAtoms;
	std::set<const class Entry*>					fInstantiatedEntries;
	std::set<const class Entry*>					fPossibleEntries;
	NameToEntryMap									fHashTable;

	static std::vector<class ObjectFile::Atom*>		fgEmptyList;
};

template <typename A>
std::vector<class ObjectFile::Atom*>		Reader<A>::fgEmptyList;


template <typename A>
bool Reader<A>::Entry::hasLongName() const
{
	return ( strncmp(this->ar_name, AR_EFMT1, strlen(AR_EFMT1)) == 0 );
}

template <typename A>
unsigned int Reader<A>::Entry::getLongNameSpace() const
{
	char* endptr;
	long result = strtol(&this->ar_name[strlen(AR_EFMT1)], &endptr, 10);
	return result;
}

template <typename A>
const char* Reader<A>::Entry::getName() const
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

template <typename A>
time_t	Reader<A>::Entry::getModTime() const
{
	char temp[14];
	strncpy(temp, this->ar_date, 12);
	temp[12] = '\0';
	char* endptr;
	return (time_t)strtol(temp, &endptr, 10);
}


template <typename A>
const uint8_t* Reader<A>::Entry::getContent() const
{
	if ( this->hasLongName() )
		return ((uint8_t*)this) + sizeof(ar_hdr) + this->getLongNameSpace();
	else
		return ((uint8_t*)this) + sizeof(ar_hdr);
}


template <typename A>
uint32_t Reader<A>::Entry::getContentSize() const
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


template <typename A>
const class Reader<A>::Entry* Reader<A>::Entry::getNext() const
{
	const uint8_t* p = this->getContent() + getContentSize();
	p = (const uint8_t*)(((uintptr_t)p+3) & (-4));  // 4-byte align
	return (class Reader<A>::Entry*)p;
}

template <typename A>
bool Reader<A>::validFile(const uint8_t* fileContent, uint64_t fileLength)
{
	// must have valid archive header
	if ( strncmp((const char*)fileContent, "!<arch>\n", 8) != 0 )
		return false;
		
	// peak at first .o file and verify it is correct architecture
	const Entry* const start = (Entry*)&fileContent[8];
	const Entry* const end = (Entry*)&fileContent[fileLength];
	for (const Entry* p=start; p < end; p = p->getNext()) {
		const char* memberName = p->getName();
		// skip option table-of-content member
		if ( (p==start) && ((strcmp(memberName, SYMDEF_SORTED) == 0) || (strcmp(memberName, SYMDEF) == 0)) )
			continue;
		// archive is valid if first .o file is valid
		return mach_o::relocatable::Reader<A>::validFile(p->getContent());
	}	
	// empty archive
	return true;
}

template <typename A>
Reader<A>::Reader(const uint8_t fileContent[], uint64_t fileLength, const char* path, time_t modTime, 
					const ObjectFile::ReaderOptions& options, uint32_t ordinalBase)
 : fPath(NULL), fModTime(modTime), fOptions(options), fOrdinalBase(ordinalBase), fFileContent(NULL), 
	fTableOfContents(NULL), fTableOfContentCount(0), fStringPool(NULL)
{
	fPath = strdup(path);
	fFileContent = fileContent;
	fFileLength = fileLength;

	if ( strncmp((const char*)fileContent, "!<arch>\n", 8) != 0 )
		throw "not an archive";

	// write out path for -whatsloaded option
	if ( options.fLogAllFiles )
		printf("%s\n", path);

	if ( !options.fFullyLoadArchives ) {
		const Entry* const firstMember = (Entry*)&fFileContent[8];
		if ( (strcmp(firstMember->getName(), SYMDEF_SORTED) == 0) || (strcmp(firstMember->getName(), SYMDEF) == 0) ) {
			const uint8_t* contents = firstMember->getContent();
			uint32_t ranlibArrayLen = E::get32(*((uint32_t*)contents));
			fTableOfContents = (const struct ranlib*)&contents[4];
			fTableOfContentCount = ranlibArrayLen / sizeof(struct ranlib);
			fStringPool = (const char*)&contents[ranlibArrayLen+8];
			if ( ((uint8_t*)(&fTableOfContents[fTableOfContentCount]) > &fileContent[fileLength])
				|| ((uint8_t*)fStringPool > &fileContent[fileLength]) )
				throw "malformed archive, perhaps wrong architecture";
			this->buildHashTable();
		}
		else
			throw "archive has no table of contents";
	}
}


template <typename A>
ObjectFile::Reader* Reader<A>::makeObjectReaderForMember(const Entry* member)
{
	const char* memberName = member->getName();
	char memberPath[strlen(fPath) + strlen(memberName)+4];
	strcpy(memberPath, fPath);
	strcat(memberPath, "(");
	strcat(memberPath, memberName);
	strcat(memberPath, ")");
	//fprintf(stderr, "using %s from %s\n", memberName, fPath);
	try {
		// offset the ordinals in this mach-o .o file, so that atoms layout in same order as in archive
		uint32_t ordinalBase = 	fOrdinalBase + (uint8_t*)member - fFileContent;
		return new typename mach_o::relocatable::Reader<A>::Reader(member->getContent(), memberPath, member->getModTime(), fOptions, ordinalBase);
	}
	catch (const char* msg) {
		throwf("in %s, %s", memberPath, msg);
	}
}


template <typename A>
std::vector<class ObjectFile::Atom*>&	Reader<A>::getAtoms()
{
	if ( fOptions.fFullyLoadArchives ) {
		// build vector of all atoms from all .o files in this archive
		const Entry* const start = (Entry*)&fFileContent[8];
		const Entry* const end = (Entry*)&fFileContent[fFileLength];
		for (const Entry* p=start; p < end; p = p->getNext()) {
			const char* memberName = p->getName();
			if ( (p==start) && ((strcmp(memberName, SYMDEF_SORTED) == 0) || (strcmp(memberName, SYMDEF) == 0)) )
				continue;
			if ( fOptions.fWhyLoad )
				printf("-all_load forced load of %s(%s)\n", this->getPath(), memberName);
			ObjectFile::Reader* r = this->makeObjectReaderForMember(p);
			std::vector<class ObjectFile::Atom*>&	atoms = r->getAtoms();
			fAllAtoms.insert(fAllAtoms.end(), atoms.begin(), atoms.end());
		}
		return fAllAtoms;
	}
	else if ( fOptions.fLoadAllObjcObjectsFromArchives ) {
		// build vector of all atoms from all .o files containing objc classes in this archive
		for(class NameToEntryMap::iterator it = fHashTable.begin(); it != fHashTable.end(); ++it) {
			if ( (strncmp(it->first, ".objc_c", 7) == 0) || (strncmp(it->first, "_OBJC_CLASS_$_", 14) == 0) ) {
				const Entry* member = (Entry*)&fFileContent[E::get32(it->second->ran_off)];
				if ( fInstantiatedEntries.count(member) == 0 ) {
					if ( fOptions.fWhyLoad )
						printf("-ObjC forced load of %s(%s)\n", this->getPath(), member->getName());
					// only return these atoms once
					fInstantiatedEntries.insert(member);
					ObjectFile::Reader* r = makeObjectReaderForMember(member);
					std::vector<class ObjectFile::Atom*>&	atoms = r->getAtoms();
					fAllAtoms.insert(fAllAtoms.end(), atoms.begin(), atoms.end());
				}
			}
		}
		return fAllAtoms;
	}
	else {
		// return nonthing for now, getJustInTimeAtomsFor() will return atoms as needed
		return fgEmptyList;
	}
}


template <typename A>
ConstRanLibPtr  Reader<A>::ranlibHashSearch(const char* name)
{
	class NameToEntryMap::iterator pos = fHashTable.find(name);
	if ( pos != fHashTable.end() )
		return pos->second;
	else
		return NULL;
}

template <typename A>
void Reader<A>::buildHashTable()
{
	// walk through list backwards, adding/overwriting entries
	// this assures that with duplicates those earliest in the list will be found
	for (int i = fTableOfContentCount-1; i >= 0; --i) {
		const struct ranlib* entry = &fTableOfContents[i];
		const char* entryName = &fStringPool[E::get32(entry->ran_un.ran_strx)];
		const Entry* member = (Entry*)&fFileContent[E::get32(entry->ran_off)];
		//fprintf(stderr, "adding hash %d, %s -> %p\n", i, entryName, entry);
		fHashTable[entryName] = entry;
		fPossibleEntries.insert(member);
	}
}

template <typename A>
void Reader<A>::dumpTableOfContents()
{
	for (unsigned int i=0; i < fTableOfContentCount; ++i) {
		const struct ranlib* e = &fTableOfContents[i];
		printf("%s in %s\n", &fStringPool[E::get32(e->ran_un.ran_strx)], ((Entry*)&fFileContent[E::get32(e->ran_off)])->getName());
	}
}

template <typename A>
std::vector<class ObjectFile::Atom*>* Reader<A>::getJustInTimeAtomsFor(const char* name)
{
	if ( fOptions.fFullyLoadArchives ) {
		return NULL;
	}
	else {
		const struct ranlib* result = NULL;
		// do a hash search of table of contents looking for requested symbol
		result = ranlibHashSearch(name);
		if ( result != NULL ) {
			const Entry* member = (Entry*)&fFileContent[E::get32(result->ran_off)];
			if ( fInstantiatedEntries.count(member) == 0 ) {
				if ( fOptions.fWhyLoad )
					printf("%s forced load of %s(%s)\n", name, this->getPath(), member->getName());
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





}; // namespace archive
}; // namespace mach_o


#endif // __OBJECT_FILE_ARCHIVE_MACH_O__
