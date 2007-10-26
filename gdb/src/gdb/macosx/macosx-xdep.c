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

/* This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "symfile.h"
#include "symtab.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "regcache.h"

#include <sys/ttycom.h>
#include <sys/ioctl.h>

void
macosx_resize_window (int *lines_per_page, int *chars_per_line)
{
  int new_lines_per_page = UINT_MAX;
  int new_chars_per_line = UINT_MAX;
  int ret;

  if (isatty (fileno (stdout)))
    {
      struct winsize window_size;
      ret = ioctl (fileno (stdout), TIOCGWINSZ, &window_size);
      if (ret == 0)
        {
          new_lines_per_page = window_size.ws_row;
          new_chars_per_line = window_size.ws_col;
        }
    }

  if ((*lines_per_page != UINT_MAX) && (*lines_per_page != 0))
    {
      *lines_per_page = new_lines_per_page;
    }

  if ((*chars_per_line != UINT_MAX) && (*chars_per_line != 0))
    {
      *chars_per_line = new_chars_per_line;
    }

  set_screen_size ();
  set_width ();
}

