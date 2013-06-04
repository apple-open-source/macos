/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
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
 * FILE: bootroot.h
 * AUTH: Soren Spies (sspies)
 * DATE: 10 March 2011 (Copyright Apple Inc.)
 * DESC: header for libBootRoot.a
 */

#ifndef BOOTROOT_H
#define BOOTROOT_H

/*
 * Link to /usr/local/lib/libBootRoot.a via -lBootRoot
 *
 * libBootRoot requires clients to link against:
 *  ApplicationServices.framework -> -framework ApplicationServices
 *  CoreFoundation.framework -> -framework CoreFoundation
 *  DiskArbitration.framework -> -framework DiskArbitration
 *  IOKit.framework -> -framework IOKit
 *  /usr/local/lib/libbless.a -> add -lbless
 * only available on 10.7 (see below for 10.6):
 *  /usr/lib/libCoreStorage.dylib -> -lCoreStorage
 *  /usr/lib/libcsfde.dylib -> -lcsfde
 *  EFILogin.framework -> -framework EFILogin
 *
 * The 10.7 libraries can be weak-linked if clients need to run
 * on 10.6 (which won't be able to see CoreStorage volumes anyway):
 * Set the "Base SDK" to "Current Mac OS", and set the deployment
 * target to "Mac OS X 10.6".  10.7-only functions should fail
 * cleanly if inadvertantly called on 10.6.
 *
 * Several clients
 * 1. basic "kextcache -u" (Installer, kextd, etc)
 * 2a. "set up Boot!=Root in an Apple_Boot" (Disk Management for CSFDE)
 * 2b. "deactivate Boot!=Root in an Apple_Boot" (Disk Management post-CSFDE)
 * 3a. "custom configure an Apple_Boot to boot off some other volume" (IA)
 * 3b. "keep system Boot!=Root out of the way" (Install Assistant)
 * 4a. set up booting within a directory in an Apple_Boot (Time Machine)
 * 4b. set up booting with a temporary directory (ANI5)
 *
 * WARNING: libBootRoot is NOT THREAD-SAFE / re-entrant.  It uses
 * basename(3), dirname(3), relies on internal static storage, and
 * uses global fchdir() to keep from straying from the target fsys.
 * Complex multi-threaded programs should probably create a 'brtool'
 * and call that as a separate process until 10561671 is addressed.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/kext/OSKextPrivate.h>


#ifdef __cplusplus
extern "C" {
#endif

// these functions
void tool_log(
    OSKextRef aKext,
    OSKextLogSpec logSpec,
    const char * format,
    ...);
void tool_openlog(const char * name);
// allow clients to configure libBootRoot to send logs to the
// system logging facility (ASL) as easily as
//     OSKextSetLogOutputFunction(&tool_log);
//     tool_openlog(getprogname() [/ myCFBundleID]);


// makes getting libBootRoot's logging into ASL as easy as

/*!
 *  @function   BRUpdateBootFiles()
 *  @abstract   kextcache -u [-f]: update as needed [always]
 *
 *  @param  volRoot - volume to be updated
 *  @param  force - copy files regardless of timestamps in volRoot
 *
 *  @result     0 if caches appear up to date / were copied to the right places.
 *              ?? kPOSIXErrorBase could be used to encode errno ??
 *
 *  @discussion
 *      At minimum, ensures that a local-root primory kext cache
 *      (traditional mkext, modern kernelcache) is up to date and
 *      then -- if needed or requested -- copies all Boot!=Root
 *      files to the appropriate helper partition(s) (e.g. Apple_Boot).
 *
 *      BRUpdateBootFiles() always attempts to lock the volume with
 *      kextd to prevent simultaneous automatic background updates.
 *
 *      This function should give the same results as spawning
 *      kextcache -u <volRoot> and waiting for it to succeed.
 *      However, it will perform everything except kernel cache
 *      building in the calling process.  When running on an older
 *      OS, volRoot's source caches must already be up to date.
 */
OSStatus BRUpdateBootFiles(CFURLRef volRoot, Boolean force);


/*!
 *  @function   BRCopyActiveBootPartitions()
 *  @abstract   return list of currently-active Boot!=Root helper partitions
 *
 *  @param  volRoot - volume for which to return helper partitions
 *
 *  @result     CFArrayRef or NULL if no supported helpers
 *
 *  @discussion
 *      Evaluates the target volume and returns a list of helper
 *      partitions.  In the simple case, ths is generally the
 *      Apple_Boot partition following the data-bearing partition
 *      in question.  For Apple_HFS/Apple_Boot, this function returns
 *      NULL.  This function uses on libbless's
 *      BLCreateBooterInformationDictionary().
 */
CFArrayRef BRCopyActiveBootPartitions(CFURLRef volRoot);


/*!
 *  @function   BRCopyBootFiles
 *  @abstract   update boot caches and copy files to specified partition
 *
 *  @param  srcVol - root of volume containing source files and bootcaches.plist
 *  @param  initialRoot - root of volume to make accessible at boot time
 *  @param  helperBSDName - name (like disk0s7) of helper partition
 *  @param  bootPrefOverrides - [optional] extra info for com.apple.Boot.plist
 *
 *  @result     0 if up to date caches were copied
 *              ?? kPOSIXErrorBase could be used to encode errno ??
 *
 *  @discussion
 *      BRCopyBootFiles() copies appropriate boot cache files from a 
 *      source volume to a single target partition.  BRCopyBootFiles()
 *      updates any out of date caches before copying them to the target.
 *
 *      The partition referred to by helperBSDName:
 *          - must contain a valid HFS+ filesystem
 *          - should not "belong" to any root volume except initialRoot
 *          - for FDE, must follow initialRoot's Apple_CoreStorage
 *      It will be treated as a helper partition: mounted as
 *      necessary and soft-unmounted regardless of success.
 *
 *      Once complete, the helper's filesystem will be blessed such
 *      that the option-boot picker will show srcVol's label.  If NVRAM
 *      needs to point to the partition before normal invocations of
 *      bless(8) would correctly set it, libbless's BLSetEFIBootDevice()
 *      can be used to point NVRAM at the helper partition.
 *
 *      Note: srcVol cache updates are made with the running system's
 *      kext subsystem.  Behavior is be undefined if a statically-
 *      linked BRCopyBootFiles() is called on an older system when
 *      srcVol's caches are out of date and the older system can't
 *      properly update them.
 *      
 *      If [Boot!=Root has not been disabled and] srcVol and initialRoot
 *      refer to the same volume, its "boot stamps" will be updated to
 *      assure the system's Boot!=Root that everything is "up to date." 
 *
 *      BR*Update*BootFiles() can be used on a volume with the force
 *      argument to get all helper partition(s) back in sync.

[NOT YET: If srcVol and initialRoot are different, BRDisableSystemBootRoot()
 *      should be called on initialRoot so Boot!=Root won't later overwrite
 *      the files copied into the helper partition in question.]
[XX also need to make it an error if one of two safe modes aren't used:
 1) srcVol == initialRoot AND helperBSDName == only valid helper
    -> system Boot!=Root must (should?) be active
 2) srcVol != initialRoot OR helperBSDName != any default helper
    -> system Boot!=Root must be disabled]
 */

OSStatus BRCopyBootFiles(CFURLRef srcVol,
                         CFURLRef initialRoot,
                         CFStringRef helperBSDName,
                         CFDictionaryRef bootPrefOverrides);

/*!
 *  @function   BRCopyBootFilesToDir
 *  @abstract   copy up-to-date boot caches to specified partition & directory
 *
 *  @param  srcVol - root of volume containing source files and bootcaches.plist
 *  @param  initialRoot - root of volume to make accessible at boot time
 *  @param  bootPrefOverrides - [optional] extra info for com.apple.Boot.plist
 *  @param  targetBSDName - name (like disk0s7) of target partition
 *  @param  targetDir - optional target directory relative to targetBSDName
 *  @param  blessSpec - how to bless the boot bits being copied
 *  @param  pickerLabel - [optional] what the option-picker should show
 *  @param  options - select various optional behaviors (details below)
 *
 *  @result     0 if up to date caches were copied
 *              ?? kPOSIXErrorBase could be used to encode errno ??
 *
 *  @discussion
 *      Similar to BRCopyBootFiles(), BRCopyBootFilesToDir() copies
 *      appropriate boot cache files from a source OS volume to a
 *      directory in a helper partition.  Caches are updated if needed,
 *      possibly using the running system's kext infrastructure.
 *
 *      Specifying a target directory will leave the helper partition
 *      mounted.  Specifying either or a target directory or a non-
 *      Boot!=Root-compatible blessSpec will skip updating the
 *      "bootstamps" in the root volume.
 *      
 *      CSFDE still requires that the target partition be an Apple_Boot
 *      following an Apple_CoreStorage.  targetDir cannot be "/"
 *      (which is reserved for the system).  A subsystem identifier
 *      (like com.apple.AppleNetInstall.caches) is good practice.
 *
 *      BRCopyBootFilesToDir() requires a bless specification.
 *      Depending on blessSpec, the target files will be "blessed"
 *      in the filesystem and/or pointed to directly or indirectly
 *      through efi-boot-* NVRAM variables.  kBRBlessOnce will create
 *      targetDir under a temporary directory that is cleaned up 
 *      by BREraseBootFiles() and BRUpdateBootFiles(...force=true).
 *      See additional details with the BRBlessStyle typedef.  
 */
// XX need proper typedef/enum HeaderDoc
typedef enum {
    kBRBlessNone      = 0,  // nothing blessed: just copy the files
                            // (no booting until later blessed)
    kBRBlessFSDefault = 1,  // fsys: finderinfo[0,1] -> targetDir, boot.efi
                            // (will show up in option-boot picker)
    // 2-7 reserved
// XX these two that manipulte NVRAM aren't implementend yet ...
    kBRBlessFull    = 0x11, // FSDefault + boot-device->targetPartition
                            // (system will boot these until changed)
    kBRBlessOnce    = 0x20  // efi-boot-next -> tmpdir/boot.efi
                            // (system will boot these files once)
} BRBlessStyle;
typedef enum {
    kBRUOptsNone            = 0x0,
    kBRUForceUpdateHelpers  = 0x1,     // helper partitions MUST be updated
    kBRUCachesOnly          = 0x2,     // do NOT update helper partitions
    kBRUHelpersOptional     = 0x4,     // ignore helper update failures
    kBRUExpectUpToDate      = 0x8,     // successful updates -> EX_OSFILE
    kBRUAnyBootStamps       = 0x10     // bootstamps written to top level
} BRUpdateOpts_t;
OSStatus BRCopyBootFilesToDir(CFURLRef srcVol,
                              CFURLRef initialRoot,
                              CFDictionaryRef bootPrefOverrides,
                              CFStringRef targetBSDName,
                              CFURLRef targetDir,
                              BRBlessStyle blessSpec,
                              CFStringRef pickerLabel);

OSStatus BRCopyBootFilesWithOpts(CFURLRef srcVol,
                              CFURLRef initialRoot,
                              CFDictionaryRef bootPrefOverrides,
                              CFStringRef targetBSDName,
                              CFURLRef targetDir,
                              BRBlessStyle blessSpec,
                              CFStringRef pickerLabel,
                              BRUpdateOpts_t opts);


/*!
 *  @function   BREraseBootFiles
 *  @abstract   put a specified helper partition into a pre-Boot!=Root state
 *
 *  @param  srcVolRoot - volume containing source files and bootcaches.plist
 *  @param  helperBSDName - name (like disk0s7) of target helper partition
 *
 *  @result     0 if all Boot!=Root files were removed
 *              (and, ignoring 8952543, any Recovery OS blessed)
 *              ENOTEMPTY if it looks like not everything got cleaned up
 *
 *  @discussion
 *      BREraseBootFiles() will erase all files previously copied from
 *      srcVolRoot by BRCopyBootFiles().  It will also appropriately
 *      re-activate any Recovery OS present in the helper partition.
 *      The helper is mounted and soft-unmounted in all cases.
 *
 *      BREraseBootFiles() will allow destruction of an active helper
 *      for srcVol.  It is up to the caller to ensure that the volume
 *      is, on disk, backed by a partition understood by the system's
 *      firmware.  With live partitioning, a Boot!=Root volume wiil
 *      look like it still requires an Apple_Boot for booting until
 *      after the next reboot.
 *
 *   XX Installing new boot files to srcVol may cause the system's
 *      Boot!=Root to copy them to the Apple_Boot, negating the
 *      effects of BREraseBootFiles().  kextd detects the partition
 *      type change, but we need to make sure that's enough.
 */
OSStatus BREraseBootFiles(CFURLRef srcVolRoot, CFStringRef helperBSDName);


// ---- functions below not yet implemented ----

/*!
 *  @function   BRDisableSystemBootRoot
 *  @abstract   stop Boot!=Root from looking at a particular volume
 *
 *  @param  sysVolRoot - volume for which to disable Boot!=Root 
 *
 *  @result     0 if Boot!=Root could no long be watching this volume
 *
 *  @discussion
 *      This function obtains a lock for the volume from the running
 *      kextd, moves aside the volume's Boot!=Root control file
 *      (/usr/standalone/bootcaches.plist) and then kills kextd
 *      which restarts but no longer watches the volume.
 */
OSStatus BRDisableSystemBootRoot(CFURLRef sysVolRoot);

/*!
 *  @function   BRRestoreSystemBootRoot
 *  @abstract   re-enable Boot!=Root for a particular volume
 *
 *  @param  sysVolRoot - volume for which to re-enable Boot!=Root 
 *
 *  @result     0 if the system Boot!=Root files are back in place and 
 *              Boot!=Root is again watching the volume.
 *
 *  @discussion
 *      Un-does BRDisableSystemBootRoot(), makes sure kextd is watching
 *      the volume, and forcibly updates all helper partitions, erasing
 *      any trickery which might have been imposed.
 *      [should this final re-update occur via libBootRoot or via the
 *       system's kextcache -u?]
 */
OSStatus BRRestoreSystemBootRoot(CFURLRef sysVolRoot);


#ifdef __cplusplus
}
#endif

#endif // BOOTROOT_H
