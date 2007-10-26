/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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
#include <CoreFoundation/CoreFoundation.h>
#include <getopt.h>
#include <IOKit/kext/KXKextManager.h>
#include <IOKit/kext/fat_util.h>
#include <IOKit/kext/macho_util.h>

#include "utility.h"

/*******************************************************************************
* Kext lib private functions that should be in a private header but aren't.
*******************************************************************************/
extern  fat_iterator _KXKextCopyFatIterator(KXKextRef aKext);
typedef SInt64       VERS_version;
extern  VERS_version _KXKextGetCompatibleVersion(KXKextRef aKext);

/*******************************************************************************
* Local function prototypes.
*******************************************************************************/
Boolean addRepositoryName(
    const char * optarg,
    CFMutableArrayRef repositoryDirectories);

static int allocateArray(CFMutableArrayRef * array);
static int allocateDictionary(CFMutableDictionaryRef * dict);
char * _cStringForCFString(CFStringRef cfString);
char * _posixPathForCFString(CFStringRef cfString);

static void usage(int level);

/*******************************************************************************
* Local function prototypes and misc grotty  bits.
*******************************************************************************/
const char * progname;
int g_verbose_level = kKXKextManagerLogLevelDefault;  // -v, -q options set this

enum {
    kKextlibsExitOK = 0,
    kKextlibsExitUndefineds = 1,
    kKextlibsExitMultiples = 2,
    kKextlibsExitUnspecified  // any nonzero
};

#define kErrorStringMemoryAllocation "memory allocation failure\n"

/*******************************************************************************
* Command-line options (as opposed to query predicate keywords).
* This data is used by getopt_long_only().
*******************************************************************************/
/* Would like a way to automatically combine these into the optstring.
 */
#define kOptNameHelp                    "help"
#define kOptNameSystemExtensions        "system-extensions"
#define kOptNameRepository              "repository"
#define kOptNameXML                     "xml"
#define kOptNameCompatible              "compatible-versions"
#define kOptNameAllSymbols              "all-symbols"
#define kOptNameNonKPI                  "non-kpi"

#define kOPT_CHARS  "cehr:"

enum {
    kOptCompatible = 'c',
    kOptSystemExtensions = 'e',
    kOptHelp = 'h',
    kOptRepository = 'r',
};

/* Options with no single-letter variant.  */
// Do not use -1, that's getopt() end-of-args return value
// and can cause confusion
enum {
    kLongOptXML = -2,
    kLongOptAllSymbols = -3,
    kLongOptNonKPI = -4,
};


int longopt = 0;

struct option opt_info[] = {
    // real options
    { kOptNameHelp,             no_argument,        NULL, kOptHelp },
    { kOptNameSystemExtensions, no_argument,        NULL, kOptSystemExtensions },
    { kOptNameRepository,       required_argument,  NULL, kOptRepository },
    { kOptNameCompatible,       no_argument,        NULL, kOptCompatible },
    { kOptNameXML,              no_argument,        &longopt, kLongOptXML },
    { kOptNameAllSymbols,       no_argument,        &longopt, kLongOptAllSymbols },
    { kOptNameNonKPI,           no_argument,        &longopt, kLongOptNonKPI },

    { NULL, 0, NULL, 0 }  // sentinel to terminate list
};

/*******************************************************************************
* Suck from a kext's executable all of its undefined symbol names and build
* an array of empty arrays keyed  by symbol. The arrays will be filled with
* lib kexts that export that symbol. Also note the cpu arch of the kext to
* match on that for the libs.
*******************************************************************************/
Boolean readSymbolReferences(
    KXKextRef theKext,
    cpu_type_t * cpu_type,
    cpu_subtype_t * cpu_subtype,
    CFMutableDictionaryRef symbols)
{
    Boolean result = false;
    fat_iterator fiter = NULL;  // must close
    void * farch = NULL;
    void * farch_end = NULL;
    macho_seek_result symtab_result = macho_seek_result_not_found;
    uint8_t swap = 0;
    struct symtab_command * symtab = NULL;
    struct nlist * syms_address;
    const void * string_list;
    unsigned int sym_offset;
    unsigned int str_offset;
    unsigned int num_syms;
    unsigned int syms_bytes;
    unsigned int sym_index;
    CFMutableArrayRef libsArray = NULL; // must release
    char kext_name[PATH_MAX];

    if (!CFStringGetFileSystemRepresentation(
	    KXKextGetBundlePathInRepository(theKext), kext_name, PATH_MAX)) {
        fprintf(stderr, "%s", kErrorStringMemoryAllocation);
        goto finish;
    }

    fiter = _KXKextCopyFatIterator(theKext);
    if (!fiter) {
        fprintf(stderr, "Can't open executable for %s\n", kext_name);
        goto finish;
    }

    farch = fat_iterator_next_arch(fiter, &farch_end);
    if (!farch) {
        fprintf(stderr, "No code in %s\n", kext_name);
        goto finish;
    }

    symtab_result = macho_find_symtab(farch, farch_end, &symtab);
    if (symtab_result != macho_seek_result_found) {
        fprintf(stderr, "Error in mach-o file for %s\n", kext_name);
        goto finish;
    }

    if (MAGIC32(farch) == MH_CIGAM) {
        swap = 1;
    }

    *cpu_type = CondSwapInt32(swap, ((struct mach_header *)farch)->cputype);
    *cpu_subtype = CondSwapInt32(swap, ((struct mach_header *)farch)->cpusubtype);

    sym_offset = CondSwapInt32(swap, symtab->symoff);
    str_offset = CondSwapInt32(swap, symtab->stroff);
    num_syms   = CondSwapInt32(swap, symtab->nsyms);

    syms_address = (struct nlist *)(farch + sym_offset);
    string_list = farch + str_offset;
    syms_bytes = num_syms * sizeof(struct nlist);

    if ((char *)syms_address + syms_bytes > (char *)farch_end) {
        fprintf(stderr, "Error in mach-o file for %s\n", kext_name);
        goto finish;
    }

    for (sym_index = 0; sym_index < num_syms; sym_index++) {
        struct nlist * seekptr;
        uint32_t string_index;

        seekptr = &syms_address[sym_index];

        string_index = CondSwapInt32(swap, seekptr->n_un.n_strx);

        // no need to swap n_type (one-byte value)
        if (string_index == 0 || seekptr->n_type & N_STAB) {
            continue;
        }

        if ((seekptr->n_type & N_TYPE) == N_UNDF) {
            char * symbol_name;
            CFStringRef cfSymbolName; // must release

            symbol_name = (char *)(string_list + string_index);
            if (!allocateArray(&libsArray)) {
                goto finish;
            }
            cfSymbolName = CFStringCreateWithCString(kCFAllocatorDefault,
                symbol_name, kCFStringEncodingASCII);
            if (!cfSymbolName) {
                fprintf(stderr, "%s", kErrorStringMemoryAllocation);
                goto finish;
            }
            CFDictionarySetValue(symbols, cfSymbolName, libsArray);
            CFRelease(cfSymbolName);
        }
    }

    result = true;
finish:
    if (fiter)     fat_iterator_close(fiter);
    if (libsArray) CFRelease(libsArray);
    return result;
}

/*******************************************************************************
* Given a library kext, check all of its exported symbols against the running
* lists of undef (not yet found) symbols, symbols found once, and symbols
* found already in other library kexts. Adjust tallies appropriately.
*******************************************************************************/
Boolean findSymbols(
    KXKextRef theKext,
    cpu_type_t cpu_type,
    cpu_subtype_t cpu_subtype,
    CFMutableDictionaryRef undefSymbols,
    CFMutableDictionaryRef onedefSymbols,
    CFMutableDictionaryRef multdefSymbols,
    CFMutableArrayRef multdefLibs)
{
    Boolean result = false;
    fat_iterator fiter = NULL;
    void * farch = NULL;
    void * farch_end = NULL;
    macho_seek_result symtab_result = macho_seek_result_not_found;
    uint8_t swap = 0;
    struct symtab_command * symtab = NULL;
    struct nlist * syms_address;
    const void * string_list;
    unsigned int sym_offset;
    unsigned int str_offset;
    unsigned int num_syms;
    unsigned int syms_bytes;
    unsigned int sym_index;
    char * symbol_name;
    Boolean eligible;
    CFMutableArrayRef libsArray = NULL; // do not release
    Boolean notedMultdef = false;
    char kext_name[PATH_MAX];

    if (!CFStringGetFileSystemRepresentation(
	    KXKextGetBundlePathInRepository(theKext), kext_name, PATH_MAX)) {
        fprintf(stderr, "%s", kErrorStringMemoryAllocation);
        goto finish;
    }

    fiter = _KXKextCopyFatIterator(theKext);
    if (!fiter) {
        fprintf(stderr, "Can't open executable for %s\n", kext_name);
        goto finish;
    }

    farch = fat_iterator_find_arch(fiter, cpu_type, cpu_subtype, &farch_end);
    if (!farch) {
        // This is not necessarily an error here.
        goto finish;
    }

    symtab_result = macho_find_symtab(farch, farch_end, &symtab);
    if (symtab_result != macho_seek_result_found) {
        fprintf(stderr, "Error in mach-o file for %s\n", kext_name);
        goto finish;
    }

    if (MAGIC32(farch) == MH_CIGAM) {
        swap = 1;
    }

    sym_offset = CondSwapInt32(swap, symtab->symoff);
    str_offset = CondSwapInt32(swap, symtab->stroff);
    num_syms   = CondSwapInt32(swap, symtab->nsyms);

    syms_address = (struct nlist *)(farch + sym_offset);
    string_list = farch + str_offset;
    syms_bytes = num_syms * sizeof(struct nlist);

    if ((char *)syms_address + syms_bytes > (char *)farch_end) {
        fprintf(stderr, "Error in mach-o file for %s\n", kext_name);
        goto finish;
    }

    for (sym_index = 0; sym_index < num_syms; sym_index++) {
        struct nlist * seekptr;
        uint32_t string_index;

        seekptr = &syms_address[sym_index];

        string_index = CondSwapInt32(swap, seekptr->n_un.n_strx);

        if (string_index == 0 || (seekptr->n_type & N_STAB)) {
            continue;
        }

       /* Kernel component kexts are weird; they are just lists of indirect
        * and undefined symtab entries for the real data in the kernel.
        */

        // no need to swap n_type (one-byte value)
        switch (seekptr->n_type & N_TYPE) {
          case N_UNDF:
            /* Fall through, only support indirects for KPI for now. */
          case N_INDR:
            eligible = KXKextGetIsKernelResource(theKext) ? true : false;
            break;
          case N_SECT:
            eligible = KXKextGetIsKernelResource(theKext) ? false : true;
            break;
          default:
            eligible = false;
            break;
        }

        if (eligible) {

            CFStringRef cfSymbolName; // must release

            symbol_name = (char *)(string_list + string_index);

            cfSymbolName = CFStringCreateWithCString(kCFAllocatorDefault,
                symbol_name, kCFStringEncodingASCII);
            if (!cfSymbolName) {
                goto finish;
            }

           /* Bubble library tallies from undef->onedef->multdef as symbols
            * are found. Also note any lib that has a duplicate match.
            */
            libsArray = (CFMutableArrayRef)CFDictionaryGetValue(
                multdefSymbols, cfSymbolName);
            if (libsArray) {
                result = true;
                CFArrayAppendValue(libsArray, theKext);
            } else {
                libsArray = (CFMutableArrayRef)CFDictionaryGetValue(
                    onedefSymbols, cfSymbolName);
                if (libsArray) {
                    result = true;
                    CFArrayAppendValue(libsArray, theKext);
                    CFDictionarySetValue(multdefSymbols, cfSymbolName, libsArray);
                    CFDictionaryRemoveValue(onedefSymbols, cfSymbolName);
                    if (!notedMultdef) {
                        CFArrayAppendValue(multdefLibs, theKext);
                        notedMultdef = true;
                    }
                } else {
                    libsArray = (CFMutableArrayRef)CFDictionaryGetValue(
                        undefSymbols, cfSymbolName);
                    if (libsArray) {
                        result = true;
                        CFArrayAppendValue(libsArray, theKext);
                        CFDictionarySetValue(onedefSymbols, cfSymbolName, libsArray);
                        CFDictionaryRemoveValue(undefSymbols, cfSymbolName);
                    }
                }
            }
            CFRelease(cfSymbolName);
        }
    }
finish:
    if (fiter) fat_iterator_close(fiter);
    return result;
}

// XX are symbol names guaranteed to be ASCII?
/*******************************************************************************
*
*******************************************************************************/
void printUndefSymbol(const void * key, const void * value, void * context)
{
    char * cSymbol = NULL; // must free

    cSymbol = _cStringForCFString((CFStringRef)key);
    if (cSymbol) {
        fprintf(stderr, "\t%s\n", cSymbol);
        free(cSymbol);
    }
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void printMultdefSymbol(const void * key, const void * value, void * context)
{
    char * cSymbol = NULL; // must free
    CFArrayRef libs = (CFArrayRef)value;
    Boolean flag_compatible = *(Boolean *)context;
    CFIndex count, i;

    cSymbol = _cStringForCFString((CFStringRef)key);
    if (cSymbol) {
        fprintf(stderr, "\t%s: in ", cSymbol);
        free(cSymbol);
    }

    count = CFArrayGetCount(libs);
    for (i = 0; i < count; i++) {
        KXKextRef libKext = (KXKextRef)CFArrayGetValueAtIndex(libs, i);
        char * libName = NULL;  // must free
        char * libVers = NULL;  // must free

        libName =_posixPathForCFString(KXKextGetBundlePathInRepository(libKext));

        libVers = _cStringForCFString(CFDictionaryGetValue(
            KXKextGetInfoDictionary(libKext), flag_compatible ?
            CFSTR("OSBundleCompatibleVersion") : kCFBundleVersionKey));

        if (libName && libVers) {
            fprintf(stderr, "%s%s (%s)", i ? ", " : "", libName, libVers);
        }
        if (libName) free(libName);
        if (libVers) free(libVers);
    }
    fprintf(stderr, "\n");

    if (cSymbol) free(cSymbol);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean kextIsLibrary(KXKextRef aKext, Boolean non_kpi)
{
    if (_KXKextGetCompatibleVersion(aKext) <= 0) {
        return false;
    }
    if (!KXKextIsLoadable(aKext, false /* safe boot */)) {
        return false;
    }
    if (KXKextGetIsKernelResource(aKext) && !KXKextGetDeclaresExecutable(aKext)) {
        return false;
    }
    if (non_kpi) {
        if (CFStringHasPrefix(KXKextGetBundleIdentifier(aKext), CFSTR("com.apple.kpi"))) {
            return false;
        }
    } else {
        if (CFEqual(KXKextGetBundleIdentifier(aKext), CFSTR("com.apple.kernel.6.0"))) {
            return false;
        }
    }
    if ((_KXKextGetCompatibleVersion(aKext) > 0) &&
        !KXKextGetIsKernelResource(aKext) &&
        !KXKextGetDeclaresExecutable(aKext)) {

        return false;
    }

    return true;
}

/*******************************************************************************
*
*******************************************************************************/
int main(int argc, char * const * argv)
{
    int exit_code = kKextlibsExitUnspecified;
    int opt_char = 0;

    CFIndex count, i;

    CFMutableArrayRef repositoryDirectories = NULL;  // must release

    const char * kext_name;        // from argv
    CFStringRef kextName = NULL;   // must release
    CFURLRef    kextURL = NULL;    // must release
    KXKextRef   theKext = NULL;    // don't release

    cpu_type_t kext_cpu_type;
    cpu_subtype_t kext_cpu_subtype;

    KXKextManagerRef theKextManager = NULL;          // must release
    KXKextManagerError kmErr;

    CFMutableArrayRef      libKexts = NULL;        // must release
    CFMutableDictionaryRef undefSymbols = NULL;    // must release
    CFMutableDictionaryRef onedefSymbols = NULL;   // must release
    CFMutableDictionaryRef multdefSymbols = NULL;  // must release
    CFMutableArrayRef      multdefLibs = NULL;     // must release

    CFIndex undefCount;
    CFIndex onedefCount;
    CFIndex multdefCount;

    Boolean flag_xml = false;
    Boolean flag_compatible = false;
    Boolean flag_all_syms = false;
    Boolean flag_non_kpi = false;

   /*****
    * Find out what the program was invoked as.
    */
    progname = rindex(argv[0], '/');
    if (progname) {
        progname++;   // go past the '/'
    } else {
        progname = (char *)argv[0];
    }

   /*****
    * Allocate collection objects needed for command line argument processing.
    */
    if (!allocateArray(&repositoryDirectories)) {
        goto finish;
    }
    if (!allocateDictionary(&undefSymbols)) {
        goto finish;
    }
    if (!allocateDictionary(&onedefSymbols)) {
        goto finish;
    }
    if (!allocateDictionary(&multdefSymbols)) {
        goto finish;
    }
    if (!allocateArray(&multdefLibs)) {
        goto finish;
    }

   /*****
    * Process command-line arguments.
    */
    while ((opt_char = getopt_long_only(argc, argv, kOPT_CHARS,
        opt_info, NULL)) != -1) {

        switch (opt_char) {

          case kOptHelp:
            usage(2);
            goto finish;
            break;

          case kOptRepository:
            if (!check_dir(optarg, 0 /* writeable */, 1 /* print error */)) {
                goto finish;
            }

            if (!addRepositoryName(optarg, repositoryDirectories)) {
                goto finish;
            };
            break;

          case kOptCompatible:
            flag_compatible = true;
            break;

          case kOptSystemExtensions:
            CFArrayAppendValue(repositoryDirectories, kKXSystemExtensionsFolder);
            break;

          case 0:
            switch (longopt) {

              case kLongOptXML:
                flag_xml = true;
                break;

              case kLongOptAllSymbols:
                flag_all_syms = true;
                break;

              case kLongOptNonKPI:
                flag_non_kpi = true;
                break;

            }
            break;
        }
    }

    argc -= optind;
    argv += optind;

    if (!argv[0]) {
        fprintf(stderr, "no kext specified\n");
        usage(1);
        goto finish;
    }

    kext_name = argv[0];
    kextName = CFStringCreateWithFileSystemRepresentation(kCFAllocatorDefault,
        kext_name);

   /*****
    * Set up the kext manager.
    */
    theKextManager = KXKextManagerCreate(kCFAllocatorDefault);
    if (!theKextManager) {
        qerror("can't allocate kernel extension manager\n");
        goto finish;
    }

    kmErr = KXKextManagerInit(theKextManager, true /* load_in_task */,
        false /* safe_boot_mode */);
    if (kmErr != kKXKextManagerErrorNone) {
        qerror("can't initialize kernel extension manager (%s)\n",
            KXKextManagerErrorStaticCStringForError(kmErr));
        goto finish;
    }

    KXKextManagerSetPerformsFullTests(theKextManager, true);
    KXKextManagerSetPerformsStrictAuthentication(theKextManager, true);
    KXKextManagerSetLogLevel(theKextManager, kKXKextManagerLogLevelSilent);
    KXKextManagerSetLogFunction(theKextManager, &verbose_log);
    KXKextManagerSetErrorLogFunction(theKextManager, &error_log);
//    KXKextManagerSetUserVetoFunction(theKextManager, &user_approve);
//    KXKextManagerSetUserApproveFunction(theKextManager, &user_approve);
//    KXKextManagerSetUserInputFunction(theKextManager, &user_input);

   /*****
    * Disable clearing of relationships until we're done putting everything
    * together.
    */
    KXKextManagerDisableClearRelationships(theKextManager);

   /*****
    * Add the extensions folders to the manager.
    */
    count = CFArrayGetCount(repositoryDirectories);
    if (count == 0) {
        CFArrayAppendValue(repositoryDirectories, kKXSystemExtensionsFolder);
    }

    count = CFArrayGetCount(repositoryDirectories);
    for (i = 0; i < count; i++) {
        CFStringRef directory = (CFStringRef)CFArrayGetValueAtIndex(
            repositoryDirectories, i);
        CFURLRef directoryURL =
            CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                directory, kCFURLPOSIXPathStyle, true);
        if (!directoryURL) {
            qerror("memory allocation failure\n");
            goto finish;
        }

        kmErr = KXKextManagerAddRepositoryDirectory(theKextManager,
            directoryURL, true /* scanForKexts */,
            false /* use_repository_caches */, NULL);
        if (kmErr != kKXKextManagerErrorNone) {
            qerror("can't add repository (%s).\n",
                KXKextManagerErrorStaticCStringForError(kmErr));
            goto finish;
        }
        CFRelease(directoryURL);
        directoryURL = NULL;
    }

    kextURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
        kextName, kCFURLPOSIXPathStyle, true);
    if (!kextURL) {
        qerror("memory allocation failure\n");
        goto finish;
    }

    kmErr = KXKextManagerAddKextWithURL(theKextManager, kextURL, true, &theKext);
    if (kmErr != kKXKextManagerErrorNone) {
        qerror("can't add kernel extension %s (%s)",
            kext_name, KXKextManagerErrorStaticCStringForError(kmErr));
        qerror(" (run %s on this kext with -t for diagnostic output)\n",
            progname);
        goto finish;
    }

    KXKextManagerEnableClearRelationships(theKextManager);

    KXKextManagerAuthenticateKexts(theKextManager);
    KXKextManagerCalculateVersionRelationships(theKextManager);
    KXKextManagerResolveAllKextDependencies(theKextManager);

    if (!readSymbolReferences(theKext,
        &kext_cpu_type, &kext_cpu_subtype, undefSymbols)) {
        goto finish;
    }

    libKexts = KXKextManagerCopyAllKexts(theKextManager);
    if (!libKexts) {
        goto finish;
    }

   /* Drop from consideration all non-library (and non-kpi) kexts. Sheesh,
    * this is too complicated. Need to add a lib routine to make this decision.
    */
    count = CFArrayGetCount(libKexts);
    for (i = count; i; i--) {
        CFIndex index = i - 1;
        KXKextRef thisKext = (KXKextRef)CFArrayGetValueAtIndex(libKexts, index);
        if (!kextIsLibrary(thisKext, flag_non_kpi)) {

            CFArrayRemoveValueAtIndex(libKexts, index);
        }
    }

    count = CFArrayGetCount(libKexts);
    for (i = count; i; i--) {
        CFIndex index = i - 1;
        KXKextRef libKext = (KXKextRef)CFArrayGetValueAtIndex(libKexts, index);
        if (!findSymbols(libKext, kext_cpu_type, kext_cpu_subtype,
            undefSymbols, onedefSymbols, multdefSymbols, multdefLibs)) {

            CFArrayRemoveValueAtIndex(libKexts, index);
        }
    }

    undefCount = CFDictionaryGetCount(undefSymbols);
    onedefCount = CFDictionaryGetCount(onedefSymbols);
    multdefCount = CFDictionaryGetCount(multdefSymbols);

    count = CFArrayGetCount(libKexts);
    if (count && flag_xml) {
        fprintf(stdout, "\t<key>OSBundleLibraries</key>\n");
        fprintf(stdout, "\t<dict>\n");
    }
    for (i = count; i; i--) {
        CFIndex index = i - 1;
        KXKextRef libKext = (KXKextRef)CFArrayGetValueAtIndex(libKexts,
            index);
        CFDictionaryRef infoDict;
        CFStringRef versString;
        char * bundle_id = NULL; // must free
        char * version = NULL; // must free

	bundle_id = _cStringForCFString(KXKextGetBundleIdentifier(libKext));
        
        infoDict = KXKextGetInfoDictionary(libKext);
        if (infoDict) {
            infoDict = KXKextGetInfoDictionary(libKext);
            versString = CFDictionaryGetValue(infoDict, flag_compatible ?
                CFSTR("OSBundleCompatibleVersion") : kCFBundleVersionKey);
            if (versString) {
                version = _cStringForCFString(versString);
            }
        }
	if (bundle_id && version) {
            if (flag_xml) {
                fprintf(stdout, "\t\t<key>%s</key>\n", bundle_id);
                fprintf(stdout, "\t\t<string>%s</string>\n", version);
            } else {
                fprintf(stdout, "%s = %s\n", bundle_id, version);
            }
        }
        if (bundle_id) free(bundle_id);
        if (version)   free(version);
    }
    if (count && flag_xml) {
        fprintf(stdout, "\t</dict>\n");
    }

    if (undefCount) {
        fprintf(stderr, "%ld symbols not found in any library kext%s\n",
            undefCount, flag_all_syms ? ":" : "");
        if (flag_all_syms) {
            CFDictionaryApplyFunction(undefSymbols, printUndefSymbol, NULL);
        }
        exit_code = kKextlibsExitUndefineds;
    }
    if (multdefCount) {
        if (flag_all_syms) {
            fprintf(stderr, "%ld symbols found in more than one library kext:\n",
                multdefCount);
            CFDictionaryApplyFunction(multdefSymbols, printMultdefSymbol,
                &flag_compatible);
        } else {
            count = CFArrayGetCount(multdefLibs);
            fprintf(stderr, "multiple symbols found among %ld libraries:\n",
                count);
            for (i = 0; i < count; i++) {
                KXKextRef lib = (KXKextRef)CFArrayGetValueAtIndex(multdefLibs, i);
                char * name = NULL; // must free
                name = _cStringForCFString(KXKextGetBundleIdentifier(lib));
		if (name) {
                    fprintf(stderr, "\t%s\n", name);
                    free(name);
                }
            }
        }
        exit_code = kKextlibsExitMultiples;
    }
    if (undefCount || multdefCount) {
        goto finish;
    }

    exit_code = kKextlibsExitOK;
finish:
    if (repositoryDirectories)  CFRelease(repositoryDirectories);
    if (kextName)               CFRelease(kextName);
    if (kextURL)                CFRelease(kextURL);
    if (theKextManager)         CFRelease(theKextManager);

    if (libKexts)       CFRelease(libKexts);
    if (undefSymbols)   CFRelease(undefSymbols);
    if (onedefSymbols)  CFRelease(onedefSymbols);
    if (multdefSymbols) CFRelease(multdefSymbols);
    if (multdefLibs)    CFRelease(multdefLibs);
    return exit_code;
}

/*******************************************************************************
*******************************************************************************/
Boolean addRepositoryName(
    const char * optarg,
    CFMutableArrayRef repositoryDirectories)
{
    Boolean result = false;
    CFStringRef name = NULL;   // must release

    name = CFStringCreateWithCString(kCFAllocatorDefault,
            optarg, kCFStringEncodingUTF8);
    if (!name) {
        qerror(kErrorStringMemoryAllocation);
        goto finish;
    }
    CFArrayAppendValue(repositoryDirectories, name);

    result = true;
finish:
    if (name) CFRelease(name);
    return result;
}

/*******************************************************************************
* allocateArray()
*******************************************************************************/
static int allocateArray(CFMutableArrayRef * array) {

    int result = 1;  // assume success

    if (!array) {
        qerror("internal error\n");
        result = 0;
        goto finish;
    }

    *array = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!*array) {
        result = 0;
        qerror(kErrorStringMemoryAllocation);
        goto finish;
    }

finish:
    return result;
}

/*******************************************************************************
* allocateDictionary()
*******************************************************************************/
static int allocateDictionary(CFMutableDictionaryRef * dict) {

    int result = 1;  // assume success

    if (!dict) {
        qerror("internal error\n");
        result = 0;
        goto finish;
    }

    *dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!*dict) {
        result = 0;
        qerror(kErrorStringMemoryAllocation);
        goto finish;
    }

finish:
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
char *
_cStringForCFString(CFStringRef cfString)
{
    uint32_t stringLength = 0;
    char * string = NULL;

    stringLength = CFStringGetMaximumSizeForEncoding(
        CFStringGetLength(cfString), kCFStringEncodingUTF8);
    string = (char *)malloc(stringLength);
    if (!string) {
        goto finish;
    }
    if (!CFStringGetCString(cfString, string, stringLength,
        kCFStringEncodingUTF8)) {

        free(string);
        string = NULL;
        goto finish;
    }
finish:
    return string;
}

/*******************************************************************************
*
*******************************************************************************/
char *
_posixPathForCFString(CFStringRef cfString)
{
    uint32_t stringLength = 0;
    char * string = NULL;

    stringLength = CFStringGetMaximumSizeForEncoding(
        CFStringGetLength(cfString), kCFStringEncodingUTF8);
    string = (char *)malloc(stringLength);
    if (!string) {
        goto finish;
    }
    if (!CFStringGetFileSystemRepresentation(cfString, string, stringLength)) {

        free(string);
        string = NULL;
        goto finish;
    }
finish:
    return string;
}

/*******************************************************************************
* usage()
*******************************************************************************/
static void usage(int level)
{
    FILE * stream = stderr;

    fprintf(stream,
      "usage: %s [options] kext"
      "\n",
      progname);

    if (level < 1) {
        return;
    }

    if (level == 1) {
        fprintf(stream, "use %s -%s for a list of options\n",
            progname, kOptNameHelp);
        return;
    }

    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -%s (-%c): look in the system exensions folder\n"
        "    (assumed if no other folders specified with %s)\n",
        kOptNameSystemExtensions, kOptSystemExtensions,
        kOptNameRepository);
    fprintf(stderr, "  -%s directory (-%c): look directory for library kexts\n",
        kOptNameRepository, kOptRepository);
    fprintf(stderr, "  -%s: print XML fragment suitable for pasting\n",
        kOptNameXML);
    fprintf(stderr, "  -%s: list all symbols not found or found in multiple\n"
        "    library kexts\n",
        kOptNameAllSymbols);
    fprintf(stderr, "  -%s (-%c): use library kext compatble versions\n"
        "    rather than current versions\n",
        kOptNameCompatible, kOptCompatible);
    return;
}
