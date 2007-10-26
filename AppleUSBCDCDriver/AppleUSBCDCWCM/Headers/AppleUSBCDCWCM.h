/*
 * Copyright (c) 1998-2004 Apple Computer, Inc. All rights reserved.
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
 
#ifndef __APPLEUSBCDCWCM__
#define __APPLEUSBCDCWCM__

#include "AppleUSBCDCCommon.h"

    // Common Defintions

#define LDEBUG		0			// for debugging
#define USE_ELG		0			// to Event LoG (via kprintf and Firewire) - LDEBUG must also be set
#define USE_IOL		0			// to IOLog - LDEBUG must also be set

#define Sleep_Time	20

#define Log IOLog
#if USE_ELG
	#undef Log
	#define Log	kprintf
#endif

#if LDEBUG
    #if USE_ELG
		#define XTRACE(ID,A,B,STRING) {Log("%8x %8x %8x %8x " DEBUG_NAME ": " STRING "\n",(unsigned int)(ID),(unsigned int)(A),(unsigned int)(B), (unsigned int)IOThreadSelf());}
    #else /* not USE_ELG */
        #if USE_IOL
            #define XTRACE(ID,A,B,STRING) {Log("%8x %8x %8x %8x " DEBUG_NAME ": " STRING "\n",(unsigned int)(ID),(unsigned int)(A),(unsigned int)(B), (unsigned int)IOThreadSelf()); IOSleep(Sleep_Time);}
        #else
            #define XTRACE(id, x, y, msg)
        #endif /* USE_IOL */
    #endif /* USE_ELG */
#else /* not LDEBUG */
    #define XTRACE(id, x, y, msg)
    #undef USE_ELG
    #undef USE_IOL
#endif /* LDEBUG */

#define ALERT(A,B,STRING)	Log("%8x %8x " DEBUG_NAME ": " STRING "\n", (unsigned int)(A), (unsigned int)(B))

    // Miscellaneous

#define kDeviceSelfPowered	1
    
enum
{
    kCDCPowerOffState	= 0,
    kCDCPowerOnState	= 1,
    kNumCDCStates	= 2
};

	/* AppleUSBCDCWCM.h - This file contains the class definition for the		*/
	/* USB Communication Device Class (CDC) WMC driver. 				*/

class AppleUSBCDCWCM : public IOService
{
    OSDeclareDefaultStructors(AppleUSBCDCWCM);			// Constructor & Destructor stuff

private:
    bool			fTerminate;				// Are we being terminated (ie the device was unplugged)
    bool			fStopping;				// Are we being "stopped"
    UInt8			fPowerState;				// Ordinal for power management
    UInt8			fSubClass;				// Interface subclass
    UInt8			fInterfaceNumber;			// My interface number
	UInt16			fControlLen;			// Subordinate interface length
	UInt8			*fControlMap;			// Subordinate interface numbers

public:

    IOUSBInterface		*fInterface;

        // IOKit methods:
		
	virtual IOService   *probe(IOService *provider, SInt32 *score);
    virtual bool		start(IOService *provider);
    virtual void		stop(IOService *provider);
    virtual IOReturn 		message(UInt32 type, IOService *provider, void *argument = 0);
												
        // CDC WMC Control Driver Methods
	
    bool			configureWHCM(void);
    bool 			configureDevice(void);
    bool			getFunctionalDescriptors(void);
    bool 			allocateResources(void);
    void			releaseResources(void);
    void                        resetLogicalHandset(void);
    
        // Power Manager Methods
        
    bool			initForPM(IOService *provider);
    unsigned long		initialPowerStateForDomainState(IOPMPowerFlags flags);
    IOReturn			setPowerState(unsigned long powerStateOrdinal, IOService *whatDevice);

}; /* end class AppleUSBCDCWCM */
#endif