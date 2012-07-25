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

#include "IOHIDEventRepairDriver.h"

//===========================================================================
// IOHIDEventRepairDriver class

#define super IOHIDEventDriver

OSDefineMetaClassAndStructors( IOHIDEventRepairDriver, IOHIDEventDriver )
//====================================================================================================
// IOHIDEventRepairDriver::dispatchKeyboardEvent
//====================================================================================================
void IOHIDEventRepairDriver::dispatchKeyboardEvent(
                                AbsoluteTime                timeStamp __unused,
                                UInt32                      usagePage __unused,
                                UInt32                      usage __unused,
                                UInt32                      value __unused,
                                IOOptionBits                options __unused)
{
}

//====================================================================================================
// IOHIDEventRepairDriver::dispatchScrollWheelEvent
//====================================================================================================
void IOHIDEventRepairDriver::dispatchScrollWheelEvent(
                                AbsoluteTime                timeStamp,
                                SInt32                      deltaAxis1,
                                SInt32                      deltaAxis2 __unused,
                                SInt32                      deltaAxis3 __unused,
                                IOOptionBits                options)
{
    super::dispatchScrollWheelEvent(timeStamp, deltaAxis1, 0, 0, options);
}

