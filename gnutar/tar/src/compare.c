/* Diff files from a tar archive.
   Copyright (C) 1988, 92, 93, 94, 96, 97, 1999 Free Software Foundation, Inc.
   Written by John Gilmore, on 1987-04-30.

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

#if HAVE_LINUX_FD_H
# include <linux/fd.h>
#endif

#include "common.h"
#include "rmt.h"

/* Spare space for messages, hopefully safe even after gettext.  */
#define MESSAGE_BUFFER_SIZE 100

/* Nonzero if we are verifying at the moment.  */
int now_verifying = 0;

/* File descriptor for the file we are diffing.  */
static int diff_handle;

/* Area for reading file contents into.  */
static char *diff_buffer = NULL;

/*--------------------------------.
| Initialize for a diff operation |
`--------------------------------*/

void
diff_init (void)
{
  diff_buffer = (char *) valloc (record_size);
  if (!diff_buffer)
    FATAL_ERROR ((0, 0,
		  _("Could not allocate memory for diff buffer of %lu bytes"),
		  (unsigned long) record_size));
}

/*------------------------------------------------------------------------.
| Sigh about something that differs by writing a MESSAGE to stdlis, given |
| MESSAGE is not NULL.  Also set the exit status if not already.          |
`------------------------------------------------------------------------*/

static void
report_difference (const char *message)
{
  if (message)
    fprintf (stdlis, "%s: %s\n", current_file_name, message);

  if (exit_status == TAREXIT_SUCCESS)
    exit_status = TAREXIT_DIFFERS;
}

/*-----------------------------------------------------------------------.
| Takes a buffer returned by read_and_process and does nothing with it.	 |
`-----------------------------------------------------------------------*/

/* Yes, I know.  SIZE and DATA are unused in this function.  Some compilers
   may even report it.  That's OK, just relax!  */

static int
process_noop (size_t size, char *data)
{
  return 1;
}

/*---.
| ?  |
`---*/

static int
process_rawdata (size_t bytes, char *buffer)
{
  ssize_t status = safe_read (diff_handle, diff_buffer, bytes);
  char message[MESSAGE_BUFFER_SIZE];

  if (status != bytes)
    {
      if (status < 0)
	{
	  WARN ((0, errno, _("Cannot read %s"), current_file_name));
	  report_difference (NULL);
	}
      else
	{
	  sprintf (message, _("Could only read %lu of %lu bytes"),
		   (unsigned long) status, (unsigned long) bytes);
	  report_difference (message);
	}
      return 0;
    }

  if (memcmp (buffer, diff_buffer, bytes))
    {
      report_difference (_("Data differs"));
      return 0;
    }

  return 1;
}

/*---.
| ?  |
`---*/

/* Directory contents, only for GNUTYPE_DUMPDIR.  */

static char *dumpdir_cursor;

static int
process_dumpdir (size_t bytes, char *buffer)
{
  if (memcmp (buffer, dumpdir_cursor, bytes))
    {
      report_difference (_("Data differs"));
      return 0;
    }

  dumpdir_cursor += bytes;
  return 1;
}

/*------------------------------------------------------------------------.
| Some other routine wants SIZE bytes in the archive.  For each chunk of  |
| the archive, call PROCESSOR with the size of the chunk, and the address |
| of the chunk it can work with.  The PROCESSOR should return nonzero for |
| success.  It it return error once, continue skipping without calling    |
| PROCESSOR anymore.                                                      |
`------------------------------------------------------------------------*/

static void
read_and_process (size_t size, int (*processor) (size_t, char *))
{
  union block *data_block;
  size_t data_size;

  if (multi_volume_option)
    save_sizeleft = size;
  while (size)
    {
      data_block = find_next_block ();
      if (data_block == NULL)
	{
	  ERROR ((0, 0, _("Unexpected EOF on archive file")));
	  return;
	}

      data_size = available_space_after (data_block);
      if (data_size > size)
	data_size = size;
      if (!(*processor) (data_size, data_block->buffer))
	processor = process_noop;
      set_next_block_after ((union block *)
			    (data_block->buffer + data_size - 1));
      size -= data_size;
      if (multi_volume_option)
	save_sizeleft -= data_size;
    }
}

/*---.
| ?  |
`---*/

/* JK This routine should be used more often than it is ... look into
   that.  Anyhow, what it does is translate the sparse information on the
   header, and in any subsequent extended headers, into an array of
   structures with true numbers, as opposed to character strings.  It
   simply makes our life much easier, doing so many comparisong and such.
   */

static void
fill_in_sparse_array (void)
{
  int counter;

  /* Allocate space for our scratch space; it's initially 10 elements
     long, but can change in this routine if necessary.  */

  sp_array_size = 10;
  sparsearray = (struct sp_array *) xmalloc (sp_array_size * sizeof (struct sp_array));

  /* There are at most five of these structures in the header itself;
     read these in first.  */

  for (counter = 0; counter < SPARSES_IN_OLDGNU_HEADER; counter++)
    {
      /* Compare to 0, or use !(int)..., for Pyramid's dumb compiler.  */
      if (current_header->oldgnu_header.sp[counter].numbytes == 0)
	break;

      sparsearray[counter].offset =
	OFF_FROM_OCT (current_header->oldgnu_header.sp[counter].offset);
      sparsearray[counter].numbytes =
	SIZE_FROM_OCT (current_header->oldgnu_header.sp[counter].numbytes);
    }

  /* If the header's extended, we gotta read in exhdr's till we're done.  */

  if (current_header->oldgnu_header.isextended)
    {
      /* How far into the sparsearray we are `so far'.  */
      static int so_far_ind = SPARSES_IN_OLDGNU_HEADER;
      union block *exhdr;

      while (1)
	{
	  exhdr = find_next_block ();
	  for (counter = 0; counter < SPARSES_IN_SPARSE_HEADER; counter++)
	    {
	      if (counter + so_far_ind > sp_array_size - 1)
		{
		  /* We just ran out of room in our scratch area -
		     realloc it.  */

		  sp_array_size *= 2;
		  sparsearray = (struct sp_array *)
		    xrealloc (sparsearray,
			      sp_array_size * sizeof (struct sp_array));
		}

	      /* Convert the character strings into offsets and sizes.  */

	      sparsearray[counter + so_far_ind].offset =
		OFF_FROM_OCT (exhdr->sparse_header.sp[counter].offset);
	      sparsearray[counter + so_far_ind].numbytes =
		SIZE_FROM_OCT (exhdr->sparse_header.sp[counter].numbytes);
	    }

	  /* If this is the last extended header for this file, we can
	     stop.  */

	  if (!exhdr->sparse_header.isextended)
	    break;

	  so_far_ind += SPARSES_IN_SPARSE_HEADER;
	  set_next_block_after (exhdr);
	}

      /* Be sure to skip past the last one.  */

      set_next_block_after (exhdr);
    }
}

/*---.
| ?  |
`---*/

/* JK Diff'ing a sparse file with its counterpart on the tar file is a
   bit of a different story than a normal file.  First, we must know what
   areas of the file to skip through, i.e., we need to contruct a
   sparsearray, which will hold all the information we need.  We must
   compare small amounts of data at a time as we find it.  */

/* FIXME: This does not look very solid to me, at first glance.  Zero areas
   are not checked, spurious sparse entries seemingly goes undetected, and
   I'm not sure overall identical sparsity is verified.  */

static void
diff_sparse_files (off_t size_of_file)
{
  off_t remaining_size = size_of_file;
  char *buffer = (char *) xmalloc (BLOCKSIZE * sizeof (char));
  size_t buffer_size = BLOCKSIZE;
  union block *data_block = NULL;
  int counter = 0;
  int different = 0;

  fill_in_sparse_array ();

  while (remaining_size > 0)
    {
      ssize_t status;
      size_t chunk_size;
      off_t offset;

#if 0
      off_t amount_read = 0;
#endif

      data_block = find_next_block ();
      chunk_size = sparsearray[counter].numbytes;
      if (!chunk_size)
	break;

      offset = sparsearray[counter].offset;
      if (lseek (diff_handle, offset, SEEK_SET) < 0)
	{
	  char buf[UINTMAX_STRSIZE_BOUND];
	  WARN ((0, errno, _("Cannot seek to %s in file %s"),
		 STRINGIFY_BIGINT (offset, buf), current_file_name));
	  report_difference (NULL);
	}

      /* Take care to not run out of room in our buffer.  */

      while (buffer_size < chunk_size)
	{
	  if (buffer_size * 2 < buffer_size)
	    FATAL_ERROR ((0, 0, _("Memory exhausted")));
	  buffer_size *= 2;
	  buffer = (char *) xrealloc (buffer, buffer_size * sizeof (char));
	}

      while (chunk_size > BLOCKSIZE)
	{
	  if (status = safe_read (diff_handle, buffer, BLOCKSIZE),
	      status != BLOCKSIZE)
	    {
	      if (status < 0)
		{
		  WARN ((0, errno, _("Cannot read %s"), current_file_name));
		  report_difference (NULL);
		}
	      else
		{
		  char message[MESSAGE_BUFFER_SIZE];

		  sprintf (message, _("Could only read %lu of %lu bytes"),
			   (unsigned long) status, (unsigned long) chunk_size);
		  report_difference (message);
		}
	      break;
	    }

	  if (memcmp (buffer, data_block->buffer, BLOCKSIZE))
	    {
	      different = 1;
	      break;
	    }

	  chunk_size -= status;
	  remaining_size -= status;
	  set_next_block_after (data_block);
	  data_block = find_next_block ();
	}
      if (status = safe_read (diff_handle, buffer, chunk_size),
	  status != chunk_size)
	{
	  if (status < 0)
	    {
	      WARN ((0, errno, _("Cannot read %s"), current_file_name));
	      report_difference (NULL);
	    }
	  else
	    {
	      char message[MESSAGE_BUFFER_SIZE];

	      sprintf (message, _("Could only read %lu of %lu bytes"),
		       (unsigned long) status, (unsigned long) chunk_size);
	      report_difference (message);
	    }
	  break;
	}

      if (memcmp (buffer, data_block->buffer, chunk_size))
	{
	  different = 1;
	  break;
	}
#if 0
      amount_read += chunk_size;
      if (amount_read >= BLOCKSIZE)
	{
	  amount_read = 0;
	  set_next_block_after (data_block);
	  data_block = find_next_block ();
	}
#endif
      set_next_block_after (data_block);
      counter++;
      remaining_size -= chunk_size;
    }

#if 0
  /* If the number of bytes read isn't the number of bytes supposedly in
     the file, they're different.  */

  if (amount_read != size_of_file)
    different = 1;
#endif

  set_next_block_after (data_block);
  free (sparsearray);

  if (different)
    report_difference (_("Data differs"));
}

/*---------------------------------------------------------------------.
| Call either stat or lstat over STAT_DATA, depending on --dereference |
| (-h), for a file which should exist.  Diagnose any problem.  Return  |
| nonzero for success, zero otherwise.				       |
`---------------------------------------------------------------------*/

static int
get_stat_data (struct stat *stat_data)
{
  int status = (dereference_option
		? stat (current_file_name, stat_data)
		: lstat (current_file_name, stat_data));

  if (status < 0)
    {
      if (errno == ENOENT)
	report_difference (_("File does not exist"));
      else
	{
	  ERROR ((0, errno, _("Cannot stat file %s"), current_file_name));
	  report_difference (NULL);
	}
#if 0
      skip_file (current_stat.st_size);
#endif
      return 0;
    }

  return 1;
}

/*----------------------------------.
| Diff a file against the archive.  |
`----------------------------------*/

void
diff_archive (void)
{
  struct stat stat_data;
  size_t name_length;
  int status;

  errno = EPIPE;		/* FIXME: errno should be read-only */
				/* FIXME: remove perrors */

  set_next_block_after (current_header);
  decode_header (current_header, &current_stat, &current_format, 1);

  /* Print the block from `current_header' and `current_stat'.  */

  if (verbose_option)
    {
      if (now_verifying)
	fprintf (stdlis, _("Verify "));
      print_header ();
    }

  switch (current_header->header.typeflag)
    {
    default:
      WARN ((0, 0, _("Unknown file type '%c' for %s, diffed as normal file"),
		 current_header->header.typeflag, current_file_name));
      /* Fall through.  */

    case AREGTYPE:
    case REGTYPE:
    case GNUTYPE_SPARSE:
    case CONTTYPE:

      /* Appears to be a file.  See if it's really a directory.  */

      name_length = strlen (current_file_name) - 1;
      if (current_file_name[name_length] == '/')
	goto really_dir;

      if (!get_stat_data (&stat_data))
	{
	  if (current_header->oldgnu_header.isextended)
	    skip_extended_headers ();
	  skip_file (current_stat.st_size);
	  goto quit;
	}

      if (!S_ISREG (stat_data.st_mode))
	{
	  report_difference (_("Not a regular file"));
	  skip_file (current_stat.st_size);
	  goto quit;
	}

      stat_data.st_mode &= MODE_ALL;
      if (stat_data.st_mode != current_stat.st_mode)
	report_difference (_("Mode differs"));

#if !MSDOS
      /* stat() in djgpp's C library gives a constant number of 42 as the
	 uid and gid of a file.  So, comparing an FTP'ed archive just after
	 unpack would fail on MSDOS.  */
      if (stat_data.st_uid != current_stat.st_uid)
	report_difference (_("Uid differs"));
      if (stat_data.st_gid != current_stat.st_gid)
	report_difference (_("Gid differs"));
#endif

      if (stat_data.st_mtime != current_stat.st_mtime)
	report_difference (_("Mod time differs"));
      if (current_header->header.typeflag != GNUTYPE_SPARSE &&
	  stat_data.st_size != current_stat.st_size)
	{
	  report_difference (_("Size differs"));
	  skip_file (current_stat.st_size);
	  goto quit;
	}

      diff_handle = open (current_file_name, O_NDELAY | O_RDONLY | O_BINARY);

      if (diff_handle < 0 && !absolute_names_option)
	{
	  char *tmpbuf = xmalloc (strlen (current_file_name) + 2);

	  *tmpbuf = '/';
	  strcpy (tmpbuf + 1, current_file_name);
	  diff_handle = open (tmpbuf, O_NDELAY | O_RDONLY);
	  free (tmpbuf);
	}
      if (diff_handle < 0)
	{
	  ERROR ((0, errno, _("Cannot open %s"), current_file_name));
	  if (current_header->oldgnu_header.isextended)
	    skip_extended_headers ();
	  skip_file (current_stat.st_size);
	  report_difference (NULL);
	  goto quit;
	}

      /* Need to treat sparse files completely differently here.  */

      if (current_header->header.typeflag == GNUTYPE_SPARSE)
	diff_sparse_files (current_stat.st_size);
      else
	{
	  if (multi_volume_option)
	    {
	      assign_string (&save_name, current_file_name);
	      save_totsize = current_stat.st_size;
	      /* save_sizeleft is set in read_and_process.  */
	    }

	  read_and_process (current_stat.st_size, process_rawdata);

	  if (multi_volume_option)
	    assign_string (&save_name, NULL);
	}

      status = close (diff_handle);
      if (status < 0)
	ERROR ((0, errno, _("Error while closing %s"), current_file_name));

    quit:
      break;

#if !MSDOS
    case LNKTYPE:
      {
	dev_t dev;
	ino_t ino;

	if (!get_stat_data (&stat_data))
	  break;

	dev = stat_data.st_dev;
	ino = stat_data.st_ino;
	status = stat (current_link_name, &stat_data);
	if (status < 0)
	  {
	    if (errno == ENOENT)
	      report_difference (_("Does not exist"));
	    else
	      {
		WARN ((0, errno, _("Cannot stat file %s"), current_file_name));
		report_difference (NULL);
	      }
	    break;
	  }

	if (stat_data.st_dev != dev || stat_data.st_ino != ino)
	  {
	    char *message = (char *)
	      xmalloc (MESSAGE_BUFFER_SIZE + strlen (current_link_name));

	    sprintf (message, _("Not linked to %s"), current_link_name);
	    report_difference (message);
	    free (message);
	    break;
	  }

	break;
      }
#endif /* not MSDOS */

#ifdef S_ISLNK
    case SYMTYPE:
      {
	char linkbuf[NAME_FIELD_SIZE + 3]; /* FIXME: may be too short.  */

	status = readlink (current_file_name, linkbuf, (sizeof linkbuf) - 1);

	if (status < 0)
	  {
	    if (errno == ENOENT)
	      report_difference (_("No such file or directory"));
	    else
	      {
		WARN ((0, errno, _("Cannot read link %s"), current_file_name));
		report_difference (NULL);
	      }
	    break;
	  }

	linkbuf[status] = '\0';	/* null-terminate it */
	if (strncmp (current_link_name, linkbuf, (size_t) status) != 0)
	  report_difference (_("Symlink differs"));

	break;
      }
#endif /* not S_ISLNK */

#ifdef S_IFCHR
    case CHRTYPE:
      current_stat.st_mode |= S_IFCHR;
      goto check_node;
#endif /* not S_IFCHR */

#ifdef S_IFBLK
      /* If local system doesn't support block devices, use default case.  */

    case BLKTYPE:
      current_stat.st_mode |= S_IFBLK;
      goto check_node;
#endif /* not S_IFBLK */

#ifdef S_ISFIFO
      /* If local system doesn't support FIFOs, use default case.  */

    case FIFOTYPE:
# ifdef S_IFIFO
      current_stat.st_mode |= S_IFIFO;
# endif
      current_stat.st_rdev = 0;	/* FIXME: do we need this? */
      goto check_node;
#endif /* S_ISFIFO */

    check_node:
      /* FIXME: deal with umask.  */

      if (!get_stat_data (&stat_data))
	break;

      if (current_stat.st_rdev != stat_data.st_rdev)
	{
	  report_difference (_("Device numbers changed"));
	  break;
	}

      if (
#ifdef S_IFMT
	  current_stat.st_mode != stat_data.st_mode
#else
	  /* POSIX lossage.  */
	  ((current_stat.st_mode & MODE_ALL)
	   != (stat_data.st_mode & MODE_ALL))
#endif
	  )
	{
	  report_difference (_("Mode or device-type changed"));
	  break;
	}

      break;

    case GNUTYPE_DUMPDIR:
      {
	char *dumpdir_buffer = get_directory_contents (current_file_name,
						       (dev_t) 0);

	if (multi_volume_option)
	  {
	    assign_string (&save_name, current_file_name);
	    save_totsize = current_stat.st_size;
	    /* save_sizeleft is set in read_and_process.  */
	  }

	if (dumpdir_buffer)
	  {
	    dumpdir_cursor = dumpdir_buffer;
	    read_and_process (current_stat.st_size, process_dumpdir);
	    free (dumpdir_buffer);
	  }
	else
	  read_and_process (current_stat.st_size, process_noop);

	if (multi_volume_option)
	  assign_string (&save_name, NULL);
	/* Fall through.  */
      }

    case DIRTYPE:
      /* Check for trailing /.  */

      name_length = strlen (current_file_name) - 1;

    really_dir:
      while (name_length && current_file_name[name_length] == '/')
	current_file_name[name_length--] = '\0';	/* zap / */

      if (!get_stat_data (&stat_data))
	break;

      if (!S_ISDIR (stat_data.st_mode))
	{
	  report_difference (_("No longer a directory"));
	  break;
	}

      if ((stat_data.st_mode & MODE_ALL) != (current_stat.st_mode & MODE_ALL))
	report_difference (_("Mode differs"));
      break;

    case GNUTYPE_VOLHDR:
      break;

    case GNUTYPE_MULTIVOL:
      {
	off_t offset;

	name_length = strlen (current_file_name) - 1;
	if (current_file_name[name_length] == '/')
	  goto really_dir;

	if (!get_stat_data (&stat_data))
	  break;

	if (!S_ISREG (stat_data.st_mode))
	  {
	    report_difference (_("Not a regular file"));
	    skip_file (current_stat.st_size);
	    break;
	  }

	stat_data.st_mode &= MODE_ALL;
	offset = OFF_FROM_OCT (current_header->oldgnu_header.offset);
	if (stat_data.st_size != current_stat.st_size + offset)
	  {
	    report_difference (_("Size differs"));
	    skip_file (current_stat.st_size);
	    break;
	  }

	diff_handle = open (current_file_name, O_NDELAY | O_RDONLY | O_BINARY);

	if (diff_handle < 0)
	  {
	    WARN ((0, errno, _("Cannot open file %s"), current_file_name));
	    report_difference (NULL);
	    skip_file (current_stat.st_size);
	    break;
	  }

	if (lseek (diff_handle, offset, SEEK_SET) < 0)
	  {
	    char buf[UINTMAX_STRSIZE_BOUND];
	    WARN ((0, errno, _("Cannot seek to %s in file %s"),
		   STRINGIFY_BIGINT (offset, buf), current_file_name));
	    report_difference (NULL);
	    break;
	  }

	if (multi_volume_option)
	  {
	    assign_string (&save_name, current_file_name);
	    save_totsize = stat_data.st_size;
	    /* save_sizeleft is set in read_and_process.  */
	  }

	read_and_process (current_stat.st_size, process_rawdata);

	if (multi_volume_option)
	  assign_string (&save_name, NULL);

	status = close (diff_handle);
	if (status < 0)
	  ERROR ((0, errno, _("Error while closing %s"), current_file_name));

	break;
      }
    }
}

/*---.
| ?  |
`---*/

void
verify_volume (void)
{
  if (!diff_buffer)
    diff_init ();

  /* Verifying an archive is meant to check if the physical media got it
     correctly, so try to defeat clever in-memory buffering pertaining to
     this particular media.  On Linux, for example, the floppy drive would
     not even be accessed for the whole verification.

     The code was using fsync only when the ioctl is unavailable, but
     Marty Leisner says that the ioctl does not work when not preceded by
     fsync.  So, until we know better, or maybe to please Marty, let's do it
     the unbelievable way :-).  */

#if HAVE_FSYNC
  fsync (archive);
#endif
#ifdef FDFLUSH
  ioctl (archive, FDFLUSH);
#endif

#ifdef MTIOCTOP
  {
    struct mtop operation;
    int status;

    operation.mt_op = MTBSF;
    operation.mt_count = 1;
    if (status = rmtioctl (archive, MTIOCTOP, (char *) &operation), status < 0)
      {
	if (errno != EIO
	    || (status = rmtioctl (archive, MTIOCTOP, (char *) &operation),
		status < 0))
	  {
#endif
	    if (rmtlseek (archive, (off_t) 0, SEEK_SET) != 0)
	      {
		/* Lseek failed.  Try a different method.  */

		WARN ((0, errno,
		       _("Could not rewind archive file for verify")));
		return;
	      }
#ifdef MTIOCTOP
	  }
      }
  }
#endif

  access_mode = ACCESS_READ;
  now_verifying = 1;

  flush_read ();
  while (1)
    {
      enum read_header status = read_header ();

      if (status == HEADER_FAILURE)
	{
	  int counter = 0;

	  while (status == HEADER_FAILURE);
	    {
	      counter++;
	      status = read_header ();
	    }
	  ERROR ((0, 0,
		  _("VERIFY FAILURE: %d invalid header(s) detected"), counter));
	}
      if (status == HEADER_ZERO_BLOCK || status == HEADER_END_OF_FILE)
	break;

      diff_archive ();
    }

  access_mode = ACCESS_WRITE;
  now_verifying = 0;
}
