/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 2003-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#ifndef _IOPLATFORMSLEWCLOCKCONTROL_H
#define _IOPLATFORMSLEWCLOCKCONTROL_H

#include "IOPlatformControl.h"

class IOPlatformSlewClockControl : public IOPlatformControl
{

	OSDeclareDefaultStructors(IOPlatformSlewClockControl)

	// we used to override fetchCurrentValue here because we considered the "standard"
	// mechanism for publishing control current-values to be the control-info dictionary.
	// The standard has since changed such that the current-value is published as a
	// property in the control driver (this is what IOHWControl does), and AppleSlewClock
	// conforms to this also, so this class is now only necessary so that we don't have to
	// make changes to existing PLISTs.

};

#endif // _IOPLATFORMSLEWCLOCKCONTROL_H
