/* Mac OS X support for GDB, the GNU debugger.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

   Contributed by Apple Computer, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "inferior.h"
#include "symfile.h"
#include "symtab.h"
#include "gdbcmd.h"
#include "objfiles.h"
#include "target.h"
#include "exceptions.h"
#include "gdbcore.h"

#include <string.h>
#include <sys/stat.h>

#include <mach-o/nlist.h>
#include <mach-o/loader.h>

#include "libbfd.h"

#include "mach-o.h"

#include "macosx-nat-inferior.h"
#include "macosx-nat-mutils.h"
#include "macosx-nat-dyld-info.h"

struct inferior_info
{
  bfd_vma addr;
  bfd_vma offset;
  bfd_vma len;
  void *read;
};

static void *
inferior_open (bfd *abfd, void *open_closure)
{
  struct inferior_info *in = NULL;
  struct inferior_info *ret = NULL;

  in = (struct inferior_info *) open_closure;
  ret = bfd_zalloc (abfd, sizeof (struct inferior_info));

  *ret = *in;

  return ret;
}

/* The target for the inferior_bfd type may "macos-child" in the case
   of a regular process, "remote" in the case of a translated (Rosetta) 
   application where we're controlling it via remote protocol, or
   "core-macho" for core files.  

   Returns zero if the inferior bfd is not supported for the current target
   type, or non-zero if it is supported.  */

static int
valid_target_for_inferior_bfd ()
{
  return strcmp (current_target.to_shortname, "macos-child") == 0
      || target_is_remote ()
      || strcmp (current_target.to_shortname, "core-macho") == 0;
}

/* Return non-zero if the bfd target is mach-o.  */

static int
bfd_target_is_mach_o (bfd *abfd)
{
  return strcmp (bfd_get_target (abfd), "mach-o-be") == 0
      || strcmp (bfd_get_target (abfd), "mach-o-le") == 0
      || strcmp (bfd_get_target (abfd), "mach-o") == 0;
}


size_t
inferior_read_memory_partial (CORE_ADDR addr, int nbytes, gdb_byte *mbuf)
{
  size_t nbytes_read = 0;

  volatile struct gdb_exception except;

  int old_trust_readonly = set_trust_readonly (0);
  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      /* Read as much memory as we can.  */
      nbytes_read = target_read (&current_target, TARGET_OBJECT_MEMORY, NULL,
				 mbuf, addr, nbytes);
    }
  set_trust_readonly (old_trust_readonly);

  if (nbytes_read == 0)
    {
      /* Set the bfd error if we didn't get anything.  */
      bfd_set_error (bfd_error_system_call);
    }
  else if (nbytes_read < nbytes)
    {
      /* Fill any extra bytes that weren't able to be read with zero if we
         were at least able to read some bytes.  */
      memset (mbuf + nbytes_read, 0, nbytes - nbytes_read);
    }
  /* Return the number of bytes read.  */
  return nbytes_read;
}


static file_ptr
inferior_read_generic (bfd *abfd, void *stream, void *data, file_ptr nbytes, file_ptr offset)
{
  struct inferior_info *iptr = (struct inferior_info *) stream;

  CHECK_FATAL (iptr != NULL);

  if (!valid_target_for_inferior_bfd ())
    {
      bfd_set_error (bfd_error_no_contents);
      return 0;
    }

  if (offset > iptr->len)
    {
      bfd_set_error (bfd_error_no_contents);
      return 0;
    }

  return inferior_read_memory_partial (iptr->addr + offset, nbytes, data);
}

static file_ptr
inferior_read_mach_o (bfd *abfd, void *stream, void *data, file_ptr nbytes, file_ptr offset)
{
  struct inferior_info *iptr = (struct inferior_info *) stream;
  unsigned int i;

  CHECK_FATAL (iptr != NULL);

  if (!valid_target_for_inferior_bfd ())
    {
      bfd_set_error (bfd_error_no_contents);
      return 0;
    }

  if (!bfd_target_is_mach_o (abfd))
    {
      bfd_set_error (bfd_error_invalid_target);
      return 0;
    }

  {
    struct mach_o_data_struct *mdata = NULL;
    CHECK_FATAL (bfd_mach_o_valid (abfd));
    mdata = abfd->tdata.mach_o_data;
    if (mdata->scanning_load_cmds == 1)
      {
       bfd_set_error (bfd_error_invalid_target);
       return 0;
      }

    for (i = 0; i < mdata->header.ncmds; i++)
      {
        struct bfd_mach_o_load_command *cmd = &mdata->commands[i];
	/* Break out of the loop if we find any zero load commands
	   since this can happen if we are currently reading the
	   load commands.  */
	if (cmd->type == 0)
	  break;
        if (cmd->type == BFD_MACH_O_LC_SEGMENT
	    || cmd->type == BFD_MACH_O_LC_SEGMENT_64)
          {
            struct bfd_mach_o_segment_command *segment =
              &cmd->command.segment;
            if ((offset  >= segment->fileoff)
                && (offset < (segment->fileoff + segment->filesize)))
              {
                bfd_vma infaddr =
                  (segment->vmaddr + (offset - segment->fileoff));

		/* If the offset (slide) is set to an invalid value, we 
		   need to figure out any rigid slides automatically. We do
		   this by finding the __TEXT segment that gets registered 
		   with the bfd (as a section). The address found in this
		   bfd section can help us to determine a single slide 
		   amount by taking the difference from the address provided.
		   This assumes that the mach header is at the beginning of
		   the __TEXT segment which is currently true for all of our
		   current binaries. 
		   
		   Automatically finding the slide amount is used when we
		   are reading an image from memory when we don't already 
		   know the slide amount. This can occur whilst attaching
		   or when we have a core file and we can only read the load
		   address from the dyld_all_image_infos array. This array
		   contains the mach header address in memory only, and not
		   the original load address contained in the load commands.
		   We normally get the slide amount from dyld when we hit 
		   the breakpoint that we set in dyld where dyld computes 
		   this information for us.
		   
		   For mach images in the shared cache, all the addresses 
		   in the mach load commands will be fixed up and the 
		   the header address will be the same as the __TEXT segment.
		   Non-shared cache mach images have all segments loaded 
		   read only, so the addresses remain as they were on disk. 
		   The only way to know the slide amount is to read the 
		   segment load commands. So instead of having to do a two
		   pass solution where we must first find all of the slide
		   amounts, the offset can be set to INVALID_ADDRESS and we
		   will figure it out as we go.  */

		if (iptr->offset == INVALID_ADDRESS)
		  {
		    /* Figure out the slide amount automatically.  */
		    asection *text_sect;
		    /* See if we have read the text segment yet? */
		    text_sect = bfd_get_section_by_name (abfd, 
							 TEXT_SEGMENT_NAME);
		    if (text_sect)
		      {
			/* The slide amount is the current load mach header
			   address (which is always at the start of the __TEXT
			   segment) minus the __TEXT segment address in the
			   mach load commands in memory.  */
			bfd_vma slide = iptr->addr - 
					bfd_section_vma (abfd, text_sect);
			infaddr = infaddr + slide;
		      }
		  }
		else
		  {
		    /* We were given an offset (slide) when this bfd was 
		       created, so we are going to use that.  */
		    infaddr = infaddr + iptr->offset;
		  }
		
		return inferior_read_memory_partial (infaddr, nbytes, data);
              }
          }
      }
  }

  bfd_set_error (bfd_error_no_contents);
  return 0;
}

static file_ptr
inferior_read (bfd *abfd, void *stream, void *data, file_ptr nbytes, file_ptr offset)
{
  file_ptr bytes_read = 0;
  if (bfd_target_is_mach_o (abfd) && bfd_mach_o_valid (abfd))
    {
      /* Try to read using the mach translation first if we have a mach
         binary.  */
      bytes_read = inferior_read_mach_o (abfd, stream, data, nbytes, offset);
    }

  if (bytes_read == 0)
    bytes_read = inferior_read_generic (abfd, stream, data, nbytes, offset);
  return bytes_read;
}

static int
inferior_close (bfd *abfd, void *stream)
{
  return 0;
}

static bfd *
inferior_bfd_generic (const char *name, CORE_ADDR addr, CORE_ADDR offset, 
                      CORE_ADDR len)
{
  struct inferior_info info;
  char *filename = NULL;
  bfd *abfd = NULL;

  info.addr = addr;
  info.offset = offset;
  info.len = len; 
  info.read = 0;

  /* If you change the string "[memory object \"" remember to go
     change it in macosx-nat-dyld-process.c & macosx-nat-dyld.c
     where we test for it.  Or:
     FIXME - make this pattern a variable other code can test against.  */
  if (name != NULL)
    {
      if (len == INVALID_ADDRESS)
	xasprintf (&filename, "[memory object \"%s\" at 0x%s]",
		   name, paddr_nz (addr));
      else
	xasprintf (&filename, "[memory object \"%s\" at 0x%s for 0x%s]",
		   name, paddr_nz (addr), paddr_nz (len));
    }
  else
    {
      xasprintf (&filename, "[memory object at 0x%s for 0x%s]",
                 paddr_nz (addr), paddr_nz (len));
    }

  if (filename == NULL)
    {
      warning ("unable to allocate memory for filename for \"%s\"", name);
      return NULL;
    }

  abfd = bfd_openr_iovec (filename, NULL, 
			 inferior_open, &info,
			 inferior_read, inferior_close);
  if (abfd == NULL)
    {
      warning ("Unable to open memory image for \"%s\"; skipping", name);
      return NULL;
    }

  /* FIXME: should check for errors from bfd_close (for one thing, on
     error it does not free all the storage associated with the bfd).  */

  if (!bfd_check_format (abfd, bfd_object))
    {
      warning ("Unable to read symbols from %s: %s.", bfd_get_filename (abfd),
               bfd_errmsg (bfd_get_error ()));
      close_bfd_or_archive (abfd);
      return NULL;
    }

  return abfd;
}

bfd *
inferior_bfd (const char *name, CORE_ADDR addr, CORE_ADDR offset, CORE_ADDR len)
{
  bfd *abfd = inferior_bfd_generic (name, addr, offset, len);
  if (abfd == NULL)
    return abfd;

  return abfd;
}

/* Returns true if the bfd is based out of memory.  */
int
macosx_bfd_is_in_memory (bfd *abfd)
{
  if (abfd)
    {
      /* Is this a standard bfd where a buffer was handed to a bfd using 
         bfd_memopenr of by memory mapping it? */
      if (abfd->flags & BFD_IN_MEMORY)
	return 1;
      /* Was this mapped as a memory object?  */
      if (abfd->filename != NULL
	  && strstr (abfd->filename, "[memory object") == abfd->filename)
	return 1;
    }
  return 0;
}
