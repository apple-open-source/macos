/* BFD library support routines for the Microchip PIC architecture.
   Copyright (C) 1997 Klee Dienes <klee@mit.edu>

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

const bfd_arch_info_type bfd_pic12_arch =
{
  16,				/* 16 bits in a word */
  16,				/* 16 bits in an address */
  8,				/* 8 bits in a byte */
  bfd_arch_pic12,
  0,				/* only 1 machine */
  "pic12",			/* arch_name  */
  "pic12",			/* printable name */
  1,
  true,				/* the default machine */
  bfd_default_compatible,
  bfd_default_scan,
  0,
};

const bfd_arch_info_type bfd_pic14_arch =
{
  16,				/* 16 bits in a word */
  16,				/* 16 bits in an address */
  8,				/* 8 bits in a byte */
  bfd_arch_pic14,
  0,				/* only 1 machine */
  "pic14",			/* arch_name  */
  "pic14",			/* printable name */
  1,
  true,				/* the default machine */
  bfd_default_compatible,
  bfd_default_scan,
  0,
};

const bfd_arch_info_type bfd_pic16_arch =
{
  16,				/* 16 bits in a word */
  16,				/* 16 bits in an address */
  8,				/* 8 bits in a byte */
  bfd_arch_pic16,
  0,				/* only 1 machine */
  "pic16",			/* arch_name  */
  "pic16",			/* printable name */
  1,
  true,				/* the default machine */
  bfd_default_compatible,
  bfd_default_scan,
  0,
};
