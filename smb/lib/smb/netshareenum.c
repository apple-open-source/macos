/*
 * Copyright (c) 2004 - 2008 Apple Inc. All rights reserved.
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
#include <netsmb/netbios.h>
#include <netsmb/smbio.h>
#include <ndl/ndrtypes.ndl>
#include <ndl/srvsvc.ndl>
#include <libmlrpc/mlrpc.h>
#include <libmlrpc/mlsvc_util.h>

#include "rap.h"
#include "charsets.h"

static mlrpc_service_t srvsvc_service = {
        (char *)"srvsvc",                       /* name */
        (char *)"Server services",              /* desc */
        (char *)"\\srvsvc",                     /* endpoint */
        (char *)"\\PIPE\\ntsvcs",                    /* sec_addr_port */
        (char *)"4b324fc8-1670-01d3-12785a47bf6ee188", 3,       /* abstract */
        (char *)"8a885d04-1ceb-11c9-9fe808002b104860", 2,       /* transfer */
        0,                              /* no bind_instance_size */
        0,                              /* no bind_req() */
        0,                              /* no unbind_and_close() */
        0,                              /* use generic_call_stub() */
        &TYPEINFO(srvsvc_interface),    /* interface ti */
        NULL               /* stub_table */
};


static char * server_name(struct smb_ctx * smbctx)
{
	char * srvname = NULL;

	if (smbctx->ct_saddr->sa_family == AF_INET) {
		asprintf(&srvname, "\\\\%s", smbctx->ct_ssn.ioc_srvname);
	} else {
		/* XXX Not sure what to do here, for now use the URL server name */
		asprintf(&srvname, "\\\\%s", smbctx->serverName);		
	}
	return srvname;
}

static int
mlrpc_netshareenum(struct smb_ctx * smbctx, int *entriesp, int *totalp,
    struct share_info **entries_listp)
{
	mlrpc_heapref_t heap;
	mlsvc_handle_t	handle;
	struct mlrpc_smb_client smbcli;

	struct mslm_NetShareEnum arg;
	struct mslm_infonres infonres;

	int error;
	int fid;
    
	memset(&heap, 0, sizeof(heap));
	memset(&handle, 0, sizeof(handle));
	memset(&arg, 0, sizeof(arg));
	memset(&infonres, 0, sizeof(infonres));

	mlrpc_register_service(&srvsvc_service);
	mlsvc_rpc_init(&heap);

	/* OK, we have an IPC$ tree connected, let's try to open the pipe. */
	error = smbio_open_pipe(smbctx, "srvsvc", &fid);
	if (error) {
		/* 
		 * Since we haven't called mlsvc_rpc_call yet use mlrpc_heap_destroy to
		 * free the heap. Check out the comments in mlsvc_rpc_init for more
		 * details.
		 */
		mlrpc_heap_destroy(heap.heap);
	    return ENOENT;
	}

	smbcli.smb_context = smbctx;
	smbcli.smb_transact = smbio_transact;
	smbcli.smb_readx = smbio_read;

	error = mlsvc_rpc_bind(&handle, &smbcli, fid, "srvsvc");
	if (error != 0) {
		/* 
		 * Since we haven't called mlsvc_rpc_call yet use mlrpc_heap_destroy to
		 * free the heap. Check out the comments in mlsvc_rpc_init for more
		 * details. Note we only call mlsvc_rpc_free after an RPC call returns.
		 */
		mlrpc_heap_destroy(heap.heap);
	    return EACCES;
	}

	arg.servername = (uint8_t *)server_name(smbctx);
	arg.level = 1;
	arg.prefmaxlen = UINT32_MAX;
	arg.result.level = 1;
	arg.result.bufptr.p = &infonres;

	error = mlsvc_rpc_call(handle.context, SRVSVC_OPNUM_NetShareEnum,
			&arg, &heap);

	free(arg.servername);

	if (error != 0) {
	    /* RPC error */
	    mlsvc_rpc_free(handle.context, &heap);
	    return EIO;
	}

	if (arg.status != 0) {
	    /* API call failed. We should be mapping NTSTATUS to errno .. */
	    mlsvc_rpc_free(handle.context, &heap);
	    return EBADRPC;
	}

	*entriesp = arg.result.bufptr.bufptr1->entriesread;
	*totalp = arg.totalentries;
	if (*entriesp > *totalp) {
		/* Should never happen but protect ourself just in case */
		smb_log_info("%s: total entries is less then number of entries!", error, 
					 ASL_LEVEL_DEBUG, __FUNCTION__);
		*entriesp = *totalp;
	}
	*entries_listp = calloc(*entriesp, sizeof (struct share_info));

	for (int i = 0; i < *entriesp; ++i) {
		struct mslm_SHARE_INFO_1 * info1;

		info1 = &(arg.result.bufptr.bufptr1->entries[i]);
		if (info1->shi1_netname == NULL) {
			/* Something wrong stop here */
			smb_log_info("%s: entry %i is empty?", error, ASL_LEVEL_DEBUG, 
						 __FUNCTION__, i);
			*entriesp = *totalp = i;
			break;
		}
		(*entries_listp)[i].type = info1->shi1_type;
		(*entries_listp)[i].netname = strdup((const char *)info1->shi1_netname);
		if (info1->shi1_remark) {
			(*entries_listp)[i].remark = strdup((const char *)info1->shi1_remark);
		}
	}

	mlsvc_rpc_free(handle.context, &heap);
	return 0;

}

static int
rap_netshareenum(struct smb_ctx *ctx, int *entriesp, int *totalp,
    struct share_info **entries_listp)
{
	int error, i, entries, total, nreturned;
	struct smb_share_info_1 *rpbuf, *ep;
	struct share_info *entry_list, *elp;
	u_int32_t  bufsize = 0xffe0;	/* samba notes win2k bug for 65535 */
	
	rpbuf = calloc(1, bufsize+1);
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
		if ((ep->shi1_remark) && (ep->shi1_remark < bufsize)) {
			char *comments = (char *)rpbuf + ep->shi1_remark;
			elp->remark = convert_wincs_to_utf8(comments);
		} else
			elp->remark = NULL;
		elp++;
		nreturned++;
	}
	*entriesp = nreturned;
	*totalp = total;
	*entries_listp = entry_list;
	free(rpbuf);
	return 0;
}

/*
 * Done with the share info that was returned not free all the
 * entries and the structure.
 */
void smb_freeshareinfo(struct share_info *shares, int entries)
{
	int ii;
	struct share_info *ep = NULL;
	
	if (!shares)
		return; /* Nothing to do here */
	
	for (ep = shares, ii = 0; ii < entries; ii++, ep++) {
		if (ep->netname)
			free(ep->netname);
		if (ep->remark)
			free(ep->remark);
	}
	free(shares);
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

	/* Only use RPC if the server supports DCE/RPC */
	if (ctx->ct_vc_caps & SMB_CAP_RPC_REMOTE_APIS) {
		/* Try getting a list of shares with the SRVSVC RPC service. */
		error = mlrpc_netshareenum(ctx, entriesp, totalp, entry_listp);
		if (error == 0)
			return 0;
	}

	/*
	 * OK, that didn't work - either they don't support RPC or we
	 * got an errorm in either case try RAP.
	 */
	return rap_netshareenum(ctx, entriesp, totalp, entry_listp);
}
