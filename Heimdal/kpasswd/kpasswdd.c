/*
 * Copyright (c) 1997-2005 Kungliga Tekniska Högskolan
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

#include "kpasswd_locl.h"

#include <kadm5/admin.h>
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#include <hdb.h>
#include <heim-ipc.h>
#include <kadm5/private.h>

static krb5_context context;
static krb5_log_facility *log_facility;

static struct getarg_strings addresses_str;
krb5_addresses explicit_addresses;

static krb5_keytab global_keytab = NULL;

static sig_atomic_t exit_flag = 0;

static void
add_one_address (const char *str, int first)
{
    krb5_error_code ret;
    krb5_addresses tmp;

    ret = krb5_parse_address (context, str, &tmp);
    if (ret)
	krb5_err (context, 1, ret, "parse_address `%s'", str);
    if (first)
	krb5_copy_addresses(context, &tmp, &explicit_addresses);
    else
	krb5_append_addresses(context, &explicit_addresses, &tmp);
    krb5_free_addresses (context, &tmp);
}

static void
send_reply(krb5_data *ap_rep, krb5_data *rest,
	   krb5_data *out_data)
{
    uint16_t ap_rep_len;
    u_char *p;

    if (ap_rep)
	ap_rep_len = ap_rep->length;
    else
	ap_rep_len = 0;

    if (krb5_data_alloc(out_data, 6 + ap_rep_len + rest->length))
	return;

    p = out_data->data;
    *p++ = (out_data->length >> 8) & 0xFF;
    *p++ = (out_data->length >> 0) & 0xFF;
    *p++ = 0;
    *p++ = 1;
    *p++ = (ap_rep_len >> 8) & 0xFF;
    *p++ = (ap_rep_len >> 0) & 0xFF;

    if (ap_rep_len) {
	memcpy(p, ap_rep->data, ap_rep->length);
	p += ap_rep->length;
    }

    memcpy(p, rest->data, rest->length);
}

static int
make_result (krb5_data *data,
	     uint16_t result_code,
	     const char *expl)
{
    char *str;
    krb5_data_zero (data);

    data->length = asprintf (&str,
			     "%c%c%s",
			     (result_code >> 8) & 0xFF,
			     result_code & 0xFF,
			     expl);

    if (str == NULL) {
	krb5_warnx (context, "Out of memory generating error reply");
	return 1;
    }
    data->data = str;
    return 0;
}

static void
reply_error(krb5_realm realm,
	    krb5_error_code error_code,
	    uint16_t result_code,
	    const char *expl,
	    krb5_data *out_data)
{
    krb5_error_code ret;
    krb5_data error_data;
    krb5_data e_data;
    krb5_principal server = NULL;

    if (make_result(&e_data, result_code, expl))
	return;

    if (realm) {
	ret = krb5_make_principal (context, &server, realm,
				   "kadmin", "changepw", NULL);
	if (ret) {
	    krb5_data_free (&e_data);
	    return;
	}
    }

    ret = krb5_mk_error (context,
			 error_code,
			 NULL,
			 &e_data,
			 NULL,
			 server,
			 NULL,
			 NULL,
			 &error_data);
    if (server)
	krb5_free_principal(context, server);
    krb5_data_free (&e_data);
    if (ret) {
	krb5_warn (context, ret, "Could not even generate error reply");
	return;
    }
    send_reply(NULL, &error_data, out_data);
    krb5_data_free (&error_data);
}

static void
reply_priv(krb5_auth_context auth_context,
	   uint16_t result_code,
	   const char *expl,
	   krb5_data *out_data)
{
    krb5_error_code ret;
    krb5_data krb_priv_data;
    krb5_data ap_rep_data;
    krb5_data e_data;

    ret = krb5_mk_rep (context,
		       auth_context,
		       &ap_rep_data);
    if (ret) {
	krb5_warn (context, ret, "Could not even generate error reply");
	return;
    }

    if (make_result(&e_data, result_code, expl))
	return;

    ret = krb5_mk_priv (context,
			auth_context,
			&e_data,
			&krb_priv_data,
			NULL);
    krb5_data_free (&e_data);
    if (ret) {
	krb5_warn (context, ret, "Could not even generate error reply");
	return;
    }
    send_reply(&ap_rep_data, &krb_priv_data, out_data);
    krb5_data_free (&ap_rep_data);
    krb5_data_free (&krb_priv_data);
}

/*
 * Change the password for `principal', sending the reply back on `s'
 * (`sa', `sa_size') to `pwd_data'.
 */

static void
change(krb5_auth_context auth_context,
       krb5_principal admin_principal,
       uint16_t version,
       krb5_data *in_data,
       krb5_data *out_data)
{
    krb5_error_code ret;
    char *client = NULL, *admin = NULL;
    const char *pwd_reason;
    kadm5_config_params conf;
    void *kadm5_handle = NULL;
    krb5_principal principal = NULL;
    krb5_data *pwd_data = NULL;
    char *tmp;
    ChangePasswdDataMS chpw;

    memset (&conf, 0, sizeof(conf));
    memset(&chpw, 0, sizeof(chpw));

    if (version == KRB5_KPASSWD_VERS_CHANGEPW) {
	ret = krb5_copy_data(context, in_data, &pwd_data);
	if (ret) {
	    krb5_warn (context, ret, "krb5_copy_data");
	    reply_priv(auth_context, KRB5_KPASSWD_MALFORMED,
		       "out out memory copying password", out_data);
	    return;
	}
	principal = admin_principal;
    } else if (version == KRB5_KPASSWD_VERS_SETPW) {
	size_t len;

	ret = decode_ChangePasswdDataMS(in_data->data, in_data->length,
					&chpw, &len);
	if (ret) {
	    krb5_warn (context, ret, "decode_ChangePasswdDataMS");
	    reply_priv(auth_context, KRB5_KPASSWD_MALFORMED,
			"malformed ChangePasswdData", out_data);
	    return;
	}
	

	ret = krb5_copy_data(context, &chpw.newpasswd, &pwd_data);
	if (ret) {
	    krb5_warn (context, ret, "krb5_copy_data");
	    reply_priv(auth_context, KRB5_KPASSWD_MALFORMED,
			"out out memory copying password", out_data);
	    goto out;
	}

	if (chpw.targname == NULL && chpw.targrealm != NULL) {
	    krb5_warn (context, ret, "kadm5_init_with_password_ctx");
	    reply_priv(auth_context, KRB5_KPASSWD_MALFORMED,
			"targrealm but not targname", out_data);
	    goto out;
	}

	if (chpw.targname) {
	    krb5_principal_data princ;

	    princ.name = *chpw.targname;
	    princ.realm = *chpw.targrealm;
	    if (princ.realm == NULL) {
		ret = krb5_get_default_realm(context, &princ.realm);

		if (ret) {
		    krb5_warnx (context,
				"kadm5_init_with_password_ctx: "
				"failed to allocate realm");
		    reply_priv(auth_context, KRB5_KPASSWD_SOFTERROR,
				"failed to allocate realm", out_data);
		    goto out;
		}
	    }
	    ret = krb5_copy_principal(context, &princ, &principal);
	    if (*chpw.targrealm == NULL)
		free(princ.realm);
	    if (ret) {
		krb5_warn(context, ret, "krb5_copy_principal");
		reply_priv(auth_context, KRB5_KPASSWD_HARDERROR,
			   "failed to allocate principal", out_data);
		goto out;
	    }
	} else
	    principal = admin_principal;
    } else {
	krb5_warnx (context, "kadm5_init_with_password_ctx: unknown proto");
	reply_priv(auth_context, KRB5_KPASSWD_HARDERROR,
		    "Unknown protocol used", out_data);
	return;
    }

    ret = krb5_unparse_name (context, admin_principal, &admin);
    if (ret) {
	krb5_warn (context, ret, "unparse_name failed");
	reply_priv(auth_context, KRB5_KPASSWD_HARDERROR, "out of memory error", out_data);
	goto out;
    }

    conf.realm = principal->realm;
    conf.mask |= KADM5_CONFIG_REALM;

    ret = kadm5_s_init_with_password_ctx(context,
					 admin,
					 NULL,
					 KADM5_ADMIN_SERVICE,
					 &conf, 0, 0,
					 &kadm5_handle);
    if (ret) {
	krb5_warn (context, ret, "kadm5_init_with_password_ctx");
	reply_priv(auth_context, 2, "Internal error", out_data);
	goto out;
    }

    ret = krb5_unparse_name(context, principal, &client);
    if (ret) {
	krb5_warn (context, ret, "unparse_name failed");
	reply_priv(auth_context, KRB5_KPASSWD_HARDERROR, "out of memory error", out_data);
	goto out;
    }

    /*
     * Check password quality if not changing as administrator
     */

    if (krb5_principal_compare(context, admin_principal, principal) == TRUE) {

	pwd_reason = kadm5_check_password_quality (context, principal,
						   pwd_data);
	if (pwd_reason != NULL ) {
	    krb5_warnx (context,
			"%s didn't pass password quality check with error: %s",
			client, pwd_reason);
	    reply_priv(auth_context, KRB5_KPASSWD_SOFTERROR, pwd_reason, out_data);
	    goto out;
	}
	krb5_warnx (context, "Changing password for %s", client);
    } else {
	ret = _kadm5_acl_check_permission(kadm5_handle, KADM5_PRIV_CPW,
					  principal);
	if (ret) {
	    krb5_warn (context, ret,
		       "Check ACL failed for %s for changing %s password",
		       admin, client);
	    reply_priv(auth_context, KRB5_KPASSWD_HARDERROR, "permission denied", out_data);
	    goto out;
	}
	krb5_warnx (context, "%s is changing password for %s", admin, client);
    }

    ret = krb5_data_realloc(pwd_data, pwd_data->length + 1);
    if (ret) {
	krb5_warn (context, ret, "malloc: out of memory");
	reply_priv(auth_context, KRB5_KPASSWD_HARDERROR,
		    "Internal error", out_data);
	goto out;
    }
    tmp = pwd_data->data;
    tmp[pwd_data->length - 1] = '\0';

    ret = kadm5_s_chpass_principal_cond (kadm5_handle, principal, tmp, NULL);
    krb5_free_data (context, pwd_data);
    pwd_data = NULL;
    if (ret) {
	const char *str = krb5_get_error_message(context, ret);
	krb5_warnx(context, "kadm5_s_chpass_principal_cond: %s", str);
	reply_priv(auth_context, KRB5_KPASSWD_SOFTERROR,
		    str ? str : "Internal error", out_data);
	krb5_free_error_message(context, str);
	goto out;
    }
    reply_priv(auth_context, KRB5_KPASSWD_SUCCESS,
	       "Password changed", out_data);
out:
    free_ChangePasswdDataMS(&chpw);
    if (principal != admin_principal)
	krb5_free_principal(context, principal);
    if (admin)
	free(admin);
    if (client)
	free(client);
    if (pwd_data)
	krb5_free_data(context, pwd_data);
    if (kadm5_handle)
	kadm5_s_destroy (kadm5_handle);
}

static int
verify(krb5_auth_context *auth_context,
       krb5_realm *realms,
       krb5_keytab keytab,
       krb5_ticket **ticket,
       uint16_t *version,
       int s,
       struct sockaddr *sa,
       int sa_size,
       u_char *msg,
       size_t len,
       krb5_data *out_data)
{
    krb5_error_code ret;
    uint16_t pkt_len, pkt_ver, ap_req_len;
    krb5_data ap_req_data;
    krb5_data krb_priv_data;
    krb5_realm *r;

    /*
     * Only send an error reply if the request passes basic length
     * verification.  Otherwise, kpasswdd would reply to every UDP packet,
     * allowing an attacker to set up a ping-pong DoS attack via a spoofed UDP
     * packet with a source address of another UDP service that also replies
     * to every packet.
     *
     * Also suppress the error reply if ap_req_len is 0, which indicates
     * either an invalid request or an error packet.  An error packet may be
     * the result of a ping-pong attacker pointing us at another kpasswdd.
     */
    pkt_len = (msg[0] << 8) | (msg[1]);
    pkt_ver = (msg[2] << 8) | (msg[3]);
    ap_req_len = (msg[4] << 8) | (msg[5]);
    if (pkt_len != len) {
	krb5_warnx (context, "Strange len: %ld != %ld",
		    (long)pkt_len, (long)len);
	return 1;
    }
    if (ap_req_len == 0) {
	krb5_warnx (context, "Request is error packet (ap_req_len == 0)");
	return 1;
    }
    if (pkt_ver != KRB5_KPASSWD_VERS_CHANGEPW &&
	pkt_ver != KRB5_KPASSWD_VERS_SETPW) {
	krb5_warnx (context, "Bad version (%d)", pkt_ver);
	reply_error (NULL, 0, 1, "Wrong program version", out_data);
	return 1;
    }
    *version = pkt_ver;

    ap_req_data.data   = msg + 6;
    ap_req_data.length = ap_req_len;

    ret = krb5_rd_req (context,
		       auth_context,
		       &ap_req_data,
		       NULL,
		       keytab,
		       NULL,
		       ticket);
    if (ret) {
	krb5_warn (context, ret, "krb5_rd_req");
	reply_error (NULL, ret, 3, "Authentication failed", out_data);
	return 1;
    }

    /* verify realm and principal */
    for (r = realms; *r != NULL; r++) {
	krb5_principal principal;
	krb5_boolean same;

	ret = krb5_make_principal (context,
				   &principal,
				   *r,
				   "kadmin",
				   "changepw",
				   NULL);
	if (ret)
	    krb5_err (context, 1, ret, "krb5_make_principal");

	same = krb5_principal_compare(context, principal, (*ticket)->server);
	krb5_free_principal(context, principal);
	if (same == TRUE)
	    break;
    }
    if (*r == NULL) {
	char *str;
	krb5_unparse_name(context, (*ticket)->server, &str);
	krb5_warnx (context, "client used not valid principal %s", str);
	free(str);
	reply_error (NULL, ret, 1,
		     "Bad request", out_data);
	goto out;
    }

    if (strcmp((*ticket)->server->realm, (*ticket)->client->realm) != 0) {
	krb5_warnx (context, "server realm (%s) not same a client realm (%s)",
		    (*ticket)->server->realm, (*ticket)->client->realm);
	reply_error ((*ticket)->server->realm, ret, 1,
		     "Bad request", out_data);
	goto out;
    }

    if (!(*ticket)->ticket.flags.initial) {
	krb5_warnx (context, "initial flag not set");
	reply_error ((*ticket)->server->realm, ret, 1,
		     "Bad request", out_data);
	goto out;
    }
    krb_priv_data.data   = msg + 6 + ap_req_len;
    krb_priv_data.length = len - 6 - ap_req_len;

    ret = krb5_rd_priv (context,
			*auth_context,
			&krb_priv_data,
			out_data,
			NULL);

    if (ret) {
	krb5_warn (context, ret, "krb5_rd_priv");
	reply_error ((*ticket)->server->realm, ret, 3,
		     "Bad request", out_data);
	goto out;
    }
    return 0;
out:
    krb5_free_ticket (context, *ticket);
    ticket = NULL;
    return 1;
}

static krb5_error_code
process(int s,
	struct sockaddr *server,
	int server_size,
	struct sockaddr *client,
	int client_size,
	u_char *msg,
	int len,
	heim_idata *out_data)
{
    krb5_error_code ret;
    krb5_auth_context auth_context = NULL;
    krb5_ticket *ticket;
    uint16_t version;
    krb5_realm *realms;
    krb5_data clear_data;
    krb5_address client_addr, server_addr;

    krb5_data_zero(&clear_data);
    memset(&client_addr, 0, sizeof(client_addr));
    memset(&server_addr, 0, sizeof(server_addr));

    ret = krb5_sockaddr2address(context, client, &client_addr);
    if (ret) {
	krb5_warn(context, ret, "krb5_sockaddr2address");
	goto out;
    }
    ret = krb5_sockaddr2address(context, server, &server_addr);
    if (ret) {
	krb5_warn(context, ret, "krb5_sockaddr2address");
	goto out;
    }

    {
	char cstr[200];
	size_t ss;
	krb5_print_address(&client_addr, cstr, sizeof(cstr), &ss);
	krb5_warnx(context, "request from client: %s", cstr);
    }

    ret = krb5_get_default_realms(context, &realms);
    if (ret) {
	krb5_warn(context, ret, "krb5_get_default_realms");
	return ret;
    }

    ret = krb5_auth_con_init(context, &auth_context);
    if (ret) {
	krb5_warn (context, ret, "krb5_auth_con_init");
	goto out;
    }

    krb5_auth_con_setflags(context, auth_context,
			   KRB5_AUTH_CONTEXT_DO_SEQUENCE);

    if (verify(&auth_context, realms, global_keytab, &ticket,
	       &version, s, client, client_size, msg, len, &clear_data) == 0)
    {
	ret = krb5_auth_con_setaddrs(context,
				     auth_context,
				     &server_addr,
				     &client_addr);
	if (ret) {
	    krb5_warn(context, ret, "krb5_sockaddr2address");
	    goto out;
	}

	change(auth_context, ticket->client,
	       version, &clear_data, out_data);

	krb5_free_ticket(context, ticket);
    }

out:
    krb5_free_address(context, &client_addr);
    krb5_free_address(context, &server_addr);
    if (clear_data.data)
	memset(clear_data.data, 0, clear_data.length);
    krb5_data_free(&clear_data);

    krb5_auth_con_free(context, auth_context);
    krb5_free_host_realm(context, realms);
    return ret;
}

struct data {
    rk_socket_t s;
    struct sockaddr *sa;
    struct sockaddr_storage __ss;
    krb5_socklen_t sa_size;
    heim_sipc service;
    struct data *next;
};

/*
 * Linked list to not leak memory
 */
static struct data *head_data = NULL;

/*
 *
 */

static void
passwd_service(void *ctx, const heim_idata *req,
	       const heim_icred cred,
	       heim_ipc_complete complete,
	       heim_sipc_call cctx)
{
    struct data *data = ctx;
    heim_idata out_data;
    krb5_error_code ret;
    struct sockaddr *client, *server;
    krb5_socklen_t client_size, server_size;
    
    client = heim_ipc_cred_get_client_address(cred, &client_size);
    heim_assert(client != NULL, "no address from client");
    
    server = heim_ipc_cred_get_server_address(cred, &server_size);
    heim_assert(server != NULL, "no address from server");

    krb5_data_zero(&out_data);

    ret = process(data->s,
		  server, server_size,
		  client, client_size,
		  req->data, req->length, &out_data);

    complete(cctx, ret, &out_data);

    memset (out_data.data, 0, out_data.length);
    krb5_data_free(&out_data);
}



static void
listen_on(krb5_context context, krb5_address *addr, int type, int port)
{
    krb5_error_code ret;
    struct data *data;

    data = calloc(1, sizeof(*data));
    if (data == NULL)
	krb5_errx(context, 1, "out of memory");
    
    data->sa = (struct sockaddr *)&data->__ss;
    data->sa_size = sizeof(data->__ss);

    krb5_addr2sockaddr(context, addr, data->sa, &data->sa_size, port);

    data->s = socket(data->sa->sa_family, type, 0);
    if (data->s < 0) {
	krb5_warn(context, errno, "socket");
	free(data);
	return;
    }

    socket_set_ipv6only(data->s, 1);

    if (bind(data->s, data->sa, data->sa_size) < 0) {
	char str[128];
	size_t len;
	int save_errno = errno;

	ret = krb5_print_address (addr, str, sizeof(str), &len);
	if (ret)
	    strlcpy(str, "unknown address", sizeof(str));
	krb5_warn (context, save_errno, "bind(%s/%d)", str, ntohs(port));
	rk_closesocket(data->s);
	free(data);
	return;
    }

    if(type == SOCK_STREAM) {
	if (listen(data->s, SOMAXCONN) < 0){
	    char a_str[256];
	    size_t len;
	    
	    krb5_print_address(addr, a_str, sizeof(a_str), &len);
	    krb5_warn(context, errno, "listen %s/%d", a_str, ntohs(port));
	    rk_closesocket(data->s);
	    free(data);
	    return;
	}

	ret = heim_sipc_stream_listener(data->s,
					HEIM_SIPC_TYPE_UINT32 | HEIM_SIPC_TYPE_ONE_REQUEST,
					passwd_service, data, &data->service);
	if (ret)
	    errx(1, "heim_sipc_stream_listener: %d", ret);
    } else {
	ret = heim_sipc_service_dgram(data->s, 0,
				      passwd_service, data, &data->service);
	if (ret)
	    errx(1, "heim_sipc_service_dgram: %d", ret);
    }

    data->next = head_data;
    head_data = data;
}

static int
doit(int port)
{
    krb5_error_code ret;
    krb5_addresses addrs;
    unsigned i;

    if (explicit_addresses.len) {
	addrs = explicit_addresses;
    } else {
#if defined(IPV6_PKTINFO) && defined(IP_PKTINFO)
	ret = krb5_get_all_any_addrs(context, &addrs);
#else
	ret = krb5_get_all_server_addrs(context, &addrs);
#endif
	if (ret)
	    krb5_err (context, 1, ret, "krb5_get_all_server_addrs");
    }

    for (i = 0; i < addrs.len; ++i) {
	listen_on(context, &addrs.val[i], SOCK_STREAM, port);
	listen_on(context, &addrs.val[i], SOCK_DGRAM, port);
    }	

    heim_ipc_main();

    krb5_free_addresses (context, &addrs);
    krb5_free_context (context);
    return 0;
}

static RETSIGTYPE
sigterm(int sig)
{
    exit_flag = 1;
}

static const char *check_library  = NULL;
static const char *check_function = NULL;
static getarg_strings policy_libraries = { 0, NULL };
static char *keytab_str = "HDB:";
static char *realm_str;
static int version_flag;
static int help_flag;
static char *port_str;
static char *config_file;

struct getargs args[] = {
#ifdef HAVE_DLOPEN
    { "check-library", 0, arg_string, &check_library,
      "library to load password check function from", "library" },
    { "check-function", 0, arg_string, &check_function,
      "password check function to load", "function" },
    { "policy-libraries", 0, arg_strings, &policy_libraries,
      "password check function to load", "function" },
#endif
    { "addresses",	0,	arg_strings, &addresses_str,
      "addresses to listen on", "list of addresses" },
    { "keytab", 'k', arg_string, &keytab_str,
      "keytab to get authentication key from", "kspec" },
    { "config-file", 'c', arg_string, &config_file },
    { "realm", 'r', arg_string, &realm_str, "default realm", "realm" },
    { "port",  'p', arg_string, &port_str, "port" },
    { "version", 0, arg_flag, &version_flag },
    { "help", 0, arg_flag, &help_flag }
};
int num_args = sizeof(args) / sizeof(args[0]);

static void
usage(int ret)
{
    arg_printusage (args, num_args, NULL, "");
    exit (ret);
}


int
main (int argc, char **argv)
{
    int optidx = 0;
    krb5_error_code ret;
    char **files;
    int port, i;

    setprogname (argv[0]);

    ret = krb5_init_context (&context);
    if (ret)
	errx(1, "krb5_init_context failed: %d", ret);

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);

    if(version_flag) {
	print_version(NULL);
	exit(0);
    }

    if (config_file == NULL) {
	asprintf(&config_file, "%s/kdc.conf", hdb_db_dir(context));
	if (config_file == NULL)
	    errx(1, "out of memory");
    }

    ret = krb5_prepend_config_files_default(config_file, &files);
    if (ret)
	krb5_err(context, 1, ret, "getting configuration files");

    ret = krb5_set_config_files(context, files);
    krb5_free_config_files(files);
    if (ret)
	krb5_err(context, 1, ret, "reading configuration files");

    if(realm_str)
	krb5_set_default_realm(context, realm_str);

    krb5_openlog (context, "kpasswdd", &log_facility);
    krb5_set_warn_dest(context, log_facility);

    if (port_str != NULL) {
	struct servent *s = roken_getservbyname (port_str, "udp");

	if (s != NULL)
	    port = s->s_port;
	else {
	    char *ptr;

	    port = strtol (port_str, &ptr, 10);
	    if (port == 0 && ptr == port_str)
		krb5_errx (context, 1, "bad port `%s'", port_str);
	    port = htons(port);
	}
    } else
	port = krb5_getportbyname (context, "kpasswd", "udp", KPASSWD_PORT);

    ret = krb5_kt_register(context, &hdb_kt_ops);
    if(ret)
	krb5_err(context, 1, ret, "krb5_kt_register");

    ret = krb5_kt_resolve(context, keytab_str, &global_keytab);
    if(ret)
	krb5_err(context, 1, ret, "%s", keytab_str);

    kadm5_setup_passwd_quality_check (context, check_library, check_function);

    for (i = 0; i < policy_libraries.num_strings; i++) {
	ret = kadm5_add_passwd_quality_verifier(context,
						policy_libraries.strings[i]);
	if (ret)
	    krb5_err(context, 1, ret, "kadm5_add_passwd_quality_verifier");
    }
    ret = kadm5_add_passwd_quality_verifier(context, NULL);
    if (ret)
	krb5_err(context, 1, ret, "kadm5_add_passwd_quality_verifier");


    explicit_addresses.len = 0;

    if (addresses_str.num_strings) {
	int i;

	for (i = 0; i < addresses_str.num_strings; ++i)
	    add_one_address (addresses_str.strings[i], i == 0);
	free_getarg_strings (&addresses_str);
    } else {
	char **foo = krb5_config_get_strings (context, NULL,
					      "kdc", "addresses", NULL);

	if (foo != NULL) {
	    add_one_address (*foo++, TRUE);
	    while (*foo)
		add_one_address (*foo++, FALSE);
	}
    }

#ifdef HAVE_SIGACTION
    {
	struct sigaction sa;

	sa.sa_flags = 0;
	sa.sa_handler = sigterm;
	sigemptyset(&sa.sa_mask);

	sigaction(SIGINT,  &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
    }
#else
    signal(SIGINT,  sigterm);
    signal(SIGTERM, sigterm);
#endif

    pidfile(NULL);

    return doit (port);
}
