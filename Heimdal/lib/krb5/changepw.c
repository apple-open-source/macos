/*
 * Copyright (c) 1997 - 2005 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
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

#include "krb5_locl.h"

struct request {
    krb5_auth_context ac;
    krb5_creds *creds;
    krb5_principal target;
    const char *password;
};

/*
 * Change password protocol defined by
 * draft-ietf-cat-kerb-chg-password-02.txt
 *
 * Share the response part of the protocol with MS set password
 * (RFC3244)
 */

static krb5_error_code
chgpw_prexmit(krb5_context context, int proto,
	      void *ctx, rk_socket_t fd, krb5_data *data)
{
    struct request *request = ctx;
    krb5_data ap_req_data, krb_priv_data, passwd_data;
    krb5_storage *sp = NULL;
    krb5_error_code ret;
    size_t len;

    krb5_data_zero(&ap_req_data);
    krb5_data_zero(&krb_priv_data);

    ret = krb5_auth_con_genaddrs(context, request->ac, fd,
				 KRB5_AUTH_CONTEXT_GENERATE_LOCAL_ADDR);
    if (ret)
	goto out;

    ret = krb5_mk_req_extended(context,
			       &request->ac,
			       AP_OPTS_MUTUAL_REQUIRED | AP_OPTS_USE_SUBKEY,
			       NULL,
			       request->creds,
			       &ap_req_data);
    if (ret)
	goto out;

    passwd_data.data   = rk_UNCONST(request->password);
    passwd_data.length = strlen(request->password);

    ret = krb5_mk_priv(context,
		       request->ac,
		       &passwd_data,
		       &krb_priv_data,
		       NULL);
    if (ret)
	goto out;

    sp = krb5_storage_emem();
    if (sp == NULL) {
	ret = ENOMEM;
	goto out;
    }

    len = 6 + ap_req_data.length + krb_priv_data.length;

    ret = krb5_store_uint16(sp, len);
    if (ret) goto out;
    ret = krb5_store_uint16(sp, 1);
    if (ret) goto out;
    ret = krb5_store_uint16(sp, ap_req_data.length);
    if (ret) goto out;
    ret = krb5_storage_write(sp, ap_req_data.data, ap_req_data.length);
    if (ret != ap_req_data.length) {
	ret = EINVAL;
	goto out;
    }
    ret = krb5_storage_write(sp, krb_priv_data.data, krb_priv_data.length);
    if (ret != krb_priv_data.length) {
	ret = EINVAL;
	goto out;
    }

    ret = krb5_storage_to_data(sp, data);

 out:
    if (ret)
	_krb5_debugx(context, 10, "chgpw_prexmit failed with: %d", ret);
    if (sp)
	krb5_storage_free(sp);
    krb5_data_free(&krb_priv_data);
    krb5_data_free(&ap_req_data);
    return ret;
}

/*
 * Set password protocol as defined by RFC3244 --
 * Microsoft Windows 2000 Kerberos Change Password and Set Password Protocols
 */

static krb5_error_code
setpw_prexmit(krb5_context context, int proto,
	      void *ctx, int fd, krb5_data *data)
{
    struct request *request = ctx;
    krb5_data ap_req_data, krb_priv_data, pwd_data;
    krb5_error_code ret;
    ChangePasswdDataMS chpw;
    krb5_storage *sp = NULL;
    size_t len;

    krb5_data_zero(&ap_req_data);
    krb5_data_zero(&krb_priv_data);
    krb5_data_zero(&pwd_data);

    ret = krb5_auth_con_genaddrs(context, request->ac, fd,
				 KRB5_AUTH_CONTEXT_GENERATE_LOCAL_ADDR);
    if (ret)
	goto out;

    ret = krb5_mk_req_extended(context,
			       &request->ac,
			       AP_OPTS_MUTUAL_REQUIRED | AP_OPTS_USE_SUBKEY,
			       NULL,
			       request->creds,
			       &ap_req_data);
    if (ret)
	goto out;

    chpw.newpasswd.length = strlen(request->password);
    chpw.newpasswd.data = rk_UNCONST(request->password);
    if (request->target) {
	chpw.targname = &request->target->name;
	chpw.targrealm = &request->target->realm;
    } else {
	chpw.targname = NULL;
	chpw.targrealm = NULL;
    }

    ASN1_MALLOC_ENCODE(ChangePasswdDataMS, pwd_data.data, pwd_data.length,
		       &chpw, &len, ret);
    if (ret)
	goto out;
    if(pwd_data.length != len)
	krb5_abortx(context, "internal error in ASN.1 encoder");

    ret = krb5_mk_priv (context,
			request->ac,
			&pwd_data,
			&krb_priv_data,
			NULL);
    if (ret)
	goto out;

    sp = krb5_storage_emem();
    if (sp == NULL) {
	ret = ENOMEM;
	goto out;
    }

    len = 6 + ap_req_data.length + krb_priv_data.length;

    ret = krb5_store_uint16(sp, len);
    if (ret) goto out;
    ret = krb5_store_uint16(sp, 0xff80);
    if (ret) goto out;
    ret = krb5_store_uint16(sp, ap_req_data.length);
    if (ret) goto out;
    ret = krb5_storage_write(sp, ap_req_data.data, ap_req_data.length);
    if (ret != ap_req_data.length) {
	ret = EINVAL;
	goto out;
    }
    ret = krb5_storage_write(sp, krb_priv_data.data, krb_priv_data.length);
    if (ret != krb_priv_data.length) {
	ret = EINVAL;
	goto out;
    }

    ret = krb5_storage_to_data(sp, data);

 out:
    if (ret)
	_krb5_debugx(context, 10, "setpw_prexmit failed with %d", ret);
    if (sp)
	krb5_storage_free(sp);
    krb5_data_free(&krb_priv_data);
    krb5_data_free(&ap_req_data);
    krb5_data_free(&pwd_data);

    return ret;
}

static krb5_error_code
process_reply(krb5_context context,
	      krb5_auth_context auth_context,
	      krb5_data *data,
	      int *result_code,
	      krb5_data *result_code_string,
	      krb5_data *result_string)
{
    krb5_error_code ret;
    ssize_t len;
    uint16_t pkt_len, pkt_ver;
    krb5_data ap_rep_data;
    uint8_t *reply;

    krb5_auth_con_clear(context, auth_context,
			KRB5_AUTH_CONTEXT_CLEAR_LOCAL_ADDR|KRB5_AUTH_CONTEXT_CLEAR_REMOTE_ADDR);
    len = data->length;
    reply = data->data;;

    if (len < 6) {
	krb5_data_format(result_string, "server sent to too short message "
			 "(%ld bytes)", (long)len);
	*result_code = KRB5_KPASSWD_MALFORMED;
	return 0;
    }

    pkt_len = (reply[0] << 8) | (reply[1]);
    pkt_ver = (reply[2] << 8) | (reply[3]);

    if ((pkt_len != len) || (reply[1] == 0x7e || reply[1] == 0x5e)) {
	KRB_ERROR error;
	size_t size;
	u_char *p;

	memset(&error, 0, sizeof(error));

	ret = decode_KRB_ERROR(reply, len, &error, &size);
	if (ret)
	    return ret;

	if (error.e_data->length < 2) {
	    krb5_data_format(result_string, "server sent too short "
		     "e_data to print anything usable");
	    free_KRB_ERROR(&error);
	    *result_code = KRB5_KPASSWD_MALFORMED;
	    return 0;
	}

	p = error.e_data->data;
	*result_code = (p[0] << 8) | p[1];
	if (error.e_data->length == 2)
	    krb5_data_format(result_string, "server only sent error code");
	else
	    krb5_data_copy (result_string,
			    p + 2,
			    error.e_data->length - 2);
	free_KRB_ERROR(&error);
	return 0;
    }

    if (pkt_len != len) {
	krb5_data_format(result_string, "client: wrong len in reply");
	*result_code = KRB5_KPASSWD_MALFORMED;
	return 0;
    }
    if (pkt_ver != KRB5_KPASSWD_VERS_CHANGEPW) {
	krb5_data_format(result_string,
		  "client: wrong version number (%d)", pkt_ver);
	*result_code = KRB5_KPASSWD_MALFORMED;
	return 0;
    }

    ap_rep_data.data = reply + 6;
    ap_rep_data.length  = (reply[4] << 8) | (reply[5]);

    if (reply + len < (u_char *)ap_rep_data.data + ap_rep_data.length) {
	krb5_data_format(result_string, "client: wrong AP len in reply");
	*result_code = KRB5_KPASSWD_MALFORMED;
	return 0;
    }

    if (ap_rep_data.length) {
	krb5_ap_rep_enc_part *ap_rep;
	krb5_data priv_data;
	u_char *p;

	priv_data.data   = (u_char*)ap_rep_data.data + ap_rep_data.length;
	priv_data.length = len - ap_rep_data.length - 6;

	ret = krb5_rd_rep (context,
			   auth_context,
			   &ap_rep_data,
			   &ap_rep);
	if (ret)
	    return ret;

	krb5_free_ap_rep_enc_part (context, ap_rep);

	ret = krb5_rd_priv (context,
			    auth_context,
			    &priv_data,
			    result_code_string,
			    NULL);
	if (ret) {
	    krb5_data_free (result_code_string);
	    return ret;
	}

	if (result_code_string->length < 2) {
	    *result_code = KRB5_KPASSWD_MALFORMED;
	    krb5_data_format(result_string,
		      "client: bad length in result");
	    return 0;
	}

        p = result_code_string->data;

        *result_code = (p[0] << 8) | p[1];
        krb5_data_copy (result_string,
                        (unsigned char*)result_code_string->data + 2,
                        result_code_string->length - 2);
        return 0;
    } else {
	KRB_ERROR error;
	size_t size;
	u_char *p;

	ret = decode_KRB_ERROR(reply + 6, len - 6, &error, &size);
	if (ret) {
	    return ret;
	}
	if (error.e_data->length < 2) {
	    krb5_warnx (context, "too short e_data to print anything usable");
	    return 1;		/* XXX */
	}

	p = error.e_data->data;
	*result_code = (p[0] << 8) | p[1];
	krb5_data_copy (result_string,
			p + 2,
			error.e_data->length - 2);
	return 0;
    }
}


/*
 * change the password using the credentials in `creds' (for the
 * principal indicated in them) to `newpw', storing the result of
 * the operation in `result_*' and an error code or 0.
 */

typedef krb5_error_code (*kpwd_process_reply) (krb5_context,
					       krb5_auth_context,
					       krb5_data *data,
					       int *,
					       krb5_data *,
					       krb5_data *);

static struct kpwd_proc {
    const char *name;
    int flags;
#define SUPPORT_TCP	1
#define SUPPORT_UDP	2
#define SUPPORT_ADMIN	4
    krb5_sendto_prexmit prexmit;
    kpwd_process_reply process_rep;
} procs[] = {
    {
	"MS set password",
	SUPPORT_TCP|SUPPORT_UDP|SUPPORT_ADMIN,
	setpw_prexmit,
	process_reply
    },
    {
	"change password",
	SUPPORT_UDP,
	chgpw_prexmit,
	process_reply
    },
    { NULL, 0, NULL, NULL }
};

/*
 *
 */

static krb5_error_code
change_password_loop(krb5_context	context,
		     struct request *request,
		     int		*result_code,
		     krb5_data		*result_code_string,
		     krb5_data		*result_string,
		     struct kpwd_proc	*proc)
{
    krb5_error_code ret;
    krb5_data zero, zero2;
    krb5_sendto_ctx ctx = NULL;
    krb5_realm realm;

    krb5_data_zero(&zero);
    krb5_data_zero(&zero2);

    if (request->target)
	realm = request->target->realm;
    else
	realm = request->creds->client->realm;

    _krb5_debugx(context, 1, "trying to set password using: %s in realm %s",
		proc->name, realm);

    ret = krb5_auth_con_init(context, &request->ac);
    if (ret)
	goto out;

    krb5_auth_con_setflags(context, request->ac, KRB5_AUTH_CONTEXT_DO_SEQUENCE);

    ret = krb5_sendto_ctx_alloc(context, &ctx);
    if (ret)
	goto out;

    krb5_sendto_ctx_set_type(ctx, KRB5_KRBHST_CHANGEPW);

    /* XXX this is a evil hack */
    if (request->creds->ticket.length > 700) {
	_krb5_debugx(context, 1, "using TCP since the ticket is large: %lu",
		    (unsigned long)request->creds->ticket.length);
	krb5_sendto_ctx_add_flags(ctx, KRB5_KRBHST_FLAGS_LARGE_MSG);
    }

    _krb5_sendto_ctx_set_prexmit(ctx, proc->prexmit, request);

    ret = krb5_sendto_context(context, ctx, &zero, realm, &zero2);

    if (ret == 0)
	ret = proc->process_rep(context, request->ac, &zero2,
				result_code, result_code_string, result_string);

 out:
    _krb5_debugx(context, 1, "set password using %s returned: %d result_code %d",
		 proc->name, ret, *result_code);

    krb5_auth_con_free(context, request->ac);
    if (ctx)
	krb5_sendto_ctx_free(context, ctx);

    krb5_data_free(&zero2);

    if (ret == KRB5_KDC_UNREACH) {
	krb5_set_error_message(context,
			       ret,
			       N_("Unable to reach any changepw server "
				 " in realm %s", "realm"), realm);
	*result_code = KRB5_KPASSWD_HARDERROR;
    }
    return ret;
}

#ifndef HEIMDAL_SMALLER

static struct kpwd_proc *
find_chpw_proto(const char *name)
{
    struct kpwd_proc *p;
    for (p = procs; p->name != NULL; p++) {
	if (strcmp(p->name, name) == 0)
	    return p;
    }
    return NULL;
}

/**
 * Deprecated: krb5_change_password() is deprecated, use krb5_set_password().
 *
 * @param context a Keberos context
 * @param creds
 * @param newpw
 * @param result_code
 * @param result_code_string
 * @param result_string
 *
 * @return On sucess password is changed.

 * @ingroup @krb5_deprecated
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_change_password (krb5_context	context,
		      krb5_creds	*creds,
		      const char	*newpw,
		      int		*result_code,
		      krb5_data		*result_code_string,
		      krb5_data		*result_string)
    KRB5_DEPRECATED_FUNCTION("Use X instead")
{
    struct kpwd_proc *p = find_chpw_proto("change password");
    struct request request;

    *result_code = KRB5_KPASSWD_MALFORMED;
    result_code_string->data = result_string->data = NULL;
    result_code_string->length = result_string->length = 0;

    if (p == NULL)
	return KRB5_KPASSWD_MALFORMED;

    request.ac = NULL;
    request.target = targprinc;
    request.creds = creds;
    request.password = password;

    return change_password_loop(context, &request,
				result_code, result_code_string,
				result_string, p);
}
#endif /* HEIMDAL_SMALLER */

/**
 * Change password using creds.
 *
 * @param context a Keberos context
 * @param creds The initial kadmin/passwd for the principal or an admin principal
 * @param newpw The new password to set
 * @param targprinc For most callers should pass NULL in this
 *        argument. Targprinc is the principal to change the password
 *        for. This argument should only be set when you want to
 *        change another Kerberos principal's password, ie you are an
 *        admin. If NULL, the default authenticating principal in the
 *        creds argument is used instead.
 * @param result_code Result code, KRB5_KPASSWD_SUCCESS is when password is changed.
 * @param result_code_string binary message from the server, contains
 * at least the result_code.
 * @param result_string A message from the kpasswd service or the
 * library in human printable form. The string is NUL terminated.
 *
 * @return On sucess and *result_code is KRB5_KPASSWD_SUCCESS, the password is changed.

 * @ingroup @krb5
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_set_password(krb5_context context,
		  krb5_creds *creds,
		  const char *newpw,
		  krb5_principal targprinc,
		  int *result_code,
		  krb5_data *result_code_string,
		  krb5_data *result_string)
{
    krb5_error_code ret = 0;
    struct request request;
    int i;

    *result_code = KRB5_KPASSWD_MALFORMED;
    krb5_data_zero(result_code_string);
    krb5_data_zero(result_string);

    _krb5_debugx(context, 1, "trying to set password");

    request.ac = NULL;
    request.target = targprinc;
    request.creds = creds;
    request.password = newpw;

    for (i = 0; procs[i].name != NULL; i++) {
	/* don't try methods that don't support admind change password */
	if (targprinc && (procs[i].flags & SUPPORT_ADMIN) == 0)
	    continue;

	ret = change_password_loop(context, &request,
				   result_code, result_code_string,
				   result_string,
				   &procs[i]);
	if (ret == 0 && *result_code == 0)
	    break;
    }

    return ret;
}

/**
 * Change password using a credential cache that contains an initial
 * credential or an admin credential.
 * 
 * @param context a Keberos context
 * @param ccache the credential cache to use to find the
 *        kadmin/changepw principal.
 * @param newpw The new password to set
 * @param targprinc For most callers should pass NULL in this
 *        argument. Targprinc is the principal to change the password
 *        for. This argument should only be set when you want to
 *        change another Kerberos principal's password, ie you are an
 *        admin. If NULL, the default authenticating principal in the
 *        creds argument is used instead.
 * @param result_code Result code, KRB5_KPASSWD_SUCCESS is when password is changed.
 * @param result_code_string binary message from the server, contains
 * at least the result_code.
 * @param result_string A message from the kpasswd service or the
 * library in human printable form. The string is NUL terminated.
 *
 * @return On sucess and *result_code is KRB5_KPASSWD_SUCCESS, the password is changed.
 *
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_set_password_using_ccache(krb5_context context,
			       krb5_ccache ccache,
			       const char *newpw,
			       krb5_principal targprinc,
			       int *result_code,
			       krb5_data *result_code_string,
			       krb5_data *result_string)
{
    krb5_creds creds, *credsp = NULL;
    krb5_error_code ret;
    krb5_principal principal = NULL;

    *result_code = KRB5_KPASSWD_MALFORMED;
    krb5_data_zero(result_code_string);
    krb5_data_zero(result_string);

    memset(&creds, 0, sizeof(creds));

    if (targprinc == NULL) {
	ret = krb5_cc_get_principal(context, ccache, &principal);
	if (ret)
	    return ret;
    } else
	principal = targprinc;

    ret = krb5_make_principal(context, &creds.server,
			      krb5_principal_get_realm(context, principal),
			      "kadmin", "changepw", NULL);
    if (ret)
	goto out;

    ret = krb5_cc_get_principal(context, ccache, &creds.client);
    if (ret) {
        krb5_free_principal(context, creds.server);
	goto out;
    }

    ret = krb5_get_credentials(context, 0, ccache, &creds, &credsp);
    krb5_free_principal(context, creds.server);
    krb5_free_principal(context, creds.client);
    if (ret)
	goto out;

    ret = krb5_set_password(context,
			    credsp,
			    newpw,
			    targprinc,
			    result_code,
			    result_code_string,
			    result_string);

 out:
    if (credsp)
	krb5_free_creds(context, credsp);
    if (targprinc == NULL)
	krb5_free_principal(context, principal);
    return ret;
}

/*
 *
 */

KRB5_LIB_FUNCTION const char* KRB5_LIB_CALL
krb5_passwd_result_to_string (krb5_context context,
			      int result)
{
    static const char *strings[] = {
	"Success",
	"Malformed",
	"Hard error",
	"Auth error",
	"Soft error" ,
	"Access denied",
	"Bad version",
	"Initial flag needed"
    };

    if (result < 0 || result > KRB5_KPASSWD_INITIAL_FLAG_NEEDED)
	return "unknown result code";
    else
	return strings[result];
}
