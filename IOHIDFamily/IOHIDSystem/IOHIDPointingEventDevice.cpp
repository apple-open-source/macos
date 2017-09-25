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

#include <IOKit/IOLib.h>

#include "IOHIDFamilyPrivate.h"
#include "IOHIDPointingEventDevice.h"
#include "IOHIDPointing.h"
#include "IOHIDPrivateKeys.h"
#include "IOHIDDebug.h"

static UInt8 DefaultMouseDesc[] = {
  0x05, 0x01,                               // Usage Page (Generic Desktop)
  0x09, 0x02,                               // Usage (Mouse)
  0xA1, 0x01,                               // Collection (Application)
  0x05, 0x09,                               //   Usage Page (Button)
  0x19, 0x01,                               //   Usage Minimum........... (1)
  0x29, 0x08,                               //   Usage Maximum........... (8)
  0x15, 0x00,                               //   Logical Minimum......... (0)
  0x25, 0x01,                               //   Logical Maximum......... (1)
  0x95, 0x08,                               //   Report Count............ (8)
  0x75, 0x01,                               //   Report Size............. (1)
  0x81, 0x02,                               //   Input...................(Data, Variable, Absolute)
  0x95, 0x00,                               //   Report Count............ (0)
  0x75, 0x01,                               //   Report Size............. (1)
  0x81, 0x00,                               //   Input...................(Data, Array, Absolute)
  0x05, 0x01,                               //   Usage Page (Generic Desktop)
  0x09, 0x01,                               //   Usage (Pointer)
  0xA1, 0x00,                               //   Collection (Physical)
  0x09, 0x30,                               //     Usage (X)
  0x09, 0x31,                               //     Usage (Y)
  0x16, 0x01, 0x80,                         //     Logical Minimum......... (-32767)
  0x26, 0xFF, 0x7F,                         //     Logical Maximum......... (32767)
  0x36, 0x00, 0x00,                         //     Physical Minimum........ (0)
  0x46, 0x00, 0x00,                         //     Physical Maximum........ (0)
  0x65, 0x00,                               //     Unit.................... (0)
  0x55, 0x00,                               //     Unit Exponent........... (0)
  0x09, 0x32,                               //     Usage (Z)
  0x75, 0x10,                               //     Report Size............. (16)
  0x95, 0x03,                               //     Report Count............ (3)
  0x81, 0x06,                               //     Input...................(Data, Variable, Relative)
  0x95, 0x01,                               //     Report Count............ (1)
  0x75, 0x10,                               //     Report Size............. (16)
  0x16, 0x01, 0x80,                         //     Logical Minimum......... (-32767)
  0x26, 0xFF, 0x7F,                         //     Logical Maximum......... (32767)
  0x09, 0x38,                               //     Usage (Wheel)
  0x81, 0x06,                               //     Input...................(Data, Variable, Relative)
  0xC0,                                     //   End Collection
  0xC0,                                     // End Collection
};


#define super IOHIDDeviceShim

OSDefineMetaClassAndStructors( IOHIDPointingEventDevice, IOHIDDeviceShim )


IOHIDPointingEventDevice *
IOHIDPointingEventDevice::newPointingDeviceAndStart(IOService *owner)
{
  
  IOHIDPointingEventDevice * device = new IOHIDPointingEventDevice;
  
  if (device)
  {
    if (!device->initWithParameters(0, true)){
      device->release();
      return 0;
    }

    device->setProperty("HIDDefaultBehavior", kOSBooleanTrue);
    device->setProperty(kIOHIDCompatibilityInterface, kOSBooleanTrue);
    
    if ( device->attach(owner) )
    {
      if (!device->start(owner))
      {
        device->detach(owner);
        device->release();
        device = 0;
      }
    }
    else
    {
      device->release();
      device = 0;
    }
  }
  
  return device;
}


bool IOHIDPointingEventDevice::initWithLocation( UInt32 location )
{
  if (!super::initWithLocation(location))
    return false;
  
  _report = 0;
  
  return true;
}

void IOHIDPointingEventDevice::free()
{
  if (_report) _report->release();
  
  super::free();
}

bool IOHIDPointingEventDevice::handleStart( IOService * provider )
{
  if (!super::handleStart(provider)) {
    return false;
  }
  
  _report = IOBufferMemoryDescriptor::withCapacity(sizeof(GenericReport), kIODirectionNone, true);
  
  if (_report) {
      bzero(_report->getBytesNoCopy(), sizeof(GenericReport));
  }

  return (_report) ? true : false;
}


bool IOHIDPointingEventDevice::start( IOService * provider ) {
  bool success = false;
  if ( !super::start(provider) ) {
    HIDLogError ("failed");
    return false;
  }
  success = ((IOHIPointing*)provider)->open(
                                            this,
                                            kIOServiceSeize,
                                            0,
                                            (RelativePointerEventCallback) _relativePointerEvent,
                                            (AbsolutePointerEventCallback) _absolutePointerEvent,
                                            (ScrollWheelEventCallback)     _scrollWheelEvent
                                            );
  provider->setProperty(kIOHIDResetPointerKey, kOSBooleanTrue);
  return success;
}


IOReturn IOHIDPointingEventDevice::newReportDescriptor(
                                                  IOMemoryDescriptor ** descriptor ) const
{
  void 	*desc;
  
  if (!descriptor)
    return kIOReturnBadArgument;
  
  *descriptor = IOBufferMemoryDescriptor::withCapacity(
                                                       sizeof(DefaultMouseDesc),
                                                       kIODirectionNone,
                                                       true);
  
  if (! *descriptor)
    return kIOReturnNoMemory;
  
  desc = ((IOBufferMemoryDescriptor *)(*descriptor))->getBytesNoCopy();
  
  bcopy(DefaultMouseDesc, desc, sizeof(DefaultMouseDesc));
  
  return kIOReturnSuccess;
}

IOReturn IOHIDPointingEventDevice::getReport(IOMemoryDescriptor  *report,
                                             IOHIDReportType     reportType,
                                            IOOptionBits        options __unused )
{
  if (!report)
    return kIOReturnError;
  
  if ( reportType != kIOHIDReportTypeInput)
    return kIOReturnUnsupported;
  
  report->writeBytes(0, _report->getBytesNoCopy(), min(report->getLength(), _report->getLength()));
  return kIOReturnSuccess;
}


#define CONVERT_EV_TO_HW_BUTTONS(ev_buttons) ((ev_buttons & ~7) | ((ev_buttons & 3) << 1) | ((ev_buttons & 4) >> 2))

void IOHIDPointingEventDevice::_relativePointerEvent(
                                                      IOHIDPointingEventDevice * self,
                                                      int        buttons,
                                                      int        dx,
                                                      int        dy,
                                                      AbsoluteTime ts __unused,
                                                      OSObject * sender __unused,
                                                      void *     refcon __unused)
{
  self->postMouseEvent(CONVERT_EV_TO_HW_BUTTONS(buttons), dx, dy, 0, 0);
}

void IOHIDPointingEventDevice::_absolutePointerEvent(
                                                      IOHIDPointingEventDevice *   self __unused,
                                                      int             buttons __unused,
                                                      IOGPoint *      newLoc __unused,
                                                      IOGBounds *     bounds __unused,
                                                      bool            proximity __unused,
                                                      int             pressure __unused,
                                                      int             stylusAngle __unused,
                                                      AbsoluteTime    ts __unused,
                                                      OSObject *      sender __unused,
                                                      void *          refcon __unused)
{
  
}

void IOHIDPointingEventDevice::_scrollWheelEvent(
                                                      IOHIDPointingEventDevice * self,
                                                      short   deltaAxis1 __unused,
                                                      short   deltaAxis2 __unused,
                                                      short   deltaAxis3 __unused,
                                                      IOFixed fixedDelta1 __unused,
                                                      IOFixed fixedDelta2 __unused,
                                                      IOFixed fixedDelta3 __unused,
                                                      SInt32  pointDeltaAxis1,
                                                      SInt32  pointDeltaAxis2,
                                                      SInt32  pointDeltaAxis3 __unused,
                                                      UInt32  options __unused,
                                                      AbsoluteTime ts __unused,
                                                      OSObject * sender __unused,
                                                      void *     refcon __unused)
{
  self->postMouseEvent(0, 0, 0, pointDeltaAxis1, pointDeltaAxis2);
}

void IOHIDPointingEventDevice::postMouseEvent(UInt8 buttons, SInt16 x, SInt16 y, SInt16 vscroll, SInt16 hscroll)
{
  GenericReport *report = (GenericReport*)_report->getBytesNoCopy();
  
  if (!report) {
    return;
  }
  report->buttons = buttons;
  report->x = x;
  report->y = y;
  report->vscroll = vscroll;
  report->hscroll = hscroll;
  
  handleReport(_report);
}

OSString * IOHIDPointingEventDevice::newProductString() const
{
  OSString * string = 0;
  
  if ( !(string = super::newProductString()) )
    string = OSString::withCString("Virtual Mouse");
  
  return string;
}

OSNumber * IOHIDPointingEventDevice::newVendorIDNumber() const
{
    return OSNumber::withNumber(0x5ac, 32);
}

OSNumber * IOHIDPointingEventDevice::newProductIDNumber() const
{
    return OSNumber::withNumber(0xffff, 32);
}

OSString * IOHIDPointingEventDevice::newManufacturerString() const
{
    return OSString::withCString("Apple");
}

IOReturn IOHIDPointingEventDevice::message(UInt32 type, IOService * provider, void * argument __unused)
{
  IOReturn     status = kIOReturnSuccess;
  
  switch (type)
  {
    case kIOMessageServiceIsTerminated:
      if (provider) {
        provider->close( this );
      }
      break;
  }
  return status;
}

bool IOHIDPointingEventDevice::matchPropertyTable(OSDictionary *table, SInt32 * score __unused)
{
  if (super::matchPropertyTable(table, score) == false) {
    return false;
  }
  if (table->getObject(kIOHIDCompatibilityInterface)) {
    return true;
  }
  return false;
}

