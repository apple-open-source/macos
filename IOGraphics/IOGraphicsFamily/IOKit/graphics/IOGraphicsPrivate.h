/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _IOKIT_IOGRAPHICSPRIVATE_H
#define _IOKIT_IOGRAPHICSPRIVATE_H

#include <mach/vm_param.h>
#include <libkern/version.h>
#include <libkern/OSDebug.h>
#include <IOKit/IOLib.h>
#include <IOKit/graphics/IOGraphicsTypesPrivate.h>


#if 1
#define KPRINTF(_fmt_, vargs...)     kprintf(_fmt_, ## vargs)
#else
#define KPRINTF(_fmt_, vargs...)
#endif

/*!
 * @group Debug logs
 * @brief Support for debug logging in development builds.
 *
 * Debug logging in IOGraphics is configured at build time via the
 * DEBG_CATEGORIES_BUILD macro, which is configured using an XCode setting of
 * the same name.
 *
 * At boot, the enabled categories default to DEBG_CATEGORIES_RUNTIME_DEFAULT.
 * The iogdebg boot-arg overrides that default if present, and the debug.iogdebg
 * sysctl allows the value to be adjusted post-boot.
 *
 * Examples:
 *   sudo nvram boot-args="debug=0x6814e iogdebg=0" # log nothing
 *   sudo nvram boot-args="debug=0x6814e iogdebg=-1" # log everything
 *   sudo nvram boot-args="debug=0x6814e iogdebg=3" # enable only categories 0 and 1
 *   sudo sysctl debug.iogdebg # print current value
 *   sudo sysctl debug.iogdebg=0 # log nothing
 *   sudo sysctl debug.iogdebg=7 # log only categories 0, 1, and 2
 *   sudo sysctl debug.iogdebg=-1 # log everything
 *
 * New code should use D(categ, name, fmt, args...) to emit debug spew. Define
 * new categories in debg_category_t as necessary.
 *
 * The old DEBG/DEBG1/DEBG2 macros are supported to ease transition; they
 * shouldn't be used in new code. Some day they might go away.
 */

// Shims for legacy DEBG(), DEBG1(), DEBG2() macros
#define DEBG(name, fmt, args...) D(GENERAL, name, fmt, ##args)
#define DEBG1(name, fmt, args...) D(POWER, name, fmt, ##args)
#define DEBG2(name, fmt, args...) D(DISPLAY_WRANGLER, name, fmt, ##args)

// New code should use D() instead, defining new categories as necessary.
#define D(categ, name, fmt, args...) do{}while(0)

#if DEBG_CATEGORIES_BUILD

#undef D

#define DC_BIT(categ) (1ull << DEBG_CATEGORY_##categ)

extern uint64_t gIOGraphicsDebugCategories;

/*!
 * Debug categories.
 *
 * Keep in mind that categories are the only way to control the spew, so
 * verbose logs may warrant separate categories.
 *
 * The DEBG_CATEGORY_LEGACY_DEBG* categories provide support for the legacy
 * DEBG/DEBG1/DEBG2 macros to continue to exist. If/when the legacy macros are
 * no longer used, the categories can be too.
 */
typedef enum debg_category_t {
    DEBG_CATEGORY_GENERAL = 0,
    DEBG_CATEGORY_POWER = 1,
    DEBG_CATEGORY_DISPLAY_WRANGLER = 2,
    DEBG_CATEGORY_TIME = 3, // TIME_LOGS
    DEBG_CATEGORY_NOTIFICATIONS = 4, // Framebuffer notifications, spammy
} debg_category_t;


// Enable everything by default. If something verbose is added in the future,
// it can be excluded here to keep the default spew level reasonable.
// If you need all logs on, then change the comments on the following lines,
// but by default the NOTIFICATION logging is turned off as it is spammy.
// #define DEBG_CATEGORIES_RUNTIME_DEFAULT (-1ull)
#define DEBG_CATEGORIES_RUNTIME_DEFAULT (-1ull & ~DC_BIT(NOTIFICATIONS))

/*!
 * @macro D(categ, name, fmt, args...)
 * @brief Log a debug message.
 *
 * This is completely compiled out in builds unless they define
 * DEBG_CATEGORIES_BUILD to some non-zero value. Further, DEBG_CATEGORIES_BUILD
 * is used to mask the categories at runtime, so compiler optimization *should*
 * remove the logs that aren't enabled in DEBG_CATEGORIES_BUILD. (Use at your
 * own risk -- always verify.) Finally, debug categories are dynamically masked
 * at runtime, allowing control via iogdebg boot-arg / debug.iogdebg sysctl.
 */
#define D(categ, name, fmt, args...)                                           \
do {                                                                           \
    if (!(gIOGraphicsDebugCategories & DEBG_CATEGORIES_BUILD & DC_BIT(categ))) \
    {                                                                          \
        continue;                                                              \
    }                                                                          \
    AbsoluteTime    DEBG_now;                                                  \
    UInt64          DEBG_nano;                                                 \
    AbsoluteTime_to_scalar(&DEBG_now) = mach_absolute_time();                  \
    absolutetime_to_nanoseconds(DEBG_now, &DEBG_nano);                         \
    KPRINTF("%08d [%s]::%s" fmt, (uint32_t) (DEBG_nano / 1000000ull), name,    \
        __FUNCTION__, ## args);                                                \
} while (false)

static inline void *OBFUSCATE(void *p)
{
    vm_offset_t pcp;
    vm_kernel_addrperm_external(reinterpret_cast<vm_offset_t>(p), &pcp);
    return reinterpret_cast<void*>(pcp);
}

/* RLOG indicates debug spew is enabled. */
#define RLOG  DEBG_CATEGORIES_BUILD
#define RLOG1 DEBG_CATEGORIES_BUILD // vestigial alias

#endif // DEBG_CATEGORIES_BUILD

#if __cplusplus >= 201103L
#define IOGRAPHICS_TYPEOF(_t_)       decltype(_t_)
#else
#define IOGRAPHICS_TYPEOF(_t_)       typeof(_t_)
#endif

#define STOREINC(_ptr_, _data_, _type_) {   \
        *((_type_ *)(_ptr_)) = _data_;                                  \
        _ptr_ = (IOGRAPHICS_TYPEOF(_ptr_)) (((char *) (_ptr_)) + sizeof(_type_));  \
    }

// blue actual:0x00426bad gamma:0x00648cc3 bootx:0x00bfbfbf
#define kIOFBBootGrayValue              0x00bfbfbf
#define kIOFBGrayValue                  0x00000000

#ifndef kAppleAudioVideoJackStateKey
#define kAppleAudioVideoJackStateKey    "AppleAudioVideoJackState"
#endif
#ifndef kIOPMIsPowerManagedKey
#define kIOPMIsPowerManagedKey          "IOPMIsPowerManaged"
#endif
#ifndef kIOAGPCommandValueKey
#define kIOAGPCommandValueKey           "IOAGPCommandValue"
#endif
#ifndef kAppleClamshellStateKey
#define kAppleClamshellStateKey         "AppleClamshellState"
#endif
#ifndef kIOFBWaitCursorFramesKey
#define kIOFBWaitCursorFramesKey        "IOFBWaitCursorFrames"
#endif
#ifndef kIOFBWaitCursorPeriodKey
#define kIOFBWaitCursorPeriodKey        "IOFBWaitCursorPeriod"
#endif

#ifndef kIOUserClientSharedInstanceKey
#define kIOUserClientSharedInstanceKey  "IOUserClientSharedInstance"
#endif

#ifndef kIOHibernateOptionsKey
#define kIOHibernateOptionsKey      "IOHibernateOptions"
#endif

#ifndef kIOHibernateGfxStatusKey
#define kIOHibernateGfxStatusKey    "IOHibernateGfxStatus"
#endif

#ifndef kIOMessageSystemPagingOff
#define kIOMessageSystemPagingOff       iokit_common_msg(0x255)
#endif

extern "C" ppnum_t pmap_find_phys(pmap_t map, addr64_t va);

extern "C" vm_map_t IOPageableMapForAddress( vm_address_t address );

extern "C" IOReturn IOGetHardwareClamshellState( IOOptionBits * result )
__OSX_DEPRECATED(10.0, 10.13, "Use IOFramebuffer::getAttribute(kIOClamshellStateAttribute)");


extern bool                   gIOGraphicsSystemPower;
extern bool                   gIOFBSystemPower;
extern IOOptionBits           gIOFBCurrentClamshellState;
extern const class OSSymbol * gIOFramebufferKey;
extern class OSData *         gIOFBZero32Data;
extern class OSData *         gIOFBOne32Data;
extern int32_t                gIOFBHaveBacklight;
extern const OSSymbol *       gIOFBPMSettingDisplaySleepUsesDimKey;
extern bool                   gIOGFades;

inline void bcopy_nc( void * from, void * to, UInt32 l) { bcopy( from, to, l ); }
inline void bzero_nc( void * p, UInt32 l )              { bzero( p, l ); }

#if VERSION_MAJOR < 9
#define getPowerState() pm_vars->myCurrentState
#endif

extern uint32_t gIOGDebugFlags;
enum {
	kIOGDbgLidOpen                      = 0x00000001,
	kIOGDbgVBLThrottle                  = 0x00000002,
	kIOGDbgK59Mode                      = 0x00000004,
	kIOGDbgDumbPanic                    = 0x00000008,
	kIOGDbgVBLDrift                     = 0x00000010,
	kIOGDbgForceBrightness              = 0x00000020,
	kIOGDbgFades                        = 0x00000040,
    kIOGDbgFBRange                      = 0x00000080,
    kIOGDbgForceLegacyMUXPreviewPolicy  = 0x00000100,
    kIOGDbgNoClamshellOffline           = 0x00000200,
    kIOGDbgNoWaitQuietController        = 0x00000400,
    kIOGDbgRemoveShutdownProtection     = 0x00000800,
    kIOGDbgDisableProbeAfterOpen        = 0x00001000,

    kIOGDbgEnableAutomatedTestSupport   = 0x00010000,
    kIOGDbgEnableGMetrics               = 0x40000000,
    kIOGDbgClamshellInjectionEnabled    = 0x80000000,
};

// FIXME: Move to common header to be shared via client/kernel
extern uint64_t gIOGMetricsFlags;
enum {
    kIOGMetrics_Enabled                 = 0x00000000000000000001ULL,
};

#ifndef kIOScreenLockStateKey

#define IOHIB_PREVIEW_V0	1

enum { kIOPreviewImageCount = 1 };

struct hibernate_preview_t
{
    uint32_t  depth;      	// Pixel Depth
    uint32_t  width;      	// Width
    uint32_t  height;     	// Height
};
typedef struct hibernate_preview_t hibernate_preview_t;

#define kIOScreenLockStateKey      "IOScreenLockState"

#endif /* ! kIOScreenLockStateKey */

// these are the private instance variables for power management
struct IODisplayPMVars
{
    UInt32              currentState;
    // highest state number normally, lowest usable state in emergency
    unsigned long       maxState;
    unsigned long       minDimState;
    // true if the display has had power lowered due to user inactivity
    bool                displayIdle;
};

#endif /* ! _IOKIT_IOGRAPHICSPRIVATE_H */

