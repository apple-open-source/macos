/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */
//		$Log: MacRISC4PE.h,v $
//		Revision 1.3  2003/06/27 00:45:07  raddog
//		[3304596]: remove unnecessary access to U3 Pwr registers on wake, [3249029]: Disable unused second process on wake, [3301232]: remove unnecessary PCI code from PE
//		
//		Revision 1.2  2003/04/14 20:05:27  raddog
//		[3224952]AppleMacRISC4CPU must specify which MPIC to use (improved fix over that previously submitted)
//		
//		Revision 1.1.1.1  2003/02/04 00:36:43  raddog
//		initial import into CVS
//		

#ifndef _IOKIT_MACRISC4PE_H
#define _IOKIT_MACRISC4PE_H

#include <IOKit/platform/ApplePlatformExpert.h>
//#include <IOKit/pci/IOPCIDevice.h>
//#include <IOKit/pci/IOPCIBridge.h>

#include "IOPMSlotsMacRISC4.h"
#include "IOPMUSBMacRISC4.h"
#include "U3.h"
#include "IOPlatformPlugin.h"

enum
{
	kMacRISC4TypeUnknown = kMachineTypeUnknown,
    kMacRISC4TypePowerMac,
    kMacRISC4TypePowerBook,
};

#define kMacRISC4ParentICKey "AAPL,parentIC"

class MacRISC4PE : public ApplePlatformExpert
{
    OSDeclareDefaultStructors(MacRISC4PE);
  
    friend class MacRISC4CPU;
  
private:
    const char 				*provider_name;
    unsigned long			uniNVersion;
	MacRISC4CPU				*macRISC4CPU;
	AppleU3					*uniN;
    class IOPMSlotsMacRISC4	*slotsMacRISC4;
    IOLock					*pmmutex;
	bool					isPortable;
	
	IOService				*ioPPluginNub;
	IOService				*plFuncNub;

    void getDefaultBusSpeeds(long *numSpeeds, unsigned long **speedList);
	//IOReturn instantiatePlatformFunctions (IOService *nub, OSArray **pfArray);
  
    void PMInstantiatePowerDomains ( void );
    void PMRegisterDevice(IOService * theNub, IOService * theDevice);
    IORegistryEntry * retrievePowerMgtEntry (void);

public:
    virtual bool start(IOService *provider);
    virtual bool platformAdjustService(IOService *service);
    virtual IOReturn callPlatformFunction(const OSSymbol *functionName,
					bool waitForFunction, void *param1, void *param2,
                    void *param3, void *param4);
};

#endif // _IOKIT_MACRISC4PE_H