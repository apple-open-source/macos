/*
 * pop3.c -- POP3 protocol methods
 *
 * Copyright 1998 by Eric S. Raymond.
 * For license terms, see the file COPYING in this directory.
 */

#include  "config.h"
#ifdef POP3_ENABLE
#include  <stdio.h>
#include  <string.h>
#include  <ctype.h>
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined(STDC_HEADERS)
#include  <stdlib.h>
#endif
#include  <errno.h>

#include  "fetchmail.h"
#include  "socket.h"
#include  "i18n.h"

#ifdef OPIE_ENABLE
#ifdef __cplusplus
extern "C" {
#endif
#include <opie.h>
#ifdef __cplusplus
}
#endif
#endif /* OPIE_ENABLE */

/* global variables: please reinitialize them explicitly for proper
 * working in daemon mode */

/* TODO: session variables to be initialized before server greeting */
#ifdef OPIE_ENABLE
static char lastok[POPBUFSIZE+1];
#endif /* OPIE_ENABLE */

/* session variables initialized in capa_probe() or pop3_getauth() */
flag done_capa = FALSE;
#if defined(GSSAPI)
flag has_gssapi = FALSE;
#endif /* defined(GSSAPI) */
#if defined(KERBEROS_V4) || defined(KERBEROS_V5)
flag has_kerberos = FALSE;
#endif /* defined(KERBEROS_V4) || defined(KERBEROS_V5) */
static flag has_cram = FALSE;
#ifdef OPIE_ENABLE
flag has_otp = FALSE;
#endif /* OPIE_ENABLE */
#ifdef NTLM_ENABLE
flag has_ntlm = FALSE;
#endif /* NTLM_ENABLE */
#ifdef SSL_ENABLE
static flag has_stls = FALSE;
#endif /* SSL_ENABLE */

/* mailbox variables initialized in pop3_getrange() */
static int last;

/* mail variables initialized in pop3_fetch() */
#ifdef SDPS_ENABLE
char *sdps_envfrom;
char *sdps_envto;
#endif /* SDPS_ENABLE */

#ifdef NTLM_ENABLE
#include "ntlm.h"

/*
 * NTLM support by Grant Edwards.
 *
 * Handle MS-Exchange NTLM authentication method.  This is the same
 * as the NTLM auth used by Samba for SMB related services. We just
 * encode the packets in base64 instead of sending them out via a
 * network interface.
 * 
 * Much source (ntlm.h, smb*.c smb*.h) was borrowed from Samba.
 */

static int do_pop3_ntlm(int sock, struct query *ctl,
	int msn_instead /** if true, send AUTH MSN, else send AUTH NTLM */)
{
    char msgbuf[POPBUFSIZE+1];
    int result;

    gen_send(sock, msn_instead ? "AUTH MSN" : "AUTH NTLM");

    if ((result = ntlm_helper(sock, ctl, "POP3")))
	return result;

    if ((result = gen_recv (sock, msgbuf, sizeof msgbuf)))
	return result;

    if (strstr (msgbuf, "OK"))
	return PS_SUCCESS;
    else
	return PS_AUTHFAIL;
}
#endif /* NTLM */


#define DOTLINE(s)	(s[0] == '.' && (s[1]=='\r'||s[1]=='\n'||s[1]=='\0'))

static int pop3_ok (int sock, char *argbuf)
/* parse command response */
{
    int ok;
    char buf [POPBUFSIZE+1];
    char *bufp;

    if ((ok = gen_recv(sock, buf, sizeof(buf))) == 0)
    {	bufp = buf;
	if (*bufp == '+' || *bufp == '-')
	    bufp++;
	else
	    return(PS_PROTOCOL);

	while (isalpha((unsigned char)*bufp))
	    bufp++;

	if (*bufp)
	  *(bufp++) = '\0';

	if (strcmp(buf,"+OK") == 0)
	{
#ifdef OPIE_ENABLE
	    strcpy(lastok, bufp);
#endif /* OPIE_ENABLE */
	    ok = 0;
	}
	else if (strncmp(buf,"-ERR", 4) == 0)
	{
	    if (stage == STAGE_FETCH)
		ok = PS_TRANSIENT;
	    else if (stage > STAGE_GETAUTH)
		ok = PS_PROTOCOL;
	    /*
	     * We're checking for "lock busy", "unable to lock", 
	     * "already locked", "wait a few minutes" etc. here. 
	     * This indicates that we have to wait for the server to
	     * unwedge itself before we can poll again.
	     *
	     * PS_LOCKBUSY check empirically verified with two recent
	     * versions of the Berkeley popper; QPOP (version 2.2)  and
	     * QUALCOMM Pop server derived from UCB (version 2.1.4-R3)
	     * These are caught by the case-indifferent "lock" check.
	     * The "wait" catches "mail storage services unavailable,
	     * wait a few minutes and try again" on the InterMail server.
	     *
	     * If these aren't picked up on correctly, fetchmail will 
	     * think there is an authentication failure and wedge the
	     * connection in order to prevent futile polls.
	     *
	     * Gad, what a kluge.
	     */
	    else if (strstr(bufp,"lock")
		     || strstr(bufp,"Lock")
		     || strstr(bufp,"LOCK")
		     || strstr(bufp,"wait")
		     /* these are blessed by RFC 2449 */
		     || strstr(bufp,"[IN-USE]")||strstr(bufp,"[LOGIN-DELAY]"))
		ok = PS_LOCKBUSY;
	    else if ((strstr(bufp,"Service")
		     || strstr(bufp,"service"))
			 && (strstr(bufp,"unavailable")))
		ok = PS_SERVBUSY;
	    else
		ok = PS_AUTHFAIL;
	    /*
	     * We always want to pass the user lock-busy messages, because
	     * they're red flags.  Other stuff (like AUTH failures on non-
	     * RFC1734 servers) only if we're debugging.
	     */
	    if (*bufp && (ok == PS_LOCKBUSY || outlevel >= O_MONITOR))
	      report(stderr, "%s\n", bufp);
	}
	else
	    ok = PS_PROTOCOL;

#if POPBUFSIZE > MSGBUFSIZE
#error "POPBUFSIZE must not be larger than MSGBUFSIZE"
#endif
	if (argbuf != NULL)
	    strcpy(argbuf,bufp);
    }

    return(ok);
}



static int capa_probe(int sock)
/* probe the capabilities of the remote server */
{
    int	ok;

    if (done_capa) {
	return PS_SUCCESS;
    }
#if defined(GSSAPI)
    has_gssapi = FALSE;
#endif /* defined(GSSAPI) */
#if defined(KERBEROS_V4) || defined(KERBEROS_V5)
    has_kerberos = FALSE;
#endif /* defined(KERBEROS_V4) || defined(KERBEROS_V5) */
    has_cram = FALSE;
#ifdef OPIE_ENABLE
    has_otp = FALSE;
#endif /* OPIE_ENABLE */
#ifdef NTLM_ENABLE
    has_ntlm = FALSE;
#endif /* NTLM_ENABLE */

    ok = gen_transact(sock, "CAPA");
    if (ok == PS_SUCCESS)
    {
	char buffer[64];

	/* determine what authentication methods we have available */
	while ((ok = gen_recv(sock, buffer, sizeof(buffer))) == 0)
	{
	    if (DOTLINE(buffer))
		break;

#ifdef SSL_ENABLE
	    if (strstr(buffer, "STLS"))
		has_stls = TRUE;
#endif /* SSL_ENABLE */

#if defined(GSSAPI)
	    if (strstr(buffer, "GSSAPI"))
		has_gssapi = TRUE;
#endif /* defined(GSSAPI) */

#if defined(KERBEROS_V4)
	    if (strstr(buffer, "KERBEROS_V4"))
		has_kerberos = TRUE;
#endif /* defined(KERBEROS_V4)  */

#ifdef OPIE_ENABLE
	    if (strstr(buffer, "X-OTP"))
		has_otp = TRUE;
#endif /* OPIE_ENABLE */

#ifdef NTLM_ENABLE
	    if (strstr(buffer, "NTLM"))
		has_ntlm = TRUE;
#endif /* NTLM_ENABLE */

	    if (strstr(buffer, "CRAM-MD5"))
		has_cram = TRUE;
	}
    }
    done_capa = TRUE;
    return(ok);
}

static void set_peek_capable(struct query *ctl)
{
    /* we're peek-capable means that the use of TOP is enabled,
     * see pop3_fetch for details - short story, we can use TOP if
     * we have a means of reliably tracking which mail we need to
     * refetch should the connection abort in the middle.
     * fetchall forces RETR, as does keep without UIDL */
    peek_capable = !ctl->fetchall && (!ctl->keep || ctl->server.uidl);
}

static int pop3_getauth(int sock, struct query *ctl, char *greeting)
/* apply for connection authorization */
{
    int ok;
    char *start,*end;
    char *msg;
#ifdef OPIE_ENABLE
    char *challenge;
#endif /* OPIE_ENABLE */
#ifdef SSL_ENABLE
    flag connection_may_have_tls_errors = FALSE;
    flag got_tls = FALSE;
#endif /* SSL_ENABLE */

    done_capa = FALSE;
#if defined(GSSAPI)
    has_gssapi = FALSE;
#endif /* defined(GSSAPI) */
#if defined(KERBEROS_V4) || defined(KERBEROS_V5)
    has_kerberos = FALSE;
#endif /* defined(KERBEROS_V4) || defined(KERBEROS_V5) */
    has_cram = FALSE;
#ifdef OPIE_ENABLE
    has_otp = FALSE;
#endif /* OPIE_ENABLE */
#ifdef SSL_ENABLE
    has_stls = FALSE;
#endif /* SSL_ENABLE */

    /* Set this up before authentication quits early. */
    set_peek_capable(ctl);

    /* Hack: allow user to force RETR. */
    if (peek_capable && getenv("FETCHMAIL_POP3_FORCE_RETR")) {
	peek_capable = 0;
    }

    /*
     * The "Maillennium POP3/PROXY server" deliberately truncates
     * TOP replies after c. 64 or 80 kByte (we have varying reports), so
     * disable TOP. Comcast once spewed marketing babble to the extent
     * of protecting Outlook -- pretty overzealous to break a protocol
     * for that that Microsoft could have read, too. Comcast aren't
     * alone in using this software though.
     * <http://lists.ccil.org/pipermail/fetchmail-friends/2004-April/008523.html>
     * (Thanks to Ed Wilts for reminding me of that.)
     *
     * The warning is printed once per server, until fetchmail exits.
     * It will be suppressed when --fetchall or other circumstances make
     * us use RETR anyhow.
     *
     * Matthias Andree
     */
    if (peek_capable && strstr(greeting, "Maillennium POP3/PROXY server")) {
	if ((ctl->server.workarounds & WKA_TOP) == 0) {
	    report(stdout, GT_("Warning: \"Maillennium POP3/PROXY server\" found, using RETR command instead of TOP.\n"));
	    ctl->server.workarounds |= WKA_TOP;
	}
	peek_capable = 0;
    }
    if (ctl->server.authenticate == A_SSH) {
        return PS_SUCCESS;
    }

#ifdef SDPS_ENABLE
    /*
     * This needs to catch both demon.co.uk and demon.net.
     * If we see either, and we're in multidrop mode, try to use
     * the SDPS *ENV extension.
     */
    if (!(ctl->server.sdps) && MULTIDROP(ctl) && strstr(greeting, "demon."))
        ctl->server.sdps = TRUE;
#endif /* SDPS_ENABLE */

   switch (ctl->server.protocol) {
    case P_POP3:
#ifdef RPA_ENABLE
	/* XXX FIXME: AUTH probing (RFC1734) should become global */
	/* CompuServe POP3 Servers as of 990730 want AUTH first for RPA */
	if (strstr(ctl->remotename, "@compuserve.com"))
	{
	    /* AUTH command should return a list of available mechanisms */
	    if (gen_transact(sock, "AUTH") == 0)
	    {
		char buffer[10];
		flag has_rpa = FALSE;

		while ((ok = gen_recv(sock, buffer, sizeof(buffer))) == 0)
		{
		    if (DOTLINE(buffer))
			break;
		    if (strncasecmp(buffer, "rpa", 3) == 0)
			has_rpa = TRUE;
		}
		if (has_rpa && !POP3_auth_rpa(ctl->remotename, 
					      ctl->password, sock))
		    return(PS_SUCCESS);
	    }

	    return(PS_AUTHFAIL);
	}
#endif /* RPA_ENABLE */

	/*
	 * CAPA command may return a list including available
	 * authentication mechanisms and STLS capability.
	 *
	 * If it doesn't, no harm done, we just fall back to a plain
	 * login -- if the user allows it.
	 *
	 * Note that this code latches the server's authentication type,
	 * so that in daemon mode the CAPA check only needs to be done
	 * once at start of run.
	 *
	 * If CAPA fails, then force the authentication method to
	 * PASSWORD, switch off opportunistic and repoll immediately.
	 * If TLS is mandatory, fail up front.
	 */
	if ((ctl->server.authenticate == A_ANY) ||
		(ctl->server.authenticate == A_GSSAPI) ||
		(ctl->server.authenticate == A_KERBEROS_V4) ||
		(ctl->server.authenticate == A_KERBEROS_V5) ||
		(ctl->server.authenticate == A_OTP) ||
		(ctl->server.authenticate == A_CRAM_MD5) ||
		maybe_tls(ctl))
	{
	    if ((ok = capa_probe(sock)) != PS_SUCCESS)
		/* we are in STAGE_GETAUTH => failure is PS_AUTHFAIL! */
		if (ok == PS_AUTHFAIL ||
		    /* Some servers directly close the socket. However, if we
		     * have already authenticated before, then a previous CAPA
		     * must have succeeded. In that case, treat this as a
		     * genuine socket error and do not change the auth method.
		     */
		    (ok == PS_SOCKET && !ctl->wehaveauthed))
		{
#ifdef SSL_ENABLE
		    if (must_tls(ctl)) {
			/* fail with mandatory STLS without repoll */
			report(stderr, GT_("TLS is mandatory for this session, but server refused CAPA command.\n"));
			report(stderr, GT_("The CAPA command is however necessary for TLS.\n"));
			return ok;
		    } else if (maybe_tls(ctl)) {
			/* defeat opportunistic STLS */
			xfree(ctl->sslproto);
			ctl->sslproto = xstrdup("");
		    }
#endif
		    /* If strong authentication was opportunistic, retry without, else fail. */
		    switch (ctl->server.authenticate) {
			case A_ANY:
			    ctl->server.authenticate = A_PASSWORD;
			    /* FALLTHROUGH */
			case A_PASSWORD: /* this should only happen with TLS enabled */
			    return PS_REPOLL;
			default:
			    return PS_AUTHFAIL;
		    }
		}
	}

#ifdef SSL_ENABLE
	if (maybe_tls(ctl)) {
	    char *commonname;

	    commonname = ctl->server.pollname;
	    if (ctl->server.via)
		commonname = ctl->server.via;
	    if (ctl->sslcommonname)
		commonname = ctl->sslcommonname;

	   if (has_stls)
	   {
	       /* Use "tls1" rather than ctl->sslproto because tls1 is the only
		* protocol that will work with STARTTLS.  Don't need to worry
		* whether TLS is mandatory or opportunistic unless SSLOpen() fails
		* (see below). */
	       if (gen_transact(sock, "STLS") == PS_SUCCESS
		       && SSLOpen(sock, ctl->sslcert, ctl->sslkey, "tls1", ctl->sslcertck,
			   ctl->sslcertfile, ctl->sslcertpath, ctl->sslfingerprint, commonname,
			   ctl->server.pollname, &ctl->remotename) != -1)
	       {
		   /*
		    * RFC 2595 says this:
		    *
		    * "Once TLS has been started, the client MUST discard cached
		    * information about server capabilities and SHOULD re-issue the
		    * CAPABILITY command.  This is necessary to protect against
		    * man-in-the-middle attacks which alter the capabilities list prior
		    * to STARTTLS.  The server MAY advertise different capabilities
		    * after STARTTLS."
		    *
		    * Now that we're confident in our TLS connection we can
		    * guarantee a secure capability re-probe.
		    */
		   got_tls = TRUE;
		   done_capa = FALSE;
		   ok = capa_probe(sock);
		   if (ok != PS_SUCCESS) {
		       return ok;
		   }
		   if (outlevel >= O_VERBOSE)
		   {
		       report(stdout, GT_("%s: upgrade to TLS succeeded.\n"), commonname);
		   }
	       }
	   }

	   if (!got_tls) {
	       if (must_tls(ctl)) {
		   /* Config required TLS but we couldn't guarantee it, so we must
		    * stop. */
		   report(stderr, GT_("%s: upgrade to TLS failed.\n"), commonname);
		   return PS_SOCKET;
	       } else {
		   /* We don't know whether the connection is usable, and there's
		    * no command we can reasonably issue to test it (NOOP isn't
		    * allowed til post-authentication), so leave it in an unknown
		    * state, mark it as such, and check more carefully if things
		    * go wrong when we try to authenticate. */
		   connection_may_have_tls_errors = TRUE;
		   if (outlevel >= O_VERBOSE)
		   {
		       report(stdout, GT_("%s: opportunistic upgrade to TLS failed, trying to continue.\n"), commonname);
		   }
	       }
	   }
	} /* maybe_tls() */
#endif /* SSL_ENABLE */

	/*
	 * OK, we have an authentication type now.
	 */
#if defined(KERBEROS_V4)
	/* 
	 * Servers doing KPOP have to go through a dummy login sequence
	 * rather than doing SASL.
	 */
	if (has_kerberos &&
	    ctl->server.service && (strcmp(ctl->server.service, KPOP_PORT)!=0)
	    && (ctl->server.authenticate == A_KERBEROS_V4
	     || ctl->server.authenticate == A_KERBEROS_V5
	     || ctl->server.authenticate == A_ANY))
	{
	    ok = do_rfc1731(sock, "AUTH", ctl->server.truename);
	    if (ok == PS_SUCCESS || ctl->server.authenticate != A_ANY)
		break;
	}
#endif /* defined(KERBEROS_V4) || defined(KERBEROS_V5) */

#if defined(GSSAPI)
	if (has_gssapi &&
	    (ctl->server.authenticate == A_GSSAPI ||
	    (ctl->server.authenticate == A_ANY
	     && check_gss_creds("pop", ctl->server.truename) == PS_SUCCESS)))
	{
	    ok = do_gssauth(sock,"AUTH","pop",ctl->server.truename,ctl->remotename);
	    if (ok == PS_SUCCESS || ctl->server.authenticate != A_ANY)
		break;
	}
#endif /* defined(GSSAPI) */

#ifdef OPIE_ENABLE
	if (has_otp &&
	    (ctl->server.authenticate == A_OTP ||
	     ctl->server.authenticate == A_ANY))
	{
	    ok = do_otp(sock, "AUTH", ctl);
	    if (ok == PS_SUCCESS || ctl->server.authenticate != A_ANY)
		break;
	}
#endif /* OPIE_ENABLE */

#ifdef NTLM_ENABLE
    /* MSN servers require the use of NTLM (MSN) authentication */
    if (!strcasecmp(ctl->server.pollname, "pop3.email.msn.com") ||
	    ctl->server.authenticate == A_MSN)
	return (do_pop3_ntlm(sock, ctl, 1) == 0) ? PS_SUCCESS : PS_AUTHFAIL;
    if (ctl->server.authenticate == A_NTLM || (has_ntlm && ctl->server.authenticate == A_ANY)) {
	ok = do_pop3_ntlm(sock, ctl, 0);
        if (ok == 0 || ctl->server.authenticate != A_ANY)
	    break;
    }
#else
    if (ctl->server.authenticate == A_NTLM || ctl->server.authenticate == A_MSN)
    {
	report(stderr,
	   GT_("Required NTLM capability not compiled into fetchmail\n"));
    }
#endif

 	if (ctl->server.authenticate == A_CRAM_MD5 || 
	    (has_cram && ctl->server.authenticate == A_ANY))
	{
	    ok = do_cram_md5(sock, "AUTH", ctl, NULL);
	    if (ok == PS_SUCCESS || ctl->server.authenticate != A_ANY)
		break;
	}

	/* ordinary validation, no one-time password or RPA */ 
	if ((ok = gen_transact(sock, "USER %s", ctl->remotename)))
	    break;

#ifdef OPIE_ENABLE
	/* see RFC1938: A One-Time Password System */
	if ((challenge = strstr(lastok, "otp-"))) {
	  char response[OPIE_RESPONSE_MAX+1];
	  int i;
	  char *n = xstrdup("");

	  i = opiegenerator(challenge, !strcmp(ctl->password, "opie") ? n : ctl->password, response);
	  free(n);
	  if ((i == -2) && !run.poll_interval) {
	    char secret[OPIE_SECRET_MAX+1];
	    fprintf(stderr, GT_("Secret pass phrase: "));
	    if (opiereadpass(secret, sizeof(secret), 0))
	      i = opiegenerator(challenge,  secret, response);
	    memset(secret, 0, sizeof(secret));
	  };

	  if (i) {
	    ok = PS_ERROR;
	    break;
	  };

	  ok = gen_transact(sock, "PASS %s", response);
	  break;
	}
#endif /* OPIE_ENABLE */

	/* KPOP uses out-of-band authentication and does not check what
	 * we send here, so send some random fixed string, to avoid
	 * users switching *to* KPOP accidentally revealing their
	 * password */
	if ((ctl->server.authenticate == A_ANY
		    || ctl->server.authenticate == A_KERBEROS_V4
		    || ctl->server.authenticate == A_KERBEROS_V5)
		&& (ctl->server.service != NULL
		    && strcmp(ctl->server.service, KPOP_PORT) == 0))
	{
	    ok = gen_transact(sock, "PASS krb_ticket");
	    break;
	}

	/* check if we are actually allowed to send the password */
	if (ctl->server.authenticate == A_ANY
		|| ctl->server.authenticate == A_PASSWORD) {
	    strlcpy(shroud, ctl->password, sizeof(shroud));
	    ok = gen_transact(sock, "PASS %s", ctl->password);
	} else {
	    report(stderr, GT_("We've run out of allowed authenticators and cannot continue.\n"));
	    ok = PS_AUTHFAIL;
	}
	memset(shroud, 0x55, sizeof(shroud));
	shroud[0] = '\0';
	break;

    case P_APOP:
	/* build MD5 digest from greeting timestamp + password */
	/* find start of timestamp */
	for (start = greeting;  *start != 0 && *start != '<';  start++)
	    continue;
	if (*start == 0) {
	    report(stderr,
		   GT_("Required APOP timestamp not found in greeting\n"));
	    return(PS_AUTHFAIL);
	}

	/* find end of timestamp */
	for (end = start;  *end != 0  && *end != '>';  end++)
	    continue;
	if (*end == 0 || end == start + 1) {
	    report(stderr, 
		   GT_("Timestamp syntax error in greeting\n"));
	    return(PS_AUTHFAIL);
	}
	else
	    *++end = '\0';

	/* SECURITY: 2007-03-17
	 * Strictly validating the presented challenge for RFC-822
	 * conformity (it must be a msg-id in terms of that standard) is
	 * supposed to make attacks against the MD5 implementation
	 * harder[1]
	 *
	 * [1] "Security vulnerability in APOP authentication",
	 *     Gaëtan Leurent, fetchmail-devel, 2007-03-17 */
	if (!rfc822_valid_msgid((unsigned char *)start)) {
	    report(stderr,
		    GT_("Invalid APOP timestamp.\n"));
	    return PS_AUTHFAIL;
	}

	/* copy timestamp and password into digestion buffer */
	msg = (char *)xmalloc((end-start+1) + strlen(ctl->password) + 1);
	strcpy(msg,start);
	strcat(msg,ctl->password);
	strcpy((char *)ctl->digest, MD5Digest((unsigned char *)msg));
	free(msg);

	ok = gen_transact(sock, "APOP %s %s", ctl->remotename, (char *)ctl->digest);
	break;

    case P_RPOP:
	if ((ok = gen_transact(sock,"USER %s", ctl->remotename)) == 0) {
	    strlcpy(shroud, ctl->password, sizeof(shroud));
	    ok = gen_transact(sock, "RPOP %s", ctl->password);
	    memset(shroud, 0x55, sizeof(shroud));
	    shroud[0] = '\0';
	}
	break;

    default:
	report(stderr, GT_("Undefined protocol request in POP3_auth\n"));
	ok = PS_ERROR;
    }

#ifdef SSL_ENABLE
    /* this is for servers which claim to support TLS, but actually
     * don't! */
    if (connection_may_have_tls_errors
		    && (ok == PS_SOCKET || ok == PS_PROTOCOL))
    {
	xfree(ctl->sslproto);
	ctl->sslproto = xstrdup("");
	/* repoll immediately without TLS */
	ok = PS_REPOLL;
    }
#endif

    if (ok != 0)
    {
	/* maybe we detected a lock-busy condition? */
        if (ok == PS_LOCKBUSY)
	    report(stderr, GT_("lock busy!  Is another session active?\n")); 

	return(ok);
    }

/* Disable the sleep. Based on patch by Brian Candler 2004-04-19/2004-11-08,
 * accepted by Matthias Andree.
 *
 * Rationale: the server must have locked the spool before returning +OK;
 * this sleep just wastes time and hence, for modem and GSM CSD users, money. */
#ifdef WANT_BOGUS
    /*
     * Empirical experience shows some server/OS combinations
     * may need a brief pause even after any lockfiles on the
     * server are released, to give the server time to finish
     * copying back very large mailfolders from the temp-file...
     * this is only ever an issue with extremely large mailboxes.
     */
    sleep(3); /* to be _really_ safe, probably need sleep(5)! */
#endif

    /* we're approved */
    return(PS_SUCCESS);
}

/* cut off C string at first POSIX space */
static void trim(char *s) {
    s += strcspn(s, POSIX_space);
    s[0] = '\0';
}

/* XXX FIXME: using the Message-ID is unsafe, some messages (spam,
 * broken messages) do not have Message-ID headers, and messages without
 * those appear to break this code and cause fetchmail (at least version
 * 6.2.3) to not delete such messages properly after retrieval.
 * See Sourceforge Bug #780933.
 *
 * The other problem is that the TOP command itself is optional, too... */
static int pop3_gettopid(int sock, int num , char *id, size_t idsize)
{
    int ok;
    int got_it;
    char buf [POPBUFSIZE+1];
    snprintf(buf, sizeof(buf), "TOP %d 1", num);
    if ((ok = gen_transact(sock, "%s", buf)) != 0)
       return ok;
    got_it = 0;
    while (gen_recv(sock, buf, sizeof(buf)) == 0)
    {
	if (DOTLINE(buf))
	    break;
	if (!got_it && 0 == strncasecmp("Message-Id:", buf, 11)) {
	    char *p = buf + 11;
	    got_it = 1;
	    p += strspn(p, POSIX_space);
	    strlcpy(id, p, idsize);
	    trim(id);
	}
    }
    /* XXX FIXME: do not return success here if no Message-ID header was
     * found. */
    return 0;
}

/** Parse the UID response (leading +OK must have been
 * stripped off) in buf, store the number in gotnum, and store the ID
 * into the caller-provided buffer "id" of size "idsize".
 * Returns PS_SUCCESS or PS_PROTOCOL for failure. */
static int parseuid(const char *buf, unsigned long *gotnum, char *id, size_t idsize)
{
    const char *i;
    char *j;

    /* skip leading blanks ourselves */
    i = buf;
    i += strspn(i, POSIX_space);
    errno = 0;
    *gotnum = strtoul(i, &j, 10);
    if (j == i || !*j || errno || NULL == strchr(POSIX_space, *j)) {
	report(stderr, GT_("Cannot handle UIDL response from upstream server.\n"));
	return PS_PROTOCOL;
    }
    j += strspn(j, POSIX_space);
    strlcpy(id, j, idsize);
    trim(id);
    return PS_SUCCESS;
}

/** request UIDL for single message \a num and stuff the result into the
 * buffer \a id which can hold \a idsize bytes */
static int pop3_getuidl(int sock, int num, char *id /** output */, size_t idsize)
{
    int ok;
    char buf [POPBUFSIZE+1];
    unsigned long gotnum;

    gen_send(sock, "UIDL %d", num);
    if ((ok = pop3_ok(sock, buf)) != 0)
	return(ok);
    if ((ok = parseuid(buf, &gotnum, id, idsize)))
	return ok;
    if (gotnum != (unsigned long)num) {
	report(stderr, GT_("Server responded with UID for wrong message.\n"));
	return PS_PROTOCOL;
    }
    return(PS_SUCCESS);
}

static int pop3_fastuidl( int sock,  struct query *ctl, unsigned int count, int *newp)
{
    int ok;
    unsigned int first_nr, last_nr, try_nr;
    char id [IDLEN+1];

    first_nr = 0;
    last_nr = count + 1;
    while (first_nr < last_nr - 1)
    {
	struct idlist	*newl;

	try_nr = (first_nr + last_nr) / 2;
	if ((ok = pop3_getuidl(sock, try_nr, id, sizeof(id))) != 0)
	    return ok;
	if ((newl = str_in_list(&ctl->oldsaved, id, FALSE)))
	{
	    flag mark = newl->val.status.mark;
	    if (mark == UID_DELETED || mark == UID_EXPUNGED)
	    {
		if (outlevel >= O_VERBOSE)
		    report(stderr, GT_("id=%s (num=%u) was deleted, but is still present!\n"), id, try_nr);
		/* just mark it as seen now! */
		newl->val.status.mark = mark = UID_SEEN;
	    }

	    /* narrow the search region! */
	    if (mark == UID_UNSEEN)
	    {
		if (outlevel >= O_DEBUG)
		    report(stdout, GT_("%u is unseen\n"), try_nr);
		last_nr = try_nr;
	    }
	    else
		first_nr = try_nr;

	    /* save the number */
	    newl->val.status.num = try_nr;
	}
	else
	{
	    if (outlevel >= O_DEBUG)
		report(stdout, GT_("%u is unseen\n"), try_nr);
	    last_nr = try_nr;

	    /* save it */
	    newl = save_str(&ctl->oldsaved, id, UID_UNSEEN);
	    newl->val.status.num = try_nr;
	}
    }
    if (outlevel >= O_DEBUG && last_nr <= count)
	report(stdout, GT_("%u is first unseen\n"), last_nr);

    /* update last! */
    *newp = count - first_nr;
    last = first_nr;
    return 0;
}

static int pop3_slowuidl( int sock,  struct query *ctl, int *countp, int *newp)
{
    /* XXX FIXME: this code is severely broken. A Cc:d mailing list
     * message will arrive twice with the same Message-ID, so this
     * slowuidl code will break. Same goes for messages without
     * Message-ID headers at all. This code would best be removed. */
    /* This approach tries to get the message headers from the
     * remote hosts and compares the message-id to the already known
     * ones:
     *  + if the first message containes a new id, all messages on
     *    the server will be new
     *  + if the first is known, try to estimate the last known message
     *    on the server and check. If this works you know the total number
     *    of messages to get.
     *  + Otherwise run a binary search to determine the last known message
     */
    int ok, nolinear = 0;
    int first_nr, list_len, try_id, try_nr, add_id;
    int num;
    char id [IDLEN+1];

    if ((ok = pop3_gettopid(sock, 1, id, sizeof(id))) != 0)
	return ok;

    if( ( first_nr = str_nr_in_list(&ctl->oldsaved, id) ) == -1 ) {
	/* the first message is unknown -> all messages are new */
	*newp = *countp;	
	return 0;
    }

    /* check where we expect the latest known message */
    list_len = count_list( &ctl->oldsaved );
    try_id = list_len  - first_nr; /* -1 + 1 */
    if( try_id > 1 ) {
	if( try_id <= *countp ) {
	    if ((ok = pop3_gettopid(sock, try_id, id, sizeof(id))) != 0)
		return ok;
    
	    try_nr = str_nr_last_in_list(&ctl->oldsaved, id);
	} else {
	    try_id = *countp+1;
	    try_nr = -1;
	}
	if( try_nr != list_len -1 ) {
	    /* some messages inbetween have been deleted... */
	    if( try_nr == -1 ) {
		nolinear = 1;

		for( add_id = 1<<30; add_id > try_id-1; add_id >>= 1 )
		    ;
		for( ; add_id; add_id >>= 1 ) {
		    if( try_nr == -1 ) {
			if( try_id - add_id <= 1 ) {
			    continue;
			}
			try_id -= add_id;
		    } else 
			try_id += add_id;
		    
		    if ((ok = pop3_gettopid(sock, try_id, id, sizeof(id))) != 0)
			return ok;
		    try_nr = str_nr_in_list(&ctl->oldsaved, id);
		}
		if( try_nr == -1 ) {
		    try_id--;
		}
	    } else {
		report(stderr, 
		       GT_("Messages inserted into list on server. Cannot handle this.\n"));
		return -1;
	    }
	} 
    }
    /* the first try_id messages are known -> copy them to the newsaved list */
    for( num = first_nr; num < list_len; num++ )
    {
	struct idlist	*newl = save_str(&ctl->newsaved, 
				str_from_nr_list(&ctl->oldsaved, num),
				UID_UNSEEN);
	newl->val.status.num = num - first_nr + 1;
    }

    if( nolinear ) {
	free_str_list(&ctl->oldsaved);
	ctl->oldsaved = 0;
	last = try_id;
    }

    *newp = *countp - try_id;
    return 0;
}

static int pop3_getrange(int sock, 
			 struct query *ctl,
			 const char *folder,
			 int *countp, int *newp, int *bytes)
/* get range of messages to be fetched */
{
    int ok;
    char buf [POPBUFSIZE+1];

    (void)folder;
    /* Ensure that the new list is properly empty */
    ctl->newsaved = (struct idlist *)NULL;

#ifdef MBOX
    /* Alain Knaff suggests this, but it's not RFC standard */
    if (folder)
	if ((ok = gen_transact(sock, "MBOX %s", folder)))
	    return ok;
#endif /* MBOX */

    /* get the total message count */
    gen_send(sock, "STAT");
    ok = pop3_ok(sock, buf);
    if (ok == 0) {
	int asgn;

	asgn = sscanf(buf,"%d %d", countp, bytes);
	if (asgn != 2)
		return PS_PROTOCOL;
    } else
	return(ok);

    /*
     * Newer, RFC-1725/1939-conformant POP servers may not have the LAST
     * command.  We work as hard as possible to hide this, but it makes
     * counting new messages intrinsically quadratic in the worst case.
     */
    last = 0;
    *newp = -1;
    /* if there are messages, and UIDL is desired, use UIDL
     * also use UIDL if fetchall is unset */
    if (*countp > 0 && (!ctl->fetchall || ctl->server.uidl))
    {
	int fastuidl;
	char id [IDLEN+1];

	/* should we do fast uidl this time? */
	fastuidl = ctl->fastuidl;
	if (*countp > 7 &&		/* linear search is better if there are few mails! */
	    !ctl->fetchall &&		/* with fetchall, all uids are required */
	    !ctl->flush &&		/* with flush, it is safer to disable fastuidl */
	    NUM_NONZERO (fastuidl))
	{
	    if (fastuidl == 1)
		dofastuidl = 1;
	    else
		dofastuidl = ctl->fastuidlcount != 0;
	}
	else
	    dofastuidl = 0;

	if (!ctl->server.uidl) {
	    gen_send(sock, "LAST");
	    ok = pop3_ok(sock, buf);
	} else
	    ok = 1;

	if (ok == 0)
	{
	    /* scan LAST reply */
	    if (sscanf(buf, "%d", &last) == 0)
	    {
		report(stderr, GT_("protocol error\n"));
		return(PS_ERROR);
	    }
	    *newp = (*countp - last);
	}
	else
	{
	    /* do UIDL */
	    if (dofastuidl)
		return(pop3_fastuidl( sock, ctl, *countp, newp));
	    /* grab the mailbox's UID list */
	    if (gen_transact(sock, "UIDL") != 0)
	    {
		/* don't worry, yet! do it the slow way */
		if (pop3_slowuidl(sock, ctl, countp, newp))
		{
		    report(stderr, GT_("protocol error while fetching UIDLs\n"));
		    return(PS_ERROR);
		}
	    }
	    else
	    {
		/* UIDL worked - parse reply */
		unsigned long unum;

		*newp = 0;
		while (gen_recv(sock, buf, sizeof(buf)) == PS_SUCCESS)
		{
		    if (DOTLINE(buf))
			break;

		    if (parseuid(buf, &unum, id, sizeof(id)) == PS_SUCCESS)
		    {
			struct idlist	*old, *newl;

			newl = save_str(&ctl->newsaved, id, UID_UNSEEN);
			newl->val.status.num = unum;

			if ((old = str_in_list(&ctl->oldsaved, id, FALSE)))
			{
			    flag mark = old->val.status.mark;
			    if (mark == UID_DELETED || mark == UID_EXPUNGED)
			    {
				/* XXX FIXME: switch 3 occurrences from
				 * (int)unum or (unsigned int)unum to
				 * remove the cast and use %lu - not now
				 * though, time for new release */
				if (outlevel >= O_VERBOSE)
				    report(stderr, GT_("id=%s (num=%d) was deleted, but is still present!\n"), id, (int)unum);
				/* just mark it as seen now! */
				old->val.status.mark = mark = UID_SEEN;
			    }
			    newl->val.status.mark = mark;
			    if (mark == UID_UNSEEN)
			    {
				(*newp)++;
				if (outlevel >= O_DEBUG)
				    report(stdout, GT_("%u is unseen\n"), (unsigned int)unum);
			    }
			}
			else
			{
			    (*newp)++;
			    if (outlevel >= O_DEBUG)
				report(stdout, GT_("%u is unseen\n"), (unsigned int)unum);
			    /* add it to oldsaved also! In case, we do not
			     * swap the lists (say, due to socket error),
			     * the same mail will not be downloaded again.
			     */
			    old = save_str(&ctl->oldsaved, id, UID_UNSEEN);
			}
			/* save the number */
			old->val.status.num = unum;
		    } else
			return PS_ERROR;
		} /* multi-line loop for UIDL reply */
	    } /* UIDL parser */
	} /* do UIDL */
    }

    return(PS_SUCCESS);
}

static int pop3_getpartialsizes(int sock, int first, int last, int *sizes)
/* capture the size of message #first */
{
    int	ok = 0, i, num;
    char buf [POPBUFSIZE+1];
    unsigned int size;

    for (i = first; i <= last; i++) {
	gen_send(sock, "LIST %d", i);
	if ((ok = pop3_ok(sock, buf)) != 0)
	    return(ok);
	if (sscanf(buf, "%d %u", &num, &size) == 2) {
	    if (num == i)
		sizes[i - first] = size;
	    else
		/* warn about possible attempt to induce buffer overrun
		 *
		 * we expect server reply message number and requested
		 * message number to match */
		report(stderr, "Warning: ignoring bogus data for message sizes returned by server.\n");
	}
    }
    return(ok);
}

static int pop3_getsizes(int sock, int count, int *sizes)
/* capture the sizes of all messages */
{
    int	ok;

    if ((ok = gen_transact(sock, "LIST")) != 0)
	return(ok);
    else
    {
	char buf [POPBUFSIZE+1];

	while ((ok = gen_recv(sock, buf, sizeof(buf))) == 0)
	{
	    unsigned int num, size;

	    if (DOTLINE(buf))
		break;
	    else if (sscanf(buf, "%u %u", &num, &size) == 2) {
		if (num > 0 && num <= (unsigned)count)
		    sizes[num - 1] = size;
		else
		    /* warn about possible attempt to induce buffer overrun */
		    report(stderr, "Warning: ignoring bogus data for message sizes returned by server.\n");
	    }
	}

	return(ok);
    }
}

static int pop3_is_old(int sock, struct query *ctl, int num)
/* is the given message old? */
{
    struct idlist *newl;
    if (!ctl->oldsaved)
	return (num <= last);
    else if (dofastuidl)
    {
	char id [IDLEN+1];

	if (num <= last)
	    return(TRUE);

	/* in fast uidl, we manipulate the old list only! */

	if ((newl = id_find(&ctl->oldsaved, num)))
	{
	    /* we already have the id! */
	    return(newl->val.status.mark != UID_UNSEEN);
	}

	/* get the uidl first! */
	if (pop3_getuidl(sock, num, id, sizeof(id)) != PS_SUCCESS)
	    return(TRUE);

	if ((newl = str_in_list(&ctl->oldsaved, id, FALSE))) {
	    /* we already have the id! */
	    newl->val.status.num = num;
	    return(newl->val.status.mark != UID_UNSEEN);
	}

	/* save it */
	newl = save_str(&ctl->oldsaved, id, UID_UNSEEN);
	newl->val.status.num = num;
	return(FALSE);
    }
    else
        return ((newl = id_find(&ctl->newsaved, num)) != NULL &&
	    newl->val.status.mark != UID_UNSEEN);
}

#ifdef UNUSED
/*
 * We could use this to fetch headers only as we do for IMAP.  The trouble 
 * is that there's no way to fetch the body only.  So the following RETR 
 * would have to re-fetch the header.  Enough messages have longer headers
 * than bodies to make this a net loss.
 */
static int pop_fetch_headers(int sock, struct query *ctl,int number,int *lenp)
/* request headers of nth message */
{
    int ok;
    char buf[POPBUFSIZE+1];

    gen_send(sock, "TOP %d 0", number);
    if ((ok = pop3_ok(sock, buf)) != 0)
	return(ok);

    *lenp = -1;		/* we got sizes from the LIST response */

    return(PS_SUCCESS);
}
#endif /* UNUSED */

static int pop3_fetch(int sock, struct query *ctl, int number, int *lenp)
/* request nth message */
{
    int ok;
    char buf[POPBUFSIZE+1];

#ifdef SDPS_ENABLE
    /*
     * See http://www.demon.net/helpdesk/producthelp/mail/sdps-tech.html/
     * for a description of what we're parsing here.
     * -- updated 2006-02-22
     */
    if (ctl->server.sdps)
    {
	int	linecount = 0;

	sdps_envfrom = (char *)NULL;
	sdps_envto = (char *)NULL;
	gen_send(sock, "*ENV %d", number);
	do {
	    if (gen_recv(sock, buf, sizeof(buf)))
            {
                break;
            }
            linecount++;
	    switch (linecount) {
	    case 4:
		/* No need to wrap envelope from address */
		/* FIXME: some parts of fetchmail don't handle null
		 * envelope senders, so use <> to mark null sender
		 * as a workaround. */
		if (strspn(buf, " \t") == strlen(buf))
		    strcpy(buf, "<>");
		sdps_envfrom = (char *)xmalloc(strlen(buf)+1);
		strcpy(sdps_envfrom,buf);
		break;
	    case 5:
                /* Wrap address with To: <> so nxtaddr() likes it */
                sdps_envto = (char *)xmalloc(strlen(buf)+7);
                sprintf(sdps_envto,"To: <%s>",buf);
		break;
            }
	} while
	    (!(buf[0] == '.' && (buf[1] == '\r' || buf[1] == '\n' || buf[1] == '\0')));
    }
#else
    (void)ctl;
#endif /* SDPS_ENABLE */

    /*
     * Though the POP RFCs don't document this fact, on almost every
     * POP3 server I know of messages are marked "seen" only at the
     * time the OK response to a RETR is issued.
     *
     * This means we can use TOP to fetch the message without setting its
     * seen flag.  This is good!  It means that if the protocol exchange
     * craps out during the message, it will still be marked `unseen' on
     * the server.  (Exception: in early 1999 SpryNet's POP3 servers were
     * reported to mark messages seen on a TOP fetch.)
     *
     * However...*don't* do this if we're using keep to suppress deletion!
     * In that case, marking the seen flag is the only way to prevent the
     * message from being re-fetched on subsequent runs.
     *
     * Also use RETR (that means no TOP, no peek) if fetchall is on.
     * This gives us a workaround for servers like usa.net's that bungle
     * TOP.  It's pretty harmless because fetchall guarantees that any
     * message dropped by an interrupted RETR will be picked up on the
     * next poll of the site.
     *
     * We take advantage here of the fact that, according to all the
     * POP RFCs, "if the number of lines requested by the POP3 client
     * is greater than than the number of lines in the body, then the
     * POP3 server sends the entire message.").
     *
     * The line count passed (99999999) is the maximum value CompuServe will
     * accept; it's much lower than the natural value 2147483646 (the maximum
     * twos-complement signed 32-bit integer minus 1) */
    if (!peek_capable)
	gen_send(sock, "RETR %d", number);
    else
	gen_send(sock, "TOP %d 99999999", number);
    if ((ok = pop3_ok(sock, buf)) != 0)
	return(ok);

    *lenp = -1;		/* we got sizes from the LIST response */

    return(PS_SUCCESS);
}

static void mark_uid_seen(struct query *ctl, int number)
/* Tell the UID code we've seen this. */
{
    struct idlist	*sdp;

    if ((sdp = id_find(&ctl->newsaved, number)))
	sdp->val.status.mark = UID_SEEN;
    /* mark it as seen in oldsaved also! In case, we do not swap the lists
     * (say, due to socket error), the same mail will not be downloaded
     * again.
     */
    if ((sdp = id_find(&ctl->oldsaved, number)))
	sdp->val.status.mark = UID_SEEN;
}

static int pop3_delete(int sock, struct query *ctl, int number)
/* delete a given message */
{
    int ok;
    mark_uid_seen(ctl, number);
    /* actually, mark for deletion -- doesn't happen until QUIT time */
    ok = gen_transact(sock, "DELE %d", number);
    if (ok != PS_SUCCESS)
	return(ok);
    delete_str(dofastuidl ? &ctl->oldsaved : &ctl->newsaved, number);
    return(PS_SUCCESS);
}

static int pop3_mark_seen(int sock, struct query *ctl, int number)
/* mark a given message as seen */
{
    (void)sock;
    mark_uid_seen(ctl, number);
    return(PS_SUCCESS);
}

static int pop3_logout(int sock, struct query *ctl)
/* send logout command */
{
    int ok;

#ifdef __UNUSED__
    /*
     * We used to do this in case the server marks messages deleted when seen.
     * (Yes, this has been reported, in the MercuryP/NLM server.
     * It's even legal under RFC 1939 (section 8) as a site policy.)
     * It interacted badly with UIDL, though.  Thomas Zajic wrote:
     * "Running 'fetchmail -F -v' and checking the logs, I found out
     * that fetchmail did in fact flush my mailbox properly, but sent
     * a RSET just before sending QUIT to log off.  This caused the
     * POP3 server to undo/forget about the previous DELEs, resetting
     * my mailbox to its original (ie.  unflushed) state. The
     * ~/.fetchids file did get flushed though, so the next time
     * fetchmail was run it saw all the old messages as new ones ..."
     */
     if (ctl->keep)
	gen_transact(sock, "RSET");
#endif /* __UNUSED__ */

    ok = gen_transact(sock, "QUIT");
    if (!ok)
	expunge_uids(ctl);

    return(ok);
}

static const struct method pop3 =
{
    "POP3",		/* Post Office Protocol v3 */
    "pop3",		/* port for plain and TLS POP3 */
    "pop3s",		/* port for SSL POP3 */
    FALSE,		/* this is not a tagged protocol */
    TRUE,		/* this uses a message delimiter */
    pop3_ok,		/* parse command response */
    pop3_getauth,	/* get authorization */
    pop3_getrange,	/* query range of messages */
    pop3_getsizes,	/* we can get a list of sizes */
    pop3_getpartialsizes,	/* we can get the size of 1 mail */
    pop3_is_old,	/* how do we tell a message is old? */
    pop3_fetch,		/* request given message */
    NULL,		/* no way to fetch body alone */
    NULL,		/* no message trailer */
    pop3_delete,	/* how to delete a message */
    pop3_mark_seen,	/* how to mark a message as seen */
    NULL,		/* no action at end of mailbox */
    pop3_logout,	/* log out, we're done */
    FALSE,		/* no, we can't re-poll */
};

int doPOP3 (struct query *ctl)
/* retrieve messages using POP3 */
{
#ifndef MBOX
    if (ctl->mailboxes->id) {
	fprintf(stderr,GT_("Option --folder is not supported with POP3\n"));
	return(PS_SYNTAX);
    }
#endif /* MBOX */

    return(do_protocol(ctl, &pop3));
}
#endif /* POP3_ENABLE */

/* pop3.c ends here */
