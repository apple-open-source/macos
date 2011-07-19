/*
 * The Single Unix Specification function nl_langinfo(CODESET)
 * returns the name of the encoding used by the currently selected
 * locale:
 *
 *   http://www.opengroup.org/onlinepubs/7908799/xsh/langinfo.h.html
 *
 * Unfortunately the encoding names are not yet standardized.
 * This function knows about the encoding names used on many
 * different systems and converts them where possible into
 * the corresponding MIME charset name registered in
 *
 *   http://www.iana.org/assignments/character-sets
 *
 * Please extend it as needed and suggest improvements to the author.
 *
 * Markus.Kuhn@cl.cam.ac.uk -- 2002-03-11
 * Permission to use, copy, modify, and distribute this software
 * for any purpose and without fee is hereby granted. The author
 * disclaims all warranties with regard to this software.
 *
 * Latest version:
 *
 *   http://www.cl.cam.ac.uk/~mgk25/ucs/norm_charmap.c
 */

#include "config.h" /* import AC_C_CONST effects */
#include "norm_charmap.h"

#include <string.h>

#ifdef TEST
#include <stdio.h>
#include <locale.h>
#include <langinfo.h>
#endif

#define digit(x) ((x) >= '0' && (x) <= '9')

static char buf[16];

const char *norm_charmap(const char *name)
{
  const char *p;
  
  if (!name)
    return name;
  
  /* Many need no remapping, but they are listed here so you
   * can see what output to expect, and modify for your needs
   * as necessary. */
  if (!strcmp(name, "UTF-8"))
    return "UTF-8";
  if (!strcmp(name, "EUC-JP"))
    return "EUC-JP";
  if (!strcmp(name, "EUC-KR"))
    return "EUC-KR";
  if (!strcmp(name, "EUC-TW"))
    return "EUC-TW";
  if (!strcmp(name, "KOI8-R"))
    return "KOI8-R";
  if (!strcmp(name, "KOI8-U"))
    return "KOI8-U";
  if (!strcmp(name, "GBK"))
    return "GBK";
  if (!strcmp(name, "GB2312"))
    return "GB2312";
  if (!strcmp(name, "GB18030"))
    return "GB18030";
  if (!strcmp(name, "VSCII"))
    return "VSCII";
  
  /* ASCII comes in many names */
  if (!strcmp(name, "ASCII") ||
      !strcmp(name, "US-ASCII") ||
      !strcmp(name, "ANSI_X3.4-1968") ||
      !strcmp(name, "646") ||
      !strcmp(name, "ISO646") ||
      !strcmp(name, "ISO_646.IRV"))
    return "US-ASCII";

  /* ISO 8859 will be converted to "ISO-8859-x" */
  if ((p = strstr(name, "8859-"))) {
    memcpy(buf, "ISO-8859-\0\0", 12);
    p += 5;
    if (digit(*p)) {
      buf[9] = *p++;
      if (digit(*p)) buf[10] = *p;
      return buf;
    }
  }

  /* Windows code pages will be converted to "WINDOWS-12xx" */
  if ((p = strstr(name, "CP12"))) {
    memcpy(buf, "WINDOWS-12\0\0", 13);
    p += 4;
    if (digit(*p)) {
      buf[10] = *p++;
      if (digit(*p)) buf[11] = *p;
      return buf;
    }
  }

  /* TIS-620 comes in at least the following two forms */
  if (!strcmp(name, "TIS-620") ||
      !strcmp(name, "TIS620.2533"))
    return "ISO-8859-11";

  /* For some, uppercase/lowercase might differ */
  if (!strcmp(name, "Big5") || !strcmp(name, "BIG5"))
    return "Big5";
  if (!strcmp(name, "Big5HKSCS") || !strcmp(name, "BIG5HKSCS"))
    return "Big5HKSCS";

  /* I don't know of any implementation of nl_langinfo(CODESET) out
   * there that returns anything else (and I'm not even certain all of
   * the above occur in the wild), but just in case, as a fallback,
   * return the unmodified name. */
#ifdef TEST
  printf("**** Unknown encoding name '%s'!\n", name);
#endif
  return name;
}

/* For a demo, compile with
 *
 *   gcc -W -Wall -o norm_charmap -D TEST norm_charmap.c
 *
 * and then test it on all available locales with
 *
 *   for i in `locale -a` ; do printf "$i: " ; LC_ALL=$i ./norm_charmap ; done
 */

#ifdef TEST
int main(int argc, char **argv)
{
  char *s;
  if (argc > 1)
    s = argv[1];
  else {
    setlocale(LC_CTYPE, "");
    s = nl_langinfo(CODESET);
  }
  printf("%s -> %s\n", s, norm_charmap(s));
  return 0;
}
#endif
