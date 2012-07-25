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
 
#include <CoreFoundation/CFMachPort.h>

#include <IOKit/ps/IOPSKeys.h>
#include "IOHIDUPSClass.h"
#include "IOHIDUsageTables.h"

__BEGIN_DECLS
#include <mach/mach.h>
#include <mach/mach_interface.h>
#include <IOKit/iokitmig.h>
#include <IOKit/IOMessage.h>
__END_DECLS

#define UPSLog(fmt, args...)

#define kDefaultUPSName			"UPS" 

#ifndef kIOPSCommandEnableAudibleAlarmKey
    #define kIOPSCommandEnableAudibleAlarmKey "Enable Audible Alarm"
#endif

#ifndef kIOPSCommandStartupDelayKey
    #define kIOPSCommandStartupDelayKey "Startup Delay"
#endif

#define IS_INPUT_ELEMENT(type) \
((type >= kIOHIDElementTypeInput_Misc) && (type <= kIOHIDElementTypeInput_ScanCodes))

#define NIBBLE_SIGN_EXTEND(value) \
    value |= ( value & 0x8 ) ? (0xf0) : 0

//===========================================================================
// Static methods
//===========================================================================

//---------------------------------------------------------------------------
// ShouldReplaceElement
//---------------------------------------------------------------------------
static bool ShouldReplaceElement(UPSHIDElement * oldValue, UPSHIDElement * newValue)
{
    // Don't replace command items
    if ( newValue->isCommand )
        return false;
        
    if ( oldValue && newValue &&
        ((!oldValue->isDesiredCollection && newValue->isDesiredCollection) || 
        ((newValue->isDesiredCollection == oldValue->isDesiredCollection) &&
        (!oldValue->isDesiredType && newValue->isDesiredType))))
        return true;
                
    return false;
}

//---------------------------------------------------------------------------
// BelongsToCollection
//---------------------------------------------------------------------------
static bool BelongsToCollection(CFDictionaryRef elementDict, UInt32 usagePage, UInt32 usage)
{
    CFDictionaryRef		collection	= elementDict;
    CFNumberRef			number		= NULL;
    UInt32			colUsagePage 	= 0;
    UInt32			colUsage	= 0;

    if ( !collection )
        return false;
    
    while ((collection = (CFDictionaryRef)CFDictionaryGetValue(
                                collection, CFSTR(kIOHIDElementParentCollectionKey))))
    {
        colUsagePage = colUsage = 0;

        number = (CFNumberRef)CFDictionaryGetValue(collection, CFSTR(kIOHIDElementUsagePageKey));
        if ( number )
            CFNumberGetValue(number, kCFNumberSInt32Type, &colUsagePage );

        number = (CFNumberRef)CFDictionaryGetValue(collection, CFSTR(kIOHIDElementUsageKey));
        if ( number )
            CFNumberGetValue(number, kCFNumberSInt32Type, &colUsage );
            
        if ((colUsage == usage) && (usagePage == colUsagePage))
            return true;
    }

    return false;
}

//---------------------------------------------------------------------------
// ShouldPollDevice
//---------------------------------------------------------------------------
static bool ShouldPollDevice(CFDictionaryRef dict)
{    
    CFIndex		count		= 0;
    CFMutableDataRef *	values		= NULL;
    CFStringRef	*	keys		= NULL;
    UPSHIDElement * 	hidElement	= NULL;
    bool		ret		= false;
    
    if ( !dict || ((count = CFDictionaryGetCount(dict))<= 0) ) 
        return false;
        
    keys 	= (CFStringRef *)malloc(sizeof(CFStringRef) * count);
    values 	= (CFMutableDataRef *)malloc(sizeof(CFMutableDataRef) * count);
      
    CFDictionaryGetKeysAndValues(dict, (const void **)keys, (const void **)values);
    
    for (int i=0; i<count; i++)
    {
        if ( !keys[i] || !values[i]) 
            continue;
        
        if ((hidElement = (UPSHIDElement *)CFDataGetMutableBytePtr(values[i]))
            && hidElement->shouldPoll)
        {
            ret =  true;
            break;
        }
    }
    
    free (keys);
    free (values);
    return ret;
}

//===========================================================================
// IOHIDUPSClass class methods
//===========================================================================

//---------------------------------------------------------------------------
// storeUPSElement
//---------------------------------------------------------------------------
void IOHIDUPSClass::storeUPSElement(CFStringRef psKey, UPSHIDElement * newElementRef)
{
    CFMutableArrayRef 	upsElementArray 	= NULL;
    CFMutableDataRef	newData			= NULL;
    CFMutableDataRef	oldData			= NULL;
    CFTypeRef 		upsType			= NULL;
    UPSHIDElement *	oldElementRef 		= NULL;
    bool		replaced		= false;
    bool		added			= false;
    
    if ( !psKey || !newElementRef || !_upsElements || !_hidElements ) 
        return;
    
    // Now create a CFMutableDataRef
    newData = CFDataCreateMutable(kCFAllocatorDefault, sizeof(UPSHIDElement));
    if ( !newData ) return;
    bcopy(newElementRef, CFDataGetMutableBytePtr(newData), sizeof(UPSHIDElement));
        
    // Check to see if we already stored an element that serves the same ps function
    upsType = CFDictionaryGetValue(_upsElements, psKey);

    if ( upsType && (CFGetTypeID(upsType) == CFDataGetTypeID()) &&
        (oldElementRef = (UPSHIDElement *)CFDataGetMutableBytePtr((CFMutableDataRef)upsType)))
    {        
        // An element was already added, but has a different usage.
        // Allocated an array and store both elements
        if ( newElementRef->isCommand ||
            (oldElementRef->usage != newElementRef->usage) ||
            (oldElementRef->usagePage != newElementRef->usagePage))
        {
            upsElementArray = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
            
            CFArrayAppendValue(upsElementArray, upsType);
            CFArrayAppendValue(upsElementArray, newData);
            
            CFDictionarySetValue(_upsElements, psKey, upsElementArray);
            
            CFRelease(upsElementArray);
            
            added = true;
        }
        // The previous element has the same usage.  If the current
        // element is better, replace the old one.
        else if ( (added = replaced = ShouldReplaceElement(oldElementRef, newElementRef)) )
            CFDictionarySetValue(_upsElements, psKey, newData);

    }
    // Looks like we already have more than onle element of this kind
    else if ( upsType && (CFGetTypeID(upsType) == CFArrayGetTypeID()) )
    {
        int 	index;
        bool	found = false;

        upsElementArray = (CFMutableArrayRef)upsType;
        
        for (index=0; (index<CFArrayGetCount(upsElementArray)) && (!newElementRef->isCommand); index++)
        {
            if ((oldData = (CFMutableDataRef)CFArrayGetValueAtIndex(upsElementArray, index))
                && (oldElementRef = (UPSHIDElement *)CFDataGetMutableBytePtr(oldData))
                && (oldElementRef->usagePage == newElementRef->usagePage)
                && (oldElementRef->usage == newElementRef->usage))
            {
                found = true;
                break;
            }
        }
        
        // Looks like we found an exisiting element with the same usage.
        // If the current element is better, replace the old one.
        if ( found )
        {
            if ( (added = replaced = ShouldReplaceElement(oldElementRef, newElementRef)) )
                CFArraySetValueAtIndex(upsElementArray, index, newData);
        }
        // Otherwise, just add it
        else
        {
            CFArrayAppendValue(upsElementArray, newData);
            added = true;
        }
    }
    // No pre-existing element.  Add it to the dictionary.
    else
    {
        CFDictionarySetValue(_upsElements, psKey, newData);
        added = true;
    }
    
    // Now add the data object to the _hidElements dictionary
    // The cookie will be used as a key.  We should also
    // remove and replaced data objects.
    if ( added && newElementRef )
    {
        CFNumberRef cookieNumber = NULL;
        
        cookieNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &(newElementRef->cookie));        
        if ( cookieNumber )
        {
            CFDictionarySetValue(_hidElements, cookieNumber, newData);
            CFRelease(cookieNumber);
        }
        
        if ( replaced && oldElementRef )
        {
            cookieNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &(oldElementRef->cookie));        
            if ( cookieNumber )
            {
                CFDictionaryRemoveValue(_hidElements, cookieNumber);
                CFRelease(cookieNumber);
            }
        }
    }
    CFRelease(newData);
}
 
//---------------------------------------------------------------------------
// alloc
//---------------------------------------------------------------------------
IOCFPlugInInterface ** IOHIDUPSClass::alloc()
{
    IOHIDUPSClass *me;

    me = new IOHIDUPSClass;
    if (me)
        return (IOCFPlugInInterface **) &me->iunknown.pseudoVTable;
    else
        return 0;
}

//---------------------------------------------------------------------------
// IOHIDUPSClass
//---------------------------------------------------------------------------
IOHIDUPSClass::IOHIDUPSClass()
: IOHIDIUnknown(&sIOCFPlugInInterfaceV1)
{
    _upsDevice.pseudoVTable 	= (IUnknownVTbl *)  &sUPSPlugInInterface_v140;
    _upsDevice.obj		= this;

    _service			= 0;

    _asyncEventSource		= NULL;
    
    _hidDeviceInterface		= NULL;
    _hidQueueInterface		= NULL;
    _hidTransactionInterface	= NULL;
    
    _hidProperties		= NULL;
    _hidElements		= NULL;
    _upsElements		= NULL;
    
    _upsEvent			= NULL;
    _upsProperties		= NULL;
    _upsCapabilities		= NULL;

    _eventCallback		= NULL;
    _eventTarget		= NULL;
    _eventRefcon		= NULL;
    
    _isACPresent		= false;

}

//---------------------------------------------------------------------------
// ~IOHIDUPSClass
//---------------------------------------------------------------------------
IOHIDUPSClass::~IOHIDUPSClass()
{
    if (_service) {
        IOObjectRelease(_service);
        _service = 0;
    }
        
    if (_hidProperties) {
        CFRelease(_hidProperties);
        _hidProperties = NULL;
    }
    
    if (_hidElements) {
        CFRelease(_hidElements);
        _hidElements = NULL;
    }

    if (_upsElements) {
        CFRelease(_upsElements);
        _upsElements = NULL;
    }

    if (_upsEvent) {
        CFRelease(_upsEvent);
        _upsEvent = NULL;
    }

    if (_upsProperties) {
        CFRelease(_upsProperties);
        _upsProperties = NULL;
    }

    if (_upsCapabilities) {
        CFRelease(_upsCapabilities);
        _upsCapabilities = NULL;
    }

    if (_hidDeviceInterface) {
        (*_hidDeviceInterface)->Release(_hidDeviceInterface);
        _hidDeviceInterface = NULL;
    }

    if (_hidQueueInterface) {
        (*_hidQueueInterface)->Release(_hidQueueInterface);
        _hidQueueInterface = NULL;
    }
    
    if (_hidTransactionInterface) {
        (*_hidTransactionInterface)->Release(_hidTransactionInterface);
        _hidTransactionInterface = NULL;
    }
    
    if (_asyncEventSource) {
        CFRelease(_asyncEventSource);
        _asyncEventSource = NULL;
    }
}

//---------------------------------------------------------------------------
// queryInterface
//---------------------------------------------------------------------------
HRESULT IOHIDUPSClass::queryInterface(REFIID iid, void **ppv)
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT res = S_OK;

    if (CFEqual(uuid, IUnknownUUID)
         ||  CFEqual(uuid, kIOCFPlugInInterfaceID))
    {
        *ppv = &iunknown;
        addRef();
    }
    else if ( CFEqual(uuid, kIOUPSPlugInInterfaceID) || CFEqual(uuid, kIOUPSPlugInInterfaceID_v140))
    {
        *ppv = &_upsDevice;
        addRef();
    }
    else {
        *ppv = 0;
        UPSLog ("not found\n");
    }

    if (!*ppv)
        res = E_NOINTERFACE;

    CFRelease(uuid);
    return res;
}

//---------------------------------------------------------------------------
// probe
//---------------------------------------------------------------------------
IOReturn IOHIDUPSClass::probe(
                            CFDictionaryRef     propertyTable __unused,
                            io_service_t        service, 
                            SInt32              *order __unused)
{
    if (!service || !IOObjectConformsTo(service, "IOHIDDevice"))
        return kIOReturnBadArgument;

    return kIOReturnSuccess;
}
                            
//---------------------------------------------------------------------------
// start
//---------------------------------------------------------------------------
IOReturn IOHIDUPSClass::start(
                            CFDictionaryRef     propertyTable __unused,
                            io_service_t        service)
{
    IOCFPlugInInterface **	cfPlugInterface = NULL;
    IOReturn			ret 		= kIOReturnSuccess;
    HRESULT 			plugInResult 	= S_OK;
    SInt32			score		= 0;
    
    _service = service;
    IOObjectRetain(_service);

    // Grab the HID properties
    ret = IORegistryEntryCreateCFProperties(service, 
                                &_hidProperties,
                                kCFAllocatorDefault, 
                                kNilOptions);

    if (ret != kIOReturnSuccess)
        return ret;
                                
    // Create the plugin
    ret = IOCreatePlugInInterfaceForService(service,
                            kIOHIDDeviceUserClientTypeID,
                            kIOCFPlugInInterfaceID,
                            &cfPlugInterface,
                            &score);
    
    if (ret != kIOReturnSuccess)
        return ret;

    //Call a method of the intermediate plug-in to create the device
    //interface
    plugInResult = (*cfPlugInterface)->QueryInterface(cfPlugInterface,
                        CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID122),
                        (void **)&_hidDeviceInterface);

    (*cfPlugInterface)->Release(cfPlugInterface);

    if ((plugInResult != S_OK) || !_hidDeviceInterface)
        return kIOReturnError;
        
    ret = (*_hidDeviceInterface)->open(_hidDeviceInterface, kIOHIDOptionsTypeNone);
    
    if (ret != kIOReturnSuccess)
        return ret;

    _upsEvent = CFDictionaryCreateMutable(
                                    kCFAllocatorDefault, 
                                    0, 
                                    &kCFTypeDictionaryKeyCallBacks, 
                                    &kCFTypeDictionaryValueCallBacks);
                                    
    if ( !_upsEvent )
        return kIOReturnNoMemory;
                                    
    if ( !findElements() )
        return kIOReturnError;
        
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// stop
//---------------------------------------------------------------------------
IOReturn IOHIDUPSClass::stop()
{
    IOReturn ret = kIOReturnSuccess;
    
    if ( _hidQueueInterface )
    {
        ret = (*_hidQueueInterface)->stop(_hidQueueInterface);
        ret = (*_hidQueueInterface)->dispose(_hidQueueInterface);
    }
        
    ret = (*_hidDeviceInterface)->close(_hidDeviceInterface);
    
    return ret;
}

//---------------------------------------------------------------------------
// getProperties
//---------------------------------------------------------------------------
IOReturn IOHIDUPSClass::getProperties(CFDictionaryRef * properties)
{
    if ( !_hidProperties )
        return kIOReturnError;
    
    if ( !_upsProperties )
    {
        _upsProperties = CFDictionaryCreateMutable(
                                            kCFAllocatorDefault, 
                                            0, 
                                            &kCFTypeDictionaryKeyCallBacks, 
                                            &kCFTypeDictionaryValueCallBacks);
                                            
        if ( !_upsProperties )
            return kIOReturnNoMemory;
                                            
        CFStringRef		transport 	= NULL;
        CFStringRef		name		= NULL;
        CFMutableStringRef	genName		= NULL;
        
        transport = (CFStringRef) CFDictionaryGetValue( _hidProperties, CFSTR( kIOHIDTransportKey ) );
        
        if (transport)
        {
            if ((CFStringFind(transport, CFSTR(kIOPSUSBTransportType), kCFCompareCaseInsensitive)).length > 0)
                CFDictionarySetValue(_upsProperties, CFSTR(kIOPSTransportTypeKey), CFSTR(kIOPSUSBTransportType));
            else
                CFDictionarySetValue(_upsProperties, CFSTR(kIOPSTransportTypeKey), transport);
        }
        
        name = (CFStringRef) CFDictionaryGetValue( _hidProperties, CFSTR( kIOHIDProductKey ) );
        if ( !name )
            name = (CFStringRef) CFDictionaryGetValue( _hidProperties, CFSTR( kIOHIDManufacturerKey ) );
        if ( !name )
        {
            if (transport)
            {
                genName = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR( kDefaultUPSName ));
                
                if (genName)
                {
                    CFStringInsert(genName, 0, CFSTR(" "));
                    CFStringInsert(genName, 0, transport);
                    CFDictionarySetValue(_upsProperties, CFSTR(kIOPSNameKey), genName);
                    CFRelease(genName);

                }
            }
            else 
            {
                name = CFSTR( kDefaultUPSName );
            }

        }
        
        if (name)
            CFDictionarySetValue(_upsProperties, CFSTR(kIOPSNameKey), name);
    }
    
    if (properties)
        *properties = _upsProperties;

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// getCapabilities
//---------------------------------------------------------------------------
IOReturn IOHIDUPSClass::getCapabilities(CFSetRef * capabilities)
{
    if (!_upsCapabilities && _upsElements)
    {
        CFIndex		count 	= CFDictionaryGetCount(_upsElements);
        CFStringRef *	keys	= NULL;
        CFTypeRef *	values	= NULL;
        
        if ( count <= 0 ) return kIOReturnError;

        keys 	= (CFStringRef *)malloc(sizeof(CFStringRef) * count);
        values 	= (CFTypeRef *)malloc(sizeof(CFTypeRef) * count);

        CFDictionaryGetKeysAndValues(_upsElements, (const void **)keys, (const void **)values);
        _upsCapabilities = CFSetCreate(
                                    kCFAllocatorDefault, 
                                    (const void **)keys,
                                    count, 
                                    &kCFTypeSetCallBacks);
                                    
        free(keys);
        free(values);
    }

    if (capabilities)
        *capabilities = _upsCapabilities;
        
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// getEventProcess
//---------------------------------------------------------------------------
void IOHIDUPSClass::getEventProcess(UPSHIDElement       *elementRef, 
                                    CFStringRef         psKey __unused, 
                                    bool                *changed)
{
    IOReturn err;
    bool ret = false;
    
    if ( !elementRef )
        return;
    
    ret = updateElementValue(elementRef, &err);
    if (kIOReturnSuccess == err)
        processEvent(elementRef);
    
    if ( ret && changed ) 
        *changed = ret;
}

//---------------------------------------------------------------------------
// getEvent
//---------------------------------------------------------------------------
IOReturn IOHIDUPSClass::getEvent(CFDictionaryRef * event, bool * changed)
{
    CFIndex		count		= 0;
    CFMutableDataRef	data 		= NULL;
    CFStringRef	*	keys		= NULL;
    CFTypeRef *		values		= NULL;
    bool		ret 		= false;

    if ( !_upsElements || !_upsEvent ) 
        return kIOReturnError;    
    
    if ( ((count = CFDictionaryGetCount(_upsElements)) <= 0))
        return kIOReturnError;
        
    keys 	= (CFStringRef *)malloc(sizeof(CFStringRef) * count);
    values 	= (CFTypeRef *)malloc(sizeof(CFTypeRef) * count);
    
    CFDictionaryGetKeysAndValues(_upsElements, (const void **)keys, (const void **)values);
    
    for (int i=0; i<count; i++)
    {
        if ( !keys[i] || !values[i]) 
            continue;
        
        if ( CFGetTypeID(values[i]) == CFDataGetTypeID() )
        {
            data = (CFMutableDataRef)values[i];
            getEventProcess((UPSHIDElement *)CFDataGetMutableBytePtr(data), keys[i], &ret);
        }
        else if ( CFGetTypeID(values[i]) == CFArrayGetTypeID() )
        {
            for (int j=0; j<CFArrayGetCount((CFArrayRef)values[i]); j++)
            {
                data = (CFMutableDataRef)CFArrayGetValueAtIndex((CFArrayRef)values[i], j);
                getEventProcess((UPSHIDElement *)CFDataGetMutableBytePtr(data), keys[i], &ret);
            }
        }
    }
    
    if (changed) 
        *changed = ret;

    if (event)
        *event = _upsEvent;
        
    free(keys);
    free(values);
        
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// setEventCallback
//---------------------------------------------------------------------------
IOReturn IOHIDUPSClass::setEventCallback(
                            IOUPSEventCallbackFunction	callback,
                            void *			target,
                            void *			refcon)
{
    _eventCallback	= callback;
    _eventTarget	= target;
    _eventRefcon	= refcon;

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// sendCommandProcess
//---------------------------------------------------------------------------
void IOHIDUPSClass::sendCommandProcess(
                            UPSHIDElement * 			elementRef, 
                            SInt32 				value)
{
    IOHIDEventStruct	event;
    IOReturn		ret;
    
    if ( !_hidTransactionInterface || !elementRef || !elementRef->isCommand)
        return;
        
    if ( !(*_hidTransactionInterface)->hasElement(_hidTransactionInterface, elementRef->cookie) )
    {
        ret = (*_hidTransactionInterface)->addElement(_hidTransactionInterface, elementRef->cookie);
        if (ret != kIOReturnSuccess)
            return;
    }
    
    
    bzero(&event, sizeof(IOHIDEventStruct));
    elementRef->currentValue = event.value = value;
    
    (*_hidTransactionInterface)->setElementValue(_hidTransactionInterface, 
                                    elementRef->cookie,
                                    &event);
}

//---------------------------------------------------------------------------
// sendCommand
//---------------------------------------------------------------------------
IOReturn IOHIDUPSClass::sendCommand(CFDictionaryRef command)
{    
    CFIndex		count 		= 0;
    CFTypeRef	*	values		= NULL;
    CFStringRef	*	keys		= NULL;
    CFMutableDataRef	data		= NULL;
    CFTypeRef		type		= NULL;
    SInt32		value;
    IOReturn		ret 		= kIOReturnSuccess;
    
    if ( !command || (((count = CFDictionaryGetCount(command)) <= 0)))
        return ret;
        
    if ( !_hidTransactionInterface )
    {
        _hidTransactionInterface = (*_hidDeviceInterface)->allocOutputTransaction(_hidDeviceInterface);
        if ( !_hidTransactionInterface )
            return kIOReturnError;
            
        ret = (*_hidTransactionInterface)->create(_hidTransactionInterface);
        
        if ( ret != kIOReturnSuccess )
        {
            (*_hidTransactionInterface)->Release(_hidTransactionInterface);
            _hidTransactionInterface = NULL;
            return ret;
        }    
    }
        
    keys 	= (CFStringRef *)malloc(sizeof(CFStringRef) * count);
    values 	= (CFTypeRef *)malloc(sizeof(CFTypeRef) * count);
        
    CFDictionaryGetKeysAndValues(command, (const void **)keys, (const void **)values);
    
    for (int i=0; i<count; i++)
    {
        if ( !keys[i] || !values[i] || !(type = (CFTypeRef)CFDictionaryGetValue(_upsElements, keys[i]))) 
            break;
        
        // Convert from minutes to seconds
        if (CFEqual(CFSTR(kIOPSCommandDelayedRemovePowerKey), keys[i]) ||
            CFEqual(CFSTR(kIOPSCommandStartupDelayKey), keys[i]))
        {
            CFNumberGetValue((CFNumberRef)values[i], kCFNumberSInt32Type, &value );
            if (value != -1)
                value *= 60;
        }
        else if (CFEqual(CFSTR(kIOPSCommandEnableAudibleAlarmKey), keys[i]))
        {
            value = (((CFBooleanRef)values[i]) == kCFBooleanTrue) ? 2 : 1;
        }
            
        if ( CFGetTypeID(type) == CFDataGetTypeID() )
        {
            data = (CFMutableDataRef)type;
            sendCommandProcess((UPSHIDElement *)CFDataGetMutableBytePtr(data), value);
        }
        else if ( CFGetTypeID(type) == CFArrayGetTypeID() )
        {
            for (int j=0; j<CFArrayGetCount((CFArrayRef)type); j++)
            {
                data = (CFMutableDataRef)CFArrayGetValueAtIndex((CFArrayRef)type, j);
                sendCommandProcess((UPSHIDElement *)CFDataGetMutableBytePtr(data), value);
            }
        }
    }
    
    free(keys);
    free(values);

    return (*_hidTransactionInterface)->commit(_hidTransactionInterface, 0, 0, 0, 0);
}

//---------------------------------------------------------------------------
// createAsyncEventSource
//---------------------------------------------------------------------------
IOReturn IOHIDUPSClass::createAsyncEventSource(CFTypeRef * eventSource)
{
    if (!eventSource)
        return kIOReturnBadArgument;
        
    // Set up CFTimerEventSource
    if ( ShouldPollDevice(_hidElements) )
    {
        CFRunLoopTimerContext timerContext;

        bzero(&timerContext, sizeof(CFRunLoopTimerContext));

        timerContext.info = this;

        _asyncEventSource = CFRunLoopTimerCreate(NULL,
                             CFAbsoluteTimeGetCurrent(),    // fire date
                             (CFTimeInterval)5.0,           // interval (kUPSPollingInterval)
                             0, 
                             0, 
                             IOHIDUPSClass::_timerCallbackFunction, 
                             &timerContext);
    }
    else if ( !setupQueue() )
        return kIOReturnError;
     
    if ( !_asyncEventSource )
        return kIOReturnError;
     
    *eventSource = _asyncEventSource;
    CFRetain( _asyncEventSource );      // Retain for our own purposes
    
    return kIOReturnSuccess;
}


//---------------------------------------------------------------------------
// findElements
//---------------------------------------------------------------------------
bool IOHIDUPSClass::findElements()
{
    CFArrayRef		elementArray	= NULL;
    CFNumberRef		number		= NULL;
    CFStringRef		psKey		= NULL;
    CFDictionaryRef	element		= NULL;
    IOReturn		ret		= kIOReturnError;
    UPSHIDElement	newElement;

    _hidElements = CFDictionaryCreateMutable(
                                    kCFAllocatorDefault, 
                                    0, 
                                    &kCFTypeDictionaryKeyCallBacks, 
                                    &kCFTypeDictionaryValueCallBacks);
                                    
    _upsElements = CFDictionaryCreateMutable(
                                    kCFAllocatorDefault, 
                                    0, 
                                    &kCFTypeDictionaryKeyCallBacks, 
                                    &kCFTypeDictionaryValueCallBacks);
                                    
    if ( !_hidElements || !_upsElements )
        return false;
        
    // Let's find the elements
    ret = (*_hidDeviceInterface)->copyMatchingElements(	
                                    _hidDeviceInterface, 
                                    NULL, 
                                    &elementArray);


    if ( (ret != kIOReturnSuccess) || !elementArray)
        goto FIND_ELEMENT_CLEANUP;

    for (int i=0; i<CFArrayGetCount(elementArray); i++)
    {
        element = (CFDictionaryRef) CFArrayGetValueAtIndex(elementArray, i);
        if ( !element )
            continue;
    
        psKey = NULL;
        bzero(&newElement, sizeof(UPSHIDElement));
        
        number = (CFNumberRef)CFDictionaryGetValue(element, CFSTR(kIOHIDElementUsagePageKey));
        if ( !number ) continue;
        CFNumberGetValue(number, kCFNumberSInt32Type, &newElement.usagePage );

        number = (CFNumberRef)CFDictionaryGetValue(element, CFSTR(kIOHIDElementUsageKey));
        if ( !number ) continue;
        CFNumberGetValue(number, kCFNumberSInt32Type, &newElement.usage );
        
        number = (CFNumberRef)CFDictionaryGetValue(element, CFSTR(kIOHIDElementCookieKey));
        if ( !number ) continue;
        CFNumberGetValue(number, kCFNumberIntType, &(newElement.cookie) );
        
        number = (CFNumberRef)CFDictionaryGetValue(element, CFSTR(kIOHIDElementTypeKey));
        if ( !number ) continue;
        CFNumberGetValue(number, kCFNumberIntType, &(newElement.type) );

        number = (CFNumberRef)CFDictionaryGetValue(element, CFSTR(kIOHIDElementUnitKey));
        if ( !number ) continue;
        CFNumberGetValue(number, kCFNumberIntType, &(newElement.unit) );

        number = (CFNumberRef)CFDictionaryGetValue(element, CFSTR(kIOHIDElementUnitExponentKey));
        if ( !number ) continue;
        CFNumberGetValue(number, kCFNumberSInt8Type, &(newElement.unitExponent) );
        NIBBLE_SIGN_EXTEND(newElement.unitExponent);
        
        newElement.multiplier		= 1.0;
        newElement.shouldPoll 		= (newElement.type == kIOHIDElementTypeFeature);
        newElement.isDesiredType 	= IS_INPUT_ELEMENT(newElement.type);
        newElement.isDesiredCollection 	= BelongsToCollection(element, kHIDPage_PowerDevice, kHIDUsage_PD_PowerSummary);

        if ( newElement.usagePage == kHIDPage_PowerDevice )
        {
            switch ( newElement.usage )
            {
                case kHIDUsage_PD_DelayBeforeShutdown:
                    newElement.isDesiredType = 
                        (( newElement.type == kIOHIDElementTypeFeature) || ( newElement.type == kIOHIDElementTypeOutput));
                    if ( newElement.isDesiredType )
                    {
                        psKey 				= CFSTR(kIOPSCommandDelayedRemovePowerKey);
                        newElement.shouldPoll 		= false;
                        newElement.isCommand 		= true;
                    }
                    break;
                case kHIDUsage_PD_DelayBeforeStartup:
                    newElement.isDesiredType = 
                        (( newElement.type == kIOHIDElementTypeFeature) || ( newElement.type == kIOHIDElementTypeOutput));
                    if ( newElement.isDesiredType )
                    {
                        psKey 				= CFSTR(kIOPSCommandStartupDelayKey);
                        newElement.shouldPoll 		= false;
                        newElement.isCommand 		= true;
                    }
                    break;
                case kHIDUsage_PD_Voltage:
                    if ( newElement.unit == kIOHIDUnitVolt )
                        newElement.multiplier = pow(10, (3 + (newElement.unitExponent - kIOHIDUnitExponentVolt)));
                    else
                        newElement.multiplier = 1000.0;
                        
                    psKey = CFSTR(kIOPSVoltageKey);
                    break;
                case kHIDUsage_PD_Current:
                    if ( newElement.unit == kIOHIDUnitAmp )
                        newElement.multiplier = pow(10, (3 + (newElement.unitExponent - kIOHIDUnitExponentAmp)));
                    else
                        newElement.multiplier = 1000.0;
                        
                    psKey = CFSTR(kIOPSCurrentKey);
                    break;
                case kHIDUsage_PD_AudibleAlarmControl:
                    newElement.isDesiredType = 
                        (( newElement.type == kIOHIDElementTypeFeature) || ( newElement.type == kIOHIDElementTypeOutput));
                    if ( newElement.isDesiredType )
                    {
                        psKey 			= CFSTR(kIOPSCommandEnableAudibleAlarmKey);
                        newElement.shouldPoll	= false;
                        newElement.isCommand	= true;
                    }
                    break;
            }
        }
        else if ( newElement.usagePage == kHIDPage_BatterySystem )
        {

            switch ( newElement.usage )
            {
                case kHIDUsage_BS_Charging:
                    psKey = CFSTR(kIOPSIsChargingKey);
                    break;
                case kHIDUsage_BS_Discharging:
                    psKey = CFSTR(kIOPSIsChargingKey);
                    break;
                case kHIDUsage_BS_RemainingCapacity:
                    psKey = CFSTR(kIOPSCurrentCapacityKey);
                    break;
                case kHIDUsage_BS_RunTimeToEmpty:
                    psKey = CFSTR(kIOPSTimeToEmptyKey);
                    break;
                case kHIDUsage_BS_AverageTimeToFull:
                    psKey = CFSTR(kIOPSTimeToFullChargeKey);
                    break;               
                case kHIDUsage_BS_ACPresent:
                    psKey = CFSTR(kIOPSPowerSourceStateKey);
                    break;
            }
        }
              
        if (psKey) 
            storeUPSElement(psKey, &newElement);
        
    }
    
FIND_ELEMENT_CLEANUP:
    if ( elementArray ) CFRelease(elementArray);
    
    return ( CFDictionaryGetCount(_hidElements) > 0 );
}

//---------------------------------------------------------------------------
// setupQueue
//---------------------------------------------------------------------------
bool IOHIDUPSClass::setupQueue()
{
    CFIndex		count 		= 0;
    CFMutableDataRef *	elements	= NULL;
    CFStringRef *	keys		= NULL;
    IOReturn		ret;
    UPSHIDElement *	tempHIDElement	= NULL;
    bool		cookieAdded 	= false;
    bool		boolRet		= true;

    if ( !_hidElements || (((count = CFDictionaryGetCount(_hidElements)) <= 0)))
        return false;
        
    keys 	= (CFStringRef *)malloc(sizeof(CFStringRef) * count);
    elements 	= (CFMutableDataRef *)malloc(sizeof(CFMutableDataRef) * count);
                
    CFDictionaryGetKeysAndValues(_hidElements, (const void **)keys, (const void **)elements);
    
    _hidQueueInterface = (*_hidDeviceInterface)->allocQueue(_hidDeviceInterface);
    if ( !_hidQueueInterface )
    {
        boolRet = false;
        goto SETUP_QUEUE_CLEANUP;
    }
        
    ret = (*_hidQueueInterface)->create(_hidQueueInterface, 0, 8);
    if (ret != kIOReturnSuccess)
    {
        boolRet = false;
        goto SETUP_QUEUE_CLEANUP;
    }
        
    for (int i=0; i<count; i++)
    {
        if ( !elements[i] || 
            !(tempHIDElement = (UPSHIDElement *)CFDataGetMutableBytePtr(elements[i])))
            continue;
        
        if ((tempHIDElement->type < kIOHIDElementTypeInput_Misc) || (tempHIDElement->type > kIOHIDElementTypeInput_ScanCodes))
            continue;
            
        ret = (*_hidQueueInterface)->addElement(_hidQueueInterface, tempHIDElement->cookie, 0);
        
        if (ret == kIOReturnSuccess)
            cookieAdded = true;

    }
    
    if ( cookieAdded )
    {
        ret = (*_hidQueueInterface)->createAsyncEventSource(_hidQueueInterface, (CFRunLoopSourceRef *)&_asyncEventSource);
        if ( ret != kIOReturnSuccess )
        {
            boolRet = false;
            goto SETUP_QUEUE_CLEANUP;
        }
    
        ret = (*_hidQueueInterface)->setEventCallout(_hidQueueInterface, IOHIDUPSClass::_queueCallbackFunction, this, NULL);
        if ( ret != kIOReturnSuccess )
        {
            boolRet = false;
            goto SETUP_QUEUE_CLEANUP;
        }
        
        ret = (*_hidQueueInterface)->start(_hidQueueInterface);
        if ( ret != kIOReturnSuccess )
        {
            boolRet = false;
            goto SETUP_QUEUE_CLEANUP;
        }
    }
    else 
    {
        (*_hidQueueInterface)->stop(_hidQueueInterface);
        (*_hidQueueInterface)->dispose(_hidQueueInterface);    
        (*_hidQueueInterface)->Release(_hidQueueInterface);
        _hidQueueInterface = NULL;        
    }
    
SETUP_QUEUE_CLEANUP:

    free(keys);
    free(elements);
    
    return boolRet;
}


//---------------------------------------------------------------------------
// _queueCallbackFunction
//---------------------------------------------------------------------------
void IOHIDUPSClass::_queueCallbackFunction(
                            void            *target, 
                            IOReturn        result, 
                            void            *refcon __unused, 
                            void            *sender)
{
    IOHIDUPSClass * 	self 		= (IOHIDUPSClass *)target;
    AbsoluteTime 	zeroTime 	= {0,0};
    CFNumberRef		number		= NULL;
    CFMutableDataRef	element		= NULL;
    UPSHIDElement *	tempHIDElement;	
    IOHIDEventStruct 	event;
    
    if ( !self || ( sender != self->_hidQueueInterface))
        return;
        
    while (result == kIOReturnSuccess) 
    {
        result = (*self->_hidQueueInterface)->getNextEvent(
                                        self->_hidQueueInterface, 
                                        &event, 
                                        zeroTime, 
                                        0);
                                        
        if ( result != kIOReturnSuccess )
            continue;
        
        // Only intersted in 32 values right now
        if ((event.longValueSize != 0) && (event.longValue != NULL))
        {
            free(event.longValue);
            continue;
        }
        
        number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &event.elementCookie);        
        if ( !number )  continue;
        element = (CFMutableDataRef)CFDictionaryGetValue(self->_hidElements, number);
        CFRelease(number);
        
        if ( !element || 
            !(tempHIDElement = (UPSHIDElement *)CFDataGetMutableBytePtr(element)))  
            continue;
        
        tempHIDElement->currentValue = event.value;

        if (!self->processEvent(tempHIDElement)) 
            continue;
        
        if (self->_eventCallback)
        {
            (self->_eventCallback)(
                                self->_eventTarget,
                                kIOReturnSuccess,
                                self->_eventRefcon,
                                (void *)&self->_upsDevice,
                                self->_upsEvent);
        }
    }
}

//---------------------------------------------------------------------------
// _timerCallbackFunction
//---------------------------------------------------------------------------
void IOHIDUPSClass::_timerCallbackFunction(
                                CFRunLoopTimerRef   timer __unused, 
                                void                *refCon)
{
    IOHIDUPSClass * 	self 		= (IOHIDUPSClass *)refCon;
    bool		changed		= false;
    
    if (!self) return;
    
    self->getEvent((CFDictionaryRef *)&(self->_upsEvent), &changed);

    if (changed && self->_eventCallback)
    {
        (self->_eventCallback)(
                            self->_eventTarget,
                            kIOReturnSuccess,
                            self->_eventRefcon,
                            (void *)&self->_upsDevice,
                            self->_upsEvent);
    }
}

static inline bool FillDictinoaryWithInt(
                            CFMutableDictionaryRef	dict,
                            CFStringRef			key,
                            SInt32			value)
{
    CFNumberRef	number;
    
    if ( !dict || !key)
        return false;
    
    number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
    
    if ( !number ) return false;
    
    CFDictionarySetValue(dict, key, number);
    CFRelease( number );
    
    return true;
}

static UPSHIDElement * GetHIDElement(CFDictionaryRef dict, CFStringRef key)
{
    CFTypeRef 		type = CFDictionaryGetValue(dict, key);
    CFMutableDataRef	data = NULL;
    
    if (type)
    {
        if ( CFGetTypeID(type) == CFDataGetTypeID() )
        {
            data = (CFMutableDataRef)type;
            return (UPSHIDElement *)CFDataGetMutableBytePtr(data);
        }
        else if ( CFGetTypeID(type) == CFArrayGetTypeID() )
        {
            if ((data = (CFMutableDataRef)CFArrayGetValueAtIndex((CFArrayRef)type, 0)))
                return (UPSHIDElement *)CFDataGetMutableBytePtr(data);
        }
    }
    
    return NULL;

}
                            
//---------------------------------------------------------------------------
// processEvent
//---------------------------------------------------------------------------
bool IOHIDUPSClass::processEvent(UPSHIDElement *	hidElement)
{
    bool		update		= false;
    bool		updateCharge	= false;
    bool		isCharging	= false;
    SInt32		value		= 0;
    
    if ( hidElement->usagePage == kHIDPage_PowerDevice )
    {
        switch ( hidElement->usage )
        {
            case kHIDUsage_PD_Voltage:
                // PS expects mv but HID units are V
                value = (SInt32)((double)hidElement->currentValue * hidElement->multiplier);
                update = FillDictinoaryWithInt(_upsEvent, CFSTR(kIOPSVoltageKey), value);
                break;
            case kHIDUsage_PD_Current:
                // PS expects mA but HID units are A
                value = (SInt32)((double)hidElement->currentValue * hidElement->multiplier);
                update = FillDictinoaryWithInt(_upsEvent, CFSTR(kIOPSCurrentKey), value);
                break;
        }
    }
    else if (hidElement->usagePage == kHIDPage_BatterySystem)
    {
        switch ( hidElement->usage )
        {
            case kHIDUsage_BS_Charging:
                update 		= true;
                updateCharge 	= true;
                isCharging 	= hidElement->currentValue;
                CFDictionarySetValue(_upsEvent, CFSTR(kIOPSIsChargingKey), ( hidElement->currentValue ? kCFBooleanTrue : kCFBooleanFalse));
                if ( isCharging != _isACPresent )
                    goto PROCESS_EVENT_UPDATE_AC;

                break;
            case kHIDUsage_BS_Discharging:
                update 		= true;
                updateCharge 	= true;
                isCharging 	= (hidElement->currentValue == 0);
                CFDictionarySetValue(_upsEvent, CFSTR(kIOPSIsChargingKey), ( hidElement->currentValue ? kCFBooleanFalse : kCFBooleanTrue));
                if ( isCharging != _isACPresent )
                    goto PROCESS_EVENT_UPDATE_AC;

                break;
            case kHIDUsage_BS_RemainingCapacity:
                update = FillDictinoaryWithInt(_upsEvent, CFSTR(kIOPSCurrentCapacityKey), hidElement->currentValue);
                
                // Update time to full
                value = 100 - hidElement->currentValue;
                if ((hidElement = GetHIDElement(_upsElements, CFSTR(kIOPSTimeToFullChargeKey))))
                {
                    value = (UInt32)(((float)hidElement->currentValue) * ((float)value / 100.0));
                    
                    // PS expects minutes but HID units are secs
                    update = FillDictinoaryWithInt(_upsEvent, CFSTR(kIOPSTimeToFullChargeKey),(value/60));
                }
                // Update the Time to empty
                // PS expects minutes but HID units are secs
                if ( (hidElement = GetHIDElement(_upsElements, CFSTR(kIOPSTimeToEmptyKey))) && updateElementValue(hidElement, NULL) )
                {
                    FillDictinoaryWithInt(_upsEvent, CFSTR(kIOPSTimeToEmptyKey), hidElement->currentValue/60);
                }
                
                break;
            case kHIDUsage_BS_RunTimeToEmpty:
                // PS expects minutes but HID units are secs
                update = FillDictinoaryWithInt(_upsEvent, CFSTR(kIOPSTimeToEmptyKey), hidElement->currentValue/60);
                break;
            case kHIDUsage_BS_AverageTimeToFull:
                value = hidElement->currentValue;
                if ((hidElement = GetHIDElement(_upsElements, CFSTR(kIOPSCurrentCapacityKey))))
                {
                    value = (UInt32)(((float)value) * (((float)(100 - hidElement->currentValue)) / 100.0));
                    
                    // PS expects minutes but HID units are secs
                    update = FillDictinoaryWithInt(_upsEvent, CFSTR(kIOPSTimeToFullChargeKey), (value/60));
                }
                break;            
            case kHIDUsage_BS_ACPresent:
                update = true;
PROCESS_EVENT_UPDATE_AC:
                if (updateCharge)
                    _isACPresent = isCharging;
                else
                    _isACPresent = hidElement->currentValue ? true : false;

                if (_isACPresent)
                    CFDictionarySetValue(_upsEvent, CFSTR(kIOPSPowerSourceStateKey),
                                        CFSTR(kIOPSACPowerValue));
                else
                    CFDictionarySetValue(_upsEvent, CFSTR(kIOPSPowerSourceStateKey), 
                                        CFSTR(kIOPSBatteryPowerValue));
                break;
        }
    }
    
    return update;
}

//---------------------------------------------------------------------------
// updateElementValue
//---------------------------------------------------------------------------
bool IOHIDUPSClass::updateElementValue(UPSHIDElement *	tempHIDElement, IOReturn * error)
{
    IOHIDEventStruct 	valueEvent;
    IOReturn		ret;
    bool		updated		= false;
    
    if (error)
        *error = kIOReturnBadArgument;
    if ( !tempHIDElement)
        return false;
        
    if ( tempHIDElement->type == kIOHIDElementTypeFeature )
    {
        ret = (*_hidDeviceInterface)->queryElementValue(_hidDeviceInterface, tempHIDElement->cookie, &valueEvent, 0, 0, 0, 0);
    }
    else
	{
        ret = (*_hidDeviceInterface)->getElementValue(_hidDeviceInterface, tempHIDElement->cookie, &valueEvent);

        // If this is a null timestamp and not an output element, query the value.
        // P.S. Don't continue to poll if the last attempt generated an error.
        if ((ret == kIOReturnSuccess) && 
            (tempHIDElement->lastReturn == kIOReturnSuccess) &&
            (*(UInt64 *)(&(valueEvent.timestamp)) == 0) && 
            (tempHIDElement->type != kIOHIDElementTypeOutput))
        {
            ret = (*_hidDeviceInterface)->queryElementValue(_hidDeviceInterface, tempHIDElement->cookie, &valueEvent, 0, 0, 0, 0);
            tempHIDElement->lastReturn = ret;
        }
     }

    if (error)
        *error = ret;
    if (ret != kIOReturnSuccess)
        return updated;

    if ((valueEvent.longValueSize != 0) && (valueEvent.longValue != NULL))
    {
        free(valueEvent.longValue);
        return updated;
    }

    if (tempHIDElement->currentValue != valueEvent.value)
    {
        tempHIDElement->currentValue = valueEvent.value;
        updated = true;
    }
    
    UPSLog("IOHIDUPSClass::updateElementValue: usagePage=0x%2.2x usage=0x%2.2x value=%ld\n", tempHIDElement->usagePage, tempHIDElement->usage, tempHIDElement->currentValue);
    
    return updated;
}


//===========================================================================
// CFPlugIn Static Assignments
//===========================================================================
IOCFPlugInInterface IOHIDUPSClass::sIOCFPlugInInterfaceV1 =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    1, 0,	// version/revision
    &IOHIDUPSClass::_probe,
    &IOHIDUPSClass::_start,
    &IOHIDUPSClass::_stop
};

IOUPSPlugInInterface_v140 IOHIDUPSClass::sUPSPlugInInterface_v140 =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    &IOHIDUPSClass::_getProperties,
    &IOHIDUPSClass::_getCapabilities,
    &IOHIDUPSClass::_getEvent,
    &IOHIDUPSClass::_setEventCallback,
    &IOHIDUPSClass::_sendCommand,
    &IOHIDUPSClass::_createAsyncEventSource
};

//===========================================================================
// IOCFPlugInInterface methods
//===========================================================================

//---------------------------------------------------------------------------
// _probe
//---------------------------------------------------------------------------
IOReturn IOHIDUPSClass::_probe(void *self,
                                CFDictionaryRef propertyTable,
                                io_service_t service, SInt32 *order)
{
    return getThis(self)->probe(propertyTable, service, order);
}

//---------------------------------------------------------------------------
// _start
//---------------------------------------------------------------------------
IOReturn IOHIDUPSClass::_start(void *self,
                                CFDictionaryRef propertyTable,
                                io_service_t service)
{
    return getThis(self)->start(propertyTable, service);
}

//---------------------------------------------------------------------------
// _stop
//---------------------------------------------------------------------------
IOReturn IOHIDUPSClass::_stop(void *self)
{
    return getThis(self)->stop();
}
    
//===========================================================================
// IOUPSPlugInInterface methods
//===========================================================================

//---------------------------------------------------------------------------
// _getProperties
//---------------------------------------------------------------------------
IOReturn IOHIDUPSClass::_getProperties(
                            void * 			self,
                            CFDictionaryRef *		properties)
{
    return getThis(self)->getProperties(properties);
}

//---------------------------------------------------------------------------
// _getCapabilities
//---------------------------------------------------------------------------
IOReturn IOHIDUPSClass::_getCapabilities(
                            void * 			self,
                            CFSetRef *		capabilities)
{
    return getThis(self)->getCapabilities(capabilities);
}

//---------------------------------------------------------------------------
// _getEvent
//---------------------------------------------------------------------------
IOReturn IOHIDUPSClass::_getEvent(
                            void * 			self,
                            CFDictionaryRef *		event)
{
    return getThis(self)->getEvent(event);
}

//---------------------------------------------------------------------------
// _setEventCallback
//---------------------------------------------------------------------------
IOReturn IOHIDUPSClass::_setEventCallback(
                            void * 			self,
                            IOUPSEventCallbackFunction	callback,
                            void *			target,
                            void *			refcon)
{
    return getThis(self)->setEventCallback(callback, target, refcon);
}


//---------------------------------------------------------------------------
// _sendCommand
//---------------------------------------------------------------------------
IOReturn IOHIDUPSClass::_sendCommand(
                            void * 			self,
                            CFDictionaryRef		command)
{
    return getThis(self)->sendCommand(command);
}

//---------------------------------------------------------------------------
// _createAsyncEventSource
//---------------------------------------------------------------------------
IOReturn IOHIDUPSClass::_createAsyncEventSource(
                            void *          self,
                            CFTypeRef *     eventSource)
{
    return getThis(self)->createAsyncEventSource(eventSource);
}
