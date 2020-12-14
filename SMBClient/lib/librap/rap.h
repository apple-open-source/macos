/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2004 - 2010 Apple Inc. All rights reserved.
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
 * $Id: rap.h,v 1.2 2004/12/13 00:25:23 lindak Exp $
 */
#ifndef _RAP_H_
#define _RAP_H_

/*
 * RAP error codes
 */
#define	SMB_ERROR_ACCESS_DENIED			5
#define	SMB_ERROR_NETWORK_ACCESS_DENIED	65
#define SMB_ERROR_MORE_DATA				234

#define get16(buf,ofs)		(*((uint16_t*)(&((uint8_t*)(buf))[ofs])))
#define get32(buf,ofs)		(*((uint32_t*)(&((uint8_t*)(buf))[ofs])))

#define getwle(buf,ofs)	OSSwapHostToLittleInt16(get16(buf,ofs)
#define getwbe(buf,ofs)	OSSwapHostToBigInt16(get16(buf,ofs)
#define getdle(buf,ofs)	OSSwapHostToLittleInt32(get32(buf,ofs)
#define getdbe(buf,ofs) OSSwapHostToBigInt32(get32(buf,ofs)

#define setwle(buf,ofs,val)	get16(buf,ofs)=OSSwapHostToLittleInt16(val)
#define setwbe(buf,ofs,val) get16(buf,ofs)=OSSwapHostToBigInt16(val)
#define setdle(buf,ofs,val) get32(buf,ofs)=OSSwapHostToLittleInt32(val)
#define setdbe(buf,ofs,val) get32(buf,ofs)=OSSwapHostToBigInt32(val)

struct smb_rap {
	char *		r_sparam;
	char *		r_nparam;
	char *		r_sdata;
	char *		r_ndata;
	char *		r_pbuf;		/* rq parameters */
	int		r_plen;		/* rq param len */
	char *		r_npbuf;
	char *		r_dbuf;		/* rq data */
	int		r_dlen;		/* rq data len */
	char *		r_ndbuf;
	uint32_t	r_result;
	char *		r_rcvbuf;
	int		r_rcvbuflen;
	int		r_entries;
};

struct smb_share_info_1 {
	char		shi1_netname[13];
	char		shi1_pad;
	uint16_t	shi1_type;
	uint32_t	shi1_remark;		/* char * */
};

__BEGIN_DECLS

int
RapNetShareEnum(SMBHANDLE inConnection, int sLevel, void **rBuffer, uint32_t *rBufferSize, 
				uint32_t *entriesRead, uint32_t *totalEntriesRead);
void RapNetApiBufferFree(void * bufptr);

__END_DECLS

#endif /* _RAP_H_ */
