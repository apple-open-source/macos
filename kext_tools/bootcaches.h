/*
 * Copyright (c) 2006-2012 Apple Inc. All rights reserved.               
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
 * FILE: bootcaches.h
 * AUTH: Soren Spies (sspies)
 * DATE: "spring" 2006
 * DESC: routines for dealing with bootcaches.plist data, bootstamps, etc
 *       shared between kextcache and kextd
 *
 */

#ifndef __BOOTCACHES_H__
#define __BOOTCACHES_H__

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <IOKit/kext/kextmanager_types.h>   // uuid_string_t
#include <mach-o/arch.h>


// cache directories that we create (we also create kCSFDEPropertyCacheDir)
#define kTSCacheDir         "/System/Library/Caches/com.apple.bootstamps"
#define kCacheDirMode       0755        // Sec reviewed
#define kCacheFileMode      0644

// bootcaches.plist and keys

#define kBootCachesPath             "/usr/standalone/bootcaches.plist"
#define kBootCachesDisabledPath "/usr/standalone/bootcaches-cachesonly.plist"
#define kBCPreBootKey               CFSTR("PreBootPaths")    // dict
#define kBCLabelKey                 CFSTR("DiskLabel")       // ".disk_label"
#define kBCBootersKey               CFSTR("BooterPaths")     // dict
#define kBCEFIBooterKey             CFSTR("EFIBooter")       // "boot.efi"
#define kBCOFBooterKey              CFSTR("OFBooter")        // "BootX"
#define kBCPostBootKey              CFSTR("PostBootPaths")   // dict
#define kBCMKextKey                 CFSTR("MKext")           // dict
#define kBCMKext2Key                CFSTR("MKext2")          // dict
#define kBCKernelcacheV1Key         CFSTR("Kernelcache v1.1")// dict
#define kBCKernelcacheV2Key         CFSTR("Kernelcache v1.2")// dict
#define kBCKernelPathKey            CFSTR("KernelPath")      // path string
#define kBCArchsKey                 CFSTR("Archs")           //   ar: ppc, i386
#define kBCExtensionsDirKey         CFSTR("ExtensionsDir")   //   /S/L/E
#define kBCPathKey                  CFSTR("Path")            //   path to cache
// AdditionalPaths are optional w/PreBootPaths, required w/PostBootPaths
#define kBCAdditionalPathsKey       CFSTR("AdditionalPaths") // array
#define kBCBootConfigKey            CFSTR("BootConfig")      // path to plist
#define kBCEncryptedRootKey         CFSTR("EncryptedRoot")   // dict
#define kBCCSFDEPropertyCacheKey    CFSTR("EncryptedPropertyCache") // .wipekey
#define kBCCSFDERootVolPropCacheKey CFSTR("RootVolumePropertyCache")//A_B only?
#define kBCCSFDEDefResourcesDirKey  CFSTR("DefaultResourcesDir") // EfiLoginUI
#define kBCCSFDELocalizationSrcKey  CFSTR("LocalizationSource")  // EFI.fr/Res
#define kBCCSFDELanguagesPrefKey    CFSTR("LanguagesPref")  // .Global...
#define kBCCSFDELocRsrcsCacheKey    CFSTR("LocalizedResourcesCache") // EFILogLocs


typedef enum {
    kMkextCRCError = -1,
    kMkextCRCFound = 0,
    kMkextCRCNotFound = 1,
} MkextCRCResult;

// 6486172 points out that kextd ends up with a lot of these buffers
// (especially w/multiple OS volumes).  8163405 adds BCPATH_MAX, etc.
#define BCPATH_MAX      128
#define TSPATH_MAX      (BCPATH_MAX + 1 + NCHARSUUID + 1 + BCPATH_MAX)
#define NCHARSUUID      (2*sizeof(uuid_t) + 5)  // hex with 4 -'s and one NUL
#define DEVMAXPATHSIZE  128                     // xnu/devfs/devfsdefs.h:
#define ROOTPATH_MAX    (sizeof("/Volumes/") + NAME_MAX)

typedef struct {
    char rpath[BCPATH_MAX];     // (relative) source path in root filesystem
    char tspath[TSPATH_MAX];    // shadow timestamp path tracking Apple_Boot[s]
    struct timeval tstamps[2];  // rpath's initial timestamp(s)
} cachedPath;

struct bootCaches {
    int cachefd;                // Sec: file descriptor to validate data
    char bsdname[DEVMAXPATHSIZE]; // for passing to bless to get helpers
    char fsys_uuid[NCHARSUUID]; // optimized for cachedPaths (cf. 5114411, XX?)
    CFStringRef csfde_uuid;     // encrypted volumes's LVF UUID
    char volname[NAME_MAX];     // for label generation
    char root[ROOTPATH_MAX];    // struct's paths relative to this root
    CFDictionaryRef cacheinfo;  // raw BootCaches.plist data (for archs, etc)
    struct timespec bcTime;     // cache the timestamp of bootcaches.plist

    char kernel[BCPATH_MAX];    // /Volumes/foo/mach_kernel (watch only)
    char exts[BCPATH_MAX];      // /S/L/Extensions (only a source)
    char locSource[BCPATH_MAX]; // only EFILogin.framework/Resources for now
    char locPref[BCPATH_MAX];   // /L/P/.GlabalPreferences
    unsigned nrps;              // number of RPS paths in Apple_Boot
    cachedPath *rpspaths;       // e.g. mkext, kernel, Boot.plist 
    unsigned nmisc;             // "other" files (non-critical)
    cachedPath *miscpaths;      // e.g. icons, labels, etc
    cachedPath efibooter;       // booters get their own paths
    cachedPath ofbooter;        // (we have to bless them, etc)

    // pointers to special watched paths (stored in arrays above)
    cachedPath *kext_boot_cache_file;     // -> mkext or kernelcache
    cachedPath *bootconfig;     // -> .../L/Prefs/SC/com.apple.Boot.plist
    cachedPath *efidefrsrcs;    // -> usr/standalone/i386/EfiLoginUI
    cachedPath *efiloccache;    // -> ...Caches/../EFILoginLocalizations
    cachedPath *label;          // -> .../S/L/CS/.disk_label (in miscPaths)
    cachedPath *erpropcache;    // crypto metadata gets special treatment
    Boolean erpropTSOnly;       // whether props expected in root fsys
};
/* use sizeof() to get it the right bounds */
#undef TSPATH_MAX
#undef BCPATH_MAX
#undef ROOTPATH_MAX

// inspectors
Boolean hasBootRootBoots(struct bootCaches *caches, CFArrayRef *auxPartsCopy,
                         CFArrayRef *dataPartsCopy, Boolean *isAPM);
// Everything except vol_path is optional.
// If specified, vol_bsd must point to at least DEVMAXPATHSIZE bytes.
// If specified, vol_name must point to at least NAME_MAX bytes.
// If no CoreStorage is detected and cslvf_uuid is non-NULL,
// *cslvf_uuid will be set to NULL.
int copyVolumeInfo(const char *vol_path, uuid_t *vol_uuid,
                   CFStringRef *cslvf_uuid, char **vol_bsd, char **vol_name);
// no CSFDE data => encContext = NULL, timeStamp = 0LL;
int copyCSFDEInfo(CFStringRef uuidStr, CFDictionaryRef *encContext,
                   time_t *timeStamp);

// ctors / dtors
struct bootCaches* readBootCaches(char *volRoot);               // kextcache
struct bootCaches* readBootCachesForDADisk(DADiskRef dadisk);   // kextd

void destroyCaches(struct bootCaches *caches);
DADiskRef createDiskForMount(DASessionRef session, const char *mount);

// "stat" a cachedPath, returning out-of-date & setting tstamp; logs errors
// (currently only used in bootcaches.c)
Boolean needsUpdate(char *root, cachedPath* cpath);
// check all cached paths w/needsUpdate (exts/mkext not checked)
Boolean needUpdates(struct bootCaches *caches, Boolean *rps, 
                    Boolean *booters, Boolean *misc, OSKextLogSpec oodLogSpec);

// update the bootstamp files from the tstamps stored in the bootCaches struct
#define kBCStampsUnlinkOnly 0           // updateStamps always unlinks
#define kBCStampsApplyTimes 1           // apply stored timestamps
int updateStamps(struct bootCaches *caches, int command);

// check / rebuild kext caches needs rebuilding
Boolean plistCachesNeedRebuild(const NXArchInfo * kernelArchInfo);
Boolean check_kext_boot_cache_file(
    struct bootCaches * caches,
    const char * cache_path,
    const char * kernel_path);
// build the mkext; waiting for the kextcache child if instructed
int rebuild_kext_boot_cache_file(
    struct bootCaches *caches,
    Boolean wait,
    const char * cache_path,
    const char * kernel_path);

// check/rebuild CSFDE caches
Boolean check_csfde(struct bootCaches *caches);
int rebuild_csfde_cache(struct bootCaches *caches);
int writeCSFDEProps(int scopefd, CFDictionaryRef ectx, 
                    char *cspvbsd, char *dstpath);
Boolean check_loccache(struct bootCaches *caches);
int rebuild_loccache(struct bootCaches *caches);

// diskarb helpers
void _daDone(DADiskRef disk, DADissenterRef dissenter, void *ctx);
int updateMount(mountpoint_t mount, uint32_t mntgoal);


pid_t launch_rebuild_all(char * rootPath, Boolean force, Boolean wait);

#endif /* __BOOTCACHES_H__ */

