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
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDValue.h>
#include <IOKit/hid/IOHIDElement.h>
#include <IOKit/hid/IOHIDLibPrivate.h>

#include "IOHIDIUnknown.h"

#define HIDLog(fmt, args...) {}

enum {
    kHIDSetElementValuePendEvent    = 0x00010000,
    kHIDGetElementValueForcePoll    = 0x00020000,
    kHIDGetElementValuePreventPoll  = 0x00040000,
    kHIDReportObsoleteCallback      = 0x00080000
};

class IOHIDQueueClass;
class IOHIDTransactionClass;

class IOHIDDeviceClass : public IOHIDIUnknown
{
    // friends with queue class
    friend class IOHIDQueueClass;
    friend class IOHIDTransactionClass;

    // Disable copy constructors
    IOHIDDeviceClass(IOHIDDeviceClass &src);
    void operator =(IOHIDDeviceClass &src);

protected:
    typedef struct MyPrivateData {
        io_object_t				notification;
        IOHIDDeviceClass *		self;
    } MyPrivateData;

    IOHIDDeviceClass();
    virtual ~IOHIDDeviceClass();

    static IOCFPlugInInterface		sIOCFPlugInInterfaceV1;
    static IOHIDDeviceDeviceInterface	sHIDDeviceInterfaceV2;

    struct InterfaceMap             fHIDDevice;
    io_service_t                    fService;
    io_connect_t                    fConnection;
    CFRunLoopRef                    fRunLoop;
    IONotificationPortRef           fNotifyPort;
	CFRunLoopSourceRef              fNotifyCFSource;
    IONotificationPortRef           fAsyncPort;
    CFMachPortRef                   fAsyncCFMachPort;
    CFRunLoopSourceRef              fAsyncCFSource;
    mach_port_t                     fDeviceValidPort;
    bool                            fIsOpen;
    bool                            fIsLUNZero;
    bool                            fIsTerminated;
    bool                            fAsyncPortSetupDone;
	
	MyPrivateData *                 fAsyncPrivateDataRef;
	MyPrivateData *                 fNotifyPrivateDataRef;
    
    IOHIDCallbackFunction           fRemovalCallback;
    void *                          fRemovalTarget;
    void *                          fRemovalRefcon;
    
    CFMutableSetRef                 fQueues;
    CFMutableDictionaryRef          fElementCache;
    
    CFMutableDictionaryRef          fProperties;
    
    // ptr to shared memory for current values of elements
#if !__LP64__
    vm_address_t                    fCurrentValuesMappedMemory;
    vm_size_t                       fCurrentValuesMappedMemorySize;
#else
    mach_vm_address_t                    fCurrentValuesMappedMemory;
    mach_vm_size_t                       fCurrentValuesMappedMemorySize;
#endif
    
    // array of leaf elements (those that can be used in get value)
    uint32_t                        fElementCount;
    CFMutableDataRef                fElementData;
    IOHIDElementStruct *            fElements;

    // array of report handler elements (those that can be used in get value)
    uint32_t                        fReportHandlerElementCount;
    CFMutableDataRef                fReportHandlerElementData;
    IOHIDElementStruct *            fReportHandlerElements;
    
    IOHIDQueueClass *               fReportHandlerQueue;
    
    IOHIDReportCallback             fInputReportCallback;
    void *                          fInputReportRefcon;
    uint8_t *                       fInputReportBuffer;
    CFIndex                         fInputReportBufferSize;
    IOOptionBits                    fInputReportOptions;
    
    uint64_t                        fGeneration;

    virtual IOReturn createSharedMemory(uint64_t generation);
    virtual IOReturn releaseSharedMemory();
    
    virtual Boolean isValid();
    
    // routines to create owned classes
    virtual HRESULT queryInterfaceQueue (CFUUIDRef uuid, void **ppv);
    virtual HRESULT queryInterfaceTransaction (CFUUIDRef uuid, void **ppv);

    IOReturn buildElements(uint32_t type, CFMutableDataRef * pDataRef, IOHIDElementStruct ** buffer, uint32_t * count );

    // helper function for copyMatchingElements
    bool getElementDictIntValue(CFDictionaryRef element, CFStringRef key, uint32_t * value);
    void setElementDictIntValue(CFMutableDictionaryRef element, CFStringRef key,  uint32_t value);
    void setElementDictBoolValue(CFMutableDictionaryRef  element, CFStringRef key,  bool value);
    CFTypeRef createElement(CFDataRef data, IOHIDElementStruct * element, uint32_t index, CFTypeRef parentElement, CFMutableDictionaryRef elementCache, 
                                bool * isElementCached = false, IOOptionBits options = 0);
    
    IOReturn getCurrentElementValueAndGeneration(IOHIDElementRef element, IOHIDValueRef *pEvent = 0, uint32_t * pGeneration = 0);    
                                
	IOReturn finishAsyncPortSetup();
	IOReturn finishReportHandlerQueueSetup();
	
	virtual IOHIDQueueClass * createQueue(bool reportHandler=false);

    // Call back methods
    static void _cfmachPortCallback(CFMachPortRef cfPort, mach_msg_header_t *msg, CFIndex size, void *info);
    static void _hidReportCallback(void *refcon, IOReturn result, uint32_t bufferSize);
    static void _deviceNotification(void *refCon, io_service_t service, natural_t messageType, void *messageArgument );
    static void _hidReportHandlerCallback(void * refcon, IOReturn result, void * sender);
                           
/*
 * Routing gumf for CFPlugIn interfaces
 */
    static inline IOHIDDeviceClass *getThis(void *self)
        { return (IOHIDDeviceClass *) ((InterfaceMap *) self)->obj; };

    // Methods for routing the iocfplugin Interface v1r1
    static IOReturn _probe(void *self, CFDictionaryRef propertyTable, io_service_t service, SInt32 *order);
    static IOReturn _start(void *self, CFDictionaryRef propertyTable, io_service_t service);
    static IOReturn _stop(void *self);	// Calls close()

    // IOHIDDeviceDeviceInterface
    static IOReturn _open(void * self, IOOptionBits options);
    static IOReturn _close(void * self, IOOptionBits options);
    static IOReturn _getProperty(void * self, CFStringRef key, CFTypeRef * pProperty);
    static IOReturn _setProperty(void * self, CFStringRef key, CFTypeRef property);
    static IOReturn _getAsyncPort(void * self, mach_port_t * port);
    static IOReturn _getAsyncEventSource(void * self, CFTypeRef * pSource);
    static IOReturn _copyMatchingElements(void * self, CFDictionaryRef matchingDict, CFArrayRef * pElements, IOOptionBits options);
    static IOReturn _setInterruptReportCallback(void * self, uint8_t * report, CFIndex reportLength, 
                            IOHIDReportCallback callback, void * refcon, IOOptionBits options);
    static IOReturn _getReport(void * self, IOHIDReportType reportType, uint32_t reportID, uint8_t * report, CFIndex * pReportLength,
                            uint32_t timeout, IOHIDReportCallback callback, void * refcon, IOOptionBits options);
    static IOReturn _setReport(void * self, IOHIDReportType reportType, uint32_t reportID, const uint8_t * report, CFIndex reportLength,
                            uint32_t timeout, IOHIDReportCallback callback, void * refcon, IOOptionBits options);
    static IOReturn _getElementValue(void * self, IOHIDElementRef element, IOHIDValueRef * pEvent,
                            uint32_t timeout, IOHIDValueCallback callback, void * refcon, IOOptionBits options);
    static IOReturn _setElementValue(void * self, IOHIDElementRef element, IOHIDValueRef event,
                            uint32_t timeout, IOHIDValueCallback callback, void * refcon, IOOptionBits options);

public:
    void * getInterfaceMap () { return &fHIDDevice; };

    // add/remove a queue 
    HRESULT attachQueue (IOHIDQueueClass * iohidQueue, bool reportHandler = false);
    HRESULT detachQueue (IOHIDQueueClass * iohidQueue);
    
    // add/remove a queue 
    HRESULT attachTransaction (IOHIDTransactionClass * transaction);
    HRESULT detachTransaction (IOHIDTransactionClass * transaction);

    
    // get an element info
    bool getElementStructPtr(IOHIDElementCookie elementCookie, IOHIDElementStruct ** ppElementStruct, uint32_t * pIndex=0, CFDataRef * pData =0);
    bool getElementStruct(IOHIDElementCookie elementCookie, IOHIDElementStruct * pElementStruct);
    uint32_t getElementByteSize (IOHIDElementCookie elementCookie);
    IOHIDElementRef getElement(IOHIDElementCookie elementCookie);

    // IOCFPlugin stuff
    static IOCFPlugInInterface **alloc();

    virtual HRESULT queryInterface(REFIID iid, void **ppv);

    virtual IOReturn probe(CFDictionaryRef propertyTable, io_service_t service, SInt32 *order);
    virtual IOReturn start(CFDictionaryRef propertyTable, io_service_t service);

    virtual IOReturn getProperty(CFStringRef key, CFTypeRef * pProperty);
    virtual IOReturn setProperty(CFStringRef key, CFTypeRef property);

    virtual IOReturn getAsyncEventSource(CFTypeRef *source);
    virtual IOReturn getAsyncPort(mach_port_t *port);

    virtual IOReturn open(IOOptionBits options = 0);
    virtual IOReturn close(IOOptionBits options = 0);

    virtual IOReturn startAllQueues();
    virtual IOReturn stopAllQueues();
    
    virtual IOReturn setReport(IOHIDReportType reportType, uint32_t reportID, const uint8_t * report, CFIndex reportLength, 
                                uint32_t timeout, IOHIDReportCallback callback, void * refcon, IOOptionBits options = 0);
    virtual IOReturn getReport(IOHIDReportType reportType, uint32_t reportID, uint8_t * report, CFIndex * pReportLength, 
                                uint32_t timeout, IOHIDReportCallback callback, void * refcon, IOOptionBits options = 0);
    virtual IOReturn copyMatchingElements(CFDictionaryRef matchingDict, CFArrayRef * elements, CFTypeRef parentElement=0, CFMutableDictionaryRef elementCache=0, IOOptionBits options=0);
    virtual IOReturn setInterruptReportCallback(uint8_t * report, CFIndex reportLength, IOHIDReportCallback callback, void * refcon, IOOptionBits options = 0);

    virtual IOReturn getElementValue(IOHIDElementRef element, IOHIDValueRef * pEvent, 
                                uint32_t timeout = 0, IOHIDValueCallback callback = 0, void * refcon = 0, IOOptionBits options = 0);

    virtual IOReturn setElementValue(IOHIDElementRef element, IOHIDValueRef event, 
                                uint32_t timeout = 0, IOHIDValueCallback callback = 0, void * refcon = 0, IOOptionBits options = 0);
};


class IOHIDObsoleteDeviceClass : public IOHIDDeviceClass
{
    // friends with queue class
    friend class IOHIDObsoleteQueueClass;
    friend class IOHIDOutputTransactionClass;

    // Disable copy constructors
    IOHIDObsoleteDeviceClass(IOHIDObsoleteDeviceClass &src);
    void operator =(IOHIDObsoleteDeviceClass &src);
    
    void * fInputReportContext;

    static void _reportCallback(
                                        void *                  context, 
                                        IOReturn                result, 
                                        void *                  sender, 
                                        IOHIDReportType         type, 
                                        uint32_t                reportID,
                                        uint8_t *               report, 
                                        CFIndex                 reportLength);

protected:

    static IOHIDDeviceInterface122	sHIDDeviceInterfaceV122;

    // routines to create owned classes
    virtual HRESULT queryInterfaceTransaction (CFUUIDRef uuid, void **ppv);
	virtual IOHIDQueueClass * createQueue(bool reportHandler=false);

    static inline IOHIDObsoleteDeviceClass *getThis(void *self) { return (IOHIDObsoleteDeviceClass *) ((InterfaceMap *) self)->obj; };

    // Methods for routing asynchronous completion plumbing.
    static IOReturn             _createAsyncEventSource(void * self, CFRunLoopSourceRef * pSource);
    static CFRunLoopSourceRef   _getAsyncEventSource(void *self);
    static mach_port_t          _getAsyncPort(void *self);
    static IOReturn             _close(void *self);
    static IOReturn             _setRemovalCallback(void * self, IOHIDCallbackFunction callback, void * target, void * refcon);
    static IOReturn             _getElementValue(void * self, IOHIDElementCookie elementCookie, IOHIDEventStruct * valueEvent);
    static IOReturn             _setElementValue(void * self, IOHIDElementCookie cookie, IOHIDEventStruct * pEvent, uint32_t timeout, IOHIDElementCallbackFunction callback, void * target,  void * refcon);
    static IOReturn             _queryElementValue(void * self, IOHIDElementCookie cookie, IOHIDEventStruct * pEvent, uint32_t timeout, IOHIDElementCallbackFunction callback, void * target, void * refcon);
    static IOReturn             _startAllQueues(void * self);
    static IOReturn             _stopAllQueues(void * self);
    static IOHIDQueueInterface **               _allocQueue(void *self);
    static IOHIDOutputTransactionInterface **   _allocOutputTransaction (void *self);
    static IOReturn             _setReport (void * self, IOHIDReportType type, uint32_t id, void * report, uint32_t length, uint32_t timeout, IOHIDReportCallbackFunction callback, void * target, void * refcon);
    static IOReturn             _getReport (void * self, IOHIDReportType type, uint32_t id, void * report, uint32_t * pLength, uint32_t timeout, IOHIDReportCallbackFunction callback, void * target, void * refcon);
    static IOReturn             _copyMatchingElements(void * self, CFDictionaryRef matchingDict, CFArrayRef * elements);
    static IOReturn             _setInterruptReportHandlerCallback(void * self, void * report, uint32_t length, IOHIDReportCallbackFunction callback, void * target, void * refcon);                           


    static void                 _elementValueCallback(void * context, IOReturn result, void * sender, IOHIDValueRef value);

public:
    IOHIDObsoleteDeviceClass();
    static IOCFPlugInInterface **alloc();
    virtual HRESULT queryInterface(REFIID iid, void **ppv);

    virtual IOReturn createAsyncEventSource(CFRunLoopSourceRef * pSource);
    virtual IOReturn setRemovalCallback(IOHIDCallbackFunction removalCallback, void * removalTarget, void * removalRefcon);
    virtual IOReturn setElementValue(IOHIDElementCookie cookie, IOHIDEventStruct * pEvent, uint32_t timeout = 0, IOHIDElementCallbackFunction callback = NULL, void * target = NULL, void * refcon = NULL, IOOptionBits options = 0);
    virtual IOReturn getElementValue(IOHIDElementCookie cookie, IOHIDEventStruct * pEvent);
    virtual IOReturn queryElementValue(IOHIDElementCookie cookie, IOHIDEventStruct * pEvent, uint32_t timeout, IOHIDElementCallbackFunction callback, void * target, void * refcon);
    virtual IOReturn setReport(IOHIDReportType type, uint32_t id, void * report, uint32_t length, uint32_t timeout, IOHIDReportCallbackFunction callback, void * target, void * refcon);
    virtual IOReturn getReport(IOHIDReportType type, uint32_t id, void * report, uint32_t * pLength, uint32_t timeout, IOHIDReportCallbackFunction callback, void * target, void * refcon);
    virtual IOReturn setInterruptReportHandlerCallback(void * report, uint32_t length, IOHIDReportCallbackFunction callback, void * target, void * refcon);                           
    
    virtual IOHIDQueueInterface ** allocQueue();
    virtual IOHIDOutputTransactionInterface ** allocOutputTransaction();
};


#endif /* !_IOKIT_IOHIDDeviceClass_H */
