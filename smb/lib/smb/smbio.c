/*
 * Copyright (c) 2008 - 2010 Apple Inc. All rights reserved.
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

#include "smbclient.h"
#include <netsmb/smbio.h>
#include <netsmb/smbio_2.h>
#include <netsmb/upi_mbuf.h>
#include <sys/mchain.h>
#include <netsmb/rq.h>
#include <netsmb/smb_converter.h>
#include <sys/smb_byte_order.h>

/*
 * Generic routine that handles open/creates.
 */
int 
smbio_ntcreatex(void *smbctx, const char *path, const char *streamName, 
				struct open_inparms *inparms, struct open_outparm *outparms, 
				int *fid)
{
	uint16_t *namelenp;
    struct smb_usr_rq *rqp;
    mbchain_t	mbp;
    mdchain_t	mdp;
    uint8_t		wc;
    size_t		nmlen;
    int			error;
	u_int16_t	fid16;
	
    /*
	 * Since the reply will fit in one mbuf, pass zero which will cause a normal
	 * mbuf to get created.
     */
    error = smb_usr_rq_init(smbctx, SMB_COM_NT_CREATE_ANDX, 0, &rqp);
    if (error != 0) {
		return error;
    }
	
    mbp = smb_usr_rq_getrequest(rqp);
	smb_usr_rq_wstart(rqp);
    mb_put_uint8(mbp, 0xff);        /* secondary command */
    mb_put_uint8(mbp, 0);           /* MBZ */
    mb_put_uint16le(mbp, 0);        /* offset to next command (none) */
    mb_put_uint8(mbp, 0);           /* MBZ */
	namelenp = (uint16_t *)mb_reserve(mbp, sizeof(uint16_t));
	/*
	 * XP to W2K Server never sets the NTCREATEX_FLAGS_OPEN_DIRECTORY
	 * for creating nor for opening a directory. Samba ignores the bit.
	 *
	 * Request the extended reply to get maximal access
	 */
	mb_put_uint32le(mbp, NTCREATEX_FLAGS_EXTENDED);	/* NTCREATEX_FLAGS_* */
	mb_put_uint32le(mbp, 0);	/* FID - basis for path if not root */
	mb_put_uint32le(mbp, inparms->rights);
	mb_put_uint64le(mbp, inparms->allocSize);	/* "initial allocation size" */
    mb_put_uint32le(mbp, inparms->attrs);	/* attributes */
 	mb_put_uint32le(mbp, inparms->shareMode);
	mb_put_uint32le(mbp, inparms->disp);
    mb_put_uint32le(mbp, inparms->createOptions);
    mb_put_uint32le(mbp, NTCREATEX_IMPERSONATION_IMPERSONATION); /* (?) */
    mb_put_uint8(mbp, 0);   /* security flags (?) */
    smb_usr_rq_wend(rqp);
	smb_usr_rq_bstart(rqp);
	nmlen = 0;
	if (streamName) {
		size_t snmlen = 0;
		
		error = smb_usr_put_dmem(smbctx, mbp, path, strlen(path), 
                        			SMB_UTF_SFM_CONVERSIONS | SMB_FULLPATH_CONVERSIONS, 
                    				&nmlen);
		if (!error) {
			/* Make sure the stream name starts with a colon */
			if (*streamName != ':') {
				mb_put_uint16le(mbp, ':');
				nmlen += 2;
			}
			error = smb_usr_rq_put_dstring(smbctx, mbp, streamName, strlen(streamName), 
                                           NO_SFM_CONVERSIONS, &snmlen);
		}
		nmlen += snmlen;
	} else {
		error = smb_usr_rq_put_dstring(smbctx, mbp, path, strlen(path), 
                        				SMB_UTF_SFM_CONVERSIONS | 
                        				SMB_FULLPATH_CONVERSIONS, 
                    					&nmlen);
	}
	if (error) {
		smb_log_info("%s: smb_usr_rq_put_dstring failed syserr = %s", 
					 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
		goto done;
	}
	/* Now the network name length into the reserved location */
	*namelenp = htoles((uint16_t)nmlen);
	smb_usr_rq_bend(rqp);
    error = smb_usr_rq_simple(rqp);
	if (error != 0) {
		smb_log_info("%s: smb_usr_rq_simple failed, syserr = %s", 
					 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
		goto done;
	}
	
	mdp = smb_usr_rq_getreply(rqp);
	/*
	 * Spec say 26 for word count, but 34 words are defined and observed from 
	 * all servers.  
	 *
	 * The spec is wrong and word count should always be 34 unless we request 
	 * the extended reply. Now some server will always return 42 even it the 
	 * NTCREATEX_FLAGS_EXTENDED flag is not set.
	 * 
	 * From the MS-SMB document concern the extend response:
	 *
	 * The word count for this response MUST be 0x2A (42). WordCount in this 
	 * case is not used as the count of parameter words but is just a number.
	 */
	if (md_get_uint8(mdp, &wc) != 0 || 
		((wc != NTCREATEX_NORMAL_WDCNT) && (wc != NTCREATEX_EXTENDED_WDCNT))) {
		error = EIO;
		smb_log_info("%s: bad word count, syserr = %s", 
					 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
		goto done;
 	}
	md_get_uint8(mdp, NULL);        /* secondary cmd */
	md_get_uint8(mdp, NULL);        /* mbz */
	md_get_uint16le(mdp, NULL);     /* andxoffset */
	md_get_uint8(mdp, NULL);        /* oplock lvl granted */
	md_get_uint16le(mdp, &fid16);      /* FID */
	*fid = fid16;
	error = md_get_uint32le(mdp, NULL);     /* create_action */
	/* Only get the rest of the  parameter values if they want request them */
	if (outparms) {
		/* Should we convert the time, current we don't */
		md_get_uint64le(mdp, &outparms->createTime);
		md_get_uint64le(mdp, &outparms->accessTime);
		md_get_uint64le(mdp, &outparms->writeTime);
		md_get_uint64le(mdp, &outparms->changeTime);
		md_get_uint32le(mdp, &outparms->attributes);
		md_get_uint64le(mdp, &outparms->allocationSize);
		md_get_uint64le(mdp, &outparms->fileSize); 
		md_get_uint16le(mdp, NULL);     /* file type */
		md_get_uint16le(mdp, NULL);     /* device state */
		error = md_get_uint8(mdp, NULL);        /* directory (boolean) */
		/* Supports extended word count, so lets get them */
		if (wc == NTCREATEX_EXTENDED_WDCNT) {			
			md_get_mem(mdp, (caddr_t)outparms->volumeGID, sizeof(outparms->volumeGID), MB_MSYSTEM);
			md_get_uint64le(mdp, &outparms->fileInode);
			md_get_uint32le(mdp, &outparms->maxAccessRights);
			error = md_get_uint32le(mdp, &outparms->maxGuessAccessRights);
		} 
	}

done:
    smb_usr_rq_done(rqp);
    return error;
}

/* Perform a SMB_READ or SMB_READX.
 *
 * Return value is -errno if < 0, otherwise the received byte count.
 */
ssize_t smbio_read(void *smbctx, int fid, uint8_t *buf, size_t buflen)
{
    int bytes;

    bytes = smb_read(smbctx, (uint16_t)fid, 0 /* offset */,
	    (uint32_t)buflen, (char *)buf);
    if (bytes == -1) {
		return -errno;
    }

    return (ssize_t)bytes;
}

#define SMB1_TRANS2_MAXSIZE 0x0000ffff

/* 
 * Perform a smb transaction call
 *
 * Return zero if no error or the appropriate errno.
 */
int smbio_transact(void *smbctx, uint16_t *setup, int setupCnt, const char *name, 
					   const uint8_t *sndPData, size_t sndPDataLen, 
					   const uint8_t *sndData, size_t sndDataLen, 
					   uint8_t *rcvPData, size_t *rcvPDataLen, 
					   uint8_t *rcvdData, size_t *rcvDataLen)
{
	uint16_t rPDataLen = 0;
	uint16_t rDataLen = 0;
    uint32_t buffer_oflow = 0;
	int error;


	/*
	 * SMB trans uses 16 bit field, never let the calling process send more than 
	 * will fit in this field. SMB 2/3 will allow us to expand this size.
	 */
	if ((sndPDataLen > SMB1_TRANS2_MAXSIZE) || (sndDataLen > SMB1_TRANS2_MAXSIZE)) {
		return -EINVAL;	/* Can't send this much data with SMB */
	}	
	
	/*
	 * SMB trans2 uses 16 bit field, never let the calling process request more 
	 * than will fit in this field. SMB 2/3 will allow us to expand this size.
	 */
	if (rcvPDataLen) {
		if (*rcvPDataLen > SMB1_TRANS2_MAXSIZE) {
			rPDataLen = (uint16_t)SMB1_TRANS2_MAXSIZE;
		} else {
			rPDataLen = (uint16_t)*rcvPDataLen;
		}
	}
	/*
	 * SMB trans2 uses 16 bit field, never let the calling process request more 
	 * than will fit in this field. SMB 2/3 will allow us to expand this size.
	 */	
	if (rcvDataLen) {
		if (*rcvDataLen > SMB1_TRANS2_MAXSIZE) {
			rDataLen = (uint16_t)SMB1_TRANS2_MAXSIZE;
		} else {
			rDataLen = (uint16_t)*rcvDataLen;
		}
	}
	
    error = smb_usr_t2_request(smbctx, setupCnt, setup, name,
                                (uint16_t)sndPDataLen, sndPData, /* uint16_t tparamcnt, void *tparam */
								(uint16_t)sndDataLen, sndData,	/* uint16_t tdatacnt, void *tdata */
								&rPDataLen, rcvPData,			/* uint16_t *rparamcnt, void *rparam */
								&rDataLen, rcvdData,			/* uint16_t *rdatacnt, void *rdata */
								&buffer_oflow);

    if (error) {
		return error;
    }
	/* They request param data, return the amount received */
	if (rcvPDataLen) {
		*rcvPDataLen = rPDataLen;
	}
	/* They request data, return the amount received */
	if (rcvDataLen) {
		*rcvDataLen = rDataLen;
	}
	
	if (buffer_oflow) {
		return EOVERFLOW;
    }

    return 0;
}

/* From comsoc_libsmb.c: */
int smbio_open_pipe(void *smbctx, const char *pipe_path, int *fid)
{
	struct open_inparms inparms;
	int error;
    SMBFID smb2_fid = 0;
    
	memset(&inparms, 0,sizeof(inparms));
	inparms.rights = SMB2_READ_CONTROL | SMB2_FILE_WRITE_ATTRIBUTES | 
						SMB2_FILE_READ_ATTRIBUTES | SMB2_FILE_WRITE_EA | 
						SMB2_FILE_READ_EA | SMB2_FILE_APPEND_DATA | 
						SMB2_FILE_WRITE_DATA | SMB2_FILE_READ_DATA | SMB2_SYNCHRONIZE;
	/* Allocate */
	inparms.attrs = SMB_EFA_NORMAL;
	inparms.shareMode = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	inparms.disp = FILE_OPEN;
	inparms.createOptions = NTCREATEX_OPTIONS_NON_DIRECTORY_FILE;
	
	error = smb2io_ntcreatex(smbctx, pipe_path, NULL, &inparms, NULL, &smb2_fid);
	if (error == EINVAL) {
		/*
		 * Windows 95/98/Me return ERRSRV/ERRerror when we try to open the 
		 * pipe.  Map that to ECONNREFUSED so that it's treated as an attempt to 
		 * connect to a port on which nobody's listening, which is probably the
		 * best match.
		 */
		error = ECONNREFUSED;
	}
	
	if (error) {
		smb_log_info("%s, syserr = %s", ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
    }
    else {
        *fid = (int) smb2_fid;  /* cast to SMB 1 fid */
    }

	return -error;
}

int smbio_close_file(void *ctx, int fid)
{
	struct smb_usr_rq *rqp;
	mbchain_t mbp;
	int error;
	u_int16_t fid16 = fid;
	
	error = smb_usr_rq_init(ctx, SMB_COM_CLOSE, 0, &rqp);
	if (error)
		return error;
	mbp = smb_usr_rq_getrequest(rqp);
	smb_usr_rq_wstart(rqp);
	mb_put_uint16le(mbp, fid16);
	/*
	 * Never set the modify time on close. Just a really bad idea!
	 *
	 * Leach and SNIA docs say to send zero here.  X/Open says
	 * 0 and -1 both are leaving timestamp up to the server.
	 * Win9x treats zero as a real time-to-be-set!  We send -1,
	 * same as observed with smbclient.
	 */
	mb_put_uint32le(mbp, -1);
	smb_usr_rq_wend(rqp);
	smb_usr_rq_bstart(rqp);
	smb_usr_rq_bend(rqp);
	error = smb_usr_rq_simple(rqp);
	smb_usr_rq_done(rqp);
	return error;
}

int smbio_check_directory(struct smb_ctx *ctx, const void *path, 
						  uint32_t flags2, uint32_t *nt_error)
{
	struct smb_usr_rq	*rqp;
	mbchain_t		mbp;
	int				error;
	
	error = smb_usr_rq_init(ctx, SMB_COM_CHECK_DIRECTORY, flags2, &rqp);
	if (error)
		return error;
	mbp = smb_usr_rq_getrequest(rqp);
	smb_usr_rq_wstart(rqp);
	smb_usr_rq_wend(rqp);
	smb_usr_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	error = smb_usr_rq_put_dstring(ctx, mbp, path, strlen(path), 
                    				SMB_UTF_SFM_CONVERSIONS, NULL);
	if (error == 0) {
		smb_usr_rq_bend(rqp);
		error = smb_usr_rq_simple(rqp);
	}
	if (error && (error != ENOENT) && (error != ENOTDIR)) {
		smb_log_info("%s failed, syserr = %s", ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
	}
	if (nt_error) {
		*nt_error = smb_usr_rq_nt_error(rqp);
	}
	
	smb_usr_rq_done(rqp);
	return error;
}

