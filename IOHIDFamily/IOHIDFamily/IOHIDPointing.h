/*
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
#ifndef _IOHIDPOINTING_H
#define _IOHIDPOINTING_H

#include <IOKit/hidsystem/IOHIDTypes.h>
#include "IOHITablet.h"
#include "IOHIDEventService.h"
#include "IOHIDevicePrivateKeys.h"

class IOHIDPointing : public IOHITablet
{
    OSDeclareDefaultStructors(IOHIDPointing);

private:
    IOHIDEventService *      _provider;

    IOItemCount             _numButtons;
    IOFixed                 _resolution;
    IOFixed                 _scrollResolution;
    bool                    _isDispatcher;

public:
    static UInt16           generateDeviceID();

    // Allocator
    static IOHIDPointing * Pointing(
                                UInt32          buttonCount,
                                IOFixed         pointerResolution,
                                IOFixed         scrollResolution,
                                bool            isDispatcher);

    virtual bool initWithMouseProperties(
                                UInt32          buttonCount,
                                IOFixed         pointerResolution,
                                IOFixed         scrollResolution,
                                bool            isDispatcher);

    virtual bool start(IOService * provider);
    virtual void stop(IOService * provider);


    virtual void dispatchAbsolutePointerEvent(
                                AbsoluteTime                timeStamp,
                                IOGPoint *                  newLoc,
                                IOGBounds *                 bounds,
                                UInt32                      buttonState,
                                bool                        inRange,
                                SInt32                      tipPressure,
                                SInt32                      tipPressureMin,
                                SInt32                      tipPressureMax,
                                IOOptionBits                options = 0);

	virtual void dispatchRelativePointerEvent(
                                AbsoluteTime                timeStamp,
								SInt32                      dx,
								SInt32                      dy,
								UInt32                      buttonState,
								IOOptionBits                options = 0);

	virtual void dispatchScrollWheelEvent(
                                AbsoluteTime                timeStamp,
								SInt32                      deltaAxis1,
								SInt32                      deltaAxis2,
								UInt32                      deltaAxis3,
								IOOptionBits                options = 0);

    virtual void dispatchTabletEvent(
                                    NXEventData *           tabletEvent,
                                    AbsoluteTime            ts);

    virtual void dispatchProximityEvent(
                                    NXEventData *           proximityEvent,
                                    AbsoluteTime            ts);

protected:
  virtual IOItemCount buttonCount();
  virtual IOFixed     resolution();

private:
  // This is needed to pass properties defined
  // in IOHIDDevice to the nub layer
  void	  setupProperties();

};

#endif /* !_IOHIDPOINTING_H */
