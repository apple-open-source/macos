/* Generic BFD support for file formats.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.
   Written by Cygnus Support.

This file is part of BFD, the Binary File Descriptor library.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/*
SECTION
	File formats

	A format is a BFD concept of high level file contents type. The
	formats supported by BFD are:

	o <<bfd_object>>

	The BFD may contain data, symbols, relocations and debug info.

	o <<bfd_archive>>

	The BFD contains other BFDs and an optional index.

	o <<bfd_core>>

	The BFD contains the result of an executable core dump.

*/

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

/* IMPORT from targets.c.  */
extern const size_t _bfd_target_vector_entries;

/*
FUNCTION
	bfd_check_format

SYNOPSIS
	bfd_boolean bfd_check_format (bfd *abfd, bfd_format format);

DESCRIPTION
	Verify if the file attached to the BFD @var{abfd} is compatible
	with the format @var{format} (i.e., one of <<bfd_object>>,
	<<bfd_archive>> or <<bfd_core>>).

	If the BFD has been set to a specific target before the
	call, only the named target and format combination is
	checked. If the target has not been set, or has been set to
	<<default>>, then all the known target backends is
	interrogated to determine a match.  If the default target
	matches, it is used.  If not, exactly one target must recognize
	the file, or an error results.

	The function returns <<TRUE>> on success, otherwise <<FALSE>>
	with one of the following error codes:

	o <<bfd_error_invalid_operation>> -
	if <<format>> is not one of <<bfd_object>>, <<bfd_archive>> or
	<<bfd_core>>.

	o <<bfd_error_system_call>> -
	if an error occured during a read - even some file mismatches
	can cause bfd_error_system_calls.

	o <<file_not_recognised>> -
	none of the backends recognised the file format.

	o <<bfd_error_file_ambiguously_recognized>> -
	more than one backend recognised the file format.
*/

bfd_boolean
bfd_check_format (bfd *abfd, bfd_format format)
{
  return bfd_check_format_matches (abfd, format, NULL);
}

/*
FUNCTION
	bfd_check_format_matches

SYNOPSIS
	bfd_boolean bfd_check_format_matches
	  (bfd *abfd, bfd_format format, char ***matching);

DESCRIPTION
	Like <<bfd_check_format>>, except when it returns FALSE with
	<<bfd_errno>> set to <<bfd_error_file_ambiguously_recognized>>.  In that
	case, if @var{matching} is not NULL, it will be filled in with
	a NULL-terminated list of the names of the formats that matched,
	allocated with <<malloc>>.
	Then the user may choose a format and try again.

	When done with the list that @var{matching} points to, the caller
	should free it.
*/

bfd_boolean
bfd_check_format_matches (bfd *abfd, bfd_format format, char ***matching)
{
  extern const bfd_target binary_vec;
  bfd save_bfd;

  char **matching_targnames = NULL;
  bfd *matching_bfd = NULL;
  int match_count = 0;

  if (! bfd_read_p (abfd) ||
      ((int) (abfd->format) < (int) bfd_unknown) ||
      ((int) (abfd->format) >= (int) bfd_type_end))
    {
      bfd_set_error (bfd_error_invalid_operation);
      return FALSE;
    }
  
  if (abfd->format != bfd_unknown)
    return abfd->format == format;

  /* Since the target type was defaulted, check them 
     all in the hope that one will be uniquely recognized.  */

  matching_targnames =
    bfd_malloc (sizeof (char *) * 
		(_bfd_target_vector_entries + 1));
  if (! matching_targnames)
    return FALSE;
  matching_targnames[0] = NULL;

  matching_bfd = bfd_malloc (sizeof (struct bfd) *
			     (_bfd_target_vector_entries + 1));
  if (! matching_bfd)
    return FALSE;

  match_count = 0;

  memcpy (&save_bfd, abfd, sizeof (struct bfd));
  
  BFD_ASSERT (abfd->tdata.any == NULL);

  if (! abfd->target_defaulted) 
    {
      const bfd_target *ntarget = NULL;

      if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)	/* rewind */
	return FALSE;
      abfd->format = format;
      ntarget = BFD_SEND_FMT (abfd, _bfd_check_format, (abfd));
      if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)	/* rewind */
	return FALSE;
      if (ntarget)
	{
	  abfd->xvec = ntarget;
	  memcpy (&matching_bfd[match_count], abfd, sizeof (struct bfd));
	  matching_targnames[match_count] = ntarget->name;
	  match_count++;
	  matching_targnames[match_count] = NULL;
	}
    }
  else 
    {
      const bfd_target *const *targetptr = NULL;
      const bfd_target *ntarget = NULL;
      int nerror;

      for (targetptr = bfd_target_vector; *targetptr != NULL; targetptr++) 
	{
	  /* no point checking for this; it will always match */
	  if (*targetptr == &binary_vec)
	    continue;
	 
	  /* If _bfd_check_format neglects to set bfd_error, assume bfd_error_wrong_format.
	     We didn't used to even pay any attention to bfd_error, so I suspect
	     that some _bfd_check_format might have this problem.  */

	  bfd_set_error (bfd_error_wrong_format);

	  memcpy (abfd, &save_bfd, sizeof (struct bfd));
	  abfd->xvec = *targetptr; /* Change BFD's target temporarily. */
	  abfd->format = format;

	  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
	    return FALSE;

	  ntarget = BFD_SEND_FMT (abfd, _bfd_check_format, (abfd));
	  nerror = bfd_get_error ();

	  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
	    return FALSE;

	  if (ntarget != NULL)
	    {
	      /* This format checks out as ok! */

	      abfd->xvec = ntarget;
	      memcpy (&matching_bfd[match_count], abfd, sizeof (struct bfd));
	      matching_targnames[match_count] = ntarget->name;
	      match_count++;
	      matching_targnames[match_count] = NULL;

	      if (ntarget == bfd_default_vector[0])
		break;

#ifdef GNU960
	      /* Big- and little-endian b.out archives look the same, but it doesn't
	       * matter: there is no difference in their headers, and member file byte
	       * orders will (I hope) be handled appropriately by bfd.  Ditto for big
	       * and little coff archives.  And the 4 coff/b.out object formats are
	       * unambiguous.  So accept the first match we find.
	       */
	      break;
#endif
	    }
	  else if (nerror != bfd_error_wrong_format) 
	    {
	      save_bfd.format = bfd_unknown;
	      break;
	    }
	}
    }
  memcpy (abfd, &save_bfd, sizeof (struct bfd));
      
  if (match_count == 1)
    {
      memcpy (abfd, &matching_bfd[0], sizeof (struct bfd));
      free (matching_bfd);
      free (matching_targnames);
      return TRUE;
    }
  else if (match_count == 0)
    {
      bfd_set_error (bfd_error_file_not_recognized);
      free (matching_bfd);
      if (matching != NULL)
	*matching = matching_targnames;
      return FALSE;
    }
  else
    {
      bfd_set_error (bfd_error_file_ambiguously_recognized);
      return FALSE;
    }
}

/*
FUNCTION
	bfd_set_format

SYNOPSIS
	bfd_boolean bfd_set_format (bfd *abfd, bfd_format format);

DESCRIPTION
	This function sets the file format of the BFD @var{abfd} to the
	format @var{format}. If the target set in the BFD does not
	support the format requested, the format is invalid, or the BFD
	is not open for writing, then an error occurs.
*/

bfd_boolean
bfd_set_format (bfd *abfd, bfd_format format)
{
  if (bfd_read_p (abfd) ||
      ((int) abfd->format < (int) bfd_unknown) ||
      ((int) abfd->format >= (int) bfd_type_end))
    {
      bfd_set_error (bfd_error_invalid_operation);
      return FALSE;
    }

  if (abfd->format != bfd_unknown)
    return abfd->format == format;

  /* Presume the answer is yes.  */
  abfd->format = format;

  if (!BFD_SEND_FMT (abfd, _bfd_set_format, (abfd)))
    {
      abfd->format = bfd_unknown;
      return FALSE;
    }

  return TRUE;
}

/*
FUNCTION
	bfd_format_string

SYNOPSIS
	const char *bfd_format_string (bfd_format format);

DESCRIPTION
	Return a pointer to a const string
	<<invalid>>, <<object>>, <<archive>>, <<core>>, or <<unknown>>,
	depending upon the value of @var{format}.
*/

const char *
bfd_format_string (bfd_format format)
{
  if (((int)format < (int) bfd_unknown)
      || ((int)format >= (int) bfd_type_end))
    return "invalid";

  switch (format)
    {
    case bfd_object:
      return "object";		/* Linker/assembler/compiler output.  */
    case bfd_archive:
      return "archive";		/* Object archive file.  */
    case bfd_core:
      return "core";		/* Core dump.  */
    default:
      return "unknown";
    }
}
