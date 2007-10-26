/*  pwd.h - Try to approximate UN*X's getuser...() functions under MS-DOS.
    Copyright (C) 1990 by Thorsten Ohl, td12@ddagsi3.bitnet

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 1, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.  */

/* This 'implementation' is conjectured from the use of this functions in
   the RCS and BASH distributions.  Of course these functions don't do too
   much useful things under MS-DOS, but using them avoids many "#ifdef
   MSDOS" in ported UN*X code ...  */

#include <sys/types.h>

struct passwd
{
  /*	...		*/
  /*    missing stuff	*/
  /*	...		*/
  char *pw_name;		/* login user id		*/
  char *pw_dir;			/* home directory		*/
  char *pw_shell;		/* login shell			*/
  int  pw_uid;
};

struct group
{
  /*	...		*/
  /*    missing stuff	*/
  /*	...		*/
  char *gr_name;		/* login user id		*/
  int  gr_gid;
};

struct passwd *getpwuid (uid_t);
struct passwd *getpwnam (const char *);
struct group *getgrgid (gid_t);
struct group *getgrnam (const char *);
char *getlogin (void);
char *getgr_name (void);
uid_t getuid (void);
gid_t getgid (void);
uid_t geteuid (void);
gid_t getegid (void);

extern int *groups;
extern int ngroups;
int getgroups (int, gid_t *);

struct passwd *getpwent (void);
void setpwent (void);
void endpwent (void);
void endgrent (void);

/*
 * Local Variables:
 * mode:C
 * ChangeLog:ChangeLog
 * compile-command:make
 * End:
 */
