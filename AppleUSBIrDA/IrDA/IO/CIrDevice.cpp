/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
    File:       CIrDevice.cpp

    Contains:   Generic interface to IR hardware.

*/

#include "CIrDevice.h"
#include "IrGlue.h"
#include "IrLAP.h"
#include "AppleIrDA.h"
#include "IrDALog.h"

#if (hasTracing > 0 && hasCIrDeviceTracing > 0) 

enum IrLogCodes
{
    kLogNew = 1,
    kLogFree,
    kLogInit,
    kLogStart,
    kLogStop,
    
    kLogChangeSpeed,
    kLogSetSpeedComplete,
    
    kLogReadComplete,
    kLogPacketDropped,
    
    kLogStartXmit,
    kLogStartXmitPutBuf,
    kLogStartXmitCtlBuf,
    kLogStartXmitCtlSize,
    kLogStartXmitDataBuf,
    kLogStartXmitDataSize,
    kLogStartXmitFrame,
    kLogStartXmitLength,
    kLogStartXmitData,
    kLogTransmitComplete,
    kLogStartXmitError,
    
    kLogSetAddress,
    kLogNotOurAddress,
    
};

static
EventTraceCauseDesc IrLogEvents[] = {
    {kLogNew,                   "IrDevice: new, obj="},
    {kLogFree,                  "IrDevice: free, obj="},
    {kLogInit,                  "IrDevice: init, obj="},
    {kLogStart,                 "IrDevice: start"},
    {kLogStop,                  "IrDevice: stop"},
    
    {kLogChangeSpeed,           "IrDevice: change speed, new="},
    {kLogSetSpeedComplete,      "IrDevice: set speed complete, worked="},
      
    {kLogReadComplete,          "IrDevice: read complete, len=,data="},
    {kLogPacketDropped,         "IrDevice: packet dropped.  no read pending from lap"},
    
    {kLogStartXmit,             "IrDevice: start xmit"},
    {kLogStartXmitPutBuf,       "IrDevice: xmit put buffer"},
    {kLogStartXmitCtlBuf,       "IrDevice: xmit ctl buffer"},
    {kLogStartXmitCtlSize,      "IrDevice: xmit ctl length"},
    {kLogStartXmitDataBuf,      "IrDevice: xmit data buffer"},
    {kLogStartXmitDataSize,     "IrDevice: xmit data length"},
    {kLogStartXmitFrame,        "IrDevice: xmit frame buffer"},
    {kLogStartXmitLength,       "IrDevice: xmit packet length"},
    {kLogStartXmitData,         "IrDevice: xmit packet data"},
    {kLogTransmitComplete,      "IrDevice: transmit complete"},
    {kLogStartXmitError,        "IrDevice: start xmit logic ERROR. fXmitting="},
    
    {kLogSetAddress,            "IrDevice: set lap address"},
    {kLogNotOurAddress,         "IrDevice: not our lap address, in=, our="},
    
};

#define XTRACE(x, y, z) IrDALogAdd( x, y, z, IrLogEvents, true )

#else
#define XTRACE(x,y,z)((void)0)
#endif

#define super OSObject
    OSDefineMetaClassAndStructors(CIrDevice, OSObject);

/*static*/
CIrDevice *
CIrDevice::cIrDevice(TIrGlue *irda, AppleIrDASerial *driver)
{
    CIrDevice *obj = new CIrDevice;
    XTRACE(kLogNew, (int)obj >> 16, (short)obj);
    if (obj && !obj->Init(irda, driver)) {
	obj->release();
	obj = nil;
    }
    return obj;
}

void
CIrDevice::free(void)
{
    XTRACE(kLogFree, (int)this >> 16, (short)this);
    super::free();
}

Boolean
CIrDevice::Init(TIrGlue *irda, AppleIrDASerial *driver)
{
    XTRACE(kLogInit, (int)this >> 16, (short)this);
    
    
    fIrDA = nil;
    fDriver = nil;
    fGetBuffer = nil;
    bzero(&fIrStatus, sizeof(fIrStatus));
    fLAPAddr = 0;
    fSpeed = 0;
    fBofs = 0;
    fXmitting = false;                  // true if in middle of xmit
    
    if (!super::init()) return false;
    
    fIrDA = irda;
    fDriver = driver;
    
    ResetStats();
    return true;
}

void
CIrDevice::Start(void)
{
    XTRACE(kLogStart, 0, 0);
    ChangeSpeed(9600);  // set to default speed
}

void
CIrDevice::Stop(void)
{
    XTRACE(kLogStop, 0, 0);
    
    fDriver = nil;
}

void
CIrDevice::SetLAPAddress(UInt8 addr)            // Set our LAP address
{
    XTRACE(kLogSetAddress, 0, addr);
    fLAPAddr = addr;
}


//--------------------------------------------------------------------------------
//      ValidFrameAddress
//
// Check the lap address field for either broadcast, or our address
//--------------------------------------------------------------------------------
Boolean
CIrDevice::ValidFrameAddress(UInt8 aField)
{
    Boolean rc =  ((aField >> 1) == kIrLAPBroadcastAddr) || ((aField >> 1) == fLAPAddr);
    if (!rc) {
	XTRACE(kLogNotOurAddress, aField, fLAPAddr);
    }
    return rc;
}

//--------------------------------------------------------------------------------
//      ChangeSpeed
//--------------------------------------------------------------------------------
IrDAErr CIrDevice::ChangeSpeed(unsigned long bps)
{
    XTRACE(kLogChangeSpeed, bps >> 16, (short)bps);
    require(fDriver, Fail);
    check(fXmitting == false);
	
    if (bps != fSpeed) {
	(void) fDriver->SetSpeed(bps);      // start the speed change sequence
	fSpeed = bps;
    }
    
    /***
    if (1) {        // doing this right again finally
	TIrLAP *lap;
	require(fIrDA, Fail);
	lap = fIrDA->GetLAP();
	require(lap, Fail);
	lap->ChangeSpeedComplete();     // finally tell lap that the speed change finished
    }
    ***/
    return noErr;

Fail:
    return kIrDAErrWrongState;
    
} // CIrDevice::ChangeSpeed

void
CIrDevice::SetSpeedComplete(Boolean worked)
{
    TIrLAP *lap;
    XTRACE(kLogSetSpeedComplete, 0, worked);
    require(fIrDA, Fail);
    lap = fIrDA->GetLAP();
    require(lap, Fail);
    lap->ChangeSpeedComplete();

Fail:
    return;
}

//--------------------------------------------------------------------------------
//      GetMediaBusy
//--------------------------------------------------------------------------------
Boolean CIrDevice::GetMediaBusy(void)
{
    // return fDevice->GetMediaBusy();
    return false;
}

//--------------------------------------------------------------------------------
//      ResetMediaBusy
//--------------------------------------------------------------------------------
void CIrDevice::ResetMediaBusy(void)
{
    // fDevice->ResetMediaBusy();
    return;
}

//--------------------------------------------------------------------------------
//      StartReceive
//--------------------------------------------------------------------------------
void CIrDevice::StartReceive(CBufferSegment* inputBuffer)
{
//  check(fGetBuffer == nil);       // see if a read is "pending" already
    fGetBuffer = inputBuffer;
}

//--------------------------------------------------------------------------------
//      StopReceive
//--------------------------------------------------------------------------------
void CIrDevice::StopReceive(void)
{
    //check(fGetBuffer != nil);     // this happens
    fGetBuffer = nil;
}

//--------------------------------------------------------------------------------
//      ReadComplete
//--------------------------------------------------------------------------------
void CIrDevice::ReadComplete(UInt8 *buffer, UInt32 length)
{
    XTRACE(kLogReadComplete, 0, length);
    UByte aField, cField;
    
    // if debugging really high, log data bytes too
#if (hasTracing > 0 && hasCIrDeviceTracing > 1)
    {
	int len = length;
	UInt32 w;
	UInt8 *b = buffer;
	int i;
	
	while (len > 0) {
	    w = 0;
	    for (i = 0 ; i < 4; i++) {
		w = w << 8;
		if (len > 0)            // don't run off end (pad w/zeros)
		    w = w | *b;
		b++;
		len--;
	    }
	    XTRACE(kLogReadComplete, w >> 16, (short)w);
	}
    }

#endif // hasCIrDeviceTracing > 1

    // test for data recvd but LAP doesn't have a read pending
    if (fGetBuffer == nil) {
	XTRACE(kLogPacketDropped, 0, 0);
	Stats_PacketDropped();              // different bucket for invalid sequence vs. this?
	return;
    }

    require(length >= 2, Fail);
    
    aField = buffer[0];
    cField = buffer[1];
    length -= 2;
    buffer += 2;
    
    if (length > 0) {
	fGetBuffer->Putn(buffer, length);       // copy to CBufferSegment
	fGetBuffer->Hide(fGetBuffer->GetSize() - fGetBuffer->Position(), kPosEnd);
	// Seek to beginning of the buffer for our client
	fGetBuffer->Seek(0, kPosBeg);
    }
    
    if (ValidFrameAddress(aField)) {                        // if packet addressed to us
	Stats_DataPacketIn();
	fGetBuffer = nil;                                   // read is done, clear our "read pending" flag
	fIrDA->GetLAP()->InputComplete(aField,cField);      // then let LAP know about it (already owns "fGetBuffer")
    }
    else {
	int review_media_busy_logic;
    //  fMediaBusy = true;                                  // else did we just saw someone else's traffic?
	Stats_PacketDropped();                              // different counters?
    }
Fail:
    return;
    
} // CIrDevice::ReadComplete

//--------------------------------------------------------------------------------
//      StartTransmit
//--------------------------------------------------------------------------------
void CIrDevice::StartTransmit(TIrLAPPutBuffer* outputBuffer, ULong leadInCount)
{
    UByte   *ctlBuffer;
    UByte   *dataBuffer;
    int     ctlSize;
    int     dataSize;
    IOReturn    ior;
    
    require(fDriver, Fail);
    require(outputBuffer, Fail);
    require(fXmitting == false, Fail);
	
    fXmitting = true;
    
    if (fBofs != leadInCount) {
	fDriver->SetBofCount(leadInCount);      // cache and call only if different?
	fBofs = leadInCount;
    }

    XTRACE(kLogStartXmitPutBuf, (int)outputBuffer >> 16, (short)outputBuffer);
    
    ctlBuffer  = outputBuffer->GetCtrlBuffer();
    XTRACE(kLogStartXmitCtlBuf, (int)ctlBuffer >> 16, (short)ctlBuffer);
    
    ctlSize    = outputBuffer->GetCtrlSize();
    XTRACE(kLogStartXmitCtlSize, (int)ctlSize >> 16, (short)ctlSize);
    
    dataBuffer = outputBuffer->GetDataBuffer();
    XTRACE(kLogStartXmitDataBuf, (int)dataBuffer >> 16, (short)dataBuffer);
    
    dataSize   = outputBuffer->GetDataSize();
    XTRACE(kLogStartXmitDataSize, (int)dataSize >> 16, (short)dataSize);
	
    //require(ctlSize + dataSize <= LapLength, Fail);
			
#if (hasTracing > 0 && hasCIrDeviceTracing > 1)
    if (1) {
	int len = ctlSize + dataSize;   // dump control and data buffers
	UInt32 w;
	UInt8 *b = ctlBuffer;           // start with control buffer, then switch to data buffer
	int i;
	
	XTRACE(kLogStartXmitLength, len >> 16, (short)len);
	
	require(len > 0 && ctlSize > 0 && ctlBuffer != nil, Fail);  // sanity (assumes non-empty ctlbuffer)
	
	while (len > 0) {               // loop over the packet
	    w = 0;                      // logging 4 bytes at a time
	    for (i = 0 ; i < 4; i++) {
		w = w << 8; 
		if (len > 0)            // don't run off end (pad w/zeros)
		    w = w | *b;
		b++;
		if (b == &ctlBuffer[ctlSize])   // if just incr'd past end of control buffer
		    b = dataBuffer;             //  then switch to data buffer
		len--;
	    }
	    XTRACE(kLogStartXmitData, w >> 16, (short)w);
	}
    }
#endif // hasCIrDeviceTracing > 1

    // start the transmit
    ior = fDriver->StartTransmit(ctlSize, ctlBuffer, dataSize, dataBuffer);
    if (ior != kIOReturnSuccess) {
	XTRACE(kLogStartXmitError, ior >> 16, (short)ior);
	TransmitComplete(false);
	return;
    }
    XTRACE(kLogStartXmitData, 0xffff, 0xffff);
    return;

Fail:
    XTRACE(kLogStartXmitError, 0, fXmitting);
    return;
}

void CIrDevice::TransmitComplete(Boolean worked)
{   
    TIrLAP *lap;

    XTRACE(kLogTransmitComplete, 0, worked);
    check(fXmitting);
    
    fXmitting = false;
    require(fIrDA, Fail);
    
    lap = fIrDA->GetLAP();
    require(lap, Fail);
    Stats_DataPacketOut();
    
    lap->OutputComplete();      // what to do if the write failed?
    
Fail:
    return;
}

//--------------------------------------------------------------------------------
//      ResetStats
//--------------------------------------------------------------------------------
void CIrDevice::ResetStats()
{
    bzero(&fIrStatus, sizeof(fIrStatus));
}
