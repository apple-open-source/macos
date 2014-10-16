/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) Apple Computer, Inc.  All Rights Reserved.
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

#include "IOHIDEventSource.h"

#undef  super
#define super IOEventSource

OSDefineMetaClassAndStructors(IOHIDEventSource, super);

IOHIDEventSource *
IOHIDEventSource::HIDEventSource(OSObject * inOwner, Action inAction)
{
    IOHIDEventSource * me = new IOHIDEventSource;
    
    if (me && !me->init(inOwner, inAction)) {
        me->release();
        return 0;
    }
    
    return me;
}


void
IOHIDEventSource::lock(void)
{
    closeGate();
}

void
IOHIDEventSource::unlock(void)
{
    openGate();
}

//
// When an event source is removed from a workloop the it results in
// the workLoop member variable of the event source being set to NULL.
// So if a thread tries to execute closeGate() after the removeEventSource()
// call there will be a kernel panic.
// If we try to work-around this problem by removing the event source in
// ::free() then it may lead to deadlocks as removeEventSource() grabs the
// workloop lock and free() can be called from any context.
// If we try to work-around this by addding an isInactive() check before the
// closeGate()/runAction() then there is a possibility that the thread
// can get preempted after the check and when closeGate() is called workLoop
// is set to NULL.
// So as a temprorary work-around, override setWorkLoop() such that if the
// workLoop is being set to NULL, skip setting it to NULL and set it to NULL
// only in IOHIDEventSource::free()
// This can be removed depending on the outcome of <rdar://problem/17666447> 

void
IOHIDEventSource::setWorkLoop(IOWorkLoop *inWorkLoop)
{
    if (!inWorkLoop) {
        disable();
    } else {
        super::setWorkLoop(inWorkLoop);
        workLoop->retain();
    }
}

void
IOHIDEventSource::free(void)
{
    OSSafeReleaseNULL(workLoop);
    
    super::free();
}
