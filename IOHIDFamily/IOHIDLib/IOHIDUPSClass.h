/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#ifndef _IOKIT_IOHIDUPSClass_H
#define _IOKIT_IOHIDUPSClass_H


#include <IOKit/ps/IOUPSPlugIn.h>
#include <IOKit/hid/IOHIDLib.h>
#include "IOHIDIUnknown.h"

//---------------------------------------------------------------------------
// UPSElementStruct
//---------------------------------------------------------------------------
struct UPSHIDElement {
    SInt32		currentValue;
    SInt32		usagePage;
    SInt32		usage;
    SInt32		unit;
    SInt8		unitExponent;
    bool		isCommand;
    bool		isDesiredCollection;
    bool		isDesiredType;
    bool		shouldPoll;
    double		multiplier;
    IOHIDElementType	type;
    IOHIDElementCookie	cookie;
    IOReturn    lastReturn;
};

#define kIOHIDUnitVolt		0xf0d121
#define kIOHIDUnitAmp		0x100001
#define kIOHIDUnitAmpSec    0x101001
#define kIOHIDUnitKelvin    0x10001

#define kIOHIDUnitExponentVolt  7

class IOHIDUPSClass : public IOHIDIUnknown
{
private:
    // Disable copy constructors
    IOHIDUPSClass(IOHIDUPSClass &src);
    void operator =(IOHIDUPSClass &src);

protected:
    IOHIDUPSClass();
    virtual ~IOHIDUPSClass();

    static IOCFPlugInInterface		sIOCFPlugInInterfaceV1;
    static IOUPSPlugInInterface_v140		sUPSPlugInInterface_v140;

    struct InterfaceMap 		_upsDevice;
    io_service_t 			_service;

    CFTypeRef                   _timerEventSource;
    CFTypeRef                   _asyncEventSource;
    
    IOHIDDeviceInterface122 **		_hidDeviceInterface;
    IOHIDQueueInterface **		_hidQueueInterface;
    IOHIDOutputTransactionInterface **	_hidTransactionInterface;

    CFMutableDictionaryRef		_hidProperties;
    CFMutableDictionaryRef		_hidElements;
    CFMutableDictionaryRef		_upsElements;
    
    CFMutableDictionaryRef		_upsEvent;
    CFMutableDictionaryRef		_upsProperties;
    CFSetRef				_upsCapabilities;

    IOUPSEventCallbackFunction		_eventCallback;
    void *				_eventTarget;
    void *				_eventRefcon;
    
    bool				_isACPresent;


    static inline IOHIDUPSClass *getThis(void *self)
        { return (IOHIDUPSClass *) ((InterfaceMap *) self)->obj; };

    // IOCFPlugInInterface methods
    static IOReturn _probe(void *self,
                                CFDictionaryRef propertyTable,
                                io_service_t service, SInt32 *order);

    static IOReturn _start(void *self,
                                CFDictionaryRef propertyTable,
                                io_service_t service);

    static IOReturn _stop(void *self);
    
    // IOUPSPlugInInterface methods
    static IOReturn _getProperties(
                            void * 			self,
                            CFDictionaryRef *		properties);

    static IOReturn _getCapabilities(
                            void * 			self,
                            CFSetRef *			capabilities);

    static IOReturn _getEvent(
                            void * 			self,
                            CFDictionaryRef *		event);

    static IOReturn _setEventCallback(
                            void * 			self,
                            IOUPSEventCallbackFunction	callback,
                            void *			target,
                            void *			refcon);

    static IOReturn _sendCommand(
                            void * 			self,
                            CFDictionaryRef		command);
                            
    static IOReturn _createAsyncEventSource(
                            void *          self,
                            CFTypeRef *     eventSource);

    static void _queueCallbackFunction(
                            void * 			target, 
                            IOReturn 			result, 
                            void * 			refcon, 
                            void * 			sender);

    static void _timerCallbackFunction(
                            CFRunLoopTimerRef 		timer, 
                            void *			refCon);
                            
    bool 	findElements();
    
    void 	storeUPSElement(CFStringRef psKey, UPSHIDElement * newElementRef);

    bool	updateElementValue(UPSHIDElement *	tempHIDElement, IOReturn * error);

    bool	setupQueue();

    bool	processEvent(UPSHIDElement *		hidElement);
                           
public:
    // IOCFPlugin stuff
    static IOCFPlugInInterface **alloc();

    virtual HRESULT queryInterface(REFIID iid, void **ppv);

    virtual IOReturn probe(
                            CFDictionaryRef 		propertyTable,
                            io_service_t 		service, 
                            SInt32 *			order);
                            
    virtual IOReturn start(
                            CFDictionaryRef 		propertyTable,
                            io_service_t 		service);
                            
    virtual IOReturn stop();

    virtual IOReturn getProperties(
                            CFDictionaryRef *		properties);

    virtual IOReturn getCapabilities(
                            CFSetRef *			capabilities);

    virtual IOReturn getEvent(
                            CFDictionaryRef *		event,
                            bool *			changed = NULL);

    virtual void getEventProcess(
                            UPSHIDElement * 		elementRef, 
                            CFStringRef 		psKey, 
                            bool * 			changed);

    virtual IOReturn setEventCallback(
                            IOUPSEventCallbackFunction	callback,
                            void *			target,
                            void *			refcon);

    virtual IOReturn sendCommand(
                            CFDictionaryRef 		command);
                            
    virtual void sendCommandProcess(
                            UPSHIDElement * 		elementRef, 
                            SInt32 			value);

    virtual IOReturn createAsyncEventSource(
                            CFTypeRef *       eventSource);

};

#endif /* !_IOKIT_IOHIDUPSClass_H */
