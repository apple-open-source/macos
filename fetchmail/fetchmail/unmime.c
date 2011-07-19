/*
 * MIME mail decoding.
 *
 * This module contains decoding routines for converting
 * quoted-printable data into pure 8-bit data, in MIME
 * formatted messages.
 *
 * By Henrik Storner <storner@image.dk>
 *
 * Configuration file support for fetchmail 4.3.8 by 
 * Frank Damgaard <frda@post3.tele.dk>
 * 
 * For license terms, see the file COPYING in this directory.
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "fetchmail.h"
#include "i18n.h"

static unsigned char unhex(unsigned char c)
{
  if ((c >= '0') && (c <= '9'))
    return (c - '0');
  else if ((c >= 'A') && (c <= 'F'))
    return (c - 'A' + 10);
  else if ((c >= 'a') && (c <= 'f'))
    return (c - 'a' + 10);
  else
      return 16;	/* invalid hex character */
}

static int qp_char(unsigned char c1, unsigned char c2, char *c_out)
{
  c1 = unhex(c1);
  c2 = unhex(c2);

  if ((c1 > 15) || (c2 > 15)) 
    return 1;
  else {
    *c_out = 16*c1+c2;
    return 0;
  }
}


/*
 * Routines to decode MIME QP-encoded headers, as per RFC 2047.
 */

/* States of the decoding state machine */
#define S_COPY_PLAIN        0	/* Just copy, but watch for the QP flag */
#define S_SKIP_MIMEINIT     1	/* Get the encoding, and skip header */
#define S_COPY_MIME         2	/* Decode a sequence of coded characters */

static const char MIMEHDR_INIT[]  = "=?";	/* Start of coded sequence */
static const char MIMEHDR_END[]   = "?=";	/* End of coded sequence */

void UnMimeHeader(char *hdr)
{
  /* Decode a buffer containing data encoded according to RFC
   * 2047. This only handles content-transfer-encoding; conversion
   * between character sets is not implemented.  In other words: We
   * assume the charsets used can be displayed by your mail program
   * without problems. 
   */

  /* Note: Decoding is done "in-situ", i.e. without using an
   * additional buffer for temp. storage. This is possible, since the
   * decoded string will always be shorter than the encoded string,
   * due to the encoding scheme.
   */

  int  state = S_COPY_PLAIN;
  char *p_in, *p_out, *p;
  char enc = '\0';		/* initialization pacifies -Wall */
  int  i;

  /* Speed up in case this is not a MIME-encoded header */
  p = strstr(hdr, MIMEHDR_INIT);
  if (p == NULL)
    return;   /* No MIME header */

  /* Loop through the buffer.
   *  p_in : Next char to be processed.
   *  p_out: Where to put the next processed char
   *  enc  : Encoding used (usually, 'q' = quoted-printable)
   */
  for (p_out = p_in = hdr; (*p_in); ) {
    switch (state) {
    case S_COPY_PLAIN:
      p = strstr(p_in, MIMEHDR_INIT);
      if (p == NULL) {
	/* 
	 * No more coded data in buffer, 
         * just move remainder into place. 
	 */
        i = strlen(p_in);   /* How much left */
	memmove(p_out, p_in, i);
	p_in += i; p_out += i;
      }
      else {
	/* MIME header init found at location p */
	if (p > p_in) {
          /* There are some uncoded chars at the beginning. */
          i = (p - p_in);
	  memmove(p_out, p_in, i);
	  p_out += i;
	}
	p_in = (p + 2);
	state = S_SKIP_MIMEINIT;
      }
      break;

    case S_SKIP_MIMEINIT:
      /* Mime type definition: "charset?encoding?" */
      p = strchr(p_in, '?');
      if (p != NULL) {
	/* p_in .. (p-1) holds the charset */

	/* *(p+1) is the transfer encoding, *(p+2) must be a '?' */
	if (*(p+2) == '?') {
	  enc = tolower((unsigned char)*(p+1));
	  p_in = p+3;
	  state = S_COPY_MIME;
	}
	else
	  state = S_COPY_PLAIN;
      }
      else
	state = S_COPY_PLAIN;   /* Invalid data */
      break;

    case S_COPY_MIME:
      p = strstr(p_in, MIMEHDR_END);  /* Find end of coded data */
      if (p == NULL) p = p_in + strlen(p_in);
      for (; (p_in < p); ) {
	/* Decode all encoded data */
	if (enc == 'q') {
	  if (*p_in == '=') {
	    /* Decode one char qp-coded at (p_in+1) and (p_in+2) */
	    if (qp_char(*(p_in+1), *(p_in+2), p_out) == 0)
	      p_in += 3;
	    else {
	      /* Invalid QP data - pass through unchanged. */
	      *p_out = *p_in;
	      p_in++;
	    }
	  }
	  else if (*p_in == '_') {
	    /* 
             * RFC 2047: '_' inside encoded word represents 0x20.
             * NOT a space - always the value 0x20.
             */
	    *p_out = 0x20;
	    p_in++;
	  }
	  else {
	    /* Copy unchanged */
	    *p_out = *p_in;
	    p_in++;
	  }
	  p_out++;
	}
	else if (enc == 'b') {
	  /* Decode base64 encoded data */
	  char delimsave;
	  int decoded_count;

	  delimsave = *p; *p = '\r';
	  decoded_count = from64tobits(p_out, p_in, 0);
	  *p = delimsave;
	  if (decoded_count > 0) 
	    p_out += decoded_count;            
	  p_in = p;
	}
	else {
	  /* Copy unchanged */
	  *p_out = *p_in;
	  p_in++;
	  p_out++;
	}
      }
      if (*p_in)
	p_in += 2;   /* Skip the MIMEHDR_END delimiter */

      /* 
       * We've completed decoding one encoded sequence. But another
       * may follow immediately, in which case whitespace before the
       * new MIMEHDR_INIT delimiter must be discarded.
       * See if that is the case 
       */
      p = strstr(p_in, MIMEHDR_INIT);
      state = S_COPY_PLAIN;
      if (p != NULL) {
	/*
	 * There is more MIME data later on. Is there
         * whitespace  only before the delimiter? 
	 */
        char *q;
        int  wsp_only = 1;

        for (q=p_in; (wsp_only && (q < p)); q++)
          wsp_only = isspace((unsigned char)*q);

        if (wsp_only) {
	  /* 
	   * Whitespace-only before the MIME delimiter. OK,
           * just advance p_in to past the new MIMEHDR_INIT,
           * and prepare to process the new MIME charset/encoding
	   * header.
	   */
	  p_in = p + sizeof(MIMEHDR_INIT) - 1;
	  state = S_SKIP_MIMEINIT;
        }
      }
      break;
    }
  }

  *p_out = '\0';
}



/*
 * Routines for decoding body-parts of a message.
 *
 * Since the "fetch" part of fetchmail gets a message body
 * one line at a time, we need to maintain some state variables
 * across multiple invokations of the UnMimeBodyline() routine.
 * The driver routine should call MimeBodyType() when all
 * headers have been received, and then UnMimeBodyline() for
 * every line in the message body.
 *
 */
#define S_BODY_DATA 0
#define S_BODY_HDR  1

/* 
 * Flag indicating if we are currently processing 
 * the headers or the body of a (multipart) message.
 */
static int  BodyState = S_BODY_DATA;

/* 
 * Flag indicating if we are in the process of decoding
 * a quoted-printable body part.
 */
static int  CurrEncodingIsQP = 0;
static int  CurrTypeNeedsDecode = 0;

/* 
 * Delimiter for multipart messages. RFC 2046 states that this must
 * NEVER be longer than 70 characters. Add 3 for the two hyphens
 * at the beginning, and a terminating null.
 */
#define MAX_DELIM_LEN 70
static char MultipartDelimiter[MAX_DELIM_LEN+3];


/* This string replaces the "Content-Transfer-Encoding: quoted-printable"
 * string in all headers, including those in body-parts. The replacement
 * must be no longer than the original string.
 */
static const char ENC8BIT[] = "Content-Transfer-Encoding: 8bit";
static void SetEncoding8bit(char *XferEncOfs)
{
  char *p;

  if (XferEncOfs != NULL) {
     memcpy(XferEncOfs, ENC8BIT, sizeof(ENC8BIT) - 1);

     /* If anything left, in this header, replace with whitespace */
     for (p=XferEncOfs+sizeof(ENC8BIT)-1; ((unsigned char)*p >= ' '); p++)
       *p=' ';
  }
}

static char *GetBoundary(char *CntType)
{
  char *p1, *p2;
  int flag;

  /* Find the "boundary" delimiter. It must be preceded with a ';'
   * and optionally some whitespace.
   */
  p1 = CntType;
  do {
    p2 = strchr(p1, ';'); 
    if (p2)
      for (p2++; isspace((unsigned char)*p2); p2++);

    p1 = p2;
  } while ((p1) && (strncasecmp(p1, "boundary", 8) != 0));

  if (p1 == NULL)
    /* No boundary delimiter */
    return NULL;

  /* Skip "boundary", whitespace and '='; check that we do have a '=' */
  for (p1+=8, flag=0; (isspace((unsigned char)*p1) || (*p1 == '=')); p1++)
    flag |= (*p1 == '=');
  if (!flag)
    return NULL;

  /* Find end of boundary delimiter string */
  if (*p1 == '\"') {
    /* The delimiter is inside quotes */
    p1++;
    p2 = strchr(p1, '\"');
    if (p2 == NULL)
      return NULL;  /* No closing '"' !?! */
  }
  else {
    /* There might be more text after the "boundary" string. */
    p2 = strchr(p1, ';');  /* Safe - delimiter with ';' must be in quotes */
  }

  /* Zero-terminate the boundary string */
  if (p2 != NULL)
    *p2 = '\0';

  return (p1 && strlen(p1)) ? p1 : NULL;
}


static int CheckContentType(char *CntType)
{
  /*
   * Static array of Content-Type's for which we will do
   * quoted-printable decoding, if requested. 
   * It is probably wise to do this only on known text-only types;
   * be really careful if you change this.
   */

  static const char *DecodedTypes[] = {
    "text/",        /* Will match ALL content-type's starting with 'text/' */
    "message/rfc822", 
    NULL
  };

  char *p = CntType;
  int i;

  /* If no Content-Type header, it isn't MIME - don't touch it */
  if (CntType == NULL) return 0;

  /* Skip whitespace, if any */
  for (; isspace((unsigned char)*p); p++) ;

  for (i=0; 
       (DecodedTypes[i] && 
	(strncasecmp(p, DecodedTypes[i], strlen(DecodedTypes[i])))); 
       i++) ;

  return (DecodedTypes[i] != NULL);
}


/*
 * This routine does three things:
 * 1) It determines - based on the message headers - whether the
 *    message body is a MIME message that may hold 8 bit data.
 *    - A message that has a "quoted-printable" or "8bit" transfer 
 *      encoding is assumed to contain 8-bit data (when decoded).
 *    - A multipart message is assumed to contain 8-bit data
 *      when decoded (there might be quoted-printable body-parts).
 *    - All other messages are assumed NOT to include 8-bit data.
 * 2) It determines the delimiter-string used in multi-part message
 *    bodies.
 * 3) It sets the initial values of the CurrEncodingIsQP, 
 *    CurrTypeNeedsDecode, and BodyState variables, from the header 
 *    contents.
 *
 * The return value is a bitmask.
 */
int MimeBodyType(char *hdrs, int WantDecode)
{
    char *NxtHdr = hdrs;
    char *XferEnc, *XferEncOfs, *CntType, *MimeVer, *p;
    int  HdrsFound = 0;     /* We only look for three headers */
    int  BodyType;          /* Return value */ 

    /* Setup for a standard (no MIME, no QP, 7-bit US-ASCII) message */
    MultipartDelimiter[0] = '\0';
    CurrEncodingIsQP = CurrTypeNeedsDecode = 0;
    BodyState = S_BODY_DATA;
    BodyType = 0;

    /* Just in case ... */
    if (hdrs == NULL)
	return BodyType;

    XferEnc = XferEncOfs = CntType = MimeVer = NULL;

    do {
	if (strncasecmp("Content-Transfer-Encoding:", NxtHdr, 26) == 0) {
	    XferEncOfs = NxtHdr;
	    p = nxtaddr(NxtHdr);
	    if (p != NULL) {
		xfree(XferEnc);
		XferEnc = xstrdup(p);
		HdrsFound++;
	    }
	}
	else if (strncasecmp("Content-Type:", NxtHdr, 13) == 0) {
	    /*
	     * This one is difficult. We cannot use the standard
	     * nxtaddr() routine, since the boundary-delimiter is
	     * (probably) enclosed in quotes - and thus appears
	     * as an rfc822 comment, and nxtaddr() "eats" up any
	     * spaces in the delimiter. So, we have to do this
	     * by hand.
	     */

	    /* Skip the "Content-Type:" part and whitespace after it */
	    for (NxtHdr += 13; ((*NxtHdr == ' ') || (*NxtHdr == '\t')); NxtHdr++);

	    /* 
	     * Get the full value of the Content-Type header;
	     * it might span multiple lines. So search for
	     * a newline char, but ignore those that have a
	     * have a TAB or space just after the NL (continued
	     * lines).
	     */
	    p = NxtHdr-1;
	    do {
		p=strchr((p+1),'\n'); 
	    } while ( (p != NULL) && ((*(p+1) == '\t') || (*(p+1) == ' ')) );
	    if (p == NULL) p = NxtHdr + strlen(NxtHdr);

	    xfree(CntType);
	    CntType = (char *)xmalloc(p-NxtHdr+1);
	    strlcpy(CntType, NxtHdr, p-NxtHdr+1);
	    HdrsFound++;
	}
	else if (strncasecmp("MIME-Version:", NxtHdr, 13) == 0) {
	    p = nxtaddr(NxtHdr);
	    if (p != NULL) {
		xfree(MimeVer);
		MimeVer = xstrdup(p);
		HdrsFound++;
	    }
	}

	NxtHdr = (strchr(NxtHdr, '\n'));
	if (NxtHdr != NULL) NxtHdr++;
    } while ((NxtHdr != NULL) && (*NxtHdr) && (HdrsFound != 3));


    /* Done looking through the headers, now check what they say */
    if ((MimeVer != NULL) && (strcmp(MimeVer, "1.0") == 0)) {

	CurrTypeNeedsDecode = CheckContentType(CntType);

	/* Check Content-Type to see if this is a multipart message */
	if ( (CntType != NULL) &&
		((strncasecmp(CntType, "multipart/mixed", 16) == 0) ||
		 (strncasecmp(CntType, "message/", 8) == 0)) ) {

	    char *p1 = GetBoundary(CntType);

	    if (p1 != NULL) {
		/* The actual delimiter is "--" followed by 
		   the boundary string */
		strcpy(MultipartDelimiter, "--");
		strlcat(MultipartDelimiter, p1, sizeof(MultipartDelimiter));
		MultipartDelimiter[sizeof(MultipartDelimiter)-1] = '\0';
		BodyType = (MSG_IS_8BIT | MSG_NEEDS_DECODE);
	    }
	}

	/* 
	 * Check Content-Transfer-Encoding, but
	 * ONLY for non-multipart messages (BodyType == 0).
	 */
	if ((XferEnc != NULL) && (BodyType == 0)) {
	    if (strcasecmp(XferEnc, "quoted-printable") == 0) {
		CurrEncodingIsQP = 1;
		BodyType = (MSG_IS_8BIT | MSG_NEEDS_DECODE);
		if (WantDecode && CurrTypeNeedsDecode) {
		    SetEncoding8bit(XferEncOfs);
		}
	    }
	    else if (strcasecmp(XferEnc, "7bit") == 0) {
		CurrEncodingIsQP = 0;
		BodyType = (MSG_IS_7BIT);
	    }
	    else if (strcasecmp(XferEnc, "8bit") == 0) {
		CurrEncodingIsQP = 0;
		BodyType = (MSG_IS_8BIT);
	    }
	}

    }

    xfree(XferEnc);
    xfree(CntType);
    xfree(MimeVer);

    return BodyType;
}


/*
 * Decode one line of data containing QP data.
 * Return flag set if this line ends with a soft line-break.
 * 'bufp' is modified to point to the end of the output buffer.
 */
static int DoOneQPLine(char **bufp, flag delimited, flag issoftline)
{
  char *buf = *bufp;
  char *p_in, *p_out, *p;
  int n;
  int ret = 0;

  /*
   * Special case: line consists of a single =2E and messages are 
   * dot-terminated.  Line has to be dot-stuffed after decoding.
   */
  if (delimited && !issoftline && buf[0]=='=' && !strncmp(*bufp, "=2E\r\n", 5))
  {
      strcpy(buf, "..\r\n");
      *bufp += 5;
      return(FALSE);
  }

  p_in = buf;
  if (delimited && issoftline && (strncmp(buf, "..", 2) == 0))
    p_in++;

  for (p_out = buf; (*p_in); ) {
    p = strchr(p_in, '=');
    if (p == NULL) {
      /* No more QP data, just move remainder into place */
      n = strlen(p_in);
      memmove(p_out, p_in, n);
      p_in += n; p_out += n;
    }
    else {
      if (p > p_in) {
	/* There are some uncoded chars at the beginning. */
	n = (p - p_in);
	memmove(p_out, p_in, n);
	p_out += n;
      }
              
      switch (*(p+1)) {
      case '\0': case '\r': case '\n':
	/* Soft line break, skip '=' */
	p_in = p+1; 
	if (*p_in == '\r') p_in++;
	if (*p_in == '\n') p_in++;
        ret = 1;
	break;

      default:
	/* There is a QP encoded byte */
	if (qp_char(*(p+1), *(p+2), p_out) == 0) {
	  p_in = p+3;
	}
	else {
	  /* Invalid QP data - pass through unchanged. */
	  *p_out = '=';
	  p_in = p+1;
	}
	p_out++;
	break;
      }
    }
  }

  *p_out = '\0';
  *bufp = p_out;
  return ret;
}


/* This is called once per line in the message body.  We need to scan
 * all lines in the message body for the multipart delimiter string,
 * and handle any body-part headers in such messages (these can toggle
 * qp-decoding on and off).
 *
 * Note: Messages that are NOT multipart-messages go through this
 * routine quickly, since BodyState will always be S_BODY_DATA,
 * and MultipartDelimiter is NULL.
 *
 * Return flag set if this line ends with a soft line-break.
 * 'bufp' is modified to point to the end of the output buffer.
 */

int UnMimeBodyline(char **bufp, flag delimited, flag softline)
{
  char *buf = *bufp;
  int ret = 0;

  switch (BodyState) {
  case S_BODY_HDR:
    UnMimeHeader(buf);   /* Headers in body-parts can be encoded, too! */
    if ((*buf == '\0') || (*buf == '\n') || (strcmp(buf, "\r\n") == 0)) {
      BodyState = S_BODY_DATA;
    } 
    else if (strncasecmp("Content-Transfer-Encoding:", buf, 26) == 0) {
      char *XferEnc;

      XferEnc = nxtaddr(buf);
      if ((XferEnc != NULL) && (strcasecmp(XferEnc, "quoted-printable") == 0)) {
	CurrEncodingIsQP = 1;

        /*
	 * Hmm ... we cannot be really sure that CurrTypeNeedsDecode
         * has been set - we may not have seen the Content-Type header
         * yet. But *usually* the Content-Type header comes first, so
         * this will work. And there is really no way of doing it 
         * "right" as long as we stick with the line-by-line processing.
	 */
	if (CurrTypeNeedsDecode)
	    SetEncoding8bit(buf);
      }
    }
    else if (strncasecmp("Content-Type:", buf, 13) == 0) {
      CurrTypeNeedsDecode = CheckContentType(nxtaddr(buf));
    }

    *bufp = (buf + strlen(buf));
    break;

  case S_BODY_DATA:
    if ((*MultipartDelimiter) && 
	(strncmp(buf, MultipartDelimiter, strlen(MultipartDelimiter)) == 0)) {
      BodyState = S_BODY_HDR;
      CurrEncodingIsQP = CurrTypeNeedsDecode = 0;
    }

    if (CurrEncodingIsQP && CurrTypeNeedsDecode) 
      ret = DoOneQPLine(bufp, delimited, softline);
    else
     *bufp = (buf + strlen(buf));
    break;
  }

  return ret;
}


#ifdef STANDALONE
#include <stdio.h>
#include <unistd.h>

const char *program_name = "unmime";
int outlevel = 0;

#define BUFSIZE_INCREMENT 4096

#ifdef DEBUG
#define DBG_FWRITE(B,L,BS,FD) do { if (fwrite((B), (L), (BS), (FD))) { } } while(0)
#else
#define DBG_FWRITE(B,L,BS,FD)
#endif

int main(int argc, char *argv[])
{
  unsigned int BufSize;
  char *buffer, *buf_p;
  int nl_count, i, bodytype;

  /* quench warnings about unused arguments */
  (void)argc;
  (void)argv;

#ifdef DEBUG
  pid_t pid;
  FILE *fd_orig, *fd_conv;
  char fnam[100];

  /* we don't need snprintf here, but for consistency, we'll use it */
  pid = getpid();
  snprintf(fnam, sizeof(fnam), "/tmp/i_unmime.%lx", (long)pid);
  fd_orig = fopen(fnam, "w");
  snprintf(fnam, sizeof(fnam), "/tmp/o_unmime.%lx", (long)pid);
  fd_conv = fopen(fnam, "w");
#endif

  BufSize = BUFSIZE_INCREMENT;    /* Initial size of buffer */
  buf_p = buffer = (char *) xmalloc(BufSize);
  nl_count = 0;

  do {
    i = fread(buf_p, 1, 1, stdin);
    switch (*buf_p) {
     case '\n':
       nl_count++;
       break;

     case '\r':
       break;

     default:
       nl_count = 0;
       break;
    }

    buf_p++;
    if ((unsigned)(buf_p - buffer) == BufSize) {
       /* Buffer is full! Get more room. */
       buffer = (char *)xrealloc(buffer, BufSize+BUFSIZE_INCREMENT);
       buf_p = buffer + BufSize;
       BufSize += BUFSIZE_INCREMENT;
    }
  } while ((i > 0) && (nl_count < 2));

  *buf_p = '\0';
  DBG_FWRITE(buffer, strlen(buffer), 1, fd_orig);

  UnMimeHeader(buffer);
  bodytype = MimeBodyType(buffer, 1);

  i = strlen(buffer);
  DBG_FWRITE(buffer, i, 1, fd_conv);
  if (fwrite(buffer, i, 1, stdout) < 1) {
      perror("fwrite");
      goto barf;
  }
  
  do {
     buf_p = (buffer - 1);
     do {
        buf_p++;
        i = fread(buf_p, 1, 1, stdin);
     } while ((i == 1) && (*buf_p != '\n'));
     if (i == 1) buf_p++;
     *buf_p = '\0';
     DBG_FWRITE(buf, (buf_p - buffer), 1, fd_orig);

     if (buf_p > buffer) {
        if (bodytype & MSG_NEEDS_DECODE) {
           buf_p = buffer;
           UnMimeBodyline(&buf_p, 0, 0);
        }
        DBG_FWRITE(buffer, (buf_p - buffer), 1, fd_conv);
        if (fwrite(buffer, (buf_p - buffer), 1, stdout) < 1) {
	    perror("fwrite");
	    goto barf;
	}
     }
  } while (buf_p > buffer);

barf:
  free(buffer);
  if (EOF == fflush(stdout)) perror("fflush");

#ifdef DEBUG
  fclose(fd_orig);
  fclose(fd_conv);
#endif

  return 0;
}
#endif
