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
 * $Id: print.c,v 1.1.1.3 2001/07/06 22:38:43 conrad Exp $
 */
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>

#include <netsmb/upi_mbuf.h>
#include <sys/mchain.h>
#include <netsmb/smb_lib.h>
#include <netsmb/rq.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_converter.h>

int
smb_smb_open_print_file(struct smb_ctx *ctx, int setuplen, int mode,
	const char *ident, smbfh *fhp)
{
	struct smb_rq *rqp;
	mbchain_t mbp;
	int error;

	error = smb_rq_init(ctx, SMB_COM_OPEN_PRINT_FILE, 0, &rqp);
	if (error)
		return error;
	mbp = smb_rq_getrequest(rqp);
	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, setuplen);
	mb_put_uint16le(mbp, mode);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	smb_rq_put_dstring(ctx, mbp, ident, strlen(ident), SMB_UTF_SFM_CONVERSIONS, NULL);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	if (!error) {
		mdchain_t mdp;

		mdp = smb_rq_getreply(rqp);
		md_get_uint8(mdp, NULL);	/* Word Count */
		md_get_uint16(mdp, fhp);
	}
	smb_rq_done(rqp);
	return error;
}

int
smb_smb_close_print_file(struct smb_ctx *ctx, smbfh fh)
{
	struct smb_rq *rqp;
	mbchain_t mbp;
	int error;

	error = smb_rq_init(ctx, SMB_COM_CLOSE_PRINT_FILE, 0, &rqp);
	if (error)
		return error;
	mbp = smb_rq_getrequest(rqp);
	smb_rq_wstart(rqp);
	mb_put_mem(mbp, (char*)&fh, 2, MB_MSYSTEM);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	smb_rq_done(rqp);
	return error;
}
