/*
 * mxget.c -- fetch MX records for given DNS name
 *
 * Copyright 1997 by Eric S. Raymond
 * For license terms, see the file COPYING in this directory.
 */

#include "config.h"
#include <stdio.h>
#ifdef HAVE_RES_SEARCH
#include <string.h>
#ifdef HAVE_NET_SOCKET_H
#include <net/socket.h>
#endif
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>

#ifdef __BEOS__
#include "beos/beos_nameser.h"
#endif

#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif

#include "mx.h"

/*
 * This ought to be in the bind library.  It's adapted from sendmail.
 */

/*
 * These are defined in RFC833. Some bind interface headers don't declare them.
 * Ghod help us if they're ever actually incompatible with what's in 
 * the arpa/nameser.h header.
 */
#ifndef PACKETSZ
#define PACKETSZ	512		/* maximum packet size */
#endif
#ifndef HFIXEDSZ
#define	HFIXEDSZ	12		/* #/bytes of fixed data in header */
#endif
#ifndef INT32SZ
#define	INT32SZ		4		/* for systems without 32-bit ints */
#endif
#ifndef INT16SZ
#define	INT16SZ		2		/* for systems without 16-bit ints */
#endif

/* minimum possible size of MX record in packet */
#define MIN_MX_SIZE	8	/* corresp to "a.com 0" w/ terminating space */

struct mxentry *getmxrecords(const char *name)
/* get MX records for given host */
{
    char answer[PACKETSZ], *eom, *cp, *bp;
    int n, ancount, qdcount, buflen, type, pref, ind;
    static struct mxentry pmx[(PACKETSZ - HFIXEDSZ) / MIN_MX_SIZE];
    static char MXHostBuf[PACKETSZ - HFIXEDSZ]; 
    HEADER *hp;

    pmx->name = (char *)NULL;
    pmx->pref = -1;
    n = res_search(name, C_IN,T_MX, (unsigned char *)&answer, sizeof(answer));
    if (n == -1)
	return((struct mxentry *)NULL);
    if ((size_t)n > sizeof(answer))
	n = sizeof(answer);    	

    hp = (HEADER *)&answer;
    cp = answer + HFIXEDSZ;
    eom = answer + n;
    h_errno = 0;
    for (qdcount = ntohs(hp->qdcount); qdcount--; cp += n + QFIXEDSZ)
      if ((n = dn_skipname((unsigned char *)cp, (unsigned char *)eom)) < 0)
	    return((struct mxentry *)NULL);
    buflen = sizeof(MXHostBuf) - 1;
    bp = MXHostBuf;
    ind = 0;
    ancount = ntohs(hp->ancount);
    while (--ancount >= 0 && cp < eom)
    {
	if ((n = dn_expand((unsigned char *)answer, (unsigned char *)eom,
			   (unsigned char *)cp, bp, buflen)) < 0)
	    break;
	cp += n;
	GETSHORT(type, cp);
	cp += INT16SZ + INT32SZ;
	GETSHORT(n, cp);
	if (type != T_MX)
	{
	    cp += n;
	    continue;
	}
	GETSHORT(pref, cp);
	if ((n = dn_expand((unsigned char *)answer, (unsigned char *)eom,
			   (unsigned char *)cp, bp, buflen)) < 0)
	    break;
	cp += n;

	pmx[ind].name = bp;
	pmx[ind].pref = pref;
	++ind;

	n = strlen((const char *)bp);
	bp += n;
	*bp++ = '\0';

	buflen -= n + 1;
    }

    pmx[ind].name = (char *)NULL;
    pmx[ind].pref = -1;
    return(pmx);
}
#endif /* HAVE_RES_SEARCH */

#ifdef STANDALONE
#include <stdlib.h>

int main(int argc, char *argv[])
{
    struct mxentry *responses;

    if (argc != 2 || 0 == strcmp(argv[1], "-h")) {
	fprintf(stderr, "Usage: %s domain\n", argv[0]);
	exit(1);
    }

#ifdef HAVE_RES_SEARCH
    responses = getmxrecords(argv[1]);
    if (responses == (struct mxentry *)NULL) {
	puts("No MX records found");
    } else {
	do {
	    printf("%s %d\n", responses->name, responses->pref);
	} while ((++responses)->name);
    }
#else
    puts("This program was compiled without HAS_RES_SEARCH and does nothing.");
#endif

    return 0;
}
#endif /* TESTMAIN */

/* mxget.c ends here */
