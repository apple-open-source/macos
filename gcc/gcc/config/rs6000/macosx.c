/* Functions for Apple Mac OS X as target machine for GNU C compiler.
   Copyright 1997 Apple Computer, Inc. (unpublished)  */

#include "rs6000/rs6000.c"
#include "apple/openstep.c"
#include "apple/machopic.h"

/* Returns 1 if OP is either a symbol reference or a sum of a symbol
   reference and a constant.  */

int
symbolic_operand (op)
     register rtx op;
{
  switch (GET_CODE (op))
    {
    case SYMBOL_REF:
    case LABEL_REF:
      return 1;
    case CONST:
      op = XEXP (op, 0);
      return (GET_CODE (op) == SYMBOL_REF ||
	      (GET_CODE (XEXP (op, 0)) == SYMBOL_REF
	       || GET_CODE (XEXP (op, 0)) == LABEL_REF)
	      && GET_CODE (XEXP (op, 1)) == CONST_INT);
    default:
      return 0;
    }
}

void
apple_output_ascii (file, p, size)
     FILE *file;
     unsigned char *p;
     int size;
{
  char *opcode = ".ascii";
  int max = 48;
  int i;

  register int num = 0;

  fprintf (file, "\t%s\t \"", opcode);
  for (i = 0; i < size; i++)
    {
      register int c = p[i];

      if (num > max)
	{
	  fprintf (file, "\"\n\t%s\t \"", opcode);
	  num = 0;
	}

      if (c == '\"' || c == '\\')
	{
	  putc ('\\', file);
	  num++;
	}

      if (c >= ' ' && c < 0177)
	{
	  putc (c, file);
	  num++;
	}
      else
	{
	  fprintf (file, "\\%03o", c);
	  num += 4;
	  /* After an octal-escape, if a digit follows,
	     terminate one string constant and start another.
	     The Vax assembler fails to stop reading the escape
	     after three digits, so this is the only way we
	     can get it to parse the data properly.  */
	  if (i < size - 1 && p[i + 1] >= '0' && p[i + 1] <= '9')
	    num = max + 1;	/* next pass will start a new string */
	}
    }
  fprintf (file, "\"\n");
}

#ifdef RS6000_LONG_BRANCH

static tree stub_list = 0;

/* ADD_COMPILER_STUB adds the compiler generated stub for handling 
   procedure calls to the linked list.  */

void 
add_compiler_stub (label_name, function_name, line_number)
     tree label_name;
     tree function_name;
     int line_number;
{
  tree stub = build_tree_list (function_name, label_name);
  TREE_TYPE (stub) = build_int_2 (line_number, 0);
  TREE_CHAIN (stub) = stub_list;
  stub_list = stub;
}

#define STUB_LABEL_NAME(STUB)     TREE_VALUE (STUB)
#define STUB_FUNCTION_NAME(STUB)  TREE_PURPOSE (STUB)
#define STUB_LINE_NUMBER(STUB)    TREE_INT_CST_LOW (TREE_TYPE (STUB))

/* OUTPUT_COMPILER_STUB outputs the compiler generated stub for handling 
   procedure calls from the linked list and initializes the linked list.  */

void output_compiler_stub ()
{
  char tmp_buf[256];
  char label_buf[256];
  char *label;
  tree tmp_stub, stub;

  if (!flag_pic)
    for (stub = stub_list; stub; stub = TREE_CHAIN (stub))
      {
	fprintf (asm_out_file,
		 "%s:\n", IDENTIFIER_POINTER(STUB_LABEL_NAME(stub)));

#if defined (DBX_DEBUGGING_INFO) || defined (XCOFF_DEBUGGING_INFO)
	if (write_symbols == DBX_DEBUG || write_symbols == XCOFF_DEBUG)
	  fprintf (asm_out_file, "\t.stabd 68,0,%d\n", STUB_LINE_NUMBER(stub));
#endif /* DBX_DEBUGGING_INFO || XCOFF_DEBUGGING_INFO */

	if (IDENTIFIER_POINTER (STUB_FUNCTION_NAME (stub))[0] == '*')
	  strcpy (label_buf,
		  IDENTIFIER_POINTER (STUB_FUNCTION_NAME (stub))+1);
	else
	  {
	    label_buf[0] = '_';
	    strcpy (label_buf+1,
		    IDENTIFIER_POINTER (STUB_FUNCTION_NAME (stub)));
	  }

	strcpy (tmp_buf, "lis r12,hi16(");
	strcat (tmp_buf, label_buf);
	strcat (tmp_buf, ")\n\tori r12,r12,lo16(");
	strcat (tmp_buf, label_buf);
	strcat (tmp_buf, ")\n\tmtctr r12\n\tbctr");
	output_asm_insn (tmp_buf, 0);

#if defined (DBX_DEBUGGING_INFO) || defined (XCOFF_DEBUGGING_INFO)
	if (write_symbols == DBX_DEBUG || write_symbols == XCOFF_DEBUG)
	  fprintf(asm_out_file, "\t.stabd 68,0,%d\n", STUB_LINE_NUMBER (stub));
#endif /* DBX_DEBUGGING_INFO || XCOFF_DEBUGGING_INFO */
      }

  stub_list = 0;
}

/* NO_PREVIOUS_DEF checks in the link list whether the function name is
   already there or not.  */

int no_previous_def (function_name)
     tree function_name;
{
  tree stub;
  for (stub = stub_list; stub; stub = TREE_CHAIN (stub))
    if (function_name == STUB_FUNCTION_NAME (stub))
      return 0;
  return 1;
}

/* GET_PREV_LABEL gets the label name from the previous definition of
   the function.  */

tree get_prev_label (function_name)
     tree function_name;
{
  tree stub;
  for (stub = stub_list; stub; stub = TREE_CHAIN (stub))
    if (function_name == STUB_FUNCTION_NAME (stub))
      return STUB_LABEL_NAME (stub);
  return 0;
}

/* INSN is either a function call or a millicode call.  It may have an
   unconditional jump in its delay slot.  

   CALL_DEST is the routine we are calling.  */

char *
output_call (insn, call_dest, operand_number)
     rtx insn;
     rtx call_dest;
     int operand_number;
{
  static char buf[256];
  if (GET_CODE (call_dest) == SYMBOL_REF && TARGET_LONG_BRANCH && !flag_pic)
    {
      tree labelname;
      tree funname = get_identifier (XSTR (call_dest, 0));
      
      if (no_previous_def (funname))
	{
	  int line_number;
	  rtx label_rtx = gen_label_rtx ();
	  char *label_buf, temp_buf[256];
	  ASM_GENERATE_INTERNAL_LABEL (temp_buf, "L",
				       CODE_LABEL_NUMBER (label_rtx));
	  label_buf = temp_buf[0] == '*' ? temp_buf + 1 : temp_buf;
	  labelname = get_identifier (label_buf);
	  for (; insn && GET_CODE (insn) != NOTE; insn = PREV_INSN (insn));
	  if (insn)
	    line_number = NOTE_LINE_NUMBER (insn);
	  add_compiler_stub (labelname, funname, line_number);
	}
      else
	labelname = get_prev_label (funname);

      sprintf (buf, "jbsr %%z%d,%.246s",
	       operand_number, IDENTIFIER_POINTER (labelname));
      return buf;
    }
  else
    {
      sprintf (buf, "bl %%z%d", operand_number);
      return buf;
    }
}

#endif /* RS6000_LONG_BRANCH */

/* Write function profiler code. */

void
output_function_profiler (file, labelno)
  FILE *file;
  int labelno;
{
  int last_parm_reg; /* The last used parameter register.  */
  int i, j;

  /* Figure out last used parameter register.  The proper thing to do is
     to walk incoming args of the function.  A function might have live
     parameter registers even if it has no incoming args.  */

  for (last_parm_reg = 10;
       last_parm_reg > 2 && ! regs_ever_live [last_parm_reg];
       last_parm_reg--)
    ;

  /* Save parameter registers in regs RS6000_LAST_REG-(no. of parm regs used)+1
     through RS6000_LAST_REG.  Don't overwrite reg 31, since it might be set up
     as the frame pointer.  */
  for (i = 3, j = RS6000_LAST_REG; i <= last_parm_reg; i++, j--)
    fprintf (file, "\tmr %s,%s\n", reg_names[j], reg_names[i]);

  /* Load location address into r3, and call mcount.  */
  if (flag_pic)
    {
      if (current_function_uses_pic_offset_table) 
	fprintf (file, "\tmr r3,r0\n");
      else
	fprintf (file, "\tmflr r3\n");
 
      fprintf (file, "\tbl Lmcount$stub\n");
      mcount_called = 1;
    }
  else
    {
      fprintf (file, "\tmflr r3\n");
      fprintf (file, "\tbl mcount\n");
    }

  /* Restore parameter registers.  */
  for (i = 3, j = RS6000_LAST_REG; i <= last_parm_reg; i++, j--)
    fprintf (file, "\tmr %s,%s\n", reg_names[i], reg_names[j]);
}

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


/* Generate PIC and indirect symbol stubs.  */
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

  if (MACHOPIC_PURE)
    {
    fprintf (file, "\tmflr r0\n");
    fprintf (file, "\tbcl 20,31,%s\n", local_label_0);
    fprintf (file, "%s:\n\tmflr r11\n", local_label_0);
    fprintf (file, "\taddis r11,r11,ha16(%s-%s)\n",
			lazy_ptr_name, local_label_0);
    fprintf (file, "\tmtlr r0\n");
    fprintf (file, "\tlwz r12,lo16(%s-%s)(r11)\n",
			lazy_ptr_name, local_label_0);
    fprintf (file, "\tmtctr r12\n");
    fprintf (file, "\taddi r11,r11,lo16(%s-%s)\n",
			lazy_ptr_name, local_label_0);
    fprintf (file, "\tbctr\n");
    }
  else
    fprintf (file, "non-pure not supported\n");
  
  machopic_lazy_symbol_ptr_section ();
  fprintf (file, "%s:\n", lazy_ptr_name);
  fprintf (file, "\t.indirect_symbol %s\n", symbol_name);
  fprintf (file, "\t.long dyld_stub_binding_helper\n");
}

/* PIC TEMP STUFF */
/* Legitimize PIC addresses.  If the address is already position-independent,
   we return ORIG.  Newly generated position-independent addresses go into a
   reg.  This is REG if non zero, otherwise we allocate register(s) as
   necessary.  */

rtx
legitimize_pic_address (orig, mode, reg)
     rtx orig;
     enum machine_mode mode;
     rtx reg;
{
#ifdef MACHO_PIC
	if (reg == 0 && ! reload_in_progress && ! reload_completed) {
		reg = gen_reg_rtx(Pmode);
	}

  	if (GET_CODE (orig) == CONST) {
      rtx base, offset;

      if (GET_CODE (XEXP (orig, 0)) == PLUS
	  && XEXP (XEXP (orig, 0), 0) == pic_offset_table_rtx)
		return orig;

      if (GET_CODE (XEXP (orig, 0)) == PLUS)
	{
	  base = legitimize_pic_address (XEXP (XEXP (orig, 0), 0), Pmode, reg);
	  offset = legitimize_pic_address (XEXP (XEXP (orig, 0), 1), Pmode,
					   reg);
	}
      else
	abort ();

      if (GET_CODE (offset) == CONST_INT)
	{
	  if (SMALL_INT (offset))
	    return plus_constant_for_output (base, INTVAL (offset));
	  else if (! reload_in_progress && ! reload_completed)
	    offset = force_reg (Pmode, offset);
	  else
	    abort ();
	}
      return gen_rtx (PLUS, Pmode, base, offset);
    }
	return (rtx) machopic_legitimize_pic_address (orig, mode, reg);
#endif
	return orig;
}


/* Set up PIC-specific rtl.  This should not cause any insns
   to be emitted.  */

void
initialize_pic ()
{
}

/* Emit special PIC prologues and epilogues.  MACHO_PIC only */

void
finalize_pic ()
{
	rtx global_offset_table;
	rtx picbase_label;
	rtx seq;
	int orig_flag_pic = flag_pic;
	rtx first, last, jumper, rest;
	char *piclabel_name;

	if (current_function_uses_pic_offset_table == 0)
#ifdef MACHO_PIC
	  /* Assume the PIC base register is needed whenever any floating-point
	     constants are present.  Though perhaps a bit pessimistic, this
	     avoids any surprises in cases in which the PIC base register is
	     not needed for any other reason.  */
	  if (flag_pic && const_double_used())
	    current_function_uses_pic_offset_table = 1;
	  else
#endif
	    return;

	if (! flag_pic)
		abort ();

	flag_pic = 0;
	picbase_label = gen_label_rtx();
	
	start_sequence ();

#if 0
	piclabel_name = machopic_function_base_name ();
	XSTR(picbase_label, 4) = piclabel_name;
	
	/* save the picbase register in the stack frame */
	first = gen_rtx(SET, VOIDmode,
				gen_rtx(MEM, SImode,
					gen_rtx(PLUS, SImode,
						gen_rtx(REG, SImode, 1),
						gen_rtx(CONST_INT, VOIDmode, -4))),
				 		gen_rtx(REG, SImode, PIC_OFFSET_TABLE_REGNUM));
	emit_insn(first);

	/* generate branch to picbase label */
	emit_call_insn (gen_rtx (SET, VOIDmode, pc_rtx, 
					gen_rtx (LABEL_REF, VOIDmode, picbase_label)));

	/* well, this is cheating but easier than try to get a bl
	   output from a jump
	emit_insn (gen_rtx(ASM_INPUT, VOIDmode, strcat("bl ", piclabel_name)));
	*/

	LABEL_PRESERVE_P(picbase_label) = 1;
	emit_label (picbase_label);

  	/* and load the picbase from the link register (65) */
	last = gen_rtx (SET, VOIDmode, 
				gen_rtx(REG, Pmode, PIC_OFFSET_TABLE_REGNUM),
				gen_rtx(REG, Pmode, 65));			
	emit_insn (last);
#endif

	emit_insn (gen_rtx (USE, VOIDmode, pic_offset_table_rtx));

	flag_pic = orig_flag_pic;

	seq = gen_sequence ();
	end_sequence ();
	emit_insn_after (seq, get_insns ());

	/* restore the picbase reg */
/*
	rest = gen_rtx(SET, VOIDmode,
				gen_rtx(REG, SImode, PIC_OFFSET_TABLE_REGNUM),
				gen_rtx(MEM, SImode,
					gen_rtx(PLUS, SImode,
						gen_rtx(REG, SImode, 1),
						gen_rtx(CONST_INT, VOIDmode, -4))));
	emit_insn(rest);
*/
#if 1 /* Without this statement libgcc2.c can almost be built with insn scheduling enabled; the comment may be inaccurate.  */
	/* Do this so setjmp/longjmp can't confuse us */
	emit_insn (gen_rtx (USE, VOIDmode, pic_offset_table_rtx));
#endif
}


/* 	emit insns to move operand[1] to operand[0]

	return true if everything was written out, else 0
	called from md descriptions to take care of legitimizing pic
*/

int
emit_move_sequence (operands, mode)
rtx *operands;
enum machine_mode mode;
{
  register rtx operand0 = operands[0];
  register rtx operand1 = operands[1];
  rtx temp_reg = (reload_in_progress || reload_completed)
	? operand0 : gen_reg_rtx (flag_pic ? Pmode : SImode);

  if (flag_pic)
    {
      operands[1] = legitimize_pic_address (operand1, mode, temp_reg);
      return 0;
    }
  else
    {
      emit_insn (gen_elf_high (temp_reg, operand1));
      emit_insn (gen_elf_low (operand0, temp_reg, operand1));
      return 1;
    }	
}


#ifdef MACHO_PIC

/* Local interface to obstack allocation stuff.  */
static char *
permalloc_str (init, size)
     char *init;
     int size;
{
  char *spc = permalloc (size);
  strcpy (spc, init);
  return spc;  
}


/* Most mach-o PIC code likes to think that it has a dedicated register,
   used to refer to this function's pic offset table.  Exceptions
   confuse the issue during unwinding.  So, the built-in, compiler-
   generated "__throw" function needs to re-establish the PIC register,
   as does the code at the start of any "catch".  The code at the start
   of a function, to generate a pic base in the first place, looks like

        func:      <set up a new stack frame>
                   call pic_base
        pic_base:  mov <return-addr> into <pic-base-reg>

   After this, the function can reference static data via a fixed offset
   from the pic base register.

   Now, what RELOAD_PIC_REGISTER must do at the other spots in the
   function where the pic reg needs to be re-established, should be

                   call fix_pic
        fix_pic:   <pic-base-reg> := <return-addr> minus
                                    ( <fix_pic> minus <pic_base> )

   The RTL for generating this stuff is architecture-dependent;
   we invoke the per-architecture versions via a macro so that
   nothing is done for the architectures that don't do this form
   of PIC or don't have the reload code implemented.

   On PPC, the relevant code should end up looking like

	bl <new-label>
   <new-label>:
	mflr pic_base_reg
	addis pbr,pbr,ha16(<new-label> - fn_pic_base)
	addi  pbr,pbr,lo16(<new-label> - fn_pic_base)

   Called from the wrapper macro RELOAD_PIC_REGISTER.  */

void
reload_ppc_pic_register ()
{
  rtx L_retaddr, fn_base, Lret_lrf;      /* labels & label refs */
  rtx Fake_Lname;
  rtx link_reg_rtx;
  rtx set_pc_next, diff, set_potr;       /* calculations */
  int orig_flag_pic = flag_pic;

  char *link_reg_name = reg_names[ 65 ];
  char *potr_name = reg_names[ PIC_OFFSET_TABLE_REGNUM ];
    int len_potr_name = strlen(potr_name);

  /* These names need to be preserved as strings referred to by the RTL,
     so they are allocated with permalloc (or permalloc_str, above). */
  char *mflrinstr;
  char *add_hi_instr;
  char *add_lo_instr;
  char *diff_str;
    int len_diff_str;
  char *fake_name;
    int len_fake_name;
  char *fn_base_name;
    int len_fn_base_name;

  if (current_function_uses_pic_offset_table == 0)
    return;

  if (! flag_pic)
    abort ();

  flag_pic = 0;

  /* link_reg_rtx should be global; the "65" should be #define'd. */
  link_reg_rtx = gen_rtx (REG, Pmode, 65);

  L_retaddr = gen_label_rtx ();
  Lret_lrf = gen_rtx (LABEL_REF, Pmode, L_retaddr);

  fake_name = permalloc_str ("*L", 14);
  sprintf (fake_name, "*L%d", CODE_LABEL_NUMBER (L_retaddr));
  len_fake_name = strlen (fake_name);

  Fake_Lname = gen_rtx (SYMBOL_REF, Pmode, fake_name);
  /* Tell the compiler that this symbol is defined in this file.  */
  SYMBOL_REF_FLAG (Fake_Lname) = 1;

  /* generate (or get) the machopic pic base label */
  fn_base = gen_rtx (SYMBOL_REF, SImode, machopic_function_base_name ());
  fn_base_name = XSTR (fn_base,0);
  len_fn_base_name = strlen (fn_base_name);

  /* Cannot get the back end to permit a LABEL_REF as a target of
     a call.  So we use the Fake_Lname, and a SYMBOL_REF to it.
  */
  emit_call_insn (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (3,
                   gen_rtx (CALL, VOIDmode,
			    gen_rtx (MEM, SImode, Fake_Lname), const0_rtx),
                   gen_rtx (USE, VOIDmode, const0_rtx),
                   gen_rtx (CLOBBER, VOIDmode,
                    gen_rtx (SCRATCH, SImode, 0)))));

  LABEL_PRESERVE_P(L_retaddr) = 1;
  emit_label (L_retaddr);

  mflrinstr = permalloc_str ("mflr ", 6+strlen(potr_name));
    strcat (mflrinstr, potr_name);
  emit_insn (gen_rtx (ASM_INPUT, VOIDmode, mflrinstr));

  diff_str = permalloc (len_fn_base_name +2 + len_fake_name);
    /* The "+1" is to avoid the '*' at the start of each name. */
    sprintf (diff_str, "%s - %s", fn_base_name+1, fake_name+1);
  len_diff_str = strlen (diff_str);

  add_hi_instr = permalloc (16 + 2*len_potr_name + len_diff_str);
    sprintf (add_hi_instr, "addis %s,%s,ha16(%s)",
	     potr_name, potr_name, diff_str);
  emit_insn (gen_rtx (ASM_INPUT, VOIDmode, add_hi_instr));

  add_lo_instr = permalloc (15 + 2*len_potr_name + len_diff_str);

  add_lo_instr = permalloc_str ("addi ", 15 + 2*len_potr_name + len_diff_str);
    sprintf (add_lo_instr, "addi %s,%s,lo16(%s)",
	     potr_name, potr_name, diff_str);
  emit_insn (gen_rtx (ASM_INPUT, VOIDmode, add_lo_instr));

  emit_insn (gen_rtx (USE, VOIDmode, pic_offset_table_rtx));

  flag_pic = orig_flag_pic;
}

#endif  /* MACHO_PIC */
