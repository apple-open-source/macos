/* Extract files from a tar archive.
   Copyright (C) 1988, 92,93,94,96,97,98, 1999 Free Software Foundation, Inc.
   Written by John Gilmore, on 1985-11-19.

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

static time_t now;		/* current time */
static int we_are_root;		/* true if our effective uid == 0 */
static mode_t newdir_umask;	/* umask when creating new directories */
static mode_t current_umask;	/* current umask (which is set to 0 if -p) */

#if 0
/* "Scratch" space to store the information about a sparse file before
   writing the info into the header or extended header.  */
struct sp_array *sparsearray;

/* Number of elts storable in the sparsearray.  */
int   sp_array_size = 10;
#endif

struct delayed_set_stat
  {
    struct delayed_set_stat *next;
    char *file_name;
    struct stat stat_info;
  };

static struct delayed_set_stat *delayed_set_stat_head;

/*--------------------------.
| Set up to extract files.  |
`--------------------------*/

void
extr_init (void)
{
  now = time ((time_t *) 0);
  we_are_root = geteuid () == 0;

  /* Option -p clears the kernel umask, so it does not affect proper
     restoration of file permissions.  New intermediate directories will
     comply with umask at start of program.  */

  newdir_umask = umask (0);
  if (same_permissions_option)
    current_umask = 0;
  else
    {
      umask (newdir_umask);	/* restore the kernel umask */
      current_umask = newdir_umask;
    }

  /* FIXME: Just make sure we can add files in directories we create.  Maybe
     should we later remove permissions we are adding, here?  */
  newdir_umask &= ~ MODE_WXUSR;
}

/*------------------------------------------------------------------.
| Restore mode for FILE_NAME, from information given in STAT_INFO.  |
`------------------------------------------------------------------*/

static void
set_mode (char *file_name, struct stat *stat_info)
{
  /* We ought to force permission when -k is not selected, because if the
     file already existed, open or creat would save the permission bits from
     the previously created file, ignoring the ones we specified.

     But with -k selected, we know *we* created this file, so the mode
     bits were set by our open.  If the file has abnormal mode bits, we must
     chmod since writing or chown has probably reset them.  If the file is
     normal, we merely skip the chmod.  This works because we did umask (0)
     when -p, so umask will have left the specified mode alone.  */

  if (!keep_old_files_option
      || (stat_info->st_mode & (S_ISUID | S_ISGID | S_ISVTX)))
    if (chmod (file_name, ~current_umask & stat_info->st_mode) < 0)
      ERROR ((0, errno, _("%s: Cannot change mode to %04lo"),
	      file_name,
	      (unsigned long) (~current_umask & stat_info->st_mode)));
}

/*----------------------------------------------------------------------.
| Restore stat attributes (owner, group, mode and times) for FILE_NAME, |
| using information given in STAT_INFO.  SYMLINK_FLAG is non-zero for a |
| freshly restored symbolic link.				        |
`----------------------------------------------------------------------*/

/* FIXME: About proper restoration of symbolic link attributes, we still do
   not have it right.  Pretesters' reports tell us we need further study and
   probably more configuration.  For now, just use lchown if it exists, and
   punt for the rest.  Sigh!  */

static void
set_stat (char *file_name, struct stat *stat_info, int symlink_flag)
{
  struct utimbuf utimbuf;

  if (!symlink_flag)
    {
      /* We do the utime before the chmod because some versions of utime are
	 broken and trash the modes of the file.  */

      if (!touch_option)
	{
	  /* We set the accessed time to `now', which is really the time we
	     started extracting files, unless incremental_option is used, in
	     which case .st_atime is used.  */

	  /* FIXME: incremental_option should set ctime too, but how?  */

	  if (incremental_option)
	    utimbuf.actime = stat_info->st_atime;
	  else
	    utimbuf.actime = now;

	  utimbuf.modtime = stat_info->st_mtime;

	  if (utime (file_name, &utimbuf) < 0)
	    ERROR ((0, errno,
		    _("%s: Could not change access and modification times"),
		    file_name));
	}

      /* Some systems allow non-root users to give files away.  Once this
	 done, it is not possible anymore to change file permissions, so we
	 have to set permissions prior to possibly giving files away.  */

      set_mode (file_name, stat_info);
    }

  /* If we are root, set the owner and group of the extracted file, so we
     extract as the original owner.  Or else, if we are running as a user,
     leave the owner and group as they are, so we extract as that user.  */

  if (we_are_root || same_owner_option)
    {
#if HAVE_LCHOWN

      /* When lchown exists, it should be used to change the attributes of
	 the symbolic link itself.  In this case, a mere chown would change
	 the attributes of the file the symbolic link is pointing to, and
	 should be avoided.  */

      if (symlink_flag)
	{
	  if (lchown (file_name, stat_info->st_uid, stat_info->st_gid) < 0)
	    ERROR ((0, errno, _("%s: Cannot lchown to uid %lu gid %lu"),
		    file_name,
		    (unsigned long) stat_info->st_uid,
		    (unsigned long) stat_info->st_gid));
	}
      else
	{
	  if (chown (file_name, stat_info->st_uid, stat_info->st_gid) < 0)
	    ERROR ((0, errno, _("%s: Cannot chown to uid %lu gid %lu"),
		    file_name,
		    (unsigned long) stat_info->st_uid,
		    (unsigned long) stat_info->st_gid));
	}

#else /* not HAVE_LCHOWN */

      if (!symlink_flag)

	if (chown (file_name, stat_info->st_uid, stat_info->st_gid) < 0)
	  ERROR ((0, errno, _("%s: Cannot chown to uid %lu gid %lu"),
		  file_name,
		  (unsigned long) stat_info->st_uid,
		  (unsigned long) stat_info->st_gid));

#endif/* not HAVE_LCHOWN */

      if (!symlink_flag)

	/* On a few systems, and in particular, those allowing to give files
	   away, changing the owner or group destroys the suid or sgid bits.
	   So let's attempt setting these bits once more.  */

	if (stat_info->st_mode & (S_ISUID | S_ISGID | S_ISVTX))
	  set_mode (file_name, stat_info);
    }
}

/*-----------------------------------------------------------------------.
| After a file/link/symlink/directory creation has failed, see if it's	 |
| because some required directory was not present, and if so, create all |
| required directories.  Return non-zero if a directory was created.	 |
`-----------------------------------------------------------------------*/

static int
make_directories (char *file_name)
{
  char *cursor;			/* points into path */
  int did_something = 0;	/* did we do anything yet? */
  int saved_errno = errno;	/* remember caller's errno */
  int status;

  for (cursor = strchr (file_name, '/');
       cursor != NULL;
       cursor = strchr (cursor + 1, '/'))
    {
      /* Avoid mkdir of empty string, if leading or double '/'.  */

      if (cursor == file_name || cursor[-1] == '/')
	continue;

      /* Avoid mkdir where last part of path is '.'.  */

      if (cursor[-1] == '.' && (cursor == file_name + 1 || cursor[-2] == '/'))
	continue;

      *cursor = '\0';		/* truncate the path there */
      status = mkdir (file_name, ~newdir_umask & MODE_RWX);

      if (status == 0)
	{
	  /* Fix ownership.  */

	  if (we_are_root)
	    if (chown (file_name, current_stat.st_uid, current_stat.st_gid) < 0)
	      ERROR ((0, errno,
		      _("%s: Cannot change owner to uid %lu, gid %lu"),
		      file_name,
		      (unsigned long) current_stat.st_uid,
		      (unsigned long) current_stat.st_gid));

	  print_for_mkdir (file_name, cursor - file_name,
			   ~newdir_umask & MODE_RWX);
	  did_something = 1;

	  *cursor = '/';
	  continue;
	}

      *cursor = '/';

      if (errno == EEXIST
#if MSDOS
	  /* Turbo C mkdir gives a funny errno.  */
	  || errno == EACCES
#endif
	  )
	/* Directory already exists.  */
	continue;

      /* Some other error in the mkdir.  We return to the caller.  */
      break;
    }

  errno = saved_errno;		/* FIXME: errno should be read-only */
  return did_something;		/* tell them to retry if we made one */
}

/*--------------------------------------------------------------------.
| Attempt repairing what went wrong with the extraction.  Delete an   |
| already existing file or create missing intermediate directories.   |
| Return nonzero if we somewhat increased our chances at a successful |
| extraction.  errno is properly restored on zero return.	      |
`--------------------------------------------------------------------*/

static int
maybe_recoverable (char *file_name)
{
  switch (errno)
    {
    case EEXIST:
      /* Attempt deleting an existing file.  However, with -k, just stay
	 quiet.  */

      if (keep_old_files_option)
	return 0;

      return remove_any_file (file_name, 0);

    case ENOENT:
      /* Attempt creating missing intermediate directories.  */

      return make_directories (file_name);

    default:
      /* Just say we can't do anything about it...  */

      return 0;
    }
}

/*---.
| ?  |
`---*/

static void
extract_sparse_file (int fd, off_t *sizeleft, off_t totalsize, char *name)
{
  int sparse_ind = 0;
  size_t written;
  ssize_t count;

  /* assuming sizeleft is initially totalsize */

  while (*sizeleft > 0)
    {
      union block *data_block = find_next_block ();
      if (data_block == NULL)
	{
	  ERROR ((0, 0, _("Unexpected EOF on archive file")));
	  return;
	}
      if (lseek (fd, sparsearray[sparse_ind].offset, SEEK_SET) < 0)
	{
	  char buf[UINTMAX_STRSIZE_BOUND];
	  ERROR ((0, errno, _("%s: lseek error at byte %s"),
		  STRINGIFY_BIGINT (sparsearray[sparse_ind].offset, buf),
		  name));
	  return;
	}
      written = sparsearray[sparse_ind++].numbytes;
      while (written > BLOCKSIZE)
	{
	  count = full_write (fd, data_block->buffer, BLOCKSIZE);
	  if (count < 0)
	    ERROR ((0, errno, _("%s: Could not write to file"), name));
	  written -= count;
	  *sizeleft -= count;
	  set_next_block_after (data_block);
	  data_block = find_next_block ();
	}

      count = full_write (fd, data_block->buffer, written);

      if (count < 0)
	ERROR ((0, errno, _("%s: Could not write to file"), name));
      else if (count != written)
	{
	  char buf1[UINTMAX_STRSIZE_BOUND];
	  char buf2[UINTMAX_STRSIZE_BOUND];
	  ERROR ((0, 0, _("%s: Could only write %s of %s bytes"),
		  name,
		  STRINGIFY_BIGINT (totalsize - *sizeleft, buf1),
		  STRINGIFY_BIGINT (totalsize, buf2)));
	  skip_file (*sizeleft);
	}

      written -= count;
      *sizeleft -= count;
      set_next_block_after (data_block);
    }

  free (sparsearray);
}

/*----------------------------------.
| Extract a file from the archive.  |
`----------------------------------*/

void
extract_archive (void)
{
  union block *data_block;
  int fd;
  int status;
  ssize_t sstatus;
  size_t name_length;
  size_t written;
  int openflag;
  off_t size;
  int skipcrud;
  int counter;
  char typeflag;
#if 0
  int sparse_ind = 0;
#endif
  union block *exhdr;
  struct delayed_set_stat *data;

#define CURRENT_FILE_NAME (skipcrud + current_file_name)

  set_next_block_after (current_header);
  decode_header (current_header, &current_stat, &current_format, 1);

  if (interactive_option && !confirm ("extract", current_file_name))
    {
      if (current_header->oldgnu_header.isextended)
	skip_extended_headers ();
      skip_file (current_stat.st_size);
      return;
    }

  /* Print the block from `current_header' and `current_stat'.  */

  if (verbose_option)
    print_header ();

  /* Check for fully specified file names and other atrocities.  */

  skipcrud = 0;
  while (!absolute_names_option && CURRENT_FILE_NAME[0] == '/')
    {
      static int warned_once = 0;

      skipcrud++;		/* force relative path */
      if (!warned_once)
	{
	  warned_once = 1;
	  WARN ((0, 0, _("\
Removing leading `/' from absolute path names in the archive")));
	}
    }

  /* Take a safety backup of a previously existing file.  */

  if (backup_option && !to_stdout_option)
    if (!maybe_backup_file (CURRENT_FILE_NAME, 0))
      {
	ERROR ((0, errno, _("%s: Was unable to backup this file"),
		CURRENT_FILE_NAME));
	if (current_header->oldgnu_header.isextended)
	  skip_extended_headers ();
	skip_file (current_stat.st_size);
	return;
      }

  /* Extract the archive entry according to its type.  */

  typeflag = current_header->header.typeflag;
  switch (typeflag)
    {
      /* JK - What we want to do if the file is sparse is loop through
	 the array of sparse structures in the header and read in and
	 translate the character strings representing 1) the offset at
	 which to write and 2) how many bytes to write into numbers,
	 which we store into the scratch array, "sparsearray".  This
	 array makes our life easier the same way it did in creating the
	 tar file that had to deal with a sparse file.

	 After we read in the first five (at most) sparse structures, we
	 check to see if the file has an extended header, i.e., if more
	 sparse structures are needed to describe the contents of the new
	 file.  If so, we read in the extended headers and continue to
	 store their contents into the sparsearray.  */

    case GNUTYPE_SPARSE:
      sp_array_size = 10;
      sparsearray = (struct sp_array *)
	xmalloc (sp_array_size * sizeof (struct sp_array));

      for (counter = 0; counter < SPARSES_IN_OLDGNU_HEADER; counter++)
	{
	  sparsearray[counter].offset =
	    OFF_FROM_OCT (current_header->oldgnu_header.sp[counter].offset);
	  sparsearray[counter].numbytes =
	    SIZE_FROM_OCT (current_header->oldgnu_header.sp[counter].numbytes);
	  if (!sparsearray[counter].numbytes)
	    break;
	}

      if (current_header->oldgnu_header.isextended)
	{
	  /* Read in the list of extended headers and translate them
	     into the sparsearray as before.  Note that this
	     invalidates current_header.  */

	  /* static */ int ind = SPARSES_IN_OLDGNU_HEADER;

	  while (1)
	    {
	      exhdr = find_next_block ();
	      for (counter = 0; counter < SPARSES_IN_SPARSE_HEADER; counter++)
		{
		  if (counter + ind > sp_array_size - 1)
		    {
		      /* Realloc the scratch area since we've run out of
			 room.  */

		      sp_array_size *= 2;
		      sparsearray = (struct sp_array *)
			xrealloc (sparsearray,
				  sp_array_size * (sizeof (struct sp_array)));
		    }
		  /* Compare to 0, or use !(int)..., for Pyramid's dumb
		     compiler.  */
		  if (exhdr->sparse_header.sp[counter].numbytes == 0)
		    break;
		  sparsearray[counter + ind].offset =
		    OFF_FROM_OCT (exhdr->sparse_header.sp[counter].offset);
		  sparsearray[counter + ind].numbytes =
		    SIZE_FROM_OCT (exhdr->sparse_header.sp[counter].numbytes);
		}
	      if (!exhdr->sparse_header.isextended)
		break;
	      else
		{
		  ind += SPARSES_IN_SPARSE_HEADER;
		  set_next_block_after (exhdr);
		}
	    }
	  set_next_block_after (exhdr);
	}
      /* Fall through.  */

    case AREGTYPE:
    case REGTYPE:
    case CONTTYPE:

      /* Appears to be a file.  But BSD tar uses the convention that a slash
	 suffix means a directory.  */

      name_length = strlen (CURRENT_FILE_NAME) - 1;
      if (CURRENT_FILE_NAME[name_length] == '/')
	goto really_dir;

      /* FIXME: deal with protection issues.  */

    again_file:
      openflag = (keep_old_files_option ?
		  O_BINARY | O_NDELAY | O_WRONLY | O_CREAT | O_EXCL :
		  O_BINARY | O_NDELAY | O_WRONLY | O_CREAT | O_TRUNC)
	| ((typeflag == GNUTYPE_SPARSE) ? 0 : O_APPEND);

      /* JK - The last | is a kludge to solve the problem the O_APPEND
	 flag causes with files we are trying to make sparse: when a file
	 is opened with O_APPEND, it writes to the last place that
	 something was written, thereby ignoring any lseeks that we have
	 done.  We add this extra condition to make it able to lseek when
	 a file is sparse, i.e., we don't open the new file with this
	 flag.  (Grump -- this bug caused me to waste a good deal of
	 time, I might add)  */

      if (to_stdout_option)
	{
	  fd = 1;
	  goto extract_file;
	}

      if (unlink_first_option)
	remove_any_file (CURRENT_FILE_NAME, recursive_unlink_option);

#if O_CTG
      /* Contiguous files (on the Masscomp) have to specify the size in
	 the open call that creates them.  */

      if (typeflag == CONTTYPE)
	fd = open (CURRENT_FILE_NAME, openflag | O_CTG,
		   current_stat.st_mode, current_stat.st_size);
      else
	fd = open (CURRENT_FILE_NAME, openflag, current_stat.st_mode);

#else /* not O_CTG */
      if (typeflag == CONTTYPE)
	{
	  static int conttype_diagnosed = 0;

	  if (!conttype_diagnosed)
	    {
	      conttype_diagnosed = 1;
	      WARN ((0, 0, _("Extracting contiguous files as regular files")));
	    }
	}
      fd = open (CURRENT_FILE_NAME, openflag, current_stat.st_mode);

#endif /* not O_CTG */

      if (fd < 0)
	{
	  if (maybe_recoverable (CURRENT_FILE_NAME))
	    goto again_file;

	  ERROR ((0, errno, _("%s: Could not create file"),
		  CURRENT_FILE_NAME));
	  if (current_header->oldgnu_header.isextended)
	    skip_extended_headers ();
	  skip_file (current_stat.st_size);
	  if (backup_option)
	    undo_last_backup ();
	  break;
	}

    extract_file:
      if (typeflag == GNUTYPE_SPARSE)
	{
	  char *name;
	  size_t name_length_bis;

	  /* Kludge alert.  NAME is assigned to header.name because
	     during the extraction, the space that contains the header
	     will get scribbled on, and the name will get munged, so any
	     error messages that happen to contain the filename will look
	     REAL interesting unless we do this.  */

	  name_length_bis = strlen (CURRENT_FILE_NAME) + 1;
	  name = (char *) xmalloc (name_length_bis);
	  memcpy (name, CURRENT_FILE_NAME, name_length_bis);
	  size = current_stat.st_size;
	  extract_sparse_file (fd, &size, current_stat.st_size, name);
	}
      else
	for (size = current_stat.st_size;
	     size > 0;
	     size -= written)
	  {
	    if (multi_volume_option)
	      {
		assign_string (&save_name, current_file_name);
		save_totsize = current_stat.st_size;
		save_sizeleft = size;
	      }

	    /* Locate data, determine max length writeable, write it,
	       block that we have used the data, then check if the write
	       worked.  */

	    data_block = find_next_block ();
	    if (data_block == NULL)
	      {
		ERROR ((0, 0, _("Unexpected EOF on archive file")));
		break;		/* FIXME: What happens, then?  */
	      }

	    written = available_space_after (data_block);

	    if (written > size)
	      written = size;
	    errno = 0;		/* FIXME: errno should be read-only */
	    sstatus = full_write (fd, data_block->buffer, written);

	    set_next_block_after ((union block *)
				  (data_block->buffer + written - 1));
	    if (sstatus == written)
	      continue;

	    /* Error in writing to file.  Print it, skip to next file in
	       archive.  */

	    if (sstatus < 0)
	      ERROR ((0, errno, _("%s: Could not write to file"),
		      CURRENT_FILE_NAME));
	    else
	      ERROR ((0, 0, _("%s: Could only write %lu of %lu bytes"),
		      CURRENT_FILE_NAME,
		      (unsigned long) sstatus,
		      (unsigned long) written));
	    skip_file (size - written);
	    break;		/* still do the close, mod time, chmod, etc */
	  }

      if (multi_volume_option)
	assign_string (&save_name, NULL);

      /* If writing to stdout, don't try to do anything to the filename;
	 it doesn't exist, or we don't want to touch it anyway.  */

      if (to_stdout_option)
	break;

      status = close (fd);
      if (status < 0)
	{
	  ERROR ((0, errno, _("%s: Error while closing"), CURRENT_FILE_NAME));
	  if (backup_option)
	    undo_last_backup ();
	}

      set_stat (CURRENT_FILE_NAME, &current_stat, 0);
      break;

    case SYMTYPE:
      if (to_stdout_option)
	break;

#ifdef S_ISLNK
      if (unlink_first_option)
	remove_any_file (CURRENT_FILE_NAME, recursive_unlink_option);

      while (status = symlink (current_link_name, CURRENT_FILE_NAME),
	     status != 0)
	if (!maybe_recoverable (CURRENT_FILE_NAME))
	  break;

      if (status == 0)

	/* Setting the attributes of symbolic links might, on some systems,
	   change the pointed to file, instead of the symbolic link itself.
	   At least some of these systems have a lchown call, and the
	   set_stat routine knows about this.    */

	set_stat (CURRENT_FILE_NAME, &current_stat, 1);

      else
	{
	  ERROR ((0, errno, _("%s: Could not create symlink to `%s'"),
		  CURRENT_FILE_NAME, current_link_name));
	  if (backup_option)
	    undo_last_backup ();
	}
      break;

#else /* not S_ISLNK */
      {
	static int warned_once = 0;

	if (!warned_once)
	  {
	    warned_once = 1;
	    WARN ((0, 0, _("\
Attempting extraction of symbolic links as hard links")));
	  }
      }
      /* Fall through.  */

#endif /* not S_ISLNK */

    case LNKTYPE:
      if (to_stdout_option)
	break;

      if (unlink_first_option)
	remove_any_file (CURRENT_FILE_NAME, recursive_unlink_option);

    again_link:
      {
	struct stat st1, st2;

	/* MSDOS does not implement links.  However, djgpp's link() actually
	   copies the file.  */
	status = link (current_link_name, CURRENT_FILE_NAME);

	if (status == 0)
	  break;
	if (maybe_recoverable (CURRENT_FILE_NAME))
	  goto again_link;

	if (incremental_option && errno == EEXIST)
	  break;
	if (stat (current_link_name, &st1) == 0
	    && stat (CURRENT_FILE_NAME, &st2) == 0
	    && st1.st_dev == st2.st_dev
	    && st1.st_ino == st2.st_ino)
	  break;

	ERROR ((0, errno, _("%s: Could not link to `%s'"),
		CURRENT_FILE_NAME, current_link_name));
	if (backup_option)
	  undo_last_backup ();
      }
      break;

#if S_IFCHR
    case CHRTYPE:
      current_stat.st_mode |= S_IFCHR;
      goto make_node;
#endif

#if S_IFBLK
    case BLKTYPE:
      current_stat.st_mode |= S_IFBLK;
#endif

#if defined(S_IFCHR) || defined(S_IFBLK)
    make_node:
      if (to_stdout_option)
	break;

      if (unlink_first_option)
	remove_any_file (CURRENT_FILE_NAME, recursive_unlink_option);

      status = mknod (CURRENT_FILE_NAME, current_stat.st_mode,
		      current_stat.st_rdev);
      if (status != 0)
	{
	  if (maybe_recoverable (CURRENT_FILE_NAME))
	    goto make_node;

	  ERROR ((0, errno, _("%s: Could not make node"), CURRENT_FILE_NAME));
	  if (backup_option)
	    undo_last_backup ();
	  break;
	};
      set_stat (CURRENT_FILE_NAME, &current_stat, 0);
      break;
#endif

#ifdef S_ISFIFO
    case FIFOTYPE:
      if (to_stdout_option)
	break;

      if (unlink_first_option)
	remove_any_file (CURRENT_FILE_NAME, recursive_unlink_option);

      while (status = mkfifo (CURRENT_FILE_NAME, current_stat.st_mode),
	     status != 0)
	if (!maybe_recoverable (CURRENT_FILE_NAME))
	  break;

      if (status == 0)
	set_stat (CURRENT_FILE_NAME, &current_stat, 0);
      else
	{
	  ERROR ((0, errno, _("%s: Could not make fifo"), CURRENT_FILE_NAME));
	  if (backup_option)
	    undo_last_backup ();
	}
      break;
#endif

    case DIRTYPE:
    case GNUTYPE_DUMPDIR:
      name_length = strlen (CURRENT_FILE_NAME) - 1;

    really_dir:
      /* Check for trailing /, and zap as many as we find.  */
      while (name_length && CURRENT_FILE_NAME[name_length] == '/')
	CURRENT_FILE_NAME[name_length--] = '\0';

      if (incremental_option)
	{
	  /* Read the entry and delete files that aren't listed in the
	     archive.  */

	  gnu_restore (skipcrud);
	}
      else if (typeflag == GNUTYPE_DUMPDIR)
	skip_file (current_stat.st_size);

      if (to_stdout_option)
	break;

    again_dir:
      status = mkdir (CURRENT_FILE_NAME,
		      ((we_are_root ? 0 : MODE_WXUSR)
		       | current_stat.st_mode));
      if (status != 0)
	{
	  /* If the directory creation fails, let's consider immediately the
	     case where the directory already exists.  We have three good
	     reasons for clearing out this case before attempting recovery.

	     1) It would not be efficient recovering the error by deleting
	     the directory in maybe_recoverable, then recreating it right
	     away.  We only hope we will be able to adjust its permissions
	     adequately, later.

	     2) Removing the directory might fail if it is not empty.  By
	     exception, this real error is traditionally not reported.

	     3) Let's suppose `DIR' already exists and we are about to
	     extract `DIR/../DIR'.  This would fail because the directory
	     already exists, and maybe_recoverable would react by removing
	     `DIR'.  This then would fail again because there are missing
	     intermediate directories, and maybe_recoverable would react by
	     creating `DIR'.  We would then have an extraction loop.  */

	  if (errno == EEXIST)
	    {
	      struct stat st1;
	      int saved_errno = errno;

	      if (stat (CURRENT_FILE_NAME, &st1) == 0 && S_ISDIR (st1.st_mode))
		goto check_perms;

	      errno = saved_errno; /* FIXME: errno should be read-only */
	    }

	  if (maybe_recoverable (CURRENT_FILE_NAME))
	    goto again_dir;

	  /* If we're trying to create '.', let it be.  */

	  /* FIXME: Strange style...  */

	  if (CURRENT_FILE_NAME[name_length] == '.'
	      && (name_length == 0
		  || CURRENT_FILE_NAME[name_length - 1] == '/'))
	    goto check_perms;

	  ERROR ((0, errno, _("%s: Could not create directory"),
		  CURRENT_FILE_NAME));
	  if (backup_option)
	    undo_last_backup ();
	  break;
	}

    check_perms:
      if (!we_are_root && MODE_WXUSR != (MODE_WXUSR & current_stat.st_mode))
	{
	  current_stat.st_mode |= MODE_WXUSR;
	  WARN ((0, 0, _("Added write and execute permission to directory %s"),
		 CURRENT_FILE_NAME));
	}

#if !MSDOS
      /* MSDOS does not associate timestamps with directories.   In this
	 case, no need to try delaying their restoration.  */

      if (touch_option)

	/* FIXME: I do not believe in this.  Ignoring time stamps does not
	   alleviate the need of delaying the restoration of directories'
	   mode.  Let's ponder this for a little while.  */

	set_mode (CURRENT_FILE_NAME, &current_stat);

      else
	{
	  data = ((struct delayed_set_stat *)
		      xmalloc (sizeof (struct delayed_set_stat)));
	  data->file_name = xstrdup (CURRENT_FILE_NAME);
	  data->stat_info = current_stat;
	  data->next = delayed_set_stat_head;
	  delayed_set_stat_head = data;
	}
#endif /* !MSDOS */
      break;

    case GNUTYPE_VOLHDR:
      if (verbose_option)
	fprintf (stdlis, _("Reading %s\n"), current_file_name);
      break;

    case GNUTYPE_NAMES:
      extract_mangle ();
      break;

    case GNUTYPE_MULTIVOL:
      ERROR ((0, 0, _("\
Cannot extract `%s' -- file is continued from another volume"),
	      current_file_name));
      skip_file (current_stat.st_size);
      if (backup_option)
	undo_last_backup ();
      break;

    case GNUTYPE_LONGNAME:
    case GNUTYPE_LONGLINK:
      ERROR ((0, 0, _("Visible long name error")));
      skip_file (current_stat.st_size);
      if (backup_option)
	undo_last_backup ();
      break;

    default:
      WARN ((0, 0,
	     _("Unknown file type '%c' for %s, extracted as normal file"),
	     typeflag, CURRENT_FILE_NAME));
      goto again_file;
    }

#undef CURRENT_FILE_NAME
}

/*----------------------------------------------------------------.
| Set back the utime and mode for all the extracted directories.  |
`----------------------------------------------------------------*/

void
apply_delayed_set_stat (void)
{
  struct delayed_set_stat *data;

  while (delayed_set_stat_head != NULL)
    {
      data = delayed_set_stat_head;
      delayed_set_stat_head = delayed_set_stat_head->next;
      set_stat (data->file_name, &data->stat_info, 0);
      free (data->file_name);
      free (data);
    }
}
