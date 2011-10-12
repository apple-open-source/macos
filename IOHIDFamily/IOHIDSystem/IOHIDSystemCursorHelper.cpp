/*
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 2011 Apple Computer, Inc.  All Rights Reserved.
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
#include "IOHIDSystemCursorHelper.h"

//===========================================================================
boolean_t IOHIDSystemCursorHelper::init()
{   
    location.fromIntFloor(0, 0);
    locationDelta.fromIntFloor(0, 0);
    locationDeltaPosting.fromIntFloor(0, 0);
    locationDeltaAccumulated.fromIntFloor(0, 0);
    screenLocation.fromIntFloor(0, 0);
    expectedCountValue.fromIntFloor(0);
    eventCount = 0; 
    eventCountPosting = 0; 
    overdueTime = 0;
    
    return true; 
}

//===========================================================================
void IOHIDSystemCursorHelper::startPosting()
{
    locationDeltaPosting = locationDeltaAccumulated;
    locationDeltaAccumulated.fromIntFloor(0, 0);
    eventCountPosting = eventCount;
    eventCount = 0;
}

//===========================================================================
void IOHIDSystemCursorHelper::updateScreenLocation(IOGBounds *desktop, 
                                                   IOGBounds *screen)
{
    if (!desktop || !screen) {
        // no transform can be performed
        screenLocation = location;
    }
    else {
        if (*(UInt64*)desktop == *(UInt64*)screen) {
            //  no transform needed
            screenLocation = location;
        }
        else {
            int screenWidth   = screen->maxx  - screen->minx;
            int screenHeight  = screen->maxy  - screen->miny;
            int desktopWidth  = desktop->maxx - desktop->minx;
            int desktopHeight = desktop->maxy - desktop->miny;
            IOFixedPoint64 scratch;
            if ((screenWidth <= 0) || (screenHeight <= 0) || (desktopWidth <= 0) || (desktopHeight <= 0)) {
                // no transform can be performed
            }
            else {
                if ((screenWidth == desktopWidth) && (screenHeight == desktopHeight)) {
                    // translation only
                    screenLocation = location;
                    screenLocation += scratch.fromIntFloor(screen->minx - desktop->minx, 
                                                           screen->miny - desktop->miny);
                }
                else {
                    // full transform
                    IOFixed64 x_scale;
                    IOFixed64 y_scale;
                    x_scale.fromIntFloor(screenWidth) /= desktopWidth;
                    y_scale.fromIntFloor(screenHeight) /= desktopHeight;
                    screenLocation = location;
                    screenLocation -= scratch.fromIntFloor(desktop->minx, desktop->miny);
                    screenLocation *= scratch.fromFixed64(x_scale, y_scale);
                    screenLocation += scratch.fromIntFloor(screen->minx, screen->miny);
                }
            }
        }
    }
}

//===========================================================================
void IOHIDSystemCursorHelper::applyPostingDelta()
{
    if (eventCountPosting && expectedCountValue) {
        // unless the eventCountPosting is within the expectedCount ± 1, this adjustment
        // will cause more harm than good.
        if ((expectedCountValue <= eventCountPosting + 1LL) &&
            (expectedCountValue >= eventCountPosting - 1LL)) {
            locationDeltaPosting *= expectedCountValue;
            locationDeltaPosting /= eventCountPosting;
        }
    }
    eventCountPosting = 0;
    location += locationDeltaPosting;
    locationDelta += locationDeltaPosting;
    locationDeltaPosting.fromIntFloor(0, 0);
}


//===========================================================================
bool IOHIDSystemCursorHelper::isPosting(UInt64 time)
{
    bool result = false;
    if (locationDeltaPosting) {
        if (!time || !overdueTime || (time < overdueTime)) {
            result = true;
        }
        else {
            IOLog("IOHIDSystem cursor update overdue. Resending.\n");
        }
    }
    return result;
}

//===========================================================================
void IOHIDSystemCursorHelper::logPosition(const char *name, uint64_t ts)
{
    IOLog("IOHIDSystem::%-20s cursor @ %lld: "
          "(%4lld.%02lld, %4lld.%02lld) "
          "[%+3lld.%02lld, %+3lld.%02lld] "
          "P[%+3lld.%02lld, %+3lld.%02lld] "
          "A[%+3lld.%02lld, %+3lld.%02lld] "
          "S(%4lld.%02lld, %4lld.%02lld) "
          "C %ld/%ld of %lld.%01lld\n",
          name, ts,
          location.xValue().as64(), (location.xValue().asFixed64() & 0xffff) / 656,
          location.yValue().as64(), (location.yValue().asFixed64() & 0xffff) / 656,
          locationDelta.xValue().as64(), (locationDelta.xValue().asFixed64() & 0xffff) / 656,
          locationDelta.yValue().as64(), (locationDelta.yValue().asFixed64() & 0xffff) / 656,
          locationDeltaPosting.xValue().as64(), (locationDeltaPosting.xValue().asFixed64() & 0xffff) / 656,
          locationDeltaPosting.yValue().as64(), (locationDeltaPosting.yValue().asFixed64() & 0xffff) / 656,
          locationDeltaAccumulated.xValue().as64(), (locationDeltaAccumulated.xValue().asFixed64() & 0xffff) / 656,
          locationDeltaAccumulated.yValue().as64(), (locationDeltaAccumulated.yValue().asFixed64() & 0xffff) / 656,
          screenLocation.xValue().as64(), (screenLocation.xValue().asFixed64() & 0xffff) / 656,
          screenLocation.yValue().as64(), (screenLocation.yValue().asFixed64() & 0xffff) / 656,
          (long int)eventCount,
          (long int)eventCountPosting,
          expectedCountValue.as64(), (expectedCountValue.asFixed64() & 0xffff) / 6554
          );
}

//===========================================================================
void IOHIDSystemCursorHelper::klogPosition(const char *name, uint64_t ts)
{
    kprintf("IOHIDSystem::%-20s cursor @ %lld: "
            "(%016llx, %016llx) "
            "[%016llx, %016llx] "
            "P[%016llx, %016llx] "
            "A[%016llx, %016llx] "
            "S(%016llx, %016llx) "
            "C %ld/%ld of %016llx\n",
            name, ts,
            location.xValue().asFixed64(), 
            location.yValue().asFixed64(),
            locationDelta.xValue().asFixed64(), 
            locationDelta.yValue().asFixed64(), 
            locationDeltaPosting.xValue().asFixed64(), 
            locationDeltaPosting.yValue().asFixed64(), 
            locationDeltaAccumulated.xValue().asFixed64(), 
            locationDeltaAccumulated.yValue().asFixed64(), 
            screenLocation.xValue().asFixed64(),
            screenLocation.yValue().asFixed64(),
            (long int)eventCount,
            (long int)eventCountPosting,
            expectedCountValue.asFixed64()
            );
}

//===========================================================================
