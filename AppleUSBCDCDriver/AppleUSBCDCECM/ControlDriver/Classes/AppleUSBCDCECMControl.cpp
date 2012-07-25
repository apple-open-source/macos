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

    /* AppleUSBCDCECMControl.cpp - MacOSX implementation of			*/
    /* USB Communication Device Class (CDC) Driver, Ethernet Control Interface.	*/
    
#include <machine/limits.h>			/* UINT_MAX */
#include <libkern/OSByteOrder.h>

#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOGatedOutputQueue.h>

#include <IOKit/IOTimerEventSource.h>
#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMessage.h>

#include <IOKit/pwr_mgt/RootDomain.h>

#include <IOKit/usb/IOUSBBus.h>
#include <IOKit/usb/IOUSBNub.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBInterface.h>

#include <UserNotification/KUNCUserNotifications.h>

extern "C"
{
    #include <sys/param.h>
    #include <sys/mbuf.h>
}
     
#define DEBUG_NAME "AppleUSBCDCECMControl"

#include "AppleUSBCDCECM.h"
#include "AppleUSBCDCECMControl.h"

#if 0
static IOPMPowerState gOurPowerStates[kNumCDCStates] =
{
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
};
#endif

#define	numStats	13
UInt16	stats[numStats] = { kXMIT_OK_REQ,
                            kRCV_OK_REQ,
                            kXMIT_ERROR_REQ,
                            kRCV_ERROR_REQ, 
                            kRCV_CRC_ERROR_REQ,
                            kRCV_ERROR_ALIGNMENT_REQ,
                            kXMIT_ONE_COLLISION_REQ,
                            kXMIT_MORE_COLLISIONS_REQ,
                            kXMIT_DEFERRED_REQ,
                            kXMIT_MAX_COLLISION_REQ,
                            kRCV_OVERRUN_REQ,
                            kXMIT_TIMES_CARRIER_LOST_REQ,
                            kXMIT_LATE_COLLISIONS_REQ
                        };

#define super IOService

OSDefineMetaClassAndStructors(AppleUSBCDCECMControl, IOService);

/****************************************************************************************************/
//
//		Function:	findCDCDriverEC
//
//		Inputs:		controlAddr - my address
//
//		Outputs:	
//
//		Desc:		Finds the initiating CDC driver
//
/****************************************************************************************************/

AppleUSBCDC *findCDCDriverEC(void *controlAddr, IOReturn *retCode)
{
    AppleUSBCDCECMControl	*me = (AppleUSBCDCECMControl *)controlAddr;
    AppleUSBCDC		*CDCDriver = NULL;
    bool		driverOK = false;
    OSIterator		*iterator = NULL;
    OSDictionary	*matchingDictionary = NULL;
    
    XTRACE(me, 0, 0, "findCDCDriverEC");
        
        // Get matching dictionary
       	
    matchingDictionary = IOService::serviceMatching("AppleUSBCDC");
    if (!matchingDictionary)
    {
        XTRACE(me, 0, 0, "findCDCDriverEC - Couldn't create a matching dictionary");
		*retCode = kIOReturnError;
        return NULL;
    }
    
	// Get an iterator
	
    iterator = IOService::getMatchingServices(matchingDictionary);
    if (!iterator)
    {
        XTRACE(me, 0, 0, "findCDCDriverEC - No AppleUSBCDC driver found!");
        matchingDictionary->release();
		*retCode = kIOReturnError;
        return NULL;
    }

    	// Iterate until we find our matching CDC driver
                
    CDCDriver = (AppleUSBCDC *)iterator->getNextObject();
    while (CDCDriver)
    {
        XTRACEP(me, 0, CDCDriver, "findCDCDriverEC - CDC driver candidate");
        
        if (me->fControlInterface->GetDevice() == CDCDriver->getCDCDevice())
        {
            XTRACEP(me, 0, CDCDriver, "findCDCDriverEC - Found our CDC driver");
            driverOK = CDCDriver->confirmControl(kUSBEthernetControlModel, me->fControlInterface);
            break;
        }
        CDCDriver = (AppleUSBCDC *)iterator->getNextObject();
    }

    matchingDictionary->release();
    iterator->release();
    
    if (!CDCDriver)
    {
        XTRACE(me, 0, 0, "findCDCDriverEC - CDC driver not found");
		*retCode = kIOReturnNotReady;
        return NULL;
    }
   
    if (!driverOK)
    {
        XTRACE(me, kUSBEthernetControlModel, 0, "findCDCDriverEC - Not my interface");
		*retCode = kIOReturnError;
        return NULL;
    }
	
	*retCode = kIOReturnSuccess;

    return CDCDriver;
    
}/* end findCDCDriverAC */

/****************************************************************************************************/
//
//		Function:	AppleUSBCDCECMControl::Asciihex_to_binary
//
//		Inputs:		c - Ascii character
//
//		Outputs:	return byte - binary byte
//
//		Desc:		Converts to hex (binary). 
//
/****************************************************************************************************/

UInt8 AppleUSBCDCECMControl::Asciihex_to_binary(char c)
{

    if ('0' <= c && c <= '9')
        return(c-'0');
                 
    if ('A' <= c && c <= 'F')
        return((c-'A')+10);
        
    if ('a' <= c && c <= 'f')
        return((c-'a')+10);
        
      // Not a hex digit, do whatever
      
    return(0);
    
}/* end Asciihex_to_binary */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::commReadComplete
//
//		Inputs:		obj - me
//				param - unused
//				rc - return code
//				remaining - what's left
//
//		Outputs:	
//
//		Desc:		Interrupt pipe (Comm interface) read completion routine
//
/****************************************************************************************************/

void AppleUSBCDCECMControl::commReadComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    AppleUSBCDCECMControl	*me = (AppleUSBCDCECMControl*)obj;
    IOReturn			ior;
    UInt32			dLen;
	Notification	*notif;
	ConnectionSpeedChange	*speedChange;

    if (rc == kIOReturnSuccess)	// If operation returned ok
    {
        dLen = COMM_BUFF_SIZE - remaining;
        XTRACE(me, rc, dLen, "commReadComplete");
		
            // Now look at what we received
			
        notif = (Notification *)me->fCommPipeBuffer;
        if (dLen > 7)
        {
            switch(notif->bNotification)
            {
                case kUSBNETWORK_CONNECTION:
					if (notif->wValue == 0)
					{
						me->fLinkStatus = kLinkDown;
					} else {
						me->fLinkStatus = kLinkUp;
					}
                    XTRACE(me, 0, me->fLinkStatus, "commReadComplete - kNetwork_Connection");
					if (me->fDataDriver)
					{
						me->fDataDriver->linkStatusChange(me->fLinkStatus);
					}
                    break;
                case kUSBCONNECTION_SPEED_CHANGE:
					speedChange = (ConnectionSpeedChange *)me->fCommPipeBuffer;
					me->fUpSpeed = USBToHostLong(speedChange->USBitRate);
					me->fDownSpeed = USBToHostLong(speedChange->DSBitRate);
                    XTRACE(me, me->fUpSpeed, me->fDownSpeed, "commReadComplete - kConnection_Speed_Change");
					if (me->fDataDriver)
					{
						me->fDataDriver->linkSpeedChange(me->fUpSpeed, me->fDownSpeed);
					}
                    break;
                default:
                    XTRACE(me, 0, notif->bNotification, "commReadComplete - Unsupported notification");
                    break;
            }
        } else {
            XTRACE(me, 0, notif->bNotification, "commReadComplete - Invalid notification");
        }
    } else {
        XTRACE(me, 0, rc, "commReadComplete - IO error");
        if (rc != kIOReturnAborted)
        {
            rc = me->checkPipe(me->fCommPipe, false);
            if (rc != kIOReturnSuccess)
            {
                XTRACE(me, 0, rc, "dataReadComplete - clear stall failed (trying to continue)");
            }
        }
    }

        // Queue the next read, only if not aborted
        
    if (rc != kIOReturnAborted)
    {
        ior = me->fCommPipe->Read(me->fCommPipeMDP, &me->fCommCompletionInfo, NULL);
        if (ior != kIOReturnSuccess)
        {
            XTRACE(me, 0, rc, "commReadComplete - Read io error");
            me->fCommDead = true;
        }
    }

    return;
	
}/* end commReadComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::merWriteComplete
//
//		Inputs:		obj - me
//				param - MER (may or may not be present depending on request) 
//				rc - return code
//				remaining - what's left
//
//		Outputs:	
//
//		Desc:		Management element request write completion routine
//
/****************************************************************************************************/

void AppleUSBCDCECMControl::merWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
#if LDEBUG
    AppleUSBCDCECMControl	*me = (AppleUSBCDCECMControl *)obj;
#endif
    IOUSBDevRequest		*MER = (IOUSBDevRequest*)param;
    UInt16			dataLen;
	
    if (MER)
    {
        if (rc == kIOReturnSuccess)
        {
            XTRACE(me, MER->bRequest, remaining, "merWriteComplete");
        } else {
            XTRACE(me, MER->bRequest, rc, "merWriteComplete - io err");
        }
		
        dataLen = MER->wLength;
        XTRACE(me, 0, dataLen, "merWriteComplete - data length");
        if ((dataLen != 0) && (MER->pData))
        {
            IOFree(MER->pData, dataLen);
        }
        IOFree(MER, sizeof(IOUSBDevRequest));
		
    } else {
        if (rc == kIOReturnSuccess)
        {
            XTRACE(me, 0, remaining, "merWriteComplete (request unknown)");
        } else {
            XTRACE(me, 0, rc, "merWriteComplete (request unknown) - io err");
        }
    }
	
    return;
	
}/* end merWriteComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::statsWriteComplete
//
//		Inputs:		obj - me
//				param - parameter block 
//				rc - return code
//				remaining - what's left
//
//		Outputs:	
//
//		Desc:		Ethernet statistics request write completion routine
//
/****************************************************************************************************/

void AppleUSBCDCECMControl::statsWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    AppleUSBCDCECMControl	*me = (AppleUSBCDCECMControl *)obj;
    IOUSBDevRequest		*STREQ = (IOUSBDevRequest*)param;
    UInt16			currStat;
	
    if (STREQ)
    {
        if (rc == kIOReturnSuccess)
        {
            XTRACE(me, STREQ->bRequest, remaining, "statsWriteComplete");
            currStat = STREQ->wValue;
            switch(currStat)
            {
                case kXMIT_OK_REQ:
                    me->fpNetStats->outputPackets = USBToHostLong(me->fStatValue);
                    break;
                case kRCV_OK_REQ:
                    me->fpNetStats->inputPackets = USBToHostLong(me->fStatValue);
                    break;
                case kXMIT_ERROR_REQ:
                    me->fpNetStats->outputErrors = USBToHostLong(me->fStatValue);
                    break;
                case kRCV_ERROR_REQ:
                    me->fpNetStats->inputErrors = USBToHostLong(me->fStatValue);
                    break;
                case kRCV_CRC_ERROR_REQ:
                    me->fpEtherStats->dot3StatsEntry.fcsErrors = USBToHostLong(me->fStatValue); 
                    break;
                case kRCV_ERROR_ALIGNMENT_REQ:
                    me->fpEtherStats->dot3StatsEntry.alignmentErrors = USBToHostLong(me->fStatValue);
                    break;
                case kXMIT_ONE_COLLISION_REQ:
                    me->fpEtherStats->dot3StatsEntry.singleCollisionFrames = USBToHostLong(me->fStatValue);
                    break;
                case kXMIT_MORE_COLLISIONS_REQ:
                    me->fpEtherStats->dot3StatsEntry.multipleCollisionFrames = USBToHostLong(me->fStatValue);
                    break;
                case kXMIT_DEFERRED_REQ:
                    me->fpEtherStats->dot3StatsEntry.deferredTransmissions = USBToHostLong(me->fStatValue);
                    break;
                case kXMIT_MAX_COLLISION_REQ:
                    me->fpNetStats->collisions = USBToHostLong(me->fStatValue);
                    break;
                case kRCV_OVERRUN_REQ:
                    me->fpEtherStats->dot3StatsEntry.frameTooLongs = USBToHostLong(me->fStatValue);
                    break;
                case kXMIT_TIMES_CARRIER_LOST_REQ:
                    me->fpEtherStats->dot3StatsEntry.carrierSenseErrors = USBToHostLong(me->fStatValue);
                    break;
                case kXMIT_LATE_COLLISIONS_REQ:
                    me->fpEtherStats->dot3StatsEntry.lateCollisions = USBToHostLong(me->fStatValue);
                    break;
                default:
                    XTRACE(me, currStat, rc, "statsWriteComplete - Invalid stats code");
                    break;
            }

        } else {
            XTRACE(me, STREQ->bRequest, rc, "statsWriteComplete - io err");
        }
		
        IOFree(STREQ, sizeof(IOUSBDevRequest));
    } else {
        if (rc == kIOReturnSuccess)
        {
            XTRACE(me, 0, remaining, "statsWriteComplete (request unknown)");
        } else {
            XTRACE(me, 0, rc, "statsWriteComplete (request unknown) - io err");
        }
    }
	
    me->fStatValue = 0;
    me->fStatInProgress = false;
    return;
	
}/* end statsWriteComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::probe
//
//		Inputs:		provider - my provider
//
//		Outputs:	IOService - from super::probe, score - probe score
//
//		Desc:		Modify the probe score if necessary (we don't  at the moment)
//
/****************************************************************************************************/

IOService* AppleUSBCDCECMControl::probe(IOService *provider, SInt32 *score)
{ 
    IOService   *res;
	
		// If our IOUSBInterface has a "do not match" property, it means that we should not match and need 
		// to bail.  See rdar://3716623
    
    OSBoolean *boolObj = OSDynamicCast(OSBoolean, provider->getProperty("kDoNotClassMatchThisInterface"));
    if (boolObj && boolObj->isTrue())
    {
        XTRACE(this, 0, 0, "probe - provider doesn't want us to match");
        return NULL;
    }

    res = super::probe(provider, score);
    
    return res;
    
}/* end probe */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::start
//
//		Inputs:		provider - my provider
//
//		Outputs:	Return code - true (it's me), false (sorry it probably was me, but I can't configure it)
//
//		Desc:		This is called once it has beed determined I'm probably the best 
//				driver for this device.
//
/****************************************************************************************************/

bool AppleUSBCDCECMControl::start(IOService *provider)
{
	IOReturn	rtn;
	UInt16		devDriverCount = 0;

    XTRACE(this, 0, 0, "start");

    fCurrStat = 0;
	fCDCDriver = NULL;
    fStatInProgress = false;
    fMax_Block_Size = MAX_BLOCK_SIZE;
    fCommDead = false;
    fPacketFilter = kPACKET_TYPE_DIRECTED | kPACKET_TYPE_BROADCAST | kPACKET_TYPE_MULTICAST;
    fpNetStats = NULL;
    fpEtherStats = NULL;
	fDataDriver = NULL;
	        
    if(!super::start(provider))
    {
        ALERT(0, 0, "start - super failed");
        return false;
    }
    
    	// Get my USB provider - the interface

    fControlInterface = OSDynamicCast(IOUSBInterface, provider);
    if(!fControlInterface)
    {
        ALERT(0, 0, "start - provider invalid");
        return false;
    }
	
		// See if we can find/wait for the CDC driver
		
	while (!fCDCDriver)
	{
		rtn = kIOReturnSuccess;
		fCDCDriver = findCDCDriverEC(this, &rtn);
		if (fCDCDriver)
		{
			XTRACE (this, 0, fControlInterface->GetInterfaceNumber(), "start: Found the CDC device driver");
			break;
		} else {
			if (rtn == kIOReturnNotReady)
			{
				devDriverCount++;
				XTRACE(this, devDriverCount, fControlInterface->GetInterfaceNumber(), "start - Waiting for CDC device driver...");
				if (devDriverCount > 9)
				{
					break;
				}
				IOSleep(100);
			} else {
				break;
			}
		}
	}
	
		// If we didn't find him then we have to bail

	if (!fCDCDriver)
	{
		ALERT(0, fControlInterface->GetInterfaceNumber(), "start - Failed to find the CDC driver");
        return false;
	}
    
    if (!configureECM())
    {
        ALERT(0, 0, "start - configureECM failed");
        return false;
    }
    
    if (!allocateResources()) 
    {
        ALERT(0, 0, "start - allocateResources failed");
        return false;
    }
	
	fControlInterface->retain();

#if 0    
    if (!initForPM(provider))
    {
        ALERT(0, 0, "start - initForPM failed");
        return false;
    }
#endif
    
    registerService();
    
    XTRACE(this, 0, 0, "start - successful");
    
    return true;
        	
}/* end start */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::stop
//
//		Inputs:		provider - my provider
//
//		Outputs:	
//
//		Desc:		Stops the driver
//
/****************************************************************************************************/

void AppleUSBCDCECMControl::stop(IOService *provider)
{
    
    XTRACE(this, 0, 0, "stop");
    
    releaseResources();
	
//	PMstop();
    
    super::stop(provider);
    
    return;
	
}/* end stop */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::configureECM
//
//		Inputs:		
//
//		Outputs:	return Code - true (device configured), false (device not configured)
//
//		Desc:		Finds the configurations and then the appropriate interfaces etc.
//
/****************************************************************************************************/

bool AppleUSBCDCECMControl::configureECM()
{
    
    XTRACE(this, 0, 0, "configureECM");
    
    fCommInterfaceNumber = fControlInterface->GetInterfaceNumber();
    XTRACE(this, 0, fCommInterfaceNumber, "configureECM - Comm interface number.");
    	
    if (!getFunctionalDescriptors())
    {
        ALERT(0, 0, "configureECM - getFunctionalDescriptors failed");
//        releaseResources();
        return false;
    }
    
    return true;

}/* end configureECM */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::getFunctionalDescriptors
//
//		Inputs:		
//
//		Outputs:	return - true (descriptors ok), false (somethings not right or not supported)	
//
//		Desc:		Finds all the functional descriptors for the specific interface
//
/****************************************************************************************************/

bool AppleUSBCDCECMControl::getFunctionalDescriptors()
{
    bool				gotDescriptors = false;
    bool				configok = true;
    bool				enet = false;
    IOReturn				ior;
    const HDRFunctionalDescriptor 	*funcDesc = NULL;
    ECMFunctionalDescriptor		*ENETFDesc;
    UnionFunctionalDescriptor		*UNNFDesc;
    UInt8				serString;
    char 				ascii_mac[14];
    UInt16 				i;
       
    XTRACE(this, 0, 0, "getFunctionalDescriptors");
        
    do
    {
        funcDesc = (const HDRFunctionalDescriptor *)fControlInterface->FindNextAssociatedDescriptor((void*)funcDesc, CS_INTERFACE);
        if (!funcDesc)
        {
            gotDescriptors = true;
        } else {
            switch (funcDesc->bDescriptorSubtype)
            {
                case Header_FunctionalDescriptor:
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - Header Functional Descriptor");
                    break;
                case ECM_Functional_Descriptor:
                    ENETFDesc = (ECMFunctionalDescriptor *)funcDesc;
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - Ethernet Functional Descriptor");
                    enet = true;
                    break;
                case Union_FunctionalDescriptor:
                    UNNFDesc = (UnionFunctionalDescriptor *)funcDesc;
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - Union Functional Descriptor");
                    if (UNNFDesc->bFunctionLength > sizeof(FunctionalDescriptorHeader))
                    {
                        fDataInterfaceNumber = UNNFDesc->bSlaveInterface[0];		// Use the first slave (may need to revisit)
                    } else {
                        XTRACE(this, UNNFDesc->bFunctionLength, 0, "getFunctionalDescriptors - Union descriptor length error");
                    }
                    break;
                default:
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - unknown Functional Descriptor");
                    break;
            }
        }
    } while(!gotDescriptors);

    if (!enet)
    {
//        configok = false;					// The Enet Func. Desc.  must be present

            // We're going to make some assumptions for now
        
        fOutputPktsOK = true;
        fInputPktsOK = true;
        fOutputErrsOK = true;
        fInputErrsOK = true;
        
        fEthernetStatistics[0] = 0;
        fEthernetStatistics[1] = 0;
        fEthernetStatistics[2] = 0;
        fEthernetStatistics[3] = 0;
        
        fMcFilters = 0;
        
        serString = fControlInterface->GetDevice()->GetSerialNumberStringIndex();	// Default to the serial number string
        ior = fControlInterface->GetDevice()->GetStringDescriptor(serString, (char *)&ascii_mac, 13);
        if (ior == kIOReturnSuccess)
        {
            for (i = 0; i < 6; i++)
            {
                fEaddr[i] = (Asciihex_to_binary(ascii_mac[i*2]) << 4) | Asciihex_to_binary(ascii_mac[i*2+1]);
            }
        } else {
                ALERT(0, 0, "getFunctionalDescriptors - Error retrieving Ethernet address (serial string)");
                configok = false;
        }
    } else {
    
            // Determine who is collecting the input/output network stats.
    
        if (!(ENETFDesc->bmEthernetStatistics[0] & kXMIT_OK))
        {
            fOutputPktsOK = true;
        } else {
            fOutputPktsOK = false;
        }
        if (!(ENETFDesc->bmEthernetStatistics[0] & kRCV_OK))
        {
            fInputPktsOK = true;
        } else {
            fInputPktsOK = false;
        }
        if (!(ENETFDesc->bmEthernetStatistics[0] & kXMIT_ERROR))
        {
            fOutputErrsOK = true;
        } else {
            fOutputErrsOK = false;
        }
        if (!(ENETFDesc->bmEthernetStatistics[0] & kRCV_ERROR))
        {
            fInputErrsOK = true;
        } else {
            fInputErrsOK = false;
        }
        
            // Save the stats (it's bit mapped)
        
        fEthernetStatistics[0] = ENETFDesc->bmEthernetStatistics[0];
        fEthernetStatistics[1] = ENETFDesc->bmEthernetStatistics[1];
        fEthernetStatistics[2] = ENETFDesc->bmEthernetStatistics[2];
        fEthernetStatistics[3] = ENETFDesc->bmEthernetStatistics[3];
        
            // Save the multicast filters (remember it's intel format)
        
        fMcFilters = USBToHostWord(ENETFDesc->wNumberMCFilters);
        
            // Get the Ethernet address
    
        if (ENETFDesc->iMACAddress != 0)
        {	
            ior = fControlInterface->GetDevice()->GetStringDescriptor(ENETFDesc->iMACAddress, (char *)&ascii_mac, 13);
            if (ior == kIOReturnSuccess)
            {
                for (i = 0; i < 6; i++)
                {
                    fEaddr[i] = (Asciihex_to_binary(ascii_mac[i*2]) << 4) | Asciihex_to_binary(ascii_mac[i*2+1]);
                }
                fMax_Block_Size = USBToHostWord(ENETFDesc->wMaxSegmentSize);
                XTRACE(this, 0, fMax_Block_Size, "getFunctionalDescriptors - Maximum segment size");
            } else {
                ALERT(0, 0, "getFunctionalDescriptors - Error retrieving Ethernet address");
                configok = false;
            }
        } else {
            ALERT(0, 0, "getFunctionalDescriptors - Ethernet address is zero");
            configok = false;
        }
    }
    
    return configok;
    
}/* end getFunctionalDescriptors */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::dataAcquired
//
//		Inputs:		netStats - Network statistics structure address
//				etherStats - Ethernet statistics structure address
//
//		Outputs:	return Code - true (it worked), false (it didn't)
//
//		Desc:		Tells this driver the data driver's port has been acquired
//
/****************************************************************************************************/

bool AppleUSBCDCECMControl::dataAcquired(IONetworkStats *netStats, IOEthernetStats *etherStats)
{
    IOReturn 	rtn = kIOReturnSuccess; 
    
    XTRACE(this, 0, 0, "dataAcquired");
    
            // Read the comm interrupt pipe for status (if we have one)
		
    fCommCompletionInfo.target = this;
    fCommCompletionInfo.action = commReadComplete;
    fCommCompletionInfo.parameter = NULL;
    
    if (fCommPipe)
    {
        rtn = fCommPipe->Read(fCommPipeMDP, &fCommCompletionInfo, NULL);
    } else {
		if (fDataDriver)
		{
			fDataDriver->fLinkStatus = fLinkStatus;
		}
	}
    if (rtn == kIOReturnSuccess)
    {

            // Set up the management Element Request completion routine
			
        fMERCompletionInfo.target = this;
        fMERCompletionInfo.action = merWriteComplete;
        fMERCompletionInfo.parameter = NULL;
        
            // Set up the statistics request completion routine:

        fStatsCompletionInfo.target = this;
        fStatsCompletionInfo.action = statsWriteComplete;
        fStatsCompletionInfo.parameter = NULL;
        
     } else {
        XTRACE(this, 0, rtn, "dataAcquired - Reading the interrupt pipe failed");
        return false;
    }
    
    fpNetStats = netStats;
    fpEtherStats = etherStats;
    
    fdataAcquired = true;
    
    return true;

}/* end dataAcquired */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::dataReleased
//
//		Inputs:		None
//
//		Outputs:	None
//
//		Desc:		Tells this driver the data driver's port has been released
//
/****************************************************************************************************/

void AppleUSBCDCECMControl::dataReleased()
{
    
    XTRACE(this, 0, 0, "dataReleased");
    
	if (fCommPipe)
	{
		fCommPipe->Abort();
	}
    fdataAcquired = false;
    
}/* end dataReleased */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::allocateResources
//
//		Inputs:		
//
//		Outputs:	return code - true (allocate was successful), false (it failed)
//
//		Desc:		Finishes up the rest of the configuration and gets all the endpoints open etc.
//
/****************************************************************************************************/

bool AppleUSBCDCECMControl::allocateResources()
{
    IOUSBFindEndpointRequest	epReq;

    XTRACE(this, 0, 0, "allocateResources.");

        // Open the end point and get the buffers

    if (!fControlInterface->open(this))
    {
        ALERT(0, 0, "allocateResources - open comm interface failed.");
        return false;
    }
        // Interrupt pipe

    epReq.type = kUSBInterrupt;
    epReq.direction = kUSBIn;
    fCommPipe = fControlInterface->FindNextPipe(0, &epReq);
    if (!fCommPipe)
    {
        XTRACE(this, 0, 0, "allocateResources - no interrupt in pipe.");
        fCommPipeMDP = NULL;
        fCommPipeBuffer = NULL;
        fLinkStatus = 1;					// Mark it active cause we'll never get told
    } else {
        XTRACE(this, epReq.maxPacketSize << 16 |epReq.interval, 0, "allocateResources - comm pipe.");

            // Allocate Memory Descriptor Pointer with memory for the Interrupt pipe:

        fCommPipeMDP = IOBufferMemoryDescriptor::withCapacity(COMM_BUFF_SIZE, kIODirectionIn);
        if (!fCommPipeMDP)
        {
            XTRACE(this, 0, 0, "allocateResources - Couldn't allocate MDP for interrupt pipe");
            return false;
        }

        fCommPipeBuffer = (UInt8*)fCommPipeMDP->getBytesNoCopy();
        XTRACEP(this, 0, fCommPipeBuffer, "allocateResources - comm buffer");
    }
    
    return true;
	
}/* end allocateResources */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::releaseResources
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Frees up the resources allocated in allocateResources
//
/****************************************************************************************************/

void AppleUSBCDCECMControl::releaseResources()
{
    XTRACE(this, 0, 0, "releaseResources");
	
    if (fControlInterface)	
    {
        fControlInterface->close(this);
        fControlInterface->release();
        fControlInterface = NULL;		
    }
    
    if (fCommPipeMDP)	
    { 
        fCommPipeMDP->release();	
        fCommPipeMDP = 0; 
    }
    	
}/* end releaseResources */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::USBSetMulticastFilter
//
//		Inputs:		addrs - the list of addresses
//				count - How many
//
//		Outputs:	
//
//		Desc:		Set up and send SetMulticastFilter Management Element Request(MER).
//
/****************************************************************************************************/

bool AppleUSBCDCECMControl::USBSetMulticastFilter(IOEthernetAddress *addrs, UInt32 count)
{
    IOReturn		rc;
    IOUSBDevRequest	*MER;
    UInt8		*eaddrs;
    UInt32		eaddLen;
    UInt32		i,j,rnum;
	
    XTRACE(this, fMcFilters, count, "USBSetMulticastFilter");
    
    if (!fControlInterface)
    {
        XTRACE(this, fMcFilters, count, "USBSetMulticastFilter - Control interface has gone");
        return true;
    }

    if (count > (UInt32)(fMcFilters & kFiltersSupportedMask))
    {
        XTRACE(this, 0, 0, "USBSetMulticastFilter - No multicast filters supported");
        return false;
    }

    MER = (IOUSBDevRequest*)IOMalloc(sizeof(IOUSBDevRequest));
    if (!MER)
    {
        XTRACE(this, 0, 0, "USBSetMulticastFilter - allocate MER failed");
        return false;
    }
    bzero(MER, sizeof(IOUSBDevRequest));
	
    eaddLen = count * kIOEthernetAddressSize;
    eaddrs = (UInt8 *)IOMalloc(eaddLen);
    if (!eaddrs)
    {
        XTRACE(this, 0, 0, "USBSetMulticastFilter - allocate address buffer failed");
        return false;
    }
    bzero(eaddrs, eaddLen); 
	
        // Build the filter address buffer
         
    rnum = 0;
    for (i=0; i<count; i++)
    {
        if (rnum > eaddLen)				// Just in case
        {
            break;
        }
        for (j=0; j<kIOEthernetAddressSize; j++)
        {
            eaddrs[rnum++] = addrs->bytes[j];
        }
    }
    
        // Now build the Management Element Request
		
    MER->bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    MER->bRequest = kSet_Ethernet_Multicast_Filter;
    MER->wValue = count;
    MER->wIndex = fCommInterfaceNumber;
    MER->wLength = eaddLen;
    MER->pData = eaddrs;
	
    fMERCompletionInfo.parameter = MER;
	
    rc = fControlInterface->GetDevice()->DeviceRequest(MER, &fMERCompletionInfo);
    if (rc != kIOReturnSuccess)
    {
        XTRACE(this, MER->bRequest, rc, "USBSetMulticastFilter - Error issueing DeviceRequest");
        IOFree(MER->pData, eaddLen);
        IOFree(MER, sizeof(IOUSBDevRequest));
        return false;
    }
    
    return true;

}/* end USBSetMulticastFilter */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::USBSetPacketFilter
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Set up and send SetEthernetPackettFilters Management Element Request(MER).
//
/****************************************************************************************************/

bool AppleUSBCDCECMControl::USBSetPacketFilter()
{
    IOReturn		rc;
    IOUSBDevRequest	*MER;
	
    XTRACE(this, 0, fPacketFilter, "USBSetPacketFilter");
    
    if (!fControlInterface)
    {
        XTRACE(this, 0, fPacketFilter, "USBSetPacketFilter - Control interface has gone");
        return true;
    }
	
    MER = (IOUSBDevRequest*)IOMalloc(sizeof(IOUSBDevRequest));
    if (!MER)
    {
        XTRACE(this, 0, 0, "USBSetPacketFilter - allocate MER failed");
        return false;
    }
    bzero(MER, sizeof(IOUSBDevRequest));
    
        // Now build the Management Element Request
		
    MER->bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    MER->bRequest = kSet_Ethernet_Packet_Filter;
    MER->wValue = fPacketFilter;
    MER->wIndex = fCommInterfaceNumber;
    MER->wLength = 0;
    MER->pData = NULL;
	
    fMERCompletionInfo.parameter = MER;
	
    rc = fControlInterface->GetDevice()->DeviceRequest(MER, &fMERCompletionInfo);
    if (rc != kIOReturnSuccess)
    {
        XTRACE(this, MER->bRequest, rc, "USBSetPacketFilter - DeviceRequest error");
        if (rc == kIOUSBPipeStalled)
        {

            // Clear the stall and try it once more
        
            fControlInterface->GetDevice()->GetPipeZero()->ClearPipeStall(false);
            rc = fControlInterface->GetDevice()->DeviceRequest(MER, &fMERCompletionInfo);
            if (rc != kIOReturnSuccess)
            {
                XTRACE(this, MER->bRequest, rc, "USBSetPacketFilter - DeviceRequest, error a second time");
                IOFree(MER, sizeof(IOUSBDevRequest));
                return false;
            }
        }
    }
    
    return true;
    
}/* end USBSetPacketFilter */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::checkInterfaceNumber
//
//		Inputs:		dataDriver - the data driver enquiring
//
//		Outputs:	
//
//		Desc:		Called by the data driver to ask if this is the correct
//				control interface driver. 
//
/****************************************************************************************************/

bool AppleUSBCDCECMControl::checkInterfaceNumber(AppleUSBCDCECMData *dataDriver)
{
    IOUSBInterface	*dataInterface;

    XTRACEP(this, 0, dataDriver, "checkInterfaceNumber");
    
        // First check we have the same provider (Device)
    
    dataInterface = OSDynamicCast(IOUSBInterface, dataDriver->getProvider());
    if (dataInterface == NULL)
    {
        XTRACE(this, 0, 0, "checkInterfaceNumber - Error getting Data provider");
        return FALSE;
    }
    
    XTRACEP(this, dataInterface->GetDevice(), fControlInterface->GetDevice(), "checkInterfaceNumber - Checking device");
    if (dataInterface->GetDevice() == fControlInterface->GetDevice())
    {
    
            // Then check to see if it's the correct data interface number
    
        if (dataDriver->fDataInterfaceNumber == fDataInterfaceNumber)
        {
            fDataDriver = dataDriver;
            return true;
        } else {
            XTRACE(this, dataDriver->fDataInterfaceNumber, fDataInterfaceNumber, "checkInterfaceNumber - Not correct interface number");
        }
    } else {
        XTRACE(this, 0, 0, "checkInterfaceNumber - Not correct device");
    }

    return false;

}/* end checkInterfaceNumber */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::checkPipe
//
//		Inputs:		thePipe - the pipe
//				devReq - true(send CLEAR_FEATURE), false(only if status returns stalled)
//
//		Outputs:	
//
//		Desc:		Clear a stall on the specified pipe. If ClearPipeStall is issued
//				all outstanding I/O is returned with kIOUSBTransactionReturned and
//				a CLEAR_FEATURE Endpoint stall is sent.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCECMControl::checkPipe(IOUSBPipe *thePipe, bool devReq)
{
    IOReturn 	rtn = kIOReturnSuccess;
    
    XTRACEP(this, 0, thePipe, "checkPipe");
    
    if (!devReq)
    {
        rtn = thePipe->GetPipeStatus();
        if (rtn != kIOUSBPipeStalled)
        {
            XTRACE(this, 0, 0, "checkPipe - Pipe not stalled");
            return rtn;
        }
    }
    
    rtn = thePipe->ClearPipeStall(true);
    if (rtn == kIOReturnSuccess)
    {
        XTRACE(this, 0, 0, "checkPipe - ClearPipeStall Successful");
    } else {
        XTRACE(this, 0, rtn, "checkPipe - ClearPipeStall Failed");
    }
    
    return rtn;

}/* end checkPipe */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::statsProcessing
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Handles stats gathering.
//
/****************************************************************************************************/

bool AppleUSBCDCECMControl::statsProcessing()
{
    UInt32		*enetStats;
    UInt16		currStat;
    IOReturn		rc;
    IOUSBDevRequest	*STREQ;
    bool		statOk = false;

//    XTRACE(this, 0, 0, "statsProcessing");

    enetStats = (UInt32 *)&fEthernetStatistics;
    if (*enetStats == 0)
    {
        XTRACE(this, 0, 0, "statsProcessing - No Ethernet statistics defined");
        return false;						// and don't bother us again
    }
    
    if ((fpNetStats == NULL) || (fpEtherStats == NULL))		// Means we're not ready yet
    {
        XTRACE(this, 0, 0, "statsProcessing - Not ready");
        return true;
    }
    
    if (fReady == false)
    {
        XTRACE(this, 0, 0, "statsProcessing - Spurious");    
    } else {
    
            // Only do it if it's not already in progress
    
        if (!fStatInProgress)
        {

                // Check if the stat we're currently interested in is supported
            
            currStat = stats[fCurrStat++];
            if (fCurrStat >= numStats)
            {
                fCurrStat = 0;
            }
            switch(currStat)
            {
                case kXMIT_OK_REQ:
                    if (fEthernetStatistics[0] & kXMIT_OK)
                    {
                        statOk = true;
                    }
                    break;
                case kRCV_OK_REQ:
                    if (fEthernetStatistics[0] & kRCV_OK)
                    {
                        statOk = true;
                    }
                    break;
                case kXMIT_ERROR_REQ:
                    if (fEthernetStatistics[0] & kXMIT_ERROR_REQ)
                    {
                        statOk = true;
                    }
                    break;
                case kRCV_ERROR_REQ:
                    if (fEthernetStatistics[0] & kRCV_ERROR_REQ)
                    {
                        statOk = true;
                    }
                    break;
                case kRCV_CRC_ERROR_REQ:
                    if (fEthernetStatistics[2] & kRCV_CRC_ERROR)
                    {
                        statOk = true;
                    }
                    break;
                case kRCV_ERROR_ALIGNMENT_REQ:
                    if (fEthernetStatistics[2] & kRCV_ERROR_ALIGNMENT)
                    {
                        statOk = true;
                    }
                    break;
                case kXMIT_ONE_COLLISION_REQ:
                    if (fEthernetStatistics[2] & kXMIT_ONE_COLLISION)
                    {
                        statOk = true;
                    }
                    break;
                case kXMIT_MORE_COLLISIONS_REQ:
                    if (fEthernetStatistics[2] & kXMIT_MORE_COLLISIONS)
                    {
                        statOk = true;
                    }
                    break;
                case kXMIT_DEFERRED_REQ:
                    if (fEthernetStatistics[2] & kXMIT_DEFERRED)
                    {
                        statOk = true;
                    }
                    break;
                case kXMIT_MAX_COLLISION_REQ:
                    if (fEthernetStatistics[2] & kXMIT_MAX_COLLISION)
                    {
                        statOk = true;
                    }
                    break;
                case kRCV_OVERRUN_REQ:
                    if (fEthernetStatistics[3] & kRCV_OVERRUN)
                    {
                        statOk = true;
                    }
                    break;
                case kXMIT_TIMES_CARRIER_LOST_REQ:
                    if (fEthernetStatistics[3] & kXMIT_TIMES_CARRIER_LOST)
                    {
                        statOk = true;
                    }
                    break;
                case kXMIT_LATE_COLLISIONS_REQ:
                    if (fEthernetStatistics[3] & kXMIT_LATE_COLLISIONS)
                    {
                        statOk = true;
                    }
                    break;
                default:
                    break;
            }
        }

        if (statOk)
        {
            STREQ = (IOUSBDevRequest*)IOMalloc(sizeof(IOUSBDevRequest));
            if (!STREQ)
            {
                XTRACE(this, 0, 0, "statsProcessing - allocate STREQ failed");
            } else {
                bzero(STREQ, sizeof(IOUSBDevRequest));
        
                    // Now build the Statistics Request
		
                STREQ->bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
                STREQ->bRequest = kGet_Ethernet_Statistics;
                STREQ->wValue = currStat;
                STREQ->wIndex = fCommInterfaceNumber;
                STREQ->wLength = 4;
                STREQ->pData = &fStatValue;
	
                fStatsCompletionInfo.parameter = STREQ;
	
                rc = fControlInterface->GetDevice()->DeviceRequest(STREQ, &fStatsCompletionInfo);
                if (rc != kIOReturnSuccess)
                {
                    XTRACE(this, STREQ->bRequest, rc, "statsProcessing - Error issueing DeviceRequest");
                    IOFree(STREQ, sizeof(IOUSBDevRequest));
                } else {
                    fStatInProgress = true;
                }
            }
        }
    }

    return true;

}/* end statsProcessing */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::message
//
//		Inputs:		type - message type
//				provider - my provider
//				argument - additional parameters
//
//		Outputs:	return Code - kIOReturnSuccess
//
//		Desc:		Handles IOKit messages. 
//
/****************************************************************************************************/

IOReturn AppleUSBCDCECMControl::message(UInt32 type, IOService *provider, void *argument)
{
    IOReturn	ior;
	
    XTRACE(this, 0, type, "message");
	
    switch (type)
    {
        case kIOMessageServiceIsTerminated:
            XTRACE(this, 0, type, "message - kIOMessageServiceIsTerminated");
			if (fDataDriver)
			{
				fDataDriver->message(kIOMessageServiceIsTerminated, fControlInterface, NULL);
			}
            fTerminate = true;		// we're being terminated (unplugged)
            releaseResources();
            return kIOReturnSuccess;			
        case kIOMessageServiceIsSuspended: 	
            XTRACE(this, 0, type, "message - kIOMessageServiceIsSuspended");
            break;			
        case kIOMessageServiceIsResumed: 	
            XTRACE(this, 0, type, "message - kIOMessageServiceIsResumed");
            break;			
        case kIOMessageServiceIsRequestingClose: 
            XTRACE(this, 0, type, "message - kIOMessageServiceIsRequestingClose"); 
            break;
        case kIOMessageServiceWasClosed: 	
            XTRACE(this, 0, type, "message - kIOMessageServiceWasClosed"); 
            break;
        case kIOMessageServiceBusyStateChange: 	
            XTRACE(this, 0, type, "message - kIOMessageServiceBusyStateChange"); 
            break;
        case kIOUSBMessagePortHasBeenResumed: 	
            XTRACE(this, 0, type, "message - kIOUSBMessagePortHasBeenResumed");
            if (fCommDead)					// If it's dead try and resurrect it
            {
                ior = fCommPipe->Read(fCommPipeMDP, &fCommCompletionInfo, NULL);
                if (ior != kIOReturnSuccess)
                {
                    XTRACE(this, 0, ior, "message - Read io error");
                } else {
                    fCommDead = false;
                }
            }
            return kIOReturnSuccess;
        case kIOUSBMessageHubResumePort:
            XTRACE(this, 0, type, "message - kIOUSBMessageHubResumePort");
            break;
        default:
            XTRACE(this, 0, type, "message - unknown message"); 
            break;
    }
    
    return super::message(type, provider, argument);
    
}/* end message */

#if 0
/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::initForPM
//
//		Inputs:		provider - my provider
//
//		Outputs:	return code - true(initialized), false(failed)
//
//		Desc:		Add ourselves to the power management tree so we can do
//				the right thing on sleep/wakeup. 
//
/****************************************************************************************************/

bool AppleUSBCDCECMControl::initForPM(IOService *provider)
{
    XTRACE(this, 0, 0, "initForPM");
    
    fPowerState = kCDCPowerOnState;				// init our power state to be 'on'
    PMinit();							// init power manager instance variables
    provider->joinPMtree(this);					// add us to the power management tree
    if (pm_vars != NULL)
    {
    
            // register ourselves with ourself as policy-maker
        
        registerPowerDriver(this, gOurPowerStates, kNumCDCStates);
        return true;
    } else {
        ALERT(0, 0, "initForPM - Initializing power manager failed");
    }
    
    return false;
    
}/* end initForPM */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::initialPowerStateForDomainState
//
//		Inputs:		flags - 
//
//		Outputs:	return code - Current power state
//
//		Desc:		Request for our initial power state. 
//
/****************************************************************************************************/

unsigned long AppleUSBCDCECMControl::initialPowerStateForDomainState(IOPMPowerFlags flags)
{

    XTRACE(this, 0, flags, "initialPowerStateForDomainState");
    
    return fPowerState;
    
}/* end initialPowerStateForDomainState */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMControl::setPowerState
//
//		Inputs:		powerStateOrdinal - on/off
//
//		Outputs:	return code - IOPMNoErr, IOPMAckImplied or IOPMNoSuchState
//
//		Desc:		Request to turn device on or off. 
//
/****************************************************************************************************/

IOReturn AppleUSBCDCECMControl::setPowerState(unsigned long powerStateOrdinal, IOService *whatDevice)
{

    XTRACE(this, 0, powerStateOrdinal, "setPowerState");
    
    if (powerStateOrdinal == kCDCPowerOffState || powerStateOrdinal == kCDCPowerOnState)
    {
        if (powerStateOrdinal == fPowerState)
            return IOPMAckImplied;

        fPowerState = powerStateOrdinal;
        if (fPowerState == kCDCPowerOnState)
        {
			if (fDataDriver)
			{
				fDataDriver->fResetState = kResetNeeded;
				fDataDriver->fReady = FALSE;
			}
        }
    
        return IOPMAckImplied;
    }
    
    return IOPMAckImplied;
    
}/* end setPowerState */
#endif