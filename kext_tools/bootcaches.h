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
 * FILE: bootcaches.h
 * AUTH: Soren Spies (sspies)
 * DATE: "spring" 2006
 * DESC: routines for dealing with bootcaches.plist data, bootstamps, etc
 *	 shared between kextcache and kextd
 *
 */

#ifndef __BOOTCACHES_H__
#define __BOOTCACHES_H__

#include <CoreFoundation/CoreFoundation.h>
#include <TargetConditionals.h>
#if !TARGET_OS_EMBEDDED
#include <DiskArbitration/DiskArbitration.h>
#endif
#include <sys/stat.h>
#include <sys/time.h>
#include <IOKit/kext/kextmanager_types.h>   // uuid_string_t


// timestamp directory
#define kTSCacheDir         "/System/Library/Caches/com.apple.bootstamps/"
#define kTSCacheMask	    0755	// Sec reviewed
#define kRPSDirMask	    0755

// bootcaches.plist and keys
#define kBootCachesPath     "/usr/standalone/bootcaches.plist"
#define kBCPreBootKey		    CFSTR("PreBootPaths")    // dict
#define kBCLabelKey		    CFSTR("DiskLabel")	     // ".disk_label"
#define kBCBootersKey		    CFSTR("BooterPaths")     // dict
#define kBCEFIBooterKey             CFSTR("EFIBooter")       // "boot.efi"
#define kBCOFBooterKey              CFSTR("OFBooter")        // "BootX"
#define kBCPostBootKey		    CFSTR("PostBootPaths")   // dict
#define kBCMKextKey                 CFSTR("MKext")           // dict
#define kBCArchsKey                 CFSTR("Archs")           //   ar: ppc, i386
#define kBCExtensionsDirKey         CFSTR("ExtensionsDir")   //   /S/L/E
#define kBCPathKey                  CFSTR("Path")            //   /S/L/E.mkext
#define kBCAdditionalPathsKey       CFSTR("AdditionalPaths") // array
#define kBCBootConfigKey            CFSTR("BootConfig")      // path string

typedef enum {
    kMkextCRCError = -1,
    kMkextCRCFound = 0,
    kMkextCRCNotFound = 1,
} MkextCRCResult;

// for kextcache and watchvol.c
typedef struct {
    char rpath[PATH_MAX];       // real path in the root filesystem
    char tspath[PATH_MAX];      // shadow timestamp path tracking Apple_Boot[s]
    struct timeval tstamps[2];  // rpath's initial timestamp(s)
} cachedPath;

#define NCHARSUUID      (2*sizeof(uuid_t) + 5)  // hex with 4 -'s and one NUL
#define DEVMAXPATHSIZE  128                     // devfs/devfsdefs.h:

struct bootCaches {
    int cachefd;                // Sec: file descriptor to validate data
    char bsdname[DEVMAXPATHSIZE]; // for passing to bless to get helpers
    char uuid_str[NCHARSUUID];  // optimized for cachedPaths (cf. 5114411, XX?)
    char volname[NAME_MAX];     // for label
    char root[PATH_MAX];        // needed to create absolute paths
    CFDictionaryRef cacheinfo;  // raw BootCaches.plist data (for archs, etc)

    char exts[PATH_MAX];        // /Volumes/foo/S/L/E (watch only; no update)
    int nrps;                   // number of RPS paths Apple_Boot
    cachedPath *rpspaths;       // e.g. mkext, kernel, Boot.plist 
    int nmisc;                  // "other" files (non-critical)
    cachedPath *miscpaths;      // e.g. icons, labels, etc
    cachedPath efibooter;       // booters get their own paths
    cachedPath ofbooter;        // (we have to bless them, etc)

    // pointers to special watched paths
    cachedPath *mkext;          // -> /Volumes/foo/S/L/E.mkext (in rpsPaths)
    cachedPath *bootconfig;     // -> .../L/Prefs/SC/com.apple.Boot.plist
    cachedPath *label;          // -> .../S/L/CS/.disk_label (in miscPaths)
};


// inspectors
Boolean hasBoots(char *bsdname, CFArrayRef *auxPartsCopy, Boolean *isGPT);
Boolean bootedFromDifferentMkext(void);

#if TARGET_OS_EMBEDDED
typedef void * DADiskRef;
typedef void * DASessionRef;
typedef void * DADissenterRef;
typedef void * DAApprovalSessionRef;
#define kDADiskDescriptionVolumeUUIDKey ""
#define kDADiskDescriptionVolumeNameKey ""
#define kDADiskDescriptionMediaBSDNameKey ""
#define kDADiskDescriptionVolumePathKey ""
#endif

// ctors / dtors
struct bootCaches* readCaches(DADiskRef dadisk);
void destroyCaches(struct bootCaches *caches);
int fillCachedPath(cachedPath *cpath, char *uuidchars, char *relpath);
DADiskRef createDiskForMount(DASessionRef session, const char *mount);

// "stat" a cachedPath, setting tstamp, logging errors
int needsUpdate(char *root, cachedPath* cpath, Boolean *outofdate);
// check all cached paths w/needsUpdate (exts/mkext not checked)
int needUpdates(struct bootCaches *caches, Boolean *any,
		    Boolean *rps, Boolean *booters, Boolean *misc);
// apply the stored timestamps to the bootstamps (?unless the source changed?)
int applyStamps(struct bootCaches *caches);

// check to see if the plist cache/mkext needs rebuilding
Boolean check_plist_cache(struct bootCaches *caches);
Boolean check_mkext(struct bootCaches *caches);
// build the mkext; waiting if instructed
int rebuild_mkext(struct bootCaches *caches, Boolean wait);

// diskarb helper
void _daDone(DADiskRef disk, DADissenterRef dissenter, void *ctx);

#endif /* __BOOTCACHES_H__ */
