/*
 * Copyright (c) 2000, Boris Popov
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
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <sysexits.h>

#include <sys/mchain.h>

#include <sys/types.h>

#include <netsmb/smb_lib.h>
#include <netsmb/smb_conn.h>

int
smb_rq_init(struct smb_ctx *ctx, u_char cmd, size_t rpbufsz, struct smb_rq **rqpp)
{
	struct smb_rq *rqp;

	rqp = malloc(sizeof(*rqp));
	if (rqp == NULL)
		return ENOMEM;
	bzero(rqp, sizeof(*rqp));
	rqp->rq_cmd = cmd;
	rqp->rq_ctx = ctx;
	mb_init(&rqp->rq_rq, SMB_LIB_M_MINSIZE);
	mb_init(&rqp->rq_rp, rpbufsz);
	*rqpp = rqp;
	return 0;
}

void
smb_rq_done(struct smb_rq *rqp)
{
	mb_done(&rqp->rq_rp);
	mb_done(&rqp->rq_rq);
	free(rqp);
}

void
smb_rq_wend(struct smb_rq *rqp)
{
	if (rqp->rq_rq.mb_count & 1)
		smb_log_info("smbrq_wend: odd word count\n", 0, ASL_LEVEL_DEBUG);
	rqp->rq_wcount = rqp->rq_rq.mb_count / 2;
	rqp->rq_rq.mb_count = 0;
}

int
smb_rq_dmem(struct mbdata *mbp, const char *src, size_t size)
{
	struct smb_lib_mbuf *m;
	char * dst;
	int cplen, error;

	if (size == 0)
		return 0;
	m = mbp->mb_cur;
	if ((error = smb_lib_mbuf_getm(m, size, &m)) != 0)
		return error;
	while (size > 0) {
		cplen = SMB_LIB_M_TRAILINGSPACE(m);
		if (cplen == 0) {
			m = m->m_next;
			continue;
		}
		if (cplen > (int)size)
			cplen = size;
		dst = SMB_LIB_MTODATA(m, char *) + m->m_len;
		memcpy(dst, src, cplen);
		size -= cplen;
		src += cplen;
		m->m_len += cplen;
		mbp->mb_count += cplen;
	}
	mbp->mb_pos = SMB_LIB_MTODATA(m, char *) + m->m_len;
	mbp->mb_cur = m;
	return 0;
}

int
smb_rq_dstring(struct mbdata *mbp, const char *s)
{
	return smb_rq_dmem(mbp, s, strlen(s) + 1);
}

int
smb_rq_simple(struct smb_rq *rqp)
{
	struct smbioc_rq krq;
	struct mbdata *mbp;
	char *data;

	mbp = smb_rq_getrequest(rqp);
	smb_lib_m_lineup(mbp->mb_top, &mbp->mb_top);
	data = SMB_LIB_MTODATA(mbp->mb_top, char*);
	bzero(&krq, sizeof(krq));
	krq.ioc_cmd = rqp->rq_cmd;
	krq.ioc_twc = rqp->rq_wcount;
	krq.ioc_twords = data;
	krq.ioc_tbc = mbp->mb_count;
	krq.ioc_tbytes = data + rqp->rq_wcount * 2;
	mbp = smb_rq_getreply(rqp);
	krq.ioc_rpbufsz = mbp->mb_top->m_maxlen;
	krq.ioc_rpbuf = SMB_LIB_MTODATA(mbp->mb_top, char *);
	if (ioctl(rqp->rq_ctx->ct_fd, SMBIOC_REQUEST, &krq) == -1) {
		return errno;
	}
	mbp->mb_top->m_len = krq.ioc_rwc * 2 + krq.ioc_rbc;
	rqp->rq_wcount = krq.ioc_rwc;
	rqp->rq_bcount = krq.ioc_rbc;
	return 0;
}

int
smb_t2_request(struct smb_ctx *ctx, int setupcount, u_int16_t *setup,
	const char *name,
	int tparamcnt, void *tparam,
	int tdatacnt, void *tdata,
	int *rparamcnt, void *rparam,
	int *rdatacnt, void *rdata,
	int *buffer_oflow)
{
	struct smbioc_t2rq krq;
	int i;

	if (setupcount < 0 || setupcount > SMB_MAXSETUPWORDS) {
		/* Bogus setup count, or too many setup words */
		return EINVAL;
	}
	bzero(&krq, sizeof(krq));
	for (i = 0; i < setupcount; i++)
		krq.ioc_setup[i] = setup[i];
	krq.ioc_setupcnt = setupcount;
	krq.ioc_name = (char *)name;
	/* 
	 * Now when passing the name into the kernel we also send the length
	 * of the name. The ioc_name_len needs to contain the name length and 
	 * the null byte.
	  */
	if (name)
		krq.ioc_name_len = strlen((char *)name) + 1;
	else
		krq.ioc_name_len = 0;
	krq.ioc_tparamcnt = tparamcnt;
	krq.ioc_tparam = tparam;
	krq.ioc_tdatacnt = tdatacnt;
	krq.ioc_tdata = tdata;
	krq.ioc_rparamcnt = *rparamcnt;
	krq.ioc_rparam = rparam;
	krq.ioc_rdatacnt = *rdatacnt;
	krq.ioc_rdata = rdata;
	if (ioctl(ctx->ct_fd, SMBIOC_T2RQ, &krq) == -1) {
		return errno;
	}
	*rparamcnt = krq.ioc_rparamcnt;
	*rdatacnt = krq.ioc_rdatacnt;
	*buffer_oflow = (krq.ioc_rpflags2 & SMB_FLAGS2_ERR_STATUS) &&
	    (krq.ioc_error == NT_STATUS_BUFFER_OVERFLOW);
	return 0;
}
