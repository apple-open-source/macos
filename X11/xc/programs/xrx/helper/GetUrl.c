/* $Xorg: GetUrl.c,v 1.4 2001/02/09 02:05:57 xorgcvs Exp $ */
/*

Copyright 1996, 1998 The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABIL-
ITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT
SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABIL-
ITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization from
The Open Group.

*/
/* $XFree86: xc/programs/xrx/helper/GetUrl.c,v 1.5 2003/07/20 16:12:20 tsi Exp $ */

/*
 * This file is really split into two major parts where GetUrl is implemented
 * in two completely different ways.
 * The first one, which is used when XUSE_WWW is defined uses the standalone
 * program "www" to perform the GET request.
 * The second one, based on the xtrans layer, performs the GET request
 * directly.
 */

#define Free(b) if (b) free(b)

#ifdef XUSE_WWW

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xos.h>		/* for strcmp() etc... */

/*
 * GetUrl first method, using www.
 */

int
GetUrl(char *url, char **reply_ret, int *len_ret)
{
    char buf[BUFSIZ], *reply, *ptr;
    FILE *fp;
    int readbytes, size;

    sprintf(buf, "www -source \"%s\"", url);
    fp = popen(buf, "r");
    if (fp == NULL)
	return 1;

    reply = NULL;
    size = 0;
    do {
	readbytes = fread(buf, sizeof(char), BUFSIZ, fp);
	if (readbytes != 0) {
	    /* malloc enough memory */
	    ptr = (char *) realloc(reply, sizeof(char) * size + readbytes);
	    if (ptr == NULL) {	/* memory allocation failed */
		Free(reply);
		return 1;
	    }
	    reply = ptr;
	    memcpy(reply + size, buf, readbytes);
	    size += readbytes;
	}
    } while (readbytes == BUFSIZ);
    pclose(fp);

    *reply_ret = reply;
    *len_ret = size;
    return 0;
}

#else /* XUSE_WWW */

/*
 * GetUrl second method, using the xtrans layer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* get definitions for the HTTP protocol */
#if !defined(UNIXCPP) || defined(ANSICPP)
#define TRANS(func) _HttpTrans##func
#else
#define TRANS(func) _HttpTrans/**/func
#endif
#include <X11/Xtrans.h>

#define HTTP_CONNECTION_RETRIES 5

static XtransConnInfo
OpenConnection(char *hostname, int port)
{
    int retry;				/* retry counter */
    XtransConnInfo trans_conn = NULL;	/* transport connection object */
    int connect_stat;
    char address[128];		/* address passed to the transport layer */
    char protocol[] = "tcp";	/* hardcoded transport */

    /* make the address of the form: protocol/host:port */
    sprintf(address,"%s/%s:%d",
	protocol ? protocol : "",
	hostname ? hostname : "",
	port);

    /*
     * Make the connection. Do retries in case server host has hit its
     * backlog (which, unfortunately, isn't distinguishable from there not
     * being a server listening at all, which is why we have to not retry
     * too many times).
     */
    for(retry = HTTP_CONNECTION_RETRIES; retry >= 0; retry--) {
	if ((trans_conn = _HttpTransOpenCOTSClient(address)) == NULL)
	    break;

	if ((connect_stat = _HttpTransConnect(trans_conn, address)) < 0) {
	    _HttpTransClose(trans_conn);
	    trans_conn = NULL;

	    if (connect_stat == TRANS_TRY_CONNECT_AGAIN) {
		sleep(1);
		continue;
	    } else
		break;
	}
    }

    return trans_conn;
}

static void
CloseConnection(XtransConnInfo trans_conn)
{
    _HttpTransDisconnect(trans_conn);
    _HttpTransClose(trans_conn);
}

static void
SendGetRequest(XtransConnInfo trans_conn, char *filename)
{
    char *request;
    char request_format[] =
	"GET %s HTTP/1.0\r\nUser-Agent: xrx\r\nAccept: */*\r\n\r\n";
    int request_len = strlen(filename) + sizeof(request_format) + 1;

    /* build request */
    request = (char *)malloc(request_len);
    sprintf(request, request_format, filename);
    /* send it */
    _HttpTransWrite(trans_conn, request, request_len);

    Free(request);
}

static int
ReadGetReply(XtransConnInfo trans_conn, char **reply_ret)
{
    char *reply = NULL;
    char buf[BUFSIZ];
    int len, nbytes;

    len = 0;
    do {
	nbytes = _HttpTransRead(trans_conn, buf, BUFSIZ);
	if (nbytes > 0) {
	    reply = (char *)realloc(reply, len + nbytes);
	    if (reply == NULL)
		break;
	    memcpy(reply + len, buf, nbytes);
	    len += nbytes;
	}
    } while (nbytes != 0);

    *reply_ret = reply;
    return len;
}

/* per HTTP 1.0 spec */
#define HTTP_DEFAULT_PORT 80

/* Parse given http url and return hostname, port number and path.
 * expected syntax: "http:" "//" host [ ":" port ] [ abs_path ]
 * default port: 80
 */
static int
ParseHttpUrl(char *url, char **hostname_ret, int *port_ret, char **path_ret)
{
    char HTTP[] = "http://";
    char *hostname, *path;
    int port;
    char *ptr, *bos;
    int status = 0;
    int bracketed = 0;

    /* check if it's an http url */
    if (strncmp(HTTP, url, sizeof(HTTP) - 1))
	return 1;		/* this is not an http url */

    /* parse hostname */
    bos = ptr = url + sizeof(HTTP) - 1;
    /* Check for RFC 2732 bracketed IPv6 numeric address */
    if (*ptr == '[') {
	bos++;
	while (*ptr && (*ptr != ']')) {
	    ptr++;
	}
	bracketed = 1;
    } else {
	while (*ptr && *ptr != ':' && *ptr != '/')
	    ptr++;
    }
    if (bos == ptr)
	return 1;		/* doesn't have any hostname */

    hostname = (char *)malloc(ptr - bos + 1);
    if (hostname == NULL)
	return 1;		/* not enouch memory */
    memcpy(hostname, bos, ptr - bos);
    hostname[ptr - bos] = '\0';

    /* make sure path is initialized in case of error */
    path = NULL;

    if (bracketed)
	ptr++;

    /* parse port */
    if (*ptr != ':' || ! ptr[1])
	port = HTTP_DEFAULT_PORT;
    else {
	++ptr;			/* skip ':' */
	port = 0;
	while (*ptr && *ptr != '/') {
	    if (isdigit((int) *ptr)) {
		port *= 10;
		port += *ptr - '0';
	    } else {
		status = 1;	/* bad port specification */
		goto error;
	    }
	    ptr++;
	}
    }

    /* the rest of the url is the path */
    path = (char *)malloc(strlen(ptr) + 1);
    if (path == NULL) {
	status = 1;		/* not enouch memory */
	goto error;
    }
    strcpy(path, ptr);

    /* set returned value */
    *hostname_ret = hostname;
    *port_ret = port;
    *path_ret = path;
    return 0;

error:
    Free(hostname);
    Free(path);
    return status;
}

int
GetUrl(char *url, char **reply_ret, int *len_ret)
{
    char *hostname, *path;
    int port;
    XtransConnInfo trans;
    int status = 0;

    if (ParseHttpUrl(url, &hostname, &port, &path) != 0)
	return 1;		/* invalid URL */

    trans = OpenConnection(hostname, port);
    if (trans == NULL) {
	status = 1;		/* connection failed */
	goto end;
    }

    SendGetRequest(trans, path);

    *len_ret = ReadGetReply(trans, reply_ret);

    CloseConnection(trans);

end:
    Free(hostname);
    Free(path);
    return status;
}


#endif /* XUSE_WWW */
