/*
 * imap.c -- IMAP2bis/IMAP4 protocol methods
 *
 * Copyright 1997 by Eric S. Raymond
 * For license terms, see the file COPYING in this directory.
 */

#include  "config.h"
#include  <stdio.h>
#include  <string.h>
#include  <ctype.h>
#if defined(STDC_HEADERS)
#include  <stdlib.h>
#endif
#include  "fetchmail.h"
#include  "socket.h"

#include  "i18n.h"

#if OPIE_ENABLE
#endif /* OPIE_ENABLE */

#ifndef strstr		/* glibc-2.1 declares this as a macro */
extern char *strstr();	/* needed on sysV68 R3V7.1. */
#endif /* strstr */

/* imap_version values */
#define IMAP2		-1	/* IMAP2 or IMAP2BIS, RFC1176 */
#define IMAP4		0	/* IMAP4 rev 0, RFC1730 */
#define IMAP4rev1	1	/* IMAP4 rev 1, RFC2060 */

static int count, unseen, deletions, imap_version, preauth; 
static int expunged, expunge_period, saved_timeout;
static flag do_idle;
static char capabilities[MSGBUFSIZE+1];
static unsigned int *unseen_messages;

static int imap_ok(int sock, char *argbuf)
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
	    if (islower(*cp))
		*cp = toupper(*cp);

	/* interpret untagged status responses */
	if (strstr(buf, "* CAPABILITY"))
	    strncpy(capabilities, buf + 12, sizeof(capabilities));
	else if (strstr(buf, "EXISTS"))
	{
	    count = atoi(buf+2);
	    /*
	     * Nasty kluge to handle RFC2177 IDLE.  If we know we're idling
	     * we can't wait for the tag matching the IDLE; we have to tell the
	     * server the IDLE is finished by shipping back a DONE when we
	     * see an EXISTS.  Only after that will a tagged response be
	     * shipped.  The idling flag also gets cleared on a timeout.
	     */
	    if (stage == STAGE_IDLE)
	    {
		/* we do our own write and report here to disable tagging */
		SockWrite(sock, "DONE\r\n", 6);
		if (outlevel >= O_MONITOR)
		    report(stdout, "IMAP> DONE\n");

		mytimeout = saved_timeout;
		stage = STAGE_FETCH;
	    }
	}
	else if (strstr(buf, "PREAUTH"))
	    preauth = TRUE;
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
	else if (!check_only && strstr(buf, "[READ-ONLY]"))
	    return(PS_LOCKBUSY);
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
	for (cp = buf; !isspace(*cp); cp++)
	    continue;
	while (isspace(*cp))
	    cp++;

        if (strncmp(cp, "OK", 2) == 0)
	{
	    if (argbuf)
		strcpy(argbuf, cp);
	    return(PS_SUCCESS);
	}
	else if (strncmp(cp, "BAD", 3) == 0)
	    return(PS_ERROR);
	else if (strncmp(cp, "NO", 2) == 0)
	{
	    if (stage == STAGE_GETAUTH) 
		return(PS_AUTHFAIL);	/* RFC2060, 6.2.2 */
	    else
		return(PS_ERROR);
	}
	else
	    return(PS_PROTOCOL);
    }
}

#if NTLM_ENABLE
#include "ntlm.h"

static tSmbNtlmAuthRequest   request;		   
static tSmbNtlmAuthChallenge challenge;
static tSmbNtlmAuthResponse  response;

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
    char msgbuf[2048];
    int result,len;
  
    gen_send(sock, "AUTHENTICATE NTLM");

    if ((result = gen_recv(sock, msgbuf, sizeof msgbuf)))
	return result;
  
    if (msgbuf[0] != '+')
	return PS_AUTHFAIL;
  
    buildSmbNtlmAuthRequest(&request,ctl->remotename,NULL);

    if (outlevel >= O_DEBUG)
	dumpSmbNtlmAuthRequest(stdout, &request);

    memset(msgbuf,0,sizeof msgbuf);
    to64frombits (msgbuf, (unsigned char*)&request, SmbLength(&request));
  
    if (outlevel >= O_MONITOR)
	report(stdout, "IMAP> %s\n", msgbuf);
  
    strcat(msgbuf,"\r\n");
    SockWrite (sock, msgbuf, strlen (msgbuf));

    if ((gen_recv(sock, msgbuf, sizeof msgbuf)))
	return result;
  
    len = from64tobits ((unsigned char*)&challenge, msgbuf);
    
    if (outlevel >= O_DEBUG)
	dumpSmbNtlmAuthChallenge(stdout, &challenge);
    
    buildSmbNtlmAuthResponse(&challenge, &response,ctl->remotename,ctl->password);
  
    if (outlevel >= O_DEBUG)
	dumpSmbNtlmAuthResponse(stdout, &response);
  
    memset(msgbuf,0,sizeof msgbuf);
    to64frombits (msgbuf, (unsigned char*)&response, SmbLength(&response));

    if (outlevel >= O_MONITOR)
	report(stdout, "IMAP> %s\n", msgbuf);
      
    strcat(msgbuf,"\r\n");
    SockWrite (sock, msgbuf, strlen (msgbuf));
  
    if ((result = gen_recv (sock, msgbuf, sizeof msgbuf)))
	return result;
  
    if (strstr (msgbuf, "OK"))
	return PS_SUCCESS;
    else
	return PS_AUTHFAIL;
}
#endif /* NTLM */

static int imap_canonicalize(char *result, char *raw, int maxlen)
/* encode an IMAP password as per RFC1730's quoting conventions */
{
    int i, j;

    j = 0;
    for (i = 0; i < strlen(raw) && i < maxlen; i++)
    {
	if ((raw[i] == '\\') || (raw[i] == '"'))
	    result[j++] = '\\';
	result[j++] = raw[i];
    }
    result[j] = '\0';

    return(i);
}

static int imap_getauth(int sock, struct query *ctl, char *greeting)
/* apply for connection authorization */
{
    int ok = 0;

    /* probe to see if we're running IMAP4 and can use RFC822.PEEK */
    capabilities[0] = '\0';
    if ((ok = gen_transact(sock, "CAPABILITY")) == PS_SUCCESS)
    {
	/* UW-IMAP server 10.173 notifies in all caps, but RFC2060 says we
	   should expect a response in mixed-case */
	if (strstr(capabilities, "IMAP4REV1") ||
	    strstr(capabilities, "IMAP4rev1"))
	{
	    imap_version = IMAP4rev1;
	    if (outlevel >= O_DEBUG)
		report(stdout, _("Protocol identified as IMAP4 rev 1\n"));
	}
	else
	{
	    imap_version = IMAP4;
	    if (outlevel >= O_DEBUG)
		report(stdout, _("Protocol identified as IMAP4 rev 0\n"));
	}
    }
    else if (ok == PS_ERROR)
    {
	imap_version = IMAP2;
	if (outlevel >= O_DEBUG)
	    report(stdout, _("Protocol identified as IMAP2 or IMAP2BIS\n"));
    }
    else
	return(ok);

    peek_capable = (imap_version >= IMAP4);

    /* 
     * Assumption: expunges are cheap, so we want to do them
     * after every message unless user said otherwise.
     */
    if (NUM_SPECIFIED(ctl->expunge))
	expunge_period = NUM_VALUE_OUT(ctl->expunge);
    else
	expunge_period = 1;

    /* 
     * Handle idling.  We depend on coming through here on startup
     * and after each timeout (including timeouts during idles).
     */
    if (strstr(capabilities, "IDLE") && ctl->idle)
    {
	do_idle = TRUE;
	if (outlevel >= O_VERBOSE)
	    report(stdout, _("will idle after poll\n"));
    }

    /* 
     * If either (a) we saw a PREAUTH token in the greeting, or
     * (b) the user specified ssh preauthentication, then we're done.
     */
    if (preauth || ctl->server.authenticate == A_SSH)
    {
        preauth = FALSE;  /* reset for the next session */
        return(PS_SUCCESS);
    }

    /*
     * Time to authenticate the user.
     * Try the protocol variants that don't require passwords first.
     */
    ok = PS_AUTHFAIL;

#ifdef GSSAPI
    if ((ctl->server.authenticate == A_ANY 
	 || ctl->server.authenticate == A_GSSAPI)
	&& strstr(capabilities, "AUTH=GSSAPI"))
	if(ok = do_gssauth(sock, "AUTHENTICATE", ctl->server.truename, ctl->remotename))
	{
	    /* SASL cancellation of authentication */
	    gen_send(sock, "*");
	    if(ctl->server.authenticate != A_ANY)
                return ok;
	}
	else
	    return ok;
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

    if ((ctl->server.authenticate == A_ANY 
	 || ctl->server.authenticate == A_CRAM_MD5)
	&& strstr(capabilities, "AUTH=CRAM-MD5"))
    {
	if ((ok = do_cram_md5 (sock, "AUTHENTICATE", ctl, NULL)))
	{
	    /* SASL cancellation of authentication */
	    gen_send(sock, "*");
	    if(ctl->server.authenticate != A_ANY)
                return ok;
	}
	else
	    return ok;
    }

#if OPIE_ENABLE
    if ((ctl->server.authenticate == A_ANY 
	 || ctl->server.authenticate == A_OTP)
	&& strstr(capabilities, "AUTH=X-OTP"))
	if ((ok = do_otp(sock, "AUTHENTICATE", ctl)))
	{
	    /* SASL cancellation of authentication */
	    gen_send(sock, "*");
	    if(ctl->server.authenticate != A_ANY)
                return ok;
	}
	else
	    return ok;
#else
    if (ctl->server.authenticate == A_NTLM)
    {
	report(stderr, 
	   _("Required OTP capability not compiled into fetchmail\n"));
    }
#endif /* OPIE_ENABLE */

#ifdef NTLM_ENABLE
    if ((ctl->server.authenticate == A_ANY 
	 || ctl->server.authenticate == A_NTLM) 
	&& strstr (capabilities, "AUTH=NTLM")) {
	if ((ok = do_imap_ntlm(sock, ctl)))
	{
	    /* SASL cancellation of authentication */
	    gen_send(sock, "*");
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
	   _("Required NTLM capability not compiled into fetchmail\n"));
    }
#endif /* NTLM_ENABLE */

#ifdef __UNUSED__	/* The Cyrus IMAP4rev1 server chokes on this */
    /* this handles either AUTH=LOGIN or AUTH-LOGIN */
    if ((imap_version >= IMAP4rev1) && (!strstr(capabilities, "LOGIN")))
    {
	report(stderr, 
	       _("Required LOGIN capability not supported by server\n"));
    }
#endif /* __UNUSED__ */

    /* we're stuck with sending the password en clair */
    if ((ctl->server.authenticate == A_ANY 
 	 || ctl->server.authenticate == A_PASSWORD) 
 	&& !strstr (capabilities, "LOGINDISABLED"))
    {
	/* these sizes guarantee no buffer overflow */
	char	remotename[NAMELEN*2+1], password[PASSWORDLEN*2+1];

	imap_canonicalize(remotename, ctl->remotename, NAMELEN);
	imap_canonicalize(password, ctl->password, PASSWORDLEN);

	strcpy(shroud, ctl->password);
	ok = gen_transact(sock, "LOGIN \"%s\" \"%s\"", remotename, password);
	shroud[0] = '\0';
	if (ok)
	{
	    /* SASL cancellation of authentication */
	    gen_send(sock, "*");
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

    if ((ok = gen_transact(sock, "EXPUNGE")))
	return(ok);

    expunged += deletions;
    deletions = 0;

#ifdef IMAP_UID	/* not used */
    expunge_uids(ctl);
#endif /* IMAP_UID */

    return(PS_SUCCESS);
}

static int imap_idle(int sock)
/* start an RFC2177 IDLE */
{
    stage = STAGE_IDLE;
    saved_timeout = mytimeout;
    mytimeout = 0;

    return (gen_transact(sock, "IDLE"));
}

static int imap_getrange(int sock, 
			 struct query *ctl, 
			 const char *folder, 
			 int *countp, int *newp, int *bytes)
/* get range of messages to be fetched */
{
    int ok;
    char buf[MSGBUFSIZE+1], *cp;

    /* find out how many messages are waiting */
    *bytes = -1;

    if (pass > 1)
    {
	/* 
	 * We have to have an expunge here, otherwise the re-poll will
	 * infinite-loop picking up un-expunged messages -- unless the
	 * expunge period is one and we've been nuking each message 
	 * just after deletion.
	 */
	ok = 0;
	if (deletions && expunge_period != 1)
	    ok = internal_expunge(sock);
	count = -1;
	if (do_idle)
	    ok = imap_idle(sock);
	if (ok || gen_transact(sock, "NOOP"))
	{
	    report(stderr, _("re-poll failed\n"));
	    return(ok);
	}
	else if (count == -1)	/* no EXISTS response to NOOP/IDLE */
	{
	    count = 0;
	}
	if (outlevel >= O_DEBUG)
	    report(stdout, _("%d messages waiting after re-poll\n"), count);
    }
    else
    {
	ok = gen_transact(sock, 
			  check_only ? "EXAMINE \"%s\"" : "SELECT \"%s\"",
			  folder ? folder : "INBOX");
	if (ok != 0)
	{
	    report(stderr, _("mailbox selection failed\n"));
	    return(ok);
	}
	else if (outlevel >= O_DEBUG)
	    report(stdout, _("%d messages waiting after first poll\n"), count);

	/* no messages?  then we may need to idle until we get some */
	if (count == 0 && do_idle)
	    imap_idle(sock);
    }

    *countp = count;

    /* OK, now get a count of unseen messages and their indices */
    if (!ctl->fetchall && count > 0)
    {
	if (unseen_messages)
	    free(unseen_messages);
	unseen_messages = xmalloc(count * sizeof(unsigned int));
	memset(unseen_messages, 0, count * sizeof(unsigned int));
	unseen = 0;

	gen_send(sock, "SEARCH UNSEEN");
	do {
	    ok = gen_recv(sock, buf, sizeof(buf));
	    if (ok != 0)
	    {
		report(stderr, _("search for unseen messages failed\n"));
		return(PS_PROTOCOL);
	    }
	    else if ((cp = strstr(buf, "* SEARCH")))
	    {
		char	*ep;

		cp += 8;	/* skip "* SEARCH" */

		while (*cp && unseen < count)
		{
		    /* skip whitespace */
		    while (*cp && isspace(*cp))
			cp++;
		    if (*cp) 
		    {
			/*
			 * Message numbers are between 1 and 2^32 inclusive,
			 * so unsigned int is large enough.
			 */
			unseen_messages[unseen]=(unsigned int)strtol(cp,&ep,10);

			if (outlevel >= O_DEBUG)
			    report(stdout, 
				   _("%u is unseen\n"), 
				   unseen_messages[unseen]);
		
			unseen++;
			cp = ep;
		    }
		}
	    }
	} while
	    (tag[0] != '\0' && strncmp(buf, tag, strlen(tag)));
    } else
	unseen = -1;

    *newp = unseen;
    expunged = 0;

    return(PS_SUCCESS);
}

static int imap_getsizes(int sock, int count, int *sizes)
/* capture the sizes of all messages */
{
    char buf [MSGBUFSIZE+1];

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
     * To get around this, we terminate the read loop on a NO and count
     * on the fact that the sizes array has been preinitialized with a
     * known-bad size value.
     */
    if (count == 1)
	gen_send(sock, "FETCH 1 RFC822.SIZE", count);
    else
	gen_send(sock, "FETCH 1:%d RFC822.SIZE", count);
    for (;;)
    {
	unsigned int num, size;
	int ok;

	if ((ok = gen_recv(sock, buf, sizeof(buf))))
	    return(ok);
	else if (strstr(buf, "OK") || strstr(buf, "NO"))
	    break;
	else if (sscanf(buf, "* %u FETCH (RFC822.SIZE %u)", &num, &size) == 2) {
	    if (num > 0 && num <= count)
	        sizes[num - 1] = size;
	    /* else, strict: protocol error, flexible: nothing
	     * I vote for flexible. */
	}
    }

    return(PS_SUCCESS);
}

static int imap_is_old(int sock, struct query *ctl, int number)
/* is the given message old? */
{
    flag seen = TRUE;
    int i;

    /* 
     * Expunges change the fetch numbers, but unseen_messages contains
     * indices from before any expungees were done.  So neither the
     * argument nor the values in message_sequence need to be decremented.
     */

    seen = TRUE;
    for (i = 0; i < unseen; i++)
	if (unseen_messages[i] == number)
	{
	    seen = FALSE;
	    break;
	}

    return(seen);
}

static char *skip_token(char *ptr)
{
    while(isspace(*ptr)) ptr++;
    while(!isspace(*ptr) && !iscntrl(*ptr)) ptr++;
    while(isspace(*ptr)) ptr++;
    return(ptr);
}

static int imap_fetch_headers(int sock, struct query *ctl,int number,int *lenp)
/* request headers of nth message */
{
    char buf [MSGBUFSIZE+1];
    int	num;

    /* expunges change the fetch numbers */
    number -= expunged;

    /*
     * This is blessed by RFC1176, RFC1730, RFC2060.
     * According to the RFCs, it should *not* set the \Seen flag.
     */
    gen_send(sock, "FETCH %d RFC822.HEADER", number);

    /* looking for FETCH response */
    for (;;) 
    {
	int	ok;
	char	*ptr;

	if ((ok = gen_recv(sock, buf, sizeof(buf))))
	    return(ok);
 	ptr = skip_token(buf);	/* either "* " or "AXXXX " */
 	if (sscanf(ptr, "%d FETCH (%*s {%d}", &num, lenp) == 2)
  	    break;
	/* try to recover from chronically fucked-up M$ Exchange servers */
 	else if (!strncmp(ptr, "NO", 2))
 	    return(PS_TRANSIENT);
 	else if (!strncmp(ptr, "BAD", 3))
 	    return(PS_TRANSIENT);
    }

    if (num != number)
	return(PS_ERROR);
    else
	return(PS_SUCCESS);
}

static int imap_fetch_body(int sock, struct query *ctl, int number, int *lenp)
/* request body of nth message */
{
    char buf [MSGBUFSIZE+1], *cp;
    int	num;

    /* expunges change the fetch numbers */
    number -= expunged;

    /*
     * If we're using IMAP4, we can fetch the message without setting its
     * seen flag.  This is good!  It means that if the protocol exchange
     * craps out during the message, it will still be marked `unseen' on
     * the server.
     *
     * However...*don't* do this if we're using keep to suppress deletion!
     * In that case, marking the seen flag is the only way to prevent the
     * message from being re-fetched on subsequent runs (and according
     * to RFC2060 p.43 this fetch should set Seen as a side effect).
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
	if (!ctl->keep)
	    gen_send(sock, "FETCH %d BODY.PEEK[TEXT]", number);
	else
	    gen_send(sock, "FETCH %d BODY[TEXT]", number);
	break;

    case IMAP4:		/* RFC 1730 */
	if (!ctl->keep)
	    gen_send(sock, "FETCH %d RFC822.TEXT.PEEK", number);
	else
	    gen_send(sock, "FETCH %d RFC822.TEXT", number);
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

    /*
     * Try to extract a length from the FETCH response.  RFC2060 requires
     * it to be present, but at least one IMAP server (Novell GroupWise)
     * botches this.
     */
    if ((cp = strchr(buf, '{')))
	*lenp = atoi(cp + 1);
    else
	*lenp = -1;	/* missing length part in FETCH reponse */

    return(PS_SUCCESS);
}

static int imap_trail(int sock, struct query *ctl, int number)
/* discard tail of FETCH response after reading message text */
{
    /* expunges change the fetch numbers */
    /* number -= expunged; */

    for (;;)
    {
	char buf[MSGBUFSIZE+1];
	int ok;

	if ((ok = gen_recv(sock, buf, sizeof(buf))))
	    return(ok);

	/* UW IMAP returns "OK FETCH", Cyrus returns "OK Completed" */
	if (strstr(buf, "OK"))
	    break;

#ifdef __UNUSED__
	/*
	 * Any IMAP server that fails to set Seen on a BODY[TEXT]
	 * fetch violates RFC2060 p.43 (top).  This becomes an issue
	 * when keep is on, because seen messages aren't deleted and
	 * get refetched on each poll.  As a workaround, if keep is on
	 * we can set the Seen flag explicitly.
	 *
	 * This code isn't used yet because we don't know of any IMAP
	 * servers broken in this way.
	 */
	if (ctl->keep)
	    if ((ok = gen_transact(sock,
			imap_version == IMAP4 
				? "STORE %d +FLAGS.SILENT (\\Seen)"
				: "STORE %d +FLAGS (\\Seen)", 
			number)))
		return(ok);
#endif /* __UNUSED__ */
    }

    return(PS_SUCCESS);
}

static int imap_delete(int sock, struct query *ctl, int number)
/* set delete flag for given message */
{
    int	ok;

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
	internal_expunge(sock);

    return(PS_SUCCESS);
}

static int imap_logout(int sock, struct query *ctl)
/* send logout command */
{
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

const static struct method imap =
{
    "IMAP",		/* Internet Message Access Protocol */
#if INET6_ENABLE
    "imap",
    "imaps",
#else /* INET6_ENABLE */
    143,                /* standard IMAP2bis/IMAP4 port */
    993,                /* ssl IMAP2bis/IMAP4 port */
#endif /* INET6_ENABLE */
    TRUE,		/* this is a tagged protocol */
    FALSE,		/* no message delimiter */
    imap_ok,		/* parse command response */
    imap_getauth,	/* get authorization */
    imap_getrange,	/* query range of messages */
    imap_getsizes,	/* get sizes of messages (used for ESMTP SIZE option) */
    imap_is_old,	/* no UID check */
    imap_fetch_headers,	/* request given message headers */
    imap_fetch_body,	/* request given message body */
    imap_trail,		/* eat message trailer */
    imap_delete,	/* delete the message */
    imap_logout,	/* expunge and exit */
    TRUE,		/* yes, we can re-poll */
};

int doIMAP(struct query *ctl)
/* retrieve messages using IMAP Version 2bis or Version 4 */
{
    return(do_protocol(ctl, &imap));
}

/* imap.c ends here */
