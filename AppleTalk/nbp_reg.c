/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *	Copyright (c) 1988, 1989, 1998 Apple Computer, Inc. 
 *
 *	The information contained herein is subject to change without
 *	notice and  should not be  construed as a commitment by Apple
 *	Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *	for any errors that may appear.
 *
 *	Confidential and Proprietary to Apple Computer, Inc.
 */

/* "@(#)nbp_reg.c: 2.0, 1.10; 9/27/89; Copyright 1988-89, Apple Computer, Inc." */

/*
 * Title:	nbp_reg.c
 *
 * Facility:	AppleTalk Name Binding Protocol Library Interface
 *
 * Author:	Gregory Burns, Creation Date: Jul-14-1988
 *
 * History:
 * X01-001	Gregory Burns	14-Jul-1988
 *	 	Initial Creation.
 *
 */
#include <stdio.h>
#include <unistd.h>
#include <mach/boolean.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <net/if.h>

#include <netat/appletalk.h>
#include <netat/at_var.h>
#include <netat/nbp.h>

#include <AppleTalk/at_proto.h>

#define	SET_ERRNO(e) errno = e

#define IFR_NEXT(ifr)   \
  ((struct ifreq *) ((char *) (ifr) + sizeof(*(ifr)) + \
      MAX(0, (int) (ifr)->ifr_addr.sa_len - (int) sizeof((ifr)->ifr_addr))))
		
/*
  nbp_reg_lookup()

  Used to make sure an NBP entity does not exist before it is registered.

  Returns 1 	if the entity already exists, 
  	  0 	if the entry does not exist
	 -1	for an error; e.g.no local zones exist

  Does the right thing in multihoming mode, namely if the zone is
  "*" (the default), it does the lookup in the default zone for
  each interface.

*/
#define MAX_IF		16

int nbp_reg_lookup(entity, retry)
     at_entity_t     *entity;
     at_retry_t      *retry;
{
	int i, s, got, cnt = 1;
	at_state_t global_state;
	at_nbptuple_t tuple;
	static at_entity_t entity_copy[IF_TOTAL_MAX]; 
		/* only one home zone per port allowed so this is safe */

	SET_ERRNO(0);
	entity_copy[0] = *entity;

	if ((s = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0)
		return(-1);

	if (ioctl(s, AIOCGETSTATE, (caddr_t)&global_state)) {
		(void)close(s);
		return(-1);
	}

	/* in multihome mode, get the default zone for each interface */
	if ((global_state.flags & AT_ST_MULTIHOME) && (entity->zone.str[0] == '*'))
	{
		/* for each interface that is configured for Appletalk */
		struct ifconf	ifc;
		struct ifreq	*ifrbuf = NULL, *ifr;
		at_if_cfg_t 	cfg;
		int				size = sizeof(struct ifreq) * MAX_IF;
		
		while (1) {
			if (ifrbuf != NULL)
				ifrbuf = (struct ifreq *)realloc(ifrbuf, size);
			else
				ifrbuf = (struct ifreq *)malloc(size);
				
			ifc.ifc_req = ifrbuf;
			ifc.ifc_len = size;
	
			if (ioctl(s, SIOCGIFCONF, &ifc) < 0 || ifc.ifc_len <= 0) {
				fprintf(stderr, "nbp_reg_lookup: SIOCGIFCONF error");
				(void)close(s);
				if (ifrbuf)
					free(ifrbuf);
				return(-1);
			}
	
			if ((ifc.ifc_len + sizeof(struct ifreq)) < size)
				break;
				
			size *= 2;
		}

		for (ifr = (struct ifreq *) ifc.ifc_buf;
		     (char *) ifr < &ifc.ifc_buf[ifc.ifc_len];
		     ifr = IFR_NEXT(ifr)) {
		  	unsigned char *p, c;

			if (ifr->ifr_addr.sa_family != AF_APPLETALK)
				continue;

			if (*ifr->ifr_name == '\0')
				continue;

			/*
			 * Adapt to buggy kernel implementation (> 9 of a type)
			 */
			p = &ifr->ifr_name[strlen(ifr->ifr_name)-1];
			if ((c = *p) > '0'+9)
			  	sprintf(p, "%d", c-'0');

			strcpy(cfg.ifr_name, ifr->ifr_name);
			if (ioctl(s, AIOCGETIFCFG, (caddr_t)&cfg) < 0)
				continue;

			/* if there's room, terminate the zone string for printing */
			if (cfg.zonename.len < NBP_NVE_STR_SIZE)
			  cfg.zonename.str[cfg.zonename.len] = '\0';

			if (cnt)
			  entity_copy[cnt] = entity_copy[0];
			entity_copy[cnt++].zone = cfg.zonename;
		}
		if(!cnt) {
			fprintf(stderr,"nbp_reg_lookup: no local zones\n");
			(void)close(s);
			SET_ERRNO(ENOENT);
			return(-1);
		}
	}
	(void)close(s);
	for (i = 0; i < cnt; i++) {
#ifdef APPLETALK_DEBUG
		entity_copy[i].object.str[entity_copy[i].object.len] = '\0';
		entity_copy[i].type.str[entity_copy[i].type.len] = '\0';
		entity_copy[i].zone.str[entity_copy[i].zone.len] = '\0';
		printf("entity %d = %s|%s|%s\n", i, entity_copy[i].object.str,
		       entity_copy[i].type.str, entity_copy[i].zone.str);
#endif
		SET_ERRNO(0);
		if ((got = nbp_lookup(&entity_copy[i], &tuple, 1, retry)) < 0)  {
			SET_ERRNO(EAGAIN);
			return(-1);
		}
		if (got > 0) {
			SET_ERRNO(EADDRNOTAVAIL);
			return(1);
		}
	}
	return(0);
} /* nbp_reg_lookup */

int nbp_register(entity, fd, retry)
	at_entity_t	*entity;
	int		fd;
	at_retry_t	*retry;
{
	int		if_id;
	ddp_addr_t	ddp_addr;
	at_nbp_reg_t    reg;

	if (fd < 0) {
		SET_ERRNO(EBADF);
		return (-1);
	}

	if (ddp_config(fd, &ddp_addr) < 0)
		return (-1);

	if (nbp_iswild(entity)) {
		fprintf(stderr,"nbp_register: object and type cannot be wild\n"); 
		SET_ERRNO(EINVAL);
		return (-1);
	}
	
	if (nbp_reg_lookup(entity, retry))
		return (-1);

	if ((if_id = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0)
		return(-1);

	reg.name = *entity;
	/* The net and node will be 0 if no bind() has been done on the
	   socket, but that's fine since they will be automatically filled in
	   with address information from the default interface in the kernel. */
	reg.addr = ddp_addr.inet;
	reg.ddptype = ddp_addr.ddptype;

	if ((ioctl(if_id, AIOCNBPREG, (caddr_t)&reg)) < 0) {
		(void)close(if_id);
		return(-1);
	}

	(void)close(if_id);
	return (0);
} /* nbp_register */	
