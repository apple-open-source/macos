/*
 * NETBIOS protocol formats
 *
 * @(#) $Header: /cvs/Darwin/Commands/Other/tcpdump/tcpdump/netbios.h,v 1.1.1.1 2001/07/07 00:50:53 bbraun Exp $
 */

struct p8022Hdr {
    u_char	dsap;
    u_char	ssap;
    u_char	flags;
};

#define	p8022Size	3		/* min 802.2 header size */

#define UI		0x03		/* 802.2 flags */

