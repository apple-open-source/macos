/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <net/dlil.h>

#include "PPPoE.h"
#include "pppoe_rfc.h"
#include "pppoe_dlil.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */
#define LOGVAL LOG_INFO


/* -----------------------------------------------------------------------------
Declarations
----------------------------------------------------------------------------- */
int pppoe_dlil_input(struct mbuf *m, char *frame_header, struct ifnet *ifp,
                     u_long dl_tag, int sync_ok);
int pppoe_dlil_pre_output(struct ifnet *ifp, struct mbuf **m0, struct sockaddr *dst_netaddr,
                          caddr_t route, char *type, char *edst, u_long dl_tag );
int pppoe_dlil_event();
int pppoe_dlil_ioctl(u_long dl_tag, struct ifnet *ifp, u_long command, caddr_t data);


/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */



/* -----------------------------------------------------------------------------
Attach PPPoE to the interface defined by if_family/unit
returns KERN_SUCCESS and the dl_tag when successfully attached
----------------------------------------------------------------------------- */
int pppoe_dlil_attach(u_long if_family, int unit, u_long *dl_tag)
{
    int 			ret;
    struct dlil_proto_reg_str   reg;
    struct dlil_demux_desc      desc, desc2;
    u_int16_t			ctrl_protocol = PPPOE_ETHERTYPE_CTRL;
    u_int16_t			data_protocol = PPPOE_ETHERTYPE_DATA;

    log(LOGVAL, "pppoe_dlil_attach, if_family = %d, unit = %d \n", if_family, unit);

    bzero(&reg, sizeof(struct dlil_proto_reg_str));
    
    // define demux for PPPoE
    TAILQ_INIT(&reg.demux_desc_head);
    bzero(&desc, sizeof(struct dlil_demux_desc));
    desc.type = DLIL_DESC_RAW;
    desc.native_type = (char *) &ctrl_protocol;
    TAILQ_INSERT_TAIL(&reg.demux_desc_head, &desc, next);
    desc2 = desc;
    desc2.native_type = (char *) &data_protocol;
    TAILQ_INSERT_TAIL(&reg.demux_desc_head, &desc2, next);
    
    reg.interface_family = if_family;
    reg.protocol_family  = PF_PPPOE;
    reg.unit_number      = unit;
    reg.input            = pppoe_dlil_input;
    reg.pre_output       = pppoe_dlil_pre_output;
    reg.event            = pppoe_dlil_event;
    reg.ioctl            = pppoe_dlil_ioctl;

    ret = dlil_attach_protocol(&reg, dl_tag);
    if (ret) {
        log(LOG_INFO, "pppoe_dlil_attach: error = 0 x%x\n", ret);
        return ret;
    }
    log(LOG_INFO, "pppoe_dlil_attach: tag = 0x%x\n", *dl_tag);

    return(KERN_SUCCESS);
}

/* -----------------------------------------------------------------------------
detach the protocol identified by tag from the dlil
----------------------------------------------------------------------------- */
int pppoe_dlil_detach(u_long dl_tag)
{

    return dlil_detach_protocol(dl_tag);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int pppoe_dlil_pre_output(struct ifnet *ifp, struct mbuf **m0, struct sockaddr *dst_netaddr,
                          caddr_t route, char *type, char *edst, u_long dl_tag )
{

    struct ether_header 	*eh;

    //log(LOGVAL, "pppenet_pre_output, ifp = 0x%x, dl_tag = 0x%x\n", ifp, dl_tag);

    // fill in expected type and dst from previously set dst_netaddr
    // looks complicated to me
    eh = (struct ether_header *)dst_netaddr->sa_data;


    memcpy(edst, eh->ether_dhost, sizeof(eh->ether_dhost));
    *(u_int16_t *)type = eh->ether_type;

    //log(LOGVAL, "addr = 0x%x%x%x%x%x%x, type = 0x%lx\n", edst[0], edst[1],
    //    edst[2], edst[3], edst[4], edst[5], *type);

    return 0;
}

/* -----------------------------------------------------------------------------
 should handle network up/down...
----------------------------------------------------------------------------- */
int  pppoe_dlil_event()
{

    log(LOGVAL, "pppoe_dlil_event\n");
    return 0;
}

/* -----------------------------------------------------------------------------
pppoe specific ioctl, nothing defined
----------------------------------------------------------------------------- */
int pppoe_dlil_ioctl(u_long dl_tag, struct ifnet *ifp, u_long command, caddr_t data)
{
    
    log(LOGVAL, "pppoe_dlil_ioctl, dl_tag = 0x%x\n", dl_tag);
    return 0;
}


/* -----------------------------------------------------------------------------
* Process a received pppoe packet;
* the packet is in the mbuf chain m without
* the ether header, which is provided separately.
----------------------------------------------------------------------------- */
int pppoe_dlil_input(struct mbuf *m, char *frame_header, struct ifnet *ifp,
                     u_long dl_tag, int sync_ok)
{

//    unsigned char *p = frame_header;
 //   unsigned char *data;
    struct ether_header *eh = (struct ether_header *)frame_header;

 //   log(LOGVAL, "pppenet_input, dl_tag = 0x%x\n", dl_tag);
//    log(LOGVAL, "pppenet_input: enet_header dst : %x:%x:%x:%x:%x:%x - src %x:%x:%x:%x:%x:%x - type - %4x\n",
//        p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10],p[11],*(u_int16_t *)&p[12]);


//    data = mtod(m, unsigned char *);
//    log(LOGVAL, "pppenet_input: data 0x %x %x %x %x %x %x\n",
//        data[0],data[1],data[2],data[3],data[4],data[5]);

    // only the dl_tag discriminate client at this point
    // pppoe will have to look at the session id to select the appropriate socket

    pppoe_rfc_lower_input(dl_tag, m, frame_header + ETHER_ADDR_LEN, eh->ether_type);

    return 0;
}

/* -----------------------------------------------------------------------------
called from pppenet_proto when data need to be sent
----------------------------------------------------------------------------- */
int pppoe_dlil_output(u_long dl_tag, struct mbuf *m, u_int8_t *to, u_int16_t typ)
{
    struct ether_header 	*eh;
    struct sockaddr 		sa;

    eh = (struct ether_header *)sa.sa_data;
    (void)memcpy(eh->ether_dhost, to, sizeof(eh->ether_dhost));
    eh->ether_type = typ; //htons(typ);	/* if_output will not swap */
    sa.sa_family = AF_UNSPEC;
    sa.sa_len = sizeof(sa);

    dlil_output(dl_tag, m, 0, &sa, 0);
    return 0;
}

