/*
 *	Copyright (c) 1988, 1989, 1998 Apple Computer, Inc. 
 *
 *	The information contained herein is subject to change without
 *	notice and  should not be  construed as a commitment by Apple
 *	Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *	for any errors that may appear.
 *
 *	Confidential and Proprietary to Apple Computer, Inc.
 */

/* atlookup.c: 2.0, 1.16; 8/1/90; Copyright 1988-89, Apple Computer, Inc. */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <curses.h>
#include <sys/types.h>
#include <dirent.h>

#include <netat/appletalk.h>
#include <netat/zip.h>
#include <netat/atp.h>
#include <netat/nbp.h>

#include <AppleTalk/at_proto.h>

/*#define MSGSTR(num,str)		catgets(catd, MS_ATBIN, num,str)*/
#define MSGSTR(num,str)		str

#define	NTUPLES	1500		/* We can handle up to 2000 names */
#define ATP_BUFFS 30

/***************************************************************

11-28-95 jjs quick & dirty fix to expand the number of zones, I
		increased the ATP_BUFFS from 10 to 30 (the NTUPLES
		was increased from 500 to 1500 on the last round, but
		didn't fully fix the problem). This code needs to 
		be fixed in a way to eliminate any limitation on the
		number of zones & or entitites. I didn't do it on this
		go around due to time constraints and the fact that
		few, if any, of our customers will even come close to
		this limit. See at_cho_prn for preferred method.

******************************************************************/

at_nbptuple_t	buf[NTUPLES];
char strbuf[NBP_NVE_STR_SIZE+1];

extern char	*quote();

static int	tuplecmp(at_nbptuple_t *t1, at_nbptuple_t *t2);
static int	error(char *progname, char *s1, char *s2);
static int	zonecmp(at_nvestr_t *t1, at_nvestr_t *t2);
static void	printEntry(at_nvestr_t *ap);
static void	printzones(int Coption);

int 		fixedWidth = 2;
int 		colWidth;
int 		entryWidth = 1;
at_nvestr_t 	zoneEntries[NTUPLES];
int		entryCount;
static char     *usage = "\
usage: %s [-d] [-r nn] [-s nn] [-x] [object:[type[@zone]]] \n\
	or \n\
       %s -z [-C] \n\
        -a            network addresses not displayed \n\
        -d            display network addresses in decimal \n\
        -r nn         retry lookup nn times \n\
        -s nn         retry every nn seconds \n\
        -z            display zones on the network \n\
        -C            display zones in multiple columns \n\
        -x            quote non-printable characters as \\XX \n\
";



main(argc, argv)
	int		argc;
	char		**argv;
{
	at_entity_t	entity;
	at_retry_t	retry;
	int		got;
	char		*p;
	char		*nve = "=:=@*";
	extern int	optind;
	extern char	*optarg;
	int		opterr = 0;
	int		aoption = 0;
	int		doption = 0;
	int		xoption = 0;
	int		zoption = 0;
	int		Coption = 0;
	char		*progname = *argv;
	int		ch;
	int		max;
	at_nvestr_t     this_zone, *zonep;
	int             i, j, n;
	u_char          zonebuf[ATP_BUFFS][ATP_DATA_SIZE+1];
	int		bufcnt;

		/* check to see if atalk is loaded */
	{
		int error = 0;
		error = checkATStack();
		/*printf ("returned from checkATStack with error = %d\n", error);*/
		switch (error) {
			case NOTLOADED:
				fprintf(stderr, MSGSTR(M_NOT_LOADED,
                                "The AppleTalk stack is not Loaded\n"));
				break;
 			case LOADED:
				fprintf(stderr, MSGSTR(M_NOT_LOADED,
                                "The AppleTalk stack is not Running\n"));
				break;

			case RUNNING:
				error =0;
				break;	
			default:
				fprintf(stderr, MSGSTR(M_NOT_LOADED,
                                "Other error with The AppleTalk stack\n"));
		}
		if (error != 0)
			exit(1);
	}
	/* Set up defaults */
	retry.interval = NBP_RETRY_INTERVAL;
	retry.retries = NBP_RETRY_COUNT;
	retry.backoff = 1;
	
        /* process the arguments */
	while (!opterr && (ch = getopt(argc, argv, "?Cadr:s:xz")) != EOF) {
            switch (ch) {
                case 'a': 	aoption++; break;
                case 'd': 	doption++; break;
                case 'C': 	Coption++; break;
                case 'z': 	zoption++; break;
                case 'x': 	xoption++; break;
		case 'r':	retry.retries = atoi(optarg);
				if (retry.retries < 1 || retry.retries > 30) {
				    fprintf(stderr, MSGSTR(M_INVAL_RETRY,
						"%s: invalid retry %d\n"),
				    		progname, retry.retries);
				    exit(1);
				}
				break;
		case 's':	retry.interval = atoi(optarg);
				if (retry.interval < 1 || retry.interval > 30) {
				    fprintf(stderr, MSGSTR(M_INVAL_TIMEOUT,
						"%s: invalid timeout %d\n"),
				    		progname, retry.interval);
				    exit(1);
				}
				break;
		case '?': 	opterr++; break;
    		default: 	opterr++; break;
    	    }
	}

	if (opterr) {
            	(void) fprintf(stderr, usage, progname, progname);
            	exit(1);
	}

	if (optind != argc) {
		if ((optind + 1) != argc) {
            		(void) fprintf(stderr, usage, progname, progname);
            		exit(1);
		}
		nve = argv[optind];
	}

	if (zoption) {
	    bufcnt = 0;
	    entryCount = 0;
	    i = ZIP_FIRST_ZONE;
	    while ((bufcnt<ATP_BUFFS) && (entryCount < NTUPLES) && 
		   i != ZIP_NO_MORE_ZONES) {
	        n = zip_getzonelist(ZIP_DEF_INTERFACE, &i,
				       &zonebuf[bufcnt][0], 
				       sizeof(zonebuf[bufcnt]));
		if (n == -1) {
		    if (bufcnt == 0) {
			exit (1);
		    }
		    else
			break;
		}
		for (j = 0, zonep =(at_nvestr_t *)(&(zonebuf[bufcnt][0]));
		     j<n; j++) {
		    strncpy ((char *) zoneEntries [entryCount].str, 
				(char *) zonep->str, zonep->len);
		    zoneEntries [entryCount++].len = zonep->len;
		    if (entryWidth < (int) zonep -> len)
				entryWidth = zonep -> len;
		    zonep = (at_nvestr_t *) (((char *)zonep) + zonep->len + 1);
		}
		bufcnt++;
	    }
	    colWidth = fixedWidth + entryWidth;
	    qsort (zoneEntries, entryCount, sizeof (at_nvestr_t), zonecmp);
	    printzones (Coption);
	    exit (0);
	}

	if (nbp_parse_entity(&entity, nve) < 0) 
		error(progname, MSGSTR(M_INVAL_ENTITY,"invalid entity "), nve);

	max = nbp_iswild(&entity) ? NTUPLES : 1;

	/* If zonename is "*", substitute it with the real zone name if
	 * it's available.
	 */
	if (entity.zone.len == 1 && entity.zone.str[0] == '*') {
		/* looking for the entity in THIS zone....
		 * susbstitute by the real zone name.  If zip_getmyzone()
		 * fails, that's not an error, 'cause it may happen just 
		 * because there's no router around.
		 */
		if (zip_getmyzone(ZIP_DEF_INTERFACE, &this_zone) != -1)
			entity.zone = this_zone;
	}

	if ((got = nbp_lookup(&entity, buf, max, &retry)) < 0) 
		error(progname, MSGSTR(M_CANT_FIND, "cannot find entity"), "");

	if (aoption)
		printf(MSGSTR(M_FOUND_1, "#  Found %d entries in zone "), got);
	else
		printf(MSGSTR(M_FOUND, "Found %d entries in zone "), got);
	fflush(stdout);
	p = quote(entity.zone.str, &entity.zone.len, xoption);
	write(1, p, entity.zone.len);
	write(1, "\n", 1);

	qsort(buf, got, sizeof(at_nbptuple_t), tuplecmp);

	print_tuples(buf, got, 0, doption, xoption?1:0, 0, -1, aoption);
	/* no header line, decimal per user option, always quote, in hex
	 * if user says so, no zone name, no item numbers 
	 */
	exit(0);
	/*NOTREACHED*/
}

static int
tuplecmp(t1, t2)
	at_nbptuple_t	*t1, *t2;
{
	return (strncmp((char *) t1->enu_entity.object.str, (char *) t2->enu_entity.object.str,
		t1->enu_entity.object.len));
}

static int
error(progname, s1, s2)
	char *progname, *s1, *s2;
{
	char errorbuf[256];

	(void) strcpy(errorbuf, progname);
	(void) strcat(errorbuf, ": ");
	(void) strcat(errorbuf, s1);
	(void) strcat(errorbuf, s2);
	perror(errorbuf);
	exit(1);
}

static int
zonecmp(t1, t2)
	at_nvestr_t	*t1, *t2;
{
	int len;
	
	len = (t1->len >= t2->len) ? t1->len : t2->len;
	
	return (strncmp((char *) t1->str, (char *) t2->str, len));
}

static void
printzones (Coption)
	int Coption;
{
	int totalWidth = 80, ncols = 1, nrows, row, col, i;
	at_nvestr_t *ep;
	char *cp;

	if (Coption) {
		if ((cp = getenv("COLUMNS")) != NULL)
			totalWidth = atoi(cp);
		else {
/* we don't have a term.h so do nothing just make totalWidth 0 and it will be fixed later */
			totalWidth = 0;
		}
		if (totalWidth <= 0)
			totalWidth = 80;

		ncols = totalWidth / colWidth;
	}

	if (ncols <= 1 || !Coption) {
	    for (ep = &zoneEntries [0], i = 0; i < entryCount; i++, ep++) {
			strncpy(strbuf,ep->str,ep->len);
			strbuf[ep->len]='\0';
			printf ("%s\n", strbuf);
		}
	    return;
	}

	nrows = (entryCount - 1) / ncols + 1;
	for (row = 0; row < nrows; row++) {
		for (col = 0; col < ncols; col++) {
			ep = &zoneEntries [0] + (nrows * col) + row;
			if (ep < &zoneEntries [entryCount])
				printEntry(ep);
		}
		printf ("\n");
	}
}

static void
printEntry(ap) 
    at_nvestr_t *ap;
{
    char format [80];

    (void) memset (format, 0, 80);
    sprintf (format, "%%-%ds", colWidth);
	strncpy(strbuf,ap->str,ap->len);
	strbuf[ap->len]='\0';
    printf (format, strbuf);
}
