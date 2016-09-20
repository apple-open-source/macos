/*
 * Copyright (c) 2016 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_krb5.h>
#include "lucid.h"
#include "gssd.h"

int
make_lucid_stream(gss_krb5_lucid_context_v1_t *lctx, size_t *len, void **data)
{
	lucid_context lucid;
	uint8_t *bufptr;
	uint32_t size;
	XDR xdrs;

	*len = 0;
	*data = NULL;
	switch (lctx->version) {
	case 1:
		break;
	default:
		return (FALSE);
	}
	lucid.vers = lctx->version;
	lucid.initiate = lctx->initiate;
	lucid.end_time = lctx->endtime;
	lucid.send_seq = lctx->send_seq;
	lucid.recv_seq = lctx->recv_seq;
	lucid.key_data.proto = lctx->protocol;
	switch (lctx->protocol) {
	case 0:
		lucid.key_data.lucid_protocol_u.data_1964.sign_alg = lctx->rfc1964_kd.sign_alg;
		lucid.key_data.lucid_protocol_u.data_1964.seal_alg = lctx->rfc1964_kd.seal_alg;
		lucid.ctx_key.etype = lctx->rfc1964_kd.ctx_key.type;
		lucid.ctx_key.key.key_len = lctx->rfc1964_kd.ctx_key.length;
		lucid.ctx_key.key.key_val = lctx->rfc1964_kd.ctx_key.data;
		break;
	case 1:
		lucid.key_data.lucid_protocol_u.data_4121.acceptor_subkey = lctx->cfx_kd.have_acceptor_subkey;
		if (lctx->cfx_kd.have_acceptor_subkey) {
			lucid.ctx_key.etype = lctx->cfx_kd.acceptor_subkey.type;
			lucid.ctx_key.key.key_len = lctx->cfx_kd.acceptor_subkey.length;
			lucid.ctx_key.key.key_val = lctx->cfx_kd.acceptor_subkey.data;
		} else {
			lucid.ctx_key.etype = lctx->cfx_kd.ctx_key.type;
			lucid.ctx_key.key.key_len = lctx->cfx_kd.ctx_key.length;
			lucid.ctx_key.key.key_val = lctx->cfx_kd.ctx_key.data;
		}
		break;
	default:
		return (FALSE);
	}
	size = xdr_sizeof((xdrproc_t)xdr_lucid_context, &lucid);
	if (size == 0)
		return (FALSE);
	bufptr = malloc(size);
	if (bufptr == NULL)
		return (FALSE);
	xdrmem_create(&xdrs, bufptr, size, XDR_ENCODE);
	if (xdr_lucid_context(&xdrs, &lucid)) {
		*len = size;
		*data = bufptr;
		return (TRUE);
	}
	return (FALSE);
}
