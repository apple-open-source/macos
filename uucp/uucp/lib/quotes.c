/* quotes.c
   Quote a field in a UUCP command.

   Copyright (C) 2002 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com.
   */

#include "uucp.h"

#if USE_RCS_ID
const char quotes_rcsid[] = "$Id: quotes.c,v 1.2 2002/03/05 19:10:42 ian Rel $";
#endif

#include <ctype.h>

#include "uudefs.h"

/* Copy a string, adding quotes if necessary.  */

char *
zquote_cmd_string (zorig, fbackslashonly)
     const char *zorig;
     boolean fbackslashonly;
{
  const char *z;
  char *zret;
  char *zto;

  if (zorig == NULL)
    return NULL;

  zret = zbufalc (strlen (zorig) * 4 + 1);
  zto = zret;
  for (z = zorig; *z != '\0'; ++z)
    {
      if (*z == '\\')
	{
	  *zto++ = '\\';
	  *zto++ = '\\';
	}
      else if (fbackslashonly || isgraph (BUCHAR (*z)))
	*zto++ = *z;
      else
	{
	  sprintf (zto, "\\%03o", (unsigned int) BUCHAR (*z));
	  zto += strlen (zto);
	}
    }

  *zto = '\0';

  return zret;
}
