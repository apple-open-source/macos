#ifndef __PPC_TDEP_H__
#define __PPC_TDEP_H__

#include "defs.h"

/* Used in frameinfo, and passed to ppc_parse_instructions it means
   keep looking as long as necessary... */
#define INVALID_ADDRESS 0xffffffff

struct type;
struct frame_info;

struct frame_extra_info
{
  CORE_ADDR initial_sp;			/* initial stack pointer. */
  struct ppc_function_boundaries *bounds;
  struct ppc_function_properties *props;
};

char *ppc_register_name (int regno);

/* core stack frame decoding functions */

void ppc_init_extra_frame_info PARAMS ((int fromleaf, struct frame_info *prev));

void ppc_print_extra_frame_info PARAMS ((struct frame_info *frame));

void ppc_init_frame_pc_first PARAMS ((int fromleaf, struct frame_info *prev));

void ppc_init_frame_pc PARAMS ((int fromleaf, struct frame_info *prev));

CORE_ADDR ppc_frame_saved_pc PARAMS ((struct frame_info *fi));

CORE_ADDR ppc_frame_saved_pc_after_call PARAMS ((struct frame_info *frame));

CORE_ADDR ppc_frame_prev_pc PARAMS ((struct frame_info *frame));

CORE_ADDR ppc_frame_chain PARAMS ((struct frame_info *frame));

int ppc_frame_chain_valid PARAMS ((CORE_ADDR chain, struct frame_info *frame));

/* more esoteric functions */

int ppc_is_dummy_frame PARAMS ((struct frame_info *frame));

CORE_ADDR ppc_frame_cache_initial_stack_address PARAMS ((struct frame_info *fi));
CORE_ADDR ppc_frame_initial_stack_address PARAMS ((struct frame_info *fi));

int ppc_is_magic_function_pointer PARAMS ((CORE_ADDR addr));

CORE_ADDR ppc_skip_trampoline_code PARAMS ((CORE_ADDR pc));

CORE_ADDR ppc_convert_from_func_ptr_addr PARAMS ((CORE_ADDR addr));

CORE_ADDR ppc_find_toc_address PARAMS ((CORE_ADDR pc));

int ppc_use_struct_convention PARAMS ((int gccp, struct type *valtype));

CORE_ADDR ppc_extract_struct_value_address PARAMS
  ((char regbuf[]));

void ppc_extract_return_value PARAMS 
  ((struct type *valtype, char regbuf[], char *valbuf));

CORE_ADDR ppc_skip_prologue PARAMS ((CORE_ADDR pc));

int ppc_frameless_function_invocation PARAMS ((struct frame_info *frame));

int ppc_invalid_float PARAMS ((char *f, size_t len));

#endif /* __PPC_TDEP_H__ */
