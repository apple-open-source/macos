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

/* "@(#)nbp_send.c: 2.0, 1.12; 9/28/89; Copyright 1988-89, Apple Computer, Inc." */

/*
 * Title:	nbp_send.c
 *
 * Facility:	AppleTalk Name Binding Protocol Library Interface
 *
 * Author:	Gregory Burns, Creation Date: Jun-24-1988
 *
 * History:
 * X01-001	Gregory Burns	24-Jun-1988
 *	 	Initial Creation.
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <mach/boolean.h>

#include <sys/errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <sys/select.h>
#include <net/if.h>

#include <netat/appletalk.h>
#include <netat/at_var.h>
#include <netat/ddp.h>
#include <netat/nbp.h>

#include <AppleTalk/at_proto.h>

#define	SET_ERRNO(e) errno = e

#define	ZONEOK	TRUE
#define	NOZONE	FALSE
#define	METAOK	TRUE
#define	NOMETA	FALSE

static int nbpId = 0;

static int doNbpReply(at_nbp_t *nbpIn, u_char *reply, u_char **replyPtr,
		      int got, int max);
static int nbp_len(at_nbptuple_t *tuple);
static void nbp_pack_tuple(at_entity_t *entity, at_nbptuple_t *tuple);
static void nbp_unpack_tuple(at_nbptuple_t *tuple, at_entity_t *entity);
static void time_diff(struct timeval *time1, struct timeval *time2,
			struct timeval *diff);

int _nbp_send_ (func, addr, name, reply, max, retry)
	u_char		func;
	at_inet_t	*addr; /* used only for nbp_confirm; checked there */
	at_entity_t	*name;
	u_char		*reply;
	int		max;
	at_retry_t	*retry;
{
	at_ddp_t	ddpIn;
	at_nbp_t	*nbpIn = (at_nbp_t *) ddpIn.data;
	struct sockaddr_at local, remote;
	int len, flags = 0;
	at_nbp_t	nbpOutBuf, 
			*nbpOut = &nbpOutBuf;
	at_retry_t	retrybuf;
	int		fd;
	int 		olderrno;
	int		n, got = 0;	/* Number of tuples received */
	fd_set		selmask;
	struct timeval	timeout;
	char		expectedControl;
	u_char		*replyPtr;
	struct	timeval	start_time, stop_time, time_spent, time_remaining;
	struct timezone	tzp;
	int		result, size, ddp_addr_size;
	at_if_cfg_t 	cfg;
	ddp_addr_t	ddp_addr;
	
	SET_ERRNO(0);

	if (max <= 0) {
		fprintf(stderr, "nbp_send: bad max %d\n", max);
		SET_ERRNO(EINVAL);
		return(-1);
	}
	if ((fd = socket(AF_APPLETALK, SOCK_RAW, DDP_NBP)) < 0) {
		fprintf(stderr, "nbp_send: socket failed\n");
		return(-1);
	}
	size = sizeof(struct sockaddr_at);

	bzero((caddr_t)&remote, size);
	remote.sat_len = size;
	remote.sat_family = AF_APPLETALK;
	remote.sat_port = NBP_SOCKET;

	bzero((caddr_t)&local, size);
	local.sat_len = size;
	local.sat_family = AF_APPLETALK;
	if (bind(fd, (struct sockaddr *)&local, size) < 0) {
		fprintf(stderr, "nbp_send: bind failed\n");
		return(-1);
	}
	cfg.ifr_name[0] = '\0'; /* use the default interface */
	if (ioctl(fd, AIOCGETIFCFG, (caddr_t)&cfg) < 0) {
		fprintf(stderr, "nbp_send: AIOCGETIFCFG failed (%s)\n", 
			sys_errlist[errno]);
		(void)close(fd);
		return(-1);
	}

	/* get the "lport" part of the local address of this connection to use 
	   in the "return address" part of the tuple */
	ddp_addr_size = sizeof(ddp_addr);
	if (getsockopt(fd, 0, DDP_GETSOCKNAME, &ddp_addr, &ddp_addr_size) < 0) {
		fprintf(stderr, "nbp_send: getsockopt failed\n");
		return(-1);
	}
	nbpOut->tuple[0].enu_addr = ddp_addr.inet;

	/* fill in the default address, since fd was not bound to a specific
	   local address */
	nbpOut->tuple[0].enu_addr.node = cfg.node.s_node;
	nbpOut->tuple[0].enu_addr.net = cfg.node.s_net;

	if (!retry) {
		retrybuf.retries = NBP_RETRY_COUNT;
		retrybuf.interval = NBP_RETRY_INTERVAL;
		retrybuf.backoff = 1;
	} else {
	        if ((retry->retries <= 0) || (retry->interval <= 0)) {
		  fprintf(stderr, "nbp_send: bad retry %d %d\n", retry->retries,
			 retry->interval);
		  SET_ERRNO(EINVAL);
		  goto out;
		}
		retrybuf.retries = retry->retries; 
		retrybuf.interval = retry->interval;
		retrybuf.backoff = retry->backoff;
	}
	retry = &retrybuf;
	
	/* Set up common NBP header fields */
	nbpOut->control = func;
	nbpOut->tuple_count = 1;
	nbpOut->at_nbp_id = ++nbpId;
	nbpOut->tuple[0].enu_enum = 0;

	if (!_nbp_validate_entity_(name, METAOK, ZONEOK)) {
		fprintf(stderr, "nbp_send: nbp_validate_entity failed\n");
		SET_ERRNO(EINVAL);
		goto out;
	}

	switch (func) {
		case NBP_LKUP:
			if (cfg.router.s_node) {
				remote.sat_addr = cfg.router;
				nbpOut->control = NBP_BRRQ;
			} else {
				/* Check for attempt to lookup non-local zone when 
				   no bridge exists.  Inside AppleTalk, pp VI-5. */
				if (name->zone.str[0] != '*' ) {
					SET_ERRNO(ENETUNREACH);
					goto out;
				}
				remote.sat_addr.s_net = 0;
				remote.sat_addr.s_node = 0xff;
			}
			expectedControl = NBP_LKUP_REPLY;
			SET_ERRNO(0);
			break;

		case NBP_CONFIRM:
			nbpOut->control = NBP_LKUP;
			remote.sat_addr.s_net = addr->net;
			remote.sat_addr.s_node = addr->node;
			expectedControl = NBP_LKUP_REPLY;
			SET_ERRNO(0);
			break;

		default:
			SET_ERRNO(EINVAL);
			fprintf(stderr, "nbp_send: bad func %d\n", func);
			goto out;

	}

	(void)nbp_pack_tuple(name, &nbpOut->tuple[0]);

	len = NBP_HDR_SIZE + nbp_len(&nbpOut->tuple[0]);

	replyPtr = reply;

	while (got < max) {
		if ((result = sendto(fd, nbpOut, len, flags, 
				     (struct sockaddr *)&remote, size)) < 0) {
			fprintf(stderr, "nbp_send: sendto failed\n");
			goto out;
		}
		time_remaining.tv_sec = retry->interval;
		time_remaining.tv_usec = 0;
		timeout = time_remaining;
		gettimeofday(&start_time, &tzp);
poll:
		FD_ZERO(&selmask);
		FD_SET(fd, &selmask);
		switch (select(fd+1, &selmask, 0, 0, &timeout)) {
		  default:
			if ((result = recvfrom(fd, &ddpIn, DDP_DATAGRAM_SIZE,
					       flags, NULL, &size)) < 0) {
					fprintf(stderr, "nbp_send: recvfrom failed\n");
					goto out;
			}
			/* If we re-poll, return immediately */
			timeout.tv_sec = timeout.tv_usec = 0;
			/* Match NBP-ID and must be reply to request func */
			if (nbpIn->at_nbp_id != nbpOut->at_nbp_id) 
				goto poll;
			if (nbpIn->control != expectedControl) 
				goto poll;
				
			n = doNbpReply(nbpIn, reply, &replyPtr, got, max);
			got += n;
			SET_ERRNO(0);
			if (got >= max)
				break;
			goto poll;

			SET_ERRNO(0);
			break;
		case -1:
			/* An error occurred */
			if (errno != EINTR) {
				fprintf(stderr, "nbp_send: select failed\n");
				goto out;
			}				
			/* fall through */
			SET_ERRNO(0);
		case 0:
			/* Nothing found, we timed out */
			gettimeofday (&stop_time, &tzp);
			time_diff(&stop_time, &start_time, &time_spent);
			time_diff(&time_remaining, &time_spent, &timeout);
			time_remaining = timeout;
			if (timeout.tv_sec > 0 || 
			    timeout.tv_usec > 0) {
				gettimeofday(&start_time, &tzp);
				goto poll;
			}
			SET_ERRNO(0);
			break;
		}
		/* Are we finished yet ? */
		if (retry->retries-- == 0) {
			SET_ERRNO(0);
			break;
		}
	}

out:
	olderrno = errno;
	(void)close(fd);
	SET_ERRNO(olderrno);
	
	if (errno)
		return(-1);

	if ((got > 0) && (func == NBP_CONFIRM))
		addr->socket = nbpIn->tuple[0].enu_addr.socket;

	return (got);
} /* _nbp_send_ */

static int doNbpReply(nbpIn, reply, replyPtr, got, max)
	at_nbp_t	*nbpIn;
	u_char		*reply, **replyPtr;
	int		got, max;
{
	int 		i, wegot = 0;
	at_nbptuple_t	*tuple, *tupleIn, *tupleNext;

	tupleIn = &nbpIn->tuple[0];
	tupleNext = * (at_nbptuple_t **) replyPtr;

	for (i = 0; i < (int) nbpIn->tuple_count; i++) {
		for (tuple = (at_nbptuple_t *) reply; tuple < tupleNext; tuple++) {
			if (tuple->enu_enum == tupleIn->enu_enum &&
			    tuple->enu_addr.net == tupleIn->enu_addr.net &&
			    tuple->enu_addr.node == tupleIn->enu_addr.node &&
			    tuple->enu_addr.socket == tupleIn->enu_addr.socket){
				goto skip;
			}
		}
		if (got + wegot >= max)
			break;
		wegot++;

		tupleNext->enu_addr = tupleIn->enu_addr;
		tupleNext->enu_enum = tupleIn->enu_enum;
		(void) nbp_unpack_tuple(tupleIn, &tupleNext->enu_entity);
		tupleNext++;
skip:
		tupleIn = (at_nbptuple_t *) (((u_char *) tupleIn) + nbp_len(tupleIn));
	}
	*replyPtr = (u_char *) tupleNext;
	return (wegot);
}

/* tuple is in the compressed (no "filler") format */
static int nbp_len(tuple)
	at_nbptuple_t	*tuple;
{
	register u_char	*p;
	int		len;

	len = sizeof(at_inet_t) + 1;
	p = (u_char *)&tuple->enu_entity;
	len += *p + 1;
	p += *p + 1;
	len += *p + 1;
	p += *p + 1;
	len += *p + 1;
	return (len);
}

/* tuple is in the compressed (no "filler") format */
static void nbp_pack_tuple(entity, tuple)
	at_entity_t	*entity;
	at_nbptuple_t	*tuple;
{
	register u_char	*p;

	p = (u_char *)&tuple->enu_entity;
	memcpy (p, &entity->object, entity->object.len + 1);
	p += entity->object.len + 1;
	memcpy (p, &entity->type, entity->type.len + 1);
	p += entity->type.len + 1;
	memcpy (p, &entity->zone, entity->zone.len + 1);
}

/* tuple is in the compressed (no "filler") format */
static void nbp_unpack_tuple(tuple, entity)
	at_nbptuple_t	*tuple;
	at_entity_t	*entity;
{
	register u_char	*p;

	p = (u_char *)&tuple->enu_entity;
	memcpy (&entity->object, p, *p + 1);
	p += *p + 1;
	memcpy (&entity->type, p, *p + 1);
	p += *p + 1;
	memcpy (&entity->zone, p, *p + 1);
}

/* returns (time1 - time2) in diff.
 * diff may be the same as either time1 or time2.
 * Will return 0's in diff if the time difference is negative.
 */
static	void	time_diff (time1, time2, diff)
struct	timeval	*time1, *time2, *diff;
{
	int	carry = 0;

	if (time1->tv_usec >= time2->tv_usec) {
		diff->tv_usec = time1->tv_usec - time2->tv_usec;
	} else {
		diff->tv_usec = time1->tv_usec+1000000-time2->tv_usec;
		carry = 1;
	}
	
	if (time1->tv_sec < (time2->tv_sec + carry))
		diff->tv_sec = diff->tv_usec = 0;
	else
		diff->tv_sec = time1->tv_sec - (time2->tv_sec + carry);
	return;
}
