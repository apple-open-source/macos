/* Various processing of names.
   Copyright (C) 1988, 92, 94, 96, 97, 98, 1999 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
   Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "system.h"

#include <pwd.h>
#include <grp.h>

#ifndef FNM_LEADING_DIR
# include <fnmatch.h>
#endif

#include "common.h"

/* User and group names.  */

extern struct group *getgrnam ();
extern struct passwd *getpwnam ();
#if !HAVE_GETPWUID
extern struct passwd *getpwuid ();
#endif
#if !HAVE_GETGRGID
extern struct group *getgrgid ();
#endif

/* Make sure you link with the proper libraries if you are running the
   Yellow Peril (thanks for the good laugh, Ian J.!), or, euh... NIS.
   This code should also be modified for non-UNIX systems to do something
   reasonable.  */

static char cached_uname[UNAME_FIELD_SIZE] = "";
static char cached_gname[GNAME_FIELD_SIZE] = "";

static uid_t cached_uid;	/* valid only if cached_uname is not empty */
static gid_t cached_gid;	/* valid only if cached_gname is not empty */

/* These variables are valid only if nonempty.  */
static char cached_no_such_uname[UNAME_FIELD_SIZE] = "";
static char cached_no_such_gname[GNAME_FIELD_SIZE] = "";

/* These variables are valid only if nonzero.  It's not worth optimizing
   the case for weird systems where 0 is not a valid uid or gid.  */
static uid_t cached_no_such_uid = 0;
static gid_t cached_no_such_gid = 0;

/*------------------------------------------.
| Given UID, find the corresponding UNAME.  |
`------------------------------------------*/

void
uid_to_uname (uid_t uid, char uname[UNAME_FIELD_SIZE])
{
  struct passwd *passwd;

  if (uid != 0 && uid == cached_no_such_uid)
    {
      *uname = '\0';
      return;
    }

  if (!cached_uname[0] || uid != cached_uid)
    {
      passwd = getpwuid (uid);
      if (passwd)
	{
	  cached_uid = uid;
	  strncpy (cached_uname, passwd->pw_name, UNAME_FIELD_SIZE);
	}
      else
	{
	  cached_no_such_uid = uid;
	  *uname = '\0';
	  return;
	}
    }
  strncpy (uname, cached_uname, UNAME_FIELD_SIZE);
}

/*------------------------------------------.
| Given GID, find the corresponding GNAME.  |
`------------------------------------------*/

void
gid_to_gname (gid_t gid, char gname[GNAME_FIELD_SIZE])
{
  struct group *group;

  if (gid != 0 && gid == cached_no_such_gid)
    {
      *gname = '\0';
      return;
    }

  if (!cached_gname[0] || gid != cached_gid)
    {
      setgrent ();		/* FIXME: why?! */
      group = getgrgid (gid);
      if (group)
	{
	  cached_gid = gid;
	  strncpy (cached_gname, group->gr_name, GNAME_FIELD_SIZE);
	}
      else
	{
	  cached_no_such_gid = gid;
	  *gname = '\0';
	  return;
	}
    }
  strncpy (gname, cached_gname, GNAME_FIELD_SIZE);
}

/*-------------------------------------------------------------------------.
| Given UNAME, set the corresponding UID and return 1, or else, return 0.  |
`-------------------------------------------------------------------------*/

int
uname_to_uid (char uname[UNAME_FIELD_SIZE], uid_t *uidp)
{
  struct passwd *passwd;

  if (cached_no_such_uname[0]
      && strncmp (uname, cached_no_such_uname, UNAME_FIELD_SIZE) == 0)
    return 0;

  if (!cached_uname[0]
      || uname[0] != cached_uname[0]
      || strncmp (uname, cached_uname, UNAME_FIELD_SIZE) != 0)
    {
      passwd = getpwnam (uname);
      if (passwd)
	{
	  cached_uid = passwd->pw_uid;
	  strncpy (cached_uname, uname, UNAME_FIELD_SIZE);
	}
      else
	{
	  strncpy (cached_no_such_uname, uname, UNAME_FIELD_SIZE);
	  return 0;
	}
    }
  *uidp = cached_uid;
  return 1;
}

/*-------------------------------------------------------------------------.
| Given GNAME, set the corresponding GID and return 1, or else, return 0.  |
`-------------------------------------------------------------------------*/

int
gname_to_gid (char gname[GNAME_FIELD_SIZE], gid_t *gidp)
{
  struct group *group;

  if (cached_no_such_gname[0]
      && strncmp (gname, cached_no_such_gname, GNAME_FIELD_SIZE) == 0)
    return 0;

  if (!cached_gname[0]
      || gname[0] != cached_gname[0]
      || strncmp (gname, cached_gname, GNAME_FIELD_SIZE) != 0)
    {
      group = getgrnam (gname);
      if (group)
	{
	  cached_gid = group->gr_gid;
	  strncpy (cached_gname, gname, GNAME_FIELD_SIZE);
	}
      else
	{
	  strncpy (cached_no_such_gname, gname, GNAME_FIELD_SIZE);
	  return 0;
	}
    }
  *gidp = cached_gid;
  return 1;
}

/* Names from the command call.  */

static const char **name_array;	/* store an array of names */
static int allocated_names;	/* how big is the array? */
static int names;		/* how many entries does it have? */
static int name_index = 0;	/* how many of the entries have we scanned? */

/*------------------------.
| Initialize structures.  |
`------------------------*/

void
init_names (void)
{
  allocated_names = 10;
  name_array = (const char **)
    xmalloc (sizeof (const char *) * allocated_names);
  names = 0;
}

/*--------------------------------------------------------------.
| Add NAME at end of name_array, reallocating it as necessary.  |
`--------------------------------------------------------------*/

void
name_add (const char *name)
{
  if (names == allocated_names)
    {
      allocated_names *= 2;
      name_array = (const char **)
	xrealloc (name_array, sizeof (const char *) * allocated_names);
    }
  name_array[names++] = name;
}

/* Names from external name file.  */

static FILE *name_file;		/* file to read names from */
static char *name_buffer;	/* buffer to hold the current file name */
static size_t name_buffer_length; /* allocated length of name_buffer */

/*---.
| ?  |
`---*/

/* FIXME: I should better check more closely.  It seems at first glance that
   is_pattern is only used when reading a file, and ignored for all
   command line arguments.  */

static inline int
is_pattern (const char *string)
{
  return strchr (string, '*') || strchr (string, '[') || strchr (string, '?');
}

/*-----------------------------------------------------------------------.
| Set up to gather file names for tar.  They can either come from a file |
| or were saved from decoding arguments.				 |
`-----------------------------------------------------------------------*/

void
name_init (int argc, char *const *argv)
{
  name_buffer = xmalloc (NAME_FIELD_SIZE + 2);
  name_buffer_length = NAME_FIELD_SIZE;

  if (files_from_option)
    {
      if (!strcmp (files_from_option, "-"))
	{
	  request_stdin ("-T");
	  name_file = stdin;
	}
      else if (name_file = fopen (files_from_option, "r"), !name_file)
	FATAL_ERROR ((0, errno, _("Cannot open file %s"), files_from_option));
    }
}

/*---.
| ?  |
`---*/

void
name_term (void)
{
  free (name_buffer);
  free (name_array);
}

/*---------------------------------------------------------------------.
| Read the next filename from name_file and null-terminate it.  Put it |
| into name_buffer, reallocating and adjusting name_buffer_length if   |
| necessary.  Return 0 at end of file, 1 otherwise.		       |
`---------------------------------------------------------------------*/

static int
read_name_from_file (void)
{
  int character;
  size_t counter = 0;

  /* FIXME: getc may be called even if character was EOF the last time here.  */

  /* FIXME: This + 2 allocation might serve no purpose.  */

  while (character = getc (name_file),
	 character != EOF && character != filename_terminator)
    {
      if (counter == name_buffer_length)
	{
	  name_buffer_length += NAME_FIELD_SIZE;
	  name_buffer = xrealloc (name_buffer, name_buffer_length + 2);
	}
      name_buffer[counter++] = character;
    }

  if (counter == 0 && character == EOF)
    return 0;

  if (counter == name_buffer_length)
    {
      name_buffer_length += NAME_FIELD_SIZE;
      name_buffer = xrealloc (name_buffer, name_buffer_length + 2);
    }
  name_buffer[counter] = '\0';

  return 1;
}

/*------------------------------------------------------------------------.
| Get the next name from ARGV or the file of names.  Result is in static  |
| storage and can't be relied upon across two calls.			  |
| 									  |
| If CHANGE_DIRS is true, treat a filename of the form "-C" as meaning	  |
| that the next filename is the name of a directory to change to.  If	  |
| `filename_terminator' is NUL, CHANGE_DIRS is effectively always false.  |
`------------------------------------------------------------------------*/

char *
name_next (int change_dirs)
{
  const char *source;
  char *cursor;
  int chdir_flag = 0;

  if (filename_terminator == '\0')
    change_dirs = 0;

  while (1)
    {
      /* Get a name, either from file or from saved arguments.  */

      if (name_file)
	{
	  if (!read_name_from_file ())
	    break;
	}
      else
	{
	  if (name_index == names)
	    break;

	  source = name_array[name_index++];
	  if (strlen (source) > name_buffer_length)
	    {
	      free (name_buffer);
	      name_buffer_length = strlen (source);
	      name_buffer = xmalloc (name_buffer_length + 2);
	    }
	  strcpy (name_buffer, source);
	}

      /* Zap trailing slashes.  */

      cursor = name_buffer + strlen (name_buffer) - 1;
      while (cursor > name_buffer && *cursor == '/')
	*cursor-- = '\0';

      if (chdir_flag)
	{
	  if (chdir (name_buffer) < 0)
	    FATAL_ERROR ((0, errno, _("Cannot change to directory %s"),
			  name_buffer));
	  chdir_flag = 0;
	}
      else if (change_dirs && strcmp (name_buffer, "-C") == 0)
	chdir_flag = 1;
      else
	{
	  unquote_string (name_buffer);
	  return name_buffer;
	}
    }

  /* No more names in file.  */

  if (name_file && chdir_flag)
    FATAL_ERROR ((0, 0, _("Missing file name after -C")));

  return NULL;
}

/*------------------------------.
| Close the name file, if any.  |
`------------------------------*/

void
name_close (void)
{
  if (name_file != NULL && name_file != stdin)
    if (fclose (name_file) == EOF)
      ERROR ((0, errno, "%s", name_buffer));
}

/*-------------------------------------------------------------------------.
| Gather names in a list for scanning.  Could hash them later if we really |
| care.									   |
| 									   |
| If the names are already sorted to match the archive, we just read them  |
| one by one.  name_gather reads the first one, and it is called by	   |
| name_match as appropriate to read the next ones.  At EOF, the last name  |
| read is just left in the buffer.  This option lets users of small	   |
| machines extract an arbitrary number of files by doing "tar t" and	   |
| editing down the list of files.					   |
`-------------------------------------------------------------------------*/

void
name_gather (void)
{
  /* Buffer able to hold a single name.  */
  static struct name *buffer;
  static size_t allocated_length = 0;

  char *name;

  if (same_order_option)
    {
      if (allocated_length == 0)
	{
	  allocated_length = sizeof (struct name) + NAME_FIELD_SIZE;
	  buffer = (struct name *) xmalloc (allocated_length);
	  /* FIXME: This memset is overkill, and ugly...  */
	  memset (buffer, 0, allocated_length);
	}
      name = name_next (0);
      if (name)
	{
	  if (strcmp (name, "-C") == 0)
	    {
	      char *copy = xstrdup (name_next (0));

	      name = name_next (0);
	      if (!name)
		FATAL_ERROR ((0, 0, _("Missing file name after -C")));
	      buffer->change_dir = copy;
	    }
	  buffer->length = strlen (name);
	  if (sizeof (struct name) + buffer->length >= allocated_length)
	    {
	      allocated_length = sizeof (struct name) + buffer->length;
	      buffer = (struct name *) xrealloc (buffer, allocated_length);
	    }
	  strncpy (buffer->name, name, (size_t) buffer->length);
	  buffer->name[buffer->length] = 0;
	  buffer->next = NULL;
	  buffer->found = 0;

	  /* FIXME: Poorly named globals, indeed...  */
	  namelist = buffer;
	  namelast = namelist;
	}
      return;
    }

  /* Non sorted names -- read them all in.  */

  while (name = name_next (0), name)
    addname (name);
}

/*-----------------------------.
| Add a name to the namelist.  |
`-----------------------------*/

void
addname (const char *string)
{
  /* FIXME: This is ugly.  How is memory managed?  */
  static char *chdir_name = NULL;

  struct name *name;
  size_t length;

  if (strcmp (string, "-C") == 0)
    {
      chdir_name = xstrdup (name_next (0));
      string = name_next (0);
      if (!chdir_name)
	FATAL_ERROR ((0, 0, _("Missing file name after -C")));

      if (chdir_name[0] != '/')
	{
	  char *path = xmalloc (PATH_MAX);

	  /* FIXME: Shouldn't we use xgetcwd?  */
#if HAVE_GETCWD
	  if (!getcwd (path, PATH_MAX))
	    FATAL_ERROR ((0, 0, _("Could not get current directory")));
#else
	  char *getwd ();

	  if (!getwd (path))
	    FATAL_ERROR ((0, 0, _("Could not get current directory: %s"),
			  path));
#endif
	  chdir_name = new_name (path, chdir_name);
	  free (path);
	}
    }

  length = string ? strlen (string) : 0;
  name = (struct name *) xmalloc (sizeof (struct name) + length);
  memset (name, 0, sizeof (struct name) + length);
  name->next = NULL;

  if (string)
    {
      name->fake = 0;
      name->length = length;
      /* FIXME: Possibly truncating a string, here?  Tss, tss, tss!  */
      strncpy (name->name, string, length);
      name->name[length] = '\0';
    }
  else
    name->fake = 1;

  name->found = 0;
  name->regexp = 0;		/* assume not a regular expression */
  name->firstch = 1;		/* assume first char is literal */
  name->change_dir = chdir_name;
  name->dir_contents = 0;

  if (string && is_pattern (string))
    {
      name->regexp = 1;
      if (string[0] == '*' || string[0] == '[' || string[0] == '?')
	name->firstch = 0;
    }

  if (namelast)
    namelast->next = name;
  namelast = name;
  if (!namelist)
    namelist = name;
}

/*------------------------------------------------------------------------.
| Return true if and only if name PATH (from an archive) matches any name |
| from the namelist.							  |
`------------------------------------------------------------------------*/

int
name_match (const char *path)
{
  size_t length = strlen (path);

  while (1)
    {
      struct name *cursor = namelist;

      if (!cursor)
	return 1;		/* empty namelist is easy */

      if (cursor->fake)
	{
	  if (cursor->change_dir && chdir (cursor->change_dir))
	    FATAL_ERROR ((0, errno, _("Cannot change to directory %s"),
			  cursor->change_dir));
	  namelist = 0;
	  return 1;
	}

      for (; cursor; cursor = cursor->next)
	{
	  /* If first chars don't match, quick skip.  */

	  if (cursor->firstch && cursor->name[0] != path[0])
	    continue;

	  /* Regular expressions (shell globbing, actually).  */

	  if (cursor->regexp)
	    {
	      if (fnmatch (cursor->name, path, FNM_LEADING_DIR) == 0)
		{
		  cursor->found = 1; /* remember it matched */
		  if (starting_file_option)
		    {
		      free (namelist);
		      namelist = NULL;
		    }
		  if (cursor->change_dir && chdir (cursor->change_dir))
		    FATAL_ERROR ((0, errno, _("Cannot change to directory %s"),
				  cursor->change_dir));

		  /* We got a match.  */
		  return 1;
		}
	      continue;
	    }

	  /* Plain Old Strings.  */

	  if (cursor->length <= length
				/* archive length >= specified */
	      && (path[cursor->length] == '\0'
		  || path[cursor->length] == '/')
				/* full match on file/dirname */
	      && strncmp (path, cursor->name, cursor->length) == 0)
				/* name compare */
	    {
	      cursor->found = 1;	/* remember it matched */
	      if (starting_file_option)
		{
		  free ((void *) namelist);
		  namelist = 0;
		}
	      if (cursor->change_dir && chdir (cursor->change_dir))
		FATAL_ERROR ((0, errno, _("Cannot change to directory %s"),
			      cursor->change_dir));

	      /* We got a match.  */
	      return 1;
	    }
	}

      /* Filename from archive not found in namelist.  If we have the whole
	 namelist here, just return 0.  Otherwise, read the next name in and
	 compare it.  If this was the last name, namelist->found will remain
	 on.  If not, we loop to compare the newly read name.  */

      if (same_order_option && namelist->found)
	{
	  name_gather ();	/* read one more */
	  if (namelist->found)
	    return 0;
	}
      else
	return 0;
    }
}

/*------------------------------------------------------------------.
| Print the names of things in the namelist that were not matched.  |
`------------------------------------------------------------------*/

void
names_notfound (void)
{
  struct name *cursor;
  struct name *next;

  for (cursor = namelist; cursor; cursor = next)
    {
      next = cursor->next;
      if (!cursor->found && !cursor->fake)
	ERROR ((0, 0, _("%s: Not found in archive"), cursor->name));

      /* We could free the list, but the process is about to die anyway, so
	 save some CPU time.  Amigas and other similarly broken software
	 will need to waste the time, though.  */

#ifdef amiga
      if (!same_order_option)
	free (cursor);
#endif
    }
  namelist = (struct name *) NULL;
  namelast = (struct name *) NULL;

  if (same_order_option)
    {
      char *name;

      while (name = name_next (1), name)
	ERROR ((0, 0, _("%s: Not found in archive"), name));
    }
}

/*---.
| ?  |
`---*/

void
name_expand (void)
{
}

/*-------------------------------------------------------------------------.
| This is like name_match, except that it returns a pointer to the name it |
| matched, and doesn't set FOUND in structure.  The caller will have to do |
| that if it wants to.  Oh, and if the namelist is empty, it returns NULL, |
| unlike name_match, which returns TRUE.                                   |
`-------------------------------------------------------------------------*/

struct name *
name_scan (const char *path)
{
  size_t length = strlen (path);

  while (1)
    {
      struct name *cursor = namelist;

      if (!cursor)
	return NULL;		/* empty namelist is easy */

      for (; cursor; cursor = cursor->next)
	{
	  /* If first chars don't match, quick skip.  */

	  if (cursor->firstch && cursor->name[0] != path[0])
	    continue;

	  /* Regular expressions.  */

	  if (cursor->regexp)
	    {
	      if (fnmatch (cursor->name, path, FNM_LEADING_DIR) == 0)
		return cursor;	/* we got a match */
	      continue;
	    }

	  /* Plain Old Strings.  */

	  if (cursor->length <= length
				/* archive length >= specified */
	      && (path[cursor->length] == '\0'
		  || path[cursor->length] == '/')
				/* full match on file/dirname */
	      && strncmp (path, cursor->name, cursor->length) == 0)
				/* name compare */
	    return cursor;	/* we got a match */
	}

      /* Filename from archive not found in namelist.  If we have the whole
	 namelist here, just return 0.  Otherwise, read the next name in and
	 compare it.  If this was the last name, namelist->found will remain
	 on.  If not, we loop to compare the newly read name.  */

      if (same_order_option && namelist->found)
	{
	  name_gather ();	/* read one more */
	  if (namelist->found)
	    return NULL;
	}
      else
	return NULL;
    }
}

/*-----------------------------------------------------------------------.
| This returns a name from the namelist which doesn't have ->found set.	 |
| It sets ->found before returning, so successive calls will find and	 |
| return all the non-found names in the namelist			 |
`-----------------------------------------------------------------------*/

struct name *gnu_list_name = NULL;

char *
name_from_list (void)
{
  if (!gnu_list_name)
    gnu_list_name = namelist;
  while (gnu_list_name && gnu_list_name->found)
    gnu_list_name = gnu_list_name->next;
  if (gnu_list_name)
    {
      gnu_list_name->found = 1;
      if (gnu_list_name->change_dir)
	if (chdir (gnu_list_name->change_dir) < 0)
	  FATAL_ERROR ((0, errno, _("Cannot change to directory %s"),
			gnu_list_name->change_dir));
      return gnu_list_name->name;
    }
  return NULL;
}

/*---.
| ?  |
`---*/

void
blank_name_list (void)
{
  struct name *name;

  gnu_list_name = 0;
  for (name = namelist; name; name = name->next)
    name->found = 0;
}

/*---.
| ?  |
`---*/

char *
new_name (const char *path, const char *name)
{
  char *buffer = (char *) xmalloc (strlen (path) + strlen (name) + 2);

  sprintf (buffer, "%s/%s", path, name);
  return buffer;
}
