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
#ifndef _IOKIT_IOHIDDeviceClass_H
#define _IOKIT_IOHIDDeviceClass_H

// FIXME
// #include <IOKit/hid/IOHIDLib.h>
#include "IOHIDLib.h"

#include "IOHIDIUnknown.h"

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
    unsigned long	valueLocation;
    CFDictionaryRef	elementDictionaryRef;
};
typedef struct IOHIDElementStruct IOHIDElementStruct;


class IOHIDDeviceClass : public IOHIDIUnknown
{
typedef struct MyPrivateData {
    io_object_t				notification;
    IOHIDDeviceClass *		self;
} MyPrivateData;

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

    static IOCFPlugInInterface		sIOCFPlugInInterfaceV1;
    static IOHIDDeviceInterface122	sHIDDeviceInterfaceV122;

    struct InterfaceMap 	fHIDDevice;
    io_service_t 		fService;
    io_connect_t 		fConnection;
    IONotificationPortRef 		fAsyncPort;
    CFRunLoopRef 		fRunLoop;
    IONotificationPortRef 	fNotifyPort;
    CFRunLoopSourceRef 		fCFSource;
	CFRunLoopSourceRef		fNotifyCFSource;
    bool 			fIsOpen;
    bool 			fIsLUNZero;
    bool			fIsTerminated;
    bool			fIsSeized;
    bool            fAsyncPortSetupDone;
    UInt32			fCachedFlags;
	
	MyPrivateData * fAsyncPrivateDataRef;
	MyPrivateData * fNotifyPrivateDataRef;
    
    IOHIDCallbackFunction 	fRemovalCallback;
    void *			fRemovalTarget;
    void *			fRemovalRefcon;
    
    CFMutableSetRef		fQueues;
    CFMutableSetRef		fDeviceElements;
    
    // ptr to shared memory for current values of elements
    vm_address_t 	fCurrentValuesMappedMemory;
    vm_size_t		fCurrentValuesMappedMemorySize;
    
    // array of leaf elements (those that can be used in get value)
    long fElementCount;
    IOHIDElementStruct * fElements;

    // array of report handler elements (those that can be used in get value)
    long 				fReportHandlerElementCount;
    IOHIDElementStruct * 		fReportHandlerElements;
    
    IOHIDQueueClass *		fReportHandlerQueue;
    
    IOHIDReportCallbackFunction		fInputReportCallback;
    void *				fInputReportTarget;
    void *				fInputReportRefcon;
    void *				fInputReportBuffer;
    UInt32				fInputReportBufferSize;   

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
                        
	IOReturn finishAsyncPortSetup();
	IOReturn finishReportHandlerQueueSetup();
	
	IOHIDQueueClass * createQueue(bool reportHandler=false);

    // Call back methods
    static void _cfmachPortCallback(CFMachPortRef cfPort, mach_msg_header_t *msg, CFIndex size, void *info);

    static void _hidReportCallback(void *refcon, IOReturn result, UInt32 bufferSize);
    
    static void _deviceNotification( void *refCon,
                                    io_service_t service,
                                    natural_t messageType,
                                    void *messageArgument );

    static void _hidReportHandlerCallback(void * target, IOReturn result, void * refcon, void * sender);
                           
public:
    // add/remove a queue 
    HRESULT attachQueue (IOHIDQueueClass * iohidQueue, bool reportHandler = false);
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

    virtual IOReturn startAllQueues(bool deviceInitiated = false);
    virtual IOReturn stopAllQueues(bool deviceInitiated = false);

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
                                
    virtual IOReturn copyMatchingElements(
                                CFDictionaryRef 		matchingDict, 
                                CFArrayRef * 			elements);
    
    virtual IOReturn setInterruptReportHandlerCallback(
                                void *				reportBuffer,
                                UInt32				reportBufferSize,
                                IOHIDReportCallbackFunction 	callback, 
                                void * 				callbackTarget, 
                                void * 				callbackRefcon);

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
                                
    static IOReturn deviceCopyMatchingElements(void * 		self, 
                                CFDictionaryRef			matchingDict, 
                                CFArrayRef * 			elements);
    
    static IOReturn deviceSetInterruptReportHandlerCallback(void * 	self, 
                                void *				reportBuffer,
                                UInt32				reportBufferSize,
                                IOHIDReportCallbackFunction 	callback, 
                                void * 				callbackTarget, 
                                void * 				callbackRefcon);

/*
 * Internal functions
 */
    
    static void		StaticCountElements (const void * value, void * parameter);
    static void		StaticCreateLeafElements (const void * value, void * parameter);

    kern_return_t	BuildElements (CFDictionaryRef properties, CFMutableSetRef set);
    long		CountElements (CFDictionaryRef properties, CFTypeRef element, CFStringRef key);
    kern_return_t	CreateLeafElements (CFDictionaryRef properties,
                                            CFMutableSetRef set,
                                            CFTypeRef element, 
                                            long * allocatedElementCount,
                                            CFStringRef key,
                                            IOHIDElementStruct *elements);
                                            
    kern_return_t	FindReportHandlers(CFDictionaryRef properties);
};

#endif /* !_IOKIT_IOHIDDeviceClass_H */
