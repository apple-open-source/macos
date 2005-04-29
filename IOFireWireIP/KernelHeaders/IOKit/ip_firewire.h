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
#ifndef _IP_FIREWIRE_H_
#define _IP_FIREWIRE_H_

typedef unsigned char BOOLEAN;
#define CONST const
#define EXTERN extern
#define INT int
#define REGISTER register
#define STATIC static
#define VOID void
#define ULONG u_long
#define USHORT u_short
#define UCHAR u_char

#if !defined(LOG_ENABLE)
#define LOG_ENABLE 0
#endif

#define kInvalidAsyncStreamRefID 0

struct FWUnsignedWideStruct{
    ULONG	hi;
    ULONG	lo;
};

typedef struct FWUnsignedWideStruct FWUnsignedWide;

/* All the different platforms (and their development environments) have
 slightly different type definition capitalization conventions, etc. This
 section regularizes all that so that our source code may use uniform names */
#define UNSIGNED unsigned
#define UWIDE FWUnsignedWide

/* Macros for convenience */
#undef LAST
#define LAST(array) ((sizeof(array) / sizeof(array[0])) - 1)

/*
 * Miscellanaeous constants from IEEE Std 1394a-2000, ISO/IEC 13213:1994
 * and RFC 2734.
 */
#define DEFAULT_BROADCAST_CHANNEL 31
#define GASP_TAG 3
#define LOCAL_BUS_ID 0x3FF

#define ETHER_TYPE_MCAP 0x8861

#define SPECIFIER_ID_KEY 0x12
#define VERSION_KEY 0x13

#define IP1394_MTU 1500
#define IP1394_SPEC_ID 0x00005E     /* Specifier_ID and ... */
#define IP1394_VERSION 0x000001     /* ... Version values for unit directory */
#define IP1394v6_VERSION 0x000002	/* ... Version values for unit directory */


#define IPV4_ADDR_SIZE 4            /* Four octets (xxx.xxx.xxx.xxx) */
#define IPV4_HDR_SIZE  20           /* Minimum size of an RFC 791 header */
#define IPV6_HDR_SIZE  40           /* Minimum size of an RFC 2460 header */

#define MIB2_RX_BYTES 1             /* Total bytes received */
#define MIB2_RX_UNICAST 2           /* Unicast datagrams received */
#define MIB2_RX_MULTICAST 3         /* Broadcast/multicast datagrams received */
#define MIB2_RX_DISCARD 4           /* Discarded datagrams (inbound) */
#define MIB2_RX_ERROR 5             /* Input errors */
#define MIB2_RX_UNKNOWN 6           /* Unrecognized input */
#define MIB2_TX_BYTES 7             /* Total bytes transmitted */
#define MIB2_TX_UNICAST 8           /* Unicast datagrams transmitted */
#define MIB2_TX_MULTICAST 9         /* Broadcast/multicast datagrams transmitted */
#define MIB2_TX_DISCARD 10          /* Discarded datagrams (outbound) */
#define MIB2_TX_ERROR 11            /* Input errors */

#define M2_ifType_ieee1394 144      /* ifType number assigned by IANA */

/* Encapsulation headers defined by RFC 2734. Unfragmented datagrams use a
 short (one quadlet) encapsulation header that specifies the Ether type while
 the headers for datagram fragments are two quadlets and contain total datagram
 size and the position of the fragment within the datagram. When a datagram is
 fragmented, the Ether type is present in the first fragment, only */

typedef struct {                    /* Unfragmented encapsulation header*/
   USHORT reserved;                 /* Always zero for unfragmented datagram */
   USHORT etherType;                /* See RFC 1700 */
} IP1394_UNFRAG_HDR;

typedef struct {                    /* IP1394 encapsulation header */
   USHORT datagramSize;             /* Total size of the datagram, less one */
   USHORT fragmentOffset;           /* Second and subsequent fragments */
   USHORT dgl;                      /* Datagram label */
   USHORT reserved;
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
   UCHAR specifierID[3];      /* 24-bit RID (0x 00 005E) */
   UCHAR version[3];          /* 24-bit version (0x00 00001) */
} GASP_ID;

typedef struct {
   USHORT sourceID;           /* 16-bit node ID of sender */
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
   USHORT dataLength;
   UCHAR channel;             /* Plus tag (in two most significant bits) */
   UCHAR tCode;               /* Plus sy (in four least significant bits) */
   GASP_HDR gaspHdr;          /* Two-quadlet GASP header */
   IP1394_UNFRAG_HDR ip1394Hdr; /* Fragmentation not permitted! */
} GASP;
#else
typedef struct {
   UCHAR tCode;               /* Plus sy (in four least significant bits) */
   UCHAR channel;             /* Plus tag (in two most significant bits) */
   USHORT dataLength;
   GASP_HDR gaspHdr;          /* Two-quadlet GASP header */
   IP1394_UNFRAG_HDR ip1394Hdr; /* Fragmentation not permitted! */
} GASP;
#endif

/* ARP message format defined by RFC 2734. When transmitted, it is preceded by
 an encapsulation header (one quadlet), whichis itself preceded by a GASP
 header (two quadlets). */

typedef struct {
   USHORT hardwareType;       /* Constant 0x0018 for Serial Bus */
   USHORT protocolType;       /* Constant 0x0800 for Serial Bus ARP */
   UCHAR hwAddrLen;           /* "Hardware address" length */
   UCHAR ipAddrLen;           /* IPv4 address length */
   USHORT opcode;             /* ARP request or response */
   UWIDE senderUniqueID;      /* EUI-64 from sender's bus information block */
   UCHAR senderMaxRec;        /* Maximum payload (2 ** senderMaxRec) */
   UCHAR sspd;                /* Maximum speed */
   USHORT senderUnicastFifoHi;/* Most significant 16 bits of FIFO address */
   ULONG senderUnicastFifoLo; /* Least significant 32 bits of FIFO address */
   ULONG senderIpAddress;     /* Sender's IPv4 address */
   ULONG targetIpAddress;     /* In ARP request, sought-after IPv4 address */
} IP1394_ARP;

/* NDP message format defined by RFC 3146. */
struct ip1394_ndp {
   UCHAR type;					/* type = 1 or 2 */
   UCHAR len;       			/* len in units of 8 octets */
   UCHAR lladdr[8];				/* EUI-64 from sender's bus information block */
   UCHAR senderMaxRec;			/* Maximum payload (2 ** senderMaxRec) */
   UCHAR sspd;					/* Maximum speed */
   USHORT senderUnicastFifoHi;	/* Most significant 16 bits of FIFO address */
   ULONG senderUnicastFifoLo;	/* Least significant 32 bits of FIFO address */
   UCHAR reserved[6];			/* reserved by the RFC 3146 */
} __attribute__((__packed__));

typedef struct ip1394_ndp IP1394_NDP;

#define ARP_HDW_TYPE			24       /* ARP hrd type assigned by IANA */
#define ARP_REQUEST 			1
#define ARP_RESPONSE 			2
#define	ARP_SHIM_RESPONSE		5

/* The senderUniqueID, senderMaxRec, sspd and senderUnicastFifo fields in the
 ARP message collectively make a link-level "hardware" aaddress for IP394. This
 address is used in other contexts and its structure is given below. */

typedef struct {              /* IP1394 "hardware" address */
   UWIDE eui64;               /* Node's EUI-64 (from bus information block) */
   UCHAR maxRec;              /* Maximum asynchronous payload */
   UCHAR spd;                 /* Maximum speed */
   USHORT unicastFifoHi;      /* Most significant bits of unicast FIFO address */
   ULONG unicastFifoLo;       /* Least significant bits of unicast FIFO address */
} IP1394_HDW_ADDR;

/* Multicast channel allocation protocol (MCAP) message format defined by
 RFC 2734. When transmitted, it is preceded by an encapsulation header (one
 quadlet), whichis itself preceded by a GASP header (two quadlets). */

typedef struct {
   UCHAR length;              /* Total size of descriptor (bytes) */
   UCHAR type;                /* Constant one (1) for MCAST_DESCR */
   USHORT reserved1;
   UCHAR expiration;          /* For advertisements, lifespan remaining */
   UCHAR channel;             /* Channel number for the group */
   UCHAR speed;               /* Transmission speed for the group */
   UCHAR reserved2;
   ULONG bandwidth;           /* Not yet utilized */
   ULONG groupAddress;        /* IPv4 multicast address */
} MCAST_DESCR;

#define MCAST_TYPE 1

typedef struct {
   USHORT length;             /* Total length of MCAP message, in bytes */
   UCHAR reserved;
   UCHAR opcode;              /* Advertise or solicit */
   MCAST_DESCR groupDescr[0]; /* Zero or more instances of MCAST_DESCR */
} IP1394_MCAP;

#define MCAP_ADVERTISE 0
#define MCAP_SOLICIT 1

/* The IP1394 code requires a stable "handle" to represent an internal
 address (used analogously to Ethernet MAC addresses) for each IP address in
 the ARP cache. The handle is twelve bytes long and has a different form for
 unicast addresses (reachable via Serial Bus block write requests) and multi-
 cast addresses (reachable via asynchronous stream packets) */

typedef struct {              /* IP1394 "hardware" address */
   ULONG deviceID;			  /* Stable reference to unit architecture */
   UCHAR maxRec;              /* Maximum asynchronous payload */
   UCHAR spd;                 /* Maximum speed */
   USHORT unicastFifoHi;      /* Upper 16 bits of unicast FIFO address */
   ULONG unicastFifoLo;       /* Lower 32 bits of unicast FIFO address */
} TNF_UNICAST_HANDLE;

typedef struct {              /* IP1394 "hardware" address */
   ULONG deviceID;   /* Always zero */
   UCHAR maxRec;              /* Maximum asynchronous payload */
   UCHAR spd;                 /* Maximum speed */
   UCHAR reserved;
   UCHAR channel;             /* Channel number for GASP transmit / receive */
   ULONG groupAddress;        /* Distinguish groups that share channel */
} TNF_MULTICAST_HANDLE;

typedef union {
   TNF_UNICAST_HANDLE unicast;
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

typedef struct {
	USHORT size;
	UCHAR  tag:2;
	UCHAR  chan:6;
	UCHAR  tcode:4;
	UCHAR  sy:4;
} ISOC_DATA_PKT;

#define FW_M_BCAST 0x10
#define FW_M_MCAST 0x20
#define FW_M_UCAST 0x40
#define FW_M_PKTHDR 0x02

typedef struct {
	UCHAR sin_len;
	UCHAR sin_family;
	USHORT sin_port;
	ULONG sin_addr;
	UCHAR sin_zero[8];
} SOCKADDR_IN;

typedef struct {
   VOID (*callback)();
   VOID *ipDatagram;
   VOID *socket;
   VOID *passThru1;
   VOID *passThru2;
} ARP_HOLD;

/* Address resolution block (ARB) contains all of the information necessary to
 map, in either direction, between an IPv4 address and a link-level "hardware"
 address */

class ARB : public OSObject		/* Used by both ARP 1394 and MCAP */
{      
	OSDeclareDefaultStructors(ARB);
public:
	UWIDE		eui64;          /* EUI-64 obtained from ARP response */
	UCHAR		fwaddr[8];
	USHORT		timer;          /* Permits the ARB to be "aged" */
	ULONG		ipAddress;      /* IP address */
	TNF_HANDLE	handle;         /* Pseudo "hardware" address used internally */
	BOOLEAN		itsMac;   		/* Indicates whether the destination Macintosh or not */
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
	UCHAR		fwaddr[8];
	ULONG		timer;			/* If nonzero, decrement and release upon zero */
	ULONG		deviceID;		/* Stable "handle" for the IP-capable device */
	USHORT		maxPayload;		/* Maximum payload and... */
	IOFWSpeed	maxSpeed;		/* ...speed to device in current topology */
	BOOLEAN		itsMac;			/* Indicates whether the destination Macintosh or not */
};

/* Multicast control block (MCB) permits the management of multicast channel
 assignments, whether we are the owner or simply one of the participants in
 the multicast group. */

class MCB : public OSObject 
{
public:
   ULONG ownerNodeID;         /* Channel owner (it may be us!) */
   ULONG groupCount;          /* IP address groups active (this channel )*/
   ULONG asyncStreamID;
   UCHAR channel;             /* Redundant (but makes debug easier) */
   UCHAR expiration;          /* Seconds remaining in valid channel mapping */
   UCHAR nextTransmit;        /* Seconds 'til MCAP advertisement transmitted */
   UCHAR finalWarning;        /* Channel deallocation warning messages */
};

#define MCAP_UNOWNED 0        /* No channel owner */

/* Reassembly control block (RCB) tracks the progress of the entire datagram
 as fragments arrive. The algorithm is simple---primitive even---but adequate
 for the unconfirmed nature of IP datagrams. When one of the fragments first
 arrives, an MBUF adequate to hold the entire datagram is allocated and the
 fragment is copied to its correct location; the residual count is decremented.
 This process repeats with successive fragments until residual is zero. */

class RCB : public OSObject
{
	OSDeclareDefaultStructors(RCB);
public:
	USHORT sourceID;           /* Saved from LK_DATA.indication */
	USHORT dgl;                /* Obtained from the fragment header */
	USHORT etherType;          /* Saved from first fraagment header */
	USHORT datagramSize;       /* Total size of the reassembled datagram */
	USHORT residual;           /* Bytes still outstanding */
	ULONG  timer;			  /* If nonzero, decrement and release upon zero */
	UInt8  *datagram;
	mbuf_t mBuf;               /* MBUF eventually passed to OS code */
};

/* End of the type definitions for the miscellaneous control structures */

/* Link control block (LCB) used to maintain context for IOFireWireIP routines for a
 single link instance. The ARP and MCAP caches are also referenced by this
 structure. */
#define MAX_CHANNEL_DES			64

typedef struct lcb /* Link Control Block (LCB) for each link */
{          
   IP1394_HDW_ADDR		ownHardwareAddress; /* Our external address on Serial Bus */
   USHORT			ownMaxPayload;      /* From this link's bus information block */
   USHORT			ownMaxSpeed;        /* Link/PHY hardware capability */
   USHORT			ownNodeID;          /* Management information, only */
   USHORT			maxBroadcastPayload;/* Updated when topology changes */
   IOFWSpeed		maxBroadcastSpeed;  /* Ditto */
   USHORT			datagramLabel;
   ULONG			busGeneration;      /* Current as of most recent bus reset */
} LCB;

#endif /* _IP_FIREWIRE_H_ */
