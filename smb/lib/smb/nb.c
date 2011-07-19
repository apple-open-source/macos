/*
 * Copyright (c) 2000, 2001 Boris Popov
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
 * $Id: nb.c,v 1.2 2005/10/07 03:51:09 lindak Exp $
 */

#include <netsmb/netbios.h>
#include <netsmb/smb_lib.h>
#include <netsmb/nb_lib.h>


#define NBNS_FMT_ERR	0x01	/* Request was invalidly formatted */
#define NBNS_SRV_ERR	0x02	/* Problem with NBNS, connot process name */
#define NBNS_NME_ERR	0x03	/* No such name */
#define NBNS_IMP_ERR	0x04	/* Unsupport request */
#define NBNS_RFS_ERR	0x05	/* For policy reasons server will not register this name fron this host */
#define NBNS_ACT_ERR	0x06	/* Name is owned by another host */
#define NBNS_CFT_ERR	0x07	/* Name conflict error  */

static int nb_resolve_wins(CFArrayRef WINSAddresses, CFMutableArrayRef *addressArray)
{
	CFIndex ii, count = CFArrayGetCount(WINSAddresses);
	int  error = ENOMEM;
	
	for (ii = 0; ii < count; ii++) {
		CFStringRef winsString = CFArrayGetValueAtIndex(WINSAddresses, ii);
		char winsName[SMB_MAX_DNS_SRVNAMELEN+1];
		
		if (winsString == NULL) {
			continue;		
		}
		
		CFStringGetCString(winsString, winsName,  sizeof(winsName), kCFStringEncodingUTF8);
		error = resolvehost(winsName, addressArray, NULL, NBNS_UDP_PORT_137, TRUE, FALSE);
		if (error == 0) {
			break;
		}
		smb_log_info("can't resolve WINS[%d] %s, syserr = %s", ASL_LEVEL_DEBUG, 
					 (int)ii, winsName, strerror(error));
	}
	return error;
}

/*
 * Used for resolving NetBIOS names
 */
int nb_ctx_resolve(struct nb_ctx *ctx, CFArrayRef WINSAddresses)
{
	int error = 0;

	if (WINSAddresses == NULL) {
		ctx->nb_ns.sin_addr.s_addr = htonl(INADDR_BROADCAST);
		ctx->nb_ns.sin_port = htons(NBNS_UDP_PORT_137);
		ctx->nb_ns.sin_family = AF_INET;
		ctx->nb_ns.sin_len = sizeof(ctx->nb_ns);
	} else {
		CFMutableArrayRef addressArray = NULL;
		CFMutableDataRef addressData = NULL;
		struct connectAddress *conn = NULL;
		
		error = nb_resolve_wins(WINSAddresses, &addressArray);
		if (error) {
			return error;
		}
		/* 
		 * At this point we have at least one IPv4 sockaddr in outAddressArray 
		 * that we can use. May want to change this in the future to try all
		 * address.
		 */
		addressData = (CFMutableDataRef)CFArrayGetValueAtIndex(addressArray, 0);
		if (addressData)
			conn = (struct connectAddress *)((void *)CFDataGetMutableBytePtr(addressData));
			
		if (conn)
			memcpy(&ctx->nb_ns, &conn->addr, conn->addr.sa_len);
		else
			error = ENOMEM;
		CFRelease(addressArray);
	}
	return error;
}

/*
 * Convert NetBIOS name lookup errors to UNIX errors
 */
int nb_error_to_errno(int error)
{
	switch (error) {
	case NBNS_FMT_ERR:
		error = EINVAL;
		break;
	case NBNS_SRV_ERR: 
		error = EBUSY;
		break;
	case NBNS_NME_ERR: 
		error = ENOENT;
		break;
	case NBNS_IMP_ERR: 
		error = ENOTSUP;
		break;
	case NBNS_RFS_ERR: 
		error = EACCES;
		break;
	case NBNS_ACT_ERR: 
		error = EADDRINUSE;
		break;
	case NBNS_CFT_ERR: 
		error = EADDRINUSE;
		break;
	default:
		error = ETIMEDOUT;
		break;
	};
	return error;
}
