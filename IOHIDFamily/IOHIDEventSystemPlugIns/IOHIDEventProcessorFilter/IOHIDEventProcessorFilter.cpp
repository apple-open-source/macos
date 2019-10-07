/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2016 Apple Computer, Inc.  All Rights Reserved.
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

#include "IOHIDEventProcessorFilter.hpp"
#include <IOKit/hid/IOHIDEventTypes.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDPrivateKeys.h>
#include <IOKit/hid/IOHIDLibPrivate.h>
#include <IOKit/hid/IOHIDEventData.h>
#include "IOHIDDebug.h"
#include "CF.h"

#include <mach/mach.h>
#include <mach/mach_time.h>

#include <new>

#define MAX_BUTTON_EVENTS               11
#define MAX_TAP_EVENTS                  1
#define KEY_CREATE(usagePage, usage)    (((UInt64)(usagePage)<<32) | (usage))
#define KEY_GETUSAGEPAGE(key)           (UInt32)((key) >> 32)
#define KEY_GETUSAGE(key)               (UInt32)(key & 0xFFFFFFFF)
#define STATE_IS_DOWN(state)            ((state == kKPStateFirstDown) || \
                                            (state == kKPStateSecondDown) || \
                                            (state == kKPStateThirdDown) || \
                                            (state == kKPStateLongPress))


//7DCF18B5-07BE-4FF5-87CF-44B3C17C9216
#define kIOHIDEventProcessorFilterFactory CFUUIDGetConstantUUIDWithBytes(kCFAllocatorSystemDefault, \
   0x7D, 0xCF, 0x18, 0xB5, 0x07, 0xBE, 0x4F, 0xF5, 0x87, 0xCF, 0x44, 0xB3, 0xC1, 0x7C, 0x92, 0x16)

#define AbsoluteToUS(abs) ((abs) * sEventProcessorTimebaseInfo.numer / sEventProcessorTimebaseInfo.denom / NSEC_PER_USEC)
#define DeltaInUS(last, prev) AbsoluteToUS(last - prev)

#define PastDeadline(current, reference, deadline) (DeltaInUS(current, reference) > deadline)

extern "C" void * IOHIDEventProcessorFactory(CFAllocatorRef allocator, CFUUIDRef typeUUID);

static mach_timebase_info_data_t    sEventProcessorTimebaseInfo;

static bool isDownEvent(IOHIDEventRef event);

//------------------------------------------------------------------------------
// IOHIDEventProcessorFilterFactory
//------------------------------------------------------------------------------

void *IOHIDEventProcessorFactory(CFAllocatorRef allocator __unused, CFUUIDRef typeUUID)
{
    if (CFEqual(typeUUID, kIOHIDServiceFilterPlugInTypeID)) {
        void *p = CFAllocatorAllocate(kCFAllocatorDefault, sizeof(IOHIDEventProcessor), 0);
        return new(p) IOHIDEventProcessor(kIOHIDEventProcessorFilterFactory);
    }
    
    return NULL;
}


// The IOHIDEventProcessor function table.
IOHIDServiceFilterPlugInInterface IOHIDEventProcessor::sIOHIDEventProcessorFtbl =
{
    // Required padding for COM
    NULL,
    // These three are the required COM functions
    IOHIDEventProcessor::QueryInterface,
    IOHIDEventProcessor::AddRef,
    IOHIDEventProcessor::Release,
    // IOHIDSimpleServiceFilterPlugInInterface functions
    IOHIDEventProcessor::match,
    IOHIDEventProcessor::filter,
    IOHIDEventProcessor::filterCopyEvent,
    // IOHIDServiceFilterPlugInInterface functions
    IOHIDEventProcessor::open,
    NULL,
    IOHIDEventProcessor::scheduleWithDispatchQueue,
    IOHIDEventProcessor::unscheduleFromDispatchQueue,
    IOHIDEventProcessor::copyPropertyForClient,
    IOHIDEventProcessor::setPropertyForClient,
    NULL,
    IOHIDEventProcessor::setEventCallback,
};


//------------------------------------------------------------------------------
// IOHIDEventProcessor::IOHIDEventProcessor
//------------------------------------------------------------------------------

IOHIDEventProcessor::IOHIDEventProcessor(CFUUIDRef factoryID)
:
_serviceInterface(&sIOHIDEventProcessorFtbl),
_factoryID( static_cast<CFUUIDRef>( CFRetain(factoryID) ) ),
_refCount(1),
_matchScore(0),
_queue(0),
_service(0),
_eventCallback(0),
_eventTarget(0),
_eventContext(0),
_multiPressTrackingEnabled(0),
_multiPressUsagePairs(NULL),
_multiPressDoublePressTimeout(0),
_multiPressTriplePressTimeout(0),
_multiTapTrackingEnabled(0),
_multiTapDoubleTapTimeout(0),
_multiTapTripleTapTimeout(0),
_longPressTimeout(0),
_eventHead(0),
_freeButtonHead(0),
_freeTapHead(0)
{
    _timer = new Timer;
    
    CFPlugInAddInstanceForFactory( factoryID );
    
    if (sEventProcessorTimebaseInfo.denom == 0) {
        (void) mach_timebase_info(&sEventProcessorTimebaseInfo);
    }
}


//------------------------------------------------------------------------------
// IOHIDEventProcessor::~IOHIDEventProcessor
//------------------------------------------------------------------------------

IOHIDEventProcessor::~IOHIDEventProcessor()
{
    if (_multiPressUsagePairs) {
        CFRelease(_multiPressUsagePairs);
    }
    
    Event * e = _freeButtonHead;
    Event * p = 0;
    
    while (e) {
        p = e->getNextEvent();
        delete e;
        e = p;
    }
    
    e = _freeTapHead;
    
    while (e) {
        p = e->getNextEvent();
        delete e;
        e = p;
    }
    
    e = _eventHead;
    
    while (e) {
        p = e->getNextEvent();
        delete e;
        e = p;
    }
    
    delete _timer;
    
    CFPlugInRemoveInstanceForFactory( _factoryID );
    CFRelease( _factoryID );
}


//------------------------------------------------------------------------------
// IOHIDEventProcessor::QueryInterface
//------------------------------------------------------------------------------

HRESULT IOHIDEventProcessor::QueryInterface( void *self, REFIID iid, LPVOID *ppv )
{
    return static_cast<IOHIDEventProcessor *>(self)->QueryInterface(iid, ppv);
}

// Implementation of the IUnknown QueryInterface function.
HRESULT IOHIDEventProcessor::QueryInterface( REFIID iid, LPVOID *ppv )
{
    // Create a CoreFoundation UUIDRef for the requested interface.
    CFUUIDRef interfaceID = CFUUIDCreateFromUUIDBytes( NULL, iid );
        
    // Test the requested ID against the valid interfaces.
    if (CFEqual(interfaceID, kIOHIDSimpleServiceFilterPlugInInterfaceID) || CFEqual(interfaceID, kIOHIDServiceFilterPlugInInterfaceID)) {
        AddRef();
        *ppv = this;
        CFRelease(interfaceID);
        return S_OK;
    }
    if (CFEqual(interfaceID, IUnknownUUID)) {
        // If the IUnknown interface was requested, same as above.
        AddRef();
        *ppv = this;
        CFRelease(interfaceID);
        return S_OK;
    }
    // Requested interface unknown, bail with error.
    *ppv = NULL;
    CFRelease( interfaceID );
    return E_NOINTERFACE;
}


//------------------------------------------------------------------------------
// IOHIDEventProcessor::AddRef
//------------------------------------------------------------------------------

ULONG IOHIDEventProcessor::AddRef( void *self )
{
    return static_cast<IOHIDEventProcessor *>(self)->AddRef();
}

ULONG IOHIDEventProcessor::AddRef()
{
    // Should probably use atomic operations for refcount
    _refCount += 1;
    return _refCount;
}


//------------------------------------------------------------------------------
// IOHIDEventProcessor::Release
//------------------------------------------------------------------------------

ULONG IOHIDEventProcessor::Release( void *self )
{
    return static_cast<IOHIDEventProcessor *>(self)->Release();
}

ULONG IOHIDEventProcessor::Release()
{    
    _refCount -= 1;
    if (_refCount == 0) {
        delete this;
        return 0;
    }
    return _refCount;
}


static CFStringRef PropertyList [] = {
    CFSTR(kIOHIDKeyboardPressCountTrackingEnabledKey),
    CFSTR(kIOHIDKeyboardPressCountUsagePairsKey),
    CFSTR(kIOHIDKeyboardPressCountDoublePressTimeoutKey),
    CFSTR(kIOHIDKeyboardPressCountTriplePressTimeoutKey),
    CFSTR(kIOHIDKeyboardLongPressTimeoutKey),
    CFSTR(kIOHIDBiometricTapTrackingEnabledKey),
    CFSTR(kIOHIDBiometricDoubleTapTimeoutKey),
    CFSTR(kIOHIDBiometricTripleTapTimeoutKey)
};

//------------------------------------------------------------------------------
// IOHIDEventProcessor::setPropertyForClient
//------------------------------------------------------------------------------

void IOHIDEventProcessor::setPropertyForClient(void * self,CFStringRef key,CFTypeRef property,CFTypeRef client)
{
    static_cast<IOHIDEventProcessor *>(self)->setPropertyForClient(key, property, client);
}

void IOHIDEventProcessor::setPropertyForClient(CFStringRef key,CFTypeRef property, CFTypeRef client __unused)
{
    UInt64  number  = 0;
    Event * e       = NULL;
    Event * p       = NULL;
    
    if (!key || !property)
        return;
    
    //TODO: should add support for passing a dictionary in for settings as that would allow atomicity of
    //      setting.
    
    if (CFStringCompare(key, CFSTR(kIOHIDKeyboardPressCountTrackingEnabledKey), 0) == kCFCompareEqualTo) {
        _multiPressTrackingEnabled = (property == kCFBooleanTrue);
        HIDLogDebug("Press Count %s", _multiPressTrackingEnabled ? "enabled" : "false");
        
        // Instantiate ButtonEvent pool if multi press is enabled.
        if ( _multiPressTrackingEnabled && !_freeButtonHead ) {
            for (int i = 0; i < MAX_BUTTON_EVENTS; i++) {
                e = new ButtonEvent;
                
                if (e)
                    e->setNextEvent(p);
                p = e;
            }
            _freeButtonHead = e;
        }
    }
    
    if (CFStringCompare(key, CFSTR(kIOHIDKeyboardPressCountUsagePairsKey), 0) == kCFCompareEqualTo) {
        if (_multiPressUsagePairs) {
            CFRelease(_multiPressUsagePairs);
        }
        _multiPressUsagePairs = (CFArrayRef)property;
        CFRetain(_multiPressUsagePairs);
        HIDLogDebug("Press Count Usage Pairs %@", _multiPressUsagePairs);
    }
    
    if (CFStringCompare(key, CFSTR(kIOHIDKeyboardPressCountDoublePressTimeoutKey),0) == kCFCompareEqualTo) {
        CFNumberGetValue((CFNumberRef)property, kCFNumberLongLongType, &number);
        _multiPressDoublePressTimeout = number;
        HIDLogDebug("doublePressTimeout now %llu", _multiPressDoublePressTimeout);
    }

    if (CFStringCompare(key, CFSTR(kIOHIDKeyboardPressCountTriplePressTimeoutKey),0) == kCFCompareEqualTo) {
        CFNumberGetValue((CFNumberRef)property, kCFNumberLongLongType, &number);
        _multiPressTriplePressTimeout = number;
        HIDLogDebug("triplePressTimeout now %llu", _multiPressTriplePressTimeout);
    }
    
    if (CFStringCompare(key, CFSTR(kIOHIDKeyboardLongPressTimeoutKey),0) == kCFCompareEqualTo) {
        CFNumberGetValue((CFNumberRef)property, kCFNumberLongLongType, &number);
        _longPressTimeout = number;
        HIDLogDebug("LongPress now %llu", _longPressTimeout);
    }
    
    if (CFStringCompare(key, CFSTR(kIOHIDBiometricTapTrackingEnabledKey), 0) == kCFCompareEqualTo) {
        _multiTapTrackingEnabled = (property == kCFBooleanTrue);
        HIDLogDebug("Tap Count %s",  _multiTapTrackingEnabled? "enabled" : "false");
        
        // Instantiate TapEvent pool if multi tap is enabled.
        if ( _multiTapTrackingEnabled && !_freeTapHead ) {
            for (int i = 0; i < MAX_TAP_EVENTS; i++) {
                e = new TapEvent;
                
                if (e)
                    e->setNextEvent(p);
                p = e;
            }
            _freeTapHead = e;
        }
    }
    
    if (CFStringCompare(key, CFSTR(kIOHIDBiometricDoubleTapTimeoutKey),0) == kCFCompareEqualTo) {
        CFNumberGetValue((CFNumberRef)property, kCFNumberLongLongType, &number);
        _multiTapDoubleTapTimeout = number;
        HIDLogDebug("double tap timeout now %llu", _multiTapDoubleTapTimeout);
    }
    
    if (CFStringCompare(key, CFSTR(kIOHIDBiometricTripleTapTimeoutKey),0) == kCFCompareEqualTo) {
        CFNumberGetValue((CFNumberRef)property, kCFNumberLongLongType, &number);
        _multiTapTripleTapTimeout = number;
        HIDLogDebug("triple tap timeout now %llu",  _multiTapTripleTapTimeout);
    }
    
    //TODO: Currently we only update the instance variables
    //      This potentially needs to be enhanced to handle in flight events
    //      for e.g. what happens when triple click timeout is changed when in double click state
}

//------------------------------------------------------------------------------
// IOHIDEventProcessor::copyPropertyForClient
//------------------------------------------------------------------------------

CFTypeRef IOHIDEventProcessor::copyPropertyForClient(void * self,CFStringRef key, CFTypeRef client)
{
    return static_cast<IOHIDEventProcessor *>(self)->copyPropertyForClient(key, client);
}

CFTypeRef IOHIDEventProcessor::copyPropertyForClient(CFStringRef key, CFTypeRef client __unused)
{
    CFTypeRef result = NULL;
  
    if (CFEqual(key, CFSTR(kIOHIDServiceFilterDebugKey))) {
        CFMutableDictionaryRefWrap serializer;
        if (serializer) {
          serialize(serializer);
          result = CFRetain(serializer.Reference());
        }
    }
    return result;
}

//------------------------------------------------------------------------------
// IOHIDEventProcessor::match
//------------------------------------------------------------------------------

SInt32 IOHIDEventProcessor::match(void * self, IOHIDServiceRef service, IOOptionBits options)
{
    return static_cast<IOHIDEventProcessor *>(self)->match(service, options);
}


SInt32 IOHIDEventProcessor::match(IOHIDServiceRef service, IOOptionBits options __unused)
{
#if TARGET_OS_IPHONE
    CFNumberRef queueSize = (CFNumberRef)IOHIDServiceCopyProperty(service, CFSTR(kIOHIDEventServiceQueueSize));
    if (queueSize) {
        uint32_t value = 0;
        CFNumberGetValue (queueSize, kCFNumberSInt32Type, &value);
        if (value != 0) {
            _matchScore = 200;
            _service = service;
        }
        CFRelease(queueSize);
    } else {
        _matchScore = 200;
        _service = service;
    }
#else
    // Event processor filter should load before NX translator filter.
    // see 31636239.
    _matchScore = 200;
    _service = service;
#endif
    
    HIDLogDebug("(%p) for ServiceID %@ with score %d", this, IOHIDServiceGetRegistryID(service), (int)_matchScore);
    
    return _matchScore;
}


//------------------------------------------------------------------------------
// IOHIDEventProcessor::filter
//------------------------------------------------------------------------------

IOHIDEventRef IOHIDEventProcessor::filter(void * self, IOHIDEventRef event)
{
    return static_cast<IOHIDEventProcessor *>(self)->filter(event);
}

// The IOHIDEventProcessor class is the the controller class, the state machine
// for the keyboard and biometric events is handled in the Event class. This 
// function looks for any existing Event object that corresponds to the the 
// HIDEvent that was just received. If none is found then a new Event object
// is assigned from the free pool and kicked off. stateHandler() is the main
// entry point into the Event state machine.

IOHIDEventRef IOHIDEventProcessor::filter(IOHIDEventRef event)
{
    Event *         curr            = NULL;
    Event **        freeHead        = NULL;
    IOHIDEventType  eventType       = 0;
    UInt32          usagePage       = 0;
    UInt32          usage           = 0;
    UInt64          doubleTO        = 0;
    UInt64          tripleTO        = 0;
    
    if (!_queue) {
        return event;
    }
    
    eventType = IOHIDEventGetType(event);
    
    if (eventType == kIOHIDEventTypeKeyboard) {
        if (!_multiPressTrackingEnabled) {
            goto exit;
        }
        
        doubleTO = _multiPressDoublePressTimeout;
        tripleTO = _multiPressTriplePressTimeout;
        
        freeHead = &_freeButtonHead;
    }
    
    else if (eventType == kIOHIDEventTypeBiometric) {
        if (!_multiTapTrackingEnabled) {
            goto exit;
        }
        
        doubleTO = _multiTapDoubleTapTimeout;
        tripleTO = _multiTapTripleTapTimeout;
        
        freeHead = &_freeTapHead;
    }
    
    else {
        goto exit;
    }
    
    usagePage = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage);
    usage     = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
    
    HIDLogDebug("filter: type = %d p = %d u = %d", (int)eventType, (int)usagePage, (int)usage);
    
    // If usage pairs have been specified, filter for the specified usage pairs.
    // If usage pairs have not been specified, process all usage pairs.
    if (eventType == kIOHIDEventTypeKeyboard && _multiPressUsagePairs) {
        bool    match = false;
        CFIndex count = CFArrayGetCount(_multiPressUsagePairs);
        
        for (CFIndex i = 0; i < count; i++) {
            CFNumberRef pairNum = (CFNumberRef)CFArrayGetValueAtIndex(_multiPressUsagePairs, i);
            UInt64 pair;
            
            CFNumberGetValue(pairNum, kCFNumberLongLongType, &pair);
            
            if (((usagePage<<16)|usage) == pair) {
                match = true;
            }
        }
        
        if (match == false) {
            HIDLogDebug("usage pair should not be processed, letting through");
            goto exit;
        }
    }
    
    if ((IOHIDEventGetPhase(event) & kIOHIDEventPhaseEnded) != 0) {
        HIDLogDebug("terminal event detected, letting through");
        goto exit;
    }
    
    if (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardLongPress) != 0) {
        HIDLogDebug("letting long press event through");
        goto exit;
    }
   
    curr = _eventHead;
    
    while (curr != NULL) {
        if (curr->conformsTo(eventType, usagePage, usage))
            break;
        
        curr = curr->getNextEvent();
    }
    
    if (curr == NULL) {
        curr = *freeHead;
        
        if (!curr) {
            HIDLogError("No more free events");
            return event;
        }
        
        *freeHead = curr->getNextEvent();
        
        curr->init(this,
                   _timer,
                   eventType,
                   usagePage,
                   usage,
                   doubleTO,
                   tripleTO,
                   eventType == kIOHIDEventTypeKeyboard ? _longPressTimeout : 0);
        
        if (!curr) {
            HIDLogError("Could not create new event");
            return event;
        }
        
        curr->setNextEvent(_eventHead);
        _eventHead = curr;
    }

    curr->stateHandler(isDownEvent(event) ? kKPTransitionDown : kKPTransitionUp, event);
    
exit:
    
    return event;
}


//------------------------------------------------------------------------------
// IOHIDEventProcessor::scheduleWithDispatchQueue
//------------------------------------------------------------------------------

void IOHIDEventProcessor::scheduleWithDispatchQueue(void * self, dispatch_queue_t queue)
{
    static_cast<IOHIDEventProcessor *>(self)->scheduleWithDispatchQueue(queue);
}

void IOHIDEventProcessor::scheduleWithDispatchQueue(dispatch_queue_t queue)
{
    _queue = queue;
    
    _timer->init(_queue);
}


//------------------------------------------------------------------------------
// IOHIDEventProcessor::unscheduleFromDispatchQueue
//------------------------------------------------------------------------------

void IOHIDEventProcessor::unscheduleFromDispatchQueue(void * self, dispatch_queue_t queue)
{
    static_cast<IOHIDEventProcessor *>(self)->unscheduleFromDispatchQueue(queue);
}


void IOHIDEventProcessor::unscheduleFromDispatchQueue(dispatch_queue_t queue)
{
    if ( _queue != queue )
        return;
    
    _timer->cancel(queue);
    _queue = NULL;
}


//------------------------------------------------------------------------------
// IOHIDEventProcessor::setEventCallback
//------------------------------------------------------------------------------

void IOHIDEventProcessor::setEventCallback(void * self, IOHIDServiceEventCallback callback, void * target, void * refcon)
{
    static_cast<IOHIDEventProcessor *>(self)->setEventCallback(callback, target, refcon);
}

void IOHIDEventProcessor::setEventCallback(IOHIDServiceEventCallback callback, void * target, void * refcon)
{
    _eventCallback  = callback;
    _eventTarget    = target;
    _eventContext   = refcon;
}


//------------------------------------------------------------------------------
// IOHIDEventProcessor::filter
//------------------------------------------------------------------------------

IOHIDEventRef IOHIDEventProcessor::filterCopyEvent(void * self, IOHIDEventRef event)
{
    return static_cast<IOHIDEventProcessor *>(self)->filterCopyEvent(event);
}


IOHIDEventRef IOHIDEventProcessor::filterCopyEvent(IOHIDEventRef event)
{
    return event;
}


//------------------------------------------------------------------------------
// IOHIDEventProcessor::open
//------------------------------------------------------------------------------

void IOHIDEventProcessor::open(void * self, IOHIDServiceRef service, IOOptionBits options) {
    static_cast<IOHIDEventProcessor *>(self)->open(service, options);
}

void IOHIDEventProcessor::open(IOHIDServiceRef service, IOOptionBits options __unused) {

    for (size_t i = 0; i < sizeof(PropertyList)/sizeof(PropertyList[0]); i++) {
        CFTypeRef value = IOHIDServiceCopyProperty(service, PropertyList[i]);
        if (value) {
            setPropertyForClient(PropertyList[i], value, NULL);
            CFRelease(value);
        }
    }
}


//------------------------------------------------------------------------------
// IOHIDEventProcessor::dispatchEvent
//------------------------------------------------------------------------------

void IOHIDEventProcessor::dispatchEvent(IOHIDEventRef event, bool async)
{
    if (!_queue)
        return;
    
    if ( async ) {
        CFRetain(event);
        dispatch_async(_queue, ^{
            HIDLogDebug("asynchronously dispatching event = %p", event);
            _eventCallback(_eventTarget, _eventContext, _service, event, 0);
            CFRelease(event);
        });
    }
    else {
        HIDLogDebug("synchronously dispatching event = %p", event);
        _eventCallback(_eventTarget, _eventContext, _service, event, 0);
    }
}


//------------------------------------------------------------------------------
// IOHIDEventProcessor::returnToFreePool
//------------------------------------------------------------------------------

void IOHIDEventProcessor::returnToFreePool(Event * event)
{
    Event *  p = 0;
    Event *  c = _eventHead;
    Event ** f = NULL;
    
    HIDLogDebug("returning event %p to free pool", event);
    
    if (event->getEventType() == kIOHIDEventTypeKeyboard) {
        f = &_freeButtonHead;
    }
    else if (event->getEventType() == kIOHIDEventTypeBiometric){
        f = &_freeTapHead;
    }
    
    while (c) {
        if (*c == *event) {
            if (p) {
                p->setNextEvent(event->getNextEvent());
            }
            
            else {
                _eventHead = _eventHead->getNextEvent();
            }
            break;
        }
        
        p = c;
        c = c->getNextEvent();
    }
    
    if (f) {
        event->setNextEvent(*f);
        *f = event;
    }
}


//------------------------------------------------------------------------------
// IOHIDEventProcessor::serialize
//------------------------------------------------------------------------------
void IOHIDEventProcessor::serialize (CFMutableDictionaryRef dict) const {
    CFMutableDictionaryRefWrap serializer (dict);
    serializer.SetValueForKey(CFSTR("Class"), CFSTR("IOHIDEventProcessor"));
    serializer.SetValueForKey(CFSTR(kIOHIDKeyboardPressCountTrackingEnabledKey), _multiPressTrackingEnabled);
    serializer.SetValueForKey(CFSTR(kIOHIDKeyboardPressCountUsagePairsKey), _multiPressUsagePairs);
    serializer.SetValueForKey(CFSTR(kIOHIDKeyboardPressCountDoublePressTimeoutKey), _multiPressDoublePressTimeout);
    serializer.SetValueForKey(CFSTR(kIOHIDKeyboardPressCountTriplePressTimeoutKey), _multiPressTriplePressTimeout);
    serializer.SetValueForKey(CFSTR(kIOHIDKeyboardLongPressTimeoutKey), _longPressTimeout);
    serializer.SetValueForKey(CFSTR(kIOHIDBiometricTapTrackingEnabledKey), _multiPressTriplePressTimeout);
    serializer.SetValueForKey(CFSTR(kIOHIDBiometricDoubleTapTimeoutKey), _multiTapDoubleTapTimeout);
    serializer.SetValueForKey(CFSTR(kIOHIDBiometricTripleTapTimeoutKey), _multiTapTripleTapTimeout);
    serializer.SetValueForKey(CFSTR("MatchScore"), (uint64_t)_matchScore);
}



#pragma mark - 
#pragma mark Event
#pragma mark -


//------------------------------------------------------------------------------
// Event::Event
//------------------------------------------------------------------------------

Event::Event()
:
_nextTimeout(0),
_nextEvent(0),
_nextTimerEvent(0),
_owner(0),
_isComplete(true),
_eventType(0),
_usagePage(0),
_usage(0),
_timer(0),
_state(0),
_timeoutState(0),
_multiEventCount(0),
_terminalEventDispatched(0),
_secondEventTimeout(0),
_thirdEventTimeout(0),
_lastActionTimestamp(0),
_longPressTimeout(0)
{
}


//------------------------------------------------------------------------------
// Event::~Event
//------------------------------------------------------------------------------

Event::~Event()
{
}


//------------------------------------------------------------------------------
// Event::init
//------------------------------------------------------------------------------

void Event::init(IOHIDEventProcessor * owner,
                 Timer * timer,
                 IOHIDEventType eventType,
                 UInt32 usagePage,
                 UInt32 usage,
                 UInt64 secondEventTimeout,
                 UInt64 thirdEventTimeout,
                 UInt64 longPressTimeout)
{
    _owner                   = owner;
    _eventType               = eventType;
    _usagePage               = usagePage;
    _usage                   = usage;
    _secondEventTimeout      = secondEventTimeout;
    _thirdEventTimeout       = thirdEventTimeout;
    _longPressTimeout        = longPressTimeout;
    _timer                   = timer;
    
    _terminalEventDispatched = false;
    _lastActionTimestamp     = 0;
    _nextEvent               = 0;
    _nextTimerEvent          = 0;
    _state                   = 0;
    _timeoutState            = 0;
    _nextTimeout             = 0;
    _isComplete              = false;
    _multiEventCount         = 0;
    
    // Slightly offset terminal event timeouts from long press timeout,
    // if they are the same.
    if (_longPressTimeout) {
        if (_longPressTimeout == _secondEventTimeout)
            _secondEventTimeout++;
        if (_longPressTimeout == _thirdEventTimeout)
            _thirdEventTimeout++;
    }
    
    if ((_longPressTimeout < _secondEventTimeout) ||
        (_longPressTimeout < _thirdEventTimeout)) {
        HIDLogDebug("long %llu second %llu third %llu\n",
                    _longPressTimeout, _secondEventTimeout, _thirdEventTimeout);
    }
}


//------------------------------------------------------------------------------
// Event::conformsTo
//------------------------------------------------------------------------------

bool Event::conformsTo(IOHIDEventType eventType, UInt32 usagePage, UInt32 usage)
{
    bool ret = false;
    
    do {
        if (eventType != _eventType)
            break;
        
        if (usage != _usage)
            break;
        
        if (usagePage != _usagePage)
            break;
        
        ret = true;
        
    } while (0);
    
    return ret;
}


//------------------------------------------------------------------------------
// isDownEvent
//------------------------------------------------------------------------------

static bool isDownEvent(IOHIDEventRef event)
{
    bool down                   = false;
    IOHIDEventType eventType    = IOHIDEventGetType(event);
    
    if (eventType == kIOHIDEventTypeKeyboard) {
        down = (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown) == 1);
    }
    
    else if (eventType == kIOHIDEventTypeBiometric) {
        down = (IOHIDEventGetFloatValue(event, kIOHIDEventFieldBiometricLevel) == 1.0);
    }
    
    return down;
}


//------------------------------------------------------------------------------
// Event::dispatchEvent
//------------------------------------------------------------------------------

void Event::dispatchEvent(IOHIDEventRef event, bool async)
{
    _owner->dispatchEvent(event, async);
}


//------------------------------------------------------------------------------
// Event::completed
//------------------------------------------------------------------------------

void Event::completed()
{
    _isComplete = true; 
    _owner->returnToFreePool(this);
}


//------------------------------------------------------------------------------
// state machine map
//------------------------------------------------------------------------------

static void FDEnter(Event * e, IOHIDEventRef event);
static void FUEnter(Event * e, IOHIDEventRef event);
static void SDEnter(Event * e, IOHIDEventRef event);
static void SUEnter(Event * e, IOHIDEventRef event);
static void TDEnter(Event * e, IOHIDEventRef event);
static void TUEnter(Event * e, IOHIDEventRef event);
static void TOEnter(Event * e, IOHIDEventRef event);
static void TEEnter(Event * e, IOHIDEventRef event);
static void LPEnter(Event * e, IOHIDEventRef event);
static void NoneEnter(Event * e, IOHIDEventRef event);

typedef void (*transition_handler_t)(Event * e, IOHIDEventRef event);

transition_handler_t _stateMap[kKPStateCount][kKPTransitionCount] = {
    /*          Down                Up              Timeout*/
    /*None*/{   &FDEnter,           NULL,           NULL,    },
    /*FD*/  {   NULL,               &FUEnter,       &TOEnter },
    /*FU*/  {   &SDEnter,           NULL,           &TOEnter },
    /*SD*/  {   NULL,               &SUEnter,       &TOEnter },
    /*SU*/  {   &TDEnter,           NULL,           &TOEnter },
    /*TD*/  {   NULL,               &TUEnter,       &TOEnter },
    /*TU*/  {   NULL,               NULL,           &TOEnter },
    /*TE*/  {   &FDEnter,           &NoneEnter,     &TOEnter },
    /*LP*/  {   NULL,               &NoneEnter,     &TOEnter }
};

static const char * _stateNames[] = {"NON", "FDN", "FUP", "SDN", "SUP", "TDN", "TUP", "TEE", "LPE"};
static const char * _transitions[] = {"DOWN", "UPUP", "TOUT" };

//------------------------------------------------------------------------------
// Event::stateHandler
//------------------------------------------------------------------------------

bool Event::stateHandler(KPTransition transition, IOHIDEventRef event)
{
    bool ret = false;
    transition_handler_t handler = NULL;
    
    // Handle and dispatch any overdue events on the timer. This could modify _state.
    // This will catch up the state machine before _stateMap is indexed.
    _timer->checkEventTimeouts();
    
    handler = _stateMap[_state][transition];
    
    HIDLogDebug("state = %s transition = %s", _stateNames[_state], _transitions[transition]);
    
    if (!handler) {
        HIDLogDebug("Invalid state transition: [%d][%d]", (unsigned int)_state, transition);
        goto exit;
    }
    
    ret = true;
    
    handler(this, event);
    
    HIDLogDebug("new state = %s", _stateNames[_state]);
    
exit:
    return ret;
}


//------------------------------------------------------------------------------
// Transistion function wrappers
//------------------------------------------------------------------------------

static void NoneEnter(Event * e, IOHIDEventRef event)
{
    static_cast <Event*>(e)->NoneEnter(event);
}


static void FDEnter(Event * e, IOHIDEventRef event)
{
    static_cast <Event *>(e)->FDEnter(event);
}


static void FUEnter(Event * e, IOHIDEventRef event)
{
    static_cast <Event *>(e)->FUEnter(event);
}


static void SDEnter(Event * e, IOHIDEventRef event)
{
    static_cast <Event *>(e)->SDEnter(event);
}


static void SUEnter(Event * e, IOHIDEventRef event)
{
    static_cast <Event *>(e)->SUEnter(event);
}


static void TDEnter(Event * e, IOHIDEventRef event)
{
    static_cast <Event *>(e)->TDEnter(event);
}


static void TUEnter(Event * e, IOHIDEventRef event)
{
    static_cast <Event *>(e)->TUEnter(event);
}


static void TOEnter(Event * e, IOHIDEventRef event)
{
    static_cast <Event *>(e)->TOEnter(event);
}


__unused static void TEEnter(Event * e, IOHIDEventRef event)
{
    static_cast <Event *>(e)->TEEnter(event);
}


__unused static void LPEnter(Event * e, IOHIDEventRef event)
{
    static_cast <Event *>(e)->LPEnter(event);
}


#pragma mark -
#pragma mark ButtonEvent functions
#pragma mark -


//------------------------------------------------------------------------------
// ButtonEvent::setMultiEventCount
//------------------------------------------------------------------------------

void ButtonEvent::setMultiEventCount(IOHIDEventRef event, CFIndex count)
{
    HIDLogDebug("%p %p setting multi count = %d", this, event, (int)count);
    
    IOHIDEventSetIntegerValue(event, kIOHIDEventFieldKeyboardPressCount, count);
    
    _multiEventCount = count ? (int)count : _multiEventCount;
}


//------------------------------------------------------------------------------
// ButtonEvent::createSyntheticEvent
//------------------------------------------------------------------------------

IOHIDEventRef ButtonEvent::createSyntheticEvent(bool isTerminalEvent)
{
    IOHIDEventRef   event = NULL;
    UInt64          timestamp = mach_absolute_time();
    
    event = IOHIDEventCreateKeyboardEvent(kCFAllocatorDefault,
                                          timestamp,
                                          _usagePage,
                                          _usage,
                                          STATE_IS_DOWN(_state),
                                          0);
    
    if (event && isTerminalEvent) {
        // set presscount for consistency
        setMultiEventCount(event, _multiEventCount);
        
        IOHIDEventSetPhase(event, IOHIDEventGetPhase(event) | kIOHIDEventPhaseEnded);
        _terminalEventDispatched = true;
    }
    
    HIDLogDebug("created terminal(%d) event %p type %d", isTerminalEvent, event, (int)_eventType);
    
    return event;
}


//------------------------------------------------------------------------------
// ButtonEvent::NoneEnter
//------------------------------------------------------------------------------

void ButtonEvent::NoneEnter(IOHIDEventRef event __unused)
{
    if (event) {
        _timer->registerEventTimeout(this, 0);
    }
    
    // None is entered via up events, the press count
    // should match the corresponding down
    if (event) {
        setMultiEventCount(event, _multiEventCount);
    }
    
    completed();
    
    _state = kKPStateNone;
}


//------------------------------------------------------------------------------
// ButtonEvent::FDEnter
//------------------------------------------------------------------------------

void ButtonEvent::FDEnter(IOHIDEventRef event)
{
    _lastActionTimestamp = IOHIDEventGetTimeStamp(event);
    
    IOHIDEventSetPhase(event, IOHIDEventGetPhase(event) | kIOHIDEventPhaseBegan);
    
    setMultiEventCount(event, 1);
    
    _state = kKPStateFirstDown;
    
    // trigger next timeout immediately to dispatch terminal event
    // if there is no SD or LP timeout
    if ( _secondEventTimeout == 0 ) {
        TEEnter(event);
    }
    if ( _longPressTimeout > _secondEventTimeout || _longPressTimeout == 0 ) {
        _timeoutState = kKPStateTerminalEvent;
        _timer->registerEventTimeout(this, _secondEventTimeout);
    }
    else {
        _timeoutState = kKPStateLongPress;
        _timer->registerEventTimeout(this, _longPressTimeout);
    }
}


//------------------------------------------------------------------------------
// ButtonEvent::FUEnter
//------------------------------------------------------------------------------

void ButtonEvent::FUEnter(IOHIDEventRef event)
{
    setMultiEventCount(event, 1);
    
    _state = kKPStateFirstUp;
}


//------------------------------------------------------------------------------
// ButtonEvent::SDEnter
//------------------------------------------------------------------------------

void ButtonEvent::SDEnter(IOHIDEventRef event)
{
    _lastActionTimestamp = IOHIDEventGetTimeStamp(event);
    
    setMultiEventCount(event, 2);
    
    _state = kKPStateSecondDown;
    
    // trigger next timeout immediately to dispatch terminal event
    // if there is no TD or LP timeout
    if ( _thirdEventTimeout == 0 ) {
        TEEnter(event);
    }
    else if ( _longPressTimeout > _thirdEventTimeout || _longPressTimeout == 0 ) {
        _timeoutState = kKPStateTerminalEvent;
        _timer->registerEventTimeout(this, _thirdEventTimeout);
    }
    else {
        _timeoutState = kKPStateLongPress;
        _timer->registerEventTimeout(this, _longPressTimeout);
    }
}


//------------------------------------------------------------------------------
// ButtonEvent::SUEnter
//------------------------------------------------------------------------------

void ButtonEvent::SUEnter(IOHIDEventRef event)
{
    setMultiEventCount(event, 2);
    
    _state = kKPStateSecondUp;
}


//------------------------------------------------------------------------------
// ButtonEvent::TDEnter
//------------------------------------------------------------------------------

void ButtonEvent::TDEnter(IOHIDEventRef event)
{
    _lastActionTimestamp = IOHIDEventGetTimeStamp(event);
    
    setMultiEventCount(event, 3);
    
    _state = kKPStateThirdDown;
    
    // trigger next timeout immediately
    TEEnter(event);
}


//------------------------------------------------------------------------------
// ButtonEvent::TUEnter
//------------------------------------------------------------------------------

void ButtonEvent::TUEnter(IOHIDEventRef event)
{
    setMultiEventCount(event, 3);
    
    NoneEnter(event);
}


//------------------------------------------------------------------------------
// ButtonEvent::TOEnter
//------------------------------------------------------------------------------

void ButtonEvent::TOEnter(IOHIDEventRef event)
{
    if (_timeoutState == kKPStateTerminalEvent) {
        TEEnter(event);
    }
    else if (_timeoutState == kKPStateLongPress) {
        LPEnter(event);
    }
}


//------------------------------------------------------------------------------
// ButtonEvent::TEEnter
//------------------------------------------------------------------------------

void ButtonEvent::TEEnter(IOHIDEventRef event)
{
    uint64_t    nextTimeout = 0;
    bool        isDown      = false;
    
    isDown = ((_state == kKPStateFirstDown) ||
              (_state == kKPStateSecondDown) ||
              (_state == kKPStateThirdDown) ||
              (_state == kKPStateLongPress));
    
    // There is only a single timer, so we entered here it is due to
    // one of second or third event timeout, set the next timeout
    // to the difference between them
    
    if (isDown && _longPressTimeout) {
        if (_state == kKPStateFirstDown && _longPressTimeout > _secondEventTimeout) {
            nextTimeout = _longPressTimeout - _secondEventTimeout;
        }
        
        else if (_state == kKPStateSecondDown && _longPressTimeout > _thirdEventTimeout) {
            nextTimeout = _longPressTimeout - _thirdEventTimeout;
        }
        
        else if (_state == kKPStateThirdDown) {
            nextTimeout = _longPressTimeout;
        }
        
        _timeoutState = kKPStateLongPress;
    }
    
    IOHIDEventRef terminalEvent = createSyntheticEvent(true);
    
    // If there is an event being processed, asynchronously dispatch the synthetic event afterward.
    // Otherwise, synchronously dispatch the synthetic event.
    dispatchEvent(terminalEvent, (event ? true : false));
    
    _lastActionTimestamp = IOHIDEventGetTimeStamp(terminalEvent);
    CFRelease(terminalEvent);
    
    _timer->registerEventTimeout(this, nextTimeout);
    
    if (isDown) {
        _state = kKPStateTerminalEvent;
    }
    
    else {
        NoneEnter(NULL);
    }
}


//------------------------------------------------------------------------------
// ButtonEvent::LPEnter
//------------------------------------------------------------------------------

void ButtonEvent::LPEnter(IOHIDEventRef event)
{
    bool isDown = false;
    bool isUp = false;
    
    isDown = ((_state == kKPStateFirstDown) ||
              (_state == kKPStateSecondDown) ||
              (_state == kKPStateThirdDown));
    
    isUp = ((_state == kKPStateFirstUp) ||
            (_state == kKPStateSecondUp) ||
            (_state == kKPStateThirdUp));
    
    if (isUp) {
        uint64_t nextTimeout = 0;
        
        // Button-up has occured so do not sent long press. TE can still happen.
        // There is only a single timer, so set the next timeout
        // to the difference between LP and multiclick.
        
        if (_state == kKPStateFirstUp && _secondEventTimeout > _longPressTimeout) {
            nextTimeout = _secondEventTimeout - _longPressTimeout;
        }
        
        else if (_state == kKPStateSecondUp && _thirdEventTimeout > _longPressTimeout) {
            nextTimeout = _thirdEventTimeout - _longPressTimeout;
        }
        
        _timeoutState = kKPStateTerminalEvent;
        
        _lastActionTimestamp = mach_absolute_time();
        
        _timer->registerEventTimeout(this, nextTimeout);
    }
    else {
        
        // Button is still down, so send long press event. Send terminal event immediately
        // afterward (if not already sent), and reset presscount.
        
        IOHIDEventRef lpEvent = createSyntheticEvent(false);
        
        IOHIDEventSetIntegerValue(lpEvent, kIOHIDEventFieldKeyboardLongPress, kIOHIDKeyboardLongPress);
        IOHIDEventSetIntegerValue(lpEvent, kIOHIDEventFieldKeyboardDown, true);
        
        setMultiEventCount(lpEvent, _multiEventCount);
        
        // If there is an event being processed, asynchronously dispatch the synthetic event afterward.
        // Otherwise, synchronously dispatch the synthetic event.
        dispatchEvent(lpEvent, (event ? true : false));
        
        _state = kKPStateLongPress;
        
        if (isDown) {
            _lastActionTimestamp = IOHIDEventGetTimeStamp(lpEvent);
            
            TEEnter(event);
        }
        
        CFRelease(lpEvent);
    }
}


#pragma mark -
#pragma mark TapEvent functions
#pragma mark -


//------------------------------------------------------------------------------
// TapEvent::setMultiEventCount
//------------------------------------------------------------------------------

void TapEvent::setMultiEventCount(IOHIDEventRef event, CFIndex count)
{
    HIDLogDebug("%p %p setting multi count = %d", this, event, (int)count);
    
    IOHIDEventSetIntegerValue(event, kIOHIDEventFieldBiometricTapCount, count);
    
    _multiEventCount = count ? (int)count : _multiEventCount;
}


//------------------------------------------------------------------------------
// TapEvent::createSyntheticEvent
//------------------------------------------------------------------------------

IOHIDEventRef TapEvent::createSyntheticEvent(bool isTerminalEvent)
{
    IOHIDEventRef   event = NULL;
    UInt64          timestamp = mach_absolute_time();
    
    event = IOHIDEventCreateBiometricEvent(kCFAllocatorDefault,
                                           timestamp,
                                           kIOHIDBiometricEventTypeHumanTouch,
                                           (STATE_IS_DOWN(_state) ?
                                            1.0  : 0),
                                           0);
    
    
    if (event) {
        IOHIDEventSetIntegerValue(event,
                                  kIOHIDEventFieldBiometricUsagePage,
                                  _usagePage);
        IOHIDEventSetIntegerValue(event,
                                  kIOHIDEventFieldBiometricUsage,
                                  _usage);
    }
    
    if (event && isTerminalEvent) {
        // set presscount for consistency
        setMultiEventCount(event, _multiEventCount);
        
        IOHIDEventSetPhase(event, IOHIDEventGetPhase(event) | kIOHIDEventPhaseEnded);
        _terminalEventDispatched = true;
    }
    
    HIDLogDebug("created terminal(%d) event %p type %d", isTerminalEvent, event, (int)_eventType);
    
    return event;
}


//------------------------------------------------------------------------------
// TapEvent::NoneEnter
//------------------------------------------------------------------------------

void TapEvent::NoneEnter(IOHIDEventRef event __unused)
{
    if (event) {
        _timer->registerEventTimeout(this, 0);
    }
    
    // None is entered via up events, the press count
    // should match the corresponding up
    if (event) {
        setMultiEventCount(event, _multiEventCount);
    }
    
    completed();
    
    _state = kKPStateNone;
}


//------------------------------------------------------------------------------
// TapEvent::FDEnter
//------------------------------------------------------------------------------

void TapEvent::FDEnter(IOHIDEventRef event)
{
    setMultiEventCount(event, 0);
    
    _state = kKPStateFirstDown;
}


//------------------------------------------------------------------------------
// TapEvent::FUEnter
//------------------------------------------------------------------------------

void TapEvent::FUEnter(IOHIDEventRef event)
{
    _lastActionTimestamp = IOHIDEventGetTimeStamp(event);
    
    IOHIDEventSetPhase(event, IOHIDEventGetPhase(event) | kIOHIDEventPhaseBegan);
    
    setMultiEventCount(event, 1);
    
    _state = kKPStateFirstUp;
    
    // trigger next timeout immediately to dispatch terminal event
    // if there is no SU timeout
    if ( _secondEventTimeout == 0) {
        TEEnter(event);
    }
    else {
        _timer->registerEventTimeout(this, _secondEventTimeout);
    }
}


//------------------------------------------------------------------------------
// TapEvent::SDEnter
//------------------------------------------------------------------------------

void TapEvent::SDEnter(IOHIDEventRef event)
{
    setMultiEventCount(event, 1);
    
    _state = kKPStateSecondDown;
}


//------------------------------------------------------------------------------
// TapEvent::SUEnter
//------------------------------------------------------------------------------

void TapEvent::SUEnter(IOHIDEventRef event)
{
    _lastActionTimestamp = IOHIDEventGetTimeStamp(event);
    
    setMultiEventCount(event, 2);
    
    _state = kKPStateSecondUp;
    
    // trigger next timeout immediately to dispatch terminal event
    // if there is no TU timeout
    if ( _thirdEventTimeout == 0 ) {
        TEEnter(event);
    }
    else {
        _timer->registerEventTimeout(this, _thirdEventTimeout);
    }
}


//------------------------------------------------------------------------------
// TapEvent::TDEnter
//------------------------------------------------------------------------------

void TapEvent::TDEnter(IOHIDEventRef event)
{
    setMultiEventCount(event, 2);
    
    _state = kKPStateThirdDown;
}


//------------------------------------------------------------------------------
// TapEvent::TUEnter
//------------------------------------------------------------------------------

void TapEvent::TUEnter(IOHIDEventRef event)
{
    _lastActionTimestamp = IOHIDEventGetTimeStamp(event);
    
    setMultiEventCount(event, 3);

    _state = kKPStateThirdUp;
    
    // trigger next timeout immediately
    TEEnter(event);
}


//------------------------------------------------------------------------------
// TapEvent::TOEnter
//------------------------------------------------------------------------------

void TapEvent::TOEnter(IOHIDEventRef event __unused)
{
    // There is no tap longpress (for now). Go directly to TE.
    TEEnter(event);
}


//------------------------------------------------------------------------------
// TapEvent::TEEnter
//------------------------------------------------------------------------------

void TapEvent::TEEnter(IOHIDEventRef event)
{
    bool        isDown      = false;
    
    isDown = ((_state == kKPStateFirstDown) ||
              (_state == kKPStateSecondDown) ||
              (_state == kKPStateThirdDown));
    
    // There is only a single timer, so we entered here it is due to
    // one of second or third event timeout, set the next timeout
    // to the dfference between them
    
    IOHIDEventRef terminalEvent = createSyntheticEvent(true);
    
    // If there is an event being processed, asynchronously dispatch the synthetic event afterward.
    // Otherwise, synchronously dispatch the synthetic event.
    dispatchEvent(terminalEvent, (event ? true : false));
    CFRelease(terminalEvent);
    
    // There is no tap longpress (for now). Stop event timer.
    _timer->registerEventTimeout(this, 0);
    
    if (isDown) {
        _state = kKPStateFirstDown;
    }
    
    else {
        NoneEnter(NULL);
    }
}


//------------------------------------------------------------------------------
// TapEvent::LPEnter
//------------------------------------------------------------------------------

void TapEvent::LPEnter(IOHIDEventRef event __unused)
{
    // There is no tap longpress (for now). This should not be entered.
    _state = kKPStateLongPress;
}



#pragma mark -
#pragma mark Timer functions
#pragma mark -


//------------------------------------------------------------------------------
// Timer::Timer
//------------------------------------------------------------------------------

Timer::Timer()
:
_timer(0),
_queue(0),
_headEvent(0)
{
    
}

//------------------------------------------------------------------------------
// Timer::init
//------------------------------------------------------------------------------
void Timer::init(dispatch_queue_t q)
{
    dispatch_source_t tempTimer;
    setQueue(q);
    
    if (_timer == NULL) {
        tempTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, DISPATCH_TIMER_STRICT, _queue);
        
        dispatch_source_set_event_handler(tempTimer, ^{
            timeoutHandler();
        });
        
        dispatch_source_set_cancel_handler(tempTimer, ^(void) {
            dispatch_release(tempTimer);
        });
        
        dispatch_source_set_timer(tempTimer, DISPATCH_TIME_FOREVER, 0, 0);
        _timer = tempTimer;
        dispatch_resume(_timer);
    }
}


//------------------------------------------------------------------------------
// Timer::cancel
//------------------------------------------------------------------------------

void Timer::cancel(dispatch_queue_t q __unused)
{
    if (_timer != NULL) {
        dispatch_source_cancel(_timer);
    }
    
    setQueue(NULL);
}


//------------------------------------------------------------------------------
// Timer::removeEvent
//------------------------------------------------------------------------------

void Timer::removeEvent(Event * event)
{
    Event * e = _headEvent;
    Event * p = NULL;
    
    while (e) {
        if (*e == *event) {
            if (p) {
                p->setNextTimerEvent(event->getNextTimerEvent());
            } else {
                _headEvent = _headEvent->getNextTimerEvent();
            }
            
            break;
        }
        
        p = e;
        e = e->getNextTimerEvent();
    }
    
    event->setNextTimerEvent(NULL);
}

//------------------------------------------------------------------------------
// Timer::insertEvent
//------------------------------------------------------------------------------

void Timer::insertEvent(Event * event)
{
    // make sure there is no existing event
    removeEvent(event);
    
    // insert the event at the head of the list
    event->setNextTimerEvent(_headEvent);
    _headEvent = event;
}


//------------------------------------------------------------------------------
// Timer::timeoutHandlers
//------------------------------------------------------------------------------

void Timer::timeoutHandler()
{
    HIDLogDebug("%p timeout occurred", this);
    
    checkEventTimeouts();
}


//------------------------------------------------------------------------------
// Timer::checkEventTimeouts
//------------------------------------------------------------------------------

void Timer::checkEventTimeouts()
{
    Event *         event           = _headEvent;
    UInt64          currentTime     = mach_absolute_time();
    Event *         nextEvent       = NULL;
    
    while (event) {
        nextEvent = event->getNextTimerEvent();
        
        if ((event->getNextTimeout() == 0) ||
            (event->isComplete())) {
            removeEvent(event);
            event = nextEvent;
            continue;
        }
        
        if (PastDeadline(currentTime, event->epoch(), event->getNextTimeout())) {
            // remove the event first as timeout handler may
            // put the event back in
            HIDLogDebug("%p past deadline %lld us", event, DeltaInUS(currentTime, event->epoch()) - event->getNextTimeout());
            
            removeEvent(event);
            
            event->stateHandler(kKPTransitionTimeout, NULL);
        }
        
        //use cached next event, as the event may have gone back in free queue
        event = nextEvent;
    }
    
    updateTimeout();
}


//------------------------------------------------------------------------------
// Timer::updateTimeout
//------------------------------------------------------------------------------

void Timer::updateTimeout()
{
    UInt64  currentTime = mach_absolute_time();
    SInt64  nextTimeout = INT64_MAX;
    Event*  event       = _headEvent;
    
    // Find the smallest remaining timeout of all events.
    while (event) {
        SInt64 eventTimeout = event->getNextTimeout() - DeltaInUS(currentTime, event->epoch());
        
        if (nextTimeout > eventTimeout) {
            nextTimeout = eventTimeout;
        }
        
        event = event->getNextTimerEvent();
    }
    
    // If somehow the timeout is past its deadline, set to 0.
    if (nextTimeout < 0) {
        nextTimeout = 0;
    }
    
    if (nextTimeout == INT64_MAX) {
        dispatch_source_set_timer(_timer, DISPATCH_TIME_FOREVER, 0, 0);
    }
   
    else {
        dispatch_source_set_timer(_timer,
                              dispatch_time(DISPATCH_TIME_NOW, nextTimeout * NSEC_PER_USEC),
                              DISPATCH_TIME_FOREVER,
                              0);
    }
    
    return;
}

                              
//------------------------------------------------------------------------------
// Timer::registerEventTimeout
//------------------------------------------------------------------------------

void Timer::registerEventTimeout(Event * event, UInt64 deadline)
{
    HIDLogDebug("registering %p for timeout in %llu uS", event, deadline);
    event->setNextTimeout(deadline);
    
    if (deadline == 0)
        removeEvent(event);
    else
        insertEvent(event);
        
    updateTimeout();
}


