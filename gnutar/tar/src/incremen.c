/* GNU dump extensions to tar.
   Copyright (C) 1988, 92, 93, 94, 96, 97 Free Software Foundation, Inc.

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

/* Variable sized generic character buffers.  */

struct accumulator
{
  size_t allocated;
  size_t length;
  char *pointer;
};

/* Amount of space guaranteed just after a reallocation.  */
#define ACCUMULATOR_SLACK 50

/*---------------------------------------------------------.
| Return the accumulated data from an ACCUMULATOR buffer.  |
`---------------------------------------------------------*/

static char *
get_accumulator (struct accumulator *accumulator)
{
  return accumulator->pointer;
}

/*-----------------------------------------------.
| Allocate and return a new accumulator buffer.	 |
`-----------------------------------------------*/

static struct accumulator *
new_accumulator (void)
{
  struct accumulator *accumulator
    = (struct accumulator *) xmalloc (sizeof (struct accumulator));

  accumulator->allocated = ACCUMULATOR_SLACK;
  accumulator->pointer = (char *) xmalloc (ACCUMULATOR_SLACK);
  accumulator->length = 0;
  return accumulator;
}

/*-----------------------------------.
| Deallocate an ACCUMULATOR buffer.  |
`-----------------------------------*/

static void
delete_accumulator (struct accumulator *accumulator)
{
  free (accumulator->pointer);
  free (accumulator);
}

/*----------------------------------------------------------------------.
| At the end of an ACCUMULATOR buffer, add a DATA block of SIZE bytes.  |
`----------------------------------------------------------------------*/

static void
add_to_accumulator (struct accumulator *accumulator,
		    const char *data, size_t size)
{
  if (accumulator->length + size > accumulator->allocated)
    {
      accumulator->allocated = accumulator->length + size + ACCUMULATOR_SLACK;
      accumulator->pointer = (char *)
	xrealloc (accumulator->pointer, accumulator->allocated);
    }
  memcpy (accumulator->pointer + accumulator->length, data, size);
  accumulator->length += size;
}

/* Incremental dump specialities.  */

/* Current time.  */
static time_t time_now;

/* List of directory names.  */
struct directory
  {
    struct directory *next;	/* next entry in list */
    const char *name;		/* path name of directory */
    dev_t device_number;	/* device number for directory */
    ino_t inode_number;		/* inode number for directory */
    char allnew;
    char nfs;
    const char *dir_text;
  };
static struct directory *directory_list = NULL;

#if HAVE_ST_FSTYPE_STRING
  static const char nfs[] = "nfs";
# define NFS_FILE_STAT(st) (strcmp ((st).st_fstype, nfs) == 0)
#else
# define ST_DEV_MSB(st) (~ (dev_t) 0 << (sizeof (st).st_dev * CHAR_BIT - 1))
# define NFS_FILE_STAT(st) (((st).st_dev & ST_DEV_MSB (st)) != 0)
#endif

/*-------------------------------------------------------------------.
| Create and link a new directory entry for directory NAME, having a |
| DEVICE_NUMBER and a INODE_NUMBER, with some TEXT.		     |
`-------------------------------------------------------------------*/

static void
note_directory (char *name, struct stat *st, const char *text)
{
  struct directory *directory
    = (struct directory *) xmalloc (sizeof (struct directory));

  directory->next = directory_list;
  directory_list = directory;

  directory->device_number = st->st_dev;
  directory->inode_number = st->st_ino;
  directory->name = xstrdup (name);
  directory->dir_text = text;
  directory->allnew = 0;
  directory->nfs = NFS_FILE_STAT (*st);
}

/*------------------------------------------------------------------------.
| Return a directory entry for a given path NAME, or NULL if none found.  |
`------------------------------------------------------------------------*/

static struct directory *
find_directory (char *name)
{
  struct directory *directory;

  for (directory = directory_list;
       directory;
       directory = directory->next)
    {
      if (!strcmp (directory->name, name))
	return directory;
    }
  return NULL;
}

/*---.
| ?  |
`---*/

static int
compare_dirents (const voidstar first, const voidstar second)
{
  return strcmp ((*(char *const *) first) + 1,
		 (*(char *const *) second) + 1);
}

/*---.
| ?  |
`---*/

char *
get_directory_contents (char *path, dev_t device)
{
  struct accumulator *accumulator;

  /* Recursively scan the given PATH.  */

  {
    DIR *dirp = opendir (path);	/* for scanning directory */
    struct dirent *entry;	/* directory entry being scanned */
    char *name_buffer;		/* directory, `/', and directory member */
    size_t name_buffer_size;	/* allocated size of name_buffer, minus 2 */
    size_t name_length;		/* used length in name_buffer */
    struct directory *directory; /* for checking if already already seen */
    int all_children;

    if (dirp == NULL)
      {
	ERROR ((0, errno, _("Cannot open directory %s"), path));
	return NULL;
      }
    errno = 0;			/* FIXME: errno should be read-only */

    name_buffer_size = strlen (path) + NAME_FIELD_SIZE;
    name_buffer = xmalloc (name_buffer_size + 2);
    strcpy (name_buffer, path);
    if (path[strlen (path) - 1] != '/')
      strcat (name_buffer, "/");
    name_length = strlen (name_buffer);

    directory = find_directory (path);
    all_children = directory ? directory->allnew : 0;

    accumulator = new_accumulator ();

    while (entry = readdir (dirp), entry)
      {
	/* Skip `.' and `..'.  */

	if (is_dot_or_dotdot (entry->d_name))
	  continue;

	if (excluded_filename (excluded, entry->d_name))
	  add_to_accumulator (accumulator, "N", 1);
	else
	  {
	    struct stat stat_data;

	    if (NAMLEN (entry) + name_length >= name_buffer_size)
	      {
		while (NAMLEN (entry) + name_length >= name_buffer_size)
		  name_buffer_size += NAME_FIELD_SIZE;
		name_buffer = (char *)
		  xrealloc (name_buffer, name_buffer_size + 2);
	      }
	    strcpy (name_buffer + name_length, entry->d_name);

	    if (dereference_option
#if STX_HIDDEN && !_LARGE_FILES /* AIX */
		? statx (name_buffer, &stat_data, STATSIZE, STX_HIDDEN)
		: statx (name_buffer, &stat_data, STATSIZE, STX_HIDDEN | STX_LINK)
#else
		? stat (name_buffer, &stat_data)
		: lstat (name_buffer, &stat_data)
#endif
		)
	      {
		ERROR ((0, errno, _("Cannot stat %s"), name_buffer));
		continue;
	      }

	    if (one_file_system_option && device != stat_data.st_dev)
	      add_to_accumulator (accumulator, "N", 1);

#ifdef AIX
	    else if (S_ISHIDDEN (stat_data.st_mode))
	      {
		add_to_accumulator (accumulator, "D", 1);
		strcat (entry->d_name, "A");
		entry->d_namlen++;
	      }
#endif

	    else if (S_ISDIR (stat_data.st_mode))
	      {
		if (directory = find_directory (name_buffer), directory)
		  {
		    /* With NFS, the same file can have two different
		       devices if an NFS directory is mounted in
		       multiple locations, which is relatively common
		       when automounting.  For avoiding spurious
		       incremental redumping of directories, we have
		       to plainly consider all NFS devices as equal,
		       relying on the i-node only to establish
		       differences.  */

		    if (! (((directory->nfs && NFS_FILE_STAT (stat_data))
			    || directory->device_number == stat_data.st_dev)
			   && directory->inode_number == stat_data.st_ino))
		      {
			if (verbose_option)
			  WARN ((0, 0, _("Directory %s has been renamed"),
				 name_buffer));
			directory->allnew = 1;
			directory->nfs = NFS_FILE_STAT (stat_data);
			directory->device_number = stat_data.st_dev;
			directory->inode_number = stat_data.st_ino;
		      }
		    directory->dir_text = "";
		  }
		else
		  {
		    if (verbose_option)
		      WARN ((0, 0, _("Directory %s is new"), name_buffer));
		    note_directory (name_buffer, &stat_data, "");
		    directory = find_directory (name_buffer);
		    directory->allnew = 1;
		  }
		if (all_children && directory)
		  directory->allnew = 1;

		add_to_accumulator (accumulator, "D", 1);
	      }

	    else
	      if (!all_children
		  && stat_data.st_mtime < newer_mtime_option
		  && (!after_date_option
		      || stat_data.st_ctime < newer_ctime_option))
		add_to_accumulator (accumulator, "N", 1);
	      else
		add_to_accumulator (accumulator, "Y", 1);
	  }

	add_to_accumulator (accumulator,
			    entry->d_name, NAMLEN (entry) + 1);
      }
    add_to_accumulator (accumulator, "\000\000", 2);

    free (name_buffer);
    closedir (dirp);
  }

  /* Sort the contents of the directory, now that we have it all.  */

  {
    char *pointer = get_accumulator (accumulator);
    size_t counter;
    char *cursor;
    char *buffer;
    char **array;
    char **array_cursor;

    counter = 0;
    for (cursor = pointer; *cursor; cursor += strlen (cursor) + 1)
      counter++;

    if (counter == 0)
      {
	delete_accumulator (accumulator);
	return NULL;
      }

    array = (char **) xmalloc (sizeof (char *) * (counter + 1));

    array_cursor = array;
    for (cursor = pointer; *cursor; cursor += strlen (cursor) + 1)
      *array_cursor++ = cursor;
    *array_cursor = NULL;

    qsort ((voidstar) array, counter, sizeof (char *), compare_dirents);

    buffer = (char *) xmalloc ((size_t) (cursor - pointer + 2));

    cursor = buffer;
    for (array_cursor = array; *array_cursor; array_cursor++)
      {
	char *string = *array_cursor;

	while ((*cursor++ = *string++))
	  continue;
      }
    *cursor = '\0';

    delete_accumulator (accumulator);
    free (array);
    return buffer;
  }
}

/*----------------------------------------------------------------------.
| Add all the files in PATH, which is a directory, to the namelist.  If |
| any of the files is a directory, recurse on the subdirectory.	        |
`----------------------------------------------------------------------*/

static void
add_hierarchy_to_namelist (char *path, dev_t device)
{
  char *buffer = get_directory_contents (path, device);

  {
    struct name *name;

    for (name = namelist; name; name = name->next)
      if (strcmp (name->name, path) == 0)
	  break;
    if (name)
      name->dir_contents = buffer ? buffer : "\0\0\0\0";
  }

  if (buffer)
    {
      size_t name_length = strlen (path);
      size_t allocated_length = (name_length >= NAME_FIELD_SIZE
				 ? name_length + NAME_FIELD_SIZE
				 : NAME_FIELD_SIZE);
      char *name_buffer = xmalloc (allocated_length + 1);
				/* FIXME: + 2 above?  */
      char *string;
      size_t string_length;

      strcpy (name_buffer, path);
      if (name_buffer[name_length - 1] != '/')
	{
	  name_buffer[name_length++] = '/';
	  name_buffer[name_length] = '\0';
	}

      for (string = buffer; *string; string += string_length + 1)
	{
	  string_length = strlen (string);
	  if (*string == 'D')
	    {
	      if (name_length + string_length >= allocated_length)
		{
		  while (name_length + string_length >= allocated_length)
		    allocated_length += NAME_FIELD_SIZE;
		  name_buffer = (char *)
		    xrealloc (name_buffer, allocated_length + 1);
		}
	      strcpy (name_buffer + name_length, string + 1);
	      addname (name_buffer);
	      add_hierarchy_to_namelist (name_buffer, device);
	    }
	}

      free (name_buffer);
    }
}

/*---.
| ?  |
`---*/

static void
read_directory_file (void)
{
  char *strp;
  FILE *fp;
  char buf[512];
  static char *path = NULL;

  if (path == NULL)
    path = xmalloc (PATH_MAX);
  time (&time_now);
  if (listed_incremental_option[0] != '/')
    {
#if HAVE_GETCWD
      if (!getcwd (path, PATH_MAX))
	FATAL_ERROR ((0, 0, _("Could not get current directory")));
#else
      char *getwd ();

      if (!getwd (path))
	FATAL_ERROR ((0, 0, _("Could not get current directory: %s"), path));
#endif

      if (strlen (path) + 1 + strlen (listed_incremental_option) + 1 > PATH_MAX)
	ERROR ((TAREXIT_FAILURE, 0, _("File name %s/%s too long"),
		path, listed_incremental_option));

      strcat (path, "/");
      strcat (path, listed_incremental_option);
      listed_incremental_option = path;
    }
  fp = fopen (listed_incremental_option, "r");
  if (fp == 0 && errno != ENOENT)
    {
      ERROR ((0, errno, _("Cannot open %s"), listed_incremental_option));
      return;
    }
  if (!fp)
    return;
  fgets (buf, sizeof (buf), fp);

  /* FIXME: Using after_date_option as a first time flag looks fairly
     dubious to me!  So, using -N with incremental might be buggy just
     because of the next few lines.  I saw a few unexplained, almost harsh
     advices, from other GNU people, about *not* using -N with incremental
     dumps, and here might lie (part of) the reason.  */
  if (!after_date_option)
    {
      newer_mtime_option = atol (buf);
      after_date_option = 1;
    }

  while (fgets (buf, sizeof (buf), fp))
    {
      struct stat st;
      long l;

      strp = &buf[strlen (buf)];
      if (strp[-1] == '\n')
	strp[-1] = '\0';
      /* FIXME: For files ending with an incomplete line, maybe a NUL might
	 be missing, here...  */

      memset (&st, 0, sizeof st);
      strp = buf;
      st.st_dev = l = atol (strp);
      if (st.st_dev != l)
	ERROR ((0, 0, _("Device number out of range")));
      while (ISDIGIT (*strp))
	strp++;
      st.st_ino = l = atol (strp);
      if (st.st_ino != l)
	ERROR ((0, 0, _("Inode number out of range")));
      while (ISSPACE ((unsigned char) *strp))
	strp++;
      while (ISDIGIT (*strp))
	strp++;
      strp++;
      unquote_string (strp);
      note_directory (strp, &st, NULL);
    }
  if (fclose (fp) == EOF)
    ERROR ((0, errno, "%s", listed_incremental_option));
}

/*---.
| ?  |
`---*/

void
write_dir_file (void)
{
  FILE *fp;
  struct directory *directory;
  char *str;

  fp = fopen (listed_incremental_option, "w");
  if (fp == 0)
    {
      ERROR ((0, errno, _("Cannot write to %s"), listed_incremental_option));
      return;
    }
  fprintf (fp, "%lu\n", (unsigned long) time_now);
  for (directory = directory_list; directory; directory = directory->next)
    {
      if (!directory->dir_text)
	continue;
      str = quote_copy_string (directory->name);
      if (str)
	{
	  fprintf (fp, "%lu %lu %s\n",
		   (unsigned long) directory->device_number,
		   (unsigned long) directory->inode_number,
		   str);
	  free (str);
	}
      else
	fprintf (fp, "%lu %lu %s\n",
		 (unsigned long) directory->device_number,
		 (unsigned long) directory->inode_number,
		 directory->name);
    }
  if (fclose (fp) == EOF)
    ERROR ((0, errno, "%s", listed_incremental_option));
}

/*---.
| ?  |
`---*/

static int
compare_names (char *param1, char *param2)
{
  struct name *n1 = (struct name *) param1;
  struct name *n2 = (struct name *) param2;

  if (n1->found)
    return n2->found ? strcmp (n1->name, n2->name) : -1;

  if (n2->found)
    return 1;

  return strcmp (n1->name, n2->name);
}

/*-------------------------------------------------------------------------.
| Collect all the names from argv[] (or whatever), then expand them into a |
| directory tree, and put all the directories at the beginning.		   |
`-------------------------------------------------------------------------*/

void
collect_and_sort_names (void)
{
  struct name *name;
  struct name *next_name;
  int num_names;
  struct stat statbuf;

  name_gather ();

  if (listed_incremental_option)
    read_directory_file ();

  if (!namelist)
    addname (".");

  for (name = namelist; name; name = next_name)
    {
      next_name = name->next;
      if (name->found || name->dir_contents)
	continue;
      if (name->regexp)		/* FIXME: just skip regexps for now */
	continue;
      if (name->change_dir)
	if (chdir (name->change_dir) < 0)
	  {
	    ERROR ((0, errno, _("Cannot chdir to %s"), name->change_dir));
	    continue;
	  }

      if (
#if STX_HIDDEN && !_LARGE_FILES /* AIX */
	  statx (name->name, &statbuf, STATSIZE, STX_HIDDEN | STX_LINK)
#else
	  lstat (name->name, &statbuf) < 0
#endif
	  )
	{
	  ERROR ((0, errno, _("Cannot stat %s"), name->name));
	  continue;
	}
      if (S_ISDIR (statbuf.st_mode))
	{
	  name->found = 1;
	  add_hierarchy_to_namelist (name->name, statbuf.st_dev);
	}
    }

  num_names = 0;
  for (name = namelist; name; name = name->next)
    num_names++;
  namelist = (struct name *)
    merge_sort ((voidstar) namelist, num_names,
		(char *) (&(namelist->next)) - (char *) namelist,
		compare_names);

  for (name = namelist; name; name = name->next)
    name->found = 0;

  if (listed_incremental_option)
    write_dir_file ();
}

/* Restoration of incremental dumps.  */

/*---.
| ?  |
`---*/

void
gnu_restore (int skipcrud)
{
  char *current_dir;
  char *archive_dir;
  struct accumulator *accumulator;
  char *p;
  DIR *dirp;
  struct dirent *d;
  char *cur, *arc;
  off_t size;
  size_t copied;
  union block *data_block;
  char *to;

#define CURRENT_FILE_NAME (skipcrud + current_file_name)

  dirp = opendir (CURRENT_FILE_NAME);

  if (!dirp)
    {
      /* The directory doesn't exist now.  It'll be created.  In any
	 case, we don't have to delete any files out of it.  */

      skip_file (current_stat.st_size);
      return;
    }

  accumulator = new_accumulator ();
  while (d = readdir (dirp), d)
    {
      if (is_dot_or_dotdot (d->d_name))
	continue;

      add_to_accumulator (accumulator, d->d_name, NAMLEN (d) + 1);
    }
  closedir (dirp);
  add_to_accumulator (accumulator, "", 1);

  current_dir = get_accumulator (accumulator);
  size = current_stat.st_size;
  if (size != current_stat.st_size)
    FATAL_ERROR ((0, 0, _("Memory exhausted")));
  archive_dir = (char *) xmalloc (size);
  to = archive_dir;
  for (; size > 0; size -= copied)
    {
      data_block = find_next_block ();
      if (!data_block)
	{
	  ERROR ((0, 0, _("Unexpected EOF in archive")));
	  break;		/* FIXME: What happens then?  */
	}
      copied = available_space_after (data_block);
      if (copied > size)
	copied = size;
      memcpy (to, data_block->buffer, copied);
      to += copied;
      set_next_block_after ((union block *)
			    (data_block->buffer + copied - 1));
    }

  for (cur = current_dir; *cur; cur += strlen (cur) + 1)
    {
      for (arc = archive_dir; *arc; arc += strlen (arc) + 1)
	{
	  arc++;
	  if (!strcmp (arc, cur))
	    break;
	}
      if (*arc == '\0')
	{
	  p = new_name (CURRENT_FILE_NAME, cur);
	  if (interactive_option && !confirm ("delete", p))
	    {
	      free (p);
	      continue;
	    }
	  if (verbose_option)
	    fprintf (stdlis, _("%s: Deleting %s\n"), program_name, p);
	  if (!remove_any_file (p, 1))
	    ERROR ((0, errno, _("Error while deleting %s"), p));
	  free (p);
	}

    }
  delete_accumulator (accumulator);
  free (archive_dir);

#undef CURRENT_FILE_NAME
}
