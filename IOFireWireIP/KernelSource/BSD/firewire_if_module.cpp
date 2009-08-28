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

#ifndef INET
#define INET 1
#endif

extern "C"{
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/dlil.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <netinet/in.h>	/* For M_LOOP */

#include <sys/socketvar.h>

#include <net/dlil.h>

#include "firewire.h"
#include <if_firewire.h>
}
#include "IOFireWireIP.h" 

struct fw_desc {
	u_int16_t	type;			/* Type of protocol stored in data */
	u_long 		protocol_family;	/* Protocol family */
	u_long		data[2];		/* Protocol data */
};

#define FIREWIRE_DESC_BLK_SIZE (10)

//
// Statics for demux module
//
struct firewire_desc_blk_str {
    u_long  n_max_used;
    u_long	n_count;
	u_long	n_used;
    struct fw_desc  block_ptr[1];
};
/* Size of the above struct before the array of struct fw_desc */
#define FIREWIRE_DESC_HEADER_SIZE	((size_t)&(((struct firewire_desc_blk_str*)0)->block_ptr[0]))

static ifnet_t	loop_ifp;


////////////////////////////////////////////////////////////////////////////////
//
// firewire_del_proto
//
// IN: ifnet_t ifp, u_long protocol_family 
// 
// Invoked by : 
//  dlil_detach_protocol calls this funcion
// 
// Release all descriptor entries owned by this ifp/protocol_family (there may be several).
// Setting the type to 0 releases the entry. Eventually we should compact-out
// the unused entries.
//
////////////////////////////////////////////////////////////////////////////////
__private_extern__
int  firewire_del_proto(ifnet_t ifp, protocol_family_t protocol_family)
{
	IOFWInterface					*fwIf		= (IOFWInterface*)ifnet_softc(ifp);

	if(fwIf == NULL)
		return EINVAL;
		
	struct firewire_desc_blk_str	*desc_blk	= (struct firewire_desc_blk_str *)fwIf->getFamilyCookie();

	if (desc_blk == NULL)
		return EINVAL;
		
	int		found = 0;
	
	for (u_long current = desc_blk->n_max_used; current > 0; current--) 
	{
		if (desc_blk->block_ptr[current - 1].protocol_family == protocol_family) 
		{
			found = 1;
			desc_blk->block_ptr[current - 1].type = 0;
			desc_blk->n_used--;
		}
	}
	
	if (desc_blk->n_used == 0) 
	{
		FREE(fwIf->getFamilyCookie(), M_IFADDR);
		fwIf->setFamilyCookie(NULL);
	}
	else 
	{
		/* Decrement n_max_used */
		for (; desc_blk->n_max_used > 0 && desc_blk->block_ptr[desc_blk->n_max_used - 1].type == 0; desc_blk->n_max_used--)
			;
	}
	
	return found;
 }

////////////////////////////////////////////////////////////////////////////////
//
// firewire_add_proto
//
// IN: ifnet_t ifp, u_long protocol_family, struct ddesc_head_str *desc_head
// 
// Invoked by : 
//  dlil_attach_protocol calls this funcion
// 
//
////////////////////////////////////////////////////////////////////////////////
__private_extern__ int
firewire_add_proto_internal(ifnet_t ifp, u_long protocol_family, const struct ifnet_demux_desc	*demux)
{
	IOFWInterface					*fwIf		= (IOFWInterface*)ifnet_softc(ifp);

	if(fwIf == NULL)
		return EINVAL;
	
	struct firewire_desc_blk_str	*desc_blk	= (struct firewire_desc_blk_str *)fwIf->getFamilyCookie();

	struct fw_desc	*ed;
	u_long		   i;
   
	switch (demux->type) 
	{
		case DLIL_DESC_ETYPE2:
			if (demux->datalen != 2) 
				return EINVAL;
			break;

		default:
			return EOPNOTSUPP;
	}

	// Check for case where all of the descriptor blocks are in use
	if (desc_blk == NULL || desc_blk->n_used == desc_blk->n_count) 
	{
		struct firewire_desc_blk_str *tmp;
		u_long	new_count = FIREWIRE_DESC_BLK_SIZE;
		u_long	new_size;
		u_long	old_size = 0;

		i = 0;
		if (desc_blk) 
		{
			new_count += desc_blk->n_count;
			old_size = desc_blk->n_count * sizeof(struct fw_desc) + FIREWIRE_DESC_HEADER_SIZE;
			i = desc_blk->n_used;
		}
		
		new_size = new_count * sizeof(struct fw_desc) + FIREWIRE_DESC_HEADER_SIZE;

		tmp = (struct firewire_desc_blk_str*)_MALLOC(new_size, M_IFADDR, M_WAITOK);
		if (tmp  == 0) 
			return ENOMEM;
		
		bzero(tmp + old_size, new_size - old_size);
		if (desc_blk) 
		{
			bcopy(desc_blk, tmp, old_size);
			FREE(desc_blk, M_IFADDR);
		}
		desc_blk = tmp;
		fwIf->setFamilyCookie(desc_blk);
		desc_blk->n_count = new_count;
	}
	else
	{
		// Find a free entry
		for (i = 0; i < desc_blk->n_count; i++) 
		{
			if (desc_blk->block_ptr[i].type == 0) 
				break;
		}
	}
		
	// Bump n_max_used if appropriate
	if (i + 1 > desc_blk->n_max_used) {
		desc_blk->n_max_used = i + 1;
	}
	
	ed = &desc_blk->block_ptr[i];
	ed->protocol_family = protocol_family;
	ed->data[0] = 0;
	ed->data[1] = 0;
	
	switch (demux->type) {
		case DLIL_DESC_ETYPE2:
			/* 2 byte ethernet raw protocol type is at native_type */
			/* prtocol must be in network byte order */
			ed->type = DLIL_DESC_ETYPE2;
			ed->data[0] = *(u_int16_t*)demux->data;
			break;
	}
    
	desc_blk->n_used++;

    return 0;
} 

int
firewire_add_proto(ifnet_t   ifp, protocol_family_t protocol, const struct ifnet_demux_desc *demux_list, u_int32_t demux_count)
{
	int			error = 0;
	u_int32_t	i;
	
	for (i = 0; i < demux_count; i++) 
	{
		error = firewire_add_proto_internal(ifp, protocol, &demux_list[i]);
		if (error) 
		{
			firewire_del_proto(ifp, protocol);
			break;
		}
	}
	
	return error;
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_demux
//
// IN: ifnet_t ifp,struct mbuf  *m,char *frame_header,
//	   u_long *protocol_family
// 
// Invoked by : 
//  dlil_input_packet()
// 
////////////////////////////////////////////////////////////////////////////////
__private_extern__ int firewire_demux(ifnet_t ifp, mbuf_t m, char *frame_header, protocol_family_t *protocol_family)
{
    register struct firewire_header *eh = (struct firewire_header *)frame_header;

	IOFWInterface					*fwIf		= (IOFWInterface*)ifnet_softc(ifp);
	
	if(fwIf == NULL)
		return EINVAL;
	
	struct firewire_desc_blk_str	*desc_blk	= (struct firewire_desc_blk_str *)fwIf->getFamilyCookie();

	if (desc_blk == NULL)
		return EINVAL;

	u_short			fw_type = eh->fw_type;
    u_int16_t		type = DLIL_DESC_ETYPE2;
    u_long			maxd = desc_blk->n_max_used;
    struct fw_desc	*ed = desc_blk->block_ptr;

    /* 
     * Search through the connected protocols for a match. 
     */
	for (u_long i = 0; i < maxd; i++) 
	{
		if ((ed[i].type == type) && (ed[i].data[0] == fw_type)) 
		{
			*protocol_family = ed[i].protocol_family;
			return 0;
		}
	}
    
    return ENOENT;
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_frameout
//
// IN:	ifnet_t ifp,struct mbuf **m
// IN:  struct sockaddr *ndest - contains the destination IP Address 
// IN:	char *edst - filled by firewire_arpresolve function in if_firewire.c
// IN:  char *fw_type 
//
// Invoked by : 
//  dlil.c for dlil_output, Its called after inet_firewire_pre_output
//
// Encapsulate a packet of type family for the local net.
// Use trailer local net encapsulation if enough data in first
// packet leaves a multiple of 512 bytes of data in remainder.
//
////////////////////////////////////////////////////////////////////////////////
__private_extern__ int
firewire_frameout(ifnet_t ifp, mbuf_t *m, 
					const struct sockaddr *ndest, const char *edst, const char *fw_type)
{
	register struct firewire_header *fwh;
		
	/*
	 * If a simplex interface, and the packet is being sent to our
	 * Ethernet address or a broadcast address, loopback a copy.
	 * XXX To make a simplex device behave exactly like a duplex
	 * device, we should copy in the case of sending to our own
	 * ethernet address (thus letting the original actually appear
	 * on the wire). However, we don't do that here for security
	 * reasons and compatibility with the original behavior.
	 */
	 
	if ((ifnet_flags(ifp) & IFF_SIMPLEX) &&
	    (mbuf_flags(*m) & M_LOOP))
	{
		if (loop_ifp == NULL) {
			ifnet_find_by_name("lo0", &loop_ifp);
			
			/*
			 * We make an assumption here that lo0 will never go away. This
			 * means we don't have to worry about releasing the reference
			 * later and we don't have to worry about leaking a reference
			 * every time we are loaded.
			 */
			ifnet_release(loop_ifp);
		}
		
	    if (loop_ifp) 
		{
            if (mbuf_flags(*m) & M_BCAST)
			{
                mbuf_t n;
                
                if (mbuf_copym(*m, 0, MBUF_COPYALL, M_WAITOK, &n) == 0)
                    ifnet_output(loop_ifp, PF_INET, n, 0, ndest);
            } 
            else 
            {
				if (bcmp(edst, ifnet_lladdr(ifp), FIREWIRE_ADDR_LEN) == 0) 
				{
                    ifnet_output(loop_ifp, PF_INET, *m, 0, ndest);
                    return EJUSTRETURN;
                }
            }
	    }
	}

	//
	// Add local net header.  If no space in first mbuf,
	// allocate another.
	//
	if (mbuf_prepend(m, sizeof(struct firewire_header), M_DONTWAIT) != 0)
	    return (EJUSTRETURN);

	//
	// Lets put this intelligent here into the mbuf 
	// so we can demux on our output path
	//
	fwh = (struct firewire_header*)mbuf_data(*m);
	(void)memcpy(&fwh->fw_type, fw_type,sizeof(fwh->fw_type));
	memcpy(fwh->fw_dhost, edst, FIREWIRE_ADDR_LEN);
	(void)memcpy(fwh->fw_shost, ifnet_lladdr(ifp), sizeof(fwh->fw_shost));
	
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_add_if
//
// IN:	ifnet_t ifp
//
////////////////////////////////////////////////////////////////////////////////
__private_extern__
int  firewire_add_if(ifnet_t ifp)
{
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_del_if
//
// IN:	ifnet_t ifp
//
// Invoked by : 
//    firewire_free calls this function
//
////////////////////////////////////////////////////////////////////////////////
__private_extern__
int  firewire_del_if(IOFWInterface	*fwIf)
{
	if (fwIf->getFamilyCookie()) {
		FREE(fwIf->getFamilyCookie(), M_IFADDR);
		return 0;
	}
	else
		return ENOENT;
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_ifmod_ioctl
//
// IN:	ifnet_t ifp
//
// Invoked by : 
//    dlil_ioctl calls this function, all ioctls are handled at 
//    firewire_inet_prmod_ioctl
//
////////////////////////////////////////////////////////////////////////////////
__private_extern__ int
firewire_ifmod_ioctl(ifnet_t ifp, unsigned long cmd, void  *data)
{
	int err = EOPNOTSUPP;
	return err;
}

__private_extern__ int
firewire_init_if(ifnet_t   ifp)
{
    return 0;
}
