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

/* at_cho_prn.c: 2.0, 1.9; 8/1/90; Copyright 1988-89, Apple Computer, Inc. */


/*+
  |  Description:
  |      At_cho_prn is an AppleTalk utility for choosing a laserwriter.
  |
  |  Copyright:
  |      Copyright 1985 by UniSoft Systems Corporation.
  |
  |  History:
  |      25-May-87: Created by Karin Verhaest.
  |      12-Feb-88: Modified to support other printers (ie ImageWriter)
  |					and zones properly. Wm. Kanawyer
  |
  |      17-May-88: alm: Modified to store users' choice of printers away in
  |                 a file. Also only lookup printers, not all nve's. Removed
  |                 -f option processing.
  |
  |  	 25-May-88: alm: Use tabs as separators in file which stores printer
  |                      choices.The -s was added for smit.
  |
  |	 19-Sept-88: alm: Print out zone list.

	12-12-95 jjs	added support  for -l, -c & -s
	
	
	usage: at_cho_prn [-l] (list local zones only)
				  [-c] (list printers in current zone)
				  [-s printer]  (for explicitly setting a fully
						 qualified printer name). 
				  [no params] (list all zones)
  |
  +*/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <curses.h>
#include <sys/types.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>

#include <netat/appletalk.h>
#include <netat/atp.h>

#include <AppleTalk/at_proto.h>

/*#define MSGSTR(num,str)		catgets(catd, MS_ATBIN, num,str)*/
#define MSGSTR(num,str)			str

#define NUM_APPLE_PRINTERS 2
#define LASERWRITER	"LaserWriter"
#define IMAGEWRITER	"ImageWriter"
#define	MAX_PRINTERS	50
#define LOOKUP_ALL	0		/* perform lookup on all zones */
#define LOOKUP_LOCAL	1		/* perform lookup on local zones */
#define LOOKUP_CURRENT	2		/* perform lookup on current zone */	

char	*progname;
int 		fixedWidth = 2;
int 		colWidth;
int 		entryWidth = 1;
char		tempFile1[50];
char		tempFile2[50];
jmp_buf	jmpbuf;
extern int	zonecmp();

static void	sighandler(int sig);
static int	at_cho_zone(char *zoneChoice, int lookupType);
static int	choose_printer(at_entity_t entity[], int numPrinters,
			at_retry_t *retry);

main(argc,argv)
int     argc;
char   *argv[];
{
   	int		i;
	char		*type, *zone;
	int		z_option = 0;
	at_retry_t	retry;
	int		numPrinters;
	at_entity_t	entity[NUM_APPLE_PRINTERS];
/* XXX 	FILE		*fopen(); */
	char 	buffer[150];
	char zoneChoice[50];
	int lookupType=LOOKUP_ALL;

        {
                int error = 0;
                error = checkATStack();
/*
                printf ("returned from checkATStack with error = %d\n", error);
*/
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

	signal(SIGINT, sighandler);
	if (setjmp(jmpbuf))
		goto abort;
	progname = argv[0];

	if (access_allowed())
		exit(1);

	switch (argc) {
	case 1 :
	case 2 :
		/* user has not provided type@zone string */
		numPrinters = NUM_APPLE_PRINTERS;
		nbp_make_entity(&entity[0], "=", IMAGEWRITER, "*");
		nbp_make_entity(&entity[1], "=", LASERWRITER, "*");
		if (argc == 1)
			break;
		if (!strncmp(argv[1], "-l",2)) {
				lookupType=LOOKUP_LOCAL;
				break;
		}
		if (!strncmp(argv[1], "-c",2)) {
				lookupType=LOOKUP_CURRENT;
				break;
		}
		numPrinters = 1;
		type = argv[1];
		if (zone = strchr(argv[1], '@')) {
			*zone++ = '\0';
			z_option++;
		} else 
			zone = "*";
		if (nbp_make_entity(&entity[0], "=", type, zone) != 0) {
			perror(MSGSTR(M_PARSING, "parsing nbp name"));
			goto abort;
		}
		break;
	case 3:
		if (!strncmp(argv[1], "-s",2)) {
			if (nbp_parse_entity(&entity[0], argv[2])) {
				printf("error parsing printer name\n");
				exit(1);
			}
		}
			/* check printer status before setting it as default */
		sprintf(buffer,"/usr/bin/atstatus \"%s\"",argv[2]);
		i = system(buffer);
		if (i) {
			fprintf(stderr,"Default printer not changed\n");
			goto abort;
		}
		if (i = set_chooserPrinter(progname, &entity[0]))
			goto abort;
		goto exitok;

	default :
		fprintf (stderr, MSGSTR(M_USAGE2, 
			"usage: %s [type[@zone]]\n"), argv[0]);
		goto abort;
	}
	
	 if (!z_option) {
		int status=0;
		at_nvestr_t     zone;

	 	if (lookupType == LOOKUP_ALL || lookupType == LOOKUP_LOCAL) {
			status = at_cho_zone(zoneChoice, lookupType); 
		}
		if (status == -1) {
			if (errno == ENETUNREACH)
				fprintf(stderr,MSGSTR(M_NET_UNREACH,
					"Network is unreachable, perhaps the Appletalk stack is not running\n"));
			goto abort;
		} else if (status != 0) {
			for (i=0; i<numPrinters; i++) {
				strcpy((char *) entity[i].zone.str, zoneChoice);
				entity[i].zone.len = strlen(zoneChoice);
			}
		}
		else {
				/* current zone, get name */
			if (zip_getmyzone(ZIP_DEF_INTERFACE, &zone)) {
				perror(argv[0]);
				exit(1);
			}
			strncpy(zoneChoice,zone.str, zone.len);
		}
		printf(MSGSTR(M_ZONE,"Zone:%s\n"), zoneChoice);
	}

	/*
   	 * call choose_printer for prompting the user for a choice 
	 */
	retry.retries = 2;
	retry.interval = 1;

	if (choose_printer(entity, numPrinters, &retry) != 0) {
abort:
		unlink(tempFile1);
		unlink(tempFile2);
		exit(1);
	}
	else {
		unlink(tempFile1);
		unlink(tempFile2);
exitok:
		exit(0);
	}
}

static int
at_cho_zone(zoneChoice, lookupType)
	char *zoneChoice;
	int	lookupType;
{
	int		item;
	at_nvestr_t     *zonep;
	static u_char	zonebuf[ATP_DATA_SIZE+1];
	int             i, j, n;
	char            save;
	int		bufcnt = 0, entryCount=0;
	char		buffer[128];
	FILE		*pF;

	tmpnam(tempFile1);
	tmpnam(tempFile2);
	if (!(pF=fopen(tempFile1, "w+"))) { 
		sprintf(buffer,MSGSTR(M_ERR_TMP,
			"Error opening temporary file (%s) in %s:\n"), tempFile1,P_tmpdir);
		fprintf(stderr,buffer);
		perror(progname);
		goto abort;
	}
	i = ZIP_FIRST_ZONE;
	do {
		n = (lookupType == LOOKUP_ALL)?
			zip_getzonelist(ZIP_DEF_INTERFACE, &i, 
					zonebuf, sizeof(zonebuf)) :
			zip_getlocalzones(ZIP_DEF_INTERFACE, &i, 
					  zonebuf, sizeof(zonebuf));
		if (n == -1) {
			switch (errno) {
			case ENETUNREACH:
				goto abort;
				return(-1);
			}
			if (bufcnt == 0) {
			 	return(0);
			}
			else
				break;
		}
		for (j = 0, zonep =(at_nvestr_t *)(&(zonebuf[0]));
		     j<n; j++) {
			save = zonep->str[zonep->len];	
			zonep->str[zonep->len] = '\0';
			if (fprintf(pF,"%s\n", zonep->str) <= 0 ) {
				sprintf (buffer,
					MSGSTR(M_ERR_ZLIST,
					"Error writing zone list to temporary file (%s)\n"), 
						tempFile1);
				fprintf(stderr,buffer);
				perror(progname);
				goto abort;
			}
			zonep->str[zonep->len] = save;
			zonep = (at_nvestr_t *) (((char *)zonep) + zonep->len + 1);
		}
		bufcnt++;
		entryCount += n;

	} while (n != -1 && i != ZIP_NO_MORE_ZONES);
	fclose(pF);
/*#### LD 07/22/97  Rhapsody problem with sort */
	/*sprintf(buffer, "sort %s > %s",tempFile1, tempFile2);*/
	sprintf(buffer, "sort -o %s %s",tempFile2, tempFile1);
	if (system(buffer)) {
		sprintf(buffer, MSGSTR(M_ERR_SORT, "Error sorting zone list in %s\n"\
			"(Perhaps there is not enough free space)\n"),
			P_tmpdir);
		fprintf(stderr, buffer);
		goto abort;
	}
	if (!(pF = fopen(tempFile2, "r"))) {
		sprintf(buffer,MSGSTR(M_ERR_OPEN_SORT, 
			"Error opening sorted temporary file (%s)\n"),
			tempFile2);
		fprintf(stderr,buffer);
		goto abort;
	}
	i=1;
	while(!feof(pF)) {
		if (!(fgets(buffer,sizeof(buffer), pF)))
			break;
		buffer[strlen(buffer)-1] = '\0';
		printf("%-4d %-33s ", i, buffer);
		if (!(i%2))
			printf("\n");
		i++;
	}
	printf("%s\n", !(i%2) ? "\n" : "");

	if (entryCount  == 0)
		return(0);
	while (1) {
		fprintf(stdout,MSGSTR(M_ZONE_NO, 
			"\nZONE number (0 for current zone)? "));
		fflush(stdout);
		if (fgets(buffer,sizeof(buffer),stdin) != buffer) 
			return(-1);
	
		if (buffer[0] == '\0')
			continue;
		if (!isdigit(buffer[strspn(buffer, " \t")]))
			return(-1);
	
		item = atoi(buffer);
		if (item == 0)
			return(0);
		if (!(1 <= item  &&  item <= entryCount))
			continue;
		rewind(pF);
		i=1;
		while(!feof(pF)) {
			if (!(fgets(buffer,sizeof(buffer), pF)))
				break;
			if (i == item)
				break;
			i++;
		}
		break;
	}

	
	buffer[strlen(buffer)-1] = '\0'; 	/* remove trailing '\n' */
	strcpy(zoneChoice, buffer); 

	return(item);
abort:
	
	fclose(pF);
	return(-1);
}

int	choose_printer(entity, numPrinters, retry)
at_entity_t	entity[];
int		numPrinters;
at_retry_t	*retry;
{
	at_nbptuple_t	buf[NUM_APPLE_PRINTERS][MAX_PRINTERS];
	int		got[NUM_APPLE_PRINTERS];
	int		i;
	int		item_nums[NUM_APPLE_PRINTERS];
	char		input[10];
	int		item;
	at_entity_t	choice_ent;
	int		found = 0;

	fprintf(stderr, MSGSTR(M_LOOKUP_PRINT, "Looking up printers...\r"));
	fflush(stdout);
	for (i=0; i < numPrinters; i++) {
		got[i] = nbp_lookup(&entity[i],&buf[i][0],MAX_PRINTERS,retry);
		if(i == 0)
			item_nums[0] = 1;
		else
			item_nums[i] = item_nums[i-1] + got[i-1];
		
		if (got[i]>0) {
			found++;
			print_tuples(&buf[i][0],got[i],!i,0,0,0,item_nums[i],0);
			/* headers the first time around, no decimal, quote
			 * always, no print zone
			 */
		}
	}
	
	if (!found) {
		fprintf(stdout, MSGSTR(M_NO_PTR_FOUND,
			"No printers found in zone %s\n"), entity[0].zone.str);
		return (0);
	}

	fprintf(stdout, MSGSTR(M_ITEM_NO,
		"\nITEM number (0 to make no selection)?"));
	fflush(stdout);
	if (fgets(input, sizeof(input), stdin) != input) {
		errno = EIO;
		return(-1);
	}
	if (input[0] == '\0')
		return (0);
	item = atoi(input);
	if (item == 0)
		return(0);
	if(!(1 <= item && 
		item <= (item_nums[numPrinters-1]+got[numPrinters-1] - 1))) {
		fprintf(stdout, MSGSTR(M_INVAL_NO,
			"Invalid ITEM number; no selection made\n"));
		return(-1);
	}
	
	i = 0;
	for (i=0; item>got[i]; item-=got[i], i++)
		;
	
	choice_ent = buf[i][item-1].enu_entity;
	/* now, smash the zone name part, since the tuple returned by 
	 * nbp_lookup doesn't reflect reality
	 */
	choice_ent.zone = entity[0].zone;
	if (i = set_chooserPrinter(progname, &choice_ent))
		return(i);
	i = system("/usr/bin/atstatus");
	return(i);
}

static void
sighandler(int sig)
{
	printf("                       \r"); 	/* clear "looking up printer" msg */
	longjmp(jmpbuf, sig);
}
