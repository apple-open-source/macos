/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2007 Apple Inc. All rights reserved.
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

#ifndef _NETSMB_SMB_TRAN_H_
#define	_NETSMB_SMB_TRAN_H_

#include <sys/socket.h>

/*
 * Known transports
 */
#define	SMBT_NBTCP	1

/*
 * Transport parameters
 */
#define	SMBTP_SNDSZ     1   /* R  - int */
#define	SMBTP_RCVSZ     2   /* R  - int */
#define	SMBTP_TIMEOUT   3   /* RW - struct timespec */
#define	SMBTP_SELECTID  4   /* RW - (void *) */
#define SMBTP_UPCALL    5   /* RW - (* void)(void *) */
#define SMBTP_QOS       6   /* RW - uint32_t */
#define SMBTP_IP_ADDR   7   /* R  - struct sockaddr_storage */
#define SMBTP_BOUND_IF  8   /* W  - uint32_t */

struct smb_tran_ops;

struct smb_tran_desc {
	sa_family_t	tr_type;
	int	 (*tr_create)(struct smbiod *iod);                          /* smb_nbst_create */
	int	 (*tr_done)(struct smbiod *iod);                            /* smb_nbst_done */
	int	 (*tr_bind)(struct smbiod *iod, struct sockaddr *sap);      /* smb_nbst_bind */
	int	 (*tr_connect)(struct smbiod *iod, struct sockaddr *sap);   /* smb_nbst_connect */
	int	 (*tr_disconnect)(struct smbiod *iod);                      /* smb_nbst_disconnect */
	int	 (*tr_send)(struct smbiod *iod, mbuf_t m0);                 /* smb_nbst_send */
	int	 (*tr_recv)(struct smbiod *iod, mbuf_t *mpp);               /* smb_nbst_recv */
	void (*tr_timo)(struct smbiod *iod);                            /* smb_nbst_timo */
	int	 (*tr_getparam)(struct smbiod *iod, int param, void *data); /* smb_nbst_getparam */
	int	 (*tr_setparam)(struct smbiod *iod, int param, void *data); /* smb_nbst_setparam */
	int	 (*tr_fatal)(struct smbiod *iod, int error);                /* smb_nbst_fatal */
	LIST_ENTRY(smb_tran_desc)	tr_link;
};

#define SMB_TRAN_CREATE(iod)            (iod)->iod_tdesc->tr_create(iod)
#define SMB_TRAN_DONE(iod)              (iod)->iod_tdesc->tr_done(iod)
#define	SMB_TRAN_BIND(iod,sap)          (iod)->iod_tdesc->tr_bind(iod,sap)
#define	SMB_TRAN_CONNECT(iod,sap)       (iod)->iod_tdesc->tr_connect(iod,sap)
#define	SMB_TRAN_DISCONNECT(iod)        (iod)->iod_tdesc->tr_disconnect(iod)
#define	SMB_TRAN_SEND(iod,m0)           (iod)->iod_tdesc->tr_send(iod,m0)
#define	SMB_TRAN_RECV(iod,m)            (iod)->iod_tdesc->tr_recv(iod,m)
#define	SMB_TRAN_TIMO(iod)              (iod)->iod_tdesc->tr_timo(iod)
#define	SMB_TRAN_GETPARAM(iod,par,data) (iod)->iod_tdesc->tr_getparam(iod, par, data)
#define	SMB_TRAN_SETPARAM(iod,par,data) (iod)->iod_tdesc->tr_setparam(iod, par, data)
#define	SMB_TRAN_FATAL(iod, error)      (iod)->iod_tdesc->tr_fatal(iod, error)

#endif /* _NETSMB_SMB_TRAN_H_ */
