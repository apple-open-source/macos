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

#ifndef __APPLEUSBCDCECMData__
#define __APPLEUSBCDCECMData__

#include "AppleUSBCDCCommon.h"
#include "AppleUSBCDC.h"
#include "AppleUSBCDCECMControl.h"

#define TRANSMIT_QUEUE_SIZE     256				// How does this relate to MAX_BLOCK_SIZE?
#define WATCHDOG_TIMER_MS       1000

#define MAX_BLOCK_SIZE		PAGE_SIZE
#define COMM_BUFF_SIZE		16

#define nameLength		32				// Arbitrary length
#define defaultName		"USB Ethernet"

#define kFiltersSupportedMask	0xefff
#define kPipeStalled		1

    // Default and Maximum buffer pool values

#define kInBufPool		4
#define kOutBufPool		8

#define kMaxInBufPool		kInBufPool*16
#define kMaxOutBufPool		kOutBufPool*16

#define	inputTag		"InputBuffers"
#define	outputTag		"OutputBuffers"

typedef struct 
{
    IOBufferMemoryDescriptor	*pipeOutMDP;
    UInt8			*pipeOutBuffer;
	mbuf_t			m;
    bool			avail;
    IOUSBCompletion		writeCompletionInfo;
} pipeOutBuffers;

typedef struct 
{
    IOBufferMemoryDescriptor	*pipeInMDP;
    UInt8			*pipeInBuffer;
    bool			dead;
    IOUSBCompletion		readCompletionInfo;
} pipeInBuffers;

class AppleUSBCDC;
class AppleUSBCDCECMControl;

class AppleUSBCDCECMData : public IOEthernetController
{
    OSDeclareDefaultStructors(AppleUSBCDCECMData);	// Constructor & Destructor stuff

private:
    bool			fTerminate;				// Are we being terminated (ie the device was unplugged)
    UInt16			fVendorID;
    UInt16			fProductID;
        
    IOEthernetInterface		*fNetworkInterface;
    IOBasicOutputQueue		*fTransmitQueue;

    IOTimerEventSource		*fTimerSource;
    
    OSDictionary		*fMediumDict;

    bool			fNetifEnabled;
    bool			fWOL;
    UInt8			fLinkStatus;
	bool			fSleeping;
    
    IOUSBPipe			*fInPipe;
    IOUSBPipe			*fOutPipe;
    
    pipeInBuffers		fPipeInBuff[kMaxInBufPool];
    pipeOutBuffers		fPipeOutBuff[kMaxOutBufPool];
    UInt16			fOutPoolIndex;
    
    UInt8			fCommInterfaceNumber;
    UInt32			fCount;
    UInt32			fOutPacketSize;

    static void			dataReadComplete(void *obj, void *param, IOReturn ior, UInt32 remaining);
    static void			dataWriteComplete(void *obj, void *param, IOReturn ior, UInt32 remaining);
    
           // CDC Driver instance Methods
	
    void			USBLogData(UInt8 Dir, SInt32 Count, char *buf);
    void			dumpData(char *buf, SInt32 size);
    bool			configureData(void);
    bool			wakeUp(void);
    void			putToSleep(void);
    bool			createMediumTables(void);
    bool 			allocateResources(void);
    void			releaseResources(void);
    bool			createNetworkInterface(void);
    UInt32			outputPacket(mbuf_t pkt, void *param);
	bool			getOutputBuffer(UInt32 *bufIndx);
    IOReturn		USBTransmitPacket(mbuf_t packet);
    IOReturn		clearPipeStall(IOUSBPipe *thePipe);
    void			receivePacket(UInt8 *packet, UInt32 size);
    static void 	timerFired(OSObject *owner, IOTimerEventSource *sender);
    void			timeoutOccurred(IOTimerEventSource *timer);

public:

	AppleUSBCDCECMControl		*fControlDriver;			// Our Control driver
    IOUSBInterface		*fDataInterface;
    IOWorkLoop			*fWorkLoop;
    IOLock			*fBufferPoolLock;
    UInt8			fDataInterfaceNumber;
    
    UInt16			fInBufPool;
    UInt16			fOutBufPool;
    
    UInt8			fConfigAttributes;
    UInt8			fEthernetaddr[6];
	
	bool			fReady;
	UInt8			fResetState;
    
    IONetworkStats		*fpNetStats;
    IOEthernetStats		*fpEtherStats;

        // IOKit methods
        
    virtual bool		init(OSDictionary *properties = 0);
	virtual IOService   *probe(IOService *provider, SInt32 *score);
    virtual bool		start(IOService *provider);
    virtual void		stop(IOService *provider);
    virtual IOReturn 		message(UInt32 type, IOService *provider, void *argument = 0);

        // IOEthernetController methods

    virtual IOReturn		enable(IONetworkInterface *netif);
    virtual IOReturn		disable(IONetworkInterface *netif);
    virtual IOReturn		setWakeOnMagicPacket(bool active);
    virtual IOReturn		getPacketFilters(const OSSymbol	*group, UInt32 *filters ) const;
    virtual IOReturn		selectMedium(const IONetworkMedium *medium);
    virtual IOReturn		getHardwareAddress(IOEthernetAddress *addr);
	virtual IOReturn		getMaxPacketSize(UInt32 *maxSize) const;
    virtual IOReturn		setMulticastMode(IOEnetMulticastMode mode);
    virtual IOReturn		setMulticastList(IOEthernetAddress *addrs, UInt32 count);
    virtual IOReturn		setPromiscuousMode(IOEnetPromiscuousMode mode);
    virtual IOOutputQueue	*createOutputQueue(void);
    virtual const OSString	*newVendorString(void) const;
    virtual const OSString	*newModelString(void) const;
    virtual const OSString	*newRevisionString(void) const;
    virtual bool			configureInterface(IONetworkInterface *netif);
	virtual IOReturn		registerWithPolicyMaker(IOService *policyMaker);
												
}; /* end class AppleUSBCDCECMData */
#endif