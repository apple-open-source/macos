/* Assorted BFD support routines, only used internally.
   Copyright 1990, 91, 92, 93, 94, 95, 96, 97, 98, 1999
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

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

/*
SECTION
	Internal functions

DESCRIPTION
	These routines are used within BFD.
	They are not intended for export, but are documented here for
	completeness.
*/

/* A routine which is used in target vectors for unsupported
   operations.  */

/*ARGSUSED*/
boolean
bfd_false (ignore)
     bfd *ignore ATTRIBUTE_UNUSED;
{
  bfd_set_error (bfd_error_invalid_operation);
  return false;
}

/* A routine which is used in target vectors for supported operations
   which do not actually do anything.  */

/*ARGSUSED*/
boolean
bfd_true (ignore)
     bfd *ignore ATTRIBUTE_UNUSED;
{
  return true;
}

/* A routine which is used in target vectors for unsupported
   operations which return a pointer value.  */

/*ARGSUSED*/
PTR
bfd_nullvoidptr (ignore)
     bfd *ignore ATTRIBUTE_UNUSED;
{
  bfd_set_error (bfd_error_invalid_operation);
  return NULL;
}

/*ARGSUSED*/
int 
bfd_0 (ignore)
     bfd *ignore ATTRIBUTE_UNUSED;
{
  return 0;
}

/*ARGSUSED*/
unsigned int 
bfd_0u (ignore)
     bfd *ignore ATTRIBUTE_UNUSED;
{
   return 0;
}

/*ARGUSED*/
long
bfd_0l (ignore)
     bfd *ignore ATTRIBUTE_UNUSED;
{
  return 0;
}

/* A routine which is used in target vectors for unsupported
   operations which return -1 on error.  */

/*ARGSUSED*/
long
_bfd_n1 (ignore_abfd)
     bfd *ignore_abfd ATTRIBUTE_UNUSED;
{
  bfd_set_error (bfd_error_invalid_operation);
  return -1;
}

/*ARGSUSED*/
void 
bfd_void (ignore)
     bfd *ignore ATTRIBUTE_UNUSED;
{
}

/*ARGSUSED*/
boolean
_bfd_nocore_core_file_matches_executable_p (ignore_core_bfd, ignore_exec_bfd)
     bfd *ignore_core_bfd ATTRIBUTE_UNUSED;
     bfd *ignore_exec_bfd ATTRIBUTE_UNUSED;
{
  bfd_set_error (bfd_error_invalid_operation);
  return false;
}

/* Routine to handle core_file_failing_command entry point for targets
   without core file support.  */

/*ARGSUSED*/
char *
_bfd_nocore_core_file_failing_command (ignore_abfd)
     bfd *ignore_abfd ATTRIBUTE_UNUSED;
{
  bfd_set_error (bfd_error_invalid_operation);
  return (char *)NULL;
}

/* Routine to handle core_file_failing_signal entry point for targets
   without core file support.  */

/*ARGSUSED*/
int
_bfd_nocore_core_file_failing_signal (ignore_abfd)
     bfd *ignore_abfd ATTRIBUTE_UNUSED;
{
  bfd_set_error (bfd_error_invalid_operation);
  return 0;
}

/*ARGSUSED*/
const bfd_target *
_bfd_dummy_target (ignore_abfd)
     bfd *ignore_abfd ATTRIBUTE_UNUSED;
{
  bfd_set_error (bfd_error_wrong_format);
  return 0;
}

/* Allocate memory using malloc.  */

PTR
bfd_malloc (size)
     size_t size;
{
  PTR ptr;

  ptr = (PTR) xmalloc (size);
  if (ptr == NULL && size != 0)
    bfd_set_error (bfd_error_no_memory);
  return ptr;
}

/* Reallocate memory using realloc.  */

PTR
bfd_realloc (ptr, size)
     PTR ptr;
     size_t size;
{
  PTR ret;

  if (ptr == NULL)
    ret = xmalloc (size);
  else
    ret = xrealloc (ptr, size);

  if (ret == NULL)
    bfd_set_error (bfd_error_no_memory);

  return ret;
}

/* Allocate memory using malloc and clear it.  */

PTR
bfd_zmalloc (size)
     size_t size;
{
  PTR ptr;

  ptr = (PTR) xmalloc (size);

  if (size != 0)
    {
      if (ptr == NULL)
	bfd_set_error (bfd_error_no_memory);
      else
	memset (ptr, 0, size);
    }

  return ptr;
}

/*
INTERNAL_FUNCTION
	bfd_write_bigendian_4byte_int

SYNOPSIS
	void bfd_write_bigendian_4byte_int(bfd *abfd,  int i);

DESCRIPTION
	Write a 4 byte integer @var{i} to the output BFD @var{abfd}, in big
	endian order regardless of what else is going on.  This is useful in
	archives.

*/
void
bfd_write_bigendian_4byte_int (abfd, i)
     bfd *abfd;
     int i;
{
  bfd_byte buffer[4];
  bfd_putb32(i, buffer);
  if (bfd_write((PTR)buffer, 4, 1, abfd) != 4)
    abort ();
}

/** The do-it-yourself (byte) sex-change kit */

/* The middle letter e.g. get<b>short indicates Big or Little endian
   target machine.  It doesn't matter what the byte order of the host
   machine is; these routines work for either.  */

/* FIXME: Should these take a count argument?
   Answer (gnu@cygnus.com):  No, but perhaps they should be inline
                             functions in swap.h #ifdef __GNUC__. 
                             Gprof them later and find out.  */

/*
FUNCTION
	bfd_put_size
FUNCTION
	bfd_get_size

DESCRIPTION
	These macros as used for reading and writing raw data in
	sections; each access (except for bytes) is vectored through
	the target format of the BFD and mangled accordingly. The
	mangling performs any necessary endian translations and
	removes alignment restrictions.  Note that types accepted and
	returned by these macros are identical so they can be swapped
	around in macros---for example, @file{libaout.h} defines <<GET_WORD>>
	to either <<bfd_get_32>> or <<bfd_get_64>>.

	In the put routines, @var{val} must be a <<bfd_vma>>.  If we are on a
	system without prototypes, the caller is responsible for making
	sure that is true, with a cast if necessary.  We don't cast
	them in the macro definitions because that would prevent <<lint>>
	or <<gcc -Wall>> from detecting sins such as passing a pointer.
	To detect calling these with less than a <<bfd_vma>>, use
	<<gcc -Wconversion>> on a host with 64 bit <<bfd_vma>>'s.

.
.{* Byte swapping macros for user section data.  *}
.
.#define bfd_put_8(abfd, val, ptr) \
.                ((void) (*((unsigned char *)(ptr)) = (unsigned char)(val)))
.#define bfd_put_signed_8 \
.		bfd_put_8
.#define bfd_get_8(abfd, ptr) \
.                (*(unsigned char *)(ptr))
.#define bfd_get_signed_8(abfd, ptr) \
.		((*(unsigned char *)(ptr) ^ 0x80) - 0x80)
.
.#define bfd_put_16(abfd, val, ptr) \
.                BFD_SEND(abfd, bfd_putx16, ((val),(ptr)))
.#define bfd_put_signed_16 \
.		 bfd_put_16
.#define bfd_get_16(abfd, ptr) \
.                BFD_SEND(abfd, bfd_getx16, (ptr))
.#define bfd_get_signed_16(abfd, ptr) \
.         	 BFD_SEND (abfd, bfd_getx_signed_16, (ptr))
.
.#define bfd_put_32(abfd, val, ptr) \
.                BFD_SEND(abfd, bfd_putx32, ((val),(ptr)))
.#define bfd_put_signed_32 \
.		 bfd_put_32
.#define bfd_get_32(abfd, ptr) \
.                BFD_SEND(abfd, bfd_getx32, (ptr))
.#define bfd_get_signed_32(abfd, ptr) \
.		 BFD_SEND(abfd, bfd_getx_signed_32, (ptr))
.
.#define bfd_put_64(abfd, val, ptr) \
.                BFD_SEND(abfd, bfd_putx64, ((val), (ptr)))
.#define bfd_put_signed_64 \
.		 bfd_put_64
.#define bfd_get_64(abfd, ptr) \
.                BFD_SEND(abfd, bfd_getx64, (ptr))
.#define bfd_get_signed_64(abfd, ptr) \
.		 BFD_SEND(abfd, bfd_getx_signed_64, (ptr))
.
.#define bfd_get(bits, abfd, ptr)				\
.                ((bits) == 8 ? bfd_get_8 (abfd, ptr)		\
.		 : (bits) == 16 ? bfd_get_16 (abfd, ptr)	\
.		 : (bits) == 32 ? bfd_get_32 (abfd, ptr)	\
.		 : (bits) == 64 ? bfd_get_64 (abfd, ptr)	\
.		 : (abort (), (bfd_vma) - 1))
.
.#define bfd_put(bits, abfd, val, ptr)				\
.                ((bits) == 8 ? bfd_put_8 (abfd, val, ptr)	\
.		 : (bits) == 16 ? bfd_put_16 (abfd, val, ptr)	\
.		 : (bits) == 32 ? bfd_put_32 (abfd, val, ptr)	\
.		 : (bits) == 64 ? bfd_put_64 (abfd, val, ptr)	\
.		 : (abort (), (void) 0))
.
*/ 

/*
FUNCTION
	bfd_h_put_size
	bfd_h_get_size

DESCRIPTION
	These macros have the same function as their <<bfd_get_x>>
	bretheren, except that they are used for removing information
	for the header records of object files. Believe it or not,
	some object files keep their header records in big endian
	order and their data in little endian order.
.
.{* Byte swapping macros for file header data.  *}
.
.#define bfd_h_put_8(abfd, val, ptr) \
.		bfd_put_8 (abfd, val, ptr)
.#define bfd_h_put_signed_8(abfd, val, ptr) \
.		bfd_put_8 (abfd, val, ptr)
.#define bfd_h_get_8(abfd, ptr) \
.		bfd_get_8 (abfd, ptr)
.#define bfd_h_get_signed_8(abfd, ptr) \
.		bfd_get_signed_8 (abfd, ptr)
.
.#define bfd_h_put_16(abfd, val, ptr) \
.                BFD_SEND(abfd, bfd_h_putx16,(val,ptr))
.#define bfd_h_put_signed_16 \
.		 bfd_h_put_16
.#define bfd_h_get_16(abfd, ptr) \
.                BFD_SEND(abfd, bfd_h_getx16,(ptr))
.#define bfd_h_get_signed_16(abfd, ptr) \
.		 BFD_SEND(abfd, bfd_h_getx_signed_16, (ptr))
.
.#define bfd_h_put_32(abfd, val, ptr) \
.                BFD_SEND(abfd, bfd_h_putx32,(val,ptr))
.#define bfd_h_put_signed_32 \
.		 bfd_h_put_32
.#define bfd_h_get_32(abfd, ptr) \
.                BFD_SEND(abfd, bfd_h_getx32,(ptr))
.#define bfd_h_get_signed_32(abfd, ptr) \
.		 BFD_SEND(abfd, bfd_h_getx_signed_32, (ptr))
.
.#define bfd_h_put_64(abfd, val, ptr) \
.                BFD_SEND(abfd, bfd_h_putx64,(val, ptr))
.#define bfd_h_put_signed_64 \
.		 bfd_h_put_64
.#define bfd_h_get_64(abfd, ptr) \
.                BFD_SEND(abfd, bfd_h_getx64,(ptr))
.#define bfd_h_get_signed_64(abfd, ptr) \
.		 BFD_SEND(abfd, bfd_h_getx_signed_64, (ptr))
.
*/ 

/* Sign extension to bfd_signed_vma.  */
#define COERCE16(x) (((bfd_signed_vma) (x) ^ 0x8000) - 0x8000)
#define COERCE32(x) \
  ((bfd_signed_vma) (long) (((unsigned long) (x) ^ 0x80000000) - 0x80000000))
#define EIGHT_GAZILLION (((BFD_HOST_64_BIT)0x80000000) << 32)
#define COERCE64(x) \
  (((bfd_signed_vma) (x) ^ EIGHT_GAZILLION) - EIGHT_GAZILLION)

bfd_vma
bfd_getb16 (addr)
     register const bfd_byte *addr;
{
  return (addr[0] << 8) | addr[1];
}

bfd_vma
bfd_getl16 (addr)
     register const bfd_byte *addr;
{
  return (addr[1] << 8) | addr[0];
}

bfd_signed_vma
bfd_getb_signed_16 (addr)
     register const bfd_byte *addr;
{
  return COERCE16((addr[0] << 8) | addr[1]);
}

bfd_signed_vma
bfd_getl_signed_16 (addr)
     register const bfd_byte *addr;
{
  return COERCE16((addr[1] << 8) | addr[0]);
}

void
bfd_putb16 (data, addr)
     bfd_vma data;
     register bfd_byte *addr;
{
  addr[0] = (bfd_byte)(data >> 8);
  addr[1] = (bfd_byte )data;
}

void
bfd_putl16 (data, addr)
     bfd_vma data;             
     register bfd_byte *addr;
{
  addr[0] = (bfd_byte )data;
  addr[1] = (bfd_byte)(data >> 8);
}

bfd_vma
bfd_getb32 (addr)
     register const bfd_byte *addr;
{
  unsigned long v;

  v = (unsigned long) addr[0] << 24;
  v |= (unsigned long) addr[1] << 16;
  v |= (unsigned long) addr[2] << 8;
  v |= (unsigned long) addr[3];
  return (bfd_vma) v;
}

bfd_vma
bfd_getl32 (addr)
     register const bfd_byte *addr;
{
  unsigned long v;

  v = (unsigned long) addr[0];
  v |= (unsigned long) addr[1] << 8;
  v |= (unsigned long) addr[2] << 16;
  v |= (unsigned long) addr[3] << 24;
  return (bfd_vma) v;
}

bfd_signed_vma
bfd_getb_signed_32 (addr)
     register const bfd_byte *addr;
{
  unsigned long v;

  v = (unsigned long) addr[0] << 24;
  v |= (unsigned long) addr[1] << 16;
  v |= (unsigned long) addr[2] << 8;
  v |= (unsigned long) addr[3];
  return COERCE32 (v);
}

bfd_signed_vma
bfd_getl_signed_32 (addr)
     register const bfd_byte *addr;
{
  unsigned long v;

  v = (unsigned long) addr[0];
  v |= (unsigned long) addr[1] << 8;
  v |= (unsigned long) addr[2] << 16;
  v |= (unsigned long) addr[3] << 24;
  return COERCE32 (v);
}

bfd_vma
bfd_getb64 (addr)
     register const bfd_byte *addr ATTRIBUTE_UNUSED;
{
#ifdef BFD64
  bfd_vma low, high;

  high= ((((((((addr[0]) << 8) |
              addr[1]) << 8) |
            addr[2]) << 8) |
          addr[3]) );

  low = (((((((((bfd_vma)addr[4]) << 8) |
              addr[5]) << 8) |
            addr[6]) << 8) |
          addr[7]));

  return high << 32 | low;
#else
  BFD_FAIL();
  return 0;
#endif
}

bfd_vma
bfd_getl64 (addr)
     register const bfd_byte *addr ATTRIBUTE_UNUSED;
{
#ifdef BFD64
  bfd_vma low, high;
  high= (((((((addr[7] << 8) |
              addr[6]) << 8) |
            addr[5]) << 8) |
          addr[4]));

  low = ((((((((bfd_vma)addr[3] << 8) |
              addr[2]) << 8) |
            addr[1]) << 8) |
          addr[0]) );

  return high << 32 | low;
#else
  BFD_FAIL();
  return 0;
#endif

}

bfd_signed_vma
bfd_getb_signed_64 (addr)
     register const bfd_byte *addr ATTRIBUTE_UNUSED;
{
#ifdef BFD64
  bfd_vma low, high;

  high= ((((((((addr[0]) << 8) |
              addr[1]) << 8) |
            addr[2]) << 8) |
          addr[3]) );

  low = (((((((((bfd_vma)addr[4]) << 8) |
              addr[5]) << 8) |
            addr[6]) << 8) |
          addr[7]));

  return COERCE64(high << 32 | low);
#else
  BFD_FAIL();
  return 0;
#endif
}

bfd_signed_vma
bfd_getl_signed_64 (addr)
     register const bfd_byte *addr ATTRIBUTE_UNUSED;
{
#ifdef BFD64
  bfd_vma low, high;
  high= (((((((addr[7] << 8) |
              addr[6]) << 8) |
            addr[5]) << 8) |
          addr[4]));

  low = ((((((((bfd_vma)addr[3] << 8) |
              addr[2]) << 8) |
            addr[1]) << 8) |
          addr[0]) );

  return COERCE64(high << 32 | low);
#else
  BFD_FAIL();
  return 0;
#endif
}

void
bfd_putb32 (data, addr)
     bfd_vma data;
     register bfd_byte *addr;
{
        addr[0] = (bfd_byte)(data >> 24);
        addr[1] = (bfd_byte)(data >> 16);
        addr[2] = (bfd_byte)(data >>  8);
        addr[3] = (bfd_byte)data;
}

void
bfd_putl32 (data, addr)
     bfd_vma data;
     register bfd_byte *addr;
{
        addr[0] = (bfd_byte)data;
        addr[1] = (bfd_byte)(data >>  8);
        addr[2] = (bfd_byte)(data >> 16);
        addr[3] = (bfd_byte)(data >> 24);
}

void
bfd_putb64 (data, addr)
     bfd_vma data ATTRIBUTE_UNUSED;
     register bfd_byte *addr ATTRIBUTE_UNUSED;
{
#ifdef BFD64
  addr[0] = (bfd_byte)(data >> (7*8));
  addr[1] = (bfd_byte)(data >> (6*8));
  addr[2] = (bfd_byte)(data >> (5*8));
  addr[3] = (bfd_byte)(data >> (4*8));
  addr[4] = (bfd_byte)(data >> (3*8));
  addr[5] = (bfd_byte)(data >> (2*8));
  addr[6] = (bfd_byte)(data >> (1*8));
  addr[7] = (bfd_byte)(data >> (0*8));
#else
  BFD_FAIL();
#endif
}

void
bfd_putl64 (data, addr)
     bfd_vma data ATTRIBUTE_UNUSED;
     register bfd_byte *addr ATTRIBUTE_UNUSED;
{
#ifdef BFD64
  addr[7] = (bfd_byte)(data >> (7*8));
  addr[6] = (bfd_byte)(data >> (6*8));
  addr[5] = (bfd_byte)(data >> (5*8));
  addr[4] = (bfd_byte)(data >> (4*8));
  addr[3] = (bfd_byte)(data >> (3*8));
  addr[2] = (bfd_byte)(data >> (2*8));
  addr[1] = (bfd_byte)(data >> (1*8));
  addr[0] = (bfd_byte)(data >> (0*8));
#else
  BFD_FAIL();
#endif
}

/* Default implementation */

boolean
_bfd_generic_get_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  if (count == 0)
    return true;

  if ((bfd_size_type) (offset + count) > section->_raw_size)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  if (bfd_seek (abfd, section->filepos + offset, SEEK_SET) != 0
      || bfd_read (location, (bfd_size_type) 1, count, abfd) != count)
    return false;

  return true;
}

/* This generic function can only be used in implementations where creating
   NEW sections is disallowed.  It is useful in patching existing sections
   in read-write files, though.  See other set_section_contents functions
   to see why it doesn't work for new sections.  */
boolean
_bfd_generic_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  if (count == 0)
    return true;

  if (bfd_seek (abfd, (file_ptr) (section->filepos + offset), SEEK_SET) == -1
      || bfd_write (location, (bfd_size_type) 1, count, abfd) != count)
    return false;

  return true;
}

/*
INTERNAL_FUNCTION
	bfd_log2

SYNOPSIS
	unsigned int bfd_log2(bfd_vma x);

DESCRIPTION
	Return the log base 2 of the value supplied, rounded up.  E.g., an
	@var{x} of 1025 returns 11.
*/

unsigned int
bfd_log2 (x)
     bfd_vma x;
{
  unsigned int result = 0;

  while ((x = (x >> 1)) != 0)
    ++result;
  return result;
}

boolean
bfd_generic_is_local_label_name (abfd, name)
     bfd *abfd;
     const char *name;
{
  char locals_prefix = (bfd_get_symbol_leading_char (abfd) == '_') ? 'L' : '.';

  return (name[0] == locals_prefix);
}

/*  Can be used from / for bfd_merge_private_bfd_data to check that
    endianness matches between input and output file.  Returns
    true for a match, otherwise returns false and emits an error.  */
boolean
_bfd_generic_verify_endian_match (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  if (ibfd->xvec->byteorder != obfd->xvec->byteorder
      && ibfd->xvec->byteorder != BFD_ENDIAN_UNKNOWN
      && obfd->xvec->byteorder != BFD_ENDIAN_UNKNOWN)
    {
      const char *msg;

      if (bfd_big_endian (ibfd))
	msg = _("%s: compiled for a big endian system and target is little endian");
      else
	msg = _("%s: compiled for a little endian system and target is big endian");

      (*_bfd_error_handler) (msg, bfd_get_filename (ibfd));

      bfd_set_error (bfd_error_wrong_format);
      return false;
    }

  return true;
}
