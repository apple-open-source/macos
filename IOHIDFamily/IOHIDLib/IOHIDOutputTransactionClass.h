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
#ifndef _IOKIT_IOHIDOutputTransactionClass_H
#define _IOKIT_IOHIDOutputTransactionClass_H

#include "IOHIDDeviceClass.h"

class IOHIDOutputTransactionClass : public IOHIDIUnknown
{
private:
    // friends with our parent device class
    friend class IOHIDDeviceClass;
    
    // Disable copy constructors
    IOHIDOutputTransactionClass(IOHIDOutputTransactionClass &src);
    void operator =(IOHIDOutputTransactionClass &src);

protected:
    IOHIDOutputTransactionClass();
    virtual ~IOHIDOutputTransactionClass();

    static IOHIDOutputTransactionInterface	sHIDOutputTransactionInterfaceV1;

    struct InterfaceMap fHIDOutputTransaction;
    mach_port_t fAsyncPort;
    CFRunLoopSourceRef fCFSource;
    
    // if created, how we were created
    bool fIsCreated;
        
    // owming device
    IOHIDDeviceClass *	fOwningDevice;
        
    // Related transaction call back info
    IOHIDCallbackFunction	fEventCallback;
    void *			fEventTarget;
    void *			fEventRefcon;
    
    // The transaction linked list
    CFMutableDictionaryRef	fElementDictionaryRef;
    
    // CFMachPortCallBack routine
    static void transactionEventSourceCallback(CFMachPortRef *cfPort, mach_msg_header_t *msg, CFIndex size, void *info);
        
public:
    // set owner
    void setOwningDevice (IOHIDDeviceClass * owningDevice) { fOwningDevice = owningDevice; };
    
    // get interface map (for queryInterface)
    void * getInterfaceMap (void) { return &fHIDOutputTransaction; };

    // IOCFPlugin stuff
    virtual HRESULT queryInterface(REFIID iid, void **ppv);

    virtual IOReturn createAsyncEventSource(CFRunLoopSourceRef *source);
    virtual CFRunLoopSourceRef getAsyncEventSource();

    virtual IOReturn createAsyncPort(mach_port_t *port);
    virtual mach_port_t getAsyncPort();

    /* Basic IOHIDOutputTransaction interface */
    /* depth is the maximum number of elements in the queue before	*/
    /*   the oldest elements in the queue begin to be lost		*/
    virtual IOReturn create ();
    virtual IOReturn dispose ();
    
    /* Any number of hid elements can feed the same transaction */
    virtual IOReturn addElement (IOHIDElementCookie elementCookie);
    virtual IOReturn removeElement (IOHIDElementCookie elementCookie);
    virtual Boolean hasElement (IOHIDElementCookie elementCookie);


    virtual IOReturn setElementDefault( IOHIDElementCookie	elementCookie,
                                        IOHIDEventStruct *	valueEvent); 
                                  
    /* get the default value for that element */
    virtual IOReturn getElementDefault( IOHIDElementCookie	elementCookie,
                                        IOHIDEventStruct *	valueEvent);
                                  
    /* set the value for that element */
    virtual IOReturn setElementValue(   IOHIDElementCookie	elementCookie,
                                        IOHIDEventStruct *	valueEvent);
                                  
    /* get the value for that element */
    virtual IOReturn getElementValue(IOHIDElementCookie	elementCookie,
                                        IOHIDEventStruct *	valueEvent);
       
    /* commit the changes to the device */
    virtual IOReturn commit(UInt32 		timeoutMS,
                            IOHIDCallbackFunction callback,
                            void * 		callbackTarget,
                            void *		callbackRefcon);
    
    /* Clear all the changes and start over */
    virtual IOReturn clear();

    
/*
 * Routing gumf for CFPlugIn interfaces
 */
protected:

    static inline IOHIDOutputTransactionClass *getThis(void *self)
        { return (IOHIDOutputTransactionClass *) ((InterfaceMap *) self)->obj; };

    // Methods for routing the iocfplugin Interface v1r1

    // Methods for routing asynchronous completion plumbing.
    static IOReturn outputTransactionCreateAsyncEventSource(void *self,
                                                 CFRunLoopSourceRef *source);
    static CFRunLoopSourceRef outputTransactionGetAsyncEventSource(void *self);
    static IOReturn outputTransactionCreateAsyncPort(void *self, mach_port_t *port);
    static mach_port_t outputTransactionGetAsyncPort(void *self);

    /* Basic IOHIDQueue interface */
    static IOReturn outputTransactionCreate (void * self);
    static IOReturn outputTransactionDispose (void * self);
    
    /* Any number of hid elements can feed the same outputTransaction */
    static IOReturn outputTransactionAddElement (   void * 		self,
                                                    IOHIDElementCookie 	elementCookie);
                                
    static IOReturn outputTransactionRemoveElement ( void * 		self, 
                                                    IOHIDElementCookie 	elementCookie);
                                                    
    static Boolean outputTransactionHasElement (    void * 		self, 
                                                    IOHIDElementCookie 	elementCookie);
    
    /* set the default value for that element */
    static IOReturn outputTransactionSetElementDefault(void * 		self,
                                                    IOHIDElementCookie	elementCookie,
                                                    IOHIDEventStruct *	valueEvent); 
                                  
    /* get the default value for that element */
    static IOReturn outputTransactionGetElementDefault(void * 		self,
                                                    IOHIDElementCookie	elementCookie,
                                                    IOHIDEventStruct *	valueEvent);
                                  
    /* set the value for that element */
    static IOReturn outputTransactionSetElementValue(void * 		self,
                                                    IOHIDElementCookie	elementCookie,
                                                    IOHIDEventStruct *	valueEvent);
                                  
    /* get the value for that element */
    static IOReturn outputTransactionGetElementValue(void * 		self,
                                                    IOHIDElementCookie	elementCookie,
                                                    IOHIDEventStruct *	valueEvent);
       
    /* commit the changes to the device */
    static IOReturn outputTransactionCommit(	    void * 		self,
                                                    UInt32 		timeoutMS,
                                                    IOHIDCallbackFunction callback,
                                                    void * 		callbackTarget,
                                                    void *		callbackRefcon);
    
    /* Clear all the changes and start over */
    static IOReturn outputTransactionClear(void * self);
                                     

/*
 * Internal functions
 */
    
};

#endif /* !_IOKIT_IOHIDOutputTransactionClass_H */
