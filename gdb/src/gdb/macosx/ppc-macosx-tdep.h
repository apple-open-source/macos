#ifndef __GDB_PPC_MACOSX_TDEP_H__
#define __GDB_PPC_MACOSX_TDEP_H__

#include "defs.h"

/* Used in frameinfo, and passed to ppc_parse_instructions it means
   keep looking as long as necessary... */

#define INVALID_ADDRESS ((CORE_ADDR) (-1))

struct frame_extra_info
{
  CORE_ADDR initial_sp;
  struct ppc_function_boundaries *bounds;
  struct ppc_function_properties *props;
};

const char *ppc_register_name (int regno);

/* core stack frame decoding functions */

void ppc_init_extra_frame_info (int fromleaf, struct frame_info *prev);

void ppc_print_extra_frame_info (struct frame_info *frame);

CORE_ADDR ppc_init_frame_pc_first (int fromleaf, struct frame_info *prev);

CORE_ADDR ppc_init_frame_pc (int fromleaf, struct frame_info *prev);

CORE_ADDR ppc_frame_saved_pc (struct frame_info *fi);

CORE_ADDR ppc_frame_saved_pc_after_call (struct frame_info *frame);

CORE_ADDR ppc_frame_prev_pc (struct frame_info *frame);

CORE_ADDR ppc_frame_chain (struct frame_info *frame);

int ppc_frame_chain_valid (CORE_ADDR chain, struct frame_info *frame);

/* more esoteric functions */

int ppc_is_dummy_frame (struct frame_info *frame);

CORE_ADDR ppc_frame_cache_initial_stack_address (struct frame_info *fi);
CORE_ADDR ppc_frame_initial_stack_address (struct frame_info *fi);

int ppc_is_magic_function_pointer (CORE_ADDR addr);

CORE_ADDR ppc_skip_trampoline_code (CORE_ADDR pc);

int ppc_use_struct_convention (int gccp, struct type *valtype);

CORE_ADDR ppc_extract_struct_value_address (char regbuf[]);

void ppc_extract_return_value (struct type *valtype, char *regbuf, char *valbuf);

CORE_ADDR ppc_skip_prologue (CORE_ADDR pc);

int ppc_frameless_function_invocation (struct frame_info *frame);

int ppc_invalid_float (char *f, size_t len);

void ppc_debug (const char *fmt, ...);

CORE_ADDR ppc_macosx_skip_trampoline_code (CORE_ADDR pc);
int ppc_macosx_in_solib_return_trampoline (CORE_ADDR pc, char *name);
int ppc_macosx_in_solib_call_trampoline (CORE_ADDR pc, char *name);

CORE_ADDR ppc_macosx_dynamic_trampoline_nextpc (CORE_ADDR pc);
int ppc_macosx_in_solib_return_trampoline (CORE_ADDR pc, char *name);

CORE_ADDR ppc_macosx_skip_trampoline_code (CORE_ADDR pc);

int
ppc_fast_show_stack (int show_frames, int get_names,
		     unsigned int count_limit, unsigned int print_limit,
		     unsigned int *count,
		     void (print_fun) (struct ui_out *uiout, int frame_num,
				       CORE_ADDR pc, CORE_ADDR fp));
#endif /* __GDB_PPC_MACOSX_TDEP_H__ */
