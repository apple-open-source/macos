/*
    File:		USBCDCEthernet.cpp
	
    Description:	This is a sample USB Communication Device Class (CDC) driver, Ethernet model.
                        Note that this sample has not been tested against any actual hardware since there
                        are very few CDC Ethernet devices currently in existence. 
                        
                        This sample requires Mac OS X 10.1 and later. If built on a version prior to 
                        Mac OS X 10.2, a compiler warning "warning: ANSI C++ forbids data member `ip_opts'
                        with same name as enclosing class" will be issued. This warning can be ignored.
                        
    Copyright:		© Copyright 1998-2002 Apple Computer, Inc. All rights reserved.
	
    Disclaimer:		IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
                        ("Apple") in consideration of your agreement to the following terms, and your
                        use, installation, modification or redistribution of this Apple software
                        constitutes acceptance of these terms.  If you do not agree with these terms,
                        please do not use, install, modify or redistribute this Apple software.

                        In consideration of your agreement to abide by the following terms, and subject
                        to these terms, Apple grants you a personal, non-exclusive license, under Apple’s
                        copyrights in this original Apple software (the "Apple Software"), to use,
                        reproduce, modify and redistribute the Apple Software, with or without
                        modifications, in source and/or binary forms; provided that if you redistribute
                        the Apple Software in its entirety and without modifications, you must retain
                        this notice and the following text and disclaimers in all such redistributions of
                        the Apple Software.  Neither the name, trademarks, service marks or logos of
                        Apple Computer, Inc. may be used to endorse or promote products derived from the
                        Apple Software without specific prior written permission from Apple.  Except as
                        expressly stated in this notice, no other rights or licenses, express or implied,
                        are granted by Apple herein, including but not limited to any patent rights that
                        may be infringed by your derivative works or by other works in which the Apple
                        Software may be incorporated.

                        The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
                        WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
                        WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
                        PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
                        COMBINATION WITH YOUR PRODUCTS.

                        IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
                        CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
                        GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
                        ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
                        OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
                        (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
                        ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
				
	Change History (most recent first):
        
            <1>	 	07/30/02	New sample.
            <2>		12/01/02	Fixed a couple of bugs and added an output buffer pool
        
*/

#include "USBCDCEthernet.h"

#define MIN_BAUD (50 << 1)

static globals	g;	// Instantiate the globals
    
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

OSDefineMetaClassAndStructors(com_apple_driver_dts_USBCDCEthernet, IOEthernetController);

#if USE_ELG
/****************************************************************************************************/
//
//		Function:	AllocateEventLog
//
//		Inputs:		size - amount of memory to allocate
//
//		Outputs:	None
//
//		Desc:		Allocates the event log buffer
//
/****************************************************************************************************/

static void AllocateEventLog(UInt32 size)
{
    if (g.evLogBuf)
        return;

    g.evLogFlag = 0;            // assume insufficient memory
    g.evLogBuf = (UInt8*)IOMalloc(size);
    if (!g.evLogBuf)
    {
        kprintf("com_apple_driver_dts_USBCDCEthernet evLog allocation failed ");
        return;
    }

    bzero(g.evLogBuf, size);
    g.evLogBufp	= g.evLogBuf;
    g.evLogBufe	= g.evLogBufp + kEvLogSize - 0x20; // ??? overran buffer?
    g.evLogFlag  = 0xFEEDBEEF;	// continuous wraparound
//	g.evLogFlag  = 'step';		// stop at each ELG
//	g.evLogFlag  = 0x0333;		// any nonzero - don't wrap - stop logging at buffer end

    IOLog("AllocateEventLog - &globals=%8x buffer=%8x", (unsigned int)&g, (unsigned int)g.evLogBuf);

    return;
	
}/* end AllocateEventLog */

/****************************************************************************************************/
//
//		Function:	EvLog
//
//		Inputs:		a - anything, b - anything, ascii - 4 charater tag, str - any info string			
//
//		Outputs:	None
//
//		Desc:		Writes the various inputs to the event log buffer
//
/****************************************************************************************************/

static void EvLog(UInt32 a, UInt32 b, UInt32 ascii, char* str)
{
    register UInt32	*lp;           // Long pointer
    mach_timespec_t	time;

    if (g.evLogFlag == 0)
        return;

    IOGetTime(&time);

    lp = (UInt32*)g.evLogBufp;
    g.evLogBufp += 0x10;

    if (g.evLogBufp >= g.evLogBufe)       // handle buffer wrap around if any
    {    
        g.evLogBufp  = g.evLogBuf;
        if (g.evLogFlag != 0xFEEDBEEF)    // make 0xFEEDBEEF a symbolic ???
            g.evLogFlag = 0;                // stop tracing if wrap undesired
    }

        // compose interrupt level with 3 byte time stamp:

    *lp++ = (g.intLevel << 24) | ((time.tv_nsec >> 10) & 0x003FFFFF);   // ~ 1 microsec resolution
    *lp++ = a;
    *lp++ = b;
    *lp   = ascii;

    if(g.evLogFlag == 'step')
    {	
        static char	code[ 5 ] = {0,0,0,0,0};
        *(UInt32*)&code = ascii;
        IOLog("%8x com_apple_driver_dts_USBCDCEthernet: %8x %8x %s\n", time.tv_nsec>>10, (unsigned int)a, (unsigned int)b, code);
    }

    return;
	
}/* end EvLog */
#endif // USE_ELG

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

static void USBLogData(UInt8 Dir, UInt32 Count, char *buf)
{
    UInt8	wlen, i, Aspnt, Hxpnt;
    UInt8	wchr;
    char	LocBuf[buflen+1];

    for (i=0; i<=buflen; i++)
    {
        LocBuf[i] = 0x20;
    }
    LocBuf[i] = 0x00;
	
    if (Dir == kUSBIn)
    {
        IOLog("com_apple_driver_dts_USBCDCEthernet: USBLogData - Read Complete, size = %8x\n", (unsigned int)Count);
    } else {
        if (Dir == kUSBOut)
        {
            IOLog("com_apple_driver_dts_USBCDCEthernet: USBLogData - Write, size = %8x\n", (unsigned int)Count);
        } else {
            if (Dir == kUSBAnyDirn)
            {
                IOLog("com_apple_driver_dts_USBCDCEthernet: USBLogData - Other, size = %8x\n", (unsigned int)Count);
            }
        }			
    }

    if (Count > dumplen)
    {
        wlen = dumplen;
    } else {
        wlen = Count;
    }
	
    if (wlen > 0)
    {
        Aspnt = Asciistart;
        Hxpnt = 0;
        for (i=1; i<=wlen; i++)
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
        LocBuf[(wlen + Asciistart) + 1] = 0x00;
        IOLog(LocBuf);
        IOLog("\n");
        IOSleep(Sleep_Time);					// Try and keep the log from overflowing
    } else {
        IOLog("com_apple_driver_dts_USBCDCEthernet: USBLogData - No data, Count=0\n");
    }
	
}/* end USBLogData */
#endif // LOG_DATA

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::commReadComplete
//
//		Inputs:		obj - me, param - parameter block(the Port), rc - return code, remaining - what's left
//												(whose idea was that?)
//
//		Outputs:	None
//
//		Desc:		Interrupt pipe (Comm interface) read completion routine
//
/****************************************************************************************************/

void com_apple_driver_dts_USBCDCEthernet::commReadComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    com_apple_driver_dts_USBCDCEthernet	*me = (com_apple_driver_dts_USBCDCEthernet*)obj;
    IOReturn		ior;
    UInt32		dLen;
    UInt8		notif;

    if (rc == kIOReturnSuccess)	// If operation returned ok
    {
        dLen = COMM_BUFF_SIZE - remaining;
        ELG(rc, dLen, 'cRC+', "com_apple_driver_dts_USBCDCEthernet::commReadComplete");
		
            // Now look at the state stuff
            
        LogData(kUSBAnyDirn, dLen, me->fCommPipeBuffer);
        
        notif = me->fCommPipeBuffer[1];
        if (dLen > 7)
        {
            switch(notif)
            {
                case kNetwork_Connection:
                    me->fLinkStatus = me->fCommPipeBuffer[2];
                    ELG(0, me->fLinkStatus, 'cRNC', "com_apple_driver_dts_USBCDCEthernet::commReadComplete - kNetwork_Connection");
                    break;
                case kConnection_Speed_Change:				// In you-know-whose format
                    me->fUpSpeed = USBToHostLong((UInt32)me->fCommPipeBuffer[8]);
                    me->fDownSpeed = USBToHostLong((UInt32)me->fCommPipeBuffer[13]);
                    ELG(me->fUpSpeed, me->fDownSpeed, 'cRCS', "com_apple_driver_dts_USBCDCEthernet::commReadComplete - kConnection_Speed_Change");
                    break;
                default:
                    ELG(0, notif, 'cRUn', "com_apple_driver_dts_USBCDCEthernet::commReadComplete - Unknown notification");
                    break;
            }
        } else {
            ELG(0, notif, 'cRIn', "com_apple_driver_dts_USBCDCEthernet::commReadComplete - Invalid notification");
        }
    } else {
        ELG(0, rc, 'cRC-', "com_apple_driver_dts_USBCDCEthernet::commReadComplete - IO error");
    }

        // Queue the next read, only if not aborted
	
    if (rc != kIOReturnAborted)
    {
        ior = me->fCommPipe->Read(me->fCommPipeMDP, &me->fCommCompletionInfo, NULL);
        if (ior != kIOReturnSuccess)
        {
            ELG(0, ior, 'cRF-', "com_apple_driver_dts_USBCDCEthernet::commReadComplete - Failed to queue next read");
            if (ior == kIOUSBPipeStalled)
            {
                me->fCommPipe->Reset();
                ior = me->fCommPipe->Read(me->fCommPipeMDP, &me->fCommCompletionInfo, NULL);
                if (ior != kIOReturnSuccess)
                {
                    ELG(0, ior, 'cR--', "com_apple_driver_dts_USBCDCEthernet::commReadComplete - Failed, read dead");
                    me->fCommDead = true;
                }
            }

        }
    }
    
    return;
	
}/* end commReadComplete */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::dataReadComplete
//
//		Inputs:		obj - me
//				param - unused
//				rc - return code
//				remaining - what's left
//
//		Outputs:	None
//
//		Desc:		BulkIn pipe (Data interface) read completion routine
//
/****************************************************************************************************/

void com_apple_driver_dts_USBCDCEthernet::dataReadComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    com_apple_driver_dts_USBCDCEthernet	*me = (com_apple_driver_dts_USBCDCEthernet*)obj;
    IOReturn		ior;

    if (rc == kIOReturnSuccess)	// If operation returned ok
    {	
        ELG(0, me->fMax_Block_Size - remaining, 'dRC+', "dataReadComplete");
		
        LogData(kUSBIn, (me->fMax_Block_Size - remaining), me->fPipeInBuffer);
	
            // Move the incoming bytes up the stack

        me->receivePacket(me->fPipeInBuffer, me->fMax_Block_Size - remaining);
	
    } else {
        ELG(0, rc, 'dRc-', "com_apple_driver_dts_USBCDCEthernet::dataReadComplete - Read completion io err");
        if (rc != kIOReturnAborted)
        {
            rc = me->clearPipeStall(me->fInPipe);
            if (rc != kIOReturnSuccess)
            {
                ELG(0, rc, 'dR--', "com_apple_driver_dts_USBCDCEthernet::dataReadComplete - clear stall failed (trying to continue)");
            }
        }
    }
    
        // Queue the next read, only if not aborted
	
    if (rc != kIOReturnAborted)
    {
        ior = me->fInPipe->Read(me->fPipeInMDP, &me->fReadCompletionInfo, NULL);
        if (ior != kIOReturnSuccess)
        {
            ELG(0, ior, 'dRe-', "com_apple_driver_dts_USBCDCEthernet::dataReadComplete - Failed to queue read");
            if (ior == kIOUSBPipeStalled)
            {
                me->fInPipe->Reset();
                ior = me->fInPipe->Read(me->fPipeInMDP, &me->fReadCompletionInfo, NULL);
                if (ior != kIOReturnSuccess)
                {
                    ELG(0, ior, 'dR--', "com_apple_driver_dts_USBCDCEthernet::dataReadComplete - Failed, read dead");
                    me->fDataDead = true;
                }
            }
        }
    }

    return;
	
}/* end dataReadComplete */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::dataWriteComplete
//
//		Inputs:		obj - me
//				param - pool index
//				rc - return code
//				remaining - what's left
//
//		Outputs:	None
//
//		Desc:		BulkOut pipe (Data interface) write completion routine
//
/****************************************************************************************************/

void com_apple_driver_dts_USBCDCEthernet::dataWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    com_apple_driver_dts_USBCDCEthernet	*me = (com_apple_driver_dts_USBCDCEthernet *)obj;
    struct mbuf		*m;
    UInt32		pktLen = 0;
    UInt32		numbufs = 0;
    UInt32		poolIndx;

    poolIndx = (UInt32)param;
    
    if (rc == kIOReturnSuccess)						// If operation returned ok
    {	
        ELG(rc, poolIndx, 'dWC+', "com_apple_driver_dts_USBCDCEthernet::dataWriteComplete");
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
                ELG(rc, pktLen, 'dWCz', "com_apple_driver_dts_USBCDCEthernet::dataWriteComplete - writing zero length packet");
                me->fPipeOutBuff[poolIndx].pipeOutMDP->setLength(0);
                me->fWriteCompletionInfo.parameter = NULL;
                me->fOutPipe->Write(me->fPipeOutBuff[poolIndx].pipeOutMDP, &me->fWriteCompletionInfo);
            }
        }
    } else {
        ELG(rc, poolIndx, 'dWe-', "com_apple_driver_dts_USBCDCEthernet::dataWriteComplete - IO err");

        if (me->fPipeOutBuff[poolIndx].m != NULL)
        {
            me->freePacket(me->fPipeOutBuff[poolIndx].m);		// Free the mbuf anyway
            me->fPipeOutBuff[poolIndx].m = NULL;
        }
        if (rc != kIOReturnAborted)
        {
            rc = me->clearPipeStall(me->fOutPipe);
            if (rc != kIOReturnSuccess)
            {
                ELG(0, rc, 'dW--', "com_apple_driver_dts_USBCDCEthernet::dataWriteComplete - clear stall failed (trying to continue)");
            }
        }
    }
        
    return;
	
}/* end dataWriteComplete */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::merWriteComplete
//
//		Inputs:		obj - me
//				param - parameter block (may or may not be present depending on request) 
//				rc - return code
//				remaining - what's left
//
//		Outputs:	None
//
//		Desc:		Management element request write completion routine
//
/****************************************************************************************************/

void com_apple_driver_dts_USBCDCEthernet::merWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    IOUSBDevRequest	*MER = (IOUSBDevRequest*)param;
    UInt16		dataLen;
	
    if (MER)
    {
        if (rc == kIOReturnSuccess)
        {
            ELG(MER->bRequest, remaining, 'mWC+', "com_apple_driver_dts_USBCDCEthernet::merWriteComplete");
        } else {
            ELG(MER->bRequest, rc, 'mWC-', "com_apple_driver_dts_USBCDCEthernet::merWriteComplete - io err");
        }
		
        dataLen = MER->wLength;
        ELG(0, dataLen, 'mWC ', "com_apple_driver_dts_USBCDCEthernet::merWriteComplete - data length");
        if ((dataLen != 0) && (MER->pData))
        {
            IOFree(MER->pData, dataLen);
        }
        IOFree(MER, sizeof(IOUSBDevRequest));
		
    } else {
        if (rc == kIOReturnSuccess)
        {
            ELG(0, remaining, 'mWr+', "com_apple_driver_dts_USBCDCEthernet::merWriteComplete (request unknown)");
        } else {
            ELG(0, rc, 'rWr-', "com_apple_driver_dts_USBCDCEthernet::merWriteComplete (request unknown) - io err");
        }
    }
	
    return;
	
}/* end merWriteComplete */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::statsWriteComplete
//
//		Inputs:		obj - me
//				param - parameter block 
//				rc - return code
//				remaining - what's left
//
//		Outputs:	None
//
//		Desc:		Ethernet statistics request write completion routine
//
/****************************************************************************************************/

void com_apple_driver_dts_USBCDCEthernet::statsWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    com_apple_driver_dts_USBCDCEthernet	*me = (com_apple_driver_dts_USBCDCEthernet *)obj;
    IOUSBDevRequest	*STREQ = (IOUSBDevRequest*)param;
    UInt16		currStat;
	
    if (STREQ)
    {
        if (rc == kIOReturnSuccess)
        {
            ELG(STREQ->bRequest, remaining, 'sWC+', "com_apple_driver_dts_USBCDCEthernet::statsWriteComplete");
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
                    ELG(currStat, rc, 'sWI-', "com_apple_driver_dts_USBCDCEthernet::statsWriteComplete - Invalid stats code");
                    break;
            }

        } else {
            ELG(STREQ->bRequest, rc, 'sWC-', "com_apple_driver_dts_USBCDCEthernet::statsWriteComplete - io err");
        }
		
        IOFree(STREQ, sizeof(IOUSBDevRequest));
    } else {
        if (rc == kIOReturnSuccess)
        {
            ELG(0, remaining, 'sWr+', "com_apple_driver_dts_USBCDCEthernet::statsWriteComplete (request unknown)");
        } else {
            ELG(0, rc, 'sWr-', "com_apple_driver_dts_USBCDCEthernet::statsWriteComplete (request unknown) - io err");
        }
    }
	
    me->fStatValue = 0;
    me->fStatInProgress = false;
    return;
	
}/* end statsWriteComplete */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::init
//
//		Inputs:		properties - data (keys and values) used to match
//
//		Outputs:	Return code - true (init successful), false (init failed)
//
//		Desc:		Initialize the driver.
//
/****************************************************************************************************/

bool com_apple_driver_dts_USBCDCEthernet::init(OSDictionary *properties)
{
    UInt32	i;

    g.evLogBufp = NULL;
        
#if USE_ELG
    AllocateEventLog(kEvLogSize);
    ELG(&g, g.evLogBufp, 'USBM', "com_apple_driver_dts_USBCDCEthernet::init - event logging set up.");

    waitForService(resourceMatching("kdp"));
#endif /* USE_ELG */

    ELG(0, 0, 'init', "com_apple_driver_dts_USBCDCEthernet::init");
    
    if (super::init(properties) == false)
    {
        ELG(0, 0, 'in--', "com_apple_driver_dts_USBCDCEthernet::init - initialize super failed");
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
    }

    return true;

}/* end init*/

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::start
//
//		Inputs:		provider - my provider
//
//		Outputs:	Return code - true (it's me), false (sorry it probably was me, but I can't configure it)
//
//		Desc:		This is called once it has beed determined I'm probably the best 
//				driver for this device.
//
/****************************************************************************************************/

bool com_apple_driver_dts_USBCDCEthernet::start(IOService *provider)
{
    UInt8	configs;	// number of device configurations

    ELG(this, provider, 'strt', "com_apple_driver_dts_USBCDCEthernet::start - this, provider.");
    if(!super::start(provider))
    {
        ALERT(0, 0, 'SS--', "com_apple_driver_dts_USBCDCEthernet::start - start super failed");
        return false;
    }

	// Get my USB device provider - the device

    fpDevice = OSDynamicCast(IOUSBDevice, provider);
    if(!fpDevice)
    {
        ALERT(0, 0, 'Dev-', "com_apple_driver_dts_USBCDCEthernet::start - provider invalid");
        stop(provider);
        return false;
    }

	// Let's see if we have any configurations to play with
		
    configs = fpDevice->GetNumConfigurations();
    if (configs < 1)
    {
        ALERT(0, 0, 'Cfg-', "com_apple_driver_dts_USBCDCEthernet::start - no configurations");
        stop(provider);
        return false;
    }
	
	// Now take control of the device and configure it
		
    if (!fpDevice->open(this))
    {
        ALERT(0, 0, 'Opn-', "com_apple_driver_dts_USBCDCEthernet::start - unable to open device");
        stop(provider);
        return false;
    }
	
    if (!configureDevice(configs))
    {
        ALERT(0, 0, 'Nub-', "com_apple_driver_dts_USBCDCEthernet::start - failed");
        fpDevice->close(this);
        fpDevice = NULL;
        stop(provider);
        return false;
    }
    
    ELG(0, 0, 'Nub+', "com_apple_driver_dts_USBCDCEthernet::start - successful");
    
    return true;
    	
}/* end start */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::free
//
//		Inputs:		None
//
//		Outputs:	None
//
//		Desc:		Clean up and free the log 
//
/****************************************************************************************************/

void com_apple_driver_dts_USBCDCEthernet::free()
{

    ELG(0, 0, 'free', "com_apple_driver_dts_USBCDCEthernet::free");
	
#if USE_ELG
    if (g.evLogBuf)
    	IOFree(g.evLogBuf, kEvLogSize);
#endif /* USE_ELG */

    super::free();
    return;
	
}/* end free */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::stop
//
//		Inputs:		provider - my provider
//
//		Outputs:	None
//
//		Desc:		Stops
//
/****************************************************************************************************/

void com_apple_driver_dts_USBCDCEthernet::stop(IOService *provider)
{
    
    ELG(0, 0, 'stop', "com_apple_driver_dts_USBCDCEthernet::stop");
    
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
//		Method:		com_apple_driver_dts_USBCDCEthernet::configureDevice
//
//		Inputs:		numConfigs - number of configurations present
//
//		Outputs:	return Code - true (device configured), false (device not configured)
//
//		Desc:		Finds the configurations and then the appropriate interfaces etc.
//
/****************************************************************************************************/

bool com_apple_driver_dts_USBCDCEthernet::configureDevice(UInt8 numConfigs)
{
    IOUSBFindInterfaceRequest		req;			// device request
    const IOUSBInterfaceDescriptor	*altInterfaceDesc;
    IOReturn				ior = kIOReturnSuccess;
    UInt16				numends;
    UInt16				alt;
    bool				goodCall;
       
    ELG(0, numConfigs, 'cDev', "com_apple_driver_dts_USBCDCEthernet::configureDevice");
    	
        // Initialize and "configure" the device
        
    if (!initDevice(numConfigs))
    {
        ELG(0, 0, 'cDi-', "com_apple_driver_dts_USBCDCEthernet::configureDevice - initDevice failed");
        return false;
    }

        // Get the Comm. Class interface

    req.bInterfaceClass	= kUSBCommClass;
    req.bInterfaceSubClass = kEthernetControlModel;
    req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
    
    fCommInterface = fpDevice->FindNextInterface(NULL, &req);
    if (!fCommInterface)
    {
        ELG(0, 0, 'FIC-', "com_apple_driver_dts_USBCDCEthernet::configureDevice - Finding the first CDC interface failed");
        return false;
    }
    
    if (!getFunctionalDescriptors())
    {
        ELG(0, 0, 'cDi-', "com_apple_driver_dts_USBCDCEthernet::configureDevice - getFunctionalDescriptors failed");
        return false;
    }

    goodCall = fCommInterface->open(this);
    if (!goodCall)
    {
        ELG(0, 0, 'epC-', "com_apple_driver_dts_USBCDCEthernet::configureDevice - open comm interface failed.");
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
            ELG(numends, fDataInterface, 'cDD+', "com_apple_driver_dts_USBCDCEthernet::configureDevice - Data Class interface found");
        } else {
            altInterfaceDesc = fDataInterface->FindNextAltInterface(NULL, &req);
            if (!altInterfaceDesc)
            {
                ELG(0, 0, 'cDn-', "com_apple_driver_dts_USBCDCEthernet::configureDevice - FindNextAltInterface failed");
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
                        ELG(numends, alt, 'cD++', "com_apple_driver_dts_USBCDCEthernet::configureDevice - Data Class interface (alternate) found");
                        ior = fDataInterface->SetAlternateInterface(this, alt);
                        if (ior == kIOReturnSuccess)
                        {
                            ELG(0, 0, 'cDA+', "com_apple_driver_dts_USBCDCEthernet::configureDevice - Alternate set");
                            break;
                        } else {
                            ELG(0, 0, 'cDS-', "com_apple_driver_dts_USBCDCEthernet::configureDevice - SetAlternateInterface failed");
                            numends = 0;
                        }
                    } else {
                        ELG(0, 0, 'cDD-', "com_apple_driver_dts_USBCDCEthernet::configureDevice - open data interface failed.");
                        numends = 0;
                    }
                } else {
                    ELG(0, altInterfaceDesc, 'cDe-', "com_apple_driver_dts_USBCDCEthernet::configureDevice - No endpoints this alternate");
                }
                altInterfaceDesc = fDataInterface->FindNextAltInterface(altInterfaceDesc, &req);
            }
        }
    } else {
        ELG(0, 0, 'cDr-', "com_apple_driver_dts_USBCDCEthernet::configureDevice - FindNextInterface failed");
    }

    if (numends < 2)
    {
        ELG(0, 0, 'cDs-', "com_apple_driver_dts_USBCDCEthernet::configureDevice - Finding a Data Class interface failed");
        fCommInterface->close(this);
        fCommInterface = NULL;
        return false;
    } else {
    
        fCommInterface->retain();
        fDataInterface->retain();

            // Found both so now let's publish the interface
	
        if (!createNetworkInterface())
        {
            ELG(0, 0, 'cDc-', "com_apple_driver_dts_USBCDCEthernet::configureDevice - createNetworkInterface failed");
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
            ELG(0, ior, 'cCSD', "com_apple_driver_dts_USBCDCEthernet::configureDevice - SuspendDevice error");
        }
    }

    return true;

}/* end configureDevice */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::initDevice
//
//		Inputs:		numConfigs - number of configurations present
//
//		Outputs:	return Code - true (CDC present), false (CDC not present)
//
//		Desc:		Determines if this is a CDC compliant device and then sets the configuration
//
/****************************************************************************************************/

bool com_apple_driver_dts_USBCDCEthernet::initDevice(UInt8 numConfigs)
{
    IOUSBFindInterfaceRequest		req;
    const IOUSBConfigurationDescriptor	*cd = NULL;		// configuration descriptor
    IOUSBInterfaceDescriptor 		*intf = NULL;		// interface descriptor
    IOReturn				ior = kIOReturnSuccess;
    UInt8				cval;
    UInt8				config = 0;
    bool				goodconfig = false;
       
    ELG(0, numConfigs, 'cDev', "com_apple_driver_dts_USBCDCEthernet::initDevice");
    	
        // Make sure we have a CDC interface to play with
        
    for (cval=0; cval<numConfigs; cval++)
    {
    	ELG(0, cval, 'CkCn', "com_apple_driver_dts_USBCDCEthernet::initDevice - Checking Configuration");
		
     	cd = fpDevice->GetFullConfigurationDescriptor(cval);
     	if (!cd)
    	{
            ELG(0, 0, 'GFC-', "com_apple_driver_dts_USBCDCEthernet::initDevice - Error getting the full configuration descriptor");
        } else {
            req.bInterfaceClass	= kUSBCommClass;
            req.bInterfaceSubClass = kEthernetControlModel;
            req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
            req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
            ior = fpDevice->FindNextInterfaceDescriptor(cd, intf, &req, &intf);
            if (ior == kIOReturnSuccess)
            {
                if (intf)
                {
                    ELG(0, config, 'FNI+', "com_apple_driver_dts_USBCDCEthernet::initDevice - Interface descriptor found");
                    config = cd->bConfigurationValue;
                    goodconfig = true;					// We have at least one CDC interface in this configuration
                    break;
                } else {
                    ELG(0, config, 'FNI-', "com_apple_driver_dts_USBCDCEthernet::initDevice - That's weird the interface was null");
                }
            } else {
                ELG(ior, cval, 'FNID', "com_apple_driver_dts_USBCDCEthernet::initDevice - No CDC interface found this configuration");
            }
        }
    }
        
    if (goodconfig)
    {
        ior = fpDevice->SetConfiguration(this, config);
        if (ior != kIOReturnSuccess)
        {
            ELG(0, ior, 'SCo-', "com_apple_driver_dts_USBCDCEthernet::initDevice - SetConfiguration error");
            goodconfig = false;			
        }
    } else {
        return false;
    }
    
    fbmAttributes = cd->bmAttributes;
    ELG(fbmAttributes, kUSBAtrRemoteWakeup, 'GFbA', "com_apple_driver_dts_USBCDCEthernet::initDevice - Configuration bmAttributes");
    
        // Save the ID's
    
    fVendorID = fpDevice->GetVendorID();
    fProductID = fpDevice->GetProductID();
    
    return goodconfig;

}/* end initDevice */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::getFunctionalDescriptors
//
//		Inputs:		
//
//		Outputs:	return - true (descriptors ok), false (somethings not right or not supported)	
//
//		Desc:		Finds all the functional descriptors for the specific interface
//
/****************************************************************************************************/

bool com_apple_driver_dts_USBCDCEthernet::getFunctionalDescriptors()
{
    bool				gotDescriptors = false;
    bool				configok = true;
    bool				enet = false;
    IOReturn				ior;
    const HeaderFunctionalDescriptor 	*funcDesc = NULL;
    EnetFunctionalDescriptor		*ENETFDesc;
       
    ELG(0, 0, 'gFDs', "com_apple_driver_dts_USBCDCEthernet::getFunctionalDescriptors");
        
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
                    ELG(funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, 'gFHd', "com_apple_driver_dts_USBCDCEthernet::getFunctionalDescriptors - Header Functional Descriptor");
                    break;
                case Enet_Functional_Descriptor:
                    (const HeaderFunctionalDescriptor *)ENETFDesc = funcDesc;
                    ELG(funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, 'gFEN', "com_apple_driver_dts_USBCDCEthernet::getFunctionalDescriptors - Ethernet Functional Descriptor");
                    enet = true;
                    break;
                case Union_FunctionalDescriptor:
                    ELG(funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, 'gFUn', "com_apple_driver_dts_USBCDCEthernet::getFunctionalDescriptors - Union Functional Descriptor");
                    break;
                default:
                    ELG(funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, 'gFFD', "com_apple_driver_dts_USBCDCEthernet::getFunctionalDescriptors - unknown Functional Descriptor");
                    break;
            }
        }
    } while(!gotDescriptors);

    if (!enet)
    {
        configok = false;					// The Enet Func. Desc.  must be present
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
        
        fMcFilters = USBToHostWord((UInt16)ENETFDesc->wNumberMCFilters[0]);
        
            // Get the Ethernet address
    
        if (ENETFDesc->iMACAddress != 0)
        {	
            ior = fpDevice->GetStringDescriptor(ENETFDesc->iMACAddress, (char *)&fEaddr, 6);
            if (ior == kIOReturnSuccess)
            {
#if LOG_DATA
                ELG(0, ENETFDesc->iMACAddress, 'gFEA', "com_apple_driver_dts_USBCDCEthernet::getFunctionalDescriptors - Ethernet address");
#endif
                LogData(kUSBAnyDirn, 6, fEaddr);
                
                fMax_Block_Size = USBToHostWord((UInt16)ENETFDesc->wMaxSegmentSize[0]);
                ELG(0, fMax_Block_Size, 'gFMs', "com_apple_driver_dts_USBCDCEthernet::getFunctionalDescriptors - Maximum segment size");
            } else {
                ELG(0, 0, 'gFAe', "com_apple_driver_dts_USBCDCEthernet::getFunctionalDescriptors - Error retrieving Ethernet address");
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
//		Method:		com_apple_driver_dts_USBCDCEthernet::createNetworkInterface
//
//		Inputs:		
//
//		Outputs:	return Code - true (created and initialilzed ok), false (it failed)
//
//		Desc:		Creates and initializes the network interface
//
/****************************************************************************************************/

bool com_apple_driver_dts_USBCDCEthernet::createNetworkInterface()
{
	
    ELG(0, 0, 'crIf', "com_apple_driver_dts_USBCDCEthernet::createNetworkInterface");
    
            // Allocate memory for buffers etc

    fTransmitQueue = (IOGatedOutputQueue *)getOutputQueue();
    if (!fTransmitQueue) 
    {
        ALERT(0, 0, 'crO-', "com_apple_driver_dts_USBCDCEthernet::createNetworkInterface - Output queue initialization failed");
        return false;
    }
    fTransmitQueue->retain();
    
        // Get a reference to the IOWorkLoop in our superclass
        
    fWorkLoop = getWorkLoop();
    
        // Allocate Timer event source
        
    fTimerSource = IOTimerEventSource::timerEventSource(this, timerFired);
    if (fTimerSource == NULL)
    {
        ALERT(0, 0, 'crT-', "com_apple_driver_dts_USBCDCEthernet::createNetworkInterface - Allocate Timer event source failed");
        return false;
    }
    
    if (fWorkLoop->addEventSource(fTimerSource) != kIOReturnSuccess)
    {
        ALERT(0, 0, 'crt-', "com_apple_driver_dts_USBCDCEthernet::start - Add Timer event source failed");        
        return false;
    }

        // Attach an IOEthernetInterface client
        
    ELG(0, 0, 'crai', "com_apple_driver_dts_USBCDCEthernet::createNetworkInterface - attaching and registering interface");
    
    if (!attachInterface((IONetworkInterface **)&fNetworkInterface, true))
    {	
        ALERT( 0, 0, 'crt-', "com_apple_driver_dts_USBCDCEthernet::createNetworkInterface - attachInterface failed");      
        return false;
    }
    
        // Ready to service interface requests
    
    fNetworkInterface->registerService();
    
    ELG(0, 0, 'crEx', "com_apple_driver_dts_USBCDCEthernet::createNetworkInterface - Exiting, successful");

    return true;
	
}/* end createNetworkInterface */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::enable
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

IOReturn com_apple_driver_dts_USBCDCEthernet::enable(IONetworkInterface *netif)
{
    IONetworkMedium	*medium;
    IOMediumType    	mediumType = kIOMediumEthernet10BaseT | kIOMediumOptionFullDuplex;
    
    ELG(0, netif, 'enbl', "com_apple_driver_dts_USBCDCEthernet::enable");

        // If an interface client has previously enabled us,
        // and we know there can only be one interface client
        // for this driver, then simply return true.

    if (fNetifEnabled)
    {
        ELG(0, 0, 'enae', "com_apple_driver_dts_USBCDCEthernet::enable - already enabled");
        return kIOReturnSuccess;
    }
    
    if ((fReady == false) && !wakeUp())
    {
        ELG(0, fReady, 'enr-', "com_apple_driver_dts_USBCDCEthernet::enable - failed");
        return kIOReturnIOError;
    }

        // Mark the controller as enabled by the interface.

    fNetifEnabled = true;
    
        // Assume an active link for now
    
    fLinkStatus = 1;
    medium = IONetworkMedium::getMediumWithType(fMediumDict, mediumType);
    ELG(mediumType, medium, 'enam', "com_apple_driver_dts_USBCDCEthernet::enable - medium type and pointer");
    setLinkStatus(kIONetworkLinkActive | kIONetworkLinkValid, medium, 10 * 1000000);
    ELG(0, 0, 'enaL', "com_apple_driver_dts_USBCDCEthernet::enable - LinkStatus set");
    
        // Start our IOOutputQueue object.

    fTransmitQueue->setCapacity(TRANSMIT_QUEUE_SIZE);
    ELG(0, TRANSMIT_QUEUE_SIZE, 'enaC', "com_apple_driver_dts_USBCDCEthernet::enable - capicity set");
    fTransmitQueue->start();
    ELG(0, 0, 'enaT', "com_apple_driver_dts_USBCDCEthernet::enable - transmit queue started");
    
    USBSetPacketFilter();
    ELG(0, 0, 'enaP', "com_apple_driver_dts_USBCDCEthernet::enable - packet filter applied");

    return kIOReturnSuccess;
    
}/* end enable */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::disable
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
 
IOReturn com_apple_driver_dts_USBCDCEthernet::disable(IONetworkInterface * /*netif*/)
{

    ELG(0, 0, 'dsbl', "com_apple_driver_dts_USBCDCEthernet::disable");

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
//		Method:		com_apple_driver_dts_USBCDCEthernet::setWakeOnMagicPacket
//
//		Inputs:		active - true(wake), false(don't)
//
//		Outputs:	Return code - kIOReturnSuccess
//
//		Desc:		Set for wake on magic packet
//
/****************************************************************************************************/

IOReturn com_apple_driver_dts_USBCDCEthernet::setWakeOnMagicPacket(bool active)
{
    IOUSBDevRequest	devreq;
    IOReturn		ior = kIOReturnSuccess;

    ELG(0, active, 'sWMP', "com_apple_driver_dts_USBCDCEthernet::setWakeOnMagicPacket");
	
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
                ELG(0, ior, 'SCCs', "com_apple_driver_dts_USBCDCEthernet::initDevice - Clearing remote wake up feature successful");
            } else {
                ELG(0, ior, 'SCCf', "com_apple_driver_dts_USBCDCEthernet::initDevice - Clearing remote wake up feature failed");
            }
        }
    } else {
        ELG(0, 0, 'SCRw', "com_apple_driver_dts_USBCDCEthernet::initDevice - Remote wake up not supported");
    }

    
    return kIOReturnSuccess;
    
}/* end setWakeOnMagicPacket */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::getPacketFilters
//
//		Inputs:		group - the filter group
//
//		Outputs:	Return code - kIOReturnSuccess and others
//				filters - the capability
//
//		Desc:		Set the filter capability for the driver
//
/****************************************************************************************************/

IOReturn com_apple_driver_dts_USBCDCEthernet::getPacketFilters(const OSSymbol *group, UInt32 *filters) const
{
    IOReturn	rtn = kIOReturnSuccess;
    
    ELG(group, filters, 'gPkF', "com_apple_driver_dts_USBCDCEthernet::getPacketFilters");

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
        ELG(0, rtn, 'gPk-', "com_apple_driver_dts_USBCDCEthernet::getPacketFilters - failed");
    }
    
    return rtn;
    
}/* end getPacketFilters */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::selectMedium
//
//		Inputs:
//
//		Outputs:
//
//		Desc:		Lets us know if someone is playing with ifconfig
//
/****************************************************************************************************/

IOReturn com_apple_driver_dts_USBCDCEthernet::selectMedium(const IONetworkMedium *medium)
{
    
    ELG(0, 0, 'SlMd', "com_apple_driver_dts_USBCDCEthernet::selectMedium");

    setSelectedMedium(medium);
    
    return kIOReturnSuccess;
        
}/* end selectMedium */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::getHardwareAddress
//
//		Inputs:		
//
//		Outputs:	Return code - kIOReturnSuccess or kIOReturnError
//				ea - the address
//
//		Desc:		Get the ethernet address from the hardware (actually the descriptor)
//
/****************************************************************************************************/

IOReturn com_apple_driver_dts_USBCDCEthernet::getHardwareAddress(IOEthernetAddress *ea)
{
    UInt32      i;

    ELG(0, 0, 'gHdA', "com_apple_driver_dts_USBCDCEthernet::getHardwareAddress");
     
    for (i=0; i<6; i++)
    {
        ea->bytes[i] = fEaddr[i];
    }

    return kIOReturnSuccess;
    
}/* end getHardwareAddress */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::newVendorString
//
//		Inputs:		
//
//		Outputs:	Return code - the vendor string
//
//		Desc:		Identifies the hardware vendor
//
/****************************************************************************************************/

const OSString* com_apple_driver_dts_USBCDCEthernet::newVendorString() const
{

    ELG(0, 0, 'nVSt', "com_apple_driver_dts_USBCDCEthernet::newVendorString");
    
    return OSString::withCString((const char *)defaultName);		// Maybe we should use the descriptors

}/* end newVendorString */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::newModelString
//
//		Inputs:		
//
//		Outputs:	Return code - the model string
//
//		Desc:		Identifies the hardware model
//
/****************************************************************************************************/

const OSString* com_apple_driver_dts_USBCDCEthernet::newModelString() const
{

    ELG(0, 0, 'nMSt', "com_apple_driver_dts_USBCDCEthernet::newModelString");
    
    return OSString::withCString("USB");		// Maybe we should use the descriptors
    
}/* end newModelString */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::newRevisionString
//
//		Inputs:		
//
//		Outputs:	Return code - the revision string
//
//		Desc:		Identifies the hardware revision
//
/****************************************************************************************************/

const OSString* com_apple_driver_dts_USBCDCEthernet::newRevisionString() const
{

    ELG(0, 0, 'nRSt', "com_apple_driver_dts_USBCDCEthernet::newRevisionString");
    
    return OSString::withCString("");
    
}/* end newRevisionString */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::setMulticastMode
//
//		Inputs:		active - true (set it), false (don't)
//
//		Outputs:	Return code - kIOReturnSuccess
//
//		Desc:		Sets multicast mode
//
/****************************************************************************************************/

IOReturn com_apple_driver_dts_USBCDCEthernet::setMulticastMode(bool active)
{

    ELG(0, active, 'stMM', "com_apple_driver_dts_USBCDCEthernet::setMulticastMode" );

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
//		Method:		com_apple_driver_dts_USBCDCEthernet::setMulticastList
//
//		Inputs:		addrs - list of addresses
//				count - number in the list
//
//		Outputs:	Return code - kIOReturnSuccess or kIOReturnIOError
//
//		Desc:		Sets multicast list
//
/****************************************************************************************************/

IOReturn com_apple_driver_dts_USBCDCEthernet::setMulticastList(IOEthernetAddress *addrs, UInt32 count)
{
    bool	uStat;
    ELG(addrs, count, 'stML', "com_apple_driver_dts_USBCDCEthernet::setMulticastList" );
    
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
//		Method:		com_apple_driver_dts_USBCDCEthernet::setPromiscuousMode
//
//		Inputs:		active - true (set it), false (don't)
//
//		Outputs:	Return code - kIOReturnSuccess
//
//		Desc:		Sets promiscuous mode
//
/****************************************************************************************************/

IOReturn com_apple_driver_dts_USBCDCEthernet::setPromiscuousMode(bool active)
{
    
    ELG(0, active, 'stPM', "com_apple_driver_dts_USBCDCEthernet::setPromiscuousMode");

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
//		Method:		com_apple_driver_dts_USBCDCEthernet::createOutputQueue
//
//		Inputs:		
//
//		Outputs:	Return code - the output queue
//
//		Desc:		Creates the output queue
//
/****************************************************************************************************/

IOOutputQueue* com_apple_driver_dts_USBCDCEthernet::createOutputQueue()
{

    ELG(0, 0, 'crOQ', "com_apple_driver_dts_USBCDCEthernet::createOutputQueue" );
    
    return IOBasicOutputQueue::withTarget(this, TRANSMIT_QUEUE_SIZE);
    
}/* end createOutputQueue */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::outputPacket
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

UInt32 com_apple_driver_dts_USBCDCEthernet::outputPacket(struct mbuf *pkt, void *param)
{
    UInt32	ret = kIOReturnOutputSuccess;
    
    ELG(pkt, 0, 'otPk', "com_apple_driver_dts_USBCDCEthernet::outputPacket" );

    if (!fLinkStatus)
    {
        ELG(pkt, fLinkStatus, 'otL-', "com_apple_driver_dts_USBCDCEthernet::outputPacket - link is down" );
        if (fOutputErrsOK)
            fpNetStats->outputErrors++;
        freePacket(pkt);
    } else { 
        if (USBTransmitPacket(pkt) == false)
        {
            ret = kIOReturnOutputStall;
        }
    }

    return ret;
    
}/* end outputPacket */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::configureInterface
//
//		Inputs:		netif - the interface being configured
//
//		Outputs:	Return code - true (configured ok), false (not)
//
//		Desc:		Finish the network interface configuration
//
/****************************************************************************************************/

bool com_apple_driver_dts_USBCDCEthernet::configureInterface(IONetworkInterface *netif)
{
    IONetworkData	*nd;

    ELG(IOThreadSelf(), netif, 'cfIt', "com_apple_driver_dts_USBCDCEthernet::configureInterface");

    if (super::configureInterface(netif) == false)
    {
        ALERT(0, 0, 'cfs-', "com_apple_driver_dts_USBCDCEthernet::configureInterface - super failed");
        return false;
    }
    
        // Get a pointer to the statistics structure in the interface

    nd = netif->getNetworkData(kIONetworkStatsKey);
    if (!nd || !(fpNetStats = (IONetworkStats *)nd->getBuffer()))
    {
        ALERT(0, 0, 'cfn-', "com_apple_driver_dts_USBCDCEthernet::configureInterface - Invalid network statistics");
        return false;
    }

        // Get the Ethernet statistics structure:

    nd = netif->getParameter(kIOEthernetStatsKey);
    if (!nd || !(fpEtherStats = (IOEthernetStats*)nd->getBuffer()))
    {
        ALERT(0, 0, 'cfe-', "com_apple_driver_dts_USBCDCEthernet::configureInterface - Invalid ethernet statistics\n" );
        return false;
    }

    return true;
    
}/* end configureInterface */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::wakeUp
//
//		Inputs:		
//
//		Outputs:	Return Code - true(we're awake), false(failed)
//
//		Desc:		Resumes the device it it was suspended and then gets all the data
//				structures sorted out and all the pipes ready.
//
/****************************************************************************************************/

bool com_apple_driver_dts_USBCDCEthernet::wakeUp()
{
    IOReturn 	rtn = kIOReturnSuccess;

    ELG(0, 0, 'wkUp', "com_apple_driver_dts_USBCDCEthernet::wakeUp");
    
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
		
        // Read the comm interrupt pipe for status:
		
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
	
        ALERT(0, 0, 'wkp-', "com_apple_driver_dts_USBCDCEthernet::wakeUp - Setting up the pipes failed" );
        releaseResources();
        return false;
    } else {
        if (!fMediumDict)
        {
            if (!createMediumTables())
            {
                ALERT(0, 0, 'wkc-', "com_apple_driver_dts_USBCDCEthernet::wakeUp - createMediumTables failed" );
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
//		Method:		com_apple_driver_dts_USBCDCEthernet::putToSleep
//
//		Inputs:		
//
//		Outputs:	Return Code - true(we're asleep), false(failed)
//
//		Desc:		Do clean up and suspend the device.
//
/****************************************************************************************************/

void com_apple_driver_dts_USBCDCEthernet::putToSleep()
{
    IOReturn	ior;

    ELG(0, 0, 'pToS', "com_apple_driver_dts_USBCDCEthernet::putToSleep");
        
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
                ELG(0, ior, 'rPSD', "com_apple_driver_dts_USBCDCEthernet::releasePort - SuspendDevice error");
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
//		Method:		com_apple_driver_dts_USBCDCEthernet::createMediumTables
//
//		Inputs:		
//
//		Outputs:	Return code - true (tables created), false (not created)
//
//		Desc:		Creates the medium tables
//
/****************************************************************************************************/

bool com_apple_driver_dts_USBCDCEthernet::createMediumTables()
{
    IONetworkMedium	*medium;
    UInt64		maxSpeed;
    UInt32		i;

    ELG(0, 0, 'crMT', "com_apple_driver_dts_USBCDCEthernet::createMediumTables");

    maxSpeed = 100;
    fMediumDict = OSDictionary::withCapacity(sizeof(mediumTable) / sizeof(mediumTable[0]));
    if (fMediumDict == 0)
    {
        ELG( 0, 0, 'crc-', "com_apple_driver_dts_USBCDCEthernet::createMediumTables - create dict. failed" );
        return false;
    }

    for (i = 0; i < sizeof(mediumTable) / sizeof(mediumTable[0]); i++ )
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
        ELG( 0, 0, 'crp-', "com_apple_driver_dts_USBCDCEthernet::createMediumTables - publish dict. failed" );
        return false;
    }

    medium = IONetworkMedium::getMediumWithType(fMediumDict, kIOMediumEthernetAuto);
    setCurrentMedium(medium);

    return true;
    
}/* end createMediumTables */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::allocateResources
//
//		Inputs:		
//
//		Outputs:	return code - true (allocate was successful), false (it failed)
//
//		Desc:		Finishes up the rest of the configuration and gets all the endpoints open etc.
//
/****************************************************************************************************/

bool com_apple_driver_dts_USBCDCEthernet::allocateResources()
{
    IOUSBFindEndpointRequest	epReq;		// endPoint request struct on stack
    UInt32			i;

    ELG(0, 0, 'Allo', "com_apple_driver_dts_USBCDCEthernet::allocateResources.");

        // Open all the end points

    epReq.type = kUSBBulk;
    epReq.direction = kUSBIn;
    epReq.maxPacketSize	= 0;
    epReq.interval = 0;
    fInPipe = fDataInterface->FindNextPipe(0, &epReq);
    if (!fInPipe)
    {
        ELG(0, 0, 'inP-', "com_apple_driver_dts_USBCDCEthernet::allocateResources - no bulk input pipe.");
        return false;
    }
    ELG(epReq.maxPacketSize << 16 |epReq.interval, fInPipe, 'inP+', "com_apple_driver_dts_USBCDCEthernet::allocateResources - bulk input pipe.");

    epReq.direction = kUSBOut;
    fOutPipe = fDataInterface->FindNextPipe(0, &epReq);
    if (!fOutPipe)
    {
        ELG(0, 0, 'otP-', "com_apple_driver_dts_USBCDCEthernet::allocateResources - no bulk output pipe.");
        return false;
    }
    fOutPacketSize = epReq.maxPacketSize;
    ELG(epReq.maxPacketSize << 16 |epReq.interval, fOutPipe, 'otP+', "com_apple_driver_dts_USBCDCEthernet::allocateResources - bulk output pipe.");

        // Interrupt pipe - Comm Interface

    epReq.type = kUSBInterrupt;
    epReq.direction = kUSBIn;
    fCommPipe = fCommInterface->FindNextPipe(0, &epReq);
    if (!fCommPipe)
    {
        ELG(0, 0, 'cmP-', "com_apple_driver_dts_USBCDCEthernet::allocateResources - no interrupt in pipe.");
        fCommPipeMDP = NULL;
        fCommPipeBuffer = NULL;
//        return false;
    } else {
        ELG(epReq.maxPacketSize << 16 |epReq.interval, fCommPipe, 'cmP+', "com_apple_driver_dts_USBCDCEthernet::allocateResources - comm pipe.");

            // Allocate Memory Descriptor Pointer with memory for the Comm pipe:

        fCommPipeMDP = IOBufferMemoryDescriptor::withCapacity(COMM_BUFF_SIZE, kIODirectionIn);
        if (!fCommPipeMDP)
            return false;
		
        fCommPipeMDP->setLength(COMM_BUFF_SIZE);
        fCommPipeBuffer = (UInt8*)fCommPipeMDP->getBytesNoCopy();
        ELG(0, fCommPipeBuffer, 'cBuf', "com_apple_driver_dts_USBCDCEthernet::allocateResources - comm buffer");
    }

        // Allocate Memory Descriptor Pointer with memory for the data-in bulk pipe:

    fPipeInMDP = IOBufferMemoryDescriptor::withCapacity(fMax_Block_Size, kIODirectionIn);
    if (!fPipeInMDP)
        return false;
		
    fPipeInMDP->setLength(fMax_Block_Size);
    fPipeInBuffer = (UInt8*)fPipeInMDP->getBytesNoCopy();
    ELG(0, fPipeInBuffer, 'iBuf', "com_apple_driver_dts_USBCDCEthernet::allocateResources - input buffer");
    
        // Allocate Memory Descriptor Pointers with memory for the data-out bulk pipe pool

    for (i=0; i<kOutBufPool; i++)
    {
        fPipeOutBuff[i].pipeOutMDP = IOBufferMemoryDescriptor::withCapacity(fMax_Block_Size, kIODirectionOut);
        if (!fPipeOutBuff[i].pipeOutMDP)
        {
            ELG(0, 0, 'obf-', "com_apple_driver_dts_USBCDCEthernet::allocateResources - Allocate output descriptor failed");
            return false;
        }
		
        fPipeOutBuff[i].pipeOutMDP->setLength(fMax_Block_Size);
        fPipeOutBuff[i].pipeOutBuffer = (UInt8*)fPipeOutBuff[i].pipeOutMDP->getBytesNoCopy();
        ELG(fPipeOutBuff[i].pipeOutMDP, fPipeOutBuff[i].pipeOutBuffer, 'oBuf', "com_apple_driver_dts_USBCDCEthernet::allocateResources - output buffer");
    }
		
    return true;
	
}/* end allocateResources */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::releaseResources
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Frees up the resources allocated in allocateResources
//
/****************************************************************************************************/

void com_apple_driver_dts_USBCDCEthernet::releaseResources()
{
    UInt32	i;
    
    ELG(0, 0, 'rlRs', "com_apple_driver_dts_USBCDCEthernet::releaseResources");

    for (i=0; i<kOutBufPool; i++)
    {
        if (fPipeOutBuff[i].pipeOutMDP)	
        { 
            fPipeOutBuff[i].pipeOutMDP->release();	
            fPipeOutBuff[i].pipeOutMDP = NULL;
        }
    }
	
    if (fPipeInMDP  )	
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
//		Method:		com_apple_driver_dts_USBCDCEthernet::USBTransmitPacket
//
//		Inputs:		packet - the packet
//
//		Outputs:	Return code - true (transmit started), false (it didn't)
//
//		Desc:		Set up and then transmit the packet
//
/****************************************************************************************************/

bool com_apple_driver_dts_USBCDCEthernet::USBTransmitPacket(struct mbuf *packet)
{
    UInt32		numbufs;			// number of mbufs for this packet
    struct mbuf		*m;				// current mbuf
    UInt32		total_pkt_length = 0;
    UInt32		rTotal = 0;
    IOReturn		ior = kIOReturnSuccess;
    UInt32		poolIndx;
    bool		gotBuffer = false;
    UInt16		tryCount = 0;
	
    ELG (0, packet, 'txPk', "com_apple_driver_dts_USBCDCEthernet::USBTransmitPacket");
			
	// Count the number of mbufs in this packet
        
    m = packet;
    while (m)
    {
        total_pkt_length += m->m_len;
        numbufs++;
	m = m->m_next;
    }
    
    ELG(total_pkt_length, numbufs, 'txTN', "com_apple_driver_dts_USBCDCEthernet::USBTransmitPacket - Total packet length and Number of mbufs");
    
    if (total_pkt_length > fMax_Block_Size)
    {
        ELG(0, 0, 'txBp', "com_apple_driver_dts_USBCDCEthernet::USBTransmitPacket - Bad packet size");	// Note for now and revisit later
        if (fOutputErrsOK)
            fpNetStats->outputErrors++;
        return false;
    }
    
            // Find an ouput buffer in the pool
    
    while (!gotBuffer)
    {
        for (poolIndx=0; poolIndx<kOutBufPool; poolIndx++)
        {
            if (fPipeOutBuff[poolIndx].m == NULL)
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
                ELG(0, 0, 'txBT', "PocketZaurusUSB::USBTransmitPacket - Exceeded output buffer wait threshold");
                if (fOutputErrsOK)
                    fpNetStats->outputErrors++;
                return false;
            } else {
                ELG(0, tryCount, 'txBT', "PocketZaurusUSB::USBTransmitPacket - Waiting for output buffer");
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
    
    LogData(kUSBOut, rTotal, fPipeOutBuff[poolIndx].pipeOutBuffer);
	
    fPipeOutBuff[poolIndx].m = packet;
    fWriteCompletionInfo.parameter = (void *)poolIndx;
    fPipeOutBuff[poolIndx].pipeOutMDP->setLength(rTotal);
    ior = fOutPipe->Write(fPipeOutBuff[poolIndx].pipeOutMDP, &fWriteCompletionInfo);
    if (ior != kIOReturnSuccess)
    {
        ELG(0, ior, 'txBp', "com_apple_driver_dts_USBCDCEthernet::USBTransmitPacket - Write failed");
        if (ior == kIOUSBPipeStalled)
        {
            fOutPipe->Reset();
            ior = fOutPipe->Write(fPipeOutBuff[poolIndx].pipeOutMDP, &fWriteCompletionInfo);
            if (ior != kIOReturnSuccess)
            {
                ELG(0, ior, 'txBp', "com_apple_driver_dts_USBCDCEthernet::USBTransmitPacket - Write really failed");
                if (fOutputErrsOK)
                    fpNetStats->outputErrors++;
                return false;
            }
        }
    }
    if (fOutputPktsOK)		
        fpNetStats->outputPackets++;
    
    return true;

}/* end USBTransmitPacket */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::USBSetMulticastFilter
//
//		Inputs:		addrs - the list of addresses
//				count - How many
//
//		Outputs:	
//
//		Desc:		Set up and send SetMulticastFilter Management Element Request(MER).
//
/****************************************************************************************************/

bool com_apple_driver_dts_USBCDCEthernet::USBSetMulticastFilter(IOEthernetAddress *addrs, UInt32 count)
{
    IOReturn		rc;
    IOUSBDevRequest	*MER;
    UInt8		*eaddrs;
    UInt32		eaddLen;
    UInt32		i,j,rnum;
	
    ELG(fMcFilters, count, 'USMF', "com_apple_driver_dts_USBCDCEthernet::USBSetMulticastFilter");

    if (count > (UInt32)(fMcFilters & kFiltersSupportedMask))
    {
        ELG(0, 0, 'USf-', "com_apple_driver_dts_USBCDCEthernet::USBSetMulticastFilter - No multicast filters supported");
        return false;
    }

    MER = (IOUSBDevRequest*)IOMalloc(sizeof(IOUSBDevRequest));
    if (!MER)
    {
        ELG(0, 0, 'USM-', "com_apple_driver_dts_USBCDCEthernet::USBSetMulticastFilter - allocate MER failed");
        return false;
    }
    bzero(MER, sizeof(IOUSBDevRequest));
	
    eaddLen = count * kIOEthernetAddressSize;
    eaddrs = (UInt8 *)IOMalloc(eaddLen);
    if (!eaddrs)
    {
        ELG(0, 0, 'USA-', "com_apple_driver_dts_USBCDCEthernet::USBSetMulticastFilter - allocate address buffer failed");
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
        ELG(MER->bRequest, rc, 'USE-', "com_apple_driver_dts_USBCDCEthernet::USBSetMulticastFilter - Error issueing DeviceRequest");
        IOFree(MER->pData, eaddLen);
        IOFree(MER, sizeof(IOUSBDevRequest));
        return false;
    }
    
    return true;

}/* end USBSetMulticastFilter */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::USBSetPacketFilter
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Set up and send SetEthernetPackettFilters Management Element Request(MER).
//
/****************************************************************************************************/

bool com_apple_driver_dts_USBCDCEthernet::USBSetPacketFilter()
{
    IOReturn		rc;
    IOUSBDevRequest	*MER;
	
    ELG(0, fPacketFilter, 'USPF', "com_apple_driver_dts_USBCDCEthernet::USBSetPacketFilter");
	
    MER = (IOUSBDevRequest*)IOMalloc(sizeof(IOUSBDevRequest));
    if (!MER)
    {
        ELG(0, 0, 'USP-', "com_apple_driver_dts_USBCDCEthernet::USBSetPacketFilter - allocate MER failed");
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
        ELG(MER->bRequest, rc, 'USE-', "com_apple_driver_dts_USBCDCEthernet::USBSetPacketFilter - DeviceRequest error");
        if (rc == kIOUSBPipeStalled)
        {

            // Clear the stall and try it once more
        
            fpDevice->GetPipeZero()->ClearPipeStall(false);
            rc = fpDevice->DeviceRequest(MER, &fMERCompletionInfo);
            if (rc != kIOReturnSuccess)
            {
                ELG(MER->bRequest, rc, 'USE-', "com_apple_driver_dts_USBCDCEthernet::USBSetPacketFilter - DeviceRequest, error a second time");
                IOFree(MER, sizeof(IOUSBDevRequest));
                return false;
            }
        }
    }
    
    return true;
    
}/* end USBSetPacketFilter */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::clearPipeStall
//
//		Inputs:		thePipe - the pipe
//
//		Outputs:	
//
//		Desc:		Clear a stall on the specified pipe. All outstanding I/O
//				is returned as aborted.
//
/****************************************************************************************************/

IOReturn com_apple_driver_dts_USBCDCEthernet::clearPipeStall(IOUSBPipe *thePipe)
{
    UInt8	pipeStatus;
    IOReturn 	rtn = kIOReturnSuccess;
    
    ELG(0, thePipe, 'clSt', "com_apple_driver_dts_USBCDCEthernet::clearPipeStall");
    
    pipeStatus = thePipe->GetStatus();
    if (pipeStatus == kPipeStalled)
    {
        rtn = thePipe->ClearPipeStall(true);
        if (rtn == kIOReturnSuccess)
        {
            ELG(0, 0, 'clSS', "com_apple_driver_dts_USBCDCEthernet::clearPipeStall - Successful");
        } else {
            ELG(0, rtn, 'clSF', "com_apple_driver_dts_USBCDCEthernet::clearPipeStall - Failed");
        }
    } else {
        ELG(0, pipeStatus, 'clSP', "com_apple_driver_dts_USBCDCEthernet::clearPipeStall - Pipe not stalled");
    }
    
    return rtn;

}/* end clearPipeStall */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::receivePacket
//
//		Inputs:		packet - the packet
//				size - Number of bytes in the packet
//
//		Outputs:	
//
//		Desc:		Build the mbufs and then send to the network stack.
//
/****************************************************************************************************/

void com_apple_driver_dts_USBCDCEthernet::receivePacket(UInt8 *packet, UInt32 size)
{
    struct mbuf		*m;
    UInt32		submit;
    
    ELG(0, size, 'rcPk', "com_apple_driver_dts_USBCDCEthernet::receivePacket");
    
    if (size > fMax_Block_Size)
    {
        ELG(0, 0, 'rcP-', "com_apple_driver_dts_USBCDCEthernet::receivePacket - Packet size error, packet dropped");
        if (fInputErrsOK)
            fpNetStats->inputErrors++;
        return;
    }
    
    m = allocatePacket(size);
    if (m)
    {
        bcopy(packet, mtod(m, unsigned char *), size);
        submit = fNetworkInterface->inputPacket(m, size);
        ELG(0, submit, 'rcSb', "com_apple_driver_dts_USBCDCEthernet::receivePacket - Packets submitted");
        if (fInputPktsOK)
            fpNetStats->inputPackets++;
    } else {
        ELG(0, 0, 'rcB-', "com_apple_driver_dts_USBCDCEthernet::receivePacket - Buffer allocation failed, packet dropped");
        if (fInputErrsOK)
            fpNetStats->inputErrors++;
    }

}/* end receivePacket */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::timeoutFired
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Static member function called when a timer event fires.
//
/****************************************************************************************************/
void com_apple_driver_dts_USBCDCEthernet::timerFired(OSObject *owner, IOTimerEventSource *sender)
{

//    ELG(0, 0, 'tmFd', "com_apple_driver_dts_USBCDCEthernet::timerFired");
    
    if (owner)
    {
	com_apple_driver_dts_USBCDCEthernet* target = OSDynamicCast(com_apple_driver_dts_USBCDCEthernet, owner);
	
	if (target)
	{
	    target->timeoutOccurred(sender);
	}
    }
    
}/* end timerFired */

/****************************************************************************************************/
//
//		Method:		com_apple_driver_dts_USBCDCEthernet::timeoutOccurred
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Timeout handler, used for stats gathering.
//
/****************************************************************************************************/

void com_apple_driver_dts_USBCDCEthernet::timeoutOccurred(IOTimerEventSource * /*timer*/)
{
    UInt32		*enetStats;
    UInt16		currStat;
    IOReturn		rc;
    IOUSBDevRequest	*STREQ;
    bool		statOk = false;

//    ELG(0, 0, 'tmOd', "com_apple_driver_dts_USBCDCEthernet::timeoutOccurred");

    enetStats = (UInt32 *)&fEthernetStatistics;
    if (*enetStats == 0)
    {
        ELG(0, 0, 'tmN-', "com_apple_driver_dts_USBCDCEthernet::timeoutOccurred - No Ethernet statistics defined");
        return;
    }
    
    if (fReady == false)
    {
        ELG(0, 0, 'tmS-', "com_apple_driver_dts_USBCDCEthernet::timeoutOccurred - Spurious");    
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
                ELG(0, 0, 'tma-', "com_apple_driver_dts_USBCDCEthernet::timeoutOccurred - allocate STREQ failed");
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
                    ELG(STREQ->bRequest, rc, 'tmE-', "com_apple_driver_dts_USBCDCEthernet::timeoutOccurred - Error issueing DeviceRequest");
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
//		Method:		com_apple_driver_dts_USBCDCEthernet::message
//
//		Inputs:		type - message type, provider - my provider, argument - additional parameters
//
//		Outputs:	return Code - kIOReturnSuccess
//
//		Desc:		Handles IOKit messages. 
//
/****************************************************************************************************/

IOReturn com_apple_driver_dts_USBCDCEthernet::message(UInt32 type, IOService *provider, void *argument)
{
    IOReturn	ior;
	
    ELG(0, type, 'mess', "com_apple_driver_dts_USBCDCEthernet::message");
	
    switch (type)
    {
        case kIOMessageServiceIsTerminated:
            ELG(fReady, type, 'mess', "com_apple_driver_dts_USBCDCEthernet::message - kIOMessageServiceIsTerminated");
			
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
			"/System/Library/Extensions/com_apple_driver_dts_USBCDCEthernet.kext",				// localizationPath
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
            return kIOReturnSuccess;			
        case kIOMessageServiceIsSuspended: 	
            ELG(0, type, 'mess', "com_apple_driver_dts_USBCDCEthernet::message - kIOMessageServiceIsSuspended");
            break;			
        case kIOMessageServiceIsResumed: 	
            ELG(0, type, 'mess', "com_apple_driver_dts_USBCDCEthernet::message - kIOMessageServiceIsResumed");
            break;			
        case kIOMessageServiceIsRequestingClose: 
            ELG(0, type, 'mess', "com_apple_driver_dts_USBCDCEthernet::message - kIOMessageServiceIsRequestingClose"); 
            break;
        case kIOMessageServiceWasClosed: 	
            ELG(0, type, 'mess', "com_apple_driver_dts_USBCDCEthernet::message - kIOMessageServiceWasClosed"); 
            break;
        case kIOMessageServiceBusyStateChange: 	
            ELG(0, type, 'mess', "com_apple_driver_dts_USBCDCEthernet::message - kIOMessageServiceBusyStateChange"); 
            break;
        case kIOUSBMessagePortHasBeenResumed: 	
            ELG(0, type, 'mess', "com_apple_driver_dts_USBCDCEthernet::message - kIOUSBMessagePortHasBeenResumed");
            
                // If the reads are dead try and resurrect them
            
            if (fCommDead)
            {
                ior = fCommPipe->Read(fCommPipeMDP, &fCommCompletionInfo, NULL);
                if (ior != kIOReturnSuccess)
                {
                    ELG(0, ior, 'msC-', "com_apple_driver_dts_USBCDCEthernet::message - Failed to queue Comm pipe read");
                } else {
                    fCommDead = false;
                }
            }
            
            if (fDataDead)
            {
                ior = fInPipe->Read(fPipeInMDP, &fReadCompletionInfo, NULL);
                if (ior != kIOReturnSuccess)
                {
                    ELG(0, ior, 'msD-', "com_apple_driver_dts_USBCDCEthernet::message - Failed to queue Data pipe read");
                } else {
                    fDataDead = false;
                }
            }

            break;
        case kIOUSBMessageHubResumePort:
            ELG(0, type, 'mess', "com_apple_driver_dts_USBCDCEthernet::message - kIOUSBMessageHubResumePort");
            break;
        default:
            ELG(0, type, 'mess', "com_apple_driver_dts_USBCDCEthernet::message - unknown message"); 
            break;
    }
    
    return kIOReturnUnsupported;
}/* end message */

