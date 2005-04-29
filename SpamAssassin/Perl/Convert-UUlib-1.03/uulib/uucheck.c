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

/*
 * uucheck.c
 *
 * Various checking and processing of one input part
 **/

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

#include <uudeview.h>
#include <uuint.h>
#include <fptools.h>
#include <uustring.h>

char * uucheck_id = "$Id: uucheck.c,v 1.1 2004/05/14 15:23:05 dasenbro Exp $";

/*
 * Arbitrary number. This is the maximum number of part numbers we
 * store for our have-parts and missing-parts lists
 */

#define MAXPLIST	256


/*
 * forward declarations of local functions
 */

static char *	UUGetFileName	(char *, char *, char *);
static int	UUGetPartNo	(char *, char **, char **);

/*
 * State of Scanner function and PreProcessPart
 */

int lastvalid, lastenc, nofnum;
char *uucheck_lastname;
char *uucheck_tempname;
static int  lastpart = 0;
static char *nofname = "UNKNOWN";

/*
 * special characters we allow an unquoted filename to have
 */

static char *fnchars = "._-~!";

/*
 * Policy for extracting a part number from the subject line.
 * usually, look for part numbers in () brackets first, then in []
 */

static char *brackchr[] = {
  "()[]", "[]()"
};

/*
 * Extract a filename from the subject line. We need anything to identify
 * the name of the program for sorting. If a nice filename cannot be found, 
 * the subject line itself is used
 * ptonum is, if not NULL, a pointer to the part number in the subject line,
 * so that it won't be used as filename.
 **/

static char *
UUGetFileName (char *subject, char *ptonum, char *ptonend)
{
  char *ptr = subject, *iter, *result, *part;
  int count, length=0, alflag=0;

/*
 * If this file has no subject line, assume it is the next part of the
 * previous file (this is done in UUPreProcessPart)
 **/

  if (subject == NULL)
    return NULL;

/*
 * If the subject starts with 'Re', it is ignored
 * REPosts or RETries are not ignored!
 **/

  if (uu_ignreply &&
      (subject[0] == 'R' || subject[0] == 'r') &&
      (subject[1] == 'E' || subject[1] == 'e') &&
      (subject[2] == ':' || subject[2] == ' ')) {
    return uu_FileNameCallback ? uu_FileNameCallback(uu_FNCBArg, subject, NULL) : NULL;
  }

/*
 * Ignore a "Repost" prefix of the subject line. We don't want to get
 * a file named "Repost" :-)
 **/

  if (_FP_strnicmp (subject, "repost", 6) == 0)
    subject += 6;
  if (_FP_strnicmp (subject, "re:", 3) == 0)
    subject += 3;

  while (*subject == ' ' || *subject == ':') subject++;

  part = _FP_stristr (subject, "part");
  if (part == subject) {
    subject += 4;
    while (*subject == ' ') subject++;
  }

  /*
   * If the file was encoded by uuenview, then the filename is enclosed
   * in [brackets]. But check what's inside these bracket's, try not to
   * fall for something other than a filename
   */

  ptr = subject;
  while ((iter = strchr (ptr, '[')) != NULL) {
    if (strchr (iter, ']') == NULL) {
      ptr = iter + 1;
      continue;
    }
    iter++;
    while (isspace (*iter))
      iter++;
    count = length = alflag = 0;
    while (iter[count] && 
	   (isalnum (iter[count]) || strchr (fnchars, iter[count])!=NULL)) {
      if (isalpha (iter[count]))
	alflag++;
      count++;
    }
    if (count<4 || alflag==0) {
      ptr = iter + 1;
      continue;
    }
    length = count;
    while (isspace (iter[count]))
      count++;
    if (iter[count] == ']') {
      ptr = iter;
      break;
    }
    length = 0;
    ptr = iter + 1;
  }

  /*
   * new filename detection routine, fists mostly for files by ftp-by-email
   * servers that create subject lines with ftp.host.address:/full/path/file
   * on them. We look for slashes and take the filename from after the last
   * one ... or at least we try to.
   */

  if (length == 0) {
    ptr = subject;
    while ((iter = strchr (ptr, '/')) != NULL) {
      if (iter >= ptonum && iter <= ptonend) {
	ptr = iter + 1;
	continue;
      }
      count = length = 0;
      iter++;
      while (iter[count] &&
	     (isalnum(iter[count])||strchr(fnchars, iter[count])!=NULL))
	count++;
      if (iter[count] == ' ' && length > 4) {
	length = count;
	break;
      }
      ptr = iter + ((count)?count:1);
    }
  }

  /*
   * Look for two alphanumeric strings separated by a '.'
   * (That's most likely a filename)
   **/

  if (length == 0) {
    ptr = subject;
    /* #warning another experimental change */
    /*while (*ptr && *ptr != 0x0a && *ptr != 0x0d && ptr != part) {*/
    while (*ptr && *ptr != 0x0a && *ptr != 0x0d) {
      iter  = ptr;
      count = length = alflag = 0;
      
      if (_FP_strnicmp (ptr, "ftp", 3) == 0) {
	/* hey, that's an ftp address */
	while (isalpha (*ptr) || isdigit (*ptr) || *ptr == '.')
	  ptr++;
	continue;
      }
      
      while ((isalnum(*iter)||strchr(fnchars, *iter)!=NULL||
	      *iter=='/') && *iter && iter != ptonum && *iter != '.') {
	if (isalpha (*iter))
	  alflag = 1;
	
	count++; iter++;
      }
      if (*iter == '\0' || iter == ptonum) {
	if (iter == ptonum)
	  ptr  = ptonend;
	else
	  ptr  = iter;

	length = 0;
	continue;
      }

      /* #warning multi-part change experimental, make pluggable */
      /* if (*iter++ != '.' || count > 32 || alflag == 0) { */
      if (*iter++ != '.' || count > 32) {
	ptr    = iter;
	length = 0;
	continue;
      }

      /* two consecutive dots don't look correct */
      if (*iter == '.') {
	ptr    = iter + 1;
	length = 0;
	continue;
      }

      if (_FP_strnicmp (iter, "edu", 3) == 0 || 
	  _FP_strnicmp (iter, "gov", 3) == 0) {
	/* hey, that's an ftp address */
	while (isalpha (*iter) || isdigit (*iter) || *iter == '.')
	  iter++;
	ptr    = iter;
	length = 0;
	continue;
      }
      
      length += count + 1;
      count   = 0;
      
      while ((isalnum(iter[count])||strchr(fnchars, iter[count])!=NULL||
	      iter[count]=='/') && iter[count] && iter[count] != '.')
	count++;
      
      if (iter[count]==':' && iter[count+1]=='/') {
	/* looks like stuff from a mail server */
	ptr = iter + 1;
	length = 0;
	continue;
      }
      
      if (count > 8 || iter == ptonum) {
	ptr    = iter;
	length = 0;
	continue;
      }

      if (iter[count] != '.') {
	length += count;
	break;
      }
      
      while (iter[count] &&
	     (isalnum(iter[count])||strchr(fnchars, iter[count])!=NULL||
	      iter[count]=='/'))
	count++;
      
      if (iter[count]==':' && iter[count+1]=='/') {
	/* looks like stuff from a mail server */
	ptr = iter + 1;
	length = 0;
	continue;
      }
      
      if (count < 12 && iter != ptonum) {
	length += count;
	break;
      }

      ptr    = iter;
      length = 0;
    }
  }

  if (length == 0) { /* No filename found, use subject line for ident */
    ptr = subject;

    while (*ptr && !isalpha (*ptr))
      ptr++;

    while ((isalnum(ptr[length])||strchr(fnchars,ptr[length])!=NULL||
	    ptr[length] == '/') && 
	   ptr[length] && ptr+length!=part && ptr+length!=ptonum)
      length++;

    if (length) {
      if (ptr[length] == '\0' || ptr[length] == 0x0a || ptr[length] == 0x0d) {
        length--;

	/*
	 * I used to cut off digits from the end of the string, but
	 * let's try to live without. We want to distinguish
	 * DUTCH951 from DUTCH952
	 *
         * while ((ptr[length] == ' ' || isdigit (ptr[length])) && length > 0)
         *   length--;
	 */
      }
      else {
        length--;

        while (ptr[length] == ' ' && length > 0)
          length--;
      }
      length++;
    }
  }

  if (length == 0) { /* Still found nothing? We need *something*! */
    ptr    = nofname;
    length = strlen (nofname);
  }

  if ((result = (char *) malloc (length + 1)) == NULL) {
    UUMessage (uucheck_id, __LINE__, UUMSG_ERROR,
	       uustring (S_OUT_OF_MEMORY), length+1);
    return NULL;
  }
    
  memcpy (result, ptr, length);
  result[length] = '\0';
    
  return uu_FileNameCallback ? uu_FileNameCallback(uu_FNCBArg, subject, result) : result;
}

/*
 * Extract the Part Number from the subject line.
 * We look first for numbers in (#/#)'s, then for numbers in [#/#]'s
 * and then for digits that are not part of a string.
 * If we cannot find anything, assume it is the next part of the
 * previous file.
 * If we find a part number, we put a pointer to it in *where. This is
 * done so that the UUGetFileName function doesn't accidentally use the
 * part number as the file name. *whend points to the end of this part
 * number.
 **/

static int
UUGetPartNo (char *subject, char **where, char **whend)
{
  char *ptr = subject, *iter, *delim, bdel[2]=" ";
  int count, length=0, bpc;

  *where = NULL; bdel[0] = ' ';
  *whend = NULL; bdel[1] = '\0';

  iter  = NULL;
  delim = "";

  if (subject == NULL)
    return -1;

  if (uu_ignreply &&
      (subject[0] == 'R' || subject[0] == 'r') && /* Ignore replies, but not */
      (subject[1] == 'E' || subject[1] == 'e') && /* reposts                 */
      (subject[2] == ':' || subject[2] == ' '))
    return -2;

  /*
   * First try numbers in () or [] (or vice versa, according to bracket
   * policy)
   * For multiple occurences, give priority to the bracket with a slash
   * or the last one, whichever is "found" first.
   */

  for (bpc=0, length=0; brackchr[uu_bracket_policy][bpc]; bpc+=2) {
    iter = subject;
    length = 0;
    while ((iter = strchr (iter, brackchr[uu_bracket_policy][bpc])) != NULL) {
      int plength;

      count = 0; iter++;

      while (*iter == ' ' || *iter == '#')
	iter++;

      if (!isdigit (*iter)) {
	continue;
      }
      while (isdigit (iter[count]))
	count++;
      
      if (iter[count] == '\0') {
	iter += count;
	break;
      }

      plength = count;

      if (iter[count] == brackchr[uu_bracket_policy][bpc+1]) {
	*where  = iter;
	bdel[0] = brackchr[uu_bracket_policy][bpc+1];
	delim   = bdel;
        length  = plength;
	continue;
      }

      while (iter[count] == ' ' || iter[count] == '#' ||
	     iter[count] == '/' || iter[count] == '\\')  count++;
      
      if (_FP_strnicmp (iter + count, "of", 2) == 0)
	count += 2;
      
      while (iter[count] == ' ')    count++;
      while (isdigit (iter[count])) count++;
      while (iter[count] == ' ')    count++;
      
      if (iter[count] == brackchr[uu_bracket_policy][bpc+1]) {
	*where  = iter;
	bdel[0] = brackchr[uu_bracket_policy][bpc+1];
	delim   = bdel;
        length  = plength;
	break;
      }
    }
    if (length)
      {
        iter = *where; /* strange control flow, but less changes == less hassle */
        break;
      }
  }

  /*
   * look for the string "part " followed by a number
   */

  if (length == 0) {
    if ((iter = _FP_stristr (subject, "part ")) != NULL) {
      iter += 5;

      while (isspace (*iter) || *iter == '.' || *iter == '-')
	iter++;

      while (isdigit (iter[length]))
        length++;

      if (length == 0) {
	if (_FP_strnicmp (iter, "one", 3) == 0)        length = 1;
	else if (_FP_strnicmp (iter, "two", 3) == 0)   length = 2;
	else if (_FP_strnicmp (iter, "three", 5) == 0) length = 3;
	else if (_FP_strnicmp (iter, "four",  4) == 0) length = 4;
	else if (_FP_strnicmp (iter, "five",  4) == 0) length = 5;
	else if (_FP_strnicmp (iter, "six",   3) == 0) length = 6;
	else if (_FP_strnicmp (iter, "seven", 5) == 0) length = 7;
	else if (_FP_strnicmp (iter, "eight", 5) == 0) length = 8;
	else if (_FP_strnicmp (iter, "nine",  4) == 0) length = 9;
	else if (_FP_strnicmp (iter, "ten",   3) == 0) length = 10;

	if (length && (*whend = strchr (iter, ' '))) {
	  *where = iter;
	  return length;
	}
	else
	  length = 0;
      }
      else {
	*where = iter;
	delim  = "of";
      }
    }
  }

  /*
   * look for the string "part" followed by a number
   */

  if (length == 0) {
    if ((iter = _FP_stristr (subject, "part")) != NULL) {
      iter += 4;

      while (isspace (*iter) || *iter == '.' || *iter == '-')
	iter++;

      while (isdigit (iter[length]))
        length++;

      if (length == 0) {
	if (_FP_strnicmp (iter, "one", 3) == 0)        length = 1;
	else if (_FP_strnicmp (iter, "two", 3) == 0)   length = 2;
	else if (_FP_strnicmp (iter, "three", 5) == 0) length = 3;
	else if (_FP_strnicmp (iter, "four",  4) == 0) length = 4;
	else if (_FP_strnicmp (iter, "five",  4) == 0) length = 5;
	else if (_FP_strnicmp (iter, "six",   3) == 0) length = 6;
	else if (_FP_strnicmp (iter, "seven", 5) == 0) length = 7;
	else if (_FP_strnicmp (iter, "eight", 5) == 0) length = 8;
	else if (_FP_strnicmp (iter, "nine",  4) == 0) length = 9;
	else if (_FP_strnicmp (iter, "ten",   3) == 0) length = 10;

	if (length && (*whend = strchr (iter, ' '))) {
	  *where = iter;
	  return length;
	}
	else
	  length = 0;
      }
      else {
	*where = iter;
	delim  = "of";
      }
    }
  }

  /*
   * look for [0-9]* "of" [0-9]*
   */

  if (length == 0) {
    if ((iter = _FP_strirstr (subject, "of")) != NULL) {
      while (iter>subject && isspace (*(iter-1)))
	iter--;
      if (isdigit(*(iter-1))) {
	while (iter>subject && isdigit (*(iter-1)))
	  iter--;
	if (!isdigit (*iter) && !isalpha (*iter) && *iter != '.')
	  iter++;
	ptr = iter;

	while (isdigit (*ptr)) {
	  ptr++; length++;
	}
	*where = iter;
	delim  = "of";
      }
    }
  }

  /*
   * look for whitespace-separated (or '/'-separated) digits
   */

  if (length == 0) {
    ptr = subject;

    while (*ptr && length==0) {
      while (*ptr && !isdigit (*ptr))
	ptr++;
      if (isdigit (*ptr) && (ptr==subject || *ptr==' ' || *ptr=='/')) {
	while (isdigit (ptr[length]))
	  length++;
	if (ptr[length]!='\0' && ptr[length]!=' ' && ptr[length]!='/') {
	  ptr   += length;
	  length = 0;
	}
	else {
	  iter    = ptr;
	  bdel[0] = ptr[length];
	  delim   = bdel;
	}
      }
      else {
	while (isdigit (*ptr))
	  ptr++;
      }
    }
  }

  /*
   * look for _any_ digits -- currently disabled, because it also fell
   * for "part numbers" in file names
   */

#if 0
  if (length == 0) {
    count = strlen(subject) - 1;
    ptr   = subject;
 
    while (count > 0) {
      if (!isdigit(ptr[count])||isalpha(ptr[count+1])||ptr[count+1] == '.') {
	count--;
	continue;
      }
      length = 0;

      while (count >= 0 && isdigit (ptr[count])) {
	count--; length++;
      }
      if (count>=0 && ((isalpha (ptr[count]) && 
			(ptr[count] != 's' || ptr[count+1] != 't') &&
			(ptr[count] != 'n' || ptr[count+1] != 'd')) || 
		       ptr[count] == '/' || ptr[count] == '.' || 
		       ptr[count] == '-' || ptr[count] == '_')) {
        length = 0;
        continue;
      }
      count++;
      iter = ptr + count;

      if (length > 4) {
	length = 0;
	continue;
      }
      *where = iter;
      delim  = "of";
      break;
    }
  }
#endif

  /*
   * look for part numbering as string
   */

  if (length == 0) {
    /*
     * some people use the strangest things, including spelling mistakes :-)
     */
    if ((iter = _FP_stristr (subject, "first")) != NULL)        length = 1;
    else if ((iter = _FP_stristr (subject, "second")) != NULL)  length = 2;
    else if ((iter = _FP_stristr (subject, "third")) != NULL)   length = 3;
    else if ((iter = _FP_stristr (subject, "forth")) != NULL)   length = 4;
    else if ((iter = _FP_stristr (subject, "fourth")) != NULL)  length = 4;
    else if ((iter = _FP_stristr (subject, "fifth")) != NULL)   length = 5;
    else if ((iter = _FP_stristr (subject, "sixth")) != NULL)   length = 6;
    else if ((iter = _FP_stristr (subject, "seventh")) != NULL) length = 7;
    else if ((iter = _FP_stristr (subject, "eigth")) != NULL)   length = 8;
    else if ((iter = _FP_stristr (subject, "eighth")) != NULL)  length = 8;
    else if ((iter = _FP_stristr (subject, "nineth")) != NULL)  length = 9;
    else if ((iter = _FP_stristr (subject, "ninth")) != NULL)   length = 9;
    else if ((iter = _FP_stristr (subject, "tenth")) != NULL)   length = 10;
    else iter = NULL;

    if (length && iter && (*whend = strchr (iter, ' '))) {
      *where = iter;
      return length;
    }
    else
      length = 0;
  }

  if (iter == NULL || length == 0)	/* should be equivalent */
    return -1;

  *where = iter;

  if (delim && delim[0]) {
    if ((*whend=_FP_stristr (iter, delim)) != NULL && (*whend - *where) < 12) {
      ptr = (*whend += strlen (delim));

      while (*ptr == ' ')
	ptr++;

      if (isdigit (*ptr)) {
	*whend = ptr;
	while (isdigit (**whend))
	  *whend += 1;
      }
    }
    else {
      *whend = iter + length;
    }
  }
  else {
    *whend = iter + length;
  }

  return atoi (iter);
}

/*
 * Obtain and process some information about the data.
 **/

uufile *
UUPreProcessPart (fileread *data, int *ret)
{
  char *where, *whend, temp[80], *ptr, *p2;
  uufile *result;

  if ((result = (uufile *) malloc (sizeof (uufile))) == NULL) {
    UUMessage (uucheck_id, __LINE__, UUMSG_ERROR,
	       uustring (S_OUT_OF_MEMORY), sizeof (uufile));
    *ret = UURET_NOMEM;
    return NULL;
  }
  memset (result, 0, sizeof (uufile));

  if (data->partno) {
    where = whend  = NULL;
    result->partno = data->partno;
  }
  else if (uu_dumbness) {
    result->partno = -1;
    where = whend  = NULL;
  }
  else if ((result->partno=UUGetPartNo(data->subject,&where,&whend)) == -2) {
    *ret = UURET_NODATA;
    UUkillfile (result);
    return NULL;
  }

  if (data->filename != NULL) {
    if ((result->filename = _FP_strdup (data->filename)) == NULL) {
      UUMessage (uucheck_id, __LINE__, UUMSG_ERROR,
		 uustring (S_OUT_OF_MEMORY),
		 strlen (data->filename)+1);
      *ret = UURET_NOMEM;
      UUkillfile (result);
      return NULL;
    }
  }
  else
    result->filename = NULL;

  if (uu_dumbness <= 1)
    result->subfname = UUGetFileName (data->subject, where, whend);
  else
    result->subfname = NULL;

  result->mimeid   = _FP_strdup (data->mimeid);
  result->mimetype = _FP_strdup (data->mimetype);

  if (result->partno == -1 && 
      (data->uudet == PT_ENCODED || data->uudet == QP_ENCODED))
    result->partno = 1;

  if (data->flags & FL_SINGLE) {
    /*
     * Don't touch this part. But it should really have a filename
     */
    if (result->filename == NULL) {
      sprintf (temp, "%s.%03d", nofname, ++nofnum);
      result->filename = _FP_strdup (temp);
    }
    if (result->subfname == NULL)
      result->subfname = _FP_strdup (result->filename);

    if (result->filename == NULL || 
	result->subfname == NULL) {
      UUMessage (uucheck_id, __LINE__, UUMSG_ERROR,
		 uustring (S_OUT_OF_MEMORY),
		 (result->filename==NULL)?
		 (strlen(temp)+1):(strlen(result->filename)+1));
      *ret = UURET_NOMEM;
      UUkillfile(result);
      return NULL;
    }
    if (result->partno == -1)
      result->partno = 1;
  }
  else if (result->subfname == NULL && data->uudet &&
      (data->begin || result->partno == 1 || 
       (!uu_dumbness && result->partno == -1 && 
	(data->subject != NULL || result->filename != NULL)))) {
    /*
     * If it's the first part of something and has some valid data, but
     * no subject or anything, initialize lastvalid
     */
    /*
     * in this case, it really _should_ have a filename somewhere
     */
    if (result->filename != NULL && *result->filename)
      result->subfname = _FP_strdup (result->filename);
    else { /* if not, escape to UNKNOWN. We need to fill subfname */
      sprintf (temp, "%s.%03d", nofname, ++nofnum);
      result->subfname = _FP_strdup (temp);
    }
    /*
     * in case the strdup failed
     */
    if (result->subfname == NULL) {
      UUMessage (uucheck_id, __LINE__, UUMSG_ERROR,
		 uustring (S_OUT_OF_MEMORY),
		 (result->filename)?
		 (strlen(result->filename)+1):(strlen(temp)+1));
      *ret = UURET_NOMEM;
      UUkillfile (result);
      return NULL;
    }
    /*
     * if it's also got an 'end', or is the last part in a MIME-Mail,
     * then don't set lastvalid
     */
    if (!data->end && (!data->partno || data->partno != data->maxpno)) {
      /*
       * initialize lastvalid
       */
      lastvalid = 1;
      lastenc   = data->uudet;
      lastpart  = result->partno = 1;
      _FP_strncpy (uucheck_lastname, result->subfname, 256);
    }
    else
      result->partno = 1;
  }
  else if (result->subfname == NULL && data->uudet && data->mimeid) {
    /*
     * if it's got a file name, use it. Else use the mime-id for identifying
     * this part, and hope there's no other files encoded in the same message
     * under the same id.
     */
    if (result->filename)
      result->subfname = _FP_strdup (result->filename);
    else
      result->subfname = _FP_strdup (result->mimeid);
  }
  else if (result->subfname == NULL && data->uudet) {
    /*
     * ff we have lastvalid, use it. Make an exception for
     * Base64-encoded files.
     */
    if (data->uudet == B64ENCODED) {
      /*
       * Assume it's the first part. I wonder why it's got no part number?
       */
      if (result->filename != NULL && *result->filename)
        result->subfname = _FP_strdup (result->filename);
      else { /* if not, escape to UNKNOWN. We need to fill subfname */
        sprintf (temp, "%s.%03d", nofname, ++nofnum);
        result->subfname = _FP_strdup (temp);
      }
      if (result->subfname == NULL) {
	UUMessage (uucheck_id, __LINE__, UUMSG_ERROR,
		   uustring (S_OUT_OF_MEMORY),
		   (result->filename)?
		   (strlen(result->filename)+1):(strlen(temp)+1));
	*ret = UURET_NOMEM;
	UUkillfile (result);
        return NULL;
      }
      lastvalid = 0;
    }
    else if (lastvalid && data->uudet == lastenc && result->partno == -1) {
      result->subfname = _FP_strdup (uucheck_lastname);
      result->partno   = ++lastpart;

      /*
       * if it's the last part, invalidate lastvalid
       */
      if (data->end || (data->partno && data->partno == data->maxpno))
	lastvalid = 0;
    }
    else if (data->partno != -1 && result->filename) {
      result->subfname = _FP_strdup (result->filename);
    }
    else { 
      /* 
       * it's got no info, it's got no begin, and we don't know anything
       * about this part. Let's forget all about it.
       */
      *ret = UURET_NODATA;
      UUkillfile (result);
      return NULL;
    }
  }
  else if (result->subfname == NULL && result->partno == -1) {
    /*
     * This, too, is a part without any useful information that we
     * should forget about.
     */
    *ret = UURET_NODATA;
    UUkillfile (result);
    return NULL;
  }
  else if (result->subfname == NULL) {
    /*
     * This is a part without useful subject name, a valid part number
     * but no encoded data. It *could* be the zeroeth part of something,
     * but we don't care here. Just forget it.
     */
    *ret = UURET_NODATA;
    UUkillfile (result);
    return NULL;
  }

  /*
   * now, handle some cases where we have a useful subject but no
   * useful part number
   */

  if (result->partno == -1 && data->begin) {
    /*
     * hmm, this is reason enough to initialize lastvalid, at least 
     * if we have no end
     */
    if (!data->end) {
      _FP_strncpy (uucheck_lastname, result->subfname, 256);
      result->partno = lastpart = 1;
      lastenc = data->uudet;
      lastvalid = 1;
    }
    else
      result->partno = 1;
  }
  else if (result->partno == -1 && data->uudet) {
    if (lastvalid && _FP_stricmp (uucheck_lastname, result->subfname) == 0) {
      /*
       * if the subject filename is the same as last time, use part no
       * of lastvalid. If at end, invalidate lastvalid
       */
      result->partno = ++lastpart;

      if (data->end)
	lastvalid = 0;
    }
    else {
      /*
       * data but no part no. It's something UUInsertPartToList() should
       * handle
       */
      goto skipcheck;
    }
  }
  else if (result->partno == -1) {
    /*
     * it's got no data, so why should we need this one anyway?
     */
    *ret = UURET_NODATA;
    UUkillfile (result);
    return NULL;
  }

  /*
   * at this point, the part should have a valid subfname and a valid
   * part number. If it doesn't, then fail.
   */
  if (result->subfname == NULL || result->partno == -1) {
    *ret = UURET_NODATA;
    UUkillfile (result);
    return NULL;
  }

 skipcheck:

  if (result->filename) {
    if (*(ptr = _FP_cutdir (result->filename))) {
      p2 = _FP_strdup (ptr);
      _FP_free (result->filename);
      result->filename = p2;
    }
  }

  result->data = data;
  result->NEXT = NULL;

  *ret = UURET_OK;

  return result;
}

/*
 * Insert one part of a file into the global list
 **/

int
UUInsertPartToList (uufile *data)
{
  uulist *iter = UUGlobalFileList, *unew;
  uufile *fiter, *last;

  /*
   * Part belongs together, if
   * (1) The MIME-IDs match, or
   * (2) The file name received from the subject lines match, and
   *     (a) Not both parts have a begin line
   *     (b) Not both parts have an end line
   *     (c) Both parts don't have different MIME-IDs
   *     (d) Both parts don't encode different files
   *     (e) The other part wants to stay alone (FL_SINGLE)
   */

  /*
   * check if this part wants to be left alone. If so, don't bother
   * to do all the checks
   */

  while (iter) {
    if (data->data->flags & FL_SINGLE) {
      /* this space intentionally left blank */
    }
    else if ((data->mimeid && iter->mimeid &&
	      strcmp (data->mimeid, iter->mimeid) == 0) ||
	     (_FP_stricmp (data->subfname, iter->subfname) == 0 &&
	      !(iter->begin && data->data->begin) &&
	      !(iter->end   && data->data->end) &&
	      !(data->mimeid && iter->mimeid &&
		strcmp (data->mimeid, iter->mimeid) != 0) &&
	      !(data->filename && iter->filename &&
		strcmp (data->filename, iter->filename) != 0) &&
	      !(iter->flags & FL_SINGLE))) {

      /*
       * Don't insert a part that is already there.
       *
       * Also don't add a part beyond the "end" marker (unless we
       * have a mimeid, which screws up the marker).
       */

      for (fiter=iter->thisfile; fiter; fiter=fiter->NEXT) {
	if (data->partno == fiter->partno)
	  goto goahead;
	if (!iter->mimeid) {
	  if (data->partno > fiter->partno && fiter->data->end) {
	    goto goahead;
	  }
	}
      }

      if (iter->filename == NULL && data->filename != NULL) {
        if ((iter->filename = _FP_strdup (data->filename)) == NULL)
	  return UURET_NOMEM;
      }

      /*
       * special case when we might have tagged a part as Base64 when the
       * file was really XX
       */

      if (data->data->uudet == B64ENCODED && 
	  iter->uudet == XX_ENCODED && iter->begin) {
	data->data->uudet = XX_ENCODED;
      }
      else if (data->data->uudet == XX_ENCODED && data->data->begin &&
	       iter->uudet == B64ENCODED) {
	iter->uudet = XX_ENCODED;

	fiter = iter->thisfile;
	while (fiter) {
	  fiter->data->uudet = XX_ENCODED;
	  fiter = fiter->NEXT;
	}
      }

      /*
       * If this is from a Message/Partial, we believe only the
       * iter->uudet from the first part
       */
      if (data->data->flags & FL_PARTIAL) {
	if (data->partno == 1) {
	  iter->uudet = data->data->uudet;
	  iter->flags = data->data->flags;
	}
      }
      else {
	if (data->data->uudet) iter->uudet = data->data->uudet;
	if (data->data->flags) iter->flags = data->data->flags;
      }

      if (iter->mode == 0 && data->data->mode != 0)
        iter->mode = data->data->mode;
      if (data->data->begin) iter->begin = (data->partno)?data->partno:1;
      if (data->data->end)   iter->end   = (data->partno)?data->partno:1;

      if (data->mimetype) {
	_FP_free (iter->mimetype);
	iter->mimetype = _FP_strdup (data->mimetype);
      }

      /*
       * insert part at the beginning
       */

      if (data->partno != -1 && data->partno < iter->thisfile->partno) {
	iter->state    = UUFILE_READ;
	data->NEXT     = iter->thisfile;
	iter->thisfile = data;
	return UURET_OK;
      }

      /*
       * insert part somewhere else
       */

      iter->state = UUFILE_READ;	/* prepare for re-checking */
      fiter       = iter->thisfile;
      last        = NULL;

      while (fiter) {
	/*
	 * if we find the same part no again, check which one looks better
	 */
	if (data->partno == fiter->partno) {
          if (fiter->data->subject == NULL)
            return UURET_NODATA;
	  else if (_FP_stristr (fiter->data->subject, "repost") != NULL &&
		   _FP_stristr (data->data->subject,  "repost") == NULL)
	    return UURET_NODATA;
          else if (fiter->data->uudet && !data->data->uudet)
            return UURET_NODATA;
          else {
	    /*
	     * replace
	     */
            data->NEXT  = fiter->NEXT;
            fiter->NEXT = NULL;
            UUkillfile (fiter);

            if (last == NULL)
              iter->thisfile = data;
            else
              last->NEXT     = data;

            return UURET_OK;
          }
        }

	/*
	 * if at the end of the part list, add it
	 */

	if (fiter->NEXT == NULL || 
	    (data->partno != -1 && data->partno < fiter->NEXT->partno)) {
	  data->NEXT  = fiter->NEXT;
	  fiter->NEXT = data;

	  if (data->partno == -1)
	    data->partno = fiter->partno + 1;

	  return UURET_OK;
	}
        last  = fiter;
	fiter = fiter->NEXT;
      }
      
      return UURET_OK; /* Shouldn't get here */
    }
  goahead:
    /*
     * we need iter below
     */
    if (iter->NEXT == NULL) 
      break;

    iter = iter->NEXT;
  }
  /*
   * handle new entry
   */

  if (data->partno == -1) {
    /*
     * if it's got no part no, and it's MIME mail, then assume this is
     * part no. 1. If it's not MIME, then we can't handle it; if it
     * had a 'begin', it'd have got a part number assigned by
     * UUPreProcessPart().
     */
    if (data->data->uudet == B64ENCODED || data->data->uudet == BH_ENCODED)
      data->partno = 1;
    else
      return UURET_NODATA;
  }

  if ((unew = (uulist *) malloc (sizeof (uulist))) == NULL) {
    return UURET_NOMEM;
  }

  if ((unew->subfname = _FP_strdup (data->subfname)) == NULL) {
    _FP_free (unew);
    return UURET_NOMEM;
  }

  if (data->filename != NULL) {
    if ((unew->filename = _FP_strdup (data->filename)) == NULL) {
      _FP_free (unew->subfname);
      _FP_free (unew);
      return UURET_NOMEM;
    }
  }
  else
    unew->filename = NULL;

  if (data->mimeid != NULL) {
    if ((unew->mimeid = _FP_strdup (data->mimeid)) == NULL) {
      _FP_free (unew->subfname);
      _FP_free (unew->filename);
      _FP_free (unew);
      return UURET_NOMEM;
    }
  }
  else
    unew->mimeid = NULL;

  if (data->mimetype != NULL) {
    if ((unew->mimetype = _FP_strdup (data->mimetype)) == NULL) {
      _FP_free (unew->mimeid);
      _FP_free (unew->subfname);
      _FP_free (unew->filename);
      _FP_free (unew);
      return UURET_NOMEM;
    }
  }
  else
    unew->mimetype = NULL;

  unew->state     = UUFILE_READ;
  unew->binfile   = NULL;
  unew->thisfile  = data;
  unew->mode      = data->data->mode;
  unew->uudet     = data->data->uudet;
  unew->flags     = data->data->flags;
  unew->begin     = (data->data->begin) ? ((data->partno)?data->partno:1) : 0;
  unew->end       = (data->data->end)   ? ((data->partno)?data->partno:1) : 0;
  unew->misparts  = NULL;
  unew->haveparts = NULL;
  unew->NEXT      = NULL;

  if (iter == NULL)
    UUGlobalFileList = unew;
  else
    iter->NEXT = unew;

  return UURET_OK;
}

/*
 * At this point, all files are read in and stored in the
 * "UUGlobalFileList". Do some checking. All parts there?
 **/

uulist *
UUCheckGlobalList (void)
{
  int misparts[MAXPLIST], haveparts[MAXPLIST];
  int miscount, havecount, count, flag, part;
  uulist *liter=UUGlobalFileList, *prev;
  uufile *fiter;
  long thesize;

  while (liter) {
    miscount = 0;
    thesize  = 0;

    if (liter->state & UUFILE_OK) {
      liter = liter->NEXT;
      continue;
    }
    else if ((liter->uudet == QP_ENCODED ||
	      liter->uudet == PT_ENCODED) && 
	     (liter->flags & FL_SINGLE)) {
      if ((liter->flags&FL_PROPER)==0)
	liter->size = -1;
      else
	liter->size = liter->thisfile->data->length;

      liter->state = UUFILE_OK;
      continue;
    }
    else if ((fiter = liter->thisfile) == NULL) {
      liter->state = UUFILE_NODATA;
      liter = liter->NEXT;
      continue;
    }

    /*
     * Re-Check this file
     */

    flag      = 0;
    miscount  = 0;
    havecount = 0;
    thesize   = 0;
    liter->state = UUFILE_READ;

    /*
     * search encoded data
     */

    while (fiter && !fiter->data->uudet) {
      if (havecount<MAXPLIST) {
	haveparts[havecount++] = fiter->partno;
      }
      fiter = fiter->NEXT;
    }

    if (fiter == NULL) {
      liter->state = UUFILE_NODATA;
      liter = liter->NEXT;
      continue;
    }

    if (havecount<MAXPLIST) {
      haveparts[havecount++] = fiter->partno;
    }

    if ((part = fiter->partno) > 1) {
      if (!fiter->data->begin) {
	for (count=1; count < part && miscount < MAXPLIST; count++)
	  misparts[miscount++] = count;
      }
    }

    /*
     * don't care if so many parts are missing
     */

    if (miscount >= MAXPLIST) {
      liter->state = UUFILE_MISPART;
      liter        = liter->NEXT;
      continue;
    }

    if (liter->uudet == B64ENCODED ||
	liter->uudet == QP_ENCODED ||
	liter->uudet == PT_ENCODED)
      flag |= 3; /* Don't need begin or end with Base64 or plain text*/

    if (fiter->data->begin) flag |= 1;
    if (fiter->data->end)   flag |= 2;
    if (fiter->data->uudet) flag |= 4; 

    /*
     * guess size of part
     */

    switch (fiter->data->uudet) {
    case UU_ENCODED:
    case XX_ENCODED:
      thesize += 3*fiter->data->length/4;
      thesize -= 3*fiter->data->length/124; /* substract 2 of 62 chars */
      break;
    case B64ENCODED:
      thesize += 3*fiter->data->length/4;
      thesize -=  fiter->data->length/52;   /* substract 2 of 78 chars */
      break;
    case QP_ENCODED:
    case PT_ENCODED:
      thesize += fiter->data->length;
      break;
    }
      
    fiter = fiter->NEXT;

    while (fiter != NULL) {
      for (count=part+1; count<fiter->partno && miscount<MAXPLIST; count++)
	misparts[miscount++] = count;

      part = fiter->partno;
      
      if (havecount<MAXPLIST)
	haveparts[havecount++]=part;

      if (fiter->data->begin) flag |= 1;
      if (fiter->data->end)   flag |= 2;
      if (fiter->data->uudet) flag |= 4;

      switch (fiter->data->uudet) {
      case UU_ENCODED:
      case XX_ENCODED:
	thesize += 3*fiter->data->length/4;
	thesize -= 3*fiter->data->length/124; /* substract 2 of 62 chars */
	break;
      case B64ENCODED:
	thesize += 3*fiter->data->length/4;
	thesize -=  fiter->data->length/52;   /* substract 2 of 78 chars */
	break;
      case QP_ENCODED:
      case PT_ENCODED:
	thesize += fiter->data->length;
	break;
      }

      if (fiter->data->end)
	break;
	
      fiter = fiter->NEXT;
    }

    /*
     * if in fast mode, we don't notice an 'end'. So if its uu or xx
     * encoded, there's a begin line and encoded data, assume it's
     * there.
     */
    
    if (uu_fast_scanning && (flag & 0x01) && (flag & 0x04) &&
	(liter->uudet == UU_ENCODED || liter->uudet == XX_ENCODED))
      flag |= 2;

    /*
     * Set the parts we have and/or missing
     */

    _FP_free (liter->haveparts);
    _FP_free (liter->misparts);

    liter->haveparts = NULL;
    liter->misparts  = NULL;
    
    if (havecount) {
      if ((liter->haveparts=(int*)malloc((havecount+1)*sizeof(int)))!=NULL) {
	memcpy (liter->haveparts, haveparts, havecount*sizeof(int));
	liter->haveparts[havecount] = 0;
      }
    }
    
    if (miscount) {
      if ((liter->misparts=(int*)malloc((miscount+1)*sizeof(int)))!=NULL) {
	memcpy (liter->misparts, misparts, miscount*sizeof(int));
	liter->misparts[miscount] = 0;
      }
      liter->state |= UUFILE_MISPART;
    }

    /*
     * Finalize checking
     */

    if ((flag & 4) == 0) liter->state |= UUFILE_NODATA;
    if ((flag & 1) == 0) liter->state |= UUFILE_NOBEGIN;
    if ((flag & 2) == 0) liter->state |= UUFILE_NOEND;
    
    if ((flag & 7) == 7 && miscount==0) {
      liter->state = UUFILE_OK;
    }

    if ((uu_fast_scanning && (liter->flags&FL_PROPER)==0) || thesize<=0)
      liter->size = -1;
    else
      liter->size = thesize;

    if (liter->state==UUFILE_OK && 
        (liter->filename==NULL || liter->filename[0]=='\0')) {
      /*
       * Emergency backup if the file does not have a filename
       */
      _FP_free (liter->filename);
      if (liter->subfname && liter->subfname[0] &&
          _FP_strpbrk (liter->subfname, "()[];: ") == NULL)
        liter->filename = _FP_strdup (liter->subfname);
      else {
        sprintf (uucheck_tempname, "%s.%03d", nofname, ++nofnum);
        liter->filename = _FP_strdup (uucheck_tempname);
      }
    }
    liter = liter->NEXT;
  }

  /*
   * Sets back (PREV) links
   */

  liter = UUGlobalFileList;
  prev  = NULL;

  while (liter) {
    liter->PREV = prev;
    prev        = liter;
    liter       = liter->NEXT;
  }

  return UUGlobalFileList;
}

