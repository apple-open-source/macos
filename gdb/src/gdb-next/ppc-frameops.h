#ifndef __PPC_FRAMEOPS_H__
#define __PPC_FRAMEOPS_H__

#include "defs.h"

struct frame_info;
struct frame_saved_regs;
struct value;
struct type;

#define INSTRUCTION_SIZE 4

#define CALL_DUMMY_START_OFFSET      ((32 + 6) * INSTRUCTION_SIZE)
#define	TOC_ADDR_OFFSET              CALL_DUMMY_START_OFFSET
#define	TARGET_ADDR_OFFSET           (CALL_DUMMY_START_OFFSET + (2 * INSTRUCTION_SIZE))
#define CALL_DUMMY_BREAKPOINT_OFFSET (CALL_DUMMY_START_OFFSET + (6 * INSTRUCTION_SIZE))
#define CALL_DUMMY_LENGTH            ((32 + 16 + 32) * INSTRUCTION_SIZE)

void ppc_push_dummy_frame PARAMS (());

void ppc_pop_frame PARAMS (());

void ppc_fix_call_dummy PARAMS 
  ((char *dummy, CORE_ADDR pc, CORE_ADDR addr, int nargs, struct value **args, struct type *type, int gcc_p));
	
CORE_ADDR ppc_push_arguments PARAMS 
  ((int nargs, struct value **args, CORE_ADDR sp, 
    int struct_return, CORE_ADDR struct_addr));

void ppc_stack_alloc PARAMS ((CORE_ADDR *sp, CORE_ADDR *start, size_t argsize, size_t len));

void ppc_frame_saved_regs PARAMS ((struct frame_info *frame, CORE_ADDR *regs));

void ppc_frame_cache_saved_regs PARAMS ((struct frame_info *frame));

#endif /* __PPC_FRAMEOPS_H__ */

