/*
 * Copyright (c) 2006 - 2007 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 - 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "ntlm.h"
#include <heim-ipc.h>
#include <digest_asn1.h>

/*
 *
 */

struct ntlmdgst {
    heim_ipc ipc;
    char *domain;
    OM_uint32 flags;
    struct ntlm_buf key;
    krb5_data sessionkey;
};

static OM_uint32 dstg_destroy(OM_uint32 *, void *);

/*
 *
 */

static OM_uint32
dstg_alloc(OM_uint32 *minor, void **ctx)
{
    krb5_error_code ret;
    struct ntlmdgst *c;

    c = calloc(1, sizeof(*c));
    if (c == NULL) {
	*minor = ENOMEM;
	return GSS_S_FAILURE;
    }

    ret = heim_ipc_init_context("ANY:org.h5l.ntlm-service", &c->ipc);
    if (ret) {
	free(c);
	*minor = ENOMEM;
	return GSS_S_FAILURE;
    }

    *ctx = c;

    return GSS_S_COMPLETE;
}

static int
dstg_probe(OM_uint32 *minor_status, void *ctx, const char *realm, unsigned int *flags)
{
    struct ntlmdgst *c = ctx;
    heim_idata dreq, drep;
    NTLMInitReply ir;
    size_t size;
    NTLMInit ni;
    int ret;

    memset(&ni, 0, sizeof(ni));
    
    ni.flags = 0;

    ASN1_MALLOC_ENCODE(NTLMInit, dreq.data, dreq.length, &ni, &size, ret);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
    if (size != dreq.length)
	abort();
    
    ret = heim_ipc_call(c->ipc, &dreq, &drep, NULL);
    free(dreq.data);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
    
    ret = decode_NTLMInitReply(drep.data, drep.length, &ir, &size);
    free(drep.data);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    if (ir.ntlmNegFlags & NTLM_NEG_SIGN)
	*flags |= NSI_NO_SIGNING;

    free_NTLMInitReply(&ir);
    
    return 0;
}

/*
 *
 */

static OM_uint32
dstg_destroy(OM_uint32 *minor, void *ctx)
{
    struct ntlmdgst *c = ctx;
    krb5_data_free(&c->sessionkey);
    if (c->ipc)
	heim_ipc_free_context(c->ipc);
    memset(c, 0, sizeof(*c));
    free(c);

    return GSS_S_COMPLETE;
}

/*
 *
 */

static OM_uint32
dstg_ti(OM_uint32 *minor_status,
	ntlm_ctx ntlmctx,
	void *ctx,
	const char *hostname,
	const char *domain,
	uint32_t *negNtlmFlags)
{
    struct ntlmdgst *c = ctx;
    OM_uint32 maj_stat = GSS_S_FAILURE;
    heim_idata dreq, drep;
    NTLMInitReply ir;
    size_t size;
    NTLMInit ni;
    int ret;

    memset(&ni, 0, sizeof(ni));
    memset(&ir, 0, sizeof(ir));

    ni.flags = 0;
    if (hostname)
	ni.hostname = (char **)&hostname;
    if (domain)
	ni.domain = (char **)&domain;

    ASN1_MALLOC_ENCODE(NTLMInit, dreq.data, dreq.length, &ni, &size, ret);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
    if (size != dreq.length)
	abort();

    ret = heim_ipc_call(c->ipc, &dreq, &drep, NULL);
    free(dreq.data);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    ret = decode_NTLMInitReply(drep.data, drep.length, &ir, &size);
    free(drep.data);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    {
	struct ntlm_buf buf;

	buf.data = ir.targetinfo.data;
	buf.length = ir.targetinfo.length;

	ret = heim_ntlm_decode_targetinfo(&buf, 1, &ntlmctx->ti);
	if (ret) {
	    free_NTLMInitReply(&ir);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}
    }
    *negNtlmFlags = ir.ntlmNegFlags;

    maj_stat = GSS_S_COMPLETE;
    
    free_NTLMInitReply(&ir);

    return maj_stat;
}

/*
 *
 */

static OM_uint32
dstg_type3(OM_uint32 *minor_status,
	   ntlm_ctx ntlmctx,
	   void *ctx,
	   const struct ntlm_type3 *type3,
	   uint32_t *flags,
	   struct ntlm_buf *sessionkey,
	   ntlm_name *name, struct ntlm_buf *uuid,
	   struct ntlm_buf *pac)
{
    struct ntlmdgst *c = ctx;
    krb5_error_code ret;
    NTLMRequest2 req;
    NTLMReply rep;
    heim_idata dreq, drep;
    size_t size;

    *flags = 0;

    sessionkey->data = NULL;
    sessionkey->length = 0;
    *name = NULL;
    uuid->data = NULL;
    uuid->length = 0;
    pac->data = NULL;
    pac->length = 0;

    memset(&req, 0, sizeof(req));
    memset(&rep, 0, sizeof(rep));

    req.loginUserName = type3->username;
    req.loginDomainName = type3->targetname;
    req.workstation = type3->ws;
    req.ntlmFlags = type3->flags;
    req.lmchallenge.data = ntlmctx->challenge;
    req.lmchallenge.length = sizeof(ntlmctx->challenge);
    req.ntChallengeResponse.data = type3->ntlm.data;
    req.ntChallengeResponse.length = type3->ntlm.length;
    req.lmChallengeResponse.data = type3->lm.data;
    req.lmChallengeResponse.length = type3->lm.length;
    req.encryptedSessionKey.data = type3->sessionkey.data;
    req.encryptedSessionKey.length = type3->sessionkey.length;

    /* take care of type3->targetname ? */

    ASN1_MALLOC_ENCODE(NTLMRequest2, dreq.data, dreq.length, &req, &size, ret);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
    if (size != dreq.length)
	abort();
	
    ret = heim_ipc_call(c->ipc, &dreq, &drep, NULL);
    free(dreq.data);
    if (ret) {
	*minor_status = ret;
	return gss_mg_set_error_string(GSS_NTLM_MECHANISM, GSS_S_FAILURE, ret,
				       "ipc to digest-service failed");
    }	

    ret = decode_NTLMReply(drep.data, drep.length, &rep, &size);
    free(drep.data);
    if (ret) {
	*minor_status = ret;
	return gss_mg_set_error_string(GSS_NTLM_MECHANISM, GSS_S_FAILURE, ret,
				       "message from digest-service malformed");
    }	
    
    if (rep.success != TRUE) {
	ret = HNTLM_ERR_AUTH;
	gss_mg_set_error_string(GSS_NTLM_MECHANISM,
				GSS_S_FAILURE, ret,
				"ntlm: authentication failed");
	goto out;
    }

    *flags = rep.ntlmFlags;
    
    if (rep.flags & nTLM_anonymous)
	*flags |= NTLM_NEG_ANONYMOUS;

    /* handle session key */
    if (rep.sessionkey) {
	sessionkey->data = malloc(rep.sessionkey->length);
	memcpy(sessionkey->data, rep.sessionkey->data,
	       rep.sessionkey->length);
	sessionkey->length = rep.sessionkey->length;
    }
    
    *name = calloc(1, sizeof(**name));
    if (*name == NULL)
	goto out;
    (*name)->user = strdup(rep.user);
    (*name)->domain = strdup(rep.domain);
    if ((*name)->user == NULL || (*name)->domain == NULL)
	goto out;

    if (rep.uuid) {
	uuid->data = malloc(rep.uuid->length);
	memcpy(uuid->data, rep.uuid->data, rep.uuid->length);
	uuid->length = rep.uuid->length;
    }

    free_NTLMReply(&rep);

    return 0;

 out:
    free_NTLMReply(&rep);
    *minor_status = ret;
    return GSS_S_FAILURE;
}

/*
 *
 */

static void
dstg_free_buffer(struct ntlm_buf *sessionkey)
{
    if (sessionkey->data)
	free(sessionkey->data);
    sessionkey->data = NULL;
    sessionkey->length = 0;
}

/*
 *
 */

struct ntlm_server_interface ntlmsspi_dstg_digest = {
    "digest",
    dstg_alloc,
    dstg_destroy,
    dstg_probe,
    dstg_type3,
    dstg_free_buffer,
    dstg_ti
};
