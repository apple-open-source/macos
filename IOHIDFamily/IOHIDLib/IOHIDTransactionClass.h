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
#ifndef _IOKIT_IOHIDOutputTransactionClass_H
#define _IOKIT_IOHIDOutputTransactionClass_H

#include <IOKit/hid/IOHIDLib.h>
#include "IOHIDDeviceClass.h"

class IOHIDTransactionClass : public IOHIDIUnknown
{
    // Disable copy constructors
    IOHIDTransactionClass(IOHIDTransactionClass &src);
    void operator =(IOHIDTransactionClass &src);

protected:
    static IOHIDDeviceTransactionInterface	sHIDTransactionInterface;

    struct InterfaceMap fHIDTransaction;
    
    IOHIDTransactionDirectionType fDirection;
    
    // if created, how we were created
    bool fIsCreated;
        
    // owming device
    IOHIDDeviceClass *	fOwningDevice;
        
    // Related transaction call back info
    IOHIDCallback	fEventCallback;
    void *			fEventRefcon;
    
    // The transaction linked list
    CFMutableDictionaryRef	fElementDictionaryRef;
    
    // CFMachPortCallBack routine
    static void _eventSourceCallback(CFMachPortRef *cfPort, mach_msg_header_t *msg, CFIndex size, void *info);

    static inline IOHIDTransactionClass *getThis(void *self) { return (IOHIDTransactionClass *) ((InterfaceMap *) self)->obj; };

    static IOReturn _getAsyncPort(void *self, mach_port_t *port);
    static IOReturn _getAsyncEventSource(void *self, CFTypeRef *source);
    static IOReturn _getDirection(void *self, IOHIDTransactionDirectionType * pDirection);
    static IOReturn _setDirection(void *self, IOHIDTransactionDirectionType direction, IOOptionBits options);
    static IOReturn _addElement (void * self, IOHIDElementRef element, IOOptionBits options);
    static IOReturn _removeElement (void * self, IOHIDElementRef element, IOOptionBits options);                                                    
    static IOReturn _hasElement (void * self, IOHIDElementRef element, Boolean * pValue, IOOptionBits options);
    static IOReturn _setElementValue(void * self, IOHIDElementRef element, IOHIDValueRef event, IOOptionBits options);
    static IOReturn _getElementValue(void * self, IOHIDElementRef element, IOHIDValueRef *	pEvent, IOOptionBits options);
    static IOReturn _commit(void * self, uint32_t timeoutMS, IOHIDCallback callback, void * callbackRefcon, IOOptionBits options);
    static IOReturn _clear(void * self, IOOptionBits options);                                     

public:
    IOHIDTransactionClass();
    virtual ~IOHIDTransactionClass();

    // set owner
    void setOwningDevice (IOHIDDeviceClass * owningDevice) { fOwningDevice = owningDevice; };
    
    // get interface map (for queryInterface)
    void * getInterfaceMap () { return &fHIDTransaction; };

    virtual HRESULT queryInterface(REFIID iid, void **ppv);
    virtual IOReturn getAsyncEventSource(CFTypeRef *source);
    virtual IOReturn getAsyncPort(mach_port_t *port);
    virtual IOReturn getDirection(IOHIDTransactionDirectionType * pDirection);
    virtual IOReturn setDirection(IOHIDTransactionDirectionType direction, IOOptionBits options=0);
    virtual IOReturn create ();
    virtual IOReturn dispose ();    
    virtual IOReturn addElement (IOHIDElementRef element, IOOptionBits options=0);
    virtual IOReturn removeElement (IOHIDElementRef element, IOOptionBits options=0);
    virtual IOReturn hasElement (IOHIDElementRef element, Boolean * pValue, IOOptionBits options=0);
    virtual IOReturn setElementValue(IOHIDElementRef element, IOHIDValueRef event, IOOptionBits options=0);
    virtual IOReturn getElementValue(IOHIDElementRef element, IOHIDValueRef * pEvent, IOOptionBits options=0);
    virtual IOReturn commit(uint32_t timeoutMS, IOHIDCallback callback, void * callbackRefcon, IOOptionBits options=0);
    virtual IOReturn clear(IOOptionBits options=0);
};

class IOHIDOutputTransactionClass : public IOHIDTransactionClass
{
    // Disable copy constructors
    IOHIDOutputTransactionClass(IOHIDOutputTransactionClass &src);
    void operator =(IOHIDOutputTransactionClass &src);

    IOHIDCallbackFunction   fCallback;
    void *                  fTarget;
    void *                  fRefcon;

    static void _commitCallback(void * context, IOReturn result, void * sender);

protected:
    static IOHIDOutputTransactionInterface	sHIDOutputTransactionInterface;

    static inline IOHIDOutputTransactionClass *getThis(void *self) { return (IOHIDOutputTransactionClass *) ((InterfaceMap *) self)->obj; };

    static IOReturn _createAsyncEventSource(void * self, CFRunLoopSourceRef * pSource);
    static CFRunLoopSourceRef _getAsyncEventSource(void *self);
    static mach_port_t _getAsyncPort(void *self);
    static IOReturn _create (void * self);
    static IOReturn _dispose (void * self);    
    static IOReturn _addElement (void * self, IOHIDElementCookie cookie);
    static IOReturn _removeElement (void * self, IOHIDElementCookie cookie);                                                    
    static Boolean _hasElement (void * self, IOHIDElementCookie cookie);
    static IOReturn _setElementDefault(void * self, IOHIDElementCookie cookie, IOHIDEventStruct * valueEvent); 
    static IOReturn _getElementDefault(void * self, IOHIDElementCookie cookie, IOHIDEventStruct * valueEvent);
    static IOReturn _setElementValue(void * self, IOHIDElementCookie cookie, IOHIDEventStruct * pEvent);
    static IOReturn _getElementValue(void * self, IOHIDElementCookie cookie, IOHIDEventStruct *	pEvent);
    static IOReturn _commit(void * self, uint32_t timeoutMS, IOHIDCallbackFunction callback, void * callbackTarget, void * callbackRefcon);
    static IOReturn _clear(void * self);                                     

public:
    IOHIDOutputTransactionClass();
    virtual ~IOHIDOutputTransactionClass();

    virtual HRESULT queryInterface(REFIID iid, void **ppv);
    virtual IOReturn create ();
    virtual IOReturn createAsyncEventSource(CFRunLoopSourceRef * pSource);
    virtual IOReturn addElement (IOHIDElementCookie cookie);
    virtual IOReturn removeElement (IOHIDElementCookie cookie);
    virtual Boolean hasElement (IOHIDElementCookie cookie);
    virtual IOReturn setElementValue(IOHIDElementCookie cookie, IOHIDEventStruct * pEvent, IOOptionBits options = 0);
    virtual IOReturn getElementValue(IOHIDElementCookie	cookie, IOHIDEventStruct * pEvent, IOOptionBits options = 0);
    virtual IOReturn commit(uint32_t timeoutMS, IOHIDCallbackFunction callback, void * target, void * refcon);
};

#endif /* !_IOKIT_IOHIDOutputTransactionClass_H */
