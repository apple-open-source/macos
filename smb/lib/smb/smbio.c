/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <netsmb/smbio.h>

/* Perform a SMB_READ or SMB_READX.
 *
 * Return value is -errno if < 0, otherwise the received byte count.
 */
ssize_t
smbio_read(
	void *		smbctx,
	int		fid,
	uint8_t *	buf,
	size_t		buflen)
{
    int bytes;

    bytes = smb_read(smbctx, (uint16_t)fid, 0 /* offset */,
	    (uint32_t)buflen, (char *)buf);
    if (bytes == -1) {
		return -errno;
    }

    return (ssize_t)bytes;
}

/* Perform a TRANSACT_NAMES_PIPE.
 *
 * Return value is -errno if < 0, otherwise the received byte count.
 */
ssize_t
smbio_transact(
	void *		smbctx,
	int		fid,
	const uint8_t *	out_buf,
	size_t		out_len,
	uint8_t *	in_buf,
	size_t		in_len)
{
    uint16_t    setup[2];
    int         rparamcnt = 0;
    int         buffer_oflow = 0;

    int		reply_size;

    int error;

    setup[0] = TRANS_TRANSACT_NAMED_PIPE;
    setup[1] = (uint16_t)fid;
    rparamcnt = 0;
    reply_size = (int)in_len;

    error = smb_t2_request(smbctx, (int)(sizeof(setup) / 2), setup,
	"\\PIPE\\",
	0, NULL,                /* int tparamcnt, void *tparam */
	(int)out_len, (void *)out_buf,    /* int tdatacnt, void *tdata */
	&rparamcnt, NULL,       /* int *rparamcnt, void *rparam */
	&reply_size,		/* int *rdatacnt */
	in_buf,	/* void *rdata */
	&buffer_oflow);

    if (error) {
		return -error;
    }

	if (buffer_oflow) {
		return -EOVERFLOW;
    }

    return reply_size;
}

/* From comsoc_libsmb.c: */
int
smbio_open_pipe(
	void *		smbctx,
	const char *	pipe_path,
	int *		fid)
{
    struct smb_rq *     rqp;
    struct mbdata *     mbp;
    uint8_t		wc;
    size_t              namelen, pathlen, i;
    uint16_t            flags2;

    int			error;

    flags2 = smb_ctx_flags2(smbctx);

    /*
     * Next, open the pipe.
     * XXX - 42 is the biggest reply we expect.
     */
    error = smb_rq_init(smbctx, SMB_COM_NT_CREATE_ANDX, 42, &rqp);
    if (error != 0) {
	return -error;
    }

    mbp = smb_rq_getrequest(rqp);
    mb_put_uint8(mbp, 0xff);        /* secondary command */
    mb_put_uint8(mbp, 0);           /* MBZ */
    mb_put_uint16le(mbp, 0);        /* offset to next command (none) */
    mb_put_uint8(mbp, 0);           /* MBZ */

    if (flags2 & SMB_FLAGS2_UNICODE) {
		namelen = strlen(pipe_path) + 1;
		pathlen = 2 + (namelen * 2);
    } else {
		namelen = strlen(pipe_path) + 1;
		pathlen = 1 + namelen;
    }

    mb_put_uint16le(mbp, pathlen);
    mb_put_uint32le(mbp, 0);        /* create flags */
    mb_put_uint32le(mbp, 0);        /* FID - basis for path if not root */
    mb_put_uint32le(mbp, STD_RIGHT_READ_CONTROL_ACCESS|
			SA_RIGHT_FILE_WRITE_ATTRIBUTES|
			SA_RIGHT_FILE_READ_ATTRIBUTES|
			SA_RIGHT_FILE_WRITE_EA|
			SA_RIGHT_FILE_READ_EA|
			SA_RIGHT_FILE_APPEND_DATA|
			SA_RIGHT_FILE_WRITE_DATA|
			SA_RIGHT_FILE_READ_DATA);
    mb_put_uint64le(mbp, 0);        /* "initial allocation size" */
    mb_put_uint32le(mbp, SMB_EFA_NORMAL);
    mb_put_uint32le(mbp, NTCREATEX_SHARE_ACCESS_READ |
			NTCREATEX_SHARE_ACCESS_WRITE);
    mb_put_uint32le(mbp, NTCREATEX_DISP_OPEN);
    mb_put_uint32le(mbp, NTCREATEX_OPTIONS_NON_DIRECTORY_FILE);
				    /* create_options */
    mb_put_uint32le(mbp, NTCREATEX_IMPERSONATION_IMPERSONATION); /* (?) */
    mb_put_uint8(mbp, 0);   /* security flags (?) */
    smb_rq_wend(rqp);

    if (flags2 & SMB_FLAGS2_UNICODE) {
		mb_put_uint8(mbp, 0);   /* pad byte - needed only for Unicode */
		mb_put_uint16le(mbp, '\\');
		for (i = 0; i < namelen; i++) {
			mb_put_uint16le(mbp, pipe_path[i]);
		}
    } else {
		mb_put_uint8(mbp, '\\');
		for (i = 0; i < namelen; i++) {
			mb_put_uint8(mbp, pipe_path[i]);
		}
    }

    error = smb_rq_simple(rqp);
    if (error != 0) {
		smb_rq_done(rqp);
		if (error == EINVAL) {
			/*
			 * Windows 98, at least - and probably Windows 95
			 * and Windows Me - return ERRSRV/ERRerror when we try to
			 * open the pipe.  Map that to RPC_C_SOCKET_ECONNREFUSED
			 * so that it's treated as an attempt to connect to
			 * a port on which nobody's listening, which is probably
			 * the best match.
			 */
			error = ECONNREFUSED;
		}

		return -error;
    }

    mbp = smb_rq_getreply(rqp);

    /*
     * spec says 26 for word count, but 34 words are defined
     * and observed from win2000
     */
    wc = rqp->rq_wcount;
    if (wc != 26 && wc != 34 && wc != 42) {
		smb_rq_done(rqp);
		return -EIO;
    }

    uint16_t fid16;

    mb_get_uint8(mbp, NULL);        /* secondary cmd */
    mb_get_uint8(mbp, NULL);        /* mbz */
    mb_get_uint16le(mbp, NULL);     /* andxoffset */
    mb_get_uint8(mbp, NULL);        /* oplock lvl granted */
    mb_get_uint16le(mbp, &fid16);      /* FID */
    mb_get_uint32le(mbp, NULL);     /* create_action */
    mb_get_uint64le(mbp, NULL);     /* creation time */
    mb_get_uint64le(mbp, NULL);     /* access time */
    mb_get_uint64le(mbp, NULL);     /* write time */
    mb_get_uint64le(mbp, NULL);     /* change time */
    mb_get_uint32le(mbp, NULL);     /* attributes */
    mb_get_uint64le(mbp, NULL);     /* allocation size */
    mb_get_uint64le(mbp, NULL);     /* EOF */
    mb_get_uint16le(mbp, NULL);     /* file type */
    mb_get_uint16le(mbp, NULL);     /* device state */
    mb_get_uint8(mbp, NULL);        /* directory (boolean) */
    smb_rq_done(rqp);

    *fid = fid16;
    return 0;
}

/* vim: set ts=4 et tw=79 */
