/* uudir.c
   Create a directory owned by uucp.  This is Unix specific.

   Copyright (C) 1992, 1993 Ian Lance Taylor

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
const char uudir_rcsid[] = "$Id: uudir.c,v 1.8 2002/03/05 19:10:42 ian Rel $";
#endif

#include "sysdep.h"

#include <pwd.h>

/* External functions.  */
#if GETPWNAM_DECLARATION_OK
#ifndef getpwnam
extern struct passwd *getpwnam ();
#endif
#endif

/* This is a simple program which sets its real uid to uucp and then
   invokes /bin/mkdir.  It is only used if the system does not support
   the mkdir system call.  It must be installed suid to root.

   This program is needed because the UUCP programs will be run suid
   to uucp.  On a system without the mkdir system call, /bin/mkdir is
   a suid root program.  This means that /bin/mkdir always creates
   directories using the real uid, rather than the effective uid.
   This is wrong, since the UUCP programs always want to create
   directories that are owned by uucp.  Therefore, this simple suid
   root program is used to force /bin/mkdir into making a directory
   owned by uucp.

   If we made the program publically executable, this would mean that
   anybody could create a directory owned by uucp.  This is probably
   not a good thing, but since the program must be owned by root we
   can't simply make it executable only by uucp.  Therefore, the
   Makefile hides the program away in /usr/lib/uucp/util, and makes
   that directory searchable only by uucp.  This should prevent
   anybody else from getting to the program.

   This is not a perfect solution, since any suid root program is by
   definition a potential security hole.  I really can't see any way
   to avoid this, though.  */

int
main (argc, argv)
     int argc;
     char **argv;
{
  struct passwd *q;
  const char *zprog, *zname;

  /* We don't print any error messages, since this program should
     never be run directly by a user.  */

  if (argc != 2)
    exit (EXIT_FAILURE);

  /* OWNER is passed in from the Makefile.  It will normally be
     "uucp".  */
  q = getpwnam (OWNER);
  if (q == NULL)
    exit (EXIT_FAILURE);

  if (setuid (q->pw_uid) < 0)
    exit (EXIT_FAILURE);

  zprog = MKDIR_PROGRAM;
  zname = strrchr (zprog, '/');
  if (zname == NULL)
    zname = zprog;
  else
    ++zname;

  (void) execl (zprog, zname, argv[1], (char *) NULL);
  exit (EXIT_FAILURE);
}
