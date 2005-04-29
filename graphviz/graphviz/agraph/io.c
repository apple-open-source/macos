/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/
#include <stdio.h>
#include <aghdr.h>

/* experimental ICONV code - probably should be removed - JCE */
#undef HAVE_ICONV

#ifdef HAVE_ICONV
#include <iconv.h>
#include <langinfo.h>
#include <errno.h>
#endif

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifdef HAVE_ICONV
static int iofreadiconv(void *chan, char *buf, int bufsize)
{
#define CHARBUFSIZE 30
	static char charbuf[CHARBUFSIZE];
	static iconv_t cd=NULL;
	char *inbuf, *outbuf, *readbuf;
	size_t inbytesleft, outbytesleft, readbytesleft, resbytes, result;
	int fd;

	if (!cd) {
		cd=iconv_open(nl_langinfo(CODESET),"UTF-8");
	}
	fd = fileno((FILE*)chan);
	readbuf=inbuf=charbuf;
	readbytesleft=CHARBUFSIZE;
	inbytesleft=0;
	outbuf=buf;
	outbytesleft=bufsize-1;
	while (1) {
		if ((result=read(fd, readbuf++, 1)) != 1) break;
		readbytesleft--;
		inbytesleft++;
		result=iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
		if (result != -1) {
			readbuf=inbuf=charbuf;
			readbytesleft=CHARBUFSIZE;
			inbytesleft=0;
		}
		else if (errno != EINVAL)  break;
	}
	*outbuf='\0';
	resbytes=bufsize-1-outbytesleft;
	if (resbytes) result=resbytes;
	return result;
}
#endif

static int iofread(void *chan, char *buf, int bufsize)
{
	return read(fileno((FILE*)chan), buf, bufsize);
	/* return fread(buf, 1, bufsize, (FILE*)chan); */
}

/* default IO methods */
static int ioputstr(void *chan, char *str)
{
	return fputs(str, (FILE*)chan);
}

static int ioflush(void *chan)
{
	return fflush((FILE*)chan);
}

/* Agiodisc_t AgIoDisc = { iofreadiconv, ioputstr, ioflush }; */
Agiodisc_t AgIoDisc = { iofread, ioputstr, ioflush };
