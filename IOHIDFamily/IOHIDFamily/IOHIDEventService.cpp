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

#include "IOHIDSystem.h"
#include "IOHIDEventService.h"
#include "IOHIDInterface.h"
#include "IOHIDKeys.h"
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/IOLib.h>
#include <sys/kdebug.h>
#include <IOKit/usb/USB.h>

#include "AppleHIDUsageTables.h"
#include "IOHIDPointing.h"
#include "IOHIDKeyboard.h"
#include "IOHIDConsumer.h"
#include "IOHIDFamilyPrivate.h"
#include "IOHIDevicePrivateKeys.h"
#include "ev_private.h"

enum {
    kBootProtocolNone   = 0,
    kBootProtocolKeyboard,
    kBootProtocolMouse
};

enum
{
    kShimEventProcessor = 0x01
};

#define     kDefaultFixedResolution             (400 << 16)
#define     kDefaultScrollFixedResolution       (9 << 16)

#define     kMaxSystemBarrelPressure            32767
#define     kMaxSystemTipPressure               65535

#define     kEjectDelayMS                       250
#define     kEjectDelayedOption                 (1<<31)

#define     NUB_LOCK                            if (_nubLock) IORecursiveLockLock(_nubLock)
#define     NUB_UNLOCK                          if (_nubLock) IORecursiveLockUnlock(_nubLock)

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

#define     _workLoop                           _reserved->workLoop
#define     _ejectTimerEventSource              _reserved->ejectTimerEventSource
#define     _ejectState                         _reserved->ejectState
#define     _ejectOptions                       _reserved->ejectOptions

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
    
    return true;
}
//====================================================================================================
// IOHIDEventService::start
//====================================================================================================
bool IOHIDEventService::start ( IOService * provider )
{
    UInt32          bootProtocol        = 0;
    
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
        
    SET_HID_PROPERTIES(this);    
    
    //RY: Correctly deal with kIOHIDDeviceUsagePairsKey
    OSObject * providerUsagePair = provider->getProperty(kIOHIDDeviceUsagePairsKey);
    if ( providerUsagePair && (providerUsagePair != getProperty(kIOHIDDeviceUsagePairsKey)) )
        setProperty(kIOHIDDeviceUsagePairsKey, providerUsagePair);

    OSNumber *  number = OSDynamicCast(OSNumber, getProperty("BootProtocol"));
    
    if (number) 
        bootProtocol = number->unsigned32BitValue();

    parseSupportedElements (getReportElements(), bootProtocol);
    
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

    return true;
}

//====================================================================================================
// IOHIDEventService::setSystemProperties
//====================================================================================================
IOReturn IOHIDEventService::setSystemProperties( OSDictionary * properties )
{
    OSDictionary * propsCopy = OSDictionary::withDictionary(properties);
    
    if ( propsCopy ) {
        propsCopy->setObject(kIOHIDEventServicePropertiesKey, kOSBooleanTrue);
        
        if ( _keyboardNub )
            _keyboardNub->setParamProperties(properties);
            
        if ( _pointingNub )
            _pointingNub->setParamProperties(properties);
            
        if ( _consumerNub )
            _consumerNub->setParamProperties(properties);
            
        propsCopy->release();
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
    IOHIDPointing * pointingNub;
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

    return pointingNub;
}
                                
//====================================================================================================
// IOHIDEventService::newKeyboardShim
//====================================================================================================
IOHIDKeyboard * IOHIDEventService::newKeyboardShim (
                                UInt32          supportedModifiers,
                                IOOptionBits    options)
{
    IOHIDKeyboard * keyboardNub;
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
    
    return keyboardNub;
}

//====================================================================================================
// IOHIDEventService::newConsumerShim
//====================================================================================================
IOHIDConsumer * IOHIDEventService::newConsumerShim ( IOOptionBits options )
{
    IOHIDConsumer * consumerNub;
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
        if ( _workLoop )
            _workLoop->removeEventSource(_ejectTimerEventSource);
            
        _ejectTimerEventSource->release();
        _ejectTimerEventSource = 0; 
    }

    if ( _workLoop ) {
        _workLoop->release();
        _workLoop = NULL;
    }

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
    return 0;
}

//====================================================================================================
// IOHIDEventService::getManufacturer
//====================================================================================================
OSString * IOHIDEventService::getManufacturer ()
{
    return 0;
}

//====================================================================================================
// IOHIDEventService::getProduct
//====================================================================================================
OSString * IOHIDEventService::getProduct ()
{
    return 0;
}

//====================================================================================================
// IOHIDEventService::getSerialNumber
//====================================================================================================
OSString * IOHIDEventService::getSerialNumber ()
{
    return 0;
}

//====================================================================================================
// IOHIDEventService::getLocationID
//====================================================================================================
UInt32 IOHIDEventService::getLocationID ()
{
    return 0;
}

//====================================================================================================
// IOHIDEventService::getVendorID
//====================================================================================================
UInt32 IOHIDEventService::getVendorID ()
{
    return 0;
}

//====================================================================================================
// IOHIDEventService::getVendorIDSource
//====================================================================================================
UInt32 IOHIDEventService::getVendorIDSource ()
{
    return 0;
}

//====================================================================================================
// IOHIDEventService::getProductID
//====================================================================================================
UInt32 IOHIDEventService::getProductID ()
{
    return 0;
}

//====================================================================================================
// IOHIDEventService::getVersion
//====================================================================================================
UInt32 IOHIDEventService::getVersion ()
{
    return 0;
}

//====================================================================================================
// IOHIDEventService::getCountryCode
//====================================================================================================
UInt32 IOHIDEventService::getCountryCode ()
{
    return 0;
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
        
        dispatchKeyboardEvent(timeStamp, kHIDPage_Consumer, kHIDUsage_Csmr_Eject, 1, _ejectOptions | kEjectDelayedOption);
        dispatchKeyboardEvent(timeStamp, kHIDPage_Consumer, kHIDUsage_Csmr_Eject, 0, _ejectOptions | kEjectDelayedOption);
        
        _ejectState = 0;
    }
    NUB_UNLOCK;
}

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
        
	if ((( usagePage == kHIDPage_KeyboardOrKeypad ) &&
        (usage != kHIDUsage_KeyboardLockingNumLock)) ||
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
            if ( (usagePage == kHIDPage_Consumer) && (usage == kHIDUsage_Csmr_Eject) && ((options & kEjectDelayedOption) == 0) && _keyboardNub && ((_keyboardNub->eventFlags() & SPECIALKEYS_MODIFIER_MASK) == 0)) 
            {
                if ( _ejectState != value ) {
                    if ( value ) {
                        _ejectOptions       = options;
                        
                        _ejectTimerEventSource->setTimeoutMS( kEjectDelayMS );
                    } else
                        _ejectTimerEventSource->cancelTimeout();
                        
                    _ejectState = value;
                }
            }
            else  if (!(((options & kEjectDelayedOption) == 0) && _ejectState && (usagePage == kHIDPage_Consumer) && (usage == kHIDUsage_Csmr_Eject))) 
            {
                _consumerNub->dispatchConsumerEvent(_keyboardNub, timeStamp, usagePage, usage, value, options);
            }
        }
    }
    
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
    if ( ! _readyForInputReports )
        return;

    NUB_LOCK;

    if ( !_pointingNub )
		_pointingNub = newPointingShim();

	if ( _pointingNub )
        _pointingNub->dispatchRelativePointerEvent(timeStamp, dx, dy, buttonState, options);

    NUB_UNLOCK;
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
    if ( ! _readyForInputReports )
        return;

    NUB_LOCK;

    if ( !_pointingNub )
		_pointingNub = newPointingShim();

	if ( !_pointingNub )
		return;
			
	_pointingNub->dispatchScrollWheelEvent(timeStamp, deltaAxis1, deltaAxis2, deltaAxis3, options);

    NUB_UNLOCK;

}

static void ScalePressure(SInt32 *pressure, SInt32 pressureMin, SInt32 pressureMax, SInt32 systemPressureMin, SInt32 systemPressureMax)
{    
    SInt64  systemScale = systemPressureMax - systemPressureMin;

    
    *pressure = ((pressureMin != pressureMax)) ? 
                (((unsigned)(*pressure - pressureMin) * systemScale) / 
                (unsigned)( pressureMax - pressureMin)) + systemPressureMin: 0;
}

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
    if ( ! _readyForInputReports )
        return;

    if ( !_pointingNub )
		_pointingNub = newPointingShim();

    NUB_LOCK;
			    
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

}

OSMetaClassDefineReservedUnused(IOHIDEventService,  0);
OSMetaClassDefineReservedUnused(IOHIDEventService,  1);
OSMetaClassDefineReservedUnused(IOHIDEventService,  2);
OSMetaClassDefineReservedUnused(IOHIDEventService,  3);
OSMetaClassDefineReservedUnused(IOHIDEventService,  4);
OSMetaClassDefineReservedUnused(IOHIDEventService,  5);
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

