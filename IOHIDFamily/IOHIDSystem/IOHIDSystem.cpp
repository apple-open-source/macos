/*
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2012 Apple Computer, Inc.  All Rights Reserved.
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

#define DEBUG_ASSERT_COMPONENT_NAME_STRING "IOHIDSystem"
//#define DEBUG_ASSERT_PRODUCTION_CODE 0


#include <AssertMacros.h>
#include "IOHIDKeys.h"

#include "IOHIDEvent.h"
#include <IOKit/system.h>
#include <IOKit/assert.h>

#include <libkern/c++/OSContainers.h>
#include <libkern/c++/OSCollectionIterator.h>

#include <kern/queue.h>

#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/hidsystem/IOHIDevice.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/usb/USB.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <kern/clock.h>
#include "IOHIDShared.h"
#include "IOHIDSystem.h"
#include "IOHIDEventService.h"
#include "IOHIDPointing.h"
#include "IOHIDKeyboard.h"
#include "IOHIDConsumer.h"
#include "IOHITablet.h"
#include "IOHIDPointingDevice.h"
#include "IOHIDKeyboardDevice.h"
#include "IOHIDPrivate.h"
#include "IOHIDPrivateKeys.h"
#include "IOHIDEventServiceQueue.h"
#include "IOLLEvent.h"

#include "ev_private.h"
#include "IOHIDUserClient.h"
#include "AppleHIDUsageTables.h"
#include "IOHIDKeyboard.h"
#include "IOHIDFamilyTrace.h"
#include "IOHIDWorkLoop.h"
#include "IOHIDSystemCursorHelper.h"

#include <sys/kdebug.h>
#include <sys/proc.h>

#ifdef __cplusplus
    extern "C"
    {
        #include <UserNotification/KUNCUserNotifications.h>
        void cons_cinput( char c);
    }
#endif

extern "C" {
    #include <sys/kauth.h>

    #define CONFIG_MACF 1
    #include <security/mac_framework.h>
    #undef CONFIG_MACF
};

bool displayWranglerUp( OSObject *, void *, IOService * );

#if 0
#define PROFILE_TRACE(X)     IOHID_DEBUG(kIOHIDDebugCode_Profiling, X, __LINE__, 0, 0)
#else
#define PROFILE_TRACE(X)
#endif

static IOHIDSystem * evInstance = 0;
MasterAudioFunctions *masterAudioFunctions = 0;

#define xpr_ev_cursor(x, a, b, c, d, e)

#ifndef kIOFBWaitCursorFramesKey
#define kIOFBWaitCursorFramesKey    "IOFBWaitCursorFrames"
#endif
#ifndef kIOFBWaitCursorPeriodKey
#define kIOFBWaitCursorPeriodKey    "IOFBWaitCursorPeriod"
#endif

#ifndef kIOUserClientCrossEndianKey
#define kIOUserClientCrossEndianKey "IOUserClientCrossEndian"
#endif

#ifndef kIOUserClientCrossEndianCompatibleKey
#define kIOUserClientCrossEndianCompatibleKey "IOUserClientCrossEndianCompatible"
#endif

#ifndef abs
#define abs(_a) ((_a >= 0) ? _a : -_a)
#endif

#define NORMAL_MODIFIER_MASK (NX_COMMANDMASK | NX_CONTROLMASK | NX_SHIFTMASK | NX_ALTERNATEMASK)

#define EV_MAX_SCREENS 32

#define IDLE_HID_ACTIVITY_NSECS ((uint64_t)(5*60*NSEC_PER_SEC))

static UInt8 ScalePressure(unsigned pressure)
{
    return ((pressure * (unsigned long long)EV_MAXPRESSURE) / (unsigned)(65535LL));
}

#define EV_NS_TO_TICK(ns)   AbsoluteTimeToTick(ns)
#define EV_TICK_TO_NS(tick,ns)  TickToAbsoluteTime(tick,ns)

/* COMMAND GATE COMPATIBILITY TYPE DEFS */
typedef struct _IOHIDCmdGateActionArgs {
    void*   arg0;
    void*   arg1;
    void*   arg2;
    void*   arg3;
    void*   arg4;
    void*   arg5;
    void*   arg6;
    void*   arg7;
    void*   arg8;
    void*   arg9;
    void*   arg10;
    void*   arg11;
} IOHIDCmdGateActionArgs;
/* END COMMAND GATE COMPATIBILITY TYPE DEFS */

/* HID SYSTEM EVENT LOCK OUT SUPPORT */

static bool         gKeySwitchLocked = false;
static bool             gUseKeyswitch = true;
static IONotifier *     gSwitchNotification = 0;

// IONotificationHandler
static bool keySwitchNotificationHandler(void *target __unused, void *refCon __unused, IOService *service, IONotifier * /* ignored */)
{

    gKeySwitchLocked = (service->getProperty("Keyswitch") == kOSBooleanTrue);

    return true;
}

/* END HID SYSTEM EVENT LOCK OUT SUPPORT */

// RY: Consume any keyboard events that come in before the
// deadline after the system wakes up or if the keySwitch is locked
#define kHIDConsumeCauseNone    0x00
#define kHIDConsumeCauseKeyLock 0x01
#define kHIDConsumeCauseDeadline 0x02

static inline UInt32 ShouldConsumeHIDEvent(AbsoluteTime ts, AbsoluteTime deadline, bool checkKeySwitch = true )
{
    if (checkKeySwitch && gKeySwitchLocked && gUseKeyswitch)
        return kHIDConsumeCauseKeyLock;

    if ( AbsoluteTime_to_scalar(&ts) == 0 )
        clock_get_uptime(&ts);

    if (CMP_ABSOLUTETIME(&ts, &deadline) <= 0)
    {
         return kHIDConsumeCauseDeadline;
    }

    return kHIDConsumeCauseNone;
}

#define WAKE_DISPLAY_ON_MOVEMENT (NX_WAKEMASK & MOVEDEVENTMASK)

#define DISPLAY_IS_ENABLED (displayState & IOPMDeviceUsable)

#define TICKLE_DISPLAY(event) \
{ \
    if (!evStateChanging && displayManager) { \
        IOHID_DEBUG(kIOHIDDebugCode_DisplayTickle, event, __LINE__, 0, 0); \
        if (!DISPLAY_IS_ENABLED) kprintf("IOHIDSystem tickle when screen off for event %d at line %d\n", (int)event, __LINE__); \
        displayManager->activityTickle(IOHID_DEBUG_CODE(event)); \
    } \
    updateHidActivity(); \
}

#define CONVERT_EV_TO_HW_BUTTONS(ev_buttons,hw_buttons)                         \
        hw_buttons = ev_buttons & ~7; /* Keep everything but bottom 3 bits. */  \
        hw_buttons |= (ev_buttons & 3) << 1;  /* Map bits 01 to 12 */           \
        hw_buttons |= (ev_buttons & 4) >> 2;  /* Map bit 2 back to bit 0 */

#define CONVERT_EV_TO_HW_DELTA(ev_buttons,hw_delta)                             \
        hw_delta = ev_buttons & ~7;                                             \
        hw_delta |= (ev_buttons & 3) << 1; /* Map bits 01 to 12 */              \
        hw_delta |= (ev_buttons & 4) >> 2; /* Map bit 2 back to bit 0 */

#define CONVERT_HW_TO_WV_BUTTONS(hw_buttons,ev_buttons)                         \
        ev_buttons = hw_buttons & ~7; /* Keep everything but bottom 3 bits. */  \
        ev_buttons |= (hw_buttons & 6) >> 1;  /* Map bits 12 to 01 */           \
        ev_buttons |= (hw_buttons & 1) << 2;  /* Map bit 0 to bit 2 */



enum {
    // Options for IOHIDPostEvent()
    kIOHIDSetGlobalEventFlags       = 0x00000001,
    kIOHIDSetCursorPosition         = 0x00000002,
    kIOHIDSetRelativeCursorPosition = 0x00000004,
    kIOHIDPostHIDManagerEvent       = 0x00000008
};

#define kIOHIDPowerOnThresholdNS            (500ULL * kMillisecondScale)    // 1/2 second
#define kIOHIDRelativeTickleThresholdNS     (50ULL * kMillisecondScale)     // 1/20 second
#define kIOHIDRelativeTickleThresholdPixel  3
#define kIOHIDDispaySleepAbortThresholdNS   (5ULL * kSecondScale)           // 5 seconds
#define kIOHIDChattyMouseSuppressionDelayNS kSecondScale                    // 1 second
#define kIOHIDSystenDistantFuture           INT64_MAX

static AbsoluteTime gIOHIDPowerOnThresoldAbsoluteTime;
static AbsoluteTime gIOHIDRelativeTickleThresholdAbsoluteTime;
// gIOHIDDisplaySleepAbortThresholdAbsoluteTime  - Time before which display sleep
// can be aborted by a mouse/scroll motion
static AbsoluteTime gIOHIDDisplaySleepAbortThresholdAbsoluteTime;
static AbsoluteTime gIOHIDZeroAbsoluteTime;

//************************************************************
// Cached Mouse Event Info Support
//************************************************************
enum {
    kCachedMousePointingTabletEventDispFlag = 0x01,
    kCachedMousePointingTabletEventPendFlag = 0x02,
    kCachedMousePointingEventDispFlag       = 0x04,
    kCachedMouseTabletEventDispFlag         = 0x08
};

typedef struct _CachedMouseEventStruct {
    OSObject *                  service;
    UInt64                      eventDeadline;
    UInt64                      reportInterval_ns;
    SInt32                      lastButtons;
    SInt32                      accumX;
    SInt32                      accumY;
    bool                        proximity;
    UInt32                      state;
    UInt8                       subType;
    NXEventData                 tabletData;
    NXEventData                 proximityData;
    UInt16                      pointerFractionX;
    UInt16                      pointerFractionY;
    UInt8                       lastPressure;
} CachedMouseEventStruct;

//************************************************************
static void CalculateEventCountsForService(CachedMouseEventStruct *mouseStruct)
{
    if ( mouseStruct ) {
        mouseStruct->reportInterval_ns = 8000000; // default to 8ms
        if ( mouseStruct->service ) {
            IORegistryEntry *senderEntry = OSDynamicCast(IORegistryEntry, mouseStruct->service);
            if (senderEntry) {
                OSNumber *reportInterval_us = (OSNumber*)senderEntry->copyProperty(kIOHIDReportIntervalKey, gIOServicePlane, kIORegistryIterateRecursively| kIORegistryIterateParents);
                if (OSDynamicCast(OSNumber, reportInterval_us)) {
                    mouseStruct->reportInterval_ns = reportInterval_us->unsigned64BitValue() * 1000;
                }
                else {
                    IOLog("No interval found for %s. Using %lld\n", senderEntry->getLocation(), mouseStruct->reportInterval_ns);
                }
                OSSafeReleaseNULL(reportInterval_us);
            }
        }
    }
}

//************************************************************
static void CalculateEventCountsForAllServices(OSArray *events)
{
    if ( events )
    {
        int count = events->getCount();
        int i;

        for ( i=0; i<count; i++ )
        {
            OSData *data;
            CachedMouseEventStruct *mouseEvent;
            if ( (data = (OSData *)events->getObject(i) ) &&
                (mouseEvent = (CachedMouseEventStruct *)data->getBytesNoCopy()) )
            {
                CalculateEventCountsForService(mouseEvent);
            }
        }
    }
}

//************************************************************
static SInt32 GetCachedMouseButtonStates(OSArray *events)
{
    CachedMouseEventStruct *    mouseEvent  = 0;
    OSData *                    data        = 0;
    SInt32                      buttonState = 0;
    UInt32                      count       = 0;
    UInt32                      i           = 0;

    if ( events )
    {
        count = events->getCount();

        for ( i=0; i<count; i++ )
        {
            if ( (data = (OSData *)events->getObject(i)) &&
                (mouseEvent = (CachedMouseEventStruct *)data->getBytesNoCopy()))
            {
                buttonState |= mouseEvent->lastButtons;
            }
        }
    }

    return buttonState;
}

//************************************************************
static void ClearCachedMouseButtonStates(OSArray *events)
{
    CachedMouseEventStruct *    mouseEvent  = 0;
    OSData *                    data        = 0;
    UInt32                      count       = 0;
    UInt32                      i           = 0;

    if ( events )
    {
        count = events->getCount();

        for ( i=0; i<count; i++ )
        {
            if ( (data = (OSData *)events->getObject(i)) &&
                 (mouseEvent = (CachedMouseEventStruct *)data->getBytesNoCopy()))
            {
                mouseEvent->lastButtons = 0;
                CalculateEventCountsForService(mouseEvent);
            }
        }
    }
}

//************************************************************
static CachedMouseEventStruct * GetCachedMouseEventForService(OSArray *events, OSObject *service, UInt32 * index = 0)
{
    CachedMouseEventStruct *    mouseEvent  = 0;
    OSData *                    data        = 0;
    UInt32                      count       = 0;
    UInt32                      i           = 0;

    if ( events )
    {
        count = events->getCount();

        for ( i=0; i<count; i++ )
        {
            if ( (data = (OSData *)events->getObject(i)) &&
                 (mouseEvent = (CachedMouseEventStruct *)data->getBytesNoCopy()) &&
                 (mouseEvent->service == service) )
            {
                if ( index ) *index = i;
                return mouseEvent;
            }
        }
    }

    return NULL;
}

//************************************************************
static void AppendNewCachedMouseEventForService(OSArray *events, OSObject *service)
{
    CachedMouseEventStruct  temp;
    OSData *                data;

    bzero(&temp, sizeof(CachedMouseEventStruct));
    temp.service = service;

    data = OSData::withBytes(&temp, sizeof(CachedMouseEventStruct));
    events->setObject(data);
    data->release();
    CalculateEventCountsForAllServices(events);
}

//************************************************************
static void RemoveCachedMouseEventForService(OSArray *events, OSObject *service)
{
    UInt32  index;

    if ( events && GetCachedMouseEventForService(events, service, &index) )
    {
        events->removeObject(index);
    }
    CalculateEventCountsForAllServices(events);
}

//************************************************************
// NX System Info Support
//************************************************************
#define kNXSystemInfoKey "NXSystemInfo"

static void AppendNewNXSystemInfoForService(OSArray *systemInfo, IOService *service)
{
    OSDictionary *  deviceInfo  = NULL;
    OSNumber *      deviceID    = NULL;
    IOHIDevice *    hiDevice    = NULL;
    OSObject *      object      = NULL;

    if ( !systemInfo || !(hiDevice = OSDynamicCast(IOHIDevice, service)))
        return;

    deviceInfo = OSDictionary::withCapacity(4);

    if ( !deviceInfo )
        return;

    deviceID = OSNumber::withNumber(hiDevice->getRegistryEntryID(), 64);
    if (deviceID)
    {
        deviceInfo->setObject("serviceID", deviceID);
        deviceID->release();
    }

    object = hiDevice->copyProperty(kIOHIDKindKey);
    if ( object )
    {
        deviceInfo->setObject(kIOHIDKindKey, object);
        object->release();
    }

    object = hiDevice->copyProperty(kIOHIDInterfaceIDKey);
    if ( object )
    {
        deviceInfo->setObject(kIOHIDInterfaceIDKey, object);
        object->release();
    }

    object = hiDevice->copyProperty(kIOHIDSubinterfaceIDKey);
    if ( object )
    {
        deviceInfo->setObject(kIOHIDSubinterfaceIDKey, object);
        object->release();
    }

    if ( hiDevice->metaCast("AppleADBKeyboard") || (hiDevice->getProvider() && hiDevice->getProvider()->metaCast("AppleEmbeddedKeyboard")) )
        deviceInfo->setObject("built-in", kOSBooleanTrue);

    // RY: Hack for rdar://4365935 Turning on NumLock causes Keyboard
    // Viewer to display the external keyboard
    // Because keyboardType information is not passed in special key
    // events, CG infers that this event comes from the default keyboard,
    // or first keyboard in NXSystemInfo. Unforunately, in some cases a
    // USB external keyboard will enumerate before a USB internal keyboard.
    // If this is encountered, we should alway insert the internal keyboard
    // at the beginning of the list.  This will cause CG to guess correctly.
    // RY: Extension for rdar://4418444 Keyboard Viewer defaults to wrong
    // keyboard layout when launched.
    // We should also insert keyboards to the front of the list, prefering
    // Apple, if the front does not already contain a built-in keyboard.
    OSDictionary * tempSystemInfo;
    OSNumber * number;

    // If a keyboard
    if ( ((hiDevice->hidKind() == kHIKeyboardDevice) && (hiDevice->deviceType() != 0))&&
            // And keyboard is built-in
            ( (deviceInfo->getObject("built-in") == kOSBooleanTrue) ||
            // Or, first item in the list is not a keyboard
            !((tempSystemInfo = OSDynamicCast(OSDictionary, systemInfo->getObject(0))) && (number = OSDynamicCast(OSNumber,tempSystemInfo->getObject(kIOHIDKindKey))) && (number->unsigned32BitValue() == kHIKeyboardDevice)) ||
            // Or, if the keyboard is Apple and the first item in the list is not built-in
            // vtn3: potentially unsafe, but a pain to fix. leave it for now.
            ((number = OSDynamicCast(OSNumber, hiDevice->getProperty(kIOHIDVendorIDKey))) && (number->unsigned32BitValue() == kIOUSBVendorIDAppleComputer) && (tempSystemInfo->getObject("built-in") != kOSBooleanTrue)) ) )
    {
        systemInfo->setObject(0, deviceInfo);
    }
    else
    {
        systemInfo->setObject(deviceInfo);
    }

    deviceInfo->release();
}

static void RemoveNXSystemInfoForService(OSArray *systemInfo, IOService *service)
{
    OSDictionary *  deviceInfo  = NULL;
    OSNumber *      serviceID   = NULL;
    UInt32          i, count;

    if ( !systemInfo || !OSDynamicCast(IOHIDevice, service))
        return;

    count = systemInfo->getCount();

    for ( i=0; i<count; i++ )
    {
        if ( (deviceInfo = (OSDictionary *)systemInfo->getObject(i)) &&
             (serviceID = (OSNumber *)deviceInfo->getObject("serviceID")) &&
             (serviceID->unsigned64BitValue() == service->getRegistryEntryID()) )
        {
            systemInfo->removeObject(i);
            break;
        }
    }
}

//************************************************************
// keyboardEventQueue support
//************************************************************
static queue_head_t     gKeyboardEQ;
static IOLock *         gKeyboardEQLock = 0;

typedef IOReturn (*KeyboardEQAction)(IOHIDSystem * self, void *args);

typedef struct _KeyboardEQElement {
    queue_chain_t   link;

    KeyboardEQAction    action;
    OSObject *          sender;
    AbsoluteTime        ts;

    union {
        struct {
            unsigned        eventType;
            unsigned        flags;
            unsigned        key;
            unsigned        charCode;
            unsigned        charSet;
            unsigned        origCharCode;
            unsigned        origCharSet;
            unsigned        keyboardType;
            bool            repeat;
        } keyboard;
        struct {
            unsigned        eventType;
            unsigned        flags;
            unsigned        key;
            unsigned        flavor;
            UInt64          guid;
            bool            repeat;
        } keyboardSpecial;
        struct {
            unsigned        flags;
        } flagsChanged;
    } event;
} KeyboardEQElement;

#define KEYBOARD_EQ_LOCK    if (gKeyboardEQLock) IOLockLock(gKeyboardEQLock);
#define KEYBOARD_EQ_UNLOCK  if (gKeyboardEQLock) IOLockUnlock(gKeyboardEQLock);


static UInt8 stickyKeysState = false;

static void notifyHIDevices(IOService *service, OSArray *hiDevices, UInt32 type)
{
    IOHIKeyboard    *keyboard;

    if(!stickyKeysState || !hiDevices)
        return;

    switch ( type )
    {
        case kIOHIDSystem508MouseClickMessage:
        case kIOHIDSystem508SpecialKeyDownMessage:
            for(unsigned index=0; index<hiDevices->getCount(); index++)
            {
                keyboard = OSDynamicCast(IOHIKeyboard, hiDevices->getObject(index));
                if (keyboard)
                    keyboard->IOHIKeyboard::message(type, service);
            }
            break;
    }
}

/* This function exists because IOHIDSystem uses OSArrays of OSNumbers. Since
 * OSArray::containsObject() compares by pointer equality instead of
 * OSObject::isEqualTo().  OSNumbers cannot be found by value with
 * containsObject(), so we must use this instead.
 */
#define kObjectNotFound ((unsigned int) -1)
static unsigned int
getArrayIndexForObject(OSArray *array, OSObject* object)
{
    OSObject *tmp;
    u_int i;

    for (i = 0; i < array->getCount(); ++i) {
        tmp = array->getObject(i);
        if (tmp->isEqualTo(object)) {
            return i;
        }
    }

    return kObjectNotFound;
}

static void
hidActivityThread_cb(thread_call_param_t us, thread_call_param_t )
{
    ((IOHIDSystem *)us)->hidActivityChecker();
}

typedef struct {
    IOCommandGate::Action   handler;
    IOService               *newService;
}
IOHIDSystem_notificationData;

#define super IOService
OSDefineMetaClassAndStructors(IOHIDSystem, IOService);

struct IOHIDSystem::ExpansionData
{
    // The goal of the cursorHelper is to isolate all of the cursor
    // calculations and updates into one class.
    IOHIDSystemCursorHelper cursorHelper;
    UInt32                  delayedScrollMomentum;
    UInt8                   scDirection;
    UInt8                   scIgnoreMomentum;
    UInt8                   scMouseCanReset;
    UInt8                   scIncrementedThisPhrase;
    UInt16                  scCount;
    UInt16                  scCountMax;
    IOFixed64               scAccelerationFactor;
    UInt64                  scMinDeltaSqToStart;
    UInt64                  scMinDeltaSqToSustain;
    UInt64                  scLastScrollEndTime;
    UInt64                  scLastScrollSustainTime;
    UInt64                  scMaxTimeDeltaBetween;
    UInt64                  scMaxTimeDeltaToSustain;
    IOFixedPoint64          scLastScrollLocation;
    OSDictionary            *devicePhaseState;

    UInt64                  periodicEventLast;
    UInt64                  periodicEventNext;
    UInt64                  cursorEventLast;
    UInt64                  cursorMoveLast;
    UInt64                  cursorMoveDelta;
    UInt64                  cursorWaitLast;
    UInt64                  cursorWaitDelta;

    IONotifier *            displayWranglerMatching;

    bool                    continuousCursor;
    bool                    hidActivityIdle; // Is HID activity idle for more than IDLE_HID_ACTIVITY_NSECS ?
    AbsoluteTime            lastTickleTime;
    thread_call_t           hidActivityThread;

    // async delayed notifications
    IOLock                  *delayedNotificationLock;
    OSArray                 *delayedNotificationArray;
    IOTimerEventSource      *delayedNotificationSource;
    
    OSDictionary            *senderIDDictionary;
};

#define _cursorHelper               (_privateData->cursorHelper)
#define _devicePhaseState           (_privateData->devicePhaseState)
#define _delayedScrollMomentum      (_privateData->delayedScrollMomentum)
#define _scCount                    (_privateData->scCount)
#define _scCountMax                 (_privateData->scCountMax)
#define _scDirection                (_privateData->scDirection)
#define _scIgnoreMomentum           (_privateData->scIgnoreMomentum)
#define _scIncrementedThisPhrase    (_privateData->scIncrementedThisPhrase)
#define _scMouseCanReset            (_privateData->scMouseCanReset)
#define _scMinDeltaSqToStart        (_privateData->scMinDeltaSqToStart)
#define _scMinDeltaSqToSustain      (_privateData->scMinDeltaSqToSustain)
#define _scLastScrollEndTime        (_privateData->scLastScrollEndTime)
#define _scLastScrollSustainTime    (_privateData->scLastScrollSustainTime)
#define _scMaxTimeDeltaBetween      (_privateData->scMaxTimeDeltaBetween)
#define _scMaxTimeDeltaToSustain    (_privateData->scMaxTimeDeltaToSustain)
#define _scLastScrollLocation       (_privateData->scLastScrollLocation)
#define _scAccelerationFactor       (_privateData->scAccelerationFactor)

#define _periodicEventLast          (_privateData->periodicEventLast)
#define _periodicEventNext          (_privateData->periodicEventNext)
#define _cursorEventLast            (_privateData->cursorEventLast)
#define _cursorMoveLast             (_privateData->cursorMoveLast)
#define _cursorMoveDelta            (_privateData->cursorMoveDelta)
#define _cursorWaitLast             (_privateData->cursorWaitLast)
#define _cursorWaitDelta            (_privateData->cursorWaitDelta)

#define _displayWranglerMatching    (_privateData->displayWranglerMatching)

#define _continuousCursor            (_privateData->continuousCursor)
#define _hidActivityIdle            (_privateData->hidActivityIdle)
#define _lastTickleTime             (_privateData->lastTickleTime)
#define _hidActivityThread          (_privateData->hidActivityThread)

#define _delayedNotificationLock    (_privateData->delayedNotificationLock)
#define _delayedNotificationArray   (_privateData->delayedNotificationArray)
#define _delayedNotificationSource  (_privateData->delayedNotificationSource)

#define _senderIDDictionary         (_privateData->senderIDDictionary)

enum {
    kScrollDirectionInvalid = 0,
    kScrollDirectionXPositive,
    kScrollDirectionXNegative,
    kScrollDirectionYPositive,
    kScrollDirectionYNegative,
    kScrollDirectionZPositive,
    kScrollDirectionZNegative,
};

#define _cursorLog(ts)      do { \
                                if (evg != 0) \
                                    if (evg->logCursorUpdates) { \
                                        _cursorHelper.logPosition(__func__, ts); \
                                    } \
                            } \
                            while(false)

#define _cursorLogTimed()   do { \
                                if (evg != 0) \
                                    if (evg->logCursorUpdates) { \
                                        AbsoluteTime ts; \
                                        clock_get_uptime(&ts); \
                                        _cursorHelper.logPosition(__func__, AbsoluteTime_to_scalar(&ts)); \
                                    } \
                            } \
                            while(false)

/* Return the current instance of the EventDriver, or 0 if none. */
IOHIDSystem * IOHIDSystem::instance()
{
  return evInstance;
}

#define kDefaultMinimumDelta    0x1ffffffffULL

bool IOHIDSystem::init(OSDictionary * properties)
{
    if (!super::init(properties))
        return false;

    _privateData = (ExpansionData*)IOMalloc(sizeof(ExpansionData));
    if (!_privateData)
        return false;
    bzero(_privateData, sizeof(ExpansionData));
    _periodicEventNext = kIOHIDSystenDistantFuture;

    if (!_cursorHelper.init())
        return false;
    _cursorLogTimed();

    // hard defaults. these exist just to keep the state machine sane.
    _scMinDeltaSqToStart = _scMinDeltaSqToSustain = kDefaultMinimumDelta;
    _scAccelerationFactor.fromIntFloor(5);
    _scCountMax = 2000;

    /*
     * Initialize minimal state.
     */
    evScreen          = NULL;
    periodicES        = 0;
    eventConsumerES   = 0;
    keyboardEQES      = 0;
    cmdGate           = 0;
    workLoop          = 0;
    cachedEventFlags  = 0;
    displayState = IOPMDeviceUsable;
    AbsoluteTime_to_scalar(&lastEventTime) = 0;
    AbsoluteTime_to_scalar(&lastUndimEvent) = 0;
    AbsoluteTime_to_scalar(&rootDomainStateChangeDeadline) = 0;
    AbsoluteTime_to_scalar(&displayStateChangeDeadline) = 0;
    AbsoluteTime_to_scalar(&displaySleepWakeupDeadline) = 0;
    AbsoluteTime_to_scalar(&gIOHIDZeroAbsoluteTime) = 0;
    displaySleepDrivenByPM = false;


    ioHIDevices         = OSArray::withCapacity(2);
    cachedButtonStates  = OSArray::withCapacity(3);
    touchEventPosters   = OSSet::withCapacity(2);
    consumedKeys        = OSArray::withCapacity(5);
    _senderIDDictionary = OSDictionary::withCapacity(2);

    // RY: Populate cachedButtonStates key=0 with a button State
    // This will cover all pointing devices that don't support
    // the new private methods.
    AppendNewCachedMouseEventForService(cachedButtonStates, 0);

    nanoseconds_to_absolutetime(kIOHIDPowerOnThresholdNS, &gIOHIDPowerOnThresoldAbsoluteTime);
    nanoseconds_to_absolutetime(kIOHIDRelativeTickleThresholdNS, &gIOHIDRelativeTickleThresholdAbsoluteTime);
    nanoseconds_to_absolutetime(kIOHIDDispaySleepAbortThresholdNS, &gIOHIDDisplaySleepAbortThresholdAbsoluteTime);

    queue_init(&gKeyboardEQ);
    gKeyboardEQLock = IOLockAlloc();
    return true;
}

IOHIDSystem * IOHIDSystem::probe(IOService *    provider,
                 SInt32 *   score)
{
  if (!super::probe(provider,score))  return 0;

  return this;
}

/*
 * Perform reusable initialization actions here.
 */
IOWorkLoop * IOHIDSystem::getWorkLoop() const
{
    return workLoop;
}

bool IOHIDSystem::start(IOService * provider)
{
    bool            iWasStarted = false;
    OSObject        *obj = NULL;
    OSNumber        *number = NULL;
    OSDictionary    *matchingDevice = serviceMatching("IOHIDevice");
    OSDictionary    *matchingService = serviceMatching("IOHIDEventService");
    OSDictionary    *matchingWrangler = serviceMatching("IODisplayWrangler");
    OSDictionary    *matchingKeyswitch = serviceMatching("AppleKeyswitch");
    IOServiceMatchingNotificationHandler iohidNotificationHandler = OSMemberFunctionCast(IOServiceMatchingNotificationHandler, this, &IOHIDSystem::genericNotificationHandler);
    
    require(super::start(provider), exit_early);
    
    _setScrollCountParameters();
    
    evInstance = this;
    
    /* A few details to be set up... */
    _cursorHelper.desktopLocation().fromIntFloor(INIT_CURSOR_X, INIT_CURSOR_Y);
    _cursorHelper.desktopLocationDelta().fromIntFloor(0, 0);
    _cursorHelper.updateScreenLocation(NULL, NULL);
    _cursorLogTimed();
    _delayedNotificationLock = IOLockAlloc();
    _delayedNotificationArray = OSArray::withCapacity(2);
    
    evScreenSize = sizeof(EvScreen) * EV_MAX_SCREENS;
    evScreen = (void *) IOMalloc(evScreenSize);
    bzero(evScreen, evScreenSize);
    savedParameters = OSDictionary::withCapacity(4);
    
    require(evScreen && savedParameters && _delayedNotificationLock && _delayedNotificationArray, exit_early);
    
    bzero(evScreen, evScreenSize);
    firstWaitCursorFrame = EV_WAITCURSOR;
    maxWaitCursorFrame   = EV_MAXCURSOR;
    createParameters();
    
    // Allocated and publish the systemInfo array
    systemInfo = OSArray::withCapacity(4);
    if (systemInfo) {
        setProperty(kNXSystemInfoKey, systemInfo);
    }
    
    // Let's go ahead and cache our registry name.
    // This was added to remove a call to getName while
    // we are disabling preemption
    registryName = getName();
    
    obj = copyProperty(kIOHIDPowerOnDelayNSKey, gIOServicePlane);
    if (obj != NULL) {
        number = OSDynamicCast(OSNumber, obj);
        if (number != NULL) {
            UInt64 value = number->unsigned64BitValue();
            if (value < kMillisecondScale) {
                // logging not yet available
            }
            else if (value > (10ULL * kSecondScale)) {
                // logging not yet available
            }
            else {
                setProperty(kIOHIDPowerOnDelayNSKey, number);
                gIOHIDPowerOnThresoldAbsoluteTime = value;
            }
        }
        obj->release();
    }
        
    /*
     * Start up the work loop
     */
    workLoop = IOHIDWorkLoop::workLoop();
    cmdGate = IOCommandGate::commandGate(this);
    periodicES = IOTimerEventSource::timerEventSource(this, (IOTimerEventSource::Action) &_periodicEvents );
    eventConsumerES = IOInterruptEventSource::interruptEventSource(this, (IOInterruptEventSource::Action) &doKickEventConsumer);
    keyboardEQES = IOInterruptEventSource::interruptEventSource(this, (IOInterruptEventSource::Action) &doProcessKeyboardEQ);
    _delayedNotificationSource = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &IOHIDSystem::doProcessNotifications));
    
    require(workLoop && cmdGate && periodicES && eventConsumerES && keyboardEQES && _delayedNotificationSource, exit_early);
    
    require_noerr(workLoop->addEventSource(cmdGate), exit_early);
    require_noerr(workLoop->addEventSource(periodicES), exit_early);
    require_noerr(workLoop->addEventSource(eventConsumerES), exit_early);
    require_noerr(workLoop->addEventSource(keyboardEQES), exit_early);
    require_noerr(workLoop->addEventSource(_delayedNotificationSource), exit_early);
    
    publishNotify = addMatchingNotification(gIOPublishNotification,
                                            matchingDevice,
                                            iohidNotificationHandler,
                                            this,
                                            (void *)&IOHIDSystem::handlePublishNotification );
    require(publishNotify, exit_early);
    
    eventPublishNotify = addMatchingNotification(gIOPublishNotification,
                                                 matchingService,
                                                 iohidNotificationHandler,
                                                 this,
                                                 (void *)&IOHIDSystem::handlePublishNotification );
    require(eventPublishNotify, exit_early);
    
    terminateNotify = addMatchingNotification(gIOTerminatedNotification,
                                              matchingDevice,
                                              iohidNotificationHandler,
                                              this,
                                              (void *)&IOHIDSystem::handleTerminateNotification );
    require(terminateNotify, exit_early);
    
    eventTerminateNotify = addMatchingNotification(gIOTerminatedNotification,
                                                   matchingService,
                                                   iohidNotificationHandler,
                                                   this,
                                                   (void *)&IOHIDSystem::handleTerminateNotification );
    require(eventTerminateNotify, exit_early);
    
    // RY: Listen to the root domain
    rootDomain = (IOService *)getPMRootDomain();
    
    if (rootDomain)
        rootDomain->registerInterestedDriver(this);
    
    registerPrioritySleepWakeInterest(powerStateHandler, this, 0);
        
    _displayWranglerMatching = addMatchingNotification(gIOPublishNotification,
                                                       matchingWrangler,
                                                       iohidNotificationHandler,
                                                       this,
                                                       (void *)&IOHIDSystem::handlePublishNotification);
    require(_displayWranglerMatching, exit_early);
    
    // Get notified everytime AppleKeyswitch registers (each time keyswitch changes)
    gSwitchNotification = addMatchingNotification(gIOPublishNotification,
                                                  matchingKeyswitch,
                                                  keySwitchNotificationHandler,
                                                  this,
                                                  0);
    require(gSwitchNotification, exit_early);
    
    /*
     * IOHIDSystem serves both as a service and a nub (we lead a double
     * life).  Register ourselves as a nub to kick off matching.
     */
    
#if !TARGET_OS_EMBEDDED
    _hidActivityThread = thread_call_allocate(hidActivityThread_cb, (thread_call_param_t)this);
    _hidActivityIdle = true;
    require(_hidActivityThread, exit_early);
#endif

    registerService();
    iWasStarted = true;

exit_early:
    matchingDevice->release();
    matchingService->release();
    matchingWrangler->release();
    matchingKeyswitch->release();
    
    if (!iWasStarted)
        evInstance = 0;
    
    return iWasStarted;
}

void IOHIDSystem::setDisplaySleepDrivenByPM(bool val)
{
  displaySleepDrivenByPM = val;
}

IOReturn IOHIDSystem::powerStateHandler( void *target, void *refCon __unused,
                        UInt32 messageType, IOService *service __unused, void *messageArgs, vm_size_t argSize __unused)
{

   IOPMSystemCapabilityChangeParameters * params;
   IOHIDSystem*  myThis = OSDynamicCast( IOHIDSystem, (OSObject*)target );


   if ( messageType != kIOMessageSystemCapabilityChange ) {
      // We are not interested in anything other than cap change.
      return kIOReturnSuccess;
   }
   params = (IOPMSystemCapabilityChangeParameters *) messageArgs;

   if ((params->changeFlags & kIOPMSystemCapabilityWillChange) &&
       (params->fromCapabilities & kIOPMSystemCapabilityGraphics) &&
       (params->toCapabilities & kIOPMSystemCapabilityGraphics) == 0) {

      /*
       * This display sleep driven by IOPMrootDomain. Don't let HID activity
       * tickle display
       */
      myThis->setDisplaySleepDrivenByPM(true);

   }
   return kIOReturnSuccess;
}

// powerStateDidChangeTo
//
// The display wrangler has changed state, so the displays have changed
// state, too.  We save the new state.

IOReturn IOHIDSystem::powerStateDidChangeTo( IOPMPowerFlags theFlags, unsigned long state, IOService * service)
{
    IOHID_DEBUG(kIOHIDDebugCode_PowerStateChangeEvent, service, state, theFlags, 0);
    if (service == displayManager)
    {
        displayState = theFlags;
        if (theFlags & IOPMDeviceUsable) {
            clock_get_uptime(&displayStateChangeDeadline);
            ADD_ABSOLUTETIME(&displayStateChangeDeadline,
                             &gIOHIDRelativeTickleThresholdAbsoluteTime);
            AbsoluteTime_to_scalar(&displaySleepWakeupDeadline) = 0;

           // Reset all flags. Flag key release events may not get delivered as they
           // come late in system sleep or display sleep process.
           updateEventFlags(0);
        }
        else {
             // If the display has transitioned from usable to unusable state
             // because of display sleep  then set the deadline before which
             // pointer movement can bring the display to usable state.
             // Also make sure that this display sleep is not driven by IOPM as part of system sleep

           if ( !CMP_ABSOLUTETIME(&displaySleepWakeupDeadline, &gIOHIDZeroAbsoluteTime) && 
                   (kOSBooleanFalse == rootDomain->copyProperty("DisplayIdleForDemandSleep"))) {
             clock_get_uptime(&displaySleepWakeupDeadline);
             ADD_ABSOLUTETIME(&displaySleepWakeupDeadline, &gIOHIDDisplaySleepAbortThresholdAbsoluteTime);
           }
           displaySleepDrivenByPM = false; // Reset flag for next use

           // Force set HID activity to idle
           _lastTickleTime = 0;
           thread_call_enter(_hidActivityThread);
        }

    }
    else if (service == rootDomain)
    {
        if (theFlags & kIOPMPowerOn)
        {
            clock_get_uptime(&rootDomainStateChangeDeadline);
            ADD_ABSOLUTETIME(&rootDomainStateChangeDeadline, &gIOHIDPowerOnThresoldAbsoluteTime);
            ClearCachedMouseButtonStates(cachedButtonStates);
        }
    }
    return IOPMNoErr;
}

bool IOHIDSystem::genericNotificationHandler(void * handler,
                                             IOService * newService,
                                             IONotifier * /* notifier */)
{
    bool result = false;

    if (handler && newService && _delayedNotificationSource) {
        IOHIDSystem_notificationData    rawData = {(IOCommandGate::Action)handler, newService};
        OSData                          *data = OSData::withBytes(&rawData, sizeof(rawData));

        if (data) {
            newService->retain();
            IOLockLock(_delayedNotificationLock);
            _delayedNotificationArray->setObject(data);
            retain(); // Retain IOHIDSystem in case it tries to go away before this notification is processed.
            IOLockUnlock(_delayedNotificationLock);
            data->release();
            _delayedNotificationSource->setTimeoutUS(1);
            result = true;
        }
    }

    return result;
}

void IOHIDSystem::doProcessNotifications(IOTimerEventSource *sender __unused)
{
    bool reschedule = false;
    
    while (_delayedNotificationArray->getCount() > 0) {
        // retrieve the first item from the queue
        IOLockLock(_delayedNotificationLock);
        OSData *notificationData = OSDynamicCast(OSData, _delayedNotificationArray->getObject(0));
        if (notificationData) {
            notificationData->retain();
        }
        _delayedNotificationArray->removeObject(0);
        IOLockUnlock(_delayedNotificationLock);
        
        // process the notification
        if (notificationData) {
            const IOHIDSystem_notificationData *data = (const IOHIDSystem_notificationData *)notificationData->getBytesNoCopy();
            cmdGate->runAction(data->handler, data->newService);
            data->newService->release();
            notificationData->release();
            release(); // IOHIDSystem was retained in case it tried to go away before this notification was processed.
        }
    }
    
    if (reschedule) {
        _delayedNotificationSource->setTimeoutUS(1);
    }
}


bool IOHIDSystem::handlePublishNotification(
            void * target,
            IOService * newService )
{
    IOHIDSystem * self = (IOHIDSystem *) target;

    if (newService->isInactive()) {
        // device went away before we could add it. ignore.
        return true;
    }
    
    // avoiding OSDynamicCast & dependency on graphics family
    if( newService->metaCast("IODisplayWrangler")) {
        if( !self->displayManager) {
            self->displayManager = newService;
            self->displayState = newService->registerInterestedDriver(self);
        }
        return true;
    }

    self->attach( newService );

    if( OSDynamicCast(IOHIDevice, newService) ||
        OSDynamicCast(IOHIDEventService, newService)) {
        OSNumber *altSender = OSDynamicCast(OSNumber, newService->getProperty(kIOHIDAltSenderIdKey, gIOServicePlane));
        if (altSender) {
            self->_privateData->senderIDDictionary->setObject((const OSSymbol *)newService, altSender);
        }
        
        if (self->ioHIDevices) {
            if (self->ioHIDevices->getNextIndexOfObject(newService, 0) == (unsigned)-1)
                self->ioHIDevices->setObject(newService);
        }

        if (OSDynamicCast(IOHIPointing, newService))
        {
            AppendNewCachedMouseEventForService(self->cachedButtonStates, newService);
        }

        OSArray * newSystemInfo = OSArray::withArray(self->systemInfo);
        if ( newSystemInfo )
        {
            AppendNewNXSystemInfoForService(newSystemInfo, newService);
            self->setProperty(kNXSystemInfoKey, newSystemInfo);
            OSSafeReleaseNULL(self->systemInfo);
            self->systemInfo = newSystemInfo;
        }

        if(self->eventsOpen || OSDynamicCast(IOHIKeyboard, newService))
            self->registerEventSource( newService );
    }

    return true;
}

bool IOHIDSystem::handleTerminateNotification(
            void * target,
            IOService * service )
{
    IOHIDSystem * self = (IOHIDSystem *) target;
    OSArray * newSystemInfo = NULL;
    int index;
    
    require(OSDynamicCast(IOHIDSystem, self), exit_early);
    if( self->eventsOpen && (
        OSDynamicCast(IOHIDevice, service) ||
        OSDynamicCast(IOHIDEventService, service)))
    {
        service->close(self);
        self->_privateData->senderIDDictionary->removeObject((const OSSymbol *)service);
    }

    // <rdar://problem/14116334&14536084&14757282&14775621>
    // self->detach(service);

    if (self->ioHIDevices) {
        if ((index = self->ioHIDevices->getNextIndexOfObject(service, 0)) != -1)
            self->ioHIDevices->removeObject(index);
    }

    newSystemInfo = OSArray::withArray(self->systemInfo);
    if ( newSystemInfo )
    {
        RemoveNXSystemInfoForService(newSystemInfo, service);
        self->setProperty(kNXSystemInfoKey, newSystemInfo);
        OSSafeReleaseNULL(self->systemInfo);
        self->systemInfo = newSystemInfo;
    }

    // RY: Remove this object from the cachedButtonState
    if (OSDynamicCast(IOHIPointing, service))
    {
        // Clear the service button state
        AbsoluteTime    ts;
        clock_get_uptime(&ts);
        if ( (self->displayState & IOPMDeviceUsable) ) {
            self->relativePointerEvent(0, 0, 0, ts, service);
        }

        CachedMouseEventStruct *cachedMouseEvent;
        if ((cachedMouseEvent = GetCachedMouseEventForService(self->cachedButtonStates, service)) &&
            (cachedMouseEvent->proximityData.proximity.enterProximity))
        {
            cachedMouseEvent->proximityData.proximity.enterProximity = false;
            cachedMouseEvent->state |= kCachedMousePointingTabletEventPendFlag;
            self->proximityEvent(&(cachedMouseEvent->proximityData), ts, service);
            cachedMouseEvent->state &= ~kCachedMousePointingTabletEventPendFlag;

            IOGBounds   bounds = {0, 0, 0, 0};
            IOGPoint   newLoc = {0, 0};
            self->absolutePointerEvent(0, &newLoc, &bounds, false, 0, 0, ts, service);
        }

        RemoveCachedMouseEventForService(self->cachedButtonStates, service);
    }
    
exit_early:
    return true;
}

/*
 * Free locally allocated resources, and then ourselves.
 */
void IOHIDSystem::free()
{
    if (cmdGate) {
        evClose();
    }

    // we are going away. stop the workloop.
    if (workLoop) {
        workLoop->disableAllEventSources();
    }

    if (periodicES) {
        periodicES->cancelTimeout();
        
        if ( workLoop )
            workLoop->removeEventSource( periodicES );
        
        periodicES->release();
        periodicES = 0;
    }
    if (eventConsumerES) {
        eventConsumerES->disable();
        
        if ( workLoop )
            workLoop->removeEventSource( eventConsumerES );
        
        eventConsumerES->release();
        eventConsumerES = 0;
    }
    if (keyboardEQES) {
        keyboardEQES->disable();
        
        if ( workLoop )
            workLoop->removeEventSource( keyboardEQES );
        keyboardEQES->release();
        keyboardEQES = 0;
    }
    
    if (_privateData) {
        if (_delayedNotificationSource) {
            _delayedNotificationSource->cancelTimeout();
            if ( workLoop )
                workLoop->removeEventSource( _delayedNotificationSource );
            _delayedNotificationSource->release();
        }
        if (_delayedNotificationLock) {
            IOLockFree(_delayedNotificationLock);
            _delayedNotificationLock = 0;
        }
        OSSafeReleaseNULL(_delayedNotificationArray);
    }
    
    
    if (evScreen) IOFree( (void *)evScreen, evScreenSize );
    evScreen = (void *)0;
    evScreenSize = 0;

    if (publishNotify) {
        publishNotify->remove();
        publishNotify = 0;
    }
    if (gSwitchNotification) {
        gSwitchNotification->remove();
        gSwitchNotification = 0;
    }
    if (terminateNotify) {
        terminateNotify->remove();
        terminateNotify = 0;
    }
    if (eventPublishNotify) {
        eventPublishNotify->remove();
        eventPublishNotify = 0;
    }
    if (eventTerminateNotify) {
        eventTerminateNotify->remove();
        eventTerminateNotify = 0;
    }
    if (_displayWranglerMatching) {
        _displayWranglerMatching->remove();
        _displayWranglerMatching = 0;
    }
    
    OSSafeReleaseNULL(cmdGate); // gate is already closed
    OSSafeReleaseNULL(workLoop);
    OSSafeReleaseNULL(ioHIDevices);
    OSSafeReleaseNULL(cachedButtonStates);
    OSSafeReleaseNULL(consumedKeys);
    OSSafeReleaseNULL(systemInfo);

    if ( gKeyboardEQLock ) {
        IOLock * lock = gKeyboardEQLock;
        IOLockLock(lock);
        gKeyboardEQLock = 0;
        IOLockUnlock(lock);
        IOLockFree(lock);
    }

    OSSafeReleaseNULL(_hidKeyboardDevice);
    OSSafeReleaseNULL(_hidPointingDevice);

    if (_privateData) {
        _cursorHelper.finalize();
        IOFree((void*)_privateData, sizeof(ExpansionData));
        _privateData = NULL;
    }
    
    super::free();
}



/*
 * Open the driver for business.  This call must be made before
 * any other calls to the Event driver.  We can only be opened by
 * one user at a time.
 */
IOReturn IOHIDSystem::evOpen(void)
{
    IOReturn r = kIOReturnSuccess;

    if ( evOpenCalled == true )
    {
        r = kIOReturnBusy;
        goto done;
    }
    evOpenCalled = true;

    if (!evInitialized)
    {
        evInitialized = true;
        // Put code here that is to run on the first open ONLY.
    }

done:
    return r;
}

IOReturn IOHIDSystem::evClose(void){
    return cmdGate->runAction((IOCommandGate::Action)doEvClose);
}

IOReturn IOHIDSystem::doEvClose(IOHIDSystem *self)
                        /* IOCommandGate::Action */
{
    return self->evCloseGated();
}

IOReturn IOHIDSystem::evCloseGated(void)
{
    if ( evOpenCalled == false )
        return kIOReturnBadArgument;

    evStateChanging = true;

    // Early close actions here
    if( cursorEnabled)
        hideCursor();
    cursorStarted = false;
    cursorEnabled = false;

    // Release the input devices.
    detachEventSources();

    // Clear screens registry and related data
    if ( evScreen != (void *)0 )
    {
        screens = 0;
        lastShmemPtr = (void *)0;
    }
    // Remove port notification for the eventPort and clear the port out
    setEventPortGated(MACH_PORT_NULL);
//  ipc_port_release_send(event_port);

    // Clear local state to shutdown
    evStateChanging = false;
    evOpenCalled = false;
    eventsOpen = false;

    return kIOReturnSuccess;
}

//
// Dispatch state to screens registered with the Event Driver
// Pending state changes for a device may be coalesced.
//
//
// This should be run from a command gate action.
//
void IOHIDSystem::evDispatch(
               /* command */ EvCmd evcmd)
{
    if( !eventsOpen || (evcmd == EVLEVEL) || (evcmd == EVNOP))
        return;

    for( int i = 0; i < screens; i++ ) {
        bool onscreen = (0 != (cursorScreens & (1 << i)));

        if (onscreen) {
            EvScreen *esp = &((EvScreen*)evScreen)[i];

            if ( esp->instance ) {
                IOGPoint p;
                p.x = evg->screenCursorFixed.x / 256;   // Copy from shmem.
                p.y = evg->screenCursorFixed.y / 256;

                switch ( evcmd )
                {
                    case EVMOVE:
                        esp->instance->moveCursor(&p, evg->frame);
                        break;

                    case EVSHOW:
                        esp->instance->showCursor(&p, evg->frame);
                        break;

                    case EVHIDE:
                        esp->instance->hideCursor();
                        break;

                    default:
                        // should never happen
                        break;
                }
            }
        }
    }
}

//
// Dispatch mechanism for special key press.  If a port has been registered,
// a message is built to be sent out to that port notifying that the key has
// changed state.  A level in the range 0-64 is provided for convenience.
//
void IOHIDSystem::evSpecialKeyMsg(unsigned key,
          /* direction */ unsigned dir,
          /* flags */     unsigned f,
          /* level */     unsigned l)
{
    mach_port_t dst_port;
    struct evioSpecialKeyMsg *msg;

    static const struct evioSpecialKeyMsg init_msg =
        { { MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, // mach3xxx, is the right?
                        MACH_MSG_TYPE_MAKE_SEND),   // mach_msg_bits_t  msgh_bits;
            sizeof (struct evioSpecialKeyMsg),      // mach_msg_size_t  msgh_size;
            MACH_PORT_NULL,             // mach_port_t  msgh_remote_port;
            MACH_PORT_NULL,             // mach_port_t  msgh_local_port;
            0,                      // mach_msg_size_t msgh_reserved;
            EV_SPECIAL_KEY_MSG_ID           // mach_msg_id_t    msgh_id;
            },
            0,  /* key */
            0,  /* direction */
            0,  /* flags */
            0   /* level */
        };

    if ( (dst_port = specialKeyPort(key)) == MACH_PORT_NULL )
        return;
    msg = (struct evioSpecialKeyMsg *) IOMalloc(
                sizeof (struct evioSpecialKeyMsg) );
    if ( msg == NULL )
        return;

    // Initialize the message.
    bcopy( &init_msg, msg, sizeof (struct evioSpecialKeyMsg) );
    msg->Head.msgh_remote_port = dst_port;
    msg->key = key;
    msg->direction = dir;
    msg->flags = f;
    msg->level = l;

    // Send the message out from the command gate.
        cmdGate->runAction((IOCommandGate::Action)doSpecialKeyMsg,(void*)msg);
}

//
// Reset instance variables to their default state for mice/pointers
//

void IOHIDSystem::_resetMouseParameters(void)
{
    if ( eventsOpen == false )
        return;

    OSDictionary    *tempDict = OSDictionary::withCapacity(3);
    UInt64      nano;

    nanoseconds_to_absolutetime( EV_DCLICKTIME, &clickTimeThresh);
    clickSpaceThresh.x = clickSpaceThresh.y = EV_DCLICKSPACE;
    AbsoluteTime_to_scalar( &clickTime) = 0;
    clickLoc.x = clickLoc.y = -EV_DCLICKSPACE;
    clickState = 1;

    if (tempDict) {
        UInt32  tempClickSpace[] = {clickSpaceThresh.x, clickSpaceThresh.y};
        makeInt32ArrayParamProperty( tempDict, kIOHIDClickSpaceKey,
                                    tempClickSpace, sizeof(tempClickSpace)/sizeof(UInt32) );

        nano = EV_DCLICKTIME;
        makeNumberParamProperty( tempDict, kIOHIDClickTimeKey,
                                nano, 64 );

        setParamProperties(tempDict);

        tempDict->release();
    }
}

////////////////////////////////////////////////////////////////////////////
//#define LOG_SCREEN_REGISTRATION
#ifdef LOG_SCREEN_REGISTRATION
#warning LOG_SCREEN_REGISTRATION is defined
#define log_screen_reg(fmt, args...)  kprintf(">>> " fmt, args)
#else
#define log_screen_reg(fmt, args...)
#endif

////////////////////////////////////////////////////////////////////////////
int
IOHIDSystem::registerScreen(IOGraphicsDevice * io_gd,
                            IOGBounds * boundsPtr,
                            IOGBounds * virtualBoundsPtr)
{
    int result = -1;

    // If we are not open for business, fail silently
    if (eventsOpen) {
        // for this version of the call, these must all be supplied
        if (!io_gd || !boundsPtr || !virtualBoundsPtr) {
            IOLog("%s invalid call %p %p %p\n", __PRETTY_FUNCTION__, io_gd, boundsPtr, virtualBoundsPtr);
        }
        else {
            UInt32 index;
            IOReturn ret = workLoop->runAction((IOWorkLoop::Action)&IOHIDSystem::doRegisterScreen,
                                               this, io_gd, boundsPtr, virtualBoundsPtr, &index);
            if (ret == kIOReturnSuccess) {
                result = SCREENTOKEN + index;
            }
            else {
                IOLog("%s failed %08x\n", __PRETTY_FUNCTION__, ret);
            }
        }
    }
    log_screen_reg("%s: registered token %d\n", __PRETTY_FUNCTION__, result);
    return result;
}

////////////////////////////////////////////////////////////////////////////
IOReturn
IOHIDSystem::extRegisterVirtualDisplay(void* token_ptr,void*,void*,void*,void*,void*)
{
    IOReturn result = kIOReturnBadArgument;
    if (token_ptr) {
        SInt32 index;
        UInt64 *token = (UInt64 *)token_ptr;
        result = workLoop->runAction((IOWorkLoop::Action)&IOHIDSystem::doRegisterScreen,
                                     this, NULL, NULL, NULL, &index);
        if ((index >= 0) && (index < EV_MAX_SCREENS)) {
            *token = SCREENTOKEN + index;
        }
        else {
            *token = 0;
            if (result == kIOReturnSuccess) {
                result = kIOReturnInternalError;
                IOLog("IOHIDSystem tried to return an invalid token with no error\n");
            }
        }
        log_screen_reg("%s: registered token %lld on %x\n", __PRETTY_FUNCTION__, *token, result);
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////
IOReturn
IOHIDSystem::doRegisterScreen(IOHIDSystem *self,
                              IOGraphicsDevice *io_gd,
                              IOGBounds *bounds,
                              IOGBounds *virtualBounds,
                              void *index)
{
    return self->registerScreenGated(io_gd, bounds, virtualBounds, (SInt32*)index);
}

////////////////////////////////////////////////////////////////////////////
//#define LOG_SCROLL_STATE
#ifdef LOG_SCROLL_STATE
#   define log_scroll_state(s, ...)     kprintf(">>> %s:%d " s, "IOHIDSystem", __LINE__, __VA_ARGS__)
#   define log_scroll_state_b(s, ...)   kprintf(">>> %s:%d " s, "IOHIDSystem", __LINE__, __VA_ARGS__)
#else
#   define log_scroll_state(s, ...)
#   define log_scroll_state_b(s, ...)
#endif

////////////////////////////////////////////////////////////////////////////
IOReturn
IOHIDSystem::registerScreenGated(IOGraphicsDevice *io_gd,
                                 IOGBounds *boundsPtr,
                                 IOGBounds *virtualBoundsPtr,
                                 SInt32 *index)
{
    EvScreen *screen_ptr = NULL;
    OSNumber *num = NULL;
    IOReturn result = kIOReturnSuccess;
    *index = -1;

    if ( lastShmemPtr == (void *)0 )
        lastShmemPtr = evs;

    /* shmemSize and bounds already set */
    log_screen_reg("%s %p %p %p\n", __func__, io_gd, boundsPtr, virtualBoundsPtr);

    // locate next available screen
    for (int i = 0; (i < EV_MAX_SCREENS) && (screen_ptr == NULL); i++) {
        screen_ptr = &((EvScreen*)evScreen)[i];

        if (io_gd && (screen_ptr->instance == io_gd)) {
            // Empty slot.
            log_screen_reg("%s refound at index %d\n", __func__, i);
            *index = i;
        }
        else if (screen_ptr->creator_pid) {
            proc_t isStillAlive = proc_find(screen_ptr->creator_pid);
            if (isStillAlive) {
                // Still in use.
                screen_ptr = NULL;
                proc_rele(isStillAlive);
            }
            else {
                // Dead head.
                IOLog("IOHIDSystem::%s: Screen %d recycled from pid %d\n", __func__, i, screen_ptr->creator_pid);
                *index = i;
                screen_ptr->creator_pid = 0;
                screen_ptr->displayBounds = NULL;
                screen_ptr->desktopBounds = NULL;
            }
        }
        else if (screen_ptr->instance) {
            // Still in use.
            screen_ptr = NULL; // try the next one
        }
        else {
            // New slot
            log_screen_reg("%s new index at %d\n", __func__, i);
            *index = i;
        }
    }

    if (!screen_ptr) {
        IOLog("IOHIDSystem::%s: No space found for new screen\n", __func__);
        result = kIOReturnNoResources;
    }
    else if (io_gd && boundsPtr && virtualBoundsPtr) {
        // called by video driver. they maintain their own bounds.
        screen_ptr->instance = io_gd;
        screen_ptr->displayBounds = boundsPtr;
        screen_ptr->desktopBounds = virtualBoundsPtr;
        screen_ptr->creator_pid = 0; // kernel made

        // Update our idea of workSpace bounds
        if ( boundsPtr->minx < workSpace.minx )
            workSpace.minx = boundsPtr->minx;
        if ( boundsPtr->miny < workSpace.miny )
            workSpace.miny = boundsPtr->miny;
        if ( boundsPtr->maxx < workSpace.maxx )
            workSpace.maxx = boundsPtr->maxx;
        if ( screen_ptr->displayBounds->maxy < workSpace.maxy )
            workSpace.maxy = boundsPtr->maxy;

        // perform other bookkeeping
        num = (OSNumber*)io_gd->copyProperty(kIOFBWaitCursorFramesKey);
        if(OSDynamicCast(OSNumber, num) &&
           (num->unsigned32BitValue() > maxWaitCursorFrame)) {
            firstWaitCursorFrame = 0;
            maxWaitCursorFrame   = num->unsigned32BitValue();
            evg->lastFrame       = maxWaitCursorFrame;
        }
        OSSafeReleaseNULL(num);
        num = (OSNumber*)io_gd->copyProperty(kIOFBWaitCursorPeriodKey);
        if( OSDynamicCast(OSNumber, num) ) {
            clock_interval_to_absolutetime_interval(num->unsigned32BitValue(), kNanosecondScale, &_cursorWaitDelta);
        }
        OSSafeReleaseNULL(num);
    }
    else if (!io_gd && !boundsPtr && !virtualBoundsPtr) {
        // called by window server. we maintain the bounds.
        screen_ptr->displayBounds = (IOGBounds*)screen_ptr->scratch;
        screen_ptr->desktopBounds = screen_ptr->displayBounds + 1;
        screen_ptr->creator_pid = proc_selfpid();

        // default the bounds to lala land
        screen_ptr->displayBounds->minx = screen_ptr->desktopBounds->minx = screen_ptr->displayBounds->miny = screen_ptr->desktopBounds->miny = -30001;
        screen_ptr->displayBounds->maxx = screen_ptr->desktopBounds->maxx = screen_ptr->displayBounds->maxy = screen_ptr->desktopBounds->maxy = -30000;
    }
    else {
        result = kIOReturnBadArgument;
    }

    if (result == kIOReturnSuccess) {
        log_screen_reg(">>> display %d is for %d\n", *index, screen_ptr->creator_pid);
        if (*index >= screens) {
            screens = 1 + *index;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////
void IOHIDSystem::unregisterScreen(int token) {
    uintptr_t index = token - SCREENTOKEN;
    log_screen_reg("%s: unregistering token %d\n", __PRETTY_FUNCTION__, token);
    if (index < EV_MAX_SCREENS) {
        IOReturn ret = cmdGate->runAction((IOCommandGate::Action)doUnregisterScreen, (void *)index);
        if (ret != kIOReturnSuccess) {
            IOLog("%s recieved %08x for token %d.\n", __PRETTY_FUNCTION__, ret, token);
        }
    }
    else {
        IOLog("%s called with invalid token %d.\n", __PRETTY_FUNCTION__, token);
    }
}

////////////////////////////////////////////////////////////////////////////
IOReturn
IOHIDSystem::extUnregisterVirtualDisplay(void* token_ptr,void*,void*,void*,void*,void*)
{
    IOReturn result = kIOReturnSuccess;
    uintptr_t token = (uintptr_t)token_ptr;
    uintptr_t index = token - SCREENTOKEN;
    if (index < EV_MAX_SCREENS) {
        result = cmdGate->runAction((IOCommandGate::Action)doUnregisterScreen, (void *)index);
    }
    else {
        IOLog("%s called with invalid token %d.\n", __PRETTY_FUNCTION__, (int)token);
        result = kIOReturnBadArgument;
    }
    log_screen_reg("%s: unregistering token %lu on %x\n", __PRETTY_FUNCTION__, token, result);

    return result;
}

////////////////////////////////////////////////////////////////////////////
IOReturn IOHIDSystem::doUnregisterScreen (IOHIDSystem *self, void * arg0, void *arg1)
                        /* IOCommandGate::Action */
{
    uintptr_t index = (uintptr_t) arg0;
    uintptr_t internal = (uintptr_t) arg1;

    return self->unregisterScreenGated(index, internal);
}

////////////////////////////////////////////////////////////////////////////
IOReturn IOHIDSystem::unregisterScreenGated(int index, bool internal)
{
    IOReturn result = kIOReturnSuccess;
    log_screen_reg("%s %d %d %d\n", __func__, index, internal, screens);

    if ( eventsOpen == false || index >= screens ) {
        result = kIOReturnNoResources;
    }
    else {
        EvScreen *screen_ptr = ((EvScreen*)evScreen)+index;

        if (!screen_ptr->displayBounds) {
            IOLog("%s called with invalid index %d\n", __PRETTY_FUNCTION__, index);
            result = kIOReturnBadArgument;
        }
        else if (internal && !screen_ptr->instance) {
            IOLog("%s called internally on an external device %d\n", __PRETTY_FUNCTION__, index);
            result = kIOReturnNoDevice;
        }
        else if (!internal && screen_ptr->instance) {
            IOLog("%s called externally on an internal device %d\n", __PRETTY_FUNCTION__, index);
            result = kIOReturnNotPermitted;
        }
        else {
            hideCursor();

            // clear the variables
            screen_ptr->instance = NULL;
            screen_ptr->desktopBounds = NULL;
            screen_ptr->displayBounds = NULL;
            screen_ptr->creator_pid = 0;

            // Put the cursor someplace reasonable if it was on the destroyed screen
            cursorScreens &= ~(1 << index);
            // This will jump the cursor back on screen
            setCursorPosition((IOGPoint *)&evg->cursorLoc, true);

            showCursor();
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////
IOReturn
IOHIDSystem::extSetVirtualDisplayBounds(void* token_ptr,void* minx,void* maxx,void* miny,void* maxy,void*)
{
    IOReturn result = kIOReturnSuccess;
    uintptr_t token = (uintptr_t)token_ptr;
    uintptr_t index = token - SCREENTOKEN;
    log_screen_reg("%s: set bounds on token %lu\n", __PRETTY_FUNCTION__, token);
    if (index < EV_MAX_SCREENS) {
        IOGBounds tempBounds = { (uintptr_t) minx, (uintptr_t) maxx, (uintptr_t) miny, (uintptr_t) maxy };
        result = cmdGate->runAction((IOCommandGate::Action)doSetDisplayBounds, (void*) index, (void*) &tempBounds);
    }
    else {
        IOLog("%s called with invalid token %d.\n", __PRETTY_FUNCTION__, (int)token);
        result = kIOReturnBadArgument;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////
IOReturn
IOHIDSystem::doSetDisplayBounds (IOHIDSystem *self, void * arg0, void * arg1)
{
    uintptr_t index = (uintptr_t) arg0;
    IOGBounds *tempBounds = (IOGBounds*) arg1;

    return self->setDisplayBoundsGated(index, tempBounds);
}

////////////////////////////////////////////////////////////////////////////
IOReturn
IOHIDSystem::setDisplayBoundsGated (UInt32 index, IOGBounds *tempBounds)
{
    IOReturn result = kIOReturnSuccess;
    log_screen_reg("%s ((%d,%d),(%d,%d))\n", __func__, tempBounds->minx, tempBounds->miny, tempBounds->maxx, tempBounds->maxy);

    if ( eventsOpen == false || index >= (UInt32)screens ) {
        result = kIOReturnNoResources;
    }
    else {
        EvScreen *screen_ptr = ((EvScreen*)evScreen)+index;

        if (screen_ptr->instance) {
            IOLog("%s called on an internal device %d\n", __PRETTY_FUNCTION__, (int)index);
            result = kIOReturnNotPermitted;
        }
        else if (!screen_ptr->displayBounds || !screen_ptr->desktopBounds) {
            IOLog("%s called with invalid index %d\n", __PRETTY_FUNCTION__, (int)index);
            result = kIOReturnBadArgument;
        }
        else {
            // looks good
            hideCursor();
            *(screen_ptr->displayBounds) = *(screen_ptr->desktopBounds) = *tempBounds;
            // Put the cursor someplace reasonable if it was on the moved screen
            cursorScreens &= ~(1 << index);
            // This will jump the cursor back on screen
            setCursorPosition((IOGPoint *)&evg->cursorLoc, true);
            showCursor();
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////
/* Member of EventClient protocol
 *
 * Absolute position input devices and some specialized output devices
 * may need to know the bounding rectangle for all attached displays.
 * The following method returns a IOGBounds* for the workspace.  Please note
 * that the bounds are kept as signed values, and that on a multi-display
 * system the minx and miny values may very well be negative.
 */
IOGBounds * IOHIDSystem::workspaceBounds()
{
    return &workSpace;
}

IOReturn IOHIDSystem::registerEventQueue(IODataQueue * queue)
{
    return cmdGate->runAction((IOCommandGate::Action)doRegisterEventQueue, (void *)queue);
}

IOReturn IOHIDSystem::doRegisterEventQueue (IOHIDSystem *self, void * arg0)
                        /* IOCommandGate::Action */
{
    return self->registerEventQueueGated((IODataQueue *)arg0);
}

IOReturn IOHIDSystem::registerEventQueueGated(void * p1)
{
    IODataQueue * queue = (IODataQueue *)p1;
    if ( !queue )
        return kIOReturnBadArgument;

    if ( !dataQueueSet )
        dataQueueSet = OSSet::withCapacity(4);

    dataQueueSet->setObject(queue);

    return kIOReturnSuccess;
}

IOReturn IOHIDSystem::unregisterEventQueue(IODataQueue * queue)
{
    return cmdGate->runAction((IOCommandGate::Action)doUnregisterEventQueue, (void *)queue);
}

IOReturn IOHIDSystem::doUnregisterEventQueue (IOHIDSystem *self, void * arg0)
                        /* IOCommandGate::Action */
{
    return self->unregisterEventQueueGated((IODataQueue *)arg0);
}

IOReturn IOHIDSystem::unregisterEventQueueGated(void * p1)
{
    IODataQueue * queue = (IODataQueue *)p1;

    if ( !queue )
        return kIOReturnBadArgument;

    if ( dataQueueSet )
        dataQueueSet->removeObject(queue);

    return kIOReturnSuccess;
}

IOReturn IOHIDSystem::createShmem(void* p1, void*, void*, void*, void*, void*)
{                                                                    // IOMethod
    return cmdGate->runAction((IOCommandGate::Action)doCreateShmem, p1);
}

IOReturn IOHIDSystem::doCreateShmem (IOHIDSystem *self, void * arg0)
                        /* IOCommandGate::Action */
{
    return self->createShmemGated(arg0);
}

IOReturn IOHIDSystem::createShmemGated(void* p1)
{

    int                 shmemVersion = (uintptr_t)p1;
    IOByteCount         size;
    bool                clean = false;

    if ( shmemVersion < kIOHIDLastCompatibleShmemVersion ) {
        IOLog("IOHIDSystem::createShmemGated called with low version: %d < %d\n", shmemVersion, kIOHIDLastCompatibleShmemVersion);
        return kIOReturnUnsupported;
    }

    if ( shmemVersion > kIOHIDCurrentShmemVersion ) {
        IOLog("IOHIDSystem::createShmemGated called with hi version: %d > %d\n", shmemVersion, kIOHIDCurrentShmemVersion);
        return kIOReturnUnsupported;
    }

    if ( 0 == globalMemory) {

        size = sizeof(EvOffsets) + sizeof(EvGlobals);
        globalMemory = IOBufferMemoryDescriptor::withOptions( kIODirectionNone | kIOMemoryKernelUserShared, size );

        if ( !globalMemory)
            return kIOReturnNoMemory;

        shmem_addr = (uintptr_t) globalMemory->getBytesNoCopy();
        shmem_size = size;

        clean = true;
    }

    initShmem(clean);

    return kIOReturnSuccess;
}

// Initialize the shared memory area.
//
// This should be run from a command gate action.
void IOHIDSystem::initShmem(bool clean)
{
    int  i;
    EvOffsets *eop;
    int oldFlags = 0;

    /* top of sharedMem is EvOffsets structure */
    eop = (EvOffsets *) shmem_addr;

    if (!clean) {
        oldFlags = ((EvGlobals *)((char *)shmem_addr + sizeof(EvOffsets)))->eventFlags;
    }

    bzero( (void*)shmem_addr, shmem_size);

    /* fill in EvOffsets structure */
    eop->evGlobalsOffset = sizeof(EvOffsets);
    eop->evShmemOffset = eop->evGlobalsOffset + sizeof(EvGlobals);

    /* find pointers to start of globals and private shmem region */
    evg = (EvGlobals *)((char *)shmem_addr + eop->evGlobalsOffset);
    evs = (void *)((char *)shmem_addr + eop->evShmemOffset);

    evg->version = kIOHIDCurrentShmemVersion;
    evg->structSize = sizeof( EvGlobals);

    /* Set default wait cursor parameters */
    evg->waitCursorEnabled = TRUE;
    evg->globalWaitCursorEnabled = TRUE;
    evg->lastFrame = maxWaitCursorFrame;
    evg->waitThreshold = (12 * EV_TICKS_PER_SEC) / 10;

    evg->buttons = 0;
    evg->eNum = INITEVENTNUM;
    evg->eventFlags = oldFlags;

    evg->cursorLoc.x = _cursorHelper.desktopLocation().xValue().as32();
    evg->cursorLoc.y = _cursorHelper.desktopLocation().yValue().as32();
    evg->desktopCursorFixed.x = _cursorHelper.desktopLocation().xValue().asFixed24x8();
    evg->desktopCursorFixed.y = _cursorHelper.desktopLocation().yValue().asFixed24x8();
    evg->screenCursorFixed.x = _cursorHelper.getScreenLocation().xValue().asFixed24x8();
    evg->screenCursorFixed.y = _cursorHelper.getScreenLocation().yValue().asFixed24x8();

    evg->updateCursorPositionFromFixed = 0;
    evg->logCursorUpdates = 0;
    evg->dontCoalesce = 0;
    evg->dontWantCoalesce = 0;
    evg->wantPressure = 0;
    evg->wantPrecision = 0;
    evg->mouseRectValid = 0;
    evg->movedMask = 0;
    evg->cursorSema = OS_SPINLOCK_INIT;
    evg->waitCursorSema = OS_SPINLOCK_INIT;

    /* Set up low-level queues */
    lleqSize = LLEQSIZE;
    for (i=lleqSize; --i != -1; ) {
        evg->lleq[i].event.type = 0;
        AbsoluteTime_to_scalar(&evg->lleq[i].event.time) = 0;
        evg->lleq[i].event.flags = 0;
        evg->lleq[i].sema = OS_SPINLOCK_INIT;
        evg->lleq[i].next = i+1;
    }
    evg->LLELast = 0;
    evg->lleq[lleqSize-1].next = 0;
    evg->LLEHead = evg->lleq[evg->LLELast].next;
    evg->LLETail = evg->lleq[evg->LLELast].next;

    _cursorLogTimed();

    // Set eventsOpen last to avoid race conditions.
    eventsOpen = true;
}

//
// Set the event port.  The event port is both an ownership token
// and a live port we hold send rights on.  The port is owned by our client,
// the WindowServer.  We arrange to be notified on a port death so that
// we can tear down any active resources set up during this session.
// An argument of PORT_NULL will cause us to forget any port death
// notification that's set up.
//
// This should be run from a command gate action.
//
void IOHIDSystem::setEventPort(mach_port_t port)
{
    if ((eventPort != port) && (workLoop))
        workLoop->runAction((IOWorkLoop::Action)&IOHIDSystem::doSetEventPort, this, (void*)port);
}

IOReturn IOHIDSystem::doSetEventPort(IOHIDSystem *self, void *port_void, void *arg1 __unused, void *arg2 __unused, void *arg3 __unused)
{
    self->setEventPortGated((mach_port_t)port_void);
    return kIOReturnSuccess;
}

void IOHIDSystem::setEventPortGated(mach_port_t port)
{
    static struct _eventMsg init_msg = { {
            // mach_msg_bits_t  msgh_bits;
            MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,0),
            // mach_msg_size_t  msgh_size;
            sizeof (struct _eventMsg),
            // mach_port_t  msgh_remote_port;
            MACH_PORT_NULL,
            // mach_port_t  msgh_local_port;
            MACH_PORT_NULL,
            // mach_msg_size_t  msgh_reserved;
            0,
            // mach_msg_id_t    msgh_id;
            0
        } };

    init_msg.h.msgh_remote_port = port;

    if ( eventMsg == NULL )
        eventMsg = IOMalloc( sizeof (struct _eventMsg) );

    // Initialize the events available message.
    *((struct _eventMsg *)eventMsg) = init_msg;
    eventPort = port;

        // RY: Added this check so that the event consumer
        // can get notified if the queue in not empty.
        // Otherwise the event consumer will never get a
        // notification.
        if (EventsInQueue())
            kickEventConsumer();
}

//
// Set the port to be used for a special key notification.  This could be more
// robust about letting ports be set...
//
IOReturn IOHIDSystem::setSpecialKeyPort(
                        /* keyFlavor */ int         special_key,
                        /* keyPort */   mach_port_t key_port)
{
    if ( special_key >= 0 && special_key < NX_NUM_SCANNED_SPECIALKEYS )
        _specialKeyPort[special_key] = key_port;
    return kIOReturnSuccess;
}

mach_port_t IOHIDSystem::specialKeyPort(int special_key)
{
    if ( special_key >= 0 && special_key < NX_NUM_SCANNED_SPECIALKEYS )
        return _specialKeyPort[special_key];
    return MACH_PORT_NULL;
}

//
// Set the port to be used for stack shot
//
void IOHIDSystem::setStackShotPort(mach_port_t port)
{
    stackShotPort = port;

    static IOHIDSystem_stackShotMessage init_msg =
    {
        {                                           // mach_msg_header_t    header
            MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,0),  // mach_msg_bits_t      msgh_bits;
            sizeof (init_msg),                          // mach_msg_size_t      msgh_size;
            MACH_PORT_NULL,                             // mach_port_t          msgh_remote_port;
            MACH_PORT_NULL,                             // mach_port_t          msgh_local_port;
            0,                                          // mach_msg_size_t      msgh_reserved;
            0                                           // mach_msg_id_t        msgh_id;
        },
        0                                           // UInt32               flavor
    };

    if ( stackShotMsg ) {
        IOFree(stackShotMsg, sizeof(IOHIDSystem_stackShotMessage));
        stackShotMsg = NULL;
    }

    if ( stackShotPort ) {
        if ( !(stackShotMsg = IOMalloc(sizeof(IOHIDSystem_stackShotMessage))) )
            return;

        // Initialize the events available message.
        init_msg.header.msgh_remote_port = stackShotPort;
        *((IOHIDSystem_stackShotMessage*)stackShotMsg) = init_msg;
    }
}

UInt32 IOHIDSystem::eventFlags()
{
    return evg ? (evg->eventFlags) : 0;
}

void IOHIDSystem::dispatchEvent(IOHIDEvent *event, IOOptionBits options __unused)
{
    if ( !event || !dataQueueSet)
        return;

    OSCollectionIterator *      iterator    = OSCollectionIterator::withCollection(dataQueueSet);
    IOHIDEventServiceQueue *    dataQueue   = NULL;

    if ( !iterator )
        return;

    while ((dataQueue = OSDynamicCast(IOHIDEventServiceQueue, iterator->getNextObject()))) {
        dataQueue->enqueueEvent(event);
    }

    iterator->release();

}

void IOHIDSystem::updateHidActivity()
{
#if !TARGET_OS_EMBEDDED
    clock_get_uptime(&_lastTickleTime);
    if (_hidActivityIdle)
        thread_call_enter(_hidActivityThread);
#endif
}

void IOHIDSystem::hidActivityChecker( )
{
    cmdGate->runAction((IOCommandGate::Action)reportUserHidActivity, NULL);
}

void IOHIDSystem::reportUserHidActivity(IOHIDSystem *self, void *args )
{
    self->reportUserHidActivityGated(args);
}

void IOHIDSystem::reportUserHidActivityGated(void *args __unused)
{
    AbsoluteTime deadline = 0;
    AbsoluteTime ts;
    static AbsoluteTime  idleHidActivity = 0;

    if (!idleHidActivity)
        nanoseconds_to_absolutetime(IDLE_HID_ACTIVITY_NSECS, &idleHidActivity);


    clock_get_uptime(&ts);
    if ((ts-_lastTickleTime)  < idleHidActivity) {
        if (_hidActivityIdle) {
            _hidActivityIdle = false;
            messageClients(kIOHIDSystemUserHidActivity, (void *)_hidActivityIdle );
        }
        clock_absolutetime_interval_to_deadline((idleHidActivity+_lastTickleTime-ts), &deadline);

        thread_call_enter_delayed(_hidActivityThread, deadline);
    }
    else if ( !_hidActivityIdle ) {
        _hidActivityIdle = true;
        messageClients(kIOHIDSystemUserHidActivity, (void *)_hidActivityIdle );
    }
}

IOReturn IOHIDSystem::extGetUserHidActivityState(void *arg0,void*,void*,void*,void*,void*)
{
    return cmdGate->runAction((IOCommandGate::Action)getUserHidActivityState, arg0);
}

IOReturn IOHIDSystem::getUserHidActivityState(IOHIDSystem *self, void *arg0)
{
    return self->getUserHidActivityStateGated(arg0);
}

IOReturn IOHIDSystem::getUserHidActivityStateGated(void *state)
{
    if (state) {
        *((uint64_t*)state) = _hidActivityIdle ? 1 : 0;
        return kIOReturnSuccess;
    }
    return kIOReturnBadArgument;
}

//
// Helper functions for postEvent
//
static inline int myAbs(int a) { return(a > 0 ? a : -a); }

short IOHIDSystem::getUniqueEventNum()
{
    while (++evg->eNum == NULLEVENTNUM)
    ; /* sic */
    return(evg->eNum);
}

// postEvent
//
// This routine actually places events in the event queue which is in
// the EvGlobals structure.  It is called from all parts of the ev
// driver.
//
// This should be run from a command gate action.
//

void IOHIDSystem::postEvent(int           what,
             /* at */       IOFixedPoint64 *location,
             /* atTime */   AbsoluteTime  ts,
             /* withData */ NXEventData * myData,
             /* sender */   OSObject *    sender,
             /* extPID */   UInt32        extPID,
             /* processKEQ*/bool          processKEQ)
{
    // Clear out the keyboard queue up until this TS.  This should keep
    // the events in order.
    PROFILE_TRACE(7);
    if ( processKEQ )
        processKeyboardEQ(this, &ts);

    NXEQElement * theHead = (NXEQElement *) &evg->lleq[evg->LLEHead];
    NXEQElement * theLast = (NXEQElement *) &evg->lleq[evg->LLELast];
    NXEQElement * theTail = (NXEQElement *) &evg->lleq[evg->LLETail];
    int         wereEvents;

    if (CMP_ABSOLUTETIME(&ts, &lastEventTime) < 0) {
        ts = lastEventTime;
    }
    lastEventTime = ts;

    // dispatch new event
    // RY: doesn't appear there is any traction here after 6 years
#if 0
    IOHIDEvent * event = IOHIDEvent::withEventData(ts, what, myData);
    if ( event ) {
        dispatchEvent(event);
        event->release();
    }
#endif

    /* Some events affect screen dimming (idle time) */
    if (EventCodeMask(what) & NX_UNDIMMASK) {
        lastUndimEvent = ts;
    }

    wereEvents = EventsInQueue();

    xpr_ev_post("postEvent: what %d, X %d Y %d Q %d, needKick %d\n", what,location->x,location->y, EventsInQueue(), needToKickEventConsumer);
    IOHID_DEBUG(kIOHIDDebugCode_PostEvent, what, theHead, theTail, sender);

    if ((!evg->dontCoalesce) /* Coalescing enabled */
            && (theHead != theTail)
            && (theLast->event.type == what)
            && (EventCodeMask(what) & COALESCEEVENTMASK)
            && OSSpinLockTry(&theLast->sema)) {
        /* coalesce events */
        theLast->event.location.x = location->xValue().as64();
        theLast->event.location.y = location->yValue().as64();
        absolutetime_to_nanoseconds(ts, &theLast->event.time);
        if (myData != NULL)
            theLast->event.data = *myData;
        OSSpinLockUnlock(&theLast->sema);
    }
    else if (theTail->next != evg->LLEHead) {
        /* store event in tail */
        theTail->event.type         = what;
        // <rdar://problem/12682920> Task: Switch event.service_id to use registry ID
        // theTail->event.service_id   = (uintptr_t)sender;
        theTail->event.service_id = 0;
        if (sender) {
            OSNumber *altSender = OSDynamicCast(OSNumber, _senderIDDictionary->getObject((const OSSymbol *)sender));
            if (altSender) {
                theTail->event.service_id = altSender->unsigned64BitValue();
            }
            else {
                IORegistryEntry *entry = OSDynamicCast(IORegistryEntry, sender);
                if (entry) {
                    theTail->event.service_id = (uintptr_t)entry->getRegistryEntryID();
                }
                else {
                    theTail->event.service_id = getRegistryEntryID();
                }
            }
        }
        else {
            theTail->event.service_id = getRegistryEntryID();
        }
        theTail->event.ext_pid      = extPID;
        theTail->event.location.x   = location->xValue().as64();
        theTail->event.location.y   = location->yValue().as64();
        theTail->event.flags        = evg->eventFlags;
        absolutetime_to_nanoseconds(ts, &theLast->event.time);
        theTail->event.window = 0;

        if (myData != NULL)
            theTail->event.data = *myData;

        switch (what) {
            case NX_LMOUSEDOWN:
                theTail->event.data.mouse.eventNum =
                    leftENum = getUniqueEventNum();
                break;
            case NX_RMOUSEDOWN:
                theTail->event.data.mouse.eventNum =
                    rightENum = getUniqueEventNum();
                break;
            case NX_LMOUSEUP:
                theTail->event.data.mouse.eventNum = leftENum;
                leftENum = NULLEVENTNUM;
                // Inform the devices that the mouse was clicked
                notifyHIDevices(this, ioHIDevices, kIOHIDSystem508MouseClickMessage);
                break;
            case NX_RMOUSEUP:
                theTail->event.data.mouse.eventNum = rightENum;
                rightENum = NULLEVENTNUM;
                // Inform the devices that the mouse was clicked
                notifyHIDevices(this, ioHIDevices, kIOHIDSystem508MouseClickMessage);
                break;
        }

        if (EventCodeMask(what) & PRESSUREEVENTMASK) {
            // this case will not happen unless someone modifies PRESSUREEVENTMASK
            if (!((EventCodeMask(what) & MOUSEEVENTMASK) || (EventCodeMask(what) & MOVEDEVENTMASK)))
                IOLog("%s: postEvent unknown pressure event, cannot fill pressure.\n", registryName);
        }
        if (EventCodeMask(what) & MOUSEEVENTMASK) {
            /* Click state */
            AbsoluteTime delta = ts;
            SUB_ABSOLUTETIME( &delta, &clickTime);
            if ((CMP_ABSOLUTETIME(&delta, &clickTimeThresh) <= 0)
                    && (myAbs(location->xValue().as64() - clickLoc.x) <= clickSpaceThresh.x)
                    && (myAbs(location->yValue().as64() - clickLoc.y) <= clickSpaceThresh.y)) {
                if ((what == NX_LMOUSEDOWN)||(what == NX_RMOUSEDOWN)) {
                    clickTime = ts;
                    theTail->event.data.mouse.click = ++clickState;
                }
                else {
                    theTail->event.data.mouse.click = clickState;
                }
            }
            else if ((what == NX_LMOUSEDOWN)||(what == NX_RMOUSEDOWN)) {
                clickLoc = *location;
                clickTime = ts;
                clickState = 1;
                theTail->event.data.mouse.click = clickState;
            }
            else
                theTail->event.data.mouse.click = 0;
        }
#if PMON
        pmon_log_event(PMON_SOURCE_EV,
                       KP_EV_POST_EVENT,
                       what,
                       evg->eventFlags,
                       theClock);
#endif
        evg->LLETail = theTail->next;
        evg->LLELast = theLast->next;
        if ( ! wereEvents ) // Events available, so wake event consumer
            kickEventConsumer();
    }
    else {
        /*
         * if queue is full, ignore event, too hard to take care of all cases
         */
        static uint64_t next_log = 0;
        if (AbsoluteTime_to_scalar(&ts) > next_log)
        {
            IOLog("%s: postEvent LLEventQueue overflow.\n", registryName);
            nanoseconds_to_absolutetime(60000000000LL, (AbsoluteTime*)&next_log);
            next_log += AbsoluteTime_to_scalar(&ts);
        }
        kickEventConsumer();
#if PMON
        pmon_log_event( PMON_SOURCE_EV,
                        KP_EV_QUEUE_FULL,
                        what,
                        evg->eventFlags,
                        theClock);
#endif
    }
    PROFILE_TRACE(8);
}

/*
 * - kickEventConsumer
 *
 *  Try to send a message out to let the event consumer know that
 *  there are now events available for consumption.
 */

void IOHIDSystem::kickEventConsumer()
{
    xpr_ev_post("kickEventConsumer (need == %d)\n",
        needToKickEventConsumer,2,3,4,5);

    if ( needToKickEventConsumer == true )
        return;     // Request is already pending

    needToKickEventConsumer = true; // Posting a request now

        // Trigger eventConsumerES, so that doKickEventConsumer
        // is run from the workloop thread.
        eventConsumerES->interruptOccurred(0, 0, 0);
}

/*
 * - sendStackShotMessage
 *
 *  Try to send a message out to let the stack shot know we got
 *  the magic key sequence
 */

void IOHIDSystem::sendStackShotMessage(UInt32 flavor)
{
    kern_return_t r;
    mach_msg_header_t *msgh;

    xpr_ev_post("sendStackShotMessage\n", 1,2,3,4,5);

    if (stackShotMsg) {
        ((IOHIDSystem_stackShotMessage*)stackShotMsg)->flavor = flavor;
        msgh = (mach_msg_header_t *)stackShotMsg;
        if( msgh) {

            r = mach_msg_send_from_kernel( msgh, msgh->msgh_size);
            switch ( r ) {
                case MACH_SEND_TIMED_OUT:/* Already has a message posted */
                case MACH_MSG_SUCCESS:  /* Message is posted */
                    break;
                default:        /* Log the error */
                    IOLog("%s: sendStackShotMessage msg_send returned %d\n", registryName, r);
                    break;
            }
        }
    }
}

/*
 * HID System no longer runs the following methods on the EventSource thread.
 * Instead all are run from the caller thread through the use of command gates.
 * This will limit the amount of context switching that takes place.
 */


/*
 * The following methods are executed from the caller thread only.
 */

/*
 * The method is now called from a command gate and is run on the caller thread
 */

void IOHIDSystem::doSpecialKeyMsg(IOHIDSystem * self,
                    struct evioSpecialKeyMsg *msg) /* IOCommandGate::Action */
{
    kern_return_t r;

    xpr_ev_post("doSpecialKeyMsg 0x%x\n", msg,2,3,4,5);


    /* FIXME: Don't block */
    r = mach_msg_send_from_kernel( &msg->Head, msg->Head.msgh_size);

    xpr_ev_post("doSpecialKeyMsg: msg_send() == %d\n",r,2,3,4,5);
    if ( r != MACH_MSG_SUCCESS )
    {
        IOLog("%s: doSpecialKeyMsg msg_send returned %d\n",
            self->registryName, r);
    }
    if ( r == MACH_SEND_INVALID_DEST )  /* Invalidate the port */
    {
        self->setSpecialKeyPort(
          /* keyFlavor */ msg->key,
          /* keyPort   */ MACH_PORT_NULL);
    }
    IOFree( (void *)msg, sizeof (struct evioSpecialKeyMsg) );
}

/*
 * This is now being run from the workloop via an IOInterruptEventSource.
 * Note that we perform a non-blocking send.  The Event port in the event
 * consumer has a queue depth of 1 message.  Once the consumer picks up that
 * message, it runs until the event queue is exhausted before trying to read
 * another message.  If a message is pending,there is no need to enqueue a
 * second one.  This also keeps us from blocking the I/O thread in a msg_send
 * which could result in a deadlock if the consumer were to make a call into
 * the event driver.
 */
void IOHIDSystem::doKickEventConsumer(IOHIDSystem * self)  /*IOInterruptEventSource::Action */
{
    kern_return_t r;
    mach_msg_header_t *msgh;

    self->needToKickEventConsumer = false;   // Request received and processed

        // RY: If the eventPost is null, do nothing
        if ( self->eventPort == MACH_PORT_NULL )
            return;

    xpr_ev_post("doKickEventConsumer\n", 1,2,3,4,5);

    msgh = (mach_msg_header_t *)self->eventMsg;
    if( msgh) {

            r = mach_msg_send_from_kernel( msgh, msgh->msgh_size);
            switch ( r )
            {
                case MACH_SEND_TIMED_OUT:/* Already has a message posted */
                case MACH_MSG_SUCCESS:  /* Message is posted */
                    break;
                default:        /* Log the error */
                    IOLog("%s: doKickEventConsumer msg_send returned %d\n",
                self->registryName, r);
                    break;
            }
    }
}

void IOHIDSystem::scheduleNextPeriodicEvent()
{
    if ( !eventsOpen ) {
        // If eventsOpen is false, then the driver shmem is
        // no longer valid, and it is in the process of shutting down.
        // We should give up without rescheduling.
        IOHID_DEBUG(kIOHIDDebugCode_Scheduling, 0, 0, 0, 0);
    }
    else {
        uint64_t            scheduledEvent = _periodicEventNext;
        uint64_t            now = 0;
        static uint64_t     kOneMS = 0;
        if (!kOneMS) {
            clock_interval_to_absolutetime_interval(1, kMillisecondScale, &kOneMS);
        }
        clock_get_uptime(&now);
        
        if ((scheduledEvent > _periodicEventLast) && (scheduledEvent < (now + kOneMS))) {
            // we have an event scheduled in the next MS. Do not reschedule.
        }
        else {
            if (scheduledEvent <= _periodicEventLast) {
                // scheduledEvent is old. do not reuse.
                scheduledEvent = kIOHIDSystenDistantFuture;
            }
            
            if (screens && (kIOPMDeviceUsable | displayState)) {
                // displays are on and nothing is scheduled.
                // calculate deltas
                uint64_t nextMove = _cursorMoveLast + _cursorMoveDelta;
                uint64_t nextWait = _cursorWaitLast + _cursorWaitDelta;
                
                if (_cursorMoveDelta) {
                    if (_cursorEventLast > _cursorMoveLast) {
                        scheduledEvent = nextMove;
                    }
                }
                
                bool waitCursorShouldAlreadyBeUp =  evg->waitCursorEnabled &&
                                                    evg->globalWaitCursorEnabled &&
                                                    evg->ctxtTimedOut &&
                                                    _cursorWaitDelta;
                if (waitCursorShouldAlreadyBeUp) {
                    if (evg->waitCursorUp) {
                        if (scheduledEvent > nextWait) {
                            // update the wait cursor
                            scheduledEvent = nextWait;
                        }
                    }
                    else {
                        // *show* the wait cursor immediately
                        scheduledEvent = now + kOneMS;
                    }
                    
                }
                else if (evg->waitCursorUp) {
                    // *hide* the wait cursor immediately
                    scheduledEvent = now + kOneMS;
                }
#if 0
                kprintf(" %d%d%d%d %lld : %lld %lld %lld %llu\n",
                        evg->waitCursorEnabled, evg->globalWaitCursorEnabled, evg->ctxtTimedOut, evg->waitCursorUp,
                        now,
                        scheduledEvent, nextMove, nextWait, _cursorWaitDelta);
#endif
                IOHID_DEBUG(kIOHIDDebugCode_Scheduling, scheduledEvent, nextMove, nextWait, 0);
            }
            else {
                // We have something to do, but no one is "listening".
                // Try again in 50 ms.
                scheduledEvent = now + (50 * kOneMS);
                IOHID_DEBUG(kIOHIDDebugCode_Scheduling, scheduledEvent, 0, 0, 0);
            }
            
            if (kIOHIDSystenDistantFuture == scheduledEvent) {
                // no periodic events. cancel any pending periodic timer.
                periodicES->cancelTimeout();
            }
            else {
                _periodicEventNext = scheduledEvent;
                if (now + kOneMS > _periodicEventNext) {
                    // do not schedule the event too soon.
                    _periodicEventNext = now + kOneMS;
                }
                if (now + (100 * kOneMS) < _periodicEventNext) {
                    // after 100 ms, we really should check again. *something* is happening.
                    _periodicEventNext = now + (100 * kOneMS);
                }
                IOReturn err = periodicES->wakeAtTime(_periodicEventNext);
                if (err) {
                    IOLog("%s:%d wakeAtTime failed for %lld: %08x (%s)\n", __func__, __LINE__, _periodicEventNext, err, stringFromReturn(err));
                }
            }
        }
    }
}

// Periodic events are driven from this method.
// After taking care of all pending work, the method
// calls scheduleNextPeriodicEvent to compute and set the
// next callout.

// Modified this method to call a command Gate action.
// This will hopefully insure that this is serialized
// with other actions associated with the command gate.

void IOHIDSystem::_periodicEvents(IOHIDSystem * self,
                                  IOTimerEventSource *timer)
{
    self->periodicEvents(timer);
}

void IOHIDSystem::periodicEvents(IOTimerEventSource * timer __unused)
{
    // If eventsOpen is false, then the driver shmem is
    // no longer valid, and it is in the process of shutting down.
    // We should give up without rescheduling.
    if ( !eventsOpen )
        return;

    clock_get_uptime(&_periodicEventLast);          // update last event
    _periodicEventNext = kIOHIDSystenDistantFuture; // currently no next event

    // Update cursor position if needed
    if (_periodicEventLast >= _cursorMoveLast + _cursorMoveDelta) {
        _cursorHelper.startPosting();
        _cursorHelper.applyPostingDelta();
        _setCursorPosition(false, false, lastSender);
        OSSafeReleaseNULL(lastSender);
        _cursorMoveLast = _periodicEventLast;
    }

    // WAITCURSOR ACTION
    if ( OSSpinLockTry(&evg->waitCursorSema) )
    {
        if ( OSSpinLockTry(&evg->cursorSema) )
        {
            // If wait cursor enabled and context timed out, do waitcursor
            if (evg->waitCursorEnabled && evg->globalWaitCursorEnabled)
            {
                /* WAIT CURSOR SHOULD BE ON */
                if (!evg->waitCursorUp) {
                    showWaitCursor();
                    _cursorWaitLast = _periodicEventLast;
                }
            }
            else {
                /* WAIT CURSOR SHOULD BE OFF */
                if (evg->waitCursorUp) {
                    hideWaitCursor();
                    _cursorWaitLast = _periodicEventLast;
                }
            }
            /* Animate cursor */
            if (evg->waitCursorUp && ((_cursorWaitLast + _cursorWaitDelta) < _periodicEventLast)) {
                animateWaitCursor();
                _cursorWaitLast = _periodicEventLast;
            }
            OSSpinLockUnlock(&evg->cursorSema);
        }
        OSSpinLockUnlock(&evg->waitCursorSema);
    }

    scheduleNextPeriodicEvent();

    return;
}

//
// Start the cursor system running.
//
// At this point, the WindowServer is up, running, and ready to process events.
// We will attach the keyboard and mouse, if none are available yet.
//
bool IOHIDSystem::resetCursor()
{
    volatile IOGPoint * p;
    UInt32 newScreens = 0;
    SInt32 candidate = 0;
    SInt32 pinScreen = -1L;

    p = &evg->cursorLoc;

    /* Get mask of screens on which the cursor is present */
    EvScreen *screen = (EvScreen *)evScreen;
    for (int i = 0; i < screens; i++ ) {
        if (!screen[i].desktopBounds)
            continue;
        if ((screen[i].desktopBounds->maxx - screen[i].desktopBounds->minx) < 128)
            continue;
        candidate = i;
        if (_cursorHelper.desktopLocation().inRect(*screen[i].desktopBounds)) {
            pinScreen = i;
            newScreens |= (1 << i);
        }
    }

    if (newScreens == 0)
        pinScreen = candidate;

    if (!cursorPinned) {
        // reset pin rect
        if (((EvScreen*)evScreen)[pinScreen].desktopBounds) {
            cursorPin = *(((EvScreen*)evScreen)[pinScreen].desktopBounds);
            cursorPin.maxx--;   /* Make half-open rectangle */
            cursorPin.maxy--;
            cursorPinScreen = pinScreen;
        }
        else {
            // How do you pin when there are no screens with bounds...
        }
    }

    if (newScreens == 0) {
        /* Pin new cursor position to cursorPin rect */
        p->x = (p->x < cursorPin.minx) ?
            cursorPin.minx : ((p->x > cursorPin.maxx) ?
            cursorPin.maxx : p->x);
        p->y = (p->y < cursorPin.miny) ?
            cursorPin.miny : ((p->y > cursorPin.maxy) ?
            cursorPin.maxy : p->y);

        /* regenerate mask for new position */
        for (int i = 0; i < screens; i++ ) {
            if (!screen[i].desktopBounds)
                continue;
            if ((screen[i].desktopBounds->maxx - screen[i].desktopBounds->minx) < 128)
                continue;
            if (_cursorHelper.desktopLocation().inRect(*screen[i].desktopBounds)){
                pinScreen = i;
                newScreens |= (1 << i);
            }
        }
    }

    cursorScreens = newScreens;
    IOFixedPoint64 tempLoc;
    if (evg->updateCursorPositionFromFixed) {
        tempLoc.fromFixed24x8(evg->desktopCursorFixed.x, evg->desktopCursorFixed.y);
    }
    else {
        tempLoc.fromIntFloor(evg->cursorLoc.x, evg->cursorLoc.y);
    }
    _cursorHelper.desktopLocationDelta() += tempLoc - _cursorHelper.desktopLocation();
    _cursorHelper.desktopLocation() = tempLoc;
    if (pinScreen >= 0) {
        _cursorHelper.updateScreenLocation(screen[pinScreen].desktopBounds, screen[pinScreen].displayBounds);
    }
    else {
        _cursorHelper.updateScreenLocation(NULL, NULL);
    }

    _cursorMoveDelta = _cursorWaitDelta = 0;
    _cursorHelper.desktopLocationPosting().fromIntFloor(0, 0);
    _cursorHelper.clearEventCounts();

    _cursorLogTimed();

    scheduleNextPeriodicEvent();

    return( true );
}

bool IOHIDSystem::startCursor()
{
    if (0 == screens) {
        // no screens, no cursor
    }
    else {
        cursorPinned = false;
        resetCursor();
        showCursor();

        // Start the cursor control callouts
        scheduleNextPeriodicEvent();

        cursorStarted = true;
    }

    return( cursorStarted );
}

//
// Wait Cursor machinery.  These methods should be run from a command
// gate action and the shared memory area must be set up.
//
void IOHIDSystem::showWaitCursor()
{
    xpr_ev_cursor("showWaitCursor\n",1,2,3,4,5);
    evg->waitCursorUp = true;
    hideCursor();
    evg->frame = EV_WAITCURSOR;
    showCursor();
}

void IOHIDSystem::hideWaitCursor()
{
    xpr_ev_cursor("hideWaitCursor\n",1,2,3,4,5);
    evg->waitCursorUp = false;
    hideCursor();
    evg->frame = EV_STD_CURSOR;
    showCursor();
}

void IOHIDSystem::animateWaitCursor()
{
    xpr_ev_cursor("animateWaitCursor\n",1,2,3,4,5);
    changeCursor(evg->frame + 1);
}

void IOHIDSystem::changeCursor(int frame)
{
    evg->frame = ((frame > (int)maxWaitCursorFrame) || (frame > evg->lastFrame)) ? firstWaitCursorFrame : frame;
    xpr_ev_cursor("changeCursor %d\n",evg->frame,2,3,4,5);
    moveCursor();
}

//
// Return the screen number in which point p lies.  Return -1 if the point
// lies outside of all registered screens.
//
int IOHIDSystem::pointToScreen(IOGPoint * p)
{
    int i;
    EvScreen *screen = (EvScreen *)evScreen;
    for (i=screens; --i != -1; ) {
        if ((screen[i].desktopBounds != 0)
            && (p->x >= screen[i].desktopBounds->minx)
            && (p->x < screen[i].desktopBounds->maxx)
            && (p->y >= screen[i].desktopBounds->miny)
            && (p->y < screen[i].desktopBounds->maxy))
        return i;
    }
    return(-1); /* Cursor outside of known screen boundary */
}

//
// API used to drive event state out to attached screens
//
// These should be run from a command gate action.
//
inline void IOHIDSystem::showCursor()
{
    evDispatch(/* command */ EVSHOW);
}
inline void IOHIDSystem::hideCursor()
{
    evDispatch(/* command */ EVHIDE);
}

inline void IOHIDSystem::moveCursor()
{
    evDispatch(/* command */ EVMOVE);
}

//
// - attachDefaultEventSources
//  Attach the default event sources.
//
void IOHIDSystem::attachDefaultEventSources()
{
    IOService  *     source;
    OSIterator *    sources;


        sources = getProviderIterator();

        if (!sources)  return;

    while( (source = (IOService *)sources->getNextObject())) {
        if ((OSDynamicCast(IOHIDevice, source) && !OSDynamicCast(IOHIKeyboard, source))
            || OSDynamicCast(IOHIDEventService, source)) {

            registerEventSource(source);
       }
    }
        sources->release();
}

//
// - detachEventSources
//  Detach all event sources
//
void IOHIDSystem::detachEventSources()
{
    OSIterator * iter;
    IOService * srcInstance;

    iter = getOpenProviderIterator();
    if( iter) {
        while( (srcInstance = (IOService *) iter->getNextObject())) {

            if ( ! OSDynamicCast(IOHIKeyboard, srcInstance) ) {
                #ifdef DEBUG
                kprintf("detachEventSource:%s\n", srcInstance->getName());
                #endif
                srcInstance->close(this);
            }
        }
        iter->release();
    }
}

//
// EventSrcClient implementation
//

//
// A new device instance desires to be added to our list.
// Try to get ownership of the device. If we get it, add it to
// the list.
//
bool IOHIDSystem::registerEventSource(IOService * source)
{
    bool success = false;

#ifdef DEBUG
    kprintf("registerEventSource:%s\n", source->getName());
#endif

    if ( OSDynamicCast(IOHIKeyboard, source) ) {
        success = ((IOHIKeyboard*)source)->open(this, kIOServiceSeize,0,
                    (KeyboardEventCallback)        _keyboardEvent,
                    (KeyboardSpecialEventCallback) _keyboardSpecialEvent,
                    (UpdateEventFlagsCallback)     _updateEventFlags);
        source->setProperty(kIOHIDResetKeyboardKey, kOSBooleanTrue);
    } else if ( OSDynamicCast(IOHIPointing, source) ) {
        if ( OSDynamicCast(IOHITablet, source) ) {
            success = ((IOHITablet*)source)->open(this, kIOServiceSeize,0,
                        (RelativePointerEventCallback)  _relativePointerEvent,
                        (AbsolutePointerEventCallback)  _absolutePointerEvent,
                        (ScrollWheelEventCallback)      _scrollWheelEvent,
                        (TabletEventCallback)           _tabletEvent,
                        (ProximityEventCallback)        _proximityEvent);
        } else {
            success = ((IOHIPointing*)source)->open(this, kIOServiceSeize,0,
                        (RelativePointerEventCallback) _relativePointerEvent,
                        (AbsolutePointerEventCallback) _absolutePointerEvent,
                        (ScrollWheelEventCallback)     _scrollWheelEvent);
        }
        source->setProperty(kIOHIDResetPointerKey, kOSBooleanTrue);
    } else {
        success = source->open(this, kIOServiceSeize, 0);
    }

    if ( success )
    {

        OSDictionary * newParams = OSDictionary::withDictionary( savedParameters );
        if( newParams) {

            // update with user settings
            if ( OSDynamicCast(IOHIDevice, source) )
                ((IOHIDevice *)source)->setParamProperties( newParams );
            else if ( OSDynamicCast(IOHIDEventService, source) )
                ((IOHIDEventService *)source)->setSystemProperties( newParams );

            setProperty( kIOHIDParametersKey, newParams );
            newParams->release();
            savedParameters = newParams;
        }

    }
    else
        IOLog("%s: Seize of %s failed.\n", registryName, source->getName());

    return success;
}

IOReturn IOHIDSystem::message(UInt32 type, IOService * provider,
                void * argument)
{
    IOReturn     status = kIOReturnSuccess;

    switch (type)
    {
        case kIOMessageServiceIsTerminated:
            if (provider) {
                provider->close( this );
            }
            break;
            
        case kIOMessageServiceWasClosed:
            break;

        case kIOHIDSystemActivityTickle: {
            intptr_t nxEvent = (intptr_t) argument;
            if ((nxEvent >= 0) && (nxEvent <= NX_LASTEVENT)) {
                if (!evStateChanging && displayManager) {
                    IOHID_DEBUG(kIOHIDDebugCode_DisplayTickle, nxEvent, __LINE__, displayState,
                                provider ? provider->getRegistryEntryID() : 0);
                    if (DISPLAY_IS_ENABLED || (NX_WAKEMASK & EventCodeMask(nxEvent))) {
                        if (!DISPLAY_IS_ENABLED) {
                            kprintf("IOHIDSystem tickle when screen off for event %ld\n", nxEvent);
                        }
                        displayManager->activityTickle(IOHID_DEBUG_CODE(nxEvent));
                    }
                }
            }
            else if (nxEvent == NX_HARDWARE_TICKLE) {
                if (!evStateChanging && displayManager) {
                    IOHID_DEBUG(kIOHIDDebugCode_DisplayTickle, nxEvent, __LINE__, displayState,
                                provider ? provider->getRegistryEntryID() : 0);
                    if (!DISPLAY_IS_ENABLED) {
                        kprintf("IOHIDSystem tickle when screen off for hardware event from %08llx\n",
                                provider ? provider->getRegistryEntryID() : 0);
                    }
                    displayManager->activityTickle(IOHID_DEBUG_CODE(nxEvent));
                }
            }
            else {
                IOLog("kIOHIDSystemActivityTickle message for unsupported event %ld sent from %08llx\n",
                      nxEvent, provider ? provider->getRegistryEntryID() : 0);
            }
            break;
        }

        default:
            status = super::message(type, provider, argument);
            break;
    }

    return status;
}

//
// This will scale the point at location in the coordinate system represented by bounds
// to the coordinate system of the current screen.
// This is needed for absolute pointer events that come from devices with different bounds.
//
void IOHIDSystem::scaleLocationToCurrentScreen(IOGPoint *location, IOGBounds *bounds __unused)
{
    IOHIDSystem * hidsystem = instance();

    if ( hidsystem ) {
        IOFixedPoint64 temp;
        temp.fromIntFloor(location->x, location->y);
        hidsystem->_scaleLocationToCurrentScreen(temp, bounds);
        *location = (IOGPoint)temp;
    }
}

void IOHIDSystem::_scaleLocationToCurrentScreen(IOFixedPoint64 &location, IOGBounds *bounds)
{
    if (!bounds) {
        // no transform can be performed
    }
    else {
        if (*(UInt64*)&cursorPin == *(UInt64*)bounds) {
            //  no transform needed
        }
        else {
            int boundsWidth   = bounds->maxx  - bounds->minx;
            int boundsHeight  = bounds->maxy  - bounds->miny;
            int cursorPinWidth  = cursorPin.maxx - cursorPin.minx;
            int cursorPinHeight = cursorPin.maxy - cursorPin.miny;
            if ((boundsWidth <= 0) || (boundsHeight <= 0) || (cursorPinWidth <= 0) || (cursorPinHeight <= 0)) {
                // no transform can be performed
            }
            else {
                IOFixedPoint64 scratch;
                if ((boundsWidth == cursorPinWidth) && (boundsHeight == cursorPinHeight)) {
                    // translation only
                    location += scratch.fromIntFloor(bounds->minx - cursorPin.minx,
                                                     bounds->miny - cursorPin.miny);
                }
                else {
                    // full transform
                    IOFixed64 x_scale;
                    IOFixed64 y_scale;
                    x_scale.fromIntFloor(cursorPinWidth) /= boundsWidth;
                    y_scale.fromIntFloor(cursorPinHeight) /= boundsHeight;
                    location -= scratch.fromIntFloor(bounds->minx, bounds->miny);
                    location *= scratch.fromFixed64(x_scale, y_scale);
                    location += scratch.fromIntFloor(cursorPin.minx, cursorPin.miny);
                }
            }
        }
    }
    return;
}

//
// Process a mouse status change.  The driver should sign extend
// it's deltas and perform any bit flipping needed there.
//
// We take the state as presented and turn it into events.
//
void IOHIDSystem::_relativePointerEvent(IOHIDSystem * self,
                    int        buttons,
                       /* deltaX */ int        dx,
                       /* deltaY */ int        dy,
                       /* atTime */ AbsoluteTime ts,
                                    OSObject * sender,
                                    void *     refcon __unused)
{
    self->relativePointerEvent(buttons, dx, dy, ts, sender);
}

void IOHIDSystem::relativePointerEvent(int        buttons,
                          /* deltaX */ int        dx,
                          /* deltaY */ int        dy,
                          /* atTime */ AbsoluteTime ts)
{
    relativePointerEvent(buttons, dx, dy, ts, 0);
}

void IOHIDSystem::relativePointerEvent(int        buttons,
                          /* deltaX */   int        dx,
                          /* deltaY */   int        dy,
                          /* atTime */   AbsoluteTime ts,
                          /* sender */   OSObject * sender)
{
    IOHIDCmdGateActionArgs args;
    args.arg0 = &buttons;
    args.arg1 = &dx;
    args.arg2 = &dy;
    args.arg3 = &ts;
    args.arg4 = sender;

    cmdGate->runAction((IOCommandGate::Action)doRelativePointerEvent, &args);
}


IOReturn IOHIDSystem::doRelativePointerEvent(IOHIDSystem *self, void * args)
                        /* IOCommandGate::Action */
{
    int             buttons = *(int *)((IOHIDCmdGateActionArgs *)args)->arg0;
    int             dx  = *(int *)((IOHIDCmdGateActionArgs *)args)->arg1;
    int             dy  = *(int *)((IOHIDCmdGateActionArgs *)args)->arg2;
    SInt64          ts  = *(SInt64 *)((IOHIDCmdGateActionArgs *)args)->arg3;
    OSObject *          sender  = (OSObject *)((IOHIDCmdGateActionArgs *)args)->arg4;

    self->relativePointerEventGated(buttons, dx, dy, ts, sender);

    return kIOReturnSuccess;
}

//************************************************************
static bool vblForScreen(IOGraphicsDevice *io_gd_I, uint64_t &delta_O)
{
    uint64_t        nextVBL = 0;
    static UInt64   minVBLdelta = 0;
    static UInt64   maxVBLdelta = 0;

    // rdar://5565815 Capping VBL interval
    if (!minVBLdelta) {
        // A screen refresh of more than 50ms (20Hz) is not acceptable.
        nanoseconds_to_absolutetime(50000000, (&maxVBLdelta));
        // A screen refresh of less than 5ms (200Hz) is not necessary.
        nanoseconds_to_absolutetime(5000000, (&minVBLdelta));
    }

    if (io_gd_I) {
        io_gd_I->getVBLTime( &nextVBL, &delta_O );
        if (delta_O < minVBLdelta) {
            delta_O = minVBLdelta;
        }
        else if (delta_O > maxVBLdelta) {
            delta_O = maxVBLdelta;
        }
    }
    else {
        delta_O = maxVBLdelta;
    }

    return (nextVBL != 0);
}

void IOHIDSystem::relativePointerEventGated(int buttons, int dx_I, int dy_I, SInt64 ts, OSObject * sender)
{
    bool movementEvent = false;

    PROFILE_TRACE(1);

    if( eventsOpen == false )
        return;

    // Compare relative mouse deltas against thresholds to determine if the
    // movement should generate a tickle. This will prevent chatty mice from
    // tickling without user input.
    CachedMouseEventStruct *cachedMouseEvent = GetCachedMouseEventForService(cachedButtonStates, sender);
    if (cachedMouseEvent) {
        UInt64 ts_nano;
        absolutetime_to_nanoseconds(ts, &ts_nano);

        if (ts_nano > cachedMouseEvent->eventDeadline) {
            cachedMouseEvent->eventDeadline = ts_nano + kIOHIDChattyMouseSuppressionDelayNS;
            cachedMouseEvent->accumX = 0;
            cachedMouseEvent->accumY = 0;
        }

        cachedMouseEvent->accumX += dx_I;
        cachedMouseEvent->accumY += dy_I;

        if ((abs(cachedMouseEvent->accumX) >= kIOHIDRelativeTickleThresholdPixel) ||
            (abs(cachedMouseEvent->accumY) >= kIOHIDRelativeTickleThresholdPixel))
        {
            movementEvent = true;
        }

        cachedMouseEvent->lastButtons = buttons;
        cachedMouseEvent->eventDeadline = ts_nano;

        // Fake up pressure changes from button state changes
        if( (buttons & EV_LB) != (evg->buttons & EV_LB) ){
            cachedMouseEvent->lastPressure = ( buttons & EV_LB ) ? MAXPRESSURE : MINPRESSURE;
        }
    }

    // do not tickle on movement if the screen is dark (some restrictions apply)
    if      (buttons & EV_LB) {
        // always tickle on left button down
        TICKLE_DISPLAY(NX_LMOUSEDOWN);
    }
    else if (buttons & EV_RB) {
        // always tickle on right button down
        TICKLE_DISPLAY(NX_RMOUSEDOWN);
    }
    else if (buttons) {
        // always tickle on other buttons down
        TICKLE_DISPLAY(NX_OMOUSEDOWN);
    }
    else if (movementEvent) {
        if (DISPLAY_IS_ENABLED) {
            // the display is on. send a moved tickle.
            TICKLE_DISPLAY(NX_MOUSEMOVED);
        }
        else
        {
            // the display is off...
            if (CMP_ABSOLUTETIME(&ts, &displaySleepWakeupDeadline) <= 0) {
                // but not too much time has passed. send a moved tickle.
                TICKLE_DISPLAY(NX_MOUSEMOVED);
            }
            return;
        }
    }

    if (ShouldConsumeHIDEvent(ts, rootDomainStateChangeDeadline)) {
        return;
    }

    _setButtonState(buttons, /* atTime */ ts, sender);

    _cursorHelper.incrementEventCount();
    IOFixedPoint64 scratch;
    if ( scratch.fromIntFloor(dx_I, dy_I) ) {
        UInt64              uptime = 0;
        bool                haveVBL = vblForScreen(((EvScreen*)evScreen)[cursorPinScreen].instance, _cursorMoveDelta);

        clock_get_uptime(&uptime);

        if (((scratch.xValue() > 0LL) && (_cursorHelper.desktopLocationAccumulated().xValue() < 0LL)) ||
            ((scratch.xValue() < 0LL) && (_cursorHelper.desktopLocationAccumulated().xValue() > 0LL))) {
            _cursorHelper.desktopLocationAccumulated().xValue().fromIntFloor(0);
        }
        if (((scratch.yValue() > 0LL) && (_cursorHelper.desktopLocationAccumulated().yValue() < 0LL)) ||
            ((scratch.yValue() < 0LL) && (_cursorHelper.desktopLocationAccumulated().yValue() > 0LL))) {
            _cursorHelper.desktopLocationAccumulated().yValue().fromIntFloor(0);
        }

        IOHID_DEBUG(kIOHIDDebugCode_RelativePointerEventTiming, _cursorMoveDelta, 0,
                    dx_I, dy_I);

        _cursorHelper.desktopLocationAccumulated() += scratch;

        scratch = _cursorHelper.desktopLocationAccumulated(); // for ease of reference
        _cursorEventLast = uptime;

        if (!haveVBL) {
            // no VBL
            // post immediatly
            periodicEvents(NULL);
        }
        else {
            // see rdar://7675662
            if (cachedMouseEvent && (cachedMouseEvent->reportInterval_ns > 0)) {
                uint64_t cursorMoveDelta_ns;
                absolutetime_to_nanoseconds(_cursorMoveDelta, &cursorMoveDelta_ns);
                _cursorHelper.expectedCount().fromIntFloor(cursorMoveDelta_ns);
                _cursorHelper.expectedCount() /= cachedMouseEvent->reportInterval_ns;
                IOHID_DEBUG(kIOHIDDebugCode_RelativePointerEventScaling,
                            scratch.xValue().asFixed(), scratch.yValue().asFixed(),
                            _cursorHelper.expectedCount().asFixed(),
                            _cursorHelper.getEventCountPosting() * 65536);
            }
            else {
                _cursorHelper.expectedCount().fromIntFloor(0);
            }

            if (sender)
                sender->retain();
            OSSafeReleaseNULL(lastSender);
            lastSender = sender;

            if (uptime > _cursorMoveLast + 2 * _cursorMoveDelta) {
                // either we are not posting currently or this event is overdue.
                // post immediately
                periodicEvents(periodicES); // handles scheduling
            }
            else {
                scheduleNextPeriodicEvent();
            }
        }
        _cursorLog(AbsoluteTime_to_scalar(&ts));
    }
    PROFILE_TRACE(2);
}

void IOHIDSystem::_absolutePointerEvent(
                                IOHIDSystem *   self,
                                int             buttons,
            /* at */            IOGPoint *      newLoc,
            /* withBounds */    IOGBounds *     bounds,
            /* inProximity */   bool            proximity,
            /* withPressure */  int             pressure,
            /* withAngle */     int             stylusAngle,
            /* atTime */        AbsoluteTime    ts,
                                OSObject *      sender,
                                void *          refcon __unused)
{
    self->absolutePointerEvent(buttons, newLoc, bounds, proximity,
                    pressure, stylusAngle, ts, sender);
}

void IOHIDSystem::absolutePointerEvent(
                                int             buttons,
            /* at */            IOGPoint *      newLoc,
            /* withBounds */    IOGBounds *     bounds,
            /* inProximity */   bool            proximity,
            /* withPressure */  int             pressure,
            /* withAngle */     int             stylusAngle,
            /* atTime */        AbsoluteTime    ts)
{
    absolutePointerEvent(buttons, newLoc, bounds, proximity,
                    pressure, stylusAngle, ts, 0);
}

void IOHIDSystem::absolutePointerEvent(
                                int             buttons,
            /* at */            IOGPoint *      newLoc,
            /* withBounds */    IOGBounds *     bounds,
            /* inProximity */   bool            proximity,
            /* withPressure */  int             pressure,
            /* withAngle */     int             stylusAngle,
            /* atTime */        AbsoluteTime    ts,
            /* sender */        OSObject *      sender)
{
    IOHIDCmdGateActionArgs args;

    args.arg0 = &buttons;
    args.arg1 = (void *)newLoc;
    args.arg2 = (void *)bounds;
    args.arg3 = &proximity;
    args.arg4 = &pressure;
    args.arg5 = &stylusAngle;
    args.arg6 = &ts;
    args.arg7 = sender;

    cmdGate->runAction((IOCommandGate::Action)doAbsolutePointerEvent, &args);
}

IOReturn IOHIDSystem::doAbsolutePointerEvent(IOHIDSystem *self, void * args)
                        /* IOCommandGate::Action */
{
    int         buttons     = *(int *)  ((IOHIDCmdGateActionArgs *)args)->arg0;
    IOGPoint *  newLoc      = (IOGPoint *)  ((IOHIDCmdGateActionArgs *)args)->arg1;
    IOGBounds * bounds      = (IOGBounds *) ((IOHIDCmdGateActionArgs *)args)->arg2;
    bool        proximity   = *(bool *) ((IOHIDCmdGateActionArgs *)args)->arg3;
    int         pressure    = *(int *)  ((IOHIDCmdGateActionArgs *)args)->arg4;
    int         stylusAngle = *(int *)  ((IOHIDCmdGateActionArgs *)args)->arg5;
    AbsoluteTime    ts  = *(AbsoluteTime *)     ((IOHIDCmdGateActionArgs *)args)->arg6;
    OSObject *  sender          = (OSObject *)((IOHIDCmdGateActionArgs *)args)->arg7;


    self->absolutePointerEventGated(buttons, newLoc, bounds, proximity, pressure, stylusAngle, ts, sender);

    return kIOReturnSuccess;
}

void IOHIDSystem::absolutePointerEventGated(
                                int             buttons,
            /* at */            IOGPoint *      newLocGPoint,
            /* withBounds */    IOGBounds *     bounds __unused,
            /* inProximity */   bool            proximity,
            /* withPressure */  int             pressure,
            /* withAngle */     int             stylusAngle __unused,
            /* atTime */        AbsoluteTime    ts,
            /* sender */        OSObject *      sender)
{

    /*
     * If you don't know what to pass for the following fields, pass the
     * default values below:
     *    pressure    = MINPRESSURE or MAXPRESSURE
     *    stylusAngle = 90
     */
    NXEventData     outData;    /* dummy data */
    bool            proximityChange = false;

    PROFILE_TRACE(5);

    if ( !eventsOpen )
        return;

    if (ShouldConsumeHIDEvent(ts, rootDomainStateChangeDeadline)) {
        TICKLE_DISPLAY(NX_MOUSEMOVED);
        return;
    }

    if (!DISPLAY_IS_ENABLED) {
#if !WAKE_DISPLAY_ON_MOVEMENT
        if ( CMP_ABSOLUTETIME(&ts, &displaySleepWakeupDeadline) <= 0 )
        {
            TICKLE_DISPLAY(NX_MOUSEMOVED);
            return;
        }

        if (buttons)
#endif
        {
            TICKLE_DISPLAY(NX_LMOUSEDOWN);
        }
        return;
    }

    TICKLE_DISPLAY(NX_MOUSEMOVED);

    IOFixedPoint64 newLoc;
    newLoc.fromIntFloor(newLocGPoint->x, newLocGPoint->y);
    _scaleLocationToCurrentScreen(newLoc, bounds);

    // RY: Attempt to add basic tablet support to absolute pointing devices
    // Basically, we will fill in the tablet support portions of both the
    // mouse and mouseMove of NXEventData.  Pending tablet events are stored
    // in the CachedMouseEventStruct and then later picked off in
    // _setButtonState and _postMouseMoveEvent
    CachedMouseEventStruct  *cachedMouseEvent = GetCachedMouseEventForService(cachedButtonStates, sender);
    if (cachedMouseEvent)
    {
        cachedMouseEvent->pointerFractionX = newLoc.xValue().fraction();
        cachedMouseEvent->pointerFractionY = newLoc.yValue().fraction();

        proximityChange = (cachedMouseEvent->proximity != proximity);

        cachedMouseEvent->state        |= kCachedMousePointingEventDispFlag;
        cachedMouseEvent->proximity     = proximity;
        cachedMouseEvent->lastPressure  = ScalePressure(pressure);

        if ( !(cachedMouseEvent->state & kCachedMouseTabletEventDispFlag) )
        {
            // initialize the proximity and tablet event structs
            if ( !(cachedMouseEvent->state & kCachedMousePointingTabletEventDispFlag) )
            {
                cachedMouseEvent->state |= kCachedMousePointingTabletEventDispFlag;
                cachedMouseEvent->proximityData.proximity.capabilityMask  = (
                                                                             NX_TABLET_CAPABILITY_DEVICEIDMASK  |
                                                                             NX_TABLET_CAPABILITY_ABSXMASK |
                                                                             NX_TABLET_CAPABILITY_ABSYMASK |
                                                                             NX_TABLET_CAPABILITY_BUTTONSMASK |
                                                                             NX_TABLET_CAPABILITY_PRESSUREMASK);
                cachedMouseEvent->proximityData.proximity.pointerType       = NX_TABLET_POINTER_PEN;
                cachedMouseEvent->proximityData.proximity.systemTabletID    = IOHITablet::generateTabletID();
                cachedMouseEvent->proximityData.proximity.deviceID          =
                cachedMouseEvent->tabletData.tablet.deviceID                = IOHIDPointing::generateDeviceID();
            }

            if ( proximityChange )
            {
                cachedMouseEvent->proximityData.proximity.enterProximity    = proximity;
                cachedMouseEvent->subType                                   = NX_SUBTYPE_TABLET_PROXIMITY;

                cachedMouseEvent->state |= kCachedMousePointingTabletEventPendFlag;
                proximityEventGated(&(cachedMouseEvent->proximityData), ts, sender);
                cachedMouseEvent->state &= ~kCachedMousePointingTabletEventPendFlag;
            }
            else if ( proximity )
            {
                // RY: revert the button state
                // The window server requires that the lower 3 bits of
                // the buttons bit field be mangled for interpretation
                // when handling a button event.  Unfortunately,
                // applications that make use of the tablet events
                // require that the buttons field not be mangled.  Thus
                // the button state should be reverted.
                cachedMouseEvent->tabletData.tablet.buttons     = buttons & ~0x7;
                if (buttons & 2)
                    cachedMouseEvent->tabletData.tablet.buttons |= 4;
                if (buttons & EV_RB)
                    cachedMouseEvent->tabletData.tablet.buttons |= 2;
                if (buttons & EV_LB)
                    cachedMouseEvent->tabletData.tablet.buttons |= 1;

                cachedMouseEvent->tabletData.tablet.x           = newLoc.xValue().as32();
                cachedMouseEvent->tabletData.tablet.y           = newLoc.yValue().as32();
                cachedMouseEvent->tabletData.tablet.pressure    = pressure;
                cachedMouseEvent->subType                       = NX_SUBTYPE_TABLET_POINT;
            }
        }
    }

    clock_get_uptime(&_cursorEventLast);
    if ( (newLoc != _cursorHelper.desktopLocation()) || proximityChange)
    {
        _cursorHelper.desktopLocationDelta() = newLoc - _cursorHelper.desktopLocation();
        _cursorHelper.desktopLocation() = newLoc;
        _cursorLog(_cursorEventLast);

        _setCursorPosition(false, proximityChange, sender);
        vblForScreen(((EvScreen*)evScreen)[cursorPinScreen].instance, _cursorMoveDelta);
        _cursorMoveLast = _cursorEventLast;
    }

    if ( proximityChange && proximity == true )
    {
        evg->eventFlags |= NX_STYLUSPROXIMITYMASK;
        bzero( (char *)&outData, sizeof outData );
        postEvent(         NX_FLAGSCHANGED,
                  /* at */        &_cursorHelper.desktopLocation(),
                  /* atTime */   ts,
                  /* withData */ &outData,
                  /* sender */   sender);
    }
    if ( proximityChange || proximity == true )
        _setButtonState(buttons, /* atTime */ ts, sender);
    if ( proximityChange && proximity == false )
    {
        evg->eventFlags &= ~NX_STYLUSPROXIMITYMASK;
        bzero( (char *)&outData, sizeof outData );
        postEvent(         NX_FLAGSCHANGED,
                  /* at */       &_cursorHelper.desktopLocation(),
                  /* atTime */   ts,
                  /* withData */ &outData,
                  /* sender */   sender);
    }

    // RY: Clean it off
    if (cachedMouseEvent)
    {
        cachedMouseEvent->subType = NX_SUBTYPE_DEFAULT;
        cachedMouseEvent->pointerFractionX = cachedMouseEvent->pointerFractionY = 0;
    }

    scheduleNextPeriodicEvent();
    PROFILE_TRACE(6);
}

void IOHIDSystem::_scrollWheelEvent(IOHIDSystem * self,
                                    short   deltaAxis1,
                                    short   deltaAxis2,
                                    short   deltaAxis3,
                                    IOFixed fixedDelta1,
                                    IOFixed fixedDelta2,
                                    IOFixed fixedDelta3,
                                    SInt32  pointDeltaAxis1,
                                    SInt32  pointDeltaAxis2,
                                    SInt32  pointDeltaAxis3,
                                    UInt32  options,
                 /* atTime */       AbsoluteTime ts,
                                    OSObject * sender,
                                    void *     refcon __unused)
{
        self->scrollWheelEvent(deltaAxis1, deltaAxis2, deltaAxis3, fixedDelta1, fixedDelta2, fixedDelta3,  pointDeltaAxis1, pointDeltaAxis2, pointDeltaAxis3, options, ts, sender);
}

void IOHIDSystem::scrollWheelEvent(short    deltaAxis1,
                                   short    deltaAxis2,
                                   short    deltaAxis3,
                    /* atTime */   AbsoluteTime ts)

{
    scrollWheelEvent(deltaAxis1, deltaAxis2, deltaAxis3, deltaAxis1<<16, deltaAxis2<<16, deltaAxis3<<16, 0, 0, 0, 0, ts, 0);
}

void IOHIDSystem::scrollWheelEvent(short    deltaAxis1,
                                   short    deltaAxis2,
                                   short    deltaAxis3,
                                   IOFixed  fixedDelta1,
                                   IOFixed  fixedDelta2,
                                   IOFixed  fixedDelta3,
                                   SInt32   pointDeltaAxis1,
                                   SInt32   pointDeltaAxis2,
                                   SInt32   pointDeltaAxis3,
                                   UInt32   options,
                    /* atTime */   AbsoluteTime ts,
                                    OSObject *  sender)

{
    IOHIDCmdGateActionArgs args;

    args.arg0 = &deltaAxis1;
    args.arg1 = &deltaAxis2;
    args.arg2 = &deltaAxis3;
    args.arg3 = &fixedDelta1;
    args.arg4 = &fixedDelta2;
    args.arg5 = &fixedDelta3;
    args.arg6 = &pointDeltaAxis1;
    args.arg7 = &pointDeltaAxis2;
    args.arg8 = &pointDeltaAxis3;
    args.arg9 = &options;
    args.arg10 = &ts;
    args.arg11 = sender;

    cmdGate->runAction((IOCommandGate::Action)doScrollWheelEvent, (void *)&args);
}

IOReturn IOHIDSystem::doScrollWheelEvent(IOHIDSystem *self, void * args)
                        /* IOCommandGate::Action */
{
    short   deltaAxis1      = *(short *)((IOHIDCmdGateActionArgs *)args)->arg0;
    short   deltaAxis2      = *(short *)((IOHIDCmdGateActionArgs *)args)->arg1;
    short   deltaAxis3      = *(short *)((IOHIDCmdGateActionArgs *)args)->arg2;
    IOFixed fixedDelta1     = *(IOFixed *)((IOHIDCmdGateActionArgs *)args)->arg3;
    IOFixed fixedDelta2     = *(IOFixed *)((IOHIDCmdGateActionArgs *)args)->arg4;
    IOFixed fixedDelta3     = *(IOFixed *)((IOHIDCmdGateActionArgs *)args)->arg5;
    SInt32  pointDeltaAxis1 = *(IOFixed *)((IOHIDCmdGateActionArgs *)args)->arg6;
    SInt32  pointDeltaAxis2 = *(IOFixed *)((IOHIDCmdGateActionArgs *)args)->arg7;
    SInt32  pointDeltaAxis3 = *(IOFixed *)((IOHIDCmdGateActionArgs *)args)->arg8;
    UInt32  options        = *(UInt32 *)((IOHIDCmdGateActionArgs *)args)->arg9;
    AbsoluteTime ts  = *(AbsoluteTime *)((IOHIDCmdGateActionArgs *)args)->arg10;
    OSObject * sender= (OSObject *)((IOHIDCmdGateActionArgs *)args)->arg11;

    self->scrollWheelEventGated(deltaAxis1, deltaAxis2, deltaAxis3, fixedDelta1, fixedDelta2, fixedDelta3, pointDeltaAxis1, pointDeltaAxis2, pointDeltaAxis3, options, ts, sender);

    return kIOReturnSuccess;
}

#if 0
#   define log_event_phase(s, ...)     kprintf("%s:%d " s, __FILE__, __LINE__, __VA_ARGS__)
#else
#   define log_event_phase(s, ...)
#endif

void IOHIDSystem::scrollWheelEventGated(short   deltaAxis1,
                                        short   deltaAxis2,
                                        short   deltaAxis3,
                                       IOFixed  fixedDelta1,
                                       IOFixed  fixedDelta2,
                                       IOFixed  fixedDelta3,
                                       SInt32   pointDeltaAxis1,
                                       SInt32   pointDeltaAxis2,
                                       SInt32   pointDeltaAxis3,
                                       UInt32   options,
                        /* atTime */    AbsoluteTime ts,
                                        OSObject * sender)
{
    NXEventData wheelData;
    bool        moved = (deltaAxis1 || pointDeltaAxis1 ||
                         deltaAxis2 || pointDeltaAxis2 ||
                         deltaAxis3 || pointDeltaAxis3);
    UInt32      momentum = (options & kScrollTypeMomentumAny);

    if (!eventsOpen)
        return;

    if (ShouldConsumeHIDEvent(ts, rootDomainStateChangeDeadline)) {
        return;
    }

#if !WAKE_DISPLAY_ON_MOVEMENT
    if (!DISPLAY_IS_ENABLED) {
        if (CMP_ABSOLUTETIME(&ts, &displaySleepWakeupDeadline) <= 0)
        {
           TICKLE_DISPLAY(NX_SCROLLWHEELMOVED);
        }
        return;
    }
#endif

    /***********************************************************************************************
    BEGIN PHASE SUPPRESSION STATE MACHINE
        Quiet: (start)
            on May Begin:
                post May Begin
                goto Suspended
            on Began:
                if moved
                    post Begin
                    goto Begun
                else
                    post May Begin
                    goto Suspended
            on Continue:
            on Ended:
            on Canceled:
                log and post?

        Suspended:
            on Began:
            on Continue:
                if moved:
                    post Begin
                    goto Begun
            on Ended:
            on Canceled:
                post Canceled
                goto Quiet
            on May Begin:
                log and post?


        Begun:
            on Ended
            on Canceled:
                post Ended or Cancelled
                goto Quiet
            on Continue:
                post Continue
            on Began:
            on May Begin:
                log and post?

     */
    UInt32      phase = (options & kScrollTypeOptionPhaseAny);
    UInt32      oldDelayedPhase = 0;
    UInt32      newDelayedPhase = 0;
    if (_devicePhaseState && _devicePhaseState->getObject((OSSymbol*)sender)) {
        newDelayedPhase = oldDelayedPhase = ((OSNumber*)_devicePhaseState->getObject((OSSymbol*)sender))->unsigned32BitValue();
    }
    // phaseAnnotation = |32 supplied phase |24 original state |16 new state |8 empty |
    UInt32      phaseAnnotation = (phase << 16) | (oldDelayedPhase << 8);

    switch (oldDelayedPhase) {
        default: // quiet
            switch (phase) {
                case kScrollTypeOptionPhaseMayBegin:
                    newDelayedPhase = kScrollTypeOptionPhaseMayBegin;
                    break;
                case kScrollTypeOptionPhaseBegan:
                    if (moved) {
                        newDelayedPhase = kScrollTypeOptionPhaseBegan;
                    }
                    else {
                        newDelayedPhase = kScrollTypeOptionPhaseMayBegin;
                        options &= ~kScrollTypeOptionPhaseAny;
                        options |= kScrollTypeOptionPhaseMayBegin;
                        phase = kScrollTypeOptionPhaseMayBegin;
                    }
                    break;
                case kScrollTypeOptionPhaseChanged:
                case kScrollTypeOptionPhaseEnded:
                case kScrollTypeOptionPhaseCanceled:
                    log_event_phase("unexpected phase (%04x) state (%04x) combination\n", phase, oldDelayedPhase);
                    break;
            }
            break;

        case kScrollTypeOptionPhaseMayBegin: // suspended
            switch (phase) {
                case kScrollTypeOptionPhaseMayBegin:
                case kScrollTypeOptionPhaseBegan:
                case kScrollTypeOptionPhaseChanged:
                    if (moved) {
                        newDelayedPhase = kScrollTypeOptionPhaseBegan;
                        options &= ~kScrollTypeOptionPhaseAny;
                        options |= kScrollTypeOptionPhaseBegan;
                        phase = kScrollTypeOptionPhaseBegan;
                    }
                    else {
                        options &= ~kScrollTypeOptionPhaseAny;
                        phase = 0;
                    }
                    break;
                case kScrollTypeOptionPhaseEnded:
                case kScrollTypeOptionPhaseCanceled:
                    newDelayedPhase = 0;
                    options &= ~kScrollTypeOptionPhaseAny;
                    options |= kScrollTypeOptionPhaseCanceled;
                    phase = kScrollTypeOptionPhaseCanceled;
                    break;
            }
            break;

        case kScrollTypeOptionPhaseBegan: // Begun
            switch (phase) {
                case kScrollTypeOptionPhaseMayBegin:
                case kScrollTypeOptionPhaseBegan:
                    log_event_phase("unexpected phase (%04x) state (%04x) combination\n", phase, oldDelayedPhase);
                    break;
                case kScrollTypeOptionPhaseChanged:
                    break;
                case kScrollTypeOptionPhaseEnded:
                case kScrollTypeOptionPhaseCanceled:
                    newDelayedPhase = 0;
                    break;
            }
            break;
    }
    phaseAnnotation |= phase | (newDelayedPhase >> 8);
    if (oldDelayedPhase != newDelayedPhase) {
        log_event_phase("updating phase from %04x to %04x for %p\n", oldDelayedPhase, newDelayedPhase, sender);
        if (newDelayedPhase) {
            if (!_devicePhaseState)
                _devicePhaseState = OSDictionary::withCapacity(0);
            if (_devicePhaseState) {
                OSNumber *newDelayedPhaseNumber = OSNumber::withNumber(newDelayedPhase, 32);
                _devicePhaseState->setObject((OSSymbol*)sender, newDelayedPhaseNumber);
                newDelayedPhaseNumber->release();
            }
            else {
                IOLog("%s unable to create _devicePhaseState dictionary\n", __func__);
            }
        }
        else {
            if (_devicePhaseState) {
                _devicePhaseState->removeObject((OSSymbol*)sender);
            }
        }
    }
    /*
     END PHASE SUPPRESSION STATE MACHINE
     **********************************************************************************************/

    switch (momentum) {
        case kScrollTypeMomentumStart:
            if (!moved) {
                // suppress the start until we get a changed with movement
                _delayedScrollMomentum = momentum;
                momentum = 0;
                options &= ~kScrollTypeMomentumAny;
            }
            else {
                // clear state and let go
                _delayedScrollMomentum = 0;
            }
            break;

        case kScrollTypeMomentumContinue:
            if (_delayedScrollMomentum) {
                // we suppressed a start
                options &= ~kScrollTypeMomentumAny;
                if (!moved) {
                    // continue suppressing it
                    momentum = 0;
                }
                else {
                    // make this the start
                    momentum = _delayedScrollMomentum;
                    options |= momentum;
                    // clear the state
                    _delayedScrollMomentum = 0;
                }
            }
            else {
                if (!moved) {
                    // if there is phase, we will allow this event to go through
                    // without changing it. if there isn't, it will get suppressed below.
                    momentum = 0;
                }
            }
            break;

        case kScrollTypeMomentumEnd:
            if (_delayedScrollMomentum) {
                // we have a suppressed start. the whole scroll needs to be suppressed.
                options &= ~kScrollTypeMomentumAny;
                momentum = 0;
                _delayedScrollMomentum = 0;
            }
            else {
                // do nothing
            }

            break;

        case 0:
            // no momentum. do nothing.
            break;

        default:
            kprintf("IOHIDSystem::scrollWheelEventGated called with unknown momentum state: %08x\n", (unsigned)momentum);
            break;
    }

    /***********************************************************************************************
     BEGIN SCROLL COUNT STATE MACHINE
     */
    UInt32 phase_momentum = options & (_scIgnoreMomentum ? kScrollTypeOptionPhaseAny : (kScrollTypeOptionPhaseAny | kScrollTypeMomentumAny));
    if (phase_momentum) {
        UInt64  axis1squared = (pointDeltaAxis1 * pointDeltaAxis1);
        UInt64  axis2squared = (pointDeltaAxis2 * pointDeltaAxis2);
        UInt64  axis3squared = (pointDeltaAxis3 * pointDeltaAxis3);
        UInt64  scrollMagnitudeSquared = axis1squared + axis2squared + axis3squared;
        UInt8   newDirection = kScrollDirectionInvalid;
        bool    checkSustain = false;

        if ((axis1squared > axis2squared) && (axis1squared > axis3squared)) {
            if (pointDeltaAxis1 > 0) {
                newDirection = kScrollDirectionXPositive;
            }
            else if (pointDeltaAxis1 < 0) {
                newDirection = kScrollDirectionXNegative;
            }
        }
        else if ((axis2squared > axis1squared) && (axis2squared > axis3squared)) {
            if (pointDeltaAxis2 > 0) {
                newDirection = kScrollDirectionYPositive;
            }
            else if (pointDeltaAxis2 < 0) {
                newDirection = kScrollDirectionYNegative;
            }
        }
        else if ((axis3squared > axis1squared) && (axis3squared > axis2squared)) {
            if (pointDeltaAxis3 > 0) {
                newDirection = kScrollDirectionZPositive;
            }
            else if (pointDeltaAxis3 < 0) {
                newDirection = kScrollDirectionZNegative;
            }
        }
        if ((newDirection != kScrollDirectionInvalid) && (newDirection != _scDirection)) {
            if (_scCount) {
                log_scroll_state("Resetting _scCount on change from %d to %d\n", _scDirection, newDirection);
                _scCount = 0;
            }
            _scDirection = newDirection;
        }

        if (_scCount && _scMouseCanReset) {
            IOFixedPoint64 thresh = IOFixedPoint64().fromIntFloor(clickSpaceThresh.x, clickSpaceThresh.y);
            IOFixedPoint64 min = _scLastScrollLocation - thresh;
            IOFixedPoint64 max = _scLastScrollLocation + thresh;
            IOFixedPoint64 location = _cursorHelper.desktopLocation();
            if ( (location > max) || (location < min) ) {
                log_scroll_state("Resetting _scCount on mouse move [%d, %d] vs [%d, %d]\n",
                                 location.xValue().as32(), location.yValue().as32(),
                                 _scLastScrollLocation.xValue().as32(), _scLastScrollLocation.yValue().as32());
                _scCount = 0;
            }
        }

        switch (phase_momentum) {
            case kScrollTypeOptionPhaseBegan: {
                if (_scCount > 0) {
                    if ((_scLastScrollEndTime + _scMaxTimeDeltaBetween) > ts) {
                        if (!_scIncrementedThisPhrase) {
                            log_scroll_state("Incrementing _scCount: %lld\n", ts);
                            _scCount++;
                            _scIncrementedThisPhrase = 1;
                        }
                        _scLastScrollSustainTime = ts;
                    }
                    else {
                        _scCount = 0;
                        log_scroll_state("Resetting _scCount due to delay: %lld + %lld < %lld\n", _scLastScrollEndTime, _scMaxTimeDeltaBetween, ts);
                    }
                }
                break;
            }
            case kScrollTypeOptionPhaseChanged: {
                if (_scCount == 0) {
                    if (scrollMagnitudeSquared >= _scMinDeltaSqToStart) {
                        log_scroll_state("_scCount to 1 on %lld > %lld\n", scrollMagnitudeSquared, _scMinDeltaSqToStart);
                        _scCount = 1;
                        _scLastScrollSustainTime = ts;
                    }
                }
                else {
                    if (_scCount > 2) {
                        IOFixed64 temp;
                        temp.fromIntFloor(llsqrt(scrollMagnitudeSquared));
                        temp /= _scAccelerationFactor;
                        _scCount += temp.as32();
                        log_scroll_state("_scCount to %d on (llsqrt(%lld) * 65536 / %lld)\n", _scCount, scrollMagnitudeSquared, _scAccelerationFactor.asFixed64());
                        if (_scCount > _scCountMax) {
                            _scCount = _scCountMax;
                        }
                    }
                    checkSustain = true;
                }
                break;
            }
            case kScrollTypeOptionPhaseEnded: {
                if (_scCount > 0) {
                    _scLastScrollEndTime = ts;
                    _scIncrementedThisPhrase = 0;
                }
                break;
            }
            case kScrollTypeOptionPhaseCanceled: {
                log_scroll_state("Resetting _scCount cancelled: %lld\n", ts);
                _scIncrementedThisPhrase = false;
                _scCount = 0;
                break;
            }
            case kScrollTypeOptionPhaseMayBegin: {
                if (_scCount > 0) {
                    if (((_scLastScrollEndTime + _scMaxTimeDeltaBetween) > ts) && !_scIncrementedThisPhrase) {
                        log_scroll_state("Incrementing _scCount: %lld\n", ts);
                        _scCount++;
                        _scIncrementedThisPhrase = 1;
                        _scLastScrollSustainTime = ts;
                    }
                    else {
                        log_scroll_state("Resetting _scCount due to delay: %lld + %lld < %lld\n", _scLastScrollEndTime, _scMaxTimeDeltaBetween, ts);
                        _scCount = 0;
                        _scIncrementedThisPhrase = 0;
                    }
                }
                break;
            }
            case kScrollTypeMomentumStart: {
                // do nothing
                break;
            }
            case kScrollTypeMomentumContinue: {
                checkSustain = true;
                break;
            }
            case kScrollTypeMomentumEnd: {
                if (_scCount > 0) {
                    _scLastScrollEndTime = ts;
                    _scIncrementedThisPhrase = 0;
                }
                break;
            }
        }

        if (checkSustain) {
            if (scrollMagnitudeSquared > _scMinDeltaSqToSustain) {
                _scLastScrollSustainTime = ts;
            }
            else if (_scLastScrollSustainTime + _scMaxTimeDeltaToSustain < ts) {
                log_scroll_state("Resetting _scCount due to sustain delay: %lld + %lld < %lld\n", _scLastScrollSustainTime, _scMaxTimeDeltaToSustain, ts);
                _scCount = 0;
            }
        }
        _scLastScrollLocation = _cursorHelper.desktopLocation();
    }
    /*
     END SCROLL COUNT STATE MACHINE
     **********************************************************************************************/

    if (!moved && !momentum && !phase) {
        log_event_phase("annotation %08x suppressed\n", phaseAnnotation);
        return;
    }
    else {
        log_event_phase("annotation %08x posted with %08x\n", phaseAnnotation, options);
    }

    TICKLE_DISPLAY(NX_SCROLLWHEELMOVED);

    bzero((char *)&wheelData, sizeof wheelData);
    wheelData.scrollWheel.deltaAxis1 = deltaAxis1;
    wheelData.scrollWheel.deltaAxis2 = deltaAxis2;
    wheelData.scrollWheel.deltaAxis3 = deltaAxis3;
    wheelData.scrollWheel.fixedDeltaAxis1 = fixedDelta1;
    wheelData.scrollWheel.fixedDeltaAxis2 = fixedDelta2;
    wheelData.scrollWheel.fixedDeltaAxis3 = fixedDelta3;
    wheelData.scrollWheel.pointDeltaAxis1 = pointDeltaAxis1;
    wheelData.scrollWheel.pointDeltaAxis2 = pointDeltaAxis2;
    wheelData.scrollWheel.pointDeltaAxis3 = pointDeltaAxis3;
    wheelData.scrollWheel.reserved1       = (UInt16)options & (kScrollTypeContinuous | kScrollTypeMomentumAny | kScrollTypeOptionPhaseAny);
    updateScrollEventForSender(sender, &wheelData);

    if (options & (kScrollTypeMomentumAny | kScrollTypeOptionPhaseAny)) {
        wheelData.scrollWheel.reserved8[0]  = phaseAnnotation;
        wheelData.scrollWheel.reserved8[1]  = _scCount;
        log_scroll_state("posting scroll: (%d, %d, %d) %d %d, %lld %lld, %lld\n",
                         pointDeltaAxis1, pointDeltaAxis2, pointDeltaAxis3, _scCount, _scDirection,
                         _scLastScrollEndTime, _scLastScrollSustainTime, ts);
        wheelData.scrollWheel.reserved8[2]  = IOHIDevice::GenerateKey(sender);
    }

    postEvent(             (options & kScrollTypeZoom) ? NX_ZOOM : NX_SCROLLWHEELMOVED,
            /* at */       &_cursorHelper.desktopLocation(),
            /* atTime */   ts,
            /* withData */ &wheelData,
            /* sender */   sender);

    return;
}

void IOHIDSystem::_tabletEvent(IOHIDSystem *self,
                               NXEventData *tabletData,
                               AbsoluteTime ts,
                               OSObject * sender,
                               void *     refcon __unused)
{
    self->tabletEvent(tabletData, ts, sender);
}

void IOHIDSystem::tabletEvent(NXEventData *tabletData,
                              AbsoluteTime ts)
{
    tabletEvent(tabletData, ts, 0);
}

void IOHIDSystem::tabletEvent(NXEventData *tabletData,
                              AbsoluteTime ts,
                              OSObject * sender)
{
    cmdGate->runAction((IOCommandGate::Action)doTabletEvent, tabletData, &ts, sender);
}

IOReturn IOHIDSystem::doTabletEvent(IOHIDSystem *self, void * arg0, void * arg1, void * arg2)
                        /* IOCommandGate::Action */
{
    NXEventData *tabletData     = (NXEventData *) arg0;
    AbsoluteTime ts     = *(AbsoluteTime *) arg1;
    OSObject * sender       = (OSObject *) arg2;

    self->tabletEventGated(tabletData, ts, sender);

    return kIOReturnSuccess;
}

void IOHIDSystem::tabletEventGated(NXEventData *tabletData,
                                    AbsoluteTime ts,
                                    OSObject * sender)
{
    CachedMouseEventStruct  *cachedMouseEvent;

    if (!eventsOpen)
        return;

    if(ShouldConsumeHIDEvent(ts, rootDomainStateChangeDeadline))
        return;

    if ((cachedMouseEvent = GetCachedMouseEventForService(cachedButtonStates, sender)) &&
        !(cachedMouseEvent->state & kCachedMousePointingTabletEventPendFlag))
    {

        cachedMouseEvent->state     |= kCachedMouseTabletEventDispFlag;
        cachedMouseEvent->subType   = NX_SUBTYPE_TABLET_POINT;
        bcopy( tabletData, &(cachedMouseEvent->tabletData), sizeof(NXEventData));

        // Don't dispatch an event if they can be embedded in pointing events
        if ( cachedMouseEvent->state & kCachedMousePointingEventDispFlag )
            return;
    }

    postEvent(NX_TABLETPOINTER,
              &_cursorHelper.desktopLocation(),
              ts,
              tabletData,
              sender);

    return;
}

void IOHIDSystem::_proximityEvent(IOHIDSystem *self,
                                  NXEventData *proximityData,
                                  AbsoluteTime ts,
                                  OSObject * sender,
                                  void *     refcon __unused)
{
    self->proximityEvent(proximityData, ts, sender);
}

void IOHIDSystem::proximityEvent(NXEventData *proximityData,
                                 AbsoluteTime ts)
{

    proximityEvent(proximityData, ts, 0);
}

void IOHIDSystem::proximityEvent(NXEventData *proximityData,
                                 AbsoluteTime ts,
                                 OSObject * sender)
{
    cmdGate->runAction((IOCommandGate::Action)doProximityEvent, proximityData, &ts, sender);
}

IOReturn IOHIDSystem::doProximityEvent(IOHIDSystem *self, void * arg0, void *arg1, void * arg2)
                        /* IOCommandGate::Action */
{

    NXEventData *proximityData  = (NXEventData *)arg0;
    AbsoluteTime ts     = *(AbsoluteTime *)arg1;
    OSObject * sender       = (OSObject *)arg2;

    self->proximityEventGated(proximityData, ts, sender);

    return kIOReturnSuccess;
}

void IOHIDSystem::proximityEventGated(NXEventData *proximityData,
                                        AbsoluteTime ts,
                                        OSObject * sender)
{
    CachedMouseEventStruct  *cachedMouseEvent;

    if (!eventsOpen)
        return;

    if(ShouldConsumeHIDEvent(ts, rootDomainStateChangeDeadline))
        return;

    if ((cachedMouseEvent = GetCachedMouseEventForService(cachedButtonStates, sender)) &&
        !(cachedMouseEvent->state & kCachedMousePointingTabletEventPendFlag))
    {
        cachedMouseEvent->state     |= kCachedMouseTabletEventDispFlag;
        cachedMouseEvent->subType   = NX_SUBTYPE_TABLET_PROXIMITY;
        bcopy( proximityData, &(cachedMouseEvent->proximityData), sizeof(NXEventData));
    }

    postEvent(NX_TABLETPROXIMITY,
              &_cursorHelper.desktopLocation(),
              ts,
              proximityData,
              sender);

    return;
}

void IOHIDSystem::doProcessKeyboardEQ(IOHIDSystem * self)
{
    processKeyboardEQ(self);
}

void IOHIDSystem::processKeyboardEQ(IOHIDSystem * self, AbsoluteTime * deadline)
{
    KeyboardEQElement * keyboardEQElement;

    KEYBOARD_EQ_LOCK;

    while ( ((keyboardEQElement = (KeyboardEQElement *)dequeue_head(&gKeyboardEQ)) != NULL)
           && !(deadline && (CMP_ABSOLUTETIME(&(keyboardEQElement->ts), deadline) > 0)))
    {
        KEYBOARD_EQ_UNLOCK;

        if (keyboardEQElement->action)
            (*(keyboardEQElement->action))(self, keyboardEQElement);
        OSSafeReleaseNULL(keyboardEQElement->sender); // NOTE: This is the matching release

        IOFree(keyboardEQElement, sizeof(KeyboardEQElement));

        KEYBOARD_EQ_LOCK;
    }

    KEYBOARD_EQ_UNLOCK;
}


//
// Process a keyboard state change.
//
void IOHIDSystem::_keyboardEvent(IOHIDSystem * self,
                                unsigned   eventType,
         /* flags */            unsigned   flags,
         /* keyCode */          unsigned   key,
         /* charCode */         unsigned   charCode,
         /* charSet */          unsigned   charSet,
         /* originalCharCode */ unsigned   origCharCode,
         /* originalCharSet */  unsigned   origCharSet,
         /* keyboardType */     unsigned   keyboardType,
         /* repeat */           bool       repeat,
         /* atTime */           AbsoluteTime ts,
                                OSObject * sender,
                                void *     refcon __unused)
{
    self->keyboardEvent(eventType, flags, key, charCode, charSet,
                        origCharCode, origCharSet, keyboardType, repeat, ts, sender);
}

void IOHIDSystem::keyboardEvent(unsigned   eventType,
         /* flags */            unsigned   flags,
         /* keyCode */          unsigned   key,
         /* charCode */         unsigned   charCode,
         /* charSet */          unsigned   charSet,
         /* originalCharCode */ unsigned   origCharCode,
         /* originalCharSet */  unsigned   origCharSet,
         /* keyboardType */     unsigned   keyboardType,
         /* repeat */           bool       repeat,
         /* atTime */           AbsoluteTime ts)
{
    keyboardEvent(eventType, flags, key, charCode, charSet,
                origCharCode, origCharSet, keyboardType, repeat, ts, 0);
}

void IOHIDSystem::keyboardEvent(unsigned   eventType,
         /* flags */            unsigned   flags,
         /* keyCode */          unsigned   key,
         /* charCode */         unsigned   charCode,
         /* charSet */          unsigned   charSet,
         /* originalCharCode */ unsigned   origCharCode,
         /* originalCharSet */  unsigned   origCharSet,
         /* keyboardType */     unsigned   keyboardType,
         /* repeat */           bool       repeat,
         /* atTime */           AbsoluteTime ts,
         /* sender */       OSObject * sender)
{
    KeyboardEQElement * keyboardEQElement = (KeyboardEQElement *)IOMalloc(sizeof(KeyboardEQElement));

    if ( !keyboardEQElement )
        return;

    bzero(keyboardEQElement, sizeof(KeyboardEQElement));

    keyboardEQElement->action   = IOHIDSystem::doKeyboardEvent;
    keyboardEQElement->ts       = ts;
    keyboardEQElement->sender   = sender;
    if (sender) sender->retain(); // NOTE: Matching release is in IOHIDSystem::processKeyboardEQ


    keyboardEQElement->event.keyboard.eventType     = eventType;
    keyboardEQElement->event.keyboard.flags         = flags;
    keyboardEQElement->event.keyboard.key           = key;
    keyboardEQElement->event.keyboard.charCode      = charCode;
    keyboardEQElement->event.keyboard.charSet       = charSet;
    keyboardEQElement->event.keyboard.origCharCode  = origCharCode;
    keyboardEQElement->event.keyboard.origCharSet   = origCharSet;
    keyboardEQElement->event.keyboard.keyboardType  = keyboardType;
    keyboardEQElement->event.keyboard.repeat        = repeat;

    KEYBOARD_EQ_LOCK;
    enqueue_tail(&gKeyboardEQ, (queue_entry_t)keyboardEQElement);
    KEYBOARD_EQ_UNLOCK;

    keyboardEQES->interruptOccurred(0, 0, 0);
}

IOReturn IOHIDSystem::doKeyboardEvent(IOHIDSystem *self, void * args)
                        /* IOCommandGate::Action */
{
    KeyboardEQElement * keyboardEQElement = (KeyboardEQElement *)args;

    AbsoluteTime ts         = keyboardEQElement->ts;
    OSObject * sender       = keyboardEQElement->sender;

    unsigned   eventType    = keyboardEQElement->event.keyboard.eventType;
    unsigned   flags        = keyboardEQElement->event.keyboard.flags;
    unsigned   key          = keyboardEQElement->event.keyboard.key;
    unsigned   charCode     = keyboardEQElement->event.keyboard.charCode;
    unsigned   charSet      = keyboardEQElement->event.keyboard.charSet;
    unsigned   origCharCode = keyboardEQElement->event.keyboard.origCharCode;
    unsigned   origCharSet  = keyboardEQElement->event.keyboard.origCharSet;
    unsigned   keyboardType = keyboardEQElement->event.keyboard.keyboardType;
    bool       repeat       = keyboardEQElement->event.keyboard.repeat;

    self->keyboardEventGated(eventType, flags, key, charCode, charSet,
                origCharCode, origCharSet, keyboardType, repeat, ts, sender);

    return kIOReturnSuccess;
}

/* This method adds a key press to the consumedKeys array, indicating that a
 * key down event was consumed and its corresponding key up must be consumed.
 * It returns true if the key was added successfully, and false if the key
 * couldn't be allocated or was already in the array.
 */
bool IOHIDSystem::addConsumedKey(unsigned key)
{
    bool result = false;
    OSNumber *keyCodeNumber;
    unsigned int index;

    keyCodeNumber = OSNumber::withNumber(key, sizeof(key) * 8);
    if ( !keyCodeNumber ) goto finish;

    index = getArrayIndexForObject(consumedKeys, keyCodeNumber);
    if ( index != kObjectNotFound ) goto finish;

    consumedKeys->setObject(keyCodeNumber);
    result = true;
finish:
    if (keyCodeNumber) keyCodeNumber->release();
    return result;
}

/* This method removes a key press from the consumed keys array, indicating
 * that a key down / key up pair have been consumed. It returns true when the
 * key is successfully removed from the array, and false if the key was not in
 * the array.
 */
bool IOHIDSystem::removeConsumedKey(unsigned key)
{
    bool result = false;
    OSNumber *keyCodeNumber;
    unsigned int index;

    keyCodeNumber = OSNumber::withNumber(key, sizeof(key) * 8);
    if ( !keyCodeNumber ) goto finish;

    index = getArrayIndexForObject(consumedKeys, keyCodeNumber);
    if ( index == kObjectNotFound ) goto finish;

    consumedKeys->removeObject(index);
    result = true;
finish:
    if (keyCodeNumber) keyCodeNumber->release();
    return result;
}

void IOHIDSystem::keyboardEventGated(unsigned   eventType,
                                /* flags */            unsigned   flags,
                                /* keyCode */          unsigned   key,
                                /* charCode */         unsigned   charCode,
                                /* charSet */          unsigned   charSet,
                                /* originalCharCode */ unsigned   origCharCode,
                                /* originalCharSet */  unsigned   origCharSet,
                                /* keyboardType */  unsigned   keyboardType,
                                /* repeat */           bool       repeat,
                                /* atTime */           AbsoluteTime ts,
                                /* sender */           OSObject * sender)
{
    UInt32 rootDomainConsumeCause;
    UInt32 displayConsumeCause;
    NXEventData outData;
    static unsigned prevFlags = 0;

    /* If we get a key up event, check the consumedKeys array to see if it
     * needs to be consumed.
     */
    if ( eventType == NX_KEYUP && consumedKeys->getCount() ) {
        if (removeConsumedKey(key)) {
        return;
        }
    }

    /* The goal here is to ignore the key presses that woke the system, and for
     * that purpose we are interested in two deadlines. First, we want to
     * consume the first key press that occurs within a second of the
     * rootDomain domain powering on, which will prevent the key press that
     * wakes the machine from posting event. Second, we want to consume any key
     * presses that occur within 50ms of that first key press or of the
     * displayWrangler powering on.  This prevents a hand smashing the keyboard
     * from generating lots of spurious events both on wake from sleep and
     * display sleep.
     */
    rootDomainConsumeCause = ShouldConsumeHIDEvent(ts, rootDomainStateChangeDeadline);
    displayConsumeCause = ShouldConsumeHIDEvent(ts, displayStateChangeDeadline);
    if (rootDomainConsumeCause || displayConsumeCause) {
        TICKLE_DISPLAY(NX_KEYDOWN);
        if (eventType != NX_KEYDOWN) return;

        (void) addConsumedKey(key);

        if (rootDomainConsumeCause) {
            AbsoluteTime_to_scalar(&rootDomainStateChangeDeadline) = 0;

            clock_get_uptime(&displayStateChangeDeadline);
            ADD_ABSOLUTETIME(&displayStateChangeDeadline,
                &gIOHIDRelativeTickleThresholdAbsoluteTime);
            }

            return;
        }

    // Notify stackshotd for CMD-OPT-CTRL-ALT-(COMMA|SLASH|PERIOD)
    if( !repeat && ((flags & NORMAL_MODIFIER_MASK) == NORMAL_MODIFIER_MASK) )
    {
        UInt32 keycode = UINT_MAX;
        switch (key) {
            case 0x2f: // kHIDUsage_KeyboardPeriod
            case 0x41: // kHIDUsage_KeypadPeriod
                keycode = kHIDUsage_KeyboardPeriod;
                break;
            case 0x2b: // kHIDUsage_KeyboardComma
            case 0x5f: // kHIDUsage_KeypadComma
                keycode = kHIDUsage_KeyboardComma;
                break;
            case 0x2c: // kHIDUsage_KeyboardSlash
            case 0x4b: // kHIDUsage_KeypadSlash
                keycode = kHIDUsage_KeyboardSlash;
                break;
            default:
                // do nothing
                break;
        }
        if (keycode != UINT_MAX) {
            if (eventType == NX_KEYDOWN) {
                sendStackShotMessage(keycode);
                IOLog("IOHIDSystem posted stackshot event 0x%02x\n", (unsigned)keycode);
            }
            return;
        }
    }

    /* If the display is off, we consume a key down event and send a tickle to
     * wake the displays. Key up events are also consumed on the rare chance
     * they aren't in the consumedKeys array, but they don't cause a tickle.
    */
    if ( !DISPLAY_IS_ENABLED ) {
        if ( eventType == NX_KEYDOWN ) {
            (void) addConsumedKey(key);
        }
        else if ( eventType == NX_KEYUP) {
            return;
        }
        else if ( eventType == NX_FLAGSCHANGED ) {
            unsigned kbFlagChanges = (flags & KEYBOARD_FLAGSMASK) ^ (prevFlags & KEYBOARD_FLAGSMASK);

            /* Flag changes due to key release shouldn't tickle display */
            if (!(kbFlagChanges & flags)) {
                prevFlags = flags;
                return;
            }
        }
        TICKLE_DISPLAY(eventType);
        return;
    }
    prevFlags = flags;

    TICKLE_DISPLAY(NX_KEYDOWN);

    // RY: trigger NMI for CMD-OPT-CTRL-ALT-ESC
    if( !repeat && (key == 0x35) &&
        (eventType == NX_KEYDOWN) &&
        ((flags & NORMAL_MODIFIER_MASK) == NORMAL_MODIFIER_MASK))
    {
        PE_enter_debugger("USB Programmer Key");
    }

    if ( eventsOpen ) {
        UInt32 usage = 0;
        UInt32 usagePage = 0;
        if ((flags & NX_HIGHCODE_ENCODING_MASK) == NX_HIGHCODE_ENCODING_MASK) {
            usage = (origCharCode & 0xffff0000) >> 16;
            usagePage = (origCharSet & 0xffff0000) >> 16;
            origCharCode &= 0xffff;
            origCharSet &= 0xffff;
        }

        outData.key.repeat = repeat;
        outData.key.keyCode = key;
        outData.key.charSet = charSet;
        outData.key.charCode = charCode;
        outData.key.origCharSet = origCharSet;
        outData.key.origCharCode = origCharCode;
        outData.key.keyboardType = keyboardType;
        outData.key.reserved2 = usage;
        outData.key.reserved3 = usagePage;

        evg->eventFlags = (evg->eventFlags & ~KEYBOARD_FLAGSMASK)
                | (flags & KEYBOARD_FLAGSMASK);

        if (cachedEventFlags != evg->eventFlags) {
            cachedEventFlags = evg->eventFlags;

            // RY: Reset the clickTime as well on modifier
            // change to prevent double click from occuring
            nanoseconds_to_absolutetime(0, &clickTime);
        }

        postEvent(         eventType,
            /* at */       &_cursorHelper.desktopLocation(),
            /* atTime */   ts,
            /* withData */ &outData,
            /* sender */   sender,
            /* extPID */   0,
            /* processKEQ*/false);
    } else {
        // Take on BSD console duties and dispatch the keyboardEvents.
        static const char cursorCodes[] = { 'D', 'A', 'C', 'B' };
        if( (eventType == NX_KEYDOWN) && ((flags & NX_ALTERNATEMASK) != NX_ALTERNATEMASK)) {
            if( (charSet == NX_SYMBOLSET)
                && (charCode >= 0xac) && (charCode <= 0xaf)) {
                cons_cinput( '\033');
                cons_cinput( 'O');
                charCode = cursorCodes[ charCode - 0xac ];
            }
            cons_cinput( charCode );
        }
    }
}

void IOHIDSystem::_keyboardSpecialEvent(  IOHIDSystem * self,
                      unsigned   eventType,
                       /* flags */        unsigned   flags,
                       /* keyCode  */     unsigned   key,
                       /* specialty */    unsigned   flavor,
                       /* guid */         UInt64     guid,
                       /* repeat */       bool       repeat,
                       /* atTime */       AbsoluteTime ts,
                      OSObject * sender,
                      void *     refcon __unused)
{
    self->keyboardSpecialEvent(eventType, flags, key, flavor, guid, repeat, ts, sender);
}

void IOHIDSystem::keyboardSpecialEvent(   unsigned   eventType,
                       /* flags */        unsigned   flags,
                       /* keyCode  */     unsigned   key,
                       /* specialty */    unsigned   flavor,
                       /* guid */         UInt64     guid,
                       /* repeat */       bool       repeat,
                       /* atTime */       AbsoluteTime ts)
{
    keyboardSpecialEvent(eventType, flags, key, flavor, guid, repeat, ts, 0);
}

void IOHIDSystem::keyboardSpecialEvent(   unsigned   eventType,
                       /* flags */        unsigned   flags,
                       /* keyCode  */     unsigned   key,
                       /* specialty */    unsigned   flavor,
                       /* guid */         UInt64     guid,
                       /* repeat */       bool       repeat,
                       /* atTime */       AbsoluteTime ts,
                       /* sender */       OSObject * sender)
{
    KeyboardEQElement * keyboardEQElement = NULL;

    keyboardEQElement = (KeyboardEQElement *)IOMalloc(sizeof(KeyboardEQElement));

    if ( !keyboardEQElement )
        return;

    bzero(keyboardEQElement, sizeof(KeyboardEQElement));

    keyboardEQElement->action   = IOHIDSystem::doKeyboardSpecialEvent;
    keyboardEQElement->ts       = ts;
    keyboardEQElement->sender   = sender;
    if (sender) sender->retain(); // NOTE: Matching release is in IOHIDSystem::processKeyboardEQ

    keyboardEQElement->event.keyboardSpecial.eventType  = eventType;
    keyboardEQElement->event.keyboardSpecial.flags      = flags;
    keyboardEQElement->event.keyboardSpecial.key        = key;
    keyboardEQElement->event.keyboardSpecial.flavor     = flavor;
    keyboardEQElement->event.keyboardSpecial.guid       = guid;
    keyboardEQElement->event.keyboardSpecial.repeat     = repeat;

    KEYBOARD_EQ_LOCK;
    enqueue_tail(&gKeyboardEQ, (queue_entry_t)keyboardEQElement);
    KEYBOARD_EQ_UNLOCK;

    keyboardEQES->interruptOccurred(0, 0, 0);
}


IOReturn IOHIDSystem::doKeyboardSpecialEvent(IOHIDSystem *self, void * args)
                        /* IOCommandGate::Action */
{
    KeyboardEQElement * keyboardEQElement = (KeyboardEQElement *)args;

    AbsoluteTime    ts          = keyboardEQElement->ts;
    OSObject *      sender      = keyboardEQElement->sender;

    unsigned        eventType   = keyboardEQElement->event.keyboardSpecial.eventType;
    unsigned        flags       = keyboardEQElement->event.keyboardSpecial.flags;
    unsigned        key         = keyboardEQElement->event.keyboardSpecial.key;
    unsigned        flavor      = keyboardEQElement->event.keyboardSpecial.flavor;
    UInt64          guid        = keyboardEQElement->event.keyboardSpecial.guid;
    bool            repeat      = keyboardEQElement->event.keyboardSpecial.repeat;

    self->keyboardSpecialEventGated(eventType, flags, key, flavor, guid, repeat, ts, sender);

    return kIOReturnSuccess;
}

void IOHIDSystem::keyboardSpecialEventGated(
                                /* event */     unsigned   eventType,
                                /* flags */     unsigned   flags,
                                /* keyCode  */  unsigned   key,
                                /* specialty */ unsigned   flavor,
                                /* guid */      UInt64     guid,
                                /* repeat */    bool       repeat,
                                /* atTime */    AbsoluteTime ts,
                                /* sender */    OSObject * sender)
{
    NXEventData outData;
    int     level = -1;

    if ((key != NX_NOSPECIALKEY) || (flavor != NX_SUBTYPE_STICKYKEYS_RELEASE)) {
        /* Key up events shouldn't tickle when display is off */
        if (DISPLAY_IS_ENABLED || eventType == NX_KEYDOWN)
            TICKLE_DISPLAY(NX_SYSDEFINED);
    }

    if (ShouldConsumeHIDEvent(ts, rootDomainStateChangeDeadline) ||
        !DISPLAY_IS_ENABLED)
    {
        return;
    }

    // Since the HIDSystem will now take on BSD Console duty,
    // we need to make sure to process the programmer key info
    // prior to doing the eventsOpen check
    if ( eventType == NX_KEYDOWN && flavor == NX_POWER_KEY && !repeat) {
        if ( (flags & (NORMAL_MODIFIER_MASK | NX_DEVICELCMDKEYMASK | NX_DEVICERCMDKEYMASK)) ==
              (NX_COMMANDMASK | NX_DEVICELCMDKEYMASK | NX_DEVICERCMDKEYMASK) )
            PE_enter_debugger("Programmer Key");
        else if ( (flags & NORMAL_MODIFIER_MASK) == ( NX_COMMANDMASK | NX_CONTROLMASK ) )
            PEHaltRestart(kPERestartCPU);
    }

    if ( !eventsOpen )
        return;

    // clear event record
    bzero( (void *)&outData, sizeof outData );

    // Update flags.
    evg->eventFlags = (evg->eventFlags & ~KEYBOARD_FLAGSMASK)
            | (flags & KEYBOARD_FLAGSMASK);

    // was this a keydown event
    if ( eventType == NX_KEYDOWN )
    {
        notifyHIDevices(this, ioHIDevices, kIOHIDSystem508SpecialKeyDownMessage);

        // which special key went down
        switch ( flavor )
        {
            case NX_KEYTYPE_EJECT:

                //  Special key handlers:
                //
                //  Command = invoke macsbug
                //  Command+option = sleep now
                //  Command+option+control = shutdown now
                //  Control = logout dialog

                if(  (evg->eventFlags & NX_COMMANDMASK)     &&
                    !(evg->eventFlags & NX_CONTROLMASK)     &&
                    !(evg->eventFlags & NX_SHIFTMASK)       &&
                    !(evg->eventFlags & NX_ALTERNATEMASK)       )
                {
                    // Post a power key event, Classic should pick this up and
                    // drop into MacsBug.
                    //
                    outData.compound.subType = NX_SUBTYPE_POWER_KEY;
                    postEvent(     NX_SYSDEFINED,
                    /* at */       &_cursorHelper.desktopLocation(),
                    /* atTime */   ts,
                    /* withData */ &outData,
                    /* sender */   sender,
                    /* extPID */   0,
                    /* processKEQ*/false);
                }
                else if(   (evg->eventFlags & NX_COMMANDMASK)   &&
                          !(evg->eventFlags & NX_CONTROLMASK)   &&
                          !(evg->eventFlags & NX_SHIFTMASK)     &&
                           (evg->eventFlags & NX_ALTERNATEMASK)     )
                {
                    //IOLog( "IOHIDSystem -- sleep now!\n" );

                    // The Key release events may not get delivered
                    // to clients if they come late in sleep process. So,
                    // clear the flags before heading to sleep.
                    evg->eventFlags = 0;
                    // Post the sleep now event. Someone else will handle the actual call.
                    //
                    outData.compound.subType = NX_SUBTYPE_SLEEP_EVENT;
                    postEvent(     NX_SYSDEFINED,
                    /* at */       &_cursorHelper.desktopLocation(),
                    /* atTime */   ts,
                    /* withData */ &outData,
                    /* sender */   sender,
                    /* extPID */   0,
                    /* processKEQ*/false);

                }
                else if(   (evg->eventFlags & NX_COMMANDMASK)   &&
                           (evg->eventFlags & NX_CONTROLMASK)   &&
                          !(evg->eventFlags & NX_SHIFTMASK)     &&
                           (evg->eventFlags & NX_ALTERNATEMASK)     )
                {
                    //IOLog( "IOHIDSystem -- shutdown now!\n" );

                    // Post the shutdown now event. Someone else will handle the actual call.
                    //
                    outData.compound.subType = NX_SUBTYPE_SHUTDOWN_EVENT;
                    postEvent(     NX_SYSDEFINED,
                    /* at */       &_cursorHelper.desktopLocation(),
                    /* atTime */   ts,
                    /* withData */ &outData,
                    /* sender */   sender,
                    /* extPID */   0,
                    /* processKEQ*/false);

                }
                else if(   (evg->eventFlags & NX_COMMANDMASK)   &&
                           (evg->eventFlags & NX_CONTROLMASK)   &&
                          !(evg->eventFlags & NX_SHIFTMASK)     &&
                          !(evg->eventFlags & NX_ALTERNATEMASK)     )
                {
                    // Restart now!
                    //IOLog( "IOHIDSystem -- Restart now!\n" );

                    // Post the Restart now event. Someone else will handle the actual call.
                    //
                    outData.compound.subType = NX_SUBTYPE_RESTART_EVENT;
                    postEvent(     NX_SYSDEFINED,
                    /* at */       &_cursorHelper.desktopLocation(),
                    /* atTime */   ts,
                    /* withData */ &outData,
                    /* sender */   sender,
                    /* extPID */   0,
                    /* processKEQ*/false);

                }
                else if(  !(evg->eventFlags & NX_COMMANDMASK)   &&
                           (evg->eventFlags & NX_CONTROLMASK)   &&
                          !(evg->eventFlags & NX_SHIFTMASK)     &&
                          !(evg->eventFlags & NX_ALTERNATEMASK)     )
                {
                    // Looks like we should put up the normal 'Power Key' dialog.
                    //
                    // Set the event flags to zero, because the system will not do the right
                    // thing if we don't zero this out (it will ignore the power key event
                    // we post, thinking that some modifiers are down).
                    //
                    evg->eventFlags = 0;

                    // Post the power keydown event.
                    //
                    outData.compound.subType = NX_SUBTYPE_POWER_KEY;
                    postEvent(     NX_SYSDEFINED,
                    /* at */       &_cursorHelper.desktopLocation(),
                    /* atTime */   ts,
                    /* withData */ &outData,
                    /* sender */   sender,
                    /* extPID */   0,
                    /* processKEQ*/false);

                }
                else
                {
                    // After all that checking, no modifiers are down, so let's pump up a
                    // system defined eject event. This way we can have anyone who's watching
                    // for this event (aka LoginWindow) route this event to the right target
                    // (aka AutoDiskMounter).

                    //IOLog( "IOHIDSystem--Normal Eject action!\n" );

                    // Post the eject keydown event.
                    //
                    outData.compound.subType = NX_SUBTYPE_EJECT_KEY;
                    postEvent(     NX_SYSDEFINED,
                    /* at */       &_cursorHelper.desktopLocation(),
                    /* atTime */   ts,
                    /* withData */ &outData,
                    /* sender */   sender,
                    /* extPID */   0,
                    /* processKEQ*/false);

                }
                break;

            case NX_POWER_KEY:
                // first issue a power key down event
                keyboardEventGated(eventType,
                                   flags,
                                   0x7f,
                                   0,
                                   0,
                                   0,
                                   0,
                                   0,
                                   0,
                                   ts,
                                   sender);
                // Now the special event
                outData.compound.subType = NX_SUBTYPE_POWER_KEY;
                postEvent(         NX_SYSDEFINED,
                    /* at */       &_cursorHelper.desktopLocation(),
                    /* atTime */   ts,
                    /* withData */ &outData,
                    /* sender */   sender,
                    /* extPID */   0,
                    /* processKEQ*/false);
                break;
        }
    }
    else if ( eventType == NX_KEYUP )
    {
        switch ( flavor )
        {
            case NX_POWER_KEY:
                keyboardEventGated(eventType,
                                   flags,
                                   0x7f,
                                   0,
                                   0,
                                   0,
                                   0,
                                   0,
                                   0,
                                   ts,
                                   sender);
                break;
        }
    }
    // if someone pases a sysdefined type in, then its ready to go already
    else if ( eventType == NX_SYSDEFINED )
    {
        outData.compound.subType = flavor;
        postEvent(         NX_SYSDEFINED,
            /* at */       &_cursorHelper.desktopLocation(),
            /* atTime */   ts,
            /* withData */ &outData,
            /* sender */   sender,
                    /* extPID */   0,
                    /* processKEQ*/false);

    }

    // post keydowns and key ups if this flavor should be posted
    // note, we dont want to do this for events of type NX_SYSDEFINED
    // we probably _should_ just check for keydowns and keyups here
    // but the smallest change was to just make sure we didnt do this for
    // the new type, which is NX_SYSDEFINED (used for sticky keys feature)
    if ( eventType != NX_SYSDEFINED )
    {
          if( (flags & SPECIALKEYS_MODIFIER_MASK)
            && (flavor == NX_POWER_KEY))
      {
        //Should modifier + POWER ever be dispatched?  Command-power
        //  is used to get into debugger and MacsBug
      }
      else
      {
            outData.compound.subType   = NX_SUBTYPE_AUX_CONTROL_BUTTONS;
            outData.compound.misc.L[0] = (flavor << 16) | (eventType << 8) | repeat;
            outData.compound.misc.L[1] = guid & 0xffffffff;
            outData.compound.misc.L[2] = guid >> 32;

            postEvent(             NX_SYSDEFINED,
                    /* at */       &_cursorHelper.desktopLocation(),
                    /* atTime */   ts,
                    /* withData */ &outData,
                    /* sender */   sender,
                    /* extPID */   0,
                    /* processKEQ*/false);

          }
    }

    if ( level != -1 )  // An interesting special key event occurred
    {
        evSpecialKeyMsg(        flavor,
            /* direction */ eventType,
            /* flags */     flags,
            /* level */     level);
    }
}

/*
 * Update current event flags.  Restricted to keyboard flags only, this
 * method is used to silently update the flags state for keys which both
 * generate characters and flag changes.  The specs say we don't generate
 * a flags-changed event for such keys.  This method is also used to clear
 * the keyboard flags on a keyboard subsystem reset.
 */
void IOHIDSystem::_updateEventFlags(IOHIDSystem * self,
                                    unsigned      flags,
                                    OSObject *    sender,
                                    void *        refcon __unused)
{
    self->updateEventFlags(flags, sender);
}

void IOHIDSystem::updateEventFlags(unsigned flags)
{
    updateEventFlags(flags, 0);
}

void IOHIDSystem::updateEventFlags(unsigned flags, OSObject * sender)
{
    KeyboardEQElement * keyboardEQElement = (KeyboardEQElement *)IOMalloc(sizeof(KeyboardEQElement));

    if ( !keyboardEQElement )
        return;

    bzero(keyboardEQElement, sizeof(KeyboardEQElement));

    keyboardEQElement->action = IOHIDSystem::doUpdateEventFlags;
    keyboardEQElement->sender = sender;
    if (sender) sender->retain(); // NOTE: Matching release is in IOHIDSystem::processKeyboardEQ

    keyboardEQElement->event.flagsChanged.flags = flags;

    KEYBOARD_EQ_LOCK;
    enqueue_tail(&gKeyboardEQ, (queue_entry_t)keyboardEQElement);
    KEYBOARD_EQ_UNLOCK;

    keyboardEQES->interruptOccurred(0, 0, 0);
}

IOReturn IOHIDSystem::doUpdateEventFlags(IOHIDSystem *self, void * args)
                        /* IOCommandGate::Action */
{
    KeyboardEQElement * keyboardEQElement = (KeyboardEQElement *)args;

    OSObject * sender   = keyboardEQElement->sender;
    unsigned   flags    = keyboardEQElement->event.flagsChanged.flags;

    self->updateEventFlagsGated(flags, sender);

    return kIOReturnSuccess;
}

void IOHIDSystem::updateEventFlagsGated(unsigned flags, OSObject * sender __unused)
{
    if ( eventsOpen ) {
        evg->eventFlags = (evg->eventFlags & ~KEYBOARD_FLAGSMASK)
                | (flags & KEYBOARD_FLAGSMASK);

            // RY: Reset the clickTime as well on modifier
            // change to prevent double click from occuring
            nanoseconds_to_absolutetime(0, &clickTime);
        }
}

//
// - _setButtonState:(int)buttons  atTime:(int)t
//  Update the button state.  Generate button events as needed
//
void IOHIDSystem::_setButtonState(int buttons,
                                  /* atTime */ AbsoluteTime ts,
                                  OSObject * sender)
{
    // *** HACK ALERT ***
    // Chache the sent button state and or it with the other button states.
    // This will prevent awkward behavior when two pointing devices are used
    // at the same time.
    CachedMouseEventStruct *cachedMouseEvent = NULL;

    if ( cachedButtonStates ) {
        cachedMouseEvent = GetCachedMouseEventForService(cachedButtonStates, sender);

        if (cachedMouseEvent) {
            cachedMouseEvent->lastButtons = buttons;
        }

        if (evg->buttons == buttons)
            return;

        buttons = GetCachedMouseButtonStates(cachedButtonStates);
    }
    // *** END HACK ***

    // Once again check if new button state differs
    if (evg->buttons == buttons)
        return;

    if (_scMouseCanReset && _scCount) {
        log_scroll_state("Resetting _scCount due to click: %lld\n", ts);
        _scCount = 0;
    }

    // Magic uber-mouse buttons changed event so we can get all of the buttons...
    NXEventData evData;
    unsigned long hwButtons, hwDelta;

    /* I'd like to keep the event button mapping linear, so
        I have to "undo" the LB/RB mouse bit numbering funkiness
        before I pass the information down to the app. */
    /* Ideally this would all go away if we fixed EV_LB and EV_RB
        to be bits 0 and 1 */
    CONVERT_EV_TO_HW_BUTTONS(buttons, hwButtons);
    CONVERT_EV_TO_HW_DELTA((evg->buttons ^ buttons), hwDelta);

    evData.compound.reserved = 0;
    evData.compound.subType = NX_SUBTYPE_AUX_MOUSE_BUTTONS;
    evData.compound.misc.L[0] = hwDelta;
    evData.compound.misc.L[1] = hwButtons;

    postEvent(  NX_SYSDEFINED,
                /* at */ &_cursorHelper.desktopLocation(),
                /* atTime */ ts,
                /* withData */ &evData,
                /* sender */    sender);

    // End Magic uber-mouse buttons changed event

    bzero(&evData, sizeof(NXEventData));

    // vtn3: This will update the data structure for touch devices
    updateMouseEventForSender(sender, &evData);

    // RY: Roll in the tablet info we got from absolutePointerEventGated
    // into a button event.  Prior to this we were sending null data to
    // post event.  This won't be much different as the only non-zero
    // contents should be the tablet area of the event.
    if (cachedMouseEvent ||
        (NULL != (cachedMouseEvent = GetCachedMouseEventForService(cachedButtonStates, sender)))) {
        if (evData.mouse.subType != NX_SUBTYPE_MOUSE_TOUCH)
            evData.mouse.subType = cachedMouseEvent->subType;
        evData.mouse.pressure = cachedMouseEvent->lastPressure;

        if (cachedMouseEvent->subType == NX_SUBTYPE_TABLET_POINT) {
            bcopy(&(cachedMouseEvent->tabletData), &(evData.mouse.tablet.point), sizeof(NXTabletPointData));
        }
        else if (cachedMouseEvent->subType == NX_SUBTYPE_TABLET_PROXIMITY) {
            bcopy(&(cachedMouseEvent->proximityData), &(evData.mouse.tablet.proximity), sizeof(NXTabletProximityData));
        }
    }
    evData.mouse.subx = _cursorHelper.desktopLocation().xValue().fraction() >> 8;
    evData.mouse.suby = _cursorHelper.desktopLocation().yValue().fraction() >> 8;

    if ((evg->buttons & EV_LB) != (buttons & EV_LB)) {
        if (buttons & EV_LB) {
            postEvent(             NX_LMOUSEDOWN,
                                   /* at */       &_cursorHelper.desktopLocation(),
                                   /* atTime */   ts,
                                   /* withData */ &evData,
                                   /* sender */   sender);
        }
        else {
            postEvent(             NX_LMOUSEUP,
                                   /* at */       &_cursorHelper.desktopLocation(),
                                   /* atTime */   ts,
                                   /* withData */ &evData,
                                   /* sender */   sender);
        }
        // After entering initial up/down event, set up
        // coalescing state so drags will behave correctly
        evg->dontCoalesce = evg->dontWantCoalesce;
        if (evg->dontCoalesce)
            evg->eventFlags |= NX_NONCOALSESCEDMASK;
        else
            evg->eventFlags &= ~NX_NONCOALSESCEDMASK;
    }

    if ((evg->buttons & EV_RB) != (buttons & EV_RB)) {
        if (buttons & EV_RB) {
            postEvent(             NX_RMOUSEDOWN,
                                   /* at */       &_cursorHelper.desktopLocation(),
                                   /* atTime */   ts,
                                   /* withData */ &evData,
                                   /* sender */   sender);
        }
        else {
            postEvent(             NX_RMOUSEUP,
                                   /* at */       &_cursorHelper.desktopLocation(),
                                   /* atTime */   ts,
                                   /* withData */ &evData,
                                   /* sender */   sender);
        }
    }

    evg->buttons = buttons;
}

//
//  Sets the cursor position (evg->cursorLoc) to the new
//  location.  The location is clipped against the cursor pin rectangle,
//  mouse moved/dragged events are generated using the given event mask,
//  and a mouse-exited event may be generated. The cursor image is
//  moved.
//  This should be run from a command gate action.
//
void IOHIDSystem::setCursorPosition(IOGPoint * newLoc, bool external, OSObject * sender)
{
    if ( eventsOpen == true )
    {
        clock_get_uptime(&_cursorEventLast);
        _cursorHelper.desktopLocationDelta().xValue() += (newLoc->x - _cursorHelper.desktopLocation().xValue());
        _cursorHelper.desktopLocationDelta().yValue() += (newLoc->y - _cursorHelper.desktopLocation().yValue());
        _cursorHelper.desktopLocation().fromIntFloor(newLoc->x, newLoc->y);
        _setCursorPosition(external, false, sender);
        _cursorMoveLast = _cursorEventLast;
        scheduleNextPeriodicEvent();
    }
}

//
// This mechanism is used to update the cursor position, possibly generating
// messages to registered frame buffer devices and posting drag, tracking, and
// mouse motion events.
//
// This should be run from a command gate action.
// This can be called from setCursorPosition:(IOGPoint *)newLoc to set the
// position by a _IOSetParameterFromIntArray() call, directly from the absolute or
// relative pointer device routines, or on a timed event callback.
//

void IOHIDSystem::enableContinuousCursor()
{
    kprintf("%s called\n", __func__);
    
    _continuousCursor = true;
}

void IOHIDSystem::disableContinuousCursor()
{
    kprintf("%s called\n", __func__);
    
    _continuousCursor = false;
}

void IOHIDSystem::_setCursorPosition(bool external, bool proximityChange, OSObject * sender)
{
    bool cursorMoved = true;
    IOHID_DEBUG(kIOHIDDebugCode_SetCursorPosition, sender,
                _cursorHelper.desktopLocation().xValue().as64(),
                _cursorHelper.desktopLocation().yValue().as64(), 0);
    IOHID_DEBUG(kIOHIDDebugCode_SetCursorPosition, sender, proximityChange, external, 1);
    PROFILE_TRACE(9);

    if (!screens) {
        return;
    }

    if( OSSpinLockTry(&evg->cursorSema) == 0 ) { // host using shmem
        // try again later
        return;
    }

    // Past here we hold the cursorSema lock.  Make sure the lock is
    // cleared before returning or the system will be wedged.
    if (cursorCoupled || external)
    {
        UInt32 newScreens = 0;
        SInt32 pinScreen = -1L;
        EvScreen *screen = (EvScreen *)evScreen;

        bool foundFirstScreen = false;      // T => found first valid screen
        bool foundSecondScreen = false;     // T => found second valid screen
        int leftScreen = 0;
        int rightScreen = 0;

        if (cursorPinned) {
            _cursorHelper.desktopLocation().clipToRect(cursorPin);
        }
        else {
            /* Get mask of screens on which the cursor is present */
            for (int i = 0; i < screens; i++ ) {
                if (!screen[i].desktopBounds)
                    continue;
                if ((screen[i].desktopBounds->maxx - screen[i].desktopBounds->minx) < 128)
                    continue;
                if (_continuousCursor) {
                    if( !foundFirstScreen ) {
                        foundFirstScreen = true;
                        leftScreen = i;
                        rightScreen = i;
                    }
                    else if( !foundSecondScreen ) {
                        // I need this if I care about vertical stacks of displays
    //                    foundSecondScreen =
    //                            (screen[leftScreen].desktopBounds->minx != screen[i].desktopBounds->minx) ||
    //                            (screen[leftScreen].desktopBounds->maxx != screen[i].desktopBounds->maxx) ||
    //                            (screen[leftScreen].desktopBounds->miny != screen[i].desktopBounds->miny) ||
    //                            (screen[leftScreen].desktopBounds->maxy != screen[i].desktopBounds->maxy);
                    }

    //                if (screen[i].desktopBounds->minx < screen[leftScreen].desktopBounds->minx) {     // overlaps can torroid
                    if (screen[i].desktopBounds->maxx <= screen[leftScreen].desktopBounds->minx) {      // non-overlaps torroid
                        leftScreen = i;
                    }
    //                if (screen[rightScreen].desktopBounds->maxx < screen[i].desktopBounds->maxx )     // overlaps can torroid
                    if (screen[rightScreen].desktopBounds->maxx <= screen[i].desktopBounds->minx )  // non-overlaps torroid
                    {
                        rightScreen = i;
                    }
                }
                if (_cursorHelper.desktopLocation().inRect(*screen[i].desktopBounds)) {
                    pinScreen = i;
                    newScreens |= (1 << i);
                }
            }
        }

        if (newScreens == 0) {
            /* At this point cursor has gone off all screens,
               clip it to the closest screen. */
            IOFixedPoint64  aimLoc = _cursorHelper.desktopLocation();
            int64_t     dx;
            int64_t     dy;
            uint64_t    distance;
            uint64_t    bestDistance = -1ULL;
            
            const int64_t       kHackMiniumWrapDistance = 0; // 22 * 22;
            const int64_t       kHackUpperNoWrap = 40;       // Amount of space at the top that does not wrap
            const int64_t       kHackLowerNoWrap = 40;       // Amount of space at the bottom that does not wrap
                                                            // TODO: decide if menubar is special for wrap (and how)
            for (int i = 0; i < screens; i++ ) {
                if (!screen[i].desktopBounds)
                    continue;
                if ((screen[i].desktopBounds->maxx - screen[i].desktopBounds->minx) < 128)
                    continue;
                if (_continuousCursor && (screen[i].desktopBounds->maxy - screen[i].desktopBounds->miny) < 128)
                    continue;

                IOFixedPoint64  pinnedLoc = aimLoc;
                pinnedLoc.clipToRect(*screen[i].desktopBounds);
                dx = (pinnedLoc.xValue() - aimLoc.xValue()).as64();
                dy = (pinnedLoc.yValue() - aimLoc.yValue()).as64();
                distance = dx * dx + dy * dy;
                
                if (distance <= bestDistance) {
                    bestDistance = distance;
                    
                    if (_continuousCursor &&
                        leftScreen != rightScreen &&
                        screen[i].desktopBounds->miny + kHackUpperNoWrap < pinnedLoc.yValue().as64()  &&
                        pinnedLoc.yValue().as64() < screen[i].desktopBounds->maxy - kHackLowerNoWrap  &&
                        kHackMiniumWrapDistance < bestDistance ) {
                        
                        IOFixed64   targetX;
                        IOFixed64   targetY;
                        int         targetDisplay = i;
                        bool        shouldWrap = true;      // Assume we're wrapping (makes code below shorter)
                        
                        if (aimLoc.xValue().as64() < screen[leftScreen].desktopBounds->minx ) {
                            // Wrap to right side of right screen
                            targetX.fromIntFloor(screen[rightScreen].desktopBounds->maxx);
                            targetY.fromIntFloor(screen[rightScreen].desktopBounds->maxy - screen[rightScreen].desktopBounds->miny);
                            targetDisplay = rightScreen;
                            kprintf("IOHIDSystem::_setCursorPosition Wrap to right side of right screen\n");
                       }
                        else if (screen[rightScreen].desktopBounds->maxx < aimLoc.xValue().as64()) {
                            // Wrap to left side of left screen
                            targetX.fromIntFloor(screen[leftScreen].desktopBounds->minx);
                            targetY.fromIntFloor(screen[leftScreen].desktopBounds->maxy - screen[leftScreen].desktopBounds->miny);
                            targetDisplay = leftScreen;
                            kprintf("IOHIDSystem::_setCursorPosition Wrap to left side of left screen\n");
                       }
                        else {
                            shouldWrap = false;
                            kprintf("IOHIDSystem::_setCursorPosition don't wrap\n");
                        }

                        if (shouldWrap)
                        {
                            IOFixed64   closestYFraction;
                            closestYFraction.fromIntFloor(pinnedLoc.yValue().as64() - screen[i].desktopBounds->miny);
                            closestYFraction /= (screen[i].desktopBounds->maxy - screen[i].desktopBounds->miny);                            
                            targetY *= closestYFraction;
                            
                            pinnedLoc.fromFixed64(targetX, targetY);
                            pinnedLoc.clipToRect(*screen[targetDisplay].desktopBounds); // SHOULD NOT NEED - makes SURE we hit a screen

                        }
                    }
                    
                    _cursorHelper.desktopLocation() = pinnedLoc;
                }
            }
            IOHID_DEBUG(kIOHIDDebugCode_SetCursorPosition, sender,
                        _cursorHelper.desktopLocation().xValue().as64(),
                        _cursorHelper.desktopLocation().yValue().as64(), 2);

            /* regenerate mask for new position */
            for (int i = 0; i < screens; i++ ) {
                if (!screen[i].desktopBounds)
                    continue;
                if ((screen[i].desktopBounds->maxx - screen[i].desktopBounds->minx) < 128)
                    continue;
                if (_cursorHelper.desktopLocation().inRect(*screen[i].desktopBounds)) {
                    pinScreen = i;
                    newScreens |= (1 << i);
                }
            }
        }

        /* Catch the no-move case */
        if ((_cursorHelper.desktopLocation().xValue().asFixed24x8() == evg->desktopCursorFixed.x) &&
            (_cursorHelper.desktopLocation().yValue().asFixed24x8() == evg->desktopCursorFixed.y) &&
            (proximityChange == 0) && (!_cursorHelper.desktopLocationDelta())) {
            cursorMoved = false;    // mouse moved, but cursor didn't
        }
        else {
            evg->cursorLoc.x = _cursorHelper.desktopLocation().xValue().as32();
            evg->cursorLoc.y = _cursorHelper.desktopLocation().yValue().as32();
            evg->desktopCursorFixed.x = _cursorHelper.desktopLocation().xValue().asFixed24x8();
            evg->desktopCursorFixed.y = _cursorHelper.desktopLocation().yValue().asFixed24x8();
            if (pinScreen >= 0) {
                _cursorHelper.updateScreenLocation(screen[pinScreen].desktopBounds, screen[pinScreen].displayBounds);
            }
            else {
                _cursorHelper.updateScreenLocation(NULL, NULL);
            }
            evg->screenCursorFixed.x = _cursorHelper.getScreenLocation().xValue().asFixed24x8();
            evg->screenCursorFixed.y = _cursorHelper.getScreenLocation().yValue().asFixed24x8();

            /* If cursor changed screens */
            if (newScreens != cursorScreens) {
                hideCursor();   /* hide cursor on old screens */
                cursorScreens = newScreens;
                if (pinScreen >= 0) {
                    cursorPin = *(((EvScreen*)evScreen)[pinScreen].desktopBounds);
                    cursorPinScreen = pinScreen;
                    showCursor();
                }
            } else {
                /* cursor moved on same screens */
                moveCursor();
            }
        }
    }
    else {
        /* cursor uncoupled */
        _cursorHelper.desktopLocation().xValue().fromFixed24x8(evg->desktopCursorFixed.x);
        _cursorHelper.desktopLocation().yValue().fromFixed24x8(evg->desktopCursorFixed.y);
    }

    AbsoluteTime    uptime;
    clock_get_uptime(&uptime);

    _cursorLog(uptime);

    /* See if anybody wants the mouse moved or dragged events */
    // Note: extPostEvent clears evg->movedMask as a hack to prevent these events
    // so any change here should check to make sure it does not break that hack
    if (evg->movedMask) {
        if      ((evg->movedMask & NX_LMOUSEDRAGGEDMASK) && (evg->buttons & EV_LB)) {
            _postMouseMoveEvent(NX_LMOUSEDRAGGED, uptime, sender);
        }
        else if ((evg->movedMask & NX_RMOUSEDRAGGEDMASK) && (evg->buttons & EV_RB)) {
            _postMouseMoveEvent(NX_RMOUSEDRAGGED, uptime, sender);
        }
        else if  (evg->movedMask & NX_MOUSEMOVEDMASK) {
            _postMouseMoveEvent(NX_MOUSEMOVED, uptime, sender);
        }
    }

    /* check new cursor position for leaving evg->mouseRect */
    if (cursorMoved && evg->mouseRectValid && _cursorHelper.desktopLocation().inRect(evg->mouseRect))
    {
        if (evg->mouseRectValid)
        {
            postEvent(/* what */     NX_MOUSEEXITED,
                      /* at */       &_cursorHelper.desktopLocation(),
                      /* atTime */   uptime,
                      /* withData */ NULL,
                      /* sender */   sender);
            evg->mouseRectValid = 0;
        }
    }
    OSSpinLockUnlock(&evg->cursorSema);
    PROFILE_TRACE(10);
}

void IOHIDSystem::_postMouseMoveEvent(int          what,
                                     AbsoluteTime  ts,
                                     OSObject *    sender)
{
    NXEventData data;
    CachedMouseEventStruct *cachedMouseEvent = 0;
    PROFILE_TRACE(11);

    bzero( &data, sizeof(data) );
    data.mouseMove.dx = _cursorHelper.desktopLocationDelta().xValue().as32();
    data.mouseMove.dy = _cursorHelper.desktopLocationDelta().yValue().as32();
    data.mouseMove.subx = _cursorHelper.desktopLocation().xValue().fraction() >> 8;
    data.mouseMove.suby = _cursorHelper.desktopLocation().yValue().fraction() >> 8;

    _cursorHelper.desktopLocationDelta() = IOFixedPoint64();
    _cursorLog(AbsoluteTime_to_scalar(&ts));

    // vtn3: This will update the data structure for touch devices
    updateMouseMoveEventForSender(sender, &data);

    // RY: Roll in the tablet info we got from absolutePointerEventGated
    // into the mouseMove event.
    if (sender && (cachedMouseEvent = GetCachedMouseEventForService(cachedButtonStates, sender)))
    {
        if (data.mouse.subType != NX_SUBTYPE_MOUSE_TOUCH)
            data.mouseMove.subType = cachedMouseEvent->subType;
        data.mouseMove.reserved1 = cachedMouseEvent->lastPressure;

        if (cachedMouseEvent->subType == NX_SUBTYPE_TABLET_PROXIMITY)
        {
            bcopy(&(cachedMouseEvent->proximityData), &(data.mouseMove.tablet.proximity), sizeof(NXTabletProximityData));
        }
        else if (cachedMouseEvent->subType == NX_SUBTYPE_TABLET_POINT)
        {
            bcopy(&(cachedMouseEvent->tabletData), &(data.mouseMove.tablet.point), sizeof(NXTabletPointData));
        }
    }

    postEvent(what, &_cursorHelper.desktopLocation(), ts, &data, sender);
    PROFILE_TRACE(12);
}

/**
 ** IOUserClient methods
 **/

IOReturn IOHIDSystem::newUserClient(task_t         owningTask,
                    /* withToken */ void *         security_id,
                    /* ofType */    UInt32         type,
                    /* withProps*/  OSDictionary *  properties,
                    /* client */    IOUserClient ** handler)
{
    IOHIDCmdGateActionArgs args;

    args.arg0 = &owningTask;
    args.arg1 = security_id;
    args.arg2 = &type;
    args.arg3 = properties;
    args.arg4 = handler;

    return cmdGate->runAction((IOCommandGate::Action)doNewUserClient, &args);
}

IOReturn IOHIDSystem::doNewUserClient(IOHIDSystem *self, void * args)
                        /* IOCommandGate::Action */
{
    task_t         owningTask   = *(task_t *) ((IOHIDCmdGateActionArgs *)args)->arg0;
    void *         security_id  = ((IOHIDCmdGateActionArgs *)args)->arg1;
    UInt32         type         = *(UInt32 *) ((IOHIDCmdGateActionArgs *)args)->arg2;
    OSDictionary * properties   = (OSDictionary *) ((IOHIDCmdGateActionArgs *)args)->arg3;
    IOUserClient ** handler     = (IOUserClient **) ((IOHIDCmdGateActionArgs *)args)->arg4;

    return self->newUserClientGated(owningTask, security_id, type, properties, handler);
}

IOReturn IOHIDSystem::newUserClientGated(task_t    owningTask,
                    /* withToken */ void *         security_id,
                    /* ofType */    UInt32         type,
                    /* withProps*/  OSDictionary *  properties,
                    /* client */    IOUserClient ** handler)
{
    IOUserClient * newConnect = 0;
    IOReturn  err = kIOReturnNoMemory;

    do {
        if ( type == kIOHIDParamConnectType) {
            if ( paramConnect) {
                newConnect = paramConnect;
                newConnect->retain();
            }
            else if ( eventsOpen) {
                newConnect = new IOHIDParamUserClient;
            }
            else {
                err = kIOReturnNotOpen;
                continue;
            }

        }
        else if ( type == kIOHIDServerConnectType) {
            newConnect = new IOHIDUserClient;
        }
        else if ( type == kIOHIDStackShotConnectType ) {
            newConnect = new IOHIDStackShotUserClient;
        }
        else if ( type == kIOHIDEventSystemConnectType ) {
            newConnect = new IOHIDEventSystemUserClient;
        }
        else {
            err = kIOReturnUnsupported;
        }

        if ( !newConnect) {
            continue;
        }

        // initialization is getting out of hand

        if ( (newConnect != paramConnect) && (
                    (false == newConnect->initWithTask(owningTask, security_id, type, properties))
                    || (false == newConnect->setProperty(kIOUserClientCrossEndianCompatibleKey, kOSBooleanTrue))
                    || (false == newConnect->attach( this ))
                    || (false == newConnect->start( this ))
                    || ((type == kIOHIDServerConnectType)
                        && (err = evOpen()))
                )) {
            newConnect->detach( this );
            newConnect->release();
            newConnect = 0;
            continue;
        }
        if ( type == kIOHIDParamConnectType)
            paramConnect = newConnect;

        err = kIOReturnSuccess;

    }
    while( false );

#ifdef DEBUG
    int             pid = -1;
    proc_t          p = (proc_t)get_bsdtask_info(owningTask);
    pid = proc_pid(p);
    IOLog("%s (%d) %s returned %p\n", __func__, pid,
          type == kIOHIDParamConnectType ? "IOHIDParamUserClient" :
          type == kIOHIDServerConnectType ? "IOHIDUserClient" :
          type == kIOHIDStackShotConnectType ? "IOHIDStackShotUserClient" :
          type == kIOHIDEventSystemConnectType ? "IOHIDEventSystemUserClient" :
          "kIOReturnUnsupported",
          newConnect);
#endif

    IOHID_DEBUG(kIOHIDDebugCode_NewUserClient, type, err, newConnect, 0);
    *handler = newConnect;
    return err;
}


IOReturn IOHIDSystem::setEventsEnable(void*p1,void*,void*,void*,void*,void*)
{                                                                    // IOMethod
    IOReturn ret;

    if (mac_iokit_check_hid_control(kauth_cred_get()))
        return kIOReturnNotPermitted;

    ret = cmdGate->runAction((IOCommandGate::Action)doSetEventsEnablePre, p1);
    if ( ret == kIOReturnSuccess ) {
        // reset outside gated context
        _resetMouseParameters();
    }
    ret = cmdGate->runAction((IOCommandGate::Action)doSetEventsEnablePost, p1);

    return ret;
}

IOReturn IOHIDSystem::doSetEventsEnablePre(IOHIDSystem *self, void *p1)
                        /* IOCommandGate::Action */
{
    return self->setEventsEnablePreGated(p1);
}

IOReturn IOHIDSystem::setEventsEnablePreGated(void*p1)
{
    bool enable = (bool)p1;

    if( enable) {
        while ( evStateChanging )
            cmdGate->commandSleep(&evStateChanging);

        evStateChanging = true;
        attachDefaultEventSources();
    }
    return( kIOReturnSuccess);
}

IOReturn IOHIDSystem::doSetEventsEnablePost(IOHIDSystem *self, void *p1)
                        /* IOCommandGate::Action */
{
    return self->setEventsEnablePostGated(p1);
}

IOReturn IOHIDSystem::setEventsEnablePostGated(void*p1)
{
    bool enable = (bool)p1;

    if( enable) {
        evStateChanging = false;
        cmdGate->commandWakeup(&evStateChanging);
    }
    return( kIOReturnSuccess);
}


IOReturn IOHIDSystem::setCursorEnable(void*p1,void*,void*,void*,void*,void*)
{                                                                    // IOMethod
    if (mac_iokit_check_hid_control(kauth_cred_get()))
        return kIOReturnNotPermitted;

    return cmdGate->runAction((IOCommandGate::Action)doSetCursorEnable, p1);

}

IOReturn IOHIDSystem::doSetCursorEnable(IOHIDSystem *self, void * arg0)
                        /* IOCommandGate::Action */
{
    return self->setCursorEnableGated(arg0);
}

IOReturn IOHIDSystem::setCursorEnableGated(void* p1)
{
    bool        enable = (bool)(intptr_t)p1;

    if ( !eventsOpen ) {
        return kIOReturnNotOpen;
    }

    if( !screens) {
        return kIOReturnNoDevice;
    }

    if( enable ) {
        if( cursorStarted) {
            hideCursor();
            cursorEnabled = resetCursor();
            showCursor();
        }
        else {
            cursorEnabled = startCursor();
        }
    }
    else {
        cursorEnabled = enable;
    }

    if (cursorCoupled != cursorEnabled) {
        _periodicEventNext = kIOHIDSystenDistantFuture;
        _cursorMoveDelta = 0;
        _cursorHelper.desktopLocationPosting().fromIntFloor(0, 0);
        _cursorHelper.clearEventCounts();
        _cursorLogTimed();
        scheduleNextPeriodicEvent();
        cursorCoupled = cursorEnabled;
    }

    return kIOReturnSuccess;
}

IOReturn IOHIDSystem::setContinuousCursorEnable(void*p1,void*,void*,void*,void*,void*)
{                                                                    // IOMethod
    kprintf("%s called\n", __func__);
    
    if (mac_iokit_check_hid_control(kauth_cred_get()))
        return kIOReturnNotPermitted;

    return cmdGate->runAction((IOCommandGate::Action)doSetContinuousCursorEnable, p1);

}

IOReturn IOHIDSystem::doSetContinuousCursorEnable(IOHIDSystem *self, void * arg0)
                        /* IOCommandGate::Action */
{
    kprintf("%s called\n", __func__);
    
    return self->setContinuousCursorEnableGated(arg0);
}

IOReturn IOHIDSystem::setContinuousCursorEnableGated(void* p1)
{
    bool        enable = (bool)(intptr_t)p1;

    kprintf("%s called\n", __func__);
    
    if ( !eventsOpen ) {
        return kIOReturnNotOpen;
    }

    if( !screens) {
        return kIOReturnNoDevice;
    }

    if( enable )
        enableContinuousCursor();
    else
        disableContinuousCursor();

    return kIOReturnSuccess;
}

IOReturn IOHIDSystem::extSetBounds( IOGBounds * bounds )
{
    if (mac_iokit_check_hid_control(kauth_cred_get()))
        return kIOReturnNotPermitted;

    if( bounds->minx != bounds->maxx) {
        cursorPin = *bounds;
        cursorPinned = true;
    } else
        cursorPinned = false;

    return( kIOReturnSuccess );
}

IOReturn IOHIDSystem::extPostEvent(void*p1,void*p2,void*,void*,void*,void*)
{                                                                    // IOMethod
    AbsoluteTime    ts;

    clock_get_uptime(&ts);

    return cmdGate->runAction((IOCommandGate::Action)doExtPostEvent, p1, p2, &ts);
}

IOReturn IOHIDSystem::doExtPostEvent(IOHIDSystem *self, void * arg0, void * arg1, void * arg2, void * arg3 __unused)
                        /* IOCommandGate::Action */
{
    return self->extPostEventGated(arg0, arg1, arg2);
}

IOReturn IOHIDSystem::extPostEventGated(void *p1,void *p2 __unused, void *p3)
{
    struct evioLLEvent * event      = (struct evioLLEvent *)p1;
    bool        isMoveOrDragEvent   = false;
    bool        isSeized            = false;
    int         oldMovedMask        = 0;
    UInt32      buttonState         = 0;
    UInt32      newFlags            = 0;
    AbsoluteTime ts                 = *(AbsoluteTime *)p3;
    CachedMouseEventStruct *cachedMouseEvent = NULL;
    UInt32      typeMask            = EventCodeMask(event->type);

    // rdar://problem/8689199
    int         extPID = proc_selfpid();

    IOHID_DEBUG(kIOHIDDebugCode_ExtPostEvent, event->type, *(UInt32*)&(event->location), event->setFlags, event->flags);


    if (event->type != NX_NULLEVENT && mac_iokit_check_hid_control(kauth_cred_get()))
        return kIOReturnNotPermitted;

    if ( eventsOpen == false )
        return kIOReturnNotOpen;

    if (ShouldConsumeHIDEvent(ts, rootDomainStateChangeDeadline, false)) {
        if (typeMask & NX_WAKEMASK) {
            TICKLE_DISPLAY(event->type);
        }
        return kIOReturnSuccess;
    }

    if (!DISPLAY_IS_ENABLED) {
#if !WAKE_DISPLAY_ON_MOVEMENT
        if ( (typeMask & NX_WAKEMASK) ||
           ((typeMask & MOVEDEVENTMASK) && (CMP_ABSOLUTETIME(&ts, &displaySleepWakeupDeadline) <= 0)) )
#endif
        {
            TICKLE_DISPLAY(event->type);
        }
        return kIOReturnSuccess;
    }

    TICKLE_DISPLAY(event->type);

    // used in set cursor below
    if (typeMask & MOVEDEVENTMASK)
    {
        isMoveOrDragEvent = true;

        // We have mouse move event without a specified pressure value and an embedded tablet event
        // We need to scale the tablet pressure to fit in mouseMove pressure
        if ((event->data.mouseMove.subType == NX_SUBTYPE_TABLET_POINT) && (event->data.mouseMove.reserved1 == 0))
        {
            event->data.mouseMove.reserved1 = ScalePressure(event->data.mouseMove.tablet.point.pressure);
        }
    }
    // We have mouse event without a specified pressure value and an embedded tablet event
    // We need to scale the tablet pressure to fit in mouse pressure
    else if ((typeMask & MOUSEEVENTMASK) &&
            (event->data.mouse.subType == NX_SUBTYPE_TABLET_POINT) && (event->data.mouse.pressure == 0))
    {
        event->data.mouse.pressure = ScalePressure(event->data.mouse.tablet.point.pressure);
    }

    if( event->setCursor)
    {
        // hack: clear evg->movedMask so setCursorPosition will not post
        // mouse moved events if this event is itself a mouse move event
        // this will prevent double events
        if (isMoveOrDragEvent)
        {
            oldMovedMask = evg->movedMask;
            evg->movedMask = 0;
        }

        if (( event->type == NX_MOUSEMOVED ) &&
            ( event->setCursor & kIOHIDSetRelativeCursorPosition ))
        {
            IOFixedPoint32 move;
            move.x = (event->data.mouseMove.dx * 256);
            move.y = (event->data.mouseMove.dy * 256);
            _cursorHelper.desktopLocation() += move;
            _cursorHelper.desktopLocationDelta() += move;

            _cursorLog(AbsoluteTime_to_scalar(&ts));

            clock_get_uptime(&_cursorEventLast);
            _setCursorPosition();
            _cursorMoveLast = _cursorEventLast;
            // scheduleNextPeriodicEvent() happens at the end.
        }
        else if ( event->setCursor & kIOHIDSetCursorPosition )
        {
            setCursorPosition(&event->location, false);
        }

        // other side of hack
        if (isMoveOrDragEvent)
            evg->movedMask = oldMovedMask;
    }
    // RY: This event is not moving the cursor, but we should
    // still determine if we need to click the location
    else
    {
        UInt32 newScreens = 0;
        EvScreen *screen = (EvScreen *)evScreen;

        if (!cursorPinned) {
            /* Get mask of screens on which the cursor is present */
            for (int i = 0; i < screens; i++ ) {
                if (!screen[i].desktopBounds)
                    continue;
                if ((screen[i].desktopBounds->maxx - screen[i].desktopBounds->minx) < 128)
                    continue;
                if (_cursorHelper.desktopLocation().inRect(*screen[i].desktopBounds)) {
                    newScreens |= (1 << i);
                }
            }
        }

        if (newScreens == 0)
        {
            /* At this point cursor has gone off all screens,
                    just clip it to one of the previous screens. */
            event->location.x = (event->location.x < cursorPin.minx) ?
                cursorPin.minx : ((event->location.x > cursorPin.maxx) ?
                cursorPin.maxx : event->location.x);
            event->location.y = (event->location.y < cursorPin.miny) ?
                cursorPin.miny : ((event->location.y > cursorPin.maxy) ?
                cursorPin.maxy : event->location.y);
        }
    }

    if ((typeMask & (NX_LMOUSEDOWNMASK | NX_RMOUSEDOWNMASK | NX_LMOUSEUPMASK | NX_RMOUSEUPMASK)) ||
        ((event->type == NX_SYSDEFINED) && (event->data.compound.subType == NX_SUBTYPE_AUX_MOUSE_BUTTONS)))
    {
        cachedMouseEvent = GetCachedMouseEventForService(cachedButtonStates, 0);
        if (cachedMouseEvent)
        {
            buttonState = cachedMouseEvent->lastButtons;
            switch ( event->type )
            {
                case NX_LMOUSEDOWN:
                    buttonState |= EV_LB;
                    break;
                case NX_RMOUSEDOWN:
                    buttonState |= EV_RB;
                    break;
                case NX_LMOUSEUP:
                    buttonState &= ~EV_LB;
                    break;
                case NX_RMOUSEUP:
                    buttonState &= ~EV_RB;
                    break;
                case NX_SYSDEFINED:
                    CONVERT_HW_TO_WV_BUTTONS(event->data.compound.misc.L[1], buttonState);
            }
            cachedMouseEvent->lastButtons = buttonState;

            evg->buttons = GetCachedMouseButtonStates(cachedButtonStates);
        }
    }

    if( event->setFlags & kIOHIDSetGlobalEventFlags)
    {
        newFlags = evg->eventFlags = (evg->eventFlags & ~KEYBOARD_FLAGSMASK)
                        | (event->flags & KEYBOARD_FLAGSMASK);
    }

    if ( event->setFlags & kIOHIDPostHIDManagerEvent )
    {
        if ((typeMask & (MOUSEEVENTMASK | MOVEDEVENTMASK | NX_SCROLLWHEELMOVEDMASK)) &&
            (_hidPointingDevice || (_hidPointingDevice = IOHIDPointingDevice::newPointingDeviceAndStart(this, 8, 400, true, 2))))
        {
            SInt32  dx = 0;
            SInt32  dy = 0;
            SInt32  wheel = 0;
            buttonState = 0;

            if (typeMask & MOVEDEVENTMASK)
            {
                dx = event->data.mouseMove.dx;
                dy = event->data.mouseMove.dy;
            }
            else if ( event->type == NX_SCROLLWHEELMOVED )
            {
                wheel = event->data.scrollWheel.deltaAxis1;
            }

            // Button state should have already been taken care of by above.
            if (cachedMouseEvent ||
                (NULL != (cachedMouseEvent = GetCachedMouseEventForService(cachedButtonStates, 0))))
                CONVERT_EV_TO_HW_BUTTONS((cachedMouseEvent->lastButtons), buttonState);

            _hidPointingDevice->postMouseEvent(buttonState, dx, dy, wheel);
            isSeized |= _hidPointingDevice->isSeized();
        }

        if ((typeMask & (NX_KEYDOWNMASK | NX_KEYUPMASK | NX_FLAGSCHANGEDMASK)) &&
            (_hidKeyboardDevice || (_hidKeyboardDevice = IOHIDKeyboardDevice::newKeyboardDeviceAndStart(this, 1))))
        {
            _hidKeyboardDevice->postFlagKeyboardEvent(newFlags & KEYBOARD_FLAGSMASK);

            if ((event->type != NX_FLAGSCHANGED) && (event->data.key.repeat == 0))
            {
                _hidKeyboardDevice->postKeyboardEvent(event->data.key.keyCode, (event->type == NX_KEYDOWN));
            }

            isSeized |= _hidKeyboardDevice->isSeized();
        }
    }

    if ( !isSeized )
    {
        postEvent(             event->type,
                /* at */       &_cursorHelper.desktopLocation(),
                /* atTime */   ts,
                /* withData */ &event->data,
                /* sender */   0,
                /* extPID */   extPID);
    }

    scheduleNextPeriodicEvent();

    return kIOReturnSuccess;
}


IOReturn IOHIDSystem::extSetMouseLocation(void*p1,void*p2,void*,void*,void*,void*)
{                                                                    // IOMethod
    if (mac_iokit_check_hid_control(kauth_cred_get()))
        return kIOReturnNotPermitted;
    if ((sizeof(int32_t)*3) != (intptr_t)p2) {
        IOLog("IOHIDSystem::extSetMouseLocation called with inappropriate data size: %d\n", (int)(intptr_t)p2);
        return kIOReturnBadArgument;
    }

    return cmdGate->runAction((IOCommandGate::Action)doExtSetMouseLocation, p1);
}

IOReturn IOHIDSystem::doExtSetMouseLocation(IOHIDSystem *self, void * arg0)
                        /* IOCommandGate::Action */
{
    return self->extSetMouseLocationGated(arg0);
}

IOReturn IOHIDSystem::extSetMouseLocationGated(void *p1)
{
    IOFixedPoint32 * loc = (IOFixedPoint32 *)p1;

    IOHID_DEBUG(kIOHIDDebugCode_ExtSetLocation, loc ? loc->x : 0, loc ? loc->y : 0, loc, 0);

    //    setCursorPosition(loc, true);
    if ( eventsOpen == true )
    {
        _cursorHelper.desktopLocationDelta() += *loc;
        _cursorHelper.desktopLocationDelta() -= _cursorHelper.desktopLocation();
        _cursorHelper.desktopLocation() = *loc;
        _setCursorPosition(true);
    }

    return kIOReturnSuccess;
}

IOReturn IOHIDSystem::extGetStateForSelector(void*p1,void*p2,void*,void*,void*,void*)
{                                                                    // IOMethod
    return cmdGate->runAction((IOCommandGate::Action)doExtGetStateForSelector, p1, p2);
}

IOReturn IOHIDSystem::extSetStateForSelector(void*p1,void*p2,void*,void*,void*,void*)
{                                                                    // IOMethod
    if (mac_iokit_check_hid_control(kauth_cred_get()))
        return kIOReturnNotPermitted;

    return cmdGate->runAction((IOCommandGate::Action)doExtSetStateForSelector, p1, p2);
}

IOReturn IOHIDSystem::doExtGetStateForSelector(IOHIDSystem *self, void *p1, void *p2)
/* IOCommandGate::Action */
{
    IOReturn result = kIOReturnSuccess;
    unsigned int selector = (uintptr_t)p1;
    unsigned int *state_O = (unsigned int*)p2;
    switch (selector) {
        case kIOHIDCapsLockState:
            result = self->getCapsLockState(state_O);
            break;

        case kIOHIDNumLockState:
            result = self->getNumLockState(state_O);
            break;
            
        case kIOHIDActivityUserIdle:
            *state_O = self->_privateData->hidActivityIdle ? 1 : 0;
            break;
            
        case kIOHIDActivityDisplayOn:
            *state_O = self->displayState & IOPMDeviceUsable ? 1 : 0;
            break;
            
        default:
            IOLog("IOHIDSystem::doExtGetStateForSelector recieved unexpected selector: %d\n", selector);
            result = kIOReturnBadArgument;
            break;
    }
    return result;
}

IOReturn IOHIDSystem::doExtSetStateForSelector(IOHIDSystem *self, void *p1, void *p2)
/* IOCommandGate::Action */
{
    IOReturn result = kIOReturnSuccess;
    unsigned int selector = (uintptr_t)p1;
    unsigned int state_I = (uintptr_t)p2;
    
    switch (selector) {
        case kIOHIDCapsLockState:
            result = self->setCapsLockState(state_I);
            break;
            
        case kIOHIDNumLockState:
            result = self->setNumLockState(state_I);
            break;
            
        case kIOHIDActivityUserIdle:    // not settable
        case kIOHIDActivityDisplayOn:   // not settable
        default:
            IOLog("IOHIDSystem::doExtGetStateForSelector recieved unexpected selector: %d\n", selector);
            result = kIOReturnBadArgument;
            break;
            
    }
    return result;
}

IOReturn IOHIDSystem::getCapsLockState(unsigned int *state_O)
{
    IOReturn retVal = kIOReturnNoDevice;
    *state_O = false;
    OSIterator *itr = getProviderIterator();
    if (itr) {
        bool done = false;
        while (!done) {
            OSObject *provider;
            while (!done && (NULL != (provider = itr->getNextObject()))) {
                IOHIDKeyboard *keyboard = OSDynamicCast(IOHIDKeyboard, provider);
                if (keyboard) {
                    retVal = kIOReturnSuccess;
                    if (keyboard->alphaLock()) {
                        *state_O = true;
                        done = true;
                    }
                }
            }
            if (itr->isValid()) {
                done = true;
            }
            else {
                itr->reset();
            }
        }
        itr->release();
    }
    return retVal;
}

IOReturn IOHIDSystem::setCapsLockState(unsigned int state_I)
{
    IOReturn retVal = kIOReturnNoDevice;
    OSIterator *itr = getProviderIterator();
    if (itr) {
        bool done = false;
        while (!done) {
            OSObject *provider;
            while (!done && (NULL != (provider = itr->getNextObject()))) {
                IOHIDKeyboard *keyboard = OSDynamicCast(IOHIDKeyboard, provider);
                if (keyboard) {
                    if ((state_I && !keyboard->alphaLock()) || (!state_I && keyboard->alphaLock())) {
                        AbsoluteTime timeStamp;
                        UInt32 opts = (1<<31) /* kDelayedOption */;
                        clock_get_uptime(&timeStamp);

                        keyboard->dispatchKeyboardEvent(timeStamp, kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardCapsLock, 1, opts);
                        keyboard->dispatchKeyboardEvent(timeStamp, kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardCapsLock, 0, opts);
                    }
                    retVal = kIOReturnSuccess;
                }
            }
            if (itr->isValid()) {
                done = true;
            }
            else {
                itr->reset();
            }
        }
        itr->release();
    }
    return retVal;
}

IOReturn IOHIDSystem::getNumLockState(unsigned int *state_O)
{
    IOReturn retVal = kIOReturnNoDevice;
    *state_O = false;
    OSIterator *itr = getProviderIterator();
    if (itr) {
        bool done = false;
        while (!done) {
            OSObject *provider;
            while (!done && (NULL != (provider = itr->getNextObject()))) {
                IOHIDKeyboard *keyboard = OSDynamicCast(IOHIDKeyboard, provider);
                if (keyboard) {
                    retVal = kIOReturnSuccess;
                    if (keyboard->numLock()) {
                        *state_O = true;
                        done = true;
                    }
                }
            }
            if (itr->isValid()) {
                done = true;
            }
            else {
                itr->reset();
            }
        }
        itr->release();
    }
    return retVal;
}

IOReturn IOHIDSystem::setNumLockState(unsigned int state_I)
{
    IOReturn retVal = kIOReturnNoDevice;
    OSIterator *itr = getProviderIterator();
    if (itr) {
        bool done = false;
        while (!done) {
            OSObject *provider;
            while (!done && (NULL != (provider = itr->getNextObject()))) {
                IOHIDKeyboard *keyboard = OSDynamicCast(IOHIDKeyboard, provider);
                if (keyboard) {
                    if ((state_I && !keyboard->numLock()) || (!state_I && keyboard->numLock())) {
                        AbsoluteTime timeStamp;
                        UInt32 opts = 0;
                        clock_get_uptime(&timeStamp);

                        keyboard->dispatchKeyboardEvent(timeStamp, kHIDPage_AppleVendorTopCase, kHIDUsage_AV_TopCase_KeyboardFn, 1, opts);
                        keyboard->dispatchKeyboardEvent(timeStamp, kHIDPage_AppleVendorTopCase, kHIDUsage_AV_TopCase_KeyboardFn, 0, opts);
                    }
                    retVal = kIOReturnSuccess;
                }
            }
            if (itr->isValid()) {
                done = true;
            }
            else {
                itr->reset();
            }
        }
        itr->release();
    }
    return retVal;
}

IOReturn IOHIDSystem::extGetButtonEventNum(void*p1,void*p2,void*,void*,void*,void*)
{                                                                   // IOMethod
    return cmdGate->runAction((IOCommandGate::Action)doExtGetButtonEventNum, p1, p2);
}

IOReturn IOHIDSystem::doExtGetButtonEventNum(IOHIDSystem *self, void * arg0, void * arg1)
                        /* IOCommandGate::Action */
{
    return self->extGetButtonEventNumGated(arg0, arg1);
}

IOReturn IOHIDSystem::extGetButtonEventNumGated(void *p1, void* p2)
{
    NXMouseButton button   = (NXMouseButton)(uintptr_t)p1;
    int *         eventNum = (int *)p2;
    IOReturn      err      = kIOReturnSuccess;

    switch( button) {
    case NX_LeftButton:
            *eventNum = leftENum;
        break;
    case NX_RightButton:
            *eventNum = rightENum;
        break;
    default:
        err = kIOReturnBadArgument;
    }

    return err;
}

void IOHIDSystem::makeNumberParamProperty( OSDictionary * dict, const char * key,
                            unsigned long long number, unsigned int bits )
{
    OSNumber *  numberRef;
    numberRef = OSNumber::withNumber(number, bits);

    if( numberRef) {
        dict->setObject( key, numberRef);
        numberRef->release();
    }
}

void IOHIDSystem::makeInt32ArrayParamProperty( OSDictionary * dict, const char * key,
                            UInt32 * intArray, unsigned int count )
{
    OSArray *   array;
    OSNumber *  number;

    array = OSArray::withCapacity(count);

    if ( !array )
        return;

    for (unsigned i=0; i<count; i++)
    {
        number = OSNumber::withNumber(intArray[i], sizeof(UInt32) << 3);
        if (number)
        {
            array->setObject(number);
            number->release();
        }
    }

    dict->setObject( key, array);
    array->release();
}

void IOHIDSystem::createParameters( void )
{
    UInt64  nano;
    IOFixed fixed;
    UInt32  int32;

    savedParameters->setObject(kIOHIDDefaultParametersKey, kOSBooleanTrue);

    nano = EV_DCLICKTIME;
    makeNumberParamProperty( savedParameters, kIOHIDClickTimeKey,
                nano, 64 );

    UInt32  tempClickSpace[] = {clickSpaceThresh.x, clickSpaceThresh.y};
    makeInt32ArrayParamProperty( savedParameters, kIOHIDClickSpaceKey,
                tempClickSpace, sizeof(tempClickSpace)/sizeof(UInt32) );

    nano = EV_DEFAULTKEYREPEAT;
    makeNumberParamProperty( savedParameters, kIOHIDKeyRepeatKey,
                nano, 64 );
    nano = EV_DEFAULTINITIALREPEAT;
    makeNumberParamProperty( savedParameters, kIOHIDInitialKeyRepeatKey,
                nano, 64 );

    fixed = EV_DEFAULTPOINTERACCELLEVEL;
    makeNumberParamProperty( savedParameters, kIOHIDPointerAccelerationKey,
                fixed, sizeof(fixed) << 3);

    fixed = EV_DEFAULTSCROLLACCELLEVEL;
    makeNumberParamProperty( savedParameters, kIOHIDScrollAccelerationKey,
                fixed, sizeof(fixed) << 3);

    fixed = kIOHIDButtonMode_EnableRightClick;
    makeNumberParamProperty( savedParameters, kIOHIDPointerButtonMode,
                fixed, sizeof(fixed) << 3);

    // set eject delay properties
    int32 = kEjectF12DelayMS;
    makeNumberParamProperty( savedParameters, kIOHIDF12EjectDelayKey,
                            int32, 32 );
    int32 = kEjectKeyDelayMS;
    makeNumberParamProperty( savedParameters, kIOHIDKeyboardEjectDelay,
                            int32, 32 );

    // set slow keys delay property
    int32 = 0;
    makeNumberParamProperty( savedParameters, kIOHIDSlowKeysDelayKey,
                int32, 32 );

    // set disabled property
    int32 = 0; // not disabled
    makeNumberParamProperty( savedParameters, kIOHIDStickyKeysDisabledKey,
                int32, 32 );

    // set on/off property
    int32 = 0; // off
    makeNumberParamProperty( savedParameters, kIOHIDStickyKeysOnKey,
                int32, 32 );

    // set shift toggles property
    int32 = 0; // off, shift does not toggle
    makeNumberParamProperty( savedParameters, kIOHIDStickyKeysShiftTogglesKey,
                int32, 32 );

    // set option toggles property
    int32 = 0; // off, option does not toggle
    makeNumberParamProperty( savedParameters, kIOHIDMouseKeysOptionTogglesKey,
                int32, 32 );

    int32 = 0;
    makeNumberParamProperty( savedParameters, kIOHIDFKeyModeKey,
                            int32, 32 );

    setProperty( kIOHIDParametersKey, savedParameters );
    savedParameters->release();

    // RY: Set up idleTimeSerializer.  This should generate the
    // current idle time when requested.
    OSSerializer * idleTimeSerializer = OSSerializer::forTarget(this, IOHIDSystem::_idleTimeSerializerCallback);

    if (idleTimeSerializer)
    {
        setProperty( kIOHIDIdleTimeKey, idleTimeSerializer);
        idleTimeSerializer->release();
    }

#if 0
    OSSerializer * displaySerializer = OSSerializer::forTarget(this, IOHIDSystem::_displaySerializerCallback);

    if (displaySerializer)
    {
        setProperty("DisplayState", displaySerializer);
        displaySerializer->release();
    }
#endif
}

bool IOHIDSystem::_idleTimeSerializerCallback(void * target, void * ref __unused, OSSerialize *s)
{
    IOHIDSystem *   self = (IOHIDSystem *) target;
    AbsoluteTime    currentTime;
    OSNumber *      number;
    UInt64          idleTimeNano = 0;
    bool            retValue = false;

    if( self->eventsOpen )
    {
        clock_get_uptime( &currentTime);
        SUB_ABSOLUTETIME( &currentTime, &(self->lastUndimEvent));
        absolutetime_to_nanoseconds( currentTime, &idleTimeNano);
    }

    number = OSNumber::withNumber(idleTimeNano, 64);
    if (number)
    {
        retValue = number->serialize( s );
        number->release();
    }

    return retValue;
}

bool IOHIDSystem::_displaySerializerCallback(void * target, void * ref __unused, OSSerialize *s)
{
    IOHIDSystem     *self = (IOHIDSystem *) target;
    bool            retValue = false;
    OSDictionary    *mainDict = OSDictionary::withCapacity(4);
    require(mainDict, exit_early);

#define IfNotNullAddNumToDictWithKey(x,y, w,z) \
    if (x) { \
        OSNumber *num = NULL; \
        num = OSNumber::withNumber(y, 8*sizeof(y)); \
        if (num) { \
            w->setObject(z, num); \
            num->release(); \
        } \
    }

    for(int i = 0; i < self->screens; i++) {
        EvScreen &esp = ((EvScreen*)(self->evScreen))[i];
        OSDictionary    *thisDisplay = OSDictionary::withCapacity(4);
        char            key[256];

        require(thisDisplay, next_display);
        snprintf(key, sizeof(key), "%d", i);
        mainDict->setObject(key, thisDisplay);

        IfNotNullAddNumToDictWithKey(esp.instance, esp.instance->getRegistryEntryID(), thisDisplay, "io_fb_id");
        IfNotNullAddNumToDictWithKey(esp.displayBounds, esp.displayBounds->minx, thisDisplay, "disp_min_x");
        IfNotNullAddNumToDictWithKey(esp.displayBounds, esp.displayBounds->maxx, thisDisplay, "disp_max_x");
        IfNotNullAddNumToDictWithKey(esp.displayBounds, esp.displayBounds->miny, thisDisplay, "disp_min_y");
        IfNotNullAddNumToDictWithKey(esp.displayBounds, esp.displayBounds->maxy, thisDisplay, "disp_max_y");
        IfNotNullAddNumToDictWithKey(esp.desktopBounds, esp.desktopBounds->minx, thisDisplay, "desk_min_x");
        IfNotNullAddNumToDictWithKey(esp.desktopBounds, esp.desktopBounds->maxx, thisDisplay, "desk_max_x");
        IfNotNullAddNumToDictWithKey(esp.desktopBounds, esp.desktopBounds->miny, thisDisplay, "desk_min_y");
        IfNotNullAddNumToDictWithKey(esp.desktopBounds, esp.desktopBounds->maxy, thisDisplay, "desk_max_y");
        IfNotNullAddNumToDictWithKey(esp.creator_pid, esp.creator_pid, thisDisplay, "creator_pid");

    next_display:
        OSSafeReleaseNULL(thisDisplay);
    }

    {   // safety scoping
        OSDictionary    *workSpaceDict = OSDictionary::withCapacity(4);
        IfNotNullAddNumToDictWithKey(workSpaceDict, self->workSpace.minx, workSpaceDict, "minx");
        IfNotNullAddNumToDictWithKey(workSpaceDict, self->workSpace.miny, workSpaceDict, "miny");
        IfNotNullAddNumToDictWithKey(workSpaceDict, self->workSpace.maxx, workSpaceDict, "maxx");
        IfNotNullAddNumToDictWithKey(workSpaceDict, self->workSpace.maxy, workSpaceDict, "maxy");
        mainDict->setObject("workspace", workSpaceDict ? (OSObject*)workSpaceDict : (OSObject*)kOSBooleanFalse);
        OSSafeReleaseNULL(workSpaceDict);
    }

    {   // safety scoping
        OSDictionary    *cursorPinDict = OSDictionary::withCapacity(4);
        IfNotNullAddNumToDictWithKey(cursorPinDict, self->cursorPin.minx, cursorPinDict, "minx");
        IfNotNullAddNumToDictWithKey(cursorPinDict, self->cursorPin.miny, cursorPinDict, "miny");
        IfNotNullAddNumToDictWithKey(cursorPinDict, self->cursorPin.maxx, cursorPinDict, "maxx");
        IfNotNullAddNumToDictWithKey(cursorPinDict, self->cursorPin.maxy, cursorPinDict, "maxy");
        mainDict->setObject("cursorPin", cursorPinDict ? (OSObject*)cursorPinDict : (OSObject*)kOSBooleanFalse);
        OSSafeReleaseNULL(cursorPinDict);
    }

    retValue = mainDict->serialize( s );

exit_early:
    OSSafeReleaseNULL(mainDict);
    return retValue;
}

IOReturn IOHIDSystem::setProperties( OSObject * properties )
{
    OSDictionary *  dict;
    IOReturn        err = kIOReturnSuccess;
    IOReturn        ret;

    dict = OSDynamicCast( OSDictionary, properties );
    if( dict) {
        if (dict->getObject(kIOHIDUseKeyswitchKey) &&
            ( IOUserClient::clientHasPrivilege(current_task(), kIOClientPrivilegeAdministrator) != kIOReturnSuccess)) {
            dict->removeObject(kIOHIDUseKeyswitchKey);
        }
        ret = setParamProperties( dict );
    }
    else
    err = kIOReturnBadArgument;

    return( err );
}

IOReturn IOHIDSystem::setParamProperties( OSDictionary * dict )
{
    OSIterator *    iter    = NULL;
    IOReturn        ret     = kIOReturnSuccess;
    IOReturn        err     = kIOReturnSuccess;

    // Tip off devices that these are default parameters
    dict->setObject(kIOHIDDefaultParametersKey, kOSBooleanTrue);

    ret = cmdGate->runAction((IOCommandGate::Action)doSetParamPropertiesPre, dict, &iter);

    if ( ret == kIOReturnSuccess ) {

        // Do the following down calls outside of the gate
        if( iter) {
            IOService *     eventSrc;
            OSDictionary *  validParameters;
            while( (eventSrc = (IOService *) iter->getNextObject())) {

                if ( OSDynamicCast(IOHIDKeyboard, eventSrc) || OSDynamicCast(IOHIDPointing, eventSrc) || OSDynamicCast(IOHIDConsumer, eventSrc))
                    continue;

                // Use valid parameters per device.  Basically if the IOHIDevice has a given property
                // in its registery we should NOT push it down via setParamProperties as properties
                // generated via the global IOHIDSystem::setParamProperties are defaults.
                validParameters = createFilteredParamPropertiesForService(eventSrc, dict);
                if ( validParameters ) {

                    if ( OSDynamicCast(IOHIDevice, eventSrc) )
                        ret = ((IOHIDevice *)eventSrc)->setParamProperties( validParameters );
                    else if ( OSDynamicCast( IOHIDEventService, eventSrc ) )
                        ret = ((IOHIDEventService *)eventSrc)->setSystemProperties( validParameters );

                    if( (ret != kIOReturnSuccess) && (ret != kIOReturnBadArgument))
                        err = ret;

                    dict->merge(validParameters);

                    validParameters->release();
                }
            }
            iter->release();
        }

        // Grab the gate again.
        cmdGate->runAction((IOCommandGate::Action)doSetParamPropertiesPost, dict);
    }

    return err;
}

OSDictionary * IOHIDSystem::createFilteredParamPropertiesForService(IOService * service, OSDictionary * dict)
{
    OSDictionary * validParameters = OSDictionary::withCapacity(4);

    if ( !validParameters )
        return NULL;

    OSDictionary * deviceParameters = NULL;

    if ( OSDynamicCast(IOHIDevice, service) ) {
        deviceParameters = (OSDictionary*)service->copyProperty(kIOHIDParametersKey);
    }
    else if ( OSDynamicCast( IOHIDEventService, service ) ) {
        deviceParameters = (OSDictionary*)service->copyProperty(kIOHIDEventServicePropertiesKey);
    }
    if (!OSDynamicCast(OSDictionary, deviceParameters))
        OSSafeReleaseNULL(deviceParameters);

    OSCollectionIterator * iterator = OSCollectionIterator::withCollection(dict);
    if ( iterator ) {
        bool done = false;
        while (!done) {
            OSSymbol * key = NULL;
            while ((key = (OSSymbol*)iterator->getNextObject()) != NULL) {
                if ( !deviceParameters || !deviceParameters->getObject(key) ) {
                    validParameters->setObject(key, dict->getObject(key));
                }
            }
            if (iterator->isValid()) {
                done = true;
            }
            else {
                iterator->reset();
                validParameters->flushCollection();
            }
        }
        iterator->release();
    }

    if ( validParameters->getCount() == 0 ) {
        validParameters->release();
        validParameters = NULL;
    }
    else {
        validParameters->setObject(kIOHIDDefaultParametersKey, kOSBooleanTrue);
    }

    return validParameters;
}


IOReturn IOHIDSystem::doSetParamPropertiesPre(IOHIDSystem *self, void * arg0, void * arg1)
                        /* IOCommandGate::Action */
{
    return self->setParamPropertiesPreGated((OSDictionary *)arg0, (OSIterator**)arg1);
}

IOReturn IOHIDSystem::setParamPropertiesPreGated( OSDictionary * dict, OSIterator ** pOpenIter)
{
    OSArray *   array;
    OSNumber *  number;

    // check for null
    if (dict == NULL)
        return kIOReturnError;

    // adding a pending flag here because we will be momentarily openning the gate
    // to make down calls into the client, before closing back up again to merge
    // the properties.
    while ( setParamPropertiesInProgress )
        cmdGate->commandSleep(&setParamPropertiesInProgress);

    setParamPropertiesInProgress = true;

    if( (number = OSDynamicCast( OSNumber, dict->getObject(kIOHIDUseKeyswitchKey))))
    {
        gUseKeyswitch = number->unsigned32BitValue();
    }

    if( (number = OSDynamicCast( OSNumber, dict->getObject(kIOHIDClickTimeKey))))
    {
        UInt64  nano = number->unsigned64BitValue();
        nanoseconds_to_absolutetime(nano, &clickTimeThresh);
    }

    // check the reset before setting the other parameters
    if (dict->getObject(kIOHIDScrollCountResetKey)) {
        _setScrollCountParameters();
    }

    _setScrollCountParameters(dict);

    if( (array = OSDynamicCast( OSArray,
        dict->getObject(kIOHIDClickSpaceKey)))) {

        if ((number = OSDynamicCast( OSNumber,
        array->getObject(EVSIOSCS_X))))
        {
            clickSpaceThresh.x = number->unsigned32BitValue();
        }
        if ((number = OSDynamicCast( OSNumber,
        array->getObject(EVSIOSCS_Y))))
        {
            clickSpaceThresh.y = number->unsigned32BitValue();
        }
    }

    if( (number = OSDynamicCast( OSNumber, dict->getObject(kIOHIDWaitCursorFrameIntervalKey)))) {
        uint32_t value = number->unsigned32BitValue();
        _cursorWaitDelta = value;
        if (_cursorWaitDelta < kTickScale) {
            _cursorWaitDelta = kTickScale;
        }
        scheduleNextPeriodicEvent();
    }

    if( (number = OSDynamicCast( OSNumber, dict->getObject(kIOHIDStickyKeysOnKey)))) {
        if (number->unsigned32BitValue())
            stickyKeysState |= (1 << 0);
        else
            stickyKeysState &= ~(1 << 0);

    }

    if( (number = OSDynamicCast( OSNumber, dict->getObject(kIOHIDStickyKeysShiftTogglesKey)))) {
        if (number->unsigned32BitValue())
            stickyKeysState |= (1 << 1);
        else
            stickyKeysState &= ~(1 << 1);

    }

    // save all params for new devices
    if ( dict->getObject(kIOHIDResetKeyboardKey) ) {
        UInt64 nano = EV_DEFAULTKEYREPEAT;
        makeNumberParamProperty( dict, kIOHIDKeyRepeatKey, nano, 64 );

        nano = EV_DEFAULTINITIALREPEAT;
        makeNumberParamProperty( dict, kIOHIDInitialKeyRepeatKey, nano, 64 );
    }

    if ( dict->getObject(kIOHIDResetPointerKey) ) {
        IOFixed fixed = EV_DEFAULTPOINTERACCELLEVEL;
        makeNumberParamProperty( dict, kIOHIDPointerAccelerationKey, fixed, sizeof(fixed) << 3);

        fixed = kIOHIDButtonMode_EnableRightClick;
        makeNumberParamProperty( dict, kIOHIDPointerButtonMode, fixed, sizeof(fixed) << 3);

        UInt64 nano = EV_DCLICKTIME;
        makeNumberParamProperty( dict, kIOHIDClickTimeKey, nano, 64 );
    }

    if ( dict->getObject(kIOHIDScrollResetKey) ) {
        IOFixed fixed = EV_DEFAULTSCROLLACCELLEVEL;
        makeNumberParamProperty( dict, kIOHIDScrollAccelerationKey, fixed, sizeof(fixed) << 3);
    }

    // update connected input devices
    if ( pOpenIter )
        *pOpenIter = getOpenProviderIterator();

    return kIOReturnSuccess;
}

void IOHIDSystem::_setScrollCountParameters(OSDictionary *newSettings)
{
    if (!newSettings) {
        newSettings = (OSDictionary*)copyProperty(kIOHIDScrollCountBootDefaultKey);
        if (!OSDynamicCast(OSDictionary, newSettings)) {
            newSettings->release();
            newSettings = NULL;
        }
    }
    else {
        newSettings->retain();
    }

    if (newSettings) {
        OSNumber *number = NULL;
        OSBoolean *boolean = NULL;
        if((number = OSDynamicCast(OSNumber, newSettings->getObject(kIOHIDScrollCountMinDeltaToStartKey))))
        {
            _scMinDeltaSqToStart = number->unsigned64BitValue();
            _scMinDeltaSqToStart *= _scMinDeltaSqToStart;
            if (_scMinDeltaSqToSustain == kDefaultMinimumDelta)
                _scMinDeltaSqToSustain = _scMinDeltaSqToStart;
            setProperty(kIOHIDScrollCountMinDeltaToStartKey, number);
        }

        if((number = OSDynamicCast(OSNumber, newSettings->getObject(kIOHIDScrollCountMinDeltaToSustainKey))))
        {
            _scMinDeltaSqToSustain = number->unsigned64BitValue();
            _scMinDeltaSqToSustain *= _scMinDeltaSqToSustain;
            if (_scMinDeltaSqToStart == kDefaultMinimumDelta)
                _scMinDeltaSqToStart = _scMinDeltaSqToSustain;
            setProperty(kIOHIDScrollCountMinDeltaToSustainKey, number);
        }

        if((number = OSDynamicCast(OSNumber, newSettings->getObject(kIOHIDScrollCountMaxTimeDeltaBetweenKey))))
        {
            UInt64 valueInMs = number->unsigned64BitValue();
            nanoseconds_to_absolutetime(valueInMs * kMillisecondScale, &_scMaxTimeDeltaBetween);
            if (!_scMaxTimeDeltaToSustain)
                _scMaxTimeDeltaToSustain = _scMaxTimeDeltaBetween;
            setProperty(kIOHIDScrollCountMaxTimeDeltaBetweenKey, number);
        }

        if((number = OSDynamicCast(OSNumber, newSettings->getObject(kIOHIDScrollCountMaxTimeDeltaToSustainKey))))
        {
            UInt64 valueInMs = number->unsigned64BitValue();
            nanoseconds_to_absolutetime(valueInMs * kMillisecondScale, &_scMaxTimeDeltaToSustain);
            if (!_scMaxTimeDeltaBetween)
                _scMaxTimeDeltaBetween = _scMaxTimeDeltaToSustain;
            setProperty(kIOHIDScrollCountMaxTimeDeltaToSustainKey, number);
        }

        if((boolean = OSDynamicCast(OSBoolean, newSettings->getObject(kIOHIDScrollCountIgnoreMomentumScrollsKey))))
        {
            _scIgnoreMomentum = (boolean == kOSBooleanTrue);
            setProperty(kIOHIDScrollCountIgnoreMomentumScrollsKey, boolean);
        }

        if((boolean = OSDynamicCast(OSBoolean, newSettings->getObject(kIOHIDScrollCountMouseCanResetKey))))
        {
            _scMouseCanReset = (boolean == kOSBooleanTrue);
            setProperty(kIOHIDScrollCountMouseCanResetKey, boolean);
        }

        if((number = OSDynamicCast(OSNumber, newSettings->getObject(kIOHIDScrollCountMaxKey))))
        {
            _scCountMax = number->unsigned16BitValue();
            setProperty(kIOHIDScrollCountMaxKey, number);
        }

        if((number = OSDynamicCast(OSNumber, newSettings->getObject(kIOHIDScrollCountAccelerationFactorKey))))
        {
            if (number->unsigned32BitValue() > 0) {
                _scAccelerationFactor.fromFixed(number->unsigned32BitValue());
                setProperty(kIOHIDScrollCountAccelerationFactorKey, number);
            }
        }

        if((boolean = OSDynamicCast(OSBoolean, newSettings->getObject(kIOHIDScrollCountZeroKey))))
        {
            if (boolean == kOSBooleanTrue) {
                log_scroll_state("Resetting _scCount on kIOHIDScrollCountZeroKey%s\n", "");
                _scCount = 0;
            }
        }

        newSettings->release();
    }
}

IOReturn IOHIDSystem::doSetParamPropertiesPost(IOHIDSystem *self, void * arg0)
                        /* IOCommandGate::Action */
{
    return self->setParamPropertiesPostGated((OSDictionary *)arg0);
}

IOReturn IOHIDSystem::setParamPropertiesPostGated( OSDictionary * dict)
{
    if ( dict->getObject(kIOHIDTemporaryParametersKey) == NULL ) {
        bool resetKeyboard  = dict->getObject(kIOHIDResetKeyboardKey) != NULL;
        bool resetPointer   = dict->getObject(kIOHIDResetPointerKey) != NULL;
        bool resetScroll    = dict->getObject(kIOHIDScrollResetKey) != NULL;

        OSDictionary * newParams = OSDictionary::withDictionary( savedParameters );
        if( newParams) {
            if ( resetKeyboard ) {
                dict->removeObject(kIOHIDResetKeyboardKey);
            }
            if ( resetPointer ) {
                dict->removeObject(kIOHIDResetPointerKey);
                newParams->removeObject(kIOHIDTrackpadAccelerationType);
                newParams->removeObject(kIOHIDMouseAccelerationType);
            }
            if ( resetScroll ) {
                dict->removeObject(kIOHIDScrollResetKey);
                newParams->removeObject(kIOHIDTrackpadScrollAccelerationKey);
                newParams->removeObject(kIOHIDMouseScrollAccelerationKey);
            }

            newParams->merge( dict );
            setProperty( kIOHIDParametersKey, newParams );
            newParams->release();
            savedParameters = newParams;
        }

        // Send anevent notification that the properties changed.
        struct evioLLEvent      event;
        AbsoluteTime        ts;
        // clear event record
        bzero( (void *)&event, sizeof event);

        event.data.compound.subType = NX_SUBTYPE_HIDPARAMETER_MODIFIED;
        clock_get_uptime(&ts);
        postEvent(NX_SYSDEFINED, &_cursorHelper.desktopLocation(), ts, &(event.data));

    }

    // Wake any pending setParamProperties commands.  They
    // still won't do much until we return out.
    setParamPropertiesInProgress = false;
    cmdGate->commandWakeup(&setParamPropertiesInProgress);

    return kIOReturnSuccess;
}

UInt8 IOHIDSystem::getSubtypeForSender(OSObject * sender)
{
    UInt8 subtype = NX_SUBTYPE_DEFAULT;
    if (touchEventPosters->containsObject(sender)) {
        subtype = NX_SUBTYPE_MOUSE_TOUCH;
    }
    return subtype;
}

void IOHIDSystem::updateMouseMoveEventForSender(OSObject * sender, NXEventData * evData)
{
    if (sender && evData) {
        evData->mouse.subType = getSubtypeForSender(sender);
    }
}

void IOHIDSystem::updateMouseEventForSender(OSObject * sender, NXEventData * evData)
{
    if (sender && evData) {
        evData->mouseMove.subType = getSubtypeForSender(sender);
    }
}

void IOHIDSystem::updateScrollEventForSender(OSObject * sender, NXEventData * evData)
{
    if (sender && evData) {
        if (NX_SUBTYPE_MOUSE_TOUCH == getSubtypeForSender(sender)) {
            evData->scrollWheel.reserved1 |= kScrollTypeTouch;
        }
    }
}

bool IOHIDSystem::attach( IOService * provider )
{
    IORegistryEntry *entry = provider;

    if (!super::attach(provider))   return false;
    while(entry) {
        if (kOSBooleanTrue == entry->getProperty("MTEventSource")) {
            touchEventPosters->setObject(provider);
            entry = 0;
        }
        else {
            entry = entry->getParentEntry(gIOServicePlane);
        }
    }

    return true;
}

void IOHIDSystem::detach( IOService * provider )
{
    touchEventPosters->removeObject(provider);
    super::detach(provider);
}
