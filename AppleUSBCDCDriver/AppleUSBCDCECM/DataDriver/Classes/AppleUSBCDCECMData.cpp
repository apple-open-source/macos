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

#define DEBUG_NAME "AppleUSBCDCECMData"
 
#include "AppleUSBCDCECM.h"
#include "AppleUSBCDCECMData.h"

#define MIN_BAUD (50 << 1)

static IOPMPowerState gOurPowerStates[kNumCDCStates] =
{
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
};
    
static struct MediumTable
{
    UInt32	type;
    UInt32	speed;
}

mediumTable[] =
{
    {kIOMediumEthernetNone,												0},
    {kIOMediumEthernetAuto,												0},
    {kIOMediumEthernet10BaseT 	 | kIOMediumOptionHalfDuplex,								10},
    {kIOMediumEthernet10BaseT 	 | kIOMediumOptionFullDuplex,								10},
    {kIOMediumEthernet100BaseTX  | kIOMediumOptionHalfDuplex,								100},
    {kIOMediumEthernet100BaseTX  | kIOMediumOptionFullDuplex,								100}
};

#define super IOEthernetController

OSDefineMetaClassAndStructors(AppleUSBCDCECMData, IOEthernetController);

/****************************************************************************************************/
//
//		Function:	findCDCDriverED
//
//		Inputs:		myDevice - Address of the controlling device
//				dataAddr - my address
//				dataInterfaceNum - the data interface number
//
//		Outputs:	
//
//		Desc:		Finds the initiating CDC driver and confirm the interface number
//
/****************************************************************************************************/

IOReturn findCDCDriverED(IOUSBDevice *myDevice, void *dataAddr, UInt8 dataInterfaceNum)
{
    AppleUSBCDCECMData	*me = (AppleUSBCDCECMData *)dataAddr;
    AppleUSBCDC		*CDCDriver = NULL;
    bool		driverOK = false;
    OSIterator		*iterator = NULL;
    OSDictionary	*matchingDictionary = NULL;
    UInt16		i;
    
    XTRACE(me, 0, 0, "findCDCDriverED");
        
        // Get matching dictionary
       	
    matchingDictionary = IOService::serviceMatching("AppleUSBCDC");
    if (!matchingDictionary)
    {
        XTRACE(me, 0, 0, "findCDCDriverED - Couldn't create a matching dictionary");
        return kIOReturnError;
    }
    
	// Get an iterator
	
    iterator = IOService::getMatchingServices(matchingDictionary);
    if (!iterator)
    {
        XTRACE(me, 0, 0, "findCDCDriverED - No AppleUSBCDC driver found!");
        matchingDictionary->release();
        return kIOReturnError;
    }

#if 0    
	// Use iterator to find driver (there's only one so we won't bother to iterate)
                
    CDCDriver = (AppleUSBCDC *)iterator->getNextObject();
    if (CDCDriver)
    {
        driverOK = CDCDriver->confirmDriver(kUSBEthernetControlModel, dataInterfaceNum);
    }
#endif

 	// Iterate until we find our matching CDC driver
                
    CDCDriver = (AppleUSBCDC *)iterator->getNextObject();
    while (CDCDriver)
    {
        XTRACE(me, 0, CDCDriver, "findCDCDriverED - CDC driver candidate");
        
        if (me->fDataInterface->GetDevice() == CDCDriver->getCDCDevice())
        {
            XTRACE(me, 0, CDCDriver, "findCDCDriverED - Found our CDC driver");
            driverOK = CDCDriver->confirmDriver(kUSBEthernetControlModel, dataInterfaceNum);
            break;
        }
        CDCDriver = (AppleUSBCDC *)iterator->getNextObject();
    }

    matchingDictionary->release();
    iterator->release();
    
    if (!CDCDriver)
    {
        XTRACE(me, 0, 0, "findCDCDriverED - CDC driver not found");
        return kIOReturnError;
    }
   
    if (!driverOK)
    {
        XTRACE(me, kUSBEthernetControlModel, dataInterfaceNum, "findCDCDriverED - Not my interface");
        return kIOReturnError;
    }
    
    me->fConfigAttributes = CDCDriver->fbmAttributes;
    
    for (i=0; i<6; i++)
    {
        me->fEthernetaddr[i] = CDCDriver->fCacheEaddr[i];
    }
    
    return kIOReturnSuccess;
    
}/* end findCDCDriverED */

/****************************************************************************************************/
//
//		Function:	findControlDriverED
//
//		Inputs:		me - my address
//
//		Outputs:	
//
//		Desc:		Finds our matching control driver
//
/****************************************************************************************************/

IOReturn findControlDriverED(AppleUSBCDCECMData *me)
{
    AppleUSBCDCECMControl	*tempDriver = NULL;
    OSIterator			*iterator = NULL;
    OSDictionary		*matchingDictionary = NULL;
    
    XTRACE(me, 0, 0, "findControlDriverED");
	
	me->fControlDriver = NULL;
    
        // Get matching dictionary
       	
    matchingDictionary = IOService::serviceMatching("AppleUSBCDCECMControl");
    if (!matchingDictionary)
    {
        XTRACE(me, 0, 0, "findControlDriverED - Couldn't create a matching dictionary");
        return kIOReturnError;
    }
    
	// Get an iterator
	
    iterator = IOService::getMatchingServices(matchingDictionary);
    if (!iterator)
    {
        XTRACE(me, 0, 0, "findControlDriverED - No AppleUSBCDCECMControl drivers found (iterator)");
        matchingDictionary->release();
        return kIOReturnError;
    }
    
	// Iterate until we find our matching driver
                
    tempDriver = (AppleUSBCDCECMControl *)iterator->getNextObject();
    while (tempDriver)
    {
        XTRACE(me, 0, tempDriver, "findControlDriverED - Control driver candidate");
        if (tempDriver->checkInterfaceNumber((AppleUSBCDCECMData *)me))
        {
            XTRACE(me, 0, tempDriver, "findControlDriverED - Found our control driver");
            me->fControlDriver = tempDriver;
            break;
        }
        tempDriver = (AppleUSBCDCECMControl *)iterator->getNextObject();
    }

    matchingDictionary->release();
    iterator->release();
   
    if (!me->fControlDriver)
    {
        XTRACE(me, 0, 0, "findControlDriverED - Failed");
        return kIOReturnError;
    }

    return kIOReturnSuccess;
    
}/* end findControlDriverED */

#if LOG_DATA
#define dumplen		32		// Set this to the number of bytes to dump and the rest should work out correct

#define buflen		((dumplen*2)+dumplen)+3
#define Asciistart	(dumplen*2)+3

/****************************************************************************************************/
//
//		Function:	AppleUSBCDCECMData::USBLogData
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

void AppleUSBCDCECMData::USBLogData(UInt8 Dir, SInt32 Count, char *buf)
{    
    SInt32	wlen;
    UInt8	tDir = Dir;
    SInt32	llen, rlen;
    SInt16	i, Aspnt, Hxpnt;
    UInt8	wchr;
    char	LocBuf[buflen+1];
    
    switch (tDir)
    {
        case kDataIn:
            Log("AppleUSBCDCECMData: USBLogData - Read Complete, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count);
            break;
        case kDataOut:
            Log("AppleUSBCDCECMData: USBLogData - Write, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count);
            break;
        case kDataOther:
            Log("AppleUSBCDCECMData: USBLogData - Other, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count);
            break;
        case kDataNone:
            tDir = kDataOther;
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
        Log("AppleUSBCDCECMData: USBLogData - No data, Count=0\n");
        return;
    }

    rlen = 0;
    do
    {
        memset(LocBuf, 0x20, buflen);
        
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
		
		Log("%s\n", LocBuf);
#if USE_IOL
        IOSleep(Sleep_Time);					// Try and keep the log from overflowing
#endif       
        rlen += llen;
        buf = &buf[rlen];
    } while (wlen != 0);

}/* end USBLogData */

/****************************************************************************************************/
//
//		Function:	AppleUSBCDCECMData::dumpData
//
//		Inputs:		buf - the data
//				size - number of bytes
//
//		Outputs:	None
//
//		Desc:		Creats formatted data for the log (cannot be used at interrupt time) 
//
/****************************************************************************************************/

void AppleUSBCDCECMData::dumpData(char *buf, SInt32 size)
{
    SInt32	curr, len, dlen;

    Log("AppleUSBCDCECMData: dumpData - Address = %8x, size = %8d\n", (UInt)buf, (UInt)size);

    dlen = 0;
    len = size;
    
    for (curr=0; curr<size; curr+=dumplen)
    {
        if (len > dumplen)
        {
            dlen = dumplen;
        } else {
            dlen = len;
        }
        Log("%8x ", (UInt)&buf[curr]);
        USBLogData(kDataNone, dlen, &buf[curr]);
        len -= dlen;
    }
   
}/* end dumpData */
#endif

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::dataReadComplete
//
//		Inputs:		obj - me
//				param - pool index
//				rc - return code
//				remaining - what's left
//
//		Outputs:	
//
//		Desc:		BulkIn pipe (Data interface) read completion routine
//
/****************************************************************************************************/

void AppleUSBCDCECMData::dataReadComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    AppleUSBCDCECMData	*me = (AppleUSBCDCECMData*)obj;
    IOReturn		ior;
    UInt32		poolIndx = (UInt32)param;
    
    XTRACE(me, 0, poolIndx, "dataReadComplete");

    if (rc == kIOReturnSuccess)	// If operation returned ok
    {	
        XTRACE(me, 0, me->fControlDriver->fMax_Block_Size - remaining, "dataReadComplete - data length");
		
        meLogData(kDataIn, (me->fControlDriver->fMax_Block_Size - remaining), me->fPipeInBuff[poolIndx].pipeInBuffer);
	
            // Move the incoming bytes up the stack

        me->receivePacket(me->fPipeInBuff[poolIndx].pipeInBuffer, me->fControlDriver->fMax_Block_Size - remaining);
	
    } else {
        XTRACE(me, 0, rc, "dataReadComplete - Read completion io err");
        if (rc != kIOReturnAborted)
        {
			me->fDeferredClear = true;
#if 0
            rc = me->clearPipeStall(me->fInPipe);
            if (rc != kIOReturnSuccess)
            {
                XTRACE(me, 0, rc, "dataReadComplete - clear stall failed (trying to continue)");
            }
#endif
        }
    }
    
        // Queue the next read, only if not aborted
	
    if (rc != kIOReturnAborted)
    {
        ior = me->fInPipe->Read(me->fPipeInBuff[poolIndx].pipeInMDP, &me->fPipeInBuff[poolIndx].readCompletionInfo, NULL);
        if (ior != kIOReturnSuccess)
        {
            XTRACE(me, 0, ior, "dataReadComplete - Failed to queue read");
            me->fPipeInBuff[poolIndx].dead = true;
        }
    } else {
        XTRACE(me, poolIndx, 0, "dataReadComplete - Read terminated");
        me->fPipeInBuff[poolIndx].dead = true;
    }

    return;
	
}/* end dataReadComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::dataWriteComplete
//
//		Inputs:		obj - me
//				param - pool index
//				rc - return code
//				remaining - what's left
//
//		Outputs:	
//
//		Desc:		BulkOut pipe (Data interface) write completion routine
//
/****************************************************************************************************/

void AppleUSBCDCECMData::dataWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    AppleUSBCDCECMData	*me = (AppleUSBCDCECMData *)obj;
    mbuf_t		m;
    UInt32		pktLen = 0;
    UInt32		numbufs = 0;
    UInt32		poolIndx;

    poolIndx = (UInt32)param;
    if (me->fBufferPoolLock)
    {
        IOLockLock(me->fBufferPoolLock);
    }
    
    if (rc == kIOReturnSuccess)						// If operation returned ok
    {	
        XTRACE(me, rc, poolIndx, "dataWriteComplete");
        
        if (me->fPipeOutBuff[poolIndx].m != NULL)			// Null means zero length write
        {
            m = me->fPipeOutBuff[poolIndx].m;
			while (m)
			{
				pktLen += mbuf_len(m);
				numbufs++;
				m = mbuf_next(m);
			}
                        
            me->freePacket(me->fPipeOutBuff[poolIndx].m);		// Free the mbuf
            me->fPipeOutBuff[poolIndx].m = NULL;
        
            if ((pktLen % me->fOutPacketSize) == 0)			// If it was a multiple of max packet size then we need to do a zero length write
            {
                XTRACE(me, rc, pktLen, "dataWriteComplete - writing zero length packet");
//                me->fPipeOutBuff[poolIndx].pipeOutMDP->setLength(0);
                me->fPipeOutBuff[poolIndx].writeCompletionInfo.parameter = (void *)poolIndx;
                me->fOutPipe->Write(me->fPipeOutBuff[poolIndx].pipeOutMDP, 2000, 5000, 0, &me->fPipeOutBuff[poolIndx].writeCompletionInfo);
            } else {
                me->fPipeOutBuff[poolIndx].avail = true;
            }
        } else {
            me->fPipeOutBuff[poolIndx].avail = true;			// Make the buffer available again
        }
    } else {
        XTRACE(me, rc, poolIndx, "dataWriteComplete - IO err");

        if (me->fPipeOutBuff[poolIndx].m != NULL)
        {
            me->freePacket(me->fPipeOutBuff[poolIndx].m);		// Free the mbuf anyway
            me->fPipeOutBuff[poolIndx].m = NULL;
            me->fPipeOutBuff[poolIndx].avail = true;
        }
        if ((rc != kIOReturnAborted) || (rc != kIOUSBTransactionReturned))
        {
			me->fDeferredClear = true;
#if 0
			rc = me->clearPipeStall(me->fOutPipe);
            if (rc != kIOReturnSuccess)
            {
                XTRACE(me, 0, rc, "dataWriteComplete - clear stall failed (trying to continue)");
            }
#endif
        }
    }
    
    if (me->fBufferPoolLock)
    {
        IOLockUnlock(me->fBufferPoolLock);
    }
        
    return;
	
}/* end dataWriteComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::init
//
//		Inputs:		properties - data (keys and values) used to match
//
//		Outputs:	Return code - true (init successful), false (init failed)
//
//		Desc:		Initialize the driver.
//
/****************************************************************************************************/

bool AppleUSBCDCECMData::init(OSDictionary *properties)
{
    UInt32	i;

    XTRACE(this, 0, 0, "init");
    
    if (super::init(properties) == false)
    {
        XTRACE(this, 0, 0, "init - initialize super failed");
        return false;
    }
	
	fResetState = kResetNormal;
	fSleeping = false;
	fDeferredClear = false;
	
    for (i=0; i<kMaxOutBufPool; i++)
    {
        fPipeOutBuff[i].pipeOutMDP = NULL;
        fPipeOutBuff[i].pipeOutBuffer = NULL;
        fPipeOutBuff[i].m = NULL;
        fPipeOutBuff[i].avail = false;
        fPipeOutBuff[i].writeCompletionInfo.target = NULL;
        fPipeOutBuff[i].writeCompletionInfo.action = NULL;
        fPipeOutBuff[i].writeCompletionInfo.parameter = NULL;
    }
    fOutPoolIndex = 0;
    
    for (i=0; i<kMaxInBufPool; i++)
    {
        fPipeInBuff[i].pipeInMDP = NULL;
        fPipeInBuff[i].pipeInBuffer = NULL;
        fPipeInBuff[i].dead = false;
        fPipeInBuff[i].readCompletionInfo.target = NULL;
        fPipeInBuff[i].readCompletionInfo.action = NULL;
        fPipeInBuff[i].readCompletionInfo.parameter = NULL;
    }

    return true;

}/* end init*/

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::probe
//
//		Inputs:		provider - my provider
//
//		Outputs:	IOService - from super::probe, score - probe score
//
//		Desc:		Modify the probe score if necessary (we don't at the moment)
//
/****************************************************************************************************/

IOService* AppleUSBCDCECMData::probe(IOService *provider, SInt32 *score)
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
//		Method:		AppleUSBCDCECMData::start
//
//		Inputs:		provider - my provider
//
//		Outputs:	Return code - true (it's me), false (sorry it probably was me, but I can't configure it)
//
//		Desc:		This is called once it has beed determined I'm probably the best 
//				driver for this device.
//
/****************************************************************************************************/

bool AppleUSBCDCECMData::start(IOService *provider)
{
    OSNumber		*bufNumber = NULL;
    UInt16		bufValue = 0;

    XTRACE(this, 0, provider, "start");
    
		// Get my USB provider - the interface

    fDataInterface = OSDynamicCast(IOUSBInterface, provider);
    if(!fDataInterface)
    {
        ALERT(0, 0, "start - provider invalid");
        return false;
    }
    
    fDataInterfaceNumber = fDataInterface->GetInterfaceNumber();
    
    if (findCDCDriverED(fDataInterface->GetDevice(), this, fDataInterfaceNumber) != kIOReturnSuccess)
    {
        XTRACE(this, 0, 0, "start - Find CDC driver failed");
        return false;
    }
	
		// Now we can start for real
	
	if(!super::start(provider))
    {
        ALERT(0, 0, "start - start super failed");
        return false;
    }
    
    fBufferPoolLock = IOLockAlloc();
    if (!fBufferPoolLock)
    {
        ALERT(0, 0, "start - Buffer pool lock allocate failed");
        return false;
    }
    
        // get workloop
        
    fWorkLoop = getWorkLoop();
    if (!fWorkLoop)
    {
        ALERT(0, 0, "start - getWorkLoop failed");
        return false;
    }
    
    if (!configureData())
    {
        ALERT(0, 0, "start - configureData failed");
        return false;
    }
    
		// Check for an input buffer pool override first
	
	fInBufPool = 0;
	fOutBufPool = 0;
		
	bufNumber = (OSNumber *)provider->getProperty(inputTag);
    if (bufNumber)
    {
		bufValue = bufNumber->unsigned16BitValue();
		XTRACE(this, 0, bufValue, "start - Number of input buffers override value");
        if (bufValue <= kMaxInBufPool)
        {
            fInBufPool = bufValue;
        } else {
            fInBufPool = kMaxInBufPool;
        }
	} else {
		fInBufPool = 0;
	}
    
		// Now set up the real input buffer pool values (only if not overridden)
    
	if (fInBufPool == 0)
	{
		bufNumber = NULL;
		bufNumber = (OSNumber *)getProperty(inputTag);
		if (bufNumber)
		{
			bufValue = bufNumber->unsigned16BitValue();
			XTRACE(this, 0, bufValue, "start - Number of input buffers requested");
			if (bufValue <= kMaxInBufPool)
			{
				fInBufPool = bufValue;
			} else {
				fInBufPool = kMaxInBufPool;
			}
		} else {
			fInBufPool = kInBufPool;
		}
    }
	
		// Check for an output buffer pool override
		
	bufNumber = NULL;
	bufNumber = (OSNumber *)provider->getProperty(outputTag);
    if (bufNumber)
    {
		bufValue = bufNumber->unsigned16BitValue();
		XTRACE(this, 0, bufValue, "start - Number of output buffers override value");
        if (bufValue <= kMaxOutBufPool)
        {
            fOutBufPool = bufValue;
        } else {
            fOutBufPool = kMaxOutBufPool;
        }
	} else {
		fOutBufPool = 0;
	}
    
        // Now set up the real output buffer pool values (only if not overridden)
    
	if (fOutBufPool == 0)
	{
		bufNumber = NULL;
		bufNumber = (OSNumber *)getProperty(outputTag);
		if (bufNumber)
		{
			bufValue = bufNumber->unsigned16BitValue();
			XTRACE(this, 0, bufValue, "start - Number of output buffers requested");
			if (bufValue <= kMaxOutBufPool)
			{
				fOutBufPool = bufValue;
			} else {
				fOutBufPool = kMaxOutBufPool;
			}
		} else {
			fOutBufPool = kOutBufPool;
		}
	}
    
    XTRACE(this, fInBufPool, fOutBufPool, "start - Buffer pools (input, output)");
    
    if (findControlDriverED(this) != kIOReturnSuccess)
    {
        ALERT(0, 0, "start - Find control driver failed");
    }

    if (!createNetworkInterface())
    {
        ALERT(0, 0, "start - createNetworkInterface failed");
        return false;
    }
	
         // Looks like we're ok
    
    fDataInterface->retain();
    fWorkLoop->retain();
    fTransmitQueue->retain();
    
        // Ready to service interface requests
    
    fNetworkInterface->registerService();
        
    XTRACE(this, 0, 0, "start - successful");
	Log(DEBUG_NAME ": Version number - %s, Input buffers %d, Output buffers %d\n", VersionNumber, fInBufPool, fOutBufPool);

    return true;
    	
}/* end start */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::stop
//
//		Inputs:		provider - my provider
//
//		Outputs:	
//
//		Desc:		Stops the driver
//
/****************************************************************************************************/

void AppleUSBCDCECMData::stop(IOService *provider)
{
    
    XTRACE(this, 0, 0, "stop");
    
        // Release all resources
		
    releaseResources();
    
    if (fDataInterface)	
    { 
        fDataInterface->close(this);
        fDataInterface->release();
        fDataInterface = NULL;
    }
    
    if (fNetworkInterface)
    {
        fNetworkInterface->release();
        fNetworkInterface = NULL;
    }

    if (fMediumDict)
    {
        fMediumDict->release();
        fMediumDict = NULL;
    }
    
    if (fBufferPoolLock)
    {
        IOLockFree(fBufferPoolLock);
        fBufferPoolLock = NULL;
    }
    
    if (fWorkLoop)
    {
        fWorkLoop->release();
        fWorkLoop = NULL;
    }
    
    if (fTransmitQueue)
    {
        fTransmitQueue->release();
        fTransmitQueue = NULL;
    }
    
    super::stop(provider);
    
    return;
	
}/* end stop */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::configureData
//
//		Inputs:		
//
//		Outputs:	return code - true (configure was successful), false (it failed)
//
//		Desc:		Finishes up the rest of the configuration
//
/****************************************************************************************************/

bool AppleUSBCDCECMData::configureData()
{
    IOUSBFindInterfaceRequest		req;
    const IOUSBInterfaceDescriptor	*altInterfaceDesc;
    IOReturn				ior = kIOReturnSuccess;
    UInt16				numends = 0;
    UInt16				alt;

    XTRACE(this, 0, 0, "configureData.");
    
    if (!fDataInterface)
    {
        XTRACE(this, 0, 0, "configureData - Data interface is NULL");
        return false;
    }

    if (!fDataInterface->open(this))
    {
        XTRACE(this, 0, 0, "configureData - open data interface failed");
        return false;
    }
        
        // Check we have the correct interface (there maybe an alternate)
    
    numends = fDataInterface->GetNumEndpoints();
    if (numends < 2)
    {
        req.bInterfaceClass = kUSBDataClass;
        req.bInterfaceSubClass = 0;
        req.bInterfaceProtocol = 0;
        req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
        altInterfaceDesc = fDataInterface->FindNextAltInterface(NULL, &req);
        if (!altInterfaceDesc)
        {
            XTRACE(this, 0, 0, "configureData - FindNextAltInterface failed");
            return false;
        }
        while (altInterfaceDesc)
        {
            numends = altInterfaceDesc->bNumEndpoints;
            if (numends > 1)
            {
                alt = altInterfaceDesc->bAlternateSetting;
                XTRACE(this, numends, alt, "configureData - Data Class interface (alternate) found");
                ior = fDataInterface->SetAlternateInterface(this, alt);
                if (ior == kIOReturnSuccess)
                {
                    XTRACE(this, 0, 0, "configureData - Alternate set");
                    break;
                } else {
                    XTRACE(this, 0, 0, "configureData - SetAlternateInterface failed");
                    return false;
                }
            } else {
                XTRACE(this, 0, altInterfaceDesc, "configureData - No endpoints this alternate");
            }
            altInterfaceDesc = fDataInterface->FindNextAltInterface(altInterfaceDesc, &req);
        }
    }
    
    if (numends < 2)
    {
        XTRACE(this, 0, 0, "configureData - Could not find the correct interface");
        return false;
    }
		
    return true;
	
}/* end configureData */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::createNetworkInterface
//
//		Inputs:		
//
//		Outputs:	return Code - true (created and initialilzed ok), false (it failed)
//
//		Desc:		Creates and initializes the network interface
//
/****************************************************************************************************/

bool AppleUSBCDCECMData::createNetworkInterface()
{
	
    XTRACE(this, 0, 0, "createNetworkInterface");
    
            // Allocate memory for transmit queue

    fTransmitQueue = (IOGatedOutputQueue *)getOutputQueue();
    if (!fTransmitQueue) 
    {
        ALERT(0, 0, "createNetworkInterface - Output queue initialization failed");
        return false;
    }
    
        // Allocate Timer event source
        
    fTimerSource = IOTimerEventSource::timerEventSource(this, timerFired);
    if (fTimerSource == NULL)
    {
        ALERT(0, 0, "createNetworkInterface - Allocate Timer event source failed");
        return false;
    }
    
    if (fWorkLoop->addEventSource(fTimerSource) != kIOReturnSuccess)
    {
        ALERT(0, 0, "createNetworkInterface - Add Timer event source failed");        
        return false;
    }

        // Attach an IOEthernetInterface client
        
    XTRACE(this, 0, 0, "createNetworkInterface - attaching and registering interface");
    
    if (!attachInterface((IONetworkInterface **)&fNetworkInterface, false))
    {	
        ALERT(0, 0, "createNetworkInterface - attachInterface failed");      
        return false;
    }
    
    XTRACE(this, 0, 0, "createNetworkInterface - Exiting, successful");

    return true;
	
}/* end createNetworkInterface */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::enable
//
//		Inputs:		netif - the interface being enabled
//
//		Outputs:	Return code - kIOReturnSuccess or kIOReturnIOError
//
//		Desc:		Called by IOEthernetInterface client to enable the controller.
//				This method is always called while running on the default workloop
//				thread
//
/****************************************************************************************************/

IOReturn AppleUSBCDCECMData::enable(IONetworkInterface *netif)
{
    IONetworkMedium	*medium;
    IOMediumType    	mediumType = kIOMediumEthernet10BaseT | kIOMediumOptionFullDuplex;
    
    XTRACE(this, 0, netif, "enable");
    
    IOSleep(5);				// Just in case (to let start finish - on another thread)

        // If an interface client has previously enabled us,
        // and we know there can only be one interface client
        // for this driver, then simply return success.

    if (fNetifEnabled)
    {
        XTRACE(this, 0, 0, "enable - already enabled");
        return kIOReturnSuccess;
    }
    
    if (!fControlDriver)
    {
        if (findControlDriverED(this) != kIOReturnSuccess)
        {
            ALERT(0, 0, "enable - Find control driver failed");
            return kIOReturnIOError;
        }
    }
    
    if (!fReady)
    {
        if (!wakeUp())
        {
            XTRACE(this, 0, fReady, "enable - wakeUp failed");
            return kIOReturnIOError;
        }
    }
    
    if (!fControlDriver->dataAcquired(fpNetStats, fpEtherStats))
    {
        XTRACE(this, 0, 0, "enable - dataAcquired to Control failed");
        return kIOReturnIOError;
    }

        // Mark the controller as enabled by the interface.

    fNetifEnabled = true;
    
        // Assume an active link (leave this in for now - until we know better)
        // Should probably use the values returned in the Network Connection notification
        // that is if we have an interrupt pipe, otherwise default to these
    
    fLinkStatus = 1;
    
    medium = IONetworkMedium::getMediumWithType(fMediumDict, mediumType);
    XTRACE(this, mediumType, medium, "enable - medium type and pointer");
    setLinkStatus(kIONetworkLinkActive | kIONetworkLinkValid, medium, 10 * 1000000);
    XTRACE(this, 0, 0, "enable - LinkStatus set");
    
        // Start our IOOutputQueue object.

    fTransmitQueue->setCapacity(TRANSMIT_QUEUE_SIZE);
    XTRACE(this, 0, TRANSMIT_QUEUE_SIZE, "enable - capicity set");
    fTransmitQueue->start();
    XTRACE(this, 0, 0, "enable - transmit queue started");
    
    if (fControlDriver)
    {
        fControlDriver->USBSetPacketFilter();
        XTRACE(this, 0, 0, "enable - packet filter applied");
    }
	
	Log("%s: - Link up\n", DEBUG_NAME);

    return kIOReturnSuccess;
    
}/* end enable */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::disable
//
//		Inputs:		netif - the interface being disabled
//
//		Outputs:	Return code - kIOReturnSuccess
//
//		Desc:		Called by IOEthernetInterface client to disable the controller.
//				This method is always called while running on the default workloop
//				thread
//
/****************************************************************************************************/
 
IOReturn AppleUSBCDCECMData::disable(IONetworkInterface *netif)
{

    XTRACE(this, 0, 0, "disable");
	
	setLinkStatus(kIONetworkLinkValid, 0);          // Link is inactive

        // Disable our IOOutputQueue object. This will prevent the
        // outputPacket() method from being called
        
	if (fTransmitQueue)
	{
		fTransmitQueue->stop();

			// Flush all packets currently in the output queue

		fTransmitQueue->setCapacity(0);
		fTransmitQueue->flush();
	}

    putToSleep();

    fNetifEnabled = false;
    fReady = false;
    
         // Tell the Control driver the port's been disabled
		 // If reset state's not normal then he already knows
        
	if (fResetState == kResetNormal)
	{
		if (fControlDriver)
		{
			fControlDriver->dataReleased();
		}
	}
	
//	fDataInterface->GetDevice()->ReEnumerateDevice(0);

    return kIOReturnSuccess;
    
}/* end disable */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::setWakeOnMagicPacket
//
//		Inputs:		active - true(wake), false(don't)
//
//		Outputs:	Return code - kIOReturnSuccess
//
//		Desc:		Set for wake on magic packet
//
/****************************************************************************************************/

IOReturn AppleUSBCDCECMData::setWakeOnMagicPacket(bool active)
{
    IOUSBDevRequest	devreq;
    IOReturn		ior = kIOReturnSuccess;

    XTRACE(this, 0, active, "setWakeOnMagicPacket");
	
    fWOL = active;
    
    if (fConfigAttributes & kUSBAtrRemoteWakeup)
    {
    
            // Set/Clear the Device Remote Wake feature depending upon the active flag
    
		devreq.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBStandard, kUSBDevice);
		if (active)
		{
			devreq.bRequest = kUSBRqSetFeature;
		} else {
			devreq.bRequest = kUSBRqClearFeature;
		}
		devreq.wValue = kUSBFeatureDeviceRemoteWakeup;
		devreq.wIndex = 0;
		devreq.wLength = 0;
		devreq.pData = 0;

		ior = fDataInterface->GetDevice()->DeviceRequest(&devreq);
		if (ior == kIOReturnSuccess)
		{
			XTRACE(this, 0, ior, "setWakeOnMagicPacket - Set/Clear remote wake up feature successful");
		} else {
			XTRACE(this, 0, ior, "setWakeOnMagicPacket - Set/Clear remote wake up feature failed");
		}
    } else {
        XTRACE(this, 0, 0, "setWakeOnMagicPacket - Remote wake up not supported");
    }

    
    return kIOReturnSuccess;
    
}/* end setWakeOnMagicPacket */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::getPacketFilters
//
//		Inputs:		group - the filter group
//
//		Outputs:	Return code - kIOReturnSuccess and others
//				filters - the capability
//
//		Desc:		Set the filter capability for the driver
//
/****************************************************************************************************/

IOReturn AppleUSBCDCECMData::getPacketFilters(const OSSymbol *group, UInt32 *filters) const
{
    IOReturn	rtn = kIOReturnSuccess;
    
    XTRACE(this, group, filters, "getPacketFilters");

    if (group == gIOEthernetWakeOnLANFilterGroup)
    {
        if (fConfigAttributes & kUSBAtrRemoteWakeup)
        {
            *filters = kIOEthernetWakeOnMagicPacket;
        } else {
            *filters = 0;
        }
    } else {
        if (group == gIONetworkFilterGroup)
        {
            *filters = kIOPacketFilterUnicast | kIOPacketFilterBroadcast | kIOPacketFilterMulticast | kIOPacketFilterMulticastAll | kIOPacketFilterPromiscuous;
        } else {
            rtn = super::getPacketFilters(group, filters);
        }
    }
    
    if (rtn != kIOReturnSuccess)
    {
        XTRACE(this, 0, rtn, "getPacketFilters - failed");
    }
    
    return rtn;
    
}/* end getPacketFilters */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::getMaxPacketSize
//
//		Inputs:		
//
//		Outputs:	Return code - kIOReturnSuccess or kIOReturnIOError
//					maxSize - the max size we currently support
//
//		Desc:		Set the max size from the descriptors of this device
//
/****************************************************************************************************/

IOReturn AppleUSBCDCECMData::getMaxPacketSize(UInt32 *maxSize) const
{
    IOReturn	rtn = kIOReturnSuccess;
    
    XTRACE(this, 0, 0, "getMaxPacketSize");
	
	if (fControlDriver)
	{
		XTRACE(this, 0, fControlDriver->fMax_Block_Size, "getMaxPacketSize - Setting max size");
		*maxSize = fControlDriver->fMax_Block_Size;
	} else {
		rtn = kIOReturnIOError;				// If we don't have a control driver by now we're in trouble...
		XTRACE(this, 0, 0, "getMaxPacketSize - No control driver???");
	}

    return rtn;
    
}/* end getMaxPacketSize */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::selectMedium
//
//		Inputs:
//
//		Outputs:
//
//		Desc:		Lets us know if someone is playing with ifconfig
//
/****************************************************************************************************/

IOReturn AppleUSBCDCECMData::selectMedium(const IONetworkMedium *medium)
{
    
    XTRACE(this, 0, 0, "selectMedium");

    setSelectedMedium(medium);
    
    return kIOReturnSuccess;
        
}/* end selectMedium */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::getHardwareAddress
//
//		Inputs:		
//
//		Outputs:	Return code - kIOReturnSuccess or kIOReturnError
//				ea - the address
//
//		Desc:		Get the ethernet address from the hardware (actually the descriptor)
//
/****************************************************************************************************/

IOReturn AppleUSBCDCECMData::getHardwareAddress(IOEthernetAddress *ea)
{
    UInt32      i;

    XTRACE(this, 0, 0, "getHardwareAddress");

    if (fControlDriver)
    {
        for (i=0; i<6; i++)
        {
            ea->bytes[i] = fControlDriver->fEaddr[i];
        }
    } else {					// Use cached address
        for (i=0; i<6; i++)
        {
            ea->bytes[i] = fEthernetaddr[i];
        }
    }

    return kIOReturnSuccess;
    
}/* end getHardwareAddress */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::newVendorString
//
//		Inputs:		
//
//		Outputs:	Return code - the vendor string
//
//		Desc:		Identifies the hardware vendor
//
/****************************************************************************************************/

const OSString* AppleUSBCDCECMData::newVendorString() const
{

    XTRACE(this, 0, 0, "newVendorString");
    
    return OSString::withCString((const char *)defaultName);		// Maybe we should use the descriptors

}/* end newVendorString */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::newModelString
//
//		Inputs:		
//
//		Outputs:	Return code - the model string
//
//		Desc:		Identifies the hardware model
//
/****************************************************************************************************/

const OSString* AppleUSBCDCECMData::newModelString() const
{

    XTRACE(this, 0, 0, "newModelString");
    
    return OSString::withCString("USB");		// Maybe we should use the descriptors
    
}/* end newModelString */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::newRevisionString
//
//		Inputs:		
//
//		Outputs:	Return code - the revision string
//
//		Desc:		Identifies the hardware revision
//
/****************************************************************************************************/

const OSString* AppleUSBCDCECMData::newRevisionString() const
{

    XTRACE(this, 0, 0, "newRevisionString");
    
    return OSString::withCString("");
    
}/* end newRevisionString */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::setMulticastMode
//
//		Inputs:		active - true (set it), false (don't)
//
//		Outputs:	Return code - kIOReturnSuccess
//
//		Desc:		Sets multicast mode
//
/****************************************************************************************************/

IOReturn AppleUSBCDCECMData::setMulticastMode(bool active)
{

    XTRACE(this, 0, active, "setMulticastMode");
    
    if (fControlDriver)
    { 
        if (active)
        {
            fControlDriver->fPacketFilter |= kPACKET_TYPE_ALL_MULTICAST;
        } else {
            fControlDriver->fPacketFilter &= ~kPACKET_TYPE_ALL_MULTICAST;
        }
    
        fControlDriver->USBSetPacketFilter();
    
        return kIOReturnSuccess;
    }
    
    return kIOReturnIOError;
    
}/* end setMulticastMode */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::setMulticastList
//
//		Inputs:		addrs - list of addresses
//				count - number in the list
//
//		Outputs:	Return code - kIOReturnSuccess or kIOReturnIOError
//
//		Desc:		Sets multicast list
//
/****************************************************************************************************/

IOReturn AppleUSBCDCECMData::setMulticastList(IOEthernetAddress *addrs, UInt32 count)
{
    bool	uStat;
    
    XTRACE(this, addrs, count, "setMulticastList");
    
    if (fControlDriver)
    {
        if (count != 0)
        {
            uStat = fControlDriver->USBSetMulticastFilter(addrs, count);
            if (!uStat)
            {
                return kIOReturnIOError;
            }
        }

        return kIOReturnSuccess;
    }
    
    return kIOReturnIOError;
    
}/* end setMulticastList */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::setPromiscuousMode
//
//		Inputs:		active - true (set it), false (don't)
//
//		Outputs:	Return code - kIOReturnSuccess
//
//		Desc:		Sets promiscuous mode
//
/****************************************************************************************************/

IOReturn AppleUSBCDCECMData::setPromiscuousMode(bool active)
{
    
    XTRACE(this, 0, active, "setPromiscuousMode");

    if (fControlDriver)
    {
        if (active)
        {
            fControlDriver->fPacketFilter |= kPACKET_TYPE_PROMISCUOUS;
        } else {
            fControlDriver->fPacketFilter &= ~kPACKET_TYPE_PROMISCUOUS;
        }
    
        fControlDriver->USBSetPacketFilter();
        
        return kIOReturnSuccess;
    }
    
    return kIOReturnIOError;
    
}/* end setPromiscuousMode */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::createOutputQueue
//
//		Inputs:		
//
//		Outputs:	Return code - the output queue
//
//		Desc:		Creates the output queue
//
/****************************************************************************************************/

IOOutputQueue* AppleUSBCDCECMData::createOutputQueue()
{

    XTRACE(this, 0, 0, "createOutputQueue");
    
    return IOBasicOutputQueue::withTarget(this, TRANSMIT_QUEUE_SIZE);
    
}/* end createOutputQueue */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::outputPacket
//
//		Inputs:		mbuf - the packet
//				param - optional parameter
//
//		Outputs:	Return code - kIOReturnOutputSuccess or kIOReturnOutputStall
//
//		Desc:		Packet transmission. The BSD mbuf needs to be formatted correctly
//				and transmitted
//
/****************************************************************************************************/

UInt32 AppleUSBCDCECMData::outputPacket(mbuf_t pkt, void *param)
{
    UInt32	ior = kIOReturnSuccess;
    
    XTRACE(this, pkt, 0, "outputPacket");

    if (!fLinkStatus)
    {
        XTRACE(this, pkt, fLinkStatus, "outputPacket - link is down");
        if (fControlDriver->fOutputErrsOK)
            fpNetStats->outputErrors++;
        freePacket(pkt);
        return kIOReturnOutputDropped;
    }
	
	if (fResetState != kResetNormal)
	{
		XTRACE(this, pkt, 0, "outputPacket - deferred reset");
        if (fControlDriver->fOutputErrsOK)
            fpNetStats->outputErrors++;
        freePacket(pkt);
		if (fResetState == kResetNeeded)
		{
			fResetState = kResetDone;
			fDataInterface->GetDevice()->ReEnumerateDevice(0);
		}
        return kIOReturnOutputDropped;
	}
    
    ior = USBTransmitPacket(pkt);
    if (ior != kIOReturnSuccess)
    {
        return kIOReturnOutputStall;
    }

    return kIOReturnOutputSuccess;
    
}/* end outputPacket */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::configureInterface
//
//		Inputs:		netif - the interface being configured
//
//		Outputs:	Return code - true (configured ok), false (not)
//
//		Desc:		Finish the network interface configuration
//
/****************************************************************************************************/

bool AppleUSBCDCECMData::configureInterface(IONetworkInterface *netif)
{
    IONetworkData	*nd;

    XTRACE(this, IOThreadSelf(), netif, "configureInterface");

    if (super::configureInterface(netif) == false)
    {
        ALERT(0, 0, "configureInterface - super failed");
        return false;
    }
    
        // Get a pointer to the statistics structure in the interface

    nd = netif->getNetworkData(kIONetworkStatsKey);
    if (!nd || !(fpNetStats = (IONetworkStats *)nd->getBuffer()))
    {
        ALERT(0, 0, "configureInterface - Invalid network statistics");
        return false;
    }

        // Get the Ethernet statistics structure

    nd = netif->getParameter(kIOEthernetStatsKey);
    if (!nd || !(fpEtherStats = (IOEthernetStats*)nd->getBuffer()))
    {
        ALERT(0, 0, "configureInterface - Invalid ethernet statistics\n");
        return false;
    }

    return true;
    
}/* end configureInterface */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::wakeUp
//
//		Inputs:		
//
//		Outputs:	Return Code - true(we're awake), false(failed)
//
//		Desc:		Resumes the device it it was suspended and then gets all the data
//				structures sorted out and all the pipes ready.
//
/****************************************************************************************************/

bool AppleUSBCDCECMData::wakeUp()
{
    IOReturn 	rtn = kIOReturnSuccess;
    UInt32	i;
    bool	readOK = false;

    XTRACE(this, 0, 0, "wakeUp");
	
	if ((fSleeping) && (fDataInterface))
	{
		fDataInterface->GetDevice()->ReEnumerateDevice(0);
		return false;
	}
    
    fReady = false;
    
    if (fTimerSource)
    { 
        fTimerSource->cancelTimeout();
    }
    
    setLinkStatus(kIONetworkLinkValid, 0);				// Initialize the link state
    
    if (!allocateResources()) 
    {
        ALERT(0, 0, "wakeUp - allocateResources failed");
    	return false;
    }

        // Kick off the data-in bulk pipe reads
    
    for (i=0; i<fInBufPool; i++)
    {
        if (fPipeInBuff[i].pipeInMDP)
        {
            fPipeInBuff[i].readCompletionInfo.parameter = (void *)i;
            rtn = fInPipe->Read(fPipeInBuff[i].pipeInMDP, &fPipeInBuff[i].readCompletionInfo, NULL);
            if (rtn == kIOReturnSuccess)
            {
                readOK = true;
            } else {
                XTRACE(this, i, rtn, "wakeUp - Read failed");
            }
        }
    }
			
    if (!readOK)
    {
    
    	// We failed for some reason
	
        ALERT(0, 0, "wakeUp - Starting the input pipe read(s) failed");
        return false;
    } else {
        if (!fMediumDict)
        {
            if (!createMediumTables())
            {
                ALERT(0, 0, "wakeUp - createMediumTables failed");
                return false;
            }
        }

        fTimerSource->setTimeoutMS(WATCHDOG_TIMER_MS);
        fReady = true;
    }

	fSleeping = false;
	
    return true;
	
}/* end wakeUp */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::putToSleep
//
//		Inputs:		
//
//		Outputs:	Return Code - true(we're asleep), false(failed)
//
//		Desc:		Do clean up and suspend the device.
//
/****************************************************************************************************/

void AppleUSBCDCECMData::putToSleep()
{

    XTRACE(this, 0, 0, "putToSleep");
        
	if (!fReady)							// We've been here before...
	{
		return;
	}
	
    fReady = false;
	
		// Abort any outstanding I/O
			
	if (fInPipe)
		fInPipe->Abort();
	if (fOutPipe)
		fOutPipe->Abort();

    if (fTimerSource)
    { 
        fTimerSource->cancelTimeout();
    }
    
    releaseResources();

    setLinkStatus(kIONetworkLinkValid, 0);
	
	fLinkStatus = 0;
	fSleeping = true;
	
	Log("%s: - Link down\n", DEBUG_NAME);

}/* end putToSleep */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::createMediumTables
//
//		Inputs:		
//
//		Outputs:	Return code - true (tables created), false (not created)
//
//		Desc:		Creates the medium tables
//
/****************************************************************************************************/

bool AppleUSBCDCECMData::createMediumTables()
{
    IONetworkMedium	*medium;
    UInt64		maxSpeed;
    UInt32		i;

    XTRACE(this, 0, 0, "createMediumTables");

    maxSpeed = 100;
    fMediumDict = OSDictionary::withCapacity(sizeof(mediumTable) / sizeof(mediumTable[0]));
    if (fMediumDict == 0)
    {
        XTRACE(this, 0, 0, "createMediumTables - create dict. failed");
        return false;
    }

    for (i = 0; i < sizeof(mediumTable) / sizeof(mediumTable[0]); i++)
    {
        medium = IONetworkMedium::medium(mediumTable[i].type, mediumTable[i].speed);
        if (medium && (medium->getSpeed() <= maxSpeed))
        {
            IONetworkMedium::addMedium(fMediumDict, medium);
            medium->release();
        }
    }

    if (publishMediumDictionary(fMediumDict) != true)
    {
        XTRACE(this, 0, 0, "createMediumTables - publish dict. failed");
        return false;
    }

    medium = IONetworkMedium::getMediumWithType(fMediumDict, kIOMediumEthernetAuto);
    setCurrentMedium(medium);

    return true;
    
}/* end createMediumTables */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::allocateResources
//
//		Inputs:		
//
//		Outputs:	return code - true (allocate was successful), false (it failed)
//
//		Desc:		Gets all the endpoints open and buffers allocated etc.
//
/****************************************************************************************************/

bool AppleUSBCDCECMData::allocateResources()
{
    IOUSBFindEndpointRequest		epReq;
    UInt32				i;

    XTRACE(this, 0, 0, "allocateResources.");
	
		// Check things are still around (waking from sleep)
	
	if (!fDataInterface)
	{
		XTRACE(this, 0, 0, "allocateResources - No data interface");
		return false;
	}
	
	if (!fControlDriver)
	{
		XTRACE(this, 0, 0, "allocateResources - No control driver");
		return false;
	}

        // Open all the end points

    epReq.type = kUSBBulk;
    epReq.direction = kUSBIn;
    epReq.maxPacketSize	= 0;
    epReq.interval = 0;
    fInPipe = fDataInterface->FindNextPipe(0, &epReq);
    if (!fInPipe)
    {
        XTRACE(this, 0, 0, "allocateResources - no bulk input pipe.");
        return false;
    }
    XTRACE(this, epReq.maxPacketSize << 16 |epReq.interval, fInPipe, "allocateResources - bulk input pipe.");

    epReq.direction = kUSBOut;
    fOutPipe = fDataInterface->FindNextPipe(0, &epReq);
    if (!fOutPipe)
    {
        XTRACE(this, 0, 0, "allocateResources - no bulk output pipe.");
        return false;
    }
    fOutPacketSize = epReq.maxPacketSize;
    XTRACE(this, epReq.maxPacketSize << 16 |epReq.interval, fOutPipe, "allocateResources - bulk output pipe.");
    
        // Allocate Memory Descriptor Pointer with memory for the data-in bulk pipe

    for (i=0; i<fInBufPool; i++)
    {
        fPipeInBuff[i].pipeInMDP = IOBufferMemoryDescriptor::withCapacity(fControlDriver->fMax_Block_Size, kIODirectionIn);
        if (!fPipeInBuff[i].pipeInMDP)
        {
            XTRACE(this, 0, i, "allocateResources - Allocate input descriptor failed");
            return false;
        }
		
        fPipeInBuff[i].pipeInMDP->setLength(fControlDriver->fMax_Block_Size);
        fPipeInBuff[i].pipeInBuffer = (UInt8*)fPipeInBuff[i].pipeInMDP->getBytesNoCopy();
        XTRACE(this, fPipeInBuff[i].pipeInMDP, fPipeInBuff[i].pipeInBuffer, "allocateResources - input buffer");
        fPipeInBuff[i].dead = false;
        fPipeInBuff[i].readCompletionInfo.target = this;
        fPipeInBuff[i].readCompletionInfo.action = dataReadComplete;
        fPipeInBuff[i].readCompletionInfo.parameter = NULL;
    }
    
        // Allocate Memory Descriptor Pointers with memory for the data-out bulk pipe pool

    for (i=0; i<fOutBufPool; i++)
    {
        fPipeOutBuff[i].pipeOutMDP = IOBufferMemoryDescriptor::withCapacity(fControlDriver->fMax_Block_Size, kIODirectionOut);
        if (!fPipeOutBuff[i].pipeOutMDP)
        {
            XTRACE(this, 0, i, "allocateResources - Allocate output descriptor failed");
            return false;
        }
		
        fPipeOutBuff[i].pipeOutMDP->setLength(fControlDriver->fMax_Block_Size);
        fPipeOutBuff[i].pipeOutBuffer = (UInt8*)fPipeOutBuff[i].pipeOutMDP->getBytesNoCopy();
        XTRACE(this, fPipeOutBuff[i].pipeOutMDP, fPipeOutBuff[i].pipeOutBuffer, "allocateResources - output buffer");
        fPipeOutBuff[i].avail = true;
        fPipeOutBuff[i].writeCompletionInfo.target = this;
        fPipeOutBuff[i].writeCompletionInfo.action = dataWriteComplete;
        fPipeOutBuff[i].writeCompletionInfo.parameter = NULL;				// for now, filled in with pool index when sent
    }
		
    return true;
	
}/* end allocateResources */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::releaseResources
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Frees up the resources allocated in allocateResources
//
/****************************************************************************************************/

void AppleUSBCDCECMData::releaseResources()
{
    UInt32	i;
    
    XTRACE(this, 0, 0, "releaseResources");

    for (i=0; i<fOutBufPool; i++)
    {
        if (fPipeOutBuff[i].pipeOutMDP)	
        { 
            fPipeOutBuff[i].pipeOutMDP->release();	
            fPipeOutBuff[i].pipeOutMDP = NULL;
            fPipeOutBuff[i].avail = false;
            fPipeOutBuff[i].writeCompletionInfo.target = NULL;
            fPipeOutBuff[i].writeCompletionInfo.action = NULL;
            fPipeOutBuff[i].writeCompletionInfo.parameter = NULL;
        }
    }
    fOutPoolIndex = 0;
    
    for (i=0; i<fInBufPool; i++)
    {
        if (fPipeInBuff[i].pipeInMDP)	
        { 
            fPipeInBuff[i].pipeInMDP->release();	
            fPipeInBuff[i].pipeInMDP = NULL;
            fPipeInBuff[i].dead = false;
            fPipeInBuff[i].readCompletionInfo.target = NULL;
            fPipeInBuff[i].readCompletionInfo.action = NULL;
            fPipeInBuff[i].readCompletionInfo.parameter = NULL;
        }
    }

}/* end releaseResources */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::getOutputBuffer
//
//		Inputs:		bufIndx - index of an available buffer
//
//		Outputs:	Return code - True (got one), False (none available)
//
//		Desc:		Get an available buffer from the output buffer pool
//
/****************************************************************************************************/

bool AppleUSBCDCECMData::getOutputBuffer(UInt32 *bufIndx)
{
	bool	gotBuffer = false;
	UInt32	indx = 0;
	
	XTRACE(this, 0, 0, "getOutputBuffer");
	
	if (fBufferPoolLock)
	{
		IOLockLock(fBufferPoolLock);
	} else {
		XTRACE(this, 0, 0, "getOutputBuffer - Cannot get buffer pool lock");
		*bufIndx = 0;
		return false;
	}
	
		// Get an ouput buffer (use the hint first then if that's not available look for one and then create one...)
		
	indx = fOutPoolIndex;
	if (!fPipeOutBuff[indx].avail)
	{
		for (indx=0; indx<fOutBufPool; indx++)
		{
			if (fPipeOutBuff[indx].avail)
			{
				fOutPoolIndex = indx;
				gotBuffer = true;
				break;
			}
		}
	} else {
		gotBuffer = true;
	}
	
	if (gotBuffer)
	{
		fPipeOutBuff[indx].avail = false;
		fOutPoolIndex++;
		if (fOutPoolIndex >= fOutBufPool)
		{
			fOutPoolIndex = 0;
		}
	} else {
		if (fOutBufPool >= kMaxOutBufPool)
		{
			ALERT(kMaxOutBufPool, fOutBufPool, "getOutputBuffer - Output buffer pool empty");
			indx = 0;
			gotBuffer = false;
		} else {
			XTRACE(this, 0, fOutBufPool, "getOutputBuffer - Adding output buffer to pool");
		
				// Create a new one (should never really get here - maybe very very heavy transmit traffic)
			
			indx = fOutBufPool;
			fPipeOutBuff[indx].pipeOutMDP = IOBufferMemoryDescriptor::withCapacity(fControlDriver->fMax_Block_Size, kIODirectionOut);
			if (!fPipeOutBuff[indx].pipeOutMDP)
			{
				XTRACE(this, 0, indx, "getOutputBuffer - Allocate output descriptor failed");
				gotBuffer = false;
				indx = 0;
			} else {
				fPipeOutBuff[indx].pipeOutMDP->setLength(fControlDriver->fMax_Block_Size);
				fPipeOutBuff[indx].pipeOutBuffer = (UInt8*)fPipeOutBuff[indx].pipeOutMDP->getBytesNoCopy();
				XTRACE(this, fPipeOutBuff[indx].pipeOutMDP, fPipeOutBuff[indx].pipeOutBuffer, "getOutputBuffer - output buffer");
				fPipeOutBuff[indx].avail = false;
				fPipeOutBuff[indx].writeCompletionInfo.target = this;
				fPipeOutBuff[indx].writeCompletionInfo.action = dataWriteComplete;
				fPipeOutBuff[indx].writeCompletionInfo.parameter = NULL;
				fOutBufPool++;
				fTransmitQueue->setCapacity(fOutBufPool);
				fOutPoolIndex = 0;
				gotBuffer = true;
			}
		}
	}
	
	*bufIndx = indx;
	
	IOLockUnlock(fBufferPoolLock);
		
	return gotBuffer;

}/* end getOutputBuffer */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::USBTransmitPacket
//
//		Inputs:		packet - the packet
//
//		Outputs:	Return code - kIOReturnSuccess (transmit started), everything else (it didn't)
//
//		Desc:		Set up and then transmit the packet.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCECMData::USBTransmitPacket(mbuf_t packet)
{
    UInt32		numbufs = 0;			// number of mbufs for this packet
    mbuf_t		m;						// current mbuf
    UInt32		total_pkt_length = 0;
    UInt32		rTotal = 0;
    IOReturn	ior = kIOReturnSuccess;
    UInt32		indx;
	UInt16		bufTries = 0;
	
    XTRACE(this, 0, packet, "USBTransmitPacket - with timeout");
	
	if (fDeferredClear)
	{
		ior = clearPipeStall(fOutPipe);
		if (ior != kIOReturnSuccess)
		{
			XTRACE(this, 0, ior, "USBTransmitPacket - clear stall failed (trying to continue)");
		}
		fDeferredClear = false;
	}
			
		// Count the number of mbufs in this packet
		
	m = packet;
    while (m)
	{
		if (mbuf_len(m) != 0)
		{
			total_pkt_length += mbuf_len(m);
			numbufs++;
		}
		m = mbuf_next(m);
    }
    
    XTRACE(this, total_pkt_length, numbufs, "USBTransmitPacket - Total packet length and Number of mbufs");
    
    if (total_pkt_length > fControlDriver->fMax_Block_Size)
    {
        XTRACE(this, 0, 0, "USBTransmitPacket - Bad packet size");	// Note for now and revisit later
        if (fControlDriver->fOutputErrsOK)
            fpNetStats->outputErrors++;
		freePacket(packet);
        return kIOReturnOutputDropped;
    }
	
	while (1)
	{
		if (!getOutputBuffer(&indx))
		{
			if (bufTries++ > 8)
			{
				ALERT(fOutBufPool, fOutPoolIndex, "USBTransmitPacket - Output buffer unavailable");
				if (fControlDriver->fOutputErrsOK)
				{
					fpNetStats->outputErrors++;
				}
				freePacket(packet);
				return kIOReturnOutputDropped;
			} else {
//				ALERT(fOutBufPool, bufTries, "USBTransmitPacket - Sleeping 100 milliseconds...");
				IOSleep(100);				// See if any free up
			}
		} else {
			break;
		}
	}
    
        // Start filling in the send buffer

    m = packet;							// start with the first mbuf of the packet
    rTotal = 0;							// running total				
    do
    {  
        if (mbuf_len(m) == 0)			// Ignore zero length buffers
			continue;
        
        bcopy(mbuf_data(m), &fPipeOutBuff[indx].pipeOutBuffer[rTotal], mbuf_len(m));
        rTotal += mbuf_len(m);
        
    } while ((m = mbuf_next(m)) != 0);
	
    LogData(kDataOut, rTotal, fPipeOutBuff[indx].pipeOutBuffer);
	
    fPipeOutBuff[indx].m = packet;
    fPipeOutBuff[indx].writeCompletionInfo.parameter = (void *)indx;
//    fPipeOutBuff[indx].pipeOutMDP->setLength(rTotal);
//    ior = fOutPipe->Write(fPipeOutBuff[indx].pipeOutMDP, &fPipeOutBuff[indx].writeCompletionInfo);
	ior = fOutPipe->Write(fPipeOutBuff[indx].pipeOutMDP, 2000, 5000, rTotal, &fPipeOutBuff[indx].writeCompletionInfo);
    if (ior != kIOReturnSuccess)
    {
        XTRACE(this, 0, ior, "USBTransmitPacket - Write failed");
        if (ior == kIOUSBPipeStalled)
        {
			XTRACE(this, 0, 0, "USBTransmitPacket - Pipe stalled, retrying...");
            fOutPipe->Reset();
//            ior = fOutPipe->Write(fPipeOutBuff[indx].pipeOutMDP, &fPipeOutBuff[indx].writeCompletionInfo);
			ior = fOutPipe->Write(fPipeOutBuff[indx].pipeOutMDP, 2000, 5000, rTotal, &fPipeOutBuff[indx].writeCompletionInfo);
            if (ior != kIOReturnSuccess)
            {
                XTRACE(this, 0, ior, "USBTransmitPacket - Write really failed");
                if (fControlDriver->fOutputErrsOK)
                    fpNetStats->outputErrors++;
				if (fBufferPoolLock)
				{
					IOLockLock(fBufferPoolLock);
				}
				fPipeOutBuff[indx].avail = true;
				if (fBufferPoolLock)
				{
					IOLockUnlock(fBufferPoolLock);
				}
                return ior;
            }
        } else {
			if (fControlDriver->fOutputErrsOK)
				fpNetStats->outputErrors++;
			if (fBufferPoolLock)
			{
				IOLockLock(fBufferPoolLock);
			}
			fPipeOutBuff[indx].avail = true;
			if (fBufferPoolLock)
			{
				IOLockUnlock(fBufferPoolLock);
			}
			return ior;
		}
    }
    if (fControlDriver->fOutputPktsOK)		
        fpNetStats->outputPackets++;
    
    return ior;

}/* end USBTransmitPacket */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::clearPipeStall
//
//		Inputs:		thePipe - the pipe
//
//		Outputs:	
//
//		Desc:		Clear a stall on the specified pipe.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCECMData::clearPipeStall(IOUSBPipe *thePipe)
{
    IOReturn 	rtn = kIOReturnSuccess;
    
    XTRACE(this, 0, thePipe, "clearPipeStall");
    
//    rtn = thePipe->GetPipeStatus();
//    if (rtn == kIOUSBPipeStalled)
//    {
        rtn = thePipe->ClearPipeStall(true);
        if (rtn == kIOReturnSuccess)
        {
            XTRACE(this, 0, 0, "clearPipeStall - Successful");
        } else {
            XTRACE(this, 0, rtn, "clearPipeStall - Failed");
        }
//    } else {
//        XTRACE(this, 0, 0, "clearPipeStall - Pipe not stalled");
//    }
    
    return rtn;

}/* end clearPipeStall */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::receivePacket
//
//		Inputs:		packet - the packet
//				size - Number of bytes in the packet
//
//		Outputs:	
//
//		Desc:		Build the mbufs and then send to the network stack.
//
/****************************************************************************************************/

void AppleUSBCDCECMData::receivePacket(UInt8 *packet, UInt32 size)
{
    mbuf_t		m;
    UInt32		submit;
    
    XTRACE(this, 0, size, "receivePacket");
    
    if (size > fControlDriver->fMax_Block_Size)
    {
        XTRACE(this, 0, 0, "receivePacket - Packet size error, packet dropped");
        if (fControlDriver->fInputErrsOK)
            fpNetStats->inputErrors++;
        return;
    }
    
    m = allocatePacket(size);
    if (m)
    {
        bcopy(packet, mbuf_data(m), size);
        submit = fNetworkInterface->inputPacket(m, size);
        XTRACE(this, 0, submit, "receivePacket - Packets submitted");
        if (fControlDriver->fInputPktsOK)
            fpNetStats->inputPackets++;
    } else {
        XTRACE(this, 0, 0, "receivePacket - Buffer allocation failed, packet dropped");
        if (fControlDriver->fInputErrsOK)
            fpNetStats->inputErrors++;
    }

}/* end receivePacket */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::timerFired
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Static member function called when a timer event fires.
//
/****************************************************************************************************/
void AppleUSBCDCECMData::timerFired(OSObject *owner, IOTimerEventSource *sender)
{

//    XTRACE(this, 0, 0, "timerFired");
    
    if (owner)
    {
	AppleUSBCDCECMData* target = OSDynamicCast(AppleUSBCDCECMData, owner);
	
	if (target)
	{
	    target->timeoutOccurred(sender);
	}
    }
    
}/* end timerFired */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::timeoutOccurred
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Timeout handler, used for stats gathering.
//
/****************************************************************************************************/

void AppleUSBCDCECMData::timeoutOccurred(IOTimerEventSource * /*timer*/)
{
    bool		statsOK = false;

//    XTRACE(this, 0, 0, "timeoutOccurred");

    if (fControlDriver)
    {
        statsOK = fControlDriver->statsProcessing();
    }

    if (statsOK)
    {
            
            // Restart the watchdog timer
        
        fTimerSource->setTimeoutMS(WATCHDOG_TIMER_MS);
    }

}/* end timeoutOccurred */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCECMData::message
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

IOReturn AppleUSBCDCECMData::message(UInt32 type, IOService *provider, void *argument)
{
    UInt16	i;
    IOReturn	ior;
	
    XTRACE(this, 0, type, "message");
	
    switch (type)
    {
        case kIOMessageServiceIsTerminated:
            XTRACE(this, fReady, type, "message - kIOMessageServiceIsTerminated");
#if 0
				// As a precaution abort any outstanding I/O
			
			if (fInPipe)
				fInPipe->Abort();
			if (fOutPipe)
				fOutPipe->Abort();
#endif
						
            if (fReady)
            {
                if (!fTerminate)		// Check if we're already being terminated
                {
#if 0
		    // NOTE! This call below depends on the hard coded path of this KEXT. Make sure
		    // that if the KEXT moves, this path is changed!
		    KUNCUserNotificationDisplayNotice(
			10,		// Timeout in seconds
			0,		// Flags (for later usage)
			"",		// iconPath (not supported yet)
			"",		// soundPath (not supported yet)
			"/System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCECMData.kext",	// localizationPath
			"Unplug Header",		// the header
			"Unplug Notice",		// the notice - look in Localizable.strings
			"OK");
#endif
                }
            }
			
			putToSleep();
            
//            releaseResources();

            if (fDataInterface)	
            { 
                fDataInterface->close(this);
                fDataInterface->release();
                fDataInterface = NULL;
            }
            fTerminate = true;		// we're being terminated (unplugged)
            fLinkStatus = 0;		// and of course we're offline
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
            for (i=0; i<fInBufPool; i++)
            {
                if (fPipeInBuff[i].dead)			// If it's dead try and resurrect it
                {
                    ior = fInPipe->Read(fPipeInBuff[i].pipeInMDP, &fPipeInBuff[i].readCompletionInfo, NULL);
                    if (ior != kIOReturnSuccess)
                    {
                        XTRACE(this, 0, ior, "message - Read io error");
                    } else {
                        fPipeInBuff[i].dead = false;
                    }
                }
            }
            break;
        case kIOUSBMessageHubResumePort:
            XTRACE(this, 0, type, "message - kIOUSBMessageHubResumePort");
            break;
		case kIOMessageServiceIsAttemptingOpen:
			XTRACE(this, 0, type, "message - kIOMessageServiceIsAttemptingOpen");
			break;
        default:
            XTRACE(this, 0, type, "message - unknown message"); 
            break;
    }
    
    return kIOReturnUnsupported;
    
}/* end message */

/****************************************************************************************************/
//
//	Method:		AppleUSBCDCECMData::registerWithPolicyMaker
//
//	Inputs:		provider - my provider
//
//	Outputs:	return code - From policy maker
//
//	Desc:		Initialize the driver for power management and register ourselves with policy-maker.
//				Called by superclass - not by Power Management.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCECMData::registerWithPolicyMaker(IOService *policyMaker)
{
	IOReturn	ior;

	XTRACE(this, 0, 0, "registerWithPolicyMaker");

	ior = policyMaker->registerPowerDriver(this, gOurPowerStates, kNumCDCStates);

	return ior;
	
}/* end registerWithPolicyMaker */
