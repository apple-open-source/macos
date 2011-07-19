/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#ifndef _NETSMB_SMB_TRANTCP_H_
#define	_NETSMB_SMB_TRANTCP_H_

#ifdef _KERNEL

#ifdef NB_DEBUG
#define NBDEBUG(format, args...)	 printf("%s(%d): "format,	\
					    __FUNCTION__ , __LINE__ ,## args)
#else
#define NBDEBUG(format, args...)
#endif

enum nbstate {
	NBST_CLOSED,
	NBST_RQSENT,
	NBST_SESSION,
	NBST_RETARGET,
	NBST_REFUSED
};


#define	NBF_LOCADDR		0x0001		/* has local addr */
#define	NBF_CONNECTED	0x0002
#define	NBF_RECVLOCK	0x0004
#define	NBF_UPCALLED	0x0010
#define	NBF_NETBIOS		0x0020	


/*
 * socket specific data
 */
struct nbpcb {
	struct smb_vc *	nbp_vc;
	socket_t	nbp_tso;	/* transport socket */
	struct sockaddr_nb *nbp_laddr;	/* local address */
	struct sockaddr_nb *nbp_paddr;	/* peer address */

	int		nbp_flags;
	enum nbstate	nbp_state;
	struct timespec	nbp_timo;
	uint32_t		nbp_sndbuf;
	uint32_t		nbp_rcvbuf;
	void *		nbp_selectid;
	void		(* nbp_upcall)(void *);
	lck_mtx_t	nbp_lock;

/*	LIST_ENTRY(nbpcb) nbp_link;*/
};

/*
 * TCP slowstart presents a problem in conjunction with large
 * reads.  To ensure a steady stream of ACKs while reading using
 * large transaction sizes, we call soreceive() with a smaller
 * buffer size.  See nbssn_recv().
 */
#define NB_SORECEIVE_CHUNK	(8 * 1024)

extern lck_grp_attr_t *nbp_grp_attr;
extern lck_grp_t *nbp_lck_group;
extern lck_attr_t *nbp_lck_attr;

extern struct smb_tran_desc smb_tran_nbtcp_desc;

#define SMBSBTIMO		5 /* seconds for sockbuf timeouts */
#define SMB_SB_RCVTIMEO 15 /* seconds before we give up on a sock_receivembuf */

#endif /* _KERNEL */

#endif /* !_NETSMB_SMB_TRANTCP_H_ */
