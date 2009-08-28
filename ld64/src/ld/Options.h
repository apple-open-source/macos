/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
 *
 * Copyright (c) 2005-2007 Apple  Inc. All rights reserved.
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

#include <vector>
#include <ext/hash_set>
#include <ext/hash_map>

#include "ObjectFile.h"

extern void throwf (const char* format, ...) __attribute__ ((noreturn));
extern void warning(const char* format, ...);

class LibraryOptions
{
public:
	LibraryOptions() : fWeakImport(false), fReExport(false), fBundleLoader(false), fLazyLoad(false), fForceLoad(false) {}
	// for dynamic libraries
	bool		fWeakImport;
	bool		fReExport;
	bool		fBundleLoader;
	bool		fLazyLoad;
	// for static libraries
	bool		fForceLoad;
};



//
// The public interface to the Options class is the abstract representation of what work the linker
// should do.
//
// This abstraction layer will make it easier to support a future where the linker is a shared library
// invoked directly from Xcode.  The target settings in Xcode would be used to directly construct an Options
// object (without building a command line which is then parsed).
//
//
class Options
{
public:
	Options(int argc, const char* argv[]);
	~Options();

	enum OutputKind { kDynamicExecutable, kStaticExecutable, kDynamicLibrary, kDynamicBundle, kObjectFile, kDyld, kPreload, kKextBundle };
	enum NameSpace { kTwoLevelNameSpace, kFlatNameSpace, kForceFlatNameSpace };
	// Standard treatment for many options.
	enum Treatment { kError, kWarning, kSuppress, kNULL, kInvalid };
	enum UndefinedTreatment { kUndefinedError, kUndefinedWarning, kUndefinedSuppress, kUndefinedDynamicLookup };
	enum WeakReferenceMismatchTreatment { kWeakReferenceMismatchError, kWeakReferenceMismatchWeak,
										  kWeakReferenceMismatchNonWeak };
	enum CommonsMode { kCommonsIgnoreDylibs, kCommonsOverriddenByDylibs, kCommonsConflictsDylibsError };
	enum DeadStripMode { kDeadStripOff, kDeadStripOn, kDeadStripOnPlusUnusedInits };
	enum UUIDMode { kUUIDNone, kUUIDRandom, kUUIDContent };
	enum LocalSymbolHandling { kLocalSymbolsAll, kLocalSymbolsNone, kLocalSymbolsSelectiveInclude, kLocalSymbolsSelectiveExclude };

	struct FileInfo {
		const char*				path;
		uint64_t				fileLen;
		time_t					modTime;
		LibraryOptions			options;
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

	struct OrderedSymbol {
		const char*				symbolName;
		const char*				objectFileName;
	};

	struct SegmentStart {
		const char*				name;
		uint64_t				address;
	};
	
	struct SegmentSize {
		const char*				name;
		uint64_t				size;
	};
	
	struct SegmentProtect {
		const char*				name;
		uint32_t				max;
		uint32_t				init;
	};
	
	struct DylibOverride {
		const char*				installName;
		const char*				useInstead;
	};


	const ObjectFile::ReaderOptions&	readerOptions();
	const char*							getOutputFilePath();
	std::vector<FileInfo>&				getInputFiles();

	cpu_type_t					architecture() { return fArchitecture; }
	bool						preferSubArchitecture() { return fHasPreferredSubType; }
	cpu_subtype_t				subArchitecture() { return fSubArchitecture; }
	bool						allowSubArchitectureMismatches() { return fAllowCpuSubtypeMismatches; }
	OutputKind					outputKind();
	bool						prebind();
	bool						bindAtLoad();
	bool						fullyLoadArchives();
	NameSpace					nameSpace();
	const char*					installPath();			// only for kDynamicLibrary
	uint32_t					currentVersion();		// only for kDynamicLibrary
	uint32_t					compatibilityVersion();	// only for kDynamicLibrary
	const char*					entryName();			// only for kDynamicExecutable or kStaticExecutable
	const char*					executablePath();
	uint64_t					baseAddress();
	bool						keepPrivateExterns();			// only for kObjectFile
	bool						needsModuleTable();				// only for kDynamicLibrary
	bool						interposable(const char* name);
	bool						hasExportRestrictList();		// -exported_symbol or -unexported_symbol
	bool						hasExportMaskList();			// just -exported_symbol
	bool						hasWildCardExportRestrictList();
	bool						allGlobalsAreDeadStripRoots();
	bool						shouldExport(const char*);
	bool						ignoreOtherArchInputFiles();
	bool						forceCpuSubtypeAll();
	bool						traceDylibs();
	bool						traceArchives();
	DeadStripMode				deadStrip();
	UndefinedTreatment			undefinedTreatment();
	ObjectFile::ReaderOptions::MacVersionMin	macosxVersionMin() { return fReaderOptions.fMacVersionMin; }
	ObjectFile::ReaderOptions::IPhoneVersionMin	iphoneOSVersionMin() { return fReaderOptions.fIPhoneVersionMin; }
	bool						minOS(ObjectFile::ReaderOptions::MacVersionMin mac, ObjectFile::ReaderOptions::IPhoneVersionMin iPhoneOS);
	bool						messagesPrefixedWithArchitecture();
	Treatment					picTreatment();
	WeakReferenceMismatchTreatment	weakReferenceMismatchTreatment();
	const char*					umbrellaName();
	std::vector<const char*>&	allowableClients();
	const char*					clientName();
	const char*					initFunctionName();			// only for kDynamicLibrary
	const char*					dotOutputFile();
	uint64_t					zeroPageSize();
	bool						hasCustomStack();
	uint64_t					customStackSize();
	uint64_t					customStackAddr();
	bool						hasExecutableStack();
	std::vector<const char*>&	initialUndefines();
	bool						printWhyLive(const char* name);
	uint32_t					minimumHeaderPad();
	uint64_t					segmentAlignment() { return fSegmentAlignment; }
	bool						maxMminimumHeaderPad() { return fMaxMinimumHeaderPad; }
	std::vector<ExtraSection>&	extraSections();
	std::vector<SectionAlignment>&	sectionAlignments();
	CommonsMode					commonsMode();
	bool						warnCommons();
	bool						keepRelocations();
	FileInfo					findFile(const char* path);
	UUIDMode					getUUIDMode() { return fUUIDMode; }
	bool						warnStabs();
	bool						pauseAtEnd() { return fPause; }
	bool						printStatistics() { return fStatistics; }
	bool						printArchPrefix() { return fMessagesPrefixedWithArchitecture; }
	void						gotoClassicLinker(int argc, const char* argv[]);
	bool						sharedRegionEligible() { return fSharedRegionEligible; }
	bool						printOrderFileStatistics() { return fPrintOrderFileStatistics; }
	const char*					dTraceScriptName() { return fDtraceScriptName; }
	bool						dTrace() { return (fDtraceScriptName != NULL); }
	std::vector<OrderedSymbol>&	orderedSymbols() { return fOrderedSymbols; }
	bool						splitSeg() { return fSplitSegs; }
	uint64_t					baseWritableAddress() { return fBaseWritableAddress; }
	std::vector<SegmentStart>&	customSegmentAddresses() { return fCustomSegmentAddresses; }
	std::vector<SegmentSize>&	customSegmentSizes() { return fCustomSegmentSizes; }
	std::vector<SegmentProtect>& customSegmentProtections() { return fCustomSegmentProtections; }
	bool						saveTempFiles() { return fSaveTempFiles; }
	const std::vector<const char*>&   rpaths() { return fRPaths; }
	bool						readOnlyx86Stubs() { return fReadOnlyx86Stubs; }
	std::vector<DylibOverride>&	dylibOverrides() { return fDylibOverrides; }
	const char*					generatedMapPath() { return fMapPath; }
	bool						positionIndependentExecutable() { return fPositionIndependentExecutable; }
	Options::FileInfo			findFileUsingPaths(const char* path);
	bool						deadStripDylibs() { return fDeadStripDylibs; }
	bool						allowedUndefined(const char* name) { return ( fAllowedUndefined.find(name) != fAllowedUndefined.end() ); }
	bool						someAllowedUndefines() { return (fAllowedUndefined.size() != 0); }
	LocalSymbolHandling			localSymbolHandling() { return fLocalSymbolHandling; }
	bool						keepLocalSymbol(const char* symbolName);
	bool						allowTextRelocs() { return fAllowTextRelocs; }
	bool						warnAboutTextRelocs() { return fWarnTextRelocs; }
	bool						usingLazyDylibLinking() { return fUsingLazyDylibLinking; }
	bool						verbose() { return fVerbose; }
	bool						makeEncryptable() { return fEncryptable; }
	bool						needsUnwindInfoSection() { return fReaderOptions.fAddCompactUnwindEncoding; }
	std::vector<const char*>&	llvmOptions() { return fLLVMOptions; }
	bool						makeClassicDyldInfo() { return fMakeClassicDyldInfo; }
	bool						makeCompressedDyldInfo() { return fMakeCompressedDyldInfo; }
	bool						hasExportedSymbolOrder();
	bool						exportedSymbolOrder(const char* sym, unsigned int* order);
	bool						orderData() { return fOrderData; }
	bool						errorOnOtherArchFiles() { return fErrorOnOtherArchFiles; }
	bool						markAutoDeadStripDylib() { return fMarkDeadStrippableDylib; }
	bool						removeEHLabels() { return fReaderOptions.fNoEHLabels; }

private:
	class CStringEquals
	{
	public:
		bool operator()(const char* left, const char* right) const { return (strcmp(left, right) == 0); }
	};
	typedef __gnu_cxx::hash_map<const char*, unsigned int, __gnu_cxx::hash<const char*>, CStringEquals> NameToOrder;
	typedef __gnu_cxx::hash_set<const char*, __gnu_cxx::hash<const char*>, CStringEquals>  NameSet;
	enum ExportMode { kExportDefault, kExportSome, kDontExportSome };
	enum LibrarySearchMode { kSearchDylibAndArchiveInEachDir, kSearchAllDirsForDylibsThenAllDirsForArchives };
	enum InterposeMode { kInterposeNone, kInterposeAllExternal, kInterposeSome };

	class SetWithWildcards {
	public:
		void					insert(const char*);
		bool					contains(const char*);
		bool					hasWildCards()	{ return !fWildCard.empty(); }
		NameSet::iterator		regularBegin()	{ return fRegular.begin(); }
		NameSet::iterator		regularEnd()	{ return fRegular.end(); }
	private:
		static bool				hasWildCards(const char*);
		bool					wildCardMatch(const char* pattern, const char* candidate);
		bool					inCharRange(const char*& range, unsigned char c);

		NameSet							fRegular;
		std::vector<const char*>		fWildCard;
	};


	void						parse(int argc, const char* argv[]);
	void						checkIllegalOptionCombinations();
	void						buildSearchPaths(int argc, const char* argv[]);
	void						parseArch(const char* architecture);
	FileInfo					findLibrary(const char* rootName, bool dylibsOnly=false);
	FileInfo					findFramework(const char* frameworkName);
	FileInfo					findFramework(const char* rootName, const char* suffix);
	bool						checkForFile(const char* format, const char* dir, const char* rootName,
											 FileInfo& result);
	uint32_t					parseVersionNumber(const char*);
	void						parseSectionOrderFile(const char* segment, const char* section, const char* path);
	void						parseOrderFile(const char* path, bool cstring);
	void						addSection(const char* segment, const char* section, const char* path);
	void						addSubLibrary(const char* name);
	void						loadFileList(const char* fileOfPaths);
	uint64_t					parseAddress(const char* addr);
	void						loadExportFile(const char* fileOfExports, const char* option, SetWithWildcards& set);
	void						parseAliasFile(const char* fileOfAliases);
	void						parsePreCommandLineEnvironmentSettings();
	void						parsePostCommandLineEnvironmentSettings();
	void						setUndefinedTreatment(const char* treatment);
	void						setMacOSXVersionMin(const char* version);
	void						setIPhoneVersionMin(const char* version);
	void						setWeakReferenceMismatchTreatment(const char* treatment);
	void						addDylibOverride(const char* paths);
	void						addSectionAlignment(const char* segment, const char* section, const char* alignment);
	CommonsMode					parseCommonsTreatment(const char* mode);
	Treatment					parseTreatment(const char* treatment);
	void						reconfigureDefaults();
	void						checkForClassic(int argc, const char* argv[]);
	void						parseSegAddrTable(const char* segAddrPath, const char* installPath);
	void						addLibrary(const FileInfo& info);
	void						warnObsolete(const char* arg);
	uint32_t					parseProtection(const char* prot);
	void						loadSymbolOrderFile(const char* fileOfExports, NameToOrder& orderMapping);


	ObjectFile::ReaderOptions			fReaderOptions;
	const char*							fOutputFile;
	std::vector<Options::FileInfo>		fInputFiles;
	cpu_type_t							fArchitecture;
	cpu_subtype_t						fSubArchitecture;
	OutputKind							fOutputKind;
	bool								fHasPreferredSubType;
	bool								fPrebind;
	bool								fBindAtLoad;
	bool								fKeepPrivateExterns;
	bool								fNeedsModuleTable;
	bool								fIgnoreOtherArchFiles;
	bool								fErrorOnOtherArchFiles;
	bool								fForceSubtypeAll;
	InterposeMode						fInterposeMode;
	DeadStripMode						fDeadStrip;
	NameSpace							fNameSpace;
	uint32_t							fDylibCompatVersion;
	uint32_t							fDylibCurrentVersion;
	const char*							fDylibInstallName;
	const char*							fFinalName;
	const char*							fEntryName;
	uint64_t							fBaseAddress;
	uint64_t							fBaseWritableAddress;
	bool								fSplitSegs;
	SetWithWildcards					fExportSymbols;
	SetWithWildcards					fDontExportSymbols;
	SetWithWildcards					fInterposeList;
	NameToOrder							fExportSymbolsOrder;
	ExportMode							fExportMode;
	LibrarySearchMode					fLibrarySearchMode;
	UndefinedTreatment					fUndefinedTreatment;
	bool								fMessagesPrefixedWithArchitecture;
	WeakReferenceMismatchTreatment		fWeakReferenceMismatchTreatment;
	std::vector<const char*>			fSubUmbellas;
	std::vector<const char*>			fSubLibraries;
	std::vector<const char*>			fAllowableClients;
	std::vector<const char*>			fRPaths;
	const char*							fClientName;
	const char*							fUmbrellaName;
	const char*							fInitFunctionName;
	const char*							fDotOutputFile;
	const char*							fExecutablePath;
	const char*							fBundleLoader;
	const char*							fDtraceScriptName;
	const char*							fSegAddrTablePath;
	const char*							fMapPath;
	uint64_t							fZeroPageSize;
	uint64_t							fStackSize;
	uint64_t							fStackAddr;
	bool								fExecutableStack;
	uint32_t							fMinimumHeaderPad;
	uint64_t							fSegmentAlignment;
	CommonsMode							fCommonsMode;
	UUIDMode							fUUIDMode;
	SetWithWildcards					fLocalSymbolsIncluded;
	SetWithWildcards					fLocalSymbolsExcluded;
	LocalSymbolHandling					fLocalSymbolHandling;
	bool								fWarnCommons;
	bool								fVerbose;
	bool								fKeepRelocations;
	bool								fWarnStabs;
	bool								fTraceDylibSearching;
	bool								fPause;
	bool								fStatistics;
	bool								fPrintOptions;
	bool								fSharedRegionEligible;
	bool								fPrintOrderFileStatistics;
	bool								fReadOnlyx86Stubs;
	bool								fPositionIndependentExecutable;
	bool								fMaxMinimumHeaderPad;
	bool								fDeadStripDylibs;
	bool								fAllowTextRelocs;
	bool								fWarnTextRelocs;
	bool								fUsingLazyDylibLinking;
	bool								fEncryptable;
	bool								fOrderData;
	bool								fMarkDeadStrippableDylib;
	bool								fMakeClassicDyldInfo;
	bool								fMakeCompressedDyldInfo;
	bool								fNoEHLabels;
	bool								fAllowCpuSubtypeMismatches;
	std::vector<const char*>			fInitialUndefines;
	NameSet								fAllowedUndefined;
	NameSet								fWhyLive;
	std::vector<ExtraSection>			fExtraSections;
	std::vector<SectionAlignment>		fSectionAlignments;
	std::vector<OrderedSymbol>			fOrderedSymbols;
	std::vector<SegmentStart>			fCustomSegmentAddresses;
	std::vector<SegmentSize>			fCustomSegmentSizes;
	std::vector<SegmentProtect>			fCustomSegmentProtections;
	std::vector<DylibOverride>			fDylibOverrides; 
	std::vector<const char*>			fLLVMOptions;
	std::vector<const char*>			fLibrarySearchPaths;
	std::vector<const char*>			fFrameworkSearchPaths;
	std::vector<const char*>			fSDKPaths;
	bool								fSaveTempFiles;
};



#endif // __OPTIONS__
