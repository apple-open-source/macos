/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */


#ifndef _MACRISC4_PLATFORMPLUGIN_H
#define _MACRISC4_PLATFORMPLUGIN_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>

#include "IOPlatformPlugin.h"

/*!
    @class MacRISC4_PlatformPlugin
    @abstract A class for monitor system functions such as power and thermal */
class MacRISC4_PlatformPlugin : public IOPlatformPlugin
{
    OSDeclareDefaultStructors(MacRISC4_PlatformPlugin)	

public:

	//virtual bool 			start(IOService *nub);
	//virtual IOReturn		powerStateWillChangeTo (IOPMPowerFlags, unsigned long, IOService*);
	
};
#endif // _MACRISC4_PLATFORMPLUGIN_H
