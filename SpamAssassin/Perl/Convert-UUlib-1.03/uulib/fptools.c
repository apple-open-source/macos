/*
 * fptools.c, some helper functions for getcgi.c and uu(en|de)view
 *
 * Distributed under the terms of the GNU General Public License.
 * Use and be happy.
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
 * This file provides replacements for some handy functions that aren't
 * available on all systems, like most of the <string.h> functions. They
 * should behave exactly as their counterparts. There are also extensions
 * that aren't portable at all (like strirstr etc.).
 * The proper behaviour in a configure script is as follows:
 *    AC_CHECK_FUNC(strrchr,AC_DEFINE(strrchr,_FP_strrchr))
 * This way, the (probably less efficient) replacements will only be used
 * where it is not provided by the default libraries. Be aware that this
 * does not work with replacements that just shadow wrong behaviour (like
 * _FP_free) or provide extended functionality (FP_gets).
 * The above is not used in the uuenview/uudeview configuration script,
 * since both only use the replacement functions in non-performance-cri-
 * tical sections (except for _FP_tempnam and FP_strerror, where some
 * functionality of the original would be lost).
 */

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

#include <fptools.h>

#if 0
#ifdef SYSTEM_WINDLL
BOOL _export WINAPI
DllEntryPoint (HINSTANCE hInstance, DWORD seginfo,
	       LPVOID lpCmdLine)
{
  /* Don't do anything, so just return true */
  return TRUE;
}
#endif
#endif

char * fptools_id = "$Id: fptools.c,v 1.1 2004/05/14 15:23:05 dasenbro Exp $";

/*
 * some versions of free can't handle a NULL pointer properly
 * (ANSI says, free ignores a NULL pointer, but some machines
 * prefer to SIGSEGV on it)
 */

void TOOLEXPORT
_FP_free (void *ptr)
{
  if (ptr) free (ptr);
}

/*
 * This is non-standard, so I'm defining my own
 */

char * TOOLEXPORT
_FP_strdup (char *string)
{
  char *result;

  if (string == NULL)
    return NULL;

  if ((result = (char *) malloc (strlen (string) + 1)) == NULL)
    return NULL;

  strcpy (result, string);
  return result;
}

/*
 * limited-length string copy. this function behaves differently from
 * the original in that the dest string is always terminated with a
 * NULL character.
 */

char * TOOLEXPORT
_FP_strncpy (char *dest, char *src, int length)
{
  char *odest=dest;
  if (src == NULL || dest == NULL || length-- <= 0)
    return dest;

  while (length-- && *src)
    *dest++ = *src++;

  *dest++ = '\0';
  return odest;
}

/*
 * duplicate a memory area
 */

void * TOOLEXPORT
_FP_memdup (void *ptr, int len)
{
  void *result;

  if (ptr == NULL)
    return NULL;

  if ((result = malloc (len)) == NULL)
    return NULL;

  memcpy (result, ptr, len);
  return result;
}

/*
 * case-insensitive compare
 */

int TOOLEXPORT
_FP_stricmp (char *str1, char *str2)
{
  if (str1==NULL || str2==NULL)
    return -1;

  while (*str1) {
    if (tolower(*str1) != tolower(*str2))
      break;
    str1++;
    str2++;
  }
  return (tolower (*str1) - tolower (*str2));
}

int TOOLEXPORT
_FP_strnicmp (char *str1, char *str2, int count)
{
  if (str1==NULL || str2==NULL)
    return -1;

  while (*str1 && count) {
    if (tolower(*str1) != tolower(*str2))
      break;
    str1++;
    str2++;
    count--;
  }
  return count ? (tolower (*str1) - tolower (*str2)) : 0;
}

/*
 * autoconf says this function might be a compatibility problem
 */

char * TOOLEXPORT
_FP_strstr (char *str1, char *str2)
{
  char *ptr1, *ptr2;

  if (str1==NULL)
    return NULL;
  if (str2==NULL)
    return str1;

  while (*(ptr1=str1)) {
    for (ptr2=str2;
	 *ptr1 && *ptr2 && *ptr1==*ptr2;
	 ptr1++, ptr2++)
      /* empty loop */ ;

    if (*ptr2 == '\0')
      return str1;
    str1++;
  }
  return NULL;
}

char * TOOLEXPORT
_FP_strpbrk (char *str, char *accept)
{
  char *ptr;

  if (str == NULL)
    return NULL;
  if (accept == NULL || *accept == '\0')
    return str;

  for (; *str; str++)
    for (ptr=accept; *ptr; ptr++)
      if (*str == *ptr)
	return str;

  return NULL;
}

/*
 * autoconf also complains about this one
 */

char * TOOLEXPORT
_FP_strtok (char *str1, char *str2)
{
  static char *optr;
  char *ptr;

  if (str2 == NULL)
    return NULL;

  if (str1) {
    optr = str1;
  }
  else {
    if (*optr == '\0')
      return NULL;
  }

  while (*optr && strchr (str2, *optr))	/* look for beginning of token */
    optr++;

  if (*optr == '\0')			/* no token found */
    return NULL;

  ptr = optr;
  while (*optr && strchr (str2, *optr) == NULL) /* look for end of token */
    optr++;

  if (*optr) {
    *optr++ = '\0';
  }
  return ptr;
}

/*
 * case insensitive strstr.
 */

char * TOOLEXPORT
_FP_stristr (char *str1, char *str2)
{
  char *ptr1, *ptr2;

  if (str1==NULL)
    return NULL;
  if (str2==NULL)
    return str1;

  while (*(ptr1=str1)) {
    for (ptr2=str2;
	 *ptr1 && *ptr2 && tolower(*ptr1)==tolower(*ptr2);
	 ptr1++, ptr2++)
      /* empty loop */ ;

    if (*ptr2 == '\0')
      return str1;
    str1++;
  }
  return NULL;
}

/*
 * Nice fake of the real (non-standard) one
 */

char * TOOLEXPORT
_FP_strrstr (char *ptr, char *str)
{
  char *found=NULL, *new, *iter=ptr;

  if (ptr==NULL || str==NULL)
    return NULL;

  if (*str == '\0')
    return ptr;

  while ((new = _FP_strstr (iter, str)) != NULL) {
    found = new;
    iter  = new + 1;
  }
  return found;
}

char * TOOLEXPORT
_FP_strirstr (char *ptr, char *str)
{
  char *found=NULL, *iter=ptr, *new;

  if (ptr==NULL || str==NULL)
    return NULL;
  if (*str == '\0')
    return ptr;

  while ((new = _FP_stristr (iter, str)) != NULL) {
    found = new;
    iter  = new + 1;
  }
  return found;
}

/*
 * convert whole string to case
 */

char * TOOLEXPORT
_FP_stoupper (char *input)
{
  char *iter = input;

  if (input == NULL)
    return NULL;

  while (*iter) {
    *iter = toupper (*iter);
    iter++;
  }
  return input;
}

char * TOOLEXPORT
_FP_stolower (char *input)
{
  char *iter = input;

  if (input == NULL)
    return NULL;

  while (*iter) {
    *iter = tolower (*iter);
    iter++;
  }
  return input;
}

/*
 * string matching with wildcards
 */

int TOOLEXPORT
_FP_strmatch (char *string, char *pattern)
{
  char *p1 = string, *p2 = pattern;

  if (pattern==NULL || string==NULL)
    return 0;

  while (*p1 && *p2) {
    if (*p2 == '?') {
      p1++; p2++;
    }
    else if (*p2 == '*') {
      if (*++p2 == '\0')
	return 1;
      while (*p1 && *p1 != *p2)
	p1++;
    }
    else if (*p1 == *p2) {
      p1++; p2++;
    }
    else
      return 0;
  }
  if (*p1 || *p2)
    return 0;

  return 1;
}

char * TOOLEXPORT
_FP_strrchr (char *string, int tc)
{
  char *ptr;

  if (string == NULL || !*string)
    return NULL;

  ptr = string + strlen (string) - 1;

  while (ptr != string && *ptr != tc)
    ptr--;

  if (*ptr == tc)
    return ptr;

  return NULL;
}

/*
 * strip directory information from a filename. Works only on DOS and
 * Unix systems so far ...
 */

char * TOOLEXPORT
_FP_cutdir (char *filename)
{
  char *ptr;

  if (filename == NULL)
    return NULL;

  if ((ptr = _FP_strrchr (filename, '/')) != NULL)
    ptr++;
  else if ((ptr = _FP_strrchr (filename, '\\')) != NULL)
    ptr++;
  else
    ptr = filename;

  return ptr;
}

/*
 * My own fgets function. It handles all kinds of line terminators
 * properly: LF (Unix), CRLF (DOS) and CR (Mac). In all cases, the
 * terminator is replaced by a single LF
 */

char * TOOLEXPORT
_FP_fgets (char *buf, int n, FILE *stream)
{
  char *obp = buf;
  int c;

  /* shield against buffer overflows caused by "255 - bytes_left"-kind of bugs when bytes_left > 255 */
  if (n <= 0)
    return NULL;

  if (feof (stream))
    return NULL;

  while (--n && !feof (stream)) {
    if ((c = fgetc (stream)) == EOF) {
      if (ferror (stream))
	return NULL;
      else {
	if (obp == buf)
	  return NULL;
	*buf = '\0';
	return obp;
      }
    }
    if (c == '\015') { /* CR */
      /*
       * Peek next character. If it's no LF, push it back.
       * ungetc(EOF, stream) is handled correctly according
       * to the manual page
       */
      if ((c = fgetc (stream)) != '\012')
	if (!feof (stream))
	  ungetc (c, stream);
      *buf++ = '\012';
      *buf   = '\0';
      return obp;
    }
    else if (c == '\012') { /* LF */
      *buf++ = '\012';
      *buf   = '\0';
      return obp;
    }
    /*
     * just another standard character
     */
    *buf++ = c;
  }

  /*
   * n-1 characters already transferred
   */

  *buf = '\0';

  /*
   * If a line break is coming up, read it
   */

  if (!feof (stream)) {
    if ((c = fgetc (stream)) == '\015' && !feof (stream)) {
      if ((c = fgetc (stream)) != '\012' && !feof (stream)) {
	ungetc (c, stream);
      }
    }
    else if (c != '\012' && !feof (stream)) {
      ungetc (c, stream);
    }
  }

  return obp;
}

/*
 * A replacement strerror function that just returns the error code
 */

char * TOOLEXPORT
_FP_strerror (int errcode)
{
  static char number[8];

  sprintf (number, "%03d", errcode);

  return number;
}
#ifndef HAVE_MKSTEMP
/*
 * tempnam is not ANSI, but tmpnam is. Ignore the prefix here.
 */

char * TOOLEXPORT
_FP_tempnam (char *dir, char *pfx)
{
  return _FP_strdup (tmpnam (NULL));
}
#endif /* HAVE_MKSTEMP */
