// 45678901234567890123456789012345678901234567890123456789012345678901234567890
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <mach/mach.h>
#include <mach/mach_error.h>

#include <architecture/byte_order.h>

#include <CoreFoundation/CoreFoundation.h>

#include <Kernel/libsa/mkext.h>

// In lcss.c
__private_extern__ u_int8_t *
encodeLZSS(u_int8_t *dstP, long dstLen, u_int8_t *srcP, long srcLen);
__private_extern__ void
checkLZSS(u_int8_t *codeP, u_int8_t *srcEnd, u_int8_t *textP, u_int32_t tLen);

// In fat.c
__private_extern__ void
find_arch(u_int8_t **dataP, off_t *sizeP, cpu_type_t in_cpu,
    cpu_subtype_t in_cpu_subtype, u_int8_t *data_ptr, off_t filesize);
__private_extern__ int
get_arch_from_flag(char *name, cpu_type_t *cpuP, cpu_subtype_t *subcpuP);

#define KEXT_PATH "/System/Library/Extensions"
#define KEXT_BUNDLEEXTN ".kext"
#define KEXT_PACKAGETYPE "KEXT"
#define INFO_PATH "Contents/Info.plist"

#define kMkxtDefaultSize (6 * 1024 * 1024)

#define kKEXTPathStr CFSTR(KEXT_PATH)
#define kKEXTBundleExtnStr CFSTR(KEXT_BUNDLEEXTN)
#define kKEXTPackageTypeStr CFSTR(KEXT_PACKAGETYPE)

#define kKEXTPackageTypeKey CFSTR("CFBundlePackageType")

#define CFDynamicCast(type, ref) \
    ((type ## Ref) ((CFGetTypeID(ref) == type ## GetTypeID())? ref : 0))

static volatile void bomb(int code, const char *msg, ...);
#define checkFatal(expr, msg) do {  \
    if ( !(expr) ) {                \
        bomb msg;                   \
    }                               \
} while(0)

#define ZERO_POINT ((UInt8 *) sKEXTs)
#define ADLER_POINT ((UInt8 *) &sKEXTs->version)

static const char *sCmdName = NULL;
static int sVerbose = 0;

static mkext_header *sKEXTs = 0;
static u_int8_t *sKEXTEnd = 0;

static cpu_type_t sCPU;
static cpu_subtype_t sSubType;
static const char *sOutFilename;

static Boolean sLocalRoot = FALSE;
static Boolean sNetworkRoot = FALSE;

struct validationContext {
    CFMutableDictionaryRef dict;
    Boolean isChild;
};

struct compressContext {
    mkext_kext *curkext;
    u_int8_t *dst;
};

static volatile void bomb(int code, const char *msg, ...)
{
    char buf[1024];
    va_list ap;

    va_start(ap, msg);
    vsnprintf(buf, sizeof(buf), msg, ap);
    va_end(ap);

    fprintf(stderr, "%s: %s\n", sCmdName, buf);
    fflush(stderr);
    exit(code);
}

static volatile void verbosePrintf(int level, const char *msg, ...)
{
    if (level <= sVerbose) {
        va_list ap;
    
        va_start(ap, msg);
        vprintf(msg, ap);
        va_end(ap);
        fflush(stdout);
    }
}

static volatile void usage(void)
{
    static const char *usageMessage = 
"Usage: %s [-v [1-3]] [-t arch] [-l] [-n] [-o <filename>]\n"
"  [-d <directory> ...] [<kextlist> ...]\n"
"  -v   run in verbose mode (may specify level 1, 2, or 3)\n"
"  -t   thin KEXT executables to one architecture only\n"
"  -l   include only KEXTs required for local disk boot\n"
"  -n   include only KEXTs required for network boot\n"
"       (-l and -n may be combined)\n"
"  -d   directory to scan for kernel extensions\n"
"  -o   write archive to filename (.mkext or .mke extension)\n";

    fprintf(stderr, usageMessage, sCmdName);
    exit(EX_USAGE);
}

static CFURLRef URLCreateWithFileSystemPath(const char *dirname)
{
    CFURLRef relURL, absURL;

    relURL = CFURLCreateFromFileSystemRepresentation
                (NULL, dirname, strlen(dirname), TRUE);
    if (!relURL)
        return relURL;

    absURL = CFURLCopyAbsoluteURL(relURL);
    CFRelease(relURL);

    return absURL;
}

static void ArrayAppendDirPath(CFMutableArrayRef dirList, const char *dirArg)
{
    CFURLRef url;

    url = URLCreateWithFileSystemPath(dirArg);
    checkFatal(url, (EX_TEMPFAIL, "Couldn't create url - no memory?"));

    CFArrayAppendValue(dirList, url);
    CFRelease(url);
}

static void URLAbsoluteGetPath(CFURLRef url, char *buf, int len)
{
    CFURLRef absURL;
    CFStringRef urlPath;

    absURL = CFURLCopyAbsoluteURL(url); assert(absURL);
    urlPath = CFURLCopyPath(absURL); assert(urlPath);
    CFRelease(absURL);

    CFStringGetCString(urlPath, buf, len, kCFStringEncodingMacRoman);
    CFRelease(urlPath);
}

static void BundleGetInfoPlistPath(CFBundleRef bundle, char *buf, int len)
{
    CFURLRef dataURL;
    int baseLen;

    dataURL = CFBundleCopyBundleURL(bundle); assert(dataURL);
    URLAbsoluteGetPath(dataURL, buf, len - 1);
    CFRelease(dataURL);

    baseLen = strlen(buf);
    strncpy(buf + baseLen, INFO_PATH, len - baseLen);
}

// Process the arguments and return a list of candidate kernel extensions.
static CFArrayRef createBundleList(int argc, const char *argv[])
{
    CFMutableArrayRef bundleList;
    CFMutableArrayRef baseDirList;
    char c;
    int i;
    Boolean baseDirSpecified = FALSE;

    // initialise the list of directories to search to none
    baseDirList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    checkFatal(baseDirList,
        (EX_TEMPFAIL, "Couldn't create base dir array - no memory?"));

    // initialise the list of kernel extension candidates to null
    bundleList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    checkFatal(bundleList,
        (EX_TEMPFAIL, "Couldn't Bundle array - no memory?"));

    // initialize the fat architecture types to NO thinning.
    sCPU = CPU_TYPE_ANY;
    sSubType = CPU_SUBTYPE_MULTIPLE;
    sOutFilename = NULL;

    // Process arguments
    while ((c = getopt(argc, argv, "vt:d:o:ln?")) != -1) {
        switch(c) {
        case 'd':
            ArrayAppendDirPath(baseDirList, optarg);
            baseDirSpecified = TRUE;
            break;
        case 'l':
            sLocalRoot = TRUE;
            break;
        case 'n':
            sNetworkRoot = TRUE;
            break;
        case 'o': sOutFilename = optarg; break;
        case 't': get_arch_from_flag(optarg, &sCPU, &sSubType); break;
        case 'v':
        {
            const char * next = argv[optind];
            if ((next[0] == '1' || next[0] == '2' || next[0] == '3') &&
                next[1] == '\0') {
                sVerbose = atoi(argv[optind]);
                optind++;
            } else if (next[0] == '-') {
                sVerbose = 1;
            } else {
                usage();
            }
            break;
        }
        case '?':
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;

    // Grab the list of kernel extensions after the option list
    for (i = 0; i < argc; i++) {
        CFURLRef url;
        CFBundleRef kext;

        url = URLCreateWithFileSystemPath(argv[i]);
        checkFatal(url, (EX_TEMPFAIL, "Couldn't create url - no memory?"));

        kext = CFBundleCreate(NULL, url); CFRelease(url);
        if (kext) {
            CFArrayAppendValue(bundleList, kext);
            CFRelease(kext);
        }
    }

    // If we don't have any directories to search and no bundles
    // were specified then default to searching the Extensions directory
    if (!baseDirSpecified && !CFArrayGetCount(bundleList)) {
        ArrayAppendDirPath(baseDirList, KEXT_PATH);
        baseDirSpecified = TRUE;
    }

    // For each base dir, find child directories of type '.kext' and
    // append them to the kext list.  Finally we are finished with the
    // baseDirList so release it now.
    if (baseDirSpecified) {
        int count = CFArrayGetCount(baseDirList);

        for (i = 0; i < count; i++) {
            CFTypeRef obj = CFArrayGetValueAtIndex(baseDirList, i);
            CFURLRef dirUrl = CFDynamicCast(CFURL, obj);
            CFArrayRef kextList = 0;
        
            if (dirUrl) {
                kextList = CFBundleCreateBundlesFromDirectory
                                    (NULL, dirUrl, kKEXTBundleExtnStr);
            }
            if (kextList) {
                CFArrayAppendArray(bundleList, kextList, 
                                   CFRangeMake(0, CFArrayGetCount(kextList)));
                CFRelease(kextList);
            }
        }
        CFRelease(baseDirList);
    }

    checkFatal(CFArrayGetCount(bundleList),
              (EX_NOINPUT, "No kernel extensions found"));

    return bundleList;
}

// A bundle is valid if
//  - is a CFBundle class
//  - has an identifier
//  - has a package type of KEXT
//  - has a readable Info.plist
//
// If it is a valid bundle and it isn't already recursive then
// recursively check for some more children in the PlugIn Directory
static void validateBundle(const void *val, void *context)
{
    CFBundleRef kext = CFDynamicCast(CFBundle, (CFTypeRef) val);
    CFTypeRef   rawValue;  // used for safe typecasting
    CFStringRef bundleIdent = 0;
    CFStringRef bundleType = 0;
    char bundle_ident[120];
    CFURLRef execUrl, pluginDir;
    CFArrayRef childKexts;
    CFIndex childCount;
    struct validationContext *c = context;
    char buf[MAXPATHLEN+1];
    int fd;

// FIXME: Need to add printout of reason for failure (early return from this
// function.

    if (!kext)
        return;  // This isn't a bundle so return immediately

    // Is this in the list of bundles already?
    bundleIdent = CFBundleGetIdentifier(kext);
    if (!bundleIdent || CFDictionaryGetValue(c->dict, bundleIdent))
        return;

    if (!CFStringGetCString(bundleIdent, bundle_ident, sizeof(bundle_ident) / sizeof(char),
        kCFStringEncodingASCII)) {

        strcpy(bundle_ident, "(unknown)");
    }

    // If sLocalRoot or sNetworkRoot specified, cull bundles with
    // OSBundleRequired key not set or not correct for setting.
    //
    if (sLocalRoot || sNetworkRoot) {
        do {
            CFStringRef requiredString = 0;

            rawValue = CFBundleGetValueForInfoDictionaryKey(kext,
                CFSTR("OSBundleRequired"));
            if (rawValue) {
               requiredString = CFDynamicCast(CFString, rawValue);
            }

            if (!requiredString) {

                // no OSBundleRequired; bail on this one
                verbosePrintf(3,
                    "Skipping bundle %s; no OSBundleRequired key.\n",
                    bundle_ident);
                return;

            } else if (CFStringCompare(requiredString,
                    CFSTR("Root"), 0) == kCFCompareEqualTo ||
                CFStringCompare(requiredString,
                    CFSTR("Console"), 0) == kCFCompareEqualTo) {

                // "Root" and "Console" always included
                continue;

            } else if (sLocalRoot &&
                CFStringCompare(requiredString, CFSTR("Local-Root"), 0) ==
                    kCFCompareEqualTo) {

                // Match for local
                continue;

            } else if (sNetworkRoot &&
                CFStringCompare(requiredString, CFSTR("Network-Root"), 0) ==
                    kCFCompareEqualTo) {

                // Match for network
                continue;

            } else {
                char required_string[120];

               // skip for any other value

               if (!CFStringGetCString(requiredString, required_string,
                   sizeof(required_string) / sizeof(char),
                   kCFStringEncodingASCII)) {

                   strcpy(required_string, "(unknown)");
                }

                verbosePrintf(3,
                    "Skipping bundle %s; OSBundleRequired key is \"%s\".\n",
                    bundle_ident, required_string);
                return;
            }
        } while (0);
    }

    // Validate that the bundle is of type KEXT
    rawValue = CFBundleGetValueForInfoDictionaryKey(kext, kKEXTPackageTypeKey);
    if (rawValue) {
        bundleType = CFDynamicCast(CFString, rawValue);
    }
    if (!bundleType || !CFEqual(bundleType, kKEXTPackageTypeStr))
        return;

    BundleGetInfoPlistPath(kext, buf, sizeof(buf));
    fd = open(buf, O_RDONLY, 0);
    if (-1 == fd)
        return;
    close(fd);

    // Check for an executable, and make sure it is readable and
    // of the right architecture
    execUrl = CFBundleCopyExecutableURL(kext);
    if (execUrl) {
        URLAbsoluteGetPath(execUrl, buf, sizeof(buf) - 1);
        CFRelease(execUrl);
        if (buf && *buf) {
            int cc;
            off_t size;

            fd = open(buf, O_RDONLY, 0);
            if (-1 == fd)
                return;  // Can't read executable

            cc = read(fd, buf, 512);  // First block only
            close(fd);
            if (512 != cc)
                return; //  Not even one block in size?
                
            find_arch(NULL, &size, sCPU, sSubType, buf, 512);
            if (!size)  // Couldn't find an architecture
                return;
        }
    }

    CFDictionarySetValue(c->dict, bundleIdent, kext);

    // If we are already a child then don't check further for children
    if (c->isChild) 
        return;
    
    pluginDir = CFBundleCopyBuiltInPlugInsURL(kext);
    if (!pluginDir)
        return;

    childKexts = CFBundleCreateBundlesFromDirectory
                        (NULL, pluginDir, kKEXTBundleExtnStr);
    CFRelease(pluginDir);
    if (!childKexts)
        return;

    if ( (childCount = CFArrayGetCount(childKexts)) ) {
        struct validationContext childContext;

        childContext.dict = c->dict;
        childContext.isChild = TRUE;
        CFArrayApplyFunction(childKexts,
                                CFRangeMake(0, childCount),
                                validateBundle,
                                &childContext);
    }
    CFRelease(childKexts);
}

static CFDictionaryRef validateBundleList(CFArrayRef bundleList)
{
    struct validationContext context;
    CFMutableDictionaryRef bundleDict;

    bundleDict = CFDictionaryCreateMutable(NULL, 0,
                        &kCFTypeDictionaryKeyCallBacks,
                        &kCFTypeDictionaryValueCallBacks);
    checkFatal(bundleDict,
        (EX_TEMPFAIL, "Couldn't create bundle array - no memory?"));

    context.dict = bundleDict;
    context.isChild = FALSE;
    CFArrayApplyFunction(bundleList,
                         CFRangeMake(0, CFArrayGetCount(bundleList)),
                         validateBundle,
                         &context);

    checkFatal(CFDictionaryGetCount(bundleDict),
        (EX_TEMPFAIL, "Couldn't find any valid bundles to cache"));

    return bundleDict;
}

// compress one file and fill in it's mkext_file structure
// However if we have no work, i.e. the filename is empty, the file isn't
// readable or the size is 0, then just return immediately
static u_int8_t *
compressFile(u_int8_t *dst, mkext_file *file, char *fileName)
{
    struct stat statbuf;
    off_t size;
    u_int8_t *dstend, *src, *data;
    int fd;

    memset(file, '\0', sizeof(*file));
    if (!fileName || !*fileName)
        return dst;

    fd = open(fileName, O_RDONLY, 0);
    if (-1 == fd)
        return dst;
        
    checkFatal(-1 != fstat(fd, &statbuf),
        (EX_NOINPUT, "Can't stat file %s - %s", fileName, strerror(errno)));

    if (!statbuf.st_size) {
        close(fd);
        return dst;
    }

    src = mmap(0, (size_t) statbuf.st_size, PROT_READ, MAP_FILE, fd, 0);
    checkFatal(-1 != (int) src,
        (EX_SOFTWARE, "Can't map file %s - %s", fileName, strerror(errno)));

    find_arch(&data, &size, sCPU, sSubType, src, statbuf.st_size);
    if (!size) {
        verbosePrintf(1, "Couldn't find valid architecture for %s\n", fileName);
        munmap(src, statbuf.st_size);
        close(fd);
        return dst;
    }

    verbosePrintf(2, "Compressing %s %ld => ", fileName, (size_t) size);
    dstend = compress_lzss(dst, sKEXTEnd - dst, data, size);
    checkFatal(dstend,
        (EX_SOFTWARE, "Too many kexts to fit in mkext"));
    file->offset       = NXSwapHostLongToBig(dst - ZERO_POINT);
    file->realsize     = NXSwapHostLongToBig((size_t) size);
    file->compsize     = NXSwapHostLongToBig(dstend - dst);
    file->modifiedsecs = NXSwapHostLongToBig(statbuf.st_mtimespec.tv_sec);

    if (dstend >= dst + size) {
        // Compression grew the code so copy in clear - already compressed?
        file->compsize = 0;
        memcpy(dst, data, (size_t) size);
        dstend = dst + size;
    }

   /* checkFatal() exits the program, so it's okay for the malloc()'ed data
    * not to be freed.
    */
    if (file->compsize && 3 <= sVerbose) {
        u_int8_t *chkbuf = (u_int8_t *)malloc((size_t) size);
        size_t chklen;

        chklen = decompress_lzss(chkbuf, dst, dstend - dst);
        checkFatal(chklen == (size_t) size, (EX_SOFTWARE,
            "Internal error, decompressed size expected %ld got %d",
            (size_t) size, chklen));
        checkFatal(0 == memcmp(chkbuf, data, (size_t) size),
            (EX_SOFTWARE, "Internal error, decompressed not same as input"));
        free(chkbuf);
    }

    verbosePrintf(2, "%ld\n", NXSwapBigLongToHost(file->compsize));

    checkFatal(-1 != munmap(src, (size_t) statbuf.st_size),
        (EX_SOFTWARE, "Can't unmap memory - %s", strerror(errno)));

    close(fd);

    return dstend;
}

static void
compressAndSaveBundle(const void *key, const void *val, void *context)
{
    char curfile[MAXPATHLEN+1];
    CFBundleRef kext;
    CFURLRef dataURL;
    struct compressContext *current = context;
    u_int8_t *dst = current->dst;

    kext = CFDynamicCast(CFBundle, (CFTypeRef) val); assert(kext);

    BundleGetInfoPlistPath(kext, curfile, sizeof(curfile));
    dst = compressFile(dst, &current->curkext->plist, curfile);
    checkFatal(dst, (EX_NOPERM, 
        "Can't open info file %s - %s", curfile, strerror(errno)));

    dataURL = CFBundleCopyExecutableURL(kext);
    if (dataURL) {
        URLAbsoluteGetPath(dataURL, curfile, sizeof(curfile) - 1);
        CFRelease(dataURL);
    }
    else
        *curfile = '\0';  // No executable
    dst = compressFile(dst, &current->curkext->module, curfile);

    current->dst = dst;
    current->curkext++;
}

static void compressBundleDict(CFDictionaryRef validBundles)
{
    CFIndex bundleCount = CFDictionaryGetCount(validBundles);
    struct compressContext context;
    kern_return_t ret;
    vm_address_t driverVM;

    checkFatal(bundleCount,
        (EX_NOINPUT, "Couldn't find any valid bundles to bundle up"));

    ret = vm_allocate(mach_task_self(),
                      &driverVM, kMkxtDefaultSize, VM_FLAGS_ANYWHERE);
    checkFatal(ret == KERN_SUCCESS, (EX_OSERR,
        "Failed to allocate address space - %s", mach_error_string(ret)));

    sKEXTs = (mkext_header *) driverVM;
    sKEXTEnd = (u_int8_t *) (driverVM + kMkxtDefaultSize);
    sKEXTs->magic = NXSwapHostIntToBig(MKEXT_MAGIC);
    sKEXTs->signature = NXSwapHostIntToBig(MKEXT_SIGN);
    sKEXTs->version = NXSwapHostIntToBig(0x01008000);   // 'vers' 1.0.0
    sKEXTs->numkexts = NXSwapHostIntToBig(bundleCount);
    sKEXTs->cputype = NXSwapHostIntToBig(sCPU);
    sKEXTs->cpusubtype = NXSwapHostIntToBig(sSubType);

    //
    // Setup context for the dictionary walk.
    // First set the current kext to be the first kext entry.
    // Then set the pointer for compressed data stream to the
    // first byte after the kext list in the header section.
    context.curkext = &sKEXTs->kext[0];
    context.dst = (u_int8_t *) &sKEXTs->kext[bundleCount];

    CFDictionaryApplyFunction(validBundles, compressAndSaveBundle, &context);
    
    sKEXTs->length = NXSwapHostIntToBig(context.dst - ZERO_POINT);
    sKEXTs->adler32 = 
        NXSwapHostIntToBig(adler32(ADLER_POINT, context.dst - ADLER_POINT));
}

static void outputFile(void)
{
    size_t length = NXSwapBigIntToHost(sKEXTs->length);
    char filename[MAXPATHLEN];

    if (sOutFilename && sOutFilename[0] != '\0') {
        int fd;
        ssize_t res;
    
        strncpy(filename, sOutFilename, sizeof(filename));
    
        fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0666);
        checkFatal(-1 != fd,
            (EX_CANTCREAT, "Can't create %s - %s", filename, strerror(errno)));
    
        res = write(fd, sKEXTs, length);
        close(fd);
        checkFatal(-1 != res,
            (EX_IOERR, "Can't write %s - %s", filename, strerror(errno)));
    }
    else
        strcpy(filename, "(file unspecified)");

    verbosePrintf(1, "%s: %s contains %d kexts for %d bytes with crc 0x%x\n",
        sCmdName, 
        filename,
        NXSwapBigIntToHost(sKEXTs->numkexts),
        length,
        NXSwapBigIntToHost(sKEXTs->adler32));
}

int main (int argc, const char *argv[])
{
    CFArrayRef bundleList;
    CFDictionaryRef bundleDict;

    // Get the command name and strip the leading dirpath
    if ( (sCmdName = strrchr(argv[0], '/')) )
        sCmdName++;
    else
        sCmdName = argv[0];

    bundleList = createBundleList(argc, argv); assert(bundleList);

    bundleDict = validateBundleList(bundleList); assert(bundleDict);
    CFRelease(bundleList);

    compressBundleDict(bundleDict); assert(sKEXTs);
    CFRelease(bundleDict);

    outputFile();

    verbosePrintf(3, "%s: struct File = %d, KEXT = %d, header = %d\n",
        sCmdName, sizeof(mkext_file), sizeof(mkext_kext), sizeof(mkext_header));

    exit(0);
}
