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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef SYSTEM_WINDLL
#include <windows.h>
#endif
#ifdef SYSTEM_OS2
#include <os2.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#ifdef STDC_HEADERS
#include <stdlib.h>
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <uudeview.h>
#include <uuint.h>
#include <fptools.h>
#include <uustring.h>
#include <crc32.h>

/* for braindead systems */
#ifndef SEEK_SET
#ifdef L_BEGIN
#define SEEK_SET L_BEGIN
#else
#define SEEK_SET 0
#endif
#endif

char * uuencode_id = "$Id: uuencode.c,v 1.1 2004/04/19 17:50:27 dasenbro Exp $";

#if 0
/*
 * the End-Of-Line string. MIME enforces CRLF, so that's what we use. Some
 * implementations of uudecode will complain about a missing end line, since
 * they look for "end^J" but find "end^J^M". We don't care - especially be-
 * cause they still decode the file properly despite this complaint.
 */

#ifndef EOLSTRING
#define EOLSTRING	"\015\012"
#endif

#else

/*
 * Argh. Some delivery software (inews) has problems with the CRLF
 * line termination. Let's try native EOL and see if we run into
 * any problems.
 * This involves opening output files in text mode instead of binary
 */

#ifndef EOLSTRING
#define EOLSTRING	"\n"
#endif

#endif


/*
 * =========================================================================
 * User-configurable settings end here. Don't spy below unless you know what
 * you're doing.
 * =========================================================================
 */

/*
 * Define End-Of-Line sequence
 */

#ifdef EOLSTRING
static unsigned char *eolstring = (unsigned char *) EOLSTRING;
#else
static unsigned char *eolstring = (unsigned char *) "\012";
#endif

/*
 * Content-Transfer-Encoding types for non-MIME encodings
 */

#define CTE_UUENC	"x-uuencode"
#define CTE_XXENC	"x-xxencode"
#define CTE_BINHEX	"x-binhex"
#define CTE_YENC	"x-yenc"

#define CTE_TYPE(y)	(((y)==B64ENCODED) ? "Base64"  : \
			 ((y)==UU_ENCODED) ? CTE_UUENC : \
			 ((y)==XX_ENCODED) ? CTE_XXENC : \
                         ((y)==PT_ENCODED) ? "8bit" : \
                         ((y)==QP_ENCODED) ? "quoted-printable" : \
			 ((y)==BH_ENCODED) ? CTE_BINHEX : \
			 ((y)==YENC_ENCODED) ? CTE_YENC : "x-oops")

/*
 * encoding tables
 */

unsigned char UUEncodeTable[64] = {
  '`', '!', '"', '#', '$', '%', '&', '\'',
  '(', ')', '*', '+', ',', '-', '.', '/',
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', ':', ';', '<', '=', '>', '?',
  '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
  'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
  'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
  'X', 'Y', 'Z', '[', '\\',']', '^', '_'
};
  

unsigned char B64EncodeTable[64] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
  'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
  'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
  'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
  'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
  'w', 'x', 'y', 'z', '0', '1', '2', '3',
  '4', '5', '6', '7', '8', '9', '+', '/'
};

unsigned char XXEncodeTable[64] = {
  '+', '-', '0', '1', '2', '3', '4', '5',
  '6', '7', '8', '9', 'A', 'B', 'C', 'D',
  'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
  'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
  'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b',
  'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
  'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r',
  's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
};

unsigned char BHEncodeTable[64] = {
  '!', '"', '#', '$', '%', '&', '\'', '(',
  ')', '*', '+', ',', '-', '0', '1', '2',
  '3', '4', '5', '6', '8', '9', '@', 'A', 
  'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 
  'J', 'K', 'L', 'M', 'N', 'P', 'Q', 'R', 
  'S', 'T', 'U', 'V', 'X', 'Y', 'Z', '[', 
  '`', 'a', 'b', 'c', 'd', 'e', 'f', 'h', 
  'i', 'j', 'k', 'l', 'm', 'p', 'q', 'r'
};

unsigned char HexEncodeTable[16] = {
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

typedef struct {
  char *extension;
  char *mimetype;
} mimemap;

/*
 * This table maps a file's extension into a Content-Type. The current
 * official list can be downloaded as
 *   ftp://ftp.isi.edu/in-notes/iana/assignments/media-type
 * I haven't listed any text types since it doesn't make sense to encode
 * them. Everything not on the list gets mapped to application/octet-stream
 */

static mimemap mimetable[] = {
	{ "gif",  "image/gif"        }, /* Grafics Interchange Format  */
	{ "jpg",  "image/jpeg"       }, /* JFIF encoded files          */
	{ "jpeg", "image/jpeg"       },
	{ "tif",  "image/tiff"       }, /* Tag Image File Format       */
	{ "tiff", "image/tiff"       },
	{ "cgm",  "image/cgm"        }, /* Computer Graphics Metafile  */
	{ "au",   "audio/basic"      }, /* 8kHz ulaw audio data        */
	{ "mov",  "video/quicktime"  }, /* Apple Quicktime             */
	{ "qt",   "video/quicktime"  }, /* Also infrequently used      */
	{ "mpeg", "video/mpeg"       }, /* Motion Picture Expert Group */
	{ "mpg",  "video/mpeg"       },
	{ "mp2",  "video/mpeg"       }, /* dito, MPEG-2 encoded files  */
	{ "mp3",  "audio/mpeg"       }, /* dito, MPEG-3 encoded files  */
	{ "ps",   "application/postscript" }, /* Postscript Language   */
	{ "zip",  "application/zip"  }, /* ZIP archive                 */
	{ "doc",  "application/msword"},/* assume Microsoft Word       */
	{ NULL,   NULL               }
};

/*
 * the order of the following two tables must match the
 * Encoding Types definition in uudeview.h
 */

/*
 * encoded bytes per line
 */

static int bpl[8] = { 0, 45, 57, 45, 45, 0, 0, 128 };

/*
 * tables
 */

static unsigned char *etables[5] = {
  NULL,
  UUEncodeTable,
  B64EncodeTable,
  XXEncodeTable,
  BHEncodeTable
};

/*
 * variables to malloc upon initialization
 */

char *uuestr_itemp;
char *uuestr_otemp;

/*
 * Encode one part of the data stream
 */

static int 
UUEncodeStream (FILE *outfile, FILE *infile, int encoding, long linperfile, crc32_t *crc, crc32_t *pcrc)
{
  uchar *itemp = (uchar *) uuestr_itemp;
  uchar *otemp = (uchar *) uuestr_otemp;
  unsigned char *optr, *table, *tptr;
  int index, count;
  long line=0;
  size_t llen;

  if (outfile==NULL || infile==NULL ||
      (encoding!=UU_ENCODED&&encoding!=XX_ENCODED&&encoding!=B64ENCODED&&
       encoding!=PT_ENCODED&&encoding!=QP_ENCODED&&encoding!=YENC_ENCODED)) {
    UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
	       uustring (S_PARM_CHECK), "UUEncodeStream()");
    return UURET_ILLVAL;
  }

  /*
   * Special handling for plain text and quoted printable. Text is
   * read line oriented.
   */

  if (encoding == PT_ENCODED || encoding == QP_ENCODED) {
    while (!feof (infile) && (linperfile <= 0 || line < linperfile)) {
      if (_FP_fgets (itemp, 255, infile) == NULL) {
	break;
      }

      itemp[255] = '\0';
      count = strlen (itemp);

      llen = 0;
      optr = otemp;

      /*
       * Busy Callback
       */
      
      if (UUBUSYPOLL(ftell(infile)-progress.foffset,progress.fsize)) {
	UUMessage (uuencode_id, __LINE__, UUMSG_NOTE,
		   uustring (S_ENCODE_CANCEL));
	return UURET_CANCEL;
      }

      if (encoding == PT_ENCODED) {
	/*
	 * If there is a line feed, replace by eolstring
	 */
	if (count > 0 && itemp[count-1] == '\n') {
	  itemp[--count] = '\0';
	  if (fwrite (itemp, 1, count, outfile) != count ||
	      fwrite ((char *) eolstring, 1,
		      strlen(eolstring), outfile) != strlen (eolstring)) {
	    return UURET_IOERR;
	  }
	}
	else {
	  if (fwrite (itemp, 1, count, outfile) != llen) {
	    return UURET_IOERR;
	  }
	}
      }
      else if (encoding == QP_ENCODED) {
	for (index=0; index<count; index++) {
	  if (llen == 0 && itemp[index] == '.') {
	    /*
	     * Special rule: encode '.' at the beginning of a line, so
	     * that some mailers aren't confused.
	     */
	    *optr++ = '=';
	    *optr++ = HexEncodeTable[itemp[index] >> 4];
	    *optr++ = HexEncodeTable[itemp[index] & 0x0f];
	    llen += 3;
	  }
	  else if ((itemp[index] >= 33 && itemp[index] <= 60) ||
		   (itemp[index] >= 62 && itemp[index] <= 126) ||
		   itemp[index] == 9 || itemp[index] == 32) {
	    *optr++ = itemp[index];
	    llen++;
	  }
	  else if (itemp[index] == '\n') {
	    /*
	     * If the last character before EOL was a space or tab,
	     * we must encode it. If llen > 74, there's no space to do
	     * that, so generate a soft line break instead.
	     */

	    if (index>0 && (itemp[index-1] == 9 || itemp[index-1] == 32)) {
	      *(optr-1) = '=';
	      if (llen <= 74) {
		*optr++ = HexEncodeTable[itemp[index-1] >> 4];
		*optr++ = HexEncodeTable[itemp[index-1] & 0x0f];
		llen += 2;
	      }
	    }

	    if (fwrite (otemp, 1, llen, outfile) != llen ||
		fwrite ((char *) eolstring, 1,
			strlen(eolstring), outfile) != strlen (eolstring)) {
	      return UURET_IOERR;
	    }

	    /*
	     * Fix the soft line break condition from above
	     */

	    if (index>0 && (itemp[index-1] == 9 || itemp[index-1] == 32) &&
		*(optr-1) == '=') {
	      otemp[0] = '=';
	      otemp[1] = HexEncodeTable[itemp[index-1] >> 4];
	      otemp[2] = HexEncodeTable[itemp[index-1] & 0x0f];

	      if (fwrite (otemp, 1, 3, outfile) != 3 ||
		  fwrite ((char *) eolstring, 1,
			  strlen(eolstring), outfile) != strlen (eolstring)) {
		return UURET_IOERR;
	      }
	    }

	    optr = otemp;
	    llen = 0;
	  }
	  else {
	    *optr++ = '=';
	    *optr++ = HexEncodeTable[itemp[index] >> 4];
	    *optr++ = HexEncodeTable[itemp[index] & 0x0f];
	    llen += 3;
	  }

	  /*
	   * Lines must be shorter than 76 characters (not counting CRLF).
	   * If the line grows longer than that, we must include a soft
	   * line break.
	   */

	  if (itemp[index+1] != 0 && itemp[index+1] != '\n' &&
	      (llen >= 75 ||
	       (!((itemp[index+1] >= 33 && itemp[index+1] <= 60) ||
		  (itemp[index+1] >= 62 && itemp[index+1] <= 126)) &&
		llen >= 73))) {

	    *optr++ = '=';
	    llen++;
	    
	    if (fwrite (otemp, 1, llen, outfile) != llen ||
		fwrite ((char *) eolstring, 1,
			strlen(eolstring), outfile) != strlen (eolstring)) {
	      return UURET_IOERR;
	    }
	    
	    optr = otemp;
	    llen = 0;
	  }
	}
      }

      line++;
    }

    return UURET_OK;
  }

  /*
   * Special handling for yEnc
   */

  if (encoding == YENC_ENCODED) {
    llen = 0;
    optr = otemp;

    while (!feof (infile) && (linperfile <= 0 || line < linperfile)) {
      if ((count = fread (itemp, 1, 128, infile)) != 128) {
	if (count == 0) {
	  break;
	}
	else if (ferror (infile)) {
	  return UURET_IOERR;
	}
      }

      if (pcrc)
	*pcrc = crc32(*pcrc, itemp, count);
      if (crc)
	*crc = crc32(*crc, itemp, count);

      line++;

      /*
       * Busy Callback
       */
      
      if (UUBUSYPOLL(ftell(infile)-progress.foffset,progress.fsize)) {
	UUMessage (uuencode_id, __LINE__, UUMSG_NOTE,
		   uustring (S_ENCODE_CANCEL));
	return UURET_CANCEL;
      }

      for (index=0; index<count; index++) {
	if (llen > 127) {
	  if (fwrite (otemp, 1, llen, outfile) != llen ||
	      fwrite ((char *) eolstring, 1,
		      strlen(eolstring), outfile) != strlen (eolstring)) {
	    return UURET_IOERR;
	  }
	  llen = 0;
	  optr = otemp;
	}

	switch ((char) ((int) itemp[index] + 42)) {
	case '\0':
	case '\t':
	case '\n':
	case '\r':
	case '=':
	case '\033':
	  *optr++ = '=';
	  *optr++ = (char) ((int) itemp[index] + 42 + 64);
	  llen += 2;
	  break;

	case '.':
	  if (llen == 0) {
	    *optr++ = '=';
	    *optr++ = (char) ((int) itemp[index] + 42 + 64);
	    llen += 2;
	  }
	  else {
	    *optr++ = (char) ((int) itemp[index] + 42);
	    llen++;
	  }
	  break;

	default:
	  *optr++ = (char) ((int) itemp[index] + 42);
	  llen++;
	  break;
	}
      }
    }

    /*
     * write last line
     */

    if (llen) {
      if (fwrite (otemp, 1, llen, outfile) != llen ||
	  fwrite ((char *) eolstring, 1,
		  strlen(eolstring), outfile) != strlen (eolstring)) {
	return UURET_IOERR;
      }
    }

    return UURET_OK;
  }

  /*
   * Handling for binary encodings
   */

  /*
   * select charset
   */

  table = etables[encoding];

  if (table==NULL || bpl[encoding]==0) {
    UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
               uustring (S_PARM_CHECK), "UUEncodeStream()");
    return UURET_ILLVAL;
  }

  while (!feof (infile) && (linperfile <= 0 || line < linperfile)) {
    if ((count = fread (itemp, 1, bpl[encoding], infile)) != bpl[encoding]) {
      if (count == 0)
	break;
      else if (ferror (infile))
	return UURET_IOERR;
    }

    optr = otemp;
    llen = 0;

    /*
     * Busy Callback
     */

    if (UUBUSYPOLL(ftell(infile)-progress.foffset,progress.fsize)) {
      UUMessage (uuencode_id, __LINE__, UUMSG_NOTE,
		 uustring (S_ENCODE_CANCEL));
      return UURET_CANCEL;
    }

    /*
     * for UU and XX, encode the number of bytes as first character
     */

    if (encoding == UU_ENCODED || encoding == XX_ENCODED) {
      *optr++ = table[count];
      llen++;
    }

    /*
     * Main encoding
     */

    for (index=0; index<=count-3; index+=3, llen+=4) {
      *optr++ = table[itemp[index] >> 2];
      *optr++ = table[((itemp[index  ] & 0x03) << 4)|(itemp[index+1] >> 4)];
      *optr++ = table[((itemp[index+1] & 0x0f) << 2)|(itemp[index+2] >> 6)];
      *optr++ = table[  itemp[index+2] & 0x3f];
    }

    /*
     * Special handling for incomplete lines
     */

    if (index != count) {
      if (encoding == B64ENCODED) {
	if (count - index == 2) {
	  *optr++ = table[itemp[index] >> 2];
	  *optr++ = table[((itemp[index  ] & 0x03) << 4) | 
			  ((itemp[index+1] & 0xf0) >> 4)];
	  *optr++ = table[((itemp[index+1] & 0x0f) << 2)];
	  *optr++ = '=';
	}
	else if (count - index == 1) {
	  *optr++ = table[ itemp[index] >> 2];
	  *optr++ = table[(itemp[index] & 0x03) << 4];
	  *optr++ = '=';
	  *optr++ = '=';
	}
	llen += 4;
      }
      else if (encoding == UU_ENCODED || encoding == XX_ENCODED) {
	if (count - index == 2) {
	  *optr++ = table[itemp[index] >> 2];
	  *optr++ = table[((itemp[index  ] & 0x03) << 4) | 
			  ( itemp[index+1] >> 4)];
	  *optr++ = table[((itemp[index+1] & 0x0f) << 2)];
	  *optr++ = table[0];
	}
	else if (count - index == 1) {
	  *optr++ = table[ itemp[index] >> 2];
	  *optr++ = table[(itemp[index] & 0x03) << 4];
	  *optr++ = table[0];
	  *optr++ = table[0];
	}
	llen += 4;
      }
    }

    /*
     * end of line
     */

    tptr = eolstring;

    while (*tptr)
      *optr++ = *tptr++;

    *optr++ = '\0';
    llen   += strlen ((char *) eolstring);

    if (fwrite (otemp, 1, llen, outfile) != llen)
      return UURET_IOERR;

    line++;
  }
  return UURET_OK;
}

/*
 * Encode as MIME multipart/mixed sub-message.
 */

int UUEXPORT
UUEncodeMulti (FILE *outfile, FILE *infile, char *infname, int encoding,
	       char *outfname, char *mimetype, int filemode)
{
  mimemap *miter=mimetable;
  struct stat finfo;
  int res, themode;
  FILE *theifile;
  char *ptr;
  crc32_t crc;
  crc32_t *crcptr=NULL;

  if (outfile==NULL || 
      (infile == NULL && infname==NULL) ||
      (outfname==NULL && infname==NULL) ||
      (encoding!=UU_ENCODED&&encoding!=XX_ENCODED&&encoding!=B64ENCODED&&
       encoding!=PT_ENCODED&&encoding!=QP_ENCODED&&encoding!=YENC_ENCODED)) {
    UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
	       uustring (S_PARM_CHECK), "UUEncodeMulti()");
    return UURET_ILLVAL;
  }

  progress.action = 0;

  if (infile==NULL) {
    if (stat (infname, &finfo) == -1) {
      UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
		 uustring (S_NOT_STAT_FILE),
		 infname, strerror (uu_errno=errno));
      return UURET_IOERR;
    }
    if ((theifile = fopen (infname, "rb")) == NULL) {
      UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
		 uustring (S_NOT_OPEN_FILE),
		 infname, strerror (uu_errno=errno));
      return UURET_IOERR;
    }
    themode = (filemode) ? filemode : ((int) finfo.st_mode & 0777);
    progress.fsize = (long) finfo.st_size;
  }
  else {
    if (fstat (fileno (infile), &finfo) != 0) {
      themode  = (filemode)?filemode:0644;
      progress.fsize = -1;
    }
    else {
      themode = (int) finfo.st_mode & 0777;
      progress.fsize = (long) finfo.st_size;
    }
    theifile = infile;
  }

  if (progress.fsize < 0)
    progress.fsize = -1;

  _FP_strncpy (progress.curfile, (outfname)?outfname:infname, 256);

  progress.partno   = 1;
  progress.numparts = 1;
  progress.percent  = 0;
  progress.foffset  = 0;
  progress.action   = UUACT_ENCODING;

  /*
   * If not given from outside, select an appropriate Content-Type by
   * looking at the file's extension. If it is unknown, default to
   * Application/Octet-Stream
   */

  if (mimetype == NULL) {
    if ((ptr = _FP_strrchr ((outfname)?outfname:infname, '.'))) {
      while (miter->extension && _FP_stricmp (ptr+1, miter->extension) != 0)
	miter++;
      mimetype = miter->mimetype;
    }
  }

  if (mimetype == NULL && (encoding == PT_ENCODED || encoding == QP_ENCODED)) {
    mimetype = "text/plain";
  }

  /*
   * print sub-header
   */

  if (encoding != YENC_ENCODED) {
    fprintf (outfile, "Content-Type: %s%s",
	     (mimetype)?mimetype:"Application/Octet-Stream",
	     eolstring);
    fprintf (outfile, "Content-Transfer-Encoding: %s%s",
	     CTE_TYPE(encoding), eolstring);
    fprintf (outfile, "Content-Disposition: attachment; filename=\"%s\"%s",
	     UUFNameFilter ((outfname)?outfname:infname), eolstring);
    fprintf (outfile, "%s", eolstring);
  }

  if (encoding == UU_ENCODED || encoding == XX_ENCODED) {
    fprintf (outfile, "begin %o %s%s",
	     (themode) ? themode : 0644,
	     UUFNameFilter ((outfname)?outfname:infname), 
	     eolstring);
  }
  else if (encoding == YENC_ENCODED) {
    crc = crc32(0L, Z_NULL, 0);
    crcptr = &crc;
    if (progress.fsize == -1) {
      fprintf (outfile, "=ybegin line=128 name=%s%s",
	       UUFNameFilter ((outfname)?outfname:infname), 
	       eolstring);
    }
    else {
      fprintf (outfile, "=ybegin line=128 size=%ld name=%s%s",
	       progress.fsize,
	       UUFNameFilter ((outfname)?outfname:infname), 
	       eolstring);
    }
  }

  if ((res = UUEncodeStream (outfile, theifile, encoding, 0, crcptr, NULL)) != UURET_OK) {
    if (res != UURET_CANCEL) {
      UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
		 uustring (S_ERR_ENCODING),
		 UUFNameFilter ((infname)?infname:outfname),
		 (res==UURET_IOERR)?strerror(uu_errno):UUstrerror(res));
    }
    progress.action = 0;
    return res;
  }

  if (encoding == UU_ENCODED || encoding == XX_ENCODED) {
    fprintf (outfile, "%c%s",    
	     (encoding==UU_ENCODED) ? UUEncodeTable[0] : XXEncodeTable[0], 
	     eolstring);
    fprintf (outfile, "end%s", eolstring);
  }
  else if (encoding == YENC_ENCODED) {
    if (progress.fsize == -1) {
      fprintf (outfile, "=yend crc32=%08lx%s",
	       crc,
	       eolstring);
    }
    else {
      fprintf (outfile, "=yend size=%ld crc32=%08lx%s",
	       progress.fsize,
	       crc,
	       eolstring);
    }
  }

  /*
   * empty line at end does no harm
   */

  fprintf (outfile, "%s", eolstring);

  if (infile==NULL)
    fclose (theifile);

  progress.action = 0;
  return UURET_OK;
}

/*
 * Encode as MIME message/partial
 */

int UUEXPORT
UUEncodePartial (FILE *outfile, FILE *infile,
		 char *infname, int encoding,
		 char *outfname, char *mimetype,
		 int filemode, int partno, long linperfile,
		 crc32_t *crcptr)
{
  mimemap *miter=mimetable;
  static FILE *theifile;
  int themode, numparts;
  struct stat finfo;
  long thesize;
  char *ptr;
  int res;
  crc32_t pcrc;
  crc32_t *pcrcptr=NULL;

  if ((outfname==NULL&&infname==NULL) || partno<=0 ||
      (infile == NULL&&infname==NULL) || outfile==NULL ||
      (encoding!=UU_ENCODED&&encoding!=XX_ENCODED&&encoding!=B64ENCODED&&
       encoding!=PT_ENCODED&&encoding!=QP_ENCODED&&encoding!=YENC_ENCODED)) {
    UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
	       uustring (S_PARM_CHECK), "UUEncodePartial()");
    return UURET_ILLVAL;
  }

  /*
   * The first part needs a set of headers
   */

  progress.action = 0;

  if (partno == 1) {
    if (infile==NULL) {
      if (stat (infname, &finfo) == -1) {
	UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
		   uustring (S_NOT_STAT_FILE),
		   infname, strerror (uu_errno=errno));
	return UURET_IOERR;
      }
      if ((theifile = fopen (infname, "rb")) == NULL) {
	UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
		   uustring (S_NOT_OPEN_FILE),
		   infname, strerror (uu_errno=errno));
	return UURET_IOERR;
      }
      if (linperfile <= 0)
	numparts = 1;
      else 
	numparts = (int) (((long)finfo.st_size+(linperfile*bpl[encoding]-1))/
			  (linperfile*bpl[encoding]));

      themode  = (filemode) ? filemode : ((int) finfo.st_mode & 0777);
      thesize  = (long) finfo.st_size;
    }
    else {
      if (fstat (fileno (infile), &finfo) != 0) {
	UUMessage (uuencode_id, __LINE__, UUMSG_WARNING,
		   uustring (S_STAT_ONE_PART));
	numparts = 1;
	themode  = (filemode)?filemode:0644;
	thesize  = -1;
      }
      else {
	if (linperfile <= 0)
	  numparts = 1;
	else
	  numparts = (int) (((long)finfo.st_size+(linperfile*bpl[encoding]-1))/
			    (linperfile*bpl[encoding]));

	themode =  (int) finfo.st_mode & 0777;
	thesize = (long) finfo.st_size;
      }
      theifile = infile;
    }

    _FP_strncpy (progress.curfile, (outfname)?outfname:infname, 256);

    progress.totsize  = (thesize>=0) ? thesize : -1;
    progress.partno   = 1;
    progress.numparts = numparts;
    progress.percent  = 0;
    progress.foffset  = 0;

    /*
     * If not given from outside, select an appropriate Content-Type by
     * looking at the file's extension. If it is unknown, default to
     * Application/Octet-Stream
     */

    if (mimetype == NULL) {
      if ((ptr = _FP_strrchr ((outfname)?outfname:infname, '.'))) {
	while (miter->extension && _FP_stricmp (ptr+1, miter->extension) != 0)
	  miter++;
	mimetype = miter->mimetype;
      }
    }

    if (mimetype == NULL && (encoding==PT_ENCODED || encoding==QP_ENCODED)) {
      mimetype = "text/plain";
    }

    /*
     * print sub-header
     */

    if (encoding != YENC_ENCODED) {
      fprintf (outfile, "MIME-Version: 1.0%s", eolstring);
      fprintf (outfile, "Content-Type: %s%s",
	       (mimetype)?mimetype:"Application/Octet-Stream",
	       eolstring);
      fprintf (outfile, "Content-Transfer-Encoding: %s%s",
	       CTE_TYPE(encoding), eolstring);
      fprintf (outfile, "Content-Disposition: attachment; filename=\"%s\"%s",
	       UUFNameFilter ((outfname)?outfname:infname), eolstring);
    }

    fprintf (outfile, "%s", eolstring);
    
    /*
     * for the first part of UU or XX messages, print a begin line
     */

    if (encoding == UU_ENCODED || encoding == XX_ENCODED) {
      fprintf (outfile, "begin %o %s%s",
	       (themode) ? themode : ((filemode)?filemode:0644),
	       UUFNameFilter ((outfname)?outfname:infname), eolstring);
    }
  }
  if (encoding == YENC_ENCODED) {
    pcrc = crc32(0L, Z_NULL, 0);
    pcrcptr = &pcrc;
    if (numparts != 1) {
      if (progress.totsize == -1) {
	fprintf (outfile, "=ybegin part=%d line=128 name=%s%s",
		 partno,
		 UUFNameFilter ((outfname)?outfname:infname), 
		 eolstring);
      }
      else {
	fprintf (outfile, "=ybegin part=%d line=128 size=%ld name=%s%s",
		 partno,
		 progress.totsize,
		 UUFNameFilter ((outfname)?outfname:infname), 
		 eolstring);
      }

      fprintf (outfile, "=ypart begin=%d end=%d%s",
	       (partno-1)*linperfile*128+1,
	       (partno*linperfile*128) < progress.totsize ? 
	       (partno*linperfile*128) : progress.totsize,
	       eolstring);
    }
    else {
      if (progress.totsize == -1) {
	fprintf (outfile, "=ybegin line=128 name=%s%s",
		 UUFNameFilter ((outfname)?outfname:infname), 
		 eolstring);
      }
      else {
	fprintf (outfile, "=ybegin line=128 size=%ld name=%s%s",
		 progress.totsize,
		 UUFNameFilter ((outfname)?outfname:infname), 
		 eolstring);
      }
    }
  }

  /*
   * update progress information
   */

  progress.partno  = partno;
  progress.percent = 0;
  progress.foffset = ftell (theifile);

  if (progress.totsize <= 0)
    progress.fsize = -1;
  else if (linperfile <= 0)
    progress.fsize = progress.totsize;
  else if (progress.foffset+linperfile*bpl[encoding] > progress.totsize)
    progress.fsize = progress.totsize - progress.foffset;
  else
    progress.fsize = linperfile*bpl[encoding];

  progress.action  = UUACT_ENCODING;

  if ((res = UUEncodeStream (outfile, theifile, encoding, linperfile,
			     crcptr, pcrcptr)) != UURET_OK) {
    if (infile==NULL) fclose (theifile);
    if (res != UURET_CANCEL) {
      UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
		 uustring (S_ERR_ENCODING),
		 UUFNameFilter ((outfname)?outfname:infname),
		 (res==UURET_IOERR)?strerror(uu_errno):UUstrerror (res));
    }
    progress.action = 0;
    return res;
  }

  /*
   * print end line
   */

  if (feof (theifile) &&
      (encoding == UU_ENCODED || encoding == XX_ENCODED)) {
    fprintf (outfile, "%c%s",    
	     (encoding==UU_ENCODED) ? UUEncodeTable[0] : XXEncodeTable[0], 
	     eolstring);
    fprintf (outfile, "end%s", eolstring);
  }
  else if (encoding == YENC_ENCODED) {
    if (numparts != 1) {
      fprintf (outfile, "=yend size=%d part=%d pcrc32=%08lx",
	       (partno*linperfile*128) < progress.totsize ? 
	       linperfile*128 : (progress.totsize-(partno-1)*linperfile*128),
	       partno,
	       pcrc);
    }
    else {
      fprintf (outfile, "=yend size=%d",
	       progress.totsize);
    }
    if (feof (theifile))
      fprintf (outfile, " crc32=%08lx", *crcptr);
    fprintf (outfile, "%s", eolstring);
  }

  /*
   * empty line at end does no harm
   */

  if (encoding != PT_ENCODED && encoding != QP_ENCODED) {
    fprintf (outfile, "%s", eolstring);
  }

  if (infile==NULL) {
    if (res != UURET_OK) {
      progress.action = 0;
      fclose (theifile);
      return res;
    }
    if (feof (theifile)) {
      progress.action = 0;
      fclose (theifile);
      return UURET_OK;
    }
    return UURET_CONT;
  }

  /*
   * leave progress.action as-is
   */

  return UURET_OK;
}

/*
 * send output to a stream, don't do any headers at all
 */

int UUEXPORT
UUEncodeToStream (FILE *outfile, FILE *infile,
		  char *infname, int encoding,
		  char *outfname, int filemode)
{
  struct stat finfo;
  FILE *theifile;
  int themode;
  int res;
  crc32_t crc;
  crc32_t *crcptr=NULL;

  if (outfile==NULL ||
      (infile == NULL&&infname==NULL) ||
      (outfname==NULL&&infname==NULL) ||
      (encoding!=UU_ENCODED&&encoding!=XX_ENCODED&&encoding!=B64ENCODED&&
       encoding!=PT_ENCODED&&encoding!=QP_ENCODED&&encoding!=YENC_ENCODED)) {
    UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
	       uustring (S_PARM_CHECK), "UUEncodeToStream()");
    return UURET_ILLVAL;
  }

  progress.action = 0;

  if (infile==NULL) {
    if (stat (infname, &finfo) == -1) {
      UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
		 uustring (S_NOT_STAT_FILE),
		 infname, strerror (uu_errno=errno));
      return UURET_IOERR;
    }
    if ((theifile = fopen (infname, "rb")) == NULL) {
      UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
		 uustring (S_NOT_OPEN_FILE),
		 infname, strerror (uu_errno=errno));
      return UURET_IOERR;
    }
    themode = (filemode) ? filemode : ((int) finfo.st_mode & 0777);
    progress.fsize = (long) finfo.st_size;
  }
  else {
    if (fstat (fileno (infile), &finfo) == -1) {
      /* gotta live with it */
      themode = 0644;
      progress.fsize = -1;
    }
    else {
      themode = (filemode) ? filemode : ((int) finfo.st_mode & 0777);
      progress.fsize = (long) finfo.st_size;
    }
    theifile = infile;
  }

  if (progress.fsize < 0)
    progress.fsize = -1;

  _FP_strncpy (progress.curfile, (outfname)?outfname:infname, 256);

  progress.partno   = 1;
  progress.numparts = 1;
  progress.percent  = 0;
  progress.foffset  = 0;
  progress.action   = UUACT_ENCODING;

  if (encoding == UU_ENCODED || encoding == XX_ENCODED) {
    fprintf (outfile, "begin %o %s%s",
	     (themode) ? themode : 0644,
	     UUFNameFilter ((outfname)?outfname:infname), 
	     eolstring);
  }
  else if (encoding == YENC_ENCODED) {
    crc = crc32(0L, Z_NULL, 0);
    crcptr = &crc;
    if (progress.fsize == -1) {
      fprintf (outfile, "=ybegin line=128 name=%s%s",
	       UUFNameFilter ((outfname)?outfname:infname), 
	       eolstring);
    }
    else {
      fprintf (outfile, "=ybegin line=128 size=%ld name=%s%s",
	       progress.fsize,
	       UUFNameFilter ((outfname)?outfname:infname), 
	       eolstring);
    }
  }

  if ((res = UUEncodeStream (outfile, theifile, encoding, 0, crcptr, NULL)) != UURET_OK) {
    if (res != UURET_CANCEL) {
      UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
		 uustring (S_ERR_ENCODING),
		 UUFNameFilter ((infname)?infname:outfname), 
		 (res==UURET_IOERR)?strerror(uu_errno):UUstrerror (res));
    }
    progress.action = 0;
    return res;
  }

  if (encoding == UU_ENCODED || encoding == XX_ENCODED) {
    fprintf (outfile, "%c%s",    
	     (encoding==UU_ENCODED) ? UUEncodeTable[0] : XXEncodeTable[0], 
	     eolstring);
    fprintf (outfile, "end%s", eolstring);
  }
  else if (encoding == YENC_ENCODED) {
    if (progress.fsize == -1) {
      fprintf (outfile, "=yend crc32=%08lx%s",
	       crc,
	       eolstring);
    }
    else {
      fprintf (outfile, "=yend size=%ld crc32=%08lx%s",
	       progress.fsize,
	       crc,
	       eolstring);
    }
  }

  /*
   * empty line at end does no harm
   */

  fprintf (outfile, "%s", eolstring);

  if (infile==NULL) fclose (theifile);
  progress.action = 0;

  return UURET_OK;
}

/*
 * Encode to files on disk, don't generate any headers
 */

int UUEXPORT
UUEncodeToFile (FILE *infile, char *infname, int encoding,
		char *outfname, char *diskname, long linperfile)
{
  int part, numparts, len, filemode, res;
  char *oname=NULL, *optr, *ptr;
  FILE *theifile, *outfile;
  struct stat finfo;
  crc32_t pcrc, crc;
  crc32_t *pcrcptr=NULL, *crcptr=NULL;

  if ((diskname==NULL&&infname==NULL) ||
      (outfname==NULL&&infname==NULL) || (infile==NULL&&infname==NULL) ||
      (encoding!=UU_ENCODED&&encoding!=XX_ENCODED&&encoding!=B64ENCODED&&
       encoding!=PT_ENCODED&&encoding!=QP_ENCODED&&encoding!=YENC_ENCODED)) {
    UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
	       uustring (S_PARM_CHECK), "UUEncodeToFile()");
    return UURET_ILLVAL;
  }

  if (diskname) {
    if ((ptr = strchr (diskname, '/')) == NULL)
      ptr = strchr (diskname, '\\');
    if (ptr) {
      len = strlen (diskname) + ((uuencodeext)?strlen(uuencodeext):3) + 5;

      if ((oname = malloc (len)) == NULL) {
	UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
		   uustring (S_OUT_OF_MEMORY), len);
	return UURET_NOMEM;
      }
      sprintf (oname, "%s", diskname);
    }
    else {
      len = ((uusavepath)?strlen(uusavepath):0) + strlen (diskname) 
	+ ((uuencodeext)?strlen(uuencodeext):0) + 5;

      if ((oname = malloc (len)) == NULL) {
	UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
		   uustring (S_OUT_OF_MEMORY), len);
	return UURET_NOMEM;
      }
      sprintf (oname, "%s%s", (uusavepath)?uusavepath:"", diskname);
    }
  }
  else {
    len = ((uusavepath) ? strlen (uusavepath) : 0) + 
      strlen(UUFNameFilter(infname)) +
	((uuencodeext)?strlen(uuencodeext):0) + 5;

    if ((oname = malloc (len)) == NULL) {
      UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
		 uustring (S_OUT_OF_MEMORY), len);
      return UURET_NOMEM;
    }
    optr = UUFNameFilter (infname);
    sprintf (oname, "%s%s", 
	     (uusavepath)?uusavepath:"",
	     (*optr=='.')?optr+1:optr);
  }

  /*
   * optr points after the last dot, so that we can print the part number
   * there.
   */

  optr = _FP_strrchr (oname, '.');
  if (optr==NULL || strchr (optr, '/')!=NULL || strchr (optr, '\\')!=NULL) {
    optr = oname + strlen (oname);
    *optr++ = '.';
  }
  else if (optr==oname || *(optr-1)=='/' || *(optr-1)=='\\') {
    optr = oname + strlen (oname);
    *optr++ = '.';
  }
  else
    optr++;

  progress.action = 0;

  if (infile==NULL) {
    if (stat (infname, &finfo) == -1) {
      UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
		 uustring (S_NOT_STAT_FILE),
		 infname, strerror (uu_errno=errno));
      _FP_free (oname);
      return UURET_IOERR;
    }
    if ((theifile = fopen (infname, "rb")) == NULL) {
      UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
		 uustring (S_NOT_OPEN_FILE),
		 infname, strerror (uu_errno=errno));
      _FP_free (oname);
      return UURET_IOERR;
    }
    if (linperfile <= 0)
      numparts = 1;
    else 
      numparts = (int) (((long)finfo.st_size + (linperfile*bpl[encoding]-1)) /
			(linperfile*bpl[encoding]));

    filemode = (int) finfo.st_mode & 0777;
    progress.totsize = (long) finfo.st_size;
  }
  else {
    if (fstat (fileno (infile), &finfo) == -1) {
      /* gotta live with it */
      filemode = 0644;
      numparts = -1;
      progress.totsize = -1;
    }
    else {
      if (linperfile <= 0)
	numparts = 1;
      else
	numparts = (int) (((long)finfo.st_size+(linperfile*bpl[encoding]-1))/
			  (linperfile*bpl[encoding]));

      filemode = (int) finfo.st_mode & 0777;
      progress.totsize = -1;
    }
    theifile = infile;
  }

  _FP_strncpy (progress.curfile, (outfname)?outfname:infname, 256);

  progress.totsize  = (progress.totsize<0) ? -1 : progress.totsize;
  progress.numparts = numparts;

  for (part=1; !feof (theifile); part++) {
    /*
     * Attach extension
     */
    if (progress.numparts==1 && progress.totsize!=-1 && uuencodeext!=NULL)
      strcpy  (optr, uuencodeext);
    else 
      sprintf (optr, "%03d", part);

    /*
     * check if target file exists
     */

    if (!uu_overwrite) {
      if (stat (oname, &finfo) == 0) {
	UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
		   uustring (S_TARGET_EXISTS), oname);
	if (infile==NULL) fclose (theifile);
	progress.action = 0;
	free (oname);
	return UURET_EXISTS;
      }
    }

    /*
     * update progress information
     */

    progress.action  = 0;
    progress.partno  = part;
    progress.percent = 0;
    progress.foffset = ftell (theifile);

    if (progress.totsize == -1)
      progress.fsize = -1;
    else if (linperfile <= 0)
      progress.fsize = progress.totsize;
    else if (progress.foffset+linperfile*bpl[encoding] > progress.totsize)
      progress.fsize = progress.totsize - progress.foffset;
    else
      progress.fsize = linperfile*bpl[encoding];

    progress.action  = UUACT_ENCODING;

    if ((outfile = fopen (oname, "w")) == NULL) {
      UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
		 uustring (S_NOT_OPEN_TARGET),
		 oname, strerror (uu_errno = errno));
      if (infile==NULL) fclose (theifile);
      progress.action = 0;
      free (oname);
      return UURET_IOERR;
    }

    if (encoding != YENC_ENCODED) {
      fprintf (outfile, "%s", eolstring);
      fprintf (outfile, "_=_ %s", eolstring);
      if (numparts == -1)
	fprintf (outfile, "_=_ Part %03d of file %s%s",
		 part, UUFNameFilter ((outfname)?outfname:infname),
		 eolstring);
      else
	fprintf (outfile, "_=_ Part %03d of %03d of file %s%s",
		 part, numparts,
		 UUFNameFilter ((outfname)?outfname:infname),
		 eolstring);
      fprintf (outfile, "_=_ %s", eolstring);
      fprintf (outfile, "%s", eolstring);
    }

    if (part==1 && (encoding == UU_ENCODED || encoding == XX_ENCODED)) {
      fprintf (outfile, "begin %o %s%s",
	       (filemode)?filemode : 0644,
	       UUFNameFilter ((outfname)?outfname:infname), 
	       eolstring);
    }
    else if (encoding == YENC_ENCODED) {
      if (!crcptr) {
        crc = crc32(0L, Z_NULL, 0);
        crcptr = &crc;
      }
      pcrc = crc32(0L, Z_NULL, 0);
      pcrcptr = &pcrc;
      if (numparts != 1) {
	if (progress.totsize == -1) {
	  fprintf (outfile, "=ybegin part=%d line=128 name=%s%s",
		   part,
		   UUFNameFilter ((outfname)?outfname:infname), 
		   eolstring);
	}
	else {
	  fprintf (outfile, "=ybegin part=%d line=128 size=%ld name=%s%s",
		   part,
		   progress.totsize,
		   UUFNameFilter ((outfname)?outfname:infname), 
		   eolstring);
	}

	fprintf (outfile, "=ypart begin=%d end=%d%s",
		 (part-1)*linperfile*128+1,
		 (part*linperfile*128) < progress.totsize ? 
		 (part*linperfile*128) : progress.totsize,
		 eolstring);
      }
      else {
	if (progress.totsize == -1) {
	  fprintf (outfile, "=ybegin line=128 name=%s%s",
		   UUFNameFilter ((outfname)?outfname:infname), 
		   eolstring);
	}
	else {
	  fprintf (outfile, "=ybegin line=128 size=%ld name=%s%s",
		   progress.totsize,
		   UUFNameFilter ((outfname)?outfname:infname), 
		   eolstring);
	}
      }
    }

    if ((res = UUEncodeStream (outfile, theifile,
			       encoding, linperfile, crcptr, pcrcptr)) != UURET_OK) {
      if (res != UURET_CANCEL) {
	UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
		   uustring (S_ERR_ENCODING),
		   UUFNameFilter ((infname)?infname:outfname),	 
		   (res==UURET_IOERR)?strerror(uu_errno):UUstrerror (res));
      }
      if (infile==NULL) fclose (theifile);
      progress.action = 0;
      fclose (outfile);
      unlink (oname);
      _FP_free (oname);
      return res;
    }

    if (feof (theifile) &&
	(encoding == UU_ENCODED || encoding == XX_ENCODED)) {
      fprintf (outfile, "%c%s",    
	       (encoding==UU_ENCODED) ? UUEncodeTable[0] : XXEncodeTable[0], 
	       eolstring);
      fprintf (outfile, "end%s", eolstring);
    }
    else if (encoding == YENC_ENCODED) {
      if (numparts != 1) {
	fprintf (outfile, "=yend size=%d part=%d pcrc32=%08lx",
		 (part*linperfile*128) < progress.totsize ? 
		 linperfile*128 : (progress.totsize-(part-1)*linperfile*128),
		 part,
		 pcrc);
      }
      else {
	fprintf (outfile, "=yend size=%d",
		 progress.totsize);
      }
      if (feof (theifile))
	fprintf (outfile, " crc32=%08lx", crc); 
      fprintf (outfile, "%s", eolstring);
    }

    /*
     * empty line at end does no harm
     */

    fprintf (outfile, "%s", eolstring);
    fclose  (outfile);
  }

  if (infile==NULL) fclose (theifile);
  progress.action = 0;
  _FP_free (oname);
  return UURET_OK;
}

/*
 * Encode a MIME Mail message or Newsgroup posting and send to a
 * stream. Still needs a somewhat smart MDA, since we only gene-
 * rate a minimum set of headers.
 */

int UUEXPORT
UUE_PrepSingle (FILE *outfile, FILE *infile,
		char *infname, int encoding,
		char *outfname, int filemode,
		char *destination, char *from,
		char *subject, int isemail)
{
  return UUE_PrepSingleExt (outfile, infile,
			    infname, encoding,
			    outfname, filemode,
			    destination, from,
			    subject, NULL,
			    isemail);
}

int UUEXPORT
UUE_PrepSingleExt (FILE *outfile, FILE *infile,
		   char *infname, int encoding,
		   char *outfname, int filemode,
		   char *destination, char *from,
		   char *subject, char *replyto,
		   int isemail)
{
  mimemap *miter=mimetable;
  char *subline, *oname;
  char *mimetype, *ptr;
  int res, len;

  if ((outfname==NULL&&infname==NULL) || (infile==NULL&&infname==NULL) ||
      (encoding!=UU_ENCODED&&encoding!=XX_ENCODED&&encoding!=B64ENCODED&&
       encoding!=PT_ENCODED&&encoding!=QP_ENCODED&&encoding!=YENC_ENCODED)) {
    UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
	       uustring (S_PARM_CHECK), "UUE_PrepSingle()");
    return UURET_ILLVAL;
  }

  oname = UUFNameFilter ((outfname)?outfname:infname);
  len   = ((subject)?strlen(subject):0) + strlen(oname) + 40;

  if ((ptr = _FP_strrchr (oname, '.'))) {
    while (miter->extension && _FP_stricmp (ptr+1, miter->extension) != 0)
      miter++;
    mimetype = miter->mimetype;
  }
  else
    mimetype = NULL;

  if (mimetype == NULL && (encoding == PT_ENCODED || encoding == QP_ENCODED)) {
    mimetype = "text/plain";
  }

  if ((subline = (char *) malloc (len)) == NULL) {
    UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
	       uustring (S_OUT_OF_MEMORY), len);
    return UURET_NOMEM;
  }

  if (encoding == YENC_ENCODED) {
    if (subject)
      sprintf (subline, "- %s - %s (001/001)", oname, subject);
    else
      sprintf (subline, "- %s - (001/001)", oname);
  }
  else {
    if (subject)
      sprintf (subline, "%s (001/001) - [ %s ]", subject, oname);
    else
      sprintf (subline, "[ %s ] (001/001)", oname);
  }

  if (from) {
    fprintf (outfile, "From: %s%s", from, eolstring);
  }
  if (destination) {
    fprintf (outfile, "%s: %s%s",
	     (isemail)?"To":"Newsgroups",
	     destination, eolstring);
  }

  fprintf (outfile, "Subject: %s%s", subline, eolstring);

  if (replyto) {
    fprintf (outfile, "Reply-To: %s%s", replyto, eolstring);
  }

  if (encoding != YENC_ENCODED) {
    fprintf (outfile, "MIME-Version: 1.0%s", eolstring);
    fprintf (outfile, "Content-Type: %s; name=\"%s\"%s",
	     (mimetype)?mimetype:"Application/Octet-Stream",
	     UUFNameFilter ((outfname)?outfname:infname),
	     eolstring);
    fprintf (outfile, "Content-Transfer-Encoding: %s%s",
	     CTE_TYPE(encoding), eolstring);
  }

  fprintf (outfile, "%s", eolstring);

  res = UUEncodeToStream (outfile, infile, infname, encoding,
			  outfname, filemode);
  
  _FP_free (subline);
  return res;
}

int UUEXPORT
UUE_PrepPartial (FILE *outfile, FILE *infile,
		 char *infname, int encoding,
		 char *outfname, int filemode,
		 int partno, long linperfile, long filesize,
		 char *destination, char *from, char *subject,
		 int isemail)
{
  return UUE_PrepPartialExt (outfile, infile,
			     infname, encoding,
			     outfname, filemode,
			     partno, linperfile, filesize,
			     destination,
			     from, subject, NULL,
			     isemail);
}

int UUEXPORT
UUE_PrepPartialExt (FILE *outfile, FILE *infile,
		    char *infname, int encoding,
		    char *outfname, int filemode,
		    int partno, long linperfile, long filesize,
		    char *destination,
		    char *from, char *subject, char *replyto,
		    int isemail)
{
  static int numparts, themode;
  static char mimeid[64];
  static FILE *theifile;
  struct stat finfo;
  char *subline, *oname;
  long thesize;
  int res, len;
  static crc32_t crc;
  crc32_t *crcptr=NULL;

  if ((outfname==NULL&&infname==NULL) || (infile==NULL&&infname==NULL) ||
      (encoding!=UU_ENCODED&&encoding!=XX_ENCODED&&encoding!=B64ENCODED&&
       encoding!=PT_ENCODED&&encoding!=QP_ENCODED&&encoding!=YENC_ENCODED)) {
    UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
	       uustring (S_PARM_CHECK), "UUE_PrepPartial()");
    return UURET_ILLVAL;
  }

  oname = UUFNameFilter ((outfname)?outfname:infname);
  len   = ((subject)?strlen(subject):0) + strlen (oname) + 40;

  /*
   * if first part, get information about the file
   */

  if (partno == 1) {
    if (infile==NULL) {
      if (stat (infname, &finfo) == -1) {
	UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
		   uustring (S_NOT_STAT_FILE),
		   infname, strerror (uu_errno=errno));
	return UURET_IOERR;
      }
      if ((theifile = fopen (infname, "rb")) == NULL) {
	UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
		   uustring (S_NOT_OPEN_FILE),
		   infname, strerror (uu_errno=errno));
	return UURET_IOERR;
      }
      if (linperfile <= 0)
	numparts = 1;
      else 
	numparts = (int) (((long)finfo.st_size+(linperfile*bpl[encoding]-1))/
			  (linperfile*bpl[encoding]));

      themode  = (filemode) ? filemode : ((int) finfo.st_mode & 0777);
      thesize  = (long) finfo.st_size;
    }
    else {
      if (fstat (fileno (infile), &finfo) != 0) {
	if (filesize <= 0) {
	  UUMessage (uuencode_id, __LINE__, UUMSG_WARNING,
		     uustring (S_STAT_ONE_PART));
	  numparts = 1;
	  themode  = (filemode)?filemode:0644;
	  thesize  = -1;
	}
	else {
	  if (linperfile <= 0)
	    numparts = 1;
	  else
	    numparts = (int) ((filesize+(linperfile*bpl[encoding]-1))/
			      (linperfile*bpl[encoding]));

	  themode  = (filemode)?filemode:0644;
	  thesize  = filesize;
	}
      }
      else {
	if (linperfile <= 0)
	  numparts = 1;
	else
	  numparts = (int) (((long)finfo.st_size+(linperfile*bpl[encoding]-1))/
			    (linperfile*bpl[encoding]));

	filemode = (int) finfo.st_mode & 0777;
	thesize  = (long) finfo.st_size;
      }
      theifile = infile;
    }

    /*
     * if there's one part only, don't use Message/Partial
     */

    if (numparts == 1) {
      if (infile==NULL) fclose (theifile);
      return UUE_PrepSingleExt (outfile, infile, infname, encoding,
				outfname, filemode, destination,
				from, subject, replyto, isemail);
    }

    /*
     * we also need a unique ID
     */

    sprintf (mimeid, "UUDV-%ld.%ld.%s",
	     (long) time(NULL), thesize,
	     (strlen(oname)>16)?"oops":oname);
  }

  if ((subline = (char *) malloc (len)) == NULL) {
    UUMessage (uuencode_id, __LINE__, UUMSG_ERROR,
	       uustring (S_OUT_OF_MEMORY), len);
    if (infile==NULL) fclose (theifile);
    return UURET_NOMEM;
  }


  if (encoding == YENC_ENCODED) {
    if (partno == 1)
      crc = crc32(0L, Z_NULL, 0);
    crcptr = &crc;
    if (subject)
      sprintf (subline, "- %s - %s (%03d/%03d)", oname, subject,
	       partno, numparts);
    else
      sprintf (subline, "- %s - (%03d/%03d)", oname,
	       partno, numparts);
  }
  else {
    if (subject)
      sprintf (subline, "%s (%03d/%03d) - [ %s ]", 
	       subject, partno, numparts, oname);
    else
      sprintf (subline, "[ %s ] (%03d/%03d)",
	       oname, partno, numparts);
  }

  if (from) {
    fprintf (outfile, "From: %s%s", from, eolstring);
  }

  if (destination) {
    fprintf (outfile, "%s: %s%s",
	     (isemail)?"To":"Newsgroups",
	     destination, eolstring);
  }

  fprintf (outfile, "Subject: %s%s", subline, eolstring);

  if (replyto) {
    fprintf (outfile, "Reply-To: %s%s", replyto, eolstring);
  }

  if (encoding != YENC_ENCODED) {
    fprintf (outfile, "MIME-Version: 1.0%s", eolstring);
    fprintf (outfile, "Content-Type: Message/Partial; number=%d; total=%d;%s",
	     partno, numparts, eolstring);
    fprintf (outfile, "\tid=\"%s\"%s",
	     mimeid, eolstring);
  }
    
  fprintf (outfile, "%s", eolstring);

  res = UUEncodePartial (outfile, theifile,
			 infname, encoding,
			 (outfname)?outfname:infname, NULL,
			 themode, partno, linperfile, crcptr);

  _FP_free (subline);

  if (infile==NULL) {
    if (res != UURET_OK) {
      fclose (theifile);
      return res;
    }
    if (feof (theifile)) {
      fclose (theifile);
      return UURET_OK;
    }
    return UURET_CONT;
  }

  return res;
}
