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
 *	$Id: util.c,v 1.12 2003/08/23 01:26:57 lutherj Exp $
 */

/* FD_SETSIZE has to be defined here before sys/types.h is brought in
 * to allow select to work on file descriptors higher than 256.	 This 
 * number should be at least WEBDAV_MAX_FILES + the number of sockets
 * we may be using at any given time (probably 10).	 A buffer is a 
 * good idea here.	*/
 
#define FD_SETSIZE 2500

#include <sys/types.h>
#include <sys/syslog.h>

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
		syslog(LOG_ERR, "utf8_encode: new_string could not be allocated");
		return (NULL);
	}

	/* we don't want to escape characters in the host name part */
	slash = strchr(orig, '/');					/* the end of the host name */

	while (orig[orig_index] != '\0')
	{
		charval = (int)orig[orig_index];
		if (((const char *) & orig[orig_index] > slash) &&
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
	unsigned long webdavprefixlength;

	webdavprefixlength = strlen(_WEBDAVPREFIX);
	if (strncmp(remotefile, _WEBDAVPREFIX, webdavprefixlength) == 0)
	{
		/* remotefile is full url */
		(void *) * url = malloc(strlen(remotefile) + 1);
		if (!*url)
		{
			syslog(LOG_ERR, "reconstruct_url: *url could not be allocated");
			return (ENOMEM);
		}
		(void)strcpy(*url, remotefile);
	}
	else
	{
		length = strlen(hostheader) + strlen(remotefile) + webdavprefixlength + 1;
		(void *) * url = malloc(length);
			if (!*url)
		{
			syslog(LOG_ERR, "reconstruct_url: *url could not be allocated");
			return (ENOMEM);
		}
	
		(void)strcpy(*url, _WEBDAVPREFIX);
		colon = strchr(hostheader, ':');
	
		if (colon != NULL)
		{
			errno = 0;
			(void)strncat(*url, hostheader, (size_t)(colon - hostheader));
		}
		else
		{
			(void)strcat(*url, hostheader);
		}
	
		(void)strcat(*url, remotefile);
	}	

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

	s = malloc((((len + 2) / 3) * 4) + 1);
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

/* The from_base64 function decodes a base64 encoded c-string into outBuffer.
 * The outBuffer's size is *lengthptr. The actual number of bytes decoded into
 * outBuffer is also returned in *lengthptr. If outBuffer is large enough to
 * decode the base64 string and if the base64 encoding is valid, from_base64()
 * returns 0; otherwise -1 is returned. Note that outBuffer is just an array of
 * bytes... it is not a c-string.
 */
int from_base64(const char *base64str, unsigned char *outBuffer, size_t *lengthptr)
{
	char			decodedChar;
	unsigned long	base64Length;
	unsigned char	*eightBitByte;
	unsigned char	sixBitEncoding[4];
	unsigned short	encodingIndex;
	int				endOfData;
	const char		*equalPtr;
	const char		*base64CharPtr;
	const char		*base64EndPtr;
	
	/* Determine the length of the base64 input string.
	 * This also catches illegal '=' characters within a base64 string.
	 */
	
	base64Length = 0;
	
	/* is there an '=' character? */
	equalPtr = strchr(base64str, '=');
	if ( equalPtr != NULL )
	{
		/* yes -- then it must be the last character of an octet, or
		 * it must be the next to last character of an octet followed
		 * by another '=' character */
		switch ( (equalPtr - base64str) % 4 )
		{
			case 0:
			case 1:
				/* invalid encoding */
				goto error_exit;
				break;
				
			case 2:
				if ( equalPtr[1] != '=' ) 
				{
					/* invalid encoding */
					goto error_exit;
				}
				base64Length = (equalPtr - base64str) + 2;
				*lengthptr += 2;	/* adjust for padding */
				break;
				
			case 3:
				base64Length = (equalPtr - base64str) + 1;
				*lengthptr += 1;	/* adjust for padding */
				break;
		}
	}
	else
	{
		base64Length = strlen(base64str);
	}
	
	/* Make sure outBuffer is big enough */
	if ( *lengthptr < ((base64Length / 4) * 3) )
	{
		/* outBuffer is too small */
		goto error_exit;
	}
	
	/* Make sure length is a multiple of 4 */
	if ( (base64Length % 4) != 0 )
	{
		/* invalid encoding */
		goto error_exit;
	}
	
	/* OK -- */
	eightBitByte = outBuffer;
	encodingIndex = 0;
	endOfData = FALSE;
	base64EndPtr = (char *)((unsigned long)base64str + base64Length);
	base64CharPtr = base64str;
	while ( base64CharPtr < base64EndPtr )
	{
		decodedChar = *base64CharPtr++;
		
		if ( (decodedChar >= 'A') && (decodedChar <= 'Z') )
		{
			decodedChar = decodedChar - 'A';
		}
		else if ( (decodedChar >= 'a') && (decodedChar <= 'z') )
		{
			decodedChar = decodedChar - 'a' + 26;
		}
		else if ( (decodedChar >= '0') && (decodedChar <= '9') )
		{
			decodedChar = decodedChar - '0' + 52;
		}
		else if ( decodedChar == '+' )
		{
			decodedChar = 62;
		}
		else if ( decodedChar == '/' )
		{
			decodedChar = 63;
		}
		else if ( decodedChar == '=' ) /* end of base64 encoding */
		{
			endOfData = TRUE;
		}
		else
		{
			/* invalid character */
			goto error_exit;
		}
		
		if ( endOfData )
		{
			/* make sure there's no more looping */
			base64CharPtr = base64EndPtr;
		}
		else
		{
			sixBitEncoding[encodingIndex] = (unsigned char)decodedChar;
			++encodingIndex;
		}
		
		if ( (encodingIndex == 4) || endOfData)
		{
			/* convert four 6-bit characters into three 8-bit bytes */
			
			/* always get first byte */
			*eightBitByte++ =
				(sixBitEncoding[0] << 2) | ((sixBitEncoding[1] & 0x30) >> 4);
			if ( encodingIndex >= 3 )
			{
				/* get second byte only if encodingIndex is 3 or 4 */
				*eightBitByte++ =
					((sixBitEncoding[1] & 0x0F) << 4) | ((sixBitEncoding[2] & 0x3C) >> 2);
				if ( encodingIndex == 4 )
				{
					/* get third byte only if encodingIndex is 4 */
					*eightBitByte++ =
						((sixBitEncoding[2] & 0x03) << 6) | (sixBitEncoding[3] & 0x3F);
				}
			}
			
			/* reset encodingIndex */
			encodingIndex = 0;
		}
	}
	
	/* return the number of bytes in outBuffer and no error */
	*lengthptr = eightBitByte - outBuffer;
	return ( 0 );

error_exit:
	/* return 0 bytes in outBuffer and an error */
	*lengthptr = 0;
	return ( -1 );
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
		else
		{
			syslog(LOG_ERR, "socket_read_bytes: recv(): %s", strerror(errno));
		}
	}
	else if (ret < 0)
	{
		/* there was an error */
		syslog(LOG_ERR, "socket_read_bytes: select(): %s", strerror(errno));
	}
	else
	{
		/* select timed out */
		syslog(LOG_ERR, "socket_read_bytes: select(): timed out");
	}

	errno = EIO;
	return 0;
}

/*****************************************************************************/

#define USUAL_LINE_LEN 48	/* XXX what is this? */

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
					else
					{
						/* buffer is full and no LF was found; fall through */
						syslog(LOG_ERR, "socket_read_line: index >= max");
					}
				}
			}
			else
			{
				/* recv (peek) failed unexpectedly; fall through */
				if ( len < 0 )
				{
					/* log error message if this wasn't just a close from the server */
					syslog(LOG_ERR, "socket_read_line: recv(): %s", strerror(errno));
				}
			}
		}
		else
		{
			/* recv (peek) failed unexpectedly; fall through */
			if ( len < 0 )
			{
				/* log error message if this wasn't just a close from the server */
				syslog(LOG_ERR, "socket_read_line: recv() MSG_PEEK: %s", strerror(errno));
			}
		}
	}
	else if (ret < 0)
	{
		/* there was an error */
		syslog(LOG_ERR, "socket_read_line: select(): %s", strerror(errno));
	}
	else
	{
		/* select timed out */
		syslog(LOG_ERR, "socket_read_line: select(): timed out");
	}

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
