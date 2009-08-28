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
#ifndef _IOKIT_IOFWIPDEFINITIONS_H
#define _IOKIT_IOFWIPDEFINITIONS_H

#include "IOFWController.h"

struct FWUnsignedWideStruct{
    UInt32	hi;
    UInt32	lo;
};

typedef struct FWUnsignedWideStruct FWUnsignedWide;

/* All the different platforms (and their development environments) have
 slightly different type definition capitalization conventions, etc. This
 section regularizes all that so that our source code may use uniform names */
#define UNSIGNED	unsigned
#define UWIDE		FWUnsignedWide

/* Macros for convenience */
#undef LAST
#define LAST(array) ((sizeof(array) / sizeof(array[0])) - 1)

#define BIT_SET(x)	(1 << x) 

/* Well known IPv4 & IPv6 multicast addresses */
const UInt8 IPv4KnownMcastAddresses[5] =	{0x01, 0x00, 0x5E, 0x00, 0xE0};

/* IPv6 well know multicast addresses Scope ID 0xFF 1, 2, 4, 5, 8, D */										
/* Solicited Multicast address FF 02::0:01:FFXX:XXXX */
		
/*
 * Miscellanaeous constants from IEEE Std 1394a-2000, ISO/IEC 13213:1994
 * and RFC 2734.
 */
#define DEFAULT_BROADCAST_CHANNEL	31
#define GASP_TAG					3
#define LOCAL_BUS_ID				0x3FF

#define ETHER_TYPE_MCAP				0x8861

#define IP1394_SPEC_ID				0x00005E     /* Specifier_ID and ... */
#define IP1394_VERSION				0x000001     /* ... Version values for unit directory */
#define IP1394v6_VERSION			0x000002	/* ... Version values for unit directory */


#define IPV4_ADDR_SIZE				4            /* Four octets (xxx.xxx.xxx.xxx) */
#define IPV4_HDR_SIZE				20           /* Minimum size from RFC 791 */

#define FW_M_BCAST		0x10
#define FW_M_MCAST		0x20
#define FW_M_UCAST		0x40
#define FW_M_PKTHDR		0x02

/* Encapsulation headers defined by RFC 2734. Unfragmented datagrams use a
 short (one quadlet) encapsulation header that specifies the Ether type while
 the headers for datagram fragments are two quadlets and contain total datagram
 size and the position of the fragment within the datagram. When a datagram is
 fragmented, the Ether type is present in the first fragment, only */

typedef struct {                    /* Unfragmented encapsulation header*/
   UInt16 reserved;                 /* Always zero for unfragmented datagram */
   UInt16 etherType;                /* See RFC 1700 */
} IP1394_UNFRAG_HDR;

typedef struct {                    /* IP1394 encapsulation header */
   UInt16 datagramSize;             /* Total size of the datagram, less one */
   UInt16 fragmentOffset;           /* Second and subsequent fragments */
   UInt16 dgl;                      /* Datagram label */
   UInt16 reserved;
} IP1394_FRAG_HDR;

typedef union {
   IP1394_UNFRAG_HDR singleFragment;
   IP1394_FRAG_HDR fragment;
} IP1394_ENCAP_HDR;

/* NOTE: The link fragment type (lf) field is not shown above. It is the most
 significant two bits of the encapsulation header, with values as follows. */
typedef enum
{
	UNFRAGMENTED = 0,
	FIRST_FRAGMENT,
	LAST_FRAGMENT,
	INTERIOR_FRAGMENT
} FragmentType;

/* IEEE Std 1394a-2000 defines a global asynchronous stream packet (GASP)
 format that is used by RFC 2734 for the broadcast and multicast datagrams
 and also for ARP and MCAP. The two-quadlet GASP header and its components
 are specified below, along with an entire packet structure (as specified by
 RFC 2734). */

typedef struct {
   UInt8 specifierID[3];      /* 24-bit RID (0x 00 005E) */
   UInt8 version[3];          /* 24-bit version (0x00 00001) */
} GASP_ID;

typedef struct {
   UInt16  sourceID;           /* 16-bit node ID of sender */
   GASP_ID gaspID;
} GASP_HDR;


/* In those cases where the entire GASP is sent or received, firewire services automatically
 alters the endian-ness of the first quadlet (packet header) while leaving the
 data payload as is. This means that code that interprets that quadlet has to
 account for the endian orientation of the CPU on which it executes. Hence
 there are two flavors of the GASP type, one for big- and the other for
 little-endian. */

#if (BYTE_ORDER == BIG_ENDIAN)
typedef struct {
   UInt16	dataLength;
   UInt8	channel;            /* Plus tag (in two most significant bits) */
   UInt8	tCode;              /* Plus sy (in four least significant bits) */
   GASP_HDR gaspHdr;			/* Two-quadlet GASP header */
   IP1394_UNFRAG_HDR ip1394Hdr; /* Fragmentation not permitted! */
} GASP;
#else
typedef struct {
   UInt8	tCode;              /* Plus sy (in four least significant bits) */
   UInt8	channel;            /* Plus tag (in two most significant bits) */
   UInt16	dataLength;
   GASP_HDR gaspHdr;			/* Two-quadlet GASP header */
   IP1394_UNFRAG_HDR ip1394Hdr; /* Fragmentation not permitted! */
} GASP;
#endif

/* ARP message format defined by RFC 2734. When transmitted, it is preceded by
 an encapsulation header (one quadlet), whichis itself preceded by a GASP
 header (two quadlets). */

typedef struct {
   UInt16	hardwareType;        /* Constant 0x0018 for Serial Bus */
   UInt16	protocolType;        /* Constant 0x0800 for Serial Bus ARP */
   UInt8	hwAddrLen;           /* "Hardware address" length */
   UInt8	ipAddrLen;           /* IPv4 address length */
   UInt16	opcode;              /* ARP request or response */
   UWIDE	senderUniqueID;      /* EUI-64 from sender's bus information block */
   UInt8	senderMaxRec;        /* Maximum payload (2 ** senderMaxRec) */
   UInt8	sspd;                /* Maximum speed */
   UInt16	senderUnicastFifoHi; /* Most significant 16 bits of FIFO address */
   UInt32	senderUnicastFifoLo; /* Least significant 32 bits of FIFO address */
   UInt32	senderIpAddress;     /* Sender's IPv4 address */
   UInt32	targetIpAddress;     /* In ARP request, sought-after IPv4 address */
} IP1394_ARP;

/* NDP message format defined by RFC 3146. */
struct ip1394_ndp {
   UInt8	type;					/* type = 1 or 2 */
   UInt8	len;					/* len in units of 8 octets */
   UInt8	lladdr[kIOFWAddressSize];	/* EUI-64 from sender's bus information block */
   UInt8	senderMaxRec;			/* Maximum payload (2 ** senderMaxRec) */
   UInt8	sspd;					/* Maximum speed */
   UInt16	senderUnicastFifoHi;	/* Most significant 16 bits of FIFO address */
   UInt32	senderUnicastFifoLo;	/* Least significant 32 bits of FIFO address */
   UInt8	reserved[6];			/* reserved by the RFC 3146 */
} __attribute__((__packed__));

typedef struct ip1394_ndp IP1394_NDP;

#define ARP_HDW_TYPE			24       /* ARP hrd type assigned by IANA */

/* The senderUniqueID, senderMaxRec, sspd and senderUnicastFifo fields in the
 ARP message collectively make a link-level "hardware" aaddress for IP394. This
 address is used in other contexts and its structure is given below. */

typedef struct {              /* IP1394 "hardware" address */
   UWIDE	eui64;            /* Node's EUI-64 (from bus information block) */
   UInt8	maxRec;           /* Maximum asynchronous payload */
   UInt8	spd;              /* Maximum speed */
   UInt16	unicastFifoHi;    /* Most significant bits of unicast FIFO address */
   UInt32	unicastFifoLo;    /* Least significant bits of unicast FIFO address */
} IP1394_HDW_ADDR;

/* Multicast channel allocation protocol (MCAP) message format defined by
 RFC 2734. When transmitted, it is preceded by an encapsulation header (one
 quadlet), whichis itself preceded by a GASP header (two quadlets). */

typedef struct {
   UInt8	length;             /* Total size of descriptor (bytes) */
   UInt8	type;               /* Constant one (1) for MCAST_DESCR */
   UInt16	reserved1;
   UInt8	expiration;         /* For advertisements, lifespan remaining */
   UInt8	channel;            /* Channel number for the group */
   UInt8	speed;              /* Transmission speed for the group */
   UInt8	reserved2;
   UInt32	bandwidth;          /* Not yet utilized */
   UInt32	groupAddress;		/* IPv4 multicast address */
} MCAST_DESCR;

#define MCAST_TYPE			1	/* IPv4 MCAST type */

typedef struct {
   UInt16	length;             /* Total length of MCAP message, in bytes */
   UInt8	reserved;
   UInt8	opcode;             /* Advertise or solicit */
   MCAST_DESCR groupDescr[0];	/* Zero or more instances of MCAST_DESCR */
} IP1394_MCAP;

#define MCAP_ADVERTISE		0
#define MCAP_SOLICIT		1

/* The IP1394 code requires a stable "handle" to represent an internal
 address (used analogously to Ethernet MAC addresses) for each IP address in
 the ARP cache. The handle is twelve bytes long and has a different form for
 unicast addresses (reachable via Serial Bus block write requests) and multi-
 cast addresses (reachable via asynchronous stream packets) */

typedef struct {				/* IP1394 "hardware" address */
   void*	deviceID;			/* Stable reference to unit architecture */
   UInt8	maxRec;             /* Maximum asynchronous payload */
   UInt8	spd;                /* Maximum speed */
   UInt16	unicastFifoHi;      /* Upper 16 bits of unicast FIFO address */
   UInt32	unicastFifoLo;      /* Lower 32 bits of unicast FIFO address */
} TNF_UNICAST_HANDLE;

typedef struct {				/* IP1394 "hardware" address */
   UInt32	deviceID;			/* Always zero */
   UInt8	maxRec;				/* Maximum asynchronous payload */
   UInt8	spd;				/* Maximum speed */
   UInt8	reserved;
   UInt8	channel;			/* Channel number for GASP transmit / receive */
   UInt32	groupAddress;		/* Distinguish groups that share channel */
} TNF_MULTICAST_HANDLE;

typedef union {
   TNF_UNICAST_HANDLE	unicast;
   TNF_MULTICAST_HANDLE multicast;
} TNF_HANDLE;

struct arp_packet {
	GASP_HDR gaspHdr;
	IP1394_UNFRAG_HDR ip1394Hdr;
	IP1394_ARP arp;
};


struct mcap_packet {
	GASP_HDR gaspHdr;
	IP1394_UNFRAG_HDR ip1394Hdr;
	IP1394_MCAP mcap;
};

#if defined(__BIG_ENDIAN__)
typedef struct {
	UInt16 size;
	UInt8  tag:2;
	UInt8  chan:6;
	UInt8  tcode:4;
	UInt8  sy:4;
} ISOC_DATA_PKT;
#elif defined(__LITTLE_ENDIAN__)
typedef struct {    
	UInt8  sy:4;
	UInt8  tcode:4;
	UInt8  chan:6;
	UInt8  tag:2;
	UInt16 size;
} ISOC_DATA_PKT;
#else
#error host endian unknown
#endif

/* Multicast Address resolution block (ARB) contains all of the information necessary to
 map, in either direction, between an IPv4 address and a link-level "hardware"
 address 
 rfc2373 - section 2.7.2 notes that 32 bits are enough to identify unique IPv6 multicast
		   addresses.
 */

class MARB : public OSObject		/* Used by both ARP 1394 and MCAP */
{      
	OSDeclareDefaultStructors(MARB);
public:
	TNF_HANDLE	handle;         /* Pseudo "hardware" address used internally */
};

/* Address resolution block (ARB) contains all of the information necessary to
 map, in either direction, between an IPv4 address and a link-level "hardware"
 address */

class ARB : public OSObject		/* Used by both ARP 1394 and MCAP */
{      
	OSDeclareDefaultStructors(ARB);
public:
	UWIDE		eui64;          /* EUI-64 obtained from ARP response */
	UInt8		fwaddr[kIOFWAddressSize];
	TNF_HANDLE	handle;         /* Pseudo "hardware" address used internally */
	bool		itsMac;   		/* Indicates whether the destination Macintosh or not */
};

/* Device reference block (DRB) correlates an EUI-64 with a IOFireWireNub
 reference ID acquired with a kGUIDType parameter. A pointer to the LCB is
 also part of the structure---because the address of a DRB is passed to a
 bus reset notification procedure which in turn needs to reference the
 relevant LCB. Note also the inclusion of a timer field; because node
 removals may be temporary, the IP1394 code does NOT dispose of the device
 reference as soon as the node disappears. Instead, an expiration timer is
 started. If the device has not reappeared within the specified number of
 seconds, then the device reference ID is released. */
 
class DRB : public OSObject
{
	OSDeclareDefaultStructors(DRB);
public:
	UWIDE		eui64;			/* EUI-64 of the IP-capable device */
	UInt8		fwaddr[kIOFWAddressSize];
	void*		deviceID;		/* Stable "handle" for the IP-capable device */
	UInt16		maxPayload;		/* Maximum payload and... */
	IOFWSpeed	maxSpeed;		/* ...speed to device in current topology */
	bool		itsMac;			/* Indicates whether the destination Macintosh or not */
};

/* Multicast control block (MCB) permits the management of multicast channel
 assignments, whether we are the owner or simply one of the participants in
 the multicast group. */

class MCB : public OSObject 
{
	OSDeclareDefaultStructors(MCB);
public:
   UInt32	ownerNodeID;         /* Channel owner (it may be us!) */
   UInt32	groupCount;          /* IP address groups active (this channel )*/
   OSObject *asyncStreamID;
   UInt8	channel;             /* Redundant (but makes debug easier) */
   UInt8	expiration;          /* Seconds remaining in valid channel mapping */
   UInt8	nextTransmit;        /* Seconds 'til MCAP advertisement transmitted */
   UInt8	finalWarning;        /* Channel deallocation warning messages */
};

#define MCAP_UNOWNED 0        /* No channel owner */

/* Reassembly control block (RCB) tracks the progress of the entire datagram
 as fragments arrive. The algorithm is simple---primitive even---but adequate
 for the unconfirmed nature of IP datagrams. When one of the fragments first
 arrives, an MBUF adequate to hold the entire datagram is allocated and the
 fragment is copied to its correct location; the residual count is decremented.
 This process repeats with successive fragments until residual is zero. */
class RCB : public IOCommand
{
	OSDeclareDefaultStructors(RCB);

private:
    void free();
	
public:
	UInt16	sourceID;           /* Saved from LK_DATA.indication */
	UInt16	dgl;                /* Obtained from the fragment header */
	UInt16	etherType;          /* Saved from first fraagment header */
	UInt16	datagramSize;       /* Total size of the reassembled datagram */
	UInt16	residual;           /* Bytes still outstanding */
	UInt32  timer;				/* If nonzero, decrement and release upon zero */
	mbuf_t	mBuf;				/* MBUF eventually passed to OS code */

	void reinit(UInt16 id, UInt16 label, UInt16 etherType, UInt16 size, mbuf_t m);
};

/* End of the type definitions for the miscellaneous control structures */

/* Link control block (LCB) used to maintain context for IOFireWireIP routines for a
 single link instance. The ARP and MCAP caches are also referenced by this
 structure. */
typedef struct lcb /* Link Control Block (LCB) for each link */
{          
   IP1394_HDW_ADDR	ownHardwareAddress; /* Our external address on Serial Bus */
   UInt16			ownMaxPayload;      /* From this link's bus information block */
   UInt16			ownMaxSpeed;        /* Link/PHY hardware capability */
   UInt16			ownNodeID;          /* Management information, only */
   UInt16			maxBroadcastPayload;/* Updated when topology changes */
   IOFWSpeed		maxBroadcastSpeed;  /* Ditto */
   UInt16			datagramLabel;
   UInt32			busGeneration;      /* Current as of most recent bus reset */
} LCB;

#endif /* _IOKIT_IOFWIPDEFINITIONS_H */
