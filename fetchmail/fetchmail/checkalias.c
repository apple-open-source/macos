/*
 * checkalias.c -- check to see if two hostnames or IP addresses are equivalent
 *
 * Copyright 1997 by Eric S. Raymond
 * For license terms, see the file COPYING in this directory.
 */
#include "config.h"
#ifdef HAVE_GETHOSTBYNAME
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

#define MX_RETRIES	3

static int is_ip_alias(const char *name1,const char *name2)
/*
 * Given two hostnames as arguments, returns TRUE if they
 * have at least one IP address in common.
 * No check is done on errors returned by gethostbyname,
 * the calling function does them.
 */
{
    typedef unsigned char address_t[sizeof (struct in_addr)]; 
    typedef struct _address_e
    {
	struct _address_e *next;
	address_t address;
    } 
    address_e;
    address_e *host_a_addr=0, *host_b_addr=0;	/* assignments pacify -Wall */
    address_e *dummy_addr;

    int i;
    struct hostent *hp;
    char **p;
 
    hp = gethostbyname((char*)name1);
 
    dummy_addr = (address_e *)NULL;

    for (i=0,p = hp->h_addr_list; *p != 0; i++,p++)
    {
	struct in_addr in;
	(void) memcpy(&in.s_addr, *p, sizeof (in.s_addr));
	xalloca(host_a_addr, address_e *, sizeof (address_e));
	memset (host_a_addr,0, sizeof (address_e));
	host_a_addr->next = dummy_addr;
	(void) memcpy(&host_a_addr->address, *p, sizeof (in.s_addr));
	dummy_addr = host_a_addr;
    }

    hp = gethostbyname((char*)name2);

    dummy_addr = (address_e *)NULL;
    for (i=0,p = hp->h_addr_list; *p != 0; i++,p++)
    {
	struct in_addr in;
	(void) memcpy(&in.s_addr, *p, sizeof (in.s_addr));
	xalloca(host_b_addr, address_e *, sizeof (address_e));
	memset (host_b_addr,0, sizeof (address_e));
	host_b_addr->next = dummy_addr;
	(void) memcpy(&host_b_addr->address, *p, sizeof (in.s_addr));
	dummy_addr = host_b_addr;
    }

    while (host_a_addr)
    {
	while (host_b_addr)
	{
	    if (!memcmp(host_b_addr->address,host_a_addr->address, sizeof (address_t)))
		return (TRUE);

	    host_b_addr = host_b_addr->next;
	}
	host_a_addr = host_a_addr->next;
    }
    return (FALSE);
}

int is_host_alias(const char *name, struct query *ctl)
/* determine whether name is a DNS alias of the mailserver for this query */
{
    struct hostent	*he,*he_st;
    struct mxentry	*mxp, *mxrecords;
    struct idlist	*idl;
    int			namelen;

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
	char	*ep;

	/*
	 * Test is >= here because str_in_list() should have caught the
	 * equal-length case above.  Doing it this way guarantees that
	 * ep[-1] is a valid reference.
	 */
	if (strlen(idl->id) >= namelen)
	    continue;
	ep = (char *)name + (namelen - strlen(idl->id));
	/* a suffix led by . must match */
	if (ep[-1] == '.' && !strcasecmp(ep, idl->id))
	    return(TRUE);
    }

    if (!ctl->server.dns)
	return(FALSE);
#ifndef HAVE_RES_SEARCH
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
    if ((he = gethostbyname((char*)name)) != (struct hostent *)NULL)
    {
	if (strcasecmp(ctl->server.truename, he->h_name) == 0)
	    goto match;
        else if (((he_st = gethostbyname(ctl->server.truename)) != (struct hostent *)NULL) && ctl->server.checkalias)
	{
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
	}
	else
	    return(FALSE);
    }
    else
	switch (h_errno)
	{
	case HOST_NOT_FOUND:	/* specified host is unknown */
#ifndef __BEOS__
	case NO_ADDRESS:	/* valid, but does not have an IP address */
	    break;
#endif
	case NO_RECOVERY:	/* non-recoverable name server error */
	case TRY_AGAIN:		/* temporary error on authoritative server */
	default:
	    if (outlevel != O_SILENT)
		report_complete(stdout, "\n");	/* terminate the progress message */
	    report(stderr,
		GT_("nameserver failure while looking for `%s' during poll of %s.\n"),
		name, ctl->server.pollname);
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
#ifndef __BEOS__
	case NO_ADDRESS:	/* valid, but does not have an IP address */
	    return(FALSE);
	    break;
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
    }
    else
    {
	for (mxp = mxrecords; mxp->name; mxp++)
	    if (strcasecmp(ctl->server.truename, mxp->name) == 0)
		goto match;
	return(FALSE);
    match:;
    }

    /* add this name to relevant server's `also known as' list */
    save_str(&lead_server->akalist, name, 0);
    return(TRUE);
#endif /* HAVE_RES_SEARCH */
}
#endif /* HAVE_GETHOSTBYNAME */

/* checkalias.c ends here */
