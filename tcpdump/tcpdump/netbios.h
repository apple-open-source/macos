/*
 * NETBIOS protocol formats
 *
 * @(#) $Header: /cvs/root/tcpdump/tcpdump/netbios.h,v 1.1.1.2 2003/03/17 18:42:16 rbraun Exp $
 */

struct p8022Hdr {
    u_char	dsap;
    u_char	ssap;
    u_char	flags;
};

#define	p8022Size	3		/* min 802.2 header size */

#define UI		0x03		/* 802.2 flags */

