#include "config.h"
#include <stdio.h>
#include <ctype.h>
#include "sdbm.h"

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>
extern int errno;

void
oops(s1, s2)
register char *s1;
register char *s2;
{
#ifndef HAVE_STRERROR
	extern int sys_nerr;
	extern char *sys_errlist[];
#endif
	extern char *progname;

	if (progname)
		fprintf(stderr, "%s: ", progname);
	fprintf(stderr, s1, s2);
#ifndef HAVE_STRERROR
	if (errno > 0 && errno < sys_nerr)
		fprintf(stderr, " (%s)", sys_errlist[errno]);
#else
	if (errno > 0)
		fprintf(stderr, " (%s)", strerror(errno));
#endif
	fprintf(stderr, "\n");
	exit(1);
}

int
okpage(char *pag)
{
	register unsigned n;
	register off;
	register short *ino = (short *) pag;

	if ((n = ino[0]) > PBLKSIZ / sizeof(short))
		return 0;

	if (!n)
		return 1;

	off = PBLKSIZ;
	for (ino++; n; ino += 2) {
		if (ino[0] > off || ino[1] > off ||
		    ino[1] > ino[0])
			return 0;
		off = ino[1];
		n -= 2;
	}

	return 1;
}



/***************************************************************************\
**                                                                         **
**   Function name: getopt()                                               **
**   Author:        Henry Spencer, UofT                                    **
**   Coding date:   84/04/28                                               **
**                                                                         **
**   Description:                                                          **
**                                                                         **
**   Parses argv[] for arguments.                                          **
**   Works with Whitesmith's C compiler.                                   **
**                                                                         **
**   Inputs   - The number of arguments                                    **
**            - The base address of the array of arguments                 **
**            - A string listing the valid options (':' indicates an       **
**              argument to the preceding option is required, a ';'        **
**              indicates an argument to the preceding option is optional) **
**                                                                         **
**   Outputs  - Returns the next option character,                         **
**              '?' for non '-' arguments                                  **
**              or ':' when there is no more arguments.                    **
**                                                                         **
**   Side Effects + The argument to an option is pointed to by 'optarg'    **
**                                                                         **
*****************************************************************************
**                                                                         **
**   REVISION HISTORY:                                                     **
**                                                                         **
**     DATE           NAME                        DESCRIPTION              **
**   YY/MM/DD  ------------------   ------------------------------------   **
**   88/10/20  Janick Bergeron      Returns '?' on unamed arguments        **
**                                  returns '!' on unknown options         **
**                                  and 'EOF' only when exhausted.         **
**   88/11/18  Janick Bergeron      Return ':' when no more arguments      **
**   89/08/11  Janick Bergeron      Optional optarg when ';' in optstring  **
**                                                                         **
\***************************************************************************/

char *optarg;			       /* Global argument pointer. */

#ifdef VMS
#define index  strchr
#endif

#ifndef HAVE_GETOPT
char
getopt(argc, argv, optstring)
int argc;
char **argv;
char *optstring;
{
	register int c;
	register char *place;
	extern char *index();
	static int optind = 0;
	static char *scan = NULL;

	optarg = NULL;

	if (scan == NULL || *scan == '\0') {

		if (optind == 0)
			optind++;
		if (optind >= argc)
			return ':';

		optarg = place = argv[optind++];
		if (place[0] != '-' || place[1] == '\0')
			return '?';
		if (place[1] == '-' && place[2] == '\0')
			return '?';
		scan = place + 1;
	}

	c = *scan++;
	place = index(optstring, c);
	if (place == NULL || c == ':' || c == ';') {

		(void) fprintf(stderr, "%s: unknown option %c\n", argv[0], c);
		scan = NULL;
		return '!';
	}
	if (*++place == ':') {

		if (*scan != '\0') {

			optarg = scan;
			scan = NULL;

		}
		else {

			if (optind >= argc) {

				(void) fprintf(stderr, "%s: %c requires an argument\n",
					       argv[0], c);
				return '!';
			}
			optarg = argv[optind];
			optind++;
		}
	}
	else if (*place == ';') {

		if (*scan != '\0') {

			optarg = scan;
			scan = NULL;

		}
		else {

			if (optind >= argc || *argv[optind] == '-')
				optarg = NULL;
			else {
				optarg = argv[optind];
				optind++;
			}
		}
	}
	return c;
}
#endif
