/* makemsg.c - aeb - 940605 */
/*
 * Read a file input with lines
 *	LABEL "text"
 * and either output two files:
 * a file msgout.c with content      char *msg[] = { "text", ... };
 * and a file msgout.h with content  #define LABEL 1
 * or output a single file:
 * a message catalog with lines      1 "text"
 *
 * The former two are used during compilation of the main program
 * and give default (English) messages. The latter output file is
 * input for gencat, and used in non-English locales.
 *
 * Call:
 *	makemsg input msgout.h msgout.c 
 * or
 *	makemsg -c input message_catalog
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef __QNX__
#include <unix.h>
#endif
extern char *index(const char *, int);
extern char *rindex(const char *, int);

#define BUFSIZE 4096

#define whitespace(c) ((c) == ' ' || (c) == '\t' || (c) == '\n')

static void
usage(void){
    fprintf (stderr, "call is: makemsg input msgout.h msgout.c\n");
    fprintf (stderr, "or:  makemsg -c input catalog\n");
    exit (1);
}

int
main(int argc, char **argv) {
    FILE *fin, *foh, *foc;
    char *s, *t;
    char *infile, *outcfile, *outhfile;
    char buf[BUFSIZE];
    int defct = 0;
    int makecat = 0;

#define getbuf	if (fgets (buf, sizeof(buf), fin) == NULL) {\
		    fprintf (stderr, "makemsg: unexpected end of input\n");\
		    fprintf (stderr, "[output file(s) removed]\n");\
		    unlink (outcfile);\
		    if (!makecat) unlink (outhfile);\
		    exit (1);\
		}

    if (argc != 4)
      usage ();

    outhfile = 0; foh = 0;	/* just to keep gcc happy */

    if (!strcmp(argv[1], "-c")) {
	makecat = 1;
	infile = argv[2];
	outcfile = argv[3];
    } else {
	infile = argv[1];
	outhfile = argv[2];
	outcfile = argv[3];
    }

    fin = fopen (infile, "r");
    if (!fin) {
	perror (infile);
	fprintf (stderr, "makemsg: cannot open input file %s\n", infile);
	usage ();
    }

    /* help people not to confuse the order of these args */
    if (!makecat) {
	s = rindex(outhfile, '.');
	if (!s || s[1] != 'h') {
	    fprintf (stderr, "defines output file should have name ending in .h\n");
	    usage ();
	}
	s = rindex(outcfile, '.');
	if (!s || s[1] != 'c') {
	    fprintf (stderr, "string output file should have name ending in .c\n");
	    usage ();
	}
    }

    if (!makecat) {
	foh = fopen (outhfile, "w");
	if (!foh) {
	    perror (argv[1]);
	    fprintf (stderr, "makemsg: cannot open output file %s\n", outhfile);
	    usage ();
	}
    }
    foc = fopen (outcfile, "w");
    if (!foc) {
	perror (argv[2]);
	fprintf (stderr, "makemsg: cannot open output file %s\n", outcfile);
	usage ();
    }

    if (makecat)
      fputs ("$quote \"\n$set 1\n", foc);
    else
      fputs ("char *msg[] = {\n  \"\",\n", foc);

    while (fgets (buf, sizeof(buf), fin) != NULL) {
	char ss;

	/* skip leading blanks and blank lines */
	s = buf;
	while (whitespace(*s))
	  s++;
	if (*s == 0)
	  continue;

	/* extract label part */
	t = s;
	while (*s && !whitespace(*s))
	  s++;
	ss = *s;
	*s = 0;
	if (makecat) {
	    /* the format here used to be "%d  ", but that breaks
	       glibc-2.1.2 gencat */
	    fprintf (foc, "%d ", ++defct); /* gencat cannot handle %2d */
	} else {
	    fprintf (foh, "#define %s %d\n", t, ++defct);
	    fprintf (foc, "/* %2d */  ", defct);
	}
	*s = ss;

	/* skip blanks and newlines until string found */
	while (whitespace(*s) || *s == 0) {
	    if (*s == 0) {
		getbuf;
		s = buf;
	    } else
	      s++;
	}

	/* output string - it may extend over several lines */
	while ((t = index(s, '\n')) == NULL || (t > buf && t[-1] == '\\')) {
	    fputs (s, foc);
	    getbuf;
	    s = buf;
	}
	*t = 0;
	fputs (s, foc);
	if (makecat)
	  fputs ("\n", foc);
	else
	  fputs (",\n", foc);
    }

    if (!makecat) {
	fputs ("};\n", foc);
	fprintf (foh, "\n#define MAXMSG %d\n", defct);
    }

    if (!makecat) {
	fclose (foh);
    }

    fclose (foc);
    fclose (fin);

    return 0;
}
