/*
 * driver.c -- generic driver for mail fetch method protocols
 *
 * Copyright 1997 by Eric S. Raymond
 * For license terms, see the file COPYING in this directory.
 */

#include  "config.h"
#include  <stdio.h>
#include  <setjmp.h>
#include  <errno.h>
#include  <string.h>
#ifdef HAVE_MEMORY_H
#include  <memory.h>
#endif /* HAVE_MEMORY_H */
#if defined(STDC_HEADERS)
#include  <stdlib.h>
#include  <limits.h>
#endif
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined(HAVE_SYS_ITIMER_H)
#include <sys/itimer.h>
#endif
#include  <sys/time.h>
#include  <signal.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_NET_SOCKET_H
#include <net/socket.h>
#endif
#ifdef HESIOD
#include <hesiod.h>
#endif

#if defined(HAVE_RES_SEARCH) || defined(HAVE_GETHOSTBYNAME)
#include <netdb.h>
#include "mx.h"
#endif /* defined(HAVE_RES_SEARCH) || defined(HAVE_GETHOSTBYNAME) */

#include "kerberos.h"
#ifdef KERBEROS_V4
#include <netinet/in.h>
#endif /* KERBEROS_V4 */

#include "i18n.h"
#include "socket.h"

#include "fetchmail.h"
#include "tunable.h"

/* throw types for runtime errors */
#define THROW_TIMEOUT	1		/* server timed out */
#define THROW_SIGPIPE	2		/* SIGPIPE on stream socket */

/* magic values for the message length array */
#define MSGLEN_UNKNOWN	0		/* length unknown (0 is impossible) */
#define MSGLEN_INVALID	-1		/* length passed back is invalid */
#define MSGLEN_TOOLARGE	-2		/* message is too large */
#define MSGLEN_OLD	-3		/* message is old */

int pass;		/* how many times have we re-polled? */
int stage;		/* where are we? */
int phase;		/* where are we, for error-logging purposes? */
int batchcount;		/* count of messages sent in current batch */
flag peek_capable;	/* can we peek for better error recovery? */
int mailserver_socket_temp = -1;	/* socket to free if connect timeout */ 

static int timeoutcount;		/* count consecutive timeouts */

static jmp_buf	restart;

void set_timeout(int timeleft)
/* reset the nonresponse-timeout */
{
#if !defined(__EMX__) && !defined(__BEOS__) 
    struct itimerval ntimeout;

    if (timeleft == 0)
	timeoutcount = 0;

    ntimeout.it_interval.tv_sec = ntimeout.it_interval.tv_usec = 0;
    ntimeout.it_value.tv_sec  = timeleft;
    ntimeout.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &ntimeout, (struct itimerval *)NULL);
#endif
}

static void timeout_handler (int signal)
/* handle SIGALRM signal indicating a server timeout */
{
    timeoutcount++;
    longjmp(restart, THROW_TIMEOUT);
}

static void sigpipe_handler (int signal)
/* handle SIGPIPE signal indicating a broken stream socket */
{
    longjmp(restart, THROW_SIGPIPE);
}

/* ignore SIGALRM signal indicating a timeout during cleanup */
static void cleanup_timeout_handler (int signal) { }

#define CLEANUP_TIMEOUT 60 /* maximum timeout during cleanup */

static int cleanupSockClose (int fd)
/* close sockets in maximum CLEANUP_TIMEOUT seconds during cleanup */
{
    int scerror;
    void (*alrmsave)(int);
    alrmsave = signal(SIGALRM, cleanup_timeout_handler);
    set_timeout(CLEANUP_TIMEOUT);
    scerror = SockClose(fd);
    set_timeout(0);
    signal(SIGALRM, alrmsave);
    return (scerror);
}

#ifdef KERBEROS_V4
static int kerberos_auth(socket, canonical, principal) 
/* authenticate to the server host using Kerberos V4 */
int socket;		/* socket to server host */
char *canonical;	/* server name */
char *principal;
{
    char * host_primary;
    KTEXT ticket;
    MSG_DAT msg_data;
    CREDENTIALS cred;
    Key_schedule schedule;
    int rem;
    char * prin_copy = (char *) NULL;
    char * prin = (char *) NULL;
    char * inst = (char *) NULL;
    char * realm = (char *) NULL;

    if (principal != (char *)NULL && *principal)
    {
        char *cp;
        prin = prin_copy = xstrdup(principal);
	for (cp = prin_copy; *cp && *cp != '.'; ++cp)
	    ;
	if (*cp)
	{
	    *cp++ = '\0';
	    inst = cp;
	    while (*cp && *cp != '@')
	        ++cp;
	    if (*cp)
	    {
	        *cp++ = '\0';
	        realm = cp;
	    }
	}
    }
  
    xalloca(ticket, KTEXT, sizeof (KTEXT_ST));
    rem = (krb_sendauth (0L, socket, ticket,
			 prin ? prin : "pop",
			 inst ? inst : canonical,
			 realm ? realm : ((char *) (krb_realmofhost (canonical))),
			 ((unsigned long) 0),
			 (&msg_data),
			 (&cred),
			 (schedule),
			 ((struct sockaddr_in *) 0),
			 ((struct sockaddr_in *) 0),
			 "KPOPV0.1"));
    if (prin_copy)
    {
        free(prin_copy);
    }
    if (rem != KSUCCESS)
    {
	report(stderr, GT_("kerberos error %s\n"), (krb_get_err_text (rem)));
	return (PS_AUTHFAIL);
    }
    return (0);
}
#endif /* KERBEROS_V4 */

#ifdef KERBEROS_V5
static int kerberos5_auth(socket, canonical)
/* authenticate to the server host using Kerberos V5 */
int socket;             /* socket to server host */
const char *canonical;  /* server name */
{
    krb5_error_code retval;
    krb5_context context;
    krb5_ccache ccdef;
    krb5_principal client = NULL, server = NULL;
    krb5_error *err_ret = NULL;

    krb5_auth_context auth_context = NULL;

    krb5_init_context(&context);
#ifndef __APPLE__
    krb5_init_ets(context);
#endif
    krb5_auth_con_init(context, &auth_context);

    if (retval = krb5_cc_default(context, &ccdef)) {
        report(stderr, "krb5_cc_default: %s\n", error_message(retval));
        return(PS_ERROR);
    }

    if (retval = krb5_cc_get_principal(context, ccdef, &client)) {
        report(stderr, "krb5_cc_get_principal: %s\n", error_message(retval));
        return(PS_ERROR);
    }

    if (retval = krb5_sname_to_principal(context, canonical, "pop",
           KRB5_NT_UNKNOWN,
           &server)) {
        report(stderr, "krb5_sname_to_principal: %s\n", error_message(retval));
        return(PS_ERROR);
    }

    retval = krb5_sendauth(context, &auth_context, (krb5_pointer) &socket,
         "KPOPV1.0", client, server,
         AP_OPTS_MUTUAL_REQUIRED,
         NULL,  /* no data to checksum */
         0,   /* no creds, use ccache instead */
         ccdef,
         &err_ret, 0,

         NULL); /* don't need reply */

    krb5_free_principal(context, server);
    krb5_free_principal(context, client);
    krb5_auth_con_free(context, auth_context);

    if (retval) {
#ifdef HEIMDAL
      if (err_ret && err_ret->e_text) {
          report(stderr, GT_("krb5_sendauth: %s [server says '%*s'] \n"),
                 error_message(retval),
                 err_ret->e_text);
#else
      if (err_ret && err_ret->text.length) {
          report(stderr, GT_("krb5_sendauth: %s [server says '%*s'] \n"),
		 error_message(retval),
		 err_ret->text.length,
		 err_ret->text.data);
#endif
	  krb5_free_error(context, err_ret);
      } else
          report(stderr, "krb5_sendauth: %s\n", error_message(retval));
      return(PS_ERROR);
    }

    return 0;
}
#endif /* KERBEROS_V5 */

static void clean_skipped_list(struct idlist **skipped_list)
/* struct "idlist" contains no "prev" ptr; we must remove unused items first */
{
    struct idlist *current=NULL, *prev=NULL, *tmp=NULL, *head=NULL;
    prev = current = head = *skipped_list;

    if (!head)
	return;
    do
    {
	/* if item has no reference, remove it */
	if (current && current->val.status.mark == 0)
	{
	    if (current == head) /* remove first item (head) */
	    {
		head = current->next;
		if (current->id) free(current->id);
		free(current);
		prev = current = head;
	    }
	    else /* remove middle/last item */
	    {
		tmp = current->next;
		prev->next = tmp;
		if (current->id) free(current->id);
		free(current);
		current = tmp;
	    }
	}
	else /* skip this item */
	{
	    prev = current;
	    current = current->next;
	}
    } while(current);

    *skipped_list = head;
}

static void send_size_warnings(struct query *ctl)
/* send warning mail with skipped msg; reset msg count when user notified */
{
    int size, nbr;
    int msg_to_send = FALSE;
    struct idlist *head=NULL, *current=NULL;
    int max_warning_poll_count;

    head = ctl->skipped;
    if (!head)
	return;

    /* don't start a notification message unless we need to */
    for (current = head; current; current = current->next)
	if (current->val.status.num == 0 && current->val.status.mark)
	    msg_to_send = TRUE;
    if (!msg_to_send)
	return;

    /*
     * There's no good way to recover if we can't send notification mail, 
     * but it's not a disaster, either, since the skipped mail will not
     * be deleted.
     */
    if (open_warning_by_mail(ctl, (struct msgblk *)NULL))
	return;
    stuff_warning(ctl,
	   GT_("Subject: Fetchmail oversized-messages warning.\n"
	     "\n"
	     "The following oversized messages remain on the mail server %s:"),
		  ctl->server.pollname);
 
    if (run.poll_interval == 0)
	max_warning_poll_count = 0;
    else
	max_warning_poll_count = ctl->warnings/run.poll_interval;

    /* parse list of skipped msg, adding items to the mail */
    for (current = head; current; current = current->next)
    {
	if (current->val.status.num == 0 && current->val.status.mark)
	{
	    nbr = current->val.status.mark;
	    size = atoi(current->id);
	    stuff_warning(ctl, 
		    GT_("\t%d msg %d octets long skipped by fetchmail.\n"),
		    nbr, size);
	}
	current->val.status.num++;
	current->val.status.mark = 0;

	if (current->val.status.num >= max_warning_poll_count)
	    current->val.status.num = 0;
    }

    close_warning_by_mail(ctl, (struct msgblk *)NULL);
}

static void mark_oversized(struct query *ctl, int num, int size)
/* mark a message oversized */
{
    struct idlist *current=NULL, *tmp=NULL;
    char sizestr[32];
    int cnt;

    /* convert size to string */
#ifdef HAVE_SNPRINTF
    snprintf(sizestr, sizeof(sizestr),
#else
    sprintf(sizestr,
#endif /* HAVE_SNPRINTF */
      "%d", size);

    /* build a list of skipped messages
     * val.id = size of msg (string cnvt)
     * val.status.num = warning_poll_count
     * val.status.mask = nbr of msg this size
     */

    current = ctl->skipped;

    /* initialise warning_poll_count to the
     * current value so that all new msg will
     * be included in the next mail
     */
    cnt = current ? current->val.status.num : 0;

    /* if entry exists, increment the count */
    if (current && str_in_list(&current, sizestr, FALSE))
    {
	for ( ; current; current = current->next)
	{
	    if (strcmp(current->id, sizestr) == 0)
	    {
		current->val.status.mark++;
		break;
	    }
	}
    }
    /* otherwise, create a new entry */
    /* initialise with current poll count */
    else
    {
	tmp = save_str(&ctl->skipped, sizestr, 1);
	tmp->val.status.num = cnt;
    }
}

static int fetch_messages(int mailserver_socket, struct query *ctl, 
			  int count, int *msgsizes, int *msgcodes, int maxfetch,
			  int *fetches, int *dispatches, int *deletions)
/* fetch messages in lockstep mode */
{
    int num, err, len;

    for (num = 1; num <= count; num++)
    {
	flag suppress_delete = FALSE;
	flag suppress_forward = FALSE;
	flag suppress_readbody = FALSE;
	flag retained = FALSE;

	if (msgcodes[num-1] < 0)
	{
	    if ((msgcodes[num-1] == MSGLEN_TOOLARGE) && !check_only)
		mark_oversized(ctl, num, msgsizes[num-1]);
  		/* To avoid flooding the syslog when using --keep,
  		 * report "Skipped message" only when:
  		 *  1) --verbose is on, or
  		 *  2) fetchmail does not use syslog, or
  		 *  3) the message was skipped for some other
  		 *     reason than being old.
  		 */
	    if (   (outlevel >= O_VERBOSE) ||
	           (outlevel > O_SILENT && (!run.use_syslog || msgcodes[num-1] != MSGLEN_OLD))
	       )
	    {
		report_build(stdout, 
			     GT_("skipping message %s@%s:%d (%d octets)"),
			     ctl->remotename, ctl->server.truename, num,
			     msgsizes[num-1]);
		switch (msgcodes[num-1])
		{
		case MSGLEN_INVALID:
		    /*
		     * Invalid lengths are produced by Post Office/NT's
		     * annoying habit of randomly prepending bogus
		     * LIST items of length -1.  Patrick Audley
		     * <paudley@pobox.com> tells us: LIST shows a
		     * size of -1, RETR and TOP return "-ERR
		     * System error - couldn't open message", and
		     * DELE succeeds but doesn't actually delete
		     * the message.
		     */
		    report_build(stdout, GT_(" (length -1)"));
		    break;
		case MSGLEN_TOOLARGE:
		    report_build(stdout, GT_(" (oversized)"));
		    break;
		}
	    }
	}
	else
	{
	    flag wholesize = !ctl->server.base_protocol->fetch_body;

	    /* request a message */
	    err = (ctl->server.base_protocol->fetch_headers)(mailserver_socket,ctl,num, &len);
	    if (err == PS_TRANSIENT)    /* server is probably Exchange */
	    {
		report(stdout,
			     GT_("couldn't fetch headers, message %s@%s:%d (%d octets)\n"),
			     ctl->remotename, ctl->server.truename, num,
			     msgsizes[num-1]);
		continue;
	    }
	    else if (err != 0)
		return(err);

	    /* -1 means we didn't see a size in the response */
	    if (len == -1)
	    {
		len = msgsizes[num - 1];
		wholesize = TRUE;
	    }

	    if (outlevel > O_SILENT)
	    {
		report_build(stdout, GT_("reading message %s@%s:%d of %d"),
			     ctl->remotename, ctl->server.truename,
			     num, count);

		if (len > 0)
		    report_build(stdout, GT_(" (%d %soctets)"),
				 len, wholesize ? "" : GT_("header "));
		if (outlevel >= O_VERBOSE)
		    report_complete(stdout, "\n");
		else
		    report_complete(stdout, " ");
	    }

	    /* 
	     * Read the message headers and ship them to the
	     * output sink.  
	     */
	    err = readheaders(mailserver_socket, len, msgsizes[num-1],
			     ctl, num);
	    if (err == PS_RETAINED)
	    {
		suppress_forward = suppress_delete = retained = TRUE;
		/* do not read the body only if the underlying protocol
		 * allows the body to be fetched separately */
		if (ctl->server.base_protocol->fetch_body)
		    suppress_readbody = TRUE;
	    }
	    else if (err == PS_TRANSIENT)
	    {
		suppress_delete = suppress_forward = TRUE;
		if (ctl->server.base_protocol->fetch_body)
		    suppress_readbody = TRUE;
	    }
	    else if (err == PS_REFUSED)
	    {
		suppress_forward = TRUE;
		if (ctl->server.base_protocol->fetch_body)
		    suppress_readbody = TRUE;
	    }
	    else if (err == PS_TRUNCATED)
	    {
		if (ctl->server.base_protocol->fetch_body)
		    suppress_readbody = TRUE;
		len = 0;	/* suppress body processing */
	    }
	    else if (err)
		return(err);

	    /* 
	     * If we're using IMAP4 or something else that
	     * can fetch headers separately from bodies,
	     * it's time to request the body now.  This
	     * fetch may be skipped if we got an anti-spam
	     * or other PS_REFUSED error response during
	     * readheaders.
	     */
	    if (ctl->server.base_protocol->fetch_body && !suppress_readbody) 
	    {
		if (outlevel >= O_VERBOSE && !isafile(1))
		{
		    fputc('\n', stdout);
		    fflush(stdout);
		}

		if ((err = (ctl->server.base_protocol->trail)(mailserver_socket, ctl, num)))
		    return(err);
		len = 0;
		if (!suppress_forward)
		{
		    if ((err=(ctl->server.base_protocol->fetch_body)(mailserver_socket,ctl,num,&len)))
			return(err);
		    /*
		     * Work around a bug in Novell's
		     * broken GroupWise IMAP server;
		     * its body FETCH response is missing
		     * the required length for the data
		     * string.  This violates RFC2060.
		     */
		    if (len == -1)
			len = msgsizes[num-1] - msgblk.msglen;
		    if (outlevel > O_SILENT && !wholesize)
			report_complete(stdout,
					GT_(" (%d body octets) "), len);
		}
	    }

	    /* process the body now */
	    if (len > 0)
	    {
		if (suppress_readbody)
		{
		    err = PS_SUCCESS;
		}
		else
		{
		    err = readbody(mailserver_socket,
				  ctl,
				  !suppress_forward,
				  len);
		}
		if (err == PS_TRANSIENT)
		    suppress_delete = suppress_forward = TRUE;
		else if (err)
		    return(err);

		/* tell server we got it OK and resynchronize */
		if (ctl->server.base_protocol->trail)
		{
		    if (outlevel >= O_VERBOSE && !isafile(1))
		    {
			fputc('\n', stdout);
			fflush(stdout);
		    }

		    err = (ctl->server.base_protocol->trail)(mailserver_socket, ctl, num);
		    if (err != 0)
			return(err);
		}
	    }

	    /* count # messages forwarded on this pass */
	    if (!suppress_forward)
		(*dispatches)++;

	    /*
	     * Check to see if the numbers matched?
	     *
	     * Yes, some servers foo this up horribly.
	     * All IMAP servers seem to get it right, and
	     * so does Eudora QPOP at least in 2.xx
	     * versions.
	     *
	     * Microsoft Exchange gets it completely
	     * wrong, reporting compressed rather than
	     * actual sizes (so the actual length of
	     * message is longer than the reported size).
	     * Another fine example of Microsoft brain death!
	     *
	     * Some older POP servers, like the old UCB
	     * POP server and the pre-QPOP QUALCOMM
	     * versions, report a longer size in the LIST
	     * response than actually gets shipped up.
	     * It's unclear what is going on here, as the
	     * QUALCOMM server (at least) seems to be
	     * reporting the on-disk size correctly.
	     */
	    if (msgblk.msglen != msgsizes[num-1])
	    {
		if (outlevel >= O_DEBUG)
		    report(stdout,
			   GT_("message %s@%s:%d was not the expected length (%d actual != %d expected)\n"),
			   ctl->remotename, ctl->server.truename, num,
			   msgblk.msglen, msgsizes[num-1]);
	    }

	    /* end-of-message processing starts here */
	    if (!close_sink(ctl, &msgblk, !suppress_forward))
	    {
		ctl->errcount++;
		suppress_delete = TRUE;
	    }
	    if (!retained)
		(*fetches)++;
	}

	/*
	 * At this point in flow of control, either
	 * we've bombed on a protocol error or had
	 * delivery refused by the SMTP server
	 * (unlikely -- I've never seen it) or we've
	 * seen `accepted for delivery' and the
	 * message is shipped.  It's safe to mark the
	 * message seen and delete it on the server
	 * now.
	 */

	/*
	 * Tell the UID code we've seen this.
	 * Matthias Andree: only register the UID if we could actually
	 * forward this mail. If we omit this !suppress_delete check,
	 * fetchmail will never retry mail that the local listener
	 * refused temporarily.
	 */
	if (ctl->newsaved && !suppress_delete)
	{
	    struct idlist	*sdp;

	    for (sdp = ctl->newsaved; sdp; sdp = sdp->next)
		if ((sdp->val.status.num == num) && (msgcodes[num-1] >= 0))
		{
		    sdp->val.status.mark = UID_SEEN;
		    save_str(&ctl->oldsaved, sdp->id,UID_SEEN);
		}
	}

	/* maybe we delete this message now? */
	if (retained)
	{
	    if (outlevel > O_SILENT) 
		report(stdout, GT_(" retained\n"));
	}
	else if (ctl->server.base_protocol->delete
		 && !suppress_delete
		 && ((msgcodes[num-1] >= 0) ? !ctl->keep : ctl->flush))
	{
	    (*deletions)++;
	    if (outlevel > O_SILENT) 
		report_complete(stdout, GT_(" flushed\n"));
	    err = (ctl->server.base_protocol->delete)(mailserver_socket, ctl, num);
	    if (err != 0)
		return(err);
#ifdef POP3_ENABLE
	    delete_str(&ctl->newsaved, num);
#endif /* POP3_ENABLE */
	}
	else if (   (outlevel >= O_VERBOSE) ||
         		/* To avoid flooding the syslog when using --keep,
         		 * report "Skipped message" only when:
         		 *  1) --verbose is on, or
         		 *  2) fetchmail does not use syslog, or
         		 *  3) the message was skipped for some other
         		 *     reason than just being old.
         		 */
	           (outlevel > O_SILENT && (!run.use_syslog || msgcodes[num-1] != MSGLEN_OLD))
	       )
	    report_complete(stdout, GT_(" not flushed\n"));

	/* perhaps this as many as we're ready to handle */
	if (maxfetch && maxfetch <= *fetches && *fetches < count)
	{
	    report(stdout, GT_("fetchlimit %d reached; %d messages left on server %s account %s\n"),
		   maxfetch, count - *fetches, ctl->server.truename, ctl->remotename);
	    return(PS_MAXFETCH);
	}
    }

    return(PS_SUCCESS);
}

static int do_session(ctl, proto, maxfetch)
/* retrieve messages from server using given protocol method table */
struct query *ctl;		/* parsed options with merged-in defaults */
const struct method *proto;	/* protocol method table */
const int maxfetch;		/* maximum number of messages to fetch */
{
    int js;
#ifdef HAVE_VOLATILE
    volatile int err, mailserver_socket = -1;	/* pacifies -Wall */
#else
    int err, mailserver_socket = -1;
#endif /* HAVE_VOLATILE */
    const char *msg;
    void (*pipesave)(int);
    void (*alrmsave)(int);

    ctl->server.base_protocol = proto;

    pass = 0;
    err = 0;
    init_transact(proto);

    /* set up the server-nonresponse timeout */
    alrmsave = signal(SIGALRM, timeout_handler);
    mytimeout = ctl->server.timeout;

    /* set up the broken-pipe timeout */
    pipesave = signal(SIGPIPE, sigpipe_handler);

    if ((js = setjmp(restart)))
    {
#ifdef HAVE_SIGPROCMASK
	/*
	 * Don't rely on setjmp() to restore the blocked-signal mask.
	 * It does this under BSD but is required not to under POSIX.
	 *
	 * If your Unix doesn't have sigprocmask, better hope it has
	 * BSD-like behavior.  Otherwise you may see fetchmail get
	 * permanently wedged after a second timeout on a bad read,
	 * because alarm signals were blocked after the first.
	 */
	sigset_t	allsigs;

	sigfillset(&allsigs);
	sigprocmask(SIG_UNBLOCK, &allsigs, NULL);
#endif /* HAVE_SIGPROCMASK */
	
	if (js == THROW_SIGPIPE)
	{
	    signal(SIGPIPE, SIG_IGN);
	    report(stdout,
		   GT_("SIGPIPE thrown from an MDA or a stream socket error\n"));
	    wait(0);
	}
	else if (js == THROW_TIMEOUT)
	{
	    if (phase == OPEN_WAIT)
		report(stdout,
		       GT_("timeout after %d seconds waiting to connect to server %s.\n"),
		       ctl->server.timeout, ctl->server.pollname);
	    else if (phase == SERVER_WAIT)
		report(stdout,
		       GT_("timeout after %d seconds waiting for server %s.\n"),
		       ctl->server.timeout, ctl->server.pollname);
	    else if (phase == FORWARDING_WAIT)
		report(stdout,
		       GT_("timeout after %d seconds waiting for %s.\n"),
		       ctl->server.timeout,
		       ctl->mda ? "MDA" : "SMTP");
	    else if (phase == LISTENER_WAIT)
		report(stdout,
		       GT_("timeout after %d seconds waiting for listener to respond.\n"), ctl->server.timeout);
	    else
		report(stdout, 
		       GT_("timeout after %d seconds.\n"), ctl->server.timeout);

	    /*
	     * If we've exceeded our threshold for consecutive timeouts, 
	     * try to notify the user, then mark the connection wedged.
	     * Don't do this if the connection can idle, though; idle
	     * timeouts just mean the frequency of mail is low.
	     */
	    if (timeoutcount > MAX_TIMEOUTS 
		&& !open_warning_by_mail(ctl, (struct msgblk *)NULL))
	    {
		stuff_warning(ctl,
			      GT_("Subject: fetchmail sees repeated timeouts\n"));
		stuff_warning(ctl,
			      GT_("Fetchmail saw more than %d timeouts while attempting to get mail from %s@%s.\n"), 
			      MAX_TIMEOUTS,
			      ctl->remotename,
			      ctl->server.truename);
		stuff_warning(ctl, 
    GT_("This could mean that your mailserver is stuck, or that your SMTP\n" \
    "server is wedged, or that your mailbox file on the server has been\n" \
    "corrupted by a server error.  You can run `fetchmail -v -v' to\n" \
    "diagnose the problem.\n\n" \
    "Fetchmail won't poll this mailbox again until you restart it.\n"));
		close_warning_by_mail(ctl, (struct msgblk *)NULL);
		ctl->wedged = TRUE;
	    }
	}

	err = PS_SOCKET;
	goto cleanUp;
    }
    else
    {
	char buf[MSGBUFSIZE+1], *realhost;
	int count, new, bytes, deletions = 0;
	int *msgsizes = (int *)NULL;
	int *msgcodes = (int *)NULL;
#if INET6_ENABLE
	int fetches, dispatches, oldphase;
#else /* INET6_ENABLE */
	int port, fetches, dispatches, oldphase;
#endif /* INET6_ENABLE */
	struct idlist *idp;

	/* execute pre-initialization command, if any */
	if (ctl->preconnect && (err = system(ctl->preconnect)))
	{
	    report(stderr, 
		   GT_("pre-connection command failed with status %d\n"), err);
	    err = PS_SYNTAX;
	    goto closeUp;
	}

	/* open a socket to the mail server */
	oldphase = phase;
	phase = OPEN_WAIT;
	set_timeout(mytimeout);
#if !INET6_ENABLE
#ifdef SSL_ENABLE
	port = ctl->server.port ? ctl->server.port : ( ctl->use_ssl ? ctl->server.base_protocol->sslport : ctl->server.base_protocol->port );
#else
	port = ctl->server.port ? ctl->server.port : ctl->server.base_protocol->port;
#endif
#endif /* !INET6_ENABLE */

#ifdef HESIOD
	/* If either the pollname or vianame are "hesiod" we want to
	   lookup the user's hesiod pobox host */
	if (!strcasecmp(ctl->server.queryname, "hesiod")) {
	    struct hes_postoffice *hes_p;
	    hes_p = hes_getmailhost(ctl->remotename);
	    if (hes_p != NULL && strcmp(hes_p->po_type, "POP") == 0) {
		 free(ctl->server.queryname);
		 ctl->server.queryname = xstrdup(hes_p->po_host);
		 if (ctl->server.via)
		     free(ctl->server.via);
		 ctl->server.via = xstrdup(hes_p->po_host);
	    } else {
		 report(stderr,
			GT_("couldn't find HESIOD pobox for %s\n"),
			ctl->remotename);
	    }
	}
#endif /* HESIOD */

#ifdef HAVE_GETHOSTBYNAME
	/*
	 * Canonicalize the server truename for later use.  This also
	 * functions as a probe for whether the mailserver is accessible.
	 * We try it on each poll cycle until we get a result.  This way,
	 * fetchmail won't fail if started up when the network is inaccessible.
	 */
	if (ctl->server.dns && !ctl->server.trueaddr)
	{
	    if (ctl->server.lead_server)
	    {
		char	*leadname = ctl->server.lead_server->truename;

		/* prevent core dump from ill-formed or duplicate entry */
		if (!leadname)
		{
		    report(stderr, GT_("Lead server has no name.\n"));
		    err = PS_DNS;
		    set_timeout(0);
		    phase = oldphase;
		    goto closeUp;
		}

		ctl->server.truename = xstrdup(leadname);
	    }
	    else
	    {
		struct hostent	*namerec;
		    
		/* 
		 * Get the host's IP, so we can report it like this:
		 *
		 * Received: from hostname [10.0.0.1]
		 */
		errno = 0;
		namerec = gethostbyname(ctl->server.queryname);
		if (namerec == (struct hostent *)NULL)
		{
		    report(stderr,
			   GT_("couldn't find canonical DNS name of %s\n"),
			   ctl->server.pollname);
		    err = PS_DNS;
		    set_timeout(0);
		    phase = oldphase;
		    goto closeUp;
		}
		else 
		{
		    ctl->server.truename=xstrdup((char *)namerec->h_name);
		    ctl->server.trueaddr=xmalloc(namerec->h_length);
		    memcpy(ctl->server.trueaddr, 
			   namerec->h_addr_list[0],
			   namerec->h_length);
		}
	    }
	}
#endif /* HAVE_GETHOSTBYNAME */

	realhost = ctl->server.via ? ctl->server.via : ctl->server.pollname;

	/* allow time for the port to be set up if we have a plugin */
	if (ctl->server.plugin)
	    (void)sleep(1);
#if INET6_ENABLE
	if ((mailserver_socket = SockOpen(realhost, 
			     ctl->server.service ? ctl->server.service : ( ctl->use_ssl ? ctl->server.base_protocol->sslservice : ctl->server.base_protocol->service ),
			     ctl->server.netsec, ctl->server.plugin)) == -1)
#else /* INET6_ENABLE */
	if ((mailserver_socket = SockOpen(realhost, port, NULL, ctl->server.plugin)) == -1)
#endif /* INET6_ENABLE */
	{
	    char	errbuf[BUFSIZ];
#if !INET6_ENABLE
	    int err_no = errno;
#ifdef HAVE_RES_SEARCH
	    if (err_no != 0 && h_errno != 0)
		report(stderr, GT_("internal inconsistency\n"));
#endif
	    /*
	     * Avoid generating a bogus error every poll cycle when we're
	     * in daemon mode but the connection to the outside world
	     * is down.
	     */
	    if (!((err_no == EHOSTUNREACH || err_no == ENETUNREACH) 
		  && run.poll_interval))
	    {
		report_build(stderr, GT_("%s connection to %s failed"), 
			     ctl->server.base_protocol->name, ctl->server.pollname);
#ifdef HAVE_RES_SEARCH
		if (h_errno != 0)
		{
		    if (h_errno == HOST_NOT_FOUND)
			strcpy(errbuf, GT_("host is unknown."));
#ifndef __BEOS__
		    else if (h_errno == NO_ADDRESS)
			strcpy(errbuf, GT_("name is valid but has no IP address."));
#endif
		    else if (h_errno == NO_RECOVERY)
			strcpy(errbuf, GT_("unrecoverable name server error."));
		    else if (h_errno == TRY_AGAIN)
			strcpy(errbuf, GT_("temporary name server error."));
		    else
#ifdef HAVE_SNPRINTF
			snprintf(errbuf, sizeof(errbuf),
#else
			sprintf(errbuf,
#endif /* HAVE_SNPRINTF */
			  GT_("unknown DNS error %d."), h_errno);
		}
		else
#endif /* HAVE_RES_SEARCH */
		    strcpy(errbuf, strerror(err_no));
		report_complete(stderr, ": %s\n", errbuf);

#ifdef __UNUSED
		/* 
		 * Don't use this.  It was an attempt to address Debian bug
		 * #47143 (Notify user by mail when pop server nonexistent).
		 * Trouble is, that doesn't work; you trip over the case 
		 * where your SLIP or PPP link is down...
		 */
		/* warn the system administrator */
		if (open_warning_by_mail(ctl, (struct msgblk *)NULL) == 0)
		{
		    stuff_warning(ctl,
			 GT_("Subject: Fetchmail unreachable-server warning.\n"
			   "\n"
			   "Fetchmail could not reach the mail server %s:")
				  ctl->server.pollname);
		    stuff_warning(ctl, errbuf, ctl->server.pollname);
		    close_warning_by_mail(ctl, (struct msgblk *)NULL);
		}
#endif
	    }
#endif /* INET6_ENABLE */
	    err = PS_SOCKET;
	    set_timeout(0);
	    phase = oldphase;
	    goto closeUp;
	}

#ifdef SSL_ENABLE
	/* Save the socket opened. Usefull if Fetchmail hangs on SSLOpen 
	 * because the socket can be closed
	 */
	mailserver_socket_temp = mailserver_socket;
	set_timeout(mytimeout);

	/* perform initial SSL handshake on open connection */
	/* Note:  We pass the realhost name over for certificate
		verification.  We may want to make this configurable */
	if (ctl->use_ssl && SSLOpen(mailserver_socket,ctl->sslcert,ctl->sslkey,ctl->sslproto,ctl->sslcertck,
	    ctl->sslcertpath,ctl->sslfingerprint,realhost,ctl->server.pollname) == -1) 
	{
	    report(stderr, GT_("SSL connection failed.\n"));
	    goto closeUp;
	}
	
	/* Fetchmail didn't hang on SSLOpen, 
	 * then no need to set mailserver_socket_temp 
	 */
	mailserver_socket_temp = -1;
#endif
	
	/* A timeout is still defined before SSLOpen, 
	 * then Fetchmail hanging on SSLOpen is handled.
	 */
	set_timeout(0);
	phase = oldphase;
#ifdef KERBEROS_V4
	if (ctl->server.authenticate == A_KERBEROS_V4 && (strcasecmp(proto->name,"IMAP") != 0))
	{
	    set_timeout(mytimeout);
	    err = kerberos_auth(mailserver_socket, ctl->server.truename,
			       ctl->server.principal);
	    set_timeout(0);
 	    if (err != 0)
		goto cleanUp;
	}
#endif /* KERBEROS_V4 */

#ifdef KERBEROS_V5
	if (ctl->server.authenticate == A_KERBEROS_V5)
	{
	    set_timeout(mytimeout);
	    err = kerberos5_auth(mailserver_socket, ctl->server.truename);
	    set_timeout(0);
 	    if (err != 0)
		goto cleanUp;
	}
#endif /* KERBEROS_V5 */

	/* accept greeting message from mail server */
	err = (ctl->server.base_protocol->parse_response)(mailserver_socket, buf);
	if (err != 0)
	    goto cleanUp;

	/* try to get authorized to fetch mail */
	stage = STAGE_GETAUTH;
	if (ctl->server.base_protocol->getauth)
	{
	    err = (ctl->server.base_protocol->getauth)(mailserver_socket, ctl, buf);

	    if (err != 0)
	    {
		if (err == PS_LOCKBUSY)
		    report(stderr, GT_("Lock-busy error on %s@%s\n"),
			  ctl->remotename,
			  ctl->server.truename);
		else if (err == PS_SERVBUSY)
		    report(stderr, GT_("Server busy error on %s@%s\n"),
			  ctl->remotename,
			  ctl->server.truename);
		else if (err == PS_AUTHFAIL)
		{
		    report(stderr, GT_("Authorization failure on %s@%s%s\n"), 
			   ctl->remotename,
			   ctl->server.truename,
			   (ctl->wehaveauthed ? GT_(" (previously authorized)") : "")
			);

		    /*
		     * If we're running in background, try to mail the
		     * calling user a heads-up about the authentication 
		     * failure once it looks like this isn't a fluke 
		     * due to the server being temporarily inaccessible.
		     * When we get third succesive failure, we notify the user
		     * but only if we haven't already managed to get
		     * authorization.  After that, once we get authorization
		     * we let the user know service is restored.
		     */
		    if (run.poll_interval
			&& !ctl->wehavesentauthnote
			&& ((ctl->wehaveauthed && ++ctl->authfailcount >= 10)
			    || (!ctl->wehaveauthed && ++ctl->authfailcount >= 3))
			&& !open_warning_by_mail(ctl, (struct msgblk *)NULL))
		    {
			ctl->wehavesentauthnote = 1;
			stuff_warning(ctl,
				      GT_("Subject: fetchmail authentication failed on %s@%s\n"),
			    ctl->remotename, ctl->server.truename);
			stuff_warning(ctl,
				      GT_("Fetchmail could not get mail from %s@%s.\n"), 
				      ctl->remotename,
				      ctl->server.truename);
			if (ctl->wehaveauthed)
			    stuff_warning(ctl, GT_("\
The attempt to get authorization failed.\n\
Since we have already succeeded in getting authorization for this\n\
connection, this is probably another failure mode (such as busy server)\n\
that fetchmail cannot distinguish because the server didn't send a useful\n\
error message.\n\
\n\
However, if you HAVE changed your account details since starting the\n\
fetchmail daemon, you need to stop the daemon, change your configuration\n\
of fetchmail, and then restart the daemon.\n\
\n\
The fetchmail daemon will continue running and attempt to connect\n\
at each cycle.  No future notifications will be sent until service\n\
is restored."));
			else
			    stuff_warning(ctl, GT_("\
The attempt to get authorization failed.\n\
This probably means your password is invalid, but some servers have\n\
other failure modes that fetchmail cannot distinguish from this\n\
because they don't send useful error messages on login failure.\n\
\n\
The fetchmail daemon will continue running and attempt to connect\n\
at each cycle.  No future notifications will be sent until service\n\
is restored."));
			close_warning_by_mail(ctl, (struct msgblk *)NULL);
		    }
		}
		else if (err == PS_REPOLL)
		{
		  if (outlevel >= O_VERBOSE)
		    report(stderr, GT_("Repoll immediately on %s@%s\n"),
			   ctl->remotename,
			   ctl->server.truename);
		}
		else
		    report(stderr, GT_("Unknown login or authentication error on %s@%s\n"),
			   ctl->remotename,
			   ctl->server.truename);
		    
		goto cleanUp;
	    }
	    else
	    {
		/*
		 * This connection has given us authorization at least once.
		 *
		 * There are dodgy server (clubinternet.fr for example) that
		 * give spurious authorization failures on patently good
		 * account/password details, then 5 minutes later let you in!
		 *
		 * This is meant to build in some tolerance of such nasty bits
		 * of work.
		 */
		ctl->wehaveauthed = 1;
		/*if (ctl->authfailcount >= 3)*/
		if (ctl->wehavesentauthnote)
		{
		    ctl->wehavesentauthnote = 0;
		    report(stderr,
			   GT_("Authorization OK on %s@%s\n"),
			   ctl->remotename,
			   ctl->server.truename);
		    if (!open_warning_by_mail(ctl, (struct msgblk *)NULL))
		    {
			stuff_warning(ctl,
			      GT_("Subject: fetchmail authentication OK on %s@%s\n"),
				      ctl->remotename, ctl->server.truename);
			stuff_warning(ctl,
			      GT_("Fetchmail was able to log into %s@%s.\n"), 
				      ctl->remotename,
				      ctl->server.truename);
			stuff_warning(ctl, 
				      GT_("Service has been restored.\n"));
			close_warning_by_mail(ctl, (struct msgblk *)NULL);
		    
		    }
		}
		/*
		 * Reporting only after the first three
		 * consecutive failures, or ten consecutive
		 * failures after we have managed to get
		 * authorization.
		 */
		ctl->authfailcount = 0;
	    }
	}

	ctl->errcount = fetches = 0;

	/* now iterate over each folder selected */
	for (idp = ctl->mailboxes; idp; idp = idp->next)
	{
	    pass = 0;
	    do {
		dispatches = 0;
		++pass;

		/* reset timeout, in case we did an IDLE */
		mytimeout = ctl->server.timeout;

		if (outlevel >= O_DEBUG)
		{
		    if (idp->id)
			report(stdout, GT_("selecting or re-polling folder %s\n"), idp->id);
		    else
			report(stdout, GT_("selecting or re-polling default folder\n"));
		}

		/* compute # of messages and number of new messages waiting */
		stage = STAGE_GETRANGE;
		err = (ctl->server.base_protocol->getrange)(mailserver_socket, ctl, idp->id, &count, &new, &bytes);
		if (err != 0)
		    goto cleanUp;

		/* show user how many messages we downloaded */
		if (idp->id)
#ifdef HAVE_SNPRINTF
		    (void) snprintf(buf, sizeof(buf),
#else
		    (void) sprintf(buf,
#endif /* HAVE_SNPRINTF */
				   GT_("%s at %s (folder %s)"),
				   ctl->remotename, ctl->server.truename, idp->id);
		else
#ifdef HAVE_SNPRINTF
		    (void) snprintf(buf, sizeof(buf),
#else
		    (void) sprintf(buf,
#endif /* HAVE_SNPRINTF */
			       GT_("%s at %s"),
				   ctl->remotename, ctl->server.truename);
		if (outlevel > O_SILENT)
		{
		    if (count == -1)		/* only used for ETRN */
			report(stdout, GT_("Polling %s\n"), ctl->server.truename);
		    else if (count != 0)
		    {
			if (new != -1 && (count - new) > 0)
			    report_build(stdout, GT_("%d %s (%d seen) for %s"),
				  count, count > 1 ? GT_("messages") :
				                     GT_("message"),
				  count-new, buf);
			else
			    report_build(stdout, GT_("%d %s for %s"), 
				  count, count > 1 ? GT_("messages") :
				                     GT_("message"), buf);
			if (bytes == -1)
			    report_complete(stdout, ".\n");
			else
			    report_complete(stdout, GT_(" (%d octets).\n"), bytes);
		    }
		    else
		    {
			/* these are pointless in normal daemon mode */
			if (pass == 1 && (run.poll_interval == 0 || outlevel >= O_VERBOSE))
			    report(stdout, GT_("No mail for %s\n"), buf); 
		    }
		}

		/* very important, this is where we leave the do loop */ 
		if (count == 0)
		    break;

		if (check_only)
		{
		    if (new == -1 || ctl->fetchall)
			new = count;
		    fetches = new;	/* set error status correctly */
		    /*
		     * There used to be a `goto noerror' here, but this
		     * prevented checking of multiple folders.  This
		     * comment is a reminder in case I introduced some
		     * subtle bug by removing it...
		     */
		}
		else if (count > 0)
		{    
		    flag	force_retrieval;
		    int		i, num;

		    /*
		     * What forces this code is that in POP2 and
		     * IMAP2bis you can't fetch a message without
		     * having it marked `seen'.  In POP3 and IMAP4, on the
		     * other hand, you can (peek_capable is set by 
		     * each driver module to convey this; it's not a
		     * method constant because of the difference between
		     * IMAP2bis and IMAP4, and because POP3 doesn't  peek
		     * if fetchall is on).
		     *
		     * The result of being unable to peek is that if there's
		     * any kind of transient error (DNS lookup failure, or
		     * sendmail refusing delivery due to process-table limits)
		     * the message will be marked "seen" on the server without
		     * having been delivered.  This is not a big problem if
		     * fetchmail is running in foreground, because the user
		     * will see a "skipped" message when it next runs and get
		     * clued in.
		     *
		     * But in daemon mode this leads to the message
		     * being silently ignored forever.  This is not
		     * acceptable.
		     *
		     * We compensate for this by checking the error
		     * count from the previous pass and forcing all
		     * messages to be considered new if it's nonzero.
		     */
		    force_retrieval = !peek_capable && (ctl->errcount > 0);

		    /*
		     * Don't trust the message count passed by the server.
		     * Without this check, it might be possible to do a
		     * DNS-spoofing attack that would pass back a ridiculous 
		     * count, and allocate a malloc area that would overlap
		     * a portion of the stack.
		     */
		    if (count > INT_MAX/sizeof(int))
		    {
			report(stderr, GT_("bogus message count!"));
			return(PS_PROTOCOL);
		    }

		    /* OK, we're going to gather size info next */
		    xalloca(msgsizes, int *, sizeof(int) * count);
		    xalloca(msgcodes, int *, sizeof(int) * count);
		    for (i = 0; i < count; i++) {
			msgsizes[i] = 0;
			msgcodes[i] = MSGLEN_UNKNOWN;
		    }

		    /* 
		     * We need the size of each message before it's
		     * loaded in order to pass it to the ESMTP SIZE
		     * option.  If the protocol has a getsizes method,
		     * we presume this means it doesn't get reliable
		     * sizes from message fetch responses.
		     */
		    if (proto->getsizes)
		    {
			stage = STAGE_GETSIZES;
			err = (proto->getsizes)(mailserver_socket, count, msgsizes);
			if (err != 0)
			    goto cleanUp;

			if (bytes == -1)
			{
			    bytes = 0;
			    for (i = 0; i < count; i++)
				bytes += msgsizes[i];
			}
		    }

		    /* mark some messages not to be retrieved */
		    for (num = 1; num <= count; num++)
		    {
			if (NUM_NONZERO(ctl->limit) && (msgsizes[num-1] > ctl->limit))
			    msgcodes[num-1] = MSGLEN_TOOLARGE;
			else if (ctl->fetchall || force_retrieval)
			    continue;
			else if (ctl->server.base_protocol->is_old && (ctl->server.base_protocol->is_old)(mailserver_socket,ctl,num))
			    msgcodes[num-1] = MSGLEN_OLD;
/*			else if (msgsizes[num-1] == 512)
				msgcodes[num-1] = MSGLEN_OLD;  (hmh) sample code to skip message */
		    }

		    /* read, forward, and delete messages */
		    stage = STAGE_FETCH;

		    /* fetch in lockstep mode */
		    err = fetch_messages(mailserver_socket, ctl, 
					 count, msgsizes, msgcodes,
					 maxfetch,
					 &fetches, &dispatches, &deletions);
		    if (err)
			goto cleanUp;

		    if (!check_only && ctl->skipped
			&& run.poll_interval > 0 && !nodetach)
		    {
			clean_skipped_list(&ctl->skipped);
			send_size_warnings(ctl);
		    }
		}
	    } while
		  /*
		   * Only re-poll if we either had some actual forwards and 
		   * either allowed deletions and had no errors.
		   * Otherwise it is far too easy to get into infinite loops.
		   */
		  (dispatches && ctl->server.base_protocol->retry && !ctl->keep && !ctl->errcount);
	}

    /* no_error: */
	/* ordinary termination with no errors -- officially log out */
	err = (ctl->server.base_protocol->logout_cmd)(mailserver_socket, ctl);
	/*
	 * Hmmmm...arguably this would be incorrect if we had fetches but
	 * no dispatches (due to oversized messages, etc.)
	 */
	if (err == 0)
	    err = (fetches > 0) ? PS_SUCCESS : PS_NOMAIL;
	cleanupSockClose(mailserver_socket);
	goto closeUp;

    cleanUp:
	/* we only get here on error */
	if (err != 0 && err != PS_SOCKET && err != PS_REPOLL)
	{
	    stage = STAGE_LOGOUT;
	    (ctl->server.base_protocol->logout_cmd)(mailserver_socket, ctl);
	}

	/* try to clean up all streams */
	release_sink(ctl);
	smtp_close(ctl, 0);
	if (mailserver_socket != -1) {
	    cleanupSockClose(mailserver_socket);
	    mailserver_socket = -1;
	}
	/* If there was a connect timeout, the socket should be closed.
	 * mailserver_socket_temp contains the socket to close.
	 */
	if (mailserver_socket_temp != -1) {
	    cleanupSockClose(mailserver_socket_temp);
	    mailserver_socket_temp = -1;
	}
    }

    msg = (const char *)NULL;	/* sacrifice to -Wall */
    switch (err)
    {
    case PS_SOCKET:
	msg = GT_("socket");
	break;
    case PS_SYNTAX:
	msg = GT_("missing or bad RFC822 header");
	break;
    case PS_IOERR:
	msg = GT_("MDA");
	break;
    case PS_ERROR:
	msg = GT_("client/server synchronization");
	break;
    case PS_PROTOCOL:
	msg = GT_("client/server protocol");
	break;
    case PS_LOCKBUSY:
	msg = GT_("lock busy on server");
	break;
    case PS_SMTP:
	msg = GT_("SMTP transaction");
	break;
    case PS_DNS:
	msg = GT_("DNS lookup");
	break;
    case PS_UNDEFINED:
	report(stderr, GT_("undefined error\n"));
	break;
    }
    /* no report on PS_MAXFETCH or PS_UNDEFINED or PS_AUTHFAIL */
    if (err==PS_SOCKET || err==PS_SYNTAX
		|| err==PS_IOERR || err==PS_ERROR || err==PS_PROTOCOL 
		|| err==PS_LOCKBUSY || err==PS_SMTP || err==PS_DNS)
    {
	char	*stem;

	if (phase == FORWARDING_WAIT || phase == LISTENER_WAIT)
	    stem = GT_("%s error while delivering to SMTP host %s\n");
	else
	    stem = GT_("%s error while fetching from %s\n");
	report(stderr, stem, msg, ctl->server.pollname);
    }

closeUp:
    /* execute wrapup command, if any */
    if (ctl->postconnect && (err = system(ctl->postconnect)))
    {
	report(stderr, GT_("post-connection command failed with status %d\n"), err);
	if (err == PS_SUCCESS)
	    err = PS_SYNTAX;
    }

    set_timeout(0); /* cancel any pending alarm */
    signal(SIGALRM, alrmsave);
    signal(SIGPIPE, pipesave);
    return(err);
}

int do_protocol(ctl, proto)
/* retrieve messages from server using given protocol method table */
struct query *ctl;		/* parsed options with merged-in defaults */
const struct method *proto;	/* protocol method table */
{
    int	err;

#ifndef KERBEROS_V4
    if (ctl->server.authenticate == A_KERBEROS_V4)
    {
	report(stderr, GT_("Kerberos V4 support not linked.\n"));
	return(PS_ERROR);
    }
#endif /* KERBEROS_V4 */

#ifndef KERBEROS_V5
    if (ctl->server.authenticate == A_KERBEROS_V5)
    {
	report(stderr, GT_("Kerberos V5 support not linked.\n"));
	return(PS_ERROR);
    }
#endif /* KERBEROS_V5 */

    /* lacking methods, there are some options that may fail */
    if (!proto->is_old)
    {
	/* check for unsupported options */
	if (ctl->flush) {
	    report(stderr,
		    GT_("Option --flush is not supported with %s\n"),
		    proto->name);
	    return(PS_SYNTAX);
	}
	else if (ctl->fetchall) {
	    report(stderr,
		    GT_("Option --all is not supported with %s\n"),
		    proto->name);
	    return(PS_SYNTAX);
	}
    }
    if (!proto->getsizes && NUM_SPECIFIED(ctl->limit))
    {
	report(stderr,
		GT_("Option --limit is not supported with %s\n"),
		proto->name);
	return(PS_SYNTAX);
    }

    /*
     * If no expunge limit or we do expunges within the driver,
     * then just do one session, passing in any fetchlimit.
     */
    if (proto->retry || !NUM_SPECIFIED(ctl->expunge))
	return(do_session(ctl, proto, NUM_VALUE_OUT(ctl->fetchlimit)));
    /*
     * There's an expunge limit, and it isn't handled in the driver itself.
     * OK; do multiple sessions, each fetching a limited # of messages.
     * Stop if the total count of retrieved messages exceeds ctl->fetchlimit
     * (if it was nonzero).
     */
    else
    {
	int totalcount = 0; 
	int lockouts   = 0;
	int expunge    = NUM_VALUE_OUT(ctl->expunge);
	int fetchlimit = NUM_VALUE_OUT(ctl->fetchlimit);

	do {
	    if (fetchlimit > 0 && (expunge == 0 || expunge > fetchlimit - totalcount))
		expunge = fetchlimit - totalcount;
	    err = do_session(ctl, proto, expunge);
	    totalcount += expunge;
	    if (NUM_SPECIFIED(ctl->fetchlimit) && totalcount >= fetchlimit)
		break;
	    if (err != PS_LOCKBUSY)
		lockouts = 0;
	    else if (lockouts >= MAX_LOCKOUTS)
		break;
	    else /* err == PS_LOCKBUSY */
	    {
		/*
		 * Allow time for the server lock to release.  if we
		 * don't do this, we'll often hit a locked-mailbox
		 * condition and fail.
		 */
		lockouts++;
		sleep(3);
	    }
	} while
	    (err == PS_MAXFETCH || err == PS_LOCKBUSY);

	return(err);
    }
}


/* driver.c ends here */
