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

#ifndef IOHIDEventProcessorFilter_hpp
#define IOHIDEventProcessorFilter_hpp

#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#if COREFOUNDATION_CFPLUGINCOM_SEPARATE
#include <CoreFoundation/CFPlugInCOM.h>
#endif
#include <IOKit/hid/IOHIDServiceFilterPlugIn.h>

typedef enum _KPState {
    kKPStateNone = 0,
    kKPStateFirstDown,
    kKPStateFirstUp,
    kKPStateSecondDown,
    kKPStateSecondUp,
    kKPStateThirdDown,
    kKPStateThirdUp,
    kKPStateTerminalEvent,
    kKPStateLongPress,
    kKPStateCount,
} KPState;

typedef enum _KPTransition {
    kKPTransitionDown,
    kKPTransitionUp,
    kKPTransitionTimeout,
    kKPTransitionCount,
} KPTransition;

class Event;
class IOHIDEventProcessor;

class Timer {
private:
    dispatch_source_t   _timer;
    dispatch_queue_t    _queue;
    Event *             _headEvent;
    
    void                timeoutHandler();
    
public:
    Timer();
    void                init(dispatch_queue_t q);
    void               	cancel(dispatch_queue_t q); 
    void                setQueue(dispatch_queue_t q) { _queue = q; };
    void                registerEventTimeout(Event * event, UInt64 deadline);
    void                checkEventTimeouts();
    void                removeEvent(Event * event);
    void                insertEvent(Event * event);
    void                updateTimeout();
};

class Event {
private:
    UInt64              _nextTimeout;
    Event *             _nextEvent;
    Event *             _nextTimerEvent;
    IOHIDEventProcessor * _owner;
    bool                _isComplete;
    
protected:
    IOHIDEventType      _eventType;
    UInt32              _usagePage;
    UInt32              _usage;
    Timer *             _timer;
    UInt32              _state;
    UInt32              _timeoutState;
    int                 _multiEventCount;
    bool                _terminalEventDispatched;
    
    UInt64              _secondEventTimeout;
    UInt64              _thirdEventTimeout;
    UInt64              _lastActionTimestamp;
    UInt64              _longPressTimeout;

public:
    void                init(IOHIDEventProcessor * owner, Timer * timer, IOHIDEventType type, UInt32 usagePage, UInt32 usage, UInt64 secondEventTimeout, UInt64 thirdEventTimeout, UInt64 longPressTimeout);

    bool                eventOccurred(IOHIDEventRef event, bool timedOut, IOHIDEventRef &terminalEvent);
    bool                conformsTo(IOHIDEventType type, UInt32 usagePage, UInt32 usage);
    
    Event();
    virtual ~Event();
    IOHIDEventType      getEventType() { return _eventType; };
    Event *             getNextEvent() { return _nextEvent; };
    void                setNextEvent(Event *event) { _nextEvent = event; };
    Event *             getNextTimerEvent() { return _nextTimerEvent; };
    void                setNextTimerEvent(Event *event) { _nextTimerEvent = event; };
    UInt64              getNextTimeout() { return _nextTimeout; };
    
    void                setEventType(IOHIDEventType type) { _eventType = type; };
    void                setSecondEventTimeout(UInt64 timeout) { _secondEventTimeout = timeout; };
    void                setThirdEventTimeout(UInt64 timeout) { _thirdEventTimeout = timeout; };
    void                setLongPressTimeout(UInt64 timeout) { _longPressTimeout = timeout; };
    
    void                dispatchEvent(IOHIDEventRef event, bool async);
    void                completed();
    
    void                setNextTimeout(UInt64 timeout) { _nextTimeout = timeout; };
    
    virtual IOHIDEventRef createSyntheticEvent(bool isTerminalEvent) = 0;
    virtual void        setMultiEventCount(IOHIDEventRef event, CFIndex count) = 0;
    
    virtual void        NoneEnter(IOHIDEventRef event) = 0;
    virtual void        FDEnter(IOHIDEventRef event) = 0;
    virtual void        FUEnter(IOHIDEventRef event) = 0;
    virtual void        SDEnter(IOHIDEventRef event) = 0;
    virtual void        SUEnter(IOHIDEventRef event) = 0;
    virtual void        TDEnter(IOHIDEventRef event) = 0;
    virtual void        TUEnter(IOHIDEventRef event) = 0;
    virtual void        TOEnter(IOHIDEventRef event) = 0;
    virtual void        TEEnter(IOHIDEventRef event) = 0;
    virtual void        LPEnter(IOHIDEventRef event) = 0;
    
    bool                stateHandler(KPTransition e, IOHIDEventRef event = 0);
    
    inline bool operator == (const Event &b) const
    {
        return ((b._usagePage == _usagePage) && (b._usage == _usage) && (b._eventType == _eventType));
    }

    bool                isComplete() {return _isComplete;};
    UInt64              epoch() {return _lastActionTimestamp;};
};

class ButtonEvent : public Event {
private:
    IOHIDEventRef       createSyntheticEvent(bool isTerminalEvent);
    void                setMultiEventCount(IOHIDEventRef event, CFIndex count);
    
    void                NoneEnter(IOHIDEventRef event);
    void                FDEnter(IOHIDEventRef event);
    void                FUEnter(IOHIDEventRef event);
    void                SDEnter(IOHIDEventRef event);
    void                SUEnter(IOHIDEventRef event);
    void                TDEnter(IOHIDEventRef event);
    void                TUEnter(IOHIDEventRef event);
    void                TOEnter(IOHIDEventRef event);
    void                TEEnter(IOHIDEventRef event);
    void                LPEnter(IOHIDEventRef event);
};

class TapEvent : public Event {
private:
    IOHIDEventRef       createSyntheticEvent(bool isTerminalEvent);
    void                setMultiEventCount(IOHIDEventRef event, CFIndex count);
    
    void                NoneEnter(IOHIDEventRef event);
    void                FDEnter(IOHIDEventRef event);
    void                FUEnter(IOHIDEventRef event);
    void                SDEnter(IOHIDEventRef event);
    void                SUEnter(IOHIDEventRef event);
    void                TDEnter(IOHIDEventRef event);
    void                TUEnter(IOHIDEventRef event);
    void                TOEnter(IOHIDEventRef event);
    void                TEEnter(IOHIDEventRef event);
    void                LPEnter(IOHIDEventRef event);
};


class IOHIDEventProcessor {
    
public:
    IOHIDEventProcessor(CFUUIDRef factoryID);
    ~IOHIDEventProcessor();
    HRESULT QueryInterface( REFIID iid, LPVOID *ppv );
    ULONG AddRef();
    ULONG Release();
    
    SInt32 match(IOHIDServiceRef service, IOOptionBits options);
    IOHIDEventRef filter(IOHIDEventRef event);
    IOHIDEventRef filterCopyEvent(IOHIDEventRef event);
    void open(IOHIDServiceRef service, IOOptionBits options);
    CFTypeRef copyPropertyForClient(CFStringRef key, CFTypeRef client);
    void setPropertyForClient(CFStringRef key, CFTypeRef property, CFTypeRef client);
    void dispatchEvent(IOHIDEventRef event, bool async);
    void returnToFreePool(Event * event);

private:
    static IOHIDServiceFilterPlugInInterface sIOHIDEventProcessorFtbl;
    IOHIDServiceFilterPlugInInterface *_serviceInterface;
    CFUUIDRef                   _factoryID;
    UInt32                      _refCount;
    SInt32                      _matchScore;

    dispatch_queue_t            _queue;
    IOHIDServiceRef             _service;
    IOHIDServiceEventCallback   _eventCallback;
    void *                      _eventTarget;
    void *                      _eventContext;
    
    bool                        _multiPressTrackingEnabled;
    CFArrayRef                  _multiPressUsagePairs;
    UInt64                      _multiPressDoublePressTimeout;
    UInt64                      _multiPressTriplePressTimeout;
    
    bool                        _multiTapTrackingEnabled;
    UInt64                      _multiTapDoubleTapTimeout;
    UInt64                      _multiTapTripleTapTimeout;

    UInt64                      _longPressTimeout;
    
    Event *                     _eventHead;
    Event *                     _freeButtonHead;
    Event *                     _freeTapHead;
    
    Timer *                     _timer;

    static HRESULT QueryInterface( void *self, REFIID iid, LPVOID *ppv );
    static ULONG AddRef( void *self );
    static ULONG Release( void *self );
    
    static SInt32 match(void * self, IOHIDServiceRef service, IOOptionBits options);
    static IOHIDEventRef filter(void * self, IOHIDEventRef event);
    static void setEventCallback(void * self, IOHIDServiceEventCallback callback, void * target, void * refcon);
    static IOHIDEventRef filterCopyEvent(void * self, IOHIDEventRef event);
    static CFTypeRef copyPropertyForClient(void * self,CFStringRef key, CFTypeRef client);
    static void setPropertyForClient(void * self,CFStringRef key, CFTypeRef property, CFTypeRef client);
    static void scheduleWithDispatchQueue(void * self, dispatch_queue_t queue);
    static void unscheduleFromDispatchQueue(void * self, dispatch_queue_t queue);
    static void open(void * self, IOHIDServiceRef service, IOOptionBits options);
   
    IOHIDEventProcessor(const IOHIDEventProcessor &);
    IOHIDEventProcessor &operator=(const IOHIDEventProcessor &);
    
    void scheduleWithDispatchQueue(dispatch_queue_t queue);
    void unscheduleFromDispatchQueue(dispatch_queue_t queue);
    
    void setEventCallback(IOHIDServiceEventCallback callback, void * target, void * refcon);

    void setTimeoutForState(UInt64 timeout, KPState state);
  
    void serialize (CFMutableDictionaryRef dict) const;

};
#endif /* IOHIDEventProcessorFilter_hpp */
