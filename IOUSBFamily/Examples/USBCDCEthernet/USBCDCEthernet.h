/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
    File:		USBCDCEthernet.h
	
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
        
*/

	/*****	For fans of kprintf, IOLog and debugging infrastructure of the	*****/
	/*****	string ilk, please modify the ELG and PAUSE macros or their	*****/
	/*****	associated EvLog and Pause functions to suit your taste. These	*****/
	/*****	macros currently are set up to log events to a wraparound	*****/
	/*****	buffer with minimal performance impact. They take 2 UInt32	*****/
	/*****	parameters so that when the buffer is dumped 16 bytes per line,	*****/
	/*****	time stamps (~1 microsecond) run down the left side while	*****/
	/*****	unique 4-byte ASCII codes can be read down the right side.	*****/
	/*****	Preserving this convention facilitates different maintainers	*****/
	/*****	using different debugging styles with minimal code clutter.	*****/
        
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

#define LDEBUG		0			// for debugging
#define USE_ELG		0			// to event log - LDEBUG must also be set
#define kEvLogSize  (4096*16)			// 16 pages = 64K = 4096 events
#define	LOG_DATA	0			// logs data to the IOLog - LDEBUG must also be set

#define Sleep_Time	20

#if LDEBUG
    #if USE_ELG
        #define ELG(A,B,ASCI,STRING)    EvLog((UInt32)(A), (UInt32)(B), (UInt32)(ASCI), STRING)		
    #else /* not USE_ELG */
        #define ELG(A,B,ASCI,STRING)	{IOLog("com_apple_driver_dts_USBCDCEthernet: %8x %8x " STRING "\n", (unsigned int)(A), (unsigned int)(B));IOSleep(Sleep_Time);}
    #endif /* USE_ELG */
    #if LOG_DATA
        #define LogData(D, C, b)	USBLogData((UInt8)D, (UInt32)C, (char *)b)
    #else /* not LOG_DATA */
        #define LogData(D, C, b)
    #endif /* LOG_DATA */
#else /* not LDEBUG */
    #define ELG(A,B,ASCI,S)
    #define LogData(D, C, b)
    #undef USE_ELG
    #undef LOG_DATA
#endif /* LDEBUG */

#define ALERT(A,B,ASCI,STRING)	IOLog("com_apple_driver_dts_USBCDCEthernet: %8x %8x " STRING "\n", (unsigned int)(A), (unsigned int)(B))

#define TRANSMIT_QUEUE_SIZE     256				// How does this relate to MAX_BLOCK_SIZE?
#define WATCHDOG_TIMER_MS       1000

#define MAX_BLOCK_SIZE		PAGE_SIZE
#define COMM_BUFF_SIZE		16

#define nameLength		32				// Arbitrary length
#define defaultName		"USB Ethernet"

#define kFiltersSupportedMask	0xefff
#define kPipeStalled		1

#define kOutBufPool		6
#define kOutBuffThreshold	100

        // USB CDC Definitions (Ethernet Control Model)
		
#define kEthernetControlModel	6		

    //	Requests

enum
{
    kSend_Encapsulated_Command		= 0,
    kGet_Encapsulated_Response		= 1,
    kSet_Ethernet_Multicast_Filter	= 0x40,
    kSet_Ethernet_PM_Packet_Filter	= 0x41,
    kGet_Ethernet_PM_Packet_Filter	= 0x42,
    kSet_Ethernet_Packet_Filter		= 0x43,
    kGet_Ethernet_Statistics		= 0x44,
    kGet_AUX_Inputs			= 4,
    kSet_AUX_Outputs			= 5,
    kSet_Temp_MAC			= 6,
    kGet_Temp_MAC			= 7,
    kSet_URB_Size			= 8,
    kSet_SOFS_To_Wait			= 9,
    kSet_Even_Packets			= 10,
    kScan				= 0xFF
};

    // Notifications

enum
{
    kNetwork_Connection			= 0,
    kResponse_Available			= 1,
    kConnection_Speed_Change		= 0x2A
};

enum
{
    CS_INTERFACE			= 0x24,
		
    Header_FunctionalDescriptor		= 0x00,
    CM_FunctionalDescriptor		= 0x01,
    Union_FunctionalDescriptor		= 0x06,
    CS_FunctionalDescriptor		= 0x07,
    Enet_Functional_Descriptor		= 0x0f,
		
    CM_ManagementData			= 0x01,
    CM_ManagementOnData			= 0x02
};

    // Stats of interest in bmEthernetStatistics (bit definitions)

enum
{
    kXMIT_OK =			0x01,		// Byte 1
    kRCV_OK =			0x02,
    kXMIT_ERROR =		0x04,
    kRCV_ERROR =		0x08,

    kRCV_CRC_ERROR =		0x02,		// Byte 3
    kRCV_ERROR_ALIGNMENT =	0x08,
    kXMIT_ONE_COLLISION =	0x10,
    kXMIT_MORE_COLLISIONS =	0x20,
    kXMIT_DEFERRED =		0x40,
    kXMIT_MAX_COLLISION =	0x80,

    kRCV_OVERRUN =		0x01,		// Byte 4
    kXMIT_TIMES_CARRIER_LOST =	0x08,
    kXMIT_LATE_COLLISIONS =	0x10
};

    // Stats request definitions
  
enum
{
    kXMIT_OK_REQ =			0x0001,
    kRCV_OK_REQ =			0x0002,
    kXMIT_ERROR_REQ =			0x0003,
    kRCV_ERROR_REQ =			0x0004,

    kRCV_CRC_ERROR_REQ =		0x0012,
    kRCV_ERROR_ALIGNMENT_REQ =		0x0014,
    kXMIT_ONE_COLLISION_REQ =		0x0015,
    kXMIT_MORE_COLLISIONS_REQ =		0x0016,
    kXMIT_DEFERRED_REQ =		0x0017,
    kXMIT_MAX_COLLISION_REQ =		0x0018,

    kRCV_OVERRUN_REQ =			0x0019,
    kXMIT_TIMES_CARRIER_LOST_REQ =	0x001c,
    kXMIT_LATE_COLLISIONS_REQ =		0x001d
};

    // Packet Filter definitions
  
enum
{
    kPACKET_TYPE_PROMISCUOUS =		0x0001,
    kPACKET_TYPE_ALL_MULTICAST =	0x0002,
    kPACKET_TYPE_DIRECTED =		0x0004,
    kPACKET_TYPE_BROADCAST =		0x0008,
    kPACKET_TYPE_MULTICAST =		0x0010
};

typedef struct 
{
    UInt8	bFunctionLength;
    UInt8 	bDescriptorType;
    UInt8 	bDescriptorSubtype;
} HeaderFunctionalDescriptor;

typedef struct 
{
    UInt8	bFunctionLength;
    UInt8 	bDescriptorType;
    UInt8 	bDescriptorSubtype;
    UInt8 	bmCapabilities;
    UInt8 	bDataInterface;
} CMFunctionalDescriptor;
	
typedef struct
{
    UInt8 	bFunctionLength;
    UInt8 	bDescriptorType;
    UInt8 	bDescriptorSubtype;
    UInt8 	iMACAddress;
    UInt8 	bmEthernetStatistics[4];
    UInt8 	wMaxSegmentSize[2];
    UInt8 	wNumberMCFilters[2];
    UInt8 	bNumberPowerFilters;
} EnetFunctionalDescriptor;

typedef struct 
{
    UInt8	bFunctionLength;
    UInt8 	bDescriptorType;
    UInt8 	bDescriptorSubtype;
    UInt8 	bMasterInterface;
    UInt8	bSlaveInterface[];
} UnionFunctionalDescriptor;

typedef struct 
{
    IOBufferMemoryDescriptor	*pipeOutMDP;
    UInt8			*pipeOutBuffer;
    struct mbuf			*m;
} pipeOutBuffers;

    // Globals

typedef struct globals      // Globals for this module (not per instance)
{
    UInt32      evLogFlag; 				// debugging only
    UInt8       *evLogBuf;
    UInt8       *evLogBufe;
    UInt8       *evLogBufp;
    UInt8       intLevel;
    class com_apple_driver_dts_USBCDCEthernet	*USBCDCEthernetInstance;
} globals;
	
    // Inline time conversions
	
static inline unsigned long tval2long(mach_timespec val)
{
   return (val.tv_sec * NSEC_PER_SEC) + val.tv_nsec;   
}

static inline mach_timespec long2tval(unsigned long val)
{
    mach_timespec	tval;

    tval.tv_sec  = val / NSEC_PER_SEC;
    tval.tv_nsec = val % NSEC_PER_SEC;
    return tval;	
}

class com_apple_driver_dts_USBCDCEthernet : public IOEthernetController
{
    OSDeclareDefaultStructors(com_apple_driver_dts_USBCDCEthernet);	// Constructor & Destructor stuff

private:
    bool			fTerminate;				// Are we being terminated (ie the device was unplugged)
    UInt8			fbmAttributes;				// Device attributes
    UInt16			fVendorID;
    UInt16			fProductID;
        
    IOEthernetInterface		*fNetworkInterface;
    IOBasicOutputQueue		*fTransmitQueue;

    IOWorkLoop			*fWorkLoop;
    IONetworkStats		*fpNetStats;
    IOEthernetStats		*fpEtherStats;
    IOTimerEventSource		*fTimerSource;
    
    OSDictionary		*fMediumDict;

    bool			fReady;
    bool			fNetifEnabled;
    bool			fWOL;
    bool			fDataDead;
    bool			fCommDead;
    UInt8			fLinkStatus;
    UInt32			fUpSpeed;
    UInt32			fDownSpeed;
    UInt16			fPacketFilter;
     
    IOUSBInterface		*fCommInterface;
    IOUSBInterface		*fDataInterface;
    
    IOUSBPipe			*fInPipe;
    IOUSBPipe			*fOutPipe;
    IOUSBPipe			*fCommPipe;
    
    IOBufferMemoryDescriptor	*fCommPipeMDP;
    IOBufferMemoryDescriptor	*fPipeInMDP;

    UInt8			*fCommPipeBuffer;
    UInt8			*fPipeInBuffer;
    
    pipeOutBuffers		fPipeOutBuff[kOutBufPool];
    
    UInt8			fCommInterfaceNumber;
    UInt8			fDataInterfaceNumber;
    UInt32			fCount;
    UInt32			fOutPacketSize;
    
    UInt8			fEaddr[6];
    UInt16			fMax_Block_Size;
    UInt16			fMcFilters;
    UInt8 			fEthernetStatistics[4];
    
    UInt16			fCurrStat;
    UInt32			fStatValue;
    bool			fStatInProgress;
    bool			fInputPktsOK;
    bool			fInputErrsOK;
    bool			fOutputPktsOK;
    bool			fOutputErrsOK;

    IOUSBCompletion		fCommCompletionInfo;
    IOUSBCompletion		fReadCompletionInfo;
    IOUSBCompletion		fWriteCompletionInfo;
    IOUSBCompletion		fMERCompletionInfo;
    IOUSBCompletion		fStatsCompletionInfo;

    static void			commReadComplete(void *obj, void *param, IOReturn ior, UInt32 remaining);
    static void			dataReadComplete(void *obj, void *param, IOReturn ior, UInt32 remaining);
    static void			dataWriteComplete(void *obj, void *param, IOReturn ior, UInt32 remaining);
    static void			merWriteComplete(void *obj, void *param, IOReturn ior, UInt32 remaining);
    static void			statsWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining);
    
           // CDC Driver instance Methods
	
    bool			wakeUp(void);
    void			putToSleep(void);
    bool			createMediumTables(void);
    bool 			allocateResources(void);
    void			releaseResources(void);
    bool 			configureDevice(UInt8 numConfigs);
    bool			initDevice(UInt8 numConfigs);
    bool			getFunctionalDescriptors(void);
    bool			createNetworkInterface(void);
    UInt32			outputPacket(struct mbuf *pkt, void *param);
    bool			USBTransmitPacket(struct mbuf *packet);
    bool			USBSetMulticastFilter(IOEthernetAddress *addrs, UInt32 count);
    bool			USBSetPacketFilter(void);
    IOReturn			clearPipeStall(IOUSBPipe *thePipe);
    void			receivePacket(UInt8 *packet, UInt32 size);
    static void 		timerFired(OSObject *owner, IOTimerEventSource *sender);
    void			timeoutOccurred(IOTimerEventSource *timer);

public:

    IOUSBDevice			*fpDevice;

        // IOKit methods
        
    virtual bool		init(OSDictionary *properties = 0);
    virtual bool		start(IOService *provider);
    virtual void		free(void);
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
												
}; /* end class com_apple_driver_dts_USBCDCEthernet */
