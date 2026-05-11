/*
  tre_regcomp.c - TRE POSIX compatible regex compilation functions.

  This software is released under a BSD-style license.
  See the file LICENSE for details and copyright.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "tre.h"
#include "tre-internal.h"
#include "xmalloc.h"

#ifndef BUILDING_VARIANT
int
tre_regncomp_l(regex_t *preg, const char *regex, size_t n, int cflags, locale_t loc)
{
  int ret;

  if (n > TRE_MAX_RE)
    return REG_ESPACE;
#if TRE_WCHAR
  tre_char_t *wregex;
  size_t wlen;

  wregex = xmalloc(sizeof(tre_char_t) * (n + 1));
  if (wregex == NULL)
    return REG_ESPACE;
#ifdef __LIBC__
  NORMALIZE_LOCALE(loc);
#endif /* __LIBC__ */

  /* If the current locale uses the standard single byte encoding of
     characters, we don't do a multibyte string conversion.  If we did,
     many applications which use the default locale would break since
     the default "C" locale uses the 7-bit ASCII character set, and
     all characters with the eighth bit set would be considered invalid. */
#if TRE_MULTIBYTE
  if (TRE_MB_CUR_MAX_L(loc) == 1)
#endif /* TRE_MULTIBYTE */
    {
      size_t i;
      const unsigned char *str = (const unsigned char *)regex;
      tre_char_t *wstr = wregex;

      for (i = 0; i < n; i++)
	*(wstr++) = *(str++);
      wlen = n;
    }
#if TRE_MULTIBYTE
  else
    {
      size_t consumed;
      tre_char_t *wcptr = wregex;
#ifdef HAVE_MBSTATE_T
      mbstate_t state;
      memset(&state, '\0', sizeof(state));
#endif /* HAVE_MBSTATE_T */
      while (n > 0)
	{
	  consumed = tre_mbrtowc_l(wcptr, regex, n, &state, loc);

	  switch (consumed)
	    {
	    case 0:
	      if (*regex == '\0')
		consumed = 1;
	      else
		{
		  xfree(wregex);
		  return REG_BADPAT;
		}
	      break;
	    case (size_t)-1:
	    case (size_t)-2:
	      DPRINT(("mbrtowc: error %d: %s.\n", errno, strerror(errno)));
	      xfree(wregex);
	      return REG_ILLSEQ;
	    }
	  regex += consumed;
	  n -= consumed;
	  wcptr++;
	}
      wlen = wcptr - wregex;
    }
#endif /* TRE_MULTIBYTE */

  wregex[wlen] = L'\0';
  ret = tre_compile(preg, wregex, wlen, cflags, loc);
  xfree(wregex);
#else /* !TRE_WCHAR */
#ifdef __LIBC__
  NORMALIZE_LOCALE(loc);
#endif /* __LIBC__ */

  ret = tre_compile(preg, (const tre_char_t *)regex, n, cflags, loc);
#endif /* !TRE_WCHAR */

  return ret;
}

#ifndef __APPLE__
/* this version takes bytes literally, to be used with raw vectors */
int
tre_regncompb(regex_t *preg, const char *regex, size_t n, int cflags)
{
  int ret;
  if (n > TRE_MAX_RE)
    return REG_ESPACE;
#if TRE_WCHAR /* wide chars = we need to convert it all to the wide format */
  tre_char_t *wregex;
  size_t i;

  wregex = xmalloc(sizeof(tre_char_t) * n);
  if (wregex == NULL)
    return REG_ESPACE;

  for (i = 0; i < n; i++)
    wregex[i] = (tre_char_t) ((unsigned char) regex[i]);

  ret = tre_compile(preg, wregex, n, cflags | REG_USEBYTES);
  xfree(wregex);
#else /* !TRE_WCHAR */
  ret = tre_compile(preg, (const tre_char_t *)regex, n, cflags | REG_USEBYTES);
#endif /* !TRE_WCHAR */

  return ret;
}
#endif /* !__APPLE__ */

int
tre_regncomp(regex_t *preg, const char *regex, size_t n, int cflags)
{
  locale_t loc;

#ifdef __LIBC__
  loc = __current_locale();
#else /* !__LIBC__ */
  if ((loc = duplocale(NULL)) == NULL)
    return REG_ESPACE;
#endif /* !__LIBC__ */

  return tre_regncomp_l(preg, regex, n, cflags, loc);
}

int
tre_regcomp_l(regex_t *preg, const char *regex, int cflags, locale_t loc)
{
  size_t len;

#ifdef REG_PEND
  if (cflags & REG_PEND)
    {
      if (regex == NULL || preg->re_endp == NULL ||
	  (const char *)preg->re_endp < regex)
	return REG_INVARG;
      len = (const char *)preg->re_endp - regex;
    }
  else
#endif /* REG_PEND */
    len = regex ? strlen(regex) : 0;
  if (len > TRE_MAX_RE)
    return REG_ESPACE;
  return tre_regncomp(preg, regex, len, cflags);
}
#endif /* !BUILDING_VARIANT */

int
tre_regcomp(regex_t *preg, const char *regex, int cflags)
{
  locale_t loc;

#ifdef __LIBC__
  loc = __current_locale();
#else /* !__LIBC__ */
  if ((loc = duplocale(NULL)) == NULL)
    return REG_ESPACE;
#endif /* !__LIBC__ */

  return tre_regcomp_l(preg, regex, cflags, loc);
}

#ifndef __APPLE__
int
tre_regcompb(regex_t *preg, const char *regex, int cflags)
{
  int ret;
  tre_char_t *wregex;
  size_t i, n = regex ? strlen(regex) : 0;
  const unsigned char *str = (const unsigned char *)regex;
  tre_char_t *wstr;

  if (n > TRE_MAX_RE)
    return REG_ESPACE;
  wregex = xmalloc(sizeof(tre_char_t) * (n + 1));
  if (wregex == NULL) return REG_ESPACE;
  wstr = wregex;

  for (i = 0; i < n; i++)
    *(wstr++) = *(str++);
  wregex[n] = L'\0';
  ret = tre_compile(preg, wregex, n, cflags | REG_USEBYTES);
  xfree(wregex);
  return ret;
}
#endif /* !__APPLE__ */


#ifndef BUILDING_VARIANT
#ifdef TRE_WCHAR
int
tre_regwncomp_l(regex_t *preg, const wchar_t *regex, size_t n, int cflags, locale_t loc)
{
  if (n > TRE_MAX_RE)
    return REG_ESPACE;
#ifdef __LIBC__
  NORMALIZE_LOCALE(loc);
#endif /* __LIBC__ */
  return tre_compile(preg, regex, n, cflags, loc);
}

int
tre_regwncomp(regex_t *preg, const wchar_t *regex, size_t n, int cflags)
{
  locale_t loc;

#ifdef __LIBC__
  loc = __current_locale();
#else /* !__LIBC__ */
  if ((loc = duplocale(NULL)) == NULL)
    return REG_ESPACE;
#endif /* !__LIBC__ */

  return tre_compile(preg, regex, n, cflags, loc);
}

int
tre_regwcomp_l(regex_t *preg, const wchar_t *regex, int cflags, locale_t loc)
{
  size_t n = regex ? wcslen(regex) : 0;

  if (n > TRE_MAX_RE)
    return REG_ESPACE;
#ifdef __LIBC__
  NORMALIZE_LOCALE(loc);
#endif /* __LIBC__ */
  return tre_compile(preg, regex, n, cflags, loc);
}

int
tre_regwcomp(regex_t *preg, const wchar_t *regex, int cflags)
{
  locale_t loc;

#ifdef __LIBC__
  loc = __current_locale();
#else /* !__LIBC__ */
  if ((loc = duplocale(NULL)) == NULL)
    return REG_ESPACE;
#endif /* !__LIBC__ */

  return tre_regwcomp_l(preg, regex, cflags, loc);
}
#endif /* TRE_WCHAR */

void
tre_regfree(regex_t *preg)
{
  tre_free(preg);
}
#endif /* !BUILDING_VARIANT */

/* EOF */
