#include "ppc-reg.h"
#include "ppc-frameops.h"

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "target.h"
#include "gdbcore.h"
#include "symfile.h"
#include "objfiles.h"

#include "ppc-frameinfo.h"
#include "ppc-tdep.h"

#include <string.h>

/* external definitions (should be provided by gdb) */

extern struct obstack frame_cache_obstack;
struct type *check_typedef PARAMS ((struct type *type));

/* static function declarations */

static void ppc_pop_dummy_frame PARAMS (());

static void ppc_fill_region PARAMS
  ((CORE_ADDR addr, CORE_ADDR nwords, unsigned long value));

static void ppc_frame_dummy_saved_regs PARAMS 
  ((struct frame_info *frame, CORE_ADDR *saved_regs));

extern int ppc_debug (const char *fmt, ...);

#define	DEFAULT_LR_SAVE 8	/* Default offset from SP where the LR is stored */
#define LINK_AREA_SIZE 24

#define MINIMUM_RECOMMENDED_STACK_ALIGNMENT 16
#define MINIMUM_STACK_ALIGNMENT 8
#define MINIMUM_INSTRUCTION_ALIGNMENT 4
#define MINIMUM_FRAME_SIZE 64	/* 24 bytes link area + 40 to 8-word-align stack */

#define ROUND_UP(x, n) (((x + (n - 1)) / n) * n)

#define DUMMY_REG_OFFSET \
  (MINIMUM_FRAME_SIZE + ROUND_UP (REGISTER_BYTES, MINIMUM_RECOMMENDED_STACK_ALIGNMENT) - LINK_AREA_SIZE)

/* Be careful! If the stack pointer is not decremented first, then
   the kernel thinks it is free to use the space underneath it. And
   kernel actually uses that area for IPC purposes when executing
   ptrace(2) calls. So before writing register values into the new frame,
   decrement and update %sp first in order to secure your frame. */

/* function definitions */

static void
ppc_fill_region (addr, nwords, value)
     CORE_ADDR addr;
     CORE_ADDR nwords;
     unsigned long value;
{
  CORE_ADDR i;
  unsigned char *buf;
  
  unsigned int pagesize = 8192; /* child_get_pagesize (); */
  unsigned int pagewords = pagesize / 4;

  unsigned int bufwords = nwords;
  if (bufwords > pagewords) { bufwords = pagewords; }
  
  CHECK_FATAL ((pagesize % 4) == 0);
  CHECK_FATAL ((addr % 4) == 0);

  buf = (unsigned char *) alloca (bufwords * 4 * sizeof (char));
  CHECK_FATAL (buf != NULL);

  for (i = 0; (i < bufwords); i++) {
    store_unsigned_integer (buf + (i * 4), 4, value);
  }
  
  i = 0;
  while (i < nwords) {
    unsigned int curwords = (nwords - i);
    if (curwords > bufwords) { curwords = bufwords; }

    write_memory (addr + (i * 4), buf, (curwords * 4));
    i += curwords;
  }
}

/* Push a dummy frame into stack, saving all registers.  Currently we
   save only the standard special-purpose registers, which may not be
   good enough.  FIXME. */

void
ppc_push_dummy_frame ()
{
  CORE_ADDR newsp;		/* address of new stack frame */
  CORE_ADDR start;		/* address of local storage for registers */
  CORE_ADDR sp;			/* address of original stack pointer */
  CORE_ADDR pc;
  char buf[4];
  /* to save registers[] while we muck about in the inferior */
  unsigned char rdata [REGISTER_BYTES];	

  /* save a copy of the inferior register data */
  target_fetch_registers (-1);
  memcpy (rdata, registers, REGISTER_BYTES);

  sp = read_sp ();
  ppc_debug ("ppc_push_dummy_frame: initial sp is 0x%lx\n", (unsigned long) sp);
  ppc_debug ("ppc_push_dummy_frame: initial *sp is 0x%lx\n", 
	     (unsigned long) read_memory_unsigned_integer (sp, 4));
  if ((sp % MINIMUM_RECOMMENDED_STACK_ALIGNMENT) != 0) {
    warning ("stack pointer (0x%lx) is not aligned to a %d-byte boundary.",
	   (unsigned long) sp, MINIMUM_RECOMMENDED_STACK_ALIGNMENT);
  }
  if ((sp % MINIMUM_STACK_ALIGNMENT) != 0) {
    error ("Unable to push dummy stack frame for function call: "
	   "stack pointer (0x%lx) is not aligned to a %d-byte boundary.",
	   (unsigned long) sp, MINIMUM_STACK_ALIGNMENT);
  }

  pc = read_pc ();
  if ((pc % MINIMUM_INSTRUCTION_ALIGNMENT) != 0) {
    error ("Unable to push dummy stack frame for function call: "
	   "program counter (0x%lx) is not aligned to a %d-byte boundary.",
	   (unsigned long) pc, MINIMUM_INSTRUCTION_ALIGNMENT);
  }
  
  /* update %sp register first --- see Note 1 at top of file */

  /* FIXME: We don't check if the stack really has this much space.
     This is a problem on the ppc simulator (which only grants one page
     (4096 bytes) by default.  */

  newsp = sp - MINIMUM_FRAME_SIZE;
  CHECK_FATAL ((newsp % MINIMUM_STACK_ALIGNMENT) == 0);
  write_sp (newsp);

  /* save pc */
  store_address (buf, 4, pc);
  write_memory (sp + DEFAULT_LR_SAVE, buf, 4);

  /* clear new stack frame */
  ppc_fill_region (newsp, MINIMUM_FRAME_SIZE / 4, 0xbeefbeef);

  /* write stack chain */
  store_address (buf, 4, sp);
  write_memory (newsp, buf, 4);
  
  /* store register state */
  ppc_debug ("ppc_push_dummy_frame: allocating stack space for storing registers\n");
  ppc_stack_alloc (&newsp, &start, 0, REGISTER_BYTES);

  /* we assume this location in ppc_pop_dummy_frame () */
  CHECK_FATAL (start == (sp - DUMMY_REG_OFFSET));
  ppc_debug ("ppc_push_dummy_frame: saving registers into range from 0x%lx to 0x%lx\n", 
	     (unsigned long) start, (unsigned long) (start + REGISTER_BYTES));
  write_memory (start, rdata, REGISTER_BYTES);

  ppc_debug ("ppc_push_dummy_frame: stack pointer for dummy frame is 0x%lx\n", 
	     (unsigned long) newsp);

  /* need to update current_frame or do_registers_info() et. al. will break */
  flush_cached_frames ();
}

/* pop a dummy frame */
   
static void ppc_pop_dummy_frame ()
{
  struct frame_info *frame;
  struct frame_info *prev;

  frame = get_current_frame ();
  CHECK_FATAL (frame != NULL);

  prev = get_prev_frame (frame);
  CHECK_FATAL (prev != NULL);

  /* read register data */
  ppc_debug ("pop_dummy_frame: restoring registers from range from 0x%lx to 0x%lx\n", 
	     (unsigned long) (prev->frame - DUMMY_REG_OFFSET), (unsigned long) prev->frame);
  read_memory (prev->frame - DUMMY_REG_OFFSET, registers, REGISTER_BYTES);

  target_store_registers (-1);
  flush_cached_frames ();
}

/* pop the innermost frame from the stack, restoring all saved registers.  */

void
ppc_pop_frame ()
{
  struct frame_info *frame;
  struct frame_info *prev;
  int i;

  frame = get_current_frame ();
  CHECK_FATAL (frame != NULL);

  prev = get_prev_frame (frame);
  CHECK_FATAL (prev != NULL);

  if (ppc_is_dummy_frame (frame)) {
    ppc_pop_dummy_frame ();
    return;
  }
  
  /* ensure that all registers are valid.  */
  target_fetch_registers (-1);

  ppc_frame_cache_saved_regs (frame);

  for (i = 0; i < NUM_REGS; i++) {
    if (frame->saved_regs[i] != 0) {
      read_memory (frame->saved_regs[i], &registers [REGISTER_BYTE (i)], REGISTER_RAW_SIZE (i));
    }
  }

  write_register (PC_REGNUM, prev->pc);
  write_register (SP_REGNUM, prev->frame);

  target_store_registers (-1);
  flush_cached_frames ();
}

/* Fix up the call sequence of a dummy function with the real function
   address.  Its arguments will be passed by gdb. */

/* Insert the specified number of args and function address
   into a call sequence of the above form stored at DUMMYNAME.  */

void
ppc_fix_call_dummy (char *dummy, CORE_ADDR pc, CORE_ADDR addr, int nargs, struct value **args, struct type *type, int gcc_p)
{
  int i;
  CORE_ADDR tocvalue;

  tocvalue = ppc_find_toc_address (addr);

  i = *(int*)((char*)dummy + TOC_ADDR_OFFSET);
  i = (i & 0xffff0000) | (tocvalue >> 16);
  *(int*)((char*)dummy + TOC_ADDR_OFFSET) = i;

  i = *(int*)((char*)dummy + TOC_ADDR_OFFSET + 4);
  i = (i & 0xffff0000) | (tocvalue & 0x0000ffff);
  *(int*)((char*)dummy + TOC_ADDR_OFFSET + 4) = i;

  i  = *(int*)((char*)dummy + TARGET_ADDR_OFFSET);
  i = (i & 0xffff0000) | (addr >> 16);
  *(int*)((char*)dummy + TARGET_ADDR_OFFSET) = i;

  i = *(int*)((char*)dummy + TARGET_ADDR_OFFSET + 4);
  i = (i & 0xffff0000) | (addr & 0x0000ffff);
  *(int*)((char*)dummy + TARGET_ADDR_OFFSET + 4) = i;
}

/* Pass the arguments in either registers, or in the stack. In RS6000,
   the first eight words of the argument list (that might be less than
   eight parameters if some parameters occupy more than one word) are
   passed in r3..r11 registers.  float and double parameters are
   passed in fpr's, in addition to that. Rest of the parameters if any
   are passed in user stack. There might be cases in which half of the
   parameter is copied into registers, the other half is pushed into
   stack.

   If the function is returning a structure, then the return address is passed
   in r3, then the first 7 words of the parametes can be passed in registers,
   starting from r4. */

CORE_ADDR
ppc_push_arguments (nargs, args, sp, struct_return, struct_addr)
     int nargs;
     struct value **args;
     CORE_ADDR sp;
     int struct_return;
     CORE_ADDR struct_addr;
{
  unsigned int arg_bytes_required = 0;
  unsigned int cur_offset = 0;
  unsigned int cur_gpr = 0;
  unsigned int cur_fpr = 0;

  unsigned int argno;

  unsigned int nfargs = UINT_MAX;
  struct value **fargs = NULL;

  /* if struct return address is supplied, add it to the beginning of
     the arglist as a pseudo-argument */

  if (struct_return) {
    struct type *voidptr_type = lookup_pointer_type (builtin_type_void);
    fargs = (struct value **) alloca (nargs * (sizeof (struct value *) + 1));
    fargs[0] = value_from_longest (voidptr_type, struct_addr);
    memcpy (fargs + 1, args, nargs * sizeof (struct value *));
    nfargs = nargs + 1;
  } else {
    fargs = (struct value **) alloca (nargs * sizeof (struct value *));
    memcpy (fargs, args, nargs * sizeof (struct value *));
    nfargs = nargs;
  }
    
  /* float arguments are stored as doubles even if the function is
     prototyped to take a float */

  for (argno = 0; argno < nfargs; argno++) {
    value_ptr arg = fargs[argno];
    struct type *type = check_typedef (VALUE_TYPE (arg));

    if (TYPE_CODE (type) == TYPE_CODE_FLT) {
      if (TYPE_LENGTH (type) < TYPE_LENGTH (builtin_type_double)) {
	fargs[argno] = value_cast (builtin_type_double, arg); 
      }
    }
  }
    
  arg_bytes_required = 0;
  for (argno = 0; argno < nfargs; argno++) {
    value_ptr arg = fargs[argno];
    struct type *type = check_typedef (VALUE_TYPE (arg));
    unsigned int len = TYPE_LENGTH (type);
    if (TYPE_CODE (type) == TYPE_CODE_FLT) {
      if (TYPE_LENGTH (type) != 8) {
	/* currently only doubles are supported (floats should have
           been cast to doubles already) */
	error ("invalid TYPE_LENGTH %u for argument %u", len, argno);
      }
      arg_bytes_required += 8;
    } else {
      arg_bytes_required += TYPE_LENGTH (type);
    }
  }

  if (arg_bytes_required > 32) { 
    error ("function arguments too large (must be less than 32 bytes)");
  }

  cur_offset = 0;
  cur_gpr = 3;
  cur_fpr = 1;
  for (argno = 0; argno < nfargs; argno++) {

    value_ptr arg = fargs[argno];
    struct type *type = check_typedef (VALUE_TYPE (arg));
    int len = TYPE_LENGTH (type);

    if (TYPE_CODE (type) == TYPE_CODE_FLT) {

      if (len != 8) {
	/* currently only doubles are supported (floats should have
           been cast to doubles already) */
	error ("invalid TYPE_LENGTH %u for argument %u", len, argno);
      }
      CHECK_FATAL (cur_gpr <= 10);
      memcpy (&registers[REGISTER_BYTE (FP0_REGNUM + cur_gpr)], VALUE_CONTENTS (arg), len);
      CHECK_FATAL (cur_fpr <= 13);
      memcpy (&registers[REGISTER_BYTE (FP0_REGNUM + cur_fpr)], VALUE_CONTENTS (arg), len);
      cur_fpr++;
      cur_gpr++;

    } else {

      int argbytes = 0;
      while (argbytes < len) {
	*((int*) &registers[REGISTER_BYTE (GP0_REGNUM + cur_gpr)]) = 0;
	CHECK_FATAL (cur_gpr <= 10);
	memcpy (&registers[REGISTER_BYTE (GP0_REGNUM + cur_gpr)], 
		((char *) VALUE_CONTENTS (arg)) + argbytes, 
		(len - argbytes) > 4 ? 4 : len - argbytes);
	cur_gpr++;
	argbytes += 4;
      }

    }
  }

  ppc_debug ("ppc_push_arguments: new stack pointer is 0x%lx\n", (unsigned long) sp);

  target_store_registers (-1);
  return sp;
}

void
ppc_stack_alloc (sp, start, argsize, len)
     CORE_ADDR *sp;
     CORE_ADDR *start;
     size_t argsize;
     size_t len;
{
  char *linkbuf;
  size_t space = ROUND_UP (len, MINIMUM_RECOMMENDED_STACK_ALIGNMENT);

  if ((*sp % MINIMUM_RECOMMENDED_STACK_ALIGNMENT) != 0) {
    warning ("stack pointer (0x%lx) is not aligned to a %d-byte boundary.",
	     (unsigned long) *sp, MINIMUM_RECOMMENDED_STACK_ALIGNMENT);
  }
  if ((*sp % MINIMUM_STACK_ALIGNMENT) != 0) {
    error ("Unable to allocate space on stack of inferior: "
	   "stack pointer (0x%lx) is not aligned to a %d-byte boundary.",
	   (unsigned long) *sp, MINIMUM_STACK_ALIGNMENT);
  }

  /* save link and argument area */ 
  linkbuf = alloca (LINK_AREA_SIZE + argsize);
  if (linkbuf == NULL) { 
    fprintf (stderr, "ppc_stack_alloc: unable to alloc space to store link area\n");
    abort ();
  }
  read_memory (*sp, linkbuf, LINK_AREA_SIZE + argsize);
  
  /* first expand stack frame --- see note 1 */
  *sp = *sp - space;		
  CHECK_FATAL ((*sp % MINIMUM_STACK_ALIGNMENT) == 0);
  write_register (SP_REGNUM, *sp);
  
  /* zero new stack region */
  ppc_fill_region (*sp, space / 4, 0xfeebfeeb);

  /* store pointer to previous stack frame */
  write_memory (*sp, linkbuf, LINK_AREA_SIZE + argsize);

  /* available space is right after link area */
  *start = *sp + LINK_AREA_SIZE;

  ppc_debug ("ppc_stack_alloc: allocated 0x%lx bytes from 0x%lx to 0x%lx\n",
	     (unsigned long) space, (unsigned long) *start, (unsigned long) (*start + space));
  ppc_debug ("ppc_stack_alloc: new stack pointer is 0x%lx\n", (unsigned long) *sp) ;
  
  return;
}

void
ppc_frame_cache_saved_regs (frame)
     struct frame_info *frame;
{
  if (frame->saved_regs) {
    return;
  }

  frame_saved_regs_zalloc (frame);
    
  ppc_frame_saved_regs (frame, frame->saved_regs);
}

/* Put here the code to store, into a struct frame_saved_regs,
   the addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special:
   the address we return for it IS the sp for the next frame.  */

static void
ppc_frame_dummy_saved_regs (frame, saved_regs)
     struct frame_info *frame;
     CORE_ADDR *saved_regs;
{
  int i;
  struct frame_info *prev;

  CHECK_FATAL (frame != NULL);
  CHECK_FATAL (ppc_is_dummy_frame (frame));
  prev = get_prev_frame (frame);
  CHECK_FATAL (prev != NULL);

  for (i = 0; i < NUM_REGS; i++) {
    saved_regs[i] = prev->frame - DUMMY_REG_OFFSET + REGISTER_BYTE (i);
  }
}

void
ppc_frame_saved_regs (frame, saved_regs)
     struct frame_info *frame;
     CORE_ADDR *saved_regs;
{
  CORE_ADDR prev_sp = 0;
  ppc_function_properties *props;
  int i;

  if (ppc_is_dummy_frame (frame)) {
    ppc_frame_dummy_saved_regs (frame, saved_regs);
    return;
  }

  if (ppc_frame_cache_properties (frame, NULL)) { 
    ppc_debug ("frame_initial_stack_address: unable to find properties of " 
	       "function containing 0x%lx\n", (unsigned long) frame->pc);
    return;
  }    
  props = frame->extra_info->props;
  CHECK_FATAL (props != NULL);  

  /* record stored stack pointer */
  if (! props->frameless) {
    prev_sp = saved_regs[SP_REGNUM] = read_memory_unsigned_integer (frame->frame, 4);
  } else {
    prev_sp = saved_regs[SP_REGNUM] = frame->frame;
  }

  saved_regs[PC_REGNUM] = ppc_frame_saved_pc (frame);

  if (props->cr_saved) {
    saved_regs[CR_REGNUM] = prev_sp + props->cr_offset;
  }
  if (props->lr_saved) {
    saved_regs[LR_REGNUM] = prev_sp + props->lr_offset;
  }

  if (props->frameless && ((props->saved_fpr != -1) || (props->saved_gpr != -1))) {
    ppc_debug ("frame_find_saved_regs: "
	       "registers marked as saved in frameless function; ignoring\n");
    return;
  }

  /* fixme 32x64 'long int offset' */

  if (props->saved_fpr >= 0) {						
    for (i = props->saved_fpr; i < 32; i++) {				
      long int offset = props->fpr_offset + ((i - props->saved_fpr) * sizeof (FP_REGISTER_TYPE));
      saved_regs[FP0_REGNUM + i] = prev_sp + offset;
    }									
  }									
									
  if (props->saved_gpr >= 0) {						
    for (i = props->saved_gpr; i < 32; i++) {				
      long int offset = props->gpr_offset + ((i - props->saved_gpr) * sizeof (REGISTER_TYPE));
      saved_regs[GP0_REGNUM + i] = prev_sp + offset;
    }									
  }									
}
