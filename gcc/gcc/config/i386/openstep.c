/* apple.c:  Functions for Mac OS X as target machine for GNU C compiler.  */

/* Note that the include below means that we can't debug routines in
   i386.c when running on a COFF system.  */

#define MAX_386_STACK_LOCALS 3

#include "i386/i386.c"
#include "apple/openstep.c"

#ifndef FIXED_PIC_REG
int pic86_reg_num = 0;
#endif
 
static rtx
lookup_i386_stack_local (mode, n)
  enum machine_mode mode;
  int n;
{
    if (n < 0 || n >= MAX_386_STACK_LOCALS)
      abort ();

    return  i386_stack_locals[(int) mode][n];
}

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

i386_finalize_machopic ()
{
  extern int current_function_uses_pic_offset_table;
#ifndef FIXED_PIC_REG
    const int pic_reg = pic86_reg_num;
  rtx first_insn = next_real_insn (get_insns ());

    if (pic_reg >= FIRST_PSEUDO_REGISTER && regno_reg_rtx [pic_reg])
      pic_offset_table_rtx = regno_reg_rtx [pic_reg];
#endif

  if (!current_function_uses_pic_offset_table)
#ifndef FIXED_PIC_REG
    {
#endif
      current_function_uses_pic_offset_table =
	profile_flag || profile_block_flag || get_pool_size()
	  || current_function_uses_const_pool || const_double_used ();

      if (current_function_uses_pic_offset_table)
#ifndef FIXED_PIC_REG
	{
	  if (!first_insn)
	    return;
	  emit_insn_before (gen_rtx (SET, VOIDmode, pic_offset_table_rtx,
				     gen_rtx (PC, VOIDmode)),
			    first_insn);
	  emit_insn_after (gen_rtx (USE, VOIDmode, pic_offset_table_rtx),
			   get_last_insn ());
	}
    }
  else
    {
	if (first_insn)
	  emit_insn_before (gen_rtx (USE, VOIDmode, pic_offset_table_rtx),
			    first_insn);
#endif
      /* This is necessary. */
	if (const_double_used ())
	  emit_insn_after (gen_rtx (USE, VOIDmode, pic_offset_table_rtx),
			   get_last_insn ());
#ifndef FIXED_PIC_REG
    }
#endif
}

#ifndef FIXED_PIC_REG
static int replace_count, inside_set_rtx;
static
rtx
replace_machopic86_base_refs (rtx x, rtx repl)
{
  register enum rtx_code code;
  register int i;
  register char *fmt;

  if (x == 0)
    return x;

  code = GET_CODE (x);
  switch (code)
    {
    case SCRATCH:
    case PC:
    case CC0:
    case CONST_INT:
    case CONST_DOUBLE:
    case CONST:
    case SYMBOL_REF:
    case LABEL_REF:
    case SUBREG:
      return x;

    case REG:
      /* Only switch registers if we're inside a "(set ...)" rtx.  */
      if (REGNO (x) == pic86_reg_num && inside_set_rtx)
      {
	++replace_count;
	return repl;
      }
      return x;

    case SET:
      ++inside_set_rtx;
      SET_SRC (x) = replace_machopic86_base_refs (SET_SRC (x), repl);
      --inside_set_rtx;
      return x;
    }

  fmt = GET_RTX_FORMAT (code);
  for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
    {
      if (fmt[i] == 'e')
        XEXP (x, i) = replace_machopic86_base_refs (XEXP (x, i), repl);
      else
      if (fmt[i] == 'E')
        {
          register int j;
          for (j = 0; j < XVECLEN (x, i); j++)
            XVECEXP (x, i, j) = 
		replace_machopic86_base_refs (XVECEXP (x, i, j), repl);
        }
    }
  return x;
}


/* fixup_machopic386_pic_base_refs fixes up COUNT insns beginning with
   INSNS so that references to the pic_offset_table_rtx pseudo are
   replaced with the real thing.

   Only SET insns are affected.

   This is because the compiler can't handle reloading this pseudo
   for reasons mightily obscure, but I think caused by the fact that
   "(reg:SI 21)" appears from nowhere and has no mem or const equivalences.

   Note that passing a negative COUNT will fixup all instructions.  */

void
fixup_machopic386_pic_base_refs (rtx insns, int count)
{

  /* Replace all occurrences of "(reg:SI 21)" -- the PIC base register --
     if a register or memory location has been allocated for it.
     I am at a loss as to why the dratted compiler can't do this itself.  */

  if (1 && flag_pic && current_function_uses_pic_offset_table
     && regno_reg_rtx[pic86_reg_num] 
     && ((  GET_CODE (regno_reg_rtx[pic86_reg_num]) != REG
	   || /* it is REG, but a different register */
            REGNO (regno_reg_rtx[pic86_reg_num]) != pic86_reg_num)
        || (reg_renumber && reg_renumber[pic86_reg_num] >= 0)))
  {
    rtx replacement;
    int output_hdr = 0;

    if (reg_renumber && reg_renumber[pic86_reg_num] >= 0)
      replacement = gen_rtx_REG(SImode, reg_renumber[pic86_reg_num]);
    else
      replacement = regno_reg_rtx[pic86_reg_num];

    for (; insns != 0 && count--; insns = next_real_insn (insns))    
    {
      replace_count = 0;
      inside_set_rtx = 0;
      insns = replace_machopic86_base_refs (insns, replacement);
      if (replace_count && getenv("REPLACED_INSNS") != 0)
      {
	if (!output_hdr)
	{
	  fprintf (stderr, "## %s -- replace (reg:SI %d) with ",
			current_function_name, pic86_reg_num);
	  debug_rtx (replacement);
	  output_hdr = 1;
	}
        debug_rtx (insns);
	fprintf(stderr, "\n##\n");
      }
    }   
  }     
}
#endif	/* FIXED_PIC_REG */

/* Returns 1 if OP is a

     (const:SI (minus:SI (symbol_ref:SI ("symbol_name"))
			 (symbol_ref:SI ("name_of_pic_base_label"))

   i.e., a MACHOPIC pic-base-offset constant.

   This is only needed by a hacked pattern in i386.md, and does not
   apply if the PIC base reg is fixed.  Returning 0 here in this case
   effectively disables that pattern.  */

int
machopic_symbolic_operand (op, mode)
     register rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
#ifndef FIXED_PIC_REG
  if (GET_CODE (op) == CONST)
    {
      op = XEXP (op, 0);
      if (GET_CODE (op) == MINUS)
	   return (GET_CODE (XEXP (op, 0)) == SYMBOL_REF
		   && GET_CODE (XEXP (op, 1)) == SYMBOL_REF);
    }
#endif
  return 0;
}
