/* Encode long filenames for GNU tar.
   Copyright (C) 1988, 1992, 1994, 1996, 1997 Free Software Foundation, Inc.

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

#include <time.h>
time_t time ();

#include "common.h"

struct mangled
  {
    struct mangled *next;
    int type;
    char mangled[NAME_FIELD_SIZE];
    char *linked_to;
    char normal[1];
  };

/* Should use a hash table, etc. .  */
struct mangled *first_mangle;
int mangled_num = 0;

/*---------------------------------------------------------------------.
| Extract a GNUTYPE_NAMES record contents.  It seems that such are not |
| produced anymore by GNU tar, but we leave the reading code around    |
| nevertheless, for salvaging old tapes.			       |
`---------------------------------------------------------------------*/

void
extract_mangle (void)
{
  off_t size = current_stat.st_size;
  char *buffer = xmalloc ((size_t) (size + 1));
  char *copy = buffer;
  char *cursor = buffer;

  if (size != (size_t) size || size == (size_t) -1)
    FATAL_ERROR ((0, 0, _("Memory exhausted")));

  buffer[size] = '\0';

  while (size > 0)
    {
      union block *block = find_next_block ();
      size_t available;

      if (!block)
	{
	  ERROR ((0, 0, _("Unexpected EOF in mangled names")));
	  return;
	}
      available = available_space_after (block);
      if (available > size)
	available = size;
      memcpy (copy, block->buffer, available);
      copy += available;
      size -= available;
      set_next_block_after ((union block *) (block->buffer + available - 1));
    }

  while (*cursor)
    {
      char *next_cursor;
      char *name;
      char *name_end;

      next_cursor = strchr (cursor, '\n');
      *next_cursor++ = '\0';

      if (!strncmp (cursor, "Rename ", 7))
	{

	  name = cursor + 7;
	  name_end = strchr (name, ' ');
	  while (strncmp (name_end, " to ", 4))
	    {
	      name_end++;
	      name_end = strchr (name_end, ' ');
	    }
	  *name_end = '\0';
	  if (next_cursor[-2] == '/')
	    next_cursor[-2] = '\0';
	  unquote_string (name_end + 4);
	  if (rename (name, name_end + 4))
	    ERROR ((0, errno, _("Cannot rename %s to %s"), name, name_end + 4));
	  else if (verbose_option)
	    WARN ((0, 0, _("Renamed %s to %s"), name, name_end + 4));
	}
#ifdef S_ISLNK
      else if (!strncmp (cursor, "Symlink ", 8))
	{
	  name = cursor + 8;
	  name_end = strchr (name, ' ');
	  while (strncmp (name_end, " to ", 4))
	    {
	      name_end++;
	      name_end = strchr (name_end, ' ');
	    }
	  *name_end = '\0';
	  unquote_string (name);
	  unquote_string (name_end + 4);
	  if (symlink (name, name_end + 4)
	      && (unlink (name_end + 4) || symlink (name, name_end + 4)))
	    ERROR ((0, errno, _("Cannot symlink %s to %s"),
		    name, name_end + 4));
	  else if (verbose_option)
	    WARN ((0, 0, _("Symlinked %s to %s"), name, name_end + 4));
	}
#endif
      else
	ERROR ((0, 0, _("Unknown demangling command %s"), cursor));

      cursor = next_cursor;
    }
}
