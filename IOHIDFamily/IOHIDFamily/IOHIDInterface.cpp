/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2013 Apple Computer, Inc.  All Rights Reserved.
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

#include <IOKit/IOLib.h>    // IOMalloc/IOFree
#include "IOHIDInterface.h"
#include "IOHIDDevice.h"
#include "IOHIDElementPrivate.h"
#include "OSStackRetain.h"

//===========================================================================
// IOHIDInterface class

#define super IOService

OSDefineMetaClassAndStructors( IOHIDInterface, IOService )

// RESERVED IOHIDInterface CLASS VARIABLES
// Defined here to avoid conflicts from within header file
#define _reportInterval             _reserved->reportInterval

//---------------------------------------------------------------------------
// IOHIDInterface::free

void IOHIDInterface::free()
{
    OSSafeReleaseNULL(_transportString);
    OSSafeReleaseNULL(_elementArray);
    OSSafeReleaseNULL(_manufacturerString);
    OSSafeReleaseNULL(_productString);
    OSSafeReleaseNULL(_serialNumberString);

    if ( _reserved )
    {        
        IODelete( _reserved, ExpansionData, 1 );
    }

    super::free();
}

//---------------------------------------------------------------------------
// IOHIDInterface::init

bool IOHIDInterface::init( OSDictionary * dictionary )
{
    if ( !super::init(dictionary) )
        return false;

    _reserved = IONew( ExpansionData, 1 );

    if (!_reserved)
        return false;
		
	bzero(_reserved, sizeof(ExpansionData));
            
        
    bzero(_maxReportSize, sizeof(IOByteCount) * kIOHIDReportTypeCount);
        
    return true;
}

//---------------------------------------------------------------------------
// IOHIDInterface::withElements

IOHIDInterface * IOHIDInterface::withElements( OSArray * elements )
{
    IOHIDInterface * nub = new IOHIDInterface;
    
    if ((nub == 0) || !nub->init() || !elements)
    {
        if (nub) nub->release();
        return 0;
    }
    
    nub->_elementArray  = elements;
    nub->_elementArray->retain();
    
    return nub;
}

//---------------------------------------------------------------------------
// IOHIDInterface::message

IOReturn IOHIDInterface::message(UInt32 type,
                                 IOService * provider,
                                 void * argument)
{
    IOReturn result = kIOReturnSuccess;
    if (kIOMessageServiceIsRequestingClose == type) {
        result = messageClients(type, argument);
        if (result != kIOReturnSuccess) {
            IOLog("IOHIDInterface unsuccessfully requested close of clients: 0x%08x\n", result);
        }
        else {
            provider->close(this);
        }
    }
    else {
        result = super::message(type, provider, argument);
    }
    
    return result;
}


//---------------------------------------------------------------------------
// IOHIDInterface::start

bool IOHIDInterface::start( IOService * provider )
{
    OSNumber * number;
    
    if ( !super::start(provider) )
        return false;
		
	_owner = OSDynamicCast( IOHIDDevice, provider );
	
	if ( !_owner )
		return false;
            
    
    _transportString    = (OSString*)copyProperty(kIOHIDTransportKey);
    _manufacturerString = (OSString*)copyProperty(kIOHIDManufacturerKey);
    _productString      = (OSString*)copyProperty(kIOHIDProductKey);
    _serialNumberString = (OSString*)copyProperty(kIOHIDSerialNumberKey);
    if (_transportString && !OSDynamicCast(OSString, _transportString))
        OSSafeReleaseNULL(_transportString);
    if (_manufacturerString && !OSDynamicCast(OSString, _manufacturerString))
        OSSafeReleaseNULL(_manufacturerString);
    if (_productString && !OSDynamicCast(OSString, _productString))
        OSSafeReleaseNULL(_productString);
    if (_serialNumberString && !OSDynamicCast(OSString, _serialNumberString))
        OSSafeReleaseNULL(_serialNumberString);
    
    number = (OSNumber*)copyProperty(kIOHIDLocationIDKey);
    if ( OSDynamicCast(OSNumber, number) ) _locationID = number->unsigned32BitValue();
    OSSafeReleaseNULL(number);
    
    number = (OSNumber*)copyProperty(kIOHIDVendorIDKey);
    if ( OSDynamicCast(OSNumber, number) ) _vendorID = number->unsigned32BitValue();
    OSSafeReleaseNULL(number);

    number = (OSNumber*)copyProperty(kIOHIDVendorIDSourceKey);
    if ( OSDynamicCast(OSNumber, number) ) _vendorIDSource = number->unsigned32BitValue();
    OSSafeReleaseNULL(number);

    number = (OSNumber*)copyProperty(kIOHIDProductIDKey);
    if ( OSDynamicCast(OSNumber, number) ) _productID = number->unsigned32BitValue();
    OSSafeReleaseNULL(number);

    number = (OSNumber*)copyProperty(kIOHIDVersionNumberKey);
    if ( OSDynamicCast(OSNumber, number) ) _version = number->unsigned32BitValue();
    OSSafeReleaseNULL(number);

    number = (OSNumber*)copyProperty(kIOHIDCountryCodeKey);
    if ( number ) _countryCode = number->unsigned32BitValue();
    OSSafeReleaseNULL(number);

    number = (OSNumber*)_owner->copyProperty(kIOHIDMaxInputReportSizeKey);
    if ( OSDynamicCast(OSNumber, number) ) _maxReportSize[kIOHIDReportTypeInput] = number->unsigned32BitValue();
    OSSafeReleaseNULL(number);

    number = (OSNumber*)_owner->copyProperty(kIOHIDMaxOutputReportSizeKey);
    if ( OSDynamicCast(OSNumber, number) ) _maxReportSize[kIOHIDReportTypeOutput] = number->unsigned32BitValue();
    OSSafeReleaseNULL(number);

    number = (OSNumber*)_owner->copyProperty(kIOHIDMaxFeatureReportSizeKey);
    if ( OSDynamicCast(OSNumber, number) ) _maxReportSize[kIOHIDReportTypeFeature] = number->unsigned32BitValue();
    OSSafeReleaseNULL(number);
    
    number = (OSNumber*)_owner->copyProperty(kIOHIDReportIntervalKey);
    if ( OSDynamicCast(OSNumber, number) ) _reportInterval = number->unsigned32BitValue();
    OSSafeReleaseNULL(number);
    
    registerService(kIOServiceAsynchronous);
    
    return true;
}

//====================================================================================================
// IOHIDInterface::stop
//====================================================================================================
void IOHIDInterface::stop( IOService * provider )
{
    _owner = NULL;
    super::stop(provider);
}
    
//---------------------------------------------------------------------------
// IOHIDInterface::matchPropertyTable

bool IOHIDInterface::matchPropertyTable(
                                OSDictionary *              table, 
                                SInt32 *                    score)
{
    IOService * provider;
    bool        ret;
    RETAIN_ON_STACK(this);
    
    if ( !super::matchPropertyTable(table, score) || !(provider = OSDynamicCast(IOService, copyParentEntry(gIOServicePlane))) )
        return false;
        
    // We should retain a reference to our provider while calling matchPropertyTable.
    // This is necessary in a situation where a user space process could be searching
    // the registry during termination.    
    ret = provider->matchPropertyTable(table, score);
    
    provider->release();
    
    return ret;
}

//---------------------------------------------------------------------------
// IOHIDInterface::open

bool IOHIDInterface::open (
                                IOService *                 client,
                                IOOptionBits                options,
                                InterruptReportAction       action,
                                void *                      refCon)
{
    if ( !super::open(client, options) )
        return false;
        
    if ( !_owner || !_owner->IOService::open(client, options) )
    {
        super::close(client, options);
        return false;
    }
    
    // Do something with action and refcon here
    _interruptTarget = client;
    _interruptAction = action;
    _interruptRefCon = refCon;
    
    return true;
}

void IOHIDInterface::close(  
								IOService *					client,
								IOOptionBits				options)
{
    if (_owner)
    _owner->close(client, options);
    
    super::close(client, options);
    
    _interruptTarget = NULL;
    _interruptAction = NULL;
    _interruptRefCon = NULL;    
    
}

OSString * IOHIDInterface::getTransport ()
{
    return _transportString;
}

OSString * IOHIDInterface::getManufacturer ()
{
    return _manufacturerString;
}

OSString * IOHIDInterface::getProduct ()
{
    return _productString;
}

OSString * IOHIDInterface::getSerialNumber ()
{
    return _serialNumberString;
}

UInt32 IOHIDInterface::getLocationID ()
{
    return _locationID;
}

UInt32 IOHIDInterface::getVendorID ()
{
    return _vendorID;
}

UInt32 IOHIDInterface::getVendorIDSource ()
{
    return _vendorIDSource;
}

UInt32 IOHIDInterface::getProductID ()
{
    return _productID;
}

UInt32 IOHIDInterface::getVersion ()
{
    return _version;
}

UInt32 IOHIDInterface::getCountryCode ()
{
    return _countryCode;
}

IOByteCount IOHIDInterface::getMaxReportSize (IOHIDReportType type)
{
    return _maxReportSize[type];
}


OSArray * IOHIDInterface::createMatchingElements (
                                OSDictionary *              matching, 
                                IOOptionBits                options __unused)
{
    UInt32                    count        = _elementArray->getCount();
    IOHIDElementPrivate *   element        = NULL;
    OSArray *                elements    = NULL;

    if ( count )
    {
        if ( matching )
        {
            elements = OSArray::withCapacity(count);

            for ( UInt32 i = 0; i < count; i ++)
            {
                // Compare properties.
                if (( element = (IOHIDElementPrivate *)_elementArray->getObject(i) )
                        && element->matchProperties(matching))
                {
                    elements->setObject(element);
                }
            }
        }
        else
            elements = OSArray::withArray(_elementArray);
    }

    return elements;
}

void IOHIDInterface::handleReport ( 
                                AbsoluteTime                timestamp,
                                IOMemoryDescriptor *        report,
                                IOHIDReportType             reportType,
                                UInt32                      reportID,
                                IOOptionBits                options __unused)
{
    if ( !_interruptAction )
        return;
        
    (*_interruptAction)(_interruptTarget, timestamp, report, reportType, reportID, _interruptRefCon);
}

    
IOReturn IOHIDInterface::setReport ( 
                                IOMemoryDescriptor *        report,
                                IOHIDReportType             reportType,
                                UInt32                      reportID,
                                IOOptionBits                options)
{
    return _owner ? _owner->setReport(report, reportType, (reportID | (options << 8))) : kIOReturnOffline;
}

IOReturn IOHIDInterface::getReport ( 
                                IOMemoryDescriptor *        report,
                                IOHIDReportType             reportType,
                                UInt32                      reportID,
                                IOOptionBits                options)
{
    return _owner ? _owner->getReport(report, reportType, (reportID | (options << 8))) : kIOReturnOffline;
}


IOReturn IOHIDInterface::setReport ( 
                                IOMemoryDescriptor *        report __unused,
                                IOHIDReportType             reportType __unused,
                                UInt32                      reportID __unused,
                                IOOptionBits                options __unused,
                                UInt32                      completionTimeout __unused,
                                CompletionAction *          completion __unused)
{
    return kIOReturnUnsupported;
}
    

IOReturn IOHIDInterface::getReport ( 
                                IOMemoryDescriptor *        report __unused,
                                IOHIDReportType             reportType __unused,
                                UInt32                      reportID __unused,
                                IOOptionBits                options __unused,
                                UInt32                      completionTimeout __unused,
                                CompletionAction *          completion __unused)
{
    return kIOReturnUnsupported;
}


OSMetaClassDefineReservedUsed(IOHIDInterface,  0);
UInt32 IOHIDInterface::getReportInterval ()
{
    return _reportInterval;
}

OSMetaClassDefineReservedUnused(IOHIDInterface,  1);
OSMetaClassDefineReservedUnused(IOHIDInterface,  2);
OSMetaClassDefineReservedUnused(IOHIDInterface,  3);
OSMetaClassDefineReservedUnused(IOHIDInterface,  4);
OSMetaClassDefineReservedUnused(IOHIDInterface,  5);
OSMetaClassDefineReservedUnused(IOHIDInterface,  6);
OSMetaClassDefineReservedUnused(IOHIDInterface,  7);
OSMetaClassDefineReservedUnused(IOHIDInterface,  8);
OSMetaClassDefineReservedUnused(IOHIDInterface,  9);
OSMetaClassDefineReservedUnused(IOHIDInterface, 10);
OSMetaClassDefineReservedUnused(IOHIDInterface, 11);
OSMetaClassDefineReservedUnused(IOHIDInterface, 12);
OSMetaClassDefineReservedUnused(IOHIDInterface, 13);
OSMetaClassDefineReservedUnused(IOHIDInterface, 14);
OSMetaClassDefineReservedUnused(IOHIDInterface, 15);
OSMetaClassDefineReservedUnused(IOHIDInterface, 16);
OSMetaClassDefineReservedUnused(IOHIDInterface, 17);
OSMetaClassDefineReservedUnused(IOHIDInterface, 18);
OSMetaClassDefineReservedUnused(IOHIDInterface, 19);
OSMetaClassDefineReservedUnused(IOHIDInterface, 20);
OSMetaClassDefineReservedUnused(IOHIDInterface, 21);
OSMetaClassDefineReservedUnused(IOHIDInterface, 22);
OSMetaClassDefineReservedUnused(IOHIDInterface, 23);
OSMetaClassDefineReservedUnused(IOHIDInterface, 24);
OSMetaClassDefineReservedUnused(IOHIDInterface, 25);
OSMetaClassDefineReservedUnused(IOHIDInterface, 26);
OSMetaClassDefineReservedUnused(IOHIDInterface, 27);
OSMetaClassDefineReservedUnused(IOHIDInterface, 28);
OSMetaClassDefineReservedUnused(IOHIDInterface, 29);
OSMetaClassDefineReservedUnused(IOHIDInterface, 30);
OSMetaClassDefineReservedUnused(IOHIDInterface, 31);
