/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
 
 /*
 * This is a basic HID driver to provide support for USB class 3
 * (HID) devices. The initial design allows the original OSX
 * drivers for the Apple USB mouse and keyboard to be used for
 * those devices instead of this driver.
 */

#include <libkern/OSByteOrder.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLib.h>		// For IOLog()...
#include <IOKit/IOMessage.h>

#include <IOKit/hid/IOHIDKeys.h>

#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBHIDDriver.h>

#define super IOHIDDevice

OSDefineMetaClassAndStructors(IOUSBHIDDriver, super)


// Do what is necessary to start device before probe is called.
//
bool 
IOUSBHIDDriver::init(OSDictionary *properties)
{
    if (!super::init(properties))
    {
        return false;
    }

    _interface = NULL;
    _buffer = 0;
    _retryCount = kHIDDriverRetryCount;
    _outstandingIO = 0;
    _needToClose = false;
    _maxReportSize = kMaxHIDReportSize;
    _maxOutReportSize = kMaxHIDReportSize;
    _outBuffer = 0;
    _deviceUsage = 0;
    _deviceUsagePage = 0;

    return true;
}


//
// Note: handleStart is not an IOKit thing, but is a IOHIDDevice thing. It is called from 
// IOHIDDevice::start after some initialization by that method, but before it calls registerService
// this method needs to open the provider, and make sure to have enough state (basically _interface
// and _device) to be able to get information from the device. we do NOT need to start the interrupt read
// yet, however
//
bool 
IOUSBHIDDriver::handleStart(IOService * provider)
{
    HIDPreparsedDataRef parseData;
    HIDCapabilities	myHIDCaps;
    UInt8 * 		myHIDDesc;
    UInt32 			hidDescSize;
    IOReturn 		err = kIOReturnSuccess;

    USBLog(7, "%s[%p]::handleStart", getName(), this);
    if( !super::handleStart(provider))
    {
        return false;
    }

    if( !provider->open(this))
    {
        USBError(1, "%s[%p]::start - unable to open provider. returning false", getName(), this);
        return (false);
    }

    _interface = OSDynamicCast(IOUSBInterface, provider);
    if (!_interface)
    {
        return false;
    }

    // remember my device
    _device = _interface->GetDevice();
    if (!_device)
    {
        return false;
    }

    // Get the size of the HID descriptor.
    hidDescSize = 0;
    err = GetHIDDescriptor(kUSBReportDesc, 0, NULL, &hidDescSize);
    if ((err != kIOReturnSuccess) || (hidDescSize == 0))
    {
        return false;		// Won't be able to set last properties.
    }
    
    myHIDDesc = (UInt8 *)IOMalloc(hidDescSize);
    if (myHIDDesc == NULL)
    {
        return false;
    }
    
    // Get the real report descriptor.
    err = GetHIDDescriptor(kUSBReportDesc, 0, myHIDDesc, &hidDescSize);
    if (err == kIOReturnSuccess) 
    {
        err = HIDOpenReportDescriptor(myHIDDesc, hidDescSize, &parseData, 0);
        if (err == kIOReturnSuccess) 
        {
            err = HIDGetCapabilities(parseData, &myHIDCaps);
            if (err == kIOReturnSuccess)
            {
                // Just get these values!
                _deviceUsage = myHIDCaps.usage;
                _deviceUsagePage = myHIDCaps.usagePage;
                
                _maxOutReportSize = myHIDCaps.outputReportByteLength;
                _maxReportSize = (myHIDCaps.inputReportByteLength > myHIDCaps.featureReportByteLength) ?
                    myHIDCaps.inputReportByteLength : myHIDCaps.featureReportByteLength;
            }

            HIDCloseReportDescriptor(parseData);
        }
    }

    if (myHIDDesc)
    {
        IOFree(myHIDDesc, hidDescSize);
    }

    // Set HID Manager properties in IO registry.
	// Will now be done by IOHIDDevice::start calling newTransportString, etc.
//    SetProperties();
    return true;
}


void
IOUSBHIDDriver::handleStop(IOService * provider)
{
    USBLog(7, "%s[%p]::handleStop", getName(), this);

    if (_outBuffer)
    {
        _outBuffer->release();
        _outBuffer = NULL;
    }

    if (_buffer)
    {
        _buffer->release();
        _buffer = NULL;
    }

    if (_deviceDeadCheckThread)
    {
        thread_call_cancel(_deviceDeadCheckThread);
        thread_call_free(_deviceDeadCheckThread);
    }
    
    if (_clearFeatureEndpointHaltThread)
    {
        thread_call_cancel(_clearFeatureEndpointHaltThread);
        thread_call_free(_clearFeatureEndpointHaltThread);
    }

    super::handleStop(provider);
}


void 
IOUSBHIDDriver::free()
{
    USBLog(7, "%s[%p]::free", getName(), this);

    super::free();    
}


void
IOUSBHIDDriver::processPacket(void *data, UInt32 size)
{
    IOLog("Should not be here, IOUSBHIDDriver: processPacket()\n");

    return;
}


// *************************************************************************************
// ************************ HID Driver Dispatch Table Functions ************************
// *************************************************************************************

IOReturn 
IOUSBHIDDriver::GetReport(UInt8 inReportType, UInt8 inReportID, UInt8 *vInBuf, UInt32 *vInSize)
{
    return kIOReturnSuccess;
}

IOReturn 
IOUSBHIDDriver::getReport(	IOMemoryDescriptor * report,
                                IOHIDReportType      reportType,
                                IOOptionBits         options )
{
    UInt8		reportID;
    IOReturn		ret;
    UInt8		usbReportType;
    IOUSBDevRequestDesc requestPB;
    
    IncrementOutstandingIO();
    
    // Get the reportID from the lower 8 bits of options
    //
    reportID = (UInt8) ( options & 0x000000ff);

    // And now save the report type
    //
    usbReportType = HIDMgr2USBReportType(reportType);
    
    //--- Fill out device request form
    //
    requestPB.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBInterface);
    requestPB.bRequest = kHIDRqGetReport;
    requestPB.wValue = (usbReportType << 8) | reportID;
    requestPB.wIndex = _interface->GetInterfaceNumber();
    requestPB.wLength = report->getLength();
    requestPB.pData = report;
    requestPB.wLenDone = 0;
    
    ret = _device->DeviceRequest(&requestPB);
    if ( ret != kIOReturnSuccess )
            USBLog(3, "%s[%p]::getReport request failed; err = 0x%x)", getName(), this, ret);
           
    DecrementOutstandingIO();
    return ret;
}


// DEPRECATED
//
IOReturn 
IOUSBHIDDriver::SetReport(UInt8 outReportType, UInt8 outReportID, UInt8 *vOutBuf, UInt32 vOutSize)
{
    return kIOReturnSuccess;
}

IOReturn 
IOUSBHIDDriver::setReport( IOMemoryDescriptor * 	report,
                            IOHIDReportType      	reportType,
                            IOOptionBits         	options)
{
    UInt8		reportID;
    IOReturn		ret;
    UInt8		usbReportType;
    IOUSBDevRequestDesc requestPB;
    
    IncrementOutstandingIO();
    
    // Get the reportID from the lower 8 bits of options
    //
    reportID = (UInt8) ( options & 0x000000ff);

    // And now save the report type
    //
    usbReportType = HIDMgr2USBReportType(reportType);
    
    // If we have an interrupt out pipe, try to use it for output type of reports.
    if ( kHIDOutputReport == usbReportType && _interruptOutPipe )
    {
        #if ENABLE_HIDREPORT_LOGGING
            USBLog(3, "%s[%p]::setReport sending out interrupt out pipe buffer (%p,%d):", getName(), this, report, report->getLength() );
            LogMemReport(report);
        #endif
        ret = _interruptOutPipe->Write(report);
        if (ret == kIOReturnSuccess)
        {       
            DecrementOutstandingIO();
            return ret;
        }
        else
        {
            USBLog(3, "%s[%p]::setReport _interruptOutPipe->Write failed; err = 0x%x)", getName(), this, ret);
        }
    }
        
    // If we did not succeed using the interrupt out pipe, we may still be able to use the control pipe.
    // We'll let the family check whether it's a disjoint descriptor or not (but right now it doesn't do it)
    //
    #if ENABLE_HIDREPORT_LOGGING
        USBLog(3, "%s[%p]::SetReport sending out control pipe:", getName(), this);
        LogMemReport( report);
    #endif

    //--- Fill out device request form
    requestPB.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    requestPB.bRequest = kHIDRqSetReport;
    requestPB.wValue = (usbReportType << 8) | reportID;
    requestPB.wIndex = _interface->GetInterfaceNumber();
    requestPB.wLength = report->getLength();
    requestPB.pData = report;
    requestPB.wLenDone = 0;
    
    ret = _device->DeviceRequest(&requestPB);
    if (ret != kIOReturnSuccess)
            USBLog(3, "%s[%p]::setReport request failed; err = 0x%x)", getName(), this, ret);
        
    DecrementOutstandingIO();
    return ret;
}


// HIDGetHIDDescriptor is used to get a specific HID descriptor from a HID device 
// (such as a report descriptor).
IOReturn 
IOUSBHIDDriver::GetHIDDescriptor(UInt8 inDescriptorType, UInt8 inDescriptorIndex, UInt8 *vOutBuf, UInt32 *vOutSize)
{
    IOUSBDevRequest 		requestPB;
    IOUSBHIDDescriptor 		*theHIDDesc;
    IOUSBHIDReportDesc 		*hidTypeSizePtr;	// For checking owned descriptors.
    UInt8 			*descPtr;
    UInt32 			providedBufferSize;
    UInt16 			descSize;
    UInt8 			descType;
    UInt8 			typeIndex;
    UInt8 			numberOwnedDesc;
    IOReturn 		err = kIOReturnSuccess;
    Boolean			foundIt;

    if (!vOutSize)
        return  kIOReturnBadArgument;
	
    if (!_interface)
    {
	USBLog(2, "%s[%p]::GetHIDDescriptor - no _interface", getName(), this);
	return kIOReturnNotFound;
    }

    // From the interface descriptor, get the HID descriptor.
    theHIDDesc = (IOUSBHIDDescriptor *)_interface->FindNextAssociatedDescriptor(NULL, kUSBHIDDesc);
    
    if (theHIDDesc == NULL)
    {
	USBLog(2, "%s[%p]::GetHIDDescriptor - FindNextAssociatedDescriptor(NULL, kUSBHIDDesc) failed", getName(), this);
        return kIOReturnNotFound;
    }

    // Remember the provided buffer size
    providedBufferSize = *vOutSize;
    // Are we looking for just the main HID descriptor?
    if (inDescriptorType == kUSBHIDDesc || (inDescriptorType == 0 && inDescriptorIndex == 0))
    {
	descSize = theHIDDesc->descLen;
	descPtr = (UInt8 *)theHIDDesc;

	// No matter what, set the return size to the actual size of the data.
	*vOutSize = descSize;
	
	// If the provided size is 0, they are just asking for the size, so don't return an error.
	if (providedBufferSize == 0)
		err = kIOReturnSuccess;
	// Otherwise, if the buffer too small, return buffer too small error.
	else if (descSize > providedBufferSize)
		err = kIOReturnNoSpace;
	// Otherwise, if the buffer nil, return that error.
	else if (vOutBuf == NULL)
		err = kIOReturnBadArgument;
	// Otherwise, looks good, so copy the deiscriptor.
	else
	{
	    //IOLog("  Copying HIDDesc w/ vOutBuf = 0x%x, descPtr = 0x%x, and descSize = 0x%x.\n", vOutBuf, descPtr, descSize);
	    memcpy(vOutBuf, descPtr, descSize);
	}
    }
    else
    {	// Looking for a particular type of descriptor.
        // The HID descriptor tells how many endpoint and report descriptors it contains.
        numberOwnedDesc = ((IOUSBHIDDescriptor *)theHIDDesc)->hidNumDescriptors;
        hidTypeSizePtr = (IOUSBHIDReportDesc *)&((IOUSBHIDDescriptor *)theHIDDesc)->hidDescriptorType;
        //IOLog("     %d owned descriptors start at %08x\n", numberOwnedDesc, (unsigned int)hidTypeSizePtr);
    
        typeIndex = 0;
        foundIt = false;
        err = kIOReturnNotFound;
        for (UInt8 i = 0; i < numberOwnedDesc; i++)
        {
            descType = hidTypeSizePtr->hidDescriptorType;

	    // Are we indexing for a specific type?
	    if (inDescriptorType != 0)
	    {
		if (inDescriptorType == descType)
                {
                    if (inDescriptorIndex == typeIndex)
                    {
                        foundIt = true;
                    }
                    else
                    {
                        typeIndex++;
                    }
                }
	    }
	    // Otherwise indexing across descriptors in general.
            // (If looking for any type, index must be 1 based or we'll get HID descriptor.)
	    else if (inDescriptorIndex == i + 1)
            {
                //IOLog("  said we found it because inDescriptorIndex = 0x%x.\n", inDescriptorIndex);
                typeIndex = i;
		foundIt = true;
            }
				
	    if (foundIt)
	    {
                err = kIOReturnSuccess;		// Maybe
                //IOLog("     Found the requested owned descriptor, %d.\n", i);
                descSize = (hidTypeSizePtr->hidDescriptorLengthHi << 8) + hidTypeSizePtr->hidDescriptorLengthLo;
                
                // Did we just want the size or the whole descriptor?
                // No matter what, set the return size to the actual size of the data.
                *vOutSize = descSize;	// OSX: Won't get back if we return an error!
		
                // If the provided size is 0, they are just asking for the size, so don't return an error.
                if (providedBufferSize == 0)
                    err = kIOReturnSuccess;
                // Otherwise, if the buffer too small, return buffer too small error.
                else if (descSize > providedBufferSize)
                    err = kIOReturnNoSpace;
                // Otherwise, if the buffer nil, return that error.
                else if (vOutBuf == NULL)
                    err = kIOReturnBadArgument;
                // Otherwise, looks good, so copy the descriptor.
                else
                {
		    if (!_device)
		    {
			USBLog(2, "%s[%p]::GetHIDDescriptor - no _device", getName(), this);
			return kIOReturnNotFound;
		    }

                    //IOLog("  Requesting new desscriptor.\n");
                    requestPB.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBInterface);
                    requestPB.bRequest = kUSBRqGetDescriptor;
                    requestPB.wValue = (inDescriptorType << 8) + typeIndex;		// type and index
                    requestPB.wIndex = _interface->GetInterfaceNumber();
                    requestPB.wLength = descSize;
                    requestPB.pData = vOutBuf;						// So we don't have to do any allocation here.
                    err = _device->DeviceRequest(&requestPB, 5000, 0);
                    if (err != kIOReturnSuccess)
                    {
                        USBLog(3, "%s[%p]::GetHIDDescriptor Final request failed; err = 0x%x", getName(), this, err);
                        return err;
                    }
                }
                break;	// out of for i loop.
	    }
            // Make sure we add 3 bytes not 4 regardless of struct alignment.
            hidTypeSizePtr = (IOUSBHIDReportDesc *)(((UInt8 *)hidTypeSizePtr) + 3);	
        }
    }
    return err;
}

IOReturn 
IOUSBHIDDriver::newReportDescriptor(IOMemoryDescriptor ** desc) const
{
    IOBufferMemoryDescriptor * bufferDesc = NULL;
    IOReturn ret = kIOReturnNoMemory;
    IOUSBHIDDriver * me = (IOUSBHIDDriver *) this;

    // Get the proper HID report descriptor size.
    UInt32 inOutSize = 0;
    ret = me->GetHIDDescriptor(kUSBReportDesc, 0, NULL, &inOutSize);

    if ( ret == kIOReturnSuccess &&  inOutSize != 0)
    {
        bufferDesc = IOBufferMemoryDescriptor::withCapacity(inOutSize, kIODirectionOutIn);
    }
    
    if (bufferDesc)
    {
        ret = me->GetHIDDescriptor(kUSBReportDesc, 0, (UInt8 *)bufferDesc->getBytesNoCopy(), &inOutSize);
	
		if ( ret != kIOReturnSuccess )
		{
			bufferDesc->release();
			bufferDesc = NULL;
		}
    }

    *desc = bufferDesc;
    
    return ret;
}


OSString * 
IOUSBHIDDriver::newTransportString() const
{
    return OSString::withCString("USB");
}


OSNumber * 
IOUSBHIDDriver::newPrimaryUsageNumber() const
{
    return OSNumber::withNumber(_deviceUsage, 32);
}


OSNumber * 
IOUSBHIDDriver::newPrimaryUsagePageNumber() const
{
    return OSNumber::withNumber(_deviceUsagePage, 32);
}


OSNumber * 
IOUSBHIDDriver::newVendorIDNumber() const
{
    UInt16 vendorID = 0;
    
    if (_device != NULL)
        vendorID = _device->GetVendorID();
        
    return OSNumber::withNumber(vendorID, 16);
}


OSNumber * 
IOUSBHIDDriver::newProductIDNumber() const
{
    UInt16 productID = 0;
    
    if (_device != NULL)
        productID = _device->GetProductID();
        
    return OSNumber::withNumber(productID, 16);
}


OSNumber * 
IOUSBHIDDriver::newVersionNumber() const
{
    UInt16 releaseNum = 0;
    
    if (_device != NULL)
        releaseNum = _device->GetDeviceRelease();
        
    return OSNumber::withNumber(releaseNum, 16);
}


UInt32 
IOUSBHIDDriver::getMaxReportSize()
{
    return _maxReportSize;
}


OSString * 
IOUSBHIDDriver::newManufacturerString() const
{
    char 	manufacturerString[256];
    UInt32 	strSize;
    UInt8 	index;
    IOReturn	err;
    
    manufacturerString[0] = 0;

    index = _device->GetManufacturerStringIndex();
    strSize = sizeof(manufacturerString);
    
    err = GetIndexedString(index, (UInt8 *)manufacturerString, &strSize);
    
    if ( err == kIOReturnSuccess )
        return OSString::withCString(manufacturerString);
    else
        return NULL;
}


OSString * 
IOUSBHIDDriver::newProductString() const
{
    char 	productString[256];
    UInt32 	strSize;
    UInt8 	index;
    IOReturn	err;
    
    productString[0] = 0;

    index = _device->GetProductStringIndex();
    strSize = sizeof(productString);
    
    err = GetIndexedString(index, (UInt8 *)productString, &strSize);
    
    if ( err == kIOReturnSuccess )
        return OSString::withCString(productString);
    else
        return NULL;
}


OSString * 
IOUSBHIDDriver::newSerialNumberString() const
{
    char 	serialNumberString[256];
    UInt32 	strSize;
    UInt8 	index;
    IOReturn	err;
    
    serialNumberString[0] = 0;

    index = _device->GetSerialNumberStringIndex();
    strSize = sizeof(serialNumberString);
    
    err = GetIndexedString(index, (UInt8 *)serialNumberString, &strSize);
    
    if ( err == kIOReturnSuccess )
        return OSString::withCString(serialNumberString);
    else
        return NULL;
}


OSNumber * 
IOUSBHIDDriver::newLocationIDNumber() const
{
    OSNumber * newLocationID = NULL;
    
    if (_interface != NULL)
    {
        OSNumber * locationID = (OSNumber *)_interface->getProperty(kUSBDevicePropertyLocationID);
        if ( locationID )
            // I should be able to just duplicate locationID, but no OSObject::clone() or such.
            newLocationID = OSNumber::withNumber(locationID->unsigned32BitValue(), 32);
    }
        
    return newLocationID;
}


IOReturn 
IOUSBHIDDriver::GetIndexedString(UInt8 index, UInt8 *vOutBuf, UInt32 *vOutSize, UInt16 lang) const
{
    char 	strBuf[256];
    UInt16 	strLen = sizeof(strBuf) - 1;	// GetStringDescriptor MaxLen = 255
    UInt32 	outSize = *vOutSize;
    IOReturn 	err;
        
    // Valid string index?
    if (index == 0)
    {
            return kIOReturnBadArgument;
    }

    // Valid language?
    if (lang == 0)
    {
        lang = 0x409;	// Default is US English.
    }

    err = _device->GetStringDescriptor((UInt8)index, strBuf, strLen, (UInt16)lang);
    // When string is returned, it has been converted from Unicode and is null terminated!

    if (err != kIOReturnSuccess)
    {
        return err;
    }
    
    // We return the length of the string plus the null terminator,
    // but don't say a null string is 1 byte long.
    strLen = (strBuf[0] == 0) ? 0 : strlen(strBuf) + 1;

    if (outSize == 0)
    {
        *vOutSize = strLen;
        return kIOReturnSuccess;
    }
    else if (outSize < strLen)
    {
        return kIOReturnMessageTooLarge;
    }

    strcpy((char *)vOutBuf, strBuf);
    *vOutSize = strLen;
    return kIOReturnSuccess;
}

OSString * 
IOUSBHIDDriver::newIndexedString(UInt8 index) const
{
    char string[256];
    UInt32 strSize;
    IOReturn	err = kIOReturnSuccess;
    
    string[0] = 0;
    strSize = sizeof(string);

    err = GetIndexedString(index, (UInt8 *)string, &strSize );
    
    if ( err == kIOReturnSuccess )
        return OSString::withCString(string);
    else
        return NULL;
}


IOReturn 
IOUSBHIDDriver::message( UInt32 type, IOService * provider,  void * argument )
{
    IOReturn	err = kIOReturnSuccess;
    
    err = super::message (type, provider, argument);
    
    switch ( type )
    {
        case kIOMessageServiceIsTerminated:
	    USBLog(5, "%s[%p]: service is terminated - ignoring", getName(), this);
	    break;

      case kIOUSBMessagePortHasBeenReset:
	    USBLog(3, "%s[%p]: received kIOUSBMessagePortHasBeenReset", getName(), this);
            _retryCount = kHIDDriverRetryCount;
            _deviceIsDead = FALSE;
            _deviceHasBeenDisconnected = FALSE;
            
            IncrementOutstandingIO();
            err = _interruptPipe->Read(_buffer, &_completion);
            if (err != kIOReturnSuccess)
            {
                DecrementOutstandingIO();
                USBLog(3, "%s[%p]::message - err (%x) in interrupt read", getName(), this, err);
                // _interface->close(this); will be done in didTerminate
            }
            break;
  
        default:
            break;
    }
    
    return kIOReturnSuccess;
}


bool
IOUSBHIDDriver::willTerminate( IOService * provider, IOOptionBits options )
{
    // this method is intended to be used to stop any pending I/O and to make sure that 
    // we have begun getting our callbacks in order. by the time we get here, the 
    // isInactive flag is set, so we really are marked as being done. we will do in here
    // what we used to do in the message method (this happens first)
    USBLog(3, "%s[%p]::willTerminate isInactive = %d", getName(), this, isInactive());
    if (_interruptPipe)
	_interruptPipe->Abort();

    return super::willTerminate(provider, options);
}



bool
IOUSBHIDDriver::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    // this method comes at the end of the termination sequence. Hopefully, all of our outstanding IO is complete
    // in which case we can just close our provider and IOKit will take care of the rest. Otherwise, we need to 
    // hold on to the device and IOKit will terminate us when we close it later
    USBLog(3, "%s[%p]::didTerminate isInactive = %d, outstandingIO = %d", getName(), this, isInactive(), _outstandingIO);
    if (!_outstandingIO)
	_interface->close(this);
    else
	_needToClose = true;
    return super::didTerminate(provider, options, defer);
}



bool
IOUSBHIDDriver::start(IOService *provider)
{
    IOReturn 		err = kIOReturnSuccess;
    IOWorkLoop		*wl = NULL;
    
    USBLog(7, "%s[%p]::start", getName(), this);
    IncrementOutstandingIO();			// make sure that once we open we don't close until start is open
    if (super::start(provider))
    do {
	// OK - at this point IOHIDDevice has successfully started, and now we need to start out interrupt pipe
	// read. we have not initialized a bunch of this stuff yet, because we needed to wait to see if
	// IOHIDDevice::start succeeded or not
        IOUSBFindEndpointRequest	request;

        USBLog(7, "%s[%p]::start - getting _gate", getName(), this);
        _gate = IOCommandGate::commandGate(this);

        if(!_gate)
        {
            USBError(1, "%s[%p]::start - unable to create command gate", getName(), this);
            break;
        }

        wl = getWorkLoop();
        if (!wl)
        {
                USBError(1, "%s[%p]::start - unable to find my workloop", getName(), this);
                break;
        }
	
        if (wl->addEventSource(_gate) != kIOReturnSuccess)
        {
            USBError(1, "%s[%p]::start - unable to add gate to work loop", getName(), this);
            break;
        }

        // Errata for ALL Saitek devices.  Do a SET_IDLE 0 call
        if ( (_device->GetVendorID()) == 0x06a3 )
            SetIdleMillisecs(0);

        request.type = kUSBInterrupt;
        request.direction = kUSBOut;
        _interruptOutPipe = _interface->FindNextPipe(NULL, &request);

        request.type = kUSBInterrupt;
        request.direction = kUSBIn;
        _interruptPipe = _interface->FindNextPipe(NULL, &request);

        if(!_interruptPipe)
        {
            USBError(1, "%s[%p]::start - unable to get interrupt pipe", getName(), this);
            break;
        }

        _maxReportSize = getMaxReportSize();
        if (_maxReportSize)
        {
            _buffer = IOBufferMemoryDescriptor::withCapacity(_maxReportSize, kIODirectionIn);
            if ( !_buffer )
            {
                USBError(1, "%s[%p]::start - unable to get create buffer", getName(), this);
                break;
            }
        }


        // allocate a thread_call structure
        _deviceDeadCheckThread = thread_call_allocate((thread_call_func_t)CheckForDeadDeviceEntry, (thread_call_param_t)this);
        _clearFeatureEndpointHaltThread = thread_call_allocate((thread_call_func_t)ClearFeatureEndpointHaltEntry, (thread_call_param_t)this);
        
        if ( !_deviceDeadCheckThread || !_clearFeatureEndpointHaltThread )
        {
            USBError(1, "[%s]%p: could not allocate all thread functions", getName(), this);
            break;
        }

        err = StartFinalProcessing();
        if (err != kIOReturnSuccess)
        {
            USBError(1, "%s[%p]::start - err (%x) in StartFinalProcessing", getName(), this, err);
            break;
        }

        USBError(1, "%s[%p]::start -  USB HID Device @ %d (0x%x)", getName(), this, _device->GetAddress(), strtol(_device->getLocation(), (char **)NULL, 16));
        
        DecrementOutstandingIO();		// release the hold we put on at the beginning
	return true;
    } while (false);
    
    USBError(1, "%s[%p]::start - aborting startup", getName(), this);
    if (_gate)
    {
	if (wl)
	    wl->removeEventSource(_gate);
	    
	_gate->release();
	_gate = NULL;
    }
    
    if (_deviceDeadCheckThread)
        thread_call_free(_deviceDeadCheckThread);
    
    if (_clearFeatureEndpointHaltThread)
        thread_call_free(_clearFeatureEndpointHaltThread);

    if (_interface)
	_interface->close(this);
	
    DecrementOutstandingIO();		// release the hold we put on at the beginning
    return false;
}


//=============================================================================================
//
//  InterruptReadHandlerEntry is called to process any data coming in through our interrupt pipe
//
//=============================================================================================
//
void 
IOUSBHIDDriver::InterruptReadHandlerEntry(OSObject *target, void *param, IOReturn status, UInt32 bufferSizeRemaining)
{
    IOUSBHIDDriver *	me = OSDynamicCast(IOUSBHIDDriver, target);

    if (!me)
        return;
    
    me->InterruptReadHandler(status, bufferSizeRemaining);
    me->DecrementOutstandingIO();
}


void 
IOUSBHIDDriver::InterruptReadHandler(IOReturn status, UInt32 bufferSizeRemaining)
{
    bool		queueAnother = true;
    bool		timeToGoAway = false;
    IOReturn		err = kIOReturnSuccess;

    switch (status)
    {
        case kIOReturnOverrun:
            USBLog(3, "%s[%p]::InterruptReadHandler kIOReturnOverrun error", getName(), this);
            // This is an interesting error, as we have the data that we wanted and more...  We will use this
            // data but first we need to clear the stall and reset the data toggle on the device.  We will not
            // requeue another read because our _clearFeatureEndpointHaltThread will requeue it.  We then just 
            // fall through to the kIOReturnSuccess case.
            // 01-18-02 JRH If we are inactive, then ignore this
	    if (!isInactive())
	    {
		//
		// First, clear the halted bit in the controller
		//
		_interruptPipe->ClearStall();
		
		// And call the device to reset the endpoint as well
		//
		IncrementOutstandingIO();
		thread_call_enter(_clearFeatureEndpointHaltThread);
	    }
            queueAnother = false;
            timeToGoAway = false;
            
            // Fall through to process the data.
            
        case kIOReturnSuccess:
            // Reset the retry count, since we had a successful read
            //
            _retryCount = kHIDDriverRetryCount;

            // Handle the data
            //
#if ENABLE_HIDREPORT_LOGGING
            USBLog(6, "%s[%p]::InterruptReadHandler report came in:", getName(), this);
            LogMemReport(_buffer);
#endif
            handleReport(_buffer);
	    
	    if (isInactive())
		queueAnother = false;
            
            break;

        case kIOReturnNotResponding:
            USBLog(3, "%s[%p]::InterruptReadHandler kIOReturnNotResponding error", getName(), this);
            // If our device has been disconnected or we're already processing a
            // terminate message, just go ahead and close the device (i.e. don't
            // queue another read.  Otherwise, go check to see if the device is
            // around or not. 
            //
            if ( _deviceHasBeenDisconnected || isInactive() )
            {
                  queueAnother = false;
                  timeToGoAway = true;
            }
            else
            {
                USBLog(3, "%s[%p]::InterruptReadHandler Checking to see if HID device is still connected", getName(), this);
                IncrementOutstandingIO();
                thread_call_enter(_deviceDeadCheckThread);
                
                // Before requeueing, we need to clear the stall
                //
                _interruptPipe->ClearStall();
            }
                
            break;
            
	case kIOReturnAborted:
	    // This generally means that we are done, because we were unplugged, but not always
            //
            if (isInactive() || _deviceIsDead )
	    {
                USBLog(3, "%s[%p]::InterruptReadHandler error kIOReturnAborted (expected)", getName(), this);
		queueAnother = false;
                timeToGoAway = true;
	    }
	    else
            {
                USBLog(3, "%s[%p]::InterruptReadHandler error kIOReturnAborted. Try again.", getName(), this);
            }
	    break;
            
        case kIOReturnUnderrun:
        case kIOUSBPipeStalled:
        case kIOUSBLinkErr:
        case kIOUSBNotSent2Err:
        case kIOUSBNotSent1Err:
        case kIOUSBBufferUnderrunErr:
        case kIOUSBBufferOverrunErr:
        case kIOUSBWrongPIDErr:
        case kIOUSBPIDCheckErr:
        case kIOUSBDataToggleErr:
        case kIOUSBBitstufErr:
        case kIOUSBCRCErr:
            // These errors will halt the endpoint, so before we requeue the interrupt read, we have
            // to clear the stall at the controller and at the device.  We will not requeue the read
            // until after we clear the ENDPOINT_HALT feature.  We need to do a callout thread because
            // we are executing inside the gate here and we cannot issue a synchronous request.
            USBLog(3, "%s[%p]::InterruptReadHandler OHCI error (0x%x) reading interrupt pipe", getName(), this, status);
            // 01-18-02 JRH If we are inactive, then ignore this
	    if (!isInactive())
	    {
		// First, clear the halted bit in the controller
		//
		_interruptPipe->ClearStall();
		
		// And call the device to reset the endpoint as well
		//
		IncrementOutstandingIO();
		thread_call_enter(_clearFeatureEndpointHaltThread);
            }
            // We don't want to requeue the read here, AND we don't want to indicate that we are done
            //
            queueAnother = false;
            break;
            
        default:
            // We should handle other errors more intelligently, but
            // for now just return and assume the error is recoverable.
            USBLog(3, "%s[%p]::InterruptReadHandler error (0x%x) reading interrupt pipe", getName(), this, status);
	    if (isInactive())
		queueAnother = false;
            
            break;
    }

    if ( queueAnother )
    {
        // Queue up another one before we leave.
        //
        IncrementOutstandingIO();
        err = _interruptPipe->Read(_buffer, &_completion);
        if ( err != kIOReturnSuccess)
        {
            // This is bad.  We probably shouldn't continue on from here.
            USBError(1, "%s[%p]::InterruptReadHandler immediate error 0x%x queueing read\n", getName(), this, err);
            DecrementOutstandingIO();
            timeToGoAway = true;
        }
    }
}


//=============================================================================================
//
//  CheckForDeadDevice is called when we get a kIODeviceNotResponding error in our interrupt pipe.
//  This can mean that (1) the device was unplugged, or (2) we lost contact
//  with our hub.  In case (1), we just need to close the driver and go.  In
//  case (2), we need to ask if we are still attached.  If we are, then we update 
//  our retry count.  Once our retry count (3 from the 9 sources) are exhausted, then we
//  issue a DeviceReset to our provider, with the understanding that we will go
//  away (as an interface).
//
//=============================================================================================
//
void 
IOUSBHIDDriver::CheckForDeadDeviceEntry(OSObject *target)
{
    IOUSBHIDDriver *	me = OSDynamicCast(IOUSBHIDDriver, target);
    
    if (!me)
        return;
        
    me->CheckForDeadDevice();
    me->DecrementOutstandingIO();
}

void 
IOUSBHIDDriver::CheckForDeadDevice()
{
    IOReturn			err = kIOReturnSuccess;

    // Are we still connected?  Don't check again if we're already
    // checking
    //
    if ( _interface && _device && !_deviceDeadThreadActive)
    {
        _deviceDeadThreadActive = TRUE;

        err = _device->message(kIOUSBMessageHubIsDeviceConnected, NULL, 0);
    
        if ( kIOReturnSuccess == err )
        {
            // Looks like the device is still plugged in.  Have we reached our retry count limit?
            //
            if ( --_retryCount == 0 )
            {
                _deviceIsDead = TRUE;
                USBLog(3, "%s[%p]: Detected an kIONotResponding error but still connected.  Resetting port", getName(), this);
                
                if (_interruptPipe)
                    _interruptPipe->Abort();  // This will end up closing the interface as well.

                // OK, let 'er rip.  Let's do the reset thing
                //
                    _device->ResetDevice();
                    
            }
        }
        else
        {
            // Device is not connected -- our device has gone away.  The message kIOServiceIsTerminated
            // will take care of shutting everything down.  
            //
            _deviceHasBeenDisconnected = TRUE;
            USBLog(5, "%s[%p]: CheckForDeadDevice: device has been unplugged", getName(), this);
        }
        _deviceDeadThreadActive = FALSE;
    }
}


//=============================================================================================
//
//  ClearFeatureEndpointHaltEntry is called when we get an OHCI error from our interrupt read
//  (except for kIOReturnNotResponding  which will check for a dead device).  In these cases
//  we need to clear the halted bit in the controller AND we need to reset the data toggle on the
//  device.
//
//=============================================================================================
//
void 
IOUSBHIDDriver::ClearFeatureEndpointHaltEntry(OSObject *target)
{
    IOUSBHIDDriver *	me = OSDynamicCast(IOUSBHIDDriver, target);
    
    if (!me)
        return;
        
    me->ClearFeatureEndpointHalt();
    me->DecrementOutstandingIO();
}

void 
IOUSBHIDDriver::ClearFeatureEndpointHalt( )
{
    IOReturn			status;
    IOUSBDevRequest		request;
    
    // Clear out the structure for the request
    //
    bzero( &request, sizeof(IOUSBDevRequest));

    // Build the USB command to clear the ENDPOINT_HALT feature for our interrupt endpoint
    //
    request.bmRequestType 	= USBmakebmRequestType(kUSBNone, kUSBStandard, kUSBEndpoint);
    request.bRequest 		= kUSBRqClearFeature;
    request.wValue		= 0;	// Zero is ENDPOINT_HALT
    request.wIndex		= _interruptPipe->GetEndpointNumber() | 0x80 ; // bit 7 sets the direction of the endpoint to IN
    request.wLength		= 0;
    request.pData 		= NULL;

    // Send the command over the control endpoint
    //
    status = _device->DeviceRequest(&request, 5000, 0);

    if ( status )
    {
        USBLog(3, "%s[%p]::ClearFeatureEndpointHalt -  DeviceRequest returned: 0x%x", getName(), this, status);
    }
    
    // Now that we've sent the ENDPOINT_HALT clear feature, we need to requeue the interrupt read.  Note
    // that we are doing this even if we get an error from the DeviceRequest.
    //
    IncrementOutstandingIO();
    status = _interruptPipe->Read(_buffer, &_completion);
    if ( status != kIOReturnSuccess)
    {
        // This is bad.  We probably shouldn't continue on from here.
        USBLog(3, "%s[%p]::ClearFeatureEndpointHalt -  immediate error %d queueing read", getName(), this, status);
        DecrementOutstandingIO();
        // _interface->close(this); this will be done in didTerminate
    }
}



IOReturn
IOUSBHIDDriver::ChangeOutstandingIO(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
    IOUSBHIDDriver *me = OSDynamicCast(IOUSBHIDDriver, target);
    UInt32	direction = (UInt32)param1;
    
    if (!me)
    {
	USBLog(1, "IOUSBHIDDriver::ChangeOutstandingIO - invalid target");
	return kIOReturnSuccess;
    }
    switch (direction)
    {
	case 1:
	    me->_outstandingIO++;
	    break;
	    
	case -1:
	    if (!--me->_outstandingIO && me->_needToClose)
	    {
		USBLog(3, "%s[%p]::ChangeOutstandingIO isInactive = %d, outstandingIO = %d - closing device", me->getName(), me, me->isInactive(), me->_outstandingIO);
		me->_interface->close(me);
	    }
	    break;
	    
	default:
	    USBLog(1, "%s[%p]::ChangeOutstandingIO - invalid direction", me->getName(), me);
    }
    return kIOReturnSuccess;
}


void
IOUSBHIDDriver::DecrementOutstandingIO(void)
{
    if (!_gate)
    {
	if (!--_outstandingIO && _needToClose)
	{
	    USBLog(3, "%s[%p]::DecrementOutstandingIO isInactive = %d, outstandingIO = %d - closing device", getName(), this, isInactive(), _outstandingIO);
	    _interface->close(this);
	}
	return;
    }
    _gate->runAction(ChangeOutstandingIO, (void*)-1);
}


void
IOUSBHIDDriver::IncrementOutstandingIO(void)
{
    if (!_gate)
    {
	_outstandingIO++;
	return;
    }
    _gate->runAction(ChangeOutstandingIO, (void*)1);
}


//
// StartFinalProcessing
//
// This method may have a confusing name. This is not talking about Final Processing of the driver (as in
// the driver is going away or something like that. It is talking about FinalProcessing of the start method.
// It is called as the very last thing in the start method, and by default it issues a read on the interrupt
// pipe.
//
IOReturn
IOUSBHIDDriver::StartFinalProcessing(void)
{
    IOReturn	err = kIOReturnSuccess;

    _completion.target = (void *)this;
    _completion.action = (IOUSBCompletionAction) &IOUSBHIDDriver::InterruptReadHandlerEntry;
    _completion.parameter = (void *)0;

    IncrementOutstandingIO();
    err = _interruptPipe->Read(_buffer, &_completion);
    if (err != kIOReturnSuccess)
    {
        DecrementOutstandingIO();
        USBError(1, "%s[%p]::StartFinalProcessing - err (%x) in interrupt read, retain count %d after release", getName(), this, err, getRetainCount());
    }
    return err;
}


IOReturn
IOUSBHIDDriver::SetIdleMillisecs(UInt16 msecs)
{
    IOReturn    		err = kIOReturnSuccess;
    IOUSBDevRequest		request;

    request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    request.bRequest = kHIDRqSetIdle;  //See USBSpec.h
    request.wValue = (msecs/4) << 8;
    request.wIndex = _interface->GetInterfaceNumber();
    request.wLength = 0;
    request.pData = NULL;

    err = _device->DeviceRequest(&request, 5000, 0);
    if (err != kIOReturnSuccess)
    {
        USBLog(3, "%s[%p]: IOUSBHIDDriver::SetIdleMillisecs returned error 0x%x",getName(), this, err);
    }

    return err;

}


#if ENABLE_HIDREPORT_LOGGING
void 
IOUSBHIDDriver::LogMemReport(IOMemoryDescriptor * reportBuffer)
{
    IOByteCount reportSize;
    char outBuffer[1024];
    char in[1024];
    char *out;
    char inChar;
    
    out = (char *)&outBuffer;
    reportSize = reportBuffer->getLength();
    reportBuffer->readBytes(0, in, reportSize );
    if (reportSize > 256) reportSize = 256;
    
    for (unsigned int i = 0; i < reportSize; i++)
    {
        inChar = in[i];
        *out++ = ' ';
        *out++ = GetHexChar(inChar >> 4);
        *out++ = GetHexChar(inChar & 0x0F);
    }
    
    *out = 0;
    
    USBLog(6, outBuffer);
}

char 
IOUSBHIDDriver::GetHexChar(char hexChar)
{
    char hexChars[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
    return hexChars[0x0F & hexChar];
}
#endif


OSMetaClassDefineReservedUnused(IOUSBHIDDriver,  0);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver,  1);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver,  2);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver,  3);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver,  4);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver,  5);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver,  6);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver,  7);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver,  8);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver,  9);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 10);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 11);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 12);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 13);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 14);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 15);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 16);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 17);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 18); 
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 19);

