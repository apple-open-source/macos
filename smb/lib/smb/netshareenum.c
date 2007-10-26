/*
 * Copyright (c) 2004 - 2007 Apple Inc. All rights reserved.
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
/*      @(#)ui.c      *
 *      (c) 2004   Apple Computer, Inc.  All Rights Reserved
 *
 *
 *      netshareenum.c -- Routines for getting a list of share information
 *			  from a server.
 *
 *      MODIFICATION HISTORY:
 *       27-Nov-2004     Guy Harris	New today
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <sys/mchain.h>
#include <netsmb/smb_lib.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_netshareenum.h>
#include <dce/exc_handling.h>
#include <netsmb/netbios.h>

#include "rap.h"
#include "srvsvc.h"
#include "charsets.h"

static int
rpc_netshareenum(struct smb_ctx *ctx, int *entriesp, int *totalp,
    struct share_info **entries_listp)
{
	char ctx_string[2+16+1];	/* enough for 64-bit pointer, in hex */
	unsigned_char_p_t binding;
	unsigned32 binding_status;
	rpc_binding_handle_t binding_h;
	int error, i, entries;
	char *addrstr, *srvnamestr;
	unsigned short *usrvnamestr;
	unsigned32 level;
	SHARE_ENUM_STRUCT share_info;
	SHARE_INFO_1_CONTAINER share_info_1_container;
	SHARE_INFO_1 *shares, *share;
	unsigned32 total_entries;
	unsigned32 status, free_status;
	struct share_info *entry_list, *elp;
	static EXCEPTION rpc_x_connect_rejected;
	static int exceptions_initialized;
	struct sockaddr_in	saddr;
	
	if (ctx->ct_ssn.ioc_server == NULL)	/* Should never happen, but just in case */ {
		smb_log_info("ctx->ct_ssn.ioc_server is NULL", EINVAL, ASL_LEVEL_ERR);
		return EINVAL;
	}
	
	sprintf(ctx_string, "%p", ctx);
	rpc_string_binding_compose(NULL, (unsigned_char_p_t)"ncacn_np", 
		(unsigned_char_p_t)ctx_string, (unsigned_char_p_t)"srvsvc", 
		NULL, (unsigned_char_p_t *)&binding, (unsigned32 *)&binding_status);
	if (binding_status != rpc_s_ok) {
		smb_log_info("rpc_string_binding_compose failed with %d", 0, ASL_LEVEL_ERR, binding_status);
		return EINVAL;
	}
	rpc_binding_from_string_binding(binding, &binding_h, &status);
	rpc_string_free(&binding, (unsigned32 *)&free_status);
	if (binding_status != rpc_s_ok) {
		smb_log_info("rpc_binding_from_string_binding failed with %d", 0, ASL_LEVEL_ERR, binding_status);
		return EINVAL;
	}
	level = 1;
	share_info.share_union.level = 1;
	share_info.share_union.tagged_union.share1 = &share_info_1_container;
	share_info_1_container.share_count = 0;
	share_info_1_container.shares = NULL;
	/*
	 * Convert the server IP address to a string, and send that as
	 * the "server name" - that's what Windows appears to do, and
	 * that avoids problems with NetBIOS names containing
	 * non-ASCII characters.
	 */
	
	saddr = GET_IP_ADDRESS_FROM_NB((ctx->ct_ssn.ioc_server));
	addrstr = inet_ntoa(saddr.sin_addr);
	srvnamestr = malloc(strlen(addrstr) + 3);
	if (srvnamestr == NULL) {
		status = errno;
		smb_log_info("can't allocate string for server address", status, ASL_LEVEL_ERR);
		rpc_binding_free(&binding_h, &free_status);
		return status;
	}
	strcpy(srvnamestr, "\\\\");
	strcat(srvnamestr, addrstr);
	usrvnamestr = convert_utf8_to_leunicode(srvnamestr);
	if (usrvnamestr == NULL) {
		smb_log_info("can't convert string for server address to Unicode", 0, ASL_LEVEL_ERR);
		rpc_binding_free(&binding_h, &free_status);
		free(srvnamestr);
		return EINVAL;
	}
	if (!exceptions_initialized) {
		EXCEPTION_INIT(rpc_x_connect_rejected);
		exc_set_status(&rpc_x_connect_rejected, rpc_s_connect_rejected);
		exceptions_initialized = 1;
	}
	TRY
		status = NetrShareEnum(binding_h, usrvnamestr, &level, &share_info, 4294967295U, &total_entries, NULL);
		if (status != 0)
			smb_log_info("error from NetrShareEnum call: status = 0x%08x", 0, ASL_LEVEL_ERR, status);
	CATCH (rpc_x_connect_rejected)
		/*
		 * This is what we get if we can't open the pipe.
		 * That's a normal occurrence when we're talking
		 * to a system that (presumably) doesn't support
		 * DCE RPC on the server side, such as Windows 95/98/Me,
		 * so we don't log an error.
		 */
		status = ENOTSUP;
	CATCH_ALL
		/*
		 * Looks like samba 3.0.14a doesn't support this dce rpc call. If we get an error
		 * always try the old RAP call.
		 */
		status = ENOTSUP;
	ENDTRY
	rpc_binding_free(&binding_h, &free_status);
	free(srvnamestr);
	free(usrvnamestr);
	if (status != 0)
		return ENOTSUP;

	/*
	 * XXX - if the IDL is correct, it's not clear whether the
	 * unmarshalling code will properly handle the case where
	 * a packet where "share_count" and the max count for the
	 * array of shares don't match; a valid DCE RPC implementation
	 * won't marshal something like that, but there's no guarantee
	 * that the server we're talking to has a valid implementation
	 * (which could be a *malicious* implementation!).
	 */
	entries = share_info.share_union.tagged_union.share1->share_count;
	shares = share_info.share_union.tagged_union.share1->shares;
	entry_list = calloc(entries, sizeof (struct share_info));
	if (entry_list == NULL) {
		error = errno;
		goto cleanup_and_return;
	}
	for (share = shares, elp = entry_list, i = 0; i < entries;
	    i++, share++) {
		elp->type = share->shi1_type;
		elp->netname = convert_unicode_to_utf8(share->shi1_share);
		if (elp->netname == NULL)
			goto fail;
		elp->remark = convert_unicode_to_utf8(share->shi1_remark);
		if (elp->remark == NULL)
			goto fail;
		elp++;
	}
	*entriesp = entries;
	*totalp = total_entries;
	*entries_listp = entry_list;
	error = 0;
	goto cleanup_and_return;

fail:
	error = errno;
	for (elp = entry_list, i = 0; i < entries; i++, elp++) {
		/*
		 * elp->netname is set before elp->remark, so if
		 * elp->netname is null, elp->remark is also null.
		 * If either of them is null, we haven't done anything
		 * to any entries after this one.
		 */
		if (elp->netname == NULL)
			break;
		free(elp->netname);
		if (elp->remark == NULL)
			break;
		free(elp->remark);
	}
	free(entry_list);

cleanup_and_return:
	for (share = shares, i = 0; i < entries; i++, share++) {
		free(share->shi1_share);
		free(share->shi1_remark);
	}
	free(shares);
	/*
	 * XXX - "share1" should be a unique pointer, but we haven't
	 * changed the marshalling code to support non-full pointers
	 * in unions, so we leave it as a full pointer.
	 *
	 * That means that this might, or might not, be changed from
	 * pointing to "share_info_1_container" to pointing to a
	 * mallocated structure, according to the DCE RPC 1.1 IDL spec;
	 * we free it only if it's changed.
	 */
	if (share_info.share_union.tagged_union.share1 != &share_info_1_container)
		free(share_info.share_union.tagged_union.share1);
	return error;		
}

static int
rap_netshareenum(struct smb_ctx *ctx, int *entriesp, int *totalp,
    struct share_info **entries_listp)
{
	int error, bufsize, i, entries, total, nreturned;
	struct smb_share_info_1 *rpbuf, *ep;
	struct share_info *entry_list, *elp;

	bufsize = 0xffe0;	/* samba notes win2k bug for 65535 */
	rpbuf = malloc(bufsize);
	if (rpbuf == NULL)
		return errno;

	error = smb_rap_NetShareEnum(ctx, 1, rpbuf, bufsize, &entries, &total);
	/* SMB_ERROR_MORE_DATA always get translated to no error */
	if (error) {
	    	free(rpbuf);
	    	return error;
	}
	entry_list = malloc(entries * sizeof (struct share_info));
	if (entry_list == NULL) {
		error = errno;
		free(rpbuf);
		return error;
	}
	for (ep = rpbuf, elp = entry_list, i = 0, nreturned = 0; i < entries;
	    i++, ep++) {
		elp->type = letohs(ep->shi1_type);
		ep->shi1_pad = '\0'; /* ensure null termination */
		elp->netname = convert_wincs_to_utf8(ep->shi1_netname);
		if (elp->netname == NULL)
			continue;	/* punt on this entry */
		if (ep->shi1_remark != 0) {
			/* We never use this, but if we ever decide to we should convert it to the correct code page? */
			elp->remark = (char *)rpbuf + ep->shi1_remark;
		} else
			elp->remark = NULL;
		elp++;
		nreturned++;
	}
	*entriesp = nreturned;
	*totalp = total;
	*entries_listp = entry_list;
	return 0;
}

/*
 * First we try the RPC-based NetrShareEnum, and, if that fails, we fall
 * back on the RAP-based NetShareEnum.
 */
int
smb_netshareenum(struct smb_ctx *ctx, int *entriesp, int *totalp,
    struct share_info **entry_listp)
{
	int error;

	/*
	 * Try getting a list of shares with the SRVSVC RPC service.
	 */
	error = rpc_netshareenum(ctx, entriesp, totalp, entry_listp);
	if (error == 0)
		return 0;

	/*
	 * OK, that didn't work - try RAP.
	 * XXX - do so only if it failed because we couldn't open
	 * the pipe?
	 */
	return rap_netshareenum(ctx, entriesp, totalp, entry_listp);
}
