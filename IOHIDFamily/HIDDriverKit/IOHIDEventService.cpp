//
//  IOHIDEventService.cpp
//  HIDDriverKit
//
//  Created by dekom on 1/25/19.
//

#include <assert.h>
#include <DriverKit/DriverKit.h>
#include <DriverKit/OSCollections.h>
#include <HIDDriverKit/HIDDriverKit_Private.h>


void
IMPL(IOHIDEventService, SetLED)
{
    return;
}

void IOHIDEventService::dispatchEvent(IOHIDEvent *event)
{
    return;
}

kern_return_t IOHIDEventService::dispatchKeyboardEvent(uint64_t timeStamp,
                                                       uint32_t usagePage,
                                                       uint32_t usage,
                                                       uint32_t value,
                                                       IOOptionBits options,
                                                       bool repeat)
{
    return _DispatchKeyboardEvent(timeStamp,
                                  usagePage,
                                  usage,
                                  value,
                                  options,
                                  repeat);
}

kern_return_t IOHIDEventService::dispatchRelativePointerEvent(uint64_t timeStamp,
                                                              IOFixed dx,
                                                              IOFixed dy,
                                                              uint32_t buttonState,
                                                              IOOptionBits options,
                                                              bool accelerate)
{
    return _DispatchRelativePointerEvent(timeStamp,
                                         dx,
                                         dy,
                                         buttonState,
                                         options,
                                         accelerate);
}

kern_return_t IOHIDEventService::dispatchAbsolutePointerEvent(uint64_t timeStamp,
                                                              IOFixed x,
                                                              IOFixed y,
                                                              uint32_t buttonState,
                                                              IOOptionBits options,
                                                              bool accelerate)
{
    return _DispatchAbsolutePointerEvent(timeStamp,
                                         x,
                                         y,
                                         buttonState,
                                         options,
                                         accelerate);
}

kern_return_t IOHIDEventService::dispatchRelativeScrollWheelEvent(uint64_t timeStamp,
                                                                  IOFixed dx,
                                                                  IOFixed dy,
                                                                  IOFixed dz,
                                                                  IOOptionBits options,
                                                                  bool accelerate)
{
    return _DispatchRelativeScrollWheelEvent(timeStamp,
                                             dx,
                                             dy,
                                             dz,
                                             options,
                                             accelerate);
}

kern_return_t IOHIDEventService::dispatchDigitizerStylusEvent(
                                        uint64_t timeStamp,
                                        IOHIDDigitizerStylusData *stylusData)
{
    return kIOReturnUnsupported;
}

kern_return_t IOHIDEventService::dispatchDigitizerTouchEvent(
                                            uint64_t timeStamp,
                                            IOHIDDigitizerTouchData *touchData,
                                            uint32_t touchDataCount)
{
    return kIOReturnUnsupported;
}
