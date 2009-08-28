/* Target support for Mac OS X on PowerPC for GDB, the GNU debugger.
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

#ifndef _TM_ARM_MACOSX_H_
#define _TM_ARM_MACOSX_H_

#include "tm-macosx.h"


int arm_macosx_fast_show_stack (unsigned int count_limit, 
				unsigned int print_limit,
				unsigned int *count,
				void (print_fun) (struct ui_out * uiout, 
				int frame_num, CORE_ADDR pc, CORE_ADDR fp));

#define FAST_COUNT_STACK_DEPTH(count_limit, print_limit, count, print_fun) \
  (arm_macosx_fast_show_stack (count_limit, print_limit, count, print_fun))

char *arm_throw_catch_find_typeinfo (struct frame_info *curr_frame,
                               int exception_type);
#define THROW_CATCH_FIND_TYPEINFO(curr_frame, exception_type) \
  (arm_throw_catch_find_typeinfo (curr_frame, exception_type))

int arm_macosx_in_switch_glue (CORE_ADDR pc);
#define IN_SWITCH_GLUE(pc) \
  (arm_macosx_in_switch_glue (pc))

#endif /* _TM_ARM_MACOSX_H_ */
