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

#ifndef __OPTIONS__
#define __OPTIONS__


#include <stdint.h>
#include <mach/machine.h>

#ifndef CPU_TYPE_POWERPC64
#define CPU_TYPE_POWERPC64 0x1000012
#endif


#include <vector>
#include <ext/hash_set>

#include "ObjectFile.h"

extern  __attribute__((noreturn)) void throwf(const char* format, ...);


class DynamicLibraryOptions 
{
public:
				DynamicLibraryOptions() : fWeakImport(false), fReExport(false), fInstallPathOverride(NULL) {}
				
	bool		fWeakImport;
	bool		fReExport;
	const char* fInstallPathOverride;
};


class Options
{
public:
								Options(int argc, const char* argv[]);
								~Options();

	enum OutputKind { kDynamicExecutable, kStaticExecutable, kDynamicLibrary, kDynamicBundle, kObjectFile, kDyld };
	enum NameSpace { kTwoLevelNameSpace, kFlatNameSpace, kForceFlatNameSpace };
	enum UndefinedTreatment { kUndefinedError, kUndefinedWarning, kUndefinedSuppress, kUndefinedDynamicLookup };
	enum PICTreatment { kPICError, kPICWarning, kPICSuppress };
	enum WeakReferenceMismatchTreatment { kWeakReferenceMismatchError, kWeakReferenceMismatchWeak, kWeakReferenceMismatchNonWeak };
	enum CommonsMode { kCommonsIgnoreDylibs, kCommonsOverriddenByDylibs, kCommonsConflictsDylibsError };

	struct FileInfo {
		const char*				path;
		uint64_t				fileLen;
		DynamicLibraryOptions	options;
	};
	
	struct ExtraSection {
		const char*				segmentName;
		const char*				sectionName;
		const char*				path;
		const uint8_t*			data;
		uint64_t				dataLen;
	};
	
	struct SectionAlignment {
		const char*				segmentName;
		const char*				sectionName;
		uint8_t					alignment;
	};

	ObjectFile::ReaderOptions&	readerOptions();
	const char*					getOutputFilePath();
	std::vector<FileInfo>&		getInputFiles();
	
	cpu_type_t					architecture();
	OutputKind					outputKind();
	bool						stripLocalSymbols();
	bool						stripDebugInfo();
	bool						bindAtLoad();
	bool						fullyLoadArchives();
	NameSpace					nameSpace();
	const char*					installPath();			// only for kDynamicLibrary
	uint32_t					currentVersion();		// only for kDynamicLibrary
	uint32_t					compatibilityVersion();	// only for kDynamicLibrary
	const char*					entryName();			// only for kDynamicExecutable or kStaticExecutable
	uint64_t					baseAddress();
	bool						keepPrivateExterns();	// only for kObjectFile
	bool						interposable();			// only for kDynamicLibrary 
	bool						hasExportRestrictList();
	bool						shouldExport(const char*);
	bool						ignoreOtherArchInputFiles();
	bool						forceCpuSubtypeAll();
	bool						traceDylibs();
	bool						traceArchives();
	UndefinedTreatment			undefinedTreatment();
	bool						messagesPrefixedWithArchitecture();
	PICTreatment				picTreatment();
	WeakReferenceMismatchTreatment	weakReferenceMismatchTreatment();
	const char*					umbrellaName();
	const char*					initFunctionName();			// only for kDynamicLibrary
	uint64_t					zeroPageSize();
	bool						hasCustomStack();
	uint64_t					customStackSize();
	uint64_t					customStackAddr();
	std::vector<const char*>&	initialUndefines();
	uint32_t					minimumHeaderPad();
	std::vector<ExtraSection>&	extraSections();
	std::vector<SectionAlignment>&	sectionAlignments();
	CommonsMode					commonsMode();
	bool						warnCommons();
	FileInfo					findFile(const char* path);

private:
	class CStringEquals
	{
	public:
		bool operator()(const char* left, const char* right) const { return (strcmp(left, right) == 0); }
	};

	typedef __gnu_cxx::hash_set<const char*, __gnu_cxx::hash<const char*>, CStringEquals>  NameSet;
	enum ExportMode { kExportDefault, kExportSome, kDontExportSome };
	enum LibrarySearchMode { kSearchDylibAndArchiveInEachDir, kSearchAllDirsForDylibsThenAllDirsForArchives };

	void						parse(int argc, const char* argv[]);
	void						checkIllegalOptionCombinations();
	void						buildSearchPaths(int argc, const char* argv[]);
	void						parseArch(const char* architecture);
	FileInfo					findLibrary(const char* rootName);
	FileInfo					findFramework(const char* rootName);
	bool						checkForFile(const char* format, const char* dir, const char* rootName, FileInfo& result);
	uint32_t					parseVersionNumber(const char*);
	void						parseSectionOrderFile(const char* segment, const char* section, const char* path);
	void						addSection(const char* segment, const char* section, const char* path);
	void						addSubLibrary(const char* name);
	void						loadFileList(const char* fileOfPaths);
	uint64_t					parseAddress(const char* addr);
	void						loadExportFile(const char* fileOfExports, const char* option, NameSet& set);
	void						parsePreCommandLineEnvironmentSettings();
	void						parsePostCommandLineEnvironmentSettings();
	void						setUndefinedTreatment(const char* treatment);
	void						setPICTreatment(const char* treatment);
	void						setReadOnlyRelocTreatment(const char* treatment);
	void						setWeakReferenceMismatchTreatment(const char* treatment);
	void						setDylibInstallNameOverride(const char* paths);
	void						setExecutablePath(const char* path);
	void						addSectionAlignment(const char* segment, const char* section, const char* alignment);
	CommonsMode					parseCommonsTreatment(const char* mode);
		
		

	ObjectFile::ReaderOptions			fReaderOptions;
	const char*							fOutputFile;
	std::vector<Options::FileInfo>		fInputFiles;
	cpu_type_t							fArchitecture;
	OutputKind							fOutputKind;
	bool								fBindAtLoad;
	bool								fStripLocalSymbols;
	bool								fKeepPrivateExterns;
	bool								fInterposable;
	bool								fIgnoreOtherArchFiles;
	bool								fForceSubtypeAll;
	NameSpace							fNameSpace;
	uint32_t							fDylibCompatVersion;
	uint32_t							fDylibCurrentVersion;
	const char*							fDylibInstallName;
	const char*							fEntryName;
	uint64_t							fBaseAddress;
	NameSet								fExportSymbols;
	NameSet								fDontExportSymbols;
	ExportMode							fExportMode;
	LibrarySearchMode					fLibrarySearchMode;
	UndefinedTreatment					fUndefinedTreatment;
	bool								fMessagesPrefixedWithArchitecture;
	PICTreatment						fPICTreatment;
	WeakReferenceMismatchTreatment		fWeakReferenceMismatchTreatment;
	std::vector<const char*>			fSubUmbellas;
	std::vector<const char*>			fSubLibraries;
	const char*							fUmbrellaName;
	const char*							fInitFunctionName;
	uint64_t							fZeroPageSize;
	uint64_t							fStackSize;
	uint64_t							fStackAddr;
	uint32_t							fMinimumHeaderPad;
	CommonsMode							fCommonsMode;
	bool								fWarnCommons;
	bool								fVerbose;
	std::vector<const char*>			fInitialUndefines;
	std::vector<ExtraSection>			fExtraSections;
	std::vector<SectionAlignment>		fSectionAlignments;
	
	std::vector<const char*>			fLibrarySearchPaths;
	std::vector<const char*>			fFrameworkSearchPaths;
	std::vector<const char*>			fSDKPaths;

};




#endif // __OPTIONS__











