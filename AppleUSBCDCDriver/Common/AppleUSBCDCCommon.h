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
 
#ifndef __APPLEUSBCDCCOMMON__
#define __APPLEUSBCDCCOMMON__

#define VersionNumber   "4.1.17"

    // USB CDC Common Defintions
		
#define kUSBAbstractControlModel	2
#define kUSBEthernetControlModel	6
#define kUSBWirelessHandsetControlModel	8
#define kUSBDeviceManagementModel	9
#define kUSBMobileDirectLineModel	10
#define kUSBOBEXModel			11
#define kUSBEthernetEmulationModel  12

#define kUSBv25				1
#define kUSBv25PCCA			2
#define kUSBv25PCCAO			3
#define kUSBv25GSM			4
#define kUSBv253GPPP			5
#define kUSBv25CS			6

enum
{
    kUSBSEND_ENCAPSULATED_COMMAND 	= 0,			// Requests
    kUSBGET_ENCAPSULATED_RESPONSE 	= 1,
    kUSBSET_COMM_FEATURE 		= 2,
    kUSBGET_COMM_FEATURE 		= 3,
    kUSBCLEAR_COMM_FEATURE 		= 4,
    kUSBSET_LINE_CODING 		= 0x20,
    kUSBGET_LINE_CODING 		= 0x21,
    kUSBSET_CONTROL_LINE_STATE 		= 0x22,
    kUSBSEND_BREAK 			= 0x23
};

enum
{
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
	
enum
{
    kUSBNETWORK_CONNECTION 		= 0,			// Notifications
    kUSBRESPONSE_AVAILABLE 		= 1,
    kUSBSERIAL_STATE 			= 0x20,
    kUSBCONNECTION_SPEED_CHANGE		= 0x2A
};

enum
{
    CS_INTERFACE		= 0x24,
		
    Header_FunctionalDescriptor	= 0x00,
    CM_FunctionalDescriptor	= 0x01,
    ACM_FunctionalDescriptor	= 0x02,
    Union_FunctionalDescriptor	= 0x06,
    CS_FunctionalDescriptor	= 0x07,
    ECM_Functional_Descriptor	= 0x0f,
    WCM_FunctionalDescriptor	= 0x11,
    DMM_FunctionalDescriptor	= 0x14,
    OBEX_FunctionalDescriptor	= 0x15,
	EEM_Functional_Descriptor	= 0xff,
		
    CM_ManagementData		= 0x01,
    CM_ManagementOnData		= 0x02,
		
    ACM_DeviceSuppCommFeature	= 0x01,
    ACM_DeviceSuppControl	= 0x02,
    ACM_DeviceSuppBreak		= 0x04,
    ACM_DeviceSuppNetConnect	= 0x08
};

    // Ethernet Stats of interest in bmEthernetStatistics (bit definitions)

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

    // Ethernet Stats request definitions
  
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

    // Ethernet Packet Filter definitions
  
enum
{
    kPACKET_TYPE_PROMISCUOUS =		0x0001,
    kPACKET_TYPE_ALL_MULTICAST =	0x0002,
    kPACKET_TYPE_DIRECTED =		0x0004,
    kPACKET_TYPE_BROADCAST =		0x0008,
    kPACKET_TYPE_MULTICAST =		0x0010
};

	// EEM packet definitions
	
typedef struct
{
	UInt16  bmHeader;

} EEMPacketHeader;

    // Functional Descriptors
	
typedef struct 
{
    UInt8	bFunctionLength;
    UInt8 	bDescriptorType;
    UInt8 	bDescriptorSubtype;
} FunctionalDescriptorHeader;

typedef struct 
{
    UInt8	bFunctionLength;
    UInt8 	bDescriptorType;
    UInt8 	bDescriptorSubtype;
    UInt8 	bcdCDC1;
    UInt8 	bcdCDC2;
} HDRFunctionalDescriptor;

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
    UInt8	bFunctionLength;
    UInt8 	bDescriptorType;
    UInt8 	bDescriptorSubtype;
    UInt8 	bmCapabilities;
} ACMFunctionalDescriptor;

typedef struct
{
    UInt8 	bFunctionLength;
    UInt8 	bDescriptorType;
    UInt8 	bDescriptorSubtype;
    UInt8 	iMACAddress;
    UInt8 	bmEthernetStatistics[4];
    UInt16 	wMaxSegmentSize;
    UInt16 	wNumberMCFilters;
    UInt8 	bNumberPowerFilters;
} ECMFunctionalDescriptor;

typedef struct
{
    UInt8 	bFunctionLength;
    UInt8 	bDescriptorType;
    UInt8 	bDescriptorSubtype;
	UInt8 	iMACAddress;
    UInt8 	bmEthernetStatistics[4];
    UInt16 	wMaxSegmentSize;
    UInt16 	wNumberMCFilters;
    UInt8 	bNumberPowerFilters;
} EEMFunctionalDescriptor;

typedef struct 
{
    UInt8	bFunctionLength;
    UInt8 	bDescriptorType;
    UInt8 	bDescriptorSubtype;
    UInt8 	bcdCDC1;
    UInt8 	bcdCDC2;
} WHCMFunctionalDescriptor;

typedef struct 
{
    UInt8	bFunctionLength;
    UInt8 	bDescriptorType;
    UInt8 	bDescriptorSubtype;
    UInt8 	bcdCDC1;
    UInt8 	bcdCDC2;
	UInt16  wMaxCommand;
} DMMFunctionalDescriptor;

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
    UInt8	bLength;
    UInt8 	bDescriptorType;
    UInt8 	bFirstInterface;
    UInt8 	bInterfaceCount;
    UInt8	bFunctionClass;
	UInt8	bFunctionSubClass;
	UInt8	bFunctionProtocol;
	UInt8	iFunction;
} IADDescriptor;

    // Inline conversions
	
static inline unsigned long tval2long(mach_timespec val)
{
   return (val.tv_sec * NSEC_PER_SEC) + val.tv_nsec;   
}

static inline mach_timespec long2tval(unsigned long val)
{
    mach_timespec tval;

    tval.tv_sec = val / NSEC_PER_SEC;
    tval.tv_nsec = val % NSEC_PER_SEC;
    return tval;	
}

static inline UInt8 Asciify(UInt8 i)
{

    i &= 0xF;
    if (i < 10)
        return('0' + i);
    else return(55  + i);
	
}		
	
#endif