/* uid.c
   Switch back and forth between UUCP and user permissions.

   Copyright (C) 1992, 1995, 2002 Ian Lance Taylor

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

#include "uudefs.h"
#include "sysdep.h"

#include <errno.h>

/* NetBSD apparently does not support setuid as required by POSIX when
   using saved setuid, so use seteuid instead.  */

#if HAVE_SETEUID
#define setuid seteuid
#endif

/* Switch to permissions of the invoking user.  */

boolean
fsuser_perms (pieuid, piegid)
     uid_t *pieuid;
     gid_t *piegid;
{
  uid_t ieuid, iuid;
  gid_t iegid, igid;

  ieuid = geteuid ();
  iuid = getuid ();
  if (pieuid != NULL)
    *pieuid = ieuid;

  iegid = getegid ();
  igid = getgid ();
  if (piegid != NULL)
    *piegid = iegid;

#if HAVE_SETREUID
  /* Swap the effective user id and the real user id.  We can then
     swap them back again when we want to return to the uucp user's
     permissions.  */
  if (setregid (iegid, igid) < 0)
    {
      ulog (LOG_ERROR, "setregid (%ld, %ld): %s",
	    (long) iegid, (long) igid, strerror (errno));
      return FALSE;
    }
  if (setreuid (ieuid, iuid) < 0)
    {
      ulog (LOG_ERROR, "setreuid (%ld, %ld): %s",
	    (long) ieuid, (long) iuid, strerror (errno));
      return FALSE;
    }
#else /* ! HAVE_SETREUID */
#if HAVE_SAVED_SETUID
  /* Set the effective user id to the real user id.  Since the
     effective user id is saved (it's the saved setuid) we will able
     to set back to it later.  If the real user id is root we will not
     be able to switch back and forth, so don't even try.  */
  if (iuid != 0)
    {
      if (setgid (igid) < 0)
	{
	  ulog (LOG_ERROR, "setgid (%ld): %s", (long) igid, strerror (errno));
	  return FALSE;
	}
      if (setuid (iuid) < 0)
	{
	  ulog (LOG_ERROR, "setuid (%ld): %s", (long) iuid, strerror (errno));
	  return FALSE;
	}
    }
#else /* ! HAVE_SAVED_SETUID */
  /* There's no way to switch between real permissions and effective
     permissions.  Just try to open the file with the uucp
     permissions.  */
#endif /* ! HAVE_SAVED_SETUID */
#endif /* ! HAVE_SETREUID */

  return TRUE;
}

/* Restore the uucp permissions.  */

/*ARGSUSED*/
boolean
fsuucp_perms (ieuid, iegid)
     long ieuid ATTRIBUTE_UNUSED;
     long iegid ATTRIBUTE_UNUSED;
{
#if HAVE_SETREUID
  /* Swap effective and real user id's back to what they were.  */
  if (! fsuser_perms ((uid_t *) NULL, (gid_t *) NULL))
    return FALSE;
#else /* ! HAVE_SETREUID */
#if HAVE_SAVED_SETUID
  /* Set ourselves back to our original effective user id.  */
  if (setgid ((gid_t) iegid) < 0)
    {
      ulog (LOG_ERROR, "setgid (%ld): %s", (long) iegid, strerror (errno));
      /* Is this error message helpful or confusing?  */
      if (errno == EPERM)
	ulog (LOG_ERROR,
	      "Probably HAVE_SAVED_SETUID in policy.h should be set to 0");
      return FALSE;
    }
  if (setuid ((uid_t) ieuid) < 0)
    {
      ulog (LOG_ERROR, "setuid (%ld): %s", (long) ieuid, strerror (errno));
      /* Is this error message helpful or confusing?  */
      if (errno == EPERM)
	ulog (LOG_ERROR,
	      "Probably HAVE_SAVED_SETUID in policy.h should be set to 0");
      return FALSE;
    }
#else /* ! HAVE_SAVED_SETUID */
  /* We didn't switch, no need to switch back.  */
#endif /* ! HAVE_SAVED_SETUID */
#endif /* ! HAVE_SETREUID */

  return TRUE;
}
