/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2009 Apple Computer, Inc.  All Rights Reserved.
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

#include <TargetConditionals.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/IOLib.h>
#include <IOKit/usb/USB.h>

#include "IOHIDSystem.h"
#include "IOHIDEventService.h"
#include "IOHIDInterface.h"
#include "IOHIDKeys.h"
#include "IOHIDPrivateKeys.h"
#include "AppleHIDUsageTables.h"

#if !TARGET_OS_EMBEDDED
    #include "IOHIDPointing.h"
    #include "IOHIDKeyboard.h"
    #include "IOHIDConsumer.h"
#endif /* TARGET_OS_EMBEDDED */

#include "IOHIDFamilyPrivate.h"
#include "IOHIDevicePrivateKeys.h"
#include "ev_private.h"
#include "IOHIDFamilyTrace.h"

enum {
    kBootProtocolNone   = 0,
    kBootProtocolKeyboard,
    kBootProtocolMouse
};

enum {
    kShimEventProcessor = 0x01
};

#define     kDefaultFixedResolution             (400 << 16)
#define     kDefaultScrollFixedResolution       (9 << 16)

#define     kMaxSystemBarrelPressure            32767
#define     kMaxSystemTipPressure               65535

#define     kDelayedOption                      (1<<31)

#define     NUB_LOCK                            if (_nubLock) IORecursiveLockLock(_nubLock)
#define     NUB_UNLOCK                          if (_nubLock) IORecursiveLockUnlock(_nubLock)

#if TARGET_OS_EMBEDDED
    #define     SET_HID_PROPERTIES_EMBEDDED(service)                                \
        service->setProperty(kIOHIDPrimaryUsagePageKey, getPrimaryUsagePage(), 32); \
        service->setProperty(kIOHIDPrimaryUsageKey, getPrimaryUsage(), 32);         \
        service->setProperty(kIOHIDReportIntervalKey, getReportInterval(), 32);
#else
    #define     SET_HID_PROPERTIES_EMBEDDED(service)                                \
        {};
#endif


#define     SET_HID_PROPERTIES(service)                                     \
    service->setProperty(kIOHIDTransportKey, getTransport());               \
    service->setProperty(kIOHIDLocationIDKey, getLocationID(), 32);         \
    service->setProperty(kIOHIDVendorIDKey, getVendorID(), 32);             \
    service->setProperty(kIOHIDVendorIDSourceKey, getVendorIDSource(), 32); \
    service->setProperty(kIOHIDProductIDKey, getProductID(), 32);           \
    service->setProperty(kIOHIDVersionNumberKey, getVersion(), 32);         \
    service->setProperty(kIOHIDCountryCodeKey, getCountryCode(), 32);       \
    service->setProperty(kIOHIDManufacturerKey, getManufacturer());         \
    service->setProperty(kIOHIDProductKey, getProduct());                   \
    service->setProperty(kIOHIDSerialNumberKey, getSerialNumber());         \
    service->setProperty(kIOHIDDeviceUsagePairsKey, getDeviceUsagePairs());

#define		_provider							_reserved->provider
#define     _workLoop                           _reserved->workLoop
#define     _ejectDelayMS                       _reserved->ejectDelayMS
#define     _ejectTimerEventSource              _reserved->ejectTimerEventSource
#define     _ejectState                         _reserved->ejectState
#define     _ejectOptions                       _reserved->ejectOptions
#define     _capsDelayMS                        _reserved->capsDelayMS
#define     _capsTimerEventSource               _reserved->capsTimerEventSource
#define     _capsState                          _reserved->capsState
#define     _capsOptions                        _reserved->capsOptions
#define     _deviceUsagePairs                   _reserved->deviceUsagePairs
#define     _commandGate                        _reserved->commandGate

#if TARGET_OS_EMBEDDED
    #define     _clientDict                         _reserved->clientDict
    #define     _debuggerMask                       _reserved->debuggerMask
    #define     _startDebuggerMask                  _reserved->startDebuggerMask
    #define     _debuggerTimerEventSource           _reserved->debuggerTimerEventSource
    #define     _shouldSwapISO                      _reserved->shouldSwapISO

    #define     kDebuggerDelayMS                    2500

    //===========================================================================
    // IOHIDClientData class
    class IOHIDClientData : public OSObject
    {
        OSDeclareDefaultStructors(IOHIDClientData)

        IOService * client;
        void *      context;
        void *      action;
        
    public:
        static IOHIDClientData* withClientInfo(IOService *client, void* context, void * action);
        inline IOService *  getClient()     { return client; }
        inline void *       getContext()    { return context; }
        inline void *       getAction()     { return action; }
    };

#endif /* TARGET_OS_EMBEDDED */

struct  TransducerData {
    UInt32  reportID;
    UInt32  deviceID;
    UInt32  type;
    UInt32  capabilities;
    bool    digitizerCollection;
    bool    supportsTransducerIndex;
};

//===========================================================================
// IOHIDEventService class

#define super IOService

OSDefineMetaClassAndAbstractStructors( IOHIDEventService, IOService )
//====================================================================================================
// IOHIDEventService::init
//====================================================================================================
bool IOHIDEventService::init ( OSDictionary * properties )
{
    if (!super::init(properties))
        return false;

    _reserved = IONew(ExpansionData, 1);
    bzero(_reserved, sizeof(ExpansionData));

    _nubLock = IORecursiveLockAlloc();         

#if TARGET_OS_EMBEDDED
    _clientDict = OSDictionary::withCapacity(2);
    if ( _clientDict == 0 )
        return false;
#endif /* TARGET_OS_EMBEDDED */

    _ejectDelayMS = kEjectKeyDelayMS;
    
    return true;
}
//====================================================================================================
// IOHIDEventService::start
//====================================================================================================
bool IOHIDEventService::start ( IOService * provider )
{
    UInt32      bootProtocol = 0;
    OSNumber *  number       = NULL;
    
    _provider = provider;
    
    if ( !super::start(provider) )
        return false;
        
    if ( !handleStart(provider) )
        return false;
        
    _workLoop = getWorkLoop();
    if ( !_workLoop )
        return false;
        
    _workLoop->retain();
    
    _ejectTimerEventSource = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &IOHIDEventService::ejectTimerCallback));
    if (!_ejectTimerEventSource || (_workLoop->addEventSource(_ejectTimerEventSource) != kIOReturnSuccess))
        return false;
        
    number = (OSNumber*)copyProperty(kIOHIDKeyboardEjectDelay);
    if ( OSDynamicCast(OSNumber, number) )
        _ejectDelayMS = number->unsigned32BitValue();
    OSSafeReleaseNULL(number);
        
    _capsTimerEventSource = 
            IOTimerEventSource::timerEventSource(this, 
                                                 OSMemberFunctionCast(IOTimerEventSource::Action, 
                                                                      this, 
                                                                      &IOHIDEventService::capsTimerCallback));
    if (!_capsTimerEventSource || (_workLoop->addEventSource(_capsTimerEventSource) != kIOReturnSuccess))
        return false;
    
    _commandGate = IOCommandGate::commandGate(this);
    if (!_commandGate || (_workLoop->addEventSource(_commandGate) != kIOReturnSuccess))
        return false;
        
    calculateCapsLockDelay();
    
    calculateStandardType();
        
    SET_HID_PROPERTIES(this);
    SET_HID_PROPERTIES_EMBEDDED(this);
        
    number = (OSNumber*)copyProperty("BootProtocol");
    if (OSDynamicCast(OSNumber, number))
        bootProtocol = number->unsigned32BitValue();
    OSSafeReleaseNULL(number);
        
    parseSupportedElements (getReportElements(), bootProtocol);
    
#if !TARGET_OS_EMBEDDED
    if ((!_consumerNub && _keyboardNub) || (!_keyboardNub && _consumerNub)) {
        OSDictionary * matchingDictionary = IOService::serviceMatching( "IOHIDEventService" );
        if ( matchingDictionary ) {
            OSDictionary *      propertyMatch = OSDictionary::withCapacity(4);
            
            if (propertyMatch) {
                OSObject *          object;
                object = copyProperty(kIOHIDTransportKey);
                if (object) propertyMatch->setObject(kIOHIDTransportKey, object);
                OSSafeReleaseNULL(object);
                
                object = copyProperty(kIOHIDVendorIDKey);
                if (object) propertyMatch->setObject(kIOHIDVendorIDKey, object);
                OSSafeReleaseNULL(object);
                
                object = copyProperty(kIOHIDProductIDKey);
                if (object) propertyMatch->setObject(kIOHIDProductIDKey, object);
                OSSafeReleaseNULL(object);
                
                object = copyProperty(kIOHIDLocationIDKey);
                if (object) propertyMatch->setObject(kIOHIDLocationIDKey, object);
                OSSafeReleaseNULL(object);
                
                matchingDictionary->setObject(gIOPropertyMatchKey, propertyMatch);
                
                propertyMatch->release();
            }
            _publishNotify = addMatchingNotification( gIOPublishNotification,
                             matchingDictionary,
                             &IOHIDEventService::_publishMatchingNotificationHandler,
                             this, 0 );
            matchingDictionary->release();
        }
    }
#endif /* TARGET_OS_EMBEDDED */
    
    _readyForInputReports = true;
    
    registerService(kIOServiceSynchronous);
    
    return true;
}

#if !TARGET_OS_EMBEDDED

//====================================================================================================
// stopAndReleaseShim
//====================================================================================================

static void stopAndReleaseShim ( IOService * service, IOService * provider )
{
    if ( !service )
        return;

    IOService * serviceProvider = service->getProvider();
        
    if ( serviceProvider == provider )
    {
        service->stop(provider);
        service->detach(provider);
    }
    service->release();
}

#endif /* TARGET_OS_EMBEDDED */

//====================================================================================================
// IOHIDEventService::stop
//====================================================================================================
void IOHIDEventService::stop( IOService * provider )
{
    handleStop ( provider );

    if (_capsTimerEventSource) {
        _capsTimerEventSource->cancelTimeout();
        if ( _workLoop )
            _workLoop->removeEventSource(_capsTimerEventSource);
        
        _capsTimerEventSource->release();
        _capsTimerEventSource = 0;
    }
    
    if (_ejectTimerEventSource) {
        _ejectTimerEventSource->cancelTimeout();
        if ( _workLoop )
            _workLoop->removeEventSource(_ejectTimerEventSource);
        
        _ejectTimerEventSource->release();
        _ejectTimerEventSource = 0;
    }

    if (_commandGate) {
        if ( _workLoop )
            _workLoop->removeEventSource(_commandGate);
        
        _commandGate->release();
        _commandGate = 0;
    }
    
#if TARGET_OS_EMBEDDED
    
    if ( _debuggerTimerEventSource )
        _debuggerTimerEventSource->cancelTimeout();
    
#else

    NUB_LOCK;

    stopAndReleaseShim ( _keyboardNub, this );
    _keyboardNub = 0;

    stopAndReleaseShim ( _pointingNub, this );
    _pointingNub = 0;

    stopAndReleaseShim ( _consumerNub, this );
    _consumerNub = 0;

    if (_publishNotify) {
        _publishNotify->remove();
    	_publishNotify = 0;
    }

    NUB_UNLOCK;
#endif /* TARGET_OS_EMBEDDED */
    
    super::stop( provider );
}

//====================================================================================================
// IOHIDEventService::matchPropertyTable
//====================================================================================================
bool IOHIDEventService::matchPropertyTable(OSDictionary * table, SInt32 * score)
{
    // Ask our superclass' opinion.
    if (super::matchPropertyTable(table, score) == false)  
        return false;

    return MatchPropertyTable(this, table, score);
}

//====================================================================================================
// IOHIDEventService::_publishMatchingNotificationHandler
//====================================================================================================
bool IOHIDEventService::_publishMatchingNotificationHandler(
    void * target,
    void * /* ref */,
    IOService * newService,
    IONotifier * /* notifier */)
{
#if !TARGET_OS_EMBEDDED
    IOHIDEventService * self    = (IOHIDEventService *) target;
    IOHIDEventService * service = (IOHIDEventService *) newService;
    
    IORecursiveLockLock(self->_nubLock);
    
    if ( service->_keyboardNub ) {
        if ( self->_keyboardNub
                && self->_keyboardNub->isDispatcher()
                && !service->_keyboardNub->isDispatcher() ) {
            stopAndReleaseShim ( self->_keyboardNub, self );
            self->_keyboardNub = 0;
        }
        
        if ( !self->_keyboardNub ) {
            self->_keyboardNub = service->_keyboardNub;
            self->_keyboardNub->retain();
            
            if (self->_publishNotify) {
                self->_publishNotify->remove();
                self->_publishNotify = 0;
            }
        }
    }
    
    if ( service->_consumerNub ) {
        if ( self->_consumerNub
                && self->_consumerNub->isDispatcher()
                && !service->_consumerNub->isDispatcher() ) {
            stopAndReleaseShim ( self->_consumerNub, self );
            self->_consumerNub = 0;
        }
        
        if ( !self->_consumerNub ) {
            self->_consumerNub = service->_consumerNub;
            self->_consumerNub->retain();
            
            if (self->_publishNotify) {
                self->_publishNotify->remove();
                self->_publishNotify = 0;
            }
        }
    }
    
    IORecursiveLockUnlock(self->_nubLock);
#endif /* TARGET_OS_EMBEDDED */
    return true;
}

//====================================================================================================
// IOHIDEventService::calculateCapsLockDelay
//====================================================================================================
void IOHIDEventService::calculateCapsLockDelay()
{
    OSNumber        *delay = NULL;
    OSNumber        *delayOverride = NULL;
    OSDictionary    *deviceParameters = NULL;
    OSArray         *mappings = NULL;
    UInt32          count = 0;
    
    // default to no delay
    _capsDelayMS = 0;
    
    // If this keyboard does not support delay, get out. Otherwise, use it.
    delay = (OSNumber*)copyProperty(kIOHIDKeyboardCapsLockDelay);
    if (!OSDynamicCast(OSNumber, delay)) 
        goto GET_OUT;
    _capsDelayMS = delay->unsigned32BitValue();
    
    // If there is an override in place, use that.
    
    delayOverride = (OSNumber*)copyProperty(kIOHIDKeyboardCapsLockDelayOverride);
    if (OSDynamicCast(OSNumber, delayOverride))
        _capsDelayMS = delayOverride->unsigned32BitValue();
    OSSafeReleaseNULL(delayOverride);
    
    // If there is no delay at this point, get out.
    if (!_capsDelayMS) 
        goto GET_OUT;
    
    // At this point, we need to scan all of the modifier mappings (if any) to see
    // if the NX_MODIFIERKEY_ALPHALOCK is remapped to something other than the
    // NX_MODIFIERKEY_ALPHALOCK.
    deviceParameters = (OSDictionary*)copyProperty(kIOHIDEventServicePropertiesKey);
    if (!OSDynamicCast(OSDictionary, deviceParameters)) 
        goto GET_OUT;
    
    mappings = OSDynamicCast(OSArray, deviceParameters->getObject(kIOHIDKeyboardModifierMappingPairsKey));
    if (!mappings) goto GET_OUT;
    
    count = mappings->getCount();
    if ( count ) {
        for ( unsigned i=0; i < count; i++ ) {
            OSDictionary    *pair   = OSDynamicCast(OSDictionary, mappings->getObject(i));
            OSNumber        *number         = NULL;
            SInt32   src    = 0;
            SInt32   dst    = 0;
            
            if ( !pair ) continue;
            
            number = OSDynamicCast(OSNumber, pair->getObject(kIOHIDKeyboardModifierMappingSrcKey));
            
            if ( !number ) continue;
            
            src = number->unsigned32BitValue();
            
            if (src != NX_MODIFIERKEY_ALPHALOCK) continue;
            
            number = OSDynamicCast(OSNumber, pair->getObject(kIOHIDKeyboardModifierMappingDstKey));
            
            if ( !number ) continue;
            
            dst = number->unsigned32BitValue();
            
            if (dst == NX_MODIFIERKEY_ALPHALOCK) continue;
            
            // NX_MODIFIERKEY_ALPHALOCK is remapped. Set delay to 0 and get out.
            _capsDelayMS = 0;
            goto GET_OUT;
        }
    }
    
GET_OUT:
    OSSafeReleaseNULL(deviceParameters);
    OSSafeReleaseNULL(delay);
    IOHID_DEBUG(kIOHIDDebugCode_CalculatedCapsDelay, _capsDelayMS, 0, 0, 0);
}

//====================================================================================================
// IOHIDEventService::calculateStandardType
//====================================================================================================
void IOHIDEventService::calculateStandardType()
{
    IOHIDStandardType   result = kIOHIDStandardTypeANSI;
    OSNumber *          number;

	number = (OSNumber*)copyProperty(kIOHIDStandardTypeKey);
	if ( OSDynamicCast(OSNumber, number) ) {
		result = number->unsigned32BitValue();
	}
	else {
		OSSafeReleaseNULL(number);
		UInt16 productID    = getProductID();
		UInt16 vendorID     = getVendorID();
		
		if (vendorID == kIOUSBVendorIDAppleComputer) {
			
			switch (productID) {
				case kprodUSBCosmoISOKbd:  //Cosmo ISO
				case kprodUSBAndyISOKbd:  //Andy ISO
				case kprodQ6ISOKbd:  //Q6 ISO
				case kprodQ30ISOKbd:  //Q30 ISO
#if TARGET_OS_EMBEDDED
					_shouldSwapISO = true;
#endif /* TARGET_OS_EMBEDDED */
					// fall through
				case kprodFountainISOKbd:  //Fountain ISO
				case kprodSantaISOKbd:  //Santa ISO
					result = kIOHIDStandardTypeISO;
					break;
				case kprodUSBCosmoJISKbd:  //Cosmo JIS
				case kprodUSBAndyJISKbd:  //Andy JIS is 0x206
				case kprodQ6JISKbd:  //Q6 JIS
				case kprodQ30JISKbd:  //Q30 JIS
				case kprodFountainJISKbd:  //Fountain JIS
				case kprodSantaJISKbd:  //Santa JIS
					result = kIOHIDStandardTypeJIS;
					break;
			}
			
			setProperty(kIOHIDStandardTypeKey, result, 32);
		}
	}
    OSSafeReleaseNULL(number);
    
#if TARGET_OS_EMBEDDED
    if ( !_shouldSwapISO && result == kIOHIDStandardTypeISO ) {
        number = (OSNumber*)copyProperty("alt_handler_id");
        if ( OSDynamicCast(OSNumber, number) ) {
            switch (number->unsigned32BitValue()) {
                case kgestUSBCosmoISOKbd: 
                case kgestUSBAndyISOKbd: 
                case kgestQ6ISOKbd: 
                case kgestQ30ISOKbd: 
                case kgestM89ISOKbd:
                case kgestUSBGenericISOkd: 
                    _shouldSwapISO = true;
                    break;
            }
        }
        OSSafeReleaseNULL(number);
    }
#endif /* TARGET_OS_EMBEDDED */
}

//====================================================================================================
// IOHIDEventService::setSystemProperties
//====================================================================================================
IOReturn IOHIDEventService::setSystemProperties( OSDictionary * properties )
{
    OSDictionary *  dict        = NULL;
    OSArray *       array       = NULL;
    OSNumber *      number      = NULL;
    bool            setCapsDelay= false;
    
    if ( !properties )
        return kIOReturnBadArgument;
        
    if ( properties->getObject(kIOHIDDeviceParametersKey) != kOSBooleanTrue ) {
        OSDictionary * propsCopy = OSDictionary::withDictionary(properties);
        if ( propsCopy ) {
            propsCopy->setObject(kIOHIDEventServicePropertiesKey, kOSBooleanTrue);
            
#if !TARGET_OS_EMBEDDED
            if ( _keyboardNub )
                _keyboardNub->setParamProperties(properties);
                
            if ( _pointingNub )
                _pointingNub->setParamProperties(properties);
                
            if ( _consumerNub )
                _consumerNub->setParamProperties(properties);
#endif
            propsCopy->release();
        }
    }
    
    number = OSDynamicCast(OSNumber, properties->getObject(kIOHIDKeyboardCapsLockDelayOverride));
    if (number) {
        setProperty(kIOHIDKeyboardCapsLockDelayOverride, number);
        setCapsDelay = true;
    }
    
    if ( ( array = OSDynamicCast(OSArray, properties->getObject(kIOHIDKeyboardModifierMappingPairsKey)) ) ) {
        UInt32  srcVirtualCode, dstVirtualCode;
        Boolean capsMap = FALSE;
        
        for (UInt32 index=0; index<array->getCount(); index++) {
        
            dict = OSDynamicCast(OSDictionary, array->getObject(index));
            if ( !dict )
                continue;
                
            number = OSDynamicCast(OSNumber, dict->getObject(kIOHIDKeyboardModifierMappingSrcKey));
            if ( !number )
                continue;
                
            srcVirtualCode = number->unsigned32BitValue();
            if ( srcVirtualCode != NX_MODIFIERKEY_ALPHALOCK )
                continue;
                
            number = OSDynamicCast(OSNumber, dict->getObject(kIOHIDKeyboardModifierMappingDstKey));
            if ( !number )
                continue;
                
            dstVirtualCode = number->unsigned32BitValue();
            if ( dstVirtualCode == srcVirtualCode )
                continue;
                
            capsMap = TRUE;
            
            break;
        }
        
        if ( capsMap ) {
            // Clear out the delay
            _capsDelayMS = 0;
            setCapsDelay = false;
        }
        else if ( !_capsDelayMS ) {
            setCapsDelay = true;
        }
    }
    
    if (setCapsDelay)
        calculateCapsLockDelay();
        
    if ( properties->getObject(kIOHIDDeviceParametersKey) == kOSBooleanTrue ) {
        OSDictionary * eventServiceProperties = (OSDictionary*)copyProperty(kIOHIDEventServicePropertiesKey);
        if ( OSDynamicCast(OSDictionary, eventServiceProperties) ) {
            if (eventServiceProperties->setOptions(0, 0) & OSDictionary::kImmutable) {
                OSDictionary * temp = eventServiceProperties;
                eventServiceProperties = OSDynamicCast(OSDictionary, temp->copyCollection());
                temp->release();
            }
            else {
                // do nothing
            }
        }
        else {
            OSSafeReleaseNULL(eventServiceProperties);
            eventServiceProperties = OSDictionary::withCapacity(4);
        }
        
        if ( eventServiceProperties ) {
            eventServiceProperties->merge(properties);
            eventServiceProperties->removeObject(kIOHIDResetKeyboardKey);
            eventServiceProperties->removeObject(kIOHIDResetPointerKey);
            eventServiceProperties->removeObject(kIOHIDDeviceParametersKey);
            
            setProperty(kIOHIDEventServicePropertiesKey, eventServiceProperties);
            eventServiceProperties->release();
        }
    }
    
    return kIOReturnSuccess;
}

//====================================================================================================
// IOHIDEventService::setProperties
//====================================================================================================
IOReturn IOHIDEventService::setProperties( OSObject * properties )
{
    OSDictionary *  propertyDict    = OSDynamicCast(OSDictionary, properties);
    IOReturn        ret             = kIOReturnBadArgument;
    
    if ( propertyDict ) {
        propertyDict->setObject(kIOHIDDeviceParametersKey, kOSBooleanTrue);
        ret = setSystemProperties( propertyDict );
        propertyDict->removeObject(kIOHIDDeviceParametersKey);
    }
    
    return ret;
}


//====================================================================================================
// IOHIDEventService::parseSupportedElements
//====================================================================================================
void IOHIDEventService::parseSupportedElements ( OSArray * elementArray, UInt32 bootProtocol )
{
    UInt32              count               = 0;
    UInt32              index               = 0;
    UInt32              usage               = 0;
    UInt32              usagePage           = 0;
    UInt32              supportedModifiers  = 0;
    UInt32              buttonCount         = 0;
    IOHIDElement *      element             = 0;
    OSArray *           functions           = 0;
    IOFixed             pointingResolution  = 0;
    IOFixed             scrollResolution    = 0;
    bool                pointingDevice      = false;
    bool                keyboardDevice      = false;
    bool                consumerDevice      = false;
    
    switch ( bootProtocol )
    {
        case kBootProtocolMouse:
            pointingDevice = true;
            break;
        case kBootProtocolKeyboard:
            keyboardDevice = true;
            break;
    }
    
    if ( elementArray )
    {        
        count = elementArray->getCount();
        
        for ( index = 0; index < count; index++ )
        {
            element = OSDynamicCast(IOHIDElement, elementArray->getObject(index));
            
            if ( !element )
                continue;
            
            usagePage   = element->getUsagePage();
            usage       = element->getUsage();

            switch ( usagePage )
            {
                case kHIDPage_GenericDesktop:
                    switch ( usage )
                    {
                        case kHIDUsage_GD_Mouse:
                            pointingDevice      = true;
                            break;
                        case kHIDUsage_GD_X:
                            if ( !(pointingResolution = determineResolution(element)) )
                                pointingResolution = kDefaultFixedResolution;
                        case kHIDUsage_GD_Y:
                        case kHIDUsage_GD_Z:
                            if ((element->getFlags() & kIOHIDElementFlagsRelativeMask) == 0)
                            {
                                processTabletElement ( element );
                            }
                            break;
                        case kHIDUsage_GD_Wheel:
                            if ( !(scrollResolution = determineResolution(element)) )
                                scrollResolution = kDefaultScrollFixedResolution;
                            break;
                        case kHIDUsage_GD_SystemPowerDown:
                        case kHIDUsage_GD_SystemSleep:
                        case kHIDUsage_GD_SystemWakeUp:
                            consumerDevice      = true;
                            break;
                    }
                    break;
                    
                case kHIDPage_Button:
                    processTabletElement ( element );
                    buttonCount ++;
                    break;
                
                case kHIDPage_KeyboardOrKeypad:
                    keyboardDevice = true;
                    switch ( usage )
                    {
                        case kHIDUsage_KeyboardLeftControl:
                            supportedModifiers |= NX_CONTROLMASK;
                            supportedModifiers |= NX_DEVICELCTLKEYMASK;
                            break;
                        case kHIDUsage_KeyboardLeftShift:
                            supportedModifiers |= NX_SHIFTMASK;
                            supportedModifiers |= NX_DEVICELSHIFTKEYMASK;
                            break;
                        case kHIDUsage_KeyboardLeftAlt:
                            supportedModifiers |= NX_ALTERNATEMASK;
                            supportedModifiers |= NX_DEVICELALTKEYMASK;
                            break;
                        case kHIDUsage_KeyboardLeftGUI:
                            supportedModifiers |= NX_COMMANDMASK;
                            supportedModifiers |= NX_DEVICELCMDKEYMASK;
                            break;
                        case kHIDUsage_KeyboardRightControl:
                            supportedModifiers |= NX_CONTROLMASK;
                            supportedModifiers |= NX_DEVICERCTLKEYMASK;
                            break;
                        case kHIDUsage_KeyboardRightShift:
                            supportedModifiers |= NX_SHIFTMASK;
                            supportedModifiers |= NX_DEVICERSHIFTKEYMASK;
                            break;
                        case kHIDUsage_KeyboardRightAlt:
                            supportedModifiers |= NX_ALTERNATEMASK;
                            supportedModifiers |= NX_DEVICERALTKEYMASK;
                            break;
                        case kHIDUsage_KeyboardRightGUI:
                            supportedModifiers |= NX_COMMANDMASK;
                            supportedModifiers |= NX_DEVICERCMDKEYMASK;
                            break;
                    }
                    break;
                
                case kHIDPage_Consumer:
                    consumerDevice = true;
                    break;
                case kHIDPage_Digitizer:
                    pointingDevice = true;
                    switch ( usage )
                    {
                        case kHIDUsage_Dig_TipSwitch:
                        case kHIDUsage_Dig_BarrelSwitch:
                        case kHIDUsage_Dig_Eraser:
                            buttonCount ++;
                        default:
                             processTabletElement ( element );
                            break;
                    }
                    break;
                case kHIDPage_AppleVendorTopCase:
                    if ((getVendorID() == kIOUSBVendorIDAppleComputer) && 
                        (usage == kHIDUsage_AV_TopCase_KeyboardFn))
                    {
                        supportedModifiers |= NX_SECONDARYFNMASK;
                    }
                    break;
            }
            
            // Cache device functions
            if ((element->getType() == kIOHIDElementTypeCollection) &&
                ((element->getCollectionType() == kIOHIDElementCollectionTypeApplication) ||
                (element->getCollectionType() == kIOHIDElementCollectionTypePhysical)))
            {
                OSNumber * usagePageRef, * usageRef;
                OSDictionary * pairRef;
                
                if(!functions) functions = OSArray::withCapacity(2);
                
                pairRef     = OSDictionary::withCapacity(2);
                usageRef    = OSNumber::withNumber(usage, 32);
                usagePageRef= OSNumber::withNumber(usagePage, 32);
                
                pairRef->setObject(kIOHIDDeviceUsageKey, usageRef);
                pairRef->setObject(kIOHIDDeviceUsagePageKey, usagePageRef);
                
                UInt32 	pairCount = functions->getCount();
                bool 	found = false;
                for(unsigned i=0; i<pairCount; i++)
                {
                    OSDictionary *tempPair = (OSDictionary *)functions->getObject(i);
                    
                    if ( NULL != (found = tempPair->isEqualTo(pairRef)) )
                        break;
                }
                
                if (!found) 
                {
                    functions->setObject(functions->getCount(), pairRef);                
                }
                
                pairRef->release();
                usageRef->release();
                usagePageRef->release();
            }
        }
        
        _deviceUsagePairs = functions;
    }
    
    processTransducerData();
    
    if ( pointingDevice )
    {
		_pointingNub = newPointingShim(buttonCount, pointingResolution, scrollResolution, kShimEventProcessor);
	}
    if ( keyboardDevice )
    {
        _keyboardNub = newKeyboardShim(supportedModifiers, kShimEventProcessor);
	}
    if ( consumerDevice )
    {
		_consumerNub = newConsumerShim(kShimEventProcessor);
    }    
}

//====================================================================================================
// IOHIDEventService::processTabletElement
//====================================================================================================
void IOHIDEventService::processTabletElement ( IOHIDElement * element )
{
    TransducerData *    transducerRef;
    IOHIDElement *      parent;
    
    transducerRef = getTransducerData(element->getReportID());
    
    if ( !transducerRef )
    {
        transducerRef = createTransducerData ( element->getReportID() );
    }
    
    if ( element->getUsagePage() == kHIDPage_Digitizer )
    {
        switch (element->getUsage())
        {
            case kHIDUsage_Dig_Stylus:
                transducerRef->type         = NX_TABLET_POINTER_PEN;
                break;
            case kHIDUsage_Dig_Puck:
                transducerRef->type         = NX_TABLET_POINTER_CURSOR;
                break;
            case kHIDUsage_Dig_XTilt:
                transducerRef->capabilities |= NX_TABLET_CAPABILITY_TILTXMASK;
                break;
            case kHIDUsage_Dig_YTilt:
                transducerRef->capabilities |= NX_TABLET_CAPABILITY_TILTYMASK;
                break;
            case kHIDUsage_Dig_TipPressure:
                if ( transducerRef->type == NX_TABLET_POINTER_UNKNOWN )
                    transducerRef->type = NX_TABLET_POINTER_PEN;
                    
                transducerRef->capabilities |= NX_TABLET_CAPABILITY_PRESSUREMASK;
                break;
            case kHIDUsage_Dig_BarrelPressure:
                if ( transducerRef->type == NX_TABLET_POINTER_UNKNOWN )
                    transducerRef->type = NX_TABLET_POINTER_PEN;
                    
                transducerRef->capabilities |= NX_TABLET_CAPABILITY_TANGENTIALPRESSUREMASK;
                break;
            case kHIDUsage_Dig_Twist:
                transducerRef->capabilities |= NX_TABLET_CAPABILITY_ROTATIONMASK;
                break;
            case kHIDUsage_Dig_TransducerIndex:
                transducerRef->supportsTransducerIndex = true;
                break;
            case kHIDUsage_Dig_Eraser:
                if ( transducerRef->type == NX_TABLET_POINTER_UNKNOWN )
                    transducerRef->type = NX_TABLET_POINTER_ERASER;
            case kHIDUsage_Dig_TipSwitch:
            case kHIDUsage_Dig_BarrelSwitch:
                if ( transducerRef->type == NX_TABLET_POINTER_UNKNOWN )
                    transducerRef->type = NX_TABLET_POINTER_PEN;
                    
                transducerRef->capabilities |= NX_TABLET_CAPABILITY_BUTTONSMASK;
                break;
        }
    }
    else if ( element->getUsagePage() == kHIDPage_GenericDesktop )
    {
        switch (element->getUsage())
        {
            case kHIDUsage_GD_X:
                transducerRef->capabilities |= NX_TABLET_CAPABILITY_ABSXMASK;
                break;
            case kHIDUsage_GD_Y:
                transducerRef->capabilities |= NX_TABLET_CAPABILITY_ABSYMASK;
                break;
            case kHIDUsage_GD_Z:
                transducerRef->capabilities |= NX_TABLET_CAPABILITY_ABSZMASK;
                break;
        }
    }
    else if ( element->getUsagePage() == kHIDPage_Button )
    {
        transducerRef->capabilities |= NX_TABLET_CAPABILITY_BUTTONSMASK;
    }
    
    if ( element->getType() != kIOHIDElementTypeCollection )
    {
        parent = element->getParentElement();
        
        if ( parent && ( parent->getUsagePage() == kHIDPage_Digitizer ))
        {
            transducerRef->digitizerCollection = true;    
        }
    }
}

//====================================================================================================
// IOHIDEventService::createTransducerData
//====================================================================================================
TransducerData * IOHIDEventService::createTransducerData ( UInt32 tranducerID )
{
    TransducerData      temp;
    OSData *            data;
    
    if ( ! _transducerDataArray )
        _transducerDataArray = OSArray::withCapacity(4);

    bzero(&temp, sizeof(TransducerData));
    
    temp.reportID   = tranducerID;
    temp.type       = NX_TABLET_POINTER_UNKNOWN;

    data            = OSData::withBytes(&temp, sizeof(TransducerData));
    
    _transducerDataArray->setObject(data);
    data->release();
    
    return (data) ? (TransducerData *)data->getBytesNoCopy() : 0;
}

//====================================================================================================
// IOHIDEventService::getTransducerData
//====================================================================================================
TransducerData * IOHIDEventService::getTransducerData ( UInt32 tranducerID )
{
    TransducerData *    transducerRef       = 0;
    TransducerData *    transducerIndexRef  = 0;
    OSData *            data                = 0;
    bool                found               = 0;
    
    if ( !_transducerDataArray )
        return NULL;

    UInt32  count = _transducerDataArray->getCount();
    
    for (unsigned i=0; i<count; i++)
    {
        data = (OSData *)_transducerDataArray->getObject(i);
        
        if (!data) continue;
        
        transducerRef = (TransducerData *)data->getBytesNoCopy();
        
        if (!transducerRef) continue;
        
        if (transducerRef->supportsTransducerIndex)
            transducerIndexRef = transducerRef;
        
        if (transducerRef->reportID != tranducerID) continue;
        
        found = true;
        break;
    }
        
    return ( found ) ? transducerRef : transducerIndexRef;
}

//====================================================================================================
// IOHIDEventService::processTransducerData
//====================================================================================================
void IOHIDEventService::processTransducerData ()
{
#if !TARGET_OS_EMBEDDED
    TransducerData *    transducerRef;
    OSData *            data;
    
    if ( ! _transducerDataArray )
        return;
        
    for (unsigned i=0; i<_transducerDataArray->getCount(); i++)
    {
        data = (OSData *)_transducerDataArray->getObject(i);
        
        if (!data) continue;
        
        transducerRef = (TransducerData *)data->getBytesNoCopy();
        
        if ( (transducerRef->capabilities & ~NX_TABLET_CAPABILITY_BUTTONSMASK) || 
             (transducerRef->digitizerCollection))
        {
            transducerRef->deviceID     = IOHIDPointing::generateDeviceID();
            transducerRef->capabilities |= NX_TABLET_CAPABILITY_DEVICEIDMASK;
        }
        else 
        {
            _transducerDataArray->removeObject(i--);
        }
    }
    
    if ( _transducerDataArray->getCount() == 0 )
    {
        _transducerDataArray->release();
        _transducerDataArray = 0;
    }
#endif /* TARGET_OS_EMBEDDED */
}

//====================================================================================================
// IOHIDEventService::newPointingShim
//====================================================================================================
IOHIDPointing * IOHIDEventService::newPointingShim (
                            UInt32          buttonCount,
                            IOFixed         pointerResolution,
                            IOFixed         scrollResolution,
                            IOOptionBits    options)
{
    IOHIDPointing * pointingNub = NULL;
#if !TARGET_OS_EMBEDDED
    bool            isDispatcher = ((options & kShimEventProcessor) == 0);

    pointingNub = IOHIDPointing::Pointing(buttonCount, pointerResolution, scrollResolution, isDispatcher);
    if (pointingNub) {
        SET_HID_PROPERTIES(pointingNub);    
     
        if (!pointingNub->attach(this) || !pointingNub->start(this)) 
        {
            pointingNub->release();
            pointingNub = 0;
        }
    }
#endif /* TARGET_OS_EMBEDDED */
    return pointingNub;
}
                                
//====================================================================================================
// IOHIDEventService::newKeyboardShim
//====================================================================================================
IOHIDKeyboard * IOHIDEventService::newKeyboardShim (
                                UInt32          supportedModifiers,
                                IOOptionBits    options)
{
    IOHIDKeyboard * keyboardNub = NULL;
#if !TARGET_OS_EMBEDDED
    bool            isDispatcher = ((options & kShimEventProcessor) == 0);
    
    keyboardNub = IOHIDKeyboard::Keyboard(supportedModifiers, isDispatcher);
    if (keyboardNub) {
        SET_HID_PROPERTIES(keyboardNub);    
     
        if (!keyboardNub->attach(this) || !keyboardNub->start(this)) 
        {
            keyboardNub->release();
            keyboardNub = 0;
        }
    }
#endif    
    return keyboardNub;
}

//====================================================================================================
// IOHIDEventService::newConsumerShim
//====================================================================================================
IOHIDConsumer * IOHIDEventService::newConsumerShim ( IOOptionBits options )
{
    IOHIDConsumer * consumerNub = NULL;
#if !TARGET_OS_EMBEDDED
    bool            isDispatcher = ((options & kShimEventProcessor) == 0);

    consumerNub = IOHIDConsumer::Consumer(isDispatcher);
    if (consumerNub) {
        SET_HID_PROPERTIES(consumerNub);    
     
        if (!consumerNub->attach(this) || !consumerNub->start(this)) 
        {
            consumerNub->release();
            consumerNub = 0;
        }
    }
#endif    
    return consumerNub;
}

//====================================================================================================
// IOHIDEventService::determineResolution
//====================================================================================================
IOFixed IOHIDEventService::determineResolution ( IOHIDElement * element )
{
    IOFixed resolution = 0;
    
    if ((element->getPhysicalMin() != element->getLogicalMin()) &&
        (element->getPhysicalMax() != element->getLogicalMax()))
    {
        SInt32 logicalDiff = (element->getLogicalMax() - element->getLogicalMin());
        SInt32 physicalDiff = (element->getPhysicalMax() - element->getPhysicalMin());
        
        // Since IOFixedDivide truncated fractional part and can't use floating point
        // within the kernel, have to convert equation when using negative exponents:
        // _resolution = ((logMax -logMin) * 10 **(-exp))/(physMax -physMin)

        // Even though unitExponent is stored as SInt32, The real values are only
        // a signed nibble that doesn't expand to the full 32 bits.
        SInt32 resExponent = element->getUnitExponent() & 0x0F;
        
        if (resExponent < 8)
        {
            for (int i = resExponent; i > 0; i--)
            {
                physicalDiff *=  10;
            }
        }
        else
        {
            for (int i = 0x10 - resExponent; i > 0; i--)
            {
                logicalDiff *= 10;
            }
        }
        resolution = (logicalDiff / physicalDiff) << 16;
    }
    
    return resolution;
}

//====================================================================================================
// IOHIDEventService::free
//====================================================================================================
void IOHIDEventService::free()
{
    IORecursiveLock* tempLock = NULL;
    
    if ( _nubLock ) {
        IORecursiveLockLock(_nubLock);
        tempLock = _nubLock;
        _nubLock = NULL;
    }
    
    if ( _transducerDataArray ) {
        _transducerDataArray->release();
        _transducerDataArray = 0;
    }
    
    if ( _transducerDataArray ) {
        _transducerDataArray->release();
        _transducerDataArray = 0;
    }
    
    if (_ejectTimerEventSource) {
        if ( _workLoop )
            _workLoop->removeEventSource(_ejectTimerEventSource);
            
        _ejectTimerEventSource->release();
        _ejectTimerEventSource = 0;
    }
    
    if (_commandGate) {
        if ( _workLoop )
            _workLoop->removeEventSource(_commandGate);
        
        _commandGate->release();
        _commandGate = 0;
    }
    
    if (_capsTimerEventSource) {
        if ( _workLoop )
            _workLoop->removeEventSource(_capsTimerEventSource);
            
        _capsTimerEventSource->release();
        _capsTimerEventSource = 0;
    }
    
#if TARGET_OS_EMBEDDED
    if ( _deviceUsagePairs ) {
        _deviceUsagePairs->release();
        _deviceUsagePairs = NULL;
    }
    
    if ( _clientDict ) {
        assert(_clientDict->getCount() == 0);
        _clientDict->release();
        _clientDict = NULL;
    }

    if (_debuggerTimerEventSource) {
        if ( _workLoop )
            _workLoop->removeEventSource(_debuggerTimerEventSource);
        
        _debuggerTimerEventSource->release();
        _debuggerTimerEventSource = 0;
    }

#endif /* TARGET_OS_EMBEDDED */

    if ( _workLoop ) {
        // not our workloop. don't stop it.
        _workLoop->release();
        _workLoop = NULL;
    }
        
    if (_reserved) {
        IODelete(_reserved, ExpansionData, 1);
        _reserved = NULL;
    }
    
    if ( tempLock ) {
        IORecursiveLockUnlock(tempLock);
        IORecursiveLockFree(tempLock);
    }
    
    super::free();
}

//==============================================================================
// IOHIDEventService::handleOpen
//==============================================================================
bool IOHIDEventService::handleOpen(IOService *  client,
                                    IOOptionBits options,
                                    void *       argument)
{
#if TARGET_OS_EMBEDDED

    bool accept = false;
    do {
        // Was this object already registered as our client?

        if ( _clientDict->getObject((const OSSymbol *)client) ) {
            accept = true;
            break;
        }
        
        // Add the new client object to our client dict.
        if ( !OSDynamicCast(IOHIDClientData, (OSObject *)argument) || 
                !_clientDict->setObject((const OSSymbol *)client, (IOHIDClientData *)argument))
            break;
        
        accept = true;
    } while (false);
    
    return accept;
    
#else

    return super::handleOpen(client, options, argument);
    
#endif /* TARGET_OS_EMBEDDED */

}

//==============================================================================
// IOHIDEventService::handleClose
//==============================================================================
void IOHIDEventService::handleClose(IOService * client, IOOptionBits options)
{
#if TARGET_OS_EMBEDDED
    if ( _clientDict->getObject((const OSSymbol *)client) )
        _clientDict->removeObject((const OSSymbol *)client);
#else
    super::handleClose(client, options);
#endif /* TARGET_OS_EMBEDDED */
}

//==============================================================================
// IOHIDEventService::handleIsOpen
//==============================================================================
bool IOHIDEventService::handleIsOpen(const IOService * client) const
{
#if TARGET_OS_EMBEDDED
    if (client)
        return _clientDict->getObject((const OSSymbol *)client) != NULL;
    else
        return (_clientDict->getCount() > 0);
#else
    return super::handleIsOpen(client);
#endif /* TARGET_OS_EMBEDDED */
}

//====================================================================================================
// IOHIDEventService::handleStart
//====================================================================================================
bool IOHIDEventService::handleStart( IOService * provider __unused )
{
    return true;
}

//====================================================================================================
// IOHIDEventService::handleStop
//====================================================================================================
void IOHIDEventService::handleStop(  IOService * provider __unused )
{}

//====================================================================================================
// IOHIDEventService::getTransport
//====================================================================================================
OSString * IOHIDEventService::getTransport ()
{
    return _provider ? OSDynamicCast(OSString, _provider->getProperty(kIOHIDTransportKey)) : 0;
}

//====================================================================================================
// IOHIDEventService::getManufacturer
//====================================================================================================
OSString * IOHIDEventService::getManufacturer ()
{
    // vtn3: This is not safe, but I am unsure how to fix it
    return _provider ? OSDynamicCast(OSString, _provider->getProperty(kIOHIDManufacturerKey)) : 0;
}

//====================================================================================================
// IOHIDEventService::getProduct
//====================================================================================================
OSString * IOHIDEventService::getProduct ()
{
    // vtn3: This is not safe, but I am unsure how to fix it
    return _provider ? OSDynamicCast(OSString, _provider->getProperty(kIOHIDProductKey)) : 0;
}

//====================================================================================================
// IOHIDEventService::getSerialNumber
//====================================================================================================
OSString * IOHIDEventService::getSerialNumber ()
{
    // vtn3: This is not safe, but I am unsure how to fix it
    return _provider ? OSDynamicCast(OSString, _provider->getProperty(kIOHIDSerialNumberKey)) : 0;
}

//====================================================================================================
// IOHIDEventService::getLocationID
//====================================================================================================
UInt32 IOHIDEventService::getLocationID ()
{
	UInt32 value = 0;
	
	if ( _provider ) {
		OSNumber * number = (OSNumber*)_provider->copyProperty(kIOHIDSerialNumberKey);
		if ( OSDynamicCast(OSNumber, number) )
			value = number->unsigned32BitValue();
		OSSafeReleaseNULL(number);
	}
    return value;
}

//====================================================================================================
// IOHIDEventService::getVendorID
//====================================================================================================
UInt32 IOHIDEventService::getVendorID ()
{
	UInt32 value = 0;
	
	if ( _provider ) {
		OSNumber * number = (OSNumber*)_provider->copyProperty(kIOHIDVendorIDKey);
		if ( OSDynamicCast(OSNumber, number) )
			value = number->unsigned32BitValue();
		OSSafeReleaseNULL(number);
	}
    return value;
}

//====================================================================================================
// IOHIDEventService::getVendorIDSource
//====================================================================================================
UInt32 IOHIDEventService::getVendorIDSource ()
{
	UInt32 value = 0;
	
	if ( _provider ) {
		OSNumber * number = (OSNumber*)_provider->copyProperty(kIOHIDVendorIDSourceKey);
		if ( OSDynamicCast(OSNumber, number) )
			value = number->unsigned32BitValue();
		OSSafeReleaseNULL(number);			
	}
    return value;
}

//====================================================================================================
// IOHIDEventService::getProductID
//====================================================================================================
UInt32 IOHIDEventService::getProductID ()
{
	UInt32 value = 0;
	
	if ( _provider ) {
		OSNumber * number = (OSNumber*)_provider->copyProperty(kIOHIDProductIDKey);
		if ( OSDynamicCast(OSNumber, number) )
			value = number->unsigned32BitValue();
		OSSafeReleaseNULL(number);			
	}
    return value;
}

//====================================================================================================
// IOHIDEventService::getVersion
//====================================================================================================
UInt32 IOHIDEventService::getVersion ()
{
	UInt32 value = 0;
	
	if ( _provider ) {
		OSNumber * number = (OSNumber*)_provider->copyProperty(kIOHIDVersionNumberKey);
		if ( OSDynamicCast(OSNumber, number) )
			value = number->unsigned32BitValue();
		OSSafeReleaseNULL(number);			
	}
    return value;
}

//====================================================================================================
// IOHIDEventService::getCountryCode
//====================================================================================================
UInt32 IOHIDEventService::getCountryCode ()
{
	UInt32 value = 0;
	
	if ( _provider ) {
		OSNumber * number = (OSNumber*)_provider->copyProperty(kIOHIDCountryCodeKey);
		if ( OSDynamicCast(OSNumber, number) )
			value = number->unsigned32BitValue();
        OSSafeReleaseNULL(number);			
	}
    return value;
}

//====================================================================================================
// IOHIDEventService::getReportElements
//====================================================================================================
OSArray * IOHIDEventService::getReportElements()
{
    return 0;
}

//====================================================================================================
// IOHIDEventService::setElementValue
//====================================================================================================
void IOHIDEventService::setElementValue (
                                UInt32                      usagePage __unused,
                                UInt32                      usage __unused,
                                UInt32                      value __unused )
{
}

//====================================================================================================
// IOHIDEventService::getElementValue
//====================================================================================================
UInt32 IOHIDEventService::getElementValue ( 
                                UInt32                      usagePage __unused,
                                UInt32                      usage __unused )
{
    return 0;
}


//====================================================================================================
// ejectTimerCallback
//====================================================================================================
void IOHIDEventService::ejectTimerCallback(IOTimerEventSource *sender __unused)
{
    NUB_LOCK;
    IOHID_DEBUG(kIOHIDDebugCode_EjectCallback, _ejectState, 0, 0, 0);
    if ( _ejectState ) {
        AbsoluteTime timeStamp;
        
        clock_get_uptime(&timeStamp);
        
        dispatchKeyboardEvent(timeStamp, kHIDPage_Consumer, kHIDUsage_Csmr_Eject, 1, _ejectOptions | kDelayedOption);
        dispatchKeyboardEvent(timeStamp, kHIDPage_Consumer, kHIDUsage_Csmr_Eject, 0, _ejectOptions | kDelayedOption);
        
        _ejectState = 0;
    }
    NUB_UNLOCK;
}

//====================================================================================================
// capsTimerCallback
//====================================================================================================
void IOHIDEventService::capsTimerCallback(IOTimerEventSource *sender __unused)
{
    NUB_LOCK;
    IOHID_DEBUG(kIOHIDDebugCode_CapsCallback, _capsState, 0, 0, 0);
    if ( _capsState ) {
        AbsoluteTime timeStamp;
        
        clock_get_uptime(&timeStamp);
        
        dispatchKeyboardEvent(timeStamp, kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardCapsLock, 1, _capsOptions | kDelayedOption);
        dispatchKeyboardEvent(timeStamp, kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardCapsLock, 0, _capsOptions | kDelayedOption);
        
        _capsState = 0;
    }
    NUB_UNLOCK;
}


#if TARGET_OS_EMBEDDED
//==============================================================================
// IOHIDEventService::debuggerTimerCallback
//==============================================================================
void IOHIDEventService::debuggerTimerCallback(IOTimerEventSource *sender)
{
    NUB_LOCK;
    if ( _debuggerMask && _debuggerMask == _startDebuggerMask   )
        PE_enter_debugger("NMI");
    NUB_UNLOCK;
}
#endif /* TARGET_OS_EMBEDDED */

//====================================================================================================
// IOHIDEventService::dispatchKeyboardEvent
//====================================================================================================
void IOHIDEventService::dispatchKeyboardEvent(
                                AbsoluteTime                timeStamp,
                                UInt32                      usagePage,
                                UInt32                      usage,
                                UInt32                      value,
                                IOOptionBits                options)
{
    if ( ! _readyForInputReports )
        return;
        
    NUB_LOCK;

#if TARGET_OS_EMBEDDED // {
    IOHIDEvent * event = NULL;
    UInt32 debugMask = 0;
    
    switch (usagePage) {
        case kHIDPage_KeyboardOrKeypad:
            if ( _shouldSwapISO ) {

                switch ( usage ) {
                    case kHIDUsage_KeyboardGraveAccentAndTilde:
                        usage = kHIDUsage_KeyboardNonUSBackslash;
                        break;
                    case kHIDUsage_KeyboardNonUSBackslash:
                        usage = kHIDUsage_KeyboardGraveAccentAndTilde;
                        break;
                }
            }
            break;
        case kHIDPage_Consumer:
            switch (usage) {
                case kHIDUsage_Csmr_Power:
                    debugMask = 0x1;
                    break;
                case kHIDUsage_Csmr_VolumeIncrement:
                case kHIDUsage_Csmr_VolumeDecrement:
                    debugMask = 0x2;
                    break;
            };
            break;
        case kHIDPage_Telephony:
            switch (usage) {
                case kHIDUsage_Tfon_Hold:
                    debugMask = 0x1;
                    break;
            };
            break;
    };
    
    if ( value )
        _debuggerMask |= debugMask;
    else
        _debuggerMask &= ~debugMask;
        
    if ( _debuggerMask == 0x3) {
        if ( !_debuggerTimerEventSource ) {
            _debuggerTimerEventSource = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &IOHIDEventService::debuggerTimerCallback));
            if (_debuggerTimerEventSource) {
                if ((_workLoop->addEventSource(_debuggerTimerEventSource) != kIOReturnSuccess)) {
                    _debuggerTimerEventSource->release();
                    _debuggerTimerEventSource = NULL;
                }
            }
        }
        if ( _debuggerTimerEventSource ) {
            _debuggerTimerEventSource->setTimeoutMS( kDebuggerDelayMS );
            _startDebuggerMask = _debuggerMask;
        }
    }
    
    event = IOHIDEvent::keyboardEvent(timeStamp, usagePage, usage, value, options);
    if ( !event )
        return;
        
    dispatchEvent(event);
    
    event->release();
    
#else // } {
    
    IOHID_DEBUG(kIOHIDDebugCode_DispatchKeyboard, usagePage, usage, value, options);
    
    if ((( usagePage == kHIDPage_KeyboardOrKeypad ) &&
            (usage != kHIDUsage_KeyboardLockingNumLock) &&
            !(_capsDelayMS && (usage == kHIDUsage_KeyboardCapsLock))) ||
            ((getVendorID() == kIOUSBVendorIDAppleComputer) &&
             ((usagePage == kHIDPage_AppleVendorKeyboard) ||
              ((usagePage == kHIDPage_AppleVendorTopCase) &&
               (usage == kHIDUsage_AV_TopCase_KeyboardFn))))) {
        if ( !_keyboardNub )
            _keyboardNub = newKeyboardShim();
    
        if ( _keyboardNub )
            _keyboardNub->dispatchKeyboardEvent(timeStamp, usagePage, usage, (value != 0), options);
    
    }
    else {
        if ( !_consumerNub )
            _consumerNub = newConsumerShim();
    
        if ( _consumerNub ) {
            if ( (usagePage == kHIDPage_Consumer) && (usage == kHIDUsage_Csmr_Eject) && ((options & kDelayedOption) == 0) && _keyboardNub && ((_keyboardNub->eventFlags() & SPECIALKEYS_MODIFIER_MASK) == 0)) {
                if (( _ejectState != value ) && _ejectTimerEventSource) {
                    if ( value ) {
                        _ejectOptions       = options;
    
                        _ejectTimerEventSource->setTimeoutMS( _ejectDelayMS );
                    }
                    else {
                        _ejectTimerEventSource->cancelTimeout();
                    }
    
                   _ejectState = value;
                }
            }
            else  if (!(((options & kDelayedOption) == 0) && _ejectState && (usagePage == kHIDPage_Consumer) && (usage == kHIDUsage_Csmr_Eject))) {
                _consumerNub->dispatchConsumerEvent(_keyboardNub, timeStamp, usagePage, usage, value, options);
            }
        }
    
        if ( _capsDelayMS && (usagePage == kHIDPage_KeyboardOrKeypad) && (usage == kHIDUsage_KeyboardCapsLock)) {
            if ( (options & kDelayedOption) == 0) {
    
                if ( getElementValue(kHIDPage_LEDs, kHIDUsage_LED_CapsLock) == 0 ) {
                    if (( _capsState != value ) && _capsTimerEventSource ) {
                        if ( value ) {
                            _capsOptions       = options;
    
                            _capsTimerEventSource->setTimeoutMS( _capsDelayMS );
                        }
                        else {
                            _capsTimerEventSource->cancelTimeout();
                        }
    
                        _capsState = value;
                    }
                }
                else {
                    _keyboardNub->dispatchKeyboardEvent(timeStamp, usagePage, usage, value, options);
                }
            }
            else  if (!( ((options & kDelayedOption) == 0) && _capsState ) ) {
                _keyboardNub->dispatchKeyboardEvent(timeStamp, usagePage, usage, value, options);
            }
        }
    }
#endif /* TARGET_OS_EMBEDDED */ // }
    
    NUB_UNLOCK;
}


//====================================================================================================
// IOHIDEventService::dispatchRelativePointerEvent
//====================================================================================================
void IOHIDEventService::dispatchRelativePointerEvent(
                                AbsoluteTime                timeStamp,
                                SInt32                      dx,
                                SInt32                      dy,
                                UInt32                      buttonState,
                                IOOptionBits                options)
{
#if !TARGET_OS_EMBEDDED
    IOHID_DEBUG(kIOHIDDebugCode_DispatchRelativePointer, dx, dy, buttonState, options);
    
    if ( ! _readyForInputReports )
        return;
        
    NUB_LOCK;
    
    if ( !_pointingNub )
        _pointingNub = newPointingShim();
        
    if ( _pointingNub )
        _pointingNub->dispatchRelativePointerEvent(timeStamp, dx, dy, buttonState, options);
        
    NUB_UNLOCK;
#endif /* TARGET_OS_EMBEDDED */
}
    
//====================================================================================================
// IOHIDEventService::dispatchAbsolutePointerEvent
//====================================================================================================
void IOHIDEventService::dispatchAbsolutePointerEvent(
                                AbsoluteTime                timeStamp,
                                SInt32                      x,
                                SInt32                      y,
                                IOGBounds *                 bounds,
                                UInt32                      buttonState,
                                bool                        inRange,
                                SInt32                      tipPressure,
                                SInt32                      tipPressureMin,
                                SInt32                      tipPressureMax,
                                IOOptionBits                options)
{
#if TARGET_OS_EMBEDDED
    IOHID_DEBUG(kIOHIDDebugCode_DispatchAbsolutePointer, x, y, buttonState, options);
    IOHIDEvent *childEvent = IOHIDEvent::absolutePointerEvent(timeStamp, x, y, bounds, buttonState, inRange, tipPressure, tipPressureMin, tipPressureMax, options);
    
    if (!childEvent) {
        return;
    }
    
    SInt32 eventMask = 0;   // what's changed

    bool touch = (tipPressure > tipPressureMin);

    if (inRange) {
        childEvent->setIntegerValue(kIOHIDEventFieldDigitizerRange, TRUE, 0);
        
        if (!_reserved->previousRangeState) {
            _reserved->previousX = x;
            _reserved->previousY = y;
            eventMask |= kIOHIDDigitizerEventIdentity;
        }
        
        if ( (_reserved->previousX != x) || (_reserved->previousY != y) ) {
            eventMask |= kIOHIDDigitizerEventPosition;
        }

        if (touch) {
            childEvent->setIntegerValue(kIOHIDEventFieldDigitizerTouch, TRUE, 0);
        }

        if (touch != _reserved->prevousTouchState) {
            eventMask |= kIOHIDDigitizerEventTouch;
        }
    }

    if (inRange != _reserved->previousRangeState)
        eventMask |= kIOHIDDigitizerEventRange;
    
    _reserved->previousRangeState = inRange;
    _reserved->previousX = x;
    _reserved->previousY = y;
    _reserved->prevousTouchState = touch;
    
    
    childEvent->setIntegerValue(kIOHIDEventFieldDigitizerEventMask, eventMask, 0);

    
    IOHIDEvent *event = IOHIDEvent::absolutePointerEvent(timeStamp, x, y, bounds, buttonState, inRange, tipPressure, tipPressureMin, tipPressureMax, options);
    
    if (!event)
    {
        childEvent->release();
    }

    event->setIntegerValue(kIOHIDEventFieldDigitizerCollection, TRUE, 0);
    event->setIntegerValue(kIOHIDEventFieldDigitizerRange, childEvent->getIntegerValue(kIOHIDEventFieldDigitizerRange, 0), 0);
    event->setIntegerValue(kIOHIDEventFieldDigitizerEventMask, childEvent->getIntegerValue(kIOHIDEventFieldDigitizerEventMask, 0), 0);
    event->setIntegerValue(kIOHIDEventFieldDigitizerTouch, childEvent->getIntegerValue(kIOHIDEventFieldDigitizerTouch, 0), 0);

    event->appendChild(childEvent);
    childEvent->release();
    
    if ( event ) {
        dispatchEvent(event);
        event->release();
    }
#else
    IOHID_DEBUG(kIOHIDDebugCode_DispatchAbsolutePointer, x, y, buttonState, options);
    
    if ( ! _readyForInputReports )
        return;
        
    NUB_LOCK;
    
    if ( !_pointingNub )
        _pointingNub = newPointingShim();
        
    IOGPoint newLoc;
    
    newLoc.x = x;
    newLoc.y = y;
    
    _pointingNub->dispatchAbsolutePointerEvent(timeStamp, &newLoc, bounds, buttonState, inRange, tipPressure, tipPressureMin, tipPressureMax, options);
    
    NUB_UNLOCK;
#endif /* TARGET_OS_EMBEDDED */
    
}

//====================================================================================================
// IOHIDEventService::dispatchScrollWheelEvent
//====================================================================================================
void IOHIDEventService::dispatchScrollWheelEvent(
                                AbsoluteTime                timeStamp,
                                SInt32                      deltaAxis1,
                                SInt32                      deltaAxis2,
                                SInt32                      deltaAxis3,
                                IOOptionBits                options)
{
#if !TARGET_OS_EMBEDDED
    IOHID_DEBUG(kIOHIDDebugCode_DispatchScroll, deltaAxis1, deltaAxis2, deltaAxis3, options);
    
    if ( ! _readyForInputReports )
        return;
        
    NUB_LOCK;
    
    if ( !_pointingNub )
        _pointingNub = newPointingShim();
        
    if ( !_pointingNub )
        return;
        
        
        
    _pointingNub->dispatchScrollWheelEvent(timeStamp, deltaAxis1, deltaAxis2, deltaAxis3, options);
    
    NUB_UNLOCK;
#endif /* TARGET_OS_EMBEDDED */
}

#if !TARGET_OS_EMBEDDED
static void ScalePressure(SInt32 *pressure, SInt32 pressureMin, SInt32 pressureMax, SInt32 systemPressureMin, SInt32 systemPressureMax)
{    
    SInt64  systemScale = systemPressureMax - systemPressureMin;

    
    *pressure = ((pressureMin != pressureMax)) ? 
                (((unsigned)(*pressure - pressureMin) * systemScale) / 
                (unsigned)( pressureMax - pressureMin)) + systemPressureMin: 0;
}
#endif /* TARGET_OS_EMBEDDED */

//====================================================================================================
// IOHIDEventService::dispatchTabletPointEvent
//====================================================================================================
void IOHIDEventService::dispatchTabletPointerEvent(
                                AbsoluteTime                timeStamp,
                                UInt32                      tranducerID,
                                SInt32                      x,
                                SInt32                      y,
                                SInt32                      z,
                                IOGBounds *                 bounds __unused,
                                UInt32                      buttonState,
                                SInt32                      tipPressure,
                                SInt32                      tipPressureMin,
                                SInt32                      tipPressureMax,
                                SInt32                      barrelPressure,
                                SInt32                      barrelPressureMin,
                                SInt32                      barrelPressureMax,
                                SInt32                      tiltX,
                                SInt32                      tiltY,
                                UInt32                      twist,
                                IOOptionBits                options)
{
#if !TARGET_OS_EMBEDDED
    IOHID_DEBUG(kIOHIDDebugCode_DispatchTabletPointer, x, y, buttonState, options);
    
    if ( ! _readyForInputReports )
        return;
        
    NUB_LOCK;
    
    if ( !_pointingNub )
        _pointingNub = newPointingShim();
        
    TransducerData *    transducerRef = getTransducerData(tranducerID);
    NXEventData         tabletData;
    
    if (!transducerRef) return;
    
    bzero(&tabletData, sizeof(NXEventData));
    
    ScalePressure(&tipPressure, tipPressureMin, tipPressureMax, 0, kMaxSystemTipPressure);
    ScalePressure(&barrelPressure, barrelPressureMin, barrelPressureMax, -kMaxSystemBarrelPressure, kMaxSystemBarrelPressure);
    
    IOGPoint newLoc;
    
    newLoc.x = x;
    newLoc.y = y;
    
    //IOHIDSystem::scaleLocationToCurrentScreen(&newLoc, bounds);
    
    tabletData.tablet.x                    = newLoc.x;
    tabletData.tablet.y                    = newLoc.y;
    tabletData.tablet.z                    = z;
    tabletData.tablet.buttons              = buttonState;
    tabletData.tablet.pressure             = tipPressure;
    tabletData.tablet.tilt.x               = tiltX;
    tabletData.tablet.tilt.y               = tiltY;
    tabletData.tablet.rotation             = twist;
    tabletData.tablet.tangentialPressure   = barrelPressure;
    tabletData.tablet.deviceID             = transducerRef->deviceID;
    
    _pointingNub->dispatchTabletEvent(&tabletData, timeStamp);
    
    NUB_UNLOCK;
#endif /* TARGET_OS_EMBEDDED */
}

//====================================================================================================
// IOHIDEventService::dispatchTabletProximityEvent
//====================================================================================================
void IOHIDEventService::dispatchTabletProximityEvent(
                                AbsoluteTime                timeStamp,
                                UInt32                      tranducerID,
                                bool                        inRange,
                                bool                        invert,
                                UInt32                      vendorTransducerUniqueID,
                                UInt32                      vendorTransducerSerialNumber,
                                IOOptionBits                options)
{
#if !TARGET_OS_EMBEDDED
    IOHID_DEBUG(kIOHIDDebugCode_DispatchTabletProx, tranducerID, vendorTransducerUniqueID, vendorTransducerSerialNumber, options);
    
    if ( ! _readyForInputReports )
        return;
        
    NUB_LOCK;
    
    if ( !_pointingNub )
        _pointingNub = newPointingShim();
        
    TransducerData *    transducerRef = getTransducerData(tranducerID);
    NXEventData         tabletData;
    
    if (!transducerRef) return;
    
    bzero(&tabletData, sizeof(NXEventData));
    
    tabletData.proximity.vendorID               = getVendorID();
    tabletData.proximity.tabletID               = getProductID();
    tabletData.proximity.pointerID              = tranducerID;
    tabletData.proximity.deviceID               = transducerRef->deviceID;
    tabletData.proximity.vendorPointerType      = transducerRef->type;
    tabletData.proximity.pointerSerialNumber    = vendorTransducerSerialNumber;
    tabletData.proximity.uniqueID               = vendorTransducerUniqueID;
    tabletData.proximity.capabilityMask         = transducerRef->capabilities;
    tabletData.proximity.enterProximity         = inRange;
    tabletData.proximity.pointerType            =
        (invert && (transducerRef->type == NX_TABLET_POINTER_PEN)) ?
        NX_TABLET_POINTER_ERASER : NX_TABLET_POINTER_PEN;
        
    _pointingNub->dispatchProximityEvent(&tabletData, timeStamp);
    
    NUB_UNLOCK;
#endif /* TARGET_OS_EMBEDDED */
}

bool IOHIDEventService::readyForReports() 
{
    return _readyForInputReports;
}

//==============================================================================
// IOHIDEventService::getDeviceUsagePairs
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  0);
OSArray * IOHIDEventService::getDeviceUsagePairs()
{
    //RY: Correctly deal with kIOHIDDeviceUsagePairsKey
    OSArray * providerUsagePairs = (OSArray*)_provider->copyProperty(kIOHIDDeviceUsagePairsKey);
    
    if ( OSDynamicCast(OSArray, providerUsagePairs) && ( providerUsagePairs != _deviceUsagePairs ) ) {
        setProperty(kIOHIDDeviceUsagePairsKey, providerUsagePairs);
        if ( _deviceUsagePairs )
            _deviceUsagePairs->release();
        
        _deviceUsagePairs = providerUsagePairs;
        _deviceUsagePairs->retain();
    }
#if TARGET_OS_EMBEDDED
    else if ( !_deviceUsagePairs ) {
        _deviceUsagePairs = OSArray::withCapacity(2);
        
        if ( _deviceUsagePairs ) {
            OSDictionary * pair = OSDictionary::withCapacity(2);
            
            if ( pair ) {
                OSNumber * number;
                
                number = OSNumber::withNumber(getPrimaryUsagePage(), 32);
                if ( number ) {
                    pair->setObject(kIOHIDDeviceUsagePageKey, number);
                    number->release();
                }
                
                number = OSNumber::withNumber(getPrimaryUsage(), 32);
                if ( number ) {
                    pair->setObject(kIOHIDDeviceUsageKey, number);
                    number->release();
                }
                
                _deviceUsagePairs->setObject(pair);
                pair->release();
            }
        }
    }
#endif
    OSSafeRelease(providerUsagePairs);
    
    return _deviceUsagePairs;
}

#if TARGET_OS_EMBEDDED
void IOHIDEventService::close(IOService *forClient, IOOptionBits options)
{
    _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventService::closeGated), forClient, (void*)options);
}

void IOHIDEventService::closeGated(IOService *forClient, IOOptionBits options)
{
    super::close(forClient, options);
}

OSDefineMetaClassAndStructors(IOHIDClientData, OSObject)

IOHIDClientData * IOHIDClientData::withClientInfo(IOService *client, void* context, void * action)
{
    IOHIDClientData * data = new IOHIDClientData;
    
    if (!data) { }
    else if (data->init()) {
        data->client  = client;
        data->context = context;
        data->action  = action;
    } else {
        data->release();
        data = NULL;
    }
        
    return data;
}

//==============================================================================
// IOHIDEventService::open
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  1);
bool IOHIDEventService::open(   IOService *                 client,
                                IOOptionBits                options,
                                void *                      context,
                                Action                      action)
{
    return _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventService::openGated), client, (void*)options, context, (void*)action);
}

//==============================================================================
// IOHIDEventService::dispatchEvent
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  2);
void IOHIDEventService::dispatchEvent(IOHIDEvent * event, IOOptionBits options)
{
    OSCollectionIterator *  iterator = OSCollectionIterator::withCollection(_clientDict);
    IOHIDClientData *       clientData;
    OSObject *              clientKey;
    IOService *             client;
    void *                  context;
    Action                  action;
    
    event->setDeviceID(getRegistryEntryID());

    IOHID_DEBUG(kIOHIDDebugCode_DispatchHIDEvent, options, 0, 0, 0);
    
    if ( !iterator )
        return;
        
    while ((clientKey = iterator->getNextObject())) {
    
        clientData = OSDynamicCast(IOHIDClientData, _clientDict->getObject((const OSSymbol *)clientKey));
        
        if ( !clientData )
            continue;
            
        client  = clientData->getClient();
        context = clientData->getContext();
        action  = (Action)clientData->getAction();
        
        if ( action )
            (*action)(client, this, context, event, options);
    }
    
    iterator->release();
    
}

//==============================================================================
// IOHIDEventService::getPrimaryUsagePage
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  3);
UInt32 IOHIDEventService::getPrimaryUsagePage ()
{
    UInt32		primaryUsagePage = 0;
    OSArray *	deviceUsagePairs = getDeviceUsagePairs();
	
    if ( deviceUsagePairs && deviceUsagePairs->getCount() ) {
        OSDictionary * pair = OSDynamicCast(OSDictionary, deviceUsagePairs->getObject(0));
        
        if ( pair ) {
            OSNumber * number = OSDynamicCast(OSNumber, pair->getObject(kIOHIDDeviceUsagePageKey));
            
            if ( number )
                primaryUsagePage = number->unsigned32BitValue();
        }
    }
    
    return primaryUsagePage;
}

//==============================================================================
// IOHIDEventService::getPrimaryUsage
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  4);
UInt32 IOHIDEventService::getPrimaryUsage ()
{
    UInt32		primaryUsage		= 0;
    OSArray *	deviceUsagePairs	= getDeviceUsagePairs();
	
    if ( deviceUsagePairs && deviceUsagePairs->getCount() ) {
        OSDictionary * pair = OSDynamicCast(OSDictionary, deviceUsagePairs->getObject(0));
        
        if ( pair ) {
            OSNumber * number = OSDynamicCast(OSNumber, pair->getObject(kIOHIDDeviceUsageKey));
                                              
            if ( number )
                primaryUsage = number->unsigned32BitValue();
        }
    }
    
    return primaryUsage;
}

//==============================================================================
// IOHIDEventService::getReportInterval
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  5);
UInt32 IOHIDEventService::getReportInterval()
{
    UInt32 interval = 8000; // default to 8 milliseconds
    OSObject *object = copyProperty(kIOHIDReportIntervalKey, gIOServicePlane, kIORegistryIterateRecursively | kIORegistryIterateParents);
    if ( OSDynamicCast(OSNumber, object) )
        interval = ((OSNumber*)object)->unsigned32BitValue();
    OSSafeReleaseNULL(object);
    
    return interval;
}

//==============================================================================
// IOHIDEventService::copyEvent
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  6);
IOHIDEvent * IOHIDEventService::copyEvent(
                                IOHIDEventType              type, 
                                IOHIDEvent *                matching,
                                IOOptionBits                options)
{
    return NULL;
}

//==============================================================================
// IOHIDEventService::openGated
//==============================================================================
bool IOHIDEventService::openGated(IOService *                 client,
                                  IOOptionBits                options,
                                  void *                      context,
                                  Action                      action)
{
    IOHIDClientData * clientData = 
    IOHIDClientData::withClientInfo(client, context, (void*)action);
    bool ret = false;
    
    if ( clientData ) {
        if ( super::open(client, options, clientData) )
            ret = true;
        clientData->release();
    }
    
    return ret;
}

#else

OSMetaClassDefineReservedUnused(IOHIDEventService,  1);
OSMetaClassDefineReservedUnused(IOHIDEventService,  2);
OSMetaClassDefineReservedUnused(IOHIDEventService,  3);
OSMetaClassDefineReservedUnused(IOHIDEventService,  4);
OSMetaClassDefineReservedUnused(IOHIDEventService,  5);
OSMetaClassDefineReservedUnused(IOHIDEventService,  6);
#endif /* TARGET_OS_EMBEDDED */
OSMetaClassDefineReservedUnused(IOHIDEventService,  7);
OSMetaClassDefineReservedUnused(IOHIDEventService,  8);
OSMetaClassDefineReservedUnused(IOHIDEventService,  9);
OSMetaClassDefineReservedUnused(IOHIDEventService, 10);
OSMetaClassDefineReservedUnused(IOHIDEventService, 11);
OSMetaClassDefineReservedUnused(IOHIDEventService, 12);
OSMetaClassDefineReservedUnused(IOHIDEventService, 13);
OSMetaClassDefineReservedUnused(IOHIDEventService, 14);
OSMetaClassDefineReservedUnused(IOHIDEventService, 15);
OSMetaClassDefineReservedUnused(IOHIDEventService, 16);
OSMetaClassDefineReservedUnused(IOHIDEventService, 17);
OSMetaClassDefineReservedUnused(IOHIDEventService, 18);
OSMetaClassDefineReservedUnused(IOHIDEventService, 19);
OSMetaClassDefineReservedUnused(IOHIDEventService, 20);
OSMetaClassDefineReservedUnused(IOHIDEventService, 21);
OSMetaClassDefineReservedUnused(IOHIDEventService, 22);
OSMetaClassDefineReservedUnused(IOHIDEventService, 23);
OSMetaClassDefineReservedUnused(IOHIDEventService, 24);
OSMetaClassDefineReservedUnused(IOHIDEventService, 25);
OSMetaClassDefineReservedUnused(IOHIDEventService, 26);
OSMetaClassDefineReservedUnused(IOHIDEventService, 27);
OSMetaClassDefineReservedUnused(IOHIDEventService, 28);
OSMetaClassDefineReservedUnused(IOHIDEventService, 29);
OSMetaClassDefineReservedUnused(IOHIDEventService, 30);
OSMetaClassDefineReservedUnused(IOHIDEventService, 31);

