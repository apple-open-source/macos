/*
 * sink.c -- forwarding/delivery support for fetchmail
 *
 * The interface of this module (open_sink(), stuff_line(), close_sink(),
 * release_sink()) seals off the delivery logic from the protocol machine,
 * so the latter won't have to care whether it's shipping to an [SL]MTP
 * listener daemon or an MDA pipe.
 *
 * Copyright 1998 by Eric S. Raymond
 * For license terms, see the file COPYING in this directory.
 */

#include  "config.h"
#include  <stdio.h>
#include  <errno.h>
#include  <string.h>
#include  <signal.h>
#ifdef HAVE_MEMORY_H
#include  <memory.h>
#endif /* HAVE_MEMORY_H */
#if defined(STDC_HEADERS)
#include  <stdlib.h>
#endif
#if defined(HAVE_UNISTD_H)
#include  <unistd.h>
#endif
#if defined(HAVE_STDARG_H)
#include  <stdarg.h>
#else
#include  <varargs.h>
#endif
#include  <ctype.h>
#include  <langinfo.h>

#include  "fetchmail.h"

/* for W* macros after pclose() */
#define _USE_BSD
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include  "socket.h"
#include  "smtp.h"
#include  "i18n.h"

/* BSD portability hack...I know, this is an ugly place to put it */
#if !defined(SIGCHLD) && defined(SIGCLD)
#define SIGCHLD	SIGCLD
#endif

/* makes the open_sink()/close_sink() pair non-reentrant */
static int lmtp_responses;

void smtp_close(struct query *ctl, int sayquit)
/* close the socket to SMTP server */
{
    if (ctl->smtp_socket != -1)
    {
	if (sayquit)
	    SMTP_quit(ctl->smtp_socket, ctl->smtphostmode);
	SockClose(ctl->smtp_socket);
	ctl->smtp_socket = -1;
    }
    batchcount = 0;
}

int smtp_open(struct query *ctl)
/* try to open a socket to the appropriate SMTP server for this query */ 
{
    /* maybe it's time to close the socket in order to force delivery */
    if (last_smtp_ok > 0 && time((time_t *)NULL) - last_smtp_ok > mytimeout)
    {
	smtp_close(ctl, 1);
	last_smtp_ok = 0;
    }
    if (NUM_NONZERO(ctl->batchlimit)) {
	if (batchcount == ctl->batchlimit)
	    smtp_close(ctl, 1);
	batchcount++;
    }

    /* if no socket to any SMTP host is already set up, try to open one */
    if (ctl->smtp_socket == -1) 
    {
	/* 
	 * RFC 1123 requires that the domain name in HELO address is a
	 * "valid principal domain name" for the client host. If we're
	 * running in invisible mode, violate this with malice
	 * aforethought in order to make the Received headers and
	 * logging look right.
	 *
	 * In fact this code relies on the RFC1123 requirement that the
	 * SMTP listener must accept messages even if verification of the
	 * HELO name fails (RFC1123 section 5.2.5, paragraph 2).
	 *
	 * How we compute the true mailhost name to pass to the
	 * listener doesn't affect behavior on RFC1123-violating
	 * listeners that check for name match; we're going to lose
	 * on those anyway because we can never give them a name
	 * that matches the local machine fetchmail is running on.
	 * What it will affect is the listener's logging.
	 */
	struct idlist	*idp;
	const char *id_me = run.invisible ? ctl->server.truename : fetchmailhost;
	int oldphase = phase;
	char *parsed_host = NULL;

	errno = 0;

	/*
	 * Run down the SMTP hunt list looking for a server that's up.
	 * Use both explicit hunt entries (value TRUE) and implicit 
	 * (default) ones (value FALSE).
	 */
	oldphase = phase;
	phase = LISTENER_WAIT;

	set_timeout(ctl->server.timeout);
	for (idp = ctl->smtphunt; idp; idp = idp->next)
	{
	    char	*cp;
	    char	*portnum = SMTP_PORT;

	    ctl->smtphost = idp->id;  /* remember last host tried. */
	    if (ctl->smtphost[0]=='/')
	    {
		ctl->smtphostmode = LMTP_MODE;
		xfree(parsed_host);
		if ((ctl->smtp_socket = UnixOpen(ctl->smtphost))==-1)
		    continue;
	    }
	    else
	    {
		ctl->smtphostmode = ctl->listener;
		parsed_host = xstrdup(idp->id);
		if ((cp = strrchr(parsed_host, '/')))
		{
		    *cp++ = 0;
		    if (cp[0])
			portnum = cp;
		}
		if ((ctl->smtp_socket = SockOpen(parsed_host,portnum,
				ctl->server.plugout, &ai1)) == -1)
		{
		    xfree(parsed_host);
		    continue;
		}
	    }

	    /* return immediately for ODMR */
	    if (ctl->server.protocol == P_ODMR)
	    {
		set_timeout(0);
		phase = oldphase;
		xfree(parsed_host);
		return(ctl->smtp_socket); /* success */
	    }

	    /* first, probe for ESMTP */
	    if (SMTP_ok(ctl->smtp_socket, ctl->smtphostmode, TIMEOUT_STARTSMTP) == SM_OK &&
		    SMTP_ehlo(ctl->smtp_socket, ctl->smtphostmode, id_me,
			ctl->server.esmtp_name, ctl->server.esmtp_password,
			&ctl->server.esmtp_options) == SM_OK)
		break;  /* success */

	    /*
	     * RFC 1869 warns that some listeners hang up on a failed EHLO,
	     * so it's safest not to assume the socket will still be good.
	     */
	    smtp_close(ctl, 0);

	    /* if opening for ESMTP failed, try SMTP */
	    if (ctl->smtphost[0]=='/')
	    {
		if ((ctl->smtp_socket = UnixOpen(ctl->smtphost))==-1)
		    continue;
	    }
	    else
	    {
		if ((ctl->smtp_socket = SockOpen(parsed_host,portnum,
				ctl->server.plugout, &ai1)) == -1)
		{
		    xfree(parsed_host);
		    continue;
		}
	    }

	    if (SMTP_ok(ctl->smtp_socket, ctl->smtphostmode, TIMEOUT_STARTSMTP) == SM_OK &&
		    SMTP_helo(ctl->smtp_socket, ctl->smtphostmode, id_me) == SM_OK)
		break;  /* success */

	    smtp_close(ctl, 0);
	}
	set_timeout(0);
	phase = oldphase;

	/*
	 * RFC 1123 requires that the domain name part of the
	 * RCPT TO address be "canonicalized", that is a FQDN
	 * or MX but not a CNAME.  Some listeners (like exim)
	 * enforce this.  Now that we have the actual hostname,
	 * compute what we should canonicalize with.
	 */
	xfree(ctl->destaddr);
	if (ctl->smtpaddress)
	    ctl->destaddr = xstrdup(ctl->smtpaddress);
	/* parsed_host is smtphost without the /port */
	else if (parsed_host && parsed_host[0] != 0)
	    ctl->destaddr = xstrdup(parsed_host);
	/* No smtphost is specified or it is a UNIX socket, then use
	   localhost as a domain part. */
	else
	    ctl->destaddr = xstrdup("localhost");
	xfree(parsed_host);
    }
    /* end if (ctl->smtp_socket == -1) */

    if (outlevel >= O_DEBUG && ctl->smtp_socket != -1)
	report(stdout, GT_("forwarding to %s\n"), ctl->smtphost);

    return(ctl->smtp_socket);
}

static void sanitize(char *s)
/* replace ' by _ */
{
    char *cp;

    for (cp = s; (cp = strchr (cp, '\'')); cp++)
    	*cp = '_';
}

char *rcpt_address(struct query *ctl, const char *id,
			  int usesmtpname)
{
    static char addr[HOSTLEN+USERNAMELEN+1];
    if (strchr(id, '@'))
    {
	snprintf(addr, sizeof (addr), "%s", id);
    }
    else if (usesmtpname && ctl->smtpname)
    {
	snprintf(addr, sizeof (addr), "%s", ctl->smtpname);
    }
    else
    {
	snprintf(addr, sizeof (addr), "%s@%s", id, ctl->destaddr);
    }
    return addr;
}

static int send_bouncemail(struct query *ctl, struct msgblk *msg,
			   int userclass, char *message /* should have \r\n at the end */,
			   int nerrors, char *errors[])
/* bounce back an error report a la RFC 1892 */
{
    char daemon_name[15 + HOSTLEN] = "MAILER-DAEMON@";
    char boundary[BUFSIZ], *bounce_to;
    int sock;
    static char *fqdn_of_host = NULL;
    const char *md1 = "MAILER-DAEMON", *md2 = "MAILER-DAEMON@";

    /* don't bounce in reply to undeliverable bounces */
    if (!msg || !msg->return_path[0] ||
	strcmp(msg->return_path, "<>") == 0 ||
	strcasecmp(msg->return_path, md1) == 0 ||
	strncasecmp(msg->return_path, md2, strlen(md2)) == 0)
	return(TRUE);

    bounce_to = (run.bouncemail ? msg->return_path : run.postmaster);

    /* can't just use fetchmailhost here, it might be localhost */
    if (fqdn_of_host == NULL)
	fqdn_of_host = host_fqdn(0); /* can't afford to bail out and
					lose the NDN here */
    strlcat(daemon_name, fqdn_of_host, sizeof(daemon_name));

    /* we need only SMTP for this purpose */
    /* XXX FIXME: hardcoding localhost is nonsense if smtphost can be
     * configured */
    if ((sock = SockOpen("localhost", SMTP_PORT, NULL, &ai1)) == -1)
	return(FALSE);

    if (SMTP_ok(sock, SMTP_MODE, TIMEOUT_STARTSMTP) != SM_OK)
    {
	SockClose(sock);
	return FALSE;
    }

    if (SMTP_helo(sock, SMTP_MODE, fetchmailhost) != SM_OK
	|| SMTP_from(sock, SMTP_MODE, "<>", (char *)NULL) != SM_OK
	|| SMTP_rcpt(sock, SMTP_MODE, bounce_to) != SM_OK
	|| SMTP_data(sock, SMTP_MODE) != SM_OK)
    {
	SMTP_quit(sock, SMTP_MODE);
	SockClose(sock);
	return(FALSE);
    }

    /* our first duty is to keep the sacred foo counters turning... */
    snprintf(boundary, sizeof(boundary), "foo-mani-padme-hum-%ld-%ld-%ld", 
	    (long)getpid(), (long)getppid(), (long)time(NULL));

    if (outlevel >= O_VERBOSE)
	report(stdout, GT_("SMTP: (bounce-message body)\n"));
    else
	/* this will usually go to sylog... */
	report(stderr, GT_("mail from %s bounced to %s\n"),
	       daemon_name, bounce_to);


    /* bouncemail headers */
    SockPrintf(sock, "Subject: Mail delivery failed: returning message to sender\r\n");
    SockPrintf(sock, "From: Mail Delivery System <%s>\r\n", daemon_name);
    SockPrintf(sock, "To: %s\r\n", bounce_to);
    SockPrintf(sock, "MIME-Version: 1.0\r\n");
    SockPrintf(sock, "Content-Type: multipart/report; report-type=delivery-status;\r\n\tboundary=\"%s\"\r\n", boundary);
    SockPrintf(sock, "\r\n");

    /* RFC1892 part 1 -- human-readable message */
    SockPrintf(sock, "--%s\r\n", boundary); 
    SockPrintf(sock,"Content-Type: text/plain\r\n");
    SockPrintf(sock, "\r\n");
    SockPrintf(sock, "This message was created automatically by mail delivery software.\r\n");
    SockPrintf(sock, "\r\n");
    SockPrintf(sock, "A message that you sent could not be delivered to one or more of its\r\n");
    SockPrintf(sock, "recipients. This is a permanent error.\r\n");
    SockPrintf(sock, "\r\n");
    SockPrintf(sock, "Reason: %s", message);
    SockPrintf(sock, "\r\n");
    SockPrintf(sock, "The following address(es) failed:\r\n");

    if (nerrors)
    {
	struct idlist	*idp;
	int		nusers;
	
        nusers = 0;
        for (idp = msg->recipients; idp; idp = idp->next)
        {
            if (idp->val.status.mark == userclass)
            {
                char	*error;
                SockPrintf(sock, "%s\r\n", rcpt_address (ctl, idp->id, 1));
                
                if (nerrors == 1) error = errors[0];
                else if (nerrors <= nusers)
                {
                    SockPrintf(sock, "Internal error: SMTP error count doesn't match number of recipients.\r\n");
                    break;
                }
                else error = errors[nusers++];
                        
                SockPrintf(sock, "   SMTP error: %s\r\n\r\n", error);
            }
        }
    
	/* RFC1892 part 2 -- machine-readable responses */
	SockPrintf(sock, "--%s\r\n", boundary); 
	SockPrintf(sock,"Content-Type: message/delivery-status\r\n");
	SockPrintf(sock, "\r\n");
	SockPrintf(sock, "Reporting-MTA: dns; %s\r\n", fqdn_of_host);

	nusers = 0;
	for (idp = msg->recipients; idp; idp = idp->next)
	    if (idp->val.status.mark == userclass)
	    {
		char	*error;
		/* Minimum RFC1894 compliance + Diagnostic-Code field */
		SockPrintf(sock, "\r\n");
		SockPrintf(sock, "Final-Recipient: rfc822; %s\r\n", 
			   rcpt_address (ctl, idp->id, 1));
		SockPrintf(sock, "Last-Attempt-Date: %s\r\n", rfc822timestamp());
		SockPrintf(sock, "Action: failed\r\n");

		if (nerrors == 1)
		    /* one error applies to all users */
		    error = errors[0];
		else if (nerrors <= nusers)
		{
		    SockPrintf(sock, "Internal error: SMTP error count doesn't match number of recipients.\r\n");
		    break;
		}
		else
		    /* errors correspond 1-1 to selected users */
		    error = errors[nusers++];
		
		if (strlen(error) > 9 && isdigit((unsigned char)error[4])
			&& error[5] == '.' && isdigit((unsigned char)error[6])
			&& error[7] == '.' && isdigit((unsigned char)error[8]))
		    /* Enhanced status code available, use it */
		    SockPrintf(sock, "Status: %5.5s\r\n", &(error[4]));
		else
		    /* Enhanced status code not available, fake one */
		    SockPrintf(sock, "Status: %c.0.0\r\n", error[0]);
		SockPrintf(sock, "Diagnostic-Code: %s\r\n", error);
	    }
	SockPrintf(sock, "\r\n");
    }

    /* RFC1892 part 3 -- headers of undelivered message */
    SockPrintf(sock, "--%s\r\n", boundary); 
    SockPrintf(sock, "Content-Type: text/rfc822-headers\r\n");
    SockPrintf(sock, "\r\n");
    if (msg->headers)
    {
	SockWrite(sock, msg->headers, strlen(msg->headers));
	SockPrintf(sock, "\r\n");
    }
    SockPrintf(sock, "--%s--\r\n", boundary); 

    if (SMTP_eom(sock, SMTP_MODE) != SM_OK
	    || SMTP_quit(sock, SMTP_MODE) != SM_OK)
    {
	SockClose(sock);
	return(FALSE);
    }

    SockClose(sock);

    return(TRUE);
}

static int handle_smtp_report(struct query *ctl, struct msgblk *msg)
/* handle SMTP errors based on the content of SMTP_response */
/* returns either PS_REFUSED (to delete message from the server),
 *             or PS_TRANSIENT (keeps the message on the server) */
{
    int smtperr = atoi(smtp_response);
    char *responses[1];
    struct idlist *walk;
    int found = 0;

    responses[0] = xstrdup(smtp_response);

#ifdef __UNUSED__
    /*
     * Don't do this!  It can really mess you up if, for example, you're
     * reporting an error with a single RCPT TO address among several;
     * RSET discards the message body and it doesn't get sent to the
     * valid recipients.
     */
    SMTP_rset(ctl->smtp_socket);    /* stay on the safe side */
    if (outlevel >= O_DEBUG)
	report(stdout, GT_("Saved error is still %d\n"), smtperr);
#endif /* __UNUSED */

    /*
     * Note: send_bouncemail message strings are not made subject
     * to gettext translation because (a) they're going to be 
     * embedded in a text/plain 7bit part, and (b) they're
     * going to be associated with listener error-response
     * messages, which are probably in English (none of the
     * MTAs I know about are internationalized).
     */
    for( walk = ctl->antispam; walk; walk = walk->next )
        if ( walk->val.status.num == smtperr ) 
	{ 
		found=1;
		break;
	}

    /* if (str_find(&ctl->antispam, smtperr)) */
    if ( found )
    {
	/*
	 * SMTP listener explicitly refuses to deliver mail
	 * coming from this address, probably due to an
	 * anti-spam domain exclusion.  Respect this.  Don't
	 * try to ship the message, and don't prevent it from
	 * being deleted.  There's no point in bouncing the
	 * email either since most spammers don't put their
	 * real return email address anywhere in the headers
	 * (unless the user insists with the SET SPAMBOUNCE
	 * config option).
	 *
	 * Default values:
	 *
	 * 571 = sendmail's "unsolicited email refused"
	 * 550 = exim's new antispam response (temporary)
	 * 501 = exim's old antispam response
	 * 554 = Postfix antispam response.
	 *
	 */
	if (run.spambounce)
	{
	    char rejmsg[160];
	    snprintf(rejmsg, sizeof(rejmsg),
		    "spam filter or virus scanner rejected message because:\r\n"
		    "%s\r\n", responses[0]);

	    send_bouncemail(ctl, msg, XMIT_ACCEPT,
		    rejmsg, 1, responses);
	}
	free(responses[0]);
	return(PS_REFUSED);
    }

    /*
     * Suppress error message only if the response specifically 
     * meant `excluded for policy reasons'.  We *should* see
     * an error when the return code is less specific.
     */
    if (smtperr >= 400)
	report(stderr, GT_("%cMTP error: %s\n"), 
	      ctl->smtphostmode,
	      responses[0]);

    switch (smtperr)
    {
    case 552: /* message exceeds fixed maximum message size */
	/*
	 * Permanent no-go condition on the
	 * ESMTP server.  Don't try to ship the message, 
	 * and allow it to be deleted.
	 */
	if (run.bouncemail)
	    send_bouncemail(ctl, msg, XMIT_ACCEPT,
			"This message was too large (SMTP error 552).\r\n", 
			1, responses);
	free(responses[0]);
	return(PS_REFUSED);
  
    case 553: /* invalid sending domain */
	/*
	 * These latter days 553 usually means a spammer is trying to
	 * cover his tracks.  We never bouncemail on these, because 
	 * (a) the return address is invalid by definition, and 
	 * (b) we wouldn't want spammers to get confirmation that
	 * this address is live, anyway.
	 */
#ifdef __DONT_FEED_THE_SPAMMERS__
	if (run.bouncemail)
	    send_bouncemail(ctl, msg, XMIT_ACCEPT,
			"Invalid address in MAIL FROM (SMTP error 553).\r\n", 
			1, responses);
#endif /* __DONT_FEED_THE_SPAMMERS__ */
	free(responses[0]);
	return(PS_REFUSED);

    case 530: /* must issue STARTTLS error */
	/*
	 * Some SMTP servers insist on encrypted communication
	 * Let's set PS_TRANSIENT, otherwise all messages to be sent
	 * over such server would be blackholed - see RFC 3207.
	 */
	if (outlevel > O_SILENT)
		report_complete(stdout,
				GT_("SMTP server requires STARTTLS, keeping message.\n"));
	free(responses[0]);
	return(PS_TRANSIENT);

    default:
	/* bounce non-transient errors back to the sender */
	if (smtperr >= 500 && smtperr <= 599)
	{
	    if (run.bouncemail)
		send_bouncemail(ctl, msg, XMIT_ACCEPT,
				"General SMTP/ESMTP error.\r\n", 
				1, responses);
	    free(responses[0]);
	    return(PS_REFUSED);
	}
	/*
	 * We're going to end up here on 4xx errors, like:
	 *
	 * 451: temporarily unable to identify sender (exim)
	 * 452: temporary out-of-queue-space condition on the ESMTP server.
	 *
	 * These are temporary errors.  Don't try to ship the message,
	 * and suppress deletion so it can be retried on a future
	 * retrieval cycle.
	 *
	 * Bouncemail *might* be appropriate here as a delay
	 * notification (note; if we ever add this, we must make
	 * sure the RFC1894 Action field is "delayed" rather than
	 * "failed").  But it's not really necessary because
	 * these are not actual failures, we're very likely to be
	 * able to recover on the next cycle.
	 */
	free(responses[0]);
	return(PS_TRANSIENT);
    }
}

static int handle_smtp_report_without_bounce(struct query *ctl, struct msgblk *msg)
/* handle SMTP errors based on the content of SMTP_response */
/* atleast one PS_TRANSIENT: do not send the bounce mail, keep the mail;
 * no PS_TRANSIENT, atleast one PS_SUCCESS: send the bounce mail, delete the mail;
 * no PS_TRANSIENT, no PS_SUCCESS: do not send the bounce mail, delete the mail */
{
    int smtperr = atoi(smtp_response);

    (void)msg;

    if (str_find(&ctl->antispam, smtperr))
    {
	if (run.spambounce)
	 return(PS_SUCCESS);
	return(PS_REFUSED);
    }

    if (smtperr >= 400)
	report(stderr, GT_("%cMTP error: %s\n"), 
	      ctl->smtphostmode,
	      smtp_response);

    switch (smtperr)
    {
    case 552: /* message exceeds fixed maximum message size */
	if (run.bouncemail)
	    return(PS_SUCCESS);
	return(PS_REFUSED);

    case 553: /* invalid sending domain */
#ifdef __DONT_FEED_THE_SPAMMERS__
	if (run.bouncemail)
	    return(PS_SUCCESS);
#endif /* __DONT_FEED_THE_SPAMMERS__ */
	return(PS_REFUSED);

    default:
	/* bounce non-transient errors back to the sender */
	if (smtperr >= 500 && smtperr <= 599)
	    return(PS_SUCCESS);
	return(PS_TRANSIENT);
    }
}

/* these are shared by open_sink and stuffline */
static FILE *sinkfp;

int stuffline(struct query *ctl, char *buf)
/* ship a line to the given control block's output sink (SMTP server or MDA) */
{
    int	n, oldphase;
    char *last;

    /* The line may contain NUL characters. Find the last char to use
     * -- the real line termination is the sequence "\n\0".
     */
    last = buf + 1; /* last[-1] must be valid! */
    while ((last += strlen(last)) && (last[-1] != '\n'))
        last++;

    /* fix message lines that have only \n termination (for qmail) */
    if (ctl->forcecr)
    {
        if (last - 1 == buf || last[-2] != '\r')
	{
	    last[-1] = '\r';
	    *last++  = '\n';
	    *last    = '\0';
	}
    }

    oldphase = phase;
    phase = FORWARDING_WAIT;

    /*
     * SMTP byte-stuffing.  We only do this if the protocol does *not*
     * use .<CR><LF> as EOM.  If it does, the server will already have
     * decorated any . lines it sends back up.
     */
    if (*buf == '.')
    {
	if (ctl->server.base_protocol->delimited)	/* server has already byte-stuffed */
	{
	    if (ctl->mda) {
		/* writing to MDA, undo byte-stuffing */
		++buf;
	    } else {
		/* writing to SMTP, leave the byte-stuffing in place */;
	    }
	}
        else /* if (!protocol->delimited)	-- not byte-stuffed already */
	{
	    /* byte-stuff it */
	    if (!ctl->mda)  {
		if (!ctl->bsmtp) {
		    n = SockWrite(ctl->smtp_socket, buf, 1);
		} else {
		    n = fwrite(buf, 1, 1, sinkfp);
		    if (ferror(sinkfp)) n = -1;
		}
		if (n < 0)
		    return n;
	    }
	}
    }

    /* we may need to strip carriage returns */
    if (ctl->stripcr)
    {
	char	*sp, *tp;

	for (sp = tp = buf; sp < last; sp++)
	    if (*sp != '\r')
		*tp++ =  *sp;
	*tp = '\0';
        last = tp;
    }

    n = 0;
    if (ctl->mda || ctl->bsmtp) {
	n = fwrite(buf, 1, last - buf, sinkfp);
	if (ferror(sinkfp)) n = -1;
    } else if (ctl->smtp_socket != -1)
	n = SockWrite(ctl->smtp_socket, buf, last - buf);

    phase = oldphase;

    return(n);
}

static int open_bsmtp_sink(struct query *ctl, struct msgblk *msg,
	      int *good_addresses, int *bad_addresses)
/* open a BSMTP stream */
{
    struct	idlist *idp;
    int		need_anglebrs;

    if (strcmp(ctl->bsmtp, "-") == 0)
	sinkfp = stdout;
    else
	sinkfp = fopen(ctl->bsmtp, "a");

    if (!sinkfp || ferror(sinkfp)) {
	report(stderr, GT_("BSMTP file open failed: %s\n"), 
		strerror(errno));
        return(PS_BSMTP);
    }

    /* see the ap computation under the SMTP branch */
    need_anglebrs = (msg->return_path[0] != '<');
    fprintf(sinkfp,
	    "MAIL FROM:%s%s%s",
	    need_anglebrs ? "<" : "",
	    (msg->return_path[0]) ? msg->return_path : user,
	    need_anglebrs ? ">" : "");

    if (ctl->pass8bits || (ctl->mimemsg & MSG_IS_8BIT))
	fputs(" BODY=8BITMIME", sinkfp);
    else if (ctl->mimemsg & MSG_IS_7BIT)
	fputs(" BODY=7BIT", sinkfp);

    /* exim's BSMTP processor does not handle SIZE */
    /* fprintf(sinkfp, " SIZE=%d", msg->reallen); */

    fprintf(sinkfp, "\r\n");

    /*
     * RFC 1123 requires that the domain name part of the
     * RCPT TO address be "canonicalized", that is a FQDN
     * or MX but not a CNAME.  Some listeners (like exim)
     * enforce this.  Now that we have the actual hostname,
     * compute what we should canonicalize with.
     */
    xfree(ctl->destaddr);
    ctl->destaddr = xstrdup(ctl->smtpaddress ? ctl->smtpaddress : "localhost");

    *bad_addresses = 0;
    for (idp = msg->recipients; idp; idp = idp->next)
	if (idp->val.status.mark == XMIT_ACCEPT)
	{
	    fprintf(sinkfp, "RCPT TO:<%s>\r\n",
		rcpt_address (ctl, idp->id, 1));
	    (*good_addresses)++;
	}

    fputs("DATA\r\n", sinkfp);

    if (fflush(sinkfp) || ferror(sinkfp))
    {
	report(stderr, GT_("BSMTP preamble write failed.\n"));
	return(PS_BSMTP);
    }

    return(PS_SUCCESS);
}

/* this is experimental and will be removed if double bounces are reported */
#define EXPLICIT_BOUNCE_ON_BAD_ADDRESS


static const char *is_quad(const char *q)
/* Check if the string passed in points to what could be one quad of a
 * dotted-quad IP address.  Requirements are that the string is not a
 * NULL pointer, begins with a period (which is skipped) or a digit
 * and ends with a period or a NULL.  If these requirements are met, a
 * pointer to the last character (the period or the NULL character) is
 * returned; otherwise NULL.
 */
{
  const char *r;
  
  if (!q || !*q)
    return NULL;
  if (*q == '.')
    q++;
  for(r=q;isdigit((unsigned char)*r);r++)
    ;
  if ( ((*r) && (*r != '.')) || ((r-q) < 1) || ((r-q)>3) )
    return NULL;
  /* Make sure quad is < 255 */
  if ( (r-q) == 3)
  {
    if (*q > '2')
      return NULL;
    else if (*q == '2')
    {
      if (*(q+1) > '5')
        return NULL;
      else if (*(q+1) == '5')
      {
        if (*(q+2) > '5')
          return NULL;
      }
    }
  }
  return r;
}

static int is_dottedquad(const char *hostname)
/* Returns a true value if the passed in string looks like an IP
 *  address in dotted-quad form, and a false value otherwise.
 */

{
  return ((hostname=is_quad(is_quad(is_quad(is_quad(hostname))))) != NULL) &&
    (*hostname == '\0');
}

static int open_smtp_sink(struct query *ctl, struct msgblk *msg,
	      int *good_addresses, int *bad_addresses /* this must be signed, to prevent endless loop in from_addresses */)
/* open an SMTP stream */
{
    const char	*ap;
    struct	idlist *idp;
    char		options[MSGBUFSIZE]; 
    char		addr[HOSTLEN+USERNAMELEN+1];
#ifdef EXPLICIT_BOUNCE_ON_BAD_ADDRESS
    char		**from_responses;
#endif /* EXPLICIT_BOUNCE_ON_BAD_ADDRESS */
    int		total_addresses;
    int		force_transient_error = 0;
    int		smtp_err;

    /*
     * Compute ESMTP options.
     */
    options[0] = '\0';
    if (ctl->server.esmtp_options & ESMTP_8BITMIME) {
	 if (ctl->pass8bits || (ctl->mimemsg & MSG_IS_8BIT))
	    strcpy(options, " BODY=8BITMIME");
	 else if (ctl->mimemsg & MSG_IS_7BIT)
	    strcpy(options, " BODY=7BIT");
    }

    if ((ctl->server.esmtp_options & ESMTP_SIZE) && msg->reallen > 0)
	sprintf(options + strlen(options), " SIZE=%d", msg->reallen);

    /*
     * Try to get the SMTP listener to take the Return-Path
     * address as MAIL FROM.  If it won't, fall back on the
     * remotename and mailserver host.  This won't affect replies,
     * which use the header From address anyway; the MAIL FROM
     * address is a place for the SMTP listener to send
     * bouncemail.  The point is to guarantee a FQDN in the MAIL
     * FROM line -- some SMTP listeners, like smail, become
     * unhappy otherwise.
     *
     * RFC 1123 requires that the domain name part of the
     * MAIL FROM address be "canonicalized", that is a
     * FQDN or MX but not a CNAME.  We'll assume the Return-Path
     * header is already in this form here (it certainly
     * is if rewrite is on).  RFC 1123 is silent on whether
     * a nonexistent hostname part is considered canonical.
     *
     * This is a potential problem if the MTAs further upstream
     * didn't pass canonicalized From/Return-Path lines, *and* the
     * local SMTP listener insists on them. 
     *
     * Handle the case where an upstream MTA is setting a return
     * path equal to "@".  Ghod knows why anyone does this, but 
     * it's been reported to happen in mail from Amazon.com and
     * Motorola.
     *
     * Also, if the hostname is a dotted quad, wrap it in square brackets.
     * Apparently this is required by RFC2821, section 4.1.3.
     */
    if (!msg->return_path[0] || (msg->return_path[0] == '@'))
    {
      if (strchr(ctl->remotename,'@') || strchr(ctl->remotename,'!'))
      {
	snprintf(addr, sizeof(addr), "%s", ctl->remotename);
      }
      else if (is_dottedquad(ctl->server.truename))
      {
	snprintf(addr, sizeof(addr), "%s@[%s]", ctl->remotename,
		ctl->server.truename);
      }
      else
      {
	snprintf(addr, sizeof(addr),
	      "%s@%s", ctl->remotename, ctl->server.truename);
      }
	ap = addr;
    }
    else if (strchr(msg->return_path,'@') || strchr(msg->return_path,'!'))
	ap = msg->return_path;
    /* in case Return-Path was "<>" we want to preserve that */
    else if (strcmp(msg->return_path,"<>") == 0)
	ap = msg->return_path;
    else		/* in case Return-Path existed but was local */
    {
      if (is_dottedquad(ctl->server.truename))
      {
	snprintf(addr, sizeof(addr), "%s@[%s]", msg->return_path,
		ctl->server.truename);
      }
      else
      {
	snprintf(addr, sizeof(addr), "%s@%s",
		msg->return_path, ctl->server.truename);
      }
	ap = addr;
    }

    if ((smtp_err = SMTP_from(ctl->smtp_socket, ctl->smtphostmode,
		    ap, options)) == SM_UNRECOVERABLE)
    {
	smtp_close(ctl, 0);
	return(PS_TRANSIENT);
    }
    if (smtp_err != SM_OK)
    {
	int err = handle_smtp_report(ctl, msg); /* map to PS_TRANSIENT or PS_REFUSED */

	SMTP_rset(ctl->smtp_socket, ctl->smtphostmode);    /* stay on the safe side */
	return(err);
    }

    /*
     * Now list the recipient addressees
     */
    total_addresses = 0;
    for (idp = msg->recipients; idp; idp = idp->next)
	total_addresses++;
#ifdef EXPLICIT_BOUNCE_ON_BAD_ADDRESS
    from_responses = (char **)xmalloc(sizeof(char *) * total_addresses);
#endif /* EXPLICIT_BOUNCE_ON_BAD_ADDRESS */
    for (idp = msg->recipients; idp; idp = idp->next)
	if (idp->val.status.mark == XMIT_ACCEPT)
	{
	    const char *address;
	    address = rcpt_address (ctl, idp->id, 1);
	    if ((smtp_err = SMTP_rcpt(ctl->smtp_socket, ctl->smtphostmode,
			    address)) == SM_UNRECOVERABLE)
	    {
		smtp_close(ctl, 0);
transient:
#ifdef EXPLICIT_BOUNCE_ON_BAD_ADDRESS
		while (*bad_addresses)
		    free(from_responses[--*bad_addresses]);
		free(from_responses);
#endif /* EXPLICIT_BOUNCE_ON_BAD_ADDRESS */
		return(PS_TRANSIENT);
	    }
	    if (smtp_err == SM_OK)
		(*good_addresses)++;
	    else
	    {
		switch (handle_smtp_report_without_bounce(ctl, msg))
		{
		    case PS_TRANSIENT:
		    force_transient_error = 1;
		    break;

		    case PS_SUCCESS:
#ifdef EXPLICIT_BOUNCE_ON_BAD_ADDRESS
		    from_responses[*bad_addresses] = xstrdup(smtp_response);
#endif /* EXPLICIT_BOUNCE_ON_BAD_ADDRESS */

		    (*bad_addresses)++;
		    idp->val.status.mark = XMIT_RCPTBAD;
		    if (outlevel >= O_VERBOSE)
			report(stderr,
			      GT_("%cMTP listener doesn't like recipient address `%s'\n"),
			      ctl->smtphostmode, address);
		    break;

		    case PS_REFUSED:
		    if (outlevel >= O_VERBOSE)
			report(stderr,
			      GT_("%cMTP listener doesn't really like recipient address `%s'\n"),
			      ctl->smtphostmode, address);
		    break;
		}
	    }
	}

    if (force_transient_error) {
	    /* do not risk dataloss due to overengineered multidrop
	     * crap. If one of the recipients returned PS_TRANSIENT,
	     * we return exactly that.
	     */
	    SMTP_rset(ctl->smtp_socket, ctl->smtphostmode);        /* required by RFC1870 */
	    goto transient;
    }
#ifdef EXPLICIT_BOUNCE_ON_BAD_ADDRESS
    /*
     * This should not be necessary, because the SMTP listener itself
     * should generate a bounce for the bad address.
     *
     * XXX FIXME 2006-01-19: is this comment true? I don't think
     * it is, because the SMTP listener isn't required to accept bogus
     * messages. There appears to be general SMTP<->MDA and
     * responsibility confusion.
     */
    if (*bad_addresses)
	send_bouncemail(ctl, msg, XMIT_RCPTBAD,
			"Some addresses were rejected by the MDA fetchmail forwards to.\r\n",
			*bad_addresses, from_responses);
    while (*bad_addresses)
	free(from_responses[--*bad_addresses]);
    free(from_responses);
#endif /* EXPLICIT_BOUNCE_ON_BAD_ADDRESS */

    /*
     * It's tempting to do local notification only if bouncemail was
     * insufficient -- that is, to add && total_addresses > *bad_addresses
     * to the test here.  The problem with this theory is that it would
     * make initial diagnosis of a broken multidrop configuration very
     * hard -- most single-recipient messages would just invisibly bounce.
     */
    if (!(*good_addresses)) 
    {
	if (!run.postmaster[0])
	{
	    if (outlevel >= O_VERBOSE)
		report(stderr, GT_("no address matches; no postmaster set.\n"));
	    SMTP_rset(ctl->smtp_socket, ctl->smtphostmode);	/* required by RFC1870 */
	    return(PS_REFUSED);
	}
	if ((smtp_err = SMTP_rcpt(ctl->smtp_socket, ctl->smtphostmode,
		rcpt_address (ctl, run.postmaster, 0))) == SM_UNRECOVERABLE)
	{
	    smtp_close(ctl, 0);
	    return(PS_TRANSIENT);
	}
	if (smtp_err != SM_OK)
	{
	    report(stderr, GT_("can't even send to %s!\n"), run.postmaster);
	    SMTP_rset(ctl->smtp_socket, ctl->smtphostmode);	/* required by RFC1870 */
	    return(PS_REFUSED);
	}

	if (outlevel >= O_VERBOSE)
	    report(stderr, GT_("no address matches; forwarding to %s.\n"), run.postmaster);
    }

    /* 
     * Tell the listener we're ready to send data.
     * Some listeners (like zmailer) may return antispam errors here.
     */
    if ((smtp_err = SMTP_data(ctl->smtp_socket, ctl->smtphostmode))
	    == SM_UNRECOVERABLE)
    {
	smtp_close(ctl, 0);
	return(PS_TRANSIENT);
    }
    if (smtp_err != SM_OK)
    {
	int err = handle_smtp_report(ctl, msg);
	SMTP_rset(ctl->smtp_socket, ctl->smtphostmode);    /* stay on the safe side */
	return(err);
    }

    /*
     * We need to stash this away in order to know how many
     * response lines to expect after the LMTP end-of-message.
     */
    lmtp_responses = *good_addresses;

    return(PS_SUCCESS);
}

static int open_mda_sink(struct query *ctl, struct msgblk *msg,
	      int *good_addresses, int *bad_addresses)
/* open a stream to a local MDA */
{
#ifdef HAVE_SETEUID
    uid_t orig_uid;
#endif /* HAVE_SETEUID */
    struct	idlist *idp;
    int	length = 0, fromlen = 0, nameslen = 0;
    char	*names = NULL, *before, *after, *from = NULL;

    (void)bad_addresses;
    xfree(ctl->destaddr);
    ctl->destaddr = xstrdup("localhost");

    for (idp = msg->recipients; idp; idp = idp->next)
	if (idp->val.status.mark == XMIT_ACCEPT)
	    (*good_addresses)++;

    length = strlen(ctl->mda);
    before = xstrdup(ctl->mda);

    /* get user addresses for %T (or %s for backward compatibility) */
    if (strstr(before, "%s") || strstr(before, "%T"))
    {
	/*
	 * We go through this in order to be able to handle very
	 * long lists of users and (re)implement %s.
	 */
	nameslen = 0;
	for (idp = msg->recipients; idp; idp = idp->next)
	    if ((idp->val.status.mark == XMIT_ACCEPT))
		nameslen += (strlen(idp->id) + 1);	/* string + ' ' */
	if ((*good_addresses == 0))
	    nameslen = strlen(run.postmaster);

	names = (char *)xmalloc(nameslen + 1);	/* account for '\0' */
	if (*good_addresses == 0)
	    strcpy(names, run.postmaster);
	else
	{
	    names[0] = '\0';
	    for (idp = msg->recipients; idp; idp = idp->next)
		if (idp->val.status.mark == XMIT_ACCEPT)
		{
		    strcat(names, idp->id);
		    strcat(names, " ");
		}
	    names[--nameslen] = '\0';	/* chop trailing space */
	}

	sanitize(names);
    }

    /* get From address for %F */
    if (strstr(before, "%F"))
    {
	from = xstrdup(msg->return_path);

	sanitize(from);

	fromlen = strlen(from);
    }

    /* do we have to build an mda string? */
    if (names || from) 
    {		
	char	*sp, *dp;

	/* find length of resulting mda string */
	sp = before;
	while ((sp = strstr(sp, "%s"))) {
	    length += nameslen;	/* subtract %s and add '' */
	    sp += 2;
	}
	sp = before;
	while ((sp = strstr(sp, "%T"))) {
	    length += nameslen;	/* subtract %T and add '' */
	    sp += 2;
	}
	sp = before;
	while ((sp = strstr(sp, "%F"))) {
	    length += fromlen;	/* subtract %F and add '' */
	    sp += 2;
	}

	after = (char *)xmalloc(length + 1);

	/* copy mda source string to after, while expanding %[sTF] */
	for (dp = after, sp = before; (*dp = *sp); dp++, sp++) {
	    if (sp[0] != '%')	continue;

	    /* need to expand? BTW, no here overflow, because in
	    ** the worst case (end of string) sp[1] == '\0' */
	    if (sp[1] == 's' || sp[1] == 'T') {
		*dp++ = '\'';
		strcpy(dp, names);
		dp += nameslen;
		*dp++ = '\'';
		sp++;	/* position sp over [sT] */
		dp--;	/* adjust dp */
	    } else if (sp[1] == 'F') {
		*dp++ = '\'';
		strcpy(dp, from);
		dp += fromlen;
		*dp++ = '\'';
		sp++;	/* position sp over F */
		dp--;	/* adjust dp */
	    }
	}

	if (names) {
	    free(names);
	    names = NULL;
	}
	if (from) {
	    free(from);
	    from = NULL;
	}

	free(before);

	before = after;
    }


    if (outlevel >= O_DEBUG)
	report(stdout, GT_("about to deliver with: %s\n"), before);

#ifdef HAVE_SETEUID
    /*
     * Arrange to run with user's permissions if we're root.
     * This will initialize the ownership of any files the
     * MDA creates properly.  (The seteuid call is available
     * under all BSDs and Linux)
     */
    orig_uid = getuid();
    seteuid(ctl->uid);
#endif /* HAVE_SETEUID */

    sinkfp = popen(before, "w");
    free(before);
    before = NULL;

#ifdef HAVE_SETEUID
    /* this will fail quietly if we didn't start as root */
    seteuid(orig_uid);
#endif /* HAVE_SETEUID */

    if (!sinkfp)
    {
	report(stderr, GT_("MDA open failed\n"));
	return(PS_IOERR);
    }

    /*
     * We need to disable the normal SIGCHLD handling here because 
     * sigchld_handler() would reap away the error status, returning
     * error status instead of 0 for successful completion.
     */
    set_signal_handler(SIGCHLD, SIG_DFL);

    return(PS_SUCCESS);
}

int open_sink(struct query *ctl, struct msgblk *msg,
	      int *good_addresses, int *bad_addresses)
/* set up sinkfp to be an input sink we can ship a message to */
{
    *bad_addresses = *good_addresses = 0;

    if (ctl->bsmtp)		/* dump to a BSMTP batch file */
	return(open_bsmtp_sink(ctl, msg, good_addresses, bad_addresses));
    /* 
     * Try to forward to an SMTP or LMTP listener.  If the attempt to 
     * open a socket fails, fall through to attempt delivery via
     * local MDA.
     */
    else if (!ctl->mda && smtp_open(ctl) != -1)
	return(open_smtp_sink(ctl, msg, good_addresses, bad_addresses));

    /*
     * Awkward case.  User didn't specify an MDA.  Our attempt to get a
     * listener socket failed.  Try to cope anyway -- initial configuration
     * may have found procmail.
     */
    else if (!ctl->mda)
    {
	report(stderr, GT_("%cMTP connect to %s failed\n"),
	       ctl->smtphostmode,
	       ctl->smtphost ? ctl->smtphost : "localhost");

#ifndef FALLBACK_MDA
	/* No fallback MDA declared.  Bail out. */
	return(PS_SMTP);
#else
	/*
	 * If user had things set up to forward offsite, no way
	 * we want to deliver locally!
	 */
	if (ctl->smtphost && strcmp(ctl->smtphost, "localhost"))
	    return(PS_SMTP);

	/* 
	 * User was delivering locally.  We have a fallback MDA.
	 * Latch it in place, logging the error, and fall through.
	 * Set stripcr as we would if MDA had been the initial transport
	 */
	ctl->mda = FALLBACK_MDA;
	if (!ctl->forcecr)
	    ctl->stripcr = TRUE;

	report(stderr, GT_("can't raise the listener; falling back to %s"),
			 FALLBACK_MDA);
#endif
    }

    if (ctl->mda)		/* must deliver through an MDA */
	return(open_mda_sink(ctl, msg, good_addresses, bad_addresses));

    return(PS_SUCCESS);
}

void release_sink(struct query *ctl)
/* release the per-message output sink, whether it's a pipe or SMTP socket */
{
    if (ctl->bsmtp && sinkfp)
    {
	if (strcmp(ctl->bsmtp, "-"))
	{
	    fclose(sinkfp);
	    sinkfp = (FILE *)NULL;
	}
    }
    else if (ctl->mda)
    {
	if (sinkfp)
	{
	    pclose(sinkfp);
	    sinkfp = (FILE *)NULL;
	}
	deal_with_sigchld(); /* Restore SIGCHLD handling to reap zombies */
    }
}

int close_sink(struct query *ctl, struct msgblk *msg, flag forward)
/* perform end-of-message actions on the current output sink */
{
    int smtp_err;
    if (ctl->mda)
    {
	int rc = 0, e = 0, e2 = 0, err = 0;

	/* close the delivery pipe, we'll reopen before next message */
	if (sinkfp)
	{
	    if (ferror(sinkfp))
		err = 1, e2 = errno;
	    if ((fflush(sinkfp)))
		err = 1, e2 = errno;

	    errno = 0;
	    rc = pclose(sinkfp);
	    e = errno;
	    sinkfp = (FILE *)NULL;
	}
	else
	    rc = e = 0;

	deal_with_sigchld(); /* Restore SIGCHLD handling to reap zombies */

	if (rc || err)
	{
	    if (err) {
		report(stderr, GT_("Error writing to MDA: %s\n"), strerror(e2));
	    } else if (WIFSIGNALED(rc)) {
		report(stderr, 
			GT_("MDA died of signal %d\n"), WTERMSIG(rc));
	    } else if (WIFEXITED(rc)) {
		report(stderr, 
			GT_("MDA returned nonzero status %d\n"), WEXITSTATUS(rc));
	    } else {
		report(stderr,
			GT_("Strange: MDA pclose returned %d and errno %d/%s, cannot handle at %s:%d\n"),
			rc, e, strerror(e), __FILE__, __LINE__);
	    }

	    return(FALSE);
	}
    }
    else if (ctl->bsmtp && sinkfp)
    {
	int error;

	/* implicit disk-full check here... */
	fputs(".\r\n", sinkfp);
	error = ferror(sinkfp);
	if (strcmp(ctl->bsmtp, "-"))
	{
	    if (fclose(sinkfp) == EOF) error = 1;
	    sinkfp = (FILE *)NULL;
	}
	if (error)
	{
	    report(stderr, 
		   GT_("Message termination or close of BSMTP file failed\n"));
	    return(FALSE);
	}
    }
    else if (forward)
    {
	/* write message terminator */
	if ((smtp_err = SMTP_eom(ctl->smtp_socket, ctl->smtphostmode))
		== SM_UNRECOVERABLE)
	{
	    smtp_close(ctl, 0);
	    return(FALSE);
	}
	if (smtp_err != SM_OK)
	{
	    if (handle_smtp_report(ctl, msg) != PS_REFUSED)
	    {
	        SMTP_rset(ctl->smtp_socket, ctl->smtphostmode);    /* stay on the safe side */
		return(FALSE);
	    }
	    else
	    {
		report(stderr, GT_("SMTP listener refused delivery\n"));
	        SMTP_rset(ctl->smtp_socket, ctl->smtphostmode);    /* stay on the safe side */
		return(TRUE);
	    }
	}

	/*
	 * If this is an SMTP connection, SMTP_eom() ate the response.
	 * But could be this is an LMTP connection, in which case we have to
	 * interpret either (a) a single 503 response meaning there
	 * were no successful RCPT TOs, or (b) a variable number of
	 * responses, one for each successful RCPT TO.  We need to send
	 * bouncemail on each failed response and then return TRUE anyway,
	 * otherwise the message will get left in the queue and resent
	 * to people who got it the first time.
	 */
	if (ctl->smtphostmode == LMTP_MODE)
	{
	    if (lmtp_responses == 0)
	    {
		SMTP_ok(ctl->smtp_socket, ctl->smtphostmode, TIMEOUT_EOM);

		/*
		 * According to RFC2033, 503 is the only legal response
		 * if no RCPT TO commands succeeded.  No error recovery
		 * is really possible here, as we have no idea what
		 * insane thing the listener might be doing if it doesn't
		 * comply.
		 */
		if (atoi(smtp_response) == 503)
		    report(stderr, GT_("LMTP delivery error on EOM\n"));
		else
		    report(stderr,
			  GT_("Unexpected non-503 response to LMTP EOM: %s\n"),
			  smtp_response);

		/*
		 * It's not completely clear what to do here.  We choose to
		 * interpret delivery failure here as a transient error, 
		 * the same way SMTP delivery failure is handled.  If we're
		 * wrong, an undead message will get stuck in the queue.
		 */
		return(FALSE);
	    }
	    else
	    {
		int	i, errors, rc = FALSE;
		char	**responses;

		/* eat the RFC2033-required responses, saving errors */ 
		responses = (char **)xmalloc(sizeof(char *) * lmtp_responses);
		for (errors = i = 0; i < lmtp_responses; i++)
		{
		    if ((smtp_err = SMTP_ok(ctl->smtp_socket, ctl->smtphostmode, TIMEOUT_EOM))
			    == SM_UNRECOVERABLE)
		    {
			smtp_close(ctl, 0);
			goto unrecov;
		    }
		    if (smtp_err != SM_OK)
		    {
			responses[errors] = xstrdup(smtp_response);
			errors++;
		    }
		}

		if (errors == 0)
		    rc = TRUE;	/* all deliveries succeeded */
		else
		    /*
		     * One or more deliveries failed.
		     * If we can bounce a failures list back to the
		     * sender, and the postmaster does not want to
		     * deal with the bounces return TRUE, deleting the
		     * message from the server so it won't be
		     * re-forwarded on subsequent poll cycles.
		     */
		    rc = send_bouncemail(ctl, msg, XMIT_ACCEPT,
			    "LMTP partial delivery failure.\r\n",
			    errors, responses);

unrecov:
		for (i = 0; i < errors; i++)
		    free(responses[i]);
		free(responses);
		return rc;
	    }
	}
    }

    return(TRUE);
}

int open_warning_by_mail(struct query *ctl)
/* set up output sink for a mailed warning to calling user */
{
    int	good, bad;

    /*
     * Dispatching warning email is a little complicated.  The problem is
     * that we have to deal with three distinct cases:
     *
     * 1. Single-drop running from user account.  Warning mail should
     * go to the local name for which we're collecting (coincides
     * with calling user).
     *
     * 2. Single-drop running from root or other privileged ID, with rc
     * file generated on the fly (Ken Estes's weird setup...)  Mail
     * should go to the local name for which we're collecting (does not 
     * coincide with calling user).
     * 
     * 3. Multidrop.  Mail must go to postmaster.  We leave the recipients
     * member null so this message will fall through to run.postmaster.
     *
     * The zero in the reallen element means we won't pass a SIZE
     * option to ESMTP; the message length would be more trouble than
     * it's worth to compute.
     */
    struct msgblk reply = {NULL, NULL, "FETCHMAIL-DAEMON@", 0, 0};
    int status;

    strlcat(reply.return_path, ctl->smtpaddress ? ctl->smtpaddress :
	    fetchmailhost, sizeof(reply.return_path));

    if (!MULTIDROP(ctl))		/* send to calling user */
    {
	save_str(&reply.recipients, ctl->localnames->id, XMIT_ACCEPT);
	status = open_sink(ctl, &reply, &good, &bad);
	free_str_list(&reply.recipients);
    }
    else				/* send to postmaster  */
	status = open_sink(ctl, &reply, &good, &bad);
    if (status == 0) {
	stuff_warning(NULL, ctl, "From: FETCHMAIL-DAEMON@%s",
		ctl->smtpaddress ? ctl->smtpaddress : fetchmailhost);
	stuff_warning(NULL, ctl, "Date: %s", rfc822timestamp());
	stuff_warning(NULL, ctl, "MIME-Version: 1.0");
	stuff_warning(NULL, ctl, "Content-Transfer-Encoding: 8bit");
	stuff_warning(NULL, ctl, "Content-Type: text/plain; charset=\"%s\"", iana_charset);
    }
    return(status);
}

/* format and ship a warning message line by mail */
/* if rfc2047charset is non-NULL, encode the line (that is assumed to be
 * a header line) as per RFC-2047 using rfc2047charset as the character
 * set field */
#if defined(HAVE_STDARG_H)
void stuff_warning(const char *rfc2047charset, struct query *ctl, const char *fmt, ... )
#else
void stuff_warning(rfc2047charset, ctl, fmt, va_alist)
const char *charset;
struct query *ctl;
const char *fmt;	/* printf-style format */
va_dcl
#endif
{
    /* make huge -- i18n can bulk up error messages a lot */
    char	buf[2*MSGBUFSIZE+4];
    va_list ap;

    /*
     * stuffline() requires its input to be writeable (for CR stripping),
     * so we needed to copy the message to a writeable buffer anyway in
     * case it was a string constant.  We make a virtue of that necessity
     * here by supporting stdargs/varargs.
     */
#if defined(HAVE_STDARG_H)
    va_start(ap, fmt) ;
#else
    va_start(ap);
#endif
    vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);

    snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\r\n");

    /* guard against very long lines */
    buf[MSGBUFSIZE+1] = '\r';
    buf[MSGBUFSIZE+2] = '\n';
    buf[MSGBUFSIZE+3] = '\0';

    stuffline(ctl, rfc2047charset != NULL ? rfc2047e(buf, rfc2047charset) : buf);
}

void close_warning_by_mail(struct query *ctl, struct msgblk *msg)
/* sign and send mailed warnings */
{
    stuff_warning(NULL, ctl, GT_("-- \nThe Fetchmail Daemon"));
    close_sink(ctl, msg, TRUE);
}

/* sink.c ends here */
