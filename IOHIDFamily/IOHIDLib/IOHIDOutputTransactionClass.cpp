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

#define CFRUNLOOP_NEW_API 1

#include <CoreFoundation/CFMachPort.h>
//#include <IOKit/hid/IOHIDLib.h>
//#include <unistd.h>

#include "IOHIDOutputTransactionClass.h"
#include "IOHIDLibUserClient.h"

__BEGIN_DECLS
#include <mach/mach_interface.h>
#include <IOKit/iokitmig.h>
__END_DECLS

typedef struct IOHIDTransactionElement
{
    IOHIDElementCookie		cookie;
    
    UInt8			state;
    
    IOHIDEventStruct 		defaultValue;
    IOHIDEventStruct 		currentValue;
    
} IOHIDTransactionElement;

enum {
    kIOHIDTransactionDefault	= 0x01,
    kIOHIDTransactionCurrent	= 0x02
};

#define ownerCheck() do {		\
    if (!fOwningDevice)			\
	return kIOReturnNoDevice;	\
} while (0)

#define connectCheck() do {		\
    if ((!fOwningDevice) ||		\
    	(!fOwningDevice->fConnection))	\
	return kIOReturnNoDevice;	\
} while (0)

#define openCheck() do {	    \
    if (!fIsCreated)		    \
        return kIOReturnNotOpen;    \
} while (0)

#define allChecks() do {	    \
    connectCheck();		    \
    openCheck();		    \
} while (0)

IOHIDOutputTransactionClass::IOHIDOutputTransactionClass()
: IOHIDIUnknown(NULL),
  fAsyncPort(MACH_PORT_NULL),
  fIsCreated(false),
  fEventCallback(NULL),
  fEventTarget(NULL),
  fEventRefcon(NULL),
  fElementDictionaryRef(NULL)
{
    fHIDOutputTransaction.pseudoVTable = (IUnknownVTbl *)  &sHIDOutputTransactionInterfaceV1;
    fHIDOutputTransaction.obj = this;
}

IOHIDOutputTransactionClass::~IOHIDOutputTransactionClass()
{
}

HRESULT IOHIDOutputTransactionClass::queryInterface(REFIID /*iid*/, void **	/*ppv*/)
{
    // еее should we return our parent if that type is asked for???
    
    return E_NOINTERFACE;
}

IOReturn IOHIDOutputTransactionClass::
createAsyncEventSource(CFRunLoopSourceRef *source)
{
    IOReturn ret;
    CFMachPortRef cfPort;
    CFMachPortContext context;
    Boolean shouldFreeInfo;

    if (!fAsyncPort) {     
        ret = createAsyncPort(0);
        if (kIOReturnSuccess != ret)
            return ret;
    }

    context.version = 1;
    context.info = this;
    context.retain = NULL;
    context.release = NULL;
    context.copyDescription = NULL;

    cfPort = CFMachPortCreateWithPort(NULL, fAsyncPort,
                (CFMachPortCallBack) IOHIDOutputTransactionClass::transactionEventSourceCallback,
                &context, &shouldFreeInfo);
    if (!cfPort)
        return kIOReturnNoMemory;
    
    fCFSource = CFMachPortCreateRunLoopSource(NULL, cfPort, 0);
    CFRelease(cfPort);
    if (!fCFSource)
        return kIOReturnNoMemory;

    if (source)
        *source = fCFSource;

    return kIOReturnSuccess;
}

CFRunLoopSourceRef IOHIDOutputTransactionClass::getAsyncEventSource()
{
    return fCFSource;
}

/* CFMachPortCallBack */
void IOHIDOutputTransactionClass::transactionEventSourceCallback(CFMachPortRef *cfPort, mach_msg_header_t *msg, CFIndex size, void *info){
    
    IOHIDOutputTransactionClass *transaction = (IOHIDOutputTransactionClass *)info;
    
    if ( transaction ) {
        if ( transaction->fEventCallback ) {
                
            (transaction->fEventCallback)(transaction->fEventTarget, 
                            kIOReturnSuccess, 
                            transaction->fEventRefcon, 
                            (void *)&transaction->fHIDOutputTransaction);
        }
    }
}

IOReturn IOHIDOutputTransactionClass::createAsyncPort(mach_port_t *port)
{
    IOReturn	ret = kIOReturnSuccess;
    
    if (!fAsyncPort)
        ret = fOwningDevice->createAsyncPort(&fAsyncPort);
    
    if (port && (ret == kIOReturnSuccess))
        *port = fAsyncPort;
        
    return ret;
    
}

mach_port_t IOHIDOutputTransactionClass::getAsyncPort()
{
    return fAsyncPort;
}

IOReturn IOHIDOutputTransactionClass::create ()
{
    IOReturn ret = kIOReturnSuccess;

    if (fIsCreated)
        return kIOReturnSuccess;
    
    // Create the mutable dictionary that will hold
    // the transaction elements.
    fElementDictionaryRef = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, 
                            &kCFTypeDictionaryKeyCallBacks, 
                            &kCFTypeDictionaryValueCallBacks);
                            
    if (!fElementDictionaryRef)
        return kIOReturnNoMemory;

    // we have created it
    fIsCreated = true;
    
    // if we have async port, set it on other side
    if (fAsyncPort)
    {
        natural_t 			asyncRef[1];
        int				input[1];
        mach_msg_type_number_t 	len = 0;
    
        // async kIOHIDLibUserClientSetQueueAsyncPort, kIOUCScalarIScalarO, 1, 0
        //ret = io_async_method_scalarI_scalarO(
        //        fOwningDevice->fConnection, fAsyncPort, asyncRef, 1,
        //        kIOHIDLibUserClientSetQueueAsyncPort, input, 1, NULL, &len);
        if (ret != kIOReturnSuccess) {
            (void) this->dispose();
        }
    }
        
    return ret;
}

IOReturn IOHIDOutputTransactionClass::dispose()
{
    CFIndex			numElements;
    IOHIDTransactionElement 	*element;
    IOReturn			ret = kIOReturnSuccess;

    // mark it dead
    fIsCreated = false;
    
    //if (!fElementDictionaryRef)
    //    return kIOReturnSuccess;
     
    numElements = CFDictionaryGetCount(fElementDictionaryRef);
    
    CFDataRef	elementDataRefs[numElements];
    
    if (!numElements) {
        ret = kIOReturnError;
        goto DISPOSE_RELEASE;
    }
        
    CFDictionaryGetKeysAndValues(fElementDictionaryRef, NULL, elementDataRefs);
    
    if (!elementDataRefs) {
        ret = kIOReturnError;
        goto DISPOSE_RELEASE;
    }
    
    for (int i=0; elementDataRefs[i] && i<numElements; i++)
    {
        element = (IOHIDTransactionElement *)CFDataGetBytePtr(elementDataRefs[i]);
        
        if (!element)
            continue;
            
        if ((element->state & kIOHIDTransactionCurrent) != 0)
        {
            // If a long value is present we should free it now.
            if (element->currentValue.longValueSize > 0)
                free(element->currentValue.longValue);
        }
        
        if ((element->state & kIOHIDTransactionDefault) != 0)
        {
            // If a long value is present we should free it now.
            if (element->defaultValue.longValueSize > 0)
                free(element->defaultValue.longValue);
        }

    }

DISPOSE_RELEASE:    
    // Destroy the transaction dictionary
    CFRelease(fElementDictionaryRef);
    
    return ret;

}

/* Any number of hid elements can feed the same queue */
IOReturn IOHIDOutputTransactionClass::addElement (
                            IOHIDElementCookie elementCookie)
{
    IOHIDTransactionElement 	*element;
    IOHIDElementStruct		tempElementStruct;
    IOReturn			ret = kIOReturnSuccess;
    CFDataRef			elementDataRef;
    CFNumberRef			elementKeyRef;
    int 			cookieValue = (int)elementCookie;
    
    if (!fIsCreated)
        return kIOReturnError;
        
    if (hasElement(elementCookie))
        return kIOReturnError;
        
    if (!fOwningDevice->getElement(elementCookie, &tempElementStruct))
        return kIOReturnBadArgument;
         
    elementDataRef = CFDataCreateMutable(kCFAllocatorDefault, sizeof(IOHIDTransactionElement));
    elementKeyRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &cookieValue);
    
    if (!fElementDictionaryRef || !elementDataRef || !elementKeyRef) {
        ret = kIOReturnError;
        goto ADD_ELEMENT_RELEASE;
    }
    
    // Initialize the transaction element
    element = (IOHIDTransactionElement *)CFDataGetMutableBytePtr(elementDataRef);
    bzero (element, sizeof(IOHIDTransactionElement));
    element->cookie = elementCookie;
    element->defaultValue.elementCookie = elementCookie;
    element->defaultValue.type = tempElementStruct.type;
    element->currentValue.elementCookie = elementCookie;
    element->currentValue.type = tempElementStruct.type;


    CFDictionarySetValue(fElementDictionaryRef, elementKeyRef, elementDataRef);

ADD_ELEMENT_RELEASE:    
    if (elementKeyRef) CFRelease(elementKeyRef);
    if (elementDataRef) CFRelease(elementDataRef);
    
    return kIOReturnSuccess;
}

IOReturn IOHIDOutputTransactionClass::removeElement (IOHIDElementCookie elementCookie)
{
    CFNumberRef			elementKeyRef;
    int 			cookieValue = (int)elementCookie;
    IOReturn			ret = kIOReturnSuccess;
    
    if (!fIsCreated ||!hasElement(elementCookie))
        return kIOReturnError;
         
    elementKeyRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &cookieValue);
    
    if (!fElementDictionaryRef || !elementKeyRef) {
        ret = kIOReturnError;
        goto REMOVE_ELEMENT_RELEASE;
    }
        
    CFDictionaryRemoveValue(fElementDictionaryRef, elementKeyRef);

REMOVE_ELEMENT_RELEASE:
    if (elementKeyRef) CFRelease(elementKeyRef);
    
    return kIOReturnSuccess;

}

Boolean IOHIDOutputTransactionClass::hasElement (IOHIDElementCookie elementCookie)
{
    CFNumberRef			elementKeyRef;
    int 			cookieValue = (int)elementCookie;
    Boolean			ret = false;
    
    if (!fIsCreated)
        return false;
    
    elementKeyRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &cookieValue);
    
    if (!fElementDictionaryRef || !elementKeyRef)
        goto HAS_ELEMENT_RELEASE;
    
    ret = CFDictionaryContainsKey(fElementDictionaryRef, elementKeyRef);

HAS_ELEMENT_RELEASE:    
    if (elementKeyRef) CFRelease(elementKeyRef);

    return ret;
}

IOReturn IOHIDOutputTransactionClass::getElementDefault(IOHIDElementCookie	elementCookie,
                                                        IOHIDEventStruct *	valueEvent)
{    
    CFNumberRef			elementKeyRef;
    CFDataRef			elementDataRef;
    IOHIDTransactionElement 	*element;
    int 			cookieValue = (int)elementCookie;
             
    if (!fIsCreated)
        return kIOReturnError;
        
    elementKeyRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &cookieValue);
    
    if (!fElementDictionaryRef || !elementKeyRef) {
        if (elementKeyRef) CFRelease(elementKeyRef);
        return kIOReturnError;
    }
        
    elementDataRef = CFDictionaryGetValue(fElementDictionaryRef, elementKeyRef);
    
    CFRelease(elementKeyRef);
    
    if (!elementDataRef)
        return kIOReturnError;
        
    element = (IOHIDTransactionElement *)CFDataGetBytePtr(elementDataRef);
    
    if (!element || ((element->state & kIOHIDTransactionDefault) == 0))
        return kIOReturnError;
        
    // Fill in the value event
    valueEvent->type 		= element->defaultValue.type;
    valueEvent->elementCookie	= element->defaultValue.elementCookie;
    valueEvent->value		= element->defaultValue.value;
    valueEvent->timestamp	= element->defaultValue.timestamp;

    if (element->defaultValue.longValueSize > 0) 
    {
        valueEvent->longValueSize = element->defaultValue.longValueSize;
        
        valueEvent->longValue = malloc(valueEvent->longValueSize);
        bcopy(element->defaultValue.longValue, valueEvent->longValue, valueEvent->longValueSize);
    } 
    else
    {
        valueEvent->longValueSize = 0;
        valueEvent->longValue = NULL;
    }
    
    return kIOReturnSuccess;

}

IOReturn IOHIDOutputTransactionClass::setElementDefault(IOHIDElementCookie	elementCookie,
                                                      IOHIDEventStruct *	valueEvent)
{
    CFNumberRef			elementKeyRef;
    CFDataRef			elementDataRef;
    IOHIDTransactionElement 	*element;
    int 			cookieValue = (int)elementCookie;
             
    if (!fIsCreated)
        return kIOReturnError;

    elementKeyRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &cookieValue);
    
    if (!fElementDictionaryRef || !elementKeyRef) {
        if (elementKeyRef) CFRelease(elementKeyRef);
        return kIOReturnError;
    }
        
    elementDataRef = CFDictionaryGetValue(fElementDictionaryRef, elementKeyRef);
    
    CFRelease(elementKeyRef);
    
    if (!elementDataRef)
        return kIOReturnError;
        
    element = (IOHIDTransactionElement *)CFDataGetMutableBytePtr(elementDataRef);
    
    if (!element)
        return kIOReturnError;

    // If a long value has been set, free it
    if (element->defaultValue.longValueSize > 0)
        free(element->defaultValue.longValue);
    
    // Set the state
    element->state |= kIOHIDTransactionDefault;
    
    // Fill in the value event
    element->defaultValue.value 	= valueEvent->value;

    // RY: Deal with long values.  I've wrestled with this, and the best
    // course of action is to copy the value.
    if (valueEvent->longValueSize > 0) 
    {
        element->defaultValue.longValueSize = valueEvent->longValueSize;
        
        element->defaultValue.longValue	= malloc(valueEvent->longValueSize);
        bcopy(valueEvent->longValue, element->defaultValue.longValue, valueEvent->longValueSize);
    } 
    else
    {
        element->defaultValue.longValueSize = 0;
        element->defaultValue.longValue = NULL;
    }
        
    
    return kIOReturnSuccess;
}

/* set the value for that element */
IOReturn IOHIDOutputTransactionClass::setElementValue(IOHIDElementCookie	elementCookie,
                                                      IOHIDEventStruct *	valueEvent)
{
    CFNumberRef			elementKeyRef;
    CFDataRef			elementDataRef;
    IOHIDTransactionElement 	*element;
    int 			cookieValue = (int)elementCookie;
    
    if (!fIsCreated)
        return kIOReturnError;
             
    elementKeyRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &cookieValue);
    
    if (!fElementDictionaryRef || !elementKeyRef) {
        if (elementKeyRef) CFRelease(elementKeyRef);
        return kIOReturnError;
    }
        
    elementDataRef = CFDictionaryGetValue(fElementDictionaryRef, elementKeyRef);
    
    CFRelease(elementKeyRef);
    
    if (!elementDataRef)
        return kIOReturnError;
        
    element = (IOHIDTransactionElement *)CFDataGetMutableBytePtr(elementDataRef);
    
    if (!element)
        return kIOReturnError;
        
    // If a long value has been set, free it
    if (element->currentValue.longValueSize > 0)
        free(element->currentValue.longValue);

    
    // Set the state;
    element->state |= kIOHIDTransactionCurrent;
    
    // Fill in the value event
    element->currentValue.value 	= valueEvent->value;

    // RY: Deal with long values.  I've wrestled with this, and the best
    // course of action is to copy the value.
    if (valueEvent->longValueSize > 0) 
    {
        element->currentValue.longValueSize = valueEvent->longValueSize;
        
        element->currentValue.longValue	= malloc(valueEvent->longValueSize);
        bcopy(valueEvent->longValue, element->currentValue.longValue, valueEvent->longValueSize);
    } 
    else
    {
        element->currentValue.longValueSize = 0;
        element->currentValue.longValue = NULL;
    }
    
    return kIOReturnSuccess;
}
                                
/* get the value for that element */
IOReturn IOHIDOutputTransactionClass::getElementValue(IOHIDElementCookie	elementCookie,
                                                        IOHIDEventStruct *	valueEvent)
{
    CFNumberRef			elementKeyRef;
    CFDataRef			elementDataRef;
    IOHIDTransactionElement 	*element;
    int 			cookieValue = (int)elementCookie;

    if (!fIsCreated)
        return kIOReturnError;
             
    elementKeyRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &cookieValue);
    
    if (!fElementDictionaryRef || !elementKeyRef) {
        if (elementKeyRef) CFRelease(elementKeyRef);
        return kIOReturnError;
    }
        
    elementDataRef = CFDictionaryGetValue(fElementDictionaryRef, elementKeyRef);
    
    CFRelease(elementKeyRef);
    
    if (!elementDataRef)
        return kIOReturnError;
        
    element = (IOHIDTransactionElement *)CFDataGetBytePtr(elementDataRef);
    
    if (!element || ((element->state & kIOHIDTransactionCurrent) == 0))
        return kIOReturnError;
        
    // Fill in the value event
    valueEvent->type 		= element->currentValue.type;
    valueEvent->elementCookie	= element->currentValue.elementCookie;
    valueEvent->value		= element->currentValue.value;
    valueEvent->timestamp	= element->currentValue.timestamp;

    if (element->currentValue.longValueSize > 0) 
    {
        valueEvent->longValueSize = element->currentValue.longValueSize;
        
        valueEvent->longValue = malloc(valueEvent->longValueSize);
        bcopy(element->currentValue.longValue, valueEvent->longValue, valueEvent->longValueSize);
    } 
    else
    {
        valueEvent->longValueSize = 0;
        valueEvent->longValue = NULL;
    }

    return kIOReturnSuccess;

}

/* start/stop data delivery to a queue */
IOReturn IOHIDOutputTransactionClass::commit(UInt32 			timeoutMS,
                                             IOHIDCallbackFunction 	callback,
                                             void * 			callbackTarget,
                                             void *			callbackRefcon)
{
    CFIndex			numElements;
    IOHIDTransactionElement 	*element;
    IOReturn			ret = kIOReturnError;

    
    if (!fIsCreated || !fElementDictionaryRef)
        return kIOReturnError;
     
    numElements = CFDictionaryGetCount(fElementDictionaryRef);
    
    if (!numElements)
        return kIOReturnError;
    
    CFDataRef		elementDataRefs[numElements];

    CFDictionaryGetKeysAndValues(fElementDictionaryRef, NULL, elementDataRefs);
    
    if (!elementDataRefs)
        return kIOReturnError;
    
    // run through and call setElementValue w/o device push
    // *** we definitely have to hold a lock here. ***
    int				numValidElements = 0;
    UInt32			transactionCookies[numElements];
    
    for (int i=0; elementDataRefs[i] && i<numElements; i++)
    {
        element = (IOHIDTransactionElement *)CFDataGetBytePtr(elementDataRefs[i]);
        
        if (!element)
            continue;
            
        if ((element->state & kIOHIDTransactionCurrent) != 0)
        {
            fOwningDevice->setElementValue(element->cookie, &(element->currentValue));
            
            // If a long value is present, we should free it now.
            if (element->currentValue.longValueSize > 0) 
            {
                free(element->currentValue.longValue);
                    
                element->currentValue.longValue = 0;
                element->currentValue.longValueSize = 0;
            }
                
            element->currentValue.value = 0;

            element->state &= ~kIOHIDTransactionCurrent;
        }
        else if ((element->state & kIOHIDTransactionDefault) != 0)
        {
            fOwningDevice->setElementValue(element->cookie, &(element->defaultValue));
        }
        else 
            continue;
            
        transactionCookies[numValidElements] = (UInt32)element->cookie;
        numValidElements++;
    }
    
    // put together an ioconnect here
    //  kIOHIDLibUserClientPostElementValue,  kIOUCStructIStructO, 1, 0
    IOByteCount			outputCount = 0;
    
    allChecks();

    ret = io_connect_method_structureI_structureO( fOwningDevice->fConnection, 
            kIOHIDLibUserClientPostElementValue, (UInt8 *)transactionCookies, 
            sizeof(UInt32) * numValidElements, NULL, &outputCount);
            
    return ret;
            
}

IOReturn IOHIDOutputTransactionClass::clear ()
{
    CFIndex			numElements;
    IOHIDTransactionElement 	*element;

    
    if (!fIsCreated || !fElementDictionaryRef)
        return kIOReturnError;
     
    numElements = CFDictionaryGetCount(fElementDictionaryRef);
    
    if (!numElements)
        return kIOReturnError;
        
    CFDataRef	elementDataRefs[numElements];
    
    CFDictionaryGetKeysAndValues(fElementDictionaryRef, NULL, elementDataRefs);
    
    if (!elementDataRefs)
        return kIOReturnError;
    
    for (int i=0; elementDataRefs[i] && i<numElements; i++)
    {
        element = (IOHIDTransactionElement *)CFDataGetBytePtr(elementDataRefs[i]);
        
        if (!element)
            continue;
            
        if ((element->state & kIOHIDTransactionCurrent) != 0)
        {
            // If a long value is present, we should free it now.
            if (element->currentValue.longValueSize > 0) 
            {
                free(element->currentValue.longValue);
                    
                element->currentValue.longValue = 0;
                element->currentValue.longValueSize = 0;
            }
                
            element->currentValue.value = 0;


            element->state &= ~kIOHIDTransactionCurrent;
        }
    }
    
    return kIOReturnSuccess;
}



IOHIDOutputTransactionInterface IOHIDOutputTransactionClass::sHIDOutputTransactionInterfaceV1 =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    &IOHIDOutputTransactionClass::outputTransactionCreateAsyncEventSource,
    &IOHIDOutputTransactionClass::outputTransactionGetAsyncEventSource,
    &IOHIDOutputTransactionClass::outputTransactionCreateAsyncPort,
    &IOHIDOutputTransactionClass::outputTransactionGetAsyncPort,
    &IOHIDOutputTransactionClass::outputTransactionCreate,
    &IOHIDOutputTransactionClass::outputTransactionDispose,
    &IOHIDOutputTransactionClass::outputTransactionAddElement,
    &IOHIDOutputTransactionClass::outputTransactionRemoveElement,
    &IOHIDOutputTransactionClass::outputTransactionHasElement,
    &IOHIDOutputTransactionClass::outputTransactionSetElementDefault,
    &IOHIDOutputTransactionClass::outputTransactionGetElementDefault,
    &IOHIDOutputTransactionClass::outputTransactionSetElementValue,
    &IOHIDOutputTransactionClass::outputTransactionGetElementValue,
    &IOHIDOutputTransactionClass::outputTransactionCommit,
    &IOHIDOutputTransactionClass::outputTransactionClear,
};

// Methods for routing asynchronous completion plumbing.
IOReturn IOHIDOutputTransactionClass::
outputTransactionCreateAsyncEventSource(void *self, CFRunLoopSourceRef *source)
    { return getThis(self)->createAsyncEventSource(source); }

CFRunLoopSourceRef IOHIDOutputTransactionClass::
outputTransactionGetAsyncEventSource(void *self)
    { return getThis(self)->getAsyncEventSource(); }

IOReturn IOHIDOutputTransactionClass::
outputTransactionCreateAsyncPort(void *self, mach_port_t *port)
    { return getThis(self)->createAsyncPort(port); }

mach_port_t IOHIDOutputTransactionClass::
outputTransactionGetAsyncPort(void *self)
    { return getThis(self)->getAsyncPort(); }

/* Basic IOHIDQueue interface */
IOReturn IOHIDOutputTransactionClass::
outputTransactionCreate(void * 			self)
    { return getThis(self)->create(); }

IOReturn IOHIDOutputTransactionClass::outputTransactionDispose (void * self)
    { return getThis(self)->dispose(); }

/* Any number of hid elements can feed the same queue */
IOReturn IOHIDOutputTransactionClass::outputTransactionAddElement (void * self,
                            IOHIDElementCookie elementCookie)
    { return getThis(self)->addElement(elementCookie); }

IOReturn IOHIDOutputTransactionClass::outputTransactionRemoveElement (void * self, IOHIDElementCookie elementCookie)
    { return getThis(self)->removeElement(elementCookie); }

Boolean IOHIDOutputTransactionClass::outputTransactionHasElement (void * self, IOHIDElementCookie elementCookie)
    { return getThis(self)->hasElement(elementCookie); }

IOReturn IOHIDOutputTransactionClass::outputTransactionSetElementDefault(void * 		self,
                                                    IOHIDElementCookie	elementCookie,
                                                    IOHIDEventStruct *	valueEvent)
    { return getThis(self)->setElementDefault(elementCookie, valueEvent); }
    
IOReturn IOHIDOutputTransactionClass::outputTransactionGetElementDefault(void * 		self,
                                                    IOHIDElementCookie	elementCookie,
                                                    IOHIDEventStruct *	valueEvent)
    { return getThis(self)->getElementDefault(elementCookie, valueEvent); }


IOReturn IOHIDOutputTransactionClass::outputTransactionSetElementValue(void * 		self,
                                                    IOHIDElementCookie	elementCookie,
                                                    IOHIDEventStruct *	valueEvent)
    { return getThis(self)->setElementValue(elementCookie, valueEvent); }
    
IOReturn IOHIDOutputTransactionClass::outputTransactionGetElementValue(void * 		self,
                                                    IOHIDElementCookie	elementCookie,
                                                    IOHIDEventStruct *	valueEvent)
    { return getThis(self)->getElementValue(elementCookie, valueEvent); }


IOReturn IOHIDOutputTransactionClass::outputTransactionCommit(void * 		self,
                                                    UInt32 		timeoutMS,
                                                    IOHIDCallbackFunction callback,
                                                    void * 		callbackTarget,
                                                    void *		callbackRefcon)
    { return getThis(self)->commit(timeoutMS, callback, callbackTarget, callbackRefcon);}
    
    /* Clear all the changes and start over */
IOReturn IOHIDOutputTransactionClass::outputTransactionClear(void * self)
    { return getThis(self)->clear();}
