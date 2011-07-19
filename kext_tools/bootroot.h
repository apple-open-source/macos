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
 * only available on 10.7:
 *  /usr/lib/libCoreStorage.dylib -> -lCoreStorage
 *  /usr/lib/libcsfde.dylib -> -lcsfde
 *  EFILogin.framework -> -framework EFILogin
 *
 * The 10.7 libraries can be weak-linked if clients need to run
 * on 10.6 (which won't be able to see CoreStorage volumes anyway):
 * Set the "Base SDK" to "Current Mac OS", and set the deployment
 * target to "Mac OS X 10.6".  10.7-only functions will still
 * be available to the target but should be guarded by NULL checks 
 * so they're not called when running on 10.6.
 *
 * Several clients
 * 1. basic "kextcache -u" (Installer, kextd, etc)
 * 2a. "set up Boot!=Root in an Apple_Boot" (Disk Management for CSFDE)
 * 2b. "deactivate Boot!=Root in an Apple_Boot" (Disk Management post-CSFDE)
 * 3a. "custom configure an Apple_Boot to boot off some other volume" (IA)
 * 3b. "keep system Boot!=Root out of the way" (Install Assistant)
 * ---- initial support target ----
 * 4. "complement existing Boot!=Root setup" (ANI5)
 */

#include <CoreFoundation/CoreFoundation.h>


#ifdef __cplusplus
extern "C" {
#endif

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
 *  @result     CFDictionaryRef or NULL if no supported helpers
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
 *      BRCopyBootfiles() allows the caller to copy boot files from a
 *      source volume to a single target partition.  If any of srcVol's
 *      boot caches are out of date, BRCopyBootfiles() updates them
 *      before copying them to the target partition.
 *
 *      The partition referred to by helperBSDName:
 *          - must contain a valid HFS+ filesystem
 *          - should not "belong" to any root volume except initialRoot
 *          - for FDE, must follow initialRoot's Apple_CoreStorage
 *      It will be treated as a helper partition: mounted as
 *      necessary and soft-unmounted regardless of success.
 *
 *      Once complete, the helper's filesystem will be blessed so
 *      that it can be option-booted.  If NVRAM needs to point to the
 *      partition before normal invocations of bless(8) would correctly
 *      set it, libbless's new BLSetEFIBootDevice() [9300207] can be
 *      used to point NVRAM at the helper partition.
 *
 *      srcVol cache updates are made with the running system's
 *      kext subsystem.  Behavior will be undefined if a statically-
 *      linked BRCopyBootFiles() is called on an older system when
 *      srcVol's caches are out of date and the older system can't
 *      properly update them.
 *      
 *      If [Boot!=Root has not been disabled and] srcVol and initialRoot
 *      refer to the same volume, its "boot stamps" will be updated to
 *      assure the system's Boot!=Root that everything is "up to date." 

[NOT YET: If srcVol and initialRoot are different, BRDisableSystemBootRoot()
 *      should be called on initialRoot so Boot!=Root won't later overwrite
 *      the files copied into the helper partition in question.
 XX need to teach BRCopyBootFiles() to read bootcaches.plist.disabled
 so it can find the FDE metadata after BRDisableSystemBootRoot(initialRoot)!]
[XX also need to make it an error if one of two safe modes aren't used:
 1) srcVol == initialRoot -> system Boot!=Root must be active
 2) srcVol != initialRoot -> system Boot!=Root must be disabled

 *      [In addition to BREnableSystemBootRoot(),] BR*Update*BootFiles()
 *      can be used on a volume with the force argument to get its
 *      helper partition(s) back in sync with normal contents.
 *      
 */
OSStatus BRCopyBootFiles(CFURLRef srcVol,
                         CFURLRef initialRoot,
                         CFStringRef helperBSDName,
                         CFDictionaryRef bootPrefOverrides);


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

#endif
