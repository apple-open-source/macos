/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/adb/IOADBController.h>
#include <IOKit/pccard/IOPCCard.h>

#ifndef _IOKIT_APPLEPMUPCCARDEJECT_H
#define _IOKIT_APPLEPMUPCCARDEJECT_H

class ApplePMUPCCardEject : public IOPCCardEjectController
{
    OSDeclareDefaultStructors(ApplePMUPCCardEject)

  private:
  
    IOService * 		bridge;			// our parent
    UInt8			pmuSocket;		// the physical socket index from OF
    IOService * 		pmuDriver;		// handle to the PMU driver

    struct ExpansionData	{ };
    ExpansionData *		reserved;
    
    IOReturn			localSendMiscCommand(int command, IOByteCount sLength, UInt8 *sBuffer);
    static void			handleInterrupt(IOService *client, UInt8 matchingMask, UInt32 length, UInt8 *buffer);

  public:
    bool			start(IOService * provider);
    void			stop(IOService * provider);
    
    IOReturn			ejectCard();
};

#endif /* ! _IOKIT_APPLEPMUPCCARDEJECT_H */
