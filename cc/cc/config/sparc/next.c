/* next.c:  Functions for NeXT as target machine for GNU C compiler.  */

#include "sparc/sparc.c"
#include "next/nextstep.c"
#include "next/machopic.h"

#define GEN_LOCAL_LABEL_FOR_SYMBOL(BUF,SYMBOL,LENGTH,N)		\
  do {								\
    const char *symbol_ = (SYMBOL);				\
    char *buffer_ = (BUF);					\
    if (symbol_[0] == '"')					\
      {								\
        sprintf(buffer_, "\"L%d$%s", (N), symbol_+1);		\
      }								\
    else if (name_needs_quotes(symbol_))			\
      {								\
        sprintf(buffer_, "\"L%d$%s\"", (N), symbol_);		\
      }								\
    else							\
      {								\
        sprintf(buffer_, "L%d$%s", (N), symbol_);		\
      }								\
  } while (0)

// Generate PIC and indirect symbol stubs 
void
machopic_output_stub (file, symb, stub)
     FILE *file;
     const char *symb, *stub;
{
  unsigned int length;
  char *binder_name, *symbol_name, *lazy_ptr_name;
  char *local_label_0, *local_label_1, *local_label_2;
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

  local_label_0 = alloca(length + 32);
  GEN_LOCAL_LABEL_FOR_SYMBOL(local_label_0, symb, length, 0);

  local_label_1 = alloca(length + 32);
  GEN_LOCAL_LABEL_FOR_SYMBOL(local_label_1, symb, length, 1);

  local_label_2 = alloca(length + 32);
  GEN_LOCAL_LABEL_FOR_SYMBOL(local_label_2, symb, length, 2);

  if (MACHOPIC_PURE)
    machopic_picsymbol_stub_section ();
  else
    machopic_symbol_stub_section ();

  fprintf (file, "%s:\n", stub);
  fprintf (file, "\t.indirect_symbol %s\n", symbol_name);

  if (MACHOPIC_PURE) {
      fprintf(file, "\tmov %%o7,%%g4\n");
      fprintf(file, "%s:\n\tcall %s\n", local_label_0, local_label_1);
      fprintf(file, "\tsethi %%hi(%s-%s),%%g6\n",
	      lazy_ptr_name, local_label_0);
      fprintf(file, "%s:\n\tor %%g6,%%lo(%s-%s),%%g6\n", local_label_1,
	      lazy_ptr_name, local_label_0);
      fprintf(file, "\tld [%%g6+%%o7],%%g6\n");
      fprintf(file, "\tjmp %%g6\n");
      fprintf(file, "\tmov %%g4,%%o7\n");
  } else {
      fprintf(file, "\tsethi %%hi(%s),%%g6\n", lazy_ptr_name);
      fprintf(file, "\tld [%%g6+%%lo(%s),%g6\n", lazy_ptr_name);
      fprintf(file, "\tjmp %g6\n\tnop\n");
  }
  
  fprintf (file, "%s:\n", binder_name);
  
  if (MACHOPIC_PURE) {
      char *binder = machopic_non_lazy_ptr_name ("*dyld_stub_binding_helper");
      machopic_validate_stub_or_non_lazy_ptr (binder, 0);

      if (binder[0] == '*') 
	  binder += 1;
      fprintf(file, "\tcall %s\n", local_label_2);
      fprintf(file, "\tsethi %%hi(%s-%s),%%g6\n", binder, binder_name);
      fprintf(file, "%s:\n\tor %%g6,%%lo(%s-%s),%%g6\n", local_label_2,
	      binder, binder_name);
      fprintf(file, "\tld [%%g6+%%o7],%%g6\n");
      fprintf(file, "\tsethi %%hi(%s-%s),%%g5\n", lazy_ptr_name, binder_name);
      fprintf(file, "\tor %%g5,%%lo(%s-%s),%%g5\n", lazy_ptr_name,
	      binder_name);
      fprintf(file, "\tjmp %%g6\n\tadd %%g5,%%o7,%%g5\n");
  } else {
      fprintf(file, "\tsethi %%hi(%s),%%g5\n", lazy_ptr_name);
      fprintf(file, "\tsethi %%hi(dyld_stub_binding_helper),%%g6\n");
      fprintf(file, "\tjmp %%g6+%%lo(dyld_stub_binding_helper\n");
      fprintf(file, "\tor %%g5,%%lo(%s),%%g5\n", lazy_ptr_name);
  }

  machopic_lazy_symbol_ptr_section ();
  fprintf (file, "%s:\n", lazy_ptr_name);
  fprintf (file, "\t.indirect_symbol %s\n", symbol_name);
  fprintf (file, "\t.long %s\n", binder_name);
}

#ifdef MACHO_PIC

/* Mach-o PIC code likes to think that it has a dedicated register,
   used to refer to this function's pic offset table.  Exceptions
   confuse the issue during unwinding.  So, the built-in, compiler-
   generated "__throw" function needs to re-establish the PIC register,
   as does the code at the start of any "catch".

   The sparc code at the start of a function, to generate a PIC base in
   the first place, looks like
        func:      save %sp,<framesize>,%sp  ; set up a frame
        pic_base:  call L_next
                   nop
        L_next:    mov %o7,%l7  ; now L7 has &pic_base.
   References to static data are via a fixed offset from L7.

   Now, what reload_sparc_pic_register must do at the other spots in
   the function where the pic reg needs to be re-established, should
   be the equivalent of

        L_call:    call L_next
                   nop
        L_next:    subi %o7,(L_call-pic_base),%l7

   At L_next, %o7 has the address of L_call in it.  It needs to have
   the address of pic_base, so we subtract the difference.

   HOWEVER, it's difficult to create that tricky an instruction, and
   the simple "sub" instruction might have to be a separated lo16/hi16
   thing if the function is too big, anyway.  So instead, we generate

        L_call:    call L_next
                   nop
        L_next:    sethi   %hi(L_call-pic_base),%l7
                   or      %l7,%lo(L_call-pic_base),%l7
                   sub     %o7,%l7,%l7

*/

void
reload_sparc_pic_register ()
{
  rtx L_call, L_next;                   /* labels */
  rtx fn_base, Lcall_lrf, Lnext_lrf;    /* label refs */

  /* calculations */
  rtx set_pc_next, set_rl7_next, reg_O7, diff, set_potr;

  int orig_flag_pic = flag_pic;

  if (current_function_uses_pic_offset_table == 0)
    return;

  if (! flag_pic)
    abort ();

  flag_pic = 0;
  L_call = gen_label_rtx ();
  L_next = gen_label_rtx ();

  /* generate (or get) the machopic pic base label */
  fn_base = gen_rtx (SYMBOL_REF, VOIDmode, machopic_function_base_name ());

  reg_O7 = gen_rtx (REG, SImode, 15);
  
  Lnext_lrf = gen_rtx (LABEL_REF, VOIDmode, L_next);
  Lcall_lrf = gen_rtx (LABEL_REF, VOIDmode, L_call);

  /* the two rtx's below share the same L_next label ref. */
  set_pc_next = gen_rtx (SET, VOIDmode, pc_rtx, Lnext_lrf);
  set_rl7_next = gen_rtx (SET, VOIDmode, reg_O7, Lnext_lrf);

  LABEL_PRESERVE_P(L_call) = 1;
  emit_label (L_call);

  /* Note that we pun calls and jumps here!  */
  emit_jump_insn (gen_rtx (PARALLEL, VOIDmode,
                           gen_rtvec (2, set_pc_next, set_rl7_next)));

  LABEL_PRESERVE_P(L_next) = 1;
  emit_label (L_next);

  /* Figure out the (constant) difference between "here" and picbase: */
  diff = gen_rtx (CONST, SImode, gen_rtx (MINUS, SImode, Lcall_lrf, fn_base));

  /* Put that difference into the pic register. */
  /* Has to be done as a sethi/or thing on SPARC */
  emit_insn (gen_rtx (SET, VOIDmode, pic_offset_table_rtx,
                      gen_rtx (HIGH, SImode, diff)));
  emit_insn (gen_rtx (SET, VOIDmode, pic_offset_table_rtx,
                      gen_rtx (LO_SUM, SImode, pic_offset_table_rtx, diff)));

  /* Then, set  %L7  to   %O7 minus %L7.  */
  emit_insn (gen_rtx (SET, Pmode, pic_offset_table_rtx,
                      gen_rtx (MINUS, Pmode, reg_O7, pic_offset_table_rtx)));

  /* Mark that register used.  (?Again?) */
  emit_insn (gen_rtx (USE, VOIDmode, pic_offset_table_rtx));

  flag_pic = orig_flag_pic;
}

#endif /* MACHO_PIC */
