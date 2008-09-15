/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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
 
        
#ifndef __APPLEUSBCDCECMControl__
#define __APPLEUSBCDCECMControl__

#include "AppleUSBCDCCommon.h"
#include "AppleUSBCDCECMData.h"

    // Miscellaneous

#define COMM_BUFF_SIZE		16
#define WATCHDOG_TIMER_MS       1000
#define kDeviceSelfPowered	1
    
enum
{
    kCDCPowerOffState	= 0,
    kCDCPowerOnState	= 1,
    kNumCDCStates	= 2
};

class AppleUSBCDCECMData;

        /* AppleUSBCDCECMControl.h - This file contains the class definition for the		*/
	/* USB Communication Device Class (CDC) Ethernet Control driver. 			*/


class AppleUSBCDCECMControl : public IOService
{
    OSDeclareDefaultStructors(AppleUSBCDCECMControl);	// Constructor & Destructor stuff

private:
	AppleUSBCDC				*fCDCDriver;		// The CDC driver
	AppleUSBCDCECMData		*fDataDriver;		// Our data interface driver
    bool			fdataAcquired;				// Has the data port been acquired
    bool			fTerminate;				// Are we being terminated (ie the device was unplugged)
    UInt8			fPowerState;				// Ordinal for power management
    bool			fCommDead;				// Interrupt read status
    IOUSBPipe			*fCommPipe;				// The interrupt pipe
    IOBufferMemoryDescriptor	*fCommPipeMDP;				// Interrupt pipe memory descriptor
    UInt8			*fCommPipeBuffer;			// Interrupt pipe buffer
    IOUSBCompletion		fCommCompletionInfo;			// Interrupt completion routine
    IOUSBCompletion		fMERCompletionInfo;			// MER Completion routine
    IOUSBCompletion		fStatsCompletionInfo;			// Stats Completion routine
    UInt8			fCommInterfaceNumber;			// My interface number
    
    bool			fReady;
    UInt8			fLinkStatus;
    UInt32			fUpSpeed;
    UInt32			fDownSpeed;
    
    bool			fStatInProgress;
    UInt16			fCurrStat;
    UInt32			fStatValue;
    
    static void			commReadComplete( void *obj, void *param, IOReturn ior, UInt32 remaining);
    static void			merWriteComplete(void *obj, void *param, IOReturn ior, UInt32 remaining);
    static void			statsWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining);
    
    UInt16			fVendorID;
    UInt16			fProductID;

public:

    IOUSBInterface		*fControlInterface;
    UInt8			fDataInterfaceNumber;			// Matching Data interface number
    
    UInt16			fPacketFilter;
    UInt8			fEaddr[6];
    UInt16			fMcFilters;
    
    UInt16			fMax_Block_Size;
    
    IONetworkStats		*fpNetStats;
    IOEthernetStats		*fpEtherStats;
    bool			fInputPktsOK;
    bool			fInputErrsOK;
    bool			fOutputPktsOK;
    bool			fOutputErrsOK;
    UInt8 			fEthernetStatistics[4];

        // IOKit methods:
		
	virtual IOService   *probe(IOService *provider, SInt32 *score);
    virtual bool		start(IOService *provider);
    virtual void		stop(IOService *provider);
    virtual IOReturn 	message(UInt32 type, IOService *provider, void *argument = 0);
    
        // CDC ECM Control Driver Methods
    
    UInt8			Asciihex_to_binary(char c);
    bool			configureECM(void);
    bool			getFunctionalDescriptors(void);
    virtual bool		dataAcquired(IONetworkStats *netStats, IOEthernetStats *etherStats);
    virtual void		dataReleased(void);
    bool 			allocateResources(void);
    void			releaseResources(void);
    virtual bool		checkInterfaceNumber(AppleUSBCDCECMData *dataDriver);
    IOReturn			checkPipe(IOUSBPipe *thePipe, bool devReq);
    virtual bool		USBSetMulticastFilter(IOEthernetAddress *addrs, UInt32 count);
    virtual bool		USBSetPacketFilter(void);
    virtual bool		statsProcessing(void);
    
            // Power Manager Methods

//    bool			initForPM(IOService *provider);
//    unsigned long		initialPowerStateForDomainState(IOPMPowerFlags flags);
//    IOReturn			setPowerState(unsigned long powerStateOrdinal, IOService *whatDevice);
												
}; /* end class AppleUSBCDCECMControl */
#endif