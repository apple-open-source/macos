/* quote.c
   Quote a UUCP command.

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
const char quote_rcsid[] = "$Id: quote.c,v 1.2 2002/03/05 19:10:42 ian Rel $";
#endif

#include "uudefs.h"

/* Local functions.  */

__inline__ static boolean fneeds_quotes P((const char *z));

/* Return whether a string needs quotes.  We want to be conservative
   here--we don't want to reject a string which would work with an
   older UUCP version.  */

__inline__
static boolean
fneeds_quotes (z)
     const char *z;
{
  return z != NULL && z[strcspn (z, " \t\n")] != '\0';
}

/* Return whether a command needs quotes.  */

boolean
fcmd_needs_quotes (qcmd)
     const struct scmd *qcmd;
{
  if (fneeds_quotes (qcmd->zfrom)
      || fneeds_quotes (qcmd->zto)
      || fneeds_quotes (qcmd->zuser)
      || fneeds_quotes (qcmd->znotify))
    return TRUE;

  /* We don't check qcmd->zcmd.  It is already permitted to have
     spaces, and uux will never generate a command with an embedded
     newline.  */

  return FALSE;
}

/* Quote the strings which appear in a UUCP command string.  Add 'q'
   to the list of options.  This creates a new command in qnew, with
   freshly allocated strings.  */

void
uquote_cmd (qorig, qnew)
     const struct scmd *qorig;
     struct scmd *qnew;
{
  qnew->bcmd = qorig->bcmd;
  qnew->bgrade = qorig->bgrade;
  qnew->pseq = qorig->pseq;
  qnew->zfrom = zquote_cmd_string (qorig->zfrom, FALSE);
  qnew->zto = zquote_cmd_string (qorig->zto, FALSE);
  qnew->zuser = zquote_cmd_string (qorig->zuser, FALSE);

  if (strchr (qorig->zoptions, 'q') != NULL)
    qnew->zoptions = zbufcpy (qorig->zoptions);
  else
    {
      size_t clen;
      char *z;

      clen = strlen (qorig->zoptions);
      z = zbufalc (clen + 2);
      memcpy (z, qorig->zoptions, clen);
      z[clen] = 'q';
      z[clen + 1] = '\0';
      qnew->zoptions = z;
    }

  qnew->ztemp = zbufcpy (qorig->ztemp);
  qnew->imode = qorig->imode;
  qnew->znotify = zquote_cmd_string (qorig->znotify, FALSE);
  qnew->cbytes = qorig->cbytes;

  /* The zcmd field is never quoted.  */
  qnew->zcmd = zbufcpy (qorig->zcmd);

  qnew->ipos = qorig->ipos;
}

/* Free a command structure created by uquote_cmd.  */

void
ufree_quoted_cmd (qcmd)
     struct scmd *qcmd;
{
  ubuffree ((char *) qcmd->zfrom);
  ubuffree ((char *) qcmd->zto);
  ubuffree ((char *) qcmd->zuser);
  ubuffree ((char *) qcmd->ztemp);
  ubuffree ((char *) qcmd->znotify);
  ubuffree ((char *) qcmd->zcmd);
  ubuffree ((char *) qcmd->zoptions);
}
