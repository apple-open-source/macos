/* Shell quoting.
   Copyright (C) 2001-2003 Free Software Foundation, Inc.
   Written by Bruno Haible <haible@clisp.cons.org>, 2001.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* Specification.  */
#include "sh-quote.h"

#include <string.h>

#include "strpbrk.h"
#include "xalloc.h"


/* Must quote the program name and arguments since Unix shells interpret
   characters like " ", "'", "<", ">", "$" etc. in a special way.  This
   kind of quoting should work unless the string contains "\n" and we call
   csh.  But we are lucky: only /bin/sh will be used.  */

#define SHELL_SPECIAL_CHARS "\t\n !\"#$&'()*;<=>?[\\]`{|}~"

/* Returns the number of bytes needed for the quoted string.  */
size_t
shell_quote_length (const char *string)
{
  if (string[0] == '\0')
    return 2;
  else if (strpbrk (string, SHELL_SPECIAL_CHARS) == NULL)
    return strlen (string);
  else
    {
      char qchar = '\0';	/* last quote character: none or ' or " */
      size_t length = 0;

      for (; *string != '\0'; string++)
	{
	  char c = *string;
	  char q = (c == '\'' ? '"' : '\'');

	  if (qchar != q)
	    {
	      if (qchar)
		length++;
	      qchar = q;
	      length++;
	    }
	  length++;
	}
      if (qchar)
	length++;

      return length;
    }
}

/* Copies the quoted string to p and returns the incremented p.  */
char *
shell_quote_copy (char *p, const char *string)
{
  if (string[0] == '\0')
    {
      memcpy (p, "''", 2);
      return p + 2;
    }
  else if (strpbrk (string, SHELL_SPECIAL_CHARS) == NULL)
    {
      memcpy (p, string, strlen (string));
      return p + strlen (string);
    }
  else
    {
      char qchar = '\0';	/* last quote character: none or ' or " */

      for (; *string != '\0'; string++)
	{
	  char c = *string;
	  char q = (c == '\'' ? '"' : '\'');

	  if (qchar != q)
	    {
	      if (qchar)
		*p++ = qchar;
	      qchar = q;
	      *p++ = qchar;
	    }
	  *p++ = c;
	}
      if (qchar)
	*p++ = qchar;

      return p;
    }
}

/* Returns the freshly allocated quoted string.  */
char *
shell_quote (const char *string)
{
  size_t length = shell_quote_length (string);
  char *quoted = (char *) xmalloc (length + 1);
  char *p = shell_quote_copy (quoted, string);
  *p = '\0';
  return quoted;
}

/* Returns a freshly allocated string containing all argument strings, quoted,
   separated through spaces.  */
char *
shell_quote_argv (char **argv)
{
  if (*argv != NULL)
    {
      char **argp;
      size_t length;
      char *command;
      char *p;

      length = 0;
      for (argp = argv; ; )
	{
	  length += shell_quote_length (*argp) + 1;
	  argp++;
	  if (*argp == NULL)
	    break;
	}

      command = (char *) xmalloc (length);

      p = command;
      for (argp = argv; ; )
	{
	  p = shell_quote_copy (p, *argp);
	  argp++;
	  if (*argp == NULL)
	    break;
	  *p++ = ' ';
	}
      *p = '\0';

      return command;
    }
  else
    return xstrdup ("");
}
