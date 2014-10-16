/*
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 2008 Apple, Inc.  All Rights Reserved.
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

#include "IOHIDUserDevice.h"
#include "IOHIDResourceUserClient.h"
#include "IOHIDKeys.h"
#include <IOKit/IOLib.h>

#define super IOHIDDevice

OSDefineMetaClassAndStructors(IOHIDUserDevice, IOHIDDevice)

#pragma mark -
#pragma mark Methods

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDUserDevice::withProperties
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDUserDevice * IOHIDUserDevice::withProperties(OSDictionary * properties)
{
    IOHIDUserDevice * device = new IOHIDUserDevice;
    
    do { 
        if ( !device )
            break;
            
        if ( !device->initWithProperties(properties) )
            break;
        
        return device;
        
    } while ( false );
    
    if ( device )
        device->release();
    
    return NULL;
}

//----------------------------------------------------------------------------------------------------
// IOHIDUserDevice::initWithProperties
//----------------------------------------------------------------------------------------------------
bool IOHIDUserDevice::initWithProperties(OSDictionary * properties)
{
    if ( !properties )
        return false;
        
    if ( !super::init(properties) )
        return false;
        
    _properties = properties;
    _properties->retain();

    setProperty("HIDDefaultBehavior", kOSBooleanTrue);
    
    return TRUE;
}

//----------------------------------------------------------------------------------------------------
// IOHIDUserDevice::free
//----------------------------------------------------------------------------------------------------
void IOHIDUserDevice::free()
{
    if ( _properties )
        _properties->release();
        
    super::free();
}

//----------------------------------------------------------------------------------------------------
// IOHIDUserDevice::handleStart
//----------------------------------------------------------------------------------------------------
bool IOHIDUserDevice::handleStart( IOService * provider )
{
    if (!super::handleStart(provider))
        return false;
    
    _provider = OSDynamicCast(IOHIDResourceDeviceUserClient, provider);
    if ( !_provider )
        return false;
        
    return true;
}

//----------------------------------------------------------------------------------------------------
// IOHIDUserDevice::handleStop
//----------------------------------------------------------------------------------------------------
void IOHIDUserDevice::handleStop(  IOService * provider )
{
    super::handleStop(provider);
}

//----------------------------------------------------------------------------------------------------
// IOHIDUserDevice::newTransportString
//----------------------------------------------------------------------------------------------------
OSString *IOHIDUserDevice::newTransportString() const
{
    OSString * string = OSDynamicCast(OSString, _properties->getObject(kIOHIDTransportKey));
    
    if ( !string ) 
        return NULL;
        
    string->retain();
        
    return string;
}

//----------------------------------------------------------------------------------------------------
// IOHIDUserDevice::newManufacturerString
//----------------------------------------------------------------------------------------------------
OSString *IOHIDUserDevice::newManufacturerString() const
{
    OSString * string = OSDynamicCast(OSString, _properties->getObject(kIOHIDManufacturerKey));
    
    if ( !string ) 
        return NULL;
        
    string->retain();
        
    return string;
}

//----------------------------------------------------------------------------------------------------
// IOHIDUserDevice::newProductString
//----------------------------------------------------------------------------------------------------
OSString *IOHIDUserDevice::newProductString() const
{
    OSString * string = OSDynamicCast(OSString, _properties->getObject(kIOHIDProductKey));
    
    if ( !string ) 
        return NULL;
        
    string->retain();
        
    return string;
}

//----------------------------------------------------------------------------------------------------
// IOHIDUserDevice::newVendorIDNumber
//----------------------------------------------------------------------------------------------------
OSNumber *IOHIDUserDevice::newVendorIDNumber() const
{
    OSNumber * number = OSDynamicCast(OSNumber, _properties->getObject(kIOHIDVendorIDKey));
    
    if ( !number ) 
        return NULL;
        
    number->retain();
        
    return number;
}

//----------------------------------------------------------------------------------------------------
// IOHIDUserDevice::newProductIDNumber
//----------------------------------------------------------------------------------------------------
OSNumber *IOHIDUserDevice::newProductIDNumber() const
{
    OSNumber * number = OSDynamicCast(OSNumber, _properties->getObject(kIOHIDProductIDKey));
    
    if ( !number ) 
        return NULL;
        
    number->retain();
        
    return number;
}

//----------------------------------------------------------------------------------------------------
// IOHIDUserDevice::newVersionNumber
//----------------------------------------------------------------------------------------------------
OSNumber *IOHIDUserDevice::newVersionNumber() const
{
    OSNumber * number = OSDynamicCast(OSNumber, _properties->getObject(kIOHIDVersionNumberKey));
    
    if ( !number ) 
        return NULL;
        
    number->retain();
        
    return number;
}

//----------------------------------------------------------------------------------------------------
// IOHIDUserDevice::newSerialNumberString
//----------------------------------------------------------------------------------------------------
OSString *IOHIDUserDevice::newSerialNumberString() const
{
    OSString * string = OSDynamicCast(OSString, _properties->getObject(kIOHIDSerialNumberKey));
    
    if ( !string ) 
        return NULL;
        
    string->retain();
        
    return string;
}

//----------------------------------------------------------------------------------------------------
// IOHIDUserDevice::newVendorIDSourceNumber
//----------------------------------------------------------------------------------------------------
OSNumber *IOHIDUserDevice::newVendorIDSourceNumber() const
{
    OSNumber * number = OSDynamicCast(OSNumber, _properties->getObject(kIOHIDVendorIDSourceKey));
    
    if ( !number ) 
        return NULL;
        
    number->retain();
        
    return number;
}

//----------------------------------------------------------------------------------------------------
// IOHIDUserDevice::newCountryCodeNumber
//----------------------------------------------------------------------------------------------------
OSNumber *IOHIDUserDevice::newCountryCodeNumber() const
{
    OSNumber * number = OSDynamicCast(OSNumber, _properties->getObject(kIOHIDCountryCodeKey));
    
    if ( !number ) 
        return NULL;
    
    number->retain();
    
    return number;
}

//----------------------------------------------------------------------------------------------------
// IOHIDUserDevice::newReportIntervalNumber
//----------------------------------------------------------------------------------------------------
OSNumber *IOHIDUserDevice::newReportIntervalNumber() const
{
    OSNumber * number = OSDynamicCast(OSNumber, _properties->getObject(kIOHIDReportIntervalKey));
    
    if ( !number ) {
        number = IOHIDDevice::newReportIntervalNumber();
    }
    else {
        number->retain();
    }
        
    return number;
}

//----------------------------------------------------------------------------------------------------
// IOHIDUserDevice::newLocationIDNumber
//----------------------------------------------------------------------------------------------------
OSNumber *IOHIDUserDevice::newLocationIDNumber() const
{
    OSNumber * number = OSDynamicCast(OSNumber, _properties->getObject(kIOHIDLocationIDKey));
    
    if ( !number ) 
        return NULL;
    
    number->retain();
    
    return number;
}

//----------------------------------------------------------------------------------------------------
// IOHIDUserDevice::newReportDescriptor
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDUserDevice::newReportDescriptor(IOMemoryDescriptor ** descriptor ) const
{
    OSData *                    data;
    
    data = OSDynamicCast(OSData, _properties->getObject(kIOHIDReportDescriptorKey));
    if ( !data )
        return kIOReturnError;
            
    *descriptor = IOBufferMemoryDescriptor::withBytes(data->getBytesNoCopy(), data->getLength(), kIODirectionNone);

    return kIOReturnSuccess;
}


//----------------------------------------------------------------------------------------------------
// IOHIDUserDevice::getReport
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDUserDevice::getReport(IOMemoryDescriptor    *report,
                                    IOHIDReportType        reportType,
                                    IOOptionBits        options )
{
    return _provider->getReport(report, reportType, options);
}


//----------------------------------------------------------------------------------------------------
// IOHIDUserDevice::setReport
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDUserDevice::setReport(IOMemoryDescriptor    *report,
                                    IOHIDReportType        reportType,
                                    IOOptionBits        options)
{
    return _provider->setReport(report, reportType, options);
}


