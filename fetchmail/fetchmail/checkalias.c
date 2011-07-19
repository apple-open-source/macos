/*
 * checkalias.c -- check to see if two hostnames or IP addresses are equivalent
 *
 * Copyright 1997 by Eric S. Raymond
 * For license terms, see the file COPYING in this directory.
 */
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_NET_SOCKET_H
#include <net/socket.h>
#else
#include <sys/socket.h>
#endif
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <netdb.h>
#include "i18n.h"
#include "mx.h"
#include "fetchmail.h"
#include "getaddrinfo.h"

#define MX_RETRIES	3

typedef unsigned char address_t[sizeof (struct in_addr)];

#ifdef HAVE_RES_SEARCH
static int getaddresses(struct addrinfo **result, const char *name)
{
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype=SOCK_STREAM;
    hints.ai_protocol=PF_UNSPEC;
    hints.ai_family=AF_UNSPEC;
    return fm_getaddrinfo(name, NULL, &hints, result);
}

/* XXX FIXME: doesn't detect if an IPv6-mapped IPv4 address
 * matches a real IPv4 address */
static int compareaddr(const struct addrinfo *a1, const struct addrinfo *a2)
{
    if (a1->ai_family != a2->ai_family) return FALSE;
    if (a1->ai_addrlen != a2->ai_addrlen) return FALSE;
    return (!memcmp(a1->ai_addr, a2->ai_addr, a1->ai_addrlen));
}

static int is_ip_alias(const char *name1,const char *name2)
/*
 * Given two hostnames as arguments, returns TRUE if they
 * have at least one IP address in common.
 * No check is done on errors returned by gethostbyname,
 * the calling function does them.
 */
{
    int rc = FALSE;

    struct addrinfo *res1 = NULL, *res2 = NULL, *ii, *ij;

    if (getaddresses(&res1, name1))
	goto found;

    if (getaddresses(&res2, name2))
	goto found;

    for (ii = res1 ; ii ; ii = ii -> ai_next) {
	for (ij = res2 ; ij ; ij = ij -> ai_next) {
	    if (compareaddr(ii, ij)) {
		rc = TRUE;
		goto found;
	    }
	}
    }

found:
    if (res2)
	fm_freeaddrinfo(res2);
    if (res1)
	fm_freeaddrinfo(res1);
    return rc;
}
#endif

int is_host_alias(const char *name, struct query *ctl, struct addrinfo **res)
/* determine whether name is a DNS alias of the mailserver for this query */
{
#ifdef HAVE_RES_SEARCH
    struct mxentry	*mxp, *mxrecords;
    int			e;
    struct addrinfo	hints, *res_st;
#endif
    struct idlist	*idl;
    size_t		namelen;

    struct hostdata *lead_server =
	ctl->server.lead_server ? ctl->server.lead_server : &ctl->server;

    /*
     * The first two checks are optimizations that will catch a good
     * many cases.
     *
     * (1) check against the `true name' deduced from the poll label
     * and the via option (if present) at the beginning of the poll cycle.
     * Odds are good this will either be the mailserver's FQDN or a suffix of
     * it with the mailserver's domain's default host name omitted.
     *
     * (2) Then check the rest of the `also known as'
     * cache accumulated by previous DNS checks.  This cache is primed
     * by the aka list option.
     *
     * Any of these on a mail address is definitive.  Only if the
     * name doesn't match any is it time to call the bind library.
     * If this happens odds are good we're looking at an MX name.
     */
    if (strcasecmp(lead_server->truename, name) == 0)
	return(TRUE);
    else if (str_in_list(&lead_server->akalist, name, TRUE))
	return(TRUE);

    /*
     * Now check for a suffix match on the akalist.  The theory here is
     * that if the user says `aka netaxs.com', we actually want to match
     * foo.netaxs.com and bar.netaxs.com.
     */
    namelen = strlen(name);
    for (idl = lead_server->akalist; idl; idl = idl->next)
    {
	const char	*ep;

	/*
	 * Test is >= here because str_in_list() should have caught the
	 * equal-length case above.  Doing it this way guarantees that
	 * ep[-1] is a valid reference.
	 */
	if (strlen(idl->id) >= namelen)
	    continue;
	ep = name + (namelen - strlen(idl->id));
	/* a suffix led by . must match */
	if (ep[-1] == '.' && !strcasecmp(ep, idl->id))
	    return(TRUE);
    }

    if (!ctl->server.dns)
	return(FALSE);
#ifndef HAVE_RES_SEARCH
    (void)res;
    return(FALSE);
#else
    /*
     * The only code that calls the BIND library is here and in the
     * start-of-run probe with gethostbyname(3) under ETRN/Kerberos.
     *
     * We know DNS service was up at the beginning of the run.
     * If it's down, our nameserver has crashed.  We don't want to try
     * delivering the current message or anything else from the
     * current server until it's back up.
     */
    memset(&hints, 0, sizeof hints);
    hints.ai_family=AF_UNSPEC;
    hints.ai_protocol=PF_UNSPEC;
    hints.ai_socktype=SOCK_STREAM;
    hints.ai_flags=AI_CANONNAME;

    e = fm_getaddrinfo(name, NULL, &hints, res);
    if (e == 0)
    {
	int rr = (strcasecmp(ctl->server.truename, (*res)->ai_canonname) == 0);
	fm_freeaddrinfo(*res); *res = NULL;
	if (rr)
	    goto match;
        else if (ctl->server.checkalias && 0 == fm_getaddrinfo(ctl->server.truename, NULL, &hints, &res_st))
	{
	    fm_freeaddrinfo(res_st);
	    if (outlevel >= O_DEBUG)
		report(stdout, GT_("Checking if %s is really the same node as %s\n"),ctl->server.truename,name);
	    if (is_ip_alias(ctl->server.truename,name) == TRUE)
	    {
		if (outlevel >= O_DEBUG)
		    report(stdout, GT_("Yes, their IP addresses match\n"));
		goto match;
	    }
	    if (outlevel >= O_DEBUG)
		report(stdout, GT_("No, their IP addresses don't match\n"));
	    return(FALSE);
	} else {
	    return(FALSE);
	}
    }
    else
	switch (e)
	{
	    case EAI_NONAME:	/* specified host is unknown */
		break;

	    default:
		if (outlevel != O_SILENT)
		    report_complete(stdout, "\n");	/* terminate the progress message */
		report(stderr,
			GT_("nameserver failure while looking for '%s' during poll of %s: %s\n"),
			name, ctl->server.pollname, gai_strerror(e));
		ctl->errcount++;
		break;
	}

    /*
     * We're only here if DNS was OK but the gethostbyname() failed
     * with a HOST_NOT_FOUND or NO_ADDRESS error.
     * Search for a name match on MX records pointing to the server.
     */
    h_errno = 0;
    if ((mxrecords = getmxrecords(name)) == (struct mxentry *)NULL)
    {
	switch (h_errno)
	{
	case HOST_NOT_FOUND:	/* specified host is unknown */
#ifdef NO_ADDRESS
	case NO_ADDRESS:	/* valid, but does not have an IP address */
	    return(FALSE);
#endif
	case NO_RECOVERY:	/* non-recoverable name server error */
	case TRY_AGAIN:		/* temporary error on authoritative server */
	default:
	    report(stderr,
		GT_("nameserver failure while looking for `%s' during poll of %s.\n"),
		name, ctl->server.pollname);
	    ctl->errcount++;
	    break;
	}
    } else {
	for (mxp = mxrecords; mxp->name; mxp++)
	    if (strcasecmp(ctl->server.truename, mxp->name) == 0
		    || is_ip_alias(ctl->server.truename, mxp->name) == TRUE)
		goto match;
	return(FALSE);
    match:;
    }

    /* add this name to relevant server's `also known as' list */
    save_str(&lead_server->akalist, name, 0);
    return(TRUE);
#endif /* HAVE_RES_SEARCH */
}

/* checkalias.c ends here */
