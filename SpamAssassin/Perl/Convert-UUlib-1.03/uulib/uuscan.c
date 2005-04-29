/*
 * This file is part of uudeview, the simple and friendly multi-part multi-
 * file uudecoder  program  (c) 1994-2001 by Frank Pilhofer. The author may
 * be contacted at fp@fpx.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * These are very central functions of UUDeview. Here, we scan a file
 * and decide whether it contains encoded data or not. ScanPart() must
 * be called repeatedly on the same file until feof(file). Each time,
 * it returns information about the next part found within.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef SYSTEM_WINDLL
#include <windows.h>
#endif
#ifdef SYSTEM_OS2
#include <os2.h>
#endif

#include <stdio.h>
#include <ctype.h>

#ifdef STDC_HEADERS
#include <stdlib.h>
#include <string.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <uudeview.h>
#include <uuint.h>
#include <fptools.h>
#include <uustring.h>

char * uuscan_id = "$Id: uuscan.c,v 1.1 2004/05/14 15:23:05 dasenbro Exp $";

/*
 * Header fields we recognize as such. See RFC822. We add "From ",
 * the usual marker for a beginning of a new message, and a couple
 * of usual MDA, News and MIME headers.
 * We make a distinction of MIME headers as we need the difference
 * to scan the bodies from partial multipart messages.
 */

static char *knownmsgheaders[] = {
  "From ", "Return-Path:", "Received:", "Reply-To:",
  "From:", "Sender:", "Resent-Reply-To:", "Resent-From:",
  "Resent-Sender:", "Date:", "Resent-Date:", "To:",
  "Resent-To:", "Cc:", "Bcc:", "Resent-bcc:",
  "Message-ID:", "Resent-Message-Id:", "In-Reply-To:",
  "References:", "Keywords:", "Subject:", "Comments:",
  
  "Delivery-Date:", "Posted-Date:", "Received-Date:",
  "Precedence:", 

  "Path:", "Newsgroups:", "Organization:", "Lines:",
  "NNTP-Posting-Host:",
  NULL
};

static char *knownmimeheaders[] = {
  "Mime-Version:",  "Content-Transfer-Encoding:",
  "Content-Type:", "Content-Disposition:", 
  "Content-Description:", "Content-Length:",
  NULL
};

/*
 * for MIME (plaintext) parts without filename
 */
int mimseqno;

/*
 * how many lines do we read when checking for headers
 */
#define WAITHEADER	10

/*
 * The stack for encapsulated Multipart messages
 */
#define MSMAXDEPTH	3

int       mssdepth = 0;
scanstate multistack[MSMAXDEPTH+1];

/*
 * The state and the local envelope
 */
headers   localenv;
scanstate sstate;

/*
 * mallocable areas
 */

char *uuscan_shlline;
char *uuscan_shlline2;
char *uuscan_pvvalue;
char *uuscan_phtext;
char *uuscan_sdline;
char *uuscan_sdbhds1;
char *uuscan_sdbhds2;
char *uuscan_spline;

/*
 * Macro: print cancellation message in UUScanPart
 */

#define SPCANCEL()	{UUMessage(uuscan_id,__LINE__,UUMSG_NOTE,uustring(S_SCAN_CANCEL));*errcode=UURET_CANCEL;goto ScanPartEmergency;}

/*
 * Is line empty? A line is empty if it is composed of whitespace.
 */

static int
IsLineEmpty (char *data)
{
  if (data == NULL) return 0;
  while (*data && isspace (*data)) data++;
  return ((*data)?0:1);
}

/*
 * Is this a header line? A header line has alphanumeric characters
 * followed by a colon.
 */

static int
IsHeaderLine (char *data)
{
  if (data == NULL) return 0;
  if (*data == ':') return 0;
  while (*data && (isalnum (*data) || *data=='-')) data++;
  return (*data == ':') ? 1 : 0;
}

/*
 * Scans a potentially folded header line from the input file. If
 * initial is non-NULL, it is the first line of the header, useful
 * if the calling function just coincidentally noticed that this is
 * a header.
 * RFC0822 does not specify a maximum length for headers, but we
 * truncate everything beyond an insane value of 1024 characters.
 */

static char *
ScanHeaderLine (FILE *datei, char *initial)
{
  char *ptr=uuscan_shlline;
  char *ptr2, *p1, *p2, *p3;
  int llength, c;
  long curpos;
  int hadcr;

  if (initial) {
    _FP_strncpy (uuscan_shlline, initial, 1024);
  }
  else {
    /* read first line */
    if (feof (datei) || ferror (datei))
      return NULL;
    if (_FP_fgets (uuscan_shlline, 1023, datei) == NULL)
      return NULL;
    uuscan_shlline[1023] = '\0';
  }

  llength = strlen (uuscan_shlline);
  hadcr   = 0;

  /* strip whitespace at end */
  ptr = uuscan_shlline + llength;
  while (llength && isspace(*(ptr-1))) {
    if (*(ptr-1) == '\012' || *(ptr-1) == '\015')
      hadcr = 1;
    ptr--; llength--;
  }
  if (llength == 0) {
    uuscan_shlline[0] = '\0';
    return uuscan_shlline;
  }

  while (!feof (datei)) {
    c = fgetc (datei);
    if (feof (datei))
      break;

    /*
     * If the line didn't have a CR, it was longer than 256 characters
     * and is continued anyway.
     */

    if (hadcr==1 && c != ' ' && c != '\t') {
      /* no LWSP-char, header line does not continue */
      ungetc (c, datei);
      break;
    }
    while (!feof (datei) && (c == ' ' || c == '\t'))
      c = fgetc (datei);

    if (!feof (datei))
      ungetc (c, datei);	/* push back for fgets() */

    /* insert a single LWSP */
    if (hadcr==1 && llength < 1023) {
      *ptr++ = ' ';
      llength++;
    }
    *ptr = '\0'; /* make lint happier */

    if (feof (datei))
      break;

    /* read next line */
    curpos = ftell (datei);
    if (_FP_fgets (uugen_inbuffer, 255, datei) == NULL)
      break;
    uugen_inbuffer[255] = '\0';

    if (IsLineEmpty (uugen_inbuffer)) { /* oops */
      fseek (datei, curpos, SEEK_SET);
      break;
    }

    _FP_strncpy (ptr, uugen_inbuffer, 1024-llength);

    /*
     * see if line was terminated with CR. Otherwise, it continues ...
     */
    c = strlen (ptr);
    if (c>0 && (ptr[c-1] == '\012' || ptr[c-1] == '\015'))
      hadcr = 1;
    else
      hadcr = 0;

    /*
     * strip whitespace
     */

    ptr     += c;
    llength += c;
    while (llength && isspace(*(ptr-1))) {
      ptr--; llength--;
    }
  }

  *ptr = '\0';

  if (llength == 0)
    return NULL;

  /*
   * Now that we've read the header line, we can RFC 1522-decode it
   */

  ptr = uuscan_shlline;
  ptr2 = uuscan_shlline2;

  while (*ptr) {
    /*
     * Look for =? magic
     */

    if (*ptr == '=' && *(ptr+1) == '?') {
      /*
       * Let p1 point to the charset, look for next question mark
       */

      p1 = p2 = ptr+2;

      while (*p2 && *p2 != '?') {
	p2++;
      }

      if (*p2 == '?' &&
	  (*(p2+1) == 'q' || *(p2+1) == 'Q' ||
	   *(p2+1) == 'b' || *(p2+1) == 'B') &&
	  *(p2+2) == '?') {
	/*
	 * Let p2 point to the encoding, look for ?= magic
	 */

	p2++;
	p3=p2+2;

	while (*p3 && (*p3 != '?' || *(p3+1) != '=')) {
	  p3++;
	}

	if (*p3 == '?' && *(p3+1) == '=') {
	  /*
	   * Alright, we've found an RFC 1522 header field
	   */
	  if (*p2 == 'q' || *p2 == 'Q') {
	    c = UUDecodeField (p2+2, ptr2, QP_ENCODED);
	  }
	  else if (*p2 == 'b' || *p2 == 'B') {
	    c = UUDecodeField (p2+2, ptr2, B64ENCODED);
	  }
	  if (c >= 0) {
	    ptr2 += c;
	    ptr = p3+2;
	    continue;
	  }
	}
      }
    }

    *ptr2++ = *ptr++;
  }

  *ptr2 = 0;

  return uuscan_shlline2;
}

/*
 * Extract the value from a MIME attribute=value pair. This function
 * receives a pointer to the attribute.
 */
static char *
ParseValue (char *attribute)
{
  char *ptr=uuscan_pvvalue;
  int length=0;

  if (attribute == NULL)
    return NULL;

  while ((isalnum(*attribute) || *attribute=='_') && *attribute != '=')
    attribute++;

  while (isspace(*attribute))
    attribute++;

  if (*attribute == '=') {
    attribute++;
    while (isspace (*attribute))
      attribute++;
  }
  else
    return NULL;

  if (*attribute == '"') {
    /* quoted-string */
    attribute++;
    while (*attribute && *attribute != '"' && length < 255) {
      if (*attribute == '\\'
          && (attribute[1] == '"'
              || attribute[1] == '\015'
              || attribute[1] == '\\'))
        /* we dequote only the three characters that MUST be quoted, since
         * microsoft is obviously unable to correctly implement even mime headers:
         * filename="c:\xxx". *sigh*
         */
	*ptr++ = *++attribute;
      else
	*ptr++ = *attribute;
      attribute++;
      length++;
    }
    *ptr = '\0';
  }
  else {
    /* tspecials from RFC1521 */
    /*
     * Note - exclude '[', ']' and ';' on popular request; these are
     * used in some Content-Type fields by the Klez virus, and people
     * who feed their virus scanners with the output of UUDeview would
     * like to catch it!
     */

    while (*attribute && !isspace (*attribute) &&
	   *attribute != '(' && *attribute != ')' &&
	   *attribute != '<' && *attribute != '>' &&
	   *attribute != '@' && *attribute != ',' &&
	   /* *attribute != ';' && */ *attribute != ':' &&
	   *attribute != '\\' &&*attribute != '"' &&
	   *attribute != '/' && /* *attribute != '[' &&
	   *attribute != ']' && */ *attribute != '?' &&
	   *attribute != '=' && length < 255) {
      *ptr++ = *attribute++;
      length++;
    }

    *ptr = '\0';
  }
  return uuscan_pvvalue;
}

/*
 * Extract the information we need from header fields
 */

static headers *
ParseHeader (headers *theheaders, char *line)
{
  char **variable=NULL;
  char *value, *ptr, *thenew;
  int delimit, length;

  value = 0; delimit = 0; /* calm down gcc */

  if (line == NULL)
    return theheaders;

  if (_FP_strnicmp (line, "From:", 5) == 0) {
    if (theheaders->from) return theheaders;
    variable = &theheaders->from;
    value    = line+5;
    delimit  = 0;
  }
  else if (_FP_strnicmp (line, "Subject:", 8) == 0) {
    if (theheaders->subject) return theheaders;
    variable = &theheaders->subject;
    value    = line+8;
    delimit  = 0;
  }
  else if (_FP_strnicmp (line, "To:", 3) == 0) {
    if (theheaders->rcpt) return theheaders;
    variable = &theheaders->rcpt;
    value    = line+3;
    delimit  = 0;
  }
  else if (_FP_strnicmp (line, "Date:", 5) == 0) {
    if (theheaders->date) return theheaders;
    variable = &theheaders->date;
    value    = line+5;
    delimit  = 0;
  }
  else if (_FP_strnicmp (line, "Mime-Version:", 13) == 0) {
    if (theheaders->mimevers) return theheaders;
    variable = &theheaders->mimevers;
    value    = line+13;
    delimit  = 0;
  }
  else if (_FP_strnicmp (line, "Content-Type:", 13) == 0) {
    if (theheaders->ctype) return theheaders;
    variable = &theheaders->ctype;
    value    = line+13;
    delimit  = ';';

    /* we can probably extract more information */
    if ((ptr = _FP_stristr (line, "boundary")) != NULL) {
      if ((thenew = ParseValue (ptr))) {
	if (theheaders->boundary) free (theheaders->boundary);
	theheaders->boundary = _FP_strdup (thenew);
      }
    }
    if ((ptr = _FP_stristr (line, "name")) != NULL) {
      if ((thenew = ParseValue (ptr))) {
	if (theheaders->fname) free (theheaders->fname);
	theheaders->fname = _FP_strdup (thenew);
      }
    }
    if ((ptr = _FP_stristr (line, "id")) != NULL) {
      if ((thenew = ParseValue (ptr))) {
	if (theheaders->mimeid) free (theheaders->mimeid);
	theheaders->mimeid = _FP_strdup (thenew);
      }
    }
    if ((ptr = _FP_stristr (line, "number")) != NULL) {
      if ((thenew = ParseValue (ptr))) {
	theheaders->partno = atoi (thenew);
      }
    }
    if ((ptr = _FP_stristr (line, "total")) != NULL) {
      if ((thenew = ParseValue (ptr))) {
	theheaders->numparts = atoi (thenew);
      }
    }
  }
  else if (_FP_strnicmp (line, "Content-Transfer-Encoding:", 26) == 0) {
    if (theheaders->ctenc) return theheaders;
    variable = &theheaders->ctenc;
    value    = line+26;
    delimit  = ';';
  }
  else if (_FP_strnicmp (line, "Content-Disposition:", 20) == 0) {
    /*
     * Some encoders mention the original filename as parameter to
     * Content-Type, others as parameter to Content-Disposition. We
     * do prefer the first solution, but accept this situation, too.
     * TODO: Read RFC1806
     */
    if ((ptr = _FP_stristr (line, "name")) != NULL) {
      if (theheaders->fname == NULL && (thenew=ParseValue(ptr)) != NULL) {
	theheaders->fname = _FP_strdup (thenew);
      }
    }
    variable = NULL;
  }
  else {
    /*
     * nothing interesting
     */
    return theheaders;
  }

  /*
   * okay, so extract the actual data
   */
  if (variable) {
    length = 0;
    ptr = uuscan_phtext;

    while (isspace (*value))
      value++;
    while (*value && (delimit==0 || *value!=delimit) &&
	   *value != '\012' && *value != '\015' && length < 255) {
      *ptr++ = *value++;
      length++;
    }
    while (length && isspace(*(ptr-1))) {
      ptr--; length--;
    }
    *ptr = '\0';

    if ((*variable = _FP_strdup (uuscan_phtext)) == NULL)
      return NULL;
  }

  return theheaders;
}

/*
 * is this a header line we know about?
 */

static int
IsKnownHeader (char *line)
{
  char **iter = knownmsgheaders;

  while (iter && *iter) {
    if (_FP_strnicmp (line, *iter, strlen (*iter)) == 0)
      return 1;
    iter++;
  }

  iter = knownmimeheaders;

  while (iter && *iter) {
    if (_FP_strnicmp (line, *iter, strlen (*iter)) == 0)
      return 2;
    iter++;
  }

  return 0;
}

/*
 * Scan a header
 */

int
UUScanHeader (FILE *datei, headers *envelope)
{
  char *ptr;

  while (!feof (datei)) {
    if ((ptr = ScanHeaderLine (datei, NULL)) == NULL)
      break;
    if (*ptr == '\0' || *ptr == '\012' || *ptr == '\015')
      break;
    ParseHeader (envelope, ptr);
  }
  return 0;
}

/*
 * Scan something for encoded data and fill the fileread* structure.
 * If boundary is non-NULL, we stop after having read it. If Check-
 * Headers != 0, we also stop after we've found uu_headercount recog-
 * nized header lines.
 * If we have a boundary, then we also don't accept Base64; MIME mails
 * really should handle this properly.
 * We return -1 if something went wrong, 0 if everything is normal,
 * 1 if we found a new header and 2 if we found a boundary.
 * In MIME message bodies (not multiparts), we also disable our reduced
 * MIME handling.
 */

static int
ScanData (FILE *datei, char *fname, int *errcode,
	  char *boundary, int ismime, int checkheaders,
	  fileread *result)
{
  char *line=uuscan_sdline, *bhds1=uuscan_sdbhds1, *bhds2=uuscan_sdbhds2;
  static char *ptr, *p2, *p3=NULL, *bhdsp, bhl;
  int islen[10], isb64[10], isuue[10], isxxe[10], isbhx[10], iscnt;
  int cbb64, cbuue, cbxxe, cbbhx;
  int bhflag=0, vflag, haddh=0, hadct=0;
  int bhrpc=0, bhnf=0, c, hcount, lcount, blen=0;
  int encoding=0, dflag=0, ctline=42;
  int dontcare=0, hadnl=0;
  long preheaders=0, oldposition;
  long yefilesize=0, yepartends=0;
  size_t dcc, bhopc;

  *errcode = UURET_OK;
  (void) UUDecodeLine (NULL, NULL, 0);          /* init */
  bhdsp = bhds2;

  if (datei == NULL || feof (datei))
    return -1;

  result->startpos = ftell (datei);
  hcount = lcount  = 0;

  for (iscnt=0; iscnt<10; iscnt++) {
    isb64[iscnt] = isuue[iscnt] = isxxe[iscnt] = isbhx[iscnt] = 0;
    islen[iscnt] = -1;
  }

  iscnt = 0;

  if (boundary)
    blen = strlen (boundary);

  while (!feof (datei)) {
    oldposition = ftell (datei);
    if (_FP_fgets (line, 255, datei) == NULL)
      break;
    if (ferror (datei))
      break;

    line[255] = '\0'; /* For Safety of string functions */

    /*
     * Make Busy Polls
     */

    if (UUBUSYPOLL(ftell(datei),progress.fsize)) {
      UUMessage (uuscan_id, __LINE__, UUMSG_NOTE,
		 uustring (S_SCAN_CANCEL));
      *errcode = UURET_CANCEL;
      break;
    }

    if (IsLineEmpty (line)) { /* line empty? */
      hcount = 0;
      hadnl  = 1;
      continue;               /* then ignore */
    }

    if (checkheaders) {
      if (IsKnownHeader (line)) {
	(void) ScanHeaderLine (datei, line);

	if (hcount == 0) {
	  preheaders = oldposition;
	  lcount     = 0;
	}
	hcount++;
	lcount++;

	/*
	 * check for the various restart counters
	 */

	if ((hcount >= hlcount.restart) ||
	    (hcount >= hlcount.afterdata && ismime == 0) ||
	    (hcount >= hlcount.afterdata && result->uudet) ||
	    (hcount >= hlcount.afternl   && result->uudet && hadnl)) {
	  /*
	   * Hey, a new header starts here
	   */
	  fseek (datei, preheaders, SEEK_SET);
	  break;
	}
	/* continue; */
      }
      else if (lcount > WAITHEADER) {
	hcount = 0;
	lcount = 0;
	dontcare=0;
      }
      else if (hcount) {
	lcount++;
	dontcare=1;
      }
      else {
	dontcare=0;
      }
    }
    else {
      dontcare=0;
    }

    if (boundary != NULL && 
	line[0] == '-' && line[1] == '-' &&
	strncmp (line+2, boundary, blen) == 0) {
      fseek (datei, oldposition, SEEK_SET);
      break;
    }
    if (boundary != NULL && line[0] == 'C' && line[1] == 'o' &&
	_FP_strnicmp (line, "Content-Type:", 13) == 0) {
      ptr = ScanHeaderLine (datei, line);
      p2  = (ptr)?_FP_stristr(ptr,"boundary"):NULL;
      p3  = (p2)?ParseValue(p2):NULL;

      if (p3 && strcmp (p3, boundary) == 0) {
	fseek (datei, oldposition, SEEK_SET);
	break;
      }
      else {
	p3 = NULL;
      }
    }

    if (strncmp      (line, "begin ",       6) == 0 ||
	_FP_strnicmp (line, "<pre>begin ", 11) == 0) {
      if ((result->begin || result->end ||
	   result->uudet == B64ENCODED ||
	   result->uudet == BH_ENCODED) && !uu_more_mime) {
	fseek (datei, oldposition, SEEK_SET);
	break;
      }
      
      if (*line == '<')
	ptr = line + 10;
      else
	ptr = line + 5;

      while (*ptr == ' ') ptr++;
      while (isdigit (*ptr)) 
	result->mode = result->mode * 8 + *ptr++ - '0';
      while (*ptr == ' ') ptr++;

      /*
       * We may have picked up a filename from a uuenview-style header
       */
      _FP_free (result->filename);
      result->filename = _FP_strdup (ptr);
      result->begin    = 1;

      while (isspace (result->filename[strlen(result->filename)-1]))
	result->filename[strlen(result->filename)-1] = '\0';

      continue;
    }

    if ((strncmp (line, "end", 3) == 0) &&
	result->uudet != BH_ENCODED &&
	result->uudet != YENC_ENCODED) {
      if (result->uudet == B64ENCODED && result->begin)
	result->uudet = XX_ENCODED;

      if (result->uudet != B64ENCODED) {
	result->end = 1;
	if (dflag && encoding)
	  result->uudet = encoding;
	continue;
      }
    }

    hadnl = 0;

    /*
     * Detect a UUDeview-Style header
     */

    if (_FP_strnicmp (line, "_=_ Part ", 9) == 0 &&
	result->uudet != YENC_ENCODED) {
      if (result->uudet) {
	fseek (datei, oldposition, SEEK_SET);
	break;
      }
      result->partno = atoi (line + 8);
      if ((ptr = _FP_stristr (line, "of file ")) != NULL) {
	ptr += 8;
	while (isspace (*ptr)) ptr++;
	p2 = ptr;
	while (isalnum(*p2) || 
	       *p2 == '.' || *p2=='_' || *p2 == '-' ||
	       *p2 == '!' || *p2=='@' || *p2 == '$')
	  p2++;
	c = *p2; *p2 = '\0';
	if (p2 != ptr && result->filename == NULL)
	  result->filename = _FP_strdup (ptr);
	else if (p2 - ptr > 5 && strchr (ptr, '.') != NULL) {
	  /*
	   * This file name looks good, too. Let's use it
	   */
	  _FP_free (result->filename);
	  result->filename = _FP_strdup (ptr);
	}
	*p2 = c;
      }
    }

    /*
     * Some reduced MIME handling. Only use if boundary == NULL. Also
     * accept the "X-Orcl-Content-Type" used by some braindead program.
     */
    if (boundary == NULL && !ismime && !uu_more_mime &&
	result->uudet != YENC_ENCODED) {
      if (_FP_strnicmp (line, "Content-Type", 12) == 0 ||
	  _FP_strnicmp (line, "X-Orcl-Content-Type", 19) == 0) {
	/*
	 * We use Content-Type to mark a new attachment and split the file.
	 * However, we do not split if we haven't found anything encoded yet.
	 */
	if (result->uudet) {
	  fseek (datei, oldposition, SEEK_SET);
	  break;
	}
	if ((ptr = strchr (line, ':')) != NULL) {
	  ptr++;
	  while (isspace (*ptr)) ptr++; p2 = ptr;
	  while (!isspace (*p2) && *p2 != ';') p2++;
	  c = *p2; *p2 = '\0';
	  if (p2 != ptr) {
	    _FP_free (result->mimetype);
	    result->mimetype = _FP_strdup (ptr);
	  }
	  *p2 = c;
	}
	ctline=0;
	hadct=1;
      }
      if ((ptr = _FP_stristr (line, "number=")) && ctline<4) {
	ptr += 7; if (*ptr == '"') ptr++;
	result->partno = atoi (ptr);
      }
      if ((ptr = _FP_stristr (line, "total=")) && ctline<4) {
	ptr += 6; if (*ptr == '"') ptr++;
	result->maxpno = atoi (ptr);
      }
      if ((ptr = _FP_stristr (line, "name=")) && ctline<4) {
	ptr += 5;
	while (isspace (*ptr)) ptr++;
	if (*ptr == '"' && *(ptr+1) && (p2 = strchr (ptr+2, '"')) != NULL) {
	  c = *p2; *p2 = '\0';
	  _FP_free (result->filename);
	  result->filename = _FP_strdup (ptr+1);
	  *p2 = c;
	}
	else if (*ptr=='\''&&*(ptr+1)&&(p2 = strchr(ptr+2, '\'')) != NULL) {
	  c = *p2; *p2 = '\0';
	  _FP_free (result->filename);
	  result->filename = _FP_strdup (ptr+1);
	  *p2 = c;
	}
	else {
	  p2 = ptr;
	  while (isalnum(*p2) || 
		 *p2 == '.' || *p2=='_' || *p2 == '-' ||
		 *p2 == '!' || *p2=='@' || *p2 == '$')
	    p2++;
	  c = *p2; *p2 = '\0';
	  if (p2 != ptr && result->filename == NULL)
	    result->filename = _FP_strdup (ptr);
	  else if (p2 - ptr > 5 && strchr (ptr, '.') != NULL) {
	    /*
	     * This file name looks good, too. Let's use it
	     */
	    _FP_free (result->filename);
	    result->filename = _FP_strdup (ptr);
	  }
	  *p2 = c;
	}
      }
      if ((ptr = _FP_stristr (line, "id=")) && ctline<4) {
	p2 = ptr += 3;
	if (*p2 == '"') {
	  p2 = strchr (++ptr, '"');
	}
	else {
	  while (*p2 && isprint(*p2) && !isspace(*p2) && *p2 != ';')
	    p2++;
	}
	if (p2 && *p2 && p2!=ptr) {
	  c = *p2; *p2 = '\0';
	  if (result->mimeid)
	    _FP_free (result->mimeid);
	  result->mimeid = _FP_strdup (ptr);
	  *p2 = c;
	}
      }
      
      /* 
       * Handling for very short Base64 files.
       */
      if (uu_tinyb64 && !ismime && !uu_more_mime) {
	if (line[0] == '-' && line[1] == '-') {
	  if (dflag && (encoding==B64ENCODED || result->uudet==B64ENCODED)) {
	    if (encoding==B64ENCODED && result->uudet==0 && (haddh||hadct)) {
	      result->uudet = encoding;
	      encoding = dflag = 0;
	    }
	    haddh = 1;
	    continue;
	  }
	  hadct = 0;
	}
      }
    } /* end of reduced MIME handling */

    /*
     * If we're in "freestyle" mode, have not encountered anything
     * interesting yet, and stumble upon something that looks like
     * a boundary, followed by a Content-* line, try to use it.
     */

    if (boundary == NULL && !ismime && !uu_more_mime && dflag <= 1 &&
	line[0] == '-' && line[1] == '-' && strlen(line+2)>10 &&
	(((ptr = _FP_strrstr (line+2, "--")) == NULL) ||
	 (*(ptr+2) != '\012' && *(ptr+2) != '\015')) &&
	_FP_strstr (line+2, "_=_") != NULL) {
      if (_FP_fgets (line, 255, datei) == NULL) {
	break;
      }
      if (_FP_strnicmp (line, "Content-", 8) == 0) {
	/*
	 * Okay, let's do it. This breaks out of ScanData. ScanPart will
	 * recognize the boundary on the next call and use it.
	 */
	fseek (datei, oldposition, SEEK_SET);
	break;
      }
    }

    /*
     * Detection for yEnc encoding
     */

    if (strncmp (line, "=ybegin ", 8) == 0 &&
	_FP_strstr (line, " name=") != NULL) {
      if ((result->begin || result->end || result->uudet) && !uu_more_mime) {
	fseek (datei, oldposition, SEEK_SET);
	break;
      }

      /*
       * name continues to the end of the line
       */
      
      _FP_free (result->filename);
      ptr = _FP_strstr (line, " name=") + 6;
      result->filename = _FP_strdup (ptr);

      while (isspace (result->filename[strlen(result->filename)-1]))
	result->filename[strlen(result->filename)-1] = '\0';

      /*
       * Determine size
       */

      if ((ptr = _FP_strstr (line, " size=")) != NULL) {
	ptr += 6;
	yefilesize = atoi (ptr);
      }
      else {
	yefilesize = -1;
      }

      /*
       * check for multipart file and read =ypart line
       */

      if ((ptr = _FP_strstr (line, " part=")) != NULL) {
	result->partno = atoi (ptr + 6);

	if (result->partno == 1) {
	  result->begin = 1;
	}

	if (_FP_fgets (line, 255, datei) == NULL) {
	  break;
	}

	line[255] = '\0';

	if (strncmp (line, "=ypart ", 7) != 0) {
	  break;
	}

	if ((ptr = _FP_strstr (line, " end=")) == NULL) {
	  break;
	}
       
	yepartends = atoi (ptr + 5);
      }
      else {
	result->partno = 1;
	result->begin = 1;
      }

      /*
       * Don't want auto-detection
       */

      result->uudet = YENC_ENCODED;
      continue;
    }

    if (strncmp (line, "=yend ", 6) == 0 &&
	result->uudet == YENC_ENCODED) {
      if (yepartends == 0 || yepartends >= yefilesize) {
	result->end = 1;
      }
#if 0
      if (!uu_more_mime)
	break;
#endif
      continue;
    }

    /*
     * if we haven't yet found anything encoded, try to find something
     */

    if (!(result->uudet)) {
      /*
       * Netscape-Repair code is the same as in uunconc.c
       */

      if ((vflag = UUValidData (line, 0, &bhflag)) == 0 && !ismime)
	vflag = UURepairData (datei, line, 0, &bhflag);

      /*
       * Check data against all possible encodings
       */

      islen[iscnt%10] = strlen(line);
      isb64[iscnt%10] = (UUValidData (line, B64ENCODED, &bhflag)==B64ENCODED);
      isuue[iscnt%10] = (UUValidData (line, UU_ENCODED, &bhflag)==UU_ENCODED);
      isxxe[iscnt%10] = (UUValidData (line, XX_ENCODED, &bhflag)==XX_ENCODED);
      isbhx[iscnt%10] = (UUValidData (line, BH_ENCODED, &bhflag)==BH_ENCODED);

      /*
       * If we've got a first valid encoded line, we get suspicious if
       * it's shorter than, say, 40 characters.
       */

      if (vflag == B64ENCODED &&
	  (dflag == 0 || encoding != B64ENCODED) &&
	  strlen (line) < 40 && !result->begin && !uu_tinyb64) {
	isb64[iscnt%10] = 0;
	vflag = 0;
      }

      if ((vflag == UU_ENCODED || vflag == XX_ENCODED) &&
	      (dflag == 0 || encoding != vflag) &&
	      strlen (line) < 40 && !result->begin) {
	isuue[iscnt%10] = isxxe[iscnt%10] = 0;
	vflag = 0;
      }

      iscnt++;

      /*
       * Ah, so we got an encoded line? How interesting!
       */

      if (vflag) {
	/*
	 * For BinHex data, we can use the initial colon ':' as begin
	 * and the terminating colon as ':'.
	 * If (vflag && !bhflag), this is the last line,
	 */
	if (vflag == BH_ENCODED) {
	  if (line[0] == ':' && result->end) {
	    fseek (datei, oldposition, SEEK_SET);
	    break;
	  }
	  if (line[0] == ':')
	    result->begin = 1;
	  if (bhflag == 0) {
	    result->uudet = BH_ENCODED;
	    result->end   = 1;
	  }
	}
	/*
	 * For BinHex files, the file name is encoded in the first encoded
	 * data bytes. We try to extract it here
	 */
	if (vflag == BH_ENCODED && bhnf == 0 && result->filename == NULL) {
	  if (bhdsp == bhds2 ||
	      ((bhdsp-bhds2) <= (int) bhds2[0] &&
	       (bhdsp-bhds2) <  256)) { 
	    dcc = UUDecodeLine (line, bhds1, BH_ENCODED);
	    UUbhdecomp (bhds1, bhdsp, &bhl, &bhrpc,
			dcc, 256-(bhdsp-bhds2), &bhopc);
	    bhdsp += bhopc;
	  }
	  if ((bhdsp-bhds2) > (int) bhds2[0] && bhds2[0]>0 &&
	      result->filename==NULL) {
	    memcpy (bhds1, bhds2+1, (int) bhds2[0]);
	    bhds1[(int)bhds2[0]]='\0';
	    result->filename = _FP_strdup (bhds1);
	    bhnf             = 1;
	  }
	  else if (bhdsp-bhds2 >= 256 && bhds2[0]>0) {
	    memcpy (bhds1, bhds2+1, 255);
	    bhds1[255]       = '\0';
	    result->filename = _FP_strdup (bhds1);
	    bhnf             = 1;
	  }
	  else if (bhds2[0] <= 0)
	    bhnf = 1;
	}

	/*
	 * We accept an encoding if it has been true for four consecutive
	 * lines. Check the is<enc> arrays to avoid mistaking one encoding
	 * for the other. Uuencoded data is rather easily mistaken for
	 * Base 64. If the data matches more than one encoding, we need to
	 * scan further.
	 *
	 * Since text can also rather easily be mistaken for UUencoded
	 * data if it just happens to have 4 lines in a row that have the
	 * correct first character for the length of the line, we also add
	 * a check that the first 3 lines must be the same length, and the
	 * 4th line must be less than or equal to that length. (since
	 * uuencoders use the same length for all lines except the last,
	 * this shouldn't increase the minimum size of UUdata we can
	 * detect, as it would if we tested all 4 lines for being the same
	 * length.)  - Matthew Mueller, 20030109
	 */

	if (iscnt > 3) {
	  cbb64 = (isb64[(iscnt-1)%10] && isb64[(iscnt-2)%10] &&
		   isb64[(iscnt-3)%10] && isb64[(iscnt-4)%10]);
	  cbuue = (isuue[(iscnt-1)%10] && isuue[(iscnt-2)%10] &&
		   isuue[(iscnt-3)%10] && isuue[(iscnt-4)%10] &&
		   islen[(iscnt-1)%10] <= islen[(iscnt-2)%10] &&
		   islen[(iscnt-2)%10] == islen[(iscnt-3)%10] &&
		   islen[(iscnt-3)%10] == islen[(iscnt-4)%10]);
	  cbxxe = (isxxe[(iscnt-1)%10] && isxxe[(iscnt-2)%10] &&
		   isxxe[(iscnt-3)%10] && isxxe[(iscnt-4)%10] &&
		   islen[(iscnt-1)%10] <= islen[(iscnt-2)%10] &&
		   islen[(iscnt-2)%10] == islen[(iscnt-3)%10] &&
		   islen[(iscnt-3)%10] == islen[(iscnt-4)%10]);
	  cbbhx = (isbhx[(iscnt-1)%10] && isbhx[(iscnt-2)%10] &&
		   isbhx[(iscnt-3)%10] && isbhx[(iscnt-4)%10]);
	}
	else {
	  cbb64 = cbuue = cbxxe = cbbhx = 0;
	}

	if (cbb64 && !cbuue && !cbxxe && !cbbhx) {
	  result->uudet = B64ENCODED;
	}
	else if (!cbb64 && cbuue && !cbxxe && !cbbhx) {
	  result->uudet = UU_ENCODED;
	}
	else if (!cbb64 && !cbuue && cbxxe && !cbbhx) {
	  result->uudet = XX_ENCODED;
	}
	else if (!cbb64 && !cbuue && !cbxxe && cbbhx) {
	  result->uudet = BH_ENCODED;
	}

	if (result->uudet) {
          encoding = dflag = 0;

	  /*
	   * If we're in fast mode, we're told we don't have to look
	   * for anything below, so we can as well break out of every-
	   * thing
	   * We cannot fast-scan if we have a boundary to look after.
	   */

	  if (uu_fast_scanning && boundary == NULL)
	    break;

	  /*
	   * Skip the encoded data. We simply wait for a boundary, for
	   * a header or for an empty line. But we also try to pick up
	   * an "end"
	   */

	  hcount = lcount = 0;

	  while (!feof (datei)) {
	    /*
	     * Make Busy Polls
	     */
	    if (UUBUSYPOLL(ftell(datei),progress.fsize)) {
	      UUMessage (uuscan_id, __LINE__, UUMSG_NOTE,
			 uustring (S_SCAN_CANCEL));
	      *errcode = UURET_CANCEL;
	      break;
	    }

	    oldposition = ftell (datei);
	    if (_FP_fgets (line, 255, datei) == NULL)
	      break;
	    if (ferror (datei))
	      break;

	    line[255] = '\0';

	    /*
	     * Stop scanning at an empty line or a MIME-boundary.
	     */
	    if (IsLineEmpty (line))
	      break;
	    if (boundary && line[0] == '-' && line[1] == '-' &&
		strncmp (line+2, boundary, blen) == 0) {
	      fseek (datei, oldposition, SEEK_SET);
	      break;
	    }
	    else if (line[0] == 'e' && (result->uudet == UU_ENCODED ||
					result->uudet == XX_ENCODED)) {
	      if (strncmp (line, "end", 3) == 0) {
		result->end = 1;
		break;
	      }
	    }
	    else if (line[0] == 'b') {
	      if (strncmp (line, "begin ", 6) == 0) {
		fseek (datei, oldposition, SEEK_SET);
		break;
	      }
	    }

	    if (checkheaders) {
	      if (IsKnownHeader (line)) {
		(void) ScanHeaderLine (datei, line);
		if (hcount == 0)
		  preheaders = oldposition;
		hcount++;
		lcount++;
		if ((hcount >= hlcount.restart) ||
		    (hcount >= hlcount.afterdata && result->uudet)) {
		  /*
		   * Hey, a new header starts here
		   */
		  fseek (datei, preheaders, SEEK_SET);
		  break;
		}
	      }
	      else if (lcount > WAITHEADER) {
		hcount = 0;
		lcount = 0;
	      }
	      else if (hcount) {
		lcount++;
	      }
	    }
	    if (result->uudet == BH_ENCODED) {
	      /* pick up ``EOF'' for BinHex files. Slow :-< */
	      if (line[0] && strchr (line+1, ':') != NULL) {
		result->end = 1;
		bhflag      = 0;
		break;
	      }
	    }
	  }

	  if (ferror (datei) || *errcode == UURET_CANCEL)
	    break;

	  if (line[0] == '-' && line[1] == '-')
	    haddh = 1;

	  /*
	   * Okay, got everything we need. If we had headers or a
	   * boundary, we break out of the outer loop immediately.
	   */

	  if (IsKnownHeader (line) ||
	      (boundary && line[0] == '-' && line[1] == '-' &&
	       strncmp (line+2, boundary, blen) == 0)) {
	    break;
	  }

	  /*
	   * Otherwise, we wait until finding something more interesting
	   * in the outer loop
	   */

	  continue;
	}
	
	/*
	 * Select the encoding with the best "history"
	 */

	cbb64 = isb64[(iscnt-1)%10];
	cbuue = isuue[(iscnt-1)%10];
	cbxxe = isxxe[(iscnt-1)%10];
	cbbhx = isbhx[(iscnt-1)%10];
	dflag = 0;

	if (cbb64 || cbuue || cbxxe || cbbhx) {
	  for (dflag=2; dflag<iscnt && dflag<4; dflag++) {
	    if ((!cbb64 || !isb64[(iscnt-dflag)%10]) &&
		(!cbuue || !isuue[(iscnt-dflag)%10]) &&
		(!cbxxe || !isxxe[(iscnt-dflag)%10]) &&
		(!cbbhx || !isbhx[(iscnt-dflag)%10])) {
	      dflag--;
	      break;
	    }
	    cbb64 &= isb64[(iscnt-dflag)%10];
	    cbuue &= isuue[(iscnt-dflag)%10];
	    cbxxe &= isxxe[(iscnt-dflag)%10];
	    cbbhx &= isbhx[(iscnt-dflag)%10];
	  }
	}

	/*
	 * clear-cut cases
	 */

	if (cbb64 && !cbuue && !cbxxe && !cbbhx) {
	  encoding = B64ENCODED;
	}
	else if (!cbb64 && cbuue && !cbxxe && !cbbhx) {
	  encoding = UU_ENCODED;
	}
	else if (!cbb64 && !cbuue && cbxxe && !cbbhx) {
	  encoding = XX_ENCODED;
	}
	else if (!cbb64 && !cbuue && !cbxxe && cbbhx) {
	  encoding = BH_ENCODED;
	}
	else {
	  encoding = 0;
	}

	/*
	 * Check a few common non-clear-cut cases
	 */

	if (!encoding && cbuue && result->begin) {
	  encoding = UU_ENCODED;
	}
	else if (!encoding && cbxxe && result->begin) {
	  encoding = XX_ENCODED;
	}
	else if (!encoding && cbb64) {
	  encoding = B64ENCODED;
	}
	else if (!encoding && cbuue) {
	  encoding = UU_ENCODED;
	}
	else if (!encoding && cbxxe) {
	  encoding = XX_ENCODED;
	}
	else if (!encoding && cbbhx) {
	  encoding = BH_ENCODED;
	}
      }
      else if (!dontcare) {
	encoding = 0;
        dflag = 0;
	haddh = 0;
      }
    } /* if (!uudet) */
    /*
     * End of scanning loop
     */
  } /* while (!feof (datei)) */

  if (feof (datei))
    oldposition = ftell (datei);

  if (dflag && encoding == B64ENCODED && haddh)
    result->uudet = B64ENCODED;
  else if (dflag && encoding == BH_ENCODED)
    result->uudet = BH_ENCODED;

  /* Base64 doesn't have begin or end, so it was probably XX */
  if (result->uudet == B64ENCODED && result->begin && result->end)
    result->uudet = XX_ENCODED;

  /* Base64 doesn't have begin or end */
  if (result->uudet == B64ENCODED)
    result->begin = result->end = 0;

  /* Base64 and BinHex don't have a file mode */
  if (result->uudet == B64ENCODED || result->uudet == BH_ENCODED ||
      result->uudet == YENC_ENCODED)
    result->mode  = 6*64+4*8+4;

  /*
   * When strict MIME adherance is set, throw out suspicious attachments
   */

  if (uu_more_mime) {
    /*
     * In a MIME message, Base64 should be appropriately tagged
     */

    if (result->uudet == B64ENCODED) {
      result->uudet = 0;
    }

    /*
     * Do not accept incomplete UU or XX encoded messages
     */

    if ((result->uudet != 0 && result->uudet != B64ENCODED) &&
	(!result->begin || !result->end)) {
      result->uudet = 0;
    }
  }

  /*
   * In fast mode, this length will yield a false value. We don't care.
   * This must be checked for in uunconc(), where we usually stop decoding
   * after reaching startpos+length
   */

  if (uu_fast_scanning)
    result->length = progress.fsize-result->startpos;
  else
    result->length = ftell(datei)-result->startpos;

  if (ferror (datei)) {
    *errcode = UURET_IOERR;
    uu_errno = errno;
    return -1;
  }
  if (*errcode != UURET_OK) {
    return -1;
  }

  if (boundary && line[0] == '-' && line[1] == '-' &&
      strncmp (line+2, boundary, blen) == 0)
    return 2;
  else if (boundary && p3 &&
	   line[0] == 'C' && line[1] == 'o' &&
	   _FP_strnicmp (line, "Content-Type:", 13) == 0 &&
	   strcmp (p3, boundary) == 0)
    return 2;
  else if (IsKnownHeader (line))
    return 1;

  return 0;
}

/*
 * This is the main scanning function.
 */

fileread *
ScanPart (FILE *datei, char *fname, int *errcode)
{
  int ecount, hcount, lcount;
  int bhflag, begflag, vflag, blen=0, res;
  long preheaders, prevpos=0, preenc, before;
  char *line=uuscan_spline;
  fileread *result;
  char *ptr1, *ptr2;

  (void) UUDecodeLine (NULL, NULL, 0);          /* init */
  if (datei == NULL || feof (datei)) {
    *errcode = UURET_OK;
    return NULL;
  }

  *errcode = UURET_OK;

  if ((result = (fileread *) malloc (sizeof (fileread))) == NULL) {
    *errcode = UURET_NOMEM;
    return NULL;
  }
  memset (result, 0, sizeof (fileread));
  result->startpos = ftell (datei);
  preheaders       = result->startpos;
  before           = result->startpos;

  /* if this is a new file, reset our scanning state */
  if (sstate.source == NULL || strcmp (fname, sstate.source) != 0) {
    sstate.isfolder  = 1;		/* assume yes            */
    sstate.ismime    = 0;		/* wait for MIME-Version */
    sstate.mimestate = MS_HEADERS;	/* assume headers follow */
    /* mimseqno      = 1; */

    while (mssdepth) {
      mssdepth--;
      UUkillheaders (&(multistack[mssdepth].envelope));
      _FP_free (multistack[mssdepth].source);
    }

    UUkillheaders (&sstate.envelope);
    memset (&sstate.envelope, 0, sizeof (headers));

    _FP_free (sstate.source);
    if ((sstate.source = _FP_strdup (fname)) == NULL) {
      *errcode = UURET_NOMEM;
      _FP_free (result);
      return NULL;
    }

    /* ignore empty lines at the beginning of a file */
    preheaders = ftell (datei);
    while (!feof (datei)) {
      if (UUBUSYPOLL(ftell(datei),progress.fsize)) SPCANCEL();
      if (_FP_fgets (line, 255, datei) == NULL)
	break;
      line[255] = '\0';
      if (!IsLineEmpty (line)) {
	fseek (datei, preheaders, SEEK_SET);
	break;
      }
      preheaders = ftell (datei);
    }
  }

  if (ferror(datei) || feof(datei)) {
    _FP_free (result);
    return NULL;
  }

  /*
   * If we are confident that this is a mail folder and are at the
   * beginning of the file, expecting to read some headers, scan
   * the envelope.
   */

  if (sstate.isfolder && sstate.mimestate == MS_HEADERS) {
    hcount = 0;
    lcount = 0;
    UUkillheaders (&sstate.envelope);

    /*
     * clean up leftovers from invalid messages
     */

    while (mssdepth) {
      mssdepth--;
      UUkillheaders (&(multistack[mssdepth].envelope));
      _FP_free (multistack[mssdepth].source);
    }

    prevpos = ftell (datei);
    if (_FP_fgets (line, 255, datei) == NULL) {
      _FP_free (result);
      return NULL;
    }
    line[255] = '\0';

    /*
     * Special handling for AOL folder files, which start off with a boundary.
     * We recognize them by a valid boundary line as the first line of a file.
     * Note that the rest of the scanning code becomes suspicious if a boun-
     * dary does never appear in a file -- this should save us from grave
     * false detection errors
     */

    if (!feof (datei) && line[0] == '-' && line[1] == '-' && line[2]) {
      while (line[strlen(line)-1] == '\012' ||
	     line[strlen(line)-1] == '\015') {
	line[strlen(line)-1] = '\0';
      }

      sstate.ismime            = 1;
      sstate.envelope.mimevers = _FP_strdup ("1.0");
      sstate.envelope.boundary = _FP_strdup (line+2);
      sstate.envelope.ctype    = _FP_strdup ("multipart/mixed");
      sstate.mimestate         = MS_SUBPART;

      *errcode = UURET_CONT;
      _FP_free (result);
      return NULL;
    }

    /*
     * Normal behavior: look for a RFC 822 header
     */

    while (!feof (datei) && !IsLineEmpty (line)) {
      if (IsKnownHeader (line))
	hcount++;
      if (UUBUSYPOLL(ftell(datei),progress.fsize)) SPCANCEL();
      if (IsHeaderLine (line)) {
	ptr1 = ScanHeaderLine (datei, line);
	if (ParseHeader (&sstate.envelope, ptr1) == NULL) {
	  *errcode = UURET_NOMEM;
	  _FP_free (result);
	  return NULL;
	}
      }
      /*
       * if we've read too many lines without finding headers, then
       * this probably isn't a mail folder after all
       */
      lcount++;
      if (lcount > WAITHEADER && hcount < hlcount.afternl) {
	fseek (datei, prevpos, SEEK_SET);
	line[0] = '\0';
	break;
      }

      if (_FP_fgets (line, 255, datei) == NULL) {
        /* If we are at eof without finding headers, there probably isn't */
        if (hcount < hlcount.afternl) {
          fseek (datei, prevpos, SEEK_SET);
          line[0] = '\0';
        }
        break;
      }
      line[255] = '\0';
    }

    /* skip empty lines */
    prevpos = ftell (datei);
    if (IsLineEmpty (line)) {
      while (!feof (datei)) {
	if (_FP_fgets (line, 255, datei) == NULL)
	  break;
	if (UUBUSYPOLL(ftell(datei),progress.fsize)) SPCANCEL();
	if (!IsLineEmpty (line)) {
	  fseek (datei, prevpos, SEEK_SET);
	  line[255] = '\0';
	  break;
	}
	prevpos = ftell (datei);
      }
    }

    /*
     * If we don't have all valid MIME headers yet, but the following
     * line is a MIME header, accept it anyway.
     */

    if (!uu_more_mime &&
	sstate.envelope.mimevers == NULL &&
	sstate.envelope.ctype    == NULL &&
	sstate.envelope.ctenc    == NULL &&
	IsKnownHeader (line)) {
      /*
       * see above
       */
      if (_FP_fgets (line, 255, datei) == NULL) {
	line[0] = '\012';
	line[1] = '\0';
      }
      line[255] = '\0';

      while (!feof (datei) && !IsLineEmpty (line)) {
	if (IsKnownHeader (line))
	  hcount++;
	if (UUBUSYPOLL(ftell(datei),progress.fsize)) SPCANCEL();
	ptr1 = ScanHeaderLine (datei, line);
	if (ParseHeader (&sstate.envelope, ptr1) == NULL) {
	  *errcode = UURET_NOMEM;
	  _FP_free (result);
	  return NULL;
	}

	if (_FP_fgets (line, 255, datei) == NULL)
	  break;
	line[255] = '\0';
      }
      /* skip empty lines */
      prevpos = ftell (datei);
      while (!feof (datei)) {
	if (_FP_fgets (line, 255, datei) == NULL)
	  break;
	if (UUBUSYPOLL(ftell(datei),progress.fsize)) SPCANCEL();
	if (!IsLineEmpty (line)) {
	  fseek (datei, prevpos, SEEK_SET);
	  line[255] = '\0';
	  break;
	}
	prevpos = ftell (datei);
      }
    }

    /*
     * A partial multipart message probably has only a Content-Type
     * header but nothing else. In this case, at least simulate a
     * MIME message
     * if mimevers is not set but there are other well-known MIME
     * headers, don't be too picky about it.
     */
    if (sstate.envelope.ctype && sstate.envelope.mimevers==NULL  &&
	_FP_stristr (sstate.envelope.ctype, "multipart") != NULL &&
	sstate.envelope.boundary != NULL) {
      sstate.envelope.mimevers = _FP_strdup ("1.0");
      hcount = hlcount.afternl;
    }
    else if (sstate.envelope.mimevers==NULL && sstate.envelope.ctype &&
	     sstate.envelope.fname && sstate.envelope.ctenc) {
      sstate.envelope.mimevers = _FP_strdup ("1.0");
      hcount = hlcount.afternl;
    }

    if (sstate.envelope.mimevers != NULL) {
      /* this is a MIME file. check the Content-Type */
      sstate.ismime = 1;
      if (_FP_stristr (sstate.envelope.ctype, "multipart") != NULL) {
	if (sstate.envelope.boundary == NULL) {
	  UUMessage (uuscan_id, __LINE__, UUMSG_WARNING,
		     uustring (S_MIME_NO_BOUNDARY));
	  sstate.mimestate = MS_BODY;
	  _FP_free (sstate.envelope.ctype);
	  sstate.envelope.ctype = _FP_strdup ("text/plain");
	}
	else {
	  sstate.mimestate = MS_PREAMBLE;
	}
      }
      else {
	sstate.mimestate = MS_BODY;	/* just a `simple' message */
      }
    }
    else {
      /* not a folder after all */
      fseek (datei, prevpos, SEEK_SET);
      sstate.isfolder = 0;
      sstate.ismime   = 0;
    }
  }

  if (feof (datei) || ferror (datei)) { /* oops */
    _FP_free (result);
    return NULL;
  }

  /*
   * Handle MIME stuff
   */

  /*
   * Read Preamble. This must be ended by a sstate.envelope.boundary line.
   * If uu_usepreamble is set, we produce a result from this one
   */

  if (sstate.ismime && sstate.mimestate == MS_PREAMBLE) {
    result->startpos = ftell (datei);	/* remember start of preamble */
    prevpos          = ftell (datei);
    preheaders       = ftell (datei);

    blen   = strlen (sstate.envelope.boundary);
    lcount = 0;
    
    while (!feof (datei)) {
      if (_FP_fgets (line, 255, datei) == NULL) {
	line[0] = '\0';
	break;
      }
      if (UUBUSYPOLL(ftell(datei),progress.fsize)) SPCANCEL();
      if (line[0] == '-' && line[1] == '-' &&
	  strncmp (line+2, sstate.envelope.boundary, blen) == 0)
	break;
      if (!IsLineEmpty (line))
	lcount++;

      prevpos = ftell (datei);
    }
    if (feof (datei) || ferror (datei)) {
      UUMessage (uuscan_id, __LINE__, UUMSG_WARNING,
		 uustring (S_MIME_B_NOT_FOUND));
      /*
       * restart and try again; don't restart if uu_fast_scanning
       */
      sstate.isfolder  = 0;
      sstate.ismime    = 0;
      sstate.mimestate = MS_BODY;

      if (!uu_fast_scanning) {
	*errcode = UURET_CONT;
	fseek (datei, preheaders, SEEK_SET);
      }
      _FP_free (result);
      return NULL;
    }
    if (line[0] == '-' && line[1] == '-' &&
	strncmp (line+2, sstate.envelope.boundary, blen) == 0) {
      ptr1 = line + 2 + blen;
      if (*ptr1 == '-' && *(ptr1+1) == '-') {
	/* Empty Multipart Message. Duh. */
	sstate.mimestate = MS_EPILOGUE;
      }
      else {
	sstate.mimestate = MS_SUBPART;
      }
    }
    else { /* shouldn't happen */
      UUMessage (uuscan_id, __LINE__, UUMSG_WARNING,
		 uustring (S_MIME_B_NOT_FOUND));
      /*
       * restart and try again; don't restart if uu_fast_scanning
       */
      sstate.isfolder  = 0;
      sstate.ismime    = 0;
      sstate.mimestate = MS_BODY;

      if (!uu_fast_scanning) {
	*errcode = UURET_CONT;
	fseek (datei, preheaders, SEEK_SET);
      }
      _FP_free (result);
      return NULL;
    }
    /* produce result if uu_usepreamble is set */
    if (uu_usepreamble && lcount) {
      sprintf (line, "%04d.txt", ++mimseqno);
      result->subject  = _FP_strdup (sstate.envelope.subject);
      result->filename = _FP_strdup (line);
      result->origin   = _FP_strdup (sstate.envelope.from);
      result->mimeid   = _FP_strdup (sstate.envelope.mimeid);
      result->mimetype = _FP_strdup ("text/plain");
      result->mode     = 0644;
      result->uudet    = PT_ENCODED;	/* plain text */
      result->sfname   = _FP_strdup (fname);
      result->flags    = FL_SINGLE | FL_PROPER;
      /* result->startpos set from above */
      result->length   = prevpos - result->startpos;
      result->partno   = 1;

      /* MIME message, let's continue */
      *errcode = UURET_CONT;

      if ((sstate.envelope.subject != NULL && result->subject == NULL) ||
	  result->filename == NULL || result->sfname == NULL) {
	*errcode = UURET_NOMEM;
      }

      return result;
    }
    /* MIME message, let's continue */
    if (*errcode == UURET_OK)
      *errcode = UURET_CONT;

    /* otherwise, just return NULL */
    _FP_free (result);
    return NULL;
  }

  /*
   * Read Epilogue, the plain text after the last boundary.
   * This can either end with new headers from the next message of a
   * mail folder or with a `parent' boundary if we are inside an
   * encapsulated Multipart message. Oh yes, and of course the file
   * may also simply end :-)
   * Another possibility is that we might find plain encoded data
   * without encapsulating message. We're not _too_ flexible here,
   * we won't detect Base64, and require a proper `begin' line for
   * uuencoding and xxencoding
   * If uu_usepreamble is set, we produce a result from this one
   */

  if (sstate.ismime && sstate.mimestate == MS_EPILOGUE) {
    result->startpos = ftell (datei);	/* remember start of epilogue */
    prevpos          = ftell (datei);
    preheaders       = ftell (datei);
    preenc           = ftell (datei);
    hcount = lcount  = 0;
    ecount = bhflag  = 0;
    begflag = vflag  = 0;
    res = 0;

    /*
     * If we are in the outermost message and uu_fast_scanning, we
     * know (or assume) that no more messages will follow, so there's
     * no need to scan the rest.
     */
    if (uu_fast_scanning && mssdepth == 0) {
      /*
       * check if the epilogue is empty
       */
      while (!feof (datei) && !ferror (datei) && lcount<10 && res==0) {
	if (_FP_fgets (line, 255, datei) == NULL)
	  break;
	if (!IsLineEmpty (line))
	  res++;
	lcount++;
      }
      if (uu_usepreamble && res) {
	sprintf (line, "%04d.txt", ++mimseqno);
	result->subject  = _FP_strdup (sstate.envelope.subject);
	result->filename = _FP_strdup (line);
	result->origin   = _FP_strdup (sstate.envelope.from);
	result->mimeid   = _FP_strdup (sstate.envelope.mimeid);
	result->mimetype = _FP_strdup ("text/plain");
	result->mode     = 0644;
	result->uudet    = PT_ENCODED;	/* plain text */
	result->sfname   = _FP_strdup (fname);
	result->flags    = FL_SINGLE | FL_PROPER | FL_TOEND;
	result->partno   = 1;
	/* result->startpos set from above */
	result->length   = progress.fsize - result->startpos;

	if ((sstate.envelope.subject != NULL && result->subject == NULL) ||
	    result->filename == NULL || result->sfname == NULL) {
	  *errcode = UURET_NOMEM;
	}

	return result;
      }
      _FP_free (result);
      return NULL;
    }

    if (mssdepth > 0)
      blen = strlen (multistack[mssdepth-1].envelope.boundary);

    while (!feof (datei)) {
      if (_FP_fgets (line, 255, datei) == NULL) {
	line[0] = '\0';
	break;
      }
      if (UUBUSYPOLL(ftell(datei),progress.fsize)) SPCANCEL();
      line[255] = '\0';
      /* check for parent boundary */
      if (mssdepth > 0 && line[0] == '-' && line[1] == '-' &&
	  strncmp (line+2,
		   multistack[mssdepth-1].envelope.boundary, blen) == 0)
	break;

      /* check for next headers only at outermost level */
      if (mssdepth == 0 && IsKnownHeader (line)) {
	(void) ScanHeaderLine (datei, line);
	if (hcount == 0) {
	  preheaders = prevpos;
	  lcount     = 0;
	}
	hcount++; 
	lcount++;

	if (hcount >= hlcount.restart) {
	  /* okay, new headers */
	  break;
	}
      }
      else if (lcount > WAITHEADER) {
	hcount = 0;
	lcount = 0;
      }
      else if (hcount) {
	lcount++;
      }
      else {
	hcount = lcount = 0;
      }

      /* check for begin and encoded data only at outermost level */
      if (mssdepth == 0 && !uu_more_mime) {
	if (strncmp      (line, "begin ",       6) == 0 ||
	    _FP_strnicmp (line, "<pre>begin ", 11) == 0) {
	  preenc  = prevpos;
	  begflag = 1;
	}
	else if (strncmp (line, "end", 3) == 0 && begflag) {
	  ecount = ELC_COUNT;
	  break;
	}
	else if ((vflag = UUValidData (line, 0, &bhflag)) != 0) {
	  if (vflag == BH_ENCODED && bhflag == 0) {
	    /* very short BinHex file follows */
	    preenc = prevpos;
	    break;
	  }
	  /* remember that XX can easily be mistaken as Base64 */
	  if ((vflag == UU_ENCODED || vflag == XX_ENCODED ||
	       vflag == B64ENCODED) && begflag) {
	    if (++ecount >= ELC_COUNT)
	      break;
	  }
	  else {
	    begflag = 0;
	    ecount  = 0;
	  }
	}
	else {
	  begflag = 0;
	  ecount  = 0;
	}
      }

      if (!IsLineEmpty (line))
	res++;

      prevpos = ftell (datei);
    }

    if (mssdepth > 0 &&	line[0] == '-' && line[1] == '-' &&
	strncmp (line+2,
		 multistack[mssdepth-1].envelope.boundary, blen) == 0) {
      /* restore previous state */
      mssdepth--;
      UUkillheaders (&sstate.envelope);
      _FP_free  (sstate.source);
      memcpy (&sstate, &(multistack[mssdepth]), sizeof (scanstate));

      ptr1 = line + 2 + strlen (sstate.envelope.boundary);

      if (*ptr1 == '-' && *(ptr1+1) == '-') {
	sstate.mimestate = MS_EPILOGUE;
      }
      else {
	sstate.mimestate = MS_SUBPART;
      }
      result->length = prevpos - result->startpos;
      *errcode = UURET_CONT;
    }
    else if (mssdepth > 0) {
      UUMessage (uuscan_id, __LINE__, UUMSG_WARNING,
		 uustring (S_MIME_B_NOT_FOUND));
      /*
       * restart and try again; don't restart if uu_fast_scanning
       */
      sstate.isfolder  = 0;
      sstate.ismime    = 0;
      sstate.mimestate = MS_BODY;

      while (mssdepth) {
	mssdepth--;
	UUkillheaders (&(multistack[mssdepth].envelope));
	_FP_free (multistack[mssdepth].source);
      }

      if (!uu_fast_scanning) {
	*errcode = UURET_CONT;
	fseek (datei, preheaders, SEEK_SET);
      }
      _FP_free (result);
      return NULL;
    }
    else if (IsKnownHeader (line)) {
      /* new message follows */
      sstate.isfolder  = 1;
      sstate.ismime    = 0;
      sstate.mimestate = MS_HEADERS;
      result->length   = preheaders - result->startpos;
      fseek (datei, preheaders, SEEK_SET);
    }
    else if (ecount >= ELC_COUNT) {
      /* new plain encoding */
      sstate.isfolder  = 0;
      sstate.ismime    = 0;
      sstate.mimestate = MS_BODY;
      result->length   = preenc - result->startpos;
      fseek (datei, preenc, SEEK_SET);
    }

    /* produce result if uu_usepreamble is set */
    if (uu_usepreamble && res) {
      sprintf (line, "%04d.txt", ++mimseqno);
      result->subject  = _FP_strdup (sstate.envelope.subject);
      result->filename = _FP_strdup (line);
      result->origin   = _FP_strdup (sstate.envelope.from);
      result->mimeid   = _FP_strdup (sstate.envelope.mimeid);
      result->mimetype = _FP_strdup ("text/plain");
      result->mode     = 0644;
      result->uudet    = PT_ENCODED;	/* plain text */
      result->sfname   = _FP_strdup (fname);
      result->flags    = FL_SINGLE | FL_PROPER;
      result->partno   = 1;
      /* result->startpos set from above */
      /* result->length set from above */

      if ((sstate.envelope.subject != NULL && result->subject == NULL) ||
	  result->filename == NULL || result->sfname == NULL) {
	*errcode = UURET_NOMEM;
      }

      return result;
    }
    /* otherwise, just return NULL */
    _FP_free (result);
    return NULL;
  }

  /*
   * Scan a new part from a Multipart message. Check for a new local
   * envelope (which defaults to `Content-Type: text/plain') and
   * evaluate its Content-Type and Content-Transfer-Encoding. If this
   * is another Multipart/something, push the current state onto our
   * stack and dive into the new environment, starting with another
   * preamble.
   */
			   
  if (sstate.ismime && sstate.mimestate == MS_SUBPART) {
    memset (&localenv, 0, sizeof (headers));
    result->startpos = ftell (datei);
    prevpos = ftell (datei);
    hcount  = 0;
    lcount  = 0;

    /*
     * Duplicate some data from outer envelope
     */

    localenv.mimevers = _FP_strdup (sstate.envelope.mimevers);
    localenv.from     = _FP_strdup (sstate.envelope.from);
    localenv.subject  = _FP_strdup (sstate.envelope.subject);
    localenv.rcpt     = _FP_strdup (sstate.envelope.rcpt);
    localenv.date     = _FP_strdup (sstate.envelope.date);

    if ((sstate.envelope.mimevers != NULL && localenv.mimevers == NULL) ||
	(sstate.envelope.from     != NULL && localenv.from     == NULL) ||
	(sstate.envelope.subject  != NULL && localenv.subject  == NULL) ||
	(sstate.envelope.rcpt     != NULL && localenv.rcpt     == NULL) ||
	(sstate.envelope.date     != NULL && localenv.date     == NULL)) {

      while (mssdepth) {
	mssdepth--;
	UUkillheaders (&(multistack[mssdepth].envelope));
	_FP_free (multistack[mssdepth].source);
      }
      sstate.isfolder = 0;
      sstate.ismime   = 0;
      
      UUkillheaders (&localenv);
      *errcode = UURET_NOMEM;
      _FP_free (result);
      return NULL;
    }
    
    /* Scan subheader. But what if there is no subheader? */
    hcount = 0;
    lcount = 0;
    preheaders = prevpos;
    
    if (_FP_fgets (line, 255, datei) == NULL) {
      sstate.isfolder = 0;
      sstate.ismime   = 0;
      while (mssdepth) {
	mssdepth--;
	UUkillheaders (&(multistack[mssdepth].envelope));
	_FP_free (multistack[mssdepth].source);
      }
      UUkillheaders (&localenv);
      _FP_free (result);
      return NULL;
    }
    line[255] = '\0';

    while (!feof (datei) && !IsLineEmpty (line)) {
      if (IsKnownHeader (line))
	hcount++;
      if (UUBUSYPOLL(ftell(datei),progress.fsize)) SPCANCEL();
      if (lcount > WAITHEADER && hcount == 0) {
	fseek (datei, preheaders, SEEK_SET);
	prevpos = preheaders;
	break;
      }
      ptr1 = ScanHeaderLine (datei, line);
      if (ParseHeader (&localenv, ptr1) == NULL)
	*errcode = UURET_NOMEM;

      if (line[0] == '-' && line[1] == '-')
	break;

      prevpos = ftell (datei);

      if (_FP_fgets (line, 255, datei) == NULL)
	break;
      line[255] = '\0';
      lcount++;
    }
    if (line[0] == '-' && line[1] == '-') {
      /*
       * this shouldn't happen, there must always be an empty line,
       * but let's accept it anyway. Just skip back to before the
       * boundary, so that it gets handled below
       */
      fseek (datei, prevpos, SEEK_SET);
    }

    if (_FP_stristr (localenv.ctype, "multipart") != NULL) {
      /* oh no, not again */
      if (mssdepth >= MSMAXDEPTH) {
	/* Argh, what an isane message. Treat as plain text */
	UUMessage (uuscan_id, __LINE__, UUMSG_WARNING,
		   uustring (S_MIME_MULTI_DEPTH));
      }
      else if (localenv.boundary == NULL) {
	UUMessage (uuscan_id, __LINE__, UUMSG_WARNING,
		   uustring (S_MIME_NO_BOUNDARY));
      }
      else {
	memcpy (&multistack[mssdepth], &sstate, sizeof (scanstate));
	memcpy (&sstate.envelope,    &localenv, sizeof (headers));
	memset (&localenv, 0, sizeof (headers));
	sstate.mimestate = MS_PREAMBLE;
	if ((sstate.source = _FP_strdup (sstate.source)) == NULL)
	  *errcode = UURET_NOMEM;

	if (*errcode == UURET_OK)
	  *errcode = UURET_CONT;

	mssdepth++;
	/* need a restart */
	_FP_free (result);
	return NULL;
      }
    }

    /*
     * So this subpart is either plain text or something else. Check
     * the Content-Type and Content-Transfer-Encoding. If the latter
     * is a defined value, we know what to do and just copy everything
     * up to the boundary.
     * If Content-Transfer-Encoding is unknown or missing, look at the
     * Content-Type. If it's "text/plain" or undefined, we subject the
     * message to our encoding detection. Otherwise, treat as plain
     * text.
     * This is done because users might `attach' a uuencoded file, which
     * would then be correctly typed as `text/plain'.
     */

    if (_FP_stristr (localenv.ctenc, "base64") != NULL)
      result->uudet = B64ENCODED;
    else if (_FP_stristr (localenv.ctenc, "x-uue") != NULL) {
      result->uudet = UU_ENCODED;
      result->begin = result->end = 1;
    }
    else if (_FP_stristr (localenv.ctenc, "x-yenc") != NULL) {
      result->uudet = YENC_ENCODED;
      result->begin = result->end = 1;
    }
    else if (_FP_stristr (localenv.ctenc, "quoted-printable") != NULL)
      result->uudet = QP_ENCODED;
    else if (_FP_stristr (localenv.ctenc, "7bit") != NULL ||
	     _FP_stristr (localenv.ctenc, "8bit") != NULL)
      result->uudet = PT_ENCODED;
    else if (_FP_stristr (localenv.ctype, "multipart") != NULL ||
	     _FP_stristr (localenv.ctype, "message")   != NULL)
      result->uudet = PT_ENCODED;

    /*
     * If we're switched to MIME-only mode, handle as text
     */

    if (uu_more_mime >= 2 && !result->uudet) {
      result->uudet = PT_ENCODED;
    }

    if (result->uudet) {
      /*
       * Oh-kay, go ahead. Just read and wait for the boundary
       */
      result->startpos = ftell (datei);
      prevpos          = ftell (datei);
      blen = strlen (sstate.envelope.boundary);
      lcount = 0;
      
      while (!feof (datei)) {
	if (_FP_fgets (line, 255, datei) == NULL) {
	  line[0] = '\0';
	  break;
	}
	if (UUBUSYPOLL(ftell(datei),progress.fsize)) SPCANCEL();
	line[255] = '\0';
	if (line[0] == '-' && line[1] == '-' &&
	    strncmp (line+2, sstate.envelope.boundary, blen) == 0)
	  break;
	/*
	 * I've had a report of someone who tried to decode a huge file
	 * that had an early truncated multipart message and later another
	 * multipart message with the *same* boundary. Consequently, all
	 * some hundred messages inbetween were ignored ...
	 * This check here doesn't cover folded header lines, but we don't
	 * want to slow down scanning too much. We just check for
	 * Content-Type: multipart/... boundary="same-boundary"
	 */
	if (line[0] == 'C' && line[1] == 'o' &&
	    _FP_strnicmp (line, "Content-Type:", 13) == 0) {
	  ptr1 = ScanHeaderLine (datei, line);
	  ptr2 = (ptr1)?_FP_stristr(ptr1,"boundary"):NULL;
	  ptr1 = (ptr2)?ParseValue(ptr2):NULL;
	  if (ptr1 && strcmp (ptr1, sstate.envelope.boundary) == 0)
	    break;
	  for (res=0; ptr1 && res<mssdepth; res++)
	    if (strcmp (ptr1, multistack[res].envelope.boundary) == 0)
	      break;
	  if (res<mssdepth)
	    break;
	}
	if (!IsLineEmpty (line))
	  lcount++;
	prevpos = ftell (datei);
      }
      if (line[0] == '-' && line[1] == '-' &&
	  strncmp (line+2, sstate.envelope.boundary, blen) == 0) {
	ptr1 = line + 2 + blen;
	if (*ptr1 == '-' && *(ptr1+1) == '-')
	  sstate.mimestate = MS_EPILOGUE;
	else
	  sstate.mimestate = MS_SUBPART;
      }
      else {
	UUMessage (uuscan_id, __LINE__, UUMSG_WARNING,
		   uustring (S_MIME_B_NOT_FOUND));

	while (mssdepth) {
	  mssdepth--;
	  UUkillheaders (&(multistack[mssdepth].envelope));
	  _FP_free (multistack[mssdepth].source);
	}
	/*
	 * Don't retry if uu_fast_scanning
	 */

	if (uu_fast_scanning) {
	  UUkillheaders (&localenv);
	  sstate.isfolder  = 0;
	  sstate.ismime    = 0;
	  sstate.mimestate = MS_BODY;
	  _FP_free (result);
	  return NULL;
	}

	/*
	 * Retry, but listening to headers this time
	 */
	fseek (datei, result->startpos, SEEK_SET);

	UUkillfread (result);
	if ((result = (fileread *) malloc (sizeof (fileread))) == NULL) {
	  *errcode = UURET_NOMEM;
	  sstate.isfolder = 0;
	  sstate.ismime   = 0;
	  UUkillheaders (&localenv);
	  return NULL;
	}
	memset (result, 0, sizeof (fileread));

	if ((res = ScanData (datei, fname, errcode, NULL, 1, 1, result))==-1) {
	  /* oops, something went wrong */
	  sstate.isfolder = 0;
	  sstate.ismime   = 0;
	  UUkillfread   (result);
	  UUkillheaders (&localenv);
	  return NULL;
	}
	if (res == 1) {
	  /*
	   * new headers found
	   */
	  sstate.isfolder  = 1;
	  sstate.ismime    = 0;
	  sstate.mimestate = MS_HEADERS;
	}
	else {
	  sstate.isfolder  = 0;
	  sstate.ismime    = 0;
	}
      }
      /* produce result if uu_handletext is set */
      /* or if the file is explicitely named */
      if (result->uudet == B64ENCODED || lcount) {
	if (localenv.fname) {
	  _FP_free (result->filename);
	  if ((result->filename = _FP_strdup (localenv.fname)) == NULL)
	    *errcode = UURET_NOMEM;
	}
	else if ((result->uudet==QP_ENCODED||result->uudet==PT_ENCODED) &&
		 result->filename == NULL && uu_handletext) {
	  sprintf (line, "%04d.txt", ++mimseqno);
	  if ((result->filename = _FP_strdup (line)) == NULL)
	    *errcode = UURET_NOMEM;
	}
	result->subject  = _FP_strdup (localenv.subject);
	result->origin   = _FP_strdup (localenv.from);
	result->mimeid   = _FP_strdup (localenv.mimeid);
	result->mimetype = _FP_strdup (localenv.ctype);
	result->mode     = 0644;
	result->sfname   = _FP_strdup (fname);
	result->flags    = FL_SINGLE | FL_PROPER;
	result->partno   = 1;
	/* result->uudet determined above */
	/* result->startpos set from above */
	result->length   = prevpos - result->startpos;

	if ((localenv.subject != NULL && result->subject == NULL) ||
	    result->filename == NULL  || result->sfname == NULL) {
	  *errcode = UURET_NOMEM;
	}
      }
      else {
	/* don't produce a result */
	_FP_free (result);
	result = NULL;
      }
      if (*errcode == UURET_OK)
	*errcode = UURET_CONT;
      /*
       * destroy local envelope
       */
      UUkillheaders (&localenv);
      return result;
    }

    /*
     * we're in a subpart, but the local headers don't give us any
     * clue about what's to find here. So look for encoded data by
     * ourselves.
     */

    if ((res = ScanData (datei, fname, errcode,
			 sstate.envelope.boundary,
			 1, 0, result)) == -1) {
      /* oops, something went wrong */
      sstate.isfolder = 0;
      sstate.ismime   = 0;
      UUkillfread   (result);
      UUkillheaders (&localenv);
      return NULL;
    }
    /*
     * we should really be at a boundary here, but check again
     */
    blen    = strlen (sstate.envelope.boundary);
    prevpos = ftell  (datei);

    while (!feof (datei)) {
      if (_FP_fgets (line, 255, datei) == NULL) {
	line[0] = '\0';
	break;
      }
      if (UUBUSYPOLL(ftell(datei),progress.fsize)) SPCANCEL();
      line[255] = '\0';
      if (line[0] == '-' && line[1] == '-' &&
	  strncmp (line+2, sstate.envelope.boundary, blen) == 0)
	break;
      if (line[0] == 'C' && line[1] == 'o' &&
	  _FP_strnicmp (line, "Content-Type:", 13) == 0) {
	ptr1 = ScanHeaderLine (datei, line);
	ptr2 = (ptr1)?_FP_stristr(ptr1,"boundary"):NULL;
	ptr1 = (ptr2)?ParseValue(ptr2):NULL;
	if (ptr1 && strcmp (ptr1, sstate.envelope.boundary) == 0)
	  break;
      }
      prevpos = ftell (datei);
    }
    /*
     * check if this was the last subpart
     */
    if (line[0] == '-' && line[1] == '-' &&
	strncmp (line+2, sstate.envelope.boundary, blen) == 0) {
      ptr1 = line + 2 + blen;
      if (*ptr1 == '-' && *(ptr1+1) == '-')
	sstate.mimestate = MS_EPILOGUE;
      else
	sstate.mimestate = MS_SUBPART;
    }
    else {
      UUMessage (uuscan_id, __LINE__, UUMSG_WARNING,
		 uustring (S_MIME_B_NOT_FOUND));
      
      while (mssdepth) {
	mssdepth--;
	UUkillheaders (&(multistack[mssdepth].envelope));
	_FP_free (multistack[mssdepth].source);
      }

      if (uu_fast_scanning) {
	UUkillheaders (&localenv);
	sstate.isfolder  = 0;
	sstate.ismime    = 0;
	sstate.mimestate = MS_BODY;
	_FP_free (result);
	return NULL;
      }

      /*
       * Retry, listening to headers this time
       */
      fseek (datei, result->startpos, SEEK_SET);
      
      UUkillfread (result);
      if ((result = (fileread *) malloc (sizeof (fileread))) == NULL) {
	*errcode = UURET_NOMEM;
	sstate.isfolder = 0;
	sstate.ismime   = 0;
	UUkillheaders (&localenv);
	return NULL;
      }
      memset (result, 0, sizeof (fileread));

      if ((res = ScanData (datei, fname, errcode, NULL, 1, 1, result))==-1) {
	/* oops, something went wrong */
	sstate.isfolder = 0;
	sstate.ismime   = 0;
	UUkillfread   (result);
	UUkillheaders (&localenv);
	return NULL;
      }
      if (res == 1) {
	/*
	 * new headers found
	 */
	sstate.isfolder  = 1;
	sstate.ismime    = 0;
	sstate.mimestate = MS_HEADERS;
      }
      else {
	sstate.isfolder  = 0;
	sstate.ismime    = 0;
      }
    }

    /*
     * If this file has been nicely MIME so far, then be very suspicious
     * if ScanData reports anything else. So do a double check, and if
     * it doesn't hold up, handle as plain text instead.
     */

    if (sstate.ismime && sstate.mimestate == MS_SUBPART &&
        strcmp (localenv.mimevers, "1.0") == 0 &&
	_FP_stristr (localenv.ctype, "text") != NULL &&
	!uu_desperate) {
      if (result->uudet == UU_ENCODED && !(result->begin || result->end)) {
	result->uudet = 0;
      }
    }

    /*
     * produce result
     */

    if (result->uudet == 0) {
      result->uudet = PT_ENCODED; /* plain text */
    }

    if (localenv.fname) {
      _FP_free (result->filename);
      if ((result->filename = _FP_strdup (localenv.fname)) == NULL)
	*errcode = UURET_NOMEM;
    }
    else if ((result->uudet==QP_ENCODED || result->uudet==PT_ENCODED) &&
	     result->filename==NULL && uu_handletext) {
      sprintf (line, "%04d.txt", ++mimseqno);
      if ((result->filename = _FP_strdup (line)) == NULL)
	*errcode = UURET_NOMEM;
    }
    else {
      /* assign a filename lateron */
    }
    if (result->mimetype) _FP_free (result->mimetype);
    if (result->uudet) {
      if (_FP_stristr (localenv.ctype, "text") != NULL &&
	  result->uudet != QP_ENCODED && result->uudet != PT_ENCODED)
	result->mimetype = NULL; /* better don't set it */
      else
	result->mimetype = _FP_strdup (localenv.ctype);
    }
    if (result->origin) _FP_free  (result->origin);
    result->origin  = _FP_strdup  (localenv.from);

    if (result->subject) _FP_free (result->subject);
    result->subject = _FP_strdup  (localenv.subject);

    if (result->sfname == NULL)
      if ((result->sfname = _FP_strdup (fname)) == NULL)
	*errcode = UURET_NOMEM;

    result->length = prevpos - result->startpos;
    result->flags  = FL_SINGLE | FL_PROPER;
    result->partno = 1;

    if (result->mode == 0)
      result->mode = 0644;

    /*
     * the other fields should already be set appropriately
     */

    if (*errcode == UURET_OK)
      *errcode = UURET_CONT;

    /*
     * kill local envelope
     */
    UUkillheaders (&localenv);
    
    return result;
  }

  /*
   * All right, so we're not in a Multipart message. Phew, took quite
   * long to figure this out. But this might still be a MIME message
   * body. And if it's a message/partial, we need more special handling
   */

  if (sstate.isfolder && sstate.ismime && sstate.mimestate == MS_BODY &&
      _FP_stristr (sstate.envelope.ctype, "message") != NULL &&
      _FP_stristr (sstate.envelope.ctype, "partial") != NULL) {

    result->startpos = ftell (datei);

    if (sstate.envelope.partno == 1) {
      /* read local envelope */
      UUkillheaders (&localenv);
      memset (&localenv, 0, sizeof (headers));

      /* skip over blank lines first */
      prevpos = ftell (datei);
      while (!feof (datei)) {
	if (_FP_fgets (line, 255, datei) == NULL)
	  break;
	line[255] = '\0';
	if (UUBUSYPOLL(ftell(datei),progress.fsize)) SPCANCEL();
	if (!IsLineEmpty (line))
	  break;
	prevpos = ftell (datei);
      }
      /* Next, read header. But what if there is no subheader? */
      hcount = 0;
      lcount = 0;
      preheaders = prevpos;

      while (!feof (datei) && !IsLineEmpty (line)) {
	if (IsKnownHeader (line))
	  hcount++;
	if (UUBUSYPOLL(ftell(datei),progress.fsize)) SPCANCEL();
	if (lcount > WAITHEADER && hcount == 0) {
	  fseek (datei, preheaders, SEEK_SET);
	  break;
	}
	ptr1 = ScanHeaderLine (datei, line);
	if (ParseHeader (&localenv, ptr1) == NULL)
	  *errcode = UURET_NOMEM;

	if (_FP_fgets (line, 255, datei) == NULL)
	  break;
	line[255] = '\0';
	lcount++;
      }
      prevpos = ftell (datei);
      /*
       * Examine local header. We're mostly interested in the Content-Type
       * and the Content-Transfer-Encoding.
       */
      if (_FP_stristr (localenv.ctype, "multipart") != NULL) {
	UUMessage (uuscan_id, __LINE__, UUMSG_WARNING,
		   uustring (S_MIME_PART_MULTI));
      }
      if (localenv.subject)
	result->subject  = _FP_strdup (localenv.subject);
      else
	result->subject  = _FP_strdup (sstate.envelope.subject);

      if (localenv.from)
	result->origin   = _FP_strdup (localenv.from);
      else
	result->origin   = _FP_strdup (sstate.envelope.from);

      if (localenv.ctype)
	result->mimetype = _FP_strdup (localenv.ctype);
      else
	result->mimetype = _FP_strdup ("text/plain");

      if (_FP_stristr (localenv.ctenc, "quoted-printable") != NULL)
	result->uudet = QP_ENCODED;
      else if (_FP_stristr (localenv.ctenc, "base64") != NULL)
	result->uudet = B64ENCODED;
      else if (_FP_stristr (localenv.ctenc, "x-uue") != NULL) {
	result->uudet = UU_ENCODED;
	result->begin = result->end = 1;
      }
      else if (_FP_stristr (localenv.ctenc, "x-yenc") != NULL) {
	result->uudet = YENC_ENCODED;
	result->begin = result->end = 1;
      }
      else if (_FP_stristr (localenv.ctenc, "7bit") != NULL ||
	       _FP_stristr (localenv.ctenc, "8bit") != NULL)
	result->uudet = PT_ENCODED;
      else if (_FP_stristr (localenv.ctype, "multipart") != NULL ||
	       _FP_stristr (localenv.ctype, "message")   != NULL)
	result->uudet = PT_ENCODED;

      /*
       * If we're switched to MIME-only mode, handle as text
       */

      if (uu_more_mime >= 2 && !result->uudet) {
	result->uudet = PT_ENCODED;
      }
    }
    else {
      memset (&localenv, 0, sizeof (headers));
    }

    /*
     * If this is Quoted-Printable or Plain Text, just try looking
     * for the next message header. If uu_fast_scanning, and the
     * encoding is known, there's no need to look below. Otherwise,
     * we check the type of encoding first.
     * The encoding type is determined on the first part; in all
     * others, we also don't read on.
     * If we have a partial multipart message, scan for headers, but
     * do not react on standard MIME headers, as they are probably
     * from the subparts. However, we're stuck if there's an embedded
     * message/rfc822 :-(
     * If it is a "trivial" (non-embedded) message/rfc822, skip over
     * the message header and then start looking for the next header.
     */
    if (uu_fast_scanning && (result->uudet!=0||sstate.envelope.partno!=1)) {
      /* do nothing */
      res = 0;
    }
    else if (result->uudet != 0) {
      hcount = lcount = 0;
      prevpos = ftell (datei);

      if (_FP_stristr (localenv.ctype, "message") != NULL &&
	  _FP_stristr (localenv.ctype, "rfc822")  != NULL) {
	/*
	 * skip over empty lines and local header
	 */
	preheaders = ftell (datei);
	while (!feof (datei)) {
	  if (_FP_fgets (line, 255, datei) == NULL)
	    break;
	  line[255] = '\0';
	  if (!IsLineEmpty (line)) {
	    break;
	  }
	}

	while (!feof (datei) && !IsLineEmpty (line)) { 
	  if (IsKnownHeader (line))
	    hcount++;
	  lcount++;
	  if (lcount > WAITHEADER && hcount < hlcount.afternl)
	    break;

	  if (_FP_fgets (line, 255, datei) == NULL)
	    break;
	  line[255] = '\0';
	}
	if (hcount < hlcount.afternl)
	  fseek (datei, preheaders, SEEK_SET);
	hcount = lcount = 0;
      }

      /*
       * look for next header
       */

      while (!feof (datei)) {
	if (_FP_fgets (line, 255, datei) == NULL)
	  break;
	if (UUBUSYPOLL(ftell(datei),progress.fsize)) SPCANCEL();
	if (ferror (datei))
	  break;
	line[255] = '\0';

	if ((vflag = IsKnownHeader (line))) {
	  (void) ScanHeaderLine (datei, line);

	  if (result->uudet != PT_ENCODED || vflag == 1) {
	    if (hcount == 0)
	      preheaders = prevpos;
	    hcount++;
	    lcount++;
	    if (hcount >= hlcount.restart) {
	      /*
	       * Hey, a new header starts here
	       */
	      fseek (datei, preheaders, SEEK_SET);
	      prevpos = preheaders;
	      break;
	    }
	  }
	}
	else if (lcount > WAITHEADER) {
	  hcount = 0;
	  lcount = 0;
	}
	else if (hcount) {
	  lcount++;
	}
	prevpos = ftell (datei);
      }
      res = 1;
    }
    else {
      /*
       * Otherwise, let's see what we can find ourself. No
       * boundary (NULL) but MIME, and respect new headers.
       */
      if ((res = ScanData (datei, fname, errcode, NULL, 1, 1, result)) == -1) {
	/* oops, something went wrong */
	sstate.isfolder = 0;
	sstate.ismime   = 0;
	UUkillfread   (result);
	UUkillheaders (&localenv);
	return NULL;
      }
      if (result->uudet == 0 && uu_handletext)
	result->uudet = PT_ENCODED;

      prevpos = ftell (datei);
    }
    /*
     * produce result
     */
    if (localenv.fname) {
      _FP_free (result->filename);
      if ((result->filename = _FP_strdup (localenv.fname)) == NULL)
	*errcode = UURET_NOMEM;
    }
    else if (sstate.envelope.fname) {
      _FP_free (result->filename);
      if ((result->filename = _FP_strdup (sstate.envelope.fname)) == NULL)
	*errcode = UURET_NOMEM;
    }
    else if ((result->uudet==QP_ENCODED || result->uudet==PT_ENCODED) &&
	     result->filename == NULL) {
      sprintf (line, "%04d.txt", ++mimseqno);
      if ((result->filename = _FP_strdup (line)) == NULL)
	*errcode = UURET_NOMEM;
    }
    else {
      /* assign a filename lateron */
    }
    if (result->subject == NULL) {
      if (sstate.envelope.subject)
	result->subject = _FP_strdup (sstate.envelope.subject);
    }
    result->partno = sstate.envelope.partno;
    result->maxpno = sstate.envelope.numparts;
    result->flags  = FL_PARTIAL | 
      ((res==1 || uu_fast_scanning) ? FL_PROPER : 0) |
	((uu_fast_scanning) ? FL_TOEND : 0);
    result->mimeid = _FP_strdup (sstate.envelope.mimeid);
    if (result->partno == 1)
      result->begin = 1;

    if (uu_fast_scanning)
      result->length = progress.fsize - result->startpos;
    else
      result->length = prevpos - result->startpos;

    if (result->sfname == NULL)
      result->sfname = _FP_strdup (fname);

    if (result->mode == 0)
      result->mode = 0644;

    /*
     * the other fields should already be set appropriately
     */

    if (res == 1) {
      /*
       * new headers found
       */
      sstate.isfolder  = 1;
      sstate.ismime    = 0;
      sstate.mimestate = MS_HEADERS;
      
      UUkillheaders (&sstate.envelope);
      memset (&sstate.envelope, 0, sizeof (headers));
    }
    else {
      /*
       * otherwise, this can't be a mail folder
       */
      sstate.isfolder  = 0;
      sstate.ismime    = 0;
    }
    /*
     * kill local envelope
     */
    UUkillheaders (&localenv);
    return result;
  }

  /*
   * If this is a MIME body, honor a Content-Type different than
   * text/plain or a proper Content-Transfer-Encoding.
   * We also go in here if we have an assigned filename - this means
   * that we've had a Content-Disposition field, and we should probably
   * decode a plain-text segment with a filename.
   */

  if (sstate.isfolder && sstate.ismime &&
      sstate.mimestate == MS_BODY &&
      (_FP_stristr (sstate.envelope.ctenc, "quoted-printable") != NULL ||
       _FP_stristr (sstate.envelope.ctenc, "base64")           != NULL ||
       _FP_stristr (sstate.envelope.ctenc, "x-uue")            != NULL ||
       _FP_stristr (sstate.envelope.ctenc, "x-yenc")           != NULL ||
       _FP_stristr (sstate.envelope.ctype, "message")          != NULL ||
       sstate.envelope.fname != NULL)) {

    if (sstate.envelope.subject)
      result->subject = _FP_strdup (sstate.envelope.subject);
    if (sstate.envelope.from)
      result->origin  = _FP_strdup (sstate.envelope.from);

    if (sstate.envelope.ctype)
      result->mimetype = _FP_strdup (sstate.envelope.ctype);
    else
      result->mimetype = _FP_strdup ("text/plain");

    if (_FP_stristr (sstate.envelope.ctenc, "quoted-printable") != NULL)
      result->uudet = QP_ENCODED;
    else if (_FP_stristr (sstate.envelope.ctenc, "base64") != NULL)
      result->uudet = B64ENCODED;
    else if (_FP_stristr (sstate.envelope.ctenc, "x-uue") != NULL) {
      result->uudet = UU_ENCODED;
      result->begin = result->end = 1;
    }
    else if (_FP_stristr (sstate.envelope.ctenc, "x-yenc") != NULL) {
      result->uudet = YENC_ENCODED;
    }
    else if (_FP_stristr (sstate.envelope.ctenc, "7bit") != NULL ||
	     _FP_stristr (sstate.envelope.ctenc, "8bit") != NULL)
      result->uudet = PT_ENCODED;
    else if (_FP_stristr (sstate.envelope.ctype, "multipart") != NULL ||
	     _FP_stristr (sstate.envelope.ctype, "message")   != NULL ||
	     sstate.envelope.fname != NULL)
      result->uudet = PT_ENCODED;

    /*
     * If we're switched to MIME-only mode, handle as text
     */

    if (uu_more_mime >= 2 && !result->uudet) {
      result->uudet = PT_ENCODED;
    }

    result->startpos = prevpos = ftell (datei);

    /*
     * If this is Quoted-Printable or Plain Text, just try looking
     * for the next message header. If uu_fast_scanning, we know
     * there won't be more headers.
     * If it is a "trivial" (non-embedded) message/rfc822, skip over
     * the message header and then start looking for the next header.
     */
    if (result->uudet != 0 && uu_fast_scanning) {
      /* do nothing */
      res = 0;
    }
    else if (result->uudet != 0) {
      hcount = lcount = 0;
      prevpos = ftell (datei);

      if (_FP_stristr (sstate.envelope.ctype, "message") != NULL &&
	  _FP_stristr (sstate.envelope.ctype, "rfc822")  != NULL) {
	/*
	 * skip over empty lines and local header
	 */
	preheaders = ftell (datei);
	while (!feof (datei)) {
	  if (_FP_fgets (line, 255, datei) == NULL)
	    break;
	  line[255] = '\0';
	  if (!IsLineEmpty (line)) {
	    break;
	  }
	}

	while (!feof (datei) && !IsLineEmpty (line)) { 
	  if (IsKnownHeader (line))
	    hcount++;
	  lcount++;
	  if (lcount > WAITHEADER && hcount < hlcount.afternl)
	    break;

	  if (_FP_fgets (line, 255, datei) == NULL)
	    break;
	  line[255] = '\0';
	}
	if (hcount < hlcount.afternl)
	  fseek (datei, preheaders, SEEK_SET);
	hcount = lcount = 0;
      }

      /*
       * look for next header
       */

      while (!feof (datei)) {
	if (_FP_fgets (line, 255, datei) == NULL)
	  break;
	if (UUBUSYPOLL(ftell(datei),progress.fsize)) SPCANCEL();
	if (ferror (datei))
	  break;
	line[255] = '\0';

	if (IsKnownHeader (line)) {
	  (void) ScanHeaderLine (datei, line);
	  if (hcount == 0)
	    preheaders = prevpos;
	  hcount++;
	  lcount++;
	  if (hcount >= hlcount.restart) {
	    /*
	     * Hey, a new header starts here
	     */
	    fseek (datei, preheaders, SEEK_SET);
	    prevpos = preheaders;
	    break;
	  }
	}
	else if (lcount > WAITHEADER) {
	  hcount = 0;
	  lcount = 0;
	}
	else if (hcount) {
	  lcount++;
	}
	prevpos = ftell (datei);
      }
      res = 1;
    }
    else {
      /*
       * Otherwise, let's see what we can find ourself. No
       * boundary (NULL) but MIME, and respect new headers.
       */
      if ((res = ScanData (datei, fname, errcode, NULL, 1, 1, result)) == -1) {
	/* oops, something went wrong */
	sstate.isfolder = 0;
	sstate.ismime   = 0;
	UUkillfread   (result);
	return NULL;
      }
      if (result->uudet == 0 && uu_handletext) {
	result->startpos = before;	/* display headers */
	result->uudet = PT_ENCODED;
      }

      prevpos = ftell (datei);
    }
    /*
     * produce result
     */
    if (sstate.envelope.fname) {
      _FP_free (result->filename);
      if ((result->filename = _FP_strdup (sstate.envelope.fname)) == NULL)
	*errcode = UURET_NOMEM;
    }
    else if ((result->uudet==QP_ENCODED||result->uudet==PT_ENCODED) &&
	     result->filename == NULL) {
      sprintf (line, "%04d.txt", ++mimseqno);
      if ((result->filename = _FP_strdup (line)) == NULL)
	*errcode = UURET_NOMEM;
    }
    else {
      /* assign a filename lateron */
    }
    if (result->subject == NULL) {
      if (sstate.envelope.subject)
	result->subject = _FP_strdup (sstate.envelope.subject);
    }
    result->flags  = ((res==1||uu_fast_scanning)?FL_PROPER:0) |
      ((uu_fast_scanning) ? FL_TOEND : 0);
    result->mimeid = _FP_strdup (sstate.envelope.mimeid);

    if (uu_fast_scanning)
      result->length = progress.fsize - result->startpos;
    else
      result->length = prevpos - result->startpos;

    if (result->sfname == NULL)
      result->sfname = _FP_strdup (fname);

    if (result->mode == 0)
      result->mode = 0644;

    /*
     * the other fields should already be set appropriately
     */

    if (res == 1) {
      /*
       * new headers found
       */
      sstate.isfolder  = 1;
      sstate.ismime    = 0;
      sstate.mimestate = MS_HEADERS;

      UUkillheaders (&sstate.envelope);
      memset (&sstate.envelope, 0, sizeof (headers));
    }
    else {
      /*
       * otherwise, this can't be a mail folder
       */
      sstate.isfolder  = 0;
      sstate.ismime    = 0;
    }

    return result;
  }

  /*
   * Some files have reduced headers, and what should be a multipart
   * message is missing the proper Content-Type. If the first thing
   * we find after a couple of empty lines is a boundary, try it!
   * But make sure that this is indeed intended as being a boundary.
   *
   * Only accept it if there was indeed no Content-Type header line
   * and if the following line is a proper Content-Type header. BTW,
   * we know that sstate.envelope.boundary is NULL, or we wouldn't
   * be here!
   */

  if ((sstate.envelope.ctype == NULL ||
       _FP_stristr (sstate.envelope.ctype, "multipart") != NULL) &&
      !uu_more_mime) {
    prevpos = ftell (datei);
    while (!feof (datei)) {
      if (_FP_fgets (line, 255, datei) == NULL) {
	line[0] = '\0';
	break;
      }
      if (UUBUSYPOLL(ftell(datei),progress.fsize)) SPCANCEL();
      if (!IsLineEmpty (line))
	break;
    }
    if (line[0] == '-' && line[1] == '-' &&
	!IsLineEmpty (line+2) && !feof (datei)) {
      ptr1 = _FP_strrstr (line+2, "--");
      ptr2 = ScanHeaderLine (datei, NULL);
      if ((ptr1 == NULL || (*(ptr1+2) != '\012' && *(ptr1+2) != '\015')) &&
	  ptr2 && _FP_strnicmp (ptr2, "Content-", 8) == 0) {
	/*
	 * hmm, okay, let's do it!
	 */
	sstate.isfolder  = 1;
	sstate.ismime    = 1;
	sstate.mimestate = MS_PREAMBLE;
	/*
	 * get boundary
	 */
	ptr1 = line+2;
	while (*ptr1 && !isspace(*ptr1))
	  ptr1++;
	*ptr1 = '\0';

	sstate.envelope.mimevers = _FP_strdup ("1.0");
	sstate.envelope.boundary = _FP_strdup (line+2);
	
	/*
	 * need restart
	 */
	
	fseek (datei, prevpos, SEEK_SET);
	
	_FP_free (result);
	return NULL;
      }
    }
    fseek (datei, prevpos, SEEK_SET);
  }

  /*
   * Hmm, we're not in a ''special'' state, so it's more or less
   * Freestyle time. Anyway, if this seems to be a Mime message,
   * don't allow the minimal Base64 handling.
   */

  if (sstate.envelope.subject)
    result->subject = _FP_strdup (sstate.envelope.subject);
  if (sstate.envelope.from)
    result->origin  = _FP_strdup (sstate.envelope.from);

  if (sstate.envelope.ctype)
    result->mimetype = _FP_strdup (sstate.envelope.ctype);
  
  if ((res=ScanData (datei, fname, errcode, NULL, 
		     sstate.ismime, 1, result))==-1) {
    /* oops, something went wrong */
    sstate.isfolder = 0;
    sstate.ismime   = 0;
    UUkillfread   (result);
    return NULL;
  }

  /*
   * produce result
   */

  if (result->uudet == 0 && uu_handletext) {
    result->startpos = before;	/* display headers */
    result->uudet  = PT_ENCODED;
    result->partno = 1;
  }

  if (result->uudet == YENC_ENCODED && result->filename != NULL) {
    /*
     * prevent replacing the filename found on the =ybegin line
     */
  }
  else if (sstate.envelope.fname) {
    _FP_free (result->filename);
    if ((result->filename = _FP_strdup (sstate.envelope.fname)) == NULL)
      *errcode = UURET_NOMEM;
  }
  else if ((result->uudet==QP_ENCODED||result->uudet==PT_ENCODED) &&
	   result->filename == NULL) {
    sprintf (line, "%04d.txt", ++mimseqno);
    if ((result->filename = _FP_strdup (line)) == NULL)
      *errcode = UURET_NOMEM;
  }
  else {
    /* assign a filename lateron */
  }

  if (result->subject == NULL) {
    if (sstate.envelope.subject)
      result->subject = _FP_strdup (sstate.envelope.subject);
  }

  result->flags  = (result->uudet==PT_ENCODED)?FL_SINGLE:0;
  result->mimeid = _FP_strdup (sstate.envelope.mimeid);
  result->length = ftell (datei) - result->startpos;

  if (result->mode == 0)
    result->mode = 0644;

  if (result->sfname == NULL)
    result->sfname = _FP_strdup (fname);

  if (res == 1) {
    /*
     * new headers found
     */
    sstate.isfolder  = 1;
    sstate.ismime    = 0;
    sstate.mimestate = MS_HEADERS;

    UUkillheaders (&sstate.envelope);
    memset (&sstate.envelope, 0, sizeof (headers));
  }
  else {
    /*
     * otherwise, this can't be a mail folder
     */
    sstate.isfolder  = 0;
    sstate.ismime    = 0;
  }

  return result;

  /*
   * Emergency handling. Set errcode before jumping here.
   */
 ScanPartEmergency:
  UUkillfread   (result);
  UUkillheaders (&localenv);

  while (mssdepth) {
    mssdepth--;
    UUkillheaders (&(multistack[mssdepth].envelope));
    _FP_free (multistack[mssdepth].source);
  }

  return NULL;
}

