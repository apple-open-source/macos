/* next.c:  Functions for NeXT as target machine for GNU C compiler.  */

/* Note that the include below means that we can't debug routines in
   i386.c when running on a COFF system.  */

#include "i386/i386.c"
#include "next/nextstep.c"

void
machopic_output_stub (file, symb, stub)
     FILE *file;
     const char *symb, *stub;
{
  unsigned int length;
  char *binder_name, *symbol_name, *lazy_ptr_name;
  static int label = 0;
  label += 1;

  length = strlen(stub);
  binder_name = alloca(length + 32);
  GEN_BINDER_NAME_FOR_STUB(binder_name, stub, length);

  length = strlen(symb);
  symbol_name = alloca(length + 32);
  GEN_SYMBOL_NAME_FOR_SYMBOL(symbol_name, symb, length);

  lazy_ptr_name = alloca(length + 32);
  GEN_LAZY_PTR_NAME_FOR_SYMBOL(lazy_ptr_name, symb, length);

  if (MACHOPIC_PURE)
    machopic_picsymbol_stub_section ();
  else
    machopic_symbol_stub_section ();

  fprintf (file, "%s:\n", stub);
  fprintf (file, "\t.indirect_symbol %s\n", symbol_name);

  if (MACHOPIC_PURE)
    {
      fprintf (file, "\tcall LPC$%d\nLPC$%d:\tpopl %%eax\n", label, label);
      fprintf (file, "\tmovl %s-LPC$%d(%%eax),%%edx\n", lazy_ptr_name, label);
      fprintf (file, "\tjmp %%edx\n");
    }
  else
    {
      fprintf (file, "\tjmp *%s\n", lazy_ptr_name);
    }
  
  fprintf (file, "%s:\n", binder_name);
  
  if (MACHOPIC_PURE)
    {
      fprintf (file, "\tlea %s-LPC$%d(%%eax),%%eax\n", lazy_ptr_name, label);
      fprintf (file, "\tpushl %%eax\n");
    }
  else
    {
      fprintf (file, "\t pushl $%s\n", lazy_ptr_name);
    }

  fprintf (file, "\tjmp dyld_stub_binding_helper\n");

  machopic_lazy_symbol_ptr_section ();
  fprintf (file, "%s:\n", lazy_ptr_name);
  fprintf (file, "\t.indirect_symbol %s\n", symbol_name);
  fprintf (file, "\t.long %s\n", binder_name);
}

i386_finalize_machopic()
{
    extern int current_function_uses_pic_offset_table;
    if (PIC_OFFSET_TABLE_REGNUM >= FIRST_PSEUDO_REGISTER
	&& regno_reg_rtx [PIC_OFFSET_TABLE_REGNUM])
      pic_offset_table_rtx = regno_reg_rtx [PIC_OFFSET_TABLE_REGNUM];

    if (!current_function_uses_pic_offset_table) 
      {
	current_function_uses_pic_offset_table =
	  profile_flag
	    | profile_block_flag
	      | get_pool_size()
		| current_function_uses_const_pool
		  | const_double_used ();

	if (current_function_uses_pic_offset_table) {
	  rtx first_insn = next_real_insn (get_insns());

	  if (!first_insn)
	    return;

	  emit_insn_before(gen_rtx(SET, VOIDmode,
				   pic_offset_table_rtx,
				   gen_rtx(PC, VOIDmode)),
			   first_insn);

	  emit_insn_after(gen_rtx(USE, VOIDmode,
				  pic_offset_table_rtx),
			  get_last_insn());
	}
      }
    else
      {
	rtx first_insn = next_real_insn (get_insns ());
	if (first_insn)
	  emit_insn_before (gen_rtx (USE, VOIDmode, pic_offset_table_rtx),
			    first_insn);
	if (const_double_used ())
	  emit_insn_after (gen_rtx (USE, VOIDmode, pic_offset_table_rtx),
			   get_last_insn ());
      }
}


#ifdef MACHO_PIC
 
/* Reload the pic base register -- in certain cases (e.g., exceptions),
   the control flow is weird enough that the compiler doesn't realize
   that it must re-establish the pic base address for the current
   function.  See reload_ppc_pic_register.

   Called via the wrapper macro RELOAD_PIC_REGISTER.   */

void
reload_i386_pic_register ()
{
  rtx L_retaddr, L_dbg;                 /* labels */
  rtx fn_base, Lret_lrf;                /* label refs */

  /* registers & calculations */
  rtx call_next, push_retaddr, diff;

  int orig_flag_pic = flag_pic;

  if (current_function_uses_pic_offset_table == 0)
    return;

  if (! flag_pic)
    abort ();

  flag_pic = 0;

  L_retaddr = gen_label_rtx ();
  Lret_lrf = gen_rtx (LABEL_REF, VOIDmode, L_retaddr);

  L_dbg = gen_label_rtx ();
  XSTR (L_dbg, 4) = "L_my_debug_label";

  /* generate (or get) the machopic pic base label */
  fn_base = gen_rtx (SYMBOL_REF, VOIDmode, machopic_function_base_name ());


  call_next = gen_rtx (CALL, VOIDmode,
		       gen_rtx (MEM, QImode, Lret_lrf), const0_rtx);

  push_retaddr = gen_rtx (SET, VOIDmode, stack_pointer_rtx,
			  gen_rtx (PLUS, SImode,
				   stack_pointer_rtx, const0_rtx));

  emit_call_insn (gen_rtx (PARALLEL, VOIDmode,
                           gen_rtvec (2, call_next, push_retaddr)));

  LABEL_PRESERVE_P(L_retaddr) = 1;
  emit_label (L_retaddr);

  /* Pop the (pushed return address) into the pic register */
  /*
  emit_insn (gen_rtx (SET, VOIDmode, pic_offset_table_rtx,
		      gen_rtx (PLUS, SImode, pic_offset_table_rtx,
			       gen_rtx (MEM, Pmode, stack_pointer_rtx))));

     The above stuff became a  "addl (%esp,1),%ebx".  Not a pop.
     And the obvious (heh heh) attempt

  emit_insn (gen_rtx (PARALLEL, VOIDmode,
    gen_rtvec (2,
      gen_rtx (SET, VOIDmode, pic_offset_table_rtx,
	       gen_rtx (PLUS, SImode, const0_rtx,
			gen_rtx (MEM, Pmode, stack_pointer_rtx))),
	       gen_rtx (SET, VOIDmode, stack_pointer_rtx,
			gen_rtx (PLUS, SImode, stack_pointer_rtx, 
				 GEN_INT(4))))));

     Got a "unrecognizable insn".  I give up.  Below, we should use
     something like "reg_names[PIC_OFFSET_TABLE_REGNUM]" instead of
     "%ebx".  Need to know how gcc handles strings ...
  */
  emit_insn (gen_rtx (ASM_INPUT, VOIDmode, "popl %ebx"));

  /* Figure out the (constant) difference between "here" and picbase.
     Make it negative so we can add the retn addr to it.  */
  diff = gen_rtx (CONST, SImode, gen_rtx (MINUS, SImode, fn_base, Lret_lrf));

  /* This hopefully becomes a single leal instruction */
  emit_insn (gen_rtx (SET, VOIDmode, pic_offset_table_rtx,
		      gen_rtx (PLUS, Pmode, pic_offset_table_rtx, diff)));

  /* Mark that register used.  (?Again?) */
  emit_insn (gen_rtx (USE, VOIDmode, pic_offset_table_rtx));

  flag_pic = orig_flag_pic;
}
#endif   /*  MACHO_PIC */
