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
 * $Id: rap.c,v 1.6 2005/05/06 23:16:29 lindak Exp $
 *
 * This is very simple implementation of RAP protocol.
 */
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <sysexits.h>

#include <stdint.h>
#include <libkern/OSByteOrder.h>
#include <smbclient/smbclient.h>
#include <smbclient/smbclient_internal.h>
#include <smbclient/ntstatus.h>

#include "rap.h"

#define MAX_RAP_SHARE_BUFFER 0xffe0

static int
smb_rap_parserqparam(const char *s, char **next, int *rlen)
{
	char *np;
	int len;

	switch (*s++) {
	    case 'L':
	    case 'T':
	    case 'W':
		len = 2;
		break;
	    case 'D':
	    case 'O':
		len = 4;
		break;
	    case 'b':
	    case 'F':
		len = 1;
		break;
	    case 'r':
	    case 's':
		len = 0;
		break;
	    default:
		return EINVAL;
	}
	if (isdigit(*s)) {
		len *= (int)strtoul(s, &np, 10);
		s = np;
	}
	*rlen = len;
	*(const char**)next = s;
	return 0;
}

static int
smb_rap_parserpparam(const char *s, char **next, int *rlen)
{
	char *np;
	int len;

	switch (*s++) {
	    case 'e':
	    case 'h':
		len = 2;
		break;
	    case 'i':
		len = 4;
		break;
	    case 'g':
		len = 1;
		break;
	    default:
		return EINVAL;
	}
	if (isdigit(*s)) {
		len *= (int)strtoul(s, &np, 10);
		s = np;
	}
	*rlen = len;
	*(const char**)next = s;
	return 0;
}

static int
smb_rap_parserpdata(const char *s, char **next, int *rlen)
{
	char *np;
	int len;

	switch (*s++) {
	    case 'B':
		len = 1;
		break;
	    case 'W':
		len = 2;
		break;
	    case 'D':
	    case 'O':
	    case 'z':
		len = 4;
		break;
	    default:
		return EINVAL;
	}
	if (isdigit(*s)) {
		len *= (int)strtoul(s, &np, 10);
		s = np;
	}
	*rlen = len;
	*(const char**)next = s;
	return 0;
}

static int
smb_rap_rqparam_z(struct smb_rap *rap, const char *value)
{
	int len = (int)strlen(value) + 1;

	bcopy(value, rap->r_npbuf, len);
	rap->r_npbuf += len;
	rap->r_plen += len;
	return 0;
}

static int
smb_rap_rqparam(struct smb_rap *rap, char ptype, char plen, int32_t value)
{
	char *p = rap->r_npbuf;
	int len;

	switch (ptype) {
		case 'L':
		case 'W':
				setwle(p, 0, value);
				len = 2;
				break;
		case 'D':
				setdle(p, 0, value);
				len = 4;
				break;
		case 'b':
				memset(p, value, plen);
				len = plen;
				break;
		default:
				return EINVAL;
	}
	rap->r_npbuf += len;
	rap->r_plen += len;
	return 0;
}

static int
smb_rap_create(int fn, const char *param, const char *data,
	struct smb_rap **rapp)
{
	struct smb_rap *rap;
	char *p;
	int plen, len;

	rap = malloc(sizeof(*rap));
	if (rap == NULL)
		return ENOMEM;
	bzero(rap, sizeof(*rap));
	p = rap->r_sparam = rap->r_nparam = strdup(param);
	rap->r_sdata = rap->r_ndata = strdup(data);
	/*
	 * Calculate length of request parameter block
	 */
	len = 2 + (int)strlen(param) + 1 + (int)strlen(data) + 1;
	
	while (*p) {
		if (smb_rap_parserqparam(p, &p, &plen) != 0)
			break;
		len += plen;
	}
	rap->r_pbuf = rap->r_npbuf = malloc(len);
	smb_rap_rqparam(rap, 'W', 1, fn);
	smb_rap_rqparam_z(rap, rap->r_sparam);
	smb_rap_rqparam_z(rap, rap->r_sdata);
	*rapp = rap;
	return 0;
}

static void
smb_rap_done(struct smb_rap *rap)
{
	if (rap->r_sparam)
		free(rap->r_sparam);
	if (rap->r_sdata)
		free(rap->r_sdata);
	if (rap->r_pbuf)
		free(rap->r_pbuf);		
	if (rap->r_dbuf)
		free(rap->r_dbuf);		
	free(rap);
}

static int
smb_rap_setNparam(struct smb_rap *rap, int32_t value)
{
	char *p = rap->r_nparam;
	char ptype = *p;
	int error, plen;

	error = smb_rap_parserqparam(p, &p, &plen);
	if (error)
		return error;
	switch (ptype) {
	    case 'L':
			rap->r_rcvbuflen = value;
			/* FALLTHROUGH */
	    case 'W':
	    case 'D':
	    case 'b':
			error = smb_rap_rqparam(rap, ptype, plen, value);
		break;
	    default:
			return EINVAL;
	}
	rap->r_nparam = p;
	return error;
}

static int
smb_rap_setPparam(struct smb_rap *rap, void *value)
{
	char *p = rap->r_nparam;
	char ptype = *p;
	int error, plen;

	error = smb_rap_parserqparam(p, &p, &plen);
	if (error)
		return error;
	switch (ptype) {
	    case 'r':
		rap->r_rcvbuf = value;
		break;
	    default:
		return EINVAL;
	}
	rap->r_nparam = p;
	return 0;
}

static int
smb_rap_getNparam(struct smb_rap *rap, int32_t *value)
{
	char *p = rap->r_nparam;
	char ptype = *p;
	int error, plen;

	error = smb_rap_parserpparam(p, &p, &plen);
	if (error)
		return error;
	switch (ptype) {
	    case 'h':
		*value = OSSwapLittleToHostInt16(*(uint16_t*)rap->r_npbuf);
		break;
	    default:
		return EINVAL;
	}
	rap->r_npbuf += plen;
	rap->r_nparam = p;
	return 0;
}

static int
smb_rap_request(SMBHANDLE inConnection, struct smb_rap *rap)
{
	uint16_t *rp, conv;
	uint32_t *p32;
	char *dp, *p = rap->r_nparam;
	char ptype;
	int error = 0, entries, done, dlen, status;
	size_t rdatacnt, rparamcnt;

	rdatacnt = rap->r_rcvbuflen;
	rparamcnt = rap->r_plen;
	
	status = SMBTransactMailSlot(inConnection, "\\PIPE\\LANMAN", 
								 rap->r_pbuf, rap->r_plen, 
								 rap->r_pbuf, &rparamcnt, 
								 rap->r_rcvbuf, &rdatacnt);
	if (!NT_SUCCESS(status)) {
		/* Should never happen */
		return errno;
	}
	rp = (uint16_t*)rap->r_pbuf;
	rap->r_result = OSSwapLittleToHostInt16(*rp++);
	conv = OSSwapLittleToHostInt16(*rp++);
	rap->r_npbuf = (char*)rp;
	rap->r_entries = entries = 0;
	done = 0;
	while (!done && *p) {
		ptype = *p;
		switch (ptype) {
		    case 'e':
			rap->r_entries = entries = OSSwapLittleToHostInt16(*(uint16_t*)rap->r_npbuf);
			rap->r_npbuf += 2;
			p++;
			break;
		    default:
			done = 1;
		}
	}
	rap->r_nparam = p;
	/*
	 * In general, unpacking entries we may need to relocate
	 * entries for proper aligning. For now use them as is.
	 */
	dp = rap->r_rcvbuf;
	while (entries--) {
		p = rap->r_sdata;
		while (*p) {
			ptype = *p;
			error = smb_rap_parserpdata(p, &p, &dlen);
			if (error) {
				SMBLogInfo("reply data mismatch %s", ASL_LEVEL_ERR, p);
				return EBADRPC;
			}
			switch (ptype) {
			    case 'z':
				p32 = (uint32_t*)dp;
				*p32 = (OSSwapLittleToHostInt32(*p32) & 0xffff) - conv;
				break;
			}
			dp += dlen;
		}
	}
	return error;
}

/*
 * We could translate these better, but these are the old enumerate rap calls and 
 * we are using them less and less. The old code just passed up the rap errors, now
 * we convert it to real errno. 
 */
static int smb_rap_error(struct smb_rap *rap, int error)
{
	if (error)
		return error;
	if ((rap->r_result == 0) || (rap->r_result == SMB_ERROR_MORE_DATA))
		return 0;
	switch (rap->r_result) {
	case SMB_ERROR_ACCESS_DENIED:
	case SMB_ERROR_NETWORK_ACCESS_DENIED:
		error = EACCES;
		break;			
	default:
		SMBLogInfo("received an unknown rap enumerate share error %d", 
					 ASL_LEVEL_ERR, rap->r_result);
		error = EIO;
		break;
	}
	return error;
}

int
RapNetShareEnum(SMBHANDLE inConnection, int sLevel, void **rBuffer, uint32_t *rBufferSize, 
				uint32_t *entriesRead, uint32_t *totalEntriesRead)
{
	struct smb_rap *rap = NULL;
	int error;

	if (inConnection == NULL) {
		/* Should never happen */
		return EINVAL;
	}
	*rBufferSize = MAX_RAP_SHARE_BUFFER;	/* samba notes win2k bug for 65535 */
	*rBuffer = malloc(*rBufferSize);
	if (*rBuffer == NULL) {
		return errno;
	}
	
	error = smb_rap_create(0, "WrLeh", "B13BWz", &rap);
	if (error) {
		RapNetApiBufferFree(*rBuffer);
		*rBuffer = NULL;
		return error;
	}
	smb_rap_setNparam(rap, sLevel);		/* W - sLevel */
	smb_rap_setPparam(rap, *rBuffer);	/* r - pbBuffer */
	smb_rap_setNparam(rap, *rBufferSize);	/* L - cbBuffer */
	error = smb_rap_request(inConnection, rap);
	
	if (error == 0) {
		*entriesRead = rap->r_entries;
		if (totalEntriesRead) {
			int32_t lval = 0;
			error = smb_rap_getNparam(rap, &lval);
			*totalEntriesRead = lval;
		}
	}
	error = smb_rap_error(rap, error);
	smb_rap_done(rap);
	return error;
}

void RapNetApiBufferFree(void * bufptr)
{
	free(bufptr);
}
