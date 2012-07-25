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
#ifndef _IOKIT_IOHIDQueueClass_H
#define _IOKIT_IOHIDQueueClass_H

#include <IOKit/IODataQueueShared.h>

#include "IOHIDDeviceClass.h"

class IOHIDQueueClass : public IOHIDIUnknown
{
    // Disable copy constructors
    IOHIDQueueClass(IOHIDQueueClass &src);
    void operator =(IOHIDQueueClass &src);

protected:
    static IOHIDDeviceQueueInterface	sHIDQueueInterfaceV2;

    struct InterfaceMap fHIDQueue;
    mach_port_t         fAsyncPort;
    CFMachPortRef       fCFMachPort;
    CFRunLoopSourceRef  fCFSource;
    
    // if created, how we were created
    bool                fIsCreated;
    uint32_t            fCreatedFlags;
    uint32_t            fCreatedDepth;
    uint64_t            fQueueRef;
    
    bool                fQueueEntrySizeChanged;
    
    // ptr to shared memory for queue
    IODataQueueMemory * fQueueMappedMemory;
    vm_size_t           fQueueMappedMemorySize;
    
    // owming device
    IOHIDDeviceClass *	fOwningDevice;
        
    // Related IOHIDQueue call back info
    IOHIDCallback       fEventCallback;
    void *              fEventRefcon;
    CFMutableSetRef     fElements;

    static IOReturn _getAsyncEventSource(void *self, CFTypeRef *source);
    static IOReturn _getAsyncPort(void *self, mach_port_t *port);
    static IOReturn _setDepth(void *self, uint32_t depth, IOOptionBits options);
    static IOReturn _getDepth(void *self, uint32_t *pDepth);
    static IOReturn _addElement (void * self, IOHIDElementRef element, IOOptionBits options);
    static IOReturn _removeElement (void * self, IOHIDElementRef element, IOOptionBits options);
    static IOReturn _hasElement (void * self, IOHIDElementRef element, Boolean *pValue, IOOptionBits options);
    static IOReturn _start (void * self, IOOptionBits options);
    static IOReturn _stop (void * self, IOOptionBits options);    
    static IOReturn _copyNextEventValue (void * self, IOHIDValueRef * pEvent, uint32_t timeout, IOOptionBits options);
    static IOReturn _setEventCallback ( void * self, IOHIDCallback callback, void * refcon);
    
public:
    IOHIDQueueClass();
    virtual ~IOHIDQueueClass();

    void setOwningDevice (IOHIDDeviceClass * owningDevice) { fOwningDevice = owningDevice; };
    void * getInterfaceMap () { return &fHIDQueue; };

    virtual HRESULT queryInterface(REFIID iid, void **ppv);
    virtual IOReturn getAsyncEventSource(CFTypeRef *source);
    virtual IOReturn getAsyncPort(mach_port_t *port);
	IOReturn setAsyncPort(mach_port_t port);

    virtual IOReturn create (IOOptionBits options, uint32_t depth);
    virtual IOReturn dispose ();
    virtual IOReturn getDepth(uint32_t * pDepth);
    virtual IOReturn addElement (IOHIDElementRef element, IOOptionBits options = 0);
    virtual IOReturn removeElement (IOHIDElementRef element, IOOptionBits options = 0);
    virtual IOReturn hasElement (IOHIDElementRef element, Boolean * pValue, IOOptionBits options = 0);
    virtual IOReturn start (IOOptionBits options = 0);
    virtual IOReturn stop (IOOptionBits options = 0);
    virtual IOReturn copyNextEventValue (IOHIDValueRef * pEvent, uint32_t timeout, IOOptionBits options = 0);
    virtual IOReturn setEventCallback (IOHIDCallback callback, void * refcon);

    static void queueEventSourceCallback(CFMachPortRef cfPort, mach_msg_header_t *msg, CFIndex size, void *info);

    static inline IOHIDQueueClass *getThis(void *self) { return (IOHIDQueueClass *) ((InterfaceMap *) self)->obj; };
};


class IOHIDObsoleteQueueClass : public IOHIDQueueClass
{
    // Disable copy constructors
    IOHIDObsoleteQueueClass(IOHIDObsoleteQueueClass &src);
    void operator =(IOHIDObsoleteQueueClass &src);

    IOHIDCallbackFunction   fCallback;
    void *                  fTarget;
    void *                  fRefcon;

    static void _eventCallback(void * refcon, IOReturn result, void * sender);
protected:

    static IOHIDQueueInterface	sHIDQueueInterface;
    static IOReturn _createAsyncEventSource(void * self, CFRunLoopSourceRef * pSource);
    static CFRunLoopSourceRef _getAsyncEventSource(void *self);
    static mach_port_t  _getAsyncPort(void *self);
    static IOReturn _create(void * self, uint32_t flags, uint32_t depth);
    static IOReturn _dispose (void * self);
    static IOReturn _addElement (void * self, IOHIDElementCookie cookie, uint32_t flags);
    static IOReturn _removeElement (void * self, IOHIDElementCookie cookie);
    static Boolean  _hasElement (void * self, IOHIDElementCookie cookie);
    static IOReturn _start (void * self);
    static IOReturn _stop (void * self);    
    static IOReturn _getNextEvent (void * self, IOHIDEventStruct * event, AbsoluteTime maxTime, uint32_t timeoutMS);
    static IOReturn _getEventCallout (void * self, IOHIDCallbackFunction * pCallback,  void ** pTarget, void ** pRefcon);
    static IOReturn _setEventCallout (void * self, IOHIDCallbackFunction callback, void * target, void * refcon);
    
public:
    IOHIDObsoleteQueueClass();

    virtual HRESULT     queryInterface(REFIID iid, void **ppv);
    virtual IOReturn    createAsyncEventSource(CFRunLoopSourceRef * pSource);
    virtual IOReturn    addElement (IOHIDElementCookie cookie, 
                                    uint32_t flags);
    virtual IOReturn    addElement (IOHIDElementRef element, 
                                    IOOptionBits options = 0);
    virtual IOReturn    removeElement (IOHIDElementCookie cookie);
    virtual IOReturn    removeElement (IOHIDElementRef element, 
                                       IOOptionBits options = 0);
    virtual Boolean     hasElement (IOHIDElementCookie cookie);
    virtual IOReturn    hasElement (IOHIDElementRef element, 
                                    Boolean * pValue, 
                                    IOOptionBits options = 0);
    virtual IOReturn    getNextEvent (IOHIDEventStruct * event, AbsoluteTime maxTime, uint32_t timeoutMS);
    virtual IOReturn    getEventCallout (IOHIDCallbackFunction * pCallback, void ** pTarget, void ** pRefcon);
    virtual IOReturn    setEventCallout (IOHIDCallbackFunction callback, void * target, void * refcon);

    static inline IOHIDObsoleteQueueClass *getThis(void *self){ return (IOHIDObsoleteQueueClass *)((InterfaceMap *) self)->obj; };

};


#endif /* !_IOKIT_IOHIDQueueClass_H */
