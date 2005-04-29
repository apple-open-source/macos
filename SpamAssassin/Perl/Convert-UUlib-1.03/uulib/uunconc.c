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
 * These are the functions that are responsible for decoding. The
 * original idea is from a freeware utility called "uunconc", and
 * few lines of this code may still bear a remote resemblance to
 * its code. If you are the author or know him, contact me.
 * This program could only decode one multi-part, uuencoded file
 * where the parts were in order. Base64, XX and BinHex decoding,
 * support for multi-files and part-ordering covered by myself.
 **/

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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <crc32.h>
#include <uudeview.h>
#include <uuint.h>
#include <fptools.h>
#include <uustring.h>

char * uunconc_id = "$Id: uunconc.c,v 1.1 2004/05/14 15:23:05 dasenbro Exp $";

/* for braindead systems */
#ifndef SEEK_SET
#ifdef L_BEGIN
#define SEEK_SET L_BEGIN
#else
#define SEEK_SET 0
#endif
#endif

/*
 * decoder states
 */

#define BEGIN     (1)
#define DATA      (2)
#define END       (3)
#define DONE      (4)

/*
 * mallocable areas
 */

char *uunconc_UUxlat;
char *uunconc_UUxlen;
char *uunconc_B64xlat;
char *uunconc_XXxlat;
char *uunconc_BHxlat;
char *uunconc_save;

/*
 * decoding translation tables and line length table
 */

static int      * UUxlen;	/* initialized in UUInitConc()    */
static int      * UUxlat;	/* from the malloc'ed areas above */
static int      * B64xlat;
static int      * XXxlat;
static int     	* BHxlat;

/*
 * buffer for decoding
 */

static char *save[3];

/*
 * mallocable areas
 */

char *uuncdl_fulline;
char *uuncdp_oline;

/*
 * Return information for QuickDecode
 */

static int uulboundary;

/*
 * To prevent warnings when using a char as index into an array
 */

#define ACAST(s)	((int)(uchar)(s))

/*
 * Initialize decoding tables
 */

void
UUInitConc (void)
{
  int i, j;

  /*
   * Update pointers
   */
  UUxlen  = (int *) uunconc_UUxlen;
  UUxlat  = (int *) uunconc_UUxlat;
  B64xlat = (int *) uunconc_B64xlat;
  XXxlat  = (int *) uunconc_XXxlat;
  BHxlat  = (int *) uunconc_BHxlat;

  save[0] = uunconc_save;
  save[1] = uunconc_save + 1200;
  save[2] = uunconc_save + 2400;

  /* prepare decoding translation table */
  for(i = 0; i < 256; i++)
    UUxlat[i] = B64xlat[i] = XXxlat[i] = BHxlat[i] = -1;

  /*
   * At some time I received a file which used lowercase characters for
   * uuencoding. This shouldn't be, but let's accept it. Must take special
   * care that this doesn't break xxdecoding. This is giving me quite a
   * headache. If this one file hadn't been a Pocahontas picture, I might
   * have ignored it for good.
   */

  for (i = ' ', j = 0; i < ' ' + 64; i++, j++)
    UUxlat[i] /* = UUxlat[i+64] */ = j;
  for (i = '`', j = 0; i < '`' + 32; i++, j++)
    UUxlat[i] = j;

  /* add special cases */
  UUxlat['`'] = UUxlat[' '];
  UUxlat['~'] = UUxlat['^'];

  /* prepare line length table */
  UUxlen[0] = 1;
  for(i = 1, j = 5; i <= 61; i += 3, j += 4)
    UUxlen[i] = UUxlen[i+1] = UUxlen[i+2] = j;

  /* prepare other tables */
  for (i=0; i<64; i++) {
    B64xlat[ACAST(B64EncodeTable[i])] = i;
    XXxlat [ACAST(XXEncodeTable [i])] = i;
    BHxlat [ACAST(BHEncodeTable [i])] = i;
  }
}

/*
 * Workaround for Netscape
 */

/*
 * Determines whether Netscape may have broken up a data line (by
 * inserting a newline). This only seems to happen after <a in a
 * href statement
 */

int
UUBrokenByNetscape (char *string)
{
  char *ptr;
  int len;

  if (string==NULL || (len=strlen(string))<3)
    return 0;

  if ((ptr = _FP_stristr (string, "<a href=")) != NULL) {
    if (_FP_stristr (string, "</a>") > ptr)
      return 2;
  }

  ptr = string + len;

  while (len && (*(ptr-1)=='\015' || *(ptr-1)=='\012')) {
    ptr--; len--;
  }
  if (len<3)         return 0;
  if (*--ptr == ' ') ptr--;
  ptr--;

  if (_FP_strnicmp (ptr, "<a", 2) == 0)
    return 1;

  return 0;
}

/*
 * Try to repair a Netscape-corrupted line of data.
 * This must only be called on corrupted lines, since non-Netscape
 * data may even _get_ corrupted by this procedure.
 * 
 * Some checks are included multiply to speed up the procedure. For
 * example: (*p1!='<' || strnicmp(p1,"</a>",4)). If the first expression
 * becomes true, the costly function isn't called :-)
 *
 * Since '<', '>', '&' might even be replaced by their html equivalents
 * in href strings, I'm now using two passes, the first one for &amp; + co,
 * the second one for hrefs.
 */

int
UUNetscapeCollapse (char *string)
{
  char *p1=string, *p2=string;
  int res = 0;

  if (string==NULL)
    return 0;

  /*
   * First pass
   */
  while (*p1) {
    if (*p1 == '&') {
      if      (_FP_strnicmp (p1, "&amp;", 5) == 0) { p1+=5; *p2++='&'; res=1; }
      else if (_FP_strnicmp (p1, "&lt;",  4) == 0) { p1+=4; *p2++='<'; res=1; }
      else if (_FP_strnicmp (p1, "&gt;",  4) == 0) { p1+=4; *p2++='>'; res=1; }
      else *p2++ = *p1++;
      res = 1;
    }
    else *p2++ = *p1++;
  }
  *p2 = '\0';
  /*
   * Second pass
   */
  p1 = p2 = string;

  while (*p1) {
    if (*p1 == '<') {
      if ((_FP_strnicmp (p1, "<ahref=", 7) == 0 ||
	   _FP_strnicmp (p1, "<a href=",8) == 0) && 
	  (_FP_strstr (p1, "</a>") != 0 || _FP_strstr (p1, "</A>") != 0)) {
	while (*p1 && *p1!='>')        p1++;
	if (*p1=='\0' || *(p1+1)!='<') return 0;
	p1++;
	while (*p1 && (*p1!='<' || _FP_strnicmp(p1,"</a>",4)!=0)) {
	  *p2++ = *p1++;
	}
	if (_FP_strnicmp(p1,"</a>",4) != 0)
	  return 0;
	p1+=4;
	res=1;
      }
      else
	*p2++ = *p1++;
    }
    else
      *p2++ = *p1++;
  }
  *p2 = '\0';

  return res;
}

/*
 * The second parameter is 0 if we are still searching for encoded data,
 * otherwise it indicates the encoding we're using right now. If we're
 * still in the searching stage, we must be a little more strict in
 * deciding for or against encoding; there's too much plain text looking
 * like encoded data :-(
 */

int
UUValidData (char *ptr, int encoding, int *bhflag)
{
  int i=0, j, len=0, suspicious=0, flag=0;
  char *s = ptr;

  if ((s == NULL) || (*s == '\0')) {
    return 0;              /* bad string */
  }

  while (*s && *s!='\012' && *s!='\015') {
    s++;
    len++;
    i++;
  }

  if (i == 0)
    return 0;

  switch (encoding) {
  case UU_ENCODED:
    goto _t_UU;
  case XX_ENCODED:
    goto _t_XX;
  case B64ENCODED:
    goto _t_B64;
  case BH_ENCODED:
    goto _t_Binhex;
  case YENC_ENCODED:
    return YENC_ENCODED;
  }

 _t_Binhex:                 /* Binhex Test */
  len = i; s = ptr;

  /*
   * bhflag notes the state we're in. Within the data, it's 1. If we're
   * still looking for the initial :, it's 0
   */
  if (*bhflag == 0 && *s != ':') {
    if (encoding==BH_ENCODED) return 0;
    goto _t_B64;
  }
  else if (*bhflag == 0 /* *s == ':' */) {
    s++; len--;
  }

  while (len && BHxlat[ACAST(*s)] != -1) {
    len--; s++;
  }

  /* allow space characters at the end of the line if we are sure */
  /* that this is Binhex encoded data or the line was long enough */

  flag = (*s == ':') ? 0 : 1;

  if (*s == ':' && len>0) {
    s++; len--;
  }
  if (((i>=60 && len<=10) || encoding) && *s==' ') {
    while (len && *s==' ') {
      s++; len--;
    }
  }

  /*
   * BinHex data shall have exactly 64 characters (except the last
   * line). We ignore everything with less than 40 characters to
   * be flexible
   */

  if (len != 0 || (flag && i < 40)) {
    if (encoding==BH_ENCODED) return 0;
    goto _t_B64;
  }

  *bhflag = flag;

  return BH_ENCODED;

 _t_B64:                    /* Base64 Test */
  len = i; s = ptr;

  /*
   * Face it: there _are_ Base64 lines that are not a multiple of four
   * in length :-(
   *
   * if (len%4)
   *   goto _t_UU;
   */

  while (len--) {
    if (*s < 0 || (B64xlat[ACAST(*s)] == -1 && *s != '=')) {
      /* allow space characters at the end of the line if we are sure */
      /* that this is Base64 encoded data or the line was long enough */
      if (((i>=60 && len<=10) || encoding) && *s++==' ') {
	while (*s==' ' && len) s++;
	if (len==0) return B64ENCODED;
      }
      if (encoding==B64ENCODED) return 0;
      goto _t_UU;
    }
    else if (*s == '=') {   /* special case at end */
      /* if we know this is B64encoded, allow spaces at end of line */
      s++;
      if (*s=='=' && len>=1) {
	len--; s++;
      }
      if (encoding && len && *s==' ') {
	while (len && *s==' ') {
	  s++; len--;
	}
      }
      if (len != 0) {
	if (encoding==B64ENCODED) return 0;
	goto _t_UU;
      }
      return B64ENCODED;
    }
    s++;
  }
  return B64ENCODED;

 _t_UU:
  len = i; s = ptr;

  if (UUxlat[ACAST(*s)] == -1) {    /* uutest */
    if (encoding==UU_ENCODED) return 0;
    goto _t_XX;
  }

  j = UUxlen[UUxlat[ACAST(*s)]];

  if (len-1 == j)	    /* remove trailing character */
    len--;
  if (len != j) {
    switch (UUxlat[ACAST(*s)]%3) {
    case 1:
      if (j-2 == len) j-=2;
      break;
    case 2:
      if (j-1 == len) j-=1;
      break;
    }
  }

  /*
   * some encoders are broken with respect to encoding the last line of
   * a file and produce extraoneous characters beyond the expected EOL
   * So were not too picky here about the last line, as long as it's longer
   * than necessary and shorter than the maximum
   * this tolerance broke the xxdecoding, because xxencoded data was
   * detected as being uuencoded :( so don't accept 'h' as first character
   * also, if the first character is lowercase, don't accept the line to
   * have space characters. the only encoder I've heard of which uses
   * lowercase characters at least accepts the special case of encoding
   * 0 as `. The strchr() shouldn't be too expensive here as it's only
   * evaluated if the first character is lowercase, which really shouldn't
   * be in uuencoded text.
   */
  if (len != j &&
      ((ptr[0] == '-' && ptr[1] == '-' && strstr(ptr,"part")!=NULL) ||
       !(*ptr != 'M' && *ptr != 'h' &&
	 len > j && len <= UUxlen[UUxlat['M']]))) {
    if (encoding==UU_ENCODED) return 0;
    goto _t_XX;             /* bad length */
  }

  if (len != j || islower (*ptr)) {
    /*
     * if we are not in a 'uuencoded' state, don't allow the line to have
     * space characters at all. if we know we _are_ decoding uuencoded
     * data, the rest of the line, beyond the length of encoded data, may
     * have spaces.
     */
    if (encoding != UU_ENCODED)
      if (strchr (ptr, ' ') != NULL)
	goto _t_XX;

/*  suspicious = 1;    we're careful here REMOVED 0.4.15 __FP__ */
    len        = j;
  }

  while (len--) {
    if (*s < 0 || UUxlat[ACAST(*s++)] < 0) {
      if (encoding==UU_ENCODED) return 0;
      goto _t_XX;           /* bad code character */
    }
    if (*s == ' ' && suspicious) {
      if (encoding==UU_ENCODED) return 0;
      goto _t_XX;           /* this line looks _too_ suspicious */
    }
  }
  return UU_ENCODED;        /* data is valid */

 _t_XX:                     /* XX Test */
  len = i; s = ptr;

  if (XXxlat[ACAST(*s)] == -1)
    return 0;

  j = UUxlen[XXxlat[ACAST(*s)]];   /* Same line length table as UUencoding */

  if (len-1 == j)	    /* remove trailing character */
    len--;
  if (len != j)
    switch (UUxlat[ACAST(*s)]%3) {
    case 1:
      if (j-2 == len) j-=2;
      break;
    case 2:
      if (j-1 == len) j-=1;
      break;
    }
  /*
   * some encoders are broken with respect to encoding the last line of
   * a file and produce extraoneous characters beyond the expected EOL
   * So were not too picky here about the last line, as long as it's longer
   * than necessary and shorter than the maximum
   */
  if (len != j && !(*ptr != 'h' && len > j && len <= UUxlen[UUxlat['h']]))
    return 0;               /* bad length */

  while(len--) {
    if(*s < 0 || XXxlat[ACAST(*s++)] < 0) {
      return 0;             /* bad code character */
    }
  }
  return XX_ENCODED;        /* data is valid */
}

/*
 * This function may be called upon a line that does not look like
 * valid encoding on first sight, but might be erroneously encoded
 * data from Netscape, Lynx or MS Exchange. We might need to read
 * a new line from the stream, which is why we need the FILE.
 * Returns the type of encoded data if successful or 0 otherwise.
 */

int
UURepairData (FILE *datei, char *line, int encoding, int *bhflag)
{
  int nflag, vflag=0, safety=42;
  char *ptr;

  nflag = UUBrokenByNetscape (line);

  while (vflag == 0 && nflag && safety--) {
    if (nflag == 1) {		/* need next line to repair */
      if (strlen (line) > 250)
	break;
      ptr = line + strlen (line);
      while (ptr>line && (*(ptr-1)=='\015' || *(ptr-1)=='\012'))
	ptr--;
      if (_FP_fgets (ptr, 299-(ptr-line), datei) == NULL)
	break;
    }
    else {			/* don't need next line to repair */
    }
    if (UUNetscapeCollapse (line)) {
      if ((vflag = UUValidData (line, encoding, bhflag)) == 0)
	nflag = UUBrokenByNetscape (line);
    }
    else
      nflag = 0;
  }
  /*
   * Sometimes, a line is garbled even without it being split into
   * the next line. Then we try this in our despair
   */
  if (vflag == 0) {
    if (UUNetscapeCollapse (line))
      vflag = UUValidData (line, encoding, bhflag);
  }

  /*
   * If this line looks uuencoded, but the line is one character short
   * of a valid line, it was probably broken by MS Exchange. According
   * to my test cases, there is at most one space character missing;
   * there are never two spaces together.
   * If adding a space character helps making this line uuencoded, do
   * it!
   */

  if (vflag == 0) {
    ptr    = line + strlen(line);
    while (ptr>line && (*(ptr-1)=='\012' || *(ptr-1)=='\015')) {
      ptr--;
    }
    *ptr++ = ' ';
    *ptr-- = '\0';
    if ((vflag = UUValidData (line, encoding, bhflag)) != UU_ENCODED) {
      *ptr  = '\0';
      vflag = 0;
    }
  }
  return vflag;
}

/*
 * Decode a single encoded line using method
 */

size_t
UUDecodeLine (char *s, char *d, int method)
{
  int i, j, c, cc, count=0, z1, z2, z3, z4;
  static int leftover=0;
  int *table;

  /*
   * for re-initialization
   */

  if (s == NULL || d == NULL) {
    leftover = 0;
    return 0;
  }

  /*
   * To shut up gcc -Wall
   */
  z1 = z2 = z3 = z4 = 0;

  if (method == UU_ENCODED || method == XX_ENCODED) {
    if (method == UU_ENCODED)
      table = UUxlat;
    else
      table = XXxlat;

    i = table [ACAST(*s++)];
    j = UUxlen[i] - 1;

    while(j > 0) {
      c  = table[ACAST(*s++)] << 2;
      cc = table[ACAST(*s++)];
      c |= (cc >> 4);

      if(i-- > 0)
	d[count++] = c;
      
      cc <<= 4;
      c    = table[ACAST(*s++)];
      cc  |= (c >> 2);
      
      if(i-- > 0)
	d[count++] = cc;
      
      c <<= 6;
      c |= table[ACAST(*s++)];
      
      if(i-- > 0)
	d[count++] = c;
      
      j -= 4;
    }
  }
  else if (method == B64ENCODED) {
    if (leftover) {
      strcpy (uuncdl_fulline + leftover, s);

      leftover = 0;
      s        = uuncdl_fulline;
    }

    while ((z1 = B64xlat[ACAST(*s)]) != -1) {
      if ((z2 = B64xlat[ACAST(*(s+1))]) == -1) break;
      if ((z3 = B64xlat[ACAST(*(s+2))]) == -1) break;
      if ((z4 = B64xlat[ACAST(*(s+3))]) == -1) break;

      d[count++] = (z1 << 2) | (z2 >> 4);
      d[count++] = (z2 << 4) | (z3 >> 2);
      d[count++] = (z3 << 6) | (z4);

      s += 4;
    }
    if (z1 != -1 && z2 != -1 && *(s+2) == '=') {
      d[count++] = (z1 << 2) | (z2 >> 4);
      s+=2;
    }
    else if (z1 != -1 && z2 != -1 && z3 != -1 && *(s+3) == '=') {
      d[count++] = (z1 << 2) | (z2 >> 4);
      d[count++] = (z2 << 4) | (z3 >> 2);
      s+=3;
    }
    while (B64xlat[ACAST(*s)] != -1)
      uuncdl_fulline[leftover++] = *s++;
  }
  else if (method == BH_ENCODED) {
    if (leftover) {
      strcpy (uuncdl_fulline + leftover, s);

      leftover = 0;
      s        = uuncdl_fulline;
    }
    else if (*s == ':')
      s++;

    while ((z1 = BHxlat[ACAST(*s)]) != -1) {
      if ((z2 = BHxlat[ACAST(*(s+1))]) == -1) break;
      if ((z3 = BHxlat[ACAST(*(s+2))]) == -1) break;
      if ((z4 = BHxlat[ACAST(*(s+3))]) == -1) break;

      d[count++] = (z1 << 2) | (z2 >> 4);
      d[count++] = (z2 << 4) | (z3 >> 2);
      d[count++] = (z3 << 6) | (z4);

      s += 4;
    }
    if (z1 != -1 && z2 != -1 && *(s+2) == ':') {
      d[count++] = (z1 << 2) | (z2 >> 4);
      s+=2;
    }
    else if (z1 != -1 && z2 != -1 && z3 != -1 && *(s+3) == ':') {
      d[count++] = (z1 << 2) | (z2 >> 4);
      d[count++] = (z2 << 4) | (z3 >> 2);
      s+=3;
    }
    while (BHxlat[ACAST(*s)] != -1)
      uuncdl_fulline[leftover++] = *s++;
  }
  else if (method == YENC_ENCODED) {
    while (*s) {
      if (*s == '=') {
	if (*++s != '\0') {
	  d[count++] = (char) ((int) *s - 64 - 42);
	  s++;
	}
      }
      else if (*s == '\n' || *s == '\r') {
	s++; /* ignore */
      }
      else {
	d[count++] = (char) ((int) *s++ - 42);
      }
    }
  }

  return count;
}

/*
 * ``Decode'' Quoted-Printable text
 */

int
UUDecodeQP (FILE *datain, FILE *dataout, int *state,
	    long maxpos, int method, int flags,
	    char *boundary)
{
  char *line=uugen_inbuffer, *p1, *p2;
  int val;

  uulboundary = -1;

  while (!feof (datain) && 
	 (ftell(datain)<maxpos || flags&FL_TOEND ||
	  (!(flags&FL_PROPER) && uu_fast_scanning))) {
    if (_FP_fgets (line, 1023, datain) == NULL)
      break;
    if (ferror (datain)) {
      UUMessage (uunconc_id, __LINE__, UUMSG_ERROR,
		 uustring (S_SOURCE_READ_ERR),
		 strerror (uu_errno = errno));
      return UURET_IOERR;
    }
    line[255] = '\0';

    if (boundary && line[0]=='-' && line[1]=='-' &&
	strncmp (line+2, boundary, strlen (boundary)) == 0) {
      if (line[strlen(boundary)+2]=='-')
	uulboundary = 1;
      else
	uulboundary = 0;
      return UURET_OK;
    }

    if (UUBUSYPOLL(ftell(datain)-progress.foffset,progress.fsize)) {
      UUMessage (uunconc_id, __LINE__, UUMSG_NOTE,
		 uustring (S_DECODE_CANCEL));
      return UURET_CANCEL;
    }

    p1 = p2 = line;

    while (*p2) {
      while (*p2 && *p2 != '=')
	p2++;
      if (*p2 == '\0')
	break;
      *p2 = '\0';
      fprintf (dataout, "%s", p1);
      p1  = ++p2;

      if (isxdigit (*p2) && isxdigit (*(p2+1))) {
	val  = ((isdigit(*p2))    ?  (*p2-'0')   : (tolower(*p2)-'a'+10)) << 4;
	val |= ((isdigit(*(p2+1)))?(*(p2+1)-'0') : (tolower(*(p2+1))-'a'+10));

	fputc (val, dataout);
	p2 += 2;
	p1  = p2;
      }
      else if (*p2 == '\012' || *(p2+1) == '\015') {
	/* soft line break */
	*p2 = '\0';
	break;
      }
      else {
	/* huh? */
	fputc ('=', dataout);
      }
    }
    /*
     * p2 points to a nullbyte right after the CR/LF/CRLF
     */
    val = 0;
    while (p2>p1 && isspace (*(p2-1))) {
      if (*(p2-1) == '\012' || *(p2-1) == '\015')
	val = 1;
      p2--;
    }
    *p2 = '\0';

    /*
     * If the part ends directly after this line, the data does not end
     * with a linebreak. Or, as the docs put it, "the CRLF preceding the
     * encapsulation line is conceptually attached to the boundary.
     * So if the part ends here, don't print a line break"
     */
    if (val && (!feof (datain) && 
		(ftell(datain)<maxpos || flags&FL_TOEND ||
		 (!(flags&FL_PROPER) && uu_fast_scanning))))
      fprintf (dataout, "%s\n", p1);
    else
      fprintf (dataout, "%s", p1);
  }
  return UURET_OK;
}

/*
 * ``Decode'' plain text. Our job is to properly handle the EOL sequence
 */

int
UUDecodePT (FILE *datain, FILE *dataout, int *state,
	    long maxpos, int method, int flags,
	    char *boundary)
{
  char *line=uugen_inbuffer, *ptr;

  uulboundary = -1;

  while (!feof (datain) && 
	 (ftell(datain)<maxpos || flags&FL_TOEND ||
	  (!(flags&FL_PROPER) && uu_fast_scanning))) {
    if (_FP_fgets (line, 1023, datain) == NULL)
      break;
    if (ferror (datain)) {
      UUMessage (uunconc_id, __LINE__, UUMSG_ERROR,
		 uustring (S_SOURCE_READ_ERR),
		 strerror (uu_errno = errno));
      return UURET_IOERR;
    }
    line[255] = '\0';

    if (boundary && line[0]=='-' && line[1]=='-' &&
	strncmp (line+2, boundary, strlen (boundary)) == 0) {
      if (line[strlen(boundary)+2]=='-')
	uulboundary = 1;
      else
	uulboundary = 0;
      return UURET_OK;
    }

    if (UUBUSYPOLL(ftell(datain)-progress.foffset,progress.fsize)) {
      UUMessage (uunconc_id, __LINE__, UUMSG_NOTE,
		 uustring (S_DECODE_CANCEL));
      return UURET_CANCEL;
    }

    ptr = line + strlen (line);

    while (ptr>line && (*(ptr-1) == '\012' || *(ptr-1) == '\015'))
      ptr--;


    /*
     * If the part ends directly after this line, the data does not end
     * with a linebreak. Or, as the docs put it, "the CRLF preceding the
     * encapsulation line is conceptually attached to the boundary.
     * So if the part ends here, don't print a line break"
     */
    if ((*ptr == '\012' || *ptr == '\015') &&
	(ftell(datain)<maxpos || flags&FL_TOEND || flags&FL_PARTIAL ||
	 !boundary || (!(flags&FL_PROPER) && uu_fast_scanning))) {
      *ptr = '\0';
      fprintf (dataout, "%s\n", line);
    }
    else {
      *ptr = '\0';
      fprintf (dataout, "%s", line);
    }
  }
  return UURET_OK;
}

/*
 * Decode a single field using method. For the moment, this supports
 * Base64 and Quoted Printable only, to support RFC 1522 header decoding.
 * Quit when seeing the RFC 1522 ?= end marker.
 */

int
UUDecodeField (char *s, char *d, int method)
{
  int z1, z2, z3, z4;
  int count=0;

  if (method == B64ENCODED) {
    while ((z1 = B64xlat[ACAST(*s)]) != -1) {
      if ((z2 = B64xlat[ACAST(*(s+1))]) == -1) break;
      if ((z3 = B64xlat[ACAST(*(s+2))]) == -1) break;
      if ((z4 = B64xlat[ACAST(*(s+3))]) == -1) break;

      d[count++] = (z1 << 2) | (z2 >> 4);
      d[count++] = (z2 << 4) | (z3 >> 2);
      d[count++] = (z3 << 6) | (z4);

      s+=4;
    }
    if (z1 != -1 && z2 != -1 && *(s+2) == '=') {
      d[count++] = (z1 << 2) | (z2 >> 4);
      s+=2;
    }
    else if (z1 != -1 && z2 != -1 && z3 != -1 && *(s+3) == '=') {
      d[count++] = (z1 << 2) | (z2 >> 4);
      d[count++] = (z2 << 4) | (z3 >> 2);
      s+=3;
    }
  }
  else if (method == QP_ENCODED) {
    while (*s && (*s != '?' || *(s+1) != '=')) {
      while (*s && *s != '=' && (*s != '?' || *(s+1) != '=')) {
	d[count++] = *s++;
      }
      if (*s == '=') {
	if (isxdigit (*(s+1)) && isxdigit (*(s+2))) {
	  d[count]  = (isdigit (*(s+1)) ? (*(s+1)-'0') : (tolower (*(s+1))-'a'+10)) << 4;
	  d[count] |= (isdigit (*(s+2)) ? (*(s+2)-'0') : (tolower (*(s+2))-'a'+10));
	  count++;
	  s+=3;
	}
	else if (*(s+1) == '\012' || *(s+1) == '\015') {
	  s+=2;
	}
	else {
	  d[count++] = *s++;
	}
      }
    }
  }
  else {
    return -1;
  }

  d[count] = '\0';
  return count;
}

int
UUDecodePart (FILE *datain, FILE *dataout, int *state,
	      long maxpos, int method, int flags,
	      char *boundary)
{
  char *line, *oline=uuncdp_oline;
  int warning=0, vlc=0, lc[2], hadct=0;
  int tc=0, tf=0, vflag, haddata=0, haddh=0;
  long yefilesize=0, yepartends=0;
  crc32_t yepartcrc=crc32(0L, Z_NULL, 0);
  static crc32_t yefilecrc=0;
  static int bhflag=0;
  size_t count=0;
  size_t yepartsize=0;
  char *ptr;

  if (datain == NULL || dataout == NULL) {
    yefilecrc = crc32(0L, Z_NULL, 0);
    bhflag = 0;
    return UURET_OK;
  }

  /*
   * Use specialized functions for QP_ENCODED and PT_ENCODED plaintext
   */

  if (method == QP_ENCODED)
    return UUDecodeQP (datain, dataout, state, maxpos,
		       method, flags, boundary);
  else if (method == PT_ENCODED)
    return UUDecodePT (datain, dataout, state, maxpos,
		       method, flags, boundary);

  lc[0] = lc[1] = 0;
  vflag = 0;

  uulboundary = -1;

  if (method == YENC_ENCODED) {
    *state = BEGIN;
  }

  while (!feof (datain) && *state != DONE && 
	 (ftell(datain)<maxpos || flags&FL_TOEND || maxpos==-1 ||
	  (!(flags&FL_PROPER) && uu_fast_scanning))) {
    if (_FP_fgets ((line = uugen_fnbuffer), 1200 - 5, datain) == NULL)
      break;

    /* optionally skip .. */
    if (*line == '.' && uu_dotdot)
      line++;

    if (ferror (datain)) {
      UUMessage (uunconc_id, __LINE__, UUMSG_ERROR,
		 uustring (S_SOURCE_READ_ERR),
		 strerror (uu_errno = errno));
      return UURET_IOERR;
    }

    if (line[0]=='\015' || line[0]=='\012') { /* Empty line? */
      if (*state == DATA &&
	  (method == UU_ENCODED || method == XX_ENCODED))
	*state = END;

      /*
       * if we had a whole block of valid lines before, we reset our
       * 'valid data' flag, tf. Without this 'if', we'd break decoding
       * files with interleaved blank lines. The value of 5 is chosen
       * quite arbitrarly.
       */

      if (vlc > 5)
	tf = tc = 0;
      vlc = 0;
      continue;
    }
    
    /*
     * Busy Polls
     */

    if (UUBUSYPOLL(ftell(datain)-progress.foffset,progress.fsize)) {
      UUMessage (uunconc_id, __LINE__, UUMSG_NOTE,
		 uustring (S_DECODE_CANCEL));
      return UURET_CANCEL;
    }

    /*
     * try to make sense of data
     */

    line[1200 - 1] = '\0'; /* For Safety of string functions */
    count          =  0;

    if (boundary && line[0]=='-' && line[1]=='-' &&
	strncmp (line+2, boundary, strlen (boundary)) == 0) {
      if (line[strlen(boundary)+2]=='-')
	uulboundary = 1;
      else
	uulboundary = 0;
      return UURET_OK;
    }

    /*
     * Use this pseudo-handling only if !FL_PROPER
     */

    if ((flags&FL_PROPER) == 0) {
      if (strncmp (line, "BEGIN", 5) == 0 &&
	  _FP_strstr  (line, "CUT HERE")  && !tf) { /* I hate these lines */
	tc = tf = vlc = 0;
	continue;
      }
      /* MIME body boundary */
      if (line[0] == '-' && line[1] == '-' && method == B64ENCODED) {
	if ((haddata || tc) && (haddh || hadct)) {
	  *state = DONE;
	  vlc   = 0;
	  lc[0] = lc[1] = 0;
	  continue;
	}
	hadct = 0;
	haddh = 1;
	continue;
      }
      if (_FP_strnicmp (line, "Content-Type", 12) == 0)
	hadct = 1;
    }

    if (*state == BEGIN) {
      if ((method == UU_ENCODED || method == XX_ENCODED) &&
	  (strncmp      (line, "begin ",       6) == 0 ||
	   _FP_strnicmp (line, "<pre>begin ", 11) == 0)) { /* for LYNX */
	*state = DATA;
	continue;
      }
      else if (method == BH_ENCODED && line[0] == ':') {
	if (UUValidData (line, BH_ENCODED, &bhflag) == BH_ENCODED) {
	  bhflag = 0;
	  *state = DATA;
	}
	else
	  continue;
      }
      else if (method == YENC_ENCODED &&
	       strncmp (line, "=ybegin ", 8) == 0 &&
	       _FP_strstr (line, " name=") != NULL) {
	*state = DATA;

	if ((ptr = _FP_strstr (line, " size=")) != NULL) {
	  ptr += 6;
	  yefilesize = atoi (ptr);
	}
	else {
	  yefilesize = -1;
	}

	if (_FP_strstr (line, " part=") != NULL) {
	  if (_FP_fgets (line, 1200 - 5, datain) == NULL) {
	    break;
	  }

	  if ((ptr = _FP_strstr (line, " end=")) == NULL) {
	    break;
	  }
       
	  yepartends = atoi (ptr + 5);
	}
	tf = 1;
	continue;
      }
      else {
	continue;
      }
      
      tc = tf = vlc = 0;
      lc[0] = lc[1] = 0;
    }
    else if ((*state == END) &&
	     (method == UU_ENCODED || method == XX_ENCODED)) {
      if (strncmp (line, "end", 3) == 0) {
	*state = DONE;
	break;
      }
    }

    if (*state == DATA && method == YENC_ENCODED &&
	strncmp (line, "=yend ", 6) == 0) {
      if ((ptr = _FP_strstr (line, " pcrc32=")) != NULL) {
	crc32_t pcrc32 = strtoul (ptr + 8, NULL, 16);
	if (pcrc32 != yepartcrc) {
	  UUMessage (uunconc_id, __LINE__, UUMSG_WARNING,
		     uustring (S_PCRC_MISMATCH), progress.curfile, progress.partno);
	}
      }
      if ((ptr = _FP_strstr (line, " crc32=")) != NULL)
      {
	crc32_t fcrc32 = strtoul (ptr + 7, NULL, 16);
	if (fcrc32 != yefilecrc) {
	  UUMessage (uunconc_id, __LINE__, UUMSG_WARNING,
		     uustring (S_CRC_MISMATCH), progress.curfile);
	}
      }
      if ((ptr = _FP_strstr (line, " size=")) != NULL)
      {
	size_t size = atol(ptr + 6);
	if (size != yepartsize && yefilesize != -1) {
	  if (size != yefilesize)
	    UUMessage (uunconc_id, __LINE__, UUMSG_WARNING,
		       uustring (S_PSIZE_MISMATCH), progress.curfile,
		       progress.partno, yepartsize, size);
	  else
	    UUMessage (uunconc_id, __LINE__, UUMSG_WARNING,
		       uustring (S_SIZE_MISMATCH), progress.curfile,
		       yepartsize, size);
	}
      }
      if (yepartends == 0 || yepartends >= yefilesize) {
	*state = DONE;
      }
      break;
    }

    if (*state == DATA || *state == END) {
      if (method==B64ENCODED && line[0]=='-' && line[1]=='-' && tc) {
	break;
      }

      if ((vflag = UUValidData (line, (tf)?method:0, &bhflag)) == 0)
	vflag = UURepairData (datain, line, (tf)?method:0, &bhflag);

      /*
       * correct XX/UUencoded lines that were declared Base64
       */

      if ((method == XX_ENCODED || method == UU_ENCODED) &&
	  vflag == B64ENCODED) {
	if (UUValidData (line, method, &bhflag) == method)
	  vflag = method;
      }

      if (vflag == method) {
	if (tf) {
	  count  = UUDecodeLine (line, oline, method);
	  if (method == YENC_ENCODED) {
	    if (yepartends)
	      yepartcrc = crc32(yepartcrc, oline, count);
	    yefilecrc = crc32(yefilecrc, oline, count);
	    yepartsize += count;
	  }
	  vlc++; lc[1]++;
	}
	else if (tc == 3) {
	  count  = UUDecodeLine (save[0], oline,         method);
	  count += UUDecodeLine (save[1], oline + count, method);
	  count += UUDecodeLine (save[2], oline + count, method);
	  count += UUDecodeLine (line,    oline + count, method);
	  tf     = 1;
	  tc     = 0;

	  /*
	   * complain if we had one or two invalid lines amidst of
	   * correctly encoded data. This usually means that the
	   * file is in error
	   */

	  if (lc[1] > 10 && (lc[0] >= 1 && lc[0] <= 2) && !warning) {
	    UUMessage (uunconc_id, __LINE__, UUMSG_WARNING,
		       uustring (S_DATA_SUSPICIOUS));
	    warning=1;
	  }
	  lc[0] = 0;
	  lc[1] = 3;
	}
	else {
	  _FP_strncpy (save[tc++], line, 1200);
	}

	if (method == UU_ENCODED)
	  *state = (line[0] == 'M') ? DATA : END;
	else if (method == XX_ENCODED)
	  *state = (line[0] == 'h') ? DATA : END;
	else if (method == B64ENCODED)
	  *state = (strchr (line, '=') == NULL) ? DATA : DONE;
	else if (method == BH_ENCODED)
	  *state = (!line[0] || strchr(line+1,':')==NULL)?DATA:DONE;
      }
      else {
	vlc = tf = tc = 0;
	haddh = 0;
	lc[0]++;
      }
    }
    else if (*state != DONE) {
      return UURET_NOEND;
    }

    if (count) {
      if (method == BH_ENCODED) {
	if (UUbhwrite (oline, 1, count, dataout) != count) {
	  UUMessage (uunconc_id, __LINE__, UUMSG_ERROR,
		     uustring (S_WR_ERR_TEMP),
		     strerror (uu_errno = errno));
	  return UURET_IOERR;
	}
      }
      else if (fwrite (oline, 1, count, dataout) != count) {
	UUMessage (uunconc_id, __LINE__, UUMSG_ERROR,
		   uustring (S_WR_ERR_TEMP),
		   strerror (uu_errno = errno));
	return UURET_IOERR;
      }
      haddata++;
      count = 0;
    }
  }

  if (*state  == DONE ||
      (*state == DATA && method == B64ENCODED &&
       vflag == B64ENCODED && (flags&FL_PROPER || haddh))) {
    for (tf=0; tf<tc; tf++) 
      count += UUDecodeLine (save[tf], oline + count, method);
    if (count) {
      if (method == BH_ENCODED) {
	if (UUbhwrite (oline, 1, count, dataout) != count) {
	  UUMessage (uunconc_id, __LINE__, UUMSG_ERROR,
		     uustring (S_WR_ERR_TEMP),
		     strerror (uu_errno = errno));
	  return UURET_IOERR;
	}
      }
      else if (fwrite (oline, 1, count, dataout) != count) {
	UUMessage (uunconc_id, __LINE__, UUMSG_ERROR,
		   uustring (S_WR_ERR_TEMP),
		   strerror (uu_errno = errno));
	return UURET_IOERR;
      }
    }
  }
  return UURET_OK;
}

/*
 * this function decodes the file into a temporary file
 */

int
UUDecode (uulist *data)
{
  int state=BEGIN, part=-1, res=0, hb;
  long rsize, dsize, numbytes;
  FILE *datain, *dataout;
  unsigned char r[8];
  char *mode, *ntmp;
  uufile *iter;
  size_t bytes;
#ifdef HAVE_MKSTEMP
  int tmpfd;
  const char *tmpprefix = "uuXXXXXX";
  char *tmpdir = NULL;
#endif /* HAVE_MKSTEMP */

  if (data == NULL || data->thisfile == NULL)
    return UURET_ILLVAL;

  if (data->state & UUFILE_TMPFILE)
    return UURET_OK;

  if (data->state & UUFILE_NODATA)
    return UURET_NODATA;

  if (data->state & UUFILE_NOBEGIN && !uu_desperate)
    return UURET_NODATA;

  if (data->uudet == PT_ENCODED)
    mode = "wt";	/* open text files in text mode */
  else
    mode = "wb";	/* otherwise in binary          */

#ifdef HAVE_MKSTEMP
  if ((getuid()==geteuid()) && (getgid()==getegid())) {
	  tmpdir=getenv("TMPDIR");
  }

  if (!tmpdir) {
	  tmpdir = "/tmp";
  }
  data->binfile = malloc(strlen(tmpdir)+strlen(tmpprefix)+2);

  if (!data->binfile) {
#else
  if ((data->binfile = tempnam (NULL, "uu")) == NULL) {
#endif /* HAVE_MKSTEMP */
    UUMessage (uunconc_id, __LINE__, UUMSG_ERROR,
	       uustring (S_NO_TEMP_NAME));
    return UURET_NOMEM;
  }

#ifdef HAVE_MKSTEMP
  strcpy(data->binfile, tmpdir);
  strcat(data->binfile, "/");
  strcat(data->binfile, tmpprefix);

  if ((tmpfd = mkstemp(data->binfile)) == -1 || 
	  (dataout = fdopen(tmpfd, mode)) == NULL) {
#else
  if ((dataout = fopen (data->binfile, mode)) == NULL) {
#endif /* HAVE_MKSTEMP */
    /*
     * we couldn't create a temporary file. Usually this means that TMP
     * and TEMP aren't set
     */
    UUMessage (uunconc_id, __LINE__, UUMSG_ERROR,
	       uustring (S_WR_ERR_TARGET),
	       data->binfile, strerror (uu_errno = errno));
#ifdef HAVE_MKSTEMP
	if (tmpfd != -1) {
		unlink(data->binfile);
		close(tmpfd);
    }
#endif /* HAVE_MKSTEMP */
    _FP_free (data->binfile);
    data->binfile = NULL;
    uu_errno = errno;
    return UURET_IOERR;
  }

  /*
   * we don't have begin lines in Base64 or plain text files.
   */
  if (data->uudet == B64ENCODED || data->uudet == QP_ENCODED ||
      data->uudet == PT_ENCODED)
    state = DATA;

  /*
   * If we know that the file does not have a begin, we simulate
   * it in desperate mode
   */

  if ((data->state & UUFILE_NOBEGIN) && uu_desperate)
    state = DATA;

  (void) UUDecodeLine (NULL, NULL, 0);                   /* init */
  (void) UUbhwrite    (NULL, 0, 0, NULL);                /* dito */
  (void) UUDecodePart (NULL, NULL, NULL, 0, 0, 0, NULL); /* yep  */

  /*
   * initialize progress information
   */
  progress.action = 0;
  if (data->filename != NULL) {
    _FP_strncpy (progress.curfile,
		 (strlen(data->filename)>255)?
		 (data->filename+strlen(data->filename)-255):data->filename,
		 256);
  }
  else {
    _FP_strncpy (progress.curfile,
		 (strlen(data->binfile)>255)?
		 (data->binfile+strlen(data->binfile)-255):data->binfile,
		 256);
  }
  progress.partno   =  0;
  progress.numparts =  0;
  progress.fsize    = -1;
  progress.percent  =  0;
  progress.action   =  UUACT_DECODING;

  iter = data->thisfile;
  while (iter) {
    progress.numparts = (iter->partno)?iter->partno:1;
    iter = iter->NEXT;
  }
  
  /*
   * let's rock!
   */

  iter = data->thisfile;
  while (iter) {
    if (part != -1 && iter->partno != part+1 && !uu_desperate)
      break;
    else
      part = iter->partno;

    if (iter->data->sfname == NULL) {
      iter = iter->NEXT;
      continue;
    }

    /*
     * call our FileCallback to retrieve the file
     */

    if (uu_FileCallback) {
      if ((res = (*uu_FileCallback) (uu_FileCBArg, iter->data->sfname,
				     uugen_fnbuffer, 1)) != UURET_OK)
	break;
      if ((datain = fopen (uugen_fnbuffer, "rb")) == NULL) {
	(*uu_FileCallback) (uu_FileCBArg, iter->data->sfname,
			    uugen_fnbuffer, 0);
	UUMessage (uunconc_id, __LINE__, UUMSG_ERROR,
		   uustring (S_NOT_OPEN_FILE),
		   uugen_fnbuffer, strerror (uu_errno = errno));
	res = UURET_IOERR;
	break;
      }
    }
    else {
      if ((datain = fopen (iter->data->sfname, "rb")) == NULL) {
	UUMessage (uunconc_id, __LINE__, UUMSG_ERROR,
		   uustring (S_NOT_OPEN_FILE),
		   iter->data->sfname, strerror (uu_errno = errno));
	res = UURET_IOERR;
	break;
      }
      _FP_strncpy (uugen_fnbuffer, iter->data->sfname, 1024);
    }

    progress.partno  = part;
    progress.fsize   = (iter->data->length)?iter->data->length:-1;
    progress.percent = 0;
    progress.foffset = iter->data->startpos;

    fseek              (datain, iter->data->startpos, SEEK_SET);
    res = UUDecodePart (datain, dataout, &state,
			iter->data->startpos+iter->data->length,
			data->uudet, iter->data->flags, NULL);
    fclose             (datain);

    if (uu_FileCallback)
      (*uu_FileCallback) (uu_FileCBArg, iter->data->sfname, uugen_fnbuffer, 0);

    if (state == DONE || res != UURET_OK)
      break;

    iter = iter->NEXT;
  }

  if (state == DATA && 
      (data->uudet == B64ENCODED || data->uudet == QP_ENCODED ||
       data->uudet == PT_ENCODED))
    state = DONE; /* assume we're done */

  if (fclose (dataout)) {
    UUMessage (uunconc_id, __LINE__, UUMSG_ERROR,
	       uustring (S_WR_ERR_TEMP),
	       strerror (uu_errno = errno));
    res = UURET_IOERR;
  }

  if (res != UURET_OK || (state != DONE && !uu_desperate)) {
    unlink (data->binfile);
    _FP_free (data->binfile);
    data->binfile = NULL;
    data->state  &= ~UUFILE_TMPFILE;
    data->state  |=  UUFILE_ERROR;

    if (res == UURET_OK && state != DONE)
      res = UURET_NOEND;
  }
  else if (res != UURET_OK) {
    data->state &= ~UUFILE_DECODED;
    data->state |=  UUFILE_ERROR | UUFILE_TMPFILE;
  }
  else {
    data->state &= ~UUFILE_ERROR;
    data->state |=  UUFILE_TMPFILE;
  }

  /*
   * If this was a BinHex file, we must extract its data or resource fork
   */

  if (data->uudet == BH_ENCODED && data->binfile) {
#ifdef HAVE_MKSTEMP
	  ntmp = malloc(strlen(tmpdir)+strlen(tmpprefix)+2);
	  
	  if (ntmp == NULL) {
#else
    if ((ntmp = tempnam (NULL, "uu")) == NULL) {
#endif /* HAVE_MKSTEMP */
      UUMessage (uunconc_id, __LINE__, UUMSG_ERROR,
		 uustring (S_NO_TEMP_NAME));
      progress.action = 0;
      return UURET_NOMEM;
    }
    if ((datain = fopen (data->binfile, "rb")) == NULL) {
      UUMessage (uunconc_id, __LINE__, UUMSG_ERROR,
		 uustring (S_NOT_OPEN_FILE),
		 data->binfile, strerror (uu_errno = errno));
      progress.action = 0;
      free (ntmp);
      return UURET_IOERR;
    }
#ifdef HAVE_MKSTEMP
	strcpy(ntmp, tmpdir);
	strcat(ntmp, "/");
	strcat(ntmp, tmpprefix); 
    if ((tmpfd = mkstemp(ntmp)) == -1 ||
		(dataout = fdopen(tmpfd, "wb")) == NULL) {
#else
    if ((dataout = fopen (ntmp, "wb")) == NULL) {
#endif /* HAVE_MKSTEMP */
      UUMessage (uunconc_id, __LINE__, UUMSG_ERROR,
		 uustring (S_NOT_OPEN_TARGET),
		 ntmp, strerror (uu_errno = errno));
      progress.action = 0;
      fclose (datain);
#ifdef HAVE_MKSTEMP
	  if (tmpfd != -1) {
		  unlink(ntmp);
		  close(tmpfd);
	  }
#endif /* HAVE_MKSTEMP */
      free   (ntmp);
      return UURET_IOERR;
    }

    /*
     * read fork lengths. remember they're in Motorola format
     */
    r[0] = fgetc (datain);
    hb   = (int) r[0] + 22;
    fseek (datain, (int) r[0] + 12, SEEK_SET);
    fread (r, 1, 8, datain);

    dsize = (((long) 1 << 24) * (long) r[0]) +
            (((long) 1 << 16) * (long) r[1]) +
            (((long) 1 <<  8) * (long) r[2]) +
            (                   (long) r[3]);
    rsize = (((long) 1 << 24) * (long) r[4]) +
	    (((long) 1 << 16) * (long) r[5]) +
	    (((long) 1 <<  8) * (long) r[6]) +
	    (                   (long) r[7]);

    UUMessage (uunconc_id, __LINE__, UUMSG_MESSAGE,
	       uustring (S_BINHEX_SIZES),
	       dsize, rsize);

    if (dsize == 0) {
      fseek  (datain, dsize + hb + 2, SEEK_SET);
      numbytes = rsize;
    }
    else if (rsize == 0) {
      fseek  (datain, hb, SEEK_SET);
      numbytes = dsize;
    }
    else {
      /* we should let the user have the choice here */
      UUMessage (uunconc_id, __LINE__, UUMSG_NOTE,
		 uustring (S_BINHEX_BOTH));
      fseek  (datain, hb, SEEK_SET);
      numbytes = dsize;
    }

    progress.action   = 0;
    progress.partno   = 0;
    progress.numparts = 1;
    progress.fsize    = (numbytes)?numbytes:-1;
    progress.foffset  = hb;
    progress.percent  = 0;
    progress.action   = UUACT_COPYING;

    /*
     * copy the chosen fork
     */

    while (!feof (datain) && numbytes) {
      if (UUBUSYPOLL(ftell(datain)-progress.foffset,progress.fsize)) {
	UUMessage (uunconc_id, __LINE__, UUMSG_NOTE,
		   uustring (S_DECODE_CANCEL));
	fclose (datain);
	fclose (dataout);
	unlink (ntmp);
	free   (ntmp);
	return UURET_CANCEL;
      }

      bytes = fread (uugen_inbuffer, 1,
		     (size_t) ((numbytes>1024)?1024:numbytes), datain);

      if (ferror (datain) || (bytes == 0 && !feof (datain))) {
	progress.action = 0;
	UUMessage (uunconc_id, __LINE__, UUMSG_ERROR,
		   uustring (S_SOURCE_READ_ERR),
		   data->binfile, strerror (uu_errno = errno));
	fclose (datain);
	fclose (dataout);
	unlink (ntmp);
	free   (ntmp);
	return UURET_IOERR;
      }
      if (fwrite (uugen_inbuffer, 1, bytes, dataout) != bytes) {
	progress.action = 0;
	UUMessage (uunconc_id, __LINE__, UUMSG_ERROR,
		   uustring (S_WR_ERR_TARGET),
		   ntmp, strerror (uu_errno = errno));
	fclose (datain);
	fclose (dataout);
	unlink (ntmp);
	free   (ntmp);
	return UURET_IOERR;
      }
      numbytes -= bytes;
    }

    if (numbytes) {
      UUMessage (uunconc_id, __LINE__, UUMSG_WARNING,
		 uustring (S_SHORT_BINHEX),
		 (data->filename)?data->filename:
		 (data->subfname)?data->subfname:"???",
		 numbytes);
    }

    /*
     * replace temp file
     */

    fclose (datain);
    if (fclose (dataout)) {
      UUMessage (uunconc_id, __LINE__, UUMSG_ERROR,
		 uustring (S_WR_ERR_TARGET),
		 ntmp, strerror (uu_errno = errno));
      unlink (ntmp);
      free   (ntmp);
      return UURET_IOERR;
    }

    if (unlink (data->binfile)) {
      UUMessage (uunconc_id, __LINE__, UUMSG_WARNING,
		 uustring (S_TMP_NOT_REMOVED),
		 data->binfile, strerror (uu_errno = errno));
    }

    free (data->binfile);
    data->binfile = ntmp;
  }

  progress.action = 0;
  return res;
}

/*
 * QuickDecode for proper MIME attachments. We expect the pointer to
 * be on the first header line.
 */

int
UUQuickDecode (FILE *datain, FILE *dataout, char *boundary, long maxpos)
{
  int state=BEGIN, encoding=-1;
  headers myenv;

  /*
   * Read header and find out about encoding.
   */

  memset (&myenv, 0, sizeof (headers));
  UUScanHeader (datain, &myenv);

  if (_FP_stristr (myenv.ctenc, "uu") != NULL)
    encoding = UU_ENCODED;
  else if (_FP_stristr (myenv.ctenc, "xx") != NULL)
    encoding = XX_ENCODED;
  else if (_FP_stricmp (myenv.ctenc, "base64") == 0)
    encoding = B64ENCODED;
  else if (_FP_stricmp (myenv.ctenc, "quoted-printable") == 0)
    encoding = QP_ENCODED;
  else
    encoding = PT_ENCODED;

  UUkillheaders (&myenv);

  /*
   * okay, so decode this one
   */

  (void) UUDecodePart (NULL, NULL, NULL, 0, 0, 0, NULL); /* init  */
  return UUDecodePart (datain, dataout, &state, maxpos,
		       encoding, FL_PROPER|FL_TOEND,
		       boundary);
}
