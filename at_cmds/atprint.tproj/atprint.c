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

/* atprint.c: 2.0, 1.14; 9/15/92; Copyright 1988-89, Apple Computer, Inc. */

/*
 *	atprint - pipe stdin to PAP
 *	usage: 
 *		atprint		destination is Mac Chooser printer
 *		atprint x	destination is x:LaserWriter@*
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <mach/boolean.h>

#include <netat/appletalk.h>
#include <netat/pap.h>
#include <netat/atp.h>

#include <AppleTalk/at_proto.h>

/*#define MSGSTR(num,str)		catgets(catd, MS_ATBIN, num,str)*/
#define MSGSTR(num,str)		str

char *usage = "usage: %s [object[:type[@zone]]]\n";
#define DEFAULT_TYPE	"LaserWriter"

#if _AIX || sparc
/* Fast kernel ethernet drivers tend to overwhelm routers/printers */
/* Hence a smaller buffer is used */
#define	PRINTBUFSZ	512
#endif

/*this is copied from the AIX or sparc definition*/
#define PRINTBUFSZ      512

static int	isLaser(at_entity_t *entity);

main(argc, argv)
int	argc;
char	**argv;
{
	register int	fd = -1;
	register int	i, j;
	register int	c;
	char		buff[PRINTBUFSZ];
	char	 	*progname = *argv;
	static at_entity_t	entity;
	char		*pap_status, *pap_status_get();
	char		*namestr = NULL;
	char		*ch;
	extern	char	*get_chooserPrinter();
	static at_nbptuple_t	tuple;
	int		first;

        {
                int error = 0;
                error = checkATStack();
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

	switch (argc) {
	case 1 :
		/* read the system chooser file for printer choice */
		if ((namestr = get_chooserPrinter(argv[0])) == NULL)
			exit(1);
		if (nbp_parse_entity(&entity, namestr) < 0) {
			perror(MSGSTR(M_PARSING,"parsing nbp name"));
			exit(1);
		}
		break;
	case 2 :
		namestr = argv[1];
		if (nbp_parse_entity(&entity, namestr) < 0) {
			perror(MSGSTR(M_PARSING,"parsing nbp name"));
			exit(1);
		}
	
		if ((entity.type.len==1) && (entity.type.str[0]=='=')) {
			if ((ch=strchr(namestr, ':')) && (*(ch++) == '=')) {
				fprintf (stderr,MSGSTR(M_WILD,
					"%s: wildcard not allowed in name string\n"), argv[0]);
				exit(1);
			} else {
				entity.type.len = strlen(DEFAULT_TYPE);
				strcpy((char *) entity.type.str, DEFAULT_TYPE);
			}
		}
	
		if (nbp_iswild (&entity)) {
				fprintf (stderr,MSGSTR(M_WILD,
					"%s: wildcard not allowed in name string\n"), argv[0]);
			exit(1);
		}

		if ((ch = strchr(namestr, ':')) == NULL) {
			/* use did not specify the "type' */
			strcpy((char *) entity.type.str, DEFAULT_TYPE);
			entity.type.len = strlen(DEFAULT_TYPE);
		}
		break;
	default :
		fprintf(stderr, MSGSTR(M_USAGE1, 
			"usage: %s [object[:type[@zone]]]\n"), argv[0]);
		exit(1);
	}
	
	first = 1;	/* some things happening for the first time */
	while(1) {
		/* Sit in an infinite loop and try to connect to the printer.
		 * Modus operandi is thus : look for the printer, try several
		 * times to connect to it.  If can't connect, start the loop
		 * again by looking up the printer (in case a printer 
		 * disappears from the net, we don't want to embarrass
		 * ourselves!)
		 */

		/* find the "entity" printer */
		if (first || isatty(fileno(stderr)))
			fprintf(stderr, MSGSTR(M_LOOKING, "Looking for %s.\n"), namestr);
		if (nbp_lookup(&entity, &tuple, 1, NULL) <= 0) {
			fprintf(stderr, MSGSTR(M_S_NOT_FOUND, 
				"%s: '%s' not found\n"),argv[0],namestr);
			exit(2);
		}

		if (first)
			fprintf(stderr, MSGSTR(M_TRYING,
				"Trying to connect to %s:%s@%s.\n"), 
				tuple.enu_entity.object.str, 
				tuple.enu_entity.type.str, entity.zone.str);
		
		/* Try to connect to the printer at this adress 5 times */
		for (i=0; i<5; i++) {
			if ((fd = pap_open(&tuple)) < 0) {
				pap_status = pap_status_get();
				if (!strcmp(pap_status, PAP_SOCKERR)) {
					fprintf (stderr, MSGSTR(M_SKT_ERR, "Socket Error %d\n"), 
						errno);
					exit(2);
				} else {
					if (first || isatty(fileno(stderr)))
						fprintf (stderr, "%s\n", 
							pap_status);
				}
			} else {
				break;
			}

			first = 0;
			sleep(5);
		}

		/* If the connection has been successfully established, get
		 * out of the infinite loop!
		 */

		if (fd >= 0)
			break;
	}

	if (isLaser(&entity))
		pap_read_ignore(fd);

	fprintf(stderr, MSGSTR(M_PRINTING,
		"%s: printing on %s:%s@%s.\n"), progname,
		tuple.enu_entity.object.str, tuple.enu_entity.type.str, 
		entity.zone.str);
	for (;;) {
		c = 0;
		for (;;) {
			i = read(0, &buff[c], sizeof(buff) - c);
			if (i <= 0)
				break;
			c += i;
			if (c >= sizeof(buff))
				break;
		}
		if (i <= 0) {
			i = pap_write(fd, buff, c, TRUE, FALSE);
			if (i < 0) {
				fprintf(stderr, MSGSTR(M_WRITE_FAIL,
					"%s: write failed "), progname);
				perror("");
				exit(2);
			}
			break;
		}
		if (c) {
			j = pap_write(fd, buff, c, FALSE, FALSE);
			if (j < 0) {
				fprintf(stderr, MSGSTR(M_WRITE_FAIL,
					"%s: write failed "), progname);
				perror("");
				exit(2);
				/*NOTREACHED*/
			}
		}
	}

	pap_close(fd);

	exit(0);
}

static int
isLaser(entity)
at_entity_t	*entity;
{
	int	i;
	char	type[11];

  	/* Length of "LaserWriter", "LaserShared" */
	if (entity->type.len != 11) {
		return(0);
	} else {
		for (i = 0; i < 11; i++)
			type[i] = tolower(entity->type.str[i]);
		if ( (strncmp("laserwriter", type, 11) == 0) ||
				(strncmp("lasershared", type, 11) == 0))
			return(1);
		else
			return(0);
	}
}
