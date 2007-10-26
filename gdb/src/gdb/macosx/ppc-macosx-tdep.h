#ifndef __GDB_PPC_MACOSX_TDEP_H__
#define __GDB_PPC_MACOSX_TDEP_H__

#include "defs.h"

/* Used in frameinfo, and passed to ppc_parse_instructions it means
   keep looking as long as necessary... */

#define INVALID_ADDRESS ((CORE_ADDR) (-1))

/* A regression to pre-gdbarch days, until we decide on a final
   solution.  */
#ifndef PRINT_EXTRA_FRAME_INFO
#define PRINT_EXTRA_FRAME_INFO(NEXT,CACHE) ppc_print_extra_frame_info (NEXT, CACHE)
#endif

const char *ppc_register_name (int regno);

/* core stack frame decoding functions */

struct ppc_frame_cache *ppc_frame_cache (struct frame_info *next_frame,
                                         void **this_cache);

void ppc_print_extra_frame_info (struct frame_info *next_frame,
                                 void **this_cache);

CORE_ADDR ppc_init_frame_pc_first (int fromleaf, struct frame_info *prev);

CORE_ADDR ppc_init_frame_pc (int fromleaf, struct frame_info *prev);

CORE_ADDR ppc_frame_saved_pc (struct frame_info *fi);

CORE_ADDR ppc_frame_saved_pc_after_call (struct frame_info *frame);

CORE_ADDR ppc_frame_prev_pc (struct frame_info *frame);

CORE_ADDR ppc_frame_chain (struct frame_info *frame);

int ppc_frame_chain_valid (CORE_ADDR chain, struct frame_info *frame);

/* more esoteric functions */

int ppc_is_dummy_frame (struct frame_info *frame);

int ppc_is_magic_function_pointer (CORE_ADDR addr);

int ppc_use_struct_convention (int gccp, struct type *valtype);

CORE_ADDR ppc_extract_struct_value_address (char regbuf[]);

void ppc_extract_return_value (struct type *valtype, char *regbuf,
                               char *valbuf);

CORE_ADDR ppc_skip_prologue (CORE_ADDR pc);

int ppc_frameless_function_invocation (struct frame_info *frame);

int ppc_invalid_float (char *f, size_t len);

void ppc_debug (const char *fmt, ...);

CORE_ADDR ppc_frame_unwind_sp (struct frame_info *next_frame, void **this_cache);

CORE_ADDR ppc_frame_unwind_fp (struct frame_info *next_frame, void **this_cache);

CORE_ADDR ppc_frame_unwind_fp_for_dereferencing (struct frame_info *next_frame, void **this_cache);

CORE_ADDR ppc_frame_find_prev_sp (struct frame_info *next_frame, void **this_cache);

CORE_ADDR ppc_frame_find_prev_pc (struct frame_info *next_frame, void **this_cache);

#endif /* __GDB_PPC_MACOSX_TDEP_H__ */
