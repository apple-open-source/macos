/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
#ifndef _IOKIT_IOHIDQueueClass_H
#define _IOKIT_IOHIDQueueClass_H

#include <IOKit/IODataQueueShared.h>

#include "IOHIDDeviceClass.h"

class IOHIDQueueClass : public IOHIDIUnknown
{
private:
    // friends with our parent device class
    friend class IOHIDDeviceClass;
    
    // Disable copy constructors
    IOHIDQueueClass(IOHIDQueueClass &src);
    void operator =(IOHIDQueueClass &src);

protected:
    IOHIDQueueClass();
    virtual ~IOHIDQueueClass();

    static IOHIDQueueInterface	sHIDQueueInterfaceV1;

    struct InterfaceMap fHIDQueue;
    mach_port_t fAsyncPort;
    CFRunLoopSourceRef fCFSource;
    
    // if created, how we were created
    bool fIsCreated;
    UInt32 fCreatedFlags;
    UInt32 fCreatedDepth;
    unsigned int fQueueRef;
    
    // ptr to shared memory for queue
    IODataQueueMemory * fQueueMappedMemory;
    vm_size_t		fQueueMappedMemorySize;
    
    // owming device
    IOHIDDeviceClass *	fOwningDevice;
    
    // CFMachPortCallBack routine for IOHIDQueue
    static void queueEventSourceCallback(CFMachPortRef *cfPort, mach_msg_header_t *msg, CFIndex size, void *info);
    
    // Related IOHIDQueue call back info
    IOHIDCallbackFunction	fEventCallback;
    void *			fEventTarget;
    void *			fEventRefcon;
    
public:
    // set owner
    void setOwningDevice (IOHIDDeviceClass * owningDevice) { fOwningDevice = owningDevice; };
    
    // get interface map (for queryInterface)
    void * getInterfaceMap (void) { return &fHIDQueue; };

    // IOCFPlugin stuff
    virtual HRESULT queryInterface(REFIID iid, void **ppv);

    virtual IOReturn createAsyncEventSource(CFRunLoopSourceRef *source);
    virtual CFRunLoopSourceRef getAsyncEventSource();

    virtual IOReturn createAsyncPort(mach_port_t *port);
    virtual mach_port_t getAsyncPort();

    /* Basic IOHIDQueue interface */
    /* depth is the maximum number of elements in the queue before	*/
    /*   the oldest elements in the queue begin to be lost		*/
    virtual IOReturn create (UInt32 			flags,
                            UInt32			depth);
    virtual IOReturn dispose ();
    
    /* Any number of hid elements can feed the same queue */
    virtual IOReturn addElement (IOHIDElementCookie elementCookie,
                                UInt32 flags);
    virtual IOReturn removeElement (IOHIDElementCookie elementCookie);
    virtual Boolean hasElement (IOHIDElementCookie elementCookie);

    /* start/stop data delivery to a queue */
    virtual IOReturn start ();
    virtual IOReturn stop ();
    
    /* read next event from a queue */
    /* maxtime, if non-zero, limits read events to those that occured */
    /*   on or before maxTime */
    /* timoutMS is the timeout in milliseconds, a zero timeout will cause */
    /*	this call to be non-blocking (returning queue empty) if there */
    /*	is a NULL callback, and blocking forever until the queue is */
    /*	non-empty if their is a valid callback */
    virtual IOReturn getNextEvent (
                            IOHIDEventStruct *		event,
                            AbsoluteTime		maxTime,
                            UInt32 			timeoutMS);
    
    /* set a callback for notification when queue transistions from non-empty */
    /* callback, if non-NULL is a callback to be called when data is */
    /*  inserted to the queue  */
    /* callbackTarget and callbackRefcon are passed to the callback */
    virtual IOReturn setEventCallout (
                            IOHIDCallbackFunction  	callback,
                            void * 			callbackTarget,
                            void *			callbackRefcon);

    /* Get the current notification callout */
    virtual IOReturn getEventCallout (
                            IOHIDCallbackFunction * 	outCallback,
                            void ** 			outCallbackTarget,
                            void **			outCallbackRefcon);
    
/*
 * Routing gumf for CFPlugIn interfaces
 */
protected:

    static inline IOHIDQueueClass *getThis(void *self)
        { return (IOHIDQueueClass *) ((InterfaceMap *) self)->obj; };

    // Methods for routing the iocfplugin Interface v1r1

    // Methods for routing asynchronous completion plumbing.
    static IOReturn queueCreateAsyncEventSource(void *self,
                                                 CFRunLoopSourceRef *source);
    static CFRunLoopSourceRef queueGetAsyncEventSource(void *self);
    static IOReturn queueCreateAsyncPort(void *self, mach_port_t *port);
    static mach_port_t queueGetAsyncPort(void *self);

    /* Basic IOHIDQueue interface */
    static IOReturn queueCreate (void * 			self, 
                            UInt32 			flags,
                            UInt32			depth);
    static IOReturn queueDispose (void * self);
    
    /* Any number of hid elements can feed the same queue */
    static IOReturn queueAddElement (void * self,
                                IOHIDElementCookie elementCookie,
                                UInt32 flags);
    static IOReturn queueRemoveElement (void * self, IOHIDElementCookie elementCookie);
    static Boolean queueHasElement (void * self, IOHIDElementCookie elementCookie);

    /* start/stop data delivery to a queue */
    static IOReturn queueStart (void * self);
    static IOReturn queueStop (void * self);
    
    /* read next event from a queue */
    static IOReturn queueGetNextEvent (
                            void * 			self,
                            IOHIDEventStruct *		event,
                            AbsoluteTime		maxTime,
                            UInt32 			timeoutMS);
    
    /* set a callback for notification when queue transistions from non-empty */
    static IOReturn queueSetEventCallout (
                            void * 			self,
                            IOHIDCallbackFunction  	callback,
                            void * 			callbackTarget,
                            void *			callbackRefcon);

    /* Get the current notification callout */
    static IOReturn queueGetEventCallout (
                            void * 			self,
                            IOHIDCallbackFunction * 	outCallback,
                            void ** 			outCallbackTarget,
                            void **			outCallbackRefcon);



/*
 * Internal functions
 */
    
};

#endif /* !_IOKIT_IOHIDQueueClass_H */
