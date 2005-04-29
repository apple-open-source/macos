/*++
/* NAME
/*	smtp_session 3
/* SUMMARY
/*	SMTP_SESSION structure management
/* SYNOPSIS
/*	#include "smtp.h"
/*
/*	SMTP_SESSION *smtp_session_alloc(stream, host, addr)
/*	VSTREAM *stream;
/*	char	*host;
/*	char	*addr;
/*
/*	void	smtp_session_free(session)
/*	SMTP_SESSION *session;
/* DESCRIPTION
/*	smtp_session_alloc() allocates memory for an SMTP_SESSION structure
/*	and initializes it with the given stream and host name and address
/*	information.  The host name and address strings are copied. The code
/*	assumes that the stream is connected to the "best" alternative.
/*
/*	smtp_session_free() destroys an SMTP_SESSION structure and its
/*	members, making memory available for reuse.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>

/* Utility library. */

#include <mymalloc.h>
#include <vstream.h>
#include <stringops.h>

#include <mail_params.h>
#include <maps.h>
#include <pfixtls.h>

/* Application-specific. */

#include "smtp.h"

/* static lists */
static MAPS *tls_per_site;

/* smtp_tls_list_init - initialize lists */

void smtp_tls_list_init(void)
{
    tls_per_site = maps_create(VAR_SMTP_TLS_PER_SITE, var_smtp_tls_per_site,
			       DICT_FLAG_LOCK);
}

/* smtp_session_alloc - allocate and initialize SMTP_SESSION structure */

SMTP_SESSION *smtp_session_alloc(char *dest, VSTREAM *stream, char *host, char *addr)
{
    SMTP_SESSION *session;
    const char *lookup;
    char *lookup_key;
    int host_dont_use = 0;
    int host_use = 0;
    int host_enforce = 0;
    int host_enforce_peername = 0;
    int recipient_dont_use = 0;
    int recipient_use = 0;
    int recipient_enforce = 0;
    int recipient_enforce_peername = 0;

    session = (SMTP_SESSION *) mymalloc(sizeof(*session));
    session->stream = stream;
    session->host = mystrdup(host);
    session->addr = mystrdup(addr);
    session->namaddr = concatenate(host, "[", addr, "]", (char *) 0);
    session->best = 1;
    session->tls_use_tls = session->tls_enforce_tls = 0;
    session->tls_enforce_peername = 0;
#ifdef USE_SSL
    lookup_key = lowercase(mystrdup(host));
    if (lookup = maps_find(tls_per_site, lookup_key, 0)) {
	if (!strcasecmp(lookup, "NONE"))
	    host_dont_use = 1;
	else if (!strcasecmp(lookup, "MAY"))
	    host_use = 1;
	else if (!strcasecmp(lookup, "MUST"))
	    host_enforce = host_enforce_peername = 1;
	else if (!strcasecmp(lookup, "MUST_NOPEERMATCH"))
	    host_enforce = 1;
	else
	    msg_warn("Unknown TLS state for receiving host %s: '%s', using default policy", session->host, lookup);
    }
    myfree(lookup_key);
    lookup_key = lowercase(mystrdup(dest));
    if (lookup = maps_find(tls_per_site, dest, 0)) {
	if (!strcasecmp(lookup, "NONE"))
	    recipient_dont_use = 1;
	else if (!strcasecmp(lookup, "MAY"))
	    recipient_use = 1;
	else if (!strcasecmp(lookup, "MUST"))
	    recipient_enforce = recipient_enforce_peername = 1;
	else if (!strcasecmp(lookup, "MUST_NOPEERMATCH"))
	    recipient_enforce = 1;
	else
	    msg_warn("Unknown TLS state for recipient domain %s: '%s', using default policy", dest, lookup);
    }
    myfree(lookup_key);

    if ((var_smtp_enforce_tls && !host_dont_use && !recipient_dont_use) || host_enforce ||
	 recipient_enforce)
	session->tls_enforce_tls = session->tls_use_tls = 1;

    /*
     * Set up peername checking. We want to make sure that a MUST* entry in
     * the tls_per_site table always has precedence. MUST always must lead to
     * a peername check, MUST_NOPEERMATCH must always disable it. Only when
     * no explicit setting has been found, the default will be used.
     * There is the case left, that both "host" and "recipient" settings
     * conflict. In this case, the "host" setting wins.
     */
    if (host_enforce && host_enforce_peername)
	session->tls_enforce_peername = 1;
    else if (recipient_enforce && recipient_enforce_peername)
	session->tls_enforce_peername = 1;
    else if (var_smtp_enforce_tls && var_smtp_tls_enforce_peername)
	session->tls_enforce_peername = 1;

    else if ((var_smtp_use_tls && !host_dont_use && !recipient_dont_use) || host_use || recipient_use)
      session->tls_use_tls = 1;
#endif
    session->tls_info = tls_info_zero;
    return (session);
}

/* smtp_session_free - destroy SMTP_SESSION structure and contents */

void    smtp_session_free(SMTP_SESSION *session)
{
#ifdef USE_SSL
    vstream_fflush(session->stream);
    pfixtls_stop_clienttls(session->stream, var_smtp_starttls_tmout, 0,
			   &(session->tls_info));
#endif
    vstream_fclose(session->stream);
    myfree(session->host);
    myfree(session->addr);
    myfree(session->namaddr);
    myfree((char *) session);
}
