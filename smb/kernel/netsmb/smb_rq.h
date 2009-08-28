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
#ifndef _NETSMB_SMB_RQ_H_
#define	_NETSMB_SMB_RQ_H_

#ifndef MB_MSYSTEM
#include <sys/mchain.h>
#endif

#define	SMBR_ALLOCED		0x0001	/* structure was malloced */
#define SMBR_ASYNC			0x0002	/* This is async request, should never be set if SMBR_INTERNAL is set */
#define	SMBR_REXMIT			0x0004	/* Request was retransmitted during a reconnect*/
#define	SMBR_INTR			0x0008	/* request interrupted */
#define	SMBR_RECONNECTED	0x0010	/* The message was handled during a reconnect */
#define	SMBR_DEAD			0x0020	/* Network down nothing we can do with it. */
#define	SMBR_MULTIPACKET	0x0040	/* multiple packets can be sent and received */
#define	SMBR_INTERNAL		0x0080	/* request is internal to smbrqd runs off the main thread. */
#define	SMBR_XLOCK			0x0100	/* request locked and can't be moved */
#define	SMBR_XLOCKWANT		0x0200	/* waiter on XLOCK */
#define	SMBR_VCREF			0x4000	/* took vc reference */
#define	SMBR_MOREDATA		0x8000	/* our buffer was too small */

#define SMBT2_ALLSENT		0x0001	/* all data and params are sent */
#define SMBT2_ALLRECV		0x0002	/* all data and params are received */
#define	SMBT2_ALLOCED		0x0004
#define SMBT2_SECONDARY		0x0020
#define	SMBT2_MOREDATA		0x8000	/* our buffer was too small */

#define SMBRQ_SLOCK(rqp)	lck_mtx_lock(&(rqp)->sr_slock)
#define SMBRQ_SUNLOCK(rqp)	lck_mtx_unlock(&(rqp)->sr_slock)
#define SMBRQ_SLOCKPTR(rqp)	(&(rqp)->sr_slock)

/* Need to look at this and see why SMBRQ_NOTSENT must be zero? */
enum smbrq_state {
	SMBRQ_NOTSENT = 0x00,		/* rq have data to send */
	SMBRQ_SENT = 0x01,		/* send procedure completed */
	SMBRQ_NOTIFIED = 0x04,		/* owner notified about completion */
	SMBRQ_RECONNECT = 0x08		/* Reconnect hold till done */
};

struct smb_vc;

#define MAX_SR_RECONNECT_CNT	5

struct smb_rq {
	enum smbrq_state	sr_state;
	struct smb_vc * 	sr_vc;
	struct smb_share*	sr_share;
	u_int32_t		sr_reconnect_cnt;
	u_short			sr_mid;
	u_int32_t		sr_seqno;
	u_int32_t		sr_rseqno;
	struct mbchain	sr_rq;
	u_int8_t		sr_cmd;
	u_int8_t		sr_rqflags;
	u_int16_t		sr_rqflags2;
	u_char *		sr_wcount;
	u_short *		sr_bcount;
	struct mdchain		sr_rp;
	int			sr_rpgen;
	int			sr_rplast;
	u_int32_t	sr_flags;	/* SMBR_* */
	int			sr_rpsize;
	vfs_context_t	sr_context;
	int			sr_timo;
	struct timespec 	sr_timesent;
	int			sr_lerror;
	u_int8_t *		sr_rqsig;
	u_int16_t *		sr_rqtid;
	u_int16_t *		sr_rquid;
	u_int8_t		sr_errclass;
	u_int16_t		sr_serror;
	u_int32_t		sr_error;	/* Note: This is the NT Status, if the flag is set */
	u_int8_t		sr_rpflags;
	u_int16_t		sr_rpflags2;
	u_int16_t		sr_rptid;
	u_int16_t		sr_rppid;
	u_int16_t		sr_rpuid;
	u_int16_t		sr_rpmid;
	lck_mtx_t		sr_slock;	/* short term locks */
	struct smb_t2rq *sr_t2;
	TAILQ_ENTRY(smb_rq)	sr_link;
	void *sr_callback_args;
	void (*sr_callback)(void *);
};

struct smb_t2rq {
	u_int16_t	t2_setupcount;
	u_int16_t *	t2_setupdata;
	u_int16_t	t2_setup[SMB_MAXSETUPWORDS];
	u_int8_t	t2_maxscount;	/* max setup words to return */
	u_int16_t	t2_maxpcount;	/* max param bytes to return */
	u_int16_t	t2_maxdcount;	/* max data bytes to return */
	u_int16_t       t2_fid;		/* for T2 request */
	char *		t_name;		/* for T, should be zero for T2 */
	int		t2_flags;	/* SMBT2_ */
	struct mbchain	t2_tparam;	/* parameters to transmit */
	struct mbchain	t2_tdata;	/* data to transmit */
	struct mdchain	t2_rparam;	/* received paramters */
	struct mdchain	t2_rdata;	/* received data */
	vfs_context_t	t2_context;
	struct smb_connobj * t2_source;
	struct smb_rq *	t2_rq;
	struct smb_vc * t2_vc;
	struct smb_share * t2_share;	/* for smb up/down */
	/* unmapped windows error detail */
	u_int8_t	t2_sr_errclass;
	u_int16_t	t2_sr_serror;
	u_int32_t	t2_sr_error;
	u_int16_t	t2_sr_rpflags2;
};

struct smb_ntrq {
	u_int16_t	nt_function;
	u_int8_t	nt_maxscount;	/* max setup words to return */
	u_int32_t	nt_maxpcount;	/* max param bytes to return */
	u_int32_t	nt_maxdcount;	/* max data bytes to return */
	int		nt_flags;	/* SMBT2_ */
	struct mbchain	nt_tsetup;	/* setup to transmit */
	struct mbchain	nt_tparam;	/* parameters to transmit */
	struct mbchain	nt_tdata;	/* data to transmit */
	struct mdchain	nt_rparam;	/* received paramters */
	struct mdchain	nt_rdata;	/* received data */
	vfs_context_t	nt_context;
	struct smb_connobj * nt_source;
	struct smb_rq *	nt_rq;
	struct smb_vc * nt_vc;
	struct smb_share * nt_share;	/* for smb up/down */
	/* unmapped windows error details */
	u_int32_t	nt_sr_error;
	u_int16_t	nt_sr_rpflags2;
};

void mbuf_cat_internal(mbuf_t md_top, mbuf_t m0);
int  smb_rq_alloc(struct smb_connobj *layer, u_char cmd, vfs_context_t context, struct smb_rq **rqpp);
int smb_rq_init(struct smb_rq *rqp, struct smb_connobj *layer, u_char cmd, vfs_context_t context);
void smb_rq_done(struct smb_rq *rqp);
int  smb_rq_getrequest(struct smb_rq *rqp, struct mbchain **mbpp);
int  smb_rq_getreply(struct smb_rq *rqp, struct mdchain **mbpp);
void smb_rq_wstart(struct smb_rq *rqp);
void smb_rq_wend(struct smb_rq *rqp);
void smb_rq_bstart(struct smb_rq *rqp);
void smb_rq_bend(struct smb_rq *rqp);
int  smb_rq_intr(struct smb_rq *rqp);
int  smb_rq_simple(struct smb_rq *rqp);
int  smb_rq_simple_timed(struct smb_rq *rqp, int timo);

int  smb_t2_alloc(struct smb_connobj *layer, u_short setup, vfs_context_t context,
	struct smb_t2rq **rqpp);
int  smb_t2_init(struct smb_t2rq *rqp, struct smb_connobj *layer, u_short *setup,
	int setupcnt, vfs_context_t context);
void smb_t2_done(struct smb_t2rq *t2p);
int  smb_t2_request(struct smb_t2rq *t2p);
u_int32_t smb_t2_err(struct smb_t2rq *t2p);

int smb_nt_alloc(struct smb_connobj *layer, u_short fn, vfs_context_t context, struct smb_ntrq **rqpp);
void smb_nt_done(struct smb_ntrq *ntp);
int  smb_nt_request(struct smb_ntrq *ntp);
int  smb_nt_reply(struct smb_ntrq *ntp);
int smb_nt_async_request(struct smb_ntrq *ntp, void *nt_callback, void *nt_callback_args);

#endif /* !_NETSMB_SMB_RQ_H_ */
