/*
 *   subfont.c
 *
 *   This file is part of the ttf2pk package.
 *
 *   Copyright 1997-1999 by
 *     Frederic Loyer <loyer@ensta.fr>
 *     Werner Lemberg <wl@gnu.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>         /* for size_t */
#include <ctype.h>
#include <string.h>

#include "filesrch.h"
#include "subfont.h"
#include "newobj.h"
#include "errormsg.h"


static char *real_sfd_name;
static FILE *sfd;


/*
 *   Initialize subfont functionality.  The argument is the subfont
 *   definition file name.  If `fatal' is `True', the routine exits
 *   with an error.  If `fatal' is `False', a warning message is emitted
 *   and `False' returned if an error occurs; in case of success `True'
 *   will be returned.
 */

Boolean
init_sfd(Font *fnt, Boolean fatal)
{
  real_sfd_name = TeX_search_sfd_file(&(fnt->sfdname));
  if (!real_sfd_name)
  {
    if (fatal)
      oops("Cannot find subfont definition file `%s'.", fnt->sfdname);
    else
    {
      warning("Cannot find subfont definition file `%s'.", fnt->sfdname);
      return False;
    }
  }

  sfd = fopen(real_sfd_name, "rt");
  if (sfd == NULL)
  {
    if (fatal)
      oops("Cannot open subfont definition file `%s'.", fnt->sfdname);
    else
    {
      warning("Cannot open subfont definition file `%s'.", fnt->sfdname);
      return False;
    }
  }

  return True;
}


/*
 *   This function fills the font structure sequentially with subfont
 *   entries; it returns `False' if no more subfont entries are available,
 *   `True' otherwise.
 *
 *   fnt->subfont_name must be set to NULL before the first call.
 *
 *   The subset parser was inspired by ttf2bdf.c .
 */

Boolean
get_sfd(Font *fnt)
{
  long i, offset;
  long begin, end = -1;
  char *buffer, *oldbuffer, *bufp, *bufp2, *bufp3;


  for (i = 0; i < 256; i++)
    fnt->sf_code[i] = -1;

again:

  buffer = get_line(sfd);
  if (!buffer)
    oops("Error reading subfont definition file `%s'.", real_sfd_name);
  if (!*buffer)
    return False;

  oldbuffer = newstring(buffer);
  bufp = buffer;
  offset = 0;

  while (*bufp)             /* remove comment */
  {
    if (*bufp == '#')
    {
      bufp++;
      break;
    }
    bufp++;
  }
  *(--bufp) = '\0';         /* remove final newline character */

  bufp = buffer;

  while (isspace(*bufp))
    bufp++;

  if (*bufp == '\0')                    /* empty line? */
  {
    free(buffer);
    free(oldbuffer);
    goto again;
  }

  while (*bufp && !isspace(*bufp))      /* subfont name */
    bufp++;
  *(bufp++) = '\0';

  while (isspace(*bufp))
    bufp++;

  if (*bufp == '\0')
    oops("Invalid subfont entry in `%s'.", real_sfd_name);

  if (fnt->subfont_name)
    free(fnt->subfont_name);
  fnt->subfont_name = newstring(buffer);

  while (1)
  {
    bufp3 = bufp;

    begin = strtol(bufp, &bufp2, 0);

    if (bufp == bufp2 || begin < 0 || begin > 0xFFFF)
      boops(oldbuffer, bufp - buffer,
            "Invalid subfont range or offset entry.");

    if (*bufp2 == ':')                  /* offset */
    {
      offset = begin;
      if (offset > 0xFF)
        boops(oldbuffer, bufp - buffer, "Invalid subfont offset.");

      bufp = bufp2 + 1;

      while (isspace(*bufp))
        bufp++;

      continue;
    }
    else if (*bufp2 == '_')             /* range */
    {
      bufp = bufp2 + 1;
      if (!isdigit(*bufp))
        boops(oldbuffer, bufp - buffer, "Invalid subfont range entry.");

      end = strtol(bufp, &bufp2, 0);

      if (bufp == bufp2 || end < 0 || end > 0xFFFFL)
        boops(oldbuffer, bufp - buffer, "Invalid subfont range entry.");
      if (*bufp2 && !isspace(*bufp2))
        boops(oldbuffer, bufp2 - buffer, "Invalid subfont range entry.");
      if (end < begin)
        boops(oldbuffer, bufp - buffer, "End of subfont range too small.");
      if (offset + (end - begin) > 255)
        boops(oldbuffer, bufp3 - buffer,
              "Subfont range too large for current offset (%i).", offset);
    }
    else if (isspace(*bufp2) || !*bufp2)        /* single value */
      end = begin;
    else
      boops(oldbuffer, bufp2 - buffer, "Invalid subfont range entry.");

    for (i = begin; i <= end; i++)
    {
      if (fnt->sf_code[offset] != -1)
        boops(oldbuffer, bufp3 - buffer, "Overlapping subfont ranges.");

      fnt->sf_code[offset++] = i;
    }

    bufp = bufp2;

    while (isspace(*bufp))
      bufp++;

    if (!*bufp)
      break;
  }

  free(buffer);
  free(oldbuffer);

  return True;
}


void
close_sfd(void)
{
  if (sfd)
    fclose(sfd);
}


/*
 *   We extract the subfont definition file name.  The name must
 *   be embedded between two `@' characters.  If there is no sfd file,
 *   `sfd_begin' is set to -1.
 *
 *   The `@' characters will be replaced with null characters.
 */

void
handle_sfd(char *s, int *sfd_begin, int *postfix_begin)
{
  size_t len;
  int i;
  Boolean have_atsign;


  have_atsign = False;
  len = strlen(s);
  *sfd_begin = -1;
  *postfix_begin = -1;

  for (i = 0; s[i]; i++)
  {
    if (s[i] == '@')
    {
      if (have_atsign)
      {
        *postfix_begin = i + 1;

        s[i] = '\0';
        break;
      }
      have_atsign = True;
      *sfd_begin = i + 1;

      s[i] = '\0';
    }
  }

  if (*sfd_begin != -1 &&
      (*postfix_begin == -1 || *postfix_begin < *sfd_begin + 2))
    oops("Invalid subfont definition file name.");

  if (*postfix_begin > -1)
    for (i = *postfix_begin; s[i]; i++)
      if (s[i] == '/' || s[i] == ':' || s[i] == '\\' || s[i] == '@')
        oops("`/', `:', `\\', and `@' not allowed after second `@'.");
}


/* end */
