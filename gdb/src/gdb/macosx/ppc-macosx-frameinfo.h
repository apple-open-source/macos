#ifndef __GDB_PPC_MACOSX_FRAMEINFO_H__
#define __GDB_PPC_MACOSX_FRAMEINFO_H__

#include "defs.h"

struct frame_info;

typedef struct ppc_function_boundaries_request
  ppc_function_boundaries_request;
typedef struct ppc_function_boundaries ppc_function_boundaries;
typedef struct ppc_function_properties ppc_function_properties;

struct ppc_function_boundaries_request
{
  CORE_ADDR min_start;          /* mininimum start address for the function */
  CORE_ADDR max_end;            /* maximum address for the end */

  CORE_ADDR contains_pc;        /* PC of address in function */

  CORE_ADDR prologue_start;     /* guess start of the function prologue */
  CORE_ADDR body_start;         /* guess start of the function */
  CORE_ADDR epilogue_start;     /* guess start of the function eplogue */
  CORE_ADDR function_end;       /* guess address after last instruction in function */
};

struct ppc_function_boundaries
{
  CORE_ADDR prologue_start;     /* start of the function prologue */
  CORE_ADDR body_start;         /* start of the function */
  CORE_ADDR epilogue_start;     /* start of the function eplogue */
  CORE_ADDR function_end;       /* address after last instruction in function */
};

struct ppc_function_properties
{
  int offset;                   /* This is the stack offset.  The
                                   tricky bit is that on the PPC you
                                   can EITHER move the SP first, and
                                   then save the registers OR save the
                                   registers and then move the SP.
                                   This offset is used to get the
                                   gpr_offset right in either case. */

  int saved_gpr;                /* smallest # of saved gpr */
  int saved_fpr;                /* smallest # of saved fpr */
  int gpr_offset;               /* offset of saved gprs */
  int fpr_offset;               /* offset of saved fprs */

  char frameless;               /* true if no stack frame allocated */

  char frameptr_used;           /* true if frame uses a frame pointer */
  int frameptr_reg;             /* frame pointer register number */
  CORE_ADDR frameptr_pc;        /* Where in the prologue is the frameptr
                                   set up (if used). */
  CORE_ADDR lr_saved;           /* 0 if the lr is not saved, otherwise
                                   the pc at which it is saved. */
  int lr_offset;                /* offset of saved lr */
  CORE_ADDR lr_invalid;         /* if 0, then the prev. frame pc is
                                   either saved, or in the link
                                   register .  Otherwise, the insn
                                   which moves something else to the
                                   lr. */
  CORE_ADDR lr_valid_again;     /* If lr_invalid != 0, then this is where
                                   the prev. frame pc gets moved BACK to the
                                   lr. */
  int lr_reg;                   /* If lr_invalid is true, then this is the
                                   reg # where the lr is stored. */

  char cr_saved;                /* true if condition register is saved */
  int cr_offset;                /* offset of saved cr */

  char minimal_toc_loaded;
  int pic_base_reg;             /* Did we see the magic pic base setup code */

  CORE_ADDR pic_base_address;   /* What address was the pic base reg set to?  */
};

struct ppc_frame_cache
{
  /* Base address.  */
  CORE_ADDR stack;
  CORE_ADDR frame;
  CORE_ADDR pc;

  CORE_ADDR prev_pc;
  CORE_ADDR prev_sp;

  CORE_ADDR *saved_regs;
  int saved_regs_valid;

  /* 32 bit apps use 32 bit sigcontexts on G4's and earlier
     but 64 bit on G5's.  We make the simplification that
     the register size is 64 bit on all machines, but we need
     to pull the right number of bits from the sigcontext.
     This gets set in ppc_sigtramp_frame_cache, and used in
     ppc_sigtramp_frame_prev_register.  */

  int sigtramp_gp_store_size;
  struct ppc_function_properties properties;
  int properties_valid;

  struct ppc_function_boundaries boundaries;
  int boundaries_status;
};

void ppc_print_boundaries (struct ppc_function_boundaries * bounds);
void ppc_print_properties (struct ppc_function_properties * props);

CORE_ADDR ppc_parse_instructions (CORE_ADDR start, CORE_ADDR end,
                                  struct ppc_function_properties * props);

void ppc_clear_function_boundaries_request (struct ppc_function_boundaries_request * request);
void ppc_clear_function_boundaries (struct ppc_function_boundaries * boundaries);
void ppc_clear_function_properties (struct ppc_function_properties * properties);

int ppc_find_function_boundaries (struct ppc_function_boundaries_request * request, struct ppc_function_boundaries * reply);

struct ppc_function_boundaries *ppc_frame_function_boundaries (struct
                                                               frame_info
                                                               *frame,
                                                               void
                                                               **this_cache);

struct ppc_function_properties *ppc_frame_function_properties (struct
                                                               frame_info
                                                               *frame,
                                                               void
                                                               **this_cache);

CORE_ADDR *ppc_frame_saved_regs (struct frame_info *next_frame,
                                 void **this_cache);

int safe_read_memory_unsigned_integer (CORE_ADDR addr, int len,
                                       ULONGEST * val);

#endif /* __GDB_PPC_MACOSX_FRAMEINFO_H__ */
