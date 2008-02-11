/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-*
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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <mach/mach_time.h>
#include <mach/vm_statistics.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#include <mach-o/fat.h>
#include <dlfcn.h>

#include <string>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <list>
#include <algorithm>
#include <ext/hash_map>
#include <dlfcn.h>
#include <AvailabilityMacros.h>

#include "Options.h"

#include "ObjectFile.h"

#include "MachOReaderRelocatable.hpp"
#include "MachOReaderArchive.hpp"
#include "MachOReaderDylib.hpp"
#include "MachOWriterExecutable.hpp"

#define LLVM_SUPPORT 0
 
#if LLVM_SUPPORT
#include "LLVMReader.hpp"
#endif

#include "OpaqueSection.hpp"


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
	const char*		getName() { return fSectionName; }
private:
					Section(const char* sectionName, const char* segmentName, bool zeroFill);

	struct Sorter {
		static int	segmentOrdinal(const char* segName);
		bool operator()(Section* left, Section* right);
	};

	typedef __gnu_cxx::hash_map<const char*, uint32_t, __gnu_cxx::hash<const char*>, CStringEquals> NameToOrdinal;
	typedef __gnu_cxx::hash_map<const char*, class Section*, __gnu_cxx::hash<const char*>, CStringEquals> NameToSection;
	//typedef std::map<const char*, class Section*, CStringComparor> NameToSection;

	const char*		fSectionName;
	const char*		fSegmentName;
	bool			fZeroFill;

	static NameToSection			fgMapping;
	static std::vector<Section*>	fgSections;
	static NameToOrdinal			fgSegmentDiscoverOrder;
};

Section::NameToSection	Section::fgMapping;
std::vector<Section*>	Section::fgSections;
Section::NameToOrdinal	Section::fgSegmentDiscoverOrder;

Section::Section(const char* sectionName, const char* segmentName, bool zeroFill)
 : fSectionName(sectionName), fSegmentName(segmentName), fZeroFill(zeroFill)
{
	this->fIndex = fgSections.size();
	//fprintf(stderr, "new Section(%s, %s) => %p, %u\n", sectionName, segmentName, this, this->getIndex());
}

Section* Section::find(const char* sectionName, const char* segmentName, bool zeroFill)
{
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
	fgMapping[sectionName] = sect;
	fgSections.push_back(sect);

	if ( (strcmp(sectionName, "__text") == 0) && (strcmp(segmentName, "__TEXT") == 0) ) {
		// special case __textcoal_nt to be right after __text
		find("__textcoal_nt", "__TEXT", false);
	}

	// remember segment discovery order
	if ( fgSegmentDiscoverOrder.find(segmentName) == fgSegmentDiscoverOrder.end() ) 
		fgSegmentDiscoverOrder[segmentName] = fgSegmentDiscoverOrder.size();

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
	if ( strcmp(segName, "__OBJC2") == 0 )
		return 5;
	if ( strcmp(segName, "__LINKEDIT") == 0 )
		return INT_MAX;	// linkedit segment should always sort last
	else
		return fgSegmentDiscoverOrder[segName]+6;
}


bool Section::Sorter::operator()(Section* left, Section* right)
{
	// Segment is primary sort key
	int leftSegOrdinal = segmentOrdinal(left->fSegmentName);
	int rightSegOrdinal = segmentOrdinal(right->fSegmentName);
	if ( leftSegOrdinal < rightSegOrdinal )
		return true;
	if ( leftSegOrdinal > rightSegOrdinal )
		return false;

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
	//printf("unsorted sections:\n");
	//for (std::vector<Section*>::iterator it=fgSections.begin(); it != fgSections.end(); it++) {
	//	printf("section: name=%s, segment: name=%s, discovery order=%d\n", (*it)->fSectionName, (*it)->fSegmentName, (*it)->fIndex);
	//}

	// sort it
	std::sort(fgSections.begin(), fgSections.end(), Section::Sorter());

	// assign correct section ordering to each Section object
	unsigned int newOrder = 1;
	for (std::vector<Section*>::iterator it=fgSections.begin(); it != fgSections.end(); it++)
		(*it)->fIndex = newOrder++;

	//printf("sorted sections:\n");
	//for (std::vector<Section*>::iterator it=fgSections.begin(); it != fgSections.end(); it++) {
	//	printf("section: index=%d, obj=%p, name=%s\n", (*it)->fIndex, (*it), (*it)->fSectionName);
	//}
}

class Linker : public ObjectFile::Reader::DylibHander {
public:
						Linker(int argc, const char* argv[]);

	const char*			getArchPrefix();
	const char*			architectureName();
	bool				showArchitectureInErrors();
	bool				isInferredArchitecture();
	void				createReaders();
	void				createWriter();
	void				addInputFile(ObjectFile::Reader* reader, const Options::FileInfo& );
	void				setOutputFile(ExecutableFile::Writer* writer);
	void				link();
	void				optimize();
	
	// implemenation from ObjectFile::Reader::DylibHander
	virtual ObjectFile::Reader* findDylib(const char* installPath, const char* fromPath);

private:
	struct WhyLiveBackChain
	{
		WhyLiveBackChain*	previous;
		const char*			name;
	};

	ObjectFile::Reader*	createReader(const Options::FileInfo&);
	void				addAtom(ObjectFile::Atom& atom);
	void				addAtoms(std::vector<class ObjectFile::Atom*>& atoms);
	void				buildAtomList();
	void				processDylibs();
	void				updateContraints(ObjectFile::Reader* reader);
	void				loadAndResolve();
	void				processDTrace();
	void				checkObjC();
	void				loadUndefines();
	void				checkUndefines();
	void				addWeakAtomOverrides();
	void				resolveReferences();
	void				deadStripResolve();
	void				addLiveRoot(const char* name);
	ObjectFile::Atom*	findAtom(const Options::OrderedSymbol& pair);
	void				logArchive(ObjectFile::Reader* reader);
	void				sortSections();
	void				sortAtoms();
	void				tweakLayout();
	void				writeDotOutput();
	static bool			minimizeStab(ObjectFile::Reader::Stab& stab);
	static const char*	truncateStabString(const char* str);
	void				collectDebugInfo();
	void				writeOutput();
	ObjectFile::Atom*	entryPoint();
	ObjectFile::Atom*	dyldHelper();
	const char*			assureFullPath(const char* path);
	void				markLive(ObjectFile::Atom& atom, Linker::WhyLiveBackChain* previous);
	void				collectStabs(ObjectFile::Reader* reader, std::map<const class ObjectFile::Atom*, uint32_t>& atomOrdinals);
	void				synthesizeDebugNotes(std::vector<class ObjectFile::Atom*>& allAtomsByReader);
	void				printStatistics();
	void				printTime(const char* msg, uint64_t partTime, uint64_t totalTime);
	char*				commatize(uint64_t in, char* out);
	void				getVMInfo(vm_statistics_data_t& info);
	cpu_type_t			inferArchitecture();
	void 				addDtraceProbe(ObjectFile::Atom& atom, uint32_t offsetInAtom, const char* probeName);
	void				checkDylibClientRestrictions(ObjectFile::Reader* reader);
	void				logDylib(ObjectFile::Reader* reader, bool indirect);
	
	void									resolve(ObjectFile::Reference* reference);
	void									resolveFrom(ObjectFile::Reference* reference);
	std::vector<class ObjectFile::Atom*>*	addJustInTimeAtoms(const char* name);
	void									addJustInTimeAtomsAndMarkLive(const char* name);

	ObjectFile::Reader*	addDylib(ObjectFile::Reader* reader, const Options::FileInfo& info, uint64_t mappedLen);
	ObjectFile::Reader*	addObject(ObjectFile::Reader* reader, const Options::FileInfo& info, uint64_t mappedLen);
	ObjectFile::Reader*	addArchive(ObjectFile::Reader* reader, const Options::FileInfo& info, uint64_t mappedLen);

	void				logTraceInfo(const char* format, ...);


	class SymbolTable
	{
	public:
							SymbolTable(Linker&);
		void				require(const char* name);
		bool				add(ObjectFile::Atom& atom);
		ObjectFile::Atom*	find(const char* name);
		unsigned int		getRequireCount() { return fRequireCount; }
		void				getNeededNames(bool andWeakDefintions, std::vector<const char*>& undefines);
		typedef __gnu_cxx::hash_map<const char*, ObjectFile::Atom*, __gnu_cxx::hash<const char*>, CStringEquals> Mapper;
	private:
		Linker&				fOwner;
		Mapper				fTable;
		unsigned int		fRequireCount;
	};

	class AtomSorter
	{
	public:
		AtomSorter(std::map<const ObjectFile::Atom*, uint32_t>* map) : fOverriddenOrdinalMap(map) {}
		bool operator()(const ObjectFile::Atom* left, const ObjectFile::Atom* right);
	private:
		std::map<const ObjectFile::Atom*, uint32_t>*	fOverriddenOrdinalMap;
	};

	typedef std::map<const char*, uint32_t, CStringComparor> SectionOrder;

	struct DTraceProbeInfo {
		DTraceProbeInfo(const ObjectFile::Atom* a, uint32_t o, const char* n) : atom(a), offset(o), probeName(n) {}
		const ObjectFile::Atom*			atom;
		uint32_t						offset;
		const char*						probeName;
	};
	typedef __gnu_cxx::hash_map<const char*, std::vector<DTraceProbeInfo>, __gnu_cxx::hash<const char*>, CStringEquals>	ProviderToProbes;
	typedef	__gnu_cxx::hash_set<const char*, __gnu_cxx::hash<const char*>, CStringEquals>  CStringSet;
	typedef __gnu_cxx::hash_map<const char*, ObjectFile::Reader*, __gnu_cxx::hash<const char*>, CStringEquals>	InstallNameToReader;

	struct IndirectLibrary {
		const char*							path;
		uint64_t							fileLen;
		ObjectFile::Reader*					reader;
		std::set<ObjectFile::Reader*>		parents;
		ObjectFile::Reader*					reExportedViaDirectLibrary;
	};

	ObjectFile::Reader* findDirectLibraryWhichReExports(struct IndirectLibrary& indirectLib);

	Options												fOptions;
	SymbolTable											fGlobalSymbolTable;
	uint32_t											fNextInputOrdinal;
	std::vector<class ObjectFile::Reader*>				fInputFiles;
	ExecutableFile::Writer*								fOutputFile;
	InstallNameToReader									fDylibMap;
	std::map<ObjectFile::Reader*,DynamicLibraryOptions> fDylibOptionsMap;
	std::set<ObjectFile::Reader*>						fDylibsProcessed;
	ObjectFile::Reader*									fBundleLoaderReader;
	std::vector<class ObjectFile::Reader*>				fReadersThatHaveSuppliedAtoms;
	std::vector<class ObjectFile::Atom*>				fAllAtoms;
	std::set<class ObjectFile::Reader*>					fArchiveReaders;
	std::set<class ObjectFile::Reader*>					fArchiveReadersLogged;
	std::set<class ObjectFile::Atom*>					fDeadAtoms;
	std::set<ObjectFile::Atom*>							fLiveAtoms;
	std::set<ObjectFile::Atom*>							fLiveRootAtoms;
	std::vector<class ObjectFile::Reader::Stab>			fStabs;
	std::vector<class ObjectFile::Atom*>				fAtomsWithUnresolvedReferences;
	std::vector<DTraceProbeInfo>						fDtraceProbes;
	std::vector<DTraceProbeInfo>						fDtraceProbeSites;
	std::vector<DTraceProbeInfo>						fDtraceIsEnabledSites;
	std::map<const ObjectFile::Atom*,CStringSet>		fDtraceAtomToTypes;
	bool												fCreateUUID;
	bool												fCanScatter;
	SectionOrder										fSectionOrder;
	cpu_type_t											fArchitecture;
	const char*											fArchitectureName;
	bool												fArchitectureInferred;
	bool												fDirectLibrariesComplete;
	bool												fBiggerThanTwoGigOutput;
	uint64_t											fOutputFileSize;
	uint64_t											fTotalZeroFillSize;
	uint64_t											fTotalSize;
	uint64_t											fStartTime;
	uint64_t											fStartCreateReadersTime;
	uint64_t											fStartCreateWriterTime;
	uint64_t											fStartBuildAtomsTime;
	uint64_t											fStartLoadAndResolveTime;
	uint64_t											fStartSortTime;
	uint64_t											fStartDebugTime;
	uint64_t											fStartWriteTime;
	uint64_t											fEndTime;
	uint64_t											fTotalObjectSize;
	uint64_t											fTotalArchiveSize;
	uint32_t											fTotalObjectLoaded;
	uint32_t											fTotalArchivesLoaded;
	uint32_t											fTotalDylibsLoaded;
	vm_statistics_data_t								fStartVMInfo;
	ObjectFile::Reader::ObjcConstraint					fCurrentObjCConstraint;
	ObjectFile::Reader::CpuConstraint					fCurrentCpuConstraint;
	bool												fObjcReplacmentClasses;
	bool												fAllDirectDylibsLoaded;
};


Linker::Linker(int argc, const char* argv[])
	: fOptions(argc, argv), fGlobalSymbolTable(*this), fNextInputOrdinal(1), fOutputFile(NULL), fBundleLoaderReader(NULL), 
	  fCreateUUID(false), fCanScatter(true),
	  fArchitecture(0), fArchitectureInferred(false), fDirectLibrariesComplete(false), fBiggerThanTwoGigOutput(false),
	  fOutputFileSize(0), fTotalZeroFillSize(0), fTotalSize(0), fTotalObjectSize(0),
	  fTotalArchiveSize(0),  fTotalObjectLoaded(0), fTotalArchivesLoaded(0), fTotalDylibsLoaded(0),
	  fCurrentObjCConstraint(ObjectFile::Reader::kObjcNone), fCurrentCpuConstraint(ObjectFile::Reader::kCpuAny),
	  fObjcReplacmentClasses(false), fAllDirectDylibsLoaded(false)
{
	fStartTime = mach_absolute_time();
	if ( fOptions.printStatistics() )
		getVMInfo(fStartVMInfo);

	fArchitecture = fOptions.architecture();
	if ( fArchitecture == 0 ) {
		// -arch not specified, scan .o files to figure out what it should be
		fArchitecture = inferArchitecture();
		fArchitectureInferred = true;
	}
	switch (fArchitecture) {
		case CPU_TYPE_POWERPC:
			fArchitectureName = "ppc";
			break;
		case CPU_TYPE_POWERPC64:
			fArchitectureName = "ppc64";
			break;
		case CPU_TYPE_I386:
			fArchitectureName = "i386";
			break;
		case CPU_TYPE_X86_64:
			fArchitectureName = "x86_64";
			break;
		default:
			fArchitectureName = "unknown architecture";
			break;
	}
}

const char*	Linker::architectureName()
{
	return fArchitectureName;
}

bool Linker::showArchitectureInErrors()
{
	return fOptions.printArchPrefix();
}

bool Linker::isInferredArchitecture()
{
	return fArchitectureInferred;
}

cpu_type_t Linker::inferArchitecture()
{
	// scan all input files, looking for a thin .o file.
	// the first one found is presumably the architecture to link
	uint8_t buffer[sizeof(mach_header_64)];
	std::vector<Options::FileInfo>& files = fOptions.getInputFiles();
	for (std::vector<Options::FileInfo>::iterator it = files.begin(); it != files.end(); ++it) {
		int fd = ::open(it->path, O_RDONLY, 0);
		if ( fd != -1 ) {
			ssize_t amount = read(fd, buffer, sizeof(buffer));
			::close(fd);
			if ( amount >= (ssize_t)sizeof(buffer) ) {
				if ( mach_o::relocatable::Reader<ppc>::validFile(buffer) ) {
					//fprintf(stderr, "ld: warning -arch not used, infering -arch ppc based on %s\n", it->path);
					return CPU_TYPE_POWERPC;
				}
				else if ( mach_o::relocatable::Reader<ppc64>::validFile(buffer) ) {
					//fprintf(stderr, "ld: warning -arch not used, infering -arch ppc64 based on %s\n", it->path);
					return CPU_TYPE_POWERPC64;
				}
				else if ( mach_o::relocatable::Reader<x86>::validFile(buffer) ) {
					//fprintf(stderr, "ld: warning -arch not used, infering -arch i386 based on %s\n", it->path);
					return CPU_TYPE_I386;
				}
				else if ( mach_o::relocatable::Reader<x86_64>::validFile(buffer) ) {
					//fprintf(stderr, "ld: warning -arch not used, infering -arch x86_64 based on %s\n", it->path);
					return CPU_TYPE_X86_64;
				}
			}
		}
	}

	// no thin .o files found, so default to same architecture this was built as
	fprintf(stderr, "ld: warning -arch not specified\n");
#if __ppc__
	return CPU_TYPE_POWERPC;
#elif __i386__
	return CPU_TYPE_I386;
#elif __ppc64__
	return CPU_TYPE_POWERPC64;
#elif __x86_64__
	return CPU_TYPE_X86_64;
#else
	#error unknown default architecture
#endif
}


void Linker::addInputFile(ObjectFile::Reader* reader, const Options::FileInfo& info)
{
	fInputFiles.push_back(reader);
	fDylibOptionsMap[reader] = info.options;
}

void Linker::setOutputFile(ExecutableFile::Writer* writer)
{
	fOutputFile = writer;
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

void Linker::loadAndResolve()
{
	fStartLoadAndResolveTime = mach_absolute_time();
	if ( fOptions.deadStrip() == Options::kDeadStripOff ) {
		// without dead-code-stripping:
		// find atoms to resolve all undefines
		this->loadUndefines();
		// verify nothing is missing
		this->checkUndefines();
		// once all undefines fulfill, then bind all references
		this->resolveReferences();
		// remove atoms weak atoms that have been overridden
		fAllAtoms.erase(std::remove_if(fAllAtoms.begin(), fAllAtoms.end(), InSet(fDeadAtoms)), fAllAtoms.end());
	}
	else {
		// with dead code stripping:
		// start binding references from roots,
		this->deadStripResolve();
		// verify nothing is missing
		this->checkUndefines();
	}
}

void Linker::optimize()
{
	std::vector<class ObjectFile::Atom*> newAtoms;

	const int readerCount = fInputFiles.size();
	for (int i=0; i < readerCount; ++i) {
		fInputFiles[i]->optimize(fAllAtoms, newAtoms, fNextInputOrdinal);
	}
	// note: When writer start generating stubs and non-lazy-pointers for all architecture, do not insert
	// newAtoms into fGlobalSymbolTable. Instead directly insert them in fAllAtoms and set their order appropriately.
	this->addAtoms(newAtoms);

	// Some of the optimized atoms may not have identified section properly, if they
	// were created before optimizer produces corrosponding real atom. Here, input
	// file readers are not able to patch it themselves because Section::find() is
	// linker specific.
	for(std::vector<class ObjectFile::Atom*>::iterator itr = fAllAtoms.begin();
		itr != fAllAtoms.end(); ++itr) {

		ObjectFile::Atom *atom = *itr;
		if (atom->getSectionName() && !atom->getSection())
			atom->setSection(Section::find(atom->getSectionName(), atom->getSegment().getName(), atom->isZeroFill()));
	}

	if ( fOptions.deadStrip() != Options::kDeadStripOff ) {
		fLiveAtoms.clear();
        deadStripResolve();
    }
    else
        resolveReferences();
}

void Linker::link()
{
	this->buildAtomList();
	this->loadAndResolve();
	this->optimize();
	this->checkObjC();
	this->processDTrace();
	this->tweakLayout();
	this->sortSections();
	this->sortAtoms();
	this->writeDotOutput();
	this->collectDebugInfo();
	this->writeOutput();
	this->printStatistics();

	if ( fOptions.pauseAtEnd() )
		sleep(10);
}

void Linker::printTime(const char* msg, uint64_t partTime, uint64_t totalTime)
{
	static uint64_t sUnitsPerSecond = 0;
	if ( sUnitsPerSecond == 0 ) {
		struct mach_timebase_info timeBaseInfo;
		if ( mach_timebase_info(&timeBaseInfo) == KERN_SUCCESS ) {
			sUnitsPerSecond = 1000000000ULL * timeBaseInfo.denom / timeBaseInfo.numer;
			//fprintf(stderr, "sUnitsPerSecond=%llu\n", sUnitsPerSecond);
		}
	}
	if ( partTime < sUnitsPerSecond ) {
		uint32_t milliSecondsTimeTen = (partTime*10000)/sUnitsPerSecond;
		uint32_t milliSeconds = milliSecondsTimeTen/10;
		uint32_t percentTimesTen = (partTime*1000)/totalTime;
		uint32_t percent = percentTimesTen/10;
		fprintf(stderr, "%s: %u.%u milliseconds (%u.%u%%)\n", msg, milliSeconds, milliSecondsTimeTen-milliSeconds*10, percent, percentTimesTen-percent*10);
	}
	else {
		uint32_t secondsTimeTen = (partTime*10)/sUnitsPerSecond;
		uint32_t seconds = secondsTimeTen/10;
		uint32_t percentTimesTen = (partTime*1000)/totalTime;
		uint32_t percent = percentTimesTen/10;
		fprintf(stderr, "%s: %u.%u seconds (%u.%u%%)\n", msg, seconds, secondsTimeTen-seconds*10, percent, percentTimesTen-percent*10);
	}
}

char* Linker::commatize(uint64_t in, char* out)
{
	char* result = out;
	char rawNum[30];
	sprintf(rawNum, "%llu", in);
	const int rawNumLen = strlen(rawNum);
	for(int i=0; i < rawNumLen-1; ++i) {
		*out++ = rawNum[i];
		if ( ((rawNumLen-i) % 3) == 1 )
			*out++ = ',';
	}
	*out++ = rawNum[rawNumLen-1];
	*out = '\0';
	return result;
}

void Linker::getVMInfo(vm_statistics_data_t& info)
{
	mach_msg_type_number_t count = sizeof(vm_statistics_data_t) / sizeof(natural_t);
	kern_return_t error = host_statistics(mach_host_self(), HOST_VM_INFO,
							(host_info_t)&info, &count);
	if (error != KERN_SUCCESS) {
		bzero(&info, sizeof(vm_statistics_data_t));
	}
}

void Linker::printStatistics()
{
	fEndTime = mach_absolute_time();
	if ( fOptions.printStatistics() ) {
		vm_statistics_data_t endVMInfo;
		getVMInfo(endVMInfo);

		uint64_t totalTime = fEndTime - fStartTime;
		printTime("ld total time", totalTime, totalTime);
		printTime(" option parsing time",	fStartCreateReadersTime -	fStartTime,					totalTime);
		printTime(" object file processing",fStartCreateWriterTime -	fStartCreateReadersTime,	totalTime);
		printTime(" output file setup",		fStartBuildAtomsTime -		fStartCreateWriterTime,		totalTime);
		printTime(" build atom list",		fStartLoadAndResolveTime -	fStartBuildAtomsTime,		totalTime);
		printTime(" resolve references",	fStartSortTime -			fStartLoadAndResolveTime,	totalTime);
		printTime(" sort output",			fStartDebugTime -			fStartSortTime,				totalTime);
		printTime(" process debug info",	fStartWriteTime -			fStartDebugTime,			totalTime);
		printTime(" write output",			fEndTime -					fStartWriteTime,			totalTime);
		fprintf(stderr, "pageins=%u, pageouts=%u, faults=%u\n", endVMInfo.pageins-fStartVMInfo.pageins,
										endVMInfo.pageouts-fStartVMInfo.pageouts, endVMInfo.faults-fStartVMInfo.faults);
		char temp[40];
		fprintf(stderr, "processed %3u object files,  totaling %15s bytes\n", fTotalObjectLoaded, commatize(fTotalObjectSize, temp));
		fprintf(stderr, "processed %3u archive files, totaling %15s bytes\n", fTotalArchivesLoaded, commatize(fTotalArchiveSize, temp));
		fprintf(stderr, "processed %3u dylib files\n", fTotalDylibsLoaded);
		fprintf(stderr, "wrote output file            totaling %15s bytes\n", commatize(fOutputFileSize, temp));
	}
}

inline void Linker::addAtom(ObjectFile::Atom& atom)
{
	// add to list of all atoms
	fAllAtoms.push_back(&atom);

	if ( fOptions.deadStrip() == Options::kDeadStripOff ) {
		// not dead-stripping code, so add atom's references's names to symbol table as to-be-resolved-later
		std::vector<class ObjectFile::Reference*>& references = atom.getReferences();
		for (std::vector<ObjectFile::Reference*>::iterator it=references.begin(); it != references.end(); it++) {
			ObjectFile::Reference* reference = *it;
			if ( reference->getTargetBinding() == ObjectFile::Reference::kUnboundByName )
				fGlobalSymbolTable.require(reference->getTargetName());
			if ( reference->getFromTargetBinding() == ObjectFile::Reference::kUnboundByName )
				fGlobalSymbolTable.require(reference->getFromTargetName());
			if ( reference->getTargetBinding() == ObjectFile::Reference::kDontBind )
				addDtraceProbe(atom, reference->getFixUpOffset(), reference->getTargetName());
		}
		// update total size info (except for __ZEROPAGE atom)
		if ( atom.getSegment().isContentReadable() ) {
			fTotalSize += atom.getSize();
			if ( atom.isZeroFill() )
				fTotalZeroFillSize += atom.getSize();
		}
	}
	else {
		if ( atom.dontDeadStrip() )
			fLiveRootAtoms.insert(&atom);
	}

	// if in global namespace, add atom itself to symbol table
	ObjectFile::Atom::Scope scope = atom.getScope();
	const char* name = atom.getName();
	if ( (scope != ObjectFile::Atom::scopeTranslationUnit) && (name != NULL) ) {
		// update scope based on export list (possible that globals are downgraded to private_extern)
		if ( (scope == ObjectFile::Atom::scopeGlobal) && fOptions.hasExportRestrictList() ) {
			bool doExport = fOptions.shouldExport(name);
			if ( !doExport ) {
				atom.setScope(ObjectFile::Atom::scopeLinkageUnit);
			}
		}
		// add to symbol table
		fGlobalSymbolTable.add(atom);
	}

	// record section orders so output file can have same order
	if (atom.getSectionName())
		atom.setSection(Section::find(atom.getSectionName(), atom.getSegment().getName(), atom.isZeroFill()));
}

void Linker::updateContraints(ObjectFile::Reader* reader)
{
	// check objc objects were compiled compatibly
	ObjectFile::Reader::ObjcConstraint objcAddition = reader->getObjCConstraint();
	if ( reader->getInstallPath() == NULL ) {
		// adding a .o file
		switch ( fCurrentObjCConstraint ) {
			case ObjectFile::Reader::kObjcNone:
				fCurrentObjCConstraint = objcAddition;
				break;
			case ObjectFile::Reader::kObjcRetainRelease:
			case ObjectFile::Reader::kObjcRetainReleaseOrGC:
			case ObjectFile::Reader::kObjcGC:
				if ( (fCurrentObjCConstraint != objcAddition) && (objcAddition != ObjectFile::Reader::kObjcNone) ) 
					throwf("%s built with different Garbage Collection settings", reader->getPath());
				break;
		}
	}
	if ( reader->objcReplacementClasses() )
		fObjcReplacmentClasses = true;

	// check cpu sub-types
	ObjectFile::Reader::CpuConstraint  cpuAddition = reader->getCpuConstraint();
	switch ( fCurrentCpuConstraint ) {
		case ObjectFile::Reader::kCpuAny:
			fCurrentCpuConstraint = cpuAddition;
			break;
		case ObjectFile::Reader::kCpuG3:
			switch ( cpuAddition ) {
				case ObjectFile::Reader::kCpuAny:
				case ObjectFile::Reader::kCpuG3:
					break;
				case ObjectFile::Reader::kCpuG4:
				case ObjectFile::Reader::kCpuG5:
					// previous file for G3 this one is more contrained, use it
					fCurrentCpuConstraint = cpuAddition;
					break;
			}
			break;
		case ObjectFile::Reader::kCpuG4:
			switch ( cpuAddition ) {
				case ObjectFile::Reader::kCpuAny:
				case ObjectFile::Reader::kCpuG3:
				case ObjectFile::Reader::kCpuG4:
					break;
				case ObjectFile::Reader::kCpuG5:
					// previous file for G5 this one is more contrained, use it
					fCurrentCpuConstraint = cpuAddition;
					break;
			}
			break;
		case ObjectFile::Reader::kCpuG5:
			// G5 can run everything
			break;
	}
}

inline void Linker::addAtoms(std::vector<class ObjectFile::Atom*>& atoms)
{
	bool scanAll = fOptions.readerOptions().fFullyLoadArchives || fOptions.readerOptions().fLoadAllObjcObjectsFromArchives;
	bool first = true; 
	for (std::vector<ObjectFile::Atom*>::iterator it=atoms.begin(); it != atoms.end(); it++) {
		// usually we only need to get the first atom's reader, but
		// with -all_load all atoms from all .o files come come back together
		// so we need to scan all atoms
		if ( first || scanAll ) {
			// update fReadersThatHaveSuppliedAtoms
			ObjectFile::Reader* reader = (*it)->getFile();
			if ( std::find(fReadersThatHaveSuppliedAtoms.begin(), fReadersThatHaveSuppliedAtoms.end(), reader)
					== fReadersThatHaveSuppliedAtoms.end() ) {
				fReadersThatHaveSuppliedAtoms.push_back(reader);
				updateContraints(reader);				
			}	
		}
		this->addAtom(**it);
		first = false;
	}
}

void Linker::logArchive(ObjectFile::Reader* reader)
{
	if ( (fArchiveReaders.count(reader) != 0) && (fArchiveReadersLogged.count(reader) == 0) ) {
		fArchiveReadersLogged.insert(reader);
		const char* fullPath = reader->getPath();
		char realName[MAXPATHLEN];
		if ( realpath(fullPath, realName) != NULL )
			fullPath = realName;
		logTraceInfo("[Logging for XBS] Used static archive: %s\n", fullPath);
	}
}


void Linker::buildAtomList()
{
	fStartBuildAtomsTime = mach_absolute_time();
	// add initial undefines from -u option
	std::vector<const char*>& initialUndefines = fOptions.initialUndefines();
	for (std::vector<const char*>::iterator it=initialUndefines.begin(); it != initialUndefines.end(); it++) {
		fGlobalSymbolTable.require(*it);
	}

	// writer can contribute atoms
	this->addAtoms(fOutputFile->getAtoms());

	// each reader contributes atoms
	for (std::vector<class ObjectFile::Reader*>::iterator it=fInputFiles.begin(); it != fInputFiles.end(); it++) {
		ObjectFile::Reader* reader = *it;
		std::vector<class ObjectFile::Atom*>& atoms = reader->getAtoms();
		this->addAtoms(atoms);
		if ( fOptions.readerOptions().fTraceArchives && (atoms.size() != 0) ) 
			logArchive(reader);
	}

	// extra command line section always at end
	std::vector<Options::ExtraSection>& extraSections = fOptions.extraSections();
	for( std::vector<Options::ExtraSection>::iterator it=extraSections.begin(); it != extraSections.end(); ++it) {
		this->addAtoms((new opaque_section::Reader(it->segmentName, it->sectionName, it->path, it->data, it->dataLen, fNextInputOrdinal))->getAtoms());
		fNextInputOrdinal += it->dataLen;
	}
}

static const char* pathLeafName(const char* path)
{
	const char* shortPath = strrchr(path, '/');
	if ( shortPath == NULL )
		return path;
	else
		return &shortPath[1];
}

void Linker::loadUndefines()
{
	// keep looping until no more undefines were added in last loop
	unsigned int undefineCount = 0xFFFFFFFF;
	while ( undefineCount != fGlobalSymbolTable.getRequireCount() ) {
		undefineCount = fGlobalSymbolTable.getRequireCount();
		std::vector<const char*> undefineNames;
		fGlobalSymbolTable.getNeededNames(false, undefineNames);
		for(std::vector<const char*>::iterator it = undefineNames.begin(); it != undefineNames.end(); ++it) {
			const char* name = *it;
			ObjectFile::Atom* possibleAtom = fGlobalSymbolTable.find(name);
			if ( (possibleAtom == NULL)
			  || ((possibleAtom->getDefinitionKind()==ObjectFile::Atom::kWeakDefinition) 
			  && (fOptions.outputKind() != Options::kObjectFile) 
			  && (possibleAtom->getScope() == ObjectFile::Atom::scopeGlobal)) ) {
				std::vector<class ObjectFile::Atom*>* atoms = this->addJustInTimeAtoms(name);
				if ( atoms != NULL ) 
					delete atoms;
			}
		}
	}
}

// temp hack for rdar://problem/4718189 map ObjC class names to new runtime names
class ExportedObjcClass
{
public:
	ExportedObjcClass(Options& opt) : fOptions(opt)  {}

	bool operator()(const char* name) const {
		if ( fOptions.shouldExport(name) ) {
			if ( strncmp(name, ".objc_class_name_", 17) == 0 )
				return true;
			if ( strncmp(name, "_OBJC_CLASS_$_", 14) == 0 )
				return true;
			if ( strncmp(name, "_OBJC_METACLASS_$_", 18) == 0 )
				return true;
		}
		//fprintf(stderr, "%s is not exported\n", name);
		return false;
	}
private:
	Options& fOptions;
};


void Linker::checkUndefines()
{
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

	// temp hack for rdar://problem/4718189 map ObjC class names to new runtime names
	// ignore unresolved references to Objc class names that are listed in -exported_symbols_list
	if ( fOptions.hasExportRestrictList() )
		unresolvableUndefines.erase(std::remove_if(unresolvableUndefines.begin(), unresolvableUndefines.end(), ExportedObjcClass(fOptions)), unresolvableUndefines.end());

	const int unresolvableCount = unresolvableUndefines.size();
	int unresolvableExportsCount  = 0;
	if ( unresolvableCount != 0 ) {
		if ( doPrint ) {
			if ( fOptions.printArchPrefix() )
				fprintf(stderr, "Undefined symbols for architecture %s:\n", fArchitectureName);
			else
				fprintf(stderr, "Undefined symbols:\n");
			for (int i=0; i < unresolvableCount; ++i) {
				const char* name = unresolvableUndefines[i];
				fprintf(stderr, "  \"%s\", referenced from:\n", name);
				// scan all atoms for references
				bool foundAtomReference = false;
				for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms.begin(); it != fAllAtoms.end(); it++) {
					ObjectFile::Atom* atom = *it;
					std::vector<class ObjectFile::Reference*>& references = atom->getReferences();
					for (std::vector<ObjectFile::Reference*>::iterator rit=references.begin(); rit != references.end(); rit++) {
						ObjectFile::Reference* reference = *rit;
						if ( reference->getTargetBinding() == ObjectFile::Reference::kUnboundByName ) {
							if ( strcmp(reference->getTargetName(), name) == 0 ) {
								fprintf(stderr, "      %s in %s\n", atom->getDisplayName(), pathLeafName(atom->getFile()->getPath()));
								foundAtomReference = true;
							}
						}
						if ( reference->getFromTargetBinding() == ObjectFile::Reference::kUnboundByName ) {
							if ( strcmp(reference->getFromTargetName(), name) == 0 ) {
								fprintf(stderr, "      %s in %s\n", atom->getDisplayName(), pathLeafName(atom->getFile()->getPath()));
								foundAtomReference = true;
							}
						}
					}
				}
				// scan command line options
				if  ( !foundAtomReference && fOptions.hasExportRestrictList() && fOptions.shouldExport(name) ) {
					fprintf(stderr, "     -exported_symbols_list command line option\n");
					++unresolvableExportsCount;
				}
			}
		}
		if ( doError ) 
			throw "symbol(s) not found";
	}
}



std::vector<class ObjectFile::Atom*>* Linker::addJustInTimeAtoms(const char* name)
{
	// when creating final linked image, writer gets first chance
	if ( fOptions.outputKind() != Options::kObjectFile ) {
		std::vector<class ObjectFile::Atom*>* atoms = fOutputFile->getJustInTimeAtomsFor(name);
		if ( atoms != NULL ) {
			this->addAtoms(*atoms);
			//fprintf(stderr, "addJustInTimeAtoms(%s) => found in file %s\n", name, fOutputFile->getPath() );
			return atoms;  // found a definition, no need to search anymore
		}
	}

	// give readers a chance
	for (std::vector<class ObjectFile::Reader*>::iterator it=fInputFiles.begin(); it != fInputFiles.end(); it++) {
		ObjectFile::Reader* reader = *it;
		if ( reader != NULL ) {
			// if this reader is a static archive that has the symbol we need, pull in all atoms in that module
			// if this reader is a dylib that exports the symbol we need, have it synthesize an atom for us.
			//fprintf(stderr, "addJustInTimeAtoms(%s), looking in reader %s\n", name, reader->getPath() );
			std::vector<class ObjectFile::Atom*>* atoms = reader->getJustInTimeAtomsFor(name);
			if ( atoms != NULL ) {
				this->addAtoms(*atoms);
				//fprintf(stderr, "addJustInTimeAtoms(%s) => found in file %s\n", name, reader->getPath() );
				if ( fOptions.readerOptions().fTraceArchives ) {
					logArchive(reader);
				}
				// if this is a weak definition in a dylib
				if ( (atoms->size() == 1) && (reader->getInstallPath() != NULL) && (atoms->at(0)->getDefinitionKind() == ObjectFile::Atom::kExternalWeakDefinition) ) {
					// keep looking for a non-weak definition
				}
				else {
					// found a definition, no need to search anymore
					return atoms;  
				}
			}
		}
	}

	// for two level namesapce, give all implicitly link dylibs a chance
	if ( fOptions.nameSpace() == Options::kTwoLevelNameSpace ) {
		for (InstallNameToReader::iterator it=fDylibMap.begin(); it != fDylibMap.end(); it++) {
			if ( it->second->implicitlyLinked() ) {
				//fprintf(stderr, "addJustInTimeAtoms(%s), looking in implicitly linked %s\n", name, it->second->getPath() );
				std::vector<class ObjectFile::Atom*>* atoms = it->second->getJustInTimeAtomsFor(name);
				if ( atoms != NULL ) {
					this->addAtoms(*atoms);
					//fprintf(stderr, "addJustInTimeAtoms(%s) => found in file %s\n", name, reader->getPath() );
					// if this is a weak definition in a dylib
					if ( (atoms->size() == 1) && (atoms->at(0)->getDefinitionKind() == ObjectFile::Atom::kExternalWeakDefinition) ) {
						// keep looking for a non-weak definition
					}
					else {
						// found a definition, no need to search anymore
						return atoms;  
					}
				}
			}
		}
	}

	// for flat namespace, give indirect dylibs
	if ( fOptions.nameSpace() != Options::kTwoLevelNameSpace ) {
		for (InstallNameToReader::iterator it=fDylibMap.begin(); it != fDylibMap.end(); it++) {
			if ( ! it->second->explicitlyLinked() ) {
				std::vector<class ObjectFile::Atom*>* atoms = it->second->getJustInTimeAtomsFor(name);
				if ( atoms != NULL ) {
					this->addAtoms(*atoms);
					//fprintf(stderr, "addJustInTimeAtoms(%s) => found in file %s\n", name, reader->getPath() );
					return atoms;  // found a definition, no need to search anymore
				}
			}
		}
	}

	// when creating .o file, writer goes last (this is so any static archives will be searched above)
	if (	(fOptions.outputKind() == Options::kObjectFile) 
		||  (fOptions.undefinedTreatment() != Options::kUndefinedError)
		||	fOptions.someAllowedUndefines() ) {
		ObjectFile::Atom* atom = fOutputFile->getUndefinedProxyAtom(name);
		if ( atom != NULL ) {
			this->addAtom(*atom);
			return NULL;
		}
	}
	//fprintf(stderr, "addJustInTimeAtoms(%s) => not found\n", name);
	return NULL;
}

void Linker::resolve(ObjectFile::Reference* reference)
{
	// look in global symbol table
	const char* targetName = reference->getTargetName();
	ObjectFile::Atom* target = fGlobalSymbolTable.find(targetName);
	if ( target == NULL ) {
		fprintf(stderr, "Undefined symbol: %s\n", targetName);
	}
	reference->setTarget(*target, reference->getTargetOffset());
}

void Linker::resolveFrom(ObjectFile::Reference* reference)
{
	// handle references that have two (from and to) targets
	const char* fromTargetName = reference->getFromTargetName();
	ObjectFile::Atom* fromTarget = fGlobalSymbolTable.find(fromTargetName);
	if ( fromTarget == NULL ) {
		fprintf(stderr, "Undefined symbol: %s\n", fromTargetName);
	}
	reference->setFromTarget(*fromTarget);
}


void Linker::resolveReferences()
{
	// note: the atom list may grow during this loop as libraries supply needed atoms
	for (unsigned int j=0; j < fAllAtoms.size(); ++j) {
		ObjectFile::Atom* atom = fAllAtoms[j];
		std::vector<class ObjectFile::Reference*>& references = atom->getReferences();
		for (std::vector<ObjectFile::Reference*>::iterator it=references.begin(); it != references.end(); it++) {
			ObjectFile::Reference* reference = *it;
			if ( reference->getTargetBinding() == ObjectFile::Reference::kUnboundByName )
				this->resolve(reference);
			if ( reference->getFromTargetBinding() == ObjectFile::Reference::kUnboundByName )
				this->resolveFrom(reference);
		}
	}
}


// used to remove stabs associated with atoms that won't be in output file
class NotInSet
{
public:
	NotInSet(std::set<ObjectFile::Atom*>& theSet) : fSet(theSet) {}

	bool operator()(const ObjectFile::Reader::Stab& stab) const {
		if ( stab.atom == NULL )
			return false;	// leave stabs that are not associated with any atome
		else
			return ( fSet.count(stab.atom) == 0 );
	}

private:
	std::set<ObjectFile::Atom*>& fSet;
};


class NotLive
{
public:
	NotLive(std::set<ObjectFile::Atom*>& set) : fLiveAtoms(set)  {}

	bool operator()(ObjectFile::Atom*& atom) const {
		//if ( fLiveAtoms.count(atom) == 0 )
		//	fprintf(stderr, "dead strip %s\n", atom->getDisplayName());
		return ( fLiveAtoms.count(atom) == 0 );
	}
private:
	std::set<ObjectFile::Atom*>& fLiveAtoms;
};


void Linker::addJustInTimeAtomsAndMarkLive(const char* name)
{
	std::vector<class ObjectFile::Atom*>* atoms = this->addJustInTimeAtoms(name);
	if ( atoms != NULL ) {
		if ( fOptions.allGlobalsAreDeadStripRoots() ) {
			for (std::vector<ObjectFile::Atom*>::iterator it=atoms->begin(); it != atoms->end(); it++) {
				ObjectFile::Atom* atom = *it;
				if ( atom->getScope() ==  ObjectFile::Atom::scopeGlobal ) {
					WhyLiveBackChain rootChain;
					rootChain.previous = NULL;
					rootChain.name = atom->getDisplayName();
					this->markLive(*atom, &rootChain);
				}
			}
		}
		delete atoms;
	}
}

void Linker::markLive(ObjectFile::Atom& atom, struct Linker::WhyLiveBackChain* previous)
{
	if ( fLiveAtoms.count(&atom) == 0 ) {
		// if -whylive cares about this symbol, then dump chain
		if ( (previous->name != NULL) && fOptions.printWhyLive(previous->name) ) {
			int depth = 0;
			for(WhyLiveBackChain* p = previous; p != NULL; p = p->previous, ++depth) {
				for(int i=depth; i > 0; --i)
					fprintf(stderr, "  ");
				fprintf(stderr, "%s\n", p->name);
			}
		}
		// set up next chain
		WhyLiveBackChain thisChain;
		thisChain.previous = previous;
		// this atom is live
		fLiveAtoms.insert(&atom);
		// update total size info (except for __ZEROPAGE atom)
		if ( atom.getSegment().isContentReadable() ) {
			fTotalSize += atom.getSize();
			if ( atom.isZeroFill() )
				fTotalZeroFillSize += atom.getSize();
		}
		// and all atoms it references
		std::vector<class ObjectFile::Reference*>& references = atom.getReferences();
		for (std::vector<ObjectFile::Reference*>::iterator it=references.begin(); it != references.end(); it++) {
			ObjectFile::Reference* reference = *it;
			if ( reference->getTargetBinding() == ObjectFile::Reference::kUnboundByName ) {
				// look in global symbol table
				const char* targetName = reference->getTargetName();
				ObjectFile::Atom* target = fGlobalSymbolTable.find(targetName);
				if ( target == NULL ) {
					// load archives or dylibs
					this->addJustInTimeAtomsAndMarkLive(targetName);
				}
				// look again
				target = fGlobalSymbolTable.find(targetName);
				if ( target != NULL ) {
					reference->setTarget(*target, reference->getTargetOffset());
				}
				else {
					// mark as undefined, for later error processing
					fAtomsWithUnresolvedReferences.push_back(&atom);
					fGlobalSymbolTable.require(targetName);
				}
			}
			switch ( reference->getTargetBinding() ) {
				case ObjectFile::Reference::kBoundDirectly:
				case ObjectFile::Reference::kBoundByName:
					thisChain.name = reference->getTargetName();
					markLive(reference->getTarget(), &thisChain);
					break;
				case ObjectFile::Reference::kDontBind:
					addDtraceProbe(atom, reference->getFixUpOffset(), reference->getTargetName());
					break;
				case ObjectFile::Reference::kUnboundByName:
					// do nothing
					break;
			}
			// do the same as above, for "from target"
			if ( reference->getFromTargetBinding() == ObjectFile::Reference::kUnboundByName ) {
				// look in global symbol table
				const char* targetName = reference->getFromTargetName();
				ObjectFile::Atom* target = fGlobalSymbolTable.find(targetName);
				if ( target == NULL ) {
					// load archives or dylibs
					this->addJustInTimeAtomsAndMarkLive(targetName);
				}
				// look again
				target = fGlobalSymbolTable.find(targetName);
				if ( target != NULL ) {
					reference->setFromTarget(*target);
				}
				else {
					// mark as undefined, for later error processing
					fGlobalSymbolTable.require(targetName);
				}
			}
			switch ( reference->getFromTargetBinding() ) {
				case ObjectFile::Reference::kBoundDirectly:
				case ObjectFile::Reference::kBoundByName:
					thisChain.name = reference->getFromTargetName();
					markLive(reference->getFromTarget(), &thisChain);
					break;
				case ObjectFile::Reference::kUnboundByName:
				case ObjectFile::Reference::kDontBind:
					// do nothing
					break;
			}
		}
	}
}


void Linker::addLiveRoot(const char* name)
{
	ObjectFile::Atom* target = fGlobalSymbolTable.find(name);
	if ( target == NULL ) {
		this->addJustInTimeAtomsAndMarkLive(name);
		target = fGlobalSymbolTable.find(name);
	}
	if ( target != NULL )
		fLiveRootAtoms.insert(target);
}


void Linker::deadStripResolve()
{
	// add main() to live roots
	ObjectFile::Atom* entryPoint = this->entryPoint();
	if ( entryPoint != NULL )
		fLiveRootAtoms.insert(entryPoint);

	// add dyld_stub_binding_helper() to live roots
	ObjectFile::Atom* dyldHelper = this->dyldHelper();
	if ( dyldHelper != NULL )
		fLiveRootAtoms.insert(dyldHelper);

	// add -exported_symbols_list, -init, and -u entries to live roots
	std::vector<const char*>& initialUndefines = fOptions.initialUndefines();
	for (std::vector<const char*>::iterator it=initialUndefines.begin(); it != initialUndefines.end(); it++)
		addLiveRoot(*it);

	// in some cases, every global scope atom in initial .o files is a root
	if ( fOptions.allGlobalsAreDeadStripRoots() ) {
		for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms.begin(); it != fAllAtoms.end(); it++) {
			ObjectFile::Atom* atom = *it;
			if ( (atom->getScope() == ObjectFile::Atom::scopeGlobal) && (fDeadAtoms.count(atom) == 0) )
				fLiveRootAtoms.insert(atom);
		}
	}

	// mark all roots as live, and all atoms they reference
	for (std::set<ObjectFile::Atom*>::iterator it=fLiveRootAtoms.begin(); it != fLiveRootAtoms.end(); it++) {
		WhyLiveBackChain rootChain;
		rootChain.previous = NULL;
		rootChain.name = (*it)->getDisplayName();
		markLive(**it, &rootChain);
	}

	// it is possible that there are unresolved references that can be resolved now
	// this can happen if the first reference to a common symbol in an archive.
	// common symbols are not in the archive TOC, but the .o could have been pulled in later.
	// <rdar://problem/4654131> ld64 while linking cc1 [ when dead_strip is ON]
	for (std::vector<ObjectFile::Atom*>::iterator it=fAtomsWithUnresolvedReferences.begin(); it != fAtomsWithUnresolvedReferences.end(); it++) {
		std::vector<class ObjectFile::Reference*>& references = (*it)->getReferences();
		for (std::vector<ObjectFile::Reference*>::iterator rit=references.begin(); rit != references.end(); rit++) {
			ObjectFile::Reference* reference = *rit;
			if ( reference->getTargetBinding() == ObjectFile::Reference::kUnboundByName ) {
				ObjectFile::Atom* target = fGlobalSymbolTable.find(reference->getTargetName());
				if ( target != NULL ) {
					reference->setTarget(*target, reference->getTargetOffset());
					fLiveAtoms.insert(target);
					// by just adding this atom to fLiveAtoms set, we are assuming it has no
					// references, which is true for commons.
					if ( target->getDefinitionKind() != ObjectFile::Atom::kTentativeDefinition )
						fprintf(stderr, "warning: ld64 internal error %s is not a tentative definition\n", target->getDisplayName());
				}
			}
			if ( reference->getFromTargetBinding() == ObjectFile::Reference::kUnboundByName ) {
				ObjectFile::Atom* target = fGlobalSymbolTable.find(reference->getFromTargetName());
				if ( target != NULL ) {
					reference->setFromTarget(*target);
					fLiveAtoms.insert(target);
					// by just adding this atom to fLiveAtoms set, we are assuming it has no
					// references, which is true for commons.
					if ( target->getDefinitionKind() != ObjectFile::Atom::kTentativeDefinition )
						fprintf(stderr, "warning: ld64 internal error %s is not a tentative definition\n", target->getDisplayName());
				}
			}
		}
	}

	// now remove all non-live atoms from fAllAtoms
	fAllAtoms.erase(std::remove_if(fAllAtoms.begin(), fAllAtoms.end(), NotLive(fLiveAtoms)), fAllAtoms.end());
}

void Linker::checkObjC()
{
	// check dylibs
	switch ( fCurrentObjCConstraint ) {
		case ObjectFile::Reader::kObjcNone:
			// can link against any dylib
			break;
		case ObjectFile::Reader::kObjcRetainRelease:
			// cannot link against GC-only dylibs
			for (InstallNameToReader::iterator it=fDylibMap.begin(); it != fDylibMap.end(); it++) {
				if ( it->second->explicitlyLinked() ) {
					if ( it->second->getObjCConstraint() == ObjectFile::Reader::kObjcGC )
						throwf("this linkage unit uses Retain/Release.  It cannot link against the GC-only dylib: %s", it->second->getPath());
				}
			}
			break;
		case ObjectFile::Reader::kObjcRetainReleaseOrGC:
			// can link against GC or RR dylibs
			break;
		case ObjectFile::Reader::kObjcGC:
			// cannot link against RR-only dylibs
			for (InstallNameToReader::iterator it=fDylibMap.begin(); it != fDylibMap.end(); it++) {
				if ( it->second->explicitlyLinked() ) {
					if ( it->second->getObjCConstraint() == ObjectFile::Reader::kObjcRetainRelease )
						throwf("this linkage unit requires GC.  It cannot link against Retain/Release dylib: %s", it->second->getPath());
				}
			}
			break;
	}
	
	// synthesize __OBJC __image_info atom if needed
	if ( fCurrentObjCConstraint != ObjectFile::Reader::kObjcNone ) {
		this->addAtom(fOutputFile->makeObjcInfoAtom(fCurrentObjCConstraint, fObjcReplacmentClasses));
	}
}

void Linker::addDtraceProbe(ObjectFile::Atom& atom, uint32_t offsetInAtom, const char* probeName)
{
	if ( probeName != NULL ) {
		if ( strncmp(probeName, "___dtrace_probe$", 16) == 0 ) 
			fDtraceProbeSites.push_back(DTraceProbeInfo(&atom, offsetInAtom, probeName));
		else if ( strncmp(probeName, "___dtrace_isenabled$", 20) == 0 ) 
			fDtraceIsEnabledSites.push_back(DTraceProbeInfo(&atom, offsetInAtom, probeName));
		else if ( strncmp(probeName, "___dtrace_", 10) == 0 )
			fDtraceAtomToTypes[&atom].insert(probeName);
		else if ( fOptions.dTrace() && (strncmp(probeName, "__dtrace_probe$", 15) == 0) ) 
			fDtraceProbes.push_back(DTraceProbeInfo(&atom, offsetInAtom, probeName));
	}
}

static uint8_t pointerKind(cpu_type_t arch)
{
	switch ( arch ) {
		case CPU_TYPE_POWERPC:
			return ppc::kPointer;
		case CPU_TYPE_POWERPC64:
			return ppc64::kPointer;
		case CPU_TYPE_I386:
			return x86::kPointer;
		case CPU_TYPE_X86_64:
			return x86_64::kPointer;
	}
	throw "uknown architecture";
}

static uint8_t pcRelKind(cpu_type_t arch)
{
	switch ( arch ) {
		case CPU_TYPE_POWERPC:
			return ppc::kPointerDiff32;
		case CPU_TYPE_POWERPC64:
			return ppc64::kPointerDiff32;
		case CPU_TYPE_I386:
			return x86::kPointerDiff;
		case CPU_TYPE_X86_64:
			return x86_64::kPointerDiff32;
	}
	throw "uknown architecture";
}

typedef uint8_t* (*oldcreatedof_func_t) (const char*, cpu_type_t, unsigned int, const char*[], const char*[], uint64_t offsetsInDOF[], size_t* size);
typedef uint8_t* (*createdof_func_t)(cpu_type_t, unsigned int, const char*[], unsigned int, const char*[], const char*[], uint64_t offsetsInDOF[], size_t* size);


void Linker::processDTrace()
{
	// handle dtrace 2.0 static probes
	if ( (fOptions.outputKind() != Options::kObjectFile) && ((fDtraceProbeSites.size() != 0) || (fDtraceIsEnabledSites.size() != 0)) ) {
		// partition probes by provider name
		// The symbol names looks like:
		//	"___dtrace_isenabled$" provider-name "$" probe-name [ "$"... ]
		//	"___dtrace_probe$" provider-name "$" probe-name [ "$"... ]
		ProviderToProbes providerToProbes;
		std::vector<DTraceProbeInfo> emptyList;
		for(std::vector<DTraceProbeInfo>::iterator it = fDtraceProbeSites.begin(); it != fDtraceProbeSites.end(); ++it) {
			const char* providerStart = &it->probeName[16];
			const char* providerEnd = strchr(providerStart, '$');
			if ( providerEnd != NULL ) {
				char providerName[providerEnd-providerStart+1];
				strlcpy(providerName, providerStart, providerEnd-providerStart+1);
				ProviderToProbes::iterator pos = providerToProbes.find(providerName);
				if ( pos == providerToProbes.end() ) {
					const char* dup = strdup(providerName);
					providerToProbes[dup] = emptyList;
				}
				providerToProbes[providerName].push_back(*it);
			}
		}
		for(std::vector<DTraceProbeInfo>::iterator it = fDtraceIsEnabledSites.begin(); it != fDtraceIsEnabledSites.end(); ++it) {
			const char* providerStart = &it->probeName[20];
			const char* providerEnd = strchr(providerStart, '$');
			if ( providerEnd != NULL ) {
				char providerName[providerEnd-providerStart+1];
				strlcpy(providerName, providerStart, providerEnd-providerStart+1);
				ProviderToProbes::iterator pos = providerToProbes.find(providerName);
				if ( pos == providerToProbes.end() ) {
					const char* dup = strdup(providerName);
					providerToProbes[dup] = emptyList;
				}
				providerToProbes[providerName].push_back(*it);
			}
		}
		
		// create a DOF section for each provider
		int dofIndex=1;
		CStringSet sectionNamesUsed;
		for(ProviderToProbes::iterator pit = providerToProbes.begin(); pit != providerToProbes.end(); ++pit, ++dofIndex) {
			const char* providerName = pit->first;
			const std::vector<DTraceProbeInfo>& probes = pit->second;

			// open library and find dtrace_create_dof()
			void* handle = dlopen("/usr/lib/libdtrace.dylib", RTLD_LAZY);
			if ( handle == NULL )
				throwf("couldn't dlopen() /usr/lib/libdtrace.dylib: %s\n", dlerror());
			createdof_func_t pCreateDOF = (createdof_func_t)dlsym(handle, "dtrace_ld_create_dof");
			if ( pCreateDOF == NULL )
				throwf("couldn't find \"dtrace_ld_create_dof\" in /usr/lib/libdtrace.dylib: %s\n", dlerror());
			// build list of typedefs/stability infos for this provider
			CStringSet types;
			for(std::vector<DTraceProbeInfo>::const_iterator it = probes.begin(); it != probes.end(); ++it) {
				std::map<const ObjectFile::Atom*,CStringSet>::iterator pos = fDtraceAtomToTypes.find(it->atom);
				if ( pos != fDtraceAtomToTypes.end() ) {
					for(CStringSet::iterator sit = pos->second.begin(); sit != pos->second.end(); ++sit) {
						const char* providerStart = strchr(*sit, '$')+1;
						const char* providerEnd = strchr(providerStart, '$');
						if ( providerEnd != NULL ) {
							char aProviderName[providerEnd-providerStart+1];
							strlcpy(aProviderName, providerStart, providerEnd-providerStart+1);
							if ( strcmp(aProviderName, providerName) == 0 )
								types.insert(*sit);
						}
					}
				}
			}
			int typeCount = types.size();
			const char* typeNames[typeCount];
			//fprintf(stderr, "types for %s:\n", providerName);
			uint32_t index = 0;
			for(CStringSet::iterator it = types.begin(); it != types.end(); ++it) {
				typeNames[index] = *it;
				//fprintf(stderr, "\t%s\n", *it);
				++index;
			}
			
			// build list of probe/isenabled sites
			const uint32_t probeCount = probes.size();
			const char* probeNames[probeCount];
			const char* funtionNames[probeCount];
			uint64_t offsetsInDOF[probeCount];
			index = 0;
			for(std::vector<DTraceProbeInfo>::const_iterator it = probes.begin(); it != probes.end(); ++it) {
				probeNames[index] = it->probeName;
				funtionNames[index] = it->atom->getName();
				offsetsInDOF[index] = 0;
				++index;
			}
			// call dtrace library to create DOF section
			size_t dofSectionSize;
			uint8_t* p = (*pCreateDOF)(fArchitecture, typeCount, typeNames, probeCount, probeNames, funtionNames, offsetsInDOF, &dofSectionSize);
			if ( p != NULL ) {
				char sectionName[18];
				strcpy(sectionName, "__dof_");
				strlcpy(&sectionName[6], providerName, 10);
				// create unique section name so each DOF is in its own section
				if ( sectionNamesUsed.count(sectionName) != 0 ) {
					sectionName[15] = '0';
					sectionName[16] = '\0';
					while ( sectionNamesUsed.count(sectionName) != 0 )
						++sectionName[15];
				}
				sectionNamesUsed.insert(sectionName);
				char symbolName[strlen(providerName)+64];
				sprintf(symbolName, "__dtrace_dof_for_provider_%s", providerName);
				opaque_section::Reader* reader = new opaque_section::Reader("__TEXT", sectionName, 
														"dtrace", p, dofSectionSize, fNextInputOrdinal, symbolName);
				fNextInputOrdinal += dofSectionSize;
				// add references
				for (uint32_t i=0; i < probeCount; ++i) {
					uint64_t offset = offsetsInDOF[i];
					//fprintf(stderr, "%s offset[%d]=0x%08llX\n", providerName, i, offset);
					if ( offset > dofSectionSize )
						throwf("offsetsInDOF[i]=%0llX > dofSectionSize=%0lX\n", i, offset, dofSectionSize);
					reader->addSectionReference(pcRelKind(fArchitecture), offset, probes[i].atom, probes[i].offset, reader->getAtoms()[0], 0);
				}
				this->addAtoms(reader->getAtoms());
			}
			else {
				throw "error creating dtrace DOF section";
			}
		}
	}
	// create a __DATA __dof section iff -dtrace option was used and static probes were found in .o files
	else if ( fOptions.dTrace() && (fDtraceProbes.size() != 0) ) {
		const uint32_t probeCount = fDtraceProbes.size();
		const char* labels[probeCount];
		const char* funtionNames[probeCount];
		uint64_t offsetsInDOF[probeCount];

		// open libray and find dtrace_ld64_create_dof()
		void* handle = dlopen("/usr/lib/libdtrace.dylib", RTLD_LAZY);
		if ( handle == NULL )
			throwf("couldn't dlopen() /usr/lib/libdtrace.dylib: %s\n", dlerror());
		oldcreatedof_func_t pCreateDOF = (oldcreatedof_func_t)dlsym(handle, "dtrace_ld64_create_dof");
		if ( pCreateDOF == NULL )
			throwf("couldn't find \"dtrace_ld64_create_dof\" in /usr/lib/libdtrace.dylib: %s\n", dlerror());

		// build argument list
		uint32_t index = 0;
		for(std::vector<DTraceProbeInfo>::iterator it = fDtraceProbes.begin(); it != fDtraceProbes.end(); ++it) {
			labels[index] = it->probeName;
			funtionNames[index] = it->atom->getName();
			offsetsInDOF[index] = 0;
			++index;
		}
		size_t dofSectionSize;
		// call dtrace library to create DOF section
		uint8_t* p = (*pCreateDOF)(fOptions.dTraceScriptName(), fArchitecture, probeCount, labels, funtionNames, offsetsInDOF, &dofSectionSize);
		if ( p != NULL ) {
			opaque_section::Reader* reader = new opaque_section::Reader("__DATA", "__dof", "dtrace", p, dofSectionSize, fNextInputOrdinal);
			fNextInputOrdinal += dofSectionSize;
			// add references
			for (uint32_t i=0; i < probeCount; ++i) {
				uint64_t offset = offsetsInDOF[i];
				if ( offset > dofSectionSize )
					throwf("offsetsInDOF[i]=%0llX > dofSectionSize=%0lX\n", i, offset, dofSectionSize);
				reader->addSectionReference(pointerKind(fArchitecture), offset, fDtraceProbes[i].atom, fDtraceProbes[i].offset);
			}
			this->addAtoms(reader->getAtoms());
		}
		else {
			throw "error created dtrace DOF section";
		}
	}
}


static bool matchesObjectFile(ObjectFile::Atom* atom, const char* objectFileLeafName)
{
	if ( objectFileLeafName == NULL )
		return true;
	const char* atomFullPath = atom->getFile()->getPath();
	const char* lastSlash = strrchr(atomFullPath, '/');
	if ( lastSlash != NULL ) {
		if ( strcmp(&lastSlash[1], objectFileLeafName) == 0 )
			return true;
	}
	else {
		if ( strcmp(atomFullPath, objectFileLeafName) == 0 )
			return true;
	}
	return false;
}


static bool usesAnonymousNamespace(const char* symbol)
{
	return ( (strncmp(symbol, "__Z", 3) == 0) && (strstr(symbol, "_GLOBAL__N_") != NULL) );
}


//
//  convert:
//		__ZN20_GLOBAL__N__Z5main2v3barEv					=>  _ZN-3barEv
//		__ZN37_GLOBAL__N_main.cxx_00000000_493A01A33barEv	=>  _ZN-3barEv
//
static void canonicalizeAnonymousName(const char* inSymbol, char outSymbol[])
{
	const char* globPtr = strstr(inSymbol, "_GLOBAL__N_");
	while ( isdigit(*(--globPtr)) )
		; // loop
	char* endptr;
	unsigned long length = strtoul(globPtr+1, &endptr, 10);
	const char* globEndPtr = endptr + length;
	int startLen = globPtr-inSymbol+1;
	memcpy(outSymbol, inSymbol, startLen);
	outSymbol[startLen] = '-';
	strcpy(&outSymbol[startLen+1], globEndPtr);
}


ObjectFile::Atom* Linker::findAtom(const Options::OrderedSymbol& orderedSymbol)
{
	ObjectFile::Atom* atom = fGlobalSymbolTable.find(orderedSymbol.symbolName);
	if ( atom != NULL ) {
		if ( matchesObjectFile(atom, orderedSymbol.objectFileName) )
			return atom;
	}
	else {
		// slow case.  The requested symbol is not in symbol table, so might be static function
		static SymbolTable::Mapper hashTableOfTranslationUnitScopedSymbols;
		static SymbolTable::Mapper hashTableOfSymbolsWithAnonymousNamespace;
		static bool built = false;
		// build a hash_map the first time
		if ( !built ) {
			for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms.begin(); it != fAllAtoms.end(); it++) {
				atom = *it;
				const char* name = atom->getName();
				if ( name != NULL) {
					if ( usesAnonymousNamespace(name) ) {
						// symbol that uses anonymous namespace
						char canonicalName[strlen(name)+2];
						canonicalizeAnonymousName(name, canonicalName);
						const char* hashName = strdup(canonicalName);
						SymbolTable::Mapper::iterator pos = hashTableOfSymbolsWithAnonymousNamespace.find(hashName);
						if ( pos == hashTableOfSymbolsWithAnonymousNamespace.end() )
							hashTableOfSymbolsWithAnonymousNamespace[hashName] = atom;
						else
							hashTableOfSymbolsWithAnonymousNamespace[hashName] = NULL;	// collision, denote with NULL
					}
					else if ( atom->getScope() == ObjectFile::Atom::scopeTranslationUnit ) {
						// static function or data
						SymbolTable::Mapper::iterator pos = hashTableOfTranslationUnitScopedSymbols.find(name);
						if ( pos == hashTableOfTranslationUnitScopedSymbols.end() )
							hashTableOfTranslationUnitScopedSymbols[name] = atom;
						else
							hashTableOfTranslationUnitScopedSymbols[name] = NULL;	// collision, denote with NULL
					}
				}
			}
			//fprintf(stderr, "built hash table of %lu static functions\n", hashTableOfTranslationUnitScopedSymbols.size());
			built = true;
		}

		// look for name in hashTableOfTranslationUnitScopedSymbols
		SymbolTable::Mapper::iterator pos = hashTableOfTranslationUnitScopedSymbols.find(orderedSymbol.symbolName);
		if ( pos != hashTableOfTranslationUnitScopedSymbols.end() ) {
			if ( (pos->second != NULL) && matchesObjectFile(pos->second, orderedSymbol.objectFileName) ) {
				//fprintf(stderr, "found %s in hash table\n", orderedSymbol.symbolName);
				return pos->second;
			}
			if ( pos->second == NULL )
			// name is in hash table, but atom is NULL, so that means there are duplicates, so we use super slow way
			for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms.begin(); it != fAllAtoms.end(); it++) {
				atom = *it;
				if ( atom->getScope() == ObjectFile::Atom::scopeTranslationUnit ) {
					const char* name = atom->getName();
					if ( (name != NULL) && (strcmp(name, orderedSymbol.symbolName) == 0) ) {
						if ( matchesObjectFile(atom, orderedSymbol.objectFileName) ) {
							if ( fOptions.printOrderFileStatistics() )
								fprintf(stderr, "ld: warning %s specified in order_file but it exists in multiple .o files. "
										"Prefix symbol with .o filename in order_file to disambiguate\n", orderedSymbol.symbolName);
							return atom;
						}
					}
				}
			}
		}
		
		// look for name in hashTableOfSymbolsWithAnonymousNamespace
		if ( usesAnonymousNamespace(orderedSymbol.symbolName) ) {
			// symbol that uses anonymous namespace
			char canonicalName[strlen(orderedSymbol.symbolName)+2];
			canonicalizeAnonymousName(orderedSymbol.symbolName, canonicalName);
			SymbolTable::Mapper::iterator pos = hashTableOfSymbolsWithAnonymousNamespace.find(canonicalName);
			if ( pos != hashTableOfSymbolsWithAnonymousNamespace.end() ) {
				if ( (pos->second != NULL) && matchesObjectFile(pos->second, orderedSymbol.objectFileName) ) {
					//fprintf(stderr, "found %s in anonymous namespace hash table\n", canonicalName);
					return pos->second;
				}
				if ( pos->second == NULL )
				// name is in hash table, but atom is NULL, so that means there are duplicates, so we use super slow way
				for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms.begin(); it != fAllAtoms.end(); it++) {
					atom = *it;
					const char* name = atom->getName();
					if ( (name != NULL) && usesAnonymousNamespace(name) ) {
						char canonicalAtomName[strlen(name)+2];
						canonicalizeAnonymousName(name, canonicalAtomName);
						if ( strcmp(canonicalAtomName, canonicalName) == 0 ) {
							if ( matchesObjectFile(atom, orderedSymbol.objectFileName) ) {
								if ( fOptions.printOrderFileStatistics() )
									fprintf(stderr, "ld: warning %s specified in order_file but it exists in multiple .o files. "
										"Prefix symbol with .o filename in order_file to disambiguate\n", orderedSymbol.symbolName);
								return atom;
							}
						}
					}
				}
			}
		}
	}
	return NULL;
}


void Linker::sortSections()
{
	Section::assignIndexes();
}


//
// Linker::sortAtoms()
//
// The purpose of this method is to take the graph of all Atoms and produce an ordered
// sequence of atoms.  The constraints are that: 1) all Atoms of the same Segment must
// be contiguous, 2)  all Atoms of the same Section must be contigous, 3) Atoms specified
// in an order_file are seqenced as in the order_file and before Atoms not specified,
// 4) Atoms in the same section from the same .o file should be contiguous and sequenced
// in the same order they were in the .o file, 5) Atoms in the same Section but which came
// from different .o files should be sequenced in the same order that the .o files
// were passed to the linker (i.e. command line order).
//
// The way this is implemented is that the linker passes a "base ordinal" to each Reader
// as it is constructed.  The reader should construct it Atoms so that calling getOrdinal()
// on its atoms returns a contiguous range of values starting at the base ordinal.  Then
// sorting is just sorting by section, then by ordinal.
//
// If an order_file is specified, it gets more complicated.  First, an override-ordinal map
// is created.  It causes the sort routine to ignore the value returned by getOrdinal() and
// use the override value instead.  Next some Atoms must be layed out consecutively
// (e.g. hand written assembly that does not end with return, but rather falls into
// the next label).  This is modeled in Readers via a "kFollowOn" reference.  The use of
// kFollowOn refernces produces "clusters" of atoms that must stay together.
// If an order_file tries to move one atom, it may need to move a whole cluster.  The
// algorithm to do this models clusters using two maps.  The "starts" maps maps any
// atom in a cluster to the first Atom in the cluster.  The "nexts" maps an Atom in a
// cluster to the next Atom in the cluster.  With this in place, while processing an
// order_file, if any entry is in a cluster (in "starts" map), then the entire cluster is
// given ordinal overrides.
//
void Linker::sortAtoms()
{
	fStartSortTime = mach_absolute_time();
	// if -order_file is used, build map of atom ordinal overrides
	std::map<const ObjectFile::Atom*, uint32_t>* ordinalOverrideMap = NULL;
	std::map<const ObjectFile::Atom*, uint32_t> theOrdinalOverrideMap;
	const bool log = false;
	if ( fOptions.orderedSymbols().size() != 0 ) {
		// first make a pass to find all follow-on references and build start/next maps
		// which are a way to represent clusters of atoms that must layout together
		std::map<const ObjectFile::Atom*, const ObjectFile::Atom*> followOnStarts;
		std::map<const ObjectFile::Atom*, const ObjectFile::Atom*> followOnNexts;
		for (std::vector<ObjectFile::Atom*>::iterator ait=fAllAtoms.begin(); ait != fAllAtoms.end(); ait++) {
			ObjectFile::Atom* atom = *ait;
			std::vector<class ObjectFile::Reference*>& references = atom->getReferences();
			for (std::vector<ObjectFile::Reference*>::iterator rit=references.begin(); rit != references.end(); rit++) {
				ObjectFile::Reference* ref = *rit;
				if ( ref->getKind() == 1 ) {	// FIX FIX
					ObjectFile::Atom* targetAtom = &ref->getTarget();
					if ( log ) fprintf(stderr, "ref %s -> %s", atom->getDisplayName(), targetAtom->getDisplayName());
					std::map<const ObjectFile::Atom*, const ObjectFile::Atom*>::iterator startFrom = followOnStarts.find(atom);
					std::map<const ObjectFile::Atom*, const ObjectFile::Atom*>::iterator startTo = followOnStarts.find(targetAtom);
					if ( (startFrom == followOnStarts.end()) && (startTo == followOnStarts.end()) ) {
						// this is first time we've seen either atom, make simple cluster of the two
						if ( log ) fprintf(stderr, "  new cluster\n");
						followOnStarts[atom] = atom;
						followOnStarts[targetAtom] = atom;
						followOnNexts[atom] = targetAtom;
						followOnNexts[targetAtom] = NULL;
					}
					else if ( (startFrom != followOnStarts.end()) && (startTo == followOnStarts.end()) && (followOnNexts[atom] == NULL) ) {
						// atom is at end of an existing cluster, so append target to end of cluster
						if ( log ) fprintf(stderr, "  end of cluster starting with %s\n", followOnStarts[atom]->getDisplayName());
						followOnNexts[atom] = targetAtom;
						followOnNexts[targetAtom] = NULL;
						followOnStarts[targetAtom] = followOnStarts[atom];
					}
					else {
						// gerneral case of inserting into an existing cluster
						if ( followOnNexts[atom] != NULL ) {
							// an atom with two follow-ons is illegal
							fprintf(stderr, "ld: warning can't order %s because both %s and %s must follow it\n",
										atom->getDisplayName(), targetAtom->getDisplayName(), followOnNexts[atom]->getDisplayName());
						}
						else {
							// there already exists an atom that says target must be its follow-on
							const ObjectFile::Atom* originalStart = startTo->second;
							const ObjectFile::Atom* originalPrevious = originalStart;
							while ( followOnNexts[originalPrevious] != targetAtom )
								originalPrevious = followOnNexts[originalPrevious];
							bool otherIsAlias = (originalPrevious->getSize() == 0);
							bool thisIsAlias = (atom->getSize() == 0);
							if ( !otherIsAlias && !thisIsAlias ) {
								fprintf(stderr, "ld: warning can't order %s because both %s and %s must preceed it\n",
											targetAtom->getDisplayName(), originalPrevious->getDisplayName(), atom->getDisplayName());
							}
							else if ( otherIsAlias ) {
								if ( originalPrevious == originalStart ) {
									// other is alias at start of cluster, make this the new start of cluster
									if ( log ) fprintf(stderr, "  becomes new start of cluster previous starting with %s\n", originalStart->getDisplayName());
									followOnNexts[atom] = originalPrevious;
									for(const ObjectFile::Atom* nextAtom = atom; nextAtom != NULL; nextAtom = followOnNexts[nextAtom])
										followOnStarts[nextAtom] = atom;
								}
								else {
									// other is alias in middle of cluster, insert new atom before it
									if ( log ) fprintf(stderr, "  insert into cluster starting with %s before alias %s\n", originalStart->getDisplayName(), originalPrevious->getDisplayName());
									followOnStarts[atom] = originalStart;
									followOnNexts[atom] = originalPrevious;
									for(const ObjectFile::Atom* a = originalStart; a != NULL; a = followOnNexts[a]) {
										if ( followOnNexts[a] == originalPrevious ) {
											followOnNexts[a] = atom;
											break;
										}
									}
								}
							}
							else {
								// this is alias, so it can go inbetween originalPrevious and targetAtom
								if ( log ) fprintf(stderr, "  insert into cluster starting with %s after %s\n", originalStart->getDisplayName(), originalPrevious->getDisplayName());
								followOnStarts[atom] = originalStart;
								followOnNexts[atom] = followOnNexts[originalPrevious];
								followOnNexts[originalPrevious] = atom;
							}
						}
					}
				}
			}
		}

		if ( log ) {
			for(std::map<const ObjectFile::Atom*, const ObjectFile::Atom*>::iterator it = followOnStarts.begin(); it != followOnStarts.end(); ++it)
				fprintf(stderr, "start %s -> %s\n", it->first->getDisplayName(), it->second->getDisplayName());

			for(std::map<const ObjectFile::Atom*, const ObjectFile::Atom*>::iterator it = followOnNexts.begin(); it != followOnNexts.end(); ++it)
				fprintf(stderr, "next %s -> %s\n", it->first->getDisplayName(), (it->second != NULL) ? it->second->getDisplayName() : "null");
		}

		// with the start/next maps of follow-on atoms we can process the order file and produce override ordinals
		ordinalOverrideMap = &theOrdinalOverrideMap;
		uint32_t index = 0;
		uint32_t matchCount = 0;
		std::vector<Options::OrderedSymbol>& orderedSymbols = fOptions.orderedSymbols();
		for(std::vector<Options::OrderedSymbol>::iterator it = orderedSymbols.begin(); it != orderedSymbols.end(); ++it) {
			ObjectFile::Atom* atom = this->findAtom(*it);
			if ( atom != NULL ) {
				std::map<const ObjectFile::Atom*, const ObjectFile::Atom*>::iterator start = followOnStarts.find(atom);
				if ( start != followOnStarts.end() ) {
					// this symbol for the order file corresponds to an atom that is in a cluster that must lay out together
					for(const ObjectFile::Atom* nextAtom = start->second; nextAtom != NULL; nextAtom = followOnNexts[nextAtom]) {
						std::map<const ObjectFile::Atom*, uint32_t>::iterator pos = theOrdinalOverrideMap.find(nextAtom);
						if ( pos == theOrdinalOverrideMap.end() ) {
							theOrdinalOverrideMap[nextAtom] = index++;
							if (log ) fprintf(stderr, "override ordinal %u assigned to %s in cluster from %s\n", index, nextAtom->getDisplayName(), nextAtom->getFile()->getPath());
						}
						else {
							if (log ) fprintf(stderr, "could not order %s as %u because it was already laid out earlier by %s as %u\n",
											atom->getDisplayName(), index, followOnStarts[atom]->getDisplayName(), theOrdinalOverrideMap[atom] );
						}
					}
				}
				else {
					theOrdinalOverrideMap[atom] = index;
					if (log ) fprintf(stderr, "override ordinal %u assigned to %s from %s\n", index, atom->getDisplayName(), atom->getFile()->getPath());
				}
			}
			else {
				++matchCount;
				//fprintf(stderr, "can't find match for order_file entry %s/%s\n", it->objectFileName, it->symbolName);
			}
			 ++index;
		}
		if ( fOptions.printOrderFileStatistics() && (fOptions.orderedSymbols().size() != matchCount) ) {
			fprintf(stderr, "ld: warning only %u out of %lu order_file symbols were applicable\n", matchCount, fOptions.orderedSymbols().size() );
		}
	}

	// sort atoms
	std::sort(fAllAtoms.begin(), fAllAtoms.end(), Linker::AtomSorter(ordinalOverrideMap));

	//fprintf(stderr, "Sorted atoms:\n");
	//for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms.begin(); it != fAllAtoms.end(); it++) {
	//	fprintf(stderr, "\t%p, %u  %s\n", (*it)->getSection(), (*it)->getSection()->getIndex(), (*it)->getDisplayName());
	//}
}


// make sure given addresses are within reach of branches, etc
void Linker::tweakLayout()
{
	// > 2GB images need their large zero fill atoms sorted to the end to keep access with +/- 2GB
	if ( fTotalSize > 0x7F000000 ) {
		fBiggerThanTwoGigOutput = true;

		if ( (fTotalSize-fTotalZeroFillSize) > 0x7F000000 )
			throwf("total output size exceeds 2GB (%lldMB)", (fTotalSize-fTotalZeroFillSize)/(1024*1024));		

		// move very large (>1MB) zero fill atoms to a new section at very end
		Section* hugeZeroFills = Section::find("__huge", "__DATA", true);
		for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms.begin(); it != fAllAtoms.end(); it++) {
			ObjectFile::Atom* atom = *it;
			if ( atom->isZeroFill() && (atom->getSize() > 1024*1024) && atom->getSegment().isContentReadable() )
				atom->setSection(hugeZeroFills);
		}
	}
}


void Linker::writeDotOutput()
{
	const char* dotOutFilePath = fOptions.dotOutputFile();
	if ( dotOutFilePath != NULL ) {
		FILE* out = fopen(dotOutFilePath, "w");
		if ( out != NULL ) {
			// print header
			fprintf(out, "digraph dg\n{\n");
			fprintf(out, "\tconcentrate = true;\n");
			fprintf(out, "\trankdir = LR;\n");

			// print each atom as a node
			for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms.begin(); it != fAllAtoms.end(); it++) {
				ObjectFile::Atom* atom = *it;
				if ( atom->getFile() != fOutputFile ) {
					const char* name = atom->getDisplayName();
					if ( (atom->getDefinitionKind() == ObjectFile::Atom::kExternalDefinition)
					  || (atom->getDefinitionKind() == ObjectFile::Atom::kExternalWeakDefinition) ) {
						fprintf(out, "\taddr%p [ shape = plaintext, label = \"%s\" ];\n", atom, name);
					}
					else if ( strcmp(atom->getSectionName(), "__cstring") == 0 ) {
						char cstring[atom->getSize()+2];
						atom->copyRawContent((uint8_t*)cstring);
						fprintf(out, "\taddr%p [ label = \"string: '", atom);
						for (const char* s=cstring; *s != '\0'; ++s) {
							if ( *s == '\n' )
								fprintf(out, "\\\\n");
							else
								fputc(*s, out);
						}
						fprintf(out, "'\" ];\n");
					}
					else {
						fprintf(out, "\taddr%p [ label = \"%s\" ];\n", atom, name);
					}
				}
			}
			fprintf(out, "\n");

			// print each reference as an edge
			for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms.begin(); it != fAllAtoms.end(); it++) {
				ObjectFile::Atom* fromAtom = *it;
				if ( fromAtom->getFile() != fOutputFile ) {
					std::vector<ObjectFile::Reference*>&  references = fromAtom->getReferences();
					std::set<ObjectFile::Atom*> seenTargets;
					for (std::vector<ObjectFile::Reference*>::iterator rit=references.begin(); rit != references.end(); rit++) {
						ObjectFile::Reference* reference = *rit;
						ObjectFile::Atom* toAtom = &(reference->getTarget());
						if ( seenTargets.count(toAtom) == 0 ) {
							seenTargets.insert(toAtom);
							fprintf(out, "\taddr%p -> addr%p;\n", fromAtom, toAtom);
						}
					}
				}
			}
			fprintf(out, "\n");

			// push all imports to bottom of graph
			fprintf(out, "{ rank = same; ");
			for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms.begin(); it != fAllAtoms.end(); it++) {
				ObjectFile::Atom* atom = *it;
				if ( atom->getFile() != fOutputFile )
					if ( (atom->getDefinitionKind() == ObjectFile::Atom::kExternalDefinition)
					  || (atom->getDefinitionKind() == ObjectFile::Atom::kExternalWeakDefinition) ) {
						fprintf(out, "addr%p; ", atom);
					}
			}
			fprintf(out, "};\n ");

			// print footer
			fprintf(out, "}\n");
			fclose(out);
		}
		else {
			fprintf(stderr, "ld: warning could not write dot output file: %s\n", dotOutFilePath);
		}
	}
}

ObjectFile::Atom* Linker::entryPoint()
{
	// if main executable, find entry point atom
	ObjectFile::Atom* entryPoint = NULL;
	switch ( fOptions.outputKind() ) {
		case Options::kDynamicExecutable:
		case Options::kStaticExecutable:
		case Options::kDyld:
			entryPoint = fGlobalSymbolTable.find(fOptions.entryName());
			if ( entryPoint == NULL ) {
				throwf("could not find entry point \"%s\" (perhaps missing crt1.o)", fOptions.entryName());
			}
			break;
		case Options::kDynamicLibrary:
			if ( fOptions.initFunctionName() != NULL ) {
				entryPoint = fGlobalSymbolTable.find(fOptions.initFunctionName());
				if ( entryPoint == NULL ) {
					throwf("could not find -init function: \"%s\"", fOptions.initFunctionName());
				}
			}
			break;
		case Options::kObjectFile:
		case Options::kDynamicBundle:
			entryPoint = NULL;
			break;
	}
	return entryPoint;
}

ObjectFile::Atom* Linker::dyldHelper()
{
	return fGlobalSymbolTable.find("dyld_stub_binding_helper");
}

const char* Linker::assureFullPath(const char* path)
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
// The stab strings are of the form:
//		<name> ':' <type-code> <number-pari>
//  but the <name> contain a colon.
//  For C++ <name> may contain a double colon (e.g. std::string:f(0,1) )
//  For Objective-C name may contain a colon instead square bracket (e.g. [Foo doit:]:f(0,1) )
//
const char* Linker::truncateStabString(const char* str)
{
	enum { start, inObjc } state = start;
	for (const char* s = str; *s != 0; ++s) {
		char c = *s;
		switch (state) {
			case start:
				if ( c == '[' ) {
					state = inObjc;
				}
				else {
					if ( c == ':' ) {
						if ( s[1] == ':' ) {
							++s;
						}
						else {
							// found colon
							// Duplicate strndup behavior here.
							int trunStrLen = s-str+2;
							char* temp = new char[trunStrLen+1];
							memcpy(temp, str, trunStrLen);
							temp[trunStrLen] = '\0';
							return temp;
						}
					}
				}
				break;
			case inObjc:
				if ( c == ']' ) {
					state = start;
				}
				break;
		}
	}
	// malformed
	return str;
}


bool Linker::minimizeStab(ObjectFile::Reader::Stab& stab)
{
	switch(stab.type){
		case N_GSYM:
		case N_STSYM:
		case N_LCSYM:
		case N_FUN:
			// these all need truncated strings
			stab.string = truncateStabString(stab.string);
			return true;
		case N_SO:
		case N_OSO:
		case N_OPT:
		case N_SOL:
			// these are included in the minimal stabs, but they keep their full string
			return true;
		default:
			return false;
	}
}


struct HeaderRange {
	std::vector<ObjectFile::Reader::Stab>::iterator	begin;
	std::vector<ObjectFile::Reader::Stab>::iterator	end;
	int												parentRangeIndex;
	uint32_t										sum;
	bool											sumPrecomputed;
	bool											useEXCL;
	bool											cannotEXCL; // because of SLINE, etc stabs
};


typedef __gnu_cxx::hash_map<const char*, std::vector<uint32_t>, __gnu_cxx::hash<const char*>, CStringEquals> PathToSums;

// hash table that maps header path to a vector of known checksums for that path
static PathToSums sKnownBINCLs;


void Linker::collectStabs(ObjectFile::Reader* reader, std::map<const class ObjectFile::Atom*, uint32_t>& atomOrdinals)
{
	bool log = false;
	bool minimal = ( fOptions.readerOptions().fDebugInfoStripping == ObjectFile::ReaderOptions::kDebugInfoMinimal );
	std::vector<class ObjectFile::Reader::Stab>* readerStabs = reader->getStabs();
	if ( readerStabs == NULL )
		return;

	if ( log ) fprintf(stderr, "processesing %lu stabs for %s\n", readerStabs->size(), reader->getPath());
	std::vector<HeaderRange> ranges;
	int curRangeIndex = -1;
	int count = 0;
	ObjectFile::Atom* atomWithLowestOrdinal = NULL;
	ObjectFile::Atom* atomWithHighestOrdinal = NULL;
	uint32_t highestOrdinal = 0;
	uint32_t lowestOrdinal = UINT_MAX;
	std::vector<std::pair<ObjectFile::Atom*,ObjectFile::Atom*> > soRanges;
	// 1) find all (possibly nested) BINCL/EINCL ranges and their checksums
	// 2) find all SO/SO ranges and the first/last atom own by a FUN stab therein
	for(std::vector<class ObjectFile::Reader::Stab>::iterator it=readerStabs->begin(); it != readerStabs->end(); ++it) {
		++count;
		switch ( it->type ) {
			case N_BINCL:
				{
					HeaderRange range;
					range.begin = it;
					range.end = readerStabs->end();
					range.parentRangeIndex = curRangeIndex;
					range.sum = it->value;
					range.sumPrecomputed = (range.sum != 0);
					range.useEXCL = false;
					range.cannotEXCL = false;
					curRangeIndex = ranges.size();
					if ( log ) fprintf(stderr, "[%d]BINCL %s\n", curRangeIndex, it->string);
					ranges.push_back(range);
				}
				break;
			case N_EINCL:
				if ( curRangeIndex == -1 ) {
					fprintf(stderr, "ld: warning EINCL missing BINCL in %s\n", reader->getPath());
				}
				else {
					ranges[curRangeIndex].end = it+1;
					if ( log ) fprintf(stderr, "[%d->%d]EINCL %s\n", curRangeIndex, ranges[curRangeIndex].parentRangeIndex, it->string);
					curRangeIndex = ranges[curRangeIndex].parentRangeIndex;
				}
				break;
			case N_FUN:
				{
					std::map<const class ObjectFile::Atom*, uint32_t>::iterator pos = atomOrdinals.find(it->atom);
					if ( pos != atomOrdinals.end() ) {
						uint32_t ordinal = pos->second;
						if ( ordinal > highestOrdinal ) {
							highestOrdinal = ordinal;
							atomWithHighestOrdinal = it->atom;
						}
						if ( ordinal < lowestOrdinal ) {
							lowestOrdinal = ordinal;
							atomWithLowestOrdinal = it->atom;
						}
					}
				}
				// fall through
			case N_BNSYM:
			case N_ENSYM:
			case N_LBRAC:
			case N_RBRAC:
			case N_SLINE:
			case N_STSYM:
			case N_LCSYM:
				if ( curRangeIndex != -1 ) {
					ranges[curRangeIndex].cannotEXCL = true;
					if ( fOptions.warnStabs() )
						fprintf(stderr, "ld: cannot do BINCL/EINCL optimzation because of stabs kinds in %s for %s\n", ranges[curRangeIndex].begin->string, reader->getPath());
				}
				break;
			case N_SO:
				if ( (it->string != NULL) && (strlen(it->string) > 0) ) {
					// start SO, reset hi/low FUN tracking
					atomWithLowestOrdinal = NULL;
					atomWithHighestOrdinal = NULL;
					highestOrdinal = 0;
					lowestOrdinal = UINT_MAX;
				}
				else {
					// end SO, record hi/low atoms for this SO range
					soRanges.push_back(std::make_pair<ObjectFile::Atom*,ObjectFile::Atom*>(atomWithLowestOrdinal, atomWithHighestOrdinal));
				}
				// fall through
			default:
				if ( curRangeIndex != -1 ) {
					if ( ! ranges[curRangeIndex].sumPrecomputed ) {
						uint32_t sum = 0;
						const char* s = it->string;
						char c;
						while ( (c = *s++) != 0 ) {
							sum += c;
							// don't checkusm first number (file index) after open paren in string
							if ( c == '(' ) {
								while(isdigit(*s))
									++s;
							}
						}
						ranges[curRangeIndex].sum += sum;
					}
				}

		}
	}
	if ( log ) fprintf(stderr, "processesed %d stabs for %s\n", count, reader->getPath());
	if ( curRangeIndex != -1 )
		fprintf(stderr, "ld: warning BINCL (%s) missing EINCL in %s\n", ranges[curRangeIndex].begin->string, reader->getPath());

	// if no BINCLs
	if ( ranges.size() == 0 ) {
		int soIndex = 0;
		for(std::vector<ObjectFile::Reader::Stab>::iterator it=readerStabs->begin(); it != readerStabs->end(); ++it) {
			// copy minimal or all stabs
			ObjectFile::Reader::Stab stab = *it;
			if ( !minimal || minimizeStab(stab) ) {
				if ( stab.type == N_SO ) {
					if ( (stab.string != NULL) && (strlen(stab.string) > 0) ) {
						// starting SO is associated with first atom
						stab.atom = soRanges[soIndex].first;
					}
					else {
						// ending SO is associated with last atom
						stab.atom = soRanges[soIndex].second;
						++soIndex;
					}
				}
				fStabs.push_back(stab);
			}
		}
		return;
	}

	//fprintf(stderr, "BINCL/EINCL info for %s\n", reader->getPath());
	//for(std::vector<HeaderRange>::iterator it=ranges.begin(); it != ranges.end(); ++it) {
	//	fprintf(stderr, "%08X %s\n", it->sum, it->begin->string);
	//}

	// see if any of these BINCL/EINCL ranges have already been seen and therefore can be replaced with EXCL
	for(std::vector<HeaderRange>::iterator it=ranges.begin(); it != ranges.end(); ++it) {
		if ( ! it->cannotEXCL ) {
			const char* header = it->begin->string;
			uint32_t sum = it->sum;
			PathToSums::iterator pos = sKnownBINCLs.find(header);
			if ( pos != sKnownBINCLs.end() ) {
				std::vector<uint32_t>& sums = pos->second;
				for(std::vector<uint32_t>::iterator sit=sums.begin(); sit != sums.end(); ++sit) {
					if (*sit == sum) {
						//fprintf(stderr, "use EXCL for %s in %s\n", header, reader->getPath());
						it->useEXCL = true;
						break;
					}
				}
				if ( ! it->useEXCL ) {
					// have seen this path, but not this checksum
					//fprintf(stderr, "registering another checksum %08X for %s\n", sum, header);
					sums.push_back(sum);
				}
			}
			else {
				// have not seen this path, so add to known BINCLs
				std::vector<uint32_t> empty;
				sKnownBINCLs[header] = empty;
				sKnownBINCLs[header].push_back(sum);
				//fprintf(stderr, "registering checksum %08X for %s\n", sum, header);
			}
		}
	}

	// add a new set of stabs with BINCL/EINCL runs that have been seen before, replaced with EXCLs
	curRangeIndex = -1;
	const int maxRangeIndex = ranges.size();
	int soIndex = 0;
	for(std::vector<ObjectFile::Reader::Stab>::iterator it=readerStabs->begin(); it != readerStabs->end(); ++it) {
		switch ( it->type ) {
			case N_BINCL:
				for(int i=curRangeIndex+1; i < maxRangeIndex; ++i) {
					if ( ranges[i].begin == it ) {
						curRangeIndex = i;
						HeaderRange& range = ranges[curRangeIndex];
						ObjectFile::Reader::Stab stab = *it;
						stab.value = range.sum; // BINCL and EXCL have n_value set to checksum
						if ( range.useEXCL )
							stab.type = N_EXCL;	// transform BINCL into EXCL
						if ( !minimal )
							fStabs.push_back(stab);
						break;
					}
				}
				break;
			case N_EINCL:
				if ( curRangeIndex != -1 ) {
					if ( !ranges[curRangeIndex].useEXCL && !minimal )
						fStabs.push_back(*it);
					curRangeIndex = ranges[curRangeIndex].parentRangeIndex;
				}
				break;
			default:
				if ( (curRangeIndex == -1) || !ranges[curRangeIndex].useEXCL ) {
					ObjectFile::Reader::Stab stab = *it;
					if ( !minimal || minimizeStab(stab) ) {
						if ( stab.type == N_SO ) {
							if ( (stab.string != NULL) && (strlen(stab.string) > 0) ) {
								// starting SO is associated with first atom
								stab.atom = soRanges[soIndex].first;
							}
							else {
								// ending SO is associated with last atom
								stab.atom = soRanges[soIndex].second;
								++soIndex;
							}
						}
						fStabs.push_back(stab);
					}
				}
		}
	}

}


// used to prune out atoms that don't need debug notes generated
class NoDebugNoteAtom
{
public:
	NoDebugNoteAtom(const std::map<class ObjectFile::Reader*, uint32_t>& readersWithDwarfOrdinals)
			: fReadersWithDwarfOrdinals(readersWithDwarfOrdinals) {}

	bool operator()(const ObjectFile::Atom* atom) const {
		if ( atom->getSymbolTableInclusion() == ObjectFile::Atom::kSymbolTableNotIn )
			return true;
		if ( atom->getName() == NULL )
			return true;
		if ( fReadersWithDwarfOrdinals.find(atom->getFile()) == fReadersWithDwarfOrdinals.end() )
			return true;
		return false;
	}

private:
	const std::map<class ObjectFile::Reader*, uint32_t>& fReadersWithDwarfOrdinals;
};

// used to sort atoms with debug notes
class ReadersWithDwarfSorter
{
public:
	ReadersWithDwarfSorter(const std::map<class ObjectFile::Reader*, uint32_t>& readersWithDwarfOrdinals,
						   const std::map<const class ObjectFile::Atom*, uint32_t>& atomOrdinals)
			: fReadersWithDwarfOrdinals(readersWithDwarfOrdinals), fAtomOrdinals(atomOrdinals) {}

	bool operator()(const ObjectFile::Atom* left, const ObjectFile::Atom* right) const
	{
		// first sort by reader
		unsigned int leftReaderIndex  = fReadersWithDwarfOrdinals.find(left->getFile())->second;
		unsigned int rightReaderIndex = fReadersWithDwarfOrdinals.find(right->getFile())->second;
		if ( leftReaderIndex != rightReaderIndex )
			return (leftReaderIndex < rightReaderIndex);

		// then sort by atom ordinal
		unsigned int leftAtomIndex  = fAtomOrdinals.find(left)->second;
		unsigned int rightAtomIndex = fAtomOrdinals.find(right)->second;
		return leftAtomIndex < rightAtomIndex;
	}

private:
	const std::map<class ObjectFile::Reader*, uint32_t>& fReadersWithDwarfOrdinals;
	const std::map<const class ObjectFile::Atom*, uint32_t>& fAtomOrdinals;
};





void Linker::synthesizeDebugNotes(std::vector<class ObjectFile::Atom*>& allAtomsByReader)
{
	// synthesize "debug notes" and add them to master stabs vector
	const char* dirPath = NULL;
	const char* filename = NULL;
	bool wroteStartSO = false;
	bool useZeroOSOModTime = (getenv("RC_RELEASE") != NULL);
	__gnu_cxx::hash_set<const char*, __gnu_cxx::hash<const char*>, CStringEquals>  seenFiles;
	for (std::vector<ObjectFile::Atom*>::iterator it=allAtomsByReader.begin(); it != allAtomsByReader.end(); it++) {
		ObjectFile::Atom* atom = *it;
		const char* newDirPath;
		const char* newFilename;
		//fprintf(stderr, "debug note for %s\n", atom->getDisplayName());
		if ( atom->getTranslationUnitSource(&newDirPath, &newFilename) ) {
			// need SO's whenever the translation unit source file changes
			if ( newFilename != filename ) {
				// gdb like directory SO's to end in '/', but dwarf DW_AT_comp_dir usually does not have trailing '/'
				if ( (newDirPath != NULL) && (strlen(newDirPath) > 1 ) && (newDirPath[strlen(newDirPath)-1] != '/') )
					asprintf((char**)&newDirPath, "%s/", newDirPath);
				if ( filename != NULL ) {
					// translation unit change, emit ending SO
					ObjectFile::Reader::Stab endFileStab;
					endFileStab.atom		= NULL;
					endFileStab.type		= N_SO;
					endFileStab.other		= 1;
					endFileStab.desc		= 0;
					endFileStab.value		= 0;
					endFileStab.string		= "";
					fStabs.push_back(endFileStab);
				}
				// new translation unit, emit start SO's
				ObjectFile::Reader::Stab dirPathStab;
				dirPathStab.atom		= NULL;
				dirPathStab.type		= N_SO;
				dirPathStab.other		= 0;
				dirPathStab.desc		= 0;
				dirPathStab.value		= 0;
				dirPathStab.string		= newDirPath;
				fStabs.push_back(dirPathStab);
				ObjectFile::Reader::Stab fileStab;
				fileStab.atom		= NULL;
				fileStab.type		= N_SO;
				fileStab.other		= 0;
				fileStab.desc		= 0;
				fileStab.value		= 0;
				fileStab.string		= newFilename;
				fStabs.push_back(fileStab);
				// Synthesize OSO for start of file
				ObjectFile::Reader::Stab objStab;
				objStab.atom		= NULL;
				objStab.type		= N_OSO;
				objStab.other		= 0;
				objStab.desc		= 1;
				objStab.value		= useZeroOSOModTime ? 0 : atom->getFile()->getModificationTime();
				objStab.string		= assureFullPath(atom->getFile()->getPath());
				fStabs.push_back(objStab);
				wroteStartSO = true;
				// add the source file path to seenFiles so it does not show up in SOLs
				seenFiles.insert(newFilename);
			}
			filename = newFilename;
			dirPath = newDirPath;
			if ( atom->getSegment().isContentExecutable() && (strncmp(atom->getSectionName(), "__text", 6) == 0) ) {
				// Synthesize BNSYM and start FUN stabs
				ObjectFile::Reader::Stab beginSym;
				beginSym.atom		= atom;
				beginSym.type		= N_BNSYM;
				beginSym.other		= 1;
				beginSym.desc		= 0;
				beginSym.value		= 0;
				beginSym.string		= "";
				fStabs.push_back(beginSym);
				ObjectFile::Reader::Stab startFun;
				startFun.atom		= atom;
				startFun.type		= N_FUN;
				startFun.other		= 1;
				startFun.desc		= 0;
				startFun.value		= 0;
				startFun.string		= atom->getName();
				fStabs.push_back(startFun);
				// Synthesize any SOL stabs needed
				std::vector<ObjectFile::LineInfo>* lineInfo = atom->getLineInfo();
				if ( lineInfo != NULL ) {
					const char* curFile = NULL;
					for (std::vector<ObjectFile::LineInfo>::iterator it = lineInfo->begin(); it != lineInfo->end(); ++it) {
						if ( it->fileName != curFile ) {
							if ( seenFiles.count(it->fileName) == 0 ) {
								seenFiles.insert(it->fileName);
								ObjectFile::Reader::Stab sol;
								sol.atom		= 0;
								sol.type		= N_SOL;
								sol.other		= 0;
								sol.desc		= 0;
								sol.value		= 0;
								sol.string		= it->fileName;
								fStabs.push_back(sol);
							}
							curFile = it->fileName;
						}
					}
				}
				// Synthesize end FUN and ENSYM stabs
				ObjectFile::Reader::Stab endFun;
				endFun.atom			= atom;
				endFun.type			= N_FUN;
				endFun.other		= 0;
				endFun.desc			= 0;
				endFun.value		= 0;
				endFun.string		= "";
				fStabs.push_back(endFun);
				ObjectFile::Reader::Stab endSym;
				endSym.atom			= atom;
				endSym.type			= N_ENSYM;
				endSym.other		= 1;
				endSym.desc			= 0;
				endSym.value		= 0;
				endSym.string		= "";
				fStabs.push_back(endSym);
			}
			else {
				ObjectFile::Reader::Stab globalsStab;
				if ( atom->getScope() == ObjectFile::Atom::scopeTranslationUnit ) {
					// Synthesize STSYM stab for statics
					const char* name = atom->getName();
					if ( name[0] == '_' ) {
						globalsStab.atom		= atom;
						globalsStab.type		= N_STSYM;
						globalsStab.other		= 1;
						globalsStab.desc		= 0;
						globalsStab.value		= 0;
						globalsStab.string		= name;
						fStabs.push_back(globalsStab);
					}
				}
				else {
					// Synthesize GSYM stab for other globals (but not .eh exception frame symbols)
					const char* name = atom->getName();
					if ( (name[0] == '_') && (strcmp(atom->getSectionName(), "__eh_frame") != 0) ) {
						globalsStab.atom		= atom;
						globalsStab.type		= N_GSYM;
						globalsStab.other		= 1;
						globalsStab.desc		= 0;
						globalsStab.value		= 0;
						globalsStab.string		= name;
						fStabs.push_back(globalsStab);
					}
				}
			}
		}
	}

	if ( wroteStartSO ) {
		//  emit ending SO
		ObjectFile::Reader::Stab endFileStab;
		endFileStab.atom		= NULL;
		endFileStab.type		= N_SO;
		endFileStab.other		= 1;
		endFileStab.desc		= 0;
		endFileStab.value		= 0;
		endFileStab.string		= "";
		fStabs.push_back(endFileStab);
	}
}




void Linker::collectDebugInfo()
{
	std::map<const class ObjectFile::Atom*, uint32_t>	atomOrdinals;
	fStartDebugTime = mach_absolute_time();
	if ( fOptions.readerOptions().fDebugInfoStripping != ObjectFile::ReaderOptions::kDebugInfoNone ) {

		// determine mixture of stabs and dwarf
		bool someStabs = false;
		bool someDwarf = false;
		for (std::vector<class ObjectFile::Reader*>::iterator it=fReadersThatHaveSuppliedAtoms.begin();
				it != fReadersThatHaveSuppliedAtoms.end();
				it++) {
			ObjectFile::Reader* reader = *it;
			if ( reader != NULL ) {
				switch ( reader->getDebugInfoKind() ) {
					case ObjectFile::Reader::kDebugInfoNone:
						break;
					case ObjectFile::Reader::kDebugInfoStabs:
						someStabs = true;
						break;
					case ObjectFile::Reader::kDebugInfoDwarf:
						someDwarf = true;
						fCreateUUID = true;
						break;
				    case ObjectFile::Reader::kDebugInfoStabsUUID:
						someStabs = true;
						fCreateUUID = true;
						break;
					default:
						throw "Unhandled type of debug information";
				}
			}
		}

		if ( someDwarf || someStabs ) {
			// try to minimize re-allocations
			fStabs.reserve(1024);

			// make mapping from atoms to ordinal
			uint32_t ordinal = 1;
			for (std::vector<ObjectFile::Atom*>::iterator it=fAllAtoms.begin(); it != fAllAtoms.end(); it++) {
				atomOrdinals[*it] = ordinal++;
			}
		}

		// process all dwarf .o files as a batch
		if ( someDwarf ) {
			// make mapping from readers with dwarf to ordinal
			std::map<class ObjectFile::Reader*, uint32_t>	readersWithDwarfOrdinals;
			uint32_t readerOrdinal = 1;
			for (std::vector<class ObjectFile::Reader*>::iterator it=fReadersThatHaveSuppliedAtoms.begin();
					it != fReadersThatHaveSuppliedAtoms.end();
					it++) {
				ObjectFile::Reader* reader = *it;
				if ( (reader != NULL) && (reader->getDebugInfoKind() == ObjectFile::Reader::kDebugInfoDwarf) ) {
					readersWithDwarfOrdinals[reader] = readerOrdinal++;
				}
			}

			// make a vector of atoms
			std::vector<class ObjectFile::Atom*> allAtomsByReader(fAllAtoms.begin(), fAllAtoms.end());
			// remove those not from a reader that has dwarf
			allAtomsByReader.erase(std::remove_if(allAtomsByReader.begin(), allAtomsByReader.end(),
								NoDebugNoteAtom(readersWithDwarfOrdinals)), allAtomsByReader.end());
			// sort by reader then atom ordinal
			std::sort(allAtomsByReader.begin(), allAtomsByReader.end(), ReadersWithDwarfSorter(readersWithDwarfOrdinals, atomOrdinals));
			// add debug notes for each atom
			this->synthesizeDebugNotes(allAtomsByReader);
		}

		// process all stabs .o files one by one
		if ( someStabs ) {
			// get stabs from each reader, in command line order
			for (std::vector<class ObjectFile::Reader*>::iterator it=fReadersThatHaveSuppliedAtoms.begin();
					it != fReadersThatHaveSuppliedAtoms.end();
					it++) {
				ObjectFile::Reader* reader = *it;
				if ( reader != NULL ) {
					switch ( reader->getDebugInfoKind() ) {
						case ObjectFile::Reader::kDebugInfoDwarf:
						case ObjectFile::Reader::kDebugInfoNone:
							// do nothing
							break;
						case ObjectFile::Reader::kDebugInfoStabs:
						case ObjectFile::Reader::kDebugInfoStabsUUID:
							collectStabs(reader, atomOrdinals);
							break;
						default:
							throw "Unhandled type of debug information";
					}
				}
			}
			// remove stabs associated with atoms that won't be in output
			std::set<class ObjectFile::Atom*>	allAtomsSet;
			allAtomsSet.insert(fAllAtoms.begin(), fAllAtoms.end());
			fStabs.erase(std::remove_if(fStabs.begin(), fStabs.end(), NotInSet(allAtomsSet)), fStabs.end());
		}
	}
}

void Linker::writeOutput()
{
	if ( fOptions.forceCpuSubtypeAll() )
		fCurrentCpuConstraint = ObjectFile::Reader::kCpuAny;
		
	fStartWriteTime = mach_absolute_time();
	// tell writer about each segment's atoms
	fOutputFileSize = fOutputFile->write(fAllAtoms, fStabs, this->entryPoint(), this->dyldHelper(),
											fCreateUUID, fCanScatter, 
											fCurrentCpuConstraint, fBiggerThanTwoGigOutput);
}

ObjectFile::Reader* Linker::createReader(const Options::FileInfo& info)
{
	// map in whole file
	uint64_t len = info.fileLen;
	int fd = ::open(info.path, O_RDONLY, 0);
	if ( fd == -1 )
		throwf("can't open file, errno=%d", errno);
	if ( info.fileLen < 20 )
		throw "file too small";

	uint8_t* p = (uint8_t*)::mmap(NULL, info.fileLen, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0);
	if ( p == (uint8_t*)(-1) )
		throwf("can't map file, errno=%d", errno);

	// if fat file, skip to architecture we want
	const fat_header* fh = (fat_header*)p;
	if ( fh->magic == OSSwapBigToHostInt32(FAT_MAGIC) ) {
		// Fat header is always big-endian
		const struct fat_arch* archs = (struct fat_arch*)(p + sizeof(struct fat_header));
		for (unsigned long i=0; i < OSSwapBigToHostInt32(fh->nfat_arch); ++i) {
			if ( OSSwapBigToHostInt32(archs[i].cputype) == (uint32_t)fArchitecture ) {
				uint32_t fileOffset = OSSwapBigToHostInt32(archs[i].offset);
				len = OSSwapBigToHostInt32(archs[i].size);
				// if requested architecture is page aligned within fat file, then remap just that portion of file
				if ( (fileOffset & 0x00000FFF) == 0 ) {
					// unmap whole file
					munmap((caddr_t)p, info.fileLen);
					// re-map just part we need
					p = (uint8_t*)::mmap(NULL, len, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, fileOffset);
					if ( p == (uint8_t*)(-1) )
						throwf("can't re-map file, errno=%d", errno);
				}
				else {
					p = &p[fileOffset];
				}
				break;
			}
		}
	}
	::close(fd);

	switch (fArchitecture) {
		case CPU_TYPE_POWERPC:
			if ( mach_o::relocatable::Reader<ppc>::validFile(p) )
				return this->addObject(new mach_o::relocatable::Reader<ppc>::Reader(p, info.path, info.modTime, fOptions.readerOptions(), fNextInputOrdinal), info, len);
			else if ( mach_o::dylib::Reader<ppc>::validFile(p, info.options.fBundleLoader) )
				return this->addDylib(new mach_o::dylib::Reader<ppc>::Reader(p, len, info.path, info.options.fBundleLoader, fOptions.readerOptions(), fNextInputOrdinal), info, len);
			else if ( mach_o::archive::Reader<ppc>::validFile(p, len) )
				return this->addArchive(new mach_o::archive::Reader<ppc>::Reader(p, len, info.path, info.modTime, fOptions.readerOptions(), fNextInputOrdinal), info, len);
			break;
		case CPU_TYPE_POWERPC64:
			if ( mach_o::relocatable::Reader<ppc64>::validFile(p) )
				return this->addObject(new mach_o::relocatable::Reader<ppc64>::Reader(p, info.path, info.modTime, fOptions.readerOptions(), fNextInputOrdinal), info, len);
			else if ( mach_o::dylib::Reader<ppc64>::validFile(p, info.options.fBundleLoader) )
				return this->addDylib(new mach_o::dylib::Reader<ppc64>::Reader(p, len, info.path, info.options.fBundleLoader, fOptions.readerOptions(), fNextInputOrdinal), info, len);
			else if ( mach_o::archive::Reader<ppc64>::validFile(p, len) )
				return this->addArchive(new mach_o::archive::Reader<ppc64>::Reader(p, len, info.path, info.modTime, fOptions.readerOptions(), fNextInputOrdinal), info, len);
			break;
		case CPU_TYPE_I386:
			if ( mach_o::relocatable::Reader<x86>::validFile(p) )
				return this->addObject(new mach_o::relocatable::Reader<x86>::Reader(p, info.path, info.modTime, fOptions.readerOptions(), fNextInputOrdinal), info, len);
			else if ( mach_o::dylib::Reader<x86>::validFile(p, info.options.fBundleLoader) )
				return this->addDylib(new mach_o::dylib::Reader<x86>::Reader(p, len, info.path, info.options.fBundleLoader, fOptions.readerOptions(), fNextInputOrdinal), info, len);
			else if ( mach_o::archive::Reader<x86>::validFile(p, len) )
				return this->addArchive(new mach_o::archive::Reader<x86>::Reader(p, len, info.path, info.modTime, fOptions.readerOptions(), fNextInputOrdinal), info, len);
			break;
		case CPU_TYPE_X86_64:
			if ( mach_o::relocatable::Reader<x86_64>::validFile(p) )
				return this->addObject(new mach_o::relocatable::Reader<x86_64>::Reader(p, info.path, info.modTime, fOptions.readerOptions(), fNextInputOrdinal), info, len);
			else if ( mach_o::dylib::Reader<x86_64>::validFile(p, info.options.fBundleLoader) )
				return this->addDylib(new mach_o::dylib::Reader<x86_64>::Reader(p, len, info.path, info.options.fBundleLoader, fOptions.readerOptions(), fNextInputOrdinal), info, len);
			else if ( mach_o::archive::Reader<x86_64>::validFile(p, len) )
				return this->addArchive(new mach_o::archive::Reader<x86_64>::Reader(p, len, info.path, info.modTime, fOptions.readerOptions(), fNextInputOrdinal), info, len);
			break;
	}

#if LLVM_SUPPORT
	if ( LLVMReader::validFile(p, info.path, fArchitecture, fOptions) ) {
		return this->addObject(LLVMReader::make(p, info.path, info.modTime, fOptions), info, len);
	}
#endif
	// error handling
	if ( ((fat_header*)p)->magic == OSSwapBigToHostInt32(FAT_MAGIC) ) {
		throwf("missing required architecture %s in file", fArchitectureName);
	}
	else {
		throw "file is not of required architecture";
	}
}

void Linker::logDylib(ObjectFile::Reader* reader, bool indirect)
{
	if ( fOptions.readerOptions().fTraceDylibs ) {
		const char* fullPath = reader->getPath();
		char realName[MAXPATHLEN];
		if ( realpath(fullPath, realName) != NULL )
			fullPath = realName;
		if ( indirect )
			logTraceInfo("[Logging for XBS] Used indirect dynamic library: %s\n", fullPath);
		else
			logTraceInfo("[Logging for XBS] Used dynamic library: %s\n", fullPath);
	}
}



ObjectFile::Reader* Linker::findDylib(const char* installPath, const char* fromPath)
{
	//fprintf(stderr, "findDylib(%s, %s)\n", installPath, fromPath);
	InstallNameToReader::iterator pos = fDylibMap.find(installPath);
	if ( pos != fDylibMap.end() ) {
		return pos->second;
	}
	else {
		// allow -dylib_path option to override indirect library to use
		for (std::vector<Options::DylibOverride>::iterator dit = fOptions.dylibOverrides().begin(); dit != fOptions.dylibOverrides().end(); ++dit) {
			if ( strcmp(dit->installName,installPath) == 0 ) {\
				try {
					Options::FileInfo info = fOptions.findFile(dit->useInstead);
					ObjectFile::Reader* reader = this->createReader(info);
					fDylibMap[strdup(installPath)] = reader;
					this->logDylib(reader, true);
					return reader;
				}
				catch (const char* msg) {
					fprintf(stderr, "ld: warning ignoring -dylib_file option, %s\n", msg);
				}
			}
		}
		char newPath[MAXPATHLEN];
		// handle @loader_path
		if ( strncmp(installPath, "@loader_path/", 13) == 0 ) {
			strcpy(newPath, fromPath);
			char* addPoint = strrchr(newPath,'/');
			if ( addPoint != NULL )
				strcpy(&addPoint[1], &installPath[13]);
			else
				strcpy(newPath, &installPath[13]);
			installPath = newPath;
		}
		// note: @executable_path case is handled inside findFileUsingPaths()
		// search for dylib using -F and -L paths
		Options::FileInfo info = fOptions.findFileUsingPaths(installPath);
		try {
			ObjectFile::Reader* reader = this->createReader(info);
			fDylibMap[strdup(installPath)] = reader;
			this->logDylib(reader, true);
			return reader;
		}
		catch (const char* msg) {
			throwf("in %s, %s", info.path, msg);
		}
	}
}


void Linker::processDylibs()
{
	fAllDirectDylibsLoaded = true;

	// mark all dylibs initially specified as required and check if they can be used
	for (InstallNameToReader::iterator it=fDylibMap.begin(); it != fDylibMap.end(); it++) {
		it->second->setExplicitlyLinked();
		this->checkDylibClientRestrictions(it->second);
	}
	
	// keep processing dylibs until no more dylibs are added
	unsigned long lastMapSize = 0;
	while ( lastMapSize != fDylibMap.size() ) {
		lastMapSize = fDylibMap.size();
		// can't iterator fDylibMap while modifying it, so use temp buffer
		std::vector<ObjectFile::Reader*> currentUnprocessedReaders;
		for (InstallNameToReader::iterator it=fDylibMap.begin(); it != fDylibMap.end(); it++) {
			if ( fDylibsProcessed.count(it->second) == 0 )
				currentUnprocessedReaders.push_back(it->second);
		}
		for (std::vector<ObjectFile::Reader*>::iterator it=currentUnprocessedReaders.begin(); it != currentUnprocessedReaders.end(); it++) {
			fDylibsProcessed.insert(*it);
			(*it)->processIndirectLibraries(this);
		}
	}
	
	// go back over original dylibs and mark sub frameworks as re-exported
	if ( fOptions.outputKind() == Options::kDynamicLibrary ) {
		const char* myLeaf = strrchr(fOptions.installPath(), '/');
		if ( myLeaf != NULL ) {
			for (std::vector<class ObjectFile::Reader*>::iterator it=fInputFiles.begin(); it != fInputFiles.end(); it++) {
				ObjectFile::Reader* reader = *it;
				const char* childParent = reader->parentUmbrella();
				if ( childParent != NULL ) {
					if ( strcmp(childParent, &myLeaf[1]) == 0 ) {
						// set re-export bit of info
						std::map<ObjectFile::Reader*,DynamicLibraryOptions>::iterator pos = fDylibOptionsMap.find(reader);
						if ( pos != fDylibOptionsMap.end() ) {
							pos->second.fReExport = true;
						}
					}
				}
			}
		}
	}
	
}

	

void Linker::createReaders()
{
	fStartCreateReadersTime = mach_absolute_time();
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
				this->addInputFile(this->createReader(entry), entry);
			}
			catch (const char* msg) {
				if ( strstr(msg, "architecture") != NULL ) {
					if (  fOptions.ignoreOtherArchInputFiles() ) {
						// ignore, because this is about an architecture not in use
					}
					else {
						fprintf(stderr, "ld: warning in %s, %s\n", entry.path, msg);
					}
				}
				else {
					throwf("in %s, %s", entry.path, msg);
				}
			}
		}
	}

	this->processDylibs();
}



ObjectFile::Reader* Linker::addArchive(ObjectFile::Reader* reader, const Options::FileInfo& info, uint64_t mappedLen)
{
	fNextInputOrdinal += mappedLen;
	// remember which readers are archives because they are logged differently
	fArchiveReaders.insert(reader);

	// update stats
	fTotalArchiveSize += mappedLen;
	++fTotalArchivesLoaded;
	return reader;
}

ObjectFile::Reader* Linker::addObject(ObjectFile::Reader* reader, const Options::FileInfo& info, uint64_t mappedLen)
{
	fNextInputOrdinal += mappedLen;
	// any .o files that don't have MH_SUBSECTIONS_VIA_SYMBOLS, that means a generated .o file can't
	if ( (fOptions.outputKind() == Options::kObjectFile) && !reader->canScatterAtoms() )
		fCanScatter = false;

	// update stats
	fTotalObjectSize += mappedLen;
	++fTotalObjectLoaded;
	return reader;
}


void Linker::checkDylibClientRestrictions(ObjectFile::Reader* reader)
{
	// Check for any restrictions on who can link with this dylib  
	const char* readerParentName = reader->parentUmbrella() ;
	std::vector<const char*>* clients = reader->getAllowableClients();
	if ( (readerParentName != NULL) || (clients != NULL) ) {
		// only dylibs that are in an umbrella or have a client list need verification
		const char* installName = fOptions.installPath();
		const char* installNameLastSlash = strrchr(installName, '/');
		bool isParent = false;
		bool isSibling = false;
		bool isAllowableClient = false;
		// There are three cases:
		if ( (readerParentName != NULL) && (installNameLastSlash != NULL) ) {
			// case 1) The dylib has a parent umbrella, and we are creating the parent umbrella
			isParent = ( strcmp(&installNameLastSlash[1], readerParentName) == 0 );
			
			// hack to support umbrella variants that encode the variant name in the install name 
			// e.g. CoreServices_profile
			if ( !isParent ) {
				const char* underscore = strchr(&installNameLastSlash[1], '_');
				if ( underscore != NULL ) {
					isParent = ( strncmp(&installNameLastSlash[1], readerParentName, underscore-installNameLastSlash-1) == 0 );
				}
			}
			
			// case 2) The dylib has a parent umbrella, and we are creating a sibling with the same parent
			isSibling = ( (fOptions.umbrellaName() != NULL) && (strcmp(fOptions.umbrellaName(), readerParentName) == 0) );
		}

		if ( !isParent && !isSibling && (clients != NULL) ) {
			// case 3) the dylib has a list of allowable clients, and we are creating one of them
			const char* clientName = fOptions.clientName();
			int clientNameLen = 0;
			if ( clientName != NULL ) {
				// use client name as specified on command line
				clientNameLen = strlen(clientName);
			}
			else {
				// infer client name from output path (e.g. xxx/libfoo_variant.A.dylib --> foo, Bar.framework/Bar_variant --> Bar)
				clientName = installName;
				clientNameLen = strlen(clientName);
				// starts after last slash
				if ( installNameLastSlash != NULL )
					clientName = &installNameLastSlash[1];
				if ( strncmp(clientName, "lib", 3) == 0 )
					clientName = &clientName[3];
				// up to first dot
				const char* firstDot = strchr(clientName, '.');
				if ( firstDot != NULL )
					clientNameLen = firstDot - clientName;
				// up to first underscore
				const char* firstUnderscore = strchr(clientName, '_');
				if ( (firstUnderscore != NULL) && ((firstUnderscore - clientName) < clientNameLen) )
					clientNameLen = firstUnderscore - clientName;
			}

			// Use clientName to check if this dylib is able to link against the allowable clients.
			for (std::vector<const char*>::iterator it = clients->begin(); it != clients->end(); it++) {
				if ( strncmp(*it, clientName, clientNameLen) == 0 )
					isAllowableClient = true;
			}
		}
	
		if ( !isParent && !isSibling && !isAllowableClient ) {
			if ( readerParentName != NULL ) {
				throwf("cannot link directly with %s.  Link against the umbrella framework '%s.framework' instead.", 
					reader->getPath(), readerParentName);
			}
			else {
				throwf("cannot link directly with %s", reader->getPath());
			}
		}
	}


}

ObjectFile::Reader* Linker::addDylib(ObjectFile::Reader* reader, const Options::FileInfo& info, uint64_t mappedLen)
{
	fNextInputOrdinal += mappedLen;
	if ( (reader->getInstallPath() == NULL) && !info.options.fBundleLoader ) {
		// this is a "blank" stub
		// silently ignore it
		return reader;
	}
	// add to map of loaded dylibs
	const char* installPath = reader->getInstallPath();
	if ( installPath != NULL ) {
		InstallNameToReader::iterator pos = fDylibMap.find(installPath);
		if ( pos == fDylibMap.end() ) {
			fDylibMap[strdup(installPath)] = reader;
		}
		else {
			InstallNameToReader::iterator pos2 = fDylibMap.find(reader->getPath());
			if ( pos2 == fDylibMap.end() ) 
				fDylibMap[strdup(reader->getPath())] = reader;
			else
				fprintf(stderr, "ld: warning, duplicate dylib %s\n", reader->getPath());
		}
	}
	else if ( info.options.fBundleLoader )
		fBundleLoaderReader = reader;

	// log direct readers
	if ( !fAllDirectDylibsLoaded ) 
		this->logDylib(reader, false);

	// update stats
	++fTotalDylibsLoaded;

	return reader;
}


void Linker::logTraceInfo (const char* format, ...)
{
	static int trace_file = -1;
	char trace_buffer[MAXPATHLEN * 2];
	char *buffer_ptr;
	int length;
	ssize_t amount_written;
	const char *trace_file_path = fOptions.readerOptions().fTraceOutputFile;

	if(trace_file == -1) {
		if(trace_file_path != NULL) {
			trace_file = open(trace_file_path, O_WRONLY | O_APPEND | O_CREAT, 0666);
			if(trace_file == -1)
				throwf("Could not open or create trace file: %s\n", trace_file_path);
		}
		else {
			trace_file = fileno(stderr);
		}
	}

    va_list ap;
	va_start(ap, format);
	length = vsnprintf(trace_buffer, sizeof(trace_buffer), format, ap);
	va_end(ap);
	buffer_ptr = trace_buffer;

	while(length > 0) {
		amount_written = write(trace_file, buffer_ptr, length);
		if(amount_written == -1)
			/* Failure to write shouldn't fail the build. */
			return;
		buffer_ptr += amount_written;
		length -= amount_written;
	}
}



void Linker::createWriter()
{
	fStartCreateWriterTime = mach_absolute_time();

	// make a vector out of all required dylibs in fDylibMap
	std::vector<ExecutableFile::DyLibUsed>	dynamicLibraries;
	// need to preserve command line order 
	for (std::vector<class ObjectFile::Reader*>::iterator it=fInputFiles.begin(); it != fInputFiles.end(); it++) {
		ObjectFile::Reader* reader = *it;
		for (InstallNameToReader::iterator mit=fDylibMap.begin(); mit != fDylibMap.end(); mit++) {
			if ( reader == mit->second ) {
				ExecutableFile::DyLibUsed dylibInfo;
				dylibInfo.reader = reader;
				dylibInfo.options = fDylibOptionsMap[reader];
				dynamicLibraries.push_back(dylibInfo);
				break;
			}
		}
	}
	// then add any other dylibs
	for (InstallNameToReader::iterator it=fDylibMap.begin(); it != fDylibMap.end(); it++) {
		if ( it->second->implicitlyLinked()  ) {
			// if not already in dynamicLibraries
			bool alreadyInDynamicLibraries = false;
			for (std::vector<ExecutableFile::DyLibUsed>::iterator dit=dynamicLibraries.begin(); dit != dynamicLibraries.end(); dit++) {
				if ( dit->reader == it->second ) {
					alreadyInDynamicLibraries = true;
					break;
				}
			}
			if ( ! alreadyInDynamicLibraries ) {	
				ExecutableFile::DyLibUsed dylibInfo;
				dylibInfo.reader = it->second;
				std::map<ObjectFile::Reader*,DynamicLibraryOptions>::iterator pos = fDylibOptionsMap.find(it->second);
				if ( pos != fDylibOptionsMap.end() ) {
					dylibInfo.options = pos->second;
				}
				else {
					dylibInfo.options.fWeakImport = false;		// FIX ME
					dylibInfo.options.fReExport = false;
					dylibInfo.options.fBundleLoader = false;
				}
				dynamicLibraries.push_back(dylibInfo);
			}
		}
	}
	if ( fBundleLoaderReader != NULL ) {
		ExecutableFile::DyLibUsed dylibInfo;
		dylibInfo.reader = fBundleLoaderReader;
		dylibInfo.options.fWeakImport = false;		
		dylibInfo.options.fReExport = false;
		dylibInfo.options.fBundleLoader = true;
		dynamicLibraries.push_back(dylibInfo);
	}

	const char* path = fOptions.getOutputFilePath();
	switch ( fArchitecture ) {
		case CPU_TYPE_POWERPC:
			this->setOutputFile(new mach_o::executable::Writer<ppc>(path, fOptions, dynamicLibraries));
			break;
		case CPU_TYPE_POWERPC64:
			this->setOutputFile(new mach_o::executable::Writer<ppc64>(path, fOptions, dynamicLibraries));
			break;
		case CPU_TYPE_I386:
			this->setOutputFile(new mach_o::executable::Writer<x86>(path, fOptions, dynamicLibraries));
			break;
		case CPU_TYPE_X86_64:
			this->setOutputFile(new mach_o::executable::Writer<x86_64>(path, fOptions, dynamicLibraries));
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

// convenience labels for 2-dimensional switch statement
enum AllDefinitionCombinations {
	kRegAndReg				= (ObjectFile::Atom::kRegularDefinition << 3)	| ObjectFile::Atom::kRegularDefinition,
	kRegAndWeak				= (ObjectFile::Atom::kRegularDefinition << 3)	| ObjectFile::Atom::kWeakDefinition,
	kRegAndTent				= (ObjectFile::Atom::kRegularDefinition << 3)	| ObjectFile::Atom::kTentativeDefinition,
	kRegAndExtern			= (ObjectFile::Atom::kRegularDefinition << 3)	| ObjectFile::Atom::kExternalDefinition,
	kRegAndExternWeak		= (ObjectFile::Atom::kRegularDefinition << 3)	| ObjectFile::Atom::kExternalWeakDefinition,
	kRegAndAbsolute			= (ObjectFile::Atom::kRegularDefinition << 3)	| ObjectFile::Atom::kAbsoluteSymbol,
	kWeakAndReg				= (ObjectFile::Atom::kWeakDefinition << 3)		| ObjectFile::Atom::kRegularDefinition,
	kWeakAndWeak			= (ObjectFile::Atom::kWeakDefinition << 3)		| ObjectFile::Atom::kWeakDefinition,
	kWeakAndTent			= (ObjectFile::Atom::kWeakDefinition << 3)		| ObjectFile::Atom::kTentativeDefinition,
	kWeakAndExtern			= (ObjectFile::Atom::kWeakDefinition << 3)		| ObjectFile::Atom::kExternalDefinition,
	kWeakAndExternWeak		= (ObjectFile::Atom::kWeakDefinition << 3)		| ObjectFile::Atom::kExternalWeakDefinition,
	kWeakAndAbsolute		= (ObjectFile::Atom::kWeakDefinition << 3)		| ObjectFile::Atom::kAbsoluteSymbol,
	kTentAndReg				= (ObjectFile::Atom::kTentativeDefinition << 3) | ObjectFile::Atom::kRegularDefinition,
	kTentAndWeak			= (ObjectFile::Atom::kTentativeDefinition << 3) | ObjectFile::Atom::kWeakDefinition,
	kTentAndTent			= (ObjectFile::Atom::kTentativeDefinition << 3) | ObjectFile::Atom::kTentativeDefinition,
	kTentAndExtern			= (ObjectFile::Atom::kTentativeDefinition << 3) | ObjectFile::Atom::kExternalDefinition,
	kTentAndExternWeak		= (ObjectFile::Atom::kTentativeDefinition << 3) | ObjectFile::Atom::kExternalWeakDefinition,
	kTentAndAbsolute		= (ObjectFile::Atom::kTentativeDefinition << 3)	| ObjectFile::Atom::kAbsoluteSymbol,
	kExternAndReg			= (ObjectFile::Atom::kExternalDefinition << 3)	| ObjectFile::Atom::kRegularDefinition,
	kExternAndWeak			= (ObjectFile::Atom::kExternalDefinition << 3)	| ObjectFile::Atom::kWeakDefinition,
	kExternAndTent			= (ObjectFile::Atom::kExternalDefinition << 3)	| ObjectFile::Atom::kTentativeDefinition,
	kExternAndExtern		= (ObjectFile::Atom::kExternalDefinition << 3)	| ObjectFile::Atom::kExternalDefinition,
	kExternAndExternWeak	= (ObjectFile::Atom::kExternalDefinition << 3)	| ObjectFile::Atom::kExternalWeakDefinition,
	kExternAndAbsolute		= (ObjectFile::Atom::kExternalDefinition << 3)	| ObjectFile::Atom::kAbsoluteSymbol,
	kExternWeakAndReg		= (ObjectFile::Atom::kExternalWeakDefinition << 3) | ObjectFile::Atom::kRegularDefinition,
	kExternWeakAndWeak		= (ObjectFile::Atom::kExternalWeakDefinition << 3) | ObjectFile::Atom::kWeakDefinition,
	kExternWeakAndTent		= (ObjectFile::Atom::kExternalWeakDefinition << 3) | ObjectFile::Atom::kTentativeDefinition,
	kExternWeakAndExtern	= (ObjectFile::Atom::kExternalWeakDefinition << 3) | ObjectFile::Atom::kExternalDefinition,
	kExternWeakAndExternWeak= (ObjectFile::Atom::kExternalWeakDefinition << 3) | ObjectFile::Atom::kExternalWeakDefinition,
	kExternWeakAndAbsolute	= (ObjectFile::Atom::kExternalWeakDefinition << 3) | ObjectFile::Atom::kAbsoluteSymbol,
	kAbsoluteAndReg			= (ObjectFile::Atom::kAbsoluteSymbol << 3)		| ObjectFile::Atom::kRegularDefinition,
	kAbsoluteAndWeak		= (ObjectFile::Atom::kAbsoluteSymbol << 3)		| ObjectFile::Atom::kWeakDefinition,
	kAbsoluteAndTent		= (ObjectFile::Atom::kAbsoluteSymbol << 3)		| ObjectFile::Atom::kTentativeDefinition,
	kAbsoluteAndExtern		= (ObjectFile::Atom::kAbsoluteSymbol << 3)		| ObjectFile::Atom::kExternalDefinition,
	kAbsoluteAndExternWeak	= (ObjectFile::Atom::kAbsoluteSymbol << 3)		| ObjectFile::Atom::kExternalWeakDefinition,
	kAbsoluteAndAbsolute	= (ObjectFile::Atom::kAbsoluteSymbol << 3)		| ObjectFile::Atom::kAbsoluteSymbol
};

bool Linker::SymbolTable::add(ObjectFile::Atom& newAtom)
{
	bool useNew = true;
	const char* name = newAtom.getName();
	//fprintf(stderr, "map.add(%s => %p from %s)\n", name, &newAtom, newAtom.getFile()->getPath());
	Mapper::iterator pos = fTable.find(name);
	ObjectFile::Atom* existingAtom = NULL;
	if ( pos != fTable.end() )
		existingAtom = pos->second;
	if ( existingAtom != NULL ) {
		// already have atom with same name in symbol table
		switch ( (AllDefinitionCombinations)((existingAtom->getDefinitionKind() << 3) | newAtom.getDefinitionKind()) ) {
			case kRegAndReg:
				throwf("duplicate symbol %s in %s and %s\n", name, newAtom.getFile()->getPath(), existingAtom->getFile()->getPath());
			case kRegAndWeak:
				// ignore new weak atom, because we already have a non-weak one
				useNew = false;
				break;
			case kRegAndTent:
				// ignore new tentative atom, because we already have a regular one
				useNew = false;
				if ( newAtom.getSize() > existingAtom->getSize() ) {
					fprintf(stderr, "ld: warning for symbol %s tentative definition of size %llu from %s is "
									"is smaller than the real definition of size %llu from %s\n",
									newAtom.getDisplayName(), newAtom.getSize(), newAtom.getFile()->getPath(),
									existingAtom->getSize(), existingAtom->getFile()->getPath());
				}
				break;
			case kRegAndExtern:
				// ignore external atom, because we already have a one
				useNew = false;
				break;
			case kRegAndExternWeak:
				// ignore external atom, because we already have a one
				useNew = false;
				break;
			case kRegAndAbsolute:
				throwf("duplicate symbol %s in %s and %s\n", name, newAtom.getFile()->getPath(), existingAtom->getFile()->getPath());
				break;
			case kWeakAndReg:
				// replace existing weak atom with regular one
				break;
			case kWeakAndWeak:
				// have another weak atom, use whichever has largest alignment requirement
				// because codegen of some client may require alignment
				useNew = ( newAtom.getAlignment().trailingZeros() > existingAtom->getAlignment().trailingZeros() );
				break;
			case kWeakAndTent:
				// replace existing weak atom with tentative one ???
				break;
			case kWeakAndExtern:
				// keep weak atom, at runtime external one may override
				useNew = false;
				break;
			case kWeakAndExternWeak:
				// keep weak atom, at runtime external one may override
				useNew = false;
				break;
			case kWeakAndAbsolute:
				// replace existing weak atom with absolute one
				break;
			case kTentAndReg:
				// replace existing tentative atom with regular one
				if ( newAtom.getSize() < existingAtom->getSize() ) {
					fprintf(stderr, "ld: warning for symbol %s tentative definition of size %llu from %s is "
									"being replaced by a real definition of size %llu from %s\n",
									newAtom.getDisplayName(), existingAtom->getSize(), existingAtom->getFile()->getPath(),
									newAtom.getSize(), newAtom.getFile()->getPath());
				}
				break;
			case kTentAndWeak:
				// replace existing tentative atom with weak one ???
				break;
			case kTentAndTent:
				// use largest
				if ( newAtom.getSize() < existingAtom->getSize() ) {
					useNew = false;
				} 
				else {
					if ( newAtom.getAlignment().trailingZeros() < existingAtom->getAlignment().trailingZeros() )
						fprintf(stderr, "ld: warning alignment lost in merging tentative definition %s\n", newAtom.getDisplayName());
				}
				break;
			case kTentAndExtern:
			case kTentAndExternWeak:
				// a tentative definition and a dylib definition, so commons-mode decides how to handle
				switch ( fOwner.fOptions.commonsMode() ) {
					case Options::kCommonsIgnoreDylibs:
						if ( fOwner.fOptions.warnCommons() )
							fprintf(stderr, "ld: using common symbol %s from %s and ignoring defintion from dylib %s\n",
									existingAtom->getName(), existingAtom->getFile()->getPath(), newAtom.getFile()->getPath());
						useNew = false;
						break;
					case Options::kCommonsOverriddenByDylibs:
						if ( fOwner.fOptions.warnCommons() )
							fprintf(stderr, "ld: replacing common symbol %s from %s with true definition from dylib %s\n",
									existingAtom->getName(), existingAtom->getFile()->getPath(), newAtom.getFile()->getPath());
						break;
					case Options::kCommonsConflictsDylibsError:
						throwf("common symbol %s from %s conflicts with defintion from dylib %s",
								existingAtom->getName(), existingAtom->getFile()->getPath(), newAtom.getFile()->getPath());
				}
				break;
			case kTentAndAbsolute:
				// replace tentative with absolute (can't size check because absolutes have no size)
				break;
			case kExternAndReg:
				// replace external atom with regular one
				break;
			case kExternAndWeak:
				// replace external atom with weak one
				break;
			case kExternAndTent:
				// a tentative definition and a dylib definition, so commons-mode decides how to handle
				switch ( fOwner.fOptions.commonsMode() ) {
					case Options::kCommonsIgnoreDylibs:
						if ( fOwner.fOptions.warnCommons() )
							fprintf(stderr, "ld: using common symbol %s from %s and ignoring defintion from dylib %s\n",
									newAtom.getName(), newAtom.getFile()->getPath(), existingAtom->getFile()->getPath());
						break;
					case Options::kCommonsOverriddenByDylibs:
						if ( fOwner.fOptions.warnCommons() )
							fprintf(stderr, "ld: replacing defintion of %s from dylib %s with common symbol from %s\n",
									newAtom.getName(), existingAtom->getFile()->getPath(), newAtom.getFile()->getPath());
						useNew = false;
						break;
					case Options::kCommonsConflictsDylibsError:
						throwf("common symbol %s from %s conflicts with defintion from dylib %s",
									newAtom.getName(), newAtom.getFile()->getPath(), existingAtom->getFile()->getPath());
				}
				break;
			case kExternAndExtern:
				throwf("duplicate symbol %s in %s and %s\n", name, newAtom.getFile()->getPath(), existingAtom->getFile()->getPath());
			case kExternAndExternWeak:
				// keep strong dylib atom, ignore weak one
				useNew = false;
				break;
			case kExternAndAbsolute:
				// replace external atom with absolute one
				break;
			case kExternWeakAndReg:
				// replace existing weak external with regular
				break;
			case kExternWeakAndWeak:
				// replace existing weak external with weak (let dyld decide at runtime which to use)
				break;
			case kExternWeakAndTent:
				// a tentative definition and a dylib definition, so commons-mode decides how to handle
				switch ( fOwner.fOptions.commonsMode() ) {
					case Options::kCommonsIgnoreDylibs:
						if ( fOwner.fOptions.warnCommons() )
							fprintf(stderr, "ld: using common symbol %s from %s and ignoring defintion from dylib %s\n",
									newAtom.getName(), newAtom.getFile()->getPath(), existingAtom->getFile()->getPath());
						break;
					case Options::kCommonsOverriddenByDylibs:
						if ( fOwner.fOptions.warnCommons() )
							fprintf(stderr, "ld: replacing defintion of %s from dylib %s with common symbol from %s\n",
									newAtom.getName(), existingAtom->getFile()->getPath(), newAtom.getFile()->getPath());
						useNew = false;
						break;
					case Options::kCommonsConflictsDylibsError:
						throwf("common symbol %s from %s conflicts with defintion from dylib %s",
									newAtom.getName(), newAtom.getFile()->getPath(), existingAtom->getFile()->getPath());
				}
				break;
			case kExternWeakAndExtern:
				// replace existing weak external with external
				break;
			case kExternWeakAndExternWeak:
				// keep existing external weak
				useNew = false;
				break;
			case kExternWeakAndAbsolute:
				// replace existing weak external with absolute
				break;
			case kAbsoluteAndReg:
				throwf("duplicate symbol %s in %s and %s\n", name, newAtom.getFile()->getPath(), existingAtom->getFile()->getPath());
			case kAbsoluteAndWeak:
				// ignore new weak atom, because we already have a non-weak one
				useNew = false;
				break;
			case kAbsoluteAndTent:
				// ignore new tentative atom, because we already have a regular one
				useNew = false;
				break;
			case kAbsoluteAndExtern:
				// ignore external atom, because we already have a one
				useNew = false;
				break;
			case kAbsoluteAndExternWeak:
				// ignore external atom, because we already have a one
				useNew = false;
				break;
			case kAbsoluteAndAbsolute:
				throwf("duplicate symbol %s in %s and %s\n", name, newAtom.getFile()->getPath(), existingAtom->getFile()->getPath());
				break;
		}
	}
	if ( (existingAtom != NULL) && (newAtom.getScope() != existingAtom->getScope()) ) {
		fprintf(stderr, "ld: warning %s has different visibility (%d) in %s and (%d) in %s\n", 
			newAtom.getDisplayName(), newAtom.getScope(), newAtom.getFile()->getPath(), existingAtom->getScope(), existingAtom->getFile()->getPath());
	}
	if ( useNew ) {
		fTable[name] = &newAtom;
		if ( existingAtom != NULL )
			fOwner.fDeadAtoms.insert(existingAtom);
	}
	else {
		fOwner.fDeadAtoms.insert(&newAtom);
	}
	return useNew;
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
		if ( (it->second == NULL) || (andWeakDefintions && (it->second->getDefinitionKind()==ObjectFile::Atom::kWeakDefinition)) ) {
			undefines.push_back(it->first);
		}
	}
}




bool Linker::AtomSorter::operator()(const ObjectFile::Atom* left, const ObjectFile::Atom* right)
{
	if ( left == right )
		return false;

	// first sort by section order (which is already sorted by segment)
	unsigned int leftSectionIndex  =  left->getSection()->getIndex();
	unsigned int rightSectionIndex = right->getSection()->getIndex();
	if ( leftSectionIndex != rightSectionIndex)
		return (leftSectionIndex < rightSectionIndex);

	// if a -order_file is specified, then sorting is altered to sort those symbols first
	if ( fOverriddenOrdinalMap != NULL ) {
		std::map<const ObjectFile::Atom*, uint32_t>::iterator leftPos  = fOverriddenOrdinalMap->find(left);
		std::map<const ObjectFile::Atom*, uint32_t>::iterator rightPos = fOverriddenOrdinalMap->find(right);
		std::map<const ObjectFile::Atom*, uint32_t>::iterator end = fOverriddenOrdinalMap->end();
		if ( leftPos != end ) {
			if ( rightPos != end ) {
				// both left and right are overridden, so compare overridden ordinals
				return leftPos->second < rightPos->second;
			}
			else {
				// left is overridden and right is not, so left < right
				return true;
			}
		}
		else {
			if ( rightPos != end ) {
				// right is overridden and left is not, so right < left
				return false;
			}
			else {
				// neither are overridden, do default sort
				// fall into default sorting below
			}
		}
	}
	
	// the __common section can have real or tentative definitions
	// we want the real ones to sort before tentative ones
	bool leftIsTent  =  (left->getDefinitionKind() == ObjectFile::Atom::kTentativeDefinition);
	bool rightIsTent =  (right->getDefinitionKind() == ObjectFile::Atom::kTentativeDefinition);
	if ( leftIsTent != rightIsTent )
		return rightIsTent; 
	
	// lastly sort by atom ordinal.  this is already sorted by .o order
	return left->getOrdinal() < right->getOrdinal();
}


int main(int argc, const char* argv[])
{
	const char* archName = NULL;
	bool showArch = false;
	bool archInferred = false;
	try {
		// create linker object given command line arguments
		Linker ld(argc, argv);

		// save error message prefix
		archName = ld.architectureName();
		archInferred = ld.isInferredArchitecture();
		showArch = ld.showArchitectureInErrors();

		// open all input files
		ld.createReaders();

		// open output file
		ld.createWriter();

		// do linking
		ld.link();
	}
	catch (const char* msg) {
		if ( archInferred )
			fprintf(stderr, "ld: %s for inferred architecture %s\n", msg, archName);
		else if ( showArch )
			fprintf(stderr, "ld: %s for architecture %s\n", msg, archName);
		else
			fprintf(stderr, "ld: %s\n", msg);
		return 1;
	}

	return 0;
}
