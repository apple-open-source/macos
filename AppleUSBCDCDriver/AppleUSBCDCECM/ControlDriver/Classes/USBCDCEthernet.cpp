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

/*
 File:		USBCDCEthernet.cpp

 Description:	This is a sample USB Communication Device Class (CDC) driver, Ethernet model.
 */
 
#include "USBCDCEthernet.h"

#define MIN_BAUD (50 << 1)

//static globals	g;	// Instantiate the globals

#if USE_ELG
    com_apple_iokit_XTrace	*gXTrace = 0;
#endif
    
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

#define	numStats	13
UInt16	stats[13] = {	kXMIT_OK_REQ,
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

#define super IOEthernetController

OSDefineMetaClassAndStructors(AppleUSBCDCEthernet, IOEthernetController);

#if USE_ELG
#define DEBUG_NAME "AppleUSBCDCEthernet"

/****************************************************************************************************/
//
//		Function:	findKernelLoggerUSBEnet
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Just like the name says
//
/****************************************************************************************************/

IOReturn findKernelLoggerUSBEnet()
{
    OSIterator		*iterator = NULL;
    OSDictionary	*matchingDictionary = NULL;
    IOReturn		error = 0;
	
	// Get matching dictionary
	
    matchingDictionary = IOService::serviceMatching("com_apple_iokit_XTrace");
    if (!matchingDictionary)
    {
        error = kIOReturnError;
        IOLog(DEBUG_NAME "[findKernelLoggerUSBEnet] Couldn't create a matching dictionary.\n");
        goto exit;
    }
	
	// Get an iterator
	
    iterator = IOService::getMatchingServices(matchingDictionary);
    if (!iterator)
    {
        error = kIOReturnError;
        IOLog(DEBUG_NAME "[findKernelLoggerUSBEnet] No XTrace logger found.\n");
        goto exit;
    }
	
	// User iterator to find each com_apple_iokit_XTrace instance. There should be only one, so we
	// won't iterate
	
    gXTrace = (com_apple_iokit_XTrace*)iterator->getNextObject();
    if (gXTrace)
    {
        IOLog(DEBUG_NAME "[findKernelLoggerUSBEnet] Found XTrace logger at %p.\n", gXTrace);
    }
	
exit:
	
    if (error != kIOReturnSuccess)
    {
        gXTrace = NULL;
        IOLog(DEBUG_NAME "[findKernelLoggerUSBEnet] Could not find a logger instance. Error = %X.\n", error);
    }
	
    if (matchingDictionary)
        matchingDictionary->release();
            
    if (iterator)
        iterator->release();
		
    return error;
    
}/* end findKernelLoggerUSBEnet */
#endif

/****************************************************************************************************/
//
//		Function:	Asciihex_to_binary
//
//		Inputs:		c - Ascii character
//
//		Outputs:	return byte - binary byte
//
//		Desc:		Converts to hex (binary). 
//
/****************************************************************************************************/

UInt8 Asciihex_to_binary(char c)
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

void USBLogData(UInt8 Dir, UInt32 Count, char *buf, void *id)
{    
    SInt32	wlen;
    UInt8	tDir = Dir;
#if USE_ELG
    UInt8 	*b;
    UInt8 	w[8];
#else
    UInt32	llen, rlen;
    UInt16	i, Aspnt, Hxpnt;
    UInt8	wchr;
    char	LocBuf[buflen+1];
#endif
    
    switch (tDir)
    {
        case kDataIn:
#if USE_ELG
            XTRACE2(id, buf, Count, "USBLogData - Read Complete, address, size");
#else
            IOLog("AppleUSBCDCEthernet: USBLogData - Read Complete, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count);
#endif
            break;
        case kDataOut:
#if USE_ELG
            XTRACE2(id, buf, Count, "USBLogData - Write, address, size");
#else
            IOLog("AppleUSBCDCEthernet: USBLogData - Write, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count);
#endif
            break;
        case kDataOther:
#if USE_ELG
            XTRACE2(id, buf, Count, "USBLogData - Other, address, size");
#else
            IOLog("AppleUSBCDCEthernet: USBLogData - Other, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count);
#endif
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
#if USE_ELG
        XTRACE2(id, 0, Count, "USBLogData - No data, Count=0");
#else
        IOLog("AppleUSBCDCEthernet: USBLogData - No data, Count=0\n");
#endif
        return;
    }

#if (USE_ELG)
    b = (UInt8 *)buf;
    while (wlen > 0)							// loop over the buffer
    {
        bzero(w, sizeof(w));						// zero it
        bcopy(b, w, min(wlen, 8));					// copy bytes over
    
        switch (tDir)
        {
            case kDataIn:
                XTRACE2(id, (w[0] << 24 | w[1] << 16 | w[2] << 8 | w[3]), (w[4] << 24 | w[5] << 16 | w[6] << 8 | w[7]), "USBLogData - Rx buffer dump");
                break;
            case kDataOut:
                XTRACE2(id, (w[0] << 24 | w[1] << 16 | w[2] << 8 | w[3]), (w[4] << 24 | w[5] << 16 | w[6] << 8 | w[7]), "USBLogData - Tx buffer dump");
                break;
            case kDataOther:
                XTRACE2(id, (w[0] << 24 | w[1] << 16 | w[2] << 8 | w[3]), (w[4] << 24 | w[5] << 16 | w[6] << 8 | w[7]), "USBLogData - Misc buffer dump");
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

/****************************************************************************************************/
//
//		Function:	dumpData
//
//		Inputs:		buf - the data
//				size - number of bytes
//
//		Outputs:	None
//
//		Desc:		Creats formatted data for the log (cannot be used at interrupt time) 
//
/****************************************************************************************************/

void dumpData(char *buf, UInt32 size)
{
    UInt32	curr, len, dlen;

    IOLog("AppleUSBCDCEthernet: dumpData - Address = %8x, size = %8d\n", (UInt)buf, (UInt)size);

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
        IOLog("%8x ", (UInt)&buf[curr]);
        USBLogData(kDataNone, dlen, &buf[curr], 0);
        len -= dlen;
    }
   
}/* end dumpData */
#endif

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::commReadComplete
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

void AppleUSBCDCEthernet::commReadComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    AppleUSBCDCEthernet	*me = (AppleUSBCDCEthernet*)obj;
    IOReturn		ior;
    UInt32		dLen;
    UInt8		notif;

    if (rc == kIOReturnSuccess)	// If operation returned ok
    {
        dLen = COMM_BUFF_SIZE - remaining;
        XTRACE(me, rc, dLen, "commReadComplete");
		
            // Now look at the state stuff
            
        LogData(kDataOther, dLen, me->fCommPipeBuffer, me);
        
        notif = me->fCommPipeBuffer[1];
        if (dLen > 7)
        {
            switch(notif)
            {
                case kNetwork_Connection:
                    me->fLinkStatus = me->fCommPipeBuffer[2];
                    XTRACE(me, 0, me->fLinkStatus, "commReadComplete - kNetwork_Connection");
                    break;
                case kConnection_Speed_Change:				// In you-know-whose format
                    me->fUpSpeed = USBToHostLong((UInt32)me->fCommPipeBuffer[8]);
                    me->fDownSpeed = USBToHostLong((UInt32)me->fCommPipeBuffer[13]);
                    XTRACE(me, me->fUpSpeed, me->fDownSpeed, "commReadComplete - kConnection_Speed_Change");
                    break;
                default:
                    XTRACE(me, 0, notif, "commReadComplete - Unknown notification");
                    break;
            }
        } else {
            XTRACE(me, 0, notif, "commReadComplete - Invalid notification");
        }
    } else {
        XTRACE(me, 0, rc, "commReadComplete - IO error");
    }

        // Queue the next read, only if not aborted
	
    if (rc != kIOReturnAborted)
    {
        ior = me->fCommPipe->Read(me->fCommPipeMDP, &me->fCommCompletionInfo, NULL);
        if (ior != kIOReturnSuccess)
        {
            XTRACE(me, 0, ior, "commReadComplete - Failed to queue next read");
            if (ior == kIOUSBPipeStalled)
            {
                me->fCommPipe->Reset();
                ior = me->fCommPipe->Read(me->fCommPipeMDP, &me->fCommCompletionInfo, NULL);
                if (ior != kIOReturnSuccess)
                {
                    XTRACE(me, 0, ior, "commReadComplete - Failed, read dead");
                    me->fCommDead = true;
                }
            }

        }
    }
    
    return;
	
}/* end commReadComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::dataReadComplete
//
//		Inputs:		obj - me
//				param - unused
//				rc - return code
//				remaining - what's left
//
//		Outputs:	
//
//		Desc:		BulkIn pipe (Data interface) read completion routine
//
/****************************************************************************************************/

void AppleUSBCDCEthernet::dataReadComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    AppleUSBCDCEthernet	*me = (AppleUSBCDCEthernet*)obj;
    IOReturn		ior;
    
    XTRACE(me, 0, 0, "dataReadComplete");

    if (rc == kIOReturnSuccess)	// If operation returned ok
    {	
        XTRACE(me, 0, me->fMax_Block_Size - remaining, "dataReadComplete - data length");
		
        LogData(kDataIn, (me->fMax_Block_Size - remaining), me->fPipeInBuffer, me);
	
            // Move the incoming bytes up the stack

        me->receivePacket(me->fPipeInBuffer, me->fMax_Block_Size - remaining);
	
    } else {
        XTRACE(me, 0, rc, "dataReadComplete - Read completion io err");
        if (rc != kIOReturnAborted)
        {
            rc = me->clearPipeStall(me->fInPipe);
            if (rc != kIOReturnSuccess)
            {
                XTRACE(me, 0, rc, "dataReadComplete - clear stall failed (trying to continue)");
            }
        }
    }
    
        // Queue the next read, only if not aborted
	
    if (rc != kIOReturnAborted)
    {
        ior = me->fInPipe->Read(me->fPipeInMDP, &me->fReadCompletionInfo, NULL);
        if (ior != kIOReturnSuccess)
        {
            XTRACE(me, 0, ior, "dataReadComplete - Failed to queue read");
            if (ior == kIOUSBPipeStalled)
            {
                me->fInPipe->Reset();
                ior = me->fInPipe->Read(me->fPipeInMDP, &me->fReadCompletionInfo, NULL);
                if (ior != kIOReturnSuccess)
                {
                    XTRACE(me, 0, ior, "dataReadComplete - Failed, read dead");
                    me->fDataDead = true;
                }
            }
        }
    }

    return;
	
}/* end dataReadComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::dataWriteComplete
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

void AppleUSBCDCEthernet::dataWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    AppleUSBCDCEthernet	*me = (AppleUSBCDCEthernet *)obj;
    struct mbuf		*m;
    UInt32		pktLen = 0;
    UInt32		numbufs = 0;
    UInt32		poolIndx;

    poolIndx = (UInt32)param;
    
    if (rc == kIOReturnSuccess)						// If operation returned ok
    {	
        XTRACE(me, rc, poolIndx, "dataWriteComplete");
        
        if (me->fPipeOutBuff[poolIndx].m != NULL)			// Null means zero length write
        {
            m = me->fPipeOutBuff[poolIndx].m;
            while (m)
            {
                pktLen += m->m_len;
                numbufs++;
                m = m->m_next;
            }
            
            me->freePacket(me->fPipeOutBuff[poolIndx].m);		// Free the mbuf
            me->fPipeOutBuff[poolIndx].m = NULL;
        
            if ((pktLen % me->fOutPacketSize) == 0)			// If it was a multiple of max packet size then we need to do a zero length write
            {
                XTRACE(me, rc, pktLen, "dataWriteComplete - writing zero length packet");
                me->fPipeOutBuff[poolIndx].pipeOutMDP->setLength(0);
                me->fWriteCompletionInfo.parameter = (void *)poolIndx;
                me->fOutPipe->Write(me->fPipeOutBuff[poolIndx].pipeOutMDP, &me->fWriteCompletionInfo);
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
        if (rc != kIOReturnAborted)
        {
            rc = me->clearPipeStall(me->fOutPipe);
            if (rc != kIOReturnSuccess)
            {
                XTRACE(me, 0, rc, "dataWriteComplete - clear stall failed (trying to continue)");
            }
        }
    }
        
    return;
	
}/* end dataWriteComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::merWriteComplete
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

void AppleUSBCDCEthernet::merWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
#if LDEBUG
    AppleUSBCDCEthernet	*me = (AppleUSBCDCEthernet *)obj;
#endif
    IOUSBDevRequest	*MER = (IOUSBDevRequest*)param;
    UInt16		dataLen;
	
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
//		Method:		AppleUSBCDCEthernet::statsWriteComplete
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

void AppleUSBCDCEthernet::statsWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    AppleUSBCDCEthernet	*me = (AppleUSBCDCEthernet *)obj;
    IOUSBDevRequest	*STREQ = (IOUSBDevRequest*)param;
    UInt16		currStat;
	
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
//		Method:		AppleUSBCDCEthernet::init
//
//		Inputs:		properties - data (keys and values) used to match
//
//		Outputs:	Return code - true (init successful), false (init failed)
//
//		Desc:		Initialize the driver.
//
/****************************************************************************************************/

bool AppleUSBCDCEthernet::init(OSDictionary *properties)
{
    UInt32	i;
        
#if USE_ELG
    XTraceLogInfo	*logInfo;
    
    findKernelLoggerUSBEnet();
    if (gXTrace)
    {
        gXTrace->retain();		// don't let it unload ...
        XTRACE(this, 0, 0xbeefbeef, "Hello from start");
        logInfo = gXTrace->LogGetInfo();
        IOLog("AppleUSBCDCEthernet: start - Log is at %x\n", (unsigned int)logInfo);
    } else {
        return false;
    }
#endif

    XTRACE(this, 0, 0, "init");
    
    if (super::init(properties) == false)
    {
        XTRACE(this, 0, 0, "init - initialize super failed");
        return false;
    }
    
        // Set some defaults
    
    fMax_Block_Size = MAX_BLOCK_SIZE;
    fCurrStat = 0;
    fStatInProgress = false;
    fDataDead = false;
    fCommDead = false;
    fPacketFilter = kPACKET_TYPE_DIRECTED | kPACKET_TYPE_BROADCAST | kPACKET_TYPE_MULTICAST;
    
    for (i=0; i<kOutBufPool; i++)
    {
        fPipeOutBuff[i].pipeOutMDP = NULL;
        fPipeOutBuff[i].pipeOutBuffer = NULL;
        fPipeOutBuff[i].m = NULL;
        fPipeOutBuff[i].avail = false;
    }

    return true;

}/* end init*/

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::start
//
//		Inputs:		provider - my provider
//
//		Outputs:	Return code - true (it's me), false (sorry it probably was me, but I can't configure it)
//
//		Desc:		This is called once it has beed determined I'm probably the best 
//				driver for this device.
//
/****************************************************************************************************/

bool AppleUSBCDCEthernet::start(IOService *provider)
{
    UInt8	configs;	// number of device configurations

    XTRACE(this, 0, provider, "start");
    
    if(!super::start(provider))
    {
        ALERT(0, 0, "start - start super failed");
        return false;
    }

	// Get my USB device provider - the device

    fpDevice = OSDynamicCast(IOUSBDevice, provider);
    if(!fpDevice)
    {
        ALERT(0, 0, "start - provider invalid");
        stop(provider);
        return false;
    }

	// Let's see if we have any configurations to play with
		
    configs = fpDevice->GetNumConfigurations();
    if (configs < 1)
    {
        ALERT(0, 0, "start - no configurations");
        stop(provider);
        return false;
    }
	
	// Now take control of the device and configure it
		
    if (!fpDevice->open(this))
    {
        ALERT(0, 0, "start - unable to open device");
        stop(provider);
        return false;
    }
    
        // get workloop
        
    fWorkLoop = getWorkLoop();
    if (!fWorkLoop)
    {
        ALERT(0, 0, "start - getWorkLoop failed");
        fpDevice->close(this);
        fpDevice = NULL;
        stop(provider);
        return false;
    }
    
    fWorkLoop->retain();
    
    fCommandGate = IOCommandGate::commandGate(this);
    if (!fCommandGate)
    {
        ALERT(0, 0, "start - commandGate failed");
        fpDevice->close(this);
        fpDevice = NULL;
        stop(provider);
        return false;
    }
    
    if (fWorkLoop->addEventSource(fCommandGate) != kIOReturnSuccess)
    {
        ALERT(0, 0, "start - addEventSource(commandGate) failed");
        fpDevice->close(this);
        fpDevice = NULL;
        stop(provider);
        return false;
    }

    fCommandGate->enable();
	
    if (!configureDevice(configs))
    {
        ALERT(0, 0, "start - configureDevice failed");
        fpDevice->close(this);
        fpDevice = NULL;
        stop(provider);
        return false;
    }
    
    XTRACE(this, 0, 0, "start - successful");
    
    return true;
    	
}/* end start */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::stop
//
//		Inputs:		provider - my provider
//
//		Outputs:	
//
//		Desc:		Stops the driver
//
/****************************************************************************************************/

void AppleUSBCDCEthernet::stop(IOService *provider)
{
    
    XTRACE(this, 0, 0, "stop");
    
    if (fNetworkInterface)
    {
        fNetworkInterface->release();
        fNetworkInterface = NULL;
    }

    if (fCommInterface)	
    {
	fCommInterface->close(this);	
	fCommInterface->release();
	fCommInterface = NULL;	
    }
	
    if (fDataInterface)	
    { 
        fDataInterface->close(this);	
        fDataInterface->release();
        fDataInterface = NULL;	
    }

    if (fpDevice)
    {
        fpDevice->close(this);
        fpDevice = NULL;
    }
    
    if (fMediumDict)
    {
        fMediumDict->release();
        fMediumDict = NULL;
    }
    
    super::stop(provider);
    
    return;
	
}/* end stop */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::configureDevice
//
//		Inputs:		numConfigs - number of configurations present
//
//		Outputs:	return Code - true (device configured), false (device not configured)
//
//		Desc:		Finds the configurations and then the appropriate interfaces etc.
//
/****************************************************************************************************/

bool AppleUSBCDCEthernet::configureDevice(UInt8 numConfigs)
{
    IOUSBFindInterfaceRequest		req;
    const IOUSBInterfaceDescriptor	*altInterfaceDesc;
    IOReturn				ior = kIOReturnSuccess;
    UInt16				numends;
    UInt16				alt;
    bool				goodCall;
       
    XTRACE(this, 0, numConfigs, "configureDevice");
    	
        // Initialize and "configure" the device
        
    if (!initDevice(numConfigs))
    {
        XTRACE(this, 0, 0, "configureDevice - initDevice failed");
        return false;
    }

        // Get the Comm. Class interface

    req.bInterfaceClass	= kUSBCommClass;
    req.bInterfaceSubClass = kUSBEthernetControlModel;
    req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
    
    fCommInterface = fpDevice->FindNextInterface(NULL, &req);
    if (!fCommInterface)
    {
        XTRACE(this, 0, 0, "configureDevice - Finding the first CDC interface failed");
        return false;
    }
    
    if (!getFunctionalDescriptors())
    {
        XTRACE(this, 0, 0, "configureDevice - getFunctionalDescriptors failed");
        fCommInterface = NULL;					// The interface is of no use to us
        return false;
    }

    goodCall = fCommInterface->open(this);
    if (!goodCall)
    {
        XTRACE(this, 0, 0, "configureDevice - open comm interface failed.");
        fCommInterface = NULL;
        return false;
    }
    
    fCommInterfaceNumber = fCommInterface->GetInterfaceNumber();
    
        // Now get the Data Class interface
        
    req.bInterfaceClass = kUSBDataClass;
    req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
    req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
    
    fDataInterface = fpDevice->FindNextInterface(NULL, &req);    
    if (fDataInterface)
    {
        numends = fDataInterface->GetNumEndpoints();
        if (numends > 1)					// There must be (at least) two bulk endpoints
        {
            XTRACE(this, numends, fDataInterface, "configureDevice - Data Class interface found");
        } else {
            altInterfaceDesc = fDataInterface->FindNextAltInterface(NULL, &req);
            if (!altInterfaceDesc)
            {
                XTRACE(this, 0, 0, "configureDevice - FindNextAltInterface failed");
            }
            while (altInterfaceDesc)
            {
                numends = altInterfaceDesc->bNumEndpoints;
                if (numends > 1)
                {
                    goodCall = fDataInterface->open(this);
                    if (goodCall)
                    {
                        alt = altInterfaceDesc->bAlternateSetting;
                        XTRACE(this, numends, alt, "configureDevice - Data Class interface (alternate) found");
                        ior = fDataInterface->SetAlternateInterface(this, alt);
                        if (ior == kIOReturnSuccess)
                        {
                            XTRACE(this, 0, 0, "configureDevice - Alternate set");
                            break;
                        } else {
                            XTRACE(this, 0, 0, "configureDevice - SetAlternateInterface failed");
                            numends = 0;
                        }
                    } else {
                        XTRACE(this, 0, 0, "configureDevice - open data interface failed.");
                        numends = 0;
                    }
                } else {
                    XTRACE(this, 0, altInterfaceDesc, "configureDevice - No endpoints this alternate");
                }
                altInterfaceDesc = fDataInterface->FindNextAltInterface(altInterfaceDesc, &req);
            }
        }
    } else {
        XTRACE(this, 0, 0, "configureDevice - FindNextInterface failed");
    }

    if (numends < 2)
    {
        XTRACE(this, 0, 0, "configureDevice - Finding a Data Class interface failed");
        fCommInterface->close(this);
        fCommInterface = NULL;
        return false;
    } else {
    
        fCommInterface->retain();
        fDataInterface->retain();

            // Found both so now let's publish the interface
	
        if (!createNetworkInterface())
        {
            XTRACE(this, 0, 0, "configureDevice - createNetworkInterface failed");
            fCommInterface->release();
            fCommInterface->close(this);
            fCommInterface = NULL;
            fDataInterface->release();
            fDataInterface->close(this);
            fDataInterface = NULL;
            return false;
        }
    }
    
    if (fbmAttributes & kUSBAtrBusPowered)
    {
        ior = fpDevice->SuspendDevice(true);         // Suspend the device (if supported and bus powered)
        if (ior)
        {
            XTRACE(this, 0, ior, "configureDevice - SuspendDevice error");
        }
    }

    return true;

}/* end configureDevice */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::initDevice
//
//		Inputs:		numConfigs - number of configurations present
//
//		Outputs:	return Code - true (CDC present), false (CDC not present)
//
//		Desc:		Determines if this is a CDC compliant device and then sets the configuration
//
/****************************************************************************************************/

bool AppleUSBCDCEthernet::initDevice(UInt8 numConfigs)
{
    IOUSBFindInterfaceRequest		req;
    const IOUSBConfigurationDescriptor	*cd = NULL;
    IOUSBInterfaceDescriptor 		*intf = NULL;
    IOReturn				ior = kIOReturnSuccess;
    UInt8				cval;
    UInt8				config = 0;
    bool				goodconfig = false;
       
    XTRACE(this, 0, numConfigs, "initDevice");
    	
        // Make sure we have a CDC interface to play with
        
    for (cval=0; cval<numConfigs; cval++)
    {
    	XTRACE(this, 0, cval, "initDevice - Checking Configuration");
		
     	cd = fpDevice->GetFullConfigurationDescriptor(cval);
     	if (!cd)
    	{
            XTRACE(this, 0, 0, "initDevice - Error getting the full configuration descriptor");
        } else {
            req.bInterfaceClass	= kUSBCommClass;
            req.bInterfaceSubClass = kUSBEthernetControlModel;
            req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
            req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
            ior = fpDevice->FindNextInterfaceDescriptor(cd, intf, &req, &intf);
            if (ior == kIOReturnSuccess)
            {
                if (intf)
                {
                    XTRACE(this, 0, config, "initDevice - Interface descriptor found");
                    config = cd->bConfigurationValue;
                    goodconfig = true;					// We have at least one CDC interface in this configuration
                    break;
                } else {
                    XTRACE(this, 0, config, "initDevice - That's weird the interface was null");
                }
            } else {
                XTRACE(this, ior, cval, "initDevice - No CDC interface found this configuration");
            }
        }
    }
        
    if (goodconfig)
    {
        ior = fpDevice->SetConfiguration(this, config);
        if (ior != kIOReturnSuccess)
        {
            XTRACE(this, 0, ior, "initDevice - SetConfiguration error");
            return false;			
        }
    } else {
        return false;
    }
    
    fbmAttributes = cd->bmAttributes;
    XTRACE(this, fbmAttributes, kUSBAtrRemoteWakeup, "initDevice - Configuration bmAttributes");
    
        // Save the ID's
    
    fVendorID = fpDevice->GetVendorID();
    fProductID = fpDevice->GetProductID();
    
    return goodconfig;

}/* end initDevice */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::getFunctionalDescriptors
//
//		Inputs:		
//
//		Outputs:	return - true (descriptors ok), false (somethings not right or not supported)	
//
//		Desc:		Finds all the functional descriptors for the specific interface
//
/****************************************************************************************************/

bool AppleUSBCDCEthernet::getFunctionalDescriptors()
{
    bool				gotDescriptors = false;
    bool				configok = true;
    bool				enet = false;
    IOReturn				ior;
    const HeaderFunctionalDescriptor 	*funcDesc = NULL;
    EnetFunctionalDescriptor		*ENETFDesc;
    UInt8				serString;
    char 				ascii_mac[14];
    int 				i;
       
    XTRACE(this, 0, 0, "getFunctionalDescriptors");
        
    do
    {
        (IOUSBDescriptorHeader*)funcDesc = fCommInterface->FindNextAssociatedDescriptor((void*)funcDesc, CS_INTERFACE);
        if (!funcDesc)
        {
            gotDescriptors = true;
        } else {
            switch (funcDesc->bDescriptorSubtype)
            {
                case Header_FunctionalDescriptor:
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - Header Functional Descriptor");
                    break;
                case Enet_Functional_Descriptor:
                    (const HeaderFunctionalDescriptor *)ENETFDesc = funcDesc;
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - Ethernet Functional Descriptor");
                    enet = true;
                    break;
                case Union_FunctionalDescriptor:
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - Union Functional Descriptor");
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
        
        serString = fpDevice->GetSerialNumberStringIndex();	// Default to the serial number string
        ior = fpDevice->GetStringDescriptor(serString, (char *)&ascii_mac, 13);
        if (ior == kIOReturnSuccess)
        {
            for (i = 0; i < 6; i++)
            {
                fEaddr[i] = (Asciihex_to_binary(ascii_mac[i*2]) << 4) | Asciihex_to_binary(ascii_mac[i*2+1]);
            }
#if LOG_DATA
            XTRACE(this, 0, serString, "getFunctionalDescriptors - Ethernet address (serial string)");
#endif
            LogData(kDataOther, 6, fEaddr, this);
        } else {
                XTRACE(this, 0, 0, "getFunctionalDescriptors - Error retrieving Ethernet address (serial string)");
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
            ior = fpDevice->GetStringDescriptor(ENETFDesc->iMACAddress, (char *)&ascii_mac, 13);
            if (ior == kIOReturnSuccess)
            {
                for (i = 0; i < 6; i++)
                {
                    fEaddr[i] = (Asciihex_to_binary(ascii_mac[i*2]) << 4) | Asciihex_to_binary(ascii_mac[i*2+1]);
                }
#if LOG_DATA
                XTRACE(this, 0, ENETFDesc->iMACAddress, "getFunctionalDescriptors - Ethernet address");
#endif
                LogData(kDataOther, 6, fEaddr, this);
                
                fMax_Block_Size = USBToHostWord(ENETFDesc->wMaxSegmentSize);
                XTRACE(this, 0, fMax_Block_Size, "getFunctionalDescriptors - Maximum segment size");
            } else {
                XTRACE(this, 0, 0, "getFunctionalDescriptors - Error retrieving Ethernet address");
                configok = false;
            }
        } else {
            configok = false;
        }
    }
    
    return configok;
    
}/* end getFunctionalDescriptors */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::createNetworkInterface
//
//		Inputs:		
//
//		Outputs:	return Code - true (created and initialilzed ok), false (it failed)
//
//		Desc:		Creates and initializes the network interface
//
/****************************************************************************************************/

bool AppleUSBCDCEthernet::createNetworkInterface()
{
	
    XTRACE(this, 0, 0, "createNetworkInterface");
    
            // Allocate memory for buffers etc

    fTransmitQueue = (IOGatedOutputQueue *)getOutputQueue();
    if (!fTransmitQueue) 
    {
        ALERT(0, 0, "createNetworkInterface - Output queue initialization failed");
        return false;
    }
    fTransmitQueue->retain();
    
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
    
    if (!attachInterface((IONetworkInterface **)&fNetworkInterface, true))
    {	
        ALERT(0, 0, "createNetworkInterface - attachInterface failed");      
        return false;
    }
    
        // Ready to service interface requests
    
    fNetworkInterface->registerService();
    
    XTRACE(this, 0, 0, "createNetworkInterface - Exiting, successful");

    return true;
	
}/* end createNetworkInterface */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::enable
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

IOReturn AppleUSBCDCEthernet::enable(IONetworkInterface *netif)
{
    IONetworkMedium	*medium;
    IOMediumType    	mediumType = kIOMediumEthernet10BaseT | kIOMediumOptionFullDuplex;
    
    XTRACE(this, 0, netif, "enable");

        // If an interface client has previously enabled us,
        // and we know there can only be one interface client
        // for this driver, then simply return success.

    if (fNetifEnabled)
    {
        XTRACE(this, 0, 0, "enable - already enabled");
        return kIOReturnSuccess;
    }
    
    if ((fReady == false) && !wakeUp())
    {
        XTRACE(this, 0, fReady, "enable - not ready or wakeUp failed");
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
    
    USBSetPacketFilter();
    XTRACE(this, 0, 0, "enable - packet filter applied");

    return kIOReturnSuccess;
    
}/* end enable */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::disable
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
 
IOReturn AppleUSBCDCEthernet::disable(IONetworkInterface *netif)
{

    XTRACE(this, 0, 0, "disable");

        // Disable our IOOutputQueue object. This will prevent the
        // outputPacket() method from being called
        
    fTransmitQueue->stop();

        // Flush all packets currently in the output queue

    fTransmitQueue->setCapacity(0);
    fTransmitQueue->flush();

    putToSleep();

    fNetifEnabled = false;
    fReady = false;

    return kIOReturnSuccess;
    
}/* end disable */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::setWakeOnMagicPacket
//
//		Inputs:		active - true(wake), false(don't)
//
//		Outputs:	Return code - kIOReturnSuccess
//
//		Desc:		Set for wake on magic packet
//
/****************************************************************************************************/

IOReturn AppleUSBCDCEthernet::setWakeOnMagicPacket(bool active)
{
    IOUSBDevRequest	devreq;
    IOReturn		ior = kIOReturnSuccess;

    XTRACE(this, 0, active, "setWakeOnMagicPacket");
	
    fWOL = active;
    
    if (fbmAttributes & kUSBAtrRemoteWakeup)
    {
    
            // Clear the feature if wake-on-lan is not set (SetConfiguration sets the feature 
            // automatically if the device supports remote wake up)
    
        if (!active)				
        {
            devreq.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBStandard, kUSBDevice);
            devreq.bRequest = kUSBRqClearFeature;
            devreq.wValue = kUSBFeatureDeviceRemoteWakeup;
            devreq.wIndex = 0;
            devreq.wLength = 0;
            devreq.pData = 0;

            ior = fpDevice->DeviceRequest(&devreq);
            if (ior == kIOReturnSuccess)
            {
                XTRACE(this, 0, ior, "setWakeOnMagicPacket - Clearing remote wake up feature successful");
            } else {
                XTRACE(this, 0, ior, "setWakeOnMagicPacket - Clearing remote wake up feature failed");
            }
        }
    } else {
        XTRACE(this, 0, 0, "setWakeOnMagicPacket - Remote wake up not supported");
    }

    
    return kIOReturnSuccess;
    
}/* end setWakeOnMagicPacket */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::getPacketFilters
//
//		Inputs:		group - the filter group
//
//		Outputs:	Return code - kIOReturnSuccess and others
//				filters - the capability
//
//		Desc:		Set the filter capability for the driver
//
/****************************************************************************************************/

IOReturn AppleUSBCDCEthernet::getPacketFilters(const OSSymbol *group, UInt32 *filters) const
{
    IOReturn	rtn = kIOReturnSuccess;
    
    XTRACE(this, group, filters, "getPacketFilters");

    if (group == gIOEthernetWakeOnLANFilterGroup)
    {
        if (fbmAttributes & kUSBAtrRemoteWakeup)
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
//		Method:		AppleUSBCDCEthernet::selectMedium
//
//		Inputs:
//
//		Outputs:
//
//		Desc:		Lets us know if someone is playing with ifconfig
//
/****************************************************************************************************/

IOReturn AppleUSBCDCEthernet::selectMedium(const IONetworkMedium *medium)
{
    
    XTRACE(this, 0, 0, "selectMedium");

    setSelectedMedium(medium);
    
    return kIOReturnSuccess;
        
}/* end selectMedium */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::getHardwareAddress
//
//		Inputs:		
//
//		Outputs:	Return code - kIOReturnSuccess or kIOReturnError
//				ea - the address
//
//		Desc:		Get the ethernet address from the hardware (actually the descriptor)
//
/****************************************************************************************************/

IOReturn AppleUSBCDCEthernet::getHardwareAddress(IOEthernetAddress *ea)
{
    UInt32      i;

    XTRACE(this, 0, 0, "getHardwareAddress");
     
    for (i=0; i<6; i++)
    {
        ea->bytes[i] = fEaddr[i];
    }

    return kIOReturnSuccess;
    
}/* end getHardwareAddress */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::newVendorString
//
//		Inputs:		
//
//		Outputs:	Return code - the vendor string
//
//		Desc:		Identifies the hardware vendor
//
/****************************************************************************************************/

const OSString* AppleUSBCDCEthernet::newVendorString() const
{

    XTRACE(this, 0, 0, "newVendorString");
    
    return OSString::withCString((const char *)defaultName);		// Maybe we should use the descriptors

}/* end newVendorString */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::newModelString
//
//		Inputs:		
//
//		Outputs:	Return code - the model string
//
//		Desc:		Identifies the hardware model
//
/****************************************************************************************************/

const OSString* AppleUSBCDCEthernet::newModelString() const
{

    XTRACE(this, 0, 0, "newModelString");
    
    return OSString::withCString("USB");		// Maybe we should use the descriptors
    
}/* end newModelString */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::newRevisionString
//
//		Inputs:		
//
//		Outputs:	Return code - the revision string
//
//		Desc:		Identifies the hardware revision
//
/****************************************************************************************************/

const OSString* AppleUSBCDCEthernet::newRevisionString() const
{

    XTRACE(this, 0, 0, "newRevisionString");
    
    return OSString::withCString("");
    
}/* end newRevisionString */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::setMulticastMode
//
//		Inputs:		active - true (set it), false (don't)
//
//		Outputs:	Return code - kIOReturnSuccess
//
//		Desc:		Sets multicast mode
//
/****************************************************************************************************/

IOReturn AppleUSBCDCEthernet::setMulticastMode(bool active)
{

    XTRACE(this, 0, active, "setMulticastMode");

    if (active)
    {
        fPacketFilter |= kPACKET_TYPE_ALL_MULTICAST;
    } else {
        fPacketFilter &= ~kPACKET_TYPE_ALL_MULTICAST;
    }
    
    USBSetPacketFilter();
    
    return kIOReturnSuccess;
    
}/* end setMulticastMode */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::setMulticastList
//
//		Inputs:		addrs - list of addresses
//				count - number in the list
//
//		Outputs:	Return code - kIOReturnSuccess or kIOReturnIOError
//
//		Desc:		Sets multicast list
//
/****************************************************************************************************/

IOReturn AppleUSBCDCEthernet::setMulticastList(IOEthernetAddress *addrs, UInt32 count)
{
    bool	uStat;
    
    XTRACE(this, addrs, count, "setMulticastList");
    
    if (count != 0)
    {
        uStat = USBSetMulticastFilter(addrs, count);
        if (!uStat)
        {
            return kIOReturnIOError;
        }
    }

    return kIOReturnSuccess;
    
}/* end setMulticastList */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::setPromiscuousMode
//
//		Inputs:		active - true (set it), false (don't)
//
//		Outputs:	Return code - kIOReturnSuccess
//
//		Desc:		Sets promiscuous mode
//
/****************************************************************************************************/

IOReturn AppleUSBCDCEthernet::setPromiscuousMode(bool active)
{
    
    XTRACE(this, 0, active, "setPromiscuousMode");

    if (active)
    {
        fPacketFilter |= kPACKET_TYPE_PROMISCUOUS;
    } else {
        fPacketFilter &= ~kPACKET_TYPE_PROMISCUOUS;
    }
    
    USBSetPacketFilter();
        
    return kIOReturnSuccess;
    
}/* end setPromiscuousMode */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::createOutputQueue
//
//		Inputs:		
//
//		Outputs:	Return code - the output queue
//
//		Desc:		Creates the output queue
//
/****************************************************************************************************/

IOOutputQueue* AppleUSBCDCEthernet::createOutputQueue()
{

    XTRACE(this, 0, 0, "createOutputQueue");
    
    return IOBasicOutputQueue::withTarget(this, TRANSMIT_QUEUE_SIZE);
    
}/* end createOutputQueue */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::outputPacket
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

UInt32 AppleUSBCDCEthernet::outputPacket(struct mbuf *pkt, void *param)
{
    UInt32	ior = kIOReturnSuccess;
    
    XTRACE(this, pkt, 0, "outputPacket");

    if (!fLinkStatus)
    {
        XTRACE(this, pkt, fLinkStatus, "outputPacket - link is down");
        if (fOutputErrsOK)
            fpNetStats->outputErrors++;
        freePacket(pkt);
        return kIOReturnOutputDropped;
    }
    
    retain();
    ior = fCommandGate->runAction(USBTransmitPacketAction, (void *)pkt);
    release();
    
    if (ior != kIOReturnSuccess)
    {
        return kIOReturnOutputStall;
    }

    return kIOReturnOutputSuccess;
    
}/* end outputPacket */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::configureInterface
//
//		Inputs:		netif - the interface being configured
//
//		Outputs:	Return code - true (configured ok), false (not)
//
//		Desc:		Finish the network interface configuration
//
/****************************************************************************************************/

bool AppleUSBCDCEthernet::configureInterface(IONetworkInterface *netif)
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

        // Get the Ethernet statistics structure:

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
//		Method:		AppleUSBCDCEthernet::wakeUp
//
//		Inputs:		
//
//		Outputs:	Return Code - true(we're awake), false(failed)
//
//		Desc:		Resumes the device it it was suspended and then gets all the data
//				structures sorted out and all the pipes ready.
//
/****************************************************************************************************/

bool AppleUSBCDCEthernet::wakeUp()
{
    IOReturn 	rtn = kIOReturnSuccess;

    XTRACE(this, 0, 0, "wakeUp");
    
    fReady = false;
    
    if (fTimerSource)
    { 
        fTimerSource->cancelTimeout();
    }
    
    setLinkStatus(0, 0);				// Initialize the link state
    
    if (fbmAttributes & kUSBAtrBusPowered)
    {
        rtn = fpDevice->SuspendDevice(false);		// Resume the device
        if (rtn != kIOReturnSuccess)
        {
            return false;
        }
    }
    
    IOSleep(50);
    
    if (!allocateResources()) 
    {
    	return false;
    }
		
        // Read the comm interrupt pipe for status (if we have one):
		
    fCommCompletionInfo.target = this;
    fCommCompletionInfo.action = commReadComplete;
    fCommCompletionInfo.parameter = NULL;
    
    if (fCommPipe)
    {
        rtn = fCommPipe->Read(fCommPipeMDP, &fCommCompletionInfo, NULL);
    }
    if (rtn == kIOReturnSuccess)
    {
        	// Read the data-in bulk pipe:
			
        fReadCompletionInfo.target = this;
        fReadCompletionInfo.action = dataReadComplete;
        fReadCompletionInfo.parameter = NULL;
		
        rtn = fInPipe->Read(fPipeInMDP, &fReadCompletionInfo, NULL);
			
        if (rtn == kIOReturnSuccess)
        {
        	// Set up the data-out bulk pipe:
			
            fWriteCompletionInfo.target	= this;
            fWriteCompletionInfo.action	= dataWriteComplete;
            fWriteCompletionInfo.parameter = NULL;				// for now, filled in with the mbuf address when sent
		
                // Set up the management element request completion routine:

            fMERCompletionInfo.target = this;
            fMERCompletionInfo.action = merWriteComplete;
            fMERCompletionInfo.parameter = NULL;				// for now, filled in with parm block when allocated
            
                // Set up the statistics request completion routine:

            fStatsCompletionInfo.target = this;
            fStatsCompletionInfo.action = statsWriteComplete;
            fStatsCompletionInfo.parameter = NULL;				// for now, filled in with parm block when allocated
        }
    }

    if (rtn != kIOReturnSuccess)
    {
    
    	// We failed for some reason
	
        ALERT(0, 0, "wakeUp - Setting up the pipes failed");
        releaseResources();
        return false;
    } else {
        if (!fMediumDict)
        {
            if (!createMediumTables())
            {
                ALERT(0, 0, "wakeUp - createMediumTables failed");
                releaseResources();    
                return false;
            }
        }

        fTimerSource->setTimeoutMS(WATCHDOG_TIMER_MS);
        fReady = true;
    }

    return true;
	
}/* end wakeUp */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::putToSleep
//
//		Inputs:		
//
//		Outputs:	Return Code - true(we're asleep), false(failed)
//
//		Desc:		Do clean up and suspend the device.
//
/****************************************************************************************************/

void AppleUSBCDCEthernet::putToSleep()
{
    IOReturn	ior;

    XTRACE(this, 0, 0, "putToSleep");
        
    fReady = false;

    if (fTimerSource)
    { 
        fTimerSource->cancelTimeout();
    }

    setLinkStatus(0, 0);
	
        // Release all resources
		
    releaseResources();
    
    if (!fTerminate)
    {
        if (fbmAttributes & kUSBAtrBusPowered)
        {
            ior = fpDevice->SuspendDevice(true);         // Suspend the device again (if supported and not unplugged)
            if (ior)
            {
                XTRACE(this, 0, ior, "releasePort - SuspendDevice error");
            }
        }
    }

    if ((fTerminate) && (!fReady))		// if it's the result of a terminate and no interfaces enabled we also need to close the device
    {
    	fpDevice->close(this);
        fpDevice = NULL;
    }
    
}/* end putToSleep */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::createMediumTables
//
//		Inputs:		
//
//		Outputs:	Return code - true (tables created), false (not created)
//
//		Desc:		Creates the medium tables
//
/****************************************************************************************************/

bool AppleUSBCDCEthernet::createMediumTables()
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
//		Method:		AppleUSBCDCEthernet::allocateResources
//
//		Inputs:		
//
//		Outputs:	return code - true (allocate was successful), false (it failed)
//
//		Desc:		Finishes up the rest of the configuration and gets all the endpoints open etc.
//
/****************************************************************************************************/

bool AppleUSBCDCEthernet::allocateResources()
{
    IOUSBFindEndpointRequest	epReq;		// endPoint request struct on stack
    UInt32			i;

    XTRACE(this, 0, 0, "allocateResources.");

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

        // Interrupt pipe - Comm Interface

    epReq.type = kUSBInterrupt;
    epReq.direction = kUSBIn;
    fCommPipe = fCommInterface->FindNextPipe(0, &epReq);
    if (!fCommPipe)
    {
        XTRACE(this, 0, 0, "allocateResources - no interrupt in pipe.");
        fCommPipeMDP = NULL;
        fCommPipeBuffer = NULL;
        fLinkStatus = 1;					// Mark it active cause we'll never get told
    } else {
        XTRACE(this, epReq.maxPacketSize << 16 |epReq.interval, fCommPipe, "allocateResources - comm pipe.");

            // Allocate Memory Descriptor Pointer with memory for the Comm pipe:

        fCommPipeMDP = IOBufferMemoryDescriptor::withCapacity(COMM_BUFF_SIZE, kIODirectionIn);
        if (!fCommPipeMDP)
            return false;
		
        fCommPipeMDP->setLength(COMM_BUFF_SIZE);
        fCommPipeBuffer = (UInt8*)fCommPipeMDP->getBytesNoCopy();
        XTRACE(this, 0, fCommPipeBuffer, "allocateResources - comm buffer");
    }

        // Allocate Memory Descriptor Pointer with memory for the data-in bulk pipe:

    fPipeInMDP = IOBufferMemoryDescriptor::withCapacity(fMax_Block_Size, kIODirectionIn);
    if (!fPipeInMDP)
        return false;
		
    fPipeInMDP->setLength(fMax_Block_Size);
    fPipeInBuffer = (UInt8*)fPipeInMDP->getBytesNoCopy();
    XTRACE(this, 0, fPipeInBuffer, "allocateResources - input buffer");
    
        // Allocate Memory Descriptor Pointers with memory for the data-out bulk pipe pool

    for (i=0; i<kOutBufPool; i++)
    {
        fPipeOutBuff[i].pipeOutMDP = IOBufferMemoryDescriptor::withCapacity(fMax_Block_Size, kIODirectionOut);
        if (!fPipeOutBuff[i].pipeOutMDP)
        {
            XTRACE(this, 0, i, "allocateResources - Allocate output descriptor failed");
            return false;
        }
		
        fPipeOutBuff[i].pipeOutMDP->setLength(fMax_Block_Size);
        fPipeOutBuff[i].pipeOutBuffer = (UInt8*)fPipeOutBuff[i].pipeOutMDP->getBytesNoCopy();
        XTRACE(this, fPipeOutBuff[i].pipeOutMDP, fPipeOutBuff[i].pipeOutBuffer, "allocateResources - output buffer");
        fPipeOutBuff[i].avail = true;
    }
		
    return true;
	
}/* end allocateResources */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::releaseResources
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Frees up the resources allocated in allocateResources
//
/****************************************************************************************************/

void AppleUSBCDCEthernet::releaseResources()
{
    UInt32	i;
    
    XTRACE(this, 0, 0, "releaseResources");

    for (i=0; i<kOutBufPool; i++)
    {
        if (fPipeOutBuff[i].pipeOutMDP)	
        { 
            fPipeOutBuff[i].pipeOutMDP->release();	
            fPipeOutBuff[i].pipeOutMDP = NULL;
            fPipeOutBuff[i].avail = false;
        }
    }
	
    if (fPipeInMDP)	
    { 
        fPipeInMDP->release();	
        fPipeInMDP = 0; 
    }
	
    if (fCommPipeMDP)	
    { 
        fCommPipeMDP->release();	
        fCommPipeMDP = 0; 
    }
	
}/* end releaseResources */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::USBTransmitPacketAction
//
//		Desc:		Dummy pass through for USBTransmitPacket.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCEthernet::USBTransmitPacketAction(OSObject *owner, void *arg0, void *, void *, void *)
{

    return ((AppleUSBCDCEthernet *)owner)->USBTransmitPacket((mbuf *)arg0);
    
}/* end USBTransmitPacketAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::USBTransmitPacket
//
//		Inputs:		packet - the packet
//
//		Outputs:	Return code - true (transmit started), false (it didn't)
//
//		Desc:		Set up and then transmit the packet. 
//				This metod is serialized (on the command gate) with respect
//				to the completion routine to protect the buffer pool
//
/****************************************************************************************************/

IOReturn AppleUSBCDCEthernet::USBTransmitPacket(struct mbuf *packet)
{
    UInt32		numbufs;			// number of mbufs for this packet
    struct mbuf		*m;				// current mbuf
    UInt32		total_pkt_length = 0;
    UInt32		rTotal = 0;
    IOReturn		ior = kIOReturnSuccess;
    UInt32		poolIndx;
    bool		gotBuffer = false;
    UInt16		tryCount = 0;
	
    XTRACE(this, 0, packet, "USBTransmitPacket");
			
	// Count the number of mbufs in this packet
        
    m = packet;
    while (m)
    {
        total_pkt_length += m->m_len;
        numbufs++;
	m = m->m_next;
    }
    
    XTRACE(this, total_pkt_length, numbufs, "USBTransmitPacket - Total packet length and Number of mbufs");
    
    if (total_pkt_length > fMax_Block_Size)
    {
        XTRACE(this, 0, 0, "USBTransmitPacket - Bad packet size");	// Note for now and revisit later
        if (fOutputErrsOK)
            fpNetStats->outputErrors++;
        return kIOReturnInternalError;
    }
    
            // Find an ouput buffer in the pool
    
    while (!gotBuffer)
    {
        for (poolIndx=0; poolIndx<kOutBufPool; poolIndx++)
        {
            if (fPipeOutBuff[poolIndx].avail)
            {
                gotBuffer = true;
                break;
            }
        }
        if (gotBuffer)
        {
            break;
        } else {
            tryCount++;
            if (tryCount > kOutBuffThreshold)
            {
                XTRACE(this, 0, 0, "USBTransmitPacket - Exceeded output buffer wait threshold");
                if (fOutputErrsOK)
                    fpNetStats->outputErrors++;
                return kIOReturnInternalError;
            } else {
                XTRACE(this, 0, tryCount, "USBTransmitPacket - Waiting for output buffer");
                IOSleep(1);
            }
        }
    }

        // Start filling in the send buffer

    m = packet;							// start with the first mbuf of the packet
    rTotal = 0;							// running total				
    do
    {  
        if (m->m_len == 0)					// Ignore zero length mbufs
            continue;
        
        bcopy(mtod(m, unsigned char *), &fPipeOutBuff[poolIndx].pipeOutBuffer[rTotal], m->m_len);
        rTotal += m->m_len;
        
    } while ((m = m->m_next) != 0);
    
    LogData(kDataOut, rTotal, fPipeOutBuff[poolIndx].pipeOutBuffer, this);
	
    fPipeOutBuff[poolIndx].m = packet;
    fWriteCompletionInfo.parameter = (void *)poolIndx;
    fPipeOutBuff[poolIndx].pipeOutMDP->setLength(rTotal);
    ior = fOutPipe->Write(fPipeOutBuff[poolIndx].pipeOutMDP, &fWriteCompletionInfo);
    if (ior != kIOReturnSuccess)
    {
        XTRACE(this, 0, ior, "USBTransmitPacket - Write failed");
        if (ior == kIOUSBPipeStalled)
        {
            fOutPipe->Reset();
            ior = fOutPipe->Write(fPipeOutBuff[poolIndx].pipeOutMDP, &fWriteCompletionInfo);
            if (ior != kIOReturnSuccess)
            {
                XTRACE(this, 0, ior, "USBTransmitPacket - Write really failed");
                if (fOutputErrsOK)
                    fpNetStats->outputErrors++;
                return ior;
            }
        }
    }
    if (fOutputPktsOK)		
        fpNetStats->outputPackets++;
    
    return ior;

}/* end USBTransmitPacket */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::USBSetMulticastFilter
//
//		Inputs:		addrs - the list of addresses
//				count - How many
//
//		Outputs:	
//
//		Desc:		Set up and send SetMulticastFilter Management Element Request(MER).
//
/****************************************************************************************************/

bool AppleUSBCDCEthernet::USBSetMulticastFilter(IOEthernetAddress *addrs, UInt32 count)
{
    IOReturn		rc;
    IOUSBDevRequest	*MER;
    UInt8		*eaddrs;
    UInt32		eaddLen;
    UInt32		i,j,rnum;
	
    XTRACE(this, fMcFilters, count, "USBSetMulticastFilter");

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
	
    rc = fpDevice->DeviceRequest(MER, &fMERCompletionInfo);
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
//		Method:		AppleUSBCDCEthernet::USBSetPacketFilter
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Set up and send SetEthernetPackettFilters Management Element Request(MER).
//
/****************************************************************************************************/

bool AppleUSBCDCEthernet::USBSetPacketFilter()
{
    IOReturn		rc;
    IOUSBDevRequest	*MER;
	
    XTRACE(this, 0, fPacketFilter, "USBSetPacketFilter");
	
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
	
    rc = fpDevice->DeviceRequest(MER, &fMERCompletionInfo);
    if (rc != kIOReturnSuccess)
    {
        XTRACE(this, MER->bRequest, rc, "USBSetPacketFilter - DeviceRequest error");
        if (rc == kIOUSBPipeStalled)
        {

            // Clear the stall and try it once more
        
            fpDevice->GetPipeZero()->ClearPipeStall(false);
            rc = fpDevice->DeviceRequest(MER, &fMERCompletionInfo);
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
//		Method:		AppleUSBCDCEthernet::clearPipeStall
//
//		Inputs:		thePipe - the pipe
//
//		Outputs:	
//
//		Desc:		Clear a stall on the specified pipe. All outstanding I/O
//				is returned as aborted.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCEthernet::clearPipeStall(IOUSBPipe *thePipe)
{
    UInt8	pipeStatus;
    IOReturn 	rtn = kIOReturnSuccess;
    
    XTRACE(this, 0, thePipe, "clearPipeStall");
    
    pipeStatus = thePipe->GetStatus();
    if (pipeStatus == kPipeStalled)
    {
        rtn = thePipe->ClearPipeStall(true);
        if (rtn == kIOReturnSuccess)
        {
            XTRACE(this, 0, 0, "clearPipeStall - Successful");
        } else {
            XTRACE(this, 0, rtn, "clearPipeStall - Failed");
        }
    } else {
        XTRACE(this, 0, pipeStatus, "clearPipeStall - Pipe not stalled");
    }
    
    return rtn;

}/* end clearPipeStall */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::receivePacket
//
//		Inputs:		packet - the packet
//				size - Number of bytes in the packet
//
//		Outputs:	
//
//		Desc:		Build the mbufs and then send to the network stack.
//
/****************************************************************************************************/

void AppleUSBCDCEthernet::receivePacket(UInt8 *packet, UInt32 size)
{
    struct mbuf		*m;
    UInt32		submit;
    
    XTRACE(this, 0, size, "receivePacket");
    
    if (size > fMax_Block_Size)
    {
        XTRACE(this, 0, 0, "receivePacket - Packet size error, packet dropped");
        if (fInputErrsOK)
            fpNetStats->inputErrors++;
        return;
    }
    
    m = allocatePacket(size);
    if (m)
    {
        bcopy(packet, mtod(m, unsigned char *), size);
        submit = fNetworkInterface->inputPacket(m, size);
        XTRACE(this, 0, submit, "receivePacket - Packets submitted");
        if (fInputPktsOK)
            fpNetStats->inputPackets++;
    } else {
        XTRACE(this, 0, 0, "receivePacket - Buffer allocation failed, packet dropped");
        if (fInputErrsOK)
            fpNetStats->inputErrors++;
    }

}/* end receivePacket */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::timeoutFired
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Static member function called when a timer event fires.
//
/****************************************************************************************************/
void AppleUSBCDCEthernet::timerFired(OSObject *owner, IOTimerEventSource *sender)
{

//    XTRACE(this, 0, 0, "timerFired");
    
    if (owner)
    {
	AppleUSBCDCEthernet* target = OSDynamicCast(AppleUSBCDCEthernet, owner);
	
	if (target)
	{
	    target->timeoutOccurred(sender);
	}
    }
    
}/* end timerFired */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::timeoutOccurred
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Timeout handler, used for stats gathering.
//
/****************************************************************************************************/

void AppleUSBCDCEthernet::timeoutOccurred(IOTimerEventSource * /*timer*/)
{
    UInt32		*enetStats;
    UInt16		currStat;
    IOReturn		rc;
    IOUSBDevRequest	*STREQ;
    bool		statOk = false;

//    XTRACE(this, 0, 0, "timeoutOccurred");

    enetStats = (UInt32 *)&fEthernetStatistics;
    if (*enetStats == 0)
    {
        XTRACE(this, 0, 0, "timeoutOccurred - No Ethernet statistics defined");
        return;                                                 // and don't bother us again
    }
    
    if (fReady == false)
    {
        XTRACE(this, 0, 0, "timeoutOccurred - Spurious");    
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
                XTRACE(this, 0, 0, "timeoutOccurred - allocate STREQ failed");
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
	
                rc = fpDevice->DeviceRequest(STREQ, &fStatsCompletionInfo);
                if (rc != kIOReturnSuccess)
                {
                    XTRACE(this, STREQ->bRequest, rc, "timeoutOccurred - Error issueing DeviceRequest");
                    IOFree(STREQ, sizeof(IOUSBDevRequest));
                } else {
                    fStatInProgress = true;
                }
            }
        }
    }

        // Restart the watchdog timer
        
    fTimerSource->setTimeoutMS(WATCHDOG_TIMER_MS);

}/* end timeoutOccurred */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCEthernet::message
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

IOReturn AppleUSBCDCEthernet::message(UInt32 type, IOService *provider, void *argument)
{
    IOReturn	ior;
	
    XTRACE(this, 0, type, "message");
	
    switch (type)
    {
        case kIOMessageServiceIsTerminated:
            XTRACE(this, fReady, type, "message - kIOMessageServiceIsTerminated");
			
            if (fReady)
            {
                if (!fTerminate)		// Check if we're already being terminated
                { 
		    // NOTE! This call below depends on the hard coded path of this KEXT. Make sure
		    // that if the KEXT moves, this path is changed!
		    KUNCUserNotificationDisplayNotice(
			0,		// Timeout in seconds
			0,		// Flags (for later usage)
			"",		// iconPath (not supported yet)
			"",		// soundPath (not supported yet)
			"/System/Library/Extensions/AppleUSBCDCEthernet.kext",				// localizationPath
			"Unplug Header",		// the header
			"Unplug Notice",		// the notice - look in Localizable.strings
			"OK"); 
                }
            } else {
                if (fCommInterface)	
                {
                    fCommInterface->close(this);	
                    fCommInterface->release();
                    fCommInterface = NULL;	
                }
	
                if (fDataInterface)	
                { 
                    fDataInterface->close(this);	
                    fDataInterface->release();
                    fDataInterface = NULL;	
                }
                
            	fpDevice->close(this); 	// need to close so we can get the free and stop calls, only if no sessions active (see putToSleep)
                fpDevice = NULL;
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
            
                // If the reads are dead try and resurrect them
            
            if (fCommDead)
            {
                ior = fCommPipe->Read(fCommPipeMDP, &fCommCompletionInfo, NULL);
                if (ior != kIOReturnSuccess)
                {
                    XTRACE(this, 0, ior, "message - Failed to queue Comm pipe read");
                } else {
                    fCommDead = false;
                }
            }
            
            if (fDataDead)
            {
                ior = fInPipe->Read(fPipeInMDP, &fReadCompletionInfo, NULL);
                if (ior != kIOReturnSuccess)
                {
                    XTRACE(this, 0, ior, "message - Failed to queue Data pipe read");
                } else {
                    fDataDead = false;
                }
            }

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
