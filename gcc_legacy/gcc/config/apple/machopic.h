/* NeXTSTEP mach-o pic support functions.
   Copyright (C) 1992, 1994 Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifdef MACHO_PIC
#ifndef _MACHOPIC_H
#define _MACHOPIC_H

enum machopic_addr_class {
  MACHOPIC_UNDEFINED,
  MACHOPIC_DEFINED_DATA,
  MACHOPIC_UNDEFINED_DATA,
  MACHOPIC_DEFINED_FUNCTION,
  MACHOPIC_UNDEFINED_FUNCTION,
};

#ifndef MACHOPIC_PURE
#define MACHOPIC_PURE          (flag_pic == 2)
#define MACHOPIC_INDIRECT      (flag_pic)
#define MACHOPIC_JUST_INDIRECT (flag_pic == 1)
#endif

#ifdef MACHOPIC_OPERAND_P
#undef MACHOPIC_OPERAND_P
#endif

#define MACHOPIC_OPERAND_P(X) machopic_operand_p (X)

int machopic_operand_p PROTO ((rtx));
enum machopic_addr_class machopic_classify_ident PROTO ((tree));
enum machopic_addr_class machopic_classify_name PROTO ((const char*));
void machopic_define_ident PROTO ((tree));
void machopic_define_name PROTO ((const char*));
int machopic_name_defined_p PROTO ((const char*));
int machopic_ident_defined_p PROTO ((tree));

char* machopic_function_base_name ();
char* machopic_non_lazy_ptr_name PROTO ((const char*));
char* machopic_stub_name PROTO((const char*));

rtx machopic_indirect_data_reference PROTO((rtx, rtx));
rtx machopic_indirect_call_target PROTO ((rtx));
rtx machopic_legitimize_pic_address PROTO ((rtx, enum machine_mode, rtx));

extern int mcount_called;	/* Nonzero if mcount is ever called.  */

#endif
#endif
