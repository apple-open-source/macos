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
#include <string.h>
#include <errno.h>

void KRB5_CALLCONV
krb5_get_init_creds_opt_init(mit_krb5_get_init_creds_opt *opt)
{
    LOG_ENTRY();
    memset(opt, 0, sizeof(*opt));
}

mit_krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_opt_set_process_last_req(mit_krb5_context context,
					     mit_krb5_get_init_creds_opt *opt,
					     mit_krb5_gic_process_last_req req,
					     void *ctx)
{
    LOG_ENTRY();
    /* XXX need to implement this, its will be require for Lion
       some time later */
    return 0;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_opt_alloc(mit_krb5_context context,
			      mit_krb5_get_init_creds_opt **opt)
{
    mit_krb5_get_init_creds_opt *c;
    LOG_ENTRY();
    c = calloc(1, sizeof(*c));
    if (c == NULL)
	return ENOMEM;
    *opt = c;
    return 0;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_free(mit_krb5_context context,
			     mit_krb5_get_init_creds_opt *opt)
{
    LOG_ENTRY();
    free(opt);
}


void KRB5_CALLCONV
krb5_get_init_creds_opt_set_tkt_life(mit_krb5_get_init_creds_opt *opt,
				     mit_krb5_deltat tkt_life)
{
    LOG_ENTRY();
    opt->flags |= MIT_KRB5_GET_INIT_CREDS_OPT_TKT_LIFE;
    opt->tkt_life = tkt_life;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_renew_life(mit_krb5_get_init_creds_opt *opt,
				       mit_krb5_deltat renew_life)
{
    LOG_ENTRY();
    opt->flags |= MIT_KRB5_GET_INIT_CREDS_OPT_RENEW_LIFE;
    opt->renew_life = renew_life;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_forwardable(mit_krb5_get_init_creds_opt *opt,
					int forwardable)
{
    LOG_ENTRY();
    opt->flags |= MIT_KRB5_GET_INIT_CREDS_OPT_FORWARDABLE;
    opt->forwardable = forwardable;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_proxiable(mit_krb5_get_init_creds_opt *opt,
				      int proxiable)
{
    LOG_ENTRY();
    opt->flags |= MIT_KRB5_GET_INIT_CREDS_OPT_PROXIABLE;
    opt->proxiable = proxiable;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_canonicalize(mit_krb5_get_init_creds_opt *opt,
					 int canonicalize)
{
    LOG_ENTRY();
    opt->flags |= MIT_KRB5_GET_INIT_CREDS_OPT_CANONICALIZE;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_etype_list(mit_krb5_get_init_creds_opt *opt,
				       mit_krb5_enctype *etype_list,
				       int etype_list_length)
{
    LOG_ENTRY();
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_ETYPE_LIST;
    opt->etype_list = etype_list;
    opt->etype_list_length = etype_list_length;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_address_list(mit_krb5_get_init_creds_opt *opt,
					 mit_krb5_address **addresses)
{
    LOG_UNIMPLEMENTED();
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_preauth_list(mit_krb5_get_init_creds_opt *opt,
					 mit_krb5_preauthtype *preauth_list,
					 int preauth_list_length)
{
    LOG_ENTRY();
}

static krb5_get_init_creds_opt *
mshim_gic_opt(krb5_context context, mit_krb5_get_init_creds_opt *mopt)
{
    krb5_get_init_creds_opt *opt = NULL;
    if (mopt) {
	heim_krb5_get_init_creds_opt_alloc(context, &opt);
	if (mopt->flags & MIT_KRB5_GET_INIT_CREDS_OPT_FORWARDABLE)
	    heim_krb5_get_init_creds_opt_set_forwardable(opt, mopt->forwardable);
	if (mopt->flags & MIT_KRB5_GET_INIT_CREDS_OPT_PROXIABLE)
	    heim_krb5_get_init_creds_opt_set_proxiable(opt, mopt->proxiable);
	if (mopt->flags & MIT_KRB5_GET_INIT_CREDS_OPT_CANONICALIZE)
	    heim_krb5_get_init_creds_opt_set_canonicalize(HC(context), opt, TRUE);
	if (mopt->flags & MIT_KRB5_GET_INIT_CREDS_OPT_TKT_LIFE)
	    heim_krb5_get_init_creds_opt_set_tkt_life(opt, mopt->tkt_life);
	if (mopt->flags & MIT_KRB5_GET_INIT_CREDS_OPT_RENEW_LIFE)
	    heim_krb5_get_init_creds_opt_set_renew_life(opt, mopt->renew_life);
	/* XXX */
    }
    return opt;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_password(mit_krb5_context context,
			     mit_krb5_creds *creds,
			     mit_krb5_principal client,
			     char *password,
			     mit_krb5_prompter_fct prompter,
			     void *data,
			     mit_krb5_deltat start_time,
			     char *in_tkt_service,
			     mit_krb5_get_init_creds_opt *mopt)
{
    struct comb_principal *p = (struct comb_principal *)client;
    krb5_get_init_creds_opt *opt = NULL;
    krb5_error_code ret;
    krb5_creds hcreds;
    krb5_prompter_fct pfct = NULL;

    LOG_ENTRY();

    opt = mshim_gic_opt(HC(context), mopt);

    memset(creds, 0, sizeof(*creds));
    memset(&hcreds, 0, sizeof(hcreds));

    if (prompter == krb5_prompter_posix)
	pfct = heim_krb5_prompter_posix;
    else if (prompter == NULL)
	pfct = NULL;
    else {
	if (opt)
	    heim_krb5_get_init_creds_opt_free(HC(context), opt);
	return EINVAL;
    }

    ret = heim_krb5_get_init_creds_password(HC(context), &hcreds, p->heim, password, 
					    pfct, NULL, start_time, in_tkt_service, opt);
    if (opt)
	heim_krb5_get_init_creds_opt_free(HC(context), opt);
    if (ret)
	return ret;

    mshim_hcred2mcred(HC(context), &hcreds, creds);

    heim_krb5_free_cred_contents(HC(context), &hcreds);

    return ret;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_keytab(mit_krb5_context context,
			   mit_krb5_creds *creds,
			   mit_krb5_principal client,
			   mit_krb5_keytab arg_keytab,
			   mit_krb5_deltat start_time,
			   char *in_tkt_service,
			   mit_krb5_get_init_creds_opt *mopt)
{
    struct comb_principal *p = (struct comb_principal *)client;
    krb5_get_init_creds_opt *opt = NULL;
    krb5_error_code ret;
    krb5_creds hcreds;

    LOG_ENTRY();

    opt = mshim_gic_opt(HC(context), mopt);

    memset(creds, 0, sizeof(*creds));
    memset(&hcreds, 0, sizeof(hcreds));

    ret = heim_krb5_get_init_creds_keytab(HC(context), &hcreds, p->heim,
					  (krb5_keytab)arg_keytab,
					  start_time, in_tkt_service, opt);
    if (opt)
	heim_krb5_get_init_creds_opt_free(HC(context), opt);
    if (ret)
	return ret;

    mshim_hcred2mcred(HC(context), &hcreds, creds);

    heim_krb5_free_cred_contents(HC(context), &hcreds);

    return ret;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_get_in_tkt_with_password(mit_krb5_context context,
			      mit_krb5_flags flags,
			      mit_krb5_address * const *addr,
			      mit_krb5_enctype *enctype,
			      mit_krb5_preauthtype *preauth,
			      const char *password,
			      mit_krb5_ccache cache,
			      mit_krb5_creds *cred,
			      mit_krb5_kdc_rep **rep)
{
    struct comb_principal *p;
    krb5_error_code ret;
    krb5_creds hcreds;

    LOG_ENTRY();

    if (rep)
	*rep = NULL;

    if (cred->client)
	p = (struct comb_principal *)cred->client;
    else
	return KRB5_PRINC_NOMATCH;

    memset(&hcreds, 0, sizeof(hcreds));

    ret = heim_krb5_get_init_creds_password(HC(context), &hcreds, p->heim, password,
					    NULL, NULL, 0, NULL, NULL);
    if (ret)
	return ret;
    
    if (cache)
	heim_krb5_cc_store_cred(HC(context), (krb5_ccache)cache, &hcreds);

    heim_krb5_free_cred_contents(HC(context), &hcreds);

    return 0;
}

void KRB5_CALLCONV
krb5_verify_init_creds_opt_init(mit_krb5_verify_init_creds_opt *options)
{
    memset(options, 0, sizeof(options));
}

void KRB5_CALLCONV
krb5_verify_init_creds_opt_set_ap_req_nofail(mit_krb5_verify_init_creds_opt *options,
					     int ap_req_nofail)
{
    if (ap_req_nofail) {
	options->flags |= MIT_KRB5_VERIFY_INIT_CREDS_OPT_AP_REQ_NOFAIL;
    } else {
	options->flags &= ~MIT_KRB5_VERIFY_INIT_CREDS_OPT_AP_REQ_NOFAIL;
    }
    options->ap_req_nofail = ap_req_nofail;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_verify_init_creds(mit_krb5_context context,
		       mit_krb5_creds *creds,
		       mit_krb5_principal ap_req_server,
		       mit_krb5_keytab ap_req_keytab,
		       mit_krb5_ccache *ccache,
		       mit_krb5_verify_init_creds_opt *options)
{
    struct comb_principal *p = (struct comb_principal *)ap_req_server;
    krb5_error_code ret;
    krb5_creds hcreds;
    krb5_verify_init_creds_opt hopts;

    memset(&hcreds, 0, sizeof(hcreds));
    heim_krb5_verify_init_creds_opt_init(&hopts);
    
    if (options->ap_req_nofail)
	heim_krb5_verify_init_creds_opt_set_ap_req_nofail(&hopts, options->ap_req_nofail);

    mshim_mcred2hcred(HC(context), creds, &hcreds);

    ret = heim_krb5_verify_init_creds(HC(context), 
				      &hcreds, p->heim,
				      (krb5_keytab)ap_req_keytab,
				      (krb5_ccache *)ccache,
				      &hopts);
    heim_krb5_free_cred_contents(HC(context), &hcreds);

    return ret;
}


