#include "ppc-reg.h"

#include "ppc-frameinfo.h"
#include "ppc-tdep.h"

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "target.h"
#include "gdbcore.h"
#include "symfile.h"
#include "objfiles.h"
#include "command.h"

/* external definitions (should be provided by gdb) */

extern struct obstack frame_cache_obstack;
extern struct frame_info *parse_frame_specification PARAMS ((char *frame_exp));

/* local definitions */


#define	DEFAULT_LR_SAVE 8

/* utility functions */

inline int SIGNED_SHORT (long x)
{
  if (sizeof (short) == 2) {
    return ((short) x);
  } else {
    return (((x & 0xffff) ^ 0x8000) - 0x8000);
  }
}

inline int GET_SRC_REG (long x) 
{
  return (x >> 21) & 0x1f;
}

/* function definitions */

void
ppc_print_boundaries (bounds)
     ppc_function_boundaries *bounds;
{
  if (bounds->prologue_start != INVALID_ADDRESS) {
    printf_filtered 
      (" The function prologue begins at 0x%lx.\n", bounds->prologue_start);
  }
  if (bounds->body_start != INVALID_ADDRESS) {
    printf_filtered 
      (" The function body begins at 0x%lx.\n", bounds->body_start);
  }
  if (bounds->epilogue_start != INVALID_ADDRESS) {
    printf_filtered 
      (" The function epilogue begins at 0x%lx.\n", bounds->epilogue_start);
  }
  if (bounds->function_end != INVALID_ADDRESS) {
    printf_filtered 
      (" The function ends at 0x%lx.\n", bounds->function_end);
  }
}

void
ppc_print_properties (props)
     ppc_function_properties *props;
{
  if (props->frameless) {
    printf_filtered (" No stack frame has been allocated.\n");
  } else {
    printf_filtered (" A stack frame has been allocated.\n");
  }
  if (props->alloca_reg >= 0) {
    printf_filtered 
      (" The stack pointer has been saved by alloca() in r%d.\n",
       props->alloca_reg);
  }
  if (props->offset < 0) {
    printf_filtered (" No registers have been saved.\n");
  } else {
    if (props->offset >= 0) {
      printf_filtered 
	(" %d bytes of integer and floating-point registers have been saved:\n",
	 props->offset);
    }
    if (props->saved_gpr >= 0) {
      printf_filtered
	(" General-purpose registers r%d--r%d have been saved at offset 0x%x.\n", 
	 props->saved_gpr, 31, props->gpr_offset);
    } else {
      printf_filtered (" No general-purpose registers have been saved.\n");
    } 
    if (props->saved_fpr >= 0) {
      printf_filtered
	(" Floating-point registers r%d--r%d have been saved at offset 0x%x.\n",
	 props->saved_gpr, 31, props->fpr_offset);
    } else {
      printf_filtered (" No floating-point registers have been saved.\n");
    } 
  }
  if (props->lr_saved) {
    printf_filtered 
      (" The link register has been saved at offset 0x%x.\n", props->lr_offset);
  }
  if (props->cr_saved) {
    printf_filtered 
      (" The condition register has been saved at offset 0x%x.\n",
       props->cr_offset);
  }
}

/* return pc value after skipping a function prologue and also return
   information about a function frame. */

CORE_ADDR
ppc_parse_instructions (start, end, props)
     CORE_ADDR start;
     CORE_ADDR end;
     ppc_function_properties *props;
{
  CORE_ADDR pc;
  int insn_count = 1; /* Some patterns occur in a particular order, so I 
                         keep the instruction count so I can match them
                         with more certainty. */
  int could_be_save_world = 0;

  CHECK_FATAL (props != NULL);
  ppc_clear_function_properties (props);

  CHECK_FATAL (start != INVALID_ADDRESS);
  /* instructions must be word-aligned */
  CHECK_FATAL ((start % 4) == 0);   
  /* instructions must be word-aligned */
  CHECK_FATAL ((end == INVALID_ADDRESS) || (end % 4) == 0);	
  CHECK_FATAL ((end >= start) || (end == INVALID_ADDRESS));

  for (pc = start; (end == INVALID_ADDRESS) || (pc < end); 
       pc += 4, insn_count++) {

    unsigned long op = read_memory_unsigned_integer (pc, 4);

    /* Check to see if we are seeing a prolog using the save_world
       routine to store away registers.  The pattern will be:

         mflr    r0
         li      r11,stack_size
         bl      <save_world>

       and it will occur at the very beginning of the prolog (we will set
       could_be_save_world if we see mflr r0 as the first insn.)
    */

    if (could_be_save_world)
      {
	/* li r11,stack_size */ 
	if ((op & 0xffff0000) == 0x39600000) 
	  {
	    CORE_ADDR next_pc = pc + 4;
	    unsigned long next_op = read_memory_unsigned_integer (next_pc, 4);
	    if ((next_op & 0xfc000003) == 0x48000001) /* bl <save_world> */
	      {
		/* Look up the pc you are branching to, and make
		   sure it is save_world.  The instruction in the bl
		   is bits 6-31, with the last two bits zeroed, sign extended and
		   added to the pc. */

		struct objfile *objfile;
		struct obj_section *osect;
		asection *sect;
		struct minimal_symbol *msymbol;
		CORE_ADDR branch_target = (next_op & 0x03fffffc), sect_addr;

		if ((branch_target & 0x02000000) == 0x02000000)
		  branch_target |= 0xfc000000;
		
		branch_target += next_pc;

		ALL_OBJSECTIONS (objfile, osect)
		  {
		    sect = osect->the_bfd_section;
		    sect_addr = overlay_mapped_address (branch_target, sect);

		    if (osect->addr <= sect_addr && sect_addr < osect->endaddr &&
			(msymbol = lookup_minimal_symbol_by_pc_section (sect_addr, sect)))
		      if (strcmp (SYMBOL_SOURCE_NAME (msymbol), "save_world"))
			{
			  could_be_save_world == 0;
			}
		  }

		if (could_be_save_world) {

		  /* save_world currently saves all the volatile registers,
		     and saves $lr & $cr on the stack in the usual place.
		     if gcc changes, this needs to be updated as well. */
		  
		  props->frameless = 0;
		  props->alloca_saved = 0;
		  
		  props->lr_saved = 1;
		  props->lr_offset = 8;
		  props->lr_reg = 0;
		  
		  props->cr_saved = 1;
		  props->cr_offset = 4;
		  props->cr_reg = 0;
		  
		  props->saved_gpr = 13;
		  props->gpr_offset = -220;
		  
		  props->saved_fpr = 14;
		  props->fpr_offset = -144;
		  
		  break;
		}
	      }
	    else
	      could_be_save_world = 0;
	  }
      }

    if ((op & 0xfc1fffff) == 0x7c0802a6) { /* mflr Rx */
      props->lr_reg = (op & 0x03e00000) | 0x90010000;
      if (insn_count == 1)
	could_be_save_world = 1;
      continue;
    } else if ((op & 0xfc1fffff) == 0x7c000026) { /* mfcr Rx */
      props->cr_reg = (op & 0x03e00000) | 0x90010000;
      continue;

    } else if ((op & 0xfc1f0000) == 0xd8010000) { /* stfd Rx,NUM(r1) */
      int reg = GET_SRC_REG (op);
      if ((props->saved_fpr == -1) || (props->saved_fpr > reg)) {
	props->saved_fpr = reg;
	props->fpr_offset = SIGNED_SHORT (op) + props->offset2;
      }
      continue;

    } else if (((op & 0xfc1f0000) == 0xbc010000) || /* stm Rx, NUM(r1) */
	       ((op & 0xfc1f0000) == 0x90010000 && /* st rx,NUM(r1), rx >= r13 */
		(op & 0x03e00000) >= 0x01a00000)) {
      int reg = GET_SRC_REG (op);
      if ((props->saved_gpr == -1) || (props->saved_gpr > reg)) {
	props->saved_gpr = reg;
	props->gpr_offset = SIGNED_SHORT (op) + props->offset2;
      }
      continue;

    } else if ((op & 0xffff0000) == 0x3c000000) { /* addis 0,0,NUM, used for >= 32k frames */
      props->offset = (op & 0x0000ffff) << 16;
      props->frameless = 0;
      continue;

    } else if ((op & 0xffff0000) == 0x60000000) { /* ori 0,0,NUM, 2nd half of >= 32k frames */
      props->offset |= (op & 0x0000ffff);
      props->frameless = 0;
      continue;

    } else if ((op & 0xffff0000) == props->lr_reg) { /* st Rx,NUM(r1) where Rx == lr */
      props->lr_saved = 1;
      props->lr_offset = SIGNED_SHORT (op) + props->offset2;
      props->lr_reg = 0;
      continue;

    } else if ((op & 0xffff0000) == props->cr_reg) { /* st Rx,NUM(r1) where Rx == cr */
      props->cr_saved = 1;
      props->cr_offset = SIGNED_SHORT (op) + props->offset2;
      props->cr_reg = 0;
      continue;

    } else if (op == 0x48000005) { /* bl .+4 used in -mrelocatable */
      continue;

    } else if (op == 0x48000004) { /* b .+4 (xlc) */
      break;

    } else if (((op & 0xffff0000) == 0x801e0000 || /* lwz 0,NUM(r30), used in V.4 -mrelocatable */
		op == 0x7fc0f214) && /* add r30,r0,r30, used in V.4 -mrelocatable */
	       props->lr_reg == 0x901e0000) {
      continue;

    } else if ((op & 0xffff0000) == 0x3fc00000 || /* addis 30,0,foo@ha, used in V.4 -mminimal-toc */
	       (op & 0xffff0000) == 0x3bde0000) { /* addi 30,30,foo@l */
      continue;

    } else if ((op & 0xfc000000) == 0x48000000) { /* bl foo, to save fprs??? */

      /* Don't skip over the subroutine call if it is not within the first
	 three instructions of the prologue, or if it is the FIRST instruction (since
         then we are probably just looking at a bl in ordinary code... 
      */

      if ((pc - start) > 8 || pc == start) {
	break;
      }

      op = read_memory_unsigned_integer (pc + 4, 4);

      /* At this point, make sure this is not a trampoline function
	 (a function that simply calls another functions, and nothing else).
	 If the next is not a nop, this branch was part of the function
	 prologue. */

      if (op == 0x4def7b82 || op == 0) { /* crorc 15, 15, 15 */
	break;			/* don't skip over this branch */
      }

      continue;

      /* update stack pointer */
    } else if ((op & 0xffff0000) == 0x94210000) { /* stu r1,NUM(r1) */
      props->frameless = 0;
      props->offset = SIGNED_SHORT (op);
      props->offset2 = props->offset;
      continue;

    } else if (op == 0x7c21016e) { /* stwux 1,1,0 */
      props->frameless = 0;
      props->offset2 = props->offset;
      continue;

      /* load up minimal toc pointer */
    } else if ((op >> 22) == 0x20f
	       && ! props->minimal_toc_loaded) { /* l r31,... or l r30,... */
      props->minimal_toc_loaded = 1;
      continue;

      /* store parameters in stack */
    } else if ((op & 0xfc1f0000) == 0x90010000 || /* st rx,NUM(r1) */
	       (op & 0xfc1f0000) == 0xd8010000 || /* stfd Rx,NUM(r1) */
	       (op & 0xfc1f0000) == 0xfc010000) { /* frsp, fp?,NUM(r1) */
      continue;

      /* store parameters in stack via frame pointer */
    } else if (props->alloca_saved &&
	       ((op & 0xfc1f0000) == 0x901f0000 || /* st rx,NUM(r1) */
		(op & 0xfc1f0000) == 0xd81f0000 || /* stfd Rx,NUM(r1) */
		(op & 0xfc1f0000) == 0xfc1f0000)) { /* frsp, fp?,NUM(r1) */
      continue;

      /* Set up frame pointer */
    } else if (op == 0x603f0000	/* oril r31, r1, 0x0 */
	       || op == 0x7c3f0b78) { /* mr r31, r1 */
      props->alloca_saved = 1;
      props->alloca_reg = 31;
      continue;

      /* Set up frame pointer (this time check for r30) */
    } else if (op == 0x603e0000	/* oril r31, r1, 0x0 */
	       || op == 0x7c3e0b78) { /* mr r31, r1 */
      props->alloca_saved = 1;
      props->alloca_reg = 30;
      continue;

      /* Another way to set up the frame pointer.  */
    } else if ((op & 0xfc1fffff) == 0x38010000) { /* addi rX, r1, 0x0 */
      props->alloca_saved = 1;
      props->alloca_reg = (op & ~0x38010000) >> 21;
      continue;

      /* unknown instruction */
    } else {
      break;
    }
  }

#if 0
/* I have problems with skipping over __main() that I need to address
 * sometime. Previously, I used to use misc_function_vector which
 * didn't work as well as I wanted to be.  -MGO */

  /* If the first thing after skipping a prolog is a branch to a function,
     this might be a call to an initializer in main(), introduced by gcc2.
     We'd like to skip over it as well. Fortunately, xlc does some extra
     work before calling a function right after a prologue, thus we can
     single out such gcc2 behaviour. */
     

  if ((op & 0xfc000001) == 0x48000001) { /* bl foo, an initializer function? */
    op = read_memory_unsigned_integer (pc+4, 4);

    if (op == 0x4def7b82) {		/* cror 0xf, 0xf, 0xf (nop) */

      /* check and see if we are in main. If so, skip over this initializer
         function as well. */

      tmp = find_pc_misc_function (pc);
      if (tmp >= 0 && STREQ (misc_function_vector [tmp].name, "main"))
        return pc + 8;
    }
  }
#endif /* 0 */
 
  if (props->offset != -1) { props->offset = -props->offset; }
  
  return pc;
}

void
ppc_clear_function_boundaries_request (request)
     ppc_function_boundaries_request *request;
{
  request->min_start = INVALID_ADDRESS;
  request->max_end = INVALID_ADDRESS;

  request->contains_pc = INVALID_ADDRESS;

  request->prologue_start = INVALID_ADDRESS;
  request->body_start = INVALID_ADDRESS;
  request->epilogue_start = INVALID_ADDRESS;
  request->function_end = INVALID_ADDRESS;
}

void
ppc_clear_function_boundaries (boundaries)
     ppc_function_boundaries *boundaries;
{
  boundaries->prologue_start = INVALID_ADDRESS;
  boundaries->body_start = INVALID_ADDRESS;
  boundaries->epilogue_start = INVALID_ADDRESS;
  boundaries->function_end = INVALID_ADDRESS;
}

void
ppc_clear_function_properties (properties)
     ppc_function_properties *properties;
{
  properties->offset = -1;
  properties->offset2 = 0;

  properties->saved_gpr = -1;
  properties->saved_fpr = -1;
  properties->gpr_offset = 0;
  properties->fpr_offset = 0;

  properties->alloca_reg = -1;
  properties->frameless = 1;

  properties->lr_saved = 0;
  properties->lr_offset = -1;
  properties->lr_reg = -1;

  properties->cr_saved = 0;
  properties->cr_offset = -1;
  properties->cr_reg = -1;

  properties->minimal_toc_loaded = 0;
}


int
ppc_find_function_boundaries (request, reply)
     ppc_function_boundaries_request *request;
     ppc_function_boundaries *reply;
{
  ppc_function_properties props;

  CHECK_FATAL (request != NULL);
  CHECK_FATAL (reply != NULL);

  if (request->prologue_start != INVALID_ADDRESS) {
    reply->prologue_start = request->prologue_start;
  } else if (request->contains_pc != INVALID_ADDRESS) {
    reply->prologue_start = get_pc_function_start (request->contains_pc);
    if (reply->prologue_start == 0) { return -1; }
  }

  CHECK_FATAL (reply->prologue_start != INVALID_ADDRESS);

  if ((reply->prologue_start % 4) != 0) { return -1; }
  reply->body_start = ppc_parse_instructions (reply->prologue_start, INVALID_ADDRESS, &props);

  return 0;
}

int
ppc_frame_cache_boundaries (frame, retbounds)
     struct frame_info *frame;
     struct ppc_function_boundaries *retbounds;
{
  if (! frame->extra_info->bounds) {

    if (ppc_is_dummy_frame (frame)) {

      frame->extra_info->bounds = (struct ppc_function_boundaries *)
	obstack_alloc (&frame_cache_obstack, sizeof (ppc_function_boundaries));
      CHECK_FATAL (frame->extra_info->bounds != NULL);
      
      frame->extra_info->bounds->prologue_start = INVALID_ADDRESS;
      frame->extra_info->bounds->body_start = INVALID_ADDRESS;
      frame->extra_info->bounds->epilogue_start = INVALID_ADDRESS;
      frame->extra_info->bounds->function_end = INVALID_ADDRESS;

    } else {

      ppc_function_boundaries_request request;
      ppc_function_boundaries lbounds;
      int ret;
  
      ppc_clear_function_boundaries (&lbounds);
      ppc_clear_function_boundaries_request (&request);

      request.contains_pc = frame->pc;
      ret = ppc_find_function_boundaries (&request, &lbounds);
      if (ret != 0) { return ret; }
    
      frame->extra_info->bounds = (struct ppc_function_boundaries *)
	obstack_alloc (&frame_cache_obstack, sizeof (ppc_function_boundaries));
      CHECK_FATAL (frame->extra_info->bounds != NULL);
      memcpy (frame->extra_info->bounds, &lbounds, sizeof (ppc_function_boundaries));
    }
  }

  if (retbounds != NULL) { 
    memcpy (retbounds, frame->extra_info->bounds, sizeof (ppc_function_boundaries));
  }

  return 0;
}

int
ppc_frame_cache_properties (frame, retprops)
     struct frame_info *frame;
     struct ppc_function_properties *retprops;
{
  /* FIXME: I have seen a couple of cases where this function gets called with a frame whose
     extra_info has not been set yet.  It is always when the stack is mauled, and you try
     to run "up" or some other such command, so I am not sure we can really recover well,
     but at some point it might be good to look at this more closely. */

  CHECK (frame->extra_info);

  if (! frame->extra_info->props) {

    if (ppc_is_dummy_frame (frame)) {

      ppc_function_properties *props;

      frame->extra_info->props = (struct ppc_function_properties *)
	obstack_alloc (&frame_cache_obstack, sizeof (ppc_function_properties));
      CHECK_FATAL (frame->extra_info->props != NULL);
      ppc_clear_function_properties (frame->extra_info->props);
      props = frame->extra_info->props;

      props->offset = 0;
      props->saved_gpr = -1;
      props->saved_fpr = -1;
      props->gpr_offset = -1;
      props->fpr_offset = -1;
      props->frameless = 0;
      props->alloca_saved = 0;
      props->alloca_reg = -1;
      props->lr_saved = 1;
      props->lr_offset = DEFAULT_LR_SAVE;
      props->lr_reg = -1;
      props->cr_saved = 0;
      props->cr_offset = -1;
      props->cr_reg = -1;
      props->minimal_toc_loaded = 0;

    } else {

      int ret;
      ppc_function_properties lprops;

      ppc_clear_function_properties (&lprops);

      ret = ppc_frame_cache_boundaries (frame, NULL);
      if (ret != 0) { return ret; }

      if ((frame->extra_info->bounds->prologue_start % 4) != 0) { return -1; }
      if ((frame->pc % 4) != 0) { return -1; }
      ppc_parse_instructions (frame->extra_info->bounds->prologue_start, frame->pc, &lprops);
  
      frame->extra_info->props = (struct ppc_function_properties *)
	obstack_alloc (&frame_cache_obstack, sizeof (ppc_function_properties));
      CHECK_FATAL (frame->extra_info->props != NULL);
      memcpy (frame->extra_info->props, &lprops, sizeof (ppc_function_properties));
    }
  }

  if (retprops != NULL) {
    memcpy (retprops, frame->extra_info->props, sizeof (ppc_function_properties));
  }

  return 0;    
}
