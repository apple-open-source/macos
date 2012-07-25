/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2010 Apple Inc. All rights reserved.
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
#ifndef _NETSMB_RQ_H_
#define _NETSMB_RQ_H_

#include <sys/types.h>

struct smb_usr_rq;

int smb_ntwrkpath_to_localpath(struct smb_ctx *, 
							   const char */*ntwrkstr*/, size_t /*ntwrk_len*/,
							   char */*utf8str*/, size_t */*utf8_len*/,
							   uint32_t /*flags*/);
int smb_localpath_to_ntwrkpath(struct smb_ctx *,
							   const char */*utf8str*/, size_t /*utf8_len*/,
							   char */*ntwrkstr*/, size_t */*ntwrk_len*/,
							   uint32_t /*flags*/);

/*
 * Calling smb_usr_rq_init_rcvsize with a request size causes it to allocate a 
 * receive buffer of that size. 
 */
int  smb_usr_rq_init_rcvsize(struct smb_ctx *, u_char, uint16_t, size_t, struct smb_usr_rq **);
/* The smb_usr_rq_init routtine will always allocate a receive buffer of page size. */
int  smb_usr_rq_init(struct smb_ctx *, u_char, uint16_t, struct smb_usr_rq **);
void smb_usr_rq_done(struct smb_usr_rq *);
mbchain_t smb_usr_rq_getrequest(struct smb_usr_rq *);
mdchain_t smb_usr_rq_getreply(struct smb_usr_rq *);
uint32_t smb_usr_rq_get_error(struct smb_usr_rq *);
uint32_t smb_usr_rq_flags2(struct smb_usr_rq *);
uint32_t smb_usr_rq_nt_error(struct smb_usr_rq *rqp);
void smb_usr_rq_setflags2(struct smb_usr_rq *, uint32_t );
void smb_usr_rq_wstart(struct smb_usr_rq *);
void smb_usr_rq_wend(struct smb_usr_rq *);
void smb_usr_rq_bstart(struct smb_usr_rq *);
void smb_usr_rq_bend(struct smb_usr_rq *);
int smb_usr_rq_simple(struct smb_usr_rq *);
int smb_usr_put_dmem(struct smb_ctx *, mbchain_t , const char *, 
            			size_t , int /*flags*/, size_t *);
int smb_usr_rq_put_dstring(struct smb_ctx *, mbchain_t , const char *, size_t, 
							int /*flags*/, size_t *);

int smb_usr_t2_request(struct smb_ctx *ctx, int setupcount, uint16_t *setup, const char *name, 
				   		uint16_t tparamcnt, const void *tparam, 
				   		uint16_t tdatacnt, const void *tdata, 
				   		uint16_t *rparamcnt, void *rparam, 
				   		uint16_t *rdatacnt, void *rdata, 
				   		uint32_t *buffer_oflow);
#endif // _NETSMB_RQ_H_
