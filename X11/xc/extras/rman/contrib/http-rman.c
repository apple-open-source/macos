/* $Id: http-rman.c,v 1.2 1994/05/15 14:55:12 fredrik Exp $
 *
 * Name:
 *	http-rman.c -- a rudimentary man-page HTTP server
 *
 * Description:
 *	This is a minimal HTTP server using RosettaMan by T.A. Phelps
 *	(phelps@ACM.org) to produce hypertext renditions
 *	of man-pages on the fly.
 *
 *	This server processes URLs with the following syntax:
 *
 *		[/man] ?<command> [ ?<section> ]
 *
 *	For URLs matching this format, it pipes the output of
 *	man <section> <command> through rman and sends it to
 *	the HTTP client. For other URLs, it returns the document
 *	given as argv[1] (using cat(1). The leading /man is
 *	optional, but is strongly recommended.
 *
 *	This server is shipped as two files, the http-rman.c
 *	sources and the http-rman.html sample frontpage. I have
 *	not included a Makefile; you can write your own or just
 *	type [g]cc -o http-rman http-rman.c
 *
 * What do I need to run this:
 *	If you don't have it, pick up RosettaMan by anonymous ftp
 *	from ftp.cs.berkeley.edu: /ucb/people/phelps/tcl/rman.tar.Z
 *
 *	You'll also need an HTTP client such as NCSA Mosaic to talk
 *	to this server. Mosaic is available by anonymous FTP from
 *	ftp://ftp.ncsa.uiuc.edu/Mosaic 
 *
 *	Both RosettaMan (rman) and Mosaic are available from many
 *	other sites. Try Archie, or check your local or national net
 *	archive.
 *
 * How do I get it running:
 *	First, compile the server (see above), and install it
 *	somewhere.
 *
 *	The server runs under inetd(8). Add a service to
 *	/etc/services, say:
 *
 *		http-rman 4080/tcp
 *
 *	If you're not about to install another HTTP server on your
 *	machine, you may use the default HTTP port, 80, instead.
 *
 *	Then add an entry to /etc/inetd.conf, such as (on a single line):
 *
 *		http-rman stream tcp nowait root /usr/local/bin/http-rman
 *			http-rman /usr/local/lib/rman/http-rman.html
 *
 *	Change /usr/local/bin and /usr/local/lib/rman to where you
 *	installed the two files. In addition, you may wish to run
 *	the server as something other than root...
 *
 *	Restart inetd(8) (use kill -HUP or kill it and start it again)
 *	and try the following:
 *
 *	$ Mosaic http://localhost:4080
 *
 *	If you don't have Mosaic, try the following instead:
 *
 *	$ telnet localhost 4080
 *	Trying 127.0.0.1...
 *	Connected to localhost.
 *	Escape character is '^]'.
 *	GET /man?ls <return>
 *	<return>
 *	HTTP/1.0 200 OK
 *	...
 *
 * Portability:
 *	You'll need an ANSI compiler (or an editor and some patience).
 *	As it stands right now, this code has been successfully
 *	compiled on OSF/1 AXP using cc, and on SunOS 4.1 using gcc.
 *	Might need some tuning for other platforms.
 *
 * Legal Issues:
 *	Check the external visibility of the http-rman service
 *	you choose. This server gives a user access to ALL man-
 *	pages on your machine. You may have installed copyrighted
 *	software (your operating system, for example) with
 *	man-pages that you are NOT allowed to make visible for
 *	anyone out there...
 *
 * History:
 *	94-04-30 fl: created
 *	94-05-13 fl: stripped away everything but rman support
 *
 * Copyright (c) Fredrik Lundh 1994 (fredrik_lundh@ivab.se)
 * All rights reserved.
 */


#include <stdio.h>		/* printf(), fflush(stdio) etc */
#include <string.h>		/* strrchr(), strcmp() etc */


static int
http_error(int error)
{
    char *p;

    switch (error) {
    case 400:
	p = "Bad Request";
	break;
    case 404:
	p = "Not Found";
	break;
    default:
	p = "Error";
    }
    printf("HTTP/1.0 %d %s\r\n", error, p);
    printf("MIME-version: 1.0\r\n");
    printf("Content-Type: text/html\r\n\r\n");
    printf("<head><title>%d %s</title></head>\r\n", error, p);
    printf("<body><h1>%d %s</h1></body>\r\n", error, p);
    return 0;
}


static int
http_rman(char *url)
{
    char *pName;
    char *pSection;
    char buf[200];

    /* parse URL: should be /man?command[?section] */
    pSection = strrchr(url, '?');
    if (!pSection) {
	return -1;
    }
    pName = pSection-1;
    *pSection++ = '\0';

    pName = strrchr(url, '?');
    if (!pName) {
	pName = pSection;
	pSection = "";
    }
    else
	pName++;

    sprintf(buf, "man %s %s | rman -r \"man?%%s?%%s\" -n %s -f html",
	    pSection, pName, pName);

    return system(buf);
}


int
main(int ac, char **av)
{
    char buf[200];
    char url[200];
    int status;
    char *sFrontpage = "/usr/local/lib/rman/http-rman.html";

/* check arguments */

    if (ac > 1)
	sFrontpage = av[1];

/* just read in one line from stdin and make sure it is a GET
command */

    if (gets(buf)) {

	/* read rest of command (just for the sake of it) */
	while (gets(url) && url[0] != '\r')
	    ;

	/* command should be GET <url> [HTTP/1.0] */
	if (sscanf(buf, "GET %s", url) == 1) {

	    status  = http_rman(url);

	    if (status < 0) {
		sprintf(buf, "cat %s", sFrontpage);
		if (system(buf) == 0)
		    status = 0;
	    }

	    if (status < 0)
		http_error(404);

	} else

	    http_error(400);
    }

    fflush(stdout);

    exit(0);
}
