/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Portions Copyright 2008 Apple, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include "libmlrpc/ndr.h"
#include "libmlrpc/mlrpc.h"

#define	MLRPC_IS_LAST_FRAG(F)	((F) & MLRPC_PFC_LAST_FRAG)
#define	MLRPC_DEFAULT_FRAGSZ	8192

static void mlrpc_c_init_hdr(struct mlrpc_client *, struct mlrpc_xaction *);
static int mlrpc_c_get_frags(struct mlrpc_client *, struct mlrpc_xaction *);

int
mlrpc_c_bind(struct mlrpc_client *mcli, const char *service_name,
    struct mlrpc_binding **ret_binding_p)
{
	struct mlrpc_service 	*msvc;
	struct mlrpc_binding 	*mbind;
	struct mlrpc_xaction	mxa;
	mlrpcconn_bind_hdr_t 	*bhdr;
	mlrpc_p_cont_elem_t 	*pce;
	mlrpcconn_bind_ack_hdr_t *bahdr;
	mlrpc_p_result_t 	*pre;
	int			rc;

	bzero(&mxa, sizeof (mxa));

	msvc = mlrpc_find_service_by_name(service_name);
	if (msvc == NULL)
		return (MLRPC_DRC_FAULT_API_SERVICE_INVALID);

	mxa.binding_list = mcli->binding_list;
	if ((mbind = mlrpc_new_binding(&mxa)) == NULL)
		return (MLRPC_DRC_FAULT_API_BIND_NO_SLOTS);

	mlrpc_c_init_hdr(mcli, &mxa);

	bhdr = &mxa.send_hdr.bind_hdr;
	bhdr->common_hdr.ptype = MLRPC_PTYPE_BIND;
	bhdr->common_hdr.frag_length = sizeof (*bhdr);
	bhdr->max_xmit_frag = MLRPC_DEFAULT_FRAGSZ;
	bhdr->max_recv_frag = MLRPC_DEFAULT_FRAGSZ;
	bhdr->assoc_group_id = 0;
	bhdr->p_context_elem.n_context_elem = 1;

	/* Assign presentation context id */
	pce = &bhdr->p_context_elem.p_cont_elem[0];
	pce->p_cont_id = mcli->next_p_cont_id++;
	pce->n_transfer_syn = 1;

	/* Set up UUIDs and versions from the service */
	pce->abstract_syntax.if_version = msvc->abstract_syntax_version;
	rc = mlrpc_str_to_uuid(msvc->abstract_syntax_uuid,
	    &pce->abstract_syntax.if_uuid);
	if (!rc)
		return (MLRPC_DRC_FAULT_API_SERVICE_INVALID);

	pce->transfer_syntaxes[0].if_version = msvc->transfer_syntax_version;
	rc = mlrpc_str_to_uuid(msvc->transfer_syntax_uuid,
	    &pce->transfer_syntaxes[0].if_uuid);
	if (!rc)
		return (MLRPC_DRC_FAULT_API_SERVICE_INVALID);

	/* Format and exchange the PDU */

	rc = (*mcli->xa_init)(mcli, &mxa, 0);
	if (MLRPC_DRC_IS_FAULT(rc))
		return (rc);

	rc = mlrpc_encode_pdu_hdr(&mxa);
	if (MLRPC_DRC_IS_FAULT(rc))
		goto fault_exit;

	rc = (*mcli->xa_exchange)(mcli, &mxa);
	if (MLRPC_DRC_IS_FAULT(rc))
		goto fault_exit;

	rc = mlrpc_decode_pdu_hdr(&mxa);
	if (MLRPC_DRC_IS_FAULT(rc))
		goto fault_exit;

	/* done with buffers */
	(*mcli->xa_destruct)(mcli, &mxa);

	bahdr = &mxa.recv_hdr.bind_ack_hdr;

	if (mxa.ptype != MLRPC_PTYPE_BIND_ACK)
		return (MLRPC_DRC_FAULT_RECEIVED_MALFORMED);

	if (bahdr->p_result_list.n_results != 1)
		return (MLRPC_DRC_FAULT_RECEIVED_MALFORMED);

	pre = &bahdr->p_result_list.p_results[0];

	if (pre->result != MLRPC_PCDR_ACCEPTANCE)
		return (MLRPC_DRC_FAULT_RECEIVED_MALFORMED);

	mbind->p_cont_id = pce->p_cont_id;
	mbind->which_side = MLRPC_BIND_SIDE_CLIENT;
	mbind->context = mcli;
	mbind->service = msvc;
	mbind->instance_specific = 0;

	*ret_binding_p = mbind;
	return (MLRPC_DRC_OK);

fault_exit:
	(*mcli->xa_destruct)(mcli, &mxa);
	return (rc);
}

int
mlrpc_c_call(struct mlrpc_binding *mbind, int opnum, void *params,
    mlrpc_heapref_t *heapref)
{
	struct mlrpc_client 	*mcli = mbind->context;
	struct mlrpc_xaction	mxa;
	mlrpcconn_request_hdr_t *reqhdr;
	mlrpcconn_common_header_t *rsphdr;
	int			rc;

#if 0
	/* The OpenSolaris code does this check, apparantly to verify that you
	 * have passed in a realistic opnum. However, for a RPC client, there's
	 * no real reason to provide a set of service stubs, so we allow them
	 * to be NULL.
	 *	- jpeach
	 */

	struct mlrpc_service	*msvc = mbind->service;
	if (mlrpc_find_stub_in_svc(msvc, opnum) == NULL)
		return (MLRPC_DRC_FAULT_API_OPNUM_INVALID);
#endif

	bzero(&mxa, sizeof (mxa));
	mxa.ptype = MLRPC_PTYPE_REQUEST;
	mxa.opnum = opnum;
	mxa.binding = mbind;

	mlrpc_c_init_hdr(mcli, &mxa);

	reqhdr = &mxa.send_hdr.request_hdr;
	reqhdr->common_hdr.ptype = MLRPC_PTYPE_REQUEST;
	reqhdr->p_cont_id = mbind->p_cont_id;
	reqhdr->opnum = opnum;

	rc = (*mcli->xa_init)(mcli, &mxa, heapref->heap);
	if (MLRPC_DRC_IS_FAULT(rc))
		return (rc);

	/* Reserve room for hdr */
	mxa.send_mlnds.pdu_scan_offset = sizeof (*reqhdr);

	rc = mlrpc_encode_call(&mxa, params);
	if (!MLRPC_DRC_IS_OK(rc))
		goto fault_exit;

	mxa.send_mlnds.pdu_scan_offset = 0;

	/*
	 * Now we have the PDU size, we need to set up the
	 * frag_length and calculate the alloc_hint.
	 */
	mxa.send_hdr.common_hdr.frag_length = mxa.send_mlnds.pdu_size;
	reqhdr->alloc_hint = mxa.send_mlnds.pdu_size -
	    sizeof (mlrpcconn_request_hdr_t);

	rc = mlrpc_encode_pdu_hdr(&mxa);
	if (MLRPC_DRC_IS_FAULT(rc))
		goto fault_exit;

	rc = (*mcli->xa_exchange)(mcli, &mxa);
	if (MLRPC_DRC_IS_FAULT(rc))
		goto fault_exit;

	rc = mlrpc_decode_pdu_hdr(&mxa);
	if (MLRPC_DRC_IS_FAULT(rc))
		goto fault_exit;

	if (mxa.ptype != MLRPC_PTYPE_RESPONSE) {
		rc = MLRPC_DRC_FAULT_RECEIVED_MALFORMED;
		goto fault_exit;
	}

	rsphdr = &mxa.recv_hdr.common_hdr;

	if (!MLRPC_IS_LAST_FRAG(rsphdr->pfc_flags)) {
		/*
		 * This is a multi-fragment response.
		 * Concatenate the  fragments so that we can
		 * continue afterward as if we had received
		 * the entire response as a single PDU.
		 */

		if (mlrpc_c_get_frags(mcli, &mxa) < 0) {
			rc = MLRPC_DRC_FAULT_RECEIVED_MALFORMED;
			goto fault_exit;
		}
	}

	rc = mlrpc_decode_return(&mxa, params);
	if (MLRPC_DRC_IS_FAULT(rc))
		goto fault_exit;

	rc = (*mcli->xa_preserve)(mcli, &mxa, heapref);
	if (MLRPC_DRC_IS_FAULT(rc))
		goto fault_exit;

	(*mcli->xa_destruct)(mcli, &mxa);
	return (MLRPC_DRC_OK);

fault_exit:
	(*mcli->xa_destruct)(mcli, &mxa);
	return (rc);
}

void
mlrpc_c_free_heap(struct mlrpc_binding *mbind, mlrpc_heapref_t *heapref)
{
	struct mlrpc_client *mcli = mbind->context;

	(*mcli->xa_release)(mcli, heapref);
}

static void
mlrpc_c_init_hdr(struct mlrpc_client *mcli, struct mlrpc_xaction *mxa)
{
	mlrpcconn_common_header_t *hdr = &mxa->send_hdr.common_hdr;

	hdr->rpc_vers = 5;
	hdr->rpc_vers_minor = 0;
	hdr->pfc_flags = MLRPC_PFC_FIRST_FRAG + MLRPC_PFC_LAST_FRAG;
	hdr->packed_drep.intg_char_rep = MLRPC_REPLAB_CHAR_ASCII;
#ifndef _BIG_ENDIAN
	hdr->packed_drep.intg_char_rep |= MLRPC_REPLAB_INTG_LITTLE_ENDIAN;
#endif
	/* hdr->frag_length */
	hdr->auth_length = 0;
	hdr->call_id = mcli->next_call_id++;
}

/*
 * mlrpc_c_remove_hdr
 *
 * Remove an RPC fragment header from the received data stream.
 *
 * Original RPC receive buffer:
 * |-      frag1                   -|    |-frag M(partial)-|
 * +==================+=============+----+=================+
 * | SmbTransact Rsp1 | SmbTransact |    | SmbReadX RspN   |
 * | (with RPC hdr)   | Rsp2        | .. | (with RPC hdr)  |
 * +-----+------------+-------------+    +-----+-----------+
 * | hdr | data       | data        | .. | hdr | data      |
 * +=====+============+=============+----+=====+===========+
 *                                       <------
 * ^                                     ^     ^
 * |                                     |     |
 * base_offset                          hdr   data
 *
 * |-------------------------------------|-----------------|
 *              offset                           len
 *
 * RPC receive buffer (after this call):
 * +==================+=============+----+===========+
 * | SmbTransact Rsp1 | SmbTransact |    | SmbReadX  |
 * | (with RPC hdr)   | Rsp2        | .. | RspN      |
 * +-----+------------+-------------+    +-----------+
 * | hdr | data       | data        | .. | data      |
 * +=====+============+=============+----+===========+
 */
static void
mlrpc_c_remove_hdr(struct mlndr_stream *mlnds, uint8_t * pdu_scan_offset,
	int *nbytes)
{
	uint8_t *hdr;
	uint8_t *data;

	hdr = pdu_scan_offset;
	data = hdr + MLRPC_RSP_HDR_SIZE;
	*nbytes -= MLRPC_RSP_HDR_SIZE;

	bcopy(data, hdr, *nbytes);
	mlnds->pdu_size -= MLRPC_RSP_HDR_SIZE;
}

/*
 * mlrpc_c_get_frags
 *
 * A DCE RPC message that is larger than a single fragment is transmitted
 * as a series of fragments: 5280 bytes for Windows NT and 4280 bytes for
 * both Windows 2000 and 2003.
 *
 * Collect RPC fragments and append them to the receive stream buffer.
 * Each received fragment has a header, which we need to remove as we
 * build the full RPC PDU.
 *
 * The xa_read() calls will translate to SmbReadX requests.  Note that
 * there is no correspondence between SmbReadX buffering and DCE RPC
 * fragment alignment.
 *
 * Return -1 on error. Otherwise, return the total data count of the
 * complete RPC response upon success.
 */
static int
mlrpc_c_get_frags(struct mlrpc_client *mcli, struct mlrpc_xaction *mxa)
{
	struct mlndr_stream *mlnds = &mxa->recv_mlnds;
	mlrpcconn_request_hdr_t hdr;
	uint8_t * pdu_scan_offset;
	bool last_frag = false;

	do {
		int frag_rcvd = 0;
		int frag_size = 0;

		do {
			int nbytes;

			/* The scan offset will be used to locate the
			 * frag header.
			 */
			pdu_scan_offset =
			    mlnds->pdu_base_addr + mlnds->pdu_size;

			if ((nbytes = (*mcli->xa_read)(mcli, mxa)) < 0)
				return (-1);

			/* xa_read puts new data at pdu_base_addr + pdu_size,
			 * and then updates pdu_size. So pdu_scan_offset now
			 * points into initialized data space.
			 */

			if (frag_rcvd == 0) {
				bzero(&hdr, sizeof(hdr));
				mlrpc_decode_frag_hdr(mlnds, &hdr,
					pdu_scan_offset);

				/* We can only deal with fragmentation on
				 * request PDUs. Other PDUs should never be
				 * fragmented anyway.
				 */
				if (hdr.common_hdr.ptype !=
					MLRPC_PTYPE_RESPONSE) {
					return -1;
				}

				last_frag =
				    MLRPC_IS_LAST_FRAG(hdr.common_hdr.pfc_flags);
				frag_size =
				    hdr.common_hdr.frag_length - MLRPC_RSP_HDR_SIZE;

				mlrpc_c_remove_hdr(mlnds, pdu_scan_offset,
					&nbytes);
			}

			frag_rcvd += nbytes;

			/* Make sure that we always have some free headroom to
			 * read another fragment into.
			 */
			if ((mlnds->pdu_max_size - mlnds->pdu_size) < 512) {
				MLNDS_GROW_PDU(mlnds,
					mlnds->pdu_max_size + getpagesize(), 0);
			}

		} while (frag_rcvd < frag_size);
	} while (!last_frag);

	return (0);
}
