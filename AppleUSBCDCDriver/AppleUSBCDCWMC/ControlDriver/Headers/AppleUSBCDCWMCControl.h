/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
 
#ifndef __APPLEUSBCDCWMCControl__
#define __APPLEUSBCDCWMCControl__

#include "AppleUSBCDCCommon.h"
#include "AppleUSBCDCWMCData.h"

    // Miscellaneous

#define COMM_BUFF_SIZE	16

#define kDeviceSelfPowered	1
    
enum
{
    kCDCPowerOffState	= 0,
    kCDCPowerOnState	= 1,
    kNumCDCStates	= 2
};

class AppleUSBCDCWMCData;

	/* AppleUSBCDCWMCControl.h - This file contains the class definition for the		*/
	/* USB Communication Device Class (CDC) WMC Control driver. 				*/

class AppleUSBCDCWMCControl : public IOService
{
    OSDeclareDefaultStructors(AppleUSBCDCWMCControl);			// Constructor & Destructor stuff

private:
    bool			fdataAcquired;				// Has the data port been acquired
    bool			fTerminate;				// Are we being terminated (ie the device was unplugged)
    bool			fStopping;				// Are we being "stopped"
    UInt8			fPowerState;				// Ordinal for power management
    UInt8			fConfig;				// Configuration number
    IOUSBPipe			*fCommPipe;				// The interrupt pipe
    IOBufferMemoryDescriptor	*fCommPipeMDP;				// Interrupt pipe memory descriptor
    UInt8			*fCommPipeBuffer;			// Interrupt pipe buffer
    IOUSBCompletion		fCommCompletionInfo;			// Interrupt completion routine
    IOUSBCompletion		fMERCompletionInfo;			// MER Completion routine
    UInt8			fCommSubClass;				// Interface subclass
    UInt8			fCommInterfaceNumber;			// My interface number

    static void			commReadComplete( void *obj, void *param, IOReturn ior, UInt32 remaining);
    static void			merWriteComplete(void *obj, void *param, IOReturn ior, UInt32 remaining);

public:

    IOUSBInterface		*fControlInterface;
    UInt8			fDataInterfaceNumber;			// Matching Data interface number

        // IOKit methods:
		
    virtual bool		start(IOService *provider);
    virtual void		stop(IOService *provider);
    virtual IOReturn 		message(UInt32 type, IOService *provider, void *argument = 0);
												
        // CDC WMC Control Driver Methods
	
    bool			configureWHCM(void);
    bool			configureDMM(void);
    bool			configureMDLM(void);
    bool 			configureDevice(void);
    bool			getFunctionalDescriptors(void);
    virtual bool		dataAcquired(void);
    virtual void		dataReleased(void);
    IOReturn			checkPipe(IOUSBPipe *thePipe, bool devReq);
    bool 			allocateResources(void);
    void			releaseResources(void);
    bool			WakeonRing(void);
    void                        resetDevice(void);
    virtual bool		checkInterfaceNumber(AppleUSBCDCWMCData *dataDriver);
    
        // Power Manager Methods
        
    bool			initForPM(IOService *provider);
    unsigned long		initialPowerStateForDomainState(IOPMPowerFlags flags);
    IOReturn			setPowerState(unsigned long powerStateOrdinal, IOService *whatDevice);

}; /* end class AppleUSBCDCWMCControl */
#endif