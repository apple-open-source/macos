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

#define CFRUNLOOP_NEW_API 1

#include <CoreFoundation/CFMachPort.h>
//#include <IOKit/hid/IOHIDLib.h>
//#include <unistd.h>

#include "IOHIDTransactionClass.h"
#include "IOHIDLibUserClient.h"
#include "IOHIDTransactionElement.h"

__BEGIN_DECLS
#include <mach/mach_interface.h>
#include <mach/mach.h>
#include <IOKit/iokitmig.h>
#include <System/libkern/OSCrossEndian.h>
__END_DECLS

#define ownerCheck() do {		\
    if (!fOwningDevice)			\
	return kIOReturnNoDevice;	\
} while (0)

#define connectCheck() do {		\
    if ((!fOwningDevice) ||		\
    	(!fOwningDevice->fConnection))	\
	return kIOReturnNoDevice;	\
} while (0)

#define createdCheck() do {     \
    if (!fIsCreated)            \
    return kIOReturnError;      \
} while (0)

#define openCheck() do {            \
    if (!fOwningDevice ||           \
        !fOwningDevice->fIsOpen)    \
        return kIOReturnNotOpen;    \
} while (0)

#define seizeCheck() do {               \
    if ((!fOwningDevice) ||             \
         (!fOwningDevice->isValid()))    \
        return kIOReturnExclusiveAccess;\
} while (0)


#define mostChecks() do {   \
    connectCheck();		    \
    createdCheck();         \
} while (0)

#define allChecks() do {    \
    mostChecks();           \
    openCheck();		    \
    seizeCheck();           \
} while (0)

IOHIDTransactionClass::IOHIDTransactionClass() : IOHIDIUnknown(NULL)
{
    fHIDTransaction.pseudoVTable    = (IUnknownVTbl *)  &sHIDTransactionInterface;
    fHIDTransaction.obj             = this;

    fDirection                      = 0;
    fIsCreated                      = false;
    fOwningDevice                   = NULL;
    fEventCallback                  = NULL;
    fEventRefcon                    = NULL;
    fElementDictionaryRef           = NULL;
}

IOHIDTransactionClass::~IOHIDTransactionClass()
{
    if (fIsCreated)
        dispose();
        
    if( fOwningDevice ) 
        fOwningDevice->detachTransaction(this);

}

HRESULT IOHIDTransactionClass::queryInterface(REFIID iid, void ** ppv)
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT res = S_OK;

    if (CFEqual(uuid, kIOHIDDeviceTransactionInterfaceID))
    {
        *ppv = getInterfaceMap();
        addRef();
    }
    else {
        res = fOwningDevice->queryInterface(iid, ppv);
    }

    if (!*ppv)
        res = E_NOINTERFACE;

    CFRelease(uuid);
    return res;
}

IOReturn IOHIDTransactionClass::getAsyncEventSource(CFTypeRef *source)
{
    connectCheck();
    
    return fOwningDevice->getAsyncEventSource(source);
}

void IOHIDTransactionClass::_eventSourceCallback(CFMachPortRef *cfPort __unused, mach_msg_header_t *msg __unused, CFIndex size __unused, void *info)
{
    
    IOHIDTransactionClass *transaction = (IOHIDTransactionClass *)info;
    
    if ( transaction && transaction->fEventCallback) 
    {
        (transaction->fEventCallback)(transaction->fEventRefcon, kIOReturnSuccess, (void *)&transaction->fHIDTransaction);
    }
}

IOReturn IOHIDTransactionClass::getAsyncPort(mach_port_t *port)
{
    connectCheck();
    
    return fOwningDevice->getAsyncPort(port);
}

IOReturn IOHIDTransactionClass::getDirection(IOHIDTransactionDirectionType * pDirection)
{
    if (!pDirection)
        return kIOReturnBadArgument;
        
    *pDirection = fDirection;
    
    return kIOReturnSuccess;
}

IOReturn IOHIDTransactionClass::setDirection(IOHIDTransactionDirectionType direction, IOOptionBits options __unused)
{
    IOHIDTransactionElementRef *elementRefs = NULL;
    IOHIDElementRef             element = NULL;
    CFStringRef                *keyRefs= NULL;
    CFIndex                     numElements	= 0;
    
    if (!fIsCreated || !fElementDictionaryRef) 
        return kIOReturnError;
     
    // RY: If we change directions, we should remove the opposite direction elements
    // from the transaction.  I might decide to leave them alone in the future and 
    // just ignore them during the commit.
    numElements = CFDictionaryGetCount(fElementDictionaryRef);
    
    if (!numElements) 
        return kIOReturnError;
        
    elementRefs = (IOHIDTransactionElementRef *)malloc(sizeof(IOHIDTransactionElementRef) * numElements);
    keyRefs     = (CFStringRef *)malloc(sizeof(CFStringRef) * numElements);
    CFDictionaryGetKeysAndValues(fElementDictionaryRef, (const void **)keyRefs, (const void **)elementRefs);
    
    for (int i=0;i<numElements && elementRefs[i] && keyRefs[i]; i++)
    {
        element = IOHIDTransactionElementGetElement(elementRefs[i]);
        if (((IOHIDElementGetType(element) == kIOHIDElementTypeOutput) && (direction == kIOHIDTransactionDirectionTypeInput)) ||
            ((IOHIDElementGetType(element) >= kIOHIDElementTypeInput_Misc) && (IOHIDElementGetType(element) <= kIOHIDElementTypeInput_ScanCodes) && (direction == kIOHIDTransactionDirectionTypeOutput)))
            CFDictionaryRemoveValue(fElementDictionaryRef, keyRefs[i]);
    }

    if (elementRefs) 
        free(elementRefs);
    
    if (keyRefs)
        free(keyRefs);
    
    fDirection = direction;
    
    return kIOReturnSuccess;
}

IOReturn IOHIDTransactionClass::create ()
{
    IOReturn ret = kIOReturnSuccess;

    connectCheck();

    if (fIsCreated)
        return kIOReturnSuccess;
    
    // Create the mutable dictionary that will hold the transaction elements.
    fElementDictionaryRef = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                            
    if (!fElementDictionaryRef)
        return kIOReturnNoMemory;

    // we have created it
    fIsCreated = true;
            
    return ret;
}

IOReturn IOHIDTransactionClass::dispose()
{
    IOReturn ret = kIOReturnSuccess;

    // mark it dead
    fIsCreated = false;
    
    if (fElementDictionaryRef) 
    {
        CFRelease(fElementDictionaryRef);
        fElementDictionaryRef = NULL;
    }
    
    return ret;
}

IOReturn IOHIDTransactionClass::addElement (IOHIDElementRef element, IOOptionBits options __unused)
{
    IOHIDTransactionElementRef  transactionElement;
    IOHIDElementType            elementType;
    IOReturn                    ret = kIOReturnSuccess;
    Boolean                     added = false;

    mostChecks();
    
    if (!element)
        return kIOReturnBadArgument;

    if (!fIsCreated || hasElement(element, &added) || added)
        return kIOReturnError;
        
    elementType = IOHIDElementGetType(element);
    
    // Since this is a Output Transaction only allow feature and output elements
    if ((elementType != kIOHIDElementTypeOutput) && (elementType != kIOHIDElementTypeFeature))
        return kIOReturnBadArgument;
         
    transactionElement = IOHIDTransactionElementCreate(kCFAllocatorDefault, element, 0);
        
    if (!fElementDictionaryRef || !transactionElement)
        ret = kIOReturnError;
    else 
        CFDictionarySetValue(fElementDictionaryRef, element, transactionElement);

    if (transactionElement) CFRelease(transactionElement);
    
    return kIOReturnSuccess;
}

IOReturn IOHIDTransactionClass::removeElement (IOHIDElementRef element, IOOptionBits options __unused)
{    
    Boolean added;

    mostChecks();
    
    if (!element)
        return kIOReturnBadArgument;
         
    if (!fIsCreated || !fElementDictionaryRef || hasElement(element, &added) || !added)
        return kIOReturnError;

    CFDictionaryRemoveValue(fElementDictionaryRef, element);
    
    return kIOReturnSuccess;
}

IOReturn IOHIDTransactionClass::hasElement (IOHIDElementRef element, Boolean *pValue, IOOptionBits options __unused)
{
    mostChecks();
    
    if (!element || !pValue)
        return kIOReturnBadArgument;
        
    if (!fIsCreated || !fElementDictionaryRef)
        return kIOReturnError;
        
    *pValue = CFDictionaryContainsKey(fElementDictionaryRef, element);
    
    return kIOReturnSuccess;
}

IOReturn IOHIDTransactionClass::setElementValue(IOHIDElementRef element, IOHIDValueRef event, IOOptionBits options)
{
    IOHIDTransactionElementRef transactionElement;

    mostChecks();
    
    if (!element || (fDirection == kIOHIDTransactionDirectionTypeInput)) 
        return kIOReturnBadArgument;
    
    if (!fIsCreated || !fElementDictionaryRef) 
        return kIOReturnError;
             
    transactionElement = (IOHIDTransactionElementRef)CFDictionaryGetValue(fElementDictionaryRef, element);
    
    if (!transactionElement)
        return kIOReturnError;
    
    if ( options & kIOHIDTransactionOptionDefaultOutputValue )
        IOHIDTransactionElementSetDefaultValue(transactionElement, event);
    else
        IOHIDTransactionElementSetValue(transactionElement, event);

    return kIOReturnSuccess;
}
                                
/* get the value for that element */
IOReturn IOHIDTransactionClass::getElementValue(IOHIDElementRef element, IOHIDValueRef * pEvent, IOOptionBits options)
{    
    IOHIDTransactionElementRef transactionElement;

    mostChecks();
    
    if (!element) 
        return kIOReturnBadArgument;
    
    if (!fIsCreated || !fElementDictionaryRef) 
        return kIOReturnError;
             
    transactionElement = (IOHIDTransactionElementRef)CFDictionaryGetValue(fElementDictionaryRef, element);
    
    if (!transactionElement) 
        return kIOReturnError;
    
    *pEvent = ((fDirection == kIOHIDTransactionDirectionTypeOutput) && (options & kIOHIDTransactionOptionDefaultOutputValue)) ? 
            IOHIDTransactionElementGetDefaultValue(transactionElement) : IOHIDTransactionElementGetValue(transactionElement);

    return kIOReturnSuccess;
}

/* start/stop data delivery to a queue */
IOReturn IOHIDTransactionClass::commit(uint32_t timeoutMS __unused, IOHIDCallback callback __unused, void * callbackRefcon __unused, IOOptionBits options __unused)
{
    IOHIDTransactionElementRef *    elementRefs         = NULL;
    uint64_t *                      cookies             = NULL;
    CFIndex                         numElements         = 0;
    IOReturn                        ret                 = kIOReturnError;
    int                             numValidElements = 0;
    IOHIDValueRef                   event;

    allChecks();
    
    if (!fIsCreated || !fElementDictionaryRef)
        return kIOReturnError;
     
    numElements = CFDictionaryGetCount(fElementDictionaryRef);
    
    if (!numElements) 
        return kIOReturnError;
    
    cookies     = (uint64_t *)malloc(sizeof(uint64_t) * numElements);
    elementRefs = (IOHIDTransactionElementRef *)malloc(sizeof(IOHIDTransactionElementRef) * numElements);
    
    CFDictionaryGetKeysAndValues(fElementDictionaryRef, NULL, (const void **)elementRefs);
        
    // run through and call setElementValue w/o device push
    // *** we definitely have to hold a lock here. ***
    for (int i=0;i<numElements && elementRefs[i]; i++)
    {        
        if ( fDirection == kIOHIDTransactionDirectionTypeOutput )
        {
            if ((event = IOHIDTransactionElementGetValue(elementRefs[i])))
            {
                fOwningDevice->setElementValue(IOHIDTransactionElementGetElement(elementRefs[i]), event, 0, NULL, NULL, kHIDSetElementValuePendEvent);                
            }
            else if ((event = IOHIDTransactionElementGetDefaultValue(elementRefs[i])))
            {
                fOwningDevice->setElementValue(IOHIDTransactionElementGetElement(elementRefs[i]), event, 0, NULL, NULL, kHIDSetElementValuePendEvent);
            }
            else 
                continue;
        }
        
        IOHIDTransactionElementSetValue(elementRefs[i], NULL);
            
        cookies[numValidElements] = (uint32_t)IOHIDElementGetCookie(IOHIDTransactionElementGetElement(elementRefs[i]));
        ROSETTA_ONLY(
            cookies[numValidElements] = OSSwapInt32(cookies[numValidElements]);
        );

        numValidElements++;
    }
    
    uint32_t outputCount = 0;
    
    if ( fDirection == kIOHIDTransactionDirectionTypeOutput )
    {
        ret = IOConnectCallScalarMethod(fOwningDevice->fConnection, kIOHIDLibUserClientPostElementValues, cookies, numValidElements, 0, &outputCount); 
    } 
    else 
    {
        // put together an ioconnect here
        ret = IOConnectCallScalarMethod(fOwningDevice->fConnection, kIOHIDLibUserClientUpdateElementValues, cookies, numValidElements, 0, &outputCount); 

        for (int i=0;i<numElements && elementRefs[i]; i++)
        {        
            fOwningDevice->getElementValue(IOHIDTransactionElementGetElement(elementRefs[i]), &event, 0, NULL, NULL, kHIDGetElementValuePreventPoll);
            IOHIDTransactionElementSetValue(elementRefs[i], event);
        }
    }
            
    if (elementRefs)  
        free(elementRefs);
    
    if ( cookies ) 
        free(cookies);
            
    return ret;
            
}

IOReturn IOHIDTransactionClass::clear (IOOptionBits options __unused)
{
    IOHIDTransactionElementRef *elementRefs = NULL;
    CFIndex                     numElements	= 0;

    mostChecks();
    
    if (!fIsCreated || !fElementDictionaryRef) 
        return kIOReturnError;
     
    numElements = CFDictionaryGetCount(fElementDictionaryRef);
    
    if (!numElements) 
        return kIOReturnError;
        
    elementRefs = (IOHIDTransactionElementRef *)malloc(sizeof(IOHIDTransactionElementRef) * numElements);
    
    CFDictionaryGetKeysAndValues(fElementDictionaryRef, NULL, (const void **)elementRefs);
    
    for (int i=0;i<numElements && elementRefs[i]; i++)
        IOHIDTransactionElementSetValue(elementRefs[i], NULL);

    if (elementRefs) 
        free(elementRefs);
    
    return kIOReturnSuccess;
}

IOHIDDeviceTransactionInterface IOHIDTransactionClass::sHIDTransactionInterface =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    &IOHIDTransactionClass::_getAsyncEventSource,
    &IOHIDTransactionClass::_setDirection,
    &IOHIDTransactionClass::_getDirection,
    &IOHIDTransactionClass::_addElement,
    &IOHIDTransactionClass::_removeElement,
    &IOHIDTransactionClass::_hasElement,
    &IOHIDTransactionClass::_setElementValue,
    &IOHIDTransactionClass::_getElementValue,
    &IOHIDTransactionClass::_commit,
    &IOHIDTransactionClass::_clear,
};

IOReturn IOHIDTransactionClass::_getAsyncEventSource(void *self, CFTypeRef *source)
    { return getThis(self)->getAsyncEventSource(source); }

IOReturn IOHIDTransactionClass::_getAsyncPort(void *self, mach_port_t *port)
    { return getThis(self)->getAsyncPort(port); }

IOReturn IOHIDTransactionClass::_setDirection(void *self, IOHIDTransactionDirectionType direction, IOOptionBits options)
    { return getThis(self)->setDirection(direction, options); }

IOReturn IOHIDTransactionClass::_getDirection(void *self, IOHIDTransactionDirectionType * pDirection)
    { return getThis(self)->getDirection(pDirection); }

IOReturn IOHIDTransactionClass::_addElement (void * self, IOHIDElementRef element, IOOptionBits options)
    { return getThis(self)->addElement(element, options); }

IOReturn IOHIDTransactionClass::_removeElement (void * self, IOHIDElementRef element, IOOptionBits options)
    { return getThis(self)->removeElement(element, options); }

IOReturn IOHIDTransactionClass::_hasElement (void * self, IOHIDElementRef element, Boolean *pValue, IOOptionBits options)
    { return getThis(self)->hasElement(element, pValue, options); }

IOReturn IOHIDTransactionClass::_setElementValue(void * self, IOHIDElementRef element, IOHIDValueRef event, IOOptionBits options)
    { return getThis(self)->setElementValue(element, event, options); }
    
IOReturn IOHIDTransactionClass::_getElementValue(void * self, IOHIDElementRef element, IOHIDValueRef * pEvent, IOOptionBits options)
    { return getThis(self)->getElementValue(element, pEvent, options); }

IOReturn IOHIDTransactionClass::_commit(void * self, uint32_t timeoutMS, IOHIDCallback callback, void * callbackRefcon, IOOptionBits options)
    { return getThis(self)->commit(timeoutMS, callback, callbackRefcon, options);}
    
IOReturn IOHIDTransactionClass::_clear(void * self, IOOptionBits options)
    { return getThis(self)->clear(options);}
    

//****************************************************************************************************
// Class:       IOHIDOutputTransactionClass
// Subclasses:  IOHIDTransactionClass
//****************************************************************************************************

IOHIDOutputTransactionInterface IOHIDOutputTransactionClass::sHIDOutputTransactionInterface =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    &IOHIDOutputTransactionClass::_createAsyncEventSource,
    &IOHIDOutputTransactionClass::_getAsyncEventSource,
    &IOHIDTransactionClass::_getAsyncPort,
    &IOHIDOutputTransactionClass::_getAsyncPort,
    &IOHIDOutputTransactionClass::_create,
    &IOHIDOutputTransactionClass::_dispose,
    &IOHIDOutputTransactionClass::_addElement,
    &IOHIDOutputTransactionClass::_removeElement,
    &IOHIDOutputTransactionClass::_hasElement,
    &IOHIDOutputTransactionClass::_setElementDefault,
    &IOHIDOutputTransactionClass::_getElementDefault,
    &IOHIDOutputTransactionClass::_setElementValue,
    &IOHIDOutputTransactionClass::_getElementValue,
    &IOHIDOutputTransactionClass::_commit,
    &IOHIDOutputTransactionClass::_clear,
};

IOReturn IOHIDOutputTransactionClass::_createAsyncEventSource(void * self, CFRunLoopSourceRef * pSource)
    { return getThis(self)->createAsyncEventSource(pSource); }

CFRunLoopSourceRef IOHIDOutputTransactionClass::_getAsyncEventSource(void *self)
    { CFTypeRef source = NULL; getThis(self)->getAsyncEventSource(&source); return (CFRunLoopSourceRef)source;}

mach_port_t IOHIDOutputTransactionClass::_getAsyncPort(void *self)
    { mach_port_t port = MACH_PORT_NULL; getThis(self)->getAsyncPort(&port); return port; }

IOReturn IOHIDOutputTransactionClass::_create(void * self)
    { return getThis(self)->create(); }

IOReturn IOHIDOutputTransactionClass::_dispose (void * self)
    { return getThis(self)->dispose(); }

IOReturn IOHIDOutputTransactionClass::_addElement (void * self, IOHIDElementCookie cookie)
    { return getThis(self)->addElement(cookie); }

IOReturn IOHIDOutputTransactionClass::_removeElement (void * self, IOHIDElementCookie cookie)
    { return getThis(self)->removeElement(cookie); }

Boolean IOHIDOutputTransactionClass::_hasElement (void * self, IOHIDElementCookie cookie)
    { return getThis(self)->hasElement(cookie); }

IOReturn IOHIDOutputTransactionClass::_setElementDefault(void * self, IOHIDElementCookie cookie, IOHIDEventStruct * pEvent)
    { return getThis(self)->setElementValue(cookie, pEvent, kIOHIDTransactionOptionDefaultOutputValue); }
    
IOReturn IOHIDOutputTransactionClass::_getElementDefault(void * self, IOHIDElementCookie cookie, IOHIDEventStruct * pEvent)
    { return getThis(self)->getElementValue(cookie, pEvent, kIOHIDTransactionOptionDefaultOutputValue); }

IOReturn IOHIDOutputTransactionClass::_setElementValue(void * self, IOHIDElementCookie cookie, IOHIDEventStruct * pEvent)
    { return getThis(self)->setElementValue(cookie, pEvent); }
    
IOReturn IOHIDOutputTransactionClass::_getElementValue(void * self, IOHIDElementCookie cookie, IOHIDEventStruct * pEvent)
    { return getThis(self)->getElementValue(cookie, pEvent); }

IOReturn IOHIDOutputTransactionClass::_commit(void * self, uint32_t timeoutMS, IOHIDCallbackFunction callback, void * callbackTarget, void * callbackRefcon)
    { return getThis(self)->commit(timeoutMS, callback, callbackTarget, callbackRefcon);}
    
IOReturn IOHIDOutputTransactionClass::_clear(void * self)
    { return getThis(self)->clear();}

IOHIDOutputTransactionClass::IOHIDOutputTransactionClass() : IOHIDTransactionClass()
{
    fHIDTransaction.pseudoVTable = (IUnknownVTbl *)  &sHIDOutputTransactionInterface;
    fHIDTransaction.obj = this;
    
    fCallback   = NULL;
    fTarget     = NULL;
    fRefcon     = NULL;
}

IOHIDOutputTransactionClass::~IOHIDOutputTransactionClass()
{
}

HRESULT IOHIDOutputTransactionClass::queryInterface(REFIID iid, void ** ppv)
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT res = S_OK;

    if (CFEqual(uuid, kIOHIDOutputTransactionInterfaceID))
    {
        *ppv = getInterfaceMap();
        addRef();
    }
    else {
        res = fOwningDevice->queryInterface(iid, ppv);
    }

    if (!*ppv)
        res = E_NOINTERFACE;

    CFRelease(uuid);
    return res;
}

IOReturn IOHIDOutputTransactionClass::createAsyncEventSource(CFRunLoopSourceRef * pSource)
{
    IOReturn ret = IOHIDTransactionClass::getAsyncEventSource((CFTypeRef *)pSource);
    
    if ( ret == kIOReturnSuccess && pSource && *pSource )
        CFRetain(*pSource);
        
    return ret;
}

IOReturn IOHIDOutputTransactionClass::create ()
{
	fDirection = kIOHIDTransactionDirectionTypeOutput;
	
    return IOHIDTransactionClass::create();
}

IOReturn IOHIDOutputTransactionClass::addElement (IOHIDElementCookie cookie)
{
    IOHIDElementRef element = fOwningDevice->getElement(cookie);

    return IOHIDTransactionClass::addElement(element);
}

IOReturn 
IOHIDOutputTransactionClass::addElement(IOHIDElementRef element, 
                                        IOOptionBits options)
{
    return IOHIDTransactionClass::addElement(element, options);
}

IOReturn IOHIDOutputTransactionClass::removeElement (IOHIDElementCookie cookie)
{
    IOHIDElementRef element = fOwningDevice->getElement(cookie);

    return IOHIDTransactionClass::removeElement(element);
}

IOReturn
IOHIDOutputTransactionClass::removeElement(IOHIDElementRef element, 
                                           IOOptionBits options)
{
    return IOHIDTransactionClass::removeElement(element, options);
}

Boolean IOHIDOutputTransactionClass::hasElement (IOHIDElementCookie cookie)
{
    IOHIDElementRef element = fOwningDevice->getElement(cookie);
    Boolean         value   = FALSE;

    return (IOHIDTransactionClass::hasElement(element, &value) == kIOReturnSuccess) ? value : FALSE;
}

IOReturn 
IOHIDOutputTransactionClass::hasElement(IOHIDElementRef element, 
                                        Boolean * pValue, 
                                        IOOptionBits options)
{
    return IOHIDTransactionClass::hasElement(element, pValue, options);
}

/* set the value for that element */
IOReturn 
IOHIDOutputTransactionClass::setElementValue(IOHIDElementCookie cookie, 
                                             IOHIDEventStruct * pEvent, 
                                             IOOptionBits options)
{
    IOHIDValueRef   event;
    IOHIDElementRef element;
    IOReturn        ret;

    if ( !pEvent )
        return kIOReturnBadArgument;
        
    element =  fOwningDevice->getElement(cookie);
    event   = _IOHIDValueCreateWithStruct(kCFAllocatorDefault, element, pEvent);
    
    ret = IOHIDTransactionClass::setElementValue(element, event, options);
    
    CFRelease(event);

    return ret;
}

IOReturn 
IOHIDOutputTransactionClass::setElementValue(IOHIDElementRef element, 
                                             IOHIDValueRef event, 
                                             IOOptionBits options)
{
    return IOHIDTransactionClass::setElementValue(element, event, options);
}
                                
/* get the value for that element */
IOReturn 
IOHIDOutputTransactionClass::getElementValue(IOHIDElementCookie cookie, 
                                             IOHIDEventStruct * pEvent, 
                                             IOOptionBits options)
{
    IOHIDValueRef   event;
    IOReturn        ret;
    
    if ( !pEvent )
        return kIOReturnBadArgument;
        
    ret = IOHIDTransactionClass::getElementValue(fOwningDevice->getElement(cookie), &event, options);

    if ((ret==kIOReturnSuccess) && event)
    {
        uint32_t length = _IOHIDElementGetLength(IOHIDValueGetElement(event));;
        
        pEvent->type            = IOHIDElementGetType(IOHIDValueGetElement(event));
        pEvent->elementCookie   = cookie;
        *(UInt64 *)&pEvent->timestamp = IOHIDValueGetTimeStamp(event);
        
        if ( length > sizeof(uint32_t) )
        {
            pEvent->longValueSize = length;
            pEvent->longValue     = (void *)IOHIDValueGetBytePtr(event);
        }
        else
        {
            pEvent->longValueSize = 0;
            pEvent->longValue     = NULL;
            pEvent->value         = IOHIDValueGetIntegerValue(event);
        }        
    }
    
    return ret;
}

IOReturn 
IOHIDOutputTransactionClass::getElementValue(IOHIDElementRef element, 
                                             IOHIDValueRef * pEvent, IOOptionBits options)
{
    return IOHIDTransactionClass::getElementValue(element, pEvent, options);
}

IOReturn IOHIDOutputTransactionClass::commit(uint32_t timeoutMS, IOHIDCallbackFunction callback, void * target, void * refcon)
{
    fCallback   = callback;
    fTarget     = target;
    fRefcon     = refcon;

    return IOHIDTransactionClass::commit(timeoutMS, IOHIDOutputTransactionClass::_commitCallback, this);
}

IOReturn 
IOHIDOutputTransactionClass::commit(uint32_t timeoutMS, 
                                    IOHIDCallback callback, 
                                    void * callbackRefcon, 
                                    IOOptionBits options)
{
    return IOHIDTransactionClass::commit(timeoutMS, callback, callbackRefcon, options);
}

void IOHIDOutputTransactionClass::_commitCallback(void * context, IOReturn result __unused, void * sender){
    
    IOHIDOutputTransactionClass *transaction = (IOHIDOutputTransactionClass *)context;
    
    if ( transaction && transaction->fCallback) 
    {
        (transaction->fCallback)(transaction->fTarget, kIOReturnSuccess, transaction->fRefcon, sender);
    }
}
