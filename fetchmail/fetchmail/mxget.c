/*
 * mxget.c -- fetch MX records for given DNS name
 *
 * Copyright (C) 1996, 1997, 1998, 2000, 2002 by Eric S. Raymond
 * Copyright (C) 2005, 2006, 2007 by Matthias Andree
 * For license terms, see the file COPYING in this directory.
 *
 * 28 July 2006
 * Converted to use dns API for MacOS X - majka
 */


#include "config.h"
#include "fetchmail.h"
#include <stdio.h>
#include <string.h>
#ifdef HAVE_RES_SEARCH
#ifdef HAVE_NET_SOCKET_H
#include <net/socket.h>
#endif
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <resolv.h>
#include <nameser.h>
#include <dns.h>
#include <dns_util.h>


#include "mx.h"

#define MX_MAX 255

/* get MX records for given host */
struct mxentry *
getmxrecords(const char *name)
{
    static struct mxentry pmx[MX_MAX + 1];
	int i, j;
	dns_handle_t dns;
	dns_reply_t *reply;

	dns = dns_open(NULL);
	if (dns == NULL) return NULL;
	
	reply = dns_lookup(dns, name, ns_c_in, ns_t_mx);
	dns_free(dns);
	
	if (reply == NULL) return NULL;
	
	j = 0;
	for (i = 0; (i < reply->header->ancount) && (j < MX_MAX); i++)
	{
		if (reply->answer[i]->dnstype != ns_t_mx) continue;
		pmx[j].pref = reply->answer[i]->data.MX->preference;
		pmx[j].name = strdup(reply->answer[i]->data.MX->name);
		j++;
	}
	
	if (j == 0) return NULL;
	
	pmx[i].name = NULL;
	pmx[i].pref = -1;
	
	return(pmx);
}
#endif /* HAVE_RES_SEARCH */

#ifdef STANDALONE
#include <stdlib.h>

int main(int argc, char *argv[])
{
#ifdef HAVE_RES_SEARCH
    struct mxentry *responses;
#endif
    int i;

    if (argc != 2 || 0 == strcmp(argv[1], "-h")) {
	fprintf(stderr, "Usage: %s domain\n", argv[0]);
	exit(1);
    }

#ifdef HAVE_RES_SEARCH
    responses = getmxrecords(argv[1]);
    if (responses == (struct mxentry *)NULL)
    {
	    puts("No MX records found");
    }
    else
    {
	    for (i = 0; responses[i].name != NULL; i++)
	    {
		    printf("%s %d\n", responses[i].name, responses[i].pref);
	    }
    }
#else
    puts("This program was compiled without HAS_RES_SEARCH and does nothing.");
#endif

    return 0;
}
#endif /* TESTMAIN */

/* mxget.c ends here */
