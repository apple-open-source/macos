/* Target support for Mac OS X for GDB, the GNU debugger.
   Copyright (C) 1997-2002,
   Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _TM_NEXTSTEP_H_
#define _TM_NEXTSTEP_H_

#include "macosx-tdep.h"

#define BLOCK_ADDRESS_ABSOLUTE 1
#define BELIEVE_PCC_PROMOTION 1

#define INTERNALIZE_SYMBOL(intern, extern, abfd) \
macosx_internalize_symbol (&intern, extern, abfd)

#define TEXT_SECTION_NAME "LC_SEGMENT.__TEXT.__text"
#define DATA_SECTION_NAME "LC_SEGMENT.__DATA.__data"

#define TM_NEXTSTEP 1

#endif /* _TM_NEXTSTEP_H_ */
