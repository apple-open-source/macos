/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*! @class IOFWIPAsyncWriteCommand
*/
//
#ifndef _IOKIT_IOFWETHERNETSHIM_H
#define _IOKIT_IOFWETHERNETSHIM_H

#include <IOKit/IOService.h>
#include "IOFireWireIP.h"
#include "ip_firewire.h"

#define		BROADCAST_ADDRESS		0xFFFFFFFFFFFF
#define		MIN_ETH_FRAME			60

#define		MULTICAST_HEADER_LEN	3
#define		ETH_ADDR_LEN			6
#define		IP_ADDR_LEN				4
#define		HANDLE_LEN				4
#define		IP_DUM_HDR				12

/*******************************************************************************
 * IP pseudo header                                                   
 ********************************************************************************/
typedef struct
{
	UInt8	header[IP_DUM_HDR];	/* rest of ip's 12 byte standard header */
	UInt32	ip_src, ip_dst;	/* source and dest address */
} ipshimhdr;


/*******************************************************************************
 * ARP header                                                   
 ********************************************************************************/
typedef struct
{
    UInt16	ar_hrd;	/* format of hardware address */
    UInt16	ar_pro; /* format of protocol address */
	UInt8	ar_hln;	/* length of hardware address */
	UInt8	ar_pln;	/* length of protocol address */
	UInt16	ar_op;	/*  ARP/RARP operation */
	UInt8	arp_sha[ETH_ADDR_LEN];	/* sender hardware address */
	UInt8	arp_spa[IP_ADDR_LEN];	/* sender protocol address */
	UInt8	arp_tha[ETH_ADDR_LEN];  /* target hardware address */
	UInt8	arp_tpa[IP_ADDR_LEN];   /* target protocol address */
} arpshimpkt;

/*******************************************************************************
 * Ethernet header                                                   
 ********************************************************************************/
typedef struct
{
    UInt8	dstaddr[ETH_ADDR_LEN];
    UInt8	srcaddr[ETH_ADDR_LEN];
    UInt16	frametype;
} ethshimheader;

#define MAX_FW_HANDLE_INDEX 512

//
// Array of firewire handle instead of a memory address
//
typedef struct {
	TNF_HANDLE *handle;   
} FW_INDEX;

// s/w mac address
typedef struct {
	UInt8 vendor[3];   
	UInt8 inst;
	UInt16 idx;
} V_MACADDR;

class IOFireWireIP;

/*! @class IOFWEthernetShim
*/
class IOFWEthernetShim : public OSObject
{
    OSDeclareDefaultStructors(IOFWEthernetShim)
	
private :
    IOFireWireIP		*fwIpObj;
	LCB					*fLcb;
	TNF_HANDLE			*broadcastHandle;
	UInt8  				*fwMemHandles;
	UInt32  			fMemIndex;
	
protected:
/*! @struct ExpansionData
    @discussion This structure will be used to expand the capablilties of the class in the future.
    */    
    struct ExpansionData { };

/*! @var reserved
    Reserved for future use.  (Internal use only)  */
    ExpansionData *reserved;
    
    virtual void		free();
	
	UInt32		getNextFwIndex		(UInt32	*idx);
	
	UInt32		getFwIndex			(UInt32 idx);
	
	void		recvIPPacket		(UInt32	*ethBuffer, UInt32	ethBufferLength);

	void		recvArpPacket		(UInt32	*ethBuffer, UInt32	ethBufferLength);

	IOReturn 	txArpRequest		(struct mbuf	*m);
	
	void		txArpResponse		(UInt32	*ethBuffer,	UInt32	ethBufferLength);

	IOReturn	sendArpPacket		(struct mbuf	*m);

	IOReturn	sendIPPacket		(struct mbuf	*m);

	IOReturn	sendUnicastPacket	(struct mbuf	*m);

	IOReturn	sendMulticastPacket	(struct mbuf	*m);
    

public:
	bool 		initAll		(IOFireWireIP	*provider, LCB	*lcb);

	void		*fwIndexExists		(TNF_HANDLE	*handle, UInt32 *index);
	
	IOReturn	sendPacket	(struct mbuf	*m, void	*reserved);

	void		recvPacket	(UInt32			*ethBuffer,	UInt32	ethBufferLength);

	void		arpCallback	(void	*pFrame, void	*param2, void	*param3, void	*param4);


	
private:
    OSMetaClassDeclareReservedUnused(IOFWEthernetShim, 0);
    OSMetaClassDeclareReservedUnused(IOFWEthernetShim, 1);

};

#endif // _IOKIT_IOFWETHERNETSHIM_H

