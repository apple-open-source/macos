/* APPLE LOCAL begin this entire file */
/* Handle the tree-based feedback info.
   Copyright (C) 2003 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

enum fdo_note_kind {fdo_none, fdo_incoming, fdo_outgoing, fdo_block,
                    fdo_tablejump};

extern void init_feedback PARAMS ((const char *));

extern tree end_feedback PARAMS ((void));

extern void set_fdo_note_count PARAMS ((rtx, HOST_WIDEST_INT));

extern HOST_WIDEST_INT get_fdo_note_count PARAMS ((rtx));

extern void emit_rtx_feedback_counter PARAMS ((tree, int, enum fdo_note_kind));

extern unsigned int n_slots PARAMS ((tree));

extern void clone_rtx_feedback_counter PARAMS ((tree, tree));

extern void emit_rtx_call_feedback_counter PARAMS ((tree));

extern void decorate_for_feedback PARAMS ((tree));

extern void add_new_feedback_specifics_to_header PARAMS ((tree *, tree *));

extern HOST_WIDEST_INT times_call_executed PARAMS ((tree));

extern void set_times_call_executed PARAMS ((tree, HOST_WIDEST_INT));

extern HOST_WIDEST_INT times_arc_executed PARAMS ((tree, unsigned int));

extern void set_times_arc_executed PARAMS ((tree, unsigned int, HOST_WIDEST_INT));

extern void note_feedback_count_to_br_prob PARAMS ((void));

extern void expand_function_feedback_end PARAMS ((void));

extern unsigned int n_slots PARAMS ((tree));
/* APPLE LOCAL end entire file */
