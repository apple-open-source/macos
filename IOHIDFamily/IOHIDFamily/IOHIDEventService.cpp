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
#include <sys/kdebug.h>
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

#define     kEjectDelayMS                       250
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
    service->setProperty(kIOHIDSerialNumberKey, getSerialNumber());

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

#if TARGET_OS_EMBEDDED
    #define     _clientDict                         _reserved->clientDict
    #define     _debuggerMask                       _reserved->debuggerMask
    #define     _startDebuggerMask                  _reserved->startDebuggerMask
    #define     _debuggerTimerEventSource           _reserved->debuggerTimerEventSource

    #define     kDebuggerDelayMS                    2500
    #define     kDebuggerDelayPwrOnlyMS             10000

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

OSDefineMetaClassAndAbstractStructors( IOHIDEventService, super )
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

    _ejectDelayMS = kEjectDelayMS;
    
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

    _workLoop = provider->getWorkLoop();
    if ( !_workLoop )
        return false;
        
    _workLoop->retain();

    _ejectTimerEventSource = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &IOHIDEventService::ejectTimerCallback));
    if (!_ejectTimerEventSource || (_workLoop->addEventSource(_ejectTimerEventSource) != kIOReturnSuccess))
        return false;

    number = OSDynamicCast(OSNumber, getProperty(kIOHIDKeyboardEjectDelay));
    if ( number )
        _ejectDelayMS = number->unsigned32BitValue();

    _capsTimerEventSource = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &IOHIDEventService::capsTimerCallback));
    if (!_capsTimerEventSource || (_workLoop->addEventSource(_capsTimerEventSource) != kIOReturnSuccess))
        return false;

	number = OSDynamicCast(OSNumber, getProperty(kIOHIDKeyboardCapsLockDelay));
	if ( number )
		_capsDelayMS = number->unsigned32BitValue();

    SET_HID_PROPERTIES(this);  
    SET_HID_PROPERTIES_EMBEDDED(this);  
    
    //RY: Correctly deal with kIOHIDDeviceUsagePairsKey
    OSObject * providerUsagePair = provider->getProperty(kIOHIDDeviceUsagePairsKey);
    if ( providerUsagePair && (providerUsagePair != getProperty(kIOHIDDeviceUsagePairsKey)) ) {
        setProperty(kIOHIDDeviceUsagePairsKey, providerUsagePair);
    } else {
        OSArray* array = OSArray::withCapacity(2);
        
#if TARGET_OS_EMBEDDED
        OSDictionary * pair = OSDictionary::withCapacity(2);

        number = OSNumber::withNumber(getPrimaryUsagePage(), 32);
        pair->setObject(kIOHIDPrimaryUsagePageKey, number);
        number->release();

        number = OSNumber::withNumber(getPrimaryUsage(), 32);
        pair->setObject(kIOHIDPrimaryUsageKey, number);
        number->release();
        
        array->setObject(pair);
        pair->release();
#endif
        
        setProperty(kIOHIDDeviceUsagePairsKey, array);
        array->release();
    }

    number = OSDynamicCast(OSNumber, getProperty("BootProtocol"));
    if (number) 
        bootProtocol = number->unsigned32BitValue();

    parseSupportedElements (getReportElements(), bootProtocol);
    
#if !TARGET_OS_EMBEDDED
    if ((!_consumerNub && _keyboardNub) || (!_keyboardNub && _consumerNub))
    {
        OSDictionary * matchingDictionary = IOService::serviceMatching( "IOHIDEventService" );
        if( matchingDictionary )
        {    
            OSDictionary *      propertyMatch = OSDictionary::withCapacity(4);
            OSObject *          object;  
                      
            if (propertyMatch)
            {
                object = getProperty(kIOHIDTransportKey);
                if (object) propertyMatch->setObject(kIOHIDTransportKey, object);
                
                object = getProperty(kIOHIDVendorIDKey);
                if (object) propertyMatch->setObject(kIOHIDVendorIDKey, object);

                object = getProperty(kIOHIDProductIDKey);
                if (object) propertyMatch->setObject(kIOHIDProductIDKey, object);

                object = getProperty(kIOHIDLocationIDKey);
                if (object) propertyMatch->setObject(kIOHIDLocationIDKey, object);

                matchingDictionary->setObject(gIOPropertyMatchKey, propertyMatch);
                
                propertyMatch->release();
            }
            _publishNotify = addNotification( gIOPublishNotification, 
                                matchingDictionary,
                                &IOHIDEventService::_publishNotificationHandler,
                                this, 0 );    
        }
    }
#endif /* TARGET_OS_EMBEDDED */
    
    _readyForInputReports = true;
    
    registerService(kIOServiceSynchronous);
    
    return true;
}

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

//====================================================================================================
// IOHIDEventService::stop
//====================================================================================================
void IOHIDEventService::stop( IOService * provider )
{
    handleStop ( provider );

#if !TARGET_OS_EMBEDDED
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
// IOHIDEventService::_publishNotificationHandler
//====================================================================================================
bool IOHIDEventService::_publishNotificationHandler(
			void * target,
			void * /* ref */,
			IOService * newService )
{
#if !TARGET_OS_EMBEDDED
    IOHIDEventService * self    = (IOHIDEventService *) target;
    IOHIDEventService * service = (IOHIDEventService *) newService;
    
    IORecursiveLockLock(self->_nubLock);
    
    if( service->_keyboardNub )
    {
        if ( self->_keyboardNub 
             && self->_keyboardNub->isDispatcher()
             && !service->_keyboardNub->isDispatcher() )
        {
            stopAndReleaseShim ( self->_keyboardNub, self );
            self->_keyboardNub = 0;
        }

        if ( !self->_keyboardNub ) 
        {
            self->_keyboardNub = service->_keyboardNub;
            self->_keyboardNub->retain();

            if (self->_publishNotify) 
            {
                self->_publishNotify->remove();
                self->_publishNotify = 0;
            }
        }
    }

    if( service->_consumerNub )
    {
        if ( self->_consumerNub 
             && self->_consumerNub->isDispatcher()
             && !service->_consumerNub->isDispatcher() )
        {
            stopAndReleaseShim ( self->_consumerNub, self );
            self->_consumerNub = 0;
        }

        if ( !self->_consumerNub ) 
        {
            self->_consumerNub = service->_consumerNub;
            self->_consumerNub->retain();

            if (self->_publishNotify) 
            {
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
// IOHIDEventService::setSystemProperties
//====================================================================================================
IOReturn IOHIDEventService::setSystemProperties( OSDictionary * properties )
{
    OSDictionary *  dict        = NULL;
    OSArray *       array       = NULL;
    OSNumber *      number      = NULL;
    
    if ( !properties )
        return kIOReturnBadArgument;

    if ( properties->getObject(kIOHIDDeviceParametersKey) != kOSBooleanTrue )
    {
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
        
        } else if ( !_capsDelayMS ) {
            number = OSDynamicCast(OSNumber, getProperty(kIOHIDKeyboardCapsLockDelay));
            if ( number )
                _capsDelayMS = number->unsigned32BitValue();
        }
        
        
    }


    if ( properties->getObject(kIOHIDDeviceParametersKey) == kOSBooleanTrue ) {
        OSDictionary * eventServiceProperties = OSDynamicCast(OSDictionary, copyProperty(kIOHIDEventServicePropertiesKey));        
        if ( !eventServiceProperties )
            eventServiceProperties = OSDictionary::withCapacity(4);
    
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
                    
                    if (found = tempPair->isEqualTo(pairRef))
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
        
        setProperty(kIOHIDDeviceUsagePairsKey, functions);
        if (functions) functions->release();
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

    if ( _nubLock )
    {
        IORecursiveLockLock(_nubLock);
        tempLock = _nubLock;
        _nubLock = NULL;
    }

    if ( _transducerDataArray )
    {
        _transducerDataArray->release();
        _transducerDataArray = 0;
    }
    
    if ( _transducerDataArray )
    {
        _transducerDataArray->release();
        _transducerDataArray = 0;
    }

    if (_ejectTimerEventSource) {
        _ejectTimerEventSource->cancelTimeout();
        if ( _workLoop )
            _workLoop->removeEventSource(_ejectTimerEventSource);
            
        _ejectTimerEventSource->release();
        _ejectTimerEventSource = 0; 
    }

    if (_capsTimerEventSource) {
        _capsTimerEventSource->cancelTimeout();
       if ( _workLoop )
            _workLoop->removeEventSource(_capsTimerEventSource);
            
        _capsTimerEventSource->release();
        _capsTimerEventSource = 0; 
    }

    if ( _workLoop ) {
    	// not our workloop. don't stop it.
        _workLoop->release();
        _workLoop = NULL;
    }
    
#if TARGET_OS_EMBEDDED
    if ( _clientDict )
    {
        assert(_clientDict->getCount() == 0);
        _clientDict->release();
        _clientDict = NULL;
    }
#endif /* TARGET_OS_EMBEDDED */

    if (_reserved) {
        IODelete(_reserved, ExpansionData, 1);
        _reserved = NULL;
    }

    if ( tempLock )
    {
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
bool IOHIDEventService::handleStart( IOService * provider )
{
    return true;
}

//====================================================================================================
// IOHIDEventService::handleStop
//====================================================================================================
void IOHIDEventService::handleStop(  IOService * provider )
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
    return _provider ? OSDynamicCast(OSString, _provider->getProperty(kIOHIDManufacturerKey)) : 0;
}

//====================================================================================================
// IOHIDEventService::getProduct
//====================================================================================================
OSString * IOHIDEventService::getProduct ()
{
    return _provider ? OSDynamicCast(OSString, _provider->getProperty(kIOHIDProductKey)) : 0;
}

//====================================================================================================
// IOHIDEventService::getSerialNumber
//====================================================================================================
OSString * IOHIDEventService::getSerialNumber ()
{
    return _provider ? OSDynamicCast(OSString, _provider->getProperty(kIOHIDSerialNumberKey)) : 0;
}

//====================================================================================================
// IOHIDEventService::getLocationID
//====================================================================================================
UInt32 IOHIDEventService::getLocationID ()
{
	UInt32 value = 0;
	
	if ( _provider ) {
		OSNumber * number = OSDynamicCast(OSNumber, _provider->getProperty(kIOHIDSerialNumberKey));
		if ( number )
			value = number->unsigned32BitValue();
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
		OSNumber * number = OSDynamicCast(OSNumber, _provider->getProperty(kIOHIDVendorIDKey));
		if ( number )
			value = number->unsigned32BitValue();
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
		OSNumber * number = OSDynamicCast(OSNumber, _provider->getProperty(kIOHIDVendorIDSourceKey));
		if ( number )
			value = number->unsigned32BitValue();
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
		OSNumber * number = OSDynamicCast(OSNumber, _provider->getProperty(kIOHIDProductIDKey));
		if ( number )
			value = number->unsigned32BitValue();
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
		OSNumber * number = OSDynamicCast(OSNumber, _provider->getProperty(kIOHIDVersionNumberKey));
		if ( number )
			value = number->unsigned32BitValue();
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
		OSNumber * number = OSDynamicCast(OSNumber, _provider->getProperty(kIOHIDCountryCodeKey));
		if ( number )
			value = number->unsigned32BitValue();
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
                                UInt32                      usagePage,
                                UInt32                      usage,
                                UInt32                      value )
{
}

//====================================================================================================
// IOHIDEventService::getElementValue
//====================================================================================================
UInt32 IOHIDEventService::getElementValue ( 
                                UInt32                      usagePage,
                                UInt32                      usage )
{
    return 0;
}


//====================================================================================================
// ejectTimerCallback
//====================================================================================================
void IOHIDEventService::ejectTimerCallback(IOTimerEventSource *sender) 
{ 
    NUB_LOCK;
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
void IOHIDEventService::capsTimerCallback(IOTimerEventSource *sender) 
{ 
    NUB_LOCK;
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

#if TARGET_OS_EMBEDDED
    IOHIDEvent * event = NULL;
    UInt32 debugMask = 0;

    switch (usagePage) {
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
        
    if ( _debuggerMask == 0x3 || _debuggerMask == 0x1 ) {
        if ( !_debuggerTimerEventSource ) {
            _debuggerTimerEventSource = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &IOHIDEventService::debuggerTimerCallback));
            if (_debuggerTimerEventSource) {
                IOWorkLoop * wl = getWorkLoop();
                if (!wl || (wl->addEventSource(_debuggerTimerEventSource) != kIOReturnSuccess)) {
                    _debuggerTimerEventSource->release();
                    _debuggerTimerEventSource = NULL;
                }
            }
        }
        if ( _debuggerTimerEventSource ) {
            if ( _debuggerMask == 0x3 ) {
                _debuggerTimerEventSource->setTimeoutMS( kDebuggerDelayMS );
            } else { 
                _debuggerTimerEventSource->setTimeoutMS( kDebuggerDelayPwrOnlyMS );
            }
            _startDebuggerMask = _debuggerMask;
        }
    }
    
    event = IOHIDEvent::keyboardEvent(timeStamp, usagePage, usage, value, options);    
    if ( !event )
        return;

    dispatchEvent(event);
    
    event->release();

#else
     
	if ((( usagePage == kHIDPage_KeyboardOrKeypad ) &&
        (usage != kHIDUsage_KeyboardLockingNumLock) && 
        !(_capsDelayMS && (usage == kHIDUsage_KeyboardCapsLock))) ||
        ((getVendorID() == kIOUSBVendorIDAppleComputer) && 
        ((usagePage == kHIDPage_AppleVendorKeyboard) ||
        ((usagePage == kHIDPage_AppleVendorTopCase) && 
        (usage == kHIDUsage_AV_TopCase_KeyboardFn)))))
    {
        if ( !_keyboardNub )
            _keyboardNub = newKeyboardShim();

        if ( _keyboardNub )		
            _keyboardNub->dispatchKeyboardEvent(timeStamp, usagePage, usage, (value != 0), options);
            
    }
    else    
    {
        if ( !_consumerNub )
            _consumerNub = newConsumerShim();

        if ( _consumerNub ) {
            if ( (usagePage == kHIDPage_Consumer) && (usage == kHIDUsage_Csmr_Eject) && ((options & kDelayedOption) == 0) && _keyboardNub && ((_keyboardNub->eventFlags() & SPECIALKEYS_MODIFIER_MASK) == 0)) 
            {
                if ( _ejectState != value ) {
                    if ( value ) {
                        _ejectOptions       = options;
                        
                        _ejectTimerEventSource->setTimeoutMS( _ejectDelayMS );
                    } else
                        _ejectTimerEventSource->cancelTimeout();
                        
                    _ejectState = value;
                }
            }
            else  if (!(((options & kDelayedOption) == 0) && _ejectState && (usagePage == kHIDPage_Consumer) && (usage == kHIDUsage_Csmr_Eject))) 
            {
                _consumerNub->dispatchConsumerEvent(_keyboardNub, timeStamp, usagePage, usage, value, options);
            }
        }
        
        if ( _capsDelayMS && (usagePage == kHIDPage_KeyboardOrKeypad) && (usage == kHIDUsage_KeyboardCapsLock))
        {
            if ( (options & kDelayedOption) == 0) 
            {
            
                if ( getElementValue(kHIDPage_LEDs, kHIDUsage_LED_CapsLock) == 0 ) {
                    if ( _capsState != value ) {
                        if ( value ) {
                            _capsOptions       = options;
                            
                            _capsTimerEventSource->setTimeoutMS( _capsDelayMS );
                        } else
                            _capsTimerEventSource->cancelTimeout();
                            
                        _capsState = value;
                    }
                } else {
                    _keyboardNub->dispatchKeyboardEvent(timeStamp, usagePage, usage, value, options);
                }
            }
            else  if (!( ((options & kDelayedOption) == 0) && _capsState ) ) 
            {
                _keyboardNub->dispatchKeyboardEvent(timeStamp, usagePage, usage, value, options);
            }
        }
    }
#endif /* TARGET_OS_EMBEDDED */
    
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
#if !TARGET_OS_EMBEDDED
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
                                IOGBounds *                 bounds,
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

#if TARGET_OS_EMBEDDED
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

// IOHIDEventService::open
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  0);
bool IOHIDEventService::open(   IOService *                 client,
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

//==============================================================================
// IOHIDEventService::dispatchEvent
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  1);
void IOHIDEventService::dispatchEvent(IOHIDEvent * event, IOOptionBits options)
{
    OSCollectionIterator *  iterator = OSCollectionIterator::withCollection(_clientDict);
    IOHIDClientData *       clientData;
    OSObject *              clientKey;
    IOService *             client;
    void *                  context;
    Action                  action;

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
OSMetaClassDefineReservedUsed(IOHIDEventService,  2);
UInt32 IOHIDEventService::getPrimaryUsagePage ()
{
    return 0;
}

//==============================================================================
// IOHIDEventService::getPrimaryUsage
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  3);
UInt32 IOHIDEventService::getPrimaryUsage ()
{
    return 0;
}

//==============================================================================
// IOHIDEventService::getReportInterval
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  4);
UInt32 IOHIDEventService::getReportInterval()
{
    // default to 8 milliseconds
    return 8000;
}

//==============================================================================
// IOHIDEventService::copyEvent
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  5);
IOHIDEvent * IOHIDEventService::copyEvent(
                                IOHIDEventType              type, 
                                IOHIDEvent *                matching,
                                IOOptionBits                options)
{
    return NULL;
}

#else

OSMetaClassDefineReservedUnused(IOHIDEventService,  0);
OSMetaClassDefineReservedUnused(IOHIDEventService,  1);
OSMetaClassDefineReservedUnused(IOHIDEventService,  2);
OSMetaClassDefineReservedUnused(IOHIDEventService,  3);
OSMetaClassDefineReservedUnused(IOHIDEventService,  4);
OSMetaClassDefineReservedUnused(IOHIDEventService,  5);
#endif /* TARGET_OS_EMBEDDED */

OSMetaClassDefineReservedUnused(IOHIDEventService,  6);
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

