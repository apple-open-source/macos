/* Delete entries from a tar archive.
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

#include "common.h"
#include "rmt.h"

static union block *new_record = NULL;
static union block *save_record = NULL;
static off_t records_read = 0;
static int new_blocks = 0;
static int blocks_needed = 0;

/* FIXME: This module should not directly handle the following three
   variables, instead, this should be done in buffer.c only.  */
extern union block *record_start;
extern union block *record_end;
extern union block *current_block;

/*-------------------------------------------------------------------------.
| Move archive descriptor by COUNT records worth.  If COUNT is positive we |
| move forward, else we move negative.  If its a tape, MTIOCTOP had better |
| work.  If its something else, we try to seek on it.  If we can't seek,   |
| we loose!								   |
`-------------------------------------------------------------------------*/

static void
move_archive (off_t count)
{
#ifdef MTIOCTOP
  {
    struct mtop operation;
    int status;

    if (count > 0)
      {
	operation.mt_op = MTFSR;
	operation.mt_count = count;
	if (operation.mt_count != count)
	  FATAL_ERROR ((0, 0, _("Could not re-position archive file")));
      }
    else
      {
	operation.mt_op = MTBSR;
	operation.mt_count = -count;
	if (operation.mt_count != -count)
	  FATAL_ERROR ((0, 0, _("Could not re-position archive file")));
      }

    if (status = rmtioctl (archive, MTIOCTOP, (char *) &operation),
	status >= 0)
      return;

    if (errno == EIO)
      if (status = rmtioctl (archive, MTIOCTOP, (char *) &operation),
	  status >= 0)
      return;
  }
#endif /* MTIOCTOP */

  {
    off_t position0 = rmtlseek (archive, (off_t) 0, SEEK_CUR);
    off_t increment = record_size * (off_t) count;
    off_t position = position0 + increment;

    if (increment / count != record_size
	|| (position < position0) != (increment < 0)
	|| rmtlseek (archive, position, SEEK_SET) != position)
      FATAL_ERROR ((0, 0, _("Could not re-position archive file")));

    return;
  }
}

/*----------------------------------------------------------------.
| Write out the record which has been filled.  If MOVE_BACK_FLAG, |
| backspace to where we started.                                  |
`----------------------------------------------------------------*/

static void
write_record (int move_back_flag)
{
  save_record = record_start;
  record_start = new_record;

  if (archive == STDIN_FILENO)
    {
      archive = STDOUT_FILENO;
      flush_write ();
      archive = STDIN_FILENO;
    }
  else
    {
      move_archive (-(records_read + 1));
      flush_write ();
    }

  record_start = save_record;

  if (move_back_flag)
    {
      /* Move the tape head back to where we were.  */

      if (archive != STDIN_FILENO)
	move_archive (records_read);

      records_read--;
    }

  blocks_needed = blocking_factor;
  new_blocks = 0;
}

/*---.
| ?  |
`---*/

void
delete_archive_members (void)
{
  enum read_header logical_status = HEADER_STILL_UNREAD;
  enum read_header previous_status = HEADER_STILL_UNREAD;

  /* FIXME: Should clean the routine before cleaning these variables :-( */
  struct name *name;
  off_t blocks_to_skip = 0;
  off_t blocks_to_keep = 0;
  int kept_blocks_in_record;

  name_gather ();
  open_archive (ACCESS_UPDATE);

  while (logical_status == HEADER_STILL_UNREAD)
    {
      enum read_header status = read_header ();

      switch (status)
	{
	case HEADER_STILL_UNREAD:
	  abort ();

	case HEADER_SUCCESS:
	  if (name = name_scan (current_file_name), !name)
	    {
	      set_next_block_after (current_header);
	      if (current_header->oldgnu_header.isextended)
		skip_extended_headers ();
	      skip_file (current_stat.st_size);
	      break;
	    }
	  name->found = 1;
	  logical_status = HEADER_SUCCESS;
	  break;

	case HEADER_ZERO_BLOCK:
	case HEADER_END_OF_FILE:
	  logical_status = HEADER_END_OF_FILE;
	  break;

	case HEADER_FAILURE:
	  set_next_block_after (current_header);
	  switch (previous_status)
	    {
	    case HEADER_STILL_UNREAD:
	      WARN ((0, 0, _("This does not look like a tar archive")));
	      /* Fall through.  */

	    case HEADER_SUCCESS:
	    case HEADER_ZERO_BLOCK:
	      ERROR ((0, 0, _("Skipping to next header")));
	      /* Fall through.  */

	    case HEADER_FAILURE:
	      break;

	    case HEADER_END_OF_FILE:
	      abort ();
	    }
	  break;
	}

      previous_status = status;
    }

  if (logical_status != HEADER_SUCCESS)
    {
      write_eot ();
      close_archive ();
      names_notfound ();
      return;
    }

  write_archive_to_stdout = 0;
  new_record = (union block *) xmalloc (record_size);

  /* Save away blocks before this one in this record.  */

  new_blocks = current_block - record_start;
  blocks_needed = blocking_factor - new_blocks;
  if (new_blocks)
    memcpy ((void *) new_record, (void *) record_start,
	   (size_t) (new_blocks * BLOCKSIZE));

#if 0
  /* FIXME: Old code, before the goto was inserted.  To be redesigned.  */
  set_next_block_after (current_header);
  if (current_header->oldgnu_header.isextended)
    skip_extended_headers ();
  skip_file (current_stat.st_size);
#endif
  logical_status = HEADER_STILL_UNREAD;
  goto flush_file;

  /* FIXME: Solaris 2.4 Sun cc (the ANSI one, not the old K&R) says:
       "delete.c", line 223: warning: loop not entered at top
     Reported by Bruno Haible.  */
  while (1)
    {
      enum read_header status;

      /* Fill in a record.  */

      if (current_block == record_end)
	{
	  flush_archive ();
	  records_read++;
	}
      status = read_header ();

      if (status == HEADER_ZERO_BLOCK && ignore_zeros_option)
	{
	  set_next_block_after (current_header);
	  continue;
	}
      if (status == HEADER_END_OF_FILE || status == HEADER_ZERO_BLOCK)
	{
	  logical_status = HEADER_END_OF_FILE;
	  memset (new_record[new_blocks].buffer, 0,
		 (size_t) (BLOCKSIZE * blocks_needed));
	  new_blocks += blocks_needed;
	  blocks_needed = 0;
	  write_record (0);
	  break;
	}

      if (status == HEADER_FAILURE)
	{
	  ERROR ((0, 0, _("Deleting non-header from archive")));
	  set_next_block_after (current_header);
	  continue;
	}

      /* Found another header.  */

      if (name = name_scan (current_file_name), name)
	{
	  name->found = 1;
	flush_file:
	  set_next_block_after (current_header);
	  blocks_to_skip = (current_stat.st_size + BLOCKSIZE - 1) / BLOCKSIZE;

	  while (record_end - current_block <= blocks_to_skip)
	    {
	      blocks_to_skip -= (record_end - current_block);
	      flush_archive ();
	      records_read++;
	    }
	  current_block += blocks_to_skip;
	  blocks_to_skip = 0;
	  continue;
	}

      /* Copy header.  */

      new_record[new_blocks] = *current_header;
      new_blocks++;
      blocks_needed--;
      blocks_to_keep
	= (current_stat.st_size + BLOCKSIZE - 1) / BLOCKSIZE;
      set_next_block_after (current_header);
      if (blocks_needed == 0)
	write_record (1);

      /* Copy data.  */

      kept_blocks_in_record = record_end - current_block;
      if (kept_blocks_in_record > blocks_to_keep)
	kept_blocks_in_record = blocks_to_keep;

      while (blocks_to_keep)
	{
	  int count;

	  if (current_block == record_end)
	    {
	      flush_read ();
	      records_read++;
	      current_block = record_start;
	      kept_blocks_in_record = blocking_factor;
	      if (kept_blocks_in_record > blocks_to_keep)
		kept_blocks_in_record = blocks_to_keep;
	    }
	  count = kept_blocks_in_record;
	  if (count > blocks_needed)
	    count = blocks_needed;

	  memcpy ((void *) (new_record + new_blocks),
		  (void *) current_block,
		  (size_t) (count * BLOCKSIZE));
	  new_blocks += count;
	  blocks_needed -= count;
	  current_block += count;
	  blocks_to_keep -= count;
	  kept_blocks_in_record -= count;

	  if (blocks_needed == 0)
	    write_record (1);
	}
    }

  write_eot ();
  close_archive ();
  names_notfound ();
}
