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

    /* AppleUSBCDCWMCControlControl.cpp - MacOSX implementation of		*/
    /* USB Communication Device Class (CDC) Driver, WMC Control Interface.	*/

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

#define DEBUG_NAME "AppleUSBCDCWMCControl"

#include "AppleUSBCDCWMC.h"
#include "AppleUSBCDCWMCControl.h"

#define MIN_BAUD (50 << 1)

    // Globals

#if USE_ELG
    com_apple_iokit_XTrace	*gXTrace = 0;
#endif

AppleUSBCDCWMCData		*gDataDriver = NULL;

static IOPMPowerState gOurPowerStates[kNumCDCStates] =
{
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
};

#define super IOService

OSDefineMetaClassAndStructors(AppleUSBCDCWMCControl, IOService);

#if USE_ELG
/****************************************************************************************************/
//
//		Function:	findKernelLogger
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Just like the name says
//
/****************************************************************************************************/

IOReturn findKernelLogger()
{
    OSIterator		*iterator = NULL;
    OSDictionary	*matchingDictionary = NULL;
    IOReturn		error = 0;
	
	// Get matching dictionary
	
    matchingDictionary = IOService::serviceMatching("com_apple_iokit_XTrace");
    if (!matchingDictionary)
    {
        error = kIOReturnError;
        IOLog(DEBUG_NAME "[FindKernelLogger] Couldn't create a matching dictionary.\n");
        goto exit;
    }
	
	// Get an iterator
	
    iterator = IOService::getMatchingServices(matchingDictionary);
    if (!iterator)
    {
        error = kIOReturnError;
        IOLog(DEBUG_NAME "[FindKernelLogger] No XTrace logger found.\n");
        goto exit;
    }
	
	// User iterator to find each com_apple_iokit_XTrace instance. There should be only one, so we
	// won't iterate
	
    gXTrace = (com_apple_iokit_XTrace*)iterator->getNextObject();
    if (gXTrace)
    {
        IOLog(DEBUG_NAME "[FindKernelLogger] Found XTrace logger at %p.\n", gXTrace);
    }
	
exit:
	
    if (error != kIOReturnSuccess)
    {
        gXTrace = NULL;
        IOLog(DEBUG_NAME "[FindKernelLogger] Could not find a logger instance. Error = %X.\n", error);
    }
	
    if (matchingDictionary)
        matchingDictionary->release();
            
    if (iterator)
        iterator->release();
		
    return error;
    
}/* end findKernelLogger */
#endif

#if LOG_DATA
/****************************************************************************************************/
//
//		Function:	Asciify
//
//		Inputs:		i - the nibble
//
//		Outputs:	return byte - ascii byte
//
//		Desc:		Converts to ascii. 
//
/****************************************************************************************************/
 
static UInt8 Asciify(UInt8 i)
{

    i &= 0xF;
    if (i < 10)
        return('0' + i);
    else return(55  + i);
	
}/* end Asciify */

#define dumplen		32		// Set this to the number of bytes to dump and the rest should work out correct

#define buflen		((dumplen*2)+dumplen)+3
#define Asciistart	(dumplen*2)+3

/****************************************************************************************************/
//
//		Function:	USBLogData
//
//		Inputs:		Dir - direction
//				Count - number of bytes
//				buf - the data
//
//		Outputs:	
//
//		Desc:		Puts the data in the log. 
//
/****************************************************************************************************/

void USBLogData(UInt8 Dir, UInt32 Count, char *buf)
{    
    SInt32	wlen;
#if USE_ELG
    UInt8 	*b;
    UInt8 	w[8];
#else
    UInt32	llen, rlen;
    UInt16	i, Aspnt, Hxpnt;
    UInt8	wchr;
    char	LocBuf[buflen+1];
#endif
    
    switch (Dir)
    {
        case kDataIn:
#if USE_ELG
            XTRACE2(this, buf, Count, "USBLogData - Read Complete, address, size");
#else
            IOLog( "AppleUSBCDCWMCControl: USBLogData - Read Complete, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count );
#endif
            break;
        case kDataOut:
#if USE_ELG
            XTRACE2(this, buf, Count, "USBLogData - Write, address, size");
#else
            IOLog( "AppleUSBCDCWMCControl: USBLogData - Write, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count );
#endif
            break;
        case kDataOther:
#if USE_ELG
            XTRACE2(this, buf, Count, "USBLogData - Other, address, size");
#else
            IOLog( "AppleUSBCDCWMCControl: USBLogData - Other, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count );
#endif
            break;
    }

#if DUMPALL
    wlen = Count;
#else
    if (Count > dumplen)
    {
        wlen = dumplen;
    } else {
        wlen = Count;
    }
#endif

    if (wlen == 0)
    {
#if USE_ELG
        XTRACE2(this, 0, Count, "USBLogData - No data, Count=0");
#else
        IOLog( "AppleUSBCDCWMCControl: USBLogData - No data, Count=0\n" );
#endif
        return;
    }

#if (USE_ELG)
    b = (UInt8 *)buf;
    while (wlen > 0)							// loop over the buffer
    {
        bzero(w, sizeof(w));						// zero it
        bcopy(b, w, min(wlen, 8));					// copy bytes over
    
        switch (Dir)
        {
            case kDataIn:
                XTRACE2(this, (w[0] << 24 | w[1] << 16 | w[2] << 8 | w[3]), (w[4] << 24 | w[5] << 16 | w[6] << 8 | w[7]), "USBLogData - Rx buffer dump");
                break;
            case kDataOut:
                XTRACE2(this, (w[0] << 24 | w[1] << 16 | w[2] << 8 | w[3]), (w[4] << 24 | w[5] << 16 | w[6] << 8 | w[7]), "USBLogData - Tx buffer dump");
                break;
            case kDataOther:
                XTRACE2(this, (w[0] << 24 | w[1] << 16 | w[2] << 8 | w[3]), (w[4] << 24 | w[5] << 16 | w[6] << 8 | w[7]), "USBLogData - Misc buffer dump");
                break;
        }
        wlen -= 8;							// adjust by 8 bytes for next time (if have more)
        b += 8;
    }
#else
    rlen = 0;
    do
    {
        for (i=0; i<=buflen; i++)
        {
            LocBuf[i] = 0x20;
        }
        LocBuf[i] = 0x00;
        
        if (wlen > dumplen)
        {
            llen = dumplen;
            wlen -= dumplen;
        } else {
            llen = wlen;
            wlen = 0;
        }
        Aspnt = Asciistart;
        Hxpnt = 0;
        for (i=1; i<=llen; i++)
        {
            wchr = buf[i-1];
            LocBuf[Hxpnt++] = Asciify(wchr >> 4);
            LocBuf[Hxpnt++] = Asciify(wchr);
            if ((wchr < 0x20) || (wchr > 0x7F)) 		// Non printable characters
            {
                LocBuf[Aspnt++] = 0x2E;				// Replace with a period
            } else {
                LocBuf[Aspnt++] = wchr;
            }
        }
        LocBuf[(llen + Asciistart) + 1] = 0x00;
        IOLog(LocBuf);
        IOLog("\n");
        IOSleep(Sleep_Time);					// Try and keep the log from overflowing
       
        rlen += llen;
        buf = &buf[rlen];
    } while (wlen != 0);
#endif 

}/* end USBLogData */
#endif

	// Encode the 4 modem status bits (so we only make one call to changeState below)

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
//		Method:		AppleUSBCDCWMCControl::commReadComplete
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

void AppleUSBCDCWMCControl::commReadComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    AppleUSBCDCWMCControl	*me = (AppleUSBCDCWMCControl*)obj;
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
            
//        LogData(kDataOther, dLen, port->CommPipeBuffer);
		
        if ((dLen > 7) && (me->fCommPipeBuffer[1] == kUSBSERIAL_STATE))
        {
            tState = (UInt16 *)&me->fCommPipeBuffer[8];
            tempS = USBToHostWord(*tState);
            XTRACE(me, 0, tempS, "commReadComplete - kUSBSERIAL_STATE");
			
            mask = sMapModemStates[15];				// All 4 on
            value = sMapModemStates[tempS & 15];		// now the status bits
            if (gDataDriver)
            {
                gDataDriver->setState(value, mask, NULL);
            }
        }
    } else {
        XTRACE(me, 0, rc, "commReadComplete - error");
        if (rc != kIOReturnAborted)
        {
            rc = me->checkPipe(me->fCommPipe, false);
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
        }
    }
	
}/* end commReadComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWMCControl::merWriteComplete
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

void AppleUSBCDCWMCControl::merWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    AppleUSBCDCWMCControl	*me = (AppleUSBCDCWMCControl *)obj;
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
	
}/* end merWriteComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWMCControl::start
//
//		Inputs:		provider - my provider
//
//		Outputs:	Return code - true (it's me), false (sorry it probably was me, but I can't configure it)
//
//		Desc:		This is called once it has beed determined I'm probably the best 
//				driver for this interface.
//
/****************************************************************************************************/

bool AppleUSBCDCWMCControl::start(IOService *provider)
{

    fTerminate = false;
    fStopping = false;
    fdataAcquired = false;
    fCommPipeMDP = NULL;
    fCommPipe = NULL;
    fCommPipeBuffer = NULL;
    
#if USE_ELG
    XTraceLogInfo	*logInfo;
    
    findKernelLogger();
    if (gXTrace)
    {
        gXTrace->retain();		// don't let it unload ...
        XTRACE(this, 0, 0xbeefbeef, "Hello from start");
        logInfo = gXTrace->LogGetInfo();
        IOLog("AppleUSBCDCWMCControl: start - Log is at %x\n", (unsigned int)logInfo);
    } else {
        return false;
    }
#endif

    XTRACE(this, 0, provider, "start - provider.");
    
    return false;
    
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

    if (!configureDevice())
    {
        ALERT(0, 0, "start - configureDevice failed");
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
//		Method:		AppleUSBCDCWMCControl::stop
//
//		Inputs:		provider - my provider
//
//		Outputs:	None
//
//		Desc:		Stops the driver
//
/****************************************************************************************************/

void AppleUSBCDCWMCControl::stop(IOService *provider)
{

    XTRACE(this, 0, 0, "stop");
    
    fStopping = true;
    
    releaseResources();
	
	PMstop();
                    
    super::stop(provider);
    
}/* end stop */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWMCControl::configureWHCM
//
//		Inputs:		None
//
//		Outputs:	return Code - true (configured), false (not configured)
//
//		Desc:		Configures the Wireless handset Control Model interface
//
/****************************************************************************************************/

bool AppleUSBCDCWMCControl::configureWHCM()
{

    return false;

}/* end configureWHCM */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWMCControl::configureDMM
//
//		Inputs:		None
//
//		Outputs:	return Code - true (configured), false (not configured)
//
//		Desc:		Configures the Device Management Model interface
//
/****************************************************************************************************/

bool AppleUSBCDCWMCControl::configureDMM()
{

    return false;

}/* end configureDMM */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWMCControl::configureMDLM
//
//		Inputs:		None
//
//		Outputs:	return Code - true (configured), false (not configured)
//
//		Desc:		Configures the Mobile Direct Line Model interface
//
/****************************************************************************************************/

bool AppleUSBCDCWMCControl::configureMDLM()
{

    return false;

}/* end configureMDLM */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWMCControl::configureDevice
//
//		Inputs:		None
//
//		Outputs:	return Code - true (device configured), false (device not configured)
//
//		Desc:		Finds the configurations and then the appropriate interfaces etc.
//
/****************************************************************************************************/

bool AppleUSBCDCWMCControl::configureDevice()
{
    bool	configOK = false;

    XTRACE(this, 0, 0, "configureDevice");

    fCommInterfaceNumber = fControlInterface->GetInterfaceNumber();
    fCommSubClass = fControlInterface->GetInterfaceSubClass();
    XTRACE(this, fCommSubClass, fCommInterfaceNumber, "configureDevice - Subclass and interface number.");
    
    switch (fCommSubClass)
    {
        case kUSBWirelessHandsetControlModel:
            if (configureWHCM())
            {
                configOK = true;
            }
            break;
        case kUSBDeviceManagementModel:
            if (configureDMM())
            {
                configOK = true;
            }
            break;
        case kUSBMobileDirectLineModel:
            if (configureMDLM())
            {
                configOK = true;
            }
            break;
        default:
            XTRACE(this, 0, fCommSubClass, "configureDevice - Unsupported subclass");
            break;
        }

    if (!configOK)
    {
        XTRACE(this, 0, 0, "configureDevice - configuration failed");
        releaseResources();
        return false;
    }
    
    return true;
    
}/* end configureDevice */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWMCControl::getFunctionalDescriptors
//
//		Inputs:		
//
//		Outputs:	return - true (descriptors ok), false (somethings not right or not supported)	
//
//		Desc:		Finds all the functional descriptors for the specific interface
//
/****************************************************************************************************/

bool AppleUSBCDCWMCControl::getFunctionalDescriptors()
{
    bool				gotDescriptors = false;
    UInt16				vers;
    UInt16				*hdrVers;
    const FunctionalDescriptorHeader 	*funcDesc = NULL;
    HDRFunctionalDescriptor		*HDRFDesc;		// hearder functional descriptor
    UnionFunctionalDescriptor		*UNNFDesc;		// union functional descriptor
       
    XTRACE(this, 0, 0, "getFunctionalDescriptors");

    fDataInterfaceNumber = 0xff;
    
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
                case Union_FunctionalDescriptor:
                    (const FunctionalDescriptorHeader*)UNNFDesc = funcDesc;
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - Union Functional Descriptor");
                    if (UNNFDesc->bFunctionLength > sizeof(FunctionalDescriptorHeader))
                    {
                        if (fCommInterfaceNumber != UNNFDesc->bMasterInterface)
                        {
                            XTRACE(this, fCommInterfaceNumber, UNNFDesc->bMasterInterface, "getFunctionalDescriptors - Master interface incorrect");
                        }
                        if (fDataInterfaceNumber == 0xff)
                        {
                            fDataInterfaceNumber = UNNFDesc->bSlaveInterface[0];	// Use the first slave (may get overwritten by other descriptors)
                        }
                        if (fDataInterfaceNumber != UNNFDesc->bSlaveInterface[0])
                        {
                            XTRACE(this, fDataInterfaceNumber, UNNFDesc->bSlaveInterface[1], "getFunctionalDescriptors - Slave interface incorrect");
                        }
                    } else {
                        XTRACE(this, UNNFDesc->bFunctionLength, 0, "getFunctionalDescriptors - Union descriptor length error");
                    }
                    break;
                case WCM_FunctionalDescriptor:
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - WMC Functional Descriptor");
                    break;
                default:
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - unknown Functional Descriptor");
                    break;
            }
        }
    } while(!gotDescriptors);
    
        // If we get this far and actually have a data interface number we're good to go

    if (fDataInterfaceNumber == 0xff)
    {
        XTRACE(this, 0, 0, "getFunctionalDescriptors - No data interface specified");
        return false;
    } else {
        XTRACE(this, 0, fDataInterfaceNumber, "getFunctionalDescriptors - Data interface number");
    }
    
    return true;

}/* end getFunctionalDescriptors */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWMCControl::dataAcquired
//
//		Inputs:		None
//
//		Outputs:	return Code - true (it worked), false (it didn't)
//
//		Desc:		Tells this driver the data driver's port has been acquired
//
/****************************************************************************************************/

bool AppleUSBCDCWMCControl::dataAcquired()
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
//		Method:		AppleUSBCDCWMCControl::dataReleased
//
//		Inputs:		None
//
//		Outputs:	None
//
//		Desc:		Tells this driver the data driver's port has been released
//
/****************************************************************************************************/

void AppleUSBCDCWMCControl::dataReleased()
{
    
    XTRACE(this, 0, 0, "dataReleased");
    
    fCommPipe->Abort();
    fdataAcquired = false;
    
}/* end dataReleased */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWMCControl::checkPipe
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

IOReturn AppleUSBCDCWMCControl::checkPipe(IOUSBPipe *thePipe, bool devReq)
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
//		Method:		AppleUSBCDCWMCControl::allocateResources
//
//		Inputs:		
//
//		Outputs:	return code - true (allocate was successful), false (it failed)
//
//		Desc:		Finishes up the rest of the configuration and gets all the endpoints open etc.
//
/****************************************************************************************************/

bool AppleUSBCDCWMCControl::allocateResources()
{
    IOUSBFindEndpointRequest	epReq;

    XTRACE(this, 0, 0, "allocateResources.");

        // Open the end point and get the buffers

    if (!fControlInterface->open(this))
    {
        XTRACE(this, 0, 0, "allocateResources - open comm interface failed.");
        return false;
    }
        // Interrupt pipe

    epReq.type = kUSBInterrupt;
    epReq.direction = kUSBIn;
    fCommPipe = fControlInterface->FindNextPipe(0, &epReq);
    if (!fCommPipe)
    {
        XTRACE(this, 0, 0, "allocateResources - no comm pipe.");
        return false;
    }
    XTRACE(this, epReq.maxPacketSize << 16 |epReq.interval, fCommPipe, "allocateResources - comm pipe.");

        // Allocate Memory Descriptor Pointer with memory for the Interrupt pipe:

    fCommPipeMDP = IOBufferMemoryDescriptor::withCapacity(COMM_BUFF_SIZE, kIODirectionIn);
    if (!fCommPipeMDP)
    {
        XTRACE(this, 0, 0, "allocateResources - Couldn't allocate MDP for interrupt pipe");
        return false;
    }

    fCommPipeBuffer = (UInt8*)fCommPipeMDP->getBytesNoCopy();
    XTRACE(this, 0, fCommPipeBuffer, "allocateResources - comm buffer");
    
    return true;
	
}/* end allocateResources */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWMCControl::releaseResources
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Frees up the resources allocated in allocateResources
//
/****************************************************************************************************/

void AppleUSBCDCWMCControl::releaseResources()
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
//		Method:		AppleUSBCDCWMCControl::checkInterfaceNumber
//
//		Inputs:		dataDriver - the control driver enquiring
//
//		Outputs:	
//
//		Desc:		Called by the data driver to ask if this is the correct
//				control interface driver. 
//
/****************************************************************************************************/

bool AppleUSBCDCWMCControl::checkInterfaceNumber(AppleUSBCDCWMCData *dataDriver)
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
            gDataDriver = dataDriver;
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
//		Method:		AppleUSBCDCWMCControl::resetDevice
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Check to see if we need to reset the device on wakeup. 
//
/****************************************************************************************************/

void AppleUSBCDCWMCControl::resetDevice(void)
{
    IOReturn 	rtn = kIOReturnSuccess;
    USBStatus	status;
    bool	reset = true;

    XTRACE(this, 0, 0, "resetDevice");
	
	if ((fStopping) || (fControlInterface == NULL))
	{
		return;
	}
    
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
        fControlInterface->GetDevice()->ResetDevice();
    }
    
}/* end resetDevice */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWMCControl::message
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

IOReturn AppleUSBCDCWMCControl::message(UInt32 type, IOService *provider, void *argument)
{	
    
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
            break;
        case kIOUSBMessageHubResumePort:
            XTRACE(this, 0, type, "message - kIOUSBMessageHubResumePort");
            break;
        default:
            XTRACE(this, 0, type, "message - unknown message"); 
            break;
    }
    
    return kIOReturnUnsupported;
    
}/* end message */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWMCControl::initForPM
//
//		Inputs:		provider - my provider
//
//		Outputs:	return code - true(initialized), false(failed)
//
//		Desc:		Add ourselves to the power management tree so we can do
//				the right thing on sleep/wakeup. 
//
/****************************************************************************************************/

bool AppleUSBCDCWMCControl::initForPM(IOService *provider)
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
//		Method:		AppleUSBCDCWMCControl::initialPowerStateForDomainState
//
//		Inputs:		flags - 
//
//		Outputs:	return code - Current power state
//
//		Desc:		Request for our initial power state. 
//
/****************************************************************************************************/

unsigned long AppleUSBCDCWMCControl::initialPowerStateForDomainState(IOPMPowerFlags flags)
{

    XTRACE(this, 0, flags, "initialPowerStateForDomainState");
    
    return fPowerState;
    
}/* end initialPowerStateForDomainState */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWMCControl::setPowerState
//
//		Inputs:		powerStateOrdinal - on/off
//
//		Outputs:	return code - IOPMNoErr, IOPMAckImplied or IOPMNoSuchState
//
//		Desc:		Request to turn device on or off. 
//
/****************************************************************************************************/

IOReturn AppleUSBCDCWMCControl::setPowerState(unsigned long powerStateOrdinal, IOService *whatDevice)
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