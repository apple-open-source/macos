/* Create a tar archive.
   Copyright 1985, 92, 93, 94, 96, 97, 1999 Free Software Foundation, Inc.
   Written by John Gilmore, on 1985-08-25.

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

#if !MSDOS
# include <pwd.h>
# include <grp.h>
#endif

#if HAVE_UTIME_H
# include <utime.h>
#else
struct utimbuf
  {
    long actime;
    long modtime;
  };
#endif

#include "common.h"

#ifndef MSDOS
extern dev_t ar_dev;
extern ino_t ar_ino;
#endif

extern struct name *gnu_list_name;

/* This module is the only one that cares about `struct link's.  */

struct link
  {
    struct link *next;
    dev_t dev;
    ino_t ino;
    short linkcount;
    char name[1];
  };

struct link *linklist = NULL;	/* points to first link in list */


/*------------------------------------------------------------------------.
| Convert VALUE (with substitute SUBSTITUTE if VALUE is out of range)	  |
| into a size-SIZE field at WHERE, including a				  |
| trailing space.  For example, 3 for SIZE means two digits and a space.  |
|                                                                         |
| We assume the trailing NUL is already there and don't fill it in.  This |
| fact is used by start_header and finish_header, so don't change it!     |
`------------------------------------------------------------------------*/

/* Output VALUE in octal, using SUBSTITUTE if value won't fit.
   Output to buffer WHERE with size SIZE.
   TYPE is the kind of value being output (useful for diagnostics).
   Prefer SIZE - 1 octal digits (with leading '0's), followed by '\0';
   but if SIZE octal digits would fit, omit the '\0'.  */

static void
to_oct (uintmax_t value, uintmax_t substitute, char *where, size_t size, const char *type)
{
  uintmax_t v = value;
  size_t i = size;

# define MAX_OCTAL_VAL_WITH_DIGITS(digits) \
    ((digits) * 3 < sizeof (uintmax_t) * CHAR_BIT \
     ? ((uintmax_t) 1 << ((digits) * 3)) - 1 \
     : (uintmax_t) -1)

  /* Output a trailing NUL unless the value is too large.  */
  if (value <= MAX_OCTAL_VAL_WITH_DIGITS (size - 1))
    where[--i] = '\0';

  /* Produce the digits -- at least one.  */

  do
    {
      where[--i] = '0' + (int) (v & 7);	/* one octal digit */
      v >>= 3;
    }
  while (i != 0 && v != 0);

  /* Leading zeros, if necessary.  */
  while (i != 0)
    where[--i] = '0';

  if (v != 0)
    {
      uintmax_t maxval = MAX_OCTAL_VAL_WITH_DIGITS (size);
      char buf1[UINTMAX_STRSIZE_BOUND];
      char buf2[UINTMAX_STRSIZE_BOUND];
      char buf3[UINTMAX_STRSIZE_BOUND];
      char *value_string = STRINGIFY_BIGINT (value, buf1);
      char *maxval_string = STRINGIFY_BIGINT (maxval, buf2);
      if (substitute)
	{
	  substitute &= maxval;
	  WARN ((0, 0, _("%s value %s too large (max=%s); substituting %s"),
		 type, value_string, maxval_string,
		 STRINGIFY_BIGINT (substitute, buf3)));
	  to_oct (substitute, (uintmax_t) 0, where, size, type);
	}
      else
	ERROR ((0, 0, _("%s value %s too large (max=%s)"),
		type, value_string, maxval_string));
    }
}
#ifndef GID_NOBODY
#define GID_NOBODY 0
#endif
void
gid_to_oct (gid_t v, char *p, size_t s)
{
  to_oct ((uintmax_t) v, (uintmax_t) GID_NOBODY, p, s, "gid_t");
}
void
major_to_oct (major_t v, char *p, size_t s)
{
  to_oct ((uintmax_t) v, (uintmax_t) 0, p, s, "major_t");
}
void
minor_to_oct (minor_t v, char *p, size_t s)
{
  to_oct ((uintmax_t) v, (uintmax_t) 0, p, s, "minor_t");
}
void
mode_to_oct (mode_t v, char *p, size_t s)
{
  /* In the common case where the internal and external mode bits are the same,
     propagate all unknown bits to the external mode.
     This matches historical practice.
     Otherwise, just copy the bits we know about.  */
  uintmax_t u =
    ((S_ISUID == TSUID && S_ISGID == TSGID && S_ISVTX == TSVTX
      && S_IRUSR == TUREAD && S_IWUSR == TUWRITE && S_IXUSR == TUEXEC
      && S_IRGRP == TGREAD && S_IWGRP == TGWRITE && S_IXGRP == TGEXEC
      && S_IROTH == TOREAD && S_IWOTH == TOWRITE && S_IXOTH == TOEXEC)
     ? v
     : ((v & S_ISUID ? TSUID : 0)
	| (v & S_ISGID ? TSGID : 0)
	| (v & S_ISVTX ? TSVTX : 0)
	| (v & S_IRUSR ? TUREAD : 0)
	| (v & S_IWUSR ? TUWRITE : 0)
	| (v & S_IXUSR ? TUEXEC : 0)
	| (v & S_IRGRP ? TGREAD : 0)
	| (v & S_IWGRP ? TGWRITE : 0)
	| (v & S_IXGRP ? TGEXEC : 0)
	| (v & S_IROTH ? TOREAD : 0)
	| (v & S_IWOTH ? TOWRITE : 0)
	| (v & S_IXOTH ? TOEXEC : 0)));
  to_oct (u, (uintmax_t) 0, p, s, "mode_t");
}
void
off_to_oct (off_t v, char *p, size_t s)
{
  to_oct ((uintmax_t) v, (uintmax_t) 0, p, s, "off_t");
}
void
size_to_oct (size_t v, char *p, size_t s)
{
  to_oct ((uintmax_t) v, (uintmax_t) 0, p, s, "size_t");
}
void
time_to_oct (time_t v, char *p, size_t s)
{
  to_oct ((uintmax_t) v, (uintmax_t) 0, p, s, "time_t");
}
#ifndef UID_NOBODY
#define UID_NOBODY 0
#endif
void
uid_to_oct (uid_t v, char *p, size_t s)
{
  to_oct ((uintmax_t) v, (uintmax_t) UID_NOBODY, p, s, "uid_t");
}
void
uintmax_to_oct (uintmax_t v, char *p, size_t s)
{
  to_oct (v, (uintmax_t) 0, p, s, "uintmax_t");
}

/* Writing routines.  */

/*-----------------------------------------------------------------------.
| Just zeroes out the buffer so we don't confuse ourselves with leftover |
| data.									 |
`-----------------------------------------------------------------------*/

static void
clear_buffer (char *buffer)
{
  memset (buffer, 0, BLOCKSIZE);
}

/*-------------------------------------------------------------------------.
| Write the EOT block(s).  We actually zero at least one block, through	   |
| the end of the record.  Old tar, as previous versions of GNU tar, writes |
| garbage after two zeroed blocks.					   |
`-------------------------------------------------------------------------*/

void
write_eot (void)
{
  union block *pointer = find_next_block ();

  if (pointer)
    {
      size_t space = available_space_after (pointer);

      memset (pointer->buffer, 0, space);
      set_next_block_after (pointer);
    }
}

/*-----------------------------------------------------.
| Write a GNUTYPE_LONGLINK or GNUTYPE_LONGNAME block.  |
`-----------------------------------------------------*/

/* FIXME: Cross recursion between start_header and write_long!  */

static union block *start_header PARAMS ((const char *, struct stat *));

static void
write_long (const char *p, char type)
{
  size_t size = strlen (p) + 1;
  size_t bufsize;
  union block *header;
  struct stat foo;

  memset (&foo, 0, sizeof foo);
  foo.st_size = size;

  header = start_header ("././@LongLink", &foo);
  header->header.typeflag = type;
  finish_header (header);

  header = find_next_block ();

  bufsize = available_space_after (header);

  while (bufsize < size)
    {
      memcpy (header->buffer, p, bufsize);
      p += bufsize;
      size -= bufsize;
      set_next_block_after (header + (bufsize - 1) / BLOCKSIZE);
      header = find_next_block ();
      bufsize = available_space_after (header);
    }
  memcpy (header->buffer, p, size);
  memset (header->buffer + size, 0, bufsize - size);
  set_next_block_after (header + (size - 1) / BLOCKSIZE);
}

/* Header handling.  */

/*---------------------------------------------------------------------.
| Make a header block for the file name whose stat info is st.  Return |
| header pointer for success, NULL if the name is too long.	       |
`---------------------------------------------------------------------*/

static union block *
start_header (const char *name, struct stat *st)
{
  union block *header;

  if (!absolute_names_option)
    {
      static int warned_once = 0;

#if MSDOS
      if (name[1] == ':')
	{
	  name += 2;
	  if (!warned_once)
	    {
	      warned_once = 1;
	      WARN ((0, 0, _("Removing drive spec from names in the archive")));
	    }
	}
#endif

      while (*name == '/')
	{
	  name++;		/* force relative path */
	  if (!warned_once)
	    {
	      warned_once = 1;
	      WARN ((0, 0, _("\
Removing leading `/' from absolute path names in the archive")));
	    }
	}
    }

  /* Check the file name and put it in the block.  */

  if (strlen (name) >= (size_t) NAME_FIELD_SIZE)
    write_long (name, GNUTYPE_LONGNAME);
  header = find_next_block ();
  memset (header->buffer, 0, sizeof (union block));

  assign_string (&current_file_name, name);

  strncpy (header->header.name, name, NAME_FIELD_SIZE);
  header->header.name[NAME_FIELD_SIZE - 1] = '\0';

  /* Override some stat fields, if requested to do so.  */

  if (owner_option != (uid_t) -1)
    st->st_uid = owner_option;
  if (group_option != (gid_t) -1)
    st->st_gid = group_option;
  if (mode_option)
    st->st_mode = ((st->st_mode & S_IFMT)
		   | mode_adjust (st->st_mode, mode_option));

  /* Paul Eggert tried the trivial test ($WRITER cf a b; $READER tvf a)
     for a few tars and came up with the following interoperability
     matrix:

	      WRITER
	1 2 3 4 5 6 7 8 9   READER
	. . . . . . . . .   1 = SunOS 4.2 tar
	# . . # # . . # #   2 = NEC SVR4.0.2 tar
	. . . # # . . # .   3 = Solaris 2.1 tar
	. . . . . . . . .   4 = GNU tar 1.11.1
	. . . . . . . . .   5 = HP-UX 8.07 tar
	. . . . . . . . .   6 = Ultrix 4.1
	. . . . . . . . .   7 = AIX 3.2
	. . . . . . . . .   8 = Hitachi HI-UX 1.03
	. . . . . . . . .   9 = Omron UNIOS-B 4.3BSD 1.60Beta

	     . = works
	     # = ``impossible file type''

     The following mask for old archive removes the `#'s in column 4
     above, thus making GNU tar both a universal donor and a universal
     acceptor for Paul's test.  */

  if (archive_format == V7_FORMAT)
    MODE_TO_OCT (st->st_mode & MODE_ALL, header->header.mode);
  else
    MODE_TO_OCT (st->st_mode, header->header.mode);

  UID_TO_OCT (st->st_uid, header->header.uid);
  GID_TO_OCT (st->st_gid, header->header.gid);
  OFF_TO_OCT (st->st_size, header->header.size);
  TIME_TO_OCT (st->st_mtime, header->header.mtime);

  if (incremental_option)
    if (archive_format == OLDGNU_FORMAT)
      {
	TIME_TO_OCT (st->st_atime, header->oldgnu_header.atime);
	TIME_TO_OCT (st->st_ctime, header->oldgnu_header.ctime);
      }

  header->header.typeflag = archive_format == V7_FORMAT ? AREGTYPE : REGTYPE;

  switch (archive_format)
    {
    case DEFAULT_FORMAT:
    case V7_FORMAT:
      break;

    case OLDGNU_FORMAT:
      /* Overwrite header->header.magic and header.version in one blow.  */
      strcpy (header->header.magic, OLDGNU_MAGIC);
      break;

    case POSIX_FORMAT:
    case GNU_FORMAT:
      strncpy (header->header.magic, TMAGIC, TMAGLEN);
      strncpy (header->header.version, TVERSION, TVERSLEN);
      break;
    }

  if (archive_format == V7_FORMAT || numeric_owner_option)
    {
      /* header->header.[ug]name are left as the empty string.  */
    }
  else
    {
      uid_to_uname (st->st_uid, header->header.uname);
      gid_to_gname (st->st_gid, header->header.gname);
    }

  return header;
}

/*-------------------------------------------------------------------------.
| Finish off a filled-in header block and write it out.  We also print the |
| file name and/or full info if verbose is on.				   |
`-------------------------------------------------------------------------*/

void
finish_header (union block *header)
{
  size_t i;
  int sum;
  char *p;

  memcpy (header->header.chksum, CHKBLANKS, sizeof (header->header.chksum));

  sum = 0;
  p = header->buffer;
  for (i = sizeof (*header); i-- != 0; )
    /* We can't use unsigned char here because of old compilers, e.g. V7.  */
    sum += 0xFF & *p++;

  /* Fill in the checksum field.  It's formatted differently from the
     other fields: it has [6] digits, a null, then a space -- rather than
     digits, then a null.  We use to_oct.
     The final space is already there, from
     checksumming, and to_oct doesn't modify it.

     This is a fast way to do:

     sprintf(header->header.chksum, "%6o", sum);  */

  uintmax_to_oct ((uintmax_t) sum, header->header.chksum, 7);

  set_next_block_after (header);

  if (verbose_option
      && header->header.typeflag != GNUTYPE_LONGLINK
      && header->header.typeflag != GNUTYPE_LONGNAME)
    {
      /* These globals are parameters to print_header, sigh.  */

      current_header = header;
      /* current_stat is already set up.  */
      current_format = archive_format;
      print_header ();
    }
}

/* Sparse file processing.  */

/*-------------------------------------------------------------------------.
| Takes a blockful of data and basically cruises through it to see if it's |
| made *entirely* of zeros, returning a 0 the instant it finds something   |
| that is a nonzero, i.e., useful data.					   |
`-------------------------------------------------------------------------*/

static int
zero_block_p (char *buffer)
{
  int counter;

  for (counter = 0; counter < BLOCKSIZE; counter++)
    if (buffer[counter] != '\0')
      return 0;
  return 1;
}

/*---.
| ?  |
`---*/

static void
init_sparsearray (void)
{
  int counter;

  sp_array_size = 10;

  /* Make room for our scratch space -- initially is 10 elts long.  */

  sparsearray = (struct sp_array *)
    xmalloc (sp_array_size * sizeof (struct sp_array));
  for (counter = 0; counter < sp_array_size; counter++)
    {
      sparsearray[counter].offset = 0;
      sparsearray[counter].numbytes = 0;
    }
}

/*---.
| ?  |
`---*/

static void
find_new_file_size (off_t *filesize, int highest_index)
{
  int counter;

  *filesize = 0;
  for (counter = 0;
       sparsearray[counter].numbytes && counter <= highest_index;
       counter++)
    *filesize += sparsearray[counter].numbytes;
}

/*-----------------------------------------------------------------------.
| Make one pass over the file NAME, studying where any non-zero data is, |
| that is, how far into the file each instance of data is, and how many  |
| bytes are there.  Save this information in the sparsearray, which will |
| later be translated into header information.                           |
`-----------------------------------------------------------------------*/

/* There is little point in trimming small amounts of null data at the head
   and tail of blocks, only avoid dumping full null blocks.  */

/* FIXME: this routine might accept bits of algorithmic cleanup, it is
   too kludgey for my taste...  */

static int
deal_with_sparse (char *name, union block *header)
{
  size_t numbytes = 0;
  off_t offset = 0;
  int file;
  int sparse_index = 0;
  ssize_t count;
  char buffer[BLOCKSIZE];

  if (archive_format == OLDGNU_FORMAT)
    header->oldgnu_header.isextended = 0;

  if (file = open (name, O_RDONLY), file < 0)
    /* This problem will be caught later on, so just return.  */
    return 0;

  init_sparsearray ();
  clear_buffer (buffer);

  while (count = safe_read (file, buffer, sizeof buffer), count != 0)
    {
      /* Realloc the scratch area as necessary.  FIXME: should reallocate
	 only at beginning of a new instance of non-zero data.  */

      if (sparse_index > sp_array_size - 1)
	{

	  sparsearray = (struct sp_array *)
	    xrealloc (sparsearray,
		      2 * sp_array_size * sizeof (struct sp_array));
	  sp_array_size *= 2;
	}

      /* Process one block.  */

      if (count == sizeof buffer)

	if (zero_block_p (buffer))
	  {
	    if (numbytes)
	      {
		sparsearray[sparse_index++].numbytes = numbytes;
		numbytes = 0;
	      }
	  }
	else
	  {
	    if (!numbytes)
	      sparsearray[sparse_index].offset = offset;
	    numbytes += count;
	  }

      else

	/* Since count < sizeof buffer, we have the last bit of the file.  */

	if (!zero_block_p (buffer))
	  {
	    if (!numbytes)
	      sparsearray[sparse_index].offset = offset;
	    numbytes += count;
	  }
	else
	  /* The next two lines are suggested by Andreas Degert, who says
	     they are required for trailing full blocks to be written to the
	     archive, when all zeroed.  Yet, it seems to me that the case
	     does not apply.  Further, at restore time, the file is not as
	     sparse as it should.  So, some serious cleanup is *also* needed
	     in this area.  Just one more... :-(.  FIXME.  */
	  if (numbytes)
	    numbytes += count;

      /* Prepare for next block.  */

      offset += count;
      /* FIXME: do not clear unless necessary.  */
      clear_buffer (buffer);
    }

  if (numbytes)
    sparsearray[sparse_index++].numbytes = numbytes;
  else
    {
      sparsearray[sparse_index].offset = offset - 1;
      sparsearray[sparse_index++].numbytes = 1;
    }

  close (file);
  return sparse_index - 1;
}

/*---.
| ?  |
`---*/

static int
finish_sparse_file (int file, off_t *sizeleft, off_t fullsize, char *name)
{
  union block *start;
  size_t bufsize;
  int sparse_index = 0;
  ssize_t count;

  while (*sizeleft > 0)
    {
      start = find_next_block ();
      memset (start->buffer, 0, BLOCKSIZE);
      bufsize = sparsearray[sparse_index].numbytes;
      if (!bufsize)
	{
	  /* We blew it, maybe.  */
	  char buf1[UINTMAX_STRSIZE_BOUND];
	  char buf2[UINTMAX_STRSIZE_BOUND];

	  ERROR ((0, 0, _("Wrote %s of %s bytes to file %s"),
		  STRINGIFY_BIGINT (fullsize - *sizeleft, buf1),
		  STRINGIFY_BIGINT (fullsize, buf2),
		  name));
	  break;
	}

      if (lseek (file, sparsearray[sparse_index++].offset, SEEK_SET) < 0)
	{
	  char buf[UINTMAX_STRSIZE_BOUND];
	  ERROR ((0, errno, _("lseek error at byte %s in file %s"),
		  STRINGIFY_BIGINT (sparsearray[sparse_index - 1].offset, buf),
		  name));
	  break;
	}

      /* If the number of bytes to be written here exceeds the size of
	 the temporary buffer, do it in steps.  */

      while (bufsize > BLOCKSIZE)
	{
#if 0
	  if (amount_read)
	    {
	      count = safe_read (file, start->buffer + amount_read,
				 BLOCKSIZE - amount_read);
	      bufsize -= BLOCKSIZE - amount_read;
	      amount_read = 0;
	      set_next_block_after (start);
	      start = find_next_block ();
	      memset (start->buffer, 0, BLOCKSIZE);
	    }
#endif
	  /* Store the data.  */

	  count = safe_read (file, start->buffer, BLOCKSIZE);
	  if (count < 0)
	    {
	      char buf[UINTMAX_STRSIZE_BOUND];
	      ERROR ((0, errno, _("\
Read error at byte %s, reading %lu bytes, in file %s"),
		      STRINGIFY_BIGINT (fullsize - *sizeleft, buf),
		      (unsigned long) bufsize, name));
	      return 1;
	    }
	  bufsize -= count;
	  *sizeleft -= count;
	  set_next_block_after (start);
	  start = find_next_block ();
	  memset (start->buffer, 0, BLOCKSIZE);
	}

      {
	char buffer[BLOCKSIZE];

	clear_buffer (buffer);
	count = safe_read (file, buffer, bufsize);
	memcpy (start->buffer, buffer, BLOCKSIZE);
      }

      if (count < 0)
	{
	  char buf[UINTMAX_STRSIZE_BOUND];
	  
	  ERROR ((0, errno,
		  _("Read error at byte %s, reading %lu bytes, in file %s"),
		  STRINGIFY_BIGINT (fullsize - *sizeleft, buf),
		  (unsigned long) bufsize, name));
	  return 1;
	}
#if 0
      if (amount_read >= BLOCKSIZE)
	{
	  amount_read = 0;
	  set_next_block_after (start + (count - 1) / BLOCKSIZE);
	  if (count != bufsize)
	    {
	      ERROR ((0, 0,
		      _("File %s shrunk, padding with zeros"),
		      name));
	      return 1;
	    }
	  start = find_next_block ();
	}
      else
	amount_read += bufsize;
#endif
      *sizeleft -= count;
      set_next_block_after (start);

    }
  free (sparsearray);
#if 0
  set_next_block_after (start + (count - 1) / BLOCKSIZE);
#endif
  return 0;
}

/* Main functions of this module.  */

/*---.
| ?  |
`---*/

void
create_archive (void)
{
  char *p;

  open_archive (ACCESS_WRITE);

  if (incremental_option)
    {
      char *buffer = xmalloc (PATH_MAX);
      const char *q;
      char *bufp;

      collect_and_sort_names ();

      while (p = name_from_list (), p)
	dump_file (p, (dev_t) -1, 1);

      blank_name_list ();
      while (p = name_from_list (), p)
	{
	  strcpy (buffer, p);
	  if (p[strlen (p) - 1] != '/')
	    strcat (buffer, "/");
	  bufp = buffer + strlen (buffer);
	  for (q = gnu_list_name->dir_contents;
	       q && *q;
	       q += strlen (q) + 1)
	    {
	      if (*q == 'Y')
		{
		  strcpy (bufp, q + 1);
		  dump_file (buffer, (dev_t) -1, 1);
		}
	    }
	}
      free (buffer);
    }
  else
    {
      while (p = name_next (1), p)
	dump_file (p, (dev_t) -1, 1);
    }

  write_eot ();
  close_archive ();

  if (listed_incremental_option)
    write_dir_file ();
}

/*----------------------------------------------------------------------.
| Dump a single file.  Recurse on directories.  Result is nonzero for   |
| success.  P is file name to dump.  PARENT_DEVICE is device our parent |
| directory was on.  TOP_LEVEL tells wether we are a toplevel call.     |
|                                                                       |
|  Sets global CURRENT_STAT to stat output for this file.               |
`----------------------------------------------------------------------*/

/* FIXME: One should make sure that for *every* path leading to setting
   exit_status to failure, a clear diagnostic has been issued.  */

void
dump_file (char *p, dev_t parent_device, int top_level)
{
  union block *header;
  char type;
  union block *exhdr;
  char save_typeflag;
  struct utimbuf restore_times;
  off_t restore_size;

  /* FIXME: `header' and `upperbound' might be used uninitialized in this
     function.  Reported by Bruno Haible.  */

  if (interactive_option && !confirm ("add", p))
    return;

  /* Use stat if following (rather than dumping) 4.2BSD's symbolic links.
     Otherwise, use lstat (which falls back to stat if no symbolic links).  */

  if (dereference_option != 0
#if STX_HIDDEN && !_LARGE_FILES /* AIX */
      ? statx (p, &current_stat, STATSIZE, STX_HIDDEN)
      : statx (p, &current_stat, STATSIZE, STX_HIDDEN | STX_LINK)
#else
      ? stat (p, &current_stat) : lstat (p, &current_stat)
#endif
      )
    {
      WARN ((0, errno, _("Cannot add file %s"), p));
      if (!ignore_failed_read_option)
	exit_status = TAREXIT_FAILURE;
      return;
    }

  restore_times.actime = current_stat.st_atime;
  restore_times.modtime = current_stat.st_mtime;
  restore_size = current_stat.st_size;

#ifdef S_ISHIDDEN
  if (S_ISHIDDEN (current_stat.st_mode))
    {
      char *new = (char *) alloca (strlen (p) + 2);
      if (new)
	{
	  strcpy (new, p);
	  strcat (new, "@");
	  p = new;
	}
    }
#endif

  /* See if we only want new files, and check if this one is too old to
     put in the archive.  */

  if (!incremental_option && !S_ISDIR (current_stat.st_mode)
      && current_stat.st_mtime < newer_mtime_option
      && (!after_date_option || current_stat.st_ctime < newer_ctime_option))
    {
      if (parent_device == (dev_t) -1)
	WARN ((0, 0, _("%s: is unchanged; not dumped"), p));
      /* FIXME: recheck this return.  */
      return;
    }

#if !MSDOS
  /* See if we are trying to dump the archive.  */

  if (ar_dev && current_stat.st_dev == ar_dev && current_stat.st_ino == ar_ino)
    {
      WARN ((0, 0, _("%s is the archive; not dumped"), p));
      return;
    }
#endif

  /* Check for multiple links.

     We maintain a list of all such files that we've written so far.  Any
     time we see another, we check the list and avoid dumping the data
     again if we've done it once already.  */

  if (current_stat.st_nlink > 1
      && (S_ISREG (current_stat.st_mode)
#ifdef S_ISCTG
	  || S_ISCTG (current_stat.st_mode)
#endif
#ifdef S_ISCHR
	  || S_ISCHR (current_stat.st_mode)
#endif
#ifdef S_ISBLK
	  || S_ISBLK (current_stat.st_mode)
#endif
#ifdef S_ISFIFO
	  || S_ISFIFO (current_stat.st_mode)
#endif
      ))
    {
      struct link *lp;

      /* FIXME: First quick and dirty.  Hashing, etc later.  */

      for (lp = linklist; lp; lp = lp->next)
	if (lp->ino == current_stat.st_ino && lp->dev == current_stat.st_dev)
	  {
	    char *link_name = lp->name;

	    /* We found a link.  */

	    while (!absolute_names_option && *link_name == '/')
	      {
		static int warned_once = 0;

		if (!warned_once)
		  {
		    warned_once = 1;
		    WARN ((0, 0, _("\
Removing leading `/' from absolute links")));
		  }
		link_name++;
	      }
	    if (strlen (link_name) >= NAME_FIELD_SIZE)
	      write_long (link_name, GNUTYPE_LONGLINK);
	    assign_string (&current_link_name, link_name);

	    current_stat.st_size = 0;
	    header = start_header (p, &current_stat);
	    if (header == NULL)
	      {
		exit_status = TAREXIT_FAILURE;
		return;
	      }
	    strncpy (header->header.linkname,
		     link_name, NAME_FIELD_SIZE);

	    /* Force null truncated.  */

	    header->header.linkname[NAME_FIELD_SIZE - 1] = 0;

	    header->header.typeflag = LNKTYPE;
	    finish_header (header);

	    /* FIXME: Maybe remove from list after all links found?  */

	    if (remove_files_option)
	      if (unlink (p) == -1)
		ERROR ((0, errno, _("Cannot remove %s"), p));

	    /* We dumped it.  */
	    return;
	  }

      /* Not found.  Add it to the list of possible links.  */

      lp = (struct link *)
	xmalloc ((size_t) (sizeof (struct link) + strlen (p)));
      lp->ino = current_stat.st_ino;
      lp->dev = current_stat.st_dev;
      strcpy (lp->name, p);
      lp->next = linklist;
      linklist = lp;
    }

  /* This is not a link to a previously dumped file, so dump it.  */

  if (S_ISREG (current_stat.st_mode)
#ifdef S_ISCTG
      || S_ISCTG (current_stat.st_mode)
#endif
      )
    {
      int f;			/* file descriptor */
      size_t bufsize;
      ssize_t count;
      off_t sizeleft;
      union block *start;
      int header_moved;
      char isextended = 0;
      int upperbound;
#if 0
      static int cried_once = 0;
#endif

      header_moved = 0;

      if (sparse_option)
	{
	  /* Check the size of the file against the number of blocks
	     allocated for it, counting both data and indirect blocks.
	     If there is a smaller number of blocks that would be
	     necessary to accommodate a file of this size, this is safe
	     to say that we have a sparse file: at least one of those
	     blocks in the file is just a useless hole.  For sparse
	     files not having more hole blocks than indirect blocks, the
	     sparseness will go undetected.  */

	  /* Bruno Haible sent me these statistics for Linux.  It seems
	     that some filesystems count indirect blocks in st_blocks,
	     while others do not seem to:

	     minix-fs   tar: size=7205, st_blocks=18 and ST_NBLOCKS=18
	     extfs      tar: size=7205, st_blocks=18 and ST_NBLOCKS=18
	     ext2fs     tar: size=7205, st_blocks=16 and ST_NBLOCKS=16
	     msdos-fs   tar: size=7205, st_blocks=16 and ST_NBLOCKS=16

	     Dick Streefland reports the previous numbers as misleading,
	     because ext2fs use 12 direct blocks, while minix-fs uses only
	     6 direct blocks.  Dick gets:

	     ext2	size=20480	ls listed blocks=21
	     minix	size=20480	ls listed blocks=21
	     msdos	size=20480	ls listed blocks=20

	     It seems that indirect blocks *are* included in st_blocks.
	     The minix filesystem does not account for phantom blocks in
	     st_blocks, so `du' and `ls -s' give wrong results.  So, the
	     --sparse option would not work on a minix filesystem.  */

	  if (ST_NBLOCKS (current_stat)
	      < (current_stat.st_size / ST_NBLOCKSIZE
		 + (current_stat.st_size % ST_NBLOCKSIZE != 0)))
	    {
	      off_t filesize = current_stat.st_size;
	      int counter;

	      header = start_header (p, &current_stat);
	      if (header == NULL)
		{
		  exit_status = TAREXIT_FAILURE;
		  return;
		}
	      header->header.typeflag = GNUTYPE_SPARSE;
	      header_moved = 1;

	      /* Call the routine that figures out the layout of the
		 sparse file in question.  UPPERBOUND is the index of the
		 last element of the "sparsearray," i.e., the number of
		 elements it needed to describe the file.  */

	      upperbound = deal_with_sparse (p, header);

	      /* See if we'll need an extended header later.  */

	      if (upperbound > SPARSES_IN_OLDGNU_HEADER - 1)
		header->oldgnu_header.isextended = 1;

	      /* We store the "real" file size so we can show that in
		 case someone wants to list the archive, i.e., tar tvf
		 <file>.  It might be kind of disconcerting if the
		 shrunken file size was the one that showed up.  */

	      OFF_TO_OCT (current_stat.st_size,
			  header->oldgnu_header.realsize);

	      /* This will be the new "size" of the file, i.e., the size
		 of the file minus the blocks of holes that we're
		 skipping over.  */

	      find_new_file_size (&filesize, upperbound);
	      current_stat.st_size = filesize;
	      OFF_TO_OCT (filesize, header->header.size);

	      for (counter = 0; counter < SPARSES_IN_OLDGNU_HEADER; counter++)
		{
		  if (!sparsearray[counter].numbytes)
		    break;

		  OFF_TO_OCT (sparsearray[counter].offset,
			      header->oldgnu_header.sp[counter].offset);
		  SIZE_TO_OCT (sparsearray[counter].numbytes,
			       header->oldgnu_header.sp[counter].numbytes);
		}

	    }
	}
      else
	upperbound = SPARSES_IN_OLDGNU_HEADER - 1;

      sizeleft = current_stat.st_size;

      /* Don't bother opening empty, world readable files.  Also do not open
	 files when archive is meant for /dev/null.  */

      if (dev_null_output
	  || (sizeleft == 0
	      && MODE_R == (MODE_R & current_stat.st_mode)))
	f = -1;
      else
	{
	  f = open (p, O_RDONLY | O_BINARY);
	  if (f < 0)
	    {
	      WARN ((0, errno, _("Cannot add file %s"), p));
	      if (!ignore_failed_read_option)
		exit_status = TAREXIT_FAILURE;
	      return;
	    }
	}

      /* If the file is sparse, we've already taken care of this.  */

      if (!header_moved)
	{
	  header = start_header (p, &current_stat);
	  if (header == NULL)
	    {
	      if (f >= 0)
		close (f);
	      exit_status = TAREXIT_FAILURE;
	      return;
	    }
	}
#ifdef S_ISCTG
      /* Mark contiguous files, if we support them.  */

      if (archive_format != V7_FORMAT && S_ISCTG (current_stat.st_mode))
	header->header.typeflag = CONTTYPE;
#endif
      isextended = header->oldgnu_header.isextended;
      save_typeflag = header->header.typeflag;
      finish_header (header);
      if (isextended)
	{
#if 0
	  int sum = 0;
#endif
	  int counter;
#if 0
	  union block *exhdr;
	  int arraybound = SPARSES_IN_SPARSE_HEADER;
#endif
	  /* static */ int index_offset = SPARSES_IN_OLDGNU_HEADER;

	extend:
	  exhdr = find_next_block ();

	  if (exhdr == NULL)
	    {
	      exit_status = TAREXIT_FAILURE;
	      return;
	    }
	  memset (exhdr->buffer, 0, BLOCKSIZE);
	  for (counter = 0; counter < SPARSES_IN_SPARSE_HEADER; counter++)
	    {
	      if (counter + index_offset > upperbound)
		break;

	      SIZE_TO_OCT (sparsearray[counter + index_offset].numbytes,
			   exhdr->sparse_header.sp[counter].numbytes);
	      OFF_TO_OCT (sparsearray[counter + index_offset].offset,
			  exhdr->sparse_header.sp[counter].offset);
	    }
	  set_next_block_after (exhdr);
#if 0
	  sum += counter;
	  if (sum < upperbound)
	    goto extend;
#endif
	  if (index_offset + counter <= upperbound)
	    {
	      index_offset += counter;
	      exhdr->sparse_header.isextended = 1;
	      goto extend;
	    }

	}
      if (save_typeflag == GNUTYPE_SPARSE)
	{
	  if (f < 0
	      || finish_sparse_file (f, &sizeleft, current_stat.st_size, p))
	    goto padit;
	}
      else
	while (sizeleft > 0)
	  {
	    if (multi_volume_option)
	      {
		assign_string (&save_name, p);
		save_sizeleft = sizeleft;
		save_totsize = current_stat.st_size;
	      }
	    start = find_next_block ();

	    bufsize = available_space_after (start);

	    if (sizeleft < bufsize)
	      {
		/* Last read -- zero out area beyond.  */

		bufsize = sizeleft;
		count = bufsize % BLOCKSIZE;
		if (count)
		  memset (start->buffer + sizeleft, 0,
			  (size_t) (BLOCKSIZE - count));
	      }
	    if (f < 0)
	      count = bufsize;
	    else
	      count = safe_read (f, start->buffer, bufsize);
	    if (count < 0)
	      {
		char buf[UINTMAX_STRSIZE_BOUND];
		ERROR ((0, errno, _("\
Read error at byte %s, reading %lu bytes, in file %s"),
			STRINGIFY_BIGINT (current_stat.st_size - sizeleft,
					  buf),
			(unsigned long) bufsize, p));
		goto padit;
	      }
	    sizeleft -= count;

	    /* This is nonportable (the type of set_next_block_after's arg).  */

	    set_next_block_after (start + (count - 1) / BLOCKSIZE);

	    if (count == bufsize)
	      continue;
	    else
	      {
		char buf[UINTMAX_STRSIZE_BOUND];
		ERROR ((0, 0,
			_("File %s shrunk by %s bytes, padding with zeros"),
			p, STRINGIFY_BIGINT (sizeleft, buf)));
		goto padit;		/* short read */
	      }
	  }

      if (multi_volume_option)
	assign_string (&save_name, NULL);

      if (f >= 0)
	{
	  struct stat final_stat;
	  if (fstat (f, &final_stat) != 0)
	    ERROR ((0, errno, "%s: fstat", p));
	  else if (final_stat.st_mtime != restore_times.modtime
		   || final_stat.st_size != restore_size)
	    ERROR ((0, errno, _("%s: file changed as we read it"), p));
	  if (close (f) != 0)
	    ERROR ((0, errno, _("%s: close"), p));
	  if (atime_preserve_option)
	    utime (p, &restore_times);
	}
      if (remove_files_option)
	{
	  if (unlink (p) == -1)
	    ERROR ((0, errno, _("Cannot remove %s"), p));
	}
      return;

      /* File shrunk or gave error, pad out tape to match the size we
	 specified in the header.  */

    padit:
      while (sizeleft > 0)
	{
	  save_sizeleft = sizeleft;
	  start = find_next_block ();
	  memset (start->buffer, 0, BLOCKSIZE);
	  set_next_block_after (start);
	  sizeleft -= BLOCKSIZE;
	}
      if (multi_volume_option)
	assign_string (&save_name, NULL);
      if (f >= 0)
	{
	  close (f);
	  if (atime_preserve_option)
	    utime (p, &restore_times);
	}
      return;
    }

#ifdef S_ISLNK
  else if (S_ISLNK (current_stat.st_mode))
    {
      int size;
      char *buffer = (char *) alloca (PATH_MAX + 1);

      size = readlink (p, buffer, PATH_MAX + 1);
      if (size < 0)
	{
	  WARN ((0, errno, _("Cannot add file %s"), p));
	  if (!ignore_failed_read_option)
	    exit_status = TAREXIT_FAILURE;
	  return;
	}
      buffer[size] = '\0';
      if (size >= NAME_FIELD_SIZE)
	write_long (buffer, GNUTYPE_LONGLINK);
      assign_string (&current_link_name, buffer);

      current_stat.st_size = 0;	/* force 0 size on symlink */
      header = start_header (p, &current_stat);
      if (header == NULL)
	{
	  exit_status = TAREXIT_FAILURE;
	  return;
	}
      strncpy (header->header.linkname, buffer, NAME_FIELD_SIZE);
      header->header.linkname[NAME_FIELD_SIZE - 1] = '\0';
      header->header.typeflag = SYMTYPE;
      finish_header (header);	/* nothing more to do to it */
      if (remove_files_option)
	{
	  if (unlink (p) == -1)
	    ERROR ((0, errno, _("Cannot remove %s"), p));
	}
      return;
    }
#endif /* S_ISLNK */

  else if (S_ISDIR (current_stat.st_mode))
    {
      DIR *directory;
      struct dirent *entry;
      char *namebuf;
      size_t buflen;
      size_t len;
      dev_t our_device = current_stat.st_dev;

      /* If this tar program is installed suid root, like for Amanda, the
	 access might look like denied, while it is not really.

	 FIXME: I have the feeling this test is done too early.  Couldn't it
	 just be bundled in later actions?  I guess that the proper support
	 of --ignore-failed-read is the key of the current writing.  */

      if (access (p, R_OK) == -1 && geteuid () != 0)
	{
	  WARN ((0, errno, _("Cannot add directory %s"), p));
	  if (!ignore_failed_read_option)
	    exit_status = TAREXIT_FAILURE;
	  return;
	}

      /* Build new prototype name.  Ensure exactly one trailing slash.  */

      len = strlen (p);
      buflen = len + NAME_FIELD_SIZE;
      namebuf = xmalloc (buflen + 1);
      strncpy (namebuf, p, buflen);
      while (len >= 1 && namebuf[len - 1] == '/')
	len--;
      namebuf[len++] = '/';
      namebuf[len] = '\0';

      if (1)
	{
	  /* The "1" above used to be "archive_format != V7_FORMAT", GNU tar
	     was just not writing directory blocks at all.  Daniel Trinkle
	     writes: ``All old versions of tar I have ever seen have
	     correctly archived an empty directory.  The really old ones I
	     checked included HP-UX 7 and Mt. Xinu More/BSD.  There may be
	     some subtle reason for the exclusion that I don't know, but the
	     current behavior is broken.''  I do not know those subtle
	     reasons either, so until these are reported (anew?), just allow
	     directory blocks to be written even with old archives.  */

	  current_stat.st_size = 0;	/* force 0 size on dir */

	  /* FIXME: If people could really read standard archives, this
	     should be:

	     header
	       = start_header (standard_option ? p : namebuf, &current_stat);

	     but since they'd interpret DIRTYPE blocks as regular
	     files, we'd better put the / on the name.  */

	  header = start_header (namebuf, &current_stat);
	  if (header == NULL)
	    {
	      exit_status = TAREXIT_FAILURE;
	      return;	/* eg name too long */
	    }

	  if (incremental_option)
	    header->header.typeflag = GNUTYPE_DUMPDIR;
	  else /* if (standard_option) */
	    header->header.typeflag = DIRTYPE;

	  /* If we're gnudumping, we aren't done yet so don't close it.  */

	  if (!incremental_option)
	    finish_header (header);	/* done with directory header */
	}

      if (incremental_option && gnu_list_name->dir_contents)
	{
	  off_t sizeleft;
	  off_t totsize;
	  size_t bufsize;
	  union block *start;
	  ssize_t count;
	  const char *buffer, *p_buffer;

	  buffer = gnu_list_name->dir_contents; /* FOO */
	  totsize = 0;
	  for (p_buffer = buffer; p_buffer && *p_buffer;)
	    {
	      size_t tmp;

	      tmp = strlen (p_buffer) + 1;
	      totsize += tmp;
	      p_buffer += tmp;
	    }
	  totsize++;
	  OFF_TO_OCT (totsize, header->header.size);
	  finish_header (header);
	  p_buffer = buffer;
	  sizeleft = totsize;
	  while (sizeleft > 0)
	    {
	      if (multi_volume_option)
		{
		  assign_string (&save_name, p);
		  save_sizeleft = sizeleft;
		  save_totsize = totsize;
		}
	      start = find_next_block ();
	      bufsize = available_space_after (start);
	      if (sizeleft < bufsize)
		{
		  bufsize = sizeleft;
		  count = bufsize % BLOCKSIZE;
		  if (count)
		    memset (start->buffer + sizeleft, 0,
			   (size_t) (BLOCKSIZE - count));
		}
	      memcpy (start->buffer, p_buffer, bufsize);
	      sizeleft -= bufsize;
	      p_buffer += bufsize;
	      set_next_block_after (start + (bufsize - 1) / BLOCKSIZE);
	    }
	  if (multi_volume_option)
	    assign_string (&save_name, NULL);
	  if (atime_preserve_option)
	    utime (p, &restore_times);
	  return;
	}

      /* See if we are about to recurse into a directory, and avoid doing
	 so if the user wants that we do not descend into directories.  */

      if (no_recurse_option)
	return;

      /* See if we are crossing from one file system to another, and
	 avoid doing so if the user only wants to dump one file system.  */

      if (one_file_system_option && !top_level
	  && parent_device != current_stat.st_dev)
	{
	  if (verbose_option)
	    WARN ((0, 0, _("%s: On a different filesystem; not dumped"), p));
	  return;
	}

      /* Now output all the files in the directory.  */

      errno = 0;		/* FIXME: errno should be read-only */

      directory = opendir (p);
      if (!directory)
	{
	  ERROR ((0, errno, _("Cannot open directory %s"), p));
	  return;
	}

      /* Hack to remove "./" from the front of all the file names.  */

      if (len == 2 && namebuf[0] == '.' && namebuf[1] == '/')
	len = 0;

      /* FIXME: Should speed this up by cd-ing into the dir.  */

      while (entry = readdir (directory), entry)
	{
	  /* Skip `.', `..', and excluded file names.  */

	  if (is_dot_or_dotdot (entry->d_name)
	      || excluded_filename (excluded, entry->d_name))
	    continue;

	  if ((int) NAMLEN (entry) + len >= buflen)
	    {
	      buflen = len + NAMLEN (entry);
	      namebuf = (char *) xrealloc (namebuf, buflen + 1);
#if 0
	      namebuf[len] = '\0';
	      ERROR ((0, 0, _("File name %s%s too long"),
		      namebuf, entry->d_name));
	      continue;
#endif
	    }
	  strcpy (namebuf + len, entry->d_name);
	  dump_file (namebuf, our_device, 0);
	}

      closedir (directory);
      free (namebuf);
      if (atime_preserve_option)
	utime (p, &restore_times);
      return;
    }

#ifdef S_ISCHR
  else if (S_ISCHR (current_stat.st_mode))
    type = CHRTYPE;
#endif

#ifdef S_ISBLK
  else if (S_ISBLK (current_stat.st_mode))
    type = BLKTYPE;
#endif

  /* Avoid screwy apollo lossage where S_IFIFO == S_IFSOCK.  */

#if (_ISP__M68K == 0) && (_ISP__A88K == 0) && defined(S_ISFIFO)
  else if (S_ISFIFO (current_stat.st_mode))
    type = FIFOTYPE;
#endif

#ifdef S_ISSOCK
  else if (S_ISSOCK (current_stat.st_mode))
    type = FIFOTYPE;
#endif

  else
    goto unknown;

  if (archive_format == V7_FORMAT)
    goto unknown;

  current_stat.st_size = 0;	/* force 0 size */
  header = start_header (p, &current_stat);
  if (header == NULL)
    {
      exit_status = TAREXIT_FAILURE;
      return;	/* eg name too long */
    }

  header->header.typeflag = type;

#if defined(S_IFBLK) || defined(S_IFCHR)
  if (type != FIFOTYPE)
    {
      MAJOR_TO_OCT (major (current_stat.st_rdev), header->header.devmajor);
      MINOR_TO_OCT (minor (current_stat.st_rdev), header->header.devminor);
    }
#endif

  finish_header (header);
  if (remove_files_option)
    {
      if (unlink (p) == -1)
	ERROR ((0, errno, _("Cannot remove %s"), p));
    }
  return;

unknown:
  ERROR ((0, 0, _("%s: Unknown file type; file ignored"), p));
}
