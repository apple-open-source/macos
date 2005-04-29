#include <CoreFoundation/CoreFoundation.h>
#include <Kernel/libsa/mkext.h>
#include <architecture/byte_order.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <libc.h>
#include <mach-o/arch.h>
#include <mach-o/fat.h>

#include <mach/mach.h>
#include <mach/mach_types.h>
#include <mach/kmod.h>

static const char * progname = "mkextunpack";
static Boolean gVerbose = false;

__private_extern__ u_int32_t local_adler32(u_int8_t *buffer, int32_t length);

CFDictionaryRef extractEntriesFromMkext(char * mkextFileData, char * arch);
Boolean uncompressMkextEntry(
    char * mkext_base_address,
    mkext_file * entry_address,
    CFDataRef * uncompressedEntry);
Boolean writeEntriesToDirectory(CFDictionaryRef entries,
    char * outputDirectory);
CFStringRef createKextNameFromPlist(
    CFDictionaryRef entries, CFDictionaryRef kextPlist);
int getBundleIDAndVersion(CFDictionaryRef kextPlist, unsigned index,
    char ** bundle_id_out, char ** bundle_version_out);

/*******************************************************************************
*
*******************************************************************************/
void usage(int num) {
    fprintf(stderr, "usage: %s [-v] [-a arch] [-d output_dir] mkextfile\n", progname);
    fprintf(stderr, "    -d output_dir: where to put kexts (must exist)\n");
    fprintf(stderr, "    -a arch:  pick architecture from fat mkext file\n");
    fprintf(stderr, "    -v:  verbose output; list kexts in mkextfile\n");
    return;
}

/*******************************************************************************
*
*******************************************************************************/
int main (int argc, const char * argv[]) {
    int exit_code = 0;

    char * outputDirectory = NULL;
    const char * mkextFile = NULL;
    int mkextFileFD;
    struct stat stat_buf;
    char * mkextFileContents = NULL;
    char optchar;
    CFDictionaryRef entries = NULL;
    char *arch = NULL;

    progname = argv[0];

    while ((optchar = getopt(argc, (char * const *)argv, "d:va:")) != -1) {
        switch (optchar) {
          case 'd':
            if (!optarg) {
                fprintf(stderr, "no argument for -d\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            outputDirectory = optarg;
            break;
          case 'v':
            gVerbose = true;
            break;
          case 'a':
            if (arch != NULL) {
                fprintf(stderr, "-a may be specified only once\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            if (!optarg) {
                fprintf(stderr, "no argument for -a\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            arch = optarg;
            break;
        }
    }

   /* Update argc, argv based on option processing.
    */
    argc -= optind;
    argv += optind;

    if (argc == 0 || argc > 1) {
        usage(0);
        exit_code = 1;
        goto finish;
    }

    mkextFile = argv[0];

    if (!outputDirectory && !gVerbose) {
        fprintf(stderr, "no work to do; please specify -d or -v\n");
        usage(0);
        exit_code = 1;
        goto finish;
    }

    if (!mkextFile) {
        fprintf(stderr, "no mkext file given\n");
        usage(0);
        exit_code = 1;
        goto finish;
    }

    if (outputDirectory) {
        if (stat(outputDirectory, &stat_buf) < 0) {
            fprintf(stderr, "can't stat directory %s\n",
                outputDirectory);
            exit_code = 1;
            goto finish;
        }

        if ( !(stat_buf.st_mode & S_IFDIR) ) {
            fprintf(stderr, "%s is not a directory\n",
                outputDirectory);
            exit_code = 1;
            goto finish;
        }

        if (access(outputDirectory, W_OK) == -1) {
            fprintf(stderr, "can't write into directory %s "
                "(permission denied)\n", outputDirectory);
            exit_code = 1;
            goto finish;
        }
    }

    if (stat(mkextFile, &stat_buf) < 0) {
        fprintf(stderr, "can't stat file %s\n", mkextFile);
        exit_code = 1;
        goto finish;
    }

    if (access(mkextFile, R_OK) == -1) {
        fprintf(stderr, "can't read file %s (permission denied)\n",
            mkextFile);
        exit_code = 1;
        goto finish;
    }

    mkextFileFD = open(mkextFile, O_RDONLY, 0);
    if (mkextFileFD < 0) {
        fprintf(stderr, "can't open file %s\n", mkextFile);
        exit_code = 1;
        goto finish;
    }

    if ( !(stat_buf.st_mode & S_IFREG) ) {
        fprintf(stderr, "%s is not a regular file\n",
            mkextFile);
        exit_code = 1;
        goto finish;
    }

    if (!stat_buf.st_size) {
        fprintf(stderr, "%s is an empty file\n",
            mkextFile);
        exit_code = 1;
        goto finish;
    }

    mkextFileContents = mmap(0, stat_buf.st_size, PROT_READ, 
        MAP_FILE|MAP_PRIVATE, mkextFileFD, 0);
    if (mkextFileContents == (char *)-1) {
        fprintf(stderr, "can't map file %s\n", mkextFile);
        exit_code = 1;
        goto finish;
    }

    entries = extractEntriesFromMkext(mkextFileContents, arch);
    if (!entries) {
        fprintf(stderr, "can't unpack file %s\n", mkextFile);
        exit_code = 1;
        goto finish;
    }

    if (outputDirectory) {
        writeEntriesToDirectory(entries, outputDirectory);
    }

finish:
    exit(exit_code);
    return exit_code;
}
/*******************************************************************************
*
*******************************************************************************/

static Boolean CaseInsensitiveEqual(CFTypeRef left, CFTypeRef right)
{
    return (kCFCompareEqualTo == CFStringCompare(left, right, kCFCompareCaseInsensitive));
}

static CFHashCode CaseInsensitiveHash(const void *key)
{
    return (CFStringGetLength(key));
}

/*******************************************************************************
*
*******************************************************************************/
CFDictionaryRef extractEntriesFromMkext(char * mkextFileData, char * arch)
{
    CFMutableDictionaryRef entries = NULL;  // returned
    Boolean error = false;

    u_int8_t      * crc_address = 0;
    u_int32_t       checksum;
    struct fat_header *fat_data = 0; // don't free
    mkext_header  * mkext_data = 0;   // don't free
    const NXArchInfo *arch_info;

    unsigned int    i;

    mkext_kext    * onekext_data = 0;     // don't free
    mkext_file    * plist_file = 0;       // don't free
    mkext_file    * module_file = 0;      // don't free
    CFStringRef     entryName = NULL;     // must release
    CFMutableDictionaryRef entryDict = NULL; // must release
    CFDataRef       kextPlistDataObject = 0; // must release
    CFDictionaryRef kextPlist = 0;        // must release
    CFStringRef     errorString = NULL;   // must release
    CFDataRef       kextExecutable = 0;   // must release
    CFDictionaryKeyCallBacks keyCallBacks;


    keyCallBacks = kCFTypeDictionaryKeyCallBacks;
    keyCallBacks.equal = &CaseInsensitiveEqual;
    keyCallBacks.hash = &CaseInsensitiveHash;
    entries = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &keyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (!entries) {
        goto finish;
    }

    if (arch == NULL) {
        arch_info = NXGetLocalArchInfo();
    } else {
        arch_info = NXGetArchInfoFromName(arch);
    }
    if (arch_info == NULL) {
        fprintf(stderr, "unknown architecture '%s'\n", arch);
        error = true;
        goto finish;
    }

    fat_data = (struct fat_header *)mkextFileData;
    if (NXSwapBigLongToHost(fat_data->magic) == FAT_MAGIC) {
        int nfat = NXSwapBigLongToHost(fat_data->nfat_arch);
        struct fat_arch *arch_data =
            (struct fat_arch *)(((void *)mkextFileData) + sizeof(struct fat_header));
        for (i=0; i<nfat; i++) {
            if (NXSwapBigLongToHost(arch_data[i].cputype) == arch_info->cputype) {
                break;
            }
        }
        if (i == nfat) {
            fprintf(stderr, "archive data for architecture '%s' not found\n",
                    arch);
            error = true;
            goto finish;
        }
        mkextFileData = (char *)(mkextFileData + NXSwapBigLongToHost(arch_data[i].offset));
    }

    mkext_data = (mkext_header *)mkextFileData;

    if (NXSwapBigLongToHost(mkext_data->magic) != MKEXT_MAGIC ||
        NXSwapBigLongToHost(mkext_data->signature) != MKEXT_SIGN) {
        fprintf(stderr, "extension archive has invalid magic or signature.\n");
        error = true;
        goto finish;
    }

    crc_address = (u_int8_t *)&mkext_data->version;
    checksum = local_adler32(crc_address,
        (unsigned int)mkext_data +
        NXSwapBigLongToHost(mkext_data->length) - (unsigned int)crc_address);

    if (NXSwapBigLongToHost(mkext_data->adler32) != checksum) {
        fprintf(stderr, "extension archive has a bad checksum.\n");
        error = true;
        goto finish;
    }

    if (gVerbose) {
        fprintf(stdout, "Found %ld kexts:\n",
            NXSwapBigLongToHost(mkext_data->numkexts));
    }

    for (i = 0; i < NXSwapBigLongToHost(mkext_data->numkexts); i++) {
        if (entryName) {
            CFRelease(entryName);
            entryName = NULL;
        }
        if (entryDict) {
            CFRelease(entryDict);
            entryDict = NULL;
        }
        if (kextPlistDataObject) {
            CFRelease(kextPlistDataObject);
            kextPlistDataObject = NULL;
        }
        if (kextPlist) {
            CFRelease(kextPlist);
            kextPlist = NULL;
        }
        if (errorString) {
            CFRelease(errorString);
            errorString = NULL;
        }
        if (kextExecutable) {
            CFRelease(kextExecutable);
            kextExecutable = NULL;
        }

        entryDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        if (!entryDict) {
            fprintf(stderr, "internal error.\n");
            error = true;
            goto finish;
        }

        onekext_data = &mkext_data->kext[i];
        plist_file = &onekext_data->plist;
        module_file = &onekext_data->module;

       /*****
        * Get the plist
        */
        if (!uncompressMkextEntry(mkextFileData,
            plist_file, &kextPlistDataObject) || !kextPlistDataObject) {

            fprintf(stderr, "couldn't uncompress plist at index %d.\n", i);
            continue;
        }

        kextPlist = CFPropertyListCreateFromXMLData(kCFAllocatorDefault,
            kextPlistDataObject, kCFPropertyListImmutable, &errorString);
        if (!kextPlist) {
            if (errorString) {
                CFIndex length = CFStringGetLength(errorString);
                char * error_string = (char *)malloc((1+length) * sizeof(char));
                if (!error_string) {
                    fprintf(stderr, "memory allocation failure\n");
                    error = true;
                    goto finish;
                }
                if (CFStringGetCString(errorString, error_string,
                     length, kCFStringEncodingMacRoman)) {
                    fprintf(stderr, "error reading plist: %s",
                        error_string);
                }
                free(error_string);
            } else {
                fprintf(stderr, "error reading plist");
            }
            goto finish;
        }

        entryName = createKextNameFromPlist(entries, kextPlist);
        if (!entryName) {
            fprintf(stderr, "internal error.\n");
            error = true;
            goto finish;
        }

        CFDictionarySetValue(entryDict, CFSTR("plistData"), kextPlistDataObject);
        CFDictionarySetValue(entryDict, CFSTR("plist"), kextPlist);

        if (gVerbose) {
            char kext_name[MAXPATHLEN];
            char * bundle_id = NULL;  // don't free
            char * bundle_vers = NULL; // don't free

            if (!CFStringGetCString(entryName, kext_name, sizeof(kext_name) - 1,
                    kCFStringEncodingMacRoman)) {
                    fprintf(stderr, "memory or string conversion error\n");
            } else {
                switch (getBundleIDAndVersion(kextPlist, i, &bundle_id,
                    &bundle_vers)) {

                  case 1:
                    fprintf(stdout, "%s - %s (%s)\n", kext_name, bundle_id,
                        bundle_vers);
                    break;
                  case 0:
                    continue;
                  case -1:
                  default:
                    error = true;
                    goto finish;
                    break;
                }
            }
        }

       /*****
        * Get the executable
        */
        if (NXSwapBigLongToHost(module_file->offset) ||
            NXSwapBigLongToHost(module_file->compsize) ||
            NXSwapBigLongToHost(module_file->realsize) ||
            NXSwapBigLongToHost(module_file->modifiedsecs)) {

            if (!uncompressMkextEntry(mkextFileData,
                module_file, &kextExecutable)) {

                fprintf(stderr, "couldn't uncompress executable at index %d.\n",
                    i);
                continue;
            }

            if (kextExecutable) {
                CFDictionarySetValue(entryDict, CFSTR("executable"),
                    kextExecutable);
            }
        }

        CFDictionarySetValue(entries, entryName, entryDict);
    }

finish:
    if (error) {
        if (entries) {
            CFRelease(entries);
            entries = NULL;
        }
    }
    if (entryName)       CFRelease(entryName);
    if (entryDict)       CFRelease(entryDict);
    if (kextPlistDataObject) CFRelease(kextPlistDataObject);
    if (kextPlist)       CFRelease(kextPlist);
    if (errorString)     CFRelease(errorString);
    if (kextExecutable)  CFRelease(kextExecutable);

    return entries;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean uncompressMkextEntry(
    char * mkext_base_address,
    mkext_file * entry_address,
    CFDataRef * uncompressedEntry)
{
    Boolean result = true;

    u_int8_t * uncompressed_data = 0; // free() on error
    CFDataRef uncompressedData = NULL;    // returned
    size_t uncompressed_size = 0;

    size_t offset = NXSwapBigLongToHost(entry_address->offset);
    size_t compsize = NXSwapBigLongToHost(entry_address->compsize);
    size_t realsize = NXSwapBigLongToHost(entry_address->realsize);
    time_t modifiedsecs = NXSwapBigLongToHost(entry_address->modifiedsecs);

    *uncompressedEntry = NULL;

   /* If these four fields are zero there's no file, but that isn't
    * an error.
    */
    if (offset == 0 && compsize == 0 &&
        realsize == 0 && modifiedsecs == 0) {
        goto finish;
    }

    uncompressed_data = malloc(realsize);
    if (!uncompressed_data) {
        fprintf(stderr, "malloc failure\n");
        result = false;
        goto finish;
    }

    if (compsize != 0) {
        uncompressed_size = decompress_lzss(uncompressed_data,
            mkext_base_address + offset,
            compsize);
        if (uncompressed_size != realsize) {
            fprintf(stderr, "uncompressed file is not the length "
                  "recorded.\n");
            result = false;
            goto finish;
        }
    } else {
        bcopy(mkext_base_address + offset, uncompressed_data,
            realsize);
    }

    uncompressedData = CFDataCreate(kCFAllocatorDefault,
        (const UInt8 *)uncompressed_data, realsize);
    if (!uncompressedData) {
        fprintf(stderr, "malloc failure\n");
        result = false;
        goto finish;
    }
    *uncompressedEntry = uncompressedData;

finish:
    if (!result) {
        if (uncompressed_data) free(uncompressed_data);
    }

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean writeEntriesToDirectory(CFDictionaryRef entryDict,
    char * outputDirectory)
{
    Boolean result = true;
    unsigned int count, i;
    CFStringRef * kextNames = NULL;    // must free
    CFDictionaryRef * entries = NULL;  // must free
    char path[MAXPATHLEN];

    count = CFDictionaryGetCount(entryDict);

    kextNames = (CFStringRef *)malloc(count * sizeof(CFStringRef));
    entries = (CFDictionaryRef *)malloc(count * sizeof(CFDictionaryRef));
    if (!kextNames || !entries) {
        fprintf(stderr, "malloc failure\n");
        result = false;
    }

    CFDictionaryGetKeysAndValues(entryDict, (const void **)kextNames,
        (const void **)entries);

    for (i = 0; i < count; i++) {
        CFStringRef kextName = kextNames[i];
        char kext_name[MAXPATHLEN];  // overkill but hey
        char executable_name[MAXPATHLEN];  // overkill but hey
        CFDictionaryRef kextEntry = entries[i];
        CFDataRef fileData = NULL;
	CFDictionaryRef plist;
	CFStringRef executableString;

        const char * file_data = NULL;
        int fd;
        CFIndex bytesWritten;

        if (!CFStringGetCString(kextName, kext_name, sizeof(kext_name) -1,
            kCFStringEncodingMacRoman)) {
            fprintf(stderr, "memory or string conversion error\n");
            result = false;
            goto finish;
        }

       /*****
        * Write the plist file.
        */
        fileData = CFDictionaryGetValue(kextEntry, CFSTR("plistData"));
        if (!fileData) {
            fprintf(stderr, "kext entry %d has no plist\n", i);
            continue;
        }
        file_data = CFDataGetBytePtr(fileData);
        if (!file_data) {
            fprintf(stderr, "kext %s has no plist\n", kext_name);
            continue;
        }

        sprintf(path, "%s/", outputDirectory);
        strcat(path, kext_name);
        strcat(path, ".kext");
        if ((mkdir(path, 0777) < 0) && (errno != EEXIST)) {
            fprintf(stderr, "can't create directory %s\n", path);
            result = false;
            goto finish;
        }

        strcat(path, "/");
        strcat(path, "Contents");
        if ((mkdir(path, 0777) < 0) && (errno != EEXIST)) {
            fprintf(stderr, "can't create directory %s\n", path);
            result = false;
            goto finish;
        }

        strcat(path, "/Info.plist");

        fd = open(path, O_WRONLY | O_CREAT, 0777);
        if (fd < 0) {
            fprintf(stderr, "can't open file %s for writing\n", path);
            result = false;
            goto finish;
        }

        bytesWritten = 0;
        while (bytesWritten < CFDataGetLength(fileData)) {
            int writeResult;
            writeResult = write(fd, file_data + bytesWritten,
                CFDataGetLength(fileData) - bytesWritten);
            if (writeResult < 0) {
                fprintf(stderr, "write failed for %s\n", path);
                result = false;
                goto finish;
            }
            bytesWritten += writeResult;
        }

        close(fd);

       /*****
        * Write the executable file.
        */
        fileData = CFDictionaryGetValue(kextEntry, CFSTR("executable"));
        if (!fileData) {
            continue;
        }
        file_data = CFDataGetBytePtr(fileData);
        if (!file_data) {
            fprintf(stderr, "kext %s has no executable\n", kext_name);
            continue;
        }

        sprintf(path, "%s/", outputDirectory);
        strcat(path, kext_name);
        strcat(path, ".kext/Contents/MacOS");
        if ((mkdir(path, 0777) < 0) && (errno != EEXIST)) {
            fprintf(stderr, "can't create directory %s\n", path);
            result = false;
            goto finish;
        }

        plist = CFDictionaryGetValue(kextEntry, CFSTR("plist"));
	executableString = CFDictionaryGetValue(plist, CFSTR("CFBundleExecutable"));
        if (!executableString) {
            fprintf(stderr, "kext %s has executable but no CFBundleExecutable property\n", kext_name);
            continue;
        }
        if (!CFStringGetCString(executableString, executable_name, sizeof(executable_name) -1,
            kCFStringEncodingMacRoman)) {
            fprintf(stderr, "memory or string conversion error\n");
            result = false;
            goto finish;
        }

        strcat(path, "/");
        strcat(path, executable_name);

        fd = open(path, O_WRONLY | O_CREAT, 0777);
        if (fd < 0) {
            fprintf(stderr, "can't open file %s for writing\n", path);
        }

        bytesWritten = 0;
        while (bytesWritten < CFDataGetLength(fileData)) {
            int writeResult;
            writeResult = write(fd, file_data + bytesWritten,
                CFDataGetLength(fileData) - bytesWritten);
            if (writeResult < 0) {
                fprintf(stderr, "write failed for %s\n", path);
                result = false;
                goto finish;
            }
            bytesWritten += writeResult;
        }

        close(fd);
    }

finish:
    if (kextNames) free(kextNames);
    if (entries)   free(entries);
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
CFStringRef createKextNameFromPlist(
    CFDictionaryRef entries, CFDictionaryRef kextPlist)
{
    CFStringRef result = NULL; // returned
    CFStringRef bundleName = NULL; // don't release
    CFStringRef bundleID = NULL; // don't release
    static unsigned int nameUnknownIndex = 1;
    unsigned int dupIndex = 1;
    CFArrayRef idParts = NULL; // must release

   /* First see if there's a CFBundleExecutable.
    */
    bundleName = CFDictionaryGetValue(kextPlist, CFSTR("CFBundleExecutable"));
    if (!bundleName) {

       /* No? Try for CFBundleIdentifier and get the last component.
        */
        bundleID = CFDictionaryGetValue(kextPlist, CFSTR("CFBundleIdentifier"));
        if (bundleID) {
            CFIndex length;

            idParts = CFStringCreateArrayBySeparatingStrings(
            kCFAllocatorDefault, bundleID, CFSTR("."));
            length = CFArrayGetCount(idParts);
            bundleName = (CFStringRef)CFArrayGetValueAtIndex(idParts, length - 1);

           /* Last compoonent of identifer null? Sigh, back to square one.
            */
            if (!CFStringGetLength(bundleName)) {
                bundleName = NULL;
            }
        }

       /* If we didn't find a name to use, conjure one up.
        */
        if (!bundleID) {
            result = CFStringCreateWithFormat(kCFAllocatorDefault,
                NULL, CFSTR("NameUnknown-%d"), nameUnknownIndex);
            nameUnknownIndex++;
            goto finish;
        }
    }

   /* See if we already have the name found; if so, add numbers until we get
    * a unique.
    */
    if (CFDictionaryGetValue(entries, bundleName)) {
        for ( ; ; dupIndex++) {
            result = CFStringCreateWithFormat(kCFAllocatorDefault,
                NULL, CFSTR("%@-%d"), bundleName, dupIndex);
            if (!CFDictionaryGetValue(entries, result)) {
                goto finish;
            }
            CFRelease(result);
            result = NULL;
        }
    } else {
        result = CFRetain(bundleName);
        goto finish;
    }
finish:
    if (idParts) CFRelease(idParts);
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
int getBundleIDAndVersion(CFDictionaryRef kextPlist, unsigned index,
    char ** bundle_id_out, char ** bundle_version_out)
{
    int result = 1;

    CFStringRef bundleID = NULL; // don't release
    CFStringRef bundleVersion = NULL; // don't release
    static char bundle_id[KMOD_MAX_NAME];
    static char bundle_vers[KMOD_MAX_NAME];

    bundleID = CFDictionaryGetValue(kextPlist, CFSTR("CFBundleIdentifier"));
    if (!bundleID) {
        fprintf(stderr, "kext entry %d has no CFBundleIdentifier\n", index);
        result = 0;
        goto finish;
    } else if (CFGetTypeID(bundleID) != CFStringGetTypeID()) {
        fprintf(stderr, "kext entry %d, CFBundleIdentifier is not a string\n", index);
        result = 0;
        goto finish;
    } else {
        if (!CFStringGetCString(bundleID, bundle_id, sizeof(bundle_id) - 1,
            kCFStringEncodingMacRoman)) {
            fprintf(stderr, "memory or string conversion error\n");
            result = -1;
            goto finish;
        }
    }

    bundleVersion = CFDictionaryGetValue(kextPlist,
        CFSTR("CFBundleVersion"));
    if (!bundleVersion) {
        fprintf(stderr, "kext entry %d has no CFBundleVersion\n", index);
        result = 0;
        goto finish;
    } else if (CFGetTypeID(bundleVersion) != CFStringGetTypeID()) {
        fprintf(stderr, "kext entry %d, CFBundleVersion is not a string\n", index);
        result = 0;
        goto finish;
    } else {
        if (!CFStringGetCString(bundleVersion, bundle_vers,
            sizeof(bundle_vers) - 1, kCFStringEncodingMacRoman)) {
            fprintf(stderr, "memory or string conversion error\n");
            result = -1;
            goto finish;
        }
    }

    if (bundle_id_out) {
        *bundle_id_out = bundle_id;
    }

    if (bundle_version_out) {
        *bundle_version_out = bundle_vers;
    }


finish:

    return result;
}
