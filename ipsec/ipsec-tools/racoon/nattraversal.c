/*
 * Copyright (C) 2004 SuSE Linux AG, Nuernberg, Germany.
 * Contributed by: Michal Ludvig <mludvig@suse.cz>, SUSE Labs
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>

#include <netinet/udp.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "debug.h"

#include "localconf.h"
#include "remoteconf.h"
#include "sockmisc.h"
#include "isakmp_var.h"
#include "isakmp.h"
#include "oakley.h"
#include "ipsec_doi.h"
#include "vendorid.h"
#include "handler.h"
#include "crypto_openssl.h"
#include "schedule.h"
#include "nattraversal.h"
#include "grabmyaddr.h"
#include "ike_session.h"

struct natt_ka_addrs {
  struct sockaddr_storage	*src;
  struct sockaddr_storage	*dst;
  unsigned		in_use;

  TAILQ_ENTRY(natt_ka_addrs) chain;
};

/*
 * check if the given vid is NAT-T.
 */
int
natt_vendorid (int vid)
{
  return (
#ifdef ENABLE_NATT_00
	  vid == VENDORID_NATT_00 ||
#endif
#ifdef ENABLE_NATT_01
	  vid == VENDORID_NATT_01 ||
#endif
#ifdef ENABLE_NATT_02
	  vid == VENDORID_NATT_02 ||
	  vid == VENDORID_NATT_02_N ||
#endif
#ifdef ENABLE_NATT_03
	  vid == VENDORID_NATT_03 ||
#endif
#ifdef ENABLE_NATT_04
	  vid == VENDORID_NATT_04 ||
#endif
#ifdef ENABLE_NATT_05
	  vid == VENDORID_NATT_05 ||
#endif
#ifdef ENABLE_NATT_06
	  vid == VENDORID_NATT_06 ||
#endif
#ifdef ENABLE_NATT_07
	  vid == VENDORID_NATT_07 ||
#endif
#ifdef ENABLE_NATT_08
	  vid == VENDORID_NATT_08 ||
#endif
#ifdef ENABLE_NATT_APPLE
	  vid == VENDORID_NATT_APPLE ||
#endif
	  /* Always enable NATT RFC if ENABLE_NATT
	   */
	  vid == VENDORID_NATT_RFC);
}

vchar_t *
natt_hash_addr (phase1_handle_t *iph1, struct sockaddr_storage *addr)
{
  vchar_t *natd;
  vchar_t *buf;
  char *ptr;
  void *addr_ptr, *addr_port;
  size_t buf_size, addr_size;

	if (iph1->approval) {
		plog(ASL_LEVEL_INFO, "Hashing %s with algo #%d %s\n",
			 saddr2str((struct sockaddr *)addr), iph1->approval->hashtype, 
			 (iph1->rmconf->nat_traversal == NATT_FORCE)?"(NAT-T forced)":"");
	}
  
  if (addr->ss_family == AF_INET) {
    addr_size = sizeof (struct in_addr);	/* IPv4 address */
    addr_ptr = &((struct sockaddr_in *)addr)->sin_addr;
    addr_port = &((struct sockaddr_in *)addr)->sin_port;
  }
  else if (addr->ss_family == AF_INET6) {
    addr_size = sizeof (struct in6_addr);	/* IPv6 address */
    addr_ptr = &((struct sockaddr_in6 *)addr)->sin6_addr;
    addr_port = &((struct sockaddr_in6 *)addr)->sin6_port;
  }
  else {
    plog(ASL_LEVEL_ERR, "Unsupported address family #0x%x\n", addr->ss_family);
    return NULL;
  }

  buf_size = 2 * sizeof (cookie_t);	/* CKY-I + CKY+R */
  buf_size += addr_size + 2;	/* Address + Port */
  
  if ((buf = vmalloc (buf_size)) == NULL)
    return NULL;

  ptr = buf->v;
  
  /* Copy-in CKY-I */
  memcpy (ptr, iph1->index.i_ck, sizeof (cookie_t));
  ptr += sizeof (cookie_t);
  
  /* Copy-in CKY-I */
  memcpy (ptr, iph1->index.r_ck, sizeof (cookie_t));
  ptr += sizeof (cookie_t);
  
  /* Copy-in Address (or zeroes if NATT_FORCE) */
  if (iph1->rmconf->nat_traversal == NATT_FORCE)
    memset (ptr, 0, addr_size);
  else
    memcpy (ptr, addr_ptr, addr_size);
  ptr += addr_size;

  /* Copy-in Port number */
  memcpy (ptr, addr_port, 2);

  natd = oakley_hash (buf, iph1);
  vfree(buf);

  return natd;
}

int 
natt_compare_addr_hash (phase1_handle_t *iph1, vchar_t *natd_received,
			int natd_seq)
{
  vchar_t *natd_computed;
  u_int32_t flag;
  int verified = 0;

  if (iph1->rmconf->nat_traversal == NATT_FORCE)
    return verified;

	/* old APPLE version sends natd payload in the wrong order */
  if (iph1->natt_options->version == VENDORID_NATT_APPLE) {
	  if (natd_seq == 0) {
		natd_computed = natt_hash_addr (iph1, iph1->remote);
		flag = NAT_DETECTED_PEER;
	  }
	  else {
		natd_computed = natt_hash_addr (iph1, iph1->local);
		flag = NAT_DETECTED_ME;
	  }
	} else
	{
		if (natd_seq == 0) {
			natd_computed = natt_hash_addr (iph1, iph1->local);
			flag = NAT_DETECTED_ME;
		}
		else {
			natd_computed = natt_hash_addr (iph1, iph1->remote);
			flag = NAT_DETECTED_PEER;
		}
	}

  if (natd_received->l == natd_computed->l &&
      memcmp (natd_received->v, natd_computed->v, natd_received->l) == 0) {
    iph1->natt_flags &= ~flag;
    verified = 1;
  }

    if (iph1->parent_session)
        iph1->parent_session->natt_flags = iph1->natt_flags;

  vfree (natd_computed);

  return verified;
}

int
natt_udp_encap (int encmode)
{
  return (encmode == IPSECDOI_ATTR_ENC_MODE_UDPTUNNEL_RFC || 
	  encmode == IPSECDOI_ATTR_ENC_MODE_UDPTRNS_RFC ||
	  encmode == IPSECDOI_ATTR_ENC_MODE_UDPTUNNEL_DRAFT ||
	  encmode == IPSECDOI_ATTR_ENC_MODE_UDPTRNS_DRAFT);
}

int
natt_fill_options (struct ph1natt_options *opts, int version)
{
  if (! opts)
    return -1;

  opts->version = version;

  switch (version) {
    case VENDORID_NATT_02:
    case VENDORID_NATT_02_N:
    case VENDORID_NATT_03:
      opts->float_port = lcconf->port_isakmp_natt;
      opts->payload_nat_d = ISAKMP_NPTYPE_NATD_DRAFT;
      opts->payload_nat_oa = ISAKMP_NPTYPE_NATOA_DRAFT;
      opts->mode_udp_tunnel = IPSECDOI_ATTR_ENC_MODE_UDPTUNNEL_DRAFT;
      opts->mode_udp_transport = IPSECDOI_ATTR_ENC_MODE_UDPTRNS_DRAFT;
      opts->encaps_type = UDP_ENCAP_ESPINUDP;
      break;
    case VENDORID_NATT_04:
    case VENDORID_NATT_05:
    case VENDORID_NATT_06:
    case VENDORID_NATT_07:
    case VENDORID_NATT_08:
	case VENDORID_NATT_APPLE:
      opts->float_port = lcconf->port_isakmp_natt;
      opts->payload_nat_d = ISAKMP_NPTYPE_NATD_BADDRAFT;
      opts->payload_nat_oa = ISAKMP_NPTYPE_NONE;
      opts->mode_udp_tunnel = IPSECDOI_ATTR_ENC_MODE_UDPTUNNEL_RFC;
      opts->mode_udp_transport = IPSECDOI_ATTR_ENC_MODE_UDPTRNS_RFC;
      opts->encaps_type = UDP_ENCAP_ESPINUDP;
      break;
    case VENDORID_NATT_RFC:
      opts->float_port = lcconf->port_isakmp_natt;
      opts->payload_nat_d = ISAKMP_NPTYPE_NATD_RFC;
      opts->payload_nat_oa = ISAKMP_NPTYPE_NATOA_RFC;
      opts->mode_udp_tunnel = IPSECDOI_ATTR_ENC_MODE_UDPTUNNEL_RFC;
      opts->mode_udp_transport = IPSECDOI_ATTR_ENC_MODE_UDPTRNS_RFC;
      opts->encaps_type = UDP_ENCAP_ESPINUDP;
	  break;
    default:
      plog(ASL_LEVEL_ERR, 
	   "unsupported NAT-T version: %s\n",
	   vid_string_by_id(version));
      return -1;
  }
 
  opts->mode_udp_diff = opts->mode_udp_tunnel - IPSECDOI_ATTR_ENC_MODE_TUNNEL;

  return 0;
}

int
create_natoa_payloads(phase2_handle_t *iph2, vchar_t **natoa_i, vchar_t **natoa_r)
{
	int natoa_type = 0;
	vchar_t		*i;
	vchar_t		*r;
	u_int8_t	*p;
	struct sockaddr_storage *i_addr;
	struct sockaddr_storage *r_addr;
	size_t		i_size;
	size_t		r_size;
	
	*natoa_i = *natoa_r = NULL;
	

	/* create natoa payloads if natt being used */
	/* don't send if type == apple				*/
	if (!iph2->ph1->natt_options)
		return 0;

	natoa_type = iph2->ph1->natt_options->payload_nat_oa;
	if (natoa_type == 0)
		return 0;

	if (iph2->side == INITIATOR) {
		i_addr = iph2->src;
		r_addr = iph2->dst;
	} else {
		i_addr = iph2->dst;
		r_addr = iph2->src;
	}

	switch (i_addr->ss_family) {
		case AF_INET:
			i_size = sizeof(in_addr_t);
			break;
#ifdef INET6
		case AF_INET6:
			i_size = sizeof(struct in6_addr);
			break;
#endif
		default:
			plog(ASL_LEVEL_ERR, 
				 "invalid address family: %d\n", i_addr->ss_family);
			return -1;		
	}

	switch (r_addr->ss_family) {
		case AF_INET:
			r_size = sizeof(in_addr_t);
			break;
#ifdef INET6
		case AF_INET6:
			r_size = sizeof(struct in6_addr);
			break;
#endif
		default:
			plog(ASL_LEVEL_ERR, 
				 "invalid address family: %d\n", r_addr->ss_family);
			return -1;		
	}

	i = vmalloc(sizeof(struct isakmp_pl_natoa) + i_size - sizeof(struct isakmp_gen));
	if (i == NULL) {
		plog(ASL_LEVEL_ERR, 
			 "failed to get buffer for natoa payload.\n");
		return -1;
	}
	r = vmalloc(sizeof(struct isakmp_pl_natoa) + r_size - sizeof(struct isakmp_gen));
	if (r == NULL) {
		vfree(i);
		plog(ASL_LEVEL_ERR, 
			"failed to get buffer for natoa payload.\n");
		return -1;
	}
	
	/* copy src address */
	p = (__typeof__(p))i->v;
	
	switch (i_addr->ss_family) {
		case AF_INET:
			*p = IPSECDOI_ID_IPV4_ADDR;
			bcopy(&(((struct sockaddr_in *)i_addr)->sin_addr.s_addr), p + sizeof(u_int32_t), i_size);
			break;
#ifdef INET6
		case AF_INET6:
			*p = IPSECDOI_ID_IPV6_ADDR;
			bcopy(&(((struct sockaddr_in6 *)i_addr)->sin6_addr), p + sizeof(u_int32_t), i_size);
			break;
#endif
	}

	/* copy dst address */
	p = (__typeof__(p))r->v;
	
	switch (r_addr->ss_family) {
		case AF_INET:
			*p = IPSECDOI_ID_IPV4_ADDR;
			bcopy(&(((struct sockaddr_in *)r_addr)->sin_addr.s_addr), p + sizeof(u_int32_t), r_size);
			break;
#ifdef INET6
		case AF_INET6:
			*p = IPSECDOI_ID_IPV6_ADDR;
			bcopy(&(((struct sockaddr_in6 *)r_addr)->sin6_addr), p + sizeof(u_int32_t), r_size);
			break;
#endif
	}
	
	*natoa_i = i;
	*natoa_r = r;
	return natoa_type;
}	

struct sockaddr_storage *
process_natoa_payload(vchar_t *buf)
{
	struct sockaddr_storage      *saddr = NULL;
	struct ipsecdoi_id_b *id_b = ALIGNED_CAST(struct ipsecdoi_id_b *)buf->v;

	switch (id_b->type) {
		case IPSECDOI_ID_IPV4_ADDR:
			saddr = racoon_malloc(sizeof(struct sockaddr_in));
			if (!saddr) {
				plog(ASL_LEVEL_ERR, 
					 "error allocating addr for NAT-OA payload\n");
				return NULL;
			}
			saddr->ss_len = sizeof(struct sockaddr_in);
			saddr->ss_family = AF_INET;
			((struct sockaddr_in *)saddr)->sin_port = IPSEC_PORT_ANY;
			memcpy(&((struct sockaddr_in *)saddr)->sin_addr,
				   buf->v + sizeof(*id_b), sizeof(struct in_addr));
			break;
#ifdef INET6
		case IPSECDOI_ID_IPV6_ADDR:
			saddr = racoon_malloc(sizeof(struct sockaddr_in6));
			if (!saddr) {
				plog(ASL_LEVEL_ERR, 
					 "error allocating addr for NAT-OA payload\n");
				return NULL;
			}
			saddr->ss_len = sizeof(struct sockaddr_in6);
			saddr->ss_family = AF_INET6;
			((struct sockaddr_in6 *)saddr)->sin6_port = IPSEC_PORT_ANY;
			memcpy(&((struct sockaddr_in6 *)saddr)->sin6_addr,
				   buf->v + sizeof(*id_b), sizeof(struct in6_addr));
			break;
#endif
		default:
			plog(ASL_LEVEL_ERR, 
				 "invalid NAT-OA payload %d\n", id_b->type);
			return NULL;
	}
	return saddr;
}

void
natt_float_ports (phase1_handle_t *iph1)
{
	
	if (! (iph1->natt_flags & NAT_DETECTED) )
		return;
	if (! iph1->natt_options->float_port){
		/* Drafts 00 / 01, just schedule keepalive */
		return;
	}
	
	/*
	 * Only switch ports if port == isakmp port.
	 * In the case where ports are set from policy or from
	 * remote config we could be talking to a device behind
	 * a nat using the translated port.
	 */
	if (*get_port_ptr(iph1->local) == htons(lcconf->port_isakmp))
		set_port (iph1->local, iph1->natt_options->float_port);
	if (*get_port_ptr(iph1->remote) == htons(lcconf->port_isakmp))
		set_port (iph1->remote, iph1->natt_options->float_port);
	iph1->natt_flags |= NAT_PORTS_CHANGED | NAT_ADD_NON_ESP_MARKER;

    if (iph1->version == ISAKMP_VERSION_NUMBER_IKEV1)
        ike_session_ikev1_float_ports(iph1);
}

void
natt_handle_vendorid (phase1_handle_t *iph1, int vid_numeric)
{
  int version;

  if (! iph1->natt_options)
    iph1->natt_options = racoon_calloc (1, sizeof (*iph1->natt_options));

  if (! iph1->natt_options) {
    plog (ASL_LEVEL_ERR, 
	  "Allocating memory for natt_options failed!\n");
    return;
  }

  // stick to the version we already selected on a previous phase1
  version = ike_session_get_natt_version(iph1);
  if (version) {
    vid_numeric = version;
  }

  if (iph1->natt_options->version < vid_numeric)
    if (natt_fill_options (iph1->natt_options, vid_numeric) == 0)
      iph1->natt_flags |= NAT_ANNOUNCED;
}

static struct remoteconf *
natt_enabled_in_rmconf_stub (struct remoteconf *rmconf, void *data)
{
  return (rmconf->nat_traversal ? rmconf : NULL);
}

int
natt_enabled_in_rmconf ()
{
  return foreachrmconf (natt_enabled_in_rmconf_stub, NULL) != NULL;
}


struct payload_list *
isakmp_plist_append_natt_vids (struct payload_list *plist, vchar_t *vid_natt[MAX_NATT_VID_COUNT]){
	int i, vid_natt_i = 0;

	if(vid_natt == NULL)
		return NULL;

	for (i = 0; i < MAX_NATT_VID_COUNT; i++)
		vid_natt[i]=NULL;
	
	/* Puts the olders VIDs last, as some implementations may choose the first
	 * NATT VID given
	 */

	/* Always set RFC VID
	 */
	if ((vid_natt[vid_natt_i] = set_vendorid(VENDORID_NATT_RFC)) != NULL)
		vid_natt_i++;
#ifdef ENABLE_NATT_APPLE
	if ((vid_natt[vid_natt_i] = set_vendorid(VENDORID_NATT_APPLE)) != NULL)
		vid_natt_i++;
#endif
#ifdef ENABLE_NATT_08
	if ((vid_natt[vid_natt_i] = set_vendorid(VENDORID_NATT_08)) != NULL)
		vid_natt_i++;
#endif
#ifdef ENABLE_NATT_07
	if ((vid_natt[vid_natt_i] = set_vendorid(VENDORID_NATT_07)) != NULL)
		vid_natt_i++;
#endif
#ifdef ENABLE_NATT_06
	if ((vid_natt[vid_natt_i] = set_vendorid(VENDORID_NATT_06)) != NULL)
		vid_natt_i++;
#endif
#ifdef ENABLE_NATT_05
	if ((vid_natt[vid_natt_i] = set_vendorid(VENDORID_NATT_05)) != NULL)
		vid_natt_i++;
#endif
#ifdef ENABLE_NATT_04
	if ((vid_natt[vid_natt_i] = set_vendorid(VENDORID_NATT_04)) != NULL)
		vid_natt_i++;
#endif
#ifdef ENABLE_NATT_03
	if ((vid_natt[vid_natt_i] = set_vendorid(VENDORID_NATT_03)) != NULL)
		vid_natt_i++;
#endif
#ifdef ENABLE_NATT_02
	if ((vid_natt[vid_natt_i] = set_vendorid(VENDORID_NATT_02)) != NULL)
		vid_natt_i++;
	if ((vid_natt[vid_natt_i] = set_vendorid(VENDORID_NATT_02_N)) != NULL)
		vid_natt_i++;
#endif
#ifdef ENABLE_NATT_01
	if ((vid_natt[vid_natt_i] = set_vendorid(VENDORID_NATT_01)) != NULL)
		vid_natt_i++;
#endif
#ifdef ENABLE_NATT_00
	if ((vid_natt[vid_natt_i] = set_vendorid(VENDORID_NATT_00)) != NULL)
		vid_natt_i++;
#endif
	/* set VID payload for NAT-T */
	for (i = 0; i < vid_natt_i; i++)
		plist = isakmp_plist_append(plist, vid_natt[i], ISAKMP_NPTYPE_VID);
	
	return plist;
}
