/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
extern "C"{
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/dlil.h>
#include <sys/syslog.h>
#include <sys/socketvar.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <net/ethernet.h>

extern void _logMbuf(struct mbuf * m);

}

#include <IOKit/firewire/IOFireWireDevice.h>
#include "IOFWEthernetShim.h"

extern const OSSymbol *gFireWireVendor_ID;

//
// Lets use the following high 24 bit as the vendor ID for all other
// nodes on the bus
//
UInt8   vbits[3] = {0x00, 0x54, 0x04};
extern	UInt8 bcastAddr[];

OSDefineMetaClassAndStructors(IOFWEthernetShim, OSObject);
OSMetaClassDefineReservedUnused(IOFWEthernetShim, 0);
OSMetaClassDefineReservedUnused(IOFWEthernetShim, 1);
	
bool IOFWEthernetShim::initAll(IOFireWireIP *provider, LCB *lcb)
{
    fwIpObj = provider;
	fLcb = lcb;

	// start ethshim
	broadcastHandle = (TNF_HANDLE*)IOMalloc(sizeof(TNF_HANDLE));

	// store a broadcast handle
	fwIpObj->fwResolve(fLcb, M_BCAST, NULL, broadcastHandle, NULL);

	// mem handles
	fwMemHandles = (UInt8*)IOMalloc(sizeof(FW_INDEX) * MAX_FW_HANDLE_INDEX);
	fMemIndex = 0;
			
	return true;
}

void IOFWEthernetShim::free()
{
	if(fwMemHandles){
		IOFree(fwMemHandles, (sizeof(FW_INDEX) * MAX_FW_HANDLE_INDEX));
		fwMemHandles = NULL;
	}	
		
	if(broadcastHandle){
		IOFree(broadcastHandle, sizeof(TNF_HANDLE));
		broadcastHandle = NULL;
	}
	
	OSObject::free();
	
	return;
}

UInt32 IOFWEthernetShim::getFwIndex(UInt32 idx)
{
	if(idx > MAX_FW_HANDLE_INDEX)
		return NULL;
	
	return (ULONG)(fwMemHandles + (sizeof(FW_INDEX)*idx));
}

UInt32 IOFWEthernetShim::getNextFwIndex(UInt32 *idx)
{
	if(fMemIndex >= MAX_FW_HANDLE_INDEX)
		fMemIndex = 0;
	
	*idx = fMemIndex;

	return (ULONG)(fwMemHandles + (sizeof(FW_INDEX)*fMemIndex++));
}

void* IOFWEthernetShim::fwIndexExists(TNF_HANDLE *handle, UInt32 *index)
{
	FW_INDEX *fwi;
	UInt32 idx = 0;
	
	for(idx = 0;idx <= MAX_FW_HANDLE_INDEX; idx++)
	{
		fwi = (FW_INDEX *)(fwMemHandles + (sizeof(FW_INDEX)*idx));
		if(fwi->handle == handle)
		{
			*index = idx; 
			return ((void*)fwi);
		}
	}
	
	return NULL;
}

/*******************************************************************************
 *
 * sendPacket - Understand the frame as ARP or IP based.
 *
 *******************************************************************************/
IOReturn IOFWEthernetShim::sendPacket( 
										struct mbuf	*m, 
										void		*reserved)
{
	register ethshimheader	*eh	= NULL;
	int	status = kIOReturnSuccess;
	
	eh = mtod(m, ethshimheader*);

	if(eh == NULL)
		return kIOReturnError;


	/* find type of packet from frame type */
	switch(htons(eh->frametype))
	{
		case ETHERTYPE_ARP:
			// if packet ARP REQUEST
			status = sendArpPacket(m);
			fwIpObj->freePacket(m);
			break;

		case ETHERTYPE_IP:
			// if packet IP PACKET
			status = sendIPPacket(m);
			break;
			
		default :
			// IOLog("ethshim: ether type not supported %x\n", eh->frametype);
			fwIpObj->freePacket(m);
			break;
	}

	return status;
}

/*******************************************************************************
 *
 * sendArpPacket - Send the ARP Request or Response frame.
 *
 *******************************************************************************/
IOReturn IOFWEthernetShim::sendArpPacket(struct mbuf	*m)
{
	arpshimpkt		*arphdr	= NULL;
	UInt32			status	= kIOReturnSuccess;
	vm_address_t 	src;

	src = mtod(m, vm_offset_t);

	if(src == NULL)
		return kIOReturnError;

	// find type of packet from options field
	arphdr	= (arpshimpkt*)(src + sizeof(firewire_header));

	switch(htons(arphdr->ar_op))
	{
		case ARP_REQUEST:
			// if packet ARP REQUEST
			status	=	txArpRequest(m);
			break;

		case ARP_RESPONSE:
			// if packet ARP RESPONSE ..? 
			txArpResponse(NULL, 0);
			break;
	}

	return status;
}

/*******************************************************************************
 *
 * txArpRequest - Trasmit the ARP request.
 *
 *******************************************************************************/
IOReturn IOFWEthernetShim::txArpRequest(struct mbuf	*m)
{
	arpshimpkt*			arphdr	= NULL;
	UInt8				flags		= 0;
	UInt8				*bytePtr	= NULL;
	UInt32				cmdLen = 0;
	ARP_HOLD			arpHold;
	SOCKADDR_IN			socket;
	UInt32				status	= kIOReturnError;
	ARB					*arb;
	vm_address_t 		src;

	
	src = mtod(m, vm_offset_t);
	if(src == NULL)
		return kIOReturnError;

	cmdLen = m->m_pkthdr.len;

	arphdr	= (arpshimpkt*)(src + sizeof(firewire_header));

	bzero(&socket, sizeof(SOCKADDR_IN));

	// construct the dummy socket structure for firewire interface
	socket.sin_family	= 0;
	socket.sin_len		= sizeof(SOCKADDR_IN);
				
	/* If target IP address is equal to the local then its a gratuitious ARP */
	if (bcmp(arphdr->arp_tpa, arphdr->arp_spa, IP_ADDR_LEN) == 0)
	{
		bytePtr	=	(UInt8*)&fLcb->ownIpAddress;
		bcopy(arphdr->arp_tpa, bytePtr, 4);
	}

	if (bcmp(arphdr->arp_tpa, "0x00000000", IP_ADDR_LEN) != 0)
	{
  		bcopy((void*)arphdr->arp_tpa, (void*)&socket.sin_addr, IP_ADDR_LEN);
		// IOLog("target IP address %lX %d \n\r",  socket.sin_addr, __LINE__);
 	}

	// contruct the arphold structure
	arpHold.callback		= NULL; // not used		
	arpHold.socket			= (void*)cmdLen;
	arpHold.passThru1		= fwIpObj;
	arpHold.passThru2		= NULL;

	// IOLog("target IP address %lX %d \n\r",  socket.sin_addr, __LINE__);

	// Only first time allocate the ARP packet for the ARB cache
	if((arb = fwIpObj->getUnicastArb(fLcb, socket.sin_addr)) == NULL) 
	{ 
		// Not in ARB cache, Lets allocate a space for our frame 
		arpHold.ipDatagram	= IOMalloc(cmdLen);
		if(arpHold.ipDatagram == NULL)
			return status;

		// make a copy of the ethernet frame
		bcopy((void*)src, arpHold.ipDatagram, cmdLen);

		// Allocate an ARB
		if((arb = (ARB*)fwIpObj->allocateCBlk(fLcb)) == NULL) 
			return status;

		// Resolve the IPAddress & Start completing the ARB 
		arb->ipAddress = socket.sin_addr;        
		arb->datagramPending = TRUE;
		memcpy(&arb->arpHold, &arpHold, sizeof(ARP_HOLD));
		fwIpObj->linkCBlk(&fLcb->unicastArb, arb);

		// Broadcast ARP request
		fwIpObj->txFwARP(fLcb, ARP_REQUEST, arb->ipAddress, 0); 

		status = kIOReturnSuccess;
	} 
	else
	{
		// We got it in cache
		if(arb->arpHold.ipDatagram == NULL)
		{
			arpHold.ipDatagram	= IOMalloc(cmdLen);
			// make a copy of the ethernet frame
			bcopy((void*)src, arpHold.ipDatagram, cmdLen);
		}
		else
		{
			arpHold.ipDatagram = arb->arpHold.ipDatagram;
		}
		
		// Resolve the IPAddress
		status = fwIpObj->fwResolve(fLcb, flags, &socket, &arb->handle, &arpHold);

		if(status == 1)
		{
			// indicates we got a handle to the request from the cache	- Do ARP Response
			arpCallback(arpHold.ipDatagram, arpHold.socket, arpHold.passThru1, 
						  &arb->handle); 

			status	=	kIOReturnSuccess;
		}
	}


	return status;
}

/*******************************************************************************
 *
 * sendIPPacket - Send IP packet.
 *
 *******************************************************************************/
IOReturn IOFWEthernetShim::sendIPPacket(struct mbuf *m)
{
	ethshimheader		*eh	= NULL;
	IOReturn	status  = kIOReturnError;

	eh = mtod(m, ethshimheader*);

	if(eh == NULL)
		return status;
	
	// IOLog("sendIPPacket \n");
	
	// If unicast type
	if(bcmp(eh->dstaddr, vbits, 3) == 0) 
	{
		status = sendUnicastPacket(m);
	}
	else
	{
		status = sendMulticastPacket(m);
	}

	return status;
}

/*******************************************************************************
 *
 * sendMulticastPacket - Send Multicast packet.
 *
 *******************************************************************************/
IOReturn IOFWEthernetShim::sendMulticastPacket(struct mbuf *m)
{
	ipshimhdr		*iphdr	= NULL;
	ARB				*mcarb	= NULL;
	SOCKADDR_IN		socket;
	ARP_HOLD		arpHold;
	IOReturn		status = kIOReturnError;
	vm_address_t 	src;
	struct mbuf 	*temp;

	//IOLog("+sendMulticastPacket \n");

	src = mtod(m, vm_offset_t);
	if(src == NULL)
		return status;

	// check whether len equals ether header
	if(m->m_len == sizeof(firewire_header))
	{
		temp = m->m_next;
		if(temp == NULL)
			return status;

		src =  mtod(temp, vm_offset_t);
		
		if(temp->m_len < (int)sizeof(ipshimhdr))
			return status;
		
		iphdr = (ipshimhdr*)(src);
	}
	else
	{
		iphdr = (ipshimhdr*)(src + sizeof(firewire_header));
	}

	bzero(&socket, sizeof(SOCKADDR_IN));

	/* construct the dummy socket structure for IP1394 interface */
	socket.sin_family	= 0;
	socket.sin_len		= sizeof(SOCKADDR_IN);

	mcarb	=	fwIpObj->getMulticastArb(fLcb, iphdr->ip_dst);
	
	if(mcarb == NULL)
	{
		// Allocate a handle
		if ((mcarb = (ARB*)fwIpObj->allocateCBlk(fLcb)) == NULL)
		{
			IOLog("No CBLK available for new entry in MCAP cache\n");
			return status;    // No CBLK available for new entry in MCAP cache
		}

		// Set the destination address as the  group address
		mcarb->ipAddress	=	iphdr->ip_dst;
		// Set the socket's dummy ipaddress
  		socket.sin_addr		=	iphdr->ip_dst;		

		fwIpObj->fwResolve(fLcb, M_MCAST, &socket, &mcarb->handle, &arpHold);

		fwIpObj->linkCBlk(&fLcb->multicastArb, mcarb);

		/* Solicit in hope of speedy response */
		fwIpObj->txMCAP(fLcb, NULL, mcarb->handle.multicast.groupAddress); 
	}

	status = fwIpObj->txIP(NULL, m, &mcarb->handle); 

	return status;

}

/*******************************************************************************
 *
 * sendUnicastPacket - Send Unicast packet.
 *
 *******************************************************************************/
IOReturn IOFWEthernetShim::sendUnicastPacket(struct mbuf *m)
{
	ethshimheader		*eh	= NULL;
	UInt32				handle		= 0;
	V_MACADDR			*vMacAddr = NULL;
	FW_INDEX			*fwIdx = NULL;

	eh = mtod(m, ethshimheader*);

	if(eh == NULL)
		return kIOReturnError;
	
	// Check if its our mem handle 
	if(bcmp(eh->dstaddr, vbits, 3) == 0)
	{
		vMacAddr	= (V_MACADDR*)eh->dstaddr;
		fwIdx		= (FW_INDEX*)getFwIndex(vMacAddr->idx);
		handle		= (UInt32)((fwIdx != NULL && fwIdx->handle != NULL) ? 
												fwIdx->handle: broadcastHandle);
		
		//fwIpObj->showHandle((TNF_HANDLE*)handle);
	}
	else
	{
		handle	= (UInt32)broadcastHandle;
	}

	// get the handle for the corresponding IP
	return (fwIpObj->txIP(NULL, m, (void*)handle));
}

/*******************************************************************************
 *
 * arpCallback - Callback once the ARP is resolved.
 *
 * param1 - Frame - pointer to the ARP Frame
 * param2 - FrameLength
 * param3 - Adapter
 * param4 - TNF_HANDLE from the IP1394 layer
 *
 *******************************************************************************/
void IOFWEthernetShim::arpCallback(void* pFrame, void* param2, void* param3, void* param4)
{
	UInt32				cmdLen = (UInt32)param2;
	UInt8				srcaddr[IP_ADDR_LEN];
	TNF_HANDLE			*pHandle;
	ethshimheader		*eh;
	arpshimpkt			*arphdr;
	struct mbuf			*rxMBuf;
	FW_INDEX			*fwIndex = NULL;
	UInt32				idx = 0;
	V_MACADDR			*vMacAddr = NULL;
	
#ifdef FIREWIRETODO	
	DRB					*drb = NULL;
	IOFireWireDevice	*fDevice = NULL;
	OSObject			*prop;
	char				tempAddr[FIREWIRE_ADDR_LEN];
#endif
		
	if ((rxMBuf = fwIpObj->getMBuf(cmdLen)) != NULL) {

		// Copy the data
		memcpy(rxMBuf->m_data, pFrame, rxMBuf->m_pkthdr.len = rxMBuf->m_len = cmdLen);
		eh = (ethshimheader*)rxMBuf->m_data;
		eh->frametype = ETHERTYPE_ARP;
		
		bcopy(bcastAddr, eh->dstaddr, ETH_ADDR_LEN);
		
		arphdr	= (arpshimpkt*)(rxMBuf->m_data + sizeof(firewire_header));
		pHandle	= (TNF_HANDLE*)param4;

#ifdef FIREWIRETODO	
		// fix with legitimate 48 bit address
		drb = fwIpObj->getDrbFromDeviceID(fwIpObj->getLcb(), (void*)pHandle->unicast.deviceID);

		if(drb == NULL)
		{
			IOLog("OOps, null drb !, ARP RESPONSE will be dropped\n");
			return;
		}

		// get the GUID
		IOLog("Remote Device GUID = 0x%lx:0x%lx \n", drb->eui64.hi, drb->eui64.lo);
				
		//
		// If vendor ID == Apple, then manufacture 48 bit address
		//    else
		// follow MS standard.
		//
		if(drb->deviceID == kInvalidIPDeviceRefID)
		{
			IOLog("Null deviceID, lets go home !\n");
			return;
		}
		
		fDevice = (IOFireWireDevice*)drb->deviceID;

		prop = fDevice->getProperty(gFireWireVendor_ID);
		if(prop == 0)
		{
			IOLog("gFireWireVendor_ID property not found\n");
			return;
		}
		
		OSNumber *num  = OSDynamicCast(OSNumber, prop);
		if(num != 0)
		{
			IOLog("Remote Device Vendor ID %d\n", num->unsigned32BitValue());
		}
		
		CSRNodeUniqueID	fwuid = fDevice->getUniqueID();

		// make the remote 48 bit address
		fwIpObj->makeEthernetAddress(&fwuid, tempAddr, num->unsigned32BitValue());
		
        IOLog("Remote Device Ethernet address %02x:%02x:%02x:%02x:%02x:%02x\n",
															tempAddr[0],
															tempAddr[1],
															tempAddr[2],
															tempAddr[5],
															tempAddr[6],
															tempAddr[7]);
		//
#endif
		
		// check if the index already allocated
		fwIndex = (FW_INDEX*)fwIndexExists(pHandle, &idx);
		if(fwIndex == NULL)
			fwIndex = (FW_INDEX*)getNextFwIndex(&idx);
			
		fwIndex->handle = pHandle;
		
		// Set up the ARP Reply
		bcopy(arphdr->arp_spa, srcaddr, IP_ADDR_LEN);
		bcopy(arphdr->arp_tpa, arphdr->arp_spa, IP_ADDR_LEN);
		bcopy(srcaddr, arphdr->arp_tpa, IP_ADDR_LEN);
		
		vMacAddr = (V_MACADDR*)arphdr->arp_sha;
		vMacAddr->vendor[0] = vbits[0];
		vMacAddr->vendor[1] = vbits[1];
		vMacAddr->vendor[2] = vbits[2]; 	
		vMacAddr->inst = (UInt8)fwIpObj->ourUnitNumber(); 
		vMacAddr->idx = idx;
		
		arphdr->ar_op	=	htons(ARP_RESPONSE);
		
		fwIpObj->receivePackets(rxMBuf, rxMBuf->m_pkthdr.len, NULL);
	}
}

/*******************************************************************************
 *
 * txArpResponse - Whats the deal here ? Who will call us ?..:)
 *
 *******************************************************************************/
void IOFWEthernetShim::txArpResponse(
										UInt32				*ethBuffer,
										UInt32				ethBufferLength)
{
	/* misc log messages  */
//		IOLog(" ar_hrd %x \n", arphdr->ar_hrd);
//		IOLog(" ar_pro %x \n", arphdr->ar_pro);
//		IOLog(" ar_hln %d \n", arphdr->ar_hln);
//		IOLog(" ar_pln %d \n", arphdr->ar_pln);
	
//		IOLog(" arp_op %x \n", htons(arphdr->ar_op));
	
//		IOLog(" ARP RES sha %x.%x.%x.%x.%x.%x to tha %x.%x.%x.%x.%x.%x \n", 
//										arphdr->arp_sha[0], arphdr->arp_sha[1], arphdr->arp_sha[2], 
//										arphdr->arp_sha[3], arphdr->arp_sha[4], arphdr->arp_sha[5],
//										arphdr->arp_tha[0], arphdr->arp_tha[1], arphdr->arp_tha[2], 
//										arphdr->arp_tha[3], arphdr->arp_tha[4], arphdr->arp_tha[5]);
													
//		IOLog(" ARP RES SIP: %u.%u.%u.%u DIP: %u.%u.%u.%u \n\r",
//					((UCHAR *) &arphdr->arp_spa)[0],
//					((UCHAR *) &arphdr->arp_spa)[1],
//					((UCHAR *) &arphdr->arp_spa)[2],
//					((UCHAR *) &arphdr->arp_spa)[3],
//					((UCHAR *) &arphdr->arp_tpa)[0],
//					((UCHAR *) &arphdr->arp_tpa)[1],
//					((UCHAR *) &arphdr->arp_tpa)[2],
//					((UCHAR *) &arphdr->arp_tpa)[3]
//					);

//		_logMbuf(rxMBuf);

}

/*******************************************************************************
 *
 * recvArpPacket - ARP Recv request who is calling us ?
 *
 *******************************************************************************/
void	IOFWEthernetShim::recvArpPacket(
										UInt32				*ethBuffer,
										UInt32				ethBufferLength)
{

}


/*******************************************************************************
 *
 * recvPacket - Understand the frame as ARP or IP based.
 *
 *******************************************************************************/
void IOFWEthernetShim::recvPacket( 
									UInt32				*ethBuffer, 
									UInt32				ethBufferLength)
{

}

/*******************************************************************************
 *
 * recvIPPacket - Receive IP Packet.
 *
 *******************************************************************************/
void IOFWEthernetShim::recvIPPacket(
									UInt32				*ethBuffer,
									UInt32				ethBufferLength)
{

}