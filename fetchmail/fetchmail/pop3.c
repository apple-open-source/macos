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
 
#include  "fetchmail.h"
#include  "socket.h"
#include  "i18n.h"

#if OPIE_ENABLE
#include <opie.h>
#endif /* OPIE_ENABLE */

#ifndef strstr		/* glibc-2.1 declares this as a macro */
extern char *strstr();	/* needed on sysV68 R3V7.1. */
#endif /* strstr */

static int last;
#ifdef SDPS_ENABLE
char *sdps_envfrom;
char *sdps_envto;
#endif /* SDPS_ENABLE */

#if OPIE_ENABLE
static char lastok[POPBUFSIZE+1];
#endif /* OPIE_ENABLE */

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

	while (isalpha(*bufp))
	    bufp++;

	if (*bufp)
	  *(bufp++) = '\0';

	if (strcmp(buf,"+OK") == 0)
	{
#if OPIE_ENABLE
	    strcpy(lastok, bufp);
#endif /* OPIE_ENABLE */
	    ok = 0;
	}
	else if (strncmp(buf,"-ERR", 4) == 0)
	{
	    if (stage > STAGE_GETAUTH) 
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

	if (argbuf != NULL)
	    strcpy(argbuf,bufp);
    }

    return(ok);
}

static int pop3_getauth(int sock, struct query *ctl, char *greeting)
/* apply for connection authorization */
{
    int ok;
    char *start,*end;
    char *msg;
#if OPIE_ENABLE
    char *challenge;
#endif /* OPIE_ENABLE */
#if defined(GSSAPI)
    flag has_gssapi = FALSE;
#endif /* defined(GSSAPI) */
#if defined(KERBEROS_V4) || defined(KERBEROS_V5)
    flag has_kerberos = FALSE;
#endif /* defined(KERBEROS_V4) || defined(KERBEROS_V5) */
    flag has_cram = FALSE;
#ifdef OPIE_ENABLE
    flag has_otp = FALSE;
#endif /* OPIE_ENABLE */
#ifdef SSL_ENABLE
    flag has_ssl = FALSE;
#endif /* SSL_ENABLE */

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
	 * authentication mechanisms.  if it doesn't, no harm done, we
	 * just fall back to a plain login.  Note that this code 
	 * latches the server's authentication type, so that in daemon mode
	 * the CAPA check only needs to be done once at start of run.
	 *
	 * APOP was introduced in RFC 1450, and CAPA not until
	 * RFC2449. So the < check is an easy way to prevent CAPA from
	 * being sent to the more primitive POP3 servers dating from
	 * RFC 1081 and RFC 1225, which seem more likely to choke on
	 * it.  This certainly catches IMAP-2000's POP3 gateway.
	 * 
	 * These authentication methods are blessed by RFC1734,
	 * describing the POP3 AUTHentication command.
	 */
	if (ctl->server.authenticate == A_ANY 
	    && strchr(greeting, '<') 
	    && gen_transact(sock, "CAPA") == 0)
	{
	    char buffer[64];

	    /* determine what authentication methods we have available */
	    while ((ok = gen_recv(sock, buffer, sizeof(buffer))) == 0)
	    {
		if (DOTLINE(buffer))
		    break;
#ifdef SSL_ENABLE
               if (strstr(buffer, "STLS"))
                   has_ssl = TRUE;
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
		if (strstr(buffer, "CRAM-MD5"))
		    has_cram = TRUE;
	    }
	}

#ifdef SSL_ENABLE
       if (has_ssl &&
#if INET6_ENABLE
           ctl->server.service && (strcmp(ctl->server.service, "pop3s"))
#else /* INET6_ENABLE */
           ctl->server.port != 995
#endif /* INET6_ENABLE */
           )
       {
           char *realhost;

           realhost = ctl->server.via ? ctl->server.via : ctl->server.pollname;           gen_transact(sock, "STLS");
           if (SSLOpen(sock,ctl->sslcert,ctl->sslkey,ctl->sslproto,ctl->sslcertck, ctl->sslcertpath,ctl->sslfingerprint,realhost,ctl->server.pollname) == -1)
           {
               report(stderr,
                      GT_("SSL connection failed.\n"));
               return(PS_AUTHFAIL);
           }
       }
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
#if INET6_ENABLE
	    ctl->server.service && (strcmp(ctl->server.service, KPOP_PORT)!=0)
#else /* INET6_ENABLE */
	    ctl->server.port != KPOP_PORT
#endif /* INET6_ENABLE */
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
	     ctl->server.authenticate == A_ANY))
	{
	    ok = do_gssauth(sock,"AUTH",ctl->server.truename,ctl->remotename);
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

	if (has_cram &&
	    (ctl->server.authenticate == A_CRAM_MD5 ||
	     ctl->server.authenticate == A_ANY))
	{
	    ok = do_cram_md5(sock, "AUTH", ctl, NULL);
	    if (ok == PS_SUCCESS || ctl->server.authenticate != A_ANY)
		break;
	}

	/* ordinary validation, no one-time password or RPA */ 
	gen_transact(sock, "USER %s", ctl->remotename);
	strcpy(shroud, ctl->password);
	ok = gen_transact(sock, "PASS %s", ctl->password);
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

	/* copy timestamp and password into digestion buffer */
	xalloca(msg, char *, (end-start+1) + strlen(ctl->password) + 1);
	strcpy(msg,start);
	strcat(msg,ctl->password);

	strcpy(ctl->digest, MD5Digest((unsigned char *)msg));

	ok = gen_transact(sock, "APOP %s %s", ctl->remotename, ctl->digest);
	break;

    case P_RPOP:
	if ((ok = gen_transact(sock,"USER %s", ctl->remotename)) == 0)
	    ok = gen_transact(sock, "RPOP %s", ctl->password);
	break;

    default:
	report(stderr, GT_("Undefined protocol request in POP3_auth\n"));
	ok = PS_ERROR;
    }

    if (ok != 0)
    {
	/* maybe we detected a lock-busy condition? */
        if (ok == PS_LOCKBUSY)
	    report(stderr, GT_("lock busy!  Is another session active?\n")); 

	return(ok);
    }

    /*
     * Empirical experience shows some server/OS combinations
     * may need a brief pause even after any lockfiles on the
     * server are released, to give the server time to finish
     * copying back very large mailfolders from the temp-file...
     * this is only ever an issue with extremely large mailboxes.
     */
    sleep(3); /* to be _really_ safe, probably need sleep(5)! */

    /* we're peek-capable if use of TOP is enabled */
    peek_capable = !(ctl->fetchall || ctl->keep);

    /* we're approved */
    return(PS_SUCCESS);
}

static int pop3_gettopid( int sock, int num , char *id)
{
    int ok;
    int got_it;
    char buf [POPBUFSIZE+1];
    sprintf( buf, "TOP %d 1", num );
    if ((ok = gen_transact(sock, buf )) != 0)
       return ok; 
    got_it = 0;
    while ((ok = gen_recv(sock, buf, sizeof(buf))) == 0) 
    {
	if (DOTLINE(buf))
	    break;
	if ( ! got_it && ! strncasecmp("Message-Id:", buf, 11 )) {
	    got_it = 1;
	    /* prevent stack overflows */
	    buf[IDLEN+12] = 0;
	    sscanf( buf+12, "%s", id);
	}
    }
    return 0;
}

static int pop3_slowuidl( int sock,  struct query *ctl, int *countp, int *newp)
{
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
    
    if( (ok = pop3_gettopid( sock, 1, id )) != 0 )
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
	    if( (ok = pop3_gettopid( sock, try_id, id )) != 0 )
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
		    
		    if( (ok = pop3_gettopid( sock, try_id, id )) != 0 )
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
	struct idlist	*new = save_str(&ctl->newsaved, 
				str_from_nr_list(&ctl->oldsaved, num),
				UID_UNSEEN);
	new->val.status.num = num - first_nr + 1;
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
    if (ok == 0)
	sscanf(buf,"%d %d", countp, bytes);
    else
	return(ok);

    /*
     * Newer, RFC-1725-conformant POP servers may not have the LAST command.
     * We work as hard as possible to hide this ugliness, but it makes 
     * counting new messages intrinsically quadratic in the worst case.
     */
    last = 0;
    *newp = -1;
    if (*countp > 0 && !ctl->fetchall)
    {
	char id [IDLEN+1];

	if (!ctl->server.uidl) {
	    gen_send(sock, "LAST");
	    ok = pop3_ok(sock, buf);
	} else
	    ok = 1;
	if (ok == 0)
	{
	    if (sscanf(buf, "%d", &last) == 0)
	    {
		report(stderr, GT_("protocol error\n"));
		return(PS_ERROR);
	    }
	    *newp = (*countp - last);
	}
 	else
 	{
	    /* grab the mailbox's UID list */
	    if ((ok = gen_transact(sock, "UIDL")) != 0)
	    {
		/* don't worry, yet! do it the slow way */
		if((ok = pop3_slowuidl( sock, ctl, countp, newp))!=0)
		{
		    report(stderr, GT_("protocol error while fetching UIDLs\n"));
		    return(PS_ERROR);
		}
	    }
	    else
	    {
		int	num;

		*newp = 0;
 		while ((ok = gen_recv(sock, buf, sizeof(buf))) == 0)
		{
 		    if (DOTLINE(buf))
 			break;
 		    else if (sscanf(buf, "%d %s", &num, id) == 2)
		    {
 			struct idlist	*new;

			new = save_str(&ctl->newsaved, id, UID_UNSEEN);
			new->val.status.num = num;

			if (str_in_list(&ctl->oldsaved, id, FALSE)) {
			    new->val.status.mark = UID_SEEN;
			    str_set_mark(&ctl->oldsaved, id, UID_SEEN);
			}
			else
			    (*newp)++;
		    }
 		}
 	    }
 	}
    }

    return(PS_SUCCESS);
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
		if (num > 0 && num <= count)
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
    if (!ctl->oldsaved)
	return (num <= last);
    else
        return (str_in_list(&ctl->oldsaved,
			    str_find(&ctl->newsaved, num), FALSE));
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
     * See http://www.demon.net/services/mail/sdps-tech.html
     * for a description of what we're parsing here.
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
		sdps_envfrom = xmalloc(strlen(buf)+1);
		strcpy(sdps_envfrom,buf);
		break;
	    case 5:
                /* Wrap address with To: <> so nxtaddr() likes it */
                sdps_envto = xmalloc(strlen(buf)+7);
                sprintf(sdps_envto,"To: <%s>",buf);
		break;
            }
	} while
	    (!(buf[0] == '.' && (buf[1] == '\r' || buf[1] == '\n' || buf[1] == '\0')));
    }
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
     * Also use RETR if fetchall is on.  This gives us a workaround
     * for servers like usa.net's that bungle TOP.  It's pretty
     * harmless because fetchall guarantees that any message dropped
     * by an interrupted RETR will be picked up on the next poll of the
     * site.
     *
     * We take advantage here of the fact that, according to all the
     * POP RFCs, "if the number of lines requested by the POP3 client
     * is greater than than the number of lines in the body, then the
     * POP3 server sends the entire message.").
     *
     * The line count passed (99999999) is the maximum value CompuServe will
     * accept; it's much lower than the natural value 2147483646 (the maximum
     * twos-complement signed 32-bit integer minus 1) */
    if (ctl->keep || ctl->fetchall)
	gen_send(sock, "RETR %d", number);
    else
	gen_send(sock, "TOP %d 99999999", number);
    if ((ok = pop3_ok(sock, buf)) != 0)
	return(ok);

    *lenp = -1;		/* we got sizes from the LIST response */

    return(PS_SUCCESS);
}

static int pop3_delete(int sock, struct query *ctl, int number)
/* delete a given message */
{
    /* actually, mark for deletion -- doesn't happen until QUIT time */
    return(gen_transact(sock, "DELE %d", number));
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

    if (ctl->lastid)
    {
	free(ctl->lastid);
	ctl->lastid = NULL;
    }

    return(ok);
}

const static struct method pop3 =
{
    "POP3",		/* Post Office Protocol v3 */
#if INET6_ENABLE
    "pop3",		/* standard POP3 port */
    "pop3s",		/* ssl POP3 port */
#else /* INET6_ENABLE */
    110,		/* standard POP3 port */
    995,		/* ssl POP3 port */
#endif /* INET6_ENABLE */
    FALSE,		/* this is not a tagged protocol */
    TRUE,		/* this uses a message delimiter */
    pop3_ok,		/* parse command response */
    pop3_getauth,	/* get authorization */
    pop3_getrange,	/* query range of messages */
    pop3_getsizes,	/* we can get a list of sizes */
    pop3_is_old,	/* how do we tell a message is old? */
    pop3_fetch,		/* request given message */
    NULL,		/* no way to fetch body alone */
    NULL,		/* no message trailer */
    pop3_delete,	/* how to delete a message */
    pop3_logout,	/* log out, we're done */
    FALSE,		/* no, we can't re-poll */
};

int doPOP3 (struct query *ctl)
/* retrieve messages using POP3 */
{
#ifndef MBOX
    if (ctl->mailboxes->id) {
	fprintf(stderr,GT_("Option --remote is not supported with POP3\n"));
	return(PS_SYNTAX);
    }
#endif /* MBOX */
    peek_capable = !ctl->fetchall;
    return(do_protocol(ctl, &pop3));
}
#endif /* POP3_ENABLE */

/* pop3.c ends here */
