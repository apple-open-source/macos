/*
 * kdc/do_as_req.c
 *
 * Portions Copyright (C) 2007 Apple Inc.
 * Copyright 1990,1991,2007 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 *
 * KDC Routines to deal with AS_REQ's
 */

#include "k5-int.h"
#include "com_err.h"

#include <syslog.h>
#ifdef HAVE_NETINET_IN_H
#include <sys/types.h>
#include <netinet/in.h>
#ifndef hpux
#include <arpa/inet.h>
#endif	/* hpux */
#endif /* HAVE_NETINET_IN_H */

#include "kdc_util.h"
#include "policy.h"
#include "adm.h"
#include "adm_proto.h"
#include "extern.h"

#if APPLE_PKINIT
#define     AS_REQ_DEBUG    0
#if	    AS_REQ_DEBUG
#define     asReqDebug(args...)       printf(args)
#else
#define     asReqDebug(args...)
#endif
#endif /* APPLE_PKINIT */

#ifndef APPLE_KDC_MODS
#define APPLE_KDC_MODS
#endif
#ifdef APPLE_KDC_MODS
int kdc_update_pws( const char *in_user_principle, int in_error, int inCheck, krb5_db_entry server );
#endif

static krb5_error_code prepare_error_as (krb5_kdc_req *, int, krb5_data *, 
					 krb5_data **, const char *);

/*ARGSUSED*/
krb5_error_code
process_as_req(krb5_kdc_req *request, krb5_data *req_pkt,
	       const krb5_fulladdr *from, krb5_data **response)
{
    krb5_db_entry client, server;
    krb5_kdc_rep reply;
    krb5_enc_kdc_rep_part reply_encpart;
    krb5_ticket ticket_reply;
    krb5_enc_tkt_part enc_tkt_reply;
    krb5_error_code errcode;
    int c_nprincs = 0, s_nprincs = 0;
    krb5_boolean more;
    krb5_timestamp kdc_time, authtime;
    krb5_keyblock session_key;
    krb5_keyblock encrypting_key;
    const char *status;
    krb5_key_data  *server_key, *client_key;
    krb5_enctype useenctype;
#ifdef	KRBCONF_KDC_MODIFIES_KDB
    krb5_boolean update_client = 0;
#endif	/* KRBCONF_KDC_MODIFIES_KDB */
    krb5_data e_data;
    register int i;
    krb5_timestamp until, rtime;
    char *cname = 0, *sname = 0;
    const char *fromstring = 0;
    char ktypestr[128];
    char rep_etypestr[128];
    char fromstringbuf[70];
    void *pa_context = NULL;

#if APPLE_PKINIT
    asReqDebug("process_as_req top realm %s name %s\n", 
	request->client->realm.data, request->client->data->data);
#endif /* APPLE_PKINIT */

    ticket_reply.enc_part.ciphertext.data = 0;
    e_data.data = 0;
    encrypting_key.contents = 0;
    memset(&reply, 0, sizeof(reply));
    session_key.contents = 0;
    enc_tkt_reply.authorization_data = NULL;

    ktypes2str(ktypestr, sizeof(ktypestr),
	       request->nktypes, request->ktype);

    fromstring = inet_ntop(ADDRTYPE2FAMILY (from->address->addrtype),
			   from->address->contents,
			   fromstringbuf, sizeof(fromstringbuf));
    if (!fromstring)
	fromstring = "<unknown>";

    if (!request->client) {
	status = "NULL_CLIENT";
	errcode = KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN;
	goto errout;
    }
    if ((errcode = krb5_unparse_name(kdc_context, request->client, &cname))) {
	status = "UNPARSING_CLIENT";
	goto errout;
    }
    limit_string(cname);
    if (!request->server) {
	status = "NULL_SERVER";
	errcode = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
	goto errout;
    }
    if ((errcode = krb5_unparse_name(kdc_context, request->server, &sname))) {
	status = "UNPARSING_SERVER";
	goto errout;
    }
    limit_string(sname);
    
    c_nprincs = 1;
    if ((errcode = get_principal(kdc_context, request->client,
				 &client, &c_nprincs, &more))) {
	status = "LOOKING_UP_CLIENT";
	c_nprincs = 0;
	goto errout;
    }
    if (more) {
	status = "NON-UNIQUE_CLIENT";
	errcode = KRB5KDC_ERR_PRINCIPAL_NOT_UNIQUE;
	goto errout;
    } else if (c_nprincs != 1) {
	status = "CLIENT_NOT_FOUND";
#ifdef KRBCONF_VAGUE_ERRORS
	errcode = KRB5KRB_ERR_GENERIC;
#else
	errcode = KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN;
#endif
	goto errout;
    }
    
    s_nprincs = 1;
    if ((errcode = get_principal(kdc_context, request->server, &server,
				 &s_nprincs, &more))) {
	status = "LOOKING_UP_SERVER";
	goto errout;
    }
    if (more) {
	status = "NON-UNIQUE_SERVER";
	errcode = KRB5KDC_ERR_PRINCIPAL_NOT_UNIQUE;
	goto errout;
    } else if (s_nprincs != 1) {
	status = "SERVER_NOT_FOUND";
	errcode = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
	goto errout;
    }

    if ((errcode = krb5_timeofday(kdc_context, &kdc_time))) {
	status = "TIMEOFDAY";
	goto errout;
    }

    if ((errcode = validate_as_request(request, client, server,
				      kdc_time, &status))) {
	if (!status) 
	    status = "UNKNOWN_REASON";
	errcode += ERROR_TABLE_BASE_krb5;
	goto errout;
    }
      
    /*
     * Select the keytype for the ticket session key.
     */
    if ((useenctype = select_session_keytype(kdc_context, &server,
					     request->nktypes,
					     request->ktype)) == 0) {
	/* unsupported ktype */
	status = "BAD_ENCRYPTION_TYPE";
	errcode = KRB5KDC_ERR_ETYPE_NOSUPP;
	goto errout;
    }

    if ((errcode = krb5_c_make_random_key(kdc_context, useenctype,
					  &session_key))) {
	status = "RANDOM_KEY_FAILED";
	goto errout;
    }

    ticket_reply.server = request->server;

    enc_tkt_reply.flags = 0;
    setflag(enc_tkt_reply.flags, TKT_FLG_INITIAL);

    	/* It should be noted that local policy may affect the  */
        /* processing of any of these flags.  For example, some */
        /* realms may refuse to issue renewable tickets         */

    if (isflagset(request->kdc_options, KDC_OPT_FORWARDABLE))
	setflag(enc_tkt_reply.flags, TKT_FLG_FORWARDABLE);

    if (isflagset(request->kdc_options, KDC_OPT_PROXIABLE))
	    setflag(enc_tkt_reply.flags, TKT_FLG_PROXIABLE);

    if (isflagset(request->kdc_options, KDC_OPT_ALLOW_POSTDATE))
	    setflag(enc_tkt_reply.flags, TKT_FLG_MAY_POSTDATE);

    enc_tkt_reply.session = &session_key;
    enc_tkt_reply.client = request->client;
    enc_tkt_reply.transited.tr_type = KRB5_DOMAIN_X500_COMPRESS;
    enc_tkt_reply.transited.tr_contents = empty_string; /* equivalent of "" */

    enc_tkt_reply.times.authtime = kdc_time;

    if (isflagset(request->kdc_options, KDC_OPT_POSTDATED)) {
	setflag(enc_tkt_reply.flags, TKT_FLG_POSTDATED);
	setflag(enc_tkt_reply.flags, TKT_FLG_INVALID);
	enc_tkt_reply.times.starttime = request->from;
    } else
	enc_tkt_reply.times.starttime = kdc_time;
    
    until = (request->till == 0) ? kdc_infinity : request->till;

    enc_tkt_reply.times.endtime =
	min(until,
	    min(enc_tkt_reply.times.starttime + client.max_life,
		min(enc_tkt_reply.times.starttime + server.max_life,
		    enc_tkt_reply.times.starttime + max_life_for_realm)));

    if (isflagset(request->kdc_options, KDC_OPT_RENEWABLE_OK) &&
	!isflagset(client.attributes, KRB5_KDB_DISALLOW_RENEWABLE) &&
	(enc_tkt_reply.times.endtime < request->till)) {

	/* we set the RENEWABLE option for later processing */

	setflag(request->kdc_options, KDC_OPT_RENEWABLE);
	request->rtime = request->till;
    }
    rtime = (request->rtime == 0) ? kdc_infinity : request->rtime;

    if (isflagset(request->kdc_options, KDC_OPT_RENEWABLE)) {
	/*
	 * XXX Should we squelch the output renew_till to be no
	 * earlier than the endtime of the ticket? 
	 */
	setflag(enc_tkt_reply.flags, TKT_FLG_RENEWABLE);
	enc_tkt_reply.times.renew_till =
	    min(rtime, enc_tkt_reply.times.starttime +
		       min(client.max_renewable_life,
			   min(server.max_renewable_life,
			       max_renewable_life_for_realm)));
    } else
	enc_tkt_reply.times.renew_till = 0; /* XXX */

    /* starttime is optional, and treated as authtime if not present.
       so we can nuke it if it matches */
    if (enc_tkt_reply.times.starttime == enc_tkt_reply.times.authtime)
	enc_tkt_reply.times.starttime = 0;

    enc_tkt_reply.caddrs = request->addresses;
    enc_tkt_reply.authorization_data = 0;

    /* 
     * Check the preauthentication if it is there.
     */
    if (request->padata) {
	errcode = check_padata(kdc_context, &client, req_pkt, request,
			       &enc_tkt_reply, &pa_context, &e_data);
	if (errcode) {
#ifdef KRBCONF_KDC_MODIFIES_KDB
	    /*
	     * Note: this doesn't work if you're using slave servers!!!
	     * It also causes the database to be modified (and thus
	     * need to be locked) frequently.
	     */
	    if (client.fail_auth_count < KRB5_MAX_FAIL_COUNT) {
		client.fail_auth_count = client.fail_auth_count + 1;
		if (client.fail_auth_count == KRB5_MAX_FAIL_COUNT) { 
		    client.attributes |= KRB5_KDB_DISALLOW_ALL_TIX;
		}
	    }
	    client.last_failed = kdc_time;
	    update_client = 1;
#endif
	    status = "PREAUTH_FAILED";
#ifdef KRBCONF_VAGUE_ERRORS
	    errcode = KRB5KRB_ERR_GENERIC;
#endif
	    goto errout;
	} 
    }

    /*
     * Final check before handing out ticket: If the client requires
     * preauthentication, verify that the proper kind of
     * preauthentication was carried out.
     */
    status = missing_required_preauth(&client, &server, &enc_tkt_reply);
    if (status) {
	errcode = KRB5KDC_ERR_PREAUTH_REQUIRED;
	get_preauth_hint_list(request, &client, &server, &e_data);
	goto errout;
    }

    errcode = handle_authdata(kdc_context, &client, req_pkt, request, &enc_tkt_reply);
    if (errcode) {
	krb5_klog_syslog(LOG_INFO,  "AS_REQ : handle_authdata (%d)", errcode);
    }

    ticket_reply.enc_part2 = &enc_tkt_reply;

    /*
     * Find the server key
     */
    if ((errcode = krb5_dbe_find_enctype(kdc_context, &server,
					 -1, /* ignore keytype */
					 -1,		/* Ignore salttype */
					 0,		/* Get highest kvno */
					 &server_key))) {
	status = "FINDING_SERVER_KEY";
	goto errout;
    }

    /* convert server.key into a real key (it may be encrypted
       in the database) */
    if ((errcode = krb5_dbekd_decrypt_key_data(kdc_context, &master_keyblock, 
					       server_key, &encrypting_key,
					       NULL))) {
	status = "DECRYPT_SERVER_KEY";
	goto errout;
    }
	
    errcode = krb5_encrypt_tkt_part(kdc_context, &encrypting_key, &ticket_reply);
    krb5_free_keyblock_contents(kdc_context, &encrypting_key);
    encrypting_key.contents = 0;
    if (errcode) {
	status = "ENCRYPTING_TICKET";
	goto errout;
    }
    ticket_reply.enc_part.kvno = server_key->key_data_kvno;

    /*
     * Find the appropriate client key.  We search in the order specified
     * by request keytype list.
     */
    client_key = (krb5_key_data *) NULL;
    for (i = 0; i < request->nktypes; i++) {
	useenctype = request->ktype[i];
	if (!krb5_c_valid_enctype(useenctype))
	    continue;

	if (!krb5_dbe_find_enctype(kdc_context, &client, useenctype, -1,
				   0, &client_key))
	    break;
    }
    if (!(client_key)) {
	/* Cannot find an appropriate key */
	status = "CANT_FIND_CLIENT_KEY";
	errcode = KRB5KDC_ERR_ETYPE_NOSUPP;
	goto errout;
    }

    /* convert client.key_data into a real key */
    if ((errcode = krb5_dbekd_decrypt_key_data(kdc_context, &master_keyblock, 
					       client_key, &encrypting_key,
					       NULL))) {
	status = "DECRYPT_CLIENT_KEY";
	goto errout;
    }
    encrypting_key.enctype = useenctype;

    /* Start assembling the response */
    reply.msg_type = KRB5_AS_REP;
    reply.client = request->client;
    reply.ticket = &ticket_reply;
    reply_encpart.session = &session_key;
    if ((errcode = fetch_last_req_info(&client, &reply_encpart.last_req))) {
	status = "FETCH_LAST_REQ";
	goto errout;
    }
    reply_encpart.nonce = request->nonce;
    reply_encpart.key_exp = client.expiration;
    reply_encpart.flags = enc_tkt_reply.flags;
    reply_encpart.server = ticket_reply.server;

    /* copy the time fields EXCEPT for authtime; it's location
       is used for ktime */
    reply_encpart.times = enc_tkt_reply.times;
    reply_encpart.times.authtime = authtime = kdc_time;

    reply_encpart.caddrs = enc_tkt_reply.caddrs;

    /* Fetch the padata info to be returned */
    errcode = return_padata(kdc_context, &client, req_pkt, request,
			    &reply, client_key, &encrypting_key, &pa_context);
    if (errcode) {
	status = "KDC_RETURN_PADATA";
	goto errout;
    }

#if APPLE_PKINIT
    asReqDebug("process_as_req reply realm %s name %s\n", 
	reply.client->realm.data, reply.client->data->data);
#endif /* APPLE_PKINIT */

    /* now encode/encrypt the response */

    reply.enc_part.enctype = encrypting_key.enctype;

    errcode = krb5_encode_kdc_rep(kdc_context, KRB5_AS_REP, &reply_encpart, 
				  0, &encrypting_key,  &reply, response);
    krb5_free_keyblock_contents(kdc_context, &encrypting_key);
    encrypting_key.contents = 0;
    reply.enc_part.kvno = client_key->key_data_kvno;

    if (errcode) {
	status = "ENCODE_KDC_REP";
	goto errout;
    }
    
#ifdef APPLE_KDC_MODS
    /*  Add code that checks the pws for account validity LAW */
    if(kdc_notify_pws_apple || kdc_active_realm->realm_pws_enabled){
    	errcode = kdc_update_pws(cname, 0, 1, server);
    	if (errcode) {
	    krb5_free_data(kdc_context, *response);
	    *response = NULL;
	    status = "CHECK_PWS_ACCT";
	    goto errout;
    	}
    }
#endif
    	
   
    rep_etypes2str(rep_etypestr, sizeof(rep_etypestr), &reply);
    krb5_klog_syslog(LOG_INFO,
		     "AS_REQ (%s) %s: ISSUE: authtime %d, "
		     "%s, %s for %s",
		     ktypestr,
	             fromstring, authtime,
		     rep_etypestr,
		     cname, sname);

#ifdef	KRBCONF_KDC_MODIFIES_KDB
    /*
     * If we get this far, we successfully did the AS_REQ.
     */
    client.last_success = kdc_time;
    client.fail_auth_count = 0;
    update_client = 1;
#endif	/* KRBCONF_KDC_MODIFIES_KDB */

errout:

    /* these parts are left on as a courtesy from krb5_encode_kdc_rep so we
       can use them in raw form if needed.  But, we don't... */
    memset(reply.enc_part.ciphertext.data, 0, reply.enc_part.ciphertext.length);
    free(reply.enc_part.ciphertext.data);

#ifdef APPLE_KDC_MODS
    /* call code to notify PWS of the failure iff not NEEDS_Preauth   */
    if(kdc_notify_pws_apple || kdc_active_realm->realm_pws_enabled){
      if((errcode != KRB5KDC_ERR_PREAUTH_REQUIRED ) && (errcode != KRB_AP_ERR_METHOD))
	kdc_update_pws(cname, errcode, 0, server);
    }
#endif

    if (pa_context)
	free_padata_context(kdc_context, &pa_context);

    if (status) {
	const char * emsg = 0;
	if (errcode) 
	    emsg = krb5_get_error_message (kdc_context, errcode);

        krb5_klog_syslog(LOG_INFO, "AS_REQ (%s) %s: %s: %s for %s%s%s",
			 ktypestr,
	       fromstring, status, 
	       cname ? cname : "<unknown client>",
	       sname ? sname : "<unknown server>",
	       errcode ? ", " : "",
	       errcode ? emsg : "");
	if (errcode)
	    krb5_free_error_message (kdc_context, emsg);
    }
    if (errcode) {
        int got_err = 0;
	if (status == 0) {
	    status = krb5_get_error_message (kdc_context, errcode);
	    got_err = 1;
	}
	errcode -= ERROR_TABLE_BASE_krb5;
	if (errcode < 0 || errcode > 128)
	    errcode = KRB_ERR_GENERIC;
	    
	errcode = prepare_error_as(request, errcode, &e_data, response,
				   status);
	if (got_err) {
	    krb5_free_error_message (kdc_context, status);
	    status = 0;
	}
    }

    if (enc_tkt_reply.authorization_data != NULL)
	krb5_free_authdata(kdc_context, enc_tkt_reply.authorization_data);
    if (encrypting_key.contents)
	krb5_free_keyblock_contents(kdc_context, &encrypting_key);
    if (reply.padata)
	krb5_free_pa_data(kdc_context, reply.padata);

    if (cname)
	    free(cname);
    if (sname)
	    free(sname);
    if (c_nprincs) {
#ifdef	KRBCONF_KDC_MODIFIES_KDB
	if (update_client) {
	    krb5_db_put_principal(kdc_context, &client, &c_nprincs);
	    /*
	     * ptooey.  We want krb5_db_sync() or something like that.
	     */
	    krb5_db_fini(kdc_context);
	    if (kdc_active_realm->realm_dbname)
		krb5_db_set_name(kdc_active_realm->realm_context,
				 kdc_active_realm->realm_dbname);
	    krb5_db_init(kdc_context);
	    /* Reset master key */
	    krb5_db_set_mkey(kdc_context, &kdc_active_realm->realm_mkey);
	}
#endif	/* KRBCONF_KDC_MODIFIES_KDB */
	krb5_db_free_principal(kdc_context, &client, c_nprincs);
    }
    if (s_nprincs)
	krb5_db_free_principal(kdc_context, &server, s_nprincs);
    if (session_key.contents)
	krb5_free_keyblock_contents(kdc_context, &session_key);
    if (ticket_reply.enc_part.ciphertext.data) {
	memset(ticket_reply.enc_part.ciphertext.data , 0,
	       ticket_reply.enc_part.ciphertext.length);
	free(ticket_reply.enc_part.ciphertext.data);
    }

    krb5_free_data_contents(kdc_context, &e_data);
    
    return errcode;
}

static krb5_error_code
prepare_error_as (krb5_kdc_req *request, int error, krb5_data *e_data,
		  krb5_data **response, const char *status)
{
    krb5_error errpkt;
    krb5_error_code retval;
    krb5_data *scratch;
    
    errpkt.ctime = request->nonce;
    errpkt.cusec = 0;

    if ((retval = krb5_us_timeofday(kdc_context, &errpkt.stime,
				    &errpkt.susec)))
	return(retval);
    errpkt.error = error;
    errpkt.server = request->server;
    errpkt.client = request->client;
    errpkt.text.length = strlen(status)+1;
    if (!(errpkt.text.data = strdup(status)))
	return ENOMEM;

    if (!(scratch = (krb5_data *)malloc(sizeof(*scratch)))) {
	free(errpkt.text.data);
	return ENOMEM;
    }
    if (e_data && e_data->data) {
	errpkt.e_data = *e_data;
    } else {
	errpkt.e_data.length = 0;
	errpkt.e_data.data = 0;
    }

    retval = krb5_mk_error(kdc_context, &errpkt, scratch);
    free(errpkt.text.data);
    if (retval)
	free(scratch);
    else 
	*response = scratch;

    return retval;
}

#ifdef APPLE_KDC_MODS
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "adm.h"
#include "adm_proto.h"
#include "kdc_util.h"

/* 
 * there may be value in copying the source for readFromServer &
 * writeToServer into this file and removing the link dependency on the
 * password server framework.
 */

//#include <PasswordServer/CPSUtilities.h>

typedef enum PWServerErrorType {
	kPolicyError,
	kSASLError,
	kConnectionError
} PWServerErrorType;

typedef struct PWServerError {
    int err;
    PWServerErrorType type;
} PWServerError;

void 			writeToServer( int fd, char *buf );
PWServerError 	readFromServer( int fd, char *buf, unsigned long bufLen );
PWServerError 	readFromServerGetData( int fd, char *buf, unsigned long bufLen, unsigned long *outByteCount );
PWServerError 	readFromServerGetLine( int fd, char *buf, unsigned long bufLen, int inCanReadMore, unsigned long *inOutByteCount );
PWServerError 	readFromServerGetErrorCode( char *buf );

extern int errno;

#define noErr   0

int kdc_contact_pws(void);
int kdc_update_pws( const char *inPrinciple, int inError, int inCheck, krb5_db_entry server );

#define kUpdateAuthMethod   "KERBEROS-LOGIN-CHECK"
#define kPWSPort			106

// password server error strings
#define kPasswordServerErrPrefixStr			"-ERR "
#define kPasswordServerAuthErrPrefixStr		"-AUTHERR "
#define kPasswordServerSASLErrPrefixStr		"SASL "

// Reposonse Codes (used numerically)
enum {
    kAuthOK = 0,
    kAuthFail = -1,
    kAuthUserDisabled = -2,
    kAuthNeedAdminPrivs = -3,
    kAuthUserNotSet = -4,
    kAuthUserNotAuthenticated = -5,
    kAuthPasswordExpired = -6,
    kAuthPasswordNeedsChange = -7,
    kAuthPasswordNotChangeable = -8,
    kAuthPasswordTooShort = -9,
    kAuthPasswordTooLong = -10,
    kAuthPasswordNeedsAlpha = -11,
    kAuthPasswordNeedsDecimal = -12,
    kAuthMethodTooWeak = -13,
	kAuthPasswordNeedsMixedCase = -14,
	kAuthPasswordHasGuessablePattern = -15
};

int kdc_update_pws( const char *inPrinciple, int inError, int inCheck, krb5_db_entry server )
{
	char commandBuf[4096];
	char replyBuf[4096];
	int pwsFD = kdc_contact_pws();
	int reply = -1;
	PWServerError   pwsReply;
	
	if(pwsFD == -1)
	{
		return errno;
	}
	
	// discard the greeting message
	readFromServer(pwsFD, replyBuf, sizeof(replyBuf));
	
	// build the initial request 
	if(inCheck == 1)
	{
		snprintf(commandBuf, sizeof(commandBuf), "AUTH %s %s ?\r\n", kUpdateAuthMethod, inPrinciple);
	} else {
		snprintf(commandBuf, sizeof(commandBuf), "AUTH %s %s %c\r\n", kUpdateAuthMethod, inPrinciple, (inError == noErr) ? '+' : '-');
	}
	// send it to the server
	 writeToServer(pwsFD, commandBuf);

	// get the reply
	pwsReply = readFromServer(pwsFD, replyBuf, sizeof(replyBuf));


	switch (pwsReply.err) {
	case noErr:
	case kAuthUserNotSet:
		reply = 0;
		break;
	case kAuthPasswordNeedsChange:
		if (isflagset(server.attributes, KRB5_KDB_PWCHANGE_SERVICE))
			reply = 0;
		else
			reply = KRB5KDC_ERR_POLICY;
		break;
	default:
		reply = KRB5KDC_ERR_POLICY;
	}

	snprintf(commandBuf, sizeof(commandBuf), "QUIT\r\n");

	// send it to the server
	writeToServer(pwsFD, commandBuf);

	// get the reply
	pwsReply = readFromServer(pwsFD, replyBuf, sizeof(replyBuf));
	
	// clean up
	close(pwsFD);
	
	return reply;
}

/* returns the socket of the local pws or -1 & sets errno   */
int kdc_contact_pws(void)
{
	int pws_socket;
	struct sockaddr_un  addr;
	struct timeval sendTimeoutVal = { 3 , 0 };  // three seconds
	
	addr.sun_family = AF_UNIX;
	addr.sun_len = sizeof(struct sockaddr_un);
	strcpy(addr.sun_path, "/var/run/passwordserver");

	pws_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	
	// set a timeout on the socket
	// SO_SNDTIMEO - send timeout

    if(setsockopt(pws_socket, SOL_SOCKET, SO_SNDTIMEO, &sendTimeoutVal, sizeof(sendTimeoutVal)) == -1)
	{
		return -1;	 
	}

	if(connect(pws_socket, (const struct sockaddr *)&addr, sizeof(addr)) == -1)
		return -1;
	else
		return pws_socket;
}

void writeToServer( int fd, char *buf )
{
    if ( buf != NULL ) {
	krb5_net_write (kdc_context, fd, buf, strlen(buf));
    }
}

PWServerError readFromServer( int fd, char *buf, unsigned long bufLen )
{
    PWServerError result;
	unsigned long byteCount;
	
	result = readFromServerGetData( fd, buf, bufLen, &byteCount );
	if ( result.err == 0 )
		result = readFromServerGetLine( fd, buf, bufLen, 1, &byteCount );
	if ( result.err == 0 )
		result = readFromServerGetErrorCode( buf );
	    
    return result;
}

PWServerError readFromServerGetData( int fd, char *buf, unsigned long bufLen, unsigned long *outByteCount )
{
    char readChar = '\0';
    PWServerError result = {0, kPolicyError};
	ssize_t byteCount = 0;
	
	if ( buf == NULL || bufLen < 3 ) {
        result.err = -1;
        return result;
    }
    
	if ( outByteCount != NULL )
		*outByteCount = 0;
	
	buf[0] = '\0';
	
	// wait for the first character to arrive
	byteCount = recvfrom( fd, &readChar, sizeof(readChar), (MSG_WAITALL | MSG_PEEK), NULL, NULL );
	if ( byteCount == 0 || byteCount == -1 )
	{
		result.err = -1;
		result.type = kConnectionError;
		return result;
	}
	
	// peek the buffer to get the length
	byteCount = recvfrom( fd, buf, bufLen - 1, (MSG_DONTWAIT | MSG_PEEK), NULL, NULL );
	//DEBUGLOG( "byteCount (peek): %d", (int)byteCount);
	
	if ( outByteCount != NULL )
		*outByteCount = byteCount;
	
    return result;
}


PWServerError readFromServerGetLine( int fd, char *buf, unsigned long bufLen, int inCanReadMore, unsigned long *inOutByteCount )
{
    char readChar = '\0';
    char *tstr = NULL;
	char *consumeBuf;
    PWServerError result = {0, kPolicyError};
	size_t byteCount = *inOutByteCount;
	size_t consumeLen;
	
	if ( buf == NULL || bufLen < 3 ) {
        result.err = -1;
        return result;
    }
    
	// pull to EOL or available data
	if ( byteCount >= 2 )
	{
		if ( inCanReadMore )
		{
			buf[byteCount] = '\0';
			tstr = strstr( buf, "\r\n" );
		}
		consumeLen = (tstr != NULL) ? (tstr - buf + 2) : byteCount;
		consumeBuf = (char *) malloc( consumeLen );
		if ( consumeBuf == NULL ) {
			result.err = -1;
			return result;
		}
		byteCount = recvfrom( fd, consumeBuf, consumeLen, MSG_DONTWAIT, NULL, NULL );
		free( consumeBuf );
		if ( inOutByteCount != NULL )
			*inOutByteCount = byteCount;
		buf[byteCount] = '\0';
	}
	
	// if not at EOL, pull by character until one arrives 
	if ( inCanReadMore && tstr == NULL && byteCount < (ssize_t)bufLen - 1 )
	{
		tstr = buf + byteCount;
		do
		{
			byteCount = recvfrom( fd, &readChar, sizeof(readChar), MSG_WAITALL, NULL, NULL );
			if ( byteCount == 0 || byteCount == -1 )
			{
				*tstr = '\0';
				result.err = -1;
				result.type = kConnectionError;
				return result;
			}
			
			if ( (unsigned long)(tstr - buf) < bufLen - 1 )
				*tstr++ = readChar;
			
			if ( inOutByteCount != NULL )
				(*inOutByteCount)++;
		}
		while ( readChar != '\n' );
		*tstr = '\0';
	}
		
    return result;
}


PWServerError readFromServerGetErrorCode( char *buf )
{
    char *tstr = NULL;
    PWServerError result = {0, kPolicyError};
	int compareLen;

	tstr = buf;
    compareLen = strlen(kPasswordServerErrPrefixStr);
    if ( strncmp( tstr, kPasswordServerErrPrefixStr, compareLen ) == 0 )
    {
        tstr += compareLen;
        
        // check if err is a PasswordServer or SASL error
        compareLen = strlen(kPasswordServerSASLErrPrefixStr);
        if ( strncmp(tstr, kPasswordServerSASLErrPrefixStr, compareLen) == 0 ) {
            tstr += compareLen;
            result.type = kSASLError;
        }
        
        sscanf( tstr, "%d", &result.err );
		if ( result.err == 0 )
			result.err = -1;
    }
    else
	{
		compareLen = strlen(kPasswordServerAuthErrPrefixStr);
		if ( strncmp( tstr, kPasswordServerAuthErrPrefixStr, compareLen ) == 0 )
		{
			tstr += compareLen;
			sscanf( tstr, "%d", &result.err );
			if ( result.err == 0 )
				result.err = -1;
		}
	}
	
    return result;
}

#endif /* APPLE_KDC_MODS */
