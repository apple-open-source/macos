/*
 * This program is copyright Alec Muffett 1993. The author disclaims all 
 * responsibility or liability with respect to it's usage or its effect 
 * upon hardware or computer systems, and maintains copyright as set out 
 * in the "LICENCE" document which accompanies distributions of Crack v4.0 
 * and upwards.
 */

#include "packer.h"
#include <string.h>
#include <arpa/inet.h>

static char __unused vers_id[] = "packlib.c : v2.3p2 Alec Muffett 18 May 1993";

static void PWDictHeaderToHostOrder (struct pi_header* header)
{
    header->pih_magic = ntohl (header->pih_magic);
    header->pih_numwords = ntohl (header->pih_numwords);
    header->pih_blocklen = ntohs (header->pih_blocklen);
}



static void PWDictHeaderToNetworkOrder (struct pi_header* header)
{
    header->pih_magic = htonl (header->pih_magic);
    header->pih_numwords = htonl (header->pih_numwords);
    header->pih_blocklen = htons (header->pih_blocklen);
}



static void PWHwmsToHostOrder (int32 *array, size_t max)
{
	size_t i;
	for (i = 0; i < max; ++i)
	{
		array[i] = ntohl (array[i]);
	}
}



static void PWHwmnToNetworkOrder (int32 *array, size_t max)
{
	size_t i;
	for (i = 0; i < max; ++i)
	{
		array[i] = htonl (array[i]);
	}
}



extern int PutPW(PWDICT *pwp, char *string);

PWDICT *
PWOpen(prefix, mode)
    char *prefix;
    char *mode;
{
//    int32 i;
    static PWDICT pdesc;
    char iname[STRINGSIZE];
    char dname[STRINGSIZE];
    char wname[STRINGSIZE];
//    char buffer[STRINGSIZE];
    FILE *dfp;
    FILE *ifp;
    FILE *wfp;

    if (pdesc.header.pih_magic == PIH_MAGIC)
    {
	fprintf(stderr, "%s: another dictionary already open\n", prefix);
	return ((PWDICT *) 0);
    }

    memset(&pdesc, '\0', sizeof(pdesc));

    snprintf(iname, sizeof(iname), "%s.pwi", prefix);
    snprintf(dname, sizeof(dname), "%s.pwd", prefix);
    snprintf(wname, sizeof(wname), "%s.hwm", prefix);

    if (!(pdesc.dfp = fopen(dname, mode)))
    {
	perror(dname);
	return ((PWDICT *) 0);
    }

    if (!(pdesc.ifp = fopen(iname, mode)))
    {
	fclose(pdesc.dfp);
	perror(iname);
	return ((PWDICT *) 0);
    }

    if (pdesc.wfp = fopen(wname, mode))
    {
	pdesc.flags |= PFOR_USEHWMS;
    }

    ifp = pdesc.ifp;
    dfp = pdesc.dfp;
    wfp = pdesc.wfp;

    if (mode[0] == 'w')
    {
	pdesc.flags |= PFOR_WRITE;
	pdesc.header.pih_magic = PIH_MAGIC;
	pdesc.header.pih_blocklen = NUMWORDS;
	pdesc.header.pih_numwords = 0;

	// convert to "network" byte order
	PWDictHeaderToNetworkOrder (&pdesc.header);
	fwrite((char *) &pdesc.header, sizeof(pdesc.header), 1, ifp);
	PWDictHeaderToHostOrder (&pdesc.header);
    } else
    {
	pdesc.flags &= ~PFOR_WRITE;

	if (!fread((char *) &pdesc.header, sizeof(pdesc.header), 1, ifp))
	{
	    fprintf(stderr, "%s: error reading header\n", prefix);

	    pdesc.header.pih_magic = 0;
	    fclose(ifp);
	    fclose(dfp);
	    return ((PWDICT *) 0);
	}

	PWDictHeaderToHostOrder (&pdesc.header);

	if (pdesc.header.pih_magic != PIH_MAGIC)
	{
	    fprintf(stderr, "%s: magic mismatch\n", prefix);

	    pdesc.header.pih_magic = 0;
	    fclose(ifp);
	    fclose(dfp);
	    return ((PWDICT *) 0);
	}

	if (pdesc.header.pih_blocklen != NUMWORDS)
	{
	    fprintf(stderr, "%s: size mismatch\n", prefix);

	    pdesc.header.pih_magic = 0;
	    fclose(ifp);
	    fclose(dfp);
	    return ((PWDICT *) 0);
	}

	if (pdesc.flags & PFOR_USEHWMS)
	{
	    if (fread(pdesc.hwms, 1, sizeof(pdesc.hwms), wfp) != sizeof(pdesc.hwms))
	    {
		pdesc.flags &= ~PFOR_USEHWMS;
	    }
		
		PWHwmsToHostOrder (pdesc.hwms, sizeof (pdesc.hwms) / sizeof (int32));
	}
    }

    return (&pdesc);
}

int
PWClose(pwp)
    PWDICT *pwp;
{
	int result;

    if (pwp->header.pih_magic != PIH_MAGIC)
    {
	fprintf(stderr, "PWClose: close magic mismatch\n");
	return (-1);
    }

    if (pwp->flags & PFOR_WRITE)
    {
	pwp->flags |= PFOR_FLUSH;
	PutPW(pwp, (char *) 0);	/* flush last index if necess */

	if (fseek(pwp->ifp, 0L, 0))
	{
	    fprintf(stderr, "index magic fseek failed\n");
	    return (-1);
	}

	PWDictHeaderToNetworkOrder (&pwp->header);
	result = fwrite((char *) &pwp->header, sizeof(pwp->header), 1, pwp->ifp);
	PWDictHeaderToHostOrder (&pwp->header);
	
	if (!result)
	{
	    fprintf(stderr, "index magic fwrite failed\n");
	    return (-1);
	}

	if (pwp->flags & PFOR_USEHWMS)
	{
	    int i;
	    for (i=1; i<=0xff; i++)
	    {
	    	if (!pwp->hwms[i])
	    	{
	    	    pwp->hwms[i] = pwp->hwms[i-1];
	    	}
#ifdef DEBUG
	    	printf("hwm[%02x] = %d\n", i, pwp->hwms[i]);
#endif
	    }
		
		PWHwmnToNetworkOrder (pwp->hwms, sizeof (pwp->hwms) / sizeof (int32));
	    fwrite(pwp->hwms, 1, sizeof(pwp->hwms), pwp->wfp);
		PWHwmsToHostOrder (pwp->hwms, sizeof (pwp->hwms) / sizeof (int32));
	}
    }

    fclose(pwp->ifp);
    fclose(pwp->dfp);

    pwp->header.pih_magic = 0;

    return (0);
}

int
PutPW(pwp, string)
    PWDICT *pwp;
    char *string;
{
    if (!(pwp->flags & PFOR_WRITE))
    {
	return (-1);
    }

    if (string)
    {
	strlcpy(pwp->data[pwp->count], string, MAXWORDLEN);

	pwp->hwms[string[0] & 0xff]= pwp->header.pih_numwords;

	++(pwp->count);
	++(pwp->header.pih_numwords);

    } else if (!(pwp->flags & PFOR_FLUSH))
    {
	return (-1);
    }

    if ((pwp->flags & PFOR_FLUSH) || !(pwp->count % NUMWORDS))
    {
	int i;
	int32 datum;
	register char *ostr;

	datum = htonl ((int32) ftell(pwp->dfp));
	fwrite((char *) &datum, sizeof(datum), 1, pwp->ifp);

	fputs(pwp->data[0], pwp->dfp);
	putc(0, pwp->dfp);

	ostr = pwp->data[0];

	for (i = 1; i < NUMWORDS; i++)
	{
	    register int j;
	    register char *nstr;
	    nstr = pwp->data[i];

	    if (nstr[0])
	    {
		for (j = 0; ostr[j] && nstr[j] && (ostr[j] == nstr[j]); j++);
		putc(j & 0xff, pwp->dfp);
		fputs(nstr + j, pwp->dfp);
	    }
	    putc(0, pwp->dfp);

	    ostr = nstr;
	}

	memset(pwp->data, '\0', sizeof(pwp->data));
	pwp->count = 0;
    }
    return (0);
}

char *
GetPW(pwp, number)
    PWDICT *pwp;
    int32 number;
{
    int32 datum;
    register int i;
    register char *ostr;
    register char *nstr;
    register char *bptr;
    char buffer[NUMWORDS * MAXWORDLEN];
    static char data[NUMWORDS][MAXWORDLEN];
    static int32 prevblock = 0xffffffff;
    int32 thisblock;

    thisblock = number / NUMWORDS;

    if (prevblock == thisblock)
    {
	return (data[number % NUMWORDS]);
    }

    if (fseek(pwp->ifp, sizeof(struct pi_header) + (thisblock * sizeof(int32)), 0))
    {
	perror("(index fseek failed)");
	return ((char *) 0);
    }

    if (!fread((char *) &datum, sizeof(datum), 1, pwp->ifp))
    {
	perror("(index fread failed)");
	return ((char *) 0);
    }

	datum = ntohl (datum);
	
    if (fseek(pwp->dfp, datum, 0))
    {
	perror("(data fseek failed)");
	return ((char *) 0);
    }

    if (!fread(buffer, 1, sizeof(buffer), pwp->dfp))
    {
	perror("(data fread failed)");
	return ((char *) 0);
    }

    prevblock = thisblock;

    bptr = buffer;

    for (ostr = data[0]; *(ostr++) = *(bptr++); /* nothing */ );

    ostr = data[0];

    for (i = 1; i < NUMWORDS; i++)
    {
	nstr = data[i];
	strlcpy(nstr, ostr, MAXWORDLEN);

	ostr = nstr + *(bptr++);
	while (*(ostr++) = *(bptr++));

	ostr = nstr;
    }

    return (data[number % NUMWORDS]);
}

int32
FindPW(pwp, string)
    PWDICT *pwp;
    char *string;
{
    register int32 lwm;
    register int32 hwm;
    register int32 middle;
    register char *this;
    int idx;

    if (pwp->flags & PFOR_USEHWMS)
    {
	idx = string[0] & 0xff;
    	lwm = idx ? pwp->hwms[idx - 1] : 0;
    	hwm = pwp->hwms[idx];
    } else
    {
    	lwm = 0;
    	hwm = PW_WORDS(pwp) - 1;
    }

#ifdef DEBUG
    printf("---- %lu, %lu ----\n", lwm, hwm);
#endif

    for (;;)
    {
	int cmp;

#ifdef DEBUG
	printf("%lu, %lu\n", lwm, hwm);
#endif

	middle = lwm + ((hwm - lwm + 1) / 2);

	if (middle == hwm)
	{
	    break;
	}

	this = GetPW(pwp, middle);
	cmp = strcmp(string, this);		/* INLINE ? */

	if (cmp < 0)
	{
	    hwm = middle;
	} else if (cmp > 0)
	{
	    lwm = middle;
	} else
	{
	    return (middle);
	}
    }

    return (PW_WORDS(pwp));
}
