/*-
 * Copyright 1997 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.	 M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.	 It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: util.c,v 1.7 2002/06/16 23:47:19 lutherj Exp $
 */

/* FD_SETSIZE has to be defined here before sys/types.h is brought in
 * to allow select to work on file descriptors higher than 256.	 This 
 * number should be at least WEBDAV_MAX_FILES + the number of sockets
 * we may be using at any given time (probably 10).	 A buffer is a 
 * good idea here.	*/
 
#define FD_SETSIZE 2500

#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>			/* for time() */
#include <sys/dir.h>		/* for MAXNAMLEN */ 
#include <unistd.h>

#include <sys/time.h>		/* for struct timeval */
#include <sys/socket.h>

#include "webdavd.h"
#include "pathnames.h"
#include "fetch.h"

/*****************************************************************************/

/* String-handling and -parsing functions */

/*****************************************************************************/

/*
 * Undo the standard %-sign encoding in URIs (e.g., `%2f' -> `/').	This
 * must be done after the URI is parsed, since the principal purpose of
 * the encoding is to hide characters which would otherwise be significant
 * to the parser (like `/').
 */
char *percent_decode(const char *uri)
{
	char *rv,  *s;

	s = malloc(strlen(uri) + 1);
	if (!s)
	{
		return (s);
	}

	rv = s;
	while (*uri)
	{
		if (*uri == '%' && uri[1] && isxdigit(uri[1]) && isxdigit(uri[2]))
		{
			int c;
			char buf[] = "xx";

			buf[0] = uri[1];
			buf[1] = uri[2];
			sscanf(buf, "%x", &c);
			uri += 3;
			*s++ = c;
		}
		else
		{
			*s++ = *uri++;
		}
	}
	*s = '\0';
	return rv;
}

/*****************************************************************************/

/*
 * Undo the standard %-sign encoding in URIs (e.g., `%2f' -> `/').	This
 * differs from the above routine in that it does the conversion within
 * the input string
 */

void percent_decode_in_place(char *uri)
{
	char *s;

	s = uri;

	while (*uri)
	{
		if (*uri == '%' && uri[1] && isxdigit(uri[1]) && isxdigit(uri[2]))
		{
			int c;
			char buf[] = "xx";

			buf[0] = uri[1];
			buf[1] = uri[2];
			sscanf(buf, "%x", &c);
			uri += 3;
			*s++ = c;
		}
		else
		{
			*s++ = *uri++;
		}
	}
	*s = '\0';
}

/*****************************************************************************/

/*
 *	Translate utf-8 url to http us-ascii via % encoding
 *	Caller must free the allocated return string
 */

char *utf8_encode(const unsigned char *orig)
{
	int index = 0, orig_index = 0;
	int charval;
	char *new_string = NULL,  *slash = NULL;

	new_string = malloc((size_t)(strlen(orig) * UTF8_TO_ASCII_MAX_SCALE));
	if (!new_string)
	{
		return (NULL);
	}

	/* we don't want to escape characters in the host name part */
	slash = strchr(orig, '/');					/* the end of the host name */

	while (orig[orig_index] != '\0')
	{
		charval = (int)orig[orig_index];
		if (((char *) & orig[orig_index] > slash) &&
			(charval <= 32 || charval == 34 || charval == 35 || charval == 37 ||
			 charval == 38 || (charval >= 58 && charval <= 64) ||
			 (charval >= 91 && charval <= 94) || charval == 96 || charval >= 123))
		{

			/*
			 * In other words if c is not one of the legitimate http accepted subset
			 * of US-ASCII characters, we will escape it.  That long if statement is
			 * designed to screen out all control charaters, the space character, all
			 * hi bit ascii characters the delete character and the "#%<>[]^{|} 
			 * characters.  That quote by the way is an excluded character not the
			 * beginning of a string as you may have guessed
			 */

			new_string[index] = '%';
			++index;
			sprintf(&(new_string[index]), "%02x", charval);
			index += 2;
		}
		else
		{
			new_string[index] = orig[orig_index];
			++index;
		}
		++orig_index;
	}											/* end while */

	new_string[index] = '\0';
	return (new_string);
}

/*****************************************************************************/

/*
 * Reconstruct a URL given	a standard host:port string and a remote
 * request which contains all of the url except the host name.
 */

int reconstruct_url(const char *hostheader, const char *remotefile, char **url)
{

	const char *colon;
	unsigned long length;

	length = strlen(hostheader) + strlen(remotefile) + strlen(_WEBDAVPREFIX) + 1;
	(void *) * url = malloc(length);

	if (!*url)
	{
		return (ENOMEM);
	}

	(void)strcpy(*url, _WEBDAVPREFIX);
	colon = strchr(hostheader, ':');

	if (colon != 0)
	{
		errno = 0;
		(void)strncat(*url, hostheader, colon - hostheader);
	}
	else
	{
		(void)strcat(*url, hostheader);
	}

	(void)strcat(*url, remotefile);

	return 0;
}

/*****************************************************************************/

/*
 * Implement the `base64' encoding as described in RFC 1521.
 */
static const char base64[] = 
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*****************************************************************************/

char *to_base64(const unsigned char *buf, size_t len)
{
	char *s,  *rv;
	unsigned tmp;

	s = malloc((4 * (len + 1)) / 3 + 1);
	if (!s)
	{
		return (0);
	}

	rv = s;
	while (len >= 3)
	{
		tmp = buf[0] << 16 | buf[1] << 8 | buf[2];
		s[0] = base64[tmp >> 18];
		s[1] = base64[(tmp >> 12) & 077];
		s[2] = base64[(tmp >> 6) & 077];
		s[3] = base64[tmp & 077];
		len -= 3;
		buf += 3;
		s += 4;
	}

	/* RFC 1521 enumerates these three possibilities... */
	switch (len)
	{
		case 2:
			tmp = buf[0] << 16 | buf[1] << 8;
			s[0] = base64[(tmp >> 18) & 077];
			s[1] = base64[(tmp >> 12) & 077];
			s[2] = base64[(tmp >> 6) & 077];
			s[3] = '=';
			s[4] = '\0';
			break;
			
		case 1:
			tmp = buf[0] << 16;
			s[0] = base64[(tmp >> 18) & 077];
			s[1] = base64[(tmp >> 12) & 077];
			s[2] = s[3] = '=';
			s[4] = '\0';
			break;
			
		case 0:
			s[0] = '\0';
			break;
	}

	return rv;
}

/*****************************************************************************/

int from_base64(const char *orig, unsigned char *buf, size_t *lenp)/* *** not used? *** */
{
	int len, len2;
	const char *equals;
	unsigned tmp;

	len = strlen(orig);
	while (isspace(orig[len - 1]))
	{
		len--;
	}

	if (len % 4)
	{
		return -1;
	}

	len2 = 3 * (len / 4);
	equals = strchr(orig, '=');
	if (equals != 0)
	{
		if (equals[1] == '=')
		{
			len2 -= 2;
		}
		else
		{
			len2 -= 1;
		}
	}

	/* Now the length is len2 is the actual length of the original. */
	if (len2 > *lenp)
	{
		return -1;
	}
	*lenp = len2;

	while (len > 0)
	{
		int i;
		const char *off;
		int forget;

		tmp = 0;
		forget = 0;
		for (i = 0; i < 4; i++)
		{
			if (orig[i] == '=')
			{
				off = base64;
				forget++;
			}
			else
			{
				off = strchr(base64, orig[i]);
			}
			if (off == 0)
			{
				return -1;
			}
			tmp = (tmp << 6) | (off - base64);
		}

		buf[0] = (tmp >> 16) & 0xff;
		if (forget < 2)
		{
			buf[1] = (tmp >> 8) & 0xff;
		}
		if (forget < 1)
		{
			buf[2] = (tmp >> 8) & 0xff;
		}
		len -= 4;
		orig += 4;
		buf += 3 - forget;
	}
	return 0;
}

/*****************************************************************************/

ssize_t socket_read_bytes(int fd, char *buf, size_t n)
{
	fd_set fdset;
	int ret = 0;
	struct timeval timeout;

	if (n == 0)
	{
		return (0);
	}

	bzero(&timeout, sizeof(timeout));
	timeout.tv_sec = WEBDAV_IO_TIMEOUT;

	FD_ZERO(&fdset);
	FD_SET(fd, &fdset);
	ret = select(fd + 1, &fdset, (fd_set *)0, (fd_set *)0, &timeout);

	/* If select returns a postive number, our socket is ready for reading */
	/* Otherwise it timed out or generated an errror, so return EIO */

	if (ret > 0)
	{
		ssize_t len = recv(fd, buf, n, 0);
		if (len != -1)
		{
			return len;
		}
		/* fall through, for now */
	}

	/* there was an error */
#ifdef DEBUG
	if (ret == 0)
	{
		fprintf(stderr, "read timed out\n");
	}
#endif

	errno = EIO;
	return 0;

}

/*****************************************************************************/

#define USUAL_LINE_LEN 48

ssize_t socket_read_line(int fd, char *buf, size_t n)
{
	fd_set fdset;
	int ret = 0, found_lf = 0;
	struct timeval timeout;

	size_t start = 0, 							/* index of start of current read */
	end = 0,									/* one past the last byte read in the peek */
	index = 0,									/* one past the last byte read for real */
	max = n - 1;								/* maximum length to be read, leaving room
												  for the NULL string terminator */
	ssize_t len;

	if (n == 0)
	{
		return (0);
	}

	bzero(&timeout, sizeof(timeout));
	timeout.tv_sec = WEBDAV_IO_TIMEOUT;

wait_for_data:
	
	FD_ZERO(&fdset);
	FD_SET(fd, &fdset);
	ret = select(fd + 1, &fdset, (fd_set *)0, (fd_set *)0, &timeout);

	/* If select returns a postive number, our socket is ready for reading */
	/* Otherwise it timed out or generated an errror, so return EIO */

	if (ret > 0)
	{
		/* peek at the data queued on the socket */
		start = index;
		len = recv(fd, &buf[start], MIN(USUAL_LINE_LEN, (max - start)), MSG_PEEK);
		if (len > 0)
		{
			for (end = (start + len); index < end; index++)
			{
				if (buf[index] == '\n')
				{
					found_lf = 1;
					index++;
					break;
				}
			}
			/* now do a read that actually consumes data queued on the socket */
			len = recv(fd, &buf[start], (index - start), 0);
			if (len > 0)
			{
				buf[index] = '\0';
				if (found_lf)
				{
					return (index);
				}
				else
				{
					if (index < max)
					{
						/* So long as we read something and there's still
						  room in the buffer, since it didn't time out,
						  we should try again. */
						goto wait_for_data;
					}
					/* buffer is full and no LF was found; fall through */
				}
			}
			/* read failed unexpectedly; fall through */
		}
		/* read (peek) failed unexpectedly; fall through */
	}
#ifdef DEBUG
	if (ret == 0)
	{
		fprintf(stderr, "read timed out\n");
	}
#endif

	errno = EIO;
	return 0;
}

/*****************************************************************************/

void zero_trailing_spaces(char *line)
{
	char *cp;
	if (line)
	{
		for (cp = (line + strlen(line) - 1); (cp >= line && isspace(*cp)); cp--)
		{
			*cp = '\0';
		}
	}
}

/*****************************************************************************/

/*
 * SkipQuotedString finds the end of a quoted-string using the rules
 * (rfc 2616, section 2.2):
 *	
 *	quoted-string	= ( <"> *(qdtext | quoted-pair ) <"> )
 *	qdtext			= <any TEXT except <">>
 *	quoted-pair		= "\" CHAR
 *	
 * On input, the bytes parameter points to the character AFTER the initial
 * '"' character. The function result is a pointer to the '"' character that
 * terminates the quoted-string or the end of the C string.
 */
char * SkipQuotedString(char *bytes)
{
	while ( *bytes != '\0' )
	{
		/* the end of the quoted-string? */
		if ( *bytes == '\"' )
		{
			break;
		}
		/* quoted-pair within the quoted-string? */
		else if ( *bytes == '\\' && bytes[1] )
		{
			/* skip quoted-pair */
			bytes += 2;
		}
		else
		{
			/* skip character */
			++bytes;
		}
	}
	
	return ( bytes );
}

/*****************************************************************************/

/*
 * SkipCodedURL finds the end of a Coded-URL using the rules
 * (rfc 2518, section 9.4 and rfc 2396):
 *	
 *	Coded-URL		= "<" absoluteURI ">"
 *	
 * On input, the bytes parameter points to the character AFTER the initial
 * '<' character. The function result is a pointer to the '>' character that
 * terminates the Coded-URL or the end of the C string.
 */
char * SkipCodedURL(char *bytes)
{
	/* the end of the string or Coded-URL? */
	while ( (*bytes != '\0') && (*bytes != '>') )
	{
		/* skip character */
		++bytes;
	}
	
	return ( bytes );
}


/*****************************************************************************/

/*
 * SkipToken finds the end of a token using the rules (rfc 2616, section 2.2):
 *	
 *	token		= 1*<any CHAR except CTLs or separators>
 *	CTL			= <any US-ASCII control character (octets 0 - 31) and
 *				  DEL (127)>
 *	separators	= "(" | ")" | "<" | ">" | "@"
 *				  | "," | ";" | ":" | "\" | <">
 *				  | "/" | "[" | "]" | "?" | "="
 *				  | "{" | "}" | SP | HT
 *	
 * The function result is a pointer to the first non token character or the
 * end of the C string.
 */
char * SkipToken(char *bytes)
{
	while ( *bytes != '\0' )
	{
		/* CTL - US-ASCII control character (octets 0 - 31) */
		if ( (unsigned char)*bytes <= 31 )
		{
			/* not a token character - done */
			goto Done;
		}
		else
		{
			switch ( *bytes )
			{
			/* CTL - DEL (127) */
			case '\x7f':
			/* separators */
			case '(':
			case ')':
			case '<':
			case '>':
			case '@':
			case ',':
			case ';':
			case ':':
			case '\\':
			case '\"':
			case '/':
			case '[':
			case ']':
			case '\?':
			case '=':
			case '{':
			case '}':
			case ' ':
			case '\t':
				/* not a token character - done */
				goto Done;
				break;
			
			default:
				/* skip token characters */
				++bytes;
				break;
			}
		}
	}

Done:	
	return (bytes);
}

/*****************************************************************************/

/*
 * SkipLWS finds the end of a run of LWS using the rule
 * (rfc 2616, section 2.2):
 *	
 *	LWS = [CRLF] 1*( SP | HT )
 *	
 * The function result is a pointer to the first non LWS character or the end
 * of the C string.
 */
char * SkipLWS(char *bytes)
{
	while ( *bytes != '\0' )
	{
		if ( (*bytes == ' ') || (*bytes == '\t') )
		{
			/* skip SP and HT characters */
			++bytes;
			continue;
		}
		/*
		 * skip CRLF only if followed by SP or HT (in which case the SP 
		 * or HT can be skipped, too)
		 */
		else if ( *bytes == '\x0d' ) /* CR? */
		{
			/* LF? */
			if ( bytes[1] == '\x0a' )
			{
				/* SP or HT? */
				if ( (bytes[2] == ' ') || (bytes[2] == '\t') )
				{	
					/* skip CRLF followed by SP or HT */
					bytes += 3;
					continue;
				}
			}
		}
		
		/* found the end of the LWS run */
		break;
	}
	
	return ( bytes );
}

/*****************************************************************************/
