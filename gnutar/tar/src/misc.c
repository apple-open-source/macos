/* Miscellaneous functions, not really specific to GNU tar.
   Copyright (C) 1988, 92, 94, 95, 96, 97, 1999 Free Software Foundation, Inc.

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
#include "rmt.h"
#include "common.h"

/* Handling strings.  */

/*-------------------------------------------------------------------------.
| Assign STRING to a copy of VALUE if not NULL, or to NULL.  If STRING was |
| not NULL, it is freed first.						   |
`-------------------------------------------------------------------------*/

void
assign_string (char **string, const char *value)
{
  if (*string)
    free (*string);
  *string = value ? xstrdup (value) : NULL;
}

/*------------------------------------------------------------------------.
| Allocate a copy of the string quoted as in C, and returns that.  If the |
| string does not have to be quoted, it returns the NULL string.  The	  |
| allocated copy should normally be freed with free() after the caller is |
| done with it.								  |
| 									  |
| This is used in two contexts only: either listing a tar file for the	  |
| --list (-t) option, or generating the directory file in incremental	  |
| dumps.								  |
`------------------------------------------------------------------------*/

char *
quote_copy_string (const char *string)
{
  const char *source = string;
  char *destination = NULL;
  char *buffer = NULL;
  int copying = 0;

  while (*source)
    {
      int character = (unsigned char) *source++;

      if (character == '\\')
	{
	  if (!copying)
	    {
	      size_t length = (source - string) - 1;

	      copying = 1;
	      buffer = (char *) xmalloc (length + 5 + strlen (source) * 4);
	      memcpy (buffer, string, length);
	      destination = buffer + length;
	    }
	  *destination++ = '\\';
	  *destination++ = '\\';
	}
      else if (ISPRINT (character))
	{
	  if (copying)
	    *destination++ = character;
	}
      else
	{
	  if (!copying)
	    {
	      size_t length = (source - string) - 1;

	      copying = 1;
	      buffer = (char *) xmalloc (length + 5 + strlen (source) * 4);
	      memcpy (buffer, string, length);
	      destination = buffer + length;
	    }
	  *destination++ = '\\';
	  switch (character)
	    {
	    case '\n':
	      *destination++ = 'n';
	      break;

	    case '\t':
	      *destination++ = 't';
	      break;

	    case '\f':
	      *destination++ = 'f';
	      break;

	    case '\b':
	      *destination++ = 'b';
	      break;

	    case '\r':
	      *destination++ = 'r';
	      break;

	    case '\177':
	      *destination++ = '?';
	      break;

	    default:
	      *destination++ = (character >> 6) + '0';
	      *destination++ = ((character >> 3) & 07) + '0';
	      *destination++ = (character & 07) + '0';
	      break;
	    }
	}
    }
  if (copying)
    {
      *destination = '\0';
      return buffer;
    }
  return NULL;
}

/*-------------------------------------------------------------------------.
| Takes a quoted C string (like those produced by quote_copy_string) and   |
| turns it back into the un-quoted original.  This is done in place.	   |
| Returns 0 only if the string was not properly quoted, but completes the  |
| unquoting anyway.							   |
| 									   |
| This is used for reading the saved directory file in incremental dumps.  |
| It is used for decoding old `N' records (demangling names).  But also,   |
| it is used for decoding file arguments, would they come from the shell   |
| or a -T file, and for decoding the --exclude argument.		   |
`-------------------------------------------------------------------------*/

int
unquote_string (char *string)
{
  int result = 1;
  char *source = string;
  char *destination = string;

  while (*source)
    if (*source == '\\')
      switch (*++source)
	{
	case '\\':
	  *destination++ = '\\';
	  source++;
	  break;

	case 'n':
	  *destination++ = '\n';
	  source++;
	  break;

	case 't':
	  *destination++ = '\t';
	  source++;
	  break;

	case 'f':
	  *destination++ = '\f';
	  source++;
	  break;

	case 'b':
	  *destination++ = '\b';
	  source++;
	  break;

	case 'r':
	  *destination++ = '\r';
	  source++;
	  break;

	case '?':
	  *destination++ = 0177;
	  source++;
	  break;

	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	  {
	    int value = *source++ - '0';

	    if (*source < '0' || *source > '7')
	      {
		*destination++ = value;
		break;
	      }
	    value = value * 8 + *source++ - '0';
	    if (*source < '0' || *source > '7')
	      {
		*destination++ = value;
		break;
	      }
	    value = value * 8 + *source++ - '0';
	    *destination++ = value;
	    break;
	  }

	default:
	  result = 0;
	  *destination++ = '\\';
	  if (*source)
	    *destination++ = *source++;
	  break;
	}
    else if (source != destination)
      *destination++ = *source++;
    else
      source++, destination++;

  if (source != destination)
    *destination = '\0';
  return result;
}

/* Sorting lists.  */

/*---.
| ?  |
`---*/

char *
merge_sort (char *list, int length, int offset, int (*compare) (char *, char *))
{
  char *first_list;
  char *second_list;
  int first_length;
  int second_length;
  char *result;
  char **merge_point;
  char *cursor;
  int counter;

#define SUCCESSOR(Pointer) \
  (*((char **) (((char *) (Pointer)) + offset)))

  if (length == 1)
    return list;

  if (length == 2)
    {
      if ((*compare) (list, SUCCESSOR (list)) > 0)
	{
	  result = SUCCESSOR (list);
	  SUCCESSOR (result) = list;
	  SUCCESSOR (list) = NULL;
	  return result;
	}
      return list;
    }

  first_list = list;
  first_length = (length + 1) / 2;
  second_length = length / 2;
  for (cursor = list, counter = first_length - 1;
       counter;
       cursor = SUCCESSOR (cursor), counter--)
    continue;
  second_list = SUCCESSOR (cursor);
  SUCCESSOR (cursor) = NULL;

  first_list = merge_sort (first_list, first_length, offset, compare);
  second_list = merge_sort (second_list, second_length, offset, compare);

  merge_point = &result;
  while (first_list && second_list)
    if ((*compare) (first_list, second_list) < 0)
      {
	cursor = SUCCESSOR (first_list);
	*merge_point = first_list;
	merge_point = &SUCCESSOR (first_list);
	first_list = cursor;
      }
    else
      {
	cursor = SUCCESSOR (second_list);
	*merge_point = second_list;
	merge_point = &SUCCESSOR (second_list);
	second_list = cursor;
      }
  if (first_list)
    *merge_point = first_list;
  else
    *merge_point = second_list;

  return result;

#undef SUCCESSOR
}

/* File handling.  */

/* Saved names in case backup needs to be undone.  */
static char *before_backup_name = NULL;
static char *after_backup_name = NULL;

/*------------------------------------------------------------------------.
| Returns nonzero if p is `.' or `..'.  This could be a macro for speed.  |
`------------------------------------------------------------------------*/

/* Early Solaris 2.4 readdir may return d->d_name as `' in NFS-mounted
   directories.  The workaround here skips `' just like `.'.  Without it,
   GNU tar would then treat `' much like `.' and loop endlessly.  */

int
is_dot_or_dotdot (const char *p)
{
  return (p[0] == '\0'
	  || (p[0] == '.' && (p[1] == '\0'
			      || (p[1] == '.' && p[2] == '\0'))));
}

/*-------------------------------------------------------------------------.
| Delete PATH, whatever it might be.  If RECURSE, first recursively delete |
| the contents of PATH when it is a directory.  Return zero on any error,  |
| with errno set.  As a special case, if we fail to delete a directory	   |
| when not RECURSE, do not set errno (just be tolerant to this error).	   |
`-------------------------------------------------------------------------*/

int
remove_any_file (const char *path, int recurse)
{
  struct stat stat_buffer;

  if (lstat (path, &stat_buffer) < 0)
    return 0;

  if (S_ISDIR (stat_buffer.st_mode))
    {
      if (recurse)
	{
	  DIR *dirp = opendir (path);
	  struct dirent *dp;

	  if (dirp == NULL)
	    return 0;

	  while (dp = readdir (dirp), dp)
	    if (! is_dot_or_dotdot (dp->d_name))
	      {
		char *path_buffer = new_name (path, dp->d_name);

		if (!remove_any_file (path_buffer, 1))
		  {
		    int saved_errno = errno;

		    free (path_buffer);
		    closedir (dirp);
		    /* FIXME: errno should be read-only.  */
		    errno = saved_errno;
		    return 0;
		  }
		free (path_buffer);
	      }
	  closedir (dirp);
	  return rmdir (path) >= 0;
	}
      else
	{
	  /* FIXME: Saving errno might not be needed anymore, now that
	     extract_archive tests for the special case before recovery.  */
	  int saved_errno = errno;

	  if (rmdir (path) >= 0)
	    return 1;
	  errno = saved_errno;	/* FIXME: errno should be read-only */
	  return 0;
	}
    }

  return unlink (path) >= 0;
}

/*-------------------------------------------------------------------------.
| Check if PATH already exists and make a backup of it right now.  Return  |
| success (nonzero) only if the backup in either unneeded, or successful.  |
| 									   |
| For now, directories are considered to never need backup.  If ARCHIVE is |
| nonzero, this is the archive and so, we do not have to backup block or   |
| character devices, nor remote entities.				   |
`-------------------------------------------------------------------------*/

int
maybe_backup_file (const char *path, int archive)
{
  struct stat file_stat;

  /* Check if we really need to backup the file.  */

  if (archive && _remdev (path))
    return 1;

  if (stat (path, &file_stat))
    {
      if (errno == ENOENT)
	return 1;

      ERROR ((0, errno, "%s", path));
      return 0;
    }

  if (S_ISDIR (file_stat.st_mode))
    return 1;

#ifdef S_ISBLK
  if (archive && S_ISBLK (file_stat.st_mode))
    return 1;
#endif

#ifdef S_ISCHR
  if (archive && S_ISCHR (file_stat.st_mode))
    return 1;
#endif

  assign_string (&before_backup_name, path);

  /* A run situation may exist between Emacs or other GNU programs trying to
     make a backup for the same file simultaneously.  If theoretically
     possible, real problems are unlikely.  Doing any better would require a
     convention, GNU-wide, for all programs doing backups.  */

  assign_string (&after_backup_name, NULL);
  after_backup_name = find_backup_file_name (path, backup_type);
  if (after_backup_name == NULL)
    FATAL_ERROR ((0, 0, "Virtual memory exhausted"));

  if (rename (before_backup_name, after_backup_name) == 0)
    {
      if (verbose_option)
	fprintf (stdlis, _("Renaming previous `%s' to `%s'\n"),
		 before_backup_name, after_backup_name);
      return 1;
    }

  /* The backup operation failed.  */

  ERROR ((0, errno, _("%s: Cannot rename for backup"), before_backup_name));
  assign_string (&after_backup_name, NULL);
  return 0;
}

/*-----------------------------------------------------------------------.
| Try to restore the recently backed up file to its original name.  This |
| is usually only needed after a failed extraction.			 |
`-----------------------------------------------------------------------*/

void
undo_last_backup (void)
{
  if (after_backup_name)
    {
      if (rename (after_backup_name, before_backup_name) != 0)
	ERROR ((0, errno, _("%s: Cannot rename from backup"),
		before_backup_name));
      if (verbose_option)
	fprintf (stdlis, _("Renaming `%s' back to `%s'\n"),
		 after_backup_name, before_backup_name);
      assign_string (&after_backup_name, NULL);
    }
}
