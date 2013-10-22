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

#include <IOKit/system.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include "IOFixedPoint64.h"

//===========================================================================
/*  IOHIDSystemCursorHelper tracks 4 things.
        * The location of the cursor on the desktop. (location)
        * The change in location since the previous cursor update. (locationDelta)
        * The change in location that is currently being posted. (locationDeltaPosting)
        * The accumulated change since the last posting. (locationDeltaAccumulated)
    It also calculates one more.
        * The location of the cursor on the screen. (screenLocation)
 */
class IOHIDSystemCursorHelper
{
public:
    // the creator/destructor cannot be relied upon. use init/finalize instead.
    boolean_t       init();
    void            finalize()                      { }
    
    IOFixedPoint64& desktopLocation()               { return location; }
    IOFixedPoint64& desktopLocationDelta()          { return locationDelta; }
    IOFixedPoint64& desktopLocationPosting()        { return locationDeltaPosting; }
    void            applyPostingDelta();
    void            startPosting();
    IOFixedPoint64& desktopLocationAccumulated()    { return locationDeltaAccumulated; }
    void            incrementEventCount()           { eventCount++; }
    SInt32          getEventCount()                 { return eventCount; }
    SInt32          getEventCountPosting()          { return eventCountPosting; }
    void            clearEventCounts()              { eventCount = eventCountPosting = 0; }
    IOFixed64&      expectedCount()                 { return expectedCountValue; }
    
    IOFixedPoint64  getScreenLocation()             { return screenLocation; }
    void            updateScreenLocation(IOGBounds *desktop, 
                                         IOGBounds *screen);
    void            logPosition(const char *name, uint64_t ts);
    void            klogPosition(const char *name, uint64_t ts);
    
    bool            isPosting();

private:
    // all locations in desktop coordinates unless noted otherwise
    IOFixedPoint64  location;                   // aka pointerLoc
    IOFixedPoint64  locationDelta;              // aka pointerDelta
    IOFixedPoint64  locationDeltaPosting;       // aka postDeltaX/Y
    IOFixedPoint64  locationDeltaAccumulated;   // aka accumDX/Y
    
    IOFixedPoint64  screenLocation;
    
    IOFixed64       expectedCountValue;
    SInt32          eventCount;
    SInt32          eventCountPosting;
};

//===========================================================================
