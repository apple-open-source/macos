/*
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2009 Apple Computer, Inc.  All Rights Reserved.
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
/* 	Copyright (c) 1992 NeXT Computer, Inc.  All rights reserved. 
 *
 * EventDriver.m - Event System module, ObjC implementation.
 *
 *		The EventDriver is a pseudo-device driver.
 *
 * HISTORY
 * 31-Mar-92    Mike Paquette at NeXT 
 *      Created. 
 * 04-Aug-93	Erik Kay at NeXT
 *		minor API cleanup
 * 12-Dec-00	bubba at Apple.
 *		Handle eject key cases on Pro Keyboard.
 * 20-Nov-01	ryepez at Apple
 *		Replaced use of command queue with command gate.
*/

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
#include <IOKit/hidsystem/IOHIDShared.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/hid/IOHIDEvent.h>
#include <IOKit/usb/USB.h>
#include "IOHIDSystem.h"
#include "IOHIDEventService.h"
#include "IOHIDPointing.h"
#include "IOHIDKeyboard.h"
#include "IOHIDConsumer.h"
#include "IOHITablet.h"
#include "IOHIDPointingDevice.h"
#include "IOHIDKeyboardDevice.h"
#include "IOHIDKeys.h"
#include "IOHIDPrivate.h"
#include "IOHIDPrivateKeys.h"
#include "IOHIDEventServiceQueue.h"
#include "IOLLEvent.h"

#include <IOKit/hidsystem/ev_private.h>	/* Per-machine configuration info */ 
#include "IOHIDUserClient.h"
#include "AppleHIDUsageTables.h"
#include "IOHIDKeyboard.h"
#include "IOHIDFamilyTrace.h"

#include <sys/kdebug.h>
#include <sys/proc.h>

#ifdef __cplusplus
    extern "C"
    {
        #include <UserNotification/KUNCUserNotifications.h>
        void cons_cinput( char c);
    }
#endif

bool displayWranglerUp( OSObject *, void *, IOService * );

static IOHIDSystem * evInstance = 0;
MasterAudioFunctions *masterAudioFunctions = 0;

#define xpr_ev_cursor(x, a, b, c, d, e)
#define PtInRect(ptp,rp) \
	((ptp)->x >= (rp)->minx && (ptp)->x <  (rp)->maxx && \
	(ptp)->y >= (rp)->miny && (ptp)->y <  (rp)->maxy)

#ifndef kIOFBWaitCursorFramesKey
#define kIOFBWaitCursorFramesKey	"IOFBWaitCursorFrames"
#endif
#ifndef kIOFBWaitCursorPeriodKey
#define kIOFBWaitCursorPeriodKey	"IOFBWaitCursorPeriod"
#endif

#ifndef kIOUserClientCrossEndianKey
#define kIOUserClientCrossEndianKey "IOUserClientCrossEndian"
#endif

#ifndef kIOUserClientCrossEndianCompatibleKey
#define kIOUserClientCrossEndianCompatibleKey "IOUserClientCrossEndianCompatible"
#endif

#ifndef abs
#define abs(_a)	((_a >= 0) ? _a : -_a)
#endif

#define NORMAL_MODIFIER_MASK (NX_COMMANDMASK | NX_CONTROLMASK | NX_SHIFTMASK | NX_ALTERNATEMASK)

static inline unsigned AbsoluteTimeToTick( AbsoluteTime * ts )
{
    UInt64	nano;
    absolutetime_to_nanoseconds(*ts, &nano);
    return( nano >> 24 );
}

static inline void TickToAbsoluteTime( unsigned tick, AbsoluteTime * ts )
{
    UInt64	nano = ((UInt64) tick) << 24;
    nanoseconds_to_absolutetime(nano, ts);
}

static UInt8 ScalePressure(unsigned pressure)
{    
    return ((pressure * (unsigned long long)EV_MAXPRESSURE) / (unsigned)(65535LL));                
}

#define EV_NS_TO_TICK(ns)	AbsoluteTimeToTick(ns)
#define EV_TICK_TO_NS(tick,ns)	TickToAbsoluteTime(tick,ns)

/* COMMAND GATE COMPATIBILITY TYPE DEFS */ 
typedef struct _IOHIDCmdGateActionArgs {
    void*  	arg0;
    void*	arg1;
    void*	arg2;
    void*	arg3;
    void*	arg4;
    void*	arg5;
    void*	arg6;
    void*	arg7;
    void*	arg8;
    void*	arg9;
    void*	arg10;
    void*   arg11;
} IOHIDCmdGateActionArgs;
/* END COMMAND GATE COMPATIBILITY TYPE DEFS */

/* HID SYSTEM EVENT LOCK OUT SUPPORT */

static bool 		gKeySwitchLocked = false;
static bool             gUseKeyswitch = true;
static IONotifier * 	gSwitchNotification = 0;

// IONotificationHandler
static bool keySwitchNotificationHandler(void *target, void *refCon, IOService *service) {
    
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


#define TICKLE_DISPLAY                          \
{                                               \
    if (!evStateChanging && displayManager)     \
        displayManager->activityTickle(0,0);	\
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

#define kIOHIDPowerOnThresholdNS            1000000000ULL // 1 second
#define kIOHIDRelativeTickleThresholdNS 	50000000ULL // 1/20 second
#define kIOHIDRelativeTickleThresholdPixel	20

static AbsoluteTime gIOHIDPowerOnThresoldAbsoluteTime;
static AbsoluteTime gIOHIDRelativeTickleThresholdAbsoluteTime;

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
    UInt64                      lastUpdate;
    SInt32                      lastButtons;
    SInt32                      accumX;
    SInt32                      accumY;
    bool                        proximity;
    UInt32                      state;
    UInt8                       subType;
    NXEventData                 tabletData;
    NXEventData                 proximityData;
    IOGPoint                    pointerFraction;
    UInt8                       lastPressure;
} CachedMouseEventStruct;

static SInt32 GetCachedMouseButtonStates(OSArray *events, UInt64 nowNano, UInt64 timeout)
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
                if ((nowNano - mouseEvent->lastUpdate) < timeout)
                    buttonState |= mouseEvent->lastButtons;
            }
        }
    }
    
    return buttonState;    
}

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

static void AppendNewCachedMouseEventForService(OSArray *events, OSObject *service)
{
    CachedMouseEventStruct  temp;
    OSData *                data;
    
    bzero(&temp, sizeof(CachedMouseEventStruct));
    temp.service = service;

    data = OSData::withBytes(&temp, sizeof(CachedMouseEventStruct));
    events->setObject(data);
    data->release();
}

static void RemoveCachedMouseEventForService(OSArray *events, OSObject *service)
{
    UInt32  index;
    
    if ( events && GetCachedMouseEventForService(events, service, &index) )
    {
        events->removeObject(index);
    }
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
    
    deviceID = OSNumber::withNumber((uintptr_t)hiDevice, 64);
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
             (serviceID->unsigned64BitValue() == (uintptr_t)service) )
        {
            systemInfo->removeObject(i);
            break;
        }
    }
}

//************************************************************
// keyboardEventQueue support
//************************************************************
static queue_head_t		gKeyboardEQ;
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
    IOHIKeyboard	*keyboard;
    
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

#define super IOService
OSDefineMetaClassAndStructors(IOHIDSystem, IOService);


/* Return the current instance of the EventDriver, or 0 if none. */
IOHIDSystem * IOHIDSystem::instance()
{
  return evInstance;
}

bool IOHIDSystem::init(OSDictionary * properties)
{
  if (!super::init(properties))  return false;

  /*
   * Initialize minimal state.
   */

  evScreen         = NULL;
  timerES          = 0;
  eventConsumerES  = 0;
  keyboardEQES     = 0;
  cmdGate	   = 0;
  workLoop         = 0;
  cachedEventFlags = 0;
  consumedKeyCode = (unsigned)-1;
  displayState = IOPMDeviceUsable;
  AbsoluteTime_to_scalar(&lastEventTime) = 0;
  AbsoluteTime_to_scalar(&lastUndimEvent) = 0;
  AbsoluteTime_to_scalar(&stateChangeDeadline) = 0;
    mouseButtonTimeout = 2000000000ULL; // 2 second default timeout

  ioHIDevices      = OSArray::withCapacity(2);
  cachedButtonStates = OSArray::withCapacity(3);
  touchEventPosters = OSSet::withCapacity(2);
    
  // RY: Populate cachedButtonStates key=0 with a button State
  // This will cover all pointing devices that don't support 
  // the new private methods.
  AppendNewCachedMouseEventForService(cachedButtonStates, 0);

  nanoseconds_to_absolutetime(kIOHIDPowerOnThresholdNS, &gIOHIDPowerOnThresoldAbsoluteTime);
  nanoseconds_to_absolutetime(kIOHIDRelativeTickleThresholdNS, &gIOHIDRelativeTickleThresholdAbsoluteTime);
  
  queue_init(&gKeyboardEQ);
  gKeyboardEQLock = IOLockAlloc();   
  return true;
}

IOHIDSystem * IOHIDSystem::probe(IOService * 	provider,
				 SInt32 *	score)
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
    bool iWasStarted = false;
    
    do {
        if (!super::start(provider))  break;
        
        evInstance = this;
        
        /* A few details to be set up... */
        pointerLoc.x = INIT_CURSOR_X;
        pointerLoc.y = INIT_CURSOR_Y;
        
        pointerDelta.x = 0;
        pointerDelta.y = 0;
        
        evScreenSize = sizeof(EvScreen) * 32;
        evScreen = (void *) IOMalloc(evScreenSize);
        savedParameters = OSDictionary::withCapacity(4);
        
        if (!evScreen || !savedParameters)  break;
                
        bzero(evScreen, evScreenSize);
        firstWaitCursorFrame = EV_WAITCURSOR;
        maxWaitCursorFrame   = EV_MAXCURSOR;
        createParameters();
        
      OSNumber *mouseTimeout = OSDynamicCast(OSNumber, getProperty(kIOHIDSystemMouseButtonTimeout));
      if (mouseTimeout) {
          mouseButtonTimeout = mouseTimeout->unsigned64BitValue();
      }
      else {
          mouseTimeout = OSNumber::withNumber(mouseButtonTimeout, sizeof(mouseButtonTimeout) * 8);
          setProperty(kIOHIDSystemMouseButtonTimeout, mouseTimeout);
      }

        /*
         * Start up the work loop
         */
        workLoop = IOWorkLoop::workLoop();
        cmdGate = IOCommandGate::commandGate
                  (this);
        timerES = IOTimerEventSource::timerEventSource
                  (this, (IOTimerEventSource::Action) &_periodicEvents );
        eventConsumerES = IOInterruptEventSource::interruptEventSource
                          (this, (IOInterruptEventSource::Action) &doKickEventConsumer);
        keyboardEQES = IOInterruptEventSource::interruptEventSource
                       (this, (IOInterruptEventSource::Action) &doProcessKeyboardEQ);
        vblES = IOTimerEventSource::timerEventSource
                (this, &_vblEvent );
                
        if (!workLoop || !cmdGate || !timerES || !eventConsumerES || !keyboardEQES || !vblES)
            break;
            
        if ((workLoop->addEventSource(cmdGate)    != kIOReturnSuccess)
                ||  (workLoop->addEventSource(timerES) != kIOReturnSuccess)
                ||  (workLoop->addEventSource(vblES) != kIOReturnSuccess)
                ||  (workLoop->addEventSource(eventConsumerES) != kIOReturnSuccess)
                ||  (workLoop->addEventSource(keyboardEQES) != kIOReturnSuccess))
            break;
            
        publishNotify = addNotification(
                            gIOPublishNotification, serviceMatching("IOHIDevice"),
                            &IOHIDSystem::genericNotificationHandler,
                            this, (void *)&IOHIDSystem::handlePublishNotification );
                            
        if (!publishNotify) break;
        
        eventPublishNotify = addNotification(
                                 gIOPublishNotification, serviceMatching("IOHIDEventService"),
                                 &IOHIDSystem::genericNotificationHandler,
                                 this, (void *)&IOHIDSystem::handlePublishNotification );
                                 
        if (!eventPublishNotify) break;
        
        terminateNotify = addNotification(
                              gIOTerminatedNotification, serviceMatching("IOHIDevice"),
                              &IOHIDSystem::genericNotificationHandler,
                              this, (void *)&IOHIDSystem::handleTerminateNotification );
                              
        if (!terminateNotify) break;
        
        eventTerminateNotify = addNotification(
                                   gIOTerminatedNotification, serviceMatching("IOHIDEventService"),
                                   &IOHIDSystem::genericNotificationHandler,
                                   this, (void *)&IOHIDSystem::handleTerminateNotification );
                                   
        if (!eventTerminateNotify) break;
        
        // RY: Listen to the root domain
        rootDomain = (IOService *)getPMRootDomain();
        
        if (rootDomain)
            rootDomain->registerInterestedDriver(this);
            
            
        // Allocated and publish the systemInfo array
		systemInfo = OSArray::withCapacity(4);
		if (systemInfo)
		{
            setProperty(kNXSystemInfoKey, systemInfo);
            systemInfo->release();
        }
        
        /*
         * IOHIDSystem serves both as a service and a nub (we lead a double
         * life).  Register ourselves as a nub to kick off matching.
         */
        
        registerService();
        
        addNotification( gIOPublishNotification, serviceMatching("IODisplayWrangler"),
                         &IOHIDSystem::genericNotificationHandler,
                         this, (void *)&IOHIDSystem::handlePublishNotification );
                         
        // Get notified everytime AppleKeyswitch registers (each time keyswitch changes)
        gSwitchNotification = addNotification(gIOPublishNotification, nameMatching("AppleKeyswitch"),
                                              (IOServiceNotificationHandler)keySwitchNotificationHandler, this, 0);
                                              
        iWasStarted = true;
        
        // Let's go ahead and cache our registry name.
        // This was added to remove a call to getName while
        // we are disabling preemption
        registryName = getName();
        
    }
    while (false);
    
    if (!iWasStarted)  evInstance = 0;
    
    return iWasStarted;
}

// powerStateDidChangeTo
//
// The display wrangler has changed state, so the displays have changed
// state, too.  We save the new state.

IOReturn IOHIDSystem::powerStateDidChangeTo( IOPMPowerFlags theFlags, unsigned long, IOService * service)
{
    
    if (service == displayManager)
    {
        displayState = theFlags;
    }
    else if (service == rootDomain)
    {
        if (theFlags & kIOPMPowerOn)
        {
            clock_get_uptime(&stateChangeDeadline);
            ADD_ABSOLUTETIME(&stateChangeDeadline, &gIOHIDPowerOnThresoldAbsoluteTime);
        }
    }
    return IOPMNoErr;
}

bool IOHIDSystem::genericNotificationHandler(
			void * target,
			void * handler,
			IOService * newService )
{
    IOHIDSystem * self = (IOHIDSystem *) target;

    return self->cmdGate->runAction((IOCommandGate::Action)handler, newService);
}

bool IOHIDSystem::handlePublishNotification(
			void * target,
			IOService * newService )
{
    IOHIDSystem * self = (IOHIDSystem *) target;

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
            newSystemInfo->release();
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
    int	index;

    if( self->eventsOpen && (
        OSDynamicCast(IOHIDevice, service) || 
        OSDynamicCast(IOHIDEventService, service))) 
    {
        service->close(self);
    }
    
    self->detach(service);

    if (self->ioHIDevices) {
        if ((index = self->ioHIDevices->getNextIndexOfObject(service, 0)) != -1)
            self->ioHIDevices->removeObject(index);
    }
    
    OSArray * newSystemInfo = OSArray::withArray(self->systemInfo);
    if ( newSystemInfo )
    {
        RemoveNXSystemInfoForService(newSystemInfo, service);
        self->setProperty(kNXSystemInfoKey, newSystemInfo);
        newSystemInfo->release();
        self->systemInfo = newSystemInfo;
    }

    // RY: Remove this object from the cachedButtonState
    if (OSDynamicCast(IOHIPointing, service))
    {
        // Clear the service button state
        AbsoluteTime	ts;
        clock_get_uptime(&ts);
        self->relativePointerEvent(0, 0, 0, ts, service);
        
        CachedMouseEventStruct *cachedMouseEvent;
        if ((cachedMouseEvent = GetCachedMouseEventForService(self->cachedButtonStates, service)) &&
            (cachedMouseEvent->proximityData.proximity.enterProximity))
        {
            absolutetime_to_nanoseconds(ts, &cachedMouseEvent->lastUpdate);
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
        
    return true;
}

/*
 * Free locally allocated resources, and then ourselves.
 */
void IOHIDSystem::free()
{
	// we are going away. stop the workloop.
    if (workLoop) {
        workLoop->disableAllEventSources();
    }
    
    if (evScreen) IOFree( (void *)evScreen, evScreenSize );
    evScreen = (void *)0;
    evScreenSize = 0;
    
    if (timerES) {
        timerES->cancelTimeout();
        
        if ( workLoop )
            workLoop->removeEventSource( timerES );
            
        timerES->release();
        timerES = 0;
    }
    if (vblES) {
        vblES->cancelTimeout();
        
        if ( workLoop )
            workLoop->removeEventSource( vblES );
            
        vblES->release();
        vblES = 0;
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
    if (cmdGate) {
        evClose();
        cmdGate->release();
        cmdGate = 0;
    }
    if (workLoop) {
        workLoop->release();
        workLoop = 0;
    }
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
    if (ioHIDevices) {
        ioHIDevices->release();
        ioHIDevices = 0;
    }
    if (cachedButtonStates) {
        cachedButtonStates->release();
        cachedButtonStates = 0;
    }
    
    if ( gKeyboardEQLock ) {
        IOLock * lock = gKeyboardEQLock;
        IOLockLock(lock);
        gKeyboardEQLock = 0;
        IOLockUnlock(lock);
        IOLockFree(lock);
    }
    
    if (_hidKeyboardDevice) {
        _hidKeyboardDevice->release();
        _hidKeyboardDevice = 0;
    }
    
    if (_hidPointingDevice) {
        _hidPointingDevice->release();
        _hidPointingDevice = 0;
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

	// Tear down the shared memory area if set up
//	if ( eventsOpen == true )
//	    unmapEventShmem(eventPort);

	// Clear screens registry and related data
	if ( evScreen != (void *)0 )
	{
	    screens = 0;
	    lastShmemPtr = (void *)0;
	}
	// Remove port notification for the eventPort and clear the port out
	setEventPort(MACH_PORT_NULL);
//	ipc_port_release_send(event_port);

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
    IOGPoint p;

    if( !eventsOpen)
	return;

    for( int i = 0; i < screens; i++ ) {

        EvScreen *esp = &((EvScreen*)evScreen)[i];
    
        if ( esp->instance )
        {
            p.x = evg->cursorLoc.x;	// Copy from shmem.
            p.y = evg->cursorLoc.y;

            bool onscreen = (0 != (cursorScreens & (1 << i)));
    
            switch ( evcmd )
            {
                case EVMOVE:
                    if (onscreen)
                        esp->instance->moveCursor(&p, evg->frame);
                    break;
    
                case EVSHOW:
                    if (onscreen)
                        esp->instance->showCursor(&p, evg->frame);
                    break;
    
                case EVHIDE:
                    if (onscreen)
                        esp->instance->hideCursor();
                    break;
    
                case EVLEVEL:
                case EVNOP:
                    /* lets keep that compiler happy */
                    break;
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
        { { MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,	// mach3xxx, is the right?
                        MACH_MSG_TYPE_MAKE_SEND),	// mach_msg_bits_t	msgh_bits;
            sizeof (struct evioSpecialKeyMsg), 		// mach_msg_size_t	msgh_size;
            MACH_PORT_NULL,				// mach_port_t	msgh_remote_port;
            MACH_PORT_NULL,				// mach_port_t	msgh_local_port;
            0,						// mach_msg_size_t msgh_reserved;
            EV_SPECIAL_KEY_MSG_ID			// mach_msg_id_t 	msgh_id;
            },
            0,	/* key */
            0,	/* direction */
            0,	/* flags */
            0	/* level */
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
            
        OSDictionary 	*tempDict = OSDictionary::withCapacity(3);
        UInt64 		nano;
        
        nanoseconds_to_absolutetime( EV_DCLICKTIME, &clickTimeThresh);
	clickSpaceThresh.x = clickSpaceThresh.y = EV_DCLICKSPACE;
        AbsoluteTime_to_scalar( &clickTime) = 0;
	clickLoc.x = clickLoc.y = -EV_DCLICKSPACE;
	clickState = 1;
        
        if (tempDict) {
            UInt32	tempClickSpace[] = {clickSpaceThresh.x, clickSpaceThresh.y};
            makeInt32ArrayParamProperty( tempDict, kIOHIDClickSpaceKey,
                        tempClickSpace, sizeof(tempClickSpace)/sizeof(UInt32) );

            nano = EV_DCLICKTIME;
            makeNumberParamProperty( tempDict, kIOHIDClickTimeKey,
                        nano, 64 );
            
            setParamProperties(tempDict);
            
            tempDict->release();
        }
}

/*
 * Methods exported by the EventDriver.
 *
 *	The screenRegister protocol is used by frame buffer drivers to register
 *	themselves with the Event Driver.  These methods are called in response
 *	to a registerSelf or unregisterSelf message received from the Event
 *	Driver.
 */

int IOHIDSystem::registerScreen(IOGraphicsDevice * instance,
                /* bounds */    IOGBounds * bp)
{
    EvScreen *esp;
    OSNumber *num;

    if( (false == eventsOpen) || (0 == bp) )
    {
	return -1;
    }

    if ( lastShmemPtr == (void *)0 )
	lastShmemPtr = evs;
    
    /* shmemSize and bounds already set */
    esp = &((EvScreen*)evScreen)[screens];
    esp->instance = instance;
    esp->bounds = bp;
    // Update our idea of workSpace bounds
    if ( bp->minx < workSpace.minx )
	workSpace.minx = bp->minx;
    if ( bp->miny < workSpace.miny )
	workSpace.miny = bp->miny;
    if ( bp->maxx < workSpace.maxx )
	workSpace.maxx = bp->maxx;
    if ( esp->bounds->maxy < workSpace.maxy )
	workSpace.maxy = bp->maxy;

    if( (num = OSDynamicCast(OSNumber, instance->getProperty(kIOFBWaitCursorFramesKey)))
      && (num->unsigned32BitValue() > maxWaitCursorFrame)) {
	firstWaitCursorFrame = 0;
        maxWaitCursorFrame   = num->unsigned32BitValue();
        evg->lastFrame	     = maxWaitCursorFrame;
    }
    if( (num = OSDynamicCast(OSNumber, instance->getProperty(kIOFBWaitCursorPeriodKey))))
        clock_interval_to_absolutetime_interval(num->unsigned32BitValue(), kNanosecondScale,
                                                &waitFrameRate);

    return(SCREENTOKEN + screens++);
}


void IOHIDSystem::unregisterScreen(int index) {
    
    cmdGate->runAction((IOCommandGate::Action)doUnregisterScreen, (void *)index);
    
}

IOReturn IOHIDSystem::doUnregisterScreen (IOHIDSystem *self, void * arg0) 
                        /* IOCommandGate::Action */
{
    int index = (uintptr_t) arg0;
    
    self->unregisterScreenGated(index);
    
    return kIOReturnSuccess;
}

void IOHIDSystem::unregisterScreenGated(int index)
{
    
    index -= SCREENTOKEN;

    if ( eventsOpen == false || index < 0 || index >= screens )
	return;
    
    hideCursor();

    // clear the state for the screen
    ((EvScreen*)evScreen)[index].instance = 0;
    // Put the cursor someplace reasonable if it was on the destroyed screen
    cursorScreens &= ~(1 << index);
    // This will jump the cursor back on screen
    setCursorPosition((IOGPoint *)&evg->cursorLoc, true);

    showCursor();
}

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
    IOByteCount			size;
    bool                clean = false;
    
    if ( shmemVersion != kIOHIDCurrentShmemVersion)
        return kIOReturnUnsupported;
        
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
    
    clock_interval_to_absolutetime_interval(DefaultWCFrameRate, kNanosecondScale, &waitFrameRate);
    clock_interval_to_absolutetime_interval(DefaultWCSustain, kNanosecondScale, &waitSustain);
    AbsoluteTime_to_scalar(&waitSusTime) = 0;
    AbsoluteTime_to_scalar(&waitFrameTime) = 0;
    
    EV_TICK_TO_NS(10,&periodicEventDelta);
    
    evg->buttons = 0;
    evg->eNum = INITEVENTNUM;
    evg->eventFlags = oldFlags;
    
    AbsoluteTime ts;
    unsigned tick;
    clock_get_uptime( &ts);
    tick = EV_NS_TO_TICK(&ts);
    if ( tick == 0 )
        tick = 1; // No zero values allowed!
    evg->VertRetraceClock = tick;
    
    evg->cursorLoc.x = pointerLoc.x;
    evg->cursorLoc.y = pointerLoc.y;
    evg->dontCoalesce = 0;
    evg->dontWantCoalesce = 0;
    evg->wantPressure = 0;
    evg->wantPrecision = 0;
    evg->mouseRectValid = 0;
    evg->movedMask = 0;
    ev_init_lock( &evg->cursorSema );
    ev_init_lock( &evg->waitCursorSema );
    
    /* Set up low-level queues */
    lleqSize = LLEQSIZE;
    for (i=lleqSize; --i != -1; ) {
        evg->lleq[i].event.type = 0;
        AbsoluteTime_to_scalar(&evg->lleq[i].event.time) = 0;
        evg->lleq[i].event.flags = 0;
        ev_init_lock(&evg->lleq[i].sema);
        evg->lleq[i].next = i+1;
    }
    evg->LLELast = 0;
    evg->lleq[lleqSize-1].next = 0;
    evg->LLEHead = evg->lleq[evg->LLELast].next;
    evg->LLETail = evg->lleq[evg->LLELast].next;
    
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
	static struct _eventMsg init_msg = { {
            // mach_msg_bits_t	msgh_bits;
            MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,0),
            // mach_msg_size_t	msgh_size;
            sizeof (struct _eventMsg),
            // mach_port_t	msgh_remote_port;
            MACH_PORT_NULL,
            // mach_port_t	msgh_local_port;
            MACH_PORT_NULL,
            // mach_msg_size_t 	msgh_reserved;
            0,
            // mach_msg_id_t	msgh_id;
            0
        } };

	if ( eventMsg == NULL )
		eventMsg = IOMalloc( sizeof (struct _eventMsg) );
	eventPort = port;
	// Initialize the events available message.
	*((struct _eventMsg *)eventMsg) = init_msg;

	((struct _eventMsg *)eventMsg)->h.msgh_remote_port = port;
        
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
        
    static struct _eventMsg init_msg = { {
            // mach_msg_bits_t	msgh_bits;
            MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,0),
            // mach_msg_size_t	msgh_size;
            sizeof (struct _eventMsg),
            // mach_port_t	msgh_remote_port;
            MACH_PORT_NULL,
            // mach_port_t	msgh_local_port;
            MACH_PORT_NULL,
            // mach_msg_size_t 	msgh_reserved;
            0,
            // mach_msg_id_t	msgh_id;
            0
        } };

	if ( stackShotMsg ) { 
        IOFree(stackShotMsg, sizeof (struct _eventMsg));
        stackShotMsg = NULL;
    }

    if ( stackShotPort ) {
        if ( !(stackShotMsg = IOMalloc( sizeof(struct _eventMsg))) )
            return;
            
        // Initialize the events available message.
        *((struct _eventMsg *)stackShotMsg) = init_msg;

        ((struct _eventMsg *)stackShotMsg)->h.msgh_remote_port = stackShotPort;
    }
}

UInt32 IOHIDSystem::eventFlags()
{
    return evg ? evg->eventFlags : 0;
}

void IOHIDSystem::dispatchEvent(IOHIDEvent *event, IOOptionBits options)
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
             /* at */       IOGPoint *    location,
             /* atTime */   AbsoluteTime  ts,
             /* withData */ NXEventData * myData,
             /* sender */   OSObject *    sender,
             /* extPID */   UInt32        extPID,
             /* processKEQ*/bool          processKEQ)
{
    // Clear out the keyboard queue up until this TS.  This should keep
    // the events in order.
    if ( processKEQ )
        processKeyboardEQ(this, &ts);
        
    NXEQElement * theHead = (NXEQElement *) &evg->lleq[evg->LLEHead];
    NXEQElement * theLast = (NXEQElement *) &evg->lleq[evg->LLELast];
    NXEQElement * theTail = (NXEQElement *) &evg->lleq[evg->LLETail];
    int    wereEvents;
    unsigned      theClock = EV_NS_TO_TICK(&ts);
    
    if (CMP_ABSOLUTETIME(&ts, &lastEventTime) < 0) {
        ts = lastEventTime;
    }
    lastEventTime = ts;
    
    // dispatch new event
    IOHIDEvent * event = IOHIDEvent::withEventData(ts, what, myData);
    if ( event ) {
        dispatchEvent(event);
        event->release();
    }
    
    /* Some events affect screen dimming (idle time) */
    if (EventCodeMask(what) & NX_UNDIMMASK) {
        lastUndimEvent = ts;
    }
    
    // Update the PS VertRetraceClock off of the timestamp if it looks sane
    if (   theClock > (unsigned)evg->VertRetraceClock && theClock < (unsigned)(evg->VertRetraceClock + (20 * EV_TICK_TIME)) )
        evg->VertRetraceClock = theClock;
        
    wereEvents = EventsInQueue();
    
    xpr_ev_post("postEvent: what %d, X %d Y %d Q %d, needKick %d\n", what,location->x,location->y, EventsInQueue(), needToKickEventConsumer);
    IOHID_DEBUG(kIOHIDDebugCode_PostEvent, what, theHead, theTail, sender);
    
    if ((!evg->dontCoalesce) /* Coalescing enabled */
            && (theHead != theTail)
            && (theLast->event.type == what)
            && (EventCodeMask(what) & COALESCEEVENTMASK)
            && ev_try_lock(&theLast->sema)) {
        /* coalesce events */
        theLast->event.location.x = location->x;
        theLast->event.location.y = location->y;
        absolutetime_to_nanoseconds(ts, &theLast->event.time);
        if (myData != NULL)
            theLast->event.data = *myData;
        ev_unlock(&theLast->sema);
    }
    else if (theTail->next != evg->LLEHead) {
        /* store event in tail */
        theTail->event.type         = what;
        theTail->event.service_id   = (uintptr_t)sender;
        theTail->event.ext_pid      = extPID;
        theTail->event.location.x   = location->x;
        theTail->event.location.y   = location->y;
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
                    && (myAbs(location->x - clickLoc.x) <= clickSpaceThresh.x)
                    && (myAbs(location->y - clickLoc.y) <= clickSpaceThresh.y)) {
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
        IOLog("%s: postEvent LLEventQueue overflow.\n", registryName);
        kickEventConsumer();
#if PMON
        pmon_log_event( PMON_SOURCE_EV,
                        KP_EV_QUEUE_FULL,
                        what,
                        evg->eventFlags,
                        theClock);
#endif
    }
}

/*
 * - kickEventConsumer
 *
 * 	Try to send a message out to let the event consumer know that
 *	there are now events available for consumption.
 */

void IOHIDSystem::kickEventConsumer()
{ 
	xpr_ev_post("kickEventConsumer (need == %d)\n",
		needToKickEventConsumer,2,3,4,5);
                
	if ( needToKickEventConsumer == true )
		return;		// Request is already pending

	needToKickEventConsumer = true;	// Posting a request now

        // Trigger eventConsumerES, so that doKickEventConsumer
        // is run from the workloop thread.
        eventConsumerES->interruptOccurred(0, 0, 0);
}

/*
 * - sendStackShotMessage
 *
 * 	Try to send a message out to let the stack shot know we got
 *  the magic key sequence
 */

void IOHIDSystem::sendStackShotMessage()
{ 
	kern_return_t r;
	mach_msg_header_t *msgh;

    xpr_ev_post("sendStackShotMessage\n", 1,2,3,4,5);
    
	msgh = (mach_msg_header_t *)stackShotMsg;
	if( msgh) {

        r = mach_msg_send_from_kernel( msgh, msgh->msgh_size);
        switch ( r ) {
            case MACH_SEND_TIMED_OUT:/* Already has a message posted */
            case MACH_MSG_SUCCESS:	/* Message is posted */
                break;
            default:		/* Log the error */
                IOLog("%s: sendStackShotMessage msg_send returned %d\n", registryName, r);
                break;
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
	if ( r == MACH_SEND_INVALID_DEST )	/* Invalidate the port */
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
                case MACH_MSG_SUCCESS:	/* Message is posted */
                    break;
                default:		/* Log the error */
                    IOLog("%s: doKickEventConsumer msg_send returned %d\n",
				self->registryName, r);
                    break;
            }
	}
}

//
// Schedule the next periodic event to be run, based on the current state of
// the event system.  We have to consider things here such as when the last
// periodic event pass ran, if there is currently any mouse delta accumulated,
// and how long it has been since the last event was consumed by an app (for
// driving the wait cursor).
//
// This code should only be run from the periodicEvents method or
// _setCursorPosition.
//
void IOHIDSystem::scheduleNextPeriodicEvent()
{
    if (CMP_ABSOLUTETIME( &waitFrameTime, &thisPeriodicRun) > 0)
    {
	AbsoluteTime time_for_next_run;

        clock_get_uptime(&time_for_next_run);
        ADD_ABSOLUTETIME( &time_for_next_run, &periodicEventDelta);

        if (CMP_ABSOLUTETIME( &waitFrameTime, &time_for_next_run) < 0) {
            timerES->wakeAtTime(waitFrameTime);
            return;
        }
    }

    timerES->setTimeout(periodicEventDelta);
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
                                  /* IOTimerEventSource::Action */
{
    self->periodicEvents(timer);
}

void IOHIDSystem::periodicEvents(IOTimerEventSource * /* timer */)
{
	unsigned int	tick;

	// If eventsOpen is false, then the driver shmem is
	// no longer valid, and it is in the process of shutting down.
	// We should give up without rescheduling.
	if ( eventsOpen == false )
		return;
        
	// Increment event time stamp last
        clock_get_uptime(&thisPeriodicRun);

	// Temporary hack til we wean CGS off of VertRetraceClock
	tick = EV_NS_TO_TICK(&thisPeriodicRun);
	if ( tick == 0 )
		tick = 1;
	evg->VertRetraceClock = tick;

	// Update cursor position if needed
	if ( needSetCursorPosition == true )
		_setCursorPosition(&pointerLoc, false);

	// WAITCURSOR ACTION
	if ( ev_try_lock(&evg->waitCursorSema) )
	{
	    if ( ev_try_lock(&evg->cursorSema) )
	    {
		// See if the current context has timed out
		if (   (evg->AALastEventSent != evg->AALastEventConsumed)
		    && ((evg->VertRetraceClock - evg->AALastEventSent >
					evg->waitThreshold)))
		    evg->ctxtTimedOut = TRUE;
		// If wait cursor enabled and context timed out, do waitcursor
		if (evg->waitCursorEnabled && evg->globalWaitCursorEnabled &&
		evg->ctxtTimedOut)
		{
		    /* WAIT CURSOR SHOULD BE ON */
		    if (!evg->waitCursorUp)
			showWaitCursor();
		} else
		{
		    /* WAIT CURSOR SHOULD BE OFF */
		    if (evg->waitCursorUp &&
			CMP_ABSOLUTETIME(&waitSusTime, &thisPeriodicRun) <= 0)
			hideWaitCursor();
		}
		/* Animate cursor */
		if (evg->waitCursorUp &&
			CMP_ABSOLUTETIME(&waitFrameTime, &thisPeriodicRun) <= 0)
			animateWaitCursor();
		ev_unlock(&evg->cursorSema);
	    }
	    ev_unlock(&evg->waitCursorSema);
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
        if (!screen[i].instance)
            continue;
        if ((screen[i].bounds->maxx - screen[i].bounds->minx) < 128)
            continue;
        candidate = i;
        if ((screen[i].instance) && PtInRect(p, screen[i].bounds)) {
            pinScreen = i;
            newScreens |= (1 << i);
        }
    }

    if (newScreens == 0)
        pinScreen = candidate;

    if (!cursorPinned) {
        // reset pin rect
        cursorPin = *(((EvScreen*)evScreen)[pinScreen].bounds);
        cursorPin.maxx--;	/* Make half-open rectangle */
        cursorPin.maxy--;
        cursorPinScreen = pinScreen;
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
            if ((screen[i].instance) && PtInRect(p, screen[i].bounds))
                newScreens |= (1 << i);
        }
    }

    cursorScreens = newScreens;

    pointerDelta.x += (evg->cursorLoc.x - pointerLoc.x);
    pointerDelta.y += (evg->cursorLoc.y - pointerLoc.y);
    pointerLoc.x = evg->cursorLoc.x;
    pointerLoc.y = evg->cursorLoc.y;

    postDeltaX = postDeltaY = accumDX = accumDY = 0;

    vblES->cancelTimeout();

    return( true );
}

bool IOHIDSystem::startCursor()
{
    bool		ok;

    if (0 == screens)		// no screens, no cursor
        return( false );

    cursorPinned = false;
    resetCursor();
    showCursor();

    // Start the cursor control callouts
    ok = (kIOReturnSuccess ==
            cmdGate->runAction((IOCommandGate::Action)_periodicEvents, timerES));

    cursorStarted = ok;
    return( ok );
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
	// Set animation and sustain absolute times.

	waitSusTime = waitFrameTime = thisPeriodicRun;
	ADD_ABSOLUTETIME( &waitFrameTime, &waitFrameRate);
	ADD_ABSOLUTETIME( &waitSusTime, &waitSustain);
}

void IOHIDSystem::hideWaitCursor()
{
	xpr_ev_cursor("hideWaitCursor\n",1,2,3,4,5);
	evg->waitCursorUp = false;
        hideCursor();
        evg->frame = EV_STD_CURSOR;
        showCursor();
        AbsoluteTime_to_scalar(&waitFrameTime) = 0;
        AbsoluteTime_to_scalar(&waitSusTime ) = 0;
}

void IOHIDSystem::animateWaitCursor()
{
	xpr_ev_cursor("animateWaitCursor\n",1,2,3,4,5);
	changeCursor(evg->frame + 1);
	// Set the next animation time.
	waitFrameTime = thisPeriodicRun;
	ADD_ABSOLUTETIME( &waitFrameTime, &waitFrameRate);
}

void IOHIDSystem::changeCursor(int frame)
{ 
	evg->frame =
		((frame > (int)maxWaitCursorFrame) || (frame > evg->lastFrame)) ? firstWaitCursorFrame : frame;
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
	if (screen[i].instance != 0
	&& (p->x >= screen[i].bounds->minx)
	&& (p->x < screen[i].bounds->maxx)
	&& (p->y >= screen[i].bounds->miny)
	&& (p->y < screen[i].bounds->maxy))
	    return i;
    }
    return(-1);	/* Cursor outside of known screen boundary */
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
//	Attach the default event sources.
//
void IOHIDSystem::attachDefaultEventSources()
{
	IOService  *     source;
	OSIterator * 	sources;


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
//	Detach all event sources
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
#ifdef DEBUG
      kprintf("detachEventSource:%s\n", provider->getName());
#endif
      provider->close( this );
    case kIOMessageServiceWasClosed:
      break;

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
void IOHIDSystem::scaleLocationToCurrentScreen(IOGPoint *location, IOGBounds *bounds)
{
    IOHIDSystem * hidsystem = instance();
    IOGPoint fraction;
    
    if ( hidsystem ) hidsystem->_scaleLocationToCurrentScreen(location, &fraction, bounds);
}

void IOHIDSystem::_scaleLocationToCurrentScreen(IOGPoint *location, IOGPoint *fraction, IOGBounds *bounds)
{
    IOFixed     result, locationScale, deviceScale, screenScale;
    // We probably also need to look at current screen offsets as well
    // but that shouldn't matter until we provide tablets with a way to
    // switch screens...

    /*
    location->x = ((location->x - bounds->minx) * (cursorPin.maxx - cursorPin.minx + 1)
                / (bounds->maxx - bounds->minx)) + cursorPin.minx;
    location->y = ((location->y - bounds->miny) * (cursorPin.maxy - cursorPin.miny + 1)
                / (bounds->maxy - bounds->miny)) + cursorPin.miny;
    */
    
    // Calculate X
    locationScale   = (location->x - bounds->minx)          << 16;
    screenScale     = (cursorPin.maxx - cursorPin.minx + 1) << 16;
    deviceScale     = (bounds->maxx - bounds->minx)         << 16;
    
    result = (deviceScale) ? IOFixedDivide ( locationScale, deviceScale ) : 0;
    result = IOFixedMultiply ( result, screenScale );
    
    location->x = (result >> 16);
    fraction->x = result;
    
    // Calculate Y
    locationScale   = (location->y - bounds->miny)          << 16;
    screenScale     = (cursorPin.maxy - cursorPin.miny + 1) << 16;
    deviceScale     = (bounds->maxy - bounds->miny)         << 16;
    
    result = (deviceScale) ? IOFixedDivide ( locationScale, deviceScale ) : 0;
    result = IOFixedMultiply ( result, screenScale );
    
    location->y = (result >> 16);    
    fraction->y = result;

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
                                    void *     refcon)
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
    int       	 	buttons	= *(int *)((IOHIDCmdGateActionArgs *)args)->arg0;
    int        		dx	= *(int *)((IOHIDCmdGateActionArgs *)args)->arg1;
    int        		dy	= *(int *)((IOHIDCmdGateActionArgs *)args)->arg2;
    AbsoluteTime 	ts	= *(AbsoluteTime *)((IOHIDCmdGateActionArgs *)args)->arg3;
    OSObject *          sender  = (OSObject *)((IOHIDCmdGateActionArgs *)args)->arg4;

    self->relativePointerEventGated(buttons, dx, dy, ts, sender);
    
    return kIOReturnSuccess;
}

void IOHIDSystem::relativePointerEventGated(int buttons, int dx, int dy, AbsoluteTime ts, OSObject * sender)
{ 
    UnsignedWide nextVBL, vblDeltaTime, eventDeltaTime, moveDeltaTime;
    bool haveVBL;
        
    if( eventsOpen == false )
        return;

    if(ShouldConsumeHIDEvent(ts, stateChangeDeadline))
        return;

    if ( !(displayState & IOPMDeviceUsable) ) {	// display is off, consume the button event
        if ( buttons ) {
            return;
        }

        TICKLE_DISPLAY;
        return;
    }

    CachedMouseEventStruct *cachedMouseEvent = GetCachedMouseEventForService(cachedButtonStates, sender);
    if (cachedMouseEvent) {
        do {
            // RY: If the display is dim and there is no change in buttonStates, 
            // compare relative mouse movements against thresholds to determine 
            // whether or not to wake the display.  This will prevent premature
            // display wake upon chatty mice.
            UInt64 ts_nano;
            absolutetime_to_nanoseconds(ts, &ts_nano);
            cachedMouseEvent->lastUpdate = ts_nano;

            if ((cachedMouseEvent->lastButtons == buttons) && (displayState == 0)) {
                if (ts_nano > cachedMouseEvent->eventDeadline) {
                    cachedMouseEvent->eventDeadline = ts_nano + kIOHIDPowerOnThresholdNS;
                    cachedMouseEvent->accumX = 0;
                    cachedMouseEvent->accumY = 0;
                }
                
                cachedMouseEvent->accumX += dx;
                cachedMouseEvent->accumY += dy;

                if ((abs(cachedMouseEvent->accumX) < kIOHIDRelativeTickleThresholdPixel) &&
                    (abs(cachedMouseEvent->accumY) < kIOHIDRelativeTickleThresholdPixel))
                {
                    break;
                }                
            }
            
            TICKLE_DISPLAY;
            cachedMouseEvent->lastButtons = buttons;
            cachedMouseEvent->eventDeadline = ts_nano;
            
            // Fake up pressure changes from button state changes
            if( (buttons & EV_LB) != (evg->buttons & EV_LB) )
            {
                cachedMouseEvent->lastPressure = ( buttons & EV_LB ) ? MAXPRESSURE : MINPRESSURE;
            }
            
        } while (false);
    }
    
    _setButtonState(buttons, /* atTime */ ts, sender);

    int oldDx = dx;
    int oldDy = dy;

    // figure cursor movement
    if( dx || dy )
    {
        eventDeltaTime = *((UnsignedWide *)(&ts));
        SUB_ABSOLUTETIME( &eventDeltaTime, &lastRelativeEventTime );
        lastRelativeEventTime = ts;

        IOGraphicsDevice * instance = ((EvScreen*)evScreen)[cursorPinScreen].instance;
        if( instance) {
            instance->getVBLTime( (AbsoluteTime *)&nextVBL, (AbsoluteTime *)&vblDeltaTime );
        } else
            nextVBL.hi = nextVBL.lo = vblDeltaTime.hi = vblDeltaTime.lo = 0;

        if( dx && ((dx ^ accumDX) < 0))
            accumDX = 0;
        if( dy && ((dy ^ accumDY) < 0))
            accumDY = 0;

        KERNEL_DEBUG(KDBG_CODE(12, 0, 18), nextVBL.hi, nextVBL.lo, lastRelativeEventTime.hi, lastRelativeEventTime.lo, 0);

		haveVBL = (nextVBL.lo || nextVBL.hi);

		if (haveVBL && (postDeltaX || postDeltaY))  {
			accumDX += dx;
			accumDY += dy;
			
        }
        else {
			SInt32 num = 0, div = 0;

			dx += accumDX;
			dy += accumDY;

			moveDeltaTime = *((UnsignedWide *)(&ts));
			SUB_ABSOLUTETIME( &moveDeltaTime, &lastRelativeMoveTime );
			lastRelativeMoveTime = ts;
			
			if( (eventDeltaTime.lo < vblDeltaTime.lo) && (0 == eventDeltaTime.hi)
			 && vblDeltaTime.lo && moveDeltaTime.lo) {
				num = vblDeltaTime.lo;
				div = moveDeltaTime.lo;
				dx = ((SInt64)num * dx) / div;
				dy = ((SInt64)num * dy) / div;
			}

			KERNEL_DEBUG(KDBG_CODE(12, 0, 0), dx, dy, num, div, 0);

			accumDX = accumDY = 0;

			if( dx || dy ) {
			
				if (((oldDx < 0) && (dx > 0)) || ((oldDx > 0) && (dx < 0))) {
					IOLog("IOHIDSystem::relativePointerEventGated: Unwanted Direction Change X: oldDx=%d dx=%d\n", oldDx, dx);
				}
				
				
				if (((oldDy < 0) && (dy > 0)) || ((oldDy > 0) && (dy < 0))) {
					IOLog("IOHIDSystem::relativePointerEventGated: Unwanted Direction Change Y: oldDy=%d dy=%d\n", oldDy, dy);
				}
				
				if( haveVBL ) {
					static UInt64 resonableVBL = 0;
					if (!resonableVBL) {
						nanoseconds_to_absolutetime(20000000, (AbsoluteTime*)(&resonableVBL));
					}
					
					// rdar://5565815 Capping VBL interval
					if (AbsoluteTime_to_scalar(&vblDeltaTime) > resonableVBL) {
						static int count = 0;
						if (!(count % 100))
							IOLog("IOHIDSystem::relativePointerEventGated: VBL too high (%lld), capping to %lld\n",
									AbsoluteTime_to_scalar(&vblDeltaTime), resonableVBL);
						count++;
						AbsoluteTime_to_scalar(&vblDeltaTime) = resonableVBL;
					}

					postDeltaX = dx;
					postDeltaY = dy;
					vblDeltaTime.lo += (vblDeltaTime.lo >> 4);
					ADD_ABSOLUTETIME(&nextVBL, &vblDeltaTime);
					vblES->wakeAtTime(*(AbsoluteTime *)&nextVBL);
					lastSender = sender;
				}
				else
				{
					pointerLoc.x += dx;
					pointerLoc.y += dy;
					pointerDelta.x += dx;
					pointerDelta.y += dy;
					_setCursorPosition(&pointerLoc, false, false, sender);
				}
			}
		}
    }
}

void IOHIDSystem::vblEvent(void)
{
    if (postDeltaX || postDeltaY) {
	pointerLoc.x += postDeltaX;
	pointerLoc.y += postDeltaY;
	pointerDelta.x += postDeltaX;
	pointerDelta.y += postDeltaY;
	_setCursorPosition(&pointerLoc, false, false, lastSender);
	postDeltaX = postDeltaY = 0;
    }
}

void IOHIDSystem::_vblEvent(OSObject *self, IOTimerEventSource *sender)
{
    ((IOHIDSystem *)self)->vblEvent();
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
                                void *          refcon)
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
    int        	buttons 	= *(int *)	((IOHIDCmdGateActionArgs *)args)->arg0;
    IOGPoint *  newLoc 		= (IOGPoint *)	((IOHIDCmdGateActionArgs *)args)->arg1;
    IOGBounds * bounds		= (IOGBounds *)	((IOHIDCmdGateActionArgs *)args)->arg2;
    bool       	proximity	= *(bool *)	((IOHIDCmdGateActionArgs *)args)->arg3;
    int        	pressure	= *(int *)	((IOHIDCmdGateActionArgs *)args)->arg4;
    int        	stylusAngle	= *(int *)	((IOHIDCmdGateActionArgs *)args)->arg5;
    AbsoluteTime 	ts	= *(AbsoluteTime *) 	((IOHIDCmdGateActionArgs *)args)->arg6;
    OSObject *  sender          = (OSObject *)((IOHIDCmdGateActionArgs *)args)->arg7;
    
        
    self->absolutePointerEventGated(buttons, newLoc, bounds, proximity, pressure, stylusAngle, ts, sender);
    
    return kIOReturnSuccess;
}

void IOHIDSystem::absolutePointerEventGated(
                                int             buttons,
            /* at */            IOGPoint *      newLoc,
            /* withBounds */    IOGBounds *     bounds,
            /* inProximity */   bool            proximity,
            /* withPressure */  int             pressure,
            /* withAngle */     int             stylusAngle,
            /* atTime */        AbsoluteTime    ts,
            /* sender */        OSObject *      sender)
{

  /*
   * If you don't know what to pass for the following fields, pass the
   * default values below:
   *    pressure    = MINPRESSURE or MAXPRESSURE
   *    stylusAngle = 90
   */

	NXEventData 		outData;	/* dummy data */
        bool			proximityChange = false;
        IOGPoint        pointerFraction;
                
	if ( !eventsOpen )
		return;

    if(ShouldConsumeHIDEvent(ts, stateChangeDeadline))
        return;

    if ( !(displayState & IOPMDeviceUsable) ) {	// display is off, consume the button event
        if ( buttons ) {
            return;
        }

        TICKLE_DISPLAY;
        return;
    }

    TICKLE_DISPLAY;
                
    _scaleLocationToCurrentScreen(newLoc, &pointerFraction, bounds);

    // RY: Attempt to add basic tablet support to absolute pointing devices
    // Basically, we will fill in the tablet support portions of both the
    // mouse and mouseMove of NXEventData.  Pending tablet events are stored
    // in the CachedMouseEventStruct and then later picked off in 
    // _setButtonState and _postMouseMoveEvent
	CachedMouseEventStruct	*cachedMouseEvent = GetCachedMouseEventForService(cachedButtonStates, sender);
    if (cachedMouseEvent)
    {
        bcopy(&pointerFraction, &(cachedMouseEvent->pointerFraction), sizeof(IOGPoint));

        proximityChange = (cachedMouseEvent->proximity != proximity);
        
        cachedMouseEvent->state        |= kCachedMousePointingEventDispFlag;
        cachedMouseEvent->proximity     = proximity;
        cachedMouseEvent->lastPressure  = ScalePressure(pressure);
        absolutetime_to_nanoseconds(ts, &cachedMouseEvent->lastUpdate);

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
                cachedMouseEvent->tabletData.tablet.buttons 	= buttons & ~0x7; 
                if (buttons & 2)
                    cachedMouseEvent->tabletData.tablet.buttons	|= 4;
                if (buttons & EV_RB)
                    cachedMouseEvent->tabletData.tablet.buttons	|= 2;
                if (buttons & EV_LB)
                    cachedMouseEvent->tabletData.tablet.buttons	|= 1;
                
                cachedMouseEvent->tabletData.tablet.x           = newLoc->x;             
                cachedMouseEvent->tabletData.tablet.y           = newLoc->y;                                        
                cachedMouseEvent->tabletData.tablet.pressure    = pressure; 
                cachedMouseEvent->subType                       = NX_SUBTYPE_TABLET_POINT;            
            }
        }
    }

	if ( (newLoc->x != pointerLoc.x) || (newLoc->y != pointerLoc.y) || proximityChange)
	{
        pointerDelta.x += (newLoc->x - pointerLoc.x);
        pointerDelta.y += (newLoc->y - pointerLoc.y);
	    pointerLoc = *newLoc;
            
	    _setCursorPosition(&pointerLoc, false, proximityChange, sender);
    }
	if ( proximityChange && proximity == true )
	{
	    evg->eventFlags |= NX_STYLUSPROXIMITYMASK;
	    bzero( (char *)&outData, sizeof outData );
	    postEvent(         NX_FLAGSCHANGED,
		/* at */       &pointerLoc,
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
		/* at */       &pointerLoc,
		/* atTime */   ts,
		/* withData */ &outData,
        /* sender */   sender);
        }

        // RY: Clean it off
        if (cachedMouseEvent)
        {
            cachedMouseEvent->subType = NX_SUBTYPE_DEFAULT;
            bzero(&cachedMouseEvent->pointerFraction, sizeof(IOGPoint));
        }        
}

void IOHIDSystem::_scrollWheelEvent(IOHIDSystem * self,
                                    short	deltaAxis1,
                                    short	deltaAxis2,
                                    short	deltaAxis3,
                                    IOFixed fixedDelta1,
                                    IOFixed fixedDelta2,
                                    IOFixed fixedDelta3,
                                    SInt32  pointDeltaAxis1,
                                    SInt32  pointDeltaAxis2,
                                    SInt32  pointDeltaAxis3,
                                    UInt32  options,
                 /* atTime */       AbsoluteTime ts,
                                    OSObject * sender,
                                    void *     refcon)
{
        self->scrollWheelEvent(deltaAxis1, deltaAxis2, deltaAxis3, fixedDelta1, fixedDelta2, fixedDelta3,  pointDeltaAxis1, pointDeltaAxis2, pointDeltaAxis3, options, ts, sender);
}

void IOHIDSystem::scrollWheelEvent(short	deltaAxis1,
                                   short	deltaAxis2,
                                   short	deltaAxis3,
                    /* atTime */   AbsoluteTime ts)

{
    scrollWheelEvent(deltaAxis1, deltaAxis2, deltaAxis3, deltaAxis1<<16, deltaAxis2<<16, deltaAxis3<<16, 0, 0, 0, 0, ts, 0);
}

void IOHIDSystem::scrollWheelEvent(short	deltaAxis1,
                                   short	deltaAxis2,
                                   short	deltaAxis3,
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

void IOHIDSystem::scrollWheelEventGated(short	deltaAxis1,
                                        short	deltaAxis2,
                                        short	deltaAxis3,
                                       IOFixed  fixedDelta1,
                                       IOFixed  fixedDelta2,
                                       IOFixed  fixedDelta3,
                                       SInt32   pointDeltaAxis1,
                                       SInt32   pointDeltaAxis2,
                                       SInt32   pointDeltaAxis3,
                                       UInt32   options,
                        /* atTime */   	AbsoluteTime ts,
                                        OSObject * sender)
{
    NXEventData wheelData;
    bool momentum = (options & kScrollTypeMomentumAny) ? true : false;

    if (!eventsOpen)
        return;

    if(ShouldConsumeHIDEvent(ts, stateChangeDeadline))
        return;

    if ((deltaAxis1 == 0) && (deltaAxis2 == 0) && (deltaAxis3 == 0) && 
        (pointDeltaAxis1 == 0) && (pointDeltaAxis2 == 0) && (pointDeltaAxis3 == 0) &&
        !momentum)
    {
        return;
    } 

    TICKLE_DISPLAY;
    
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
    wheelData.scrollWheel.reserved1       = (UInt16)options & (kScrollTypeContinuous | kScrollTypeMomentumAny);
    updateScrollEventForSender(sender, &wheelData);
	if (momentum)
		wheelData.scrollWheel.reserved8[2]    = IOHIDevice::GenerateKey(sender);
            
    postEvent(             (options & kScrollTypeZoom) ? NX_ZOOM : NX_SCROLLWHEELMOVED,
            /* at */       (IOGPoint *)&evg->cursorLoc,
            /* atTime */   ts,
            /* withData */ &wheelData,
            /* sender */   sender);

    return;
}

void IOHIDSystem::_tabletEvent(IOHIDSystem *self,
                               NXEventData *tabletData,
                               AbsoluteTime ts,
                               OSObject * sender,
                               void *     refcon)
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
    NXEventData *tabletData 	= (NXEventData *) arg0;
    AbsoluteTime ts		= *(AbsoluteTime *) arg1;
    OSObject * sender		= (OSObject *) arg2;
    
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

    if(ShouldConsumeHIDEvent(ts, stateChangeDeadline))
        return;

    if ((cachedMouseEvent = GetCachedMouseEventForService(cachedButtonStates, sender)) &&
        !(cachedMouseEvent->state & kCachedMousePointingTabletEventPendFlag))
    {
            
        cachedMouseEvent->state     |= kCachedMouseTabletEventDispFlag;
        cachedMouseEvent->subType   = NX_SUBTYPE_TABLET_POINT;
        absolutetime_to_nanoseconds(ts, &cachedMouseEvent->lastUpdate);
        bcopy( tabletData, &(cachedMouseEvent->tabletData), sizeof(NXEventData));
        
        // Don't dispatch an event if they can be embedded in pointing events
        if ( cachedMouseEvent->state & kCachedMousePointingEventDispFlag )
            return;
    }
                
    postEvent(NX_TABLETPOINTER,
              (IOGPoint *)&evg->cursorLoc,
              ts,
              tabletData,
              sender);

    return;
}

void IOHIDSystem::_proximityEvent(IOHIDSystem *self,
                                  NXEventData *proximityData,
                                  AbsoluteTime ts,
                                  OSObject * sender,
                                  void *     refcon)
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
    
    NXEventData *proximityData	= (NXEventData *)arg0;
    AbsoluteTime ts		= *(AbsoluteTime *)arg1;
    OSObject * sender		= (OSObject *)arg2;
    
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

    if(ShouldConsumeHIDEvent(ts, stateChangeDeadline))
        return;
    
    if ((cachedMouseEvent = GetCachedMouseEventForService(cachedButtonStates, sender)) &&
        !(cachedMouseEvent->state & kCachedMousePointingTabletEventPendFlag))
    {
        cachedMouseEvent->state     |= kCachedMouseTabletEventDispFlag;
        cachedMouseEvent->subType   = NX_SUBTYPE_TABLET_PROXIMITY;
        absolutetime_to_nanoseconds(ts, &cachedMouseEvent->lastUpdate);
        bcopy( proximityData, &(cachedMouseEvent->proximityData), sizeof(NXEventData));
    }

    postEvent(NX_TABLETPROXIMITY,
              (IOGPoint *)&evg->cursorLoc,
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
         /* keyboardType */ 	unsigned   keyboardType,
         /* repeat */           bool       repeat,
         /* atTime */           AbsoluteTime ts,
				OSObject * sender,
				void *     refcon)
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
         /* keyboardType */ 	unsigned   keyboardType,
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
         /* keyboardType */ 	unsigned   keyboardType,
         /* repeat */           bool       repeat,
         /* atTime */           AbsoluteTime ts,
         /* sender */		OSObject * sender)
{
    KeyboardEQElement * keyboardEQElement = (KeyboardEQElement *)IOMalloc(sizeof(KeyboardEQElement));

    if ( !keyboardEQElement )
        return;
        
    bzero(keyboardEQElement, sizeof(KeyboardEQElement));
    
    keyboardEQElement->action   = IOHIDSystem::doKeyboardEvent;
    keyboardEQElement->ts       = ts;
    keyboardEQElement->sender   = sender;
    
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
    OSObject * sender		= keyboardEQElement->sender;
    
    unsigned   eventType	= keyboardEQElement->event.keyboard.eventType;
    unsigned   flags		= keyboardEQElement->event.keyboard.flags;
    unsigned   key          = keyboardEQElement->event.keyboard.key;
    unsigned   charCode		= keyboardEQElement->event.keyboard.charCode;
    unsigned   charSet		= keyboardEQElement->event.keyboard.charSet;
    unsigned   origCharCode	= keyboardEQElement->event.keyboard.origCharCode;
    unsigned   origCharSet	= keyboardEQElement->event.keyboard.origCharSet;
    unsigned   keyboardType	= keyboardEQElement->event.keyboard.keyboardType;
    bool       repeat		= keyboardEQElement->event.keyboard.repeat;
        
    self->keyboardEventGated(eventType, flags, key, charCode, charSet,
				origCharCode, origCharSet, keyboardType, repeat, ts, sender);
                                
    return kIOReturnSuccess;
}

void IOHIDSystem::keyboardEventGated(unsigned   eventType,
                                /* flags */            unsigned   flags,
                                /* keyCode */          unsigned   key,
                                /* charCode */         unsigned   charCode,
                                /* charSet */          unsigned   charSet,
                                /* originalCharCode */ unsigned   origCharCode,
                                /* originalCharSet */  unsigned   origCharSet,
                                /* keyboardType */ 	unsigned   keyboardType,
                                /* repeat */           bool       repeat,
                                /* atTime */           AbsoluteTime ts,
                                /* sender */           OSObject * sender)
{         
    UInt32 consumeCause;
    NXEventData	outData;
    
    consumeCause = ShouldConsumeHIDEvent(ts, stateChangeDeadline);
        
    if (consumeCause == kHIDConsumeCauseKeyLock)
        return;
    else if(consumeCause == kHIDConsumeCauseDeadline)  {
        // deadline key consumption taking place
        if (consumedKeyCode != (unsigned)-1) {
            // a key was stored for later consumption
            if ((consumedKeyCode != key) && (eventType != NX_KEYUP))  {
                AbsoluteTime_to_scalar(&stateChangeDeadline) = 0;
                displayState |= IOPMDeviceUsable;                
                
                goto KEYBOARD_EVENT_PROCESS;
            } else if ((consumedKeyCode == key) && (eventType == NX_KEYUP)) {
                AbsoluteTime_to_scalar(&stateChangeDeadline) = 0;
                displayState |= IOPMDeviceUsable;
                consumedKeyCode = (unsigned)-1;
                
                return;            
            }
        } else if ( eventType == NX_KEYDOWN ) {
            // in theory this is the first key to be consumed
            consumedKeyCode = key;
            return;
        }
    }
    consumedKeyCode = (unsigned)-1;

KEYBOARD_EVENT_PROCESS:
    
    if ( ! (displayState & IOPMDeviceUsable) ) {	// display is off, consume the keystroke
        if ( eventType == NX_KEYDOWN ) {
            return;        
        } else if ( eventType == NX_KEYUP ) {
            TICKLE_DISPLAY;
            return;
        }
    }

    TICKLE_DISPLAY;

    // RY: trigger NMI for CMD-OPT-CTRL-ALT-ESC
    if( !repeat && (key == 0x35) &&
		(eventType == NX_KEYDOWN) &&
        ((flags & NORMAL_MODIFIER_MASK) == NORMAL_MODIFIER_MASK))
    {
        PE_enter_debugger("USB Programmer Key");
    }

    // RY: trigger stack shot for CMD-OPT-CTRL-ALT-PERIOD
    if( !repeat && ((key == 0x41) || (key == 0x2f)) &&
        ((flags & NORMAL_MODIFIER_MASK) == NORMAL_MODIFIER_MASK))
    {
        if (eventType == NX_KEYDOWN)
            sendStackShotMessage();
		
        return;
    }

	if ( eventsOpen ) {
        outData.key.repeat = repeat;
        outData.key.keyCode = key;
        outData.key.charSet = charSet;
        outData.key.charCode = charCode;
        outData.key.origCharSet = origCharSet;
        outData.key.origCharCode = origCharCode;
        outData.key.keyboardType = keyboardType;

        evg->eventFlags = (evg->eventFlags & ~KEYBOARD_FLAGSMASK)
                | (flags & KEYBOARD_FLAGSMASK);
                            
        if (cachedEventFlags != evg->eventFlags) {
            cachedEventFlags = evg->eventFlags;

            // RY: Reset the clickTime as well on modifier
            // change to prevent double click from occuring
            nanoseconds_to_absolutetime(0, &clickTime);
        }
        
        postEvent(         eventType,
            /* at */       &pointerLoc,
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
                       /* flags */     	  unsigned   flags,
                       /* keyCode  */  	  unsigned   key,
                       /* specialty */ 	  unsigned   flavor,
                       /* guid */         UInt64     guid,
                       /* repeat */       bool       repeat,
                       /* atTime */    	  AbsoluteTime ts,
					  OSObject * sender,
					  void *     refcon)
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
	NXEventData	outData;
	int		level = -1;

    if(ShouldConsumeHIDEvent(ts, stateChangeDeadline))
        return;

    TICKLE_DISPLAY;

    // Since the HIDSystem will now take on BSD Console duty,
    // we need to make sure to process the programmer key info
    // prior to doing the eventsOpen check
    if ( eventType == NX_KEYDOWN && flavor == NX_POWER_KEY && !repeat) {
        if ( (flags & NORMAL_MODIFIER_MASK) == NX_COMMANDMASK )
            PE_enter_debugger("USB Programmer Key");
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
							
				//	Special key handlers:
				//
				//	Command = invoke macsbug
				//	Command+option = sleep now
				//	Command+option+control = shutdown now
				//	Control = logout dialog
				
				if(  (evg->eventFlags & NX_COMMANDMASK) 	&&
					!(evg->eventFlags & NX_CONTROLMASK) 	&& 
					!(evg->eventFlags & NX_SHIFTMASK)		&& 
					!(evg->eventFlags & NX_ALTERNATEMASK)		)
				{
					// Post a power key event, Classic should pick this up and
					// drop into MacsBug.
					//
					outData.compound.subType = NX_SUBTYPE_POWER_KEY;
					postEvent(	   NX_SYSDEFINED,
					/* at */       &pointerLoc,
					/* atTime */   ts,
					/* withData */ &outData,
                    /* sender */   sender,
                    /* extPID */   0,
                    /* processKEQ*/false);
				}
				else if(   (evg->eventFlags & NX_COMMANDMASK) 	&&
						  !(evg->eventFlags & NX_CONTROLMASK) 	&& 
						  !(evg->eventFlags & NX_SHIFTMASK)		&& 
						   (evg->eventFlags & NX_ALTERNATEMASK)		)
				{					
					//IOLog( "IOHIDSystem -- sleep now!\n" );

					// Post the sleep now event. Someone else will handle the actual call.
					//
					outData.compound.subType = NX_SUBTYPE_SLEEP_EVENT;
					postEvent(	   NX_SYSDEFINED,
					/* at */       &pointerLoc,
					/* atTime */   ts,
					/* withData */ &outData,
                    /* sender */   sender,
                    /* extPID */   0,
                    /* processKEQ*/false);

				}
				else if(   (evg->eventFlags & NX_COMMANDMASK) 	&&
						   (evg->eventFlags & NX_CONTROLMASK) 	&& 
						  !(evg->eventFlags & NX_SHIFTMASK)		&& 
						   (evg->eventFlags & NX_ALTERNATEMASK)		)
				{					
					//IOLog( "IOHIDSystem -- shutdown now!\n" );

					// Post the shutdown now event. Someone else will handle the actual call.
					//
					outData.compound.subType = NX_SUBTYPE_SHUTDOWN_EVENT;
					postEvent(	   NX_SYSDEFINED,
					/* at */       &pointerLoc,
					/* atTime */   ts,
					/* withData */ &outData,
                    /* sender */   sender,
                    /* extPID */   0,
                    /* processKEQ*/false);

				}
				else if(   (evg->eventFlags & NX_COMMANDMASK) 	&&
						   (evg->eventFlags & NX_CONTROLMASK) 	&& 
						  !(evg->eventFlags & NX_SHIFTMASK)		&& 
						  !(evg->eventFlags & NX_ALTERNATEMASK)		)
				{
					// Restart now!
					//IOLog( "IOHIDSystem -- Restart now!\n" );
					
					// Post the Restart now event. Someone else will handle the actual call.
					//
					outData.compound.subType = NX_SUBTYPE_RESTART_EVENT;
					postEvent(	   NX_SYSDEFINED,
					/* at */       &pointerLoc,
					/* atTime */   ts,
					/* withData */ &outData,
                    /* sender */   sender,
                    /* extPID */   0,
                    /* processKEQ*/false);

				}
				else if(  !(evg->eventFlags & NX_COMMANDMASK) 	&&
						   (evg->eventFlags & NX_CONTROLMASK) 	&& 
						  !(evg->eventFlags & NX_SHIFTMASK)		&& 
						  !(evg->eventFlags & NX_ALTERNATEMASK)		)
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
					postEvent(	   NX_SYSDEFINED,
					/* at */       &pointerLoc,
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
					postEvent(	   NX_SYSDEFINED,
					/* at */       &pointerLoc,
					/* atTime */   ts,
					/* withData */ &outData,
                    /* sender */   sender,
                    /* extPID */   0,
                    /* processKEQ*/false);

				}
				break;

			case NX_POWER_KEY:
				outData.compound.subType = NX_SUBTYPE_POWER_KEY;
				postEvent(         NX_SYSDEFINED,
					/* at */       &pointerLoc,
					/* atTime */   ts,
					/* withData */ &outData,
                    /* sender */   sender,
                    /* extPID */   0,
                    /* processKEQ*/false);

				break;
		}
	}
#if 0	/* So far, nothing to do on keyup */
	else if ( eventType == NX_KEYUP )
	{
		switch ( flavor )
		{
			case NX_KEYTYPE_SOUND_UP:
				break;
			case NX_KEYTYPE_SOUND_DOWN:
				break;
			case NX_KEYTYPE_MUTE:
				break;
			case NX_POWER_KEY:
				break;
		}
	}
#endif
	// if someone pases a sysdefined type in, then its ready to go already
	else if ( eventType == NX_SYSDEFINED )
	{
		outData.compound.subType = flavor;
		postEvent(         NX_SYSDEFINED,
			/* at */       &pointerLoc,
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
                    /* at */       &pointerLoc,
                    /* atTime */   ts,
                    /* withData */ &outData,
                    /* sender */   sender,
                    /* extPID */   0,
                    /* processKEQ*/false);

          }
	}

	if ( level != -1 )	// An interesting special key event occurred
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
                                    void *        refcon)
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
    unsigned   flags	= keyboardEQElement->event.flagsChanged.flags;
    
    self->updateEventFlagsGated(flags, sender);
    
    return kIOReturnSuccess;
}

void IOHIDSystem::updateEventFlagsGated(unsigned flags, OSObject * sender)
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
//	Update the button state.  Generate button events as needed
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
    UInt64 ts_nano;
    absolutetime_to_nanoseconds(ts, &ts_nano);
    
    if ( cachedButtonStates ) {
        cachedMouseEvent = GetCachedMouseEventForService(cachedButtonStates, sender);
        
        if (cachedMouseEvent) {
            cachedMouseEvent->lastButtons = buttons;
            cachedMouseEvent->lastUpdate = ts_nano;
        }
        
        if (evg->buttons == buttons)
            return;
            
        buttons = GetCachedMouseButtonStates(cachedButtonStates, ts_nano, mouseButtonTimeout);
    }
    // *** END HACK ***
    
    // Once again check if new button state differs
    if (evg->buttons == buttons)
        return;
        
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
                /* at */ (IOGPoint *)&evg->cursorLoc,
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
        evData.mouse.subx = (cachedMouseEvent->pointerFraction.x >> 8) & 0xff;
        evData.mouse.suby = (cachedMouseEvent->pointerFraction.y >> 8) & 0xff;
        evData.mouse.pressure = cachedMouseEvent->lastPressure;
        
        if (cachedMouseEvent->subType == NX_SUBTYPE_TABLET_POINT) {
            bcopy(&(cachedMouseEvent->tabletData), &(evData.mouse.tablet.point), sizeof(NXTabletPointData));
        }
        else if (cachedMouseEvent->subType == NX_SUBTYPE_TABLET_PROXIMITY) {
            bcopy(&(cachedMouseEvent->proximityData), &(evData.mouse.tablet.proximity), sizeof(NXTabletProximityData));
        }
    }
    
    if ((evg->buttons & EV_LB) != (buttons & EV_LB)) {
        if (buttons & EV_LB) {
            postEvent(             NX_LMOUSEDOWN,
                                   /* at */       (IOGPoint *)&evg->cursorLoc,
                                   /* atTime */   ts,
                                   /* withData */ &evData,
                                   /* sender */   sender);
        }
        else {
            postEvent(             NX_LMOUSEUP,
                                   /* at */       (IOGPoint *)&evg->cursorLoc,
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
                                   /* at */       (IOGPoint *)&evg->cursorLoc,
                                   /* atTime */   ts,
                                   /* withData */ &evData,
                                   /* sender */   sender);
        }
        else {
            postEvent(             NX_RMOUSEUP,
                                   /* at */       (IOGPoint *)&evg->cursorLoc,
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
            pointerDelta.x += (newLoc->x - pointerLoc.x);
            pointerDelta.y += (newLoc->y - pointerLoc.y);
	    pointerLoc = *newLoc;
	    _setCursorPosition(newLoc, external, false, sender);
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
void IOHIDSystem::_setCursorPosition(IOGPoint * newLoc, bool external, bool proximityChange, OSObject * sender)
{
        bool cursorMoved = true;
    
	if (!screens)
	    return;
            
	if( ev_try_lock(&evg->cursorSema) == 0 ) // host using shmem
	{
		needSetCursorPosition = true;	  // try again later
//		scheduleNextPeriodicEvent();
		return;
	}

	// Past here we hold the cursorSema lock.  Make sure the lock is
	// cleared before returning or the system will be wedged.
	
	needSetCursorPosition = false;	  // We WILL succeed

        if (cursorCoupled || external)
        {
            UInt32 newScreens = 0;
            SInt32 pinScreen = -1L;
            EvScreen *screen = (EvScreen *)evScreen;

            if (!cursorPinned) {
                /* Get mask of screens on which the cursor is present */
                for (int i = 0; i < screens; i++ ) {
                    if ((screen[i].instance) && PtInRect(newLoc, screen[i].bounds)) {
                        pinScreen = i;
                        newScreens |= (1 << i);
                    }
                }
            }

            if (newScreens == 0) {
                /* At this point cursor has gone off all screens,
                   just clip it to one of the previous screens. */
                newLoc->x = (newLoc->x < cursorPin.minx) ?
                    cursorPin.minx : ((newLoc->x > cursorPin.maxx) ?
                    cursorPin.maxx : newLoc->x);
                newLoc->y = (newLoc->y < cursorPin.miny) ?
                    cursorPin.miny : ((newLoc->y > cursorPin.maxy) ?
                    cursorPin.maxy : newLoc->y);
                /* regenerate mask for new position */
                for (int i = 0; i < screens; i++ ) {
                    if ((screen[i].instance) && PtInRect(newLoc, screen[i].bounds)) {
                        pinScreen = i;
                        newScreens |= (1 << i);
                    }
                }
            }
    
            pointerLoc = *newLoc;	// Sync up pointer with clipped cursor
            /* Catch the no-move case */
            if ((evg->cursorLoc.x == newLoc->x) && (evg->cursorLoc.y == newLoc->y)) {
                if ((proximityChange == 0) && (pointerDelta.x == 0) && (pointerDelta.y == 0)) {
                    ev_unlock(&evg->cursorSema);
                    return;
                }
                cursorMoved = false;	// mouse moved, but cursor didn't
            } else {
                evg->cursorLoc.x = newLoc->x;
                evg->cursorLoc.y = newLoc->y;
    
                /* If cursor changed screens */
                if (newScreens != cursorScreens) {
                    hideCursor();	/* hide cursor on old screens */
                    cursorScreens = newScreens;
                    cursorPin = *(((EvScreen*)evScreen)[pinScreen].bounds);
                    cursorPin.maxx--;	/* Make half-open rectangle */
                    cursorPin.maxy--;
                    cursorPinScreen = pinScreen;
                    showCursor();
                } else {
                    /* cursor moved on same screens */
                    moveCursor();
                }
            }
        } else {
            /* cursor uncoupled */
            pointerLoc.x = evg->cursorLoc.x;
            pointerLoc.y = evg->cursorLoc.y;
        }

        AbsoluteTime	ts;
        clock_get_uptime(&ts);
        
	/* See if anybody wants the mouse moved or dragged events */
	// Note: extPostEvent clears evg->movedMask as a hack to prevent these events
	// so any change here should check to make sure it does not break that hack
	if (evg->movedMask) {
            if ((evg->movedMask&NX_LMOUSEDRAGGEDMASK)&&(evg->buttons& EV_LB)) {
                _postMouseMoveEvent(NX_LMOUSEDRAGGED, newLoc, ts, sender);
            } else if ((evg->movedMask&NX_RMOUSEDRAGGEDMASK) && (evg->buttons & EV_RB)) {
                _postMouseMoveEvent(NX_RMOUSEDRAGGED, newLoc, ts, sender);
            } else if (evg->movedMask & NX_MOUSEMOVEDMASK) {
                _postMouseMoveEvent(NX_MOUSEMOVED, newLoc, ts, sender);
            }
	}
    
	/* check new cursor position for leaving evg->mouseRect */
	if (cursorMoved && evg->mouseRectValid && (!PtInRect(newLoc, &evg->mouseRect)))
	{
	    if (evg->mouseRectValid)
	    {
		postEvent(             NX_MOUSEEXITED,
			/* at */       newLoc,
			/* atTime */   ts,
			/* withData */ NULL,
            /* sender */   sender);
		evg->mouseRectValid = 0;
	    }
	}
	ev_unlock(&evg->cursorSema);
}

void IOHIDSystem::_postMouseMoveEvent(int          what,
                                     IOGPoint *    location,
                                     AbsoluteTime  ts,
                                     OSObject *    sender)
{
    NXEventData data;
    CachedMouseEventStruct *cachedMouseEvent = 0;

    bzero( &data, sizeof(data) );

    data.mouseMove.dx = pointerDelta.x;
    data.mouseMove.dy = pointerDelta.y;

    pointerDelta.x = 0;
    pointerDelta.y = 0;

    // vtn3: This will update the data structure for touch devices
    updateMouseMoveEventForSender(sender, &data);
    
    // RY: Roll in the tablet info we got from absolutePointerEventGated
    // into the mouseMove event.
    if (sender && (cachedMouseEvent = GetCachedMouseEventForService(cachedButtonStates, sender)))
    {
        if (data.mouse.subType != NX_SUBTYPE_MOUSE_TOUCH)
            data.mouseMove.subType = cachedMouseEvent->subType;
        data.mouseMove.subx = (cachedMouseEvent->pointerFraction.x >> 8) & 0xff;
        data.mouseMove.suby = (cachedMouseEvent->pointerFraction.y >> 8) & 0xff;
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
    postEvent(what, location, ts, &data, sender);
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
    task_t         owningTask	= *(task_t *) ((IOHIDCmdGateActionArgs *)args)->arg0;
    void *         security_id	= ((IOHIDCmdGateActionArgs *)args)->arg1;
    UInt32         type         = *(UInt32 *) ((IOHIDCmdGateActionArgs *)args)->arg2;
    OSDictionary * properties   = (OSDictionary *) ((IOHIDCmdGateActionArgs *)args)->arg3;
    IOUserClient ** handler 	= (IOUserClient **) ((IOHIDCmdGateActionArgs *)args)->arg4;
    
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
        else
            err = kIOReturnUnsupported;
            
        if ( !newConnect)
            continue;
            
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
        
    return cmdGate->runAction((IOCommandGate::Action)doSetCursorEnable, p1);
    
}

IOReturn IOHIDSystem::doSetCursorEnable(IOHIDSystem *self, void * arg0)
                        /* IOCommandGate::Action */
{
    return self->setCursorEnableGated(arg0);
}

IOReturn IOHIDSystem::setCursorEnableGated(void* p1)
{
    bool 		enable = (bool)p1;
    IOReturn		err = kIOReturnSuccess;

    if ( eventsOpen == false )
        return kIOReturnNotOpen;



    if( 0 == screens)	// Should be at least 1!
        return kIOReturnNoDevice;


    if( enable) {
	if( cursorStarted) {
            hideCursor();
            cursorEnabled = resetCursor();
            showCursor();
	} else
            cursorEnabled = startCursor();
    } else
        cursorEnabled = enable;

    cursorCoupled = cursorEnabled;
    
    return err;
}

IOReturn IOHIDSystem::extSetBounds( IOGBounds * bounds )
{
    if( bounds->minx != bounds->maxx) {
        cursorPin = *bounds;
        cursorPin.maxx--;	/* Make half-open rectangle */
        cursorPin.maxy--;
        cursorPinned = true;
    } else
        cursorPinned = false;

    return( kIOReturnSuccess );
}

IOReturn IOHIDSystem::extPostEvent(void*p1,void*p2,void*,void*,void*,void*)
{                                                                    // IOMethod    
    AbsoluteTime	ts;

    clock_get_uptime(&ts);

    return cmdGate->runAction((IOCommandGate::Action)doExtPostEvent, p1, p2, &ts);
}

IOReturn IOHIDSystem::doExtPostEvent(IOHIDSystem *self, void * arg0, void * arg1, void * arg2, void * arg3)
                        /* IOCommandGate::Action */
{
    return self->extPostEventGated(arg0, arg1, arg2);
}

IOReturn IOHIDSystem::extPostEventGated(void *p1,void *p2, void *p3)
{
    struct evioLLEvent * event      = (struct evioLLEvent *)p1;
    UInt32      count               = (uintptr_t) p2;
    bool        isMoveOrDragEvent 	= false;
    bool        isSeized            = false;
    int         oldMovedMask		= 0;
    int         extPID              = (count > sizeof(evioLLEvent)) ? *(int *)((struct evioLLEvent *)(event + 1)) : 0;
    UInt32      buttonState         = 0;
    UInt32      newFlags            = 0;
    AbsoluteTime ts                 = *(AbsoluteTime *)p3;
	CachedMouseEventStruct *cachedMouseEvent = NULL;        

    if ( eventsOpen == false )
        return kIOReturnNotOpen;

    if(ShouldConsumeHIDEvent(ts, stateChangeDeadline, false))
        return kIOReturnSuccess;

    TICKLE_DISPLAY;

	// used in set cursor below
	if (EventCodeMask(event->type) & MOVEDEVENTMASK)
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
    else if ((EventCodeMask(event->type) & MOUSEEVENTMASK) && 
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
                pointerLoc.x += event->data.mouseMove.dx;
                pointerLoc.y += event->data.mouseMove.dy;
                pointerDelta.x += event->data.mouseMove.dx;
                pointerDelta.y += event->data.mouseMove.dy;

                _setCursorPosition(&pointerLoc, false);
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
                if ((screen[i].instance) && PtInRect(&event->location, screen[i].bounds)) {
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

    if ((EventCodeMask(event->type) & (NX_LMOUSEDOWNMASK | NX_RMOUSEDOWNMASK | NX_LMOUSEUPMASK | NX_RMOUSEUPMASK)) ||
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
            absolutetime_to_nanoseconds(ts, &cachedMouseEvent->lastUpdate);

            evg->buttons = GetCachedMouseButtonStates(cachedButtonStates, cachedMouseEvent->lastUpdate, mouseButtonTimeout);
        }
    }

    if( event->setFlags & kIOHIDSetGlobalEventFlags)
    {
        newFlags = evg->eventFlags = (evg->eventFlags & ~KEYBOARD_FLAGSMASK)
                        | (event->flags & KEYBOARD_FLAGSMASK);
    }
    
    if ( event->setFlags & kIOHIDPostHIDManagerEvent )
    {
        if ((EventCodeMask(event->type) & (MOUSEEVENTMASK | MOVEDEVENTMASK | NX_SCROLLWHEELMOVEDMASK)) &&
            (_hidPointingDevice || (_hidPointingDevice = IOHIDPointingDevice::newPointingDeviceAndStart(this, 8, 400, true, 2))))
        {
            SInt32  dx = 0;
            SInt32  dy = 0;
            SInt32  wheel = 0;
            buttonState = 0;
            
            if (EventCodeMask(event->type) & MOVEDEVENTMASK)
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

        if ((EventCodeMask(event->type) & (NX_KEYDOWNMASK | NX_KEYUPMASK | NX_FLAGSCHANGEDMASK)) &&
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
                /* at */       &event->location,
                /* atTime */   ts,
                /* withData */ &event->data,
                /* sender */   0,
                /* extPID */   extPID);
    }

    return kIOReturnSuccess;
}


IOReturn IOHIDSystem::extSetMouseLocation(void*p1,void*,void*,void*,void*,void*)
{                                                                    // IOMethod 
    return cmdGate->runAction((IOCommandGate::Action)doExtSetMouseLocation, p1);
}

IOReturn IOHIDSystem::doExtSetMouseLocation(IOHIDSystem *self, void * arg0)
                        /* IOCommandGate::Action */
{
    return self->extSetMouseLocationGated(arg0);
}

IOReturn IOHIDSystem::extSetMouseLocationGated(void *p1)
{    
    IOGPoint * loc = (IOGPoint *)p1;

    setCursorPosition(loc, true);
    return kIOReturnSuccess;
}

IOReturn IOHIDSystem::extGetModifierLockState(void*p1,void*p2,void*p3,void*,void*,void*)
{                                                                    // IOMethod
    return cmdGate->runAction((IOCommandGate::Action)doExtGetToggleState, p1, p2);
}

IOReturn IOHIDSystem::extSetModifierLockState(void*p1,void*p2,void*p3,void*,void*,void*)
{                                                                    // IOMethod
    return cmdGate->runAction((IOCommandGate::Action)doExtSetToggleState, p1, p2);
}

IOReturn IOHIDSystem::doExtGetToggleState(IOHIDSystem *self, void *p1, void *p2)
/* IOCommandGate::Action */
{
    unsigned int selector = (uintptr_t)p1;
    unsigned int *state_O = (unsigned int*)p2;
    switch (selector) {
        case kIOHIDCapsLockState:
            return self->getCapsLockState(state_O);
            
        case kIOHIDNumLockState:
            return self->getNumLockState(state_O);
    }
    return kIOReturnBadArgument;
}

IOReturn IOHIDSystem::doExtSetToggleState(IOHIDSystem *self, void *p1, void *p2)
/* IOCommandGate::Action */
{
    unsigned int selector = (uintptr_t)p1;
    unsigned int state_I = (uintptr_t)p2;
    switch (selector) {
        case kIOHIDCapsLockState:
            return self->setCapsLockState(state_I);
            
        case kIOHIDNumLockState:
            return self->setNumLockState(state_I);
    }
    return kIOReturnBadArgument;
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
    OSNumber *	numberRef;
    numberRef = OSNumber::withNumber(number, bits);
    
    if( numberRef) {
        dict->setObject( key, numberRef);
        numberRef->release();
    }
}

void IOHIDSystem::makeInt32ArrayParamProperty( OSDictionary * dict, const char * key,
                            UInt32 * intArray, unsigned int count )
{
    OSArray *	array;
    OSNumber *	number;

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
    UInt64	nano;
    IOFixed	fixed;
    UInt32	int32;

	savedParameters->setObject(kIOHIDDefaultParametersKey, kOSBooleanTrue);

    nano = EV_DCLICKTIME;
    makeNumberParamProperty( savedParameters, kIOHIDClickTimeKey,
                nano, 64 );

    UInt32	tempClickSpace[] = {clickSpaceThresh.x, clickSpaceThresh.y};
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
                
    // set eject delay property initially to 250 ms
    int32 = 250;
    makeNumberParamProperty( savedParameters, kIOHIDF12EjectDelayKey,
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
}

bool IOHIDSystem::_idleTimeSerializerCallback(void * target, void * ref, OSSerialize *s)
{
    IOHIDSystem *   self = (IOHIDSystem *) target;
    AbsoluteTime	currentTime;
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

IOReturn IOHIDSystem::setProperties( OSObject * properties )
{
    OSDictionary *	dict;
    IOReturn		err = kIOReturnSuccess;
    IOReturn		ret;

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
    IOReturn		ret     = kIOReturnSuccess;
    IOReturn		err     = kIOReturnSuccess;
    
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
    
    if ( OSDynamicCast(IOHIDevice, service) )
        deviceParameters = OSDynamicCast(OSDictionary, service->getProperty(kIOHIDParametersKey));
    else if ( OSDynamicCast( IOHIDEventService, service ) )
        deviceParameters = OSDynamicCast(OSDictionary, service->getProperty(kIOHIDEventServicePropertiesKey));
        
    OSCollectionIterator * iterator = OSCollectionIterator::withCollection(dict);
    if ( iterator ) {
        OSSymbol * key;
        while (NULL != (key = (OSSymbol*)iterator->getNextObject())) {
            if ( !deviceParameters || !deviceParameters->getObject(key) )
               validParameters->setObject(key, dict->getObject(key));
        }
        iterator->release();
    }
    
    if ( validParameters->getCount() == 0 ) {
        validParameters->release();
        validParameters = NULL;
    } else 
        validParameters->setObject(kIOHIDDefaultParametersKey, kOSBooleanTrue);
    
    return validParameters;
}


IOReturn IOHIDSystem::doSetParamPropertiesPre(IOHIDSystem *self, void * arg0, void * arg1)
                        /* IOCommandGate::Action */
{    
    return self->setParamPropertiesPreGated((OSDictionary *)arg0, (OSIterator**)arg1);
}

IOReturn IOHIDSystem::setParamPropertiesPreGated( OSDictionary * dict, OSIterator ** pOpenIter)
{
    OSArray *	array;
    OSNumber *	number;
	
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
		
    if( (number = OSDynamicCast( OSNumber, dict->getObject(kIOHIDSystemMouseButtonTimeout))))
    {
        mouseButtonTimeout = number->unsigned64BitValue();
    }
    
    if( (number = OSDynamicCast( OSNumber, dict->getObject(kIOHIDClickTimeKey))))
    {
        UInt64	nano = number->unsigned64BitValue();
        nanoseconds_to_absolutetime(nano, &clickTimeThresh);
    }
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

    if( (number = OSDynamicCast( OSNumber, dict->getObject(kIOHIDWaitCursorFrameIntervalKey))))
        clock_interval_to_absolutetime_interval(number->unsigned32BitValue(), kNanosecondScale,
                                                &waitFrameRate);

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
        struct evioLLEvent		event;
        AbsoluteTime 		ts;
        // clear event record
        bzero( (void *)&event, sizeof event);
        
        event.data.compound.subType = NX_SUBTYPE_HIDPARAMETER_MODIFIED;
        clock_get_uptime(&ts);
        postEvent(NX_SYSDEFINED, &(event.location), ts, &(event.data));
        
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
