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

    /* AppleUSBCDCACMControl.cpp - MacOSX implementation of			*/
    /* USB Communication Device Class (CDC) Driver, ACM Control Interface.	*/

#include <machine/limits.h>			/* UINT_MAX */
#include <libkern/OSByteOrder.h>

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMessage.h>

#include <IOKit/pwr_mgt/RootDomain.h>

#include <IOKit/usb/IOUSBBus.h>
#include <IOKit/usb/IOUSBNub.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBInterface.h>

#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/serial/IOSerialDriverSync.h>
#include <IOKit/serial/IOModemSerialStreamSync.h>
#include <IOKit/serial/IORS232SerialStreamSync.h>

#include <UserNotification/KUNCUserNotifications.h>

#define DEBUG_NAME "AppleUSBCDCACMControl"

#include "AppleUSBCDCACM.h"
#include "AppleUSBCDCACMControl.h"

#define MIN_BAUD (50 << 1)

    // Globals

#if USE_ELG
    com_apple_iokit_XTrace	*gXTrace = 0;
#endif

//AppleUSBCDCACMData		*gDataDriver = NULL;

static IOPMPowerState gOurPowerStates[kNumCDCStates] =
{
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
};

#define super IOService

OSDefineMetaClassAndStructors(AppleUSBCDCACMControl, IOService);

#if USE_ELG
/****************************************************************************************************/
//
//		Function:	findKernelLoggerAC
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Just like the name says
//
/****************************************************************************************************/

IOReturn findKernelLoggerAC()
{
    OSIterator		*iterator = NULL;
    OSDictionary	*matchingDictionary = NULL;
    IOReturn		error = 0;
	
	// Get matching dictionary
	
    matchingDictionary = IOService::serviceMatching("com_apple_iokit_XTrace");
    if (!matchingDictionary)
    {
        error = kIOReturnError;
        IOLog(DEBUG_NAME "[findKernelLoggerAC] Couldn't create a matching dictionary.\n");
        goto exit;
    }
	
	// Get an iterator
	
    iterator = IOService::getMatchingServices(matchingDictionary);
    if (!iterator)
    {
        error = kIOReturnError;
        IOLog(DEBUG_NAME "[findKernelLoggerAC] No XTrace logger found.\n");
        goto exit;
    }
	
	// User iterator to find each com_apple_iokit_XTrace instance. There should be only one, so we
	// won't iterate
	
    gXTrace = (com_apple_iokit_XTrace*)iterator->getNextObject();
    if (gXTrace)
    {
        IOLog(DEBUG_NAME "[findKernelLoggerAC] Found XTrace logger at %p.\n", gXTrace);
    }
	
exit:
	
    if (error != kIOReturnSuccess)
    {
        gXTrace = NULL;
        IOLog(DEBUG_NAME "[findKernelLoggerAC] Could not find a logger instance. Error = %X.\n", error);
    }
	
    if (matchingDictionary)
        matchingDictionary->release();
            
    if (iterator)
        iterator->release();
		
    return error;
    
}/* end findKernelLoggerAC */
#endif

/****************************************************************************************************/
//
//		Function:	findCDCDriverAC
//
//		Inputs:		controlAddr - my address
//
//		Outputs:	
//
//		Desc:		Finds the initiating CDC driver
//
/****************************************************************************************************/

AppleUSBCDC *findCDCDriverAC(void *controlAddr)
{
    AppleUSBCDCACMControl	*me = (AppleUSBCDCACMControl *)controlAddr;
    AppleUSBCDC		*CDCDriver = NULL;
    bool		driverOK = false;
    OSIterator		*iterator = NULL;
    OSDictionary	*matchingDictionary = NULL;
    
    XTRACE(me, 0, 0, "findCDCDriverAC");
        
        // Get matching dictionary
       	
    matchingDictionary = IOService::serviceMatching("AppleUSBCDC");
    if (!matchingDictionary)
    {
        XTRACE(me, 0, 0, "findCDCDriverAC - Couldn't create a matching dictionary");
        return NULL;
    }
    
	// Get an iterator
	
    iterator = IOService::getMatchingServices(matchingDictionary);
    if (!iterator)
    {
        XTRACE(me, 0, 0, "findCDCDriverAC - No AppleUSBCDC driver found!");
        matchingDictionary->release();
        return NULL;
    }

    	// Iterate until we find our matching CDC driver
                
    CDCDriver = (AppleUSBCDC *)iterator->getNextObject();
    while (CDCDriver)
    {
        XTRACE(me, 0, CDCDriver, "findCDCDriverAC - CDC driver candidate");
        
        if (me->fControlInterface->GetDevice() == CDCDriver->getCDCDevice())
        {
            XTRACE(me, 0, CDCDriver, "findCDCDriverAD - Found our CDC driver");
            driverOK = CDCDriver->confirmControl(kUSBAbstractControlModel, me->fControlInterface);
            break;
        }
        CDCDriver = (AppleUSBCDC *)iterator->getNextObject();
    }

    matchingDictionary->release();
    iterator->release();
    
    if (!CDCDriver)
    {
        XTRACE(me, 0, 0, "findCDCDriverAC - CDC driver not found");
        return NULL;
    }
   
    if (!driverOK)
    {
        XTRACE(me, kUSBAbstractControlModel, 0, "findCDCDriverAC - Not my interface");
        return NULL;
    }

    return CDCDriver;
    
}/* end findCDCDriverAC */

	// Encode the 4 modem status bits (so we only make one call to setState)

static UInt32 sMapModemStates[16] = 
{
	             0 |              0 |              0 |              0, // 0000
	             0 |              0 |              0 | PD_RS232_S_DCD, // 0001
	             0 |              0 | PD_RS232_S_DSR |              0, // 0010
	             0 |              0 | PD_RS232_S_DSR | PD_RS232_S_DCD, // 0011
	             0 | PD_RS232_S_BRK |              0 |              0, // 0100
	             0 | PD_RS232_S_BRK |              0 | PD_RS232_S_DCD, // 0101
	             0 | PD_RS232_S_BRK | PD_RS232_S_DSR |              0, // 0110
	             0 | PD_RS232_S_BRK | PD_RS232_S_DSR | PD_RS232_S_DCD, // 0111
	PD_RS232_S_RNG |              0 |              0 |              0, // 1000
	PD_RS232_S_RNG |              0 |              0 | PD_RS232_S_DCD, // 1001
	PD_RS232_S_RNG |              0 | PD_RS232_S_DSR |              0, // 1010
	PD_RS232_S_RNG |              0 | PD_RS232_S_DSR | PD_RS232_S_DCD, // 1011
	PD_RS232_S_RNG | PD_RS232_S_BRK |              0 |              0, // 1100
	PD_RS232_S_RNG | PD_RS232_S_BRK |              0 | PD_RS232_S_DCD, // 1101
	PD_RS232_S_RNG | PD_RS232_S_BRK | PD_RS232_S_DSR |              0, // 1110
	PD_RS232_S_RNG | PD_RS232_S_BRK | PD_RS232_S_DSR | PD_RS232_S_DCD, // 1111
};

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMControl::commReadComplete
//
//		Inputs:		obj - me
//				rc - return code
//				remaining - what's left
//
//		Outputs:	None
//
//		Desc:		Interrupt pipe (Comm interface) read completion routine
//
/****************************************************************************************************/

void AppleUSBCDCACMControl::commReadComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    AppleUSBCDCACMControl	*me = (AppleUSBCDCACMControl*)obj;
    IOReturn			ior;
    UInt32			dLen;
    UInt16			*tState;
    UInt32			tempS, value, mask;
    
    XTRACE(me, rc, 0, "commReadComplete");
    
    if (me->fStopping)
        return;

    if (rc == kIOReturnSuccess)					// If operation returned ok
    {
        dLen = COMM_BUFF_SIZE - remaining;
        XTRACE(me, 0, dLen, "commReadComplete - data length");
		
            // Now look at the state stuff
		
        if ((dLen > 7) && (me->fCommPipeBuffer[1] == kUSBSERIAL_STATE))
        {
            tState = (UInt16 *)&me->fCommPipeBuffer[8];
            tempS = USBToHostWord(*tState);
            XTRACE(me, 0, tempS, "commReadComplete - kUSBSERIAL_STATE");
			
            mask = sMapModemStates[15];				// All 4 on
            value = sMapModemStates[tempS & 15];		// now the status bits
            if (me->fDataDriver)
            {
                me->fDataDriver->setState(value, mask, NULL);
            }
        }
    } else {
        XTRACE(me, 0, rc, "commReadComplete - error");
        if (rc != kIOReturnAborted)
        {
			if (rc == kIOUSBPipeStalled)
			{
				rc = me->checkPipe(me->fCommPipe, true);
			} else {
				rc = me->checkPipe(me->fCommPipe, false);
			}
            if (rc != kIOReturnSuccess)
            {
                XTRACE(me, 0, rc, "dataReadComplete - clear stall failed (trying to continue)");
            }
        }
    }
    
        // Queue the next read only if not aborted
	
    if (rc != kIOReturnAborted)
    {
        ior = me->fCommPipe->Read(me->fCommPipeMDP, &me->fCommCompletionInfo, NULL);
        if (ior != kIOReturnSuccess)
        {
            XTRACE(me, 0, rc, "commReadComplete - Read io error");
			me->fReadDead = true;
        }
    } else {
		me->fReadDead = true;
	}
	
}/* end commReadComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMControl::merWriteComplete
//
//		Inputs:		obj - me
//				param - MER 
//				rc - return code
//				remaining - what's left
//
//		Outputs:	None
//
//		Desc:		Management Element Request write completion routine
//
/****************************************************************************************************/

void AppleUSBCDCACMControl::merWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    AppleUSBCDCACMControl	*me = (AppleUSBCDCACMControl *)obj;
    IOUSBDevRequest		*MER = (IOUSBDevRequest *)param;
    UInt16			dataLen;
    
    XTRACE(me, 0, remaining, "merWriteComplete");
    
    if (me->fStopping)
    {
        if (MER)
        {
            dataLen = MER->wLength;
            if ((dataLen != 0) && (MER->pData))
            {
                IOFree(MER->pData, dataLen);
            }
            IOFree(MER, sizeof(IOUSBDevRequest));
        }
        return;
    }
    
    if (MER)
    {
        if (rc == kIOReturnSuccess)
        {
            XTRACE(me, MER->bRequest, remaining, "merWriteComplete - request");
        } else {
            XTRACE(me, MER->bRequest, rc, "merWriteComplete - io err");
			if (rc == kIOUSBPipeStalled)
			{
				rc = me->checkPipe(me->fCommPipe, false);
			}
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
			if (rc == kIOUSBPipeStalled)
			{
				rc = me->checkPipe(me->fCommPipe, false);
			}
        }
    }
	
}/* end merWriteComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMControl::start
//
//		Inputs:		provider - my provider
//
//		Outputs:	Return code - true (it's me), false (sorry it probably was me, but I can't configure it)
//
//		Desc:		This is called once it has beed determined I'm probably the best 
//				driver for this interface.
//
/****************************************************************************************************/

bool AppleUSBCDCACMControl::start(IOService *provider)
{

    fTerminate = false;
    fStopping = false;
    fdataAcquired = false;
    fCommPipeMDP = NULL;
    fCommPipe = NULL;
    fCommPipeBuffer = NULL;
	fReadDead = false;
    
#if USE_ELG
    XTraceLogInfo	*logInfo;
    
    findKernelLoggerAC();
    if (gXTrace)
    {
        gXTrace->retain();		// don't let it unload ...
        XTRACE(this, 0, 0xbeefbeef, "Hello from start");
        logInfo = gXTrace->LogGetInfo();
        IOLog("AppleUSBCDCACMControl: start - Log is at %x\n", (unsigned int)logInfo);
    } else {
        return false;
    }
#endif

    XTRACE(this, 0, provider, "start - provider.");
    
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

    // If our IOUSBInterface has a "do not match" property, it means that we should not match and need 
    // to bail.  See rdar://3716623
    
    OSBoolean * boolObj = OSDynamicCast( OSBoolean, provider->getProperty("kDoNotClassMatchThisInterface") );
    if ( boolObj && boolObj->isTrue() )
    {
        ALERT(0, 0, "start - provider doesn't want us to match");
        return false;
    }
    
    if (!configureACM())
    {
        ALERT(0, 0, "start - configureACM failed");
        return false;
    }
    
    if (!allocateResources()) 
    {
        ALERT(0, 0, "start - allocateResources failed");
        return false;
    }
    
    if (!initForPM(provider))
    {
        ALERT(0, 0, "start - initForPM failed");
        return false;
    }
    
    fControlInterface->retain();
    
    registerService();
        
    XTRACE(this, 0, 0, "start - successful");
    
    return true;
    	
}/* end start */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMControl::stop
//
//		Inputs:		provider - my provider
//
//		Outputs:	None
//
//		Desc:		Stops the driver
//
/****************************************************************************************************/

void AppleUSBCDCACMControl::stop(IOService *provider)
{
    
    XTRACE(this, 0, 0, "stop");
    
    fStopping = true;
    
    releaseResources();
	
	PMstop();
                    
    super::stop(provider);

}/* end stop */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMControl::configureACM
//
//		Inputs:		
//
//		Outputs:	return Code - true (configured), false (not configured)
//
//		Desc:		Configures the Abstract Control Model interface etc.
//
/****************************************************************************************************/

bool AppleUSBCDCACMControl::configureACM()
{
    UInt8	protocol;
    
    XTRACE(this, 0, 0, "configureACM");
    
    protocol = fControlInterface->GetInterfaceProtocol();
    if (protocol != kUSBv25)
    {
        XTRACE(this, 0, protocol, "configureACM - Unsupported ACM protocol");
        return false;
    }
    
    fCommInterfaceNumber = fControlInterface->GetInterfaceNumber();
    XTRACE(this, 0, fCommInterfaceNumber, "configureACM - Comm interface number.");
    	
    if (!getFunctionalDescriptors())
    {
        XTRACE(this, 0, 0, "configureACM - getFunctionalDescriptors failed");
//        releaseResources();
        return false;
    }
    
    return true;

}/* end configureACM */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMControl::getFunctionalDescriptors
//
//		Inputs:		
//
//		Outputs:	return - true (descriptors ok), false (somethings not right or not supported)	
//
//		Desc:		Finds all the functional descriptors for the specific interface
//
/****************************************************************************************************/

bool AppleUSBCDCACMControl::getFunctionalDescriptors()
{
    bool				gotDescriptors = false;
    UInt16				vers;
    UInt16				*hdrVers;
    const FunctionalDescriptorHeader 	*funcDesc = NULL;
    HDRFunctionalDescriptor		*HDRFDesc;		// hearder functional descriptor
    CMFunctionalDescriptor		*CMFDesc;		// call management functional descriptor
    ACMFunctionalDescriptor		*ACMFDesc;		// abstract control management functional descriptor
    UnionFunctionalDescriptor		*UNNFDesc;		// union functional descriptor
	AppleUSBCDC				*CDCDriver = NULL;
       
    XTRACE(this, 0, 0, "getFunctionalDescriptors");
    
        // Set some defaults just in case and then get the associated functional descriptors
	
    fCMCapabilities = CM_ManagementData + CM_ManagementOnData;
    fACMCapabilities = ACM_DeviceSuppControl + ACM_DeviceSuppBreak;
    fDataInterfaceNumber = 0xFF;
    
    do
    {
        (IOUSBDescriptorHeader*)funcDesc = fControlInterface->FindNextAssociatedDescriptor((void*)funcDesc, CS_INTERFACE);
        if (!funcDesc)
        {
            gotDescriptors = true;				// We're done
        } else {
            switch (funcDesc->bDescriptorSubtype)
            {
                case Header_FunctionalDescriptor:
                    (const FunctionalDescriptorHeader*)HDRFDesc = funcDesc;
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - Header Functional Descriptor");
                    hdrVers = (UInt16 *)&HDRFDesc->bcdCDC1;
                    vers = USBToHostWord(*hdrVers);
                    if (vers > kUSBRel11)
                    {
                        XTRACE(this, vers, kUSBRel11, "getFunctionalDescriptors - Header descriptor version number is incorrect");
                    }
                    break;
                case CM_FunctionalDescriptor:
                    (const FunctionalDescriptorHeader*)CMFDesc = funcDesc;
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - CM Functional Descriptor");
                    if (fDataInterfaceNumber != 0xFF)
                    {
                        if (fDataInterfaceNumber != CMFDesc->bDataInterface)
                        {
                            XTRACE(this, fDataInterfaceNumber, CMFDesc->bDataInterface, "getFunctionalDescriptors - CMF and UNNF disagree");
                        }
                    }
                    fDataInterfaceNumber = CMFDesc->bDataInterface;	// This will be the default
                    fCMCapabilities = CMFDesc->bmCapabilities;
				
                        // Check the configuration supports data management on the data interface (that's all we support)
				
                    if (!(fCMCapabilities & CM_ManagementData))
                    {
                        XTRACE(this, 0, 0, "getFunctionalDescriptors - Interface doesn't support Call Management");
                        return false;
                    }
                    if (!(fCMCapabilities & CM_ManagementOnData))
                    {
                        XTRACE(this, 0, 0, "getFunctionalDescriptors - Interface doesn't support Call Management on Data Interface");
                       //  return false;				// Some devices get this wrong so we'll let it slide
                    }
                    break;
                case ACM_FunctionalDescriptor:
                    (const FunctionalDescriptorHeader*)ACMFDesc = funcDesc;
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - ACM Functional Descriptor");
                    fACMCapabilities = ACMFDesc->bmCapabilities;
                    break;
                case Union_FunctionalDescriptor:
                    (const FunctionalDescriptorHeader*)UNNFDesc = funcDesc;
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - Union Functional Descriptor");
                    if (UNNFDesc->bFunctionLength > sizeof(FunctionalDescriptorHeader))
                    {
						if (fCommInterfaceNumber != UNNFDesc->bMasterInterface)
                        {
                            XTRACE(this, fCommInterfaceNumber, UNNFDesc->bMasterInterface, "getFunctionalDescriptors - Master interface incorrect");
                        }

						if (fDataInterfaceNumber == 0xFF)
                        {
                            fDataInterfaceNumber = UNNFDesc->bSlaveInterface[0];	// Use the first slave (may get overwritten by CMF)
                        }
                        if (fDataInterfaceNumber != UNNFDesc->bSlaveInterface[0])
                        {
                            XTRACE(this, fDataInterfaceNumber, UNNFDesc->bSlaveInterface[1], "getFunctionalDescriptors - Slave interface incorrect");
                        }
                    } else {
                        XTRACE(this, UNNFDesc->bFunctionLength, 0, "getFunctionalDescriptors - Union descriptor length error");
                    }
                    break;
                case CS_FunctionalDescriptor:
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - CS Functional Descriptor");
                    break;
                default:
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - unknown Functional Descriptor");
                    break;
            }
        }
    } while(!gotDescriptors);
    
        // If we get this far and actually have a data interface number we're good to go (maybe...)
		// If not ask the CDC driver if there's only one data interface so we can assume that one

    if (fDataInterfaceNumber == 0xFF)
    {
        XTRACE(this, 0, 0, "getFunctionalDescriptors - No data interface specified");
		CDCDriver = findCDCDriverAC(this);
		if (CDCDriver)
		{
			if (CDCDriver->fDataInterfaceNumber != 0xFF)
			{
				fDataInterfaceNumber = CDCDriver->fDataInterfaceNumber;
				XTRACE(this, fACMCapabilities, fDataInterfaceNumber, "getFunctionalDescriptors - Data interface number (assumed from CDC driver)");
			} else {
				return false;
			}
		} else {
			return false;
		}
    } else {
        XTRACE(this, fACMCapabilities, fDataInterfaceNumber, "getFunctionalDescriptors - Data interface number");
		
			// If these two are equal then this is probably the Conexant Functional descriptor problem
		
		if (fCommInterfaceNumber == fDataInterfaceNumber)
		{
			ALERT(fCommInterfaceNumber, fDataInterfaceNumber, "getFunctionalDescriptors - Descriptors are incorrect, checking...");
			fDataInterfaceNumber = UNNFDesc->bMasterInterface;
		}
    }
    
    return true;
    
}/* end getFunctionalDescriptors */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMControl::dataAcquired
//
//		Inputs:		None
//
//		Outputs:	return Code - true (it worked), false (it didn't)
//
//		Desc:		Tells this driver the data driver's port has been acquired
//
/****************************************************************************************************/

bool AppleUSBCDCACMControl::dataAcquired()
{
    IOReturn 	rtn = kIOReturnSuccess; 
    
    XTRACE(this, 0, 0, "dataAcquired");
    
            // Read the comm interrupt pipe for status
		
    fCommCompletionInfo.target = this;
    fCommCompletionInfo.action = commReadComplete;
    fCommCompletionInfo.parameter = NULL;
		
    rtn = fCommPipe->Read(fCommPipeMDP, &fCommCompletionInfo, NULL);
    if (rtn == kIOReturnSuccess)
    {

            // Set up the management Element Request completion routine
			
        fMERCompletionInfo.target = this;
        fMERCompletionInfo.action = merWriteComplete;
        fMERCompletionInfo.parameter = NULL;
    } else {
        XTRACE(this, 0, rtn, "dataAcquired - Reading the interrupt pipe failed");
        return false;
    }
    
    fdataAcquired = true;
    return true;

}/* end dataAcquired */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMControl::dataReleased
//
//		Inputs:		None
//
//		Outputs:	None
//
//		Desc:		Tells this driver the data driver's port has been released
//
/****************************************************************************************************/

void AppleUSBCDCACMControl::dataReleased()
{
    
    XTRACE(this, 0, 0, "dataReleased");
    
    fCommPipe->Abort();
    fdataAcquired = false;
    
}/* end dataReleased */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMControl::USBSendSetLineCoding
//
//		Inputs:		The line coding parameters
//
//		Outputs:	
//
//		Desc:		Set up and send SetLineCoding Management Element Request(MER) for all settings.
//
/****************************************************************************************************/

void AppleUSBCDCACMControl::USBSendSetLineCoding(UInt32 BaudRate, UInt8 StopBits, UInt8 TX_Parity, UInt8 CharLength)
{
    LineCoding		*lineParms;
    UInt16		lcLen = sizeof(LineCoding)-1;
    IOUSBDevRequest	*MER;
    IOReturn		rc;
	
    XTRACE(this, 0, 0, "USBSendSetLineCoding");
	
	// First check that Set Line Coding is supported
	
    if (!(fACMCapabilities & ACM_DeviceSuppControl))
    {
        XTRACE(this, 0, 0, "USBSendSetLineCoding - SetLineCoding not supported");
        return;
    }
    
    lineParms = (LineCoding *)IOMalloc(lcLen);
    if (!lineParms)
    {
        XTRACE(this, 0, 0, "USBSendSetLineCoding - allocate lineParms failed");
        return;
    }
    bzero(lineParms, lcLen); 
	
        // Convert BaudRate - intel format doubleword (32 bits) 
		
    OSWriteLittleInt32(lineParms, dwDTERateOffset, BaudRate);
    lineParms->bCharFormat = StopBits - 2;
    lineParms->bParityType = TX_Parity - 1;
    lineParms->bDataBits = CharLength;
    
    MER = (IOUSBDevRequest*)IOMalloc(sizeof(IOUSBDevRequest));
    if (!MER)
    {
        XTRACE(this, 0, 0, "USBSendSetLineCoding - allocate MER failed");
        return;
    }
    bzero(MER, sizeof(IOUSBDevRequest));
	
        // now build the Management Element Request
		
    MER->bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    MER->bRequest = kUSBSET_LINE_CODING;
    MER->wValue = 0;
    MER->wIndex = fCommInterfaceNumber;
    MER->wLength = lcLen;
    MER->pData = lineParms;
    
    fMERCompletionInfo.parameter = MER;
	
    rc = fControlInterface->GetDevice()->DeviceRequest(MER, &fMERCompletionInfo);
    if (rc != kIOReturnSuccess)
    {
        XTRACE(this, MER->bRequest, rc, "USBSendSetLineCoding - error issueing DeviceRequest");
        IOFree(MER->pData, lcLen);
        IOFree(MER, sizeof(IOUSBDevRequest));
    }

}/* end USBSendSetLineCoding */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMControl::USBSendSetControlLineState
//
//		Inputs:		RTS - true(set RTS), false(clear RTS)
//				DTR - true(set DTR), false(clear DTR)
//
//		Outputs:	
//
//		Desc:		Set up and send SetControlLineState Management Element Request(MER).
//
/****************************************************************************************************/

void AppleUSBCDCACMControl::USBSendSetControlLineState(bool RTS, bool DTR)
{
    IOReturn		rc;
    IOUSBDevRequest	*MER;
    UInt16		CSBitmap = 0;
	
    XTRACE(this, 0, 0, "USBSendSetControlLineState");
	
	// First check that Set Control Line State is supported
	
    if (!(fACMCapabilities & ACM_DeviceSuppControl))
    {
        XTRACE(this, 0, 0, "USBSendSetControlLineState - SetControlLineState not supported");
        return;
    }
	
    MER = (IOUSBDevRequest*)IOMalloc(sizeof(IOUSBDevRequest));
    if (!MER)
    {
        XTRACE(this, 0, 0, "USBSendSetControlLineState - allocate MER failed");
        return;
    }
    bzero(MER, sizeof(IOUSBDevRequest));
	
        // now build the Management Element Request
		
    MER->bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    MER->bRequest = kUSBSET_CONTROL_LINE_STATE;
    if (RTS)
        CSBitmap |= kRTSOn;
    if (DTR)
        CSBitmap |= kDTROn;
    MER->wValue = CSBitmap;
    MER->wIndex = fCommInterfaceNumber;
    MER->wLength = 0;
    MER->pData = NULL;
    
    fMERCompletionInfo.parameter = MER;
	
    rc = fControlInterface->GetDevice()->DeviceRequest(MER, &fMERCompletionInfo);
    if (rc != kIOReturnSuccess)
    {
        XTRACE(this, MER->bRequest, rc, "USBSendSetControlLineState - error issueing DeviceRequest");
        IOFree(MER, sizeof(IOUSBDevRequest));
    }

}/* end USBSendSetControlLineState */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMControl::USBSendBreak
//
//		Inputs:		sBreak - true(set Break), false(clear Break)
//
//		Outputs:	
//
//		Desc:		Set up and send SendBreak Management Element Request(MER).
//
/****************************************************************************************************/

void AppleUSBCDCACMControl::USBSendBreak(bool sBreak)
{
    IOReturn		rc;
    IOUSBDevRequest	*MER;
	
    XTRACE(this, 0, 0, "USBSendBreak");
	
	// First check that Send Break is supported
	
    if (!(fACMCapabilities & ACM_DeviceSuppBreak))
    {
        XTRACE(this, 0, 0, "USBSendBreak - SendBreak not supported");
        return;
    }
	
    MER = (IOUSBDevRequest*)IOMalloc(sizeof(IOUSBDevRequest));
    if (!MER)
    {
        XTRACE(this, 0, 0, "USBSendBreak - allocate MER failed");
        return;
    }
    bzero(MER, sizeof(IOUSBDevRequest));
	
        // now build the Management Element Request
		
    MER->bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    MER->bRequest = kUSBSEND_BREAK;
    if (sBreak)
    {
        MER->wValue = 0xFFFF;
    } else {
        MER->wValue = 0;
    }
    MER->wIndex = fCommInterfaceNumber;
    MER->wLength = 0;
    MER->pData = NULL;
    
    fMERCompletionInfo.parameter = MER;
	
    rc = fControlInterface->GetDevice()->DeviceRequest(MER, &fMERCompletionInfo);
    if (rc != kIOReturnSuccess)
    {
        XTRACE(this, MER->bRequest, rc, "USBSendBreak - error issueing DeviceRequest");
        IOFree(MER, sizeof(IOUSBDevRequest));
    }

}/* end USBSendBreak */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMControl::checkPipe
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

IOReturn AppleUSBCDCACMControl::checkPipe(IOUSBPipe *thePipe, bool devReq)
{
    IOReturn 	rtn = kIOReturnSuccess;
    
    XTRACE(this, 0, thePipe, "checkPipe");
    
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
//		Method:		AppleUSBCDCACMControl::allocateResources
//
//		Inputs:		
//
//		Outputs:	return code - true (allocate was successful), false (it failed)
//
//		Desc:		Finishes up the rest of the configuration and gets all the endpoints open etc.
//
/****************************************************************************************************/

bool AppleUSBCDCACMControl::allocateResources()
{
    IOUSBFindEndpointRequest	epReq;

    XTRACE(this, 0, 0, "allocateResources.");

        // Open the end point and get the buffers

    if (!fControlInterface->open(this))
    {
        XTRACE(this, 0, 0, "allocateResources - open comm interface failed.");
        fControlInterface->release();
        fControlInterface = NULL;
        return false;
    }
        // Interrupt pipe

    epReq.type = kUSBInterrupt;
    epReq.direction = kUSBIn;
    fCommPipe = fControlInterface->FindNextPipe(0, &epReq);
    if (!fCommPipe)
    {
        XTRACE(this, 0, 0, "allocateResources - no comm pipe.");
		fControlInterface->release();
        fControlInterface = NULL;
        return false;
    }
    XTRACE(this, epReq.maxPacketSize << 16 |epReq.interval, fCommPipe, "allocateResources - comm pipe.");

        // Allocate Memory Descriptor Pointer with memory for the Interrupt pipe:

    fCommPipeMDP = IOBufferMemoryDescriptor::withCapacity(COMM_BUFF_SIZE, kIODirectionIn);
    if (!fCommPipeMDP)
    {
        XTRACE(this, 0, 0, "allocateResources - Couldn't allocate MDP for interrupt pipe");
		fControlInterface->release();
        fControlInterface = NULL;
        return false;
    }

    fCommPipeBuffer = (UInt8*)fCommPipeMDP->getBytesNoCopy();
    XTRACE(this, 0, fCommPipeBuffer, "allocateResources - comm buffer");
    
    return true;
	
}/* end allocateResources */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMControl::releaseResources
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Frees up the resources allocated in allocateResources
//
/****************************************************************************************************/

void AppleUSBCDCACMControl::releaseResources()
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
//		Method:		AppleUSBCDCACMControl::checkInterfaceNumber
//
//		Inputs:		dataDriver - the data driver enquiring
//
//		Outputs:	
//
//		Desc:		Called by the data driver to ask if this is the correct
//				control interface driver. 
//
/****************************************************************************************************/

bool AppleUSBCDCACMControl::checkInterfaceNumber(AppleUSBCDCACMData *dataDriver)
{
    IOUSBInterface	*dataInterface;

    XTRACE(this, 0, dataDriver, "checkInterfaceNumber");
    
        // First check we have the same provider (Device)
    
    dataInterface = OSDynamicCast(IOUSBInterface, dataDriver->getProvider());
    if (dataInterface == NULL)
    {
        XTRACE(this, 0, 0, "checkInterfaceNumber - Error getting Data provider");
        return FALSE;
    }
    
    XTRACE(this, dataInterface->GetDevice(), fControlInterface->GetDevice(), "checkInterfaceNumber - Checking device");
    if (dataInterface->GetDevice() == fControlInterface->GetDevice())
    {
    
            // Then check to see if it's the correct data interface number
    
        if (dataDriver->fPort.DataInterfaceNumber == fDataInterfaceNumber)
        {
            this->fDataDriver = dataDriver;
            return true;
        } else {
            XTRACE(this, 0, 0, "checkInterfaceNumber - Not correct interface number");
        }
    } else {
        XTRACE(this, 0, 0, "checkInterfaceNumber - Not correct device");
    }

    return false;

}/* end checkInterfaceNumber */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMControl::resetDevice
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Check to see if we need to reset the device on wakeup. 
//
/****************************************************************************************************/

void AppleUSBCDCACMControl::resetDevice(void)
{
    IOReturn 	rtn = kIOReturnSuccess;
    USBStatus	status;
    bool	reset = false;

    XTRACE(this, 0, 0, "resetDevice");
	
	if ((fStopping) || (fControlInterface == NULL))
	{
		return;
	}
	
	IOSleep(100);										// Don't hit him too soon after the resume
    
    rtn = fControlInterface->GetDevice()->GetDeviceStatus(&status);
    if (rtn != kIOReturnSuccess)
    {
        XTRACE(this, 0, rtn, "resetDevice - Error getting device status");
        reset = true;
    } else {
        status = USBToHostWord(status);
        XTRACE(this, 0, status, "resetDevice - Device status");
        if (status & kDeviceSelfPowered)				// Self powered devices will be reset
        {
            reset = true;
        }
    }
    
    if (reset)
    {
        XTRACE(this, 0, 0, "resetDevice - Device is being reset");
        if (fCommPipe)
        {
//            fCommPipe->Abort();
        }
        fControlInterface->GetDevice()->ResetDevice();
		IOSleep(100);
    }
    
}/* end resetDevice */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMControl::message
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

IOReturn AppleUSBCDCACMControl::message(UInt32 type, IOService *provider, void *argument)
{	
	IOReturn	rtn;
    
    XTRACE(this, 0, type, "message");
	
    switch (type)
    {
        case kIOMessageServiceIsTerminated:
            XTRACE(this, 0, type, "message - kIOMessageServiceIsTerminated");
            fTerminate = true;		// We're being terminated (unplugged)
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
			if (fReadDead)
			{
				rtn = fCommPipe->Read(fCommPipeMDP, &fCommCompletionInfo, NULL);
				if (rtn != kIOReturnSuccess)
				{
					XTRACE(this, 0, rtn, "message - Read for interrupt-in pipe failed, still dead");
				} else {
					fReadDead = false;
				}
			}
            break;
        case kIOUSBMessageHubResumePort:
            XTRACE(this, 0, type, "message - kIOUSBMessageHubResumePort");
            break;
		case kIOUSBMessagePortHasBeenReset: 	
            XTRACE(this, 0, type, "message - kIOUSBMessagePortHasBeenReset");
			if (fReadDead)
			{
				rtn = fCommPipe->Read(fCommPipeMDP, &fCommCompletionInfo, NULL);
				if (rtn != kIOReturnSuccess)
				{
					XTRACE(this, 0, rtn, "message - Read for interrupt-in pipe failed, still dead");
				} else {
					fReadDead = false;
				}
			}
            break;
        default:
            XTRACE(this, 0, type, "message - unknown message"); 
            break;
    }
    
    return kIOReturnUnsupported;
    
}/* end message */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMControl::initForPM
//
//		Inputs:		provider - my provider
//
//		Outputs:	return code - true(initialized), false(failed)
//
//		Desc:		Add ourselves to the power management tree so we can do
//				the right thing on sleep/wakeup. 
//
/****************************************************************************************************/

bool AppleUSBCDCACMControl::initForPM(IOService *provider)
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
        XTRACE(this, 0, 0, "initForPM - Initializing power manager failed");
    }
    
    return false;
    
}/* end initForPM */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMControl::initialPowerStateForDomainState
//
//		Inputs:		flags - 
//
//		Outputs:	return code - Current power state
//
//		Desc:		Request for our initial power state. 
//
/****************************************************************************************************/

unsigned long AppleUSBCDCACMControl::initialPowerStateForDomainState(IOPMPowerFlags flags)
{

    XTRACE(this, 0, flags, "initialPowerStateForDomainState");
    
    return fPowerState;
    
}/* end initialPowerStateForDomainState */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMControl::setPowerState
//
//		Inputs:		powerStateOrdinal - on/off
//
//		Outputs:	return code - IOPMNoErr, IOPMAckImplied or IOPMNoSuchState
//
//		Desc:		Request to turn device on or off. 
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMControl::setPowerState(unsigned long powerStateOrdinal, IOService *whatDevice)
{

    XTRACE(this, 0, powerStateOrdinal, "setPowerState");
    
    if (powerStateOrdinal == kCDCPowerOffState || powerStateOrdinal == kCDCPowerOnState)
    {
        if (powerStateOrdinal == fPowerState)
            return IOPMAckImplied;

        fPowerState = powerStateOrdinal;
        if (fPowerState == kCDCPowerOnState)
        {
			resetDevice();
        }
    
        return IOPMNoErr;
    }
    
    return IOPMNoSuchState;
    
}/* end setPowerState */