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
#ifndef _IOKIT_IOHIDDeviceClass_H
#define _IOKIT_IOHIDDeviceClass_H

// FIXME
// #include <IOKit/hid/IOHIDLib.h>
#include "IOHIDLib.h"

#include "IOHIDIUnknown.h"

#define IOHID_PSEUDODEVICE	0

class IOHIDQueueClass;

struct IOHIDElementStruct
{
    unsigned long	cookie;
    long		type;
    long		min;
    long		max;
    long		usage;
    long		usagePage;
    long		bytes;
#if IOHID_PSEUDODEVICE
    long		currentValue;
    long		pauseCount;
    long		increment;
#else
    unsigned long	valueLocation;
#endif
};
typedef struct IOHIDElementStruct IOHIDElementStruct;


class IOHIDDeviceClass : public IOHIDIUnknown
{
private:
    // friends with queue class
    friend class IOHIDQueueClass;
    friend class IOHIDOutputTransactionClass;

    // Disable copy constructors
    IOHIDDeviceClass(IOHIDDeviceClass &src);
    void operator =(IOHIDDeviceClass &src);

protected:
    IOHIDDeviceClass();
    virtual ~IOHIDDeviceClass();

    static IOCFPlugInInterface	sIOCFPlugInInterfaceV1;
    static IOHIDDeviceInterface	sHIDDeviceInterfaceV1;

    struct InterfaceMap fHIDDevice;
    io_service_t fService;
    io_connect_t fConnection;
    mach_port_t fAsyncPort;
    CFRunLoopSourceRef fCFSource;
    bool fIsOpen;
    bool fIsLUNZero;
    
    // ptr to shared memory for current values of elements
    vm_address_t 	fCurrentValuesMappedMemory;
    vm_size_t		fCurrentValuesMappedMemorySize;
    
    // array of leaf elements (those that can be used in get value)
    long fElementCount;
    IOHIDElementStruct * fElements;

    // routines to create owned classes
    HRESULT queryInterfaceQueue (void **ppv);
    HRESULT queryInterfaceOutputTransaction (void **ppv);
    
    // helper function for get/query ElementValue
    IOReturn fillElementValue(IOHIDElementCookie		elementCookie,
                                IOHIDEventStruct *		valueEvent);    
                                
    void convertByteToWord( const UInt8 * src,
                        UInt32 *      dst,
                        UInt32        bitsToCopy);
    
    void convertWordToByte( const UInt32 * src,
                        UInt8 *        dst,
                        UInt32         bitsToCopy);
                        
    // Call back methods
    static void _hidReportCallback(void *refcon, IOReturn result, UInt32 bufferSize);
                           
public:
    // add/remove a queue 
    HRESULT attachQueue (IOHIDQueueClass * iohidQueue);
    HRESULT detachQueue (IOHIDQueueClass * iohidQueue);
    
    // add/remove a queue 
    HRESULT attachOutputTransaction (IOHIDOutputTransactionClass * iohidOutputTrans);
    HRESULT detachOutputTransaction (IOHIDOutputTransactionClass * iohidOutputTrans);

    
    // get an element info
    bool getElement(IOHIDElementCookie elementCookie, IOHIDElementStruct *element);
    IOHIDElementType getElementType (IOHIDElementCookie elementCookie);
    UInt32 getElementByteSize (IOHIDElementCookie elementCookie);

    // IOCFPlugin stuff
    static IOCFPlugInInterface **alloc();

    virtual HRESULT queryInterface(REFIID iid, void **ppv);

    virtual IOReturn probe(CFDictionaryRef propertyTable,
                           io_service_t service, SInt32 *order);
    virtual IOReturn start(CFDictionaryRef propertyTable,
                           io_service_t service);
    // No stop as such just map the deviceStop call onto close.

    virtual IOReturn createAsyncEventSource(CFRunLoopSourceRef *source);
    virtual CFRunLoopSourceRef getAsyncEventSource();

    virtual IOReturn createAsyncPort(mach_port_t *port);
    virtual mach_port_t getAsyncPort();

    virtual IOReturn open(UInt32 flags);
    virtual IOReturn close();

    virtual IOReturn setRemovalCallback(
                                   IOHIDCallbackFunction	removalCallback,
                                   void *			removalTarget,
                                   void *			removalRefcon);

    virtual IOReturn getElementValue(	IOHIDElementCookie	elementCookie,
                                        IOHIDEventStruct *	valueEvent);

    virtual IOReturn setElementValue(
                                IOHIDElementCookie		elementCookie,
                                IOHIDEventStruct *		valueEvent,
                                UInt32 				timeoutMS = 0,
                                IOHIDElementCallbackFunction	callback = NULL,
                                void * 				callbackTargetm = NULL,
                                void *				callbackRefcon = NULL,
                                bool				pushToDevice = false);

    virtual IOReturn queryElementValue(
                                IOHIDElementCookie		elementCookie,
                                IOHIDEventStruct *		valueEvent,
                                UInt32 				timeoutMS,
                                IOHIDElementCallbackFunction	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon);

    virtual IOReturn startAllQueues();
    virtual IOReturn stopAllQueues();

    virtual IOHIDQueueInterface ** allocQueue();
    
    virtual IOHIDOutputTransactionInterface ** allocOutputTransaction();

    // Added functions Post Jaguar
    virtual IOReturn setReport (IOHIDReportType			reportType,
                                UInt32				reportID,
                                void *				reportBuffer,
                                UInt32				reportBufferSize,
                                UInt32 				timeoutMS,
                                IOHIDReportCallbackFunction	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon);


    virtual IOReturn getReport (IOHIDReportType			reportType,
                                UInt32				reportID,
                                void *				reportBuffer,
                                UInt32 *			reportBufferSize,
                                UInt32 				timeoutMS,
                                IOHIDReportCallbackFunction	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon);

/*
 * Routing gumf for CFPlugIn interfaces
 */
protected:

    static inline IOHIDDeviceClass *getThis(void *self)
        { return (IOHIDDeviceClass *) ((InterfaceMap *) self)->obj; };

    // Methods for routing the iocfplugin Interface v1r1
    static IOReturn deviceProbe(void *self,
                                CFDictionaryRef propertyTable,
                                io_service_t service, SInt32 *order);

    static IOReturn deviceStart(void *self,
                                CFDictionaryRef propertyTable,
                                io_service_t service);

    static IOReturn deviceStop(void *self);	// Calls close()

    // Methods for routing asynchronous completion plumbing.
    static IOReturn deviceCreateAsyncEventSource(void *self,
                                                 CFRunLoopSourceRef *source);
    static CFRunLoopSourceRef deviceGetAsyncEventSource(void *self);
    static IOReturn deviceCreateAsyncPort(void *self, mach_port_t *port);
    static mach_port_t deviceGetAsyncPort(void *self);

    // Basic IOHIDDevice interface
    static IOReturn deviceOpen(void *self, UInt32 flags);
    static IOReturn deviceClose(void *self);

    /* removalCallback is called if the device is removed. */
    /* removeTarget and removalRefcon are passed to the callback. */
    static IOReturn deviceSetRemovalCallback(void * 		self,
                                   IOHIDCallbackFunction	removalCallback,
                                   void *			removalTarget,
                                   void *			removalRefcon);

    /* Polling the most recent value of an element */
    /* The timestamp in the event is the last time the element was changed. */
    /* This call is most useful for the input element type. */
    static IOReturn deviceGetElementValue(void * 		self,
                                IOHIDElementCookie	elementCookie,
                                IOHIDEventStruct *	valueEvent);

    /* This call sets a value in a device. */
    /* It is most useful for the feature element type. */
    /* Using IOOutputTransaction is a better choice for output elements  */
    /* timoutMS is the timeout in milliseconds, a zero timeout will cause */
    /*	this call to be non-blocking (returning queue empty) if there */
    /*	is a NULL callback, and blocking forever until the queue is */
    /*	non-empty if their is a valid callback */
    /* callback, if non-NULL is a callback to be called when data is */
    /*  inserted to the queue  */
    /* callbackTarget and callbackRefcon are passed to the callback */
    static IOReturn deviceSetElementValue(void *	 		self,
                                IOHIDElementCookie		elementCookie,
                                IOHIDEventStruct *		valueEvent,
                                UInt32 				timeoutMS,
                                IOHIDElementCallbackFunction	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon);

    /* This call actually querys the device. */
    /* It is most useful for the feature element type. */
    /* Not all devices support this call for inputs, but all do for features */
    /* timoutMS is the timeout in milliseconds, a zero timeout will cause */
    /*	this call to be non-blocking (returning queue empty) if there */
    /*	is a NULL callback, and blocking forever until the queue is */
    /*	non-empty if their is a valid callback */
    /* callback, if non-NULL is a callback to be called when data is */
    /*  inserted to the queue  */
    /* callbackTarget and callbackRefcon are passed to the callback */
    static IOReturn deviceQueryElementValue(void * 		self,
                                IOHIDElementCookie		elementCookie,
                                IOHIDEventStruct *		valueEvent,
                                UInt32 				timeoutMS,
                                IOHIDElementCallbackFunction	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon);

    /* start/stop data delivery every queue for a device */
    static IOReturn deviceStartAllQueues(void * self);
    static IOReturn deviceStopAllQueues(void * self);

    /* Wrapper to return instances of the IOHIDQueueInterface */
    static IOHIDQueueInterface ** deviceAllocQueue(void *self);
    
    /* Wrapper to return instances of the IOHIDOutputTransactionInterface */
    static IOHIDOutputTransactionInterface ** deviceAllocOutputTransaction (void *self);
    
    // Added functions Post Jaguar
    static IOReturn deviceSetReport (void * 			self,
                                IOHIDReportType			reportType,
                                UInt32				reportID,
                                void *				reportBuffer,
                                UInt32				reportBufferSize,
                                UInt32 				timeoutMS,
                                IOHIDReportCallbackFunction	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon);


    static IOReturn deviceGetReport (void * 			self,
                                IOHIDReportType			reportType,
                                UInt32				reportID,
                                void *				reportBuffer,
                                UInt32 *			reportBufferSize,
                                UInt32 				timeoutMS,
                                IOHIDReportCallbackFunction	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon);

/*
 * Internal functions
 */
    
    static void		StaticCountLeafElements (const void * value, void * parameter);
    static void		StaticCreateLeafElements (const void * value, void * parameter);

    kern_return_t	BuildElements (CFDictionaryRef properties);
    long		CountLeafElements (CFDictionaryRef properties, CFTypeRef element);
    kern_return_t	CreateLeafElements (CFDictionaryRef properties, 
                                            CFTypeRef element, 
                                            long * allocatedElementCount);
};

#endif /* !_IOKIT_IOHIDDeviceClass_H */
