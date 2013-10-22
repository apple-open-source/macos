/*
 * Copyright (c) 2006-2012 Apple Inc. All rights reserved.
 * 
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
/*
 * FILE: bootcaches.c
 * AUTH: Soren Spies (sspies)
 * DATE: "spring" 2006
 * DESC: routines for bootcache data
 *
 */

#include <bless.h>
#include <bootfiles.h>
#include <IOKit/IOKitLib.h>

#include <fcntl.h>
#include <libgen.h>
#include <notify.h>
#include <paths.h>
#include <mach/mach.h>
#include <mach/kmod.h>
#include <sys/attr.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <EFILogin/EFILogin.h>
#include <System/libkern/mkext.h>
#include <System/libkern/OSKextLibPrivate.h>
#include <DiskArbitration/DiskArbitration.h>        // for UUID fetching
#include <IOKit/kext/fat_util.h>
#include <IOKit/kext/macho_util.h>
#include <IOKit/storage/CoreStorage/CoreStorageUserLib.h>
#include <IOKit/storage/CoreStorage/CoreStorageCryptoIDs.h>
#include <IOKit/storage/CoreStorage/CSFullDiskEncryption.h>

// Kext Management pieces from IOKitUser
#include <IOKit/kext/OSKext.h>
#include <IOKit/kext/OSKextPrivate.h>

#include "bootcaches.h"         // includes CF
#include "bootroot_internal.h"  // kBRUpdateOpts_t
#include "fork_program.h"
#include "kext_tools_util.h"
#include "safecalls.h"

// only used here
#define kBRDiskArbMaxRetries   (10)

static void removeTrailingSlashes(char * path);

// XX These are used often together, rely on 'finish', and should be all-caps.
// 'dst must be char[PATH_MAX]'
// COMPILE_TIME_ASSERT fails for get_locres_info() due to char[size] ~ char*
#define pathcpy(dst, src) do { \
        /* COMPILE_TIME_ASSERT(sizeof(dst) == PATH_MAX); */ \
        if (strlcpy(dst, src, PATH_MAX) >= PATH_MAX)  goto finish; \
    } while(0)
#define pathcat(dst, src) do { \
        /* COMPILE_TIME_ASSERT(sizeof(dst) == PATH_MAX); */ \
        if (strlcat(dst, src, PATH_MAX) >= PATH_MAX)  goto finish; \
    } while(0)


// http://lists.freebsd.org/pipermail/freebsd-hackers/2004-February/005627.html
#define LOGERRxlate(ctx1, ctx2, errval) do { \
        char *c2cpy = ctx2, ctx[256]; \
        if (ctx2 != NULL) { \
            snprintf(ctx, sizeof(ctx), "%s: %s", ctx1, c2cpy); \
        } else { \
            snprintf(ctx, sizeof(ctx), "%s", ctx1); \
        } \
        /* if necessary, modify passed-in argument so errno is returned */  \
        if (errval == -1)       errval = errno;  \
        OSKextLog(NULL, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag, \
                  "%s: %s", ctx, strerror(errval)); \
    } while(0)

/******************************************************************************
* destroyCaches cleans up a bootCaches structure
******************************************************************************/
void destroyCaches(struct bootCaches *caches)
{
    if (caches) {
        if (caches->cachefd != -1)  close(caches->cachefd);
        if (caches->cacheinfo)      CFRelease(caches->cacheinfo);
        if (caches->miscpaths)      free(caches->miscpaths);  // free strings
        if (caches->rpspaths)       free(caches->rpspaths);
        if (caches->exts)           free(caches->exts);
        if (caches->csfde_uuid)     CFRelease(caches->csfde_uuid);
        free(caches);
    }
}

/******************************************************************************
* readCaches checks for and reads bootcaches.plist
* it will also create directories for the caches in question if needed
******************************************************************************/
// used for turning /foo/bar into :foo:bar for kTSCacheDir entries (see awk(1))
static void gsub(char old, char new, char *s)
{
    char *p;

    while((p = s++) && *p)
        if (*p == old)
            *p = new;
}

static int
MAKE_CACHEDPATH(cachedPath *cpath, struct bootCaches *caches,
                CFStringRef relstr)
{
    int rval;
    size_t fullLen;
    char tsname[NAME_MAX];

    // check params
    rval = EINVAL;
    if (!(relstr))     goto finish;
    if (CFGetTypeID(relstr) != CFStringGetTypeID())     goto finish;

    // extract rpath
    rval = EOVERFLOW;
    if (!CFStringGetFileSystemRepresentation(relstr, cpath->rpath,
                                             sizeof(cpath->rpath))) {
        goto finish;
    }

    // tspath: rpath with '/' -> ':'
    if (strlcpy(tsname, cpath->rpath, sizeof(tsname)) >= sizeof(tsname))
        goto finish;
    gsub('/', ':', tsname);
    fullLen = snprintf(cpath->tspath, sizeof(cpath->tspath), "%s/%s/%s",
                       kTSCacheDir, caches->fsys_uuid, tsname);
    if (fullLen >= sizeof(cpath->tspath))
        goto finish;

    rval = 0;

finish:
    return rval;
}

// validate bootcaches.plist dict; data -> struct
// caller properly frees the structure if we fail
static int
extractProps(struct bootCaches *caches, CFDictionaryRef bcDict)
{
    int rval = ENODEV;
    CFDictionaryRef dict;   // don't release
    CFIndex keyCount;       // track whether we've handled all keys
    CFIndex rpsindex = 0;   // index into rps; compared to caches->nrps @ end
    CFStringRef str;        // used to point to objects owned by others
    CFStringRef createdStr = NULL;

    rval = EFTYPE;
    keyCount = CFDictionaryGetCount(bcDict);        // start with the top
    caches->exts = NULL;
    caches->nexts = 0;

    // process keys for paths read "before the booter"
    dict = (CFDictionaryRef)CFDictionaryGetValue(bcDict, kBCPreBootKey);
    if (dict) {
        CFArrayRef apaths;
        CFIndex miscindex = 0;

        if (CFGetTypeID(dict) != CFDictionaryGetTypeID())  goto finish;
        // only "Additional Paths" can contain > 1 path
        caches->nmisc = (int)CFDictionaryGetCount(dict);  // init w/1 path/key
        keyCount += CFDictionaryGetCount(dict);

        // look at variable-sized member first -> right size for miscpaths
        apaths = (CFArrayRef)CFDictionaryGetValue(dict, kBCAdditionalPathsKey);
        if (apaths) {
            CFIndex acount;

            if (CFArrayGetTypeID() != CFGetTypeID(apaths))  goto finish;
            acount = CFArrayGetCount(apaths);
            // total "misc" paths = # of keyed paths + # additional paths
            caches->nmisc += acount - 1;   // kBCAdditionalPathsKey not a path

            if ((unsigned int)caches->nmisc > INT_MAX/sizeof(*caches->miscpaths)) goto finish;
            caches->miscpaths = (cachedPath*)calloc(caches->nmisc,
                sizeof(*caches->miscpaths));
            if (!caches->miscpaths)  goto finish;

            for (/*miscindex = 0 (above)*/; miscindex < acount; miscindex++) {
                str = CFArrayGetValueAtIndex(apaths, miscindex);
                // MAKE_CACHEDPATH checks str != NULL && str's type
                MAKE_CACHEDPATH(&caches->miscpaths[miscindex], caches, str);
            }
            keyCount--; // AdditionalPaths sub-key
        } else {
            // allocate enough for the top-level keys (nothing variable-sized)
            if ((unsigned int)caches->nmisc > INT_MAX/sizeof(*caches->miscpaths)) goto finish;
            caches->miscpaths = calloc(caches->nmisc, sizeof(cachedPath));
            if (!caches->miscpaths)     goto finish;
        }
        
        str = (CFStringRef)CFDictionaryGetValue(dict, kBCLabelKey);
        if (str) {
            MAKE_CACHEDPATH(&caches->miscpaths[miscindex], caches, str);
            caches->label = &caches->miscpaths[miscindex];

            miscindex++;    // get ready for the next guy
#pragma unused(miscindex)
            keyCount--;     // DiskLabel is dealt with
        }

        // add new keys here
        keyCount--;     // preboot dict
    }


    // process booter keys
    dict = (CFDictionaryRef)CFDictionaryGetValue(bcDict, kBCBootersKey);
    if (dict) {
        if (CFGetTypeID(dict) != CFDictionaryGetTypeID())  goto finish;
        keyCount += CFDictionaryGetCount(dict);

        str = (CFStringRef)CFDictionaryGetValue(dict, kBCEFIBooterKey);
        if (str) {
            MAKE_CACHEDPATH(&caches->efibooter, caches, str);

            keyCount--;     // EFIBooter is dealt with
        }

        str = (CFStringRef)CFDictionaryGetValue(dict, kBCOFBooterKey);
        if (str) {
            MAKE_CACHEDPATH(&caches->ofbooter, caches, str);

            keyCount--;     // BootX, check
        }

        // add new booters here
        keyCount--;     // booters dict
    }

    // process keys for paths read "after the booter [is loaded]"
    // these are read by the booter proper, which determines which
    // of the Rock, Paper, Scissors directories is most current
    dict = (CFDictionaryRef)CFDictionaryGetValue(bcDict, kBCPostBootKey);
    if (dict) {
        CFDictionaryRef mkDict, erDict;
        CFArrayRef apaths;
        CFIndex acount;
        Boolean isKernelcache = false;
        int kcacheKeys = 0;

        if (CFGetTypeID(dict) != CFDictionaryGetTypeID())  goto finish;

        // we must deal with all sub-keys
        keyCount += CFDictionaryGetCount(dict);

        // Figure out how many files will be watched/copied via rpspaths
        // start by assuming each key provides one path to watch/copy
        caches->nrps = (int)CFDictionaryGetCount(dict);

        // AdditionalPaths: 1 key -> extra RPS entries
        apaths = (CFArrayRef)CFDictionaryGetValue(dict, kBCAdditionalPathsKey);
        if (apaths) {
            if (CFArrayGetTypeID() != CFGetTypeID(apaths))  goto finish;
            acount = CFArrayGetCount(apaths);

            // add in extra keys
            // "additional paths" array is not itself a path -> add one less
            caches->nrps += (acount - 1);
        } 


        // EncryptedRoot has 5 subkeys 
        erDict=(CFDictionaryRef)CFDictionaryGetValue(dict,kBCEncryptedRootKey);
        if (erDict) {
            if (CFGetTypeID(erDict)!=CFDictionaryGetTypeID())   goto finish;
            // erDict has one slot, but two required & copied sub-properties
            caches->nrps++;

            // the other three keys lead to a single localized resources cache
            if (CFDictionaryGetValue(erDict,kBCCSFDELocalizationSrcKey)) {
                caches->nrps++;
            }
        }

        // finally allocate correctly-sized rpspaths
        if ((unsigned int)caches->nrps > INT_MAX/sizeof(*caches->rpspaths))
            goto finish;
        caches->rpspaths = (cachedPath*)calloc(caches->nrps,
                            sizeof(*caches->rpspaths));
        if (!caches->rpspaths)  goto finish;


        // Load up rpspaths

        // populate rpspaths with AdditionalPaths; leave rpsindex -> avail
        // (above: apaths type-checked, rpsindex initialized to zero)
        if (apaths) {
            for (; rpsindex < acount; rpsindex++) {
                str = CFArrayGetValueAtIndex(apaths, rpsindex);
                MAKE_CACHEDPATH(&caches->rpspaths[rpsindex], caches, str);
            }
            keyCount--;     // handled AdditionalPaths
        }

        // com.apple.Boot.plist
        str = (CFStringRef)CFDictionaryGetValue(dict, kBCBootConfigKey);
        if (str) {
            MAKE_CACHEDPATH(&caches->rpspaths[rpsindex], caches, str);
            caches->bootconfig = &caches->rpspaths[rpsindex++];
            keyCount--;     // handled BootConfig
        }

        // EncryptedRoot items
        // two sub-keys required; one optional; one set of three optional
        if (erDict) {
            CFNumberRef boolRef;
            keyCount += CFDictionaryGetCount(erDict);

            str = CFDictionaryGetValue(erDict, kBCCSFDEPropertyCacheKey);
            if (str) {
                MAKE_CACHEDPATH(&caches->rpspaths[rpsindex], caches, str);
                caches->erpropcache = &caches->rpspaths[rpsindex++];
                keyCount--;
            }

            // !RootVolumePropertyCache => enable "timestamp only" optimization
            boolRef = CFDictionaryGetValue(erDict,kBCCSFDERootVolPropCacheKey);
            if (boolRef) {
                if (CFGetTypeID(boolRef) == CFBooleanGetTypeID()) {
                    caches->erpropTSOnly = CFEqual(boolRef, kCFBooleanFalse);
                    keyCount--;
                } else {
                    goto finish;
                }
            }

            // 8163405: non-localized resources
            str = CFDictionaryGetValue(erDict, kBCCSFDEDefResourcesDirKey);
            // XX check old key name for now
            if (!str) str=CFDictionaryGetValue(erDict,CFSTR("ResourcesDir")); 
            if (str) {
                MAKE_CACHEDPATH(&caches->rpspaths[rpsindex], caches, str);
                caches->efidefrsrcs = &caches->rpspaths[rpsindex++];
                keyCount--;
            }
            
            // localized resource cache
            str = CFDictionaryGetValue(erDict,kBCCSFDELocRsrcsCacheKey);
            if (str) {
                MAKE_CACHEDPATH(&caches->rpspaths[rpsindex], caches, str);
                caches->efiloccache = &caches->rpspaths[rpsindex++];
                keyCount--;
                
                // localization source material (required)
                str = CFDictionaryGetValue(erDict, kBCCSFDELocalizationSrcKey);
                if (str && CFGetTypeID(str) == CFStringGetTypeID() &&
                        CFStringGetFileSystemRepresentation(str,
                            caches->locSource, sizeof(caches->locSource))) {
                    keyCount--;
                } else {
                    goto finish;
                }
                
                // localization prefs file (required)
                str = CFDictionaryGetValue(erDict, kBCCSFDELanguagesPrefKey);
                if (str && CFGetTypeID(str) == CFStringGetTypeID() &&
                    CFStringGetFileSystemRepresentation(str, caches->locPref,
                                                sizeof(caches->locPref))) {
                    keyCount--;
                } else {
                    goto finish;
                }
            }

            keyCount--;     // handled EncryptedRoot
        }

        // we support any one of three kext archival methods
        kcacheKeys = 0;
        if (CFDictionaryContainsKey(dict, kBCMKextKey)) kcacheKeys++;
        if (CFDictionaryContainsKey(dict, kBCMKext2Key)) kcacheKeys++;
        if (CFDictionaryContainsKey(dict, kBCKernelcacheV1Key)) kcacheKeys++;
        if (CFDictionaryContainsKey(dict, kBCKernelcacheV2Key)) kcacheKeys++;
        if (CFDictionaryContainsKey(dict, kBCKernelcacheV3Key)) kcacheKeys++;

        if (kcacheKeys > 1) { 
            // don't support multiple types of kernel caching ...
            goto finish;
        }

      /* Handle the "Kernelcache" key for prelinked kernels for Lion and
       * later, the "MKext2 key" for format-2 mkext on Snow Leopard, and the
       * original "MKext" key for format-1 mkexts prior to SnowLeopard.
       */
        do {
            mkDict = (CFDictionaryRef)CFDictionaryGetValue(dict, kBCKernelcacheV1Key);
            if (!mkDict) {
                mkDict = (CFDictionaryRef)CFDictionaryGetValue(dict, kBCKernelcacheV2Key);
            }
            if (!mkDict) {
                mkDict = (CFDictionaryRef)CFDictionaryGetValue(dict, kBCKernelcacheV3Key);
            }

            if (mkDict) {
                isKernelcache = true;
                break;
            }

            mkDict = (CFDictionaryRef)CFDictionaryGetValue(dict, kBCMKext2Key);
            if (mkDict) break;

            mkDict = (CFDictionaryRef)CFDictionaryGetValue(dict, kBCMKextKey);
            if (mkDict) break;
        } while (0);

        if (mkDict) {
            if (CFGetTypeID(mkDict) != CFDictionaryGetTypeID()) {
                goto finish;
            }

            // path to the cache itself
            // currently /System/Library/Caches/com.apple.kext.caches/Startup/kernelcache
            str = (CFStringRef)CFDictionaryGetValue(mkDict, kBCPathKey);
            MAKE_CACHEDPATH(&caches->rpspaths[rpsindex], caches, str);   // M
            caches->kext_boot_cache_file = &caches->rpspaths[rpsindex++];
#pragma unused(rpsindex)

            // Starting with Kernelcache v1.3 kBCExtensionsDirKey is a key for
            // an array of paths to extensions directory. Pre v1.3 it is just
            // a string equal to "/System/Library/Extensions"
            size_t  bufsize = 0;
            apaths = (CFArrayRef)CFDictionaryGetValue(mkDict, kBCExtensionsDirKey);
            if (apaths && CFArrayGetTypeID() == CFGetTypeID(apaths)) {
                int     i;
                char    *bufptr;
                char    tempbuf[PATH_MAX];
                
                caches->nexts = (int) CFArrayGetCount(apaths);
                if (caches->nexts == 0)    goto finish;
                
                caches->exts = malloc(caches->nexts * PATH_MAX);
                if (caches->exts == NULL) {
                    OSKextLogMemError();
                    goto finish;
                }
                bufptr = caches->exts;
                
                for (i = 0; i < caches->nexts; i++) {
                    str = CFArrayGetValueAtIndex(apaths, i);
                    if (!str || CFGetTypeID(str) != CFStringGetTypeID()) {
                        goto finish;
                    }
                    if (!CFStringGetFileSystemRepresentation(str, tempbuf,
                                                             sizeof(tempbuf))) {
                        goto finish;
                    }
                    pathcpy(bufptr, tempbuf);
                    bufsize += (strlen(tempbuf) + 1);
                    bufptr += (strlen(tempbuf) + 1);
                }
            }
            else {
                // Pre v1.3 so we're dealing with just 1 path
                caches->exts = malloc(PATH_MAX);
                if (caches->exts == NULL) {
                    OSKextLogMemError();
                    goto finish;
                }
                caches->nexts = 1;
                str = (CFStringRef)CFDictionaryGetValue(mkDict, kBCExtensionsDirKey);
                if (!str || CFGetTypeID(str) != CFStringGetTypeID()) {
                    goto finish;
                }
                if (!CFStringGetFileSystemRepresentation(str, caches->exts, 
                                                     PATH_MAX)) {
                    goto finish;
                }
                bufsize = (strlen(caches->exts) + 1);
            }
            // trim if possible
            if (bufsize) {
                caches->exts = reallocf(caches->exts, bufsize);
                if (caches->exts == NULL) {
                    OSKextLogMemError();
                    goto finish;
                }
            }

            // kernelcaches have a kernel path key, which we set up by hand
            if (isKernelcache) {

                str = (CFStringRef)CFDictionaryGetValue(mkDict, kBCKernelPathKey);
                if (!str || CFGetTypeID(str) != CFStringGetTypeID()) goto finish;

                if (!CFStringGetFileSystemRepresentation(str, caches->kernel,
                    sizeof(caches->kernel))) {
                    goto finish;
                }
                
            }
 
            // Archs are fetched from the cacheinfo dictionary when needed
            keyCount--;     // mkext, mkext2, or kernelcache key handled
        }

        keyCount--;     // postBootPaths handled
    }

    if (keyCount == 0 && (unsigned)rpsindex == caches->nrps) {
        rval = 0;
        caches->cacheinfo = CFRetain(bcDict);   // for archs, etc
    }

finish:
    if (createdStr)     CFRelease(createdStr);
    if (rval != 0 && caches->exts != NULL) {
        free(caches->exts);
        caches->exts = NULL;
        caches->nexts = 0;
    }

    return rval;
}

// helper to create cache dirs; updateStamps() calls and accepts errors
static int
createCacheDirs(struct bootCaches *caches)
{
    int errnum, result = ELAST + 1;
    struct statfs sfs;
    char *errname;
    struct stat sb;
    char cachedir[PATH_MAX], uuiddir[PATH_MAX];      // bootstamps, csfde

    // don't create new cache directories if owners are disabled
    errname = caches->root;
    if (statfs(caches->root, &sfs) == 0) {
        if (sfs.f_flags & MNT_IGNORE_OWNERSHIP) {
            result = ENOTSUP; goto finish;
        }
    } else {
        result = errno; goto finish;
    }

    // bootstamps directory
    // (always made because it's used by libbless on non-BootRoot for ESP)
    errname = kTSCacheDir;
    pathcpy(cachedir, caches->root);
    pathcat(cachedir, kTSCacheDir);
    pathcpy(uuiddir, cachedir);
    pathcat(uuiddir, "/");
    pathcat(uuiddir, caches->fsys_uuid);
    if ((errnum = stat(uuiddir, &sb))) {
        if (errno == ENOENT) {
            // attempt to clean up cache dir to eliminate stale UUID dirs
            if (stat(cachedir, &sb) == 0) {
                (void)sdeepunlink(caches->cachefd, cachedir);
            }
            // s..mkdir ensures the cache directory is on the same volume
            if ((errnum = sdeepmkdir(caches->cachefd,uuiddir,kCacheDirMode))){
                result = errnum; goto finish;
            }
        } else {
            result = errnum; goto finish;
        }
    }

    // create /S/L/Caches/com.apple.corestorage as necessary
    if (caches->erpropcache) {
        errname = caches->erpropcache->rpath;
        pathcpy(cachedir, caches->root);
        pathcat(cachedir, dirname(caches->erpropcache->rpath));
        errname = cachedir;
        if ((-1 == stat(cachedir, &sb))) {
            if (errno == ENOENT) {
                // s..mkdir ensures cachedir is on the same volume
                errnum=sdeepmkdir(caches->cachefd,cachedir,kCacheDirMode);
                if (errnum) {
                    result = errnum; goto finish;
                }
            } else {
                result = errno; goto finish;
            }
        }
    }

    // success
    errname = NULL;
    result = 0;

// XX need to centralize this sort of error decoding (w/9217695?)
finish:
    if (result) {
        LOGERRxlate(errname, NULL, result);

        // so kextcache -u doesn't claim bootcaches.plist didn't exist, etc
        errno = 0;
    }

    return result;
}

static CFDictionaryRef
copy_dict_from_fd(int fd, struct stat *sb)
{
    CFDictionaryRef rval = NULL;
    void *buf = NULL;
    CFDataRef data = NULL;
    CFDictionaryRef dict = NULL;

    // read the plist
    if (sb->st_size > UINT_MAX || sb->st_size > LONG_MAX)   goto finish;
    if (!(buf = malloc((size_t)sb->st_size)))               goto finish;
    if (read(fd, buf, (size_t)sb->st_size) != sb->st_size)
        goto finish;
    if (!(data = CFDataCreate(nil, buf, (long)sb->st_size)))
        goto finish;

    // Sec: see 4623105 & related for an assessment of our XML parsers
    dict = (CFDictionaryRef)CFPropertyListCreateFromXMLData(nil,
                data, kCFPropertyListImmutable, NULL);
    if (!dict || CFGetTypeID(dict)!=CFDictionaryGetTypeID()) {
        goto finish;
    }

    rval = CFRetain(dict);

finish:
    if (dict)   CFRelease(dict);      // CFRetain()'d on success
    if (data)   CFRelease(data);
    if (buf)    free(buf);

    return rval;
}

/*
 * readBootCaches() reads a volumes bootcaches.plist file and returns
 * the contents in a new struct bootCaches.  Because it returns a pointer,
 * it stores a more precise error code in errno.
 */
struct bootCaches*
readBootCaches(char *volRoot, BRUpdateOpts_t opts)
{
    struct bootCaches *rval = NULL, *caches = NULL;
    int errnum = ELAST + 1;
    char *errmsg;
    struct statfs rootsfs;
    struct stat sb;
    char bcpath[PATH_MAX];
    CFDictionaryRef bcDict = NULL;
    uuid_t vol_uuid;

    errmsg = "allocation failure";
    caches = calloc(1, sizeof(*caches));
    if (!caches)            goto finish;
    caches->cachefd = -1;       // set cardinal (fd 0 valid)
    pathcpy(caches->root, volRoot);

    errmsg = "error opening " kBootCachesPath;
    pathcpy(bcpath, caches->root);
    pathcat(bcpath, kBootCachesPath);
    // Sec: cachefd lets us validate data, operations
    caches->cachefd = (errnum = open(bcpath, O_RDONLY|O_EVTONLY));
    if (errnum == -1) {
        if (errno == ENOENT) {
            // let kextcache -u log this special case
            errmsg = NULL;
        }
        goto finish;
    }

    // check the owner and mode (fstat() to insure it's the same file)
    // w/Leopard, root can see all the way to the disk; 99 -> truly unknown
    // note: 'sudo cp mach_kernel /Volumes/disrespected/' should -> error
    if (fstatfs(caches->cachefd, &rootsfs)) {
        goto finish;
    }
    if (fstat(caches->cachefd, &sb)) {
        goto finish;
    }
    // stash the timestamps for later reference (detect bc.plist changes)
    caches->bcTime = sb.st_mtimespec;      // stash so we can detect changes
    if (rootsfs.f_flags & MNT_QUARANTINE) {
        errmsg = kBootCachesPath " quarantined";
        goto finish;
    }
    if (sb.st_uid != 0) {
        errmsg = kBootCachesPath " not owned by root; no rebuilds";
        goto finish;
    }
    if (sb.st_mode & S_IWGRP || sb.st_mode & S_IWOTH) {
        errmsg = kBootCachesPath " writable by non-root";
        goto finish;
    }

    // get UUIDs & other info
    errmsg = "error obtaining storage information";
    if ((errnum = copyVolumeInfo(volRoot, &vol_uuid, &caches->csfde_uuid,
                                 caches->bsdname, caches->defLabel))){
        errno = errnum; goto finish;
    }
    if ((opts & kBRAnyBootStamps) == 0) {
        uuid_unparse_upper(vol_uuid, caches->fsys_uuid);
    }


    // plist -> dictionary
    errmsg = "error reading " kBootCachesPath;
    bcDict = copy_dict_from_fd(caches->cachefd, &sb);
    if (!bcDict)        goto finish;


    // error returned via errno now all that matters
    errmsg = NULL;

    // extractProps returns EFTYPE if the contents were whack
    // this function returns NULL on failure -> sends err# via errno :P
    if ((errnum = extractProps(caches, bcDict))) {
        errno = errnum; goto finish;
    }


    // success!
    rval = caches;

finish:
    // report any error message
    if (errmsg) {
        if (errnum == -1) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                 "%s: %s: %s.", caches->root, errmsg, strerror(errno));
        } else {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "%s: %s.", caches->root, errmsg);
        }
    }

    // clean up (unwind in reverse order of allocation)
    if (bcDict)     CFRelease(bcDict);  // extractProps() retains for struct

    // if things went awry, free anything associated with 'caches'
    if (!rval) {
        destroyCaches(caches);      // closes cachefd if needed
    }

    return rval;
}

struct bootCaches*
readBootCachesForDADisk(DADiskRef dadisk)
{
    struct bootCaches *rval = NULL;
    CFDictionaryRef ddesc = NULL;
    CFURLRef volURL;        // owned by dict; don't release
    char volRoot[PATH_MAX];
    int ntries = 0;

    // 'kextcache -U /' needs this retry to work around 5454260
    // kexd's vol_appeared filters volumes w/o mount points
    do {
        ddesc = DADiskCopyDescription(dadisk);
        if (!ddesc)     goto finish;
        volURL = CFDictionaryGetValue(ddesc,kDADiskDescriptionVolumePathKey);
        if (volURL) {
            break;
        } else {
            sleep(1);
            CFRelease(ddesc);
            ddesc = NULL;
        }
    } while (++ntries < kBRDiskArbMaxRetries);

    if (!volURL) {
        OSKextLog(NULL, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Disk description missing mount point for %d tries", ntries);
        goto finish;
    }

    if (ntries) {
        OSKextLog(NULL, kOSKextLogWarningLevel | kOSKextLogFileAccessFlag,
            "Warning: readCaches got mount point after %d tries.", ntries);
    }

    if (!CFURLGetFileSystemRepresentation(volURL, /* resolveToBase */ true,
                             (UInt8 *)volRoot, sizeof(volRoot))){
        OSKextLogStringError(NULL);
        goto finish;
    }

    rval = readBootCaches(volRoot, kBROptsNone);

finish:
    if (ddesc)      CFRelease(ddesc);

    return rval;
}

/*******************************************************************************
* needsUpdate checks a single path and timestamp; populates path->tstamp
* compares/stores *ctime* of the source file vs. the *mtime* of the bootstamp.
* returns false on error: if we can't tell, we probably can't update
*******************************************************************************/
Boolean needsUpdate(char *root, cachedPath* cpath)
{
    Boolean outofdate = false;
    Boolean rfpresent, tsvalid;
    struct stat rsb, tsb;
    char fullrp[PATH_MAX], fulltsp[PATH_MAX];

    // create full paths
    pathcpy(fullrp, root);
    pathcat(fullrp, cpath->rpath);
    pathcpy(fulltsp, root);
    pathcat(fulltsp, cpath->tspath);

    // check the source file in the root volume
    if (stat(fullrp, &rsb) == 0) {
        rfpresent = true;
    } else if (errno == ENOENT) {
        rfpresent = false;
    } else {
        // non-ENOENT errars => fail with log message
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Cached file %s: %s.", fullrp, strerror(errno));
        goto finish;
    }

    // The timestamp file's mtime tracks the source file's ctime.
    // If present, store the root path's timestamps to apply later.
    if (rfpresent) {
        TIMESPEC_TO_TIMEVAL(&cpath->tstamps[0], &rsb.st_atimespec);
        TIMESPEC_TO_TIMEVAL(&cpath->tstamps[1], &rsb.st_ctimespec);
    } else {
        // "no [corresponding] root file" is represented by a timestamp
        // file ("bootstamp") with a/mtime == 0.
        bzero(cpath->tstamps, sizeof(cpath->tstamps));
    }

    // check on the timestamp file itself
    // it's invalid if it tracks a non-existant root file
    if (stat(fulltsp, &tsb) == 0) {
        if (tsb.st_mtimespec.tv_sec != 0) {
            tsvalid = true;
        } else {
            tsvalid = false;
        }
    } else if (errno == ENOENT) {
        tsvalid = false;
    } else {
        // non-ENOENT errors => fail w/log message
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "timestamp cache %s: %s!", fulltsp, strerror(errno));
        goto finish;
    }


    // Depnding on root file vs. timestamp data, figure out what, if
    // anything, needs to be done.
    if (rfpresent && tsvalid) {
        outofdate = (tsb.st_mtimespec.tv_sec != rsb.st_ctimespec.tv_sec ||
               tsb.st_mtimespec.tv_nsec != rsb.st_ctimespec.tv_nsec);
    } else if (!rfpresent && tsvalid) {
        // need to propagate the fact that the file no longer exists
        outofdate = true;
    } else if (rfpresent && !tsvalid) {
        // need to make the timestamp valid
        outofdate = true;
    } else {
        // !rfpresent && !tsvalid
        outofdate = false;
    }

finish:
    return outofdate;
}

/*******************************************************************************
* needUpdates checks all paths and returns details if you want them
* It only to be called on volumes that will have timestamp paths
* (i.e. BootRoot volumes! ;)
* 
* In theory, all we have to do is find one "problem" (out of date file)
* but in practice, there could be real problems (like missing sources)
* which would prevent a complete update (at a minimum, all updates copy
* all RPS paths to a new RPS dir).  needsUpdate() also populates the
* tstamps used by updateStamps (for all files, regardless of whether
* they were updated).
*******************************************************************************/
#define OODMSG "not cached."
Boolean needUpdates(struct bootCaches *caches, Boolean *rps, Boolean *booters,
                    Boolean *misc, OSKextLogSpec oodLogSpec)
{
    Boolean rpsOOD, bootersOOD, miscOOD, anyOOD;
    cachedPath *cp;

    // assume nothing needs updating (caller may interpret error -> needsUpdate)
    rpsOOD = bootersOOD = miscOOD = anyOOD = false;

    for (cp = caches->rpspaths; cp < &caches->rpspaths[caches->nrps]; cp++) {
        if (needsUpdate(caches->root, cp)) {
            OSKextLog(NULL, oodLogSpec, "%s " OODMSG, cp->rpath);
            anyOOD = rpsOOD = true;
        }
    }
    if ((cp = &(caches->efibooter)), cp->rpath[0]) {
        if (needsUpdate(caches->root, cp)) {
            OSKextLog(NULL, oodLogSpec, "%s " OODMSG, cp->rpath);
            anyOOD = bootersOOD = true;
        }
    }
    if ((cp = &(caches->ofbooter)), cp->rpath[0]) {
        if (needsUpdate(caches->root, cp)) {
            OSKextLog(NULL, oodLogSpec, "%s " OODMSG, cp->rpath);
            anyOOD = bootersOOD = true;
        }
    }
    for (cp = caches->miscpaths; cp < &caches->miscpaths[caches->nmisc]; cp++) {
        if (needsUpdate(caches->root, cp)) {
            OSKextLog(NULL, oodLogSpec, "%s " OODMSG, cp->rpath);
            anyOOD = miscOOD = true;
        }
    }

    // This function only checks bootstamp timestamps as compared to
    // the source file.  kernel/kext caches, property caches files,
    // and other files are checked explicitly before this function
    // is called.

    if (rps)        *rps = rpsOOD;
    if (booters)    *booters = bootersOOD;
    if (misc)       *misc = miscOOD;

    return anyOOD;
}

/*******************************************************************************
* updateStamps runs through all of the cached paths in a struct bootCaches
* and applies the timestamps captured before the update
* not going to bother with a re-stat() of the sources for now
*******************************************************************************/
// could/should use schdirparent, move to safecalls.[ch]
static int
_sutimes(int fdvol, char *path, int oflags, struct timeval times[2])
{
    int bsderr;
    int fd = -1;

    // X O_RDONLY is the only way to open directories ... and the
    // descriptor allows timestamp updates
    if (-1 == (fd = sopen(fdvol, path, oflags, kCacheFileMode))) {
        bsderr = fd;
        // XX sopen() should log on its own after we get errors correct
        OSKextLog(NULL, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                  "%s: %s", path, strerror(errno));
        goto finish;        
    }
    if ((bsderr = futimes(fd, times))) {
        OSKextLog(NULL, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                  "futimes(<%s>): %s", path, strerror(errno));
    }

finish:
    if (fd != -1)   close(fd);

    return bsderr;
}

// Sec review: no need to drop privs thanks to safecalls.[ch]
static int
updateStamp(char *root, cachedPath *cpath, int fdvol, int command)
{
    int bsderr = -1;
    char fulltspath[PATH_MAX];

    pathcpy(fulltspath, root);
    pathcat(fulltspath, cpath->tspath);

    // we must unlink even for ApplyTimes b/c sopen() passes O_EXCL
    bsderr = sunlink(fdvol, fulltspath);    
    if (bsderr == -1 && errno == ENOENT) {
        bsderr = 0;
    }

    if (command == kBCStampsApplyTimes) {
        bsderr = _sutimes(fdvol, fulltspath, O_CREAT, cpath->tstamps);
    }

finish:
    return bsderr;
}

#define BRDBG_DISABLE_EXTSYNC_F "/var/db/.BRDisableExtraSync"
int
updateStamps(struct bootCaches *caches, int command)
{
    int anyErr = 0;         // accumulates errors
    struct statfs sfs;
    cachedPath *cp;
    struct stat sb;

    // don't try to apply bootstamps to a read-only volume
    if (statfs(caches->root, &sfs) == 0) {
        if ((sfs.f_flags & MNT_RDONLY)) {
            OSKextLog(NULL, kOSKextLogWarningLevel | kOSKextLogFileAccessFlag,
                      "Warning: %s read-only: no bootstamp updates",
                      caches->root);
            return 0;   // success
        }
    } 

    // allow known commands through
    switch (command) {
        case kBCStampsApplyTimes:
        case kBCStampsUnlinkOnly:
            break;

        default:
            return EINVAL;
    }

    // if writing stamps, make sure cache directory exists
    if (command == kBCStampsApplyTimes &&
            (anyErr = createCacheDirs(caches))) {
        return anyErr;
    }

    // run through all of the cached paths apply bootstamp
    for (cp = caches->rpspaths; cp < &caches->rpspaths[caches->nrps]; cp++) {
        anyErr |= updateStamp(caches->root, cp, caches->cachefd, command);
    }
    if ((cp = &(caches->efibooter)), cp->rpath[0]) {
        anyErr |= updateStamp(caches->root, cp, caches->cachefd, command);
    }
    if ((cp = &(caches->ofbooter)), cp->rpath[0]) {
        anyErr |= updateStamp(caches->root, cp, caches->cachefd, command);
    }
    for (cp = caches->miscpaths; cp < &caches->miscpaths[caches->nmisc]; cp++){
        anyErr |= updateStamp(caches->root, cp, caches->cachefd, command);
    }

    // Clean shutdown should make sure these stamps are on disk; this
    // code worked around 8603195/6848376 which were fixed by Lion GM.
    if (stat(BRDBG_DISABLE_EXTSYNC_F, &sb) == -1) {
        anyErr |= fcntl(caches->cachefd, F_FULLFSYNC);
    }

    return anyErr;
}

/*******************************************************************************
* rebuild_kext_boot_cache_file fires off kextcache on the given volume
* XX there is a bug here that can mask a stale mkext in the Apple_Boot (4764605)
*******************************************************************************/
int rebuild_kext_boot_cache_file(
    struct bootCaches *caches,
    Boolean wait,
    const char * kext_boot_cache_file,
    const char * kernel_file)
{   
    int             rval                    = ELAST + 1;
    int             pid                     = -1;
    CFIndex i, argi = 0, argc = 0, narchs = 0;
    CFDictionaryRef pbDict, mkDict;
    CFArrayRef      archArray;
    char **kcargs = NULL, **archstrs = NULL;    // no [ARCH_MAX] anywhere?
    char          * lastslash               = NULL;
    char            rcpath[PATH_MAX]        = "";
    struct stat     sb;
    char            full_cache_file_path[PATH_MAX]        = "";
    char            full_cache_file_dir_path[PATH_MAX]    = "";
    char          * fullextsp               = NULL;
    char            fullkernelp[PATH_MAX] = "";
    Boolean         generateKernelcache     = false;
    int             mkextVersion            = 0;

    // bootcaches.plist might not request mkext/kernelcache rebuilds
    if (!caches->kext_boot_cache_file
    ) {
       goto finish;
    }

    fullextsp = malloc(caches->nexts * PATH_MAX);
    if (!fullextsp)  goto finish;
    *fullextsp = 0x00;

    pbDict = CFDictionaryGetValue(caches->cacheinfo, kBCPostBootKey);
    if (!pbDict || CFGetTypeID(pbDict) != CFDictionaryGetTypeID())  goto finish;

   /* Try for a Kernelcache key, and if there isn't one, look for an "MKext" key.
    */
    do {
        mkDict = CFDictionaryGetValue(pbDict, kBCKernelcacheV1Key);
        if (!mkDict)
            mkDict = CFDictionaryGetValue(pbDict, kBCKernelcacheV2Key);
        if (!mkDict) {
            mkDict = CFDictionaryGetValue(pbDict, kBCKernelcacheV3Key);
        }

        if (mkDict) {
            generateKernelcache = true;
            break;
        }

        mkDict = CFDictionaryGetValue(pbDict, kBCMKext2Key);
        if (mkDict) {
            mkextVersion = 2;
            break;
        }

        mkDict = CFDictionaryGetValue(pbDict, kBCMKextKey);
        if (mkDict) {
            mkextVersion = 1;
            break;
        }

    } while (0);

    if (!mkDict || CFGetTypeID(mkDict) != CFDictionaryGetTypeID())  goto finish;

        archArray = CFDictionaryGetValue(mkDict, kBCArchsKey);
    if (archArray) {
        narchs = CFArrayGetCount(archArray);
        archstrs = calloc(narchs, sizeof(char*));
        if (!archstrs)  goto finish;
    }

    //      argv[0]   -a x -a y   -l [-n] [-r] [-K <kernel>] -c <kcache> -volume-root <vol> <exts>  NULL
    argc =  1       + (narchs*2) + 1 + 1  + 1  + 1     + 1  + 1    + 1           + 1  + 1  + caches->nexts + 1;
    kcargs = malloc(argc * sizeof(char*));
    if (!kcargs)  goto finish;
    kcargs[argi++] = "kextcache";

    // convert each -arch argument into a char* and add to the vector
    for(i = 0; i < narchs; i++) {
        CFStringRef archStr;
        size_t archSize;

        // get  arch
        archStr = CFArrayGetValueAtIndex(archArray, i);
        if (!archStr || CFGetTypeID(archStr)!=CFStringGetTypeID()) goto finish;
        // XX an arch is not a pathname; EncodingASCII might be more appropriate
        archSize = CFStringGetMaximumSizeOfFileSystemRepresentation(archStr);
        if (!archSize)  goto finish;
        // X marks the spot: over 800 lines written before I realized that
        // there were some serious security implications
        archstrs[i] = malloc(archSize);
        if (!archstrs[i])  goto finish;
        if (!CFStringGetFileSystemRepresentation(archStr,archstrs[i],archSize))
            goto finish;

        kcargs[argi++] = "-arch";
        kcargs[argi++] = archstrs[i];
    }

    // BootRoot always includes local kexts
    kcargs[argi++] = "-local-root";

    // 6413843 check if it's installation media (-> add -n)
    pathcpy(rcpath, caches->root);
    removeTrailingSlashes(rcpath);       // X caches->root trailing '/'?
    pathcat(rcpath, "/etc/rc.cdrom");
    if (stat(rcpath, &sb) == 0) {
        kcargs[argi++] = "-network-root";
    }

    // determine proper argument to precede kext_boot_cache_file
    if (generateKernelcache) {
        // for '/' only, include all kexts loaded since boot (9130863)
        // TO DO: can we optimize for the install->first boot case?
        if (0 == strcmp(caches->root, "/")) {
            kcargs[argi++] = "-all-loaded";
        }
        pathcpy(fullkernelp, caches->root);
        removeTrailingSlashes(fullkernelp);
        pathcat(fullkernelp, kernel_file);
        kcargs[argi++] = "-kernel";
        kcargs[argi++] = fullkernelp;
        // prelinked kernel path below
        kcargs[argi++] = "-prelinked-kernel";
    } else if (mkextVersion == 2) {
        kcargs[argi++] = "-mkext2";
    } else if (mkextVersion == 1) {
        kcargs[argi++] = "-mkext1";
    } else {
        // internal error!
        goto finish;
    }

    pathcpy(full_cache_file_path, caches->root);
    removeTrailingSlashes(full_cache_file_path);
    pathcat(full_cache_file_path, kext_boot_cache_file);
    kcargs[argi++] = full_cache_file_path;

    kcargs[argi++] = "-volume-root";
    kcargs[argi++] = caches->root;

    // we now support multiple extensions directories
    char    *extsDirPtr = caches->exts;
    char    *tempExtsDirPtr = fullextsp;
    
    for (i = 0; i < caches->nexts; i++) {
        pathcpy(tempExtsDirPtr, caches->root);
        removeTrailingSlashes(tempExtsDirPtr);
        pathcat(tempExtsDirPtr, extsDirPtr);
        
        kcargs[argi++] = tempExtsDirPtr;
      
        extsDirPtr += (strlen(extsDirPtr) + 1);
        tempExtsDirPtr += (strlen(tempExtsDirPtr) + 1);
    }    
    kcargs[argi] = NULL;

    pathcpy(full_cache_file_dir_path, full_cache_file_path);
    lastslash = rindex(full_cache_file_dir_path, '/');
    if (lastslash) {
        *lastslash = '\0';

       /* Make sure we have a destination directory to write the new mkext
        * file into (people occasionally delete the caches folder).
        */
        if ((rval = sdeepmkdir(caches->cachefd, full_cache_file_dir_path,
                               kCacheDirMode))) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "failed to create cache folder %s.", full_cache_file_dir_path);
            // can't make dest directory, kextcache will fail, so don't bother
            goto finish;
        }

    }
    rval = 0;

#if 0
    OSKextLog(NULL,
              kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
              "%s: kextcache args %ld ",
              __FUNCTION__, argi);
    for (i = 0; i < argi; i++) {
        OSKextLog(NULL,
                  kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                  "%s ",
                  kcargs[i]);
    }
#endif

   /* wait:false means the return value is <0 for fork/exec failures and
    * the pid of the forked process if >0.
    *
    * wait:true means the return value is <0 for fork/exec failures and
    * the exit status of the forked process (>=0) otherwise.
    */
    pid = fork_program("/usr/sbin/kextcache", kcargs, wait);  // logs errors

finish:
    if (rval) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Data error before mkext rebuild.");
    }
    if (wait || pid < 0) {
        rval = pid;
    }

    if (archstrs) {
        for (i = 0; i < narchs; i++) {
            if (archstrs[i])    free(archstrs[i]);
        }
        free(archstrs);
    }
    if (fullextsp)  free(fullextsp);
    if (kcargs)     free(kcargs);

    return rval;
}

/*******************************************************************************
* Check our various plist caches, for the current kernel arch only, to see if
* they need to be rebuilt:
*
*    id -> url index (per directory)
*    logindwindow prop/value cache for OSBundleHelper (global)
*
* This should only be called for the root volume!
*******************************************************************************/
Boolean plistCachesNeedRebuild(const NXArchInfo * kernelArchInfo)
{
    Boolean     result                     = true;
    CFArrayRef  systemExtensionsFolderURLs = NULL;  // need not release
    CFStringRef cacheBasename              = NULL;  // must release
    CFIndex     count, i;

    systemExtensionsFolderURLs = OSKextGetSystemExtensionsFolderURLs();
    if (!systemExtensionsFolderURLs ||
        !CFArrayGetCount(systemExtensionsFolderURLs)) {
        
        result = false;
        goto finish;
    }

    count = CFArrayGetCount(systemExtensionsFolderURLs);
    for (i = 0; i < count; i++) {
        CFURLRef directoryURL = CFArrayGetValueAtIndex(
            systemExtensionsFolderURLs, i);

       /* Check the KextIdentifiers index.
        */
        if (!_OSKextReadCache(directoryURL, CFSTR(_kOSKextIdentifierCacheBasename),
            /* arch */ NULL, _kOSKextCacheFormatCFBinary, /* parseXML? */ false,
            /* valuesOut*/ NULL)) {

            goto finish;
        }
    }

   /* Check the KextPropertyValues_OSBundleHelper cache for the current kernel arch.
    */
    cacheBasename = CFStringCreateWithFormat(kCFAllocatorDefault,
        /* formatOptions */ NULL, CFSTR("%s%s"),
        _kKextPropertyValuesCacheBasename,
        "OSBundleHelper");
    if (!cacheBasename) {
        OSKextLogMemError();
        result = false; // cause we don't be able to update
        goto finish;
    }

    if (!_OSKextReadCache(systemExtensionsFolderURLs, cacheBasename,
        kernelArchInfo, _kOSKextCacheFormatCFXML, /* parseXML? */ false,
        /* valuesOut*/ NULL)) {
        
        goto finish;
    }

    result = false;

finish:
    SAFE_RELEASE(cacheBasename);
    return result;
}

Boolean check_kext_boot_cache_file(
    struct bootCaches * caches,
    const char * cache_path,
    const char * kernel_path)
{   
    Boolean      needsrebuild                       = false;
    char         full_cache_file_path[PATH_MAX]     = "";
    char         fullextsp[PATH_MAX]                = "";
    char         fullkernelp[PATH_MAX]              = "";
    struct stat  extsb;
    struct stat  kernelsb;
    struct stat  sb;
    time_t       validModtime                       = 0;

   /* Do we have a cache file (mkext or kernelcache)?
    * Note: cache_path is a pointer field, not a static array.
    */
    if (cache_path == NULL)
        goto finish;
    
   /* If so, check the mod time of the cache file vs. the extensions folder.
    */
    // struct bootCaches paths are all *relative*
    pathcpy(full_cache_file_path, caches->root);
    removeTrailingSlashes(full_cache_file_path);
    pathcat(full_cache_file_path, cache_path);

    // we support multiple extensions directories, use latest mod time
    char    *bufptr;
    int     i;
    bufptr = caches->exts;
    
    for (i = 0; i < caches->nexts; i++) {
        pathcpy(fullextsp, caches->root);
        removeTrailingSlashes(fullextsp);
        pathcat(fullextsp, bufptr);

        if (stat(fullextsp, &extsb) == 0) {
            if (extsb.st_mtime + 1 > validModtime) {
                validModtime = extsb.st_mtime + 1;
          }
        }
        else {
        OSKextLog(NULL, kOSKextLogWarningLevel | kOSKextLogFileAccessFlag,
                  "Warning: %s: %s", fullextsp, strerror(errno));
        }
        bufptr += (strlen(bufptr) + 1);
        fullextsp[0] = 0x00;
    }

   /* Check the mod time of the appropriate kernel too, if applicable.
    */

   /* A kernel path in bootcaches.plist means we should have a kernelcache.
    * Note: kernel_path is a static array, not a pointer field.
    */
    if (kernel_path[0]) {            
        pathcpy(fullkernelp, caches->root);
        removeTrailingSlashes(fullkernelp);
        pathcat(fullkernelp, kernel_path);

        if (stat(fullkernelp, &kernelsb) == -1) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogBasicLevel | kOSKextLogFileAccessFlag,
                "Note: %s: %s", fullkernelp, strerror(errno));
            // assert(needsrebuild == false);   // we can't build w/o kernel
            goto finish;
        }

       /* The cache file should be 1 second newer than the newer of the
        * Extensions folder(s) or the kernel.
        */
        if (kernelsb.st_mtime > validModtime) {
            validModtime = kernelsb.st_mtime + 1;
       }
    }

    // The cache file itself
    needsrebuild = true;  // since this stat() will fail if cache file is gone
    if (stat(full_cache_file_path, &sb) == -1) {
        goto finish;
    }
    needsrebuild = (sb.st_mtime != validModtime);

finish:
    return needsrebuild;
}

/*******************************************************************************
* createDiskForMount creates a DADisk object given a mount point
* session is optional; one is created and released if the caller can't supply
*******************************************************************************/
DADiskRef createDiskForMount(DASessionRef session, const char *mount)
{
    DADiskRef rval = NULL;
    DASessionRef dasession = NULL;
    CFURLRef volURL = NULL;

    if (session) {
        dasession = session;
    } else {
        dasession = DASessionCreate(nil);
        if (!dasession)     goto finish;
    }

    volURL = CFURLCreateFromFileSystemRepresentation(nil, (UInt8*)mount,
            strlen(mount), 1 /*isDirectory*/);
    if (!volURL)        goto finish;

    rval = DADiskCreateFromVolumePath(nil, dasession, volURL);

finish:
    if (volURL)     CFRelease(volURL);
    if (!session && dasession)
        CFRelease(dasession);

    return rval;
}


/*****************************************************************************
* CoreStorage FDE check & update routines
* kextd calls check_csfde; kextcache calls check_, rebuild_csfde_cache()
*****************************************************************************/
// on success, caller is responsible for releasing econtext
int
copyCSFDEInfo(CFStringRef uuidStr, CFDictionaryRef *econtext,
               time_t *timeStamp)
{
    int             rval = ELAST+1;
    CFDictionaryRef lvfprops = NULL;
    CFDictionaryRef ectx;
    CFNumberRef     psRef;
    CFArrayRef      eusers;     // owned by lvfprops
    Boolean         encrypted;

    if (!uuidStr) {
        rval = EINVAL; goto finish;
    }

    // 04/25/11 - gab: <rdar://problem/9168337>
    // can't operate without libCoreStorage func
    if (CoreStorageCopyFamilyProperties == NULL) {
        rval = ESHLIBVERS; goto finish;
    }

    lvfprops = CoreStorageCopyFamilyProperties(uuidStr);
    if (!lvfprops) {
        rval = EFTYPE; goto finish;
    }

    ectx = (CFMutableDictionaryRef)CFDictionaryGetValue(lvfprops,
                        CFSTR(kCoreStorageFamilyEncryptionContextKey));
    if (!ectx || CFGetTypeID(ectx) != CFDictionaryGetTypeID()) {
        rval = EFTYPE; goto finish;
    }

    // does it have encrypted users?
    eusers = (CFArrayRef)CFDictionaryGetValue(ectx, CFSTR(kCSFDECryptoUsersID));
    encrypted = (eusers && CFArrayGetCount(eusers));

    if (encrypted) {
        if (econtext) {
            *econtext = CFRetain(ectx);
        }
        if (timeStamp) {
            psRef = CFDictionaryGetValue(ectx, CFSTR(kCSFDELastUpdateTime));
            if (psRef) {
                if (CFGetTypeID(psRef) != CFNumberGetTypeID() ||
                    !CFNumberGetValue(psRef,kCFNumberSInt64Type,timeStamp)){
                    rval = EFTYPE; goto finish;
                }
            } else {    // no timestamp (odd, but maybe okay)
                *timeStamp = 0LL;
            }
        }
    } else {        // not encrypted
        if (econtext)       *econtext = NULL;
        if (timeStamp)      *timeStamp = 0LL;
    }

    rval = 0;

finish:
    if (lvfprops)   CFRelease(lvfprops);

    if (rval) {
        OSKextLogCFString(NULL, kOSKextLogErrorLevel|kOSKextLogFileAccessFlag,
                          CFSTR("could not copy LVF props for %@: %s"),
                          uuidStr, strerror(rval));
    }

    return rval;
}

Boolean
check_csfde(struct bootCaches *caches)
{
    Boolean         needsupdate = false;
    time_t          propStamp, erStamp;
    char            erpath[PATH_MAX];
    struct stat     ersb;

    if (!caches->csfde_uuid || !caches->erpropcache)
        goto finish;

    if (copyCSFDEInfo(caches->csfde_uuid, NULL, &propStamp))
        goto finish;

    // get property cache file's timestamp
    pathcpy(erpath, caches->root);
    pathcat(erpath, caches->erpropcache->rpath);
    if (stat(erpath, &ersb) == 0) {
        erStamp = ersb.st_mtimespec.tv_sec;
    } else {
        if (errno == ENOENT) {
            erStamp = 0LL;
        } else {
            goto finish;
        }
    }

    // generally the timestamp advances, but != means out of date
    needsupdate = erStamp != propStamp;

finish:
    return needsupdate;
}

/*
 * _writeCSFDENoFD()
 * If possible, use CSFDEInitPropertyCache() to write
 * EncryptedRoot.plist.wipekey to the requested path. 
 *
 * CSFDEInitPropertyCache() writes to <path>/S/L/Caches/com.apple.corestorage
 * so the basic algorithm is
 * 0) provided dstpath = /path/to/S/L/Caches/com.apple.corestorage
 * 1) find substring S/L/Caches/c.a.corestorage/EncryptedRoot.plist.wipekey
 * 2) create terminated parentpath = path/to/\0ystem/L/Caches...
 * 3) create /path/to/.../SystemVersion.plist if it doesn't exist
 * 4) call CSFDEInitPropertyCache(/path/to)!
 */
// CSFDEInitPropertyCache() uses /S/L/E in 10.7.2+, but _writeCSFDENoFD()
// is only for 10.7.[01] where InitPropertyCache() uses SystemVersion.plist.
#define kOrigInitCookieDir "/System/Library/CoreServices"
#define kOrigInitCookieFile "/SystemVersion.plist"
#define kFDECacheFile kCSFDEPropertyCacheDir"/"kCSFDEPropertyCacheFileEncrypted
static int
_writeCSFDENoFD(int scopefd, CFDictionaryRef ectx,
                CFStringRef wipeKeyUUID, char *dstpath)  
{
    int bsderr, rval = ELAST + 1;       // all but path*() should set
    int fd = -1;
    Boolean createdCookie = false;
    char parentpath[PATH_MAX], cookiepath[PATH_MAX];
    char *relpath;
    struct stat sb;

    // detect expected relative path to EncryptedRoot.plist.wipekey
    // and create terminated parentpath
    pathcpy(parentpath, dstpath);
    if (!(relpath = strstr(parentpath, kFDECacheFile))) {
        // path doesn't contain expected substring
        rval = EINVAL; LOGERRxlate(dstpath, "missing" kFDECacheFile, rval);
        goto finish;
    }
    relpath[0] = '\0';      // terminate parentpath[] at common parent

    // if necessary, create sibling SystemVersion.plist
    pathcpy(cookiepath, parentpath);
    pathcat(cookiepath, kOrigInitCookieDir);
    if ((bsderr = sdeepmkdir(scopefd, cookiepath, kCacheDirMode))) {
        rval = bsderr; LOGERRxlate(cookiepath, NULL, rval); goto finish;
    }
    pathcat(cookiepath, kOrigInitCookieFile);
    if (0 != stat(cookiepath, &sb)) {
        if ((fd = sopen(scopefd, cookiepath, O_CREAT, kCacheFileMode)) < 0) {
            rval = errno; LOGERRxlate(cookiepath, NULL, rval); goto finish;
        } 
        close(fd);
        createdCookie = true;
    }

    // write via the 10.7.[01] function (scopefd ignored!)
    errno = 0;
    OSKextLog(NULL, kOSKextLogWarningLevel | kOSKextLogFileAccessFlag,
              "WARNING: no CSFDEWritePropertyCacheToFD(); "
              "trying CSFDEInitPropertyCache()");
    if (false == CSFDEInitPropertyCache(ectx, parentpath, wipeKeyUUID)) {
        rval = ELAST + 1;   // "internal error" :P
        LOGERRxlate("CSFDEInitPropertyCache", parentpath, rval);
        goto finish;
    }
    // make sure it did the deed
    if (-1 == stat(dstpath, &sb)) {
        rval = errno; LOGERRxlate(dstpath, NULL, rval); goto finish;
    }
 
    // success!
    rval = 0;

finish:
    if (createdCookie) {
        (void)sunlink(scopefd, cookiepath);   // empty boot.?/S/L/CS okay
    }

    return rval;
}

// NOTE: weak-linking depends on -weak-l/-weak_framemwork *and* the
// function declaration being marked correctly in the header file!
int
writeCSFDEProps(int scopefd, CFDictionaryRef ectx, 
                char *cspvbsd, char *dstpath)  
{
    int             errnum, rval = ELAST + 1;
    CFStringRef     wipeKeyUUID = NULL;
    char            dstparent[PATH_MAX];
    int             erfd = -1;

    // 9168337 didn't quite do it, see 10831618
    // check for required weak-linked symbol
    if (CoreStorageCopyPVWipeKeyUUID==NULL) {
        rval = ESHLIBVERS;
        LOGERRxlate("no CoreStorageCopyPVWipeKeyUUID()", NULL, rval);
        goto finish;
    }
    wipeKeyUUID = CoreStorageCopyPVWipeKeyUUID(cspvbsd);
    if (!wipeKeyUUID) {  
        OSKextLog(NULL, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                  "CoreStorageCopyPVWipeKeyUUID(%s) failed", cspvbsd);
        rval = ENODEV; goto finish;      
    }

    // prep (ENOENT ignored by szerofile())
    if ((errnum = szerofile(scopefd, dstpath)) ||
            ((errnum = sunlink(scopefd, dstpath)) && errno != ENOENT)) {
        OSKextLog(NULL, kOSKextLogWarningLevel | kOSKextLogFileAccessFlag,
                  "WARNING: %s: %s", dstpath, strerror(errno));
    }

    // recursively create the parent directory       
    if (strlcpy(dstparent,dirname(dstpath),PATH_MAX) >= PATH_MAX) {
        rval = EOVERFLOW; goto finish;
    }
    if ((errnum = sdeepmkdir(scopefd, dstparent, kCacheDirMode))) {
        rval = errnum; LOGERRxlate(dstparent, NULL, rval); goto finish;
    }

    // use modern function if available
    if (CSFDEWritePropertyCacheToFD!=NULL) {
        // open and write to FD
        erfd = sopen(scopefd, dstpath, O_CREAT|O_RDWR, kCacheFileMode);
        if (-1 == erfd) {
            rval = errno; LOGERRxlate(dstpath, NULL, rval); goto finish;
        }
        if (!CSFDEWritePropertyCacheToFD(ectx, erfd, wipeKeyUUID)) {
            rval = ELAST + 1;   // "internal error" :P
            LOGERRxlate("CSFDEWritePropertyCacheToFD", dstpath, rval);
            goto finish;
        }
    } else {
        // try to trick the old function into writing the cache
        if ((errnum = _writeCSFDENoFD(scopefd,ectx,wipeKeyUUID,dstpath))) {
            rval = errnum; goto finish;     // error logged by function
        }
    }

    // success
    rval = 0;

finish:
    if (wipeKeyUUID)    CFRelease(wipeKeyUUID);
    if (erfd != -1)     close(erfd);

    return rval;
}

// write out a populated EncryptedRoot.plist.wipekey to the root volume
static int
_writeLegacyCSFDECache(struct bootCaches *caches)
{
    int             errnum, rval = ELAST + 1;
    CFArrayRef      dataVolumes = NULL;
    CFStringRef     bsdStr;     // belongs to dataVolumes
    char            bsdname[DEVMAXPATHSIZE];
    CFDictionaryRef ectx = NULL;
    char           *errmsg;
    char            erpath[PATH_MAX];
    int             erfd = -1;

    errmsg = "invalid argument";
    if (!caches->csfde_uuid || !caches->erpropcache) {
        rval = EINVAL; goto finish;
    }
    
    // hasBRBs() cares about Apple_Boot's; FDE, data partitions
    (void)hasBootRootBoots(caches, NULL, &dataVolumes, NULL);
    if (!dataVolumes || CFArrayGetCount(dataVolumes) == 0) {
        errmsg = "no data partition! (for wipe key)";
        rval = ENODEV; goto finish;
    }

    // legacy => encrypt with the first Apple_CoreStorage wipe key
    errmsg = "error getting volume wipe key";
    bsdStr = CFArrayGetValueAtIndex(dataVolumes, 0);
    if (!bsdStr) {
        rval = ENODEV; goto finish;
    }
    if (!CFStringGetFileSystemRepresentation(bsdStr,bsdname,sizeof(bsdname))){
        rval = EINVAL; goto finish;
    }

    errmsg = "error getting encryption context data";
    if ((errnum = copyCSFDEInfo(caches->csfde_uuid, &ectx, NULL))) {
        rval = errnum; goto finish;
    }

    // build /<vol>/S/L/Caches/..corestorage/EncryptedRoot.plist.wipekey
    errmsg = "error building encryption context cache file path";
    pathcpy(erpath, caches->root);
    pathcat(erpath, caches->erpropcache->rpath);
    errmsg = NULL;

    // if not encrypted, just nuke :)
    if (!ectx) {
        (void)sunlink(caches->cachefd, erpath);
        rval = 0; goto finish;
    }

    errmsg = NULL;      // writeCSFDEProps() logs errors
    if ((errnum = writeCSFDEProps(caches->cachefd, ectx, bsdname, erpath))) {
        rval = errnum; goto finish;
    }

    // success
    rval = 0;

finish:
    if (erfd != -1)     close (erfd);
    if (dataVolumes)    CFRelease(dataVolumes);
    if (ectx)           CFRelease(ectx);

    if (rval && errmsg) {
        LOGERRxlate(caches->root, errmsg, rval);
    }
    
    return rval;
}

int
rebuild_csfde_cache(struct bootCaches *caches)
{
    int             errnum, rval = ELAST + 1;
    time_t          timeStamp;
    char            erpath[PATH_MAX] = "<unknown>";
    struct timeval  times[2] = {{ 0, 0 }, { 0, 0 }};

    if (!caches->csfde_uuid || !caches->erpropcache) {
        rval = EINVAL; goto finish;
    }

    if ((errnum = createCacheDirs(caches))) {
        rval = errnum; goto finish;
    }

    // OSes that only support single-PV CSFDE need content in erpropcache
    if (caches->erpropTSOnly == false) {
        return _writeLegacyCSFDECache(caches);    // takes care of everything
    }

    // otherwise, just grab the timestamp so update_boot.c knows to re-fetch
    if ((errnum = copyCSFDEInfo(caches->csfde_uuid, NULL, &timeStamp))) {
        rval = errnum; goto finish;
    }
    times[0].tv_sec = (__darwin_time_t)timeStamp;
    times[1].tv_sec = (__darwin_time_t)timeStamp;    // mdworker -> atime

    // build path and recreate proper timestamp
    pathcpy(erpath, caches->root);
    pathcat(erpath, caches->erpropcache->rpath);
    (void)sunlink(caches->cachefd, erpath);

    if (timeStamp != 0LL) {
        if ((errnum = _sutimes(caches->cachefd, erpath, O_CREAT, times))) {
            rval = errnum; goto finish;
        }
    }

    // success
    rval = 0;

finish:
    // no logging above
    if (rval)       LOGERRxlate(erpath, NULL, rval);
    
    return rval;
}


/*******************************************************************************
* check_loccache() checks caches that depend on the system localization
* XX could use getFilePathModTimePlusOne() -- currently in kextcache_main.c
*******************************************************************************/
// [PATH_MAX] is essentially a comment; char[] are char* after being passed
static int
get_locres_info(struct bootCaches *caches, char locRsrcDir[PATH_MAX],
                char prefPath[PATH_MAX], struct stat *prefsb,
                char locCacheDir[PATH_MAX], time_t *validModTime)
{
    int rval = EOVERFLOW;       // all other paths set rval
    time_t newestTime;
    struct stat sb;

    if (!validModTime) {
        rval = EINVAL; LOGERRxlate("get_locres_info", NULL, rval); goto finish;
    }
    
    // build localization sources directory path
    pathcpy(locRsrcDir, caches->root);
    pathcat(locRsrcDir, caches->locSource);
    // get localization sources directory timestamp
    if (stat(locRsrcDir, &sb)) {
        rval = errno; LOGERRxlate(locRsrcDir, NULL, rval); goto finish;
    }
    newestTime = sb.st_mtime;
    
    // prefs file path & timestamp (if it exists)
    pathcpy(prefPath, caches->root);
    pathcat(prefPath, caches->locPref);
    if (stat(prefPath, prefsb) == 0) {
        if (prefsb->st_mtime > newestTime) {
            newestTime = prefsb->st_mtime;
        }
    } else {
        if (errno != ENOENT) {
            rval = errno; LOGERRxlate(prefPath, NULL, rval); goto finish;
        }
    }
    
    // the cache directory must be one second newer than the 
    // later of the prefs file and the source directory.
    *validModTime = newestTime + 1;

    // build localized resources cache directory path
    pathcpy(locCacheDir, caches->root);
    pathcat(locCacheDir, caches->efiloccache->rpath);    
    
    rval = 0;

finish:
    return rval;
}

Boolean
check_loccache(struct bootCaches *caches)
{
    Boolean     needsupdate = false;   // needsupdate defaults to "nope"
    struct stat prefsb, cachesb;
    char        erpath[PATH_MAX];
    char        locRsrcDir[PATH_MAX], prefPath[PATH_MAX];
    char        locCacheDir[PATH_MAX];
    time_t      validModTime = 0;

    if (!caches->efiloccache)       goto finish;

    // 9516786: loccache only needed if EFI Login plist is active
    pathcpy(erpath, caches->root);
    pathcat(erpath, caches->erpropcache->rpath);
    if (stat(erpath, &cachesb) == -1 && errno == ENOENT) {
        // not an error, there is no cache file on non-encrypted volumes
        goto finish;
    }

    if (get_locres_info(caches, locRsrcDir, prefPath, &prefsb,
                        locCacheDir, &validModTime)) {
        goto finish;    // error logged by function
    }
    
    if (stat(locCacheDir, &cachesb) == 0) {
        needsupdate = (cachesb.st_mtime != validModTime);
    } else if (errno == ENOENT) {
        needsupdate = true;
    }

finish:
    return needsupdate;
}

/*****************************************************************************
* rebuild_loccache() rebuilds the localized resources for EFI Login
*****************************************************************************/
struct writeRsrcCtx {
    struct bootCaches *caches;
    char *locCacheDir;
    int *result;
};
void
_writeResource(const void *value, void *ctxp)
{
    int bsderr;
    CFDictionaryRef rsrc = (CFDictionaryRef)value;
    struct writeRsrcCtx *ctx = (struct writeRsrcCtx*)ctxp;
    struct bootCaches *caches = ctx->caches;

    CFDataRef data;
    void *buf;
    ssize_t bufsz;
    CFStringRef nameStr;
    int fflags, fd = -1;
    char fname[PATH_MAX], fullp[PATH_MAX];


    // extract data, filename & prepare for BSD syscalls
    bsderr = EFTYPE;
    if (!(data = CFDictionaryGetValue(rsrc, kEFILoginDataKey)))
        goto finish;
    if (!(buf = (void*)CFDataGetBytePtr(data)))
        goto finish;
    bufsz = (ssize_t)CFDataGetLength(data);
    if (bufsz < 0)      goto finish;

    if (!(nameStr = CFDictionaryGetValue(rsrc, kEFILoginFileNameKey)))
        goto finish;
    if(!CFStringGetFileSystemRepresentation(nameStr, fname, PATH_MAX))
        goto finish;
    bsderr = EOVERFLOW;
    pathcpy(fullp, ctx->locCacheDir);
    pathcat(fullp, "/");
    pathcat(fullp, fname);

    // open & write!
    fflags = O_WRONLY | O_CREAT | O_TRUNC;   // sopen() adds EXCL/NOFOL
    if (-1 == (fd = sopen(caches->cachefd, fullp, fflags, kCacheFileMode))) {
        bsderr = -1;
        goto finish;
    }
    if (write(fd, buf, bufsz) != bufsz) {
        bsderr = -1;
        goto finish;
    }

    // success
    bsderr = 0;

finish:
    if (bsderr) {
        *(ctx->result) = bsderr;
    }

    if (fd != -1)   close(fd);

    return;
}

// ahh, ye olde SysLang.h :]
// #define GLOBALPREFSFILE "/Library/Preferences/.GlobalPreferences.plist"
#define LANGSKEY    CFSTR("AppleLanguages")   // key in .GlobalPreferences
#define ENGLISHKEY  CFSTR("en")
static int
_writeEFILoginResources(struct bootCaches *caches,
                        char prefPath[PATH_MAX], struct stat *prefsb,
                        char locCacheDir[PATH_MAX])
{
    int result;         // all paths set an explicit result
    int gpfd = -1;
    CFDictionaryRef gprefs = NULL;
    CFMutableArrayRef locsList = NULL;      // retained & released
    CFStringRef volStr = NULL;
    CFArrayRef blobList = NULL;

    CFRange allEntries;
    struct writeRsrcCtx applyCtx = { caches, locCacheDir, &result };

    // can't operate without EFILogin.framework function
    // (XX as of Zin12A190, this function is not properly decorated ...)
    if (EFILoginCopyInterfaceGraphics == NULL) {
        result = ESHLIBVERS;
        goto finish;
    }

    // attempt to get AppleLanguages out of .GlobalPreferences
    if ((gpfd = sopen(caches->cachefd, prefPath, O_RDONLY, 0)) >= 0 &&
        (gprefs = copy_dict_from_fd(gpfd, prefsb)) &&
        (locsList=(CFMutableArrayRef)CFDictionaryGetValue(gprefs,LANGSKEY)) &&
        CFGetTypeID(locsList) == CFArrayGetTypeID()) {
            CFRetain(locsList);
    } else {
        // create a new array containing the default "en" (locsList !retained)
        CFRange range = { 0, 1 };
        locsList = CFArrayCreateMutable(nil, 1, &kCFTypeArrayCallBacks);
        if (!locsList) {
            result = ENOMEM;
            goto finish;
        }
        CFArrayAppendValue(locsList, ENGLISHKEY);
        if (!CFArrayContainsValue(locsList, range, ENGLISHKEY)) {
            result = ENOMEM;    // ECFFAILED :P
            goto finish;
        }
    }

    // generate all resources
    volStr = CFStringCreateWithFileSystemRepresentation(nil, caches->root);
    if (!volStr || 
            !(blobList = EFILoginCopyInterfaceGraphics(locsList, volStr))) {
        result = ENOMEM;
        goto finish;
    }

    // write everything out
    result = 0;         // applier only modifies on error
    allEntries = CFRangeMake(0, CFArrayGetCount(blobList));
    CFArrayApplyFunction(blobList, allEntries, _writeResource, &applyCtx);
    if (result)     goto finish;

    // success!
    result = 0;

finish:
    if (blobList)       CFRelease(blobList);
    if (volStr)         CFRelease(volStr);
    if (locsList)       CFRelease(locsList);
    if (gprefs)         CFRelease(gprefs);
    if (gpfd != -1)     close(gpfd);

    return result;
}

int
rebuild_loccache(struct bootCaches *caches)
{
    int errnum, result = ELAST + 1;
    struct stat cachesb, prefsb;
    char        locRsrcDir[PATH_MAX], prefPath[PATH_MAX];
    char        locCacheDir[PATH_MAX];
    time_t      validModTime = 0;
    int         fd = -1;
    struct timeval times[2];
    
    // prefsb.st_size = 0;  // Analyzer doesn't check get_locres_info(&prefsb)
    bzero(&prefsb, sizeof(prefsb)); // and doesn't know bzero sets st_size = 0
    if ((errnum = get_locres_info(caches, locRsrcDir, prefPath, &prefsb,
                                  locCacheDir, &validModTime))) {
        result = errnum; goto finish;   // error logged by function
    }

    // empty out locCacheDir ...
    /* This cache is an optional part of RPS, thus it is okay to
       destroy on failure (leaving it empty risks "right" timestamps). */
    if (sdeepunlink(caches->cachefd, locCacheDir) == -1 && errno == EROFS) {
        result = errno; LOGERRxlate(locCacheDir, NULL, result); goto finish;
    }
    if ((errnum = sdeepmkdir(caches->cachefd,locCacheDir,kCacheDirMode))) {
        result = errnum; LOGERRxlate(locCacheDir, NULL, result); goto finish;
    }

    // actually write resources!
    errnum = _writeEFILoginResources(caches, prefPath, &prefsb, locCacheDir);
    if (errnum) {
        (void)sdeepunlink(caches->cachefd, locCacheDir);
        result = errnum;
        LOGERRxlate("_writeEFILoginResources", NULL, result);
        goto finish;
    }

    // get current times (keeping access, overwriting mod)
    if ((errnum = stat(locCacheDir, &cachesb))) {
        result = errnum; LOGERRxlate(locCacheDir, NULL, result); goto finish;
    }
    cachesb.st_mtime = validModTime;
    TIMESPEC_TO_TIMEVAL(&times[0], &cachesb.st_atimespec);
    TIMESPEC_TO_TIMEVAL(&times[1], &cachesb.st_mtimespec);
    if ((errnum = _sutimes(caches->cachefd, locCacheDir, O_RDONLY, times))) {
        result = errnum; LOGERRxlate(locCacheDir, NULL, result); goto finish;
    }

    // success
    result = 0;
    
finish:
    if (fd != -1)       close(fd);

    return result;
}



/*****************************************************************************
* hasBRBoots lets you know if a volume has boot partitions and if it's on GPT
* no error reporting except residual errno
*****************************************************************************/
Boolean
hasBootRootBoots(struct bootCaches *caches, CFArrayRef *auxPartsCopy,
                         CFArrayRef *dataPartsCopy, Boolean *isAPM)
{
    CFDictionaryRef binfo = NULL;
    Boolean rval = false, apm = false;
    CFArrayRef dparts = NULL, bparts = NULL;
    char stack_bsdname[DEVMAXPATHSIZE];
    char * lookup_bsdname = caches->bsdname;
    CFArrayRef dataPartitions = NULL; // do not release;
    size_t fullLen;
    char fulldev[DEVMAXPATHSIZE];
#if DEBUG_REGISTRY
    char parentdevname[DEVMAXPATHSIZE];
    uint32_t partitionNum;
    BLPartitionType partitionType;
#endif

   /* Get the BL info about partitions & such.
    */
    if (BLCreateBooterInformationDictionary(NULL, lookup_bsdname, &binfo))
        goto finish;
    bparts = CFDictionaryGetValue(binfo, kBLAuxiliaryPartitionsKey);
    dparts = CFDictionaryGetValue(binfo, kBLDataPartitionsKey);
    if (!bparts || !dparts)     goto finish;

   /*****
    * Now, for a GPT check, use one of the data partitions given by the above
    * call to BLCreateBooterInformationDictionary().
    */
    dataPartitions = CFDictionaryGetValue(binfo, kBLDataPartitionsKey);
    if (dataPartitions && CFArrayGetCount(dataPartitions)) {
        CFStringRef dpBsdName = CFArrayGetValueAtIndex(dataPartitions, 0);

        if (dpBsdName) {
            if (!CFStringGetFileSystemRepresentation(dpBsdName, stack_bsdname,
                    sizeof(stack_bsdname)))
                goto finish;
            lookup_bsdname = stack_bsdname;
        }
    }
    
   /* Get the BL info about the partition type (that's all we use, but
    * we have to pass in valid buffer pointers for all the rest).
    */
    fullLen = snprintf(fulldev, sizeof(fulldev), "/dev/%s", lookup_bsdname);
    if (fullLen >= sizeof(fulldev)) {
        goto finish;
    }

#if DEBUG_REGISTRY
    // doesn't work on watson w/USB disk??
    if (BLGetParentDeviceAndPartitionType(NULL /* context */,
        fulldev, parentdevname, &partitionNum, &partitionType)) 
    {
        goto finish;
    }
    if (partitionType == kBLPartitionType_APM) {
        apm = true;
    }
#endif

    // 5158091 / 6413843: 10.4.x APM Apple_Boot's aren't BootRoot
    // Boot!=Root was introduced in 10.4.7 for *Intel only*.
    // BootX didn't learn about Boot!=Root until 10.5 (mkext2 era).
    // XX 10740646 tracks reviewing / dropping ppc support
    // The check is APM-only because ppc only booted APM.
    if (apm) {
        CFDictionaryRef pbDict, mk2Dict, kcDict;

        // i.e. Leopard had BootX; SnowLeopard has mkext2
        pbDict = CFDictionaryGetValue(caches->cacheinfo, kBCPostBootKey);
        if (!pbDict || CFGetTypeID(pbDict) != CFDictionaryGetTypeID())  goto finish;

        kcDict = CFDictionaryGetValue(pbDict, kBCKernelcacheV1Key);
        if (!kcDict)
            kcDict = CFDictionaryGetValue(pbDict, kBCKernelcacheV2Key);
        mk2Dict = CFDictionaryGetValue(pbDict, kBCMKext2Key);

        // if none of these indicates a more modern OS, we skip
        // XX should the ofbooter path check be != '\0' ?
        // (then we could drop the kcDict check?)
        if (!kcDict && !mk2Dict && caches->ofbooter.rpath[0] == '\0')
            goto finish;
    }

    // check for helper partitions
    rval = (CFArrayGetCount(bparts) > 0);

finish:
    // out parameters set if provided
    if (auxPartsCopy) {
        if (bparts)     CFRetain(bparts);
        *auxPartsCopy = bparts;
    }
    if (dataPartsCopy) {
        if (dparts)     CFRetain(dparts);
        *dataPartsCopy = dparts;
    }
    if (isAPM)      *isAPM = apm;

    // cleanup
    if (binfo)      CFRelease(binfo);

    return rval;
}

CFArrayRef
BRCopyActiveBootPartitions(CFURLRef volRoot)
{   
    CFArrayRef bparts, rval = NULL;
    char path[PATH_MAX], *bsdname;
    struct statfs sfs;
    CFDictionaryRef binfo = NULL;

    if (!volRoot)        goto finish;

    // get BSD Name of volRoot
    if (!CFURLGetFileSystemRepresentation(
            volRoot, false, (UInt8*)path, sizeof(path))) {
        goto finish;
    }
    if (-1 == statfs(path, &sfs))       goto finish;
    if (strlen(sfs.f_mntfromname) < sizeof(_PATH_DEV)) {
        goto finish;
    }
    bsdname = sfs.f_mntfromname + (sizeof(_PATH_DEV)-1);

    // have libbless provide the helper partitions
    // (doesn't vet them as much as hasBootRootBoots())
    if (BLCreateBooterInformationDictionary(NULL, bsdname, &binfo))
        goto finish;
    bparts = CFDictionaryGetValue(binfo, kBLAuxiliaryPartitionsKey);

    // success -> retain sub-dictionary for caller
    if (bparts && CFArrayGetCount(bparts)) {
        rval = CFRetain(bparts);
    }

finish:
    if (binfo)      CFRelease(binfo);

    return rval;
}

/*******************************************************************************
*******************************************************************************/
void _daDone(DADiskRef disk __unused, DADissenterRef dissenter, void *ctx)
{
    if (dissenter)
        CFRetain(dissenter);
    *(DADissenterRef*)ctx = dissenter;
    CFRunLoopStop(CFRunLoopGetCurrent());   // assumed okay even if not running
}

/*******************************************************************************
* We don't want to wind up invoking kextcache using assembled paths that have
* repeating slashes. Note that paths in bootcaches.plist are absolute so
* appending them should always put a slash in as expected.
*******************************************************************************/
static void removeTrailingSlashes(char * path)
{
    size_t pathLength = strlen(path);
    size_t scanIndex = pathLength - 1;

    if (!pathLength) return;

    while (path[scanIndex] == '/') {
        path[scanIndex] = '\0';
        if (scanIndex == 0)   break;
        scanIndex--;
    }

    return;
}



/******************************************************************************
 * updateMount() remounts the volume with the requested flags!
 *****************************************************************************/
int
updateMount(mountpoint_t mount, uint32_t mntgoal)
{
    int result = ELAST + 1;         // 3/22/12: all paths set result
    DASessionRef session = NULL;
    CFStringRef toggleMode = CFSTR("updateMountMode");
    CFURLRef volURL = NULL;
    DADiskRef disk = NULL;
    DADissenterRef dis = (void*)kCFNull;
    CFStringRef mountargs[] = {
            CFSTR("update"), 
       ( mntgoal & MNT_NODEV          ) ? CFSTR("nodev")    : CFSTR("dev"),
       ( mntgoal & MNT_NOEXEC         ) ? CFSTR("noexec")   : CFSTR("exec"),
       ( mntgoal & MNT_NOSUID         ) ? CFSTR("nosuid")   : CFSTR("suid"),
       ( mntgoal & MNT_RDONLY         ) ? CFSTR("rdonly")   : CFSTR("rw"),
       ( mntgoal & MNT_DONTBROWSE     ) ? CFSTR("nobrowse") : CFSTR("browse"),
       (mntgoal & MNT_IGNORE_OWNERSHIP) ? CFSTR("noowners") : CFSTR("owners"),
       NULL };

    // same 'dis' logic as mountBoot in update_boot.c
    if (!(session = DASessionCreate(nil))) {
        result = ENOMEM; goto finish;
    }
    DASessionScheduleWithRunLoop(session, CFRunLoopGetCurrent(), toggleMode);
    if (!(volURL=CFURLCreateFromFileSystemRepresentation(nil, (void*)mount,
                                                   strlen(mount), true))) {
        result = ENOMEM; goto finish;
    }
    if (!(disk = DADiskCreateFromVolumePath(nil, session, volURL))) {
        result = ENOMEM; goto finish;
    }
    DADiskMountWithArguments(disk, NULL, kDADiskMountOptionDefault, _daDone,
                             &dis, mountargs);

    while (dis == (void*)kCFNull) {
        CFRunLoopRunInMode(toggleMode, 0, true);    // _daDone updates 'dis'
    }
    if (dis) {
        result = DADissenterGetStatus(dis);     // XX errno |= unix_err()
        if (result == 0)    result = ELAST + 1;
        goto finish;
    }

    result = 0;

finish:
    if (dis && dis != (void*)kCFNull)   CFRelease(dis);
    if (disk)                           CFRelease(disk);
    if (session)                        CFRelease(session);
    if (volURL)                         CFRelease(volURL);

    if (result) {
        OSKextLog(NULL, kOSKextLogWarningLevel | kOSKextLogFileAccessFlag,
            "Warning: couldn't update %s->f_flags to %#x: error %#x", mount,
            mntgoal, result);
    }

    return result;
}

/******************************************************************************
 * returns the result of fork/exec (negative on error; pid on success)
 * a (waited-for) helper exit status will also be returned (see fork_program.c)
 * - 'force' -> -f to ignore bootstamps (13784516 removed only use)
 *****************************************************************************/
// kextcache -u helper sets up argv
pid_t launch_rebuild_all(char * rootPath, Boolean force, Boolean wait)
{
    pid_t rval = -1;
    int argc, argi = 0; 
    char **kcargs = NULL;

    //  argv[0] '-F'  '-u'  root          -f ?       NULL
    argc =  1  +  1  +  1  +  1  + (force == true) +  1;
    kcargs = malloc(argc * sizeof(char*));
    if (!kcargs)    goto finish;

    kcargs[argi++] = "/usr/sbin/kextcache";
    // fork_program(wait=false) also sets IOPOL_THROTTLE while spawning
    kcargs[argi++] = "-F";      // lower priority within kextcache
    if (force) {
        kcargs[argi++] = "-f";
    }
    kcargs[argi++] = "-u";
    kcargs[argi++] = rootPath;
    // kextcache reads bc.plist so nothing more needed

    kcargs[argi] = NULL;    // terminate the list

   /* wait:false means the return value is <0 for fork/exec failures and
    * the pid of the forked process if >0.
    */
    rval = fork_program(kcargs[0], kcargs, wait);

finish:
    if (kcargs)     free(kcargs);

    if (rval < 0)
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
            "Error launching kextcache -u.");

    return rval;
}

/*******************************************************************************
*******************************************************************************/
struct nameAndUUID {
    uint32_t nbytes;
    struct attrreference nameref;
    uuid_t uuid;
    char namedata[NAME_MAX+1];
};
int
copyVolumeInfo(const char *vol_path, uuid_t *vol_uuid, CFStringRef *cslvf_uuid,
               char vol_bsd[DEVMAXPATHSIZE], char vol_name[NAME_MAX])
{
    int bsderr, rval = ENODEV;
    struct nameAndUUID attrs;
    struct attrlist attrdesc = { ATTR_BIT_MAP_COUNT, 0, 0, ATTR_VOL_INFO |
                                 ATTR_VOL_NAME | ATTR_VOL_UUID, 0, 0, 0 };
    struct statfs sfs;
    char *bsdname;
    io_object_t ioObj = IO_OBJECT_NULL;
    CFTypeRef regEntry = NULL;

    // get basic data
    // (don't worry about FSOPT_REPORT_FULLSIZE; NAME_MAX+1 is plenty :]
    if ((bsderr=getattrlist(vol_path, &attrdesc, &attrs, sizeof(attrs), 0))
            || attrs.nbytes >= sizeof(attrs)) {
        rval = errno; goto finish;
    }
    if (vol_bsd || cslvf_uuid) {
        if ((bsderr = statfs(vol_path, &sfs))) {
            rval = errno; goto finish;
        }
        bsdname = sfs.f_mntfromname;
        if (strncmp(bsdname, _PATH_DEV, strlen(_PATH_DEV)) == 0) {
            bsdname += strlen(_PATH_DEV);
        }
    }


    // handle UUID if requested
    if (vol_uuid) {
        memcpy(*vol_uuid, attrs.uuid, sizeof(uuid_t));
    }

    // CoreStorage UUID if requested
    if (cslvf_uuid) {
        CFDictionaryRef matching;   // IOServiceGetMatchingServices() releases
        matching = IOBSDNameMatching(kIOMasterPortDefault, 0, bsdname);
        if (!matching) {
            rval = ENOMEM; goto finish;
        }
        ioObj = IOServiceGetMatchingService(kIOMasterPortDefault, matching);
        matching = NULL;        // IOServiceGetMatchingService() released
        if (ioObj == IO_OBJECT_NULL) {
            rval = ENODEV; goto finish;
        }
        regEntry = IORegistryEntryCreateCFProperty(ioObj,
                                    CFSTR(kCoreStorageLVFUUIDKey), nil, 0);
        if (regEntry && CFGetTypeID(regEntry) == CFStringGetTypeID()) {
            // retain the result (regEntry released below)
            *cslvf_uuid = (CFStringRef)CFRetain(regEntry);
        } else {
            *cslvf_uuid = NULL;
        }
    }

    // BSD Name
    if (vol_bsd) {
        if (strlcpy(vol_bsd, bsdname, DEVMAXPATHSIZE) >= DEVMAXPATHSIZE) {
            rval = EOVERFLOW; goto finish;
        }
    }

    // volume name
    if (vol_name) {
        char *volname = (char*)&attrs.nameref + attrs.nameref.attr_dataoffset;
        (void)strlcpy(vol_name, volname, NAME_MAX);
    }

    rval = 0;

finish:
    if (rval) {
        OSKextLog(NULL, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                  "%s: %s", vol_path, strerror(rval));
    }

    if (regEntry)                   CFRelease(regEntry);
    if (ioObj != IO_OBJECT_NULL)    IOObjectRelease(ioObj);
    // matching consumed by IOServiceGetMatchingService()

    return rval;
}
