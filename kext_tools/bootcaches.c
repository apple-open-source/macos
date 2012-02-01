/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <EFILogin/EFILogin.h>
#include <System/libkern/mkext.h>
#include <System/libkern/OSKextLibPrivate.h>
#include <DiskArbitration/DiskArbitration.h>        // for UUID fetching
#include <DiskArbitration/DiskArbitrationPrivate.h> // path -> DADisk
#include <IOKit/kext/fat_util.h>
#include <IOKit/kext/macho_util.h>
#include <IOKit/storage/CoreStorage/CoreStorageUserLib.h>
#include <IOKit/storage/CoreStorage/CoreStorageCryptoIDs.h>
#include <IOKit/storage/CoreStorage/CSFullDiskEncryption.h>

#include "fork_program.h"
#include "bootcaches.h"     // includes CF

#include <IOKit/kext/OSKext.h>
#include <IOKit/kext/OSKextPrivate.h>

// bad! don't use kextd files in shared source
#include "kextd_globals.h"

#include "safecalls.h"
#include "kext_tools_util.h"


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


/******************************************************************************
* destroyCaches cleans up a bootCaches structure
******************************************************************************/
void destroyCaches(struct bootCaches *caches)
{
    if (caches->cachefd != -1)  close(caches->cachefd);
    if (caches->cacheinfo)      CFRelease(caches->cacheinfo);
    if (caches->miscpaths)      free(caches->miscpaths);  // free all strings
    if (caches->rpspaths)       free(caches->rpspaths);
    if (caches->csfde_uuid)     CFRelease(caches->csfde_uuid);
    free(caches);
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

// parse bootcaches.plist and dadisk dictionaries into passed struct
// caller populates fields it needed to load the plist
// and properly frees the structure if we fail
static int
extractProps(struct bootCaches *caches, CFDictionaryRef bcDict,
             CFDictionaryRef ddesc, char **errmsg)
{
    int rval = ELAST + 1;
    CFDictionaryRef dict;   // don't release
    CFIndex keyCount;       // track whether we've handled all keys
    CFIndex rpsindex = 0;   // index into rps; compared to caches->nrps @ end
    CFStringRef str;        // used to point to objects owned by others
    CFStringRef createdStr = NULL;
    CFUUIDRef uuid;

    *errmsg = "error getting disk metadata";
    // volume UUID, name, bsdname
    if (!(uuid = CFDictionaryGetValue(ddesc, kDADiskDescriptionVolumeUUIDKey)))
        goto finish;
    if (!(createdStr = CFUUIDCreateString(nil, uuid)))
        goto finish;
     if (!CFStringGetFileSystemRepresentation(createdStr,caches->fsys_uuid,NCHARSUUID))
        goto finish;

    if (!(str = CFDictionaryGetValue(ddesc, kDADiskDescriptionVolumeNameKey)))
        goto finish;
    if (!CFStringGetFileSystemRepresentation(str, caches->volname, NAME_MAX))
        goto finish;

    // bsdname still needed for bless
    if (!(str = CFDictionaryGetValue(ddesc, kDADiskDescriptionMediaBSDNameKey)))
        goto finish;
    if (!CFStringGetFileSystemRepresentation(str, caches->bsdname, NAME_MAX))
        goto finish;

    *errmsg = "unrecognized bootcaches.plist data"; // covers the rest
    keyCount = CFDictionaryGetCount(bcDict);        // start with the top

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
            if (CFDictionaryGetValue(erDict,kBCCSFDELocalizedResourcesCache)) {
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
        // two sub-keys required; a set of another three are optional
        if (erDict) {
            keyCount += CFDictionaryGetCount(erDict);

            str = CFDictionaryGetValue(erDict, kBCCSFDEPropertyCache);
            if (str) {
                MAKE_CACHEDPATH(&caches->rpspaths[rpsindex], caches, str);
                caches->erpropcache = &caches->rpspaths[rpsindex++];
                keyCount--;
            }

            // 8163405: non-localized resources
            str = CFDictionaryGetValue(erDict, kBCCSFDEDefaultResourcesDir);
            // XX check old key name for now
            if (!str) str=CFDictionaryGetValue(erDict,CFSTR("ResourcesDir")); 
            if (str) {
                MAKE_CACHEDPATH(&caches->rpspaths[rpsindex], caches, str);
                caches->efidefrsrcs = &caches->rpspaths[rpsindex++];
                keyCount--;
            }
            
            // localized resource cache
            str = CFDictionaryGetValue(erDict,kBCCSFDELocalizedResourcesCache);
            if (str) {
                MAKE_CACHEDPATH(&caches->rpspaths[rpsindex], caches, str);
                caches->efiloccache = &caches->rpspaths[rpsindex++];
                keyCount--;
                
                // localization source material (required)
                str = CFDictionaryGetValue(erDict, kBCCSFDELocalizationSource);
                if (str && CFGetTypeID(str) == CFStringGetTypeID() &&
                        CFStringGetFileSystemRepresentation(str,
                            caches->locSource, sizeof(caches->locSource))) {
                    keyCount--;
                } else {
                    goto finish;
                }
                
                // localization prefs file (required)
                str = CFDictionaryGetValue(erDict, kBCCSFDELanguagesPref);
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

        if (kcacheKeys > 1) { 
            // big fat error
            *errmsg = "multiple cache keys found";
            goto finish;
        }

      /* Handle the "Kernelcache" key for prelinked kernels for Lion and
       * later, the "MKext2 key" for format-2 mkext on Snow Leopard, and the
       * original "MKext" key for format-1 mkexts prior to SnowLeopard.
       */
        do {
            mkDict = (CFDictionaryRef)CFDictionaryGetValue(dict, kBCKernelcacheV1Key);
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
            str = (CFStringRef)CFDictionaryGetValue(mkDict, kBCPathKey);
            MAKE_CACHEDPATH(&caches->rpspaths[rpsindex], caches, str);   // M
            caches->kext_boot_cache_file = &caches->rpspaths[rpsindex++];
#pragma unused(rpsindex)

            // get the Extensions folder path and set up exts by hand
            str = (CFStringRef)CFDictionaryGetValue(mkDict, kBCExtensionsDirKey);
            if (!str || CFGetTypeID(str) != CFStringGetTypeID()) {
                goto finish;
            }
            if (!CFStringGetFileSystemRepresentation(str, caches->exts, 
                sizeof(caches->exts))) {
                goto finish;
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

    if (keyCount || (unsigned)rpsindex != caches->nrps) {
        *errmsg = "unrecognized bootcaches.plist data; skipping";
        errno = EINVAL;
    } else {
        // hooray
        *errmsg = NULL;
        rval = 0;
        caches->cacheinfo = CFRetain(bcDict);   // for archs, etc
    }

finish:
    if (createdStr)     CFRelease(createdStr);

    return rval;
}

// create cache dirs as needed
// readCaches_internal() calls on most volumes.
static int
createCacheDirs(struct bootCaches *caches)
{
    int errnum;
    char *errmsg;
    struct stat sb;
    char cachedir[PATH_MAX], uuiddir[PATH_MAX];      // bootstamps, csfde

    // bootstamps directory
    // (always made because it's used by libbless on non-BootRoot for ESP)
    errmsg = "error creating " kTSCacheDir;
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
                goto finish;
            }
        } else {
            goto finish;
        }
    }

    // create CoreStorage cache directories if appropriate
    if (caches->erpropcache) {
        errmsg = "error creating encrypted root property cache dir";
        pathcpy(cachedir, caches->root);
        pathcat(cachedir, dirname(caches->erpropcache->rpath));
        if ((errnum = stat(cachedir, &sb))) {
            if (errno == ENOENT) {
                // s..mkdir ensures cachedir is on the same volume
                errnum=sdeepmkdir(caches->cachefd,cachedir,kCacheDirMode);
                if (errnum)         goto finish;
            } else {
                goto finish;
            }
        }
    }

    if (caches->efiloccache) {
        errmsg = "error creating localized resources cache dir";
        pathcpy(cachedir, caches->root);
        pathcat(cachedir, caches->efiloccache->rpath);
        if ((errnum = stat(cachedir, &sb))) {
            if (errno == ENOENT) {
                // s..mkdir ensures cachedir is on the same volume
                errnum=sdeepmkdir(caches->cachefd,cachedir,kCacheDirMode);
                if (errnum)         goto finish;
            } else {
                goto finish;
            }
        }
    }

    // success if the last assignment to errnum was 0
    errmsg = NULL;

// XX need to centralize this sort of error decoding (w/9217695?)
finish:
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
        // so kextcache -u doesn't claim bootcaches.plist didn't exist, etc
        errno = 0;
    }

    return errnum;
}

static CFDictionaryRef
copy_dict_from_fd(int fd, struct stat *sb)
{
    CFDictionaryRef rval = NULL;
    void *buf = NULL;
    CFDataRef data = NULL;
    CFDictionaryRef dict = NULL;

    // read the plist
    if (sb->st_size > UINT_MAX || sb->st_size > LONG_MAX)     goto finish;
    if (!(buf = malloc((size_t)sb->st_size)))              goto finish;
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
    if (buf)  free(buf);

    return rval;
}

static struct bootCaches*
readCaches_internal(CFURLRef volURL, DADiskRef dadisk)
{
    struct bootCaches *rval = NULL, *caches = NULL;
    char *errmsg;
    int errnum = 4;
    struct stat sb;
    CFDictionaryRef ddesc = NULL;

    char bcpath[PATH_MAX];
    CFDictionaryRef bcDict = NULL;
    struct statfs rootsfs;
    io_object_t ioObj = IO_OBJECT_NULL;
    CFTypeRef regEntry = NULL;

    errmsg = "allocation failure";
    caches = calloc(1, sizeof(*caches));
    if (!caches)            goto finish;
    caches->cachefd = -1;       // set cardinal (fd 0 valid)
    (void)strlcpy(caches->root, "<unknown>", sizeof(caches->root));

    errmsg = "error extracting volume's root path";
    if (!CFURLGetFileSystemRepresentation(volURL, false, 
        (UInt8*)caches->root, sizeof(caches->root))) {
        goto finish;
    }


    errmsg = "error reading " kBootCachesPath;
    pathcpy(bcpath, caches->root);
    pathcat(bcpath, kBootCachesPath);
    // Sec: cachefd lets us validate data, operations
    caches->cachefd = (errnum = open(bcpath, O_RDONLY|O_EVTONLY));
    if (errnum == -1) {
        if (errno == ENOENT) {
            // special case logged by kextcache -u
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

    // plist -> dictionary
    errmsg = "trouble reading " kBootCachesPath;
    bcDict = copy_dict_from_fd(caches->cachefd, &sb);
    if (!bcDict)        goto finish;

    // get uuid of CoreStorage volume in case it is or becomes encrypted
    errmsg = "error obtaining CoreStorage volume's UUID";
    if (IO_OBJECT_NULL == (ioObj = DADiskCopyIOMedia(dadisk)))
        goto finish;
    regEntry = IORegistryEntryCreateCFProperty(ioObj,
                                CFSTR(kCoreStorageLVFUUIDKey), nil, 0);
    if (regEntry && CFGetTypeID(regEntry) == CFStringGetTypeID()) {
        // retain the result (regEntry released below)
        caches->csfde_uuid = (CFStringRef)CFRetain(regEntry);
    }

    // extractProps() will copy the properties into the structure
    if (!(ddesc = DADiskCopyDescription(dadisk)))
        goto finish;
    if (extractProps(caches, bcDict, ddesc, &errmsg)) {
        goto finish;
    }

    // root proactively creates caches directories if missing
    // don't bother if owners are ignored (6206867)
    if (geteuid() == 0 && (rootsfs.f_flags & MNT_IGNORE_OWNERSHIP) == 0 &&
            (rootsfs.f_flags & MNT_RDONLY) == 0) {
        errmsg = NULL;      // logs its own errors
        errnum = createCacheDirs(caches);
        if (errnum)     goto finish;
    }


    // success!
    rval = caches;
    errmsg = NULL;

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
        // so kextcache -u doesn't claim bootcaches.plist didn't exist, etc
        errno = 0;
    }

    // clean up (unwind in reverse order of allocation)
    if (ddesc)      CFRelease(ddesc);
    if (bcDict)     CFRelease(bcDict);  // extractProps() retains for struct
    if (regEntry)   CFRelease(regEntry);
    if (ioObj != IO_OBJECT_NULL)
        IOObjectRelease(ioObj);

    // if things went awry, free anything associated with 'caches'
    if (!rval) {
        destroyCaches(caches);      // closes cachefd if needed
    }

    return rval;
}

/*******************************************************************************
*******************************************************************************/
struct bootCaches *
readBootCachesForVolURL(CFURLRef volumeURL)
{
    struct bootCaches    * result       = NULL;
    DASessionRef           daSession    = NULL;  // must release
    DADiskRef              dadisk       = NULL;  // must release

    daSession = DASessionCreate(kCFAllocatorDefault);
    if (!daSession) {
        OSKextLog(NULL, kOSKextLogErrorLevel | kOSKextLogIPCFlag,
                  "couldn't get description from Disk Arbitration");
        goto finish;
    }

    dadisk =DADiskCreateFromVolumePath(kCFAllocatorDefault,daSession,volumeURL);
    if (!dadisk) {
        goto finish;
    }

    result = readCaches_internal(volumeURL, dadisk);

finish:
    SAFE_RELEASE(dadisk);
    SAFE_RELEASE(daSession);

    return result;
}

struct bootCaches*
readBootCachesForDADisk(DADiskRef dadisk)
{
    struct bootCaches *rval = NULL;
    CFDictionaryRef ddesc = NULL;
    CFURLRef volURL = NULL;     // owned by dict; don't release
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
    } while (++ntries < kKextdDiskArbMaxRetries);

    if (!volURL) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Disk description missing mount point for %d tries",
            ntries);
        goto finish;
    }

    if (ntries) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogFileAccessFlag,
            "Warning: readCaches got mount point after %d tries.", ntries);
    }

    rval = readCaches_internal(volURL, dadisk);

finish:
    // clean up (unwind in allocation order)
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
Boolean needUpdates(struct bootCaches *caches, Boolean *rps, Boolean *booters,
                    Boolean *misc, OSKextLogSpec oodLogSpec)
{
    Boolean rpsOOD, bootersOOD, miscOOD, anyOOD;
    cachedPath *cp;

    // assume nothing needs updating (caller may interpret error -> needsUpdate)
    rpsOOD = bootersOOD = miscOOD = anyOOD = false;

    for (cp = caches->rpspaths; cp < &caches->rpspaths[caches->nrps]; cp++) {
        if (needsUpdate(caches->root, cp)) {
            OSKextLog(NULL, oodLogSpec, "%s out of date.", cp->rpath);
            anyOOD = rpsOOD = true;
        }
    }
    if ((cp = &(caches->efibooter)), cp->rpath[0]) {
        if (needsUpdate(caches->root, cp)) {
            OSKextLog(NULL, oodLogSpec, "%s out of date.", cp->rpath);
            anyOOD = bootersOOD = true;
        }
    }
    if ((cp = &(caches->ofbooter)), cp->rpath[0]) {
        if (needsUpdate(caches->root, cp)) {
            OSKextLog(NULL, oodLogSpec, "%s out of date.", cp->rpath);
            anyOOD = bootersOOD = true;
        }
    }
    for (cp = caches->miscpaths; cp < &caches->miscpaths[caches->nmisc]; cp++) {
        if (needsUpdate(caches->root, cp)) {
            OSKextLog(NULL, oodLogSpec, "%s out of date.", cp->rpath);
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
                  "sopen(%s): %s", path, strerror(errno));
        goto finish;        
    }
    if ((bsderr = futimes(fd, times))) {
        OSKextLog(NULL, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                  "futimes(<%s>): %s", path, strerror(errno));
    }

finish:
    if (fd == -1)   close(fd);

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
    int rval = 0;
    cachedPath *cp;
    struct stat sb;

    // allow known commands throug
    switch (command) {
        case kBCStampsApplyTimes:
        case kBCStampsUnlinkOnly:
            break;

        default:
            return EINVAL;
    }

    // run through all of the cached paths apply bootstamp
    for (cp = caches->rpspaths; cp < &caches->rpspaths[caches->nrps]; cp++) {
        rval |= updateStamp(caches->root, cp, caches->cachefd, command);
    }
    if ((cp = &(caches->efibooter)), cp->rpath[0]) {
        rval |= updateStamp(caches->root, cp, caches->cachefd, command);
    }
    if ((cp = &(caches->ofbooter)), cp->rpath[0]) {
        rval |= updateStamp(caches->root, cp, caches->cachefd, command);
    }
    for (cp = caches->miscpaths; cp < &caches->miscpaths[caches->nmisc]; cp++){
        rval |= updateStamp(caches->root, cp, caches->cachefd, command);
    }

    // clean shutdown should make sure these stamps are on disk, but until
    // the root cause of 8603195 is found, we're not risking anything.
    /* ??: if (stat(/AppleInternal, &sb) == 0 && 
                    stat(/var/db/disableAppleInternal, &sb) != 0) */
    if (stat(BRDBG_DISABLE_EXTSYNC_F, &sb) == -1) {
        rval |= fcntl(caches->cachefd, F_FULLFSYNC);
    }

    return rval;
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
    char            fullextsp[PATH_MAX]   = "";
    char            fullkernelp[PATH_MAX] = "";
    Boolean         generateKernelcache     = false;
    int             mkextVersion            = 0;

    // bootcaches.plist might not request mkext/kernelcache rebuilds
    if (!caches->kext_boot_cache_file
    ) {
       goto finish;
    }

    pbDict = CFDictionaryGetValue(caches->cacheinfo, kBCPostBootKey);
    if (!pbDict || CFGetTypeID(pbDict) != CFDictionaryGetTypeID())  goto finish;

   /* Try for a Kernelcache key, and if there isn't one, look for an "MKext" key.
    */
    do {
        mkDict = CFDictionaryGetValue(pbDict, kBCKernelcacheV1Key);
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
    argc =  1       + (narchs*2) + 1 + 1  + 1  + 1     + 1  + 1    + 1           + 1  + 1    + 1    + 1;
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

    pathcpy(fullextsp, caches->root);
    removeTrailingSlashes(fullextsp);
    pathcat(fullextsp, caches->exts);
    kcargs[argi++] = fullextsp;

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

   /* wait:false means the return value is <0 for fork/exec failures and
    * the pid of the forked process if >0.
    *
    * wait:true means the return value is <0 for fork/exec failures and
    * the exit status of the forked process (>=0) otherwise.
    */
    pid = fork_program("/usr/sbin/kextcache", kcargs, wait);  // logs its own errors

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
    Boolean      needsrebuild                     = false;
    char         full_cache_file_path[PATH_MAX] = "";
    char         fullextsp[PATH_MAX]              = "";
    char         fullkernelp[PATH_MAX]            = "";
    struct stat  extsb;
    struct stat  kernelsb;
    struct stat  sb;
    time_t       validModtime;

   /* Do we have a cache file (mkext or kernelcache)?
    * Note: cache_path is a pointer field, not a static array.
    */
    if (cache_path) {
    
       /* If so, check the mod time of the cache file vs. the extensions folder.
        */
        // struct bootCaches paths are all *relative*
        pathcpy(full_cache_file_path, caches->root);
        removeTrailingSlashes(full_cache_file_path);
        pathcat(full_cache_file_path, cache_path);

        pathcpy(fullextsp, caches->root);
        removeTrailingSlashes(fullextsp);
        pathcat(fullextsp, caches->exts);

        if (stat(fullextsp, &extsb) == -1) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogWarningLevel | kOSKextLogFileAccessFlag,
                "Warning: %s: %s", fullextsp, strerror(errno));
            // assert(needsrebuild == false);   // we can't build w/o exts
            goto finish;
        }

        validModtime = extsb.st_mtime + 1;
        
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
            * Extensions folder or the kernel.
            */
            if (kernelsb.st_mtime > extsb.st_mtime) {
                validModtime = kernelsb.st_mtime + 1;
            }
        }

        // The cache file itself
        needsrebuild = true;  // since this stat() will fail if cache file is gone
        if (stat(full_cache_file_path, &sb) == -1) {
            goto finish;
        }
        needsrebuild = (sb.st_mtime != validModtime);
    }

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
// We need both routines because kextd only checks while kextcache 
// checks and rebuilds.

// caller must release econtext on success
static int
copy_csfde_props(CFStringRef uuidStr,
                 CFDictionaryRef *encContext,
                 int64_t *timeStamp)
{
    int             rval = ELAST + 1;
    CFDictionaryRef lvfprops = NULL, ectx = NULL;
    CFNumberRef     propStampRef;
    int64_t         propStamp;

    if (!uuidStr)   goto finish;

    // 04/25/11 - gab: <rdar://problem/9168337>
    // can't operate without libCoreStorage func
    if (CoreStorageCopyFamilyProperties == NULL) {
        rval = ESHLIBVERS;
        goto finish;
    }

    lvfprops = CoreStorageCopyFamilyProperties(uuidStr);
    if (!lvfprops)  goto finish;

    ectx = (CFMutableDictionaryRef)CFDictionaryGetValue(lvfprops,
                        CFSTR(kCoreStorageFamilyEncryptionContextKey));
    if (!ectx || CFGetTypeID(ectx) != CFDictionaryGetTypeID())
        goto finish;

    // get properties' timestamp
    propStampRef = CFDictionaryGetValue(ectx, CFSTR(kCSFDELastUpdateTime));
    if (propStampRef) {
        if (CFGetTypeID(propStampRef) != CFNumberGetTypeID())
            goto finish;
        if (!CFNumberGetValue(propStampRef, kCFNumberSInt64Type, &propStamp))
            goto finish;
    } else {
        propStamp = 0LL;
    }

    rval = 0;

    if (encContext) *encContext = CFRetain(ectx);
    if (timeStamp)  *timeStamp = propStamp;

finish:
    if (lvfprops)   CFRelease(lvfprops);

    return rval;
}

Boolean
check_csfde(struct bootCaches *caches)
{
    Boolean         encrypted, needsupdate = false;
    CFDictionaryRef ectx = NULL;
    CFArrayRef      eusers;     // owned by ectx

    int64_t         propStamp, erStamp;
    char            erpath[PATH_MAX];
    struct stat     ersb;

    if (caches->csfde_uuid == NULL || !caches->erpropcache)
        goto finish;

    if (copy_csfde_props(caches->csfde_uuid, &ectx, &propStamp))
        goto finish;

    // does it have encrypted users?
    eusers = (CFArrayRef)CFDictionaryGetValue(ectx, CFSTR(kCSFDECryptoUsersID));
    encrypted = (eusers && CFArrayGetCount(eusers));

    if (!encrypted)
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

    // the timestamp should only advance, but we'll count != as out of date
    needsupdate = erStamp != propStamp;

finish:
    if (ectx)       CFRelease(ectx);

    return needsupdate;
}

// XX after 10376911 is in, remove this declaration from /trunk
bool CSFDEWritePropertyCacheToFD(CFDictionaryRef context, int fd, CFStringRef wipeKeyUUID);

int
rebuild_csfde_cache(struct bootCaches *caches)
{
    int             rval = ELAST + 1;
    CFDictionaryRef ectx = NULL;
    int64_t         propStamp;
    char           *errmsg = NULL;
    char            erpath[PATH_MAX];
    int             erfd = -1;
    CFArrayRef      dataVolumes = NULL;
    CFStringRef     wipeKeyUUID = NULL;

    // 04/25/11 - gab: <rdar://problem/9168337>
    // guard against calling unavailable weak-linked symbols
    if (CoreStorageCopyPVWipeKeyUUID==NULL || CSFDEInitPropertyCache==NULL){
        errmsg = "CoreStorage library symbols unavailable";
        errno = ESHLIBVERS;
        goto finish;
    }
    
    (void)hasBootRootBoots(caches, NULL, &dataVolumes, NULL);
    if (!dataVolumes) {
        errmsg = "no data partitions! (for Wipe Key)";
        goto finish;
    }

    // only handle one PV initially
    CFIndex     count = CFArrayGetCount(dataVolumes);
    if (count > 1) {
        errmsg = "more than one physical volume encountered";
        goto finish;
    } else if (count == 0) {
        errmsg = "no data partitions! (for Wipe Key)";
        goto finish;
    }

    CFStringRef physicalVolume = CFArrayGetValueAtIndex(dataVolumes, 0);
    char        bsdName[DEVMAXPATHSIZE];

    if (!CFStringGetCString(physicalVolume, bsdName, sizeof(bsdName), kCFStringEncodingUTF8)) {
        errmsg = "String conversion failure for bsdName.";
        goto finish;
    }

    wipeKeyUUID = CoreStorageCopyPVWipeKeyUUID(bsdName);
    if (!wipeKeyUUID) {
        errmsg = "error getting volume wipe key";
        goto finish;
    }

    errmsg = "error getting encryption context data";
    if (!caches->csfde_uuid || !caches->erpropcache)
        goto finish;
    if (copy_csfde_props(caches->csfde_uuid, &ectx, &propStamp))
        goto finish;

    // build /<vol>/S/L/Caches/..corestorage/EncryptedRoot.plist.wipekey
    errmsg = "error building encryption context cache file path";
    pathcpy(erpath, caches->root);
    pathcat(erpath, caches->erpropcache->rpath);
    errmsg = NULL;

    // zero any existing file, sopen(), and write to validated fd
    // (ENOENT ignored by szerofile())
    if (szerofile(caches->cachefd, erpath)) {
        OSKextLog(NULL, kOSKextLogWarningLevel | kOSKextLogFileAccessFlag,
                  "%s: %s", erpath, strerror(errno));
    }
    (void)sunlink(caches->cachefd, erpath);
    if(-1==(erfd=sopen(caches->cachefd,erpath,O_CREAT|O_RDWR,kCacheFileMode))){
        OSKextLog(NULL, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                  "%s: %s", erpath, strerror(errno));
        rval = errno;
        goto finish;
    }
    if (!CSFDEWritePropertyCacheToFD(ectx, erfd, wipeKeyUUID)) {
        OSKextLog(NULL, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                  "CSFDEWritePropertyCacheToFD(%s) failed", erpath);
        goto finish;
    }

    // success
    errmsg = NULL;
    rval = 0;

finish:
    if (erfd != -1)
        close (erfd);
    if (dataVolumes)
        CFRelease(dataVolumes);
    if (wipeKeyUUID)
        CFRelease(wipeKeyUUID);
    if (ectx)
        CFRelease(ectx);

    if (errmsg) {
        if (rval == -1) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "%s: %s", errmsg, strerror(errno));
        } else {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "%s: %s", caches->root, errmsg);
        }
    }
    
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
        rval = EINVAL;
        goto finish;
    }
    
    // build localization sources directory path
    pathcpy(locRsrcDir, caches->root);
    pathcat(locRsrcDir, caches->locSource);
    // get localization sources directory timestamp
    if (stat(locRsrcDir, &sb)) {
        OSKextLog(NULL, kOSKextLogWarningLevel | kOSKextLogFileAccessFlag,
                  "%s: %s", locRsrcDir, strerror(errno));
        rval = errno;
        goto finish;
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
            OSKextLog(NULL, kOSKextLogWarningLevel | kOSKextLogFileAccessFlag,
                      "%s: %s", prefPath, strerror(errno));
            rval = errno;
            goto finish;
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
    if (rval == EOVERFLOW) {
        OSKextLog(NULL, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                  "get_locres_info(%s): %s", caches->root, strerror(rval));
    }
    return rval;
}

Boolean
check_loccache(struct bootCaches *caches)
{
    Boolean     needsupdate = false;
    struct stat prefsb, cachesb;
    char        locRsrcDir[PATH_MAX], prefPath[PATH_MAX];
    char        locCacheDir[PATH_MAX];
    time_t      validModTime;

    if (!caches->efiloccache) {
        // no loc'd cache dir -> needsupdate defaults to "nope"
        goto finish;
    }

    // logs its own errors
    if (get_locres_info(caches, locRsrcDir, prefPath, &prefsb,
                        locCacheDir, &validModTime)) {
        // can't get info -> needsupdate defaults to "nope"
        goto finish;
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

// ahh, ye olde SysLang.h :)
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
        if (!locsList)      goto finish;
        CFArrayAppendValue(locsList, ENGLISHKEY);
        if (!CFArrayContainsValue(locsList, range, ENGLISHKEY)) goto finish;
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
    int result;
    struct stat cachesb, prefsb;
    char        locRsrcDir[PATH_MAX], prefPath[PATH_MAX];
    char        locCacheDir[PATH_MAX];
    time_t      validModTime;
    int         fd = -1;
    struct timeval times[2];
    
    // logs its own errors
    if ((result = get_locres_info(caches, locRsrcDir, prefPath, &prefsb,
                             locCacheDir, &validModTime))) {
        goto finish;;
    }

    // empty out locCacheDir ...
    /* This cache is an optional part of RPS, thus it is okay to
       destroy on failure (leaving empty risks "right" timestamps). */
    (void)sdeepunlink(caches->cachefd, locCacheDir);
    if ((result = sdeepmkdir(caches->cachefd,locCacheDir,kCacheDirMode)))
        goto finish;

    // actually write resources!
    result = _writeEFILoginResources(caches, prefPath, &prefsb, locCacheDir);
    if (result) {
        OSKextLog(NULL, kOSKextLogErrorLevel|kOSKextLogFileAccessFlag,
                  "_writeEFILoginResources() failed: %d",
                  (result == -1) ? errno : result);
        (void)sdeepunlink(caches->cachefd, locCacheDir);
        goto finish;
    }

    // get current times (keeping access, overwriting mod)
    if ((result = stat(locCacheDir, &cachesb))) {
        OSKextLog(NULL, kOSKextLogWarningLevel|kOSKextLogFileAccessFlag,
                  "%s: %s", locCacheDir, strerror(errno));
        goto finish;
    }
    cachesb.st_mtime = validModTime;
    TIMESPEC_TO_TIMEVAL(&times[0], &cachesb.st_atimespec);
    TIMESPEC_TO_TIMEVAL(&times[1], &cachesb.st_mtimespec);
    result = _sutimes(caches->cachefd, locCacheDir, O_RDONLY, times);

    
finish:
    if (fd != -1)       close(fd);

    return result;
}



/*****************************************************************************
* hasBRBoots lets you know if a volume has boot partitions and if it's on GPT
*****************************************************************************/
Boolean
hasBootRootBoots(struct bootCaches *caches, CFArrayRef *auxPartsCopy,
                         CFArrayRef *dataPartsCopy, Boolean *isAPM)
{
    CFDictionaryRef binfo = NULL;
    Boolean rval = false, apm = false;
    CFArrayRef dparts = NULL, bparts = NULL;
    char * errmsg = NULL;
    char stack_bsdname[DEVMAXPATHSIZE];
    char * lookup_bsdname = caches->bsdname;
    CFArrayRef dataPartitions = NULL; // do not release;
    size_t fullLen;
    char fulldev[DEVMAXPATHSIZE];
#if BLESS_BUG_RESOLVED
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
            errmsg = "String conversion failure for bsdname.";
            if (!CFStringGetFileSystemRepresentation(dpBsdName, stack_bsdname,
                    sizeof(stack_bsdname)))
                goto finish;
            lookup_bsdname = stack_bsdname;
        }
    }
    
   /* Get the BL info about the partition type (that's all we use, but
    * we have to pass in valid buffer pointers for all the rest).
    */
    errmsg = "Internal error.";
    fullLen = snprintf(fulldev, sizeof(fulldev), "/dev/%s", lookup_bsdname);
    if (fullLen >= sizeof(fulldev)) {
        goto finish;
    }

#if BLESS_BUG_RESOLVED
    // doesn't work on watson w/USB disk??
    errmsg = "Can't get partition type.";
    if (BLGetParentDeviceAndPartitionType(NULL /* context */,
        fulldevname, parentdevname, &partitionNum, &partitionType)) 
    {
        goto finish;
    }
    if (partitionType == kBLPartitionType_APM) {
        apm = true;
    }
#endif

    // 5158091 / 6413843: 10.4.x APM Apple_Boot's aren't BootRoot
    // Boot!=Root was introduced in 10.4.7 for *Intel only*.
    // BootX didn't learn about Boot!=Root until 10.5 (the era of
    // the mkext2 format).
    // The check is APM-only because ppc only booted APM.
    if (apm) {
        CFDictionaryRef pbDict, mk2Dict, kcDict;

        errmsg = NULL;

        // i.e. Leopard had BootX; SnowLeopard has mkext2
        pbDict = CFDictionaryGetValue(caches->cacheinfo, kBCPostBootKey);
        if (!pbDict || CFGetTypeID(pbDict) != CFDictionaryGetTypeID())  goto finish;

        kcDict = CFDictionaryGetValue(pbDict, kBCKernelcacheV1Key);
        mk2Dict = CFDictionaryGetValue(pbDict, kBCMKext2Key);

        // if none of these indicates a more modern OS, we skip
        // XX should the ofbooter path check be != '\0' ?
        // (then we could drop the kcDict check?)
        if (!kcDict && !mk2Dict && caches->ofbooter.rpath[0] == '\0')
            goto finish;
    }

    // check for helper partitions
    rval = (CFArrayGetCount(bparts) > 0);

    errmsg = NULL;
    
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

    // errors
    if (errmsg) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "%s: %s", caches->root, errmsg);
    }

    return rval;
}

CFArrayRef
BRCopyActiveBootPartitions(CFURLRef volRoot)
{   
    CFArrayRef rval = NULL;
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
    rval = CFDictionaryGetValue(binfo, kBLAuxiliaryPartitionsKey);

    // success -> retain sub-dictionary for caller
    if (rval)       CFRetain(rval);

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

    while (scanIndex && path[scanIndex] == '/') {
        path[scanIndex--] = '\0';
    }

    return;
}


/******************************************************************************
 * updateMount() remounts the volume with the requested flags!
 *****************************************************************************/
int
updateMount(mountpoint_t mount, uint32_t mntgoal)
{
    int result = ELAST + 1;
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
    if (!(session = DASessionCreate(nil)))      goto finish;
    DASessionScheduleWithRunLoop(session, CFRunLoopGetCurrent(), toggleMode);
    volURL=CFURLCreateFromFileSystemRepresentation(nil,(void*)mount,MNAMELEN,1);
    if (!(volURL))      goto finish;
    if (!(disk = DADiskCreateFromVolumePath(nil, session, volURL))) goto finish;
    DADiskMountWithArguments(disk, NULL, kDADiskMountOptionDefault, _daDone,
                             &dis, mountargs);

    while (dis == (void*)kCFNull) {
        CFRunLoopRunInMode(toggleMode, 0, true);    // _daDone updates 'dis'
    }
    if (dis)    goto finish;

    result = 0;

finish:
    if (dis && dis != (void*)kCFNull)   CFRelease(dis);
    if (disk)                           CFRelease(disk);
    if (session)                        CFRelease(session);
    if (volURL)                         CFRelease(volURL);

    if (result) {
        OSKextLog(NULL, kOSKextLogWarningLevel | kOSKextLogFileAccessFlag,
            "Warning: couldn't update %s's mount to %0x", mount, mntgoal);
    }

    return result;
}

/******************************************************************************
 * returns the result of fork/exec (negative on error; pid on success)
 * a helper returning an error doesn't count (?)
 * - Boolean 'force' passes -f so that bootstamps are ignored
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
#define COMPILE_TIME_ASSERT(pred)   switch(0){case 0:case pred:;}
int
copyVolumeUUIDs(const char *volPath, uuid_t vol_uuid, CFStringRef *cslvf_uuid __unused)
{
    int rval = ENODEV;
    DADiskRef dadisk = NULL;
    CFDictionaryRef dadesc = NULL;
    CFUUIDRef volUUID;      // just a reference into the dict
    CFUUIDBytes uuidBytes;
COMPILE_TIME_ASSERT(sizeof(CFUUIDBytes) == sizeof(uuid_t));
    CFTypeRef regEntry = NULL;

    dadisk = createDiskForMount(NULL, volPath);
    if (!dadisk) {
        if (errno)      rval = errno;
        goto finish;
    }
    dadesc = DADiskCopyDescription(dadisk);
    if (!dadesc)        goto finish;
    volUUID = CFDictionaryGetValue(dadesc, kDADiskDescriptionVolumeUUIDKey);
    if (!volUUID)       goto finish;
    // XX 8679674: 
    uuidBytes = CFUUIDGetUUIDBytes(volUUID);
    memcpy(vol_uuid, &uuidBytes.byte0, sizeof(uuid_t));   // sizeof(vol_uuid)?

/* didn't end up needing this; might in future?
    if (cslvf_uuid) {
        io_object_t ioObj = IO_OBJECT_NULL;

        if (IO_OBJECT_NULL == (ioObj = DADiskCopyIOMedia(dadisk)))
            goto finish;
        
        regEntry = IORegistryEntryCreateCFProperty(ioObj,
                                    CFSTR(kCoreStorageLVFUUIDKey), nil, 0);
        if (regEntry && CFGetTypeID(regEntry) == CFStringGetTypeID()) {
            // retain the result (regEntry released below)
            *cslvf_uuid = (CFStringRef)CFRetain(regEntry);
        } else {
            *cslvf_uuid = NULL;
        }
    }
*/

    rval = 0;

finish:
    if (regEntry)    CFRelease(regEntry);
    if (dadesc)     CFRelease(dadesc);
    if (dadisk)     CFRelease(dadisk);

    return rval;
}
