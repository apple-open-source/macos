/*
 * FILE: bootfiles.h
 * AUTH: Soren Spies (sspies)
 * DATE: 22 March 2006 (Copyright Apple Computer, Inc)
 * DESC: constants for boot caches
 */

#ifndef __BOOTFILES_H__
#define __BOOTFILES_H__

#include <paths.h>

/* Boot != Root directories */
#define kBootDirR "com.apple.boot.R"
#define kBootDirP "com.apple.boot.P"
#define kBootDirS "com.apple.boot.S"

/* Boot != Root key for firmware's /chosen  */
#define kBootRootActiveKey    "bootroot-active"

/* The kernel */
#define kDefaultKernel        "/mach_kernel"
#define kKernelSymfile        (_PATH_VARRUN "mach.sym")

/* The system extensions folder */
#define kSystemExtensionsDir  "/System/Library/Extensions"

/* The system mkext file */
#define kDefaultMkext         "/System/Library/Extensions.mkext"

/* The prelinked kernel */
#define kPrelinkedKernelDir   "/System/Library/Caches/com.apple.kernelcaches"
#define kPrelinkedKernelBase  "kernelcache"

/* The booter configuration file */
#define kBootConfig           "/Library/Preferences/SystemConfiguration/com.apple.Boot.plist"
#define kKernelFlagsKey       "Kernel Flags"
#define kMKextCacheKey        "MKext Cache"
#define kKernelNameKey        "Kernel"
#define kKernelCacheKey       "Kernel Cache"
#define kRootUUIDKey	      "Root UUID"
#define kRootMatchKey	      "Root Match"

#endif __BOOTFILES_H__
