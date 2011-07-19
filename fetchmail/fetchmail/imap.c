/*
 * imap.c -- IMAP2bis/IMAP4 protocol methods
 *
 * Copyright 1997 by Eric S. Raymond
 * For license terms, see the file COPYING in this directory.
 */

#include  "config.h"
#include  <stdio.h>
#include  <string.h>
#include  <strings.h>
#include  <ctype.h>
#if defined(STDC_HEADERS)
#include  <stdlib.h>
#include  <limits.h>
#include  <errno.h>
#endif
#include  "fetchmail.h"
#include  "socket.h"

#include  "i18n.h"

/* imap_version values */
#define IMAP2		-1	/* IMAP2 or IMAP2BIS, RFC1176 */
#define IMAP4		0	/* IMAP4 rev 0, RFC1730 */
#define IMAP4rev1	1	/* IMAP4 rev 1, RFC2060 */

/* global variables: please reinitialize them explicitly for proper
 * working in daemon mode */

/* TODO: session variables to be initialized before server greeting */
static int preauth = FALSE;

/* session variables initialized in capa_probe() or imap_getauth() */
static char capabilities[MSGBUFSIZE+1];
static int imap_version = IMAP4;
static flag do_idle = FALSE, has_idle = FALSE;
static int expunge_period = 1;

/* mailbox variables initialized in imap_getrange() */
static int count = 0, oldcount = 0, recentcount = 0, unseen = 0, deletions = 0;
static unsigned int startcount = 1;
static int expunged = 0;
static unsigned int *unseen_messages;

/* for "IMAP> EXPUNGE" */
static int actual_deletions = 0;

/* for "IMAP> IDLE" */
static int saved_timeout = 0, idle_timeout = 0;
static time_t idle_start_time = 0;

static int imap_untagged_response(int sock, const char *buf)
/* interpret untagged status responses */
{
    /* For each individual check, use a BLANK before the word to avoid
     * confusion with the \Recent flag or similar */
    if (stage == STAGE_GETAUTH
	    && !strncmp(buf, "* CAPABILITY", 12))
    {
	strlcpy(capabilities, buf + 12, sizeof(capabilities));
    }
    else if (stage == STAGE_GETAUTH
	    && !strncmp(buf, "* PREAUTH", 9))
    {
	preauth = TRUE;
    }
    else if (stage != STAGE_LOGOUT
	    && !strncmp(buf, "* BYE", 5))
    {
	/* log the unexpected bye from server as we expect the
	 * connection to be cut-off after this */
	if (outlevel > O_SILENT)
	    report(stderr, GT_("Received BYE response from IMAP server: %s"), buf + 5);
    }
    else if (strstr(buf, " EXISTS"))
    {
	char *t; unsigned long u;
	errno = 0;
	u = strtoul(buf+2, &t, 10);
	/*
	 * Don't trust the message count passed by the server.
	 * Without this check, it might be possible to do a
	 * DNS-spoofing attack that would pass back a ridiculous 
	 * count, and allocate a malloc area that would overlap
	 * a portion of the stack.
	 */
	if (errno /* strtoul failed */
		|| t == buf+2 /* no valid data */
		|| u > (unsigned long)(INT_MAX/sizeof(int)) /* too large */)
	{
	    report(stderr, GT_("bogus message count in \"%s\"!"), buf);
	    return(PS_PROTOCOL);
	}
	count = u; /* safe as long as count <= INT_MAX - checked above */

	if ((recentcount = count - oldcount) < 0)
	    recentcount = 0;

	/*
	 * Nasty kluge to handle RFC2177 IDLE.  If we know we're idling
	 * we can't wait for the tag matching the IDLE; we have to tell the
	 * server the IDLE is finished by shipping back a DONE when we
	 * see an EXISTS.  Only after that will a tagged response be
	 * shipped.  The idling flag also gets cleared on a timeout.
	 */
	if (stage == STAGE_IDLE)
	{
	    /* If IDLE isn't supported, we were only sending NOOPs anyway. */
	    if (has_idle)
	    {
		/* we do our own write and report here to disable tagging */
		SockWrite(sock, "DONE\r\n", 6);
		if (outlevel >= O_MONITOR)
		    report(stdout, "IMAP> DONE\n");
	    }

	    mytimeout = saved_timeout;
	    stage = STAGE_GETRANGE;
	}
    }
    /* we now compute recentcount as a difference between
     * new and old EXISTS, hence disable RECENT check */
# if 0
    else if (strstr(buf, " RECENT"))
    {
	/* fixme: use strto[u]l and error checking */
	recentcount = atoi(buf+2);
    }
# endif
    else if (strstr(buf, " EXPUNGE"))
    {
	unsigned long u; char *t;
	/* the response "* 10 EXPUNGE" means that the currently
	 * tenth (i.e. only one) message has been deleted */
	errno = 0;
	u = strtoul(buf+2, &t, 10);
	if (errno /* conversion error */ || t == buf+2 /* no number found */) {
	    report(stderr, GT_("bogus EXPUNGE count in \"%s\"!"), buf);
	    return PS_PROTOCOL;
	}
	if (u > 0)
	{
	    if (count > 0)
		count--;
	    if (oldcount > 0)
		oldcount--;
	    /* We do expect an EXISTS response immediately
	     * after this, so this updation of recentcount is
	     * just a precaution! */
	    if ((recentcount = count - oldcount) < 0)
		recentcount = 0;
	    actual_deletions++;
	}
    }
    /*
     * The server may decide to make the mailbox read-only, 
     * which causes fetchmail to go into a endless loop
     * fetching the same message over and over again. 
     * 
     * However, for check_only, we use EXAMINE which will
     * mark the mailbox read-only as per the RFC.
     * 
     * This checks for the condition and aborts if 
     * the mailbox is read-only. 
     *
     * See RFC 2060 section 6.3.1 (SELECT).
     * See RFC 2060 section 6.3.2 (EXAMINE).
     */ 
    else if (stage == STAGE_GETRANGE
	    && !check_only && strstr(buf, "[READ-ONLY]"))
    {
	return(PS_LOCKBUSY);
    }
    else
    {
	return(PS_UNTAGGED);
    }
    return(PS_SUCCESS);
}

static int imap_response(int sock, char *argbuf)
/* parse command response */
{
    char buf[MSGBUFSIZE+1];

    do {
	int	ok;
	char	*cp;

	if ((ok = gen_recv(sock, buf, sizeof(buf))))
	    return(ok);

	/* all tokens in responses are caseblind */
	for (cp = buf; *cp; cp++)
	    if (islower((unsigned char)*cp))
		*cp = toupper((unsigned char)*cp);

	/* untagged responses start with "* " */
	if (buf[0] == '*' && buf[1] == ' ') {
	    ok = imap_untagged_response(sock, buf);
	    if (ok == PS_UNTAGGED)
	    {
		if (argbuf && stage != STAGE_IDLE && tag[0] != '\0')
		{
		    /* if there is an unmatched response, pass it back to
		     * the calling function for further analysis. The
		     * calling function should call imap_response() again
		     * to read the remaining response */
		    strcpy(argbuf, buf);
		    return(ok);
		}
	    }
	    else if (ok != PS_SUCCESS)
		return(ok);
	}

	if (stage == STAGE_IDLE)
	{
	    /* reduce the timeout: servers may not reset their timeout
	     * when they send some information asynchronously */
	    mytimeout = idle_timeout - (time((time_t *) NULL) - idle_start_time);
	    if (mytimeout <= 0)
		return(PS_IDLETIMEOUT);
	}
    } while
	(tag[0] != '\0' && strncmp(buf, tag, strlen(tag)));

    if (tag[0] == '\0')
    {
	if (argbuf)
	    strcpy(argbuf, buf);
	return(PS_SUCCESS); 
    }
    else
    {
	char	*cp;

	/* skip the tag */
	for (cp = buf; !isspace((unsigned char)*cp); cp++)
	    continue;
	while (isspace((unsigned char)*cp))
	    cp++;

        if (strncasecmp(cp, "OK", 2) == 0)
	{
	    if (argbuf)
		strcpy(argbuf, cp);
	    return(PS_SUCCESS);
	}
	else if (strncasecmp(cp, "BAD", 3) == 0)
	    return(PS_ERROR);
	else if (strncasecmp(cp, "NO", 2) == 0)
	{
	    if (stage == STAGE_GETAUTH) 
		return(PS_AUTHFAIL);	/* RFC2060, 6.2.2 */
	    else if (stage == STAGE_GETSIZES)
		return(PS_SUCCESS);	/* see comments in imap_getpartialsizes() */
	    else
		return(PS_ERROR);
	}
	else
	    return(PS_PROTOCOL);
    }
}

static int imap_ok(int sock, char *argbuf)
/* parse command response */
{
    int ok;

    while ((ok = imap_response(sock, argbuf)) == PS_UNTAGGED)
	; /* wait for the tagged response */
    return(ok);
}

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

static int do_imap_ntlm(int sock, struct query *ctl)
{
    int result;

    gen_send(sock, "AUTHENTICATE NTLM");

    if ((result = ntlm_helper(sock, ctl, "IMAP")))
	return result;

    result = imap_ok (sock, NULL);
    if (result == PS_SUCCESS)
	return PS_SUCCESS;
    else
	return PS_AUTHFAIL;
}
#endif /* NTLM */

static void imap_canonicalize(char *result, char *raw, size_t maxlen)
/* encode an IMAP password as per RFC1730's quoting conventions */
{
    size_t i, j;

    j = 0;
    for (i = 0; i < strlen(raw) && i < maxlen; i++)
    {
	if ((raw[i] == '\\') || (raw[i] == '"'))
	    result[j++] = '\\';
	result[j++] = raw[i];
    }
    result[j] = '\0';
}

static void capa_probe(int sock, struct query *ctl)
/* set capability variables from a CAPA probe */
{
    int	ok;

    /* probe to see if we're running IMAP4 and can use RFC822.PEEK */
    capabilities[0] = '\0';
    if ((ok = gen_transact(sock, "CAPABILITY")) == PS_SUCCESS)
    {
	char	*cp;

	/* capability checks are supposed to be caseblind */
	for (cp = capabilities; *cp; cp++)
	    *cp = toupper((unsigned char)*cp);

	/* UW-IMAP server 10.173 notifies in all caps, but RFC2060 says we
	   should expect a response in mixed-case */
	if (strstr(capabilities, "IMAP4REV1"))
	{
	    imap_version = IMAP4rev1;
	    if (outlevel >= O_DEBUG)
		report(stdout, GT_("Protocol identified as IMAP4 rev 1\n"));
	}
	else
	{
	    imap_version = IMAP4;
	    if (outlevel >= O_DEBUG)
		report(stdout, GT_("Protocol identified as IMAP4 rev 0\n"));
	}
    }
    else if (ok == PS_ERROR)
    {
	imap_version = IMAP2;
	if (outlevel >= O_DEBUG)
	    report(stdout, GT_("Protocol identified as IMAP2 or IMAP2BIS\n"));
    }

    /* 
     * Handle idling.  We depend on coming through here on startup
     * and after each timeout (including timeouts during idles).
     */
    do_idle = ctl->idle;
    if (ctl->idle)
    {
	if (strstr(capabilities, "IDLE"))
	    has_idle = TRUE;
	else
	    has_idle = FALSE;
	if (outlevel >= O_VERBOSE)
	    report(stdout, GT_("will idle after poll\n"));
    }

    peek_capable = (imap_version >= IMAP4);
}

static int do_authcert (int sock, const char *command, const char *name)
/* do authentication "external" (authentication provided by client cert) */
{
    char buf[256];

    if (name && name[0])
    {
        size_t len = strlen(name);
        if ((len / 3) + ((len % 3) ? 4 : 0)  < sizeof(buf))
            to64frombits (buf, name, strlen(name));
        else
            return PS_AUTHFAIL; /* buffer too small. */
    }
    else
        buf[0]=0;
    return gen_transact(sock, "%s EXTERNAL %s",command,buf);
}

static int imap_getauth(int sock, struct query *ctl, char *greeting)
/* apply for connection authorization */
{
    int ok = 0;
#ifdef SSL_ENABLE
    int got_tls = 0;
#endif
    (void)greeting;

    /*
     * Assumption: expunges are cheap, so we want to do them
     * after every message unless user said otherwise.
     */
    if (NUM_SPECIFIED(ctl->expunge))
	expunge_period = NUM_VALUE_OUT(ctl->expunge);
    else
	expunge_period = 1;

    capa_probe(sock, ctl);

    /* 
     * If either (a) we saw a PREAUTH token in the greeting, or
     * (b) the user specified ssh preauthentication, then we're done.
     */
    if (preauth || ctl->server.authenticate == A_SSH)
    {
        preauth = FALSE;  /* reset for the next session */
        return(PS_SUCCESS);
    }

#ifdef SSL_ENABLE
    if (maybe_tls(ctl)) {
	char *commonname;

	commonname = ctl->server.pollname;
	if (ctl->server.via)
	    commonname = ctl->server.via;
	if (ctl->sslcommonname)
	    commonname = ctl->sslcommonname;

	if (strstr(capabilities, "STARTTLS"))
	{
	    /* Use "tls1" rather than ctl->sslproto because tls1 is the only
	     * protocol that will work with STARTTLS.  Don't need to worry
	     * whether TLS is mandatory or opportunistic unless SSLOpen() fails
	     * (see below). */
	    if (gen_transact(sock, "STARTTLS") == PS_SUCCESS
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
		got_tls = 1;
		capa_probe(sock, ctl);
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
		if (outlevel >= O_VERBOSE) {
		    report(stdout, GT_("%s: opportunistic upgrade to TLS failed, trying to continue\n"), commonname);
		}
		/* We don't know whether the connection is in a working state, so
		 * test by issuing a NOOP. */
		if (gen_transact(sock, "NOOP") != PS_SUCCESS) {
		    /* Not usable.  Empty sslproto to force an unencrypted
		     * connection on the next attempt, and repoll. */
		    ctl->sslproto = xstrdup("");
		    return PS_REPOLL;
		}
		/* Usable.  Proceed with authenticating insecurely. */
	    }
	}
    }
#endif /* SSL_ENABLE */

    /*
     * Time to authenticate the user.
     * Try the protocol variants that don't require passwords first.
     */
    ok = PS_AUTHFAIL;

    /* Yahoo hack - we'll just try ID if it was offered by the server,
     * and IGNORE errors. */
    {
	char *tmp = strstr(capabilities, " ID");
	if (tmp && !isalnum((unsigned char)tmp[3]) && strstr(ctl->server.via ? ctl->server.via : ctl->server.pollname, "yahoo.com")) {
		(void)gen_transact(sock, "ID (\"guid\" \"1\")");
	}
    }

    if ((ctl->server.authenticate == A_ANY 
         || ctl->server.authenticate == A_EXTERNAL)
	&& strstr(capabilities, "AUTH=EXTERNAL"))
    {
        ok = do_authcert(sock, "AUTHENTICATE", ctl->remotename);
	if (ok)
        {
            /* SASL cancellation of authentication */
            gen_send(sock, "*");
            if (ctl->server.authenticate != A_ANY)
                return ok;
        } else {
            return ok;
	}
    }

#ifdef GSSAPI
    if (((ctl->server.authenticate == A_ANY && check_gss_creds("imap", ctl->server.truename) == PS_SUCCESS)
	 || ctl->server.authenticate == A_GSSAPI)
	&& strstr(capabilities, "AUTH=GSSAPI"))
    {
	if ((ok = do_gssauth(sock, "AUTHENTICATE", "imap",
			ctl->server.truename, ctl->remotename)))
	{
	    if (ctl->server.authenticate != A_ANY)
                return ok;
	} else  {
	    return ok;
	}
    }
#endif /* GSSAPI */

#ifdef KERBEROS_V4
    if ((ctl->server.authenticate == A_ANY 
	 || ctl->server.authenticate == A_KERBEROS_V4
	 || ctl->server.authenticate == A_KERBEROS_V5) 
	&& strstr(capabilities, "AUTH=KERBEROS_V4"))
    {
	if ((ok = do_rfc1731(sock, "AUTHENTICATE", ctl->server.truename)))
	{
	    /* SASL cancellation of authentication */
	    gen_send(sock, "*");
	    if(ctl->server.authenticate != A_ANY)
                return ok;
	}
	else
	    return ok;
    }
#endif /* KERBEROS_V4 */

    /*
     * No such luck.  OK, now try the variants that mask your password
     * in a challenge-response.
     */

    if ((ctl->server.authenticate == A_ANY && strstr(capabilities, "AUTH=CRAM-MD5"))
	|| ctl->server.authenticate == A_CRAM_MD5)
    {
	if ((ok = do_cram_md5 (sock, "AUTHENTICATE", ctl, NULL)))
	{
	    if(ctl->server.authenticate != A_ANY)
                return ok;
	}
	else
	    return ok;
    }

#ifdef OPIE_ENABLE
    if ((ctl->server.authenticate == A_ANY 
	 || ctl->server.authenticate == A_OTP)
	&& strstr(capabilities, "AUTH=X-OTP")) {
	if ((ok = do_otp(sock, "AUTHENTICATE", ctl)))
	{
	    /* SASL cancellation of authentication */
	    gen_send(sock, "*");
	    if(ctl->server.authenticate != A_ANY)
                return ok;
	} else {
	    return ok;
	}
    }
#else
    if (ctl->server.authenticate == A_OTP)
    {
	report(stderr, 
	   GT_("Required OTP capability not compiled into fetchmail\n"));
    }
#endif /* OPIE_ENABLE */

#ifdef NTLM_ENABLE
    if ((ctl->server.authenticate == A_ANY 
	 || ctl->server.authenticate == A_NTLM) 
	&& strstr (capabilities, "AUTH=NTLM")) {
	if ((ok = do_imap_ntlm(sock, ctl)))
	{
	    if(ctl->server.authenticate != A_ANY)
                return ok;
	}
	else
	    return(ok);
    }
#else
    if (ctl->server.authenticate == A_NTLM)
    {
	report(stderr, 
	   GT_("Required NTLM capability not compiled into fetchmail\n"));
    }
#endif /* NTLM_ENABLE */

#ifdef __UNUSED__	/* The Cyrus IMAP4rev1 server chokes on this */
    /* this handles either AUTH=LOGIN or AUTH-LOGIN */
    if ((imap_version >= IMAP4rev1) && (!strstr(capabilities, "LOGIN")))
    {
	report(stderr, 
	       GT_("Required LOGIN capability not supported by server\n"));
    }
#endif /* __UNUSED__ */

    /* 
     * We're stuck with sending the password en clair.
     * The reason for this odd-looking logic is that some
     * servers return LOGINDISABLED even though login 
     * actually works.  So arrange things in such a way that
     * setting auth passwd makes it ignore this capability.
     */
    if((ctl->server.authenticate==A_ANY&&!strstr(capabilities,"LOGINDISABLED"))
	|| ctl->server.authenticate == A_PASSWORD)
    {
	/* these sizes guarantee no buffer overflow */
	char *remotename, *password;
	size_t rnl, pwl;
	rnl = 2 * strlen(ctl->remotename) + 1;
	pwl = 2 * strlen(ctl->password) + 1;
	remotename = (char *)xmalloc(rnl);
	password = (char *)xmalloc(pwl);

	imap_canonicalize(remotename, ctl->remotename, rnl);
	imap_canonicalize(password, ctl->password, pwl);

	snprintf(shroud, sizeof (shroud), "\"%s\"", password);
	ok = gen_transact(sock, "LOGIN \"%s\" \"%s\"", remotename, password);
	memset(shroud, 0x55, sizeof(shroud));
	shroud[0] = '\0';
	memset(password, 0x55, strlen(password));
	free(password);
	free(remotename);
	if (ok)
	{
	    if(ctl->server.authenticate != A_ANY)
                return ok;
	}
	else
	    return(ok);
    }

    return(ok);
}

static int internal_expunge(int sock)
/* ship an expunge, resetting associated counters */
{
    int	ok;

    actual_deletions = 0;

    if ((ok = gen_transact(sock, "EXPUNGE")))
	return(ok);

    /* if there is a mismatch between the number of mails which should
     * have been expunged and the number of mails actually expunged,
     * another email client may be deleting mails. Quit here,
     * otherwise fetchmail gets out-of-sync with the imap server,
     * reports the wrong size to the SMTP server on MAIL FROM: and
     * triggers a "message ... was not the expected length" error on
     * every subsequent mail */
    if (deletions > 0 && deletions != actual_deletions)
    {
	report(stderr,
		GT_("mail expunge mismatch (%d actual != %d expected)\n"),
		actual_deletions, deletions);
	deletions = 0;
	return(PS_ERROR);
    }

    expunged += deletions;
    deletions = 0;

#ifdef IMAP_UID	/* not used */
    expunge_uids(ctl);
#endif /* IMAP_UID */

    return(PS_SUCCESS);
}

static int imap_idle(int sock)
/* start an RFC2177 IDLE, or fake one if unsupported */
{
    int ok;

    saved_timeout = mytimeout;

    if (has_idle) {
	/* special timeout to terminate the IDLE and re-issue it
	 * at least every 28 minutes:
	 * (the server may have an inactivity timeout) */
	mytimeout = idle_timeout = 1680; /* 28 min */
	time(&idle_start_time);
	stage = STAGE_IDLE;
	/* enter IDLE mode */
	ok = gen_transact(sock, "IDLE");

	if (ok == PS_IDLETIMEOUT) {
	    /* send "DONE" continuation */
	    SockWrite(sock, "DONE\r\n", 6);
	    if (outlevel >= O_MONITOR)
		report(stdout, "IMAP> DONE\n");
	    /* reset stage and timeout here: we are not idling any more */
	    mytimeout = saved_timeout;
	    stage = STAGE_GETRANGE;
	    /* get OK IDLE message */
	    ok = imap_ok(sock, NULL);
	}
    } else {  /* no idle support, fake it */
	/* Note: stage and timeout have not been changed here as NOOP
	 * does not idle */
	ok = gen_transact(sock, "NOOP");

	/* no error, but no new mail either */
	if (ok == PS_SUCCESS && recentcount == 0)
	{
	    /* There are some servers who do send new mail
	     * notification out of the blue. This is in compliance
	     * with RFC 2060 section 5.3. Wait for that with a low
	     * timeout */
	    mytimeout = idle_timeout = 28;
	    time(&idle_start_time);
	    stage = STAGE_IDLE;
	    /* We are waiting for notification; no tag needed */
	    tag[0] = '\0';
	    /* wait (briefly) for an unsolicited status update */
	    ok = imap_ok(sock, NULL);
	    if (ok == PS_IDLETIMEOUT) {
		/* no notification came; ok */
		ok = PS_SUCCESS;
	    }
	}
    }

    /* restore normal timeout value */
    set_timeout(0);
    mytimeout = saved_timeout;
    stage = STAGE_GETRANGE;

    return(ok);
}

/* maximum number of numbers we can process in "SEARCH" response */
# define IMAP_SEARCH_MAX 1000

static int imap_search(int sock, struct query *ctl, int count)
/* search for unseen messages */
{
    int ok, first, last;
    char buf[MSGBUFSIZE+1], *cp;

    /* Don't count deleted messages. Enabled only for IMAP4 servers or
     * higher and only when keeping mails. This flag will have an
     * effect only when user has marked some unread mails for deletion
     * using another e-mail client. */
    flag skipdeleted = (imap_version >= IMAP4) && ctl->keep;
    const char *undeleted;

    /* Skip range search if there are less than or equal to
     * IMAP_SEARCH_MAX mails. */
    flag skiprangesearch = (count <= IMAP_SEARCH_MAX);

    /* startcount is higher than count so that if there are no
     * unseen messages, imap_getsizes() will not need to do
     * anything! */
    startcount = count + 1;

    for (first = 1, last = IMAP_SEARCH_MAX; first <= count; first += IMAP_SEARCH_MAX, last += IMAP_SEARCH_MAX)
    {
	if (last > count)
	    last = count;

restartsearch:
	undeleted = (skipdeleted ? " UNDELETED" : "");
	if (skiprangesearch)
	    gen_send(sock, "SEARCH UNSEEN%s", undeleted);
	else if (last == first)
	    gen_send(sock, "SEARCH %d UNSEEN%s", last, undeleted);
	else
	    gen_send(sock, "SEARCH %d:%d UNSEEN%s", first, last, undeleted);
	while ((ok = imap_response(sock, buf)) == PS_UNTAGGED)
	{
	    if ((cp = strstr(buf, "* SEARCH")))
	    {
		char	*ep;

		cp += 8;	/* skip "* SEARCH" */
		while (*cp && unseen < count)
		{
		    /* skip whitespace */
		    while (*cp && isspace((unsigned char)*cp))
			cp++;
		    if (*cp) 
		    {
			unsigned long um;

			errno = 0;
			um = strtoul(cp,&ep,10);
			if (errno == 0 && ep > cp
				&& um <= INT_MAX && um <= (unsigned)count)
			{
			    unseen_messages[unseen++] = um;
			    if (outlevel >= O_DEBUG)
				report(stdout, GT_("%lu is unseen\n"), um);
			    if (startcount > um)
				startcount = um;
			}
			cp = ep;
		    }
		}
	    }
	}
	/* if there is a protocol error on the first loop, try a
	 * different search command */
	if (ok == PS_ERROR && first == 1)
	{
	    if (skipdeleted)
	    {
		/* retry with "SEARCH 1:1000 UNSEEN" */
		skipdeleted = FALSE;
		goto restartsearch;
	    }
	    if (!skiprangesearch)
	    {
		/* retry with "SEARCH UNSEEN" */
		skiprangesearch = TRUE;
		goto restartsearch;
	    }
	    /* try with "FETCH 1:n FLAGS" */
	    goto fetchflags;
	}
	if (ok != PS_SUCCESS)
	    return(ok);
	/* loop back only when searching in range */
	if (skiprangesearch)
	    break;
    }
    return(PS_SUCCESS);

fetchflags:
    if (count == 1)
	gen_send(sock, "FETCH %d FLAGS", count);
    else
	gen_send(sock, "FETCH %d:%d FLAGS", 1, count);
    while ((ok = imap_response(sock, buf)) == PS_UNTAGGED)
    {
	unsigned int num;
	int consumed;

	/* expected response format:
	 * IMAP< * 1 FETCH (FLAGS (\Seen))
	 * IMAP< * 2 FETCH (FLAGS (\Seen \Deleted))
	 * IMAP< * 3 FETCH (FLAGS ())
	 * IMAP< * 4 FETCH (FLAGS (\Recent))
	 * IMAP< * 5 FETCH (UID 10 FLAGS (\Recent))
	 */
	if (unseen < count
		&& sscanf(buf, "* %u %n", &num, &consumed) == 1
		&& 0 == strncasecmp(buf+consumed, "FETCH", 5)
		&& isspace((unsigned char)buf[consumed+5])
		&& num >= 1 && num <= (unsigned)count
		&& strstr(buf, "FLAGS ")
		&& !strstr(buf, "\\SEEN")
		&& !strstr(buf, "\\DELETED"))
	{
	    unseen_messages[unseen++] = num;
	    if (outlevel >= O_DEBUG)
		report(stdout, GT_("%u is unseen\n"), num);
	    if (startcount > num)
		startcount = num;
	}
    }
    return(ok);
}

static int imap_getrange(int sock, 
			 struct query *ctl, 
			 const char *folder, 
			 int *countp, int *newp, int *bytes)
/* get range of messages to be fetched */
{
    int ok;

    /* find out how many messages are waiting */
    *bytes = -1;

    if (pass > 1)
    {
	/* deleted mails have already been expunged by
	 * end_mailbox_poll().
	 *
	 * recentcount is already set here by the last imap command which
	 * returned EXISTS on detecting new mail. if recentcount is 0, wait
	 * for new mail.
	 *
	 * this is a while loop because imap_idle() might return on other
	 * mailbox changes also */
	while (recentcount == 0 && do_idle) {
	    smtp_close(ctl, 1);
	    ok = imap_idle(sock);
	    if (ok)
	    {
		report(stderr, GT_("re-poll failed\n"));
		return(ok);
	    }
	}
	/* if recentcount is 0, return no mail */
	if (recentcount == 0)
		count = 0;
	if (outlevel >= O_DEBUG)
	    report(stdout, ngettext("%d message waiting after re-poll\n",
				    "%d messages waiting after re-poll\n",
				    count), count);
    }
    else
    {
	oldcount = count = 0;
	ok = gen_transact(sock, 
			  check_only ? "EXAMINE \"%s\"" : "SELECT \"%s\"",
			  folder ? folder : "INBOX");
	/* imap_ok returns PS_LOCKBUSY for READ-ONLY folders,
	 * which we can safely use in fetchall keep only */
	if (ok == PS_LOCKBUSY && ctl->fetchall && ctl-> keep)
	    ok = 0;

	if (ok != 0)
	{
	    report(stderr, GT_("mailbox selection failed\n"));
	    return(ok);
	}
	else if (outlevel >= O_DEBUG)
	    report(stdout, ngettext("%d message waiting after first poll\n",
				    "%d messages waiting after first poll\n",
				    count), count);

	/*
	 * We should have an expunge here to
	 * a) avoid fetching deleted mails during 'fetchall'
	 * b) getting a wrong count of mails during 'no fetchall'
	 */
	if (!check_only && !ctl->keep && count > 0)
	{
	    ok = internal_expunge(sock);
	    if (ok)
	    {
		report(stderr, GT_("expunge failed\n"));
		return(ok);
	    }
	    if (outlevel >= O_DEBUG)
		report(stdout, ngettext("%d message waiting after expunge\n",
					"%d messages waiting after expunge\n",
					count), count);
	}

	if (count == 0 && do_idle)
	{
	    /* no messages?  then we may need to idle until we get some */
	    while (count == 0) {
		ok = imap_idle(sock);
		if (ok)
		{
		    report(stderr, GT_("re-poll failed\n"));
		    return(ok);
		}
	    }
	    if (outlevel >= O_DEBUG)
		report(stdout, ngettext("%d message waiting after re-poll\n",
					"%d messages waiting after re-poll\n",
					count), count);
	}
    }

    *countp = oldcount = count;
    recentcount = 0;
    startcount = 1;

    /* OK, now get a count of unseen messages and their indices */
    if (!ctl->fetchall && count > 0)
    {
	if (unseen_messages)
	    free(unseen_messages);
	unseen_messages = (unsigned int *)xmalloc(count * sizeof(unsigned int));
	memset(unseen_messages, 0, count * sizeof(unsigned int));
	unseen = 0;

	ok = imap_search(sock, ctl, count);
	if (ok != 0)
	{
	    report(stderr, GT_("search for unseen messages failed\n"));
	    return(ok);
	}

	if (outlevel >= O_DEBUG && unseen > 0)
	    report(stdout, GT_("%u is first unseen\n"), startcount);
    } else
	unseen = -1;

    *newp = unseen;
    expunged = 0;
    deletions = 0;

    return(PS_SUCCESS);
}

static int imap_getpartialsizes(int sock, int first, int last, int *sizes)
/* capture the sizes of messages #first-#last */
{
    char buf [MSGBUFSIZE+1];
    int ok;

    /*
     * Some servers (as in, PMDF5.1-9.1 under OpenVMS 6.1)
     * won't accept 1:1 as valid set syntax.  Some implementors
     * should be taken out and shot for excessive anality.
     *
     * Microsoft Exchange (brain-dead piece of crap that it is) 
     * sometimes gets its knickers in a knot about bodiless messages.
     * You may see responses like this:
     *
     *	fetchmail: IMAP> A0004 FETCH 1:9 RFC822.SIZE
     *	fetchmail: IMAP< * 2 FETCH (RFC822.SIZE 1187)
     *	fetchmail: IMAP< * 3 FETCH (RFC822.SIZE 3954)
     *	fetchmail: IMAP< * 4 FETCH (RFC822.SIZE 1944)
     *	fetchmail: IMAP< * 5 FETCH (RFC822.SIZE 2933)
     *	fetchmail: IMAP< * 6 FETCH (RFC822.SIZE 1854)
     *	fetchmail: IMAP< * 7 FETCH (RFC822.SIZE 34054)
     *	fetchmail: IMAP< * 8 FETCH (RFC822.SIZE 5561)
     *	fetchmail: IMAP< * 9 FETCH (RFC822.SIZE 1101)
     *	fetchmail: IMAP< A0004 NO The requested item could not be found.
     *
     * This means message 1 has only headers.  For kicks and grins
     * you can telnet in and look:
     *	A003 FETCH 1 FULL
     *	A003 NO The requested item could not be found.
     *	A004 fetch 1 rfc822.header
     *	A004 NO The requested item could not be found.
     *	A006 FETCH 1 BODY
     *	* 1 FETCH (BODY ("TEXT" "PLAIN" ("CHARSET" "US-ASCII") NIL NIL "7BIT" 35 3))
     *	A006 OK FETCH completed.
     *
     * To get around this, we treat the final NO as success and count
     * on the fact that the sizes array has been preinitialized with a
     * known-bad size value.
     */

    /* expunges change the fetch numbers */
    first -= expunged;
    last -= expunged;

    if (last == first)
	gen_send(sock, "FETCH %d RFC822.SIZE", last);
    else if (last > first)
	gen_send(sock, "FETCH %d:%d RFC822.SIZE", first, last);
    else /* no unseen messages! */
	return(PS_SUCCESS);
    while ((ok = imap_response(sock, buf)) == PS_UNTAGGED)
    {
	unsigned int size;
	int num;
	int consumed;
	char *ptr;

	/* expected response formats:
	 * IMAP> A0005 FETCH 1 RFC822.SIZE
	 * IMAP< * 1 FETCH (RFC822.SIZE 1187)
	 * IMAP< * 1 FETCH (UID 16 RFC822.SIZE 1447)
	 */
	if (sscanf(buf, "* %d %n", &num, &consumed) == 1
	    && 0 == strncasecmp(buf + consumed, "FETCH", 5)
	    && isspace((unsigned char)buf[consumed + 5])
		&& (ptr = strstr(buf, "RFC822.SIZE "))
		&& sscanf(ptr, "RFC822.SIZE %u", &size) == 1)
	{
	    if (num >= first && num <= last)
		sizes[num - first] = size;
	    else
		report(stderr,
			GT_("Warning: ignoring bogus data for message sizes returned by the server.\n"));
	}
    }
    return(ok);
}

static int imap_getsizes(int sock, int count, int *sizes)
/* capture the sizes of all messages */
{
    return imap_getpartialsizes(sock, 1, count, sizes);
}

static int imap_is_old(int sock, struct query *ctl, int number)
/* is the given message old? */
{
    flag seen = TRUE;
    int i;

    (void)sock;
    (void)ctl;
    /* 
     * Expunges change the fetch numbers, but unseen_messages contains
     * indices from before any expungees were done.  So neither the
     * argument nor the values in message_sequence need to be decremented.
     */

    seen = TRUE;
    for (i = 0; i < unseen; i++)
	if (unseen_messages[i] == (unsigned)number)
	{
	    seen = FALSE;
	    break;
	}

    return(seen);
}

#if 0
static char *skip_token(char *ptr)
{
    while(isspace((unsigned char)*ptr)) ptr++;
    while(!isspace((unsigned char)*ptr) && !iscntrl((unsigned char)*ptr)) ptr++;
    while(isspace((unsigned char)*ptr)) ptr++;
    return(ptr);
}
#endif

static int imap_fetch_headers(int sock, struct query *ctl,int number,int *lenp)
/* request headers of nth message */
{
    char buf [MSGBUFSIZE+1];
    int	num;
    int ok;
    char *ptr;

    (void)ctl;
    /* expunges change the fetch numbers */
    number -= expunged;

    /*
     * This is blessed by RFC1176, RFC1730, RFC2060.
     * According to the RFCs, it should *not* set the \Seen flag.
     */
    gen_send(sock, "FETCH %d RFC822.HEADER", number);

    /* looking for FETCH response */
    if ((ok = imap_response(sock, buf)) == PS_UNTAGGED)
    {
		int consumed;
	/* expected response formats:
	 * IMAP> A0006 FETCH 1 RFC822.HEADER
	 * IMAP< * 1 FETCH (RFC822.HEADER {1360}
	 * IMAP< * 1 FETCH (UID 16 RFC822.HEADER {1360}
	 * IMAP< * 1 FETCH (UID 16 RFC822.SIZE 4029 RFC822.HEADER {1360}
	 */
	if (sscanf(buf, "* %d %n", &num, &consumed) == 1
	    && 0 == strncasecmp(buf + consumed, "FETCH", 5)
	    && isspace((unsigned char)buf[5+consumed])
		&& num == number
		&& (ptr = strstr(buf, "RFC822.HEADER"))
		&& sscanf(ptr, "RFC822.HEADER {%d}%n", lenp, &consumed) == 1
		&& ptr[consumed-1] == '}')
	{
	    return(PS_SUCCESS);
	}

	/* wait for a tagged response */
	imap_ok (sock, 0);

	/* try to recover for some responses */
	if (!strncmp(buf, "* NO", 4) ||
		!strncmp(buf, "* BAD", 5))
	{
	    return(PS_TRANSIENT);
	}

	/* a response which does not match any of the above */
	if (outlevel > O_SILENT)
	    report(stderr, GT_("Incorrect FETCH response: %s.\n"), buf);
	return(PS_ERROR);
    }
    else if (ok == PS_SUCCESS)
    {
	/* an unexpected tagged response */
	if (outlevel > O_SILENT)
	    report(stderr, GT_("Incorrect FETCH response: %s.\n"), buf);
	return(PS_ERROR);
    }
    return(ok);
}

static int imap_fetch_body(int sock, struct query *ctl, int number, int *lenp)
/* request body of nth message */
{
    char buf [MSGBUFSIZE+1], *cp;
    int	num;

    (void)ctl;
    /* expunges change the fetch numbers */
    number -= expunged;

    /*
     * If we're using IMAP4, we can fetch the message without setting its
     * seen flag.  This is good!  It means that if the protocol exchange
     * craps out during the message, it will still be marked `unseen' on
     * the server.
     *
     * According to RFC2060, and Mark Crispin the IMAP maintainer,
     * FETCH %d BODY[TEXT] and RFC822.TEXT are "functionally 
     * equivalent".  However, we know of at least one server that
     * treats them differently in the presence of MIME attachments;
     * the latter form downloads the attachment, the former does not.
     * The server is InterChange, and the fool who implemented this
     * misfeature ought to be strung up by his thumbs.  
     *
     * When I tried working around this by disabling use of the 4rev1 form,
     * I found that doing this breaks operation with M$ Exchange.
     * Annoyingly enough, Exchange's refusal to cope is technically legal
     * under RFC2062.  Trust Microsoft, the Great Enemy of interoperability
     * standards, to find a way to make standards compliance irritating....
     */
    switch (imap_version)
    {
    case IMAP4rev1:	/* RFC 2060 */
	gen_send(sock, "FETCH %d BODY.PEEK[TEXT]", number);
	break;

    case IMAP4:		/* RFC 1730 */
	gen_send(sock, "FETCH %d RFC822.TEXT.PEEK", number);
	break;

    default:		/* RFC 1176 */
	gen_send(sock, "FETCH %d RFC822.TEXT", number);
	break;
    }

    /* looking for FETCH response */
    do {
	int	ok;

	if ((ok = gen_recv(sock, buf, sizeof(buf))))
	    return(ok);
    } while
	(!strstr(buf+4, "FETCH") || sscanf(buf+2, "%d", &num) != 1);

    if (num != number)
	return(PS_ERROR);

    /* Understand "NIL" as length => no body present
     * (MS Exchange, BerliOS Bug #11980) */
    if (strstr(buf+10, "NIL)")) {
	    *lenp = 0;
	    return PS_SUCCESS;
    }

    /*
     * Try to extract a length from the FETCH response.  RFC2060 requires
     * it to be present, but at least one IMAP server (Novell GroupWise)
     * botches this.  The overflow check is needed because of a broken
     * server called dbmail that returns huge garbage lengths.
     */
    if ((cp = strchr(buf, '{'))) {
	long l; char *t;
        errno = 0;
	++ cp;
	l = strtol(cp, &t, 10);
        if (errno || t == cp || (t && !strchr(t, '}')) /* parse error */
		    || l < 0 || l > INT_MAX /* range check */) {
	    *lenp = -1;
	} else {
	    *lenp = l;
	}
    } else {
	*lenp = -1;	/* missing length part in FETCH reponse */
    }

    return PS_SUCCESS;
}

static int imap_trail(int sock, struct query *ctl, const char *tag)
/* discard tail of FETCH response after reading message text */
{
    /* expunges change the fetch numbers */
    /* number -= expunged; */

    (void)ctl;
    (void)tag;

    return imap_ok(sock, NULL);
}

static int imap_delete(int sock, struct query *ctl, int number)
/* set delete flag for given message */
{
    int	ok;

    (void)ctl;
    /* expunges change the fetch numbers */
    number -= expunged;

    /*
     * Use SILENT if possible as a minor throughput optimization.
     * Note: this has been dropped from IMAP4rev1.
     *
     * We set Seen because there are some IMAP servers (notably HP
     * OpenMail) that do message-receipt DSNs, but only when the seen
     * bit is set.  This is the appropriate time -- we get here right
     * after the local SMTP response that says delivery was
     * successful.
     */
    if ((ok = gen_transact(sock,
			imap_version == IMAP4 
				? "STORE %d +FLAGS.SILENT (\\Seen \\Deleted)"
				: "STORE %d +FLAGS (\\Seen \\Deleted)", 
			number)))
	return(ok);
    else
	deletions++;

    /*
     * We do an expunge after expunge_period messages, rather than
     * just before quit, so that a line hit during a long session
     * won't result in lots of messages being fetched again during
     * the next session.
     */
    if (NUM_NONZERO(expunge_period) && (deletions % expunge_period) == 0)
    {
	if ((ok = internal_expunge(sock)))
	    return(ok);
    }

    return(PS_SUCCESS);
}

static int imap_mark_seen(int sock, struct query *ctl, int number)
/* mark the given message as seen */
{
    (void)ctl;

    /* expunges change the message numbers */
    number -= expunged;

    return(gen_transact(sock,
	imap_version == IMAP4
	? "STORE %d +FLAGS.SILENT (\\Seen)"
	: "STORE %d +FLAGS (\\Seen)",
	number));
}

static int imap_end_mailbox_poll(int sock, struct query *ctl)
/* cleanup mailbox before we idle or switch to another one */
{
    (void)ctl;
    if (deletions)
	internal_expunge(sock);
    return(PS_SUCCESS);
}

static int imap_logout(int sock, struct query *ctl)
/* send logout command */
{
    (void)ctl;
    /* if any un-expunged deletions remain, ship an expunge now */
    if (deletions)
	internal_expunge(sock);

#ifdef USE_SEARCH
    /* Memory clean-up */
    if (unseen_messages)
	free(unseen_messages);
#endif /* USE_SEARCH */

    return(gen_transact(sock, "LOGOUT"));
}

static const struct method imap =
{
    "IMAP",		/* Internet Message Access Protocol */
    "imap",		/* service (plain and TLS) */
    "imaps",		/* service (SSL) */
    TRUE,		/* this is a tagged protocol */
    FALSE,		/* no message delimiter */
    imap_ok,		/* parse command response */
    imap_getauth,	/* get authorization */
    imap_getrange,	/* query range of messages */
    imap_getsizes,	/* get sizes of messages (used for ESMTP SIZE option) */
    imap_getpartialsizes,	/* get sizes of subset of messages (used for ESMTP SIZE option) */
    imap_is_old,	/* no UID check */
    imap_fetch_headers,	/* request given message headers */
    imap_fetch_body,	/* request given message body */
    imap_trail,		/* eat message trailer */
    imap_delete,	/* delete the message */
    imap_mark_seen,	/* how to mark a message as seen */
    imap_end_mailbox_poll,	/* end-of-mailbox processing */
    imap_logout,	/* expunge and exit */
    TRUE,		/* yes, we can re-poll */
};

int doIMAP(struct query *ctl)
/* retrieve messages using IMAP Version 2bis or Version 4 */
{
    return(do_protocol(ctl, &imap));
}

/* imap.c ends here */
