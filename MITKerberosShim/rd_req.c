/*
 * Copyright (c) 2008-2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2008-2010 Apple Inc. All rights reserved.
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

#include "heim.h"
#include "mit-krb5.h"
#include <string.h>
#include <errno.h>

static krb5_flags
mshim_mit_ap_rep_flags(mit_krb5_flags ap_req_options)
{
    krb5_flags flags = 0;

    if (ap_req_options & MIT_KRB5_RECVAUTH_SKIP_VERSION)
	flags |= KRB5_RECVAUTH_IGNORE_VERSION;

    return flags;
}


static void
mshim_hticket2mticket(krb5_context context, krb5_ticket *h, mit_krb5_ticket *m)
{
    m->server = mshim_hprinc2mprinc(context, h->server);
    m->enc_part2 = calloc(1, sizeof(*m->enc_part2));
    
    m->enc_part2->flags = 0;
    if (h->ticket.flags.forwardable)
	m->enc_part2->flags |= MIT_TKT_FLG_FORWARDABLE;
    if (h->ticket.flags.forwarded)
	m->enc_part2->flags |= MIT_TKT_FLG_FORWARDED;
    if (h->ticket.flags.proxiable)
	m->enc_part2->flags |= MIT_TKT_FLG_PROXIABLE;
    if (h->ticket.flags.proxy)
	m->enc_part2->flags |= MIT_TKT_FLG_PROXY;
    if (h->ticket.flags.may_postdate)
	m->enc_part2->flags |= MIT_TKT_FLG_MAY_POSTDATE;
    if (h->ticket.flags.postdated)
	m->enc_part2->flags |= MIT_TKT_FLG_POSTDATED;
    if (h->ticket.flags.invalid)
	m->enc_part2->flags |= MIT_TKT_FLG_INVALID;
    if (h->ticket.flags.renewable)
	m->enc_part2->flags |= MIT_TKT_FLG_RENEWABLE;
    if (h->ticket.flags.initial)
	m->enc_part2->flags |= MIT_TKT_FLG_INITIAL;
    if (h->ticket.flags.pre_authent)
	m->enc_part2->flags |= MIT_TKT_FLG_PRE_AUTH;
    if (h->ticket.flags.hw_authent)
	m->enc_part2->flags |= MIT_TKT_FLG_HW_AUTH;
    if (h->ticket.flags.transited_policy_checked)
	m->enc_part2->flags |= MIT_TKT_FLG_TRANSIT_POLICY_CHECKED;
    if (h->ticket.flags.ok_as_delegate)
	m->enc_part2->flags |= MIT_TKT_FLG_OK_AS_DELEGATE;
    if (h->ticket.flags.anonymous)
	m->enc_part2->flags |= MIT_TKT_FLG_ANONYMOUS;

    m->enc_part2->session = mshim_malloc(sizeof(*m->enc_part2->session));
    mshim_hkeyblock2mkeyblock(&h->ticket.key, m->enc_part2->session);

    m->enc_part2->client = mshim_hprinc2mprinc(HC(context), h->client);
    m->enc_part2->transited.tr_type = 0;
    m->enc_part2->transited.tr_contents.data = NULL;
    m->enc_part2->transited.tr_contents.length = 0;

    m->enc_part2->times.authtime = h->ticket.authtime;
    m->enc_part2->times.starttime =
	h->ticket.starttime ? *h->ticket.starttime : 0 ;
    m->enc_part2->times.endtime = h->ticket.endtime;
    m->enc_part2->times.renew_till =
	h->ticket.renew_till ? *h->ticket.renew_till : 0 ;

    m->enc_part2->caddrs = NULL;
    m->enc_part2->authorization_data = NULL;
}



mit_krb5_error_code KRB5_CALLCONV
krb5_rd_req(mit_krb5_context context,
	    mit_krb5_auth_context *ac,
	    const mit_krb5_data *inbuf,
	    mit_krb5_const_principal server,
	    mit_krb5_keytab keytab,
	    mit_krb5_flags *ap_req_options,
	    mit_krb5_ticket **ticket)
{
    krb5_data idata, *d = NULL;
    struct comb_principal *p = (struct comb_principal *)server;
    krb5_ticket *t;
    krb5_flags flags;
    krb5_error_code ret;

    LOG_ENTRY();

    if (inbuf) {
	d = &idata;
	idata.data = inbuf->data;
	idata.length = inbuf->length;
    }

    ret = heim_krb5_rd_req(HC(context), (krb5_auth_context *)ac,
			   d, p ? p->heim : NULL, (krb5_keytab)keytab,
			   &flags, &t);
    if (ret == 0 && ticket != NULL) {
	mit_krb5_ticket *mt;
	mt = calloc(1, sizeof(*mt));
	mshim_hticket2mticket(HC(context), t, mt);
	heim_krb5_free_ticket(HC(context), t);
	*ticket = mt;
    }

    return ret;
}


	
mit_krb5_error_code KRB5_CALLCONV
krb5_recvauth(mit_krb5_context context,
	      mit_krb5_auth_context *ac,
	      mit_krb5_pointer fd,
	      char *appl_version,
	      mit_krb5_principal server,
	      mit_krb5_int32 flags,
	      mit_krb5_keytab keytab,
	      mit_krb5_ticket **ticket)
{
    struct comb_principal *s = (struct comb_principal *)server;
    krb5_ticket *hticket = NULL;
    mit_krb5_error_code ret;

    LOG_ENTRY();
    
    ret = heim_krb5_recvauth(HC(context),
			     (krb5_auth_context *)ac,
			     fd,
			     appl_version,
			     s ? s->heim : NULL,
			     mshim_mit_ap_rep_flags(flags),
			     (krb5_keytab)keytab,
			     &hticket);

    if (ticket && hticket) {
	*ticket = calloc(1, sizeof(**ticket));
	mshim_hticket2mticket(HC(context), hticket, *ticket);
    }
    if (hticket)
	heim_krb5_free_ticket(HC(context), hticket);

    return ret;
}

static krb5_boolean
match_appl_version(const void *ptr, const char *str)
{
    mit_krb5_data *version = (mit_krb5_data *)ptr;
    version->magic = MIT_KV5M_DATA;
    version->data = strdup(str);
    version->length = strlen(str);
    return 1;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_recvauth_version(mit_krb5_context context,
		      mit_krb5_auth_context *ac,
		      mit_krb5_pointer fd,
		      mit_krb5_principal server,
		      mit_krb5_int32 flags, 
		      mit_krb5_keytab keytab,
		      mit_krb5_ticket **ticket,
		      mit_krb5_data *version)
{
    struct comb_principal *s = (struct comb_principal *)server;
    krb5_ticket *hticket = NULL;
    mit_krb5_error_code ret;

    LOG_ENTRY();

    ret = heim_krb5_recvauth_match_version(HC(context),
					   (krb5_auth_context *)ac,
					   fd,
					   match_appl_version,
					   version,
					   s ? s->heim : NULL,
					   mshim_mit_ap_rep_flags(flags),
					   (krb5_keytab)keytab,
					   &hticket);

    if (ticket && hticket) {
	*ticket = calloc(1, sizeof(**ticket));
	mshim_hticket2mticket(HC(context), hticket, *ticket);
    }
    if (hticket)
	heim_krb5_free_ticket(HC(context), hticket);

    return ret;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_rd_priv(mit_krb5_context context,
	     mit_krb5_auth_context auth_context,
	     const mit_krb5_data *inbuf,
	     mit_krb5_data *outbuf,
	     mit_krb5_replay_data *replay)
{
    krb5_replay_data outdata;
    mit_krb5_error_code ret;

    LOG_ENTRY();
    krb5_data in, out;

    memset(outbuf, 0, sizeof(*outbuf));
    memset(&outdata, 0, sizeof(outdata));

    in.data = inbuf->data;
    in.length = inbuf->length;

    ret = heim_krb5_rd_priv(HC(context),
			    (krb5_auth_context)auth_context,
			    &in,
			    &out,
			    &outdata);
    if (ret)
	return ret;

    mshim_hdata2mdata(&out, outbuf);
    heim_krb5_data_free(&out);

    if (replay) {
	memset(replay, 0, sizeof(*replay));
	mshim_hreplay2mreplay(&outdata, replay);
    }

    return 0;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_rd_safe(mit_krb5_context context,
	     mit_krb5_auth_context auth_context,
	     const mit_krb5_data *inbuf,
	     mit_krb5_data *outbuf,
	     mit_krb5_replay_data *replay)
{
    krb5_replay_data outdata;
    mit_krb5_error_code ret;

    LOG_ENTRY();
    krb5_data in, out;

    memset(outbuf, 0, sizeof(*outbuf));
    memset(&outdata, 0, sizeof(outdata));

    in.data = inbuf->data;
    in.length = inbuf->length;

    ret = heim_krb5_rd_safe(HC(context),
			    (krb5_auth_context)auth_context,
			    &in,
			    &out,
			    &outdata);
    if (ret)
	return ret;

    mshim_hdata2mdata(&out, outbuf);
    heim_krb5_data_free(&out);

    if (replay) {
	memset(replay, 0, sizeof(*replay));
	mshim_hreplay2mreplay(&outdata, replay);
    }

    return 0;
}
