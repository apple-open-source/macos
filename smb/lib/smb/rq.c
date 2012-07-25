/*
 * Copyright (c) 2000, Boris Popov
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
#include <sys/types.h>

#include <netsmb/upi_mbuf.h>
#include <sys/smb_byte_order.h>
#include <sys/mchain.h>
#include <netsmb/smb_lib.h>
#include <netsmb/rq.h>
#include <netsmb/smb_conn.h>
#include <smbclient/ntstatus.h>

struct smb_usr_rq {
	u_char			rq_cmd;
	struct mbchain	rq_rq;
	struct mdchain	rq_rp;
	struct smb_ctx *rq_ctx;
	uint8_t			*wcount;
	uint16_t		*bcount;
	uint8_t			flags;
	uint16_t		flags2;
	uint32_t		nt_error;
};

/*
 * Takes a Window style UTF-16 string and converts it to a Mac style UTF-8 string
 * that is not null terminated.
 */
int smb_ntwrkpath_to_localpath(struct smb_ctx *ctx, 
							   const char *ntwrkstr, size_t ntwrk_len,
							   char *utf8str, size_t *utf8_len,
							   uint32_t flags)
{
	struct smbioc_path_convert rq;
	
	bzero(&rq, sizeof(rq));
	rq.ioc_version = SMB_IOC_STRUCT_VERSION;
	rq.ioc_direction = NETWORK_TO_LOCAL;
	rq.ioc_flags = flags;
	rq.ioc_src_len = (uint32_t)ntwrk_len;
	rq.ioc_src = ntwrkstr;
	rq.ioc_dest_len = *utf8_len;
	rq.ioc_dest = utf8str;
	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_CONVERT_PATH, &rq) == 0) {
		*utf8_len = (size_t)rq.ioc_dest_len;
		/* 
		 * Currently the kernel doesn't null terminate, we may want to change
		 * this later. Since utf8_len has to be less than the utf8str buffer 
		 * size this is safe.
		 */
		utf8str[*utf8_len] = 0;
		return 0;
	}
	return -1;
}

/*
 * Takes a Mac style UTF-8 path string and converts it to a Window style UTF-16
 * that is not null terminated.
 */ 
int smb_localpath_to_ntwrkpath(struct smb_ctx *ctx,
							   const char *utf8str, size_t utf8_len,
							   char *ntwrkstr, size_t *ntwrk_len,
							   uint32_t flags)
{
	struct smbioc_path_convert rq;
	
	bzero(&rq, sizeof(rq));
	rq.ioc_version = SMB_IOC_STRUCT_VERSION;
	rq.ioc_direction = LOCAL_TO_NETWORK;
	rq.ioc_flags = flags;
	rq.ioc_src_len = (uint32_t)utf8_len;
	rq.ioc_src = utf8str;
	rq.ioc_dest_len = *ntwrk_len;
	rq.ioc_dest = ntwrkstr;
	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_CONVERT_PATH, &rq) == 0) {
		*ntwrk_len = (size_t)rq.ioc_dest_len;
		return 0;
	}
	return -1;
}

int smb_usr_rq_init(struct smb_ctx *ctx, u_char cmd, uint16_t flags2, struct smb_usr_rq **rqpp)
{
	struct smb_usr_rq *rqp;
	
	rqp = malloc(sizeof(*rqp));
	if (rqp == NULL)
		return ENOMEM;
	bzero(rqp, sizeof(*rqp));
	rqp->rq_cmd = cmd;
	rqp->rq_ctx = ctx;
	smb_usr_rq_setflags2(rqp, flags2);
	mb_init(&rqp->rq_rq);
	md_init(&rqp->rq_rp);
	*rqpp = rqp;
	return 0;
}

int smb_usr_rq_init_rcvsize(struct smb_ctx *ctx, u_char cmd, uint16_t flags2, 
						size_t rpbufsz, struct smb_usr_rq **rqpp)
{
	struct smb_usr_rq *rqp;

	rqp = malloc(sizeof(*rqp));
	if (rqp == NULL)
		return ENOMEM;
	bzero(rqp, sizeof(*rqp));
	rqp->rq_cmd = cmd;
	rqp->rq_ctx = ctx;
	smb_usr_rq_setflags2(rqp, flags2);
	mb_init(&rqp->rq_rq);
	md_init_rcvsize(&rqp->rq_rp, rpbufsz);
	*rqpp = rqp;
	return 0;
}

void
smb_usr_rq_done(struct smb_usr_rq *rqp)
{
	mb_done((mbchain_t)&rqp->rq_rp);
	md_done((mdchain_t)&rqp->rq_rq);
	free(rqp);
}

mbchain_t smb_usr_rq_getrequest(struct smb_usr_rq *rqp)
{
	return &rqp->rq_rq;
}


mdchain_t smb_usr_rq_getreply(struct smb_usr_rq *rqp)
{
	return &rqp->rq_rp;
}

uint32_t smb_usr_rq_get_error(struct smb_usr_rq *rqp)
{
	return rqp->nt_error;
}

uint32_t smb_usr_rq_flags2(struct smb_usr_rq *rqp)
{
	return rqp->flags2;
}

uint32_t smb_usr_rq_nt_error(struct smb_usr_rq *rqp)
{
	return rqp->nt_error;
}

void smb_usr_rq_setflags2(struct smb_usr_rq *rqp, uint32_t flags2)
{
	/* We only support SMB_FLAGS2_DFS, log the failure reset the flags to zero */
	if (flags2 && (flags2 != SMB_FLAGS2_DFS)) {
		smb_log_info("%s to 0x%x, syserr = %s", ASL_LEVEL_DEBUG, __FUNCTION__, 
					 flags2, strerror(ENOTSUP));
		flags2 = 0;
	}
	rqp->flags2 = flags2;
}

void smb_usr_rq_wstart(struct smb_usr_rq *rqp)
{
	rqp->wcount = (uint8_t *)mb_reserve(&rqp->rq_rq, sizeof(uint8_t));
	rqp->rq_rq.mb_count = 0;
}

void smb_usr_rq_wend(struct smb_usr_rq *rqp)
{
	if (rqp->wcount == NULL) {
		smb_log_info("%s: no word count pointer?", ASL_LEVEL_ERR, __FUNCTION__);
		return;
	}
	if (rqp->rq_rq.mb_count & 1)
		smb_log_info("%s: odd word count", ASL_LEVEL_ERR, __FUNCTION__);
	*rqp->wcount = rqp->rq_rq.mb_count / 2;
}

void smb_usr_rq_bstart(struct smb_usr_rq *rqp)
{
	rqp->bcount = (uint16_t *)mb_reserve(&rqp->rq_rq, sizeof(uint16_t));
	rqp->rq_rq.mb_count = 0;
}

void smb_usr_rq_bend(struct smb_usr_rq *rqp)
{
	uint16_t bcnt;
	
	if (rqp->bcount == NULL) {
		smb_log_info("%s: no byte count pointer?", ASL_LEVEL_ERR, __FUNCTION__);
		return;
	}
	/*
	 * Byte Count field should be ignored when dealing with  SMB_CAP_LARGE_WRITEX 
	 * or SMB_CAP_LARGE_READX messages. So if byte cound becomes to big we set
	 * it to zero and hope the calling process knows what it is doing. 
	 */
	if (rqp->rq_rq.mb_count > 0x0ffff) 
		bcnt = 0; /* Set the byte count to zero here */
	else
		bcnt = (uint16_t)rqp->rq_rq.mb_count;
	
	*rqp->bcount = htoles(bcnt);
}

/*
 * Given a UTF8 string, convert it to a network string and place it in
 * the buffer. 
 */
int smb_usr_put_dmem(struct smb_ctx *ctx, mbchain_t mbp, const char *src, 
            			size_t src_size, int flags, size_t *lenp)
{
	size_t dest_size = (src_size * 2) + 2 + 2;
	void *dst = NULL;
	int error = 0;

	/* Nothing to do here */
	if (src_size == 0)
		goto done;

    mb_put_padbyte(mbp);

	dst = mb_getbuffer(mbp, dest_size);
	if (dst == NULL) {
		smb_log_info("%s: mb_getbuffer failed, syserr = %s", ASL_LEVEL_DEBUG, 
					 __FUNCTION__, strerror(ENOMEM));
		error = ENOMEM;
		goto done;
	}
	memset(dst, 0, dest_size);
	
	if (smb_localpath_to_ntwrkpath(ctx, src, src_size, dst, &dest_size, flags)) {
		smb_log_info("%s: local to ntwrk path failed, syserr = %s", 
					 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(errno));
		error = errno;
		goto done;		
	}
	mb_consume(mbp, dest_size);
	if (lenp)
		*lenp += dest_size;
done:
	return error;
}

/*
 * Given a UTF8 string, convert it to a network string and place it in
 * the buffer. Now add the NULL byte/bytes.
 */
int smb_usr_rq_put_dstring(struct smb_ctx *ctx, mbchain_t mbp, const char *src, 
					size_t maxlen, int flags, size_t *lenp)
{
	int error;
	
	error = smb_usr_put_dmem(ctx, mbp, src, maxlen, flags, lenp);
	if (error)
		return error;
	
    if (lenp )
        *lenp += 2;
    return mb_put_uint16le(mbp, 0);
}

int
smb_usr_rq_simple(struct smb_usr_rq *rqp)
{
	struct smbioc_rq krq;
	mbchain_t mbp;
	mdchain_t mdp;

	mbp = smb_usr_rq_getrequest(rqp);
	mb_pullup(mbp);
	bzero(&krq, sizeof(krq));
	krq.ioc_version = SMB_IOC_STRUCT_VERSION;
	krq.ioc_cmd = rqp->rq_cmd;
	krq.ioc_twc = (rqp->wcount) ? *(rqp->wcount) : 0;
	krq.ioc_twords = (rqp->wcount) ? rqp->wcount+sizeof(*rqp->wcount) : NULL;
	krq.ioc_tbc = (rqp->bcount) ? letohs(*(rqp->bcount)) : 0;
	krq.ioc_tbytes = (rqp->bcount) ? (uint8_t *)rqp->bcount+sizeof(*rqp->bcount) : NULL;
	krq.ioc_flags2 = rqp->flags2;

	mdp = smb_usr_rq_getreply(rqp);
	krq.ioc_rpbufsz = (int32_t)mbuf_maxlen(mdp->md_top);
	krq.ioc_rpbuf = mbuf_data(mdp->md_top);
	
	if (smb_ioctl_call(rqp->rq_ctx->ct_fd, SMBIOC_REQUEST, &krq) == -1) {
		smb_log_info("%s: smb_ioctl_call, syserr = %s", ASL_LEVEL_DEBUG, 
					 __FUNCTION__, strerror(errno));
		return errno;
	}
	mbuf_setlen(mdp->md_top, krq.ioc_rpbufsz);
	rqp->flags = krq.ioc_flags; 
	rqp->flags2 = krq.ioc_flags2; 
	rqp->nt_error = krq.ioc_ntstatus;
	/*
	 * Returning an error to the IOCTL code means no other information in the
	 * structure will be updated. The nsmb_dev_ioctl routine should only return 
	 * unexpected internal errors. We should return all errors in the ioctl 
	 * structure and not through the ioctl call. This will be corrected by the
	 * following:
	 * <rdar://problem/7082077> "SMB error handling is just confusing and needs to be rewritten"
	 */
	return krq.ioc_errno;
}

int
smb_usr_t2_request(struct smb_ctx *ctx, int setupcount, uint16_t *setup,
			   const char *name,
			   uint16_t tparamcnt, const void *tparam,
			   uint16_t tdatacnt, const void *tdata,
			   uint16_t *rparamcnt, void *rparam,
			   uint16_t *rdatacnt, void *rdata,
			   uint32_t *buffer_oflow)
{
	struct smbioc_t2rq krq;
	int i;

	if (setupcount < 0 || setupcount > SMB_MAXSETUPWORDS) {
		/* Bogus setup count, or too many setup words */
		return EINVAL;
	}
	bzero(&krq, sizeof(krq));
	krq.ioc_version = SMB_IOC_STRUCT_VERSION;
	for (i = 0; i < setupcount; i++)
		krq.ioc_setup[i] = setup[i];
	krq.ioc_setupcnt = setupcount;
	krq.ioc_name = name;
	/* 
	 * Now when passing the name into the kernel we also send the length
	 * of the name. The ioc_name_len needs to contain the name length and 
	 * the null byte.
	  */
	if (name)
		krq.ioc_name_len = (uint32_t)strlen(name) + 1;
	else
		krq.ioc_name_len = 0;
	krq.ioc_tparamcnt = tparamcnt;
	krq.ioc_tparam = (void *)tparam;
	krq.ioc_tdatacnt = tdatacnt;
	krq.ioc_tdata = (void *)tdata;
	krq.ioc_rparamcnt = *rparamcnt;
	krq.ioc_rparam = rparam;
	krq.ioc_rdatacnt = *rdatacnt;
	krq.ioc_rdata = rdata;
	
	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_T2RQ, &krq) == -1) {
		return errno;
	}
	*rparamcnt = krq.ioc_rparamcnt;
	*rdatacnt = krq.ioc_rdatacnt;
	*buffer_oflow = (krq.ioc_flags2 & SMB_FLAGS2_ERR_STATUS) && 
					(krq.ioc_ntstatus == STATUS_BUFFER_OVERFLOW);
	return 0;
}
