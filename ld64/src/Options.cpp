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


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "Options.h"

 __attribute__((noreturn))
void throwf(const char* format, ...) 
{
	va_list	list;
	char*	p;
	va_start(list, format);
	vasprintf(&p, format, list);
	va_end(list);
	
	const char*	t = p;
	throw t;
}


Options::Options(int argc, const char* argv[])
 : fOutputFile("a.out"), fArchitecture(CPU_TYPE_POWERPC64), fOutputKind(kDynamicExecutable), fBindAtLoad(false),
	fStripLocalSymbols(false),  fKeepPrivateExterns(false),
	fInterposable(false), fIgnoreOtherArchFiles(false), fForceSubtypeAll(false), fNameSpace(kTwoLevelNameSpace),
	fDylibCompatVersion(0), fDylibCurrentVersion(0), fDylibInstallName(NULL), fEntryName("start"), fBaseAddress(0),
	fExportMode(kExportDefault), fLibrarySearchMode(kSearchAllDirsForDylibsThenAllDirsForArchives),
	fUndefinedTreatment(kUndefinedError), fMessagesPrefixedWithArchitecture(false), fPICTreatment(kPICError),
	fWeakReferenceMismatchTreatment(kWeakReferenceMismatchError), 
	fUmbrellaName(NULL), fInitFunctionName(NULL), fZeroPageSize(0x1000), fStackSize(0), fStackAddr(0), fMinimumHeaderPad(0),
	fCommonsMode(kCommonsIgnoreDylibs), fWarnCommons(false)
{
	this->parsePreCommandLineEnvironmentSettings();
	this->parse(argc, argv);
	this->parsePostCommandLineEnvironmentSettings();
	this->checkIllegalOptionCombinations();
}

Options::~Options()
{
}


ObjectFile::ReaderOptions& Options::readerOptions()
{
	return fReaderOptions;
}

cpu_type_t Options::architecture()
{
	return fArchitecture;
}


const char*	Options::getOutputFilePath()
{
	return fOutputFile;
}


std::vector<Options::FileInfo>& Options::getInputFiles()
{
	return fInputFiles;
}

Options::OutputKind	Options::outputKind()
{
	return fOutputKind;
}

bool Options::stripLocalSymbols()
{
	return fStripLocalSymbols;
}

bool Options::stripDebugInfo()
{
	return fReaderOptions.fStripDebugInfo;
}

bool Options::bindAtLoad()
{
	return fBindAtLoad;
}

bool Options::fullyLoadArchives()
{
	return fReaderOptions.fFullyLoadArchives;
}

Options::NameSpace Options::nameSpace()
{
	return fNameSpace;
}

const char*	Options::installPath()
{
	if ( fDylibInstallName != NULL ) 
		return fDylibInstallName;
	else
		return fOutputFile;
}

uint32_t Options::currentVersion()
{
	return fDylibCurrentVersion;
}

uint32_t Options::compatibilityVersion()
{
	return fDylibCompatVersion;
}

const char*	Options::entryName()
{
	return fEntryName;
}

uint64_t Options::baseAddress()
{
	return fBaseAddress;
}

bool Options::keepPrivateExterns()
{
	return fKeepPrivateExterns;
}

bool Options::interposable()
{
	return fInterposable;
}

bool Options::ignoreOtherArchInputFiles()
{
	return fIgnoreOtherArchFiles;
}

bool Options::forceCpuSubtypeAll()
{
	return fForceSubtypeAll;
}

bool Options::traceDylibs()
{
	return fReaderOptions.fTraceDylibs;
}

bool Options::traceArchives()
{
	return fReaderOptions.fTraceArchives;
}

Options::UndefinedTreatment Options::undefinedTreatment()
{
	return fUndefinedTreatment;
}

Options::WeakReferenceMismatchTreatment	Options::weakReferenceMismatchTreatment()
{
	return fWeakReferenceMismatchTreatment;
}

const char* Options::umbrellaName()
{
	return fUmbrellaName;
}

uint64_t Options::zeroPageSize()
{
	return fZeroPageSize;
}

bool Options::hasCustomStack()
{
	return (fStackSize != 0);
}
	
uint64_t Options::customStackSize()
{
	return fStackSize;
}

uint64_t Options::customStackAddr()
{
	return fStackAddr;
}

std::vector<const char*>& Options::initialUndefines()
{
	return fInitialUndefines;
}

const char*	Options::initFunctionName()
{
	return fInitFunctionName;
}

bool Options::hasExportRestrictList()
{
	return (fExportMode != kExportDefault);
}

uint32_t Options::minimumHeaderPad()
{
	return fMinimumHeaderPad;
}

std::vector<Options::ExtraSection>&	Options::extraSections()
{
	return fExtraSections;
}

std::vector<Options::SectionAlignment>&	Options::sectionAlignments()
{
	return fSectionAlignments;
}


Options::CommonsMode Options::commonsMode()
{
	return fCommonsMode;
}

bool Options::warnCommons()
{
	return fWarnCommons;
}


bool Options::shouldExport(const char* symbolName)
{
	switch (fExportMode) {
		case kExportSome:
			return ( fExportSymbols.find(symbolName) != fExportSymbols.end() );
		case kDontExportSome:
			return ( fDontExportSymbols.find(symbolName) == fDontExportSymbols.end() );
		case kExportDefault:
			return true;
	}
	throw "internal error";
}


void Options::parseArch(const char* architecture)
{
	if ( architecture == NULL )
		throw "-arch must be followed by an architecture string";
	if ( strcmp(architecture, "ppc") == 0 )
		fArchitecture = CPU_TYPE_POWERPC;
	else if ( strcmp(architecture, "ppc64") == 0 )
		fArchitecture = CPU_TYPE_POWERPC64;
	else if ( strcmp(architecture, "i386") == 0 )
		fArchitecture = CPU_TYPE_I386;
	else 
		throw "-arch followed by unknown architecture name";
}

bool Options::checkForFile(const char* format, const char* dir, const char* rootName, FileInfo& result)
{
	struct stat statBuffer;
	char possiblePath[strlen(dir)+strlen(rootName)+20];
	sprintf(possiblePath, format,  dir, rootName);
	if ( stat(possiblePath, &statBuffer) == 0 ) {
		result.path = strdup(possiblePath);
		result.fileLen = statBuffer.st_size;
		return true;
	}
	return false;
} 


Options::FileInfo Options::findLibrary(const char* rootName)
{
	FileInfo result;
	const int rootNameLen = strlen(rootName);
	// if rootName ends in .o there is no .a vs .dylib choice
	if ( (rootNameLen > 3) && (strcmp(&rootName[rootNameLen-2], ".o") == 0) ) {
		for (std::vector<const char*>::iterator it = fLibrarySearchPaths.begin(); it != fLibrarySearchPaths.end(); it++) {
			const char* dir = *it;
			if ( checkForFile("%s/%s", dir, rootName, result) )
				return result;
		}
	}
	else {
		bool lookForDylibs = ( fOutputKind != Options::kDyld);
		switch ( fLibrarySearchMode ) {
			case kSearchAllDirsForDylibsThenAllDirsForArchives:
				// first look in all directories for just for dylibs
				if ( lookForDylibs ) {
					for (std::vector<const char*>::iterator it = fLibrarySearchPaths.begin(); it != fLibrarySearchPaths.end(); it++) {
						const char* dir = *it;
						if ( checkForFile("%s/lib%s.dylib", dir, rootName, result) )
							return result;
					}
				}
				// next look in all directories for just for archives
				for (std::vector<const char*>::iterator it = fLibrarySearchPaths.begin(); it != fLibrarySearchPaths.end(); it++) {
					const char* dir = *it;
					if ( checkForFile("%s/lib%s.a", dir, rootName, result) )
						return result;
				}
				break;

			case kSearchDylibAndArchiveInEachDir:
				// look in each directory for just for a dylib then for an archive
				for (std::vector<const char*>::iterator it = fLibrarySearchPaths.begin(); it != fLibrarySearchPaths.end(); it++) {
					const char* dir = *it;
					if ( lookForDylibs && checkForFile("%s/lib%s.dylib", dir, rootName, result) )
						return result;
					if ( checkForFile("%s/lib%s.a", dir, rootName, result) )
						return result;
				}
				break;
		}
	}
	throwf("library not found for -l%s", rootName);
}


Options::FileInfo Options::findFramework(const char* rootName)
{
	struct stat statBuffer;
	const int rootNameLen = strlen(rootName);
	for (std::vector<const char*>::iterator it = fFrameworkSearchPaths.begin(); it != fFrameworkSearchPaths.end(); it++) {
		const char* dir = *it;
		char possiblePath[strlen(dir)+2*rootNameLen+20];
		strcpy(possiblePath, dir);
		strcat(possiblePath, "/");
		strcat(possiblePath, rootName);
		strcat(possiblePath, ".framework/");
		strcat(possiblePath, rootName);
		if ( stat(possiblePath, &statBuffer) == 0 ) {
			FileInfo result;
			result.path = strdup(possiblePath);
			result.fileLen = statBuffer.st_size;
			return result;
		}
	}
	throwf("framework not found %s", rootName);
}


Options::FileInfo Options::makeFileInfo(const char* path)
{
	struct stat statBuffer;
	if ( stat(path, &statBuffer) == 0 ) {
		FileInfo result;
		result.path = strdup(path);
		result.fileLen = statBuffer.st_size;
		return result;
	}
	else {
		throwf("file not found: %s", path);
	}
}

void Options::loadFileList(const char* fileOfPaths)
{
	FILE* file = fopen(fileOfPaths, "r");
	if ( file == NULL ) 
		throwf("-filelist file not found: %s\n", fileOfPaths);
	
	char path[1024];
	while ( fgets(path, 1024, file) != NULL ) {
		path[1023] = '\0';
		char* eol = strchr(path, '\n');
		if ( eol != NULL )
			*eol = '\0';
			
		fInputFiles.push_back(makeFileInfo(path));
	}
	fclose(file);
}


void Options::loadExportFile(const char* fileOfExports, const char* option, NameSet& set)
{
	// read in whole file
	int fd = ::open(fileOfExports, O_RDONLY, 0);
	if ( fd == -1 )
		throwf("can't open %s file: %s", option, fileOfExports);
	struct stat stat_buf;
	::fstat(fd, &stat_buf);
	char* p = (char*)malloc(stat_buf.st_size);
	if ( p == NULL )
		throwf("can't process %s file: %s", option, fileOfExports);
	
	if ( read(fd, p, stat_buf.st_size) != stat_buf.st_size )
		throwf("can't read %s file: %s", option, fileOfExports);
	
	::close(fd);
	
	// parse into symbols and add to hash_set
	char * const end = &p[stat_buf.st_size];
	enum { lineStart, inSymbol, inComment } state = lineStart;
	char* symbolStart = NULL;
	for (char* s = p; s < end; ++s ) {
		switch ( state ) {
			case lineStart:
				if ( *s =='#' ) {
					state = inComment;
				}
				else if ( !isspace(*s) ) {
					state = inSymbol;
					symbolStart = s;
				}
				break;
			case inSymbol:
				if ( *s == '\n' ) {
					*s = '\0';
					// removing any trailing spaces
					char* last = s-1;
					while ( isspace(*last) ) {
						*last = '\0';
						--last;
					}
					set.insert(symbolStart);
					symbolStart = NULL;
					state = lineStart;
				}
				break;
			case inComment:
				if ( *s == '\n' )
					state = lineStart;
				break;
		}
	}
	// Note: we do not free() the malloc buffer, because the strings are used by the export-set hash table
}

void Options::setUndefinedTreatment(const char* treatment)
{
	if ( treatment == NULL ) 
		throw "-undefined missing [ warning | error | suppress | dynamic_lookup ]";

	if ( strcmp(treatment, "warning") == 0 )
		fUndefinedTreatment = kUndefinedWarning;
	else if ( strcmp(treatment, "error") == 0 )
		fUndefinedTreatment = kUndefinedError;
	else if ( strcmp(treatment, "suppress") == 0 )
		fUndefinedTreatment = kUndefinedSuppress;
	else if ( strcmp(treatment, "dynamic_lookup") == 0 )
		fUndefinedTreatment = kUndefinedDynamicLookup;
	else
		throw "invalid option to -undefined [ warning | error | suppress | dynamic_lookup ]";
}

void Options::setReadOnlyRelocTreatment(const char* treatment)
{
	if ( treatment == NULL ) 
		throw "-read_only_relocs missing [ warning | error | suppress ]";

	if ( strcmp(treatment, "warning") == 0 )
		throw "-read_only_relocs warning not supported";
	else if ( strcmp(treatment, "suppress") == 0 )
		throw "-read_only_relocs suppress not supported";
	else if ( strcmp(treatment, "error") != 0 )
		throw "invalid option to -read_only_relocs [ warning | error | suppress | dynamic_lookup ]";
}

void Options::setPICTreatment(const char* treatment)
{
	if ( treatment == NULL ) 
		throw "-sect_diff_relocs missing [ warning | error | suppress ]";

	if ( strcmp(treatment, "warning") == 0 )
		fPICTreatment = kPICWarning;
	else if ( strcmp(treatment, "error") == 0 )
		fPICTreatment = kPICError;
	else if ( strcmp(treatment, "suppress") == 0 )
		fPICTreatment = kPICSuppress;
	else
		throw "invalid option to -sect_diff_relocs [ warning | error | suppress ]";
}

void Options::setWeakReferenceMismatchTreatment(const char* treatment)
{
	if ( treatment == NULL ) 
		throw "-weak_reference_mismatches missing [ error | weak | non-weak ]";

	if ( strcmp(treatment, "error") == 0 )
		fWeakReferenceMismatchTreatment = kWeakReferenceMismatchError;
	else if ( strcmp(treatment, "weak") == 0 )
		fWeakReferenceMismatchTreatment = kWeakReferenceMismatchWeak;
	else if ( strcmp(treatment, "non-weak") == 0 )
		fWeakReferenceMismatchTreatment = kWeakReferenceMismatchNonWeak;
	else
		throw "invalid option to -weak_reference_mismatches [ error | weak | non-weak ]";
}

Options::CommonsMode Options::parseCommonsTreatment(const char* mode)
{
	if ( mode == NULL ) 
		throw "-commons missing [ ignore_dylibs | use_dylibs | error ]";

	if ( strcmp(mode, "ignore_dylibs") == 0 )
		return kCommonsIgnoreDylibs;
	else if ( strcmp(mode, "use_dylibs") == 0 )
		return kCommonsOverriddenByDylibs;
	else if ( strcmp(mode, "error") == 0 )
		return kCommonsConflictsDylibsError;
	else
		throw "invalid option to -commons [ ignore_dylibs | use_dylibs | error ]";
}


void Options::setDylibInstallNameOverride(const char* paths)
{


}

void Options::setExecutablePath(const char* path)
{


}




uint64_t Options::parseAddress(const char* addr)
{
	char* endptr;
	uint64_t result = strtoull(addr, &endptr, 16);
	return result;
}



//
// Parses number of form X[.Y[.Z]] into a uint32_t where the nibbles are xxxx.yy.zz 
//
//
uint32_t Options::parseVersionNumber(const char* versionString)
{
	unsigned long x = 0;
	unsigned long y = 0;
	unsigned long z = 0;
	char* end;
	x = strtoul(versionString, &end, 10);
	if ( *end == '.' ) {
		y = strtoul(&end[1], &end, 10);
		if ( *end == '.' ) {
			z = strtoul(&end[1], &end, 10);
		}
	}
	if ( (*end != '\0') || (x > 0xffff) || (y > 0xff) || (z > 0xff) )
		throwf("malformed version number: %s", versionString);

	return (x << 16) | ( y << 8 ) | z;
}

void Options::parseSectionOrderFile(const char* segment, const char* section, const char* path)
{
	fprintf(stderr, "ld64: warning -sectorder not yet supported for 64-bit code\n");
}

void Options::addSection(const char* segment, const char* section, const char* path)
{
	if ( strlen(segment) > 16 )
		throw "-seccreate segment name max 16 chars";
	if ( strlen(section) > 16 )
		throw "-seccreate section name max 16 chars";

	// read in whole file
	int fd = ::open(path, O_RDONLY, 0);
	if ( fd == -1 )
		throwf("can't open -sectcreate file: %s", path);
	struct stat stat_buf;
	::fstat(fd, &stat_buf);
	char* p = (char*)malloc(stat_buf.st_size);
	if ( p == NULL )
		throwf("can't process -sectcreate file: %s", path);
	if ( read(fd, p, stat_buf.st_size) != stat_buf.st_size )
		throwf("can't read -sectcreate file: %s", path);
	::close(fd);
	
	// record section to create
	ExtraSection info = { segment, section, path, (uint8_t*)p, stat_buf.st_size };
	fExtraSections.push_back(info);
}

void Options::addSectionAlignment(const char* segment, const char* section, const char* alignmentStr)
{
	if ( strlen(segment) > 16 )
		throw "-sectalign segment name max 16 chars";
	if ( strlen(section) > 16 )
		throw "-sectalign section name max 16 chars";

	char* endptr;
	unsigned long value = strtoul(alignmentStr, &endptr, 16);
	if ( *endptr != '\0')
		throw "argument for -sectalign is not a hexadecimal number";
	if ( value > 0x8000 )
		throw "argument for -sectalign must be less than or equal to 0x8000";
	uint8_t alignment = 0;
	for(unsigned long x=value; x != 1; x >>= 1)
		++alignment;
	if ( (unsigned long)(1 << alignment) != value )
		throw "argument for -sectalign is not a power of two";

	SectionAlignment info = { segment, section, alignment };
	fSectionAlignments.push_back(info);
}


void Options::parse(int argc, const char* argv[])
{
	// pass one builds search list from -L and -F options
	this->buildSearchPaths(argc, argv);
	
	// pass two parse all other options
	for(int i=1; i < argc; ++i) {
		const char* arg = argv[i];
		
		if ( arg[0] == '-' ) {
			if ( (arg[1] == 'L') || (arg[1] == 'F') ) {
				// previously handled
			}
			else if ( strcmp(arg, "-arch") == 0 ) {
				parseArch(argv[++i]);
			}
			else if ( strcmp(arg, "-dynamic") == 0 ) {
				// default
			}
			else if ( strcmp(arg, "-static") == 0 ) {
				fOutputKind = kStaticExecutable;
			}
			else if ( strcmp(arg, "-dylib") == 0 ) {
				fOutputKind = kDynamicLibrary;
			}
			else if ( strcmp(arg, "-bundle") == 0 ) {
				fOutputKind = kDynamicBundle;
			}
			else if ( strcmp(arg, "-dylinker") == 0 ) {
				fOutputKind = kDyld;
			}
			else if ( strcmp(arg, "-execute") == 0 ) {
				if ( fOutputKind != kStaticExecutable )
					fOutputKind = kDynamicExecutable;
			}
			else if ( strcmp(arg, "-r") == 0 ) {
				fOutputKind = kObjectFile;
			}
			else if ( strcmp(arg, "-o") == 0 ) {
				fOutputFile = argv[++i];
			}
			else if ( arg[1] == 'l' ) {
				fInputFiles.push_back(findLibrary(&arg[2]));
			}
			else if ( strcmp(arg, "-weak-l") == 0 ) {
				FileInfo info = findLibrary(&arg[2]);
				info.options.fWeakImport = true;
				fInputFiles.push_back(info);
			}
			else if ( strcmp(arg, "-bind_at_load") == 0 ) {
				fBindAtLoad = true;
			}
			else if ( strcmp(arg, "-twolevel_namespace") == 0 ) {
				fNameSpace = kTwoLevelNameSpace;
			}
			else if ( strcmp(arg, "-flat_namespace") == 0 ) {
				fNameSpace = kFlatNameSpace;
			}
			else if ( strcmp(arg, "-force_flat_namespace") == 0 ) {
				fNameSpace = kForceFlatNameSpace;
			}
			else if ( strcmp(arg, "-all_load") == 0 ) {
				fReaderOptions.fFullyLoadArchives = true;
			}
			else if ( strcmp(arg, "-ObjC") == 0 ) {
				fReaderOptions.fLoadObjcClassesInArchives = true;
			}
			else if ( strcmp(arg, "-dylib_compatibility_version") == 0 ) {
				fDylibCompatVersion = parseVersionNumber(argv[++i]);
			}
			else if ( strcmp(arg, "-dylib_current_version") == 0 ) {
				fDylibCurrentVersion = parseVersionNumber(argv[++i]);
			}
			else if ( strcmp(arg, "-sectorder") == 0 ) {
				parseSectionOrderFile(argv[i+1], argv[i+2], argv[i+3]);
				i += 3;
			}
			else if ( (strcmp(arg, "-sectcreate") == 0) || (strcmp(arg, "-segcreate") == 0) ) {
				addSection(argv[i+1], argv[i+2], argv[i+3]);
				i += 3;
			}
			else if ( (strcmp(arg, "-dylib_install_name") == 0) || (strcmp(arg, "-dylinker_install_name") == 0) ) {
				fDylibInstallName = argv[++i];
			}
			else if ( strcmp(arg, "-seg1addr") == 0 ) {
				fBaseAddress = parseAddress(argv[++i]);
			}
			else if ( strcmp(arg, "-e") == 0 ) {
				fEntryName = argv[++i];
			}
			else if ( strcmp(arg, "-filelist") == 0 ) {
				 loadFileList(argv[++i]);
			}
			else if ( strcmp(arg, "-keep_private_externs") == 0 ) {
				 fKeepPrivateExterns = true;
			}
			else if ( strcmp(arg, "-final_output") == 0 ) {
				 ++i;
				// ignore for now
			}
			else if ( (strcmp(arg, "-interposable") == 0) || (strcmp(arg, "-multi_module") == 0)) {
				 fInterposable = true;
			}
			else if ( strcmp(arg, "-single_module") == 0 ) {
				 fInterposable = false;
			}
			else if ( strcmp(arg, "-exported_symbols_list") == 0 ) {
				if ( fExportMode == kDontExportSome )
					throw "can't use -exported_symbols_list and -unexported_symbols_list";
				fExportMode = kExportSome;
				loadExportFile(argv[++i], "-exported_symbols_list", fExportSymbols);
			}
			else if ( strcmp(arg, "-unexported_symbols_list") == 0 ) {
				if ( fExportMode == kExportSome )
					throw "can't use -exported_symbols_list and -unexported_symbols_list";
				fExportMode = kDontExportSome;
				loadExportFile(argv[++i], "-unexported_symbols_list", fDontExportSymbols);
			}
			else if ( strcmp(arg, "-no_arch_warnings") == 0 ) {
				 fIgnoreOtherArchFiles = true;
			}
			else if ( strcmp(arg, "-force_cpusubtype_ALL") == 0 ) {
				 fForceSubtypeAll = true;
			}
			else if ( strcmp(arg, "-weak_library") == 0 ) {
				FileInfo info = makeFileInfo(argv[++i]);
				info.options.fWeakImport = true;
				fInputFiles.push_back(info);
			}
			else if ( strcmp(arg, "-framework") == 0 ) {
				fInputFiles.push_back(findFramework(argv[++i]));
			}
			else if ( strcmp(arg, "-weak_framework") == 0 ) {
				FileInfo info = findFramework(argv[++i]);
				info.options.fWeakImport = true;
				fInputFiles.push_back(info);
			}
			else if ( strcmp(arg, "-search_paths_first") == 0 ) {
				fLibrarySearchMode = kSearchDylibAndArchiveInEachDir;
			}
			else if ( strcmp(arg, "-undefined") == 0 ) {
				 setUndefinedTreatment(argv[++i]);
			}
			else if ( strcmp(arg, "-arch_multiple") == 0 ) {
				 fMessagesPrefixedWithArchitecture = true;
			}
			else if ( strcmp(arg, "-read_only_relocs") == 0 ) {
				 setReadOnlyRelocTreatment(argv[++i]);
			}
			else if ( strcmp(arg, "-sect_diff_relocs") == 0 ) {
				 setPICTreatment(argv[++i]);
			}
			else if ( strcmp(arg, "-weak_reference_mismatches") == 0 ) {
				 setWeakReferenceMismatchTreatment(argv[++i]);
			}
			else if ( strcmp(arg, "-prebind") == 0 ) {
				  // FIX FIX
			}
			else if ( strcmp(arg, "-noprebind") == 0 ) {
				  // FIX FIX
			}
			else if ( strcmp(arg, "-prebind_allow_overlap") == 0 ) {
				  // FIX FIX
			}
			else if ( strcmp(arg, "-prebind_all_twolevel_modules") == 0 ) {
				  // FIX FIX
			}
			else if ( strcmp(arg, "-noprebind_all_twolevel_modules") == 0 ) {
				  // FIX FIX
			}
			else if ( strcmp(arg, "-nofixprebinding") == 0 ) {
				  // FIX FIX
			}
			else if ( strcmp(arg, "-dylib_file") == 0 ) {
				 setDylibInstallNameOverride(argv[++i]);
			}
			else if ( strcmp(arg, "-executable_path") == 0 ) {
				 setExecutablePath(argv[++i]);
			}
			else if ( strcmp(arg, "-segalign") == 0 ) {
				// FIX FIX
				++i; 
			}
			else if ( strcmp(arg, "-segaddr") == 0 ) {
				// FIX FIX
				i += 2; 
			}
			else if ( strcmp(arg, "-segs_read_only_addr") == 0 ) {
				// FIX FIX
				++i; 
			}
			else if ( strcmp(arg, "-segs_read_write_addr") == 0 ) {
				// FIX FIX
				++i; 
			}
			else if ( strcmp(arg, "-seg_addr_table") == 0 ) {
				// FIX FIX
				++i; 
			}
			else if ( strcmp(arg, "-seg_addr_table_filename") == 0 ) {
				// FIX FIX
				++i; 
			}
			else if ( strcmp(arg, "-segprot") == 0 ) {
				// FIX FIX
				i += 3; 
			}
			else if ( strcmp(arg, "-pagezero_size") == 0 ) {
				fZeroPageSize = parseAddress(argv[++i]);
				fZeroPageSize &= (-4096); // page align
			}
			else if ( strcmp(arg, "-stack_addr") == 0 ) {
				fStackAddr = parseAddress(argv[++i]);
			}
			else if ( strcmp(arg, "-stack_size") == 0 ) {
				fStackSize = parseAddress(argv[++i]);
			}
			else if ( strcmp(arg, "-sectalign") == 0 ) {
				addSectionAlignment(argv[i+1], argv[i+2], argv[i+3]);
				i += 3;
			}
			else if ( strcmp(arg, "-sectorder_detail") == 0 ) {
				 // FIX FIX
			}
			else if ( strcmp(arg, "-sectobjectsymbols") == 0 ) {
				// FIX FIX
				i += 2;
			}
			else if ( strcmp(arg, "-bundle_loader") == 0 ) {
				// FIX FIX
				++i;
			}
			else if ( strcmp(arg, "-private_bundle") == 0 ) {
				// FIX FIX
			}
			else if ( strcmp(arg, "-twolevel_namespace_hints") == 0 ) {
				// FIX FIX
			}
			else if ( strcmp(arg, "-multiply_defined") == 0 ) {
				// FIX FIX
				++i;
			}
			else if ( strcmp(arg, "-multiply_defined_unused") == 0 ) {
				// FIX FIX
				++i;
			}
			else if ( strcmp(arg, "-nomultidefs") == 0 ) {
				 // FIX FIX
			}
			else if ( arg[1] == 'y' ) {
				 // FIX FIX
			}
			else if ( strcmp(arg, "-Y") == 0 ) {
				 ++i;
				 // FIX FIX
			}
			else if ( strcmp(arg, "-m") == 0 ) {
				 // FIX FIX
			}
			else if ( strcmp(arg, "-whyload") == 0 ) {
				 // FIX FIX
			}
			else if ( strcmp(arg, "-u") == 0 ) {
				const char* name = argv[++i];
				if ( name == NULL )
					throw "-u missing argument";
				fInitialUndefines.push_back(name);
			}
			else if ( strcmp(arg, "-i") == 0 ) {
				   // FIX FIX
			}
			else if ( strcmp(arg, "-U") == 0 ) {
				 // FIX FIX
				 ++i;  
			}
			else if ( strcmp(arg, "-s") == 0 ) {
				// FIX FIX
			}
			else if ( strcmp(arg, "-x") == 0 ) {
				// FIX FIX
			}
			else if ( strcmp(arg, "-S") == 0 ) {
				// FIX FIX
			}
			else if ( strcmp(arg, "-X") == 0 ) {
				// FIX FIX
			}
			else if ( strcmp(arg, "-Si") == 0 ) {
				// FIX FIX
			}
			else if ( strcmp(arg, "-b") == 0 ) {
				// FIX FIX
			}
			else if ( strcmp(arg, "-Sn") == 0 ) {
				// FIX FIX
			}
			else if ( strcmp(arg, "-dead_strip") == 0 ) {
				 // FIX FIX
				 fprintf(stderr, "ld64: warning -dead_strip not yet supported for 64-bit code\n");
			}
			else if ( strcmp(arg, "-v") == 0 ) {
				extern const char ld64VersionString[];
				fprintf(stderr, "%s", ld64VersionString);
				 // if only -v specified, exit cleanly
				 if ( argc == 2 )
					exit(0);
			}
			else if ( strcmp(arg, "-w") == 0 ) {
				// FIX FIX
			}
			else if ( strcmp(arg, "-arch_errors_fatal") == 0 ) {
				// FIX FIX
			}
			else if ( strcmp(arg, "-M") == 0 ) {
				// FIX FIX
			}
			else if ( strcmp(arg, "-whatsloaded") == 0 ) {
				// FIX FIX
			}
			else if ( strcmp(arg, "-headerpad") == 0 ) {
				const char* size = argv[++i];
				if ( size == NULL )
					throw "-headerpad missing argument";
				 fMinimumHeaderPad = parseAddress(size);
			}
			else if ( strcmp(arg, "-headerpad_max_install_names") == 0 ) {
				// FIX FIX
			}
			else if ( strcmp(arg, "-t") == 0 ) {
				// FIX FIX
			}
			else if ( strcmp(arg, "-A") == 0 ) {
				 // FIX FIX
				 ++i;  
			}
			else if ( strcmp(arg, "-umbrella") == 0 ) {
				const char* name = argv[++i];
				if ( name == NULL )
					throw "-umbrella missing argument";
				fUmbrellaName = name;
			}
			else if ( strcmp(arg, "-allowable_client") == 0 ) {
				 // FIX FIX
				 ++i;  
			}
			else if ( strcmp(arg, "-client_name") == 0 ) {
				 // FIX FIX
				 ++i;  
			}
			else if ( strcmp(arg, "-sub_umbrella") == 0 ) {
				const char* name = argv[++i];
				if ( name == NULL )
					throw "-sub_umbrella missing argument";
				 fSubUmbellas.push_back(name);
			}
			else if ( strcmp(arg, "-sub_library") == 0 ) {
				const char* name = argv[++i];
				if ( name == NULL )
					throw "-sub_library missing argument";
				 fSubLibraries.push_back(name);
			}
			else if ( strcmp(arg, "-init") == 0 ) {
				const char* name = argv[++i];
				if ( name == NULL )
					throw "-init missing argument";
				fInitFunctionName = name;
			}
			else if ( strcmp(arg, "-warn_commons") == 0 ) {
				fWarnCommons = true;
			}
			else if ( strcmp(arg, "-commons") == 0 ) {
				fCommonsMode = parseCommonsTreatment(argv[++i]);
			}
			
			else {
				fprintf(stderr, "unknown option: %s\n", arg);
			}
		}
		else {
			fInputFiles.push_back(makeFileInfo(arg));
		}
	}
}

void Options::buildSearchPaths(int argc, const char* argv[])
{
	bool addStandardLibraryDirectories = true;
	// scan through argv looking for -L and -F options
	for(int i=0; i < argc; ++i) {
		if ( (argv[i][0] == '-') && (argv[i][1] == 'L') )
			fLibrarySearchPaths.push_back(&argv[i][2]);
		else if ( (argv[i][0] == '-') && (argv[i][1] == 'F') )
			fFrameworkSearchPaths.push_back(&argv[i][2]);
		else if ( strcmp(argv[i], "-Z") == 0 )
			addStandardLibraryDirectories = false;
	}
	if ( addStandardLibraryDirectories ) {
		fLibrarySearchPaths.push_back("/usr/lib");
		fLibrarySearchPaths.push_back("/usr/local/lib");
		
		fFrameworkSearchPaths.push_back("/Library/Frameworks/");
		fFrameworkSearchPaths.push_back("/Network/Library/Frameworks/");
		fFrameworkSearchPaths.push_back("/System/Library/Frameworks/");
	}
}

// this is run before the command line is parsed
void Options::parsePreCommandLineEnvironmentSettings()
{
	if ( getenv("RC_TRACE_ARCHIVES") != NULL)
	    fReaderOptions.fTraceArchives = true;
		
	if ( getenv("RC_TRACE_DYLIBS") != NULL) {
	    fReaderOptions.fTraceDylibs = true;
		fReaderOptions.fTraceIndirectDylibs = true;
	}
}

// this is run after the command line is parsed
void Options::parsePostCommandLineEnvironmentSettings()
{

}

void Options::checkIllegalOptionCombinations()
{
	// check -undefined setting
	switch ( fUndefinedTreatment ) {
		case kUndefinedError:
		case kUndefinedDynamicLookup:
			// always legal
			break;
		case kUndefinedWarning:
		case kUndefinedSuppress:
			// requires flat namespace
			if ( fNameSpace == kTwoLevelNameSpace )
				throw "can't use -undefined warning or suppress with -twolevel_namespace";
			break;
	}
	
	// unify -sub_umbrella with dylibs
	for (std::vector<const char*>::iterator it = fSubUmbellas.begin(); it != fSubUmbellas.end(); it++) {
		const char* subUmbrella = *it;
		bool found = false;
		for (std::vector<Options::FileInfo>::iterator fit = fInputFiles.begin(); fit != fInputFiles.end(); fit++) {
			Options::FileInfo& info = *fit;
			const char* lastSlash = strrchr(info.path, '/');
			if ( lastSlash == NULL )
				lastSlash = info.path - 1;
			if ( strcmp(&lastSlash[1], subUmbrella) == 0 ) {
				info.options.fReExport = true;
				found = true;
				break;
			}
		}
		if ( ! found )
			fprintf(stderr, "ld64 warning: -sub_umbrella %s does not match a supplied dylib\n", subUmbrella);
	}
	
	// unify -sub_library with dylibs
	for (std::vector<const char*>::iterator it = fSubLibraries.begin(); it != fSubLibraries.end(); it++) {
		const char* subLibrary = *it;
		bool found = false;
		for (std::vector<Options::FileInfo>::iterator fit = fInputFiles.begin(); fit != fInputFiles.end(); fit++) {
			Options::FileInfo& info = *fit;
			const char* lastSlash = strrchr(info.path, '/');
			if ( lastSlash == NULL )
				lastSlash = info.path - 1;
			const char* dot = strchr(lastSlash, '.');
			if ( dot == NULL )
				dot = &lastSlash[strlen(lastSlash)];
			if ( strncmp(&lastSlash[1], subLibrary, dot-lastSlash-1) == 0 ) {
				info.options.fReExport = true;
				found = true;
				break;
			}
		}
		if ( ! found )
			fprintf(stderr, "ld64 warning: -sub_library %s does not match a supplied dylib\n", subLibrary);
	}
	
	// sync reader options
	if ( fNameSpace != kTwoLevelNameSpace )
		fReaderOptions.fFlatNamespace = true;

	// check -stack_addr
	if ( fStackAddr != 0 ) { 
		switch (fArchitecture) {
			case CPU_TYPE_I386:
			case CPU_TYPE_POWERPC:
				if ( fStackAddr > 0xFFFFFFFF ) 
					throw "-stack_addr must be < 4G for 32-bit processes";
				break;
			case CPU_TYPE_POWERPC64:
				break;
		}
		if ( (fStackAddr & -4096) != fStackAddr )
			throw "-stack_addr must be multiples of 4K";
		if ( fStackSize  == 0 )
			throw "-stack_addr must be used with -stack_size";
	}
	
	// check -stack_size
	if ( fStackSize != 0 ) { 
		switch (fArchitecture) {
			case CPU_TYPE_I386:
			case CPU_TYPE_POWERPC:
				if ( fStackSize > 0xFFFFFFFF ) 
					throw "-stack_size must be < 4G for 32-bit processes";
				if ( fStackAddr == 0 ) {
					fprintf(stderr, "ld64 warning: -stack_addr not specified, using the default 0xC0000000\n");
					fStackAddr = 0xC0000000;
				}
				break;
			case CPU_TYPE_POWERPC64:
				if ( fStackAddr == 0 ) {
					fprintf(stderr, "ld64 warning: -stack_addr not specified, using the default 0x0008000000000000\n");
					fStackAddr = 0x0008000000000000LL;
				}
				break;
		}
		if ( (fStackSize & -4096) != fStackSize )
			throw "-stack_size must be multiples of 4K";
		switch ( fOutputKind ) {
			case Options::kDynamicExecutable:
			case Options::kStaticExecutable:
				// custom stack size only legeal when building main executable
				break;
			case Options::kDynamicLibrary:
			case Options::kDynamicBundle:
			case Options::kObjectFile:
			case Options::kDyld:
				throw "-stack_size option can only be used when linking a main executable";
		}
	}
	
	// check -init is only used when building a dylib
	if ( (fInitFunctionName != NULL) && (fOutputKind != Options::kDynamicLibrary) )
		throw "-init can only be used with -dynamiclib";
}


