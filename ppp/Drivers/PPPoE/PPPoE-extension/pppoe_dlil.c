/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <kern/locks.h>


#include <net/dlil.h>

/* include ppp domain as we use the ppp domain family */
#include "../../../Family/ppp_domain.h"

#include "PPPoE.h"
#include "pppoe_rfc.h"
#include "pppoe_dlil.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

struct pppoe_if {
    TAILQ_ENTRY(pppoe_if) next;
    u_short          	unit;
    u_short				refcnt;
    ifnet_t				ifp;
};

/* -----------------------------------------------------------------------------
Declarations
----------------------------------------------------------------------------- */
static errno_t pppoe_dlil_input(ifnet_t ifp, protocol_family_t protocol,
									 mbuf_t packet, char* header);
static errno_t pppoe_dlil_pre_output(ifnet_t ifp, protocol_family_t protocol,
									  mbuf_t *packet, const struct sockaddr *dest,
									  void *route, char *frame_type, char *link_layer_dest);
static void pppoe_dlil_event(ifnet_t ifp, protocol_family_t protocol,
								  const struct kev_msg *event);
static errno_t pppoe_dlil_ioctl(ifnet_t ifp, protocol_family_t protocol,
									 u_long command, void* argument);
static void pppoe_dlil_detaching(u_int16_t unit);


/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

TAILQ_HEAD(, pppoe_if) 	pppoe_if_head;
extern lck_mtx_t		*ppp_domain_mutex;

/* -----------------------------------------------------------------------------
intialize pppoe datalink attachment strutures
----------------------------------------------------------------------------- */
int pppoe_dlil_init()
{

    TAILQ_INIT(&pppoe_if_head);
        
    return 0;
}

/* -----------------------------------------------------------------------------
dispose pppoe datalink structures
can't dispose if clients are still attached
----------------------------------------------------------------------------- */
int pppoe_dlil_dispose()
{
    if (!TAILQ_EMPTY(&pppoe_if_head))
        return 1;
        
    return 0;
}

/* -----------------------------------------------------------------------------
attach pppoe to ethernet unit
returns 0 and the ifp when successfully attached
----------------------------------------------------------------------------- */
int pppoe_dlil_attach(u_short unit, ifnet_t *ifpp)
{
    struct pppoe_if  	*pppoeif;
	ifnet_t				ifp;
    int					ret;
	char				ifname[20];
    struct ifnet_attach_proto_param		reg;
	struct ifnet_demux_desc				desc[2];
    u_int16_t			ctrl_protocol = htons(PPPOE_ETHERTYPE_CTRL);
    u_int16_t			data_protocol = htons(PPPOE_ETHERTYPE_DATA);
    
    TAILQ_FOREACH(pppoeif, &pppoe_if_head, next) {
        if (pppoeif->unit == unit) {
            *ifpp = pppoeif->ifp;
            pppoeif->refcnt++;
            return 0;
        }
    }

    MALLOC(pppoeif, struct pppoe_if *, sizeof(struct pppoe_if), M_TEMP, M_WAITOK);
    if (!pppoeif) {
        IOLog("pppoe_dlil_attach : Can't allocate attachment structure\n");
        return 1;
    }
	snprintf(ifname, sizeof(ifname), "en%d", unit);
	
    if (ifnet_find_by_name(ifname, &ifp)) {
        IOLog("pppoe_dlil_attach : Can't find interface unit %d\n", unit);
        return 1;
    }

    bzero(&reg, sizeof(struct ifnet_attach_proto_param));
    
    // define demux for PPPoE
    desc[0].type = DLIL_DESC_ETYPE2;
    desc[0].data = (char *)&ctrl_protocol;
	desc[0].datalen = sizeof(ctrl_protocol);
    desc[1].type = DLIL_DESC_ETYPE2;
    desc[1].data = (char *)&data_protocol;
	desc[1].datalen = sizeof(data_protocol);
	
    reg.demux_list		 = desc;
    reg.demux_count		 = 2;	
    reg.input            = pppoe_dlil_input;
    reg.pre_output       = pppoe_dlil_pre_output;
    reg.event            = pppoe_dlil_event;
    reg.ioctl            = pppoe_dlil_ioctl;

	lck_mtx_unlock(ppp_domain_mutex);
	ret = ifnet_attach_protocol(ifp, PF_PPP, &reg);
    if (ret) {
		lck_mtx_lock(ppp_domain_mutex);
        IOLog("pppoe_dlil_attach: error = 0x%x\n", ret);
		ifnet_release(ifp);
        FREE(pppoeif, M_TEMP);
        return ret;
    }

    bzero(pppoeif, sizeof(struct pppoe_if));
	
	lck_mtx_lock(ppp_domain_mutex);
	pppoeif->ifp = ifp;
    pppoeif->unit = unit;
    pppoeif->refcnt = 1;
	*ifpp = pppoeif->ifp;

    TAILQ_INSERT_TAIL(&pppoe_if_head, pppoeif, next);
    return 0;
}

/* -----------------------------------------------------------------------------
detach pppoe from ethernet unit
----------------------------------------------------------------------------- */
int pppoe_dlil_detach(ifnet_t ifp)
{
    struct pppoe_if  	*pppoeif;
    
    if (ifp == 0)
        return 0;

    TAILQ_FOREACH(pppoeif, &pppoe_if_head, next) {
        if (pppoeif->ifp == ifp) {
            pppoeif->refcnt--;
            if (pppoeif->refcnt == 0) {
				lck_mtx_unlock(ppp_domain_mutex);
                ifnet_detach_protocol(ifp, PF_PPP);
				ifnet_release(ifp);
				lck_mtx_lock(ppp_domain_mutex);
                TAILQ_REMOVE(&pppoe_if_head, pppoeif, next);
                FREE(pppoeif, M_TEMP);
            }
            break;
        }
    }

    return 0;
}

/* -----------------------------------------------------------------------------
ethernet unit wants to detach
----------------------------------------------------------------------------- */
void pppoe_dlil_detaching(u_int16_t unit)
{
    struct pppoe_if  	*pppoeif;
	
	lck_mtx_lock(ppp_domain_mutex);

    TAILQ_FOREACH(pppoeif, &pppoe_if_head, next) {
        if (pppoeif->unit == unit) {
            pppoe_rfc_lower_detaching(pppoeif->ifp);
            break;
        }
    }
	lck_mtx_unlock(ppp_domain_mutex);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
errno_t pppoe_dlil_pre_output(ifnet_t ifp, protocol_family_t protocol,
									  mbuf_t *packet, const struct sockaddr *dest,
									  void *route, char *frame_type, char *link_layer_dest)
{

    struct ether_header 	*eh;

    //IOLog("pppenet_pre_output, ifp = %p\n", ifp);

    // fill in expected type and dst from previously set dst_netaddr
    // looks complicated to me
    eh = (struct ether_header *)dest->sa_data;

    bcopy(eh->ether_dhost, link_layer_dest, sizeof(eh->ether_dhost));
    *(u_int16_t *)frame_type = eh->ether_type;

    //IOLog("pppoe_dlil_pre_output ifname %s%d, addr = 0x%x:%x:%x:%x:%x:%x, frame_type = 0x%lx\n", ifnet_name(ifp), ifnet_unit(ifp), link_layer_dest[0], link_layer_dest[1],
    //    link_layer_dest[2], link_layer_dest[3], link_layer_dest[4], link_layer_dest[5], *(u_int16_t *)frame_type);

    return 0;
}

/* -----------------------------------------------------------------------------
 should handle network up/down...
----------------------------------------------------------------------------- */
void pppoe_dlil_event(ifnet_t ifp, protocol_family_t protocol,
								  const struct kev_msg *event)
{
    struct net_event_data 	*ev_data;
        
    // check for detaching
    if (event->vendor_code == KEV_VENDOR_APPLE
		&& event->kev_class == KEV_NETWORK_CLASS
		&& event->kev_subclass == KEV_DL_SUBCLASS
		&& event->event_code == KEV_DL_IF_DETACHING) {
        
        ev_data = (struct net_event_data*)event->dv[0].data_ptr;

        if (ev_data->if_family == APPLE_IF_FAM_ETHERNET)
            pppoe_dlil_detaching(ev_data->if_unit);
    }
}

/* -----------------------------------------------------------------------------
pppoe specific ioctl, nothing defined
----------------------------------------------------------------------------- */
errno_t pppoe_dlil_ioctl(ifnet_t ifp, protocol_family_t protocol,
									 u_long command, void* argument)
{
    
    //IOLog("pppoe_dlil_ioctl, ifp = %s%n\n", ifnet_name(ifp), ifnet_unit(ifp));
    return 0;
}


/* -----------------------------------------------------------------------------
* Process a received pppoe packet;
* the packet is in the mbuf chain m without
* the ether header, which is provided separately.
----------------------------------------------------------------------------- */
errno_t pppoe_dlil_input(ifnet_t ifp, protocol_family_t protocol,
									 mbuf_t packet, char* header)
{
    //unsigned char *p = frame_header;
    //unsigned char *data;
    struct ether_header *eh = (struct ether_header *)header;

    //IOLog("pppenet_input, ifp = %s%d\n", ifp->if_name, ifp->if_unit);
    //IOLog("pppenet_input: enet_header dst : %x:%x:%x:%x:%x:%x - src %x:%x:%x:%x:%x:%x - type - %4x\n",
        //p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10],p[11],ntohs(*(u_int16_t *)&p[12]));


    //data = mtod(m, unsigned char *);
    //IOLog("pppenet_input: data 0x %x %x %x %x %x %x\n",
    //    data[0],data[1],data[2],data[3],data[4],data[5]);

    // only the ifp discriminate client at this point
    // pppoe will have to look at the session id to select the appropriate socket

	lck_mtx_lock(ppp_domain_mutex);
    pppoe_rfc_lower_input(ifp, packet, header + ETHER_ADDR_LEN, ntohs(eh->ether_type));
	lck_mtx_unlock(ppp_domain_mutex);

    return 0;
}

/* -----------------------------------------------------------------------------
called from pppenet_proto when data need to be sent
----------------------------------------------------------------------------- */
int pppoe_dlil_output(ifnet_t ifp, mbuf_t m, u_int8_t *to, u_int16_t typ)
{
    struct ether_header 	*eh;
    struct sockaddr 		sa;

    eh = (struct ether_header *)sa.sa_data;
    (void)bcopy(to, eh->ether_dhost, sizeof(eh->ether_dhost));
    eh->ether_type = htons(typ);
    sa.sa_family = AF_UNSPEC;
    sa.sa_len = sizeof(sa);

	lck_mtx_unlock(ppp_domain_mutex);
    ifnet_output(ifp, PF_PPP, m, 0, &sa);
	lck_mtx_lock(ppp_domain_mutex);
    return 0;
}

