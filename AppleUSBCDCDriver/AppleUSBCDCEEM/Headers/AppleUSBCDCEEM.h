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

#ifndef __APPLEUSBCDCEEM__
#define __APPLEUSBCDCEEM__

#include "AppleUSBCDCCommon.h"
#include "AppleUSBCDC.h"  

#define LDEBUG		0			// for debugging
#define USE_ELG		0			// to Event LoG (via kprintf and Firewire) - LDEBUG must also be set
#define USE_IOL		0			// to IOLog - LDEBUG must also be set
#define	LOG_DATA	0			// logs data to the appropriate log - LDEBUG must also be set
#define DUMPALL		0			// Dumps all the data to the log - LOG_DATA must also be set

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
    #if LOG_DATA
        #define LogData(D, C, b)	USBLogData((UInt8)D, (UInt32)C, (char *)b)
        #define meLogData(D, C, b)	me->USBLogData((UInt8)D, (UInt32)C, (char *)b)
        #define DumpData(b, C)		dumpData(char *)b, (SInt32)C)
        #define meDumpData(b, C)	me->dumpData(char *)b, (SInt32)C)
    #else /* not LOG_DATA */
        #define LogData(D, C, b)
        #define meLogData(D, C, b)
        #define DumpData(b, C)
        #define meDumpData(b, C)
    #endif /* LOG_DATA */
#else /* not LDEBUG */
    #define XTRACE(id, x, y, msg)
    #define LogData(D, C, b)
    #define meLogData(D, C, b)
    #define DumpData(b, C)
    #define meDumpData(b, C)
    #undef USE_ELG
    #undef USE_IOL
    #undef LOG_DATA
#endif /* LDEBUG */

#define ALERT(A,B,STRING)	Log("%8x %8x " DEBUG_NAME ": " STRING "\n", (unsigned int)(A), (unsigned int)(B))

enum
{
    kDataIn			= 0,
    kDataOut,
    kDataOther,
    kDataNone
};

#define TRANSMIT_QUEUE_SIZE     256				// How does this relate to MAX_BLOCK_SIZE?
#define WATCHDOG_TIMER_MS       1000

#define MAX_BLOCK_SIZE		PAGE_SIZE
#define COMM_BUFF_SIZE		16

#define nameLength		32				// Arbitrary length
#define defaultName		"USB EEM"

#define kFiltersSupportedMask	0xefff
#define kPipeStalled		1

    // Default and Maximum buffer pool values

#define kInBufPool		4
#define kOutBufPool		2

#define kMaxInBufPool		kInBufPool*16
#define kMaxOutBufPool		kOutBufPool*8

#define	inputTag		"InputBuffers"
#define	outputTag		"OutputBuffers"

typedef struct 
{
    IOBufferMemoryDescriptor	*pipeOutMDP;
    UInt8			*pipeOutBuffer;
    mbuf_t			m;
    bool			avail;
    IOUSBCompletion		writeCompletionInfo;
	UInt32			indx;
} pipeOutBuffers;

typedef struct 
{
    IOBufferMemoryDescriptor	*pipeInMDP;
    UInt8			*pipeInBuffer;
    bool			dead;
    IOUSBCompletion		readCompletionInfo;
	UInt32			indx;
} pipeInBuffers;

	// EEM bit definitions and masks
	
#define bmTypeData			0x0000
#define bmTypeCommand		0x8000
#define bmCRC				0x4000

	// EEM Data packet masks

#define bmCRCMask			0x7fff
#define frameLenMask		0x3fff

	// EEM Command packet masks
	
#define bmEEMCmdMask		0x3fff
#define	bmEEMCmdParamMask	0x07ff

	// EEM Commands
	
#define EEMEcho					0x00
#define EEMEchoResponse			0x08
#define EEMSuspendHint			0x10
#define EEMResponseHint			0x18
#define EEMResponseCompleteHint	0x20
#define EEMTickle				0x28

class AppleUSBCDC;

class AppleUSBCDCEEM : public IOEthernetController
{
    OSDeclareDefaultStructors(AppleUSBCDCEEM);	// Constructor & Destructor stuff

private:
    bool			fTerminate;				// Are we being terminated (ie the device was unplugged)
    UInt16			fVendorID;
    UInt16			fProductID;
        
    IOEthernetInterface		*fNetworkInterface;
    IOBasicOutputQueue		*fTransmitQueue;
    
    OSDictionary		*fMediumDict;

    bool			fReady;
    bool			fNetifEnabled;
    bool			fWOL;
    UInt8			fLinkStatus;
    
    IOUSBPipe			*fInPipe;
    IOUSBPipe			*fOutPipe;
    
    pipeInBuffers		fPipeInBuff[kMaxInBufPool];
    pipeOutBuffers		fPipeOutBuff[kMaxOutBufPool];
    UInt16			fOutPoolIndex;
    
    UInt32			fCount;
    UInt32			fOutPacketSize;

    static void			dataReadComplete(void *obj, void *param, IOReturn ior, UInt32 remaining);
    static void			dataWriteComplete(void *obj, void *param, IOReturn ior, UInt32 remaining);
    
           // CDC EEM Driver instance Methods
	
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
    IOReturn		USBTransmitPacket(mbuf_t packet);
	bool			getOutputBuffer(UInt32 *bufIndx);
	IOReturn		USBSendCommand(UInt16 command, UInt16 length, UInt8 *anyData);
    IOReturn		clearPipeStall(IOUSBPipe *thePipe);
    void			receivePacket(UInt8 *packet, UInt32 size);
	void			processEEMCommand(UInt16 EEMHeader, UInt32 poolIndx, SInt16 dataIndx, SInt16 *len);
    
public:

    IOUSBInterface		*fDataInterface;
    IOWorkLoop			*fWorkLoop;
    IOLock			*fBufferPoolLock;
    UInt8			fDataInterfaceNumber;
    
    UInt16			fInBufPool;
    UInt16			fOutBufPool;
	
	UInt16			fMax_Block_Size;
    
    UInt8			fConfigAttributes;
    
    IONetworkStats		*fpNetStats;
    IOEthernetStats		*fpEtherStats;

        // IOKit methods
        
	virtual IOService   *probe(IOService *provider, SInt32 *score);
    virtual bool		init(OSDictionary *properties = 0);
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
    virtual IOReturn		setMulticastMode(IOEnetMulticastMode mode);
    virtual IOReturn		setMulticastList(IOEthernetAddress *addrs, UInt32 count);
    virtual IOReturn		setPromiscuousMode(IOEnetPromiscuousMode mode);
    virtual IOOutputQueue	*createOutputQueue(void);
    virtual const OSString	*newVendorString(void) const;
    virtual const OSString	*newModelString(void) const;
    virtual const OSString	*newRevisionString(void) const;
    virtual bool		configureInterface(IONetworkInterface *netif);
												
}; /* end class AppleUSBCDCEEM */
#endif