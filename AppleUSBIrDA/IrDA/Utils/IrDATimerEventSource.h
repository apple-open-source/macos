/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#ifndef _IRDATIMEREVENTSOURCE
#define _IRDATIMEREVENTSOURCE

#include <IOKit/IOTimerEventSource.h>

// subclass from standard IOTimerEventSource to
// add a workaround for cancelTimer not being
// reliable in the face of workloop gates.

class IrDATimerEventSource : public IOTimerEventSource
{
    OSDeclareDefaultStructors(IrDATimerEventSource)

public:
    typedef void (*Action)(OSObject *owner, IrDATimerEventSource *sender);

    static IrDATimerEventSource *IrDATimerEventSource::timerEventSource(OSObject *inOwner, Action inAction);
    Boolean SafeCancelTimeout();
};

#endif /* !_IRDATIMEREVENTSOURCE */
