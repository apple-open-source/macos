#if !__LP64__

#include <sys/cdefs.h>

#include <pthread.h>
#include <libc.h>
#include <CoreFoundation/CFRuntime.h>
#include <zlib.h>
#include <errno.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/IOKitServer.h>

#include "KXKextRepository.h"
#include "KXKextRepository_private.h"
#include "path_util.h"


#define CACHE_PERMS             (0644)

/*******************************************************************************
* The basic data structure for a kext repository (a directory containing kexts
* that can be scanned and rescanned).
*******************************************************************************/

typedef struct __KXKextRepository {

    CFRuntimeBase  cfBase;   // base CFType information

    KXKextManagerRef manager;

    CFStringRef  repositoryPath;    // canonicalized to absolute path
    Boolean   needsReset;           // indiv. kext: cache inconsistency found at some point
    Boolean   useCache;             // use/update cache files when possible
    Boolean   scansForKexts;        // scan whole directory or just kexts known

    Boolean hasAuthenticated;
    Boolean scanTimeValid;
    time_t  scanTime;            // mod time of source at scan start

   /* Kexts whose loadable status has yet to be determined or which may
    * change (disabled/enabled, dependencies arriving/departing).
    */
    CFMutableArrayRef  candidateKexts;

   /* Kexts with hard failures. These kexts aren't currently loadable,
    * but may become so if they fixed and rescanned.
    */
    CFMutableArrayRef  badKexts;

   /* Serialized personalities of valid kexts.
    */
    CFDataRef           personalityCache;

} __KXKextRepository, * __KXKextRepositoryRef;

/*******************************************************************************
* Private function declarations. Definitions at bottom of this file.
*******************************************************************************/
void __KXKextRepositoryInitialize(void);
__KXKextRepositoryRef __KXKextRepositoryCreatePrivate(
    CFAllocatorRef       allocator,
    CFAllocatorContext * context);
static CFStringRef __KXKextRepositoryCopyDebugDescription(CFTypeRef cf);
void __KXKextRepositoryReleaseContents(CFTypeRef aRepository);
KXKextManagerError __KXKextRepositoryScanDirectory(
    KXKextRepositoryRef aRepository);
static void __KXKextRepositoryAuthenticateKextArray(
    __KXKextRepositoryRef aRepository,
    CFMutableArrayRef kexts,
    CFMutableArrayRef badKexts);
static void __KXKextRepositoryCheckIntegrityOfKextArray(
    __KXKextRepositoryRef aRepository,
    CFMutableArrayRef kexts,
    CFMutableArrayRef badKexts,
    CFMutableArrayRef bomArray);


/*******************************************************************************
* Core Foundation Class Definition Stuff
*
* Private functions are at the bottom of this file with other module-internal
* code.
*******************************************************************************/

/* This gets set by __KXKextRepositoryInitialize().
 */
static CFTypeID __kKXKextRepositoryTypeID = _kCFRuntimeNotATypeID;

CFTypeID KXKextGetRepositoryTypeID(void) {
    return __kKXKextRepositoryTypeID;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerRef KXKextRepositoryGetManager(KXKextRepositoryRef aRepository)
{
    return aRepository->manager;
}

/*******************************************************************************
*
*******************************************************************************/
CFURLRef KXKextRepositoryCopyURL(KXKextRepositoryRef aRepository)
{
    CFURLRef theURL = NULL; // returned
    theURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                aRepository->repositoryPath, kCFURLPOSIXPathStyle, true);
    return theURL;
}

/*******************************************************************************
*
*******************************************************************************/
CFStringRef KXKextRepositoryGetPath(KXKextRepositoryRef aRepository)
{
    return aRepository->repositoryPath;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextRepositoryGetScansForKexts(KXKextRepositoryRef aRepository)
{
    return aRepository->scansForKexts;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError KXKextRepositorySetScansForKexts(
    KXKextRepositoryRef aRepository, Boolean flag)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    Boolean oldScansForKexts = aRepository->scansForKexts;

    aRepository->scansForKexts = flag;
    if (!oldScansForKexts && aRepository->scansForKexts) {
        result = KXKextRepositoryReset(aRepository);
    }
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextRepositoryGetNeedsReset(KXKextRepositoryRef aRepository)
{
    return aRepository->needsReset;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextRepositorySetNeedsReset(
    KXKextRepositoryRef aRepository,
    Boolean flag)
{
    aRepository->needsReset = flag;
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextRepositoryResolveBadKextDependencies(KXKextRepositoryRef aRepository)
{
    CFIndex count, i;
    KXKextRef thisKext;

    count = CFArrayGetCount(aRepository->badKexts);
    for (i = 0; i < count; i++) {
        thisKext = (KXKextRef)CFArrayGetValueAtIndex(aRepository->badKexts, i);
        KXKextResolveDependencies(thisKext);
    }

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextRepositoryEmpty(KXKextRepositoryRef aRepository)
{
    if (KXKextManagerGetLogLevel(aRepository->manager) >=
           kKXKextManagerLogLevelDetails) {

        const char * repository_name =
            _KXKextRepositoryCopyCanonicalPathnameAsCString(aRepository);
        if (repository_name) {
            _KXKextManagerLogMessage(aRepository->manager,
                "emptying repository %s", repository_name);
            free((char *)repository_name);
        }
    }

   /*****
    * Do this before dumping kexts so their relationships get cleared.
    * Since we're releasing the kexts that really isn't necessary, though,
    * is it?
    */
    KXKextManagerClearRelationships(aRepository->manager);

    CFArrayRemoveAllValues(aRepository->candidateKexts);
    CFArrayRemoveAllValues(aRepository->badKexts);

    aRepository->hasAuthenticated = false;

    return;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError KXKextRepositoryScan(
    KXKextRepositoryRef aRepository)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    result = __KXKextRepositoryScanDirectory(aRepository);

    aRepository->hasAuthenticated = false;

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError KXKextRepositoryResetIfNeeded(
    KXKextRepositoryRef aRepository)
{
    if (KXKextRepositoryGetNeedsReset(aRepository)) {
        const char * repository_name =
            _KXKextRepositoryCopyCanonicalPathnameAsCString(aRepository);
        if (repository_name) {
            _KXKextManagerLogMessage(aRepository->manager,
                "rescanning %s due to cache inconsistency", repository_name);
            free((char *)repository_name);
        }
        return KXKextRepositoryReset(aRepository);
    }
    return kKXKextManagerErrorNone;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError KXKextRepositoryReset(
    KXKextRepositoryRef aRepository)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    CFMutableArrayRef kextURLs = NULL;  // must release
    CFIndex count, i;

    if (KXKextManagerGetLogLevel(aRepository->manager) >=
            kKXKextManagerLogLevelDetails) {

        const char * repository_name =
            _KXKextRepositoryCopyCanonicalPathnameAsCString(aRepository);
        if (repository_name) {
            _KXKextManagerLogMessage(aRepository->manager,
                "resetting repository %s", repository_name);
            free((char *)repository_name);
        }
    }

   /*****
    * A repository that scans is easy to do; just empty it and have it
    * scan again. A repository that doesn't a little more involved. It
    * must record the URLs of all kexts it holds, empty itself, and then
    * try to recreate just those original kexts. If any of them has gone
    * missing, oh well!
    */
    if (aRepository->scansForKexts) {
        KXKextRepositoryEmpty(aRepository);
        result = KXKextRepositoryScan(aRepository);

       /* don't use the cache-write result as the result of this function
        */
        if (aRepository->useCache) {
            KXKextRepositoryWriteCache(aRepository, NULL);
        }

    } else {
        kextURLs = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
        if (!kextURLs) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }

       /* Record URLs of all kexts, good & bad.
        */
        count = CFArrayGetCount(aRepository->candidateKexts);
        for (i = 0; i < count; i++) {
            KXKextRef thisKext = (KXKextRef)CFArrayGetValueAtIndex(
                aRepository->candidateKexts, i);
            CFURLRef kextURL = KXKextGetAbsoluteURL(thisKext);
            CFArrayAppendValue(kextURLs, kextURL);
        }

        count = CFArrayGetCount(aRepository->badKexts);
        for (i = 0; i < count; i++) {
            KXKextRef thisKext = (KXKextRef)CFArrayGetValueAtIndex(
                aRepository->badKexts, i);
            CFURLRef kextURL = KXKextGetAbsoluteURL(thisKext);
            CFArrayAppendValue(kextURLs, kextURL);
        }

       /* Empty the repository.
        */
        KXKextRepositoryEmpty(aRepository);

       /* Add back all of the kexts for recorded URLs.
        */
        count = CFArrayGetCount(kextURLs);
        for (i = 0; i < count; i++) {
            CFURLRef thisURL = (CFURLRef)CFArrayGetValueAtIndex(
                kextURLs, i);
            KXKextRef newKext = NULL;  // must release

            newKext = _KXKextCreate(kCFAllocatorDefault);
            if (!newKext) {
                result = kKXKextManagerErrorNoMemory;
                goto finish;
            }
            result = _KXKextInitWithURLInRepository(newKext, thisURL,
                aRepository);
            if (result == kKXKextManagerErrorNoMemory ||
                result == kKXKextManagerErrorFileAccess ||
                result == kKXKextManagerErrorNotADirectory ||
                result == kKXKextManagerErrorKextNotFound ||
                result == kKXKextManagerErrorURLNotInRepository ||
                result == kKXKextManagerErrorNotABundle ||
                result == kKXKextManagerErrorNotAKext) {

                // kext is completely unusable, do not store in repository

            } else if (result == kKXKextManagerErrorNone) {
		if (KXKextManagerGetLogLevel(aRepository->manager) >=
			kKXKextManagerLogLevelKexts) {
		    const char * kext_name =
			_KXKextCopyBundlePathInRepositoryAsCString(newKext);
		    if (kext_name) {
			_KXKextManagerLogMessage(aRepository->manager,
			    "reset found valid extension %s", kext_name);
			free((char *)kext_name);
		    }
		}
		CFArrayAppendValue(aRepository->candidateKexts, newKext);
	    } else {
		if (KXKextManagerGetLogLevel(aRepository->manager) >=
			kKXKextManagerLogLevelKexts) {
		    const char * kext_name =
			_KXKextCopyBundlePathInRepositoryAsCString(newKext);
		    if (kext_name) {
			_KXKextManagerLogMessage(aRepository->manager,
			    "reset found invalid extension %s", kext_name);
			free((char *)kext_name);
		    }
		}
		CFArrayAppendValue(aRepository->badKexts, newKext);
	    }
	    CFRelease(newKext);
            result = kKXKextManagerErrorNone;
        }

       /* don't use the cache-write result as the result of this function
        */
        if (aRepository->useCache) {
            KXKextRepositoryWriteCache(aRepository, NULL);
        }

    }

    aRepository->hasAuthenticated = false;

finish:
    aRepository->needsReset = false;
    if (kextURLs) CFRelease(kextURLs);
    return result;
}
/*******************************************************************************
*
*******************************************************************************/
void KXKextRepositoryAuthenticateKexts(KXKextRepositoryRef aRepository)
{
    if (KXKextManagerGetLogLevel(aRepository->manager) >=
            kKXKextManagerLogLevelDetails) {

        const char * repository_name =
            _KXKextRepositoryCopyCanonicalPathnameAsCString(aRepository);
        if (repository_name) {
            _KXKextManagerLogMessage(aRepository->manager,
                "authenticating extensions in repository %s",
                repository_name);
            free((char *)repository_name);
        }
    }

    KXKextManagerDisableClearRelationships(aRepository->manager);

    __KXKextRepositoryAuthenticateKextArray(aRepository,
         aRepository->candidateKexts, aRepository->badKexts);

    if (KXKextManagerPerformsFullTests(aRepository->manager)) {
         __KXKextRepositoryAuthenticateKextArray(aRepository,
             aRepository->badKexts, NULL);
    }

    KXKextManagerClearRelationships(aRepository->manager);

    KXKextManagerEnableClearRelationships(aRepository->manager);

    aRepository->hasAuthenticated = true;
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextRepositoryCheckIntegrityOfKexts(KXKextRepositoryRef aRepository,
    CFMutableArrayRef bomArray)
{
    if (KXKextManagerGetLogLevel(aRepository->manager) >=
            kKXKextManagerLogLevelDetails) {

        const char * repository_name =
            _KXKextRepositoryCopyCanonicalPathnameAsCString(aRepository);
        if (repository_name) {
            _KXKextManagerLogMessage(aRepository->manager,
                "checking integrity of extensions in repository %s",
                repository_name);
            free((char *)repository_name);
        }
    }

    KXKextManagerDisableClearRelationships(aRepository->manager);

    __KXKextRepositoryCheckIntegrityOfKextArray(aRepository,
         aRepository->candidateKexts, aRepository->badKexts, bomArray);

    if (KXKextManagerPerformsFullTests(aRepository->manager)) {
         __KXKextRepositoryCheckIntegrityOfKextArray(aRepository,
             aRepository->badKexts, NULL, bomArray);
    }

    KXKextManagerClearRelationships(aRepository->manager);

    KXKextManagerEnableClearRelationships(aRepository->manager);

    return;
}


/*******************************************************************************
*
*******************************************************************************/
void KXKextRepositoryMarkKextsAuthentic(KXKextRepositoryRef aRepository)
{
    CFIndex count, i;

    if (KXKextManagerGetLogLevel(aRepository->manager) >= kKXKextManagerLogLevelDetails) {
        const char * repository_name =
            _KXKextRepositoryCopyCanonicalPathnameAsCString(aRepository);
        if (repository_name) {
            _KXKextManagerLogMessage(aRepository->manager,
                "marking extensions authentic in repository %s",
                repository_name);
            free((char *)repository_name);
        }
    }

    KXKextManagerDisableClearRelationships(aRepository->manager);

    count = CFArrayGetCount(aRepository->candidateKexts);
    for (i = 0; i < count; i++) {
        KXKextRef thisKext = (KXKextRef)
            CFArrayGetValueAtIndex(aRepository->candidateKexts, i);
        KXKextMarkAuthentic(thisKext);
    }

    count = CFArrayGetCount(aRepository->badKexts);
    for (i = 0; i < count; i++) {
        KXKextRef thisKext = (KXKextRef)
            CFArrayGetValueAtIndex(aRepository->badKexts, i);
        KXKextMarkAuthentic(thisKext);
    }

    KXKextManagerClearRelationships(aRepository->manager);

    KXKextManagerEnableClearRelationships(aRepository->manager);

    aRepository->hasAuthenticated = true;

    return;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextRepositoryHasAuthenticated(KXKextRepositoryRef aRepository)
{
    return aRepository->hasAuthenticated;
}

/*******************************************************************************
*
*******************************************************************************/
CFArrayRef KXKextRepositoryCopyCandidateKexts(KXKextRepositoryRef aRepository)
{
    return CFArrayCreateCopy(kCFAllocatorDefault, aRepository->candidateKexts);
}

/*******************************************************************************
*
*******************************************************************************/
CFArrayRef KXKextRepositoryCopyBadKexts(KXKextRepositoryRef aRepository)
{
    return CFArrayCreateCopy(kCFAllocatorDefault, aRepository->badKexts);
}

/*******************************************************************************
*
*******************************************************************************/
KXKextRef KXKextRepositoryGetKextWithURL(
    KXKextRepositoryRef aRepository, CFURLRef anURL)
{
    KXKextRef foundKext = NULL; // don't release
    CFURLRef  absURL = NULL;    // must release
    CFIndex   count, i;
    KXKextRef thisKext = NULL;  // don't release
    CFURLRef  kextURL = NULL;   // don't release

    absURL = PATH_CopyCanonicalizedURL(anURL);
    if (!absURL) {
        goto finish;
    }

    count = CFArrayGetCount(aRepository->candidateKexts);
    for (i = 0; i < count; i++) {
        thisKext = (KXKextRef)CFArrayGetValueAtIndex(
            aRepository->candidateKexts, i);
        kextURL = KXKextGetAbsoluteURL(thisKext);
        if (CFEqual(kextURL, absURL)) {
            foundKext = thisKext;
            goto finish;
        }
    }

    count = CFArrayGetCount(aRepository->badKexts);
    for (i = 0; i < count; i++) {
        thisKext = (KXKextRef)CFArrayGetValueAtIndex(
            aRepository->badKexts, i);
        kextURL = KXKextGetAbsoluteURL(thisKext);
        if (CFEqual(kextURL, absURL)) {
            foundKext = thisKext;
            goto finish;
        }
    }

finish:

    if (absURL) CFRelease(absURL);

    return foundKext;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextRef KXKextRepositoryGetKextWithBundlePathInRepository(
    KXKextRepositoryRef aRepository, CFStringRef bundlePathInRepository)
{
    CFArrayRef thisArray;
    CFIndex i, count;

    thisArray = aRepository->candidateKexts;
    count = CFArrayGetCount(thisArray);
    for (i = 0; i < count; i++) {
        KXKextRef thisKext = (KXKextRef)CFArrayGetValueAtIndex(thisArray, i);
        CFStringRef kextBundlePathInRepository =
            KXKextGetBundlePathInRepository(thisKext);

        if (kCFCompareEqualTo == CFStringCompare(bundlePathInRepository,
                kextBundlePathInRepository, 0)) {

            return thisKext;
        }
    }

    return NULL;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError KXKextRepositoryWriteCache(
    KXKextRepositoryRef aRepository,
    CFURLRef anURL)
{
    KXKextManagerError result = kKXKextManagerErrorNone;

    CFURLRef repositoryURL   = NULL;  // must release
    CFURLRef cacheDirURL     = NULL;  // must release
    CFURLRef createdCacheURL = NULL;  // must release

    CFURLRef cacheURL     = NULL;     // don't release
    CFStringRef cachePath = NULL;     // must release

    char * repository_path = NULL;    // must free
    char * cache_dir_path  = NULL;    // must free
    char * cache_path      = NULL;    // must free

    CFDictionaryRef cacheDictionary = NULL;  // must release
    CFWriteStreamRef plistStream = NULL;     // must release
    CFDataRef cacheData = NULL;              // must release
    CFIndex cacheDataLength = 0;
    const UInt8 * cache_data = NULL;         // don't free
    int    fd;
    mode_t real_umask;
    gzFile outputGZFile = NULL;              // must gzclose
    CFIndex bytes_written = 0;
    char * output_filename = NULL;           // don't free

    struct stat cache_dir_stat;
    char temp_file[MAXPATHLEN];

   /*****
    * If given an URL to write to, use that; otherwise use the
    * repository's own path plus the standard relative cache file name.
    */
    if (anURL) {
        cacheURL = anURL;
    } else {
        repositoryURL = KXKextRepositoryCopyURL(aRepository);
        if (!repositoryURL) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }

        cacheDirURL = CFURLCreateCopyAppendingPathComponent(
            kCFAllocatorDefault, repositoryURL,
            CFSTR(_kKXKextCachesFolderName),
            true /* isDirectory */);
        if (!cacheDirURL) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }

        cache_dir_path = PATH_CanonicalizedCStringForURL(cacheDirURL);
        if (!cache_dir_path) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }

        if (stat(cache_dir_path, &cache_dir_stat) != 0) {
            if (errno != ENOENT) {
                _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
                    "%s - %s", cache_dir_path, strerror(errno));
                result = kKXKextManagerErrorFileAccess;
                goto finish;
            } else {
                if (mkdir(cache_dir_path, 0755) == -1) {
                    _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
                        "%s - %s", cache_dir_path, strerror(errno));
                    result = kKXKextManagerErrorFileAccess;
                    goto finish;
                }
            }
        }

        createdCacheURL = CFURLCreateCopyAppendingPathComponent(
            kCFAllocatorDefault, cacheDirURL,
            CFSTR(_kKXKextPlistCacheFilename),
            false /* isDirectory */);
        if (!createdCacheURL) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }

        cacheURL = createdCacheURL;
    }

    cacheDictionary = _KXKextRepositoryCopyCacheDictionary(aRepository);
    if (!cacheDictionary) {
        _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
            "cannot create kext cache data");
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    cache_path = PATH_CanonicalizedCStringForURL(cacheURL);
    if (!cache_path) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    if (strlcpy(temp_file, cache_path, sizeof(temp_file)) >= sizeof(temp_file)) {
        _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
            "%s - cache file name too long",
            temp_file);
        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }
    if (strlcat(temp_file, ".XXXX", sizeof(temp_file)) >= sizeof(temp_file)) {
        _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
            "%s - cache file name too long",
            temp_file);
        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }
    fd = mkstemp(temp_file);
    if (-1 == fd) {
        _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
            "%s - %s", temp_file, strerror(errno));
        result = kKXKextManagerErrorFileAccess;
        goto finish;
    }

    output_filename = temp_file;

   /* Set the umask to get it, then set it back to iself. Wish there were a
    * better way to query it.
    */
    real_umask = umask(0);
    umask(real_umask);

    if (-1 == fchmod(fd, CACHE_PERMS & ~real_umask)) {
        _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
            "%s - %s", temp_file, strerror(errno));
        result = kKXKextManagerErrorFileAccess;
        goto finish;
    }

    plistStream = CFWriteStreamCreateWithAllocatedBuffers(
        kCFAllocatorDefault, kCFAllocatorDefault);
    if (!plistStream) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }
    CFWriteStreamOpen(plistStream);
    CFPropertyListWriteToStream(cacheDictionary, plistStream,
        kCFPropertyListBinaryFormat_v1_0, NULL);
    CFWriteStreamClose(plistStream);

    cacheData = CFWriteStreamCopyProperty(plistStream,
        kCFStreamPropertyDataWritten);
    if (!cacheData) {
        _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
            "%s - data error", temp_file);
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }

    cacheDataLength = CFDataGetLength(cacheData);
    cache_data = CFDataGetBytePtr(cacheData);
    if (!cache_data) {
        _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
            "unable to get data to create cache file %s",
            temp_file);
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }

    outputGZFile = gzdopen(fd, "w");
    if (!outputGZFile) {
        _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
            "%s - gzopen() failed", temp_file);
        output_filename = NULL;
        if (errno == 0) {
            result = kKXKextManagerErrorNoMemory;
        } else {
            result = kKXKextManagerErrorFileAccess;
        }
        goto finish;
    }

    bytes_written = gzwrite(outputGZFile, (void *)cache_data, cacheDataLength);
    if (bytes_written != cacheDataLength) {
        _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
            "%s - gzwrite() failed", temp_file);
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }

    if (gzclose(outputGZFile) != Z_OK) {
        _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
            "%s - gzclose() failed", temp_file);
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }
    outputGZFile = NULL;

    // move it to the final destination
    if (-1 == rename(output_filename, cache_path)) {
        _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
            "%s - %s", cache_path, strerror(errno));
        result = kKXKextManagerErrorFileAccess;
        goto finish;
    }
    output_filename = NULL;

    // give the cache file the mod time of the repository when scanned
    if (aRepository->scanTimeValid) {
        struct timeval cacheFileTimes[2];
        cacheFileTimes[0].tv_sec  = aRepository->scanTime + 1;
        cacheFileTimes[0].tv_usec = 0;
        cacheFileTimes[1].tv_sec  = aRepository->scanTime + 1;
        cacheFileTimes[1].tv_usec = 0;
        if(-1 == utimes(cache_path, cacheFileTimes)) {
            _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
                "%s - %s", cache_path, strerror(errno));
            result = kKXKextManagerErrorFileAccess;
            goto finish;
        }
    }

finish:

    if (outputGZFile) gzclose(outputGZFile);
    if (output_filename) {
        if (-1 == unlink(output_filename)) {
            _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
                "%s - %s", output_filename, strerror(errno));
        }
    }

    if (repositoryURL)    CFRelease(repositoryURL);
    if (cacheDirURL)      CFRelease(cacheDirURL);

    if (cachePath)        CFRelease(cachePath);
    if (createdCacheURL)  CFRelease(createdCacheURL);
    if (cacheDictionary)  CFRelease(cacheDictionary);
    if (cacheData)        CFRelease(cacheData);

    if (repository_path)  free(repository_path);
    if (cache_dir_path)   free(cache_dir_path);
    if (cache_path)       free(cache_path);

    if (plistStream)      CFRelease(plistStream);

    return result;
}

/*******************************************************************************
********************************************************************************
* FRAMEWORK-PRIVATE API BELOW HERE
********************************************************************************
*******************************************************************************/

/*******************************************************************************
*
*******************************************************************************/
KXKextRepositoryRef _KXKextRepositoryCreate(CFAllocatorRef alloc)
{
    __KXKextRepositoryRef newRepository = NULL;

    newRepository = __KXKextRepositoryCreatePrivate(alloc, NULL);
    if (!newRepository) {
        goto finish;
    }

finish:
    return (KXKextRepositoryRef)newRepository;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError _KXKextRepositoryInitWithDirectory(
    KXKextRepositoryRef aRepository,
    CFURLRef aDirectory,
    Boolean scanDirectory,  // sets the scansForKexts member!
    KXKextManagerRef aManager)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    CFURLRef absURL = NULL;  // must release

    if (!aManager) {
        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }

    aRepository->manager = aManager;  // do not retain up pointer!

    absURL = PATH_CopyCanonicalizedURL(aDirectory);
    if (absURL != NULL) {
        aRepository->repositoryPath = CFURLCopyFileSystemPath(absURL,
            kCFURLPOSIXPathStyle);
    } else {
        aRepository->repositoryPath = NULL;
        goto finish;
    }
    if (!aRepository->repositoryPath) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    aRepository->useCache = false;
    aRepository->scansForKexts = scanDirectory;

    // FIXME: Check for existence of directory!!!

    if (!CFURLHasDirectoryPath(absURL)) {
        result = kKXKextManagerErrorNotADirectory;
        goto finish;
    }

    if (!aManager) {
        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }

    aRepository->hasAuthenticated = false;

    aRepository->candidateKexts = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
    if (!aRepository->candidateKexts) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    aRepository->badKexts = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
    if (!aRepository->badKexts) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    if (aRepository->scansForKexts) {
        result = KXKextRepositoryScan(aRepository);
    }

finish:
    if (absURL) CFRelease(absURL);
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError _KXKextRepositoryInitWithCache(
    KXKextRepositoryRef aRepository,
    CFDictionaryRef     aDictionary,
    CFURLRef            aDirectory,
    KXKextManagerRef    aManager)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    CFNumberRef cacheVersion = NULL;        // don't release
    long int cache_version = 0;
    // CFStringRef repositoryPath = NULL;      // don't release
    CFURLRef absURL = NULL;                 // must release
    CFBooleanRef scansForKexts = NULL;  // don't release
    CFArrayRef kexts = NULL;                // don't release
    CFIndex count, i;
    Boolean cacheInconsistencyFound = false;

    if (!aManager) {
        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }

    aRepository->manager = aManager;  // do not retain up pointer!

    aRepository->hasAuthenticated = false;
    aRepository->useCache = true;

   /*****
    * Make sure we can parse the version of the cache handed to us.
    */
    cacheVersion = (CFNumberRef)CFDictionaryGetValue(aDictionary,
        _CACHE_VERSION_KEY);
    if (!cacheVersion || CFGetTypeID(cacheVersion) != CFNumberGetTypeID()) {
        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }

    if (!CFNumberGetValue(cacheVersion, kCFNumberLongType, &cache_version)) {
        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }

    if (cache_version > _kKXKextRepositoryCacheVersion) {
        _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
            "cannot read cache version %ld", cache_version);
        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }

    absURL = PATH_CopyCanonicalizedURL(aDirectory);
    aRepository->repositoryPath = CFURLCopyFileSystemPath(absURL,
        kCFURLPOSIXPathStyle);
    if (!aRepository->repositoryPath) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    scansForKexts = (CFBooleanRef)CFDictionaryGetValue(aDictionary,
        _CACHE_SCANS_KEY);
    if (!scansForKexts ||
        CFGetTypeID(scansForKexts) != CFBooleanGetTypeID()) {

        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }

    aRepository->scansForKexts = CFBooleanGetValue(scansForKexts) ?
        true : false;

    // FIXME: Check for existence of directory!!!

    aRepository->candidateKexts = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
    if (!aRepository->candidateKexts) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    aRepository->badKexts = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
    if (!aRepository->badKexts) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    kexts = (CFArrayRef)CFDictionaryGetValue(aDictionary,
        _CACHE_KEXTS_KEY);
    if (!kexts || CFGetTypeID(kexts) != CFArrayGetTypeID()) {

        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }

    count = CFArrayGetCount(kexts);
    for (i = 0; i < count; i++) {
        CFDictionaryRef kDict = (CFDictionaryRef)CFArrayGetValueAtIndex(
            kexts, i);
        KXKextRef newKext = NULL;  // must release
        KXKextManagerError kextResult = kKXKextManagerErrorNone;
        CFURLRef kextURL = NULL;  // must release
        char * kext_path = NULL;  // must free

        newKext = _KXKextCreate(kCFAllocatorDefault);
        if (!newKext) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }
        kextResult = _KXKextInitWithCacheDictionaryInRepository(newKext, kDict,
            aRepository);

        kextURL = KXKextGetAbsoluteURL(newKext);  // don't release
        kext_path = NULL;  // must free
        if (kextURL) { 
            kext_path = PATH_CanonicalizedCStringForURL(kextURL);
        }

        if (kextResult == kKXKextManagerErrorNoMemory) {
            result = kextResult;
            goto finish;
        } else if (kextResult == kKXKextManagerErrorFileAccess ||
            kextResult == kKXKextManagerErrorNotADirectory ||
            kextResult == kKXKextManagerErrorKextNotFound ||
            kextResult == kKXKextManagerErrorURLNotInRepository ||
            kextResult == kKXKextManagerErrorNotABundle ||
            kextResult == kKXKextManagerErrorNotAKext) {

            // kext is completely unusable, do not store in repository
            if (KXKextManagerGetLogLevel(aRepository->manager) >=
                kKXKextManagerLogLevelDetails) {

                _KXKextManagerLogMessage(aRepository->manager,
                    "%s is not a kernel extension or is inaccessible",
                    kext_path ? kext_path : "(unknown)");
            }

        } else if (kextResult != kKXKextManagerErrorNone) {

            _KXKextRepositoryAddBadKext(aRepository, newKext);
            if (kextResult == kKXKextManagerErrorCache) {
                cacheInconsistencyFound = true;
            }
            if (KXKextManagerGetLogLevel(aRepository->manager) >=
                kKXKextManagerLogLevelDetails) {

                if (kextResult == kKXKextManagerErrorCache) {
                    _KXKextManagerLogMessage(aRepository->manager,
                        "added cache-inconsistent kernel extension %s",
                        kext_path ? kext_path : "(unknown)");
                } else {
                    _KXKextManagerLogMessage(aRepository->manager,
                        "added invalid cached kernel extension %s",
                        kext_path ? kext_path : "(unknown)");
                }
            }
        } else {
            _KXKextRepositoryAddKext(aRepository, newKext);
            if (KXKextManagerGetLogLevel(aRepository->manager) >=
                kKXKextManagerLogLevelDetails) {

                _KXKextManagerLogMessage(aRepository->manager,
                    "added cached kernel extension %s",
                    kext_path ? kext_path : "(unknown)");
            }
        }
        CFRelease(newKext);
        newKext = NULL;
        if (kext_path) free(kext_path);
    }

    if (cacheInconsistencyFound) {
        char repository_path_buffer[MAXPATHLEN+1];
        char * repository_path = NULL;

        if (CFStringGetCString(aRepository->repositoryPath, repository_path_buffer,
            sizeof(repository_path_buffer), kCFStringEncodingUTF8)) {
            repository_path = repository_path_buffer;
        }
        _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
            "repository cache problem found; scanning %s directly",
            repository_path ? repository_path : "(unknown)");
        KXKextRepositoryReset(aRepository);

    } else if (KXKextManagerWillUpdateCatalog(aManager)) {
        CFStringRef string = (CFStringRef) CFDictionaryGetValue(aDictionary, _CACHE_PERSONALITIES_KEY);
        if (string) {
            aRepository->personalityCache = CFStringCreateExternalRepresentation(kCFAllocatorDefault, 
                    string, kCFStringEncodingUTF8, 0);
        }
    }

finish:
    if (absURL)     CFRelease(absURL);
    return result;
}
/*******************************************************************************
*
*******************************************************************************/

KXKextManagerError KXKextRepositorySendCatalogFromCache(
    KXKextRepositoryRef aRepository,
    CFMutableDictionaryRef candidateKexts)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    kern_return_t kern_result = KERN_SUCCESS;
    CFIndex count, i, len = 0;
    void * ptr;

    if (!aRepository->personalityCache) return kKXKextManagerErrorKextNotFound;

    count = CFArrayGetCount(aRepository->candidateKexts);
    for (i = 0; i < count; i++) {
        KXKextRef thisKext =
            (KXKextRef) CFArrayGetValueAtIndex(aRepository->candidateKexts, i);
        if (thisKext != CFDictionaryGetValue(candidateKexts, KXKextGetBundleIdentifier(thisKext))) {
            result = kKXKextManagerErrorCache;
            goto finish;
        }
    }

    // all the repository plists are candidates, so remove them all & send in bulk
    for (i = 0; i < count; i++) {
        KXKextRef thisKext =
            (KXKextRef) CFArrayGetValueAtIndex(aRepository->candidateKexts, i);
        CFDictionaryRemoveValue(candidateKexts, KXKextGetBundleIdentifier(thisKext));
    }

    len = CFDataGetLength(aRepository->personalityCache);
    ptr = (void *)CFDataGetBytePtr(aRepository->personalityCache);
    kern_result = IOCatalogueSendData(kIOMasterPortDefault, kIOCatalogAddDrivers,
        ptr, len);

    // FIXME: check specific kernel error result for permission or whatever
    if (kern_result != KERN_SUCCESS) {
        _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository), "couldn't send personalities to catalog");
        result = kKXKextManagerErrorKernelError;
    }

finish:
    CFRelease(aRepository->personalityCache);
    aRepository->personalityCache = 0;

    return (result);
}

/*******************************************************************************
*
*******************************************************************************/
const char * _KXKextRepositoryCopyCanonicalPathnameAsCString(
    KXKextRepositoryRef aRepository)
{
    char * abs_path = NULL;      // returned
    CFStringRef absPath = NULL;  // don't release
    CFIndex pathSize;
    Boolean error = false;

    absPath = KXKextRepositoryGetPath(aRepository);
    if (!absPath) {
        goto finish;
    }

    pathSize = 1 + CFStringGetMaximumSizeOfFileSystemRepresentation(absPath);
    abs_path = (char *)malloc(pathSize * sizeof(char));
    if (!abs_path) {
        goto finish;
    }

    if (!CFStringGetFileSystemRepresentation(absPath, abs_path, pathSize)) {
        error = true;
    }

finish:
    if (error && abs_path) {
        free(abs_path);
        abs_path = NULL;
    }
    return abs_path;
}

/*******************************************************************************
*
*******************************************************************************/
CFArrayRef _KXKextRepositoryGetCandidateKexts(KXKextRepositoryRef aRepository)
{
    return aRepository->candidateKexts;
}

/*******************************************************************************
*
*******************************************************************************/
CFArrayRef _KXKextRepositoryGetBadKexts(KXKextRepositoryRef aRepository)
{
    return aRepository->badKexts;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError _KXKextRepositoryScanDirectoryForKexts(
    KXKextRepositoryRef aRepository,
    CFURLRef     aDirectory,
    CFArrayRef   existingKexts,
    CFArrayRef * addedKexts,
    CFArrayRef * badKexts,
    CFArrayRef * removedKexts)
{
    KXKextManagerError result = kKXKextManagerErrorNone;

    CFMutableArrayRef addedKextArray = NULL; // returned
    CFMutableArrayRef removedKextArray = NULL; // returned
    CFMutableArrayRef notKextArray = NULL;     // returned

    CFURLRef    canonicalURL = NULL;    // must release
    CFStringRef directoryPath = NULL;   // must release
    CFStringRef dirRepositoryPath = NULL; // must release
    CFIndex     repositoryPathLength;

    CFMutableDictionaryRef existingKextDict = NULL; // must release
    CFStringRef fullPath = NULL;  // must release
    CFBooleanRef directoryExists = NULL;  // must release
    CFArrayRef immDirectoryContents = NULL;  // must release
    CFMutableArrayRef directoryContents = NULL;  // must release

    CFIndex numKexts;
    CFIndex i;
    SInt32  urlError;
    CFIndex directoryContentsCount;

    if (KXKextManagerGetLogLevel(aRepository->manager) >= kKXKextManagerLogLevelKexts) {
        const char * directory_name =
            PATH_CanonicalizedCStringForURL(aDirectory);
        if (directory_name) {
            _KXKextManagerLogMessage(aRepository->manager,
                "scanning directory %s",
                directory_name);
            free((char *)directory_name);
        }
    }

    canonicalURL = PATH_CopyCanonicalizedURL(aDirectory);
    if (!canonicalURL) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    directoryPath = CFURLCopyFileSystemPath(canonicalURL,
        kCFURLPOSIXPathStyle);
    if (!directoryPath) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    repositoryPathLength = CFStringGetLength(aRepository->repositoryPath);
    dirRepositoryPath = CFStringCreateWithSubstring(kCFAllocatorDefault,
        directoryPath, CFRangeMake(0, repositoryPathLength));
    if (!dirRepositoryPath) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    if (CFStringCompare(aRepository->repositoryPath, dirRepositoryPath, 0) !=
        kCFCompareEqualTo) {

        result = kKXKextManagerErrorURLNotInRepository;
        goto finish;
    }

    if (addedKexts) {
        addedKextArray = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
        if (!addedKextArray) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }
        *addedKexts = addedKextArray;
    }

    if (removedKexts) {
        removedKextArray = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
        if (!removedKextArray) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }
        *removedKexts = removedKextArray;
    }

    if (badKexts) {
        notKextArray = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
        if (!notKextArray) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }
        *badKexts = notKextArray;
    }

   /*****
    * Build dictionary of existing kexts based on path in repository.
    */
    existingKextDict = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (!existingKextDict) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    if (existingKexts) {
        numKexts = CFArrayGetCount(existingKexts);
        for (i = 0; i < numKexts; i++) {
            KXKextRef thisKext =
                (KXKextRef)CFArrayGetValueAtIndex(existingKexts, i);
            CFStringRef kextPathInRepository =
                KXKextGetBundlePathInRepository(thisKext);

            fullPath = CFStringCreateWithFormat(kCFAllocatorDefault,
                NULL, CFSTR("%@/%@"), aRepository->repositoryPath,
                kextPathInRepository);
            if (!fullPath) {
                result = kKXKextManagerErrorNoMemory;
                goto finish;
            }
            CFDictionarySetValue(existingKextDict,
                fullPath,
                thisKext);

            CFRelease(fullPath);
            fullPath = NULL;
        }
    }

    // get all URLs in directory
    directoryExists = CFURLCreatePropertyFromResource(
        kCFAllocatorDefault, aDirectory,
        kCFURLFileExists, &urlError);

    if (!directoryExists) {
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    } else {
        if (!CFBooleanGetValue(directoryExists)) {
            result = kKXKextManagerErrorNotADirectory;
            goto finish;
        }
    }

    immDirectoryContents = CFURLCreatePropertyFromResource(
        kCFAllocatorDefault, aDirectory,
        kCFURLFileDirectoryContents, &urlError);

    if (!immDirectoryContents) {
        if (urlError == kCFURLResourceNotFoundError) {
            result = kKXKextManagerErrorFileAccess;
        } else {
            result = kKXKextManagerErrorUnspecified;
        }
        goto finish;
    }

    directoryContents = CFArrayCreateMutableCopy(kCFAllocatorDefault,
        0, immDirectoryContents);
    if (!immDirectoryContents) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    // cull non-kext URLs
    directoryContentsCount = CFArrayGetCount(directoryContents);
    i = directoryContentsCount;
    while (1) {

        CFURLRef  thisURL = NULL;         // don't release
        CFStringRef urlExtension = NULL;  // must release
        CFStringRef bundleName = NULL;    // must release

        if (i == 0) break;
        i--;

        thisURL = CFArrayGetValueAtIndex(directoryContents, i);

       /* Drop any URL that doesn't have a ".kext" extension.
        */
        urlExtension = CFURLCopyPathExtension(thisURL);
        bundleName = CFURLCopyLastPathComponent(thisURL);

        if (!urlExtension ||
            CFStringCompare(CFSTR("kext"), urlExtension, 0) !=
            kCFCompareEqualTo) {

            CFArrayRemoveValueAtIndex(directoryContents, i);
        }


       /* Drop any URL that starts with "." (including Finder
        * metadata files, which start with "._.").
        */

        else if (!bundleName ||
            CFStringHasPrefix(bundleName, CFSTR("."))) {

            CFArrayRemoveValueAtIndex(directoryContents, i);
        }

        if (urlExtension) CFRelease(urlExtension);
        if (bundleName) CFRelease(bundleName);
    }


    // find all added kexts (easy) and remove from array of URLs
    directoryContentsCount = CFArrayGetCount(directoryContents);
    for (i = 0; i < directoryContentsCount; i++) {

        CFURLRef  thisURL;
        KXKextRef thisKext;  // must release

        thisURL = CFArrayGetValueAtIndex(directoryContents, i);
        fullPath = CFURLCopyFileSystemPath(thisURL,
            kCFURLPOSIXPathStyle);
        if (!fullPath) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }

       /* Do I already know about this kext URL? If so, drop to the
        * bottom of the loop. If not, try to create a kext for the
        * URL and record it in the appropriate array based on the
        * initialization result.
        */
        if (CFDictionaryGetValue(existingKextDict, fullPath)) {
            /* Do nothing, fall though to bottom of loop. */
        } else if (addedKextArray || notKextArray) {

            thisKext = _KXKextCreate(kCFAllocatorDefault);
            if (!thisKext) {
                result = kKXKextManagerErrorNoMemory;
                goto finish;
            }

            result = _KXKextInitWithURLInRepository(thisKext, thisURL,
                aRepository);
            if (result == kKXKextManagerErrorNoMemory ||
                result == kKXKextManagerErrorFileAccess ||
                result == kKXKextManagerErrorNotADirectory ||
                result == kKXKextManagerErrorKextNotFound ||
                result == kKXKextManagerErrorURLNotInRepository ||
                result == kKXKextManagerErrorNotABundle ||
                result == kKXKextManagerErrorNotAKext) {

                // kext is completely unusable, do not store in repository

            } else if (result == kKXKextManagerErrorNone) {
                if (KXKextManagerGetLogLevel(aRepository->manager) >=
                     kKXKextManagerLogLevelKexts) {
                    const char * kext_name =
                        _KXKextCopyBundlePathInRepositoryAsCString(thisKext);
                    if (kext_name) {
                        _KXKextManagerLogMessage(aRepository->manager,
                            "found valid extension %s",
                            kext_name);
                        free((char *)kext_name);
                    }
                }
                if (addedKextArray) {
                    CFArrayAppendValue(addedKextArray, thisKext);
                }

            } else {
                if (KXKextManagerGetLogLevel(aRepository->manager) >=
                     kKXKextManagerLogLevelKexts) {

                    const char * kext_name =
                        _KXKextCopyBundlePathInRepositoryAsCString(thisKext);
                    if (kext_name) {
                        _KXKextManagerLogMessage(aRepository->manager,
                            "found invalid extension %s",
                            kext_name);
                        free((char *)kext_name);
                    }
                }
                if (notKextArray) {
                    CFArrayAppendValue(notKextArray, thisKext);
                }
            }

	    CFRelease(thisKext);
            result = kKXKextManagerErrorNone;
        }

       /* Pull current kext entry from dictionary of previously existing kexts.
        */
        CFDictionaryRemoveValue(existingKextDict, fullPath);
        CFRelease(fullPath);
        fullPath = NULL;
    }

   /*****
    * Any remaining entries in existingKextDict were not found in the
    * directory and hence have been removed. Put the entries from that
    * dictionary into the removedKextArray.
    */
    if (removedKextArray) {
        void * dictValues = NULL;  // must free

        numKexts = CFDictionaryGetCount(existingKextDict);
        dictValues = malloc(numKexts * sizeof(void *));
        if (!dictValues) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }

        CFDictionaryGetKeysAndValues(existingKextDict, NULL,
            (const void **)&dictValues);
        CFArrayReplaceValues(removedKextArray,
            CFRangeMake(0, CFArrayGetCount(removedKextArray)),
            dictValues, numKexts);

        if (KXKextManagerGetLogLevel(aRepository->manager) >=
             kKXKextManagerLogLevelKexts) {

            for (i = 0; i < numKexts; i++) {
                KXKextRef rKext = ((KXKextRef *)(dictValues))[i];
                const char * kext_name =
                    _KXKextCopyBundlePathInRepositoryAsCString(rKext);
                if (kext_name) {
                    _KXKextManagerLogMessage(aRepository->manager,
                        "extension %s has been removed",
                        kext_name);
                    free((char *)kext_name);
                }
            }
        }

        free(dictValues);
    }

finish:

    if (fullPath)              CFRelease(fullPath);
    if (canonicalURL)          CFRelease(canonicalURL);
    if (directoryPath)         CFRelease(directoryPath);
    if (dirRepositoryPath)     CFRelease(dirRepositoryPath);
    if (directoryExists)       CFRelease(directoryExists);
    if (existingKextDict)      CFRelease(existingKextDict);
    if (immDirectoryContents)  CFRelease(immDirectoryContents);
    if (directoryContents)     CFRelease(directoryContents);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextRepositoryAddKext(KXKextRepositoryRef aRepository, KXKextRef aKext)
{
    CFArrayAppendValue(aRepository->candidateKexts, aKext);

    // FIXME: Might want to be able to disable this when adding
    // FIXME: ...a lot of kexts one at a time (as when using a cache)
    //
    _KXKextManagerClearLoadFailures(aRepository->manager);

    aRepository->hasAuthenticated = false;

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextRepositoryAddKexts(
    KXKextRepositoryRef aRepository, CFArrayRef kextArray)
{
    CFIndex count;

    _KXKextManagerClearLoadFailures(aRepository->manager);

    count = CFArrayGetCount(kextArray);

    CFArrayAppendArray(aRepository->candidateKexts, kextArray,
        CFRangeMake(0, count));

    aRepository->hasAuthenticated = false;

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextRepositoryAddBadKext(KXKextRepositoryRef aRepository, KXKextRef aKext)
{
    CFArrayAppendValue(aRepository->badKexts, aKext);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextRepositoryAddBadKexts(
    KXKextRepositoryRef aRepository, CFArrayRef kextArray)
{
    CFIndex count;

    count = CFArrayGetCount(kextArray);
    CFArrayAppendArray(aRepository->badKexts, kextArray,
        CFRangeMake(0, count));

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextRepositoryRemoveKext(KXKextRepositoryRef aRepository,
    KXKextRef aKext)
{

    CFArrayRef kextPlugins;
    CFIndex count, i;

    if (KXKextManagerGetLogLevel(aRepository->manager) >= kKXKextManagerLogLevelDetails) {
        const char * repository_name =
            _KXKextRepositoryCopyCanonicalPathnameAsCString(aRepository);
        const char * kext_name =
            _KXKextCopyBundlePathInRepositoryAsCString(aKext);
        if (kext_name && repository_name) {
            _KXKextManagerLogMessage(aRepository->manager,
                "repository %s removing extension %s",
                repository_name, kext_name);
        }
        if (kext_name) free((char *)kext_name);
        if (repository_name) free((char *)repository_name);
    }

    KXKextManagerDisableClearRelationships(aRepository->manager);

   /*****
    * First, remove any plugins of the kext being removed.
    */
    kextPlugins = KXKextGetPlugins(aKext);
    if (kextPlugins) {
        count = CFArrayGetCount(kextPlugins);
        for (i = 0; i < count; i++) {
            KXKextRef pluginKext;

            pluginKext = (KXKextRef)CFArrayGetValueAtIndex(kextPlugins, i);
            _KXKextRepositoryRemoveKext(aRepository, pluginKext);
        }
    }

   /*****
    * Now remove the kext itaRepository from the arrays of candidate and bad
    * kexts. It will only be in one or the other, but we have to check
    * both.
    */
    count = CFArrayGetCount(aRepository->candidateKexts);
    i = count;
    while (1) {
        KXKextRef thisKext = NULL;

        if (i == 0) break;
        i--;

        thisKext =
            (KXKextRef)CFArrayGetValueAtIndex(aRepository->candidateKexts, i);

        if (thisKext == aKext) {
            CFArrayRemoveValueAtIndex(aRepository->candidateKexts, i);
        }
    }

    count = CFArrayGetCount(aRepository->badKexts);
    i = count;
    while (1) {
        KXKextRef thisKext = NULL;

        if (i == 0) break;
        i--;

        thisKext =
            (KXKextRef)CFArrayGetValueAtIndex(aRepository->badKexts, i);
        if (thisKext == aKext) {
            CFArrayRemoveValueAtIndex(aRepository->badKexts, i);
        }
    }

    KXKextManagerClearRelationships(aRepository->manager);

    KXKextManagerEnableClearRelationships(aRepository->manager);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextRepositoryDisqualifyKext(KXKextRepositoryRef aRepository,
    KXKextRef aKext)
{
    CFIndex count, i;

    if (KXKextManagerGetLogLevel(aRepository->manager) >= kKXKextManagerLogLevelDetails) {
        const char * repository_name =
            _KXKextRepositoryCopyCanonicalPathnameAsCString(aRepository);
        const char * kext_name =
            _KXKextCopyBundlePathInRepositoryAsCString(aKext);
        if (kext_name && repository_name) {
            _KXKextManagerLogMessage(aRepository->manager,
                "repository %s disqualifying extension %s",
                repository_name, kext_name);
        }
        if (kext_name) free((char *)kext_name);
        if (repository_name) free((char *)repository_name);
    }
    KXKextManagerDisableClearRelationships(aRepository->manager);

    // FIXME: Do we disqualify a kext's plugins along with it?

   /*****
    * Now remove the kext from the array of candidate kexts
    * and put it in with the bad kexts.
    */
    count = CFArrayGetCount(aRepository->candidateKexts);
    i = count;
    while (1) {
        KXKextRef thisKext = NULL;

        if (i == 0) break;
        i--;

        thisKext =
            (KXKextRef)CFArrayGetValueAtIndex(aRepository->candidateKexts, i);

        if (thisKext == aKext) {
            CFArrayRemoveValueAtIndex(aRepository->candidateKexts, i);
            CFArrayAppendValue(aRepository->badKexts, thisKext);
        }
    }

    KXKextManagerClearRelationships(aRepository->manager);

    KXKextManagerEnableClearRelationships(aRepository->manager);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextRepositoryRequalifyKext(KXKextRepositoryRef aRepository,
    KXKextRef aKext)
{
    CFIndex count, i;

    if (KXKextManagerGetLogLevel(aRepository->manager) >= kKXKextManagerLogLevelDetails) {
        const char * repository_name =
            _KXKextRepositoryCopyCanonicalPathnameAsCString(aRepository);
        const char * kext_name =
            _KXKextCopyBundlePathInRepositoryAsCString(aKext);
        if (kext_name && repository_name) {
            _KXKextManagerLogMessage(aRepository->manager,
                "repository %s requalifying extension %s",
                repository_name, kext_name);
        }
        if (kext_name) free((char *)kext_name);
        if (repository_name) free((char *)repository_name);
    }
    KXKextManagerDisableClearRelationships(aRepository->manager);

   /*****
    * Now remove the kext from the array of bad kexts
    * and put it in with the candidate kexts.
    */
    count = CFArrayGetCount(aRepository->badKexts);
    i = count;
    while (1) {
        KXKextRef thisKext = NULL;

        if (i == 0) break;
        i--;

        thisKext =
            (KXKextRef)CFArrayGetValueAtIndex(aRepository->badKexts, i);

        if (thisKext == aKext) {
            CFArrayRemoveValueAtIndex(aRepository->badKexts, i);
            CFArrayAppendValue(aRepository->candidateKexts, thisKext);
            _KXKextSetHasBeenAuthenticated(thisKext, false);
        }
    }

    KXKextManagerClearRelationships(aRepository->manager);

    KXKextManagerEnableClearRelationships(aRepository->manager);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextRepositoryClearRelationships(KXKextRepositoryRef aRepository)
{
    CFIndex count, i;
    KXKextRef thisKext;

    count = CFArrayGetCount(aRepository->candidateKexts);
    for (i = 0; i < count; i++) {
        thisKext = (KXKextRef)CFArrayGetValueAtIndex(
            aRepository->candidateKexts, i);
        _KXKextClearVersionRelationships(thisKext);
    }

    count = CFArrayGetCount(aRepository->badKexts);
    for (i = 0; i < count; i++) {
        thisKext = (KXKextRef)CFArrayGetValueAtIndex(aRepository->badKexts, i);
        _KXKextClearVersionRelationships(thisKext);
    }

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextRepositoryClearDependencyRelationships(
    KXKextRepositoryRef aRepository)
{
    CFIndex count, i;
    KXKextRef thisKext;

    // Have each kext clear its dependencies
    count = CFArrayGetCount(aRepository->candidateKexts);
    for (i = 0; i < count; i++) {
        thisKext = (KXKextRef)CFArrayGetValueAtIndex(
            aRepository->candidateKexts, i);
        _KXKextClearDependencies(thisKext);
    }

    count = CFArrayGetCount(aRepository->badKexts);
    for (i = 0; i < count; i++) {
        thisKext = (KXKextRef)CFArrayGetValueAtIndex(aRepository->badKexts, i);
        _KXKextClearDependencies(thisKext);
    }

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextRepositoryMarkKextsNotLoaded(KXKextRepositoryRef aRepository)
{
    CFIndex count, i;
    KXKextRef thisKext;

    // Have each kext clear its dependencies
    count = CFArrayGetCount(aRepository->candidateKexts);
    for (i = 0; i < count; i++) {
        thisKext = (KXKextRef)CFArrayGetValueAtIndex(
            aRepository->candidateKexts, i);
        _KXKextSetIsLoaded(thisKext, false);
        _KXKextSetOtherVersionIsLoaded(thisKext, false);
    }

    count = CFArrayGetCount(aRepository->badKexts);
    for (i = 0; i < count; i++) {
        thisKext = (KXKextRef)CFArrayGetValueAtIndex(aRepository->badKexts, i);
        _KXKextSetIsLoaded(thisKext, false);
        _KXKextSetOtherVersionIsLoaded(thisKext, false);
    }

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextRepositoryClearLoadFailures(KXKextRepositoryRef aRepository)
{
    CFIndex count, i;
    KXKextRef thisKext;

    // Have each kext clear its dependencies
    count = CFArrayGetCount(aRepository->candidateKexts);
    for (i = 0; i < count; i++) {
        thisKext = (KXKextRef)CFArrayGetValueAtIndex(
            aRepository->candidateKexts, i);
        KXKextSetLoadFailed(thisKext, false);
    }

    count = CFArrayGetCount(aRepository->badKexts);
    for (i = 0; i < count; i++) {
        thisKext = (KXKextRef)CFArrayGetValueAtIndex(aRepository->badKexts, i);
        KXKextSetLoadFailed(thisKext, false);
    }

    return;
}

/*******************************************************************************
*
*******************************************************************************/
CFDictionaryRef _KXKextRepositoryCopyCacheDictionary(
    KXKextRepositoryRef aRepository)
{
    Boolean error = false;
    CFMutableDictionaryRef theDictionary = NULL; // returned
    CFMutableArrayRef thePersonalities = NULL;   // must release
    CFMutableArrayRef kexts = NULL;              // must release
    CFDataRef   cacheData = NULL;
    CFStringRef cacheString = NULL;
    long int cache_version = _kKXKextRepositoryCacheVersion;
    CFNumberRef cacheVersion = NULL;             // must release
    CFIndex count, i;

    theDictionary = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
    if (!theDictionary) {
        error = true;
        goto finish;
    }

    cacheVersion = CFNumberCreate(kCFAllocatorDefault,
        kCFNumberLongType, &cache_version);
    if (!cacheVersion) {
        error = true;
        goto finish;
    }
    CFDictionarySetValue(theDictionary, _CACHE_VERSION_KEY,
        cacheVersion);

    CFDictionarySetValue(theDictionary, _CACHE_SCANS_KEY,
        aRepository->scansForKexts ? kCFBooleanTrue : kCFBooleanFalse);


    kexts = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!kexts) {
        error = true;
        goto finish;
    }
    CFDictionarySetValue(theDictionary, _CACHE_KEXTS_KEY,
        kexts);
    // do not release plugins here

   /*****
    * Dump the candidate kexts into the kext array.
    */

    thePersonalities = CFArrayCreateMutable(kCFAllocatorDefault,
        0, &kCFTypeArrayCallBacks);
    if (!thePersonalities) {
        error = true;
        goto finish;
    }

    count = CFArrayGetCount(aRepository->candidateKexts);
    for (i = 0; i < count; i++) {
        KXKextRef kext = (KXKextRef)CFArrayGetValueAtIndex(
            aRepository->candidateKexts, i);
        CFDictionaryRef kDict = NULL;  // must release

        CFArrayRef personalities = KXKextCopyPersonalitiesArray(kext);
        if (personalities) {
            CFArrayAppendArray(thePersonalities, personalities,
                CFRangeMake(0, CFArrayGetCount(personalities)));
            CFRelease(personalities);
        }

       /* Plugins are added to the cache by their container.
        */
        if (KXKextIsAPlugin(kext)) {
            continue;
        }

        kDict = _KXKextCopyCacheDictionary(kext);
        if (!kDict) {
            error = true;
            goto finish;
        }
        CFArrayAppendValue(kexts, kDict);
        CFRelease(kDict); // clean up within loop
        kDict = NULL;
    }

   /*****
    * Dump the bad kexts into the kext array
    * (they might redeem themselves).
    */
    count = CFArrayGetCount(aRepository->badKexts);
    for (i = 0; i < count; i++) {
        KXKextRef kext = (KXKextRef)CFArrayGetValueAtIndex(
            aRepository->badKexts, i);
        CFDictionaryRef kDict = NULL;  // must release

       /* Plugins are added to the cache by their container.
        */
        if (KXKextIsAPlugin(kext)) {
            continue;
        }

        kDict = _KXKextCopyCacheDictionary(kext);
        if (!kDict) {
            error = true;
            goto finish;
        }
        CFArrayAppendValue(kexts, kDict);
        CFRelease(kDict); // clean up within loop
        kDict = NULL;
    }

    cacheData = IOCFSerialize(thePersonalities, kNilOptions);
    CFRelease(thePersonalities);
    thePersonalities = 0;
    if (cacheData) {
        cacheString = CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, 
                            cacheData, kCFStringEncodingUTF8);
        CFRelease(cacheData);
    }

    if (cacheString) {
        CFDictionarySetValue(theDictionary, _CACHE_PERSONALITIES_KEY, cacheString);
        CFRelease(cacheString);
    }

finish:
    if (thePersonalities) CFRelease(thePersonalities);
    if (cacheVersion)     CFRelease(cacheVersion);
    if (kexts)            CFRelease(kexts);

    if (error) {
        if (theDictionary) CFRelease(theDictionary);
        theDictionary = NULL;
    }

    return theDictionary;
}

/*******************************************************************************
* _KXKextRepositoryInvalidateCaches() just updates the mod time of the
* repository's directory, so that it is perforce newer than any cache file.
*******************************************************************************/
Boolean _KXKextRepositoryInvalidateCaches(
    KXKextRepositoryRef aRepository)
{
    Boolean result = true;
    char path[MAXPATHLEN+1];
    CFIndex pathSize = 1 + CFStringGetMaximumSizeOfFileSystemRepresentation(aRepository->repositoryPath);

    if (!CFStringGetFileSystemRepresentation(aRepository->repositoryPath, path, pathSize)) {

        result = false;
        goto finish;
    }

    if (utimes(path, NULL) != 0) {
        // FIXME: Should we print an error message here?
        result = false;
    }

finish:
    return result;
}

/*******************************************************************************
********************************************************************************
* MODULE-PRIVATE API BELOW HERE
********************************************************************************
*******************************************************************************/

/*******************************************************************************
*
*******************************************************************************/
static const CFRuntimeClass __KXKextRepositoryClass = {
    0,                       // version
    "KXKextRepository",                // className
    NULL,                    // init
    NULL,                    // copy
    __KXKextRepositoryReleaseContents, // finalize
    NULL,                    // equal
    NULL,                    // hash
    NULL,                    // copyFormattingDesc
    __KXKextRepositoryCopyDebugDescription  // copyDebugDesc
};

void __KXKextRepositoryInitialize(void)
{
    __kKXKextRepositoryTypeID = _CFRuntimeRegisterClass(&__KXKextRepositoryClass);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
static pthread_once_t initialized = PTHREAD_ONCE_INIT;

__KXKextRepositoryRef __KXKextRepositoryCreatePrivate(
    CFAllocatorRef       allocator,
    CFAllocatorContext * context __unused)
{
    __KXKextRepositoryRef newRepository = NULL;
    void * offset = NULL;
    UInt32           size;

    /* initialize runtime */
    pthread_once(&initialized, __KXKextRepositoryInitialize);

    /* allocate session */
    size  = sizeof(__KXKextRepository) - sizeof(CFRuntimeBase);
    newRepository = (__KXKextRepositoryRef)_CFRuntimeCreateInstance(allocator,
        __kKXKextRepositoryTypeID, size, NULL);
    if (!newRepository) {
        return NULL;
    }
    offset = newRepository;
    bzero(offset + sizeof(CFRuntimeBase), size);

    return (__KXKextRepositoryRef)newRepository;
}

/*******************************************************************************
*
*******************************************************************************/
static CFStringRef __KXKextRepositoryCopyDebugDescription(CFTypeRef cf)
{
    CFAllocatorRef     allocator = CFGetAllocator(cf);
    CFMutableStringRef result;

    result = CFStringCreateMutable(allocator, 0);
    CFStringAppendFormat(result, NULL, CFSTR("<KXKextRepository %p [%p]> {\n"), cf, allocator);
    // add useful stuff here
    CFStringAppendFormat(result, NULL, CFSTR("}"));

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
void __KXKextRepositoryReleaseContents(CFTypeRef cf)
{
    KXKextRepositoryRef aRepository = (KXKextRepositoryRef)cf;

    // manager is not retained
    if (aRepository->repositoryPath)   CFRelease(aRepository->repositoryPath);
    if (aRepository->candidateKexts)   CFRelease(aRepository->candidateKexts);
    if (aRepository->badKexts)         CFRelease(aRepository->badKexts);
    if (aRepository->personalityCache) CFRelease(aRepository->personalityCache);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError __KXKextRepositoryScanDirectory(
    KXKextRepositoryRef aRepository)
{
    KXKextManagerError result = kKXKextManagerErrorNone;

    CFURLRef repositoryURL = NULL;   // must release
    CFArrayRef addedKexts = NULL;    // must release
    CFArrayRef removedKexts = NULL;  // must release
    CFArrayRef badKexts = NULL;      // must release
    CFMutableArrayRef existingKexts = NULL;  // must release
    CFArrayRef addedPlugins = NULL;    // must release
    CFArrayRef badKextPlugins = NULL;  // must release
    CFArrayRef removedPlugins = NULL;  // must release
    char * dir_path = NULL;           // must free
    struct stat dir_stat;

    CFIndex outerCount, outerIndex;
    CFIndex innerCount, innerIndex;

    if (KXKextManagerGetLogLevel(aRepository->manager) >=
            kKXKextManagerLogLevelDetails) {

        const char * repository_name =
            _KXKextRepositoryCopyCanonicalPathnameAsCString(aRepository);
        if (repository_name) {
            _KXKextManagerLogMessage(aRepository->manager,
                "scanning repository %s",
                repository_name);
            free((char *)repository_name);
        }
    }

    KXKextManagerDisableClearRelationships(aRepository->manager);

    repositoryURL = KXKextRepositoryCopyURL(aRepository);
    if (!repositoryURL) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    dir_path = PATH_CanonicalizedCStringForURL(repositoryURL);
    if (!dir_path) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }
    
    aRepository->scanTimeValid = (stat(dir_path, &dir_stat) == 0);
    aRepository->scanTime = dir_stat.st_mtime;

    free(dir_path);

    existingKexts = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0,
            aRepository->candidateKexts);
    if (!existingKexts) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    CFArrayAppendArray(existingKexts, aRepository->badKexts,
        CFRangeMake(0, CFArrayGetCount(aRepository->badKexts)));

    result = _KXKextRepositoryScanDirectoryForKexts(
        aRepository,
        repositoryURL,
        existingKexts,
        aRepository->scansForKexts ? &addedKexts : NULL,
        aRepository->scansForKexts ? &badKexts : NULL,
        &removedKexts);

    if (result != kKXKextManagerErrorNone) {
        goto finish;
    }

    if (addedKexts) {
        _KXKextRepositoryAddKexts(aRepository, addedKexts);
    }
    if (badKexts) {
        _KXKextRepositoryAddBadKexts(aRepository, badKexts);
    }

   /*****
    * Check the removed kexts and drop them from the arrays
    * of candidate & bad kexts.
    */
    outerCount = CFArrayGetCount(removedKexts);
    for (outerIndex = 0; outerIndex < outerCount; outerIndex++) {
        KXKextRef rKext = (KXKextRef)CFArrayGetValueAtIndex(removedKexts,
            outerIndex);
        _KXKextRepositoryRemoveKext(aRepository, rKext);
    }

   /*****
    * Check every added kext for added/removed plugins.
    */
    if (CFArrayGetCount(addedKexts)) {
        outerCount = CFArrayGetCount(addedKexts);
        for (outerIndex = 0; outerIndex < outerCount; outerIndex++) {
            KXKextRef thisKext =
                (KXKextRef)CFArrayGetValueAtIndex(addedKexts, outerIndex);

            result = _KXKextScanPlugins(thisKext, &addedPlugins,
                &badKextPlugins, &removedPlugins);

            if (result != kKXKextManagerErrorNone) {
                goto finish;
            }

            _KXKextRepositoryAddKexts(aRepository, addedPlugins);
            _KXKextRepositoryAddBadKexts(aRepository, badKextPlugins);

           /*****
            * Check the removed kexts and drop them from the arrays
            * of candidate & bad kexts.
 */
            innerCount = CFArrayGetCount(removedPlugins);
            for (innerIndex = 0; innerIndex < innerCount; innerIndex++) {
                KXKextRef rKext =
                    (KXKextRef)CFArrayGetValueAtIndex(removedPlugins,
                    innerIndex);
                _KXKextRepositoryRemoveKext(aRepository, rKext);
            }

            if (addedPlugins) {
                CFRelease(addedPlugins);
                addedPlugins = NULL;
            }
            if (removedPlugins) {
                CFRelease(removedPlugins);
                removedPlugins = NULL;
            }
            if (badKextPlugins) {
                CFRelease(badKextPlugins);
                badKextPlugins = NULL;
            }
        }
    }

finish:
    if (repositoryURL)   CFRelease(repositoryURL);
    if (addedKexts)      CFRelease(addedKexts);
    if (removedKexts)    CFRelease(removedKexts);
    if (badKexts)        CFRelease(badKexts);
    if (existingKexts)   CFRelease(existingKexts);
    if (addedPlugins)    CFRelease(addedPlugins);
    if (removedPlugins)  CFRelease(removedPlugins);
    if (badKextPlugins)  CFRelease(badKextPlugins);

    KXKextManagerClearRelationships(aRepository->manager);

    KXKextManagerEnableClearRelationships(aRepository->manager);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
static void __KXKextRepositoryAuthenticateKextArray(
    __KXKextRepositoryRef aRepository __unused,
    CFMutableArrayRef kexts,
    CFMutableArrayRef badKexts)
{
    CFIndex numKexts;
    CFIndex i;

    numKexts = CFArrayGetCount(kexts);
    i = numKexts;
    while (1) {
        KXKextRef thisKext;
        KXKextManagerError kextResult;

        if (i == 0) break;
        i--;

        thisKext = (KXKextRef)CFArrayGetValueAtIndex(kexts, i);
        if (!KXKextHasBeenAuthenticated(thisKext)) {
            kextResult = KXKextAuthenticate(thisKext);

            if (kextResult == kKXKextManagerErrorNoMemory) {
                goto finish;  // no point continuing!
            }

            if (kextResult != kKXKextManagerErrorNone && badKexts) {
                CFArrayAppendValue(badKexts, thisKext);
                CFArrayRemoveValueAtIndex(kexts, i);
            }
        }
    }

finish:
    return;
}

static void __KXKextRepositoryCheckIntegrityOfKextArray(
    __KXKextRepositoryRef aRepository __unused,
    CFMutableArrayRef kexts,
    CFMutableArrayRef badKexts,
    CFMutableArrayRef bomArray)
{
    CFIndex numKexts;
    CFIndex i;

    numKexts = CFArrayGetCount(kexts);
    for (i = numKexts-1; i >= 0; i--) {
        KXKextRef thisKext;
        KXKextManagerError kextResult;

        thisKext = (KXKextRef)CFArrayGetValueAtIndex(kexts, i);
        kextResult = _KXKextCheckIntegrity(thisKext, bomArray);

        if (kextResult == kKXKextManagerErrorNoMemory) {
            goto finish;  // no point continuing!
        }

        if (kextResult != kKXKextManagerErrorNone && badKexts) {
            CFArrayAppendValue(badKexts, thisKext);
            CFArrayRemoveValueAtIndex(kexts, i);
        }
    }

finish:
    return;
}
#endif // !__LP64__
